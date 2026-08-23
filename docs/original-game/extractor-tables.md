# Extractor tables (graph substitute)

codebase-memory skip-lists `tools/`, so `OpenApoc::UFO2P` is not in the graph. This list is the indexed stand-in. Grep the headers for offsets.

## UFO2P (`tools/extractors/common/ufo2p.h`)

| Member | Header | Runtime |
| -------- | -------- | --------- |
| `research_data` / `research_names` / `research_descriptions` | `research.h` | Consumed. `prereqType` 0/1 + `prereq` copy into item gates. Type 3 (alien) and `leadsTo` / ufopaedia entry indices stay patch/unbound. |
| `vehicle_data` / `vehicle_names` | `vehicle.h` | Consumed. `loftemps_index` (+0x28) is hexa “chance to evade bullets” and drives voxelmaps. Attack-mode dodge 100/80/50/10 is a separate hardcoded path. Freight / rescue sets still hardcoded in the extractor. |
| `organisation_data` / `organisation_starting_relationships_data` / `vehicle_park` / `organisation_raid_loot_data` | `organisations.h` | Consumed. `organization_type` drives `militarizedFromType`. `raiding_strength` copied, no consumer. `rebuildingRate` serialized, unread. |
| `building_names` / `building_functions` / `alien_building_names` | `building.h` | Consumed. `agentSpawnType` is ufopaedia-only. |
| `economy_data1` / `economy_data2` / `economy_data3` | `economy.h` | Consumed. `economy_data2` IDs come from `craft_ammo_names` at `0x14B18E`. |
| `facility_data` / `facility_names` | `facilities.h` | Consumed. |
| `agent_types` / `agent_type_names` / `agent_equipment_names` | `agent.h`, `aequipment.h` | Consumed. Extractor skips hireable type index 0. |
| `vehicle_equipment` / `vehicle_weapons` / `vehicle_engines` / `vehicle_general_equipment` / `vehicle_equipment_layouts` | `vequipment.h` | Consumed. |
| `baselayouts` | `baselayout.h` | Consumed. |
| `rawsound` | `audio.h` | Loaded, unused. Sounds are `RAWSOUND:…` path strings. |
| `ufopaedia_group` | `ufopaedia.h` | Loaded; extractor asserts 10 names and creates any missing `PAEDIACATEGORY_*`. Entry unlocks stay in the patch. |
| `manufacturing_data` / `manufacturing_names` | `research.h` | Consumed. Type 2 `itemIndex` uses `craft_ammo_names`. |
| `craft_ammo_names` | `economy.h` | Consumed. 15 names at `0x14B18E`–`0x14B29A`. |
| `craft_ammo_manufacturers` | `economy.h` | Consumed. `uint16[15]` org index at `0x13EB6A`. Zorium is org 0 (X-COM). |
| `ufo_growth_rates` | `ufogrowth.h` | Consumed. |
| `ufo_mission_data` | `ufoincursion.h` | Consumed. Slot craft/count/role; trailing 24 B unbound. |
| `ufo_mission_patterns` | `ufomissionpattern.h` | Consumed. 20×10 uint16 (19 weeks + DEFAULT). IDs 3/1/2/5. |

ISO non-4 offsets are extractor-canonical. `4`-build deltas live in the sibling lab CSVs (`labels/ufo2p_rebase.csv`). Object-2 VAs for mapped city tables use `file − VA = 0x2C400` (`research_data` `0x13EE80` → `0x112A80`). Vehicle / economy / park / rawsound are `file_tail` (no VA). Bound Ghidra reload notes: [compare-report.html#ghidra](compare-report.html#ghidra).

## TACP (`tools/extractors/common/tacp.h`)

| Member | Header | Runtime |
| -------- | -------- | --------- |
| `damage_type_data` / `damage_modifier_data` | `aequipment.h` | Consumed. Modifier names hand-filled in `tacp.cpp`. |
| `agent_equipment` / `agent_armor` / `agent_weapon` / `agent_general` / `agent_payload` | `aequipment.h` | Consumed. Some hazard fields hardcoded in the extractor. |
| `agent_equipment_set_builtin` / score sets | `aequipment.h` | Consumed. City-side hexa copies are unused. |
| `bulletsprite` / `projectilesprites` | `bulletsprite.h` | Images / embedded sprites. |

TACP `4` deltas: sibling `labels/tacp_rebase.csv` (typically −0x2200).

## Hexa-only (not in these headers)

Every range below is dumped. Classification is against current OpenApoc, not a promise the stand-in matches bytes.

| Hexa name | Non-4 file | Class | OpenApoc stand-in |
| ----------- | ------------ | ------- | ------------------- |
| `UFO_mission_data` | `0x13DDFC` (VA `0x1119FC`) | extracted | `extract_ufo_incursions.cpp` (45×42). Reference: `docs/original-game/exe-tables/ufo_incursions.xml` (must not live under `common_patch/gamestate/`). |
| `manufacturing_data` | `0x13FD34` (VA `0x113934`) | extracted | `extract_manufacturing.cpp` (43×50) + patch overlay |
| `manufacturing_items` | `0x1501F3` | extracted | `manufacturing_names` strtab |
| `UFO_growth_rates` | `0x155010` (VA `0x128C10`) | extracted | `extract_ufo_growth.cpp`. Reference: `docs/original-game/exe-tables/ufo_growth_lists.xml` (must not live under `common_patch/gamestate/`). |
| `UFO_mission_patterns` | `0x155164` (VA `0x128D64`) | extracted | `extract_ufo_mission_preference.cpp` (20×10 uint16). Do not XInclude a patch copy. |
| `vehicle_park_spawn_table` | `0x188F18` | hardcoded | switches in `extract_organisations.cpp` |
| `starting_available_UFOPaedia_entries` | `0x19196A` | patch | empty `<dependency>` in `ufopaedia_entries.xml` |
| `senate_relationships` | `0x15055B` | patch | `weekly_rating_rules` in `gamestate.xml` |
| `craft_ammo_names` | `0x14B18E` | extracted | `UFO2P::craft_ammo_names` + `getVAmmoId` (15 names) |
| `craft_ammo_manufacturers_data` | `0x13EB6A` | extracted | `uint16[15]` org index; Zorium is X-COM |
| `equipment_sets_builtin` | `0x1568EC` | city-mirror | TACP `agent_equipment_set_built_in` |
| `damage_mod_data` | `0x19AA10` | city-mirror | TACP `damage_modifiers` |
| `aequip3_data` / `aequip2_*` / `aequip1_data` / `aequip_sets_data` / `aequip_score_req_data` | `0x19AD4C+` | city-mirror | TACP equipment extract |
| `alien_unit_names` | `0x150695` | city-mirror | extracted `agent_type_names` |
| `damage_types` (UFO2P) | `0x154896` | city-mirror | TACP `damage_types` |
| `unit_measurements` | `0x149DB1` | hardcoded | heights in `extract_agent_types.cpp` |
| `agent_ranks` | `0x150739` | hardcoded | rank logic in agent UI/state |
| `difficulty_levels` | `0x1541F6` | hardcoded | `Difficulty` enum |
| `cequip_score_req_data` | `0x1421C4` | unwired | no OpenApoc type |
| `aequip_alien_artifact_data` | `0x1422A8` | unwired | no reader |
| `not_used` / megaprime spare | several | padding | ignore |

Full dump list: [compare-report.html](compare-report.html) table audit.
