#!/usr/bin/env python3
"""Cross-run learner: get better at the campaign as runs accumulate.

oa_adversarial.py learns BATTLE tactics from battles. This learns CAMPAIGN strategy from whole
campaigns: how many fliers to keep, when to decline an incident, how fast to run the clock while
aliens are loose, which research to chase first, and which tactical doctrine to field. The unit of
evidence is one oa_victory run -- hours of wall-clock, one seed, one roll of the dice -- so the
method has to be sample-efficient and honest about noise.

THE LOOP (one generation = N parallel campaigns, each on its own seed and genome):

  propose  ->  launch N oa_victory --single-campaign --strategy <genome>  ->  read final.json
  ->  reward  ->  append history  ->  update model  ->  print leaderboard  ->  repeat

WHAT IS LEARNED. Each run plays one genome from tools/oa_strategy.py (15 genes, all traced to a
call site). Two things are kept:

  * A per-genome table (mean reward, n, standard error) -- the ELITIST half. The champion is the
    genome with the best LOWER confidence bound among those evaluated at least twice, so a single
    lucky seed cannot be champion. Every change of champion is recorded in the Hall of Fame (the
    same device oa_adversarial uses to keep progress from cycling).
  * A per-(gene,value) table -- the BANDIT half. New genomes are built by THOMPSON SAMPLING: for
    each gene, draw a plausible mean reward for each of its values from a shrunk Normal posterior
    and take the best draw. This assumes effects are roughly additive across genes, which is
    wrong in detail (air_patrol and min_squad interact) but is the only assumption cheap enough to
    learn from a few dozen runs; the genome table and champion mutation cover the interactions.

WHAT EACH GENERATION RUNS (N slots):
  * the all-defaults genome, in generation 0 and every `--baseline-every` generations after: the
    CONTROL ARM. "Better than where we started" is only a claim if defaults keep being measured
    on fresh seeds beside the candidates.
  * RE-EVALUATIONS of the most promising under-evaluated genome(s), on new seeds. A winner on one
    seed is a rumour; this is what turns it into evidence (or retires it).
  * new genomes, alternating Thompson samples and mutations of the champion.
Seeds are derived from (learner seed, run index): all distinct, never reused, never 0 (0 means
"engine default" to the game). Re-evaluation always gets a new seed.

REWARD (see reward()): victory dominates everything; otherwise game-days survived before
funding was cut or the campaign was lost, plus score, research completed, battles won, UFOs shot
down and alien-dimension milestones, each capped so no one term can swamp the rest. A run whose
game never produced a readable campaign is INVALID and excluded from learning: infrastructure
failure is not evidence about a genome.

PERSISTENCE (under build/learning/ by default; --resume continues):
  history.jsonl       one record per finished run: genome, seed, reward breakdown, exit reason
  model.json          derived state: generation, champion, Hall of Fame, baseline, per-gene means
  leaderboard.txt     the latest leaderboard
  plans/gen-NNNN.json what each generation intended to run (lets --resume re-run unfinished runs)
  runs/<run id>/      each run's own out dir (game.log, victory.log, progress.json, final.json)

Everything but the launcher is pure and deterministic given the history and the seed, which is
what lets tools/test_oa_learn.py exercise the whole loop against a synthetic landscape in
milliseconds with no game.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import random
import shutil
import signal
import socket
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Optional

from oa_strategy import DEFAULTS, GENES, Strategy

# ---------------------------------------------------------------------------
# Reward
# ---------------------------------------------------------------------------

# Every weight is a deliberate statement about what a good campaign is. Caps keep one runaway term
# (a long grind of cheap battles, say) from outweighing survival and progress toward victory.
REWARD_WEIGHTS = {
    "victory": 10000.0,           # winning ends the game; must beat any run that does not win
    "victory_speed": 10.0,        # per game-day under VICTORY_DAY_CAP, so faster wins rank higher
    "survival_per_day": 8.0,      # per game-day alive and funded
    "alive_at_end": 40.0,         # reached the time budget without being beaten or defunded
    "score_per_point": 0.05,      # lifetime score, clamped to SCORE_CLAMP
    "research_per_topic": 12.0,   # topics completed during the campaign (starting tech excluded)
    "per_battle_won": 6.0,
    "per_ufo_down": 3.0,
    "per_recovery": 4.0,
    "per_infil_raid": 2.0,
    "second_base": 20.0,
    "milestone_workshop": 60.0,   # Large workshop built: gate on the dimension shifter
    "milestone_shifter": 100.0,   # dimension shifter started
    "milestone_crossed": 300.0,   # a craft crossed into the alien dimension
    "per_alien_building": 150.0,  # each alien building raided and won
    "defeat": -100.0,             # campaign lost or bankrupt, on top of the survival it forfeited
}
VICTORY_DAY_CAP = 400
SURVIVAL_DAY_CAP = 365
SCORE_CLAMP = (-3000.0, 10000.0)
CAPS = {"research": 60, "battles": 60, "ufos": 40, "recoveries": 30, "raids": 30, "buildings": 10}


def reward(final: Optional[dict]) -> dict:
    """Score a final.json. Returns {"valid", "reward", "breakdown", "why"}.

    INVALID (valid False, reward None) when there is no summary, or the summary never read a
    campaign clock -- the game died before there was anything to measure. Those runs say nothing
    about the genome and must not teach the model that it is bad.
    """
    if not isinstance(final, dict):
        return {"valid": False, "reward": None, "breakdown": {},
                "why": "no final.json (runner died before it could summarise)"}
    exit_reason = final.get("exit", "")
    m = final.get("metrics") or {}
    if exit_reason == "start_failed":
        return {"valid": False, "reward": None, "breakdown": {}, "why": "game never started"}
    if "max_day" not in m:
        return {"valid": False, "reward": None, "breakdown": {},
                "why": f"no campaign clock was ever read (exit {exit_reason or '?'})"}

    w = REWARD_WEIGHTS
    p = final.get("progress") or {}
    ended = final.get("campaign_ended")
    day = int(m["max_day"])
    funded_days = int(m["funding_lost_day"]) if m.get("funding_lost_day") is not None else day
    lost = ended in ("defeat", "bankrupt") or exit_reason in ("defeat", "bankrupt")
    won = ended == "victory" or exit_reason == "victory"
    milestones = set(p.get("milestones") or [])

    b: dict = {}
    if won:
        b["victory"] = w["victory"] + w["victory_speed"] * max(0, VICTORY_DAY_CAP - day)
    b["survival"] = w["survival_per_day"] * min(max(funded_days, 0), SURVIVAL_DAY_CAP)
    if exit_reason == "time_budget" and not m.get("funding_terminated") and not lost:
        b["alive_at_end"] = w["alive_at_end"]
    score = float(m.get("score_total", 0))
    b["score"] = w["score_per_point"] * max(SCORE_CLAMP[0], min(SCORE_CLAMP[1], score))
    done = max(0, int(m.get("research_complete", 0)) - int(m.get("research_complete_start", 0)))
    b["research"] = w["research_per_topic"] * min(done, CAPS["research"])
    b["battles_won"] = w["per_battle_won"] * min(int(p.get("wins", 0)), CAPS["battles"])
    b["ufos_down"] = w["per_ufo_down"] * min(int(p.get("ufos_down", 0)), CAPS["ufos"])
    b["recoveries"] = w["per_recovery"] * min(int(p.get("recoveries", 0)), CAPS["recoveries"])
    b["infil_raids"] = w["per_infil_raid"] * min(int(p.get("infil_raids", 0)), CAPS["raids"])
    if int(p.get("bases", m.get("bases", 1)) or 1) >= 2:
        b["second_base"] = w["second_base"]
    if "advanced_workshop_built" in milestones:
        b["workshop"] = w["milestone_workshop"]
    if milestones & {"dimension_shifter_started", "gate_craft_manufacture_started",
                     "gate_craft_ready"}:
        b["shifter"] = w["milestone_shifter"]
    if "crossed_to_alien_dimension" in milestones:
        b["crossed"] = w["milestone_crossed"]
    taken = min(int(p.get("alien_buildings_taken", 0)), CAPS["buildings"])
    if taken:
        b["alien_buildings"] = w["per_alien_building"] * taken
    if lost:
        b["defeat"] = w["defeat"]
    return {"valid": True, "reward": round(sum(b.values()), 3),
            "breakdown": {k: round(v, 3) for k, v in b.items()}, "why": ""}


# ---------------------------------------------------------------------------
# Statistics over history
# ---------------------------------------------------------------------------

DEFAULT_SD = 150.0     # reward noise assumed before there is data to measure it
MIN_SD = 25.0
PRIOR_K = 2.0          # pseudo-observations of the grand mean in each (gene, value) posterior


@dataclass
class GenomeStat:
    key: str
    genes: dict
    rewards: list = field(default_factory=list)
    last_gen: int = 0

    @property
    def n(self) -> int:
        return len(self.rewards)

    @property
    def mean(self) -> float:
        return sum(self.rewards) / len(self.rewards)

    @property
    def best(self) -> float:
        return max(self.rewards)


def valid_runs(history: list) -> list:
    return [r for r in history if r.get("valid") and r.get("reward") is not None]


def genome_table(history: list) -> dict:
    table: dict = {}
    for r in valid_runs(history):
        st = table.setdefault(r["key"], GenomeStat(r["key"], r["genes"]))
        st.rewards.append(float(r["reward"]))
        st.last_gen = max(st.last_gen, int(r.get("gen", 0)))
    return table


def noise_sd(table: dict) -> float:
    """Reward noise within a genome across seeds (pooled), else the spread of everything."""
    ss, dof = 0.0, 0
    for st in table.values():
        if st.n >= 2:
            mu = st.mean
            ss += sum((x - mu) ** 2 for x in st.rewards)
            dof += st.n - 1
    if dof:
        return max(MIN_SD, math.sqrt(ss / dof))
    allr = [x for st in table.values() for x in st.rewards]
    if len(allr) >= 2:
        mu = sum(allr) / len(allr)
        return max(MIN_SD, math.sqrt(sum((x - mu) ** 2 for x in allr) / (len(allr) - 1)))
    return DEFAULT_SD


def ucb(st: GenomeStat, sd: float, c: float = 1.0) -> float:
    return st.mean + c * sd / math.sqrt(st.n)


def lcb(st: GenomeStat, sd: float, c: float = 1.0) -> float:
    return st.mean - c * sd / math.sqrt(st.n)


def champion(table: dict, min_evals: int = 2) -> Optional[GenomeStat]:
    """Best lower confidence bound among genomes seen at least `min_evals` times, else None."""
    sd = noise_sd(table)
    eligible = [s for s in table.values() if s.n >= min_evals]
    if not eligible:
        return None
    return max(eligible, key=lambda s: (lcb(s, sd), s.key))


# ---------------------------------------------------------------------------
# Proposals
# ---------------------------------------------------------------------------

@dataclass
class Proposal:
    strategy: Strategy
    kind: str       # baseline | reeval | thompson | mutant


def gene_active(genes: dict, name: str) -> bool:
    req = GENES[name].requires
    return req is None or genes.get(req[0]) == req[1]


def thompson_sample(history: list, rng: random.Random) -> Strategy:
    """A genome built gene by gene from posterior draws over each gene's values.

    For gene g and value v the evidence is every valid run whose genome had g ACTIVE and set to v.
    The posterior mean is shrunk toward the grand mean by PRIOR_K pseudo-observations, and its
    variance is sd^2/(n+PRIOR_K): a value never tried has the widest posterior, so it gets drawn
    high often enough to be tried -- exploration without a separate exploration schedule.
    """
    runs = valid_runs(history)
    table = genome_table(history)
    grand = sum(r["reward"] for r in runs) / len(runs) if runs else 0.0
    sd = noise_sd(table)
    chosen: dict = {}
    for name, gene in GENES.items():          # prerequisites come first in GENES
        if not gene_active(chosen, name):
            chosen[name] = gene.default
            continue
        best_v, best_theta = gene.default, -math.inf
        for v in gene.values:
            xs = [r["reward"] for r in runs
                  if gene_active(r["genes"], name) and r["genes"].get(name) == v]
            mu = (sum(xs) + PRIOR_K * grand) / (len(xs) + PRIOR_K)
            theta = rng.gauss(mu, sd / math.sqrt(len(xs) + PRIOR_K))
            if theta > best_theta:
                best_v, best_theta = v, theta
        chosen[name] = best_v
    return Strategy(chosen)


def mutate(parent: dict, rng: random.Random, rate: float = 0.25) -> Strategy:
    """Change a few ACTIVE genes of `parent` (a canonical genes dict); never emit a clone.

    Numeric genes usually step to a neighbouring value: with runs this expensive, a small move is
    the one a noisy signal can still resolve.
    """
    genes = dict(parent)
    names = [n for n in GENES if gene_active(genes, n)]

    def move(name: str) -> bool:
        gene = GENES[name]
        vals = list(gene.values)
        alts = [v for v in vals if v != genes[name]]
        if not alts:
            return False
        if gene.numeric and genes[name] in vals and rng.random() < 0.7:
            i = vals.index(genes[name])
            near = [vals[j] for j in (i - 1, i + 1) if 0 <= j < len(vals)]
            genes[name] = rng.choice(near)
        else:
            genes[name] = rng.choice(alts)
        return True

    changed = False
    for n in names:
        if rng.random() < rate:
            changed = move(n) or changed
    while not changed:
        changed = move(rng.choice(names))
    return Strategy(genes)


def propose(history: list, n_slots: int, gen: int, seed: int, *, baseline_every: int = 3,
            max_evals: int = 5, reeval_frac: float = 0.25) -> list:
    """Choose what this generation runs. Deterministic in (history, n_slots, gen, seed)."""
    rng = random.Random(f"{seed}:{gen}")
    table = genome_table(history)
    base_key = Strategy().key()
    sd = noise_sd(table)
    out: list = []
    taken: set = set()

    def add(strategy: Strategy, kind: str) -> None:
        out.append(Proposal(strategy, kind))
        taken.add(strategy.key())

    # Control arm: defaults, first thing and then every few generations on fresh seeds.
    if base_key not in table or (gen > 0 and gen % baseline_every == 0):
        add(Strategy(), "baseline")

    # Evidence: re-run the most promising genomes that are still under-evaluated.
    if gen > 0:
        n_re = max(1, round(n_slots * reeval_frac)) if n_slots >= 2 else (1 if gen % 2 else 0)
        cands = sorted((s for k, s in table.items() if k != base_key and s.n < max_evals),
                       key=lambda s: (-ucb(s, sd), s.key))
        for s in cands[:n_re]:
            if len(out) < n_slots:
                add(Strategy(s.genes), "reeval")

    champ = champion(table)
    i = 0
    while len(out) < n_slots:
        use_mutant = champ is not None and i % 2 == 1
        for _ in range(25):                      # avoid proposing what is already known or taken
            cand = (mutate(champ.genes, rng) if use_mutant else thompson_sample(history, rng))
            if cand.key() not in table and cand.key() not in taken:
                break
        add(cand, "mutant" if use_mutant else "thompson")
        i += 1
    return out[:n_slots]


# ---------------------------------------------------------------------------
# Seeds and ports
# ---------------------------------------------------------------------------

def seed_for(learner_seed: int, run_index: int, used: set) -> int:
    """Distinct, nonzero, deterministic. 0 is the engine's "no explicit seed", so never use it."""
    h = int(hashlib.sha256(f"{learner_seed}:{run_index}".encode()).hexdigest()[:8], 16)
    s = h % 2_000_000_000 + 1
    while s in used:
        s += 1
    return s


