// Ground traffic must not gridlock, and must look like UFO2P's. The EXE moves road vehicles (kind
// 0) apart from flyers and ATVs (FUN_000303e4 -> FUN_00033818), on two-lane roads: a road vehicle
// is held up only by one going its way (FUN_00031a4c, FUN_00031be0) or, on a junction tile, by one
// whose way across crosses its own (the conflict table DAT_000e6a30); cars going the other way
// pass; a car stopped long enough on a straight turns round (FUN_00032428). Its civilian traffic is
// a fixed mix of eight vehicle types (FUN_00034860), not the cars and bikes alone that OpenApoc's
// hand-written organisation patterns sent. See
// docs/original-game/findings/ground-vehicle-occupancy.md.
//
// OpenApoc had one vehicle per road tile and no answer to it: on a long learner save every one of
// the city's 147 ground vehicles was frozen in queues ending at head-on pairs. This builds the
// shapes on real CITYMAP_HUMAN road and steps only those cars, the way GroundVehicleMover::update
// is driven every frame.

#include "framework/configfile.h"
#include "framework/framework.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/gamestate.h"
#include "game/state/rules/city/vehicletype.h"
#include "game/state/shared/organisation.h"
#include "game/state/tilemap/tilemap.h"
#include "game/state/tilemap/tileobject_vehicle.h"
#include "tests/test_helpers.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <vector>

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

namespace OpenApoc
{
extern uint64_t cityPathCalls;
}

static sp<GameState> g_state;

