#include "framework/configfile.h"
#include "framework/filesystem.h"
#include "framework/framework.h"
#include "framework/logger.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/gamestate.h"
#include "game/state/rules/city/vehicletype.h"
#include "game/state/shared/organisation.h"
#include "library/strings.h"
#include <iostream>

// We can't just use 'using namespace OpenApoc;', see test_serialize.cpp for why.

using OpenApoc::City;
using OpenApoc::GameState;
using OpenApoc::sp;
using OpenApoc::StateRef;
using OpenApoc::Vec2;
using OpenApoc::Vec3;
using OpenApoc::Vehicle;
using OpenApoc::VehicleMission;
using OpenApoc::VehicleType;

namespace
{

// Find a Road-type vehicle type - these are the ground vehicles OPE-10 concerns itself with
// (as opposed to ATV, Flying or UFO).
StateRef<VehicleType> findRoadVehicleType(sp<GameState> state)
{
	for (auto &vt : state->vehicle_types)
	{
		if (vt.second->type == VehicleType::Type::Road)
		{
			return {state.get(), vt.first};
		}
	}
	return {};
}

// Pick two distinct road tiles from two distinct road segments in the given city. Returns
// false if the city doesn't have at least two non-empty road segments.
bool findTwoRoadTiles(StateRef<City> city, Vec3<int> &origin, Vec3<int> &target)
{
	bool foundOrigin = false;
	for (auto &seg : city->roadSegments)
	{
		if (seg.tilePosition.empty())
		{
			continue;
		}
		if (!foundOrigin)
		{
			origin = seg.tilePosition.front();
			foundOrigin = true;
			continue;
		}
		target = seg.tilePosition.front();
		return true;
	}
	return false;
}

} // namespace

// OPE-10: VehicleMission::setPathTo must not crash a ground vehicle whose road segment was
// severed out from under it (e.g. a bombed-out gap), only one that got no path at all with a
// nearby destination. This is the core regression the diff fixes: previously, "close enough
// target" alone was sufficient to crash-and-give-up, regardless of whether a (short, partial)
// path had actually been found.
//
// A severed origin segment deterministically makes City::findShortestPath return a single
// element path containing just the vehicle's own tile (see RoadSegment::getIntactByTile /
// City::findShortestPath's "Origin not intact?" early return) - a textbook "partial path", not
// "no path at all". Old code crashed such vehicles when giveUpIfInvalid and "close enough";
// new code must not.
bool test_setpathto_severed_road_does_not_crash(sp<GameState> state)
{
	LogInfo("Testing setPathTo does not crash a vehicle with a severed-but-partial path...");

	auto cityRef = StateRef<City>{state.get(), "CITYMAP_HUMAN"};
	if (!cityRef)
	{
		LogError("CITYMAP_HUMAN not found");
		return false;
	}

	auto roadType = findRoadVehicleType(state);
	if (!roadType)
	{
		LogError("No Road-type vehicle type found in loaded rules");
		return false;
	}

	Vec3<int> origin;
	Vec3<int> target;
	if (!findTwoRoadTiles(cityRef, origin, target))
	{
		LogError("Could not find two distinct road segments in CITYMAP_HUMAN");
		return false;
	}

	auto vehicle = cityRef->placeVehicle(
	    *state, roadType, state->getPlayer(),
	    Vec3<float>{(float)origin.x + 0.5f, (float)origin.y + 0.5f, (float)origin.z}, 0.0f);
	if (!vehicle || !vehicle->tileObject)
	{
		LogError("Failed to place road vehicle on the map");
		return false;
	}

	// Sever the road segment directly beneath the vehicle - the "bombed-out gap" scenario.
	cityRef->notifyRoadChange(origin, false);

	// --- Sub-case 1: giveUpIfInvalid = true ---
	// Old code: "target close enough" alone crashed the vehicle here. New code must not, since a
	// (single-tile) partial path was found.
	{
		VehicleMission mission;
		mission.type = VehicleMission::MissionType::GotoLocation;
		mission.reRouteAttempts = 5;
		vehicle->crashed = false;

		mission.setPathTo(*state, *vehicle, target, /*maxIterations=*/9001,
		                  /*checkValidity=*/true, /*giveUpIfInvalid=*/true);

		if (vehicle->crashed)
		{
			LogError("Vehicle was crashed despite having a (partial) path - severed road should "
			         "give up cleanly, never destroy the vehicle");
			return false;
		}
		if (!mission.cancelled)
		{
			LogError("Mission should be cancelled when giveUpIfInvalid is set and target isn't "
			         "reached");
			return false;
		}
	}

	// --- Sub-case 2: giveUpIfInvalid = false ---
	// Should spend a re-route attempt and, still, never crash.
	{
		VehicleMission mission;
		mission.type = VehicleMission::MissionType::GotoLocation;
		mission.reRouteAttempts = 5;
		vehicle->crashed = false;

		mission.setPathTo(*state, *vehicle, target, /*maxIterations=*/9001,
		                  /*checkValidity=*/true, /*giveUpIfInvalid=*/false);

		if (vehicle->crashed)
		{
			LogError("Vehicle was crashed with giveUpIfInvalid=false; should only ever crash "
			         "with giveUpIfInvalid=true and no path at all");
			return false;
		}
		if (mission.reRouteAttempts != 4)
		{
			LogError("Expected reRouteAttempts to be decremented once (was {0})",
			         mission.reRouteAttempts);
			return false;
		}
	}

	LogInfo("setPathTo severed-road test passed");
	return true;
}

