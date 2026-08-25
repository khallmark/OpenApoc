# Writing an X-COM AI

The harness is split into three layers so the interesting one can be replaced without touching
the other two.

| file | responsibility | may import the engine? |
|---|---|---|
| `tools/oa_capabilities.py` | **what the harness can do** — press this control, read that state | yes |
| `tools/oa_ai.py` | **what we choose to do** — decisions only | **no** |
| `tools/oa_executor.py` | the bridge: observe → ask the AI → dispatch | yes |

The test for which layer code belongs in: *could a completely different AI, with opposite
doctrine, still want this exact method?* If no, it is a decision and belongs in an AI.

## Drop-in plugin

Put a file in `tools/ai_plugins/`. Anything subclassing `TacticalAI` with a `name` is discovered
automatically — no registration, and the filename does not matter.

```python
# tools/ai_plugins/my_doctrine.py
from oa_ai import Action, TacticalAI

class MyDoctrine(TacticalAI):
    name = "my_doctrine"

    def opening(self, obs):          # once, when the battle starts
        return [Action("set_fire_mode", "snap", "")]

    def decide(self, obs):           # every round
        if obs.foes_alive > obs.mine_alive:
            return [Action("set_behaviour", "evasive", "outnumbered")]
        f = obs.foes[0]
        return [Action("focus_fire", (f.x, f.y, f.z), "engage")]
```

Run it: `python3 oa_arena.py --ai my_doctrine`

## What an AI may know

Only what a human player has on screen. `Observation` carries friendly and hostile units with
tile positions, kinds, health and TU; the current view level; the mission type; and a stall
counter. There is no handle back to the engine, so an AI **cannot** read a field the UI does not
show. That constraint is what makes run results mean anything.

## What an AI may ask for

`Action(kind, arg, why)`. The `why` is not decoration — it is what the run log prints, and it is
how a bad decision gets diagnosed after the fact.

```
set_fire_mode   aimed|snap|auto        set_behaviour  aggressive|normal|evasive
set_stance      run|walk|kneel|prone   set_move_mode  group|individual
set_reserve     aimed|snap|auto|kneel  set_layer      int (flyer altitude)
cease_fire      bool                   zoom_event     -
show_floor      int                    select_squad   int
attack          (x,y,z)                focus_fire     (x,y,z)
move            (x,y,z)                withdraw       -
wait            -
```

`pull_back` and `spread` are accepted by the AI layer but **not yet executable** — per-unit
repositioning needs a screen coordinate for one specific unit, and `battle_positions` reports
tile space. The executor reports them as unsupported rather than clicking somewhere wrong.

## Built-in AIs

| name | doctrine |
|---|---|
| `scripted` | the original behaviour, made explicit: nearest target, whole squad |
| `veteran` | triage, threat priority, focus fire, spacing, range-appropriate fire |
| `aggressive` | never withdraws, always runs, auto fire |
| `cautious` | kneels, aims, leaves early |
| `skirmisher` | example plugin: holds range, breaks contact early |

## Testing without a game

`tools/test_oa_ai.py` runs 30 doctrine assertions in milliseconds with nothing booted. Because
`oa_ai.py` imports no sockets and no engine, a plugin is testable the same way — which is the
reason the split exists. Every rule in `veteran` was written test-first this way.
