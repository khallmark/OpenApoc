// OPE-20: locks the "Behavior" AI's exposure/under-fire bookkeeping - UnitAIBehavior::reset(),
// notifyUnderFire() and notifyHit() - the state that UnitAIBehavior::think() reads back (as
// `underFire` / `heavilyPressed`) to decide whether to seek cover via
// UnitAIHelper::getTakeCoverMovement()/getKneelMovement() (see
// game/state/battle/ai/unitaibehavior.cpp). Those two helper functions, and the cover-scoring
// arithmetic they call, are PR 42 / OPE-20 territory and already locked by
// tests/test_unit_ai_helper.cpp; this file locks the smaller half that PR did not touch - the
// bookkeeping this file's think() uses to decide *whether* to call them at all.
//
// think() itself - the BehaviorMode switch, the cover/kneel/prone decision tree, canFire's
// visibleEnemies scan - is NOT covered here. Exercising it needs a live BattleUnit sitting in a
// populated Battle with a real tile map (UnitAIHelper::getTakeCoverMovement() calls
// findShortestPath(), and canAttackUnit() needs hasLineToUnit() -> tileObject->map.findCollision(),
// same dependency test_unit_ai_priority.cpp's header comment describes for the sibling vanilla
// file). That is legitimately out of proportion for this slice - a fixture would need to build a
// battle and a tile map just to exercise a handful of branches, and following that thread further
// would pull in files outside this PR's one-translation-unit scope. Disclosed gap, not fixed:
// deleting the `BattleUnit::BehaviorMode::Aggressive` case's early return, or hardcoding `cautious`
// to false, compiles cleanly and passes this suite undetected - neither is covered by anything
// below.
//
// reset()/notifyUnderFire()/notifyHit() do not touch either of their reference parameters (reset's
// GameState& and BattleUnit& are unnamed in the production signature), so this suite still
// constructs a real GameState and BattleUnit - the interface requires them - but never populates
// either; the object under test is pure state on UnitAIBehavior itself.

#include "framework/configfile.h"
#include "framework/logger.h"
#include "game/state/battle/ai/unitaibehavior.h"
#include "game/state/battle/battleunit.h"
#include "game/state/gamestate.h"
#include "library/sp.h"
#include "library/vec.h"
#include <cstdlib>

using namespace OpenApoc;

namespace
{

bool vecEquals(const Vec3<int> &a, const Vec3<int> &b) { return a == b; }

bool checkDefaultState()
{
	UnitAIBehavior behavior;
	if (!vecEquals(behavior.attackerPosition, {-1, -1, -1}))
	{
		LogError("default_state: attackerPosition should default to the NONE sentinel "
		         "{{-1,-1,-1}}, got {{{0},{1},{2}}}",
		         behavior.attackerPosition.x, behavior.attackerPosition.y,
		         behavior.attackerPosition.z);
		return false;
	}
	if (behavior.hitsSinceThink != 0)
	{
		LogError("default_state: hitsSinceThink should default to 0, got {0}",
		         behavior.hitsSinceThink);
		return false;
	}
	return true;
}

bool checkNotifyUnderFireSetsPositionOnly()
{
	UnitAIBehavior behavior;
	behavior.notifyUnderFire({5, 6, 1});
	if (!vecEquals(behavior.attackerPosition, {5, 6, 1}))
	{
		LogError("notify_under_fire: attackerPosition should be set to the reported position");
		return false;
	}
	if (behavior.hitsSinceThink != 0)
	{
		LogError("notify_under_fire: hitsSinceThink must NOT change - being shot at and missed is "
		         "not a hit, got {0}",
		         behavior.hitsSinceThink);
		return false;
	}
	return true;
}

bool checkNotifyHitSetsPositionAndCountsHit()
{
	UnitAIBehavior behavior;
	behavior.notifyHit({7, 8, 2});
	if (!vecEquals(behavior.attackerPosition, {7, 8, 2}))
	{
		LogError("notify_hit: attackerPosition should be set to the hit's position");
		return false;
	}
	if (behavior.hitsSinceThink != 1)
	{
		LogError("notify_hit: a single hit must bring hitsSinceThink to 1, got {0}",
		         behavior.hitsSinceThink);
		return false;
	}
	return true;
}

bool checkNotifyHitAccumulatesAcrossMultipleHits()
{
	UnitAIBehavior behavior;
	behavior.notifyHit({1, 1, 1});
	behavior.notifyHit({2, 2, 2});
	behavior.notifyHit({3, 3, 3});
	if (behavior.hitsSinceThink != 3)
	{
		LogError("notify_hit_accumulates: three hits must bring hitsSinceThink to 3, got {0}",
		         behavior.hitsSinceThink);
		return false;
	}
	// The most recent hit's position wins - think() only ever needs the latest attacker to react
	// to.
	if (!vecEquals(behavior.attackerPosition, {3, 3, 3}))
	{
		LogError("notify_hit_accumulates: attackerPosition should track the latest hit");
		return false;
	}
	return true;
}

bool checkResetClearsExposureState()
{
	UnitAIBehavior behavior;
	behavior.notifyHit({4, 4, 4});
	behavior.notifyHit({5, 5, 5});

	GameState state;
	auto unit = mksp<BattleUnit>();
	behavior.reset(state, *unit);

	if (!vecEquals(behavior.attackerPosition, {-1, -1, -1}))
	{
		LogError("reset: attackerPosition must be cleared back to the NONE sentinel");
		return false;
	}
	if (behavior.hitsSinceThink != 0)
	{
		LogError("reset: hitsSinceThink must be cleared back to 0, got {0}",
		         behavior.hitsSinceThink);
		return false;
	}
	return true;
}

bool checkConstructorSetsBehaviorType()
{
	UnitAIBehavior behavior;
	if (behavior.type != UnitAI::Type::Behavior)
	{
		LogError("constructor: UnitAIBehavior must report UnitAI::Type::Behavior so "
		         "gamestate_serialize.cpp round-trips it correctly");
		return false;
	}
	return true;
}

} // namespace

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	int failures = 0;
	struct Case
	{
		const char *name;
		bool (*fn)();
	};
	const Case cases[] = {
	    {"default_state", checkDefaultState},
	    {"notify_under_fire_sets_position_only", checkNotifyUnderFireSetsPositionOnly},
	    {"notify_hit_sets_position_and_counts_hit", checkNotifyHitSetsPositionAndCountsHit},
	    {"notify_hit_accumulates_across_multiple_hits", checkNotifyHitAccumulatesAcrossMultipleHits},
	    {"reset_clears_exposure_state", checkResetClearsExposureState},
	    {"constructor_sets_behavior_type", checkConstructorSetsBehaviorType},
	};

	for (const auto &c : cases)
	{
		if (!c.fn())
		{
			LogError("[{0}] FAILED", c.name);
			failures++;
		}
	}

	if (failures > 0)
	{
		LogError("test_unit_ai_behavior: {0} of {1} cases failed", failures,
		         static_cast<int>(std::size(cases)));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
