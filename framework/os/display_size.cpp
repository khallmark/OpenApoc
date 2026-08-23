#include "framework/os/display_size.h"
#include <algorithm>
#include <cstring>

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

Vec2<int> computeDisplaySize(Vec2<int> drawableSize, int scaleXPercent, int scaleYPercent,
                             bool autoScale)
{
	Vec2<int> drawable{std::max(1, drawableSize.x), std::max(1, drawableSize.y)};
	if (!autoScale && scaleXPercent == 100 && scaleYPercent == 100)
	{
		return drawable;
	}

	float scaleX = (float)scaleXPercent / 100.0f;
	float scaleY = (float)scaleYPercent / 100.0f;
	if (autoScale)
	{
		scaleX = scaleY = (float)kDefaultScreenWidth / (float)drawable.x;
	}

	Vec2<int> display{(int)((float)drawable.x * scaleX), (int)((float)drawable.y * scaleY)};
	display.x = std::max(kMinScreenWidth, display.x);
	display.y = std::max(kMinScreenHeight, display.y);
	return display;
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

} // namespace OpenApoc
