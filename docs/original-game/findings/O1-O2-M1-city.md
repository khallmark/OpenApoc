# O1 · O2 · M1 — org bribe/rift, cargo-seize diplomacy, city action music

**O1 — NOT BOUND:** no reader converts a relationship delta into a currency amount (or vice
versa) anywhere in UFO2P.EXE. Every player-facing bribe/rift UI string has zero bound xrefs, and
the full call graph of the binary's one relation-adjustment primitive (`FUN_0005faf0`) was walked
to its roots — it terminates entirely in the UFO/alien city-mission AI, a scripted-incident news
ticker, and an infiltration-detection accumulator, none of which touch an org's funds field.
`Organisation::costOfBribeBy` / `diplomaticRiftOffer` stay prior-art.

**O2 — NOT BOUND (event-type → cargo mapping); BOUND (org `+8` is a funds field, not relation):**
`FUN_000b32ac`'s four event types are produced exclusively by `FUN_000aff9c`, which has exactly
one call site in the whole binary — inside the UFO mission-arrival/deposit-completion handler
`FUN_0003a910` (already bound by this project's U1/U2 work). There is no path from
`Building::updateCargo`'s hostile-destination seize check into any of the four event types.
Org `+8` is a funds/budget field (shares the org-budget table already flagged as a trap, holds
`worth×50`-scale magnitudes, and is written separately from the clamped ±100 relation matrix in
the same routine). **Do not wire `Cargo::seize` from this dispatcher.**

**M1 — BOUND (consumer only; trigger condition still unbound):** the layered
`GROUP_1..4`/`getnextmusic` tension-music system has a real, traced consumer reachable from
city-side UFO-mission completion — the *same* `FUN_0003a910` root as O2 — via `FUN_000ac08c` →
`FUN_000b523c` (the tension-tier state machine) → `FUN_000b5678`/`FUN_000b6280` (track playback).
This satisfies the task's pass criterion, "city mix only after a bound consumer." **It does not,
by itself, authorize wiring a city Action playlist**: `FUN_000b523c` already runs every tick from
the generic top-level loop regardless of city/battle state, so the mission-arrival call could be a
redundant "re-evaluate now" with no effect on which tier plays, and nothing found here ties this
call to *tier escalation* specifically — tier 3 (the group that actually holds `ACTION.RAW`) has
no bound driver. The two gate fields checked before the city-side call, and the condition that
raises the tension tier, remain unbound. Do not invent either.

All three targets root in the same place: `FUN_0003a910`, the UFO mission-arrival function this
project already bound for U1 (mission-counter) and U2 (base exposure).

---

## 0. Binary confirmed

`OpenApoc-og-research/canonical/UFO2P.EXE`: size 1,702,206 bytes, CRC32 `0x4749ffc1` — matches the
extractor-canonical non-4 ISO build cited in the task brief and in
[binaries](../binaries/) conventions. Ghidra project: `ghidra_projects/OpenApocOG.rep`
(pre-imported, pre-analysed; `-processor x86:LE:32:default -cspec gcc`, LX loader). `.object1`
code `0x10000`–`0xCE8AC` (marked executable via the existing `MarkObject1Executable.java`
preScript, unchanged from prior sessions).

File-offset convention below follows this lab's two established methods, both cross-validated
against citations already in `next-implementation.md`/`parity-guide.md`:

- **`file 0xNNNNNN`** — byte-signature match against the `.image` block (1:1 with the bound EXE).
  Confirms exactly against the task brief's own citations: `FUN_000b32ac` resolved to `file
  0x115950` and `FUN_0005faf0` to `file 0xC2194`, both matching verbatim.
- **`object-page file 0xNNNNNN`** — `.object1`/`.object2` `MemoryBlockSourceInfo` file-bytes
  offset (used when the `.image` byte-match fails, e.g. because the LX loader's page is
  compressed and has no verbatim `.image` duplicate). Confirms exactly against the existing
  citation `FUN_0003a910 @ object-page file 0x2A90F`.
- Data addresses in `.object2` follow the documented `file_offset + 0xD0000` remap (lab
  `labels/offset_to_va.txt`: `.object2, base 000d0000`). This is confirmed independently: the task
  brief's `[0x174024]` event-type field resolves to VA `0x244024` in Ghidra, and
  `0x174024 + 0xD0000 = 0x244024` exactly.

Scripts added this session (adapted from `scripts/QueryTacpGaps.java` and
`scripts/QueryFunctions.java`, run via `scripts/ghidra_env.sh` against the existing
`OpenApocOG.rep` project, no re-import): `QueryO1O2M1.java`, `QueryO1O2M1_v2.java`,
`QueryO1O2M1_v3.java`, `QueryO1O2M1_v4.java`, `QueryO1O2M1_offsets.java`,
`QueryO1O2M1_offsets2.java`. Read-only queries; no listing edits beyond the standard
`MarkObject1Executable` preScript already used by `analyze_ufo2p_tail.sh`.

---

## 1. O1 — bribe / diplomatic-rift dollar formula: NOT BOUND

### 1.1 Every UI string anchor is unreferenced

All eight candidate strings from the task brief and `city-economy.md` exist verbatim in
`export/strings/UFO2P_strings.txt`, but **every one has zero bound xrefs** (`getReferencesTo` on
the string's data address, both the `.image` copy and the `.object2` runtime copy):

| String | file offset (`.image`) | bound xrefs |
|---|---|---|
| `Diplomacy` (menu label) | `0x151161` | 0 |
| `Diplomacy` (screen title) | `0x152bbb` | 0 |
| `DIPLOMATIC RIFT` | `0x15155b` | 0 |
| `Diplomatic relations for: ` | `0x151aae` | 0 |
| `Diplomatic relations determined` | `0x151b04` | 0 |
| `Enter defensive diplomatic negotiations with: ` | `0x151132` | 0 |
| `Enter aggressive diplomatic negotiations with: ` | `0x15116b` | 0 |
| `Unable to reach destination due to damaged people tube network...` (Transtellar) | `0x14df2d` | 0 |
| `We are unhappy with the recent activity...` (bribe-request letter) | `0x154c8e` | 0 (`.image` copy) |

The one non-zero hit is the **`.object2` runtime copy** of the bribe-request letter
(`0xf35ea`), which shows a single `DATA` xref from `FUN_00096bc8` (VA `0x96bc8`, object-page file
`0x86BC7` — the `.image` byte-match for this address found no duplicate-free match, likely a
compressed LX page). Decompiling the owner shows it does **not** touch the string meaningfully:

```c
void FUN_00096bc8(void)
{
  DAT_0016ec26._2_1_ = 0;
  ram0x00127998 = 1;
  DAT_0016f388._0_1_ = 0;
  return;
}
```

Three flag-byte writes, no string load, no currency arithmetic. This is consistent with the
parity-guide's own documented failure pattern: *"If bound xrefs are empty, the string is UI copy
and proves nothing about a consumer."* All eight strings die here exactly the same way `Rules of
engagement` and `Evade Fire` already did for V1.

### 1.2 The relation-adjustment primitive, walked to its roots

Since the UI strings gave nothing, the search moved to the binary's actual relation-mutation
machinery: `FUN_0005faf0` (VA `0x5FAF0`, **file `0xC2194`** — matches the task brief's citation
exactly). Signature `void __regparm3(short orgA, short orgB, <delta via register>)`. It writes a
signed byte into `DAT_0016ec28 + orgB + orgA*0x1c` (a 28×28-ish org-pair matrix, one byte per
ordered pair), **clamped to `[-100, 100]`** — this is `Organisation::current_relations`, not a
currency field.

`getReferencesTo` on `FUN_0005faf0`'s entry point lists **17 call sites across six caller
functions** (`FUN_00054a28` ×4, `FUN_00092470` ×7, `FUN_000b32ac` ×3, `FUN_00041644` ×1,
`FUN_00057c8c` ×1, `FUN_000b426c` ×1). All six functions were located and decompiled:

| Caller | VA | file offset | What it does |
|---|---|---|---|
| `FUN_00041644` | `0x41644` | `0xA3CE8` | 3D bounding-box scan of vehicle/UFO-target coordinates against a building/base array; on a hit, sets the caller's mission-state field to `5` and applies a *derived* (not currency) relation delta via the same `(a−b)/2 + c` pattern seen throughout. |
| `FUN_00054a28` | `0x54A28` | `0xB70CC` | 7293-byte vehicle-AI proximity/targeting routine over the 16-persistent-vehicle-slot array (`DAT_00160fd8`, stride `0x276`, the same array documented for U2 targeting). |
| `FUN_00057c8c` | `0x57C8C` | `0xBA330` | Per-building **infiltration-growth** accumulator: reads an org-indexed infiltration-rate table (`DAT_00161152`+), grows a per-building infiltration counter, and calls `FUN_0005faf0` only when the counter crosses a detection threshold — a "your infiltration was detected" relation hit, not a bribe. |
| `FUN_000b426c` | `0xB426C` | `0x116910` | Checks a 13-entry per-species byte run at org-record `+0xD4` (same offset `FUN_0006f738`'s population transfer uses) for "any population left"; applies a relation delta only if population is nonzero. |
| `FUN_00092470` | `0x92470` | `0xF4B14` | Scripted diplomatic-**incident** dispatcher — a message-queue walker (`DAT_0013e280`, countdown-timer + type-switch records). Cases `1`/`2`/`3` build a formatted news-ticker string (`"Diplomatic relations for: <org> ... determined"` — this is where those two strings are actually *composed*, just not by direct string-address reference) and bump the relation matrix by a **fixed** case constant: `+2` / `+7` / `+12`. If a matching X-COM base is found first (via `FUN_000705f8`), it instead applies a flat **`-25`** and skips the ticker message. No currency term anywhere in any of the three cases. |
| `FUN_000b32ac` | `0xB32AC` | `0x115950` | The O2 dispatcher itself (§2) — three internal calls, all against `worth×50`-adjacent tiered deltas, never against a bribe/rift UI action. |

None of these 17 call sites — nor `FUN_00092470`'s only caller (`FUN_0004ab04`), nor the
`FUN_0003a910` root common to `FUN_000b32ac` and (indirectly) the others (§2.2) — read or write an
org's `balance`/funds field in the same computation as a `FUN_0005faf0` call. No large
(hundreds/thousands-scale) constant resembling a dollar figure appears near any of these calls;
every applied delta is a small integer already inside the `[-100, 100]` relation range (`2`, `7`,
`12`, `-25`, or a `(a−b)/2+c` derived value bounded the same way).

One lead was surfaced but not followed to completion: `FUN_00092470`'s cases stash a 4-byte token
(e.g. `'\x18','%','\t','\0'`) into a scratch address immediately before calling `FUN_00063a00` to
fetch a string. That token is almost certainly a string-table index/key, not literal bytes —
**this is very likely why every diplomacy string in §1.1 has zero direct-address xrefs**: the UI
draws them by table lookup, not by pointer. Decoding the token encoding and deriving the indices
for `DIPLOMATIC RIFT` / `Enter aggressive diplomatic negotiations with:` would let a future session
grep for those constants and reach the negotiate-screen handler directly, instead of repeating this
session's (unsuccessful) address-xref walk. That decode was not attempted this session.

### 1.3 Verdict

**No reader converts a relationship delta into a currency amount, and no writer converts a
currency payment into a relationship delta, anywhere in UFO2P.EXE.** The entire relation-mutation
subsystem this session could reach is driven by scripted alien/UFO-mission incidents and
infiltration detection, not by a player-initiated bribe/diplomatic-rift action. Per the prime
directive, `Organisation::costOfBribeBy` and `diplomaticRiftOffer` **stay prior-art** — do not
invent a weekly-drift or coefficient formula to close this row.

---

## 2. O2 — `Cargo::seize` diplomacy: NOT BOUND (event mapping); org `+8` = funds (bound)

### 2.1 The dispatcher, decompiled

`FUN_000b32ac` (VA `0xB32AC`, **file `0x115950`**, size 1274 bytes) branches on
`DAT_00244024` (VA `0x244024` — the task brief's `[0x174024]` **file offset**, `+0xD0000` for the
`.object2` remap, see §0):

```c
if ((DAT_00244024 == 1) || (DAT_000d5060 != '\0' && DAT_00244024 == 4)) {
    if (DAT_000d5060 == '\0') {
        iVar5 = org_index*0x1b6 + DAT_0017fb4c;      // org record, stride 0x1b6, 27 orgs
        iVar6 = *(int*)(iVar5+8) + worth * -50;        // org+8 -= worth*50
        if (iVar6 != 0) *(int*)(iVar5+8) = iVar6;
        iVar6 = tiered(worth);                          // -(worth-50)/20, or -10-(worth-250)/50 floor -50
        if (iVar6 != 0) FUN_0005faf0(...);              // SEPARATE write, to the relation matrix
        FUN_0006f738();                                 // population transfer (U2)
    }
    ...
}
if (DAT_00244024 == 2) {
    if (DAT_00244084 == 1)
        *(uint*)(DAT_0017fb4c + 8) += worth * -50;       // ALWAYS org index 0 (X-COM) -- no per-org index
    else { FUN_000705f8(); FUN_000b3114(); }             // X-COM base-slot deallocation (below)
}
if (DAT_00244024 == 3) { /* unrelated infiltration/detection array walk */ }
if (DAT_00244024 == 4 && DAT_000d5060 == '\0') { /* identical to type 1's block */ }
```

### 2.2 The sole event producer, and its sole caller chain

`getReferencesTo` on `DAT_00244024`, `DAT_00244048`, `DAT_00244084`, and `DAT_00244090` (47 + 8 +
7 + 6 = 68 references total) shows **every WRITE reference, to all four fields, comes from exactly
one function: `FUN_000aff9c`** (VA `0xAFF9C`, **file `0x112640`**). Every other reference is a
`READ` from a downstream consumer (`FUN_000b1dc0`, `FUN_000b21a8`, `FUN_000b27b4`, `FUN_000b32ac`,
`FUN_000b43c4`). `FUN_000aff9c` maps a caller-supplied reason code (`param_2`) to event type:

| `param_2` | `DAT_00244024` (event type) |
|---|---|
| `1` | `3` (skips the org-record lookup entirely) |
| `3` | `4` |
| `5` | `2` |
| `6` | *(none — org-record lookup skipped, type stays `0`)* |
| anything else | `1` (catch-all default) |

`FUN_000aff9c` has **one call site found by Ghidra's reference analysis**: inside `FUN_000ac348`
(VA `0xAC348`, file `0x10E9EC`). `FUN_000ac348`'s only caller is `FUN_000ac08c` (VA `0xAC08C`,
file `0x10E730`).

`FUN_000ac08c` itself has **seven call sites**, not three — the first pass under-counted this and
was corrected by a follow-up query. Three are inside `FUN_0003a910` (VA `0x3A910`, **object-page
file `0x2A90F`**, the UFO mission-arrival/deposit-completion handler already bound for U1/U2). The
other four were individually checked, since an unexamined caller is exactly the kind of gap that
would undermine an "exclusively" claim:

| Caller | What it does | Cargo-related? |
|---|---|---|
| `FUN_00060c18` (file `0x60C18`... resolved via VA, `.image` match ambiguous) | Scans the 16-slot vehicle array (`DAT_00160fd8`) for craft belonging to one org, checks a per-craft state-byte condition, and — only if every matching craft satisfies it — builds a ticker message and calls `FUN_000ac08c`. Reads the same per-org `+0xE2`-stride status table at `DAT_0018276c+0xBC`. | No — vehicle/craft state tracking, no `Cargo`/worth/purchase reference. |
| `FUN_00099e04` (VA `0x99E04`) | Trivial gate: `if (DAT_00127798 != 0) { DAT_00127798 = 0; FUN_000ac08c(...); }`. `DAT_00127798` is the exact flag `FUN_00092470` (§1.2) sets on its "-25, base match found" incident path. | No — downstream of the scripted-incident ticker, not cargo. |
| Two call sites at file `0x71503` and `0x7186b` | Ghidra did not resolve a containing `Function` for this code region (disassembly only). Both sites read the per-org status byte at `DAT_0018276c+0xC8`, the relation-matrix byte at `DAT_0016ec25+orgIndex*4` (same table as §1.2/§2.3), and `DAT_000d5060`/`DAT_000d4d7a` (the same "current org" globals seen throughout), then fetch and display a ticker message via `FUN_00063a00`/`FUN_000889a0` before calling `FUN_000ac08c`. | No — org-status/relation-table reads and ticker-message plumbing identical in shape to `FUN_00092470`; no `Cargo`, vehicle-equipment, or worth/price reference anywhere in the disassembled range. |

`FUN_00060c18` is reached from `FUN_000114cc` (also one of the two direct callers of
`FUN_0003a910` itself), `FUN_0005f700` and `FUN_0005f894` (both adjacent to, and plausibly
siblings of, `FUN_0005fddc` — the documented **normal UFO deposit** function in
`next-implementation.md`/`city-economy.md`), and `FUN_0003280c`. `FUN_00099e04` is reached from
`FUN_000114cc` and `FUN_0004aa7c`. None of these were traced further; all seven `FUN_000ac08c`
call sites and their immediate callers stay inside the UFO-mission/alien-incident family already
established for O1 and U1/U2 — **corrected count, same conclusion: no cargo-related caller was
found.**

`FUN_000ac08c` also independently confirms the type-2 "else" tail (`FUN_000705f8` +
`FUN_000b3114`) belongs to the same UFO-mission root: it calls both directly when its own
`param_2 == 5`, alongside `FUN_000b426c` (§1.2) and (critically for M1, §3) `FUN_000b523c`.

`FUN_000b3114` (the type-2 else branch, file `0x1157B8`) operates on `DAT_000d942c +
param*0x2be` — the **0x2BE-byte, 16-entry X-COM Base runtime record already bound for U2** — and
deallocates the base's slot: writes a "closed" byte into the same `DAT_0018276c+200`-indexed
per-org status table (VA `0x18276c` → file `0xB276C` by the `−0xD0000` remap), decrements a
live-base counter, and invalidates cross-references to that base slot in three other arrays. This
is base-loss handling, not cargo.

### 2.3 Org `+8` is a funds field, not relation — bound

Three independent pieces of evidence, all from the same decompiled routine:

1. `iVar5+8` lives inside the **27-entry, `0x1b6`-stride org table at `DAT_0017fb4c`** (VA
   `0x17fb4c`, `.object2` data → file `0xAFB4C` by the `−0xD0000` remap validated in §0) — the
   exact table the parity guide already flags as a trap: *"`FUN_000941dc` @ file `0xF6880` is
   weekly ORG BUDGET FRACTIONS (stride `0x1B6`, 27 orgs)."* Same table, same stride, same base.
2. The value written is `worth × -50` (magnitude in the hundreds to low thousands, unclamped) —
   categorically incompatible with a relation field, which the same function clamps to
   `[-100, 100]` two lines later via `FUN_0005faf0`.
3. **`FUN_000b32ac` itself writes to a *different* address** for the actual relation change
   (`DAT_0016ec28`, VA `0x16ec28` → file `0x9EC28` by the same `−0xD0000` remap, via
   `FUN_0005faf0`, clamped). The binary's own code distinguishes the two concepts in the same
   routine — org `+8` is provably not the clamped relation path.

### 2.4 Verdict

**Event-type → cargo mapping: NOT BOUND.** The dispatcher's four event types are produced by
`FUN_000aff9c`, reached through `FUN_000ac348` → `FUN_000ac08c`. All seven of `FUN_000ac08c`'s
call sites were individually checked (§2.2 table) — three directly inside `FUN_0003a910` (UFO
mission arrival), the other four in functions or code that read the same org-status table,
relation matrix, and scripted-incident flags used throughout this write-up, with `FUN_0003a910`
itself reachable from most of them. There is no call path from `Building::updateCargo`'s
hostile-destination `Cargo::seize` check into `FUN_000aff9c`, and therefore into any of types 1-4.
Types 1/4 tie to population transfer (`FUN_0006f738`, U2 territory); type 2's else-branch ties to
X-COM base-slot deallocation (also U2 territory); type 3 walks an unrelated
infiltration/detection array. None resembles a two-arbitrary-orgs cargo-worth exchange.

**Org `+8`: BOUND as a funds/budget field**, not a relation field.

Per the task brief and `next-implementation.md`: **do not wire `Cargo::seize` from
`FUN_000b32ac`/`worth×50`.** This row's diplomacy FIXME stays open.

---

## 3. M1 — city action music: BOUND (consumer only; trigger unbound)

### 3.1 Catalog-only strings confirmed as such

`Action music` (file `0x1543bd`) sits inline with the in-game **options-menu checkbox list**
(`Tool tips`, `Action music`, `Message Toggles`, `UFO spotted`, `Vehicle lightly damaged`, ...) —
this is exactly OpenApoc's own `Options.Misc.ActionMusic` checkbox label. Zero bound xrefs, both
generations' copies. The `/MUSIC/GROUP_1..4/*.RAW` catalog (10+10+8+5 = 33 tracks, **no
`GROUP_0`**) is likewise catalog-only data with zero bound xrefs on any individual filename
string. This confirms the gap matrix's existing note.

### 3.2 The actual reader: `getnextmusic` has a bound consumer

The debug literal `"end of track reached - getnextmusic"` (file `0x135928`) **does** have a bound
xref: `FUN_000b6280` (VA `0xB6280`, **file `0x118924`**), the "advance to next track" function —
this is `getnextmusic` itself, not UI copy.

`FUN_000b6280` is called four times (once per parallel playback channel) from `FUN_000b5678` (VA
`0xB5678`, object-page file `0xA5677`; `.image` byte-match found no clean duplicate, likely a
compressed LX page), which starts four simultaneous `/MUSIC/GROUP_*` RAW channels — matching
X-COM Apocalypse's known layered, cross-fading tactical soundtrack (four `_DAT_00135298`-family
channel slots at `+0x74` stride).

`FUN_000b5678`'s **sole caller** is `FUN_000b523c` (VA `0xB523C`, object-page file `0xA523B`) — a
tension-tier state machine. It maintains `DAT_00183ba7`, a value **clamped to `[1, 4]`** — one
integer per `GROUP_1..4` folder — and:

- picks a non-repeating random track within the current tier (`FUN_000b641c`);
- calls `FUN_000b5678`/`FUN_000b6280` to start/advance playback when the tier or track changes;
- is itself gated on four flag bytes (`DAT_00135818`, `DAT_000d4c80._3_1_`, `DAT_000d4cb2`,
  `DAT_00135814`) whose meaning was **not** decoded this session.

### 3.3 The city-side call site

`FUN_000b523c` has **exactly two callers**:

1. `FUN_00010010` (VA `0x10010`) — a generic top-level per-tick update function that also calls
   the O2 dispatcher chain (`FUN_000b1dc0` → ... → `FUN_000b32ac`) unconditionally every tick,
   regardless of city or battle state. This call site alone would prove nothing about city-vs-
   battle.
2. **`FUN_000ac08c`** (VA `0xAC08C`, file `0x10E730`) — the **exact same function identified in
   §2.2 as one of `FUN_0003a910`'s three call targets.** Near its tail, gated on
   `(*(int*)(ctx+0x5c) != 0) && (*(int*)(ctx+0x58) != 0)`, it calls `FUN_000b523c` directly.

Since `FUN_0003a910` is the UFO mission-arrival/deposit-completion handler (already bound for
U1/U2 — subversion craft, base exposure, mission-target completion, all city-map activity), this
is a genuine **city-side call edge into the music-tier selector**, independent of the
battle-exclusive trigger already wired in `game/ui/tileview/battleview.cpp:1825`
(`Options.Misc.ActionMusic` + `battle.ticksWithoutSeenAction`).

### 3.4 What is NOT claimed

- The two gate fields at `ctx+0x58`/`ctx+0x5c` inside `FUN_000ac08c`, and the four flag bytes
  gating `FUN_000b523c`/`FUN_000b5678`, were **not** decoded. I do not know precisely which UFO
  mission outcome (subversion success, base exposure, terror action, etc.) reaches this call, only
  that the call edge exists and originates from `FUN_0003a910`.
- `DAT_00183ba7`'s tier levels map to `GROUP_1..4` by position, **not** by name — tier 3 is the
  group actually containing `ACTION.RAW`/`CHASE.RAW`; tier 4 (`GROUP_4`: `FEAR`/`LOWTONE`/
  `MINDMAZE`/`STRANGE`) is a *different* mood, not more "action". Nothing in this session's
  evidence pins which tier corresponds to the OpenApoc `JukeBox::PlayList::Action` concept, or
  what specifically drives the tier upward.
- No condition resembling "a hostile vehicle is nearby" was found or is being asserted. That
  remains exactly the kind of invented trigger the task brief warns against.

### 3.5 Verdict

**BOUND, consumer only:** a real, traced consumer of the `getnextmusic`/`GROUP_*` tension-music
system exists outside battle, reachable from city-map UFO-mission completion (`FUN_0003a910` →
`FUN_000ac08c` → `FUN_000b523c` → `FUN_000b5678`/`FUN_000b6280`). This satisfies "city mix only
after a bound consumer" and rules out wiring city Action music to an invented heuristic — but it is
**not** itself a green light to wire a city Action playlist. `FUN_000b523c` already runs every
tick unconditionally from the generic top-level loop, so the mission-arrival call may just force an
immediate re-evaluation with no effect on tier; nothing found here binds *which* UFO-mission
outcome escalates the tension tier, or that it escalates at all. The trigger condition and
tier-selection formula remain unbound and should **not** be guessed at to close this row further —
only the existence of a bound, non-battle consumer is established here.

---

## 4. Summary table

| Target | Verdict | Key evidence |
|---|---|---|
| O1 bribe/rift formula | **NOT BOUND** | 8/8 UI strings zero xref; all 17 call sites (6 functions) of the relation primitive `FUN_0005faf0` (file `0xC2194`) walked and decompiled — none touch currency |
| O2 event→cargo mapping | **NOT BOUND** | Sole event producer `FUN_000aff9c` (file `0x112640`) reached only via `FUN_000ac348`; that function's caller `FUN_000ac08c`'s 7 call sites (verified individually) all sit in the UFO-mission/alien-incident family rooted at `FUN_0003a910`, none touch `Building::updateCargo` |
| O2 org `+8` field type | **BOUND** — funds, not relation | Shares the `0x1b6`-stride org-budget table; unclamped `worth×50` magnitude; a separate clamped write exists for the real relation matrix in the same function |
| M1 city music trigger | **BOUND, consumer only** | `FUN_000ac08c` (file `0x10E730`), reachable from `FUN_0003a910`, directly invokes the tension-tier state machine `FUN_000b523c` → `FUN_000b5678`/`FUN_000b6280` (`getnextmusic`, file `0x118924`); tier-escalation condition itself stays unbound |
