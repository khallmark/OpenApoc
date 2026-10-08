// A day-long Speed 5 census found reciprocal blockers on these adjacent real T-junctions.
// Its stopped poses, facings and next tiles are observed; the straight continuations below
// are a minimal inferred route fixture, not a replay of the census's unrecorded onward path.
#include "framework/configfile.h"
#include "framework/framework.h"
#include "game/state/city/city.h"
#include "game/state/city/scenery.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/gamestate.h"
#include "game/state/rules/city/scenerytiletype.h"
#include "game/state/tilemap/tile.h"
#include "game/state/tilemap/tilemap.h"
#include "game/state/tilemap/tileobject_vehicle.h"
#include "tests/test_helpers.h"
#include <algorithm>
#include <array>
#include <iostream>

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

namespace
{
constexpr Vec3<int> northJunction{87, 38, 2}, southJunction{87, 39, 2};
constexpr Vec3<int> northTarget{87, 36, 2}, southTarget{87, 41, 2};
constexpr Vec3<float> taxiPose{87.5f, 39.640625f, 2.0f};
constexpr Vec3<float> policePose{87.5f, 38.359375f, 2.0f};

struct Cars
{
	GameState &state;
	sp<Vehicle> taxi, police;
	~Cars()
	{
		for (const auto &v : {taxi, police})
		{
			if (v)
			{
				v->die(state, true);
			}
		}
		state.cleanUpDeathNote();
	}
};

bool verifyJunctions(GameState &state)
{
	auto city = state.current_city;
	TEST_REQUIRE(city && city->map, "no initialized city map");
	const std::array<std::pair<Vec3<int>, std::vector<bool>>, 2> expected = {{
	    {northJunction, {true, true, true, false}},
	    {southJunction, {true, false, true, true}},
	}};
	for (const auto &[position, connections] : expected)
	{
		const auto *tile = city->map->getTile(position);
		TEST_REQUIRE(tile->presentScenery &&
		                 tile->presentScenery->type->tile_type == SceneryTileType::TileType::Road,
		             "observed junction {0} is not a road", position);
		TEST_REQUIRE(tile->presentScenery->type->connection == connections,
		             "observed junction {0} has changed its road connections", position);
	}
	const GroundVehicleTileHelper road{*city->map, VehicleType::Type::Road};
	for (int y = northTarget.y; y < southTarget.y; y++)
	{
		auto *north = city->map->getTile(Vec3<int>{87, y, 2});
		auto *south = city->map->getTile(Vec3<int>{87, y + 1, 2});
		TEST_REQUIRE(road.canEnterTile(north, south) && road.canEnterTile(south, north),
		             "straight route fixture is disconnected at y={0}", y);
	}
	return true;
}

bool placeObservedCars(GameState &state, Cars &cars)
{
	auto city = state.current_city;
	cars.taxi = city->placeVehicle(state, {&state, "VEHICLETYPE_AUTOTAXI"}, state.getPlayer(),
	                               taxiPose, 1.8449638f);
	cars.police = city->placeVehicle(state, {&state, "VEHICLETYPE_POLICE_CAR"}, state.getPlayer(),
	                                 policePose, 4.712389f);
	TEST_REQUIRE(cars.taxi && cars.police, "could not place the observed road vehicle types");
	cars.taxi->setMission(state, VehicleMission::gotoLocation(state, *cars.taxi, northTarget));
	cars.police->setMission(state, VehicleMission::gotoLocation(state, *cars.police, southTarget));
	TEST_REQUIRE(!cars.taxi->missions.empty() && !cars.police->missions.empty(),
	             "straight route missions were not created");
	// The census recorded the reciprocal next tiles. Keep a single current tile so the first
	// update tests that departure, without an initial centering move changing the stale facing.
	cars.taxi->missions.front().currentPlannedPath = {
	    southJunction, northJunction, {87, 37, 2}, northTarget};
	cars.police->missions.front().currentPlannedPath = {
	    northJunction, southJunction, {87, 40, 2}, southTarget};
	for (const auto &v : {cars.taxi, cars.police})
	{
		v->goalPosition = v->position;
		v->velocity = {0, 0, 0};
	}
	return true;
}

bool test_stopped_opposite_lanes_progress(GameState &state, unsigned step, bool turbo)
{
	Cars cars{state};
	TEST_REQUIRE(placeObservedCars(state, cars), "could not initialize reciprocal blocker fixture");
	auto *north = state.current_city->map->getTile(northJunction);
	auto *south = state.current_city->map->getTile(southJunction);
	TEST_CHECK(!cars.taxi->missions.front().roadBlocker(*cars.taxi, south, north, 0),
	           "northbound car treats the stopped southbound car's stale facing as a crossing");
	TEST_CHECK(!cars.police->missions.front().roadBlocker(*cars.police, north, south, 2),
	           "southbound car treats the stopped northbound car's stale facing as a crossing");
	const bool previousTurbo = state.skipTurboCalculations;
	state.skipTurboCalculations = turbo;
	// Coarse movement reaches the penultimate goal, then the target, then its completion
	// callback on successive frames. Require completed routes after all three frames.
	constexpr unsigned duration = 3 * City::AMBIENT_TRAFFIC_TICKS;
	for (unsigned elapsed = 0; elapsed < duration; elapsed += step)
	{
		cars.taxi->update(state, step);
		cars.police->update(state, step);
		state.gameTime.addTicks(step);
	}
	state.skipTurboCalculations = previousTurbo;
	TEST_CHECK(cars.taxi->tileObject && cars.police->tileObject,
	           "a reciprocal blocker disappeared instead of progressing");
	TEST_CHECK(
	    cars.taxi->tileObject && cars.taxi->tileObject->getOwningTile()->position == northTarget &&
	        cars.taxi->missions.empty(),
	    "northbound car did not finish its route with step {0}: {1}", step, cars.taxi->position);
	TEST_CHECK(cars.police->tileObject &&
	               cars.police->tileObject->getOwningTile()->position == southTarget &&
	               cars.police->missions.empty(),
	           "southbound car did not finish its route with step {0}: {1}", step,
	           cars.police->position);
	std::cout << format("JUNCTION_PAIR step={0} turbo={1} taxi={2} police={3}\n", step, turbo,
	                    cars.taxi->position, cars.police->position);
	return true;
}

bool test_queues_and_crossing_entrants_still_block(GameState &state)
{
	Cars cars{state};
	TEST_REQUIRE(placeObservedCars(state, cars), "could not initialize junction priority fixture");
	auto *north = state.current_city->map->getTile(northJunction);
	auto *south = state.current_city->map->getTile(southJunction);
	// An occupant leaving by the same exit still reserves that lane, regardless of its facing.
	cars.police->missions.front().currentPlannedPath = {northJunction, {87, 37, 2}, northTarget};
	TEST_CHECK(cars.taxi->missions.front().roadBlocker(*cars.taxi, south, north, 0).get() ==
	               cars.police.get(),
	           "a standing car going to the same exit no longer blocks its queue");
	// Unknown routes retain the facing fallback: a north-facing parked car also blocks the exit.
	cars.police->missions.clear();
	cars.police->facing = 0;
	TEST_CHECK(cars.taxi->missions.front().roadBlocker(*cars.taxi, south, north, 0).get() ==
	               cars.police.get(),
	           "a parked car with no known route no longer reserves its facing's exit");
	// A car entering from the east and turning south crosses the northbound approach. Place
	// its leading edge inside the junction while its owning tile remains the eastern road.
	cars.police->setPosition({88.01f, 38.359375f, 2.0f});
	cars.police->setMission(state, VehicleMission::gotoLocation(state, *cars.police, southTarget));
	TEST_REQUIRE(!cars.police->missions.empty(), "crossing entrant mission was not created");
	cars.police->missions.front().currentPlannedPath = {
	    {88, 38, 2}, northJunction, southJunction, {87, 40, 2}, southTarget};
	cars.police->goalPosition = {87.5f, 38.5f, 2.0f};
	cars.police->facing = 4.712389f;
	TEST_REQUIRE(cars.police->tileObject->getOwningTile()->position == Vec3<int>(88, 38, 2) &&
	                 std::find(north->intersectingObjects.begin(), north->intersectingObjects.end(),
	                           cars.police->tileObject) != north->intersectingObjects.end(),
	             "crossing car does not actually overlap the junction from its approach");
	TEST_CHECK(cars.taxi->missions.front().roadBlocker(*cars.taxi, south, north, 0).get() ==
	               cars.police.get(),
	           "an entering car's crossing trajectory no longer blocks the junction");
	return true;
}

bool test_skipped_left_turn_keeps_its_exit(GameState &state, bool shortcut)
{
	Cars cars{state};
	auto city = state.current_city;
	const Vec3<int> approach{87, 40, 2}, start{87, 41, 2}, west{86, 39, 2}, target{85, 39, 2};
	const Vec3<int> from = shortcut ? approach : start;
	cars.taxi = city->placeVehicle(state, {&state, "VEHICLETYPE_AUTOTAXI"}, state.getPlayer(),
	                               {87.640625f, from.y + 0.5f, 2}, 0);
	cars.police = city->placeVehicle(state, {&state, "VEHICLETYPE_POLICE_CAR"}, state.getPlayer(),
	                                 {87.359375f, 39.5f, 2}, 3.1415927f);
	TEST_REQUIRE(cars.taxi && cars.police, "could not place the left-turn conflict fixture");
	cars.taxi->setMission(state, VehicleMission::gotoLocation(state, *cars.taxi, target));
	cars.police->setMission(state, VehicleMission::gotoLocation(state, *cars.police, southTarget));
	TEST_REQUIRE(!cars.taxi->missions.empty() && !cars.police->missions.empty(),
	             "left-turn conflict missions were not created");
	auto &mission = cars.taxi->missions.front();
	// Natural goto paths can repeat their current tile. The shortcut check can therefore be
	// the first check of an adjacent junction even with ordinary, small movement updates.
	const std::deque<Vec3<int>> route =
	    shortcut ? std::deque<Vec3<int>>{approach, approach, southJunction, west, target}
	             : std::deque<Vec3<int>>{start, approach, southJunction, west, target};
	mission.currentPlannedPath = route;
	cars.police->missions.front().currentPlannedPath = {southJunction, approach, start};
	for (const auto &v : {cars.taxi, cars.police})
	{
		v->goalPosition = v->position;
		v->velocity = {0, 0, 0};
	}
	const GroundVehicleTileHelper road{*city->map, VehicleType::Type::Road};
	for (auto it = route.begin(); std::next(it) != route.end(); ++it)
	{
		const auto next = std::next(it);
		TEST_REQUIRE(*it == *next ||
		                 road.canEnterTile(city->map->getTile(*it), city->map->getTile(*next)),
		             "left-turn route fixture is disconnected at {0}", *it);
	}
	auto *junction = city->map->getTile(southJunction);
	auto *before = city->map->getTile(approach);
	// Northbound turning west is trajectory 14, which crosses southbound straight (8).
	// Passing the inbound north heading as the exit instead describes straight (2), which
	// clears 8. These direct calls establish why the following route node matters.
	TEST_REQUIRE(mission.roadBlocker(*cars.taxi, before, junction, 3).get() == cars.police.get(),
	             "the north-to-west left turn does not conflict with the southbound occupant");
	TEST_REQUIRE(!mission.roadBlocker(*cars.taxi, before, junction, 0),
	             "the opposite straight lanes unexpectedly conflict in the fixture");
	Vec3<float> destination;
	float facing = 0;
	int turboTiles = shortcut ? 0 : 20;
	TEST_REQUIRE(mission.advanceAlongPath(state, *cars.taxi, destination, facing, turboTiles),
	             "left-turn mission did not supply a movement goal");
	TEST_CHECK(Vec3<int>(destination) == approach,
	           "{0} skipped through a crossing left turn to {1}", shortcut ? "shortcut" : "turbo",
	           destination);
	TEST_CHECK(mission.currentPlannedPath.front() == approach,
	           "left-turn skip consumed the blocked junction from its route");
	// Exercise the actual mover independently of the goal calculation above. Ordinary
	// movement must wait too; hold the occupant still and stay below the U-turn timeout.
	mission.currentPlannedPath = route;
	const bool previousTurbo = state.skipTurboCalculations;
	state.skipTurboCalculations = !shortcut;
	const unsigned step = shortcut ? 6 : City::AMBIENT_TRAFFIC_TICKS;
	const unsigned duration = shortcut ? 3 * TICKS_PER_SECOND : step;
	for (unsigned elapsed = 0; elapsed < duration; elapsed += step)
	{
		cars.taxi->update(state, step);
		state.gameTime.addTicks(step);
	}
	state.skipTurboCalculations = previousTurbo;
	TEST_CHECK(cars.taxi->tileObject, "left-turn requester disappeared instead of waiting");
	const Vec3<int> actualPosition = cars.taxi->position;
	TEST_CHECK(actualPosition.x == approach.x && actualPosition.y >= approach.y,
	           "actual {0} position crossed the blocked left turn to {1}",
	           shortcut ? "shortcut" : "turbo", cars.taxi->position);
	std::cout << format("JUNCTION_LEFT_TURN shortcut={0} destination={1} position={2}\n", shortcut,
	                    destination, cars.taxi->position);
	return true;
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
	const auto common = config().getString("common");
	const auto gamestate = config().getString("gamestate");
	if (common.empty() || gamestate.empty())
	{
		LogError("Must provide common and gamestate paths");
		return EXIT_FAILURE;
	}
	Framework fw("OpenApoc", false);
	GameState state;
	if (!loadStartedGameState(state, common, gamestate) || !verifyJunctions(state))
	{
		return EXIT_FAILURE;
	}
	bool ok = test_stopped_opposite_lanes_progress(state, 6, false);
	ok = test_stopped_opposite_lanes_progress(state, City::AMBIENT_TRAFFIC_TICKS, true) && ok;
	ok = test_queues_and_crossing_entrants_still_block(state) && ok;
	ok = test_skipped_left_turn_keeps_its_exit(state, false) && ok;
	ok = test_skipped_left_turn_keeps_its_exit(state, true) && ok;
	return !ok || testCheckFailed ? EXIT_FAILURE : EXIT_SUCCESS;
}
