#pragma once

#include <cstdint>

namespace OpenApoc
{

// UFO2P non-4 UFO_mission_data at 0x13DDFC (VA 0x1119FC). 45 × 42-byte records.
// Slots: craft/count/role ×3, then 24 unnamed bytes. P4 is +0xE00, bytes identical.
// Role: 5 Attack, 7 Infiltration, 8 Subversion, 10 Overspawn, 11 Escort.
static const int UFO_MISSION_SLOT_COUNT = 3;
static const int UFO_MISSION_RECORD_COUNT = 45;
static const uint16_t UFO_MISSION_ROLE_ATTACK = 5;
static const uint16_t UFO_MISSION_ROLE_INFILTRATION = 7;
static const uint16_t UFO_MISSION_ROLE_SUBVERSION = 8;
static const uint16_t UFO_MISSION_ROLE_OVERSPAWN = 10;
static const uint16_t UFO_MISSION_ROLE_ESCORT = 11;

#pragma pack(push, 1)
struct UfoMissionData
{
	uint16_t craft[UFO_MISSION_SLOT_COUNT];
	uint16_t count[UFO_MISSION_SLOT_COUNT];
	uint16_t role[UFO_MISSION_SLOT_COUNT];
	uint8_t unknown[24];
};
#pragma pack(pop)
static_assert(sizeof(struct UfoMissionData) == 42, "Invalid UFO_mission_data size");

#define UFO_MISSION_DATA_OFFSET_START 0x13DDFC
#define UFO_MISSION_DATA_OFFSET_END                                                                \
	(UFO_MISSION_DATA_OFFSET_START + UFO_MISSION_RECORD_COUNT * sizeof(struct UfoMissionData))

} // namespace OpenApoc
