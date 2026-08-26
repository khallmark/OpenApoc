#!/usr/bin/env python3
"""Tactical AI: decisions only. No harness, no clicking, no engine.

This module is deliberately unable to touch the game. It imports nothing from oa_play, opens no
sockets, and resolves no controls. It takes an Observation -- a plain snapshot of what the driver
could see -- and returns Actions, which are descriptions of intent that somebody else executes.

Why the boundary is drawn here: the driver had grown a battle loop where "read the screen",
"decide what to do" and "click the thing" were the same forty lines. That made the AI impossible
to test without a running game, impossible to compare against another AI, and impossible to
change without risking the plumbing. Splitting them means the interesting half -- what a squad
should actually do -- can be exercised in a unit test in milliseconds, and swapped wholesale.

The contract is one method:

    ai.decide(obs) -> list[Action]

Everything in this file is pure. Given the same Observation and the same seed, decide() returns
the same Actions. That is what makes an arena run reproducible and a regression diagnosable.
"""

from __future__ import annotations

import random
from dataclasses import dataclass, field
from typing import Optional

# ---------------------------------------------------------------------------
# What the AI is allowed to know
# ---------------------------------------------------------------------------


@dataclass
class Unit:
    """One unit as the driver can observe it. Positions are tile coordinates.

    Everything here is on-screen information: the squad panel shows health and TU for your own
    units, and a hostile you can see has a visible type and position. Nothing is read from a
    field a player could not see.
    """

    uid: int
    x: int
    y: int
    z: int
    alive: bool = True
    hostile: bool = False
    health: int = 100
    max_health: int = 100
    tu: int = 100
    kind: str = ""          # lowercase type name, e.g. "popper", "brainsucker"
    # Where this unit is drawn, or None when it is off screen. Tile coordinates say WHERE a unit
    # is; only these say where to click to give it an order, and without them the executor could
    # not act on a target the AI had named -- it fell back to whatever hostile was listed first.
    sx: Optional[int] = None
    sy: Optional[int] = None

    @property
    def hurt_frac(self) -> float:
        return 1.0 - (self.health / self.max_health if self.max_health else 1.0)

    def dist2(self, other: "Unit") -> int:
        return (self.x - other.x) ** 2 + (self.y - other.y) ** 2 + ((self.z - other.z) * 4) ** 2


@dataclass
class Observation:
    """A snapshot of the battle. Everything the AI gets; nothing it cannot see in-game.

    Deliberately NOT a handle to the engine: no callbacks, no live queries. If a decision needs a
    fact, that fact has to be added here explicitly, which keeps the AI honest about what
    information it is actually using.
    """

    mine: list[Unit] = field(default_factory=list)
    foes: list[Unit] = field(default_factory=list)
    view_z: int = 0
    turn: int = 0
    mode: str = "rt"
    mission_type: str = "unknown"
    # Is the mission still running? That is the whole signal. Battle::checkMissionEnd ends a
    # mission by itself the moment no hostile organisation has a conscious unit left, so while we
    # are still in it there is by definition something left to find. A player needs nothing more
    # than this: they keep moving until the debriefing appears.
    #
    # Deliberately a boolean and deliberately defaulting to True. An earlier version passed the
    # engine's hostile COUNT and treated "unknown" as "stop", which is the same standing-still bug
    # in a quieter costume -- one unparsed field and the squad waits out the clock again. The safe
    # default is to keep hunting; the cost of searching an empty map is a few seconds, and the cost
    # of waiting on a live one is the entire mission.
    hostiles_remain: bool = True
    # Are we losing people? Also a boolean, and for the same reason.
    #
    # Taking casualties from something nobody can see does not mean we are hunting -- it means we
    # are BEING hunted, and the two call for opposite behaviour. Measured, at the cost of a whole
    # squad: a base defence where the hunt spread fifteen soldiers out, stood them up and ran them
    # into the open looking for nine aliens that had already seen them. 15 -> 8 -> 5 -> 3 -> 0,
    # every one of them picked off alone, scored 0.00. The fix for standing still had become a way
    # to lose faster.
    hard_pressed: bool = False
    # Set when the engine has told us where something just happened.
    last_event_z: Optional[int] = None
    # Rounds since the foe count last dropped -- the driver's own stall signal.
    stalls: int = 0

    @property
    def mine_alive(self) -> int:
        return sum(1 for u in self.mine if u.alive)

    @property
    def foes_alive(self) -> int:
        """How many hostiles we can SEE. Not how many are left -- see foes_remaining."""
        return sum(1 for u in self.foes if u.alive)

    @property
    def hunting(self) -> bool:
        """The mission is still on and nothing is in sight: go and find something.

        Under fog of war the last alien is usually unspotted, so "I can see none" and "there are
        none" look identical from inside the squad -- and only one of them is a reason to stop.
        A squad that waits for an unspotted enemy waits forever, because the mission ends only
        when the last hostile dies.
        """
        return self.foes_alive == 0 and self.hostiles_remain

    def sweep_floor(self, step: int) -> int:
        """A floor to look at while hunting, cycling over the levels the squad occupies and one
        above them. A squad split across floors -- which is the usual state after a fight moves
        through a building -- otherwise searches only the level it is already looking at."""
        levels = sorted({u.z for u in self.mine if u.alive}) or [self.view_z]
        levels = sorted(set(levels + [min(levels) , max(levels) + 1]))
        return levels[step % len(levels)]

    def foes_on(self, z: int) -> int:
        return sum(1 for u in self.foes if u.alive and u.z == z)


