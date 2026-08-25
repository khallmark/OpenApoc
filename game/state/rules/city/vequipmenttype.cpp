#include "game/state/rules/city/vequipmenttype.h"
#include "game/state/gamestate.h"
#include "game/state/tilemap/tilemap.h"

namespace OpenApoc
{

template <> const UString &StateObject<VEquipmentType>::getPrefix()
{
	static UString prefix = "VEQUIPMENTTYPE_";
	return prefix;
}

template <> const UString &StateObject<VEquipmentType>::getTypeName()
{
	static UString name = "VEquipmentType";
	return name;
}

template <>
sp<VEquipmentType> StateObject<VEquipmentType>::get(const GameState &state, const UString &id)
{
	auto it = state.vehicle_equipment.find(id);
	if (it == state.vehicle_equipment.end())
	{
		LogError("No vequipement type matching ID \"{0}\"", id);
		return nullptr;
	}
	return it->second;
}

// note: the range value in vanilla game files is given in half-metres
int VEquipmentType::getRangeInTiles() const { return range / 2 / (int)VELOCITY_SCALE_CITY.x; }
int VEquipmentType::getRangeInMetres() const { return range / 2; }

int VEquipmentType::scoreRequirementFor(int difficulty) const
{
	if (difficulty >= 0 && difficulty < (int)scoreRequirementByDifficulty.size())
	{
		return scoreRequirementByDifficulty[difficulty];
	}
	return scoreRequirement;
}

void VEquipmentType::clearEconomyHide()
{
	// FUN_000ab440 case 0 @ file 0x10DC0D: MOV [itemIndex+0xB3B0A], 0
	economyUnhidden = true;
}

bool VEquipmentType::isEconomyVisible(const GameState &state) const
{
	auto it = state.economy.find(id);
	if (it == state.economy.end())
	{
		return false;
	}
	const auto &economy = it->second;
	if (economy.weekAvailable == 0 && !economyUnhidden)
	{
		return false;
	}
	if (economy.weekAvailable > static_cast<int>(state.gameTime.getWeek()))
	{
		return false;
	}
	return research_dependency.satisfied();
}

} // namespace OpenApoc
