# Next implementation targets

Written from [openapoc-gap-matrix.md](openapoc-gap-matrix.md). Ghidra and decompiler output are allowed on this fork.

A row is not done until the lock test fails without the change. Remaining matrix gaps are **prior-art only** until a cited table or decompile exists. Do not invent constants.

## Locked this pass

1. **Manufacturing topics** — `manufacturing_data` 43×50 at UFO2P non-4 `0x13FD34`.
   - Lock: `test_city_rules` (`manufacture_dimension_probe`).
2. **UFOPaedia group strtab** — `0x152ADD`–`0x152B57`, 10 names ending `Alien Craft`.
   - Lock: extractor assert + `test_city_rules` (`ufopaedia_alien_craft_group`).
3. **UFO growth lists** — `UFO_growth_rates` `0x155010` already extracted.
   - Lock: `test_city_rules` (`ufo_growth_rates_match_exe`).
4. **Ground footprint** — occupancy uses extracted `size_x`/`size_y`.
   - Lock: `test_vehicle_mission` (`ground_footprint_tiles`).
5. **Diplomacy raid snapshot order** — `setRaidMissions` before `updateRelations`.
   - Lock: `test_organisation` (`raid_relation_pressure`).
6. **Stop UFO growth after Organic Factory dies** — UFOPaedia: mushrooms grow into Alien craft there.
   - Lock: `test_city_rules` (`organic_factory_gates_ufo_growth`).
7. **Player relation mirror** — org→player mirrors; player→org does not (raid asymmetry).
   - Lock: `test_diplomacy` (`player_relation_mirror`).

## Blocked (no EXE numbers)

1. Enzyme / fire / cloak ticks — names only.
2. Cover-tile / potshot / evasive metric — no TACP cover table.
3. Wounded TU/accuracy and medkit-in-cover — notification strings only.
4. Org bribe/rift dollar formulas; vehicle attack-mode 100/80/50/10; city `getnextmusic` mix; `UFO_mission_data` field map; Senate numeric funding thresholds (code at UFO2P `0xF6F00` not fully extracted).

Extractor CRC follow-on: teach [ufo2p.cpp](../../tools/extractors/common/ufo2p.cpp) / [tacp.cpp](../../tools/extractors/common/tacp.cpp) the `4`-build CRCs and the per-table deltas in the sibling rebase CSVs, or keep extracting from the ISO non-4 pair.
