#include "ufo2p.h"
#include "framework/data.h"
#include "framework/framework.h"
#include "tools/extractors/common/exe_slide.h"

#include <boost/crc.hpp>
#include <iomanip>
#include <iterator>

namespace OpenApoc
{

UFO2P::UFO2P(std::string file_name)
{
	auto file = fw().data->fs.open(file_name);

	if (!file)
	{
		LogError("Failed to open \"{0}\"", file_name.c_str());
		exit(1);
	}
	auto data = file.readAll();
	boost::crc_32_type crc;
	crc.process_bytes(data.get(), file.size());

	auto crc32 = crc.checksum();
	int32_t slide = 0;
	if (!ufo2pFileSlide(crc32, slide))
	{
		LogError("File \"{0}\" has unknown crc32 {1:08x} (known non-4 {2:08x}, 4-build {3:08x})",
		         file_name.c_str(), crc32, UFO2P_CRC_NON4, UFO2P_CRC_4);
	}
	const auto at = [slide](off_t off) -> off_t { return ufo2pTableOffset(slide, off); };

	file.seekg(0, std::ios::beg);
	file.clear();

	this->research_data.reset(new DataChunk<ResearchData>(file, at(RESEARCH_DATA_OFFSET_START),
	                                                      at(RESEARCH_DATA_OFFSET_END)));
	this->research_names.reset(new StrTab(file, at(RESEARCH_NAME_STRTAB_OFFSET_START),
	                                      at(RESEARCH_NAME_STRTAB_OFFSET_END), true));
	this->research_descriptions.reset(new StrTab(file, at(RESEARCH_DESCRIPTION_STRTAB_OFFSET_START),
	                                             at(RESEARCH_DESCRIPTION_STRTAB_OFFSET_END)));
	this->ufopaedia_group.reset(new StrTab(file, at(UFOPAEDIA_GROUP_STRTAB_OFFSET_START),
	                                       at(UFOPAEDIA_GROUP_STRTAB_OFFSET_END)));
	this->ufopaedia_catalog.reset(new DataChunk<UfopaediaCatalogRow>(
	    file, at(UFOPAEDIA_CATALOG_OFFSET_START), at(UFOPAEDIA_CATALOG_OFFSET_END)));
	this->ufopaedia_start_visible.reset(
	    new DataChunk<uint8_t>(file, at(STARTING_AVAILABLE_UFOPAEDIA_OFFSET_START),
	                           at(STARTING_AVAILABLE_UFOPAEDIA_OFFSET_END)));
	this->ufopaedia_pcx_names.reset(new StrTab(file, at(UFOPAEDIA_PCX_NAME_STRTAB_OFFSET_START),
	                                           at(UFOPAEDIA_PCX_NAME_STRTAB_OFFSET_END)));
	this->manufacturing_data.reset(new DataChunk<ManufacturingData>(
	    file, at(MANUFACTURING_DATA_OFFSET_START), at(MANUFACTURING_DATA_OFFSET_END)));
	this->manufacturing_names.reset(new StrTab(file, at(MANUFACTURING_NAME_STRTAB_OFFSET_START),
	                                           at(MANUFACTURING_NAME_STRTAB_OFFSET_END), true));

	this->organisation_data.reset(new DataChunk<OrganisationData>(
	    file, at(ORGANISATION_DATA_OFFSET_START), at(ORGANISATION_DATA_OFFSET_END)));
	this->organisation_raid_loot_data.reset(
	    new DataChunk<OrgRaidLootData>(file, at(ORGANISATION_RAID_LOOT_DATA_OFFSET_START),
	                                   at(ORGANISATION_RAID_LOOT_DATA_OFFSET_END)));
	this->organisation_starting_relationships_data.reset(
	    new DataChunk<OrgStartingRelationshipsData>(
	        file, at(ORGANISATION_STARTING_RELATIONSHIPS_DATA_OFFSET_START),
	        at(ORGANISATION_STARTING_RELATIONSHIPS_DATA_OFFSET_END)));

	this->vehicle_data.reset(new DataChunk<VehicleData>(file, at(VEHICLE_DATA_OFFSET_START),
	                                                    at(VEHICLE_DATA_OFFSET_END)));
	this->vehicle_names.reset(
	    new StrTab(file, at(VEHICLE_NAME_STRTAB_OFFSET_START), at(VEHICLE_NAME_STRTAB_OFFSET_END)));

	this->organisation_names.reset(new StrTab(file, at(ORGANISATION_NAME_STRTAB_OFFSET_START),
	                                          at(ORGANISATION_NAME_STRTAB_OFFSET_END)));
	this->building_names.reset(new StrTab(file, at(BUILDING_NAME_STRTAB_OFFSET_START),
	                                      at(BUILDING_NAME_STRTAB_OFFSET_END)));

	this->building_functions.reset(new StrTab(file, at(BUILDING_FUNCTION_STRTAB_OFFSET_START),
	                                          at(BUILDING_FUNCTION_STRTAB_OFFSET_END)));

	this->alien_building_names.reset(new StrTab(file, at(ALIEN_BUILDING_NAME_STRTAB_OFFSET_START),
	                                            at(ALIEN_BUILDING_NAME_STRTAB_OFFSET_END)));

	this->rawsound.reset(
	    new DataChunk<RawSoundData>(file, at(RAWSOUND_OFFSET_START), at(RAWSOUND_OFFSET_END)));
	this->baselayouts.reset(new DataChunk<BaseLayoutData>(file, at(BASELAYOUT_OFFSET_START),
	                                                      at(BASELAYOUT_OFFSET_END)));

	this->agent_equipment_names.reset(new StrTab(file, at(AGENT_EQUIPMENT_NAMES_OFFSET_START),
	                                             at(AGENT_EQUIPMENT_NAMES_OFFSET_END)));

	this->agent_type_names.reset(
	    new StrTab(file, at(AGENT_TYPE_NAMES_OFFSET_START), at(AGENT_TYPE_NAMES_OFFSET_END), true));

	this->agent_types.reset(new DataChunk<AgentTypeData>(file, at(AGENT_TYPE_DATA_OFFSET_START),
	                                                     at(AGENT_TYPE_DATA_OFFSET_END)));

	this->vehicle_equipment_names.reset(new StrTab(file, at(VEHICLE_EQUIPMENT_NAMES_OFFSET_START),
	                                               at(VEHICLE_EQUIPMENT_NAMES_OFFSET_END)));

	this->vehicle_equipment.reset(new DataChunk<VehicleEquipmentData>(
	    file, at(VEHICLE_EQUIPMENT_DATA_OFFSET_START), at(VEHICLE_EQUIPMENT_DATA_OFFSET_END)));
	this->vehicle_weapons.reset(new DataChunk<VehicleWeaponData>(
	    file, at(VEHICLE_WEAPON_DATA_OFFSET_START), at(VEHICLE_WEAPON_DATA_OFFSET_END)));
	this->vehicle_engines.reset(new DataChunk<VehicleEngineData>(
	    file, at(VEHICLE_ENGINE_DATA_OFFSET_START), at(VEHICLE_ENGINE_DATA_OFFSET_END)));
	this->vehicle_general_equipment.reset(new DataChunk<VehicleGeneralEquipmentData>(
	    file, at(VEHICLE_GENERAL_EQUIPMENT_DATA_OFFSET_START),
	    at(VEHICLE_GENERAL_EQUIPMENT_DATA_OFFSET_END)));
	this->cequip_score_req.reset(new DataChunk<CequipScoreReqData>(
	    file, at(CEQUIP_SCORE_REQ_DATA_OFFSET_START), at(CEQUIP_SCORE_REQ_DATA_OFFSET_END)));

	this->vehicle_equipment_layouts.reset(new DataChunk<VehicleEquipmentLayout>(
	    file, at(VEHICLE_EQUIPMENT_LAYOUT_OFFSET_START), at(VEHICLE_EQUIPMENT_LAYOUT_OFFSET_END)));

	this->facility_names.reset(
	    new StrTab(file, at(FACILITY_STRTAB_OFFSET_START), at(FACILITY_STRTAB_OFFSET_END)));
	this->facility_data.reset(new DataChunk<FacilityData>(file, at(FACILITY_DATA_OFFSET_START),
	                                                      at(FACILITY_DATA_OFFSET_END)));

	this->economy_data1.reset(new DataChunk<EconomyData>(file, at(ECONOMY_DATA1_OFFSET_START),
	                                                     at(ECONOMY_DATA1_OFFSET_END)));
	this->economy_data2.reset(new DataChunk<EconomyData>(file, at(ECONOMY_DATA2_OFFSET_START),
	                                                     at(ECONOMY_DATA2_OFFSET_END)));
	this->economy_data3.reset(new DataChunk<EconomyData>(file, at(ECONOMY_DATA3_OFFSET_START),
	                                                     at(ECONOMY_DATA3_OFFSET_END)));
	this->craft_ammo_names.reset(new StrTab(file, at(CRAFT_AMMO_NAME_STRTAB_OFFSET_START),
	                                        at(CRAFT_AMMO_NAME_STRTAB_OFFSET_END)));
	this->craft_ammo_manufacturers.reset(new DataChunk<uint16_t>(
	    file, at(CRAFT_AMMO_MANUFACTURERS_OFFSET_START), at(CRAFT_AMMO_MANUFACTURERS_OFFSET_END)));

	this->scenery_minimap_colour.reset(
	    new DataChunk<SceneryMinimapColour>(file, at(SCENERY_MINIMAP_COLOUR_DATA_OFFSET_START),
	                                        at(SCENERY_MINIMAP_COLOUR_DATA_OFFSET_END)));

	this->bullet_sprites.reset(new DataChunk<BulletSprite>(
	    file, at(BULLETSPRITE_DATA_UFO2P_OFFSET_START), at(BULLETSPRITE_DATA_UFO2P_OFFSET_END)));
	this->projectile_sprites.reset(
	    new DataChunk<ProjectileSprites>(file, at(PROJECTILESPRITES_DATA_UFO2P_OFFSET_START),
	                                     at(PROJECTILESPRITES_DATA_UFO2P_OFFSET_END)));

	this->crew_ufo_downed.reset(new DataChunk<CrewData>(file, at(CREW_UFO_DOWNED_OFFSET_START),
	                                                    at(CREW_UFO_DOWNED_OFFSET_END)));
	this->crew_ufo_deposit.reset(new DataChunk<CrewData>(file, at(CREW_UFO_DEPOSIT_OFFSET_START),
	                                                     at(CREW_UFO_DEPOSIT_OFFSET_END)));
	this->crew_alien_building.reset(new DataChunk<CrewData>(
	    file, at(CREW_ALIEN_BUILDING_OFFSET_START), at(CREW_ALIEN_BUILDING_OFFSET_END)));

	this->infiltration_speed_org.reset(
	    new DataChunk<OrgInfiltrationSpeed>(file, at(ORGANISATION_INFILTRATION_SPEED_OFFSET_START),
	                                        at(ORGANISATION_INFILTRATION_SPEED_OFFSET_END)));
	this->vehicle_park.reset(
	    new DataChunk<OrgVehicleParkData>(file, at(ORGANISATION_VEHICLE_PARK_DATA_OFFSET_START),
	                                      at(ORGANISATION_VEHICLE_PARK_DATA_OFFSET_END)));
	this->vehicle_park_spawn_table.reset(new DataChunk<uint32_t>(
	    file, at(VEHICLE_PARK_SPAWN_TABLE_OFFSET_START), at(VEHICLE_PARK_SPAWN_TABLE_OFFSET_END)));
	this->vehicle_park_spawn_cap.reset(new DataChunk<uint32_t>(
	    file, at(VEHICLE_PARK_SPAWN_CAP_OFFSET_START), at(VEHICLE_PARK_SPAWN_CAP_OFFSET_END)));
	this->aequip_alien_artifact.reset(
	    new DataChunk<uint8_t>(file, at(AEQUIP_ALIEN_ARTIFACT_DATA_OFFSET_START),
	                           at(AEQUIP_ALIEN_ARTIFACT_DATA_OFFSET_END)));

	this->infiltration_speed_agent.reset(new DataChunk<AgentInfiltrationSpeed>(
	    file, at(AGENT_INFILTRATION_SPEED_OFFSET_START), at(AGENT_INFILTRATION_SPEED_OFFSET_END)));
	this->infiltration_speed_building.reset(
	    new DataChunk<BuildingInfiltrationSpeed>(file, at(BUILDING_INFILTRATION_SPEED_OFFSET_START),
	                                             at(BUILDING_INFILTRATION_SPEED_OFFSET_END)));
	this->building_cost_data.reset(new DataChunk<BuildingCostData>(
	    file, at(BUILDING_COST_STRUCT_OFFSET_START), at(BUILDING_COST_STRUCT_OFFSET_END)));
	this->ufo_growth_rates.reset(new DataChunk<UfoGrowthRates>(
	    file, at(UFO_GROWTH_RATES_OFFSET_START), at(UFO_GROWTH_RATES_OFFSET_END)));
	this->ufo_mission_data.reset(new DataChunk<UfoMissionData>(
	    file, at(UFO_MISSION_DATA_OFFSET_START), at(UFO_MISSION_DATA_OFFSET_END)));
	this->ufo_mission_patterns.reset(new DataChunk<UfoMissionPatterns>(
	    file, at(UFO_MISSION_PATTERNS_OFFSET_START), at(UFO_MISSION_PATTERNS_OFFSET_END)));
}

void UFO2P::fillCrew(GameState &state, CrewData crew,
                     std::map<OpenApoc::StateRef<OpenApoc::AgentType>, int> &target)
{
	if (crew.alien_egg > 0)
	{
		target[{&state, "AGENTTYPE_MULTIWORM_EGG"}] = crew.alien_egg;
	}
	if (crew.anthropod > 0)
	{
		target[{&state, "AGENTTYPE_ANTHROPOD"}] = crew.anthropod;
	}
	if (crew.brainsucker > 0)
	{
		target[{&state, "AGENTTYPE_BRAINSUCKER"}] = crew.brainsucker;
	}
	if (crew.crysalis > 0)
	{
		target[{&state, "AGENTTYPE_CHRYSALIS"}] = crew.crysalis;
	}
	if (crew.hyperworm > 0)
	{
		target[{&state, "AGENTTYPE_HYPERWORM"}] = crew.hyperworm;
	}
	if (crew.megaspawn > 0)
	{
		target[{&state, "AGENTTYPE_MEGASPAWN"}] = crew.megaspawn;
	}
	if (crew.micronoid > 0)
	{
		target[{&state, "AGENTTYPE_MICRONOID_AGGREGATE"}] = crew.micronoid;
	}
	if (crew.multiworm > 0)
	{
		target[{&state, "AGENTTYPE_MULTIWORM"}] = crew.multiworm;
	}
	if (crew.popper > 0)
	{
		target[{&state, "AGENTTYPE_POPPER"}] = crew.popper;
	}
	if (crew.psimorph > 0)
	{
		target[{&state, "AGENTTYPE_PSIMORPH"}] = crew.psimorph;
	}
	if (crew.queenspawn > 0)
	{
		target[{&state, "AGENTTYPE_QUEENSPAWN"}] = crew.queenspawn;
	}
	if (crew.skeletoid > 0)
	{
		target[{&state, "AGENTTYPE_SKELETOID"}] = crew.skeletoid;
	}
	if (crew.spitter > 0)
	{
		target[{&state, "AGENTTYPE_SPITTER"}] = crew.spitter;
	}
}

} // namespace OpenApoc
