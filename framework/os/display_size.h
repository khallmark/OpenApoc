#pragma once

#include "library/vec.h"

namespace OpenApoc
{

inline constexpr int kMinScreenWidth = 640;
inline constexpr int kMinScreenHeight = 480;
inline constexpr int kDefaultScreenWidth = 1280;
inline constexpr int kDefaultScreenHeight = 720;

// 0 on an axis means "use that desktop dimension". Values below 640×480 clamp up.
// Any larger size is accepted (no max).
Vec2<int> resolveWindowSize(int requestedWidth, int requestedHeight, int desktopWidth,
                            int desktopHeight);

// Logical game size from the drawable framebuffer. AutoScale targets 1280-wide.
Vec2<int> computeDisplaySize(Vec2<int> drawableSize, int scaleXPercent, int scaleYPercent,
                             bool autoScale);

bool isFactoryWindowedDefault(int width, int height, const char *mode, bool autoScale);

} // namespace OpenApoc
