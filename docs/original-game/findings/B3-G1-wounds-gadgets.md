# B3 (wounded move/shoot penalty) and G1 (dead gadgets) — RE findings

Agent: RE-wounds. Lab: `OpenApoc-og-research`, project `ghidra_projects/OpenApocOG_TACP.rep`,
program `TACP.EXE` (non-4, CRC32 `0xfebbe39e`), loader `-processor x86:LE:32:default -cspec gcc`.
See [README.md](README.md) for the verdict convention and
[METHOD-tacp-string-regions.md](METHOD-tacp-string-regions.md) for why xref-anchoring on TACP UI
strings is not by itself evidence of a missing consumer — that applies to the B3 half of this file.

---

## Verdict — B3 wounded move/shoot penalty

> **NOT BOUND — no consumer found via five independent structural methods (string xref, whole-
> memory pointer scan, whole-binary code-constant scan, SCASB-walker cross-reference, and
> cross-check against the one confirmed real pool resolver table); wound counter's struct offset
> was not established.**

### What was searched

**Step 1 — string xref**, per [parity-guide.md §B3](../parity-guide.md):

| String | TACP (non-4) file offset | TACP4 file offset | Bound xrefs |
|---|---|---|---|
| `Unit critically wounded: ` | `0x2DF0BE` | `0x2DCEBE` | **0** |
| `Unit critically wounded` | `0x2DF0BE`(dup)/`0x2E0400` | `0x2DCEBE`(dup)/`0x2DE200` | **0** |
| `Not enough TU's - TU cost per wound: ` | `0x2DFD14` | `0x2DDB14` | **0** |
| `TU cost per wound: ` | `0x2E0204`/`0x2E0760`(dup) | `0x2DE004` | **0** |
| `Unit under fire` | `0x2E0438` | — | **0** |
| `Unit has gone berserk` (sic, one `r`) | `0x2DF134` | — | **0** |
| `Cautious mode` / `Aggressive mode` / `Kneel down` / `Reserve TUs for kneel` | `0x2DFE51`/`0x2DFE5F`/`0x2DFEAE`/`0x2E01C4` | — | **0** |

Every one of these lives in the packed, null-terminated message pool `0x2DE000`–`0x2E2FFF`
(non-4) documented in [METHOD-tacp-string-regions.md](METHOD-tacp-string-regions.md). Per the
project's own rule, entries in that pool are reached through an index, so a zero xref on any one
of them is expected, not conclusive — which is why this investigation did not stop at step 1.

**Extra methods run, beyond what §B3 requires, specifically to rule out a missed indirection:**

1. Raw 4-byte pointer scan across **all** memory (every block, not just via Ghidra's reference
   database) for the mapped-copy address of every string above — **0 hits, all ten addresses.**
2. Full executable-code scalar-operand scan for **any** instruction in TACP.EXE whose immediate
   operand lands inside `[0x28E000, 0x292000]` (the ~16 KB span covering all three mapped copies
   of the message pool) — **6 hits, all in an unrelated preceding data blob (`0x28E038`–
   `0x28E6FA`), none inside the pool itself.**
3. Direct check of the coordinator-suggested lead address `0x2DFE0E` (and a ±0x100 window, both
   raw `.image` and mapped) with the same xref + pointer-scan + code-constant-scan trio — **0 on
   every method.**
4. Every `SCASB`/`REPNE SCASB` site in TACP.EXE (128 sites, 48 containing functions — the classic
   x86 "walk to next NUL" idiom a pool-index resolver would need) cross-referenced against which
   of those 48 functions also touch the pool's address window — **2 candidates.** Both fully
   decompiled:
   - `FUN_00055400` (page-file `0x47483`) — the agent-status UI screen; touches address
     `0x28E0C6`, an **adjacent but distinct** stat-percentage label table, not the wound pool.
   - `FUN_0005B7A8` (bound-file `0xC0208`) — a random callsign generator, reading a **different**
     table at `0x28E6FA` (first-name/colour fragments), unrelated to combat messages.
