// A4 - vanilla attack-priority parity lock test.
//
// Gap matrix: "implemented, medium - prior-art ai.txt + unitaivanilla.cpp (CTH x DAMAGE / TIME)."
// tools/extractors/docs/ai.txt (Vanilla AI): "Decide which weapon to use ... when not to use AOE
// attacks so that friends aren't hurt." The implementation is
// UnitAIVanilla::getWeaponDecision()/getGrenadeDecision() in
// game/state/battle/ai/unitaivanilla.cpp, documented there as:
//     // Priority is CTH * DAMAGE / TIME
//     float priority = cth * damage / time;
//
// THIS TEST CANNOT CALL THAT CODE, and no amount of restructuring the test fixes it without
// editing game/ (out of scope):
//   - getWeaponDecision() and getGrenadeDecision() are `private` members of UnitAIVanilla
//     (game/state/battle/ai/unitaivanilla.h) - unreachable from any other translation unit.
//   - The only public entry points (UnitAIVanilla::think(), or BattleUnit::aiList.think()) route
//     through getAttackDecision() -> canAttackUnit() -> hasLineToUnit() ->
//     tileObject->map.findCollision() (battleunit.cpp:1003-1069): a real, populated tile map is
//     mandatory just to reach the priority formula. No test in this suite constructs one (that
//     machinery is BattleMap::createBattle(), driven today only from the full mission-start path),
//     and building one from scratch is a large lift explicitly out of scope for a lock test.
//
// So: this file does NOT execute UnitAIVanilla's private decision code, and does not pretend to.
// What it locks instead:
//   1. weapon_priority_formula_ordering_and_inversion - the documented formula's *ordering
//      contract* (higher CTH*DAMAGE/TIME wins; the winner flips when the factors flip), evaluated
//      against synthetic (cth, damage, time) tuples defined in this file. This is a freeze of the
//      formula's math, not of unitaivanilla.cpp's execution of it.
//   2. weapon_priority_inputs_are_sane_for_real_weapons - every real Weapon-type AEquipmentType
//      loaded from the base gamestate has the positive (accuracy, damage, fire_delay) the formula
//      requires (a zero fire_delay would divide-by-zero the real formula; a zero/negative
//      damage or accuracy would make a weapon un-selectable) - a genuine regression guard against
//      data corruption, without claiming to test the selection logic itself.
//   3. aoe_rejected_when_friendly_in_blast_radius - the documented AOE friendly-fire rule
//      (getGrenadeDecision sums +hostileDamage/-friendlyDamage across everyone in blast radius
//      and rejects the throw if the net is negative - unitaivanilla.cpp:280-288), evaluated
//      against synthetic per-unit damage tuples, for the same private-method reason as above.
//
// Finding: A4 as literally specified ("construct a unit with two weapons ... assert the higher
// weapon is chosen") cannot be closed without a testable seam in unitaivanilla.cpp, e.g.
// extracting the priority computation to a public static the way
// TacticalAIVanilla::retreatChancePercent() already was for exactly this reason (see
// tests/test_tactical_ai_retreat.cpp). That refactor is game/ work and out of scope here.

#include "framework/configfile.h"
#include "framework/framework.h"
#include "game/state/gamestate.h"
#include "game/state/rules/aequipmenttype.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static sp<GameState> g_state;

namespace
{

// Mirrors unitaivanilla.cpp's documented "float priority = cth * damage / time;" verbatim. This
// is a local reimplementation of the formula's *math*, used only to freeze its ordering
// properties - it is not, and does not claim to be, the code under game/.
double documentedPriority(double cth, double damage, double time) { return cth * damage / time; }

// Mirrors the documented AOE friendly-fire rule: sum +hostile / -friendly damage across everyone
// in the blast, reject (return false) if the net is negative. See unitaivanilla.cpp:247-288,
// specifically the `damage += (hostile ? 1.0f : -1.0f) * localDamage;` accumulation and the
// `if (damage < 0.0f) return NULLTUPLE3;` rejection.
bool documentedAoeIsUsable(double hostileDamage, double friendlyDamage)
{
	return (hostileDamage - friendlyDamage) >= 0.0;
}

} // namespace

