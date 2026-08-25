# B1 · Cautious / Normal cover + potshots — cover-tile metric

**NOT BOUND: no cover-tile scoring function found via string anchoring, pool pointer-table
entry, or unit-adjacent-function entry — all three checked. `UnitAIHelper::getTakeCoverMovement`
must keep returning `nullptr`.**

This closes the RE half of B1 with a documented negative result, per the parity-guide prime
directive: a recorded negative is a successful outcome, and inventing a "move behind the nearest
wall" heuristic to close the row would be a regression.

**Revision note.** A first pass of this document (same file, same verdict) shipped with an
invalid control: it validated the xref-scanning instrument against an address borrowed from an
external cross-project export without re-verifying it live, and that address turned out not to
hold the string it was labelled with. A review caught this and sent the investigation back. The
redo below keeps all of the original negative evidence (still correct), replaces the invalid
control with real, project-local positive controls, and adds two structural (non-string) entry
attempts the review asked for. The verdict is unchanged, but it is now resting on solid ground —
the in-pool positive controls found during the redo are *better* evidence for the negative than
anything in the first pass, not weaker (§2.3).

**Cross-cutting note.** This same packed-pool access problem blocks four other rows —
B3 wounded penalty, K1 cloak, G1 gadgets, B5 enzyme — and its UFO2P analogue blocks O1. See
[METHOD-tacp-string-regions.md](METHOD-tacp-string-regions.md) for the consolidated method note
(landed by a concurrent investigation into this shared lab while this document was being redone).
That note's claim that packed-pool strings "can never carry a direct code xref" is corrected there
using the evidence in §2.3–§2.4 below: some pool strings, including a category-matched sibling of
the B1 anchors, demonstrably do.

---

## 0. Binary confirmed

`OpenApoc-og-research/canonical/TACP.EXE`: size 3,170,298 bytes, CRC32 `0xfebbe39e` — matches the
extractor-canonical non-4 ISO build cited in
[binaries/tacp.md](../binaries/tacp.md). Ghidra project: `ghidra_projects/OpenApocOG_TACP.rep`
(pre-imported, pre-analysed; 1595 functions, `.object1` code `0x10000`–`0xD44FE`, `.object2` data
`0xE0000`–`0x33F0AF`).

## 1. Strings confirmed, both generations

All eight candidate strings (seven from the task plus one missed on the first pass — see below)
exist verbatim in TACP, at the exact `−0x2200` non-4→4 slide documented in
[binaries/tacp.md](../binaries/tacp.md) (no exception, unlike `crew_ufo_downed`):

| String | non-4 file offset | 4-build file offset | Δ |
|---|---|---|---|
| `Cautious mode` | `0x2DFE51` | `0x2DDC51` | `−0x2200` ✓ |
| `Aggressive mode` | `0x2DFE5F` | `0x2DDC5F` | `−0x2200` ✓ |
| `Kneel down` | `0x2DFEAE` | `0x2DDCAE` | `−0x2200` ✓ |
| `Unit under fire: ` (with colon) | `0x2DF2B8` | `0x2DD0B8` | `−0x2200` ✓ |
| `Unit under fire` (no colon) | `0x2E0438` | `0x2DE238` | `−0x2200` ✓ |
| `Unit has gone berserk: ` (two `r`, with colon) | `0x2DF134` | `0x2DCF34` | `−0x2200` ✓ |
| `Unit has gone beserk` (**one `r`**, no colon — missed on first pass) | `0x2E048E` | — (not re-checked) | — |
| `Reserve TUs for kneel` | `0x2E01C4` | `0x2DDFC4` | `−0x2200` ✓ |

The one-`r` spelling is a second, distinct string entry, not a typo in this document — TACP
genuinely ships both `berserk` (with colon, earlier in the file) and `beserk` (no colon, later,
inside the UI-options pool). The first pass of this investigation searched only for `berserk`
(two `r`) and missed it; a reviewer caught the gap. Both spellings were re-checked below and both
are unreferenced.

Source: `export/strings/TACP_strings.txt` / `TACP4_strings.txt` (pre-dumped, offset-labelled).
`potshot` and `evasive`: **zero hits in TACP, either generation, any case.** `cover` as a whole
word: **zero hits** — every raw match is a substring of `Recover`/`discover`/`recovery` inside
mission-briefing prose (e.g. file `0x2E0523`, `0x2E0779`, `0x2E1E59`), none of them the tactical
term.

