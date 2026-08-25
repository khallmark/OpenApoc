// A3 - TU reservation parity lock test.
//
// Gap matrix: "TU reservation - implemented (parity unverified), high." TACP strings
// "TUs reserved for kneeling" / "aimed" / "snap" / "auto" confirm the four reserve modes exist,
// but nothing pins the reserved amounts (the four strings have empty bound xrefs, so this row is
// closed by verification, not by binding a constant - see docs/original-game/parity-guide.md,
// section A3). This test drives BattleUnit::canAfford()/spendTU()/refreshReserveCost() -
// the real production reserve bookkeeping in game/state/battle/battleunit.cpp - through each
// ReserveMode and freezes the observed behaviour.
//
// canAfford()/spendTU()/refreshReserveCost() never touch tileObject or the tile map (verified by
// reading), so units here are built by hand rather than through Battle::placeUnit()/spawnUnit():
// no map, no LOS, no Framework asset lists required.

#include "framework/configfile.h"
#include "framework/framework.h"
#include "game/state/battle/battle.h"
// mksp<Battle>() instantiates ~Battle(), which needs complete types for every StateRefMap<T>
// member (units/scanners/doors) - battle.h only forward-declares BattleScanner/BattleDoor.
#include "game/state/battle/battledoor.h"
#include "game/state/battle/battlescanner.h"
#include "game/state/battle/battleunit.h"
#include "game/state/gamestate.h"
#include "game/state/rules/aequipmenttype.h"
#include "game/state/rules/agenttype.h"
#include "game/state/shared/aequipment.h"
#include "game/state/shared/agent.h"
#include "game/state/shared/equipment.h"
#include "library/sp.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static sp<GameState> g_state;

namespace
{

// state.agent_types is a StateRefMap (std::map), so iteration is alphabetical by ID - the first
// role==Soldier && playable match is AGENTTYPE_ALIEN_GREY, whose equipment_layout has no
// RightHand-shaped slot any real weapon fits. Require an actual RightHand slot so this reliably
// lands on a normal humanoid (e.g. AGENTTYPE_X-COM_AGENT_HUMAN) instead.
StateRef<AgentType> findSoldierType(GameState &state)
{
	for (auto &t : state.agent_types)
	{
		if (!t.second || t.second->role != AgentType::Role::Soldier || !t.second->playable ||
		    !t.second->equipment_layout)
		{
			continue;
		}
		for (auto &slot : t.second->equipment_layout->slots)
		{
			if (slot.type == EquipmentSlotType::RightHand)
			{
				return {&state, t.first};
			}
		}
	}
	return {};
}

// Owner deliberately state.getCivilian(), not state.getPlayer() or state.getAliens():
// AgentGenerator::createAgent() (agent.cpp:150-176) gives the player no starting gear, but DOES
// auto-equip a random kit for every other org except civilians (aliens get an alien kit, other
// orgs get a tech-level kit) - which would occupy RightHand before this file's own equip calls
// run. Civilian is also exempt from AEquipmentType::canBeUsed()'s player-research gate, so
// research state is irrelevant to what this test is checking either way.
sp<BattleUnit> makeUnit(GameState &state, const sp<Battle> &battle, StateRef<Organisation> owner,
                        StateRef<AgentType> agentType)
{
	auto agent = state.agent_generator.createAgent(state, owner, agentType);
	auto unit = mksp<BattleUnit>();
	unit->id = BattleUnit::generateObjectID(state);
	unit->agent = agent;
	unit->owner = owner;
	battle->units[unit->id] = unit;
	return unit;
}

// Not every real Weapon-type item fits a given agent's equipment_layout (alien-only natural
// weapons, oversized items, etc.) - try each real fireable one in turn and equip whichever
// actually fits and can fire, rather than assuming the first candidate found is usable.
// Deliberately not hardcoded to a specific item ID so this doesn't rot if the base gamestate's
// equipment list is renamed.
sp<AEquipment> equipFirstWorkingWeapon(GameState &state, StateRef<Agent> agent)
{
	for (auto &e : state.agent_equipment)
	{
		if (!e.second || e.second->type != AEquipmentType::Type::Weapon ||
		    e.second->fire_delay <= 0)
		{
			continue;
		}
		auto equipped =
		    agent->addEquipmentByType(state, {&state, e.first}, EquipmentSlotType::RightHand,
		                              /*allowFailure=*/true);
		if (equipped && equipped->canFire(state))
		{
			return equipped;
		}
	}
	return nullptr;
}

} // namespace

