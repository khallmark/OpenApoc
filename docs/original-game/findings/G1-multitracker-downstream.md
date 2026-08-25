# G1 — MultiTracker downstream trace (equipment type `0x04`)

Agent: this session (G1 MultiTracker downstream). Lab: `OpenApoc-og-research`, project
`ghidra_projects/OpenApocOG_TACP.rep`, program `TACP.EXE` (non-4, CRC32 `0xfebbe39e`), loader
`-processor x86:LE:32:default -cspec gcc`. All addresses cited from the live Ghidra project via
`getReferencesTo`/raw-listing queries (`scripts/QueryFunctions.java`,
`scripts/DumpListingRange.java`, `scripts/QueryRangeXrefs.java`), never from a file-offset read of
the raw EXE, per this project's method rules. Where the decompiler and the raw instruction
listing disagreed, the raw listing is what is cited below — see "Corrections to the prior read"
for two cases where this mattered.

## Verdict

> **BOUND, and it is not a multi-target extension of the Motion Scanner mechanism.** MultiTracker
> gates a live, `type == 0x04`-tested code path (`FUN_000A3170`) that — once past a shared,
> ambiguous per-unit threshold gate — draws an **instantaneous snapshot** of nearby matching-id
> battle units' relative tile positions (height-encoded, no fading/decay) directly into the
> on-screen scanner-widget pixel buffer, once per screen-refresh cycle. This is a **different**
> mechanic from what the structurally parallel Motion Scanner path does in the same neighbourhood:
> Motion Scanner's own draw function does not touch unit positions at all — it blits a **live,
> separately-maintained, decaying recency heatmap** (0–14 levels, decremented every call, boosted
> by a per-pixel comparison of two mask tables) that is functionally very close to OpenApoc's own
> `BattleScanner::movementTicks`/`TICKS_SCANNER_REMAIN_LIT` fading-dot design. MultiTracker has no
> such decay: every draw call fully recomputes from current unit positions. OpenApoc currently
> treats `AEquipmentType::Type::MultiTracker` as fully inert (`case ... return false` in
> `BattleUnit::useItem`, `break` with no effect in `BattleView`'s explicit-use dispatch) — the
> original has a real, gated, per-hand-slot live display that nothing in the current C++
> implements.

## Correction to the prior read (important — changes what "candidate" means)

`B3-G1-wounds-gadgets.md`'s existing MultiTracker row describes `FUN_000A30C8`'s first parameter
as "a target/candidate reference" and frames `FUN_00017390` as "a position/candidate-list
builder" whose entries flow into that parameter. **Traced from the raw listing, this is a
decompiler artifact and the "candidate" framing does not hold.**

`FUN_00017390` (page-file `0x738f`, size 5150 bytes) does contain a big `switch`-driven loop that
converts a linked list of heterogeneous map objects into screen coordinates (`DAT_000e4d78`/`_7a`)
— that part of the prior description is accurate — but the MultiTracker/Motion Scanner check is
**not** wired to that loop or its list at all. It is called from exactly two fixed sites inside
`FUN_00017390`, both passing a **literal constant**, not a data pointer:

```
000173b8 XOR EAX,EAX          ; EAX = 0
000173c8 CALL FUN_000a30c8    ; site 1 — top of the function, before the map-object loop

00018786 MOV EAX,0x1          ; EAX = 1
0001878b CALL FUN_000a30c8    ; site 2 — after the map-object loop, in the function's tail block
```

Inside `FUN_000a30c8`, `MOV EDX,EAX` at entry (`0xa30cb`) makes this literal the value later
tested by `TEST EDX,EDX; JNZ 0xa311f` (`0xa3101`/`0xa3103`). So the branch the prior doc read as
"candidate present vs. absent" is actually **"is this the pre-pass (mode 0) or post-pass (mode
1) of this refresh cycle."** Mode 0 (top of the function, before the object loop) takes the
`FUN_00019A3C` branch; mode 1 (bottom, after the object loop) takes the `FUN_000A2F80` branch.
`FUN_00019A3C` (791 bytes) is a generic "register a clipped screen dirty-rectangle for redraw"
routine (bounds-checks against a `0x280×0x160` = 640×352 screen, pushes into a capped 200-entry
queue at `DAT_000e6c1c`/`DAT_000e6c20`) — not something that inspects unit data at all. Read
together: **mode 0 marks the scanner widget's screen area dirty; mode 1 repopulates its pixel
data.** There is no "candidate list" being filtered by MultiTracker ownership; there is one
wearer-resolution step (below) run twice per refresh with two different jobs.

