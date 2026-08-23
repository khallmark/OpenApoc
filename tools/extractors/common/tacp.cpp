#include "tacp.h"
#include "framework/data.h"
#include "framework/framework.h"
#include "tools/extractors/common/exe_slide.h"

#include <boost/crc.hpp>
#include <iomanip>
#include <iterator>

namespace OpenApoc
{

TACP::TACP(std::string file_name)
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
	if (!tacpFileSlide(crc32, slide))
	{
		LogError("File \"{0}\" has unknown crc32 {1:08x} (known non-4 {2:08x}, 4-build {3:08x})",
		         file_name.c_str(), crc32, TACP_CRC_NON4, TACP_CRC_4);
	}
	const auto at = [slide](off_t off) -> off_t { return off + slide; };

	file.seekg(0, std::ios::beg);
	file.clear();

	// hand-filling damage mod names as they are not present in the game exe
	{
		auto vec = std::vector<std::string>();
		vec.emplace_back("Human");
		vec.emplace_back("Mutant");
		vec.emplace_back("Android");
		vec.emplace_back("Alien Egg");
		vec.emplace_back("Multiworm");
		vec.emplace_back("Hyperworm");
		vec.emplace_back("Chrysalis");
		vec.emplace_back("Brainsucker");
		vec.emplace_back("Queenspawn");
		vec.emplace_back("Anthropod");
		vec.emplace_back("Psimorph");
		vec.emplace_back("Spitter");
		vec.emplace_back("Megaspawn");
		vec.emplace_back("Popper");
		vec.emplace_back("Skeletoid");
		vec.emplace_back("Micronoid Aggregate");
		vec.emplace_back("Disruptor Shield");
		vec.emplace_back("Megapol Armor");
		vec.emplace_back("Marsec Armor");
		vec.emplace_back("X-COM Disruptor Armor");
		vec.emplace_back("Terrain 1?");
		vec.emplace_back("Terrain 2?");
		vec.emplace_back("Gun Emplacement");
		this->damage_modifier_names.reset(new StrTab(vec));
	}

	this->damage_type_names.reset(new StrTab(file, at(DAMAGE_TYPE_NAMES_OFFSET_START),
	                                          at(DAMAGE_TYPE_NAMES_OFFSET_END), true));

	this->damage_types.reset(new DataChunk<DamageTypeData>(file, at(DAMAGE_TYPE_DATA_OFFSET_START),
	                                                       at(DAMAGE_TYPE_DATA_OFFSET_END)));

	this->damage_modifiers.reset(new DataChunk<DamageModifierData>(
	    file, at(DAMAGE_MODIFIER_DATA_OFFSET_START), at(DAMAGE_MODIFIER_DATA_OFFSET_END)));

	this->agent_equipment.reset(new DataChunk<AgentEquipmentData>(
	    file, at(AGENT_EQUIPMENT_DATA_OFFSET_START), at(AGENT_EQUIPMENT_DATA_OFFSET_END)));

	this->agent_armor.reset(new DataChunk<AgentArmorData>(file, at(AGENT_ARMOR_DATA_OFFSET_START),
	                                                      at(AGENT_ARMOR_DATA_OFFSET_END)));

	this->agent_weapons.reset(new DataChunk<AgentWeaponData>(file, at(AGENT_WEAPON_DATA_OFFSET_START),
	                                                         at(AGENT_WEAPON_DATA_OFFSET_END)));

	this->agent_general.reset(new DataChunk<AgentGeneralData>(file, at(AGENT_GENERAL_DATA_OFFSET_START),
	                                                          at(AGENT_GENERAL_DATA_OFFSET_END)));

	this->agent_payload.reset(new DataChunk<AgentPayloadData>(file, at(AGENT_PAYLOAD_DATA_OFFSET_START),
	                                                          at(AGENT_PAYLOAD_DATA_OFFSET_END)));

	this->agent_equipment_set_built_in.reset(new DataChunk<AgentEquipmentSetBuiltInData>(
	    file, at(AGENT_EQUIPMENT_SET_BUILTIN_DATA_OFFSET_START),
	    at(AGENT_EQUIPMENT_SET_BUILTIN_DATA_OFFSET_END)));

	this->agent_equipment_set_score_alien.reset(new DataChunk<AgentEquipmentSetScoreDataAlien>(
	    file, at(AGENT_EQUIPMENT_SET_SCORE_ALIEN_DATA_OFFSET_START),
	    at(AGENT_EQUIPMENT_SET_SCORE_ALIEN_DATA_OFFSET_END)));

	this->agent_equipment_set_score_human.reset(new DataChunk<AgentEquipmentSetScoreDataHuman>(
	    file, at(AGENT_EQUIPMENT_SET_SCORE_HUMAN_DATA_OFFSET_START),
	    at(AGENT_EQUIPMENT_SET_SCORE_HUMAN_DATA_OFFSET_END)));

	this->agent_equipment_set_score_requirement.reset(
	    new DataChunk<AgentEquipmentSetScoreRequirement>(
	        file, at(AGENT_EQUIPMENT_SET_SCORE_REQUIREMENT_DATA_OFFSET_START),
	        at(AGENT_EQUIPMENT_SET_SCORE_REQUIREMENT_DATA_OFFSET_END)));

	this->bullet_sprites.reset(new DataChunk<BulletSprite>(
	    file, at(BULLETSPRITE_DATA_TACP_OFFSET_START), at(BULLETSPRITE_DATA_TACP_OFFSET_END)));
	this->projectile_sprites.reset(new DataChunk<ProjectileSprites>(
	    file, at(PROJECTILESPRITES_DATA_TACP_OFFSET_START),
	    at(PROJECTILESPRITES_DATA_TACP_OFFSET_END)));
}

} // namespace OpenApoc
