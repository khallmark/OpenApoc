#include "framework/configfile.h"
#include "framework/logger.h"
#include "game/state/rules/aequipmenttype.h"

using namespace OpenApoc;

namespace
{

// Expected values below are derived by hand-evaluating the formula exactly as written in
// AEquipmentType::fireHazardDamage() (aequipmenttype.cpp):
//   factor = (static_cast<unsigned char>(std::max(0, powerByte)) + 19) / 20;
//   delta  = factor - (resist * factor) / 100;
// That formula's provenance is a comment citing a decompiled address (TACP FUN_0007c110 @ VA
// 0x7C110 / file 0xD6BB4) carried over unmodified from the source branch; this test does not
// independently re-derive it from the binary, it only pins the C++ implementation as written.
struct FireHazardCase
{
	int powerByte;
	int resist;
	int expected;
};

bool test_static_form()
{
	const FireHazardCase cases[] = {
	    // clang-format off
	    {0,   0,   0},
	    {1,   0,   1},
	    {20,  0,   1},
	    {21,  0,   2},
	    {255, 0,   13},
	    {255, 50,  7},
	    {255, 100, 0},
	    // resist above 100 (field range is documented 0..200) drives the delta negative -
	    // that sign is part of the contract callers rely on, so pin it explicitly.
	    {255, 200, -13},
	    {1,   200, -1},
	    // negative powerByte is clamped to 0 before the byte cast
	    {-5,  0,   0},
	    // powerByte is carried in a single byte upstream (TACP `dl` register); this case pins
	    // the unsigned-char truncation the C++ port performs for out-of-byte-range input. It
	    // documents the current implementation, not a value read back from the binary.
	    {300, 0,   3},
	    // clang-format on
	};

	for (auto &c : cases)
	{
		int actual = AEquipmentType::fireHazardDamage(c.powerByte, c.resist);
		if (actual != c.expected)
		{
			LogError("AEquipmentType::fireHazardDamage({0}, {1}) = {2}, expected {3}", c.powerByte,
			         c.resist, actual, c.expected);
			return false;
		}
	}
	return true;
}

bool test_member_form()
{
	// Default-constructed hazardResist is 0, so the member overload should agree with the
	// static form called with resist 0.
	AEquipmentType defaultType;
	if (defaultType.hazardResist != 0)
	{
		LogError("AEquipmentType::hazardResist default = {0}, expected 0",
		         defaultType.hazardResist);
		return false;
	}
	int defaultActual = defaultType.fireHazardDamage(255);
	int defaultExpected = AEquipmentType::fireHazardDamage(255, 0);
	if (defaultActual != defaultExpected)
	{
		LogError("AEquipmentType::fireHazardDamage(255) with default hazardResist = {0}, "
		         "expected {1}",
		         defaultActual, defaultExpected);
		return false;
	}

	// A non-default hazardResist should be threaded through to the static form unchanged.
	AEquipmentType resistantType;
	resistantType.hazardResist = 50;
	int actual = resistantType.fireHazardDamage(255);
	int expected = AEquipmentType::fireHazardDamage(255, 50);
	if (actual != expected)
	{
		LogError("AEquipmentType::fireHazardDamage(255) with hazardResist=50 = {0}, expected "
		         "{1} (should delegate to the static form)",
		         actual, expected);
		return false;
	}
	return true;
}

} // namespace

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}
	if (!test_static_form())
	{
		return EXIT_FAILURE;
	}
	if (!test_member_form())
	{
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
