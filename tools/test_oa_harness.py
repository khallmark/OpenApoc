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

# --- battle-scoped queries must tolerate the battle ending -------------------
# Battle::checkMissionEnd tears current_battle down the instant the last hostile dies, so a query
# already in flight comes back an error. Twice now that raised out of win_battle and threw away a
# mission that had just been WON -- first enemies_screen, then centre_on_enemy, because the first
# fix patched the instance rather than the class.
class DyingHarness(FakeHarness):
    def gs(self, q):
        raise oa_play.HarnessError(f'gs {q} -> ERR unknown query "{q}"')

    def screen_craft(self, q):
        raise oa_play.HarnessError(f'gs {q} -> ERR unknown query "{q}"')


dead = FakeDriver("BattleView")
dead.h = DyingHarness()
for q in ("centre_on_enemy", "centre_on_friends", "battle_positions", "battle"):
    check(oa_play.battle_gs(dead, q) == {},
          f"battle_gs must return {{}} when the battle has gone, for {q}")
for q in ("enemies_screen", "friends_screen"):
    check(oa_play.on_screen(dead, q) == [],
          f"on_screen must return [] when the battle has gone, for {q}")

# No battle-scoped query in the battle loop may still call gs directly -- that is precisely how
# the second one was missed.
whole = inspect.getsource(oa_play)
for q in ("centre_on_enemy", "centre_on_friends"):
    check(f'd.h.gs("{q}")' not in whole,
          f"{q} must go through battle_gs, not raw gs")
for q in ("enemies_screen", "friends_screen"):
    check(f'd.h.screen_craft("{q}")' not in whole,
          f"{q} must go through on_screen, not raw screen_craft")

# --- never press Quit on the options menu ------------------------------------
# BUTTON_QUIT means "leave this screen" on BuildingScreen, BribeScreen and friends, and
# "exit the program" on InGameOptions (ingameoptions.cpp:215 -> StageCmd::Command::QUIT).
# return_to_city tried it FIRST, unconditionally, so any run that reached the options menu shut
# its own game down: cleanly, rc=0, no crash -- and then reported that the game had gone. That was
# the whole "ConnectionRefusedError [exited rc=0]" failure class, five attempts across two runs.
src_rtc = inspect.getsource(oa_play.return_to_city)
check("quit_exits_game" in src_rtc,
      "return_to_city must special-case the screens where Quit exits the program")
check('"InGameOptions"' in src_rtc and '"MainMenu"' in src_rtc,
      "…and must name both of them")
# The dangerous ordering must be gone: BUTTON_QUIT may never be the first thing tried unguarded.
check('("BUTTON_QUIT", "BUTTON_OK")' in src_rtc and '("BUTTON_OK",) if quit_exits_game' in src_rtc,
      "on a quit-exits screen it must try BUTTON_OK only")

# The response table must not route InGameOptions at BUTTON_QUIT either.
opts = oa_play.RESPONSES.get("InGameOptions", {})
check("BUTTON_QUIT" not in opts.values(),
      f"InGameOptions must never be acked with BUTTON_QUIT, got {opts}")

# --- a lost mission has no survivors ------------------------------------------
# survivors is the last MID-BATTLE sample; current_battle is torn down before the debriefing, so
# there is nothing left to query. A squad stunned unconscious therefore reported 6 of 6 on a
# mission it had just LOST, and the arena scored that 0.30 -- full marks for survival.
# checkMissionEnd sets playerWon=false exactly when no player unit is conscious
# (battle.cpp:2160-2205), so "lost" and "somebody still standing" cannot both be true.
src_wb = inspect.getsource(oa_play.win_battle)
check('if outcome == "lost"' in src_wb, "win_battle must zero survivors on a loss")
check('survivors_last_seen' in src_wb, "…and keep the stale reading under its own name")
check('"returned"' in src_wb,
      "…and say why a withdrawal is exempt -- those really are survivors")

import oa_adversarial_arena as _A
scores = {
    "won intact":   _A.utility("resolved", 6, 6),
    "won pyrrhic":  _A.utility("resolved", 6, 1),
    "withdrew":     _A.utility("returned", 6, 4),
    "lost":         _A.utility("lost", 6, 0),
}
check(scores["won intact"] > scores["won pyrrhic"] > scores["withdrew"] > scores["lost"],
      f"scoring must order outcomes as a campaign would: {scores}")
check(scores["lost"] == 0.0, f"a wipe scores zero, got {scores['lost']}")

# --- a base defence is never abandoned ---------------------------------------
# Not when losing, not when stalled, not when a second base exists. Withdrawing forfeits the base:
# every facility reverts to unbuilt and the labs, stores and staff go with it. The rule was
# softened once -- "losing a base only ends the game when it is the LAST base" -- and that
# reasoning is wrong in a way the citation hides: the aliens who take it are still there, the
# facilities are still gone, and the squad has spent lives buying nothing.
src_wb = inspect.getsource(oa_play)
check('may_leave = mission_type != "base_defense"' in src_wb,
      "a base defence must never be leavable")
check("bases_now" not in src_wb,
      "the base COUNT must not enter the decision at all -- owning a spare is not a reason to "
      "concede your home")

if FAILED:
    print(f"FAILED {len(FAILED)}:")
    for m in FAILED:
        print("  -", m)
    sys.exit(1)
print("all harness tests passed (Escape guard)")