5. Cross-checked against the one **confirmed real** resolver mechanism in this pool, found mid-run
   by RE-cover (B1): a sparse pointer table at object2 `0x292D18`–`0x292DEC` (non-4 file
   `0x2E27BC`–`0x2E2890`) holding one absolute 4-byte pointer per entry, each with a genuine
   `getReferencesTo` xref — full dump plus a wider margin scan to confirm the boundary is
   `0x292D18` exact (everything below it, from `0x292C00`, is the string bytes of the table's own
   first entry, not more pointers). Checked every one of the ~24 non-null entries against `Unit
   critically wounded`, `TU cost per wound: `, and (as a proposed same-category, same-pool
   control) `TU cost per shot: ` — **none of the three appear in this table**, and `TU cost per
   shot: `'s own direct xref is independently **0** too, so it is not actually a valid positive
   control: the whole `TU cost per X` family (wound/shot/throw/use/activate/attempt) is absent
   from this specific mechanism, not just the wound member. The table's real entries skew toward
   short UI-widget labels and one-shot system dialogs (`Ammo Clip`, `Weight:`, `Health`, `Pause`,
   `MISSION BRIEFING`, `Hostile unit spotted`, `PLEASE PUT THE XCOM CD BACK IN THE DRIVE`) rather
   than combat-log severity text — consistent with it being linker-packed individual `char *`
   globals for a different subsystem than whatever prints combat messages.

No method produced a positive lead. This is recorded per the project's stated rule as a genuine
structural exhaustion, not an absent-xref shortcut — see
[METHOD-tacp-string-regions.md](METHOD-tacp-string-regions.md) for the shared methodology writeup
and the follow-on "string resolver" investigation this triggered
([METHOD-tacp-string-resolver.md](METHOD-tacp-string-resolver.md)).

### What this means for B3

- The wound-counter's struct offset on the TACP unit record was **not established** by this
  investigation. No reader inside a TU or accuracy computation was found, because no path from
  any wound-related string to any code was found at all.
- **Do not** infer a wound penalty magnitude, subtraction-vs-percentage split, or body-part
  specificity from this file. None of that is evidenced. `game/state/battle/battleunit.h:42`'s
  `TICKS_PER_WOUND_EFFECT` FIXME stays open.
- If [METHOD-tacp-string-resolver.md](METHOD-tacp-string-resolver.md) later binds the pool's
  token/index mechanism, re-open this row from the `TU cost per wound: ` lead — that string is
  still the strongest indirect evidence the original modelled a per-wound cost, it is simply not
  reachable by address-based search.

---

## Verdict — G1 dead gadgets

Extractor type ids from
[tools/extractors/common/aequipment.h](../../../tools/extractors/common/aequipment.h)
(`AGENT_GENERAL_TYPE_*`). Searched: every reader of the general-equipment category/subtype lookup
tables discovered while tracing Mind Shield (below) — `DAT_002b2854` (category, 96 raw xrefs),
`DAT_002b2855` (subtype index byte, 117 raw xrefs) and `DAT_002b2e7a` (subtype value, stride
`0xc`, 25 raw xrefs collapsing to **20 unique functions**). All 20 were decompiled and read in
full; this is the complete set of TACP code that ever inspects an equipped item's general-type id.

**Worked example — Mind Shield (`0x05`), already bound, reproduced fresh in this session.**
The address in [tacp.md](../binaries/tacp.md) (`FUN_0009b780 @ 0x9B780`) did not resolve to a
distinct function in `OpenApocOG_TACP.rep` as currently imported — `0x9B780` lands mid-body of an
unrelated tile-scoring function (`FUN_0009b058`), meaning the two Ghidra import sessions assigned
different addresses to the same source. Re-derived independently via a tight
CMP-5/ADD-30/CMP-200 proximity scan: **`FUN_00066474`** (bound-file `0xC0F18`), the per-unit tick
dispatcher. Two symmetric blocks (one per hand slot, `unit+0x2AE`/`+0x2B0` and `unit+0x2B2`/
`+0x2B4`) check category `== 0x02` (`AGENT_EQUIPMENT_TYPE_GENERAL`) then subtype `== 0x05`; on
match, `unit+0x89 = min(unit+0x8A + 30, 200)`. A separate unconditional block earlier in the same
function decays `unit+0x89` toward `unit+0x8A` by 1 per tick when worn-off, so a worn Mind Shield
re-boosts every tick — reproducing the documented "+30, cap 200" exactly, just at a different
current-session address. **This is unchanged from the existing doc; recorded here only as the
worked-example baseline the rest of this table is compared against.**