def pick_port(taken: set, start: int = 18500, end: int = 18999) -> int:
    """A harness port nobody holds. Probing is the only honest test; `taken` covers the window
    between choosing a port and the game binding it."""
    span = end - start + 1
    offset = (os.getpid() * 7 + len(taken) * 13) % span
    for i in range(span):
        port = start + (offset + i) % span
        if port in taken:
            continue
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
            probe.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try:
                probe.bind(("127.0.0.1", port))
            except OSError:
                continue
        return port
    raise RuntimeError(f"no free harness port in {start}-{end}")


# ---------------------------------------------------------------------------
# Persistence
# ---------------------------------------------------------------------------

class Store:
    """Everything under one root. history.jsonl is the source of truth; the rest is derived."""

    def __init__(self, root: Path):
        self.root = Path(root)
        self.history_path = self.root / "history.jsonl"
        self.model_path = self.root / "model.json"
        self.board_path = self.root / "leaderboard.txt"
        self.plans = self.root / "plans"
        self.runs = self.root / "runs"
        for d in (self.root, self.plans, self.runs):
            d.mkdir(parents=True, exist_ok=True)

    def history(self) -> list:
        if not self.history_path.exists():
            return []
        out = []
        for line in self.history_path.read_text().splitlines():
            line = line.strip()
            if line:
                out.append(json.loads(line))
        return out

    def append(self, rec: dict) -> None:
        with self.history_path.open("a") as fh:
            fh.write(json.dumps(rec, sort_keys=True) + "\n")

    def plan_path(self, gen: int) -> Path:
        return self.plans / f"gen-{gen:04d}.json"

    def save_plan(self, gen: int, specs: list) -> None:
        self.plan_path(gen).write_text(json.dumps(specs, indent=1, sort_keys=True))

    def last_plan(self) -> Optional[tuple]:
        plans = sorted(self.plans.glob("gen-*.json"))
        if not plans:
            return None
        gen = int(plans[-1].stem.split("-")[1])
        return gen, json.loads(plans[-1].read_text())

    def model(self) -> dict:
        if self.model_path.exists():
            return json.loads(self.model_path.read_text())
        return {"version": 1, "hall_of_fame": []}

    def save_model(self, model: dict) -> None:
        tmp = self.model_path.with_suffix(".tmp")
        tmp.write_text(json.dumps(model, indent=1, sort_keys=True))
        tmp.replace(self.model_path)


