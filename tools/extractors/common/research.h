#pragma once

#include <cstdint>

struct ResearchData
{
	uint8_t labSize; // 0 = small, 1 = large
	uint8_t unknown1;
	uint8_t unknown2;
	uint8_t
	    prereqType; // 0 = craft equipment, 1 = agent equipment, 3 = alien life form, 0xff = nune
	uint16_t unknown3;
	uint16_t prereq;
	uint16_t leadsTo1;
	uint16_t leadsTo2;
	uint16_t prereqTech[3]; // IDX into research list, 0xffff for none
	uint16_t score;
	uint32_t skillHours;
	uint8_t researchGroup; // 0 = BioChem, 1 = Quantum Phys
	uint8_t ufopaediaGroup;
	uint16_t ufopaediaEntry;
};

static_assert(sizeof(struct ResearchData) == 28, "Invalid research_data size");

#define RESEARCH_DATA_OFFSET_START 0x13EE80
#define RESEARCH_DATA_OFFSET_END 0x13F954
#define RESEARCH_NAME_STRTAB_OFFSET_START 0x14E3BA
#define RESEARCH_NAME_STRTAB_OFFSET_END 0x14EA20
#define RESEARCH_DESCRIPTION_STRTAB_OFFSET_START 0x14EA22
#define RESEARCH_DESCRIPTION_STRTAB_OFFSET_END 0x1501F1

// UFO2P non-4 manufacturing_data at 0x13FD34 (VA 0x113934). 43 × 50-byte records.
// Names: manufacturing_items 0x1501F3–0x15055A. hexa.txt [48] is short; EXE stride is 50.
#pragma pack(push, 1)
struct ManufacturingData
{
	uint16_t manufacturable; // 0 = false, 1 = true
	uint32_t skillHours;
	uint16_t unused1;
	uint32_t unused2;
	uint16_t techRequired; // research_names index
	uint8_t padding[26];
	uint32_t manufacturingCost;
	uint16_t itemType; // 0 vequip, 1 aequip, 2 vequip ammo, 3 craft
	uint16_t itemIndex;
	uint16_t labSize; // 0 small, 1 large
};
#pragma pack(pop)
static_assert(sizeof(struct ManufacturingData) == 50, "Invalid manufacturing_data size");

#define MANUFACTURING_DATA_OFFSET_START 0x13FD34
#define MANUFACTURING_DATA_OFFSET_END 0x14059A
#define MANUFACTURING_NAME_STRTAB_OFFSET_START 0x1501F3
#define MANUFACTURING_NAME_STRTAB_OFFSET_END 0x15055A
