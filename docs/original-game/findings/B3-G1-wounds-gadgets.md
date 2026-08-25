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
| Disruptor Shield | `0x08` | **BOUND** — new finding, see below |
| Cloaking Field | `0x0a` | **BOUND (partial)** — recognized by two consumers, unique effect not found; see below and cross-ref K1 |
| Dimension Force Field | `0x0b` | **NOT BOUND** — recognized only as a UI-picker fallback, no effect consumer |
| MultiTracker | `0x04` | **NOT BOUND — inconclusive**, searched full 20-function subtype-reader set; found only a locally-scoped predicate whose caller chain wasn't traced past its own 3-function cluster |
| Vortex Analyzer | `0x03` | **NOT BOUND — no reader, dead in original** (within the 20-function set searched) |
| Structure Probe | `0x02` | **NOT BOUND — no reader, dead in original** (within the 20-function set searched) |
| Alien Detector | `0x07` | **NOT BOUND — no reader, dead in original** (within the 20-function set searched) |

### Disruptor Shield (`0x08`) — BOUND

This overturns the current assumption in `game/state/battle/battleunit.cpp` (`useItem` returns
`false` for `Type::DisruptorShield`). Five functions, all decompiled and read in full, all
addresses TACP.EXE non-4:

- **`FUN_0006511C`** (page-file `0x5511B`) — regenerates the shield item's own charge field
  (equipment-instance-table `+10`, a 16-bit value) by 1 per call, gated `< 100`. **Called directly
  from `FUN_00066474`** — the same per-unit tick dispatcher that drives Mind Shield, at the same
  cadence gate (`unit+0x254 < unit+0x256`, incrementing `unit+0x254` and calling this function).
