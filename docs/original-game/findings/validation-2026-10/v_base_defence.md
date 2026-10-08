# v_base_defence: base discovery and base defence, EXE versus engine

Binary: `UFO2P.EXE`, ISO non-4 generation (CRC `0x4749ffc1`), Ghidra project `OpenApocOG`, private copy `v_base_defence`, read-only, `-noanalysis`.
TACP.EXE was NOT queried (see "Not done" at the end).
Engine: `LIVE:` = frozen snapshot `scratchpad/live_src` (HEAD fda36d6c + uncommitted). `HEAD:` = committed fda36d6c. Where a file is cited without a prefix it is identical in both (checked by diff: `vehiclemission.cpp`, `organisation.cpp`, `battlemap.cpp`, `base.cpp` differ only by harness log lines; `building.cpp` differs materially, see F01/F02).

## 0. Reading rules

* OBSERVED = decompiler/listing text, bytes read from the EXE or from the original data files, or engine source lines. INFERRED = my reading of what observed text means; always marked.
* Every EXE function is cited as: binary + generation + **physical file offset** (= Ghidra VA + `0x626A4` for UFO2P code, + `0x616A4` for file-backed data), with the first bytes read directly from `canonical/UFO2P.EXE` (table in section 2). Ghidra VA is given as `FUN_000xxxxx` only as a label.
* The decompiler output of this binary mangles Watcom register arguments. Every claim below that depends on an argument or an ordering was re-read in the **listing**, not the decompiled C.
* Controls: (1) the string-pool resolver recovered `Organization raided:` / `Organization attacked:` and the call sites landed in `FUN_00092470`, which the repo docs had already bound as the scripted-incident dispatcher; (2) an instruction scan for the exposure word recovered the already-bound writers in `FUN_0005fddc`, `FUN_0006f7f8`, `FUN_0006f738`, `FUN_0007062c` (and found one the docs did not list, the clearer `FUN_0006779c`); (3) the building-collapse threshold was checked against the original building data files for **all 109 (and all 96) buildings**: it matched exactly (F13).
* Original data files used (read-only, extracted by `bsdtar` from `OpenApoc/data/cd.iso` into scratch, deleted afterwards): `XCOM3/UFODATA/CITYMAP{4,5}`, `CITYMAP{4,5}.BLD`, `CITYMAP.DAT`.
* Scripts I wrote (kept): `scratchpad/validate/vbd_work/scripts/{PoolStr,DumpFn,CallCtx,InsnGrep,Range}.java`; raw outputs `scratchpad/validate/vbd_work/*.txt`.

## 1. The answer in one screen

OBSERVED (UFO2P non-4):

1. A base-defence battle is entered from exactly one place: `FUN_000ac348` reason code 5, reached only via `FUN_000ac08c(.., 5, ..)`. `FUN_000ac08c` has 7 call sites; three pass reason 5: two inside the UFO arrival handler `FUN_0003a910` and one inside `FUN_00099e04` (the flag raised by the organisation incident executor `FUN_00092470`). The string `Hostile forces have invaded your base. Equip your Agents before battle.` (group 0x16 item 0x82, file `0x14D93E`) is drawn only by `FUN_000b48d0`, whose only caller is `FUN_000ac348`.
   * UFO path: an Infiltration UFO (`vehicle+0x15C == 3`) arriving over an **active X-COM base building** calls reason 5 at once. No exposure is required, no aliens are deposited, no 5 percent roll happens. A Subversion UFO (`== 2`) calls reason 5 only if the base's exposure word `+0x2BC` is non-zero; otherwise it sets that word (`FUN_0007062c`) and no battle follows.
   * Organisation path: a hostile organisation's scheduled incident of type 2 (Raid) or 3 (Storm) executes against an X-COM-owned building that is an active base: relation `-25` call, flag, `FUN_00099e04` -> reason 5. Types 1 (Attack), 6, 7, 8, 9, 10 do nothing to an X-COM building.
   * Nothing else. **Alien movement into a base building and building detection never start a base defence.** `FUN_0006f7f8` (movement) and `FUN_0006f964` (detection) contain no call to `FUN_000ac08c`.
2. Attackers: for a UFO, the **static** per-UFO-type crew table (`crew_deposit`, file `0x13E790`, 10 rows) minus species 0, 4, 11 (egg, chrysalis, queenspawn), at most 36 aliens and at most 60 units in total. For an organisation raid: `org.average_guards * (rand[0..50] + 75) / 100`, capped at 20. Aliens already resident in the base building do not fight.
3. Defenders: every live X-COM agent located at the base, unarmed staff included, plus class-0 agents riding docked craft; budget `36 - min(12, 4 * finished Security Stations)`; proportional per-class scaling when over budget.
4. Empty base (no live agent present): destroyed on the spot ("X-COM base destroyed by hostile forces").
5. Battle outcome: success flag == 1 -> X-COM funds `-= 50 * worth`; anything else (loss or retreat) -> base removed. Briefing text: "You can retreat from the base to cut your losses, but the base will be lost."
6. Base loss by collapse is a separate rule: a building is destroyed when `10 * sum(tile values) < original sum` (>90 percent of value gone), not when the last tile is gone.
7. Hostile organisations DO raid X-COM bases in UFO2P. Hostile means relation `<= -50` (signed byte compare), not `< -50`. The Cult of Sirius starts at -50 plus the difficulty offset (-70..-50 at Superhuman) and so is hostile to X-COM at the start. The incident-type draw is biased by difficulty; at Superhuman a triggered Cult incident is a Raid 25 percent of the time (engine rules: 10 percent), and a Raid only reaches a base when the randomly picked X-COM building is a base (1 in 2 with one base plus the starting Recyclotorium): 12.5 percent per Cult-to-X-COM trigger.

## 2. Function and data map (UFO2P non-4; first 8 bytes read from the EXE at the physical offset)

