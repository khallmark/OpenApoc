#!/usr/bin/env python3
"""Long-running, resumable OpenApoc campaign: Novice start toward victory, no cheats.

Victory in this engine is precise: raid BUILDING_DIMENSION_GATE_GENERATOR in the alien dimension
and win the tactical mission. Battle::exitBattle sets AliensDefeated only for the one building
carrying `victory`, and it is gated behind RESEARCH_ALIEN_BUILDING_9 -- the end of a chain where
each alien building must be raided and won to unlock research for the next.

That is months of game time and ten tactical missions, so this runner is built to survive:
  * it checkpoints through the harness SAVE command,
  * it resumes a dead game from the last checkpoint via --Game.Load,
  * it never aborts a battle, and never uses CheatOptions or a debug hotkey.

Nothing here manipulates GameState directly. Every action is a click or a keypress a player could
make.
"""

from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

from oa_play import (
    AdvanceOutcome,
    AdvanceResult,
    BattleOutcome,
    BattleResult,
    Driver,
    GameProcess,
    Harness,
    HarnessError,
    RunReceipt,
    advance,
    assign_research,
    classify_connection_failure,
    classify_terminal_status,
    create_run_directory,
    new_game,
    reconcile_validation_process_exit,
    require_positive,
    require_validation_seed,
    run_provenance,
    snapshot,
    win_battle,
)

BATTLE_STAGES = ("BattleBriefing", "BattlePreStart", "BattleView", "BaseDefenseScreen")
CITY_STAGES = ("CityView",)
# Protocol errors (unknown GS query, etc.) are not crashes. Restarting on them
# relaunches a live game every second until the wall-clock budget expires.
MAX_RESTARTS = 8
RESTART_COOLDOWN_S = 2.0


