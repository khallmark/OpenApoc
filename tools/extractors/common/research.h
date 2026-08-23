#pragma once

#include "library/strings.h"
#include "library/strings_format.h"
#include <cstdint>

struct ResearchData
{
	uint8_t labSize;  // 0 = small, 1 = large
	uint8_t unknown1; // item combinator: 0 All, 1 Any of three typed gates
	uint8_t unknown2; // tech combinator; 0 All, 1 Any of prereqTech[3] (FUN_000aa7a8)
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

// prereqType 3 (alien lifeform) on UFO2P non-4 research_data: uint16 prereq is
// an index into 15 live bio slots, dead specimen = live + 15.
// Slots 0-12 = agent_type_names[34..46] ALIVE/DEAD (Multiworm egg … Micronoid
// Aggregate). Slot 13 = Brainsucker Pod (no dead). Slot 14 = Overspawn.
static const uint16_t UFO2P_ALIEN_LIFEFORM_DEAD_BIAS = 15;
static const uint16_t UFO2P_ALIEN_LIFEFORM_LIVE_COUNT = 15;
static const int UFO2P_ALIEN_LIFEFORM_FIRST_AGENT = 34;

static inline OpenApoc::UString ufo2pAlienLifeformItemId(uint16_t prereq)
{
	const bool dead = prereq >= UFO2P_ALIEN_LIFEFORM_DEAD_BIAS;
	const uint16_t live =
	    dead ? static_cast<uint16_t>(prereq - UFO2P_ALIEN_LIFEFORM_DEAD_BIAS) : prereq;
	if (live >= UFO2P_ALIEN_LIFEFORM_LIVE_COUNT)
	{
		return "";
	}
	if (live == 13)
	{
		return dead ? "" : "AEQUIPMENTTYPE_BRAINSUCKER_POD";
	}
	if (live == 14)
	{
		return dead ? "AEQUIPMENTTYPE_OVERSPAWN_DEAD" : "AEQUIPMENTTYPE_OVERSPAWN_ALIVE";
	}
	static const char *kLiveSpeciesCanon[13] = {
	    "MULTIWORM_EGG",       "BRAINSUCKER", "MULTIWORM", "HYPERWORM", "CHRYSALIS", "ANTHROPOD",
	    "SKELETOID",           "SPITTER",     "POPPER",    "MEGASPAWN", "PSIMORPH",  "QUEENSPAWN",
	    "MICRONOID_AGGREGATE",
	};
	return OpenApoc::format("AEQUIPMENTTYPE_{0}_{1}", kLiveSpeciesCanon[live],
	                        dead ? "DEAD" : "ALIVE");
}

// FUN_000aa7a8 @ VA 0xAA7A8 / file 0x10CE4C after unknown2==4 zeros the
// combinator (ISO non-4 SI dispatch file 0x10D141; 4-build 0x10D9B5).
// Record 38 (SI==0x26) All of topics 8..31. Record 44 (SI==0x2C) Any of 7..33.
// Record 37 (SI==0x25) @ file 0x10D162: All of 12..17, then CMP [0xDE420]
// (0xDE2B8 + 36*10 = Genetic Structure) and MOV EBX,1. 4-build same
// immediates @ file 0x10D9D6 / 0x10D9FD.
inline constexpr const char *UFO2P_AA7A8_LIFE_CYCLE_ALL[] = {
    "RESEARCH_MULTIWORM_AUTOPSY",           "RESEARCH_MULTIWORM",
    "RESEARCH_HYPERWORM_AUTOPSY",           "RESEARCH_HYPERWORM",
    "RESEARCH_CHRYSALIS_AUTOPSY",           "RESEARCH_CHRYSALIS",
    "RESEARCH_THE_ALIEN_GENETIC_STRUCTURE",
};
inline constexpr const char *UFO2P_AA7A8_THREAT_ALL[] = {
    "RESEARCH_BRAINSUCKER_AUTOPSY",   "RESEARCH_BRAINSUCKER",
    "RESEARCH_MULTIWORM_EGG_AUTOPSY", "RESEARCH_MULTIWORM_EGG",
    "RESEARCH_MULTIWORM_AUTOPSY",     "RESEARCH_MULTIWORM",
    "RESEARCH_HYPERWORM_AUTOPSY",     "RESEARCH_HYPERWORM",
    "RESEARCH_CHRYSALIS_AUTOPSY",     "RESEARCH_CHRYSALIS",
    "RESEARCH_ANTHROPOD_AUTOPSY",     "RESEARCH_ANTHROPOD",
    "RESEARCH_PSIMORPH_AUTOPSY",      "RESEARCH_PSIMORPH",
    "RESEARCH_SPITTER_AUTOPSY",       "RESEARCH_SPITTER",
    "RESEARCH_MEGASPAWN_AUTOPSY",     "RESEARCH_MEGASPAWN",
    "RESEARCH_POPPER_AUTOPSY",        "RESEARCH_POPPER",
    "RESEARCH_SKELETOID_AUTOPSY",     "RESEARCH_SKELETOID",
    "RESEARCH_MICRONOID_AUTOPSY",     "RESEARCH_MICRONOID",
};
inline constexpr const char *UFO2P_AA7A8_BIOCHEM_ANY[] = {
    "RESEARCH_BRAINSUCKER_PODS", "RESEARCH_BRAINSUCKER_AUTOPSY",
    "RESEARCH_BRAINSUCKER",      "RESEARCH_MULTIWORM_EGG_AUTOPSY",
    "RESEARCH_MULTIWORM_EGG",    "RESEARCH_MULTIWORM_AUTOPSY",
    "RESEARCH_MULTIWORM",        "RESEARCH_HYPERWORM_AUTOPSY",
    "RESEARCH_HYPERWORM",        "RESEARCH_CHRYSALIS_AUTOPSY",
    "RESEARCH_CHRYSALIS",        "RESEARCH_ANTHROPOD_AUTOPSY",
    "RESEARCH_ANTHROPOD",        "RESEARCH_PSIMORPH_AUTOPSY",
    "RESEARCH_PSIMORPH",         "RESEARCH_SPITTER_AUTOPSY",
    "RESEARCH_SPITTER",          "RESEARCH_MEGASPAWN_AUTOPSY",
    "RESEARCH_MEGASPAWN",        "RESEARCH_POPPER_AUTOPSY",
    "RESEARCH_POPPER",           "RESEARCH_SKELETOID_AUTOPSY",
    "RESEARCH_SKELETOID",        "RESEARCH_MICRONOID_AUTOPSY",
    "RESEARCH_MICRONOID",        "RESEARCH_QUEENSPAWN_AUTOPSY",
    "RESEARCH_QUEENSPAWN",
};
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
