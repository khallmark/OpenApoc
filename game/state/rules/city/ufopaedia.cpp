#include "game/state/rules/city/ufopaedia.h"
#include "game/state/city/research.h"
#include "game/state/gamestate.h"
#include "game/state/rules/aequipmenttype.h"
#include "game/state/rules/city/vequipmenttype.h"

namespace OpenApoc
{

UfopaediaEntry::UfopaediaEntry() : data_type(Data::Nothing) {}

static bool entryEconomyVisible(const UfopaediaEntry &entry, const GameState &state)
{
	// FUN_0008c860 @ file 0xEF1EA / 0xEF1F8 (4-build 0xEF2C8 / 0xEF2D6):
	// catalog type 2 reads DAT_00183b3b[index], type 3 reads DAT_00183b0a[index].
	switch (entry.data_type)
	{
		case UfopaediaEntry::Data::Equipment:
		{
			auto it = state.agent_equipment.find(entry.data_id);
			if (it == state.agent_equipment.end() || !it->second)
			{
				return true;
			}
			return it->second->isEconomyVisible();
		}
		case UfopaediaEntry::Data::VehicleEquipment:
		{
			auto it = state.vehicle_equipment.find(entry.data_id);
			if (it == state.vehicle_equipment.end() || !it->second)
			{
				return true;
			}
			return it->second->isEconomyVisible(state);
		}
		default:
			return true;
	}
}

bool UfopaediaEntry::isVisible(const GameState &state) const
{
	// FUN_000abf50 / FUN_0008903c @ file 0xFE5F4 / 0xDB6E0: catalog start
	// byte. FUN_000abf9c writes it on research match. FUN_0008c860 still
	// applies type 2/3 hide RAM after the start byte is set.
	bool revealed = this->startVisible;
	if (!revealed)
	{
		if (this->startVisibleFromExe && this->catalogIndex != 0xFFFF)
		{
			return false;
		}
		if (!this->dependency.satisfied())
		{
			return false;
		}
	}
	return entryEconomyVisible(*this, state);
}

template <>
sp<UfopaediaEntry> StateObject<UfopaediaEntry>::get(const GameState &state, const UString &id)
{
	auto it = state.ufopaedia_entries.find(id);
	if (it == state.ufopaedia_entries.end())
	{
		LogError("No UFOPaedia entry matching ID \"{0}\"", id);
		return nullptr;
	}
	return it->second;
}

template <> const UString &StateObject<UfopaediaEntry>::getPrefix()
{
	static UString prefix = "PAEDIAENTRY_";
	return prefix;
}
template <> const UString &StateObject<UfopaediaEntry>::getTypeName()
{
	static UString name = "UfopaediaEntry";
	return name;
}

} // namespace OpenApoc
