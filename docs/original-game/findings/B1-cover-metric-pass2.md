# B1 pass 2 · Cautious / Normal cover + potshots — structural re-entry

> **BOUND (qualified): a candidate-position exposure-scoring function exists, is invoked from the
> AI's per-item weapon-fire decision, and picks a movement destination by minimising it.** This is
> not the "loop over adjacent tiles, read a wall/solidity byte" mechanism the parity guide
> hypothesised — pass 1 exhausted that shape via string anchoring and found nothing, correctly.
> The real mechanism scores **named candidate destination points** (the unit's own position, a
> unit it is tracking, a remembered danger point, up to four pre-baked terrain waypoints, a home
> point, and three fixed fallback points) by **counting nearby hostiles/visible units with a
> qualifying line-of-sight class inside a large clamped search volume around each candidate,
> penalised by a range-indexed table**, and keeps whichever candidate scores highest (least
> penalised). The caller is `FUN_0008f338`'s weapon-category branch — i.e. this runs every AI
> think-tick a unit considers firing a weapon, in both the "have ammo" and "no ammo" code paths.
>
> **What is NOT established**: which of `Cautious` / `Normal` / `Aggressive` / `Evasive`
> (if any) gates this — no mode-selecting byte was found anywhere in the traced call chain
> (`FUN_0008a524` → `FUN_0008e694` → `FUN_0008f338` → `FUN_0008c1fc` → `FUN_0007e600`, and the
> tick dispatcher `FUN_00066474` → `FUN_0008f9e0`); it appears to run unconditionally for every
> AI-controlled unit's weapon hand. A wider xref sweep this pass found a real, unexamined
> mode-field candidate (`unit+0x21C`, 20 readers/writers including the order dispatcher — §2, §7.1)
> that a future pass should decompile first. The three functions a unit's `+0x220` field dispatches
> to on arrival were decompiled this pass and turned out to be locomotion-style path-queuers
> (order types `0x18`/`0x19`/`0x1A`), **not** a kneel/prone stance selector — that specific parity-
> guide question is now a confirmed miss, not an open thread. **Do not treat this document as fully
> closing B1** — it upgrades the row from "no anchor exists" (pass 1's verdict) to "a scoring
> mechanism is recovered, structurally connected to the fire decision, with one strong unexamined
> mode-field lead remaining." Implementers should treat §4–§5 as the metric to port and §7 as the
> remaining RE backlog, not as blocking unknowns that require re-opening the whole search.

Agent: this session (B1 pass 2, structural re-entry — resumed per the note that pass 1 closed only
because "no string, no bound field, and no table entry to walk from" was available; this pass
started from the already-bound AI cluster instead). Lab: `OpenApoc-og-research`, project
`ghidra_projects/OpenApocOG_TACP.rep`, program `TACP.EXE` (non-4), loader
`-processor x86:LE:32:default -cspec gcc`. All work below was done live against the loaded Ghidra
image (decompiler + raw listing via `analyzeHeadless` post-scripts) — no citation in this document
was verified by seeking into `TACP.EXE` at a computed file offset, per this lab's standing method
warning that LX object pages are not contiguous and that shortcut produces false negatives on
correct citations.

## 0. Binary confirmed

`OpenApoc-og-research/canonical/TACP.EXE`: size 3,170,298 bytes, CRC32 `0xfebbe39e` — matches
[binaries/tacp.md](../binaries/tacp.md) and the pass-1 document. Ghidra project
`ghidra_projects/OpenApocOG_TACP.rep` (same shared, pre-analysed project pass 1 and the K1
investigation used; 1595 functions, `.object1` code `0x10000`–`0xD44FE`). This session hit the same
kind of `LockException` pass 1 records, from concurrent peer sessions in this shared lab
(`RE-cloak`, `RE-hazard`, etc. per the session's agent roster); retried with backoff, no forced or
destructive action taken on the shared project.

## 1. Read first

[B1-cover-metric.md](B1-cover-metric.md) (pass 1) is not superseded — its string-anchoring result
stands: all eight `Cautious mode`/`Unit under fire`/etc. strings genuinely have zero code xrefs,
confirmed by three independent methods. Pass 1's own "what would still change this" clause named
the missing ingredient exactly: *"locating the `BattleUnit`-equivalent struct's AI-mode field
through some anchor not yet tried — e.g. ... a different already-bound TACP function (outside
B5/F1) that turns out to touch unit behaviour state."* This pass used exactly that anchor: the K1
investigation's already-bound AI cluster (`FUN_0008a524` equip-priority switch, `FUN_0008f338`
per-item combat decision, `FUN_00066474` tick dispatcher — see
[K1-cloak.md](K1-cloak.md)) turned out to sit one call-hop from a genuine scoring function.
[METHOD-tacp-string-regions.md](METHOD-tacp-string-regions.md) and
[METHOD-tacp-string-resolver.md](METHOD-tacp-string-resolver.md) remain correct: display strings in
this binary are not a viable anchor for gameplay logic in general, cover metric included.

## 2. The unit array — recovered, independent of whether the metric binds

This is the durable, load-bearing result of this pass even considered on its own: **the
`BattleUnit`-equivalent struct that pass 1 could not anchor into is now located.**

- **Base VA**: `0x109ED0`. **Stride**: `0x49C` (1180 bytes). **End**: `0x11B360`.
  `(0x11B360 − 0x109ED0) / 0x49C = 60` exactly — **60 unit slots**, no remainder. This exact
  arithmetic is confirmed independently in the raw instruction listings of two unrelated
  functions (`FUN_0008f9e0`'s bound-check `CMP ECX,0x11b360`, and `FUN_0007e600`'s loop-termination
  `CMP ESI,0x109ed0` / `SUB ESI,0x49c`), not derived once and assumed.
- A **second, parallel per-unit array** exists at stride `0x24E` (590 bytes), walked in lockstep
  with the main array in `FUN_0007e600`'s unit-scan loop (`psVar11 = psVar11 + -0x24e` alongside
  `iVar7 = iVar7 + -0x49c` on the same iteration). This is the array K1 already named
  `agent_general_data`-adjacent AI/decay-counter state (`DAT_0010a17e`/`0x10a180`/`0x10a182` in
  that document); this pass confirms it is unit-indexed at the same 60-slot cardinality and walks
  parallel to, not instead of, the main struct.
