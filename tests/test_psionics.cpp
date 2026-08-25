// A2 - psionics timing parity lock test.
//
// Gap matrix: "implemented (parity unverified), medium." The costs below are prior-art, recovered
// from tools/extractors/docs/psionics.txt, not the binary, and are already implemented in
// game/state/battle/battleunit.cpp. Nothing held them in place before this test - see
// docs/original-game/parity-guide.md, section A2.
//
// From tools/extractors/docs/psionics.txt ("applied each half second"):
//     Attack    Initial cost   Upkeep / sec
//     Control        32             8
//     Panic          10             4
//     Stun           16            10
//     Probe           8             6
//     (regen)         -            +1
//
// Provenance for the initial/upkeep pairs mirrored below: BattleUnit::getPsiCost(),
// game/state/battle/battleunit.cpp:31-49 (as of this writing, lines 38-45 for the switch body).
//
// getPsiCost() itself cannot be called from this file: it is declared `static` at namespace
// scope in battleunit.h (internal linkage) with no out-of-line definition there, and its only
// definition lives in battleunit.cpp. Any other translation unit - including this one - that
// includes the header and calls it gets an unresolved-symbol link error. Making it callable would
// mean editing game/, which is out of scope for a lock test. psi_costs_match_prior_art therefore
// asserts the documented *initial* costs as named, cited constants: it freezes the documentation
// claim for a reviewer to diff against battleunit.cpp by eye, not the executable.
//
// The *upkeep* half of the table is not similarly stranded: BattleUnit::updatePsi() (public,
// battleunit.cpp:3747) is the real-time consumer that applies getPsiCost(status, /*attack=*/false)
// once per TICKS_PER_PSI_CHECK, and it needs no tile map, no Framework asset lists, and no LOS -
// verified by reading. psi_upkeep_per_second drives it directly, so it freezes the actual
// production debit, not a copy of it.

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

// psionics.txt initial ("if success attack begins") costs.
constexpr int PSI_INITIAL_CONTROL = 32;
constexpr int PSI_INITIAL_PANIC = 10;
constexpr int PSI_INITIAL_STUN = 16;
constexpr int PSI_INITIAL_PROBE = 8;

// psionics.txt upkeep, converted from "/sec" to "/half-second check" (psionics.txt: "applied
// each half second"; the code applies it once per TICKS_PER_PSI_CHECK, which is TICKS_PER_SECOND
// / 2 - see psi_check_cadence_is_half_second below).
constexpr int PSI_UPKEEP_PER_CHECK_CONTROL = 8 / 2;
// DIVERGENCE, deliberately locked to the CODE, not to psionics.txt.
// psionics.txt implies 2 per half-second check (4/sec); battleunit.cpp codes 3 (6/sec).
// Control/Stun/Probe all match the document exactly, and all four INITIAL costs match, which
// makes a transcription slip (3 for 2) more likely than the document being wrong - but that is
// an inference, not evidence, and TACP has not been asked. Per the parity prime directive the
// constant is NOT changed to suit a prior-art document.
// This asserts current behaviour so that editing battleunit.cpp becomes a deliberate act.
// Open row: docs/original-game/findings/A2-psi-panic-upkeep-divergence.md
constexpr int PSI_UPKEEP_PER_CHECK_PANIC = 3;
constexpr int PSI_UPKEEP_PER_CHECK_STUN = 10 / 2;
constexpr int PSI_UPKEEP_PER_CHECK_PROBE = 6 / 2;

// state.agent_types is a StateRefMap (std::map), so iteration is alphabetical by ID - the first
// role==Soldier && playable match is AGENTTYPE_ALIEN_GREY, whose equipment_layout has no
// RightHand-shaped slot a MindBender fits. Require an actual RightHand slot so this reliably
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

// Not every MindBender-type item fits a given agent's equipment_layout (alien-only items, wrong
// slot shape, etc.) - try each real one in turn and equip whichever actually fits, rather than
// assuming the first Type::MindBender entry found is wearable.
sp<AEquipment> equipFirstWorkingMindBender(GameState &state, StateRef<Agent> agent)
{
	for (auto &e : state.agent_equipment)
	{
		if (!e.second || e.second->type != AEquipmentType::Type::MindBender)
		{
			continue;
		}
		auto equipped =
		    agent->addEquipmentByType(state, {&state, e.first}, EquipmentSlotType::RightHand,
		                              /*allowFailure=*/true);
		if (equipped)
		{
			return equipped;
		}
	}
	return nullptr;
}

