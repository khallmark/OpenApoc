// G1 - Personal Disruptor Shield (equipment type 0x08) lock test.
//
// docs/original-game/findings/B3-G1-wounds-gadgets.md, "Disruptor Shield" section, is the
// specification. Summary of what is locked here:
//   - BattleUnit carries a unit-level capacity/current damage-absorption buffer
//     (disruptorShieldCapacity/disruptorShieldCurrent, mirroring TACP unit+0x256/+0x254).
//   - BattleUnit::applyDamage() absorbs against that buffer ahead of health, using the same
//     general damage-type-modified value the rest of the function already computes (bound
//     "Follow-up 1(b)": not a shield-specific resistance table).
//   - The absorption is ALL-OR-NOTHING (bound "Follow-up - overflow/passthrough", corrected from
//     an earlier decompiler-misled reading): if the buffer exceeds the type-modified damage nothing
//     reaches health; otherwise the buffer breaks and the FULL damage passes through unreduced -
//     the buffer's pre-hit value does not partially offset it. This decision is extracted as the
//     pure, static BattleUnit::resolveDisruptorShieldHit() and locked directly (see
//     test_disruptor_shield_overflow_is_all_or_nothing below) rather than only through
//     BattleUnit::applyDamage() end to end, because the "broken" branch of applyDamage() calls
//     Battle::placeDoodad(..., tileObject->getCenter()), which needs a real TileMap - out of
//     scope for a lock test, per tests/test_unit_ai_priority.cpp's own file header. The "fully
//     absorbed" branch has no such dependency and is still exercised end to end below.
//   - The buffer regenerates +1 every TICKS_PER_DISRUPTOR_SHIELD_REGEN (bound "Follow-up 1(a)":
//     once per real-time second).
//   - The new fields round-trip through save/load (game/state/gamestate_serialize.xml).
//
// Not covered here (out of scope for this row, or exercised elsewhere):
//   - The one-time full recharge on battle load (Battle::initBattle) - no synthetic Battle in
//     this file goes through initBattle, since doing so pulls in map/resource-list plumbing this
//     test does not otherwise need. The 1/tick regen path below already exercises the same
//     TICKS_PER_DISRUPTOR_SHIELD_REGEN constant that governs both, so this deliberately does not
//     duplicate that in a heavier test harness for a single extra assertion.

#include "framework/configfile.h"
#include "framework/filesystem.h"
#include "framework/framework.h"
#include "game/state/battle/battle.h"
// mksp<Battle>() instantiates ~Battle(), which needs complete types for every StateRefMap<T>
// member (units/scanners/doors) - battle.h only forward-declares BattleScanner/BattleDoor.
#include "game/state/battle/battledoor.h"
#include "game/state/battle/battlescanner.h"
#include "game/state/battle/battleunit.h"
#include "game/state/gamestate.h"
#include "game/state/gamestate_serialize.h"
#include "game/state/rules/aequipmenttype.h"
#include "game/state/rules/agenttype.h"
#include "game/state/rules/battle/damage.h"
#include "game/state/shared/aequipment.h"
#include "game/state/shared/agent.h"
#include "game/state/shared/equipment.h"
#include "library/sp.h"
#include "library/strings_format.h"
#include "tests/test_helpers.h"
#include <sstream>
#include <thread>

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static sp<GameState> g_state;

