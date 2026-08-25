#!/usr/bin/env python3
"""A worked example of a drop-in X-COM AI. Copy this file and change the rules.

Run it with:  python3 oa_arena.py --ai skirmisher   (or oa_play.py --ai skirmisher)

Everything an AI is allowed to know arrives in `obs`; everything it is allowed to ask for is an
`Action`. It cannot touch the game, so it cannot cheat, and it can be unit-tested with no game
running -- which is the entire point of keeping this file free of engine imports.
"""

from oa_ai import Action, Observation, TacticalAI


class SkirmisherAI(TacticalAI):
    """Never closes. Holds range, aims, and breaks contact the moment the odds turn.

    Deliberately a different doctrine from VeteranAI so the two can be raced against the same
    alien force in the arena: VeteranAI concentrates and closes, this one trades space for time.
    Which is better is an empirical question, and that is what the arena's ledger is for.
    """

    name = "skirmisher"

    def __init__(self, break_at: float = 1.5, seed: int = 0):
        self.break_at = break_at

    def opening(self, obs: Observation) -> list[Action]:
        return [
            Action("set_fire_mode", "aimed", "hold range, make shots count"),
            Action("set_stance", "kneel", "kneeling steadies the shot"),
            Action("set_reserve", "aimed", "never be caught with no TU"),
            Action("set_move_mode", "individual", "never bunch"),
        ]

    def decide(self, obs: Observation) -> list[Action]:
        if obs.foes_alive == 0:
            return [Action("wait", None, "nothing left to shoot")]

        acts = []
        # Fight on the floor the engine says is live.
        if obs.last_event_z is not None and obs.last_event_z != obs.view_z:
            acts.append(Action("zoom_event", None, "go where it is happening"))

        # Break contact on bad odds, well before VeteranAI would.
        if obs.mine_alive and obs.foes_alive / obs.mine_alive >= self.break_at:
            acts.append(Action("set_behaviour", "evasive", "odds turned"))
            acts.append(Action("cease_fire", True, "disengage rather than trade"))
            return acts

        acts.append(Action("set_behaviour", "normal", "holding"))
        target = min((f for f in obs.foes if f.alive),
                     key=lambda f: (f.x - obs.view_z) ** 2 + f.y ** 2, default=None)
        if target:
            acts.append(Action("select_squad", min(6, obs.mine_alive), "engage together"))
            acts.append(Action("focus_fire", (target.x, target.y, target.z), "aimed volley"))
        return acts or [Action("wait", None, "nothing to do")]
