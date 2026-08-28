#pragma once

#include "game/state/stateobject.h"
#include "library/strings.h"
#include <map>
#include <utility>
#include <vector>

namespace OpenApoc
{

// UFO2P DAT_0012d950, dumped fresh: one byte per role at stride 0x94, role in [0,15]. This is
// the damaged-withdrawal threshold as a percent of VehicleType::health -- FUN_0006da88 indexes
// it by UFO_mission_data role[slot] to seed vehicle +0x168, which FUN_000588f8 then compares
// against current constitution.
//
// It is NOT type_percent. Those are two separate sibling fields in the same 42-byte record
// (role at byte 0xC, type_percent at 0x28) read for different purposes -- an earlier pass
// conflated them, and an earlier pass also asserted a flat 75%, which holds only at a call site
// this population structurally never reaches.
//
// Roles 11 (Escort) and 9 sit at 10%, which is provably below crash_health on every UFO hull --
// so the band is empty and the gate can never fire for them. That is recovered behaviour, not a
// bug to round away. See docs/original-game/findings/U1b-gate-consumer.md.
static const int UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE[16] = {
    75, 50, 25, 15, 33, 30, 10, 30, 20, 10, 25, 10, 10, 10, 10, 0,
};

class UFOIncursionSlot
{
  public:
	UString followVehicleType;
	int zoneMode = 0;
	int missionCounter = 0;
	int scatter = 0;
	int typePercent = 0;
	// UFO_mission_data role[slot] (record byte 0xC). The extractor always parsed this to reach
	// the spawn logic but used to discard it. It is retained because FUN_0006da88 uses it to
	// index a separate fixed 16-entry table for the damaged-withdrawal threshold -- a different
	// value from typePercent, which is a per-record field read for a different purpose.
	// See docs/original-game/findings/U1b-gate-consumer.md.
	int role = 0;
};

class UFOIncursion : public StateObject<UFOIncursion>
{
  public:
	enum class PrimaryMission
	{
		Infiltration,
		Subversion,
		Attack,
		Overspawn
	};

	PrimaryMission primaryMission = PrimaryMission::Infiltration;
	std::vector<std::pair<UString, int>> primaryList;
	std::vector<std::pair<UString, int>> escortList;
	std::vector<std::pair<UString, int>> attackList;
	// Parallel to the lists above (same slot order). followVehicleType is
	// craft[follow_slot] from UFO2P UFO_mission_data; empty if follow_slot is
	// 0xFFFF. zoneMode/scatter are consumed on gate exit (FUN_0003b724).
	// typePercent is constitution×percent/100 (VehicleType::health) and the
	// scatter>50→10 clamp. missionCounter is the +0x1B byte copied to vehicle
	// +0x171; FUN_0003a910 decrements it on mission-destination arrival.
	std::vector<UFOIncursionSlot> primarySlots;
	std::vector<UFOIncursionSlot> escortSlots;
	std::vector<UFOIncursionSlot> attackSlots;
	int priority = 0;
};

}; // namespace OpenApoc
