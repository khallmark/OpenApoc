# Battle AI

[ai.txt](../../../tools/extractors/docs/ai.txt) already specifies Panic Run, Panic Freeze, Berserk, Default, Aggressive, Normal, Cautious, and Vanilla. OpenApoc implements a vanilla-style priority (`CTH * DAMAGE / TIME`) in [unitaivanilla.cpp](../../../game/state/battle/ai/unitaivanilla.cpp).

[issue 265](https://github.com/OpenApoc/OpenApoc/issues/265) still wants:

- Evasive: seek safety and cover
- Normal: cover and cautious engagement, potshots
- Aggressive: ignore risk
- Wounded movement/shooting penalty and medkit use in cover
- Hostile patterns: crawl under fire, smoke, retreat, regroup

TACP printable confirmation: `Cautious mode`, `Aggressive mode`, `Kneel down`, `Reserve TUs for kneel`, `Unit critically wounded`, `Unit under fire`, `Unit has gone berserk`, `Medi-kit`. No printable `cover`, `potshot`, or `evasive` — those behaviors stay prior-art from [ai.txt](../../../tools/extractors/docs/ai.txt), not string-backed.

Those rows are observational / prior-art, not decompiler reconstructions. Ghidra work on TACP should only confirm unknowns after this list.
