# B5 · Entropy Enzyme / F1 · Fire remainder / K1 · Personal Cloaking Field

**F1 hazard RNG — BOUND:** `FUN_0001eee8` (generic bounded RNG) and `FUN_0007b0d0` (fire-specific
neighbour-pick + resistance-gated spread roll) are both fully decompiled, with the exact neighbour
offset table recovered. `HAZARD_SPREAD_CHANCE 10` can be deleted — the real mechanism is
`RNG(0..10) + inherited baseline` compared against a per-map-part resistance byte, never a flat
percent — but the resistance table's own values were not decoded this session (see §2.4).

**B5 Entropy Enzyme — PARTIAL / hypothesis CONFIRMED at the structural level, type-id NOT BOUND:**
the tile overlay byte genuinely carries (at least) three parallel hazard types sharing one
structure — a decode triplet, an encode triplet, and a shared placement/spread engine
(`FUN_0007ae78`, the generalized twin of `FUN_0007b0d0`) — exactly the hypothesis the task said to
test first. Fire is confirmed as type 2. Which of type 1 / type 3 is Enzyme (vs. Gas/Smoke) was
**not** recoverable this session: the dispatch variable that selects a type before calling the
placement routine has zero static xrefs (see §3.4), so the assignment cannot be traced back to a
damage-type catalog index. Do not guess 1 vs. 3.

**K1 Personal Cloaking Field — NOT BOUND.** Every printable copy of the string (both UI-pool and
internal-asset-table copies) has zero xrefs. The agent_general_data catalog's runtime VA was
recovered, but no reader of its `type` field (row+10, values 0..19 checked) was found. The
"Mind Shield @ 0x9B780" address cited elsewhere in this tree does not resolve to a function start
in the bound project — it lands mid-function inside `FUN_0009b058` — confirming that address comes
from a different, non-bound disassembly pass and should not be reused as a cloak search anchor
either.

**Unit fire/hazard intensity — PARTIAL.** A generic per-damage-type unit damage/resistance function
(`FUN_00060540`) was identified with high confidence (its resistance-table stride matches
`damage_modifier_data` exactly, byte-for-byte). It is called both from the AOE blast-placement
routine and from the per-tile hazard-tick dispatcher, and a companion function (`FUN_0007bcb8`)
reads a per-unit byte at offset `+0x82` written during blast placement. This is a strong lead, not
a bound constant: the exact semantics of unit offsets `+0x45`, `+0x80`, `+0x82` were not
conclusively pinned down.

---

## 0. Binary and environment

`OpenApoc-og-research/canonical/TACP.EXE`, CRC32 `0xfebbe39e` — the extractor-canonical **non-4**
ISO build. Ghidra project `ghidra_projects/OpenApocOG_TACP.rep` (pre-imported LX-loader LE,
`-processor x86:LE:32:default -cspec gcc`), queried this session with `-noanalysis -process
TACP.EXE` (re-opens the already-analysed program; does not re-run analysis).

**Every address below was obtained this session against the non-4 build only.** The `4` build
(`TACP4.EXE`, `-0x2200` documented slide) was **not** re-tested; do not apply the code delta or the
data delta derived below to it without independent confirmation — the parity guide is explicit that
`crew_ufo_downed` is a known exception to "the" slide, and this session found **two different
deltas for two different memory classes** in the non-4 build alone (below), which is itself a
demonstration of why no delta should be assumed global.

**Two confirmed, distinct deltas (non-4 build only):**

| Memory class | Delta (file offset − VA) | Confirmed via |
|---|---|---|
| `.object1` code | `+0x5AAA4` | Ghidra's own raw-byte signature match (`Memory.findBytes` against the `.image` overlay), independently unique for 11 different functions this session (table below) |
| `.object2` data | `+0x4FAA4` | Confirmed for two tables: `fire_hazard_power_table` (already bound, `labels/tacp_rebase.csv`) and `agent_general_data` (recovered this session, §4.1) |

The two deltas differ by exactly `0xB000`, consistent with code and data living in separate LE
objects with independent load bases — expected, not a bug. **Do not extend either delta to a third
table without re-confirming**, per the parity guide's rule.

