#include "framework/logger.h"
#include "game/state/gamestate.h"
#include "game/state/rules/city/ufogrowth.h"
#include "game/state/rules/city/vehicletype.h"
#include "library/strings_format.h"
#include "tools/extractors/common/ufo2p.h"
#include "tools/extractors/extractors.h"

namespace OpenApoc
{

void InitialGameStateExtractor::extractUfoGrowth(GameState &state) const
{
	auto &data = this->ufo2p;
	if (!data.ufo_growth_rates || data.ufo_growth_rates->count() < 1)
	{
		LogError("UFO_growth_rates table missing");
		return;
	}
	const auto table = data.ufo_growth_rates->get(0);

	// Do not keep a copy under common_patch/gamestate/ — loadGame appends vectors.
	state.ufo_growth_lists.clear();

	auto fill = [&](const UString &id, int week, const uint16_t *counts, int countSlots,
	                int mothershipExtra)
	{
		auto growth = mksp<UFOGrowth>();
		growth->week = week;
		for (int i = 0; i < countSlots; i++)
		{
			if (counts[i] == 0)
			{
				continue;
			}
			growth->vehicleTypeList.emplace_back(data.getVehicleId(i), counts[i]);
		}
		if (mothershipExtra > 0)
		{
			growth->vehicleTypeList.emplace_back(data.getVehicleId(UFO_GROWTH_CRAFT_COUNT - 1),
			                                     mothershipExtra);
		}
		state.ufo_growth_lists[id] = growth;
	};

	fill("UFO_GROWTH_LIMIT", 0, table.fleet_caps, UFO_GROWTH_CRAFT_COUNT, 0);
	for (int week = 0; week < UFO_GROWTH_WEEK_COUNT; week++)
	{
		fill(format("UFO_GROWTH_{0}", week + 1), week + 1, table.weekly_spawn[week],
		     UFO_GROWTH_CRAFT_COUNT, 0);
	}
	fill("UFO_GROWTH_DEFAULT", 0, table.default_spawn, UFO_GROWTH_CRAFT_COUNT - 1,
	     table.default_mothership);
}

} // namespace OpenApoc
