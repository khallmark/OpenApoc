#pragma once

#include <algorithm>
#include <cmath>

namespace OpenApoc
{

inline float uiAnimationProgress(float elapsedMilliseconds, float durationMilliseconds)
{
	const float t = std::clamp(elapsedMilliseconds / durationMilliseconds, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

inline float uiAnimationPulse(float elapsedMilliseconds, float periodMilliseconds)
{
	constexpr float TWO_PI = 6.28318530718f;
	const float phase = std::fmod(elapsedMilliseconds, periodMilliseconds) / periodMilliseconds;
	return 0.5f - 0.5f * std::cos(TWO_PI * phase);
}

} // namespace OpenApoc