def gene_summary(history: list) -> dict:
    """Per gene: each value's mean reward and run count (active runs only). Informational."""
    runs = valid_runs(history)
    out: dict = {}
    for name, gene in GENES.items():
        row = {}
        for v in gene.values:
            xs = [r["reward"] for r in runs
                  if gene_active(r["genes"], name) and r["genes"].get(name) == v]
            if xs:
                row[str(v)] = {"mean": round(sum(xs) / len(xs), 1), "n": len(xs)}
        out[name] = row
    return out


def update_model(store: Store, gen: int, learner_seed: int) -> dict:
    """Recompute the derived model from history and record Hall-of-Fame changes."""
    history = store.history()
    table = genome_table(history)
    sd = noise_sd(table)
    champ = champion(table)
    base = table.get(Strategy().key())
    model = store.model()
    hof = model.setdefault("hall_of_fame", [])
    if champ is not None and (not hof or hof[-1]["key"] != champ.key):
        hof.append({"gen": gen, "key": champ.key, "genes": champ.genes,
                    "mean": round(champ.mean, 2), "n": champ.n,
                    "lcb": round(lcb(champ, sd), 2)})
    model.update({
        "seed": learner_seed,
        "generation": gen,
        "runs_total": len(history),
        "runs_valid": len(valid_runs(history)),
        "genomes": len(table),
        "noise_sd": round(sd, 2),
        "champion": None if champ is None else {
            "key": champ.key, "genes": champ.genes, "mean": round(champ.mean, 2), "n": champ.n,
            "lcb": round(lcb(champ, sd), 2)},
        "baseline": None if base is None else {"mean": round(base.mean, 2), "n": base.n},
        "gene_means": gene_summary(history),
    })
    store.save_model(model)
    return model


