#include "framework/configfile.h"
#include "game/state/battle/battle.h"
#include "game/state/battle/battlehazard.h"
#include "game/state/battle/battlemappart.h"
#include "game/state/battle/battleunit.h"
#include "game/state/city/vehicle.h"
#include "game/state/gametime.h"
#include "game/state/rules/aequipmenttype.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static bool test_made_up_and_tick_derived_constants()
{
	TEST_REQUIRE(HAZARD_SPREAD_CHANCE == 10, "HAZARD_SPREAD_CHANCE is {0} (made-up lock)",
	             HAZARD_SPREAD_CHANCE);
	TEST_REQUIRE(TICKS_PER_TURN == TICKS_PER_SECOND * 4, "TICKS_PER_TURN {0} != 4*TPS",
	             TICKS_PER_TURN);
	TEST_REQUIRE(TICKS_PER_HAZARD_UPDATE == TICKS_PER_TURN / 2, "TICKS_PER_HAZARD_UPDATE {0}",
	             TICKS_PER_HAZARD_UPDATE);
	TEST_REQUIRE(TICKS_PER_WOUND_EFFECT == TICKS_PER_TURN, "TICKS_PER_WOUND_EFFECT {0}",
	             TICKS_PER_WOUND_EFFECT);
	TEST_REQUIRE(TICKS_PER_ENZYME_EFFECT == TICKS_PER_SECOND / 9, "TICKS_PER_ENZYME_EFFECT {0}",
	             TICKS_PER_ENZYME_EFFECT);
	TEST_REQUIRE(TICKS_PER_FIRE_EFFECT == TICKS_PER_SECOND, "TICKS_PER_FIRE_EFFECT {0}",
	             TICKS_PER_FIRE_EFFECT);
	TEST_REQUIRE(FUEL_TICKS_PER_SECOND == 144, "FUEL_TICKS_PER_SECOND is {0}, should track TPS",
	             FUEL_TICKS_PER_SECOND);
	TEST_REQUIRE(FUEL_TICKS_PER_SECOND == static_cast<int>(TICKS_PER_SECOND),
	             "FUEL_TICKS_PER_SECOND {0} drifted from TICKS_PER_SECOND {1}",
	             FUEL_TICKS_PER_SECOND, TICKS_PER_SECOND);
	return true;
}

static bool test_fire_hazard_item_resist()
{
	// TACP FUN_0007c110: factor=(dl+19)/20; overlay 1 is power 10 → factor 1.
	TEST_REQUIRE(AEquipmentType::fireHazardDamage(10, 0) == 1, "Megapol resist 0 takes 1");
	TEST_REQUIRE(AEquipmentType::fireHazardDamage(10, 50) == 1, "Marsec resist 50 still takes 1");
	TEST_REQUIRE(AEquipmentType::fireHazardDamage(10, 100) == 0, "X-COM resist 100 takes 0");
	TEST_REQUIRE(AEquipmentType::fireHazardDamage(10, 200) == -1, "resist 200 delta is -1");
	return true;
}

static bool test_fire_overlay_power_progression()
{
	// TACP non-4 0x2E2AF4; FUN_0007ad94/FUN_0007ae18/FUN_0007b3dc.
	const std::vector<int> powerTable = {5,  10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70,
	                                     75, 70, 75, 70, 75, 70, 65, 55, 45, 35, 25, 15, 5};
	TEST_REQUIRE(BattleHazard::encodeFireOverlay(0) == 0x80, "fire overlay stage 0");
	TEST_REQUIRE(BattleHazard::encodeFireOverlay(25) == 0x99,
	             "preplaced marker 25 becomes fire overlay");
	TEST_REQUIRE(BattleHazard::encodeFireOverlay(26) == 0x9a, "fire overlay stage 26");
	TEST_REQUIRE(BattleHazard::fireOverlayStage(0x00) == -1, "empty overlay is not fire");
	TEST_REQUIRE(BattleHazard::fireOverlayStage(0x40) == -1, "type 1 is not fire");
	TEST_REQUIRE(BattleHazard::fireOverlayStage(0x80) == 0, "type 2 stage 0");
	TEST_REQUIRE(BattleHazard::fireOverlayStage(0x9a) == 26, "type 2 stage 26");
	TEST_REQUIRE(BattleHazard::fireOverlayStage(0xc1) == -1, "type 3 is not fire");
	TEST_REQUIRE(BattleHazard::fireOverlayPower(powerTable, 0x00) == 0, "non-fire overlay power");
	TEST_REQUIRE(BattleHazard::fireOverlayPower(powerTable, 0x80) == 5, "stage 0 power");
	TEST_REQUIRE(BattleHazard::fireOverlayPower(powerTable, 0x81) == 10, "stage 1 power");
	TEST_REQUIRE(BattleHazard::fireOverlayPower(powerTable, 0x8e) == 75, "stage 14 power");
	TEST_REQUIRE(BattleHazard::fireOverlayPower(powerTable, 0x9a) == 5, "stage 26 power");

	uint8_t overlay = BattleHazard::encodeFireOverlay(0);
	TEST_REQUIRE(BattleHazard::advanceFireOverlay(powerTable, overlay), "stage 0 advances");
	TEST_REQUIRE(overlay == BattleHazard::encodeFireOverlay(1), "advance retains type 2");
	overlay = BattleHazard::encodeFireOverlay(25);
	TEST_REQUIRE(BattleHazard::advanceFireOverlay(powerTable, overlay), "stage 25 advances");
	TEST_REQUIRE(overlay == BattleHazard::encodeFireOverlay(26), "stage 26 retained");
	TEST_REQUIRE(!BattleHazard::advanceFireOverlay(powerTable, overlay), "stage 26 extinguishes");
	TEST_REQUIRE(overlay == 0, "extinguished overlay is clear");
	return true;
}

static bool test_fire_overlay_terrain_threshold()
{
	// FUN_0007b3dc compares the six-bit stage directly against fire_burn_time.
	TEST_REQUIRE(!BattleMapPart::fireStageBurns(4, 5), "stage below burn time");
	TEST_REQUIRE(BattleMapPart::fireStageBurns(5, 5), "stage at burn time");
	TEST_REQUIRE(BattleMapPart::fireStageBurns(26, 5), "late stage burns");
	TEST_REQUIRE(!BattleMapPart::fireStageBurns(26, 255), "255 remains immune");
	TEST_REQUIRE(!BattleMapPart::fireStageBurns(-1, 0), "non-fire overlay cannot burn");
	return true;
}

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}
	applyDeterministicTestConfig();
	return runTestSuite({
	    {"made_up_and_tick_derived_constants", test_made_up_and_tick_derived_constants},
	    {"fire_hazard_item_resist", test_fire_hazard_item_resist},
	    {"fire_overlay_power_progression", test_fire_overlay_power_progression},
	    {"fire_overlay_terrain_threshold", test_fire_overlay_terrain_threshold},
	});
}
