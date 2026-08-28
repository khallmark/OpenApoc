#include "framework/configfile.h"
#include "game/state/battle/battle.h"
#include "game/state/battle/battlehazard.h"
#include "game/state/battle/battlemappart.h"
#include "game/state/battle/battleunit.h"
#include "game/state/city/vehicle.h"
#include "game/state/gametime.h"
#include "game/state/rules/aequipmenttype.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static bool test_made_up_and_tick_derived_constants()
{
	// HAZARD_SPREAD_CHANCE was deleted: F1's hazard spread RNG is now recovered
	// (docs/original-game/findings/B5-F1-K1-hazards.md F1 §2), see
	// hazard_spread_uses_recovered_rng / fire_neighbour_table_matches_recovered_bytes below.
	TEST_REQUIRE(TICKS_PER_TURN == TICKS_PER_SECOND * 4, "TICKS_PER_TURN {0} != 4*TPS",
	             TICKS_PER_TURN);
	TEST_REQUIRE(TICKS_PER_HAZARD_UPDATE == TICKS_PER_TURN / 2, "TICKS_PER_HAZARD_UPDATE {0}",
	             TICKS_PER_HAZARD_UPDATE);
	TEST_REQUIRE(TICKS_PER_WOUND_EFFECT == TICKS_PER_TURN, "TICKS_PER_WOUND_EFFECT {0}",
	             TICKS_PER_WOUND_EFFECT);
	TEST_REQUIRE(TICKS_PER_ENZYME_EFFECT == TICKS_PER_SECOND / 9, "TICKS_PER_ENZYME_EFFECT {0}",
	             TICKS_PER_ENZYME_EFFECT);
	TEST_REQUIRE(TICKS_PER_FIRE_EFFECT == TICKS_PER_SECOND, "TICKS_PER_FIRE_EFFECT {0}",
	             TICKS_PER_FIRE_EFFECT);
	TEST_REQUIRE(FUEL_TICKS_PER_SECOND == 144, "FUEL_TICKS_PER_SECOND is {0}, should track TPS",
	             FUEL_TICKS_PER_SECOND);
	TEST_REQUIRE(FUEL_TICKS_PER_SECOND == static_cast<int>(TICKS_PER_SECOND),
	             "FUEL_TICKS_PER_SECOND {0} drifted from TICKS_PER_SECOND {1}",
	             FUEL_TICKS_PER_SECOND, TICKS_PER_SECOND);
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
	    {"made_up_and_tick_derived_constants", test_made_up_and_tick_derived_constants},
	});
}
