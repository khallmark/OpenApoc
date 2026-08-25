# U1 — `FUN_00092470`'s scheduled population, and whether it is `OrganisationRaid::UnauthorizedVehicle`

**Scope note on citations.** Binary: `UFO2P.EXE` (canonical, CRC32 `0x4749ffc1`), `.object1`. VA is the
Ghidra listing address; file offset is `.object1`'s `MemoryBlockSourceInfo` file-bytes offset
(`VA − 0x10001`), taken directly from `DumpListingRange.java`'s own `file=` field for every instruction
quoted below — never computed by reading the raw `.EXE` at that offset with a hex tool (per this task's
method warning: object pages are not contiguous in the file, and that method previously produced a
confident false negative even on an already-committed, correct citation). All work this session was done
against the loaded Ghidra image: `DumpListingRange.java` raw-listing dumps over the full, uninterrupted
range of `FUN_00092470` (`0x92470`–`0x92e0e`, confirmed as the function's true end — `FUN_00092e10` begins
immediately after) and `FUN_00092060`, `QueryFunctions.java` decompile+listing+xref dumps for
`FUN_00092470`, `FUN_00092060`, `FUN_0005d68c`, `FUN_00091f70`, and `QueryJumpTable.java` against
`FUN_00092470`'s own case-dispatch table. Decompile is used only for orientation and is explicitly flagged
wherever a claim rests on it instead of the raw listing; one such case (§6) was caught disagreeing with the
raw bytes and dropped rather than cited.

---

## Verdict, up front

**Same population, for the purposes that decide the branch this task is about. `FUN_00092470`'s
vehicle-spawning case behaviorally corresponds to `OrganisationRaid::Type::UnauthorizedVehicle` — same
excluded-owner set (never Aliens, never X-COM), same "attack a building" mission shape — and OpenApoc's
current code at `organisation.cpp:1347` needs no change.** (The two sides source the vehicle differently —
the original spawns a fresh one against an abstract inventory counter, OpenApoc dispatches a pre-existing
one out of the org's current fleet, §5.1 — but that difference does not affect who owns the vehicle or which
mission-counter branch it reaches, which is the only thing `advanceMissionCounterOnArrival`'s owner gate
cares about.)

The prior session's hypothesis check failed because it looked for the ownership signal in the wrong place —
the vehicle-*type* classification byte (`0x1285ea + type×0x7e`), which indeed doesn't discriminate alien
from mundane types. This session traced `FUN_00092470`'s actual vehicle-spawning case (`FUN_0005d68c` →
`FUN_0005d6e4`, the same `+0x12C` writer chain `U1-ordertype-0x12c.md` already bound) back to its argument
source and found the real signal, provable without relying on any single small-constant coincidence:

- **The scheduled record's `slot[0]` field is provably an organisation-table index.** `FUN_00092060`
  populates it directly from its own outer loop variable `SI` (`0x9240a`, §4), and `SI` is independently
  shown to index the same 27-entry, `0x1b6`-stride organisation table `O1-O2-M1-city.md` already bound
  (`DAT_0017fb4c`, read inside the very same loop body, §4) — not an unrelated counter reused by coincidence.
  The identical field is *also* used, in `FUN_00092470`, to index and deplete a second, `0x44`-stride
  per-organisation table (`DAT_00170ec0`, §2–§3) — an org-shaped table used two different ways by two
  different functions, both consistent only if `slot[0]` is genuinely an org index.
- **`slot[0]` is provably what lands in `+0x12C`.** Raw, instruction-by-instruction: `0x92a87 MOVSX
  EBX,word ptr[EBP]` (= `slot[0]`) → `0x92a8b CALL 0x0005d68c` → `FUN_0005d68c` forwards its caller's `BX`
  unchanged to `FUN_0005d6e4` (`U1-ordertype-0x12c.md` §2.5, re-confirmed this session) → `0x5dbdc MOV word
  ptr[EBP+0x12c],AX` (the writer that document already bound byte-exact). No value is substituted or
  recomputed anywhere in that chain.
- **That org index is raw-provably excluded from ever being `0` or `1`** by a byte-exact `CMP`/`JZ` pair in
  `FUN_00092060` (§4) — upgrading a claim the prior session left at decompile-tier.