def leaderboard(history: list, gen: int, top: int = 10) -> str:
    table = genome_table(history)
    sd = noise_sd(table)
    champ = champion(table)
    base_key = Strategy().key()
    rows = sorted(table.values(), key=lambda s: (-s.mean, s.key))
    lines = [f"=== leaderboard after generation {gen}: {len(valid_runs(history))} valid of "
             f"{len(history)} runs, {len(table)} genomes, noise sd ~{sd:.0f} ===",
             f"{'#':>2} {'mean':>8} {'+-se':>6} {'n':>2} {'best':>8}  genome"]
    shown = rows[:top]
    if base_key in table and table[base_key] not in shown:
        shown = shown + [table[base_key]]
    for st in shown:
        rank = rows.index(st) + 1
        tag = ("  [champion]" if champ is not None and st.key == champ.key else "") + \
              ("  [defaults]" if st.key == base_key else "")
        lines.append(f"{rank:>2} {st.mean:>8.1f} {sd / math.sqrt(st.n):>6.1f} {st.n:>2} "
                     f"{st.best:>8.1f}  {st.key}{tag}")
    if champ is not None and base_key in table:
        lines.append(f"champion vs defaults: {champ.mean - table[base_key].mean:+.1f} "
                     f"(champion n={champ.n}, defaults n={table[base_key].n})")
    elif champ is None:
        lines.append("no champion yet: a genome needs 2 evaluations on different seeds")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Launching runs
