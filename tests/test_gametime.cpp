#include "framework/configfile.h"
#include "game/state/city/vehicle.h"
#include "game/state/gametime.h"
#include "game/state/shared/agent.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static bool test_tick_constants()
{
	TEST_REQUIRE(VANILLA_TICKS_PER_SECOND == 36, "VANILLA_TICKS_PER_SECOND is {0}",
	             VANILLA_TICKS_PER_SECOND);
	TEST_REQUIRE(TICKS_MULTIPLIER == 4, "TICKS_MULTIPLIER is {0}", TICKS_MULTIPLIER);
	TEST_REQUIRE(TICKS_PER_SECOND == 144, "TICKS_PER_SECOND is {0}", TICKS_PER_SECOND);
	TEST_REQUIRE(TICKS_PER_MINUTE == TICKS_PER_SECOND * 60, "TICKS_PER_MINUTE is {0}",
	             TICKS_PER_MINUTE);
	TEST_REQUIRE(TICKS_PER_HOUR == TICKS_PER_MINUTE * 60, "TICKS_PER_HOUR is {0}", TICKS_PER_HOUR);
	TEST_REQUIRE(TICKS_PER_DAY == TICKS_PER_HOUR * 24, "TICKS_PER_DAY is {0}", TICKS_PER_DAY);
	return true;
}

static bool test_midday()
{
	auto midday = GameTime::midday();
	TEST_REQUIRE(midday.getTicks() == TICKS_PER_HOUR * 12, "midday ticks {0}", midday.getTicks());
	TEST_REQUIRE(midday.getHours() == 12, "midday hours {0}", midday.getHours());
	return true;
}

static bool test_add_ticks_flags()
{
	{
		GameTime t;
		t.addTicks(TICKS_PER_SECOND);
		TEST_REQUIRE(t.secondPassed(), "adding one second did not set secondPassed");
		TEST_REQUIRE(!t.fiveMinutesPassed(), "adding one second set fiveMinutesPassed");
		TEST_REQUIRE(!t.hourPassed(), "adding one second set hourPassed");
		TEST_REQUIRE(!t.dayPassed(), "adding one second set dayPassed");
		TEST_REQUIRE(!t.weekPassed(), "adding one second set weekPassed");
	}
	{
		GameTime t;
		t.addTicks(5 * TICKS_PER_MINUTE);
		TEST_REQUIRE(t.secondPassed(), "five minutes did not set secondPassed");
		TEST_REQUIRE(t.fiveMinutesPassed(), "five minutes did not set fiveMinutesPassed");
		TEST_REQUIRE(!t.hourPassed(), "five minutes set hourPassed");
	}
	{
		GameTime t;
		t.addTicks(TICKS_PER_HOUR);
		TEST_REQUIRE(t.hourPassed(), "one hour did not set hourPassed");
		TEST_REQUIRE(!t.dayPassed(), "one hour set dayPassed");
	}
	{
		GameTime t;
		t.addTicks(TICKS_PER_DAY);
		TEST_REQUIRE(t.dayPassed(), "one day did not set dayPassed");
		TEST_REQUIRE(!t.weekPassed(), "one day set weekPassed");
	}
	{
		// weekPassedFlag is ticks/TICKS_PER_DAY % 7 == 6, not getWeek().
		GameTime t;
		t.addTicks(6 * TICKS_PER_DAY);
		TEST_REQUIRE(t.dayPassed(), "six days did not set dayPassed");
		TEST_REQUIRE(t.weekPassed(), "six days did not set weekPassed");
	}
	return true;
}

static bool test_get_ticks_between_as_implemented()
{
	// Current behavior: returns 0 unless fromHours<=toHours, fromMinutes<=toMinutes,
	// and fromSeconds<toSeconds. A 10:00:00 -> 11:00:00 span with equal seconds is 0.
	GameTime t;
	TEST_REQUIRE(t.getTicksBetween(1, 10, 0, 0, 1, 11, 0, 0) == 0,
	             "equal seconds should return 0 as currently implemented");
	const unsigned expected = TICKS_PER_HOUR + 5 * TICKS_PER_SECOND;
	TEST_REQUIRE(t.getTicksBetween(1, 10, 0, 0, 1, 11, 0, 5) == expected,
	             "10:00:00 to 11:00:05 should be {0}, got {1}", expected,
	             t.getTicksBetween(1, 10, 0, 0, 1, 11, 0, 5));
	TEST_REQUIRE(t.getTicksBetween(2, 0, 0, 0, 1, 0, 0, 1) == 0,
	             "toDays < fromDays should return 0");
	return true;
}

static bool test_hardcoded_fuel_ticks_match_tps()
{
	TEST_REQUIRE(FUEL_TICKS_PER_SECOND == 144, "FUEL_TICKS_PER_SECOND is {0}",
	             FUEL_TICKS_PER_SECOND);
	TEST_REQUIRE(FUEL_TICKS_PER_SECOND == static_cast<int>(TICKS_PER_SECOND),
	             "FUEL_TICKS_PER_SECOND {0} != TICKS_PER_SECOND {1}", FUEL_TICKS_PER_SECOND,
	             TICKS_PER_SECOND);
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
	    {"tick_constants", test_tick_constants},
	    {"midday", test_midday},
	    {"add_ticks_flags", test_add_ticks_flags},
	    {"get_ticks_between", test_get_ticks_between_as_implemented},
	    {"hardcoded_fuel_ticks_match_tps", test_hardcoded_fuel_ticks_match_tps},
	});
}
