#include "framework/logger.h"
#include "game/state/city/research.h"
#include "game/state/gamestate.h"
#include "game/state/rules/aequipmenttype.h"
#include "game/state/rules/city/vehicletype.h"
#include "game/state/rules/city/vequipmenttype.h"
#include "tools/extractors/common/ufo2p.h"
#include "tools/extractors/extractors.h"

namespace OpenApoc
{

void InitialGameStateExtractor::extractManufacturing(GameState &state) const
{
	auto &data = this->ufo2p;
	if (!data.manufacturing_data || !data.manufacturing_names)
	{
		LogError("manufacturing_data table missing");
		return;
	}

	for (unsigned i = 0; i < data.manufacturing_data->count(); i++)
	{
		const auto mdata = data.manufacturing_data->get(i);
		if (mdata.manufacturable == 0)
		{
			continue;
		}
		if (i >= data.manufacturing_names->count())
		{
			LogError("manufacturing name missing for record {0}", i);
			continue;
		}

		auto r = mksp<ResearchTopic>();
		r->name = data.manufacturing_names->get(i);
		const auto id = UString("MANUFACTURE_") + canon_string(r->name);
		r->type = ResearchTopic::Type::Engineering;
		r->man_hours = mdata.skillHours;
		r->cost = static_cast<int>(mdata.manufacturingCost);
		switch (mdata.labSize)
		{
			case 0:
				r->required_lab_size = ResearchTopic::LabSize::Small;
				break;
			case 1:
				r->required_lab_size = ResearchTopic::LabSize::Large;
				break;
			default:
				LogError("Unexpected labSize {0:02x} for manufacture {1}", (unsigned)mdata.labSize,
				         id);
				continue;
		}
		switch (mdata.itemType)
		{
			case 0:
				r->item_type = ResearchTopic::ItemType::VehicleEquipment;
				r->itemId = data.getVequipmentId(mdata.itemIndex);
				break;
			case 1:
				r->item_type = ResearchTopic::ItemType::AgentEquipment;
				if (mdata.itemIndex >= data.agent_equipment_names->count())
				{
					LogError("Agent equipment index {0} out of range for {1}", mdata.itemIndex, id);
					continue;
				}
				r->itemId = AEquipmentType::getPrefix() +
				            canon_string(data.agent_equipment_names->get(mdata.itemIndex));
				break;
			case 2:
				r->item_type = ResearchTopic::ItemType::VehicleEquipmentAmmo;
				if (!data.craft_ammo_names || mdata.itemIndex >= data.craft_ammo_names->count())
				{
					LogError("Craft ammo index {0} out of range for {1}", mdata.itemIndex, id);
					continue;
				}
				r->itemId = data.getVAmmoId(mdata.itemIndex);
				break;
			case 3:
				r->item_type = ResearchTopic::ItemType::Craft;
				r->itemId = data.getVehicleId(mdata.itemIndex);
				break;
			default:
				LogError("Unexpected itemType {0:02x} for manufacture {1}",
				         (unsigned)mdata.itemType, id);
				continue;
		}

		if (mdata.techRequired != 0xffff && mdata.techRequired < data.research_names->count())
		{
			ResearchDependency dependency;
			dependency.type = ResearchDependency::Type::All;
			const auto prereqId = ResearchTopic::getPrefix() +
			                      canon_string(data.research_names->get(mdata.techRequired));
			dependency.topics.emplace(StateRef<ResearchTopic>{&state, prereqId});
			r->dependencies.research.push_back(dependency);
		}

		if (state.research.topics.find(id) != state.research.topics.end())
		{
			LogError("Multiple manufacture topics with ID \"{0}\"", id);
		}
		state.research.topics[id] = r;
	}
}

} // namespace OpenApoc
