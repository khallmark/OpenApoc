#include "framework/configfile.h"
#include "framework/framework.h"
#include "framework/logger.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/vehicle.h"
#include "game/state/gamestate.h"
#include "game/state/rules/city/ufogrowth.h"
#include "game/state/shared/organisation.h"
#include "library/strings.h"
#include <iostream>

using namespace OpenApoc;

// GameState::updateUfoGrowth() is supposed to call UFOGrowth::craftFactoryIntact() and skip
// spawning weekly UFO growth once the alien organic factory is destroyed. UFOGrowth's own tests
// only exercise selectForWeek()/craftFactoryIntact() as free functions and never call
// updateUfoGrowth() itself, so they cannot catch a regression where the gate is declared but not
// actually wired into the caller. This test locks that wiring directly.

static int countAlienVehicles(const GameState &state, const StateRef<Organisation> &alienOrg,
                              const StateRef<City> &alienCity)
{
	int count = 0;
	for (auto &vp : state.vehicles)
	{
		if (vp.second && vp.second->owner == alienOrg && vp.second->city == alienCity)
		{
			count++;
		}
	}
	return count;
}

int main(int argc, char **argv)
{
	OpenApoc::config().addPositionalArgument("common", "Common gamestate to load");
	OpenApoc::config().addPositionalArgument("gamestate", "Gamestate to load");

	if (OpenApoc::config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	auto common_name = OpenApoc::config().getString("common");
	if (common_name.empty())
	{
		std::cerr << "Must provide common gamestate\n";
		OpenApoc::config().showHelp();
		return EXIT_FAILURE;
	}
	auto gamestate_name = OpenApoc::config().getString("gamestate");
	if (gamestate_name.empty())
	{
		std::cerr << "Must provide gamestate\n";
		OpenApoc::config().showHelp();
		return EXIT_FAILURE;
	}

	OpenApoc::Framework fw("OpenApoc", false);

	LogInfo("Loading common gamestate \"{0}\"", common_name);
	auto state = OpenApoc::mksp<OpenApoc::GameState>();
	if (!state->loadGame(common_name))
	{
		LogError("Failed to load common gamestate");
		return EXIT_FAILURE;
	}

	LogInfo("Loading gamestate \"{0}\"", gamestate_name);
	if (!state->loadGame(gamestate_name))
	{
		LogError("Failed to load supplied gamestate");
		return EXIT_FAILURE;
	}

	// initState() links each building's buildingParts from city scenery, which
	// Building::isAlive() (and therefore UFOGrowth::craftFactoryIntact()) depends on.
	state->startGame();
	state->initState();

	Building *factory = nullptr;
	for (auto &b : state->buildings)
	{
		if (b.second && b.second->city.id == "CITYMAP_ALIEN" &&
		    b.second->function.id == "BUILDINGFUNCTION_ORGANIC_FACTORY")
		{
			factory = b.second.get();
			break;
		}
	}
	if (!factory)
	{
		LogError("BUILDINGFUNCTION_ORGANIC_FACTORY not found in CITYMAP_ALIEN");
		return EXIT_FAILURE;
	}
	// Whether a freshly loaded factory starts alive, and whether clearing buildingParts kills
	// it, is Building::isAlive() behaviour -- that's PR 36's/ufogrowth's territory and is
	// already covered by test_ufo_growth.cpp. This test only asserts the wiring: does
	// updateUfoGrowth() actually consult the gate.

	const int week = static_cast<int>(state->gameTime.getWeek());
	auto growth = UFOGrowth::selectForWeek(*state, week);
	if (!growth)
	{
		LogError("No UFO growth table available for week {0}", week);
		return EXIT_FAILURE;
	}
	auto limitIt = state->ufo_growth_lists.find("UFO_GROWTH_LIMIT");
	if (limitIt == state->ufo_growth_lists.end() || !limitIt->second)
	{
		LogError("UFO_GROWTH_LIMIT not found");
		return EXIT_FAILURE;
	}
	if (state->vehicle_types.find("VEHICLETYPE_ALIEN_PROBE") == state->vehicle_types.end())
	{
		LogError("VEHICLETYPE_ALIEN_PROBE not found in vehicle_types");
		return EXIT_FAILURE;
	}

	// Guarantee a positive spawn allowance across BOTH calls below regardless of what the
	// extracted save already has in flight. The requested-per-call amount (4) stays small and
	// fixed (the growth table is static data, re-read every call), while the cap headroom (40)
	// is deliberately much larger so the first (alive) call cannot exhaust the allowance and
	// starve the second (destroyed) call of anything to spawn if the gate were not wired in.
	// Both vectors are restored below so this state is left exactly as it was found.
	const UString probeType = "VEHICLETYPE_ALIEN_PROBE";
	const auto savedGrowthList = growth->vehicleTypeList;
	const auto savedLimitList = limitIt->second->vehicleTypeList;
	growth->vehicleTypeList.emplace_back(probeType, 4);
	limitIt->second->vehicleTypeList.emplace_back(probeType, 40);

	StateRef<City> alienCity = {state.get(), "CITYMAP_ALIEN"};
	StateRef<Organisation> alienOrg = {state.get(), "ORG_ALIEN"};
	if (!alienCity || !alienOrg)
	{
		LogError("CITYMAP_ALIEN or ORG_ALIEN not found");
		return EXIT_FAILURE;
	}

	// Control: with the factory alive, updateUfoGrowth() must actually be able to spawn
	// vehicles. If this does not hold, the harness cannot observe growth at all, and the
	// "destroyed -> no growth" assertion below would pass for the wrong reason.
	const int beforeAlive = countAlienVehicles(*state, alienOrg, alienCity);
	state->updateUfoGrowth();
	const int afterAlive = countAlienVehicles(*state, alienOrg, alienCity);
	if (afterAlive <= beforeAlive)
	{
		LogError("updateUfoGrowth() spawned no vehicles with the organic factory intact -- "
		         "cannot validate the gate without an observable growth baseline");
		return EXIT_FAILURE;
	}

	// Destroy the factory: this is the behaviour under test. If updateUfoGrowth() does not
	// actually call UFOGrowth::craftFactoryIntact(), growth continues here.
	auto savedParts = factory->buildingParts;
	factory->buildingParts.clear();

	const int beforeDestroyed = countAlienVehicles(*state, alienOrg, alienCity);
	state->updateUfoGrowth();
	const int afterDestroyed = countAlienVehicles(*state, alienOrg, alienCity);

	// Restore the factory and the growth/limit tables so any later re-use of this state sees
	// the original layout and data, matching test_ufo_growth.cpp's own restoration discipline.
	factory->buildingParts = savedParts;
	growth->vehicleTypeList = savedGrowthList;
	limitIt->second->vehicleTypeList = savedLimitList;

	if (afterDestroyed != beforeDestroyed)
	{
		LogError("updateUfoGrowth() spawned {0} vehicle(s) despite the organic factory being "
		         "destroyed -- the craftFactoryIntact() gate is not wired into updateUfoGrowth()",
		         afterDestroyed - beforeDestroyed);
		return EXIT_FAILURE;
	}

	LogInfo("test_gamestate_ufo_growth_wiring success - all tests passed");
	return EXIT_SUCCESS;
}