// OPE-11/14/18: VehicleMission::advanceMissionCounterOnArrival() drives the UFO mission-counter
// decrement-to-zero transition threaded through AttackBuilding's start(). A missionCounter of 0
// (the pre-existing default) must be a no-op, preserving the old unlimited-attacks-on-one-target
// behavior for every mission that doesn't opt in via the new attackBuilding() parameter.
bool test_advance_mission_counter_zero_is_noop(sp<GameState> state)
{
	LogInfo("Testing advanceMissionCounterOnArrival with missionCounter=0 is a no-op...");

	auto cityRef = StateRef<City>{state.get(), "CITYMAP_HUMAN"};
	auto roadType = findRoadVehicleType(state);
	if (!cityRef || !roadType)
	{
		LogError("Missing city or road vehicle type for test setup");
		return false;
	}
	Vec3<int> origin;
	Vec3<int> unused;
	if (!findTwoRoadTiles(cityRef, origin, unused))
	{
		LogError("Could not find a road tile in CITYMAP_HUMAN");
		return false;
	}
	auto vehicle = cityRef->placeVehicle(
	    *state, roadType, state->getPlayer(),
	    Vec3<float>{(float)origin.x + 0.5f, (float)origin.y + 0.5f, (float)origin.z}, 0.0f);
	if (!vehicle)
	{
		LogError("Failed to place vehicle");
		return false;
	}

	VehicleMission mission;
	mission.type = VehicleMission::MissionType::AttackBuilding;
	mission.missionCounter = 0;
	mission.targetBuilding = {};

	bool result = mission.advanceMissionCounterOnArrival(*state, *vehicle);

	if (!result)
	{
		LogError("missionCounter=0 should never cancel the mission");
		return false;
	}
	if (mission.missionCounter != 0)
	{
		LogError("missionCounter=0 should stay 0 (no decrement below zero)");
		return false;
	}
	if (mission.cancelled)
	{
		LogError("missionCounter=0 should not cancel the mission");
		return false;
	}

	LogInfo("advanceMissionCounterOnArrival missionCounter=0 test passed");
	return true;
}

