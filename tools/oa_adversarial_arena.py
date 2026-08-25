#!/usr/bin/env python3
"""The closed loop: both sides pick a doctrine, fight, and learn from the result.

This is what makes Phase 2 real rather than a module that passes its own tests. Each round:

  1. The X-COM learner samples one of its tactical doctrines (a TacticalAI plugin).
  2. The alien learner samples an opponent doctrine, which is pinned into the engine via
     OpenApoc.NewFeature.OpponentBehaviorMode - a control no player has.
  3. A battle is fought with those two.
  4. The outcome becomes a zero-sum utility and BOTH learners update their regret.

Over rounds each side's mix shifts against the other's. That is the adaptation, and it is
measurable in the mixes rather than asserted - the ledger records every round so a claim about
who adapted to what can be checked afterwards.

UTILITY. Winning matters most, but a win that costs the squad is not one worth repeating, so
survivor fraction is folded in. Both are things a human sees on the debriefing screen.

    utility = 0.7 * (won ? +1 : -1) + 0.3 * (2 * survivor_fraction - 1)

NO CHEATING on the X-COM side: its doctrines are TacticalAI plugins that import no engine and see
only an Observation built from what is on screen. The alien doctrine pin is an arena control on
the OPPONENT, which is the thing being adapted to - it is not extra information for X-COM.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

from oa_adversarial import AdversarialTrainer, Matchup
from oa_ai import REGISTRY
from oa_executor import load_plugins
from oa_play import Driver, GameProcess, Harness, new_game
from oa_skirmish import run_one

# The engine's own behaviour modes, which are the aliens' real doctrine knob.
ALIEN_DOCTRINES = {"aggressive": 0, "normal": 1, "evasive": 2}


def utility(outcome: str, squad_start, squad_end) -> float:
    won = 1.0 if outcome == "resolved" else -1.0
    if squad_start and squad_end is not None and squad_start > 0:
        frac = max(0.0, min(1.0, squad_end / squad_start))
    else:
        frac = 0.5
    return 0.7 * won + 0.3 * (2.0 * frac - 1.0)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", type=int, default=17950)
    ap.add_argument("--repo", default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument("--out", default=None)
    ap.add_argument("--rounds", type=int, default=0, help="0 = until stopped")
    ap.add_argument("--budget", type=float, default=240.0)
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--map-row", type=int, default=0)
    ap.add_argument("--alien", action="append", default=[])
    args = ap.parse_args()

    aliens = {}
    for spec in args.alien:
        n, _, c = spec.partition("=")
        aliens[n] = int(c or 1)
    if not aliens:
        aliens = {"anthropod": 4, "skeletoid": 2}

    repo = Path(args.repo)
    out = Path(args.out) if args.out else repo / "build/adversarial"
    out.mkdir(parents=True, exist_ok=True)
    ledger = out / "rounds.jsonl"

    xcom_doctrines = sorted(load_plugins().keys())
    trainer = AdversarialTrainer(xcom_doctrines, sorted(ALIEN_DOCTRINES),
                                 state_path=out / "learned.json", seed=args.seed)
    print(f"[adv] X-COM doctrines: {xcom_doctrines}", flush=True)
    print(f"[adv] alien doctrines: {sorted(ALIEN_DOCTRINES)}", flush=True)

    rnd = 0
    game = None
    try:
        while args.rounds == 0 or rnd < args.rounds:
            rnd += 1
            x_doc, a_doc = trainer.next_matchup()
            print(f"[adv] round {rnd}: {x_doc} vs {a_doc}", flush=True)

            # A fresh process per round: the opponent doctrine is applied at battle start, so it
            # has to be set before the game boots, and a clean process also stops one round's
            # state leaking into the next.
            game = GameProcess(repo, args.port, out / f"game.log", seed=args.seed,
                               extra=[f"--OpenApoc.NewFeature.OpponentBehaviorMode="
                                      f"{ALIEN_DOCTRINES[a_doc]}"])
            game.start(wait_s=240)
            d = Driver(Harness(port=args.port), repo / "data/forms", shots=out / "shots",
                       verbose=False)
            d.checks = {}
            new_game(d, 1)

            before = {}
            try:
                before = d.h.gs("battle")
            except Exception:
                pass
            r = run_one(d, aliens, map_row=args.map_row, budget_s=args.budget,
                        policy={"name": x_doc, "fire_mode": None, "stance": None})
            after = {}
            try:
                after = d.h.gs("battle")
            except Exception:
                pass

            def num(dd, k):
                try:
                    return int(dd.get(k, ""))
                except (ValueError, AttributeError):
                    return None

            s0, s1 = num(before, "mine"), num(after, "mine_alive")
            u = utility(r.get("outcome", "error"), s0, s1)
            trainer.record(Matchup(x_doc, a_doc, u,
                                   {"outcome": r.get("outcome"), "start": s0, "end": s1}))
            with ledger.open("a") as fh:
                fh.write(json.dumps({"round": rnd, "xcom": x_doc, "alien": a_doc,
                                     "outcome": r.get("outcome"), "utility": round(u, 3),
                                     "squad_start": s0, "squad_end": s1,
                                     "xcom_mix": trainer.xcom.average_strategy(),
                                     "alien_mix": trainer.alien.average_strategy()}) + "\n")
            print(f"[adv]   -> {r.get('outcome')} u={u:+.2f}", flush=True)
            print(trainer.report(), flush=True)
            game.stop(); game = None
    except KeyboardInterrupt:
        print("[adv] interrupted", flush=True)
    finally:
        if game:
            try:
                game.stop()
            except Exception:
                pass
    print(trainer.report(), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
