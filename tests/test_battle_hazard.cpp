// Tests for game/state/battle/battlehazard.{h,cpp}.
//
// This file covers three behaviors extracted from the fork's `develop` branch into this
// single translation unit (battlehazard.cpp/.h are one TU, so all three ship together):
//
//   * Section 1 -- fire overlay scheduler state and cadence boundary (encode/decode of the
//     recovered TACP tile overlay byte, and the update()/updateTB() stand-down gate that
//     reads it).
//   * Section 2 -- overlay power table lookup and stage advance/extinguish (the parameterized
//     subset of overlay power computation that does not require GameState/BattleMapPart/
//     BattleItem wiring -- see the note at the top of section 2 below for what was
//     deliberately left out and why).
//   * Section 3 -- recovered hazard placement/spread RNG primitives (uniform draw contract,
//     resistance-gate comparison, neighbour tables, and per-call draw-count/ordering).
//
// All three are exercised as pure static-method tests against fixed inputs/seeds -- no
// GameState, Battle, or tile map is constructed, and the RNG-driven section is made
// deterministic by seeding Xorshift128Plus directly rather than relying on statistics.

#include "framework/configfile.h"
#include "framework/logger.h"
#include "game/state/battle/battlehazard.h"
#include "library/xorshift.h"
#include <string>

using namespace OpenApoc;

