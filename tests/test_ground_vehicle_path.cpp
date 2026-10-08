// Regression coverage for OpenApoc parity item V2 (ground vehicle engagement / large footprint) --
// the live-defect half of the row, independent of the still-unbound "Rules of engagement" table.
// See docs/original-game/parity-guide.md "V2 - Ground vehicle engagement / large footprint" and
// tools/extractors/docs/version01readme.txt, which warns that ordering ground vehicles around can
// crash.
//
// The defect lived in VehicleMission::setPathTo's "did not reach destination" handling. A ground
// vehicle is restricted to road tiles and dies instantly if the road beneath it is destroyed (see
// GroundVehicleMover::update), so a *severed* road network -- the target unreachable because a
// road segment somewhere between here and there is gone -- is an entirely ordinary outcome of
// issuing a move order, not a collision. But the pathfinder still returns a non-empty partial path
// ending at the closest reachable point, and the old code only recognised two shapes of failure:
// an empty path with a close target (crash the vehicle so it becomes recoverable), or an empty
// path with a far target (cancel, no crash). A *non-empty* partial path that falls short of the
// target fell through both checks:
//   - if the target was within the "close enough" iteration budget, the mission kept re-planning
//     to the same closest point every tick, burning a reroute attempt each time, until attempts
//     ran out -- at which point it hit the "close and giving up" branch and crashed the vehicle
//     for no collision at all (ground_vehicle_order_does_not_crash).
//   - if the target was far enough that the "close enough" heuristic didn't apply, no branch ever
//     fired, reRouteAttempts never moved, and the mission re-planned the same unreachable route
//     forever without ever finishing (ground_vehicle_path_terminates_on_severed_road).
//
// Both cases are reproduced here by placing a Road-type vehicle on a real road tile from the
// extracted city and severing the road segment directly under it via City::notifyRoadChange --
// the same call Scenery::die makes when a road tile is destroyed -- before issuing a gotoLocation
// order. Ticks are simulated by calling Vehicle::getNewGoal directly, which is what
// GroundVehicleMover::update calls every time the vehicle is idle at its goal position.

#include "framework/configfile.h"
#include "framework/framework.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/scenery.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/gamestate.h"
#include "game/state/rules/city/scenerytiletype.h"
#include "game/state/rules/city/vehicletype.h"
#include "game/state/shared/organisation.h"
#include "game/state/tilemap/tilemap.h"
#include "game/state/tilemap/tileobject_scenery.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static sp<GameState> g_state;

namespace
{

// More than the default 20 gotoLocation reroute attempts, so a genuine non-terminating loop is
// unambiguous rather than a false negative from too small a tick budget.
constexpr int MAX_TICKS = 40;

// A road tile taken from a long, intact road segment in the extracted CITYMAP_HUMAN map (segment
// length 15, first tile {32,31,2}). Any road tile would do; this one is simply known-good and far
// from the map edge.
const Vec3<int> ROAD_ORIGIN = {32, 31, 2};

// Places a Road-type vehicle directly on ROAD_ORIGIN and severs the road segment under it, so the
// vehicle's own tile is no longer part of any intact road segment -- i.e. the road immediately
// under/ahead of the vehicle has been destroyed.
sp<Vehicle> spawnVehicleOnSeveredRoad(GameState &state, sp<City> city)
{
	auto v = city->placeVehicle(
	    state, {&state, "VEHICLETYPE_CIVILIAN_CAR"}, state.getPlayer(),
	    Vec3<float>{ROAD_ORIGIN.x + 0.5f, ROAD_ORIGIN.y + 0.5f, ROAD_ORIGIN.z + 0.5f}, 0.0f);
	if (!v)
	{
		return nullptr;
	}
	city->notifyRoadChange(ROAD_ORIGIN, false);
	return v;
}

// Simulates up to MAX_TICKS idle-at-goal ticks, exactly as GroundVehicleMover::update would drive
// the mission machinery, and returns how many ticks it took for the mission to terminate (pop off
// v->missions), or -1 if it never did within the budget.
int simulateUntilMissionTerminates(GameState &state, Vehicle &v)
{
	int turboTiles = 0;
	for (int tick = 1; tick <= MAX_TICKS; tick++)
	{
		v.getNewGoal(state, turboTiles);
		if (v.missions.empty())
		{
			return tick;
		}
	}
	return -1;
}

} // namespace

