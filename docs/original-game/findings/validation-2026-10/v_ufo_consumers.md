# v_ufo_consumers: UFO schedule consumers, alien escalation, damaged-UFO withdrawal

Binary: `UFO2P.EXE`, ISO non-4 generation (CRC32 `0x4749ffc1`), read through a private copy of Ghidra project `OpenApocOG` (`-readOnly -noanalysis`, copy name `v_ufo_consumers`). Eleven headless invocations in eight batches (outputs `validate/call*.txt|full|clean`), about 40 functions decompiled, most with full listings. Engine side: `LIVE:` = frozen snapshot `scratchpad/live_src` (develop-reboot worktree), `HEAD:` = REPO worktree at committed fda36d6c. Line numbers are LIVE unless a HEAD number is given. LIVE and HEAD differ in `gamestate.cpp`, `vehicle.cpp`, `building.cpp`, `vehiclemission.cpp`, `gametime.h`; every cited line was checked in both (HEAD is 1 line earlier in `gamestate.cpp`, 25-45 lines earlier in `vehicle.cpp`/`building.cpp`/`vehiclemission.cpp`).

Notation: OBSERVED = decompile or listing text, data bytes, or save/XML content. INFERRED = my reading. Every code citation is `binary / generation / physical file offset (Ghidra VA, conversion)`; for UFO2P code `file = VA + 0x626A4`, for file-backed data `file = VA + 0x616A4`. The conversion was prologue-checked for all 33 functions cited (Section 1.3).

---

## 0. Results at a glance

| # | Rule | Verdict | One line |
|---|---|---|---|
| F1 | Weekly mission-type pick | match | `FUN_0006cfb4` (file `0xCF658`) reads `UFO_mission_patterns` row `clamp(week-1,0,19)`, slot `rand16(9)`: uniform over the 10 slots, as the engine does. |
| F2 | Incursion package selection | match | Lowest-index record in the type's range whose 3 slots all fit the pool; no fit means no launch and no retry. Engine `invasion()` is the same algorithm. All 45 records and the 94 craft/count/role/percent slots match the EXE byte for byte. |
| F3 | Fleet pool and weekly growth | match (one nuance) | Growth is `FUN_0006edac` (file `0xD1450`): `pool[t] = min(pool[t] + row[t], cap[t])`, row `clamp(week-1,0,15)`. The pool is the per-city UFO counter of city 1, i.e. physical UFOs in the alien dimension. Tables identical to the engine's XML. |
| F4 | What stops UFO growth | **diverges** | Growth is skipped when flag `DAT_000de62a` is set, which is set by destroying alien building **index 7 (Control Chamber)**, not the Organic Factory. Engine gates on the Organic Factory. |
| F5 | Control-Chamber "Apocalypse" launch | **engine_missing** | The same event sets `DAT_000f3c78 = 1`; the next incursion timer expiry launches up to 20 pooled ships (types 1-9 round-robin, no Probes), only if at least 10 can be gathered. Closes the C3/C4 "closed negative". |
| F6 | Invasion timer cadence | **diverges** (medium confidence) | First timer is the literal `10 h + rand(0..7200 s)`, engine uses 24-72 h. After that two code paths each drain the same counter; the second drain makes the effective interval about half the nominal 24-72 h at normal speeds. |
| F7 | Withdrawal threshold | **engine_bug** | EXE overwrites `+0x168` at spawn with `constitution x type_percent[slot] / 100`; engine uses the role table. `incursionTypeThreshold()` already implements the EXE formula and is never called. |
| F8 | Withdrawal mechanism | match (premise stale) | The task premise "not implemented" is stale: commit `34adbf58` implements it at HEAD and LIVE. EXE behaviour specified in F8. |
| F9 | Alien craft equipment score gates | match, timing and one unmodelled row | Table is 9 rows at `0xE0AD0`, not the 5 rows at `0x1421C4` the extractor reads. Rows 4-8 gate vequip 44-48 on UFO-kill score (lifetime); flags are latched daily. |
| F10 | Tactical-score gates (rows 0-3) | engine_missing / exe_unbound | Rows 0-3 are used (hexa calls them "not used"): Tactical Missions score gates items 42/50/51/52 inside the squad generator `FUN_000ad148`. |
| F11 | Building detection | **diverges** | `FUN_0006f964` found. Weights and formula match the engine; the "spotted" gate (dominant) and the halving rule differ. Sweep cadence matches at Superhuman (109 buildings, 110 min). |
| F12 | Alien movement | diverges (minor) | `FUN_0006f7f8` found. Chance formula `15+3M(+20)` matches; threshold `M>=3`, one shared random offset per building, and the friendly test differ. |
| F13 | Attack-incursion target choice | exe_unbound (partial decode) | `200 - dist - relation[Alien][owner] + rand16(100)` per candidate building; engine target rule not compared. |
| F14 | Food Chamber destruction and the Mutant Alliance | **engine_missing** | Building index 2 clears the Mutant Alliance's taken-over byte, sets it allied to X-COM and hostile to the aliens. The only EXE-bound takeover reversal found. Overturns the C2 "Sectoid rescue: NOT BOUND" verdict as a trigger (the branch is keyed on the building, not on rescues). |

The previous "empty xref" claim for `UFO_mission_patterns` / `UFO_growth_rates` is overturned (Section 1.2). It came from querying hexa linear VAs `0x128D64` / `0x128C10`, which lie inside `vehicle_data` in the Ghidra image.

---

## 1. Method, controls, address conversions

### 1.1 Instrument and controls

