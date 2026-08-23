#include "framework/configfile.h"
#include "game/state/gameevent.h"
#include "game/state/gameeventtypes.h"
#include "library/strings.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static bool test_defeat_messages()
{
	TEST_REQUIRE(GameEvent(GameEventType::AliensDefeated).message() ==
	                 tr("THE ALIENS ARE DEFEATED"),
	             "win banner");
	TEST_REQUIRE(GameEvent(GameEventType::XComDefeated).message() == tr("X-COM IS DEFEATED"),
	             "lose banner");
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
	    {"defeat_messages", test_defeat_messages},
	});
}
