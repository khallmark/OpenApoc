# Next implementation targets

Written from [openapoc-gap-matrix.md](openapoc-gap-matrix.md). Ghidra and decompiler output are allowed on this fork.

A row is not done until the lock test fails without the change. Remaining items have **no recovered constant or consumer** — do not invent one.

Last lock: `test_city_rules` `advanced_quantum_lab_any` — patch `Any` of the three alien systems replaces extractor `All` via `op="delete"`. `alien_building4_keeps_table_prereq` keeps `research_data[92]` tech 1 (The Alien Dimension) plus the visit unlock. `ResearchData.unknown2==1` is only three topics (Genetic Structure, Advanced Security Station, Advanced Quantum Lab); do not map it to Any without a bound consumer. `starting_available_UFOPaedia_entries` `0x19196A` is a 282-byte 0/1 blob (165 ones) with no proven entry-index map. `vehicle_park_spawn_table` `0x188F18` is 39×uint32-looking vehicle indices plus a leftover byte — no field map.

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
7. **UFO_mission_patterns `0x155164`** — 399 bytes of u16s; mapping to Infiltration/Attack/Subversion/Overspawn unproven. Keep `ufo_mission_preference.xml`. `UFO_mission_data` / `UFO_growth_rates` are extracted and must not be XIncluded (serialize appends vectors).
   - Lock: `test_city_rules` (`ufo_mission_preference_loaded`, `ufo_incursion_table`, `ufo_growth_rates_match_exe`).
   - Pass: do not invent ID→mission mapping.

Extractor CRC follow-on: teach [ufo2p.cpp](../../tools/extractors/common/ufo2p.cpp) / [tacp.cpp](../../tools/extractors/common/tacp.cpp) the `4`-build CRCs and the per-table deltas in the sibling rebase CSVs, or keep extracting from the ISO non-4 pair.

Do not put extracted list tables under `data/common_patch/gamestate/` — `loadGame` appends vectors and doubles fleet/spawn counts. Reference dumps live in [exe-tables](exe-tables/).
