#!/usr/bin/env python3
"""Socket-free contracts for the cold skirmish validation path."""

from __future__ import annotations

import sys

import oa_arena
import oa_play
import oa_skirmish


FAILED: list[str] = []


def check(condition: bool, message: str) -> None:
    if not condition:
        FAILED.append(message)


def check_equal(actual, expected, message: str) -> None:
    if actual != expected:
        FAILED.append(f"{message}: expected {expected!r}, got {actual!r}")


class ColdDriver:
    """MainMenu driver whose Skirmish button lands on the auto-pushed MapSelector."""

    def __init__(self):
        self.stage = "MainMenu"
        self.clicked: list[str] = []

    def status(self) -> oa_play.Status:
        return oa_play.Status(self.stage, 1280, 720, "")

    def click_id(self, control: str, _status=None) -> bool:
        self.clicked.append(control)
        if control == "BUTTON_SKIRMISH":
            self.stage = "MapSelector"
            return True
        return False

    def wait_for(self, expected, _seconds: float) -> oa_play.Status:
        choices = (expected,) if isinstance(expected, str) else expected
        if self.stage not in choices:
            raise TimeoutError(f"{self.stage} not in {choices}")
        return self.status()

    def say(self, _message: str) -> None:
        pass


cold = ColdDriver()
check(oa_skirmish.open_skirmish(cold),
      "a cold MainMenu must enter Skirmish through its first-class button")
check_equal(cold.clicked, ["BUTTON_SKIRMISH"],
            "cold entry must not manufacture an in-progress campaign")
check_equal(cold.status().stage, "MapSelector",
            "Skirmish.begin auto-pushes MapSelector on the cold path")


setup = oa_skirmish.SkirmishAttempt.setup_failure(
    stage="SelectForces",
    reason="select_forces_resume_pop_not_applied",
)
record = setup.as_dict()
check_equal(record["result_kind"], "setup_failure",
            "a lost lifecycle transition must be setup failure")
check_equal(record["outcome"], None,
            "setup failure must not occupy the gameplay outcome field")
check_equal(record["stage"], "SelectForces", "setup failure must preserve the stuck stage")


scored = oa_arena.score([
    record,
    {"result_kind": "gameplay", "outcome": "resolved", "survivor_frac": 0.5,
     "seconds": 12.0},
])
check_equal(scored["battles"], 1,
            "arena score must exclude setup failures from the battle denominator")
check_equal(scored["setup_failures"], 1,
            "arena score must report setup failures separately")
check_equal(scored["win_rate"], 1.0,
            "setup failure must not be scored as a gameplay loss")


if FAILED:
    print(f"FAILED {len(FAILED)}:")
    for failure in FAILED:
        print("  -", failure)
    sys.exit(1)

print("all cold skirmish result tests passed")