// Sets up an attacker mid-attack on a target (psiStatus/psiTarget/psiItem populated as
// BattleUnit::startAttackPsi() would leave them, without needing the LOS/Framework machinery
// startAttackPsi() itself requires), advances exactly one TICKS_PER_PSI_CHECK worth of real time
// via the real BattleUnit::updatePsi(), and returns how much psi_energy that single upkeep tick
// actually consumed.
int upkeepPerCheck(GameState &state, StateRef<Organisation> owner, StateRef<AgentType> agentType,
                   PsiStatus status)
{
	auto battle = mksp<Battle>();
	battle->mode = Battle::Mode::RealTime;
	battle->currentActiveOrganisation = owner;
	state.current_battle = battle;

	auto attacker = makeUnit(state, battle, owner, agentType);
	auto target = makeUnit(state, battle, owner, agentType);

	auto bender = equipFirstWorkingMindBender(state, attacker->agent);
	if (!bender)
	{
		return -1;
	}

	attacker->psiStatus = status;
	attacker->psiTarget = {&state, target->id};
	attacker->psiItem = bender->type;
	attacker->agent->modified_stats.psi_energy = 999;

	const int before = attacker->agent->modified_stats.psi_energy;
	attacker->updatePsi(state, TICKS_PER_PSI_CHECK);
	const int after = attacker->agent->modified_stats.psi_energy;
	return before - after;
}

} // namespace

static bool test_psi_costs_match_prior_art()
{
	// See the file-level comment: getPsiCost() has internal linkage and cannot be called from
	// here without editing game/. This freezes the documentation claim, not the executable -
	// psi_upkeep_per_second below is what actually exercises production code.
	TEST_REQUIRE(PSI_INITIAL_CONTROL == 32, "psionics.txt Control initial cost is 32");
	TEST_REQUIRE(PSI_INITIAL_PANIC == 10, "psionics.txt Panic initial cost is 10");
	TEST_REQUIRE(PSI_INITIAL_STUN == 16, "psionics.txt Stun initial cost is 16");
	TEST_REQUIRE(PSI_INITIAL_PROBE == 8, "psionics.txt Probe initial cost is 8");
	return true;
}

// Drives BattleUnit::updatePsi() - the real per-half-second upkeep debit - for each PsiStatus and
// checks it. Control, Stun and Probe match psionics.txt exactly. Panic does NOT: battleunit.cpp
// codes 3 per half-second (6/sec) where psionics.txt implies 2 (4/sec). That divergence is locked
// to the code and recorded as an open parity row rather than "fixed" in either direction - see
// PSI_UPKEEP_PER_CHECK_PANIC above.
static bool test_psi_upkeep_per_second()
{
	auto &state = *g_state;
	auto owner = state.getCivilian();
	auto agentType = findSoldierType(state);
	TEST_REQUIRE((bool)agentType, "no soldier agent type in loaded gamestate");

	const int control = upkeepPerCheck(state, owner, agentType, PsiStatus::Control);
	TEST_REQUIRE(control >= 0, "could not equip any MindBender-type item to test Control upkeep");
	TEST_CHECK(control == PSI_UPKEEP_PER_CHECK_CONTROL,
	           "Control upkeep per half-second check is {0}, psionics.txt implies {1} (8/sec)",
	           control, PSI_UPKEEP_PER_CHECK_CONTROL);

	const int panic = upkeepPerCheck(state, owner, agentType, PsiStatus::Panic);
	TEST_REQUIRE(panic >= 0, "could not equip any MindBender-type item to test Panic upkeep");
	TEST_CHECK(panic == PSI_UPKEEP_PER_CHECK_PANIC,
	           "Panic upkeep per half-second check is {0}, expected the locked current value {1}. "
	           "Note psionics.txt implies 2 (4/sec) - see the constant's comment before changing "
	           "either side",
	           panic, PSI_UPKEEP_PER_CHECK_PANIC);

	const int stun = upkeepPerCheck(state, owner, agentType, PsiStatus::Stun);
	TEST_REQUIRE(stun >= 0, "could not equip any MindBender-type item to test Stun upkeep");
	TEST_CHECK(stun == PSI_UPKEEP_PER_CHECK_STUN,
	           "Stun upkeep per half-second check is {0}, psionics.txt implies {1} (10/sec)", stun,
	           PSI_UPKEEP_PER_CHECK_STUN);

	const int probe = upkeepPerCheck(state, owner, agentType, PsiStatus::Probe);
	TEST_REQUIRE(probe >= 0, "could not equip any MindBender-type item to test Probe upkeep");
	TEST_CHECK(probe == PSI_UPKEEP_PER_CHECK_PROBE,
	           "Probe upkeep per half-second check is {0}, psionics.txt implies {1} (6/sec)", probe,
	           PSI_UPKEEP_PER_CHECK_PROBE);

	return true;
}