Signature-confirmed `.object1` file offsets this session (via `QueryFunctions.java`'s
`boundFileOffset`, which raw-byte-matches the function's opening bytes uniquely inside `.image`):

| Function | VA | File offset (non-4) | Confirmation |
|---|---|---|---|
| `FUN_0001eee8` | `0x1EEE8` | `0x7998C` | signature-unique |
| `FUN_0007b0d0` | `0x7B0D0` | `0xD5B74` | signature-unique |
| `FUN_0007aa8c` | `0x7AA8C` | `0xD5530` | signature-unique |
| `FUN_0007adac` | `0x7ADAC` | `0xD5850` | signature-unique |
| `FUN_0007adb0` | `0x7ADB0` | `0xD5854` | signature-unique |
| `FUN_0007add0` | `0x7ADD0` | `0xD5874` | signature-unique |
| `FUN_0007adc8` | `0x7ADC8` | `0xD586C` | signature-unique |
| `FUN_0007ae78` | `0x7AE78` | `0xD591C` | signature-unique |
| `FUN_0007b610` | `0x7B610` | `0xD60B4` | signature-unique |
| `FUN_0007d67c` | `0x7D67C` | `0xD8120` | signature-unique |
| `FUN_0009b058` | `0x9B058` | `0xF5AFC` | signature-unique |

All eleven match `VA + 0x5AAA4` exactly. The following returned **no unique signature match**
(`bound_file = -1`, byte pattern not unique enough, or `-2`, duplicate match) — their file offsets
below are **derived** from the confirmed delta, not independently signature-confirmed, and are
labelled as such everywhere they appear in this document:

| Function | VA | File offset (non-4, **derived**) |
|---|---|---|
| `FUN_0007c1a4` | `0x7C1A4` | `0xD6C48` |
| `FUN_0007b7f8` | `0x7B7F8` | `0xD629C` |
| `FUN_0007ade8` | `0x7ADE8` | `0xD588C` |
| `FUN_0007b2b4` | `0x7B2B4` | `0xD5D58` (duplicate signature) |
| `FUN_0007b348` | `0x7B348` | `0xD5DEC` (duplicate signature) |
| `FUN_0001ea38` | `0x1EA38` | `0x794DC` |
| `FUN_0001ea70` | `0x1EA70` | `0x79514` |
| `FUN_0001eb48` | `0x1EB48` | `0x795EC` |
| `FUN_00021284` | `0x21284` | `0x7BD28` |
| `FUN_0007bcb8` | `0x7BCB8` | `0xD675C` |
| `FUN_00060540` | `0x60540` | `0xBAFE4` |

Already-bound entry points, cited by the task and reused as starting points (unchanged, not
re-derived): `FUN_0007c110` @ VA `0x7C110` / file `0xD6BB4`, `FUN_0007ad94` @ VA `0x7AD94` / file
`0xD5838`, `FUN_0007ae18` @ VA `0x7AE18` / file `0xD58BC`, `FUN_0007b3dc` @ VA `0x7B3DC` / file
`0xD5E80`.

---

## 1. Method note: two string-region classes, both empty for these three targets

A concurrent peer session (working B1, cover-tile scoring, against the same project) flagged that
TACP's printable strings live in **two structurally different regions**, and that a zero-xref
result in the packed UI pool proves nothing (indexed strings in a packed, variable-length pool have
no reason to carry a direct pointer xref). That claim was checked independently against this
session's own pre-dumped `export/strings/TACP_strings.txt` before relying on it:

- **Packed UI/message pool** (`0x2DFE00`–`0x2E2FFF`-ish): variable stride, holds display names and
  log messages. `Entropy Enzyme` `0x2E2690`, `Personal Cloaking Field` `0x2DEA7A` and (lowercase
  `field`) `0x2DF5F2` live here.
- **Fixed-stride internal-name table** (`0x2F2000`–`0x2F3400`+): confirmed independently this
  session — consecutive entries are **exactly** `0x2E` (46) bytes apart end-to-end across 70+ rows
  (`APGrenade` `0x2F2590` → `StunGrenade` `0x2F25BE` → `SmokeGrenade` `0x2F25EC` → … → `FireGrenade`
  `0x2F2FFC` → `legs1` `0x2F302A`, every gap exactly `0x2E`). This is the RAW/internal asset-name
  catalog, one row per equipment/ammo id, and it plausibly indexes 1:1 with `agent_general_data` /
  `agent_weapon_data` row order. It carries `VisibilityDisruptionField` `0x2F2EBA` (the internal
  name for the cloak item — thematically unambiguous) and `EntropyPod` `0x2F2FCE` (the internal
  name for the enzyme grenade/ammo item, distinct from the UI string `Entropy Enzyme`).

**Both regions were checked for all relevant targets. Both are empty.** `Entropy Enzyme` /
`Personal Cloaking Field` (packed pool, both copies each) — zero xrefs, as previously documented.
`VisibilityDisruptionField`, `EntropyPod`, `FireGrenade`, `EnergyPod` (fixed-stride asset table) —
**also zero xrefs each**, contradicting the general claim that fixed-stride-table entries are
reliably referenced (that may hold for other rows in that table, e.g. `senator`, per prior work in
`compare-report.html`, but does not hold for these four). This is a genuine, doubly-confirmed
negative for string-anchored search on these specific items, not a tooling artifact — the method
itself (packed pool vs. fixed table, `getReferencesTo` on both) is sound and was applied correctly;
these four items simply are not reached via a static string pointer anywhere in the binary. All
downstream work in this document proceeds from structural/numeric entry points instead, per the
task's own instruction to test the "type on a shared structure" hypothesis before string-hunting.

---

## 2. F1 · Generic hazard placement/spread RNG — BOUND

### 2.1 `FUN_0001eee8` — the generic bounded RNG

VA `0x1EEE8` / file `0x7998C` (signature-confirmed). Signature: `int __regparm3
FUN_0001eee8(short max, ...)` returning a value **uniformly distributed on `[0, max]` inclusive**
(`% (max + 1)`):

```c
undefined8 __regparm3 FUN_0001eee8(short param_1,undefined4 param_2)
{
  if (param_1 < 0) param_1 = 0;
  if ((DAT_000e6d66 == '\0') || (DAT_000e6d68 != '\0')) {
    uVar2 = *(ushort *)(&DAT_001b2d70 + (uint)DAT_001b7baa * 2);
    DAT_001b7baa = (ushort)(DAT_001b7baa + 1) % 0x271d;   // cursor mod 10013
  } else {
    // deterministic/debug branch: FUN_000c522e / FUN_000c4cd3 (CRT-style rand pair)
  }
  return CONCAT44(param_2,(int)(uint)uVar2 % (param_1 + 1));
}
```

Primary path: a persistent 10,013-entry (`0x271D`) `ushort` lookup table (`DAT_001B2D70`), advanced
by a cursor (`DAT_001B7BAA`) that increments mod `0x271D` on every call — i.e. a precomputed
pseudo-random sequence table, not a runtime LCG. A secondary branch (gated by two flag bytes,
`DAT_000E6D66`/`DAT_000E6D68`) falls back to a CRT `rand()`-style pair (`FUN_000C522E` /
`FUN_000C4CD3`) — this looks like a determinism/replay toggle, not examined further (out of scope).

This is the **single shared RNG entry point** for essentially all hazard-related randomness bound
this session — every roll described below is a call to this one function with a different bound.

### 2.2 `FUN_0007b0d0` — fire-specific neighbour pick + spread roll

VA `0x7B0D0` / file `0xD5B74` (signature-confirmed). Called from `FUN_0007b3dc` (the already-bound
overlay stage/extinction function) and `FUN_0007bd8c`. Full mechanism, decompiled and cross-checked
against the raw listing:

```
CALL FUN_0001eee8(10)      ; cVar3 = RNG(0..10) inclusive  → threshold, + inherited CL baseline
CALL FUN_0001eee8(4)       ; uVar9 = RNG(0..4) inclusive   → 5-way neighbour pick
iVar6 = uVar9*3 + 1                                        ; index into the neighbour table
```

The neighbour table is read as three "parallel" arrays (`DAT_00293068`, `DAT_0029306C`,
`DAT_00293070`) at `[iVar6]`, `[iVar6]`, `[iVar6]` respectively — which turn out, per the byte dump
in §2.3, to be **one contiguous `int32[]` table** read as three consecutive elements
`table[iVar6]`, `table[iVar6+1]`, `table[iVar6+2]` (an (X, Y, Z) delta triple), not three separate
tables — Ghidra's `DAT_` boundaries here are an artifact of undefined data, not real table breaks
(this was flagged as a live ambiguity and resolved by the dump, see §2.3).

Given the destination tile, the code:

1. Reads the destination tile's overlay byte. If it is **already** a type-2 (fire) overlay, skip
   (no re-ignition of an already-burning tile).
2. Otherwise calls `FUN_0007aa8c(y, 2 /* hazard type = fire, hardcoded */, ..., threshold)` — a
   per-map-part **resistance** lookup (§2.4) — and compares its return against the RNG(10)+baseline
   threshold from above (`JNC` = jump-away/no-spread when resistance ≥ threshold, i.e. spread
   succeeds only when the resistance value is **less than** the rolled threshold).
3. On success, two more per-map-part **veto** lookups run (`FUN_0001ea70`, `FUN_0001eb48` — both
   index a per-map-part-type byte table, stride `0x56` = 86, at struct offset `+8`/`+0xB`, sentinel
   `0xFF` = "no data"; the smaller of the two non-sentinel values is taken as a further gate).
4. If nothing vetoes, `FUN_00021284` queues a visibility/redraw event, and
   `FUN_0007adac(0) | 0x80` writes the fresh type-2 overlay byte (`OR 0x80` sets the top type-bit;
   `0x80 >> 6 == 2`, i.e. this **is** how a stage-0 fire ignition is encoded — matches
   `BattleHazard::FIRE_OVERLAY_TYPE = 0x80` already in `battlehazard.h`).

**This is the exact mechanism the task asked for.** It is **not** `HAZARD_SPREAD_CHANCE`-shaped: it
is `RNG(0..10) + an inherited per-call baseline` compared against a genuine per-terrain resistance
value, gated further by two per-map-part veto lookups. `HAZARD_SPREAD_CHANCE 10` should be deleted
outright, not retuned — there is no single flat percentage anywhere in this path. The "10" in
`FUN_0001eee8(10)` is the RNG *span*, not a percent-chance constant, and coincidentally sharing the
digit with the invented `HAZARD_SPREAD_CHANCE = 10` is exactly the trap the parity guide warns
about; **do not read this as vindicating the old constant.**

**Not bound in this pass**: the numeric contents of the two resistance tables
`FUN_0007aa8c` reads from (`DAT_000F9CDB`, `DAT_000FF2DB`, stride `0x56`) — the mechanism (compare
roll against per-terrain resistance) is bound; the resistance *values themselves* were not decoded.
A future session wiring this up needs those table contents, not just the formula shape.

### 2.3 Neighbour table dump — the exact ordering

Raw dump, VA `0x293050`–`0x2930F0` (`.object2`), read as `int32[]` starting at index 0 =
`0x293050`. The relevant range for `FUN_0007b0d0` starts at index 5 (`0x293068`):

| index | VA | value | used by |
|---|---|---|---|
| 5 (X₀) | `0x293068` | `1` | fire, outcome 0 |
| 6 (Y₀) | `0x29306C` | `0` | fire, outcome 0 |
| 7 (Z₀) | `0x293070` | `0` | fire, outcome 0 |
| 8 (X₁) | `0x293074` | `0` | fire, outcome 1 |
| 9 (Y₁) | `0x293078` | `0` | fire, outcome 1 |
| 10 (Z₁) | `0x29307C` | `1` | fire, outcome 1 |
| 11 (X₂) | `0x293080` | `0` | fire, outcome 2 |
| 12 (Y₂) | `0x293084` | `-1` | fire, outcome 2 |
| 13 (Z₂) | `0x293088` | `0` | fire, outcome 2 |
| 14 (X₃) | `0x29308C` | `0` | fire, outcome 3 |
| 15 (Y₃) | `0x293090` | `0` | fire, outcome 3 |
| 16 (Z₃) | `0x293094` | `-1` | fire, outcome 3 |
| 17 (X₄) | `0x293098` | `0` | fire, outcome 4 |
| 18 (Y₄) | `0x29309C` | `0` | fire, outcome 4 |
| 19 (Z₄) | `0x2930A0` | `0` | fire, outcome 4 |

Re-deriving the field roles from the consuming code (§2.2): the first element of each triple is
added **directly** as a flat X-offset; the second is used as an **index** into a Y-row-stride table
(`DAT_001C6F1C`); the third is used as an index into a Z-plane-stride table (`DAT_001C6F18`).
Reading the table this way (the index arithmetic in §2.2 is `iVar6 = uVar9*3+1`, i.e. row *N*
starts at flat index `3N+1`, not `3N` — index 0..2, `(1, 0, 0)` at row 0's *predecessor* slot, is
unused padding, consistent with every fire row starting one element later):

| outcome (`uVar9`) | (X, Y, Z) | direction |
|---|---|---|
| 0 | `(1, 0, 0)` | **East** (+X) |
| 1 | `(0, 1, 0)` | **South** (+Y) |
| 2 | `(-1, 0, 0)` | **West** (−X) |
| 3 | `(0, -1, 0)` | **North** (−Y) |
| 4 | `(0, 0, 1)` | **Up** (+Z) |

Fire spreads to one of these five candidates, chosen uniformly at random (`RNG(0,4)`), gated by the
resistance roll in §2.2. **Fire never spreads downward** — there is no `(0,0,-1)` candidate in its
5-outcome set.

### 2.4 `FUN_0007ae78` — the generalized (parameterized-type) placement/spread engine

VA `0x7AE78` / file `0xD591C` (signature-confirmed). Structurally identical to `FUN_0007b0d0` but
parameterized by hazard `type` (a `byte` argument) instead of hardcoding `2`, and drawing **one more
outcome** from the RNG: `FUN_0001eee8(5)` (6 outcomes, `0..5`) instead of `FUN_0001eee8(4)`. It
reads the **same contiguous neighbour table**, continuing past fire's five entries:

| outcome | flat index (X) | (X, Y, Z) | direction |
|---|---|---|---|
| 0 | 16 (`0x2930A8`) | `(1, 0, 0)` | East |
| 1 | 19 (`0x2930B4`) | `(0, 1, 0)` | South |
| 2 | 22 (`0x2930C0`) | `(-1, 0, 0)` | West |
| 3 | 25 (`0x2930CC`) | `(0, -1, 0)` | North |
| 4 | 28 (`0x2930D8`) | `(0, 0, 1)` | Up |
| 5 | 31 (`0x2930E4`) | `(0, 0, -1)` | **Down** |

(Verified against the same raw dump, `0x2930A8`→`1`, `0x2930AC`→`0`, `0x2930B0`→`0`; `0x2930B4`→`0`,
`0x2930B8`→`1`, `0x2930BC`→`0`; `0x2930C0`→`-1`, `0x2930C4`→`0`, `0x2930C8`→`0`; `0x2930CC`→`0`,
`0x2930D0`→`-1`, `0x2930D4`→`0`; `0x2930D8`→`0`, `0x2930DC`→`0`, `0x2930E0`→`1`; `0x2930E4`→`0`,
`0x2930E8`→`0`, `0x2930EC`→`-1`.)

Confirms the earlier ambiguity cleanly: this is **one 32-entry `int32[]` table**, not two
unrelated ones — the `DAT_002930A4` name Ghidra assigned to the "generic" base is simply the byte
address 15 elements into the same array `DAT_00293068` starts. **Fire gets the first five rows
(no Down); the generic engine (any type) gets all six, adding Down.** This is a clean, internally
consistent, fully-verified result — not an inference from table size or "duration".

The rest of `FUN_0007ae78`'s body mirrors `FUN_0007b0d0`'s resistance-roll-then-write structure,
except the final overlay write dispatches through **one of three type-specific encoders** based on
the type argument, rather than hardcoding `FUN_0007adac`:

| type | encoder | file offset | mask |
|---|---|---|---|
| 1 | `FUN_0007adc8` | `0xD586C` (confirmed) | `\| 0x40` |
| 2 (fire) | `FUN_0007adac` | `0xD5850` (confirmed) | `\| 0x80` |
| 3 | `FUN_0007ade8` | `0xD588C` (derived) | `\| 0xC0` |

Each encoder is a 2-instruction function: `if (value != 0) value \|= mask; else 0;` — trivially
confirming the top-2-bit type field (`>>6`: `01`=1, `10`=2, `11`=3) and that **type 0 is the
"no hazard" state**, not a fourth real hazard type (consistent with the 4-way jump table in §3.1
having an actual handler for case 0, `FUN_0007b228`, which was not decompiled this session — it is
reached only when the overlay byte is nonzero but its top bits are 0, an edge case not investigated
further).

---

## 3. B5 · Entropy Enzyme — hypothesis testing

**The task's instruction was to check whether Enzyme is a TYPE on the same hazard structure as fire
before hunting a separate subsystem. It is.** Confirmed at the structural level with real,
decompiled functions; the specific type-id (1 vs. 3) and the armour-damage formula are not bound.

### 3.1 The four-way dispatch — `FUN_0007b610`

VA `0x7B610` / file `0xD60B4` (signature-confirmed). Called directly from the already-bound
real-time fire scheduler `FUN_0007b7f8` (derived file `0xD629C`) for every occupied overlay tile in
a scheduler row. Contains a genuine jump table on the overlay byte's top 2 bits:

```c
switch((int)(uint)*pbVar6 >> 6) {
  case 0: FUN_0007b228(...); break;   // not decompiled this session
  case 1: FUN_0007b2b4(...); break;   // type-1 stage advance
  case 2: FUN_0007b3dc(...); break;   // type-2 (fire) stage advance — ALREADY BOUND
  case 3: FUN_0007b348(...); break;   // type-3 stage advance
}
```

Jump table itself is at file-listing address `0x7B5F0` (`JMP dword ptr CS:[EAX*4 + 0x7B5F0]`).
**This is a real, compiled 4-entry jump table, not an inferred pattern** — cases 1 and 3 are full
peers of the already-bound fire case, called from the same real-time scheduler loop, at the same
call depth, with the same signature shape.

### 3.2 The decode/encode triplets

Decode (byte → stage, 0 if wrong type) — all three read the same top-2-bit field:

| type | function | file offset | body |
|---|---|---|---|
| 1 | `FUN_0007adb0` | `0xD5854` (confirmed) | `if (b>>6==1) return b&0x3F; else 0;` |
| 2 (fire) | `FUN_0007ad94` | `0xD5838` (already bound) | same shape, `==2` |
| 3 | `FUN_0007add0` | `0xD5874` (confirmed) | `if (b>>6==3) return b&0x3F; else 0;` |

Encode (stage → byte, `OR` the type bits) — table in §2.4.

### 3.3 The stage-advance functions — `FUN_0007b2b4` (type 1) / `FUN_0007b348` (type 3)

VA `0x7B2B4` / file `0xD5D58` (derived, duplicate signature) and VA `0x7B348` / file `0xD5DEC`
(derived, duplicate signature) respectively. Both are **structurally identical**, differing only in
the type constant (1 vs. 3) and the mask (`0x40` vs. `0xC0`):

```c
void FUN_0007b2b4(...) {
  bVar1 = FUN_0007ae78(1, ...);            // generic engine, type=1: advance stage + maybe spread
  bVar2 = (bVar1 != 0) ? (bVar1 | 0x40) : 0;
  *out = bVar2;
  if ((bVar1 == 0) || (bVar1>>3 != priorValue>>3))
    FUN_00021284(...);                      // visibility/redraw event on a significant stage change
}
```

This is the **type-1/type-3 equivalent** of the already-bound `FUN_0007b3dc` (fire's stage
advance), except where fire calls two separate hardcoded functions (`FUN_0007ae18` for the 27-byte
power lookup, `FUN_0007b0d0` for spread), types 1 and 3 call **one combined, parameterized function**
— `FUN_0007ae78` (§2.4) — that does both jobs at once. This asymmetry (fire hand-specialized into
two functions; types 1/3 sharing one generic function) is itself evidence fire was implemented
first/specially, and 1/3 are the generic hazard family the task's hypothesis predicted.

### 3.4 What could not be bound: the type discriminator and the armour reader

**Placement dispatch — `FUN_0007d67c`.** VA `0x7D67C` / file `0xD8120` (signature-confirmed), a
large (3,972-byte) function that `switch`es on a global, `DAT_003009A0`, with (at least) cases
0–4, and calls **all three** overlay encoders (`FUN_0007adc8` type 1, `FUN_0007adac` type 2,
`FUN_0007ade8` type 3) from different case bodies, over a 3D AOE-blast volume. This is the
**initial hazard-placement** routine (a grenade/explosion applying its blast footprint), as
opposed to `FUN_0007ae78`/the b2b4/b3dc/b348 family which is the **per-tick growth** of an
already-placed hazard.

**The type discriminator was not traceable.** A full-executable scan for any instruction with
`0x3009A0` as a literal operand returned **zero hits** (`QueryDataRange`-style scan, `Scalar`
operand match across every instruction in `.object1`). Since the decompiler nonetheless renders
`switch(DAT_003009A0)` inside `FUN_0007d67c`, the most likely explanation is that this is **not** a
true fixed-address global — it is very likely a mis-resolved incoming register parameter (this
function's decompile is full of `unaff_EBX`/`unaff_DI`-style unresolved-parameter artifacts
throughout, consistent with Ghidra's decompiler failing to fully model whatever non-standard
calling convention `FUN_0007d67c`'s caller uses). Whatever it really is, **no writer was found**,
so the value that selects "this explosion is a type-1 vs. type-3 blast" cannot be traced back to a
damage-type catalog index from within this session's evidence. Per the absolute rule, **this stays
unbound rather than guessed** — do not assign Enzyme to type 1 or type 3 on the basis of table
size, RAW sound path proximity, or any other circumstantial signal.

**The armour reader was not found either.** `case 2`'s body (inside `FUN_0007d67c`, the fire case)
does contain a real unit-iteration loop over a `0x24E`-byte-stride array (`0x109ECE`–`0x11B360`,
plausibly the unit table) that writes per-unit fields at `+0x41`/`+0x42`/`+0x43` when a blast voxel
exceeds a threshold (`0x18`) — but this is **inside the fire-specific case**, and nothing
equivalent was located for cases 1/3 in the portion of the function actually read this session (it
is large; only the switch body and case 2/4 were read in full, not cases exhaustively cross-checked
for a parallel unit loop). No read of an item/unit **armour** field was identified anywhere in this
pass — see §5 for the closest candidates, which are damage-application, not armour-specific.

### B5 verdict

**PARTIAL.** Structure confirmed with real, decompiled, cross-referenced functions: the tile
overlay's 2-bit type field supports three live hazard types (1, 2=fire, 3), sharing one decode
family, one encode family, and (for 1/3) one generic placement/spread engine
(`FUN_0007ae78`) that is a strict generalization of fire's spread function
(`FUN_0007b0d0`) — six neighbour candidates including Down, vs. fire's five. **Not bound**: which
of type 1 / type 3 is Enzyme (the dispatch variable has no traceable static writer), the armour
damage formula, and `TICKS_PER_ENZYME_EFFECT`. Do not replace `TICKS_PER_ENZYME_EFFECT =
TICKS_PER_SECOND/9` with anything from this session — no tick cadence was recovered for either
type 1 or type 3. `HAZARD_SPREAD_CHANCE` should still be deleted per F1's finding (§2.2) — that
part applies to the shared engine as much as to fire specifically, since `FUN_0007ae78` uses the
identical `RNG(0..10)+baseline vs. resistance` mechanism, just parameterized.

---

## 4. K1 · Personal Cloaking Field — NOT BOUND

### 4.1 String search — negative, both regions (§1)

Both `Personal Cloaking Field` copies (packed pool `0x2DEA7A`/`0x2DF5F2`) and the internal asset
name `VisibilityDisruptionField` (fixed-stride table, `0x2F2EBA`) have **zero xrefs**. This matches
and extends the parity guide's existing note that this string has empty bound xrefs.

### 4.2 Structural attempt — `agent_general_data` VA recovery and row search

Per the task's instruction to "find readers of equipment type `0x0a` on the unit-update path,"
`agent_general_data`'s runtime VA was recovered from its known bound-file offset (non-4,
`0x302914`, `tools/extractors/common/aequipment.h:131`) using the reverse of the
`boundFileOffset` byte-signature technique: read 24 bytes at `.image` file offset `0x302914`
(`FF FF 00 00 00 00 FF FF 00 00 00 00 05 10 00 00 00 00 01 00 01 00 00 00`), then search the whole
program for the same byte sequence outside `.image`. **Exactly one match**, at VA `0x2B2E70`,
block `.object2`. File-offset check: `0x302914 − 0x2B2E70 = 0x4FAA4` — the **same** data delta
confirmed independently for `fire_hazard_power_table` (§0), a second independent confirmation of
that delta.

`AgentGeneralData` is a 12-byte struct (`tools/extractors/common/aequipment.h:118`) with `type` at
struct offset `+10`. Direct Ghidra xrefs to the table base (`0x2B2E70`) and to the `type` field
address of the first 20 rows (`0x2B2E70 + row*12 + 10`, rows 0..19) were checked: **zero hits at
every address checked.** No instruction anywhere in `.object1`/`.object2` holds a Ghidra-recognised
reference to the catalog's `type` field for any of the first 20 rows (which covers row `0x0A` if
row order matches `AGENT_GENERAL_TYPE_*` ordinal order — not confirmed, since there was no reader
to inspect).

