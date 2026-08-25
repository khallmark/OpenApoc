# O2 pass 2 — `Cargo::seize` diplomacy: event-type → cargo mapping, "worth" traced to its source

**NOT BOUND — and stronger than pass 1: the dispatcher's "worth" field is not merely unrelated to
cargo, every write to it found in this binary stores zero.** `FUN_000b32ac`'s `worth` variable
(`DAT_00244090`) and the `DAT_00244084` flag that gates half its case bodies are written by **two**
independent zero-initialization blocks: one inside `FUN_000aff9c` (`XOR EBP,EBP` then several
`MOV [addr],EBP` stores), and a second, standalone record-reset routine at VA `0xBC2B8` that
Ghidra's auto-analysis never linked into a `Function` object (`XOR EDX,EDX` then the same pattern).
`getReferencesTo` alone only found the first of these — it missed the second because that code
region was left as undisassembled data by auto-analysis, the same class of gap pass 1 already
flagged for two other call sites. An exhaustive raw byte-pattern scan across every initialized
memory block (not just resolved xrefs) was run specifically to close that gap, and it accounts for
every single occurrence of both field addresses in the binary: **two writers, both storing a
register that had just been zeroed two instructions earlier, and no third occurrence of either
address anywhere else.** Every branch in `FUN_000b32ac` gated on `DAT_00244084 == 1` is therefore
dead code, and the `worth × 50` / tiered-relation arithmetic in the two live branches always
computes against `0`. There is no cargo/item/price/count/divisor field feeding this dispatcher
because there is no *live value* feeding it at all, from either producer. **`Cargo::seize` must not
be wired from `FUN_000b32ac`/`FUN_000aff9c`; this closes the row.**

---

## 0. Scope, binary, and method compliance

Binary: `OpenApoc-og-research/canonical/UFO2P.EXE`, size 1,702,206 bytes, CRC32 `0x4749ffc1` — same
canonical copy as pass 1. Ghidra project `ghidra_projects/OpenApocOG.rep`, no re-import, analyzed
in place (`-processor x86:LE:32:default -cspec gcc`, `MarkObject1Executable.java` preScript,
unchanged).

This pass re-reads [`O1-O2-M1-city.md`](O1-O2-M1-city.md) (required prior art, summarized in §1)
and follows both method warnings from the task brief:

- **All new facts were pulled from the loaded Ghidra image** (decompiler + raw listing +
  `getReferencesTo`), never by seeking into the EXE at a hand-computed offset. Where a file offset
  could not be independently re-derived by real byte-signature match this session, that is stated
  plainly instead of guessed (§5).
- **`getReferencesTo(Address)` was used directly for every xref count in this document**, not
  `QueryDataRange.java`'s `instanceof Scalar` scan, per the documented undercount warning.
- **A first draft of this document over-trusted `getReferencesTo` as exhaustive** and asserted
  "exactly one write site" for `DAT_00244090`/`DAT_00244084` on that basis alone. An independent
  reviewer flagged that `getReferencesTo` only sees instructions Ghidra has already disassembled
  and attached a resolved operand-reference to, and pass 1 itself had already hit code regions
  auto-analysis left undisassembled. That flag turned out to be correct: §2/§3 below were rewritten
  after a raw byte-pattern scan (`QueryO2IndirectWrite.java`) found a second write to each field
  outside any analyzed `Function`. All new listing/byte reads in this document, including that
  scan, came from the loaded image via `getBytes`/`getInstructions`/`getReferencesTo` — no listing
  mutation (`disassemble`/`clearListing`) was performed on the shared project; the one script that
  would have needed it was deleted unrun in favor of hand-decoding the raw bytes, and later
  read-only queries were run with `-readOnly` so nothing could be committed back to the shared
  `.rep` project other sessions may be using concurrently.

A second review round after that rewrite flagged a further gap in the same vein: the §2.1 byte
scan only catches a write whose instruction encodes the target field's *own* address as a literal
operand, not one reached through a base pointer plus computed index (e.g. an `INC` into
`[EAX*4+0x244068]`, where index `7` or `10` lands exactly on `0x244084`/`0x244090`). That gap is
closed in §2.3a by decompiling the tally helper both zero-producers call and exhaustively
byte-scanning for the two plausible base constants.

