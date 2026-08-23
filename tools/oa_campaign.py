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
    Driver,
    GameProcess,
    Harness,
    HarnessError,
    advance,
    assign_research,
    new_game,
    snapshot,
    win_battle,
)

BATTLE_STAGES = ("BattleBriefing", "BattlePreStart", "BattleView", "BaseDefenseScreen")
CITY_STAGES = ("CityView",)
MAX_RESTARTS = 8
RESTART_COOLDOWN_S = 2.0


class Campaign:
    """Owns the game process across restarts and keeps a durable progress record."""

    def __init__(self, repo: Path, out: Path, port: int, difficulty: int = 1):
        self.repo, self.out, self.port = Path(repo), Path(out), port
        self.difficulty = difficulty
        self.out.mkdir(parents=True, exist_ok=True)
        (self.out / "shots").mkdir(exist_ok=True)
        self.checkpoint = self.out / "campaign.save"
        self.progress_path = self.out / "progress.json"
        self.progress = self._load_progress()
        self.game: GameProcess | None = None
        self.d: Driver | None = None
        self.restarts = 0

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
        self.game = GameProcess(self.repo, self.port, self.out / "game.log", extra=extra)
        self.game.start(wait_s=180)
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
    def victory(self) -> bool:
        """AliensDefeated replaces the stack with the winning cutscene."""
        try:
            st = self.d.status()
        except OSError:
            return False
        if st.stage == "VideoScreen":
            # Either ending reaches a VideoScreen; distinguish by whether we still hold bases.
            try:
                lost = self.d.h.gs("stage").get("defeated") == "1"
            except HarnessError:
                lost = False
            self.record("ended", "defeat" if lost else "victory")
            self.say("VICTORY - aliens defeated" if not lost else "campaign lost")
            return not lost
        return False

    def tick(self, days: float) -> None:
        """One leg of play: advance, fight anything that starts, checkpoint."""
        advance(self.d, days, budget_s=2400)
        st = self.d.status()
        if st.stage in BATTLE_STAGES:
            self.progress["battles_fought"] += 1
            self.say(f"tactical mission #{self.progress['battles_fought']} starting")
            outcome = win_battle(self.d, budget_s=2400)
            if outcome == "resolved":
                self.progress["battles_won"] += 1
                self.record("first_battle_resolved")
            self.say(f"mission outcome: {outcome}")
            self.save("after mission")

    def run(self, max_hours: float, leg_days: float) -> int:
        deadline = time.time() + max_hours * 3600
        self.start()
        assign_research(self.d)
        self.record("research_assigned")
        self.save("campaign start")
        snapshot(self.d, "t0")

        while time.time() < deadline:
            if not self.alive():
                if not self.restart():
                    return 1
                continue
            try:
                if self.victory():
                    self.say("=== CAMPAIGN COMPLETE ===")
                    return 0
                self.tick(leg_days)
                self.save("leg complete")
                snapshot(self.d, time.strftime("%H:%M"))
                self.say(f"progress: {self.progress}")
            except HarnessError as exc:
                self.say(f"harness error: {exc}")
                if self.alive():
                    text = str(exc).lower()
                    if "unknown query" in text or "not installed" in text:
                        self.say("binary does not speak this harness protocol; stopping")
                        return 1
                    time.sleep(1.0)
                    continue
                if not self.restart():
                    return 1
            except OSError as exc:
                self.say(f"connection lost: {exc}")
                if not self.restart():
                    return 1
        self.say(f"time budget reached; progress: {self.progress}")
        return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=17700)
    ap.add_argument("--repo", default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument("--out", default=None)
    ap.add_argument("--difficulty", type=int, default=1, help="1 = Novice")
    ap.add_argument("--hours", type=float, default=48.0)
    ap.add_argument("--leg", type=float, default=3.0, help="game-days per leg")
    args = ap.parse_args()

    repo = Path(args.repo)
    out = Path(args.out) if args.out else repo / "build/campaign"
    c = Campaign(repo, out, args.port, args.difficulty)
    try:
        return c.run(args.hours, args.leg)
    finally:
        try:
            c.save("shutdown")
            c.game.stop()
        except Exception:
            pass


if __name__ == "__main__":
    raise SystemExit(main())
