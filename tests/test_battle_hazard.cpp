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
	// HAZARD_SPREAD_CHANCE was deleted: F1's hazard spread RNG is now recovered
	// (docs/original-game/findings/B5-F1-K1-hazards.md F1 §2), see
	// hazard_spread_uses_recovered_rng / fire_neighbour_table_matches_recovered_bytes below.
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

static bool test_fire_neighbour_table_matches_recovered_bytes()
{
	// NOTE: the task that produced this test asked for
	// "fire_neighbour_table_preserves_dead_outcome", guarding fire's neighbour
	// table against ever tidying away a documented dead (0,0,0) outcome at
	// index 0 (docs/original-game/findings/B5-F1-K1-hazards.md F1 §2.3,
	// parity-guide.md's "F1's open question"). While implementing this, that
	// documented (0,0,0) value was re-checked directly against the bound
	// `OpenApocOG_TACP` Ghidra project (QueryNeighbourTableFull.java) and does
	// NOT match the binary: byte 0x29306C reads 1, not 0, twice independently
	// re-read, with the addressing formula separately re-confirmed against the
	// live disassembly listing (see the comment on
	// BattleHazard::fireSpreadNeighbourDelta). Outcome 0 is East, not dead --
	// there is no dead outcome in fire's table. A test presupposing one would
	// itself be the invented behaviour this fork's prime directive forbids, so
	// this test asserts the corrected bytes and is named for what it actually
	// guards: fire's table matching the re-verified binary content, not a
	// specific hypothesis about which outcome is "the" dead one. Report this
	// naming deviation and the underlying correction to the finding's owner.
	TEST_REQUIRE(BattleHazard::fireSpreadNeighbourDelta(0) == (Vec3<int>{1, 0, 0}),
	             "outcome 0 is East (1,0,0), re-verified from the binary -- NOT dead");
	TEST_REQUIRE(BattleHazard::fireSpreadNeighbourDelta(1) == (Vec3<int>{0, 1, 0}),
	             "outcome 1 is South");
	TEST_REQUIRE(BattleHazard::fireSpreadNeighbourDelta(2) == (Vec3<int>{-1, 0, 0}),
	             "outcome 2 is West");
	TEST_REQUIRE(BattleHazard::fireSpreadNeighbourDelta(3) == (Vec3<int>{0, -1, 0}),
	             "outcome 3 is North");
	TEST_REQUIRE(BattleHazard::fireSpreadNeighbourDelta(4) == (Vec3<int>{0, 0, 1}),
	             "outcome 4 is Up");
	// Exactly 5 reachable outcomes (0..4), all live -- the RNG span constant
	// is the source of truth for the roll's actual range; assert it directly
	// so a future edit narrowing/widening the roll is caught here too.
	TEST_REQUIRE(BattleHazard::FIRE_SPREAD_NEIGHBOUR_RNG_SPAN == 4,
	             "fire's neighbour roll is RNG(0..4) inclusive -- 5 outcomes, span {0}",
	             BattleHazard::FIRE_SPREAD_NEIGHBOUR_RNG_SPAN);
	// Down is never reached by fire's table -- it is the one direction fire's
	// five outcomes omit from the generic engine's six (§2.3 vs §2.4). None of
	// fire's five outcomes should ever be a dead (0,0,0) either -- that guard
	// stays valuable even though the *specific* index the finding predicted no
	// longer applies.
	bool sawDeadOutcome = false;
	for (int outcome = 0; outcome <= BattleHazard::FIRE_SPREAD_NEIGHBOUR_RNG_SPAN; outcome++)
	{
		const auto delta = BattleHazard::fireSpreadNeighbourDelta(outcome);
		TEST_REQUIRE(delta != (Vec3<int>{0, 0, -1}), "Down is unreachable from fire's table");
		if (delta == (Vec3<int>{0, 0, 0}))
		{
			sawDeadOutcome = true;
		}
	}
	TEST_REQUIRE(!sawDeadOutcome, "fire's table has no dead outcome -- all five entries are live");

	// The generalized engine's table (FUN_0007ae78, §2.4) is a clean 6-outcome
	// compass+up/down set with NO dead slot -- this table's bytes matched the
	// finding document exactly on re-read (unlike fire's, above).
	TEST_REQUIRE(BattleHazard::GENERIC_SPREAD_NEIGHBOUR_RNG_SPAN == 5,
	             "generic neighbour roll is RNG(0..5) inclusive -- 6 outcomes, span {0}",
	             BattleHazard::GENERIC_SPREAD_NEIGHBOUR_RNG_SPAN);
	TEST_REQUIRE(BattleHazard::genericSpreadNeighbourDelta(0) == (Vec3<int>{1, 0, 0}),
	             "generic outcome 0 is East");
	TEST_REQUIRE(BattleHazard::genericSpreadNeighbourDelta(1) == (Vec3<int>{0, 1, 0}),
	             "generic outcome 1 is South");
	TEST_REQUIRE(BattleHazard::genericSpreadNeighbourDelta(2) == (Vec3<int>{-1, 0, 0}),
	             "generic outcome 2 is West");
	TEST_REQUIRE(BattleHazard::genericSpreadNeighbourDelta(3) == (Vec3<int>{0, -1, 0}),
	             "generic outcome 3 is North");
	TEST_REQUIRE(BattleHazard::genericSpreadNeighbourDelta(4) == (Vec3<int>{0, 0, 1}),
	             "generic outcome 4 is Up");
	TEST_REQUIRE(BattleHazard::genericSpreadNeighbourDelta(5) == (Vec3<int>{0, 0, -1}),
	             "generic outcome 5 is Down");
	for (int outcome = 0; outcome <= BattleHazard::GENERIC_SPREAD_NEIGHBOUR_RNG_SPAN; outcome++)
	{
		TEST_REQUIRE(BattleHazard::genericSpreadNeighbourDelta(outcome) != (Vec3<int>{0, 0, 0}),
		             "generic engine has no dead outcome at {0}", outcome);
	}
	return true;
}

