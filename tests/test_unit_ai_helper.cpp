#include "framework/configfile.h"
#include "framework/logger.h"
#include "game/state/battle/ai/unitaihelper.h"
#include "library/vec.h"
#include <cstdlib>
#include <iterator>
#include <vector>

using namespace OpenApoc;

// OPE-20: locks UnitAIHelper::exposureScore, the per-candidate threat-exposure metric that
// getTakeCoverMovement() uses to pick the least-exposed destination from its candidate menu (see
// game/state/battle/ai/unitaihelper.cpp and its header comment). The original does NOT sweep
// neighbouring tiles for wall/solidity - it scores a short, fixed menu of named candidate
// destinations by counting qualifying hostiles inside a clamped 21x21x13 box (TACP FUN_0007e600,
// VA 0x7E600 -- OpenApoc-og-research lab, "B1 cover-metric pass 2" findings section 4; not yet
// mirrored into this repo's docs tree), and getTakeCoverMovement() max-selects the highest (least
// negative) score. That box size is RECOVERED EXACTLY; the per-threat weighting by a
// range/facing-indexed table is NOT recovered (magnitude untraced) and this engine deliberately
// implements a plain per-threat count instead - only the ORDERING the metric produces (fewer
// visible hostiles is safer) is claimed as ported behaviour, and that is exactly what this file
// checks.
//
// exposureScore() is pure and static - no GameState, no battle, no tile map - so every case below
// is deterministic with no RNG involved at all.
namespace
{
struct ExposureCase
{
	const char *description;
	std::vector<Vec3<float>> threats;
	Vec3<float> candidate;
	int expected;
};

const ExposureCase CASES[] = {
    {"no threats at all scores exactly 0", {}, {10.0f, 10.0f, 1.0f}, 0},
    {"a threat exactly on the candidate costs one unit of exposure",
     {{10.0f, 10.0f, 1.0f}},
     {10.0f, 10.0f, 1.0f},
     -1},
    // Box is 21 wide in X/Y (half-extent 10.5).
    {"10 tiles away in Y is inside the half-width of a 21-wide box",
     {{10.0f, 10.0f, 1.0f}},
     {10.0f, 20.0f, 1.0f},
     -1},
    {"11 tiles away in Y is outside a 21-wide box",
     {{10.0f, 10.0f, 1.0f}},
     {10.0f, 21.0f, 1.0f},
     0},
    {"10 tiles away in X is inside the half-width of a 21-wide box",
     {{10.0f, 10.0f, 1.0f}},
     {20.0f, 10.0f, 1.0f},
     -1},
    {"11 tiles away in X is outside a 21-wide box",
     {{10.0f, 10.0f, 1.0f}},
     {21.0f, 10.0f, 1.0f},
     0},
    // Box is 13 deep in Z (half-extent 6.5) - a different half-extent from XY, checked
    // separately so a fixture that only exercised X/Y could not pass by accident.
    {"6 levels up in Z is inside the half-depth of a 13-deep box",
     {{10.0f, 10.0f, 1.0f}},
     {10.0f, 10.0f, 7.0f},
     -1},
    {"7 levels up in Z is outside a 13-deep box",
     {{10.0f, 10.0f, 1.0f}},
     {10.0f, 10.0f, 8.0f},
     0},
    // Three qualifying threats in range must cost three, not saturate at one and not double-count.
    {"three threats in range each cost one unit of exposure",
     {{10.0f, 10.0f, 1.0f}, {11.0f, 10.0f, 1.0f}, {12.0f, 10.0f, 1.0f}},
     {10.0f, 10.0f, 1.0f},
     -3},
    // A mix of in-range and out-of-range threats: only the in-range ones count.
    {"only the in-range threats among a mixed set count",
     {{10.0f, 10.0f, 1.0f}, {10.0f, 200.0f, 1.0f}, {11.0f, 10.0f, 1.0f}},
     {10.0f, 10.0f, 1.0f},
     -2},
};
} // namespace

static bool checkOrderingIsSaferHigher()
{
	// The caller (getTakeCoverMovement) does `if (score > bestScore) best = candidate;` - a
	// max-select. So a candidate with fewer/no visible threats MUST score higher (less negative)
	// than one surrounded by threats, or the AI would walk toward exposure instead of away from
	// it.
	const std::vector<Vec3<float>> threats{
	    {10.0f, 10.0f, 1.0f}, {11.0f, 10.0f, 1.0f}, {12.0f, 10.0f, 1.0f}};
	const int exposed = UnitAIHelper::exposureScore(threats, {10.0f, 10.0f, 1.0f});
	const int sheltered = UnitAIHelper::exposureScore(threats, {10.0f, 200.0f, 1.0f});
	if (!(sheltered > exposed))
	{
		LogError("ordering_is_safer_higher: sheltered score {0} must be greater than exposed "
		         "score {1} (caller max-selects)",
		         sheltered, exposed);
		return false;
	}
	return true;
}

static bool checkCustomBoxOverride()
{
	// getTakeCoverMovement always calls exposureScore() with the recovered 21x21x13 default, but
	// the box size is a parameter (not a hardcoded literal inside the loop) precisely so this
	// case can be locked without duplicating the box math: a threat that is out of range of a
	// smaller box must stop counting once the box shrinks below its distance.
	const std::vector<Vec3<float>> threats{{5.0f, 0.0f, 0.0f}};
	const int wideBox = UnitAIHelper::exposureScore(threats, {0.0f, 0.0f, 0.0f}, /*boxXY=*/21,
	                                                /*boxZ=*/13);
	const int narrowBox = UnitAIHelper::exposureScore(threats, {0.0f, 0.0f, 0.0f}, /*boxXY=*/5,
	                                                   /*boxZ=*/13);
	if (wideBox != -1)
	{
		LogError("custom_box_override: expected -1 with the default box, got {0}", wideBox);
		return false;
	}
	if (narrowBox != 0)
	{
		LogError("custom_box_override: expected 0 once the box shrank past the threat, got {0}",
		         narrowBox);
		return false;
	}
	return true;
}

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	int failures = 0;
	for (const auto &c : CASES)
	{
		const int actual = UnitAIHelper::exposureScore(c.threats, c.candidate);
		if (actual != c.expected)
		{
			LogError("[{0}] exposureScore(...) returned {1}, expected {2}", c.description, actual,
			         c.expected);
			failures++;
		}
	}

	if (!checkOrderingIsSaferHigher())
	{
		failures++;
	}
	if (!checkCustomBoxOverride())
	{
		failures++;
	}

	if (failures > 0)
	{
		LogError("test_unit_ai_helper: {0} of {1} cases failed", failures,
		         static_cast<int>(std::size(CASES)) + 2);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
