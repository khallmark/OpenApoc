#include "framework/configfile.h"
#include "framework/framework.h"
#include "framework/logger.h"
#include "game/state/city/building.h"
#include "game/state/gamestate.h"
#include "game/state/rules/city/ufogrowth.h"
#include "library/strings.h"
#include <iostream>

using namespace OpenApoc;

// Look up the requested count for vehicle type `typeId` within a growth table's list.
static int growthCount(const UFOGrowth &growth, const UString &typeId)
{
	for (auto &entry : growth.vehicleTypeList)
	{
		if (entry.first == typeId)
		{
			return entry.second;
		}
	}
	return 0;
}

// Positive growth case: UFOGrowth::selectForWeek must consume the weekly UFO_GROWTH_<week>
// table for a week that has one, and fall back to UFO_GROWTH_DEFAULT for a week that doesn't.
static bool test_select_for_week_consumes_growth_table(sp<GameState> state)
{
	LogInfo("Testing UFOGrowth::selectForWeek...");

	auto week1 = UFOGrowth::selectForWeek(*state, 1);
	if (!week1 || week1->week != 1)
	{
		LogError("selectForWeek(1) did not return the UFO_GROWTH_1 table");
		return false;
	}
	if (growthCount(*week1, "VEHICLETYPE_ALIEN_PROBE") != 9 ||
	    growthCount(*week1, "VEHICLETYPE_ALIEN_SCOUT") != 9)
	{
		LogError("selectForWeek(1) table contents do not match the extracted UFO_GROWTH_1 data");
		return false;
	}

	// A week with no dedicated UFO_GROWTH_<week> entry must fall back to DEFAULT.
	auto fallback = UFOGrowth::selectForWeek(*state, 9999);
	if (!fallback || fallback->week != 0)
	{
		LogError("selectForWeek(9999) did not fall back to UFO_GROWTH_DEFAULT");
		return false;
	}
	if (growthCount(*fallback, "VEHICLETYPE_ALIEN_MOTHERSHIP") != 1)
	{
		LogError("selectForWeek(9999) fallback table contents do not match UFO_GROWTH_DEFAULT");
		return false;
	}

	LogInfo("UFOGrowth::selectForWeek test passed");
	return true;
}

// Blocked-growth case: UFOGrowth::craftFactoryIntact must gate on a living
// BUILDINGFUNCTION_ORGANIC_FACTORY in CITYMAP_ALIEN, flipping to false once that building is
// destroyed and back to true once it is restored.
static bool test_craft_factory_intact_gates_growth(sp<GameState> state)
{
	LogInfo("Testing UFOGrowth::craftFactoryIntact...");

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
		return false;
	}
	if (!factory->isAlive())
	{
		LogError("Organic factory should start alive in a freshly loaded gamestate");
		return false;
	}
	if (!UFOGrowth::craftFactoryIntact(*state))
	{
		LogError("craftFactoryIntact() should be true while the organic factory lives");
		return false;
	}

	// Destroy the factory (no building parts left standing) and confirm the gate flips.
	auto savedParts = factory->buildingParts;
	factory->buildingParts.clear();
	if (factory->isAlive())
	{
		LogError("Clearing buildingParts should have left the organic factory dead");
		return false;
	}
	if (UFOGrowth::craftFactoryIntact(*state))
	{
		LogError("craftFactoryIntact() should be false once the organic factory is destroyed");
		return false;
	}

	// Restore the factory so any later re-use of this state sees the original layout.
	factory->buildingParts = savedParts;
	if (!UFOGrowth::craftFactoryIntact(*state))
	{
		LogError("craftFactoryIntact() should be true again once the organic factory is restored");
		return false;
	}

	LogInfo("UFOGrowth::craftFactoryIntact test passed");
	return true;
}

int main(int argc, char **argv)
{
	OpenApoc::config().addPositionalArgument("common", "Common gamestate to load");
	OpenApoc::config().addPositionalArgument("gamestate", "Gamestate to load");

	if (OpenApoc::config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	auto gamestate_name = OpenApoc::config().getString("gamestate");
	if (gamestate_name.empty())
	{
		std::cerr << "Must provide gamestate\n";
		OpenApoc::config().showHelp();
		return EXIT_FAILURE;
	}
	auto common_name = OpenApoc::config().getString("common");
	if (common_name.empty())
	{
		std::cerr << "Must provide common gamestate\n";
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

	if (!test_select_for_week_consumes_growth_table(state))
	{
		LogError("selectForWeek test failed");
		return EXIT_FAILURE;
	}

	if (!test_craft_factory_intact_gates_growth(state))
	{
		LogError("craftFactoryIntact test failed");
		return EXIT_FAILURE;
	}

	LogInfo("test_ufo_growth success - all tests passed");
	return EXIT_SUCCESS;
}