New scripts added this session (in `OpenApoc-og-research/scripts/`, run via `ghidra_env.sh`
headless, no listing edits beyond the standard preScript; later ones add `-readOnly`):
`QueryO2Pass2.java`, `QueryO2Worth.java`, `QueryO2Case3Live.java`, `QueryO2FileOffsets.java`,
`QueryO2FileOffsets2.java`, `QueryO2FileOffsets3.java`, `QueryO2ObjectPageOffsets.java`,
`QueryO2IndirectWrite.java`, `QueryO2SecondWriter.java`, `QueryO2RawBytesWide.java`,
`QueryO2ResetCallers.java`, `QueryO2BaseWrites.java`, and their matching `run_o2_*.sh` wrappers.
Logs under `OpenApoc-og-research/export/o2_*_query.log`.

---

## 1. What pass 1 already established (not repeated in full)

Per `O1-O2-M1-city.md` §2: `FUN_000b32ac` (VA `0xB32AC`, file `0x115950`) is a 4-way dispatcher on
`DAT_00244024` (VA `0x244024`, file `0x174024`), whose only writer across all four of its
discriminant fields (`DAT_00244024`/`48`/`84`/`90`) is `FUN_000aff9c` (VA `0xAFF9C`, file
`0x112640`), reached only via `FUN_000ac348` → `FUN_000ac08c`. All seven of `FUN_000ac08c`'s call
sites were individually checked and all sit in the UFO-mission/scripted-incident family rooted at
`FUN_0003a910`; none touch a cargo/shipment structure. Org `+8` was bound as a funds field (not
relation). Pass 1's verdict: **NOT BOUND**, do not wire `Cargo::seize` from this dispatcher.

Pass 1 did not, however, trace **where the `worth` value itself comes from** — it decompiled
`FUN_000b32ac` using a variable name (`worth`) for what turns out to be `DAT_00244090`, but did not
follow that global back to its write site inside `FUN_000aff9c`, nor check whether
`DAT_00244084` (which gates roughly half of the dispatcher's case bodies) is ever actually
non-zero. That is the gap this pass closes.

---

## 2. `DAT_00244090` ("worth"): every write found, from two producers, stores 0

### 2.1 What `getReferencesTo` alone finds (incomplete — see 2.1b)

`getReferencesTo(0x244090)` across the entire analyzed program returns **6 references**:

```
XREF 000b027d type=WRITE fn=FUN_000aff9c@000aff9c insn=MOV dword ptr [0x00244090],EBP
XREF 000b3402 type=READ  fn=FUN_000b32ac@000b32ac insn=MOV EDX,dword ptr [0x00244090]
XREF 000b3428 type=READ  fn=FUN_000b32ac@000b32ac insn=MOV ESI,dword ptr [0x00244090]
XREF 000b3531 type=READ  fn=FUN_000b32ac@000b32ac insn=MOV EDX,dword ptr [0x00244090]
XREF 000b36ab type=READ  fn=FUN_000b32ac@000b32ac insn=MOV EDX,dword ptr [0x00244090]
XREF 000b36f9 type=READ  fn=FUN_000b32ac@000b32ac insn=MOV ECX,dword ptr [0x00244090]
```

One write, five reads, all five reads inside `FUN_000b32ac`'s own tiered-delta arithmetic (the
same statements pass 1 already quoted as `worth * -50` and the `-(worth-50)/20` / `-10-(worth-250)/50`
tiers).

**This is not the full picture.** `getReferencesTo` only reports instructions Ghidra has already
disassembled into a resolved operand reference. To check for writes it might have missed — exactly
the class of gap pass 1 already hit at two other call sites it could not resolve into a `Function`
— a raw byte-pattern scan (`QueryO2IndirectWrite.java`) was run over every initialized memory
block, searching for the literal little-endian operand bytes `90 40 24 00` (i.e. `0x00244090`)
wherever they occur, disassembled or not. It found **7 occurrences, not 6** — one more than
`getReferencesTo` reported, at VA `0xBC3D3`, inside a code region auto-analysis had left as
undefined data. The parallel scan for `DAT_00244084`'s bytes (`84 40 24 00`) found **8 occurrences
against `getReferencesTo`'s 7**, same story (extra hit at VA `0xBC3C3`). §2.1b and §2.3 below
account for both extra hits.

### 2.1b The second writer: a standalone record-reset routine at VA `0xBC2B8`

