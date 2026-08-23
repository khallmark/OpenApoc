#pragma once

#include <cstdint>

namespace OpenApoc
{

// UFO2P non-4 UFO_mission_data at 0x13DDFC. Live LE copy is object2 0xDC758
// (claimed VA 0x1119FC is zeros). 45 × 42-byte records. P4 is +0xE00, bytes identical.
// Role: 5 Attack, 7 Infiltration, 8 Subversion, 10 Overspawn, 11 Escort.
// Tail reader: FUN_0006da88 @ VA 0x6DA88 / file 0xD012C (ISO non-4).
// follow_slot 0xFFFF = none; else index into this row's craft[] (stored as that
// craft type on the vehicle). zone_mode / scatter feed FUN_0003b724 @ file
// 0x2B723 (`FUN_0005d1d8(scatter*2)`). mission_counter (+0x1B) is copied to
// vehicle +0x171 (`FUN_0006da88` @ file 0xD030B). FUN_0003a910 @ LE object-page
// file 0x2A90F decrements it each time the UFO reaches a mission destination;
// zero advances the mission/target state. The old Hexa `building_function`
// label was incorrect. type_percent multiplies vehicle_data constitution
// (+0x2e → instance +0x12e / VehicleType::health).
static const int UFO_MISSION_SLOT_COUNT = 3;
static const int UFO_MISSION_RECORD_COUNT = 45;
static const uint16_t UFO_MISSION_ROLE_ATTACK = 5;
static const uint16_t UFO_MISSION_ROLE_INFILTRATION = 7;
static const uint16_t UFO_MISSION_ROLE_SUBVERSION = 8;
static const uint16_t UFO_MISSION_ROLE_OVERSPAWN = 10;
static const uint16_t UFO_MISSION_ROLE_ESCORT = 11;
static const int16_t UFO_MISSION_FOLLOW_NONE = -1;

#pragma pack(push, 1)
struct UfoMissionData
{
	uint16_t craft[UFO_MISSION_SLOT_COUNT];
	uint16_t count[UFO_MISSION_SLOT_COUNT];
	uint16_t role[UFO_MISSION_SLOT_COUNT];
	int16_t follow_slot[UFO_MISSION_SLOT_COUNT];
	uint8_t zone_mode[UFO_MISSION_SLOT_COUNT];
	uint8_t mission_counter[UFO_MISSION_SLOT_COUNT];
	uint16_t scatter[UFO_MISSION_SLOT_COUNT];
	uint16_t type_percent[UFO_MISSION_SLOT_COUNT];
};
#pragma pack(pop)
static_assert(sizeof(struct UfoMissionData) == 42, "Invalid UFO_mission_data size");

#define UFO_MISSION_DATA_OFFSET_START 0x13DDFC
#define UFO_MISSION_DATA_OFFSET_END                                                                \
	(UFO_MISSION_DATA_OFFSET_START + UFO_MISSION_RECORD_COUNT * sizeof(struct UfoMissionData))

} // namespace OpenApoc