# ---------------------------------------------------------------------------

class ProcessRun:
    """One oa_victory subprocess, with a hard wall-clock limit and guaranteed cleanup."""

    def __init__(self, spec: dict, repo: Path, hours: float, difficulty: int, tile: str,
                 grace_s: float):
        self.spec = spec
        self.out = Path(spec["out_dir"])
        self.deadline = time.time() + hours * 3600 + grace_s
        self.timed_out = False
        self.out.mkdir(parents=True, exist_ok=True)
        strategy_path = self.out / "strategy.json"
        strategy_path.write_text(json.dumps(spec["genes"], sort_keys=True))
        cmd = [sys.executable, "-u", str(Path(repo) / "tools" / "oa_victory.py"),
               "--repo", str(repo), "--out", str(self.out), "--port", str(spec["port"]),
               "--seed", str(spec["seed"]), "--difficulty", str(difficulty),
               "--hours", str(hours), "--no-audio", "--single-campaign",
               "--strategy", str(strategy_path)]
        env = dict(os.environ)
        if tile:
            env["OA_TILE"] = f"{tile}:{spec['slot']}"
        self.log = open(self.out / "runner.log", "a")
        self.proc = subprocess.Popen(cmd, stdout=self.log, stderr=subprocess.STDOUT, env=env,
                                     cwd=str(repo))

    def poll(self) -> Optional[int]:
        return self.proc.poll()

    def stop(self) -> None:
        """SIGTERM first so oa_victory can write final.json; then SIGKILL; then reap the game,
        which lives in its own session and would otherwise be orphaned holding its port."""
        if self.proc.poll() is None:
            self.proc.send_signal(signal.SIGTERM)
            try:
                self.proc.wait(timeout=90)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait(timeout=30)
        self.close()

    def close(self) -> None:
        from oa_play import reap_stale_game   # lazy: oa_play is heavy and tests do not need it
        try:
            reap_stale_game(self.spec["port"])
        except Exception:
            pass
        try:
            self.log.close()
        except Exception:
            pass
        # The per-run copy of the app bundle is ~20 MB and only exists to survive a rebuild.
        shutil.rmtree(self.out / "OpenApoc.app", ignore_errors=True)


