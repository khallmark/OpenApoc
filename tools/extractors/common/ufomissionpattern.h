#pragma once

#include <cstdint>

namespace OpenApoc
{

// UFO2P non-4 UFO_mission_patterns at 0x155164 (VA 0x128D64). 400 bytes:
// 19 weeks × 10 uint16 + DEFAULT 10 uint16. Hexa inclusive end 1397490 missed the
// last 0x00 of the final word. P4 is +0xE00, bytes identical.
// Slot IDs: 3 Infiltration, 1 Attack, 2 Subversion, 5 Overspawn.
static const int UFO_MISSION_PATTERN_WEEK_COUNT = 19;
static const int UFO_MISSION_PATTERN_SLOT_COUNT = 10;
static const uint16_t UFO_MISSION_PATTERN_ATTACK = 1;
static const uint16_t UFO_MISSION_PATTERN_SUBVERSION = 2;
static const uint16_t UFO_MISSION_PATTERN_INFILTRATION = 3;
static const uint16_t UFO_MISSION_PATTERN_OVERSPAWN = 5;

#pragma pack(push, 1)
struct UfoMissionPatterns
{
	uint16_t weekly[UFO_MISSION_PATTERN_WEEK_COUNT][UFO_MISSION_PATTERN_SLOT_COUNT];
	uint16_t default_list[UFO_MISSION_PATTERN_SLOT_COUNT];
};
#pragma pack(pop)
static_assert(sizeof(struct UfoMissionPatterns) == 400, "Invalid UfoMissionPatterns size");

#define UFO_MISSION_PATTERNS_OFFSET_START 0x155164
#define UFO_MISSION_PATTERNS_OFFSET_END                                                            \
	(UFO_MISSION_PATTERNS_OFFSET_START + sizeof(struct UfoMissionPatterns))

} // namespace OpenApoc
