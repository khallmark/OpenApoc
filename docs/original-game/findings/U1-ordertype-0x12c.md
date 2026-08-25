# U1 — vehicle field `+0x12C`: writer census, consumer, and reachability

**Scope note on citations.** Binary: `UFO2P.EXE` (canonical, CRC32 `0x4749ffc1`). All VAs are `.object1`
Ghidra addresses; file offsets are `.object1`'s `MemoryBlockSourceInfo` file-bytes offset, computed as
`VA − 0x10001` (re-confirmed this session against `FUN_0005d6e4` — reported `page_file=0x4d6e3` for entry VA
`0x5d6e4` by `QueryFunctions.java`'s own field — and against `FUN_0003a910`/`FUN_000588f8`, both of which
reproduce the citations already published in the sibling docs). Every instruction below is quoted from the
raw listing, not the decompiler, except where explicitly marked "decompile-tier."

**Label caution, honored throughout.** "Order-type" is the label this field inherited from an earlier
write-up in this project. Nothing below treats that label as established. The field is described purely by
what its writers and its consumers do.

---

## Verdict

**BOUND: the writer census is exhaustive over Ghidra-disassembled instructions in `.object1`'s executable
range, and byte-verified. The mission-counter-zero retarget branch's consumer is now also byte-verified (new
this session — the earlier write-ups left it unresolved). The "does the spawn-time *value* come from role /
mission-type / per-vehicle-type catalog" question is answered, and the answer is no on all three counts,
cleanly proven, not inferred. A second question this task didn't explicitly ask but that turned out to be
decisive — *which roles ever reach the branch at all* — is also now resolved, byte-exact: of the five named
roles, Attack and Escort reach it; Infiltration, Subversion, and Overspawn are excluded by an entirely
separate guard before `+0x12C` is ever read, and use a different, already-documented completion path
instead. For the roles that do reach it, the guard's own logic (not just the writer census) shows the
retarget branch is structurally unreachable for Escort, and unproduced by any writer this session found for
Attack — this population always resolves to "arrived → nearest gate," never "retarget" (§5.3). `+0x12C`
also turns out to be tested at several more points inside `FUN_0003a910` than the single branch this task
named, disclosed but not resolved to the same depth (§3.4).**

1. **Exactly two functions write `+0x12C` anywhere in `.object1`**, confirmed by a full register-relative
   displacement scan for `0x12c` across the executable range (140 raw hits, filtered for `ESP`-relative stack
   locals in unrelated functions per the task's warning, then filtered again to true writes by excluding
   `CMP`/`TEST`): `FUN_0005d6e4` (the common vehicle-spawn initializer, one write, §1.2) and `FUN_000b44a4` (a
   rare, narrow-scope utility, one write, §1.3). The only `ADD`/`INC` hits the scan found for this displacement
   were both `ESP`-relative false positives in unrelated functions (`FUN_00043b28`, `FUN_0001a088`), same
   failure mode the sibling `+0x12E`/`+0x168` scan already flagged; no genuine `ADD`/`SUB`/`INC`/`DEC`/`OR`/
   `AND`/`XOR` writer of `+0x12C` exists.

2. **`+0x12C` is *not* the UFO role, proven by direct raw evidence, not absence of evidence.** In
   `FUN_0006da88` — the dimension-gate incursion-UFO spawn function already cited in
   `game/state/city/vehiclemission.h:212-215` for three other fields — the incoming role argument travels via
   `BX`, is saved (`0x6da98`), possibly escalated `9→10`, and is written to `+0x166` (`0x6db98
   MOV byte ptr [ESI+0x166],AL`). The **same register** is then clobbered with a **hardcoded literal `1`**
   (`0x6dae3 MOV EBX,0x1`) before the call that sets `+0x12C` (§2.1). Role goes to `+0x166`. `+0x12C` gets a
   constant, independent of role, for every incursion role this function spawns.

3. **`+0x12C`'s consumer on the mission-counter-zero retarget branch (new this session) is
   `FUN_00091f70`'s search argument.** Raw, byte-exact (file offsets below): `+0x12C` (read as the high word
   of the dword at `+0x12A`) is passed directly as the argument to `FUN_00091f70`, which — per the existing
   binding in [U1-U2-V1-incursion.md §1.3](U1-U2-V1-incursion.md) — scans the `DAT_0018276c` table (stride
   `0xE2`, count `DAT_00183a68`) for a record whose own `+0xC8` byte equals the argument. **This is the same
   table [O1-O2-M1-city.md §"Cargo"](O1-O2-M1-city.md) independently identified as a per-org/per-base status
   table.** So: on this specific branch, `+0x12C`'s value selects **which org/base-status record** to match,
   not a UFO role, not an incursion/mission type, and not a per-vehicle-type catalog row. **This is not the
   field's only test site** — §3.4 discloses at least three more structurally distinct blocks inside
   `FUN_0003a910` alone that also test `+0x12C`, not resolved to the same depth this session.

4. **Which vehicles get `+0x12C == 1` *at spawn*.** Every hardcoded write found this session sets it to `1`:
   `FUN_0006da88` (the primary dimension-gate incursion-UFO spawn, §2.1), `FUN_0006de64` (a generic
   vehicle-slot allocator with defaults that read as non-AI/no-scripted-mission creation, §2.2), and
   `FUN_00070cc0` (a probabilistic reinforcement spawn called **from inside `FUN_0003a910` itself**, §2.3).
   The only writers producing anything *other* than `1` are `FUN_000631e0` (copies a byte straight out of the
   `DAT_0018276c` org/base-status catalog, §2.4) and a sampled subset of `FUN_0005d68c`'s 17 callers (literal
   `0`, `3`, `9` confirmed at specific sites; a few not fully traced, §2.5). **This is a claim about the
   *value written at spawn*, not by itself about which branch of `FUN_0003a910` a vehicle subsequently
   reaches** — whether the mission-counter-zero transition is reached at all depends on the separate,
   role-derived field `+0x15C`, now resolved byte-exact (§5): Attack- and Escort-role UFOs reach it (and
   always find `+0x12C == 1`); Infiltration-, Subversion-, and Overspawn-role UFOs never reach it at all,
   excluded by a guard before `+0x12C` is read. `+0x12C` is also tested again, differently, in at least three
   other places inside the same function (§3.4). Full implementation implication in §5.

---

## 1. The two writers, byte-verified

### 1.1 Scan methodology, and what "exhaustive" does and doesn't mean here

