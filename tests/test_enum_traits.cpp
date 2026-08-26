#include "framework/configfile.h"
#include "library/enum_traits.h"
#include "tests/test_helpers.h"

namespace OpenApoc
{
enum class TestFlags : unsigned
{
	None = 0,
	A = 1,
	B = 2,
	C = 4
};

template <> struct is_flag_enum<TestFlags> : std::true_type
{
};

enum class TestPartial : int
{
	Foo = 1,
	Bar = 2
};

template <> struct is_partial_enum<TestPartial> : std::true_type
{
};
} // namespace OpenApoc

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static bool test_flag_ops()
{
	auto ab = TestFlags::A | TestFlags::B;
	TEST_REQUIRE((ab & TestFlags::A) == TestFlags::A, "A|B missing A");
	TEST_REQUIRE((ab & TestFlags::B) == TestFlags::B, "A|B missing B");
	TEST_REQUIRE((ab & TestFlags::C) == TestFlags::None, "A|B has C");
	auto x = TestFlags::A;
	x |= TestFlags::C;
	TEST_REQUIRE((x & TestFlags::C) == TestFlags::C, "|= C failed");
	x &= TestFlags::A;
	TEST_REQUIRE(x == TestFlags::A, "&= A failed");
	auto y = TestFlags::A ^ TestFlags::B;
	TEST_REQUIRE((y & TestFlags::A) == TestFlags::A, "A^B missing A");
	TEST_REQUIRE((y & TestFlags::B) == TestFlags::B, "A^B missing B");
	y ^= TestFlags::A;
	TEST_REQUIRE((y & TestFlags::A) == TestFlags::None, "^= A failed");
	return true;
}

static bool test_partial_eq()
{
	TEST_REQUIRE(TestPartial::Foo == 1, "Foo == 1");
	TEST_REQUIRE(1 == TestPartial::Foo, "1 == Foo");
	TEST_REQUIRE(TestPartial::Foo != 2, "Foo != 2");
	TEST_REQUIRE(2 != TestPartial::Foo, "2 != Foo");
	TEST_REQUIRE(TestPartial::Bar == 2, "Bar == 2");
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
	    {"flag_ops", test_flag_ops},
	    {"partial_eq", test_partial_eq},
	});
}