// A non-alien-owned vehicle whose missionCounter reaches zero retargets via the existing
// acquireTargetBuilding()/setPathTo() machinery rather than flying to a dimension gate (that
// path is alien-only, matching the owner split gamestate.cpp already uses for incursion setup).
bool test_advance_mission_counter_retargets_for_player(sp<GameState> state)
{
	LogInfo("Testing advanceMissionCounterOnArrival retargets a player vehicle at zero...");

	auto cityRef = StateRef<City>{state.get(), "CITYMAP_HUMAN"};
	auto roadType = findRoadVehicleType(state);
	if (!cityRef || !roadType)
	{
		LogError("Missing city or road vehicle type for test setup");
		return false;
	}
	Vec3<int> origin;
	Vec3<int> unused;
	if (!findTwoRoadTiles(cityRef, origin, unused))
	{
		LogError("Could not find a road tile in CITYMAP_HUMAN");
		return false;
	}
	auto vehicle = cityRef->placeVehicle(
	    *state, roadType, state->getPlayer(),
	    Vec3<float>{(float)origin.x + 0.5f, (float)origin.y + 0.5f, (float)origin.z}, 0.0f);
	if (!vehicle)
	{
		LogError("Failed to place vehicle");
		return false;
	}
	// Player vehicles are never aliens - takes the acquireTargetBuilding() branch.
	vehicle->owner = state->getPlayer();

	VehicleMission mission;
	mission.type = VehicleMission::MissionType::AttackBuilding;
	mission.missionCounter = 1;
	mission.targetBuilding = {};
	size_t missionsBefore = vehicle->missions.size();

	bool result = mission.advanceMissionCounterOnArrival(*state, *vehicle);

	if (mission.missionCounter != 0)
	{
		LogError("missionCounter should have decremented to 0");
		return false;
	}
	// Either a new target was acquired (result == true, targetBuilding set) or none was found
	// nearby (result == false, cancelled == true) - both are valid outcomes depending on map
	// data, but the mission must NOT silently continue with the old target unexamined.
	if (result)
	{
		if (!mission.targetBuilding)
		{
			LogError("advanceMissionCounterOnArrival returned true but left targetBuilding unset");
			return false;
		}
	}
	else if (!mission.cancelled)
	{
		LogError("advanceMissionCounterOnArrival returned false but did not cancel the mission");
		return false;
	}
	// This is the AttackBuilding path, not the alien portal path - must not add a GotoPortal
	// mission to the vehicle.
	if (vehicle->missions.size() != missionsBefore)
	{
		LogError("Player retarget path should not add missions to the vehicle directly");
		return false;
	}

	LogInfo("advanceMissionCounterOnArrival player retarget test passed");
	return true;
}

// An alien-owned vehicle whose missionCounter reaches zero heads for the nearest dimension gate
// instead of retargeting, matching the structural invariant documented in
// advanceMissionCounterOnArrival() (see docs/original-game/findings/U1-retarget-reconciliation.md).
bool test_advance_mission_counter_aliens_leave_via_portal(sp<GameState> state)
{
	LogInfo("Testing advanceMissionCounterOnArrival routes aliens to a portal at zero...");

	auto cityRef = StateRef<City>{state.get(), "CITYMAP_HUMAN"};
	auto roadType = findRoadVehicleType(state);
	if (!cityRef || !roadType)
	{
		LogError("Missing city or road vehicle type for test setup");
		return false;
	}
	if (cityRef->portals.empty())
	{
		LogWarning("CITYMAP_HUMAN has no portals generated, skipping alien-portal sub-test");
		return true;
	}
	Vec3<int> origin;
	Vec3<int> unused;
	if (!findTwoRoadTiles(cityRef, origin, unused))
	{
		LogError("Could not find a road tile in CITYMAP_HUMAN");
		return false;
	}
	auto vehicle = cityRef->placeVehicle(
	    *state, roadType, state->getAliens(),
	    Vec3<float>{(float)origin.x + 0.5f, (float)origin.y + 0.5f, (float)origin.z}, 0.0f);
	if (!vehicle)
	{
		LogError("Failed to place vehicle");
		return false;
	}

	VehicleMission mission;
	mission.type = VehicleMission::MissionType::AttackBuilding;
	mission.missionCounter = 1;
	size_t missionsBefore = vehicle->missions.size();

	bool result = mission.advanceMissionCounterOnArrival(*state, *vehicle);

	if (result)
	{
		LogError("Alien vehicle with portals available should be cancelled in favor of the new "
		         "GotoPortal mission, not continue this one");
		return false;
	}
	if (!mission.cancelled)
	{
		LogError("Mission should be cancelled once the alien vehicle is sent to a portal");
		return false;
	}
	if (vehicle->missions.size() != missionsBefore + 1)
	{
		LogError("Expected exactly one new mission (GotoPortal) to be added to the vehicle");
		return false;
	}
	if (vehicle->missions.back().type != VehicleMission::MissionType::GotoPortal)
	{
		LogError("Expected the newly added mission to be GotoPortal");
		return false;
	}

	LogInfo("advanceMissionCounterOnArrival alien portal test passed");
	return true;
}