namespace
{

// A synthetic damage type, fully test-controlled rather than pulled from extracted CD data:
//   - ignore_shield = false, so the Disruptor Shield check in applyDamage() is reached.
//   - blockType = Gas: doesImpactDamage() is false for Gas, which skips applyDamage()'s
//     genericHitSounds sound-playing block - this test builds no battle_common_sample_list, so
//     that path would otherwise dereference a null sound list. Gas does not otherwise special-case
//     health damage (unlike Psionic, which zeroes it), so it is otherwise a plain hit.
//   - effectType = None, explosive = false: damage is drawn once via randDamage050150(power).
//   - modifiers: identity (100%) for whatever damage_modifier the test agent type resolves to
//     (no armor equipped => agent->type->damage_modifier), so the type-modified damage the shield
//     compares against is exactly the raw rolled damage - keeps the arithmetic legible without
//     needing to know the real extracted modifier table values.
StateRef<DamageType> makeTestDamageType(GameState &state, StateRef<DamageModifier> identityFor)
{
	static int counter = 0;
	auto dt = mksp<DamageType>();
	dt->name = "TEST_DISRUPTOR_SHIELD_DAMAGETYPE";
	dt->ignore_shield = false;
	dt->blockType = DamageType::BlockType::Gas;
	dt->effectType = DamageType::EffectType::None;
	dt->explosive = false;
	dt->modifiers[identityFor] = 100;
	UString id = format("DAMAGETYPE_TEST_DISRUPTOR_SHIELD_{}", counter++);
	state.damage_types[id] = dt;
	return {&state, id};
}

// state.agent_types is a StateRefMap (std::map), so iteration is alphabetical by ID - the first
// role==Soldier && playable match is AGENTTYPE_ALIEN_GREY, whose equipment_layout has no
// General-shaped slot a Disruptor Shield fits. Require an actual General slot so this reliably
// lands on a normal humanoid (e.g. AGENTTYPE_X-COM_AGENT_HUMAN) instead.
StateRef<AgentType> findSoldierTypeWithGeneralSlot(GameState &state)
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
			if (slot.type == EquipmentSlotType::General)
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

// Not every Disruptor Shield entry fits a given agent's equipment_layout - try each real one in
// turn and equip whichever actually fits, rather than assuming the first match is wearable.
sp<AEquipment> equipFirstWorkingDisruptorShield(GameState &state, StateRef<Agent> agent)
{
	for (auto &e : state.agent_equipment)
	{
		if (!e.second || e.second->type != AEquipmentType::Type::DisruptorShield)
		{
			continue;
		}
		auto equipped = agent->addEquipmentByType(state, {&state, e.first}, /*allowFailure=*/true);
		if (equipped)
		{
			return equipped;
		}
	}
	return nullptr;
}

} // namespace

// Locks the ALL-OR-NOTHING decision in BattleUnit::resolveDisruptorShieldHit() (the pure step
// BattleUnit::applyDamage()'s shield block delegates to - see its own extraction comment in
// battleunit.cpp for why): docs/original-game/findings/B3-G1-wounds-gadgets.md, "Follow-up -
// overflow/passthrough", corrected from an earlier decompiler-misled reading that had this as
// "damage minus the old buffer value" partial absorption. A buffer strictly greater than the
// incoming (type-modified) damage absorbs it fully; a buffer at or below it (including exact
// equality) breaks to 0 and the full damage is left for the caller to pass through unreduced.
static bool test_disruptor_shield_overflow_is_all_or_nothing()
{
	// current > damage: fully absorbed, buffer decreases by exactly the damage.
	auto fullyAbsorbed = BattleUnit::resolveDisruptorShieldHit(100, 40);
	TEST_REQUIRE(fullyAbsorbed.absorbed, "buffer 100 vs damage 40 should fully absorb");
	TEST_REQUIRE(fullyAbsorbed.remainingCurrent == 60,
	             "buffer 100 - damage 40 should leave 60, got {0}", fullyAbsorbed.remainingCurrent);

	// current == damage: the doc is explicit that equality breaks the shield too, not absorbs.
	auto exactMatch = BattleUnit::resolveDisruptorShieldHit(40, 40);
	TEST_REQUIRE(!exactMatch.absorbed, "buffer == damage must break the shield, not absorb it");
	TEST_REQUIRE(exactMatch.remainingCurrent == 0, "a broken buffer must read back as 0, got {0}",
	             exactMatch.remainingCurrent);

	// current < damage: this is the case an old "damage -= current" reading would get wrong -
	// only remainingCurrent (0, buffer state) is checked here; whether the caller then subtracts
	// current from damage (wrong) or leaves damage untouched (bound, correct) lives in
	// BattleUnit::applyDamage() itself, not in this pure function - see that function's own
	// comment for the citation.
	auto depleted = BattleUnit::resolveDisruptorShieldHit(1, 40);
	TEST_REQUIRE(!depleted.absorbed, "buffer 1 vs damage 40 should break, not absorb");
	TEST_REQUIRE(depleted.remainingCurrent == 0, "a broken buffer must read back as 0, got {0}",
	             depleted.remainingCurrent);

	return true;
}

