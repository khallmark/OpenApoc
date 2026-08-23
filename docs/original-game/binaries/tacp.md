# TACP.EXE

- Role: battlescape / tactical process. Damage types, agent equipment, armor, weapons, payloads, equipment sets, battle map consumers.
- Format: Watcom LE + DOS4GW. Two generations:

| Generation | Size | CRC32 | Where |
|------------|------|-------|-------|
| non-4 | 3,170,298 | `0xfebbe39e` | ISO `XCOM3/TACEXE/TACP.EXE` |
| `4` | 3,161,594 | `0x3ec9c268` | depot `XCOMA/TACEXE/TACP.EXE`, ISO `TACP4.EXE` |

- OpenApoc mapping: [game/state/battle](../../../game/state/battle), [game/state/battle/ai](../../../game/state/battle/ai), [game/ui/battle](../../../game/ui/battle), extractors [tacp.h](../../../tools/extractors/common/tacp.h) and [aequipment.h](../../../tools/extractors/common/aequipment.h).

## Tables

Extractor headers apply to **non-4** only. On this Steam `4` build, the same table bytes sit at **non-4 − 0x2200** (damage types, names, equipment, armor, weapons, payloads, builtin sets, bullet sprites). That is a different slide than UFO2P’s `+0xE00`. Do not reuse the cityscape delta.

See sibling `labels/tacp_rebase.csv`.

`damage_type_data` at non-4 `0x2020A0` (4-build `0x1FFEA0`) is the same blob. File-offset lookup hits the bound overlay; a byte match finds it in TACP’s LE data object (next selector after the code object).

## Prior art to reuse

[ai.txt](../../../tools/extractors/docs/ai.txt), [tactical.txt](../../../tools/extractors/docs/tactical.txt), [psionics.txt](../../../tools/extractors/docs/psionics.txt), [algo.txt](../../../tools/extractors/docs/algo.txt), [strafe.txt](../../../tools/extractors/docs/strafe.txt), [tb.txt](../../../tools/extractors/docs/tb.txt). Runtime vanilla-style AI lives in [unitaivanilla.cpp](../../../game/state/battle/ai/unitaivanilla.cpp) and [tacticalaivanilla.cpp](../../../game/state/battle/ai/tacticalaivanilla.cpp).

## Systems still thin in OpenApoc

- Cautious / Normal cover and potshots — TACP strings include `Cautious mode`, `Aggressive mode`, `Kneel down`, `Reserve TUs for kneel`, `Unit critically wounded`, and `Medi-kit`. No printable `cover`, `potshot`, or `evasive`. [ai.txt](../../../tools/extractors/docs/ai.txt) still specifies the intended cover/potshot modes; [issue 265](https://github.com/OpenApoc/OpenApoc/issues/265) is open.
- Enzyme, fire, cloak — strings `Entropy Enzyme`, `Personal Cloaking Field`, `Gas`, `Fire`, `Stun Gas`. [battlehazard.cpp](../../../game/state/battle/battlehazard.cpp) and [version01readme.txt](../../../tools/extractors/docs/version01readme.txt) call the current values approximations.
- Multi-tile unit pathing and drawing — FIXMEs on [battleunit.h](../../../game/state/battle/battleunit.h) and [battlescanner.cpp](../../../game/state/battle/battlescanner.cpp).
- Tick / TU timing — same [issue 997](https://github.com/OpenApoc/OpenApoc/issues/997) contamination as cityscape; fire rates and explosion radii are listed as wrong versus original. Strings mention `Fire rate` and TU reservation for kneeling / aimed / snap / auto.
