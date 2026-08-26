# UFO2P.EXE

- Role: cityscape / strategy process. Research, manufacturing, organisations, vehicles, economy, UFO growth, facilities, UFOPaedia names.
- Format: Watcom LE + DOS4GW. Two generations:

| Generation | Size | CRC32 | Where |
|------------|------|-------|-------|
| non-4 | 1,702,206 | `0x4749ffc1` | ISO `XCOM3/UFOEXE/UFO2P.EXE` |
| `4` | 1,705,790 | `0xdbd3b41d` | depot `XCOMA/UFOEXE/UFO2P.EXE`, ISO `UFO2P4.EXE` |

- OpenApoc mapping: [game/state/city](../../../game/state/city), [game/state/rules/city](../../../game/state/rules/city), [game/ui/city](../../../game/ui/city), extractors [ufo2p.h](../../../tools/extractors/common/ufo2p.h).

## Tables

Extractor headers apply to **non-4** only. On this Steam `4` build, signature search found the same bytes at **non-4 + 0xE00** for research, facilities, vehicles, organisations, economy, agent types, UFOPaedia groups, and bullet sprites. `crew_ufo_downed` at non-4 `0x13E560` (4-build `0x13F360`) is 560 B and identical on both builds.

See sibling `labels/ufo2p_rebase.csv` and [address-maps/README.md](../address-maps/README.md).

`research_data` at non-4 `0x13EE80` (4-build `0x13FC80`) is the same 32-byte blob in both generations. Ghidra’s file-offset API finds it on the bound-file overlay; a byte match then finds it inside the LE data object. That is the translator spike — not a new table dump.

## Prior art to reuse

[hexa.txt](../../../tools/extractors/docs/hexa.txt) already decodes research, manufacturing, craft, equipment, funding, and UFO mission tables on non-4. Do not re-dump those hex layouts here.

## Systems still thin in OpenApoc

Walk strings and xrefs on **non-4** for table layout; check **`4`** only if Pentium-only code is suspected.

- Organisation diplomacy / raids / surplus park sell — seller credit, park refill funds, and sell-above-cap are in. Remaining: two-way diplomacy and incursion FIXMEs ([issue 996](https://github.com/OpenApoc/OpenApoc/issues/996), [issue 1053](https://github.com/OpenApoc/OpenApoc/issues/1053)).
- Ground vehicles — strings include `Road type:`, `Road Vehicle Weapons`, `An illegal road vehicle has been detected.`, and `Traveling by people tube`. Mission hooks have defensive defaults; remaining work is large-vehicle footprint and engagement ([issue 785](https://github.com/OpenApoc/OpenApoc/issues/785)).
- Agent city missions — all six types are handled; unreachable fallback picks the nearest building. `AttackBuilding` / `Land` / `Infiltrate` in that header are vehicle-mission comments, not extra agent types.
- Alien dimension — dest-gate pairing is in (`leaveDimensionGate`); unset index still random. Extra UI `0x1532F5` / `0x153488` has empty bound xrefs. Umbilical / mushrooms stay prior-art (no printable names). Overspawn primaries and `attackList` both infiltrate ([issue 264](https://github.com/OpenApoc/OpenApoc/issues/264)).
- Tick rate — original city logic is observational 36 TPS. `FUN_0006d384` @ VA `0x6D384` / file `0xCFA28` uses coefficients `0x24`, `0x870`, and `0x2F7600`; their 1:60:86400 ratio corresponds to one second, one minute, and one day at 36 TPS and therefore corroborates that interpretation. It does not uniquely bind the absolute cadence (18 TPS yields exact two-second, two-minute, and two-day units). OpenApoc `TICKS_PER_SECOND = 36 × 4` is intentional; issue 997 is per-mechanic leftover 4×. No literal “36 ticks” string or absolute clock binding has been recovered.
- Victory / defeat SMKs play (`wingame2` / `lose1` → main menu). Alien takeover is a toast only.
- Dynamic / action music — `Action music` is catalog-only on the bound import; the Action playlist is battle-only ([issue 618](https://github.com/OpenApoc/OpenApoc/issues/618)).
