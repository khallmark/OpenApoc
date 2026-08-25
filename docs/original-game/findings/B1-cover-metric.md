# B1 · Cautious / Normal cover + potshots — cover-tile metric

**NOT BOUND: no cover-tile scoring function exists to find in TACP.EXE via string anchoring, and
the anchor strings themselves are unreferenced anywhere in the binary (validated against a
known-positive control). `UnitAIHelper::getTakeCoverMovement` must keep returning `nullptr`.**

This closes the RE half of B1 with a documented negative result, per the parity-guide prime
directive: a recorded negative is a successful outcome, and inventing a "move behind the nearest
wall" heuristic to close the row would be a regression.

---

## 0. Binary confirmed

`OpenApoc-og-research/canonical/TACP.EXE`: size 3,170,298 bytes, CRC32 `0xfebbe39e` — matches the
extractor-canonical non-4 ISO build cited in
[binaries/tacp.md](../binaries/tacp.md). Ghidra project: `ghidra_projects/OpenApocOG_TACP.rep`
(pre-imported, pre-analysed; 1595 functions, `.object1` code `0x10000`–`0xD44FE`, `.object2` data
`0xE0000`–`0x33F0AF`).

## 1. Strings confirmed, both generations

All seven candidate strings from the task and from `ai.txt`/parity-guide exist verbatim in TACP,
at the exact `−0x2200` non-4→4 slide documented in [binaries/tacp.md](../binaries/tacp.md) (no
exception, unlike `crew_ufo_downed`):

| String | non-4 file offset | 4-build file offset | Δ |
|---|---|---|---|
| `Cautious mode` | `0x2DFE51` | `0x2DDC51` | `−0x2200` ✓ |
| `Aggressive mode` | `0x2DFE5F` | `0x2DDC5F` | `−0x2200` ✓ |
| `Kneel down` | `0x2DFEAE` | `0x2DDCAE` | `−0x2200` ✓ |
| `Unit under fire: ` (with colon) | `0x2DF2B8` | `0x2DD0B8` | `−0x2200` ✓ |
| `Unit under fire` (no colon) | `0x2E0438` | `0x2DE238` | `−0x2200` ✓ |
| `Unit has gone berserk: ` | `0x2DF134` | `0x2DCF34` | `−0x2200` ✓ |
| `Reserve TUs for kneel` | `0x2E01C4` | `0x2DDFC4` | `−0x2200` ✓ |

Source: `export/strings/TACP_strings.txt` / `TACP4_strings.txt` (pre-dumped, offset-labelled).
`potshot` and `evasive`: **zero hits in TACP, either generation, any case.** `cover` as a whole
word: **zero hits** — every raw match is a substring of `Recover`/`discover`/`recovery` inside
mission-briefing prose (e.g. file `0x2E0523`, `0x2E0779`, `0x2E1E59`), none of them the tactical
term.

The 4-build project (`TACP4.EXE`) is not imported in this lab (only `OpenApocOG_TACP.rep` for
non-4 exists), so the xref checks below ran on non-4 only. The exact, exception-free slide match
across all seven strings is treated as sufficient confirmation that the 4-build carries the same
(unreferenced) text, consistent with how every other TACP table in this lab has behaved.

## 2. Xref walk: every anchor string is unreferenced — verified, not assumed

The parity guide already flagged "several of these have EMPTY bound xrefs" for `Cautious mode` /
`Aggressive mode` / `Kneel down` / `Reload time:` type strings, with instructions to verify rather
than trust it. Verification, two independent methods, both against the imported non-4 Ghidra
project:

**Method A — Ghidra's own reference table** (`getReferencesTo` on the string's data address, i.e.
`Program.getListing().getDataAt(...)` xrefs — this is what "bound xrefs" means in this lab).
Reused the prior run of `scripts/QueryTacpGaps.java` (already present in the lab; log dated
23 Aug, `export/tacp_gaps.log` / `tacp_gaps_stdout.txt`), which already covered `Cautious mode`,
`Aggressive mode`, `Kneel down`, `cover`, `potshot`, `evasive`, and added
`scripts/QueryB1CoverStrings.java` (new, this session) for `Unit under fire` / `Unit has gone
berserk` / `Reserve TUs for kneel`, which the existing script didn't cover:

