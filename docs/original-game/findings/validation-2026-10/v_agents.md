# v_agents: agent growth, training, healing, hiring vs UFO2P.EXE / TACP.EXE

Scope: engine rules `game/state/shared/agent.cpp` (training/healing), `game/state/battle/battleunit.cpp:4434` (`processExperience`), `game/state/shared/organisation.cpp:541` (`updateHirableAgents`), `game/ui/base/recruitscreen.cpp` (hire cost) against the original executables.
EXE authority: `UFO2P.EXE` ISO non-4 (CRC `0x4749ffc1`) and `TACP.EXE` ISO non-4 (CRC `0xfebbe39e`) from `OpenApoc-og-research/canonical/`. Ghidra private copies `v_agents_t` (TACP) and `v_agents_u` (UFO2P), read-only, helper script `validate/gh/VAgents.java` (raw outputs `validate/gh/{tacp,ufo}*.out`).
LIVE == HEAD for every file cited below except `gamestate.cpp` and `battle.cpp` (line shifts of 1 to 33 lines); `agent.cpp`, `organisation.cpp`, `base.cpp`, `recruitscreen.cpp`, `battleunit.cpp` (outside line 5754) are byte-identical, so their line numbers hold for both.

## 0. Headline (OBSERVED first, inference second)

OBSERVED
1. UFO2P has one routine that trains and heals every person record: `FUN_0006c058` (physical file `0xCE6FC`). It converts two kinds of accumulators into stat changes and is called exactly once, from `FUN_0006bc70` (file `0xCE314`), which also decrements facility construction counters. Accumulators are fed in `FUN_0006bf04` (file `0xCE5A8`), 4 calls per game hour.
2. `FUN_0006bc70` is reached from `FUN_000105b8` only on the path where the city "day loop" `FUN_00010908` returns 0, and `FUN_00010908` returns 0 exactly when the calendar weekday differs from the weekday at entry (listing `0x10969: CMP EAX,EBP / JNZ 0x11366`, `0x11366` returns `AX=[0xE1328]`, which is cleared at entry). So the conversion runs **once per game midnight**, not every 4 hours.
3. Per-roll rules in `FUN_0006c058` match the engine for reactions, speed, strength, health and the psi formula. They differ from the engine for accuracy (cap 96, chance `(96-a)/100`), stamina (+1 raw, engine +2 raw), psi (clamped to 100/99/100, engine unclamped) and in eligibility (wounded agents do not train).
4. `TACDATA/EXPERIEN.DAT` is **not** a soldier-experience table. It is a log of fixed 114-byte (`0x72`) AI sample records (type tag 2) appended by the battle loop (`FUN_000a12a4`, ring buffer of 100 records at VA `0x276BD4`), merged from `TMPEXP.DAT` at the debriefing (`FUN_000a1324`) and read by floating-point routines (`FUN_000a15f4`, `FUN_0009f2b0`) tied to `BRAIN.DAT`/`WEAPEXP.DAT`. The real post-battle soldier award is `FUN_000b3f74` in TACP (file `0x10EA18`), a 449-byte function.
5. Daily hire-pool generation is `FUN_0007a474` (file `0xDCB18`): per class, random departures, then `rand(0..3)` arrivals bounded by a cap table 100/50/50/50 that counts employed staff, plus up to one hybrid and one android per day gated on org relation `>= 0`. Hire fee `200*(pay+1)`, fire cost 0.

INFERRED (flagged where used)
- Strategy: with one roll set per agent per game day, "train every idle agent" is still free value, but stat growth is roughly 1/2 to 1/6 of what the benchmark engine produces over the first 4 to 8 weeks (tables in section 3.3).

## 1. Method and controls

Binary conversion rules used (verified by prologue bytes against Ghidra `ENTRY_BYTES`, see exe_evidence_index section 3): UFO2P code file = VA + `0x626A4`, UFO2P data file = VA + `0x616A4`, TACP code file = VA + `0x5AAA4`, TACP data file = VA + `0x4FAA4`. Every function VA below was checked: opcodes at the physical offset equal the Ghidra entry bytes (only relocated operands differ). All Ghidra VAs are session labels (`FUN_<VA>`).

