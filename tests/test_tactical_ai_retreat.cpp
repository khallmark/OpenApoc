#include "framework/configfile.h"
#include "game/state/battle/ai/tacticalaivanilla.h"
#include "game/state/battle/ai/unitailowmorale.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static bool expectChance(int total, int active, int expected)
{
	const int got = TacticalAIVanilla::retreatChancePercent(total, active);
	TEST_REQUIRE(got == expected, "retreatChancePercent({0},{1}) = {2}, expected {3}", total,
	             active, got, expected);
	return true;
}

static bool test_retreat_table()
{
	TEST_REQUIRE(expectChance(10, 10, 0), "10/10");
	TEST_REQUIRE(expectChance(10, 6, 0), "6/10");
	TEST_REQUIRE(expectChance(10, 5, 0), "5/10");
	TEST_REQUIRE(expectChance(10, 4, 10), "4/10");
	TEST_REQUIRE(expectChance(10, 0, 50), "0/10");
	TEST_REQUIRE(expectChance(1, 1, 0), "1/1");
	TEST_REQUIRE(expectChance(0, 0, 0), "0/0");
	return true;
}

static bool test_panic_run_farther_from_enemy()
{
	const Vec3<float> enemy{0.0f, 0.0f, 0.0f};
	const Vec3<float> current{4.0f, 0.0f, 0.0f};
	TEST_REQUIRE(UnitAILowMorale::isFartherFromEnemy({8.0f, 0.0f, 0.0f}, current, enemy),
	             "farther block rejected");
	TEST_REQUIRE(!UnitAILowMorale::isFartherFromEnemy({2.0f, 0.0f, 0.0f}, current, enemy),
	             "closer block accepted");
	TEST_REQUIRE(!UnitAILowMorale::isFartherFromEnemy(current, current, enemy),
	             "same-distance block accepted");
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
	    {"retreat_table", test_retreat_table},
	    {"panic_run_farther_from_enemy", test_panic_run_farther_from_enemy},
	});
}
