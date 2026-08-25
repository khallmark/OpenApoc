# F1 · Fire remainder — the two unbound spread-primitive inputs

**Binary/environment.** `OpenApoc-og-research/canonical/TACP.EXE`, CRC32 `0xfebbe39e` (the
extractor-canonical non-4 build), Ghidra project `ghidra_projects/OpenApocOG_TACP.rep`
(pre-imported LX-loader LE, `-processor x86:LE:32:default -cspec gcc`), queried this session with
`-noanalysis -process TACP.EXE` (re-opens the already-analysed program). All addresses below were
re-read this session directly from the loaded image (`QueryF1Remainder.java`, a new read-only
script left in the lab under `scripts/`) — no citation here was verified by computing a file offset
by hand and reading the raw `.EXE`; where a file offset is given it is either Ghidra's own
signature-unique `boundFileOffset` match, or explicitly labelled **derived** via the confirmed
`.object2` delta (`+0x4FAA4`, established in `B5-F1-K1-hazards.md` §0 and re-used here without
re-deriving it).

## Verdict

**Both of this session's two targets resolve.** This closes two of the three reasons
`battlehazard.h`'s class comment currently gives for leaving the spread primitives uncalled. The
**third** reason — invocation/attempt cadence, the thing that actually sets the *aggregate* spread
rate — was **not** examined this session and remains unbound; see "What this does and does not
unblock" below. Do not read this document as clearing the primitives for wiring on its own.

1. **Per-terrain resistance byte (`FUN_0007aa8c`) — BOUND**, at the field-correspondence level.
   Fire's resistance operand is `BattleMapPartType::block[DamageType::BlockType::Fire]` of
   whichever wall map-part sits across the axis of spread (a different catalog for East/West vs.
   North/South). No numeric constant is recoverable from the static image — the four catalogs this
   reads from are BSS-shaped, populated at battle-load time from the mission's tileset `.dat` files,
   exactly like OpenApoc's own extractor loads them. That is expected, not a gap: there was never
   anything here to embed, matching the RNG-table precedent already documented in
   `B5-F1-K1-hazards.md`'s correction banner.
2. **Fire's inherited baseline — BOUND, and it is not a new table.** It is the *already-bound*
   `fire_hazard_power_table` (`DAT_00293050`, `tools/extractors/common/aequipment.h:82`), read via
   the *already-implemented* `BattleHazard::fireOverlayPower()`. Fire's threshold is
   `hazardRoll(rng, 10) + fireOverlayPower(state.fireHazardPowerTable, fireOverlay)`.
3. **Bonus, not originally asked but needed to fully answer the task's framing:** `FUN_0007ae78`'s
   caller-supplied threshold (types 1/3) is traced to its exact source — the hazard's own current
   stage byte, copied straight through with no RNG and no table lookup.

---

## 1. Unknown #1 — the per-terrain resistance byte

### 1.1 The mechanism, re-confirmed from the raw listing

`FUN_0007aa8c` (VA `0x7AA8C`, file `0xD5530`, signature-confirmed in `B5-F1-K1-hazards.md` §0)
compares the *destination* tile's coordinates against the *source* tile's and picks one of two
axis tables depending on which coordinate changed:

```
if (dst_x == src_x) {                      // no X movement
    if (dst_y != src_y) {
        // North/South: read DAT_000ff2db-based table
    }
}
else if (dst_x - src_x == 1) {              // East
    // read DAT_000f9cdb-based table
}
else {                                      // West (or fallback)
    // read DAT_000f9cdb-based table
}
```
(`export/query_hazard_rng2.log:51-97`, decompiled and raw-listing-confirmed this session and in the
prior session.) For the Up outcome (X delta = 0 **and** Y delta = 0), neither branch's inner
condition fires and the function returns its zero-initialized accumulator — Up is **never** gated
by this resistance check at all, only by the separate veto tables (§1.4).

