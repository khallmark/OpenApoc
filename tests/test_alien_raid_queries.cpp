#include "framework/configfile.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/research.h"
#include "game/state/city/scenery.h"
#include "game/state/city/vehicle.h"
#include "game/state/gamestate.h"
#include "game/state/gamestateintrospect.h"
#include "game/state/rules/city/scenerytiletype.h"
#include "game/state/shared/organisation.h"
#include "game/state/tilemap/tilemap.h"
#include "library/strings_format.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static bool test_campaign_order_skips_destroyed_and_locked_buildings()
{
	GameState state;
	state.organisations["ORG_ALIEN"] = mksp<Organisation>();
	state.aliens = {&state, "ORG_ALIEN"};
	auto city = mksp<City>();
	state.cities["CITYMAP_ALIEN"] = city;
	state.current_city = {&state, "CITYMAP_ALIEN"};
	city->map = std::make_unique<TileMap>(
	    Vec3<int>{10, 1, 3}, Vec3<float>{1, 1, 1}, Vec3<int>{1, 1, 1},
	    std::vector<std::set<TileObject::Type>>{{TileObject::Type::Scenery}});
	auto type = mksp<SceneryTileType>();
	type->isBuildingPart = true;
	state.scenery_tile_types["CITYTILE_TEST"] = type;
	// Deliberately reverse storage order. The victory building must wait behind earlier targets.
	for (int number : {9, 2, 1, 0})
	{
		const auto id = format("BUILDING_TEST_{0}", number);
		const auto topicId = format("RESEARCH_ALIEN_BUILDING_{0}", number);
		auto topic = mksp<ResearchTopic>();
		topic->man_hours = 1;
		topic->man_hours_progress = number == 1 ? 0 : 1;
		state.research.topics[topicId] = topic;
		auto building = mksp<Building>();
		building->city = state.current_city;
		building->owner = state.aliens;
		building->accessTopic = {&state, topicId};
		building->victory = number == 9;
		const Vec3<int> part{number, 0, 2};
		building->buildingParts.insert(part);
		auto scenery = mksp<Scenery>();
		scenery->type = {&state, "CITYTILE_TEST"};
		city->map->getTile(part)->presentScenery = scenery;
		state.buildings[id] = building;
		city->buildings.emplace_back(&state, id);
	}
	TEST_REQUIRE(nextRaidableAlienBuilding(state).id == "BUILDING_TEST_0", "lowest open target");
	// Scenery::collapse calls this even though the building remains alien-owned.
	state.buildings["BUILDING_TEST_0"]->buildingPartChange(state, {0, 0, 2}, false);
	TEST_REQUIRE(!state.buildings["BUILDING_TEST_0"]->isAlive(), "collapsed building not alive");
	TEST_REQUIRE(state.buildings["BUILDING_TEST_0"]->owner == state.aliens, "owner stays alien");
	TEST_REQUIRE(nextRaidableAlienBuilding(state).id == "BUILDING_TEST_2",
	             "skip rubble and locked 1");
	const auto query = introspectGameState(state, "alien_buildings");
	TEST_REQUIRE(query.find("raidable=2") != UString::npos, "rubble excluded from count");
	TEST_REQUIRE(
	    query.find(
	        "BUILDING_TEST_0:topic=RESEARCH_ALIEN_BUILDING_0,open=1,alien=1,victory=0,alive=0") !=
	        UString::npos,
	    "report rubble without changing ownership or research");
	state.research.topics["RESEARCH_ALIEN_BUILDING_1"]->man_hours_progress = 1;
	TEST_REQUIRE(nextRaidableAlienBuilding(state).id == "BUILDING_TEST_1",
	             "numeric order after unlock");
	for (int number : {1, 2})
	{
		state.buildings[format("BUILDING_TEST_{0}", number)]->buildingPartChange(
		    state, {number, 0, 2}, false);
	}
	TEST_REQUIRE(nextRaidableAlienBuilding(state)->victory, "finally target gate generator");
	state.buildings["BUILDING_TEST_9"]->buildingPartChange(state, {9, 0, 2}, false);
	TEST_REQUIRE(!nextRaidableAlienBuilding(state), "all destroyed: no target");
	TEST_REQUIRE(introspectGameState(state, "alien_buildings").find("raidable=0") != UString::npos,
	             "all destroyed: zero raidable");
	return true;
}

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}
	applyDeterministicTestConfig();
	return runTestSuite(
	    {{"campaign_order", test_campaign_order_skips_destroyed_and_locked_buildings}});
}