```
cd OpenApoc-og-research
./scripts/ghidra_env.sh ghidra_projects OpenApocOG_TACP -process TACP.EXE -noanalysis \
  -scriptPath scripts -postScript QueryB1CoverStrings.java -log export/b1_cover_strings.log
```

Result — every one of the seven strings: `symbols= count=0`. Not one instruction anywhere in
`.object1`/`.object2` holds a reference Ghidra recognises to any of them.

**Method B — raw 4-byte pointer scan**, independent of Ghidra's reference table (in case
something built a pointer array Ghidra's analysis didn't tag as `Data`/didn't create a xref for).
`scripts/QueryB1PointerScan.java` searches the whole image for the little-endian 4-byte encoding
of each string's `.object2` runtime VA:

```
-postScript QueryB1PointerScan.java -log export/b1_ptr_scan.log
```

Result: `PTR_NONE` for all seven target VAs (`0x2903AD`, `0x2903BB`, `0x29040A`, `0x28F814`,
`0x290994`, `0x28F690`, `0x290720`) — **zero raw bytes anywhere in the 3.2 MB image match any of
these addresses**, code or data. (These are Ghidra `.object2` VAs, not file offsets — see the file
offsets in §1.)

### Instrument validated against a known-positive control

Two all-zero passes could mean "genuinely unreferenced" or "the query is broken" (e.g. an
unrelocated-object false negative — a real risk in a bound-LE/LX-loader import). Before trusting
the negative, `scripts/QueryB1Control.java` ran the identical two checks against a string the lab
already knows is live: `senator`, `.object2` VA `0x2B2612`, cited in
[compare-report.html](../compare-report.html) pass 5 as TACP's one gameplay-adjacent bound xref
(consumer `FUN_0009A524` among others).

```
-postScript QueryB1Control.java -log export/b1_control.log
```

Result: `count=39` symbol xrefs, 16 raw-pointer hits, all inside `.object1` (executable), from this
live project: `FUN_000548BC`, `FUN_0005ABC8`, `FUN_00024D00`, `FUN_00021D74`, `FUN_0005B0E4`,
`FUN_0005A278` (truncated at 10 of 39 in the script output). **The instrument correctly finds real
references when they exist.** The all-zero result for the seven B1 anchor strings is therefore a
genuine finding, not a tooling artifact.

(`compare-report.html` pass 5 names `FUN_0009A524` as a `senator` consumer too, but that address
comes from the depot's `string_xrefs.jsonl` export, a *different* address space — its addresses run
+0x10000 above this live project's, e.g. jsonl `FUN_00031d74` vs this run's `FUN_00021d74` for the
same reference site. Not re-derived here; cited only to identify which string was used as the
control, not as a live-project address.)

## 3. Consequence: step 2 of the RE method is a dead end by construction

The task's step 2 ("walk CALLERS of any function that references those strings") presupposes such
a function exists. It does not — there are zero callers of zero references. `Cautious mode` /
`Aggressive mode` / `Kneel down` / `Unit under fire` / `Unit has gone berserk` /
`Reserve TUs for kneel` are option-list / notification-log **text-table entries with no static
in-binary consumer** this analysis could find. The likeliest reading: the UI mode selector and the
morale-log renderer both dispatch on a raw integer (mode enum / event-type enum) and never touch
the string's address directly — some other, unindexed mechanism (a runtime string-table load, or a
generic "log this numbered message" call whose id table isn't a flat pointer array) puts the text
on screen. That mechanism, whatever it is, carries no information about *how a tile is scored*, so
even finding it would not bind the target metric.

This also means the runtime AI-mode dispatch (Aggressive=0/Normal=1/Cautious=2 or similar) almost
certainly branches on the *raw enum value*, never on the string — so no amount of further
string-anchored searching, on any TACP string, can reach it. That function can only be found by
structural/numeric search (see §5), which this project's rules do not let a single confidence-free
pass promote to "BOUND" (see §5).

## 4. Named-function check (prior-session shortcut, ruled out)

