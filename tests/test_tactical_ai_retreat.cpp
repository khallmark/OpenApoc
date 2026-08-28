#include "framework/configfile.h"
#include "framework/logger.h"
#include "game/state/battle/ai/unitailowmorale.h"
#include "library/vec.h"
#include <cstdlib>
#include <iterator>

using namespace OpenApoc;

// OPE-21: locks UnitAILowMorale::isFartherFromEnemy, the distance comparison that
// PanicRun retreat-block selection uses to prefer LOS blocks that move a routed unit
// farther away from the nearest visible enemy (see game/state/battle/ai/unitailowmorale.cpp,
// ai.txt "Panic Run"). Deterministic table + distance assertions per ticket acceptance
// criteria; no RNG is exercised because the comparison itself is pure and RNG-free -
// only the candidate list it filters is chosen randomly in production code.
namespace
{
struct RetreatDistanceCase
{
	const char *description;
	Vec3<float> candidate;
	Vec3<float> current;
	Vec3<float> enemy;
	bool expectFarther;
};

const RetreatDistanceCase CASES[] = {
    // Candidate clearly farther from the enemy than the unit's current position.
    {"candidate farther along +x",
     {10.0f, 0.0f, 0.0f},
     {5.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 0.0f},
     true},
    // Candidate clearly nearer to the enemy than the unit's current position.
    {"candidate nearer along +x",
     {2.0f, 0.0f, 0.0f},
     {5.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 0.0f},
     false},
    // Boundary: candidate is equidistant (same distance, different point) - must NOT count
    // as farther. This is the case a sloppy ">=" mutation would get wrong.
    {"candidate equidistant, different point",
     {0.0f, 5.0f, 0.0f},
     {5.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 0.0f},
     false},
    // Full 3D displacement, non-origin enemy, negative coordinates.
    {"3d farther with negative coords",
     {-8.0f, 6.0f, 4.0f},
     {-1.0f, 1.0f, 1.0f},
     {2.0f, -3.0f, 0.0f},
     true},
    {"3d nearer with negative coords",
     {1.0f, -2.0f, 0.5f},
     {-8.0f, 6.0f, 4.0f},
     {2.0f, -3.0f, 0.0f},
     false},
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
		bool actual = UnitAILowMorale::isFartherFromEnemy(c.candidate, c.current, c.enemy);
		if (actual != c.expectFarther)
		{
			LogError("[{0}] isFartherFromEnemy(candidate={{{1},{2},{3}}}, "
			         "current={{{4},{5},{6}}}, enemy={{{7},{8},{9}}}) returned {10}, expected "
			         "{11}",
			         c.description, c.candidate.x, c.candidate.y, c.candidate.z, c.current.x,
			         c.current.y, c.current.z, c.enemy.x, c.enemy.y, c.enemy.z, actual,
			         c.expectFarther);
			failures++;
		}
	}

	if (failures > 0)
	{
		LogError("test_tactical_ai_retreat: {0} of {1} cases failed", failures,
		         static_cast<int>(std::size(CASES)));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
