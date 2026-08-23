#include "game/state/battle/ai/unitaibehavior.h"
#include "game/state/battle/ai/aidecision.h"
#include "game/state/battle/ai/unitaihelper.h"
#include "game/state/battle/battle.h"
#include "game/state/battle/battleunit.h"
#include "game/state/gamestate.h"

namespace OpenApoc
{

namespace
{
static const std::tuple<AIDecision, bool> NULLTUPLE2 = std::make_tuple(AIDecision(), false);
static const Vec3<int> NONE = {-1, -1, -1};
}

UnitAIBehavior::UnitAIBehavior() { type = Type::Behavior; }

void UnitAIBehavior::reset(GameState &, BattleUnit &)
{
	attackerPosition = NONE;
	hitsSinceThink = 0;
}

void UnitAIBehavior::notifyUnderFire(Vec3<int> position) { attackerPosition = position; }

void UnitAIBehavior::notifyHit(Vec3<int> position)
{
	attackerPosition = position;
	hitsSinceThink++;
}

std::tuple<AIDecision, bool> UnitAIBehavior::think(GameState &state, BattleUnit &u, bool interrupt)
{
	std::ignore = interrupt;

	switch (u.behavior_mode)
	{
		case BattleUnit::BehaviorMode::Aggressive:
			active = false;
			attackerPosition = NONE;
			hitsSinceThink = 0;
			return NULLTUPLE2;
		case BattleUnit::BehaviorMode::Normal:
		case BattleUnit::BehaviorMode::Evasive:
			active = true;
			break;
	}

	if (!active || u.isMoving())
	{
		return NULLTUPLE2;
	}

	const bool cautious = u.behavior_mode == BattleUnit::BehaviorMode::Evasive;
	const bool underFire = attackerPosition != NONE;
	const bool heavilyPressed = hitsSinceThink > 0 || underFire;
	bool canFire = false;
	for (auto &enemy : u.visibleEnemies)
	{
		if (enemy && u.canAttackUnit(state, enemy) != WeaponStatus::NotFiring)
		{
			canFire = true;
			break;
		}
	}

	auto cover = UnitAIHelper::getTakeCoverMovement(state, u, cautious);
	if (cover)
	{
		attackerPosition = NONE;
		hitsSinceThink = 0;
		return std::make_tuple(AIDecision(nullptr, cover), true);
	}

	if (cautious || underFire || heavilyPressed || !canFire)
	{
		if (cautious && u.canProne(u.position, u.facing))
		{
			auto prone = mksp<AIMovement>();
			prone->type = AIMovement::Type::ChangeStance;
			prone->movementMode = MovementMode::Prone;
			prone->kneelingMode = KneelingMode::None;
			attackerPosition = NONE;
			hitsSinceThink = 0;
			return std::make_tuple(AIDecision(nullptr, prone), true);
		}
		auto kneel = UnitAIHelper::getKneelMovement(state, u, cautious || underFire);
		if (kneel)
		{
			attackerPosition = NONE;
			hitsSinceThink = 0;
			return std::make_tuple(AIDecision(nullptr, kneel), true);
		}
	}

	attackerPosition = NONE;
	hitsSinceThink = 0;
	return NULLTUPLE2;
}
} // namespace OpenApoc
