# U1(b) — the `+0x168`-vs-constitution gate's consumer, and what it takes to map it

**Scope note on citations.** Binary: `UFO2P.EXE` (canonical, CRC32 `0x4749ffc1`), `.object1`. VA is the
Ghidra listing address; file offset is `.object1`'s `MemoryBlockSourceInfo` file-bytes offset, taken
directly from a fresh `DumpListingRange.java`-style raw dump's own `file=` field for every instruction
quoted below (equal to `VA − 0x10001` for `.object1` code, independently re-derived, not assumed — the
lab's own method warning against computed-file-offset hex reads is honored throughout: every citation below
came from Ghidra's loaded image via `getReferencesTo`/raw listing dumps, never a standalone hex-editor
read). Caller censuses use `currentProgram.getReferenceManager().getReferencesTo(Address)`, never
`QueryDataRange.java` (its `instanceof Scalar` filter misses direct-memory operands, per the task's method
warning). Scripts used this session: `QueryU1bConsumer.java`, `QueryU1bCadence.java`,
`QueryU1bCadence2.java`, `QueryU1bTableIdentity.java`, `QueryU1bPercentTable.java` (all in
`OpenApoc-og-research/scripts/`, `export/u1b_*.log`).

---

## Verdict, up front

**The gate's consumer mechanism is now fully mapped, and the "damaged UFO breaks off and leaves through a
dimension gate" hypothesis holds for the default (non-escorting) case — with two corrections to the
starting hypothesis: this is a periodic, calendar-staggered health check, not a reaction fired at the
instant of a hit (§1), and the damage threshold itself is a role-dependent percentage, not the fixed 75%
this task's brief and this document's own first draft assumed (§4.4) — which leaves mappability genuinely
open on one concrete, disclosed point rather than fully closed.**

1. **`FUN_000588f8` has exactly one caller in the whole binary**, and that caller is not combat code — it's
   a periodic, per-organisation, per-vehicle sweep (`FUN_0005760c`) that runs once per vehicle roughly on a
   calendar cadence, staggered across vehicle-slot index so the 80-slot vehicle array isn't fully rescanned
   every call. A damaged UFO does not instantly turn and flee the tick it's hit; it is caught the next time
   this staggered sweep reaches its slot. (§1)
2. **Confirmed, full raw disassembly of `FUN_000588f8`'s consequence**: once the constitution gate passes
   (constitution has fallen into the band `[crash-floor, +0x168)`, a role-dependent fraction of the repair
   ceiling well under 75% for this population — i.e. moderately damaged, not critically damaged), it sets
   the same `+0x16A` "arrived" flag `U1-arrived-flag-and-0x168.md` already
   bound for the U1(a) mission-arrival branch — confirming that document's finding, not merely repeating it.
   **New this session**: later in the *same* function call, independent of whether the flag-setting gates
   passed, `FUN_000588f8` itself calls the flag's established reader, `FUN_00059148`, on almost every
   invocation — so the gate's own driver function frequently both sets and immediately re-triggers a
   consumption attempt in one call, not only via the 8 already-documented `FUN_0003a910` call sites. (§2)
3. **This is genuinely a damage-triggered-withdrawal behaviour for the default case, not something else.**
   For an order-type-1 vehicle not tracking an escort companion (`+0x16C == -1`), the already-bound
   resolution logic in `FUN_00059148` (re-cited, not re-derived, from `U1-arrived-flag-and-0x168.md` §1.2/§5)
   sends it to the nearest dimension gate. For a vehicle *tracking* a companion (`+0x16C` set — OpenApoc's
   `UfoIncursion::Slot::followVehicleType`), the same trigger instead feeds an "escort/follow" search that
   does not by itself mean withdrawal. So: **default population → flee; escort-role population → the
   damage trigger is absorbed into the existing follow/rendezvous logic instead.** No third behaviour was
   found. (§3)
4. **Mappability: mostly present, with one small, concrete, disclosed gap.** The `+0x168` band's two
   *endpoints* are not undiscovered data: the "ceiling" (`0x128616+type×0x7E`) and "floor"
   (`0x128618+type×0x7E`) are proven this session, by direct row reconstruction and cross-check against an
   already-established sibling field, to be exactly `VehicleData::constitution` and
   `VehicleData::crash_constitution` — which OpenApoc already extracts as `VehicleType::health` /
   `VehicleType::crash_health`. **But `+0x168` is not `0.75 × ceiling`** — that figure, from the prior
   session's doc, was proven only at a retarget call site this population's vehicles structurally never
   reach. For the incursion population, the multiplier is `DAT_0012d950[role]`, raw-verified this session
   (via `FUN_0006da88`'s only two call sites, one a hardcoded role, one reading `UfoMissionData::role[slot]`
   directly) to be **role-dependent and confirmed non-uniform** — 30/30/20/10/25/10% for the six roles this
   population uses, never 75%. OpenApoc's extractor already parses `role[slot]` from the same resource
   record (it has to, to reach the equivalent spawn logic) but currently discards it rather than retaining
   it, and the 16-entry `role → percent` table (`DAT_0012d950`) is not extracted under any name. Both gaps
   are small and nameable — not a missing-concept problem. A further structural finding (§4.4): for two of
   the six roles (Escort and the unnamed role 9), the band is provably empty on every UFO hull, so the gate
   can never fire for them regardless of extraction. (§4)