static bool test_weapon_priority_formula_ordering_and_inversion()
{
	// "Weapon A": high accuracy, modest damage, fast. "Weapon B": low accuracy, heavy damage,
	// slow. Chosen so A wins on the documented formula.
	const double cthA = 90.0, damageA = 20.0, timeA = 1.0;
	const double cthB = 40.0, damageB = 25.0, timeB = 1.0;
	const double priorityA = documentedPriority(cthA, damageA, timeA);
	const double priorityB = documentedPriority(cthB, damageB, timeB);
	TEST_REQUIRE(priorityA > priorityB,
	             "expected weapon A (cth*damage/time={0}) to beat weapon B ({1})", priorityA,
	             priorityB);

	// Invert every factor: what was worse is now better in each dimension. The ordering must
	// flip - this is "ordering is the contract, the absolute score is not" from the task.
	const double cthA2 = cthB, damageA2 = damageB, timeA2 = timeB;
	const double cthB2 = cthA, damageB2 = damageA, timeB2 = timeA;
	const double priorityA2 = documentedPriority(cthA2, damageA2, timeA2);
	const double priorityB2 = documentedPriority(cthB2, damageB2, timeB2);
	TEST_REQUIRE(priorityB2 > priorityA2,
	             "inverting all three factors did not invert the ordering ({0} vs {1})", priorityA2,
	             priorityB2);

	// A direct TIME-only inversion, holding CTH and DAMAGE fixed: a slower weapon must score
	// lower than an identical but faster one, all else equal.
	const double fast = documentedPriority(80.0, 30.0, 1.0);
	const double slow = documentedPriority(80.0, 30.0, 4.0);
	TEST_REQUIRE(fast > slow,
	             "faster weapon ({0}) did not beat slower otherwise-identical one ({1})", fast,
	             slow);

	return true;
}

static bool test_weapon_priority_inputs_are_sane_for_real_weapons()
{
	auto &state = *g_state;
	int weaponsChecked = 0;
	for (auto &e : state.agent_equipment)
	{
		if (!e.second || e.second->type != AEquipmentType::Type::Weapon)
		{
			continue;
		}
		// getPayloadType() falls back to the weapon itself when it has no separate ammo type;
		// either way fire_delay/damage/accuracy on *this* type are what a self-contained weapon
		// (no ammo_types) would feed the formula with. Weapons with ammo carry their own payload
		// stats on the ammo type instead, so restrict this check to self-contained weapons to
		// avoid conflating the two.
		if (!e.second->ammo_types.empty())
		{
			continue;
		}
		// AEquipment::canFire() requires ammo > 0, and a self-contained weapon's ammo is seeded
		// from max_ammo (Agent::addEquipmentByType(), agent.cpp:711-714). A weapon with
		// max_ammo == 0 can therefore never canFire(), so getAttackDecision() never adds it to
		// the candidate list in the first place (unitaivanilla.cpp:445-450) - it never reaches
		// the priority formula. AEQUIPMENTTYPE_FORCEWEB is exactly this: Type::Weapon with no
		// damage/accuracy/fire_delay/max_ammo at all, apparently a deployable shield item
		// mislabeled as a weapon rather than something the AI would ever evaluate for firing.
		if (e.second->max_ammo <= 0)
		{
			continue;
		}
		weaponsChecked++;
		TEST_CHECK(e.second->fire_delay > 0, "{0} has fire_delay {1} (formula divides by time)",
		           e.first, e.second->fire_delay);
		TEST_CHECK(e.second->damage >= 0, "{0} has negative damage {1}", e.first, e.second->damage);
		TEST_CHECK(e.second->accuracy >= 0, "{0} has negative accuracy {1}", e.first,
		           e.second->accuracy);
	}
	TEST_REQUIRE(weaponsChecked > 0, "no self-contained real weapons found to check");
	return true;
}

static bool test_aoe_rejected_when_friendly_in_blast_radius()
{
	// Two hostiles for modest damage, no friendlies: net damage positive, AOE usable.
	TEST_REQUIRE(documentedAoeIsUsable(/*hostileDamage=*/40.0, /*friendlyDamage=*/0.0),
	             "AOE with only hostiles in range should be usable");

	// A friendly deep in the blast outweighs a lightly-clipped hostile: net negative, rejected.
	TEST_REQUIRE(!documentedAoeIsUsable(/*hostileDamage=*/10.0, /*friendlyDamage=*/25.0),
	             "AOE with a friendly taking more damage than hostiles should be rejected");

	// Exactly break-even: the real code's condition is strictly `< 0.0f`, so a net of exactly
	// zero is still usable.
	TEST_REQUIRE(documentedAoeIsUsable(/*hostileDamage=*/15.0, /*friendlyDamage=*/15.0),
	             "AOE at exactly break-even net damage should still be usable (rejection is < 0)");

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

	Framework fw("OpenApoc", false);
	g_state = mksp<GameState>();
	if (!loadStartedGameState(*g_state, common, gamestate))
	{
		return EXIT_FAILURE;
	}

	const int rc = runTestSuite({
	    {"weapon_priority_formula_ordering_and_inversion",
	     test_weapon_priority_formula_ordering_and_inversion},
	    {"weapon_priority_inputs_are_sane_for_real_weapons",
	     test_weapon_priority_inputs_are_sane_for_real_weapons},
	    {"aoe_rejected_when_friendly_in_blast_radius",
	     test_aoe_rejected_when_friendly_in_blast_radius},
	});
	g_state.reset();
	return rc;
}