- Every offset below is a **byte displacement from a direct register pointer to one unit's struct**
  (confirmed against the *raw instruction listing*, not the decompiler's re-typed pointer
  arithmetic — `FUN_0008c1fc` declares its unit parameter `short *`, which would make its literal
  offsets look doubled if read as decompiler source; the table below is post-conversion to real
  byte offsets and cross-checked against `FUN_0008f9e0`/`FUN_00066474`, which both take an
  `undefined1 *`/direct pointer and so need no conversion).

| Offset | Width | Confirmed via | Reading |
|---|---|---|---|
| `+0x00` | short | `FUN_0008e694`, `FUN_0009cab4`, `FUN_0007e600` (raw listing, all three) | Slot-valid sentinel: `-1` = empty slot. |
| `+0x1E` | int (hi-word used) | `FUN_0009cab4`, `FUN_0007e600` | Side/faction id — matched against a caller-supplied side and used as the same-side/detect-else filter in the unit-scan loop. |
| `+0x24` | short | `FUN_0009cab4`, `FUN_0008e694` | Compared against global "active/player side" (`DAT_000e6d64`) or a debug all-visible flag (`DAT_000e6d66`) — a visibility/ownership gate distinct from `+0x1E`. |
| `+0x64` | short | `FUN_0009cab4`, `FUN_0007e600`, `FUN_0008e694`, `FUN_00066474` (raw listing, all four) | Vitality-like gate: must be `> 0` and (in the `FUN_0009cab4`/`FUN_0008e694` "usable unit" predicate) `>` a second field at `+0x80`. Reused for a different-looking ammo/ready-counter comparison in `FUN_00066474`'s tick body — **not certain the two uses are the same semantic field**; flagged, not resolved. |
| `+0x7C` | byte | `FUN_0009cab4`, `FUN_0008e694` | `== 0` required for a unit to count as usable — reads as a dead/incapacitated flag. |
| `+0x80` | short | `FUN_0009cab4`, `FUN_00066474` | Paired with `+0x64`; see caveat above. |
| `+0xA8`/`+0xAA`/`+0xAC` | int × 3 (fixed-point; `>>0x10` for the integer tile coordinate) | `FUN_0007e600` raw listing (`[ESI+0xa8]`/`[ESI+0xaa]`/`[ESI+0xac]`), `FUN_0008c1fc` (same offsets via its `short*` typing) | Unit's own map position, X/Y/Z. |
| `+0x212`/`+0x214`/`+0x216` | int × 3 | `FUN_0008f9e0` raw listing | A second position-like triple, compared against `+0x45C`/`+0x45E`/`+0x460` with a ±2-tile tolerance to decide "have I arrived". |
| `+0x21C` | short | `FUN_00066474` raw listing; **34 raw xrefs across 20 unique functions** (`getReferencesTo` on unit-slot-0's address `0x10A0EC`, this pass), including `FUN_0003D9E4` (K1's order-type dispatcher) and `FUN_0005BAD8` (one of the untraced "result→order" functions from §6) | 3-way selector (`0` / `<2` / `==2`) dispatching to `FUN_0004d3f0`/`FUN_0004d490`/`FUN_0004d434` inside the per-tick update. **The single strongest mode-field candidate this pass found**: unlike `+0x475` (consulted only inside the tight AI-think loop, see below), this field is read and written from 20 functions spanning the order dispatcher, movement, and several subsystems not yet identified — the breadth this project's own discriminator (a persistent-doctrine field is touched broadly; a transient scratch value stays local) points toward. **Not resolved this pass** — none of the 20 consumer functions were decompiled to see what the 3 values mean or where they originate; flagged as the top follow-up target, not claimed as the mode field. |
| `+0x220` | short | `FUN_0008f9e0` raw listing; **9 raw xrefs across 8 unique functions** (xref on `0x10A0F0`, this pass) | 3-way selector (`0` / `<2` / `==2`) dispatching to `FUN_000A3250`/`FUN_000A33A8`/`FUN_000A3500`. **All three decompiled this pass (§6)**: each is a near-identical "compute a path via `FUN_000A3620`, queue a follow-path order" function, differing only in the **order type queued** — `0x18`, `0x19`, `0x1A` respectively (`FUN_000A3250`/`33A8` also each have one special-case upgrade, to `0x25`/`0x26`, when the path's destination matches the currently-selected/highlighted unit). Reads as a **locomotion-style selector** (plausibly walk/run/creep, unconfirmed) consumed by whichever function dispatches orders `0x18`–`0x1A` — not traced. This narrow, low-fan-out field is a weaker mode-field candidate than `+0x21C` precisely because so few functions touch it. |
| `+0x254`/`+0x256` | short × 2 | `FUN_00066474` (already documented in K1) | Disruptor Shield charge/cap, `+1` per tick, capped. Not cover-related; listed for cross-reference. |
| `+0x2AE`/`+0x2B0`, `+0x2B2`/`+0x2B4` | short pairs | `FUN_00066474`, K1 | Hand-slot 1/2 equipment-instance index + Mind-Shield-type gate (already documented). |
| `+0x41A` | int | `FUN_0008f9e0` raw listing | Target/order-position-set sentinel: `>>0x10 == -1` means "no destination assigned"; gates the whole function. |
| `+0x45C`/`+0x45E`/`+0x460` | int × 3 | `FUN_0008f9e0` raw listing | Assigned destination X/Y/Z — what `+0x212..+0x216` is compared against. |
| `+0x464`/`+0x466`/`+0x468`/`+0x46A` | short × 4 | `FUN_0008f9e0` raw listing | Cached "last known" position/facing, compared against `+0x222`/`+0x224`/`+0x226`/`+0x22A`; a mismatch queues order-type `0x1D` ("target moved", one raw-listing-confirmed guess). |
| `+0x46C` | byte | `FUN_0008f9e0`, `FUN_00066474`, `FUN_0008e694` (raw listing, all three); **22 raw xrefs across 19 unique functions** (xref on `0x10A33C`, this pass) | Gate flag read before (re-)triggering `FUN_0008f9e0` — reads as "AI reconsideration in progress" / re-entrancy guard, not a mode. The fan-out is nearly as broad as `+0x21C`'s, so this reading is held with the same caution: it was not stress-tested against the mode-field discriminator by decompiling the 19 consumers, only inferred from the two functions already fully read. |
| `+0x475` | byte | `FUN_0008f338`, `FUN_0008e694`, `FUN_0008c1fc` (all as `param_1+0x475`/short-index `0x23A`-adjacent); **24 raw xrefs across 12 unique functions** (xref on `0x10A345`, this pass) | **Do not label this the behaviour-mode field.** `FUN_0008f338` writes `1` here on a specific outcome; `FUN_0008e694` reads it `== 3` then immediately resets it to `0`; `FUN_0008c1fc` gates one candidate on it being `< 2`. The wider xref set found this pass (10 more functions beyond the K1/B1 cluster — `FUN_0009d938`, `FUN_00096ed0`, `FUN_00093edc`, `FUN_0009b058`, `FUN_0009e0f8`, `FUN_00092354`, `FUN_0009c37c`, `FUN_00097558`, `FUN_000a3620`, `FUN_0008fd58`) is wider than assumed when this row was first written, so the "touched only inside the AI think path" claim is **weaker than originally stated** — none of the 10 new functions were decompiled to check whether they too are AI-internal or reach outside it. Kept as a lower-confidence non-mode reading than before, not retracted. |
| `+0x476` | byte | `FUN_0008f338`; 6 raw xrefs (`FUN_0009d938` write, `FUN_0008f96c` read/read/read-write, plus the two already-known `FUN_0008f338` sites) | Companion counter to `+0x475`; set to `100` alongside `+0x475=1` on one outcome — reads as a confidence/cooldown value, not examined further. |

**No byte or field in this table, or found anywhere in the five functions decompiled this pass, is
read as a 0–3/0–4 discriminant gating *whether* the exposure-scoring metric (§4) runs, or gating
*which* candidate list it evaluates.** That is the closest this pass came to a mode-field negative:
stated precisely, not as "no mode field exists in TACP" (not checked — 44+ residual functions touch
the general-equipment/behaviour region per K1's own accounting, most unexamined) but as **"no mode
gate exists in this specific, now-completely-traced call chain from the tick dispatcher and the
per-item combat evaluator down to the exposure scorer."**

## 3. The AI decision cluster, as a call graph

```
FUN_00066474  (per-unit tick dispatcher, K1's "Mind Shield home"; also drives Disruptor Shield regen)
  └─ FUN_0008f9e0  (arrival/regroup check: "have I reached my assigned destination?")
       ├─ [if arrived]   queues order 0x1B / 0x23 (debug), may call FUN_00097024 / FUN_00096ed0
       └─ [if not]       finds other units sharing the same destination, then dispatches on
                          unit+0x220 to FUN_000A3250 / FUN_000A33A8 / FUN_000A3500  (movement execution)

FUN_0008a524  (AI equip-priority scoring switch — K1)
  └─ FUN_0008e694  (per-unit-per-item AI combat evaluator; also recurses into itself for the 2nd hand)
       ├─ [early-out guards: slot valid, +0x64/+0x80/+0x7c vitality, +0x24 side/visibility,
       │   not already order-queued for this unit, +0x34-family reachability check via FUN_0005a3e8]
       └─ FUN_0008f338  (per-item combat decision — K1's 3rd cloak-order recognizer)
            ├─ [general-equipment branch, type 0x0a/Cloak]  → order 0x11 (K1: resolves to a decay
            │   counter seed, no cloak effect — unchanged from K1's verdict)
            └─ [weapon-category branch, BOTH the "has ammo" and "no ammo" sub-paths]
                 ├─ FUN_0008122c / FUN_0007f110  (small helpers; angle/zone-index arithmetic)
                 ├─ FUN_0008c1fc  (multi-candidate destination evaluator — §5)
                 │    └─ FUN_0007e600  ×N  (per-candidate exposure score — §4)
                 ├─ FUN_0008f918  (per-side "does anyone still need an order" completion check)
                 └─ FUN_0005bad8 / FUN_00063f44 / FUN_00058fe8  (result→order-queue translation;
                     not traced this pass)
```

Every arrow above is a confirmed call (`getReferencesTo` on the callee's entry point, cross-checked
against the raw `CALL` instruction in the caller's listing) — not inferred from decompiler output
alone, per this lab's standing rule to prefer the raw listing where the two disagree.

## 4. `FUN_0007e600` — the per-candidate exposure score

**VA `0x7E600`. Bound-file offset `0xD90A4` (`VA + 0x5AAA4`, the same object1 constant K1 verified
against `FUN_0008a524`/`FUN_00066474`/`FUN_0008f338` — stated as the within-page convention this
lab uses, not independently re-verified by seeking into the EXE, per the method warning). Page-file
`0x6E5FF`. Size 855 bytes.** Signature (decompiler): `FUN_0007e600(int param_1, undefined1 param_2,
undefined1 param_3, undefined1 param_4)`. `param_1` is a pointer to the **candidate's context
record** (read at `+0x1E` for a side/faction id — the same offset the unit struct uses for its own
side field, consistent with this being a "who is this candidate being evaluated on behalf of"
context rather than a raw tile coordinate). `param_2..param_4` are three bytes stashed into globals
`DAT_0030099E`/`DAT_0030099F`/`DAT_003009A0` — the last of these is the exact address this lab's
`QueryDataRange.java` warning header uses as its own worked example of the "6 real xrefs, reported
as 0" undercount bug (unrelated finding, coincidental address reuse as scratch memory — confirmed
via `getReferencesTo`, not the flagged script, per that script's own warning).

**Step 1 — build a clamped search box.** The candidate's X/Y/Z (via `FUN_0007A9D0`, called on
`param_1`) become the box centre. The box is **21 × 21 × 13 map cells** (`0x15`, `0x15`, `0xD`),
offset `11`/`11`/`7` cells below the centre before clamping, and clamped against the same three map
extent globals an already-bound function uses: `DAT_000E4C10`/`DAT_000E4C14`/`DAT_000E4C18` — B1
pass 1 §5 cites these exact three globals as the map-bounds check inside `FUN_0007B3DC` (the bound
fire-overlay function). Their reuse here, doing the same clamp-to-map-extent arithmetic, is
independent corroboration that `FUN_0007e600` operates in real battlescape tile space and that
these three globals really are map width/depth/height — not merely a repeated literal that happens
to match.

**Step 2 — scan all 60 unit slots.** The loop walks the full unit array (§2) top-down. For each
slot it requires, in order: slot valid (`≠ -1`); vitality (`+0x64 ≠ 0`); position inside the clamped
box on all three axes; and **either** same side as the candidate's context (`+0x1E` equality)
**or** a "detect regardless of side" flag at a fixed offset off the secondary 0x24E-stride array
(`+0x25E`, unresolved meaning — read as a per-unit "ignore faction for this check" bit, not
examined further). A cell index into a 3-D grid (`DAT_002FF180`, dimensions `0x15 × 0x1B9` per the
`IMUL EAX,EDX,0x15` / `IMUL EDX,EBP,0x1B9` in the raw listing) is computed from the qualifying
unit's position relative to the box; if that cell hasn't been visited yet this call
(`DAT_002FF180[cell] == 0`), a line-of-sight/path computation is triggered (`FUN_0001EA00`,
`FUN_0007C97C`) to populate it.

**Step 3 — the actual accumulation.** The populated cell's byte value (an LOS/path **class** code)
is read. If it's `4` or `6`, or if a caller-supplied bitmask (built from `param_2`/`param_3`/`param_4`
via `FUN_00049E10`) has the bit for that class set, the qualifying unit **penalises** the running
score:

```
score -= (DAT_001D3425[FUN_0008a364(unit) + path_distance] >> 0x18) * <factor>
```

`FUN_0008a364` (VA `0x8A364`, bound `0xE4E08`, page `0x7A363`, decompiled this pass) is **not** a distance
computation — it reads the qualifying unit's own field `+0x20` (unexamined; plausibly a facing or
direction index, since it feeds a table lookup alongside a path/distance value) and then searches a
global 20-byte-stride table (`DAT_001CA968`–`0x1CAE18`, ~55 entries) for an active "type `6`,
count > 0" entry whose owner pointer matches this specific unit; if one exists, its overridden value
is returned instead of the unit's own field. This reads as a **temporary per-unit status-effect
override** (the table shape — typed entries, a count/duration field, an owner pointer — matches a
generic "active effects" table more than anything cover-specific), consulted here to get an
effective facing/direction for the threat-weighting lookup. `DAT_001D3425` is then indexed by that
value plus the path/distance value, and its high byte (`>>0x18`) scaled by a per-unit multiplier
(`extraout_EDX_00`, origin not traced) is subtracted from the score. **The accumulator starts at `0`
and is only ever decremented.** The function returns that accumulator. **Lower (more negative) =
more exposed to qualifying threats; a score of exactly `0` means no qualifying unit was found in the
box at all.**