---

## 1. Who calls `FUN_000588f8`, and how often

### 1.1 Exactly one caller, confirmed by `getReferencesTo`

```
=== xrefs to 000588f8 file=0x488f7 (FUN_000588f8 entry) ===
REF 00057896 file=0x47895 fn=FUN_0005760c@0005760c type=UNCONDITIONAL_CALL insn=CALL 0x000588f8
TOTAL=1
```

`FUN_000588f8` receives one parameter — a **vehicle slot index**, not a pointer — spilled by its own
prologue (`MOV word ptr [ESP+0x78],AX` at VA `0x58901`/file `0x48900`) and used to compute the vehicle
struct pointer itself: `EBP = 0x160fd8 + index×0x276` (VA `0x5890d`–`0x5891f`, file `0x4890c`–`0x4891e`;
`0x160fd8` and stride `0x276` are the already-established vehicle array base/stride). This is a different
calling shape from `FUN_00059148` (which takes the vehicle pointer directly) and is itself a clue that
`FUN_000588f8` is driven by an index-based iteration, not a pointer callback fired from combat code.

### 1.2 The caller: `FUN_0005760c`, a periodic, per-organisation, staggered vehicle sweep

`FUN_0005760c` (VA `0x5760c`, file `0x4760b`, no parameters, **exactly one caller**: `FUN_00010908` at VA
`0x10f68`/file `0xf67`, `CALL 0x0005760c`, `UNCONDITIONAL_CALL`). Its own caller chain is likewise
single-call at every level traced this session: `FUN_00010908` (VA `0x10908`, file `0x907`) has exactly one
caller, `FUN_000105b8` (VA `0x10792`/file `0x791`, `CALL 0x00010908`); `FUN_000105b8` was not traced further
up. This is not proof of daily cadence by itself, but the body of `FUN_0005760c` is:

**Preamble — calendar decomposition, not a per-frame check (VA `0x57615`–`0x5773c`, file `0x47614`–
`0x4773b`).** The function reads four dword globals (`DAT_000d4d66`, `DAT_000d4d68`, `DAT_000d4d6a`,
`DAT_000d4d6c`) and performs a chain of `DIV`/`IDIV` operations against the constants `0x1fa40` (129600),
`0x24` (36), and `0x12` (18), producing several phase values stashed on the stack. This is classic
calendar-unit decomposition (a large tick counter broken into nested period indices), not anything a
per-frame combat handler would do.

**Main loop — all 80 vehicle slots, filtered by side, VA `0x5773c`–`0x57981`, file `0x4773b`–`0x47980`:**

```
0005773c  MOV EBX,0x160fd8          ; file 0x4773b -- vehicle array base
00057741  XOR ECX,ECX                ; file 0x47740 -- slot index = 0
00057747  MOV EAX,dword ptr [EBX]    ; file 0x47746
00057749  SAR EAX,0x10
0005774c  CMP EAX,-0x1
0005774f  JZ 0x00057976              ; file 0x4774e -- invalid slot -> next
00057755  MOV AL,byte ptr [EBX + 0x174]  ; file 0x47754 -- vehicle's owning side
0005775b  CMP AL,byte ptr [0x000d5060]   ; file 0x4775a -- DAT_000d5060, the per-side discriminant
00057761  JNZ 0x00057976              ; file 0x47760 -- side mismatch -> next
00057767  MOV EAX,EBX
00057769  CALL 0x000579b4             ; file 0x47768 -- a different per-vehicle function (not examined)
```

