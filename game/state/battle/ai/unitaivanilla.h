#pragma once
#include "game/state/battle/ai/unitai.h"

// #define VANILLA_AI_DEBUG_OUTPUT

namespace OpenApoc
{

enum class PsiStatus;

// AI that handles vanilla alien and civilian behavior
class UnitAIVanilla : public UnitAI
{
  public:
	UnitAIVanilla();

	uint64_t ticksLastThink = 0;
	// Value of 0 means we will not re-think based on timer
	uint64_t ticksUntilReThink = 0;

	// What AI decided to do at last think()
	AIDecision lastDecision;

	// Was enemy ever visible since last think()
	bool enemySpotted = false;
	// Previous value, to identify when enemy was first spotted
	bool enemySpottedPrevious = false;
	// Position of a person who attacked us since last think()
	Vec3<int> attackerPosition = {-1, -1, -1};
	// Position of last seen enemy since last think()
	Vec3<int> lastSeenEnemyPosition = {-1, -1, -1};

	// Enemy was spotted since we last re-thinked properly
	bool flagEnemySpotted = false;
	// Enemy attacked us since we last re-thinked properly
	Vec3<int> flagLastSeenPosition = {-1, -1, -1};
	// Enemy went MIA since we last re-thinked properly
	Vec3<int> flagLastAttackerPosition = {-1, -1, -1};

	void reset(GameState &state, BattleUnit &u) override;

	// Calculate AI's next decision, then do the routine
	// If unit has group AI, and patrol decision is made, the group will move together
	std::tuple<AIDecision, bool> think(GameState &state, BattleUnit &u, bool interrupt) override;

	// void reportExecuted(AIAction &action) override;
	void reportExecuted(AIMovement &movement) override;

	// Vanilla attack-priority primitives, extracted from getWeaponDecision()/getGrenadeDecision()
	// so they can be tested. Those two remain private and need a populated tile map just to be
	// reached (getAttackDecision -> canAttackUnit -> hasLineToUnit -> map.findCollision), which
	// is why the arithmetic that actually decides between weapons lives out here instead. The
	// precedent is TacticalAIVanilla::retreatChancePercent(). See tests/test_unit_ai_priority.cpp.

	// The documented ranking function: "Priority is CTH * DAMAGE / TIME". Ordering is the
	// contract; the absolute value is not compared against anything but other priorities.
	static float attackPriority(float cth, float damage, float time);
	// One unit's signed contribution to an AOE throw's worth: hostiles count for it, everyone
	// else (allies, civilians, the thrower's own side) counts against it.
	static float blastDamageContribution(float localDamage, bool hostile);
	// Whether an AOE throw survives the friendly-fire veto, given the summed contributions.
	// Negative net damage means the blast hurts our own more than theirs - don't throw.
	static bool aoeIsWorthThrowing(float netBlastDamage);

	void notifyUnderFire(Vec3<int> position) override;
	void notifyHit(Vec3<int> position) override;
	void notifyEnemySpotted(Vec3<int> position) override;

  private:
	AIDecision thinkInternal(GameState &state, BattleUnit &u);
	// Do the AI routine - organise inventory, move speed, stance etc.
	void routine(GameState &state, BattleUnit &u) override;

	void raiseFlags(GameState &state, BattleUnit &u);
	void clearFlags(GameState &state, BattleUnit &u);

	// Calculate AI's next decision in case no enemy is engaged
	// Return values are decision, priority, ticks until re-think
	std::tuple<AIDecision, float, unsigned> thinkGreen(GameState &state, BattleUnit &u);
	// Calculate AI's next decision in case enemy is engaged (attacking)
	// Return values are decision, priority, ticks until re-think
	std::tuple<AIDecision, float, unsigned> thinkRed(GameState &state, BattleUnit &u);

	std::tuple<AIDecision, float, unsigned> getAttackDecision(GameState &state, BattleUnit &u);

	std::tuple<AIDecision, float, unsigned> getWeaponDecision(GameState &state, BattleUnit &u,
	                                                          sp<AEquipment> e,
	                                                          StateRef<BattleUnit> target);
	std::tuple<AIDecision, float, unsigned> getPsiDecision(GameState &state, BattleUnit &u,
	                                                       sp<AEquipment> e,
	                                                       StateRef<BattleUnit> target,
	                                                       PsiStatus status) const;
	std::tuple<AIDecision, float, unsigned> getGrenadeDecision(GameState &state, BattleUnit &u,
	                                                           sp<AEquipment> e,
	                                                           StateRef<BattleUnit> target);
	std::tuple<AIDecision, float, unsigned> getBrainsuckerDecision(GameState &state, BattleUnit &u);
	std::tuple<AIDecision, float, unsigned> getSuicideDecision(GameState &state, BattleUnit &u);
};
} // namespace OpenApoc