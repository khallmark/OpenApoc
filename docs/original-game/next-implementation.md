# Next implementation targets

Written from [openapoc-gap-matrix.md](openapoc-gap-matrix.md). Ghidra and decompiler output are allowed on this fork.

A row is not done until the lock test fails without the change. Remaining items have **no recovered constant or consumer** — do not invent one.

Last lock: UFO subversion (micronoid rain) force-takes the building owner (`Organisation::tryMicronoidRain`, `test_organisation` `micronoid_rain_takeover`). `UFO_mission_patterns` `0x155164` is 20×10 uint16 (3=Infiltration, 1=Attack, 2=Subversion, 5=Overspawn), including week 13 (`test_city_rules` `ufo_mission_preference_loaded`). Also `unmanned_ufo_loot` and `nearby_intact_buildings`. `starting_available_UFOPaedia_entries` `0x19196A` is a 282-byte 0/1 blob (165 ones) with no proven entry-index map. `vehicle_park_spawn_table` `0x188F18` is 39×uint32-looking vehicle indices plus a leftover byte — no field map. Extractor 4-build CRCs/slides are in `exe_slide.h` (`ufo2p.cpp` / `tacp.cpp`).

1. **Battle cover-tile / potshot ([issue 265](https://github.com/OpenApoc/OpenApoc/issues/265))** — no printable `cover` / `potshot`. `getTakeCoverMovement` stays null until a TACP cover metric exists.
   - Lock: `test_tactical_ai_retreat` (retreat + panic run only).
   - Pass: Cautious/Normal take cover from a recovered metric.
2. **Enzyme / fire / cloak ticks** — names only. `HAZARD_SPREAD_CHANCE` and enzyme/fire ratios stay locked as made-up.
   - Lock: `test_battle_hazard`.
   - Pass: replace made-up values after original observation.
3. **Vehicle attack-mode dodge 100/80/50/10** — `loftemps_index` is loftemps, not a percent. `Evade Fire` xrefs empty.
   - Lock: none until an engagement table exists.
   - Pass: do not map 119–151 onto percents.
4. **City Action music ([issue 618](https://github.com/OpenApoc/OpenApoc/issues/618))** — `Action music` / `GROUP_*` catalog-only.
   - Lock: none.
   - Pass: city mix only after a bound consumer.
5. **Org bribe/rift dollar formulas ([issue 996](https://github.com/OpenApoc/OpenApoc/issues/996))** — directed relationship table is extracted; coefficients unbound.
   - Lock: `test_diplomacy`, `test_organisation`.
   - Pass: do not invent a weekly-drift formula.
6. **Dead gadgets** — Mind Shield +30/cap 200 locked. MultiTracker / VortexAnalyzer / Disruptor `useItem` unbound.
   - Lock: `test_battle_use_item`.
   - Pass: no invented scan/UI formula.
7. **UFO_mission_patterns** — extracted into `base_gamestate`. Do not XInclude a patch copy (serialize appends `missionList`).
   - Lock: `test_city_rules` (`ufo_mission_preference_loaded`).

Extractor CRC: [exe_slide.h](../../tools/extractors/common/exe_slide.h) maps UFO2P `0xdbd3b41d` → `+0xE00` (except `crew_ufo_downed`, P↔P4 same file offset) and TACP `0x3ec9c268` → `−0x2200`. Unknown CRCs log and keep slide 0. Lock: `test_research` `exe_slide_crcs`.

Do not put extracted list tables under `data/common_patch/gamestate/` — `loadGame` appends vectors and doubles fleet/spawn counts. Reference dumps live in [exe-tables](exe-tables/).
