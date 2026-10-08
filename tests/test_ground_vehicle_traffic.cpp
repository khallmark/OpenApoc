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
#include "framework/filesystem.h"
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
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <unistd.h>
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
	LogInfo("route calls over {0} ticks: {1}; tiles A {2}, B {3}; reversals {4}", TEST_TICKS, calls,
	        aTiles.size(), bTiles.size(), reversals);

	// 1. No spin: a blocked car does not re-request its route every step.
	const uint64_t bound = 3 * (TEST_TICKS / WAIT_TICKS) + 30;
	TEST_REQUIRE(calls <= bound, "blocked cars re-routed {0} times in {1} ticks (bound {2})", calls,
	             TEST_TICKS, bound);
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

// This fixture omits fillOrgStartingProperty, unlike a normal UI new game. UFO2P creates
// civilian trips itself, so an unstocked or depleted city must still generate traffic. Step the
// real CITYMAP_HUMAN roads for six hours to cover replenishment after completed journeys.
bool test_ambient_traffic_replenishes_without_parked_fleet()
{
	auto &state = *g_state;
	auto city = state.current_city;
	TEST_REQUIRE(city, "no current city");
	const auto &ambient = City::ambientTrafficTypes();
	const auto player = state.getPlayer();
	std::set<UString> existing;
	int parked = 0;
	for (const auto &[id, v] : state.vehicles)
	{
		existing.insert(id);
		if (v->owner != player && v->currentBuilding &&
		    std::find(ambient.begin(), ambient.end(), v->type.id) != ambient.end())
		{
			parked++;
		}
	}
	TEST_REQUIRE(parked == 0, "fresh-city fixture unexpectedly has {0} parked civilian vehicles",
	             parked);
	std::array<int, 6> sentPerHour{};
	sentPerHour[0] = city->dispatchAmbientTraffic(state);
	TEST_REQUIRE(sentPerHour[0] > 0,
	             "civilian traffic cannot start without a purchased vehicle park: sent {0} trips",
	             sentPerHour[0]);
	StateRef<Building> fleetBuilding;
	for (const auto &b : city->buildings)
	{
		if (b && b->owner && b->owner != player && b->owner != state.getAliens() &&
		    b->carEntranceLocation.x >= 0 && city->map->tileIsValid(b->carEntranceLocation))
		{
			fleetBuilding = b;
			break;
		}
	}
	TEST_REQUIRE(fleetBuilding, "no building for a permanent civilian fleet vehicle");
	auto permanent = city->placeVehicle(state, {&state, "VEHICLETYPE_CIVILIAN_CAR"},
	                                    fleetBuilding->owner, fleetBuilding);
	TEST_REQUIRE(permanent, "could not create the permanent fleet vehicle");
	const auto permanentId = Vehicle::getId(state, permanent);
	existing.insert(permanentId);
	const auto startTicks = state.gameTime.getTicks();
	int legArrivals = 0;
	int nonterminalArrivals = 0;
	int completedRetiredTrips = 0;
	size_t maxTransient = 0;
	std::set<UString> launched;
	for (unsigned int elapsed = 0; elapsed < 6 * TICKS_PER_HOUR; elapsed += TICKS_PER_STEP)
	{
		// Vehicles can finish trips and be removed while stepping: hold the current batch alive.
		std::vector<sp<Vehicle>> vehicles;
		for (const auto &[id, v] : state.vehicles)
		{
			if (!existing.count(id) && v->city == city)
			{
				vehicles.push_back(v);
				if (v->tileObject)
				{
					launched.insert(id);
				}
			}
		}
		for (const auto &v : vehicles)
		{
			const bool wasOnMap = v->tileObject != nullptr;
			const auto doodadsBefore = city->doodads.size();
			v->update(state, TICKS_PER_STEP);
			if (wasOnMap && !v->tileObject && v->currentBuilding)
			{
				legArrivals++;
				if (v->isDead())
				{
					completedRetiredTrips++;
					TEST_CHECK(v->missions.empty(),
					           "a civilian vehicle retired before its return trip completed");
				}
				else
				{
					nonterminalArrivals++;
					TEST_CHECK(!v->missions.empty(),
					           "a civilian vehicle remained parked after its final leg arrived");
				}
				TEST_CHECK(city->doodads.size() == doodadsBefore,
				           "a civilian trip created a doodad when it arrived");
			}
		}
		state.cleanUpDeathNote();
		if (GameTime::intervalsCrossed(state.gameTime.getTicks(), TICKS_PER_STEP,
		                               City::AMBIENT_TRAFFIC_TICKS))
		{
			sentPerHour[elapsed / TICKS_PER_HOUR] += city->dispatchAmbientTraffic(state);
		}
		maxTransient = std::max(maxTransient, state.vehicles.size() - existing.size());
		state.gameTime.addTicks(TICKS_PER_STEP);
	}
	const auto receipt =
	    format("six-hour civilian traffic: {0}, {1}, {2}, {3}, {4}, {5} trips; {6} launched, "
	           "{7} leg arrivals ({8} nonterminal), {9} completed retired trips, "
	           "at most {10} pending or active",
	           sentPerHour[0], sentPerHour[1], sentPerHour[2], sentPerHour[3], sentPerHour[4],
	           sentPerHour[5], launched.size(), legArrivals, nonterminalArrivals,
	           completedRetiredTrips, maxTransient);
	LogInfo("{0}", receipt);
	std::cout << receipt.c_str() << '\n';
	TEST_CHECK(!permanent->isDead() && permanent->currentBuilding == fleetBuilding &&
	               permanent->missions.empty(),
	           "ambient traffic borrowed or removed a permanently owned fleet vehicle");
	for (const auto &b : city->buildings)
	{
		for (const auto &v : b->currentVehicles)
		{
			TEST_CHECK(state.vehicles.count(v.id),
			           "building {0} still references retired vehicle {1}", b.id, v.id);
		}
	}
	std::vector<sp<Vehicle>> remaining;
	for (const auto &[id, v] : state.vehicles)
	{
		if (!existing.count(id))
		{
			remaining.push_back(v);
		}
	}
	for (const auto &v : remaining)
	{
		v->die(state, true);
	}
	state.cleanUpDeathNote();
	permanent->die(state, true);
	state.cleanUpDeathNote();
	state.gameTime = GameTime(startTicks);
	for (size_t hour = 0; hour < sentPerHour.size(); hour++)
	{
		TEST_CHECK(sentPerHour[hour] > 0, "civilian traffic stopped replenishing in hour {0}",
		           hour);
	}
	TEST_CHECK(!launched.empty(), "civilian trips were scheduled but none launched");
	TEST_CHECK(legArrivals > 0, "civilian traffic never arrived at a building in six hours");
	TEST_CHECK(completedRetiredTrips > 0,
	           "civilian traffic never completed and retired a trip in six hours");
	TEST_CHECK(maxTransient <= 35,
	           "civilian population exceeded its slot cap: {0} pending or active", maxTransient);
	return true;
}

