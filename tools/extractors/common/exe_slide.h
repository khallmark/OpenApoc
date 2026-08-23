#pragma once

#include "tools/extractors/common/crew.h"
#include <cstdint>
#include <iosfwd>

namespace OpenApoc
{

// ISO non-4 is extractor-canonical. 4-build tables were byte-compared at these
// slides (sibling canonical/UFO2P4.EXE and TACP4.EXE). Not a global guess: every
// DataChunk/StrTab we read matched at this delta and failed at the unshifted
// offset. Unknown CRCs keep slide 0 and log.

static const uint32_t UFO2P_CRC_NON4 = 0x4749ffc1;
static const uint32_t UFO2P_CRC_4 = 0xdbd3b41d;
static const uint32_t TACP_CRC_NON4 = 0xfebbe39e;
static const uint32_t TACP_CRC_4 = 0x3ec9c268;

inline bool ufo2pFileSlide(uint32_t crc, int32_t &slide)
{
	if (crc == UFO2P_CRC_NON4)
	{
		slide = 0;
		return true;
	}
	if (crc == UFO2P_CRC_4)
	{
		slide = 0xE00;
		return true;
	}
	slide = 0;
	return false;
}

inline bool tacpFileSlide(uint32_t crc, int32_t &slide)
{
	if (crc == TACP_CRC_NON4)
	{
		slide = 0;
		return true;
	}
	if (crc == TACP_CRC_4)
	{
		slide = -0x2200;
		return true;
	}
	slide = 0;
	return false;
}

// crew_ufo_downed @ 0x13E560 is byte-identical at the same file offset on 4-build (P↔P4).
inline std::streamoff ufo2pTableOffset(int32_t slide, std::streamoff canonicalOffset)
{
	if (slide == 0xE00 && canonicalOffset >= CREW_UFO_DOWNED_OFFSET_START &&
	    canonicalOffset < CREW_UFO_DOWNED_OFFSET_END)
	{
		return canonicalOffset;
	}
	return canonicalOffset + slide;
}

} // namespace OpenApoc