Every valid, side-matching vehicle then goes through a docked/undocked split (`+0x108` high word `== -1`
means undocked) feeding into **one of two mutually exclusive periodic dispatchers**: an undocked vehicle (or
a docked one whose order-type `== 3`) that is not the UI-selected vehicle is routed, subject to a `slot
index mod 18` match against one of several date-derived phase values (gated further by a counter,
`DAT_000d4d58`, compared against 2/4/6 — its exact meaning wasn't pursued, see §5), to:

```
00057893  MOVSX EAX,CX               ; file 0x47892 -- EAX = slot index
00057896  CALL 0x000588f8            ; file 0x47895 -- ***** THE U1(b) GATE FUNCTION *****
```

A docked vehicle with order-type `!= 3` instead falls through to a **different** staggered dispatcher
(divisor `0x24`=36 rather than `0x12`=18) that calls `FUN_00059e60`, not `FUN_000588f8` (VA
`0x577da`–`0x5789b`, file `0x477d9`–`0x4789a`). Loop increment: `INC ECX; ADD EBX,0x276; CMP CX,0x50; JL
...` (VA `0x57976`–`0x57981`, file `0x47975`–`0x47980`) — `0x50` = 80, confirming the full 80-slot vehicle
array, matching every other census in this project.

**What this establishes for task item 1:** `FUN_000588f8` is invoked from a single site, inside a function
that (a) decomposes a game-clock counter into calendar phases, (b) iterates every vehicle slot once,
filtered to the organisation/side currently being processed, and (c) fires the actual gate call only on a
`slot-index mod 18` rotation gated by those calendar phases — a deliberate "spread the per-vehicle work
across ticks" stagger, not a per-frame or per-hit check. **A UFO's constitution-vs-threshold gate is
evaluated on a periodic, staggered cadence tied to that organisation's processing pass, not reactively at
the moment it takes damage.**

---

## 2. What `FUN_000588f8` does once the threshold band is entered — full raw disassembly

Full function body, `0x588f8`–`0x58b84` (file `0x488f7`–`0x48b83`, 653 bytes), dumped and read in full this
session (not the 96-instruction-truncated `QueryFunctions.java` view).

### 2.1 The two entry gates (re-confirms `U1-arrived-flag-and-0x168.md` §2.5, raw, this session)

```
00058925  MOV AX,word ptr [EBP + 0x168]        ; file 0x48924
0005892c  CMP AX,word ptr [EBP + 0x12e]         ; file 0x4892b -- constitution
00058933  JLE 0x00058a48                        ; file 0x48932 -- +0x168 <= constitution -> skip everything
00058939  MOV EDX,dword ptr [EBP + 0x2]         ; file 0x48938
0005893c  SAR EDX,0x10                          ; EDX = vehicle type
0005893f  MOV EAX,EDX
00058941  SHL EAX,0x6
00058944  SUB EAX,EDX                           ; EAX = type * 0x3F
00058946  MOV DX,word ptr [EBP + 0x12e]         ; constitution
0005894d  CMP DX,word ptr [EAX*2 + 0x128618]    ; EAX*2 = type*0x7E -> the "floor" table (§4)
00058955  JL 0x00058a48                         ; file 0x48954 -- constitution < floor -> skip everything
```

So the whole flag-setting block below only runs when **`floor[type] <= constitution < +0x168`**, and
`+0x168` is (per the already-bound formula, `U1-arrived-flag-and-0x168.md` §2.6) `percent × ceiling[type] /
100` at the last retarget/spawn recompute — where `percent` is **not** a fixed 75% for this population; see
§4.4 for the corrected, role-dependent figure. This is a **band**, not a simple "any damage" check: a
vehicle that has dropped *below* the floor does not take this branch at all.

### 2.2 The flag write is unconditional on order-type; only the message side-effect is not

```
0005895b  MOV CX,word ptr [EBP + 0x12c]         ; order-type
00058962  MOV byte ptr [EBP + 0x16a],0x1        ; file 0x48961 -- ***** ARRIVED FLAG SET, unconditional *****
00058969  TEST CX,CX
0005896c  JNZ 0x00058a48                        ; file 0x4896b -- order-type must be 0 to continue further
00058972  CMP byte ptr [0x000d5060],0x0         ; file 0x48971
00058979  JNZ 0x00058a48                        ; file 0x48978 -- DAT_000d5060 must be 0 too
```

For the population this task cares about — alien incursion UFOs, order-type structurally `== 1` for their
whole lifetime (`U1-retarget-reconciliation.md` §1) — the flag write at `0x58962` always executes once the
band is entered, but the code from `0x58972` onward (mirror `+0x100→+0x102`, a name-substitution message
build, and a call to `FUN_00051348`) never runs, because `TEST CX,CX; JNZ` always takes the jump for
order-type 1. That message path was not chased further — it is provably irrelevant to this population.

### 2.3 New this session: `FUN_000588f8` itself re-triggers the flag's reader on almost every call

Past the merge point (`0x58a48`), after a slot-validity check and two "is this the UI-selected vehicle"
gates (calling `FUN_00058b88` when not selected — a function that itself reads/writes `+0x271`, see §5), a
**third**, independent reference to the floor table gates a call to `FUN_00059148` — the reader
`U1-arrived-flag-and-0x168.md` §1.2 already fully bound:

```
00058a99  MOV EDX,dword ptr [EBP + 0x2]         ; file 0x48a98
00058a9c  SAR EDX,0x10                          ; type
00058a9f  MOV EAX,EDX
00058aa1  SHL EAX,0x6
00058aa4  SUB EAX,EDX                           ; type*0x3F
00058aa6  LEA EDX,[EAX + EAX*0x1]               ; EDX = type*0x7E
00058aa9  MOV AX,word ptr [EBP + 0x12e]         ; constitution
00058ab0  CMP AX,word ptr [EDX + 0x128618]      ; vs floor[type]
00058ab7  JGE 0x00058ac3                        ; file 0x48ab6 -- constitution >= floor -> CALL UNCONDITIONALLY
00058ab9  CMP word ptr [EBP + 0x12c],0x1        ; file 0x48ab8 -- (only reached when constitution < floor)
00058ac1  JZ 0x00058aca                         ; file 0x48ac0 -- order-type==1 -> skip the call
00058ac3  MOV EAX,EBP
00058ac5  CALL 0x00059148                       ; file 0x48ac4
```

**Correction to how this call site reads at a glance**: it is *not* "only called when critically damaged and
order-type != 1," as a shallow reading of the tail three instructions alone would suggest. The `JGE` at
`0x58ab7` means the call fires **unconditionally whenever constitution is at or above the floor**,
regardless of order-type — the order-type check only gates the one remaining case (constitution *below* the
floor), where it's skipped solely for order-type-1 vehicles. **Net effect: `FUN_00059148` is called on every
`FUN_000588f8` invocation for a valid vehicle, except the single combination "critically damaged (below
floor) and order-type 1."** For a vehicle in the `[floor, +0x168)` band (§2.1) that combination cannot
occur (constitution is by definition `>= floor` there), so **the flag `FUN_000588f8` just set at `0x58962`
is, in the ordinary case, consumed by this same function in the same call**, not only via the 8 call sites
inside `FUN_0003a910` already documented.

