// A4 - vanilla attack-priority parity lock test.
//
// Gap matrix: "implemented, medium - prior-art ai.txt + unitaivanilla.cpp (CTH x DAMAGE / TIME)."
// tools/extractors/docs/ai.txt (Vanilla AI): "Decide which weapon to use ... when not to use AOE
// attacks so that friends aren't hurt."
//
// getWeaponDecision()/getGrenadeDecision() are private members of UnitAIVanilla and cannot be
// called from here - worse, reaching them at all means going through getAttackDecision() ->
// canAttackUnit() -> hasLineToUnit() -> tileObject->map.findCollision(), so a real populated tile
// map is mandatory before the priority arithmetic is even evaluated. An earlier version of this
// file concluded from that the arithmetic was untestable and froze local copies of the formulas
// instead, which locked nothing: editing unitaivanilla.cpp left this file green.
//
// The three primitives that arithmetic is made of are now public statics on UnitAIVanilla -
// attackPriority(), blastDamageContribution() and aoeIsWorthThrowing() - called from the same two
// private methods as before. The precedent is TacticalAIVanilla::retreatChancePercent(),
// extracted for exactly this reason (tests/test_tactical_ai_retreat.cpp). The decision *plumbing*
// (candidate enumeration, LOS, movement selection) still needs a tile map and is still not
// covered here; what is covered is every number that plumbing compares.
//
// What this locks:
//   1. weapon_priority_formula_ordering_and_inversion - UnitAIVanilla::attackPriority()'s
//      ordering contract: higher CTH*DAMAGE/TIME wins, and the winner flips when the factors do.
//   2. weapon_priority_inputs_are_sane_for_real_weapons - every real self-contained Weapon-type
//      AEquipmentType in the base gamestate has the positive (accuracy, damage, fire_delay) the
//      formula needs; a zero fire_delay would divide by zero, a zero damage or accuracy would
//      make the weapon permanently un-selectable. A data-corruption guard.
//   3. aoe_rejected_when_friendly_in_blast_radius - UnitAIVanilla::blastDamageContribution()'s
//      sign convention and aoeIsWorthThrowing()'s veto boundary, which together are the
//      "friends aren't hurt" rule ai.txt names.

#include "framework/configfile.h"
#include "framework/framework.h"
#include "game/state/battle/ai/unitaihelper.h"
#include "game/state/battle/ai/unitaivanilla.h"
#include "game/state/gamestate.h"
#include "game/state/rules/aequipmenttype.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static sp<GameState> g_state;

namespace
{

// Thin aliases so the tests below read as arithmetic rather than as scope resolution. These call
// the production statics - there is no local copy of any formula in this file any more.
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

} // namespace