// Refused a move that would drop TUs below the aimed-shot cost; permitted at exactly cost + 1.
static bool test_aimed_reserve_refuses_and_permits_at_boundary()
{
	auto &state = *g_state;
	auto owner = state.getCivilian();
	auto agentType = findSoldierType(state);
	TEST_REQUIRE((bool)agentType, "no soldier agent type in loaded gamestate");

	auto battle = mksp<Battle>();
	battle->mode = Battle::Mode::TurnBased;
	battle->currentActiveOrganisation = owner;
	state.current_battle = battle;

	auto unit = makeUnit(state, battle, owner, agentType);
	auto weapon = equipFirstWorkingWeapon(state, unit->agent);
	TEST_REQUIRE((bool)weapon, "could not equip any working weapon in right hand");

	unit->initialTU = 100;
	unit->agent->modified_stats.time_units = 100;

	unit->setReserveShotMode(state, ReserveShotMode::Aimed);
	const int R = unit->reserveShotCost;
	TEST_REQUIRE(R > 0, "aimed reserve cost is {0}, expected > 0", R);
	LogWarning("agent={0} weapon={1} aimed reserve R={2} (initialTU=100)", agentType.id,
	           weapon->type.id, R);

	const int moveCost = 10;

	// TU = moveCost + R - 1: spending moveCost would leave R-1, i.e. below the aimed reserve.
	unit->agent->modified_stats.time_units = moveCost + R - 1;
	TEST_REQUIRE(!unit->canAfford(state, moveCost),
	             "move of cost {0} permitted with TU={1}, R={2} - would drop below reserve",
	             moveCost, unit->agent->modified_stats.time_units, R);

	// TU = moveCost + R exactly: spending moveCost leaves exactly R. Permitted.
	unit->agent->modified_stats.time_units = moveCost + R;
	TEST_REQUIRE(unit->canAfford(state, moveCost),
	             "move of cost {0} refused with TU={1}, R={2} - leaves exactly the reserve",
	             moveCost, unit->agent->modified_stats.time_units, R);

	// spendTU() must enforce the identical boundary, and must not touch TU on refusal.
	unit->agent->modified_stats.time_units = moveCost + R - 1;
	const int tuBeforeRefusal = unit->agent->modified_stats.time_units;
	TEST_REQUIRE(!unit->spendTU(state, moveCost),
	             "spendTU allowed a move that dips below the aimed reserve");
	TEST_REQUIRE(unit->agent->modified_stats.time_units == tuBeforeRefusal,
	             "spendTU changed TU ({0} -> {1}) despite refusing the move", tuBeforeRefusal,
	             unit->agent->modified_stats.time_units);

	unit->agent->modified_stats.time_units = moveCost + R;
	TEST_REQUIRE(unit->spendTU(state, moveCost),
	             "spendTU refused a move that leaves exactly the reserve");
	TEST_REQUIRE(unit->agent->modified_stats.time_units == R,
	             "spendTU left {0} TU after spending, expected exactly the reserve {1}",
	             unit->agent->modified_stats.time_units, R);

	return true;
}

// Kneel reservation composes with weapon reservation (additive) rather than replacing it.
static bool test_kneel_reserve_composes_with_weapon_reserve()
{
	auto &state = *g_state;
	auto owner = state.getCivilian();
	auto agentType = findSoldierType(state);
	TEST_REQUIRE((bool)agentType, "no soldier agent type in loaded gamestate");

	auto battle = mksp<Battle>();
	battle->mode = Battle::Mode::TurnBased;
	battle->currentActiveOrganisation = owner;
	state.current_battle = battle;

	auto unit = makeUnit(state, battle, owner, agentType);
	auto weapon = equipFirstWorkingWeapon(state, unit->agent);
	TEST_REQUIRE((bool)weapon, "could not equip any working weapon in right hand");

	unit->initialTU = 100;
	unit->agent->modified_stats.time_units = 100;

	unit->setReserveShotMode(state, ReserveShotMode::Aimed);
	const int R = unit->reserveShotCost;
	TEST_REQUIRE(R > 0, "aimed reserve cost is {0}, expected > 0", R);

	TEST_REQUIRE(unit->agent->isBodyStateAllowed(BodyState::Kneeling),
	             "agent type does not allow kneeling; cannot test kneel reserve");

	unit->setReserveKneelMode(KneelingMode::Kneeling);
	TEST_REQUIRE(unit->reserve_kneel_mode == KneelingMode::Kneeling,
	             "setReserveKneelMode(Kneeling) did not take effect");

	const int K = unit->getBodyStateChangeCost(BodyState::Standing, BodyState::Kneeling);
	TEST_REQUIRE(K > 0, "kneel body-state-change cost is {0}, expected > 0", K);
	LogWarning("agent={0} weapon={1} aimed reserve R={2}, kneel reserve K={3} (initialTU=100)",
	           agentType.id, weapon->type.id, R, K);

	const int moveCost = 10;

	// If kneel REPLACED the weapon reserve, moveCost + K would already be affordable at
	// TU = moveCost + R + K - 1 whenever K < R. Requiring refusal here proves both are reserved
	// simultaneously (R + K), not just the larger of the two.
	unit->agent->modified_stats.time_units = moveCost + R + K - 1;
	TEST_REQUIRE(!unit->canAfford(state, moveCost),
	             "move permitted with TU={0} despite composed reserve R={1} + K={2}",
	             unit->agent->modified_stats.time_units, R, K);

	unit->agent->modified_stats.time_units = moveCost + R + K;
	TEST_REQUIRE(unit->canAfford(state, moveCost),
	             "move refused with TU={0} despite leaving exactly the composed reserve R={1} + "
	             "K={2}",
	             unit->agent->modified_stats.time_units, R, K);

	// Ignoring one reserve independently must fall back to only the other being enforced -
	// direct evidence the two are tracked as separate additive terms, not a single combined mode.
	unit->agent->modified_stats.time_units = moveCost + R;
	TEST_REQUIRE(unit->canAfford(state, moveCost, /*ignoreKneelReserve=*/true,
	                             /*ignoreShootReserve=*/false),
	             "ignoring kneel reserve alone should leave only the R boundary in effect");
	unit->agent->modified_stats.time_units = moveCost + K;
	TEST_REQUIRE(unit->canAfford(state, moveCost, /*ignoreKneelReserve=*/false,
	                             /*ignoreShootReserve=*/true),
	             "ignoring shot reserve alone should leave only the K boundary in effect");

	return true;
}

