# U1 · U2 · V1 — UFO mission-counter transition, base-exposure leftovers, vehicle-dodge engagement table

**U1(a) mission-counter `+0x171` zero-transition — BOUND.** `FUN_0003a910` decrements the byte at
vehicle `+0x171` and, gated by several unnamed order/role-state fields, branches on the
post-decrement value being zero. One sub-branch just sets an "arrived" flag with no new target
search. The other calls `FUN_00091f70` to pick a new target building, `FUN_0004db84` to resolve it
to a nearby site, and `FUN_0004e0d4` to commit the new destination — or, if no building is found,
resets the vehicle's role/order state to 0 and marks it target-less.

**U1(b) `FUN_000588f8` `+0x168` vs constitution `+0x12e` gate — mechanism BOUND, field semantics
NOT BOUND.** The gate itself is a clean, verified instruction pair (`CMP AX,[EBP+0x12e]; JLE …`).
Three independent writers of `+0x168` were traced, and all three set it **once, at vehicle spawn**,
either to a literal `0` or via a `(fixed-point stat) × (per-role table percent) / 100` formula
structurally identical to the already-bound `type_percent × constitution` derivation of `+0x12e`
itself. **No writer that raises `+0x168` after spawn was found anywhere in the traced call graph.**
That rules out the "regenerating health, notify when topped up" reading a first pass over this data
suggests — there is no evidence of that mechanic. What's bound: the gate, the three spawn-time
writers, and the two runtime clamps (`+0x168 = +0x12e − 1` whenever `+0x168 ≥ +0x12e` and a new
destination is assigned). What's unbound: what the field represents and why the periodic check in
`FUN_000588f8` would ever see it exceed constitution.