Launcher = Callable[[dict], object]


def process_launcher(repo: Path, hours: float, difficulty: int, tile: str,
                     grace_s: float) -> Launcher:
    def launch(spec: dict) -> ProcessRun:
        return ProcessRun(spec, repo, hours, difficulty, tile, grace_s)
    return launch


def read_final(out_dir: Path) -> Optional[dict]:
    p = Path(out_dir) / "final.json"
    if not p.exists():
        return None
    try:
        return json.loads(p.read_text())
    except ValueError:
        return None


def execute(store: Store, gen: int, specs: list, launch: Launcher, parallel: int,
            say: Callable[[str], None], poll_s: float = 5.0,
            stop_flag: Optional[Callable[[], bool]] = None) -> list:
    """Run `specs` at most `parallel` at a time and append one history record per run."""
    pending = list(specs)
    running: dict = {}
    done: list = []
    try:
        while pending or running:
            if stop_flag and stop_flag():
                raise KeyboardInterrupt
            while pending and len(running) < parallel:
                occupied = {spec["slot"] for spec, _ in running.values()}
                available = next((i for i, spec in enumerate(pending)
                                  if spec["slot"] not in occupied), None)
                if available is None:
                    break
                spec = pending.pop(available)
                say(f"[gen {gen}] launch {spec['run_id']} ({spec['kind']}) seed {spec['seed']} "
                    f"port {spec['port']} slot {spec['slot']}: "
                    f"{Strategy(spec['genes']).key()}")
                try:
                    running[spec["run_id"]] = (spec, launch(spec))
                except Exception as exc:
                    rec = make_record(gen, spec, None, f"launch failed: {exc}")
                    store.append(rec)
                    done.append(rec)
            for rid, (spec, handle) in list(running.items()):
                timed_out = False
                if handle.poll() is None:
                    if time.time() <= handle.deadline:
                        continue
                    timed_out = True
                    say(f"[gen {gen}] {rid} exceeded its wall-clock limit; stopping it")
                    handle.stop()
                else:
                    handle.close()
                del running[rid]
                rec = make_record(gen, spec, read_final(spec["out_dir"]),
                                  "timed out" if timed_out else "")
                store.append(rec)
                done.append(rec)
                say(f"[gen {gen}] {rid} finished: exit={rec['exit']} valid={rec['valid']} "
                    f"reward={rec['reward']}" + (f" ({rec['why']})" if rec["why"] else ""))
            if running:
                time.sleep(poll_s)
    except BaseException:
        # Interrupted: stop every game we started so none is orphaned. The unfinished runs have no
        # history record, so --resume re-runs them.
        for _, handle in running.values():
            try:
                handle.stop()
            except Exception:
                pass
        raise
    return done