namespace
{

constexpr int TICKS_PER_STEP = 6; // city Speed4
constexpr int TEST_TICKS = 2400;
// The wait a boxed-in vehicle sits out before trying again (UFO2P's 12-count, 0x3a290).
constexpr int WAIT_TICKS = VehicleMission::BLOCKED_WAIT_TICKS;

bool sameRow(const RoadSegment &s)
{
	if (s.tilePosition.size() < 2)
	{
		return false;
	}
	const auto d = s.tilePosition[1] - s.tilePosition[0];
	for (size_t i = 1; i < s.tilePosition.size(); i++)
	{
		if (s.tilePosition[i] - s.tilePosition[i - 1] != d || d.z != 0 ||
		    std::abs(d.x) + std::abs(d.y) != 1)
		{
			return false;
		}
	}
	return true;
}

// A road tile of the segment connected at this segment's `end` (0 first tile, 1 last).
bool beyondEnd(const City &city, const RoadSegment &s, int end, Vec3<int> &out)
{
	const auto &tip = end == 0 ? s.getFirst() : s.getLast();
	for (int id : s.connections)
	{
		const auto &other = city.roadSegments[id];
		for (const auto &t : other.tilePosition)
		{
			const auto d = t - tip;
			if (std::abs(d.x) + std::abs(d.y) == 1 && d.z == 0)
			{
				// The junction tile next to the tip; aim a tile further along the way out of it.
				out = t + (t - tip);
				if (city.map->tileIsValid(out))
				{
					for (const auto &u : other.tilePosition)
					{
						if (u == out)
						{
							return true;
						}
					}
				}
				out = t;
				return true;
			}
		}
	}
	return false;
}

// A straight, flat, intact segment of 5..8 tiles with a junction (3+ connections) at an end, so a
// car that has to back out has somewhere to go within UFO2P's 8-step local planner.
const RoadSegment *findStraightRoadByJunction(const City &city)
{
	for (const auto &s : city.roadSegments)
	{
		if (!s.intact || s.length < 5 || s.length > 8 || !sameRow(s) || s.connections.size() < 2)
		{
			continue;
		}
		bool junction = false;
		for (int id : s.connections)
		{
			junction = junction || city.roadSegments[id].connections.size() >= 3;
		}
		Vec3<int> a, b;
		if (junction && beyondEnd(city, s, 0, a) && beyondEnd(city, s, 1, b))
		{
			return &s;
		}
	}
	return nullptr;
}

sp<Vehicle> carAt(GameState &state, City &city, Vec3<int> tile, Vec3<int> target)
{
	auto v = city.placeVehicle(state, {&state, "VEHICLETYPE_CIVILIAN_CAR"}, state.getPlayer(),
	                           Vec3<float>{tile.x + 0.5f, tile.y + 0.5f, tile.z + 0.5f}, 0.0f);
	if (v)
	{
		v->setMission(state, VehicleMission::gotoLocation(state, *v, target));
	}
	return v;
}

// A car with nowhere to go, standing on `tile` facing NESW `heading` (facing 0 is north).
sp<Vehicle> parkedCarAt(GameState &state, City &city, Vec3<int> tile, int heading)
{
	return city.placeVehicle(state, {&state, "VEHICLETYPE_CIVILIAN_CAR"}, state.getPlayer(),
	                         Vec3<float>{tile.x + 0.5f, tile.y + 0.5f, tile.z + 0.5f},
	                         static_cast<float>(heading * M_PI / 2.0));
}

void removeCars(GameState &state, std::initializer_list<sp<Vehicle>> cars)
{
	for (auto &v : cars)
	{
		if (v && v->tileObject)
		{
			v->die(state, true);
		}
	}
	state.cleanUpDeathNote();
}

Vec3<int> tileOf(const Vehicle &v) { return v.tileObject->getOwningTile()->position; }

bool test_head_on_pair_does_not_gridlock()
{
	auto &state = *g_state;
	auto city = state.current_city;
	TEST_REQUIRE(city, "no current city");
	const auto *seg = findStraightRoadByJunction(*city);
	TEST_REQUIRE(seg, "no straight road segment beside a junction in CITYMAP_HUMAN");
	Vec3<int> pastFirst, pastLast;
	beyondEnd(*city, *seg, 0, pastFirst);
	beyondEnd(*city, *seg, 1, pastLast);
	const auto &road = seg->tilePosition;
	LogInfo("road {0} .. {1} (length {2}); targets {3} and {4}", road.front(), road.back(),
	        seg->length, pastFirst, pastLast);

	// A heads for the far end, B for the near end: head-on on tiles 1 and 2. C queues behind A.
	auto a = carAt(state, *city, road[1], pastLast);
	auto b = carAt(state, *city, road[2], pastFirst);
	auto c = carAt(state, *city, road[0], pastLast);
	TEST_REQUIRE(a && b && c, "could not place the three cars");
	const auto aStart = tileOf(*a), bStart = tileOf(*b);

	const auto callsBefore = cityPathCalls;
	// The tiles each car of the pair has stood on, in order, and how often one went straight back.
	std::vector<Vec3<int>> aTiles{aStart}, bTiles{bStart};
	int reversals = 0;
	auto follow = [&](const sp<Vehicle> &v, std::vector<Vec3<int>> &tiles)
	{
		if (!v->tileObject || tileOf(*v) == tiles.back())
		{
			return;
		}
		if (tiles.size() >= 2 && tiles[tiles.size() - 2] == tileOf(*v))
		{
			reversals++;
		}
		tiles.push_back(tileOf(*v));
	};
	for (int t = 0; t < TEST_TICKS; t += TICKS_PER_STEP)
	{
		for (auto &v : {a, b, c})
		{
			if (v->tileObject)
			{
				v->update(state, TICKS_PER_STEP);
			}
		}
		follow(a, aTiles);
		follow(b, bTiles);
	}
	const auto calls = cityPathCalls - callsBefore;
	auto reached = [](const std::vector<Vec3<int>> &tiles, Vec3<int> p)
	{ return std::find(tiles.begin(), tiles.end(), p) != tiles.end(); };
	LogInfo("route calls over {0} ticks: {1}; tiles A {2}, B {3}; reversals {4}", TEST_TICKS,
	        calls, aTiles.size(), bTiles.size(), reversals);

	// 1. No spin: a blocked car does not re-request its route every step.
	const uint64_t bound = 3 * (TEST_TICKS / WAIT_TICKS) + 30;
	TEST_REQUIRE(calls <= bound, "blocked cars re-routed {0} times in {1} ticks (bound {2})",
	             calls, TEST_TICKS, bound);
	// 2. The pair gets past each other: each reaches the tile the other started on, or left the map
	// on the way.
	TEST_REQUIRE(!a->tileObject || reached(aTiles, bStart),
	             "A never got past B: its tiles were {0} .. {1}", aTiles.front(), aTiles.back());
	TEST_REQUIRE(!b->tileObject || reached(bTiles, aStart),
	             "B never got past A: its tiles were {0} .. {1}", bTiles.front(), bTiles.back());
	// 3. Without going back and forth to do it.
	TEST_REQUIRE(reversals == 0, "the head-on pair went straight back {0} times", reversals);
	for (auto &v : {a, b, c})
	{
		if (v->tileObject)
		{
			LogInfo("{0} ended at {1} ({2} mission(s))", v->name, tileOf(*v), v->missions.size());
		}
	}
	removeCars(state, {a, b, c});
	return true;
}

// Right-hand traffic: a road vehicle's goal is off the tile centre to the right of its way, by
// UFO2P's lane (straight trajectories at 20 and 11 of a 32-unit tile).
bool test_road_vehicles_keep_right()
{
	auto &state = *g_state;
	auto city = state.current_city;
	const auto *seg = findStraightRoadByJunction(*city);
	TEST_REQUIRE(seg, "no straight road segment beside a junction in CITYMAP_HUMAN");
	Vec3<int> pastLast;
	beyondEnd(*city, *seg, 1, pastLast);
	const auto &road = seg->tilePosition;
	auto car = carAt(state, *city, road[0], pastLast);
	TEST_REQUIRE(car, "could not place the car");
	car->update(state, TICKS_PER_STEP);
	// Where the step ends: the mover may first insert a waypoint to change height.
	const auto goal = car->goalWaypoints.empty() ? car->goalPosition : car->goalWaypoints.back();
	const auto d = road[1] - road[0];
	const auto centre = city->map->getTile(road[1])->getRestingPosition();
	const float side = (goal.x - centre.x) * -d.y + (goal.y - centre.y) * d.x;
	const float along = (goal.x - centre.x) * d.x + (goal.y - centre.y) * d.y;
	LogInfo("heading {0}: goal {1}, centre {2}, right of centre by {3}", d, goal, centre, side);
	removeCars(state, {car});
	TEST_REQUIRE(std::abs(side - VehicleMission::ROAD_LANE_OFFSET) < 0.01f &&
	                 std::abs(along) < 0.01f,
	             "goal {0} is not in the right-hand lane of tile {1}", side, road[1]);
	return true;
}

// A car stopped behind one that is going nowhere queues behind it -- it does not drive through
// it -- and once stopped for FUN_00032428's count, turns round and drives back the way it came.
bool test_queue_then_turn_round()
{
	auto &state = *g_state;
	auto city = state.current_city;
	const auto *seg = findStraightRoadByJunction(*city);
	TEST_REQUIRE(seg, "no straight road segment beside a junction in CITYMAP_HUMAN");
	Vec3<int> pastLast;
	beyondEnd(*city, *seg, 1, pastLast);
	const auto &road = seg->tilePosition;
	const int heading = VehicleMission::roadHeading(road[1], road[2]);
	TEST_REQUIRE(heading >= 0, "segment is not a row of tiles");
	auto stopped = parkedCarAt(state, *city, road[2], heading);
	auto car = carAt(state, *city, road[1], pastLast);
	TEST_REQUIRE(stopped && car, "could not place the cars");
	const auto callsBefore = cityPathCalls;
	int turnedAt = -1;
	bool enteredStoppedTile = false;
	constexpr int LIMIT = VehicleMission::ROAD_UTURN_TICKS + 600;
	for (int t = 0; t < LIMIT && turnedAt < 0; t += TICKS_PER_STEP)
	{
		car->update(state, TICKS_PER_STEP);
		if (!car->tileObject)
		{
			break;
		}
		enteredStoppedTile = enteredStoppedTile || tileOf(*car) == road[2];
		if (tileOf(*car) == road[0])
		{
			turnedAt = t;
		}
	}
	const auto calls = cityPathCalls - callsBefore;
	LogInfo("turned round after {0} ticks; {1} route calls", turnedAt, calls);
	removeCars(state, {stopped, car});
	TEST_REQUIRE(!enteredStoppedTile, "the car drove into the stopped car's tile");
	TEST_REQUIRE(turnedAt >= 0, "the car never turned round in {0} ticks", LIMIT);
	TEST_REQUIRE(turnedAt >= (int)VehicleMission::ROAD_UTURN_TICKS - 2 * TICKS_PER_STEP,
	             "the car turned round after {0} ticks, before UFO2P's {1}", turnedAt,
	             VehicleMission::ROAD_UTURN_TICKS);
	TEST_REQUIRE(calls <= 10, "a queued car re-routed {0} times", calls);
	return true;
}

// The civilian traffic mix. Every organisation used to send only Civilian Cars and Blazer Turbo
// Bikes (Megapol Police Cars), from patterns hand-written in the extractor, one naming a type that
// does not exist (VEHICLETYPE_AIRRANS). UFO2P sends Civilian Cars, Autotaxis, Blazer Turbo Bikes,
// Construction Vehicles, Airtaxis, Airtrans, Autotrans and Rescue Transports (FUN_00034860).
bool test_ambient_traffic_mix()
{
	auto &state = *g_state;
	auto city = state.current_city;
	const auto &ambient = City::ambientTrafficTypes();
	for (const auto &o : state.organisations)
	{
		for (const auto &cityMissions : o.second->recurring_missions)
		{
			for (const auto &m : cityMissions.second)
			{
				for (const auto &t : m.pattern.allowedTypes)
				{
					TEST_REQUIRE(t.id != "VEHICLETYPE_AIRRANS" &&
					                 std::find(ambient.begin(), ambient.end(), t.id) ==
					                     ambient.end(),
					             "{0} still schedules {1} trips itself", o.first, t.id);
				}
			}
		}
	}
	// A new game's organisations have not stocked their vehicle parks yet; the game does it
	// daily (GameState::updateEndOfDay).
	for (auto &o : state.organisations)
	{
		o.second->updateVehicleAgentPark(state);
	}
	std::map<UString, int> sent;
	auto idle = [&]()
	{
		std::set<UString> ids;
		for (const auto &v : state.vehicles)
		{
			if (v.second->missions.empty() && v.second->currentBuilding)
			{
				ids.insert(v.first);
			}
		}
		return ids;
	};
	int total = 0;
	for (int batch = 0; batch < 30; batch++)
	{
		const auto before = idle();
		total += city->dispatchAmbientTraffic(state);
		const auto after = idle();
		for (const auto &id : before)
		{
			if (!after.count(id))
			{
				sent[state.vehicles[id]->type.id]++;
			}
		}
	}
	UString mix;
	for (const auto &e : sent)
	{
		mix += format(" {0}={1}", e.first, e.second);
	}
	LogInfo("sent {0}:{1}", total, mix);
	TEST_REQUIRE(total > 0, "no traffic sent");
	for (const auto &t : sent)
	{
		TEST_REQUIRE(std::find(ambient.begin(), ambient.end(), t.first) != ambient.end(),
		             "sent a {0}, which is not civilian traffic", t.first);
	}
	for (const char *t : {"VEHICLETYPE_CIVILIAN_CAR", "VEHICLETYPE_AUTOTAXI",
	                      "VEHICLETYPE_BLAZER_TURBO_BIKE", "VEHICLETYPE_AIRTAXI",
	                      "VEHICLETYPE_CONSTRUCTION_VEHICLE", "VEHICLETYPE_RESCUE_TRANSPORT"})
	{
		TEST_REQUIRE(sent[t] > 0, "no {0} in {1} trips", t, total);
	}
	return true;
}

// Vehicles are drawn between where they were when the latest step began and where they are now,
// by how far the clock has got to the next step, so they glide at the display's rate while the
// simulation keeps the original's ticks; a jump is drawn where it lands.
bool test_draw_position_interpolates_between_steps()
{
	Vehicle v;
	v.position = {10.0f, 20.0f, 2.0f};
	TEST_REQUIRE(v.getDrawPosition(0.5f) == v.position, "no step recorded: drawn where it is");
	v.stepStartPosition = {9.5f, 20.0f, 2.0f};
	v.hasStepStart = true;
	TEST_REQUIRE(v.getDrawPosition(0.0f) == v.stepStartPosition, "step start");
	TEST_REQUIRE(v.getDrawPosition(1.0f) == v.position, "step end");
	TEST_REQUIRE(v.getDrawPosition(0.5f) == (Vec3<float>{9.75f, 20.0f, 2.0f}), "half way");
	v.stepStartPosition = {1.0f, 1.0f, 2.0f};
	TEST_REQUIRE(v.getDrawPosition(0.5f) == v.position, "a jump is not slid across the map");
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
	g_state = mksp<GameState>();
	if (!loadStartedGameState(*g_state, common, gamestate))
	{
		return EXIT_FAILURE;
	}
	const int rc = runTestSuite({
	    {"head_on_pair_does_not_gridlock", test_head_on_pair_does_not_gridlock},
	    {"road_vehicles_keep_right", test_road_vehicles_keep_right},
	    {"queue_then_turn_round", test_queue_then_turn_round},
	    // Last: it sends parked vehicles off on trips.
	    {"ambient_traffic_mix", test_ambient_traffic_mix},
	    {"draw_position_interpolates_between_steps", test_draw_position_interpolates_between_steps},
	});
	g_state.reset();
	return rc;
}