**U2(a) `DAT_000e0cc0` override lifecycle — reads/override BOUND, clear sites BOUND, no set site
found.** It is OR'd into the per-base "known to aliens" predicate in two independent functions
(`FUN_000702e4`, `FUN_000705a8`), confirming the task's description exactly: nonzero forces every
base slot to read as exposed. It is explicitly zeroed at two sites inside `FUN_0003a910` (both
verified via a full push/pop register-preservation proof on the intervening callee, not just the
decompiler's guess). **Across all 6 xrefs to this address in the whole binary, no instruction sets
it to a nonzero value.** That is a genuine, exhaustively-checked negative — not an oversight.

**U2(b) `FUN_0006f738` event types 1 and 4 — BOUND.** `FUN_000b32ac`'s type-1 and type-4 dispatch
cases both run the same org-funds settlement and then call `FUN_0006f738`. The type field
(`DAT_00244024`) has exactly one writer function in the whole binary, `FUN_000aff9c`, and its
branch logic is fully recovered: type 1 is the default classification for a resolved-building
event, type 4 is the same path when a specific classification register reads 3. `FUN_000aff9c`'s
sole caller chain roots in `FUN_0003a910` (this project's already-bound U1/U2 function) via
`FUN_000ac08c → FUN_000ac348`, corroborating [O1-O2-M1-city.md](O1-O2-M1-city.md)'s independent
finding that `FUN_000aff9c` has exactly one call site tied to the same root.

**V1 vehicle attack-mode dodge table — NOT BOUND, definitively.** Every UI string that could name
an engagement/dodge mechanic (`Rules of engagement`, `Evade Fire`, `Evasive`, `Defensive`,
`Standard`, `Aggressive`, and the four altitude labels sharing that menu) has **zero bound xrefs** —
verified fresh this session, not just re-cited. A byte-pattern search of the entire canonical
`UFO2P.EXE` for a literal `{10, 50, 80, 100}` table (both orderings, as `uint8`, `uint16`, and
`uint32` arrays) found **zero matches**. No runtime field for a player-settable attack-mode enum
was identified to pivot a field-based search from. The row stays open with `dodge = 100/80/50/10`
flagged as hardcoded, exactly as documented.

---

## 0. Binary and tooling

`OpenApoc-og-research/canonical/UFO2P.EXE` — ISO non-4 build, 1,702,206 bytes, CRC32 `0x4749ffc1`.
Ghidra project `ghidra_projects/OpenApocOG.rep`, pre-imported and pre-analysed
(`-processor x86:LE:32:default -cspec gcc`, LX loader). `.object1` marked executable via the
existing `MarkObject1Executable.java` preScript (unchanged). All queries below ran read-only
against the existing project via `scripts/ghidra_env.sh … -process UFO2P.EXE`; nothing was
re-imported or unbound.

**File-offset convention** (two, used consistently, both cross-validated against citations already
in `next-implementation.md` / `parity-guide.md` / `ufoincursion.h`):

- **`file 0xNNNNNN`** — the canonical bound-EXE offset (`.image` byte-signature match, or the
  extractor's own convention for already-published table/function addresses). Confirms verbatim
  against `FUN_000b32ac @ file 0x115950` and `FUN_0006da88 @ file 0xD012C` / `0xD030B`, both already
  in the docs.
- **`object-page file 0xNNNNNN`** — `.object1` `MemoryBlockSourceInfo` file-bytes offset, used for
  every function newly bound this session. Confirmed against the two already-published examples:
  `FUN_0003a910 @ object-page file 0x2A90F` (VA `0x3A910`) and `FUN_000588f8 @ object-page file
  0x488F7` (VA `0x588F8`) both reproduce exactly via `QueryFunctions.java`'s `page_file` field. The
  VA → object-page-file delta for `.object1` functions is a constant `0x10001` (confirmed on both
  of the above plus every function newly cited below) — **this is a local, `.object1`-only
  constant, not a project-wide slide**; `.object2` data addresses use the separate documented
  `+0xD0000` remap (`[0x174024]` ↔ VA `0x244024`).

Scripts added this session (adapted from the existing `QueryFunctions.java` / `QueryB1CoverStrings.java`
templates, `scripts/ghidra_env.sh`, no re-import): `QueryV1EngagementStrings.java`,
`QueryE0cc0Lifecycle.java`, `QueryAroundAddrs.java`, `QueryFnTail.java`, `QueryDat244024.java`. All
read-only.

---

## 1. U1(a) — the `+0x171` zero-transition

### 1.1 The decrement, confirmed at the byte level

`FUN_0003a910` (VA `0x3A910`, object-page file `0x2A90F`, bound-file `0x9CFB4`) contains, inside a
larger guard (§1.2):

```
cVar12 = *(char *)((int)&DAT_00161148 + iVar9 + 1) + -1;
*(char *)((int)&DAT_00161148 + iVar9 + 1) = cVar12;
if (cVar12 == '\0') { … }
```

`DAT_00161148 + 1` = `DAT_00160fd8 + 0x171` (the vehicle-array base already bound for `+0x171`), so
this is exactly the documented `+0x171` decrement, and the `if` only fires **on the tick the counter
reaches zero**, not on every decrement. This matches the raw HIT listing independently:
`0003aad7 MOV byte ptr [EDI+0x171],AL` / `0003acf3 MOV CL,byte ptr [EDI+0x171]` /
`0003acfb MOV byte ptr [EDI+0x171],CL` (`export/ufo2p_off171.log`).

### 1.2 The full guard and the two outcomes

```c
sVar6 = (&DAT_00161134)[iVar13 * 0x13b];      // field +0x15C of the SAME 0x276-byte vehicle
                                               // record (0x13b shorts == 0x276 bytes — not a
                                               // separate array; verified by stride arithmetic)
if ((sVar6 != 2 && sVar6 != 3 && sVar6 != 5) &&
    (((sVar2 = *(short *)(vehicle + 0x104), sVar2 == 1) ||
      (sVar6 == 1 && sVar2 != 0)) && DAT_000d5060 == 0) &&
    ((&DAT_00161142)[vehicle] == -1 &&                 // "no pending message" latch
     (cVar12 = --(vehicle+0x171)) == 0))
{
    if (*(short *)(vehicle + 0x104) == 1) {
        (&DAT_00161142)[vehicle] = 1;    // just latch an "arrived" flag; no new target search
    } else {
        uVar7 = FUN_00091f70(category);          // pick a target building of a given category
        *(short *)(dest + 0x14) = uVar7;
        if (dest_x_field == -1) {                // FUN_00091f70 returned "no match" (-1)
            (&DAT_00161134)[vehicle role-state] = 0;         // clear order/role state
            *(byte *)(vehicle + 0x1610a4 field + 3) = 0xFF;  // mark "no destination"
        } else {
            FUN_0004db84();                       // resolve target -> nearby site (x/y, valid?)
            if (valid) {
                *(byte *)(… + 3) = 0;
                (&DAT_001610a8)[vehicle] = new_target_building;
                FUN_0004e0d4(vehicle, x, y);       // commit new destination waypoint
                (&DAT_0016113e)[vehicle] = 0;
                FUN_0005df1c();
            }
        }
    }
}
```

**This is the transition.** Reaching mission-counter zero causes the UFO to search for and commit
to a new destination via `FUN_00091f70` → `FUN_0004db84` → `FUN_0004e0d4`, unless a specific
order-type value (`+0x104 == 1`) just latches an "arrived" flag instead, or no matching building
exists (role/order state is cleared to 0 and the vehicle is marked target-less).

**Deliberately unnamed:** `sVar6`'s role/order-state values (2/3/5 excluded from re-triggering;
1 special-cased), the field compared to `1` at `+0x104`, and `DAT_000d5060` (a global 0/nonzero
flag read throughout this subsystem, likely a side/turn discriminant but not independently
confirmed here). These gate *whether* the transition fires, not *what it does* once it fires; naming
them would be exactly the kind of invented-filter move the task explicitly prohibits, so they're
left as raw offsets.

### 1.3 The three callees, traced

**`FUN_00091f70`** (VA `0x91F70`, object-page file `0x81F6F`, bound-file `0xF4614`) — mechanically:
counts records in the `DAT_0018276c`-based array (stride `0xE2`, count `DAT_00183a68`) whose byte at
`+0xC8` equals the `ushort` argument; if any match, calls `FUN_0005D1D8` (the same bounded-RNG
primitive used for scatter spawn XY elsewhere in this subsystem) to pick the Nth match, honoring an
alternate-priority byte at `+0xC9` gated by `DAT_001277E8`; returns that record's index, or `-1` if
none matched. **This is not named a "building" search and no name filter is added** — it is reported
purely as "counts and randomly selects a record whose `+0xC8` byte equals the argument," per the
task's explicit prohibition on inventing an `acquireTargetBuilding` filter.

**`FUN_0004db84`** (VA `0x4DB84`, object-page file `0x3DB83`, bound-file `0xB0228`) — a nested
loop bounded at 10 (`while (… < 10)`) over what appears to be a 2-D search grid, calling
`FUN_0005CE30` (lookup) and `FUN_00038678` (comparison of a returned "distance"-like value against a
running best), tracking a best-so-far x/y and a validity flag. Read as "find the nearest usable site
near the target," consistent with how it's used here (target building → committed x/y).

**`FUN_0004e0d4`** (VA `0x4E0D4`, object-page file `0x3E0D3`, bound-file `0xB0778`) — a genuinely
generic "commit a new waypoint" primitive with **30+ call sites across the whole vehicle system**
(not UFO-specific); confirmed as the call at `0003ad85` inside `FUN_0003a910`. Sets `+0x4E`/`+0x50`
either from raw arguments or from a route-pattern table indexed by `+0xE8`, sets movement-state
fields, and — when the vehicle has a valid target building (`+0x108 != -1`) and `+0x12c == 0` —
clamps `+0x168` to `+0x12e − 1` whenever `+0x168 ≥ +0x12e` (this is the mechanism behind U1(b)'s
gate, see §2.2).

---

## 2. U1(b) — `FUN_000588f8`'s `+0x168` vs `+0x12e` gate

### 2.1 The gate, confirmed at the instruction level

`FUN_000588f8` (VA `0x588F8`, object-page file `0x488F7`, bound-file `0xBAF9C` — matches the task
brief exactly). Raw disassembly (not the decompiler, which mis-renders this function's stack — see
§2.4) at function entry:

```
00058925 MOV AX,word ptr [EBP + 0x168]
0005892c CMP AX,word ptr [EBP + 0x12e]
00058933 JLE 0x00058a48          ; +0x168 <= constitution -> skip the rest of the function
```

`EBP` is proven to be the vehicle-array pointer for the argument index by the preceding six
instructions (`(EAX*4+EAX)*10 << 6, then -EAX`, i.e. `EBP = index*0x276 + 0x160FD8` — the same
0x276-byte-stride array used everywhere else in this subsystem). So the gate is precisely: **if
`+0x168 ≤ +0x12e` (constitution), return early; only when `+0x168` exceeds constitution does the
function build and dispatch a formatted UI message** (calls `FUN_00063A00` — a name/string fetch
with a byte-reversal copy pattern typical of building a two-part sentence — then `FUN_0002AE1C`, a
message/event queue call).

### 2.2 `+0x168` is written exactly three times, always at spawn, never raised afterward

Every reference to displacement `0x168` in `.object1` code was enumerated (`QueryVehicleOff171.java`,
110 hits, 21 functions). Filtering to actual stores of the vehicle-array field itself:

| Site | Function | What it does |
|---|---|---|
| VA `0x6DBDF`, object-page file `0x5DBDE` | `FUN_0006da88` (the UFO spawn function already bound for `+0x1B → +0x171`) | `(int)(EDX >> 0x10) * table[EDI + DAT_00183a72*2 + 0x22] / 100` → `+0x168`. `EDX` is loaded from `vehicle + 0x12C` as a 16.16 fixed value (`SAR EDX,0x10` extracts the integer part) — **structurally identical** to the already-bound `type_percent(role table) × constitution(vehicle_data +0x2e) / 100` derivation of `+0x12e`, just from field `+0x12C` and table column `+0x22` instead. |
| VA `0x70E00`, object-page file `0x60DFF` | `FUN_00070cc0` | `MOV word ptr [ESI+0x168], 0x0` — plain literal zero, alongside `+0x16c = 0xFFFF` ("no follow"). |
| VA `0x5DCEB`, object-page file `0x4DCEA` | `FUN_0005d6e4` (called from the generic vehicle-slot allocator `FUN_0006de64`) | `(byte)table[EAX*4 + 0x12D950] * (EBX >> 0x10) / 100` → `+0x168`, where `EBX` was loaded from a dword table at `0x128614`. Same "16.16 value × table / 100" idiom again. |

**No fourth site exists that increments, adds to, or otherwise raises `+0x168` after one of these
spawn-time writes.** The two runtime touches found elsewhere (`FUN_0004dd14`, `FUN_0004e0d4`, both
§1.3/§2.3) only ever **lower** it (`+0x168 = +0x12e − 1`, and only when `+0x168 ≥ +0x12e`).

This directly rules out reading `+0x168` as a regenerating stat that grows toward constitution and
fires a "topped up" message — there is no raising writer to support that, and the advisor review
that caught this is recorded here so the wrong reading doesn't get re-derived later. What's left
provable: `+0x168` starts life as a percentage-derived (or zeroed) per-instance stat sourced from a
*different* base field than constitution, gets defensively clamped below constitution whenever a
new destination is assigned, and is checked periodically (§2.3) for exceeding constitution — which,
given no writer ever raises it past its spawn-time value, can only happen from the spawn-time
formula itself landing above whatever `+0x12e` was independently derived to.

### 2.3 The periodic caller, and the clamp's sibling

`FUN_000588f8` runs from `FUN_0005760c` (VA `0x5760C`, object-page file `0x4760B`), a per-vehicle
tick loop over all 80 vehicle slots, staggered by `vehicleIndex % 18` against a date-derived counter
(more of the 18 buckets match at higher `DAT_000d4d58` difficulty) — i.e. each qualifying vehicle is
checked on a fixed, difficulty-scaled cadence, gated additionally on `(vehicle+0x42 unresolved) ||
(vehicle+0x4b == 3)`.

The clamp side of the mechanism has **two** independent sites, not one — both already noted above:
`FUN_0004dd14` (VA `0x4DD14`, object-page file `0x3DD13`) and `FUN_0004e0d4` (§1.3) contain the
identical pattern `if (target valid && flag+0x12c == 0 && +0x12e <= +0x168) +0x168 = +0x12e - 1;`,
confirmed byte-for-byte in raw disassembly at VA `0x4E177`–`0x4E188`
(`MOV DX,[ESI+0x12e]; CMP DX,[ESI+0x168]; JG skip; DEC EDX; MOV [ESI+0x168],DX`).

### 2.4 Why the decompiler was not trusted here

Ghidra's decompile of `FUN_000588f8` renders synthetic `stack0xffffffe2`-style variables and never
shows a literal `+0x168`/`+0x12e` access — a known failure mode for this function's stack layout.
The raw listing (§2.1) resolves this cleanly and is the evidence actually cited.

**Verdict: gate mechanism BOUND (three writers, two clamps, one periodic reader, all with exact
addresses). Field semantics NOT BOUND — no name is assigned to `+0x168` beyond "percentage-derived
per-instance stat compared against constitution," because no reader or writer ties it to a named
concept (health, cargo, growth, or otherwise).**

---

## 3. U2(a) — `DAT_000e0cc0` global override

### 3.1 What it does (already partly documented; reverified)

Six xrefs total in the entire binary (`QueryE0cc0Lifecycle.java`, exhaustive `getReferencesTo`):

| VA | Type | Function | Object-page file |
|---|---|---|---|
| `0x3AE5C` | WRITE | `FUN_0003a910` | `0x2AE5B` |
| `0x3B064` | WRITE | `FUN_0003a910` | `0x2B063` |
| `0x6D004` | READ | `FUN_0006cfb4` | `0x5D003` |
| `0x70321` | READ | `FUN_000702e4` | `0x60320` |
| `0x705AC` | READ | `FUN_000705a8` | `0x605AB` |
| `0x705EA` | WRITE | `FUN_000705a8` | `0x605E9` |

`FUN_000702e4` (VA `0x702E4`, the already-bound Subversion targeting latch) and `FUN_000705a8`
(VA `0x705A8`) both gate base-slot eligibility with the identical shape:

```
if (base_active[slot] && (base_known[slot] != 0 || DAT_000e0cc0 != 0)) { … eligible … }
```

confirming the task's description precisely: nonzero `DAT_000e0cc0` makes **every** active base slot
pass the exposure predicate, overriding the per-slot flag entirely. `FUN_0006cfb4` (VA `0x6CFB4`,
role-roll for a new UFO mission) separately reads it as `if (DAT_000e0cc0 != 0 && secondaryRoll ==
0) forcedRole = 2` — a second, distinct consumer forcing the next UFO's role toward Subversion when
the override is set, not something the task asked to bind but recorded here since it turned up in
the same exhaustive xref pass.

### 3.2 The two write sites clear it — verified, not assumed

Both writes in `FUN_0003a910` follow the identical pattern (raw disassembly, `QueryAroundAddrs.java`):

```
; site 1 (VA 0x3AE5C, role-state == 3 completion branch)
0003ae55 XOR EDX,EDX
0003ae57 CALL 0x00059148
0003ae5c MOV dword ptr [0xe0cc0],EDX

; site 2 (VA 0x3B064, role-state == 2 completion branch)
0003b05d XOR ECX,ECX
0003b05f CALL 0x00059148
0003b064 MOV dword ptr [0xe0cc0],ECX
```

Both zero the register immediately before calling `FUN_00059148`, then store that same register
right after the call returns — which only proves a clear if `FUN_00059148` provably doesn't clobber
`EDX`/`ECX`. It doesn't: `FUN_00059148`'s prologue is `PUSH EBX; PUSH ECX; PUSH EDX; PUSH ESI; PUSH
EDI; PUSH EBP; SUB ESP,0x20`, and its confirmed tail (`QueryFnTail.java`, VA `0x597ED`–`0x597F6`) is
`ADD ESP,0x20; POP EBP; POP EDI; POP ESI; POP EDX; POP ECX; POP EBX; RET` — an exact, complete
push/pop bracket around the entire function body. `EDX` and `ECX` are restored to their pre-call
values at return, so the zero set immediately before the call is exactly what lands in the global
immediately after. (Ghidra's own decompile renders these as `extraout_EDX_00` / `extraout_ECX_01`
because its calling-convention model doesn't know this specific callee preserves them — the raw
push/pop trace is the stronger evidence and is what's cited here.)

**Both sites fire in `FUN_0003a910`'s role==2 (Subversion) / role==3 (Infiltration) mission-completion
branches** — the same dispatch region documented for U1/U2's other bound behavior.

### 3.3 No set site exists

Of the six total xrefs, the only other write (`FUN_000705a8` at VA `0x705EA`) reads `DAT_000e0cc0`
into `ESI` once at function entry and is never touched again before being written straight back —
confirmed via the raw listing, a genuine no-op round-trip, not a set. **No instruction anywhere in
UFO2P.EXE writes a nonzero value to `DAT_000e0cc0`.** Per the task's own framing, this is reported as
a definitive negative rather than speculated into a save-load or bulk-init path that wasn't
observed: **the campaign lifecycle event that would set this override is NOT BOUND**, and nothing
should be invented to fill it in code.

---

## 4. U2(b) — `FUN_0006f738` event types 1 and 4

### 4.1 Dispatch: types 1 and 4 both reach `FUN_0006f738`

`FUN_000b32ac` (VA `0xB32AC`, **file `0x115950`** — matches the already-published citation exactly)
reads the event-type discriminant `DAT_00244024` (VA `0x244024`, matches `[0x174024] + 0xD0000`,
the documented `.object2` remap) and dispatches:

- **Type 1** (or, when `DAT_000d5060 != 0`, type 4 folds into the same first branch): if
  `DAT_000d5060 == 0`, applies the already-documented `worth × 50` org-funds adjustment
  (`org+8 += eventWorth × −50`, tiered extra via `FUN_0005faf0`), gated on the event's building
  having a nonzero owner org, **then unconditionally calls `FUN_0006f738`**.
- **Type 4**: `if (DAT_00244024 == 4 && DAT_000d5060 == 0)` — the identical `worth × 50` adjustment
  (unconditional on owner-org this time) **then unconditionally calls `FUN_0006f738`**.

Both call sites (VA `0xB34C1`, `0xB3792`) are `FUN_0006f738`'s only two callers in the whole binary
— confirmed by `getReferencesTo`.

`FUN_0006f738` itself (VA `0x6F738`, bound-file `0xD1DDC`) transfers a 14-byte per-species
population array from the event's source org record (`param_1 + 0xD4`) to a destination org record
one byte-index over, rolling `FUN_0005D1D8` per unit moved and setting a base-slot "known" flag
(`DAT_000d96ea[slot]`, confirmed to sit exactly at `DAT_000d942c + 0x2BE` — i.e. the same
already-documented `+0x2BC`-adjacent field of the 16×`0x2BE` base array, not a separate parallel
table as an earlier pass over this data suspected) when the per-unit roll is `< moved_count × 5` —
the same `moved_count × 5` exposure formula already bound for `FUN_0006f7f8`.

### 4.2 The source: `FUN_000aff9c` is the type field's sole writer

`getReferencesTo(DAT_00244024)` returns 47 total xrefs; **exactly 5 are writes, and all 5 are in one
function**, `FUN_000aff9c` (VA `0xAFF9C`, object-page file `0x9FF9B`, bound-file `0x112640`):

```
000b0006  MOV dword ptr [0x244024],ESI     ; a computed classification value
000b0117  MOV dword ptr [0x244024],0x2
000b0128  MOV dword ptr [0x244024],0x4
000b0134  MOV dword ptr [0x244024],0x1
000b0143  MOV dword ptr [0x244024],0x3
```

The decompiled logic resolves the branch structure precisely: `FUN_000aff9c(param_1, param_2,
param_3)` builds the entire current-event struct from a `(buildingIndex, eventKind, extra)` triple.
When `param_2` is not `1` or `6`, `param_1` is resolved as a building record and the event type is
set from a classification value (`extraout_EDX` in the decompile — its true origin instruction
wasn't traced further, see caveat below): **`== 5 → type 2`, `== 3 → type 4`, otherwise → type
1**. Separately, if the caller passed `param_2 == 1` directly, the type is force-set to `3`
regardless of the building classification.

**This answers the task's question precisely for types 1 and 4**: type 1 is the default/fallback
classification for any resolved-building event whose classification value is neither 5 nor 3; type
4 is specifically the classification-value-3 case. Both are produced by the same function, the same
branch structure, gated on the same "is this a building event, not kind 1 or 6" precondition.

**Caveat, stated plainly:** the classification value itself (`extraout_EDX`) is fed from a `sVar2 =
FUN_0005f15c(); ... param_2 = extraout_EDX;` sequence a few lines earlier in the same function, but
the decompiler's register tracking doesn't show the exact defining instruction in the excerpt
captured this session. What's bound is the branch structure and literal type values; what isn't
independently re-verified this session is the ultimate name of the thing driving `extraout_EDX`
(most likely an organisation relation-tier or alert-stage value, given the surrounding code reads
the same `DAT_0016ec25` org-relation array used elsewhere in this subsystem, but that reading is
not asserted as bound here).

### 4.3 Corroboration and root

`FUN_000aff9c`'s only caller in the binary is `FUN_000ac348` (VA `0xAC348`), passing `param_1`/
`param_2` straight through from its own arguments. This matches
[O1-O2-M1-city.md §"O2"](O1-O2-M1-city.md)'s independent finding that `FUN_000aff9c` has exactly one
call site rooted in `FUN_0003a910` (this project's already-bound U1/U2 function) via `FUN_000ac08c`
— which is itself one of `FUN_0003a910`'s callees fired from the same role==2/role==3
mission-completion branches that clear `DAT_000e0cc0` (§3.2). **U1's mission-counter transition,
U2's base-exposure override clear, and U2(b)'s event-type population transfer are all downstream of
the same completion dispatch inside `FUN_0003a910`.**

**Verdict: dispatch cases (types 1 and 4 → `FUN_0006f738`) BOUND. The writer function and its exact
branch conditions for producing types 1 and 4 BOUND. The ultimate semantic driver of the
classification register feeding that writer NOT independently re-confirmed this session.**

---

## 5. V1 — vehicle attack-mode dodge / engagement table: NOT BOUND

### 5.1 Every candidate string has zero bound xrefs — reverified fresh

`QueryV1EngagementStrings.java` ran `getReferencesTo` on both the `.object2` runtime copy and the
`.image` copy of every UI string that could plausibly encode an engagement rule:

| String | `.image` file offset | bound xrefs |
|---|---|---|
| `Rules of engagement` | `0x152D10` | 0 |
| `Evade Fire` | `0x14CE9D` | 0 |
| `Evasive` | `0x152F13` | 0 |
| `Defensive` | `0x152F1B` | 0 |
| `Standard` | `0x152F25` (+ a second copy at `0x14A949`) | 0 |
| `Aggressive` | `0x152F2E` | 0 |
| `Low altitude` | `0x152ED7` | 0 |
| `Medium altitude` | `0x152EE4` | 0 |
| `High altitude` | `0x152EF4` | 0 |
| `Highest altitude` | `0x152F02` | 0 |

All ten strings, including the `.object2` runtime copies (`0xF166C` region), have `count=0` — no
code anywhere reads or references any of them. They sit contiguously in the UFO2P string table as
one vehicle-orders context-menu block (`Attack hostile unit … Return to base … Rules of engagement …
Low/Medium/High/Highest altitude … Evasive/Defensive/Standard/Aggressive`), consistent with them
being generic menu-item text drawn by a shared, unresolvable-by-string menu renderer — exactly the
failure mode the parity guide already documents for `Reload time:` and `Evade Fire`.

### 5.2 No literal dodge-percentage table exists in the binary

A byte-pattern search of the full 1,702,206-byte canonical `UFO2P.EXE` for `{100, 80, 50, 10}` and
its reverse, as `uint8[4]`, `uint16[4]` (little-endian), and `uint32[4]` (little-endian) arrays —
eight patterns total — found **zero matches** of any kind. If the original ladder existed as static
table data anywhere in this build, this search would have found it; it did not.

### 5.3 No field-based pivot available

`loftemps_index` at vehicle-data `+0x28` is confirmed (independently, again) to be exactly what the
task says it is and nothing else — the extractor's own consumer (`extract_vehicles.cpp`) uses it for
voxelmap misalignment, not dodge. No other runtime field was identified in this session's traversal
of the vehicle instance struct (`+0x104`, `+0x108`, `+0x12c`, `+0x12e`, `+0x168`, `+0x171`, all
touched in §§1–2 above) that reads as a 4-valued attack-mode/behavior enum, so there is no field to
pivot a reader-search from either.

**Verdict: NOT BOUND. No engagement table, no dodge-percentage table, and no consumer for any
engagement-related UI string exists anywhere in UFO2P.EXE that this session could find, despite an
exhaustive string-xref pass and a full-binary byte-pattern search. This closes the row as a genuine
negative result — the hardcoded `100/80/50/10` ladder in `vehicle.cpp:170` should stay exactly as
flagged (`FIXME`, not "implemented"), and no engagement-rules code should be added on the strength
of any finding in this document.**
