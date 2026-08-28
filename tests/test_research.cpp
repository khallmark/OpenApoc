#include "framework/configfile.h"
#include "framework/filesystem.h"
#include "framework/framework.h"
#include "framework/logger.h"
#include "game/state/city/base.h"
#include "game/state/city/research.h"
#include "game/state/gamestate.h"
#include "game/state/rules/aequipmenttype.h"
#include "game/state/rules/city/vequipmenttype.h"
#include <iostream>

// Regression coverage for ItemDependency::Type (Any/All).
//
// Historically ItemDependency::satisfied() only ever implemented "require every
// listed item" (All) semantics. develop's extractor work (tools/extractors/
// extract_research.cpp, out of scope for this unit) reads UFO2P's real
// unknown1 field and can tag a dependency as "Any" (require at least one of
// the listed items) instead. Without the Type member and the Any branch in
// satisfied(), any such data-driven "Any" dependency would silently behave as
// "All" - i.e. it would be far too strict, and correctly-satisfiable research
// could stay permanently locked.
//
// This test builds ItemDependency instances directly (no dependency on the
// extractor, which does not yet populate the field) and proves both branches
// of satisfied() using a real player base's inventory maps.

using namespace OpenApoc;

namespace
{

bool pickAgentEquipment(sp<GameState> state, StateRef<AEquipmentType> &out)
{
	for (auto &pair : state->agent_equipment)
	{
		if (pair.second && !pair.second->bioStorage)
		{
			out = {state.get(), pair.first};
			return true;
		}
	}
	return false;
}

bool pickVehicleEquipment(sp<GameState> state, StateRef<VEquipmentType> &out)
{
	for (auto &pair : state->vehicle_equipment)
	{
		if (pair.second)
		{
			out = {state.get(), pair.first};
			return true;
		}
	}
	return false;
}

// Comfortably larger than any plausible max_ammo multiplier, so "held" is
// unambiguous regardless of the item's mult factor.
constexpr unsigned HELD_COUNT = 100000;

bool test_item_dependency_all(sp<GameState> state, StateRef<Base> base,
                              StateRef<AEquipmentType> agentType,
                              StateRef<VEquipmentType> vehicleType)
{
	base->inventoryAgentEquipment[agentType.id] = 0;
	base->inventoryVehicleEquipment[vehicleType.id] = 0;

	ItemDependency dep;
	dep.type = ItemDependency::Type::All;
	dep.agentItemsRequired[agentType] = 1;
	dep.vehicleItemsRequired[vehicleType] = 1;

	if (dep.satisfied(base))
	{
		LogError("ItemDependency::Type::All must not be satisfied with nothing held");
		return false;
	}

	base->inventoryAgentEquipment[agentType.id] = HELD_COUNT;
	if (dep.satisfied(base))
	{
		LogError("ItemDependency::Type::All must not be satisfied with only the agent item held");
		return false;
	}

	base->inventoryVehicleEquipment[vehicleType.id] = HELD_COUNT;
	if (!dep.satisfied(base))
	{
		LogError("ItemDependency::Type::All must be satisfied once every item is held");
		return false;
	}

	return true;
}

bool test_item_dependency_any(sp<GameState> state, StateRef<Base> base,
                              StateRef<AEquipmentType> agentType,
                              StateRef<VEquipmentType> vehicleType)
{
	base->inventoryAgentEquipment[agentType.id] = 0;
	base->inventoryVehicleEquipment[vehicleType.id] = 0;

	ItemDependency dep;
	dep.type = ItemDependency::Type::Any;
	dep.agentItemsRequired[agentType] = 1;
	dep.vehicleItemsRequired[vehicleType] = 1;

	if (dep.satisfied(base))
	{
		LogError("ItemDependency::Type::Any must not be satisfied with nothing held");
		return false;
	}

	// This is the behaviour the old (pre-Type) satisfied() got wrong: holding
	// just the agent item must already satisfy an Any dependency.
	base->inventoryAgentEquipment[agentType.id] = HELD_COUNT;
	if (!dep.satisfied(base))
	{
		LogError("ItemDependency::Type::Any must be satisfied by the agent item alone");
		return false;
	}

	base->inventoryAgentEquipment[agentType.id] = 0;
	base->inventoryVehicleEquipment[vehicleType.id] = HELD_COUNT;
	if (!dep.satisfied(base))
	{
		LogError("ItemDependency::Type::Any must be satisfied by the vehicle item alone");
		return false;
	}

	return true;
}

bool test_item_dependency_any_empty(StateRef<Base> base)
{
	ItemDependency dep;
	dep.type = ItemDependency::Type::Any;

	if (!dep.satisfied(base))
	{
		LogError("ItemDependency::Type::Any with no items listed must be trivially satisfied");
		return false;
	}

	return true;
}

} // anonymous namespace

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

	state->startGame();
	state->initState();
	state->fillPlayerStartingProperty();

	if (state->player_bases.empty())
	{
		LogError("No player base available to test against");
		return EXIT_FAILURE;
	}
	StateRef<Base> base = {state.get(), state->player_bases.begin()->first};

	StateRef<AEquipmentType> agentType;
	if (!pickAgentEquipment(state, agentType))
	{
		LogError("No suitable (non-bio-storage) agent equipment type found");
		return EXIT_FAILURE;
	}

	StateRef<VEquipmentType> vehicleType;
	if (!pickVehicleEquipment(state, vehicleType))
	{
		LogError("No vehicle equipment type found");
		return EXIT_FAILURE;
	}

	if (!test_item_dependency_all(state, base, agentType, vehicleType))
	{
		LogError("ItemDependency::Type::All test failed");
		return EXIT_FAILURE;
	}

	if (!test_item_dependency_any(state, base, agentType, vehicleType))
	{
		LogError("ItemDependency::Type::Any test failed");
		return EXIT_FAILURE;
	}

	if (!test_item_dependency_any_empty(base))
	{
		LogError("ItemDependency::Type::Any (empty) test failed");
		return EXIT_FAILURE;
	}

	LogInfo("test_research success - all tests passed");
	return EXIT_SUCCESS;
}