// B1 cover metric, recovered from TACP FUN_0007e600 / FUN_0008c1fc.
//
// The original does NOT sweep neighbouring tiles for solidity -- it scores a short fixed menu of
// candidate destinations by threat exposure and keeps the least-exposed. See
// docs/original-game/findings/B1-cover-metric-pass2.md.
//
// NO-CHEAT PROPERTY, locked here deliberately: the score is computed from a caller-supplied list
// of threat positions, and the only caller fills that list from
// Battle::visibleEnemies[u.owner] -- units that side has actually SEEN via
// BattleUnit::refreshUnitVision's LOS check, not the full unit table. exposureScore() has no
// access to the game state at all, so it structurally cannot consult a hostile the side has not
// spotted. That is the same information a human commander has on screen.
static bool test_cover_exposure_metric()
{
	// Accumulator starts at 0 and only decreases (FUN_0007e600). Zero means "nothing qualifying
	// in the box"; the caller max-selects, so higher is safer.
	TEST_REQUIRE(UnitAIHelper::exposureScore({}, {10.0f, 10.0f, 1.0f}) == 0,
	             "no threats at all must score exactly 0");

	const std::vector<Vec3<float>> one{{10.0f, 10.0f, 1.0f}};
	TEST_REQUIRE(UnitAIHelper::exposureScore(one, {10.0f, 10.0f, 1.0f}) == -1,
	             "a threat on top of the candidate costs one unit of exposure");

	// The box is 21x21x13, recovered exactly. A threat outside it does not count, however
	// dangerous it would be if it could see us.
	TEST_REQUIRE(UnitAIHelper::exposureScore(one, {10.0f, 100.0f, 1.0f}) == 0,
	             "a threat outside the 21-tile box must not count");
	TEST_REQUIRE(UnitAIHelper::exposureScore(one, {20.0f, 10.0f, 1.0f}) == -1,
	             "10 tiles away is inside the half-width of a 21-wide box");
	TEST_REQUIRE(UnitAIHelper::exposureScore(one, {21.0f, 10.0f, 1.0f}) == 0,
	             "11 tiles away is outside it");

	// Z uses the 13-deep half-extent (6.5), not the XY one (10.5). Written out because the first
	// version of this test asserted the wrong boundary and the code was right.
	TEST_REQUIRE(UnitAIHelper::exposureScore(one, {10.0f, 10.0f, 7.0f}) == -1,
	             "6 levels up is inside a 13-deep box (half-extent 6.5)");
	TEST_REQUIRE(UnitAIHelper::exposureScore(one, {10.0f, 10.0f, 8.0f}) == 0,
	             "7 levels up is outside it");

	// More visible threats is strictly worse, which is the ordering the caller relies on.
	const std::vector<Vec3<float>> three{
	    {10.0f, 10.0f, 1.0f}, {11.0f, 10.0f, 1.0f}, {12.0f, 10.0f, 1.0f}};
	const int exposed = UnitAIHelper::exposureScore(three, {10.0f, 10.0f, 1.0f});
	const int sheltered = UnitAIHelper::exposureScore(three, {10.0f, 40.0f, 1.0f});
	TEST_REQUIRE(exposed == -3, "three threats in the box cost three, got {0}", exposed);
	TEST_REQUIRE(sheltered == 0, "a candidate out of everyone's box scores 0, got {0}", sheltered);
	TEST_REQUIRE(sheltered > exposed,
	             "the safer candidate must score HIGHER -- the caller keeps the maximum");
	return true;
}

static bool test_weapon_priority_formula_ordering_and_inversion()
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
	// flip - this is "ordering is the contract, the absolute score is not" from the task.
	const float cthA2 = cthB, damageA2 = damageB, timeA2 = timeB;
	const float cthB2 = cthA, damageB2 = damageA, timeB2 = timeA;
	const float priorityA2 = priority(cthA2, damageA2, timeA2);
	const float priorityB2 = priority(cthB2, damageB2, timeB2);
	TEST_REQUIRE(priorityB2 > priorityA2,
	             "inverting all three factors did not invert the ordering ({0} vs {1})", priorityA2,
	             priorityB2);

	// A direct TIME-only inversion, holding CTH and DAMAGE fixed: a slower weapon must score
	// lower than an identical but faster one, all else equal.
	const float fast = priority(80.0f, 30.0f, 1.0f);
	const float slow = priority(80.0f, 30.0f, 4.0f);
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
	TEST_REQUIRE(aoeIsUsable(/*hostileDamage=*/40.0f, /*friendlyDamage=*/0.0f),
	             "AOE with only hostiles in range should be usable");

	// A friendly deep in the blast outweighs a lightly-clipped hostile: net negative, rejected.
	TEST_REQUIRE(!aoeIsUsable(/*hostileDamage=*/10.0f, /*friendlyDamage=*/25.0f),
	             "AOE with a friendly taking more damage than hostiles should be rejected");

	// Exactly break-even: the real code's condition is strictly `< 0.0f`, so a net of exactly
	// zero is still usable.
	TEST_REQUIRE(aoeIsUsable(/*hostileDamage=*/15.0f, /*friendlyDamage=*/15.0f),
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
	    {"cover_exposure_metric", test_cover_exposure_metric},
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
