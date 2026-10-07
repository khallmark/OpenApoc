// Coverage for game/state/city/city.cpp's dimension-portal behaviour:
//
//  - City::getNearestPortalIndex(), a small pure lookup added so a vehicle mission can pick
//    which dimension-gate doodad to head towards.
//  - City::generatePortals()'s new "linked positions" branch, which places a dimension-linked
//    city's portals at the same coordinates as CITYMAP_HUMAN's (falling back to a small
//    neighbourhood search, then to the pre-existing fully random placement) instead of always
//    generating them independently at random.
//
// This intentionally does not touch tests/test_city_rules.cpp (the monolithic city test file) -
// coverage for this slice lives in its own file per the mini-PR convention.

#include "framework/configfile.h"
#include "framework/framework.h"
#include "framework/logger.h"
#include "game/state/city/city.h"
#include "game/state/gamestate.h"
#include "game/state/gamestate_serialize.h"
#include "game/state/shared/doodad.h"
#include "game/state/tilemap/tilemap.h"
#include "game/state/tilemap/tileobject.h"
#include "library/strings.h"
#include "library/vec.h"
#include <cmath>
#include <iostream>
#include <set>
#include <vector>

namespace
{

// A dimension-linked city with a completely empty map: nothing occupies any tile, so
// City::canPlacePortal() succeeds at the very first position it is asked about. That makes the
// portal positions City::generatePortals() ends up with an exact, deterministic function of
// CITYMAP_HUMAN's portal positions, instead of depending on where real scenery/buildings happen
// to sit in the real alien city map.
OpenApoc::sp<OpenApoc::City> makeEmptyDimensionLinkedCity(OpenApoc::Vec3<int> size,
                                                          const OpenApoc::UString &id)
{
	auto city = OpenApoc::mksp<OpenApoc::City>();
	city->id = id;
	city->size = size;

	// Mirrors the layerMap City::initCity() builds its own TileMap with - it's file-local to
	// city.cpp so we can't reuse it directly, but its shape is dictated only by the
	// TileObject::Type values City ever adds to the map (Doodad, in this test).
	static const std::vector<std::set<OpenApoc::TileObject::Type>> layerMap = {
	    {OpenApoc::TileObject::Type::Scenery, OpenApoc::TileObject::Type::Doodad,
	     OpenApoc::TileObject::Type::Vehicle},
	    {OpenApoc::TileObject::Type::Projectile, OpenApoc::TileObject::Type::Shadow},
	};
	city->map = OpenApoc::mkup<OpenApoc::TileMap>(
	    size, VELOCITY_SCALE_CITY, OpenApoc::Vec3<int>{VOXEL_X_CITY, VOXEL_Y_CITY, VOXEL_Z_CITY},
	    layerMap);
	return city;
}

bool nearlyEqual(const OpenApoc::Vec3<float> &a, const OpenApoc::Vec3<float> &b, float eps = 0.01f)
{
	return std::abs(a.x - b.x) <= eps && std::abs(a.y - b.y) <= eps && std::abs(a.z - b.z) <= eps;
}

// City::getNearestPortalIndex needs no GameState/map at all - it only ever reads
// City::portals - so this is tested in complete isolation from the fixture-heavy generation
// test below.
bool test_get_nearest_portal_index()
{
	using namespace OpenApoc;
	LogInfo("Testing City::getNearestPortalIndex...");

	auto city = mksp<City>();
	city->id = "TEST_CITY_NEAREST_PORTAL_INDEX";

	if (city->getNearestPortalIndex({0.0f, 0.0f, 0.0f}) != -1)
	{
		LogError("Expected -1 from a city with no portals at all");
		return false;
	}

	auto p0 = mksp<Doodad>();
	p0->position = {10.0f, 10.0f, 5.0f};
	auto p1 = mksp<Doodad>();
	p1->position = {50.0f, 50.0f, 5.0f};
	auto p2 = mksp<Doodad>();
	p2->position = {90.0f, 10.0f, 5.0f};
	city->portals = {p0, p1, p2};

	struct Case
	{
		Vec3<float> query;
		int expected;
		const char *label;
	};
	const std::vector<Case> cases = {
	    {{10.0f, 10.0f, 5.0f}, 0, "exact match, portal 0"},
	    {{13.0f, 8.0f, 5.0f}, 0, "near portal 0"},
	    {{50.0f, 50.0f, 5.0f}, 1, "exact match, portal 1"},
	    {{55.0f, 48.0f, 5.0f}, 1, "near portal 1"},
	    {{90.0f, 10.0f, 5.0f}, 2, "exact match, portal 2"},
	    {{86.0f, 13.0f, 5.0f}, 2, "near portal 2"},
	    {{25.0f, 20.0f, 5.0f}, 0, "roughly between 0 and 1, closer to 0"},
	};
	for (auto &c : cases)
	{
		int got = city->getNearestPortalIndex(c.query);
		if (got != c.expected)
		{
			LogError("getNearestPortalIndex({0}) [{1}]: expected {2}, got {3}", c.query, c.label,
			         c.expected, got);
			return false;
		}
	}

	LogInfo("City::getNearestPortalIndex test passed");
	return true;
}

// City::generatePortals(), when asked to generate portals for a city that has no
// extractor-provided initial_portals of its own and isn't CITYMAP_HUMAN, should place its
// portals at CITYMAP_HUMAN's portal positions rather than picking independent random spots -
// that's the behaviour the removed
// "FIXME: Implement portals in alien city staying where they are and starting where they should
// (Need to be linked to portals in human city)" comment used to flag as unimplemented.
//
// NOTE: with the shipped base game data this branch is unreachable in practice - the alien city
// (CITYMAP_ALIEN) always has 3 fixed initial_portals from the original game's data, so it never
// falls into the "no initial_portals" branch this covers, and CITYMAP_HUMAN itself is excluded
// from linking to itself. This exercises the general-purpose behaviour directly (e.g. what a
// total-conversion mod's second alien-like city, or a future gameplay change, would hit) using a
// synthetic city standing in for that case.
bool test_generate_portals_links_to_human_city(OpenApoc::sp<OpenApoc::GameState> state)
{
	using namespace OpenApoc;
	LogInfo("Testing City::generatePortals links a dimension-linked city's portals to "
	        "CITYMAP_HUMAN's...");

	auto humanCity = state->cities["CITYMAP_HUMAN"];
	if (!humanCity)
	{
		LogError("CITYMAP_HUMAN missing from loaded gamestate");
		return false;
	}
	if (humanCity->portals.empty())
	{
		LogError("CITYMAP_HUMAN should already have portals from GameState::initState()");
		return false;
	}

	std::vector<Vec3<float>> humanPositions;
	for (auto &p : humanCity->portals)
	{
		humanPositions.push_back(p->getPosition());
	}

	auto testCity = makeEmptyDimensionLinkedCity(humanCity->size, "TEST_DIMENSION_LINKED_CITY");
	if (!testCity->initial_portals.empty())
	{
		LogError("Test fixture city should start with no initial_portals");
		return false;
	}

	testCity->generatePortals(*state);

	if (testCity->portals.size() != humanPositions.size())
	{
		LogError("Expected {0} linked portals (one per CITYMAP_HUMAN portal), got {1}",
		         humanPositions.size(), testCity->portals.size());
		return false;
	}

	for (size_t i = 0; i < testCity->portals.size(); i++)
	{
		Vec3<float> got = testCity->portals[i]->getPosition();
		Vec3<float> want = humanPositions[i];
		if (!nearlyEqual(got, want))
		{
			LogError("Linked portal {0} placed at {1}, expected exactly at CITYMAP_HUMAN's "
			         "portal {2}",
			         i, got, want);
			return false;
		}
	}

	// generatePortals() also records each placed linked position into initial_portals, so a
	// save/reload of a city that took this branch keeps its portals "staying where they are"
	// (the other half of the FIXME this replaces) via the pre-existing !initial_portals.empty()
	// branch above it.
	if (testCity->initial_portals.size() != humanPositions.size())
	{
		LogError("Expected generatePortals to record {0} initial_portals entries, got {1}",
		         humanPositions.size(), testCity->initial_portals.size());
		return false;
	}

	LogInfo("City::generatePortals linking test passed");
	return true;
}

} // namespace

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

	auto state = OpenApoc::mksp<OpenApoc::GameState>();
	LogInfo("Loading common gamestate \"{0}\"", common_name);
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

	bool ok = true;

	if (!test_get_nearest_portal_index())
	{
		LogError("test_get_nearest_portal_index failed");
		ok = false;
	}

	if (!test_generate_portals_links_to_human_city(state))
	{
		LogError("test_generate_portals_links_to_human_city failed");
		ok = false;
	}

	if (!ok)
	{
		return EXIT_FAILURE;
	}

	LogInfo("test_city_portals success - all tests passed");
	return EXIT_SUCCESS;
}
