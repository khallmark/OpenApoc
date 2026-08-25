# U1 — the arrived-flag branch, `+0x168` reachability, and a re-audit of U2(a)

**Scope note on citations.** All addresses below are `UFO2P.EXE` (canonical, CRC32 `0x4749ffc1`), `.object1`
executable code. Two offset conventions are used, both already established in
[U1-U2-V1-incursion.md](U1-U2-V1-incursion.md): **VA** (the Ghidra listing address) and **object-page file**
(`.object1`'s `MemoryBlockSourceInfo` file-bytes offset). The VA → object-page-file delta for `.object1`
functions is the constant `−0x10001`, and it was **re-verified fresh this session** (not assumed) against
eight newly-cited functions via `QueryFunctions.java`'s own `page_file` field: `FUN_00059148`
(`0x59148→0x49147`), `FUN_00033e84` (`0x33e84→0x23e83`), `FUN_000588f8` (`0x588f8→0x488f7`, matches the
already-published citation), `FUN_0003a910` (`0x3a910→0x2a90f`, matches), `FUN_00058280`
(`0x58280→0x4827f`), `FUN_00057c8c` (`0x57c8c→0x47c8b`), `FUN_0006cb8c` (`0x6cb8c→0x5cb8b`), `FUN_0003046c`
(`0x3046c→0x2046b`). Every hit below is exact `page_file = VA − 0x10001`; individual instruction file
offsets are given as `VA − 0x10001` without re-stating the check each time.

**Xref counting method, stated explicitly (per the task's ground rules).** All caller/reader censuses below
used `currentProgram.getReferenceManager().getReferencesTo(Address)` — Ghidra's reference manager, which
indexes every operand type (`Address`-typed direct-memory operands included) — never the repo's
`QueryDataRange.java`, which filters candidate operands with `instanceof Scalar` and therefore **cannot see
x86 direct absolute-memory operands** (`MOV byte ptr [0x3009a0],CH` is an `Address`-typed operand object,
not a `Scalar`). Every count below was cross-checked by two independent means where practical (a custom
`getReferencesTo` script and `QueryFunctions.java`'s own caller list), and matched exactly both times.

---

## Verdicts

1. **The arrived-flag branch (`+0x16A`) has a real, byte-exact-verified reader: `FUN_00059148`.** It is
   called 9 times — 8 from inside `FUN_0003a910` (essentially at the tail of every mission-dispatch branch)
   and once from `FUN_000588f8` (the U1(b) gate function). On the tick it runs with the flag set, it either
   sends the vehicle toward a nearby vehicle matching a second field (`+0x16C`), or picks a fresh random
   waypoint from the vehicle's per-side mission-data catalog, or — if neither condition is met — **re-arms
   itself** for the next call. This is BOUND, with raw disassembly for every branch, and is enough to
   implement.

2. **`+0x168` vs constitution (`+0x12e`) — the gate is reachable in practice, and `+0x168` is not fixed
   after spawn either.** Two things the previous pass did not find:
   - `FUN_00057c8c`, a combat-damage-application function reachable from 4 external call sites (not part of
     the mission-dispatch subsystem), genuinely **subtracts damage from constitution** (`+0x12e`), and
     `FUN_0006cb8c`, a base-repair tick function, genuinely **adds repair back**, clamped to a per-type
     maximum. Constitution is not static — it moves in both directions during ordinary play.
   - `+0x168` itself is **recomputed from scratch at runtime**, not merely set once at spawn and then only
     ever clamped downward. `FUN_0005df1c`/`FUN_0005df98` share an identical formula
     (`percentTable[+0x166] × perTypeTable[type] / 100`) and one call site — inside `FUN_0003a910`'s
     retarget branch, i.e. an ordinary UFO-retargets-to-a-new-building event — **overwrites the value that
     the existing clamp logic had just set**, on every retarget. The earlier finding's "fixed fraction of
     spawn-time constitution that nothing ever raises" claim is **superseded** by this.
   - A genuinely new connective result: `FUN_000588f8` (the *same* function that houses the `+0x168` gate)
     independently **sets the arrived flag `+0x16A`** as an unconditional consequence of passing the gate.
     U1(a)'s "arrived" flag and U1(b)'s damage-threshold gate are not two separate mechanisms — crossing
     the threshold trips the *identical* flag consumed by the *identical* reader (`FUN_00059148`).

3. **U2(a) `DAT_000e0cc0` re-audit (added mid-task at the coordinator's request): the row survives.**
   `getReferencesTo` on `DAT_000e0cc0` (VA `0xe0cc0`) returns exactly **6** references this session, using
   the correct reference-manager API from the start — matching the original count and every individual site
   exactly. No nonzero write exists. Full re-enumeration in §4.

---

## 1. The arrived-flag branch: `FUN_00059148` is the reader

### 1.1 The write site, re-verified byte-exact

`FUN_0003a910`'s order-type-1 outcome of the mission-counter-zero transition (already bound in
[U1-U2-V1-incursion.md §1.2](U1-U2-V1-incursion.md)) was re-disassembled fresh this session. The full guard
and set, raw, with no decompiler involved:

```
0003ace1  MOV EAX,dword ptr [EDI + 0x16a]   ; file 0x2ace0
0003ace7  SAR EAX,0x10                       ; file 0x2ace6  -- keeps only the HIGH word: bytes [+0x16C,+0x16D]
0003acea  CMP EAX,-0x1                       ; file 0x2ace9
0003aced  JNZ 0x0003adb2                     ; file 0x2acec  -- skip the whole block unless +0x16C == -1
0003acf3  MOV CL,byte ptr [EDI + 0x171]       ; file 0x2acf2  -- mission counter
0003acf9  DEC CL
0003acfb  MOV byte ptr [EDI + 0x171],CL
0003ad01  JNZ 0x0003adb2                      ; only continue on the tick the counter reaches zero
0003ad07  CMP word ptr [EDI + 0x12c],0x1      ; file 0x2ad06  -- order-type field
0003ad0f  JNZ 0x0003ad1d                       ; not 1 -> retarget-search branch (already bound)
0003ad11  MOV byte ptr [EDI + 0x16a],0x1       ; file 0x2ad10  ***** THE ARRIVED FLAG, SET *****
0003ad18  JMP 0x0003adb2
```

**A correction to the existing write-up's citation, not its conclusion.** [U1-U2-V1-incursion.md §1.2]
documents the pre-decrement guard as `vehicle[+0x16A] == -1 (read as a dword, upper word == -1 "unset"
sentinel)`. That phrasing conflates two different fields. The instructions above show precisely what's
tested: the dword *at* `+0x16A` is read, but only its **high word** — bytes `+0x16C`/`+0x16D`, a distinct
field from the `+0x16A` byte flag itself — is compared against `-1`. This exact "read a dword, keep only
the high word" pattern recurs identically in `FUN_00059148` (§1.2 below, at VA `0x591ab`), which is strong
independent confirmation that `+0x16C` (not `+0x16A`) is the field this guard actually inspects. Nothing
about U1(a)'s bound behavior changes — the guard's effect is unchanged — but the field being tested is
`+0x16C`, and that matters for anyone implementing the pre-decrement guard from scratch. `+0x16A` itself
remains exactly the one-byte "arrived" flag documented previously and used throughout this file.

### 1.2 The reader: `FUN_00059148`, full raw walkthrough

`FUN_00059148` (VA `0x59148`, object-page file `0x49147`, `__regparm3`, single parameter = vehicle pointer
in `EAX`/`EDI`). **Callers: exactly 9**, confirmed twice (a dedicated `getReferencesTo` script and
`QueryFunctions.java`'s caller list agree exactly): 8 inside `FUN_0003a910` (VA `0x3ae57`, `0x3aed9`,
`0x3afc3`, `0x3b05f`, `0x3b09e`, `0x3b2ca`, `0x3b30e`, `0x3b328` — one at the tail of nearly every
mission-dispatch branch) and 1 inside `FUN_000588f8` (VA `0x58ac5`, see §2.5). This function's whole job
reads as "given the vehicle's current order-state fields, decide what `+0x4e/+0x50/+0x52` (its immediate
navigation target x/y/z) should be right now" — called synchronously after (almost) every state-changing
branch in the two functions that own order state.

**Block 1 — consume the arrived flag (raw, VA `0x59171`–`0x591ab`, file `0x49170`–`0x491aa`):**

```
00059171  MOV AH,byte ptr [EDI + 0x16a]     ; the arrived flag
0005917d  JZ 0x591ab                         ; flag clear -> skip this whole block
0005917f  MOV SI,word ptr [EDI + 0x12c]      ; order-type
00059186  CMP SI,0x1
0005918a  JNZ 0x591ab                        ; only order-type == 1
0005918c  CMP byte ptr [EDI + 0x271],0x0     ; an unnamed per-vehicle byte flag
00059193  JZ 0x59198... -> 0005919e
00059195  MOV byte ptr [EDI + 0x16a],0x1... 0x0   ; +0x271 != 0  -> CLEAR the flag, no further action
0005919c  JMP 0x591ab
0005919e  CMP byte ptr [0xd5060],0x0         ; DAT_000d5060, the established side/turn discriminant
000591a5  JNZ 0x591ab
000591a7  MOV dword ptr [ESP + 0x10],ESI     ; +0x271==0 && DAT_000d5060==0 -> latch "assign new wander destination"
```

So: if the flag is set and order-type is 1, `+0x271` decides the outcome. Nonzero `+0x271` **consumes and
clears** the flag with no further effect this call. Zero `+0x271`, gated further by `DAT_000d5060==0`,
marks a local "assign a new destination" intent (this is the decompiler's `bVar4`, confirmed to be the
`[ESP+0x10]` stack slot by the raw trace).

**Block 2 — the escort/rendezvous search, always run, gated on a *different* field (raw, VA
`0x591ab`–`0x592c0`, file `0x491aa`–`0x492bf`):**

```
000591ab  MOV EAX,dword ptr [EDI + 0x16a]
000591b1  SAR EAX,0x10                       ; high word = +0x16C, same pattern as §1.1
000591b4  CMP EAX,-0x1
000591b7  JZ 0x592db                         ; +0x16C == -1 (unset) -> skip the whole search, go to tail
000591bd  CMP word ptr [EDI + 0x12c],0x1
000591c5  JNZ 0x592db                        ; order-type must still be 1
000591cb  CMP byte ptr [EDI + 0xcf],0x1
000591d2  JNZ 0x591e6                        ; if a candidate is not already pending, start the scan
  ... (0x591e6-0x59289) scan all 80 vehicle-array slots (piVar-style loop, stride 0x276):
        slot valid (+0 dword high word != -1)
        DAT_000d5060 == slot[+0x174]           ; a per-vehicle byte set at spawn = DAT_000d5060 (FUN_0006eea4)
        self[+0x16C] == slot[+4]               ; "type to find" matches candidate's own +4 field
        self[+4]     != slot[+4]               ; excludes candidates sharing self's own +4 value
        slot[+0x271] == 0
        nearest by FUN_0005d288 distance, over self[+0x2e/+0x30] vs slot[+0x2e/+0x30]
000592af  CMP word ptr [0xd505c],0x0           ; DAT_000d505c, distinct global, 12 total xrefs, its only
                                                 ; writers are FUN_0006cfb4/FUN_0006d384 (both already-bound
                                                 ; UFO-role-assignment functions)
000592b7  JNZ 0x592c0
000592b9  MOV byte ptr [EDI + 0x16a],0x1        ; no match found && DAT_000d505c==0 -> RE-ARM the flag
000592c0  CMP byte ptr [EDI + 0x16a],0x0
000592c7  JZ 0x592d3
000592c9  MOV dword ptr [ESP + 0x10],0x1         ; flag still set at this point -> also "assign new destination"
000592d3  MOV dword ptr [ESP + 0x8],0x1           ; flag now clear -> "follow the found/pending candidate"
```

If no matching candidate is found and `DAT_000d505c == 0`, the flag is put right back — the function
re-arms its own trigger for the next time it's called, i.e. a retry-until-a-candidate-exists loop, not a
one-shot consumption.

**Resolution — the two outcomes, fully raw-verified (VA `0x59406`–`0x59536` and `0x59480`–`0x59536`, file
`0x49405`–`0x49535`):**

- **"Assign new wander destination"** (only when order-type == 1): calls `FUN_0005d360` (VA `0x59426`) — a
  10-entry nearest-match scan over the per-side block of the mission-data catalog
  (`0x1439e0 + DAT_000d5060×0x2D4`, the same table already tied to `UFO_mission_data` in U1(a)) using
  `FUN_00038678` for the distance compare (the same primitive already bound inside `FUN_0004db84`). If a
  match is found, it directly commits `+0x4e/+0x50/+0x52` (nav x/y/z) from that catalog entry, sets
  `+0xCF = 2`, mirrors `+0x100 → +0x102`, and stages `+0xD0/+0xD2` from the new `+0x4e/+0x50` — VA
  `0x59435`–`0x59474`.
- **"Follow the candidate"**: reads the candidate's position (self `+0xCE` index into the vehicle array, or
  the game's currently-selected vehicle if `BX != 0`), sets `+0x4e/+0x50/+0x52` from it, and adds a random
  jitter to each axis via three separate `FUN_0005d1d8` calls — VA `0x594a2`–`0x59529`.

**What this means for implementation.** The reader's contract is precise and small: a vehicle with
order-type 1 whose arrived flag is set either (a) rendezvous with the nearest other active, same-side,
`+0x271==0` vehicle whose `+4` field matches its own `+0x16C` field (excluding vehicles that share its own
`+4`), with position jitter, or (b) if no such vehicle exists yet, retries every call until one does (or
until `DAT_000d505c` becomes nonzero), and once the retry path is abandoned in favor of the alternate gate
(`+0x271 != 0` — see Block 1), the flag is simply cleared; or (c) if the `+0x16C` "type to follow" field was
never set at all, picks a fresh random destination from its role's spawn-time mission-data catalog block.
**None of these three outcomes are named here** (no "escort", "patrol", or "loiter" label is asserted) —
they are described purely by offset and observed behavior, per the task's constraint.

### 1.3 A secondary, corroborating reader: `FUN_00033e84`

`FUN_00033e84` (VA `0x33e84`, object-page file `0x23e83`, single caller: `FUN_00033818` @ VA `0x33adf` —
not itself examined this session) also reads `+0x16A`, as one of three OR'd trigger conditions
(`+0x16A != 0`, or a per-role-type table lookup at `DAT_0012d905[+0x166×0x94]`, or `+0xCF == 1`) gating a
call to `FUN_0003046c` (VA `0x3046c`, file `0x2046b` — a route/pattern-recompute primitive that fills a
0xB-byte route-pattern buffer and writes `+0x66/+0x10C/+0x10E/+0x172`, unrelated to sound or UI despite
initial appearance). Separately in the same function, when `+0x16A == 0` and `+0xCF == 1` with a
now-invalid candidate index at `+0xCE`, it clears the stale candidate by writing `+0xCF = 2` and re-seating
`+0xD0/+0xD2` from the vehicle's own current position (VA range captured via decompile in
`export/ufo2p_off171.log:495-605`, not independently raw-verified this session — flagged as decompiler-tier
evidence, lower confidence than §1.2's raw-verified reads). This is not required to answer the task's
question (§1.2 already gives a complete, implementable reader) but is recorded as corroboration that
`+0x16A` is read from more than one place in the binary, all consistently as a plain boolean latch.

---

## 2. `+0x168` vs constitution: the gate is reachable, and the threshold itself moves

### 2.1 Exhaustive re-scan, corrected accounting

A register-based (not `Scalar`-only) instruction scan across all of `.object1`'s executable range for any
instruction touching displacement `0x12e` or `0x168` found **33 hits at `0x12e`, 22 hits at `0x168`**.
Filtering out false positives (`[ESP+0x12e]`/`[ESP+0x168]` are local stack variables at that displacement in
unrelated functions — `FUN_000293c4`, `FUN_00043b28`, `FUN_000aac88` — not the vehicle struct) leaves:

**`+0x12e` (constitution), all struct-relative (`reg+0x12e`, reg ∈ {EBX,EDX,EBP,ESI,EDI,ECX,EAX}), 5
genuine stores out of the register-relative hits:**

| VA | file | Function | What |
|---|---|---|---|
| `0x5dc06` | `0x4dc05` | `FUN_0005d6e4` | spawn write (already bound) |
| `0x581ea` | `0x481e9` | `FUN_00057c8c` | **NEW — damage: `constitution -= damage`** |
| `0x5835b` | `0x4835a` | `FUN_00058280` | **NEW — retire: `constitution = 0`** |
| `0x6cc38` | `0x5cc37` | `FUN_0006cb8c` | **NEW — repair: `constitution += repairAmount`** |
| `0x6cc57` | `0x5cc56` | `FUN_0006cb8c` | **NEW — repair clamp: `constitution = perTypeMax`** |

**`+0x168`, all struct-relative, 5 genuine stores:**

| VA | file | Function | What |
|---|---|---|---|
| `0x6dbdf` | `0x5dbde` | `FUN_0006da88` | spawn write #1 (already bound) |
| `0x70e00` | `0x60dff` | `FUN_00070cc0` | spawn write #2, literal 0 (already bound) |
| `0x5dceb` | `0x4dcea` | `FUN_0005d6e4` | spawn write #3 (already bound) |
| `0x4dff2` | `0x3dff1` | `FUN_0004dd14` | clamp-down (already bound) |
| `0x4e188` | `0x3e187` | `FUN_0004e0d4` | clamp-down (already bound) |
| `0x5dfe2` | `0x4dfe1` | `FUN_0005df98` | **NEW — full runtime recompute** |
| `0x5df6e` | `0x4df6d` | `FUN_0005df1c` | **NEW — full runtime recompute (+ writes `+0x15C`)** |

(`FUN_0005df1c`/`FUN_0005df98` are two entries in the same table because they are two distinct functions
sharing one formula — see §2.3.)

### 2.2 `FUN_00057c8c`: constitution genuinely takes damage

`FUN_00057c8c` (VA `0x57c8c`, file `0x47c8b`, `__regparm3(vehicleIndex, param2, param3)`). **4 callers**,
all outside the mission-dispatch subsystem: `FUN_00054a28` (VA `0x55a44`), `FUN_00080528` (VA `0x80685`,
`0x806a2`), `FUN_0007fdb0` (VA `0x7fe9d`) — none of these were examined this session, but their address
range (`0x54xxx`/`0x7fxxx`/`0x80xxx`) is well outside the UFO-mission-counter code this whole file otherwise
covers, consistent with a combat/weapon-hit resolution area, not the mission dispatcher calling itself.

Mechanically: rolls a random amount (`FUN_0005d1d8`), absorbs it against one of six directional
"shield"-like fields (`+0x17A`/`+0x17C`/`+0x17E`/`+0x180`/`+0x182`/`+0x184`, selected by an unnamed
parameter) before it ever reaches constitution. If the leftover damage is `>=` current constitution
(`(short)vehicle[+0x106][iVar9*0x13b] <= local_10`, i.e. `+0x12e <= damage`), the function builds and
dispatches a formatted "destroyed" message via `FUN_0002ae1c`, calls `FUN_00058280` (§2.4) to retire the
vehicle, and **returns without ever executing the subtraction below** — constitution is *not* independently
zeroed here; `FUN_00058280` does that. Otherwise, at VA `0x581ea` (raw: `MOV word ptr [EBP+0x12e],DI`):

```c
(&DAT_00161106)[iVar9*0x13b] = (&DAT_00161106)[iVar9*0x13b] - (short)uVar6;   // constitution -= accumulated damage
```

then compares the new constitution against `*(short*)(&DAT_00128618 + type*0x7E)` — the **same** per-type
table row already tied to `+0x168`'s spawn formula and to `FUN_000588f8`'s gates (§2.5) — and if it dropped
below that value, dispatches a tiered "damaged" message (`FUN_000409e0`, `FUN_000983ec`) and, when the
vehicle is currently player-selected, an org-funds adjustment via `FUN_0005faf0` (the same call already
bound for U2(b)'s worth-based org-funds settlement).

**This directly answers the reachability half of task item 2**: constitution is lowered by ordinary combat
damage, through a function called from outside the mission-dispatch code entirely, with a concrete,
raw-verified instruction doing the subtraction.

### 2.3 `FUN_0006cb8c`: a matching repair writer

`FUN_0006cb8c` (VA `0x6cb8c`, file `0x5cb8b`, `__regparm3(baseIndex, param2)`, single caller `FUN_0006ca60`
@ VA `0x6cab5`). Scans all 80 vehicle slots; for each slot that is valid, `+0x4b == 0`, `+0x5d == 0`
(matching the "side 0" convention seen elsewhere), and whose `+0x108`-equivalent field (`piVar6[0x42]`,
i.e. byte offset `0x108`) matches the repairing base's own building index, and whose constitution is below
`&DAT_00128616 + type×0x7E` (the per-type max, two bytes before the `0x128618` row used in §2.2/§2.5):

```c
sVar3 = constitution + repairDelta;
constitution = sVar3;
if (perTypeMax <= sVar3) {
    constitution = perTypeMax;         // VA 0x6cc57, file 0x5cc56 — clamp to max
    // dispatch a "repaired" message (FUN_00063a00 / FUN_0002ae1c)
}
```

(The add itself is at VA `0x6cc38`, file `0x5cc37`.) This is a base-side repair tick — constitution moves
back up for docked, damaged, side-0 vehicles, capped at the same per-type maximum used throughout §2.

### 2.4 `FUN_00058280`: constitution zeroed as part of vehicle retirement, not combat per se

`FUN_00058280` (VA `0x58280`, file `0x4827f`) sets `*(short*)(iVar3+0x12e) = 0` at VA `0x5835b` (file
`0x4835a`) as one step of a broader "tear down this mission/vehicle slot" cleanup: decrements a per-role
active-mission counter (`DAT_00143a6c`, the same table `FUN_0003a910` touches in its own mission-offer
logic), clears a "known building" cache slot when `+0x2 == 1`, resets the game's selected-vehicle index if
it pointed at this slot, and calls three more cleanup functions (`FUN_00058400`, `FUN_000583c8`,
`FUN_0009162c`). It is called from `FUN_00057c8c`'s lethal-damage branch (§2.2), and also from three sites
inside `FUN_0003a910` unrelated to combat (declined-mission and no-valid-slot cleanup paths, VA `0x3aa92`,
`0x3afae`, `0x3b104`/`0x3b34c` region). **This writer is real and lowers constitution to exactly zero, but
it is a retirement/cleanup side effect, not itself the primary "combat damage" mechanism** — that is
§2.2's `FUN_00057c8c`, which calls this function rather than being called by it in the lethal case.

### 2.5 `FUN_000588f8` re-examined: two more gates on the same per-type table, and a new flag write

The existing write-up (§2.1 of the prior doc) cites only the three-instruction gate at function entry. Full
raw disassembly this session (VA `0x58925`–`0x58b84`) shows the function continues well past that point,
and two more comparisons against the **same** `0x128618`-stride-`0x7E` per-type table appear:

```
00058925  MOV AX,word ptr [EBP + 0x168]
0005892c  CMP AX,word ptr [EBP + 0x12e]
00058933  JLE 0x58a48                      ; gate 1 (already documented): +0x168 <= constitution -> skip

00058939  MOV EDX,dword ptr [EBP + 0x2]
0005893c  SAR EDX,0x10                     ; EDX = vehicle[+4] (type)
0005893f  MOV EAX,EDX
00058941  SHL EAX,0x6
00058944  SUB EAX,EDX                      ; EAX = type * 0x3F
00058946  MOV DX,word ptr [EBP + 0x12e]    ; constitution
0005894d  CMP DX,word ptr [EAX*2 + 0x128618]   ; gate 2 (NEW): constitution vs perTypeTable[type*0x7E + 4]
00058955  JL 0x58a48                        ; constitution below that floor -> skip everything below

0005895b  MOV CX,word ptr [EBP + 0x12c]      ; order-type
00058962  MOV byte ptr [EBP + 0x16a],0x1     ; ***** THE ARRIVED FLAG, SET — INDEPENDENTLY OF FUN_0003a910 *****
00058969  TEST CX,CX
0005896c  JNZ 0x58a48                        ; order-type must be 0 to continue to the message-building path
```

So: once `+0x168 > constitution` **and** `constitution >= perTypeTable[type×0x7E+4]` (i.e. the vehicle has
crossed the alert threshold but is still above some type-specific floor), the function **unconditionally
sets the arrived flag** — the exact same `+0x16A` byte set by `FUN_0003a910`'s order-type-1 branch (§1.1)
and consumed by `FUN_00059148` (§1.2). Only *after* that does it check order-type again to decide whether to
build and dispatch the UI message (`FUN_00063a00` name-fetch + `FUN_0002ae1c` event-queue dispatch, order-
type 0 only) — the flag write does not depend on that second check.

Later in the same function, after the general "is this vehicle still valid" re-check (VA `0x58a48`), a
**third** reference to the identical per-type table gates the second `FUN_00059148` call:

```
00058aa9  MOV AX,word ptr [EBP + 0x12e]         ; constitution
00058ab0  CMP AX,word ptr [EDX + 0x128618]        ; EDX = type*0x7E (recomputed the same way)
00058ab7  JGE 0x58ac3                              ; constitution >= perTypeTable[...] -> skip the order-type check
00058ab9  CMP word ptr [EBP + 0x12c],0x1
00058ac1  JZ 0x58aca                                ; order-type == 1 -> skip the FUN_00059148 call
00058ac3  MOV EAX,EBP
00058ac5  CALL 0x00059148                            ; file 0x48ac4 — the reader from §1.2
```

I.e., `FUN_00059148` is called from here specifically **when constitution has fallen below the per-type
floor and the vehicle's order-type is not 1** — a second, independent trigger for the same reader, unrelated
to whether the message-gate above fired.

**This closes the causal loop the task asked about.** U1(a)'s "arrived" flag is not solely a
mission-counter artifact — `FUN_000588f8`, the function that owns the U1(b) constitution-threshold gate,
sets the identical flag as a direct consequence of the threshold being crossed, and the identical consumer
(`FUN_00059148`) reacts to it either way. Whether to *name* this connection (e.g. as a "flee" or "retreat"
behavior) is not asserted — no such label appears anywhere in the traced code — but the mechanism itself
(same flag, same reader, two independent writers, one keyed to mission progress and one keyed to damage) is
fully bound.

### 2.6 `+0x168` moves at runtime too: `FUN_0005df1c` / `FUN_0005df98`

Two small functions share one formula, both raw-verified:

```
FUN_0005df98 (VA 0x5df98, file 0x4df97, 85 bytes):
  +0x168 = (byte)DAT_0012d950[+0x166 * 0x94] * (short)(&DAT_00128614 + type*0x7E)[hi-word] / 100

FUN_0005df1c (VA 0x5df1c, file 0x4df1b, 122 bytes):
  <identical computation, same store at VA 0x5df6e (file 0x4df6d)>
  +0x15c = DAT_0012d94e[+0x166 * 0x94]        ; also refreshes the role/order-state field
```

Both use the **`0x128614`-row, high-word (`+2` relative)** catalog value as their multiplicand — the same
value `FUN_00057c8c` reads for its severity tiering (§2.2), and the value the original write-up already
correctly identified as `FUN_0005d6e4`'s (one of the three known spawn writers') multiplicand. This is
*not* the "misread multiplicand" the task warned about (that correction, already reflected in the current
doc, concerns `FUN_0006da88`'s use of live `+0x12e` rather than this catalog constant) — it is a distinct,
previously untraced pair of functions.

**Callers, `getReferencesTo`, exact counts:**

- `FUN_0005df1c`: **9 callers.** One is `FUN_0003a910` at VA `0x3ad93` — inside the U1(a) retarget branch,
  **immediately after** `FUN_0004e0d4`'s "clamp `+0x168` down if it's `>= +0x12e`" logic (already bound,
  §1.3 of the prior doc) — i.e. every time a UFO picks a new mission-destination building at runtime,
  `+0x168` is **recomputed from the catalog formula, overwriting whatever the clamp had just set it to**.
  Two more callers are the already-known spawn sites, `FUN_0006da88` (VA `0x6dba0`) and `FUN_0006de64` (VA
  `0x6dec3`, the generic vehicle-slot allocator). The remaining six (`FUN_00010380`, `FUN_00059e60`,
  `FUN_0005a120`, `FUN_0005a918`, `FUN_0005b22c`, `FUN_0005ca04`) were not examined this session.
- `FUN_0005df98`: **1 caller**, at VA `0x51f40`, inside an address range Ghidra does not currently resolve
  to a named function (`getFunctionContaining` returned none) — not characterized further this session.

**What this means.** The previous finding's central claim — "`+0x168` starts as a fraction of constitution
at spawn and nothing ever raises it afterward, so the gate can only fire if constitution falls" — is only
half right. Constitution genuinely does fall (§2.2), which is sufficient on its own to make the gate
reachable. But `+0x168` is *also* not a fixed spawn-time constant: it is unconditionally recomputed on at
least one confirmed, ordinary, non-spawn gameplay event (retargeting to a new mission building), using the
same catalog-percentage formula as spawn. Whether the post-retarget value ends up higher or lower than the
pre-retarget (clamped) value depends on live game data (current type, current `+0x166` role index) not
resolved here — so "does this specific write raise or lower `+0x168` in any given instance" is not asserted
— but the *mechanism* is a full recompute, not a monotonic decay, and that is the corrected, bound fact.

### 2.7 Verdict

**BOUND, both halves.** (a) Constitution is lowered by combat damage (`FUN_00057c8c`) and raised by base
repair (`FUN_0006cb8c`), both raw-verified, both reachable from ordinary gameplay outside the
mission-dispatch subsystem. (b) `+0x168` is not merely set once at spawn and monotonically clamped down — it
is recomputed from the same catalog formula at runtime on at least one ordinary event (mission retargeting).
Between these two facts, `FUN_000588f8`'s gate (`+0x168 > +0x12e`) is unambiguously reachable in normal
play, and — newly bound this session — firing it sets the same "arrived" flag that U1(a) sets, consumed by
the same reader (`FUN_00059148`, §1.2). No name is asserted for `+0x168`, `+0x16A`, `+0x16C`, `+0x271`,
`+0x174`, `+4`, `+0xCE`, `+0xCF`, or `+0xD0` beyond offset and observed behavior, per the task's constraint.

---

## 3. What remains open

- No semantic name for any of the fields above. Describing `+0x16A`/`+0x16C` as "escort"/"rendezvous" or
  `+0x168` as a "morale"/"alert" threshold would be inventing a concept no code or string ties to it.
- `FUN_00033e84`'s own caller (`FUN_00033818`) and `FUN_0003046c`'s full role were not chased to a
  conclusion — secondary corroboration only, not required for the primary answer.
- The six unexamined callers of `FUN_0005df1c` and the one unresolved caller of `FUN_0005df98` (§2.6) may
  contain further non-spawn recompute sites; not characterized this session.
- The 4 external callers of `FUN_00057c8c` (§2.2) were not opened; "combat/weapon-hit resolution" is an
  address-range inference (VA `0x54xxx`/`0x7fxxx`/`0x80xxx`, outside this file's subsystem), not a traced
  call chain.
- Whether a lethal `FUN_00057c8c` hit and a same-tick `FUN_000588f8` evaluation can race (i.e. whether the
  gate could ever evaluate against a vehicle slot `FUN_00058280` just retired) was not resolved.
- `DAT_000d505c`'s exact semantics beyond "read once in `FUN_00059148`, written only by the two already-bound
  UFO-role-assignment functions" were not pursued (12 total xrefs, small enough to fully resolve if this
  becomes load-bearing later).

---

## 4. Addendum — U2(a) `DAT_000e0cc0`: re-audited with `getReferencesTo`, row confirmed closed

**Why this section exists.** A parallel session working on `TACP.EXE` found that this research lab's
`QueryDataRange.java` script filters candidate operands with `instanceof Scalar`, which cannot match x86
direct absolute-memory operands (`MOV byte ptr [addr],reg` is an `Address`-typed operand, not a `Scalar`).
Every "zero hits" result that script ever produced is suspect. `FUN_0003a910` — the same function this
file's §1 is about — houses two of `DAT_000e0cc0`'s six previously-documented references, so the coordinator
asked for an independent re-enumeration here rather than trusting the existing write-up blind.

**Method, stated explicitly.** `currentProgram.getReferenceManager().getReferencesTo(Address)` — Ghidra's
reference manager, called directly against `DAT_000e0cc0` (VA `0xe0cc0`) in a fresh script this session
(`QueryU1_168WriterCallers.java`, appended query). This is **not** `QueryDataRange.java` and does not share
its bug: the reference manager indexes references by their analyzed type regardless of whether the operand
is `Scalar` or `Address`-typed, so it sees direct-memory-operand references that the buggy script would
miss. It is also worth noting the *original* U2(a) finding never used the buggy script either —
[U1-U2-V1-incursion.md §3.1](U1-U2-V1-incursion.md) cites `QueryE0cc0Lifecycle.java`, "exhaustive
`getReferencesTo`" — but the coordinator asked for a fresh, independent re-run rather than trusting that
citation, so this section is a genuine re-derivation, not a re-statement.

**Result: exactly 6 references, matching the original enumeration address-for-address.**

| VA | file (`VA−0x10001`) | Direction | Function | Instruction |
|---|---|---|---|---|
| `0x3ae5c` | `0x2ae5b` | WRITE | `FUN_0003a910` | `MOV dword ptr [0xe0cc0],EDX` |
| `0x3b064` | `0x2b063` | WRITE | `FUN_0003a910` | `MOV dword ptr [0xe0cc0],ECX` |
| `0x6d004` | `0x5d003` | READ | `FUN_0006cfb4` | `CMP dword ptr [0xe0cc0],0x0` |
| `0x70321` | `0x60320` | READ | `FUN_000702e4` | `CMP dword ptr [0xe0cc0],0x0` |
| `0x705ac` | `0x605ab` | READ | `FUN_000705a8` | `MOV ESI,dword ptr [0xe0cc0]` |
| `0x705ea` | `0x605e9` | WRITE | `FUN_000705a8` | `MOV dword ptr [0xe0cc0],ESI` |

**Both writes re-confirmed as literal-zero, fresh raw disassembly, this session:**

```
; site 1, VA 0x3ae55-0x3ae5c, file 0x2ae54-0x2ae5b
0003ae55  XOR EDX,EDX
0003ae57  CALL 0x00059148        ; the exact reader from §1.2 — prologue re-confirmed this session as
                                   ; PUSH EBX; PUSH ECX; PUSH EDX; PUSH ESI; PUSH EDI; PUSH EBP; SUB ESP,0x20,
                                   ; a full push/pop bracket around EDX -> EDX survives the call unchanged
0003ae5c  MOV dword ptr [0xe0cc0],EDX

; site 2, VA 0x3b05d-0x3b064, file 0x2b05c-0x2b063
0003b05d  XOR ECX,ECX
0003b05f  CALL 0x00059148
0003b064  MOV dword ptr [0xe0cc0],ECX
```

**The third "write" (`FUN_000705a8` @ VA `0x705ea`) re-confirmed as an unmodified round-trip, this session:**

```
000705ac  MOV ESI,dword ptr [0xe0cc0]     ; read once, into ESI
000705bd..0x705e6  <16-iteration loop over base slots, stride 0x2BE — ESI is never referenced inside it>
000705e8  MOV EAX,EDI
000705ea  MOV dword ptr [0xe0cc0],ESI     ; stored straight back, unmodified
```

**No instruction anywhere among these 6 references sets `DAT_000e0cc0` to a nonzero literal or a
computed-nonzero value.** The two genuine writes are provably zero (register self-XORed immediately before,
proven not clobbered across the intervening call); the third is a provable no-op round-trip.

**Verdict: the row stays CLOSED.** The count (6), the addresses, and the read/write classification are
identical to the existing write-up, independently reproduced this session with the explicitly-correct API
and fresh raw disassembly for every write site. The `QueryDataRange.java` Scalar-operand bug the parallel
session found is real and worth checking against every other "zero hits" claim in this project that used
that specific script — but this particular finding was never exposed to it, and the fresh re-audit changes
nothing about its conclusion.
