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

# --- nothing in sight: hunt while the mission runs, wait only once it is over ---
# This check used to read "no foes should wait", which is the bug it was meant to prevent: under
# fog of war an empty visible list is the NORMAL state late in a mission, and waiting there waits
# forever. Waiting is correct only once the mission itself is over.
acts = a.decide(obs([(0, 0, 0)], []))
check("wait" not in kinds(acts) and "search" in kinds(acts),
      f"nothing in sight while the mission runs should hunt, got {kinds(acts)}")
ended = obs([(0, 0, 0)], [])
ended.hostiles_remain = False
check(kinds(a.decide(ended)) == ["wait"],
      f"once the mission is over, wait, got {kinds(a.decide(ended))}")

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

# --- click the middle of a unit, not the corner of its tile -------------------
# battle_positions reports sx/sy (the tile ORIGIN's projection) and cx/cy (the middle of the box
# the unit is drawn in). For a 2x2 unit those are different tiles, so an order aimed at sx/sy can
# select a neighbour or the bare ground. Anything aiming AT a unit must prefer cx/cy.
boxed = FakeCaps({
    "mine_at": "10,10,0:sx=100:sy=200:cx=118:cy=209",
    "foe_at": ("20,20,4:sx=300:sy=400:cx=336:cy=418:kind=AGENTTYPE_MEGASPAWN:large=1:flying=0;"
               "24,20,4:sx=500:sy=400:cx=518:cy=409:kind=AGENTTYPE_ANTHROPOD:large=0:flying=0"),
    "view_z": "4",
})
ob = _observe(boxed)
big, small = ob.foes[0], ob.foes[1]
check(big.large and not small.large, "the large flag must survive the parse")
check(big.click_point == (336, 418), f"a large unit aims at its centre, got {big.click_point}")
check(big.click_point != (big.sx, big.sy),
      "…which for a 2x2 unit is NOT the tile corner -- that is the bug this exists to prevent")
check(small.click_point == (518, 409), "a normal unit aims at its centre too")
check(ob.mine[0].click_point == (118, 209), "our own units carry a click point as well")

# The executor must resolve a named target to that click point.
check(_screen_of((20, 20, 4), boxed) == (336, 418),
      f"_screen_of must return the drawn centre, got {_screen_of((20, 20, 4), boxed)}")

# Older engine builds send no cx/cy. Fall back to sx/sy rather than refusing the order outright.
legacy = FakeCaps({"mine_at": "-", "foe_at": "9,9,0:sx=70:sy=80:kind=X:large=0", "view_z": "0"})
check(_observe(legacy).foes[0].click_point == (70, 80),
      "with no cx/cy, fall back to the tile projection")
check(Unit(1, 0, 0, 0).click_point is None, "…and to None when there is nothing on screen")

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

# --- "I cannot see one" must never mean "there are none" ----------------------
# Under fog of war the last alien is usually unspotted. Both AIs used to read an empty visible
# list as victory and return wait() -- the one action that can never end a mission, since a
# mission ends only when the last hostile dies. Observed live: an 8-versus-1 base defence spent
# 600 rounds printing "no hostiles left" and was scored a timeout at 0.16, for a battle the engine
# had already marked player_won.
#
# The signal is a boolean: is the mission still running? A player needs nothing more than that,
# and neither does this. A COUNT was tried first and was the same bug in a quieter costume -- it
# had an "unknown" case, and unknown meant stop.
blind = Observation(mine=[U(1, 10, 10, 0), U(2, 12, 10, 2)], foes=[], view_z=0,
                    stalls=7, mission_type="base_defense", mode="rt", hostiles_remain=True)
check(blind.hunting, "mission still running + nothing visible == hunting")
check(blind.foes_alive == 0, "…and the visible count is still honestly zero")

over = Observation(mine=[U(1, 10, 10, 0)], foes=[], view_z=0, hostiles_remain=False,
                   mission_type="x", mode="rt")
