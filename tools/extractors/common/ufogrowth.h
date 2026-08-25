#pragma once

#include <cstdint>

namespace OpenApoc
{

// UFO2P non-4 file 0x155010 / VA 0x128C10. 339 bytes: 10×uint16 caps, 15×10 weekly
// uint16 counts (hexa craft 0..9), then DEFAULT (craft 0..8 uint16 + mothership uint8).
// P4 sits at 0x155E10. Dump: hexa_UFO_growth_rates_00155010_00155163.hex
static const int UFO_GROWTH_CRAFT_COUNT = 10;
static const int UFO_GROWTH_WEEK_COUNT = 15;

#pragma pack(push, 1)
struct UfoGrowthRates
{
	uint16_t fleet_caps[UFO_GROWTH_CRAFT_COUNT];
	uint16_t weekly_spawn[UFO_GROWTH_WEEK_COUNT][UFO_GROWTH_CRAFT_COUNT];
	uint16_t default_spawn[UFO_GROWTH_CRAFT_COUNT - 1];
	uint8_t default_mothership;
};
#pragma pack(pop)
static_assert(sizeof(struct UfoGrowthRates) == 339, "Invalid UfoGrowthRates size");

#define UFO_GROWTH_RATES_OFFSET_START 1396752
#define UFO_GROWTH_RATES_OFFSET_END 1397091

} // namespace OpenApoc
