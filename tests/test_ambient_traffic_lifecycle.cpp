// A destination can lose its last vehicle entrance after a generated trip has launched.
// Exercise real takeoff and mover updates: a queued-only fixture misses the flyer fallback that
// keeps routing to nearby points while reserving an ambient traffic slot indefinitely.
#include "framework/configfile.h"
#include "framework/framework.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/scenery.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/gamestate.h"
#include "game/state/gametime.h"
#include "game/state/rules/agenttype.h"
#include "game/state/rules/city/scenerytiletype.h"
#include "game/state/rules/city/vehicletype.h"
#include "game/state/rules/city/vequipmenttype.h"
#include "game/state/shared/agent.h"
#include "game/state/shared/organisation.h"
#include "game/state/tilemap/tile.h"
#include "game/state/tilemap/tilemap.h"
#include "tests/test_helpers.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

namespace
{
constexpr unsigned int STEP_TICKS = 6;
UString commonPath, gamePath;

enum class Protection
{
	None,
	Player,
	Cargo,
	Passenger,
	Permanent
};

struct RemovedAccess
{
	std::map<Tile *, sp<Scenery>> roadTiles;
	std::vector<sp<Scenery>> destroyedPads;

	void remove(GameState &state, City &city, const Building &building, bool road)
	{
		if (road)
		{
			const auto position = building.carEntranceLocation;
			if (city.map->tileIsValid(position))
			{
				auto *tile = city.map->getTile(position);
				roadTiles.emplace(tile, tile->presentScenery);
				// Scenery::die preserves bottom tiles, so model loss of this low road entrance.
				tile->presentScenery.reset();
			}
		}
		else
		{
			for (const auto &pad : building.landingPadLocations)
			{
				if (city.map->tileIsValid(pad))
				{
					auto scenery = city.map->getTile(pad)->presentScenery;
					if (scenery && !scenery->destroyed)
					{
						destroyedPads.push_back(scenery);
					}
				}
			}
			// Collect scenery before destruction callbacks update map occupancy and support.
			for (const auto &pad : destroyedPads)
			{
				pad->die(state, true);
			}
		}
	}

	bool empty() const { return roadTiles.empty() && destroyedPads.empty(); }

