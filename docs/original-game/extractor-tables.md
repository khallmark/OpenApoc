# Extractor tables (graph substitute)

codebase-memory skip-lists `tools/`, so `OpenApoc::UFO2P` is not in the graph. This list is the indexed stand-in. Grep the headers for offsets.

## UFO2P (`tools/extractors/common/ufo2p.h`)

| Member | Header | Runtime |
| -------- | -------- | --------- |
| `research_data` / `research_names` / `research_descriptions` | `research.h` | Consumed. `prereqType` 0/1 + `prereq` are craft/agent item gates. Type 3 is 15 live bio slots + dead at live+15. `unknown2` 0/1 → All/Any of `prereqTech[3]`. Case 4 is 29 rows (`XOR EBX` @ file `0x10D141`); leftover record 46 `prereqTech[0]=45` is not a combinator input (`de2b0[topic]` override unbound). `unknown1` 0/1 → All/Any of three typed item gates (`FUN_000aa7a8` / `FUN_000aab60`). After case 4, listing applies record 37 All of 12..17 plus topic 36, record 38 All of 8..31, record 44 Any of 7..33 (lock: `research_aa7a8_hardcoded_gates`). Duplicate `"Alien building"` / `"Overspawn Autopsy"` names get distinct IDs. `leadsTo` is not a research-graph edge. `ufopaediaGroup`/`ufopaediaEntry` (+25/+26) feed `FUN_000abf9c` @ file `0xFE640` (lock: `ufopaedia_abf9c_unlock`). |
| `vehicle_data` / `vehicle_names` | `vehicle.h` | Consumed. `loftemps_index` (+0x28) is hexa “chance to evade bullets” and drives voxelmaps. Attack-mode dodge 100/80/50/10 is a separate hardcoded path. Freight / rescue sets still hardcoded in the extractor. |
| `organisation_data` / `organisation_starting_relationships_data` / `vehicle_park` / `organisation_raid_loot_data` / `vehicle_park_spawn_table` / `vehicle_park_spawn_cap` | `organisations.h` | Consumed. `organization_type` drives `militarizedFromType`. `average_guards` (+7) → `FUN_000aec70` / `FUN_000aedb0`. `raiding_strength` (+2) → `FUN_00092060` fallback `(raid/100)×avg²`. `rebuildingRate` copied, no post-init reader. Raid loot A/B/C → `Organisation::loot`. Spawn table `0x188F18` (40 IDs) + caps `0x188FB8` feed `FUN_000962cc`. |
| `building_names` / `building_functions` / `alien_building_names` / `building_detection_weight` | `building.h` | Consumed. `agentSpawnType` is extracted and serialized; no UFOPaedia or simulation reader. Detection weights are 49×uint32 at `0x155354` (4-build `+0xE00`; index 0 is the dummy `-`, Senate = 155). |
| `infiltration_speed_org` / `infiltration_speed_agent` / `infiltration_speed_building` | `organisations.h`, `agent.h`, `building.h` | Consumed into organisation, alien-agent, and building-function infiltration rules. |
| `scenery_minimap_colour` | `scenery.h` | Consumed by `extract_city_scenery.cpp` for city minimap colours. |
| `bullet_sprites` / `projectile_sprites` | `bulletsprite.h` | Consumed by city/battle projectile sprite extraction. |
| `crew_ufo_downed` / `crew_ufo_deposit` / `crew_alien_building` | `crew.h` | Consumed by `extract_vehicles.cpp`; only `crew_ufo_downed` is the documented no-slide P↔P4 exception. |
| `economy_data1` / `economy_data2` / `economy_data3` | `economy.h` | Consumed. `economy_data2` IDs come from `craft_ammo_names` at `0x14B18E`. After patch, hyphenated `getVAmmoId` keys are remounted onto the live `VAmmoType` id (Elerium / Multi-Cannon). Vequip `week==0` inits hide RAM `DAT_00183b0a` (`FUN_00014854` @ `0x77116`); type-0 manufacture clears it. |
| `facility_data` / `facility_names` | `facilities.h` | Consumed. |
| `agent_types` / `agent_type_names` / `agent_equipment_names` / `alien_detection_weight` / `alien_movement_percent` | `agent.h`, `aequipment.h` | Consumed. Extractor skips hireable type index 0. Alien infiltration extras at `0x142374` / `0x1423B0` (13×uint32 each, same index as `infiltration_speed_agent`). |
| `vehicle_equipment` / `vehicle_weapons` / `vehicle_engines` / `vehicle_general_equipment` / `vehicle_equipment_layouts` / `cequip_score_req` | `vequipment.h` | Consumed. `ammo_type` indexes `craft_ammo_names` (`0xffff` = none). `split_idx` 24 is the unnamed Multi-Bomb fragment row @ `0x18B510`. `cequip_score_req_data` at `0x1421C4` is 5×5 uint32; rows 0–3 → vequip 44–47. Row 4 unnamed. |
| `baselayouts` | `baselayout.h` | Consumed. |
| `rawsound` | `audio.h` | Loaded, unused. Sounds are `RAWSOUND:…` path strings. |
| `ufopaedia_group` / `ufopaedia_catalog` / `ufopaedia_start_visible` / `ufopaedia_pcx_names` | `ufopaedia.h` | Group strtab asserts 10 names. After patch, catalog `0x1910A2` + start bytes `0x19196A` + PCX names `0x134AA8` overlay `startVisible` and store `catalogCategory`/`catalogIndex` (lock: `ufopaedia_start_visible`). Cat 8 binds Building or Nothing (story pages). Type-mismatched PCX stays `0xFFFF` and keeps patch deps. `FUN_000abf9c` @ file `0xFE640` sets the start byte from research +25/+26 (lock: `ufopaedia_abf9c_unlock`). `FUN_0008c860` @ file `0xEEF04` type 2/3 also gates `DAT_00183b3b` / `DAT_00183b0a` (lock: `ufopaedia_economy_hide`). |
| `manufacturing_data` / `manufacturing_names` | `research.h` | Consumed. Type 2 `itemIndex` uses `craft_ammo_names`. Records 38–42 keep `()` in the topic ID (lock: `manufacture_disruptor_armor_ids`). Type 0 produce clears vequip hide (`FUN_000ab440` case 0 @ `0x10DC0D`). Type 1 produce clears aequip hide (case 1 @ `0x10DC3C`). |
| `craft_ammo_names` | `economy.h` | Consumed. 15 names at `0x14B18E`–`0x14B29A`. |
| `craft_ammo_manufacturers` | `economy.h` | Consumed. `uint16[15]` org index at `0x13EB6A`. Zorium is org 0 (X-COM). |
| `ufo_growth_rates` | `ufogrowth.h` | Consumed. |
| `ufo_mission_data` | `ufoincursion.h` | Consumed. Slot craft/count/role. Tail reader `FUN_0006da88` @ file `0xD012C`: `follow_slot` → escort `followVehicleType`. Zone/scatter feed `FUN_0003b724` @ file `0x2B723` (`FUN_0005d1d8` @ `0x4D1D7`, range `0..n`). `type_percent` × constitution (`VehicleType::health`); scatter>50→10 clamp. Hexa's `building_function` label at +0x1B is wrong: the byte is copied to vehicle +0x171 and decremented on mission-destination arrival by `FUN_0003a910` @ object-page file `0x2A90F`; extracted as `mission_counter`. `FUN_0005e7f4` @ object-page file `0x4E7F3` is a vehicle-volume occupancy test. VA `0x1439E0` is dimension-gate runtime RAM initialized by `FUN_0006cdd0` and paired by `FUN_0006eab0`, not an org table. `+0x168` gate remains unbound. |
| `ufo_mission_patterns` | `ufomissionpattern.h` | Consumed. 20×10 uint16 (19 weeks + DEFAULT). IDs 3/1/2/5. |