A second block further down (`0x58adc`–`0x58b76`, gated on `+0x108` high-word `== -1` again and on a
separate "damage taken since last check" tracker at `+0x162`, the same field `FUN_00057c8c`'s combat-damage
function accumulates into) calls two more functions, `FUN_0005eddc`/`FUN_0003c374`, and resets `+0x162 = 0`.
This looked at first like it might be a second, independent "flee" trigger, but its gating (a
per-side/base counter at `+0x160` compared against `order-type×0xF/100`, plus a fourth reference to the
same floor table) reads as a *different* notification mechanism keyed to recent damage accumulation rather
than the constitution band from §2.1 — it was not chased to a conclusion and is **not** claimed as part of
the withdrawal mechanism; flagged as open in §5.

---

## 3. Is this "damaged craft withdraws," or something else?

**It is damage-triggered withdrawal for the default case.** Tracing the full chain:

- `FUN_000588f8`'s gate (§2.1) only fires in a specific damage band: below the role-dependent `+0x168`
  threshold (a minority fraction of the repair ceiling for every role this population actually uses — see
  §4.4), but not below the crash floor. That is a "moderately damaged" reading, not "any scratch" and not
  "about to die" — those are handled elsewhere (`FUN_00057c8c`'s lethal branch destroys the vehicle
  outright; going *below* the floor takes the vehicle out of this gate's band entirely).
- The flag it sets is the *identical* `+0x16A` flag and *identical* reader (`FUN_00059148`) already fully
  bound for U1(a)'s mission-arrival transition — this was already established by
  `U1-arrived-flag-and-0x168.md` §2.5, and this session's fresh raw read of the same function confirms it
  byte-for-byte and adds the new same-tick-consumption finding (§2.3).
- `FUN_00059148`'s already-bound resolution (`U1-arrived-flag-and-0x168.md` §1.2, §5 — re-cited, not
  re-derived) has exactly two outcomes for an order-type-1 vehicle: **fly to the nearest dimension gate**
  (when the vehicle is not tracking a companion, `+0x16C == -1`, and its own `+0x271`/side conditions allow
  it), or **follow a candidate** (the escort/rendezvous search, which runs whenever `+0x16C != -1`). No
  third resolution exists in that function.