A register-relative displacement scan (`QueryOrderType12cScan.java`, adapted from the lab's existing
`QueryVehicleOff171.java` pattern) walked every **Ghidra-disassembled `Instruction`** in `.object1`'s
executable range and flagged any `DYNAMIC` operand containing the scalar `0x12c`. This is exhaustive over
what Ghidra resolved to instructions, not over every byte of `.object1` — two known gaps exist and are
disclosed rather than assumed away: this session's own `DumpListingRange` dump of `FUN_000631e0` opens with
an unresolved raw byte (`000634a0 db 00`) immediately before the function, and
[U1-arrived-flag-and-0x168.md §2.6](U1-arrived-flag-and-0x168.md) records a `FUN_0005df98` caller at VA
`0x51f40` inside a region Ghidra does not currently resolve to a named function. Neither gap is likely to
hide a `+0x12C` writer (both are small, local discontinuities, not large unanalyzed regions), but the claim
below is scoped precisely: **exhaustive over Ghidra-disassembled instructions**, not over the raw file.

Raw result: **140 hits, 82 distinct functions** (a much larger haystack than `+0x12E`/`+0x168` — `0x12C`
collides with unrelated struct fields all over the binary, not just the vehicle array). Filtering:

- **`ESP`-relative hits are stack locals in unrelated functions, not the vehicle struct** — 11 of the 140,
  across `FUN_0001a088`, `FUN_00043b28`, `FUN_000b1000`, matching the exact failure mode the task warned
  about (the sibling `+0x12E`/`+0x168` scan hit the same thing in `FUN_000293c4`/`FUN_00043b28`/
  `FUN_000aac88`). Discarded.
- Of the remaining 129 register-relative hits, **only 2 are writes** (`MOV`, memory operand as destination) —
  everything else is `CMP`/`TEST` (83 hits) or a `MOV` reading *from* `+0x12C` (55 hits, including several
  dword-sized reads that are actually `+0x12E` constitution reads via the established "read a dword, keep the
  high word" idiom — see the caution in §3.3). The `ADD`/`INC` hits found by the scan (`FUN_00043b28`,
  `FUN_0001a088`) are also `ESP`-relative and discarded with the rest of that group.
- No absolute-address writer (a literal `MOV word ptr [0x1XXXXX],val` hardcoding one specific vehicle slot)
  was found or expected — every genuine access to this struct in the whole binary uses register-relative
  addressing, consistent with a per-slot array field, not a singleton.

**The two surviving writes:**

| VA | file | Function | Instruction |
|---|---|---|---|
| `0x5dbdc` | `0x4dbdb` | `FUN_0005d6e4` | `MOV word ptr [EBP + 0x12c],AX` |
| `0xb44d3` | `0xa44d2` | `FUN_000b44a4` | `MOV word ptr [EAX + 0x12c],0x0` |

### 1.2 `FUN_0005d6e4` writes from its caller's `BX`, byte-verified (not decompiler-inferred)

`FUN_0005d6e4` (VA `0x5d6e4`, file `0x4d6e3`, `int __regparm3(short param_1, short param_2, short *param_3)`)
is the shared per-vehicle spawn-field initializer — the same function already bound in
[U1-arrived-flag-and-0x168.md §2.2](U1-arrived-flag-and-0x168.md) for one of `+0x168`'s three spawn writers.
Ghidra's decompile renders the write as `param_3[0x96] = unaff_BX;` (`unaff_BX` = a 4th, implicit argument
the compiler passed via `BX`, invisible to the 3-argument signature). Per the task's own caution against
decompiler-tier inference on a load-bearing claim, this was checked against the raw bytes rather than trusted:

```
0005d6f4  MOV dword ptr [ESP + 0x24],EBX   ; file 0x4d6f3 -- entry prologue: caller's EBX parked on the stack
0005d6f8  MOV EBP,ECX                       ; file 0x4d6f7 -- param_3 (vehicle pointer) = ECX
...
0005dbd8  MOV EAX,dword ptr [ESP + 0x24]    ; file 0x4dbd7 -- reloaded from the EXACT slot EBX was parked in
0005dbdc  MOV word ptr [EBP + 0x12c],AX     ; file 0x4dbdb -- ***** +0x12C WRITE *****
```

**Confirmed, byte-exact: the value written to `+0x12C` is the low 16 bits of whatever `EBX` held at the
moment `FUN_0005d6e4` was called — no register is reused or reassigned in between.** This makes the caller's
`BX`-at-call-time the entire question, resolved caller-by-caller in §2.

`FUN_0005d6e4` also reads `param_3[0x96]` (i.e. `+0x12C`) once more, immediately after the write, gating a
per-vehicle byte-copy loop (`if (param_3[0x96] == 1) { …loop using FUN_000b3e84… } else { …straight block
copy… }`, decompile-tier, not independently raw-verified this session — recorded for completeness, not load-
bearing to the writer question).

### 1.3 `FUN_000b44a4`: a rare, narrow-scope zero-writer, disclosed rather than deprioritized silently

`FUN_000b44a4` (VA `0xb44a4`, file `0xa44a3`) is a short (73-byte), single-caller utility:

```c
piVar1 = &DAT_00160fd8;                 // the vehicle array base, confirmed
ram0x001304cc = 0xffffffff;
if (DAT_00244084 == 1) {
    for (i = 0; i < 0x50; i++, piVar1 += 0x276/4) {
        if (*piVar1 >> 0x10 == -1) {    // FIRST slot whose validity word reads "empty/unallocated"
            unique0x1000001a = i;
            *(short*)(piVar1 + 0x4b) = 0;   // == byte offset 0x12C -- the write
            return;
        }
    }
}
```

It finds the **first still-empty** vehicle slot (validity dword high word `== -1`, i.e. not yet spawned into)
and zeroes that slot's `+0x12C`, gated on a separate global `DAT_00244084 == 1`. Sole caller: `FUN_000b1dc0`
(VA `0xb1dc0`, file `0xa1dbf`), itself called once, from `FUN_00010010` — a long sequence of per-turn/per-day
housekeeping calls (org-relation reset x3, several table-clear calls, `FUN_000b32ac` — the already-bound
org-funds/event-type dispatcher from U2(b)), with the `FUN_000b44a4` call specifically gated on
`DAT_00244024 == 2` (`0xb1e09`/`0xa1e08`) — `DAT_00244024` is the same event-type field bound in
[U1-U2-V1-incursion.md §4](U1-U2-V1-incursion.md). Given it only ever touches an **unallocated** slot and is
gated behind a turn/event condition unrelated to any specific vehicle's mission state, this reads as
pre-clearing a field ahead of whatever spawns into that slot next, not a live-vehicle state change. Recorded
in full rather than dropped, per the task's instruction not to silently deprioritize a confirmed writer — but
it is not part of the main story, which is §1.2/§2.

---

## 2. What `FUN_0005d6e4`'s callers put in `BX` — the real question

`FUN_0005d6e4` has exactly 6 callers (`getReferencesTo`, cross-checked against `QueryFunctions.java`'s own
caller list): `FUN_0006da88`, `FUN_0006de64`, `FUN_00070cc0`, `FUN_000631e0` (2 call sites), `FUN_0005d68c`.