This also means Q1 as posed ("what does the candidate-list builder build a list of") has no
literal answer — the premise doesn't hold. The real question that has an answer is what the
**mode-1 draw call** (`FUN_000A2F80`) produces, below.

## The `type == 0x04` gate (direct answer to Q2)

`FUN_000A3170` (page-file `0x9316f`) is a literal predicate on the general-equipment tables
established in `B3-G1-wounds-gadgets.md`/`K1-cloak.md`:

```c
undefined8 __regparm3 FUN_000a3170(short param_1, undefined4 param_2)
{
  uVar2 = 0;
  if (param_1 != -1 &&
      DAT_002b2854[param_1 * 0x12] == '\x02' &&                     // category == GENERAL
      DAT_002b2e7a[(byte)DAT_002b2855[param_1 * 0x12] * 0xc] == '\x04')  // type == MultiTracker
  {
    uVar2 = 1;
  }
  return CONCAT44(param_2, uVar2);
}
```

This predicate gates everything downstream. `FUN_000A31B4` (page-file `0x931b3`) wraps it,
resolving `param_1`'s hand-slot equipment-instance index from a caller-supplied unit pointer's
`unit+0x2AE` (hand 1) and, only if hand 1 doesn't match, `unit+0x2B2` (hand 2) — **short-circuiting
on the hand-1 match** (a `goto` past the hand-2 check; see "Correction 2" below for why this
matters). On a match it records **which hand** in `_DAT_002a17ac` (0 or 1) and returns true.
`FUN_000A30C8` (page-file `0x930c7`) is gated additionally by a global feature-flag word at
`DAT_000E6D40+2` (`!= 0`) — the same flag `B3-G1-wounds-gadgets.md` already established gates the
Motion Scanner sibling identically — and, only if `FUN_000A31B4` returned true, resolves the
"current wearer" unit pointer via `DAT_000E6D40+4 → table@0x1B24AA → unit index → 0x109ED0 +
index*0x49C` (the same 0x49C-stride battle-unit array `K1-cloak.md` already anchors from
`FUN_0003D9E4`). **If the unit does not have a MultiTracker equipped in either hand, or the
feature flag is off, neither `FUN_00019A3C` nor `FUN_000A2F80` is called at all** — the
`type == 0x04` test is a hard gate on whether this whole subsystem does anything this refresh.

There is one more gate ahead of the type test, shared identically with Motion Scanner (see
"Corrections to the prior read" §2): `*(short*)(unit+0x80) < *(short*)(unit+0x64)`. This is the
same pair of fields `B5-F1-K1-hazards.md` §5.1 already found and left explicitly unresolved — "an
accumulator at `unit+0x80`, capped at `0xFF`... a per-unit threshold at `unit+100 (0x64)`...
resembl[ing] a stun/incapacitation accumulator at least as plausibly as a fire counter." This
document adds no new information about what that field means; it only confirms MultiTracker's (and
Motion Scanner's) wearer-check reuses it as a precondition ("skip entirely if this accumulator has
already reached its threshold"). **Do not read this as a scanning-specific "detected count vs.
max" mechanic — that reading is not established, and per the project's own prior finding on this
exact field, a stun/fire-accumulator reading is at least as likely.**

## What the mode-1 draw call actually produces (answer to Q1/Q3)

`FUN_000A2F80` (page-file `0x92f7f`), called only when the wearer check above passed:

