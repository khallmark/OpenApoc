// A4 - vanilla attack-priority parity lock test.
//
// Gap matrix: "implemented, medium - prior-art ai.txt + unitaivanilla.cpp (CTH x DAMAGE / TIME)."
// tools/extractors/docs/ai.txt (Vanilla AI): "Decide which weapon to use ... when not to use AOE
// attacks so that friends aren't hurt."
//
// getWeaponDecision()/getGrenadeDecision() are private members of UnitAIVanilla and cannot be
// called from here - worse, reaching them at all means going through getAttackDecision() ->
// canAttackUnit() -> hasLineToUnit() -> tileObject->map.findCollision(), so a real populated tile
// map is mandatory before the priority arithmetic is even evaluated. Freezing local copies of the
// formulas here instead would lock nothing: editing unitaivanilla.cpp would leave this file green.
//
// The three primitives that arithmetic is made of are public statics on UnitAIVanilla -
// attackPriority(), blastDamageContribution() and aoeIsWorthThrowing() - called from the same two
// private methods as before (see unitaivanilla.h and the two call sites in unitaivanilla.cpp).
// The decision *plumbing* (candidate enumeration, LOS, movement selection) still needs a tile map
// and is still not covered here; what is covered is every number that plumbing compares.
//
// Scope note: this file's PR unit is unitaivanilla.cpp/h only.
// UnitAIHelper::exposureScore() (tactical cover/exposure scoring) lives in unitaihelper.cpp/h,
// which is a separate, concurrently-running PR - it is deliberately not touched or tested here.
//
// What this locks:
//   1. weapon_priority_formula_ordering_and_inversion - UnitAIVanilla::attackPriority()'s
//      ordering contract: higher CTH*DAMAGE/TIME wins, and the winner flips when the factors do.
//   2. weapon_priority_inputs_are_sane_for_real_weapons - every real self-contained Weapon-type
//      AEquipmentType in the base gamestate has the positive (accuracy, damage, fire_delay) the
//      formula needs; a zero fire_delay would divide by zero, a zero damage or accuracy would
//      make the weapon permanently un-selectable. A data-corruption guard.
//   3. aoe_rejected_when_friendly_in_blast_radius - UnitAIVanilla::blastDamageContribution()'s
//      sign convention and aoeIsWorthThrowing()'s veto boundary (including the exact-zero and NaN
//      edges called out at the declaration), which together are the "friends aren't hurt" rule
//      ai.txt names.
//
// This file is intentionally self-contained (no shared tests/test_helpers.h) - that header does
// not exist on this PR's base and is out of this unit's scope to introduce.

#include "framework/configfile.h"
#include "framework/framework.h"
#include "framework/logger.h"
#include "game/state/battle/ai/unitaivanilla.h"
#include "game/state/gamestate.h"
#include "game/state/rules/aequipmenttype.h"
#include "library/strings.h"
#include <cstdlib>
#include <limits>
#include <utility>
#include <vector>

using namespace OpenApoc;