This is the metric. It is not "how solid is the wall next to this tile" (pass 1's, and the parity
guide's, working hypothesis) — it is **"how many hostiles/visible units, weighted by range and
line-of-sight class, can reach or see into the box around this candidate position."** Both are
legitimate implementations of a cover-quality metric; TACP's is threat-exposure-based, not
terrain-solidity-based. That is a substantive, checkable finding about the original's design, not
an assumption ported in from OpenApoc's existing prior art.

## 5. `FUN_0008c1fc` — candidate selection (the caller of §4)

**VA `0x8C1FC`. Bound-file `0xE6CA0`. Page-file `0x7C1FB`. Size 4340 bytes.** Called from both
branches of `FUN_0008f338`'s weapon-category code (ammo-present and ammo-fallback paths). Evaluates
a `switch` on a caller-supplied selector (`param_2+0x20`, values `0`/`1,2,3` (shared code)/`4`) and,
inside the `4` case, a *second* switch on `param_2+0x22` (`-1`/`0`/`1`/`2`) that determines which
**set** of candidates gets scored. Each candidate is evaluated by an identical pattern — compute a
position, guard it against the same kind of distance-from-threshold test used in §4's box clamp,
call `FUN_0007e600`, and if the returned score beats the running best (`if (best < score)`, a **max
comparison — this function keeps the *highest*, i.e. least-penalised, candidate**), overwrite the
output record's position and a small integer **type tag**.

Type tags observed, with the position source each is computed from (tag → source, confidence noted
where the real-world meaning is inferred rather than directly labelled in the binary):

| Tag | Position source | Reading (confidence) |
|---|---|---|
| `6` | The querying unit's own current position (`+0xA8/+0xAA/+0xAC`) | "Stay where I am" candidate — high confidence from the position source alone. |
| `1` | A unit referenced by `param_1[299]` (a stored unit-index field on the querying unit itself), read through the same `0x127`-stride position lookup `FUN_0008e694` uses for other-unit positions | Position of some specific unit the querying unit already tracks — most likely a squad-mate/rally point or its current target. Which of the two was not determined this pass. |
| `2` | A remembered point (`DAT_0030836C`/`DAT_00308368`/`DAT_00308364`, populated earlier in `FUN_0008c1fc`'s own setup code from the *nearest* thing found in an initial distance-ranked pre-scan) | Reads as "closest recently-noted danger/point of interest," but the pre-scan's own filter criteria were not traced — could be nearest visible enemy, nearest LOS block of a particular kind, or something else. Flagged, not resolved. |
| `3` | Up to four entries from a per-map table (`DAT_00136148`, 0x88-byte stride, gated on a per-entry `!=0` short at `+0x16`) capped to at most 4 candidates (`if (3 < DAT_00308360) DAT_00308360 = 4`) | Reads as **pre-authored, per-map "AI waypoint" positions** — the strongest candidate in this whole investigation for the map-part-adjacent per-tile metadata pass 1 §7 predicted ("candidates are any `unknown*` on the map-part struct that correlates with wall/solidity"). Not cross-checked against `tools/extractors/common/` structs this pass — an open thread, see §7. |
| `4`, `5`, `7`, `8`, `9` | A mix of a "home"-like point (`DAT_0030835C`/`FUN_0008D2F0`) and several further fixed globals (`DAT_00308330`, `DAT_00308324`, `DAT_00308300`) each gated by its own `!=0`/reachability pre-check | Read as a descending fallback chain — tried in order when the higher-priority candidates above are unusable (out of range or the pre-check fails). Consistent with "panic/regroup to a known-safe point when nothing better is in range," not examined field-by-field. |

**Net effect**: `FUN_0008c1fc` is a **"pick the least-exposed reachable destination from a fixed
menu of candidate types" selector**, and `FUN_0007e600` is the **exposure metric each candidate is
scored on**. This is the B1 cover mechanism at the structural level the parity guide asked for
("takes a unit pointer and a position/direction... scores or selects a tile") — except the
candidate set is a short, curated list (≤ roughly 9 named points) rather than every neighbouring
tile, and the score is threat-exposure rather than terrain-solidity.

## 6. Everything examined for a movement/potshot follow-through, and where it stopped

- **`FUN_0008f918`** (VA `0x8F918`, bound `0xEA3BC`) — called immediately after `FUN_0008c1fc` in
  both `FUN_0008f338` branches. Fully decompiled: iterates the 60-unit array checking a per-side
  `DAT_00293220`-based "already assigned an order this tick" flag table; returns whether *every*
  same-side unit already has one. A completion/short-circuit check, not part of the scoring itself.
- **`FUN_0009CAB4`** (VA `0x9CAB4`, bound `0xF7558`) — the "is this a usable, alive, correct-side
  unit" predicate reused by `FUN_0008F9E0`'s destination-sharing search; matches the vitality/side
  gates in §2's table exactly, corroborating those field readings independently.
- **`FUN_000A3250`** (VA `0xA3250`, bound `0xFDCF4`, page `0x9324F`) **/ `FUN_000A33A8`** (VA
  `0xA33A8`, bound `0xFDE4C`, page `0x933A7`) **/ `FUN_000A3500`** (VA `0xA3500`, bound `0xFDFA4`,
  page `0x934FF`) — the three functions `unit+0x220` dispatches to. Decompiled this pass: each
  computes a path via `FUN_000A3620` and queues a follow-path order
  (via the K1-documented `FUN_00021008` allocator), differing **only in the order type queued**:
  `0x18`, `0x19`, `0x1A` respectively. `FUN_000A3250`/`FUN_000A33A8` additionally upgrade to order
  type `0x25`/`0x26` when the path's destination matches a "currently selected/highlighted unit"
  global (`DAT_000E6D62`) — reads as a UI/player-directed-move variant, not an AI-only path.
  **This is not the kneel/prone answer the parity guide asked for** — none of these three touch a
  stance/posture field, and nothing in them or their raw listings references `MovementMode` (or
  this binary's equivalent). They select between three **locomotion styles** for the walk itself
  (plausibly walk/run/creep — order types `0x18`–`0x1A` were not traced to a consumer that would
  confirm this), consumed by whatever handles those order codes. `ai.txt`'s "Cautious prones,
  Normal kneels" claim remains prior-art-only after this pass, neither confirmed nor contradicted.
- **`FUN_0005BAD8` / `FUN_00063F44` / `FUN_00058FE8`** — the functions that translate
  `FUN_0008c1fc`'s winning candidate into an actual queued order (via the same `FUN_00021008`
  fixed-pool allocator K1 documents). Not decompiled this pass.
- **Order types queued in this cluster this pass**: `0x01`, `0x06`, `0x07`, `0x08`, `0x0C`, `0x11`
  (K1's cloak-order, unchanged), `0x14`, `0x1B`, `0x1D`, `0x23` (debug-only, gated on
  `DAT_0027A0EA`). K1's own order-type-`0x11` trace covers `FUN_0003D9E4`'s dispatcher for
  `0x01`–`0x13`; the higher codes (`0x14`, `0x1B`, `0x1D`, `0x23`) were not traced to a consumer
  this pass and may belong to a different dispatcher.

## 7. Explicitly open — what would still change this document

1. **Mode-gate location — narrowed, not closed.** No behaviour-mode byte gates §4/§5 anywhere in
   the traced call chain (§2's closing paragraph and the `+0x475` row). A wider xref pass this
   session (§2 table) turned up a **stronger, unexamined candidate**: `unit+0x21C`, read/written by
   20 functions including `FUN_0003D9E4` (the order-type dispatcher) and `FUN_0005BAD8` (an
   untraced result→order function in this very cluster) — far broader fan-out than `+0x475`'s 12,
   and touching the order-dispatch layer rather than staying inside the AI-think loop. **None of
   those 20 functions were decompiled** to see what the field's three observed branches (`0`/`<2`/
   `==2`, from `FUN_00066474`'s use of it) actually mean or where the value originates — this is
   the single highest-value next step for this row, not a finding. This pass also did not enumerate
   `FUN_0008a524`'s own callers (whether something above it gates *if* a unit's AI runs at all this
   tick), and did not test whether the mode changes `FUN_0007e600`'s box size / `FUN_0008c1fc`'s
   candidate menu rather than gating the call outright.
2. ~~`FUN_000A3250`/`FUN_000A33A8`/`FUN_000A3500` and the `0x10A345`/`0x10A0F0` writer check~~ —
   **completed this pass**, see §6 and the `+0x475`/`+0x21C`/`+0x220` rows in §2. Result: no
   stance/posture connection found; `+0x21C` (not `+0x220`) is now the better mode-field lead.
3. **`FUN_0008A364`/`DAT_001D3425`** — resolved to the extent decompiling `FUN_0008A364` allows
   (§4: a per-unit facing/direction value, possibly overridden by an active-status-effect table
   match, feeding a 2-D `DAT_001D3425[facing+distance]` lookup). The table's actual contents, the
   `+0x20` field's real meaning, and the scaling multiplier's origin remain untraced.
4. **Candidate types `1`, `2`, `4`, `5`, `7`, `8`, `9`** in §5's table — the position sources are
   confirmed from the raw listing; their real-world meaning (ally vs. target vs. squad rally vs.
   panic point) is inferred, not labelled in the binary, and is flagged at the confidence level
   stated in the table rather than asserted as fact.
5. **`DAT_00136148`'s per-map waypoint table** (§5, tag `3`) was not cross-checked against
   `tools/extractors/common/`'s map-part/`.SLS`-block structs. If this table is the same
   "AI-priority metadata" pass 1 §7 flagged as still-undecoded (`unknown02[116]` on the `.SLS` block
   struct), that would be a second, independent load-bearing finding — chase this first if
   continuing this row.

## 8. Mapping to OpenApoc (proposed, not implemented)

`UnitAIHelper::getTakeCoverMovement`
([unitaihelper.cpp:142](../../../game/state/battle/ai/unitaihelper.cpp#L142)) is the natural home
for a port of §4/§5's logic, but **this document does not recommend implementing it yet** — per
§7, the mode-gating question and the arrival-stance functions are open, and porting a threat-
exposure metric while guessing at the mode boundary would repeat exactly the invention risk pass 1
and this lab's prime directive warn against. If a future pass resolves §7 items 1–2, the shape to
port is: **evaluate a short, fixed menu of candidate destinations (not a full neighbour-tile
sweep), score each by counting nearby hostiles with qualifying LOS inside a large clamped box
around it, and move to the least-exposed one that is in range** — materially different from "search
adjacent tiles for the best solidity score," and OpenApoc's implementation should say so explicitly
if it ports this rather than presenting it as the tile-adjacency mechanic the parity guide
originally guessed at.

## Artifacts (lab only — not copied into this tree)

- `OpenApoc-og-research/scripts/QueryB1Pass2Entry.java` / `run_b1p2_entry.sh` — full decompile +
  listing for `FUN_0008f338`, `FUN_0008e694`, `FUN_00066474`; caller xrefs for the first two.
- `OpenApoc-og-research/scripts/QueryB1Pass2Fun8f9e0.java` / `run_b1p2_8f9e0.sh` — full decompile +
  listing for `FUN_0008f9e0`, `FUN_0008122c`, `FUN_0008c1fc`; callers of `FUN_0008f9e0`.
- `OpenApoc-og-research/scripts/QueryB1Pass2Scorer.java` / `run_b1p2_scorer.sh` — full decompile +
  listing for `FUN_0007e600`, `FUN_0009cab4`, `FUN_0008f918`.
- `OpenApoc-og-research/scripts/QueryB1Pass2Final.java` / `run_b1p2_final.sh` /
  `retry_b1p2_final.sh` — xrefs for `0x10a345`/`0x10a346`/`0x10a0f0`/`0x10a0ec`/`0x10a33c`, and
  decompile of `FUN_0008a364`, `FUN_000A3250`, `FUN_000A33A8`, `FUN_000A3500` (§6, §7.1 findings
  above).
- Logs: `export/b1p2_entry_stdout.txt`, `export/b1p2_8f9e0_stdout.txt`,
  `export/b1p2_scorer_stdout.txt`, `export/b1p2_final_stdout.txt`.

Note: this session hit `LockException` against the shared `OpenApocOG_TACP` project multiple times
from concurrent peer sessions (`RE-cloak`, `RE-hazard`, and others per this session's agent roster,
all working the same shared lab). Retried with backoff each time and eventually succeeded; no
destructive or forcing action was taken on the shared project. One retry loop was accidentally left
running as a stray duplicate background process for several minutes (interleaving its output with a
second retry loop into the same log file, which produced a garbled intermediate log) before being
caught and stopped (`TaskStop`); the final, clean run that produced the §6/§7.1 results above ran
alone.
