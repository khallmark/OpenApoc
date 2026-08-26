#include "framework/configfile.h"
#include "library/vec.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static bool test_clamp()
{
	TEST_REQUIRE(clamp(5, 0, 10) == 5, "clamp mid {0}", clamp(5, 0, 10));
	TEST_REQUIRE(clamp(-1, 0, 10) == 0, "clamp below {0}", clamp(-1, 0, 10));
	TEST_REQUIRE(clamp(11, 0, 10) == 10, "clamp above {0}", clamp(11, 0, 10));
	TEST_REQUIRE(clamp(0, 0, 10) == 0, "clamp min edge");
	TEST_REQUIRE(clamp(10, 0, 10) == 10, "clamp max edge");
	TEST_REQUIRE(clamp(1.5f, 0.0f, 1.0f) == 1.0f, "clamp float above");
	return true;
}

static bool test_mix()
{
	TEST_REQUIRE(mix(0.0f, 10.0f, 0.0f) == 0.0f, "mix 0");
	TEST_REQUIRE(mix(0.0f, 10.0f, 1.0f) == 10.0f, "mix 1");
	TEST_REQUIRE(mix(0.0f, 10.0f, 0.5f) == 5.0f, "mix 0.5");
	return true;
}

static bool test_vec2_less()
{
	Vec2<int> a{1, 2};
	Vec2<int> b{1, 3};
	Vec2<int> c{2, 0};
	TEST_REQUIRE(a < b, "{1,2} should be < {1,3}");
	TEST_REQUIRE(!(b < a), "{1,3} should not be < {1,2}");
	TEST_REQUIRE(a < c, "{1,2} should be < {2,0}");
	TEST_REQUIRE(!(c < a), "{2,0} should not be < {1,2}");
	TEST_REQUIRE(!(a < a), "vec2 should not be < itself");
	return true;
}

static bool test_vec3_less()
{
	Vec3<int> a{1, 2, 3};
	Vec3<int> b{1, 2, 4};
	Vec3<int> c{1, 3, 0};
	Vec3<int> d{2, 0, 0};
	TEST_REQUIRE(a < b, "z-order");
	TEST_REQUIRE(a < c, "y-order");
	TEST_REQUIRE(a < d, "x-order");
	TEST_REQUIRE(!(b < a), "reverse z");
	TEST_REQUIRE(!(a < a), "vec3 should not be < itself");
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
	    {"clamp", test_clamp},
	    {"mix", test_mix},
	    {"vec2_less", test_vec2_less},
	    {"vec3_less", test_vec3_less},
	});
}
