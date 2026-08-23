#include "framework/configfile.h"
#include "library/line.h"
#include "tests/test_helpers.h"
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

static bool test_diagonal_visits_voxels()
{
	auto nonCons = collectLine<false>({0, 0, 0}, {2, 2, 0});
	auto cons = collectLine<true>({0, 0, 0}, {2, 2, 0});
	TEST_REQUIRE(!nonCons.empty(), "non-conservative diagonal empty");
	TEST_REQUIRE(!cons.empty(), "conservative diagonal empty");
	TEST_REQUIRE(nonCons.front() == Vec3<int>(0, 0, 0), "non-conservative start");
	TEST_REQUIRE(cons.front() == Vec3<int>(0, 0, 0), "conservative start");
	TEST_REQUIRE(cons.size() >= nonCons.size(),
	             "conservative diagonal should visit at least as many voxels ({0} vs {1})",
	             (unsigned)cons.size(), (unsigned)nonCons.size());
	bool sawNonConsEnd = false;
	for (auto &p : nonCons)
	{
		if (p == Vec3<int>(2, 2, 0))
		{
			sawNonConsEnd = true;
		}
	}
	TEST_REQUIRE(sawNonConsEnd, "non-conservative diagonal never reached (2,2,0)");
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