Ghidra's auto-analysis left VA `0xBC380`–`0xBC3EF` as undefined `[DataDB]` bytes with no containing
`Function` (`getFunctionContaining` returns null for both new hit addresses). The bytes are real
x86 code, though — hand-decoded from a read-only `getBytes` dump (`QueryO2RawBytesWide.java`,
no `disassemble()`/`clearListing()` calls, so nothing was written back to the shared project). A
`RET` at `0xBC2B7` closes the *previous* routine, and the standard `PUSH EBX,ECX,EDX,ESI,EBP`
prologue (bytes `53 51 52 56 55`) at `0xBC2B8` opens a new one that runs straight through to a
matching `POP EBP,ESI,EDX,ECX,EBX` / `RET` at `0xBC3E5`–`0xBC3EA`:

```
000bc2b8  PUSH EBX / PUSH ECX / PUSH EDX / PUSH ESI / PUSH EBP
000bc2bd  MOV EDX,0x2
000bc2c2  MOV ECX,0xffffffff
000bc2c7  XOR EAX,EAX
000bc2c9  MOV EBP,0xfffffc18                 ; -1000, same constant as FUN_000aff9c's _DAT_00244050
000bc2ce  MOV AX,word ptr [0x000d4d60]
000bc2d4  XOR EBX,EBX
000bc2d6  MOV [0x00244028],EAX
...
000bc2e0  MOV [0x00244020],EDX               ; DAT_00244020 := 2
000bc2e9  MOV [0x00244024],EBX               ; DAT_00244024 (event type) := 0, i.e. "no event"
...
000bc2f9  MOV [0x00244044],ECX               ; := -1
000bc302  MOV [0x00244048],ECX               ; := -1
...
000bc31b  MOV [0x00244050],EBP               ; := -1000
...
000bc399  MOV ECX,0x7
000bc39e  XOR EDX,EDX
000bc3a0  MOV [0x00244088],EAX               ; EAX still 0xffffffff from 0xbc394 (-1)
000bc3a5  MOV EAX,0x00244068
000bc3aa  MOV ESI,0x13
000bc3af  CALL 0xc02a7                       ; same tally helper FUN_000aff9c calls
000bc3b4  MOV EAX,[0x000e0e84]
000bc3b9  XOR EDX,EDX                        ; EDX re-zeroed, unconditional
000bc3bb  MOV [0x002440a0],ESI
000bc3c1  MOV [0x00244084],EDX               ; DAT_00244084 := 0   <- the "missing" write
000bc3c7  MOV [0x00244094],EAX
000bc3cc  MOV EAX,[0x000e0c9a]
000bc3d1  MOV [0x00244090],EDX               ; worth := 0          <- the "missing" write
000bc3d7  SAR EAX,0x10
000bc3da  MOV [0x0024409c],EDX
000bc3e0  MOV [0x00244098],EAX
000bc3e5  POP EBP / POP ESI / POP EDX / POP ECX / POP EBX
000bc3ea  RET
```

This is a **second, self-contained "reset the diplomatic-event record to defaults" routine**,
structurally parallel to (but not byte-identical to, and not called by) `FUN_000aff9c`'s own
init prologue: it sets the event type (`DAT_00244024`) to `0` ("no event"), the same sentinel
constants (`-1`, `-1000`) into the same neighbor fields, and — like `FUN_000aff9c` — zeroes
`DAT_00244084` and `DAT_00244090` via an `XOR reg,reg` two instructions earlier, unconditionally,
on every straight-line execution. **It is a second zero-writer, not a counterexample**: nothing
about this routine's own logic ever produces a non-zero value for either field.

Its caller was searched for and not found: `getReferencesTo(0xBC2B8)` returns zero results, and a
read-only scan of all 199,330 already-disassembled instructions in the program for any `CALL` whose
flow target is `0xBC2B8` also returns zero (`QueryO2ResetCallers.java`). This matches the same
"unresolved code region" pattern pass 1 already documented for two other call sites (file
`0x71503`/`0x7186b`) — most likely this routine is reached through an indirect/table-driven call
(several dispatchers in this binary already work that way, e.g. `FUN_00092470`'s type-switch
records) that neither this pass nor pass 1 traced. This is left open (§ "What remains open").

### 2.2 The first writer, in the raw listing (`FUN_000aff9c`)

Inside `FUN_000aff9c`, immediately after the 180-entry building/tile-array scan loop
(`ECX < 0xB4`, the same `DAT_00144918`-based loop pass 1 saw feeding `DAT_00244068`+category
counts):

