# B5 · Entropy Enzyme — is overlay type 1 or type 3 the enzyme?

## Verdict: NOT BOUND

Type 1 vs. type 3 (which one is Entropy Enzyme, which one is Gas/Smoke) is **not recoverable**
from this session's evidence, and the evidence gathered argues this may not be a small gap to
close later — it is a structural one. Every consumer of the overlay's 2-bit type field found in
the binary (17 call sites across 11 functions, counting both this session's sweep and the prior
session's) treats type 1 and type 3 **identically**, differing only in the literal type constant
(`1`/`0x40` vs. `3`/`0xC0`). Fire (type 2) is consistently the one singled out for special
handling. No armour-vs-health split, no LOS/vision split, and no stage/lifetime split between 1
and 3 was found anywhere. The producer side (where a type gets chosen and written) was traced
three call-frames deep into a large AI weapon-use decision routine, where the trail runs into a
struct whose fields are not yet identified — a concrete, narrow next step, not a dead end, but not
something this session closed.

Two things **are** newly bound this session, independent of the 1-vs-3 question:

1. **The prior session's "`DAT_003009a0` has zero xrefs / no writer" claim is false**, with a
   named root cause in the query tooling (§1). Six real xrefs exist: three writers, three readers.
2. **`DAT_003009a0` is not the overlay type field.** It is a broader 0–6 blast/effect-kind
   selector; only its low four values (0/1/2/3) happen to reach the four overlay encoders (§2).

Do not assign Enzyme to type 1 or type 3 on the strength of anything in this document or its
predecessor. See §7 for the exact remaining next step if someone picks this back up.

---

## 0. Binary, environment, method

`OpenApoc-og-research/canonical/TACP.EXE` — the extractor-canonical **non-4** ISO build.
CRC32 `0xfebbe39e` (re-verified this session via `zlib.crc32`, matches
`B5-F1-K1-hazards.md`'s citation exactly — same file). Ghidra project
`ghidra_projects/OpenApocOG_TACP.rep`, queried `-noanalysis -process TACP.EXE` (reused the
existing analysis, nothing re-analyzed or modified). All new `.java` query scripts used this
session were written to a scratch directory outside the research lab and referenced via an
additional `-scriptPath`; nothing was written into `OpenApoc-og-research/scripts/` or
`OpenApoc-og-research/export/`. The lab's own `QueryFunctions.java` and `DumpListingRange.java`
were reused read-only (invoked, not edited) with `-log` redirected to the scratch directory.

**File-offset delta reconfirmed.** Every function this session where `QueryFunctions.java`'s
signature-match (`boundFileOffset`) succeeded reproduced `.object1`'s previously-confirmed
`file = VA + 0x5AAA4` delta exactly (10 more independent confirmations: `0x49D60→0xA4804`,
`0x655D0→0xC0074`, `0x7B918→0xD63BC`, `0xB8EBC→0x113960`, `0x7BD8C→0xD6830`, `0x7D2C4→0xD7D68`,
`0x7D350→0xD7DF4`, `0x7E600→0xD90A4`, `0x7E958→0xD93FC`, `0x7C97C→0xD7420`). Functions below marked
**derived** got no independent signature match this session (`bound_file = -1`); their file
offsets are `VA + 0x5AAA4` by the same delta, not independently confirmed:
`FUN_00073C98→0xCE73C`, `FUN_0007453C→0xCEFE0`, `FUN_000758B0→0xD0354`, `FUN_000762F0→0xD0D94`,
`FUN_0007CCE8→0xD778C`, `FUN_000584D0→0xB2F74`, `FUN_0003D9E4→0x98488`.

---

## 1. Methodology correction: the prior "zero xrefs" claim on `DAT_003009a0` is wrong

`B5-F1-K1-hazards.md` §3.4 states: *"A full-executable scan for any instruction with `0x3009A0`
as a literal operand returned zero hits ... the most likely explanation is that this is not a true
fixed-address global."* That scan was `QueryDataRange.java`, and it only inspects operand objects
of type `Scalar` (`OpenApoc-og-research/scripts/QueryDataRange.java`, the `instanceof Scalar` guard
around its whole match loop). An x86 direct absolute-memory operand — `MOV byte ptr
[0x003009a0],CH` — is encoded as an `Address`-typed operand object in Ghidra's model, not a
`Scalar`. The scan structurally cannot see this entire class of reference, on any address, not
just this one. This is a transferable finding for the rest of this project: **`QueryDataRange.java`
undercounts. Use `getReferencesTo(Address)` for a definitive xref check on a specific global**, not
a `Scalar`-operand sweep.

Re-run with the correct API (`getReferencesTo`, scratch script `QueryDat3009a0.java`) against
`DAT_003009a0` (VA `0x3009A0`) found **6 real references**, all in `.object1`:

| VA | Direction | Instruction | Function | File offset |
|---|---|---|---|---|
| `0x7D3DF` | WRITE | `MOV byte ptr [0x3009a0],CH` | `FUN_0007D350` | `0xD7E83` |
| `0x7D1D3` | READ | `CMP byte ptr [0x3009a0],0x4` | `FUN_0007CCE8` | `0xD7C77` (derived) |
| `0x7D256` | READ | `MOV BH,byte ptr [0x3009a0]` | `FUN_0007CCE8` | `0xD7CFA` (derived) |
| `0x7D732` | READ | `MOV AL,[0x3009a0]` | `FUN_0007D67C` | `0xD81D6` |
| `0x7E62A` | WRITE | `MOV [0x3009a0],AL` | `FUN_0007E600` | `0xD90CE` |
| `0x7E985` | WRITE | `MOV [0x3009a0],AL` | `FUN_0007E958` | `0xD9429` |

`FUN_0007D67C` is the already-known placement-dispatch switch (`B5-F1-K1-hazards.md` §3.4). The
other three functions (`FUN_0007D350`, `FUN_0007E600`, `FUN_0007E958`) were **not examined in the
prior session** — finding them is the direct payoff of fixing the scan.

---

## 2. `DAT_003009a0` is a 7-way blast/effect-kind selector, not the overlay's 2-bit type field

The dispatch in `FUN_0007D67C` (VA `0x7D67C` / file `0xD8120`, signature-confirmed) reads:

```
0007d732  MOV AL,[0x003009a0]
0007d73d  CMP AL,0x6
0007d73f  JA  0x0007e5f3          ; default / out-of-range
0007d745  AND EAX,0xff
0007d74a  JMP dword ptr CS:[EAX*0x4 + 0x7d660]
```

`CMP AL,0x6` bounds the selector at **0–6** — seven slots — not the overlay byte's 0–3. Dumped the
raw jump table at file `0xD8104` (VA `0x7D660`, scratch script `DumpJumpTable7d660.java`, 7 dwords
per the `CMP...0x6` bound, plus one extra slot read as a sanity check that it is garbage/out of the
table):

| slot | target VA | resolves into |
|---|---|---|
| 0 | `0x7D752` | `FUN_0007D67C` case 0 body |
| 1 | `0x7D9B3` | `FUN_0007D67C` case 1 body |
| 2 | `0x7DAE1` | `FUN_0007D67C` case 2 body |
| 3 | `0x7D880` | `FUN_0007D67C` case 3 body |
| 4 | `0x7DE22` | `FUN_0007D67C` case 4 body |
| 5 | `0x7E4C8` | `FUN_0007D67C` case 5 body |
| 6 | `0x7E5F3` | **same address as the out-of-range default** — case 6 is a no-op |

So the selector's practically-live range is 0–5, six real bodies. Decompiling all six (already
partly done in `B5-F1-K1-hazards.md` §3.4; re-verified and extended this session):

- **Case 0**: map-cell loop, writes the overlay via `FUN_0007AD90` (a fourth, previously
  undocumented encoder — a "clear" write, mask `0`) whenever a blast-intensity byte exceeds `0x18`.
  No unit-damage call.
- **Case 1**: same shape, encoder `FUN_0007ADC8` (the known type-1 overlay encoder, mask `0x40`).
  No unit-damage call.
- **Case 2 (fire)**: same map-cell loop, encoder `FUN_0007ADAC` (mask `0x80`), **plus** a second
  loop over the unit table (`0x109ECE`–`0x11B360`, stride `0x24E`) that calls `FUN_0007BCB8` and
  conditionally `FUN_00060540` (the generic damage/resistance function bound in
  `B5-F1-K1-hazards.md` §5.1) — this is fire's direct-hit unit damage.
- **Case 3**: same shape as case 1, encoder `FUN_0007ADE8` (mask `0xC0`, the known type-3 encoder).
  No unit-damage call.
- **Case 4**: a large terrain-destruction pass (calls `FUN_0007ADF0`/`FUN_0007AD7C`/`FUN_0007AD90`
  over a 3-axis neighbour-block-tracking pattern using `DAT_000E4C10/14/18`, which read as map
  X/Y/Z dimension bounds), **then** a unit-damage loop (`FUN_0005F860`, `FUN_00097108`,
  conditionally `FUN_00060540`), **then** a third loop over the ground-item array at
  `DAT_000EA2BE` (stride `0x30`) that reduces a per-item durability-like field and calls
  `FUN_000598D4` (the F1 "type-4 doodad on burn-out" allocator) on zero. This case writes **no**
  overlay byte at all — it is a kinetic/HE explosion case, structurally distinct from 0/1/2/3.
- **Case 5**: unit-damage loop only (`FUN_00060540`, gated by the same map-part checks as case 4).
  Also writes no overlay byte.

**This settles the question the prior session left open at §3.4**: `DAT_003009a0` was correctly
identified as "the dispatch variable that selects a type before calling the placement routine," but
it is not literally the overlay's type field — it is a superset selector for `FUN_0007D67C`'s own
six blast-effect kinds, of which values 1 and 3 happen to be the ones that also write the overlay
byte with the type-1/type-3 masks. This distinction matters for anyone tempted to treat
`DAT_003009a0`'s producer-side literal constants as directly enumerable "hazard types" — they are
not; 4 and 5 are real, separate, non-overlay explosion behaviours sharing the same selector byte.

---

## 3. Universal-symmetry sweep: nothing found distinguishes type 1 from type 3

Re-ran the existing `QueryOverlayTypeDispatch.java` scan (`SAR reg,0x6` followed within 10
instructions by `CMP reg,{0,1,3}` — i.e. every place in the binary that extracts the overlay's
top-2-bit type field and compares it against a non-fire value) against the full `.object1`
listing. It reproduces the 17 hits already on record in
`export/query_overlay_type_dispatch.log` (11 distinct functions), of which **7 were not examined
by the prior session**. All seven were decompiled and read this session; none discriminates 1 from
3:

| Function | VA / file | What it does with type 1/3 | Fire (type 2) treatment |
|---|---|---|---|
| `FUN_00049D60` | `0x49D60` / `0xA4804` (confirmed) | A per-tile "traversal cost" function gated by a caller-supplied bitmask (`in_CX` bit per type). Types 1 and 3 both call their respective decode function (`FUN_0007ADB0`/`FUN_0007ADD0`) then divide the result by the **same constant, 3**. | Own branch: calls `FUN_0007AE24` (a new, undocumented fire-specific getter) and divides by 2 instead — a different divisor and a different callee. |
| `FUN_00073C98`, `FUN_0007453C`, `FUN_000758B0`, `FUN_000762F0` | `0x73C98`/`0xCE73C`, `0x7453C`/`0xCEFE0`, `0x758B0`/`0xD0354`, `0x762F0`/`0xD0D94` (all four **derived**) | Four near-identical large functions (2177–2408 bytes), reading the same per-map-part veto-lookup family already bound in `B5-F1-K1-hazards.md` §2.2 (`FUN_0001EA70`/`FUN_0001EAB8`/`FUN_0001EB00`/`FUN_0001EB48`). Each has a 3-case `switch` on the overlay type with cases **0, 1, 3 only** — type 1 and type 3 both call their decode function then shift the result left by the **same amount, 2**, into a running move-cost accumulator (`DAT_000E6CB6`). | **No case 2 exists in this switch at all** — fire is entirely absent from this cost-accumulation path, not merely handled differently. Reads as pathfinding/LOS-edge-cost code (direction names aside, the shape matches "can a unit path through/see past this tile, and at what cost"). |
| `FUN_0007B918` | `0x7B918` / `0xD63BC` (confirmed) | Reads the overlay byte directly from a computed tile address, masks to the stage (`& 0x3F`) for cases 0/1/3 identically, and applies the **same** gate (`(stage>>3) != 7`) before calling `FUN_000C5252`/`FUN_00022F34`/`FUN_00027F6B`. Reads as an AI hazard-avoidance/threat-cost evaluator (same call shape reused across cases). | Own branch: re-fetches the tile, checks `>>6==2` again, calls `FUN_0007AD94` (decode) then `FUN_0007AE18` (the fire-only 27-byte power table, already bound), and clamps differently (max `0xB` instead of the `!=7` gate). |
| `FUN_0007BD8C` | `0x7BD8C` / `0xD6830` (confirmed this session — was **derived** in the prior doc, now signature-confirmed) | Already documented in `B5-F1-K1-hazards.md` §3 as the per-tile hazard-tick dispatcher; re-confirmed it has **three** separate `SAR..0x6`/`CMP` sites (two check `==3`, one checks `==1`), all feeding the same `FUN_0007BCB8`/`FUN_00060540` call pair with no branch-specific difference visible in the portion read. | Own dedicated branch calling `FUN_0007AD94`/`FUN_0007AE18` (fire's own power lookup), same as elsewhere. |

Combined with the already-bound decode triplet (`FUN_0007ADB0`/`FUN_0007ADD0`, identical shape),
encode triplet (`FUN_0007ADC8`/`FUN_0007ADE8`, identical shape, differ only by mask), and the
stage-advance pair (`FUN_0007B2B4`/`FUN_0007B348`, identical shape, differ only by the type
constant and mask) from the prior session, **every single reader of the overlay's type field found
in this binary — eleven functions, seventeen call sites — treats type 1 and type 3 identically**,
and singles fire out consistently. This is a strong structural result in its own right: whatever
distinguishes Enzyme from Gas/Smoke in the shipped game, it is **not encoded anywhere in how the
overlay byte itself is consumed**. The distinction, if it exists as a bound value at all, has to
live entirely on the producer side (which literal reaches `1` vs `3`), not on any consumer path.

---

## 4. Producer-side trace: as far as this session could take it

### 4.1 The chain from `FUN_0007D67C` back through three call frames

`FUN_0007D67C`'s selector is set by its caller, not computed internally (§2). Chasing writers:

- **`FUN_0007D2C4`** (VA `0x7D2C4` / file `0xD7D68`, confirmed) is `FUN_0007D67C`'s **only**
  caller (one call site, `0x7D342`). It first calls `FUN_0007D350`, then calls `FUN_0007D67C`
  immediately after, with no intervening write to `0x3009A0` — the decompiler failed on this small
  function (`Low-level Error: Cannot properly adjust input varnodes`), but the raw listing shows no
  `MOV [0x3009a0],...` inside it. So the value `FUN_0007D67C` reads was set by `FUN_0007D350`.
- **`FUN_0007D350`** (VA `0x7D350` / file `0xD7DF4`, confirmed) writes `DAT_003009A0 = param_5`
  directly from its incoming parameter — a pure passthrough, not a lookup against any local table.
  Its own body also performs a Bresenham-style tile-lighting pass (§4.3) using the two sibling
  globals `DAT_0030099E`/`DAT_0030099F`, unrelated to the overlay write.
- `FUN_0007D2C4`'s raw listing shows the byte that becomes `FUN_0007D350`'s `param_5` is read once
  from `FUN_0007D2C4`'s own incoming stack frame (`MOV AL,byte ptr [ESP + 0x14]`) and pushed twice
  in a row with no computation between the two pushes — i.e. `FUN_0007D2C4` receives this byte as
  one of *its own* parameters, unmodified.
- `FUN_0007D2C4`'s only caller is **`FUN_000584D0`** (VA `0x584D0` / file `0xB2F74`, **derived**,
  1742 bytes), which has **5** separate call sites into `FUN_0007D2C4`. All five pass the same
  4-field pattern read off an in-flight-explosive tracking struct at `DAT_001C6F70`
  (`pbVar13+0xE`, `pbVar13+0x14` for X/Y position, `*pbVar13` for one byte, `pbVar13[2]` for
  another) — never a literal constant. `*pbVar13` (the struct's own byte 0) is the strongest
  candidate for the byte that ultimately becomes `DAT_003009A0`, based on argument-position
  matching against `FUN_0007D350`'s parameter list, but this specific register/stack slot mapping
  through `FUN_0007D2C4`'s prologue was not proven bit-for-bit this session — flagging this as an
  inference, not a confirmed link.

### 4.2 Where the struct's own byte 0 comes from

`DAT_001C6F70` (the in-flight-explosive struct array `FUN_000584D0` iterates) has **confirmed
stride `0x1A`** (26 bytes) and a `0xFF` sentinel at byte 0 marking a free slot (established by
directly disassembling its allocation site — 14 different functions touch this array in total;
`getReferencesTo` on `0x1C6F70`, scratch script `QueryStructOrigin.java`). Its one clear
**creation** site (the only place found that writes byte 0 coming from something other than the
`0xFF` free-sentinel or `0x1C6F70` itself) is inside a large, previously-unexamined AI routine:

```
FUN_0003D9E4 (VA 0x3D9E4 / file 0x98488, derived), starting at VA 0x3EAE1 / file 0x99585:
  MOV EBX, 0x1c6f70            ; scan for a free (0xFF) slot, stride 0x1A confirmed by ADD EBX,0x1a
  ...
  MOV AL, byte ptr [EDI + 0xe] ; <-- becomes the new struct's byte 0 (file 0x995AC)
  MOV byte ptr [EBX], AL       ; <-- write into the new slot                (file 0x995AF)
  MOV AL, byte ptr [EDI + 0x6] ; -> struct byte 1
  MOV AL, byte ptr [EDI + 0x5] ; -> struct byte 2 (the "radius" divisor read in FUN_000584D0)
  MOV AX, word ptr [EDI + 0x8] ; -> struct word at +4
```

This is the actual origin: whatever `[EDI+0xE]` is, its value becomes (after the `FUN_000584D0` →
`FUN_0007D2C4` → `FUN_0007D350` passthrough chain above) `DAT_003009A0`. The read of `[EDI+0xE]`
itself is at VA `0x3EB08` / file `0x995AC`; the write of that byte into the new struct's byte 0 is
at VA `0x3EB0B` / file `0x995AF` (both derived, same delta). **`EDI`'s own identity was not
resolved this session.** `FUN_0003D9E4` is 9598 bytes and reads as a large unit/AI action-decision
routine (it iterates an order queue at `DAT_0011F118`, stride `0x16`, with a `switch` on task-type
bytes `1`/`2`/... — general AI turn processing, not hazard-specific). `EDI` is used earlier in the
same block for an unrelated lookup (`CMP DX,word ptr [EAX*0x2 + 0x1B24AC]`) and its assignment lies
outside the ~1500 bytes read backward from the creation site (no `MOV EDI,...`/`LEA EDI,...` found
in `0x3E600`–`0x3E905`). Whether `[EDI+0xE]` is a copy of an
item/ammo catalog "damage type" field, a locally-synthesized AI attack-descriptor byte, or
something else entirely is **open**. This is the single most concrete next step for whoever
resumes this row: find `EDI`'s assignment (likely earlier in `FUN_0003D9E4`, or passed in as one
of its own parameters — its callers were not enumerated this session either) and identify the
5/6-byte struct it points into.

### 4.3 Ruled out: `FUN_0007E600` / `FUN_0007E958` are a separate lighting subsystem, not hazard placement

The other two writers of `DAT_003009A0` found in §1 looked, at first pass, like a second hazard
family — both iterate the unit table with the exact 3D bounding-box test pattern seen in
`FUN_0007D67C`'s unit loops, and both compute a subtraction against a per-unit field that read, on
first glance, like an armour/health tally. Checked properly before trusting that read (per this
project's own rule about not trusting decompiler pointer arithmetic where a local array pointer is
reassigned mid-loop, which is exactly what happens here — `piVar8 = piVar8 + -2` inside the loop
before the suspect `+0x48`/`+0x24`/`+0x28` reads, making the decompiled field access unreliable
without the raw listing, which was not pursued further once the functions were ruled out
structurally):

- Neither `FUN_0007E600` (VA `0x7E600` / file `0xD90A4`, confirmed, 14 call sites across 3 callers)
  nor `FUN_0007E958` (VA `0x7E958` / file `0xD93FC`, confirmed, 1 caller) calls `FUN_0007D67C` or
  any of the four overlay encoders (`FUN_0007AD90`/`ADC8`/`ADAC`/`ADE8`). Their callee lists are
  identical to each other: `FUN_000C4C20`, `FUN_0008A364`, `FUN_0007A9D0`, `FUN_0001EA00`,
  `FUN_0007C97C`, `FUN_00049E10` (`FUN_0007E600` additionally calls `FUN_0007C97C`'s sibling paths
  the same way).
- **`FUN_0007C97C`** (VA `0x7C97C` / file `0xD7420`, confirmed) is a textbook **3D Bresenham
  line-tracer** (the classic octant-switch shape, stepping from one point to another and calling a
  per-voxel step function each iteration) — not an overlay writer.
- **`FUN_0007CCE8`** (VA `0x7CCE8` / file `0xD778C`, derived) is that per-voxel step function. It
  writes a decaying intensity value into a *different* tile array
  (`DAT_002FF16C`, seeded from `&DAT_002FFE81`) than the one the overlay lives in
  (`DAT_002FF180`, per `B5-F1-K1-hazards.md` §2.3), scaled by an obstruction-table byte multiplied
  by `DAT_0030099F` and shifted — the shape of a **light/vision falloff along a line-of-sight ray**,
  not a hazard placement. It is also the function containing the `CMP byte ptr
  [0x3009a0],0x4` read from §1's xref table, which reads consistently with this: `DAT_003009A0`
  is being reused here as a **lighting-mode selector** (case `4` gets separate handling at two
  points in the function), a scratch reuse of the same three-byte parameter block
  (`DAT_0030099E/9F/A0`) for a completely different subsystem (illumination), not a second copy of
  the hazard-type enum.

Conclusion: `FUN_0007E600`/`FUN_0007E958` are an **explosion-lighting** sibling pair to
`FUN_0007D350`/`FUN_0007D67C` (both families are invoked from "something exploded here" call sites
and both consume the same three scratch bytes), not a second hazard-placement path. This was worth
checking — the resemblance was strong enough to be worth ruling out explicitly rather than
assuming — but it does not bear on 1-vs-3.

---

## 5. String pool re-check (the sparse pointer table lead)

Per the task's lead about the confirmed sparse pointer table at object2 `0x292D18`–`0x292DEC`
(`METHOD-tacp-string-regions.md`): `Smoke` (packed-pool copy at `0x292B3D`) **is** one of the
table's ~24 real string entries, at table slot `0x292DDC`. Dumped every slot in the table this
session (scratch script `QuerySmokeTableSlot.java`, `getReferencesTo` on each slot address, not
just the string) to check whether *code* reads that slot (as opposed to the slot merely pointing at
the string, which is what registers as the string's own "1 xref" in a naive check). **Zero of the
~24 real string-holding slots — including `Smoke`'s — have any code reference to the slot itself.**
The only slot in the scanned range with real code references (8, across 6 functions in the
`0x5D000`–`0x65000` fire/damage cluster) is `0x292DE8`, one slot before the resolver doc's
documented trailing counter pair — its value is not a valid pointer (decodes to a null string),
so it is very likely an unrelated small integer constant that happens to sit adjacent to the table
in memory, not a 25th table entry. This does not change `METHOD-tacp-string-resolver.md`'s existing
verdict, but extends it with a concrete negative for `Smoke` specifically: **the pointer table
holds `Smoke`'s address, but nothing in the binary reads that table slot** — `Smoke` is exactly as
unconsumed as `Entropy Enzyme` was already found to be. No string-pool angle discriminates 1 vs 3
or even confirms either string is used at all.

---

## 6. What was ruled out (per the project's own wording rule for negative verdicts)

- **Armour vs. health split** (the task's strongest suggested angle): checked every function that
  reads the overlay byte's type field (§3) and every case body of the placement dispatcher (§2).
  Neither type 1 nor type 3 reaches any unit-damage call (`FUN_00060540`, `FUN_0007BCB8`) from
  `FUN_0007D67C` — only fire (case 2) and the two non-overlay kinetic cases (4, 5) do. No armour-
  specific field write was found on either type's path.
- **LOS/vision split** (gas/smoke should differ from enzyme): the four pathfinding-cost twins and
  `FUN_0007B918` (an AI hazard-cost evaluator) both read as LOS/movement-adjacent code, and both
  treat 1 and 3 identically while excluding or special-casing fire. No type-1-vs-3 split found here
  either.
- **Stage/lifetime/power table split**: `FUN_00049D60`'s divisor (3 for both), the pathfinding
  twins' shift amount (2 for both), and `FUN_0007B918`'s clamp/gate (`!=7` for both) are all
  identical between type 1 and type 3; only fire differs from the pair. No separate stage table for
  1 vs 3 was found.
- **Static-xref dispatch-variable search** (prior session's method): shown wrong in §1 — fixed and
  re-run; the corrected writer set (§1, §2) still does not resolve 1 vs 3, because the true
  producer is a caller-supplied parameter chain (§4), not a literal or catalog lookup local to the
  placement function.
- **String-pool sparse pointer table** (this session's new lead, §5): `Smoke`'s table slot has zero
  code readers, same as every other checked entry in this project's B5/K1 string work.
- **Producer-side call-site trace**: followed three call frames (`FUN_0007D67C` ←
  `FUN_0007D350` ← `FUN_0007D2C4` ← `FUN_000584D0`/struct-creation in `FUN_0003D9E4`) to a genuine
  dead end — not a null result, but a large, unrelated AI routine whose relevant pointer's origin
  was not identified in the time available.

## 7. Concrete next step, if resumed

Identify what `EDI` points to at `FUN_0003D9E4` VA `0x3EB08` (file `0x995AC`, non-4, derived
delta), where `[EDI+0xE]` is read just before it becomes the struct's byte 0. Two sub-tasks: (a)
find `EDI`'s assignment — search backward from `0x3EAE1` (file `0x99585`, the start of the
struct-allocation block) past this session's dumped window (`0x3E600`–`0x3E905` was clean; the
assignment is
either earlier in this 9598-byte function or arrives as one of `FUN_0003D9E4`'s own incoming
parameters, whose callers were not enumerated this session); (b) once `EDI`'s struct is known,
check whether byte `+0xE` is a copy of a named field from `tools/extractors/common/aequipment.h`'s
catalog data (e.g. a `damage_type` index) — if it resolves to a value of `2` for a known incendiary
item, the "per-item catalog field feeds the blast-effect selector" model is validated and the same
field read for the Entropy Pod and a Smoke/Stun Gas item would answer 1-vs-3 directly. If it
resolves to something with no catalog meaning (a synthesized AI-only value), the model is wrong and
this row should close as a permanent structural negative rather than staying open indefinitely.