// Civilian trips are temporary city traffic, not an organisation's purchased fleet. Exercise both
// purchase paths with one temporary car already owned, so it cannot satisfy the permanent quota.
bool test_ambient_traffic_does_not_replace_permanent_fleet()
{
	auto &state = *g_state;
	auto city = state.current_city;
	TEST_REQUIRE(city, "no current city");
	StateRef<Building> source;
	for (const auto &b : city->buildings)
	{
		if (b && b->owner && b->owner != state.getPlayer() && b->owner != state.getAliens() &&
		    b->owner->exeOrgIndex >= 2 && b->owner->exeOrgIndex < 27)
		{
			source = b;
			break;
		}
	}
	TEST_REQUIRE(source, "no organisation building for the fleet purchase test");
	const StateRef<VehicleType> carType{&state, "VEHICLETYPE_CIVILIAN_CAR"};
	auto org = source->owner;
	auto permanentCount = [&]()
	{
		int count = 0;
		for (const auto &[id, v] : state.vehicles)
		{
			if (v->owner == org && v->type == carType && !v->isDead() && !v->ambientTraffic)
			{
				count++;
			}
		}
		return count;
	};
	TEST_REQUIRE(permanentCount() == 0, "fleet fixture already owns permanent civilian cars");
	auto priceEntry = state.economy.find(carType.id);
	TEST_REQUIRE(priceEntry != state.economy.end() && priceEntry->second.currentPrice > 0,
	             "fleet fixture has no civilian car purchase price");
	std::set<UString> existing;
	for (const auto &[id, v] : state.vehicles)
	{
		existing.insert(id);
	}
	auto temporary = city->placeVehicle(state, carType, org, source);
	TEST_REQUIRE(temporary, "could not create the temporary fleet test car");
	temporary->ambientTraffic = true;
	const auto oldPark = org->vehiclePark;
	const auto oldBalance = org->balance;
	const auto oldWeight = org->parkBudgetWeight;
	const auto oldTable = state.vehicleParkSpawnTable;
	const auto oldCaps = state.vehicleParkSpawnCap;
	const int fundedBalance = priceEntry->second.currentPrice * 10;
	org->vehiclePark = {{carType, 1}};
	org->balance = fundedBalance;
	state.vehicleParkSpawnTable.clear();
	org->updateVehicleAgentPark(state);
	TEST_CHECK(permanentCount() == 1,
	           "legacy fleet purchase counted a temporary car as permanent fleet stock");
	std::vector<sp<Vehicle>> purchased;
	for (const auto &[id, v] : state.vehicles)
	{
		if (!existing.count(id) && v.get() != temporary.get())
		{
			purchased.push_back(v);
		}
	}
	for (const auto &v : purchased)
	{
		v->die(state, true);
	}
	state.cleanUpDeathNote();
	// A homogeneous pool makes the draw deterministic: there is always an affordable car to buy,
	// and its cap of one applies to permanent fleet stock even with a temporary car already owned.
	state.vehicleParkSpawnTable.assign(40, carType);
	state.vehicleParkSpawnCap = {{carType.id, 1}};
	org->vehiclePark.clear();
	org->balance = fundedBalance;
	org->parkBudgetWeight = 100;
	org->buyFromParkSpawnTable(state);
	TEST_CHECK(permanentCount() == 1,
	           "table fleet purchase counted a temporary car against the permanent fleet cap");
	TEST_CHECK(!temporary->isDead(), "fleet purchases removed the temporary civilian car");
	std::vector<sp<Vehicle>> cleanup;
	for (const auto &[id, v] : state.vehicles)
	{
		if (!existing.count(id))
		{
			cleanup.push_back(v);
		}
	}
	for (const auto &v : cleanup)
	{
		v->die(state, true);
	}
	state.cleanUpDeathNote();
	org->vehiclePark = oldPark;
	org->balance = oldBalance;
	org->parkBudgetWeight = oldWeight;
	state.vehicleParkSpawnTable = oldTable;
	state.vehicleParkSpawnCap = oldCaps;
	return true;
}