# ---------------------------------------------------------------------------
# What the AI is allowed to ask for
# ---------------------------------------------------------------------------


@dataclass
class Action:
    """An intent. The executor decides how to express it through the UI.

    kind:
      set_fire_mode  arg: "snap" | "aimed" | "auto"
      set_stance     arg: "run" | "walk" | "kneel" | "prone"
      show_floor     arg: int          -- bring the view to this level
      zoom_event                       -- ask the engine for the last event location
      select_squad   arg: int          -- how many of our units to select
      attack         arg: (x, y, z)    -- engage the unit at this tile
      move           arg: (x, y, z)    -- move the selection here
      withdraw                         -- leave the battle
      wait                             -- do nothing this round
      focus_fire     arg: (x, y, z)    -- everyone engages this one target
      pull_back      arg: int          -- send this unit away from the fighting
      spread         arg: int          -- this unit is too close to a neighbour
      set_behaviour  arg: "aggressive" | "normal" | "evasive"
      set_move_mode  arg: "group" | "individual"
      set_reserve    arg: "aimed" | "snap" | "auto" | "kneel" | "none"
      set_layer      arg: int          -- absolute level for flying units
      cease_fire     arg: bool         -- hold fire (True) or resume (False)
      throw          arg: (x, y, z)    -- grenade at this tile
    """

    kind: str
    arg: object = None
    why: str = ""


# ---------------------------------------------------------------------------
# The AIs
# ---------------------------------------------------------------------------


class TacticalAI:
    """Interface. Subclass and implement decide()."""

    name = "base"

    def opening(self, obs: Observation) -> list[Action]:
        """Called once when the battle starts."""
        return []

    def decide(self, obs: Observation) -> list[Action]:
        raise NotImplementedError