Two organisations are structurally excluded, by index, from ever owning this population's vehicles: index
`0` and index `1`. **Consistent with, and independently corroborated by** (not the load-bearing proof
itself): `+0x12C == 1` is the already-proven invariant for alien-incursion UFOs (`FUN_0006da88` hardcodes
`EBX = 1` before the identical write, `U1-ordertype-0x12c.md` §2.1), and
`tools/extractors/extract_organisations.cpp:11` independently defines `#define ORG_ALIENS 1` for the same
org-table index space. That single small-constant match, on its own, would not be strong evidence — `1` is
a common literal, and `FUN_0006da88`'s hardcode is equally consistent with a boolean flag. What actually
carries the argument is the org-table cross-reference chain above, plus the exact four-field correspondence
between what `FUN_00092060` writes and what `FUN_00092470` reads (§4.1) — independent of any specific
numeric value.

That excluded set — never index `0`, never index `1` — is exactly the set OpenApoc's own extractor marks
`initiatesDiplomacy = true` (`extract_organisations.cpp:151-153`, excluding `ORG_XCOM`/`ORG_ALIENS`/
`ORG_CIVILIAN`), which is exactly the gate OpenApoc already puts around calling `setRaidMissions`
(`gamestate.cpp:1508-1512`) — the function an **already-existing in-repo comment**
(`organisation.cpp:389-390`) independently identifies as `FUN_00092060`'s own reimplementation. This session
did not need to invent that binding; it was already there, unresolved only one level further down the call
chain (§5).

**What should change at `organisation.cpp:1347`: nothing.** `VehicleMission::advanceMissionCounterOnArrival`
(`vehiclemission.cpp:3137-3187`) already implements exactly the right owner-gated shape — `v.owner ==
state.getAliens()` → `gotoPortal`, otherwise fall through to the retarget search
(`acquireTargetBuilding()`) — with a comment explicitly marking the `OrganisationRaid::UnauthorizedVehicle`
case as unresolved and "deliberately left alone rather than guessed at." This session resolves it:
`RaidMission::execute`'s `owner` parameter is always `currentOrg`, the organisation whose `updateMissions()`
is running (`organisation.cpp:432-437`), and that call is only reached when `initiatesDiplomacy == true`
(`gamestate.cpp:1508-1512`) — which is never true for Aliens or X-COM. So `v.owner == state.getAliens()` is
**structurally always false** for a vehicle sent by `OrganisationRaid::Type::UnauthorizedVehicle`, and the
existing fallthrough to `acquireTargetBuilding()` is exactly the behavior the original binary has for this
population. The row closes as "already correct, now confirmed" rather than "needs a code change."

---

## 1. `FUN_00092470` is a per-slot dispatcher, not a per-org one

Decompile (orientation only, cross-checked against the raw listing throughout this document) shows
`FUN_00092470` looping over **10 fixed slots** of `DAT_0013e280`, 8 bytes (4 shorts) each:

```c
psVar8 = (short *)&DAT_0013e280;
...
LAB_00092499:                     // per-slot top
  ... process psVar8[0..3] ...
  psVar8 = psVar8 + 4;            // next 8-byte slot
  if (9 < (short)(counter+1)) return;
  goto LAB_00092499;
```

raw-confirmed for the per-slot countdown and dispatch:

```
00092499  MOV BX,word ptr [EBP + 0x6]     ; file 0x82498 -- slot[3] = countdown
0009249d  TEST BX,BX / JZ 0x92de5          ; file 0x8249c -- skip if slot unused
000924a6  MOV ECX,EBX
000924a8  DEC ECX                          ; file 0x824a7 -- decrement countdown
000924a9  MOV SI,word ptr [EBP + 0x2]      ; file 0x824a8 -- slot[1] = case value
000924ad  MOV word ptr [EBP + 0x6],CX      ; file 0x824ac -- write decremented countdown back
000924b1  TEST SI,SI / JZ 0x92de5          ; file 0x824b0 -- skip if no case pending
000924ba  TEST CX,CX / JNZ 0x92de5         ; file 0x824b9 -- fire only when countdown just hit 0
000924c3  MOV EAX,ESI
000924c5  CMP SI,0xa / JA 0x92ddf          ; file 0x824c4 -- case value bounded 0..10
000924d4  JMP dword ptr CS:[EAX*4+0x92444] ; file 0x824d3 -- 11-entry dispatch
```

`QueryJumpTable.java 0x92444 11` against the loaded image resolves every entry:

| case (slot[1]) | target | behavior |
|---|---|---|
| 0, 4, 5, 7, 10 | `0x92ddf` | shared no-op / skip |
| 1 | `0x924dc` | message + relation/funds delta via `FUN_0005faf0`, **no vehicle spawn** |
| 2 | `0x92667` | message + relation/funds delta via `FUN_0005faf0`, **no vehicle spawn** |
| 3 | `0x9283e` | message + relation/funds delta via `FUN_0005faf0`, **no vehicle spawn** |
| **6** | **`0x92a15`** | **the only case that calls `FUN_0005d68c` — spawns a vehicle** |
| 8 | `0x92bfb` | table-driven comparison + optional `FUN_0005faf0` delta, no spawn |
| 9 | `0x92c8c` | message + `FUN_0005faf0` delta, no spawn |

Only case `6` (`slot[1] == 6`) reaches a vehicle spawn. Cases `1`/`2`/`3`/`9` are message-plus-relation-delta
outcomes with no vehicle movement at all — structurally the same shape as OpenApoc's own
`OrganisationRaid::Type::Attack`/`Raid`/`Storm` (which likewise only adjust relations and push a
`GameBuildingEvent`, no vehicle involved — `organisation.cpp:1234-1288`). This document does not claim the
binary's numeric case values are bijective with OpenApoc's `enum class Type` values (`None=0, Attack=1,
Raid=2, Storm=3, UnauthorizedVehicle=4, Treaty=5`, `organisationraid.h:13-21`) — only that the *behavioral*
shape matches: several relation-only outcomes, and exactly one vehicle-dispatching outcome.

---

## 2. Case 6: a genuine, bounded double loop, and its true exit condition

Case 6 is not a single attempt — it is a nested retry loop, confirmed raw end-to-end:

```
00092a15  MOV word ptr [ESP+0x114],CX      ; file 0x82a14 -- outer counter init (0)
00092a1d  MOV dword ptr [ESP+0x118],ECX    ; file 0x82a1c -- success counter init
LAB 00092a24:
00092a24  XOR EDI,EDI
00092a26  MOV word ptr [ESP+0x110],DI      ; file 0x82a25 -- inner counter reset (0)
LAB 00092a2e:
00092a2e  MOV ECX,dword ptr [ESP+0x10e]    ; file 0x82a2d -- ***type candidate*** (bytes straddle
                                            ;   the inner counter's own word; see below)
00092a35  SAR ECX,0x10                     ; file 0x82a34
00092a38  IMUL EDX,ECX,0x7e                ; file 0x82a37
00092a3b  CMP word ptr [EDX+0x1285ea],0x1  ; file 0x82a3a -- class-byte gate
00092a43  JNZ 0x92b6b                       ; file 0x82a42 -- fail -> next type candidate
...
00092b6b  MOV ECX,dword ptr [ESP+0x110]    ; file 0x82b6a
00092b72  INC ECX                          ; file 0x82b71
00092b73  MOV word ptr [ESP+0x110],CX      ; file 0x82b72 -- inner counter++
00092b7b  CMP CX,0x22 / JL 0x92a2e          ; file 0x82b7a -- loop while < 34
...
00092b85  MOV EDX,dword ptr [ESP+0x114]    ; file 0x82b84
00092b8c  INC EDX                          ; file 0x82b8b
00092b8d  MOV word ptr [ESP+0x114],DX      ; file 0x82b8c -- outer counter++
00092b95  CMP DX,0x2 / JL 0x92a24           ; file 0x82b94 -- loop while < 2
```

**Outer loop: 2 attempts. Inner loop: 34 type candidates (`0`–`33`) per attempt.** The type-candidate
register is read as `dword ptr [ESP+0x10e]` then `SAR 0x10` — this is not a separately-written local; its
low word (`ESP+0x10e..0x10f`) is the never-reinitialized-for-this-case upper half of an unrelated case-3
local, but its **high word (`ESP+0x110..0x111`) is exactly the inner loop counter itself**, freshly zeroed
at `0x92a26` and incremented at `0x92b73` — so `SAR ECX,0x10` on this dword deterministically extracts the
inner loop counter as the type candidate. (Raw-confirmed the loop-back targets and bounds byte-exact, per
above; the aliasing mechanism is inferred from the confirmed byte ranges, not independently re-verified
against a second addressing convention this session — flagged rather than asserted as certain in isolation,
though the loop-count-matches-inventory-table-width coincidence in §3 corroborates it strongly.)

For a given type candidate, case 6 requires, in order (all raw-confirmed, file offsets as quoted):