static bool test_hazard_spread_uses_recovered_rng()
{
	// docs/original-game/findings/B5-F1-K1-hazards.md F1 §2.1/§2.2/§2.4: spread
	// decisions are RNG(0..10) inclusive + baseline compared against a
	// per-terrain resistance value -- never a flat 10%-of-100 roll.

	// The roll span is 11 outcomes (0..10 inclusive), not a percentage out of 100.
	TEST_REQUIRE(BattleHazard::FIRE_SPREAD_THRESHOLD_RNG_SPAN == 10,
	             "fire's threshold roll is RNG(0..10) inclusive -- 11 outcomes, span {0}",
	             BattleHazard::FIRE_SPREAD_THRESHOLD_RNG_SPAN);

	// hazardRoll is an inclusive uniform draw on [0, max]: never exceeds max,
	// and (across enough deterministic seeds) actually reaches the max value --
	// distinguishing it from an off-by-one exclusive roll.
	bool sawMax = false;
	for (uint32_t seed = 1; seed <= 200; seed++)
	{
		Xorshift128Plus<uint32_t> rng(seed);
		const int v = BattleHazard::hazardRoll(rng, 10);
		TEST_REQUIRE(v >= 0 && v <= 10, "hazardRoll({0}) in range, got {1}", seed, v);
		if (v == 10)
		{
			sawMax = true;
		}
	}
	TEST_REQUIRE(sawMax, "hazardRoll(10) reaches the inclusive upper bound across seeds");

	// Comparison direction: spread iff resistance is strictly less than the
	// rolled threshold (JNC = no-spread when resistance >= threshold). The
	// boundary case (resistance == threshold) must NOT spread.
	TEST_REQUIRE(BattleHazard::fireSpreadResistanceGate(3, 5),
	             "resistance below threshold spreads");
	TEST_REQUIRE(!BattleHazard::fireSpreadResistanceGate(5, 5),
	             "resistance == threshold does not spread (boundary)");
	TEST_REQUIRE(!BattleHazard::fireSpreadResistanceGate(7, 5),
	             "resistance above threshold does not spread");
	TEST_REQUIRE(!BattleHazard::fireSpreadResistanceGate(0, 0),
	             "zero resistance vs zero threshold does not spread (boundary)");

	// Fire's spread roll draws the shared RNG TWICE per invocation (threshold,
	// then neighbour) -- the generic engine's draws it ONCE. This asymmetry is
	// the §2.4 correction: FUN_0007ae78 is not "fire but generic", it takes its
	// threshold as a caller-supplied parameter instead of rolling one.
	{
		Xorshift128Plus<uint32_t> rngViaFn(4242);
		Xorshift128Plus<uint32_t> rngViaManualDraws(4242);
		bool spreads = false;
		BattleHazard::rollFireSpreadNeighbour(rngViaFn, /*baseline=*/0, /*resistance=*/0, spreads);
		BattleHazard::hazardRoll(rngViaManualDraws, BattleHazard::FIRE_SPREAD_THRESHOLD_RNG_SPAN);
		BattleHazard::hazardRoll(rngViaManualDraws, BattleHazard::FIRE_SPREAD_NEIGHBOUR_RNG_SPAN);
		uint64_t sFn[2];
		uint64_t sManual[2];
		rngViaFn.getState(sFn);
		rngViaManualDraws.getState(sManual);
		TEST_REQUIRE(sFn[0] == sManual[0] && sFn[1] == sManual[1],
		             "fire spread consumes exactly two draws, threshold then neighbour");
	}
	{
		Xorshift128Plus<uint32_t> rngViaFn(4242);
		Xorshift128Plus<uint32_t> rngViaManualDraw(4242);
		BattleHazard::rollGenericSpreadNeighbour(rngViaFn);
		BattleHazard::hazardRoll(rngViaManualDraw, BattleHazard::GENERIC_SPREAD_NEIGHBOUR_RNG_SPAN);
		uint64_t sFn[2];
		uint64_t sManual[2];
		rngViaFn.getState(sFn);
		rngViaManualDraw.getState(sManual);
		TEST_REQUIRE(sFn[0] == sManual[0] && sFn[1] == sManual[1],
		             "generic spread consumes exactly one draw -- no internal threshold roll");
	}
	return true;
}