| Label | VA | Physical offset | First bytes | Role (OBSERVED unless noted) |
|---|---|---|---|---|
| `FUN_0003a910` | 0x3A910 | 0x09CFB4 | 5351525657 5583ec | UFO arrival/mission handler; reason-5 calls at VA 0x3AE76 (file 0x9D51A, Infiltration), 0x3B079 (file 0x9D71D, Subversion known) |
| `FUN_000ac08c` | 0xAC08C | 0x10E730 | 56575583ec688944 | event entry (EAX target id, EDX reason, EBX actor, ECX ufo id); reason 5 + no defenders -> ticker + base removal |
| `FUN_000ac348` | 0xAC348 | 0x10E9EC | 56575583ec1489c7 | event UI + battle setup; reason 5: dialog, roster, launch |
| `FUN_000b48d0` | 0xB48D0 | 0x116F74 | 5351525657 5569c0 | BASE ATTACK dialog (group 0x16 items 0x81, 0x82) |
| `FUN_000b2e60` | 0xB2E60 | 0x115504 | 5156575583ec0489 | "are there defenders?" test; reason-5 branch reads the agent array |
| `FUN_000ac7d0` | 0xAC7D0 | 0x10EE74 | 5351525657 5583ec | X-COM defender roster builder |
| `FUN_000b42ec` | 0xB42EC | 0x116990 | 5351525657 5569c0 | Security Station turret count |
| `FUN_000ae218` | 0xAE218 | 0x1108BC | 535156575589c785 | alien roster (reason 5 from UFO crew table; reason 1 downed UFO) |
| `FUN_000aedb0` | 0xAEDB0 | 0x111454 | 53515256575 589c1 | organisation guard roster |
| `FUN_000b38b4` | 0xB38B4 | 0x115F58 | 535156575583ec10 | base-defence map assembly from the 8x8 cell grid |
| `FUN_000b32ac` | 0xB32AC | 0x115950 | 5351525657 5583ec | post-mission result handler; type 2 = base defence |
| `FUN_000b3114` | 0xB3114 | 0x1157B8 | 5351525689c169f0 | base removal (owner := Government, slot freed, re-home away units) |
| `FUN_00092470` | 0x92470 | 0x0F4B14 | 5351525657 5581ec | incident executor; every call (5 min) decrements each of 10 slots |
| `FUN_00092060` | 0x92060 | 0x0F4704 | 5351525657 5583ec | incident scheduler (daily) |
| `FUN_00099e04` | 0x99E04 | 0x0FC4A8 | 535152833d987705 | `if flag: reason-5 event(building=[0x12779C], org=[0x1277A0])` |
| `FUN_000638dc` | 0x638DC | 0x0C5F80 | 535156575589c189 | difficulty-biased random (sum of 3 draws minus offset, rejection sampled) |
| `FUN_0005d1d8` | 0x5D1D8 | 0x0BF87C | 5351525657 89c38b | rand16 inclusive `[0..n]`, `n<0` treated as 0 |
| `FUN_0006f7f8` | 0x6F7F8 | 0x0D1E9C | 5351525657 5583ec | alien movement, one source building |
| `FUN_0006f5e4` | 0x6F5E4 | 0x0D1C88 | 5351525657 5583ec | movement destination picker |
| `FUN_0006f964` | 0x6F964 | 0x0D2008 | 5351525657 5583ec | building detection, 5 buildings per call |
| `FUN_0005fddc` | 0x5FDDC | 0x0C2480 | 515689c10fbfc2e8 | deposit of one species into a non-base building; contains the 5 percent exposure roll |
| `FUN_0007062c`, `FUN_000705a8` | 0x7062C, 0x705A8 | 0xD2CD0, 0xD2C4C | 53525689c3b82e94 / 535256578b35c00c | set / test exposure word |
| `FUN_00070140` | 0x70140 | 0x0D27E4 | 5351525657 5583ec | infiltration target picker |
| `FUN_00070acc` | 0x70ACC | 0x0D3170 | 5351525657 5583ec | building-collapse test |
| `FUN_0006779c` | 0x6779C | 0x0C9E40 | 5351525657 5583ec | creates a base record, clears exposure word |
| `FUN_0006bc70` / `FUN_000105b8` | 0x6BC70 / 0x105B8 | 0xCE314 / 0x72C5C | 53515256575583ec / 5351525657 55e805 | daily: facility build countdown, then incident scheduler |
| `FUN_00091db0` / `FUN_00091de4` | 0x91DB0 / 0x91DE4 | 0xF4454 / 0xF4488 | | `rel <= [0x1277E4]` (hostile) / `rel >= [0x1277E5]` (allied) |
| Data | VA | Physical | Content |
|---|---|---|---|
| hostile / allied thresholds | 0x1277E4/5 | 0x188E88/9 | bytes `ce 4b` = -50 / +75 |
| incident timer | 0x1277A4 | 0x188E48 | initial `0` |
| incident type table A | 0x1277A8 | 0x188E4C | 3 rows x 10 bytes (militarised) |
| incident type table B | 0x1277C6 | 0x188E6A | 3 rows x 10 bytes (others) |
| incident slots | 0x13E280 | BSS | 10 x 8 bytes |
| UFO crew deposit | 0xDD0EC | 0x13E790 | 10 rows x 14 dword |
| alien move percent | 0xE0D0C | 0x1423B0 | 13 x dword |
| alien detection weight | 0xE0CD0 | 0x142374 | 13 x dword |
| building detection weight | 0xF3CB0 | 0x155354 | 49 x dword |
| base record array | 0xD942E | | 16 x 0x2BE; `+0x15` active, `+0x16..` 8x8 cell types, `+0x56..` cell construction days, `+0x2BC` exposure word |
| building array | `[0x18276C]` | | stride 0xE2; `+0xBC` detection timer, `+0xC8` owner, `+0xD4` crew[14] |

String pool: group pointer table at VA 0xF3794; `FUN_00063A00(EAX=group, EDX=item)` skips `item` NUL-terminated strings. Resolved here: `BASE ATTACK` g0x16/i0x81 file 0x14D932; invasion text g0x16/i0x82 file 0x14D93E; `X-COM Base destroyed due to collapsing building.` g0x18/i0x24 file 0x14DDCA; `X-COM base destroyed by hostile forces.` g0x18/i0x2C file 0x14DF03; `Organization attacked:` g0x18/i0xE; `Organization raided:` g0x18/i0x10; `All X-COM bases have been destroyed...` g0x2C/i0x15 file 0x154069.

## 3. Findings

Verdict key: match / diverges / engine_missing / engine_bug / exe_unbound. Impact is for the Superhuman strategy.

### F01  What starts a base-defence battle: movement and detection must not (LIVE matches, HEAD does not)

EXE (OBSERVED). Callers of `FUN_000ac08c` (file 0x10E730), from `CallCtx`: `0x3AE76` and `0x3B079` in `FUN_0003a910` (EDX=5), `0x99E2D` in `FUN_00099e04` (EDX=5), `0x3B266` in `FUN_0003a910` (EDX=1), `0x60E58` in `FUN_00060c18` (EDX=2), `0x71503` (EDX=0) and `0x7186B` (EDX=0 or 3) in an undefined code region. `FUN_000b48d0` (the dialog) has the single caller `FUN_000ac348` (VA 0xAC3E1), under `CMP ESI,5`. `FUN_000b38b4` (the base-defence map) has the single caller `FUN_000ac348` (VA 0xAC702). `FUN_0006f7f8` callees: `0004e1b4, 0005d1d8, 0005f15c, 0006f5e4, 00091de4` only. `FUN_0006f964` callees: `0002ae1c, 0005d1d8, 00063a00, 00091db0, 00091de4, 000983dc, 000b71b4` only. Neither calls `FUN_000ac08c`, `FUN_000ac348`, or `FUN_000b3114`.