// Integration check for the "fully absorbed" branch of BattleUnit::applyDamage() end to end
// (real code, not the extracted pure function): a buffer larger than any possible roll leaves
// health untouched and reports the hit as handled. The complementary "depleted, full damage
// passes through" branch cannot be integration-tested the same way here: past the shield check it
// calls Battle::placeDoodad(..., tileObject->getCenter()), and this suite builds no TileMap for
// exactly the reason tests/test_unit_ai_priority.cpp documents at its own file header (a real
// populated map is "a large lift explicitly out of scope for a lock test") - that branch's
// all-or-nothing arithmetic is locked directly above instead.
static bool test_disruptor_shield_absorbs_before_health()
{
	auto &state = *g_state;
	auto owner = state.getCivilian();
	auto agentType = findSoldierTypeWithGeneralSlot(state);
	TEST_REQUIRE((bool)agentType, "no soldier agent type with a General slot in loaded gamestate");

	auto battle = mksp<Battle>();
	battle->mode = Battle::Mode::RealTime;
	battle->currentActiveOrganisation = owner;
	state.current_battle = battle;

	auto fullyShielded = makeUnit(state, battle, owner, agentType);

	// applyDamage()'s shield check needs a real, worn Disruptor Shield item (for the type
	// modifier and the item->ammo sync) in addition to a non-zero disruptorShieldCurrent - the
	// unit-level buffer alone is not enough to reach the branch.
	TEST_REQUIRE((bool)equipFirstWorkingDisruptorShield(state, fullyShielded->agent),
	             "could not equip a Disruptor Shield on the fully-shielded test unit");

	auto damageType = makeTestDamageType(state, fullyShielded->agent->type->damage_modifier);
	const int power = 100;

	// Fully absorbed: buffer far exceeds any possible roll of randDamage050150(100) (50-150).
	fullyShielded->disruptorShieldCapacity = 100000;
	fullyShielded->disruptorShieldCurrent = 100000;
	const int healthBefore = fullyShielded->agent->modified_stats.health;
	state.rng.seed(0x5eed1);
	const bool fullyAbsorbedReturn = fullyShielded->applyDamage(
	    state, power, damageType, BodyPart::Body, DamageSource::Impact, {});
	TEST_REQUIRE(fullyAbsorbedReturn, "fully-absorbed hit should report handled (true)");
	TEST_REQUIRE(fullyShielded->agent->modified_stats.health == healthBefore,
	             "health changed ({0} -> {1}) despite a buffer far larger than the hit",
	             healthBefore, fullyShielded->agent->modified_stats.health);
	TEST_REQUIRE(fullyShielded->disruptorShieldCurrent < 100000,
	             "shield buffer {0} did not decrease after absorbing a hit",
	             fullyShielded->disruptorShieldCurrent);

	state.current_battle = nullptr;
	return true;
}

// docs/original-game/findings/B3-G1-wounds-gadgets.md, "Follow-up 1(a)": the buffer regenerates
// +1 every TICKS_PER_DISRUPTOR_SHIELD_REGEN (bound: once per real-time second). Drives
// BattleUnit::updateDisruptorShield() directly, mirroring test_psi_regen_one_per_second in
// tests/test_psionics.cpp.
static bool test_disruptor_shield_regenerates()
{
	auto &state = *g_state;
	auto owner = state.getCivilian();
	auto agentType = findSoldierTypeWithGeneralSlot(state);
	TEST_REQUIRE((bool)agentType, "no soldier agent type with a General slot in loaded gamestate");

	auto battle = mksp<Battle>();
	battle->mode = Battle::Mode::RealTime;
	battle->currentActiveOrganisation = owner;
	state.current_battle = battle;

	auto unit = makeUnit(state, battle, owner, agentType);
	auto shield = equipFirstWorkingDisruptorShield(state, unit->agent);
	TEST_REQUIRE((bool)shield, "could not equip any Disruptor Shield-type item to test regen");
	shield->ammo = 50;

	// First tick: rising-edge equip transfer grants capacity 100 and pulls in the item's charge
	// (bound FUN_00057A04, DISRUPTOR_SHIELD_CAPACITY_BONUS). Use up the leftover regen ticks this
	// first call consumes before asserting cadence, by driving it with 0 extra ticks.
	unit->updateDisruptorShield(state, 0);
	TEST_REQUIRE(unit->disruptorShieldCapacity == DISRUPTOR_SHIELD_CAPACITY_BONUS,
	             "capacity {0} != DISRUPTOR_SHIELD_CAPACITY_BONUS after equipping",
	             unit->disruptorShieldCapacity);
	TEST_REQUIRE(unit->disruptorShieldCurrent == 50,
	             "current {0} != transferred item charge (50) right after equipping",
	             unit->disruptorShieldCurrent);

	unit->updateDisruptorShield(state, TICKS_PER_DISRUPTOR_SHIELD_REGEN - 1);
	TEST_REQUIRE(unit->disruptorShieldCurrent == 50,
	             "current {0} changed before a full TICKS_PER_DISRUPTOR_SHIELD_REGEN accumulated",
	             unit->disruptorShieldCurrent);

	unit->updateDisruptorShield(state, 1);
	TEST_REQUIRE(unit->disruptorShieldCurrent == 51,
	             "current is {0} after exactly one TICKS_PER_DISRUPTOR_SHIELD_REGEN, expected +1 "
	             "(51)",
	             unit->disruptorShieldCurrent);

	state.current_battle = nullptr;
	return true;
}

