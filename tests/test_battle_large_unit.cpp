// Parity item A1 -- multi-tile (large) unit pathing, occupancy, LOS and targeting.
//
// Large units (bodyType->large, e.g. Megaspawn/Multiworm) occupy a 2x2x2 block of tiles. This
// is pure engine geometry: no TACP table is recovered or needed (docs/original-game/parity-guide.md
// section "A1"). Step 1 of that guide's workflow required driving an actual large unit through
// each candidate failure mode before writing any fix, and recording which ones actually failed --
// several of the code paths already handle the full block correctly (pathfinding's canEnterTile
// already rejects a destination block that isn't entirely passable; draw order already sorts a
// large unit into whichever occupied tile its head currently pops into), so this file only locks
// the parts that were genuinely broken.
//
// Each test uses a real, procedurally generated Battle (Battle::beginBattle()/enterBattle() from
// an actual UFO's battle_map, exactly as the campaign does it) rather than a hand-built TileMap.
// That is not for realism's own sake: BattleUnit::setFacing() -> refreshUnitVision() ->
// calculateVisionToTerrain() unconditionally indexes battle.visibleBlocks/visibleTiles/losBlocks,
// which only BattleMap's real map generation populates, and BattleUnit::setBodyState() needs a
// unit's image/animation packs loaded, which Battle::initBattle() only does for the participants
// and units already present when it runs. Trying to hand-roll enough of Battle to satisfy both
// turned out to be most of BattleMap's own generation pipeline; using the real thing is both
// simpler and more faithful. What stays fully controlled is the geometry actually under test:
// canEnterTile()/getPassable() only ever consult a Tile's raw scalar fields (never the scenery
// graph), so poking those directly scripts a deterministic pathfinding scenario regardless of
// what the randomly generated map underneath happens to contain. The LOS test similarly
// fabricates its own solid BattleMapPartType/VoxelMap rather than hunting for a suitably placed
// real wall.

#include "framework/configfile.h"
#include "framework/framework.h"
#include "game/state/battle/battle.h"
#include "game/state/battle/battlemappart.h"
#include "game/state/battle/battleunit.h"
#include "game/state/battle/battleunitmission.h"
#include "game/state/city/vehicle.h"
#include "game/state/gamestate.h"
#include "game/state/rules/agenttype.h"
#include "game/state/rules/battle/battlemap.h"
#include "game/state/rules/battle/battlemapparttype.h"
#include "game/state/rules/battle/battleunitanimationpack.h"
#include "game/state/rules/battle/battleunitimagepack.h"
#include "game/state/rules/city/vehicletype.h"
#include "game/state/tilemap/tile.h"
#include "game/state/tilemap/tilemap.h"
#include "game/state/tilemap/tileobject_battleunit.h"
#include "library/voxel.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