`FUN_0006f964` on detection (listing 0x6FB18-0x6FBEF) writes an incident record of type 2 for the building, calls the score helper with category 2 and delta `0xFFFFFFE2` (-30), sets the building's detection timer `+0xBC` to `0x168`, and prints a ticker string (group 0x18 item 2). No base special case.

Engine.
* LIVE `building.cpp:835-844`: detection in a base building sets `detected` and pushes `AlienSpotted`; the comment cites `FUN_0006f964`. LIVE `building.cpp:1029-1040`: movement into a base building only rolls exposure. Matches the EXE.
* HEAD `building.cpp:815-826` (`detect`: `base->die` if unoccupied else `DefendTheBase`) and `:1022-1036` (`alienMovement`: same). Both are inventions not present in UFO2P.
* The LIVE comment points to `docs/original-game/findings/base-defence.md`; that file does not exist in the LIVE worktree or in `live_src` at snapshot time. The LIVE change is uncommitted.

Evidence from the benchmark (from `research/failure_forensics.md` section 9, not re-measured here): 13 of 15 logged base defences were movement-triggered, 1 detection-triggered, 1 UFO deposit. So the committed engine is wrong on the dominant failure path and the uncommitted change is the fix.

Absence control (done after review): a byte-level scan of the whole code object (`.object1`, 780,461 bytes) for `E8`/`E9 rel32` whose target is `FUN_000ac08c`, `FUN_000ac348`, `FUN_000b48d0`, `FUN_000b38b4`, `FUN_000b3114`, `FUN_00099e04` or `FUN_00070acc`, plus a scan of all initialised blocks for the 4-byte absolute form of those addresses (function pointers, `PUSH/MOV imm32`), returned exactly the call sites listed above and nothing else (28 REL32 hits, 0 ABS32 hits; output `vbd_work/e1_raw.txt`). The two `FUN_000ac08c` call sites in code Ghidra never defined as a function (0x71503 and 0x7186B) were already known and pass reason 0 or 3, not 5 (the other two hits in that region, 0x71442 and 0x71776, call the collapse test `FUN_00070acc`). So the reason-5 producer set is complete.

LIVE leftover path (found after review, not an automatic trigger): LIVE still force-detects aliens inside the player's own base building (`building.cpp:158`), which raises `AlertScreen`; `BUTTON_EXTERMINATE` (`alertscreen.cpp:125-168`) sends agents/craft to that building, and `battlemap.cpp:176` turns any mission at a player-owned building that has a base into `MissionType::BaseDefense` with the building's `current_crew` as the enemy. So a base-defence-type battle against *resident* aliens can still start by player or harness choice. The EXE detects with the weighted roll (F07) and its extermination path for an X-COM-owned building was not examined. Also stale: `alertscreen.cpp:60-62` still says "a crew arriving in a base starts a base defence" and `:84` exports `threatens_base` to the harness on that premise.

Verdict (LIVE): match for automatic triggers. Verdict (HEAD): engine_bug. Impact: critical. Fix: commit the `building.cpp` hunk; add a regression test that alienMovement into an occupied base never emits `DefendTheBase`; update the stale `alertscreen.cpp` comment/`threatens_base` text; decide the forced-detection path in F07. Strategy rule: "garrison is needed against UFO landings and hostile-organisation Raid/Storm only; alien spread into the base building is not an attack."

### F02  UFO landing: Infiltration attacks any active base at once; Subversion needs exposure (LIVE matches on triggers)

EXE (OBSERVED, `FUN_0003a910`, listing 0x3AE2D-0x3AEDE for role 3 and 0x3AFC8-0x3B0A3 for role 2).
* Role 3 (Infiltration), after the landing doodad: `slot = FUN_000705f8(target building)`. If `slot == -1` (ordinary building): for each of 14 species with crew `!= 0` call `FUN_0005fddc` (deposit) then `FUN_0006f7f8` (movement). Else (X-COM base): set arrived flag, `DAT_000e0cc0 := 0`, `FUN_000ac08c(EAX=target, EDX=5, ECX=ufo id, EBX=1)`. The exposure word is neither read nor written.
* Role 2 (Subversion): `FUN_000705a8(target)` returns 1 only for an active base whose `+0x2BC` word is non-zero (or the never-set override `DAT_000e0cc0`). If 0: `FUN_0007062c` sets the word and nothing else happens. If 1: reason-5 call as above.

Engine. LIVE=HEAD `vehiclemission.cpp:2578-2665`: subvert on unknown base marks it known (`:2615-2626`); subvert on known base and all infiltration deposit and (if the building has an agent) push `DefendTheBase` (`:2644-2662`); an unoccupied base dies (`:2644-2647`). Same trigger set as the EXE.

Differences kept as separate findings: crew handling (F04), the 5 percent roll (F05), "unoccupied" test (F09). Verdict: match. Impact: high. Fix: none for triggers.

### F03  Which aliens attack, and how many (diverges)

EXE (OBSERVED, `FUN_000ae218` file 0x1108BC, branch `param_2 == 5`). For species index k in 0..12 it reads the **static table** `[0xDD0EC + ufoType*0x38 + k*4]` (file `0x13E790`), zeroes species 0, 4 and 11, and adds one alien per unit while `DAT_002440D4 < 0x3C` (total units < 60) and `DAT_00244528 < 0x24` (aliens < 36). The roster is not read from the building. Per UFO type (I read the 10 rows from the EXE; row labels from engine `vehicle_types.xml` crew totals, which equal the EXE totals: Probe 4, Scout 13, Transporter 20, Fast Attack 10, Destroyer 22, Assault 27, Bomber 8, Escort 13, Battleship 20, Mothership 24):

| UFO | crew in table | attackers after excluding egg/chrysalis/queenspawn |
|---|---|---|
| Probe | 4 | 4 |
| Scout | 13 | 10 |
| Transporter | 20 | 15 |
| Fast Attack | 10 | 10 |
| Destroyer | 22 | 17 |
| Assault | 27 | 22 |
| Bomber | 8 | 8 |
| Escort | 13 | 9 |
| Battleship | 20 | 15 |
| Mothership | 24 | 19 |

(Rows 0..9 of the table are in that order; the order was inferred from total-crew equality with the engine data, and `Assault 27` is corroborated by the benchmark log line "Alien Assault Ship ... 27 alien(s)".)

Because X-COM units + turrets fill up to 36 slots when staffed, in practice aliens are further limited by `60 - (X-COM units + turrets)`; with 36 X-COM-side units that is 24, which is above every attacker count in the table (maximum 22).