The 4-build project (`TACP4.EXE`) is not imported in this lab (only `OpenApocOG_TACP.rep` for
non-4 exists), so all xref/pointer/structural checks below ran on non-4 only. The exact,
exception-free slide match is treated as sufficient confirmation the 4-build carries the same text.

## 2. Xref walk and structural entry — three independent methods, all negative

### 2.1 Method A — Ghidra's symbolic reference table

`getReferencesTo` on each string's data address (this is what "bound xrefs" means in this lab).
Reused the prior run of `scripts/QueryTacpGaps.java` (already present in the lab; log dated
23 Aug, `export/tacp_gaps.log` / `tacp_gaps_stdout.txt`), which already covered `Cautious mode`,
`Aggressive mode`, `Kneel down`, `cover`, `potshot`, `evasive`, and added
`scripts/QueryB1CoverStrings.java` / `scripts/QueryB1ControlRedo.java` (this session) for
`Unit under fire`, `Unit has gone berserk` / `beserk`, `Reserve TUs for kneel`:

```
cd OpenApoc-og-research
./scripts/ghidra_env.sh ghidra_projects OpenApocOG_TACP -process TACP.EXE -noanalysis \
  -scriptPath scripts -postScript QueryB1CoverStrings.java -log export/b1_cover_strings.log
```

Result — every one of the eight strings: `symbols= count=0`.

### 2.2 Method B — raw 4-byte pointer scan across the full image

`scripts/QueryB1PointerScan.java` searches the whole image for the little-endian 4-byte encoding
of each string's `.object2` runtime VA (Ghidra VAs, not file offsets — see §1 for file offsets):
`0x2903AD`, `0x2903BB`, `0x29040A`, `0x28F814`, `0x290994`, `0x2909EA` (the `beserk` copy), and
`0x290720` — `PTR_NONE` for all seven. **Zero raw bytes anywhere in the 3.2 MB image match any of
these addresses**, code or data — not a single pointer, table entry, or immediate operand anywhere
holds any of them.

### 2.3 The control that was wrong, and the real controls that replaced it

**What was wrong.** The first pass of this document validated the above two methods against
`senator`, address `0x2B2612`, cited in [compare-report.html](../compare-report.html) pass 5 as
TACP's one gameplay-adjacent bound xref — but that address was taken directly from the depot's
`string_xrefs.jsonl` export (a *different* Ghidra project/import) without re-verifying it live in
*this* project. Re-checking: a live byte search for `senator` in this project puts it at
`0x2A2612` — exactly `0x10000` below the borrowed address, matching a uniform slide also visible
in the jsonl's function addresses (`FUN_00031d74` there vs `FUN_00021d74` here, same `−0x10000`).
Dumping the raw bytes at the borrowed address `0x2B2612` in this project shows **non-ASCII binary
data, not a string at all** (`export/b1_2b2612_stdout.txt`: `BYTES_AT_0x2b2612` is unprintable).
The original control's 39-xref, 16-pointer-hit result was real, but it was measuring some
unrelated data cell, not `senator`, and not any string. That paragraph is retracted. Lesson
recorded for future RE in this lab: **never reuse an address from a different Ghidra
project/export without a live re-check** — the two address spaces are not identical, and nothing
in the tooling warns when they silently diverge.

**Correctly located, `senator` (live, this project) is at object2 `0x2A2612`, inside a *different*,
fixed 46-byte-stride (`0x2E`) table of internal asset names — file offsets confirmed by direct
stride arithmetic: `cultboss` `0x2F205A` → `civilservant` `0x2F2088` → `senator` `0x2F20B6` →
`bodyguard` `0x2F20E4`, each exactly `0x2E` bytes after the last — not the packed pool the B1
anchor strings live in.
Checked live at its correct address: `senator` **also shows zero direct xrefs** by Method A. It is
not usable as a positive control either, and the whole "resource-load key" theory for it does not
hold up once checked against the address that actually contains the string.

