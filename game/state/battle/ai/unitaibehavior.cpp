#include "game/state/battle/ai/unitaibehavior.h"
#include "framework/configfile.h"
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
} // namespace

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

BattleUnit::BehaviorMode alienBehaviorFromConfig(const UString &mode,
                                                 BattleUnit::BehaviorMode fallback)
{
	if (mode == "aggressive")
	{
		return BattleUnit::BehaviorMode::Aggressive;
	}
	if (mode == "normal")
	{
		return BattleUnit::BehaviorMode::Normal;
	}
	// "cautious" is ai.txt's name for the mode the engine calls Evasive.
	if (mode == "cautious" || mode == "evasive")
	{
		return BattleUnit::BehaviorMode::Evasive;
	}
	return fallback;
}

namespace
{
// Adversarial-training knob, matching the OpenApoc.AlienAI.CoverBiasPercent idiom in
// unitaihelper.cpp. Empty (the shipped default) changes nothing.
//
// This existed as a config option, was dumped with the rest, and was read by NOTHING -- so ninety
// battles of co-evolution passed --OpenApoc.AlienAI.Behaviour on every launch and it had no effect
// at all. The alien side was searching one live dimension (cover bias) against X-COM's eight,
// which is why its genome wandered instead of adapting.
//
// Applied only to units the player does NOT own: this selects how the aliens fight, and a
// player's own squad keeps whatever mode the player set on it.
BattleUnit::BehaviorMode configuredAlienBehavior(const GameState &state, const BattleUnit &u,
                                                 BattleUnit::BehaviorMode current)
{
	if (!u.owner || u.owner == state.getPlayer())
	{
		return current;
	}
	return alienBehaviorFromConfig(config().getString("OpenApoc.AlienAI.Behaviour"), current);
}
} // namespace

std::tuple<AIDecision, bool> UnitAIBehavior::think(GameState &state, BattleUnit &u, bool interrupt)
{
	std::ignore = interrupt;

	u.behavior_mode = configuredAlienBehavior(state, u, u.behavior_mode);

	switch (u.behavior_mode)
	{
		case BattleUnit::BehaviorMode::Aggressive:
			// ai.txt: "Aggressive AI: Nothing?" — no cover/kneel; Default + Vanilla still run.
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