Since another concurrent session in this shared lab is independently reverse-engineering other
battle-hazard rows (B5/F1/K1) against the same Ghidra project, checked whether any function in
`OpenApocOG_TACP` already carries a human-assigned name (as opposed to Ghidra's auto `FUN_xxxxxxxx`)
that might hint at AI/cover/mode logic from earlier work:

```
-postScript QueryB1NamedFuncs.java -log export/b1_named.log
```

Result: 13 named symbols, all default compiler/runtime artefacts (`_entry`, `__fdivp_sti_st`,
`__adj_fdiv_m64`, `caseD_0`/`caseD_40` switch labels, six import thunks). **No prior AI/unit/cover
labelling exists in this project.** No shortcut available.

## 5. Method step 5 (map-part cross-check) — unreachable, and the infrastructure doesn't exist yet either

Step 5 asks to cross-check a recovered per-tile metric against an unmapped `unknown*` byte on the
battle map-part struct. No metric was recovered, so there is nothing to cross-check — but it is
also worth recording that the target struct doesn't exist yet in this codebase:
`tools/extractors/common/battlemap.h` has no per-tile runtime scenery/wall struct at all, only
map-*authoring* structures (`BuildingDatStructure`, `LineOfSightData` — the `.SLS` block struct,
136 bytes, `unknown02[116]` still undecoded per `tactical.txt`, but this is generator-time
block/spawn/AI-priority metadata, not a runtime tile). `tools/extractors/common/scenerytile.h`
holds only a UFO2P minimap-colour palette. `tools/extractors/common/tacp.h` has zero `unknown*`
fields. There is presently no TACP battle-map-part extractor to correlate against, independent of
whether a metric is ever found.

## 6. What was NOT attempted, and why

A blind decompile-and-pattern-match sweep of all 1595 TACP functions for the shape "unit pointer +
position/direction, loop over neighbour tiles, accumulate/compare a per-tile integer" was
considered and rejected. Ghidra's generic decompiler output carries no semantic tags for "unit" or
"tile" — a shape match over synthetic variable names can only produce unverified *candidates*, and
per the prime directive a candidate is not a binding: "the recurring failure mode in this project
is finding plausible bytes and guessing their meaning" ([parity-guide.md §2](../parity-guide.md)).
Doing the sweep would not have changed the verdict, only spent the budget on output that still
ends at NOT BOUND.

## Verdict

**NOT BOUND.** `getTakeCoverMovement` ([unitaihelper.cpp:142](../../../game/state/battle/ai/unitaihelper.cpp#L142))
must keep returning `nullptr` with its existing "Cover-tile search is not implemented" comment.
Do not implement a "move away from the enemy" or "move behind the nearest wall" stand-in — both
are inventions per the task's absolute rule, and this row's confidence should stay driven by
`ai.txt` prior-art only.

**What would change this**: a structural (non-string-anchored) discovery of the AI-mode dispatch
function — e.g. by first binding the `BattleUnit`-equivalent struct's AI-mode byte via some *other*
already-bound consumer. The fire/hazard functions bound for B5/F1 (`FUN_0007C110`, `FUN_0007AE18`,
`FUN_0007B3DC`) are item/terrain-contact paths per the gap matrix's Fire row, not unit-behavior
paths — but whether any of them (or their neighbours) also touches a unit-mode field was **not
examined in this pass**; this doc does not claim that check was done. That structural walk is a
materially larger RE effort than string anchoring and was out of scope here; flagging it as the
concrete next step rather than leaving it implicit.

## Artifacts (lab only — not copied into this tree)

- `OpenApoc-og-research/scripts/QueryB1CoverStrings.java` — xref dump for the 4 strings not
  already covered by `QueryTacpGaps.java`.
- `OpenApoc-og-research/scripts/QueryB1PointerScan.java` — raw pointer byte scan, all 7 targets.
- `OpenApoc-og-research/scripts/QueryB1Control.java` — known-positive (`senator`) control for both
  methods above.
- `OpenApoc-og-research/scripts/QueryB1NamedFuncs.java` — named-symbol dump (prior-work check).
- Logs: `export/b1_cover_strings{,_stdout}.txt`, `export/b1_ptr_scan{,_stdout}.txt`,
  `export/b1_control{,_stdout}.txt`, `export/b1_named{,_stdout}.txt`, plus the pre-existing
  `export/tacp_gaps{,_stdout}.txt` this session re-used for `Cautious mode` / `Aggressive mode` /
  `Kneel down` / `cover` / `potshot` / `evasive`.

Note: this session hit a genuine `LockException` twice against the shared `OpenApocOG_TACP`
Ghidra project from a concurrent peer session independently working B5/F1/K1 (hazard rows) against
the same project. Retried with backoff rather than forcing; no destructive action was taken on the
shared project.