namespace
{
int failureCount = 0;

void check(bool condition, const std::string &description)
{
	if (!condition)
	{
		failureCount++;
		LogError("FAILED: {0}", description);
	}
}

// -- Section 1: scheduler state and cadence boundary (OPE-17) --------------

void test_fire_overlay_encode_decode()
{
	// encodeFireOverlay sets the type-2 marker bits and folds the stage into
	// the low six bits.
	check(BattleHazard::encodeFireOverlay(0) == 0x80, "stage 0 encodes to 0x80");
	check(BattleHazard::encodeFireOverlay(1) == 0x81, "stage 1 encodes to 0x81");
	check(BattleHazard::encodeFireOverlay(26) == 0x9a, "stage 26 encodes to 0x9a");
	// Stage is masked to six bits: a stage beyond the mask wraps rather than
	// corrupting the type bits.
	check(BattleHazard::encodeFireOverlay(63) == 0xbf, "stage 63 fills the index mask");
	check(BattleHazard::encodeFireOverlay(64) == 0x80, "stage 64 wraps back to 0 in the mask");

	// fireOverlayStage only recognizes type 2 (top two bits == 0b10); any
	// other type, including zero/uninitialized, reports "not fire" via -1.
	check(BattleHazard::fireOverlayStage(0x00) == -1, "zero overlay is not fire");
	check(BattleHazard::fireOverlayStage(0x40) == -1, "type 1 is not fire");
	check(BattleHazard::fireOverlayStage(0x80) == 0, "type 2 stage 0 decodes to 0");
	check(BattleHazard::fireOverlayStage(0x9a) == 26, "type 2 stage 26 decodes to 26");
	check(BattleHazard::fireOverlayStage(0xc1) == -1, "type 3 is not fire");
	check(BattleHazard::fireOverlayStage(0xff) == -1, "type 3 (all bits set) is not fire");

	// Round trip for every representable stage.
	bool roundTripOk = true;
	for (unsigned stage = 0; stage <= BattleHazard::FIRE_OVERLAY_INDEX_MASK; stage++)
	{
		const uint8_t encoded = BattleHazard::encodeFireOverlay(stage);
		if (BattleHazard::fireOverlayStage(encoded) != static_cast<int>(stage))
		{
			roundTripOk = false;
			break;
		}
	}
	check(roundTripOk, "every stage in [0, FIRE_OVERLAY_INDEX_MASK] round-trips through encode/decode");
}

void test_fire_overlay_cadence_boundary_is_inert_by_default()
{
	// The update()/updateTB() cadence boundary stands down the legacy
	// per-hazard scheduler exactly when fireOverlayStage(fireOverlay) >= 0.
	// A default-constructed hazard has fireOverlay == 0, which decodes to
	// stage -1 -- so the boundary must not trip for any hazard that nothing
	// has written an overlay onto, preserving today's behavior exactly.
	BattleHazard hazard;
	check(hazard.fireOverlay == 0, "default-constructed hazard has no overlay");
	check(BattleHazard::fireOverlayStage(hazard.fireOverlay) < 0,
	      "default overlay decodes to 'not fire' so the cadence gate stays inert");

	// Once something does encode a type-2 overlay onto the field, the same
	// predicate the gate uses must report it as active.
	hazard.fireOverlay = BattleHazard::encodeFireOverlay(0);
	check(BattleHazard::fireOverlayStage(hazard.fireOverlay) >= 0,
	      "an encoded type-2 overlay reports as active to the cadence gate");
}

// -- Section 2: overlay power table (OPE-15, parameterized subset) ---------
//
// NOTE ON SCOPE: BattleHazard::advanceOriginalFireOverlay() and
// applyOriginalFireItemEffect() (terrain-damage thresholds via
// BattleMapPart::fireStageBurns(), and item effects via
// BattleItem::applyFireHazard()), plus the corresponding branch in
// applyEffect(), are NOT part of this extraction and are therefore not
// tested here. They require GameState::fireHazardPowerTable (gamestate.h),
// BattleMapPart::fireStageBurns() (battlemappart.h/.cpp), and
// BattleItem::applyFireHazard() (battleitem.h/.cpp) -- three files outside
// this PR's single-translation-unit scope. Only the parameterized power
// table lookup and stage-advance primitives, which take their table as a
// plain std::vector<int> argument and touch no external type, are extracted
// and tested below.

void test_fire_overlay_power_table()
{
	// A synthetic power-by-stage table (shape only -- not asserting this
	// matches the original game's actual table contents, which live on
	// GameState and are out of scope here).
	const std::vector<int> powerTable = {5, 10, 15, 20, 0};

	check(BattleHazard::fireOverlayPower(powerTable, 0x00) == 0,
	      "non-fire overlay has zero power regardless of table contents");
	check(BattleHazard::fireOverlayPower(powerTable, BattleHazard::encodeFireOverlay(0)) == 5,
	      "stage 0 reads table[0]");
	check(BattleHazard::fireOverlayPower(powerTable, BattleHazard::encodeFireOverlay(3)) == 20,
	      "stage 3 reads table[3]");
	check(BattleHazard::fireOverlayPower(powerTable, BattleHazard::encodeFireOverlay(4)) == 0,
	      "a table entry of zero is zero power (terminal stage)");
	check(BattleHazard::fireOverlayPower(powerTable, BattleHazard::encodeFireOverlay(5)) == 0,
	      "a stage past the end of the table is zero power, not an out-of-bounds read");

	const std::vector<int> emptyTable;
	check(BattleHazard::fireOverlayPower(emptyTable, BattleHazard::encodeFireOverlay(0)) == 0,
	      "an empty table never reads out of bounds");
}

void test_fire_overlay_advance_and_extinguish()
{
	const std::vector<int> powerTable = {5, 10, 15, 20, 0};

	// A live stage with nonzero power at the *next* stage advances and keeps
	// the type-2 marker.
	uint8_t overlay = BattleHazard::encodeFireOverlay(0);
	check(BattleHazard::advanceFireOverlay(powerTable, overlay), "stage 0 advances (stage 1 power > 0)");
	check(overlay == BattleHazard::encodeFireOverlay(1), "advance moves to stage 1, retains type 2");

	check(BattleHazard::advanceFireOverlay(powerTable, overlay), "stage 1 advances");
	check(BattleHazard::advanceFireOverlay(powerTable, overlay), "stage 2 advances");
	check(overlay == BattleHazard::encodeFireOverlay(3), "now at stage 3");

	// Stage 3's *next* entry (index 4) is zero power -> extinguish: advance
	// fails and clears the overlay to 0.
	check(!BattleHazard::advanceFireOverlay(powerTable, overlay), "stage 3 extinguishes (next power is 0)");
	check(overlay == 0, "extinguished overlay is cleared to 0");

	// A non-fire overlay never advances.
	uint8_t nonFire = 0x40;
	check(!BattleHazard::advanceFireOverlay(powerTable, nonFire), "non-fire overlay does not advance");
	check(nonFire == 0, "non-fire overlay is cleared, not left dangling");

	// A stage whose own power is already zero (not just its successor) also
	// fails to advance, per fireOverlayPower(...) == 0 guard.
	uint8_t deadStage = BattleHazard::encodeFireOverlay(4);
	check(!BattleHazard::advanceFireOverlay(powerTable, deadStage),
	      "a stage with zero power of its own does not advance");
	check(deadStage == 0, "clears to 0");
}

// -- Section 3: recovered spread RNG primitives (OPE-22) --------------------

void test_hazard_roll_inclusive_bounds()
{
	// hazardRoll draws uniformly on [0, max] inclusive: never exceeds max,
	// and (checked across many seeds) actually reaches max -- distinguishing
	// it from an off-by-one exclusive roll.
	bool sawMax = false;
	bool everyDrawInRange = true;
	for (uint32_t seed = 1; seed <= 300; seed++)
	{
		Xorshift128Plus<uint32_t> rng(seed);
		const int v = BattleHazard::hazardRoll(rng, 10);
		if (v < 0 || v > 10)
		{
			everyDrawInRange = false;
		}
		if (v == 10)
		{
			sawMax = true;
		}
	}
	check(everyDrawInRange, "hazardRoll(rng, 10) never exceeds [0, 10] across 300 seeds");
	check(sawMax, "hazardRoll(rng, 10) reaches the inclusive upper bound across 300 seeds");

	// max == 0 always returns 0 (single-outcome draw, no UB from
	// uniform_int_distribution(min, min)).
	Xorshift128Plus<uint32_t> rngZero(7);
	check(BattleHazard::hazardRoll(rngZero, 0) == 0, "hazardRoll(rng, 0) is always 0");

	// A negative max is clamped to 0 rather than throwing or reading garbage.
	Xorshift128Plus<uint32_t> rngNeg(7);
	check(BattleHazard::hazardRoll(rngNeg, -5) == 0, "hazardRoll(rng, negative) clamps to max=0");
}

void test_fire_spread_resistance_gate()
{
	// Comparison direction: spread iff resistance is strictly less than the
	// rolled threshold. The boundary case (resistance == threshold) must NOT
	// spread.
	check(BattleHazard::fireSpreadResistanceGate(3, 5), "resistance below threshold spreads");
	check(!BattleHazard::fireSpreadResistanceGate(5, 5),
	      "resistance == threshold does not spread (boundary)");
	check(!BattleHazard::fireSpreadResistanceGate(7, 5),
	      "resistance above threshold does not spread");
	check(!BattleHazard::fireSpreadResistanceGate(0, 0),
	      "zero resistance vs zero threshold does not spread (boundary)");
}

void test_neighbour_tables()
{
	// Fire's 5-outcome table: East, South, West, North, Up. No dead (0,0,0)
	// outcome, and Down is never reachable from fire's table.
	check(BattleHazard::fireSpreadNeighbourDelta(0) == (Vec3<int>{1, 0, 0}), "fire outcome 0 is East");
	check(BattleHazard::fireSpreadNeighbourDelta(1) == (Vec3<int>{0, 1, 0}), "fire outcome 1 is South");
	check(BattleHazard::fireSpreadNeighbourDelta(2) == (Vec3<int>{-1, 0, 0}), "fire outcome 2 is West");
	check(BattleHazard::fireSpreadNeighbourDelta(3) == (Vec3<int>{0, -1, 0}), "fire outcome 3 is North");
	check(BattleHazard::fireSpreadNeighbourDelta(4) == (Vec3<int>{0, 0, 1}), "fire outcome 4 is Up");
	check(BattleHazard::fireSpreadNeighbourDelta(-1) == (Vec3<int>{0, 0, 0}),
	      "out-of-range outcome (below) is the sentinel zero vector");
	check(BattleHazard::fireSpreadNeighbourDelta(5) == (Vec3<int>{0, 0, 0}),
	      "out-of-range outcome (above) is the sentinel zero vector");

	bool sawDeadOutcome = false;
	bool sawDown = false;
	for (int outcome = 0; outcome <= BattleHazard::FIRE_SPREAD_NEIGHBOUR_RNG_SPAN; outcome++)
	{
		const auto delta = BattleHazard::fireSpreadNeighbourDelta(outcome);
		if (delta == (Vec3<int>{0, 0, 0}))
		{
			sawDeadOutcome = true;
		}
		if (delta == (Vec3<int>{0, 0, -1}))
		{
			sawDown = true;
		}
	}
	check(!sawDeadOutcome, "none of fire's five live outcomes is a dead (0,0,0) entry");
	check(!sawDown, "Down is unreachable from fire's table");

	// The generalized engine's table: the same 5 plus Down, no dead slot.
	check(BattleHazard::genericSpreadNeighbourDelta(0) == (Vec3<int>{1, 0, 0}), "generic outcome 0 East");
	check(BattleHazard::genericSpreadNeighbourDelta(1) == (Vec3<int>{0, 1, 0}), "generic outcome 1 South");
	check(BattleHazard::genericSpreadNeighbourDelta(2) == (Vec3<int>{-1, 0, 0}), "generic outcome 2 West");
	check(BattleHazard::genericSpreadNeighbourDelta(3) == (Vec3<int>{0, -1, 0}), "generic outcome 3 North");
	check(BattleHazard::genericSpreadNeighbourDelta(4) == (Vec3<int>{0, 0, 1}), "generic outcome 4 Up");
	check(BattleHazard::genericSpreadNeighbourDelta(5) == (Vec3<int>{0, 0, -1}), "generic outcome 5 Down");

	bool genericSawDead = false;
	for (int outcome = 0; outcome <= BattleHazard::GENERIC_SPREAD_NEIGHBOUR_RNG_SPAN; outcome++)
	{
		if (BattleHazard::genericSpreadNeighbourDelta(outcome) == (Vec3<int>{0, 0, 0}))
		{
			genericSawDead = true;
		}
	}
	check(!genericSawDead, "the generic engine's six outcomes contain no dead entry");

	// The RNG span constants are the source of truth for the roll's actual
	// range -- assert them directly so a future edit narrowing/widening the
	// roll is caught here too, not just in the table-shape checks above.
	check(BattleHazard::FIRE_SPREAD_NEIGHBOUR_RNG_SPAN == 4, "fire's neighbour roll spans 5 outcomes");
	check(BattleHazard::GENERIC_SPREAD_NEIGHBOUR_RNG_SPAN == 5,
	      "the generic engine's neighbour roll spans 6 outcomes");
	check(BattleHazard::FIRE_SPREAD_THRESHOLD_RNG_SPAN == 10,
	      "fire's threshold roll is RNG(0..10) inclusive -- 11 outcomes");
}

void test_spread_roll_draw_count_and_order()
{
	// Fire's spread roll consumes the shared RNG stream TWICE per
	// invocation -- threshold first, then neighbour -- while the generic
	// engine's consumes it exactly ONCE (no internal threshold roll, since
	// its threshold is caller-supplied). Proven deterministically by
	// comparing final RNG state against manually replaying the expected draw
	// sequence on an identically-seeded RNG, rather than by statistics.
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
		check(sFn[0] == sManual[0] && sFn[1] == sManual[1],
		      "fire's spread roll consumes exactly two draws, threshold then neighbour, in that order");
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
		check(sFn[0] == sManual[0] && sFn[1] == sManual[1],
		      "the generic engine's spread roll consumes exactly one draw -- no internal threshold roll");
	}