namespace
{

sp<GameState> g_state;

// Finds an AgentType with the requested bodyType->large flag. Looked up by the flag itself
// rather than by name, so this does not depend on ruleset naming (Megaspawn / Multiworm in the
// vanilla ruleset happen to be the large ones).
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

// Battle::loadImagePacks()/loadAnimationPacks() (which Battle::initBattle() calls as part of
// Battle::loadResources()) only load the sprite/animation packs actually referenced by units
// already placed in the battle, and are private besides (Battle-only). makeRealBattle() below
// seeds exactly one alien of a known type before entering the battle so that type's assets get
// loaded automatically, but a test's own manually spawned units may reference a different
// AgentType (e.g. a large one where the seed was small). This loads exactly the packs one
// AgentType needs, the same way Battle::loadImagePacks()/loadAnimationPacks() do it, without
// requiring a unit of that type to already exist in the battle.
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
// enough for BattleMap::fillSquads() to make the aliens org a participant, which is what makes
// Battle::visibleBlocks/visibleTiles/visibleUnits exist for that org at all). Each test then
// spawns whatever units it actually wants to examine at chosen coordinates, and/or pokes raw Tile
// scalars to carve a controlled sub-arena within the generated map -- see the file header.
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

// Sets the raw passability fields of one tile directly. canEnterTile() and Tile::getPassable()
// only ever consult these scalars (not the scenery graph), so poking them directly is a valid,
// map-content-independent way to script a pathfinding scenario.
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

// Carves two 4-wide open rooms (x in [x0,x0+3] and x in [x0+5,x0+8], y in [y0,y0+5]) separated by
// a single wall column at x0+4, full height (z, z+1, z+2), with a passable gap at the given
// wall rows (y offsets relative to y0). Both rooms and the corridor are floor-level passable;
// wall-column cells are impassable at every level so the gap width is exactly controlled.
//
// The interior sits inside a one-tile-thick impassable perimeter, sealed on every level from
// just below the floor to just above the headroom. Without it, pathfinding is free to roam the
// rest of the real, procedurally generated map this arena is carved into and route around the
// dividing wall entirely -- through the untouched real geometry outside the carved region --
// rather than actually being forced through (or blocked by) the gap under test.
void carveTwoRoomArena(TileMap &map, int x0, int y0, int z, const std::set<int> &openWallRows)
{
	int zLo = std::max(0, z - 1);
	int zHi = std::min(map.size.z - 1, z + 3);
	for (int x = x0 - 1; x <= x0 + 9; x++)
	{
		for (int y = y0 - 1; y <= y0 + 6; y++)
		{
			bool onPerimeter = (x == x0 - 1 || x == x0 + 9 || y == y0 - 1 || y == y0 + 6);
			if (!onPerimeter)
			{
				continue;
			}
			for (int zz = zLo; zz <= zHi; zz++)
			{
				setTile(map, {x, y, zz}, false, false);
			}
		}
	}

	for (int x = x0; x <= x0 + 8; x++)
	{
		bool wallColumn = (x == x0 + 4);
		for (int y = y0; y <= y0 + 5; y++)
		{
			bool open = !wallColumn || openWallRows.count(y - y0) > 0;
			for (int zz = z; zz <= z + 2; zz++)
			{
				setTile(map, {x, y, zz}, open, open && zz == z);
			}
		}
	}
}

// A VoxelMap with every bit set, i.e. fully solid across the whole tile.
sp<VoxelMap> makeSolidVoxelMap()
{
	Vec3<int> size = {VOXEL_X_BATTLE, VOXEL_Y_BATTLE, VOXEL_Z_BATTLE};
	auto vm = mksp<VoxelMap>(size);
	for (int z = 0; z < size.z; z++)
	{
		auto slice = mksp<VoxelSlice>(Vec2<int>{size.x, size.y});
		for (int x = 0; x < size.x; x++)
		{
			for (int y = 0; y < size.y; y++)
			{
				slice->setBit({x, y}, true);
			}
		}
		vm->setSlice(z, slice);
	}
	return vm;
}

// Registers a fresh, fully solid, LOS-and-LOF-blocking Feature map part type under a fabricated
// id, and returns a StateRef to it. Not sourced from any real tileset: BattleMapPartType::get()
// resolves purely from state.battleMapTiles, so inserting one by hand is sufficient and does not
// depend on which tileset a randomly generated battle happens to load.
StateRef<BattleMapPartType> makeSolidWallType(GameState &state)
{
	auto type = mksp<BattleMapPartType>();
	type->type = BattleMapPartType::Type::Feature;
	type->blocksLOS = true;
	type->movement_cost = 255;
	type->height = 39;
	type->voxelMapLOS = makeSolidVoxelMap();
	type->voxelMapLOF = makeSolidVoxelMap();
	const UString id = "BATTLEMAPPART_TEST_SOLID_WALL";
	state.battleMapTiles[id] = type;
	return {&state, id};
}

// ---------------------------------------------------------------------------------------------
// large_unit_occupies_block
// ---------------------------------------------------------------------------------------------
// TileObjectBattleUnit::setPosition is supposed to register a large unit against the full 2x2x2
// block it occupies -- at rest, and (while moving) the union of its current and goal blocks, so
// pathfinding/occupancy queries against either block see it. The stationary case already worked;
// the moving case was missing the goal block's own floor-level corner tile from the union (see
// game/state/tilemap/tileobject_battleunit.cpp), which this locks.
static bool test_large_unit_occupies_block()
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
	carveTwoRoomArena(map, 2, 2, 1, {2, 3});

	Vec3<float> pos = {3.0f, 5.0f, 1.0f};
	auto unit = battle->spawnUnit(state, state.getAliens(), largeType, pos, {1, 0});
	TEST_REQUIRE(unit && unit->tileObject, "spawnUnit failed to produce a tile object");