class Campaign:
    """Owns the game process across restarts and keeps a durable progress record."""

    def __init__(self, repo: Path, out: Path, port: int, difficulty: int = 1,
                 seed: int = 0, validation: bool = False, receipt: RunReceipt | None = None):
        self.repo, self.out, self.port = Path(repo), Path(out), port
        self.difficulty = difficulty
        self.seed = int(seed)
        self.validation = bool(validation)
        self.receipt = receipt
        self.out.mkdir(parents=True, exist_ok=True)
        (self.out / "shots").mkdir(exist_ok=True)
        self.checkpoint = self.out / "campaign.save"
        self.progress_path = self.out / "progress.json"
        self.progress = self._load_progress()
        self.game: GameProcess | None = None
        self.d: Driver | None = None
        self.restarts = 0
        self.last_outcome = "not_started"
        self.last_reason = ""
        self.last_battle_outcome: BattleResult | None = None

    # -- durability ------------------------------------------------------
    def _load_progress(self) -> dict:
        if self.progress_path.exists():
            try:
                return json.loads(self.progress_path.read_text())
            except ValueError:
                pass
        return {"battles_fought": 0, "battles_won": 0, "restarts": 0, "milestones": []}

    def record(self, key: str, value=None) -> None:
        if key not in self.progress["milestones"]:
            self.progress["milestones"].append(key)
        if value is not None:
            self.progress[key] = value
        self.progress_path.write_text(json.dumps(self.progress, indent=1))

    def say(self, msg: str) -> None:
        line = f"[{time.strftime('%H:%M:%S')}] {msg}"
        print(line, flush=True)
        with open(self.out / "campaign.log", "a") as f:
            f.write(line + "\n")

    def save(self, why: str) -> bool:
        try:
            self.d.h.ok(f"save {self.checkpoint}")
            self.say(f"checkpoint saved ({why})")
            return True
        except (HarnessError, OSError) as exc:
            self.say(f"checkpoint FAILED ({why}): {exc}")
            return False

    # -- process lifecycle ------------------------------------------------
    def start(self) -> None:
        """Boot the game, resuming from the checkpoint when one exists."""
        resume = self.checkpoint.exists()
        extra = [f"--Game.Load={self.checkpoint}"] if resume else []
        self.game = GameProcess(
            self.repo,
            self.port,
            self.out / "game.log",
            extra=extra,
            seed=self.seed,
            require_binary_snapshot=self.validation,
        )
        self.game.start(wait_s=180)
        if self.receipt:
            self.receipt.record_binary_snapshot(self.game._run_binary, self.game.argv)
        self.d = Driver(Harness(port=self.port), self.repo / "data/forms",
                        shots=self.out / "shots", verbose=True)
        self.d.checks = {}
        if resume:
            self.say("resumed from checkpoint")
            self.d.wait_for(CITY_STAGES, 180)
        else:
            self.say(f"fresh campaign, difficulty {self.difficulty}")
            new_game(self.d, self.difficulty)
            self.record("campaign_started")

    def alive(self) -> bool:
        if not self.game or self.game.proc.poll() is not None:
            return False
        try:
            self.d.h.send("status")
            return True
        except OSError:
            return False

    def restart(self) -> bool:
        """Bring the game back from the last checkpoint after a crash."""
        if self.restarts >= MAX_RESTARTS:
            self.say(f"too many restarts ({self.restarts}); giving up")
            return False
        self.restarts += 1
        self.progress["restarts"] = self.progress.get("restarts", 0) + 1
        self.say(f"game died - restarting from checkpoint (restart #{self.restarts})")
        try:
            if self.game:
                self.game.stop()
        except Exception:
            pass
        if not self.checkpoint.exists():
            self.say("no checkpoint to resume from; cannot continue")
            return False
        time.sleep(RESTART_COOLDOWN_S)
        try:
            self.start()
            return True
        except Exception as exc:
            self.say(f"restart failed: {exc}")
            return False

    # -- the campaign itself ---------------------------------------------
    def terminal_outcome(self) -> str | None:
        """Return the evidenced ending, never treating an arbitrary video as victory."""
        try:
            st = self.d.status()
        except OSError:
            return None
        outcome = classify_terminal_status(st)
        if outcome == "victory":
            self.record("ended", "victory")
            self.say(f"VICTORY - aliens defeated ({st.detail})")
        elif outcome == "defeat":
            self.record("ended", "defeat")
            self.say(f"campaign lost ({st.detail})")
        elif outcome == "unexpected_terminal":
            self.record("ended", "unexpected_terminal")
            self.say(f"unexpected terminal stage: {st.stage} ({st.detail})")
        return outcome

    def tick(self, days: float) -> AdvanceResult:
        """One leg of play: advance, fight anything that starts, checkpoint."""
        self.last_battle_outcome = None
        advanced = advance(self.d, days, budget_s=2400)
        if self.receipt:
            self.receipt.event(
                "advance",
                requested_days=days,
                outcome=advanced.outcome.value,
                advanced_ticks=advanced.advanced_ticks,
                stage=advanced.stage,
                reason=advanced.reason,
            )
        st = self.d.status()
        if advanced.outcome is AdvanceOutcome.TRANSITION and st.stage in BATTLE_STAGES:
            self.progress["battles_fought"] += 1
            self.say(f"tactical mission #{self.progress['battles_fought']} starting")
            result = win_battle(self.d, budget_s=2400)
            self.last_battle_outcome = result
            if result.won:
                self.progress["battles_won"] += 1
                self.record("first_battle_resolved")
            self.say(f"mission outcome: {result.outcome.value}")
            if self.receipt:
                self.receipt.event(
                    "battle",
                    number=self.progress["battles_fought"],
                    **result.as_dict(),
                )
            self.save("after mission")
        return advanced

    def finish(self, outcome: str, reason: str, exit_code: int) -> int:
        self.last_outcome = outcome
        self.last_reason = reason
        self.say(f"run outcome: {outcome} ({reason})")
        return int(exit_code)

    def run(self, max_hours: float, leg_days: float) -> int:
        deadline = time.time() + max_hours * 3600
        self.start()
        assign_research(self.d)
        self.record("research_assigned")
        self.save("campaign start")
        snapshot(self.d, "t0")

        while time.time() < deadline:
            if not self.alive():
                if self.validation:
                    detail = self.game.exit_status() if self.game else "game unavailable"
                    return self.finish("process_error", detail or "game stopped", 1)
                if not self.restart():
                    return self.finish("process_error", "restart budget exhausted", 1)
                continue
            try:
                terminal = self.terminal_outcome()
                if terminal == "victory":
                    self.say("=== CAMPAIGN COMPLETE ===")
                    return self.finish("victory", "winning video observed", 0)
                if terminal:
                    return self.finish(terminal, "terminal stage observed", 1)
                advanced = self.tick(leg_days)
                if advanced.outcome is AdvanceOutcome.TERMINAL:
                    terminal = self.terminal_outcome() or "unexpected_terminal"
                    if terminal == "victory":
                        return self.finish("victory", "winning video observed during advance", 0)
                    return self.finish(terminal, advanced.reason, 1)
                if self.validation and self.last_battle_outcome \
                        and not self.last_battle_outcome.decided:
                    result = self.last_battle_outcome
                    outcome = ("timeout" if result.outcome is BattleOutcome.TIMEOUT
                               else "gameplay_incomplete")
                    return self.finish(
                        outcome,
                        f"tactical mission did not decide: {result.outcome.value}",
                        1,
                    )
                if advanced.reached:
                    self.save("leg complete")
                elif advanced.outcome is not AdvanceOutcome.TRANSITION:
                    return self.finish(
                        advanced.outcome.value,
                        f"{advanced.reason}; advanced={advanced.advanced_ticks} ticks",
                        1,
                    )
                snapshot(self.d, time.strftime("%H:%M"))
                self.say(f"progress: {self.progress}")
            except HarnessError as exc:
                self.say(f"harness error: {exc}")
                if self.alive():
                    # A live process answering ERR is still the same session. Relaunching
                    # it is how 'gs time' on a binary without that query looped forever.
                    text = str(exc).lower()
                    if "unknown query" in text or "not installed" in text:
                        self.say("binary does not speak this harness protocol; stopping")
                        return self.finish("protocol_error", str(exc), 1)
                    if self.validation:
                        return self.finish("protocol_error", str(exc), 1)
                    time.sleep(1.0)
                    continue
                if self.validation:
                    return self.finish("transport_error", str(exc), 1)
                if not self.restart():
                    return self.finish("transport_error", str(exc), 1)
            except TimeoutError as exc:
                self.say(f"harness timeout: {exc}")
                if self.validation:
                    return self.finish("timeout", str(exc), 1)
                if not self.restart():
                    return self.finish("timeout", str(exc), 1)
            except OSError as exc:
                self.say(f"connection lost: {exc}")
                outcome, detail = classify_connection_failure(self.game, exc)
                if self.validation:
                    return self.finish(outcome, detail, 1)
                if not self.restart():
                    return self.finish(outcome, detail, 1)
        self.say(f"time budget reached; progress: {self.progress}")
        return self.finish("time_budget", "campaign did not reach victory before deadline", 1)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=17700)
    ap.add_argument("--repo", default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument("--out", default=None)
    ap.add_argument("--difficulty", type=int, default=1, help="1 = Novice")
    ap.add_argument("--hours", type=float, default=48.0)
    ap.add_argument("--leg", type=float, default=3.0, help="game-days per leg")
    ap.add_argument("--seed", type=int, default=0,
                    help="explicit RNG seed; validation requires a nonzero value")
    ap.add_argument("--validation", action="store_true",
                    help="emit immutable evidence and fail closed on infrastructure errors")
    args = ap.parse_args()

    repo = Path(args.repo)
    try:
        require_positive(args.hours, "--hours")
        require_positive(args.leg, "--leg")
        seed = require_validation_seed(args.seed, args.validation)
    except ValueError as exc:
        ap.error(str(exc))
    out_root = Path(args.out) if args.out else repo / "build/campaign"
    out = create_run_directory(out_root, "campaign", seed) if args.validation else out_root
    receipt = RunReceipt(
        out, run_provenance(repo, seed, args.validation, "oa_campaign")
    ) if args.validation else None
    if receipt:
        receipt.event("started", difficulty=args.difficulty, hours=args.hours, leg=args.leg)
    c = Campaign(
        repo,
        out,
        args.port,
        args.difficulty,
        seed=seed,
        validation=args.validation,
        receipt=receipt,
    )
    rc = 1
    try:
        rc = c.run(args.hours, args.leg)
    except HarnessError as exc:
        c.last_outcome, c.last_reason = "protocol_error", str(exc)
        c.say(f"protocol failure: {exc}")
    except TimeoutError as exc:
        c.last_outcome, c.last_reason = "timeout", str(exc)
        c.say(f"timeout: {exc}")
    except OSError as exc:
        c.last_outcome, c.last_reason = classify_connection_failure(c.game, exc)
        c.say(f"connection failure: {c.last_reason}")
    except Exception as exc:
        c.last_outcome = "process_error" if c.d is None and isinstance(exc, RuntimeError) else (
            "process_error" if "snapshot" in str(exc).lower() else "unexpected_error"
        )
        c.last_reason = f"{type(exc).__name__}: {exc}"
        c.say(f"run failed: {c.last_reason}")
    finally:
        status = None
        try:
            status = c.d.status() if c.d else None
        except Exception:
            pass
        process_exit = None
        try:
            if c.d:
                c.save("shutdown")
            if c.game:
                process_exit = c.game.stop()
        except Exception as exc:
            c.last_outcome, c.last_reason, rc = (
                "process_error", f"engine shutdown raised {type(exc).__name__}: {exc}", 1
            )
        if process_exit is not None:
            c.last_outcome, c.last_reason, rc = reconcile_validation_process_exit(
                args.validation, c.last_outcome, c.last_reason, rc, process_exit
            )
        if receipt and not receipt.finished:
            receipt.finish(
                c.last_outcome,
                rc,
                reason=c.last_reason,
                stage=status.stage if status else "unavailable",
                detail=status.detail if status else "-",
                progress=c.progress,
                engine_exit=process_exit.as_dict() if process_exit else None,
            )
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