def make_record(gen: int, spec: dict, final: Optional[dict], note: str) -> dict:
    scored = reward(final)
    strat = Strategy(spec["genes"])
    why = scored["why"] or (note if not scored["valid"] else "")
    return {
        "run_id": spec["run_id"], "gen": gen, "slot": spec["slot"], "kind": spec["kind"],
        "seed": spec["seed"], "key": strat.key(), "genes": strat.canonical(),
        "valid": scored["valid"], "reward": scored["reward"], "breakdown": scored["breakdown"],
        "why": why, "exit": (final or {}).get("exit", "none"),
        "campaign_ended": (final or {}).get("campaign_ended"),
        "game_days": ((final or {}).get("metrics") or {}).get("max_day"),
        "wall_seconds": (final or {}).get("wall_seconds"),
        "out_dir": spec["out_dir"], "finished_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "note": note,
    }


# ---------------------------------------------------------------------------
# The loop
# ---------------------------------------------------------------------------

def plan_specs(store: Store, proposals: list, gen: int, seed: int, tile_cells: int,
               slot_base: int, history: list) -> list:
    # Every run ever planned, finished or not, owns its index and seed: plans are written before
    # any run starts, so they cover history and also what an interrupted generation never finished.
    used = {r["seed"] for r in history}
    index0 = 0
    for f in store.plans.glob("gen-*.json"):
        planned = json.loads(f.read_text())
        index0 += len(planned)
        used.update(s["seed"] for s in planned)
    specs, ports = [], set()
    for i, prop in enumerate(proposals):
        idx = index0 + i
        sd = seed_for(seed, idx, used)
        used.add(sd)
        port = pick_port(ports)
        ports.add(port)
        run_id = f"g{gen:03d}-r{idx:04d}"
        specs.append({
            "run_id": run_id, "gen": gen, "kind": prop.kind, "seed": sd, "port": port,
            "slot": (slot_base + i) % max(1, tile_cells),
            "genes": prop.strategy.canonical(),
            "out_dir": str(store.runs / run_id),
        })
    return specs


