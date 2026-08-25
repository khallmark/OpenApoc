#!/usr/bin/env python3
"""Unit tests for the tactical AI. No game, no harness, no sockets -- runs in milliseconds.

This file is the point of the split. Every one of these cases would previously have needed a
booted game, a generated battlemap and a live battle to exercise, which is why none of them
existed and why the battle logic was never tested at all.
"""
import sys
from oa_ai import Observation, Unit, ScriptedAI, AggressiveAI, CautiousAI, make

FAILED = []


def check(cond, msg):
    if not cond:
        FAILED.append(msg)


def kinds(acts):
    return [a.kind for a in acts]


def obs(mine, foes, view_z=0, stalls=0, last_event_z=None):
    return Observation(
        mine=[Unit(i, x, y, z) for i, (x, y, z) in enumerate(mine)],
        foes=[Unit(100 + i, x, y, z, hostile=True) for i, (x, y, z) in enumerate(foes)],
        view_z=view_z, stalls=stalls, last_event_z=last_event_z)


# --- opening ---------------------------------------------------------------
a = ScriptedAI(fire_mode="snap", stance="run")
o = a.opening(obs([(0, 0, 0)], [(5, 5, 0)]))
check(kinds(o) == ["set_fire_mode", "set_stance"], f"opening kinds: {kinds(o)}")
check(o[0].arg == "snap" and o[1].arg == "run", "opening applies the configured policy")

# --- no hostiles: do not withdraw, do not flail -----------------------------
acts = a.decide(obs([(0, 0, 0)], []))
check(kinds(acts) == ["wait"], f"no foes should wait, got {kinds(acts)}")

# --- floor: prefer the engine's event location over the headcount -----------
# Most hostiles are on z=3, but the engine says the action is on z=1. Engine wins.
acts = a.decide(obs([(0, 0, 0)], [(1, 1, 3), (2, 2, 3), (3, 3, 1)], view_z=0, last_event_z=1))
check("zoom_event" in kinds(acts), f"should defer to the engine, got {kinds(acts)}")
check("show_floor" not in kinds(acts), "must not also hand-pick a floor")

# --- floor fallback when the engine has nothing -----------------------------
acts = a.decide(obs([(0, 0, 0)], [(1, 1, 3), (2, 2, 3), (3, 3, 1)], view_z=0))
sf = [x for x in acts if x.kind == "show_floor"]
check(sf and sf[0].arg == 3, f"fallback should pick the busiest floor, got {[x.arg for x in sf]}")

# --- already on the right floor: no view churn ------------------------------
acts = a.decide(obs([(0, 0, 2)], [(1, 1, 2)], view_z=2))
check("show_floor" not in kinds(acts) and "zoom_event" not in kinds(acts),
      f"no view change needed, got {kinds(acts)}")

# --- engage the NEAREST hostile ---------------------------------------------
acts = a.decide(obs([(0, 0, 0)], [(20, 20, 0), (2, 1, 0)], view_z=0))
atk = [x for x in acts if x.kind == "attack"]
check(atk and atk[0].arg == (2, 1, 0), f"should attack the nearest, got {[x.arg for x in atk]}")

# --- withdrawal needs BOTH outnumbered and stalled --------------------------
many = [(i, 0, 0) for i in range(12)]
check("withdraw" not in kinds(a.decide(obs([(0, 0, 0)], many, stalls=0))),
      "outnumbered but not stalled must not withdraw")
check("withdraw" not in kinds(a.decide(obs([(0, 0, 0)], [(1, 1, 0)], stalls=99))),
      "stalled but not outnumbered must not withdraw")
check("withdraw" in kinds(a.decide(obs([(0, 0, 0)], many, stalls=99))),
      "outnumbered AND stalled should withdraw")

# --- the extremes behave as advertised --------------------------------------
agg = AggressiveAI()
check("withdraw" not in kinds(agg.decide(obs([(0, 0, 0)], many, stalls=10 ** 6))),
      "aggressive must never withdraw")
check(agg.opening(obs([(0, 0, 0)], []))[0].arg == "auto", "aggressive fires on auto")

cau = CautiousAI()
check("withdraw" in kinds(cau.decide(obs([(0, 0, 0)], [(1, 1, 0), (2, 2, 0)], stalls=5))),
      "cautious withdraws at 2:1 once stalled")
check(cau.opening(obs([(0, 0, 0)], []))[0].arg == "aimed", "cautious aims")

# --- determinism ------------------------------------------------------------
o1 = obs([(0, 0, 0), (1, 0, 0)], [(4, 4, 1), (9, 9, 2)], view_z=0)
check([(x.kind, x.arg) for x in ScriptedAI(seed=1).decide(o1)]
      == [(x.kind, x.arg) for x in ScriptedAI(seed=1).decide(o1)],
      "same observation + seed must give the same actions")

# --- registry ---------------------------------------------------------------
check(make("cautious").name == "cautious", "registry builds by name")
try:
    make("nope")
    check(False, "unknown AI should raise")
except KeyError:
    pass

if FAILED:
    print(f"FAILED {len(FAILED)}:")
    for m in FAILED:
        print("  -", m)
    sys.exit(1)
print("all tactical AI tests passed")

# ---------------------------------------------------------------------------
# VeteranAI — the doctrine rules, each provable on its own
# ---------------------------------------------------------------------------
from oa_ai import VeteranAI, Unit as U

