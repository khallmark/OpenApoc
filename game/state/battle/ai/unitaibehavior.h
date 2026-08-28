#pragma once
#include "game/state/battle/ai/unitai.h"

namespace OpenApoc
{

// AI that handles unit's behavior (Aggressive, Normal, Cautious)
class UnitAIBehavior : public UnitAI
{
  public:
	UnitAIBehavior();

	Vec3<int> attackerPosition = {-1, -1, -1};
	int hitsSinceThink = 0;

	void reset(GameState &state, BattleUnit &u) override;
	std::tuple<AIDecision, bool> think(GameState &state, BattleUnit &u, bool interrupt) override;

	void notifyUnderFire(Vec3<int> position) override;
	void notifyHit(Vec3<int> position) override;
};
} // namespace OpenApoc