**A real, project-local, in-pool positive control was then found by full sweep** (`scripts/
QueryB1FullSweep.java`, live byte-search of all 2503 TACP strings ≥8 characters against this
project, filtering out any hit whose own bytes live in executable code — a floor needed because
short strings <8 chars matched x86 opcode coincidences like `SQVWU`, literally the push-register
opcode bytes 53/51/56/57/55 misread as text; raised the floor and the noise disappeared). This is
a **lower bound**, not a census — the sweep keeps only the first live occurrence of each string
text, so a string that appears more than once in the file (`MISSION BRIEFING` does, `Medi-kit`
does) is only checked at its first copy. 453 of 2503 strings showed a live xref. Filtered to the
same packed pool the B1 anchors live in (object2 range `0x28E000`–`0x293000`):

| String | object2 VA | file offset (VA `+0x4FAA4`, confirmed constant within this pool) | xrefs |
|---|---|---|---|
| `Ammo Clip` | `0x28FAB7` | `0x2DF55B` | 1 |
| `The following units will be lost if left in combat zone:` | `0x2908BD` | `0x2E0361` | 1 |
| **`Hostile unit spotted`** (no-colon copy) | `0x29090D` | `0x2E03B1` | 1 |
| `Search the building for Alien life forms…` | `0x290A7F` | `0x2E0523` | 1 |
| `MISSION BRIEFING` | `0x2927B2` | (repeats; not resolved to one file offset) | 1 (same address reported 20× — `MISSION BRIEFING` appears at 20 separate file offsets, the sweep's first-hit-only search resolves all of them to this one live copy; not 20 independent xrefs) |

`Hostile unit spotted` is the load-bearing one: it is a combat-event notification string, the
**same category** as `Unit under fire` / `Unit has gone beserk`, it exists in TACP in the same
paired with/without-colon pattern (colon copy `0x2DF1F8`, no-colon copy `0x2E03B1` — structurally
identical to `Unit under fire`'s two copies), and its no-colon copy — living in the same UI-options
pool as the B1 anchors — **has a live xref**. Every one of these five sits in the same packed,
variable-length, null-terminated pool as `Cautious mode` / `Kneel down` / `Unit under fire` (same
run of consecutive-offset entries; `Ammo Clip` sits 42 bytes into it). All five reach their
reference from the **same pointer table**, mapped next (§2.4).

**This is the discriminator that matters.** Pool membership does not, by itself, explain a
zero-xref result — sibling strings in the identical pool, including one in the identical
notification-message category, are referenced. The null result for the eight B1 anchors is
therefore a property of *those specific strings*, not an artefact of where they happen to live.
That is a stronger negative than the retracted control produced, not a weaker one.

### 2.4 The pool's pointer table, mapped — anchors are not in it

The five in-pool controls above are all referenced from one small table, `scripts/
QueryB1PoolTable.java` dumped object2 `0x292D00`–`0x293200` as 4-byte little-endian words and
resolved each value that pointed at readable text:

```
00292d18: 0x00292cea -> "PLEASE PUT THE XCOM CD B[ACK...]"
00292d20: 0x0028ec22 -> "Empty"
00292d3c: 0x0028f45f -> "X-COM"
00292d8c: 0x00292a00 -> "Health"
00292d90: 0x00292927 -> "Alien Egg"
00292d94: 0x0028f9f7 -> "Rookie"
00292d98: 0x0028fa57 -> "Psi-drain"
00292d9c: 0x0028f284 -> "Explosive"
00292da0: 0x0028fab7 -> "Ammo Clip"
00292da4: 0x0028fb86 -> "Weight:"
00292da8: 0x00290317 -> "Pause"
00292dac: 0x002908bd -> "The following units will [be lost...]"
00292db0: 0x0029090d -> "Hostile unit spotted"
00292db4: 0x00290a7f -> "Search the building for [...]"
00292db8: 0x002927b2 -> "MISSION BRIEFING"
00292ddc: 0x00292b3d -> "Smoke"
00292de0: 0x00292c17 -> "BLANK"
00292de4: 0x00292cb0 -> "Monday"
```

(Full dump has ~30 non-null entries in this window, mostly quoted above; NULL words interleave
throughout — consistent with a sparse table of individually-initialized global `char *` pointer
variables the linker packed together, rather than a single deliberately-authored "message ID"
array.) None of the eight B1 anchor VAs (`0x2903AD`, `0x2903BB`, `0x29040A`, `0x28F814`, `0x290994`,
`0x2909EA`, `0x290720`, plus the colon-`berserk` copy) appears as a value anywhere in this table.
Extending the check to the entire `.object2` block (not just this table's window) with the same
raw-pointer-scan method as §2.2: **none of the seven Ghidra-VA anchors appears as a 4-byte value
anywhere in the whole data segment** (`scripts/QueryB1PoolTable.java`, `ANCHOR ... NOT_A_TABLE_
VALUE_ANYWHERE` for all seven). This rules out simple absolute-pointer-table indirection as the
access mechanism for these specific strings, anywhere in the binary — not just in the one table
found.

The table itself (`getReferencesTo` on `0x292DA0`, `0x292D9C`, `0x292D00`) has **zero xrefs from
code** — nothing statically references this table's base either, at any of the addresses tried.
That doesn't rule out the table being reached by computed/relative addressing (a base held in a
register set up elsewhere, or reached via arithmetic Ghidra didn't resolve into a direct
reference) — it means this particular structural entry point terminates without a code anchor to
walk from.

### 2.5 Pool-base / walk-by-index check

A live-updating alternative to a pointer table is a runtime loop that walks the pool sequentially
by counting null terminators (`base; while (n--) base += strlen(base) + 1;`) — a common
space-saving pattern that would leave **no reference to individual entries at all**, only to the
pool's start. Checked xrefs to the two most plausible pool starts: `Options` (both copies present
in the file — `0x2DFBC8` and `0x2DFE0E`; live search finds the first, object2 `0x290124`) and the
two colon-message-pool starts `Unit under fire: ` (object2 `0x28F814`) / `Unit has gone berserk: `
(object2 `0x28F690`). All three: **zero xrefs** (`scripts/QueryB1PoolBase.java`). This weakens but
does not eliminate the walk-by-index theory — the true base address, if this mechanism is real,
may be reached through register arithmetic this analysis can't statically resolve, or my candidate
addresses may not actually be the pool's true start.

## 3. What the null result means

Within the identical pool, some entries are reached through a live, mapped pointer table (§2.3,
§2.4) and the eight B1 anchor strings are not — confirmed both by table membership and by a
full-segment pointer scan. The likeliest reading: two independent mechanisms coexist in this
pool. One is the sparse table at `0x292D18`–`0x292DEC` (probably individual global `char*`
variables used by scattered subsystems — unit-stat labels, inventory labels, mission-briefing
headers, the `Hostile unit spotted`/`Unit under fire` no-colon message-filter checkboxes). The
other, which the eight B1 anchors belong to, is not reached that way, and no walk-by-index base
address could be confirmed either (§2.5) — so the specific access mechanism for `Cautious mode` /
`Aggressive mode` / `Kneel down` / `Unit under fire` / `Unit has gone berserk`/`beserk` /
`Reserve TUs for kneel` remains unidentified. What can be said with confidence: whatever mechanism
displays them, if any does, carries no information about *how a tile is scored* — finding it would
at best explain UI-label rendering, not bind the cover metric.

This also means the runtime AI-mode dispatch (Aggressive/Normal/Cautious/Evasive) most likely
branches on a raw enum value, never on the string — so no amount of further string-anchored
searching, on any TACP string, can reach it by construction.

## 4. Named-function check (prior-session shortcut, ruled out)

Since other concurrent sessions in this shared lab are independently reverse-engineering other
battle-hazard rows (B5/F1/K1) against the same Ghidra project, checked whether any function in
`OpenApocOG_TACP` already carries a human-assigned name (as opposed to Ghidra's auto `FUN_xxxxxxxx`)
that might hint at AI/cover/mode logic from earlier work:

```
-postScript QueryB1NamedFuncs.java -log export/b1_named.log
```

Result: 13 named symbols, all default compiler/runtime artefacts (`_entry`, `__fdivp_sti_st`,
`__adj_fdiv_m64`, `caseD_0`/`caseD_40` switch labels, six import thunks). **No prior AI/unit/cover
labelling exists in this project.** No shortcut available.

## 5. Structural entry attempt: decompiling the already-bound B5/F1 fire/hazard functions

Requested as one of the two non-string-anchored entry points: check whether any function already
bound elsewhere in this lab (item/terrain-contact functions for the Fire row, gap matrix row 60)
happens to also touch a `BattleUnit`-equivalent struct with a mode-like field.
`scripts/QueryB1FireFuncsDecompile.java` decompiled all three:

- **`FUN_0007C110`** (file `0xD6BB4`, item HP on fire tiles): `void __regparm3
  FUN_0007c110(int param_1, char param_2)` — `param_1` is an **item** struct pointer: `+0x2` is a
  `char` type index into an 18-byte-stride resist table (`DAT_002B2860`), `+0xE` is a `short` HP
  value that gets decremented and zero-floored. No unit-mode field.
- **`FUN_0007AE18`** (file `0xD58BC`, fire power table): `undefined1 __regparm3
  FUN_0007ae18(uint param_1) { return (&DAT_00293050)[param_1 & 0xff]; }` — a pure table lookup,
  no struct at all.
- **`FUN_0007B3DC`** (file `0xD5E80`, fire overlay stage/extinction): `void __regparm3
  FUN_0007b3dc(int param_1, int param_2, byte *param_3, byte *param_4)` — `param_1`/`param_2` (plus
  an unnamed-register third value) are **tile x/y/z map coordinates**, checked against map bounds
  `DAT_000E4C10`/`14`/`18`; `param_3` is a terrain overlay-state byte pointer; `param_4` is an
  item/catalog pointer whose `+3` byte indexes 86-byte-stride (`0x56`) terrain-catalog tables
  (`DAT_00104921`, `DAT_001048D8`, `DAT_000F46D9`, `DAT_001048D9`). Tile- and catalog-level, no
  unit-mode field.

**None of the three reads anything resembling a unit AI-behaviour-mode field.** They operate on
item structs, a flat lookup table, and tile/catalog data respectively — confirmed by decompile,
not assumed from the gap-matrix description. This structural entry point terminates without
reaching a `BattleUnit`-equivalent struct.

## 6. Structural entry attempt: pool pointer-table base (see §2.4–2.5)

The second non-string-anchored entry point requested — finding the pool's index/pointer table and
following it back to a code consumer — was carried out in §2.4 (table mapped, anchors absent from
it and from the whole data segment) and §2.5 (no xref to any candidate walk-by-index base). Both
terminate without a code anchor: the table has zero referrers, and no walk-by-index base could be
confirmed. There is no third structural candidate to try without an anchor to start from —
searching for the `BattleUnit`-equivalent struct's AI-mode field with **no string, no bound field,
and no table entry to walk from** would mean a blind sweep of 1595 functions, which §7 explains
was rejected on both passes of this investigation.

## 7. Method-part-5 cross-check (map-part struct) — unreachable, and the infrastructure doesn't exist yet either

No metric was recovered, so there is nothing to cross-check against a map-part `unknown*` byte —
but the target struct doesn't exist yet in this codebase either, independent of that:
`tools/extractors/common/battlemap.h` has no per-tile runtime scenery/wall struct at all, only
map-*authoring* structures (`BuildingDatStructure`, `LineOfSightData` — the `.SLS` block struct,
136 bytes, `unknown02[116]` still undecoded per `tactical.txt`, but this is generator-time
block/spawn/AI-priority metadata, not a runtime tile). `tools/extractors/common/scenerytile.h`
holds only a UFO2P minimap-colour palette. `tools/extractors/common/tacp.h` has zero `unknown*`
fields.

## 8. What was NOT attempted, and why

A blind decompile-and-pattern-match sweep of all 1595 TACP functions for the shape "unit pointer +
position/direction, loop over neighbour tiles, accumulate/compare a per-tile integer" was
considered and rejected on both passes of this investigation. Ghidra's generic decompiler output
carries no semantic tags for "unit" or "tile" — a shape match over synthetic variable names can
only produce unverified *candidates*, and per the prime directive a candidate is not a binding:
"the recurring failure mode in this project is finding plausible bytes and guessing their meaning"
([parity-guide.md §2](../parity-guide.md)). Two independent, anchored structural entry points were
tried instead (§5, §6) and both terminated cleanly; neither produced a candidate worth chasing
blind.

## Verdict

**NOT BOUND.** No cover-tile scorer was found via (a) string anchoring — direct symbolic xref and
full-image raw-pointer scan, both zero for all eight anchor strings; (b) pool pointer-table entry —
the table that *does* reference sibling pool strings (including the category-matched `Hostile unit
spotted`) was mapped and does not reference any B1 anchor, nor do the anchors appear anywhere else
in the data segment, nor does the table itself have a code-level referrer to walk from; or
(c) unit-adjacent-function entry — the three already-bound B5/F1 fire/hazard functions were
decompiled and confirmed to operate on item, table, and tile/catalog data only, never a unit-mode
field. `getTakeCoverMovement`
([unitaihelper.cpp:142](../../../game/state/battle/ai/unitaihelper.cpp#L142)) must keep returning
`nullptr` with its existing "Cover-tile search is not implemented" comment. Do not implement a
"move away from the enemy" or "move behind the nearest wall" stand-in — both are inventions per
the task's absolute rule, and this row's confidence should stay driven by `ai.txt` prior-art only.

**What would still change this**: locating the `BattleUnit`-equivalent struct's AI-mode field
through some anchor not yet tried — e.g. a save-game field-write site, or a different already-bound
TACP function (outside B5/F1) that turns out to touch unit behaviour state. No such anchor is
currently known in this lab. That is a materially larger, currently unanchored RE effort and was
correctly out of scope for both passes of this investigation.

## Artifacts (lab only — not copied into this tree)

- `OpenApoc-og-research/scripts/QueryB1CoverStrings.java` — xref dump for the strings not already
  covered by `QueryTacpGaps.java`.
- `OpenApoc-og-research/scripts/QueryB1PointerScan.java` — raw pointer byte scan, anchor targets.
- `OpenApoc-og-research/scripts/QueryB1Control.java` — **retracted**: used a borrowed, unverified
  cross-project address; kept in the lab for the record, not cited as evidence.
- `OpenApoc-og-research/scripts/QueryB1ControlRedo.java` — live re-verification of `senator`
  (correct address, correct table) and the `beserk` typo string.
- `OpenApoc-og-research/scripts/QueryB1WhatIsAt2b2612.java` — confirms the retracted control's
  address holds non-ASCII data.
- `OpenApoc-og-research/scripts/QueryB1FullSweep.java` — full live sweep of all TACP strings ≥8
  chars for xrefs; first-hit-only, a lower bound not a census (see §2.3).
- `OpenApoc-og-research/scripts/QueryB1InPoolControl.java` — full xref detail for the three
  in-pool positive controls.
- `OpenApoc-og-research/scripts/QueryB1PoolTable.java` — pointer-table dump and anchor-absence
  check across all of `.object2`.
- `OpenApoc-og-research/scripts/QueryB1PoolBase.java` — walk-by-index base xref check.
- `OpenApoc-og-research/scripts/QueryB1FireFuncsDecompile.java` — decompiles the three B5/F1
  fire/hazard functions for the unit-adjacent structural entry attempt.
- `OpenApoc-og-research/scripts/QueryB1NamedFuncs.java` — named-symbol dump (prior-work check).
- Logs: `export/b1_cover_strings{,_stdout}.txt`, `export/b1_ptr_scan{,_stdout}.txt`,
  `export/b1_control{,_stdout}.txt` (retracted), `export/b1_control_redo{,_stdout}.txt`,
  `export/b1_2b2612{,_stdout}.txt`, `export/b1_full_sweep2{,_stdout}.txt`,
  `export/b1_inpool{,_stdout}.txt`, `export/b1_pooltable{,_stdout}.txt`,
  `export/b1_poolbase{,_stdout}.txt`, `export/b1_firefuncs{,_stdout}.txt`,
  `export/b1_named{,_stdout}.txt`, plus the pre-existing `export/tacp_gaps{,_stdout}.txt`.

Note: this session hit a genuine `LockException` several times against the shared `OpenApocOG_TACP`
Ghidra project from a concurrent peer session independently working B5/F1/K1 (hazard rows) against
the same project. Retried with backoff rather than forcing; no destructive action was taken on the
shared project.