- **`FUN_0006508C`** (page-file `0x5508B`) — takes `(unit, uint amount)`. Finds the unit's
  Disruptor Shield entry; if `amount < entry+8>>0x10` (a charge-capacity field), decrements
  `entry+10` by `amount` and returns (fully absorbed); otherwise subtracts the shield's remaining
  capacity from `amount` and falls through to `FUN_000598D4` — the F1-documented type-4 doodad
  spawner (`docs/original-game/parity-guide.md` §F1) — i.e. a visible effect fires when the
  shield's charge is exceeded.
  **Caller traced:** `FUN_0005F860` (bound-file `0xBA304`) — the general "apply `amount` of
  damage-type `type` to `unit`" function (`unit`, `short damage`, `short damageTypeIndex`),
  called from the fire/hazard subsystem (`FUN_0007BCB8`, `FUN_0007BD8C`, in the same `0x7B000`–
  `0x7E000` neighbourhood as the F1-documented fire functions) as well as elsewhere — i.e. this is
  a general damage-application entry point, not fire-specific. Inside it, `unit+0x254` (same field
  `FUN_00057A04` fills from the Disruptor Shield) is read as a **damage-absorption buffer**: if
  the type-modified incoming damage is less than `unit+0x254`, the buffer absorbs it fully
  (`unit+0x254 -= damage`) and calls `FUN_0006508C`; otherwise the buffer is zeroed, the excess
  damage passes through to health, and `FUN_00064FFC` (the shield-break/doodad cleanup, above)
  fires instead. This confirms `unit+0x254`/`unit+0x256` is a general rechargeable
  damage-absorption stat, and the Disruptor Shield is what feeds it (+100 capacity, plus the
  item's own charge, while worn) — i.e. **a rechargeable damage shield that soaks incoming damage
  ahead of health**, not a stun-specific mechanic.
- **`FUN_00057A04`** (bound-file `0xB24A8`) — per-unit function. While a Disruptor Shield is worn
  and equipped (gated by a `FUN_00023960() != 0` check), adds **100** to `unit+0x256` (a capacity
  field) and adds the shield's own charge (`entry+10`) to `unit+0x254` (a current field), then
  clamps `unit+0x254` to not exceed `unit+0x256`.
- **`FUN_0005797C`** (page-file `0x4797B`) / **`FUN_00012D3C`** (page-file `0x2D3B`) — two
  call-sites of the identical pattern: loop every equipment instance, and for each Disruptor
  Shield, set its charge field (`entry+10`) to **100** (full recharge). Both also drive a
  companion per-unit loop calling `FUN_00057A04` for every unit (0–0x3B). Shape suggests a
  mission-start or per-turn reset, not confirmed which.
- **`FUN_00064FFC`** (page-file `0x54FFB`) — per-unit function. For a worn Disruptor Shield, calls
  `FUN_000598D4` (same doodad spawner) then zeroes `unit+0x254`/`unit+0x256` — read as "shield
  removed/broken" cleanup, not confirmed.

Also present in the AI equip-priority scoring switch (see below), case `8`, sharing the identical
gated `×0xc` multiplier formula with Mind Shield (case `5`) and Cloaking Field (case `10`/`0xa`) —
i.e. the AI treats these three as one "defensive shield" priority class when deciding what to
equip.

**What is NOT claimed:** an exact regen rate in seconds/ticks, the precise damage-type modifier
table values consumed by `FUN_0005F860`, or whether the mission-start/per-turn reset trigger for
the two `FUN_0005797C`/`FUN_00012D3C` full-recharge sites is confirmed (it is inferred from their
loop shape, not from a caller trace). The mechanism itself — a per-item rechargeable charge that
grants the unit +100 capacity and its own charge as current on a general damage-absorption buffer,
which soaks type-modified incoming damage ahead of health, with a visual cue on overflow and a
1/tick regen — is fully reproducible from the decompile chain above and should be treated as solid
enough to implement `Type::DisruptorShield` from, modulo the two open numeric details just named.

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

### MultiTracker (`0x04`) — NOT BOUND, inconclusive

The only consumer found is a standalone predicate, **`FUN_000A3170`** (page-file `0x9316F`, 65
bytes — "is this catalog entry a MultiTracker?"), called by wrapper `FUN_000A31B4` (page-file
`0x931B3`), called in turn only by `FUN_000A30C8` (page-file `0x930C7`). All three live in a tight
tooling cluster (12 functions, address range `0xA2B00`–`0xA3400`). This investigation traced two
hops up from the predicate and did not find a caller of `FUN_000A30C8` reaching outside that
cluster into gameplay code (time-boxed, not exhaustive).

For comparison, the byte-for-byte structural sibling predicate for Motion Scanner
(`FUN_000A2C98`/wrapper `FUN_000A2CDC`, same cluster) **is** called from an external, real
gameplay function `FUN_0003D9E4` — proving the cluster as a whole isn't dead code, only that
MultiTracker's specific branch through it wasn't traced to a live caller in the time available.
Falls into the AI equip-scoring switch's generic `default` case (no special multiplier), same as
Structure Probe / Vortex Analyzer / Alien Detector / Dimension Force Field.

**Verdict is inconclusive, not dead-confirmed** — flagging for a follow-up session to walk
`FUN_000A30C8`'s callers rather than recording a "no reader" verdict that the Motion-Scanner
comparison doesn't actually support.

### Vortex Analyzer (`0x03`), Structure Probe (`0x02`), Alien Detector (`0x07`) — NOT BOUND, no reader, dead in original

None of the 20 functions that read `DAT_002b2e7a` (the general-type subtype table) branch on
`0x02`, `0x03` or `0x07` anywhere. All three fall only into the AI equip-scoring switch's generic
`default` case (`FUN_0008A524`, bound-file `0xE4FC8`) alongside ordinary loot — no special
multiplier, no dedicated predicate (unlike MultiTracker, which at least has `FUN_000A3170`), no
per-tick effect. This is a clean, well-searched negative: **these three general-type ids were
already dead weight in the shipped TACP.EXE**, not merely unwired in OpenApoc.

---

## Do not

- Do not invent a wound TU/accuracy penalty magnitude, subtraction-vs-percentage split, or
  body-part specificity for B3. Nothing above supports any of that.
- Do not invent a Disruptor Shield exact regen-rate cadence or the damage-type modifier table
  values — those two numeric details are still open. The absorption mechanism itself (charge pool,
  capacity/current pair on `unit+0x254`/`+0x256`, doodad-on-overflow, 100-point resets) is
  confirmed via `FUN_0005F860`, the general damage-application function, and should not be
  re-litigated as unconfirmed.
- Do not invent a Cloaking Field detection/concealment mechanism from this file — see K1.
- Do not promote MultiTracker to "confirmed dead" — the evidence here is thinner than for
  Structure Probe / Vortex Analyzer / Alien Detector.
