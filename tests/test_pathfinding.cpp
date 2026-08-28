// Regression lock for Battle::groupMove's useTeleporter branch
// (game/state/tilemap/pathfinding.cpp). Before this behaviour was implemented,
// useTeleporter was ignored entirely (std::ignore = useTeleporter;) so a group
// move order with useTeleporter=true behaved identically to one without it.
// This test spawns a single battle unit carrying a fully-charged teleporter and
// asserts that groupMove(..., /*useTeleporter=*/true) actually consumes the
// teleporter's ammo and instantly relocates the unit to the requested tile,
// rather than issuing a normal walking mission.
//
// The battle/tilemap/unit fixtures below are hand-built (not a real generated
// battle map) to keep the fixture small and fast. The unit's stunDamage is set
// above its health on purpose: it makes the unit briefly "unconscious" for the
// duration of the call, which is the only lever available from outside
// pathfinding.cpp to keep BattleUnit::setBodyState/onReachGoal from touching
// the line-of-sight/vision bookkeeping (visibleBlocks/losBlocks/tileToLosBlock)
// that a real Battle::beginBattle() would populate but this synthetic fixture
// does not. None of that vision machinery is part of what's being tested here.
#include "framework/configfile.h"
#include "framework/framework.h"
#include "framework/logger.h"
#include "game/state/battle/battle.h"
#include "game/state/battle/battleunit.h"
#include "game/state/gamestate.h"
#include "game/state/gamestate_serialize.h"
#include "game/state/rules/aequipmenttype.h"
#include "game/state/rules/agenttype.h"
#include "game/state/rules/battle/battleunitanimationpack.h"
#include "game/state/rules/battle/battleunitimagepack.h"
#include "game/state/shared/aequipment.h"
#include "game/state/shared/agent.h"
#include "game/state/shared/organisation.h"
#include "game/state/tilemap/tile.h"
#include "game/state/tilemap/tilemap.h"
#include "library/strings.h"
#include <iostream>

using namespace OpenApoc;

// Battle::loadResources()/loadImagePacks()/loadAnimationPacks() are private to Battle (only
// BattleMap is a friend), and pulling in a whole generated BattleMap just to reach them is out
// of scope for this fixture. BattleUnitImagePack/BattleUnitAnimationPack's own load APIs are
// public, so load exactly the one shadow pack and one animation pack this test's unit
// references, the same way Battle::loadImagePacks()/loadAnimationPacks() would.
static bool loadUnitVisuals(GameState &state, StateRef<AgentType> type, int appearance)
{
	if (type->shadow_pack)
	{
		auto packName = BattleUnitImagePack::getNameFromID(type->shadow_pack.id);
		auto pack = mksp<BattleUnitImagePack>();
		if (!pack->loadImagePack(state, BattleUnitImagePack::getImagePackPath() + "/" + packName))
		{
			LogError("Failed to load shadow image pack \"{0}\"", packName);
			return false;
		}
		state.battle_unit_image_packs[type->shadow_pack.id] = pack;
	}
	if ((int)type->animation_packs.size() <= appearance)
	{
		LogError("AgentType {0} has no animation_packs entry for appearance {1}", type->name,
		         appearance);
		return false;
	}
	auto &animRef = type->animation_packs[appearance];
	auto animPackName = BattleUnitAnimationPack::getNameFromID(animRef.id);
	auto animPack = mksp<BattleUnitAnimationPack>();
	if (!animPack->loadAnimationPack(state, BattleUnitAnimationPack::getAnimationPackPath() + "/" +
	                                            animPackName))
	{
		LogError("Failed to load animation pack \"{0}\"", animPackName);
		return false;
	}
	state.battle_unit_animation_packs[animRef.id] = animPack;
	return true;
}

// Find a small (non-large), playable soldier-role agent type
static StateRef<AgentType> findSoldierType(sp<GameState> state)
{
	for (auto &t : state->agent_types)
	{
		if (t.second->role == AgentType::Role::Soldier && t.second->playable &&
		    t.second->bodyType && !t.second->bodyType->large)
		{
			return {state.get(), t.first};
		}
	}
	return {};
}

