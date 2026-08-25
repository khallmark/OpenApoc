#pragma once

#include <cstdint>

#define UFOPAEDIA_GROUP_STRTAB_OFFSET_START 0x152ADD
// Inclusive end: null byte after "Alien Craft". 0x152B57 lets StrTab read "Staff".
#define UFOPAEDIA_GROUP_STRTAB_OFFSET_END 0x152B56

// UFO2P non-4 catalog @ 0x1910A2 (281×8). FUN_000abf50 @ file 0xFE5F4:
// (char)packed == category, packed >> 16 == entry (0xFFFF = category header).
// pcxIndex indexes ufopaedia_pcx_names (V-TITLE …).
#pragma pack(push, 1)
struct UfopaediaCatalogRow
{
	uint32_t packed;
	uint16_t pcxIndex;
	uint16_t extra;
};
#pragma pack(pop)
static_assert(sizeof(struct UfopaediaCatalogRow) == 8, "Invalid ufopaedia catalog row");

#define UFOPAEDIA_CATALOG_OFFSET_START 0x1910A2
#define UFOPAEDIA_CATALOG_OFFSET_END 0x19196A
#define STARTING_AVAILABLE_UFOPAEDIA_OFFSET_START 0x19196A
#define STARTING_AVAILABLE_UFOPAEDIA_OFFSET_END 0x191A84
#define UFOPAEDIA_PCX_NAME_STRTAB_OFFSET_START 0x134AA8
#define UFOPAEDIA_PCX_NAME_STRTAB_OFFSET_END 0x1353F8
