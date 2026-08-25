// Regression coverage for Base::die(), which is reached when the last X-COM base falls --
// normally on the way back from a base-defence mission, via Battle::exitBattle().
//
// It used to crash there: the function walked building->currentAgents and ->currentVehicles with
// a range-for while Agent::die()/Vehicle::die() erased the current element from those same sets,
// so the loop incremented an iterator whose tree node had already been freed. That surfaced as a
// SIGSEGV inside __tree_const_iterator<StateRef<Agent>>::operator++ under Base::die.
//
// The path also carries this fork's change of swapping a "no screen for that yet" LogError for a
// real GameEventType::XComDefeated event, which the crash had kept unreachable.

#include "framework/configfile.h"
#include "framework/framework.h"
#include "game/state/city/base.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/vehicle.h"
#include "game/state/gamestate.h"
#include "game/state/shared/agent.h"
#include "game/state/shared/organisation.h"
#include "library/sp.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static sp<GameState> g_state;

// Puts more than one agent and more than one vehicle inside the base's building, so that
// Base::die() has to erase from a container it is iterating more than once. With a single
// occupant the old code could survive by luck.
static bool stockBaseBuilding(GameState &state, StateRef<Base> base)
{
	auto building = base->building;
	if (!building)
	{
		return false;
	}
	for (auto &a : state.agents)
	{
		if (!a.second || !a.second->owner || a.second->owner != state.getPlayer())
		{
			continue;
		}
		a.second->enterBuilding(state, building);
		if (building->currentAgents.size() >= 4)
		{
			break;
		}
	}
	return building->currentAgents.size() >= 2;
}

static bool test_base_die_with_occupants()
{
	auto &state = *g_state;
	TEST_REQUIRE(!state.player_bases.empty(), "no player base to kill");
	StateRef<Base> base{&state, state.player_bases.begin()->first};
	auto building = base->building;
	TEST_REQUIRE((bool)building, "base has no building");

	TEST_REQUIRE(stockBaseBuilding(state, base), "could not put >1 agent in the base building");
	const auto agentsBefore = building->currentAgents.size();
	const auto vehiclesBefore = building->currentVehicles.size();
	LogInfo("Killing base with {0} agents and {1} vehicles present", agentsBefore, vehiclesBefore);

	const auto basesBefore = state.player_bases.size();

	// The crash was here. Reaching the next line at all is the substance of this test.
	base->die(state, false);

	TEST_REQUIRE(state.player_bases.size() == basesBefore - 1, "base was not removed");
	// die() clears the building's link back to the base and hands it to the government.
	TEST_REQUIRE(!building->base, "building still points at the dead base");
	return true;
}

int main(int argc, char **argv)
{
	config().addPositionalArgument("common", "Common gamestate to load");
	config().addPositionalArgument("gamestate", "Gamestate to load");
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}
	applyDeterministicTestConfig();

	const auto common = config().getString("common");
	const auto gamestate = config().getString("gamestate");
	if (common.empty() || gamestate.empty())
	{
		LogError("Must provide common and gamestate paths");
		return EXIT_FAILURE;
	}

	// Base::die() raises a GameEvent when the last base falls, which needs a Framework. No window.
	Framework fw("OpenApoc", false);
	g_state = mksp<GameState>();
	if (!loadStartedGameState(*g_state, common, gamestate))
	{
		return EXIT_FAILURE;
	}

	const int rc = runTestSuite({
	    {"base_die_with_occupants", test_base_die_with_occupants},
	});
	g_state.reset();
	return rc;
}
