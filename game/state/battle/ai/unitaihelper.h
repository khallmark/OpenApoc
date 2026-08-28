#pragma once
#include "library/sp.h"
#include "library/vec.h"
#include <vector>

namespace OpenApoc
{

class GameState;
class BattleUnit;
class AIMovement;
class AEquipment;
enum class EquipmentSlotType;

class UnitAIHelper
{
  public:
	// TACP FUN_0007e600 builds a 21x21x13 tile box around the candidate and only counts threats
	// inside it, clamped against the map bounds. Recovered exactly.
	static constexpr int COVER_BOX_XY = 21;
	static constexpr int COVER_BOX_Z = 13;

	static sp<AIMovement> getFallbackMovement(GameState &state, BattleUnit &u, bool forced = false);

	static sp<AIMovement> getRetreatMovement(GameState &state, BattleUnit &u, bool forced = false);

	// B1 cover metric, recovered from TACP FUN_0007e600 / FUN_0008c1fc. Scores one candidate
	// position by how exposed it is: 0 when no qualifying hostile can see it, decreasing by one
	// unit of penalty per hostile that can. Higher (nearer zero) is safer, and the caller
	// max-selects. Pure and static so the decision is testable without a battle.
	//
	// Deliberately a COUNT. The original weights each threat by a range/facing-indexed table
	// (DAT_001D3425, indexed via FUN_0008a364) whose multiplied factor was never traced --
	// OpenApoc-og-research lab, TACP.EXE, function FUN_0007e600 (VA 0x7E600), "B1 cover-metric
	// pass 2" findings section 4; that write-up is not yet part of this repo's docs tree, so it
	// is cited by function/address rather than by path. The magnitude is therefore NOT recovered.
	// The ordering it produces for the common case -- fewer hostiles with line of sight is safer
	// -- is, and that is the behaviourally load-bearing part. Do not invent weights to "complete"
	// this.
	static int exposureScore(const std::vector<Vec3<float>> &threats, Vec3<float> candidate,
	                         int boxXY = COVER_BOX_XY, int boxZ = COVER_BOX_Z);

	static sp<AIMovement> getTakeCoverMovement(GameState &state, BattleUnit &u,
	                                           bool forced = false);

	static sp<AIMovement> getKneelMovement(GameState &state, BattleUnit &u, bool forced = false);

	static sp<AIMovement> getPursueMovement(GameState &state, BattleUnit &u, Vec3<int> target,
	                                        bool forced = false);

	static sp<AIMovement> getTurnMovement(GameState &state, BattleUnit &u, Vec3<int> target);

	static void ensureItemInSlot(GameState &state, sp<AEquipment> item, EquipmentSlotType slot);
};
} // namespace OpenApoc