def run_learning(store: Store, *, generations: int, n: int, seed: int, launch: Launcher,
                 parallel: int, tile_cells: int = 16, slot_base: int = 0, resume: bool = False,
                 baseline_every: int = 3, say: Callable[[str], None] = print,
                 poll_s: float = 5.0, stop_flag: Optional[Callable[[], bool]] = None) -> dict:
    history = store.history()
    if history and not resume:
        raise SystemExit(f"{store.root} already holds {len(history)} runs; pass --resume to "
                         f"continue them or choose another --root")
    gen = 0
    last = store.last_plan()
    if last is not None:
        gen, planned = last
        recorded = {r["run_id"] for r in history}
        todo = [s for s in planned if s["run_id"] not in recorded]
        if todo:
            # An interrupted generation: re-run what never finished, in fresh dirs (the old ones
            # may hold a half-played campaign's checkpoint, which oa_victory would RESUME).
            for s in todo:
                s["out_dir"] = str(store.runs / f"{s['run_id']}.retry{int(time.time())}")
            say(f"[resume] generation {gen}: {len(todo)} of {len(planned)} runs had no result; "
                f"re-running them")
            execute(store, gen, todo, launch, parallel, say, poll_s, stop_flag)
            update_model(store, gen, seed)
            text = leaderboard(store.history(), gen)
            store.board_path.write_text(text + "\n")
            say(text)
        gen += 1
    target = gen + generations
    model: dict = store.model()
    while gen < target:
        history = store.history()
        proposals = propose(history, n, gen, seed, baseline_every=baseline_every)
        specs = plan_specs(store, proposals, gen, seed, tile_cells, slot_base, history)
        store.save_plan(gen, specs)
        say(f"[gen {gen}] plan: " + "; ".join(f"{s['kind']}:{Strategy(s['genes']).key()}"
                                              for s in specs))
        execute(store, gen, specs, launch, parallel, say, poll_s, stop_flag)
        model = update_model(store, gen, seed)
        text = leaderboard(store.history(), gen)
        store.board_path.write_text(text + "\n")
        say(text)
        gen += 1
    return model


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--repo", default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument("--root", default=None, help="state dir (default <repo>/build/learning)")
    ap.add_argument("--generations", type=int, default=5)
    ap.add_argument("-n", "--n", type=int, default=4, help="campaigns per generation")
    ap.add_argument("--parallel", type=int, default=None,
                    help="how many of the N run at once (default N)")
    ap.add_argument("--hours", type=float, default=1.0, help="wall-clock budget per run")
    ap.add_argument("--difficulty", type=int, default=5, help="5 = Superhuman")
    ap.add_argument("--seed", type=int, default=1, help="learner seed: fixes proposals and seeds")
    ap.add_argument("--tile", default="4x4", help="OA_TILE grid CxR for the game windows ('' off)")
    ap.add_argument("--tile-slot-base", type=int, default=0, help="first grid slot to use")
    ap.add_argument("--baseline-every", type=int, default=3)
    ap.add_argument("--grace-minutes", type=float, default=25.0,
                    help="extra wall-clock beyond --hours before a run is force-stopped: game "
                         "start-up plus the tactical mission it may be inside when time is up")
    ap.add_argument("--resume", action="store_true", help="continue an existing --root")
    ap.add_argument("--poll", type=float, default=5.0)
    args = ap.parse_args()

    repo = Path(args.repo)
    root = Path(args.root) if args.root else repo / "build" / "learning"
    store = Store(root)
    cells = 16
    if args.tile:
        c, _, r = args.tile.partition("x")
        cells = int(c) * int(r)
    launch = process_launcher(repo, args.hours, args.difficulty, args.tile,
                              args.grace_minutes * 60.0)

    stop = {"flag": False}

    def on_signal(signum, frame):
        stop["flag"] = True
    signal.signal(signal.SIGTERM, on_signal)
    signal.signal(signal.SIGINT, on_signal)
    try:
        run_learning(store, generations=args.generations, n=args.n, seed=args.seed, launch=launch,
                     parallel=args.parallel or args.n, tile_cells=cells,
                     slot_base=args.tile_slot_base, resume=args.resume,
                     baseline_every=args.baseline_every, poll_s=args.poll,
                     stop_flag=lambda: stop["flag"])
    except KeyboardInterrupt:
        print("[learn] interrupted; games stopped. --resume re-runs unfinished runs.", flush=True)
        return 130
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
