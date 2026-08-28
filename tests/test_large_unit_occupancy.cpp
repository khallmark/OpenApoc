// Regression lock for OPE-12's large-unit occupancy fix in
// game/state/tilemap/tileobject_battleunit.cpp (TileObjectBattleUnit::setPosition()).
//
// A large unit ("bodyType->large", e.g. Megaspawn/Multiworm) is supposed to occupy the union of
// its current 2x2x2 block and, while mid-move (!atGoal), its destination 2x2x2 block. As found,
// the destination-block insertion dropped the block's own corner tile (the raw `pos` computed
// from `u->goalPosition`), inserting only the seven `pos` offsets around it. The fix is the single
// added `occupiedTiles.insert(pos);` line right after `pos = u->goalPosition;`. Any other file
// touched here is only test scaffolding.
//
// Campaign PR 39 (OPE-9, game/state/battle/battleunit.cpp/.h) explicitly declined to lock this:
// see tests/test_battle_large_unit.cpp's header comment and its
// test_large_unit_occupies_block()'s doc comment, which both point at this exact bug and this
// exact file as out of scope for that PR. This file is the completion of that deferral.
//
// This test intentionally does not include "tests/test_helpers.h": that shared seam is only
// introduced by a sibling, not-yet-merged campaign PR (kevinhallmark/ope-6, establishing
// per-slice test auto-discovery), and this file's own unit boundary is
// game/state/tilemap/tileobject_battleunit.cpp plus this one new test file. The small
// TEST_CHECK/TEST_REQUIRE/runTestSuite/gamestate-loading helpers below -- and the fixture-building
// helpers (findAgentType, ensureUnitAssetsLoaded, makeRealBattle, carveTwoRoomArena) -- are
// therefore self-contained duplicates of the shape used by tests/test_battle_large_unit.cpp,
// scoped to this file only. Until the ope-6 seam lands, this test source is not registered with
// ctest at all (see tests/test_large_unit_occupancy.cmake and the PR body).
//
// Like test_battle_large_unit.cpp, this uses a real, procedurally generated Battle
// (Battle::beginBattle()/enterBattle() from an actual UFO's battle_map) rather than a hand-built
// TileMap: Battle::spawnUnit() needs a unit's image/animation packs loaded and a populated
// ruleset to draw agent types from. The two-room arena carved into the generated map keeps the
// geometry actually under test (a large unit's footprint) fully deterministic regardless of what
// the random map contains, by keeping both the start and goal tiles on plain open floor well
// inside one carved room -- no pathing or line-of-sight is exercised here at all.

#include "framework/configfile.h"
#include "framework/framework.h"
#include "framework/logger.h"
#include "game/state/battle/battle.h"
#include "game/state/battle/battleunit.h"
#include "game/state/city/vehicle.h"
#include "game/state/gamestate.h"
#include "game/state/rules/agenttype.h"
#include "game/state/rules/battle/battlemap.h"
#include "game/state/rules/battle/battleunitanimationpack.h"
#include "game/state/rules/battle/battleunitimagepack.h"
#include "game/state/rules/city/vehicletype.h"
#include "game/state/tilemap/tile.h"
#include "game/state/tilemap/tilemap.h"
#include "game/state/tilemap/tileobject_battleunit.h"
#include <cstdlib>
#include <set>
#include <utility>
#include <vector>

using namespace OpenApoc;