```
000b0270  MOV EAX,[0x000e0e84]
000b0275  XOR EBP,EBP                          ; EBP := 0, unconditional
000b0277  MOV DL,byte ptr [0x000d5060]
000b027d  MOV dword ptr [0x00244090],EBP       ; worth := 0
000b0283  MOV [0x00244094],EAX
000b0288  MOV EAX,[0x000e0c9a]
000b028d  MOV dword ptr [0x0024409c],EBP       ; (adjacent field, also := 0)
000b0293  SAR EAX,0x10
000b0296  MOV dword ptr [0x00244084],EBP       ; DAT_00244084 := 0  (see §3)
000b029c  MOV [0x00244098],EAX
```

This is straight-line code — no branch, call, or flag-affecting instruction sits between the
`XOR EBP,EBP` at `0xB0275` and any of the three `MOV [addr],EBP` stores that follow it. `EBP` is
provably `0` at each store. The decompiler agrees: its C output for this span is literally
`DAT_00244090 = 0; ... DAT_0024409c = 0; DAT_00244084 = 0;` — raw listing and decompiler do not
disagree here, so there is no ambiguity to resolve in favor of one over the other.

### 2.3a A third gap, also closed: base-pointer/indexed writes to the record

A second review round flagged a real remaining hole: the field-address byte scan (§2.1) can only
see an instruction that encodes `0x244090`/`0x244084` as a literal operand. It is blind to a write
reached through a **base pointer plus computed index**, e.g. `INC dword ptr [EAX*4+0x244068]` —
whose operand bytes are `68 40 24 00`, not `90 40 24 00`. That pattern is not hypothetical: it
exists in `FUN_000aff9c` itself, in the 180-entry tally loop already described in pass 1
(`§2.2`/pass 1 §2.2's "`DAT_00244068`+category counts"):

```
000b0255  XOR EAX,EAX
000b0257  MOV AL,byte ptr [EBX + 0x22]         ; per-building category byte
000b025a  INC dword ptr [EAX*0x4 + 0x244068]   ; DAT_00244068 + category*4
```

`0x244068 + 7*4 = 0x244084` and `0x244068 + 10*4 = 0x244090` exactly — so *if* the category byte
can ever be `7` or `10`, this instruction increments one of the two fields this document claims are
always zero, and neither `getReferencesTo` nor the §2.1 byte scan would ever show it, since its
literal operand is `0x244068`, not the target field address.

Two checks close this:

1. **`FUN_000c02a7`** — the helper both known zero-producers call (`CALL 0xc02a7` at `0xB0235` and
   `0xBC3AF`) with `EAX=0x244068, ECX=7, EDX=0` just before this loop even starts — was decompiled
   in full. It is a generic "fill N dwords with a value" routine
   (`undefined4 *FUN_000c02a7(undefined4 *param_1, undefined4 param_2, uint param_3)`, a textbook
   duff's-device-style memset loop). With these arguments it zero-fills exactly **7 dwords starting
   at `0x244068`** — `0x244068` through `0x244080`, ending at `0x244084` **exclusive**. It does not
   touch `0x244084` or `0x244090` at all; it zeroes the tally array itself, immediately before the
   loop above populates it.
2. **A raw byte scan for both base constants** (`68 40 24 00` = `0x244068`, `20 40 24 00` =
   `0x244020`, the record's own start address) across every initialized memory block
   (`QueryO2BaseWrites.java`) found **3 total hits for `0x244068`** and **4 for `0x244020`** — and
   every single one is accounted for: the two `0x244068` hits inside `FUN_000aff9c` are the `CALL
   0xc02a7` setup and the `INC` above (both **before** that function's own `DAT_00244090 = 0; ...
   DAT_00244084 = 0;` block at `0xB027D`+); the one `0x244068` hit inside the `0xBC2B8` routine is
   its own `CALL 0xc02a7` setup, likewise before its zero-store block. Of the four `0x244020` hits,
   two are the known producers' own writes to that field (already covered), and the other two —
   inside `FUN_000b1db0` and `FUN_000b43c4` — are plain **reads** (`CMP`/`MOV`-load) checking the
   record's type tag, not writes.

So even without needing to know whether the category byte can ever reach `7` or `10` (it plausibly
cannot — the array is sized for `ECX=7` — but that was not independently verified): **the one
scaled-index write mechanism into this record was enumerated in full, and in both places it occurs
it runs strictly before that same function's own unconditional zero-store of `DAT_00244084`/`90`,
in straight-line code with no intervening branch back out.** Whatever value the tally loop leaves
behind is overwritten to `0` before either producer function returns — and every reader of either
field (`FUN_000b32ac` for `worth`; `FUN_000b32ac`, `FUN_000b21a8`, and `FUN_000b44a4` for
`DAT_00244084`, §3) only ever runs after one of the two producers has already returned, so none of
them can observe anything but the post-overwrite value. No other function touches either base
address at all.

### 2.3 Both scans now reconcile exactly

Raw-byte-scan totals (7 for `0x244090`, 8 for `0x244084`) minus `getReferencesTo` totals (6 and 7)
leave exactly one unexplained hit each — and §2.1b accounts for precisely that one hit, for both
fields, at the same two instructions (`0xBC3C1`/`0xBC3D1`) in the same routine. There is no
remaining unaccounted-for occurrence of either address anywhere in any initialized memory block.

**`worth` is not computed from a cargo/item value, an org record, or anything else — every write to
it found by either method is a compile-time constant zero, from either of its two producers, and
the sole live consequence of every `DAT_00244090`-gated computation downstream is a no-op:**
`org+8 += 0*-50` (re-writes the same value), and the tiered delta `-(worth-50)/20` etc. never fires
because `0x32 < 0` is false, so `FUN_0005faf0` (the relation-matrix writer) is **never called from
either of the two case bodies that reference `worth`**. This is not an information-theoretic proof
that no third, computed-address, indirect writer exists anywhere in the binary — no static method
can fully rule that out — but two independent search methods (resolved xrefs, and an exhaustive raw
operand-byte scan across all initialized memory) now agree on every occurrence found, and both
known producers are zero-writers by construction, not by incidental runtime state.

---

## 3. `DAT_00244084` is *also* hard-coded to 0 — this kills more of the dispatcher than pass 1 knew

Same story as `worth`: `getReferencesTo(0x244084)` reports **7 references**, one write —

```
XREF 000b0296 type=WRITE fn=FUN_000aff9c@000aff9c insn=MOV dword ptr [0x00244084],EBP
XREF 000b23f7 type=READ  fn=FUN_000b21a8@000b21a8 insn=CMP dword ptr [0x00244084],0x1
XREF 000b2517 type=READ  fn=FUN_000b21a8@000b21a8 insn=CMP dword ptr [0x00244084],0x1
XREF 000b32f6 type=READ  fn=FUN_000b32ac@000b32ac insn=CMP dword ptr [0x00244084],0x1
XREF 000b3514 type=READ  fn=FUN_000b32ac@000b32ac insn=CMP dword ptr [0x00244084],0x1
XREF 000b3562 type=READ  fn=FUN_000b32ac@000b32ac insn=CMP dword ptr [0x00244084],0x1
XREF 000b44b0 type=READ  fn=FUN_000b44a4@000b44a4 insn=MOV EBX,dword ptr [0x00244084]
```

— but per §2.1/§2.1b, the raw byte scan found an 8th occurrence: the second write inside the
`0xBC2B8` reset routine (`0xBC3C1`, `MOV [0x00244084],EDX` with `EDX` just zeroed). So this field
has **two writers, both zero**, same as `worth`.

Of the three functions that read this field, `FUN_000b32ac` and `FUN_000b21a8` each do so via a
direct `CMP dword ptr [0x00244084],0x1` — an explicit equality check. `FUN_000b44a4`'s single
reference (`MOV EBX,dword ptr [0x00244084]`) is a plain load, not a comparison; that function was
not decompiled this pass, so no claim is made about what it does with the value afterward. Since
both of this field's writers always store `0`, **`DAT_00244084 == 1` is never true**, which is
sufficient to establish the dead branches below in `FUN_000b32ac` (the function actually
decompiled and read in full this pass) and in `FUN_000b21a8` (whose own two reads are both the same
`CMP ...,0x1` pattern, though its branch bodies were not decompiled/examined). Restricting to the
dispatcher itself, using pass 1's own case numbering:

| Case | Branch gated on `DAT_00244084==1` | Status |
|---|---|---|
| 1 (`DAT_00244024==1`, `DAT_000d5060!=0`) | org-record `+0x2921` write, `DAT_0016eec6` flag bytes, **unconditional** `FUN_0005faf0()` relation call, `DAT_000f3c78` flag | **dead** — never reached |
| 2 (`DAT_00244024==2`) | `org+8 += worth*-50` (org index 0 only) | **dead** — case 2 *always* takes the `else`: `FUN_000705f8()` + `FUN_000b3114()` (base-slot deallocation, already bound as non-cargo in pass 1) |
| 3 (`DAT_00244024==3`) | the pointer-table walk over `DAT_001285e8` (`puVar7`), previously characterized as "infiltration/detection array walk" | **dead** — case 3's only live statement is the unconditional tail `FUN_00058280()` (see §4) |
| 4 (`DAT_00244024==4`) | mirrors case 1's `worth`-arithmetic block, gated on `DAT_000d5060==0`, not on `DAT_00244084` | live but inert per §2 (`worth` is always 0) |

This is a correction to pass 1's §2.4 case-3 description, not a contradiction of its verdict: pass
1 already correctly concluded case 3 "walks an unrelated infiltration/detection array," it just
didn't know that walk never executes. The corrected, *actually reachable* body of case 3 is
strictly smaller and simpler than pass 1 described.

---

## 4. What each case *actually* touches at runtime (revised)

With the dead branches removed, the four cases' **live** effects are:

- **Case 1 / Case 4** (`DAT_000d5060==0` path): `FUN_0006f738()` — decompiled in full this pass.
  Loops 14 times over two org records' `+0xD4` byte arrays (`param_1+0xD4` and a second org index's
  `+0xD4`), moving population/species counts from one org to the other one byte at a time
  (`*(char*)(param_1+0xd4) -= n; *(char*)(iVar2+0xd4) += n;`). This is the same per-org,
  per-species population table pass 1 already flagged as "U2 territory" (population transfer). No
  `cost`, `count`, `divisor`, or destination-building field anywhere in it.
- **Case 2**: `FUN_000705f8()` + `FUN_000b3114()` — already decompiled/bound in pass 1 as X-COM
  base-slot deallocation over the `0x2BE`-stride base-record array; re-confirmed unaffected by this
  pass's findings (its branch was never gated on the dead fields).
- **Case 3**: `FUN_00058280()` — newly decompiled this pass (file `0xBA924`, confirmed by direct
  `.image` byte-signature match, see §5). It (a) conditionally sets `DAT_00183a78` from a
  building/tile lookup; (b) decrements a per-org, per-species counter
  `(&DAT_00143a6c)[org*0x16A + species]` when a status-record field at `+300` reads `1` and a count
  at `+4` is under 10; (c) bumps two small casualty-style counters (`DAT_00183a70`,
  `DAT_00183a6e`, both capped at 5) when the `+300` field reads `3` and a code at `+4` falls in
  specific ranges; (d) touches a couple of unrelated UI/index flags
  (`DAT_000d75c4`, `DAT_000d4e28`). Again: no `cost`/`count`/`divisor`/destination-building shape —
  this reads like unit/population casualty bookkeeping, not cargo.

None of the three live tails reference an item id, a price/cost field, a quantity/divisor pair, or
a destination-building pointer — the defining shape of OpenApoc's `Cargo` class
(`game/state/city/vehicle.h`: `type`, `id`, `count`, `divisor`, `cost`, `destination`), whose
`Cargo::seize` (`game/state/city/vehicle.cpp:4158`) computes `worth = cost * count / divisor`. The
dispatcher's own `worth` field sharing that name with OpenApoc's FIXME variable is a coincidence
of a prior session's decompiler variable naming, not a structural match — and as shown in §2, the
binary's `worth` isn't computed from *anything*, cargo-shaped or otherwise.

---

## 5. File-offset citations and their method (per-address, honesty about what could and couldn't be re-derived)

| Address | VA | File offset | Method |
|---|---|---|---|
| `FUN_000b32ac` | `0xB32AC` | `0x115950` | Established in `O1-O2-M1-city.md` §0/§2.1 (`.image` byte match, cross-validated against task brief). Not independently re-derived this pass — my own raw-EXE grep for this function's entry bytes returned zero matches, consistent with the lab's documented note that some `.object1` pages are compressed and have no verbatim `.image` duplicate (this function's absolute-address operands likely differ pre-/post-relocation). |
| `FUN_000aff9c` | `0xAFF9C` | `0x112640` | Established in `O1-O2-M1-city.md` §2.2, **independently reconfirmed this pass**: the function's first 32 entry bytes, dumped live from the loaded Ghidra image, byte-match the canonical EXE at file `0x112640` uniquely (count=1). |
| worth zero-write insn | `0xB027D` | not independently re-derived | `FUN_000aff9c`+`0x2E1` (confirmed by VA arithmetic against the byte-matched entry above). Direct byte-signature match on this instruction's own bytes failed against the raw EXE (it encodes the absolute address `0x00244090` as an operand, which is a plausible relocation-fixup candidate). `object-page file 0xA027C` is available as the lab's documented fallback citation method (`MemoryBlockSourceInfo`-based, same method validated for `FUN_0003a910 @ object-page file 0x2A90F` in pass 1) if a second citation is wanted. |
| `DAT_00244084` zero-write insn | `0xB0296` | not independently re-derived | Same situation as above; `FUN_000aff9c`+`0x2FA`; `object-page file 0xA0295` available as fallback. |
| `FUN_00058280` | `0x58280` | `0xBA924` | Direct `.image` byte match, unique (count=1), confirmed this pass. |
| `FUN_0006f738` | `0x6F738` | not independently re-derived | Raw-EXE grep on entry bytes returned zero matches (this function opens with a `CALL rel32`, another relocation-fixup candidate). `object-page file 0x5F737` available as fallback. |
| `FUN_000b3114` | `0xB3114` | `0x1157B8` | Already established in `O1-O2-M1-city.md` §2.2 for the type-2 else-branch; not re-derived this pass. |
| `FUN_000b1dc0` (sole caller of `FUN_000b32ac`, §6) | `0xB1DC0` | not independently re-derived | Raw-EXE grep failed for the same relocation-fixup reason. `object-page file 0xA1DBF` available as fallback. |
| `DAT_00244090` (worth) | `0x244090` | `0x174090` | `.object2` linear remap `VA − 0xD0000`, the method `O1-O2-M1-city.md` §0 already validated against the task brief's own citation for `DAT_00244024`. |
| `DAT_00244084` | `0x244084` | `0x174084` | Same remap. |
| second reset routine (§2.1b) | `0xBC2B8` | none — not a citable file offset | Not linked into a `Function` by Ghidra's auto-analysis (`getFunctionContaining` returns null), so neither the `.image` byte-match nor the `object-page` `MemoryBlockSourceInfo` method applies in the form this lab has used them elsewhere (both are keyed off analyzed code/function boundaries). Hand-decoded directly from a read-only `getBytes` dump; VA is exact and traceable, file offset is deliberately left uncited rather than guessed. |
| `FUN_000c02a7` (tally/memset helper, §2.3a) | `0xC02A7` | not independently re-derived | VA and full decompile obtained directly from the loaded image; file-offset byte-match not attempted this pass — not needed for the argument in §2.3a, which turns on the function's *behavior* (which dwords it writes), not its file position. |

Nothing above was verified by seeking into the raw EXE at a computed offset and trusting a match
by construction — every "not independently re-derived" row is stated as such rather than filled in
with a guess, and every filled-in row was produced by an actual byte-for-byte match against
`canonical/UFO2P.EXE` (either directly this pass, or already cross-validated in pass 1).

---

## 6. Task item 3 — no path from a cargo check into the dispatcher (reconfirmed, tightened)

Pass 1 already exhaustively checked all seven call sites of `FUN_000ac08c` (the sole route into
`FUN_000aff9c`, via `FUN_000ac348`) and found none cargo-related. This pass adds two structural
facts that make the "no path" conclusion tighter, not just re-asserted:

- **`FUN_000b32ac` itself has exactly one caller in the whole binary**:
  `getReferencesTo(0xB32AC)` returns a single `UNCONDITIONAL_CALL` from `FUN_000b1dc0` (VA
  `0xB1DC0`) — one of the four "downstream READ consumer" functions pass 1 already listed as
  reading `DAT_00244024` alongside `FUN_000b32ac`. There is no second entry point into the
  dispatcher to re-check.
- **`FUN_000ac348` (the only function that calls `FUN_000aff9c`) also has exactly one caller**:
  `FUN_000ac08c`, matching pass 1's chain exactly. `getReferencesTo` found no additional callers
  this pass.
- Even setting the call-graph aside: **`DAT_00244090` has exactly two write instructions in the
  entire binary, found by combining resolved xrefs with an exhaustive raw-byte scan** (§2), one
  inside `FUN_000aff9c` and one inside the standalone reset routine at `0xBC2B8`, and both are
  hard-coded to zero. There is no global, parameter, or return value by which *any* caller —
  cargo-related or otherwise — could inject a non-zero worth into this dispatcher through either
  producer. The field has no external write API. A hypothetical future patch that added a call from
  `Building`'s cargo-impound logic into this chain would still produce `worth = 0` unless it also
  patched one of these two functions itself, which is a much bigger claim than "this dispatcher
  secretly already models cargo seizure."

---

## 7. Verdict

**NOT BOUND**, and more firmly closed than pass 1 left it. The event-type dispatcher
(`FUN_000b32ac`/`FUN_000aff9c`) has no cargo/shipment-shaped consumer in any of its four cases
(§4), and its `worth` field is a hard-coded zero at both write sites found in the binary — one
inside `FUN_000aff9c`, and a second, independent one inside a standalone reset routine
`getReferencesTo` missed and this pass located by raw byte-pattern scanning (§2). The flag that
would have activated the dispatcher's second (currency-adjacent) branch per case is *also*
hard-coded to zero at both of its write sites (§3), which kills more of the dispatcher's code than
pass 1 realized was already unreachable. The call paths into this machinery remain single-threaded
and UFO-mission-rooted for the dispatcher itself (§6), exactly as pass 1 found; the second reset
routine's own caller could not be located and is left open, but its existence only adds a second
*zero*-writer, it does not open a path to a nonzero one.

**Recommendation, unchanged and now on firmer ground: do not wire `Cargo::seize`'s relationship
FIXME (`game/state/city/vehicle.cpp:4183`) from `FUN_000b32ac`, `FUN_000aff9c`, or the
`DAT_00244024`/`48`/`84`/`90` event-type record.** Per the prime directive, `Cargo::seize`'s
diplomacy adjustment stays prior-art / un-invented; this row is closed as a clean negative.

### What remains open (not claimed, not investigated this pass)

- **The caller of the standalone reset routine at `0xBC2B8` (§2.1b) was not found.** Both
  `getReferencesTo` and a read-only scan of all ~200k already-disassembled instructions for a
  direct `CALL` came back empty; it is most likely reached through an indirect/table-driven call,
  consistent with this binary's other dispatch-table patterns, but that was not traced. This does
  not affect the verdict (the routine is a zero-writer regardless of who calls it or when), but a
  future session that wants to know *when* the event record gets reset to "no event" would need to
  resolve this.
- Whether a third writer to `DAT_00244090`/`DAT_00244084` exists via an address computed from a
  *variable* base register (loaded from something other than the literal constants `0x244020` /
  `0x244068` checked in §2.3a — e.g. a pointer passed in from yet another, differently-based
  caller) was not and cannot be fully ruled out by any static scan; see the caveat at the end of
  §2.3. What *was* checked and closed (§2.3a): every literal-constant-based route into this record
  this session could think to check (the field addresses themselves, the record's own base
  address, and the tally-array base the loop indexes into) is accounted for, and none produces a
  surviving non-zero value. The residual gap is narrower than "cannot be ruled out" — it requires a
  base pointer neither of this record's two known accessors' own constants, that no scan target
  this pass chose would have caught.
- Whether `FUN_000b44a4` (one of `DAT_00244084`'s three readers, §3) or `FUN_000b21a8`'s own
  branch bodies do anything that matters was not examined — only their `CMP`/`MOV` read
  instructions were confirmed to exist; this pass did not decompile either function.
- `DAT_0018276c+200`'s exact semantics beyond "per-org status byte" (unchanged from pass 1).
- `DAT_000d5060`'s full meaning (treated as a live runtime flag throughout this project's O1/O2/M1
  work; not re-derived here).
- `DAT_0024408c` (written early in `FUN_000aff9c` at file-relative offset before the block examined
  in §2, read once by `FUN_000b32ac` at `0xB32BD`) was not traced — it did not intersect the
  worth/`DAT_00244084` question this pass focused on, and no claim is made about it.
- Whether `FUN_000b3114`, `FUN_0006f738`, `FUN_000b1dc0`'s file offsets could be recovered via the
  `object-page file` fallback method with more effort was not pursued past the one attempt logged
  in §5 — VA citations for these are exact and sufficient for the call-graph argument in §6, which
  does not depend on the file-offset number.
