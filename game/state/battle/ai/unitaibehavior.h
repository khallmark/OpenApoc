#pragma once
#include "game/state/battle/battleunit.h"
#include "game/state/battle/ai/unitai.h"

namespace OpenApoc
{

// AI that handles unit's behavior (Aggressive, Normal, Cautious)
// Maps the OpenApoc.AlienAI.Behaviour option onto the engine's own BehaviorMode. Returns
// `fallback` for an empty or unrecognised value, so the shipped default changes nothing.
//
// Exposed rather than kept file-local so it can be tested by calling it. The bug this exists to
// prevent is precisely a knob that is declared and never read -- a test that only checked the
// config round-trip would have passed for the whole time this option did nothing.
BattleUnit::BehaviorMode alienBehaviorFromConfig(const UString &mode,
                                                 BattleUnit::BehaviorMode fallback);

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