Tool path: strings `Combat training (efficiency=` etc. are pool strings. UFO2P pool = 128 slot pointers at VA `0xF3794`, resolver `FUN_00063A00` (`EAX`=group, `EDX`=item; entry bytes `53515689d625ffff0000 8b048594370f`: `and eax,0xffff; mov eax,[eax*4+0xf3794]`). `rscan` (own script) enumerates every resolver call, recovers the `MOV/XOR EAX,imm` and `EDX,imm` pair and resolves it through the pool. TACP pool = 53 slot pointers at VA `0x292D18`, resolver `FUN_0005b770` (`EAX`=slot, `EDX`=skip).

Controls (positive): (a) `rscan` recovers the agent-screen strings at `FUN_0004f290`, each confirmed independently at its call site in the `0x4F5xx` listing: `Wounded` (`0x4F552`, g37 i240), `Not assigned to training` (`0x4F5D7`), `Psionic training (efficiency=` (`0x4F656`), `Combat training (efficiency=` (`0x4F6F8`), and `New recruit arrived: ` (`0x7AFF8`, `FUN_0007ae60`). (b) Independent cross-source checks that the routines identified are the right ones: hire-class cap `100` equals UFOpaedia "max 100 agents"; psi training constant `3 x initial` equals Wong/UFOpaedia; accuracy floor `4` (cap 96) equals UFOpaedia/Wong; heal arithmetic gives 20 HP/day = Wong's 0.8 HP/h; the TACP promotion thresholds in `FUN_000b4154` equal Wong's ratios for the Squaddie, Captain, Colonel and Commander tiers (Sergeant matches UFOpaedia's `> 3` instead).
Negative-result caveat: the absence of an auto-assignment writer for starting soldiers (section 4.5) rests on a `MOV/INC/ADD/XOR [reg+0xC8]` operand scan plus reading the creation routine, not on a successful decompile of the whole new-game path (`FUN_000b94c0`/`FUN_000799dc` not decompiled).

## 2. Person record (UFO2P runtime, 206 bytes, 180 slots) as far as decoded

Table `0x144918`, stride `0xCE`, 180 slots (`0x144918 + 180*0xCE = 0x14D9F0`). Fields used here (offset from record base):
`+0x21` owner org (0 = X-COM employee, nonzero = applicant of that org; set by `FUN_000791e0` from its `DL` argument), `+0x23` class (0 agent, 1 biochemist, 2 mechanic, 3 physicist; it equals the creation type index), `+0x2E..+0x37` snapshot of initial speed, health, stamina, reactions, strength, bravery, accuracy(raw), psi energy, psi attack, psi defence (copied from `+0x3A/+0x3C/...` at creation), `+0x3A` speed, `+0x3B` max health, `+0x3C` current health, `+0x3E` stamina (raw, doubled scale, cap 200), `+0x3F` reactions, `+0x40` strength, `+0x41` bravery, `+0x42` psi energy, `+0x43` psi attack, `+0x44` psi defence, `+0x46` accuracy stored as raw "inaccuracy" (smaller is better, floor 4), `+0x57` pay byte, `+0xB0` "listed as available applicant" flag, `+0xB3` = 2 for androids (type 14), `+0xC8` training assignment (0 none, 1 psionic, 2 combat). Parallel accumulator array at `0x13EBD0`, stride 12: `{A0 heal, A1 combat, A2 psi}`.
Stat generation at creation (`FUN_000791e0`, file `0xDB884`): each stat = `agent_type_data` base + `rand16(inc)` (uniform), tables at VA `0xF3ED0 + type*106` (file `0x155574`). Type 0 human, 14 android (`psiD` base 100, inc 0), 15 hybrid. This is the same min/max the engine extractor produces, so recruit stat ranges match.

## 3. Training and healing

### 3.1 Evidence

Accrual, `FUN_0006bf04` (UFO2P file `0xCE5A8`), per person with `+0x21==0` who is at the home base (building == home, or in a vehicle parked in the home building):
```
if cur_health(+0x3C) < max_health(+0x3B):        # wounded: heal only, no training
    v = ((DAT_000dc158>>16) * 10 * F_medical) / N_wounded ;  v = min(v,10) ; A0 += v     # DAT_000dc158>>16 = 10 (file 0x13D7FE)
else if assign(+0xC8) == 2:                      # combat/physical
    v = ((DAT_000dc168>>16) * 10 * F_training) / N_combat_trainees ; v = min(v,10) ; A1 += v   # =10 (file 0x13D80E)
else if assign == 1:                             # psionic
    v = ((DAT_000dc178>>16) * 10 * F_psigym) / N_psi_trainees ;       v = min(v,10) ; A2 += v   # =10 (file 0x13D81E)
```
`F_*` and `N_*` are per-base counts built by `FUN_0006bdac` (file `0xCE450`): `F` counts finished facility cells (`cell[+0x56]==0`) of the facility type, `N` counts wounded / combat trainees / psi trainees in the base. The UI line `Combat training (efficiency=%d%%)` at `FUN_0004f290` computes `min(100, cap * F * 100 / N)` from the same counts (listing `0x4F5E3..0x4F6E3`), and shows `No training facilities in base` / `No Psi-gyms in base` when it is 0.
Callers: `FUN_0004aa7c` (file `0xAD120`) is called by the calendar `FUN_0004ac40` (file `0xAD2E4`) once per game minute with `EAX = minute-of-hour` (`0x4AE7A: MOV EAX,[0xD4D68]; SAR EAX,16; CALL 0x4AA7C`). Inside it: `if minute % 30 == 0 and dimension flag 0: CALL FUN_0006bdac x2; CALL FUN_0006bf04 x2` (listing `0x4AADD..0x4AAFA`). So **minute 0 and minute 30, two calls each = 4 accrual calls per game hour**, i.e. at most `40` points per hour and `960` per day to one accumulator.

Conversion, `FUN_0006c058` (file `0xCE6FC`, called once from `FUN_0006bc70` at `0x6BD96`), for each of the 180 slots:
```
heal:    cur = min(max, cur + A0/48) ; A0 = 0                                   # every call, whether or not wounded
if +0xB3 != 2 (not android):
    if A1 >= 160:  A1 = 0
        if rand(0..99) <  raw_acc - 4 : raw_acc--                               # accuracy, floor raw 4 (accuracy cap 96)
        if rand(0..99) >= reactions    : reactions++
        if rand(0..199) >= stamina_raw : stamina_raw++
        if rand(0..99) >= speed        : speed++
        if rand(0..99) >= strength     : strength++
        if rand(0..99) >= max_health   : max_health++ ; cur_health++            # both
    if A2 >= 160:  A2 = 0
        if rand(0..99) < 3*init_psiA - psiA : psiA = min(100, psiA + psiE/20)
        if rand(0..99) < 3*init_psiD - psiD : psiD = min( 99, psiD + psiE/20)
        if rand(0..99) < 3*init_psiE - psiE : psiE = min(100, psiE + 1)
```
(`rand16(n)` = `(...) % (n+1)`, inclusive `[0,n]`: asm `0x5D24F: INC ECX; IDIV ECX`, so `rand(0..99)` is `rand16(99)` and `P(rand < x) = x/100`. Exact stat offsets and thresholds are from the listing `0x6C078..0x6C25A`.)
Cadence of `FUN_0006c058`: see headline item 2. `FUN_000105b8` listing: `0x10797 MOV ESI,EAX` (result of `FUN_00010908`), `0x107A7 TEST SI,SI / JNZ 0x10832`, `0x107B0 JMP 0x1080E` (SI==0 continues the dimension countdown), then after the loop `0x1083B INC [0xD4D64]` and `0x10842 CALL 0x6BC70`. `FUN_00010908` listing `0x10969..0x1097F`: weekday word `[0xD4D64]` compared with the entry weekday `[0xE131C]`; on mismatch jump to `0x11366` which returns `AX=[0xE1328]` (cleared at `0x10926`). Event interruptions set `[0xE1328]=1` (`0x108B4`) and return nonzero, which exits `FUN_000105b8` without the daily block.

### 3.2 Engine behaviour (LIVE == HEAD for these files)

- `game/state/shared/agent.cpp:23-26`: 4 engine-hour cadence, "prior-art" comment (issue 997). `agent.cpp:1090-1099` healing: `modified.health < current.health && !recentlyFought`, `usage = max(100, usage)`, `healingProgress += 80/usage` per hour (0.8 HP/h at <=100%), only if `usage < 999` (a Medical Bay exists). `agent.cpp:1105-1127` training per hour: `trainPhysical/trainPsi(TICKS_PER_HOUR*100/usage*mult)`, no health gate. `agent.cpp:1182-1217` `trainPhysical`: `rand(0,100) >= stat` for health/accuracy/reactions/speed/strength, stamina `rand(0,2000) >= stamina -> += 20` (`:1207-1209`). `agent.cpp:1219-1257` `trainPsi`: `rand(0,100) < 3*initial - current`, attack/defence `+= psi_energy/20`, energy `+= 1`, no clamp.
- `base.cpp:502-560, 649-662`: usage = assigned agents with this home / capacity; `base.cpp:621` counts only finished facilities.
- `gamestate.cpp:792-804` (HEAD) / `:803` (LIVE): the 10 starting soldiers are auto-assigned Psi if `initial_stats.psi_energy > 30` else Physical. `agent.cpp:402-409` `hire()` leaves `trainingAssignment == None`.
- `battle.cpp:3060` (both): `processExperience` is applied to every `allowsDirectControl` unit after the battle.

### 3.3 Comparison and consequences (expected value, mean-field, my arithmetic on the decoded rules)

Physical stat that rises with `P = (100-a)/100` (reactions, speed, strength, health), expected value after N game days of continuous training:

| start | EXE d7 / d14 / d28 / d42 / d56 | engine d7 / d14 / d28 / d42 / d56 |
|---|---|---|
| 15 | 21 / 26 / 36 / 44 / 52 | 44 / 63 / 84 / 93 / 97 |
| 25 | 30 / 35 / 43 / 51 / 57 | 51 / 68 / 86 / 94 / 97 |
| 35 | 39 / 44 / 51 / 57 / 63 | 57 / 72 / 88 / 95 / 98 |
| 50 | 53 / 57 / 62 / 67 / 72 | 67 / 79 / 91 / 96 / 98 |

Accuracy under the EXE rule (cap 96): 15 -> 21/26/35/43/50, 25 -> 30/34/42/49/56 at d7/14/28/42/56. Raw stamina (display = raw/2): EXE 80 -> 84/88/96/109 at d7/14/28/56; engine 80 -> 121/148/178/196. Psi Monte-Carlo (1,500 trials): hybrid (45,40,40) after 28 days EXE (1 set/day, clamped) `67/79/79`, engine (6 sets/day, unclamped) `118/121/121`; after 56 days EXE `84/99/98`, engine `132/122/122`.
Interpretation: the engine runs 6 roll sets per game day, the EXE at most 1. The engine's first-month stat curve is therefore far steeper than the original's. Efficiency is also very different: in the EXE `v = min(10, floor(100*F/N))` and a roll set needs `A >= 160`, with `A` growing by `96*v` per day when the agent is at the base all day, so any `v >= 2` (up to 50 trainees per facility) still yields 1 set/day if the agent spends enough hours at the base (hours needed per day = `ceil(160/(4v))`: v=10 -> 4 h, v=5 -> 8 h, v=2 -> 20 h, v=1 -> 40 h, i.e. every second day). The engine slows linearly (`100/usage`) as soon as usage exceeds 100%.

Healing: wounded agent at the home base with a finished Medical Bay accrues `4*v` per hour (v=10 when `N_wounded <= 10*F`), converted at midnight to `floor(A0/48)` HP: 20 HP per day at full rate (0.83 HP/h), `A0 % 48` discarded, `A0` zeroed every midnight even when 0 HP. Without a finished Medical Bay `F=0 -> v=0`: no healing (engine also 0: `usage == 999`). Engine 0.8 HP/h = 19.2 HP/day: **match within 4%**. Differences: the EXE has no `recentlyFought` block and heals in a lump at midnight; engine blocks healing until the daily rollover after a fight and heals hourly.

## 4. Findings in detail

### 4.1 V-AG-01 Training cadence (diverges, high impact)
EXE: at most one roll set per agent per game midnight (section 3.1). Engine: one set per 4 engine hours = 6 per game day at usage <= 100%. Fix: replace the `TICKS_PER_*_TRAINING` accumulator in `Agent::updateHourly` with the EXE accrual (`progress += v` four times per game hour, i.e. `+= 4v` hourly while eligible at the home base) and perform the roll set in `Agent::updateDaily` when `progress >= 160` (then `progress = 0`). Keep the `OpenApoc.Cheat.StatGrowthMultiplier` hook on the accrual.
Cross-check that supports daily over 4-hourly (independent of the control-flow trace): `FUN_0006bc70` decrements the facility construction counter `cell+0x56` in the same pass that calls `FUN_0006c058`. `facility_data` build times are in days (Training Area 5, record at file `0x13D804`) and the engine decrements `buildTime` once per day (`gamestate.cpp:1567` LIVE / `:1534` HEAD, in `updateEndOfDay`). If this routine ran every 4 hours, original facilities would finish 6x faster than their documented day counts.
Caveat: the 4-hour figure in Wong/engine matches the accrual constants (`160 = 4 h x 40 points/h`); the EXE accumulates toward that threshold but converts it only at midnight. Confidence: high on the per-roll rules and accrual constants; medium-high on the midnight cadence (explicit control flow plus the build-time cross-check; I did not run the game). I traced the only return-0 path of `FUN_00010908`; if some other path reached the post-loop block, the sets per day would rise.

### 4.2 V-AG-02 Training capacity / efficiency curve (diverges, medium)
EXE `v = min(10, floor(cap*10*F/N))` with `cap=10` for both Training Area and Psi-gym (file `0x13D80E`, `0x13D81E`), facility capacity 10 in `facility_data` (record at file `0x13D804`: cost 8000, build 5 d, upkeep 500, capacity 10). The UI "efficiency" shows `min(100, 10*F*100/N)`. Effective throughput does not fall until `N > 50F`. Engine: `100/usage`.

### 4.3 V-AG-03 Physical per-stat rolls (diverges, low to medium)
Accuracy: EXE `P = (96-a)/100`, cap 96, gain +1 (raw `rand(0..99) < raw-4`); engine `P = (100-a)/100`, cap 100 (`agent.cpp:1194-1196`, no clamp at 96). Stamina: EXE `P = (200-s_raw)/200`, +1 raw; engine `rand(0,2000) >= stamina_internal` with internal = 10 x raw, so same P but `+20` internal = +2 raw, double the EXE gain (UFOpaedia: "+0.5" on the display scale = +1 raw). Reactions, speed, strength, health: identical shape (`P=(100-x)/100`, +1; health raises both max and current).

### 4.4 V-AG-04 Psi training (engine_bug: no clamp; formula matches)
Chance `3*initial - current` (%), attack and defence `+= psi_energy/20`, energy `+= 1`, same roll order: **identical** to `agent.cpp:1219-1257`. EXE clamps attack at 100, defence at 99, energy at 100 (`0x6C1CF..0x6C257`); the engine does not (hybrid max overshoots to ~122). Androids never train (`+0xB3 == 2`). Humans with energy < 20 gain 0 attack/defence per roll until energy reaches 20 (`psi_energy/20` integer): same in both.
Engine psi attack chance: `getEffectivePsiDefence()` clamps defence to 99 (`battleunit.cpp:126-128`), so androids (defence 100) keep a ~1% residual at best; the TACP psi success formula was **not located** in this pass (exe_unbound).

### 4.5 V-AG-05 Eligibility and defaults (diverges, medium)
EXE: wounded agents (cur < max) take the heal branch and never train; androids excluded; agents must be at their home base; assignment `+0xC8` is set to 0 at creation (`FUN_000791e0` writes `[rec+0xC8]=0` at `0x795A1`) and is written nonzero only by the two UI handlers `FUN_00051D0C`/`FUN_00051D98` (stores at `0x51D5F`, `0x51DF4`); the hire commit `FUN_0007ea7c` does not touch it. A scan of `[reg+0xC8]` stores across the code region found no other nonzero writer, and the 10-agent new-game generator calls `FUN_000791e0`, so starting soldiers are **not** pre-assigned (INFERRED, section 1 caveat). Engine: injured agents train (no health gate), and the 10 starting soldiers are pre-assigned (`gamestate.cpp:802/803`). Fix: gate `trainPhysical/trainPsi` on `modified.health >= current.health`; if fidelity is wanted, drop the initial assignment (the harness can press `BUTTON_AGENT_PHYSICAL`).

### 4.6 V-AG-06 Healing (match within 4%; two small differences)
See 3.3. No EXE evidence for the `recentlyFought` gate. Fix: none required for the benchmark; optional: lump-heal at midnight with `floor(A0/48)`.

### 4.7 V-AG-07 Hire pool generation (diverges, medium)
EXE `FUN_0007a474` (file `0xDCB18`, called from the calendar day rollover at `0x4AE0A`, city only), listing `0x7A474..0x7A72D`. Caps (code-region constants at VA `0x78300`, file `0xDA9A4`): `{100, 50, 50, 50}` for classes `{agent, biochemist, mechanic, physicist}` and they count employed staff plus listed applicants. Each day, per class `t`:
1. `n_t` = applicants of class t (owner != 0 and listed, `+0xB0 != 0`). `depart_t = rand(0 .. floor(n_t/3)+1)`; the first `depart_t` such applicants in slot order are removed (`FUN_0007ad3c`).
2. `total_t` = employed + listed applicants. `arrive_t = min(rand(0..3), cap_t - total_t)`; each arrival is `FUN_000791e0(type=t, org=rand(0..20)+2)`, i.e. origin org uniform in 2..22, then `+0xB0=1`.
3. If `relation[Mutant Alliance][X-COM] >= 0` (signed byte at VA `0x16EEC8`, = base `0x16EC28` + 24*28): `min(rand(0..1), cap_0 - total_0)` hybrids (type 15, stored as class 0). If `relation[S.E.L.F.][X-COM] >= 0` (`0x16EEAC`): same for androids (type 14).
Steady state (my arithmetic): class 0 inflow 1.5 + 0.5 + 0.5 per day gives ~12 listed (about 7 humans, 2.4 hybrids, 2.4 androids), technical classes ~6 each. Engine steady state ~6.8 / 2.4 / 2.4 and ~7.6 per technical class, so pool *size* is close but the *mechanism* differs: the EXE does not condition human or technical arrivals on any org relation (engine: Megapol/Gravball/etc. and `25+relation` chance, hostile orgs give none), has no 33% independent departure, has a hard cap including employees, and gates hybrids/androids on relation `>= 0` only (engine chance `25+rel`, two rolls). Not examined: the UI list builder (`FUN_0007D0E0`-class) may filter applicants by origin org; whether `FUN_00099130` shifts starting relations by difficulty (engine does `10-5d`).
Impact for the strategy: do not plan on Megapol relation to supply soldiers; plan on a headcount cap of 100 including employees.

### 4.8 V-AG-08 Hire cost, fire penalty, salary (match)
`FUN_0007e6e0` (file `0xE0D84`): `iVar1 = rec[+0x57] + 1`; hiring an applicant subtracts `iVar1 * 200` from the running balance `[0x1A6084]` (message and refusal if it would go negative), unselecting adds it back; firing an employee changes nothing. `FUN_0007ea7c` (file `0xE1120`) commits and writes the balance back to the X-COM organisation record. Pay bytes (`agent_type_data +0x2E`, file `0x155574 + 106*t + 0x2E`): type 0 -> 2 (600), types 1-3 -> 3 (800), android 2, hybrid 2. Engine: `agent_salary` 600/800 charged on hire (`recruitscreen.cpp:520`), `agent_fired_penalty` 0 (`:554`). Hire also needs free living space in the target base (`FUN_0007c884` computes quarters left).

### 4.9 V-AG-10..13 Post-battle experience (TACP `FUN_000b3f74`, file `0x10EA18`, VA `0xB3F74`)
Called from `FUN_00010294` at `0x10A37` immediately after the debrief driver `FUN_000b3b28`, unconditionally. It loops the 60 battle-unit records (stride `0x49C`, base `0x109ED0`) and processes units that are alive (`short +0x64 > 0`), have `short +0x20 == 0`, and `byte +0x96 != 2` (androids excluded). Rules (decompiler text, field names inferred from roles):
- Accuracy (raw `+0x8E`, hit counter `+0x8C` capped at 40): `raw' = ((raw-4) * (100 - floor(hits/4))) / 100 + 4`, i.e. gain `ceil((96-a)*floor(hits/4)/100)`, cap 96. **Identical** to `battleunit.cpp:4440-4445`.
- Reactions (`+0x6C`, counter `+0x6E`): gain `min(counter / reactions, 5)`, cap 100: identical to `:4446-4452`.
- Health (`+0x62/+0x64` max and current, kill-score counter `+0x42`): `+min(floor(killScore/15), 2)`, cap 100: identical to `:4471-4480` (>=15 -> 1, >=30 -> 2).
- Stamina (`+0x68` raw, counter `+0x6A`): gain `counter / stamina_raw`, cap 200. Speed (`+0x5A`): `+min(floor(staminaGain/2), 5)`, cap 100. Engine `:4453-4470`: `rawGain = floor(staminaSpent/20)/displayStamina`, speed `+min(rawGain,5)` (**no halving**) and stamina `+rawGain*10` internal. Speed gain is 2x too generous in the engine (`:4459-4462`); the stamina gain shape is the same (`:4453-4467`, engine internal stamina is 10 x raw, so `+rawGain*10` internal is `+rawGain` raw); the stamina counter units are not comparable without calibration (EXE counter = stamina points actually consumed: `DEC [+0x69]; INC [+0x6A]` per movement step, listing `0x65D92..0x65DC1`; engine counts 10 or 30 per real-time second).
- Bravery (`+0x73`, stored in tens, <10): `+1` (tens) when `floor(killScore / b) >= 6` (i.e. `killScore >= 6b`), skipped if `b == 0`. Engine: `+10` when `2*killScore > bravery` (`killScore > 5b`), also when bravery is 0. Engine is slightly easier.
- No code for strength or psi: gym only, as in the engine.
Counter accrual sites (not fully decoded): accuracy counter `+0x8C` is incremented at `FUN_0005d25c` `0x5DE4E` by `BX = floor(([proj+0x156]>>16)/0x18 / 8)`, divided by 4 when the weapon class flag `[0x2B1F00 + type*0x26]` is set, minimum 1, only when the projectile's owner side equals the current player side; the engine adds 1 per hit (cap 40). Reaction counter `+0x6E` is incremented at `FUN_0006871C` `0x68D72` after a reaction-fire eligibility computation against a hostile (table `0x2B2854` relation == 1); engine: "hit while in interrupt list". These two triggers differ from the engine's description and are recorded as exe_unbound for the exact semantics.

### 4.10 V-AG-14 EXPERIEN.DAT (docs error; AI-learning store, live in single player; engine_missing)
OBSERVED (data): `depot_7661/XCOMA/TACDATA/EXPERIEN.DAT` (Steam depot copy, not the ISO generation the EXEs come from) = 899,346 bytes = 7,889 x 114-byte records, no header; each record = `u16 = 2` then 28 little-endian int32 (small enums, health pairs such as 44/44, ids <= 50, distances up to 1000): encounter samples, not per-soldier stats. Because the game appends to this file (below), the record **count** is a property of that played or seeded copy; the 114-byte stride and tag 2 are properties of the format (confirmed in code). `WEAPEXP.DAT` = 34,320 bytes = 17,160 (`0x4308`) u16; `FUN_00014948` (battle init, file `0x6F3EC`) pre-fills two 0x4308-entry arrays with `0x32` and then loads `WEAPEXP.DAT` (`FUN_0009ed00`).
OBSERVED (code): `FUN_000a12a4` (file `0xFBD48`, called from the battle function `FUN_00095490` at `0x9556F` and from `FUN_000a1148`) opens `TACDATA/EXPERIEN.DAT` (flags `0x231`, mode `0x180`), walks a 100-entry ring buffer at VA `0x276BD4` (stride `0x72`), writes every record whose flag byte is 2, clears the flag and sets `DAT_0027985C = 1`. At the debriefing, `FUN_000b3b28` (file `0x10E5CC`) runs, when `DAT_0027A805 == 0 && DAT_001B7BAC == 0 && DAT_0027985C != 0`: `FUN_000b2098`, the pool string `Experience Processing` (`FUN_0005B770`, group 35 item 17, `rscan` pair hit at `0xB3C07`), `FUN_000a1324` (file `0xFBDC8`: opens `TMPEXP.DAT` and `EXPERIEN.DAT`, copies in 100-unit steps through `FUN_000b180c`), then `FUN_000a15f4` (3,315 B, floating point) and `FUN_00013E64`. `DAT_0027A805` is set by a command-line option in `FUN_00010294` (`0x1044B`), `DAT_001B7BAC` is a network/mode flag (writers `0x6FEEE` = 1, `0x7054B` and `0x705D5` = 2), so both are 0 in a normal single-player campaign and the path is **live**. UFO2P keeps pristine `WEAPEXP0.DAT`/`BRAIN0.DAT` strings (file `0x1349B8/0x1349D0`), consistent with a new-game reset.
INFERRED (medium): this is the original's AI learning store (weapon-effectiveness table and a model refreshed from logged encounters). Consumers of the loaded `WEAPEXP`/`BRAIN` data inside the alien decision code were not decoded, so whether and how much alien behaviour changes across a campaign is unknown. It contradicts `docs/campaign-plan.md` section 16.1's statement that nothing carries tactical state between battles and that the only `EXPERIEN.DAT` machinery is the agents' stat award; the engine has no equivalent. Strategy impact: low to medium and unquantified.

### 4.11 V-AG-16 Promotion thresholds (side finding, low)
`FUN_000b4154` (file `0x10EBF8`): Rookie -> Squaddie when combat rating > 8; Squad Leader -> Sergeant when `SL/(Sgt+1) > 3`; Sergeant -> Captain `Sgt/(Cpt+1) > 2`; Captain -> Colonel `Cpt/(Col+1) > 2`; Colonel -> Commander when no commander and `Col > 1`; max 5 promotions. Engine `battle.cpp:3096-3101` (both trees) uses `> 2` for Squad Leader -> Sergeant (all other tiers equal). The Squaddie -> Squad Leader expression reads `Squaddie/(SL+1) + Rookie > 4` in the decompiler (precedence may be a decompiler artefact; engine uses `(Rookie+Squaddie)/(SL+1) > 4`). Rank only affects morale.

## 5. What the strategy may rely on

- Training: every non-android, non-wounded agent that sleeps at a base with a finished Training Area (physical) or Psi-gym (psi) gets at most one roll set per game day if it spends enough time at base; wounded agents must be healed first (needs a finished Medical Bay). Expect ~1/6 of the engine's growth per game day. Psi training is only worthwhile for hybrids (humans with energy < 20 gain no attack/defence per roll).
- Facilities: capacity 10 per facility gives full rate (`v=10`); up to 50 trainees per facility still get one set per day if they are home about 20 hours; beyond that throughput halves quickly. Facilities under construction do not count.
- Hiring: fee = one week of salary (600/800), no fire penalty, 100/50/50/50 headcount cap including employees, hybrids/androids appear only while Mutant Alliance / S.E.L.F. relation is `>= 0`. Starting soldiers must be assigned by the player.
- Combat experience: the accuracy / reaction / health formulas are exact; speed gain is half of what the engine gives; bravery slightly harder.
- Wounded healing at a full-rate Medical Bay is ~20 HP/day; fully rested within a few days. This matches the engine.

## 6. Open items / next leads (exe_unbound)

1. Psi attack success formula and any android immunity cap in TACP (not located). Lead: `psionics.txt` costs plus the psi-use function near the `Psi-drain` pool string (TACP slot 32).
2. Exact semantics of the three XP counter increments (`0x5DE4E`, `0x68D72`, `0x65DC1`): decompile the surrounding functions fully; calibrate stamina units (steps per tile, run flag `[+0x21C]==2`).
3. Whether the hire-screen list builder filters applicants by origin org or relation, and whether `FUN_00099130` (difficulty init, file `0xFB7D4`) shifts relations of Mutant Alliance/S.E.L.F.
4. New-game starting assignment: decompile `FUN_000799dc` and `FUN_000b94c0` to confirm no assignment is written.
5. `FUN_0007ec60` (called on hiring class-0 agents) updates a 87-entry per-base table at `base+0xF8`; purpose not decoded.
6. AI learning (V-AG-14): decode the 114-byte record layout, the `WEAPEXP`/`BRAIN` consumers in the alien decision code, and whether `WEAPEXP.DAT` values are read when choosing weapons; the pipeline itself is confirmed live.

## 7. Query log

TACP (copy `v_agents_t`): batch 1 string/xref/resolver pair and pool map; batch 2 EXPERIEN/WEAPEXP/BRAIN refs and decompiles of `FUN_000b3b28/79d94/79df8/9ec78/9ed00/14948/a1324/7c968`; batch 3 `FUN_000b3d54/b4154/b47f0/b2098/3980c/13e64/10294/a15f4`; batch 4 `FUN_000b3f74/b1614/15ca0/b17f4/57648`; batch 5 displacement scan of experience counters; batch 6 listings around the counter increments.
UFO2P (copy `v_agents_u`): 12 batches: pool/rscan of agent strings, calendar `FUN_0004ac40` and callees, hire pool `FUN_0007a474/791e0/7ad3c/7ae60/7e8b0/7e6e0/7ea7c/7c884`, agent screen `FUN_0004f290`, displacement scan for stat writers (found `FUN_0006c058`), accrual `FUN_0006bf04/bdac/bc70`, day loop `FUN_000105b8/10908/10010`, `rand16 FUN_0005d1d8`.
Both private copies are deleted at the end of the run.
