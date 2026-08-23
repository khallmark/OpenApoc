# UFO2P.EXE

- Role: cityscape / strategy process. Research, manufacturing, organisations, vehicles, economy, UFO growth, facilities, UFOPaedia names.
- Format: Watcom LE + DOS4GW. Two generations:

| Generation | Size | CRC32 | Where |
|------------|------|-------|-------|
| non-4 | 1,702,206 | `0x4749ffc1` | ISO `XCOM3/UFOEXE/UFO2P.EXE` |
| `4` | 1,705,790 | `0xdbd3b41d` | depot `XCOMA/UFOEXE/UFO2P.EXE`, ISO `UFO2P4.EXE` |

- OpenApoc mapping: [game/state/city](../../../game/state/city), [game/state/rules/city](../../../game/state/rules/city), [game/ui/city](../../../game/ui/city), extractors [ufo2p.h](../../../tools/extractors/common/ufo2p.h).

## Tables

Extractor headers apply to **non-4** only. On this Steam `4` build, signature search found the same bytes at **non-4 + 0xE00** for research, facilities, vehicles, organisations, economy, agent types, UFOPaedia groups, and bullet sprites. `crew_ufo_downed` at the documented non-4 offset is all zeros; do not invent a `4` address from that needle.

See sibling `labels/ufo2p_rebase.csv` and [address-maps/README.md](../address-maps/README.md).

`research_data` at non-4 `0x13EE80` (4-build `0x13FC80`) is the same 32-byte blob in both generations. Ghidra’s file-offset API finds it on the bound-file overlay; a byte match then finds it inside the LE data object. That is the translator spike — not a new table dump.

## Prior art to reuse

[hexa.txt](../../../tools/extractors/docs/hexa.txt) already decodes research, manufacturing, craft, equipment, funding, and UFO mission tables on non-4. Do not re-dump those hex layouts here.

## Systems still thin in OpenApoc

Walk strings and xrefs on **non-4** for table layout; check **`4`** only if Pentium-only code is suspected.

- Organisation diplomacy / raids / vehicle park — extractors already pull relationship and park tables; runtime in [organisation.cpp](../../../game/state/shared/organisation.cpp) still has fund and incursion FIXMEs ([issue 996](https://github.com/OpenApoc/OpenApoc/issues/996), [issue 1053](https://github.com/OpenApoc/OpenApoc/issues/1053)). UI strings include `Diplomacy`, `WEEKLY FUNDING ASSESSMENT`, and Senate funding copy.
- Ground vehicles — strings include `Road type:`, `Road Vehicle Weapons`, `An illegal road vehicle has been detected.`, and `Traveling by people tube`. [vehiclemission.cpp](../../../game/state/city/vehiclemission.cpp) still stubs mission hooks ([issue 785](https://github.com/OpenApoc/OpenApoc/issues/785)).
- Agent city missions — strings include `Investigate Building` and `Send selected units to investigate incident`. [agentmission.cpp](../../../game/state/city/agentmission.cpp) implements GotoBuilding / InvestigateBuilding / Snooze / AwaitPickup / Teleport; default branches still log `TODO: Implement`. `AttackBuilding` / `Land` / `Infiltrate` in that header are comments, not extra agent types found as strings.
- Alien dimension portals, umbilical collapse, overspawn, UFO mushrooms — strings name `Dimension Gates`, `Switching to Alien Dimension`, `Overspawn`, and `Overspawn Autopsy`. No printable `umbilical` or `mushroom` in this binary. [city.cpp](../../../game/state/city/city.cpp) notes portals as unfinished ([issue 264](https://github.com/OpenApoc/OpenApoc/issues/264)).
- Tick rate — original city logic is 36 TPS; OpenApoc mixes 36 / 60 / 144 ([issue 997](https://github.com/OpenApoc/OpenApoc/issues/997)). No literal “36 ticks” string in this binary.
- Victory / defeat / alien takeover — city strings `X-COM IS DEFEATED`, `THE ALIENS ARE DEFEATED`, `ALIEN TAKEOVER`, plus the victory paragraph about closed Dimension Gates. Launcher/SMKP name `smk/lose1.smk` and `smk/wingame2.smk`.
- Dynamic / action music — `Action music`, `getnextmusic`, and `/MUSIC/GROUP_1` … `GROUP_4` RAW paths live in UFO2P, not SMKP ([issue 618](https://github.com/OpenApoc/OpenApoc/issues/618)).
