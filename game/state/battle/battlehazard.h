#pragma once

#include "game/state/battle/battle.h"
#include "game/state/gametime.h"
#include "game/state/stateobject.h"
#include "library/sp.h"
#include "library/vec.h"
#include "library/xorshift.h"
#include <cstdint>
#include <vector>

#define HAZARD_FRAME_COUNT 3

// -- Read about fire spread formula below the class declaration ---

namespace OpenApoc
{
static const unsigned TICKS_PER_HAZARD_UPDATE = TICKS_PER_TURN / 2;

class TileObjectBattleHazard;
class DamageType;
class HazardType;
class GameState;
class Organisation;
class BattleUnit;
class TileMap;

class BattleHazard : public std::enable_shared_from_this<BattleHazard>
{
  public:
	static constexpr uint8_t FIRE_OVERLAY_TYPE = 0x80;
	static constexpr uint8_t FIRE_OVERLAY_INDEX_MASK = 0x3f;

	Vec3<float> getPosition() const { return this->position; }

	Vec3<float> position;
	StateRef<DamageType> damageType;
	StateRef<HazardType> hazardType;
	// Power (damage) for most hazards, growing/fading flag for fire
	int power = 0;
	// TTL for most hazards, Meaningless for fire
	unsigned lifetime = 0;
	// Time already lived for most hazards, stage for fire
	unsigned age = 0;
	// Recovered TACP tile overlay byte: top two bits are the overlay type (2 =
	// fire), low six bits index the fire-power-by-stage table. Zero means
	// legacy/uninitialized; encoded stage zero is 0x80, so zero is
	// distinguishable from "fire at stage 0". This is orthogonal to `age`,
	// which continues to drive the pre-existing (non-overlay) visuals and
	// per-hazard scheduling below -- nothing in this file currently writes a
	// non-zero value here (the writer is the recovered Battle-level fire
	// scheduler, tracked separately and out of this extraction's scope), so
	// this field and the cadence boundary that reads it are inert until that
	// scheduler lands, by design.
	uint8_t fireOverlay = 0;
	unsigned int frame = 0;
	unsigned ticksUntilVisible = 0;
	unsigned frameChangeTicksAccumulated = 0;
	unsigned nextUpdateTicksAccumulated = 0;
	StateRef<Organisation> ownerOrganisation;
	StateRef<BattleUnit> ownerUnit;

	bool expand(GameState &state, const TileMap &map, const Vec3<int> &to, unsigned ttl,
	            bool fireSmoke = false);
	void grow(GameState &state);
	void applyEffect(GameState &state);
	void die(GameState &state, bool violently);
	void dieAndRemove(GameState &state, bool violently);
	void updateTileVisionBlock(GameState &state);

	// -- Recovered fire overlay state (scheduler cadence boundary) ---------
	// Encode/decode helpers for `fireOverlay` above and the power-by-stage
	// lookup it indexes into. `powerTable` is caller-supplied (the extracted
	// fire-power-by-stage table itself lives on GameState, out of this file's
	// scope) so these stay pure functions of their arguments.
	static uint8_t encodeFireOverlay(unsigned stage);
	static int fireOverlayStage(uint8_t overlay);
	static int fireOverlayPower(const std::vector<int> &powerTable, uint8_t overlay);
	static bool advanceFireOverlay(const std::vector<int> &powerTable, uint8_t &overlay);

	// -- Recovered hazard placement/spread RNG -----------------------------
	// Ported from this fork's TACP.EXE reverse-engineering pass covering the
	// hazard RNG primitive, fire's spread roll, and the generalized
	// placement engine that a non-fire hazard would use. This lands the
	// recovered *shape* of the mechanism only: inclusive uniform RNG draws
	// with the recovered spans, the recovered call order (fire rolls a
	// threshold then a neighbour; the generic engine rolls only a
	// neighbour), the recovered neighbour tables, and the recovered
	// comparison direction (resistance strictly less than threshold spreads).
	//
	// It does NOT wire live spread decisions. Three things the finding work
	// traced further are still not callable from here without changes
	// outside this file's unit:
	//   * the per-terrain resistance operand resolves to
	//     BattleMapPartType::block[DamageType::BlockType::Fire], already on
	//     GameState's map part types;
	//   * fire's inherited baseline resolves to the same fire-power-by-stage
	//     table `fireOverlayPower` above reads, but that table lives on
	//     GameState, not in this file;
	//   * the invocation cadence (the rate these rolls happen at) is a
	//     Battle-level scheduler question, not a BattleHazard one, and is
	//     tracked separately from this extraction.
	// Wiring geometry and thresholds while that cadence stays unbound would
	// change fire's spread RATE while looking like a faithful port, so these
	// primitives are deliberately left uncalled by gameplay: BattleHazard::
	// grow() below still uses the pre-existing unconditional-8-neighbour
	// expand() sweep with the legacy spread-chance gate, unchanged.
	static constexpr int FIRE_SPREAD_THRESHOLD_RNG_SPAN = 10;
	static constexpr int FIRE_SPREAD_NEIGHBOUR_RNG_SPAN = 4;
	static constexpr int GENERIC_SPREAD_NEIGHBOUR_RNG_SPAN = 5;

