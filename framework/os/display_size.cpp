#include "framework/os/display_size.h"
#include "framework/configfile.h"
#include "framework/logger.h"
#include "framework/options.h"
#include "framework/os/app_paths.h"
#include "library/strings.h"
#include <algorithm>
#include <cstring>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

namespace OpenApoc
{

Vec2<int> resolveWindowSize(int requestedWidth, int requestedHeight, int desktopWidth,
                            int desktopHeight)
{
	const int deskW = std::max(kMinScreenWidth, desktopWidth);
	const int deskH = std::max(kMinScreenHeight, desktopHeight);
	int w = requestedWidth;
	int h = requestedHeight;
	if (w <= 0)
	{
		w = deskW;
	}
	if (h <= 0)
	{
		h = deskH;
	}
	w = std::max(kMinScreenWidth, w);
	h = std::max(kMinScreenHeight, h);
	return {w, h};
}

Vec2<int> computeDisplaySize(Vec2<int> logicalSize, int scaleXPercent, int scaleYPercent,
                             bool autoScale)
{
	Vec2<int> logical{std::max(1, logicalSize.x), std::max(1, logicalSize.y)};
	if (!autoScale && scaleXPercent == 100 && scaleYPercent == 100)
	{
		return logical;
	}

	float scaleX = (float)scaleXPercent / 100.0f;
	float scaleY = (float)scaleYPercent / 100.0f;
	if (autoScale)
	{
		scaleX = scaleY = (float)kDefaultScreenWidth / (float)logical.x;
	}

	Vec2<int> display{(int)((float)logical.x * scaleX), (int)((float)logical.y * scaleY)};
	// Raising each axis to its minimum independently changes the aspect ratio, and the result
	// is then stretched across the whole drawable by the scale-surface blit. A 16:9 window at
	// 50% computes 640x360, and clamping only y to 480 would render 4:3 content squashed
	// across a 16:9 screen. Lift both axes by the same factor so the shape the scale asked for
	// survives -- deliberate anisotropy from ScaleX != ScaleY is preserved, accidental
	// anisotropy from the clamp is not.
	if (display.x < kMinScreenWidth || display.y < kMinScreenHeight)
	{
		const float lift = std::max((float)kMinScreenWidth / (float)std::max(1, display.x),
		                            (float)kMinScreenHeight / (float)std::max(1, display.y));
		display.x = (int)((float)display.x * lift);
		display.y = (int)((float)display.y * lift);
	}
	// The rounding above can leave an axis a pixel short of the minimum.
	display.x = std::max(kMinScreenWidth, display.x);
	display.y = std::max(kMinScreenHeight, display.y);
	return display;
}

int computeUiScale(int logicalWidth, int requestedScale, bool autoScale)
{
	if (requestedScale > 0)
	{
		return std::clamp(requestedScale, kMinUiScale, kMaxUiScale);
	}
	if (autoScale)
	{
		return kMinUiScale;
	}
	const int width = std::max(1, logicalWidth);
	return std::clamp(width / kDefaultScreenWidth, kMinUiScale, kMaxUiScale);
}

Vec2<int> uiLogicalSize(Vec2<int> displaySize, int uiScale)
{
	const int s = std::max(kMinUiScale, uiScale);
	return {std::max(1, displaySize.x / s), std::max(1, displaySize.y / s)};
}

Vec2<int> uiToDisplay(Vec2<int> uiPoint, int uiScale)
{
	const int s = std::max(kMinUiScale, uiScale);
	return {uiPoint.x * s, uiPoint.y * s};
}

Vec2<int> displayToUi(Vec2<int> displayPoint, int uiScale)
{
	const int s = std::max(kMinUiScale, uiScale);
	return {displayPoint.x / s, displayPoint.y / s};
}

bool prefersMetal(const UString &rendererList)
{
	for (const auto &name : split(rendererList, ":"))
	{
		if (name == "Metal")
		{
			return true;
		}
		if (name == "GLES_3_0" || name == "GL_2_0")
		{
			return false;
		}
	}
	return false;
}

bool isFactoryWindowedDefault(int width, int height, const char *mode, bool autoScale)
{
	if (autoScale)
	{
		return false;
	}
	if (width != kDefaultScreenWidth || height != kDefaultScreenHeight)
	{
		return false;
	}
	return mode && std::strcmp(mode, "windowed") == 0;
}

void applyAppBundleDisplayDefaults(const UString &programPath)
{
	if (isPortableMode())
	{
		return;
	}

#ifdef __APPLE__
#if TARGET_OS_IPHONE
	(void)programPath;
	// Each default applies only where the user has not spoken. These used to be set
	// unconditionally, which silently discarded every one of them from settings.conf and from
	// the command line -- UI scale on an iPad could not be changed at all. The desktop branch
	// below bails out entirely when any screen option is overridden; that is not right here,
	// because an iOS app has no windowed mode to fall back to, so setting one option must not
	// cost the others their defaults.
	if (!config().optionOverridden("Framework.Screen.Width"))
	{
		Options::screenWidthOption.set(0);
	}
	if (!config().optionOverridden("Framework.Screen.Height"))
	{
		Options::screenHeightOption.set(0);
	}
	if (!config().optionOverridden("Framework.Screen.Mode"))
	{
		Options::screenModeOption.set("borderless");
	}
	if (!config().optionOverridden("Framework.Screen.AutoScale"))
	{
		Options::screenAutoScale.set(false);
	}
	// The auto rule only reaches 2x above 2560 points, which no iPad is, so a
	// 640x480 UI would sit in a small box in the middle of a 13-inch screen with
	// targets far under the ~44pt a finger needs. AutoScale must stay false:
	// computeUiScale pins the scale back to 1 whenever it is set.
	if (!config().optionOverridden("Framework.Screen.UiScale"))
	{
		Options::screenUiScaleOption.set(2);
	}
	return;
#endif
#endif

	if (!runningFromAppBundle(programPath))
	{
		return;
	}
	if (config().optionOverridden("Framework.Screen.Width") ||
	    config().optionOverridden("Framework.Screen.Height") ||
	    config().optionOverridden("Framework.Screen.Mode") ||
	    config().optionOverridden("Framework.Screen.AutoScale") ||
	    config().optionOverridden("Framework.Screen.ScaleX") ||
	    config().optionOverridden("Framework.Screen.ScaleY") ||
	    config().optionOverridden("Framework.Screen.UiScale"))
	{
		return;
	}
	if (!isFactoryWindowedDefault(Options::screenWidthOption.get(),
	                             Options::screenHeightOption.get(),
	                             Options::screenModeOption.get().c_str(),
	                             Options::screenAutoScale.get()))
	{
		return;
	}
	LogInfo("App bundle: native borderless display with integer UI scale");
	Options::screenWidthOption.set(0);
	Options::screenHeightOption.set(0);
	Options::screenModeOption.set("borderless");
	Options::screenAutoScale.set(false);
	Options::screenUiScaleOption.set(0);
}

} // namespace OpenApoc
