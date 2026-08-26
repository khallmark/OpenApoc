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
	// These lock the selected 36-TPS interpretation to the recovered invasion
	// coefficients. Their ratios corroborate, but do not prove, the absolute cadence.
	TEST_REQUIRE(VANILLA_TICKS_PER_MINUTE == 0x870, "VANILLA_TICKS_PER_MINUTE is {0}",
	             VANILLA_TICKS_PER_MINUTE);
	TEST_REQUIRE(VANILLA_TICKS_PER_HOUR == VANILLA_TICKS_PER_MINUTE * 60,
	             "VANILLA_TICKS_PER_HOUR is {0}", VANILLA_TICKS_PER_HOUR);
	TEST_REQUIRE(VANILLA_TICKS_PER_DAY == 0x2F7600, "VANILLA_TICKS_PER_DAY is {0}",
	             VANILLA_TICKS_PER_DAY);
	// These are deliberate canaries: changing the configured resolution requires
	// updating both expected values in the same reviewable change.
	TEST_REQUIRE(TICKS_MULTIPLIER == 4, "TICKS_MULTIPLIER is {0}", TICKS_MULTIPLIER);
	TEST_REQUIRE(TICKS_PER_SECOND == 144, "TICKS_PER_SECOND is {0}", TICKS_PER_SECOND);
	TEST_REQUIRE(TICKS_PER_MINUTE == TICKS_PER_SECOND * 60, "TICKS_PER_MINUTE is {0}",
	             TICKS_PER_MINUTE);
	TEST_REQUIRE(TICKS_PER_HOUR == TICKS_PER_MINUTE * 60, "TICKS_PER_HOUR is {0}", TICKS_PER_HOUR);
	TEST_REQUIRE(TICKS_PER_DAY == TICKS_PER_HOUR * 24, "TICKS_PER_DAY is {0}", TICKS_PER_DAY);
	TEST_REQUIRE(TICKS_PER_SECOND == vanillaTicks(VANILLA_TICKS_PER_SECOND),
	             "engine second does not scale from vanilla ticks");
	TEST_REQUIRE(TICKS_PER_MINUTE == vanillaTicks(VANILLA_TICKS_PER_MINUTE),
	             "engine minute does not scale from vanilla ticks");
	TEST_REQUIRE(TICKS_PER_HOUR == vanillaTicks(VANILLA_TICKS_PER_HOUR),
	             "engine hour does not scale from vanilla ticks");
	TEST_REQUIRE(TICKS_PER_DAY == vanillaTicks(VANILLA_TICKS_PER_DAY),
	             "engine day does not scale from vanilla ticks");
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

static bool test_fuel_ticks_scale_with_multiplier()
{
	TEST_REQUIRE(FUEL_TICKS_PER_SECOND == static_cast<int>(TICKS_PER_SECOND),
	             "FUEL_TICKS_PER_SECOND {0} != TICKS_PER_SECOND {1}", FUEL_TICKS_PER_SECOND,
	             TICKS_PER_SECOND);
	TEST_REQUIRE(FUEL_TICKS_PER_UNIT == 40000,
	             "multiplier-4 strict fuel threshold changed from 40000 to {0}",
	             FUEL_TICKS_PER_UNIT);
	TEST_REQUIRE(
	    FUEL_TICKS_PER_UNIT * static_cast<int>(FUEL_TICKS_PER_UNIT_CALIBRATED_MULTIPLIER) ==
	        FUEL_TICKS_PER_UNIT_AT_CALIBRATED_MULTIPLIER * static_cast<int>(TICKS_MULTIPLIER),
	    "FUEL_TICKS_PER_UNIT {0} did not scale with multiplier {1}", FUEL_TICKS_PER_UNIT,
	    TICKS_MULTIPLIER);
	return true;
}

static bool test_hand_weapon_fire_priority_base()
{
	TEST_REQUIRE(HAND_WEAPON_FIRE_PRIORITY_BASE == 40 * VANILLA_TICKS_PER_SECOND,
	             "HAND_WEAPON_FIRE_PRIORITY_BASE is {0}", HAND_WEAPON_FIRE_PRIORITY_BASE);
	TEST_REQUIRE(HAND_WEAPON_FIRE_PRIORITY_BASE == 1440,
	             "HAND_WEAPON_FIRE_PRIORITY_BASE changed from the extracted 1440");
	return true;
}

static bool test_invasion_delay_ticks()
{
	// FUN_0006d384 / FUN_000ad148: 24h + [0..2820] min + [0..3600] sec → 24h..72h.
	TEST_REQUIRE(INVASION_DELAY_MINUTE_MAX == 2820, "minute max is {0}", INVASION_DELAY_MINUTE_MAX);
	TEST_REQUIRE(INVASION_DELAY_SECOND_MAX == 3600, "second max is {0}", INVASION_DELAY_SECOND_MAX);
	TEST_REQUIRE(vanillaInvasionDelayTicks(0, 0) == TICKS_PER_DAY,
	             "zero rolls must be 24h, got {0}", vanillaInvasionDelayTicks(0, 0));
	TEST_REQUIRE(vanillaInvasionDelayTicks(INVASION_DELAY_MINUTE_MAX, INVASION_DELAY_SECOND_MAX) ==
	                 3ull * TICKS_PER_DAY,
	             "max rolls must be 72h, got {0}",
	             vanillaInvasionDelayTicks(INVASION_DELAY_MINUTE_MAX, INVASION_DELAY_SECOND_MAX));
	const uint64_t vanillaMax =
	    static_cast<uint64_t>(VANILLA_INVASION_DELAY_BASE) +
	    static_cast<uint64_t>(INVASION_DELAY_MINUTE_MAX) * VANILLA_INVASION_DELAY_MINUTE +
	    static_cast<uint64_t>(INVASION_DELAY_SECOND_MAX) * VANILLA_INVASION_DELAY_SECOND;
	TEST_REQUIRE(vanillaInvasionDelayTicks(INVASION_DELAY_MINUTE_MAX, INVASION_DELAY_SECOND_MAX) ==
	                 vanillaMax * TICKS_MULTIPLIER,
	             "OpenApoc ticks must be 4× the UFO2P immediates");
	TEST_REQUIRE(vanillaInvasionDelayTicks(INVASION_DELAY_MINUTE_MAX, INVASION_DELAY_SECOND_MAX) !=
	                 4ull * TICKS_PER_DAY,
	             "old 24h+U(0,72h) max of 96h must not be the EXE delay");
	return true;
}

static bool test_vanilla_city_speed1_ticks()
{
	bool skip = false;
	TEST_REQUIRE(vanillaCitySpeed1Ticks(1, skip) == 0, "first Speed1 frame is skipped");
	TEST_REQUIRE(skip, "skip flag true after first frame");
	TEST_REQUIRE(vanillaCitySpeed1Ticks(1, skip) == 1, "second Speed1 frame advances");
	TEST_REQUIRE(!skip, "skip flag false after second frame");
	TEST_REQUIRE(vanillaCitySpeed1Ticks(1, skip) == 0, "third Speed1 frame is skipped");
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
	    {"fuel_ticks_scale_with_multiplier", test_fuel_ticks_scale_with_multiplier},
	    {"hand_weapon_fire_priority_base", test_hand_weapon_fire_priority_base},
	    {"vanilla_city_speed1_ticks", test_vanilla_city_speed1_ticks},
	    {"invasion_delay_ticks", test_invasion_delay_ticks},
	});
}