This is consistent with (and was anticipated as likely by) the standard pattern seen everywhere
else in this catalog family: a **catalog row gets copied into a per-item runtime instance at spawn
time**, and gameplay code reads the runtime instance's copy of `type`, not the catalog row
directly. Finding that copy site — and then the accumulator/comparison it feeds — would require
either (a) locating the item-spawn/equip constructor (a much larger search, not attempted this
session) or (b) a numeric scan for `CMP reg, 0xA` clustered near other equipment-dispatch code,
which was judged too unconstrained to produce a reliable positive (TACP's code has many unrelated
`CMP reg, 0xA` instructions; without a narrower anchor this would only produce noise, the exact
failure mode the parity guide warns against).

### 4.3 The "flat disassembly" trap, independently reproduced

`compare-report.html` already flags that `FUN_0009B780` (cited elsewhere as the Mind Shield
`useItem` effect, type `0x05`) is **absent from the bound TACP export**. This was independently
reproduced this session: querying VA `0x9B780` against the bound `OpenApocOG_TACP` project does
**not** return a function starting at that address — it resolves to `FUN_0009B058` (VA `0x9B058`,
file `0xF5AFC`, signature-confirmed), i.e. `0x9B780` is **660 bytes inside** a different function's
body in this analysis, not a function entry point. Whatever produced the `0x9B780`/`0xCA5D8`/
`0x8CCA8` addresses cited for Mind Shield/Teleporter/DisruptorShield came from a **different,
non-bound disassembly pass**, and none of those addresses should be reused as search anchors for
cloak (or anything else) in the bound project — they will not resolve to the same code.

