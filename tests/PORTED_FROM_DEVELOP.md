# Porting the develop test suite onto master

Master carried 10 unit tests. The `develop` branch accumulated 26 more while the
decompilation work went on, and those tests are the only written record of how
several city, battle, and economy rules are meant to behave. This document
records what came across, what did not, and why.

The governing rule: **when a develop test asserted behaviour master does not
implement, the assertion was dropped — never the other way around.** The moment
develop source is backported to make a test compile or pass, this stops being a
regression baseline for master and becomes a feature port.

## Running the suite

```
ctest --test-dir build -LE known-master-bug
```

That is the regression gate: **25 tests, all green**, verified stable across five
consecutive runs. Plain `ctest` also runs three quarantined reproducers that fail
on purpose (see below).

Both CI runners were updated to match: `.github/workflows/cmake.yml` and
`appveyor.yml` / `appveyor-dev.yml` pass `-LE known-master-bug`, so a red build
still means a real regression. The GitHub workflow additionally runs
`-L known-master-bug` as a `continue-on-error` step, so the reproducers stay
visible without gating the build. Both runners fetch `cd_minimal.iso` and run the
extractor, so the data-backed tests work there unchanged.

### Data-backed tests need extracted game data

Eight tests load a real `GameState` and need the extractor's output present
under `data/`: `mods/base/base_gamestate`, `mods/base/modinfo.xml`,
`mods/base/data/submods/org.openapoc.base/difficulty*`, and the
`animationpacks/`, `bulletsprites/`, `imagepacks/`, `maps/`, `tilesets/`
directories. All are gitignored build products — a fresh clone has only
`keep_folder` stubs there, and these tests abort without them.

Build once with `-DEXTRACT_DATA=ON` and a valid `CD_PATH`, or copy the
directories from a checkout that has already extracted them. **Copy, do not
symlink**: the VFS is PhysFS, which does not follow symlinks, so symlinked
`imagepacks/` and `animationpacks/` load as empty and crash the battle tests.
(`base_gamestate` and `cd.iso` are read through plain OS paths, so those two
tolerate symlinks.)

## Test harness

`tests/test_helpers.h` carries the whole harness: `TEST_REQUIRE` (abort the
case), `TEST_CHECK` (record and continue), `runTestSuite()`, a deterministic
config helper, and `loadStartedGameState()` for the data-backed tests. Two lines
of develop's version were dropped — it cleared `GameState::vehicleParkSpawnTable`
and `fireHazardPowerTable`, neither of which exists on master.

## What came across

| Test | Cases | Assertions |
| --- | ---: | ---: |
| test_city_rules | 18 | 130 |
| test_gametime | 5 | 27 |
| test_organisation | 5 | 12 |
| test_diplomacy | 4 | 18 |
| test_economy | 4 | 16 |
| test_vec | 4 | 19 |
| test_psionics | 3 | 13 |
| test_research | 3 | 13 |
| test_tu_reservation | 3 | 25 |
| test_agent_mission | 2 | 9 |
| test_damage_predicates | 2 | 10 |
| test_enum_traits | 2 | 13 |
| test_line | 2 | 11 |
| test_vehicle_mission | 2 | 16 |
| test_battle_hazard | 1 | 7 |

Plus the three quarantined reproducers below, and master's original ten
(`test_rect`, `test_voxel`, `test_tilemap`, `test_rng`, `test_images`,
`test_unicode`, `test_colour`, `test_backtrace`, `test_serialize`,
`test_lab_assignment`), which were already green and are untouched.

## Quarantined: three defects in master today

These are **not** develop behaviour changes. Each reproduces a real defect in
master, verified against the fix that exists elsewhere. They are labelled
`known-master-bug` so the gate can exclude them, and they should start passing —
without edits — once the underlying bugs are fixed.

### `test_base_die` — use-after-free (SEGFAULT)

`Base::die()` range-iterates `building->currentAgents` and
`building->currentVehicles` while `Agent::die()` and `Vehicle::die()` erase from
those same lists, so the loop increments an iterator whose node has been freed.
`Vehicle::die()` already guards its own agent loop by copying first; `Base::die()`
does not. Fixed by commit `655ce595` on `origin/fix/base-die-iterator-invalidation`.

### `test_battle_large_unit` — large-unit geometry (3 of 4 cases)

- `large_unit_occupies_block`: while moving, a large unit's occupied set is 11
  tiles, not the union of its current and goal 2x2 blocks (12).
- `large_unit_no_ground_snag`: the eye location applies a `+0.5` offset it should
  not, leaving eyes at 5.5 when the block's true centre is 5 — and off from the
  unit's own muzzle, which is not offset.
- `large_unit_los_any_tile`: line-of-sight probes the single geometric centre of
  the 2x2 block, which sits exactly on a tile corner, instead of testing each
  occupied tile. A large target with a clear line to one of its tiles reads as
  invisible.

`large_unit_path_rejects_narrow_gap` passes on master and is the reason this file
is quarantined whole rather than split.

### `test_ground_vehicle_path` — pathfinder give-up (2 of 2 cases)

- `..._terminates_on_severed_road`: `gotoLocation` across a severed road never
  terminates — `GotoLocation` re-plans the same unreachable target forever.
- `..._order_does_not_crash`: the vehicle self-destructs at tick 21 merely
  because its destination was unreachable, not because of a collision. Master
  crashes the vehicle on give-up regardless of whether a usable partial path
  exists.

## What did not come across

### Three files never ported — develop-only subsystems

| File | Requires |
| --- | --- |
| `test_harness.cpp` | `framework/harness.h` (socket test harness) |
| `test_app_paths.cpp` | `framework/os/app_paths.h` |
| `test_display_size.cpp` | `framework/os/display_size.h` |

