#pragma once

#include "library/vec.h"

namespace OpenApoc
{

inline constexpr int kMinScreenWidth = 640;
inline constexpr int kMinScreenHeight = 480;
inline constexpr int kDefaultScreenWidth = 1280;
inline constexpr int kDefaultScreenHeight = 720;
inline constexpr int kMinUiScale = 1;
inline constexpr int kMaxUiScale = 4;

// 0 on an axis means "use that desktop dimension". Values below 640x480 clamp up.
// Any larger size is accepted (no max).
Vec2<int> resolveWindowSize(int requestedWidth, int requestedHeight, int desktopWidth,
                            int desktopHeight);

// Logical world size from the window in points (not the HiDPI backing store).
// AutoScale targets 1280-wide and scales UI with the world. Prefer AutoScale
// off + UiScale for a larger map. The GPU then blits this size to the drawable.
Vec2<int> computeDisplaySize(Vec2<int> logicalSize, int scaleXPercent, int scaleYPercent,
                             bool autoScale);

// requestedScale 0 = auto (1x until 2560-wide, then floor(width/1280), max 4).
// AutoScale already upsizes the whole frame, so auto UiScale stays 1x.
// Pass the logical/window width, not the Retina drawable width.
int computeUiScale(int logicalWidth, int requestedScale, bool autoScale);

Vec2<int> uiLogicalSize(Vec2<int> displaySize, int uiScale);
Vec2<int> uiToDisplay(Vec2<int> uiPoint, int uiScale);
Vec2<int> displayToUi(Vec2<int> displayPoint, int uiScale);

// Whether the screen options are still exactly the compiled-in defaults. Currently only
// tested: the app-bundle display defaults that consumed it live on the macOS/iOS branch.
bool isFactoryWindowedDefault(int width, int height, const char *mode, bool autoScale);

} // namespace OpenApoc