1. Calls `FUN_000275D5(x0=hand*0x21F+8, x1=hand*0x21F+89, y0=8, y1=89, fillByte=0)` — confirmed
   from `FUN_000275D5`'s own raw listing (a hand-written `PUSHAD`/`STOSB`/`STOSD` rectangle-fill
   primitive, `RET 0x4` epilogue popping its one stack-passed fill-byte argument) to be a genuine
   81×81-pixel (`0x51×0x51`, matching `0x28`/`0x29` = 40/41 used below) **clear-to-zero** of a
   region inside the buffer at `*(int*)DAT_000E6080` — confirmed a real pointer variable, not a
   byte — at pixel offset `hand*0x21F + 0x1408` (`0x1408 = 8*640+8`, i.e. the same 81×81 block the
   fill call just cleared).
   - **Correction on tracing this**: the decompiler renders the subsequent per-unit write's base
     address as a literal `0`, which would target unmapped memory (Ghidra shows no memory block
     covering low addresses at all — `.object1` starts `0x10000`, `.object2` starts `0xE0000`).
     This is wrong. `FUN_000275D5`'s `RET 0x4` pops exactly the one stack dword it consumes as an
     argument (the fill byte, pushed immediately before the call), which shifts what the caller
     wrote to a *different* reserved stack slot back into the position later read as the base
     pointer — i.e. the buffer pointer computed above genuinely is what later code uses. Traced
     from `FUN_000275D5`'s raw epilogue, not inferred.
2. Walks the full battle-unit array (`0x109ED0`..`0x11AEC4`, stride `0x49C` — 60 slots, the same
   array `K1-cloak.md` anchors) and, for every live slot (`!= -1` sentinel) whose `unit+0xA8` low
   word equals the **wearer's own** `unit+0x20` word, and whose tile position lies within `±40`
   tiles (`X`/`Y` fields at `unit+0xAA`/`unit+0xAC`, read as the high half of a 16.16-style packed
   dword sharing the low half with the `+0x20`-compared tag — confirmed by the fact that the same
   `unit+0xA8` dword is read twice in this function, once as a plain word for the equality test
   and once shifted for the X coordinate, so both readings are deliberate, not a decompiler
   coincidence), stamps a byte `0x10 + clamp(|Δheight|)` at the corresponding cell of the cleared
   81×81 block (height from `unit+0xAE`, i.e. the same dword-packed-field pattern one step further
   in the struct). The wearer's own cell is stamped unconditionally (`Δheight = 0`).
3. Draws one clipped sprite via `FUN_00027F6B` (a generic RLE sprite blitter, confirmed from its
   own decompile — clips against a screen rectangle in `DAT_000E4D74`/`_76`/`_78`/`_7A`) — read as
   the widget's frame/icon graphic, not scan data.

**What `unit+0x20`/`unit+0xA8` actually mean is not established by this session.** The natural
reading of an equality test between two per-unit tag fields, feeding a "who gets drawn near me"
filter, is squad/faction membership — but neither offset is characterized anywhere in `K1-cloak.md`
or `B5-F1-K1-hazards.md`, and the one nearby field that *is* characterized (`unit+0x26`, "a
per-unit ID field" compared against a global cursor in the fire-damage trace) is a different
offset with a different, non-faction role. **Do not cite "same side/team only" as bound** — cite
only "an equality test between an uncharacterized tag field on the wearer and the same-offset
field on each candidate", with squad/faction membership as the plausible-but-unconfirmed reading.

**Bound answer**: MultiTracker's on-screen effect, when the gates pass, is a **snapshot**, redrawn
from scratch every refresh cycle, of nearby matching-tag units' relative positions (with height
encoded as a small offset), written to a fixed 81×81-pixel widget slot selected by which hand holds
the item. There is no accumulation, no fade, no "ticks since last seen" state anywhere in this
function — contrast with Motion Scanner below.

## Corrections to the prior read, cont'd — Motion Scanner's parallel path is a different mechanic, not the same one MultiTracker extends

`B3-G1-wounds-gadgets.md` calls `FUN_000A30C8` "structurally identical" to Motion Scanner's
`FUN_000A2AE0`/`FUN_000A2CDC`/`FUN_000A2C98` chain, which is true of the *plumbing* (confirmed this
session — `FUN_000A2CDC` is byte-for-byte the same shape as `FUN_000A31B4`, same `unit+0x80 <
unit+0x64` gate, same hand-1-short-circuit, own which-hand global `_DAT_002A17A0`, own stride
`0x21E` instead of `0x21F`) but **not of the draw content**:

