#include "framework/data.h"
#include "framework/framework.h"
#include "game/state/city/economyinfo.h"
#include "game/state/gamestate.h"
#include "game/state/rules/city/vammotype.h"
#include "library/strings_format.h"
#include "tools/extractors/common/ufo2p.h"
#include "tools/extractors/extractors.h"

namespace OpenApoc
{

void InitialGameStateExtractor::extractEconomy(GameState &state) const
{
	auto &data = this->ufo2p;
	LogInfo("Number of economy 1 data chunks: {0}", (unsigned)data.economy_data1->count());
	LogInfo("Number of economy 2 data chunks: {0}", (unsigned)data.economy_data2->count());
	LogInfo("Number of economy 3 data chunks: {0}", (unsigned)data.economy_data3->count());

	for (unsigned idx = 0; idx < data.economy_data1->count(); idx++)
	{
		int i = idx;
		auto e = data.economy_data1->get(i);

		auto economyInfo = EconomyInfo();
		economyInfo.weekAvailable = e.week;
		economyInfo.basePrice = e.basePrice;
		economyInfo.currentPrice = e.curPrice;
		economyInfo.currentStock = e.curStock;
		economyInfo.lastStock = e.lastStock;
		economyInfo.maxStock = e.maxStock;
		economyInfo.minStock = e.minStock;

		UString id = "";
		if (i < 34)
		{
			// Skip overspawn
			if (i == 33)
			{
				continue;
			}
			id = data.getVehicleId(i);
		}
		else
		{
			i -= 34;
			if (i < 49)
			{
				id = data.getVequipmentId(i);
			}
			else
			{
				LogError("Unexpected data in economy data pack 1!");
			}
		}
		state.economy[id] = economyInfo;
	}
	if (!data.craft_ammo_names || data.craft_ammo_names->count() != CRAFT_AMMO_NAME_COUNT)
	{
		LogError("craft_ammo_names count {0} expected {1}",
		         data.craft_ammo_names ? data.craft_ammo_names->count() : 0, CRAFT_AMMO_NAME_COUNT);
	}
	if (!data.craft_ammo_manufacturers ||
	    data.craft_ammo_manufacturers->count() != CRAFT_AMMO_NAME_COUNT)
	{
		LogError("craft_ammo_manufacturers count {0} expected {1}",
		         data.craft_ammo_manufacturers ? data.craft_ammo_manufacturers->count() : 0,
		         CRAFT_AMMO_NAME_COUNT);
	}
	for (unsigned idx = 0; idx < data.economy_data2->count(); idx++)
	{
		int i = idx;
		auto e = data.economy_data2->get(i);

		auto economyInfo = EconomyInfo();
		economyInfo.weekAvailable = e.week;
		economyInfo.basePrice = e.basePrice;
		economyInfo.currentPrice = e.curPrice;
		economyInfo.currentStock = e.curStock;
		economyInfo.lastStock = e.lastStock;
		economyInfo.maxStock = e.maxStock;
		economyInfo.minStock = e.minStock;

		UString id = "";
		if (i < CRAFT_AMMO_NAME_COUNT && data.craft_ammo_names)
		{
			id = data.getVAmmoId(i);
		}
		else
		{
			LogError("Unexpected data in economy data pack 2!");
		}
		state.economy[id] = economyInfo;
	}
	for (unsigned idx = 0; idx < data.economy_data3->count(); idx++)
	{
		int i = idx;
		auto e = data.economy_data3->get(i);

		auto economyInfo = EconomyInfo();
		economyInfo.weekAvailable = e.week;
		economyInfo.basePrice = e.basePrice;
		economyInfo.currentPrice = e.curPrice;
		economyInfo.currentStock = e.curStock;
		economyInfo.lastStock = e.lastStock;
		economyInfo.maxStock = e.maxStock;
		economyInfo.minStock = e.minStock;

		UString id = "";
		if (i < 87)
		{
			id = data.agent_equipment_names->get(i);
			id = format("{0}{1}", AEquipmentType::getPrefix(), canon_string(id));
		}
		else
		{
			LogError("Unexpected data in economy data pack 3!");
		}
		state.economy[id] = economyInfo;
	}
}

void InitialGameStateExtractor::applyCraftAmmoManufacturers(GameState &state) const
{
	auto &data = this->ufo2p;
	if (!data.craft_ammo_names || data.craft_ammo_names->count() != CRAFT_AMMO_NAME_COUNT ||
	    !data.craft_ammo_manufacturers ||
	    data.craft_ammo_manufacturers->count() != CRAFT_AMMO_NAME_COUNT)
	{
		return;
	}
	// Patch IDs can fold '-' to '_' (Elerium-115). Match by ammo_id or EXE name;
	// do not insert a second key.
	for (unsigned i = 0; i < CRAFT_AMMO_NAME_COUNT; i++)
	{
		const auto name = data.craft_ammo_names->get(i);
		const auto orgId = data.getOrgId(data.craft_ammo_manufacturers->get(i));
		sp<VAmmoType> ammo;
		for (auto &pair : state.vehicle_ammo)
		{
			if (pair.second && pair.second->name == name)
			{
				ammo = pair.second;
				break;
			}
		}
		if (!ammo)
		{
			LogError("No vehicle_ammo entry for craft ammo {0} ({1})", i, name);
			continue;
		}
		ammo->manufacturer = {&state, orgId};
	}
}

} // namespace OpenApoc