// docs/original-game/findings/B3-G1-wounds-gadgets.md binds TWO writers of the shield's charge,
// and OpenApoc only implemented one of them:
//   - FUN_0006511C "regenerates the shield item's own charge field (equipment-instance-table +10,
//     a 16-bit value) by 1 per call, gated < 100", driven from the same per-unit tick dispatcher
//     and the same cadence gate (unit+0x254 < unit+0x256) as the buffer regen.
//   - FUN_0006508C decrements that same field on the fully-absorbed path (already implemented in
//     BattleUnit::applyDamage()).
// So absorption used to lower the item charge while regen raised only the unit-level buffer, and
// the two drifted apart - visible the moment FUN_00057A04's equip transfer re-reads the item
// charge (unequip/re-equip would snap a fully regenerated shield back down to its drained value).
//
// The second half of this test covers the other side of the same field: AEquipment::updateInner()
// applies a generic recharge to every item with recharge > 0, at TICKS_PER_RECHARGE
// (= TICKS_PER_TURN, one per four seconds) in real time and rechargeTB per turn - values
// hardcoded for the Disruptor Shield in tools/extractors/extract_agent_equipment.cpp since 2016.
// The shield's charge has exactly one bound regen, +1 per real-time second via the unit
// dispatcher, so the generic path is a second regen of the same field at a rate nothing supports,
// and "Follow-up 1(a)-continued" additionally binds the only full recharge to battle load.
static bool test_disruptor_shield_item_charge_tracks_the_buffer()
{
	auto &state = *g_state;
	auto owner = state.getCivilian();
	auto agentType = findSoldierTypeWithGeneralSlot(state);
	TEST_REQUIRE((bool)agentType, "no soldier agent type with a General slot in loaded gamestate");

	auto battle = mksp<Battle>();
	battle->mode = Battle::Mode::RealTime;
	battle->currentActiveOrganisation = owner;
	state.current_battle = battle;

	auto unit = makeUnit(state, battle, owner, agentType);
	auto shield = equipFirstWorkingDisruptorShield(state, unit->agent);
	TEST_REQUIRE((bool)shield, "could not equip any Disruptor Shield-type item");
	shield->ammo = 40;

	unit->updateDisruptorShield(state, 0);
	TEST_REQUIRE(unit->disruptorShieldCurrent == 40,
	             "current {0} != transferred item charge (40) right after equipping",
	             unit->disruptorShieldCurrent);

	unit->updateDisruptorShield(state, TICKS_PER_DISRUPTOR_SHIELD_REGEN);
	TEST_REQUIRE(unit->disruptorShieldCurrent == 41, "buffer is {0} after one regen tick, expected 41",
	             unit->disruptorShieldCurrent);
	TEST_REQUIRE(shield->ammo == 41,
	             "item charge is {0} after one regen tick but the buffer is {1} - FUN_0006511C "
	             "regenerates the item's charge field too, not just the unit-level buffer",
	             shield->ammo, unit->disruptorShieldCurrent);

	// Unequip/re-equip must round-trip the charge rather than snapping it to the item's stale
	// value: FUN_00057A04's transfer reads the item charge, so the two must already agree.
	unit->agent->removeEquipment(state, shield);
	unit->updateDisruptorShield(state, 1);
	TEST_REQUIRE(unit->disruptorShieldCapacity == 0 && unit->disruptorShieldCurrent == 0,
	             "buffer should collapse with no shield worn, got capacity {0} current {1}",
	             unit->disruptorShieldCapacity, unit->disruptorShieldCurrent);
	unit->agent->addEquipment(state, shield, EquipmentSlotType::General);
	unit->updateDisruptorShield(state, 0);
	TEST_REQUIRE(unit->disruptorShieldCurrent == 41,
	             "re-equipping restored the buffer to {0}, expected the 41 it was carrying",
	             unit->disruptorShieldCurrent);

	// The generic AEquipment recharge must not act as a second, faster regen of the same field.
	const int chargeBefore = shield->ammo;
	shield->updateInner(state, TICKS_PER_TURN + 1);
	TEST_REQUIRE(shield->ammo == chargeBefore,
	             "AEquipment::updateInner() raised the shield charge {0} -> {1}; the shield's only "
	             "bound regen is +1 per real-time second through BattleUnit::updateDisruptorShield",
	             chargeBefore, shield->ammo);

	state.current_battle = nullptr;
	return true;
}