	// Stationary: exactly the 8 tiles of the 2x2x2 block at rest.
	unit->resetGoal();
	unit->setPosition(state, unit->position, true);
	std::set<Vec3<int>> expectedStationary = {
	    {3, 5, 1}, {2, 5, 1}, {3, 4, 1}, {2, 4, 1}, {3, 5, 2}, {2, 5, 2}, {3, 4, 2}, {2, 4, 2},
	};
	TEST_REQUIRE(unit->tileObject->occupiedTiles == expectedStationary,
	             "stationary large unit does not occupy exactly its 2x2x2 block ({0} tiles, "
	             "expected 8)",
	             unit->tileObject->occupiedTiles.size());
	for (auto &t : expectedStationary)
	{
		if (t.z != 1)
		{
			continue;
		}
		auto found = map.getTile(t)->getUnitIfPresent(false, true, false, nullptr, false, false);
		TEST_REQUIRE(found && found->getUnit() == unit,
		             "floor tile {0} does not report the large unit as occupying it", t);
	}

	// Moving: mid-transit to an adjacent block, the occupied set must be the union of the
	// current block and the goal block (12 tiles for a one-step +x move), not just 11.
	unit->atGoal = false;
	unit->goalPosition = {4.0f, 5.0f, 1.0f};
	unit->tileObject->setPosition(unit->position);
	std::set<Vec3<int>> expectedUnion = {
	    // current block
	    {3, 5, 1},
	    {2, 5, 1},
	    {3, 4, 1},
	    {2, 4, 1},
	    {3, 5, 2},
	    {2, 5, 2},
	    {3, 4, 2},
	    {2, 4, 2},
	    // goal block
	    {4, 5, 1},
	    {3, 5, 1},
	    {4, 4, 1},
	    {3, 4, 1},
	    {4, 5, 2},
	    {3, 5, 2},
	    {4, 4, 2},
	    {3, 4, 2},
	};
	TEST_REQUIRE(unit->tileObject->occupiedTiles == expectedUnion,
	             "moving large unit's occupied set is not the union of its current and goal "
	             "blocks ({0} tiles, expected {1})",
	             unit->tileObject->occupiedTiles.size(), expectedUnion.size());
	// In particular, the goal block's own floor-level corner tile must be reserved -- this is
	// exactly the cell the destination-block passability check treats as "the" tile.
	TEST_REQUIRE(unit->tileObject->occupiedTiles.count({4, 5, 1}) == 1,
	             "goal block's own corner tile {4,5,1} is missing from the occupied set");

	return true;
}

// ---------------------------------------------------------------------------------------------
// large_unit_path_rejects_narrow_gap
// ---------------------------------------------------------------------------------------------
// A large unit needs its whole 2x2 footprint to fit through an opening. A corridor exactly one
// tile wide is passable for a small unit but must not be passable for a large one; widening the
// gap to two tiles must restore large-unit passage. This exercises
// BattleUnitTileHelper::canEnterTile (game/state/battle/battleunitmission.cpp) via
// TileMap::findShortestPath directly, with no unit object involved.
static bool test_large_unit_path_rejects_narrow_gap()
{
	auto &state = *g_state;
	auto smallType = findAgentType(state, false);
	TEST_REQUIRE(smallType, "ruleset has no small-bodied agent type");

	auto battle = makeRealBattle(state, smallType);
	TEST_REQUIRE(battle, "failed to create a real battle");
	auto &map = *battle->map;
	TEST_REQUIRE(map.size.x > 12 && map.size.y > 8 && map.size.z > 3,
	             "generated map ({0}) is too small for the carved arena", map.size);

	const Vec3<int> origin = {3, 5, 1};
	const Vec3<int> destination = {8, 5, 1};

	// One-tile-wide gap (a single open row in the dividing wall).
	carveTwoRoomArena(map, 2, 2, 1, {3});

	BattleUnitTileHelper smallHelper(map, /*large*/ false, /*flying*/ false, /*allowJumping*/ false,
	                                 32, nullptr);
	auto smallPath = map.findShortestPath(origin, destination, 200, smallHelper);
	TEST_REQUIRE(!smallPath.empty() && smallPath.back() == destination,
	             "sanity check failed: a small unit could not cross the 1-tile gap, so the "
	             "carved arena is not what this test thinks it is");

	BattleUnitTileHelper largeHelper(map, /*large*/ true, /*flying*/ false, /*allowJumping*/ false,
	                                 80, nullptr);
	auto largePathNarrow = map.findShortestPath(origin, destination, 200, largeHelper);
	TEST_CHECK(largePathNarrow.empty() || largePathNarrow.back() != destination,
	           "large unit found a path through a 1-tile gap it cannot physically fit through");

	// Widening the gap to two tiles must restore large-unit passage -- proves the rejection
	// above is really about gap width, not some other mistake in the carved arena.
	carveTwoRoomArena(map, 2, 2, 1, {2, 3});
	auto largePathWide = map.findShortestPath(origin, destination, 200, largeHelper);
	TEST_REQUIRE(!largePathWide.empty() && largePathWide.back() == destination,
	             "large unit could not cross a 2-tile-wide gap");

	return true;
}