1. **Class-byte gate**: `word[EDX + 0x1285ea] == 1` (`0x92a3b`/file `0x82a3a`) — the same per-type catalog
   byte the prior session already dumped for indices `0`–`24` (`QueryTypeClass1285ea.java`), uniformly `1`
   across both alien (`0`–`9`) and mundane (`10`–`24`) type indices.
2. **Org inventory gate** (new this session): `word[org×0x44 + type×2 + 0x170ec0] > 1` —
   ```
   00092a49  MOVSX EAX,word ptr [EBP]        ; file 0x82a48 -- slot[0]
   00092a4d  IMUL EBX,EAX,0x44                ; file 0x82a4c
   00092a50  LEA ESI,[ECX*2+0]                ; file 0x82a4f -- ECX = type candidate
   00092a57  CMP word ptr [ESI+EBX+0x170ec0],0x1  ; file 0x82a56
   00092a60  JBE 0x92b6b                       ; file 0x82a5f -- fail -> next candidate
   ```
3. **Authorization flag**: `byte[type×0x3f + 0x12862a] != 0` (`0x92a66`/file `0x82a65`).
4. **Target search succeeds**: `FUN_00091f70(EAX = slot[0]) != -1` (`0x92a74`/file `0x82a73` — argument
   register confirmed via `FUN_00091f70`'s own decompiled signature, `undefined8 __regparm3
   FUN_00091f70(ushort param_1, undefined4 param_2)`, first `__regparm3` arg in `EAX`; `EAX` is unclobbered
   between the `0x92a49` load and this call — traced instruction-by-instruction).
5. **Slot allocation succeeds**: `FUN_0005d68c(EAX = type, EBX = slot[0]) >= 0` (`0x92a85`-`0x92a8b`/file
   `0x82a84`-`0x82a8a`).

On success, the org's own inventory entry is **decremented** —
`DEC word ptr [EDX+ESI*1+0x170ec0]` (`0x92aa9`/file `0x82aa8`) — and both loops are abandoned (jump straight
to the outer-loop tail at `0x92b85`, bypassing the inner-loop retry at `0x92b6b` entirely).

**The inner loop's bound (34 = `0x22`) exactly matches the org-inventory table's own row width**
(`0x44` bytes ÷ `2` bytes/type = `34` type slots) — the type-search loop tries every slot the inventory table
has room for, no more, no less. This is strong internal corroboration that `DAT_00170ec0` genuinely is a
per-organisation, per-type inventory/stock table, not a coincidental byte range.

---

## 3. `+0x12C`'s source is an organisation index, not a role or a scheduler tick

`slot[0]` (`word ptr [EBP]`) is used **twice** in case 6, for two different things that only make sense if
it is the same organisation index both times:

```
00092a49  MOVSX EAX,word ptr [EBP]      ; file 0x82a48 -- slot[0] indexes the ORG inventory table (§2.2)
...
00092a87  MOVSX EBX,word ptr [EBP]      ; file 0x82a86 -- slot[0] forwarded as +0x12C's source
00092a8b  CALL 0x0005d68c               ; file 0x82a8a
```

`FUN_0005d68c(EAX=type)` forwards its caller's `BX` straight to `FUN_0005d6e4`
(`0x5d6b2 MOVSX EBX,BX`, already established in `U1-ordertype-0x12c.md` §2.5 and re-confirmed this session
via `FUN_0005d68c`'s own decompile, `int __regparm3 FUN_0005d68c(undefined2 param_1)`, entry
`MOV word ptr[ESP],AX` — param_1/type in `EAX`, consistent with the `+0x12C` writer chain already bound
byte-exact by that document), which writes it to `+0x12C`. **The value that ends up at `+0x12C` for this
population is literally the same organisation index that was just used to check and deplete that
organisation's own vehicle-type stock.**

This is not a new "±0x12C is org id" claim invented to close this row — it is directly, independently
cross-checked against the one hard, unambiguous constant this project already has for this field:

- `U1-ordertype-0x12c.md` §2.1 (raw, byte-exact): `FUN_0006da88` — the alien-incursion spawn function —
  hardcodes `EBX = 1` (`0x6dae3 MOV EBX,0x1`, file `0x5dae2`) immediately before the identical
  `FUN_0005d6e4` write, independent of role.
- `tools/extractors/extract_organisations.cpp:10-11`: `#define ORG_XCOM 0` / `#define ORG_ALIENS 1` —
  authored independently of this investigation, for an unrelated purpose (org-table indexing in the data
  extractor).

`+0x12C == 1` for aliens and `ORG_ALIENS == 1` in OpenApoc's own org-table indexing are the same fact
expressed on two sides of a byte-faithful port. `FUN_00092470`'s `+0x12C` source (`slot[0]`) being drawn
from that same index space, and independently gated away from `0`/`1` (§4), is the discriminating signal
the prior session's class-byte check was never going to find — the class byte gates vehicle *type*
eligibility, not vehicle *ownership*; ownership was never encoded there.

A second organisation-shaped field is also written onto the new vehicle from this same slot record,
disclosed but not chased further:

```
00092b0b  MOV AX,word ptr [EBP+0x4]      ; file 0x82b0a -- slot[2]
00092b16  MOV word ptr [EDX+0x15a],AX    ; file 0x82b15 -- new vehicle's +0x15a = slot[2]
```

Cases `1`/`2`/`3` (the relation-message-only outcomes) separately use `slot[2]` to index the
already-established `DAT_0018276c` org/base-status table (`(*(int*)(psVar8+1)>>0x10) * 0xe2`, decompile-tier
for these non-spawning cases, not independently raw-re-verified this session since they are not load-bearing
to the verdict). `+0x15a`'s exact role (target org? a diplomatic counterpart reference?) is **not resolved**
by this session and is not asserted — reported per the ground rule against inventing a field's meaning.

---

## 4. `FUN_00092060`'s org-index exclusion, raw-verified (upgrading a prior decompile-tier claim)

`U1-retarget-reconciliation.md` §5 flagged this exact guard as decompile-only ("the specific `CMP`/`JZ`
pair excluding `0` and `1` was not independently re-disassembled this session"). This session dumped the
full raw range `0x92060`–`0x92470` and found it directly, byte-exact, at the very top of the outer org loop:

```
00092069  XOR ESI,ESI                    ; file 0x82068 -- SI = 0
0009206b  TEST SI,SI                     ; file 0x8206a
0009206e  JZ 0x0009242e                  ; file 0x8206d -- skip org index 0 (X-COM)
00092074  CMP SI,0x1                     ; file 0x82073
00092078  JZ 0x0009242e                  ; file 0x82077 -- skip org index 1 (Aliens)
```

`SI` is confirmed as an index into the same 27-entry, `0x1b6`-stride org table `O1-O2-M1-city.md` already
bound (`DAT_0017fb4c`, read at `0x922fb`/file `0x822fa` inside this same loop body via
`IMUL EDX,ESI,0x1b6; ADD EBP,DAT_0017fb4c`) — i.e., this is unambiguously the organisation table, not an
unrelated counter. The loop runs `SI = 0..26` (`CMP SI,0x1b / JL 0x9206b`, `0x9242f`-`0x92433`/file
`0x8242e`-`0x82432`), and the value ultimately written into the scheduled slot's field 0 — the same field
case 6 later reads as `+0x12C`'s source — is `SI` itself:

```
00092405  MOV EAX,0xf0                    ; file 0x82404
00092408  ...
0009240a  MOV word ptr [EBX],SI           ; file 0x82409 -- slot[0] = SI (org index, never 0 or 1)
```

**This closes the gap the prior document left open.** `FUN_00092470`'s spawn population's `+0x12C` is drawn
from an organisation-table index that is structurally, byte-provably never X-COM and never Aliens.

### 4.1 The full record: all four slot fields, writer and reader, matched byte-for-byte

`U1-retarget-reconciliation.md` established that `FUN_00092060` is `DAT_0013e280`'s sole populator via
`getReferencesTo`, but that only proves the *function* touches the table, not that a *specific write* feeds
a *specific read* `FUN_00092470` later performs. This session traced all four 2-byte fields of the 8-byte
slot record on both ends and they match exactly, including a shared literal:

| Slot field | `FUN_00092060` writes it | `FUN_00092470` reads it as |
|---|---|---|
| `[EBX+0]` (`slot[0]`) | `0x9240a MOV word ptr[EBX],SI` (file `0x82409`) — the outer org-loop index, raw-excluded from `{0,1}` (above) | `0x92a49`/`0x92a87` — org-inventory index and `+0x12C` source (§2–§3) |
| `[EBX+2]` (`slot[1]`) | `0x92392 MOV word ptr[EBX+2],0x6` (file `0x82391`) — a literal `6`, one of several case-outcome literals this function writes (`0x1`/`0x2`/`0x3`/`0x6`/`0x7`, `0x92359`-`0x923ad`) | `0x924a9 MOV SI,word ptr[EBP+2]` → dispatch value — **`6` is the exact case value that reaches the vehicle-spawning path** (§1's dispatch table) |
| `[EBX+4]` (`slot[2]`) | `0x92398 MOV word ptr[EBX+4],DI` (file `0x82397`), where `DI` was set at `0x9219f MOV EDI,EAX` immediately after `0x92197 CALL 0x00091f70` with `EAX = MOVSX(CX)` (the *inner*-loop "candidate" org, `0x92192`-`0x92197`) — i.e. `slot[2]` = a `DAT_0018276c` record index matched against the *candidate* org, resolved once at schedule time | `0x92b0b`/`0x92b16` — copied verbatim onto the new vehicle's `+0x15a` field (§3) |
| `[EBX+6]` (`slot[3]`) | `0x92412 CALL FUN_0005d1d8; ADD EAX,0xa; 0x92417 MOV word ptr[EBX+6],AX` (file `0x82411`-`0x82416`) — a random `0..N` delay plus a `10`-tick floor | `0x92499`/`0x924ad` — the countdown, decremented every invocation, dispatch fires only when it reaches exactly `0` (§1) |

Four fields, four matching roles end to end, with the literal `6` written on one side and read as the
spawn-dispatch selector on the other, and a random-plus-floor delay written on one side and decremented to
zero on the other. This is a self-contained proof from bytes dumped this session, not an inherited
assumption. (Not independently re-verified this session: the pointer arithmetic that turns `FUN_00091e68`'s
return value into `EBX`, i.e. that `EBX` in `FUN_00092060` is genuinely `&DAT_0013e280 + slotIndex×8` and
not some other structure `FUN_00092060` happens to share a base register naming with — this was established
by the prior session via `getReferencesTo(0x13e280)` and not re-traced instruction-by-instruction here;
disclosed rather than silently assumed.)

`slot[2]`'s trace is also the first concrete lead on what `FUN_00091f70`'s `+0xC8`-matched `DAT_0018276c`
record represents for this population — see §7's discussion of the open `+0xC8` question.

---

## 5. The OpenApoc-side counterpart was already identified — one level up

An in-repo comment, already present before this session, independently ties `FUN_00092060` to
`Organisation::setRaidMissions`'s own manpower formula:

```cpp
// game/state/shared/organisation.cpp:389-390
// FUN_00092060 @ file 0xE4704: (workforce/100)*avg², or
// (raiding_strength/100)*avg² when FUN_00091f70 returns -1 twice.
```

(also documented in `docs/original-game/openapoc-gap-matrix.md:15` and
`docs/original-game/extractor-tables.md:11`, both marked "implemented," confidence "high"). This session's
contribution is the piece downstream of that: **`setRaidMissions` is only ever called for organisations
where `initiatesDiplomacy == true`**:

```cpp
// game/state/gamestate.cpp:1508-1512
if (o.second->initiatesDiplomacy)
{
    // Must run before updateRelations overwrites long_term with current.
    o.second->setRaidMissions(*this, current_city);
}
```

and `initiatesDiplomacy` is set exactly where the binary's own exclusion lives:

```cpp
// tools/extractors/extract_organisations.cpp:151-153
if (i != ORG_CIVILIAN && i != ORG_XCOM && i != ORG_ALIENS)
{
    // Everyone except Player, Aliens and Civilians can initiate diplomacy/raids
    o->initiatesDiplomacy = true;
```

And the vehicle actually dispatched, in `OrganisationRaid::Type::UnauthorizedVehicle`
(`organisation.cpp:1290-1354`), is drawn from `owner->buildings`'s own `currentVehicles` — i.e. it is
**already owned by `owner`** before the mission ever starts (`organisation.cpp:1347`,
`v->setMission(state, VehicleMission::attackBuilding(state, *v, target))`), and `owner` is always
`currentOrg` from `Organisation::updateMissions` (`organisation.cpp:432-437`), the same
`initiatesDiplomacy`-gated organisation. So on the OpenApoc side, exactly as on the binary side: the vehicle
sent by `UnauthorizedVehicle` can never be Aliens- or X-COM-owned.

### 5.1 A real mechanical difference, stated plainly rather than smoothed over

The two sides do not implement this the same way, and the difference is worth naming rather than filing
under "structural parallel": **the original *spawns a brand-new vehicle*** — `FUN_0005d68c` scans the
80-entry vehicle array (`DAT_00160fd8`) for the first empty slot and initializes it from scratch
(`U1-ordertype-0x12c.md` §1.2) — **and decrements an abstract per-type inventory *counter*** (`DAT_00170ec0`,
§2–§3), which is not tied to any specific already-existing vehicle object. **OpenApoc's
`UnauthorizedVehicle` instead dispatches a *pre-existing* vehicle**, scanned out of `owner->buildings`'s own
`currentVehicles` (`organisation.cpp:1290-1324`), and — after the attack — sends it back home via
`VehicleMission::gotoBuilding(v->currentBuilding)` (`organisation.cpp:1351`). The original has no
"return home" step visible in `FUN_00092470`'s own spawn tail; the new vehicle is simply created already in
flight, with its mission counter and waypoint set directly (§2–§3).

This is a genuine sourcing difference, not just an implementation detail: the original's mechanic reads as
"organisation X's *available combat capacity* permits sending one more raider," realized as a fresh unit;
OpenApoc's reads as "organisation X's *existing fleet* has an idle unit to spare," realized by pulling one
out of the org's current roster. Both converge on the same decision this document actually needs to answer
— *is the raiding organisation ever Aliens or X-COM, and does it end up on an `attackBuilding` mission that
reaches `advanceMissionCounterOnArrival`* — and on that question they agree exactly (§3–§5, non-alien,
non-X-COM, `AttackBuilding`-mission population, both sides). The verdict rests on that agreement, not on the
two sourcing mechanisms being identical, which they are not.

Two more parallels, neither load-bearing to the verdict but both consistent with it:

- The per-type authorization/class gates (§2, points 1 and 3) parallel OpenApoc's
  `state.organisation_raid_rules.attack_vehicle_types` allow-list (`organisation.cpp:1332`) — a restricted
  set of raid-capable craft, gated separately from raw ownership.
- `FUN_00091f70` and `FUN_0004e0d4` — the same two functions `FUN_0003a910`'s retarget branch calls
  (`U1-U2-V1-incursion.md` §1.3) — are called again directly inside case 6's own spawn tail (`0x92a74`,
  `0x92b5d`/file `0x82a73`, `0x82b5c`). This population's target-acquisition machinery is not merely
  *reachable* via the retarget branch by coincidence; it is the *same* machinery this population's own spawn
  path already uses once.

One check attempted and explicitly **not** claimed: whether `DAT_00170ec0` (§2/§3) is byte-identical to the
`vehicle_park`/`OrgVehicleParkData` chunk the extractor reads (`organisations.h:52-58`,
`ORGANISATION_VEHICLE_PARK_DATA_OFFSET_START = 0x188ea8`). It is not — that chunk is `112` bytes total (a
single `uint32` per org, feeding `parkBudgetWeight`), structurally incompatible with `DAT_00170ec0`'s
per-type, `0x44`-byte-stride shape. These are two different original data tables that happen to serve a
similar economic role; this document does not claim they are the same blob.

---

## 6. A decompiler disagreement, caught and dropped rather than cited

Ghidra's decompile of case `9` renders an org-index exclusion guard shaped identically to §4's
(`if ((*psVar8 != 0) && (*psVar8 != 1)) { ... }`). This was checked against the raw listing at case 9's own
jump-table target (`0x92bfb`, per `QueryJumpTable.java`) specifically because the task instructs preferring
raw over decompile where they disagree — and here they do: the raw bytes at `0x92bfb` open directly with
`MOV EAX,0x1` (file `0x82bfa`), with no `CMP`/`JZ` guard anywhere in the immediately following instructions.
Per the task's own methodological priority, this claim is **dropped**, not cited as corroboration — it is
not load-bearing to the verdict (case 6, not case 9, is the vehicle-spawning path this document is about),
but it is disclosed here rather than silently omitted, since it demonstrates the raw-over-decompile
discipline was actually applied, not just stated.

---

## 7. What remains open (disclosed, not blocking the verdict)

- **Next row to open: `DAT_0018276c`'s `+0xC8` field's exact semantic, and whether `acquireTargetBuilding`
  matches `FUN_00091f70`'s destination semantics.** `O1-O2-M1-city.md` calls the field a "per-org status
  byte" in one context and a "closed" flag in another — not independently pinned down as "owning org id."
  This matters beyond curiosity: in `FUN_0003a910`'s retarget branch (`U1-U2-V1-incursion.md` §1.2),
  `FUN_00091f70`'s return value becomes `vehicle[+0x1D0]`, the vehicle's **new destination**. If `+0xC8` is
  an owning-org id, then the argument that call passes (`+0x12C`, i.e. the raider's *own* org) would make
  the retarget branch send the raider to a building *owned by its own organisation* — a redeploy/return-home
  step, not "attack a different enemy building." That reading is not idle speculation: this session's own
  §4.1 trace shows `slot[2]` is populated by `FUN_00091f70` called against the *candidate/victim* org (`CX`,
  not `SI`) at schedule time, and OpenApoc's `UnauthorizedVehicle` independently ends its own mission chain
  with `gotoBuilding(v->currentBuilding)` — a literal return-home step (§5.1). If the retarget branch's
  `FUN_00091f70(+0x12C)` call is *also* a return-home/redeploy lookup rather than a new-target search,
  OpenApoc's `acquireTargetBuilding()` (which, per the existing three tests, is written to search for a
  fresh attack target) may not match what the original actually does on this branch, independent of the
  owner-gate question this document answers. **This does not change the verdict on `organisation.cpp:1347`**
  — branch selection (retarget vs. portal) is decided purely by ownership, which is settled — but it is a
  concrete, scoped follow-up worth its own session: dump `+0xC8` across a run of `DAT_0018276c` records and
  check whether the values look like organisation indices (small integers, `0`–`26`-ish, clustering the way
  building ownership would) or a small status enum, then re-examine what `acquireTargetBuilding()` actually
  searches for against what `FUN_00091f70(+0x12C)` returns on the retarget branch specifically.
- **`slot[2]`/new-vehicle-`+0x15a`'s exact role** is now traced one step further than the paragraph above
  needs (§4.1: it is `FUN_00091f70(candidateOrg)`'s return value, resolved once at schedule time) but not
  chased to a specific *consumer* on the vehicle side — whether anything ever reads `+0x15a` back off the
  vehicle is unexamined.
- **The `[ESP+0x10e]`/`[ESP+0x110]` aliasing** that makes the type-candidate register track the inner loop
  counter (§2) is inferred from the confirmed byte ranges and the loop-width/inventory-table-width
  coincidence, not independently re-verified via a second, unrelated method.
- **The binary's numeric case values are not claimed to correspond 1:1 to OpenApoc's `enum class
  OrganisationRaid::Type` values** — only the behavioral shape (relation-only outcomes vs. the one
  vehicle-dispatching outcome) is compared.
- **Cases 8 and 9's own full semantics** were read from decompile for orientation and are not claimed
  raw-verified beyond the one check in §6 (which found a discrepancy and was dropped).
- **`EBX`'s exact derivation in `FUN_00092060`** (that it is genuinely `&DAT_0013e280 + slotIndex×8`, via
  `FUN_00091e68`'s return value) rests on the prior session's `getReferencesTo(0x13e280)` finding, not
  re-traced instruction-by-instruction this session (a same-project Ghidra-project lock conflict with a
  concurrent session prevented a quick re-check; not chased further since §4.1's four-field correspondence
  already makes the writer/reader link self-contained without it).

None of these affect the verdict: the ownership question the task asked — is this population ever
Aliens-owned, and does `organisation.cpp:1347` need to change — is answered by §3–§5 alone, independent of
every item in this section. The `+0xC8` item is flagged as the natural next thing to check, not as a gap in
this document's own claim.

---

## Coordinator note

This closes the "further work" item `U1-retarget-reconciliation.md` §2.3/§5 flagged as the one open
question blocking a full verdict on the retarget branch, and resolves the explicit "NOT established" caveat
already written into `vehiclemission.cpp:3143-3162`'s own comment. The code at that call site (`v.owner ==
state.getAliens()` → `gotoPortal`; otherwise fall through to `acquireTargetBuilding()`) does not need to
change — it was already shaped correctly by the agent that landed the alien-only fix, pending exactly the
confirmation this document provides. The one remaining action, not performed here because it falls outside
this task's write scope (`docs/original-game/findings/U1-scheduler-population.md` only, no other file), is
updating the "NOT established" language in `vehiclemission.cpp:3143-3162`'s comment to point at this
document once this finding is reviewed.