| Type | id | Verdict |
|---|---|---|
| Mind Shield | `0x05` | **BOUND** (reconfirmed, see above) |
| Disruptor Shield | `0x08` | **BOUND** — regen cadence, recharge trigger, damage-type modifier, and overflow behaviour all resolved, see below |
| MultiTracker | `0x04` | **BOUND** — traced past the local cluster to a live, widely-called candidate-list builder shared with Motion Scanner; see below |
| Cloaking Field | `0x0a` | **BOUND (partial)** — recognized by two consumers, unique effect not found; see below and cross-ref K1 |
| Dimension Force Field | `0x0b` | **NOT BOUND** — recognized only as a UI-picker fallback, no effect consumer |
| Vortex Analyzer | `0x03` | **NOT BOUND — no reader, dead in original** (within the 20-function set searched) |
| Structure Probe | `0x02` | **NOT BOUND — no reader, dead in original** (within the 20-function set searched) |
| Alien Detector | `0x07` | **NOT BOUND — no reader, dead in original** (within the 20-function set searched) |

### Disruptor Shield (`0x08`) — BOUND, all four follow-up numbers resolved

This overturns the current assumption in `game/state/battle/battleunit.cpp` (`useItem` returns
`false` for `Type::DisruptorShield`). All addresses TACP.EXE non-4.

- **`FUN_0006511C`** (page-file `0x5511B`) — regenerates the shield item's own charge field
  (equipment-instance-table `+10`, a 16-bit value) by 1 per call, gated `< 100`. **Called directly
  from `FUN_00066474`** — the same per-unit tick dispatcher that drives Mind Shield, at the same
  cadence gate (`unit+0x254 < unit+0x256`, incrementing `unit+0x254` and calling this function).
