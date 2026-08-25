# Parity implementation — run status

Branch: `khallmark/parity-implementation`, cut from `develop` @ `26a72467`.

## Baseline (before any parity change)

- `cmake --build build -j` — **green**, extract-data ran clean.
- `ctest --test-dir build` — **30/30 passed, 12.97 s.**

## Final — 28 commits, suite 36/36 (baseline was 30/30)

### Implemented, each with a lock test verified to fail before the change

| Row | |
|---|---|
| **V2** | ground vehicles no longer destroyed for merely failing to reach a target |
| **A1** | large-unit occupancy and line-of-sight geometry |
| **C2** | ten alien-building briefings extracted and shown pre-battle |
| **U1(a)** | UFO mission-counter zero-transition, live in real incursion spawns |
| **G1** | Disruptor Shield absorption corrected (was infinite, never overflowed) |
| **F1** | recovered hazard-spread primitives; `HAZARD_SPREAD_CHANCE` macro and FIXME gone |
| **A2/A3/A4** | psionics, TU reservation and AI weapon priority frozen |

### Closed as genuine negatives — evidence absent, not effort

B1 cover · B3 wounded penalty · O1 bribe/rift · M1 city music · V1 engagement table ·
U2(a) `DAT_000e0cc0` · C1 umbilical · C3 late bombing · C4 Apocalypse attack ·
Vortex Analyzer, Structure Probe, Alien Detector (dead in the original too)

### Attempted and correctly abandoned

**U2(b)** — bound in the binary's control flow, but no honest seam in OpenApoc: the formula is
already shipped, the moved-count source has no counterpart in our data model, and the trigger point
was never traced. No code written.

### Still open, honestly

B5 type 1-vs-3 · K1 (20 candidate functions unexamined) · U1(b) field semantics ·
B2/B4 (depend on B1) · MultiTracker's downstream meaning

## Second pass — the loose ends

Four rows this branch had already marked done were re-examined. Three of them were not.

| Row | What the second pass found |
|---|---|
| **A2** psionics | `psi_costs_match_prior_art` asserted local constants against themselves and could never fail. Root cause was not the test: `getPsiCost()` was declared `static` at namespace scope in `battleunit.h`, giving it internal linkage in every TU that saw the header while the only definition sat in `battleunit.cpp` — a latent undefined-reference for any caller anywhere. Promoted with its sibling `getPsiAttackChance()` to public `BattleUnit` statics. |
| **A4** attack priority | Froze local copies of `cth*damage/time` and of the AOE friendly-fire rule; editing `unitaivanilla.cpp` left it green. Three primitives extracted to public `UnitAIVanilla` statics and called from the same private methods. |
| **U1(a)** mission counter | Three tests drove `advanceMissionCounterOnArrival()` directly; the call site in `VehicleMission::start()` was untested and deleting it failed nothing. New case drives the real `start()`. |
| **G1** Disruptor Shield | Only one of the **two** bound writers of the item's charge field was implemented. `FUN_0006508C`'s decrement on absorb was in; `FUN_0006511C`'s +1-per-dispatcher-call regen was not, so the item charge and the unit buffer drifted apart. Separately, `AEquipment::updateInner()` was applying a *second* regen to the same field at one-per-four-seconds, from hardcoded 2016 extractor literals. |

All four locks verified red by mutation before being committed.

## One test written and then discarded

A1's draw-order step got a candidate lock test for the block-confinement invariant. Two
independent control mutations — collapsing the large-unit bounds to `{1,1}`, and moving its
`centerOffset.z` from `1.0` to `0.5` — both left it **green**. At integer positions the half-extent
straddles the tile boundary either way, and "exactly one draw tile, inside the block" is structural
because `addToDrawnTiles()` can only choose from `intersectingTiles`. It locked nothing, so under
§0 it was not committed. The row stays UNVERIFIED and the guide now says why.

## A seventh claim overturned — and this one has blast radius

The B5 follow-up found that `scripts/QueryDataRange.java` in the research lab **structurally
cannot see an entire class of x86 reference**: it guards its match loop with `instanceof Scalar`,
but a direct absolute-memory operand (`MOV byte ptr [0x3009a0],CH`) is an `Address`-typed operand
object in Ghidra's model, not a `Scalar`. Every "zero literal-operand hits" negative that script
produced undercounts, on any address.

Re-run against `DAT_003009a0` with `getReferencesTo`, the supposedly-unreferenced global has
**six** references — three writers and three readers — and three of the four functions involved had
never been examined.

**Use `getReferencesTo(Address)` for a definitive xref check on a global. Never a `Scalar`-operand
sweep.**

Audit of every other "zero xrefs" negative in this folder: B1's table check, O1's diplomacy-string
check and the METHOD docs' string checks all used `getReferencesTo` already and are unaffected.
The one closed row whose wording does not name its method is **U2(a)** (`DAT_000e0cc0`, "across all
6 xrefs to this address in the whole binary, no instruction sets it to a nonzero value") — a closed
negative over a `DAT_` global, which is precisely the shape this bug hits. Re-verification
requested; until it lands, treat U2(a) as provisional rather than closed.

## Six claims overturned by measurement during this run

Four of them mine.

| Claim | Reality |
|---|---|
| "Pool strings can never carry a direct xref" *(mine)* | Falsified by a live pointer table serving ~30 pool strings |
| "BOUND means implementable" *(mine)* | U2(b) was bound and had no seam here |
| "The Disruptor Shield is inert" *(mine)* | Wired since 2016, and wrong — infinite absorption |
| "Preserve the dead `(0,0,0)` outcome" *(mine)* | The byte reads `1`. The guard was protecting a typo |
| "Mind Shield's citation doesn't resolve" | Sound binding; `FUN_*` addresses drift between imports |
| "The RNG is a precomputed table" | Reads as static zero, BSS-shaped, filled at runtime |

Every one was caught by an agent checking rather than building to the brief. That is the only
reason this branch is worth merging.