// Freezes the bound cadence constant itself: if a future TPS refactor changes TICKS_PER_SECOND
// without updating TICKS_PER_DISRUPTOR_SHIELD_REGEN to match, this fails loudly instead of
// silently drifting the regen rate the findings doc bound to "once per real-time second".
static bool test_disruptor_shield_regen_cadence_is_one_second()
{
	TEST_REQUIRE(TICKS_PER_DISRUPTOR_SHIELD_REGEN == TICKS_PER_SECOND,
	             "TICKS_PER_DISRUPTOR_SHIELD_REGEN ({0}) != TICKS_PER_SECOND ({1})",
	             TICKS_PER_DISRUPTOR_SHIELD_REGEN, TICKS_PER_SECOND);
	return true;
}

// game/state/gamestate_serialize.xml must carry disruptorShieldCapacity/disruptorShieldCurrent/
// disruptorShieldRegenTicksAccumulated, or a save/load cycle silently resets every worn shield's
// buffer to defaults.
static bool test_disruptor_shield_serializes_roundtrip()
{
	auto &state = *g_state;
	auto owner = state.getCivilian();
	auto agentType = findSoldierTypeWithGeneralSlot(state);
	TEST_REQUIRE((bool)agentType, "no soldier agent type with a General slot in loaded gamestate");

	auto battle = mksp<Battle>();
	battle->mode = Battle::Mode::RealTime;
	battle->currentActiveOrganisation = owner;
	state.current_battle = battle;

	auto unit = makeUnit(state, battle, owner, agentType);
	unit->disruptorShieldCapacity = 73;
	unit->disruptorShieldCurrent = 42;
	unit->disruptorShieldRegenTicksAccumulated = 17;
	const UString unitId = unit->id;

	std::stringstream ss;
	ss << "openapoc_test_disruptor_shield_serialize-" << std::this_thread::get_id();
	const auto tempFsPath = fs::temp_directory_path() / ss.str();
	const UString tempPath(tempFsPath.string());

	TEST_REQUIRE(state.saveGame(tempPath), "failed to save gamestate with a Disruptor Shield unit");

	auto reloaded = mksp<GameState>();
	const bool loaded = reloaded->loadGame(tempPath);
	fs::remove(tempFsPath);
	TEST_REQUIRE(loaded, "failed to load saved gamestate back");

	auto it = reloaded->current_battle ? reloaded->current_battle->units.find(unitId)
	                                   : reloaded->current_battle->units.end();
	TEST_REQUIRE(reloaded->current_battle && it != reloaded->current_battle->units.end(),
	             "unit {0} missing from reloaded gamestate", unitId);
	TEST_REQUIRE(it->second->disruptorShieldCapacity == 73,
	             "disruptorShieldCapacity {0} != 73 after roundtrip",
	             it->second->disruptorShieldCapacity);
	TEST_REQUIRE(it->second->disruptorShieldCurrent == 42,
	             "disruptorShieldCurrent {0} != 42 after roundtrip",
	             it->second->disruptorShieldCurrent);
	TEST_REQUIRE(it->second->disruptorShieldRegenTicksAccumulated == 17,
	             "disruptorShieldRegenTicksAccumulated {0} != 17 after roundtrip",
	             it->second->disruptorShieldRegenTicksAccumulated);

	reloaded->current_battle = nullptr;
	reloaded.reset();
	state.current_battle = nullptr;
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

	// Headless, matching test_psionics.cpp / test_base_die.cpp.
	Framework fw("OpenApoc", false);
	g_state = mksp<GameState>();
	if (!loadStartedGameState(*g_state, common, gamestate))
	{
		return EXIT_FAILURE;
	}

	const int rc = runTestSuite({
	    {"disruptor_shield_overflow_is_all_or_nothing",
	     test_disruptor_shield_overflow_is_all_or_nothing},
	    {"disruptor_shield_absorbs_before_health", test_disruptor_shield_absorbs_before_health},
	    {"disruptor_shield_regenerates", test_disruptor_shield_regenerates},
	    {"disruptor_shield_item_charge_tracks_the_buffer",
	     test_disruptor_shield_item_charge_tracks_the_buffer},
	    {"disruptor_shield_regen_cadence_is_one_second",
	     test_disruptor_shield_regen_cadence_is_one_second},
	    {"disruptor_shield_serializes_roundtrip", test_disruptor_shield_serializes_roundtrip},
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
