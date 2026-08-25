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

