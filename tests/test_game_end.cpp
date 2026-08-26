#include "framework/configfile.h"
#include "game/state/gameevent.h"
#include "game/state/gameeventtypes.h"
#include "game/state/gamestate.h"
#include "game/state/shared/organisation.h"
#include "library/sp.h"
#include "library/strings.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static bool test_alien_takeover_copy()
{
	GameState state;
	auto org = mksp<Organisation>();
	org->name = "Megapol";
	state.organisations["ORG_MEGAPOL"] = org;
	GameOrganisationEvent ev(GameEventType::AlienTakeover, {&state, "ORG_MEGAPOL"});
	const auto text = ev.message();
	TEST_REQUIRE(text.find("ALIEN TAKEOVER") != UString::npos, "missing title");
	TEST_REQUIRE(text.find("Megapol") != UString::npos, "missing org name");
	TEST_REQUIRE(text.find("Our intelligence sources have informed us that the Aliens have taken "
	                       "control of this organization.") != UString::npos,
	             "missing UFO2P 0x14D340 body");
	TEST_REQUIRE(text.find("This must not be the end. We will fight on.") != UString::npos,
	             "missing closing sentence");
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
	    {"alien_takeover_copy", test_alien_takeover_copy},
	});
}
