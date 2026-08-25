# A2 follow-up · Panic psi upkeep diverges from prior art

**Status: OPEN. Locked to the code, not resolved.**

Writing the A2 lock test surfaced a genuine mismatch between
[psionics.txt](../../tools/extractors/docs/psionics.txt) and
`BattleUnit::getPsiCost` ([battleunit.cpp:35](../../game/state/battle/battleunit.cpp#L35)).

Upkeep is debited once per `TICKS_PER_PSI_CHECK`, which is half a second
([battleunit.cpp:3771](../../game/state/battle/battleunit.cpp#L3771)).

| Attack | Initial (code) | psionics.txt | Upkeep/check (code) | psionics.txt implies | |
|---|---|---|---|---|---|
| Control | 32 | 32 | 4 → **8/sec** | 8/sec | match |
| **Panic** | 10 | 10 | **3 → 6/sec** | **4/sec** (2/check) | **DIVERGES** |
| Stun | 16 | 16 | 5 → **10/sec** | 10/sec | match |
| Probe | 8 | 8 | 3 → **6/sec** | 6/sec | match |

**All four initial costs match. Three of four upkeeps match exactly. Only Panic differs.**

## Why it is not "fixed"

A single outlier among seven correct values looks like a transcription slip — `3` typed for `2`.
That is an **inference, not evidence**, and it could equally run the other way: `psionics.txt` is
prior-art, opening with *"Vanilla seems to work this way"*, and is not a recovered table. Neither
side is authoritative, and **TACP has not been asked**.

Per the [prime directive](../parity-guide.md#0-prime-directive), a prior-art document does not
license editing a shipped constant, and a shipped constant does not license editing the document.
So the lock test asserts **current behaviour** (3 per check), with the divergence recorded in a
comment at the constant. Editing `battleunit.cpp` now fails the test — which makes any future
change deliberate instead of silent.

## To close it

Bind the psi upkeep debit in TACP and read the Panic value directly. Whichever side is wrong, fix
that side and cite the binary + generation + file offset.

Until then this row stays open. **Do not resolve it by argument.**

## Two testability gaps found alongside

Both block *verification*, not behaviour, and both have precedent in this tree —
`TacticalAIVanilla::retreatChancePercent` was already given a seam for exactly this reason
(see [test_tactical_ai_retreat.cpp](../../tests/test_tactical_ai_retreat.cpp)).

1. **`getPsiCost` has internal linkage.** It is `static` at namespace scope in the header with its
   only definition in the `.cpp`, so no test can call it. The A2 case
   `psi_costs_match_prior_art` therefore asserts against mirrored constants and **can never fail** —
   it is provenance, not coverage. The case that does execute production code is
   `psi_upkeep_per_second`, which drives `updatePsi()`.
2. **`UnitAIVanilla::getWeaponDecision` / `getGrenadeDecision` are private**, and the only public
   entry routes through `canAttackUnit()` → `hasLineToUnit()` → `map.findCollision()`, requiring a
   populated battle map that no test in this suite builds. **A4 as originally specified cannot be
   closed without a seam.** The landed `test_unit_ai_priority.cpp` locks the formula's math contract
   and the AOE net-damage rule against synthetic tuples — clearly labelled as *not* executing game
   code — plus a real-data sanity guard over every fireable weapon's
   `(fire_delay, damage, accuracy)`.

Adding both seams is a small, precedented change to `game/`. It was out of scope for the
tests-only task that found them.

## TACP.EXE static sweep (2026-08-25) · genuine negative, row stays OPEN

Binary: `TACP.EXE`, CRC32 `0xfebbe39e` (verified against the canonical copy before use). Analysis
generation: Ghidra 12.1.3, `x86:LE:32:default` / `gcc`, project `OpenApocOG`, fresh full
auto-analysis run for every query below (no stale/reused analysis state trusted). Per the prime
directive, this section reports **what was searched and found**, not an inference. It closes none
of the row — it narrows where the answer cannot be — and no constant was invented to close it.

### Durable layout facts (not previously written down in this project)

`TACP.EXE` loads as three memory blocks. Getting the VA→file-offset mapping right for each was a
prerequisite for every citation below, and is worth recording since nothing else in the lab writes
it down:

| Block | VA range | Executable | File-offset formula | Confirmed by |
|---|---|---|---|---|
| `.object1` (code) | `0x10000`–`0xd44fe` | yes | `file_offset = VA + 0x5AAA4` | see below — two independent, non-circular checks |
| `.object2` (data) | `0xe0000`–`0x33f0af` | no | `file_offset = VA + 0x4FAA4` | 7 independent string locations (below), all consistent |
| `.image` (raw file mirror) | `0x0`–`0x305ff9` | no | `file_offset = VA` (identity) | matches file size (`0x305ffa` ≈ 3,170,298 bytes) and is where `.object2`'s raw bytes land 1:1 |

An earlier draft of this table used `file_offset = VA − 0x10001` for `.object1`, derived from
Ghidra's `sourceInfo.getFileBytesOffset()` (the `QueryFunctions.java` `page_file` column). That
number is **wrong** — for `.object1` that API has no backing `FileBytes` and silently returns `−1`,
so the formula was circular (it reproduced whatever arithmetic produced it, not an independent
file location), and a raw-file read at the offset it gave did not disassemble as valid x86. The
correct formula was hiding in this task's own brief: `FUN_00066474 ... bound_file=0xc0f18` gives
delta `0xc0f18 − 0x66474 = 0x5AAA4`. Two checks confirm it, both reproducible with plain Python on
`canonical/TACP.EXE`, no Ghidra required:

1. **Byte-level match.** `canonical/TACP.EXE[0xc0f18:0xc0f2a]` =
   `53 51 52 56 57 89 c2 66 83 b8 80 00 00 00 00 0f` — exactly `PUSH EBX; PUSH ECX; PUSH EDX;
   PUSH ESI; PUSH EDI; MOV EDX,EAX (89 C2 encoding); CMP word ptr [EAX+0x80],0x0`, matching
   `FUN_00066474`'s first six instructions from the known listing byte-for-byte (the `MOV EDX,EAX`
   uses the `89 /r` store-form opcode rather than the `8B /r` load-form one; both are the same
   instruction, just the other valid encoding).
2. **49-for-49 fixup-delta consistency.** Every one of the 49 `DAT_002b2854` references found by
   the whole-memory pointer scan below (Angle 3, method 3) was re-read directly from the raw file
   at `VA + 0x5AAA4`. All 49 store the *same* raw `int32`, `0x1d2854` — not `0x2b2854` — and
   `0x2b2854 − 0x1d2854 = 0xE0000` exactly, which is `.object2`'s own base VA. This is an internal
   LE/LX relocation fixup, not a data-corruption or compression artifact: object1 code stores
   cross-object absolute-address operands on disk as **object2-relative placeholders**
   (`target_VA − 0xE0000`), and the loader/Ghidra adds `.object2`'s base back in when reconstructing
   final memory. Spot-checked at 49 independent file locations spanning nearly the full `.object1`
   range (`0x12d54`–`0xbd01a`) with zero exceptions — this is a load-bearing, durable fact about
   this binary's relocation model, not a one-off.

**Practical consequence:** Ghidra's own reconstructed memory (`getBytes`/`getInt`/the decompiler,
i.e. everything the Ghidra-side scans below use) already applies this fixup and is reliable
directly. A *raw-file* search for a cross-object absolute address must instead search for
`target_VA − 0xE0000` if the reference originates in `.object1` code — this was the trap the first
draft of this section fell into and got backwards (it wrongly concluded `.object1` wasn't stored
linearly on disk; it is, this fixup is the actual explanation).

### Angle 1 — literal cost table, byte and word width: negative

Full-file (`3,170,298`-byte) contiguous scan in Python for the four initial costs
`{32, 10, 16, 8}` and both upkeep hypotheses `{4, 2, 5, 3}` (psionics.txt) / `{4, 3, 5, 3}` (code),
plus the combined 8-value and interleaved forms — at **byte width, all 24 permutations of the
initial four**, and again at **16-bit little-endian word width, all permutations**. Zero hits,
every case, both widths, in the *entire* file. Coverage is total and confirmed, not assumed:
`.object1` is stored linearly in the raw file at `file_offset = VA + 0x5AAA4` (proven above), so
this scan's range `[0, 3170298)` provably covers all of `.object1` (file `0x6aaa4`–`0x12efa2`), and
it covers the entire raw file regardless of where `.object2`'s VA-mapped region stops being
file-backed (the `VA + 0x4FAA4` formula is confirmed only in the ~0x1300-byte window actually
tested — see Angle 3 — not asserted across all 2.48 MB of `.object2`'s VA space). That gap doesn't
cost this scan any coverage: any part of `.object2`'s VA space with no file backing is necessarily
zero-filled at load and cannot itself hold a nonzero constant table, so a scan of `[0, filesize)`
is exhaustive over everything in the file that could possibly contain one. One
coincidental single word-width hit for the upkeep-only
permutation `(2,3,4,5)` at raw offset `0x2f7c1a` was found and set aside as noise: no corroborating
structure around it, and a run of four small 16-bit values is exactly the kind of thing that turns
up by chance in 3 MB of mixed binary data.

**There is no small contiguous psi-cost table anywhere in this file, at byte or word width, in any
attack ordering.**

### Angle 3 (strings) — four independent methods, one validated control, all negative

The five UI/battle-log strings named in the task brief are present at **7 on-disk locations**
(`"Psionic attack on unit"` ×2, `"Unit has panicked"` ×2, `"Panic unit"` ×1, `"Stun unit"` ×1,
`"Exit Psionics"` ×1 — confirmed by direct raw-file text search). Ghidra exposes each of those 7
locations under **two** address labels (one in `.object2`'s VA space, one in the `.image` raw
mirror, both pointing at the identical underlying bytes), for **14 Ghidra addresses** total: VAs
`0x28f77f`/`0x290a2c`, `0x28f6a8`/`0x2909ff`, `0x29053c`, `0x290532`, `0x2904ba`. Four independent
checks were run against all of them — the first three against a fresh full Ghidra auto-analysis,
the fourth pure Python on the raw file with no analysis state involved at all:

1. **`getReferencesTo(Address)`** (per the ground rules, not the `QueryDataRange.java` scalar-only
   scanner) on all 7 unique addresses (14 with duplicates): **0 references, every one.**
2. **Manual instruction-operand scan** over `.object1` (the sole executable block) — every
   instruction's `Scalar`/`Address` operands compared against all 7 target VAs: **0 hits.**
3. **Whole-reconstructed-memory pointer-value scan** — every initialized block (`.object1`,
   `.object2`, `.image`; ~6.5 MB of Ghidra-reconstructed bytes total) scanned for a stored
   little-endian `int32` equal to any target VA: **0 hits.**
4. **Raw-file placeholder-form scan** — method 3's control (below) showed `.object1` code stores
   cross-object references pre-fixup, as `target_VA − 0xE0000`, so a pointer to these strings could
   in principle sit anywhere in the file — including in `.object2` data or a table Ghidra never
   typed as a pointer — in that same placeholder form rather than as a final VA. A plain full-file
   (`3,170,298`-byte) search for `target_VA − 0xE0000` for all 7 targets: **0 hits.** (The same
   blind search for the control's placeholder, `0x1d2854`, returns **98** occurrences — a strictly
   *broader* net than method 3's 49, since it also catches raw dwords Ghidra never typed as a
   reference at all. Confirms the encoding is real and findable this way; it just isn't used for
   these strings.)

Method 3 needed a control to be trustworthy, because a naive version of it (raw Python file-slicing
for the target VA values directly) gave a **false** zero — an artifact of the `.object1` fixup
convention documented above (raw `.object1` bytes encode object2-relative placeholders, not final
VAs), not a real result. The control used `DAT_002b2854`, a global already known to be live (it
appears in `FUN_00066474`'s own decompile as `(&DAT_002b2854)[iVar2]`). Run through Ghidra's own
memory (not raw file bytes), the scan found **49 genuine hits** for `0x2b2854`, including both of
`FUN_00066474`'s own two references to it (VA `0x6664a`, `0x666bf`) — and, as detailed above, all
49 were independently re-confirmed against the raw file too, once the fixup offset was accounted
for. That validates the technique: when the answer should be "found", this method finds it. The
0-for-7 on the psi strings is therefore a **real negative**, not a technique artifact.

**Conclusion: none of these strings are reachable from anywhere in TACP.EXE's code or data via any
absolute-address mechanism this project can recover — final-VA form (methods 1–3) or the on-disk
object-relative placeholder form that `.object1` code actually uses (method 4).** They are almost
certainly accessed through an indirect, numeric string-ID/resource-table system rather than
baked-in pointers — consistent with this engine generation, but it means the "search near the
string" angle is a dead end here, not merely unproductive so far.

### Angle 2 — the tick-dispatcher consumer: negative, with unexamined remainder

`FUN_00066474` (VA `0x66474`, file `0xc0f18`) is the already-known per-unit real-time tick
dispatcher (drives Mind Shield/Disruptor Shield). Its full decompile was read line by line: retreat
timer countdown, health/morale/stamina regen decay, mind-shield charge regen (calls
`FUN_0006511c`), and the exact `applyMindShieldIncrement` logic inlined (`+0x1e` capped at `200`,
matching `BattleUnit::applyMindShieldIncrement` exactly) for units near a shield-aura source. **No
psi-energy debit, no status-keyed cost switch, anywhere in this function.**

Its two callers were decompiled in full — `FUN_000655d0` (VA `0x655d0`, file `0xc0074`, 3707
bytes) and `FUN_000b8c50` (VA `0xb8c50`, file `0x1136f4`, 602 bytes). Both are per-battle-unit
iteration loops (`do { ... } while` over a unit-pool array) that call `FUN_00066474` once per unit
alongside ~20 other opaque `FUN_XXXXXXXX` stage calls with no naming signal. `FUN_000655d0`'s own
loop body (the code *not* delegated to a callee) was read in full: no psi/energy-shaped logic
inline.

The ~27 unique sibling callees of both loops were then batch-decompiled and grepped for the
double-pointer-dereference shape (`*(int *)(*(int *)(...))`) that a real upkeep debit would need —
`psi_energy` lives on the attacker's *Agent*, reached through a pointer stored on the unit, so the
OpenApoc-side `updatePsi()` equivalent should show a double, not single, dereference. Exactly one
candidate matched: `FUN_0008879c` (VA `0x8879c`, file `0xe3240`, 1970 bytes, full 14,689-char
decompile read in its entirety). It is a large per-unit-event dispatcher keyed on a small integer
(`case 5/6/7/8/9`) that increments per-type hit counters at fixed byte offsets and calls a
sound/message function — a plausible read is that this is the general combat
hit-result/experience-tracking dispatcher (X-COM's classic stat-training system), not psi-specific.
**It contains none of the initial-cost immediates (`32/10/16/8`) and no subtraction against any of
the upkeep hypotheses.** Ruled out.

**Not examined:** `FUN_000b8ebc` (VA `0xb8ebc`, file `0x113960`, 10,477 bytes / 64,257-char
decompile, called twice from `FUN_000b8c50`) is large enough to plausibly be the main battlescape
per-unit action processor, but its size put a full manual read outside this task's budget; its
double-dereference grep came back negative, which is a weak signal against it but not a ruling.
This function — and, more generally, any psi upkeep debit reached through something other than an
absolute-address string reference or the specific tick path traced here — remains open ground for
a future pass.

### Verdict

**Still OPEN. The code is not changed.** This sweep is a clean, multi-method, cross-validated
negative on the two most promising static-analysis angles (a literal table; the string-anchored
attack/message function), and a partial negative on the third (the known tick dispatcher and its
immediate call graph, short of one large unexamined function). It does not prove no such table or
debit exists in TACP.EXE — an indirect/fixup-based table, or logic inside `FUN_000b8ebc` or a path
that doesn't route through `FUN_00066474`'s tick, remain unruled-out. But per the prime directive,
"no recovered constant or consumer" means the Panic upkeep value is **not** invented from this
work. `BattleUnit::getPsiCost`'s Panic upkeep stays at `3` (6/sec), and the divergence with
`psionics.txt`'s implied `2` (4/sec) stays recorded and unresolved.