namespace
{

// ---------------------------------------------------------------------------------------------
// Minimal self-contained test scaffolding (deliberately not tests/test_helpers.h -- see the file
// header comment).
// ---------------------------------------------------------------------------------------------

thread_local bool testCheckFailed = false;

#define TEST_CHECK(cond, ...)                                                                      \
	do                                                                                             \
	{                                                                                              \
		if (!(cond))                                                                               \
		{                                                                                          \
			LogError(__VA_ARGS__);                                                                 \
			testCheckFailed = true;                                                                \
		}                                                                                          \
	} while (0)

#define TEST_REQUIRE(cond, ...)                                                                    \
	do                                                                                             \
	{                                                                                              \
		if (!(cond))                                                                               \
		{                                                                                          \
			LogError(__VA_ARGS__);                                                                 \
			return false;                                                                          \
		}                                                                                          \
	} while (0)

void applyDeterministicTestConfig()
{
	config().set("OpenApoc.NewFeature.SeedRng", false);
	config().set("Config.Save", false);
}

bool loadStartedGameState(GameState &state, const UString &common, const UString &gamestate)
{
	if (!state.loadGame(common))
	{
		LogError("Failed to load common gamestate \"{0}\"", common);
		return false;
	}
	if (!state.loadGame(gamestate))
	{
		LogError("Failed to load gamestate \"{0}\"", gamestate);
		return false;
	}
	state.startGame();
	state.initState();
	state.fillPlayerStartingProperty();
	return true;
}

int runTestSuite(const std::vector<std::pair<const char *, bool (*)()>> &tests)
{
	bool anyFailed = false;
	for (const auto &t : tests)
	{
		testCheckFailed = false;
		LogInfo("Running {0}", t.first);
		const bool ok = t.second();
		if (!ok || testCheckFailed)
		{
			LogError("FAILED {0}", t.first);
			anyFailed = true;
		}
		else
		{
			LogInfo("PASSED {0}", t.first);
		}
	}
	return anyFailed ? EXIT_FAILURE : EXIT_SUCCESS;
}

sp<GameState> g_state;

// ---------------------------------------------------------------------------------------------
// Shared fixture helpers (same shape as tests/test_battle_large_unit.cpp)
// ---------------------------------------------------------------------------------------------

// Finds an AgentType with the requested bodyType->large flag. Looked up by the flag itself
// rather than by name, so this does not depend on ruleset naming.
StateRef<AgentType> findAgentType(GameState &state, bool large)
{
	for (auto &p : state.agent_types)
	{
		if (p.second->bodyType && p.second->bodyType->large == large)
		{
			return {&state, p.first};
		}
	}
	return {};
}

// Loads exactly the image/animation packs one AgentType needs, the same way
// Battle::loadImagePacks()/loadAnimationPacks() do it, without requiring a unit of that type to
// already exist in the battle (those are private, Battle-only, and only load packs referenced by
// units already placed when Battle::initBattle() runs).
void ensureUnitAssetsLoaded(GameState &state, StateRef<AgentType> type)
{
	auto loadImagePack = [&state](const StateRef<BattleUnitImagePack> &ref)
	{
		if (ref.id.empty())
		{
			return;
		}
		auto name = BattleUnitImagePack::getNameFromID(ref.id);
		auto id = format("{0}{1}", BattleUnitImagePack::getPrefix(), name);
		if (state.battle_unit_image_packs.find(id) != state.battle_unit_image_packs.end())
		{
			return;
		}
		auto pack = mksp<BattleUnitImagePack>();
		if (pack->loadImagePack(state, BattleUnitImagePack::getImagePackPath() + "/" + name))
		{
			state.battle_unit_image_packs[id] = pack;
		}
	};
	for (auto &perAppearance : type->image_packs)
	{
		for (auto &bodyPartPack : perAppearance)
		{
			loadImagePack(bodyPartPack.second);
		}
	}

	for (auto &ref : type->animation_packs)
	{
		if (ref.id.empty())
		{
			continue;
		}
		auto name = BattleUnitAnimationPack::getNameFromID(ref.id);
		auto id = format("{0}{1}", BattleUnitAnimationPack::getPrefix(), name);
		if (state.battle_unit_animation_packs.find(id) != state.battle_unit_animation_packs.end())
		{
			continue;
		}
		auto pack = mksp<BattleUnitAnimationPack>();
		if (pack->loadAnimationPack(state,
		                            BattleUnitAnimationPack::getAnimationPackPath() + "/" + name))
		{
			state.battle_unit_animation_packs[id] = pack;
		}
	}
}

// Builds a real Battle by starting an actual UFO recovery mission against the first vehicle type
// in the ruleset that has a battle_map, with no player squad and exactly one seed alien (just
// enough for BattleMap::fillSquads() to make the aliens org a participant). Each test then spawns
// whatever units it actually wants to examine at chosen coordinates within a carved sub-arena.
sp<Battle> makeRealBattle(GameState &state, StateRef<AgentType> seedAlienType)
{
	// Each test builds its own battle; drop any battle a previous test left behind first.
	state.current_battle = nullptr;

	StateRef<VehicleType> craftType;
	for (auto &p : state.vehicle_types)
	{
		if (p.second->battle_map)
		{
			craftType = {&state, p.first};
			break;
		}
	}
	if (!craftType)
	{
		LogError("No vehicle type with a battle_map in the ruleset");
		return nullptr;
	}

	auto v = mksp<Vehicle>();
	auto vID = Vehicle::generateObjectID(state);
	v->type = craftType;
	v->name = format("{0} {1}", v->type->name, ++v->type->numCreated);
	v->owner = state.getAliens();
	state.vehicles[vID] = v;
	StateRef<Vehicle> targetCraft = {&state, vID};
	StateRef<Vehicle> playerCraft = {};

	std::list<StateRef<Agent>> playerAgents;
	std::map<StateRef<AgentType>, int> aliens = {{seedAlienType, 1}};

	Battle::beginBattle(state, false, state.getAliens(), playerAgents, &aliens, playerCraft,
	                    targetCraft);
	if (!state.current_battle)
	{
		LogError("beginBattle failed to create a battle");
		return nullptr;
	}
	Battle::enterBattle(state);
	return state.current_battle;
}

// Sets the raw passability fields of one tile directly.
void setTile(TileMap &map, Vec3<int> pos, bool passable, bool canStand)
{
	Tile *t = map.getTile(pos);
	t->movementCostIn = passable ? 4 : 255;
	t->movementCostLeft = 0;
	t->movementCostRight = 0;
	t->closedDoorLeft = false;
	t->closedDoorRight = false;
	t->canStand = canStand;
	t->solidGround = false;
	t->hasLift = false;
	t->hasExit = false;
}

// Carves a single open room (x in [x0, x0+7], y in [y0, y0+5]) at floor level z (z, z+1, z+2 for
// headroom), sealed on every level by a one-tile-thick impassable perimeter so pathfinding
// machinery elsewhere in this test cannot wander off into the rest of the randomly generated map.
// This is deliberately simpler than test_battle_large_unit.cpp's two-room arena: no gap/pathing is
// under test here, just a large enough open floor to place a stationary and a "mid-move" large
// unit on distinct, non-overlapping 2x2 footprints.
void carveOneRoomArena(TileMap &map, int x0, int y0, int z)
{
	int zLo = std::max(0, z - 1);
	int zHi = std::min(map.size.z - 1, z + 3);
	for (int x = x0 - 1; x <= x0 + 8; x++)
	{
		for (int y = y0 - 1; y <= y0 + 6; y++)
		{
			bool onPerimeter = (x == x0 - 1 || x == x0 + 8 || y == y0 - 1 || y == y0 + 6);
			for (int zz = zLo; zz <= zHi; zz++)
			{
				setTile(map, {x, y, zz}, !onPerimeter, !onPerimeter && zz == z);
			}
		}
	}
}

// =================================================================================================
// OPE-12 fixture -- large-unit footprint occupancy while mid-move.
// =================================================================================================

// ---------------------------------------------------------------------------------------------
// large_unit_occupies_goal_block_while_moving
// ---------------------------------------------------------------------------------------------
// While a large unit is mid-move (!atGoal), TileObjectBattleUnit::setPosition() is supposed to
// register the union of its current 2x2x2 block and its destination (goalPosition) 2x2x2 block.
// As found, the destination block's own corner tile -- the raw `pos` truncated from
// `u->goalPosition`, before the seven surrounding offsets are added -- was never inserted, only
// the seven offsets around it. A tile sitting exactly on a large unit's arriving goal corner would
// therefore be reported as free while that unit is inbound.
//
// This spawns a large unit, settles it into a clean stationary state at a start tile (via
// resetGoal() + setPosition(..., /*goal*/ true), mirroring test_battle_large_unit.cpp's
// stationary fixture), then simulates "mid-move" by pointing goalPosition one tile away and
// clearing atGoal, and re-running BattleUnit::setPosition() (with the unit's own position
// unchanged, so no actual movement side effects fire) to re-derive occupiedTiles. The goal tile
// checked here (goalPosition itself, truncated to int) falls entirely outside the start block's
// own 2x2 footprint, so its presence in occupiedTiles is attributable only to the destination-
// block insertion this fix corrects.
static bool test_large_unit_occupies_goal_block_while_moving()
{
	auto &state = *g_state;
	auto largeType = findAgentType(state, true);
	auto smallType = findAgentType(state, false);
	TEST_REQUIRE(largeType, "ruleset has no large-bodied agent type");
	TEST_REQUIRE(smallType, "ruleset has no small-bodied agent type");
	ensureUnitAssetsLoaded(state, largeType);

	auto battle = makeRealBattle(state, smallType);
	TEST_REQUIRE(battle, "failed to create a real battle");
	auto &map = *battle->map;
	TEST_REQUIRE(map.size.x > 12 && map.size.y > 8 && map.size.z > 3,
	             "generated map ({0}) is too small for the carved arena", map.size);
	carveOneRoomArena(map, 2, 2, 1);

	const Vec3<float> start = {3.0f, 5.0f, 1.0f};
	auto unit = battle->spawnUnit(state, state.getAliens(), largeType, start, {1, 0});
	TEST_REQUIRE(unit && unit->tileObject, "spawnUnit failed to produce a tile object");

	// Settle into a clean, deterministic stationary state at `start` (matches the already-correct
	// stationary case locked by test_battle_large_unit.cpp's large_unit_occupies_block).
	unit->resetGoal();
	unit->setPosition(state, unit->position, true);

	const std::set<Vec3<int>> expectedStationary = {
	    {3, 5, 1}, {2, 5, 1}, {3, 4, 1}, {2, 4, 1}, {3, 5, 2}, {2, 5, 2}, {3, 4, 2}, {2, 4, 2},
	};
	TEST_REQUIRE(unit->tileObject->occupiedTiles == expectedStationary,
	             "sanity check failed: stationary large unit does not occupy exactly its 2x2x2 "
	             "block ({0} tiles, expected 8)",
	             unit->tileObject->occupiedTiles.size());

	// Now simulate "mid-move": goal is a tile east of the current position, not yet reached. The
	// unit's actual position does not change, so BattleUnit::setPosition()'s own movement side
	// effects (notifyAction/notifyScanners/etc, gated on oldPosition != position) do not fire --
	// only TileObjectBattleUnit::setPosition() re-derives occupiedTiles from the new
	// goalPosition/atGoal.
	const Vec3<float> goal = {4.0f, 5.0f, 1.0f};
	unit->goalPosition = goal;
	unit->atGoal = false;
	unit->setPosition(state, unit->position, false);

	const Vec3<int> goalCorner = {4, 5, 1};
	TEST_CHECK(unit->tileObject->occupiedTiles.count(goalCorner) == 1,
	           "moving large unit's occupiedTiles is missing its own goal-block corner tile {0} "
	           "-- TileObjectBattleUnit::setPosition drops the destination position itself from "
	           "the occupied-tiles union while a large unit is mid-move",
	           goalCorner);

	// The rest of the destination block's tiles were already inserted even before this fix; check
	// them too so a future regression that instead over-corrects (e.g. drops one of the other
	// seven) would also be caught.
	const std::set<Vec3<int>> expectedGoalBlock = {
	    {4, 5, 1}, {3, 5, 1}, {4, 4, 1}, {3, 4, 1}, {4, 5, 2}, {3, 5, 2}, {4, 4, 2}, {3, 4, 2},
	};
	for (auto &t : expectedGoalBlock)
	{
		TEST_CHECK(unit->tileObject->occupiedTiles.count(t) == 1,
		           "moving large unit's occupiedTiles is missing expected destination-block tile "
		           "{0}",
		           t);
	}

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
	    {"large_unit_occupies_goal_block_while_moving",
	     test_large_unit_occupies_goal_block_while_moving},
	});
	g_state->current_battle = nullptr;
	g_state.reset();
	return rc;
}