static bool test_fire_scheduler_state_machine()
{
	unsigned accumulatedTicks = 0;
	TEST_REQUIRE(Battle::consumeVanillaFireTicks(accumulatedTicks, 3) == 0 && accumulatedTicks == 3,
	             "partial vanilla fire tick");
	TEST_REQUIRE(Battle::consumeVanillaFireTicks(accumulatedTicks, 1) == 1 && accumulatedTicks == 0,
	             "one vanilla fire tick");
	TEST_REQUIRE(Battle::consumeVanillaFireTicks(accumulatedTicks, 7) == 1 && accumulatedTicks == 3,
	             "batched vanilla fire ticks");

	TEST_REQUIRE(Battle::fireRowsPerVanillaTick(100, 10) == 13, "100x10 row batch");
	TEST_REQUIRE(Battle::fireRowsPerVanillaTick(8, 8) == 0, "8x8 preserves zero row batch");
	TEST_REQUIRE(Battle::fireRowsPerVanillaTick(8, 9) == 1, "8x9 one row batch");
	TEST_REQUIRE(Battle::fireRowsPerVanillaTick(17, 17) == 4, "17x17 four row batch");
	TEST_REQUIRE(Battle::fireRowsPerVanillaTick(0, 10) == 0, "empty map row batch");

	struct FireEvent
	{
		bool contact;
		int y;
		int z;
	};
	std::vector<FireEvent> events;
	int scheduledY = 11;
	int scheduledZ = 11;
	unsigned scheduledCounter = 36;
	Battle::runFireSchedulerTicks(
	    1, 12, 12, scheduledY, scheduledZ, scheduledCounter, [&](int y, int z)
	    { events.push_back({false, y, z}); }, [&]() { events.push_back({true, -1, -1}); });
	TEST_REQUIRE(events.size() == 3, "two rows then contact");
	TEST_REQUIRE(!events[0].contact && events[0].y == 11 && events[0].z == 11,
	             "first scheduled row");
	TEST_REQUIRE(!events[1].contact && events[1].y == 0 && events[1].z == 0,
	             "wrapped scheduled row");
	TEST_REQUIRE(events[2].contact, "contact follows row progression");
	TEST_REQUIRE(scheduledY == 1 && scheduledZ == 0, "scheduled cursor persists");
	TEST_REQUIRE(scheduledCounter == 1, "scheduled contact resets then increments");

	events.clear();
	scheduledY = 12;
	scheduledZ = 12;
	scheduledCounter = 0;
	Battle::runFireSchedulerTicks(
	    1, 12, 12, scheduledY, scheduledZ, scheduledCounter,
	    [&](int y, int z) { events.push_back({false, y, z}); }, []() {});
	TEST_REQUIRE(!events.empty() && events[0].y == 0 && events[0].z == 0,
	             "stale cursors normalize before processing");

	events.clear();
	scheduledCounter = 36;
	Battle::runFireSchedulerTicks(
	    1, 8, 8, scheduledY, scheduledZ, scheduledCounter, [&](int y, int z)
	    { events.push_back({false, y, z}); }, [&]() { events.push_back({true, -1, -1}); });
	TEST_REQUIRE(events.size() == 1 && events[0].contact,
	             "zero-row map still performs contact pass");

	int turnBasedContacts = 0;
	scheduledCounter = 0;
	Battle::runFireSchedulerTicks(
	    400, 8, 8, scheduledY, scheduledZ, scheduledCounter, [](int, int) {},
	    [&]() { turnBasedContacts++; });
	TEST_REQUIRE(turnBasedContacts == 11, "400-iteration TB contact count {0}", turnBasedContacts);
	TEST_REQUIRE(scheduledCounter == 4, "400-iteration TB counter phase {0}", scheduledCounter);

	int rowY = 0;
	int rowZ = 0;
	Battle::advanceFireRowCursor(3, 2, rowY, rowZ);
	TEST_REQUIRE(rowY == 1 && rowZ == 0, "cursor first row");
	Battle::advanceFireRowCursor(3, 2, rowY, rowZ);
	Battle::advanceFireRowCursor(3, 2, rowY, rowZ);
	TEST_REQUIRE(rowY == 0 && rowZ == 1, "cursor wraps Y");
	Battle::advanceFireRowCursor(3, 2, rowY, rowZ);
	Battle::advanceFireRowCursor(3, 2, rowY, rowZ);
	Battle::advanceFireRowCursor(3, 2, rowY, rowZ);
	TEST_REQUIRE(rowY == 0 && rowZ == 0, "cursor wraps Z");

	unsigned counter = 0;
	for (int tick = 0; tick < 36; tick++)
	{
		TEST_REQUIRE(!Battle::advanceFireContactCounter(counter), "early contact pass at tick {0}",
		             tick);
	}
	TEST_REQUIRE(counter == 36, "contact counter before threshold {0}", counter);
	TEST_REQUIRE(Battle::advanceFireContactCounter(counter), "counter 0x24 contact pass");
	TEST_REQUIRE(counter == 1, "counter resets then increments");
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
	    {"fire_neighbour_table_matches_recovered_bytes",
	     test_fire_neighbour_table_matches_recovered_bytes},
	    {"hazard_spread_uses_recovered_rng", test_hazard_spread_uses_recovered_rng},
	    {"fire_scheduler_state_machine", test_fire_scheduler_state_machine},
	});
}
