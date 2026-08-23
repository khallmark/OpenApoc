#pragma once

#include "game/state/stateobject.h"
#include "library/strings.h"
#include <map>
#include <utility>
#include <vector>

namespace OpenApoc
{

class UFOIncursionSlot
{
  public:
	UString followVehicleType;
	int zoneMode = 0;
	int missionCounter = 0;
	int scatter = 0;
	int typePercent = 0;
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