check(not over.hunting, "once the mission is over there is nothing to hunt")

# The default must be to keep moving. A missing or unreadable field is exactly when the old bug
# came back, so the safe direction is hunt-anyway: searching a dead map costs seconds, waiting on
# a live one costs the mission.
check(Observation(mine=[U(1, 1, 1, 0)], foes=[]).hunting,
      "the DEFAULT must be to keep hunting, not to stand still")

for ai in (VeteranAI(), ScriptedAI(), AggressiveAI(), CautiousAI()):
    kinds = {a.kind for a in ai.decide(blind)}
    check("wait" not in kinds,
          f"{type(ai).__name__} must not wait while the mission runs: {kinds}")
    check("search" in kinds, f"{type(ai).__name__} must sweep for it: {kinds}")
    check("select_squad" in kinds, f"{type(ai).__name__} must send the whole squad: {kinds}")
    done = {a.kind for a in ai.decide(over)}
    check(done == {"wait"}, f"{type(ai).__name__} must wait once the mission ends: {done}")

# The sweep must look at the floors the squad occupies -- a squad split across levels (the usual
# state after a fight moves through a building) otherwise searches only the level it is on.
floors = {blind.sweep_floor(n) for n in range(12)}
check(len(floors) > 1, f"the hunt must look at more than one floor, got {floors}")
check(0 in floors and 2 in floors, f"…including both levels the squad is on, got {floors}")

# Successive rounds must probe different ground rather than re-clicking one spot.
steps = {a.arg for n in range(6)
         for a in VeteranAI().decide(
             Observation(mine=[U(1, 10, 10, 0)], foes=[], view_z=0, stalls=n,
                         mission_type="x", mode="rt"))
         if a.kind == "search"}
check(len(steps) == 6, f"successive rounds must probe different ground, got {steps}")

# --- being hunted is not hunting ---------------------------------------------
# Taking casualties from an enemy nobody can see calls for the OPPOSITE of a sweep. The first
# version of hunt() spread the squad out, stood it up and ran it into the open; in a base defence
# against nine unseen aliens that took fifteen soldiers to none, scored 0.00. A search pattern is
# for an empty map, not for ground the enemy already holds.
pressed = Observation(mine=[U(1, 10, 10, 0), U(2, 12, 10, 0)], foes=[], view_z=0, stalls=9,
                      mission_type="base_defense", mode="rt", hard_pressed=True)
check(pressed.hunting, "still hunting -- standing still is never the answer")
p_acts = {a.kind: a.arg for a in VeteranAI().decide(pressed)}
check(p_acts.get("set_move_mode") == "group",
      f"under fire the squad must stay together, got {p_acts.get('set_move_mode')}")
check(p_acts.get("set_stance") == "kneel",
      f"…and get low rather than run upright, got {p_acts.get('set_stance')}")
check(p_acts.get("set_behaviour") == "evasive",
      f"…and not charge into it, got {p_acts.get('set_behaviour')}")
check("search" in p_acts, "…but must still keep moving, or this is the old deadlock again")

# The ordinary hunt keeps the aggressive sweep: an empty map is not a threat.
calm = Observation(mine=[U(1, 10, 10, 0), U(2, 12, 10, 0)], foes=[], view_z=0, stalls=9,
                   mission_type="ufo_recovery", mode="rt", hard_pressed=False)
c_acts = {a.kind: a.arg for a in VeteranAI().decide(calm)}
check(c_acts.get("set_move_mode") == "individual", "an unthreatened sweep still spreads out")
check(c_acts.get("set_stance") == "run", "…and still covers ground at a run")
check(c_acts.get("set_behaviour") == "aggressive", "…and still seeks contact")

# The executor must not split a squad that was told to move as one.
from oa_executor import execute as _execute


