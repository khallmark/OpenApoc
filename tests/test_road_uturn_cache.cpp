// A later U-turn candidate can clear the route cache that held the already-selected route.
// Exercise that rollover on real city roads and verify the first, shortest candidate survives.
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
#include "tests/test_helpers.h"
#include <algorithm>
#include <array>
#include <vector>

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

namespace
{
constexpr std::array<Vec3<int>, 4> directions = {{{0, -1, 0}, {1, 0, 0}, {0, 1, 0}, {-1, 0, 0}}};

const std::vector<bool> *connections(const Tile &tile)
{
	const auto &scenery = tile.presentScenery;
	return scenery && scenery->type->tile_type == SceneryTileType::TileType::Road &&
	               scenery->type->connection.size() >= 4
	           ? &scenery->type->connection
	           : nullptr;
}

struct Fixture
{
	Tile *from = nullptr;
	Vec3<int> junction;
	int heading = -1;
	std::vector<Vec3<int>> exits, shortestRoute;
};

bool findFixture(City &city, Fixture &fixture)
{
	const GroundVehicleTileHelper road{*city.map, VehicleType::Type::Road};
	for (const auto &segment : city.roadSegments)
	{
		for (const auto &junction : segment.tilePosition)
		{
			auto *at = city.map->getTile(junction);
			const auto *jc = connections(*at);
			if (!jc || std::count(jc->begin(), jc->begin() + 4, true) < 3)
			{
				continue;
			}
			for (int heading = 0; heading < 4; heading++)
			{
				const auto position = junction + directions[heading];
				if (!city.map->tileIsValid(position))
				{
					continue;
				}
				auto *from = city.map->getTile(position);
				const auto *c = connections(*from);
				if (!c || !(*c)[heading] || !(*c)[(heading + 2) % 4] || (*c)[(heading + 1) % 4] ||
				    (*c)[(heading + 3) % 4] || !road.canEnterTile(from, at))
				{
					continue;
				}
				std::vector<Vec3<int>> exits;
				for (int d = 0; d < 4; d++)
				{
					if (!(*jc)[d] || d == heading)
					{
						continue;
					}
					for (int dz : {0, 1, -1})
					{
						const auto p = junction + directions[d] + Vec3<int>{0, 0, dz};
						if (city.map->tileIsValid(p) && road.canEnterTile(at, city.map->getTile(p)))
						{
							exits.push_back(p);
							break;
						}
					}
				}
				if (exits.size() < 2)
				{
					continue;
				}
				// The first candidate is the destination itself. All later exits must also have
				// valid routes, but be longer, so overwriting the winner cannot mask invalidation.
				const auto first = city.findShortestPathUncached(exits.front(), exits.front());
				bool firstIsBest = !first.empty() && first.back() == exits.front();
				for (size_t i = 1; i < exits.size() && firstIsBest; i++)
				{
					const auto route = city.findShortestPathUncached(exits[i], exits.front());
					firstIsBest = !route.empty() && route.back() == exits.front() &&
					              route.size() > first.size();
				}
				if (firstIsBest)
				{
					fixture = {from, junction, heading, exits, {first.begin(), first.end()}};
					return true;
				}
			}
		}
	}
	return false;
}

bool test_selected_route_survives_cache_rollover(GameState &state)
{
	auto city = state.current_city;
	TEST_REQUIRE(city && city->map, "no initialized city map");
	Fixture fixture;
	TEST_REQUIRE(findFixture(*city, fixture),
	             "no real junction with two reachable exits and the first candidate shortest");
	city->routeCache.clear();
	// Actual packed route keys use only 60 bits. These noncolliding filler entries bring the
	// cache exactly to its bound: the first miss inserts entry 100001; the next miss clears it.
	constexpr uint64_t fillerKey = uint64_t{1} << 63;
	for (uint64_t i = 0; i < 100000; i++)
	{
		city->routeCache.emplace(fillerKey | i, std::vector<Vec3<int>>{});
	}
	TEST_REQUIRE(city->routeCache.size() == 100000, "cache fixture is not at the rollover bound");
	Vehicle vehicle;
	vehicle.name = "cache rollover car";
	vehicle.type = {&state, "VEHICLETYPE_CIVILIAN_CAR"};
	vehicle.city = city;
	VehicleMission mission;
	mission.currentPlannedPath = {fixture.from->position, fixture.exits.front()};
	TEST_REQUIRE(mission.roadUTurn(vehicle, fixture.from, fixture.heading),
	             "the straight road did not permit a U-turn");
	TEST_CHECK(city->routeCache.size() == fixture.exits.size() - 1,
	           "later candidate did not clear the original cached route: {0} entries remain",
	           city->routeCache.size());
	TEST_CHECK(!city->routeCache.count(fillerKey), "the route cache never rolled over");
	std::vector<Vec3<int>> expected{fixture.from->position, fixture.junction,
	                                fixture.exits.front()};
	expected.insert(expected.end(), fixture.shortestRoute.begin(), fixture.shortestRoute.end());
	const std::vector<Vec3<int>> actual(mission.currentPlannedPath.begin(),
	                                    mission.currentPlannedPath.end());
	TEST_CHECK(
	    actual == expected,
	    "cache rollover changed the selected shortest U-turn route ({0} tiles, expected {1})",
	    actual.size(), expected.size());
	city->routeCache.clear();
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
	if (!loadStartedGameState(state, common, gamestate))
	{
		return EXIT_FAILURE;
	}
	return test_selected_route_survives_cache_rollover(state) && !testCheckFailed ? EXIT_SUCCESS
	                                                                              : EXIT_FAILURE;
}