The X/Y split is exactly what fire's call site passes. Raw listing, `FUN_0007b0d0` (VA `0x7B0D0`,
file `0xD5B74`), the call at `0x7B174`-`0x7B17B`:
```
0007b174 PUSH 0x2        ; param_5 = hazard type = 2 (fire), hardcoded
0007b176 PUSH EDI        ; param_4 = destination Y (with carry)
0007b177 MOV ECX,ESI     ; param_3 = destination X
0007b179 MOV EAX,EBP     ; param_1 = source X  (EBP = FUN_0007b0d0's own param_1, untouched)
0007b17b CALL 0x0007aa8c
0007b180 CMP AL,byte ptr [ESP + 0xc]   ; AL(resistance) vs [ESP+0xc](RNG(10)+baseline)
0007b184 JNC 0x0007b220                ; JNC = no-spread when resistance >= threshold
```
(`export/query_hazard_rng.log:301-309`; param_2/source-Y arrives in EDX, a register untouched
between FUN_0007b0d0's own entry and this call — see `export/query_hazard_rng.log:191-235` for the
full body.) This reconfirms and sharpens `B5-F1-K1-hazards.md` §2.2's description with an exact
parameter mapping: `FUN_0007aa8c(srcX, srcY, dstX, dstY, hazardType)`.

### 1.2 The address arithmetic: `DAT_000f9cdb` is not a table base, it is `catalog_base + 0xB`

`FUN_0007aa8c`'s inner read (raw listing, `export/query_hazard_rng2.log:100-135`):
```
0007aad9 IMUL ECX,ECX,0x56              ; ECX = mapPartTypeId * 86
0007aadc ADD ECX,dword ptr [0x0029304c] ; ECX += DAT_0029304c   (a per-hazard-type selector, §1.3)
0007aae2 MOV CL,byte ptr [ECX + 0xf9cdb]
```
`0xf9cdb` is a literal displacement compiled into the instruction — the address the *compiler*
treats as element `[0]` of whatever C-level array expression this is. It does **not** look like a
natural table base: it ends in `...db`, one byte off round numbers, while the already-known,
independently-confirmed veto-table base (`DAT_000f46d0`, read by `FUN_0001ea70` as
`&DAT_000f46d0 + typeId*0x56`, `B5-F1-K1-hazards.md` §2.2) is a clean `...d0`.

Checking the arithmetic against `BattleMapPartEntry`'s known on-disk layout
(`tools/extractors/common/battlemap.h:92-135`, `static_assert(sizeof(...) == 86)`) resolves this.
`offsetof(BattleMapPartEntry, block_physical) == 11 == 0xB` (constitution 2 + explosion_power 2 +
explosion_depletion_rate 2 + explosion_type 2 + fire_resist 1 + fire_burn_time 1 +
fire_burn_intensity 1 = 11). And:
```
0x0F46D0 (DAT_000f46d0, confirmed catalog base)  +  0x5600 (= 256 * 86)  =  0x0F9CD0
0x0F9CD0                                          +  0x5600              =  0x0FF2D0
0x0FF2D0                                          +  0x5600              =  0x1048D0
0x1048D0                                          +  0x5600              =  0x109ED0
0x0F9CD0 + 0xB = 0x0F9CDB   (== the literal FUN_0007aa8c uses for the East/West table)
0x0FF2D0 + 0xB = 0x0FF2DB   (== the literal FUN_0007aa8c uses for the North/South table)
0x1048D0 + 0x8 = 0x1048D8, 0x1048D0 + 0x9 = 0x1048D9  (read directly by FUN_0007b3dc, below)
```
Four **256-row × 86-byte** catalogs sit contiguously in memory, `0x0F46D0`–`0x109ED0`, each row
matching `BattleMapPartEntry`'s exact size — the same size and per-type-id indexing OpenApoc's own
`readBattleMapParts()` uses for its four calls (Ground/LeftWall/RightWall/Feature,
`tools/extractors/extract_battlescape_map_parts.cpp:318-339`). `0xF9CDB`/`0xFF2DB` are
`catalog2_base+11` and `catalog3_base+11` — i.e. `&catalog[typeId].block_physical`, the start of
the four-element `block_physical/block_gas/block_fire/block_psionic` run — **not** independent
table bases. (This corrects this session's own first pass, which read `0xF9CDB` as a table base and
concluded the field didn't match `fire_resist`; it doesn't, because it was never being compared to
the right struct offset in the first place.) As a loose fifth cross-check (adjacency, not proof —
the two derivations land two bytes apart, so treat this as corroborating, not exact): the computed
catalog span's end, `0x109ED0`, sits right next to the `0x24E`-stride array
`B5-F1-K1-hazards.md` §3.4 already flagged at `0x109ECE`–`0x11B360` ("plausibly the unit table") —
consistent with the four map-part catalogs being immediately followed by an unrelated table, which
is what you'd expect at the boundary of a real, load-time-sized allocation region.