- `FUN_000A2BC8` (Motion Scanner's mode-1 draw, page-file `0x92bc7`) does **not** touch the
  battle-unit array at all. It reads a `0x29×0x29` (41×41, 1681-byte) table at `DAT_003345BD` and
  converts each cell to a pixel pair via `0xF4 - value`, clamped `≥0xE6` (16 distinct output
  levels), writing mirrored top/bottom into the same kind of `DAT_000E6080`-based buffer at
  `hand*0x21E + 0x1408`.
- That table is **not static art** — an `getReferencesTo`/range-xref sweep (`0x3345BD`..`0x334C4E`,
  1681 bytes) found a live writer, `FUN_000A2C44` (page-file `0x92c43`, called from
  `FUN_000A2D84` — one of the other confirmed-live Motion Scanner call sites), which for every
  cell: computes `delta = |DAT_00334C4E[i] - DAT_00333F2C[i]|` (comparing two other tables),
  **adds** it into `DAT_003345BD[i]`, clamps to a max of `14`, and otherwise **decrements the cell
  by 1 per call** when nonzero. This is a genuine recency/intensity accumulator with **15 levels
  and continuous decay** — mechanically the same shape as OpenApoc's own
  `BattleScanner::movementTicks` fading-dot design (`battlescanner.h`'s own comment: "we recognize
  16 different colors"), just implemented as a pixel-mask diff rather than a per-tile unit lookup.

So the two items' "scanner" widgets are populated by **two different kinds of state entirely**:
Motion Scanner shows a decaying trail derived from comparing two mask/pattern buffers (never
directly reading unit position data in the draw step traced here), while MultiTracker shows an
un-decayed, instantaneous redraw of live unit positions filtered by a tag-equality test. **There is
no evidence in this trace that MultiTracker is "the same detection mechanism, extended to more
targets."** They are two distinct display mechanics sharing identical widget-slot plumbing
(feature flag, wearer resolution, hand-indexed screen offset, pre-pass/post-pass call shape) — not
one mechanism parameterized by item type.

One more correction: `_DAT_002A17AC` is a **single global**, overwritten each pass, and
`FUN_000A31B4` short-circuits on the hand-1 match without checking hand 2 at all when hand 1
matches. **Do not claim "wearing two MultiTrackers gives two independent overlays"** — there is one
overlay slot per wearer-resolution pass, and which of the two on-screen positions (`hand*0x21F`) it
lands in depends on which hand won the (short-circuited) check.

## Mapping to OpenApoc

- `game/state/rules/aequipmenttype.h:50` declares `AEquipmentType::Type::MultiTracker`.
  `game/state/battle/battleunit.cpp` (`BattleUnit::useItem`) and
  `game/ui/tileview/battleview.cpp` both currently treat it as a no-op alongside the confirmed-dead
  `VortexAnalyzer`/`StructureProbe`/`AlienDetector` — that grouping is now wrong per this trace;
  MultiTracker has a real, gated, live path in the original that those three do not.
- `game/state/battle/battlescanner.h`/`.cpp` (`BattleScanner`, `MOTION_SCANNER_X/Y = 38×39`,
  `TICKS_PER_SCANNER_UPDATE`, `TICKS_SCANNER_REMAIN_LIT`) already implements something structurally
  close to what this session found for **Motion Scanner** specifically — a fading per-cell
  recency map, all units (not filtered by side) stamped and left to decay — which is a reasonable
  parity target for Motion Scanner's `FUN_000A2C44`/`FUN_000A2BC8` pair (note OpenApoc's current
  `BattleScanner::update` does **not** filter by faction, unlike the tag-equality filter found on
  MultiTracker's path — another point of difference between the two items worth preserving if
  MultiTracker is ever implemented separately from `BattleScanner`).
- Nothing currently in OpenApoc corresponds to MultiTracker's actual traced behaviour (an
  instantaneous, non-decaying, tag-filtered position snapshot). If this is implemented, it should
  **not** reuse `BattleScanner`'s decay/fade model — the original's MultiTracker draw path has no
  decay state at all, it recomputes from scratch every refresh.

