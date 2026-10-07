#include "framework/configfile.h"
#include "framework/logger.h"
#include "game/state/city/base.h"

using namespace OpenApoc;

namespace
{
struct Case
{
	int inclusiveRoll;
	int movedAlienCount;
	bool expected;
	const char *description;
};
} // namespace

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	// Base::alienExposureRollSucceeds is the UFO2P alien-movement exposure roll: a base becomes
	// knownToAliens when a 0..100 inclusive roll lands below (5 * number of aliens moved in).
	// Every case below is a deterministic call against the pure function - no RNG involved - so
	// each pins one specific mutant of the guard/comparison/multiplier rather than relying on
	// statistics.
	const Case cases[] = {
	    // No aliens moved at all: never exposed, regardless of how favourable the roll is.
	    {0, 0, false, "zero moved aliens never exposes, even on the lowest possible roll"},
	    {-1, 0, false, "zero moved aliens guard holds even against a roll below the threshold"},

	    // A negative moved count must be guarded out entirely (movedAlienCount > 0), even though
	    // the raw comparison alone would otherwise succeed.
	    {-100, -1, false, "negative moved-alien count is guarded out despite roll < threshold"},

	    // movedAlienCount == 1 -> threshold is exactly 5. Pin the "<" boundary precisely.
	    {0, 1, true, "roll 0 beats threshold 5 for a single moved alien"},
	    {4, 1, true, "roll 4 beats threshold 5 for a single moved alien"},
	    {5, 1, false, "roll 5 does not beat threshold 5 for a single moved alien (< not <=)"},
	    {6, 1, false, "roll 6 exceeds threshold 5 for a single moved alien"},

	    // movedAlienCount == 2 -> threshold 10; confirms the multiplier is exactly 5, not 4 or 6.
	    {9, 2, true, "roll 9 beats threshold 10 for two moved aliens"},
	    {10, 2, false, "roll 10 does not beat threshold 10 for two moved aliens"},

	    // movedAlienCount == 20 -> threshold 100, the top of the roll's inclusive range.
	    {99, 20, true, "roll 99 beats threshold 100 for twenty moved aliens"},
	    {100, 20, false, "roll 100 does not beat threshold 100 for twenty moved aliens"},
	    {101, 20, false, "roll 101 exceeds threshold 100 for twenty moved aliens"},
	};

	bool failed = false;
	for (auto &c : cases)
	{
		bool actual = Base::alienExposureRollSucceeds(c.inclusiveRoll, c.movedAlienCount);
		if (actual != c.expected)
		{
			LogError("alienExposureRollSucceeds({0}, {1}) returned {2}, expected {3}: {4}",
			         c.inclusiveRoll, c.movedAlienCount, actual ? "true" : "false",
			         c.expected ? "true" : "false", c.description);
			failed = true;
		}
	}

	if (failed)
	{
		return EXIT_FAILURE;
	}

	LogWarning("All Base::alienExposureRollSucceeds cases passed");
	return EXIT_SUCCESS;
}
