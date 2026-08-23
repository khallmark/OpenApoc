# Battle AI

[ai.txt](../../../tools/extractors/docs/ai.txt) already specifies Panic Run, Panic Freeze, Berserk, Default, Aggressive, Normal, Cautious, and Vanilla. OpenApoc implements a vanilla-style priority (`CTH * DAMAGE / TIME`) in [unitaivanilla.cpp](../../../game/state/battle/ai/unitaivanilla.cpp). Tactical retreat chance is `neutralizedPercent - 50` ([tacticalaivanilla.cpp](../../../game/state/battle/ai/tacticalaivanilla.cpp)).

[issue 265](https://github.com/OpenApoc/OpenApoc/issues/265) still wants:

- Evasive: seek safety and cover (`getTakeCoverMovement` returns null; Evasive can prone, Normal kneels)
- Normal: cover and cautious engagement, potshots
- Aggressive: ignore risk — `UnitAIBehavior` currently returns immediately
- Panic Run: drop is implemented on morale entry (50%); flee still picks a random adjacent LOS block and runs 33% of the time
- Wounded movement/shooting penalty and medkit use in cover
- Group movement using equipped personal teleporters (`groupMove` default `useTeleporter=false`)
- Hostile patterns: crawl under fire, smoke, retreat, regroup

TACP printable confirmation: `Cautious mode`, `Aggressive mode`, `Kneel down`, `Reserve TUs for kneel`, `Unit critically wounded`, `Unit under fire` (`0x2E0438`), `Unit has gone berserk` (`0x2DF134`), `Medi-kit`. No printable `cover`, `potshot`, or `evasive` — those behaviors stay prior-art from [ai.txt](../../../tools/extractors/docs/ai.txt), not string-backed.

Ghidra on TACP is allowed; confirm these modes before inventing new ones. Bound TACP strings for Cautious / Aggressive / Kneel / Medi-kit / Entropy Enzyme / Personal Cloaking Field still have empty function xrefs. Flat `FUN_0009b780` @ `0x9B780` is a Mind Shield *effect* candidate (type `0x05`, add 30, cap 200) and is not in the bound TACP export.
