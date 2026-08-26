#include "framework/configfile.h"
#include "game/state/battle/ai/unitaibehavior.h"
#include "game/state/battle/battleunit.h"
#include "tests/test_helpers.h"
#include <iostream>

using namespace OpenApoc;
using Mode = BattleUnit::BehaviorMode;

// OpenApoc.AlienAI.Behaviour was declared, dumped with the rest of the options, and read by
// NOTHING. Ninety battles of adversarial co-evolution passed it on every launch with no effect --
// which is why the alien side never adapted: it was searching one live dimension (cover bias)
// against X-COM's eight, and its genome wandered rather than converging.
//
// Tested by CALLING the mapping rather than by checking the option round-trips through config.
// A round-trip test would have passed for the entire time this option did nothing.
int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	// Each option value selects the engine's own mode. "cautious" is ai.txt's name for Evasive.
	TEST_REQUIRE(alienBehaviorFromConfig("aggressive", Mode::Normal) == Mode::Aggressive,
	             "aggressive maps to Aggressive");
	TEST_REQUIRE(alienBehaviorFromConfig("normal", Mode::Aggressive) == Mode::Normal,
	             "normal maps to Normal");
	TEST_REQUIRE(alienBehaviorFromConfig("cautious", Mode::Normal) == Mode::Evasive,
	             "cautious maps to Evasive, which is what ai.txt calls it");
	TEST_REQUIRE(alienBehaviorFromConfig("evasive", Mode::Normal) == Mode::Evasive,
	             "the engine's own spelling works too");

	// The shipped default is empty and must change nothing, so a run that never sets it behaves
	// exactly as before. Same for a typo -- silently picking a mode would be worse than ignoring.
	TEST_REQUIRE(alienBehaviorFromConfig("", Mode::Aggressive) == Mode::Aggressive,
	             "empty leaves the mode alone");
	TEST_REQUIRE(alienBehaviorFromConfig("nonsense", Mode::Evasive) == Mode::Evasive,
	             "an unrecognised value leaves the mode alone rather than guessing");

	// The option must also be reachable through the config the AI reads it from.
	config().set("OpenApoc.AlienAI.Behaviour", UString("cautious"));
	TEST_REQUIRE(alienBehaviorFromConfig(config().getString("OpenApoc.AlienAI.Behaviour"),
	                                     Mode::Normal) == Mode::Evasive,
	             "the value set on the command line reaches the mapping");

	std::cout << "alien AI behaviour option maps to real modes and is read by the AI\n";
	return EXIT_SUCCESS;
}