class SweepCaps(FakeCaps):
    def __init__(self):
        super().__init__({"mine_at": "-", "foe_at": "-"})
        self.splits = []

    def sweep(self, step, split=True):
        self.splits.append(split)
        return True

    def set_move_mode(self, m):
        return True

    def set_stance(self, m):
        return True

    def set_fire_mode(self, m):
        return True

    def set_behaviour(self, m):
        return True

    def show_floor(self, z):
        return None

    def select_units(self, n):
        return n


cap = SweepCaps()
_execute(cap, VeteranAI().decide(pressed))
check(cap.splits == [False],
      f"a grouped squad must get ONE destination, not two corners: {cap.splits}")
cap2 = SweepCaps()
_execute(cap2, VeteranAI().decide(calm))
check(cap2.splits == [True], f"an individual sweep still splits: {cap2.splits}")

# --- being wiped out while winning still ends with being wiped out -----------
# The withdraw rule required outnumbered AND stalled. stalls resets every time the foe count
# changes, so a squad trading one soldier per alien never reads as stalled and fights to the last
# man on the grounds that it is making progress. Observed: six soldiers against twenty-five,
# hostiles 20 -> 16 while the squad went 6 -> 1, no withdrawal at any point.
bleeding = Observation(
    mine=[U(1, 10, 10, 0)],
    foes=[U(100 + i, 20 + i, 20, 0, hostile=True) for i in range(16)],
    view_z=0, stalls=0, mission_type="extermination", mode="rt", hard_pressed=True)
check(bleeding.stalls == 0, "the squad is still killing, so nothing reads as stalled")
for ai in (VeteranAI(withdraw_ratio=100.0), ScriptedAI(withdraw_ratio=100.0)):
    kinds = [a.kind for a in ai.decide(bleeding)]
    check("withdraw" in kinds,
          f"{type(ai).__name__} must leave at 16:1 having lost a third: {kinds}")

# A squad that is merely outnumbered but intact fights on -- withdrawing early throws away
# missions that are winnable, and survivors of a raid hand the aliens their building back.
intact = Observation(
    mine=[U(i, 10 + i, 10, 0) for i in range(6)],
    foes=[U(100 + i, 20 + i, 20, 0, hostile=True) for i in range(12)],
    view_z=0, stalls=0, mission_type="extermination", mode="rt", hard_pressed=False)
for ai in (VeteranAI(withdraw_ratio=100.0), ScriptedAI(withdraw_ratio=100.0)):
    check("withdraw" not in [a.kind for a in ai.decide(intact)],
          f"{type(ai).__name__} must not withdraw with the squad intact")

# NEVER from a base defence: leaving forfeits the base, its facilities, stores and staff. Whether
# that is survivable depends on owning a second base, which this layer cannot see.
base = Observation(
    mine=[U(1, 10, 10, 0)],
    foes=[U(100 + i, 20 + i, 20, 0, hostile=True) for i in range(16)],
    view_z=0, stalls=99, mission_type="base_defense", mode="rt", hard_pressed=True)
for ai in (VeteranAI(withdraw_ratio=1.0, withdraw_stalls=1), ScriptedAI(withdraw_ratio=1.0,
                                                                       withdraw_stalls=1)):
    check("withdraw" not in [a.kind for a in ai.decide(base)],
          f"{type(ai).__name__} must never concede a base defence on its own judgement")

# Seeing a hostile again must end the hunt and resume fighting.
seen = Observation(mine=[U(1, 10, 10, 0)], foes=[U(9, 10, 16, 0, hostile=True)], view_z=0,
                   mission_type="x", mode="rt")
check(not seen.hunting, "a visible hostile is not a hunt")
check("search" not in {a.kind for a in VeteranAI().decide(seen)},
      "…and the squad must go back to fighting it")


