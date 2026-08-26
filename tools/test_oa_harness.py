#!/usr/bin/env python3
"""Harness tests that need no game: pure logic on the Driver.

Small, but each one pins a bug that cost a real run.
"""
import sys

import oa_play

FAILED = []


def check(cond, msg):
    if not cond:
        FAILED.append(msg)


class FakeHarness:
    """Records keys instead of sending them."""

    def __init__(self):
        self.keys = []

    def key(self, k):
        self.keys.append(k)
        return "OK"


class FakeDriver(oa_play.Driver):
    """A Driver with no socket, no forms and no game -- only the key path under test."""

    def __init__(self, stage):
        self.h = FakeHarness()
        self._stage = stage

    def status(self):
        return oa_play.Status(stage=self._stage, w=1280, h=720, raw="")


# --- Escape must not open the settings menu ----------------------------------
# CityView and BattleView both PUSH InGameOptions on SDLK_ESCAPE (cityview.cpp:4156,
# battleview.cpp:3380). Escape is not "back" there, it is "open settings" -- and the harness used
# it as its universal "stuck, get out of this" fallback. One run reached InGameOptions thirteen
# times without ever asking for it, each visit costing a round to notice and another to close.
for stage in ("CityView", "BattleView"):
    d = FakeDriver(stage)
    sent = d.escape_key()
    check(sent is False, f"escape_key must refuse on {stage}, it would open the options menu")
    check(d.h.keys == [], f"…and must not send the key at all on {stage}, sent {d.h.keys}")

# Everywhere else Escape genuinely means back, and refusing would strand the run on that screen.
for stage in ("BuildingScreen", "ResearchScreen", "UfopaediaView", "BaseScreen", "InGameOptions"):
    d = FakeDriver(stage)
    check(d.escape_key() is True, f"escape_key must still work on {stage}")
    check(d.h.keys == ["Escape"], f"…and send exactly one Escape on {stage}, sent {d.h.keys}")

# The stage may be passed in to save a round-trip; it must be honoured, not ignored.
d = FakeDriver("BuildingScreen")
check(d.escape_key("CityView") is False, "an explicit stage argument must be respected")
check(d.h.keys == [], "…and must suppress the key")

# leave_battle is the one deliberate user of raw Escape: BUTTON_EXIT_BATTLE lives inside
# InGameOptions, so opening it there is the point. Guard against a future tidy-up routing it
# through escape_key, which would make leaving a battle impossible.
import inspect

src = inspect.getsource(oa_play.leave_battle)
check('d.h.key("Escape")' in src,
      "leave_battle must keep raw Escape -- it opens InGameOptions on purpose")
check("escape_key" not in src, "…and must not be routed through the guard")

# No fallback elsewhere may still press Escape directly. Exactly two places are allowed to:
# leave_battle, where opening InGameOptions is the goal, and escape_key itself, which IS the
# guard. Anything else is a fallback that can open the settings menu by accident, which is the
# bug this whole file exists to prevent coming back.
whole = inspect.getsource(oa_play)
raw_total = whole.count('d.h.key("Escape")') + whole.count('self.h.key("Escape")')
allowed = src.count('d.h.key("Escape")') + inspect.getsource(oa_play.Driver.escape_key).count(
    'self.h.key("Escape")')
check(raw_total == allowed,
      f"{raw_total - allowed} unguarded Escape call(s) remain outside leave_battle/escape_key")
check(inspect.getsource(oa_play.Driver.escape_key).count('self.h.key("Escape")') == 1,
      "escape_key must press the key itself exactly once -- it recursed into itself when a "
      "blanket replace rewrote its own body, and every guarded call then blew the stack")

if FAILED:
    print(f"FAILED {len(FAILED)}:")
    for m in FAILED:
        print("  -", m)
    sys.exit(1)
print("all harness tests passed (Escape guard)")
