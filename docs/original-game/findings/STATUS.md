# Parity implementation — run status

Branch: `khallmark/parity-implementation`, cut from `develop` @ `26a72467`.

## Baseline (before any parity change)

- `cmake --build build -j` — **green**, extract-data ran clean.
- `ctest --test-dir build` — **30/30 passed, 12.97 s.**

Any parity change must keep all 30 green. A new lock test must **fail before** its change and
**pass after**; a lock test that passes on arrival proves nothing (see
[../parity-guide.md](../parity-guide.md#1-workflow-a--closing-a-code-only-gap)).

## In flight

**R&D / decompilation** — writes to `findings/`, no `game/` edits:

| Agent | Items | Deliverable |
|---|---|---|
| RE-cover | B1 cover metric | `B1-cover-metric.md` |
| RE-hazard | B5 enzyme, F1 fire remainder, K1 cloak | `B5-F1-K1-hazards.md` |
| RE-wounds | B3 wounded penalty, G1 dead gadgets | `B3-G1-wounds-gadgets.md` |
| RE-city | O1 bribe/rift, O2 cargo seize, M1 city music | `O1-O2-M1-city.md` |
| RE-incursion | U1 mission counter, U2 base exposure, V1 vehicle dodge | `U1-U2-V1-incursion.md` |

**Code** — isolated worktrees, merged back after review:

| Agent | Items | Deliverable |
|---|---|---|
| Code-locks | A2 psionics, A3 TU reservation, A4 attack priority | three lock tests |
| Code-largeunits | A1 multi-tile units | breakage list + fixes + `test_battle_large_unit` |
| Code-groundveh | V2 ground-vehicle order defect | `test_ground_vehicle_path` |
| Code-briefings | **New row** — ten TACP alien-building briefings unextracted | extractor + `test_city_rules` cases |

## Method correction issued mid-run

A `NOT BOUND` verdict on **B1 (cover metric)** rested on zero xrefs for `Cautious mode` et al,
validated against `senator` as a known-positive control. **The control was invalid** — `senator` is
in TACP's asset-name table (fixed 0x2E stride, directly referenced); every B1 anchor is in the
packed message pool (`0x2DE000`–`0x2E2FFF`), whose entries are reached by index and can never carry
a direct xref. See [METHOD-tacp-string-regions.md](METHOD-tacp-string-regions.md).

All three TACP agents were sent the correction and told to enter structurally instead. **RE-cover
was resumed** rather than accepted. New rule for negatives: a `NOT BOUND` must name the structural
method exhausted, not cite absent xrefs.

Unchased lead surfaced by the mapping: **`TU cost per wound: ` @ `0x2E0204`** — direct evidence the
original modelled a per-wound TU cost (parity item B3), handed to RE-wounds.

## Done in this run

- **C1 umbilical — closed.** `umbilical` is absent from *both* binaries, not just UFO2P.
- **C4 city-wide Apocalypse attack — closed.** All 8 `apocalypse` hits across both binaries are UI
  or title copy; none names an event.
- **C3 late-campaign bombing — closed** pending an incidental find. The escalation is already
  explained by the weekly growth and mission-preference tables.
- **C2 mushrooms — RECLASSIFIED Class C → Class B.** The matrix searched only UFO2P; TACP has the
  string at file `0x2E1468`. It is a **battlescape objective**, not cityscape feedback, and the row
  was filed under the wrong subsystem. See
  [C1-C4-no-evidence-items.md](C1-C4-no-evidence-items.md).

## Rule for integrating R&D

A `findings/` verdict of `NOT BOUND` **closes** its parity row as a recorded negative result. It
does **not** license implementing the feature from a plausible guess. Only `BOUND` verdicts may
become constants in `game/`.