// Find any AEquipmentType of type Teleporter
static StateRef<AEquipmentType> findTeleporterType(sp<GameState> state)
{
	for (auto &t : state->agent_equipment)
	{
		if (t.second->type == AEquipmentType::Type::Teleporter)
		{
			return {state.get(), t.first};
		}
	}
	return {};
}

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

	LogInfo("Loading common gamestate \"{0}\"", common_name);
	auto state = mksp<GameState>();
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

	auto soldierType = findSoldierType(state);
	if (!soldierType)
	{
		LogError("Could not find a small playable soldier AgentType in the loaded ruleset");
		return EXIT_FAILURE;
	}
	auto teleporterType = findTeleporterType(state);
	if (!teleporterType)
	{
		LogError("Could not find an AEquipmentType of Type::Teleporter in the loaded ruleset");
		return EXIT_FAILURE;
	}
	if (teleporterType->max_ammo <= 0)
	{
		LogError("Teleporter type {0} has max_ammo <= 0", teleporterType->name);
		return EXIT_FAILURE;
	}

	// Build a minimal synthetic battle: a bare tilemap with no scenery, just enough
	// tile bookkeeping (canStand) for the teleport destination to be a legal landing spot.
	state->current_battle = mksp<Battle>();
	auto &battle = *state->current_battle;
	battle.mode = Battle::Mode::RealTime;

	const Vec3<int> mapSize{10, 10, 3};
	battle.map = std::make_unique<TileMap>(
	    mapSize, Vec3<float>{1, 1, 1}, Vec3<int>{32, 32, 16},
	    std::vector<std::set<TileObject::Type>>{{TileObject::Type::Unit}});
	battle.size = mapSize;

	const Vec3<int> startPos{2, 2, 1};
	const Vec3<int> targetPos{6, 6, 1};

	// Mark every tile groupMove's teleport offset search could land on as standable
	// (Tile::canStand defaults to false on a freshly-constructed tile).
	for (int dx = -2; dx <= 2; dx++)
	{
		for (int dy = -2; dy <= 2; dy++)
		{
			for (int dz = -1; dz <= 1; dz++)
			{
				Vec3<int> p = targetPos + Vec3<int>{dx, dy, dz};
				if (p.x < 0 || p.y < 0 || p.z < 0 || p.x >= mapSize.x || p.y >= mapSize.y ||
				    p.z >= mapSize.z)
				{
					continue;
				}
				battle.map->getTile(p)->canStand = true;
			}
		}
	}

	if (!state->battle_common_image_list)
	{
		LogError("state->battle_common_image_list not populated by loaded ruleset");
		return EXIT_FAILURE;
	}
	if (!state->battle_common_sample_list)
	{
		LogError("state->battle_common_sample_list not populated by loaded ruleset");
		return EXIT_FAILURE;
	}

	auto owner = state->getPlayer();
	auto agent = state->agent_generator.createAgent(*state, owner, soldierType);
	if (!agent)
	{
		LogError("Failed to create test agent of type {0}", soldierType->name);
		return EXIT_FAILURE;
	}

	auto unit =
	    battle.placeUnit(*state, agent, (Vec3<float>)startPos + Vec3<float>{0.5f, 0.5f, 0.0f});
	unit->facing = {0, 1};

	// Make the unit briefly unconscious so setBodyState/onReachGoal (invoked from within
	// the teleport mission this test exercises) skip the LOS/vision refresh this synthetic
	// fixture never populates. See file header comment.
	unit->stunDamage = agent->getHealth() + 1;

	auto item = mksp<AEquipment>();
	item->type = teleporterType;
	item->ammo = teleporterType->max_ammo;
	agent->equipment.push_back(item);

	// Register the real image/animation pack the placed unit's agent type references
	// (shadow_pack, animation_packs[appearance]) so BattleUnitMission::teleport's call into
	// BattleUnit::setBodyState (which needs a resolvable animation pack) doesn't dereference
	// an unresolved StateRef. See loadUnitVisuals() above for why this isn't just
	// Battle::loadResources().
	if (!loadUnitVisuals(*state, soldierType, agent->appearance))
	{
		return EXIT_FAILURE;
	}

	std::list<StateRef<BattleUnit>> units = {StateRef<BattleUnit>{state.get(), unit->id}};

	battle.groupMove(*state, units, targetPos, 0, false, true);

	bool ok = true;
	if (item->ammo != 0)
	{
		LogError("Teleporter ammo was not consumed ({0} left of {1}) - unit did not teleport",
		         item->ammo, teleporterType->max_ammo);
		ok = false;
	}
	auto finalTile = (Vec3<int>)unit->position;
	if (finalTile != targetPos)
	{
		LogError("Unit ended up at {{{0},{1},{2}}}, expected to teleport to {{{3},{4},{5}}}",
		         finalTile.x, finalTile.y, finalTile.z, targetPos.x, targetPos.y, targetPos.z);
		ok = false;
	}

	// GameState::~GameState() calls Battle::finishBattle()/exitBattle() on any set
	// current_battle, which expect a battle that went through the full beginBattle()
	// bookkeeping (forces, participants, etc.) that this hand-built fixture skips. Drop it
	// explicitly rather than letting the real teardown path run against a synthetic battle.
	state->current_battle = nullptr;

	if (!ok)
	{
		LogError("test_pathfinding failed");
		return EXIT_FAILURE;
	}

	LogInfo("test_pathfinding success - groupMove useTeleporter test passed");
	return EXIT_SUCCESS;
}