# --- base defence: control the base, never search it -------------------------
# Aliens enter through the access lift and the vehicle repair bay and nowhere else, so this is a
# geometry problem with a known answer. Treating it as a hunt produced the worst results of every
# run: 0 of 15 survivors, a squad scattered across a base it should have been holding.
FACILITIES = [
    {"x": 4,  "y": 4,  "type": "FACILITYTYPE_ACCESS_LIFT"},
    {"x": 30, "y": 4,  "type": "FACILITYTYPE_VEHICLE_REPAIR_BAY"},
    {"x": 17, "y": 30, "type": "FACILITYTYPE_LIVING_QUARTERS"},
    {"x": 17, "y": 8,  "type": "FACILITYTYPE_STORES"},
]
_squad = [U(i, 10 + i, 10, 0) for i in range(4)]
_civvies = [U(50 + i, 12 + i, 12, 0) for i in range(3)]
for _c in _civvies:
    _c.armed = False

quiet = Observation(mine=_squad + _civvies, foes=[], view_z=0, mission_type="base_defense",
                    mode="rt", facilities=FACILITIES)
check(quiet.is_base_defence, "mission type must be recognised")
check(len(quiet.entries) == 2, f"lift and repair bay are the entries, got {quiet.entries}")
check(len(quiet.combatants) == 4 and len(quiet.noncombatants) == 3,
      "armed and unarmed must be told apart")

# The refuge must be far from BOTH doors, not merely far from one.
_r = quiet.refuge()
check(_r and _r["type"] == "FACILITYTYPE_LIVING_QUARTERS",
      f"refuge should be the far facility, got {_r}")
check(_r["type"] not in Observation.ENTRY_FACILITIES, "never shelter people on a door")

_acts = {a.kind: a.arg for a in VeteranAI().decide(quiet)}
check("search" not in _acts, f"a base defence must NEVER hunt: {sorted(_acts)}")
check("wait" not in _acts, "…nor stand idle")
check(_acts.get("set_move_mode") == "group", "hold as one body")
check(_acts.get("set_stance") == "kneel", "dug in on the door while nothing is visible")
check("move_group" in _acts, "orders must actually be issued, not just computed")

# A move order goes to whoever is SELECTED. The squad selection from the previous round is still
# live, so the non-combatants must be taken hold of BEFORE they are told to move -- otherwise the
# order reaches the soldiers and the civilians are killed where they stand. Observed exactly that
# in a live base defence: 21 -> 20 -> 18 -> 15 while this rule "ran" every round.
_kinds = [a.kind for a in VeteranAI().decide(quiet)]
check("select_units" in _kinds, "non-combatants must be selected before being moved")
check(_kinds.index("select_units") < _kinds.index("move_group"),
      f"selection must come FIRST -- ordering is the whole bug: {_kinds}")
_sel = next(a for a in VeteranAI().decide(quiet) if a.kind == "select_units")
check(set(_sel.arg) == {u.uid for u in quiet.noncombatants},
      "it must select exactly the non-combatants, not the squad")
# And the squad is re-selected afterwards, or the press order would move civilians into contact.
check(_kinds.index("select_squad") > _kinds.index("move_group"),
      f"the squad must be re-taken after the civilians are sent away: {_kinds}")

# With a hostile visible, press it -- and press the one nearest a door.
_near = U(90, 5, 5, 0, hostile=True)
_far = U(91, 25, 25, 0, hostile=True)
contact = Observation(mine=_squad + _civvies, foes=[_far, _near], view_z=0,
                      mission_type="base_defense", mode="rt", facilities=FACILITIES)
_c_acts = {a.kind: a.arg for a in VeteranAI().decide(contact)}
check(_c_acts.get("focus_fire") == (5, 5, 0),
      f"press the hostile nearest an entry, got {_c_acts.get('focus_fire')}")
check(_c_acts.get("set_behaviour") == "aggressive", "push them, never let them into the base")
check("search" not in _c_acts, "still never a search")

# Without a known layout the doctrine must not fire -- it would be guessing at geometry.
blind = Observation(mine=_squad, foes=[], view_z=0, mission_type="base_defense", mode="rt",
                    facilities=[])
check("search" in {a.kind for a in VeteranAI().decide(blind)},
      "with no layout, fall back to the ordinary hunt rather than inventing doors")