// Far target: beyond the "close enough to reach" heuristic once the origin segment is severed.
// Before the fix this never terminated -- reRouteAttempts never moved and the mission re-planned
// the same unreachable target forever.
static bool test_ground_vehicle_path_terminates_on_severed_road()
{
	auto &state = *g_state;
	auto cityIt = state.cities.find("CITYMAP_HUMAN");
	TEST_REQUIRE(cityIt != state.cities.end(), "no CITYMAP_HUMAN city in gamestate");
	auto city = cityIt->second;
	TEST_REQUIRE(state.vehicle_types.find("VEHICLETYPE_CIVILIAN_CAR") != state.vehicle_types.end(),
	             "VEHICLETYPE_CIVILIAN_CAR missing from extracted gamestate");
	TEST_REQUIRE(state.getPlayer(), "no player org");

	auto v = spawnVehicleOnSeveredRoad(state, city);
	TEST_REQUIRE(v != nullptr, "placeVehicle failed");
	TEST_REQUIRE((bool)v->tileObject, "vehicle has no tile object after placement");

	// Far side of the 140x140 city map -- well beyond gotoLocation's "close enough" iteration
	// budget, and unreachable now that the origin segment is severed.
	const Vec3<int> farTarget = {120, 120, ROAD_ORIGIN.z};
	v->addMission(state, VehicleMission::gotoLocation(state, *v, farTarget));
	TEST_REQUIRE(!v->missions.empty(), "gotoLocation mission was not queued");

	const int terminatedAtTick = simulateUntilMissionTerminates(state, *v);
	TEST_REQUIRE(terminatedAtTick > 0,
	             "gotoLocation across a severed road never terminated within {0} ticks -- "
	             "pathfinder looped instead of giving up",
	             MAX_TICKS);
	TEST_CHECK(v->missions.empty(), "vehicle should have no pending missions once terminated");
	TEST_CHECK(!v->crashed, "vehicle should not be crashed after a merely-unreachable order");
	TEST_CHECK(!v->isDead(), "vehicle should not have died from an unreachable order");
	return true;
}

// Close target: within the "close enough to reach" heuristic. Before the fix this DID terminate,
// but only by exhausting reroute attempts and then calling Vehicle::setCrashed -- destroying an
// undamaged vehicle purely because its destination was unreachable, not because of any collision.
static bool test_ground_vehicle_order_does_not_crash()
{
	auto &state = *g_state;
	auto cityIt = state.cities.find("CITYMAP_HUMAN");
	TEST_REQUIRE(cityIt != state.cities.end(), "no CITYMAP_HUMAN city in gamestate");
	auto city = cityIt->second;
	TEST_REQUIRE(state.getPlayer(), "no player org");

	auto v = spawnVehicleOnSeveredRoad(state, city);
	TEST_REQUIRE(v != nullptr, "placeVehicle failed");

	// A couple of tiles down the same (now severed) segment -- close enough that
	// maxIterations > distance, which is the branch that used to crash the vehicle on give-up.
	const Vec3<int> closeTarget = {ROAD_ORIGIN.x + 2, ROAD_ORIGIN.y, ROAD_ORIGIN.z};
	v->addMission(state, VehicleMission::gotoLocation(state, *v, closeTarget));
	TEST_REQUIRE(!v->missions.empty(), "gotoLocation mission was not queued");

	const int terminatedAtTick = simulateUntilMissionTerminates(state, *v);
	TEST_REQUIRE(terminatedAtTick > 0,
	             "gotoLocation across a severed road never terminated within {0} ticks", MAX_TICKS);
	TEST_CHECK(!v->crashed,
	           "vehicle self-destructed at tick {0} merely because its destination was "
	           "unreachable across a severed road, not because of a collision",
	           terminatedAtTick);
	TEST_CHECK(!v->isDead(), "vehicle should not have died from an unreachable order");
	return true;
}

