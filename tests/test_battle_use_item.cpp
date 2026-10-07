#include "framework/configfile.h"
#include "game/state/battle/battleunit.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

// TACP FUN_00066474: a held Mind Shield sets psi defence to min(base + 30, 200) every tick. It is
// a level, not an increment - the previous version added 30 per use, so three uses read 90.
static bool test_mind_shield_psi_defence()
{
	TEST_REQUIRE(BattleUnit::mindShieldPsiDefence(40, true) == 70, "held: base + 30");
	TEST_REQUIRE(BattleUnit::mindShieldPsiDefence(40, false) == 40, "not held: base");
	TEST_REQUIRE(BattleUnit::mindShieldPsiDefence(185, true) == 200, "capped at 200");
	const int once = BattleUnit::mindShieldPsiDefence(40, true);
	TEST_REQUIRE(BattleUnit::mindShieldPsiDefence(40, true) == once,
	             "re-applying is idempotent: the bonus does not stack");
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
	    {"mind_shield_psi_defence", test_mind_shield_psi_defence},
	});
}
