# v_score: score categories, every score writer, and everything that consumes score

Key `v_score`. Binaries: UFO2P.EXE and TACP.EXE, ISO non-4 generation (CRC 0x4749ffc1 / 0xfebbe39e). Engine: `LIVE:` = frozen snapshot of the develop-reboot worktree, `HEAD:` = committed fda36d6c where it differs.

Citation convention for the EXE: binary + physical file offset (the Ghidra VA I used, and the conversion: UFO2P code file = VA + 0x626A4, UFO2P data file = VA + 0x616A4, TACP code file = VA + 0x5AAA4). Operands inside code bytes are object-relative (data raw = Ghidra VA - 0xD0000 in UFO2P, - 0xE0000 in TACP). Every function offset cited below was prologue-checked against the canonical EXE (Watcom push sequence), see section 8.

OBSERVED = bytes / decompile / listing / saved-game data. INFERRED = my reading of what a field means. They are kept apart throughout.

---------------------------------------------------------------------------------------------------

## 0. Bottom line

1. The score census is now closed. UFO2P writes the 8-category array (file 0x142300, VA 0xE0C5C, stride 8 = week, previous) from exactly 9 `addScore` call sites plus the inline vehicle-destroyed scorer; every one of the 60 raw operand hits on the array was classified (section 3). Nothing else writes it.
2. Every UFO2P category writer is reproduced by the engine in LIVE (value and condition), with these exceptions: (a) Category 0 (tactical) is computed in TACP and the engine's formula differs in several places (section 5); (b) the "alien incident" -30 matches but the **trigger probability is wrong** (engine uses `100/(aliens+1)` where the EXE uses `100/(rank of the species among species present)`); (c) category 4 is charged by the EXE for any destruction, the engine only when an attacker is known. HEAD lacks category 7 entirely; LIVE has it and the value table is byte-identical.
3. Open Q10 is answered: the category 0-6 sums at file 0xC3642 / 0xC3907 are the **defeat and victory screens** printing a number (`FUN_00060f4c` / `FUN_00061214`, sum goes through `itoa` `FUN_000bf539`). They are display only. There is no hidden rule there.
4. The real score-dependent game rules in UFO2P are exactly four: (i) weekly funding (all 8 categories, W and H); (ii) **nine latched unlock flags set at the Monday rollover** (new finding: a 9x5 threshold table at file 0x142174; rows 4-7 compare lifetime *UFO-kill* score and gate vehicle modules 44-47 on newly built alien craft and on UFO recovery loot - live and modelled by the engine apart from the weekly latch; rows 0-3 compare lifetime *tactical* score and gate agent items 42/50/52/51 but I found no live path through which they change anything; row 8 gates module 48, which no UFO record carries); (iii) the **alien-infantry equipment level** = count of thresholds <= lifetime tactical score (engine models this and the thresholds are identical), minus a level penalty tied to the alien dimension's structure table that the engine does not model; (iv) the battle-launch hand-off of the lifetime tactical score to TACP.
5. The previously "unnamed row 4" of `cequip_score_req_data` (32000/28000/24000/20000/16000) is **Dimension Shifter (vequip 48)**, a module that none of the ten UFO vehicle records loads (OBSERVED, `loaded_equipment_slots` dump), so that row is inert. The table at 0x1421C4 is not a standalone table: it is rows 4-8 of the 9-row table at 0x142174. My first scan for references to 0x1421C4 returned zero hits (a false negative caused by the sub-slice); the reader exists and uses the base 0x142174.
6. For strategy: score grows from research (21,010 points over 99 topics, best rate from BioChem autopsies), UFO shoot-downs, alien-building raids (250-500 each) and clean tactical wins; it bleeds from UFOs leaving through gates (-score/4 each, the biggest measured sink), building hits by X-COM *and Alien* fire (-6 typical, -20 for big tiles), alien incidents (-30), and craft losses (-score).

---------------------------------------------------------------------------------------------------

## 1. Method and controls

Tools: capstone disassembly of the canonical EXEs (read-only), raw displacement scans of the code region, and Ghidra headless on private copies (`v_score` and `v_score_2`, `-readOnly -noanalysis`). Nine Ghidra batches were used. Scripts I added (in my copy dir only): `DecompNew.java` (decompile + listing, creating the function when Ghidra has none), `DecompRange.java`. Raw outputs: `validate/work_score/g{A..I}.out`, helper scripts `scan.py`, `calls.py`, `xdis.py`, `xdis_t.py`.

Controls (a claim of absence or of a constant is only made where the same instrument finds a known positive):
- Operand scan control: raw operand 0x604D8 (building score table) is found at file 0x1159DA, exactly the known cat-7 reader; vehicle_data raw 0x585E8.. finds the known score-field readers. So the scan sees table reads.
- Call-site census control: raw `E8` scan of the code region reproduces the nine direct `addScore` calls (file 0x9D112, 0x9ECD2, 0xA423D, 0xB8AFC, 0xD21F5, 0x10D5B0, 0x10D950, 0x115967, 0x1159E4). Ghidra's caller list shows only eight (0x9ECD2 is inside an undefined region that Ghidra never turned into a function; `DecompNew` created it).
- Itoa control for the display-only claim: `FUN_000bf539` decompiles to a signed base-10 digit emitter and is called from the score screen and both end screens.
- The unlock-flag reader was found only after widening the search from the exact slice (0x1421C4) to the table base (0x142174); the earlier "no reference" result is retracted.

Limits: static analysis only, no runtime session. UFO2P4/TACP4 (Pentium builds) were not examined. The engine-side numbers from the saved games (section 7) are engine observations, not EXE facts.

---------------------------------------------------------------------------------------------------

## 2. Storage and updater