// ---------------------------------------------------------------------------------------------
// large_unit_no_ground_snag
// ---------------------------------------------------------------------------------------------
// BattleUnit::getEyeLocation() is supposed to return the centre of whichever tile/block a unit
// currently occupies. For a small unit, "position" is already the tile centre (x+0.5, y+0.5), so
// truncating to int and re-adding 0.5 snaps correctly. A large unit's "position" is the block's
// own max-x/max-y/min-z corner, and a 2x2 block's centre coincides with that corner value exactly
// (no +0.5 needed) -- getEyeLocation() applied the small-unit +0.5 unconditionally, putting a
// large unit's eyes half a tile off from where its own muzzle (BattleUnit::getMuzzleLocation(),
// which does not add any offset) is. This is the concrete, provable half of what
// battleunit.h:677's "FIXME: This likely won't work properly for large units" and the "caught up
// in ground" workaround comments (battleunit.cpp, formerly at :636/:1045) were compensating for
// at the LOS/line-of-fire call sites: the aim points a large unit's own vision and gunfire
// converge on did not agree with each other, and did not sit at the block's true centre.
static bool test_large_unit_no_ground_snag()
{
	auto &state = *g_state;
	auto largeType = findAgentType(state, true);
	TEST_REQUIRE(largeType, "ruleset has no large-bodied agent type");

	auto agent = state.agent_generator.createAgent(state, state.getAliens(), largeType);
	TEST_REQUIRE(agent, "failed to create large agent");

	auto unit = mksp<BattleUnit>();
	unit->agent = agent;
	unit->current_body_state = BodyState::Standing;
	unit->target_body_state = BodyState::Standing;
	unit->body_animation_ticks_total = 1;
	unit->body_animation_ticks_remaining = 0;

	// A block whose corner is at (5,7,2): true centre is x=5,y=7 exactly.
	unit->position = {5.0f, 7.0f, 2.0f};

	auto eye = unit->getEyeLocation();
	auto muzzle = unit->getMuzzleLocation();

	TEST_CHECK(eye.x == muzzle.x,
	           "large unit's eye.x ({0}) != muzzle.x ({1}): eyes are not "
	           "centred over the block it occupies",
	           eye.x, muzzle.x);
	TEST_CHECK(eye.y == muzzle.y,
	           "large unit's eye.y ({0}) != muzzle.y ({1}): eyes are not "
	           "centred over the block it occupies",
	           eye.y, muzzle.y);
	TEST_REQUIRE(eye.x == 5.0f, "large unit eye.x is {0}, expected the block's true centre (5)",
	             eye.x);
	TEST_REQUIRE(eye.y == 7.0f, "large unit eye.y is {0}, expected the block's true centre (7)",
	             eye.y);

	return true;
}

