// Locks the relationship between the original game's tick base and OpenApoc's.
//
// The vanilla rate of 36 ticks/second is fixed by the recovered invasion-delay
// formula (see the comment block in game/state/gametime.h). TICKS_MULTIPLIER, by
// contrast, is ours to pick -- so what this test guards is not a particular
// multiplier but the invariant that every derived value still follows it.
//
// If someone changes TICKS_MULTIPLIER and something stops deriving, this fails.

#include "game/state/city/vehicle.h"
#include "game/state/gametime.h"
#include <cstdio>
#include <cstdlib>

using namespace OpenApoc;

static int failures = 0;

static void check(bool ok, const char *what)
{
	if (!ok)
	{
		printf("FAIL: %s\n", what);
		failures++;
	}
}

int main(int, char **)
{
	// The vanilla base, as the binary defines it. These three are the evidence
	// that 36 is right; they are also static_asserts, so this is belt and braces.
	check(VANILLA_TICKS_PER_SECOND == 0x24, "vanilla second is 0x24 ticks");
	check(VANILLA_TICKS_PER_MINUTE == 0x870, "vanilla minute is 0x870 ticks");
	check(VANILLA_TICKS_PER_DAY == 0x2F7600, "vanilla day is 0x2F7600 ticks");

	// A vanilla day really is a day.
	check(VANILLA_TICKS_PER_DAY == VANILLA_TICKS_PER_SECOND * 60 * 60 * 24,
	      "vanilla day is 24h of vanilla seconds");

	// The engine clock scales from the vanilla clock by exactly the multiplier,
	// whatever the multiplier happens to be.
	check(TICKS_PER_SECOND == vanillaTicks(VANILLA_TICKS_PER_SECOND), "engine second scales");
	check(TICKS_PER_MINUTE == vanillaTicks(VANILLA_TICKS_PER_MINUTE), "engine minute scales");
	check(TICKS_PER_HOUR == vanillaTicks(VANILLA_TICKS_PER_HOUR), "engine hour scales");
	check(TICKS_PER_DAY == vanillaTicks(VANILLA_TICKS_PER_DAY), "engine day scales");

	// Internal consistency of the engine clock itself.
	check(TICKS_PER_MINUTE == TICKS_PER_SECOND * 60, "minute is 60 seconds");
	check(TICKS_PER_HOUR == TICKS_PER_MINUTE * 60, "hour is 60 minutes");
	check(TICKS_PER_DAY == TICKS_PER_HOUR * 24, "day is 24 hours");
	check(TURBO_TICKS == 5 * 60 * TICKS_PER_SECOND, "turbo step is five minutes");

	// The regression this test mainly exists for: values that are conceptually
	// "one second of ticks" must derive, not be spelled out. FUEL_TICKS_PER_SECOND
	// was a literal 144 and silently desynchronised whenever the multiplier moved.
	check((unsigned)FUEL_TICKS_PER_SECOND == TICKS_PER_SECOND,
	      "FUEL_TICKS_PER_SECOND derives from TICKS_PER_SECOND");

	// Same class of defect, same fix: the fuel-unit threshold is a real-time
	// duration in engine ticks, so real seconds per fuel unit must not move when
	// the multiplier does.
	check(FUEL_TICKS_PER_UNIT * (int)FUEL_CALIBRATION_MULTIPLIER == 40000 * (int)TICKS_MULTIPLIER,
	      "FUEL_TICKS_PER_UNIT scales with the multiplier");
	{
		const double secondsPerUnit = (double)FUEL_TICKS_PER_UNIT / (double)TICKS_PER_SECOND;
		const double calibrated = 40000.0 / (36.0 * FUEL_CALIBRATION_MULTIPLIER);
		check(secondsPerUnit > calibrated - 0.01 && secondsPerUnit < calibrated + 0.01,
		      "real seconds per fuel unit is unchanged by the multiplier");
	}

	// vanillaTicks() is the one sanctioned conversion; check it is a plain scale.
	check(vanillaTicks(0) == 0, "vanillaTicks(0) is 0");
	check(vanillaTicks(1) == TICKS_MULTIPLIER, "vanillaTicks(1) is one multiplier step");
	check(vanillaTicks(VANILLA_TICKS_PER_DAY) == TICKS_PER_DAY, "a vanilla day converts to a day");

	if (failures)
	{
		printf("test_gametime: %d failure(s)\n", failures);
		return EXIT_FAILURE;
	}
	printf("test_gametime: all checks passed\n");
	return EXIT_SUCCESS;
}