### 2.1 `FUN_0006da88` — the primary dimension-gate incursion-UFO spawn: hardcoded `1`, independent of role

`FUN_0006da88` (VA `0x6da88`, file `0x5da87`) is the function `game/state/city/vehiclemission.h:212-215`
already cites for `clampIncursionScatter` and `incursionTypeThreshold` — i.e. this is unambiguously the
function OpenApoc's own incursion-spawn code already maps to. It spawns UFOs at dimension-gate coordinates
(reads `&DAT_001439e2`/`&DAT_001439f6`, the gate table established in
[U1-arrived-flag-and-0x168.md §5.2](U1-arrived-flag-and-0x168.md)) and takes an incoming role argument:

```
0006da98  MOV word ptr [ESP + 0xc],BX      ; file 0x5da97 -- incoming role, saved off BX
...                                          ; (possible 9->10 escalation via a 50% roll, decompile-tier)
0006db98  MOV byte ptr [ESI + 0x166],AL     ; file 0x5db97 -- role committed to +0x166
...
0006dae3  MOV EBX,0x1                       ; file 0x5dae2 -- ***** BX RECLOBBERED WITH LITERAL 1 *****
0006dae8  MOV ECX,ESI
0006daea  SAR EAX,0x10
0006daed  CALL 0x0005d6e4                    ; file 0x5daec
```

**Role and the `+0x12C` input are carried in the same register at different points in the same function, and
by the time `FUN_0005d6e4` is called, role has already been committed elsewhere and `BX` has been overwritten
with a constant.** This is a direct disproof of the "comes from role" hypothesis, not an absence-of-evidence
argument — every incursion role (Attack/Infiltration/Subversion/Overspawn/Escort) that spawns through this
function gets `+0x12C = 1`.

### 2.2 `FUN_0006de64` — a generic vehicle-slot allocator: also hardcoded `1`

`FUN_0006de64` (VA `0x6de64`, file `0x5de63`) scans for an empty slot and calls `FUN_0005d6e4` with
`EBX = 1` (literal, `0x6de98`/`0x5de97`, immediately before the call at `0x6dea6`/`0x5dea5`). Its own
post-spawn defaults, all hardcoded and independent of any incoming argument, read as a generic/non-AI
creation path rather than an incursion-role spawn: `+0x166 = 6` (`0x6debc`/`0x5debb`, a fixed role/order-state
value, not derived from any parameter), `+0x171 = 0xFF` (`0x6dfcd`/`0x5dfcc`, the mission counter set to its
maximum — i.e. this vehicle will not hit the mission-counter-zero transition for a very long time), `+0xCE =
9`. This is disclosed as a plausible reading, not asserted as "player craft" — no string or further evidence
was chased to confirm it — but the field pattern is consistent with creating a vehicle that has no scripted
UFO mission.

### 2.3 `FUN_00070cc0` — a reinforcement spawn called from inside `FUN_0003a910` itself: hardcoded `1`

`FUN_00070cc0` (VA `0x70cc0`, file `0x60cbf`) rolls a probability (`FUN_00071150`) some number of times and,
on success, finds an empty slot and spawns a new vehicle — again with `EBX = 1` (literal, `0x70d24`/`0x60d23`,
immediately before the call at `0x70d32`/`0x60d31`). **Its sole caller is `FUN_0003a910` at VA `0x3af57`
(file `0x2af56`)** — i.e. this reinforcement spawn is triggered from *within* the same mission-dispatch
function this whole investigation is about. It sets `+0x168 = 0` at spawn (`0x70e00`/`0x60dff` — the
already-documented "spawn write #2" from
[U1-U2-V1-incursion.md §2.2](U1-U2-V1-incursion.md)), corroborating that this reinforcement vehicle is the
same struct family, freshly initialized.

### 2.4 `FUN_000631e0` — the one confirmed non-`1` writer: value copied from the org/base-status catalog

`FUN_000631e0` (VA `0x631e0`, file `0x531df`, 2 call sites internally, both reached from `FUN_0006311c`)
scans `DAT_0018276c` (stride `0xE2`) for the first record whose `+0xAA` field `== 8`, then reads **that
record's own `+0xC8` byte** and uses it — not a literal — as the value that ends up in `+0x12C`:

```
00063262  MOV AL,byte ptr [EBX + EAX*0x2 + 0xc8]   ; file 0x53261 -- catalog record's own +0xC8 byte -> AL
...
0006327d  MOVSX EBX,AX                               ; file 0x5327c -- branch A: sign-extend into EBX
00063280  MOV ECX,EDI
00063282  MOV EAX,0xf
00063287  CALL 0x0005d6e4                             ; file 0x53286
...
000634b9  MOVSX EBX,AX                               ; file 0x534b8 -- branch B: same catalog byte, same EBX
000634bc  MOV ECX,EDI
000634be  MOV EAX,0xf
000634c3  CALL 0x0005d6e4                             ; file 0x534c2
```

