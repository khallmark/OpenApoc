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
	Options::screenWidthOption.set(0);
	Options::screenHeightOption.set(0);
	Options::screenModeOption.set("borderless");
	Options::screenAutoScale.set(false);
	Options::screenUiScaleOption.set(0);
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
	if (!isFactoryWindowedDefault(
	        Options::screenWidthOption.get(), Options::screenHeightOption.get(),
	        Options::screenModeOption.get().c_str(), Options::screenAutoScale.get()))
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