ISO non-4 offsets are extractor-canonical. `4`-build deltas live in the sibling lab CSVs (`labels/ufo2p_rebase.csv`). Object-2 VAs for mapped city tables use `file − VA = 0x2C400` (`research_data` `0x13EE80` → `0x112A80`). Vehicle / economy / park / rawsound are `file_tail` (no VA). Bound Ghidra reload notes: [compare-report.html#ghidra](compare-report.html#ghidra).

## TACP (`tools/extractors/common/tacp.h`)

| Member | Header | Runtime |
| -------- | -------- | --------- |
| `damage_type_data` / `damage_modifier_data` | `aequipment.h` | Consumed. Modifier names hand-filled in `tacp.cpp`. |
| `agent_equipment` / `agent_armor` / `agent_weapon` / `agent_general` / `agent_payload` / `fire_hazard_power_table` | `aequipment.h` | Consumed. `unknown01` → `hazardResist` (`FUN_0007c110` @ file `0xD6BB4`). Power table `0x2E2AF4` (`FUN_0007ae18` @ file `0xD58BC`) drives item contact for recovered overlays. `BattleHazard::fireOverlay` preserves type 2 + the six-bit index (`FUN_0007ad94`), including preplaced marker 25 → `0x99`. `Battle::updateFireScheduler` implements real-time `FUN_0007b7f8`: four OpenApoc ticks per vanilla iteration, `(mapY×mapZ)/0x48` X rows, persistent Y/Z cursors, then contact counter check/reset/increment at `0x24`. Turn-based round wrap runs the `FUN_000b8c50` 400-iteration batch; item contacts remain enabled and legacy per-owner updates are skipped. Row progression uses `FUN_0007b3dc` extinction and terrain `fire_burn_time`. Unit fire intensity, generic placement, and spread RNG (`FUN_0001eee8` / `FUN_0007b0d0`) remain unbound; generic hazards keep the old path and `HAZARD_SPREAD_CHANCE` is still made up. |
| `agent_equipment_set_builtin` / score sets | `aequipment.h` | Consumed. City-side hexa copies are unused. |
| `bulletsprite` / `projectilesprites` | `bulletsprite.h` | Images / embedded sprites. |

TACP `4` deltas: sibling `labels/tacp_rebase.csv` (typically −0x2200).

## Hexa-only (not in these headers)

Every range below is dumped. Classification is against current OpenApoc, not a promise the stand-in matches bytes.

| Hexa name | Non-4 file | Class | OpenApoc stand-in |
| ----------- | ------------ | ------- | ------------------- |
| `UFO_mission_data` | `0x13DDFC` (VA `0x1119FC`) | extracted | `extract_ufo_incursions.cpp` (45×42; follow-slot + `FUN_0003b724` spawn XY @ `0x2B723`; `type_percent` × constitution; scatter>50→10 clamp; +0x1B `mission_counter` bound by `FUN_0003a910`; `+0x168` gate extracted-only). Reference: `docs/original-game/exe-tables/ufo_incursions.xml` (must not live under `common_patch/gamestate/`). |
| `manufacturing_data` | `0x13FD34` (VA `0x113934`) | extracted | `extract_manufacturing.cpp` (43×50) + patch overlay. Records 38–42 keep `()` so patch armor keys merge. Type 1 `itemIndex` unhides artifacts (`FUN_000ab440` @ `0x10DC3C`). |
| `manufacturing_items` | `0x1501F3` | extracted | `manufacturing_names` strtab |
| `UFO_growth_rates` | `0x155010` (VA `0x128C10`) | extracted | `extract_ufo_growth.cpp`. Reference: `docs/original-game/exe-tables/ufo_growth_lists.xml` (must not live under `common_patch/gamestate/`). |
| `UFO_mission_patterns` | `0x155164` (VA `0x128D64`) | extracted | `extract_ufo_mission_preference.cpp` (20×10 uint16). Do not XInclude a patch copy. |
| `building_detection_weight` | `0x155354` | extracted | 49×uint32 after mission patterns (4-build `+0xE00`). Index 0 is dummy `-`; Senate = 155. Hexa left the 96 B gap at `0x1552F4` unnamed. |
| hexa “unknown; used for equipping routine??” det/move | `0x142374` / `0x1423B0` | extracted | 13×uint32 + 8 B pad + 13×uint32; same index as `infiltration_speed_agent` (egg=0 … micronoid=12). |
| `vehicle_park_spawn_table` | `0x188F18` | extracted | `FUN_000962cc` 40 IDs (Megapol +20; index 39 is the dword the EXE reads). Caps `0x188FB8`. |
| `starting_available_UFOPaedia_entries` | `0x19196A` | extracted | `applyUfopaediaStartVisible` after patch (unique PCX + data_type + catalog bind). `FUN_000abf9c` @ file `0xFE640` writes the start byte. Reference: `docs/original-game/exe-tables/ufopaedia_start_visible.xml`. |
| `senate_relationships` | `0x15055B` | patch | `weekly_rating_rules` in `gamestate.xml` |
| `craft_ammo_names` | `0x14B18E` | extracted | `UFO2P::craft_ammo_names` + `getVAmmoId` (15 names). `vehicle_equipment.ammo_type` indexes this table. |
| `craft_ammo_manufacturers_data` | `0x13EB6A` | extracted | `uint16[15]` org index; Zorium is X-COM. Overlay after patch; do not keep `<manufacturer>` in `vehicle_ammo.xml`. |
| `equipment_sets_builtin` | `0x1568EC` | city-mirror | TACP `agent_equipment_set_built_in` |
| `damage_mod_data` | `0x19AA10` | city-mirror | TACP `damage_modifiers` |
| `aequip3_data` / `aequip2_*` / `aequip1_data` / `aequip_sets_data` / `aequip_score_req_data` | `0x19AD4C+` | city-mirror | TACP equipment extract |
| `alien_unit_names` | `0x150695` | city-mirror | extracted `agent_type_names` |
| `damage_types` (UFO2P) | `0x154896` | city-mirror | TACP `damage_types` |
| `unit_measurements` | `0x149DB1` | hardcoded | UI strings (` per unit.`, ` per clip.`), not agent heights |
| `agent_ranks` | `0x150739` | hardcoded | rank logic in agent UI/state |
| `difficulty_levels` | `0x1541F6` | hardcoded | `Difficulty` enum |
| `cequip_score_req_data` | `0x1421C4` | extracted | `VEquipmentType::scoreRequirementByDifficulty`; row 4 unbound |
| `vehicle_weapon_data` row 24 | `0x18B510` | extracted | Unnamed Multi-Bomb fragment (`split_idx` of launcher data_idx 15). Overlay after patch; do not keep combat scalars in `vehicle_equipment.xml`. |
| `aequip_alien_artifact_data` | `0x1422A8` | extracted | 87×uint8, 25 flags overlay TACP’s zero `artifact` byte. `FUN_000811fc` @ file `0xE3971` / `FUN_0008c860` hide while set. Shop list does not treat `economy` week 0 as hide (lock: `aequip_market_week0`). `FUN_000aac88` clears on same-name type-1 complete; `FUN_000ab440` case 1 @ file `0x10DC3C` (4-build `0x10E4AE`) clears `DAT_00183b3b[itemIndex]` on manufacture. Destabiliser 37 is same-name. AG missiles 19/23 unhide on manufacture only. Index 57 / 75–83 never clear. |
| `not_used` / megaprime spare | several | padding | ignore |

Full dump list: [compare-report.html](compare-report.html) table audit.