	// Recovered RNG primitive: uniform draw on [0, max] inclusive, reproduced
	// against GameState's own already-serialized RNG stream. The original's
	// literal table-driven output sequence is not reproduced here -- that
	// table's on-disk region reads as uninitialized/BSS-shaped rather than a
	// baked constant, so nothing concrete exists to embed; only the
	// primitive's recovered *contract* (inclusive uniform draw) is ported.
	static int hazardRoll(Xorshift128Plus<uint32_t> &rng, int max);

	// Recovered comparison: spread succeeds only when resistance is strictly
	// less than the rolled threshold.
	static bool fireSpreadResistanceGate(int resistanceValue, int rolledThreshold);

	// Recovered 5-outcome neighbour table for fire's spread roll: East,
	// South, West, North, Up. There is no "dead" (0,0,0) outcome in this
	// table -- do not reintroduce one.
	static Vec3<int> fireSpreadNeighbourDelta(int outcome);

	// Recovered 6-outcome neighbour table for the generalized placement
	// engine: the same compass+up set as fire's table, plus Down.
	static Vec3<int> genericSpreadNeighbourDelta(int outcome);

	// Recovered fire spread roll: threshold (RNG(0..10) + baseline) then the
	// 5-way neighbour pick, in that order -- two draws from the shared RNG
	// per invocation. `baseline` and `resistance` are caller-supplied; this
	// performs no lookup of either.
	static Vec3<int> rollFireSpreadNeighbour(Xorshift128Plus<uint32_t> &rng, int baseline,
	                                         int resistance, bool &spreads);

	// Recovered generalized placement engine roll: exactly ONE draw (the
	// 6-way neighbour pick). Its resistance-gate threshold arrives as a
	// caller-supplied parameter rather than an internally-rolled RNG(0..10),
	// so do not add a second draw here to "match" fire's roll.
	static Vec3<int> rollGenericSpreadNeighbour(Xorshift128Plus<uint32_t> &rng);

	// these return true if this hazard has died and needs to be deleted
	bool update(GameState &state, unsigned int ticks);
	bool updateInner(GameState &state, unsigned int ticks);
	bool updateTB(GameState &state);

	BattleHazard() = default;
	BattleHazard(GameState &state, StateRef<DamageType> damageType, bool delayVisibility = true);
	~BattleHazard() = default;

	// Following members are not serialized, but rather are set up in the initBattle method

	sp<TileObjectBattleHazard> tileObject;
};
} // namespace OpenApoc

/*
// Alexey Andronov (Istrebitel):

Okay, so I was trying to figure out how fire works, and I've studied videos and found out
that a very weird formula fits!

Fire in the game starts small, gradually enlarges, then rages for a bit, and then dies out.
We have 12 frames for fire. Let's number them 0 to 11 where 0 is full force fire.

Let "Stage" be the value that controls the fire's current state, and what frame we use.

The model below remains observational and is unaffected by this file's `fireOverlay`
field: that field and the cadence boundary that reads it exist to make room for a
recovered type-2 tile overlay (a separate stage/power representation from the
Battle-level fire scheduler), but nothing in this file writes a non-zero overlay yet,
so today every hazard still runs the model below exactly as before.

When fire is applied (Incendiary missile or grenade), Stage = 10 - random[0-2] * 0,6
(here [] are inclusive brackets, as in widely accepted math notation)

Each 2 seconds Stage is decreased by 0,6 until it reaches 1.
After it reached one, each 2 seconds Stage is increased by 1 until i reaches 11.
After it reached 11, next time 2 seconds pass fire is extinguished completely.

What this gives us is a progression that looks like this:
    10 - 9,4 - 8,8 - 8,2 - 7,6 - 7,0 - ... 1,6 - 1 - 2 - 3 - ... - 10 - 11 - extinguished
   (^ start here ^)

Now, if we then round this value to nearest 0,5 we get progression that looks like this:
    10 - 9,5 - 9 - 8 - 7,5 - 7 - 6,5 ....

Now, if we add 0,5 and round up, or subtract 0,5 and round down, we get the possible frames
that the fire can show at every stage! One extra rule: frame 11 is reserved for dying flames

We get the following progression:

    10-9 - 10-9 - 10-8 - 9-7 - 8-7 - 8-6 - 7-6 - ...

Which is exactly how it appears to work in the game!

-- Fire spread --

Now, fire can spread to an adjacent flammable object (only feature or ground).
When it does, it starts burning from 10, as usual.
When it reaches past Stage 6 (value of 5,8) that's when object's "time to burn" (#9) timer starts
When this timer ends, object is destroyed.
Condition for spreading is the same - fire that reached past Stage 6 can spread.
At least for fire resist 25.
I don't know how it works for other resists so I will cheat and fake it

Assume 255 = immune
Otherwise "power" of current flame is 2 ^ (10 - Stage) * 3/2
Therefore:
- Stage 9 can penetrate 3
- Stage 8 can penetrate 6
- Stage 7 can penetrate 12
- Stage 6 can penetrate 24
- Stage 5 can penetrate 48
- Stage 4 can penetrate 96
- Stage 3 can penetrate 192
- Stage 2 can penetrate up to 254

Fire also never spreads to another fire

Smoke cannot extinguish fire that is burning a scenery, but can extinguish fire that is burning a
ground.
Walls are apparently immune to fire.

*/
