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
	// TACP tile overlay: top two bits are type (2 = fire), low six bits index the
	// 27-byte fire power table. Zero is legacy/uninitialized; encoded stage zero
	// is 0x80. Separate from age, which still drives legacy visuals/scheduling.
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

	static uint8_t encodeFireOverlay(unsigned stage);
	static int fireOverlayStage(uint8_t overlay);
	static int fireOverlayPower(const std::vector<int> &powerTable, uint8_t overlay);
	static bool advanceFireOverlay(const std::vector<int> &powerTable, uint8_t &overlay);
	bool advanceOriginalFireOverlay(GameState &state);
	void applyOriginalFireItemEffect(GameState &state);

	// -- Recovered hazard placement/spread RNG -----------------------------
	// TACP FUN_0001eee8 (RNG primitive) / FUN_0007b0d0 (fire spread) /
	// FUN_0007ae78 (generalized placement engine). See
	// docs/original-game/findings/B5-F1-K1-hazards.md §2 for the full
	// derivation; comments below cite section numbers from that document.
	// TWO CORRECTIONS to that document were made while implementing this,
	// both re-verified directly against the bound `OpenApocOG_TACP` Ghidra
	// project (non-4 TACP.EXE, CRC32 0xfebbe39e, confirmed this session) --
	// see the two function-level comments below for details and the
	// re-verification steps taken. Whoever owns that finding should amend it;
	// this file was not edited to match, only implemented from the corrected
	// evidence.
	//
	// This ports the recovered *shape* of the mechanism: inclusive uniform
	// RNG draws with the recovered spans, the recovered call order (fire
	// rolls a threshold then a neighbour; the generic engine rolls only a
	// neighbour), the recovered neighbour tables, and the recovered
	// comparison direction. It does NOT wire live spread decisions: the
	// per-terrain resistance byte values (FUN_0007aa8c) were never decoded,
	// fire's "inherited baseline" was never pinned down, the generic engine's
	// threshold source was never traced, and no finding confirms these map
	// onto OpenApoc's fire_resist/block fields. Callers must supply
	// resistance/baseline explicitly; nothing here reads a BattleMapPart.
	static constexpr int FIRE_SPREAD_THRESHOLD_RNG_SPAN = 10;   // FUN_0001eee8(10)
	static constexpr int FIRE_SPREAD_NEIGHBOUR_RNG_SPAN = 4;    // FUN_0001eee8(4)
	static constexpr int GENERIC_SPREAD_NEIGHBOUR_RNG_SPAN = 5; // FUN_0001eee8(5)

	// TACP FUN_0001eee8 @ non-4 file 0x7998C (§2.1): uniform draw on
	// [0, max] inclusive.
	//
	// CORRECTION to B5-F1-K1-hazards.md §2.1: the finding characterizes the
	// primitive's source as "a persistent, precomputed 10,013-entry lookup
	// table" baked into the binary. Direct inspection of the bound project
	// (QueryHazardRngTable.java, VA 0x1B2D70, 10,013 x 2 bytes) shows that
	// entire region reads as static zero in the file -- not a precomputed
	// table. The containing `.object2` block is file-backed and initialized
	// (confirmed non-zero real content elsewhere, e.g. embedded strings 64
	// bytes below the table base, and the neighbour table below), so this is
	// not a block-mapping error; the RNG table specifically is BSS-shaped,
	// meaning it is written by a fill routine at runtime that this session
	// did not locate (consistent with a signature search on its first bytes
	// returning ambiguous, as an all-zero window would). Reproducing the
	// original's literal output sequence would require RE'ing that fill
	// routine -- new work outside this task's "the RE is done" scope, and
	// outside B5-F1-K1-hazards.md's own evidence.
	//
	// Per this fork's prime directive (parity-guide.md §0), an unrecovered
	// constant/table must not be invented. What IS recovered and
	// behaviourally load-bearing is the primitive's contract -- an inclusive
	// uniform draw -- which this reproduces using GameState's own
	// already-serialized RNG stream. This is a deliberate, reported
	// substitution for an irreproducible table, not an invented replacement
	// for a bound one: there is no table to embed and no cursor to
	// serialize, since none of that data exists statically to embed.
	static int hazardRoll(Xorshift128Plus<uint32_t> &rng, int max);

	// TACP FUN_0007b0d0 (§2.2): spread succeeds only when resistance is
	// strictly less than the rolled threshold (JNC = no-spread when
	// resistance >= threshold).
	static bool fireSpreadResistanceGate(int resistanceValue, int rolledThreshold);

	// TACP FUN_0007b0d0 neighbour table (§2.2/§2.3): outcome in [0,4] maps to
	// (X,Y,Z) via iVar6 = outcome*3+1 into the int32[] table based at
	// 0x293068 (X), 0x29306C (Y, +carry), 0x293070 (Z, pre-dereference) --
	// confirmed directly from the disassembly listing this session
	// (QueryFunctions.java 0x7b0d0): `MOV ESI,[ECX*4+0x293068]` /
	// `MOV EDI,[ECX*4+0x29306C]` / `MOV EAX,[ECX*4+0x293070]`, matching
	// B5-F1-K1-hazards.md §2.2's own listing citation exactly.
	//
	// CORRECTION to B5-F1-K1-hazards.md §2.3: the finding's table reports
	// outcome 0 (iVar6=1, reading 0x29306C/0x293070/0x293074) as (0,0,0), a
	// "dead" outcome, and frames that as F1's central open question. Direct,
	// twice-independent re-reads of those exact bytes this session
	// (QueryNeighbourTableFull.java) give 0x29306C = 1, not 0 -- i.e. outcome
	// 0 is (1,0,0), East, not dead. Every other value in both the fire and
	// the six-outcome generic table (§2.4) matched the finding exactly (19
	// consecutive int32 reads cross-checked), and the addressing formula was
	// independently re-confirmed against the live listing above -- so this
	// is isolated to one transcribed value, not a base-address or build
	// mismatch (CRC32 of canonical/TACP.EXE re-verified as 0xfebbe39e, the
	// finding's own cited non-4 build, this session).
	//
	// Fire's five outcomes are therefore East/South/West/North/Up -- a clean
	// compass+up subset of the generic six-outcome table (§2.4), omitting
	// only Down. There is no dead outcome. Do not reintroduce one; do not
	// re-derive this table from the finding document without re-reading the
	// bytes, since that document's transcription is the thing that was
	// wrong here.
	static Vec3<int> fireSpreadNeighbourDelta(int outcome);

	// TACP FUN_0007ae78 neighbour table (§2.4): outcome in [0,5], the same
	// underlying table continuing where fire's addressing tops out
	// (0x2930A4 = 0x293068 + 0x3C). All six outcomes are live: East / South /
	// West / North / Up / Down. This table's bytes matched
	// B5-F1-K1-hazards.md §2.4 exactly on direct re-read (all 19 consecutive
	// int32 reads across 0x2930A4-0x2930EC) -- it is fire's table (§2.3
	// above) that had the transcription error, not the addressing mechanism
	// or this table.
	static Vec3<int> genericSpreadNeighbourDelta(int outcome);

	// TACP FUN_0007b0d0 (§2.2, call sites 0x7B0E1 then 0x7B0F3): rolls the
	// resistance-gate threshold (RNG(0..10) + baseline) then the 5-way
	// neighbour pick, in that order -- two draws from the shared RNG per
	// invocation. `baseline` and `resistance` are caller-supplied (see the
	// class comment above); this performs no lookup of either.
	static Vec3<int> rollFireSpreadNeighbour(Xorshift128Plus<uint32_t> &rng, int baseline,
	                                         int resistance, bool &spreads);

	// TACP FUN_0007ae78 (§2.4 correction): the generalized engine calls the
	// RNG primitive exactly ONCE -- only the 6-way neighbour pick -- because
	// its resistance-gate threshold arrives as a caller-supplied parameter
	// rather than an internally-rolled RNG(0..10). Do not add a second draw
	// here to "match" fire; the finding independently ruled that out for
	// this function.
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

The visual age model below remains observational. Recovered type-2 overlays use
the serialized Battle global row/contact scheduler in real-time and the
400-iteration end-of-round batch in turn-based mode. Unit fire intensity remains
to be bound.

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
FUN_0007b3dc destroys terrain when the six-bit fire overlay stage reaches
fire_burn_time. Neighbour spread/resistance remains approximate until FUN_0007b0d0 is bound.
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