class ScriptedAI(TacticalAI):
    """The behaviour the driver had, made explicit and testable.

    Reconstructed from what oa_play's battle loop actually did, not from what it was documented to
    do. Three rules, in priority order:

      1. Fight on the floor where something is happening. The engine knows this (zoom_event); the
         floor with the most hostiles is a fallback, and a worse question -- "where are most of
         them" is not "where is the fight".
      2. Shoot the nearest live hostile. Movement orders cannot target an occupied tile, so an
         attack has to be expressed as an attack.
      3. Withdraw only when badly outnumbered AND stalled. Withdrawing early puts the aliens back
         on the map; withdrawing never is how a squad gets wiped 6-0.
    """

    name = "scripted"

    def __init__(self, fire_mode: str = "snap", stance: str = "run",
                 withdraw_ratio: float = 6.0, withdraw_stalls: int = 12,
                 seed: int = 0):
        self.fire_mode = fire_mode
        self.stance = stance
        self.withdraw_ratio = withdraw_ratio
        self.withdraw_stalls = withdraw_stalls
        self.rng = random.Random(seed)

    def opening(self, obs: Observation) -> list[Action]:
        return [
            Action("set_fire_mode", self.fire_mode, "opening loadout"),
            Action("set_stance", self.stance, "opening posture"),
        ]

    def hunt(self, obs: Observation) -> list[Action]:
        """Go and find the hostiles we know are out there but cannot see.

        The squad stops being a firing line and becomes a search party: spread out so more ground
        is covered and one ambush cannot catch everyone, move at a run, look at a different floor
        each round, and sweep. Fire mode goes to snap because a unit rounding a corner onto the
        last alien needs to shoot now, not line up a shot.

        This is what a player does without thinking about it, and its absence turned an 8-versus-1
        base defence into six hundred rounds of standing still -- scored a timeout at 0.16 for a
        battle the engine had already marked won.
        """
        alive = max(1, sum(1 for u in obs.mine if u.alive))
        if obs.hard_pressed:
            # We are not hunting, we are being hunted. Something is shooting people we cannot
            # see, so the ground it is standing on is not ground to walk into one at a time.
            # Concentrate, get low, and keep moving as one body: still searching -- standing
            # still is what this whole branch exists to prevent -- but mutually supporting, so
            # contact is made by the squad rather than by whichever soldier wandered into it.
            return [
                Action("show_floor", obs.sweep_floor(obs.stalls),
                       "under fire from an unseen enemy: regroup, do not scatter"),
                Action("set_move_mode", "group", "stay together; scattered soldiers die alone"),
                Action("set_stance", "kneel", "smaller target, steadier shot"),
                Action("set_fire_mode", "snap", "react, do not line up a shot"),
                Action("set_behaviour", "evasive", "break contact rather than trade in the open"),
                Action("select_squad", alive, "the whole squad moves as one"),
                Action("search", obs.stalls, "advance together toward the threat"),
            ]
        return [
            Action("show_floor", obs.sweep_floor(obs.stalls),
                   "hunting: mission still running, nothing in sight"),
            Action("set_move_mode", "individual", "spread the search out"),
            Action("set_stance", "run", "cover ground"),
            Action("set_fire_mode", "snap", "shoot on contact"),
            Action("set_behaviour", "aggressive", "seek contact rather than avoid it"),
            Action("select_squad", alive, "everyone searches"),
            Action("search", obs.stalls, "sweep for the hostiles the engine says remain"),
        ]

    def decide(self, obs: Observation) -> list[Action]:
        acts: list[Action] = []
        if obs.hunting:
            return self.hunt(obs)
        if obs.foes_alive == 0:
            return [Action("wait", None, "no hostiles left; the engine ends the mission")]

        # 1. Be where the fight is.
        if obs.last_event_z is not None and obs.last_event_z != obs.view_z:
            acts.append(Action("zoom_event", None, "engine's own last-event location"))
        else:
            best = max({u.z for u in obs.foes if u.alive},
                       key=lambda z: (obs.foes_on(z), -z), default=None)
            if best is not None and best != obs.view_z:
                acts.append(Action("show_floor", best,
                                   f"{obs.foes_on(best)} hostiles on {best}, fallback"))

        # 2. Withdraw only when it is genuinely lost.
        if obs.mine_alive and obs.foes_alive / max(1, obs.mine_alive) >= self.withdraw_ratio \
                and obs.stalls >= self.withdraw_stalls:
            acts.append(Action("withdraw", None,
                               f"outnumbered {obs.foes_alive}:{obs.mine_alive} and stalled "
                               f"{obs.stalls} rounds"))
            return acts

        # 3. Engage. Nearest live hostile to the centre of mass of our own live units.
        live_mine = [u for u in obs.mine if u.alive]
        live_foes = [u for u in obs.foes if u.alive]
        if live_mine and live_foes:
            cx = sum(u.x for u in live_mine) / len(live_mine)
            cy = sum(u.y for u in live_mine) / len(live_mine)
            target = min(live_foes, key=lambda f: (f.x - cx) ** 2 + (f.y - cy) ** 2)
            acts.append(Action("select_squad", min(6, len(live_mine)), "engage as a group"))
            acts.append(Action("attack", (target.x, target.y, target.z),
                               "nearest live hostile"))
        return acts or [Action("wait", None, "nothing to do this round")]