- **`FUN_0006508C`** (page-file `0x5508B`) — takes `(unit, uint amount)`. Finds the unit's
  Disruptor Shield entry; if `amount < entry+8>>0x10` (a charge-capacity field), decrements
  `entry+10` by `amount` and returns; otherwise falls through to `FUN_000598D4` — the
  F1-documented type-4 doodad spawner (`docs/original-game/parity-guide.md` §F1). Called only from
  the unit-level "fully absorbed" branch below — read as syncing the item's own charge counter to
  the unit-level buffer's just-updated state, not as a second independent absorption step.
  **Caller traced:** `FUN_0005F860` (bound-file `0xBA304`) — the general "apply `amount` of
  damage-type `type` to `unit`" function (`unit`, `short damage`, `short damageTypeIndex`),
  called from the fire/hazard subsystem (`FUN_0007BCB8`, `FUN_0007BD8C`, in the same `0x7B000`–
  `0x7E000` neighbourhood as the F1-documented fire functions) as well as elsewhere — i.e. this is
  a general damage-application entry point, not fire-specific. Inside it, `unit+0x254` (same field
  `FUN_00057A04` fills from the Disruptor Shield) is read as a **damage-absorption buffer**,
  confirming `unit+0x254`/`unit+0x256` is a general rechargeable damage-absorption stat that the
  Disruptor Shield feeds (+100 capacity, plus the item's own charge, while worn) — i.e. **a
  rechargeable damage shield that soaks incoming damage ahead of health**, not a stun-specific
  mechanic.
- **`FUN_00057A04`** (bound-file `0xB24A8`) — per-unit function. While a Disruptor Shield is worn
  and equipped (gated by a `FUN_00023960() != 0` check), adds **100** to `unit+0x256` (a capacity
  field) and adds the shield's own charge (`entry+10`) to `unit+0x254` (a current field), then
  clamps `unit+0x254` to not exceed `unit+0x256`.
- **`FUN_0005797C`** (page-file `0x4797B`) / **`FUN_00012D3C`** (page-file `0x2D3B`) — two
  call-sites of the identical pattern: loop every equipment instance, and for each Disruptor
  Shield, set its charge field (`entry+10`) to **100** (full recharge). Both also drive a
  companion per-unit loop calling `FUN_00057A04` for every unit (0–0x3B).
- **`FUN_00064FFC`** (page-file `0x54FFB`) — per-unit function. For a worn Disruptor Shield, calls
  `FUN_000598D4` (same doodad spawner) then zeroes `unit+0x254`/`unit+0x256` — the "shield broken"
  cleanup, fired from the depleted branch below.

Also present in the AI equip-priority scoring switch (see below), case `8`, sharing the identical
gated `×0xc` multiplier formula with Mind Shield (case `5`) and Cloaking Field (case `10`/`0xa`) —
i.e. the AI treats these three as one "defensive shield" priority class when deciding what to
equip.

#### Follow-up 1(a) — regen cadence: BOUND, once per game-second

`FUN_0006511C` is only ever reached through `FUN_00066474`, which is driven from two callers, both
gating on the identical `tick_counter % 0x24 == 0` test (`0x24` = 36, and the counter is built from
the same minutes/seconds/subsecond fields already established elsewhere in this project as the
36-per-second vanilla clock — see `VANILLA_TICKS_PER_SECOND = 36` in `game/state/gametime.h`):

- **`FUN_000655D0`** (bound-file `0xC0074`), the real-time per-frame unit updater — called from
  `FUN_00011620` (page-file `0x161F`), the single caller, i.e. the main loop's per-frame tick. It
  additionally reads a `DAT_000e6c24` game-speed multiplier (values 1–4) to catch up 1–4 elapsed
  vanilla ticks per frame, staggering which of the 60 unit slots gets the `%36` hit on any given
  frame so the 60-unit sweep doesn't all land on the same frame — same load-balancing shape as the
  fire scheduler's cursor mechanism (F1).
- **`FUN_000B8C50`** (bound-file `0x1136F4`) — the F1-documented turn-wrap 400-vanilla-tick catch-up
  batch (its own inner `iVar3 < 400` loop, and it calls `FUN_0007B7F8`, F1's real-time fire
  scheduler, once per outer iteration — confirming this is the same function F1 already bound).
  Reached from `FUN_00010294` (bound-file `0x6AD38`, the program's `main`/entry function — argv
  parsing, `.ini` handling, then game setup) inside
  `do { ... FUN_000b8c50(); ... } while (DAT_0027a0ea != DAT_0027a0f4);` — i.e. **once per
  turn/side transition**, not once per frame. Inside its 400-tick loop the same `%0x24==0` test
  fires `FUN_00066474` roughly `400/36 ≈ 11` times per unit — the "seconds' worth of real-time
  simulation to catch up" for whatever wall-clock time a side's turn is deemed to have taken.

**Bound answer:** the Disruptor Shield's item charge regenerates **+1 every 36 vanilla ticks, i.e.
once per real-time second** — `TICKS_PER_DISRUPTOR_SHIELD_REGEN = TICKS_PER_SECOND` in this
project's own tick units, directly comparable to the fire scheduler's documented cadence (both are
driven off the same 36-per-second vanilla clock; fire ties its iteration to 1 vanilla tick, this
ties its regen to 36 vanilla ticks). This is not a per-turn or per-frame constant — the same
regen fires continuously during real-time play (`FUN_000655D0`, scaled 1–4× by game speed) and is
caught up in one burst per turn transition (`FUN_000B8C50`).

#### Follow-up 1(a)-continued — periodic full recharge: BOUND, battle load only, not periodic

`FUN_0005797C` and `FUN_00012D3C` are **both** called from `FUN_00010294` (`main`), inside the
battle-setup branch (`DAT_0027f69c >> 0x18 == -1`), and **neither call site is inside the
turn-transition loop** described above:

- `FUN_00012D3C` fires once, gated `DAT_0027a805=='\0' && DAT_0027f69c._1_1_=='\0'` — read as
  "fresh mission start" (not resuming a saved battle).
- `FUN_0005797C` fires once, later in the same branch, **unconditionally** — i.e. on every battle
  load, fresh or resumed.

**Bound answer:** the full-recharge-to-100 happens **exactly once, at battle load/start** — there
is no periodic (per-turn or per-day) re-trigger anywhere in the caller graph reached from `main`.
Do not implement a repeating full-recharge; implement it as a one-time reset when a battle begins.

#### Follow-up 1(b) — damage-type modifier: BOUND, but it is not shield-specific

Inside `FUN_0005F860`, the two tables the type-modified damage value is computed from resolve to
**already-extracted, general-purpose tables, not a Disruptor-Shield-specific resistance table**:

- `DAT_001b25fc[type]` → bound-file **`0x2020A0`** — this is exactly
  [`damage_type_data`](../../../tools/extractors/common/aequipment.h)'s already-documented offset
  (a per-type resist flag byte).
- `DAT_002b1df6[type*2]>>0x10` → bound-file **`0x30189A`** — inside/immediately adjacent to
  `damage_modifier_data` (documented bound-file `0x30165C`, currently flagged **low confidence**
  in `labels/tacp_rebase.csv`, method `near_first_of_2`). This is an independent, code-reader-based
  confirmation that a live consumer touches that exact table region — worth relaying to whoever
  owns firming up that extractor's confidence, separately from this row.

  **Relayed and declined — do not upgrade the CSV on this.** A later pass checked whether this
  justifies raising `damage_modifier_data`'s `low` confidence in `labels/tacp_rebase.csv`, and it
  does not. That field grades the **rebase mapping** — the method is `near_first_of_2`, meaning the
  4-build counterpart at `0x2FF45C` was chosen from two candidate byte-matches by taking the first.
  The evidence above is entirely within the **non-4** build: it establishes that the table region
  is real and read by live code, which was never the doubt. It cannot discriminate between the two
  4-build candidates, so it leaves the graded question untouched. Firming that row up means
  disambiguating the second candidate, not adding more non-4 readers.

**Bound answer:** the Disruptor Shield does **not** discriminate by damage type on its own. Damage
is converted to an "effective" value by the same general damage-type resist/modifier system every
other damage application already uses, and the shield buffer then absorbs whatever effective value
comes out — uniformly, regardless of type. Implement damage-type resistance (if any) through the
existing damage-type pipeline, upstream of the shield; do not add a second, shield-specific
resistance table.

#### Follow-up — overflow/passthrough: BOUND, and the original writeup here had it wrong

Re-read from the **raw instruction listing**, not the decompiler's C rendering, because the
decompiler's ordering is actively misleading on this branch (it renders as "subtract the old
capacity from the excess," but the assembly zeroes `unit+0x254` *before* the subtraction reads it,
so the subtracted value is always 0):

