#include "game/state/rules/city/ufogrowth.h"
#include "game/state/city/building.h"
#include "game/state/gamestate.h"

namespace OpenApoc
{

template <> sp<UFOGrowth> StateObject<UFOGrowth>::get(const GameState &state, const UString &id)
{
	auto it = state.ufo_growth_lists.find(id);
	if (it == state.ufo_growth_lists.end())
	{
		LogError("No ufo growth matching ID \"{0}\"", id);
		return nullptr;
	}
	return it->second;
}

template <> const UString &StateObject<UFOGrowth>::getPrefix()
{
	static UString prefix = "UFO_GROWTH_";
	return prefix;
}
template <> const UString &StateObject<UFOGrowth>::getTypeName()
{
	static UString name = "UFOGrowth";
	return name;
}

sp<UFOGrowth> UFOGrowth::selectForWeek(const GameState &state, int week)
{
	sp<UFOGrowth> fallback;
	for (auto &entry : state.ufo_growth_lists)
	{
		if (!entry.second || entry.first == "UFO_GROWTH_LIMIT")
		{
			continue;
		}
		if (entry.second->week == week)
		{
			return entry.second;
		}
		if (entry.first == "UFO_GROWTH_DEFAULT")
		{
			fallback = entry.second;
		}
	}
	return fallback;
}

bool UFOGrowth::craftFactoryIntact(const GameState &state)
{
	bool sawFactory = false;
	for (const auto &b : state.buildings)
	{
		if (!b.second || b.second->city.id != "CITYMAP_ALIEN")
		{
			continue;
		}
		if (b.second->function.id != "BUILDINGFUNCTION_ORGANIC_FACTORY")
		{
			continue;
		}
		sawFactory = true;
		if (b.second->isAlive())
		{
			return true;
		}
	}
	return !sawFactory;
}
}; // namespace OpenApoc
