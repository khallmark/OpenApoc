# U1 — reconciling `U1-ordertype-0x12c.md` vs `U1-arrived-flag-and-0x168.md` on the retarget branch

**Scope note on citations.** Binary: `UFO2P.EXE` (canonical, CRC32 `0x4749ffc1`), `.object1`. VA is the
Ghidra listing address; file offset is `.object1`'s `MemoryBlockSourceInfo` file-bytes offset (`VA −
0x10001`), re-confirmed this session via `QueryFunctions.java`'s own `page_file` field for every newly
cited function (`FUN_0005d6e4`, `FUN_0006da88`, `FUN_00092060`, `FUN_00092470`, `FUN_0005ca04`,
`FUN_00034860`, `FUN_00090d68`, `FUN_00090fe8`, `FUN_00010380`, `FUN_0004ab04`, among others). All work
this session was done against the loaded Ghidra image — `getReferencesTo`, `DumpListingRange.java`
raw-listing dumps, and `QueryFunctions.java` decompile+listing+xref dumps — never a file-offset hex read.
Every load-bearing claim below was independently raw-disassembly-verified this session; decompile is used
only for orientation and is flagged wherever it wasn't independently re-checked in raw bytes.

---

## Verdict, up front

**For the population the task names — an incursion-spawned Attack UFO, i.e. a vehicle spawned by
`FUN_0006da88` (VA `0x6da88`, file `0x5da87`, the function `game/state/city/vehiclemission.h:212-215`
already cites for `clampIncursionScatter`/`incursionTypeThreshold`) — the answer is unconditional:**

**(a) it always flies to the nearest dimension gate and leaves. It never retargets.** `U1-ordertype-0x12c.md`
is right about this population, and — contrary to its own hedging — the claim is not merely
census-dependent, it is structurally provable (§1 below). OpenApoc's current
`VehicleMission::advanceMissionCounterOnArrival` (unconditional `acquireTargetBuilding` retarget) is wrong
for this population and should call `VehicleMission::gotoPortal` instead.

**But the retarget branch (`FUN_0003a910`'s `+0x12C != 1` side, VA `0x3ad1d`) is not dead code, and
`U1-arrived-flag-and-0x168.md`'s `0x3ad93` citation is not a misattribution.** A second, structurally
distinct spawn mechanism — `FUN_00092060`/`FUN_00092470`, a periodic scheduler unrelated to the
dimension-gate incursion system — genuinely produces vehicles that take the retarget branch, calling
`FUN_00091f70` and (immediately after) `FUN_0005df1c` at `0x3ad93` for real, in that population (§2).

**Whether that second population is alien-owned or organisation-owned in the original — the one fact that
would make an OpenApoc-side fix straightforward — is NOT settled by this session's evidence, and is reported
as open rather than guessed at (§2.3).** An initial hypothesis (mapping it to OpenApoc's
`OrganisationRaid::Type::UnauthorizedVehicle`) was checked against the binary and could not be confirmed:
the vehicle-TYPE class byte `FUN_00092470` gates spawn-type selection on (`0x1285ea + type*0x7e`) reads `1`
uniformly across type indices `0`–`24`, which — cross-checked against OpenApoc's own extractor
(`tools/extractors/extract_vehicles.cpp:146`, `if (i < 10)` marks only raw type indices `0`–`9` as alien) —
spans **both** the 10 alien UFO types and roughly 15 mundane types with no distinction. This session could
not pin down which of those types `FUN_00092470`'s own type-selection loop (`[ESP+0x10e]`, not fully traced)
actually lands on for a given scheduled event, so **the owner/population identity of this second retargeting
population is an open question, not a confirmed one.** §4 reports the implementable fix for the population
the task named (still unconditional) and flags the second population as further work rather than inventing
a mapping the evidence doesn't support.

**Confidence: high** on the `FUN_0006da88` invariant (§1 — two independent exhaustive writer-census passes,
raw-verified, zero counterexamples) and on the retarget branch being live code for a real population (§2 —
the `+0x15C=1`/`+0x12C∈[2,26]`/finite-counter/`+0x16C=-1` chain is raw-verified end to end). **Low/open** on
which real-world population (alien or organisation) that second spawn path represents, and therefore on
whether OpenApoc's current data model can even express the distinction the original draws — see §2.3 and
§4's "what remains open."

---

## 1. `FUN_0006da88`: `+0x12C` is a spawn-time invariant, not merely "no writer found"

`U1-ordertype-0x12c.md` §5.3 already establishes, raw and byte-exact, that `FUN_0006da88` hardcodes
`+0x12C = 1` for every role it spawns:

```
0006dae3  MOV EBX,0x1        ; file 0x5dae2 -- BX forced to literal 1
0006dae8  MOV ECX,ESI
0006daea  SAR EAX,0x10
0006daed  CALL 0x0005d6e4     ; file 0x5daec -- +0x12C write happens inside this call, from EBX
```

`FUN_0005d6e4` (VA `0x5d6e4`, file `0x4d6e3`) is confirmed, byte-exact, to write `+0x12C` from exactly the
caller's `EBX` at call time (`0x5dbdc MOV word ptr [EBP+0x12c],AX`, file `0x4dbdb`, `U1-ordertype-0x12c.md`
§1.2) — no other register is involved.

**What makes this an invariant, not a probabilistic finding:** `U1-ordertype-0x12c.md` §1.1 already
establishes the writer census for `+0x12C` is exhaustive over Ghidra-disassembled register-relative
instructions: exactly two functions in the whole executable write it — `FUN_0005d6e4` (the spawn
initializer, one write) and `FUN_000b44a4` (VA `0xb44a4`, file `0xa44a3`, zeroes `+0x12C` only on a slot
whose validity word reads "empty/unallocated," i.e. **before** anything spawns into it, never on a live
vehicle). No other function anywhere touches this field.

That census used `Scalar == 0x12c` matching on `DYNAMIC` operands, which only catches register+small-
displacement addressing (`[EDI+0x12c]`). This codebase also demonstrably uses a second addressing pattern
for the vehicle array — a precomputed absolute constant (array base `0x160fd8` + field offset) combined
with a scaled slot index, e.g. `FUN_0005ca04`'s raw `MOV byte ptr [ESI+0x166],0x6` is rendered by the
decompiler as `(&DAT_0016113e)[iVar4 * 0x276] = 6` — so this session re-ran the census a second way,
scanning for the literal absolute constant `0x160fd8 + 0x12C = 0x161104` (plus its dword-container
neighbors `0x161102`/`0x161103`) across every `DYNAMIC`/`ADDRESS` operand in `.object1`'s executable range
(`QueryAbs12cScan.java`, this session, `export/u1_abs12c_scan.log`). Result: **24 hits, all reads** —
`CMP word ptr [reg+0x161104],0x0/0x1` (comparisons) and `MOV reg,word/dword ptr [reg+0x161102/0x161104]`
(loads into a register). **Zero writes.** The two scans use disjoint matching logic and agree: nothing in
`.object1` ever writes `+0x12C` on a live, already-spawned vehicle, under either addressing convention this
compiler uses for this struct.

**Consequence:** since `FUN_0006da88` is `+0x12C`'s only writer for the incursion-spawn population, and no
writer anywhere ever touches `+0x12C` again after spawn, `+0x12C == 1` holds for the entire lifetime of any
vehicle `FUN_0006da88` spawns — Attack, Escort, Infiltration, Subversion, Overspawn, and the unnamed role
`9`/escalated-`10`, alike. At the mission-counter-zero transition:

```
0003ad07  CMP word ptr [EDI+0x12c],0x1   ; file 0x2ad06
0003ad0f  JNZ 0x0003ad1d                  ; retarget
0003ad11  MOV byte ptr [EDI+0x16a],0x1    ; file 0x2ad10 -- arrived flag
```

this comparison is **always true** for a `FUN_0006da88`-spawned vehicle. The retarget branch at `0x3ad1d`
is therefore **structurally unreachable for this entire population**, independent of role and independent
of `+0x15C` — the `{2,3,5}`-exclusion/role-table analysis in `U1-ordertype-0x12c.md` §5 is real and correct
as far as it goes, but it is corroborating detail, not the load-bearing mechanism. The load-bearing fact is
the `+0x12C` invariant alone.

This also **closes `U1-ordertype-0x12c.md`'s own flagged gap** (§2.5/coordinator note: "4 of 17
`FUN_0005d68c` callers... not resolved... a single unexamined caller passing something other than `1`
collapses the conclusion") for the incursion population specifically: `FUN_0006da88` does not call
`FUN_0005d68c` at all — it is one of `FUN_0005d6e4`'s **six direct callers**, and its own write site is a
single, unconditional, raw-verified literal. The `FUN_0005d68c` callers are a separate question, addressed
fully in §3.

---

## 2. The retarget branch is real, live code — for a different population

### 2.1 `FUN_0005d6e4` seeds `+0x166`/`+0x15C`/`+0x168` from vehicle TYPE, independent of role — new this session

Continuing past the `+0x12C` write, `FUN_0005d6e4` unconditionally classifies the new vehicle's `+0x166`
byte from its own **TYPE** field (`+4`, i.e. `param_3[2]`) — raw, byte-exact, VA `0x5dc57`–`0x5dcb9`, file
`0x4dc56`–`0x4dcb8`:

```
0005dc57  MOV AX,word ptr [EBP+0x4]        ; AX = vehicle TYPE
0005dc64  CMP AX,0x19 / JC 0005dc80
0005dc6a  CMP AX,0x1a / JBE 0005dc8e
0005dc70  CMP AX,0x1c / JC 0005dc97
0005dc76  JBE 0005dc8e
0005dc78  CMP AX,0x1f / JZ 0005dc8e
0005dc7e  JMP 0005dc97
0005dc80  CMP AX,0xb  / JC 0005dc97
0005dc86  JBE 0005dc8e
0005dc88  CMP AX,0xe  / JNZ 0005dc97
0005dc8e  MOV byte ptr [EBP+0x166],0x0     ; file 0x4dc8d
0005dc95  JMP 0005dc9e
0005dc97  MOV byte ptr [EBP+0x166],0x2     ; file 0x4dc96
0005dc9e  ...
0005dcad  MOV DL,byte ptr [EBP+0x166]      ; file 0x4dcac -- read back the value just written
0005dcb3  MOV EBX,dword ptr [EAX*2+0x128614]
```

`DL` (the byte just written, `0` or `2`) then indexes `DAT_0012d950`/`DAT_0012d94e` (decompile,
not independently re-derived this session, but the same formula/tables `U1-arrived-flag-and-0x168.md` §2.6
already bound) to compute `+0x168` and `+0x15C`. **This is a real, unconditional write to `+0x166` inside
the shared spawn initializer, on every single call to `FUN_0005d6e4` regardless of caller.** Per
`U1-ordertype-0x12c.md`'s own dumped `DAT_0012d94e` table (§5.2), rows `0` and `2` both yield `+0x15C = 0`.

Cross-check, not just a new claim standing alone: `U1-ordertype-0x12c.md` §3.4 Block B independently found
(raw-verified, that session) that type-`0xF` vehicles self-retire via a guard requiring `+0x15C == 0`,
without knowing why. `0xF` (15) falls in the `[0xb, 0x19)`-minus-`{0xe}` range above → classification `2`
→ table row `2` → `+0x15C = 0`. The two independent findings agree exactly. This is strong corroboration
that the mechanism above is real, not a decompiler artifact.

**Callers that care about a specific role overwrite this default afterward.** `FUN_0006da88` commits its
actual role to `+0x166` and recomputes `+0x15C` via a *second* call to `FUN_0005df1c`, **after** its call
into `FUN_0005d6e4` (raw, VA `0x6db94`–`0x6dba5`, file `0x5db93`–`0x5dba4` — confirmed this session that
this sequence runs strictly after `0x6daed`'s `CALL 0x0005d6e4`, not before, resolving what looked like an
ordering ambiguity in the two prior documents' elided quotations). `FUN_0005ca04` and `FUN_00010380` do the
same thing but force role `6` (§3). Callers that do **not** re-assert role after spawning simply keep
`FUN_0005d6e4`'s TYPE-derived default (`+0x15C = 0`).

### 2.2 `FUN_00092060`/`FUN_00092470`: a periodic scheduler that writes `+0x15C = 1` directly, and `+0x12C` from its own loop index

An exhaustive `+0x15C` writer scan this session (`QueryOffset15cScan.java`, same register-relative-and-
absolute-constant technique as §1) found, beyond `FUN_0005d6e4`/`FUN_0005df1c`, two more direct writers:
`FUN_0007a730` (writes literal `5` — one of the `{2,3,5}` values `FUN_0003a910`'s entry guard excludes
outright, so irrelevant here) and **`FUN_00092470`, which writes literal `1` unconditionally**:

```
00092afb  MOV word ptr [EAX+0x15c],0x1    ; file 0x82afa -- +0x15C = 1, direct, no role/+0x166 involved
```

`EAX` here is a freshly-spawned vehicle pointer (`vehicle_array_base + FUN_0005d68c's returned slot index
* 0x276`, raw-traced this session VA `0x92a99`–`0x92ab7`). Immediately before this, at `0x92a87`
(file `0x82a86`), `EBX` (which becomes `FUN_0005d68c`'s forwarded `BX`, hence `+0x12C` via `FUN_0005d6e4`)
is loaded from `word ptr [EBP]` — **not a literal**, but the first field of a per-slot record in a table at
`DAT_0013e280`.

**Tracing where that field comes from (`FUN_00092060`, VA `0x92060`, file `0x8205f`, the sole populator of
`DAT_0013e280`'s event slots, found via `getReferencesTo(0x13e280)`):** an outer loop variable `ESI`
(decompiled as `uVar10`), running `0` through `0x1a` (26) inclusive, feeds a `DAT_0018276c` (the
org/base-status table `U1-ordertype-0x12c.md` §3.2/§3.3 already binds) lookup via `FUN_00091f70` — **the
same consumer function the retarget branch calls at `0x3ad26`** — to compare organisation strength against
alien-response strength, and on several outcome cases (a `switch` on a table-derived case byte, VA
`0x92351` jump table) writes the outer loop's own value directly into the scheduled slot's first field:

```
0009240a  MOV word ptr [EBX],SI    ; file 0x82409 -- slot[0] = ESI (the outer loop index, 0..26)
```

That slot field is exactly what feeds `+0x12C` when the scheduled event later fires (§ above, `0x92a87`).
**So this population's `+0x12C` at spawn time is the scheduler's own iteration index — a value in `[0,26]`,
excluding the population never reaching a spawn for indices `0`/`1` per the surrounding decompile-tier
guard (not independently raw-re-verified this session, disclosed as the one non-raw-checked link in this
chain) — i.e. routinely something other than `0` or `1`.**

The rest of the spawn confirms this is a genuine, ordinary-gameplay-reachable mission, not disposable
scratch data: `0x92b0f MOV byte ptr [EDX+0x171],0xa` (file `0x82b0e`) sets a real, finite mission counter
(`10`, not `FUN_0006de64`'s `0xFF` "never fires" sentinel), and `0x92b1f MOV word ptr [EDX+0x16c],0xffff`
(file `0x82b1e`) sets `+0x16C = -1`, satisfying `FUN_00059148`/`FUN_0003a910`'s own "`+0x16C == -1`"
pre-decrement guard already bound in `U1-arrived-flag-and-0x168.md` §1.1. `FUN_00092470` itself is called
from `FUN_0004ab04` (VA `0x4ab04`, file `0x3ab03`), gated on `DAT_000d5060 == 0` (the established side/turn
discriminant), alongside several other clearly-periodic per-tick functions, and `FUN_0004ab04` has a single
caller, `FUN_0004aa7c` — i.e. this runs regularly, every tick, on ordinary gameplay, not on some rare or
scripted-only path.

**Given `+0x15C == 1` unconditionally and `+0x12C` routinely outside `{0,1}`, this population satisfies
`FUN_0003a910`'s entry-guard disjunct 2 (`+0x15C==1 && +0x12C!=0`) and — at `0x3ad07`, since `+0x12C != 1`
— takes the retarget branch at `0x3ad1d` for real.** `U1-arrived-flag-and-0x168.md` §2.6's citation of
`FUN_0005df1c` being called at `0x3ad93` on the `+0x12C != 1` side is therefore **structurally accurate,
not a misattribution and not a shared tail** — the code genuinely belongs to, and is genuinely reached by,
this population. Its error was narrower: characterizing this as "every time a UFO picks a new
mission-destination building at runtime" / "ordinary UFO retargeting," which overgeneralizes across *all*
UFO populations when it is really specific to this one, structurally separate spawn mechanism.

### 2.3 What this population plausibly is, on the OpenApoc side — checked, and inconclusive

`FUN_00092470`'s spawn sequence also calls `FUN_0005faf0` with a hardcoded delta `-25` (`0xffffffe7`) —
the same org-funds-adjustment call `U1-arrived-flag-and-0x168.md` §2.2 already binds to "worth-based
org-funds settlement" — immediately before the target search, and the whole mechanism (scheduled per-org
event, threat comparison, org-funds side effect) reads like an organisation-relations- or
retaliation-triggered dispatch rather than the dimension-gate incursion pipeline. That reading, by itself,
suggested a structural match to OpenApoc's own **`OrganisationRaid::Type::UnauthorizedVehicle`**
(`game/state/shared/organisation.cpp:1288-1354`, which selects an organisation-owned vehicle and issues
`VehicleMission::attackBuilding` against a target building — the same mission type that funnels into
`advanceMissionCounterOnArrival`).

**This was checked against the binary this session, and the check did not confirm it — disclosed rather
than silently kept as a hunch.** `FUN_00092470` gates which vehicle *type* it spawns on a class byte
(`CMP word ptr [EDX+0x1285ea],0x1`, VA `0x92a3b`, file `0x82a3a` — `0x1285ea + type*0x7e` is the same
per-type catalog `FUN_0005d6e4` reads into vehicle `+0x2`/`param_3[1]`, raw-confirmed this session:
`local_1c = &DAT_001285e8 + param_1*0x3f` then `param_3[1] = local_1c[1]`). Dumping that class byte for type
indices `0`–`39` (`QueryTypeClass1285ea.java`, `Memory.getShort`, this session):

```
type  0..24  -> 1   (uniform)
type 25..32  -> 0   (uniform)
type 33..39  -> 2, 4, 1, 769, 0, 1, 12   (irregular -- likely past the real type table)
```

Cross-checked against OpenApoc's own extractor (`tools/extractors/extract_vehicles.cpp:146`,
`if (i < 10) { ...RESEARCH_UNLOCK_ALIEN_CRAFT... }` — raw type index, straight from `data.vehicle_data`,
the same index space the binary's `+4`/`+0x2` fields use): **only indices `0`–`9` are alien UFOs; `10`
upward are mundane/organisation craft.** Class `1` (the value `FUN_00092470` requires) spans indices `0`–`24`
**uniformly, covering both the alien range and roughly fifteen mundane types with no distinction between
them.** The class gate alone does not discriminate alien from organisation-owned.

Which *specific* type index a given scheduled event actually spawns depends on a second, inner loop this
session did not fully trace (`ECX` from `dword ptr [ESP+0x10e]`, bounded similarly to the outer `0..0x1a`
scheduler-index loop in `FUN_00092060` — raw-visible but not traced back to its own origin/bound this
session). **Whether that inner loop lands on alien types, mundane types, or both depending on context is not
established.** Given the prime directive against inventing a mapping that isn't backed by a recovered
consumer: **this population's owner/affiliation is reported as open, not as `OrganisationRaid`.** The
structural resemblance to `OrganisationRaid::Type::UnauthorizedVehicle` (org-funds side effect,
`DAT_0018276c`/`FUN_00091f70` target search, `attackBuilding`-shaped spawn) is real and worth a future
session's attention, but is not sufficient on its own to justify branching OpenApoc's shared
`advanceMissionCounterOnArrival` on vehicle ownership.

---

## 3. Closing `U1-ordertype-0x12c.md`'s "4 of 17 `FUN_0005d68c` callers" gap — all 17 examined this session

`FUN_0005d68c` (VA `0x5d68c`, file `0x4d68b`) does not itself set `+0x166` or `+0x15C` — its entire relevant
body forwards its own caller's `BX` straight to `FUN_0005d6e4` (`0x5d6b2 MOVSX EBX,BX`, already bound). Its
own caller list (`getReferencesTo`, re-confirmed this session, `export/u1_reconcile_callers.log`) is
**exactly 17 call sites across 12 distinct functions** — one correction to the prior session's table:
`0x91046` (previously attributed to `FUN_00090d68`, as a 2nd call site) actually belongs to a **different**
function, `FUN_00090fe8` (its own, single call site); `FUN_00090d68` has only the one site at `0x90e5e`. The
count (17) is unchanged; one site's function attribution was wrong.

**What matters for reachability is not the exact `+0x12C` literal these callers pass (which was the prior
session's framing) but whether any of them *also* produces `+0x15C == 1`** — since, per §1, that is the only
way a non-`1` `+0x12C` can ever reach the retarget branch at all. This session checked every one of the 12
functions' decompiled+raw bodies for a `+0x166` write or a `FUN_0005df1c` call after their `FUN_0005d68c`
call:

| Caller | `+0x12C` source (prior session / this session) | Role/`+0x15C` after spawn |
|---|---|---|
| `FUN_00010380` (2 sites) | literal `3` | **overrides role to `6`** (`0x103fc`, file `0xf3fb`... `0x3fb`, then `CALL FUN_0005df1c` at `0x10403`) → `+0x15C=0` |
| `FUN_0005ca04` | memory-sourced, unresolved | **overrides role to `6`** (`0x5cb17`, then `CALL FUN_0005df1c` at `0x5cb1e`) → `+0x15C=0` |
| `FUN_0007a730` | literal `9` | no override → TYPE-default → `+0x15C=0` |
| `FUN_000ab440` | literal `0` | no override → `+0x15C=0` |
| `FUN_00015400` (5 sites) | literal `0` | no override → `+0x15C=0` |
| `FUN_00034860` | memory-sourced, unresolved | no override (no `FUN_0005df1c` call anywhere in its callee list) → `+0x15C=0` |
| `FUN_00092470` | memory-sourced, unresolved | **no `+0x166`/`FUN_0005df1c` path — writes `+0x15C=1` directly** (§2.2) |
| `FUN_0008f3d4` | literal `0` | no override → `+0x15C=0` |
| `FUN_000a1f9c` | literal `9` | no override → `+0x15C=0` |
| `FUN_000a238c` | literal `9` | no override → `+0x15C=0` |
| `FUN_00090d68` | memory-sourced, unresolved | no override → `+0x15C=0` |
| `FUN_00090fe8` | literal `0` (corrected attribution) | no override → `+0x15C=0` |

**Result: of `FUN_0005d68c`'s entire 17-site caller population, exactly one — `FUN_00092470` — ever produces
`+0x15C == 1`, and it does so by an entirely different mechanism (a direct write, bypassing `+0x166`/role
altogether) than every other caller.** All the rest, whatever their exact `+0x12C` literal, are excluded from
ever reaching the retarget branch because their `+0x15C` is `0`. This resolves the prior session's exact
worry — "a single unexamined caller passing something other than `1` collapses the conclusion" — precisely:
one caller does collapse a *blanket* "never retarget for any `FUN_0005d68c` spawn" claim, but it is not one
of the four originally-flagged unresolved callers, it is not an incursion-role population, and it does not
affect `FUN_0006da88` (§1), which never calls `FUN_0005d68c` at all.

---

## 4. Reconciling the two documents, and the concrete OpenApoc fix

**Both documents are partly right and partly wrong, precisely as follows:**

- **`U1-ordertype-0x12c.md` is right about the incursion population** (§1 above proves it more strongly
  than that document itself claimed — not "no writer found" but "structurally cannot be anything but `1`").
  It is **wrong** to the extent its verdict language ("this population always resolves to arrived... never
  retarget", carried into the coordinator note as "the retarget branch is unreachable") reads as a claim
  about the mechanism in general — `FUN_00092470` refutes that broader reading. Its `+0x166`→`+0x15C` model
  (§5.2's table) is also not universal, as it implicitly assumed: `FUN_00092470` sets `+0x15C` directly,
  decoupled from `+0x166`/role entirely, a path outside that model. Its §2.5 caller table also misattributed
  one `FUN_0005d68c` call site (`0x91046`, corrected in §3 above).
- **`U1-arrived-flag-and-0x168.md`'s `0x3ad93` citation is structurally correct, and the path is genuinely,
  routinely reachable** — just not by "ordinary UFO retargeting" in general, and not by the incursion
  population its own §2.6 wrote the finding about (which, per §1, provably never reaches it). Its error is
  scope, not the raw citation.

**The fix for the population the task named.** `VehicleMission::advanceMissionCounterOnArrival`
(`game/state/city/vehiclemission.cpp:3137`) is reached from exactly one call site,
`vehiclemission.cpp:2039`, inside the `MissionType::AttackBuilding` case. `AttackBuilding` missions are
issued from **three** call sites in OpenApoc (corrected count — the initial pass here only read one of
them): `game/state/gamestate.cpp:1025` and `gamestate.cpp:1144` (both inside the incursion dispatch loop,
`owner == state.getAliens()`/`ORG_ALIEN`, `gamestate.cpp:901-902` — `:1025` is the primary-list "Attack"
slot, `:1144` is the secondary/attack-list slot, both alien-owned, both the same `FUN_0006da88` population
in binary terms) and `game/state/shared/organisation.cpp:1347`
(`OrganisationRaid::Type::UnauthorizedVehicle`, an ordinary organisation as owner).

**For the alien/incursion call sites (`:1025`, `:1144`), the fix is unconditional and well-evidenced (§1):**
these vehicles' `+0x12C` is always `1` in the original, so the mission-counter-zero transition always takes
the "leave via nearest dimension gate" branch, never the retarget branch. OpenApoc's current unconditional
`acquireTargetBuilding()` retarget is wrong for this population and should become (or be replaced by)
`VehicleMission::gotoPortal`.

**Whether the `organisation.cpp:1347` (`OrganisationRaid::UnauthorizedVehicle`) call site is the same
population `FUN_00092470` spawns, and therefore whether it should keep today's retarget behavior, is
NOT established (§2.3) — this is the one place this document stops short of a concrete recommendation, per
the task's own instruction not to invent a mapping the evidence doesn't support.** What *is* established:
the original genuinely has a second, non-incursion population whose `+0x12C` is never `1` and whose `+0x15C`
is forced to `1`, for which the retarget branch is real and routinely taken. If a future session confirms
`FUN_00092470`'s spawns are organisation-owned (or alien-owned), OpenApoc's fix follows directly: branch
`advanceMissionCounterOnArrival` on `v.owner == state.getAliens()` — `true` → `gotoPortal`; `false` (or
whichever ownership the confirmation finds retargets) → keep the current `acquireTargetBuilding()` path.
**A blind global replacement of the retarget with `gotoPortal` across all three call sites — the fix
`U1-ordertype-0x12c.md`'s coordinator note contemplated — would be wrong regardless of how §2.3 resolves**:
the original has at least one real population that retargets, so removing the capability everywhere trades
one bug for another. The narrowly-correct, immediately-actionable change is limited to the two
alien-incursion call sites.

The three existing tests in `tests/test_city_rules.cpp` (read this session, `tests/test_city_rules.cpp:606-
782`) construct a bare, synthetic `VehicleMission{type=AttackBuilding}` with no `Vehicle::owner` set — they
exercise the *shared* mechanism, not one population or the other, so they do not by themselves adjudicate
which branch is correct for which owner. They will need a companion test pinning `v.owner ==
state.getAliens()` to the `gotoPortal` behavior once that fix lands; the existing three should keep
describing the (still real, for at least one population) retarget path rather than being deleted.

---

## 5. What remains open

- **Highest priority: which population (alien or organisation) `FUN_00092060`/`FUN_00092470` actually
  spawns (§2.3).** The class-byte check that would have settled this (`0x1285ea+type*0x7e`) turned out not
  to discriminate — it reads `1` uniformly across both alien (type `0`-`9`) and mundane (type `10`-`24`)
  indices. The actual type spawned depends on `FUN_00092470`'s own inner type-selection loop (`dword ptr
  [ESP+0x10e]`), which this session did not trace back to its origin/bound. Tracing that loop — and, ideally,
  finding what the outer scheduler's case values (`1/2/3/6/7/8/9/10`, `DAT_001277a8`/`DAT_001277c6`) actually
  represent — is what would turn §4's conditional fix from "known shape, unconfirmed which side is which"
  into a shippable patch.
- **The `{0,1}`-exclusion guard for `FUN_00092060`'s scheduler-index write** (`0x9240a MOV word ptr
  [EBX],SI`) is confirmed by decompile (`if ((uVar10 != 0) && (uVar10 != 1)) {...}`) and by the surrounding
  raw context (loop bound `CMP SI,0x1b`/`JL`, raw-verified), but the specific `CMP`/`JZ` pair excluding `0`
  and `1` was not independently re-disassembled this session — it sits between VA `0x9206b` (loop top) and
  `0x92300` (start of this session's raw dump), outside the dumped range. Does not affect the core finding
  (indices `2`–`26` are reachable regardless), only the precision of the excluded-value claim.
- **`FUN_00092060`'s own case-dispatch semantics** (`DAT_001277a8`/`DAT_001277c6`, cases `1/2/3/6/7/8/9/10`,
  `DAT_0018276c+0xAA`-style per-org gating) were not chased to a full account — only enough to confirm the
  `+0x12C`/`+0x15C`/mission-counter/`+0x16C` outputs that matter for this reconciliation.
- **`FUN_000b76dc`**, found by this session's `+0x15C` writer scan, writes `+0x15C` from a *computed*
  (non-literal) value and is not among any of the `+0x12C`-writer callers traced above — its relationship
  (if any) to this mechanism is unexamined. Disclosed rather than assumed irrelevant.
- `FUN_00092060`'s own caller was not traced beyond `FUN_0004ab04`/`FUN_0004aa7c` (not opened this session)
  — worth confirming this scheduler truly runs every tick with no other gating this session missed.
- Types `25`–`32` (class `0`) and `33`+ (irregular values, likely past the real type table) in the
  `0x1285ea` catalog dump were not identified — disclosed rather than assumed to be "ground vehicles" or any
  other guess.

---

## Coordinator note

This reopens `U1-ordertype-0x12c.md`'s "Coordinator note — not implemented, and why." Both blocking items
from that note are now resolved: (1) the contradiction with `U1-arrived-flag-and-0x168.md` §2.6 is
reconciled above — both documents were partly right; (2) the `FUN_0005d68c` caller gap is closed (§3), and,
more importantly, superseded by a stronger, non-census-dependent proof for the specific population the task
asked about (§1). **What can ship now:** an owner-gated fix at the two alien-incursion `attackBuilding` call
sites (`gamestate.cpp:1025`/`:1144`) — always `gotoPortal`, never retarget — plus a new test pinning
`v.owner == state.getAliens()` to that behavior, alongside (not replacing) the three existing tests, which
still correctly describe a retarget path the original really has for at least one population. **What is not
ready to ship:** any change to the `organisation.cpp:1347` (`OrganisationRaid::UnauthorizedVehicle`) call
site's behavior, or to `advanceMissionCounterOnArrival`'s default/fallback case — §2.3/§5 leave open which
real population that path corresponds to, and per the prime directive that gap should be closed by
reverse-engineering `FUN_00092470`'s inner type-selection loop, not by guessing.
