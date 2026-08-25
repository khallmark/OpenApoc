#pragma once

#include "game/state/stateobject.h"
#include "library/strings.h"
#include <vector>

namespace OpenApoc
{

class GameState;

class UFOGrowth : public StateObject<UFOGrowth>
{
  public:
	int week = 0;
	std::vector<std::pair<UString, int>> vehicleTypeList;

	// Pick UFO_GROWTH_<week> by the stored week field; fall back to UFO_GROWTH_DEFAULT.
	static sp<UFOGrowth> selectForWeek(const GameState &state, int week);

	// UFOPaedia Organic Factory (UFO2P): "produces strange organic mushrooms which grow into
	// Alien craft." Weekly growth runs only while that alien-city building is intact. If the
	// function is missing from the map, growth is allowed so extract-less tests still run.
	static bool craftFactoryIntact(const GameState &state);
};
}; // namespace OpenApoc
