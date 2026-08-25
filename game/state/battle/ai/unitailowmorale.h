#pragma once
#include "game/state/battle/ai/unitai.h"

namespace OpenApoc
{

// AI that handles Panic and Berserk
class UnitAILowMorale : public UnitAI
{
  public:
	UnitAILowMorale();

	uint64_t ticksActionAvailable = 0;

	void reset(GameState &state, BattleUnit &u) override;
	std::tuple<AIDecision, bool> think(GameState &state, BattleUnit &u, bool interrupt) override;

	// ai.txt Panic Run: pick a LOS block farther from the closest globally visible enemy.
	static bool isFartherFromEnemy(const Vec3<float> &candidate, const Vec3<float> &current,
	                               const Vec3<float> &enemy);
};
} // namespace OpenApoc