### Five files dropped whole — every case was develop-only

| File | Depends on |
| --- | --- |
| `test_battle_disruptor_shield.cpp` | `BattleUnit::disruptorShield*`, `DISRUPTOR_SHIELD_CAPACITY_BONUS` |
| `test_battle_use_item.cpp` | `BattleUnit::applyMindShieldIncrement` |
| `test_tactical_ai_retreat.cpp` | `TacticalAIVanilla::retreatChancePercent`, `UnitAILowMorale::isFartherFromEnemy` |
| `test_unit_ai_priority.cpp` | `UnitAIVanilla::attackPriority`, `::blastDamageContribution`, `::aoeIsWorthThrowing`, `UnitAIHelper::exposureScore` |
| `test_game_end.cpp` | `GameEventType::AliensDefeated` / `::XComDefeated`, plus develop-only takeover strings — both cases dropped, leaving the file empty |

### Cases dropped from surviving files

**Would not compile — develop-only API.** 51 cases, each naming the symbol master
lacks:

- `test_city_rules` (29): `Organisation::infiltrationDisplayPercent`,
  `Vehicle::destinationPortalIndex`, `VehicleMission::advanceMissionCounterOnArrival`
  (x4), `Vehicle::withdrawBandEntered`, `UFOGrowth::selectForWeek`,
  `ItemDependency::Type`, `UFO2P_AA7A8_BIOCHEM_ANY`,
  `ALIEN_DETECTION_WEIGHT_OFFSET_START`, `UFOGrowth::craftFactoryIntact`,
  `UFOIncursion::attackSlots`, `VehicleMission::computeIncursionSpawnXY`,
  `Base::knownToAliens`, `Organisation::militarizedFromType`,
  `Building::rankNearbyIntact`, `Vehicle::loadUnmannedUfoLoot`,
  `Organisation::raidingStrength`, `VEquipmentType::scoreRequirementByDifficulty`,
  `AEquipmentType::artifactUnhidden` (x3), `Organisation::exeOrgIndex`,
  `UfopaediaEntry::startVisible`, `UfopaediaEntry::catalogCategory`,
  `VEquipmentType::clearEconomyHide`, `Base::BuildError::NoFacility`,
  `BattleMap::briefing`
- `test_battle_hazard` (6): `AEquipmentType::fireHazardDamage`,
  `BattleHazard::advanceFireOverlay`, `BattleMapPart::fireStageBurns`,
  `BattleHazard::FIRE_SPREAD_NEIGHBOUR_RNG_SPAN` (x2), `Battle::advanceFireContactCounter`
- `test_organisation` (3): `Organisation::raidRelationPressure`,
  `::guardCountFromRoll`, `::raidManpower`
- `test_vehicle_mission` (5): `Base::knownToAliens`,
  `GroundVehicleTileHelper::footprintTiles`, `Base::alienExposureRollSucceeds`,
  `GameState::allocateUfo2pBaseSlot`, `Vehicle::selectDimensionExitPortal`
- `test_research` (4): `ItemDependency::Type`, `ufo2pAlienLifeformItemId`,
  `ALIEN_DETECTION_WEIGHT_OFFSET_END`, `UFO2P_AA7A8_BIOCHEM_ANY`
- `test_gametime` (3): `HAND_WEAPON_FIRE_PRIORITY_BASE`,
  `vanillaInvasionDelayTicks`, `vanillaCitySpeed1Ticks`
- `test_psionics` (1): `test_psi_costs_match_prior_art` — develop promoted
  `getPsiCost()` to a `BattleUnit` static member. On master it is still
  `static` at namespace scope in `battleunit.h`, so it has internal linkage and
  no test translation unit can link against it.

Two develop-only extractor headers were also removed from includes:
`tools/extractors/common/exe_slide.h` (used only by the dropped
`test_exe_slide_crcs`) and `tools/extractors/common/ufomissionpattern.h`
(included but never used).

**Compiled, but asserted develop behaviour.** 10 cases, each verified against the
develop diff that changed the rule:

- `test_city_rules` (7): `org_park_funds`, `org_park_sell_surplus`,
  `purchase_deduct`, `cargo_expiry_refund` — develop added
  `settleMarketPurchase()`, `Organisation::parkPurchaseBudget()` and
  `GameState::vehicleParkSpawnTable`; `overspawn_invasion` — develop added
  `UFOIncursion::PrimaryMission::Overspawn` handling;
  `ufo_mission_counter_decrements_from_mission_start` — needs
  `advanceMissionCounterOnArrival()`; `ufo_incursion_follow_type` — develop added
  `followVehicleType` slot logic.
- `test_organisation` (2): `infiltration_hourly_ufo2p_rules`,
  `infiltration_divisor_uses_difficulty` — develop rewrote the divisor to
  `42 - state.difficulty` and moved the clamp.
- `test_economy` (1): `senate_tightest_band` — develop added tightest-band
  overwrite to funding selection.

(`test_game_end`'s two cases are counted under the whole-file drops above, not
here.)

## Totals

**28 test binaries**: master's original 10, plus 18 ported from develop. 25 are
in the regression gate and all pass; 3 are quarantined reproducers.

Of develop's 26 additional test files, 18 came across, carrying 67 cases and 394
assertions — 60 cases / 339 assertions in the gate, and 7 cases / 55 assertions
in the three quarantined reproducers. 3 files were never ported because the
subsystem does not exist on master, and 5 were dropped whole because every case
in them asserted develop-only behaviour.

A further 61 individual cases were dropped from files that otherwise survived:
51 that would not compile against master's API, and 10 that compiled but
asserted rules develop had changed.

Of the 7 quarantined cases, 6 fail, covering 3 distinct defects in master.
