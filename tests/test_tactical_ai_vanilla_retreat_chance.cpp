#include "framework/configfile.h"
#include "framework/logger.h"
#include "game/state/battle/ai/tacticalaivanilla.h"
#include <cstdlib>
#include <iterator>

using namespace OpenApoc;

// OPE-21 (chance-selection half; the movement-away half is covered by open PR 35's
// UnitAILowMorale::isFartherFromEnemy): locks TacticalAIVanilla::retreatChancePercent, the
// organisation-wide dice-roll threshold that TacticalAIVanilla::think() uses to decide whether
// to order idle units to retreat this turn (see game/state/battle/ai/tacticalaivanilla.cpp).
//
// The function is a pure, RNG-free static helper of two ints - it is only the *threshold*
// used against a later randBoundsExclusive() roll at the think() call site, so no RNG needs to
// be exercised or seeded here. No gamestate is constructed or read; this binary takes no
// positional arguments.
//
// Parity note: this reproduces develop's fix verbatim. The upstream (pre-fix) comment already
// documented the intended curve as "[0 to 50]% as number of neutralised allies goes [50 to
// 100]%" - that comment is documentary evidence from upstream source, not binary/extractor
// evidence, so the exact 50/100 breakpoints below are ANCHORED TO THAT COMMENT, not verified
// against the original game binary; treat them as inference from source, not confirmed parity.
//
// The pre-fix formula `(unitsActive - unitsTotal / 2) / unitsTotal` was NOT "always zero": at
// unitsTotal=1, unitsActive=1 it evaluated to 1 (a spurious 1% chance with zero losses). It was,
// however, zero for every unitsTotal > 1 tested here, which is the defect this test locks.
namespace
{
struct RetreatChanceCase
{
	const char *description;
	int unitsTotal;
	int unitsActive;
	int expectedPercent;
};

const RetreatChanceCase CASES[] = {
    // unitsTotal <= 0 guard: think() LogAsserts unitsTotal > 0, but the helper itself must not
    // divide by zero if ever called off that path.
    {"non-positive unitsTotal returns 0", 0, 0, 0},
    // No losses (0% neutralised) -> 0% retreat chance.
    {"no losses, 10 total", 10, 10, 0},
    // Exactly 50% neutralised -> clamp boundary, 0% retreat chance.
    {"50% neutralised clamp boundary", 10, 5, 0},
    // Just past the boundary (60% neutralised) -> first nonzero step, 10%.
    {"60% neutralised, first nonzero step", 10, 4, 10},
    // Fully neutralised (100%) -> the maximum of the documented range, 50%.
    {"fully neutralised, 10 total", 10, 0, 50},
    // Single-unit force, no losses: must be 0, unlike the old buggy formula (which gave 1).
    {"single unit, no losses", 1, 1, 0},
    // Single-unit force, neutralised: 100% neutralised -> 50% retreat chance.
    {"single unit, neutralised", 1, 0, 50},
};
} // namespace

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	int failures = 0;
	for (const auto &c : CASES)
	{
		int actual = TacticalAIVanilla::retreatChancePercent(c.unitsTotal, c.unitsActive);
		if (actual != c.expectedPercent)
		{
			LogError("[{0}] retreatChancePercent(unitsTotal={1}, unitsActive={2}) returned {3}, "
			         "expected {4}",
			         c.description, c.unitsTotal, c.unitsActive, actual, c.expectedPercent);
			failures++;
		}
	}

	if (failures > 0)
	{
		LogError("test_tactical_ai_vanilla_retreat_chance: {0} of {1} cases failed", failures,
		         static_cast<int>(std::size(CASES)));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