// Recorded pad locations remain on a building after its pads are destroyed. Dispatch must inspect
// the actual tiles, and a temporary flyer queued before the destruction must release its slot.
bool test_ambient_traffic_handles_destroyed_landing_pads()
{
	auto &state = *g_state;
	auto city = state.current_city;
	TEST_REQUIRE(city, "no current city");
	std::vector<StateRef<Building>> airBuildings;
	for (const auto &b : city->buildings)
	{
		if (b && b->owner && b->owner != state.getPlayer() && b->owner != state.getAliens() &&
		    !b->landingPadLocations.empty())
		{
			airBuildings.push_back(b);
		}
	}
	TEST_REQUIRE(airBuildings.size() >= 2, "no pair of civilian buildings with landing pads");
	std::map<Tile *, sp<Scenery>> pads;
	for (const auto &b : city->buildings)
	{
		for (const auto &p : b->landingPadLocations)
		{
			if (city->map->tileIsValid(p))
			{
				auto *tile = city->map->getTile(p);
				pads.emplace(tile, tile->presentScenery);
			}
		}
	}
	TEST_REQUIRE(!pads.empty(), "no actual landing pad tiles in the city");
	std::set<UString> existing;
	for (const auto &[id, v] : state.vehicles)
	{
		existing.insert(id);
	}
	const auto source = airBuildings.front(), destination = airBuildings.back();
	const StateRef<VehicleType> taxiType{&state, "VEHICLETYPE_AIRTAXI"};
	auto temporary = city->placeVehicle(state, taxiType, source->owner, source);
	auto permanent = city->placeVehicle(state, taxiType, source->owner, source);
	TEST_REQUIRE(temporary && permanent, "could not place the queued flyers");
	temporary->ambientTraffic = true;
	// Hold departures before damaging the pads, as if another launch is still using the exit.
	temporary->setMission(state, VehicleMission::snooze(state, *temporary, TICKS_PER_MINUTE));
	temporary->addMission(state, VehicleMission::gotoBuilding(state, *temporary, destination),
	                      true);
	permanent->setMission(state, VehicleMission::snooze(state, *permanent, TICKS_PER_MINUTE));
	permanent->addMission(state, VehicleMission::gotoBuilding(state, *permanent, destination),
	                      true);
	for (const auto &[tile, scenery] : pads)
	{
		tile->presentScenery.reset();
	}
	TEST_CHECK(!temporary->tileObject && temporary->currentBuilding == source,
	           "the temporary flyer was not queued at its damaged source");
	const auto doodadsBefore = city->doodads.size();
	temporary->update(state, TICKS_PER_STEP);
	permanent->update(state, TICKS_PER_STEP);
	TEST_CHECK(temporary->isDead(),
	           "a queued civilian flyer kept its slot after its pads were lost");
	TEST_CHECK(!permanent->isDead() && permanent->currentBuilding == source,
	           "lost landing pads retired a permanently owned flyer");
	TEST_CHECK(city->doodads.size() == doodadsBefore, "retiring the queued flyer created a doodad");
	state.cleanUpDeathNote();
	int dispatched = 0, invalidFlyers = 0;
	for (int batch = 0; batch < 40; batch++)
	{
		dispatched += city->dispatchAmbientTraffic(state);
		std::vector<sp<Vehicle>> trips;
		for (const auto &[id, v] : state.vehicles)
		{
			if (!existing.count(id) && v->ambientTraffic)
			{
				trips.push_back(v);
				if (!v->type->isGround())
				{
					invalidFlyers++;
				}
			}
		}
		for (const auto &v : trips)
		{
			v->die(state, true);
		}
		state.cleanUpDeathNote();
	}
	for (const auto &[tile, scenery] : pads)
	{
		tile->presentScenery = scenery;
	}
	permanent->die(state, true);
	state.cleanUpDeathNote();
	TEST_CHECK(dispatched > 0,
	           "destroyed landing pads prevented intact road traffic from starting");
	TEST_CHECK(invalidFlyers == 0, "dispatched {0} civilian flyers without any intact landing pad",
	           invalidFlyers);
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
	// The test helper omits the normal UI's fillOrgStartingProperty. Stock this mix fixture.
	for (auto &o : state.organisations)
	{
		o.second->updateVehicleAgentPark(state);
	}
	std::map<UString, int> sent;
	auto dispatched = [&]()
	{
		std::set<UString> ids;
		for (const auto &v : state.vehicles)
		{
			if (!v.second->missions.empty() &&
			    std::find(ambient.begin(), ambient.end(), v.second->type.id) != ambient.end())
			{
				ids.insert(v.first);
			}
		}
		return ids;
	};
	int total = 0;
	for (int batch = 0; batch < 30; batch++)
	{
		const auto before = dispatched();
		total += city->dispatchAmbientTraffic(state);
		const auto after = dispatched();
		for (const auto &id : after)
		{
			if (!before.count(id))
			{
				sent[state.vehicles[id]->type.id]++;
				// The long-running test covers real arrivals. Release this batch's slots so the mix
				// sample is not limited to whichever types happen to fill the first 35 slots.
				state.vehicles[id]->die(state, true);
			}
		}
		state.cleanUpDeathNote();
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

// The retirement marker must survive a save; vehicles in older saves remain permanent by default.
bool test_ambient_traffic_marker_roundtrips()
{
	const auto path =
	    fs::temp_directory_path() / format("openapoc-traffic-marker-{0}.save", getpid());
	struct SaveCleanup
	{
		fs::path path;
		~SaveCleanup()
		{
			std::error_code ignored;
			fs::remove(path, ignored);
		}
	} cleanup{path};
	GameState original;
	original.vehicles["VEHICLE_TRANSIENT_TEST"] = mksp<Vehicle>();
	original.vehicles["VEHICLE_TRANSIENT_TEST"]->ambientTraffic = true;
	original.vehicles["VEHICLE_PERMANENT_TEST"] = mksp<Vehicle>();
	TEST_REQUIRE(original.saveGame(path.string()), "could not save the civilian marker fixture");
	GameState restored;
	TEST_REQUIRE(restored.loadGame(path.string()), "could not reload the civilian marker fixture");
	TEST_REQUIRE(restored.vehicles.count("VEHICLE_TRANSIENT_TEST") &&
	                 restored.vehicles.count("VEHICLE_PERMANENT_TEST"),
	             "saving civilian traffic lost a vehicle record");
	TEST_CHECK(restored.vehicles["VEHICLE_TRANSIENT_TEST"]->ambientTraffic,
	           "saving a civilian trip lost its retirement marker");
	// The default false member is omitted by the generated writer, matching older vehicle records.
	TEST_CHECK(!restored.vehicles["VEHICLE_PERMANENT_TEST"]->ambientTraffic,
	           "a vehicle without a retirement marker became transient traffic");
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
	    {"ambient_traffic_replenishes_without_parked_fleet",
	     test_ambient_traffic_replenishes_without_parked_fleet},
	    {"ambient_traffic_marker_roundtrips", test_ambient_traffic_marker_roundtrips},
	    {"ambient_traffic_does_not_replace_permanent_fleet",
	     test_ambient_traffic_does_not_replace_permanent_fleet},
	    {"ambient_traffic_handles_destroyed_landing_pads",
	     test_ambient_traffic_handles_destroyed_landing_pads},
	    // Last: it leaves generated trips pending.
	    {"ambient_traffic_mix", test_ambient_traffic_mix},
	    {"draw_position_interpolates_between_steps", test_draw_position_interpolates_between_steps},
	});
	g_state.reset();
	return rc;
}