### K1 verdict

**NOT BOUND.** No consumer of `Personal Cloaking Field` (either string region) or of
`agent_general_data`'s `type` field (table base or first 20 rows) was found via structural entry.
`CLOAK_TICKS_REQUIRED_UNIT = TICKS_PER_SECOND * 2` stays exactly as documented — a forum-sourced
constant, not a binary-recovered one — and the comment attributing it to "Yatoka Shimaoka on
forums" should **not** be deleted, since nothing here replaces it.

**One behavioural note worth recording regardless of the RE result**: the parity guide's text
("OpenApoc currently breaks it only on unequip") is **stale relative to the current tree**.
`BattleUnit::updateAttacking` already zeroes `cloakTicksAccumulated` immediately after
`firingWeapon->fire(...)` (`game/state/battle/battleunit.cpp:3675`), in addition to the unequip
reset in `updateCloak` (`battleunit.cpp:2121`). So OpenApoc's code **already** breaks cloak on
firing, not only on unequip — the guide's stated gap on this specific edge does not exist in the
current tree. (Not verified against the binary — this is a statement about the current C++, not a
new binding — but it means K1's "capture what breaks it" sub-ask is already satisfied by existing
code and does not block anything.)

---

## 5. Unit fire/hazard intensity — PARTIAL (bonus finding, not in the original numbered target list order)

