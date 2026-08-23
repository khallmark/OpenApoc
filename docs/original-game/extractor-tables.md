# Extractor tables (graph substitute)

codebase-memory skip-lists `tools/`, so `OpenApoc::UFO2P` is not in the graph. This list is the indexed stand-in. Grep the headers for offsets.

## UFO2P (`tools/extractors/common/ufo2p.h`)

| Member | Header |
|--------|--------|
| `research_data` / `research_names` / `research_descriptions` | `research.h` |
| `vehicle_data` / `vehicle_names` | `vehicle.h` |
| `organisation_data` / `organisation_starting_relationships_data` / `vehicle_park` / `organisation_raid_loot_data` | `organisations.h` |
| `building_names` / `building_functions` / `alien_building_names` | `building.h` |
| `economy_data1` / `economy_data2` / `economy_data3` | `economy.h` |
| `facility_data` / `facility_names` | `facilities.h` |
| `agent_types` / `agent_type_names` / `agent_equipment_names` | `agent.h`, `aequipment.h` |
| `vehicle_equipment` / `vehicle_weapons` / `vehicle_engines` / `vehicle_general_equipment` / `vehicle_equipment_layouts` | `vequipment.h` |
| `baselayouts` | `baselayout.h` |
| `rawsound` | `audio.h` |
| `ufopaedia_group` | `ufopaedia.h` |

ISO non-4 offsets are extractor-canonical. `4`-build deltas live in the sibling lab CSVs (`labels/ufo2p_rebase.csv`).

## TACP (`tools/extractors/common/tacp.h`)

| Member | Header |
|--------|--------|
| `damage_type_data` / `damage_modifier_data` | `aequipment.h` |
| `agent_equipment` / `agent_armor` / `agent_weapon` / `agent_general` / `agent_payload` | `aequipment.h` |
| `agent_equipment_set_builtin` / score sets | `aequipment.h` |
| `bulletsprite` / `projectilesprites` | `bulletsprite.h` |

TACP `4` deltas: sibling `labels/tacp_rebase.csv` (typically −0x2200).

## Hexa-only (not in these headers)

See [compare-report.html](compare-report.html) table audit: `UFO_mission_data`, `manufacturing_data`, `UFO_growth_rates`, `UFO_mission_patterns`, `vehicle_park_spawn_table`, `senate_relationships`.
