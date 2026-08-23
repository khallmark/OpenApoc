# Next implementation targets

Written from [openapoc-gap-matrix.md](openapoc-gap-matrix.md). Ghidra and decompiler output are allowed on this fork.

Priority is milestone + evidence confidence + local file locality. A row is not done until the lock test fails without the change.

1. **Ticks per second ([issue 997](https://github.com/OpenApoc/OpenApoc/issues/997))** — 144 TPS is `36 × 4` on purpose. City Speed1 now matches vanilla half-rate. Next: fire rates / AOE / movement that still inherit a silent 4×.
   - Lock: `test_gametime` (`tick_constants`, `hardcoded_fuel_ticks_match_tps`), `test_battle_hazard`.
   - Pass: `VANILLA_TICKS_PER_SECOND == 36` stays; remaining subsystem rates no longer inherit a silent 4×; `FUEL_TICKS_PER_SECOND` tracks `TICKS_PER_SECOND`.
2. **Ground vehicle lanes ([issue 785](https://github.com/OpenApoc/OpenApoc/issues/785))** — `connection[dir]` is used; remaining work is large-vehicle occupancy and engagement tables.
   - Lock: `test_vehicle_mission`.
   - Pass: road occupancy and engagement match extracted road-layer rules.
3. **Organisation vehicle park sell-above-cap ([issue 1053](https://github.com/OpenApoc/OpenApoc/issues/1053))** — restock + seller credit are in. Selling surplus park vehicles is still open.
   - Lock: `test_city_rules` (`org_park_funds`, `purchase_deduct`).
   - Pass: weekly restock can sell surplus.
4. **Battle cover-tile search ([issue 265](https://github.com/OpenApoc/OpenApoc/issues/265))** — Normal/Evasive kneel/prone per [ai.txt](../../tools/extractors/docs/ai.txt). Solid cover + potshots still need a TACP cover metric.
   - Lock: `test_tactical_ai_retreat` (retreat table only today).
   - Pass: Cautious/Normal take cover and potshot; add a behavior test when the code exists.
5. **Enzyme / fire / cloak constants** — names are string-backed (`Entropy Enzyme`, `Personal Cloaking Field`). Replace “made up” values in [battlehazard.cpp](../../game/state/battle/battlehazard.cpp) after original observation.
   - Lock: `test_battle_hazard` (`HAZARD_SPREAD_CHANCE == 10`, enzyme/fire tick ratios).
   - Pass: constants match observed original; the made-up comment is gone.

Extractor CRC follow-on (not this experiment): teach [ufo2p.cpp](../../tools/extractors/common/ufo2p.cpp) / [tacp.cpp](../../tools/extractors/common/tacp.cpp) the `4`-build CRCs and the per-table deltas in the sibling rebase CSVs, or keep extracting from the ISO non-4 pair.
