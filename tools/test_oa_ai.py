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


# --- every doctrine knob must actually change what the squad does -------------
# VeteranAI used to hardcode fire_mode/stance in its super() call and hardcode behaviour,
# move_mode, reserve, focus_fire and priority_targets in decide(). A genome could carry all ten
# X-COM genes and change nothing. Each check below fails if its knob goes back to being a constant.
def acts_of(ai, obs):
    return {a.kind: a.arg for a in ai.decide(obs)}


# An even fight at mid range: the band where doctrine is a choice rather than a reflex.
mid = Observation(
    mine=[U(1, 10, 10, 0), U(2, 14, 10, 0), U(3, 18, 10, 0)],
    foes=[U(9, 10, 20, 0, hostile=True, kind="anthropod")],
    view_z=0, stalls=0, mission_type="ufo_recovery", mode="rt")

check(acts_of(VeteranAI(fire_mode="aimed"), mid).get("set_fire_mode") == "aimed",
      "fire_mode must reach the mid-range band")
check(acts_of(VeteranAI(fire_mode="auto"), mid).get("set_fire_mode") == "auto",
      "…and not be pinned to one value")
check(acts_of(VeteranAI(behaviour="aggressive"), mid).get("set_behaviour") == "aggressive",
      "behaviour must reach the even-fight band")
check(acts_of(VeteranAI(move_mode="group"), mid).get("set_move_mode") == "group",
      "move_mode must be settable, even to the worse option")
check(acts_of(VeteranAI(reserve="aimed"), mid).get("set_reserve") == "aimed",
      "reserve must be settable")
check(VeteranAI(stance="kneel").stance == "kneel", "stance must survive construction")

# The adaptive rules must STAY adaptive: the genome sets the middle, not the extremes.
close = Observation(mine=[U(1, 10, 10, 0)], foes=[U(9, 11, 11, 0, hostile=True)],
                    view_z=0, stalls=0, mission_type="x", mode="rt")
check(acts_of(VeteranAI(fire_mode="aimed"), close).get("set_fire_mode") == "auto",
      "close quarters must still force auto regardless of the gene")

# focus_fire: concentrate on one target, or let each unit choose.
check("focus_fire" in acts_of(VeteranAI(focus_fire=True), mid),
      "focus_fire=True concentrates the squad")
off = acts_of(VeteranAI(focus_fire=False), mid)
check("focus_fire" not in off and "attack" in off,
      "focus_fire=False must engage WITHOUT concentrating")
check("select_squad" not in off, "…and must not select the whole squad onto one target")

# priority_targets: a distant popper outranks a near anthropod, unless the gene says otherwise.
threat = Observation(
    mine=[U(1, 10, 10, 0)],
    foes=[U(8, 12, 10, 0, hostile=True, kind="anthropod"),      # near, ordinary
          U(9, 10, 24, 0, hostile=True, kind="popper")],        # far, lethal
    view_z=0, stalls=0, mission_type="x", mode="rt")
on_t = acts_of(VeteranAI(priority_targets=True), threat)
off_t = acts_of(VeteranAI(priority_targets=False), threat)
check(on_t.get("focus_fire") == (10, 24, 0),
      f"priority_targets=True must kill the popper first, got {on_t.get('focus_fire')}")
check(off_t.get("focus_fire") == (12, 10, 0),
      f"priority_targets=False must take the nearest, got {off_t.get('focus_fire')}")

# --- observe() must read the fields the engine actually sends -----------------
# battle_positions entries are "x,y,z" plus colon-separated key=value fields. The old parser took
# the FIRST colon field as the unit kind, so kind was literally the string "large=0" -- and every
# VeteranAI priority rule, which looks for "popper"/"brainsucker" inside it, was dead on arrival.
from oa_executor import observe as _observe, _screen_of


class FakeCaps:
    def __init__(self, pos, state=None):
        self._pos, self._state = pos, state or {"mission_type": "x", "mode": "rt"}

    def battle_positions(self):
        return self._pos

    def battle_state(self):
        return self._state


caps = FakeCaps({
    "mine_at": "10,10,0:sx=100:sy=200;12,10,0:sx=140:sy=200",
    "foe_at": ("20,20,4:sx=300:sy=400:kind=AGENTTYPE_POPPER:large=0:flying=0;"
               "22,20,4:sx=-1:sy=-1:kind=AGENTTYPE_ANTHROPOD:large=0:flying=0"),
    "view_z": "4",
})
o = _observe(caps)
check(len(o.mine) == 2 and len(o.foes) == 2, "observe must read both sides")
check(o.foes[0].kind == "AGENTTYPE_POPPER",
      f"kind must be the agent type, got {o.foes[0].kind!r}")
check("popper" in o.foes[0].kind.lower(), "…so the priority rules can match on it")
check((o.foes[0].sx, o.foes[0].sy) == (300, 400), "on-screen units carry screen coordinates")
check(o.foes[1].sx is None and o.foes[1].sy is None,
      "sx=-1 means off screen and must become None, not a click at (-1,-1)")
check(o.mine[1].x == 12 and o.mine[1].sx == 140, "our own units carry them too")
check(o.view_z == 4, "view_z is read")

# Threat priority must now actually fire on real engine data.
v = VeteranAI(priority_targets=True)
tgt = {a.kind: a.arg for a in v.decide(o)}.get("focus_fire")
check(tgt == (20, 20, 4), f"the popper must be chosen over the nearer anthropod, got {tgt}")

# _screen_of resolves a chosen tile target to the click point, and refuses when it cannot.
check(_screen_of((20, 20, 4), caps) == (300, 400), "a chosen target resolves to its click point")
check(_screen_of((22, 20, 4), caps) is None, "an off-screen target must refuse, not guess")
check(_screen_of((99, 99, 9), caps) is None, "an unknown target must refuse")
check(_screen_of((7, 8), caps) == (7, 8), "a ready-made screen pair passes through")
check(_screen_of(None, caps) is None and _screen_of("x", caps) is None, "junk refuses")

# Empty and absent fields must not crash the parser.
check(_observe(FakeCaps({"mine_at": "-", "foe_at": "-"})).foes == [], "'-' means none")
check(_observe(FakeCaps({})).mine == [], "absent fields mean none")

if FAILED:
    print(f"FAILED {len(FAILED)}:")
    for m in FAILED:
        print("  -", m)
    sys.exit(1)
print("all VeteranAI doctrine-knob and observe() parsing tests passed")