OBSERVED. 8 records x 2 int32 at UFO2P file 0x142300 (VA 0xE0C5C; raw 0x10C5C): `[cat*8+0]` this week, `[cat*8+4]` previous weeks. Updater `FUN_000983dc` at file 0xFAA80: `and eax,0xffff; add [eax*8+0x10c5c],edx; ret` (it only ever touches this-week). Save/load moves the 0x40 bytes at 0x106444 / 0x10725D. Labels (string group 0x25): 0 Tactical Missions, 1 Research Completed, 2 Alien Incidents in City, 3 UFOs Shot Down, 4 X-COM Craft Shot Down, 5 UFO Incursions, 6 Damage to City, 7 Alien Buildings Destroyed.

Weekly rollover `FUN_00097b90`, file 0xFA234: for k in 0..7 `prev[k] += week[k]; week[k] = 0`; **then, in the same routine (file 0xFA27E-0xFA2E5) the 9 unlock flags are evaluated** (section 6).

---------------------------------------------------------------------------------------------------

## 3. Table of every score writer: EXE value vs engine value

| Cat | EXE writer (UFO2P file) | EXE delta (OBSERVED) | Engine LIVE (HEAD if different) | Value match | Condition match | Note |
|---|---|---|---|---|---|---|
| 0 Tactical | 0x115967 in `FUN_000b32ac` (0x115950) | `addScore(0, dword [0x24408C])`, always, first thing in the battle-result handler. The dword is TACP's debrief Total (section 5) | `battle.cpp:3211-3212` `+= current_battle->score.getTotal()` (HEAD same) | **no, formula differs** | yes (every battle) | engine total lacks the leadership clamp, civilian-kill penalty, and uses item scores for live aliens |
| 1 Research | 0x10D5B0 (lab group 0) and 0x10D950 (lab group 1) in `FUN_000aac88` (0x10D32C) | `edx = u16 research_data[idx].score` (record +0x12), once per topic (completed byte set just before) | `cityview.cpp:4924-4925` `+= ev->topic->score` (HEAD 4849-4850) | **yes**: 99/99 (hours,score) pairs identical, total 21,010; 25 engine-only topics have score 0 | yes | the engine adds in the UI event handler, not in the simulation |
| 2 Alien incident | 0xD21F5 in `FUN_0006f964` (0xD2008) | `mov edx,0xffffffe2` = **-30**, once per new incident (building enters alert state 2, timer 0x168) | `cityview.h:13` `ALIEN_INCIDENT_SCORE=-30`, `cityview.cpp:4847-4848` (HEAD 4772-4773) | yes | **trigger differs** (section 4.3) | HEAD also turns a detection in an X-COM base building into `DefendTheBase` with no -30 (`HEAD:building.cpp:~815`); LIVE does not |
| 3 UFO shot down | inline in `FUN_000983ec` (0xFAA90): store to [0x10C74] at 0xFAAE1 | `+ u16 vehicle_data[type].score` (+0x44) iff dead vehicle owner==1 **and** firer vehicle owner==0; skipped when `edx==0` | `vehicle.cpp:2120-2121` (HEAD 2095-2096) | yes: 33/33 vehicle (score, health) pairs identical | yes | once per vehicle in EXE (flag +0x272); engine relies on `crashed` / `attacker` guards |
| 4 X-COM craft lost | same function: store to [0x10C7C] at 0xFAB15 | `- score` for any dead vehicle owner==0, no attacker needed, once | `vehicle.cpp:2126-2127` (HEAD 2101-2102) | yes | **partly**: engine only if `attacker` known | EXE callers: projectile hit `FUN_00054a28`, tile-crush `FUN_00080528`, `FUN_0007fdb0` (all via `FUN_00057c8c`) |
| 5 Incursion / "UFO escape" | 0x9D112 in `FUN_0003a910` (gate arrival) and 0x9ECD2 in the port-out routine at 0x9EC40 | `edx = -(u16 score >> 2)` i.e. -floor(score/4); only when vehicle owner==1 **and** current city index [0xD5060]==0 (human city) | `vehiclemission.cpp:1401-1403` (HEAD 1398) `-v.type->score / 4` when owner aliens and city HUMAN | yes (C++ truncation equals floor for positive) | yes (gate crossing out of the human city) | every alien ship leaving the human city pays, not only fleeing ones |
| 6 City damage | 0xB8AFC in `FUN_00054a28` (0xB70CC); 0xA423D in `FUN_00041644` (0xA3CE8) | `edx = byte [tile+0x0C]` (tile "value") pre-hit; charged per projectile hit when the human city, building footprint, firer org 0 **or 1**; obstruction path hard-codes org 1 | `scenery.cpp:1206-1208` (HEAD 1198), `vehicle.cpp:1028` (HEAD 1003) | yes: both read the same tile `value` byte (struct offset 12 of `citymap_tile_entry`) | yes | tile values in the data: 514 tiles value 6, 94 tiles value 20, 10 others between 1 and 18 |
| 7 Alien buildings | 0x1159E4 in `FUN_000b32ac` | `+ dword [function_idx*4 + raw 0x604D8]` = file 0x191B7C: 400,400,300,500,300,250,250,350,300,500 (Incubator, Spawning, Food, Megapod, Sleeping, Organic Factory, Alien Farm, Control Chamber, Maintenance, Gate Generator); alien dimension only, result flag [0x174084]==1 | `battle.cpp:3585-3586`, `building_functions.xml destroyedScore`, `GameScore::alienBuildingsDestroyed` (**LIVE only**; HEAD has neither the member nor the field) | yes, all 10 identical | yes (RaidAliens won) | confirms D5: LIVE has the category, HEAD does not |