```
0005f918  MOV  AX, [EBP+0x254]      ; AX = shield current
0005f91f  CMP  AX, DX               ; DX = incoming (type-modified) damage
0005f922  JLE  0x5f946              ; shield <= damage -> depleted path
; -- shield > damage: fully absorbed --
0005f926  SUB  EDI, EDX             ; remaining = shield - damage
0005f931  MOV  [EBP+0x254], DI      ; shield -= damage
0005f938  CALL FUN_0006508c
0005f93d  XOR  EAX, EAX             ; damage passed to health = 0
; -- shield <= damage: depleted --
0005f946  MOV  [EBP+0x254], 0x0     ; shield zeroed FIRST
0005f951  SUB  BX, [EBP+0x254]      ; damage - 0 = damage UNCHANGED
0005f95f  CALL FUN_00064ffc
```

**Bound answer:** this is an **all-or-nothing** shield, not a partial-absorption-then-pass-remainder
one. If `shield_current > damage`, the hit is fully absorbed and nothing reaches health. If
`shield_current <= damage` (including exact equality), the shield is zeroed and the **entire**
incoming damage passes through to health unreduced — none of the shield's pre-hit value offsets
it. Do not implement a "partial absorb, remainder passes through" formula; that reading came from
trusting the decompiler's C output over the assembly and is wrong.

