#include "framework/configfile.h"
#include "library/line.h"
#include "tests/test_helpers.h"
#include <algorithm>
#include <vector>

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

template <bool conservative>
static std::vector<Vec3<int>> collectLine(Vec3<int> start, Vec3<int> end)
{
	LineSegment<int, conservative> line{start, end};
	std::vector<Vec3<int>> points;
	unsigned guard = 0;
	for (auto p : line)
	{
		points.push_back(p);
		if (++guard > 64)
		{
			break;
		}
	}
	return points;
}

static bool test_axis_aligned()
{
	auto points = collectLine<false>({0, 0, 0}, {3, 0, 0});
	TEST_REQUIRE(!points.empty(), "axis-aligned line produced no points");
	TEST_REQUIRE(points.front() == Vec3<int>(0, 0, 0), "first point {0}", points.front());
	bool sawEnd = false;
	for (auto &p : points)
	{
		TEST_CHECK(p.y == 0 && p.z == 0, "axis-aligned left the x axis at {0}", p);
		if (p.x == 3)
		{
			sawEnd = true;
		}
	}
	TEST_REQUIRE(sawEnd, "axis-aligned line never reached x=3");
	return true;
}

static bool inBounds(const Vec3<int> &p, const Vec3<int> &a, const Vec3<int> &b)
{
	const int minX = std::min(a.x, b.x);
	const int maxX = std::max(a.x, b.x);
	const int minY = std::min(a.y, b.y);
	const int maxY = std::max(a.y, b.y);
	const int minZ = std::min(a.z, b.z);
	const int maxZ = std::max(a.z, b.z);
	return p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY && p.z >= minZ && p.z <= maxZ;
}

static bool test_diagonal_visits_voxels()
{
	const Vec3<int> start{0, 0, 0};
	const Vec3<int> end{2, 2, 0};
	auto nonCons = collectLine<false>(start, end);
	auto cons = collectLine<true>(start, end);
	TEST_REQUIRE(nonCons.size() >= 2, "non-conservative diagonal visited {0} voxels",
	             (unsigned)nonCons.size());
	TEST_REQUIRE(cons.size() >= 2, "conservative diagonal visited {0} voxels",
	             (unsigned)cons.size());
	TEST_REQUIRE(nonCons.front() == start, "non-conservative start");
	TEST_REQUIRE(cons.front() == start, "conservative start");
	TEST_REQUIRE(cons.size() >= nonCons.size(),
	             "conservative diagonal should visit at least as many voxels ({0} vs {1})",
	             (unsigned)cons.size(), (unsigned)nonCons.size());
	for (auto &p : nonCons)
	{
		TEST_CHECK(inBounds(p, start, end), "non-conservative left the segment box at {0}", p);
	}
	for (auto &p : cons)
	{
		TEST_CHECK(inBounds(p, start, end), "conservative left the segment box at {0}", p);
	}
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
	    {"axis_aligned", test_axis_aligned},
	    {"diagonal_visits_voxels", test_diagonal_visits_voxels},
	});
}
