#include "framework/configfile.h"
#include "framework/logger.h"
#include "game/state/city/building.h"
#include "library/rect.h"
#include "library/vec.h"
#include <vector>

using namespace OpenApoc;

// Regression lock for Building::rankNearbyIntact(), the destination-candidate
// selection used by Building::alienMovement() to pick where alien crew move to.
//
// The pre-fix alienMovement() inline loop claimed (in its own comment) to pick
// "15 intact buildings within range of 15 tiles", but the loop never checked
// intactness at all, and it stopped at the first 15 buildings satisfying the
// distance check in city->buildings iteration order rather than the 15
// *nearest* ones. Both defects are player-visible: aliens could move crew into
// a building that had already been reduced to rubble, and which building was
// picked depended on map storage order rather than proximity.

static bool expectRanked(const std::vector<Rect<int>> &bounds, const std::vector<bool> &intact,
                         Vec2<int> origin, int maxDistance, int maxCount,
                         const std::vector<int> &expected, const char *caseName)
{
	auto actual = Building::rankNearbyIntact(bounds, intact, origin, maxDistance, maxCount);
	if (actual != expected)
	{
		LogError("test_alien_building: case \"{0}\" failed", caseName);
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

	bool ok = true;

	// Case 1: a closer building that is NOT intact must be excluded even though
	// a farther intact building within range is kept. (Guards the missing
	// intactness check.)
	{
		std::vector<Rect<int>> bounds = {
		    Rect<int>{0, 0, 2, 2},  // index 0: origin building itself (distance 0)
		    Rect<int>{2, 0, 4, 2},  // index 1: distance 2, but NOT intact (rubble)
		    Rect<int>{10, 0, 12, 2} // index 2: distance 10, intact
		};
		std::vector<bool> intact = {true, false, true};
		Vec2<int> origin = Building::boundsCenter(bounds[0]);
		ok &= expectRanked(bounds, intact, origin, 15, 15, {2}, "excludes non-intact neighbour");
	}

	// Case 2: candidates are returned nearest-first, and truncated to maxCount
	// even when more candidates exist within range. (Guards the map-order /
	// first-N-collected bug: the fix must rank by distance before truncating.)
	{
		std::vector<Rect<int>> bounds = {
		    Rect<int>{0, 0, 2, 2},   // index 0: origin (distance 0, excluded)
		    Rect<int>{20, 0, 22, 2}, // index 1: distance 20 (farthest, intact)
		    Rect<int>{4, 0, 6, 2},   // index 2: distance 4 (nearest, intact)
		    Rect<int>{12, 0, 14, 2}, // index 3: distance 12 (middle, intact)
		};
		std::vector<bool> intact = {true, true, true, true};
		Vec2<int> origin = Building::boundsCenter(bounds[0]);
		// maxDistance wide enough for all three neighbours, but only the 2
		// nearest should be kept, ordered nearest-first.
		ok &= expectRanked(bounds, intact, origin, 25, 2, {2, 3},
		                   "orders nearest-first and truncates to maxCount");
	}

	// Case 3: a building exactly at maxDistance is kept; one tile beyond is
	// dropped.
	{
		std::vector<Rect<int>> bounds = {
		    Rect<int>{0, 0, 2, 2},  // index 0: origin
		    Rect<int>{15, 0, 17, 2}, // index 1: distance 15 == maxDistance, kept
		    Rect<int>{17, 0, 19, 2}, // index 2: distance 17 > maxDistance, dropped
		};
		std::vector<bool> intact = {true, true, true};
		Vec2<int> origin = Building::boundsCenter(bounds[0]);
		ok &= expectRanked(bounds, intact, origin, 15, 15, {1}, "respects maxDistance boundary");
	}

	// Case 4: the origin building itself (distance 0) is never a candidate,
	// even when intact.
	{
		std::vector<Rect<int>> bounds = {Rect<int>{0, 0, 2, 2}};
		std::vector<bool> intact = {true};
		Vec2<int> origin = Building::boundsCenter(bounds[0]);
		ok &= expectRanked(bounds, intact, origin, 15, 15, {}, "excludes distance-zero self");
	}

	// Case 5: empty input yields an empty ranking.
	{
		std::vector<Rect<int>> bounds;
		std::vector<bool> intact;
		ok &= expectRanked(bounds, intact, Vec2<int>{0, 0}, 15, 15, {}, "handles empty input");
	}

	if (!ok)
	{
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
