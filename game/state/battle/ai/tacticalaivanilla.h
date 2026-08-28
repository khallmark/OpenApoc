#pragma once

#include "game/state/battle/ai/tacticalai.h"

namespace OpenApoc
{

// Vanilla AI that tries to replicate how aliens/civilians/security moved around the map
class TacticalAIVanilla : public TacticalAI
{
  public:
	TacticalAIVanilla();

	void beginTurnRoutine(GameState &state, StateRef<Organisation> o) override;
	std::list<std::pair<std::list<StateRef<BattleUnit>>, AIDecision>>
	think(GameState &state, StateRef<Organisation> o) override;

	// Chance to retreat is [0 to 50]% as neutralized allies go [50 to 100]%.
	// unitsTotal <= 0 returns 0 (think() LogAsserts a positive count).
	static int retreatChancePercent(int unitsTotal, int unitsActive);

	std::tuple<std::list<StateRef<BattleUnit>>, sp<AIMovement>> getPatrolMovement(GameState &state,
	                                                                              BattleUnit &u);
};
} // namespace OpenApoc