// OPE-11/14/18: the UFO2P-derived incursion scatter clamp (FUN_0006da88 @ file 0x5DB80) - a
// scatter of exactly 15 with a >50 type percentage is clamped to 10; every other combination
// passes through unchanged.
bool test_incursion_scatter_clamp()
{
	LogInfo("Testing clampIncursionScatter...");

	if (VehicleMission::clampIncursionScatter(15, 60) != 10)
	{
		LogError("scatter=15, typePercent=60 should clamp to 10");
		return false;
	}
	if (VehicleMission::clampIncursionScatter(15, 50) != 15)
	{
		LogError("scatter=15, typePercent=50 (not > 50) should stay 15");
		return false;
	}
	if (VehicleMission::clampIncursionScatter(20, 60) != 20)
	{
		LogError("scatter=20 (not 15) should never be clamped");
		return false;
	}

	LogInfo("clampIncursionScatter test passed");
	return true;
}

// FUN_0006da88 @ file 0x5DBBF: (constitution * type_percent) / 100, integer division.
bool test_incursion_type_threshold()
{
	LogInfo("Testing incursionTypeThreshold...");

	if (VehicleMission::incursionTypeThreshold(100, 50) != 50)
	{
		LogError("incursionTypeThreshold(100, 50) should be 50");
		return false;
	}
	if (VehicleMission::incursionTypeThreshold(33, 10) != 3)
	{
		LogError("incursionTypeThreshold(33, 10) should truncate to 3");
		return false;
	}
	if (VehicleMission::incursionTypeThreshold(0, 90) != 0)
	{
		LogError("incursionTypeThreshold(0, 90) should be 0");
		return false;
	}

	LogInfo("incursionTypeThreshold test passed");
	return true;
}

// computeIncursionSpawnXY must keep zone-0 (rural) results out of the always-excluded urban
// core (10..90 both axes), which is the whole point of the zone acceptance/rejection logic in
// FUN_0003b724.
bool test_incursion_spawn_zone0_avoids_urban_core(sp<GameState> state)
{
	LogInfo("Testing computeIncursionSpawnXY zone 0 avoids the urban core...");

	for (int i = 0; i < 50; i++)
	{
		auto xy = VehicleMission::computeIncursionSpawnXY(*state, /*baseX=*/50, /*baseY=*/50,
		                                                  /*zoneMode=*/0, /*scatter=*/15);
		if (xy.x < 0 || xy.x > 100 || xy.y < 0 || xy.y > 100)
		{
			LogError("computeIncursionSpawnXY produced out-of-tile-range result {0},{1}", xy.x,
			         xy.y);
			return false;
		}
		bool inUrbanCore = xy.x > 10 && xy.x < 90 && xy.y > 10 && xy.y < 90;
		if (inUrbanCore)
		{
			LogError("Zone 0 spawn landed inside the urban core at {0},{1}", xy.x, xy.y);
			return false;
		}
	}

	LogInfo("computeIncursionSpawnXY zone 0 test passed");
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

	auto state = OpenApoc::mksp<GameState>();
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

	if (!test_setpathto_severed_road_does_not_crash(state))
	{
		LogError("setPathTo severed-road test failed");
		return EXIT_FAILURE;
	}

	if (!test_advance_mission_counter_zero_is_noop(state))
	{
		LogError("advanceMissionCounterOnArrival zero test failed");
		return EXIT_FAILURE;
	}

	if (!test_advance_mission_counter_retargets_for_player(state))
	{
		LogError("advanceMissionCounterOnArrival player retarget test failed");
		return EXIT_FAILURE;
	}

	if (!test_advance_mission_counter_aliens_leave_via_portal(state))
	{
		LogError("advanceMissionCounterOnArrival alien portal test failed");
		return EXIT_FAILURE;
	}

	if (!test_incursion_scatter_clamp())
	{
		LogError("Incursion scatter clamp test failed");
		return EXIT_FAILURE;
	}

	if (!test_incursion_type_threshold())
	{
		LogError("Incursion type threshold test failed");
		return EXIT_FAILURE;
	}

	if (!test_incursion_spawn_zone0_avoids_urban_core(state))
	{
		LogError("Incursion spawn zone 0 test failed");
		return EXIT_FAILURE;
	}

	LogInfo("test_ground_vehicle_path success - all tests passed");
	return EXIT_SUCCESS;
}
