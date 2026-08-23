#include "framework/configfile.h"
#include "game/state/battle/ai/tacticalaivanilla.h"
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

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}
	applyDeterministicTestConfig();
	return runTestSuite({
	    {"retreat_table", test_retreat_table},
	});
}