**What is still not claimed:** which exact damage-type ids get which resist flag/modifier value in
`damage_type_data`/`damage_modifier_data` (those are the existing tables' own data, out of scope
for this row), and the precise semantics of the `amount` argument `FUN_0005F860` passes into
`FUN_0006508C` on the fully-absorbed path (it updates the item's own charge counter in some way
tied to the buffer delta, but the exact arithmetic wasn't traced beyond what's shown above — it
doesn't affect the four bound answers, since the unit-level buffer update is the one the shield's
observable behaviour depends on).

### Cloaking Field (`0x0a`) — BOUND (partial), cross-ref K1

Two consumers found, no unique per-tick/per-use effect function:

- **AI equip-priority switch** (`FUN_0008A524`, case `10`) — identical gated `×0xc` scoring
  formula as Mind Shield/Disruptor Shield (same "shield class" treatment).
- **`FUN_00057AC8`** (bound-file `0xB256C`) — a "find the special equipped gadget for status
  display" picker: returns the Disruptor Shield's slot index if worn, else the first of
  Teleporter (`0x09`) / Cloaking Field (`0x0a`) / Dimension Force Field (`0x0b`) found, in that
  priority order. This is a UI/status-icon selection, not a gameplay effect.

The per-unit tick dispatcher `FUN_00066474` (Mind Shield's home) has **no case for `0x0a`** — so
whatever the concealment mechanic actually is, it is not implemented alongside Mind Shield's tick.
This is consistent with, and does not add new evidence beyond, the existing K1 finding
(`docs/original-game/parity-guide.md` §K1: "cloak tick thresholds unbound"). Recommend treating
Cloak as owned by K1, not re-opened as a second G1 sub-investigation.

### Dimension Force Field (`0x0b`) — NOT BOUND

Only appears as the lowest-priority fallback in `FUN_00057AC8`'s status-icon picker (see above).
In the AI equip-scoring switch it falls into the generic `default` case — the same untuned
`score = score*0x10*byte/(byte+2)` formula every ordinary loot item gets, no special multiplier.
No per-tick or per-use effect function found anywhere in the 20-function survey.

### MultiTracker (`0x04`) — BOUND (upgraded from "inconclusive" — Priority 2 follow-up)

The predicate chain **`FUN_000A3170`** (page-file `0x9316F`, "is this catalog entry a
MultiTracker?") → **`FUN_000A31B4`** (page-file `0x931B3`) → **`FUN_000A30C8`** (page-file
`0x930C7`) was traced one hop further, past the local 3-function cluster, and settles the
verdict cleanly:

- `FUN_000A31B4(unit, ...)` checks **both** of the unit's general-equipment slots (`unit+0x2AE`,
  `unit+0x2B2`) against the MultiTracker predicate; on a match it records **which hand** in a
  global (`_DAT_002a17ac = 0` or `1`).
- `FUN_000A30C8(candidate, ...)`, gated by a global feature flag (`DAT_000e6d40._2_2_ != 0`,
  checked identically for the Motion Scanner sibling below), calls the above for a target/candidate
  reference; on a MultiTracker match **and** a non-null candidate, it calls `FUN_000A2F80` (a
  candidate-registration routine) instead of the "no match" fallback `FUN_00019A3C`.
