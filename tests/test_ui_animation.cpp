#include "framework/configfile.h"
#include "framework/uianimation.h"
#include "tests/test_helpers.h"
#include <cmath>

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static bool test_progress()
{
	TEST_REQUIRE(uiAnimationProgress(-10.0f, 150.0f) == 0.0f, "clamp before the start");
	TEST_REQUIRE(uiAnimationProgress(0.0f, 150.0f) == 0.0f, "start transparent");
	TEST_REQUIRE(uiAnimationProgress(75.0f, 150.0f) == 0.5f, "halfway opacity");
	TEST_REQUIRE(uiAnimationProgress(150.0f, 150.0f) == 1.0f, "finish at full opacity");
	TEST_REQUIRE(uiAnimationProgress(500.0f, 150.0f) == 1.0f, "stay complete after a long frame");
	TEST_REQUIRE(uiAnimationProgress(30.0f, 120.0f) == 0.15625f, "ease into hover");
	TEST_REQUIRE(uiAnimationProgress(90.0f, 120.0f) == 0.84375f, "ease out of hover");
	float previous = 0.0f;
	for (int elapsed = 0; elapsed <= 150; elapsed++)
	{
		const float progress = uiAnimationProgress(static_cast<float>(elapsed), 150.0f);
		TEST_REQUIRE(progress >= previous && progress <= 1.0f,
		             "fade must be bounded and monotonic");
		previous = progress;
	}
	return true;
}

static bool test_pulse()
{
	TEST_REQUIRE(uiAnimationPulse(0.0f, 2000.0f) == 0.0f, "lift starts dim");
	TEST_REQUIRE(std::abs(uiAnimationPulse(500.0f, 2000.0f) - 0.5f) < 0.00001f, "quarter cycle");
	TEST_REQUIRE(uiAnimationPulse(1000.0f, 2000.0f) == 1.0f, "lift peaks after one second");
	TEST_REQUIRE(uiAnimationPulse(2000.0f, 2000.0f) == 0.0f, "lift returns to dim");
	TEST_REQUIRE(uiAnimationPulse(2500.0f, 2000.0f) == uiAnimationPulse(500.0f, 2000.0f),
	             "pulse wraps independently of update counts");
	for (int elapsed = 0; elapsed < 2000; elapsed++)
	{
		const float level = uiAnimationPulse(static_cast<float>(elapsed), 2000.0f);
		TEST_REQUIRE(level >= 0.0f && level <= 1.0f, "glow must stay within palette endpoints");
	}
	TEST_REQUIRE(uiAnimationPulse(10.0f, 2000.0f) < 0.001f &&
	                 uiAnimationPulse(1990.0f, 2000.0f) < 0.001f,
	             "cosine must ease smoothly across the wrap");
	return true;
}

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}
	return runTestSuite({{"UI easing", test_progress}, {"gravlift cosine glow", test_pulse}});
}