// Driving a unit through every ReserveMode: None reserves nothing, and Aimed/Snap/Auto produce
// three distinct, strictly decreasing costs (getFireCost divides the same fire_delay by 1/2/4).
static bool test_reserve_mode_costs_are_distinct()
{
	auto &state = *g_state;
	auto owner = state.getCivilian();
	auto agentType = findSoldierType(state);
	TEST_REQUIRE((bool)agentType, "no soldier agent type in loaded gamestate");

	auto battle = mksp<Battle>();
	battle->mode = Battle::Mode::TurnBased;
	battle->currentActiveOrganisation = owner;
	state.current_battle = battle;

	auto unit = makeUnit(state, battle, owner, agentType);
	auto weapon = equipFirstWorkingWeapon(state, unit->agent);
	TEST_REQUIRE((bool)weapon, "could not equip any working weapon in right hand");

	unit->initialTU = 100;
	unit->agent->modified_stats.time_units = 100;

	unit->setReserveShotMode(state, ReserveShotMode::None);
	TEST_REQUIRE(unit->reserveShotCost == 0, "ReserveShotMode::None left a nonzero cost ({0})",
	             unit->reserveShotCost);

	unit->setReserveShotMode(state, ReserveShotMode::Aimed);
	const int aimed = unit->reserveShotCost;
	unit->setReserveShotMode(state, ReserveShotMode::Snap);
	const int snap = unit->reserveShotCost;
	unit->setReserveShotMode(state, ReserveShotMode::Auto);
	const int auto_ = unit->reserveShotCost;
	LogWarning("agent={0} weapon={1} reserve costs: aimed={2} snap={3} auto={4} (initialTU=100)",
	           agentType.id, weapon->type.id, aimed, snap, auto_);

	TEST_REQUIRE(aimed > 0 && snap > 0 && auto_ > 0,
	             "expected all three modes to reserve TUs: aimed={0} snap={1} auto={2}", aimed,
	             snap, auto_);
	TEST_REQUIRE(aimed > snap, "aimed reserve {0} not greater than snap reserve {1}", aimed, snap);
	TEST_REQUIRE(snap > auto_, "snap reserve {0} not greater than auto reserve {1}", snap, auto_);

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

	// spendTU()'s notifyAction() path is state-only (no window needed), but keep parity with
	// test_base_die.cpp: a Framework must exist before we touch a loaded GameState.
	Framework fw("OpenApoc", false);
	g_state = mksp<GameState>();
	if (!loadStartedGameState(*g_state, common, gamestate))
	{
		return EXIT_FAILURE;
	}

	const int rc = runTestSuite({
	    {"aimed_reserve_refuses_and_permits_at_boundary",
	     test_aimed_reserve_refuses_and_permits_at_boundary},
	    {"kneel_reserve_composes_with_weapon_reserve",
	     test_kneel_reserve_composes_with_weapon_reserve},
	    {"reserve_mode_costs_are_distinct", test_reserve_mode_costs_are_distinct},
	});
	// Each test above hand-builds a minimal Battle and points state.current_battle at it, without
	// the mission_type/mission_location_building a real battle would carry.
	// ~GameState() unconditionally calls Battle::finishBattle()/exitBattle() when current_battle
	// is set, which dereferences mission_location_building - clear it first so teardown doesn't
	// segfault on our synthetic battle.
	g_state->current_battle = nullptr;
	g_state.reset();
	return rc;
}