// ---------------------------------------------------------------------------------------------
// large_unit_los_any_tile
// ---------------------------------------------------------------------------------------------
// BattleUnit::calculateVisionToUnit() is supposed to succeed if any occupied tile of a large
// target is visible, per docs/original-game/parity-guide.md's A1 step 4. The as-found
// implementation instead probed a single point (the block's exact geometric centre, nudged 0.75
// tiles toward the viewer) rather than the target's actual occupied tiles. This locks the
// documented "any occupied tile" contract against the real map/collision machinery: a solid wall
// placed in front of the near half of the target's footprint, but not the far half, must not
// make the target invisible.
//
// calculateVisionToUnit() itself is private (Battle is its only friend); it is exercised here
// through the same public path the game uses -- Battle::spawnUnit() calls
// refreshUnitVisibilityAndVision() on the unit it just placed, which updates every other unit's
// visibleUnits set. Spawning the viewer, then placing geometry, then spawning the target reflects
// the geometry that is in place at spawn time.
static bool test_large_unit_los_any_tile()
{
	auto &state = *g_state;
	auto wallType = makeSolidWallType(state);

	auto largeType = findAgentType(state, true);
	auto smallType = findAgentType(state, false);
	TEST_REQUIRE(largeType, "ruleset has no large-bodied agent type");
	TEST_REQUIRE(smallType, "ruleset has no small-bodied agent type");
	ensureUnitAssetsLoaded(state, largeType);
	ensureUnitAssetsLoaded(state, smallType);

	// Sanity check first, on a battle with no wall at all: an adjacent large target must be
	// visible to begin with.
	{
		auto battle = makeRealBattle(state, smallType);
		TEST_REQUIRE(battle, "failed to create a real battle");
		auto &map = *battle->map;
		TEST_REQUIRE(map.size.x > 12 && map.size.y > 8 && map.size.z > 3,
		             "generated map ({0}) is too small for the carved arena", map.size);
		carveTwoRoomArena(map, 2, 2, 1, {2, 3});

		auto viewer =
		    battle->spawnUnit(state, state.getPlayer(), smallType, {3.5f, 4.5f, 1.0f}, {1, 0});
		TEST_REQUIRE(viewer, "spawnUnit failed for viewer");
		// Large unit spans x in {6,7}, y in {4,5} (block corner at (7,5,1)).
		auto target =
		    battle->spawnUnit(state, state.getAliens(), largeType, {7.0f, 5.0f, 1.0f}, {-1, 0});
		TEST_REQUIRE(target, "spawnUnit failed for target");

		TEST_REQUIRE(viewer->visibleUnits.count({&state, target->id}) == 1,
		             "sanity check failed: target is not visible with a clear line at all");
	}

	// Main case: same layout, but with a solid wall across the whole row 4 of the target's
	// footprint (tiles (6,4) and (7,4), both columns) placed before either unit spawns. Row 5
	// ((6,5) and (7,5)) stays clear. The viewer sits on row 4 (y=4.5), aligned with the blocked
	// row: a straight line from the viewer to the block's single geometric-centre probe point
	// (x=7,y=5 -- the corner shared by all four tiles) gets nudged toward the viewer by the old
	// code, i.e. toward row 4, landing it inside the wall -- while a genuinely diagonal ray to
	// either row-5 tile clears row 4 well before it reaches the wall's x-range (row crosses to 5
	// by x=5, the wall is at x=6..7), so row 5 is visible by construction, independent of any
	// nudging or centre-point degeneracy. The wall is set up before spawning so that
	// spawnUnit()'s own vision refresh sees the final geometry.
	{
		auto battle = makeRealBattle(state, smallType);
		TEST_REQUIRE(battle, "failed to create a real battle");
		auto &map = *battle->map;
		TEST_REQUIRE(map.size.x > 12 && map.size.y > 8 && map.size.z > 3,
		             "generated map ({0}) is too small for the carved arena", map.size);
		carveTwoRoomArena(map, 2, 2, 1, {2, 3});

		for (int x = 6; x <= 7; x++)
		{
			auto wall = mksp<BattleMapPart>();
			wall->type = wallType;
			wall->position = {(float)x + 0.5f, 4.5f, 1.5f};
			map.addObjectToMap(wall);
			battle->map_parts.push_back(wall);
		}

		auto viewer =
		    battle->spawnUnit(state, state.getPlayer(), smallType, {3.5f, 4.5f, 1.0f}, {1, 0});
		TEST_REQUIRE(viewer, "spawnUnit failed for viewer");
		auto target =
		    battle->spawnUnit(state, state.getAliens(), largeType, {7.0f, 5.0f, 1.0f}, {-1, 0});
		TEST_REQUIRE(target, "spawnUnit failed for target");

		TEST_CHECK(viewer->visibleUnits.count({&state, target->id}) == 1,
		           "large target is not visible even though its far occupied tile has a clear "
		           "line -- LOS is probing a single point instead of any occupied tile");
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
	    {"large_unit_occupies_block", test_large_unit_occupies_block},
	    {"large_unit_path_rejects_narrow_gap", test_large_unit_path_rejects_narrow_gap},
	    {"large_unit_no_ground_snag", test_large_unit_no_ground_snag},
	    {"large_unit_los_any_tile", test_large_unit_los_any_tile},
	});
	g_state->current_battle = nullptr;
	g_state.reset();
	return rc;
}