ufo = Observation(mine=_squad, foes=[], view_z=0, mission_type="ufo_recovery", mode="rt",
                  facilities=FACILITIES)
check("search" in {a.kind for a in VeteranAI().decide(ufo)}, "a UFO recovery is still a hunt")

# A base with no armed unit cannot hold anything, and the rest of the doctrine makes it WORSE:
# grouping the staff into one far room hands a single alien a queue to work through. Observed --
# 15 unarmed, one hostile, count falling 15 -> 14 -> 12 while the AI ordered a chokepoint nobody
# could man. Scatter instead: ten people in ten directions are ten problems for one alien.
_only_civs = [U(200 + i, 10 + i, 10, 0) for i in range(10)]
for _c in _only_civs:
    _c.armed = False
undefended = Observation(mine=_only_civs, foes=[], view_z=0, mission_type="base_defense",
                         mode="rt", facilities=FACILITIES)
_u = {a.kind: a.arg for a in VeteranAI().decide(undefended)}
check(_u.get("set_move_mode") == "individual",
      f"with nobody armed they must SCATTER, not group: {_u.get('set_move_mode')}")
check("move_group" not in _u, "never gather defenceless people into one room")
check(_u.get("set_stance") == "run", "distance is the only cover they have")
check("search" in _u, "keep moving rather than standing still to be found")
check("select_squad" not in _u, "there is no squad to select")

# One armed unit is still a defence, and must go back to holding the door.
_mixed = list(_only_civs)
_gun = U(300, 9, 9, 0)
_mixed.append(_gun)
defended = Observation(mine=_mixed, foes=[], view_z=0, mission_type="base_defense", mode="rt",
                       facilities=FACILITIES)
_d = {a.kind: a.arg for a in VeteranAI().decide(defended)}
check(_d.get("set_move_mode") == "group", "one rifle is enough to hold a chokepoint")
check("move_group" in _d, "…and the civilians are moved out of the way again")


# --- you cannot hunt your way out of being wiped out -------------------------
# `hunting` is true whenever no hostile is VISIBLE -- exactly the state a squad is in while being
# shot from cover. The withdrawal rule sat BELOW the hunt check, so it was unreachable in the one
# situation it exists for. Cost a full squad: six out on a UFO recovery, four dead, seven hostiles
# to two soldiers, and the AI's last decision was "advance together toward the threat".
_two = [U(1, 10, 10, 0), U(2, 11, 10, 0)]
_seven = [U(100 + i, 20 + i, 20, 0, hostile=True) for i in range(7)]

visible = Observation(mine=_two, foes=_seven, view_z=0, mission_type="ufo_recovery", mode="rt",
                      hard_pressed=True)
check([a.kind for a in VeteranAI().decide(visible)] == ["withdraw"],
      "7 to 2 with a third lost must withdraw, not manoeuvre")

from_cover = Observation(mine=_two, foes=[], view_z=0, mission_type="ufo_recovery", mode="rt",
                         hard_pressed=True)
check([a.kind for a in VeteranAI().decide(from_cover)] == ["withdraw"],
      "being shot by an enemy you cannot see is not a reason to go looking for it")

home = Observation(mine=_two, foes=[], view_z=0, mission_type="base_defense", mode="rt",
                   hard_pressed=True)
check("withdraw" not in {a.kind for a in VeteranAI().decide(home)},
      "a base defence is never abandoned, however bad it gets")

intact = Observation(mine=[U(i, 10 + i, 10, 0) for i in range(6)], foes=[], view_z=0,
                     mission_type="ufo_recovery", mode="rt", hard_pressed=False)
check("search" in {a.kind for a in VeteranAI().decide(intact)},
      "an unhurt squad with nothing in sight still searches")

if FAILED:
    print(f"FAILED {len(FAILED)}:")
    for m in FAILED:
        print("  -", m)
    sys.exit(1)
print("all VeteranAI doctrine-knob, observe() parsing and fog-of-war hunt tests passed")