Census proof (OBSERVED): raw 4-byte displacement scan of the code region for the 16 member addresses 0x10C5C..0x10C98 gives 60 hits. Classified: updater (1); vehicle-destroyed scorer (4: 0xFAAD6, 0xFAAE3, 0xFAB0D, 0xFAB17); rollover loop (0xFA24C-0xFA266); unlock-flag latch reads of cat 0 and cat 3 (0xFA280-0xFA294, section 6); score screen reads (0xFA4DC-0xFA895 plus 0xFA8D4-0xFA8EC totals); funding reads (0xF6EB6, 0xF6EBD, loop `cmp ax,8`); end-screen reads (0xC3642/0xC3649/0xC3907/0xC390E, loop `cmp ax,7`); battle launch read of cat 0 (0x11296C/0x112962); `FUN_000ad38c` read of cat 0 (0x10FA6E/0x10FA75); save/load (0x106444, 0x10725D). Everything else in the 60 is a byte coincidence (`rel32 = 0x10C` jumps, `[reg+0x10c]` displacements). The Q13 "~42 unclassified hits" are the score screen, rollover and writer; none writes a category.

---------------------------------------------------------------------------------------------------

## 4. Detail per category

### 4.1 Category 1, research (open Q8 part 1)

OBSERVED decode, UFO2P file 0x10D32C (`FUN_000aac88`). Lab group is the research_data byte at record +0x18 (0 BioChem, 1 Quantum). Both branches do the same thing: progress accumulates in `[idx*10 + 0xE2B4]` from the lab record byte (+0x49 BioChem / +0x4A Quantum, stride 0xCE), a cheat word `[0x10CC4]` forces completion, the completed byte `[idx*10+0xE2B8]` is set to 1, and `addScore(1, research_data[idx].score)` is called with `mov dx,[ebp+0xd7ee]` (record +0x12 = `score`, u16, zero-extended). The function entry guard `cmp byte [eax+0xE2B8],0; jne` makes this once per topic.

The engine matches: values identical for all 99 EXE topics (multiset of (man-hours, score) equal), and the ten alien-building topics at records 88-97 carry the same (hours, score) at the same positions (380/420/320/460/300/440/340/400/360/480). The 25 extra engine topics (`RESEARCH_UNLOCK_*` etc.) score 0. Research score per 1000 man-hours (EXE table): Queenspawn Autopsy 30.0; Anthropod Autopsy 25.0; Hyperworm/Popper/Spitter Autopsy 25.0; Overspawn Autopsy 22.5; Psimorph Autopsy 20.8; Skeletoid Autopsy 20.0; average over all topics 6.8; worst are Quantum topics (Dimension Destabiliser 1.2, Personal Cloaking Field 1.5, Personal Teleporter 1.5, Disruptor Armor 1.8, Dimension Shifter 2.1).

### 4.2 Category 5, incursion / UFO escape (open Q8 part 2)

OBSERVED. `FUN_0003a910` (file 0x9CFB4) is the vehicle-arrives-at-gate handler. For a vehicle whose owner word (+0x12C) is 1: it increments the per-city, per-type ship counter `[city*0x2D4 + type*2 + raw 0x73A6C]` of the destination city, and if the current city index `[0xD5060]` is 0 it calls `addScore(5, -(score>>2))`, with `score = vehicle_data[type].score` (raw 0x5862C = vehicle_data +0x44). The same sequence exists in the routine at file 0x9EC40 (no direct caller found; reached indirectly). The division is `sar edx,0x1f; shl edx,2; sbb eax,edx; sar eax,2` on a zero-extended u16, i.e. floor(score/4).

Values (50/100/150/200/250/300/300/250/500/700 for Probe, Scout, Transporter, Fast Attack, Destroyer, Assault, Bomber, Escort, Battleship, Mothership): penalty 12, 25, 37, 50, 62, 75, 75, 62, 125, 175. The engine charges the identical amount at `vehiclemission.cpp:1401-1403`.

Engine coverage check: the only engine paths that move a vehicle between cities are `GotoPortal` (`vehiclemission.cpp:1415`, charged) and the three incursion spawns that bring invaders *into* the human city (`gamestate.cpp:1033/1101/1154`, no charge, matching the EXE which charges only the exit). The engine lacks the EXE's damaged-UFO withdrawal (index D11), so ships that the EXE would have driven off through a gate (and charged) simply keep fighting or die.

Wong's FAQ line "1/4 the value of any UFO that escapes through a dimension gate" is correct but narrower than the code: the EXE charges on **every** alien ship that passes a gate out of the human city, whether it finished its mission or was driven off. The engine does the same, so the bleed is real in both.

### 4.3 Category 2, alien incident (the "alert" penalty), and the trigger formula

OBSERVED value: -30 (`mov edx,0xffffffe2; mov eax,2; call 0xFAA80` at file 0xD21EB-0xD21F5), the only writer. Engine: `ALIEN_INCIDENT_SCORE = -30`, applied when the UI handles `AlienSpotted` (`cityview.cpp:4847`). Value match.

The trigger is where they differ. `FUN_0006f964` (file 0xD2008) is called from `FUN_0004ab04` once per 5 game minutes (the invasion-timer word `[0xD5068]` is decremented by 0x2A30 = 10800 per call, 288 calls per 24 h). Each call examines 5 buildings from a round-robin pointer over all buildings (`[0xF3D74]`, count `[0x183A68]`, stride 0xE2), skipping any with a non-zero alert timer (+0xBC). For one building (OBSERVED, listing 0x6F9D2-0x6FB18):