class VeteranAI(ScriptedAI):
    """Doctrine, not "walk at the nearest thing and shoot it".

    ScriptedAI charges the closest hostile with the whole squad. That loses agents to the two
    things this game punishes hardest -- suicide bombers and bunching -- and it was returning
    2 survivors from 19 in observed raids. Six rules, in priority order, each testable on its own:

      1. TRIAGE. A unit under `pull_back_at` health leaves the firing line. Agents are the
         campaign's scarcest resource: attrition, not defeat, is what ends runs, and a wounded
         agent walked out is one who raids again next week.
      2. THREAT PRIORITY. Poppers and brainsuckers are killed first regardless of range. A popper
         detonates on contact and a brainsucker removes an agent outright, so both are worth more
         than a closer, ordinary target. This is the single biggest deviation from "nearest".
      3. FOCUS FIRE. The whole squad engages ONE target per round. Spreading fire across three
         hostiles leaves three alive and shooting; concentrating it removes one shooter per round.
      4. SPACING. Units closer than `min_spacing` to a neighbour are told to spread. One
         explosive on a bunched squad is how six agents die at once.
      5. RANGE-APPROPRIATE FIRE. Auto up close, aimed at distance -- the accuracy/rate trade the
         fire modes exist to express, instead of one setting for the whole battle.
      6. WITHDRAW. Same two-condition rule as ScriptedAI: outnumbered AND stalled, never one
         alone.
    """

    name = "veteran"

    # Killed on sight, in this order, however far away they are.
    PRIORITY_KINDS = ("popper", "brainsucker", "psimorph", "spitter")

    def __init__(self, pull_back_at: float = 0.5, min_spacing: int = 2,
                 close_range: int = 6, long_range: int = 14,
                 withdraw_ratio: float = 6.0, withdraw_stalls: int = 12, seed: int = 0,
                 fire_mode: str = "snap", stance: str = "run", behaviour: str = "normal",
                 move_mode: str = "individual", reserve: str = "snap",
                 focus_fire: bool = True, priority_targets: bool = True):
        """Every argument is a doctrine knob a search can turn.

        The adaptive rules stay adaptive: fire mode is still chosen by range and behaviour still
        by the head-count ratio. What the genome sets is the MIDDLE of each -- what to do at mid
        range, and in an even fight -- because those are the cases doctrine actually disputes.
        Nobody argues about what to do when you are outnumbered three to one.

        fire_mode and stance used to be hardcoded here as "snap"/"run" in the super() call, so a
        VeteranAI could not be told to fight any other way; the two genes the driver did apply
        were exactly the two this class threw away.
        """
        super().__init__(fire_mode=fire_mode, stance=stance,
                         withdraw_ratio=withdraw_ratio, withdraw_stalls=withdraw_stalls,
                         seed=seed)
        self.pull_back_at = pull_back_at
        self.min_spacing = min_spacing
        self.close_range = close_range
        self.long_range = long_range
        self.behaviour = behaviour
        self.move_mode = move_mode
        self.reserve = reserve
        self.focus_fire = focus_fire
        self.priority_targets = priority_targets

    def threat_rank(self, foe: Unit, centre: tuple[float, float]) -> tuple[int, int]:
        """Lower sorts first. Priority kinds beat distance; distance breaks ties within a class."""
        d2 = int((foe.x - centre[0]) ** 2 + (foe.y - centre[1]) ** 2)
        if not self.priority_targets:
            # Pure "shoot the nearest thing". Kept as a real option so the search can measure
            # whether threat priority is worth what it costs in walking distance, rather than
            # having the answer asserted here.
            return (0, d2)
        kind = (foe.kind or "").lower()
        cls = len(self.PRIORITY_KINDS)
        for i, k in enumerate(self.PRIORITY_KINDS):
            if k in kind:
                cls = i
                break
        return (cls, d2)

    def decide(self, obs: Observation) -> list[Action]:
        live_mine = [u for u in obs.mine if u.alive]
        live_foes = [u for u in obs.foes if u.alive]
        if obs.hunting:
            return self.hunt(obs)
        if not live_foes:
            return [Action("wait", None, "no hostiles left; the engine ends the mission")]

        acts: list[Action] = []

        # 1. Be where the fight is (unchanged -- the engine knows better than a headcount).
        if obs.last_event_z is not None and obs.last_event_z != obs.view_z:
            acts.append(Action("zoom_event", None, "engine's own last-event location"))
        elif live_foes:
            best = max({u.z for u in live_foes},
                       key=lambda z: (obs.foes_on(z), -z))
            if best != obs.view_z:
                acts.append(Action("show_floor", best, f"{obs.foes_on(best)} hostiles on {best}"))

        # 2. Withdraw only when genuinely lost -- both conditions, never one.
        if live_mine and obs.foes_alive / max(1, len(live_mine)) >= self.withdraw_ratio \
                and obs.stalls >= self.withdraw_stalls:
            acts.append(Action("withdraw", None,
                               f"outnumbered {obs.foes_alive}:{len(live_mine)} and stalled "
                               f"{obs.stalls}"))
            return acts

        if not live_mine:
            return acts or [Action("wait", None, "nobody left to give orders to")]

        # 3. Triage: the badly hurt leave before they become casualties.
        fit = []
        for u in live_mine:
            if u.hurt_frac >= self.pull_back_at:
                acts.append(Action("pull_back", u.uid,
                                   f"at {u.health}/{u.max_health}, past the {self.pull_back_at:.0%} "
                                   f"triage line"))
            else:
                fit.append(u)
        if not fit:
            # Everyone is hurt: they still fight, but the triage intent is recorded above.
            fit = live_mine

        cx = sum(u.x for u in fit) / len(fit)
        cy = sum(u.y for u in fit) / len(fit)

        # 4. Spacing: bunching is what turns one explosive into six casualties.
        for i, u in enumerate(fit):
            for v in fit[i + 1:]:
                if u.dist2(v) < self.min_spacing ** 2:
                    acts.append(Action("spread", u.uid,
                                       f"within {self.min_spacing} tiles of unit {v.uid}"))
                    break

        # 5. Threat priority + focus fire: one target, chosen by danger then distance.
        target = min(live_foes, key=lambda f: self.threat_rank(f, (cx, cy)))
        kind = (target.kind or "unknown").lower()
        why = "priority target" if any(k in kind for k in self.PRIORITY_KINDS) else "nearest"
        d2 = int((target.x - cx) ** 2 + (target.y - cy) ** 2)

        # 6. Fire mode by range rather than one setting for the whole battle.
        if d2 <= self.close_range ** 2:
            acts.append(Action("set_fire_mode", "auto", "close quarters"))
        elif d2 >= self.long_range ** 2:
            acts.append(Action("set_fire_mode", "aimed", "long range"))
        else:
            acts.append(Action("set_fire_mode", self.fire_mode, "mid range"))

        # 7. Behaviour mode. The engine has Aggressive/Normal/Evasive and the driver never set
        #    any of them -- it fought every battle on whatever the default was. Evasive when
        #    badly outnumbered so units break contact rather than trade; aggressive when we have
        #    the numbers and want the fight over before reinforcements arrive.
        ratio = obs.foes_alive / max(1, len(fit))
        if ratio >= 2.0:
            acts.append(Action("set_behaviour", "evasive", f"outnumbered {obs.foes_alive}:{len(fit)}"))
        elif ratio <= 0.5:
            acts.append(Action("set_behaviour", "aggressive", "we have the numbers"))
        else:
            acts.append(Action("set_behaviour", self.behaviour, "even fight"))

        # 8. Move individually rather than as a blob. BUTTON_MOVE_GROUP herds the squad into one
        #    clump, which is what makes a single explosive catastrophic; individual movement is
        #    the mechanical half of the spacing rule above.
        acts.append(Action("set_move_mode", self.move_mode, "do not bunch"))

        # 9. Reserve TU for the shot rather than spending it all walking. Reserving snap keeps a
        #    unit able to answer when something steps into view mid-move.
        acts.append(Action("set_reserve", self.reserve, "keep enough TU to shoot back"))

        # 10. Flyers: hold the hostiles' level. A flying unit left on the ground layer cannot
        #     engage something a floor up, and this squad's own floor is not necessarily theirs.
        if target.z != obs.view_z:
            acts.append(Action("set_layer", target.z, f"engage on level {target.z}"))

        if self.focus_fire:
            acts.append(Action("select_squad", len(fit), "focus the whole line on one target"))
            acts.append(Action("focus_fire", (target.x, target.y, target.z), f"{why}: {kind}"))
        else:
            # Engage without concentrating. Units default to FirePermissionMode::AtWill, so an
            # ordinary attack order lets each unit pick its own target rather than the squad
            # stacking on one -- the alternative doctrine, and one worth being able to lose with.
            acts.append(Action("attack", (target.x, target.y, target.z), f"{why}: {kind}"))
        return acts


class AggressiveAI(ScriptedAI):
    """Never withdraws, always runs, fires on auto. A deliberate extreme for comparison."""

    name = "aggressive"

    def __init__(self, seed: int = 0):
        super().__init__(fire_mode="auto", stance="run",
                         withdraw_ratio=float("inf"), withdraw_stalls=10 ** 9, seed=seed)


class CautiousAI(ScriptedAI):
    """Kneels, aims, and leaves early. The other extreme."""

    name = "cautious"

    def __init__(self, seed: int = 0):
        super().__init__(fire_mode="aimed", stance="kneel",
                         withdraw_ratio=2.0, withdraw_stalls=4, seed=seed)


REGISTRY = {
    "scripted": ScriptedAI,
    "veteran": VeteranAI,
    "aggressive": AggressiveAI,
    "cautious": CautiousAI,
}


def make(name: str, **kw) -> TacticalAI:
    cls = REGISTRY.get(name)
    if cls is None:
        raise KeyError(f"unknown AI {name!r}; have {sorted(REGISTRY)}")
    return cls(**kw)