So for the population this task is about — solo-role incursion UFOs, `+0x16C == -1` at spawn unless they
were assigned an escort/follow role (OpenApoc's `UfoIncursion::Slot::followVehicleType`, per the prior
session's finding) — **the damage trigger's real-world effect is: a moderately-damaged UFO breaks off its
current mission and heads for the nearest dimension gate to leave.** For an escort-role UFO tracking a
companion, the identical trigger instead feeds the pre-existing follow/rendezvous logic rather than causing
withdrawal — the mechanism is shared, not duplicated, and the escort case is corroborating detail, not a
counterexample.

**One caveat, honestly reported rather than smoothed over.** This session's new finding (§2.3) is that
`FUN_000588f8` frequently calls `FUN_00059148` in the *same* tick it sets the flag. Whether that particular
call's Block 1 (`U1-arrived-flag-and-0x168.md` §1.2) actually latches the "go to portal" intent depends on
`DAT_000d5060 == 0` **at that exact moment** — and this session did not establish what value `DAT_000d5060`
holds while an alien-owned UFO is being processed in its own organisation's sweep (a plausible but
*unconfirmed* cross-session inference, built from stacking this session's "side filter uses `DAT_000d5060`"
observation with `U1-scheduler-population.md`'s separately-established "alien organisation index is 1, not
0," would suggest `DAT_000d5060 != 0` for aliens specifically during their own pass — which would mean this
particular same-tick call doesn't latch the direct-assignment path). This does **not** undermine the overall
verdict: the flag is not single-shot. If not actionable this call, Block 1 either clears it (if `+0x271 !=
0`) or leaves it set for the next opportunity; Block 2's escort search independently re-arms it when no
candidate is found; and it remains available to any of the 8 `FUN_0003a910` mission-dispatch call sites on
subsequent ticks — the exact same delivery mechanism U1(a)'s already-implemented `gotoPortal` behaviour
already relies on. The uncertainty is about *which* call finally consumes the flag, not about *whether* the
flag reliably reaches its established resolution.

---

## 4. Mappability — the band's endpoints are already-extracted data; its width is not yet settled

The two per-type catalog values that bound `FUN_000588f8`'s gate (§4.1-4.2) turn out to be the game's own
`VehicleData` resource table, already fully extracted — not undiscovered OpenApoc-side data. But `+0x168`
itself, the actual withdrawal threshold, needs a third value — a role-dependent percentage. §4.1-4.2
establish what's solid; §4.4 is the correction that keeps this section honest rather than declaring victory
early; §4.5 covers the remaining implementation seam.

### 4.1 Row identity, proven (not merely inferred) this session

`tools/extractors/common/vehicle.h`'s `VehicleData` struct is declared `static_assert(sizeof(struct
VehicleData) == 126, ...)` — `126 == 0x7E`, exactly the per-type row stride this and every prior U1 session
has used for the `0x128614`-based catalog. Computing `VehicleData`'s field byte offsets directly from its
declaration: `constitution` sits at struct offset `0x2E`, `crash_constitution` at `0x30`. If the catalog's
already-bound "ceiling" field (`0x128616 + type×0x7E`) is `VehicleData::constitution` for row 0, the row's
true base address must be `0x128616 − 0x2E = 0x1285E8`. That is a falsifiable prediction, checked directly
against the loaded image this session (`QueryU1bTableIdentity.java`):

```
=== candidate VehicleData row base 0x1285E8, file=0x585e7, stride=0x7E ===
type=0  manufacturer=1 movement_type=1 ... size_x=1 size_y=1 size_z=1 ... constitution=80  crash_constitution=15  weight=1000  ...
type=1  manufacturer=1 movement_type=1 ... constitution=120 crash_constitution=35  weight=4000  ...
type=8  manufacturer=1 movement_type=1 ... size_x=2 size_y=2 size_z=2 ... constitution=1800 crash_constitution=300 weight=24000 ...
type=9  manufacturer=1 movement_type=1 ... size_x=2 size_y=2 size_z=2 ... constitution=2800 crash_constitution=350 weight=24000 ...
```

Every decoded field is plausible game data end-to-end for the ten UFO types (0–9): a monotonically
increasing constitution progression (80→2800) matching increasing UFO danger tiers, `crash_constitution`
always smaller than `constitution` (consistent with "the point below which the craft is written off," which
is exactly how OpenApoc already uses this field — see §4.2), `size_x/y/z` jumping from 1 to 2 exactly at
types 8–9 (the two Battleship-tier hulls), and `weight` jumping from ~1000–5000 to 24000 at the same point.
This is not a coincidence of one lucky offset — it is 34 consecutive fields across 10 rows all reading as
sensible data simultaneously.

**Independent cross-check, not self-referential**: the prior session's `U1-retarget-reconciliation.md` §2.3
already found, separately, that `0x1285ea + type×0x7e` reads a "class byte," uniformly `1` across type
indices `0`–`24`, without being able to identify the field. Under this session's row hypothesis,
`0x1285ea = 0x1285E8 + 2`, which is exactly `VehicleData::movement_type` (the struct's second `uint16_t`
field). Dumped fresh this session for all 25 type indices: **uniformly `1`**, matching that prior finding
address-for-address and giving it a concrete field identity it didn't have before (a movement-type enum
value shared by every vehicle/UFO type in this table, not a UFO/mundane discriminant — which also
retroactively explains why that prior session's population-identity question in
`U1-retarget-reconciliation.md` couldn't be settled from this field: it isn't the right field for that
question).

**Conclusion: `0x128616+type×0x7E` (the "ceiling") is `VehicleData::constitution`; `0x128618+type×0x7E`
(the "floor") is `VehicleData::crash_constitution`.** Both are per-type resource-file fields the game's own
`vehicle_data` table already carries — not a repair-specific value invented separately from the vehicle's
nominal health, and not something requiring new extraction.

### 4.2 OpenApoc already extracts, stores, and uses both fields

- `tools/extractors/extract_vehicles.cpp:334-335`: `vehicle->health = v.constitution; vehicle->crash_health
  = v.crash_constitution;` — extracted straight from the same resource row identified above.
- `game/state/rules/city/vehicletype.h:315-316`: `int health = 0; int crash_health = 0;` on `VehicleType`.
- `game/state/rules/city/vehicletype.h:92`: `getMaxHealth(...)` returns `this->health` — i.e. the "ceiling."
- `game/state/city/vehicle.h:214,332`: `Vehicle::health`, `Vehicle::getHealth()` — current constitution.
- `game/state/city/vehicle.cpp:3462-3468`: `Vehicle::getMaxHealth()` forwards to `type->getMaxHealth(...)`.
- **`crash_health` is not merely stored — it already drives real behaviour**, at
  `game/state/city/vehicle.cpp:2686-2706`: when applied damage brings `health` down to or below
  `type->crash_health`, a UFO calls `crash(state, attacker)` and any other vehicle `startFalling(...)`. This
  is a *different* mechanic from U1(b) (it fires reactively, at the instant of the hit that crosses the
  floor, not on a periodic sweep, and its effect is "go down," not "flee to a portal") — the two should not
  be conflated — but it confirms `crash_health` is already load-bearing, tested game logic in OpenApoc, not
  a dormant extracted-but-unused field.

**So the two *endpoints* of the band — current constitution and the crash floor — already exist on
`Vehicle`/`VehicleType` today, under names that already carry the right meaning. The ceiling
(`getMaxHealth()`) exists too, but §4.4 shows the threshold is not simply a fixed fraction of it.**

### 4.4 Correction: `+0x168`'s percentage is role-dependent, not a fixed 75%, for this population

**This matters, and the advisor review of this document's first draft is the reason it's here.** The draft
asserted `+0x168 = 0.75 × ceiling[type]` for the incursion population generally. That figure is real, but
`U1-arrived-flag-and-0x168.md` §2.6 proved it **only** at one specific call site — inside `FUN_0003a910`'s
retarget branch, where `+0x166` (the role byte that indexes the percent table) is forced to `0`
immediately beforehand (`MOV byte ptr [EDI+0x166],0x0` at VA `0x3ad8c`). `U1-retarget-reconciliation.md` §1
independently proves that exact retarget branch is **structurally unreachable** for `FUN_0006da88`-spawned
incursion UFOs (their order-type is invariantly `1`, so `FUN_0003a910` always takes the arrived-flag branch,
never the retarget branch). **The one site where 75% was established is a site this task's population
provably never reaches.**

**What `FUN_0006da88` (the incursion spawn function) actually writes to `+0x166`, raw, this session (VA
`0x6da98`, file `0x5db97`):**

```
0006da98  MOV word ptr [ESP + 0xc],BX        ; file 0x5da97 -- BX is FUN_0006da88's own incoming parameter
...
0006db68  MOV BX,word ptr [ESI + 0x4]        ; file 0x5db67 -- reload BX = vehicle TYPE
0006db70  CMP BX,0x9                          ; file 0x5db6f -- type == 9 (a Battleship hull)?
0006db74  JNZ 0x0006db94                      ; file 0x5db73 -- not type 9 -> keep caller's role unchanged
0006db80  CMP AX,0x32                         ; file 0x5db7f -- (an unresolved threshold, not chased further)
0006db84  JBE 0x0006db94
0006db86  CMP BX,word ptr [ESP + 0xc]         ; file 0x5db85 -- type(9) vs the role value itself
0006db8b  JNZ 0x0006db94                      ; file 0x5db8a -- role != 9 too -> keep it unchanged
0006db8d  MOV word ptr [ESP + 0xc],0xa        ; file 0x5db8c -- role WAS 9 on a type-9 hull -> escalate to 10
0006db94  MOV AL,byte ptr [ESP + 0xc]         ; file 0x5db93
0006db98  MOV byte ptr [ESI + 0x166],AL       ; file 0x5db97 -- ***** +0x166 = the (possibly escalated) role *****
0006db9e  MOV EAX,ESI
0006dba0  CALL 0x0005df1c                     ; file 0x5db9f -- the +0x168 recompute, U1-arrived-flag-and-0x168.md §2.6
```

**So the role written to `+0x166` for this population is `FUN_0006da88`'s own caller-supplied `BX`
parameter**, with exactly one internal override: a type-9 hull already carrying role `9` gets escalated to
role `10` (this is precisely the "escalated-10" case `U1-retarget-reconciliation.md` already named without
being able to explain it). This matches — independently, from three separate sources — the role set
`tools/extractors/common/ufoincursion.h`'s own header comment names for `UFO_mission_data`: `#define
UFO_MISSION_ROLE_ATTACK 5`, `..._INFILTRATION 7`, `..._SUBVERSION 8`, `..._OVERSPAWN 10`, `..._ESCORT 11`
(role `9` is the one value that header leaves unnamed, matching this session's own "unnamed role 9"
finding).

**The percent table, dumped fresh this session (`DAT_0012d950 + role×0x94`, one byte per role, `role` in
`0..15`):**

```
role=0  percent=75      role=6  percent=10
role=1  percent=50      role=7  percent=30   (Infiltration)
role=2  percent=25      role=8  percent=20   (Subversion)
role=3  percent=15      role=9  percent=10   (unnamed)
role=4  percent=33      role=10 percent=25   (Overspawn / escalated target)
role=5  percent=30      role=11 percent=10   (Escort)
                        role=12..15 percent=10/10/10/0