**Independent cross-check, at two of the four catalog bases, both already noted but not pinned to
exact fields in `B5-F1-K1-hazards.md`:**
- `FUN_0001ea70`/`FUN_0001eb48` (the veto pair called after `FUN_0007aa8c` succeeds, see §1.4) read
  `catalog1_base(0xF46D0) + 8` — `offsetof(fire_resist) == 8`. Exact match.
- `FUN_0007b3dc` (fire's already-bound stage-advance function) compares `DAT_00104921` against
  `0x27` (39) — `catalog4_base(0x1048D0) + 0x51(81)` — `offsetof(height) == 81` — then, only if that
  passes, compares the SAME row's `DAT_001048D8` against `0xFF` — `catalog4_base + 8` —
  `offsetof(fire_resist) == 8`, testing for the immune sentinel. Raw listing,
  `export/tacp_fire_functions_fresh.log:482-492`:
  ```
  0007b47f IMUL EAX,EAX,0x56
  0007b482 CMP byte ptr [EAX + 0x104921],0x27
  0007b489 JNZ 0x0007b4b3
  0007b48b CMP byte ptr [EAX + 0x1048d8],0xff
  0007b492 JNZ 0x0007b4b3
  ```
  Two more independent, exact offset matches (`+81`=`height`, `+8`=`fire_resist` again, this time
  the `0xFF`/immune-sentinel comparison specifically) — and this is the first raw-binary
  confirmation of the header comment's forum-sourced folklore, "Assume 255 = immune": it is not
  folklore, it is exactly what this instruction checks.
- The same function separately compares its own current **stage** byte (saved at `[ESP+8]` from the
  overlay's low six bits, same value as §2.1's `bVar2`) against `fire_burn_time`, selecting between
  two *different* catalogs depending on whether a feature occupies the tile (`BL`, read from
  `param_1[3]`/`EBP+3`). Raw listing, `export/f1_b3dc_tail.log:65-67` (feature present, `BL != 0`)
  and `:93-95` (no feature, `BL == 0`):
  ```
  0007b530 MOV AL,BL
  0007b532 IMUL EAX,EAX,0x56
  0007b535 MOV CH,byte ptr [ESP + 0x8]        ; CH = current stage
  0007b539 CMP CH,byte ptr [EAX + 0x1048d9]   ; catalog4(feature) + 9 = fire_burn_time
  0007b53f JC 0x0007b5e5
  ...
  0007b58e MOV AL,BH
  0007b590 IMUL EAX,EAX,0x56
  0007b593 MOV CL,byte ptr [ESP + 0x8]        ; CL = current stage
  0007b597 CMP CL,byte ptr [EAX + 0xf46d9]    ; catalog1(ground) + 9 = fire_burn_time
  0007b59d JC 0x0007b5e5
  ```
  Two more exact `+9`=`fire_burn_time` matches, now raw-listing-sourced (this corrects this
  session's own first pass, which cited this specific pair of reads from the decompiler only).
  This is the literal mechanism `battlehazard.h`'s class comment already describes in prose
  ("`FUN_0007b3dc` destroys terrain when the six-bit fire overlay stage reaches `fire_burn_time`")
  — now tied to concrete addresses, with the ground-vs-feature catalog selection as a detail the
  prose didn't have.

Five independently-checked offsets across two different catalogs (`+8` ×2, `+9` ×2, `+81`) all land
exactly on the named field at that offset in the already-existing `BattleMapPartEntry` struct, with
zero adjustment. This is strong enough to treat the four-catalog layout as bound, not merely
suggestive.

### 1.3 What `DAT_0029304c` actually selects: `block[BlockType]`, not an arbitrary struct offset

`DAT_0029304c` is a **mutable global**, set at the top of every `FUN_0007aa8c` call from a small
table indexed by the hazard-type argument:
```c
DAT_0029304c = *(int *)(&DAT_00293030 + param_5 * 4);
```
Raw int32 dump this session (`QueryF1Remainder.java`, `export/query_f1_remainder.log`), VA
`0x293030`, `.object2`, initialized, file offset **derived** `0x2E2AD4` (`0x293030 + 0x4FAA4`):
```
[0]=1  [1]=1  [2]=2  [3]=1  [4]=0  [5]=0  [6]=3  [7]=0(*)
```
(*index 7 coincides with `DAT_0029304c`'s own address, `0x29304C` — it is overwritten at runtime on
every call, so its resting value is not meaningful; the table proper is indices 0–6, all in
`{0,1,2,3}`.)

Every value is in `{0,1,2,3}` — a four-way index, not a free-ranging byte offset (0–85 would be
possible if this were a direct struct-field selector, and it isn't one; §1.2 shows the `+0xB` is
already baked into the instruction's literal displacement, separately). Combined with §1.2, the
full expression is:
```
resistance = catalog[wallTypeId].block_physical_array[ DAT_00293030[hazardType] ]
           = catalog[wallTypeId].{block_physical, block_gas, block_fire, block_psionic}[ DAT_00293030[hazardType] ]
```
using **on-disk field order** (`block_physical`=0, `block_gas`=1, `block_fire`=2,
`block_psionic`=3 — the literal byte sequence at offsets 11–14 in `BattleMapPartEntry`). This is
*not* the same order as OpenApoc's own `DamageType::BlockType` C++ enum
(`game/state/rules/battle/damage.h:54-60`: `Physical, Psionic, Gas, Fire` — Psionic and Gas are
swapped relative to on-disk order). The extractor still assigns the right value to the right
enumerator by name (`object->block[BlockType::Fire] = entry.block_fire;`,
`extract_battlescape_map_parts.cpp:70-73`), so the **field identity** is unaffected by the enum's
internal ordinal — only a naive "index == enum ordinal" read would get this wrong.

Fire passes `param_5 = 2` (hardcoded, §1.1). `DAT_00293030[2] = 2` → on-disk index 2 →
`block_fire` (offset 13). **Fire's resistance-gate compares its rolled threshold against
`BattleMapPartType::block[DamageType::BlockType::Fire]`** of the wall map-part on the relevant
axis — the semantically correct field, recovered without guessing.

As a bonus, `FUN_0007ae78` (the generic type-1/3 engine) does **not** thread its own `type`
argument into this selector at all — it hardcodes `PUSH 0x0` for `FUN_0007aa8c`'s `param_5`
(and for its second resistance call, `FUN_0007aa20`, likely the Up/Down axis — not decompiled this
session, its call also `PUSH 0x0`s the same slot; raw listing
`export/tacp_fire_functions_fresh.log:180-224`). `DAT_00293030[0] = 1` → `block_gas`. **Both type-1
and type-3 hazards gate on `block_gas`, unconditionally, regardless of which of the two they are.**
This is a further, independent piece of evidence for `B5-F1-K1-hazards.md` §3's "types 1/3 are a
generic vapour-hazard family, fire was specialised out of it" reading — but it does **not**
disambiguate which of type 1 / type 3 is Enzyme vs. Gas/Smoke; both still resolve to the same
field, so this cannot be used to break that tie.

### 1.4 The tables have no static content — confirmed, not assumed

`QueryF1Remainder.java` dumped 344 bytes (4×86) from each of `DAT_000f9cdb`, `DAT_000ff2db`, and
`DAT_000f46d0` directly against the loaded image. All three read **all-zero**
(`ALL_ZERO=true` in the script's own output, `export/query_f1_remainder.log`), matching
`.object2`/initialized-but-empty, exactly the shape `B5-F1-K1-hazards.md`'s correction banner
already established for the hazard RNG table. This is expected, not a gap: `readBattleMapParts()`
loads this exact struct from a **per-tileset external `.dat` file**
(`tools/extractors/extract_battlescape_map_parts.cpp:20-24`, `dirName + "/" + datName + ".dat"`),
not from `TACP.EXE` itself. There is no numeric resistance constant anywhere in the static binary to
recover, for any map, because the original game didn't put one there either — it loads per-mission
tileset data at battle start, exactly like OpenApoc's extractor already does. **Per the prime
directive: nothing was invented, and nothing needs to be, because OpenApoc already has this exact
field (`BattleMapPartType::block[BlockType::Fire]`) populated live at battle-load time** — nothing
resembling this exists in the static EXE for either project to read.

---

## 2. Unknown #2 — fire's inherited baseline

### 2.1 Raw-listing trace of the `CL` argument

`FUN_0007b0d0`'s baseline arrives as a byte in `CL` (the third `__regparm3` register argument),
consumed at `0x7B0E6`: `MOV AH,CL ; ADD AH,AL` — `AL` is the fresh `RNG(0..10)` roll, `CL` is
whatever the caller left there. Raw listing of the caller, `FUN_0007b3dc` (VA `0x7B3DC`, file
`0xD5E80`, already bound), from function entry through the call:
```
0007b3f1 MOV AL,byte ptr [ECX]      ; AL = overlay byte
0007b3f5 MOV DL,AL
0007b3f7 SAR EDX,0x6                ; type (top 2 bits)
0007b3fa CMP EDX,0x2
0007b3fd JNZ 0x0007b406
0007b3ff MOV DL,AL
0007b401 AND DL,0x3f                ; if type==2 (fire): DL = stage (low 6 bits)
0007b404 JMP 0x0007b408
0007b406 XOR DL,DL                  ; else: DL = 0
0007b408 MOV byte ptr [ESP + 0x8],DL
0007b40c AND EDX,0xff
0007b412 MOV DL,byte ptr [EDX + 0x293050]   ; DL = DAT_00293050[stage]
0007b418 MOV byte ptr [ESP + 0xc],DL
0007b41c TEST DL,DL
0007b41e JZ 0x0007b5e5              ; whole-tile early-exit if DAT_00293050[stage] == 0
...
0007b44e XOR ECX,ECX
0007b450 MOV EAX,EDI
0007b452 MOV CL,DL                  ; CL = DL, UNCHANGED since 0x7b412 -- confirmed no
                                     ; intervening write to DL/EDX in the bounds-check block
0007b454 MOV EDX,ESI
0007b456 CALL 0x0007b0d0
```
(`export/tacp_fire_functions_fresh.log:498-591`, full function body and listing verified for
absence of any EDX/DL write between `0x7b412` and `0x7b452` — the intervening instructions are pure
bounds checks against `DAT_000e4c10/14/18` and do not touch that register.) **`baseline =
DAT_00293050[stage]`, where `stage` is the fire overlay's own low six bits** — the same value
`DAT_00293050[stage] != 0` already gates whether the tile is processed at all this tick.

### 2.2 The table is `fire_hazard_power_table` — already bound, already extracted

Byte dump, VA `0x293050`, `.object2`, initialized (`QueryF1Remainder.java`,
`export/query_f1_remainder.log`):
```
+0x000: 05 0a 0f 14 19 1e 23 28 2d 32 37 3c 41 46 4b 46
+0x010: 4b 46 4b 46 41 37 2d 23 19 0f 05 00 01 00 00 00
```
As decimal, indices 0–26: `5,10,15,20,25,30,35,40,45,50,55,60,65,70,75,70,75,70,75,70,65,55,45,35,
25,15,5`. A clean, hand-authored ramp-up / three-cycle plateau-at-peak / ramp-down curve — matching
the header comment's own hand-derived narrative in shape ("fire ... starts small, gradually
enlarges, then rages for a bit, and then dies out") even though that comment's actual numbers are
about the unrelated visual "Stage" variable, not this table.

File offset (derived, `+0x4FAA4`): `0x293050 + 0x4FAA4 = 0x2E2AF4`. This is **not a new address** —
it is `labels/tacp_rebase.csv:13`'s already-bound `fire_hazard_power_table` entry
(`0x2e2af4,3025652,...,exact_bytes,high`) and matches
`tools/extractors/common/aequipment.h:82`'s own comment verbatim: `// TACP DAT_00293050 @ file
0x2E2AF4 (FUN_0007ae18 @ VA 0x7AE18 / file 0xD58BC)`. The table's declared extractor length,
`FIRE_HAZARD_POWER_TABLE_OFFSET_END (0x2E2B0F) − OFFSET_START (0x2E2AF4) = 0x1B = 27 bytes`, matches
this session's raw read exactly: 27 non-zero entries (indices 0–26), zero from index 27 on. Two
independent readings (a prior session's file-offset extraction, this session's live VA read of the
same bytes in Ghidra) agree byte-for-byte.

**Fire's baseline is `BattleHazard::fireOverlayPower(state.fireHazardPowerTable, fireOverlay)`** —
a function that already exists, already reads the already-extracted table, already does exactly
`powerTable[fireOverlayStage(overlay)]` (`game/state/battle/battlehazard.cpp:132-141`), and is
already used elsewhere in the same file for fire's visual/extinguish logic
(`advanceOriginalFireOverlay`, `battlehazard.cpp:165`). `rollFireSpreadNeighbour`'s existing
`baseline` parameter should be fed this call's result — no new state, no new extraction, no new
table.

**Not an exact equivalence — one dead-branch divergence, worth recording so nobody "fixes" it
later.** §2.1's raw listing sets `DL = 0` (not an early-exit) when the overlay's type byte isn't 2,
then still indexes `DAT_00293050[0] = 5` — i.e. the original code would compute a nonzero baseline
even for a non-fire overlay, if it were ever reached with one. `fireOverlayPower()` instead goes
through `fireOverlayStage()`, which returns `-1` for a non-fire overlay and makes the function
return `0` unconditionally. The two disagree in that one case. It is unreachable in both the
original and the port — `FUN_0007b610` only dispatches to `FUN_0007b3dc` when the overlay's type
bits already read `2` (`B5-F1-K1-hazards.md` §3.1's jump table), so `FUN_0007b3dc` (and by
extension `FUN_0007b0d0`) never actually runs against a non-fire overlay. `fireOverlayPower()`'s
current behaviour is fine to keep as-is; just don't cite `DAT_00293050[0]`'s value as evidence it
should change.

### 2.3 Bonus: `FUN_0007ae78`'s own threshold source, traced

`B5-F1-K1-hazards.md` §3.3/§3's correction left `FUN_0007ae78`'s threshold parameter
(`local_20[4]`) as "caller-supplied, not traced." Both callers supply the same thing. Raw listing,
`FUN_0007b2b4` (type 1, VA `0x7B2B4`):
```
0007b2c3 MOV AL,byte ptr [ECX]      ; AL = overlay byte
0007b2c7 MOV DL,AL
0007b2c9 SAR EDX,0x6
0007b2cc CMP EDX,0x1
0007b2cf JNZ 0x0007b2d5
0007b2d1 AND AL,0x3f                ; if type==1: AL = stage
0007b2d3 JMP 0x0007b2d7
0007b2d5 XOR AL,AL                  ; else: AL = 0
0007b2d7 PUSH 0x1                   ; type=1 arg to FUN_0007ae78
0007b2dd MOV byte ptr [ESP + 0x8],AL
0007b2e1 XOR ECX,ECX
0007b2e5 MOV CL,AL                  ; CL = AL = the SAME stage byte, straight through
0007b2e9 CALL 0x0007ae78
```
(`export/query_overlay_type_siblings1.log:143-160`; `FUN_0007b348`, type 3, is byte-identical
modulo the `1`→`3`/`0x40`→`0xc0` constants — `export/query_overlay_type_siblings1.log:235-252`.)
**The generic engine's threshold is the hazard's own raw current stage byte** — no RNG applied, no
lookup table, just `overlay & 0x3F` (or `0` if the type doesn't match) passed straight into the
same resistance comparison fire uses (`FUN_0007ae78`'s own listing,
`export/tacp_fire_functions_fresh.log:212-217`: `CMP AL,byte ptr [ESP + 0x14]` /
`JNC 0x0007b0c3`, `[ESP+0x14]` being where `CL` was saved at function entry). This resolves the
task's "trace the caller that supplies its threshold parameter" ask directly.

---

## 3. What this does and does not unblock

`battlehazard.h`'s class comment gives three reasons the primitives are deliberately uncalled: (a)
"the per-terrain resistance byte values (`FUN_0007aa8c`) were never decoded", (b) "fire's inherited
baseline was never pinned down", (c) "the generic engine's threshold source was never traced" — and
separately warns that wiring under an unbound *rate* is exactly the kind of half-wiring the parity
guide prohibits, because OpenApoc's `grow()` sweeps a 3×3 XY block plus the Z column (up to a dozen
`expand()` attempts, stopping at first success) while the recovered engine tries **exactly one**
direction and gives up.

(a), (b), and (c) are now resolved (§1, §2). **The rate reason is not**, and this session did not
touch it: `FUN_0007b0d0` is invoked from `FUN_0007b3dc` ← `FUN_0007b610` ← the real-time row
scheduler / 400-iteration end-of-round batch (`FUN_0007b7f8`, already bound per
`B5-F1-K1-hazards.md` §0's table, but its tick-to-attempt cadence was not re-examined this session).
Nothing recovered this session says how often the original calls `FUN_0007b0d0` per fire hazard per
unit real time, and nothing here compares that to OpenApoc's own `TICKS_PER_HAZARD_UPDATE`-driven
`grow()` call frequency. **Wiring the now-known gate and baseline into `grow()`'s existing
"sweep ~12 neighbours, stop at first success" loop, without also changing that loop to try exactly
one direction and without matching the original's invocation cadence, would still reproduce a much
higher per-invocation spread probability than the original engine.** That is the specific,
concrete mismatch the task asked to account for, and it survives this session's findings — closing
it needs the scheduler's cadence, which is a distinct, unstarted piece of work, not a consequence of
anything found here.

There is a second, smaller behavioural detail in the same vein, found in §1.1: when the neighbour
roll picks fire's Up outcome (X delta = 0 and Y delta = 0), `FUN_0007aa8c` returns its
zero-initialized accumulator — **the primary resistance gate cannot fail for Up**, only the
separate veto tables can stop it. One of fire's five outcomes (a 1-in-5 pick before the veto stage)
is structurally biased to pass the main gate for free. A port that applies the same `block[Fire]`
resistance check uniformly across all attempted directions, including vertically, would be *more*
restrictive upward than the original — another rate-shaped detail, on top of the geometry/cadence
one above, that a real wiring pass needs to reproduce deliberately rather than inherit by accident.

**Net effect on the "half-wiring is worse than not wiring" judgment call:** the `fire_resist`/
`block[Fire]` values themselves remain permanently unrecoverable from `TACP.EXE` (§1.4) — that part
was always going to have to be sourced from OpenApoc's own already-extracted
`BattleMapPartType::block` field, RE or no RE, and that path is now confirmed correct rather than
assumed. What was missing before this session — proof that `block[Fire]` (not `fire_resist`, not an
invented constant) is the right field, and proof of where the baseline comes from — is no longer
missing. What is still missing — the attempt cadence — is exactly the piece needed to turn "the
formula is right" into "the rate is right." Recommend: leave the primitives uncalled until the
cadence is traced, but update `battlehazard.h`'s class comment to drop reasons (a)/(b)/(c) and state
the cadence as the sole remaining blocker, citing this document.

---

## 4. What was not attempted

- `FUN_0007aa20` (the generic engine's second resistance call, presumably the Up/Down axis) was not
  decompiled — its `PUSH 0x0` call-site argument was read from the existing raw listing
  (`export/tacp_fire_functions_fresh.log:180-224`), but its own body, and whether it reads the same
  four-catalog layout or something else, is unexamined. A headless run to decompile it hit a
  `LockException` this session (a concurrent agent held the project lock) and was not retried.
- Which of the four `0x5600`-spaced catalogs (`0xF46D0`/`0xF9CD0`/`0xFF2D0`/`0x1048D0`) is
  specifically Ground/LeftWall/RightWall/Feature (as opposed to merely "the one read via the
  per-tile cache's byte 0", "byte 1 (East/West axis)", "byte 2 (North/South axis)", and "the one
  `FUN_0007b3dc` reads via a differently-indexed `param_4`") was not independently proven — only the
  X-axis/Y-axis role of catalogs 2 and 3 (the ones `FUN_0007aa8c` actually reads) was established,
  which is what unknown #1 needed. Do not cite a Ground/LeftWall/RightWall/Feature assignment to a
  specific one of these four addresses as bound; it is a plausible, unconfirmed hypothesis.
- `FUN_0007b7f8`'s scheduler cadence (real-time row-by-row vs. the 400-iteration end-of-round batch,
  and how many `FUN_0007b0d0` calls either path produces per fire hazard per unit time) was not
  traced this session — this is the rate blocker described in §3 and the concrete next step for
  actually wiring `grow()`.
- B5's own open question (which of type 1 / type 3 is Enzyme vs. Gas/Smoke) is not resolved by
  §1.3's `block_gas` finding, and that finding cannot be used to resolve it (both types resolve to
  the same field).

## Artifacts (lab only — not copied into this tree)

- `OpenApoc-og-research/scripts/QueryF1Remainder.java` — new, read-only: dumps
  `DAT_00293030`/`DAT_00293050`/the neighbour table as sanity check, and the three resistance-table
  regions (`DAT_000f9cdb`, `DAT_000ff2db`, `DAT_000f46d0`), with block/initialization/file-offset
  metadata for each.
- `export/query_f1_remainder.log` — this script's full output (the two raw dumps §1.3/§2.2 are
  taken verbatim from here).
- `export/f1_b3dc_tail.log` — new, `DumpListingRange.java 0x7b4f7 0x7b5e5`, the raw listing for the
  portion of `FUN_0007b3dc` that was truncated in the prior session's decompile capture; used for
  the ground-vs-feature `fire_burn_time` comparison in §1.2.
- Re-read, not re-derived: `export/query_hazard_rng.log`, `export/query_hazard_rng2.log`,
  `export/tacp_fire_functions_fresh.log`, `export/query_overlay_type_siblings1.log` (all from the
  prior F1/B5/K1 session, re-examined this session for the raw listings cited in §1.1, §1.2, §2.1,
  §2.3 — none of these logs were re-generated, only re-read more closely than the prior pass did).