## What is NOT claimed

- The exact meaning of `unit+0x20` (wearer tag) / `unit+0xA8` low word (candidate tag) — the
  equality test is confirmed real (both fields are read as deliberate standalone comparands, not a
  decompiler coincidence), but "same side/team" is the plausible, not the established, reading.
- The exact meaning of `unit+0x80`/`unit+0x64` (the shared threshold precondition) — already left
  open by `B5-F1-K1-hazards.md` §5.1 (fire/stun accumulator vs. something else); this session found
  no new evidence either way and does not upgrade that ambiguity.
- What `FUN_00027F6B`'s second call in Motion Scanner's path (indexed through `unit+0x109FBC`, a
  table/offset not otherwise characterized this session) draws — established only that Motion
  Scanner's registration calls this generic sprite blitter twice where MultiTracker's calls it
  once; the extra sprite's identity was not traced.
- Whether `DAT_000E4D58`/`DAT_0027A805` (the two latches gating `FUN_00017390`'s top/bottom
  MultiTracker calls) tie this to a specific game screen (tactical view vs. some other UI) or
  cadence (once per frame vs. once per turn) — the six call sites of `FUN_00017390` were not
  individually traced for their invocation context this session.
- Any pixel-to-tile scale claim beyond "the fill/scan window is `0x28`/`0x29` (40/41) cells wide in
  each direction, i.e. an 81×81 tile window" — this is a fixed constant in the code, cited as-is,
  not converted to a claim about OpenApoc's `MOTION_SCANNER_X/Y = 38×39` being right or wrong for
  MultiTracker specifically (no OpenApoc equivalent exists to compare against).

## Addresses cited (TACP.EXE non-4, page-file offsets)

| Function | VA | page-file | Role |
|---|---|---|---|
| `FUN_000A3170` | `0xA3170` | `0x9316F` | `type == 0x04` predicate |
| `FUN_000A31B4` | `0xA31B4` | `0x931B3` | wearer hand-slot check, sets `_DAT_002A17AC` |
| `FUN_000A30C8` | `0xA30C8` | `0x930C7` | feature-flag gate, mode dispatch (0=clear-rect, 1=draw) |
| `FUN_000A2F80` | `0xA2F80` | `0x92F7F` | MultiTracker draw: clear + live unit-position stamp |
| `FUN_00017390` | `0x17390` | `0x738F` | shared per-refresh caller (map-marker loop + 2 mode calls) |
| `FUN_00019A3C` | `0x19A3C` | `0x9A3B` | generic dirty-rect queue (mode-0 branch) |
| `FUN_000275D5` | `0x275D5` | `0x175D4` | rectangle-fill primitive (`RET 0x4`) |
| `FUN_00027F6B` | `0x27F6B` | `0x17F6A` | generic clipped sprite blitter |
| `FUN_000A2AE0` | `0xA2AE0` | `0x92ADF` | Motion Scanner analog of `FUN_000A30C8` |
| `FUN_000A2CDC` | `0xA2CDC` | `0x92CDB` | Motion Scanner analog of `FUN_000A31B4` (byte-identical shape, confirmed this session) |
| `FUN_000A2C98` | `0xA2C98` | `0x92C97` | `type == 0x01` predicate (Motion Scanner) |
| `FUN_000A2BC8` | `0xA2BC8` | `0x92BC7` | Motion Scanner draw: blits live recency table, no unit reads |
| `FUN_000A2C44` | `0xA2C44` | `0x92C43` | maintains Motion Scanner's decaying recency table |

Globals: `_DAT_002A17AC` (MultiTracker which-hand), `_DAT_002A17A0` (Motion Scanner which-hand),
`DAT_000E6080` (screen/overlay buffer base pointer), `DAT_000E6D40` (+2 feature-flag word, +4 unit
index), `0x1B24AA` (unit-index lookup table), `0x109ED0` (battle-unit array base, stride `0x49C`),
`DAT_003345BD` (Motion Scanner live recency table, 1681 bytes), `DAT_00333F2C`/`DAT_00334C4E`
(the two tables Motion Scanner diffs to feed the recency table).
