# Next implementation targets

Written from [openapoc-gap-matrix.md](openapoc-gap-matrix.md). Ghidra and decompiler output are allowed on this fork.

A row is not done until the lock test fails without the change. Remaining items have **no recovered constant or consumer** — do not invent one.

Last lock: `craft_ammo_manufacturers` at `0x13EB6A` (Zorium = X-COM) and `prereqType` 0/1 item gates. UFO mission patterns extracted; do not keep `ufo_mission_preference.xml` under `common_patch/gamestate/` (serialize appends `missionList`). `starting_available_UFOPaedia_entries` `0x19196A` and `vehicle_park_spawn_table` `0x188F18` stay unbound. `unknown2` is not Any/All.

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
7. **UFO_mission_patterns** — extracted into `base_gamestate`. Do not XInclude a patch copy (serialize appends `missionList`). Bound xrefs to VA `0x128D64` are empty; IDs 3/1/2/5 are campaign first-appearance (weeks 1/4/5/7), not a recovered `FUN_*`.
   - Lock: `test_city_rules` (`ufo_mission_preference_loaded`).
8. **Subversion arrival** — role 8 is a real incursion slot. `tryMicronoidRain` → `takeOver` is OpenApoc, not EXE. `FUN_0007fcc0` @ `0x7FCC0` is the weekly infiltration clamp (taken-over → 200), not rain-on-arrival. Do not invent a new percent.
   - Lock: `test_organisation` (`micronoid_rain_takeover`) pins current stand-in only.
   - Pass: Ghidra bind of role 8 at hover-complete.

Extractor CRC: [exe_slide.h](../../tools/extractors/common/exe_slide.h) maps UFO2P `0xdbd3b41d` → `+0xE00` (except `crew_ufo_downed`, P↔P4 same file offset) and TACP `0x3ec9c268` → `−0x2200`. Unknown CRCs log and keep slide 0. Lock: `test_research` `exe_slide_crcs`.

Do not put extracted list tables under `data/common_patch/gamestate/` — `loadGame` appends vectors and doubles fleet/spawn counts. Reference dumps live in [exe-tables](exe-tables/).
