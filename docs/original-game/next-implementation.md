# Next implementation targets

Written from [openapoc-gap-matrix.md](openapoc-gap-matrix.md). Ghidra and decompiler output are allowed on this fork.

A row is not done until the lock test fails without the change. Remaining items have **no recovered constant or consumer** — do not invent one.

Last lock: `FUN_000abf9c` @ VA `0xABF9C` / file `0xFE640` (caller `FUN_000aac88` @ file `0xFD32C`) sets the UFOPaedia start byte from `research_data` +25/+26; catalog-bound pages ignore shuffled patch deps (`test_city_rules` `ufopaedia_abf9c_unlock`). Extra cat 3 `0x21→0x22` / `0x1B→0x13+0x17`. Type-mismatched PCX stays unbound. Manufacture records 38–42 keep `()` in the topic ID (`MANUFACTURE_DISRUPTOR_ARMOR_(LEGS)`); patch deletes leftover `__LEGS_` keys (`manufacture_disruptor_armor_ids`). `FUN_0008c860` @ file `0xEEF04` (4-build `0xEEFD4`) type 2/3 hide `DAT_00183b3b` / `DAT_00183b0a`. `FUN_000811fc` @ file `0xE3971` (4-build `0xE39BC`) is the same aequip hide for the shop — week 0 is not a hide bit (AG 19/23, destab 37). `FUN_00073750` @ file `0xD5DF4` and `FUN_0007679c` @ file `0xD8E40` are vequip screen/info-card hide graphics, not a shop list. `FUN_00081610` @ file `0xE3CB4` is an aequip layout pass, not craft-ammo shop. `FUN_00086054` @ file `0xE86F8`, `FUN_0008cb9c` @ file `0xEF240`, and `FUN_0008f7f4` @ file `0xF1E98` are org-base buy/stock/layout UI on the same `0x57` / `DAT_00183b3b` (and type-3 `DAT_00183b0a`) hide RAM — not new shop filters. Do not copy aequip week-0 shop onto vammo / vehicle types. unknown2==4 is 29 rows (`XOR EBX` then `de2b0[topic]` override); leftover Containment `prereqTech[0]=45` is not a combinator input. Overlay `INC` is `FUN_0007b3dc` @ TACP file `0xD5E80` on the tile overlay byte at `0xF3738` — do not invent placement seed 10, map `age`/`power`, or INC on an invented cadence. `FUN_00081774` @ file `0xE3E18` IDIVs live `[+0xF8]` by `ammo_rounds` — numerator unbound. `FUN_000b32ac` @ file `0x115950` is a 4-way event dispatcher, not a cargo-seize entry; do not wire `Cargo::seize` from `worth×50`. TACP `unused01` @ `+0xF` feeds a type-4 doodad (`FUN_000598d4` @ `0xB4378` / `FUN_00058e84` @ `0xB3928`) — visual slot, no damage reader. `+0x168` / `0x1439E0` rebuild / `rebuilding_rate` stay unbound. `leadsTo` is not a research-graph edge. Do not delete Containment’s All(Quantum Lab). Weekly item stock/price stays Wong’s Guide — `FUN_000941dc` @ file `0xF6880` is org budget fractions, not `EconomyInfo::update`. Panic→Stun fall-through in `battleunit.cpp` stays; no TACP listing.