Both of this function's two call sites into `FUN_0005d6e4` use the identical catalog-derived `BX`; they
differ only in `EDX` (this function's own second parameter) and in what happens to the new vehicle
afterward, not in what feeds `+0x12C`. **This is the same `DAT_0018276c`/`+0xC8` field `FUN_00091f70`
searches on the retarget branch (§3)** — i.e. this spawn path pre-seeds a vehicle's `+0x12C` with a category
value pulled from a *specific* catalog record (the one whose `+0xAA == 8`), rather than a hardcoded sentinel.
`+0xAA`'s own meaning was not chased further this session.

### 2.5 `FUN_0005d68c` — a 17-caller generic "spawn by type" utility: forwards its own caller's `BX`

`FUN_0005d68c` (VA `0x5d68c`, file `0x4d68b`) does not set `BX` itself — it forwards whatever `BX` it
received from *its own* caller straight through (`0x5d6b2 MOVSX EBX,BX`, file `0x4d6b1`, immediately before
`0x5d6bb CALL 0x0005d6e4`, file `0x4d6ba`). It has **17 call sites** across the binary. A bounded backward
sweep (walk up to 30 instructions before each `CALL 0x0005d68c`, stop at the last write to `BX`/`EBX`) found:

| Caller | `BX` source at the call | Confirmed value |
|---|---|---|
| `FUN_00010380` (2 sites: `0x103d3` file `0x3d2`, and `0x1048a` file `0x489`) | `MOV EBX,0x3` | literal `3` |
| `FUN_0007a730` (`0x7a78f` file `0x6a78e`) | `MOV EBX,0x9` | literal `9` |
| `FUN_000ab440` (`0xab4e0` file `0x9b4df`) | `XOR EBX,EBX` | literal `0` |
| `FUN_00015400` (5 sites, `0x15738`/`0x15799`/`0x157cb`/`0x157fc`/`0x1582d`) | `XOR EBX,EBX` (all 5) | literal `0` |
| `FUN_0008f3d4` (`0x8f457` file `0x7f456`) | `XOR EBX,EBX` | literal `0` |
| `FUN_000a1f9c` (`0xa20f6` file `0x920f5`) | `MOV EBX,0x9` | literal `9` |
| `FUN_000a238c` (`0xa24e6` file `0x924e5`) | `MOV EBX,0x9` | literal `9` |
| `FUN_0005ca04`, `FUN_00034860`, `FUN_00092470`, `FUN_00090d68` | value loaded from memory/another register (`[ESP+n]`, `[EBP]`) rather than a literal | **not resolved this session** — would need one more hop into each caller |

Two of the `9`-writers are self-consistent in an important way: **`FUN_000a1f9c` itself reads some other
vehicle's `+0x12C == 9`** (`0xa2082` file `0x92081`, `CMP word ptr [EDI+0x12c],0x9`) before spawning a new one
with `BX = 9` at `0xa20f6`, and **`FUN_000a238c`** does the identical thing (`0xa2472` file `0x92471` read,
`0xa24e6` file `0x924e5` write). Both read-then-propagate `9` — evidence that `9` is a real, stable category
value this pair of functions cooperates on, not incidental. Neither function's broader purpose was traced
this session.

**This branch of the census is not exhaustive.** 4 of 17 callers were not resolved to a concrete value in one
hop; they are disclosed as open rather than guessed at.

---

## 3. `+0x12C`'s consumer: `FUN_00091f70`'s search argument, byte-verified this session

This closes the gap [U1-U2-V1-incursion.md §1.3] explicitly left open ("the specific value passed as that
argument… was not resolved by the decompiler").

### 3.1 The raw guard and both branches of `FUN_0003a910`, re-walked fresh

```
0003acb4  MOV CX,word ptr [EDI + 0x12c]     ; file 0x2acb3 -- +0x12C into CX (entry-guard read, already
                                              ;   documented in U1-U2-V1-incursion.md's decompile as the
                                              ;   "vehicle[+0x12C]==1 || (sVar6==1 && vehicle[+0x12C]!=0)" test)
...
0003ad07  CMP word ptr [EDI + 0x12c],0x1     ; file 0x2ad06 -- fresh re-read, the arrived-flag discriminator
0003ad0f  JNZ 0x0003ad1d                      ; != 1 -> retarget branch
0003ad11  MOV byte ptr [EDI + 0x16a],0x1      ; file 0x2ad10 -- == 1 -> arrived flag (already bound)
0003ad18  JMP 0x0003adb2
0003ad1d  MOV EAX,dword ptr [EDI + 0x12a]      ; file 0x2ad1c -- retarget branch: dword read at +0x12A
0003ad23  SAR EAX,0x10                          ; file 0x2ad22 -- keep HIGH word = bytes +0x12C/+0x12D,
                                                  ;   sign-extended -- i.e. EAX now equals +0x12C itself
0003ad26  CALL 0x00091f70                        ; file 0x2ad25 -- ***** +0x12C IS THE ARGUMENT *****
```

**Confirmed, byte-exact: on the retarget branch, `+0x12C`'s own current value (not role, not `+0x15C`, not
any catalog lookup result) is the argument passed to `FUN_00091f70`.**

### 3.2 What `FUN_00091f70` does with it (already bound, cross-checked this session)

Per [U1-U2-V1-incursion.md §1.3](U1-U2-V1-incursion.md): `FUN_00091f70` counts records in the `DAT_0018276c`
array (stride `0xE2`, count `DAT_00183a68`) whose own `+0xC8` byte equals the argument, then randomly selects
among matches (`FUN_0005d1d8`) honoring an alternate-priority byte at `+0xC9`, returning that record's index
(or `-1`). The result feeds `FUN_0004db84` (nearest-site search) then `FUN_0004e0d4` (commit waypoint) — the
existing, already-implemented `acquireTargetBuilding`/`setPathTo` chain in OpenApoc's
`advanceMissionCounterOnArrival`.

**Forward pointer, so a reader who stops here doesn't draw the wrong conclusion:** this consumer is reached
only via the `+0x12C != 1` (retarget) side of the `0x3ad07` branch. §5.3 shows that, for every vehicle
population this session's writer census covers, that side is never actually reached — so
`acquireTargetBuilding`/`setPathTo` being an accurate structural match for `FUN_00091f70`'s *shape* does not
mean OpenApoc's current always-retarget behavior matches the original's actual, observed behavior for its
modeled incursion population. Read §5.3 before drawing implementation conclusions from this section alone.

### 3.3 `DAT_0018276c` is independently identified elsewhere in this project as a per-org/base status table

[O1-O2-M1-city.md](O1-O2-M1-city.md) traced this **same table** (`DAT_0018276c`, `+0xE2`-stride, `+0xC8`
field) from a completely different starting point (the base-deallocation path) and found it holds an
open/closed-type **status byte per org/base slot** — its own citation: "deallocates the base's slot: writes a
'closed' byte into the same `DAT_0018276c+200`-indexed per-org status table" (`+200` decimal `== 0xC8`). The
two findings corroborate: `+0x12C`, on retarget, is compared against an **org/base status byte**, not a
building "type," not a UFO role, and not `UFO_mission_data`.

**A caution carried over verbatim from the advisor review of this session's own raw scan, disclosed rather
than silently applied:** several of the 55 read-hits in the original 140-hit scan are `MOV reg,dword ptr
[reg+0x12c]` — a **dword** read. Per the "read a dword, keep the high word" idiom already established
throughout this project's other UFO-mission findings, a dword read at `+0x12C` followed by `SAR reg,0x10`
yields the word at **`+0x12E` (constitution)**, not `+0x12C` itself. The dword-read hits in
`FUN_00038a44`, `FUN_0004fca4`, `FUN_000588f8` (`0x58aed`), `FUN_0006da88` (`0x6dbc0`, already documented in
[U1-arrived-flag-and-0x168.md §2.6](U1-arrived-flag-and-0x168.md) as a constitution read feeding the `+0x168`
formula), `FUN_00076370`, `FUN_0008da9c`, `FUN_00092e10` (2 sites), and `FUN_000bd7f8` are **not** `+0x12C`
reads and are excluded from every count in this document. Only genuine **word**-sized accesses at `+0x12C`
are treated as touching this field.

**One more inference disclosed rather than silently relied on:** the `+200 == 0xC8` reconciliation and the
`DAT_0018276c` table-identity claim in §3.3 above rest on quoting `O1-O2-M1-city.md`'s own sentence rather
than re-deriving the table's shape from its initializer code the way
[U1-arrived-flag-and-0x168.md §5](U1-arrived-flag-and-0x168.md) did for the `0x1439e0` dimension-gate table —
a table-identity claim this exact project corrected once already after it turned out to be an unchecked,
inherited assumption. The `+0xE2` stride and `+0xC8` field match exactly between the two documents' *raw*
citations (this session's own `0xE2`/`0xC8` come straight from `FUN_00091f70`'s and `FUN_000631e0`'s bytes,
not from the other doc), so the identity is corroborated by independent raw evidence, not merely asserted
twice — but it was not re-derived from `DAT_0018276c`'s own initializer this session, and that re-derivation
is what would make it fully load-bearing rather than well-corroborated.

---

### 3.4 `+0x12C` is tested at several more points in `FUN_0003a910`, not resolved to the same depth

A bounded check requested during review (`DumpListingRange 0x3a940 0x3aa20` and `0x3b090 0x3b300`, both raw,
this session) found `+0x12C` referenced at **6 more sites inside `FUN_0003a910`** beyond the three walked in
§3.1, spanning at least three more structurally distinct blocks. These are disclosed in full rather than
folded into the "one confirmed consumer" framing §3.1–§3.2 originally used — that framing was too narrow, and
correcting it here matters more than preserving a clean story.

**Block A — a dimension-gate-position check near the top of the function (VA `0x3a958`–`0x3aa02`, file
`0x2a957`–`0x2aa01`), gated on the arrived flag, not the mission counter:**

```
0003a946  MOV AL,[0xd5060]             ; side/turn discriminant
0003a94b  IMUL EAX,EAX,0x2d4            ; side * 0x2D4 -- the dimension-gate table's own per-side stride
0003a951  MOV EBP,0x1439e0               ; the dimension-gate table base (bound in the sibling doc, §5.2)
0003a956  ADD EBP,EAX
0003a958  CMP byte ptr [EDI+0x16a],0x0   ; the arrived flag
0003a95f  JZ 0x3a96b
0003a961  CMP word ptr [EDI+0x12c],0x1   ; file 0x2a960 -- if arrived-flag SET: +0x12C == 1 ?
0003a969  JZ 0x3a986                      ; -> continue into the gate-slot scan below
0003a96b  CMP word ptr [EDI+0x12c],0x0   ; file 0x2a96a -- if arrived-flag CLEAR: +0x12C == 0 ?
0003a973  JNZ 0x3ab2c                      ; anything else -> skip this whole block
0003a979  CMP byte ptr [EDI+0xcf],0x2
0003a980  JNZ 0x3ab2c
0003a986: ...                              ; scans the 10-slot dimension-gate table for a slot whose
                                            ; x/y/z (+0x4/+0x18/+0x2c) match this vehicle's own current
                                            ; position (+0x30/+0x32/+0x34)
0003aa02  MOV CX,word ptr [EDI+0x12c]     ; file 0x2aa01 -- re-read
0003aa09  CMP CX,0x1 / JNZ 0x3aaa3         ; branches again on == 1 vs not, further logic not walked
```

Read literally, tracing every jump precisely (entry into the gate-slot scan itself is gated by the flag/
`+0x12C`/`+0xCF` checks below; the scan can still fail to find a matching slot, `JZ 0x3ab21` inside the loop,
even after entry): the scan at `0x3a986` is reached exactly two ways — (i) arrived-flag **set** and `+0x12C
== 1` (`0x3a961`/`0x3a969` fall straight through), or (ii) `+0x12C == 0` **and** `+0xCF == 2` (`0x3a96b`
onward), and **this second path is reachable regardless of the arrived flag's state** — both the
arrived-flag-clear fallthrough at `0x3a95f` and the arrived-flag-set-but-`+0x12C != 1` fallthrough at
`0x3a969` land on the identical `0x3a96b` check. So `+0x12C == 0` combined with `+0xCF == 2` ("have an active
waypoint," a state also set by `FUN_00059148`'s gate-flight resolution) reaches the scan independent of the
arrived flag, while `+0x12C == 1` only reaches it when the arrived flag is already set. This reads as "detect
that the vehicle has physically reached a gate slot" — consistent with `+0x12C == 1` marking the population
that flies to a gate — but the `+0x12C == 0` case, and any `+0x12C` value that is neither `0` nor `1` with
the flag set, were not resolved further this session, and no name is asserted for any path beyond what the
bytes show.

**Block B — a type-`0xF` self-retirement check (VA `0x3b0bf`–`0x3b0ff`, file `0x2b0be`–`0x2b0fe`), reached
right after one of `FUN_00059148`'s 9 call sites:**

```
0003b0a3  CMP byte ptr [EDI+0xcf],0x2 / JNZ 0x3b111
0003b0ac  CMP word ptr [EDI+0x15c],0x0 / JNZ 0x3b111
0003b0b6  CMP byte ptr [EDI+0xd5],0x0 / JNZ 0x3b111
0003b0bf  CMP word ptr [EDI+0x12c],0x1 / JZ 0x3b111      ; file 0x2b0be -- skip this block if +0x12C == 1
0003b0c9  MOV CX,word ptr [EDI+0xd0]  / CMP CX,[EDI+0x30] / JNZ 0x3b111   ; staged-x == current-x
0003b0d6  MOV SI,word ptr [EDI+0xd2]  / CMP SI,[EDI+0x32] / JNZ 0x3b111   ; staged-y == current-y
0003b0e3  CMP word ptr [EDI+0x4],0xf / JNZ 0x3b104        ; vehicle's own TYPE == 0xF (15) ?
0003b0ea  ...compute this vehicle's own slot index...
0003b0ff  CALL 0x00058280                                  ; file 0x2b0fe -- the already-bound RETIRE function
0003b104  MOV byte ptr [EDI+0x107],0x1
```

When `+0xCF==2`, `+0x15C==0`, `+0xD5==0`, `+0x12C != 1`, and the vehicle has reached its own staged waypoint
(`+0xD0/+0xD2 == +0x30/+0x32`): a vehicle of **type `0xF`** self-retires via `FUN_00058280`. **Type `0xF` is
the exact literal `FUN_000631e0` hardcodes** (`MOV EAX,0xf` at both its call sites into `FUN_0005d6e4`, §2.4)
— the same function that seeds `+0x12C` from the org/base-status catalog rather than a literal `1`. This is
a tight, self-consistent corroboration that `FUN_000631e0`'s spawn population is a distinct, disposable
vehicle kind that expires on arrival at its staged destination, gated specifically on `+0x12C != 1`.

**Block C — a final destination-staging block (VA `0x3b22e`–`0x3b2fe`, file `0x2b22d`–`0x2b2fd`), three more
discriminated cases:**

```
0003b22e  CMP word ptr [EDI+0x12c],0x0   ; file 0x2b22d -- == 0 ?  gates a call to FUN_000b2e18, then
                                          ;   (on success) FUN_000ac08c -- the U2(b) event-type function
0003b292  CMP word ptr [EDI+0x12c],0x1   ; file 0x2b291 -- == 1 ?  gates FUN_0003b724 (OpenApoc's own
                                          ;   computeIncursionSpawnXY) + a second FUN_00059148 call
0003b2db  MOV AX,word ptr [EDI+0x12c]    ; file 0x2b2da -- re-read
0003b2e2  CMP AX,0x1 / JZ (skip)
0003b2e8  TEST AX,AX / JZ (skip)          ; == 0 also skips
0003b2ed  JNZ 0x3b31d                     ; only "some OTHER value" (!= 0, != 1) reaches 0x3b31d
```

This confirms `+0x12C` is discriminated into (at least) three cases — `0`, `1`, and "anything else" — within
this one block alone, consistent with the multi-valued writer census in §2 (`0`, `1`, `3`, `9`, and
catalog-derived bytes are all real).

**What this means for the document's scope.** The task named one specific transition — the
mission-counter-zero branch at `0x3ad07` — and §3.1–§3.3 fully resolve that one, byte-exact, including its
consumer. `+0x12C` turns out to be a much more pervasively-tested field across `FUN_0003a910` than that one
branch suggested; Blocks A–C above are disclosed with byte-exact guards but their downstream logic
(`FUN_000b2e18`, the second `FUN_00059148` call's full effect, what happens at `0x3b31d`) was **not** walked
this session. Fully characterizing all of `+0x12C`'s roles inside `FUN_0003a910` is a larger task than what
was assigned; this section exists so that scope boundary is explicit rather than implied by omission.

---

## 4. What remains open

- **Blocks A–C in §3.4** — three more structurally distinct places `+0x12C` is tested inside `FUN_0003a910`,
  disclosed with byte-exact guards but not walked to their downstream logic (`FUN_000b2e18`'s effect, the
  second `FUN_00059148` call's consequence in Block C, what happens at `0x3b31d`, and Block A's
  arrived-flag-clear/`+0x12C==0` side). This is the largest open item — it means `+0x12C`'s role in
  `FUN_0003a910` is broader than the single mission-counter-zero branch this document otherwise resolves
  completely.
- ~~`+0x15C`'s own value space...~~ **Resolved in §5.2** — the full `DAT_0012d94e` role→`+0x15C` table for
  role indices 0–15 was dumped this session (values raw; the address formula itself is inherited from the
  sibling doc, not re-derived — see §5.2's caveat). What remains genuinely open here: the unnamed role `9`'s
  identity, whatever `FUN_0003a910` logic handles `+0x15C == 5` (Overspawn's value), and independently
  confirming the role *names* (§5.2's labeling caveat) rather than trusting the task brief's numbering.
- **Cross-document reconciliation needed with `U1-arrived-flag-and-0x168.md §2.6`** (flagged in §5.3): that
  document's claim that retargeting recomputes `+0x168` "every time a UFO picks a new mission-destination
  building at runtime" turns out, per this session's guard analysis, to require a combination (`+0x15C == 1`
  and a non-`{0,1}` `+0x12C`) that no writer this session found ever produces. That document's overall
  `+0x168`-reachability verdict does not depend on this and is unaffected, but the specific sub-claim should
  be revisited by whoever next owns that row.
- `FUN_0005d68c`'s 4 unresolved callers (`FUN_0005ca04`, `FUN_00034860`, `FUN_00092470`, `FUN_00090d68`) —
  each needs one more hop to pin down a concrete `BX` value.
- `DAT_0018276c+0xAA`'s own meaning (the field `FUN_000631e0` matches `== 8` against to pick which catalog
  record to copy `+0xC8` from) was not chased.
- The full set of values `+0x12C` can legitimately hold is not enumerated exhaustively — `0`, `1`, `3`, `9`,
  and arbitrary catalog-byte values are all confirmed as real writer outputs; whether other values occur via
  the 4 unresolved `FUN_0005d68c` callers is unknown.
- `FUN_00071150`'s own probability roll (gating `FUN_00070cc0`'s reinforcement spawn, §2.3) and
  `FUN_0006311c`'s role in choosing between `FUN_000631e0`'s two call sites were not examined.
- Whether any *other* game-lifecycle event (save/load, difficulty scaling, a scripted incident) can write
  `+0x12C` through a path this scan's executable-range coverage wouldn't catch — see §1.1's scoped
  "exhaustive over Ghidra-disassembled instructions" caveat; the two known small gaps are disclosed there,
  not assumed away.

---

## 5. Reachability resolved: which roles ever reach `0x3ad07`, byte-exact — and the OpenApoc mappability verdict

**This section originally claimed the `+0x12C == 1` default applied "independent of role," conflating the
proven claim (the *value* `FUN_0005d6e4` writes is role-independent) with an unproven one (that the
`0x3ad07` branch is *reached* for every role). Both the exclusion guard and the table it depends on have now
been raw-verified, and the reachability question is resolved, not just scoped — this is the single most
decision-relevant result in the document, so it is given in full.**

### 5.1 The `{2, 3, 5}` exclusion, byte-exact (closes the gap the previous draft left as decompile-tier)

```
0003ac8f  MOV AX,word ptr [EDI + 0x15c]   ; file 0x2ac8e -- +0x15C loaded into AX
0003ac96  CMP AX,0x2                       ; file 0x2ac95
0003ac9a  JZ 0x0003adb2                     ; == 2 -> bail, entire mission-counter block skipped
0003aca0  CMP AX,0x3                       ; file 0x2ac9f
0003aca4  JZ 0x0003adb2                     ; == 3 -> bail
0003acaa  CMP AX,0x5                       ; file 0x2aca9
0003acae  JZ 0x0003adb2                     ; == 5 -> bail
0003acb4  MOV CX,word ptr [EDI + 0x12c]    ; file 0x2acb3 -- only reached if +0x15C ∉ {2,3,5}
```

`+0x15C == 2`, `3`, or `5` each exit before `+0x12C` is ever read — the mission-counter decrement and the
`0x3ad07` arrived-vs-retarget branch are both skipped entirely for those three values.

### 5.2 `+0x15C` is role-derived, and the role→`+0x15C` table is now dumped, byte-exact

`FUN_0006da88` itself demonstrates the derivation at the exact spawn site §2.1 already anchors — role is
committed to `+0x166`, and the very next call recomputes `+0x15C` from it:

```
0006db98  MOV byte ptr [ESI + 0x166],AL   ; file 0x5db97 -- role committed
0006db9e  MOV EAX,ESI
0006dba0  CALL 0x0005df1c                  ; file 0x5db9f -- writes +0x15C from a role-indexed table
0006dba5  CMP word ptr [ESI + 0x15c],0x4   ; file 0x5dba4 -- read back immediately after
```

`FUN_0005df1c` is already bound in [U1-arrived-flag-and-0x168.md §2.6](U1-arrived-flag-and-0x168.md) as
writing `+0x15C = DAT_0012d94e[+0x166 * 0x94]` — a byte-indexed table, one row (stride `0x94`) per role
value. **That address formula (base, stride, offset-within-row) is inherited from the sibling doc's
decompile-tier citation, not independently re-derived this session** — a fresh decompile of `FUN_0005df1c`
was attempted this session specifically to re-confirm it and came back empty (no output under the header in
`export/u1_12c_rolecheck.log`), so the formula itself is carried over, unconfirmed this session, not
re-verified. What **is** raw and independently verified this session is the byte dump at the resulting
addresses (`Memory.getByte`, same technique the sibling doc already used successfully two bytes away at
`DAT_0012d950`) and the `0x3ac8f`/`0x3ac96` guard instructions in §5.1 that actually consume `+0x15C` — those
are load-bearing and byte-exact regardless of whether `DAT_0012d94e`'s formula is confirmed. The table below
gives every row for role indices `0`–`15`, values raw, formula inherited:

| role (`+0x166`) | `DAT_0012d94e` row value (`+0x15C` after spawn) | excluded by `{2,3,5}`? |
|---|---|---|
| 0 | 0 | no |
| 1 | 0 | no |
| 2 | 0 | no |
| 3 | 0 | no |
| 4 | 0 | no |
| **5 (Attack)** | **1** | **no** |
| 6 (`FUN_0006de64`'s hardcoded default, §2.2) | 0 | no |
| **7 (Infiltration)** | **3** | **YES** |
| **8 (Subversion)** | **2** | **YES** |
| 9 (unnamed — the role `FUN_0006da88` can escalate to 10 on a 50% roll, §2.1) | 4 | no |
| **10 (Overspawn)** | **5** | **YES** |
| **11 (Escort)** | **0** | **no** |
| 12–15 | 0 | no |

**A labeling caveat first, stated plainly:** the role *names* in the table above ("Attack", "Infiltration",
"Subversion", "Overspawn", "Escort") are the task brief's, not independently derived by this session. This
session traced `+0x166`'s numeric values and where they're written/read, but never opened
`FUN_0006d384` — `FUN_0006da88`'s only 2 callers (`0x6d4a7`, `0x6d58a`) — to independently confirm which
integer corresponds to which named mission type. The mapping is assumed from the task brief's own numbering
(`5`/`7`/`8`/`10`/`11`), not re-derived here.

**With that caveat, exactly three of the five named roles — Infiltration, Subversion, Overspawn — map to
precisely the `+0x15C` values the `{2,3,5}` guard excludes.** This is strong corroboration, not proof: the
values line up exactly against [U1-U2-V1-incursion.md §3.2](U1-U2-V1-incursion.md), which independently
describes "`FUN_0003a910`'s role==2 (Subversion) / role==3 (Infiltration) mission-completion branches" for
the `DAT_000e0cc0` clear sites — using the exact `+0x15C` values `2`/`3` this session derived from an
unrelated starting point, byte range, and table. But that doc's own "role==2/3" naming is itself an
interpretation of `+0x15C`-space (the task brief puts Subversion at raw value `8`, so "role==2" there must
already mean something derived, not `+0x166`'s raw value) — so this is two independent derivations landing
on the same numbers, which is meaningfully strong, but it is not the same as either session independently
confirming the English names from a string table or other ground truth. **Attack (role `5`, `+0x15C=1`) and
Escort (role `11`, `+0x15C=0`) are not excluded — they, along with the unnamed role `9` and `FUN_0006de64`'s
role-`6` population, are the ones that actually reach `0x3ad07`.**

### 5.3 Putting it together: the full entry guard shows the retarget branch is structurally unreachable for Escort, and unproduced-by-any-known-writer for everyone else

The entry guard's *full* logic (raw — the complete instructions, elided by `...` in §3.1's excerpt, are in
the `0x3aca0`–`0x3ada0` dump behind §5.1's citations; `CX = +0x12C`, `AX = +0x15C`) is an OR of two disjuncts:

```
0x3acbb  CMP CX,0x1 / JZ 0x3acd4      ; disjunct 1: +0x12C == 1 -> proceed, regardless of +0x15C
0x3acc1  CMP AX,0x1 / JNZ 0x3adb2      ; disjunct 2 requires +0x15C == 1 ...
0x3accb  TEST CX,CX / JZ 0x3adb2       ; ... AND +0x12C != 0
```

**Reaching `0x3ad07` with `+0x12C != 1` (the precondition for ever taking the retarget branch at `0x3ad1d`)
requires disjunct 2 — which requires `+0x15C == 1`.** Per §5.2's table, `+0x15C == 1` corresponds to exactly
one role: `5` (Attack, on the labeling caveat above). This yields two claims of different strength, kept
separate rather than merged:

**Strong claim, split into an assumption-free half and a table-dependent half.** The structural fact is pure
guard logic, independent of the writer census *and* independent of §5.2's role table: **reaching `0x3ad1d`
requires `+0x15C == 1` at the moment the mission counter hits zero — for any vehicle with `+0x15C != 1`, the
retarget branch is unreachable regardless of `+0x12C`'s value or which writer produced it.** This half
survives even if `DAT_0012d94e`'s address formula (§5.2's inherited-and-unconfirmed caveat) turns out to be
wrong. Only the second half — *which named role* has `+0x15C == 0` and is therefore covered by this
guarantee — depends on §5.2's table: on that table, Escort (`+0x15C == 0`) is one such role, so **the
retarget branch is structurally unreachable for Escort**, with the "structurally unreachable" part
assumption-free and the "this is Escort" part resting on the table.

**Weaker, census-dependent: for Attack (`+0x15C == 1`) and everyone else.** For Attack, disjunct 2 is live —
reaching `0x3ad1d` needs a role-5 vehicle whose `+0x12C` is neither `0` nor `1` at the moment the counter
hits zero. `FUN_0006da88` (the spawn function for this role, §2.1) hardcodes `+0x12C = 1` for every role
including 5, and no other confirmed writer touches a role-5 vehicle's `+0x12C` afterward (§1). **So, as far
as this session's writer census extends, the retarget branch is never actually produced for Attack either —
but this claim is only as strong as §1's "exhaustive over Ghidra-disassembled instructions" scope, unlike
Escort's guard-only claim.** For the other non-excluded roles (`9`, `6`), the same reasoning applies:
disjunct 2 requires `+0x15C == 1`, which neither has (`4` and `0` respectively), so they too can only pass via
`+0x12C == 1` — and both of their known spawn writers (`FUN_0006da88` for `9`, `FUN_0006de64` for `6`)
produce exactly that.

**A cross-document consequence, flagged rather than left for the next session to trip over.**
[U1-arrived-flag-and-0x168.md §2.6](U1-arrived-flag-and-0x168.md) builds a finding on `FUN_0005df1c` being
called at `0x3ad93`, *inside* the retarget branch, asserting "every time a UFO picks a new mission-destination
building at runtime, `+0x168` is recomputed." Read together with this session's finding: **that specific
sub-claim's practical reach shrinks to "would apply to a role-5 (Attack) vehicle whose `+0x12C` had somehow
become a nonzero value other than `1` by the time its mission counter hit zero" — a combination no writer this
session found ever produces.** That sibling document's overall §2.7 verdict ("`+0x168` is reachable in
practice") does **not** depend on this sub-claim and is unaffected — it rests independently on `FUN_00057c8c`
(combat damage) and `FUN_0006cb8c` (base repair) making the gate reachable — but the "also recomputed on
retarget" mechanism specifically should be treated as practically unobserved given this session's evidence,
not as a routine, frequently-firing event. Whoever next touches that row should reconcile the two documents.

Infiltration, Subversion, and Overspawn UFOs bypass this entire code region regardless — their mission
completion is handled by the already-documented, structurally separate `+0x15C ∈ {2,3}`-branch logic (and
whatever role-5-numbered — on the task brief's numbering, Overspawn's raw value `10` maps to `+0x15C == 5` —
`FUN_0003a910` logic handles that value, not traced this session) rather than the arrived-flag/retarget
mechanism this document is about.

### 5.4 A related, narrower note on an existing OpenApoc source comment

`game/state/city/vehiclemission.cpp:3128` cites this field as "vehicle +0x104." That offset does not appear
in any of this project's raw citations for this discriminator — every other reference across
`U1-U2-V1-incursion.md`, `U1-arrived-flag-and-0x168.md`, and this document is `+0x12C`. This is not a
re-derivation of what `+0x104` is (the struct plausibly has real, unrelated fields near there — `+0x102`,
`+0x107`, and `+0x10a` all appear in this session's own spawn-function dumps), only a note that the two
citations should be reconciled by whoever maintains that comment.

### 5.5 OpenApoc mappability verdict

**What this session can assert, scoped precisely, in two tiers of confidence:**

- **Escort** (`+0x15C == 0`): the retarget branch is **structurally unreachable**, proven by the guard's own
  logic alone (§5.3), independent of the writer census. Every mission-counter-zero transition an
  Escort-role UFO experiences goes to "arrived → nearest dimension gate."
- **Attack** (`+0x15C == 1`): the retarget branch is **never produced by any writer this session found**,
  which is a weaker, census-dependent claim (§5.3) but still means: as far as this session's evidence
  extends, Attack-role UFOs also always go to "arrived → nearest dimension gate" at mission-counter-zero.

Both roles spawn via `FUN_0006da88`, the function OpenApoc's own `vehiclemission.h:212-215` already cites, so
this is directly actionable for OpenApoc's currently-modeled incursion-spawn population: no per-vehicle
lookup is needed to reproduce the "always arrived, never retarget" behavior for Attack/Escort — it follows
from `+0x12C`'s spawn-time constant, the guard's logic, and (for Attack) the absence of any other writer.
OpenApoc already has the target mechanism implemented (`City::portals` /
`VehicleMission::MissionType::GotoPortal`). For Infiltration, Subversion, and Overspawn, this document's
subject mechanism does not govern their mission-counter-zero handling at all in the original — those roles
use a different, already-documented code region (§5.3) — so `advanceMissionCounterOnArrival`'s current
always-retarget behavior is neither confirmed nor refuted for them by this document.

**What this session cannot assert:** the alternate, non-`1` writers (`FUN_000631e0`'s org/base-catalog copy,
producing `FUN_0003a910` Block B's type-`0xF` disposable-vehicle population, §3.4; and the `0`/`3`/`9`
literals found among `FUN_0005d68c`'s callers, whose own role/`+0x15C` fields were not examined this session)
do not correspond to any OpenApoc concept found this session — no equivalent of the `DAT_0018276c`
per-org/base status table, and no equivalent of `FUN_000631e0`'s "spawn tied to a specific `+0xAA==8`
catalog record" path, exist in OpenApoc's vehicle-spawn code as far as this session's grep of `game/state/`
found. **This part of the field is not mappable on present evidence and should not be guessed at.**

---

## Coordinator note — not implemented, and why

**This finding is recorded, not acted on.** It would reverse behaviour that shipped and is locked by
three tests in `tests/test_city_rules.cpp` (`VehicleMission::advanceMissionCounterOnArrival`
unconditionally retargets). Reversing a tested, shipped branch needs a higher bar than binding a new
one, and two things are in the way.

**1. The document contradicts a sibling, and the author flagged it.** §5.3 and §11 raise a tension
with [U1-arrived-flag-and-0x168.md §2.6](U1-arrived-flag-and-0x168.md), which builds a finding on
`FUN_0005df1c` recomputing `+0x168` **on the retarget path** — a path this document argues is never
taken by the population that reaches the mechanism. Both cannot be right. That reconciliation is the
blocking step, not an optional tidy-up.

**2. The strongest claim is partly an absence-of-evidence claim.** §2.1's disproof of "the value comes
from role" is direct and positive: `FUN_0006da88` commits role to `+0x166`, then reclobbers `BX` with
a literal `1` before calling the writer. But "Attack never reaches retarget" rests on *no writer
producing a non-1 value having been found*, with **4 of 17 `FUN_0005d68c` callers unexamined** by the
author's own account. `FUN_0005d6e4` writes `+0x12C` from the caller's `EBX`, so a single unexamined
caller passing something other than `1` collapses the conclusion.

**3. A coordinator spot-check of the raw bytes was attempted and is INVALID — do not repeat it.**
Reading `UFO2P.EXE` at a citation's stated file offset and comparing instruction encodings does not
work, and the failure is silent rather than obvious. Checked `MOV EBX,0x1` at file `0x5dae2` (§2.1):
no match. Then checked a **control** — the arrived-flag write `MOV byte ptr [EDI+0x16a],1` at file
`0x2ad10`, a citation from a different agent, independently reviewed and already committed: **also no
match**, while the encoding `C6 87 6A 01 00 00 01` occurs at `0x9D3B5` and four other places.

The control failing is the whole point. These are **bound Linear Executables**, imported through the
community LX loader (lab `README.md`). Object pages are not laid out contiguously in the file, so
`VA − 0x10001` is a within-page convention, not a file-wide linear map — naive arithmetic lands in
unrelated bytes and produces a confident-looking "no match" for a perfectly good citation. Anyone
wanting to arbitrate this must do it **inside the Ghidra project**, against the loaded image, not with
a hex reader and subtraction.

**Status: BOUND, reconciliation pending, no code written.** Same discipline as U2(b): a claim about
the original's control flow is not by itself a licence to change this engine — and here it is not yet
even a settled claim about the original.