Script `RangeXrefs.java` (my own, in the private copy's script dir) takes a Ghidra data VA and length and prints (a) the first 40 bytes at the VA, to prove the right table is addressed, (b) every `getReferencesTo` hit on any address in the range, and (c) a raw dword scan for object-relative operands. Raw scan (c) proved useless as a completeness check: for example the 64 hits in the growth range are `mov reg, imm32` immediates such as `0x239` read one byte early. All conclusions rest on (a) and (b), cross-checked by reading the listing.

Controls (each queried with the same instrument in the same run, `validate/v_ufo_consumers_work/call1.txt`):

| Control | Ghidra VA | Result |
|---|---|---|
| `UFO_mission_data` (45 x 42 B), known consumers `FUN_0006cfb4`, `FUN_0006d384`, `FUN_0006da88` | `0xDC758` | refs found in exactly those three functions (e.g. `0x6D0E5`, `0x6D43E`, `0x6DAB8`) |
| role percent table `DAT_0012d950` | `0x12D950` | refs in `FUN_0005df1c`, `FUN_0005df98`, `FUN_0005d6e4` (matches `findings/U1b-gate-consumer.md`) |
| score array (known writers/readers) | `0xE0C5C` | 77 refs including `FUN_00097b90`, `FUN_000941dc`, `FUN_000983dc` |

So an empty result at a correct address would have been meaningful; the earlier empty results were at wrong addresses.

### 1.2 Table consumers (the D15 / open-Q4 deliverable)

| Table | File | Ghidra VA | Bytes seen at VA (proves the table) | Consumers (getReferencesTo) |
|---|---|---|---|---|
| `UFO_growth_rates` (10 caps + 16 rows x 10 u16 = 340 B) | `0x155010` | `0xF396C` | `0f00 0f00 0600 0600 ...` = caps 15,15,6,6,6,6,6,6,4,2 then `0900 0900 0000...` = week-1 row | **only `FUN_0006edac`** (VA `0x6EDE8` row base `+0x14`, `0x6EDF0` caps `+0`) |
| `UFO_mission_patterns` (20 rows x 10 u16 = 400 B) | `0x155164` | `0xF3AC0` | `0300 0300 ...` (week 1 = all Infiltration) | **only `FUN_0006cfb4`** (VA `0x6CFFC`, base `+0`). The 29 other refs lie at `+0x190` and beyond: adjacent globals (`0xF3C50...`, `DAT_000f3c78`, `DAT_000f3c82`) in `FUN_0006d384`, `FUN_0006da88`, `FUN_00070938`, `FUN_000702e4`, `FUN_000b32ac`. They are not table reads |
| `building_detection_weight` (49 x i32) | `0x155354` | `0xF3CB0` | `64 00 00 00 9b 00 00 00 ...` = 100, 155 | **only `FUN_0006f964`** (VA `0x6FA60`) |
| `alien_detection_weight` | `0x142374` | `0xE0CD0` | `01 00 00 00 01 00 00 00 03 ...` | **only `FUN_0006f964`** (VA `0x6FA14`) |
| `alien_movement_percent` | `0x1423B0` | `0xE0D0C` | `28 00 00 00 28 00 00 00 21 ...` = 40,40,33 | **only `FUN_0006f7f8`** (VA `0x6F82E`) |
| `cequip_score_req_data` (as the extractor reads it) | `0x1421C4` | `0xE0B20` | `d0 07 00 00 d6 06 ...` = 2000, 1750 | **zero refs**; the live table starts 80 bytes earlier (F9) |

### 1.3 Conversions verified

For each cited function the 8 bytes at `VA + 0x626A4` were checked against the Watcom prologue / listing. Examples (first 8 file bytes): `FUN_0006edac` file `0xD1450` `53 51 52 56 57 80 3d 2a` (push ebx..edi, `cmp byte [..],0` = the flag test in F4); `FUN_0006cfb4` `0xCF658` `53515256 575583ec`; `FUN_0006d384` `0xCFA28`; `FUN_0004ab04` `0xAD1A8` `53 52 e8 ...`; `FUN_0006da88` `0xD012C` `51 56 57 55 83 ec 10` (matches the listing's `PUSH ECX...`); `FUN_00097b90` `0xFA234`; `FUN_000b32ac` `0x115950`; `FUN_0006f964` `0xD2008`; `FUN_0006f7f8` `0xD1E9C`. Leaf helpers (`FUN_000b3e24`, `FUN_000b3e84`, `FUN_00091de4`, `FUN_00091db0`) start with `52`/`53 51` and agree with their listings. The literal operand `ADD EDX, 0x13C680` at VA `0x6CF71` is plain code bytes `81 c2 80 c6 13 00` at file `0xCF613` (not a relocated address: relocated operands read VA-0xD0000).

---

## 2. Findings

### F1. Weekly mission-type pick: match

OBSERVED. `FUN_0006cfb4` (UFO2P / non-4 / file `0xCF658`, VA `0x6CFB4`), listing:

```
0006cfbf  MOV AX,[0x000e0e7c]          ; week counter (1-based)
0006cfc7  DEC EAX ; clamp to [0,0x13]  ; row = clamp(week-1, 0, 19)   -> rows 0..18 = weeks 1..19, row 19 = DEFAULT
0006cfea  MOV EAX,0x9 ; CALL 0x5d1d8   ; rand16 inclusive -> 0..9
0006cffc  MOV AX,word ptr [EBX+EAX*2+0xf3ac0]   ; file 0xCF6A0: type = patterns[row*20 + 2*r]
0006d004  CMP [0x000e0cc0],0 ; ...      ; override: if DAT_000e0cc0 != 0 and mode 0, type := 2 (Subversion). DAT_000e0cc0 is only ever cleared (index §5.5), so dead
```

Type to package range (OBSERVED, VA `0x6D017-0x6D06A`): type 1 (Attack) selects records `[20,30)`, type 2 (Subversion) `[30,40)`, type 5 (Overspawn) `[40,45)`, anything else (3 Infiltration, 0, 4, >=6) `[0,20)`.

I decoded the whole 20x10 table from file `0x155164` and compared it with `ufo_mission_preference.xml` in GAMESTATE: rows 0-18 equal the numbered weeks 1-19 element for element, and row 19 equals `UFO_MISSION_PREFERENCE_DEFAULT` (`Infiltration, Attack x2, Subversion x4, Overspawn x3`; the DEFAULT entry has no `<week>` element, so my first regex skipped it and I re-ran the comparison with a week-optional regex: 20 of 20 rows equal).

Engine. LIVE `gamestate.cpp:938-944` (HEAD `937-943`): key `UFO_MISSION_PREFERENCE_<week>`, falling back to `_DEFAULT`; `pickRandom(rng, missionList)` over the 10-entry list. The fallback is equivalent: weeks 1-19 hit their own row, week >= 20 hits DEFAULT; the EXE clamps to row 19 which is DEFAULT. Uniform over 10 slots is the same distribution.

Difficulty. Not an input to these four functions: a grep of their full listings (`validate/v_ufo_consumers_work/call2.clean`: `FUN_0006cfb4`, `FUN_0006edac`, `FUN_0006d384`, `FUN_0006da88`) finds no operand at `0xE0C9A` or `0xE0C9C` (the difficulty word); the only `0xE0C9A` dword loads (the `MOV reg,[0xE0C9A]; SAR 16` idiom) in the functions I listed are in `FUN_0006f964` and `FUN_00097b90`. The other readers of dword `0xE0C9A` in the score-array window (`CTRL_SCORE` refs `+0x3e`; a direct `word [0xE0C9C]` load would lie outside that window) are `FUN_0006fcc0`, `FUN_00092060`, `FUN_00096bc8`, `FUN_000ad38c`, `FUN_000ad93c`, `FUN_000aff9c`, `FUN_000b426c`, `FUN_000b94c0`, `FUN_00015400`, `FUN_00099130`, `FUN_000638dc`. So the schedule functions do not read difficulty; Superhuman differs only through gear gates (F9) and detection (F11) among the things I examined.

Mix per week (rows verified): weeks 1-3 all Infiltration; week 4 9/1 Att; 5-6 8 Inf/1 Att/1 Sub; 7 +1 Ovr; 8 5/1/2/2; 9-11 4/2/2/2; 12 3/2/3/2; 13 3/3/2/2; 14 4/2/2/2; 15-18 3/2/2/3; 19 2/3/2/3; DEFAULT 1/2/4/3. This equals the table in `exe_evidence_index.md §5.6`.

Strategy impact: low (no divergence). Fix: none.

### F2. Incursion package selection: match

OBSERVED (same function, VA `0x6D0C6-0x6D164`, file `0xCF76A-0xCF808`):

```
for idx in [start, limit):                       ; priority order = index order
    rec = UFO_mission_data + idx*0x2a            ; 0xDC758
    ok = 0
    for slot 0..2:
        c = rec.craft[slot]                      ; i16, -1 = empty
        if c == -1 or pool[c] >= rec.count[slot] : ok++
    if ok == 3 : chosen = idx ; break            ; "CMP SI,3"
if chosen:
    for slot 0..2: if c != -1 : pool[c] -= rec.count[slot]      ; file 0xCF800 (VA 0x6D15C)
    ...start the spawn state machine (DAT_000d505c = 1), reset slot counters
return chosen != 0
```

There is no retry with another mission type or another week. Caller `FUN_0006d384` (VA `0x6D3C8`) resets the timer regardless of the result (F6).

The pool `pool[]` is `DAT_00143d40[10]` (u16 per UFO type 0-9). It is the `city 1` row of the per-city UFO counter array `0x143A6C + city*0x2D4 + type*2` (F3).

Engine. LIVE `gamestate.cpp:946-990` (HEAD `945-989`): map of `priority -> incursion` filtered by `primaryMission == missionType`; `limits[type] = count of ORG_ALIEN vehicles in CITYMAP_ALIEN` minus the package's primary + attack + escort lists; first package with no negative entry wins; none means `return` (timer already reset at line 910). Equivalent, given that the engine's three lists are the EXE's three slots split by role.

Data check (my script): all 45 EXE records (`UFO_mission_data`, file `0x13DDFC`, 42 B each; layout `craft[3] count[3] role[3] follow[3] zone_mode[3] mission_counter[3] scatter[3] type_percent[3]`) versus `ufo_incursions.xml`: mission type by index range (I1-I20 = 0-19, A1-A10 = 20-29, S1-S10 = 30-39, O1-O5 = 40-44), priority equal to index order, craft/count multiset, and the (role, type_percent) multiset all match: 0 mismatches.

Unmodelled side detail (F13): for Attack packages, `FUN_0006cfb4` also picks a target building per primary craft at selection time.

Strategy impact: none beyond what the plan already uses. Fix: none.

### F3. Fleet pool, caps, weekly growth: match

OBSERVED. `FUN_0006edac` (file `0xD1450`, VA `0x6EDAC`), full listing:

```
cmp byte [0xde62a],0 ; jnz skip                      ; F4: growth disabled flag
cx = clamp(week-1, 0, 15)                            ; word [0xe0e7c] - 1
for t in 0..9:
    new = pool[t] + growth[cx*10 + t]                ; table 0xf3980 = 0xF396C + 0x14
    pool[t] = min(new, caps[t])                      ; caps at 0xF396C   (jle: if new > cap, store cap)
```

Callers: the Monday clock (`FUN_0004ac40` VA `0x4ADF7`, file `0xAD49B`, right after `week++` at `0x4ADD9` and the funding call `FUN_000941dc`), `FUN_0004ac24` (file `0xAD2C8`, called from `FUN_000114cc`, which INFERRED advances the clock in 5-minute steps) and new-game `FUN_000b94c0` (VA `0xB95F5`, after `DAT_000e0e7c = 1`). Pool is zero at start (`FUN_0006cdd0` writes 10 zeros at VA `0x6CEB8`), so the new-game call gives Probe 9, Scout 9.

Pool bookkeeping (OBSERVED):
- launch: `pool[c] -= count` at selection (F2);
- UFO leaves the human city through a gate: `FUN_0003a910` (file `0x9D0A6-0x9D0D9`) increments the destination city's counter for the craft type, and charges score category 5 `-(vehicle_data.score)/4` when the departure is from city 0 (VA `0x3AA6E`, file `0x9D112`);
- vehicle removal (destroyed or otherwise deleted): `FUN_00058280` (file `0xBA924`) decrements the counter of the current city only for owner 1, type < 10 (VA `0x582CD-0x582F2`);
- growth, as above.
So a returned (withdrawn) UFO rejoins the pool; a destroyed UFO is gone until growth replaces it; the cap bounds only what growth can add. The engine's model (physical alien vehicles in CITYMAP_ALIEN; `allowance = cap - present`, never negative) is the same bookkeeping; the one nuance is that the EXE assigns `pool = cap` when pool+row exceeds cap, which can lower a pool that already exceeds the cap; the engine never removes ships.

Data check: caps and all 16 rows equal `ufo_growth_lists.xml` (`UFO_GROWTH_1..15`, `_DEFAULT`, `_LIMIT`): 0 differing rows. Row index: engine uses `week == row.week` else DEFAULT; EXE clamps `week-1` to 15; identical for week 16+.

Engine. LIVE `gamestate.cpp:1650-1680` (HEAD `1617-1650`) and `ufogrowth.cpp:34-48`; called from `updateEndOfWeek` (`gamestate.cpp:~1767`) and, with `gameStart=true`, from `newGame` (LIVE `:714`), reproducing the new-game call.

Strategy numbers (Superhuman, no difficulty dependence): caps Probe 15, Scout 15, Transporter 6, Fast Attack 6, Destroyer 6, Assault 6, Bomber 6, Escort 6, Battleship 4, Mothership 2. UFO type scores (`vehicle_data +0x44`, file `0x189C8C + t*126 + 0x44`): Probe 50, Scout 100, Transporter 150, Fast Attack 200, Destroyer 250, Assault 300, Bomber 300, Escort 250, Battleship 500, Mothership 700.

### F4. What stops UFO growth: diverges

OBSERVED.
- Reader: `FUN_0006edac` tests byte `DAT_000de62a` first (VA `0x6EDB1`, file `0xD1455`: `CMP byte ptr [0x000de62a],0x0 ; JNZ skip`).
- Writer: `FUN_000b4538` (file `0x116BDC`) is a 9-case switch that sets one of nine "alien building destroyed" bytes: case 0 `de666`, 1 `de652`, 2 `de65c`, 3 `de67a`, 4 `de634`, 5 `de63e`, 6 `de670`, **7 `de62a`**, 8 `de620`. Its only caller is `FUN_000b32ac` (VA `0xB33BE`, file `0x115A62`), the battle-result handler that also awards the alien-building score (VA `0xB3340`, category 7 from the `0x191B7C` table indexed by building function).
- So destroying the alien building whose index is **7** permanently disables UFO growth. No code clears the byte (the 4 individual refs are the reader in `FUN_0006edac`, the readers `FUN_000b45b8`/`FUN_0004a33c`, and the single writer; save/load copies whole blocks by start address and was not traced).

Which building is index 7. OBSERVED/INFERRED, three anchors:
1. Name string table at file `0x14AD23`: `Incubator Chamber, Spawning Chamber, Food Chamber, Megapod Chamber, Sleeping Chamber, Organic Factory, Alien Farm, Control Chamber, Maintenance Factory, Dimension Gate Generator`: index 7 = Control Chamber, index 5 = Organic Factory (I read the 10 strings from the binary).
2. The same function indexes the destruction-score table `[400,400,300,500,300,250,250,350,300,500]` (file `0x191B7C`, read from the binary) with the same index space; engine `building_functions.xml` gives CONTROL_CHAMBER `destroyedScore 350` (unique) and ORGANIC_FACTORY 250.
3. Independent behavioural corroboration (community, not EXE): UFOpaedia says destroying the Control Chamber ends UFO growth and triggers an "Apocalypse" bombing wave (`research/web_alien_campaign.md` D1/D2). F4 + F5 reproduce both behaviours at index 7.
4. Independent test of the index space of `[0x244048]` itself. The same `FUN_000b32ac` block has a different branch at index 2 (F14): it clears the Mutant Alliance's takeover and allies it to X-COM. The in-game briefing ties the Mutant Alliance outcome to rescuing Sectoids in the **Food Chamber** (`findings/C2-secondary-objectives.md:100-102`, `web_alien_campaign.md` §7.2 row 1). In the name-order space (string table of anchor 1) the Food Chamber is index 2 and the Control Chamber index 7. In the engine's raid-order space (`RESEARCH_ALIEN_BUILDING_n`: Sleeping 0, Food 1, Alien Farm 2, Maintenance 3, Incubator 4, Control Chamber 5, Spawning 6, Organic Factory 7, Megapod 8, Gate 9) index 2 would be the Alien Farm and index 7 the Organic Factory. Two independent descriptions (Food Chamber at 2, Control Chamber at 7) both fit name order; raid order fits neither except the engine's own Organic Factory guess. Definition of the variable (OBSERVED, `call8a`): `FUN_000aff9c` (file `0x112640`) stores `[0x244048] = word [building + 0xAA]` at VA `0xB009C-0xB00AB` (file `0x112740-0x11274F`), i.e. the building record's function word, and `FUN_000b32ac` indexes the destruction-score table with the same word of the same record (`MOV EAX,[EDX+EAX*2+0xA8]; SAR EAX,16; MOV EDX,[EAX*4+0x1304D8]`, VA `0xB3331-0xB3334`). So the index in the compares at `0xB3345`/`0xB33A6` is the score-table index space. Two further observations: the score array starts with the 10 alien values at index 0, so alien buildings carry function words 0-9 in that space; and `FUN_000b4538` has cases 0-8 and no case 9, which fits 0-based indices with the Dimension Gate Generator (9, the game-ending building) last. The name order of those ten indices comes from the extractor (`extract_buildings.cpp:76`, `alien_building_names->get(function_idx)`) and the behavioural anchors 2-3 above.
Identity is INFERRED from 1-4 (medium-high; the index 7 and the index space are OBSERVED, the mapping of index to building name rests on the extractor's name order plus the two behavioural anchors); the flag index 7 is OBSERVED. UFOpaedia numbers the Control Chamber "6th" because it numbers by raid order; the EXE index here is name order.

Engine. LIVE `gamestate.cpp:1626` calls `UFOGrowth::craftFactoryIntact` (`ufogrowth.cpp:50-65`) which returns false only when the alien-city `BUILDINGFUNCTION_ORGANIC_FACTORY` is dead. Nothing watches the Control Chamber. Comment at `exe_evidence_index.md §5.6` ("EXE evidence is UFOpaedia prose") is now superseded: the EXE evidence points at a different building.

Strategy impact: high. A plan that raids the Organic Factory (6th/index 5) to stop reinforcement is wrong against the original; the cut-off belongs to index 7, two raids later. In the engine the cut-off happens two raids earlier than in the original. Conversely, the engine never mass-launches (F5).

Fix sketch: replace the gate with "Control Chamber (BUILDINGFUNCTION_CONTROL_CHAMBER) has been destroyed" (latch it as a GameState bool when the building dies, so it survives save/load; the EXE byte is permanent). Keep a data-driven mapping index->function so the Organic Factory flag (case 5, `DAT_000de63e`) can be added if a reader is ever found. `blocked_on_re`: no.

### F5. Control Chamber destruction launches a mass attack: engine_missing

OBSERVED.
1. `FUN_000b32ac` (file `0x115A4A-0x115A62`, VA `0xB33A6-0xB33BE`): `CMP [0x244048],7 ; JNZ ; MOV [0xf3c78],1 ; ... CALL 0xb4538`. `[0x244048]` is the destroyed alien-building index (same value passed to `FUN_000b4538`, F4). Index 2 (Food Chamber) takes a different branch of the same block: see F14.
2. `DAT_000f3c78` readers: `FUN_0006cfb4` (VA `0x6D074-0x6D0BC`, file `0xCF718`): in mode 0, if set, it calls `FUN_00070938` instead of picking a package, and returns with the spawn machine armed (`DAT_000f3c7c = 1; DAT_000d505c = 1`).
3. `FUN_00070938` (file `0xD2FDC`) decompile: clears `DAT_001a44a4[]`, then builds up to **20** ships round-robin over UFO types **1..9** (`DAT_001a44a4[i] < pool[i]` adds one; Probes, type 0, are never included; total stops at 20 or when no type has a spare); **if fewer than 10 were gathered it clears `DAT_000f3c78` (VA `0x709F0`, file `0xD3094`) and nothing launches.**
4. Spawn: `FUN_0006d384` special branch (VA `0x6D44D-0x6D5A0`) spawns one ship per tick when `FUN_0006d994` says the gate volume is clear, hard-coding role 9 (VA `0x6D497`: `MOV EBX,9`). In `FUN_0006da88` the pending flag skips the `type_percent` override (VA `0x6DBAF`), keeps the role-9 value (10% of ceiling, below `crash_constitution` for every hull, so such ships never withdraw), and sets `+0x15C = 4` (mission 4 = `FUN_00070a2c`: random building that is not allied to the aliens, owner not taken over), `+0x16E = 0x32`, mission counter `+0x171 = 0x14`, no follow target. A Mothership (type 9) with role 9 has a `rand16(100) > 50` chance of being promoted to role 10 (VA `0x6DB76-0x6DB8D`, file `0xD021A-0xD0231`; the U1 docs cite the same instruction in object-page numbering `0x5DB75`); the engine's `clampIncursionScatter` misreads this instruction as a scatter clamp.
5. When the list is exhausted `DAT_000d505c = 0` and `DAT_000f3c78 = 0`: one shot.

INFERRED (building identity as in F4): it matches the UFOpaedia "Apocalypse mission" (every remaining UFO except Probes bombs random buildings), with the corrections that the cap is 20 ships, at least 10 are required, and it fires on the next timer expiry (or the next 5-minute check, F6), not on a fixed schedule. I did not trace whether the pool is decremented in this branch (`FUN_0006d384` increments the human-city counter but I saw no write to `0x143D40` there).

Engine: nothing (no reader of the Control Chamber; `docs` list it as a closed negative on string/table evidence; the trigger is a flag).

Strategy impact: high for the final third of a campaign: destroying the Control Chamber gives a one-time wave of up to 20 ships, but it also ends growth (F4). Thinning the pool beforehand (below 10 gatherable ships) cancels the wave entirely.

Fix sketch: in the building-destroyed path, if function == CONTROL_CHAMBER set `GameState::apocalypsePending`; in `invasion()` when pending, take up to 20 vehicles from the alien-city pool round-robin over types Scout..Mothership, require >= 10, launch each with a role-9 style attack mission on random non-allied intact buildings, withdrawPercent 0, clear the flag. `blocked_on_re`: partly (mission 4 target filter `FUN_00070acc`; the Mothership role-10 promotion effects; whether the pool is decremented).

### F6. Invasion timer cadence: diverges (INFERRED, medium confidence)

OBSERVED values.
- Reset after each incursion (`FUN_0006d384`, VA `0x6D3D4-0x6D414`; `FUN_0004ab04`, VA `0x4AB55-0x4AB9C`): `T = 0x2F7600 + rand16(0..0xB04)*0x870 + rand16(0..0xE10)*0x24` ticks, i.e. 24 h + up to 2820 min + up to 3600 s, 36 ticks per game second. Engine (`gametime.h`, `gamestate.cpp:910`) uses exactly these constants.
- **Initial value** at new game, `FUN_0006cdd0` (file `0xCF474`, VA `0x6CF54-0x6CF7D`): `rand16(0..0x1C20)`, then `ADD EDX,0x13C680` with `EDX = rand*36`: `T0 = 1,296,000 + rand*36` ticks = **10 h + 0..2 h**. Engine `newGame` (`gamestate.cpp:718-722`) uses the 24-72 h formula for the first timer.
- Drain 1: `FUN_0006d384` (called once per main-loop iteration from `FUN_00010908`, VA `0x10F6D`, only when the speed value `DAT_000d4d58` is in `1..599`) subtracts `DAT_000d4d58` when `DAT_000d5060 == 0` (human city); fires when it reaches 0. The calendar `FUN_0004ac40` adds the same `DAT_000d4d58` ticks to the clock, so drain 1 is 1x game time.
- Drain 2: `FUN_0004ab04` (file `0xAD1A8`) runs every 5th game minute (`FUN_0004aa7c` per-minute hook, `minute % 5 == 0`, VA `0x4AAC5`, called from the clock at VA `0x4AE82` and from the time-skip `FUN_000114cc` at `0x11BEA`): `if D < 0x2A30 (10800 ticks = 5 min) { pick incursion; reset D } else D -= 0x2A30`, also gated on `DAT_000d5060 == 0` (gate file `0xAD1AF`, compare `0xAD1EA`, subtract `0xAD248`). That is another 1x of game time.

INFERRED: at speeds 1-6 both drains run, so the counter empties at about 2x game time: nominal 24-72 h becomes about 12-36 h of game time, and the first incursion about 5-6 h after the 12:00 start. In the time-skip mode (speed >= 600, `FUN_000114cc`) only drain 2 runs. Not run against the original, so treated as medium confidence. Cross-check on the community side (`research/web_alien_campaign.md` lines 158-160): AOW reports about 2-2.7 incursions per week, below the 3.5/week the nominal mean (48 h) implies; that neither supports nor refutes a 7/week timer because the launch rate is also limited by fleet fit (F2/F3: early weeks the pool holds only Probes and Scouts) and by incursions an interceptor never meets. This contradicts `exe_evidence_index.md §7`'s "not divergences: invasion timer 24-72 h", which looked at drain 1 only.

Strategy impact: high if true: incursion pressure up to 2x the engine's, and a first UFO in the first evening rather than after 1-3 days. The engine is the conservative case.

Fix sketch: model as two decrements (per-tick, plus 5-minute) or equivalently draw `T/2`; start the first timer at `10 h + rand(0..7200 s)`. Gate on a measurement (count incursions per game week in a headless run vs. the original) before changing the benchmark. `blocked_on_re`: no for the code; the measurement needs original gameplay.

### F7. Withdrawal threshold value: engine_bug

OBSERVED (the part U1b got wrong). `FUN_0006da88` (spawn, file `0xD012C`), listing VA `0x6DB98-0x6DBE6`:

```
0006db98  MOV byte ptr [ESI+0x166],AL        ; role
0006dba0  CALL 0x0005df1c                    ; +0x168 := pct_role[role] * ceiling[type] / 100       (file 0xD0244; the value the engine uses)
0006dba5  CMP word ptr [ESI+0x15c],4 ; JZ skip       ; +0x15C = mission type (role 9 -> 4)
0006dbaf  CMP dword ptr [0xf3c78],0 ; JNZ skip       ; not in the F5 mass-launch mode
0006dbb8  EAX = slot ; EDX = word [ESI+0x12E] (constitution)
0006dbc6  EAX = dword [EDI + slot*2 + 0x22] >> 16     ; = u16 at record+0x24+2*slot  = type_percent[slot]
0006dbd0  IMUL EDX,EAX ; /100 ; MOV [ESI+0x168],AX    ; file 0xD0274..0xD0283  OVERWRITE
```
So for every ordinary incursion UFO, `+0x168 = constitution_at_spawn x type_percent[slot] / 100`; the 6-role table `{30,30,20,10,25,10}` only survives for role 9 / mass-launch ships. `type_percent` is the 3x u16 at record offset `0x24` (the struct in `tools/extractors/common/ufoincursion.h` has it at `0x24`; the engine comment says `0x28`), values 10/20/30/40/50 (45 records, read from the binary).

No later rewrite for alien ships (all callers of `FUN_0005df1c` checked in this session): `FUN_00010380`/`FUN_0006de64`/`FUN_0005ca04` create civilian role-6 vehicles; `FUN_00059e60`, `FUN_0005a120`, `FUN_0005a918`, `FUN_0005b22c` rewrite only under `owner != 0 && owner != 1`; `FUN_0003a910` retarget branch is unreachable for owner-1 craft (U1 docs). The other `+0x168` writers `FUN_0004dd14` / `FUN_0004e0d4` run only for owner 0 (X-COM craft; X-COM's retreat setting reuses the field). `FUN_00057c8c` does not touch it.

Effect on the band: the withdrawal band is `[crash_constitution, type_percent x ceiling / 100)`. Per UFO type, non-escort slots, from the binary (ceiling / crash / distinct EXE thresholds / distinct engine thresholds):

| Type | ceiling / crash | EXE thresholds (type_percent) | engine thresholds (role %) | slots with non-empty band, EXE vs engine |
|---|---|---|---|---|
| Probe | 80 / 15 | 24, 32 | 16 | 2/2 vs 2/2 |
| Scout | 120 / 35 | 12, 24, 36, 48 | 24, 36 | 5/7 vs 3/7 |
| Transporter | 400 / 150 | 80, 120, 160 | 120 | 1/5 vs 0/5 |
| Fast Attack | 500 / 150 | 150, 200 | 100, 150 | 3/5 vs 0/5 |
| Destroyer | 600 / 200 | 120, 180, 240 | 120, 180 | 2/11 vs 0/11 |
| Assault | 850 / 200 | 170, 255 | 170, 255 | 8/11 vs 10/11 |
| Bomber | 700 / 100 | 140, 210, 280, 350 | 140, 210 | 8/8 vs 8/8 |
| Battleship | 1800 / 300 | 180, 360, 540, 720 | 360, 540 | 8/9 vs 9/9 |
| Mothership | 2800 / 350 | 560, 840 | 560, 700, 840 | 10/10 vs 10/10 |

By mission type (non-escort slots): Infiltration 17/30 (EXE) vs 21/30 (engine) non-empty; Attack 10/15 vs 10/15; Subversion 14/17 vs 5/17; Overspawn 6/6 vs 6/6; thresholds differ in 12/30, 9/15, 17/17, 6/6 slots. The largest gap is Subversion: the engine's 20% role value makes most Subversion ships unable to withdraw while the EXE (type_percent 30-50%) lets them.

Escorts (role 11): EXE band is non-empty for them too (type_percent 10-40%) but a UFO with a follow target (`+0x16C != -1`) takes the follow branch of `FUN_00059148` instead of the gate (U1b), so they do not withdraw by damage; the engine reaches the same outcome for the wrong reason (10% of ceiling below the crash floor). An escort whose primary is gone re-arms the flag and heads for the gate (`FUN_00059148` Block 2, listing in `validate/v_ufo_consumers_work/call3.clean`); not modelled.

Engine. LIVE `gamestate.cpp:1022, 1090, 1143` (HEAD `1021, 1089, 1142`): `withdrawPercent = UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE[slot.role & 0xF]`, `invader->withdrawHealthPercent = withdrawPercent`; `Vehicle::withdrawBandEntered` LIVE `vehicle.cpp:2165` (HEAD `2140`), `maxHealth*percent/100` with an inclusive lower bound. `VehicleMission::incursionTypeThreshold(constitution, typePercent)` (LIVE `vehiclemission.cpp:3564`, HEAD `3416`) is exactly the EXE formula and is referenced only by `tests/test_city_rules.cpp:1898-1902` (never called by game code). `clampIncursionScatter` (LIVE `:3554`) misreads VA `0x6DB76-0x6DB8D`: the real instruction pair is `rand16(100) > 50 and type==9 and role==9 -> role=10`; it is a no-op because the max `typePercent` is 50.

OBSERVED engine state: in the 16 benchmark `victory.save` files of `build-funding/learning-show-old2/runs/` the alien vehicles' `withdrawHealthPercent` takes only the values 10 and 30 (for example 22 Fast Attack ships at 10 and 13 Transporters / 11 Assault Ships at 30), i.e. the role-table values 10 (Escort) and 30 (Attack/Infiltration), never the `type_percent` values 10-50 per slot.

Strategy impact: medium. Withdrawal is a minor attrition path (the band is a narrow window just above the crash threshold, and small hulls often have no window), but the engine over-withdraws Infiltration Assault/Battleship ships and under-withdraws Subversion, Transporter, Fast Attack and Destroyer ships.

Fix sketch: set `withdrawPercent = slot.typePercent` for the three launch loops (gamestate.cpp:1022/1090/1143), keep the role-% table only for the F5 mass-launch ships (role 9 and promoted role 10), and compute `threshold = incursionTypeThreshold(type->health, typePercent)` so the dead helper becomes live. Add a test per hull for (EXE band non-empty vs. role band). `blocked_on_re`: no.

### F8. Withdrawal mechanism: engine implements it (task premise stale); EXE specification

Premise check. `git log -S withdrawHealthPercent` shows commit `34adbf58 feat(city): damaged UFOs break off and leave through a dimension gate`; HEAD `vehicle.cpp:2167-2182` and LIVE `vehicle.cpp:2192-2207` implement the check in `Vehicle::update`. The "not implemented" statements (index D11, `next-implementation.md` item 9) are out of date.

EXE specification (OBSERVED unless marked):
1. Cadence. `FUN_0005760c` (file `0xB9CB0`) runs once per main-loop iteration; for every valid vehicle of the active city (`+0x174 == DAT_000d5060`) that is undocked (`+0x108` high word -1) it calls `FUN_000588f8` (call site VA `0x57896`, file `0xB9F3A`) when `slot % 18 == ticksOfDay % 18`, with extra phases for speeds >= 2/4/6 so none is skipped. At speed 1 each UFO is evaluated every 18 ticks = 0.5 game seconds. U1b described this as a calendar-staggered periodic sweep; the real period is half a game second, so a per-update check in the engine is not slower than the EXE.
2. Gate `FUN_000588f8` (file `0xBAF9C`): `if +0x168 <= constitution(+0x12E) skip; if constitution < floor[type] (crash_constitution, vehicle_data +0x30) skip;` otherwise `+0x16A (arrived flag) = 1` unconditionally, then calls `FUN_00059148` in the same invocation (VA `0x58AC5`). Band = `[crash_constitution, +0x168)`, lower bound inclusive. Engine `withdrawBandEntered` agrees on both bounds.
3. Consumer `FUN_00059148` (file `0xBB7EC`, VA `0x59171`): for owner 1 (Alien) with the flag set: if `+0x271 != 0` the flag is cleared (request dropped until the next sweep); if `+0x271 == 0` and `DAT_000d5060 == 0` it heads to the nearest gate chosen by `FUN_0005d360`, sets the point target to that gate's x/y/z (`0x1439E4/0x1439F8/0x143A0C` per city) and order mode 2. If `+0x16C != -1` (escort following a companion) it takes the follow branch instead. `DAT_000d5060` is the active-city index (0 human, 1 alien dimension; `FUN_000b94c0` sets 0 at new game; spawn stores it in `+0x174`), not an organisation index as U1b hypothesised. So the same-tick call does take the human-city path for UFOs.
4. Not modelled by the engine: the `+0x271` drop (semantics unknown: cooldown/busy counter written in 10+ functions); the re-arm of an orphaned escort (INFERRED from the listing).
5. X-COM craft use the same field and function: for owner 0 `+0x168` is the retreat threshold from the craft's attitude, and the message block at VA `0x58972+` builds a notice (per U1b; strings not read).

Strategy: a UFO inside its band turns for the nearest gate within about half a game second; it can still be killed on the way (health `<= crash_constitution`), so the band converts surviving hits into escapes. An escape returns the ship to the pool and costs X-COM `score/4` incursion points (F3); a kill earns the hull's `score` and removes the ship until growth replaces it.

### F9. Alien craft equipment score gates: match, with a table-location correction and a timing difference

OBSERVED. The unlock routine is `FUN_00097b90` (file `0xFA234`), called every game midnight in the human city (`FUN_0004ac40` VA `0x4AE14`, file `0xAD4B8`, outside the Monday-only block) and from `FUN_0004ac0c`. Its listing (VA `0x97BF8-0x97C3F`, file `0xFA29C-0xFA2E3`):

```
ESI = week[cat0] + prev[cat0]                      ; Tactical Missions, DAT_000e0c5c + DAT_000e0c60
EDI = week[cat3] + prev[cat3]                      ; UFOs Shot Down,  DAT_000e0c74 + DAT_000e0c78
for i in 0..8:                                     ; 9 latch flags at 0xE0B84 + i  (file 0x142228..0x142230)
    if flag[i] == 0:
        need = table[i*5 + difficulty]  @ 0xE0AD0 + i*0x14 + diff*4          ; diff = word 0xE0C9C
        if (i < 4 ? ESI : EDI) >= need : flag[i] = 1
```
The 9-row table (file `0x142174`, 180 bytes, `u32[9][5]`, columns Novice..Superhuman):

| Row | Novice..Superhuman | Gate quantity | Consumer |
|---|---|---|---|
| 0 | 2000 1750 1500 1250 1000 | Tactical Missions (cat 0) | `FUN_000b3e24` id 42, flag `0xE0B84` |
| 1 | 4000 3500 3000 2500 2000 | cat 0 | id 50, flag `0xE0B85` |
| 2 | 8000 7000 6000 5000 4000 | cat 0 | id 52, flag `0xE0B86` |
| 3 | 16000 14000 12000 10000 8000 | cat 0 | id 51, flag `0xE0B87` |
| 4 | 2000 1750 1500 1250 1000 | UFOs Shot Down (cat 3) | `FUN_000b3e84` vequip 44 Small Disruption Shield |
| 5 | 4000 3500 3000 2500 2000 | cat 3 | 45 Large Disruption Shield |
| 6 | 8000 7000 6000 5000 4000 | cat 3 | 46 Cloaking Field |
| 7 | 16000 14000 12000 10000 8000 | cat 3 | 47 Teleporter |
| 8 | 32000 28000 24000 20000 16000 | cat 3 | 48 (not in any UFO loadout) |

The extractor reads file `0x1421C4` (`CEQUIP_SCORE_REQ_DATA_OFFSET_START 1319364`), which is rows 4-8 (100 B); the bytes are identical to the live rows 4-8, so the engine's numbers are right. The 80 bytes before it (rows 0-3; hexa labels them "same as below (not used/for megaprime vehicles?)") are used. The `0x1421C4` copy itself has zero code references (Section 1.2) because the code addresses the 9-row table from its start.

Consumers (OBSERVED). `FUN_0005d6e4` (vehicle init, file `0xBFD88`): for a vehicle whose owner word `+0x12C == 1` it copies the 45 equipment-id slots of its `vehicle_data` row (`record + 0x51`, the last 45 bytes of the 126-byte row), calling `FUN_000b3e84(id)` for each and blanking any id for which it returns 0 (VA `0x5DDD1-0x5DE16`, file `0xC0475-0xC04BA`); for other owners it block-copies unchanged. So gear is decided at vehicle init, i.e. when the UFO is launched (`FUN_0006da88` calls it). `FUN_000b3e84` jump table (file `0x116528`): ids 44-48 map to flags 4-8; id 48 maps to the last. `FUN_000b32ac` also runs the same filter over a vehicle type's slots (VA `0xB35B9-0xB35E7`); INFERRED: the crash-site loot list.

Default loadouts (read from `vehicle_data`, file `0x189C8C`): Probe {6}; Scout {6,44}; Transporter {6,44}; Fast Attack {7,44}; Destroyer {14,44,47}; Assault {7,44,47,46}; Bomber {6,16,45,47,46}; Escort {15,45,46,47}; Battleship {8,14,45,47}; Mothership {8,16,15,45,47}. No UFO type carries id 48, so the engine's omission of row 8 changes nothing.

Score quantity. Category 3 accumulates `+vehicle_data.score` of a destroyed owner-1 vehicle only when the projectile's firer is an owner-0 vehicle, once per vehicle (index §5.2 / `0xFAA90`); the quantity checked is week + previous weeks of that category, i.e. lifetime. Superhuman thresholds: Small shield 1000, Large shield 2000, Cloak 4000, Teleporter 8000. In kills: 1000 = 20 Probes, or 10 Scouts, or 2 Battleships.

Engine. LIVE `vehicle.cpp:3928-3947` (HEAD `3883-3902`): `equipDefaultEquipment` is called at launch (`gamestate.cpp:1034`) and skips an alien item when `totalScore.craftShotDownUFO < etype->scoreRequirementFor(difficulty)` (`vequipmenttype.cpp:36-43`, `scoreRequirementByDifficulty` from the extractor); score added at `vehicle.cpp:2120`. Same quantity, same comparison direction (`>=` unlocks), same column, same time of use.

Differences. (a) Timing: EXE latches once per game midnight, engine evaluates live at each launch: the engine arms UFOs up to 24 h earlier than the EXE after a threshold is crossed. (b) The EXE never relocks. (c) Rows 0-3 unmodelled (F10).

Strategy impact: medium. Escalation is a pure function of lifetime UFO-kill score; there is no other input, and it reacts within one day. Cumulative kill score at which gear arrives is the same as the engine's. Fix sketch: optional latch (add `bool ufoGearUnlocked[5]` updated at the daily boundary) for fidelity. `blocked_on_re`: no.

### F10. Tactical-score gates (rows 0-3): engine_missing / partly unbound

OBSERVED. `FUN_000b3e24` (file `0x1164C8`) jump table (11 entries at VA `0xB3DF8`, read from the binary) maps ids 42 -> flag 0, 50 -> flag 1, 52 -> flag 2, 51 -> flag 3 (ids 43-49 return "available"). Its two call sites are in `FUN_000ad148` (file `0x10F7EC`; VA `0xAD1B5`, `0xAD31B`), called only when its first argument is 1. `FUN_000ad148` walks a 92-byte record (`DAT_000f5248 + idx*0x5c`: several groups of three ints and a five-entry tail) and appends entries through `FUN_000ad5ac` with a hard cap of 500 (`DAT_002440c4 < 500`), skipping ids whose flag is unset. Callers: `FUN_000adffc`, `FUN_000ae218`, `FUN_000aebd8`.

INFERRED: this is the generator of the ground-troop equipment/unit sets, and the four gated ids are the strongest items, available only after the Tactical Missions score (week + previous, category 0) passes 1000/2000/4000/8000 at Superhuman. I could not identify ids 42/50/51/52 or the sets table owner within budget; the engine has `EquipmentSet::min_score/max_score` ranges (`aequipmenttype.h:261`) and an org `tech_level` that rises weekly (`gamestate.cpp:~1747-1754`), a different mechanism. Strategy relevance: category 0 (Tactical Missions) is also one of the eight weekly funding score categories; winning many ground missions would arm the enemy's best gear sooner (INFERRED from the table consumer, not traced into a battle).

`blocked_on_re`: yes (ids, side parameter, set table). Next lead: dump `DAT_000f5248` (file `0x1568EC`) with `bytes:` and the callers of `FUN_000ad148`.

### F11. Building detection (`FUN_0006f964`): diverges

OBSERVED. `FUN_0006f964` (file `0xD2008`, VA `0x6F964`) is called from `FUN_0004ab04` every 5th game minute and loops 5 buildings (`AH = 5`), advancing a cursor (`DAT_000f3d74`, stride `0xE2`) over the city's building list, stopping early after one detection. For each building:

```
skip if building.cooldown (+0xBC word) != 0
for s in 0..13 with count[s] (= byte +0xD4+s) != 0:
    r = rand16(100) ; n++ ; if r < 100/n : chosen = s        ; reservoir pick used only for the message
    sum += count[s] * alien_detection_weight[s]               ; VA 0x6FA14 (table 0xE0CD0)
if week == 1 and weekday == 1 and hour < 16 : sum *= 100       ; VA 0x6FA26-0x6FA43 (game start grace window: 12:00-16:00 on day 1)
v = sum * (building_detection_weight[function] - 2*difficulty) / 100     ; VA 0x6FA60 (table 0xF3CB0), diff word 0xE0C9C
if relation[owner][Alien] > 74   (FUN_00091de4)  : v /= 2
else if relation[owner][XCOM] < -49 (FUN_00091db0): v /= 2   ; mutually exclusive
if owner.takenOver (org+0x11) : v /= 2
if chosen != -1 and rand16(100) < v - 10 :
    DETECT: building flag, score category 2 (Alien Incidents) += -30   (VA 0x6FB47-0x6FB51, file 0xD21EB-0xD21F5),
            building.cooldown = 0x168 (360), message
```
(Relation helpers decompiled: `FUN_00091de4(a,b) = relation[a][b] > 74`, `FUN_00091db0(a,b) = relation[a][b] < -49`, matrix `0x16EC28` stride 28.)

Engine. `Building::detect` LIVE `building.cpp:771-840` (HEAD `750-819`), cadence `building.cpp:155-157`, `building.h:17-19`. Same: weights, `(buildingWeight - 2*difficulty)/100`, `-10`, taken-over halving, 6 h cooldown (360 min if the EXE counter is per game minute; decrement site not traced). Divergences:
1. Spotted gate. EXE: reservoir over the species present, so `chosen == -1` only with probability about 1% (first species roll `r < 100` over 0..100). Engine: `for each species: randBoundsExclusive(0,100) < 100/(count+1)` sets `alien_spotted` (LIVE `:787`, HEAD `:766`). One species with 20 aliens passes only 4% of checks; 6 Anthropods + 6 Poppers + 2 Psimorphs about 50%. The engine therefore detects much less often than the EXE for large crews. This is the dominant difference.
2. Halving: EXE `allied` OR (else) `hostile-to-X-COM`, then `takenOver`: at most /4. Engine applies three independent halvings (allied-to-aliens, hostile-to-player, taken over): up to /8. A taken-over org (relation to X-COM -100, to Aliens +100) is /4 in the EXE and /8 in the engine. Threshold edge: EXE hostile is `<= -50` (byte `< -49`), engine `< -50` float.
3. Cadence. EXE: 5 buildings every 5 game minutes over the active city's building list (`DAT_00183a68`), so a full sweep takes `N` minutes. Engine: each building every 110 min at Superhuman (`{70,80,85,95,110}` by difficulty, `building.h:17-19`). OBSERVED in the benchmark saves (`build-funding/learning-show-old2/runs/g000-r000[0-5]/victory.save`, `gamestate/buildings.xml`, difficulty 4): the human city has **109** buildings (identical in all six runs; the alien city has 10). If `DAT_00183a68` is 109 the EXE sweep is about 109 min, i.e. the engine's 110 min matches at Superhuman (not verified for the lower-difficulty values; I did not read `DAT_00183a68` at load, `FUN_00014b3c`). No divergence at Superhuman.
4. Grace window: EXE multiplies the detection sum by 100 from 12:00 to 16:00 on the first day of week 1 (the initial infiltrated building is found almost surely within the first hours); engine: `firstDetection` forces one detection (`gamestate.cpp:~717`). Equivalent for one initial building.
5. One detection per call (EXE) vs independent per building (engine); small.

Each detection costs X-COM -30 score in category 2 (Alien Incidents), one of the eight weekly funding score categories. INFERRED: the engine's weaker detection yields fewer -30 hits per week than the original at the same infiltration level. Strategy impact: medium (score drift -30 per incident; earlier discovery of infiltrated buildings). Fix sketch: replace the per-species gate with `chosen != -1` (reservoir, ~99%), make the halving `allied ? /2 : (hostile ? /2 : 1)` then `takenOver ? /2`, cadence needs no change at Superhuman. `blocked_on_re`: no.

### F12. Alien movement (`FUN_0006f7f8`): diverges (minor)

OBSERVED. `FUN_0006f7f8` (file `0xD1E9C`), called per building each game hour from `FUN_0004ac40`'s hour block via `FUN_0006f7d4` (file `0xD1E78`):

```
r = rand16(30)                                    ; once per building (VA 0x6F81B-0x6F822)
for s in 0..13: moved[s] = count[s] * (alien_movement_percent[s] + r) / 100      ; table 0xE0D0C
M = sum(moved)
if M < 3 : return                                  ; CMP ESI,3 ; JL   (file 0xD1F03)
dest = FUN_0006f5e4(building)  ; random among the first up to 15 other buildings (list order) with centre within +-15 tiles and FUN_00070acc == 0
p = 15 + 3*M ; if owner.takenOver or relation[owner][Alien] > 74 : p += 20
if rand16(100) < p :
    if X-COM base slot at dest and rand16(100) < 5*M : base.exposed = 1
    move moved[s] of each species to dest
```
Engine LIVE `building.cpp:970-1030` (HEAD `959-1019`): destination rule 15 closest intact within 15 tiles (EXE takes the first 15 found in list order, not the closest), same `15 + 3*total + 20`, same `< 5*moved` base exposure. Differences: engine returns only for `total == 0` (EXE needs >= 3, so small crews of 1-2 movers never move in the EXE); engine draws a separate 0..30 offset per species, EXE one per building; engine's +20 applies to Friendly or Allied owners (relation >= 25), EXE only to taken-over or relation > 74. Strategy impact: low-medium (the engine spreads small crews faster, which raises infiltration through more buildings). Fix: `if (total < 3) return;`, one shared `rand(0,30)`, bonus test `takenOver || relation > 74`. `blocked_on_re`: no.

### F13. Attack-incursion target choice: exe_unbound (partial decode)

OBSERVED (listing `FUN_0006cfb4` VA `0x6D1BA-0x6D2FF`): when the chosen package contains a role-5 slot and the active city is 0, for each of ten target slots it scans the city's buildings and keeps the best score `200 - distance(building, gate_k) - relation[Alien][owner] + rand16(100)`, excluding buildings for which `FUN_00091de4(Alien, owner)` is true (owner allied with aliens), whose owner is taken over, or for which `FUN_00070acc` (not decoded) is nonzero; the result goes to `DAT_000d5072[slot]`. `relation[Alien][X-COM] = -100`, so X-COM-owned buildings gain +100 relative to neutral ones. INFERRED: Attack packages favour X-COM bases and nearby hostile buildings.

Engine: `VehicleMission::attackBuilding(state, v, nullptr, counter)` target choice not compared. Strategy relevance: base defence timing from week 4. `blocked_on_re`: yes (`FUN_00070acc`, distance metric `FUN_0005d288`).

---

### F14. Food Chamber destruction reverses the Mutant Alliance takeover: engine_missing

OBSERVED. `FUN_000b32ac` (file `0x115950`), the battle-result handler. Guard chain at the top (VA `0xB32C8-0xB32FD`, file `0x11596C-0x1159A1`): the result record `[0x244024]` must be 1, or 4 while the active city is not 0; the active city must not be 0 (alien dimension); `[0x244084]` must be 1 (INFERRED: the raid was won). Then the destruction score is added (category 7, VA `0xB3340`) and, if `[0x244048] == 2` (VA `0xB3345`, file `0x1159E9`):

```
mov  byte [orgtable+0x2921], 0        ; 0x2921 = 24*0x1B6 + 0x11 : org 24 (Mutant Alliance) taken-over byte := 0      (file 0x1159FD)
if relation[24][0] < 100 : relation[24][0] := 100                                                                       (matrix byte 0x16EEC8)
relation[0][24] := relation[24][0]                                                                                      (byte 0x16EC40)
FUN_0005faf0(24, 1, -50)                                                                                                (relation delta call, args EAX=24 EDX=1 EBX=-0x32)
if relation[24][1] > -100 : relation[24][1] := -100                                                                      (byte 0x16EEC9 := 0x9C)
[0x1409C8] := 0                                                                                                         (sole reference; meaning unknown)
```
So destroying the Food Chamber (name-order index 2, F4 anchor 4) frees the Mutant Alliance from alien takeover, makes the Alliance and X-COM mutually allied (100), and makes the Alliance hostile to the aliens (-100). No check on the number of captives rescued is in the guard chain I read. INFERRED: the briefing's "rescue Sectoids and gain a Mutant alliance" is flavour for this building-level outcome; `findings/C2-secondary-objectives.md` §2 and `exe_evidence_index.md §5.3` list "Sectoid rescue -> Mutant Alliance" as NOT BOUND because they looked for a rescue counter.

Engine: nothing. `Organisation::takeOver` sets `takenOver` (`organisation.cpp:84-108`) and nothing clears it (grep of `game/` for `takenOver = false`: none); no building-destroy hook touches `ORG_MUTANT_ALLIANCE`.

Strategy relevance: this is the only EXE-bound reversal of a takeover found, and it is hard-wired to org 24; the Government cannot be un-taken-over by it. For the takeover/relations keys: the EXE does have a takeover reset write (`takenOver := 0`), the engine has none. Mutant Alliance gates hybrid recruiting (web notes), so an allied, untaken-over Alliance matters to hiring.

Fix sketch: in the alien-building-destroyed path, when the destroyed building is `BUILDINGFUNCTION_FOOD_CHAMBER`: `mutantAlliance->takenOver = false; setRelation(MA<->XCOM, 100); setRelation(MA->ALIEN, -100)`. `blocked_on_re`: partly (the `[0x244024]`/`[0x244084]` meaning, `[0x1409C8]`, and the exact semantics of the `FUN_0005faf0(24,1,-50)` call before the clamp).

## 3. Escalation answers (strategy relevance)

- Escalation driver: the only EXE input from X-COM actions into the UFO schedule is the lifetime UFO-kill score (gear flags, daily latch). Mission mix and growth are pure functions of the week counter; neither reads difficulty or score.
- How fast: at Superhuman small shields appear after 1000 kill-score, large shields after 2000, cloaks after 4000, teleporters after 8000. Score per kill equals the hull's `score` (50-700). A kill only counts if X-COM craft deliver it. Because the check runs at midnight and the equipment is chosen at launch, the first affected UFOs are those launched after the next midnight.
- Shooting UFOs also removes them from the pool permanently until weekly growth replaces them (caps and weekly additions in F3). Large hulls regrow slowly (Mothership cap 2; additions in weeks 8, 11, 14 and then one per week after week 15; Battleship cap 4). The selector falls back to smaller packages and, if none fits, skips the launch (the timer still resets).
- Control Chamber: shooting the standing fleet below 10 gatherable ships (types 1-9) before destroying building index 7 cancels the mass launch (F5); destroying it ends growth for good (F4).

## 4. Re-leads and open items

1. `FUN_000ad148` squad generator: identify ids 42/50/51/52 and the set table at VA `0xF5248` (F10). Callers `FUN_000adffc`, `FUN_000ae218`, `FUN_000aebd8`.
2. Food Chamber (index 2) destruction clears the Mutant Alliance takeover and allies it to X-COM (F14): follow-ups for the takeover and relations keys (guard bytes `[0x244024]`, `[0x244084]`, flag `[0x1409C8]`, and whether any other writer of an org's taken-over byte at `orgtable + org*0x1B6 + 0x11` exists).
3. `FUN_00060f4c` / `FUN_00061214` (the two readers of the category 0-6 sum, open Q10): decompiled; they are screen-drawing functions (rect `0x280 x 0x1E0`, string-group lookups, a button loop ending in `DAT_00183a80 = 1`). The 0-6 sum is computed into a register and not used by gameplay logic in the decompile. Treat Q10 as closed: UI only (INFERRED from decompile text, listing at `validate/v_ufo_consumers_work/call2.clean`).
4. `+0x271` semantics (blocks the withdrawal request, F8) and the exact decrement of `building+0xBC` (cooldown) remain open.
5. `DAT_00183a68` read at city load, to confirm N = 109 for the detection sweep (F11) and the lower-difficulty values the engine hard-codes.
6. Whether the pool is decremented in the mass-launch branch (F5).
7. Dedicated measurement for F6: incursions per game week in the engine (and any original footage) against 3.5/week (single drain) and 7/week (double drain).
8. Not done: second binary (TACP) and the 4-build; no Ghidra query was made on `UFO2P4.EXE`.

## 5. Files

Scripts (private copy, deleted after this run; sources preserved in the work dir): `RangeXrefs.java`, `Multi.java`, `ListNear.java`. Raw outputs are in `validate/v_ufo_consumers_work/`: `call1.txt` (xref ranges and controls), `call2.clean` (growth, selection, timer, spawn, detection, movement, score screens), `call3.clean` (rollover and gear, flags, withdrawal), `call4.clean`, `call5.clean`, `call5b.clean`, `call6a.full`, `call6b.clean`, `call7a.full`/`call7b.full` (battle-result handler head, `0x244048` refs), `call8a.full` (`0x244048` writer), `cov.py` (reference/raw-scan coverage check). Cross-check scripts (Python, read-only on the EXE, GAMESTATE XML and the benchmark `victory.save` zips) were run inline; their results are quoted above.
