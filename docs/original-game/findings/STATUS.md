# Parity implementation — run status

Branch: `khallmark/parity-implementation`, cut from `develop` @ `26a72467`.

## Baseline (before any parity change)

- `cmake --build build -j` — **green**, extract-data ran clean.
- `ctest --test-dir build` — **30/30 passed, 12.97 s.**

## Final — 48 commits, suite 36/36 (baseline was 30/30)

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

## Third pass — the RE that was still open

Six agents were sent at the rows the second pass left blocked. Five returned negatives, and the
negatives were the point: they close rows that would otherwise be re-opened blind, and two of them
overturned claims this project had already committed.

| Row | Outcome |
|---|---|
| **K1** cloak | **CLOSED — negative.** All 19 readers of `agent_general_data`'s type field enumerated. Three recognise `0x0a`; none implements concealment. Decisive: `FUN_00066474`, the tick dispatcher hosting Mind Shield and Disruptor Shield cadences, has **no `0x0a` case at either hand slot** — there is no tick site to host a threshold. `CLOAK_TICKS_REQUIRED_UNIT` is forum folklore, now labelled as such at the constant. |
| **C2** secondary objectives | **CLOSED — triple negative.** Queen live capture is generic (all 13 species share the pair). Sectoid rescue → Mutant alliance has no consumer: `Sectoid` appears exactly once in all of TACP, in the briefing prose. The evacuation phase does not exist — the "units will be lost" string's pool neighbours are `Abort mission?` / `Yes` / `No`. |
| **B5** enzyme | **NOT BOUND, but properly.** Types 1 and 3 are treated identically by every consumer in the binary — 11 functions, 17 call sites. The armour-vs-health asymmetry that would discriminate them does not exist anywhere in the overlay code. |
| **A2** psi upkeep | **CLOSED — negative.** No literal table in 3.17 MB, all 24 permutations. Strings unreachable four independent ways, validated against a live control. Panic stays at `3`; locking the test to the code was right. |
| **U1(b)** | **OVERTURNED.** The earlier "gate can never fire" reading was wrong on both halves — constitution does take damage and is repaired, and `+0x168` is recomputed at runtime. |
| **U1(a)** arrived flag | **IMPLEMENTED.** See below. |

## The one that changed shipped behaviour

`+0x12C == 1` is a **structural invariant** for incursion-spawned UFOs: `FUN_0006da88` reclobbers `BX`
with a literal `1` immediately before the writer, and two independent exhaustive censuses found nothing
that touches the field on an already-spawned vehicle. The retarget branch is therefore *unreachable*
for that population — they always leave via the nearest dimension gate. OpenApoc retargeted them
unconditionally.

Two prior findings disagreed about this and **both were partly wrong**; it was settled by a referee
working inside the Ghidra project, not by preferring one document. The retarget branch is real code
in the original, for a different population entirely (a periodic scheduler unrelated to the incursion
system). So the fix is a **split, not a replacement**: gated on owner, with the three existing retarget
tests left exactly as they were. `OrganisationRaid::UnauthorizedVehicle` is deliberately untouched —
whether it is that second population is not established, and guessing is the failure this row already
made once.

## An eighth claim overturned, and a method retired

Attempting to arbitrate that reversal by reading raw bytes at a cited file offset produced "no match".
Running the same check against a **control** — a known-good, already-committed citation from a
different agent — *also* produced "no match", with the real encoding sitting `0x726A5` bytes away.

These are bound Linear Executables behind the LX loader; object pages are not contiguous in the file,
so `VA − 0x10001` is a within-page convention, not a file-wide map. **The method yields confident
false negatives on correct citations.** Had the control been skipped, two agents would have been
"caught" in errors they did not make. Arbitration happens inside the Ghidra project or not at all.

## The harness, and what four campaign runs actually proved

The visual campaign driver was run against this branch. Every failure was **driver-side**; the engine
itself did not misbehave once across four launches. Three harness defects fixed:

- `respond_to_event()` returned `True` after trying every key whether or not the stage moved, so a
  dead key looked like success and the caller spun forever — silently, because the log line only fires
  on a stage *change*.
- `run_clock()`'s stall detector lived on the CityView branch only, so anything it could not dismiss
  spun mutely until the leg budget expired.
- Two screens (`UfopaediaCategoryView`, `ResearchScreen`) had no dismissal policy while sitting in
  `WORKING_STAGES`, which is right for a deliberate visit and wrong for an accidental arrival.

The second fix immediately corrected the first diagnosis: the strand was `ResearchScreen`, not the
UFOpaedia, which had been fixed and credited on no evidence.

Also found live: the driver hand-rolls three mechanisms to locate the action, while `BUTTON_ZOOM_EVENT`
→ `zoomAt()` does `setZLevel(location.z + 1)` — the engine changes floor for you. And the
`Notifications.*` flags the driver disables only gate the blocking popup; `logEvent` and the ticker
fire unconditionally, so it had switched off the alarm and then gone looking for the fire.

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
The one closed row whose wording did not name its method was **U2(a)** (`DAT_000e0cc0`) — a closed
negative over a `DAT_` global, precisely the shape this bug hits. **Re-audited with
`getReferencesTo` and the row stands:** exactly 6 references, matching the original enumeration
address-for-address, both writes re-confirmed as literal zero and the third site as an unmodified
round-trip. That finding was never exposed to the bug. No other row needed re-checking.

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