	~RemovedAccess()
	{
		for (const auto &[tile, scenery] : roadTiles)
		{
			tile->presentScenery = scenery;
		}
	}
};

bool launchedAndMoving(GameState &state, Vehicle &vehicle)
{
	const auto initialPosition = vehicle.position;
	for (unsigned int elapsed = 0; elapsed < 2 * TICKS_PER_MINUTE; elapsed += STEP_TICKS)
	{
		vehicle.update(state, STEP_TICKS);
		const auto delta = vehicle.position - initialPosition;
		const bool takingOff =
		    std::any_of(vehicle.missions.begin(), vehicle.missions.end(), [](const auto &mission)
		                { return mission.type == VehicleMission::MissionType::TakeOff; });
		if (!vehicle.isDead() && vehicle.tileObject && !vehicle.currentBuilding && !takingOff &&
		    delta.x * delta.x + delta.y * delta.y > 9.0f)
		{
			return true;
		}
	}
	return false;
}

bool hasDestructiblePads(const City &city, const Building &building)
{
	bool found = false;
	for (const auto &pad : building.landingPadLocations)
	{
		if (!city.map->tileIsValid(pad))
		{
			continue;
		}
		const auto &scenery = city.map->getTile(pad)->presentScenery;
		if (scenery && !scenery->destroyed && scenery->type->isLandingPad)
		{
			// Scenery::die deliberately preserves all bottom scenery, including landing pads.
			if (scenery->initialPosition.z <= 1)
			{
				return false;
			}
			found = true;
		}
	}
	return found;
}

bool runDestinationLoss(bool road, Protection protection, bool removeDestination = true,
                        bool removeFutureReturn = false)
{
	auto state = mksp<GameState>();
	TEST_REQUIRE(loadStartedGameState(*state, commonPath, gamePath), "could not load city fixture");
	auto city = state->current_city;
	const StateRef<VehicleType> type{state.get(), road ? "VEHICLETYPE_CIVILIAN_CAR"
	                                              : protection == Protection::Cargo
	                                                  ? "VEHICLETYPE_AIRTRANS"
	                                                  : "VEHICLETYPE_AIRTAXI"};
	TEST_REQUIRE(city && type, "missing human city or traffic vehicle type");
	StateRef<Building> source, destination;
	for (const auto &candidate : city->buildings)
	{
		if (!candidate || !candidate->owner || candidate->owner == state->getPlayer() ||
		    candidate->owner == state->getAliens() || !city->hasVehicleAccess(*candidate, *type) ||
		    (!road && !hasDestructiblePads(*city, *candidate)))
		{
			continue;
		}
		if (!source)
		{
			source = candidate;
			continue;
		}
		const auto from = road ? source->carEntranceLocation : *source->landingPadLocations.begin();
		const auto to =
		    road ? candidate->carEntranceLocation : *candidate->landingPadLocations.begin();
		if (std::abs(from.x - to.x) + std::abs(from.y - to.y) < 30)
		{
			continue;
		}
		if (road)
		{
			const GroundVehicleTileHelper helper{*city->map, type->type};
			const auto &route = city->findShortestPath(from, to, helper);
			if (route.empty() || route.back() != to)
			{
				continue;
			}
		}
		destination = candidate;
		break;
	}
	TEST_REQUIRE(source && destination, "no distant accessible building pair for test trip");
	const auto owner = protection == Protection::Player ? state->getPlayer() : source->owner;
	auto vehicle = city->placeVehicle(*state, type, owner, source);
	TEST_REQUIRE(vehicle, "could not create test trip");
	vehicle->homeBuilding = source;
	vehicle->ambientTraffic = protection != Protection::Permanent;
	if (protection == Protection::Cargo)
	{
		TEST_REQUIRE(vehicle->getMaxCargo() > 0 && !state->vehicle_equipment.empty(),
		             "test freight vehicle has no cargo capacity or cargo type");
		vehicle->cargo.emplace_back(
		    *state, StateRef<VEquipmentType>{state.get(), state->vehicle_equipment.begin()->first},
		    1, 0, owner, destination);
	}
	StateRef<Agent> passenger;
	if (protection == Protection::Passenger)
	{
		TEST_REQUIRE(vehicle->getMaxPassengers() > 0, "test taxi has no passenger capacity");
		passenger = state->agent_generator.createAgent(*state, owner, AgentType::Role::Soldier);
		TEST_REQUIRE(passenger, "could not create test passenger");
		passenger->enterVehicle(*state, {state.get(), vehicle});
	}
	TEST_REQUIRE(vehicle->setMission(
	                 *state, VehicleMission::gotoBuilding(*state, *vehicle, destination, false)),
	             "could not start test trip");
	if (removeFutureReturn)
	{
		vehicle->addMission(*state, VehicleMission::gotoBuilding(*state, *vehicle, source, false),
		                    true);
	}
	TEST_REQUIRE(launchedAndMoving(*state, *vehicle), "trip did not complete takeoff and move");
	TEST_REQUIRE(!vehicle->missions.empty(), "trip finished before losing its destination");
	RemovedAccess access;
	if (removeDestination || removeFutureReturn)
	{
		access.remove(*state, *city, removeFutureReturn ? *source : *destination, road);
		TEST_REQUIRE(!access.empty(), "test removed no entrance scenery");
		TEST_REQUIRE(!city->hasVehicleAccess(removeFutureReturn ? *source : *destination, *type),
		             "test building still has vehicle access");
	}
	const auto doodadsBefore = city->doodads.size();
	const auto id = Vehicle::getId(*state, vehicle);
	vehicle->update(*state, STEP_TICKS);
	const bool shouldRetire =
	    removeDestination && !removeFutureReturn && protection == Protection::None;
	TEST_CHECK(vehicle->isDead() == shouldRetire,
	           "destination loss retired={0}, expected={1}, road={2}, protection={3}",
	           vehicle->isDead(), shouldRetire, road, static_cast<int>(protection));
	TEST_CHECK(city->doodads.size() == doodadsBefore, "retiring inaccessible trip made a doodad");
	if (passenger)
	{
		TEST_CHECK(!passenger->isDead() && vehicle->currentAgents.count(passenger),
		           "destination loss killed or unloaded the protected passenger");
	}
	if (protection == Protection::Cargo)
	{
		TEST_CHECK(vehicle->cargo.size() == 1 && vehicle->cargo.front().count == 1,
		           "destination loss discarded protected cargo");
	}
	state->cleanUpDeathNote();
	TEST_CHECK((state->vehicles.count(id) == 0) == shouldRetire,
	           "inaccessible trip did not release its vehicle slot");
	return true;
}

bool test_lost_air_destination() { return runDestinationLoss(false, Protection::None); }
bool test_lost_road_destination() { return runDestinationLoss(true, Protection::None); }
bool test_player_protected() { return runDestinationLoss(false, Protection::Player); }
bool test_cargo_protected() { return runDestinationLoss(false, Protection::Cargo); }
bool test_passenger_protected() { return runDestinationLoss(false, Protection::Passenger); }
bool test_permanent_protected() { return runDestinationLoss(false, Protection::Permanent); }
bool test_intact_destination() { return runDestinationLoss(false, Protection::None, false); }
bool test_future_return_not_active()
{
	return runDestinationLoss(false, Protection::None, false, true);
}
} // namespace

int main(int argc, char **argv)
{
	config().addPositionalArgument("common", "Common gamestate to load");
	config().addPositionalArgument("gamestate", "Gamestate to load");
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}
	applyDeterministicTestConfig();
	commonPath = config().getString("common");
	gamePath = config().getString("gamestate");
	if (commonPath.empty() || gamePath.empty())
	{
		LogError("Must provide common and gamestate paths");
		return EXIT_FAILURE;
	}
	Framework fw("OpenApoc", false);
	return runTestSuite({{"lost_air_destination", test_lost_air_destination},
	                     {"lost_road_destination", test_lost_road_destination},
	                     {"player_protected", test_player_protected},
	                     {"cargo_protected", test_cargo_protected},
	                     {"passenger_protected", test_passenger_protected},
	                     {"permanent_protected", test_permanent_protected},
	                     {"intact_destination", test_intact_destination},
	                     {"future_return_not_active", test_future_return_not_active}});
}
