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

THE CAMPAIGN MUST BE PLAYED, NOT WATCHED. An evaluator that starts a game and advances the clock
waiting for a battle gets none: X-COM has to go to the aliens. This runs the same loop
play_campaign() does -- research, replace losses, and raid the buildings the alerts have named --
which is what actually produces tactical missions.

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
from oa_play import (
    TICKS_PER_DAY, Driver, GameProcess, Harness, advance, assign_research, buy_interceptor,
    hire_staff, new_game, raid_infiltrated_building, sell_ground_fleet, sell_surplus_loot,
    win_battle,
)


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

    def evaluate(self, xcom: Policy, alien: Policy, seed: int):
        """Fight one battle. Returns the X-COM score, or None if no battle happened.

        X-COM HAS TO GO TO THE ALIENS. The first version of this method started a game and then
        looped advance() waiting for a battle to arrive, and got outcome=none three times running.
        That was not a budget shortfall -- play_campaign() already records why: with no raids "the
        only battles ever fought were base defences", i.e. the aliens arriving at our door. So this
        runs the same campaign loop the harness plays: keep the labs busy, replace losses, and
        RAID the buildings the alerts have named. Which buildings those are comes from
        Driver.alerted_buildings, filled from the game's own AlertScreen -- the same information a
        player has, and no more.

        Returning None rather than a number is the other half of the fix: an attempt that produced
        no battle says nothing about either genome, and train() drops it whole.
        """
        self.battles += 1
        run_out = self.out / f"b{self.battles:04d}"
        run_out.mkdir(parents=True, exist_ok=True)
        rec = {
            "attempt": self.battles, "seed": seed,
            "xcom": xcom.name, "alien": alien.name,
            "alien_argv": self._alien_argv(alien),
            "outcome": "none", "reason": "", "score": None,
            "days_advanced": 0.0, "legs": 0, "raids": [],
            "turbo_blocked_samples": 0, "turbo_samples": 0,
            "stages": {}, "wall_s": 0.0,
        }
        t_start = time.time()
        game = GameProcess(self.repo, self.port, run_out / "game.log",
                           extra=self._alien_argv(alien), seed=seed or 1)
        d = None
        score = None
        try:
            game.start(wait_s=240)
            d = Driver(Harness(port=self.port), self.repo / "data/forms",
                       shots=run_out / "shots", verbose=False)
            d.checks = {}
            new_game(d, 3)

            # Campaign opening, same as play_campaign: without craft the raids cannot fly, and
            # without research the campaign stalls out well before it would have run out of time.
            for label, fn in (("sell_ground", lambda: sell_ground_fleet(d)),
                              ("buy_craft", lambda: buy_interceptor(d, want=2)),
                              ("research", lambda: assign_research(d))):
                try:
                    rec[f"open_{label}"] = fn()
                except Exception as exc:
                    rec[f"open_{label}"] = f"{type(exc).__name__}: {exc}"

            t0 = self._ticks(d)
            deadline = time.time() + self.budget_s
            outcome = None

            while time.time() < deadline and outcome is None:
                st = d.status()
                rec["stages"][st.stage] = rec["stages"].get(st.stage, 0) + 1

                # A battle already in progress (base defence, or one advance() handed back).
                if st.stage in ("BattleBriefing", "BattlePreStart", "BattleView",
                                "BaseDefenseScreen"):
                    outcome = win_battle(d, budget_s=max(120.0, deadline - time.time()),
                                         policy=self._xcom_policy(xcom))
                    rec["reason"] = "battle came to us"
                    break

                try:
                    advance(d, self.leg_days, budget_s=max(30.0, deadline - time.time()))
                    rec["legs"] += 1
                except Exception as exc:
                    rec.setdefault("advance_errors", []).append(f"{type(exc).__name__}: {exc}")
                    if not d.dismiss_modal(d.status()):
                        d.h.key("Escape")
                    time.sleep(0.5)
                    continue

                # Is the clock actually moving? A campaign pinned at Speed1 produces no missions
                # no matter how long it is left running, and that is indistinguishable from a
                # quiet map unless it is measured.
                try:
                    turbo = d.h.gs("turbo")
                    rec["turbo_samples"] += 1
                    if turbo.get("can_turbo") != "1":
                        rec["turbo_blocked_samples"] += 1
                except Exception:
                    pass

                if d.game_over():
                    rec["reason"] = "campaign ended before a battle"
                    break

                # The active half: go and fight where the game said the aliens are.
                if d.status().stage == "CityView":
                    try:
                        raid = raid_infiltrated_building(
                            d, budget_s=max(120.0, deadline - time.time()),
                            policy=self._xcom_policy(xcom))
                    except Exception as exc:
                        raid = f"error:{type(exc).__name__}: {exc}"
                    rec["raids"].append(raid)
                    if raid not in ("nothing-reported", "bad-coords", "already-clear",
                                    "no-agents-selectable", "refused") \
                            and not raid.startswith(("not-in-city", "no-building-screen",
                                                     "no-battle", "error:")):
                        outcome = raid
                        rec["reason"] = "raid"
                        break
                    # Nothing to raid: keep the base able to mount the next one.
                    for fn in (lambda: sell_surplus_loot(d), lambda: buy_interceptor(d, want=2),
                               lambda: hire_staff(d, want=6)):
                        try:
                            fn()
                        except Exception:
                            pass

            rec["days_advanced"] = round((self._ticks(d) - t0) / TICKS_PER_DAY, 2)

            if outcome is None:
                rec["reason"] = rec["reason"] or "budget expired with no battle"
                print(f"    [attempt {self.battles}] NO CONTEST after "
                      f"{rec['days_advanced']:.1f} game-days / {time.time()-t_start:.0f}s "
                      f"({rec['reason']}; raids={rec['raids'][-3:]})", flush=True)
            else:
                stats = dict(d.last_battle)
                rec["battle"] = stats
                rec["outcome"] = outcome
                score = utility(outcome, stats.get("started_with"), stats.get("survivors"))
                rec["score"] = round(score, 3)
                print(f"    [battle {self.battles}] {xcom.genes.get('behaviour')}/"
                      f"{xcom.genes.get('fire_mode')} vs {alien.genes.get('behaviour_mix')} "
                      f"-> {outcome} ({stats.get('survivors')}/{stats.get('started_with')} "
                      f"survived, {rec['days_advanced']:.1f}d) score={score:.2f}", flush=True)
            return score
        except Exception as exc:
            rec["reason"] = f"{type(exc).__name__}: {exc}"
            print(f"    [attempt {self.battles}] NO CONTEST: {rec['reason']}", flush=True)
            return None
        finally:
            rec["wall_s"] = round(time.time() - t_start, 1)
            self._log(rec)
            try:
                game.stop()
            except Exception:
                pass

    @staticmethod
    def _ticks(d) -> int:
        try:
            return int(d.h.gs("time")["ticks"])
        except Exception:
            return 0

    def _log(self, rec: dict) -> None:
        """Every attempt, contest or not, lands in the ledger.

        The three no-contests that started this printed one line each and left nothing behind, so
        there was no way to tell a campaign that never advanced from one that advanced and stayed
        quiet. That distinction is the whole diagnosis.
        """
        try:
            with (self.out / "battles.jsonl").open("a") as fh:
                fh.write(json.dumps(rec) + "\n")
        except Exception as exc:
            print(f"    [adv] battle ledger write failed: {type(exc).__name__}: {exc}", flush=True)


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

    print(f"\n[adv] {arena.total_plays} battles fought, {arena.no_contests} no-contests "
          f"(attempts that produced no battle; excluded from Elo entirely)")
    print("[adv] final standings")
    for side, pop in (("XCOM", arena.xcom), ("ALIEN", arena.alien)):
        for p in sorted(pop, key=lambda q: q.elo, reverse=True):
            print(f"  {side:<5} elo={p.elo:7.1f} battles={p.battles:<3} "
                  f"wr={p.win_rate:.2f}  {p.name[:90]}", flush=True)
    print(f"\n[adv] generation ledger: {out / 'generations.jsonl'}")
    print(f"[adv] per-attempt ledger: {out / 'battles.jsonl'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