	// End-to-end determinism check on the composed roll: same seed, same
	// baseline/resistance in, same neighbour delta and spread decision out.
	{
		Xorshift128Plus<uint32_t> rngA(99);
		Xorshift128Plus<uint32_t> rngB(99);
		bool spreadsA = false;
		bool spreadsB = false;
		const auto deltaA =
		    BattleHazard::rollFireSpreadNeighbour(rngA, /*baseline=*/2, /*resistance=*/1, spreadsA);
		const auto deltaB =
		    BattleHazard::rollFireSpreadNeighbour(rngB, /*baseline=*/2, /*resistance=*/1, spreadsB);
		check(deltaA == deltaB, "identical seed/args reproduce the identical neighbour delta");
		check(spreadsA == spreadsB, "identical seed/args reproduce the identical spread decision");
	}
}

} // namespace

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	// Section 1: scheduler state and cadence boundary (OPE-17)
	test_fire_overlay_encode_decode();
	test_fire_overlay_cadence_boundary_is_inert_by_default();

	// Section 2: overlay power table (OPE-15, parameterized subset)
	test_fire_overlay_power_table();
	test_fire_overlay_advance_and_extinguish();

	// Section 3: recovered spread RNG primitives (OPE-22)
	test_hazard_roll_inclusive_bounds();
	test_fire_spread_resistance_gate();
	test_neighbour_tables();
	test_spread_roll_draw_count_and_order();

	if (failureCount > 0)
	{
		LogError("{0} check(s) failed", failureCount);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