def vobs(mine, foes, view_z=0, stalls=0, last_event_z=None):
    return Observation(mine=mine, foes=foes, view_z=view_z, stalls=stalls,
                       last_event_z=last_event_z)

def mk(uid, x, y, z, hp=100, kind="", hostile=False):
    return U(uid, x, y, z, alive=True, hostile=hostile, health=hp, max_health=100, kind=kind)

v = VeteranAI()

# THREAT PRIORITY: a distant popper outranks an adjacent ordinary hostile.
acts = v.decide(vobs([mk(1, 0, 0, 0)],
                     [mk(90, 1, 1, 0, kind="anthropod", hostile=True),
                      mk(91, 18, 18, 0, kind="popper", hostile=True)]))
ff = [a for a in acts if a.kind == "focus_fire"]
check(ff and ff[0].arg == (18, 18, 0),
      f"popper must be engaged over a closer anthropod, got {[a.arg for a in ff]}")
check("priority" in ff[0].why, f"reason should name it a priority target: {ff[0].why}")

# ...and with no priority kinds present, nearest wins.
acts = v.decide(vobs([mk(1, 0, 0, 0)],
                     [mk(90, 2, 2, 0, kind="anthropod", hostile=True),
                      mk(91, 18, 18, 0, kind="skeletoid", hostile=True)]))
ff = [a for a in acts if a.kind == "focus_fire"]
check(ff and ff[0].arg == (2, 2, 0), f"nearest when no priority kind, got {[a.arg for a in ff]}")

# TRIAGE: a unit below the line is pulled back and excluded from the firing line.
acts = v.decide(vobs([mk(1, 0, 0, 0, hp=20), mk(2, 5, 5, 0, hp=100)],
                     [mk(90, 9, 9, 0, kind="anthropod", hostile=True)]))
pb = [a for a in acts if a.kind == "pull_back"]
check(pb and pb[0].arg == 1, f"the hurt unit should be pulled back, got {[a.arg for a in pb]}")
sq = [a for a in acts if a.kind == "select_squad"]
check(sq and sq[0].arg == 1, f"only the fit unit fights, got {[a.arg for a in sq]}")

# SPACING: two units on top of each other produce a spread order.
acts = v.decide(vobs([mk(1, 5, 5, 0), mk(2, 5, 6, 0)],
                     [mk(90, 20, 20, 0, kind="anthropod", hostile=True)]))
check(any(a.kind == "spread" for a in acts), "adjacent units should be told to spread")
# ...and properly spaced units are left alone.
acts = v.decide(vobs([mk(1, 0, 0, 0), mk(2, 9, 9, 0)],
                     [mk(90, 20, 20, 0, kind="anthropod", hostile=True)]))
check(not any(a.kind == "spread" for a in acts), "spaced units must not be told to spread")

# FIRE MODE BY RANGE
def fm(acts):
    x = [a.arg for a in acts if a.kind == "set_fire_mode"]
    return x[0] if x else None
check(fm(v.decide(vobs([mk(1, 0, 0, 0)], [mk(90, 2, 0, 0, hostile=True)]))) == "auto",
      "close range should fire auto")
check(fm(v.decide(vobs([mk(1, 0, 0, 0)], [mk(90, 30, 0, 0, hostile=True)]))) == "aimed",
      "long range should aim")

# BEHAVIOUR MODE by odds
def beh(acts):
    x = [a.arg for a in acts if a.kind == "set_behaviour"]
    return x[0] if x else None
many = [mk(90 + i, 20, 20, 0, hostile=True) for i in range(8)]
check(beh(v.decide(vobs([mk(1, 0, 0, 0)], many))) == "evasive", "outnumbered -> evasive")
check(beh(v.decide(vobs([mk(i, i, 0, 0) for i in range(6)],
                        [mk(90, 20, 20, 0, hostile=True)]))) == "aggressive",
      "we have the numbers -> aggressive")

# ALWAYS: move individually, reserve TU
acts = v.decide(vobs([mk(1, 0, 0, 0)], [mk(90, 5, 5, 0, hostile=True)]))
check(any(a.kind == "set_move_mode" and a.arg == "individual" for a in acts),
      "must move individually, not as a blob")
check(any(a.kind == "set_reserve" for a in acts), "must reserve TU for a shot")

# FLYER LAYER: engage the target's level when it differs from the view
acts = v.decide(vobs([mk(1, 0, 0, 0)], [mk(90, 5, 5, 3, hostile=True)], view_z=0))
check(any(a.kind == "set_layer" and a.arg == 3 for a in acts),
      f"should select the target's level, got {[(a.kind, a.arg) for a in acts]}")

# WITHDRAW still needs BOTH conditions
check(not any(a.kind == "withdraw" for a in v.decide(vobs([mk(1, 0, 0, 0)], many, stalls=0))),
      "veteran: outnumbered alone must not withdraw")
check(any(a.kind == "withdraw" for a in v.decide(vobs([mk(1, 0, 0, 0)], many, stalls=99))),
      "veteran: outnumbered AND stalled should withdraw")

if FAILED:
    print(f"FAILED {len(FAILED)}:")
    for m in FAILED:
        print("  -", m)
    sys.exit(1)
print("all tactical AI tests passed (including VeteranAI doctrine)")
