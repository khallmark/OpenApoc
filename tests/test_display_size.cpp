#include "framework/configfile.h"
#include "framework/os/display_size.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static bool test_resolve_window_size()
{
	TEST_REQUIRE(resolveWindowSize(0, 0, 2560, 1440) == Vec2<int>(2560, 1440), "0x0 is desktop");
	TEST_REQUIRE(resolveWindowSize(0, 720, 1920, 1080) == Vec2<int>(1920, 720),
	             "0 width uses desktop width");
	TEST_REQUIRE(resolveWindowSize(320, 240, 1920, 1080) == Vec2<int>(640, 480), "clamp min");
	TEST_REQUIRE(resolveWindowSize(3840, 2160, 1920, 1080) == Vec2<int>(3840, 2160),
	             "any larger size allowed");
	return true;
}

static bool test_compute_display_size()
{
	TEST_REQUIRE(computeDisplaySize({1920, 1080}, 100, 100, false) == Vec2<int>(1920, 1080),
	             "native when scale 100");
	const auto autoScaled = computeDisplaySize({2560, 1440}, 100, 100, true);
	TEST_REQUIRE(autoScaled.x == kDefaultScreenWidth, "autoscale width 1280");
	TEST_REQUIRE(autoScaled.y == 720, "autoscale keeps aspect {0}", autoScaled.y);
	// Retina: pass window points, not the 2x drawable. Same visible map, 4x less fill.
	TEST_REQUIRE(computeDisplaySize({1800, 1169}, 100, 100, false) == Vec2<int>(1800, 1169),
	             "window points stay logical");
	TEST_REQUIRE(computeUiScale(1800, 0, false) == 1, "1800-wide window auto 1x UI");
	TEST_REQUIRE(computeUiScale(3600, 0, false) == 2, "drawable width must not drive UiScale");
	return true;
}

static bool test_ui_scale()
{
	TEST_REQUIRE(computeUiScale(1280, 0, false) == 1, "720p auto 1x");
	TEST_REQUIRE(computeUiScale(1920, 0, false) == 1, "1080p auto 1x");
	TEST_REQUIRE(computeUiScale(2560, 0, false) == 2, "1440p auto 2x");
	TEST_REQUIRE(computeUiScale(3840, 0, false) == 3, "4k auto 3x");
	TEST_REQUIRE(computeUiScale(3840, 0, true) == 1, "autoscale forces 1x UI");
	TEST_REQUIRE(computeUiScale(3840, 2, true) == 2, "explicit UiScale wins");
	TEST_REQUIRE(computeUiScale(800, 8, false) == kMaxUiScale, "clamp max");
	TEST_REQUIRE(uiLogicalSize({3840, 2160}, 3) == Vec2<int>(1280, 720), "4k / 3");
	TEST_REQUIRE(uiToDisplay({320, 592}, 3) == Vec2<int>(960, 1776), "ui to display");
	TEST_REQUIRE(displayToUi({960, 1776}, 3) == Vec2<int>(320, 592), "display to ui");
	return true;
}

static bool test_factory_default()
{
	TEST_REQUIRE(isFactoryWindowedDefault(1280, 720, "windowed", false), "factory");
	TEST_REQUIRE(!isFactoryWindowedDefault(0, 0, "borderless", false), "native not factory");
	TEST_REQUIRE(!isFactoryWindowedDefault(1280, 720, "windowed", true), "autoscale not factory");
	return true;
}

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}
	return runTestSuite({
	    {"resolve window size", test_resolve_window_size},
	    {"compute display size", test_compute_display_size},
	    {"ui scale", test_ui_scale},
	    {"factory windowed default", test_factory_default},
	});
}