```

**Which roles `FUN_0006da88` actually receives, raw-verified this session (not merely inherited from the
extractor header) — `FUN_0006da88` has exactly 2 callers, both inside `FUN_0006d384` (VA `0x6d384`, already
named in `U1-arrived-flag-and-0x168.md` §3's open-items list as one of "two already-bound UFO-role-assignment
functions" — this session gives that label concrete content):**

```
; call site 1, VA 0x6d497-0x6d4a7, file 0x5d496-0x5d4a6
0006d497  MOV EBX,0x9              ; file 0x5d496 -- ***** role = literal 9, hardcoded *****
...
0006d4a7  CALL 0x0006da88

; call site 2, VA 0x6d57d-0x6d58a, file 0x5d57c-0x5d589
0006d57d  MOV AX,word ptr [ECX + EAX*0x2 + 0xc]  ; file 0x5d57c -- record[slot].role (struct offset 0xC)
0006d585  MOVSX EBX,AX             ; file 0x5d584 -- ***** role = UfoMissionData.role[slot], loaded live *****
0006d588  MOV EAX,EDI
0006d58a  CALL 0x0006da88
```

Call site 2's addressing (`[ECX + slot×2 + 0xC]`, with a sibling read at `[ECX + slot×2 + 0]` two
instructions earlier feeding what is separately used as the vehicle type) matches
`tools/extractors/common/ufoincursion.h`'s `UfoMissionData` struct byte-for-byte: `craft[3]` at offset `0`,
`count[3]` at `6`, `role[3]` at offset `0xC` — i.e. **this call site passes `UfoMissionData::role[slot]`
directly**, the exact field the extractor header names with the `UFO_MISSION_ROLE_*` constants
(`5`=Attack, `7`=Infiltration, `8`=Subversion, `10`=Overspawn, `11`=Escort). Call site 1's hardcoded `9`
independently confirms the "unnamed role 9" reading. **This raises the role-name pairing from an inherited,
cross-session inference to a raw-verified one**: the `{5,7,8,9,10,11}` role set and its extractor-side names
are not merely plausible — the addressing arithmetic proves the same struct field is what's being read.

**Every role this population can actually receive reads well below 75%: 30% (Attack), 30% (Infiltration),
20% (Subversion), 10% (unnamed-9), 25% (Overspawn), 10% (Escort).** A lower percentage means a *lower*
`+0x168` (percent × ceiling / 100 is smaller), which means the gate's `constitution < +0x168` condition
requires **more** damage to satisfy, not less — **the real withdrawal band is narrower and sits at a
*more*-damaged constitution than the draft's `0.75×ceiling` figure implied**, not wider or less-damaged (a
direction this document's second draft got backwards and is corrected here).

**Structural consequence: for several role/hull combinations, the band is provably empty — the gate can
never fire, independent of any other condition.** The band `[floor[type], percent[role]×ceiling[type]/100)`
is non-empty only when `percent[role]×ceiling[type]/100 > floor[type]`. Checking every combination of the
ten UFO types (§4.1's dump) against the six roles above: **role 9 (10%) and role 11/Escort (10%) produce an
empty band for every one of the ten UFO types** — their threshold never clears the floor (e.g. type 0:
`10%×80=8 < floor 15`; type 9: `10%×2800=280 < floor 350`). Types 2, 3, and 4 have an empty band for *every*
role, including Attack/Infiltration/Overspawn (e.g. type 2, Attack: `30%×400=120 < floor 150`). This sharpens
§3's escort-case claim: it is not only that a tracked companion (`+0x16C`) redirects the trigger to
follow/rendezvous logic instead of withdrawal — for Escort-role craft specifically, **the withdrawal gate
itself is structurally unreachable on any hull**, for an independent, arithmetic reason. (Integer-truncation
edge effects at the boundary were not re-verified against the original's exact `IDIV` rounding; the
pattern above is robust to off-by-one rounding except at the few single-digit-wide bands like type 0/Attack,
`[15,24)`.)

**Now that call site 2 is raw-verified as reading `UfoMissionData::role[slot]` directly (rather than merely
assumed), the relationship to OpenApoc's existing `typePercent` field can be stated more precisely than an
open coin-flip.** `UfoMissionData` carries `role[slot]` and `type_percent[slot]` as two **separate, sibling
fields in the same 42-byte record** (`tools/extractors/common/ufoincursion.h`'s struct: `role` at byte
offset `0xC`, `type_percent` at byte offset `0x28`) — not one value under two names. `role` is what
`FUN_0006da88` uses to *index a separate, fixed, 16-entry global table* (`DAT_0012d950`) to get the percent
this task's gate needs; `type_percent` is a *directly-stored-per-record* value the game reads for a
different purpose (`clampIncursionScatter`, multiplying *current* constitution, not the ceiling). Structurally
these look like two different design values that happen to share a "percent of constitution" shape, not one
value read two ways — though this session did not dump `DAT_0012d950` byte-for-byte against every record's
`type_percent` to prove they diverge numerically, so this is reported as a strong structural lean, not a
closed question. **The concrete, narrower gap this leaves**: OpenApoc's extractor (`extract_ufo_incursions.cpp`
`slotFromRecord`) already reads `rec.type_percent[slot]` into `UFOIncursionSlot::typePercent`, but does
**not** currently read or forward `rec.role[slot]` anywhere — `role` is present in the extractor's own
`UfoMissionData` struct (it has to be, to reach `FUN_0006da88`'s call site 2) but is discarded during
extraction. Closing U1(b) faithfully needs two small, disclosed additions, not a full new subsystem: (a)
extract and retain `role[slot]` per incursion slot (the extractor already parses the byte, it just isn't
kept), and (b) extract the 16-entry `DAT_0012d950` role→percent table (or just the six values this
population uses: roles `5/7/8/9/10/11` → `30/30/20/10/25/10`) as a small new lookup, since no existing
OpenApoc table currently holds it under any name.

### 4.5 What has no existing seam regardless: the periodic trigger, not any data

U1(a)'s `gotoPortal` behaviour is already implemented (`game/state/city/vehiclemission.cpp:3137-3197`,
`advanceMissionCounterOnArrival`) and hooks into the ordinary per-mission update path — an existing seam.
U1(b) has no equivalent existing hook: `City::dailyLoop` (`game/state/city/city.cpp:319-331`) currently only
repairs scenery and updates building workforce; it has no per-vehicle loop at all. This is a genuine
"nothing calls a damage-band check on any cadence today" gap — but it is an ordinary implementation
decision (add a check somewhere reachable, using data that already exists), not a missing-concept gap in
the U2(b) sense (where OpenApoc had no seam capable of receiving the behaviour *at all*, even in principle).
Two existing, reasonable hook points, named without prescribing between them: (a) reactively, alongside the
existing `wasBelowCrashThreshold` check at `vehicle.cpp:2686` where damage is already applied and
`crash_health` is already compared against; or (b) periodically, inside `City::dailyLoop` or an equivalent
per-vehicle per-tick pass, closer to the original's own cadence. Either is buildable today with zero new
data model fields.

**Verdict: BOUND, and mappable — not a clean "everything already exists," but not a U2(b)-style "no seam at
all" either.** The consequence, the band's two endpoints, and the trigger's data source (`role[slot]`,
already parsed by the extractor, just not retained) all exist; the one missing piece is a small, nameable
lookup table. The lock test: for an alien-owned vehicle not on a `FollowVehicle`-style mission, once
`crash_health <= getHealth() < percentForRole(role) × getMaxHealth() / 100`, without the change no such
vehicle ever gets a `gotoPortal` mission triggered by its damage state (only by mission-counter arrival,
U1(a)'s existing behaviour); with the change, it does — for the roles where that band is non-empty (not
Escort or the unnamed role 9, per §4.4's structural-emptiness finding). Implementing it faithfully needs:
(1) retaining `UfoMissionData::role[slot]` through extraction (the byte is already parsed, just discarded),
and (2) a small new `role → percent` table (6 values suffice for this population). Both are concrete,
disclosed, and small — this is an ordinary extraction gap, not a missing-concept one.

---

## 5. What remains open

- **`DAT_0012d950` and `type_percent` were not directly diffed against each other value-for-value.** §4.4
  argues structurally that they are separate design values (sibling struct fields feeding different
  formulas), but this session did not dump all 45 `UfoMissionData` records' `type_percent` alongside
  `DAT_0012d950[role]` to prove they never coincide — a cheap follow-up if the structural argument needs
  double-checking before extraction work begins.
- **`FUN_0006d384`'s own callers**, and how its two `FUN_0006da88` call sites (§4.4) decide *which* one
  fires for a given incursion — i.e. what determines whether a spawn gets the hardcoded role `9` versus a
  `role[slot]` pulled from the mission-data record — were not traced this session.
- **`DAT_000d5060`'s exact identity.** Read 236 times, written by only 5 functions, consistent with "the
  organisation/side currently being processed in a per-org daily pass," but not proven to be exactly the
  27-entry organisation-table index `U1-scheduler-population.md` independently bound. If it is that index,
  §3's caveat about same-tick consumption applies as described; if it is a coarser 0/1 "human vs. alien"
  discriminant instead, the caveat may not apply at all. Not resolved this session.
- **`+0x271`'s semantics.** Read and written across at least 10 functions (`FUN_00038a44`, `FUN_00058b88`,
  `FUN_00059e60`, `FUN_0003c090`, `FUN_00059148`, `FUN_0004b2a4`, `FUN_000392ec`, `FUN_000114cc`,
  `FUN_000395e4`, `FUN_000636f0`, `FUN_00060560`, `FUN_0005e980`); one site (`FUN_000114cc`, VA `0x11661`)
  decrements it (`SUB byte ptr [EDI+0x271],AL`), consistent with a cooldown/busy counter rather than a
  boolean, but no name is asserted.
- **The second notification block inside `FUN_000588f8`** (§2.3, VA `0x58adc`–`0x58b76`, gated on the
  `+0x162` damage-accumulation tracker) was not chased to a conclusion. It reads as a distinct "recent
  damage" notification separate from the withdrawal mechanism, but that is not proven.
- **`FUN_000105b8`'s own caller** (the top of the chain driving `FUN_0005760c`'s cadence) was not traced;
  the "periodic, calendar-staggered" characterization rests on the calendar-decomposition arithmetic and the
  mod-18/mod-36 staggering inside `FUN_0005760c` itself, not on confirming the exact outer scheduling loop.
- **`DAT_000d4d58`'s meaning** (the counter gating how many of the mod-18 phases are "active," compared
  against 2/4/6 in this function and against other constants like `0x258` elsewhere) was not pursued — 94
  total references, too broad to resolve incidentally.
- **`FUN_00051348`** (called from the order-type-0 message-building path, §2.2) was not examined — confirmed
  irrelevant to the order-type-1 population this task is about, so not chased further.