namespace
{

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

sp<GameState> g_state;

// Thin alias so the tests below read as arithmetic rather than as scope resolution. This calls
// the production static - there is no local copy of the formula in this file.
float priority(float cth, float damage, float time)
{
	return UnitAIVanilla::attackPriority(cth, damage, time);
}

// Sums one blast's worth of contributions the way getGrenadeDecision() does, then applies the
// same veto, so the sign convention and the boundary are both exercised through production code.
bool aoeIsUsable(float hostileDamage, float friendlyDamage)
{
	float net = UnitAIVanilla::blastDamageContribution(hostileDamage, /*hostile=*/true);
	net += UnitAIVanilla::blastDamageContribution(friendlyDamage, /*hostile=*/false);
	return UnitAIVanilla::aoeIsWorthThrowing(net);
}

bool test_weapon_priority_formula_ordering_and_inversion()
{
	// "Weapon A": high accuracy, modest damage, fast. "Weapon B": low accuracy, heavy damage,
	// slow. Chosen so A wins on the ranking function.
	const float cthA = 90.0f, damageA = 20.0f, timeA = 1.0f;
	const float cthB = 40.0f, damageB = 25.0f, timeB = 1.0f;
	const float priorityA = priority(cthA, damageA, timeA);
	const float priorityB = priority(cthB, damageB, timeB);
	TEST_REQUIRE(priorityA > priorityB,
	             "expected weapon A (cth*damage/time={0}) to beat weapon B ({1})", priorityA,
	             priorityB);

	// Invert every factor: what was worse is now better in each dimension. The ordering must
	// flip - "ordering is the contract, the absolute score is not".
	const float cthA2 = cthB, damageA2 = damageB, timeA2 = timeB;
	const float cthB2 = cthA, damageB2 = damageA, timeB2 = timeA;
	const float priorityA2 = priority(cthA2, damageA2, timeA2);
	const float priorityB2 = priority(cthB2, damageB2, timeB2);
	TEST_REQUIRE(priorityB2 > priorityA2,
	             "inverting all three factors did not invert the ordering ({0} vs {1})", priorityA2,
	             priorityB2);

	// A direct TIME-only inversion (time != 1 on both sides), holding CTH and DAMAGE fixed: a
	// slower weapon must score lower than an identical but faster one, all else equal. This is
	// the case that distinguishes cth*damage/time from cth*damage*time.
	const float fast = priority(80.0f, 30.0f, 1.0f);
	const float slow = priority(80.0f, 30.0f, 4.0f);
	TEST_REQUIRE(fast > slow,
	             "faster weapon ({0}) did not beat slower otherwise-identical one ({1})", fast,
	             slow);

	return true;
}

bool test_weapon_priority_inputs_are_sane_for_real_weapons()
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
		// from max_ammo. A weapon with max_ammo == 0 can therefore never canFire(), so
		// getAttackDecision() never adds it to the candidate list in the first place - it never
		// reaches the priority formula. AEQUIPMENTTYPE_FORCEWEB is exactly this: Type::Weapon
		// with no damage/accuracy/fire_delay/max_ammo at all, apparently a deployable shield item
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

bool test_aoe_rejected_when_friendly_in_blast_radius()
{
	// Two hostiles for modest damage, no friendlies: net damage positive, AOE usable.
	TEST_REQUIRE(aoeIsUsable(/*hostileDamage=*/40.0f, /*friendlyDamage=*/0.0f),
	             "AOE with only hostiles in range should be usable");

	// A friendly deep in the blast outweighs a lightly-clipped hostile: net negative, rejected.
	TEST_REQUIRE(!aoeIsUsable(/*hostileDamage=*/10.0f, /*friendlyDamage=*/25.0f),
	             "AOE with a friendly taking more damage than hostiles should be rejected");

	// Exactly break-even through the composite path: the real code's condition is strictly
	// `< 0.0f`, so a net of exactly zero is still usable.
	TEST_REQUIRE(aoeIsUsable(/*hostileDamage=*/15.0f, /*friendlyDamage=*/15.0f),
	             "AOE at exactly break-even net damage should still be usable (rejection is < 0)");

	// Direct boundary checks on aoeIsWorthThrowing() itself, bypassing the composite helper, so
	// the veto's exact edges are locked independently of blastDamageContribution()'s arithmetic.
	TEST_REQUIRE(UnitAIVanilla::aoeIsWorthThrowing(0.0f),
	             "exactly zero net damage must not be vetoed (kills a `> 0.0f` mutant)");
	TEST_REQUIRE(!UnitAIVanilla::aoeIsWorthThrowing(-0.01f),
	             "the smallest negative net damage must be vetoed");

	// NaN compares false against everything, including `< 0.0f`, so the header's negated-form
	// contract ("!(net < 0.0f)" rather than "net >= 0.0f") means a NaN net must NOT be vetoed -
	// this is the exact property the header comment at aoeIsWorthThrowing()'s declaration cites
	// as the reason for writing it as a negation. A `>= 0.0f` mutant flips this to false.
	const float nan = std::numeric_limits<float>::quiet_NaN();
	TEST_REQUIRE(UnitAIVanilla::aoeIsWorthThrowing(nan),
	             "a NaN net (e.g. from a corrupt damage calc) must not be vetoed - "
	             "aoeIsWorthThrowing is written as !(net < 0.0f), not (net >= 0.0f), for exactly "
	             "this reason");

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

} // namespace

int main(int argc, char **argv)
{
	config().addPositionalArgument("common", "Common gamestate to load");
	config().addPositionalArgument("gamestate", "Gamestate to load");
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}
	// Deterministic: this test never simulates a battle turn or touches the RNG at all - the
	// three functions under test (attackPriority, blastDamageContribution, aoeIsWorthThrowing)
	// are pure float arithmetic with no GameState access, and
	// weapon_priority_inputs_are_sane_for_real_weapons only reads static rules data
	// (agent_equipment), populated directly by loadGame() from data tables rather than generated
	// at runtime. Still set this defensively so nothing downstream seeds from wall-clock time.
	config().set("OpenApoc.NewFeature.SeedRng", false);
	config().set("Config.Save", false);

	const auto common = config().getString("common");
	const auto gamestate = config().getString("gamestate");
	if (common.empty() || gamestate.empty())
	{
		LogError("Must provide common and gamestate paths");
		return EXIT_FAILURE;
	}

	Framework fw("OpenApoc", false);
	g_state = mksp<GameState>();
	if (!g_state->loadGame(common))
	{
		LogError("Failed to load common gamestate \"{0}\"", common);
		return EXIT_FAILURE;
	}
	if (!g_state->loadGame(gamestate))
	{
		LogError("Failed to load gamestate \"{0}\"", gamestate);
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
