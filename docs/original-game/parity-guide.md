# OpenApoc parity guide

Every gap between the original X-COM: Apocalypse binaries and this fork that is **still open on
`khallmark/FixShitUp`**, with the work needed to close each one.

Extends [openapoc-gap-matrix.md](openapoc-gap-matrix.md) (what the gaps are) and
[next-implementation.md](next-implementation.md) (which are next). This file is the *how*.

---

## 0. Prime directive

> **A row is not done until the lock test fails without the change. Items with no recovered
> constant or consumer must not have one invented.**
> — [next-implementation.md](next-implementation.md)

This is the constraint that shapes every entry below, and it is why most of this guide is not
C++ instructions. The project has spent significant effort *removing* invented numbers
(`micronoidRainChance`, the role-8 arrival RNG, the always-zero retreat formula). Re-adding a
plausible guess to close a row is a regression even when it makes the game feel better.

So each gap is classified by **what is actually blocking it**:

| Class | Meaning | The work is |
|---|---|---|
| **A** | Evidence exists, or none is needed. | Write code and a lock test. |
| **B** | Blocked on reverse engineering. A constant, table or consumer is not yet bound. | Ghidra first ([§2](#2-workflow-b--binding-a-constant)), then code. |
| **C** | No evidence exists in the binaries at all — the feature is prior-art or folklore. | A product decision, not an implementation task. |

**Do not promote a Class B row to Class A by picking a number that looks right.** If the binding
work fails, the correct outcome is that the row stays open.

### Run results — `khallmark/parity-implementation`

R&D and implementation run of 2026-08-24. Raw verdicts in [findings/](findings/); run log in
[findings/STATUS.md](findings/STATUS.md).

| Item | Verdict | Outcome |
|---|---|---|
| **A1** multi-tile units | **FIXED — and the FIXME was mostly stale** | Enumerating breakage first paid off: pathfinding across a 1-tile gap, doors, occupancy at rest and projectile collision **already worked**. Four real bugs, all one geometric mistake — probing the block's degenerate shared-corner point instead of its occupied tiles. Fixed: missing goal-block tile during movement; `getEyeLocation` putting a large unit's eyes half a tile off its own muzzle; and `calculateVisionToUnit`/`hasLineToUnit` replaced with genuine any-occupied-tile checks, deleting the 0.75-tile "caught up in ground" nudges. Draw-order sorting is **reported unverified**, not claimed fixed — no rendering harness exists. |
| **V2** ground-vehicle order defect | **FIXED** | Root cause found in `VehicleMission::setPathTo`: a severed road yields a *non-empty but short* path that fell through both give-up branches — near targets crashed an undamaged vehicle, far targets looped forever. Lock test on the real extracted city. Suite 31/31. |
| **F1** hazard spread RNG | **IMPLEMENTED — with the finding corrected twice** | `FUN_0001eee8` (VA `0x1EEE8`, file `0x7998C`) is a **precomputed 10,013-entry lookup table, not an LCG**. `FUN_0007B0D0` (file `0xD5B74`) decompiled: fire spread is `RNG(0..10) + inherited baseline` vs a per-terrain resistance byte from `FUN_0007AA8C`, behind two veto lookups, neighbour direction drawn `RNG(0,4)` from a table at `0x293068`. **`HAZARD_SPREAD_CHANCE` can be deleted on this evidence.** One open question — see below. |
| **B5** enzyme | **PARTIAL** | Confirmed: a real **4-way jump table** (`FUN_0007B610`) dispatches overlay type 1 / 2 (fire) / 3 to peer stage-advance functions sharing one decode triplet, one encode triplet, and a generalized placement engine (`FUN_0007AE78`) of which fire's is a special case. Not bound: which of type 1/3 is Enzyme (dispatch variable has zero static xrefs), and the armour-damage formula. **Not guessed.** |
| **G1 · Disruptor Shield (0x08)** | **FIXED — and the premise was wrong** | *Not* a missing feature. `useItem` returning `false` is **correct** — the shield is passive, like `CloakingField`. A shield-absorption path has existed since 2016 (`Agent::getFirstShield`, and the "Hit shield if present" block at `battleunit.cpp:1776`); it was simply **wrong**: it always returned true after any hit (infinite absorption, no overflow), used the item's own `damage_modifier` instead of the general pipeline, and destroyed the item on depletion despite the shield being rechargeable. Chain traced to the general damage-application function by caller trace. All four numbers now bound: regen **+1 per 36 vanilla ticks (once per real-time second)**; full recharge **at battle load only, not periodic**; damage-type modifier is **not shield-specific** (existing `damage_type_data` + a table adjacent to `damage_modifier_data`, applied upstream); and absorption is **all-or-nothing** — see the trap below. **Second pass:** only one of the two bound writers of the item's charge field had been implemented. `FUN_0006508C`'s decrement on absorption was in, but `FUN_0006511C` — which regenerates the item's own charge by 1 per dispatcher call, gated `< 100` — was not, so absorption lowered the item charge while regen raised only the unit buffer and the two drifted apart; unequip/re-equip snapped a fully regenerated shield back to its drained value. Also excluded the shield from `AEquipment::updateInner()`'s generic recharge, which was a *second* regen of the same field at one per four seconds, from hardcoded 2016 extractor literals rather than the binary. |
| **G1 · MultiTracker (0x04)** | **BOUND** *(upgraded from inconclusive)* | Traced one hop past the local cluster to a builder with six live call sites across five functions, invoked in the same init block as the confirmed-live Motion Scanner chain and behind the same feature flag. A real shared subsystem, not dead code. |
| **G1 · Mind Shield (0x05)** | **RECONFIRMED** | Re-bound at a fresh address; logic unchanged. Resolves the audit item below — the old citation failed because **Ghidra addresses drift between import sessions**, not because the binding was wrong. |
| **G1 · Vortex Analyzer (0x03), Structure Probe (0x02), Alien Detector (0x07)** | **NOT BOUND — clean negatives** | No reader anywhere in a full 20-function survey of general-type consumers. Dead in the original too. |
| **G1 · Dimension Force Field (0x0b)** | **NOT BOUND** | UI-picker fallback only, no effect consumer. |
| **B3** wounded penalty | **NOT BOUND — closed** | Five independent methods exhausted, including a cross-check against the pointer table discovered *after* the first pass. Wound-counter struct offset not established. No magnitude, split or body-part specificity invented. |
| **K1** cloak | **NOT BOUND — closed negative** *(was OPEN LEAD; the 20 unexamined functions have now been examined)* | All 19 functions reading `agent_general_data`'s `type` field enumerated and characterised. Three recognize type `0x0a` — a UI status-icon picker, the AI equip-priority `×0xc` scoring case, and a previously undocumented AI order-preference check whose downstream handler traces to a plain per-unit decay counter with no cloak-specific behaviour. **None implements concealment.** No accumulator, threshold-compare, is-concealed flag, detection-radius modifier or break-on-fire logic exists. Decisive: `FUN_00066474`, the per-unit tick dispatcher hosting Mind Shield's `+30/cap 200` and the Disruptor Shield's regen, has **no `0x0a` case at either hand slot** — the one function that would host a cloak tick threshold does not have one. The cloak is structurally identical to the G1 dead gadgets: catalogued, inventoriable, consulted by UI/AI bookkeeping, zero runtime effect. OpenApoc's `CLOAK_TICKS_REQUIRED_UNIT` is forum prior art with no original-game basis, now labelled as such at the constant. *(An initial mid-session misreading of a decay counter as a dispatch key was caught in review and corrected in the finding rather than hidden.)* |
| **K1** cloak | **NOT BOUND** | No reader of the `agent_general_data` type field found. Raised a separate audit item — see below. |
| **O1** bribe / rift | **NOT BOUND** | The binary's one relation-adjustment primitive `FUN_0005faf0` was walked to its roots; none touch an org funds field. Stays prior-art. |
| **O2** cargo seize | **PARTIAL** | Org `+8` **confirmed a funds field, not relation**. Event-type → cargo mapping still unbound; there is no path from `Building::updateCargo`'s seize check into any of the four event types. `Cargo::seize` still must not be wired from it. |
| **M1** city music | **NOT BOUND** | A real `getnextmusic` consumer was found and a call edge traced from the UFO mission-arrival handler — but `FUN_000b523c` has **~49 call sites** across menus, base screens and mission code, so that edge is indistinguishable from routine per-screen polling. Not a city-combat trigger. *(Corrected: an earlier pass in the same session read a truncated log as 2 callers and reported BOUND.)* |
| **U1(a)** mission-counter zero-transition | **BOUND** | `FUN_0003a910` branches on the post-decrement value: one path sets an arrived flag, the other picks a new target building (`FUN_00091f70` → `FUN_0004db84` → `FUN_0004e0d4`), or resets role/order state when none is found. Implementable. |
| **U1(a)** arrived-flag branch | **IMPLEMENTED — and it corrected shipped behaviour** | Refereed inside the Ghidra project after two findings disagreed; both were partly wrong. `+0x12C == 1` is a **structural invariant** for incursion-spawned UFOs — `FUN_0006da88` commits the role to `+0x166` then reclobbers `BX` with a literal `1` immediately before the writer, and two independent exhaustive writer censuses found nothing that touches `+0x12C` on an already-spawned vehicle. So the retarget branch is **unreachable** for that population: these craft always leave via the nearest dimension gate. OpenApoc retargeted them unconditionally. Now owner-gated to `VehicleMission::gotoPortal` (which already picks the nearest portal, matching `FUN_0005d360`'s scan). The retarget branch stays for everything else — it is real code in the original, for a *different* population (a periodic scheduler, `FUN_00092060` → `FUN_00092470`, unrelated to the incursion system, writing `+0x12C` from its loop index). **RESOLVED:** that scheduler *is* the `OrganisationRaid::UnauthorizedVehicle` population — the value becoming `+0x12C` is an organisation-table index (written from `FUN_00092060`'s outer loop over the same 27-entry `0x1b6`-stride org table bound in O1), and a raw `CMP SI,1/JZ` at the loop top excludes org indices 0 and 1, exactly `ORG_XCOM`/`ORG_ALIENS` — the same set OpenApoc gates `setRaidMissions` on via `initiatesDiplomacy`. **No change needed** at that call site; the owner gate is already the correct split. One named divergence: the binary spawns a fresh vehicle against an abstract inventory counter where OpenApoc dispatches an existing one from the org fleet. Lock test verified red with the branch disabled; the three existing retarget tests kept unchanged, because they still describe a path the original really has. |
| **U1(a)** arrived-flag branch *(superseded row, kept for the record)* | **BOUND — the sibling branch finally has a reader** | `FUN_0003a910`'s order-type-1 branch sets vehicle `+0x16A = 1` (VA `0x3ad11` / object-page file `0x2ad10`). Its reader is `FUN_00059148` (file `0x49147`), 9 call sites confirmed twice via `getReferencesTo`, fully raw-disassembled. Three outcomes, described by offset rather than named: rendezvous with the nearest active same-side vehicle whose `+4` matches this vehicle's `+0x16C`, with per-axis RNG jitter; or, if `+0x16C` was never set, commit a fresh destination from the per-side catalog block; or re-arm for the next call. `FUN_000588f8` — the U1(b) gate's own function — independently sets the **same** flag once its thresholds pass, so U1(a) and U1(b) converge on one flag and one reader. Also corrected a citation in the earlier write-up: the pre-decrement "unset sentinel" guard tests `+0x16C`, not `+0x16A`. **Mappability to OpenApoc deliberately NOT asserted** — see the guide's U1 section for the two prerequisites that must be checked first. |
| **U1(b)** `+0x168` vs constitution gate | **BOUND — and the earlier "unreachable" reading is superseded** | The previous pass concluded `+0x168` was a fixed fraction of spawn-time constitution that nothing ever raised, making the gate fire only if constitution fell. Both halves of that are now wrong. Constitution *does* take damage (`FUN_00057c8c`, `SUB EDI,EAX` @ VA `0x581e0`, called from outside the mission subsystem) and is repaired (`FUN_0006cb8c`, clamped to a per-type ceiling). And `+0x168` is **not** fixed after spawn: `FUN_0005df1c`/`FUN_0005df98` recompute it from a live formula on ordinary UFO retargeting (VA `0x3ad93`), right after the existing clamp-down, overwriting it. For that call site the inputs were read straight from the binary: 75% × a per-type catalog word that `FUN_0006cb8c` independently confirms is constitution's repair ceiling — making the gate a concrete "below three-quarters of ceiling" threshold rather than an unreachable one. |
| **U1(b)** *(superseded row, kept for the record)* | **MECHANISM BOUND, SEMANTICS NOT** | Three spawn-time writers traced; the `FUN_0006da88` formula resolves to `constitution × percent / 100`, so `+0x168` is a **fixed fraction of constitution that nothing ever raises**. The gate can therefore only fire if constitution itself later falls. No name asserted for the field. *(Corrected mid-run from a "regenerating stat" reading caused by a misread multiplicand.)* |
| **U2(a)** `DAT_000e0cc0` override | **CLOSED** | Reads and clear sites bound; **no set site exists anywhere in the binary** — all six xrefs enumerated exhaustively. The override is never activated in the original, so OpenApoc needs nothing here. |
| **U2(b)** exposure event types 1 & 4 | **BOUND in the binary, NOT MAPPABLE here** | The dispatch and branch logic are genuinely recovered — but that is a claim about the *original's control flow*, not about OpenApoc having a seam to receive it. Attempted and **stopped without writing code**: of the three pieces needed, the ×5 formula is **already shipped and locked** (`Base::alienExposureRollSucceeds`, locked by `alien_exposure_threshold`); the moved-count source is unbound (`Organisation` has no species/population field — only `Building::current_crew` does, and building↔building transfer is a *different* original function, already implemented as `Building::alienMovement`); and the trigger point is unbound (who calls the dispatcher was never traced — the link to the discriminant is a shared global, not a call edge). Two of three missing. |
| **V1** vehicle attack-mode dodge | **NOT BOUND — definitively** | All ten engagement/dodge UI strings reverified at zero bound xrefs, **and** a full-binary byte-pattern search for a `{10,50,80,100}` table across 8 pattern variants found **zero matches**. There is no engagement table. OpenApoc's hardcoded ladder is an invention with no original counterpart — leave it, and label it as such. |
| **B1** cover metric | **RE-OPENED** | First verdict rested on an invalid control; see below. Agent resumed with structural entry. |
| **C1** umbilical · **C4** Apocalypse attack | **CLOSED** | Confirmed absent from *both* binaries. |
| **C3** late-campaign bombing | **CLOSED** | No trigger; escalation already explained by the weekly growth and preference tables. |
| **C2** secondary objectives | **NOT BOUND — clean triple negative** | The three mechanics the extracted briefings describe are absent from the original too, so no code should be written from them. *Queen live capture:* real, but generic — research records 32/33 gate `RESEARCH_QUEENSPAWN` on the live specimen and `_AUTOPSY` on the dead one, and all 13 alien species have the identical pair with no anomaly in the Queen's row. Already implemented. No score bonus or relation change singles her out. *Sectoid rescue → Mutant alliance:* TACP's debrief screen has 9 fixed line items and no rescue/civilian field; `Sectoid` appears exactly once in the whole binary — the briefing prose itself. All four direct writers of UFO2P's org-relation matrix examined (going beyond O1's earlier caller-walk, and finding a previously-unexamined organisation-founding event); none is reachable from a tactical battle result. *Evacuation phase:* no countdown, timer or self-destruct text anywhere in TACP. The `units will be lost if left in combat zone` string was traced through its pointer-table slot to its resolver, and its own neighbours in the packed pool are `Abort mission?` / `Yes` / `No` — it is the standard abort-mission dialog, matching OpenApoc's existing `winnerHasRetreated` retreat, not a scripted post-disable timer. |
| **C2** mushrooms / briefings | **IMPLEMENTED** | Objective *mechanic* already worked. The residual — ten unextracted TACP briefings — is now extracted and shown pre-battle. Offsets read from the binary rather than trusted, both CRC32s matched, and the −0x2200 4-build slide was **measured byte-for-byte**, not assumed. |
| **U1(a)** mission counter | **IMPLEMENTED — with named deviations** | `advanceMissionCounterOnArrival` wired into `AttackBuilding`'s per-arrival re-entry, with the incursion spawn sites passing the extracted `+0x1B`. Four parity deviations declared rather than hidden — see below. **Second pass:** the original three tests drove `advanceMissionCounterOnArrival()` directly and left the *call site* in `VehicleMission::start()` untested — deleting that one line failed nothing. `ufo_mission_counter_decrements_from_mission_start` now drives the real `start()` on a UFO placed in the extracted `CITYMAP_HUMAN`, and was verified red with the call site removed. |
| **A2 · A4** test seams | **CLOSED — the first attempt had locked nothing** | Both rows shipped tests that could not fail: A2 asserted local constants against themselves because `getPsiCost()` had internal linkage from a namespace-scope `static` declaration in a header (a latent undefined-reference for any caller, independent of the test), and A4 froze local copies of `cth*damage/time` and of the AOE friendly-fire rule. Four functions promoted to public statics (`BattleUnit::getPsiCost`/`getPsiAttackChance`, `UnitAIVanilla::attackPriority`/`blastDamageContribution`/`aoeIsWorthThrowing`), following `TacticalAIVanilla::retreatChancePercent()`. All three locks verified red by mutation. |

**Two honesty notes carried from the implementations, neither a blocker:**

1. **U1(a) leaves its production call site untested.** The three lock tests call
   `advanceMissionCounterOnArrival` directly — deterministic, no pathfinding dependency — so
   **deleting the one-line hook in `start()` would not fail any test.** The helper's contract is
   locked; the wiring is not. Covering it needs a test that drives pathfinding, which was
   deliberately avoided for determinism. Declared, not discovered.
1b. **A fifth potential deviation was checked and is NOT one.** The byte-exact re-disassembly of the
   write site shows the whole mission-counter block is guarded on `+0x16C == -1` — a vehicle that has
   a follow type never decrements its counter at all. OpenApoc's `advanceMissionCounterOnArrival`
   has no such guard, which looks like an undeclared divergence until you check the data: of the 45
   `UFO_mission_data` records, **zero non-Escort slots carry a `follow_slot`**, and OpenApoc only
   wires `followVehicleType` for escorts, which are issued `arriveFromDimensionGate` +
   `FollowVehicle` and never an `AttackBuilding` mission — so they never reach the hook. The guard is
   satisfied structurally in both directions. Recorded so the next reader does not "fix" it.
   *(25 Escort slots do carry a nonzero `mission_counter`; in the original that byte is dead data,
   because the guard skips them. OpenApoc never applies it either.)*
2. **U1(a) has three further declared deviations**: an off-by-one on first entry (a counter of N
   yields N−1 arrivals, mirroring `Patrol`'s existing shipped convention); no counter reset after
   retargeting (the bound-only choice — no writer resets `+0x171` after the spawn-time copy); and
   an unreachable-in-practice edge case shared with `Patrol`. The sibling "latch an arrived flag"
   branch is **still not implemented, but the reason has changed and narrowed.** It is no longer
   "the gating field's semantics are NOT BOUND" — that referred to `+0x16C`, which is now bound (it
   is `followVehicleType`, already extracted) and to a supposed missing destination catalog, which
   does not exist: the branch's destination is a **dimension gate**, and OpenApoc already has
   `City::portals` / `MissionType::GotoPortal` / `Vehicle::leaveDimensionGate`. The mechanism is
   fully bound and the seams exist.
   What blocks it now is one field: `+0x12C`, the discriminator that decides *which* UFOs leave
   (`== 1`) rather than retarget (anything else). Its **values** have never been traced to a writer
   — "order-type" is a label inherited from an earlier write-up in this project, not a bound
   meaning, and this project has already had one inherited label (`building_function` for `+0x1B`)
   turn out to be flatly wrong. Implementing the split without binding it would mean guessing which
   UFOs leave the map, which is exactly the invention §0 prohibits. Binding `+0x12C`'s writers is
   the single remaining prerequisite.

**A second category error, mine: "inert" ≠ "unimplemented".** I briefed the Disruptor Shield as a
missing mechanic on the strength of `useItem` returning `false` for it. That was wrong, and the
implementer said so rather than building to the brief. `useItem` returning `false` is *correct* for
a passive item; the absorption path was elsewhere and had been for years, quietly doing the wrong
thing — **infinite absorption**, which is a considerably worse bug than a missing feature and would
never have been found by looking where I pointed.

Checking one entry point is not checking a feature. Grep for the *type*, not the *verb*.

**A category error worth naming: "BOUND" ≠ "implementable".** U2(b) went into implementation on
the strength of a `BOUND` verdict and came back with no code, correctly. A findings verdict of
`BOUND` means *the original's behaviour has been recovered*. It does **not** imply OpenApoc has a
seam to receive it, that the surrounding data model exists, or that the recovered piece is not
already shipped. All three failed here.

Before queueing a `BOUND` row for implementation, check three things: **is the formula already
implemented?** **does the data it reads exist in our model?** **is the trigger point traced, or only
the thing that sets a flag it shares?** Answering those costs minutes; skipping them cost an
implementation attempt.

**A decompiler trap worth generalising.** The Disruptor Shield's overflow behaviour was first
written up as "partial absorb, remainder passes through" — the natural reading, and **wrong**. It
is **all-or-nothing**: full absorb if `shield > damage`, otherwise the shield is zeroed *and the
entire damage passes through unreduced*. The decompiler's C **reorders the zeroing ahead of the
subtraction that reads it**, which inverts the semantics. Only the raw instruction listing shows
the real order.

Generalise this: **for anything where ordering carries meaning — absorption, clamping,
accumulate-then-test — read the listing, not the decompiled C.** This is the second semantic
inversion caught this run by re-reading raw output (the other was U1(b)'s multiplicand).

**F1's "open question" turned out to be a typo — and the guard I demanded was protecting it.**
I insisted the dead `(0,0,0)` neighbour outcome be reproduced exactly and guarded by a lock test
named `fire_neighbour_table_preserves_dead_outcome`, on the reasoning that it looks like a bug and
someone would tidy it. The byte at `0x29306C` reads **`1`, not `0`**. Fire's table is
**East/South/West/North/Up — a clean five-direction set with no dead slot.** All 19 other values in
both tables matched the finding exactly, so this was one transcription error, not a bad method.

The implementer re-read the bytes, re-confirmed the base addresses from live disassembly, corrected
it, and **renamed the test to `fire_neighbour_table_matches_recovered_bytes`** — asserting the real
bytes rather than a defect that does not exist. Had it built to my brief, the codebase would now
carry a deliberately-wrong neighbour table with a test cementing it in place.

The same run also falsified the finding's core characterisation: **the RNG is not a precomputed
table baked into the binary.** `DAT_001B2D70` reads as static zero and is BSS-shaped, filled at
runtime by a routine nobody has located. There is nothing to embed and no index to serialize, so
the implementation ports the primitive's *contract* over `state.rng`, declared in code as a
substitution rather than the original sequence.

**A second category error, mine: "inert" ≠ "unimplemented".** I briefed the Disruptor Shield as a
missing mechanic on the strength of `useItem` returning `false` for it. That was wrong, and the
implementer said so rather than building to the brief. `useItem` returning `false` is *correct* for
a passive item; the absorption path was elsewhere and had been for years, quietly doing the wrong
thing — **infinite absorption**, which is a considerably worse bug than a missing feature and would
never have been found by looking where I pointed.

Checking one entry point is not checking a feature. Grep for the *type*, not the *verb*.

**A category error worth naming: "BOUND" ≠ "implementable".** U2(b) went into implementation on
the strength of a `BOUND` verdict and came back with no code, correctly. A findings verdict of
`BOUND` means *the original's behaviour has been recovered*. It does **not** imply OpenApoc has a
seam to receive it, that the surrounding data model exists, or that the recovered piece is not
already shipped. All three failed here.

Before queueing a `BOUND` row for implementation, check three things: **is the formula already
implemented?** **does the data it reads exist in our model?** **is the trigger point traced, or only
the thing that sets a flag it shares?** Answering those costs minutes; skipping them cost an
implementation attempt.

**A decompiler trap worth generalising.** The Disruptor Shield's overflow behaviour was first
written up as "partial absorb, remainder passes through" — the natural reading, and **wrong**. It
is **all-or-nothing**: full absorb if `shield > damage`, otherwise the shield is zeroed *and the
entire damage passes through unreduced*. The decompiler's C **reorders the zeroing ahead of the
subtraction that reads it**, which inverts the semantics. Only the raw instruction listing shows
the real order.

Generalise this: **for anything where ordering carries meaning — absorption, clamping,
accumulate-then-test — read the listing, not the decompiled C.** This is the second semantic
inversion caught this run by re-reading raw output (the other was U1(b)'s multiplicand).

**F1's open question, which must survive implementation:** fire's neighbour table yields
South/West/North/Up **plus one dead `(0,0,0)` outcome** — not a clean 5-direction set. Whether that
is authored behaviour or an off-by-one in the original binary is **undetermined**. Preserve it
exactly; do not tidy it into four directions or five. Tidying it would be inventing behaviour.

**A confidence upgrade, free.** Chasing the shield's damage-type modifier produced an independent
**code-reader** confirmation of the table adjacent to `damage_modifier_data` (`0x30165C`). That
entry is currently `near_first_of_2 / low` in the lab's `tacp_rebase.csv` — located by proximity
heuristic, not by a consumer. A traced reader is stronger evidence than proximity, so **whoever
owns that extractor can raise its confidence**, citing the shield investigation.

Audit items this run produced, all about **existing** claims rather than open rows:

1. **TACP string-anchoring gives false negatives.** `0x2DE000`–`0x2E2FFF` is a packed
   variable-length pool whose entries can never carry a direct xref; `0x2F2000`–`0x2F3400` is a
   fixed-stride asset-name table whose entries can. Using one as a control for the other produced a
   wrong `NOT BOUND`. See [findings/METHOD-tacp-string-regions.md](findings/METHOD-tacp-string-regions.md).
   **Every future TACP negative must name the structural method exhausted, not cite absent xrefs.**
2. **The Mind Shield citation does not resolve — now explained, and it generalises.** The gap
   matrix records Mind Shield on `TACP FUN_0009b780 @ 0x9B780`, which cites a Ghidra VA twice
   rather than a file offset. It was re-bound this run at a **different** address with the logic
   unchanged, because **`FUN_*` addresses drift between Ghidra import sessions.**

   So the binding was always sound; the *citation form* was not. This is not one bad row — **every
   `FUN_*` VA recorded anywhere in this folder is an unstable reference**, and the project's own
   rule ("cite binary + generation + file offset") is exactly the defence against it. Treat a
   `FUN_*` name as a convenience label for the current session only, and re-locate by file offset
   before trusting any address in these docs.

### Status at a glance

The gap matrix has **52 rows; 32 are clean**. The 20 open rows expand to **23 work items** here,
because three matrix rows split (the fire row separates from enzyme; the incursion row separates the
mission counter from base exposure) and two Class C items come from
[subsystems/city-alien-dimension.md](subsystems/city-alien-dimension.md) rather than the matrix.

| Class | Items | IDs |
|---|---|---|
| **A — implementable now** | 4 | A1 multi-tile units · A2 psionics parity · A3 TU-reservation parity · A4 attack-priority parity |
| **B — blocked on RE** | 15 | B1 cover · B2 evasive · B3 wounded penalty · B4 wounded medkit · B5 enzyme · F1 fire remainder · K1 cloak · G1 dead gadgets · V1 vehicle dodge · V2 ground engagement · O1 bribe/rift · O2 cargo seize · M1 city music · U1 mission counter · U2 base exposure |
| **C — no evidence** | 4 | C1 umbilical · C2 mushroom feedback · C3 late bombing · C4 Apocalypse attack |

Class B dominates, and that is the honest shape of the remaining work: **this is a reverse-
engineering backlog with a small amount of C++ attached**, not the other way round.

---

## 1. Workflow A — closing a code-only gap

1. **Write the lock test first**, in `tests/`, and confirm it **fails**. A test that passes before
   the change proves nothing.
2. Register it in [tests/CMakeLists.txt](../../tests/CMakeLists.txt) with `add_openapoc_test(name)`,
   or `add_openapoc_test(name ARGS ${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0 ${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)`
   when the test needs a loaded campaign.
3. Use [tests/test_helpers.h](../../tests/test_helpers.h): `applyDeterministicTestConfig()`,
   `loadStartedGameState(state, common, gamestate)`, `TEST_CHECK`, `TEST_REQUIRE`,
   `runTestSuite({{"name", fn}, …})`.
4. Implement.
5. Re-run; the test must now pass and must still fail if you revert the change.
6. Update [openapoc-gap-matrix.md](openapoc-gap-matrix.md) and re-run
   `python3 tools/regen_compare_report.py`.

## 2. Workflow B — binding a constant

This is the bottleneck for eleven rows. The lab is the sibling repo
`/Users/khallmark/Desktop/Code/OpenSource/OpenApoc-og-research`; **never copy EXEs, ISOs or
`.rep` databases into this tree** ([README.md](README.md)).

```bash
cd /Users/khallmark/Desktop/Code/OpenSource/OpenApoc-og-research
./scripts/ghidra_env.sh --print                      # verify JDK 21 + Ghidra 12.1.3
./scripts/import_le.sh canonical/TACP.EXE            # bound LE, community LX loader
./scripts/import_le.sh canonical/UFO2P.EXE 0x13EE80  # optional file-offset dump
```

Rules that are easy to get wrong:

- Use `-processor x86:LE:32:default -cspec gcc` so the LX loader wins. **Do not** unbind the
  executable — every extractor offset in this repo is a *bound-file* offset.
- **Do not** force analysis of the DOS4GW stub.
- Ghidra's displayed addresses are **virtual addresses, not file offsets.** Every citation must
  carry binary + generation + file offset. Use `labels/ufo2p_rebase.csv` / `tacp_rebase.csv` and
  `labels/offset_to_va.txt` to convert.
- The ISO **non-4** pair is extractor-canonical. The Steam depot ships the **`4`** build under the
  un-suffixed names. Confirm at the non-4 offset, then relocate on the 4 build —
  [exe_slide.h](../../tools/extractors/common/exe_slide.h) maps UFO2P `0xdbd3b41d` → `+0xE00` and
  TACP `0x3ec9c268` → `−0x2200`, but **never assume a global slide**; `crew_ufo_downed` is the
  known exception.

The binding sequence for any unknown:

1. **String first.** `grep` `export/strings/TACP_strings.txt` (or `UFO2P_strings.txt`). A printable
   label is the strongest cheap evidence. **Absence is also evidence** — it is why C1/C2 are Class C.
2. **Xref the string.** If bound xrefs are empty, the string is UI copy and proves nothing about a
   consumer. Several rows died here already (`Rules of engagement` `0x152D10`, `Evade Fire`,
   `Reload time:` `0x151086`).
3. **Find the reader, not the table.** A table with no consumer cannot be wired. The recurring
   failure mode in this project is finding plausible bytes and guessing their meaning.
4. **Decompile the consumer** with `scripts/DecompileSites.java` / `QueryFunctions.java`, adapting
   an existing `Query*.java` — they are short and templated.
5. **Confirm on both generations** before writing an extractor.
6. **Write the extractor** in `tools/extractors/`, with the struct in `tools/extractors/common/`.
7. **Do not** put extracted list tables under `data/common_patch/gamestate/` — `loadGame` *appends*
   vectors, which silently doubles fleet and spawn counts. Reference dumps belong in
   [exe-tables/](exe-tables/).

**Evidence ranking, strongest first:** `table` → `string` → `xref` → `decompiler` → `prior-art` →
`openapoc-todo`.

---

## 3. Class A — implementable now

### A1 · Multi-tile (large) unit pathing and drawing

*Gap matrix: "Multi-tile unit path / draw — missing (prior-art only), medium. Evidence:
openapoc-todo; no TACP path/draw table recovered." Issue #265.*

**This is the one Class A row that is real engine work rather than a test.** It needs no recovered
constant: large units are an OpenApoc capability gap, not a fidelity-of-numbers gap. Megaspawn and
Multiworm are `bodyType->large`, and the code paths that handle them are marked provisional.

**Current behaviour**

- `BattleUnit::isLarge()` — [battleunit.h:641](../../game/state/battle/battleunit.h#L641).
- [battleunit.h:677](../../game/state/battle/battleunit.h#L677) carries
  `// FIXME: This likely won't work properly for large units`.
- [battleunit.cpp:636](../../game/state/battle/battleunit.cpp#L636) and
  [:1045](../../game/state/battle/battleunit.cpp#L1045) apply ad-hoc "offset search for large units
  as they can get caught up in ground".

**Steps**

1. **Enumerate the breakage first.** Add a scratch test that spawns a large unit and drives it
   through: path across a 1-tile gap, path through a door, occupy/vacate, LOS from each occupied
   tile, projectile collision against each occupied tile, shadow/draw order. Record which fail.
   Do not start fixing before this list exists — the FIXME is vague and the real defects may be
   fewer or different than expected.
2. **Make occupancy explicit.** A large unit occupies a 2×2×2 block. Introduce
   `BattleUnit::occupiedTiles()` returning the full set, and route every
   `tileObject->setPosition` / occupancy query through it rather than through the origin tile.
3. **Pathfinding.** In `game/state/tilemap/`, the cost function must reject a step whose destination
   *block* is not entirely passable, not merely its origin tile. This is where the "caught up in
   ground" offsets are compensating for a wrong query — remove them once the block query is correct,
   and confirm the offsets were the only thing holding it together.
4. **LOS and targeting.** Visibility to a large unit should succeed if any occupied tile is visible;
   line-of-fire from one should originate at the unit's centre, not its origin corner.
5. **Draw order.** Large units must sort against scenery per occupied tile.

**Lock test** — `tests/test_battle_large_unit.cpp`, registered with the two gamestate `ARGS`:

- `large_unit_occupies_block` — all tiles of the 2×2×2 block report the unit.
- `large_unit_path_rejects_narrow_gap` — a 1-tile corridor is not pathable.
- `large_unit_los_any_tile` — visibility succeeds from any occupied tile.
- `large_unit_no_ground_snag` — a straight walk across flat floor produces no vertical correction.

**Do not** invent a TACP "large unit table". None is recovered, and none is needed — this is engine
geometry, not a game-balance constant.

**Step 5 (draw order) — probed and left open, deliberately.** A second pass tried to close this and
concluded it cannot be, for a better reason than "no rendering harness".

The tile *selection* that feeds the sort is already block-aware:
`TileObjectBattleUnit::setPosition()` sets bounds from `bodyType->size`, `TileObject::setPosition()`
derives `intersectingTiles` from those bounds plus `centerOffset`, and
`TileObjectBattleUnit::addToDrawnTiles()` picks the topmost of them ("units are drawn in the topmost
tile their head pops into") rather than defaulting to the origin corner.

What is missing is not a large-unit property at all: `TileObject::addToDrawnTiles()` gives **every**
object a single `drawOnTile`, so no object of any type is sorted per occupied tile. Splitting one
unit's sprite across the sort orders of four ground tiles is a change to the isometric renderer for
every object type — out of scope for this row, and not a defect in it.

A candidate lock test was written for the block-confinement invariant and then **discarded**,
because two independent control mutations — collapsing the large-unit bounds to `{1,1}`, and
changing its `centerOffset.z` from `1.0` to `0.5` — both left it green. At integer positions the
half-extent straddles the tile boundary either way, so the intersecting-tile span is identical for
bounds 1 and 2; and the "exactly one draw tile, inside the block" invariant is structural, since
`addToDrawnTiles()` can only ever choose from `intersectingTiles`. No realistic regression fails it.
Per §0 that is not a lock test, so it was not committed. This row stays **UNVERIFIED**, and the
above is why.

### A2 · Psionics timing parity

*Gap matrix: "implemented (parity unverified), medium."*

The numbers are already recovered — in prior-art, not the binary — and are implemented. What is
missing is anything that **holds them in place**. `TICKS_PER_PSI_CHECK = TICKS_PER_SECOND / 2`
([battleunit.h:41](../../game/state/battle/battleunit.h#L41)) is exactly the kind of constant that
a later TPS refactor silently breaks.

From [psionics.txt](../../tools/extractors/docs/psionics.txt), applied each half-second:

| Attack | Initial cost | Upkeep / sec |
|---|---|---|
| Control | 32 | 8 |
| Panic | 10 | 4 |
| Stun | 16 | 10 |
| Probe | 8 | 6 |
| *(regen)* | — | +1 |

**Steps**

1. Write the lock test below and confirm it passes today — this is the rare case where a passing
   test is the deliverable, because it is a **freeze**, not a fix.
2. Assert against the `psionics.txt` values by name, with the file cited in a comment, so a future
   editor sees the provenance.
3. Add an assertion that `TICKS_PER_PSI_CHECK * 2 == TICKS_PER_SECOND`, so the half-second cadence
   fails loudly under a TPS change rather than drifting.

**Lock test** — `tests/test_psionics.cpp`: `psi_costs_match_prior_art`,
`psi_upkeep_per_second`, `psi_regen_one_per_second`, `psi_check_cadence_is_half_second`.

**Do not** change any of these numbers to "feel better". They are prior-art, marked as such; the
test's job is to make a future change deliberate.

**Closed — with a correction to the first attempt.** The first version of `psi_costs_match_prior_art`
asserted local constants against themselves and could never fail, because `getPsiCost()` was
declared `static` at namespace scope in `battleunit.h`. That gives internal linkage in every
translation unit that sees the header while the only definition lives in `battleunit.cpp`, so the
test could not call it — and neither could anything else, making the declaration a latent
undefined-reference independent of the test. `getPsiCost()` and its sibling `getPsiAttackChance()`
are now public statics on `BattleUnit`, following `TacticalAIVanilla::retreatChancePercent()`, and
the test drives the real cost table across both the attack and upkeep columns. Verified red by
changing Stun's initial cost from 16 to 17.

One divergence is deliberately locked to the code rather than to `psionics.txt`: Panic upkeep is
`3` per half-second check (6/sec) where the document implies `2` (4/sec). Control, Stun and Probe
all match exactly and so do all four initial costs, which makes a transcription slip likelier than
the document being wrong — but that is an inference, and TACP has not been asked. See
[findings/A2-psi-panic-upkeep-divergence.md](findings/A2-psi-panic-upkeep-divergence.md).

### A3 · TU reservation parity

*Gap matrix: "implemented (parity unverified), high."*

TACP strings `TUs reserved for kneeling` / `aimed` / `snap` / `auto` confirm the four modes exist;
the reserve logic is in `battleunit.cpp`. Nothing pins the reserved amounts.

**Steps**

1. `tests/test_tu_reservation.cpp`, driving a unit with each `ReserveMode`.
2. Assert that a unit which has reserved for aimed fire is refused a move that would drop TUs below
   the aimed-shot cost, and permitted the move at exactly cost + 1.
3. Assert kneel reservation composes with weapon reservation rather than replacing it.

**Do not** attempt to bind reservation costs to TACP — the four strings have empty bound xrefs.
This row is closed by *verification*, and its confidence stays `medium` regardless.

### A4 · Vanilla attack-priority parity

*Gap matrix: "implemented, medium — prior-art ai.txt + unitaivanilla.cpp (CTH × DAMAGE / TIME)."*

Same shape: implemented from [ai.txt](../../tools/extractors/docs/ai.txt), unverified.

**Steps**

1. `tests/test_unit_ai_priority.cpp` constructing a unit with two weapons and a known target.
2. Assert the higher `CTH × DAMAGE / TIME` weapon is chosen, and that the ordering inverts when
   the factors invert. Ordering is the contract; the absolute score is not.
3. Assert AOE weapons are rejected when a friendly is inside the blast radius (`ai.txt`, Vanilla AI).

**Closed — with a correction to the first attempt.** Step 1 as literally specified is not reachable:
`getWeaponDecision()`/`getGrenadeDecision()` are private, and reaching them at all goes through
`getAttackDecision() -> canAttackUnit() -> hasLineToUnit() -> map.findCollision()`, so a populated
tile map is mandatory before the priority arithmetic is even evaluated. The first attempt concluded
from that the arithmetic was untestable and froze local copies of both formulas — which locked
nothing, since editing `unitaivanilla.cpp` left the test green.

The three primitives that arithmetic is made of are now public statics on `UnitAIVanilla` —
`attackPriority()`, `blastDamageContribution()` and `aoeIsWorthThrowing()` — called from the same
two private methods as before, again following `TacticalAIVanilla::retreatChancePercent()`.
`aoeIsWorthThrowing()` is written as `!(net < 0.0f)` rather than `net >= 0.0f` so a NaN net keeps
the original's "don't veto" outcome instead of silently flipping it. Verified red by dropping the
hostile/friendly sign convention.

The decision *plumbing* — candidate enumeration, LOS, movement selection — still needs a tile map
and is still not covered. What is covered is every number that plumbing compares.

---

## 4. Class B — blocked on reverse engineering

Each entry gives the binding target first. **The RE step is the deliverable**; the code that
follows is usually small.

### B1 · Cautious / Normal cover + potshots — *the highest-value gap in the game*

*Gap matrix: missing (prior-art only), **high** confidence. Issue #265.
[next-implementation.md](next-implementation.md) item 1.*

**Why it matters more than its row suggests.** Because cover does not work, `Normal`, `Cautious`
and `Evasive` all collapse to kneel-or-prone. Aliens close and shoot; they never break line of
sight or trade from a defended position. Every tactical difficulty judgement made against current
OpenApoc — squad size, armour thresholds, casualty budgets, and the whole ground doctrine in
[campaign-plan.md §12.2](../campaign-plan.md#122-ground-combat) — is measured against a
meaningfully softer opponent than the 1997 game. **Closing this re-tunes the game.**

**Current behaviour.** `UnitAIHelper::getTakeCoverMovement`
([unitaihelper.cpp:142](../../game/state/battle/ai/unitaihelper.cpp#L142)) rolls the take-cover
chance — `33% × sqrt(visible enemies)`, assuming 3 when none are seen — and then returns `nullptr`
with the comment *"Cover-tile search is not implemented. Callers fall through to kneel / prone."*
Three call sites depend on it: [unitaibehavior.cpp:69](../../game/state/battle/ai/unitaibehavior.cpp#L69),
[unitaivanilla.cpp:603](../../game/state/battle/ai/unitaivanilla.cpp#L603) and
[:676](../../game/state/battle/ai/unitaivanilla.cpp#L676).

**Blocker.** No printable `cover` or `potshot` in TACP. There is no recovered metric for *what makes
a tile good cover*.

**RE steps**

1. TACP printable strings that **are** bound: `Cautious mode`, `Aggressive mode`, `Kneel down`,
   `Reserve TUs for kneel`, `Unit under fire` (`0x2E0438`), `Unit has gone berserk` (`0x2DF134`).
   Start at `Cautious mode` and `Unit under fire` and walk **callers**, not the string.
2. The target is a function that, given a unit and a threat direction, **scores or selects a tile**.
   Signature to look for: takes a unit pointer and a position/direction, loops over neighbouring
   tiles, and compares an accumulated per-tile integer.
3. The per-tile input is most likely an existing map-part field. Cross-check the battle map-part
   tables in [tools/extractors/common/](../../tools/extractors/common/) for an unmapped byte —
   candidates are any `unknown*` on the map-part struct that correlates with wall/solidity.
4. If a scoring function is found, capture: the metric, the search radius, the tie-break, and
   whether the unit kneels/prones on arrival.
5. If **no** scoring function exists after this walk, record that negative result in the gap matrix
   and **leave `getTakeCoverMovement` returning null**. A "reasonable" cover heuristic would be an
   invented mechanic wearing the original's name.

**Implementation, once the metric is bound**

1. Add `UnitAIHelper::scoreCoverTile(GameState&, BattleUnit&, Vec3<int> tile, Vec3<float> threat)`
   returning the recovered metric.
2. In `getTakeCoverMovement`, after the existing chance roll, search tiles within the recovered
   radius, reject tiles that cost more TUs than the unit has, and select the best score; return an
   `AIMovement` with `movementMode` per `ai.txt` — **Cautious prones if it can, Normal kneels.**
3. Potshots are a separate behaviour: after arriving in cover, fire and return to cover. Model as an
   `AIMovement` + `AIAction` pair; do not fold it into the movement.

**Lock test** — extend [tests/test_tactical_ai_retreat.cpp](../../tests/test_tactical_ai_retreat.cpp),
whose current lock is *"retreat + panic run only"*:

- `cautious_unit_takes_cover` — a Cautious unit under fire with a scoring tile available returns a
  non-null movement toward it.
- `normal_unit_kneels_without_cover` — with no scoring tile, falls back to kneel (today's behaviour).
- `cover_prefers_higher_metric` — given two candidates, the higher-scored tile wins.

**Do not** implement "move away from the enemy" or "move behind the nearest wall" as a stand-in.
Both are inventions, and both would make the row *look* closed.

### B2 · Evasive seek-cover

*Gap matrix: missing (prior-art only), medium.*

Strictly downstream of B1 — same missing metric, plus `Evasive` prones where `Normal` kneels
([ai.txt](../../tools/extractors/docs/ai.txt)). **Do not start before B1 lands.** There is no
printable `evasive`, so even the mode name is prior-art.

Once B1 exists: pass the cautious flag through so `Evasive` calls
`getTakeCoverMovement(state, u, /*forced*/ true)` and sets `MovementMode::Prone` on arrival when
`isMovementStateAllowed(Prone)`.

**Lock test** — `evasive_unit_prones_in_cover` in the same file as B1.

### B3 · Wounded move / shoot penalty

*Gap matrix: missing (prior-art only), medium.*

**Evidence.** TACP string `Unit critically wounded`. Fatal wounds already exist and tick:
`BattleUnit::addFatalWound` ([battleunit.cpp:1610](../../game/state/battle/battleunit.cpp#L1610)),
applied on `TICKS_PER_WOUND_EFFECT` — itself carrying
`// FIXME: Seems to correspond to vanilla behavior, but ensure it's right`
([battleunit.h:42](../../game/state/battle/battleunit.h#L42)).

**Blocker.** No recovered TU or accuracy constants. The original penalised a wounded unit; the size
of the penalty is unknown.

**RE steps**

1. Xref `Unit critically wounded` (TACP). If bound xrefs are empty, it is a message-only string —
   record that and stop.
2. Otherwise find the wound counter on the unit struct and locate **readers** other than the
   damage-over-time tick. A reader inside the TU or accuracy computation is the target.
3. Capture whether the penalty is per-wound or per-wounded-body-part, and whether it is a
   subtraction or a percentage.

**Implementation, once bound**

1. Apply in `BattleUnit::getMaxTU()` / the accuracy path, not at the call sites, so every consumer
   inherits it.
2. Body-part-specific effects (leg wounds → movement, arm wounds → accuracy) must come from the
   binding, not from plausibility.

**Lock test** — `tests/test_battle_wounds.cpp`: `wounded_unit_tu_penalty`,
`wounded_unit_accuracy_penalty`, `healed_unit_penalty_clears`.

### B4 · Wounded medkit use in cover

*Gap matrix: missing (prior-art only), medium.*

Depends on **both** B1 (cover) and B3 (knowing a unit is impaired). The AI has no heal path at all.

**Blocker.** No AI cover-heal path and no constants — specifically, no recovered threshold for
*when* a unit decides to heal instead of fight.

**Implementation, once B1 and B3 land**

1. In `UnitAIBehavior::think`, before the cover check: if the unit has fatal wounds, carries a
   Medi-kit, and is in cover, emit an `AIAction` using the item.
2. `BattleUnit::useItem` already handles `Type::MediKit`; reuse it rather than adding a second path.
3. The decision threshold must be recovered. Until then this row stays open even if the plumbing
   is written.

**Lock test** — `wounded_ai_heals_in_cover` in `tests/test_battle_use_item.cpp`.

### B5 · Entropy Enzyme

*Gap matrix: missing (prior-art only), **high**. [next-implementation.md](next-implementation.md)
item 2.*

**Evidence.** TACP strings `Entropy Enzyme`, RAW path `GASEXPLS`.
[version01readme.txt](../../tools/extractors/docs/version01readme.txt) lists enzyme as an
approximation.

**Current behaviour — both constants are invented.**

- `#define HAZARD_SPREAD_CHANCE 10 // out of 100` —
  [battlehazard.h:14](../../game/state/battle/battlehazard.h#L14), directly under
  `// FIXME: This is a MADE UP VALUE!` on [line 12](../../game/state/battle/battlehazard.h#L12).
  Consumed at [battlehazard.cpp:396](../../game/state/battle/battlehazard.cpp#L396) and
  [:425](../../game/state/battle/battlehazard.cpp#L425).
- `TICKS_PER_ENZYME_EFFECT = TICKS_PER_SECOND / 9` —
  [battleunit.h:45](../../game/state/battle/battleunit.h#L45).

**RE steps**

1. The fire work already bound a large part of this subsystem — `FUN_0007c110` (item resist),
   `FUN_0007ad94` (type-2 overlay decode), `FUN_0007ae18` (27-byte power lookup),
   `FUN_0007b3dc` (stage/extinction). **Enzyme almost certainly shares the hazard tick.** Start by
   checking whether enzyme is a *type* on the same structure rather than a separate system.
2. The unbound generic path is `FUN_0001eee8` / `FUN_0007b0d0` (placement / spread RNG). Bind those
   and `HAZARD_SPREAD_CHANCE` disappears as a concept — spread becomes a recovered neighbour
   selection, not a flat percentage.
3. Enzyme's distinguishing behaviour is armour damage. Look for a reader of the armour value on the
   hazard-contact path, separate from the health path.

**Implementation, once bound**

1. Delete `HAZARD_SPREAD_CHANCE` outright. Do not retune it.
2. Route generic hazards through the recovered placement/spread functions, as fire already is via
   `Battle::updateFireScheduler`.
3. Replace `TICKS_PER_ENZYME_EFFECT` with the recovered cadence.

**Lock test** — extend [tests/test_battle_hazard.cpp](../../tests/test_battle_hazard.cpp)
(existing locks: `fire_hazard_item_resist`, `fire_overlay_power_progression`,
`fire_overlay_terrain_threshold`, `fire_scheduler_state_machine`) with `enzyme_armour_damage`,
`enzyme_tick_cadence`, `hazard_spread_uses_recovered_rng`.

**Do not** tune `HAZARD_SPREAD_CHANCE` to a nicer number. It is not a parameter; it is a placeholder
that should cease to exist.

### F1 · Fire — the remaining unbound quarter

*Gap matrix: **partial**, high. [next-implementation.md](next-implementation.md) item 2.*

Most of fire is bound and is some of the best work in the tree —
`Battle::updateFireScheduler` implements real-time `FUN_0007b7f8` (one vanilla iteration per four
OpenApoc ticks; `(mapY × mapZ) / 0x48` complete X rows via persistent Y/Z cursors; then
check/reset/run/increment the `0x24` item-contact counter), and turn-based round wrap runs
`FUN_000b8c50`'s 400-iteration batch with item contacts and no unit contacts.

**Still unbound:** unit fire intensity · generic placement/spread RNG (`FUN_0001eee8` /
`FUN_0007b0d0`) · `HAZARD_SPREAD_CHANCE` (shared with B5) · `ttl` and AOE expansion ticks.

**RE steps**

1. Unit intensity — find the reader of the per-unit fire value distinct from
   `TICKS_PER_FIRE_EFFECT` ([battleunit.h:46](../../game/state/battle/battleunit.h#L46)).
2. Neighbour selection — `FUN_0007b0d0` is the target. Capture the neighbour ordering and the RNG
   call, then confirm `FUN_0001eee8` is the generic placement entry.
3. `unused01` at catalog `+0xF` allocates a **type-4 doodad** on burn-out
   (`FUN_000598d4` @ file `0xB4378` → `FUN_00058e84` @ `0xB3928`). It is a **visual slot with no
   damage reader** — do not wire damage from it.

**Do not** invent placement seed 10, map `age`/`power`, or an `INC` cadence. Overlay `INC` is
`FUN_0007b3dc` on the tile overlay byte at `0xF3738` and nothing else.

### K1 · Personal Cloaking Field

*Gap matrix: missing (prior-art only), **high**.*

**Current behaviour.** `CLOAK_TICKS_REQUIRED_UNIT = TICKS_PER_SECOND * 2`
([battleunit.h:64](../../game/state/battle/battleunit.h#L64)) — the comment cites *"Yataka Shimaoka
on forums"*, i.e. **community folklore, not the binary**. The accumulator is at
[battleunit.cpp:2114](../../game/state/battle/battleunit.cpp#L2114): it increments while a
`CloakingField` is in either hand and resets to zero otherwise.
`BattleUnit::useItem` returns `false` for `Type::CloakingField`
([battleunit.cpp:5144](../../game/state/battle/battleunit.cpp#L5144)) — the item is passive.

**Blocker.** Cloak tick thresholds unbound. TACP type `0x0a` is extracted; the *timing* is not.

**RE steps**

1. Xref `Personal Cloaking Field` in TACP. Note the gap matrix already records this string as
   having **empty bound function xrefs** — expect to work from the equipment type instead.
2. Find readers of equipment type `0x0a` on the unit-update path. The target is the comparison
   against an accumulator — the original's equivalent of `CLOAK_TICKS_REQUIRED_UNIT`.
3. Capture both edges: how long inaction takes to *gain* concealment, and what *breaks* it.
   OpenApoc currently breaks it only on unequip; the original very likely breaks it on firing.

**Implementation, once bound**

1. Replace the constant, and delete the forum attribution comment.
2. If firing breaks cloak, reset the accumulator in the fire path, not in `updateStateAndStats`.

**Lock test** — `tests/test_battle_cloak.cpp`: `cloak_engages_after_recovered_delay`,
`cloak_breaks_on_fire`, `cloak_resets_on_unequip`.

### G1 · Dead gadgets — MultiTracker, Vortex Analyzer, Disruptor Shield

*Gap matrix: "implemented (Mind Shield locked; other gadgets prior-art only), high."
[next-implementation.md](next-implementation.md) item 6.*

Mind Shield is bound (+30, cap 200, TACP `FUN_0009b780` @ `0x9B780`). The rest are inert:
`BattleUnit::useItem` returns `false` for `MultiTracker`, `VortexAnalyzer`, `DisruptorShield`,
`CloakingField`, `AlienDetector`, `StructureProbe` and `DimensionForceField`
([battleunit.cpp:5141](../../game/state/battle/battleunit.cpp#L5141)).

**Blocker.** No confirmed consumer for any of them.

**RE steps.** For each type in turn: locate the extracted type id in
[aequipment.h](../../tools/extractors/common/aequipment.h), then search TACP for readers of that id
on the unit-update or UI-refresh path. **A type id with no reader is a dead gadget in the original
too** — that is a legitimate finding, and it should be recorded in the matrix rather than papered
over with a UI effect.

**Do not** invent a scan radius, a detection UI, or a shield regen formula.

### V1 · Vehicle attack-mode dodge (100 / 80 / 50 / 10)

*Gap matrix: "implemented (voxel dodge extracted; attack ladder unbound), high."
[next-implementation.md](next-implementation.md) item 3.*

**Current behaviour.** [vehicle.cpp:170](../../game/state/city/vehicle.cpp#L170) carries
`// FIXME: Read vehicle engagement rules, instead for now chance to dodge is flat
100% / 80% / 50% / 10% depending on behavior`, then a switch mapping `AttackMode` →
`Evasive 100 / Defensive 80 / Standard 50 / Aggressive 10`.

**Blocker.** `Rules of engagement` at `0x152D10` has **empty bound xrefs**, and `Evade Fire` xrefs
are empty too. There is no recovered engagement table.

**Critical trap, already documented:** `loftemps_index` at `+0x28` (sample `79 00` @ `0x189CB4`) is
**not** a dodge percentage. [hexa.txt](../../tools/extractors/docs/hexa.txt) labels it "chance to
evade bullets"; that label is wrong. It is the loftemps voxelmap index, used by
[extract_vehicles.cpp](../../tools/extractors/extract_vehicles.cpp) for misaligned voxelmaps.
**Do not map values 119–151 onto percentages** — that is the single most tempting wrong move in
this row, and `next-implementation.md` calls it out by name.

**RE steps.** Find a reader that branches on the vehicle's attack-mode field and produces a
comparison value. Absent that, the four numbers stay as they are, flagged.

**Lock test.** None until an engagement table exists — a test over invented constants only freezes
the invention.

### V2 · Ground vehicle engagement / large footprint

*Gap matrix: "implemented (lanes + footprint; engagement unbound), high." Issue #785.*

Lanes and footprint work (`connection[dir]` + occupancy; `size_x`/`size_y` from `vehicle_data`).
The engagement table is the same unbound `Rules of engagement` as V1.
[version01readme.txt](../../tools/extractors/docs/version01readme.txt) warns that ordering ground
vehicles can crash — **that is a live defect and is independently fixable**: add a regression test
that issues a ground-vehicle move order across a destroyed road segment and asserts no crash and a
terminating path.

### O1 · Org-org bribe and diplomatic-rift dollar formulas

*Gap matrix: "implemented (table + raid snapshot order; formulas unbound), medium." Issue #996.
[next-implementation.md](next-implementation.md) item 5.*

The directed `organisation_starting_relationships` table is extracted, and raid ordering is correct
(raid uses `long_term − current` **before** `updateRelations` snapshots). The **coefficients** in
`Organisation::costOfBribeBy` and `diplomaticRiftOffer` are unbound.

**RE steps.** Find the reader that turns a relationship delta into a currency amount. Beware
`FUN_000941dc` @ file `0xF6880` — it is weekly **org budget fractions** (stride `0x1B6`, 27 orgs),
**not** the bribe formula, and not `EconomyInfo::update` either.

**Lock test** — extend [tests/test_diplomacy.cpp](../../tests/test_diplomacy.cpp) and
[tests/test_organisation.cpp](../../tests/test_organisation.cpp).

**Do not** invent a weekly-drift formula.

### O2 · `Cargo::seize` diplomacy

*Gap matrix: "implemented (refund + restock; seize diplomacy unbound), high."*

`Cargo::refund` credits the buyer only; the seller keeps the `settleMarketPurchase` credit
([vehicle.cpp:3990](../../game/state/city/vehicle.cpp#L3990)). `Cargo::seize` restocks the market
but its relation effect is a FIXME.

**The trap.** `FUN_000b32ac` @ VA `0xB32AC` / file `0x115950` is a **4-way event dispatcher**
(`[0x174024]`) that computes `worth × 50` into org `+8` plus a tiered extra
(`worth > 50 → −(worth−50)/20`; `worth ≥ 250 → −10 − (worth−250)/50`, floor `−50`) via
`FUN_0005faf0` @ file `0xC2194`. **It is not a cargo-seize entry.** Neither the event type → cargo
mapping nor whether `+8` is funds or relation is bound. `next-implementation.md` states plainly:
*do not wire `Cargo::seize` from `worth×50`.*

**RE step.** Bind `[0x174024]` event types to their sources. Until then the row stays open.

### M1 · City action music

*Gap matrix: "implemented (battle Action; city trigger unbound), high." Issue #618.
[next-implementation.md](next-implementation.md) item 4.*

Battle Action playlist is wired. UFO2P has `Action music`, `getnextmusic` and the `/MUSIC/GROUP_*`
RAW catalog, but **empty bound xrefs for the city combat mix** — catalog-only.

**RE step.** Find the city-side caller of the music selector. **Pass criterion: city mix only after
a bound consumer.** Triggering city action music on "a hostile vehicle is nearby" would be a
plausible invention with no evidence behind it.

### U1 · UFO mission counter `+0x171` transition

*Gap matrix: folded into "UFO mission / incursion tables", high.
[next-implementation.md](next-implementation.md) item 9.*

Nearly all of `UFO_mission_data` (45 × 42 @ `0x13DDFC`) is consumed: `follow_slot`, spawn XY,
`type_percent` × constitution, the `scatter > 50 → 10` clamp, and `+0x1B`.

**A correction worth repeating:** hexa's `building_function` label for `+0x1B` is **wrong**.
`FUN_0006da88` @ file `0xD030B` copies it to vehicle `+0x171`, and `FUN_0003a910` @ object-page file
`0x2A90F` decrements it whenever the UFO reaches a mission destination; zero advances target/mission
state. It is extracted as `mission_counter` — and incursion A1 proves values of 12, which exceed the
ten gate slots, so it cannot be a gate index.

**Unbound:** the exact OpenApoc transition when the counter hits zero, and `FUN_000588f8` @
object-page file `0x488F7`, which gates `+0x168` against constitution `+0x12e`.

**Lock test** — [tests/test_city_rules.cpp](../../tests/test_city_rules.cpp) already locks
`ufo_incursion_table`, `ufo_incursion_spawn_xy`, `ufo_incursion_follow_type`. Add
`ufo_mission_counter_transition` once bound.

**Do not** add a building-function filter, and do not invent an `acquireTargetBuilding` name filter.

### U2 · Base-exposure leftovers

*[next-implementation.md](next-implementation.md) items 8 and 10.*

Most of this subsystem is now bound and is worth reading as a model of how these rows get closed:
`+0x2BC` is the terminal word of each 0x2BE-byte, 16-entry X-COM Base runtime record, serialized as
`Base::knownToAliens` plus a reusable `Base::ufo2pSlot` allocated from the first hole. Normal UFO
deposit (`FUN_0005fddc`) exposes a base on inclusive `rand16(100) < 5`; alien movement
(`FUN_0006f7f8`) uses `< moved_count × 5`; every primary Subversion craft consumes inclusive
`rand16(15)` and cyclically scans the 16 persistent slots. The invented `micronoidRainChance`
takeover path was **removed**.

**Unbound:** the global override `DAT_000e0cc0` (which makes *all* active bases pass the exposure
predicate — its campaign lifecycle is unknown), and the event-dispatch full-transfer writer
`FUN_0006f738`, whose event types 1 and 4 are unmapped.

**Do not** restore an arrival percentage — that was the invented mechanic already deleted once.

---

## 5. Class C — no evidence exists

These four are in the matrix at **low** confidence with `missing (prior-art only)`. They come from
player-facing folklore and manuals, not from the binaries. **Each needs a product decision, not an
implementation plan** — and the honest default for all four is to close them as "not in the
original as described".

| ID | Feature | Evidence status |
|---|---|---|
| **C1** | Umbilical collapse | **No printable `umbilical` in UFO2P.** |
| **C2** | UFO mushrooms as next-week spawn feedback | ~~No printable `mushroom` in UFO2P~~ — **RECLASSIFIED to Class B.** UFO2P has none, but **TACP does**, at file `0x2E1468`. See [findings/C1-C4-no-evidence-items.md](findings/C1-C4-no-evidence-items.md). |
| **C3** | Large-UFO bombing after first alien-dimension entry | No recovered trigger table. |
| **C4** | City-wide "Apocalypse" attack after the control centre dies | No recovered trigger table. |

**C2 has been reclassified.** The matrix recorded "no printable `mushroom` in UFO2P" — true, but it
only searched UFO2P. **TACP has one**, at file offset `0x2E1468`: the Organic Factory Ufopaedia
entry, which describes embryonic UFOs growing on stems and ends *"All embryonic UFOs must be
destroyed."*

That is a **mission objective in the battlescape**, not cityscape feedback — so the row was also
filed under the wrong subsystem (`game/state/city`). OpenApoc's growth gate
`UFOGrowth::craftFactoryIntact` ([ufogrowth.cpp:51](../../game/state/rules/city/ufogrowth.cpp#L51))
keys on the **building** being alive; the original text implies the objective is the **embryos**,
which may carry their own completion condition. That gate is strategically decisive — see
[campaign-plan.md §3.7](../campaign-plan.md#37-the-organic-factory-shuts-the-tap-off) — so the
distinction matters. Investigation steps are in
[findings/C1-C4-no-evidence-items.md](findings/C1-C4-no-evidence-items.md). **Do not add a
cityscape mushroom visual.**

C3 and C4 are the only Class C rows that could reasonably be built as **declared original design**
rather than parity: both are late-campaign escalation, and both would need a new data table
(`docs/original-game/exe-tables/` is *not* the right home — a designed feature belongs in
`data/common_patch/`, clearly labelled as OpenApoc-authored). If that is done, it must be marked
**openapoc-todo / designed**, never `implemented`, so the matrix does not claim parity it does not
have.

---

## 6. Suggested order

Dependency-ordered, not value-ordered. Value is in the notes.

| # | Row | Class | Why here |
|---|---|---|---|
| 1 | A2 psionics, A3 TU reservation, A4 attack priority | A | Cheap. Freezes three `parity unverified` rows before a TPS or AI refactor moves them silently. |
| 2 | V2 ground-vehicle crash regression | A | A live crash, fixable without any binding. |
| 3 | **B1 cover metric** | B | **Highest value in the guide.** Gates B2 and B4, and re-tunes every tactical difficulty judgement. Do this before any balance work. |
| 4 | B5 + F1 hazard RNG | B | One binding session closes both; deletes `HAZARD_SPREAD_CHANCE`. |
| 5 | A1 multi-tile units | A | Self-contained engine work, no binding. Good parallel task. |
| 6 | K1 cloak | B | Small, isolated, replaces a forum-sourced constant. |
| 7 | B3 → B4 wounded | B | B4 needs B1 and B3. |
| 8 | U1, U2, O1, O2, G1, M1, V1 | B | Long-tail bindings; several may end as recorded negative results. |
| 9 | C1, C3, C4 | C | Confirmed absent from both binaries — close them. **C2 is no longer Class C**; it moves into the Class B queue with a real TACP string behind it. |

**Realistic expectation:** several Class B rows will end as *"no consumer exists in the original"*.
That is a successful outcome and should be written into the matrix as such. The failure mode this
guide exists to prevent is closing a row with a number nobody recovered.