- `FUN_000A30C8` is called from **`FUN_00017390`** (page-file `0x738F`, 5150 bytes) — a
  position/candidate-list builder (its `switch` computes tile-like X/Y positions per case). This is
  **not** an isolated function: it has **six live call sites across five different functions**
  system-wide (`FUN_00015EFC`, `FUN_000171AC`, `FUN_0001C508`, `FUN_0001CDF4`, `FUN_000402A4`,
  `FUN_000404A0`), confirming it is not dead/unreached code.
- The same call site in `FUN_00017390` also calls **`FUN_000A2AE0`** (bound-file/page-file
  `0x92ADF`) — the structurally identical wrapper for the Motion Scanner predicate chain
  (`FUN_000A2AE0` → `FUN_000A2CDC` → `FUN_000A2C98`), which the previous version of this row already
  confirmed reaches live gameplay code via `FUN_0003D9E4`. MultiTracker and Motion Scanner are
  checked **side by side, in the same initialization block, gated by the same feature flag** — this
  is one shared "extend the detection/candidate list when the squad has a scanning-type item
  equipped" subsystem, not two unrelated code paths.

**Bound answer:** MultiTracker **is** wired into a live gameplay system — structurally parallel to,
and invoked from the same call site as, the already-confirmed-live Motion Scanner chain. It falls
into the AI equip-scoring switch's generic `default` case for priority purposes (no special
multiplier there), but that is a separate, lower-priority concern from whether it has *any*
consumer, which it does.

**What is NOT claimed:** the exact game-facing meaning of "the candidate" `FUN_00017390` builds a
list of (its `switch` cases read from at least two different object shapes — one with a 0x18-byte
stride resembling `FUN_00057AC8`'s equipment-adjacent records, another with different field
offsets — consistent with a heterogeneous "things on the tactical/city map" list, not confirmed
further), nor exactly what UI/gameplay effect `FUN_000A2F80` has when a candidate is registered.
Those are follow-on questions about what MultiTracker *does* once triggered; this session confirms
it *is* triggered by live code, which was the specific question asked.

### Vortex Analyzer (`0x03`), Structure Probe (`0x02`), Alien Detector (`0x07`) — NOT BOUND, no reader, dead in original

None of the 20 functions that read `DAT_002b2e7a` (the general-type subtype table) branch on
`0x02`, `0x03` or `0x07` anywhere. All three fall only into the AI equip-scoring switch's generic
`default` case (`FUN_0008A524`, bound-file `0xE4FC8`) alongside ordinary loot — no special
multiplier, no dedicated predicate (unlike MultiTracker, which has `FUN_000A3170` and, as of the
Priority 2 follow-up above, a confirmed live caller chain), no per-tick effect. This is a clean,
well-searched negative: **these three general-type ids were already dead weight in the shipped
TACP.EXE**, not merely unwired in OpenApoc.

---

## Do not

- Do not invent a wound TU/accuracy penalty magnitude, subtraction-vs-percentage split, or
  body-part specificity for B3. Nothing above supports any of that.
- Do not implement Disruptor Shield absorption as "partial absorb, remainder passes through" — the
  raw instruction listing shows it is all-or-nothing (see the overflow/passthrough section above).
  Trust the assembly over the decompiler's C rendering on this specific branch; they disagree.
- Do not add a Disruptor-Shield-specific damage-type resistance table — the modifier it uses is the
  existing general `damage_type_data`/`damage_modifier_data` tables, applied upstream of the shield,
  not something the shield owns.
- Do not implement the Disruptor Shield full recharge as periodic (per-turn/per-day) — it is
  one-time, at battle load, per the traced `main()` caller graph.
- Do not invent a Cloaking Field detection/concealment mechanism from this file — see K1.
- Do not invent what MultiTracker's registered "candidate" represents in gameplay terms, or what
  `FUN_000A2F80` does with it — the chain is confirmed live and worth implementing a basic
  presence/toggle from, but its downstream UI/gameplay effect was not traced in this session.
