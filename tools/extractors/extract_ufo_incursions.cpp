#include "framework/logger.h"
#include "game/state/gamestate.h"
#include "game/state/rules/city/ufoincursion.h"
#include "library/strings_format.h"
#include "tools/extractors/common/ufo2p.h"
#include "tools/extractors/extractors.h"

namespace OpenApoc
{

void InitialGameStateExtractor::extractUfoIncursions(GameState &state) const
{
	auto &data = this->ufo2p;
	if (!data.ufo_mission_data || data.ufo_mission_data->count() != UFO_MISSION_RECORD_COUNT)
	{
		LogError("UFO_mission_data count {0} expected {1}",
		         data.ufo_mission_data ? data.ufo_mission_data->count() : 0,
		         UFO_MISSION_RECORD_COUNT);
		return;
	}

	// Do not keep a copy under common_patch/gamestate/ — loadGame appends vectors.
	state.ufo_incursions.clear();

	for (unsigned i = 0; i < data.ufo_mission_data->count(); i++)
	{
		const auto rec = data.ufo_mission_data->get(i);
		UFOIncursion::PrimaryMission primary = UFOIncursion::PrimaryMission::Infiltration;
		UString prefix = "UFO_INCURSION_I";
		int indexInType = static_cast<int>(i) + 1;
		if (i < 20)
		{
			primary = UFOIncursion::PrimaryMission::Infiltration;
			prefix = "UFO_INCURSION_I";
			indexInType = static_cast<int>(i) + 1;
		}
		else if (i < 30)
		{
			primary = UFOIncursion::PrimaryMission::Attack;
			prefix = "UFO_INCURSION_A";
			indexInType = static_cast<int>(i) - 19;
		}
		else if (i < 40)
		{
			primary = UFOIncursion::PrimaryMission::Subversion;
			prefix = "UFO_INCURSION_S";
			indexInType = static_cast<int>(i) - 29;
		}
		else
		{
			primary = UFOIncursion::PrimaryMission::Overspawn;
			prefix = "UFO_INCURSION_O";
			indexInType = static_cast<int>(i) - 39;
		}

		auto inc = mksp<UFOIncursion>();
		inc->primaryMission = primary;
		inc->priority = indexInType;
		for (int slot = 0; slot < UFO_MISSION_SLOT_COUNT; slot++)
		{
			if (rec.craft[slot] == 0xffff || rec.count[slot] == 0)
			{
				continue;
			}
			if (rec.craft[slot] >= 10)
			{
				LogError("UFO_mission_data[{0}] slot {1} craft {2} out of Appendix C", i, slot,
				         rec.craft[slot]);
				continue;
			}
			const auto typeId = data.getVehicleId(rec.craft[slot]);
			if (rec.role[slot] == UFO_MISSION_ROLE_ESCORT)
			{
				inc->escortList.emplace_back(typeId, rec.count[slot]);
			}
			else if (rec.role[slot] == UFO_MISSION_ROLE_ATTACK &&
			         primary != UFOIncursion::PrimaryMission::Attack)
			{
				inc->attackList.emplace_back(typeId, rec.count[slot]);
			}
			else
			{
				inc->primaryList.emplace_back(typeId, rec.count[slot]);
			}
		}
		const auto id = format("{0}{1}", prefix, indexInType);
		if (state.ufo_incursions.find(id) != state.ufo_incursions.end())
		{
			LogError("Multiple UFO incursions with ID \"{0}\"", id);
		}
		state.ufo_incursions[id] = inc;
	}
}

} // namespace OpenApoc
