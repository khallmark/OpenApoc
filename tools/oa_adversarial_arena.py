#!/usr/bin/env python3
"""Run adversarial co-evolution against the REAL engine.

oa_adversarial.py holds the learning, and knows nothing about the game. This file is the
Evaluator that connects it: it launches a real game, applies the alien policy through the
engine's own AI config knobs, plays until a real battle happens, drives that battle with the
X-COM policy, and scores the result.

WHY THE CAMPAIGN PATH AND NOT SKIRMISH. Skirmish is the natural fixture -- one map, one force,
under a minute -- and it is broken: battle generation dies on a threadpool worker with
"mutex lock failed: Invalid argument". Two separate faults were already fixed underneath it (a
std::vector::at on the sample list, a silently-empty no-location branch) and a third remains. The
campaign path reaches battles through loadBattleBuilding instead, and has been producing real
tactical missions all along. Waiting for the race to be fixed would have meant no end-to-end
training at all, so this uses the path that works and says plainly which one it is.

NO CHEATING, on either side. X-COM's policy drives the same on-screen controls a player uses,
through oa_ai/oa_executor, which cannot read a field the UI does not show. The alien policy is
applied through OpenApoc.AlienAI.* config, which selects and weights behaviours the engine
already had -- it does not grant the aliens information either.
"""

from __future__ import annotations

import argparse
import json
import random
import sys
import time
from pathlib import Path

from oa_adversarial import Arena, Evaluator, Policy, new_arena, train
from oa_play import Driver, GameProcess, Harness, advance, new_game, win_battle


def utility(outcome: str, squad_start, squad_end) -> float:
    """Score a battle for the X-COM side, in [0,1].

    Winning dominates, but survivors matter on their own: a win that costs the whole squad is not
    a result worth breeding from, and in a campaign it is exactly how a run ends up unable to fly
    the next mission. 70/30 reflects that without letting a cautious policy farm draws.
    """
    won = 1.0 if outcome == "resolved" else 0.0
    if squad_start and squad_end is not None and squad_start > 0:
        frac = max(0.0, min(1.0, squad_end / squad_start))
    else:
        frac = 0.5
    return max(0.0, min(1.0, 0.7 * won + 0.3 * frac))


class CampaignEvaluator(Evaluator):
    """One battle per call, fought in a real game process.

    Each evaluation is a fresh game: the alien genome goes in as launch config, the campaign runs
    until a tactical mission occurs, and the X-COM genome drives it. Expensive (minutes) and
    noisy (one map, one seed) -- which is precisely why the learner allocates battles by UCB
    rather than round-robin.
    """

    def __init__(self, repo: Path, out: Path, port: int, budget_s: float, leg_days: float):
        self.repo, self.out, self.port = repo, out, port
        self.budget_s, self.leg_days = budget_s, leg_days
        self.battles = 0

    def _alien_argv(self, alien: Policy) -> list:
        g = alien.genes
        argv = []
        mode = g.get("behaviour_mix", "mixed")
        if mode in ("aggressive", "normal", "cautious"):
            argv.append(f"--OpenApoc.AlienAI.Behaviour={mode}")
        bias = g.get("cover_bias")
        if bias is not None:
            argv.append(f"--OpenApoc.AlienAI.CoverBiasPercent={int(float(bias) * 100)}")
        return argv

    def _xcom_policy(self, xcom: Policy) -> dict:
        g = dict(xcom.genes)
        g["name"] = xcom.name[:60]
        return g

    def evaluate(self, xcom: Policy, alien: Policy, seed: int) -> float:
        self.battles += 1
        run_out = self.out / f"b{self.battles:04d}"
        run_out.mkdir(parents=True, exist_ok=True)
        game = GameProcess(self.repo, self.port, run_out / "game.log",
                           extra=self._alien_argv(alien), seed=seed or 1)
        d = None
        try:
            game.start(wait_s=240)
            d = Driver(Harness(port=self.port), self.repo / "data/forms",
                       shots=run_out / "shots", verbose=False)
            d.checks = {}
            new_game(d, 3)
            before = {}
            deadline = time.time() + self.budget_s
            outcome = "none"
            while time.time() < deadline:
                st = d.status()
                if st.stage in ("BattleBriefing", "BattlePreStart", "BattleView"):
                    before = d.h.gs("battle") or {}
                    outcome = win_battle(d, budget_s=max(60.0, deadline - time.time()),
                                         policy=self._xcom_policy(xcom))
                    break
                # No battle yet: advance the campaign clock a leg at a time.
                try:
                    # advance() returns as soon as a battle stage takes over, so this doubles as
                    # "run the campaign until something to fight turns up".
                    advance(d, self.leg_days, budget_s=max(30.0, deadline - time.time()))
                except Exception:
                    if not d.dismiss_modal(d.status()):
                        d.h.key("Escape")
                    time.sleep(0.5)
            after = d.h.gs("battle") or {}

            def num(dct, k):
                try:
                    return int(dct.get(k, ""))
                except (ValueError, AttributeError):
                    return None

            score = utility(outcome, num(before, "mine"), num(after, "mine_alive"))
            print(f"    [battle {self.battles}] {xcom.genes.get('behaviour')}/"
                  f"{xcom.genes.get('fire_mode')} vs {alien.genes.get('behaviour_mix')} "
                  f"-> {outcome} score={score:.2f}", flush=True)
            return score
        finally:
            try:
                game.stop()
            except Exception:
                pass


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", type=int, default=17960)
    ap.add_argument("--repo", default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument("--out", default=None)
    ap.add_argument("--generations", type=int, default=3)
    ap.add_argument("--battles-per-gen", type=int, default=4)
    ap.add_argument("--pop", type=int, default=4)
    ap.add_argument("--budget", type=float, default=420.0, help="seconds per battle attempt")
    ap.add_argument("--leg", type=float, default=3.0, help="game-days advanced per step")
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()

    repo = Path(args.repo)
    out = Path(args.out) if args.out else repo / "build/adversarial"
    out.mkdir(parents=True, exist_ok=True)

    arena = new_arena(seed=args.seed, pop=args.pop)
    ev = CampaignEvaluator(repo, out, args.port, args.budget, args.leg)
    print(f"[adv] {args.generations} generations x {args.battles_per_gen} real battles, "
          f"pop {args.pop}/side, seed {args.seed}", flush=True)
    sums = train(arena, ev, args.generations, args.battles_per_gen,
                 ledger=out / "generations.jsonl", base_seed=args.seed * 1000)

    print("\n[adv] final standings")
    for side, pop in (("XCOM", arena.xcom), ("ALIEN", arena.alien)):
        for p in sorted(pop, key=lambda q: q.elo, reverse=True):
            print(f"  {side:<5} elo={p.elo:7.1f} battles={p.battles:<3} "
                  f"wr={p.win_rate:.2f}  {p.name[:90]}", flush=True)
    print(f"\n[adv] ledger: {out / 'generations.jsonl'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