// psionics.txt: "Regen: 1/sec". BattleUnit::updateRegen() (public, battleunit.cpp:2328) applies
// psi regen once per TICKS_PER_SECOND of accumulated ticks - drive it directly.
static bool test_psi_regen_one_per_second()
{
	auto &state = *g_state;
	auto owner = state.getCivilian();
	auto agentType = findSoldierType(state);
	TEST_REQUIRE((bool)agentType, "no soldier agent type in loaded gamestate");

	auto battle = mksp<Battle>();
	battle->mode = Battle::Mode::RealTime;
	battle->currentActiveOrganisation = owner;
	state.current_battle = battle;

	auto unit = makeUnit(state, battle, owner, agentType);
	// Regen only fires while modified (current) psi_energy is below the stat ceiling.
	unit->agent->current_stats.psi_energy = 50;
	unit->agent->modified_stats.psi_energy = 40;

	unit->updateRegen(state, TICKS_PER_SECOND - 1);
	TEST_REQUIRE(unit->agent->modified_stats.psi_energy == 40,
	             "psi_energy changed to {0} before a full second accumulated",
	             unit->agent->modified_stats.psi_energy);

	unit->updateRegen(state, 1);
	TEST_REQUIRE(unit->agent->modified_stats.psi_energy == 41,
	             "psi_energy is {0} after exactly one second, expected +1 (41)",
	             unit->agent->modified_stats.psi_energy);

	return true;
}

// Makes the half-second cadence a first-class assertion: if a future TPS refactor changes
// TICKS_PER_SECOND without updating TICKS_PER_PSI_CHECK to match, this fails loudly instead of
// silently drifting the upkeep interval documented in psionics.txt ("applied each half second").
static bool test_psi_check_cadence_is_half_second()
{
	TEST_REQUIRE(TICKS_PER_PSI_CHECK * 2 == TICKS_PER_SECOND,
	             "TICKS_PER_PSI_CHECK ({0}) * 2 != TICKS_PER_SECOND ({1})", TICKS_PER_PSI_CHECK,
	             TICKS_PER_SECOND);
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

	// applyDamageDirect() (reached via updatePsi -> applyPsiAttack for Panic/Stun) is safe
	// without a window, but keep parity with test_base_die.cpp and construct one headlessly.
	Framework fw("OpenApoc", false);
	g_state = mksp<GameState>();
	if (!loadStartedGameState(*g_state, common, gamestate))
	{
		return EXIT_FAILURE;
	}

	const int rc = runTestSuite({
	    {"psi_costs_match_prior_art", test_psi_costs_match_prior_art},
	    {"psi_upkeep_per_second", test_psi_upkeep_per_second},
	    {"psi_regen_one_per_second", test_psi_regen_one_per_second},
	    {"psi_check_cadence_is_half_second", test_psi_check_cadence_is_half_second},
	});
	// Each battle-driving test above hand-builds a minimal Battle and points state.current_battle
	// at it, without the mission_type/mission_location_building a real battle would carry.
	// ~GameState() unconditionally calls Battle::finishBattle()/exitBattle() when current_battle
	// is set, which dereferences mission_location_building - clear it first so teardown doesn't
	// segfault on our synthetic battle.
	g_state->current_battle = nullptr;
	g_state.reset();
	return rc;
}
