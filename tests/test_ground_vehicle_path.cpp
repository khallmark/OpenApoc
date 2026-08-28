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
#include "game/state/city/city.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/gamestate.h"
#include "game/state/rules/city/vehicletype.h"
#include "game/state/shared/organisation.h"
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
	    {"ground_vehicle_path_terminates_on_severed_road",
	     test_ground_vehicle_path_terminates_on_severed_road},
	    {"ground_vehicle_order_does_not_crash", test_ground_vehicle_order_does_not_crash},
	});
	g_state.reset();
	return rc;
}
