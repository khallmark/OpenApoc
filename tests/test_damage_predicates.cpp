#include "framework/configfile.h"
#include "game/state/rules/battle/damage.h"
#include "tests/test_helpers.h"
#include <array>

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static bool expectedDissipation(DamageType::BlockType block)
{
	return block != DamageType::BlockType::Gas && block != DamageType::BlockType::Psionic;
}

static bool expectedImpact(DamageType::BlockType block)
{
	return block != DamageType::BlockType::Gas && block != DamageType::BlockType::Psionic;
}

static bool expectedArmorDamage(DamageType::EffectType effect)
{
	return effect != DamageType::EffectType::Stun && effect != DamageType::EffectType::Smoke &&
	       effect != DamageType::EffectType::Fire && effect != DamageType::EffectType::Psionic;
}

static bool expectedFatalWounds(DamageType::EffectType effect)
{
	return effect != DamageType::EffectType::Stun && effect != DamageType::EffectType::Smoke &&
	       effect != DamageType::EffectType::Psionic;
}

static bool test_predicates()
{
	const std::array<DamageType::BlockType, 4> blocks = {
	    DamageType::BlockType::Physical, DamageType::BlockType::Psionic, DamageType::BlockType::Gas,
	    DamageType::BlockType::Fire};
	const std::array<DamageType::EffectType, 7> effects = {
	    DamageType::EffectType::None,   DamageType::EffectType::Stun,
	    DamageType::EffectType::Smoke,  DamageType::EffectType::Fire,
	    DamageType::EffectType::Enzyme, DamageType::EffectType::Brainsucker,
	    DamageType::EffectType::Psionic};

	for (auto block : blocks)
	{
		for (auto effect : effects)
		{
			DamageType dt;
			dt.blockType = block;
			dt.effectType = effect;
			TEST_CHECK(dt.hasDamageDissipation() == expectedDissipation(block),
			           "hasDamageDissipation mismatch");
			TEST_CHECK(dt.doesImpactDamage() == expectedImpact(block), "doesImpactDamage mismatch");
			TEST_CHECK(dt.alwaysImpactsHead() == (block == DamageType::BlockType::Gas),
			           "alwaysImpactsHead mismatch");
			TEST_CHECK(dt.ignoresArmorValue() == (effect == DamageType::EffectType::Smoke),
			           "ignoresArmorValue mismatch");
			TEST_CHECK(dt.dealsArmorDamage() == expectedArmorDamage(effect),
			           "dealsArmorDamage mismatch");
			TEST_CHECK(dt.dealsFatalWounds() == expectedFatalWounds(effect),
			           "dealsFatalWounds mismatch");
			TEST_CHECK(dt.dealsStunDamage() == (effect == DamageType::EffectType::Stun ||
			                                    effect == DamageType::EffectType::Smoke),
			           "dealsStunDamage mismatch");
		}
	}
	return true;
}

static bool test_deal_damage()
{
	DamageType dt;
	StateRef<DamageModifier> known{nullptr, "DAMAGEMODIFIER_TEST"};
	dt.modifiers[known] = 50;
	TEST_REQUIRE(dt.dealDamage(100, known) == 50, "known modifier should scale 100 by 50%");
	TEST_REQUIRE(dt.dealDamage(7, known) == 3, "7 * 50 / 100 should be 3");
	StateRef<DamageModifier> missing{nullptr, "DAMAGEMODIFIER_MISSING"};
	TEST_REQUIRE(dt.dealDamage(100, missing) == 100,
	             "missing modifier LogErrors and returns raw damage");
	return true;
}

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}
	applyDeterministicTestConfig();
	return runTestSuite({
	    {"predicates", test_predicates},
	    {"deal_damage", test_deal_damage},
	});
}