While reading `FUN_0007d67c`'s AOE-blast case bodies (§3.4) looking for an armour reader, two
functions surfaced that are strong candidates for "a per-unit fire/hazard value reader distinct
from `TICKS_PER_FIRE_EFFECT`" (the task's target #4), but neither was pinned down to an exact,
citable formula.

### 5.1 `FUN_00060540` — generic per-damage-type unit damage/resistance application

VA `0x60540` / file `0xBAFE4` (derived). Signature `void __regparm3 FUN_00060540(int unit,
short damage, short damageTypeIndex)`. Reads a per-body-part armour-table pointer at
`unit+0x44+part*2`, and — the strongest evidence for this function's identity — indexes a
resistance table at stride **`0x24` = 36 bytes**, which is **byte-for-byte identical** to
`DamageModifierData`'s extracted size (`tools/extractors/common/aequipment.h`: `uint16_t
damage_type_data[18]` = 36 bytes, `DAMAGE_MODIFIER_DATA_OFFSET_START/END` span exactly
`3151452..3152280` = `828` bytes = `23` rows × `36`). The indexing shape
(`table[damage_modifier_row*0x24 + damageTypeIndex*2]`, read as a `uint16` percentage-style
multiplier applied via `(damage * modifier) / 100`) matches `DamageType::dealDamage(damage,
damageModifier)` almost exactly. Accumulates a running value at `unit+0x80`, capped at `0xFF`,
and conditionally calls `FUN_000602AC` (not investigated) when a per-unit threshold at `unit+100`
(`0x64`) is exceeded. Also sets a global flag `DAT_000E6CC0 = 1` when the computed damage is
nonzero and a per-unit ID field (`unit+0x26`) matches a global "who's under fire" cursor
(`DAT_000E6D46`) — plausibly the trigger for the already-bound `Unit under fire` notification, but
not confirmed.

Called from **both** `FUN_0007d67c` (three call sites — the AOE blast placement routine) and
`FUN_0007bd8c` (two call sites — the per-tile hazard-tick dispatcher that also calls the fire
spread function `FUN_0007b0d0`, §2.2). This dual call pattern (initial blast damage + per-tick
hazard damage, same function) is exactly the shape OpenApoc's own `BattleUnit::applyDamage` +
`updateStateAndStats`'s fire/enzyme processing loop already has — strong circumstantial support
that this is the generic damage/resistance pipeline **all** hazard types run through, fire and
enzyme alike, parameterized by `damageTypeIndex`.

**What is not bound**: which `unit+offset` field is "fire intensity" specifically (as opposed to
general accumulated damage, or a stun-style counter — `unit+0x80`'s `0xFF` cap and threshold-gated
side effect resemble a stun/incapacitation accumulator at least as plausibly as a fire counter), and
no tick-cadence constant was read from this function at all (it is a single-application function,
not a scheduler — the cadence lives in the caller, which was not traced further).

### 5.2 `FUN_0007bcb8` — RNG-gated helper, reads unit `+0x82`

VA `0x7BCB8` / file `0xD675C` (derived). Called from `FUN_0007bd8c` (twice) and `FUN_0007d67c`
(once). Reads `unit+0x45` (a byte — branches on `<2` or `==2`, rolling `FUN_0001eee8` when true)
and `unit+0x82` (read as a 4-byte int, `>>0x10`) to compute a position/index passed to
`FUN_0005f860` and `FUN_00097108(2, delta)`. In `FUN_0007d67c`'s case-2 (fire) AOE unit-loop, a
**byte** write to exactly this same offset (`unit+0x82`, via `*(byte*)(unitPtr+0x41 shorts) =
bVar2`, where `bVar2` is the blast-intensity byte gated by `> 0x18`) happens immediately before
this function is called elsewhere in the hazard pipeline — but reading a 1-byte write back as a
4-byte int (`>>0x10`) is not self-consistent without more context (the field may be
reused/repurposed between the two call sites, or the decompiler's typing is wrong, or `+0x82` is
actually a packed position field most of the time and only incidentally shares an address range
with the blast-intensity write). **Not asserted as bound** — recorded as the most promising lead
for a future session, with the exact addresses needed to pick this back up.

### Verdict on this bonus item

**PARTIAL / lead only.** `FUN_00060540` is a well-evidenced generic damage/resistance function
(high confidence via the exact `damage_modifier_data` stride match) but does not by itself
constitute "unit fire intensity distinct from `TICKS_PER_FIRE_EFFECT`" — no tick-cadence constant
was recovered. `FUN_0007bcb8` remains a genuine open lead. **Do not** wire either into
`battleunit.cpp`'s fire/enzyme tick processing on this evidence — neither function's exact field
semantics were confirmed.

---

## 6. What was not attempted

- `FUN_0007b228` (overlay-type-0 handler in the `FUN_0007b610` dispatch, §3.1) was not decompiled.
- `FUN_0007d67c`'s cases 0, 1, 3 and the tail of case 4 were not read in full (the function is
  3,972 bytes; only the switch dispatch and case 2's unit loop were read closely). A parallel unit
  loop may exist in case 1 or case 3 and was not ruled out — if it exists and differs from case 2's
  in a way that reads an armour field, that would resolve §3.4's open question.
  wire it from a table-size or duration guess.
- `FUN_0007aa8c`'s two resistance tables (`DAT_000F9CDB`, `DAT_000FF2DB`) were located but their
  contents were not dumped or decoded.
- A numeric (non-string-anchored) scan for the `agent_general_data` catalog-row → runtime-instance
  copy site (cloak's most likely remaining path) was not attempted — flagged in §4.2 as the
  concrete next step, distinct from a blind `CMP reg, 0xA` sweep (rejected as too unconstrained).

## Artifacts (lab only — not copied into this tree)

- `OpenApoc-og-research/scripts/QueryEnzymeCloak.java` — string xref check, packed-pool copies.
- `OpenApoc-og-research/scripts/QueryOverlayTypeDispatch.java` — `SAR reg,0x6` / `CMP reg,{1,3}`
  scan that found the type-1/type-3 dispatch sites (§3).
- `OpenApoc-og-research/scripts/QueryHazardDeepDive.java` — combined follow-up: neighbour-table
  dump, `DAT_003009A0` writer scan (empty), `FUN_0007bcb8`/`FUN_00060540` decompiles,
  `agent_general_data` VA recovery, fixed-stride asset-name xref checks.
- `OpenApoc-og-research/scripts/QueryCloakType0a.java` — `agent_general_data` table/row xref
  checks (§4.2).
- Logs: `export/query_hazard_rng{,2}.log`, `export/query_fire_item_resist_dispatch.log`,
  `export/query_fire_scheduler_dispatch.log`, `export/query_fire_type_dispatch.log` (superseded by
  the successful re-run after the project lock cleared), `export/query_overlay_type_dispatch.log`,
  `export/query_overlay_type_siblings1.log`, `export/query_hazard_dispatcher.log`,
  `export/query_enzyme_encoders_and_mindshield.log`, `export/query_fun_7d67c.log`,
  `export/query_hazard_deep_dive.log`, `export/query_enzyme_cloak_strings.log`.

Note: this session hit a `LockException` against the shared `OpenApocOG_TACP` Ghidra project from
a concurrent peer session (independently working B1, cover-tile scoring, against the same
project). Waited for the lock rather than forcing; coordinated over `SendMessage`; no destructive
action was taken on the shared project.