// Routes on a road cut by destroyed tiles. The pathfinder assumed a road segment's connections[0]
// meets its first tile and connections[1] its last. That holds for an intact city, but not after
// fighting: in a learner campaign the junction at {46,99,2} lost branches, and the segment map
// rebuilt when the save loaded folded it into a road {46,99,2}..{46,101,2} whose only connection
// is at its FAR end, the single-tile segment {46,102,2}. Routing from the junction tile then
// "entered" that connection straight from {46,99,2} -- a vehicle teleporting three tiles -- and
// read connections[1] past the vector, which AddressSanitizer caught as a heap-buffer-overflow
// under VehicleMission::setPathTo.
// The segment map that produced it came out of a long campaign's damage and road building, so
// it is rebuilt here directly on the same tiles. Every route must start at its origin and walk.
static bool test_route_from_road_connected_at_its_far_end()
{
	auto &state = *g_state;
	auto city = state.cities["CITYMAP_HUMAN"];
	TEST_REQUIRE(city, "no CITYMAP_HUMAN city in gamestate");
	const std::vector<Vec3<int>> roadTiles = {{46, 99, 2}, {46, 100, 2}, {46, 101, 2}};
	const Vec3<int> farEnd = {46, 102, 2};
	for (const auto &t : {roadTiles[0], roadTiles[1], roadTiles[2], farEnd})
	{
		auto scenery = city->map->getTile(t)->presentScenery;
		TEST_REQUIRE(scenery && scenery->type->tile_type == SceneryTileType::TileType::Road,
		             "expected a road tile at {0} in the extracted city", t);
	}
	const int roadID = (int)city->roadSegments.size();
	const int farID = roadID + 1;
	RoadSegment road(roadTiles[0], farID);
	road.tilePosition.push_back(roadTiles[1]);
	road.tilePosition.push_back(roadTiles[2]);
	road.finalizeStats();
	RoadSegment far(farEnd, roadID);
	far.finalizeStats();
	city->roadSegments.push_back(road);
	city->roadSegments.push_back(far);
	const auto &size = city->map->size;
	auto index = [&](const Vec3<int> &t) { return t.z * size.x * size.y + t.y * size.x + t.x; };
	for (const auto &t : roadTiles)
	{
		city->tileToRoadSegmentMap[index(t)] = roadID;
	}
	city->tileToRoadSegmentMap[index(farEnd)] = farID;

	for (const auto &origin : roadTiles)
	{
		const auto path = city->findShortestPathUncached(origin, ROAD_ORIGIN);
		TEST_REQUIRE(!path.empty() && path.front() == origin, "route from {0} does not start there",
		             origin);
		auto prev = path.front();
		for (const auto &tile : path)
		{
			const auto d = tile - prev;
			TEST_REQUIRE(std::abs(d.x) <= 1 && std::abs(d.y) <= 1 && std::abs(d.z) <= 1,
			             "route from {0} jumps from {1} to {2}", origin, prev, tile);
			prev = tile;
		}
		// Nothing past the far end leads on, so the route stops at the reachable tile nearest
		// the destination, which lies north-west: the junction end.
		TEST_CHECK(path.back() == roadTiles[0], "route from {0} should stop at {1}, got {2}",
		           origin, roadTiles[0], path.back());
	}
	// Leave the city as the other cases expect it.
	city->fillRoadSegmentMap(state);
	return true;
}

// City::update walks only the scenery that is collapsing or falling (City::activeScenery), not all
// of it. A collapse queued on a building part must still run: count down, then fall.
static bool test_queued_collapse_still_falls()
{
	auto &state = *g_state;
	auto city = state.cities["CITYMAP_HUMAN"];
	TEST_REQUIRE(city, "no CITYMAP_HUMAN city in gamestate");
	sp<Scenery> part;
	for (const auto &s : city->scenery)
	{
		if (s && s->tileObject && s->building && s->isAlive() && s->initialPosition.z >= 4)
		{
			part = s;
			break;
		}
	}
	TEST_REQUIRE(part, "no standing building part above floor 3 to collapse");
	part->queueCollapse();
	TEST_REQUIRE(part->inActiveList, "a queued collapse puts the part on the active list");
	bool fell = false;
	for (int tick = 0; tick < 200 && !fell; tick++)
	{
		city->update(state, 1);
		fell = part->falling || part->destroyed || !part->tileObject;
	}
	TEST_REQUIRE(fell, "a queued collapse never ran (ticks left {0})", part->ticksUntilCollapse);
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

	// Vehicle death/crash paths raise GameEvents and touch sound playback, which need a
	// Framework. No window required.
	Framework fw("OpenApoc", false);
	g_state = mksp<GameState>();
	if (!loadStartedGameState(*g_state, common, gamestate))
	{
		return EXIT_FAILURE;
	}

	const int rc = runTestSuite({
	    // First: the cases below sever the road at ROAD_ORIGIN, this one's destination.
	    {"route_from_road_connected_at_its_far_end", test_route_from_road_connected_at_its_far_end},
	    {"ground_vehicle_path_terminates_on_severed_road",
		 test_ground_vehicle_path_terminates_on_severed_road},
	    {"ground_vehicle_order_does_not_crash", test_ground_vehicle_order_does_not_crash},
	    {"queued_collapse_still_falls", test_queued_collapse_still_falls},
	});
	g_state.reset();
	return rc;
}