1. `edi = 0; ecx = 0; chosen = -1`. For each of the 14 species slots s (crew byte at building +0xD4+s) with count > 0: `edi += 1`; `r = rand16(100)` (inclusive 0..100, `FUN_0005d1d8`); if `r < 100/edi` (integer) then `chosen = s`. Then `ecx += count_s * alien_detection_weight[s]` (table at file 0x142374: egg 1, Brainsucker 1, Multiworm 3, Hyperworm 1, Chrysalis 1, Anthropod 3, Skeletoid 3, Spitter 3, Popper 2, Megaspawn 10, Psimorph 8, Queenspawn 20, Micronoid 1).
2. (Special case: if `[0xE0E7C]==1 && word[0xD4D64]==1 && word[0xD4D68] < 16` then `ecx *= 100`; the engine's `firstDetection` flag is a no-time-limit version of this.)
3. `ecx = ecx * (building_detection_weight[function] - 2*difficulty) / 100` (table at file 0x155354; Senate 155, Police 135, Corporate HQ 130 ...).
4. `FUN_00091de4(org,1)` (relation[org][Alien] >= the byte at VA 0x1277E5, which the decompiler folds to a compare against 'J', i.e. relation > 74, "allied with aliens") **or** `FUN_00091db0(org,0)` (relation[org][X-COM] <= the byte at VA 0x1277E4, folded to relation < -49, i.e. <= -50, "hostile to X-COM"): if either is true, `ecx /= 2` (once). Then if the organisation's taken-over byte (org record +0x11) is set, `ecx /= 2` again.
5. If `chosen != -1` and `rand16(100) < ecx - 10`: incident. Set building state byte 2, `addScore(2,-30)`, alert timer 0x168, push the ticker message.

Engine `Building::detect`, `building.cpp:771-840` (HEAD 750-): the species roll is `randBoundsExclusive(0,100) < 100/(count+1)` where `count` is the number of aliens of that species, and the halvings are three independent checks (allied-with-aliens, hostile-to-player, taken-over). Differences (all OBSERVED code vs OBSERVED code):
- Gate: EXE `100/k` with k = rank of the species among species present (first species ~99%, second ~49%); engine `100/(n+1)` with n the head count of that species (one Anthropod 50%, twelve 7.7%). For a typical large single-species crew the EXE gate is ~99% and the engine's is 5-10%.
- Halving: EXE halves once for (allied-with-aliens OR hostile-to-X-COM) and once more for taken-over; engine halves up to three times.
- Final roll: EXE inclusive 0..100 (101 values), engine 0..99. Negligible.
- Cadence: EXE 5 buildings per 5 minutes round robin over all buildings; the saved Superhuman game has 109 human-city buildings, so each building is visited every ~109 minutes. The engine's `TICKS_PER_DETECTION_ATTEMPT[4]` is 110 minutes at Superhuman. At Superhuman these agree; at Novice-Hard the engine (70/80/85/95 min) visits more often than the EXE, whose cadence does not depend on difficulty (difficulty only enters through `- 2*difficulty` in step 3).

Worked example (INFERRED arithmetic on the OBSERVED formulas), Superhuman: 12 Anthropods in a Corporate HQ, integer arithmetic: sum 12*3 = 36; x(130-8)/100 = 43; final roll P(r < 33) = 33/101 = 33%. EXE: gate ~99% x 33% = **32% per visit**. Engine: gate 100/13 = 7.7% x 33% = **2.5% per visit**, about 13x lower. For 3 Anthropods in the Senate: sum 9 x (155-8)/100 = 13; final roll P(r<3) = 3%; EXE ~3.0%, engine gate 25% x 3% = 0.75%. So the engine under-generates incidents (and therefore -30s and investigation prompts) by roughly 4-13x for realistic crews. Impact on the benchmark is two-edged: fewer -30s, but also fewer chances to find and fight crews.

### 4.4 Categories 3 and 4, shoot-down and craft loss

OBSERVED (`FUN_000983ec`, file 0xFAA90, listing in section 8). Entry guard `cmp byte [eax+0x272],0` (already scored). Cat 3: `[vehicle+0x12C]==1` (dead vehicle is Alien), `edx != 0`, firer vehicle `[edx+0x15A]` has `+0x12C == 0` (X-COM), add `u16 [type*126 + raw 0x5862C]`. Cat 4: `[vehicle+0x12C]==0`, subtract the same field. Flag `+0x272` set last. Callers: both inside `FUN_00057c8c` (file 0xBA330, the vehicle damage applier): VA 0x57F80 after the "destroyed" ticker message with `edx` = the projectile, and VA 0x58224 straight after `call 0x409e0` (start crash/fall) with `edx` still the projectile (Watcom functions here preserve all registers except eax). `FUN_00057c8c` is reached from the projectile hit `FUN_00054a28` (VA 0x55A44), from `FUN_00080528` twice (it scans for a vehicle inside a box at a tile and damages it: debris / tile crush) and from `FUN_0007fdb0`.

Engine `adjustRelationshipOnDowned`, `vehicle.cpp:2115-2132`, called from `die` (`!crashed`), `crash` and `startFalling`, always guarded by `if (attacker)`. For UFOs this equals the EXE (`crash` is the first point). Divergence (medium confidence): an X-COM craft destroyed with no attacker (tile crush, collapse debris) is charged by the EXE and not by the engine. Rare in practice.

### 4.5 Category 6, city damage (verification only)

I re-derived the writer at file 0xB8AFC: `edx = byte [eax+0x0C]` (eax is the pre-hit copy of the 52-byte tile record), `neg edx`, `mov eax,6`, `call 0xFAA80`, executed when the firer org word is 0 or 1 (`test cx,cx; je` / `cmp cx,1; jne` at 0xB8AE1-0xB8AEA). The vehicle-obstruction writer at file 0xA423D is the same shape with org hard-coded to 1. The engine reproduces both (`scenery.cpp:1183-1208`, `vehicle.cpp:1010-1031`). Struct offset 12 of `citymap_tile_entry` is `value`, extracted from the same game data file, so values match by construction.

### 4.6 Category 7, alien buildings destroyed

OBSERVED at file 0x1159A7-0x1159E4: guard `[0x174084]==1`, then `FUN_000b3d4c` (collapse and the victory test), then `addScore(7, table[ (dword at building+0xA8) >> 16 ])` with table raw 0x604D8 = file 0x191B7C. LIVE reproduces the ten values and the trigger. HEAD does not contain `alienBuildingsDestroyed` or `destroyedScore` anywhere. In all 164 Superhuman benchmark saves scanned (and so in the 100-save subset used in section 7) this category is never non-zero (no alien building was ever destroyed).

---------------------------------------------------------------------------------------------------

## 5. Category 0, tactical score (computed in TACP)

OBSERVED. UFO2P adds `[0x24408C]` (file 0x115961-0x115967) unconditionally at the start of `FUN_000b32ac`. TACP exports its Total with `_DAT_001ed334 = DAT_0027a0a4` in `FUN_00015494` (TACP file 0x6FF38). INFERRED: the two names are the same field of the hand-off block (not traced through the file write).

TACP score function `FUN_00022f90` (TACP file 0x7DA34), with the death handler `FUN_00060720` (file 0xBB1C4), decoded:

- Side matrix M[killerSide][victimSide] (int32, base VA 0x279F68, 8x8), casualty array C[side] (VA 0x27A068). Per death of a unit with value v (byte unit+0x44, set at spawn from `agent_type_data.score` low byte plus rand(high byte), high byte is 0 for all shipped types, so v = score):
  - killer hostile to victim (`FUN_00021998`: relation byte <= threshold byte at VA 0x2818B8; value INFERRED to be -50 by analogy with the UFO2P twin `FUN_00091db0`; argument order not resolved): `M[killer][victim] += v`;
  - otherwise (civilians, other non-hostile sides, own side): `M[killer][victim] -= v`;
  - always `C[victimSide] += v`.
- Combat rating (displayed, VA 0x27A088) = sum over k of M[X-COM][k]. Leadership base = sum of the **positive** entries of that row. If the mission is a win (state VA 0x1ED32C == 1: X-COM alive, no hostile alive), each incapacitated hostile adds its v to the leadership base, and to the combat rating unless its side is the Aliens.
- Casualty penalty (displayed) = -C[X-COM].
- Leadership bonus = 0 if `C*30 > 1000` (casualty points >= 34), else `base * (100 - 3*C) / 100` (truncating).
- Total (VA 0x27A0A4) = combat rating - C + leadership + equipment captured - equipment lost. "#Alien corpses" and "Live Aliens captured" are displayed but zero in this function.
- Equipment captured / lost come from a 750-record item table (stride 0x30 at VA 0xEA2BC); INFERRED semantics: items owned by another side that end the battle in X-COM hands count as captured; X-COM-owned items that end in enemy hands, or on the ground in a loss, count as lost. Low confidence on the exact conditions.
- The brainsucker conversion path (`FUN_0006cf64`, file 0xC7A08) gives the alien side 2v and takes v from the X-COM row and v from the diagonal: this is Wong's "double the points ... for getting an agent brainsucked", confirming the FAQ.

Engine (`battle.cpp:4071-4080`, `battleunit.cpp:5051-5114`): combat rating += v per hostile kill (same as EXE); casualty penalty -= v per X-COM death (same); friendly fire -10 flat (EXE: -v on the combat rating and +v casualty); leadership bonus `(100 + 3*cp)*(CR + FF + live)/100` **with no clamp**; live aliens add the species item score to both the base and the total; killing non-hostile non-own units (civilians) has no score effect.

Differences that matter:
1. Leadership clamp. At 4 or more dead agents (cp <= -34) the EXE sets the bonus to 0, the engine goes negative (factor 100-3*40 = -20%). With a 400-point base that is -80 vs 0.
2. Civilians and other non-hostile kills reduce the combat rating by v in the EXE and do nothing in the engine.
3. Live-alien credit: the EXE gives each incapacitated hostile v (aliens: leadership base only); the engine gives the species item score (requires containment) to both base and total.
4. Friendly fire: EXE -v on the combat rating but not in the leadership base; engine -10 inside the leadership base.
5. Equipment lost: engine charges destroyed X-COM items at destruction; the EXE (INFERRED) assesses the end state.
Kill credit per hostile (1 x v) and the casualty penalty (v per death) agree, which are the dominant terms, so the verdict is "diverges, moderate impact".

---------------------------------------------------------------------------------------------------

## 6. What consumes score (open Q10 and the "possible score-dependent rule")

| Consumer | Where (UFO2P file) | Reads | What it does |
|---|---|---|---|
| Weekly funding | `FUN_000941dc`, 0xF6880; sums at 0xF6EB6/0xF6EBD (loop `cmp ax,8`) | W = 8 this-week, H = 8 previous | tier delta on income, cutoff if H < -2400 or relation < -50 (`cmp ecx,0xFFFFF6A0` at 0xF6FBF) |
| Rollover + **unlock latch** | `FUN_00097b90`, 0xFA234; latch 0xFA27E-0xFA2E5 | cat 0 (week+prev) and cat 3 (week+prev), after week folded into prev | sets 9 flags `[raw 0x10B84+k]` (file 0x142228) from the 9x5 table at file 0x142174 |
| Flag consumers | `FUN_000b3e24` (0x1164C8): items 42, 50, 52, 51 <- flags 0, 1, 2, 3; `FUN_000b3e84` (0x116528): vehicle modules 44..48 <- flags 4..8 | flags | A: called from the built-in equipment-set loader `FUN_000ad148` (0x10F7EC) when the side is Alien; not reached for the two inventory species (6.1). B: called from vehicle creation `FUN_0005d6e4` (0xBFD88) for Alien-owned craft (locked module slots are cleared) and from the UFO-recovery loot builder in `FUN_000b32ac` (0x115C82) |
| Battle launch | `FUN_000aff9c`, 0x112640; read at 0x11296C | cat 0 week+prev | stored as `_DAT_002440A4`, the tactical lifetime score for the battle |
| Alien infantry equipment level | `FUN_000ad38c`, 0x10FA30 (four call sites at 0x110754, 0x1109E4, 0x110AEA, 0x110B9F, from the building-garrison builders `FUN_000adffc`, `FUN_000ae218`) | cat 0 week+prev | count of thresholds in a 5x7 table (file 0x19BEB4, identical to TACP file 0x302B00) <= score, minus a penalty, then a random weapon/grenade/equipment row of 10 |
| TACP twin | TACP `FUN_000250f8`, file 0x7FB9C | passed-in score `[0x10D34C]`, difficulty `[0x10D340]` | same threshold count, no penalty term in that function |
| Score screen | `FUN_00097c50`, 0xFA2F4 | all 8 | display |
| Defeat / victory screens | `FUN_00060f4c` (0xC35F0) / `FUN_00061214` (0xC38B8); loops at 0xC3642 / 0xC3907 | cats 0-6 week+prev (cat 7 excluded) | the sum is passed in EAX to `FUN_000bf539` (itoa) and printed. Display only. |
| Save/load | 0x106444 / 0x10725D | raw block | persistence |

### 6.1 The nine unlock flags (new)

OBSERVED. Table, file 0x142174, 9 rows x 5 difficulty columns of u32 (column = `word [0x10C9C]` = difficulty 0..4, Superhuman = 4):

| Row (flag k) | Novice, Beginner, Medium, Hard, Superhuman | Compared with | Gates |
|---|---|---|---|
| 0 | 2000 1750 1500 1250 1000 | cat 0 lifetime | agent item 42 Devastator Cannon (alien side built-in sets) |
| 1 | 4000 3500 3000 2500 2000 | cat 0 lifetime | agent item 50 Personal Disruptor Shield |
| 2 | 8000 7000 6000 5000 4000 | cat 0 lifetime | agent item 52 Personal Cloaking Field |
| 3 | 16000 14000 12000 10000 8000 | cat 0 lifetime | agent item 51 Personal Teleporter |
| 4 | 2000 1750 1500 1250 1000 | cat 3 lifetime | vehicle module 44 Small Disruption Shield |
| 5 | 4000 3500 3000 2500 2000 | cat 3 lifetime | 45 Large Disruption Shield |
| 6 | 8000 7000 6000 5000 4000 | cat 3 lifetime | 46 Cloaking Field |
| 7 | 16000 14000 12000 10000 8000 | cat 3 lifetime | 47 Teleporter |
| 8 | 32000 28000 24000 20000 16000 | cat 3 lifetime | 48 Dimension Shifter |

Flags are only ever set here (never cleared by this code), evaluated once per Monday rollover, saved in the game (9 bytes at raw 0x10B84, 9 bytes in the save routine at 0x1064C0). Item names are from the EXE name tables (agent_equipment_names file 1349488, vehicle_equipment_names file 1353641). Rows 4-7 are the table the engine calls `cequip_score_req_data` (file 0x1421C4 = row 4); row 8 is the "unnamed row 4" the docs refused to bind.

Engine: `vehicle.cpp:3940` (HEAD 3895) strips a module from an alien craft's default loadout when `totalScore.craftShotDownUFO < scoreRequirementFor(difficulty)`; the thresholds are identical to rows 4-7 at Superhuman (1000, 2000, 4000, 8000); the modules each UFO type carries (EXE `loaded_equipment_slots`, vehicle_data +0x51) equal the engine's `initial_equipment_list` (Scout/Transporter: Small shield; Fast Attack: Small; Destroyer: Small + Teleporter; Assault: Small + Teleporter + Cloak; Bomber: Large + Teleporter + Cloak; Escort: Large + Cloak + Teleporter; Battleship and Mothership: Large + Teleporter; Probe: none). Differences: (a) the EXE latches at the Monday rollover, the engine tests continuously (a mid-week crossing is visible to the engine immediately and to the EXE only after the next Monday; lifetime cat 3 is monotone so there is no un-latching difference); (b) rows 0-3 and 8 are not modelled. Row 8: **inert** (no UFO record loads module 48, OBSERVED for all ten UFO types; the engine UFO loadouts have none either). Rows 0-3: the only reader is the built-in-set loader `FUN_000ad148` for the Alien side. The five `equipment_sets[]` bytes (agent_type_data +83) of the species with `inventory`==0 name sets 0-7, which hold only natural weapons; the two species with `inventory`==1 (types 39 Anthropod `[8,9,10,11,12]`, 40 Skeletoid `[8,8,9,10,11]`) name the sets that do contain Devastator Cannon / Personal Disruptor Shield / Teleporter / Cloaking Field, but both garrison builders (`FUN_000adffc`, `FUN_000ae218`) route `inventory`==1 species to the score-based loader `FUN_000ad38c`, never to `FUN_000ad148`, and the TACP copy of agent_type_data is byte-identical. I therefore found no live path through which flags 0-3 alter anything in UFO2P (TACP's own use of sets 8-12 was not examined). Verdict for rows 0-3: no gameplay effect found, engine omission is harmless (medium-low confidence); (c) the EXE also filters the UFO-recovery loot by the same flags at recovery time.

### 6.2 Alien infantry equipment level

OBSERVED. TACP file 0x302B00 and UFO2P file 0x19BEB4 hold the same 5x7 table (difficulty row): Superhuman row `1000 2200 4000 6600 10000 18000 28000`. The saved Superhuman game's `EQUIPMENTSET_ALIEN_1..8` carry exactly those bands, so the engine matches the thresholds. Input score: the lifetime tactical score = cat 0 week + previous (UFO2P `_DAT_002440A4 = [0xE0C5C] + [0xE0C60]` at file 0x11296C; the engine uses `totalScore.tacticalMissions` at `agent.cpp:166`, lifetime, same). Which aliens: only agent types whose `inventory` byte (agent_type_data +0x44) is 1: the table shows types 39 and 40, Anthropod and Skeletoid; the engine selects on `type->inventory`, same.

UFO2P's version (`FUN_000ad38c`) additionally subtracts a penalty from the level:

```
level = (number of thresholds <= score)                      ; 0..7
N  = FUN_0006e608(city 1)        ; used slots in the alien city's 40-entry table (word at entry+2 != -1)
n5 = FUN_0006ea78(city 1, 5)     ; entries whose type word == 5
if (n5 < N/10) level = max(0, level - (N/10 - n5))
```

OBSERVED: the table is per-city, 40 entries x 14 bytes at city-struct +0x9E; the maintenance cluster `FUN_0006e644` / `FUN_0006e770` / `FUN_0006e7bc` / `FUN_0006e878` (file 0xD0CE8-0xD1150) grows it (entries of type 3 carry a 40-tick countdown and add a new entry whose type is chosen by `FUN_0006e644`). `FUN_0006e644` returns one of {7,1,8,0,5,6,2,9,4}, each chosen when that type's count falls below 1/8, 1/9, 1/10 ... of the total. INFERRED: the type word is the alien-building function index (0..9, the set {0,1,2,4,5,6,7,8,9} plus the builder type 3 = Megapod), so type 5 is Organic Factory and the rule says: the alien infantry gear level drops when Organic Factories are under one tenth of the alien city's structures. The "1/10" for type 5 in the grower and the "N/10" in the penalty agree, which supports the reading. Confidence low to medium; the initial population and removal-on-raid-victory writers were not found within the call budget (lead in section 9). The engine has no such term and the alien city has a fixed list of 10 buildings.

TACP's twin `FUN_000250f8` has no penalty term; which of the two actually equips a given raid garrison is not established (UFO2P calls `FUN_000ad38c` from the garrison builders; TACP's function takes the score from the hand-off block).

---------------------------------------------------------------------------------------------------

## 7. Strategy relevance: what grows score, what bleeds it

EXE-grounded rates (engine matches unless stated):

| Source | Per event | Condition | Notes |
|---|---|---|---|
| Research completion | +50..+500 (21,010 for all 99) | topic completes | autopsies are 17-30 points per 1000 man-hours, Quantum/engineering topics 1-3 |
| UFO shot down by an X-COM vehicle | +50 Probe .. +700 Mothership | dead vehicle owner Aliens, firer X-COM | also removes the -score/4 escape penalty, net swing 1.25 x score |
| Alien building destroyed (won raid) | +250..+500 (3,550 for all ten) | alien dimension | plus its research topic 300-480 |
| Hostile kill in a tactical battle | +v (Anthropod 10, Skeletoid 14, Spitter/Popper/Multiworm 8, Hyperworm 5, Megaspawn/Psimorph 30, Queenspawn 50) | hostile side | minus 10 per dead agent; leadership multiplier (100-3C)% with a zero floor at C>=34 |
| UFO leaves the human city | -floor(score/4): 12, 25, 37, 50, 62, 75, 75, 62, 125, 175 | every gate crossing out of the human city | biggest measured sink |
| Projectile hit on building scenery in the human city | -6 (514 tile types) or -20 (94 tile types), a few between | firer is X-COM **or Alien**, charged on the hit, destroyed or not | misses on the way to a UFO over a building cost points |
| Alien incident | -30 | building alert | rate scales with sum(count x species weight) x building weight; see 4.3 |
| X-COM craft lost | -score of the craft | any destruction | Valkyrie 150, Hawk 250, Annihilator 550, Retaliator 450 |
| Unlocks | none | | keeping lifetime tactical score low keeps alien gear at the lowest sets (Superhuman: set 2 at 1000, set 4 with Devastator Cannons at 4000) and keeping UFO-kill score low withholds shields at 1000/2000, cloaking 4000, teleporters 8000 |

OBSERVED in the engine's own saved Superhuman games. Engine behaviour, not an EXE fact. Sample: the 100 saves under `build-funding/` (all difficulty 4, file mtimes 10-07 06:40 to 19:53, i.e. all after the city-damage commit fda36d6c at 04:32 the same day, so they all use the per-hit charging); the other 64 saves (`build/`, `build-asan/`, `build-ms/`) are not used because which score code their binaries ran is not established. Tooling: `validate/work/saves_scan.json`. Mean lifetime category totals by game-day bucket:

| Day | n | Tactical | Research | Incidents | UFO kills | Craft lost | Incursions | City damage | Alien bldgs | Total |
|---|---|---|---|---|---|---|---|---|---|---|
| 5-9 | 23 | 37 | 358 | -72 | 54 | -38 | -156 | -332 | 0 | -149 |
| 10-14 | 22 | 56 | 470 | -153 | 102 | -77 | -295 | -697 | 0 | -593 |
| 15-19 | 16 | 0 | 498 | -278 | 166 | -78 | -836 | -1158 | 0 | -1687 |
| 20-24 | 14 | 98 | 760 | -752 | 189 | -100 | -1768 | -1415 | 0 | -2987 |
| 25-29 | 5 | 163 | 1222 | -846 | 230 | -86 | -2282 | -951 | 0 | -2550 |

Of the saves at day 20 or later, 78% (18 of 23) have a lifetime total below -2,400 (54% of the 39 at day 15 or later). Incursions are the largest negative, city damage the second, alien incidents the third; research is the only large positive. Neither unlock family can matter in these runs (tactical and UFO-kill scores stay far below the 1,000 first threshold), and no alien building was ever destroyed. Remember the engine's incident rate is 4-13x below the EXE's for typical crews (4.3), so the original's incident bleed would be larger than shown.

---------------------------------------------------------------------------------------------------

## 8. Offsets, prologue checks and listings

Prologue bytes read at the physical offset (all UFO2P unless TACP): `FUN_0006f964` 0xD2008 `535152565755`; `FUN_000983ec` 0xFAA90 `535189c380b8`; `FUN_000983dc` 0xFAA80 `25ffff000001`; `FUN_00097b90` 0xFA234 `535152565755`; `FUN_000ad38c` 0x10FA30 `535152565755`; `FUN_000ad148` 0x10F7EC `535156575583`; `FUN_000b3e24` 0x1164C8 `5289c283ea2a`; `FUN_000b3e84` 0x116528 `5289c283ea2c`; `FUN_0003a910` 0x9CFB4; port-out routine 0x9EC40 `53515689c389`; `FUN_000aff9c` 0x112640; `FUN_000b32ac` 0x115950; `FUN_00061214` 0xC38B8; `FUN_00060f4c` 0xC35F0; `FUN_00057c8c` 0xBA330; `FUN_0005d6e4` 0xBFD88; `FUN_0006e878` 0xD0F1C; `FUN_0006e644` 0xD0CE8. TACP: `FUN_00022f90` 0x7DA34; `FUN_00060720` 0xBB1C4; `FUN_0006cf64` 0xC7A08; `FUN_000250f8` 0x7FB9C; `FUN_00015494` 0x6FF38.

Vehicle-destroyed scorer, UFO2P file 0xFAA90 (capstone, excerpt):

```
faa94  cmp byte [eax+0x272],0 ; jne done              ; once per vehicle
faaa1  cmp word [ebx+0x12c],1 ; jne cat4              ; dead vehicle is Alien
faaab  test edx,edx ; je cat4                          ; need a projectile
faaaf  mov eax,[edx+0x15a] ; cmp word [eax+0x12c],0 ; jne cat4   ; firer is X-COM
faabf  ... mov ax,[eax*2+0x5862c]  (vehicle_data.score)  add [0x10c74],eax    ; cat 3 this-week
faae7  cmp word [ebx+0x12c],0 ; jne done                ; dead vehicle is X-COM
fab06  ... neg eax ; add [0x10c7c],eax                  ; cat 4 this-week
fab1b  mov byte [ebx+0x272],1
```

Unlock latch, UFO2P file 0xFA27E (excerpt):

```
fa27e  mov esi,[0x10c5c] ; mov edi,[0x10c60] ; add esi,edi       ; cat 0 week+prev
fa28a  mov ebp,[0x10c78] ; mov edi,[0x10c74] ; add edi,ebp       ; cat 3 week+prev
fa29f  cmp byte [edx+0x10b84],0 ; jne next                       ; flag k already set
fa2a8  imul ebx,edx,0x14 ; cmp ax,4 ; jge use_cat3
fa2ba  cmp esi,[ebx+ecx*4+0x10ad0] ; jl next ; jmp set          ; k<4: tactical
fa2ce  cmp edi,[ebx+ecx*4+0x10ad0] ; jl next                    ; k>=4: UFO kills
fa2d7  mov byte [edx+0x10b84],1
```

Alien infantry equipment level, UFO2P file 0x10FA6E (excerpt): `mov edx,[0x10c60]; mov eax,[0x10c5c]; add eax,edx; ... imul edx,edx,0x1c; cmp eax,[edx+esi+0x6a810]; jl done; add esi,4; inc ebp; cmp esi,0x1c; jl loop` (difficulty row of 7 thresholds at file 0x19BEB4), followed by the `FUN_0006e608(1,5)` / `FUN_0006ea78(1)` penalty.

---------------------------------------------------------------------------------------------------

## 9. Errata for `exe_evidence_index.md` and open leads

Errata:
- Index 5.2 "Score-gated alien gear" says the column order is "not independently verified" and row 4 is unnamed: the column index is the difficulty word at raw 0x10C9C (verified from the listing at file 0xFA2B1: `mov ecx,[0x10c9a]; sar ecx,16`), and row 4 is Dimension Shifter. The slice at 0x1421C4 is rows 4-8 of a 9-row table at 0x142174; rows 0-3 are a second family keyed on tactical score.
- Index 9a row `FUN_000ad148` (file 0x10F7EC) is labelled "invasion timer init". The decompile shows it is the **built-in equipment-set loader** (reads 92-byte set records at VA 0xF5248, calls `FUN_000b3e24` for the alien side, adds items through `FUN_000ad5ac` into the roster counter `[0x2440C4]` capped at 500). The invasion timer lives in `FUN_0004ab04` (`[0xD5068]`).
- Index Q10 (consumers at 0xC3642 / 0xC3907) and Q13 (the ~42 unclassified hits): answered in sections 3 and 6.
- Index Q8 (category 1 and 5 operands): answered in sections 4.1 and 4.2. Category 5 also fires at file 0x9D112 inside `FUN_0003a910` (the gate-arrival handler), not just the port-out routine.

Open leads (nothing here is a claim):
1. Alien-city structure table (section 6.2): find the writers that fill entries initially and remove them on raid victory (suspects: `FUN_000b3d4c` collapse, `FUN_000b4538` unlock writer, the city-1 load in `FUN_0006e098`); decode the 14-byte entry fields. Needed to confirm that type 5 is Organic Factory and to port the level penalty.
2. TACP hand-off: confirm that TACP `[0x10D34C]` is filled from the UFO2P block field `_DAT_002440A4` and that UFO2P `[0x24408C]` is TACP's `_DAT_001ed334` (trace the four `FUN_00013e64` chunk writes in `FUN_00015494`).
3. TACP equipment-captured / lost: confirm the item-table semantics (record layout at VA 0xEA2BC) to settle the equipment terms in the Total.
4. What `FUN_00080528` / `FUN_0007fdb0` represent (tile crush vs explosion) to size the "craft lost without an attacker" difference.
5. The `[0xE0E7C]`/`[0xD4D64]`/`[0xD4D68]` first-detection window (x100 detection multiplier): identify the two date words and compare with the engine's unbounded `firstDetection`.