1. **Battle cover-tile / potshot ([issue 265](https://github.com/OpenApoc/OpenApoc/issues/265))** — no printable `cover` / `potshot`. `getTakeCoverMovement` stays null until a TACP cover metric exists.
   - Lock: `test_tactical_ai_retreat` (retreat + panic run only).
   - Pass: Cautious/Normal take cover from a recovered metric.
2. **Enzyme / fire / cloak ticks** — names only. Item fire resist is `unknown01` via `FUN_0007c110` (locked). Overlay `INC` is `FUN_0007b3dc`; placement RNG `FUN_0001eee8` unbound — do not invent seed 10 or map `age`/`power`. `HAZARD_SPREAD_CHANCE` stays made-up.
   - Lock: `test_battle_hazard` (`fire_hazard_item_resist`), `test_city_rules` (`aequip_artifact_and_resist`).
   - Pass: replace made-up spread/enzyme values after original observation.
3. **Vehicle attack-mode dodge 100/80/50/10** — `loftemps_index` is loftemps, not a percent. `Evade Fire` xrefs empty.
   - Lock: none until an engagement table exists.
   - Pass: do not map 119–151 onto percents.
4. **City Action music ([issue 618](https://github.com/OpenApoc/OpenApoc/issues/618))** — `Action music` / `GROUP_*` catalog-only.
   - Lock: none.
   - Pass: city mix only after a bound consumer.
5. **Org bribe/rift dollar formulas ([issue 996](https://github.com/OpenApoc/OpenApoc/issues/996))** — directed relationship table is extracted; coefficients unbound. `Cargo::seize` relation FIXME is the same class: `FUN_000b32ac` @ file `0x115950` recovers `worth×50` + tiered extra into org `+8` / `FUN_0005faf0`, but `[0x174024]` event type is not bound to seizure.
   - Lock: `test_diplomacy`, `test_organisation`.
   - Pass: do not invent a weekly-drift or seize-diplomacy formula.
6. **Dead gadgets** — Mind Shield +30/cap 200 locked. MultiTracker / VortexAnalyzer / Disruptor `useItem` unbound.
   - Lock: `test_battle_use_item`.
   - Pass: no invented scan/UI formula.
7. **UFO_mission_patterns** — extracted into `base_gamestate`. Do not XInclude a patch copy (serialize appends `missionList`). Bound xrefs to VA `0x128D64` are empty; IDs 3/1/2/5 are campaign first-appearance (weeks 1/4/5/7), not a recovered `FUN_*`.
   - Lock: `test_city_rules` (`ufo_mission_preference_loaded`).
8. **Subversion arrival** — `tryMicronoidRain` → `takeOver` is OpenApoc, not EXE. `FUN_0007fcc0` @ VA `0x7FCC0` / file `0xD2364` is the hourly infiltration clamp, not rain-on-arrival. Prior `CMP AX/SI/DI,8` sites (`FUN_000941dc` @ file `0xF6EC1`, `FUN_0006bc70` @ `0xCE3FE` / `0xCE409`) are loop bounds of 8, not role 8. Hover-complete for pattern type 2 is `vehicle+0x15C==2` (`FUN_0003a910` @ file `0x9D66C`): doodad `0xB`, then `FUN_0007062c` @ `0xD2CD0` sets building `+0x2BC=1`. That flag has other unlisted writers. No arrival percent / `takeOver`. Do not invent one.
   - Lock: `test_organisation` (`micronoid_rain_takeover`) pins the stand-in; `taken_over_infiltration_clamp` / `infiltration_hourly_ufo2p_rules` / `infiltration_divisor_uses_difficulty` lock `FUN_0007fcc0`.
   - Pass: bind building `+0x2BC` readers, not a new percent.
9. **`UFO_mission_data` tail leftovers** — `follow_slot`, spawn XY, `type_percent` constitution multiply, and the scatter>50 clamp are consumed. `building_function` is a byte index copied to vehicle `+0x171` (`FUN_0006da88` @ file `0xD030B`) and consumed by `FUN_0006d384` @ file `0xCFD53` against org tables VA `0x1439E0` (stride `0x2D4`). That table rebuild + `FUN_0005e7f4` tile probe stay unbound. `FUN_000588f8` `+0x168` gate unbound. Do not invent an `acquireTargetBuilding` name/`function` pick; `+0x171` is later a retry counter (`FUN_0003a910`).
   - Lock: `test_city_rules` (`ufo_incursion_table`, `ufo_incursion_spawn_xy`, `ufo_incursion_follow_type`).
   - Pass: bind the `0x1439E0` rebuild + `FUN_0005e7f4`, then `FUN_000588f8` / `FUN_0004dd14`.

Extractor CRC: [exe_slide.h](../../tools/extractors/common/exe_slide.h) maps UFO2P `0xdbd3b41d` → `+0xE00` (except `crew_ufo_downed`, P↔P4 same file offset) and TACP `0x3ec9c268` → `−0x2200`. Unknown CRCs log and keep slide 0. Lock: `test_research` `exe_slide_crcs`.

Do not put extracted list tables under `data/common_patch/gamestate/` — `loadGame` appends vectors and doubles fleet/spawn counts. Reference dumps live in [exe-tables](exe-tables/).
