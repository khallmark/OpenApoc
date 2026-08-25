#include "framework/configfile.h"
#include "game/state/battle/battleunit.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static bool test_mind_shield_increment()
{
	TEST_REQUIRE(BattleUnit::applyMindShieldIncrement(0) == 30, "0+30");
	TEST_REQUIRE(BattleUnit::applyMindShieldIncrement(30) == 60, "30+30");
	TEST_REQUIRE(BattleUnit::applyMindShieldIncrement(180) == 200, "180+30 caps at 200");
	TEST_REQUIRE(BattleUnit::applyMindShieldIncrement(200) == 200, "200 stays capped");
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
	    {"mind_shield_increment", test_mind_shield_increment},
	});
}