Engine. LIVE=HEAD `battlemap.cpp:182-190`: for an alien opponent the attackers are `building->current_crew`, i.e. everything in the base building: the UFO deposit including **eggs**, plus any **resident** aliens that had wandered in, with no species exclusion and no 36/60 cap other than the unit limit. The deposit itself is `vehiclemission.cpp:2631-2634`. The 27 in the benchmark log is the whole Assault crew; the EXE sends 22.

Verdict: diverges. Impact: medium-high (resident aliens and eggs inflate fights; the old movement trigger made residents common). Fix: for a UFO-origin defence build the alien list from `v->type->crew_deposit` minus {egg, chrysalis, queenspawn} and do not deposit into the building; leave resident aliens in place and not part of the battle. Strategy rule: the garrison needed against a landing is 4 to 22 attackers, not 27+.

### F04  Defender selection, cap and turret reservation (diverges)

EXE (OBSERVED, `FUN_000ac7d0` file 0x10EE74, `FUN_000b42ec` file 0x116990, `FUN_000acbb4` file 0x10F258).
* Eligible agents: `+0x7D != 0xFF` (has a location), `+0x21 == 0` (employed/alive), `+0x7C == [0xD5060]` (the current-city flag; INFERRED meaning). Located either at the base (`+0x7D == 1` and `+0x7E == base id`) or inside a craft docked at the base (`+0x7D == 3`, `+0x23 == 0`, and the craft's base word equals the base id). `+0x23` is a class byte 0..3; class 0 agents carry an inventory (`FUN_000acf9c` walks the 52-slot inventory at `0x173DC8 + inv*0x9E`); classes 1..3 are added without one. INFERRED: class 0 = combat-capable agent, 1..3 = the technical staff types.
* Armament is never tested. Unarmed staff defend.
* Turrets: `FUN_000b42ec` scans the 8x8 grid; every cell with `+0x56 == 0` (finished) and type byte `0x17` or `0x18` adds 4. Type byte minus 14 is the facility name item in string group 5, and group 5 item 9 is `Security Station`, item 10 `Advanced Security Station`, so 0x17/0x18 are the two Security Stations (OBSERVED via the pool resolver). `FUN_000ac7d0` clamps the count to 12 (3 stations).
* Budget for agents: `0x24 - turrets` (36 minus turrets). Confirmed in the listing (0xAC7FD-0xAC937, four class counters in ECX/EDI/ESI/`[ESP+0x14]`, the decompiler had dropped class 0): if the number of eligible agents exceeds the budget every class count is scaled by `budget / total` (integer division) and then the **first n agents of each class in array order** are taken (second pass 0xAC94D-0xACA48). `FUN_000acbb4` also stops at 36 X-COM units and 60 total. The turret count is then added to the unit total (`DAT_002440D4 += turrets`), so turrets occupy roster slots.

Engine. LIVE=HEAD `battlemap.cpp:211-250`: first agents inside docked craft, then every *Soldier* role agent whose `currentBuilding` is the base, then non-soldier staff whose `homeBuilding->base` is the base **even if they are away**, up to `MAX_UNITS_PER_SIDE = 36` (`battle.h:28`). Turrets are added separately from sector `guardianLocations` (`battlemap.cpp:1214-1221`), uncapped and not charged to the 36; unfinished facilities are omitted (`:969-972`).

Verdict: diverges. Impact: medium (3 stations: EXE gives 24 agent slots; engine 36 agents plus 12 turrets, and away technicians are drafted). Fix: draft by presence, not by home base; cap agents at `36 - min(12, 4*finished stations)`; cap active stations at 3. Whether the original TACP spawns the turrets from the base map or from this count is not established (blocked_on_re for the turret side only).

### F05  Exposure ("known to aliens") writers (diverges: one dead writer in the EXE)

OBSERVED. A scan of every instruction that references the base-record `+0x2BC` word (as `0xD96EA` absolute or `+0x2BC]`) finds: writes of 1 in `FUN_0005fddc` (VA 0x5FE06), `FUN_0006f738` (0x6F7A5), `FUN_0006f7f8` (0x6F917), `FUN_0007062c` (0x70654); one write of 0 in `FUN_0006779c` (0x67A55, base creation); reads in `FUN_000702e4` (0x70317) and `FUN_000705a8` (0x705C8). No other code touches the word, so exposure is permanent for the life of a base record and a re-created base in a freed slot starts unexposed.

* `FUN_0006f7f8`: `rand16(100) < 5 * moved_total` (inclusive roll 0..100) when a movement into a base building happens. Engine LIVE `building.cpp:1036-1040` + `base.cpp:24-27`: same. Match.
* `FUN_0007062c`: first Subversion landing. Match (engine `vehiclemission.cpp:2625`).
* `FUN_0006f738`: population transfer after types 1/4 results, `rand16(100) < 5 * count` per species. Engine: no equivalent writer found; the engine's post-raid retreat housing (`battle.cpp:3003-3030`) is a different mechanism. Unverified.
* `FUN_0005fddc`: `rand16(100) < 5` exposure, but only if `FUN_0005f15c(building)` finds an active base. Its only caller (VA 0x3AEAF) is guarded by `FUN_000705f8(building) == -1`, and the two functions have the same body. **INFERRED, high confidence: the 5 percent roll can never fire in UFO2P non-4.** Engine LIVE=HEAD `vehiclemission.cpp:2637-2642` rolls it after an infiltration landing on a base. Divergence: engine marks bases known with probability 5/101 per defence; the EXE does not.
* Effect of exposure: only Subversion targeting. `FUN_000702e4` reads the word per slot; infiltration target choice (`FUN_00070140`) ignores it (F15). Engine `gamestate.cpp:882-897, 999-1007` and `vehiclemission.cpp:2580` agree.

Verdict: diverges (low). Impact: low-medium. Fix: delete the 5 percent roll in the infiltration branch; keep everything else. Strategy rule: exposure comes from a Subversion landing, from aliens (>=3 moved at once) walking into the base building, or from transfers; it never comes from a defence battle in the EXE.

### F06  Alien movement: gate, formula, destination (diverges)

EXE (OBSERVED, `FUN_0006f7f8` file 0xD1E9C, listing 0x6F81B-0x6F961; `FUN_0006f5e4` file 0xD1C88). For one source building:
1. One draw `r = rand16(30)` shared by all species.
2. `moved[k] = crew[k] * (movePercent[k] + r) / 100` with `movePercent` at file 0x1423B0 = 40, 40, 33, 33, 0, 33, 33, 33, 33, 0, 0, 0, 30. No clamp at 100.
3. `total = sum(moved)`. **Only if `total >= 3`** (`CMP ESI,3; JL`).
4. Destination: `FUN_0006f5e4` collects, in array order, up to **15 buildings** whose centre lies strictly inside the box `|dx| < 15, |dy| < 15` of the source centre and for which `FUN_00070acc(b) == 0` (not collapsed); picks one uniformly (`rand16(n-1)`). It is the first 15 in array order, not the 15 nearest.
5. Move if `rand16(100) < 15 + 3*total`, plus 20 if the **source** building's owner has the organisation flag at `+0x11` set (taken over) or `FUN_00091de4(owner, Aliens)` (relation owner->aliens `>= 75`).
6. On a move, exposure roll as in F05 if the destination is a base. Nothing else: no battle, no base kill.
Called hourly for every building (`FUN_0004abd4` -> `FUN_0006f7d4`), and once per species deposit in `FUN_0003a910`.

Engine. LIVE `building.cpp:970-1042`: per-species draw `rand[0..30]` with `min(100, ...)` (`:993-994`); moves when `totalMoveAmount > 0` (`:1002`); chance `15 + 3n` plus 20 when the **destination** owner is Friendly or Allied to aliens, i.e. relation `>= 25` (`:1008-1019`); destination = the 15 nearest intact buildings by Manhattan distance of centres `<= 15` (`building.h:23-26`, `rankNearbyIntact` `:907-937`).

Consequences. 1 or 2 moving aliens can spread in the engine and cannot in the EXE (a single 33 percent species needs about 5 or more aliens before 3 move). The benchmark's "lone Hyperworm" defences were exactly this case. The bonus keyed on the destination's owner instead of the source's changes which spreads accelerate. The neighbourhood is different in shape and selection.

Verdict: diverges. Impact: high (spread drives infiltration of the Government and the harness's early base fights before F01 was fixed). Fix: gate on `total >= 3`; one shared roll; bonus from the source owner (`takenOver` or relation to aliens `>= 75`); candidate set = first 15 intact buildings in building order within `|dx|<15 && |dy|<15`. Strategy rule: an infestation of 4 or fewer aliens never spreads; clearing nearby buildings is what keeps the base from being exposed by movement.

### F07  Building detection ("alien incidents"): cadence matches, formula and the forced-detection rule do not

EXE (OBSERVED, `FUN_0006f964` file 0xD2008). Called every 5 game minutes from `FUN_0004ab04` (`param_1 % 5 == 0` in `FUN_0004aa7c`), five buildings per call (counter initialised to 5), persistent cursor `[0xF3D74]`, wrapping at the building count `[0x183A68]`. For a building whose detection timer `+0xBC` is 0:
* `n = 0; for each species k with crew != 0: r = rand16(100); n++; if r < 100/n: sel = k; value += crew[k] * alienDetectWeight[k]` (weights at file 0x142374: 1,1,3,1,1,3,3,3,2,10,8,20,1).
* first-day boost (`value *= 100` when `[0xE0E7C]==1 && [0xD4D64]==1 && [0xD4D68]<16`).
* `value = value * (buildingDetectWeight[function] - 2*difficulty) / 100` (weights at file 0x155354: Warehouse 100, Slums 95, Offices 120, X-COM Base 0 ...).
* `value /= 2` if relation owner->Aliens `>= 75`; **else** `value /= 2` if relation owner->X-COM `<= -50`; then `value /= 2` again if the owner is taken over.
* If `sel != -1 && rand16(100) < value - 10`: detected. Timer `+0xBC := 0x168`, incident record, category-2 score `-30`, ticker.
* Timer unit: `FUN_00049b4c` (file 0xAC1F0, listing 0x49B77-0x49BAB) subtracts its argument (1) from every building's `+0xBC` word per call, and `FUN_0004aa7c` calls it once per game-minute step (5 times when `[0xD5064] != 0`); so 360 minutes. The per-minute cadence of `FUN_0004aa7c` is INFERRED from its `% 5` and `% 0x1e` tests.
* Full sweep time = building count in minutes. I read the building counts from the original data: 68, 78, 84, 96, 109 for difficulties 0..4, which are exactly the engine's 70/80/85/95/110 rounded to 5.

Engine. LIVE `building.h:14-20`, `building.cpp:139-160, 771-844`: per-building attempt every 70..110 minutes (match); timeout 6 hours (match); score -30 per `AlienSpotted` (`cityview.cpp:4847`, match). Differences:
* spot gate per species `rand[0,100) < 100/(count+1)` (`:787`). The EXE gate is the reservoir pick `100/n` over *species present*, which almost always succeeds regardless of crew size; the engine's gate suppresses large infestations.
* Allied-to-aliens and hostile-to-player halvings are independent (`:799-806`); EXE is else-if.
* Rolls are exclusive 0..99; EXE inclusive 0..100.
* **Forced detection of the player's own buildings** (`:158`: `owner == state.getPlayer()`): any alien in a base building raises `AlienSpotted` every 6 hours and costs -30 score each time. The EXE has no such rule. UNVERIFIED (medium confidence): the base building appears to keep its original function (Warehouse etc., weight about 100): the purchase routine `FUN_00015400` and the start-base routine `FUN_000669fc` write the owner byte only, and the only two writers of a `+0xAA` word/dword found anywhere (`FUN_000240a4`, `FUN_000a12f4`) are large UI-style functions that I did not read. If that holds, the EXE detects aliens in a base building by the normal weighted roll, and a handful of aliens (sum of crew*weight*(100-8)/100 <= 10) is never detected; if something sets function 37 (X-COM Base, weight 0) the base building is never detected at all. Either way the engine's forced detection has no EXE counterpart.

Remaining LIVE side effect: the forced path also feeds the AlertScreen/EXTERMINATE route described in F01. Verdict: diverges. Impact: medium now that F01 is fixed (score bleed of up to -30 per 6 hours per infested base building: -120/day each; weekly score thresholds in the funding tiers are -400/-800/-1600 and the cutoff is H < -2400). Fix: drop the forced path and use the EXE formula. Cadence: no change.

### F08  Hostile-organisation raids on X-COM bases exist; trigger chain (match in structure)

EXE (OBSERVED). `FUN_000105b8` (new day) calls `FUN_0006bc70`, which decrements every facility's construction days and then calls `FUN_00092060` (the scheduler) once per game day. `FUN_00092060`:
* for A in 2..26 (never X-COM 0, Aliens 1, or Civilian 27) and B in 0..26 except 1: skip unless A has a building with `FUN_00091f70`; skip unless `matrix[A][B] <= [0x1277E4]` where the byte is `0xCE` (-50);
* pressure `p = max(1, snapshot[A][B] - matrix[A][B])`; trigger iff `p > rand16(timer)` and an incident slot is free (10 slots, `FUN_00091e68`);
* on trigger: `timer := 80 - 2*difficulty` (`[0xE0C9C]`); otherwise `timer -= 1` (shared by all pairs); the timer starts at 0 (data bytes at 0x188E48), so the first hostile pair reached in index order triggers at the first day rollover; because A runs in ascending index (Government 2, Megapol 3 before Cult 4), whether that first trigger is the Cult's depends on the Government/Megapol start relations (not computed);
* target building = a uniformly random building of org B (`FUN_00091f70`), with `strengthA < strengthB` -> row 0, `2*strengthB < strengthA` (and A != Megapol) -> row 2, else row 1, where `strength = (workforce/100) * guards * guards`;
* table A for A in {Megapol 3, Psyke 20, Diablo 21, Osiron 22} or taken-over organisations (`org+0x11 != 0`), else table B; code = `table[row][FUN_000638dc(9,3)]`; stored with delay `rand16(240) + 10` units of 5 minutes (50..1250 min).
* Execution (`FUN_00092470`, every 5 minutes): when the countdown hits 0, types 2 and 3 on a building owned by org 0 for which `FUN_000705f8(building) != -1` set `[0x12779C] := building`, `[0x1277A0] := A`, `[0x127798] := 1`, call the relation primitive with delta -25 (arguments: holder 0, target A; the primitive skips holder 0 so it is a no-op on the byte matrix, INFERRED from the listing at 0x926AB-0x926C9 and the guard `TEST SI,SI; JZ` at 0x5FB1B), and `FUN_00099e04` turns the flag into reason 5 on the next minute tick (`FUN_0004aa7c` loops `FUN_00099e04`).

Engine. LIVE=HEAD `organisation.cpp:350-430` (scheduler, daily from `gamestate.cpp:1578-1585`), `RaidMission::execute` `:1285-1331` (Raid `:1293`, Storm `:1317`: on a player base: die if no agents, else `DefendTheBase` with the organisation as attacker). The structure, pressure rule, shared timer `80 - 2*difficulty`, start value 0, 50..1250 minute delay, strength rows and the Megapol exception all match the EXE. Differences in F09-F11.

Verdict: match (structure). Impact: high. The existence of organisation base attacks from day 1 is confirmed by the EXE; the web claim that they are "lore" is wrong.

### F09  Hostile threshold: EXE `<= -50`, engine `< -50` (diverges)

EXE (OBSERVED): `CMP AH,[0x1277E4]; JG` with the byte `0xCE`, so hostile means relation `<= -50`. The same threshold is used by `FUN_00091db0` and by the vehicle-park refill noted in the evidence index. Allied for aliens is `>= 75` (`[0x1277E5] = 0x4B`).
Engine: `Organisation::isRelatedTo` `organisation.cpp:1085-1095` uses `x < -50`.
Start state (OBSERVED, `FUN_00099130` file 0xFB7D4, matches the engine's `gamestate.cpp:620-660`): relation of organisation i toward X-COM is `base + (10 - 5*difficulty) + rand[-10..10]`; at Superhuman the Cult's -50 base gives -70..-50, so it is hostile in the EXE in all 21 cases and in the engine in 20 of 21. At lower difficulty the gap is larger (difficulty 2: EXE hostile for 11/21 start values, engine 10/21).
Also EXE `FUN_00099130` treats a relation of exactly 0 between two non-X-COM organisations as "not positive" (worsened by `rand[0..5d]`), the engine as positive (improved); not pursued here.

The EXE uses **two different thresholds**: raids, detection halving (`FUN_00091db0`) and the vehicle-park refill use `<= -50`, while the weekly funding cutoff is strict `< -50` (`CMP EAX,-0x32; JGE` at file 0xF6E9C, bytes `83 f8 ce 7d 08`, evidence index 5.1; LIVE funding uses `government->isRelatedTo(player) == Hostile`, `gamestate.cpp:1792`). So `isRelatedTo` must NOT be changed globally. Fix: add a separate predicate (for example `isHostileInclusive()` = relation `<= -50`) and use it in `setRaidMissions`, the detection halving in `Building::detect`, and the park refill; leave funding on the strict form. Verdict: diverges. Impact: medium. Strategy rule: for raids a relation of exactly -50 is already hostile; for funding it is not; keep sponsor organisations above -50 and the Government above -50.

### F10  Incident-type draw: difficulty-biased in the EXE, a flat probability map in the engine (diverges)

EXE (OBSERVED): `FUN_000638dc(9, 3)` returns `S - off` where `S` is the sum of three `rand16(9)` draws and `off = floor((4 - difficulty) * 9 / 4)`, rejection-sampled into 0..9. Offsets: difficulty 4 -> 0, 3 -> 2, 2 -> 4, 1 -> 6, 0 -> 9. At Superhuman the index distribution is `[1,3,6,10,15,21,28,36,45,55]/220` for 0..9, so high indices dominate. The tables (bytes read at file 0x188E4C and 0x188E6A):

* table A rows: `9 8 9 8 9 8 1 2 6 2` / `9 8 1 6 9 1 1 2 2 3` / `8 9 1 6 2 2 2 3 6 6`
* table B rows: `9 9 9 9 8 8 9 8 9 2` / `9 9 8 9 8 9 8 8 8 2` / `9 8 9 9 9 8 8 1 1 2`

Code 2 = Raid, code 3 = Storm (each stored code is the executor case number; code 1 = Attack, 6 = unauthorised vehicle, 8 and 9 relation events). Only 2 and 3 attack a base. Probability that one trigger yields a base attack when the target is an X-COM base:

| Superhuman (difficulty 4) | EXE | Engine rules (`extract_organisations.cpp:366-397`) |
|---|---|---|
| Non-militarised (Cult of Sirius and most corporations), any row | Raid 25.0 percent | Raid 10 percent (row "low": 0) |
| Megapol (row 1) | Raid 36.8 + Storm 25.0 = 61.8 percent | 20 + 10 = 30 percent |
| Psyke/Diablo/Osiron/taken-over (row 2) | Raid 29.1 + Storm 16.4 = 45.5 percent | 30 + 10 = 40 percent |
| Same, difficulty 0 (for contrast) | Non-militarised 8.2, Megapol row 1 27.9, row 2 43.6 | same as above |

The engine's six probability maps are hard-coded in the extractor, not read from the EXE, and the engine does not model the difficulty bias at all. In addition the engine's `None`/`Treaty` split does not match the table (`military_low_manpower` None 40 / Treaty 20 against EXE 30 / 30).

Verdict: diverges. Impact: high for the Cult (the only organisation hostile at the start): a trigger is 2.5 times more likely to be a base attack at Superhuman. Fix: replace `probabilityMapRandomizer` with the EXE two-table lookup and the biased draw (`sum of 3 x rand[0..9] - floor((4-d)*9/4)`, reject outside 0..9). Needs the meaning of codes 7, 8, 10 only for non-base effects, which are out of scope (blocked_on_re: false for the base-attack part).

### F11  Other organisation-raid differences (diverges, low-medium)

* EXE excludes the Alien organisation as a target (B != 1); engine iterates every organisation (`organisation.cpp:363-366`), so hostile-to-Aliens pairs (Government and others) consume the shared timer and schedule raids on alien buildings. More pair checks per day also lower the timer faster.
* Raid or Storm on an X-COM building that is **not** a base: EXE does nothing (the building is X-COM-owned, `FUN_000705f8 == -1`, and the `owner != 0` branch is skipped). The original data gives X-COM exactly one building before any base is bought (building index 44, "Recyclotorium", function 33; read from `CITYMAP5.BLD`). Engine `RaidMission::execute` applies relation penalties and emits `OrganisationRaidBuilding/Storm` events for a non-base target (Raid branch `organisation.cpp:1308-1315`, Storm branch from `:1317`); Attack on any building including an X-COM one (`:1285-1291`) applies -5/+2 and an event.
* Source building for the strength test: EXE takes the first building of A with the flag byte `+0xC9`, engine a random one.
* Result: with one base the chance that a Raid/Storm hits the base is 1/2 (two X-COM buildings), 2/3 with two bases. The engine data gives the same two buildings, so this part matches.

Verdict: diverges. Impact: low-medium. Fix: skip B == Aliens; make non-base X-COM targets a no-op.

### F12  Attacker count for organisation raids (match)

EXE (`FUN_000aedb0` file 0x111454, listing 0xAEDC6-0xAEDF7): `n = org.byte(+7) * (rand16(50) + 0x4B) / 100`, capped at 20; guard unit type by organisation (Megapol 10, Psyke/Diablo/Osiron 11, Cult 12, others 9); adds while total < 60. Engine `organisation.cpp:36-48`: identical formula with roll 0..50, cap 20. Match.
Sizes at Superhuman (organisations.xml `average_guards`): Cult 10 -> 7..12; Government 14 -> 10..17; Megapol 16 -> 12..20; Diablo 13 -> 9..16; Psyke 10 -> 7..12; Osiron 16 -> 12..20; most corporations 7..14 -> 5..17.

### F13  Base loss by collapse: EXE is a value threshold (>90 percent destroyed); engine is "last building-part tile gone" (diverges)

EXE (OBSERVED, `FUN_00070acc` file 0xD3170). Over the building's footprint `[x0..x1] x [y0..y1]` and layers 0..9: sum the byte at tile-type `+0x0C` (the tile `value`) for every scenery tile whose type byte `+0` is 0 (General). `dead = (10 * sum < word at building +0x0C)`. If dead, the 14 crew bytes at `+0xD4` are zeroed (aliens inside die) and 1 is returned. Callers that act on it for an X-COM base print group 0x18 item 0x24 and call `FUN_000b3114`: `FUN_00041644` (VA 0x41943/0x41956), `FUN_00054a28` (VA 0x56107/0x5611C), `FUN_0005a918` (VA 0x5AC9F/0x5ACA4). It is also consulted by `FUN_00070140`, `FUN_0006f5e4`, `FUN_000702e4`.

CONTROL: the word at building `+0x0C` (the field the extractor labels `unknown1[1]`, "people tube data populated in savegame") equals the sum of General-tile values of the intact map **exactly** for all 109 buildings of `CITYMAP5` and all 96 of `CITYMAP4` (computed from `CITYMAP.DAT` + `CITYMAP5` + `CITYMAP5.BLD`; ratio `M / (10*S)` = 0.100 for every building). Examples: building 0 M=2398, building 1 M=1024, building 4 M=3792. So the rule is: **a building (and an X-COM base) is destroyed when its remaining tile value falls below 10 percent of its original value.**

Engine. LIVE=HEAD `building.cpp:1086-1113`: on each tile loss, crew die if `crewQuarters` is no longer in `buildingParts`; the base dies when `buildingParts` is empty (`isAlive`, `:1165`, counts tiles with `isBuildingPart`); `isBuildingPart` is the extractor heuristic `value > 5` (`extract_city_scenery.cpp:222`). Whether the EXE has any crew-quarters rule is NOT established: I did not search for one (exe_unbound for that half). The only related string found, `Can't destroy: living quarters in use.`, is a base-screen message.

Verdict: diverges. Impact: high for any strategy that lets alien Attack/Bomber UFOs (weeks 4+) or crashes bombard the base building: the original loses the base at 90 percent of value, the engine only at 100 percent of "building-part" tiles. Fix: port the sum-of-values test using the same table (`SceneryTileType::value` for General tiles) and the original per-building baseline (extract BLD word `+0x0C` or recompute at load); keep the resident-crew clear on collapse; leave the crew-quarters rule alone until its EXE status is established. The caller set (when the test runs) is part of the port: after projectile hits, vehicle-scenery collisions, and the UFO attack planner.

### F14  Result of a base defence: win keeps the base and costs 50 per unit; loss or retreat destroys it (match + one missing cost)

EXE (OBSERVED, `FUN_000b32ac` file 0x115950, listing 0xB350B-0xB3552). Type 2 (`DAT_00244024 == 2`): if the mission record's success word `[0x244084] == 1`, `org0.funds -= 50 * [0x244090]` (computed as `worth*4; -worth; <<3; +worth; *2`) with no funds check; otherwise `FUN_000b3114(FUN_000705f8([0x244044]))`, i.e. the base is removed. Both fields are zeroed by `FUN_000aff9c` before the battle and so are written by the tactical process; TACP was not queried. TACP briefing text (TACP generation non-4, file 0x2E0931 and 0x2E1FC4): "You can retreat from the base to cut your losses, but the base will be lost." INFERRED: `worth` is the damage count measured by TACP; the 50 per unit matches the wiki's "$50 per damaged tile".

Engine. LIVE=HEAD `battle.cpp:3603-3620`: win -> `MissionCompletedBase`, no charge; loss -> `Base::die`. Retreat: `abortMission` -> `retreat` on every unit -> `playerWon = false` (`battle.cpp:2167-2181, 2233-2237`) -> `Base::die`. So retreat equals loss in both. The per-unit repair bill is missing.

Verdict: match for the outcome; engine_missing for the bill. Impact: medium (cash). blocked_on_re: the unit of `worth` needs the TACP writer of the shared mission record (`+0x70` of the record that starts at VA 0x244020 in UFO2P). Fix: after a won base defence, charge `50 * damagedTiles` once the unit is confirmed.

### F15  Infiltration target pool and distance metric (diverges, low-medium; belongs to the UFO-flow validator)

EXE (OBSERVED, `FUN_00070140` file 0xD27E4, listing 0x70167-0x701E7). Candidates are all buildings, any owner or function, alive (`FUN_00070acc == 0`), with `D <= 60` where `D = FUN_0005d288 = 2*max(|dx|,|dy|) + min(|dx|,|dy|)` between the building centre and the UFO. Choice: each candidate draws `rand16(100)`; highest draw wins (first index on ties): uniform over candidates. Exposure is not consulted. So a base is targeted by an Infiltration UFO with probability `1/(number of alive buildings within D <= 60)`, independent of exposure.
Engine: `vehiclemission.cpp:3275-3295`, Manhattan `<= 30` (`vehiclemission.h:21`). Same on axes (30), smaller on diagonals (the EXE includes dx=dy=20, the engine only 15).
INFERRED, medium: units of the UFO position words (`+0x30/+0x32`) are tiles.
Verdict: diverges (small). Impact: low. blocked_on_re: false.

### F16  What happens to the base's people and craft when it is removed (diverges, low-medium)

EXE (OBSERVED, `FUN_000b3114` file 0x1157B8): building owner := Government (2), `+0xC9 := 0`, base record `+0x15 := 0`, live-base counter decremented, the base's pending event rows cleared, then, if a base remains, every vehicle and every employed agent whose home is this base is **re-homed** to another base via `FUN_000b3270`. Game over when the counter reaches 0 (`FUN_00060ea4` -> `FUN_00060f4c`, string "All X-COM bases have been destroyed...").
Engine: `Base::die` (`base.cpp:87-167`): owner := Government, kills agents and vehicles that are inside, refunds cargo, GameLost at zero bases (match). There is no re-homing of agents or vehicles that are away (`homeBuilding` stays on a building that is no longer a base).
Verdict: engine_missing (re-homing). Impact: medium: with two bases, losing one keeps the squad and craft that were out. Fix: re-home away agents/vehicles to the first remaining base in `Base::die`.

### F17  Empty-base test (match, small edge)

EXE (`FUN_000b2e60` reason 5): any agent with `+0x21 == 0` located at the base (kind 1) or class-0 in a docked craft (kind 3) keeps the base from being auto-destroyed; unarmed staff count. Engine UFO path uses `building->currentAgents.empty()` (`vehiclemission.cpp:2644`), organisation path `target->currentAgents.empty()` (`organisation.cpp:1299, 1323`). Edge: agents inside a docked craft are not in `currentAgents`. Verdict: match with a minor edge. Impact: low.

### F18  Unfinished facilities in the defence map (diverges, low)

EXE (`FUN_000b38b4`, listing around 0xB3A30-0xB3AC9): a cell with construction days left is mapped to a construction sector (type 0x22, or 0x23 if the facility type is not flagged) rather than omitted. Engine `battlemap.cpp:969-972` skips it. Impact: low. Also the EXE counts only finished stations for turrets (F04).

### F19  Facility construction countdown and the daily order (match)

EXE: `FUN_0006bc70` decrements each cell's `+0x56` once per day at the day rollover and then runs the incident scheduler. Engine LIVE `gamestate.cpp:1565-1568` decrements `buildTime` once a day in the daily update. Match.

## 4. Strategy implications (INFERRED from the observed rules; tagged where it is arithmetic)

1. **Exposure.** A base becomes known to aliens only by (a) a Subversion UFO landing over it, (b) 3 or more aliens moving into the base building from a neighbour in one hourly step (probability `(15+3n)%` times `5n%` for the exposure roll), (c) a transfer after certain raids. It is never set by a defence battle. To avoid (b) keep the buildings within `|dx|<15, |dy|<15` of the base clear of infestations of 5+ aliens; clearing within roughly 14 tiles of the base building is the actionable radius. Exposure only matters for Subversion targeting: Infiltration UFOs pick buildings uniformly regardless.
2. **What attacks the base.** (a) A UFO landing, probability per Infiltration UFO about `1 / (alive buildings within D<=60)`; attackers 4 to 22. (b) A hostile organisation Raid/Storm; attackers 5 to 20. There is no spread-driven attack. The Cult of Sirius is hostile at the start at Superhuman, so organisation raids can begin in the first days. What can be stated: a Cult-to-X-COM trigger is a Raid 25 percent of the time, and a Raid reaches a base only when the random X-COM building is a base, so P(base attack | one Cult trigger) = 0.25 x 0.5 = 12.5 percent with one base plus the starting Recyclotorium (0.25 x 2/3 = 16.7 percent with two bases). How often triggers occur depends on the shared timer (72 after each trigger, minus 1 per failed hostile-pair check per day).
3. **Garrison size.** Defenders are everyone present, capped at `36 - min(12, 4*stations)`; unarmed staff take slots and die. Expected opposition: Cult 7..12, Government 10..17, Megapol 12..20, UFO 4..22. A garrison of 8 armed soldiers beats a Probe/Scout/Bomber/Fast Attack landing, is marginal against a Destroyer/Assault (17..22) without stations, and should be 10 to 12 armed soldiers with 2 to 3 stations for the larger UFOs and for Megapol raids. This is judgement, not an EXE number.
4. **Do not leave the base empty.** A base with no live agent present is deleted when the event fires. Techs alone keep it from auto-deletion but not from losing the fight.
5. **Retreat is not an option**: it loses the base. With two bases the loss re-homes your people.
6. **Bombardment is far more dangerous than the engine suggests**: the base is destroyed at 90 percent of tile value lost.
7. **Relations.** Keep every sponsor organisation above -50 (not at -50); the Cult starts hostile and raids early, so do not push it lower with raids for cash; consider reducing its chance by not leaving the base building as the only X-COM target (two X-COM buildings halve the chance per trigger).

## 5. Not done / open

* TACP was not queried: the turret spawn mechanism (does TACP create 12 turrets from the count passed by UFO2P, or from the base map's sectors), the `worth` writer, and the exact win condition are exe_unbound. Most promising lead: find the writer of the shared mission record fields at offsets `+0x64` (success) and `+0x70` (worth) of the record that begins at UFO2P VA 0x244020, in `TACP.EXE`.
* `FUN_000b3270` (home re-assignment) and the objects it chooses were not read.
* Codes 7, 8, 10 of the incident tables (non-base effects) were not decoded.
* `FUN_000702e4` (Subversion targeting) was taken from the repo docs; I only re-checked that it reads the exposure word.
* Alien species index order (egg 0, chrysalis 4, queenspawn 11) was inferred from the crew table contents, not from a name table.
* `FUN_000240a4` and `FUN_000a12f4` (the only `+0xAA` writers) were not read; the base-building function after purchase is therefore unverified (F07).
* The EXE path for an Alien Extermination of an X-COM-owned building (reason 2, `FUN_00060c18`) was not traced.
* Ghidra copy `v_base_defence` and the extracted ISO files are deleted at the end of the session.
