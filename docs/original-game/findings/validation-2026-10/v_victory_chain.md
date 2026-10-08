# v_victory_chain: alien-dimension chain, crossing, UFO growth gate, victory (UFO2P.EXE vs OpenApoc)

Binary: `UFO2P.EXE`, ISO non-4 generation (CRC 0x4749ffc1), Ghidra project OpenApocOG (private copy `v_victory_chain`, read-only, `-noanalysis`).
Conversion used everywhere: code (object1) physical file offset = Ghidra VA + 0x626A4; data (object2) = VA + 0x616A4. Every code address below was cross-checked by the file offset printed next to each listing line in `validate/v_victory_chain_work/call*.txt` (script `VChain.java`, preserved in `validate/v_victory_chain_work/VChain.java`). Raw per-call outputs are in `validate/v_victory_chain_work/`.
Engine citations: `LIVE:<path>:<line>` = frozen live snapshot, `HEAD:` = committed fda36d6c where it differs. Only `battle.cpp`, `vehicle.cpp`, `vehiclemission.cpp`, `gamestate.cpp`, `buildingscreen.cpp`, `cityview.cpp` differ between LIVE and HEAD (line shifts, plus the LIVE-only cat-7 score block); none of the differences touch the rules compared here.

Convention: OBSERVED = bytes / decompile / listing text read from the EXE or the engine source. INFERRED = my reading. CONTROL = a positive test that the same instrument finds a known thing.

## 0. Bottom line (ranked by effect on the strategy)

1. D-1 is confirmed at code level. The EXE gates alien building `f` (f = word at building record +0xAA = index in the alien-building name table) on research record **88 + f** (`FUN_000b4678`, `FUN_0004a33c`). The engine uses 88 + file position. Chain order is identical in both (both run Sleeping, Food, Farm, Maintenance, Incubator, Control, Spawning, Organic, Megapod, Gate), so only the *hours / prerequisite / score attached to each step* are wrong in the engine. Totals are equal (390,000 mh). The first raid is the part that matters: EXE = "The Alien Dimension" (6,000, needs a first crossing) then Sleeping Chamber 30,000; engine = 38,000, no prerequisite, researchable on day 0.
2. UFO growth in the EXE stops when the **Control Chamber** (building #6, f=7) is destroyed, not the Organic Factory (#8, f=5). The only guard read is byte 0xDE62A, written only when f=7 is destroyed. Engine gates on the Organic Factory.
3. Destroying the Control Chamber also arms a **one-shot mass assault** in the EXE (flag 0xF3C78). The next invasion-timer expiry (24-72 h) in the human city assembles up to 20 UFOs (types 1-9, round robin) from the alien-dimension stock, needs at least 10, and launches them instead of the normal incursion. The docs list this as "closed negative"; it is not. Engine has nothing.
4. Victory is exactly "destroy the building with function index 9 (Dimension Gate Generator)". No all-buildings test. Engine matches.
5. Crossing capability is `vehicle_data +0x1A (top_speed) != 0` (function `FUN_00060e80`), not a dedicated flag and not equipment. The set is the same 15 types as the engine's `dimension_travel` set (10 alien craft, Dimension Probe, Biotrans, Explorer, Retaliator, Annihilator), by data coincidence. The Dimension Shifter equipment exists in the EXE tables but is read only by two UI description builders.
6. Extra defeat condition missing from the engine: X-COM balance below -5,000 after the Monday payroll ends the game ("bad debts"). Engine raises GameLost only when the last base dies.
7. Not resolved: timing of the return from the dimension (no timer found in the arrival path), see F13.

## 1. Method and controls

- Ghidra headless via `ghq.sh v_victory_chain UFO2P VChain.java <commands>`; 10 calls (~8 s each). VChain.java commands: `kw:` (resolve string-pool group/item for a text and list CALL FUN_00063a00 sites with immediate EAX/EDX), `dec:` (decompile + listing with physical offsets), `lst:`, `xr:` (xrefs by address range), `disp:` (instructions with a given displacement), `callers:`, `bytes:`, `ptrscan:`.
- String-pool resolution: `FUN_00063a00` (file 0xC60A4) is `ptr = table[group]` at VA 0xF3794 (128 slots), then skip `item` NUL-terminated strings; listing shown in call1. CONTROL: the same `kw:` query finds the known score label `Alien Buildings Destroyed` = group 0x25 item 0x9B with its reader `FUN_00097c50` (call1, CALLSITE 000981b0), which matches the index's score-screen function.
- Absence claims carry a positive control inside the same query (section F7, F9).
- Static analysis only; the game was not launched.

## 2. EXE data read directly from `canonical/UFO2P.EXE` (OBSERVED)

research_data @ file 0x13EE80, 28-byte records, fields per `tools/extractors/common/research.h`:

| rec | file offset | labSize/u1/u2 | prereqTech | score | hours | group | ufopaedia (grp,entry) |
|---|---|---|---|---|---|---|---|
| 0 Dimension Gates | 0x13EE80 | 0/0/0 | none | 50 | 5,000 | Quantum | (8,10) |
| 1 The Alien Dimension | 0x13EE9C | 0/0/**4** | none | 50 | 6,000 | Quantum | (8,11) |
| 88 | 0x13F820 | 0/0/4 | none | 380 | 38,000 | BioChem | (8,0) |
| 89 | 0x13F83C | 0/0/4 | none | 420 | 42,000 | BioChem | (8,1) |
| 90 | 0x13F858 | 0/0/4 | none | 320 | 32,000 | BioChem | (8,2) |
| 91 | 0x13F874 | 0/0/4 | none | 460 | 46,000 | BioChem | (8,3) |
| **92** | 0x13F890 | 0/0/**0** | **[1]** (= The Alien Dimension) | 300 | 30,000 | BioChem | (8,4) |
| 93 | 0x13F8AC | 0/0/4 | none | 440 | 44,000 | BioChem | (8,5) |
| 94 | 0x13F8C8 | 0/0/4 | none | 340 | 34,000 | BioChem | (8,6) |
| 95 | 0x13F8E4 | 0/0/4 | none | 400 | 40,000 | BioChem | (8,7) |
| 96 | 0x13F900 | 0/0/4 | none | 360 | 36,000 | BioChem | (8,8) |
| 97 | 0x13F91C | 0/0/4 | none | 480 | 48,000 | BioChem | (8,9) |

Record 88+k carries ufopaedia entry (8,k), i.e. the entry index is the alien-building name-table index. Alien-building name table @ file 0x14AD23: 0 Incubator Chamber, 1 Spawning Chamber, 2 Food Chamber, 3 Megapod Chamber, 4 Sleeping Chamber, 5 Organic Factory, 6 Alien Farm, 7 Control Chamber, 8 Maintenance Factory, 9 Dimension Gate Generator. Per-building destroy score table @ file 0x191B7C = (400,400,300,500,300,250,250,350,300,500) indexed by the same f (matches the loader read at VA 0xB3334).

Research runtime state: 10-byte record per topic, base VA 0xDE2B0 (file 0x13F954): byte +0 "unlocked" (event-set availability), byte +8 "completed". Record n unlock byte = 0xDE2B0 + 10n; completed byte = 0xDE2B8 + 10n.

Alien campaign order from the engine's generated GAMESTATE `buildings.xml` (accessTopic index = albuild.bld file position): Sleeping(f4), Food(f2), Farm(f6), Maintenance(f8), Incubator(f0), Control(f7), Spawning(f1), Organic(f5), Megapod(f3), Gate(f9). `function` column of the same file confirms f per building.

## 3. F1 victory trigger (open Q5)

OBSERVED chain, all UFO2P non-4:

- Text: `THE ALIENS ARE DEFEATED` file 0x153F74 = pool group 0x2C item 0x12 (VA 0xF28D0), resolved at call site VA 0x612B7 (file 0xC395B) inside `FUN_00061214` (entry file 0xC38B8), the victory screen. Victory text item 0x16 at 0xC3A6D. `X-COM IS DEFEATED` = group 0x2C item 0x11, resolved in `FUN_00060f4c` (entry file 0xC35F0).
- `FUN_00061214` has one caller: `FUN_00060f24` (file 0xC35C8, call at VA 0x60F2A). `FUN_00060f24` has one caller: `FUN_000b3d4c` at VA 0xB3DE8 (file 0x11648C).
- `FUN_000b3d4c(building_index)` (entry file 0x1163F0): loops the building footprint, calls `FUN_0007f634` per tile and zeroes the tile word (the collapse), then `CMP word ptr [EDI+0xAA],9` at file 0x116482 / `JNZ` / `CALL FUN_00060f24` at file 0x11648C. Decompiled: `if (*(short *)((int)piVar3 + 0xaa) == 9) FUN_00060f24();`.
- `FUN_000b3d4c` has one caller: `FUN_000b32ac` at VA 0xB330B (file 0x1159AF), the battle-result handler (entry file 0x115950). Guards, in order: `[0x244024]==1` (file 0x115972) or (`[0xD5060]!=0` and `[0x244024]==4`) (file 0x115977-0x115987); `[0xD5060]!=0` (alien dimension, file 0x11598D); `[0x244084]==1` (INFERRED to be the battle-success flag: same word is tested for the base-defence result and written by the launch/return code `FUN_000aff9c` at file 0x11293A; its producer on the TACP side was not read, file 0x11599A). In the launch setup (`FUN_000aff9c`, file 0x1126A5-0x112755) `[0x244044]` = building index and `[0x244048]` = `building+0xAA >> 16` = the building's function index; mission type is set to 2 for EDX==5, 4 for EDX==3, 1 otherwise, 3 for EDX==1.
- `FUN_00061214`'s final-score loop (VA 0x61260-0x61278) sums categories 0-6, week + previous; category 7 (Alien Buildings Destroyed) is excluded from the number shown. This is the "residual score-array operand hit" at file 0xC3907 that the index listed as an unexplained consumer; its twin at file 0xC3642 is the defeat screen `FUN_00060f4c`. Neither is a gameplay rule.

So the EXE victory requires (the success reading of flag 1 is INFERRED): a successful (flag 1) type-1/4 tactical result in the alien dimension on the building whose function index is 9. It does not test the other nine buildings. The Gate Generator can only be entered after record 97 is complete and record 97's unlock byte is set by destroying f=3 (Megapod), so the whole chain is required in practice.

Engine: `building->victory` is set only on the albuild file-position-9 building (`LIVE:tools/extractors/extract_buildings.cpp:90`, HEAD same), which is the Gate Generator (f=9) per GAMESTATE. `Battle::finishBattle`, `RaidAliens` branch: `LIVE:game/state/battle/battle.cpp:3571-3600` (HEAD 3571-3593), `victory = building->victory` at LIVE:3588 (HEAD:3581), `GameWon` at LIVE:3659-3661 (HEAD 3652-3654). Verdict: match.

## 4. F2 / F3 alien-building research binding, first-raid gating, unlock chain

OBSERVED code:

- Access test (shown to the player as "Our Agents are unable to find an entrance ... Scientists back at HQ must complete their research", group 0x18 item 0x2B, call at VA 0x4B5BD): `FUN_000b4678(building)` (file 0x116D1C). Decompiled:
  `if (DAT_000d5060 != 0 && research_state[((*(int *)(DAT_0018276c + 0xa8 + idx*0xe2) >> 16) + 0x58) * 10 + 0xDE2B8] == 0) return 1 /*blocked*/`. The 0x58 = 88 is the first "Alien building" record, `>>16` of the dword at +0xA8 is the word at +0xAA = function index. CONTROL: stride 0xE2 = 226 = `BldFileEntry` size, +0xAA = `function_idx` per `tools/extractors/common/building.h`.
- Raidable test used by the auto-router, `FUN_0004a33c(building)` (file 0xAC9E0): jump table at VA 0x4A314 (file 0xAC9B8) in order f=0..9. For f, it returns 1 iff the "destroyed marker" A(f) is zero and the completed byte of record 88+f is non-zero (f=9 has no A test, only record 97). Handler pairs (A = unlock byte address, B = completed byte address): f0 A=0xDE666 B=0xDE628; f1 A=0xDE652 B=0xDE632; f2 A=0xDE65C B=0xDE63C; f3 A=0xDE67A B=0xDE646; f4 A=0xDE634 B=0xDE650; f5 A=0xDE63E B=0xDE65A; f6 A=0xDE670 B=0xDE664; f7 A=0xDE62A B=0xDE66E; f8 A=0xDE620 B=0xDE678; f9 B=0xDE682. Each B is exactly `0xDE2B8 + 10*(88+f)`.
- Unlock writer `FUN_000b4538(n)` (file 0x116BDC), called at VA 0xB33BE (file 0x115A62) with `n = [0x244048]` = function index of the building just destroyed (the loader store is at file 0x11274F). Jump table VA 0xB4510 (file 0x116BB4) in order n=0..9, each handler `MOV byte [addr],1`: n0 -> 0xDE666 (rec 95), n1 -> 0xDE652 (93), n2 -> 0xDE65C (94), n3 -> 0xDE67A (97), n4 -> 0xDE634 (90), n5 -> 0xDE63E (91), n6 -> 0xDE670 (96), n7 -> 0xDE62A (89), n8 -> 0xDE620 (88), n9 -> no write. So destroying f unlocks record 88+succ(f) with succ = {4:2, 2:6, 6:8, 8:0, 0:7, 7:1, 1:5, 5:3, 3:9}.
- The "destroyed" test `FUN_000b45b8` (file 0x116C5C) reads the same permutation by the building's function index (jump table VA 0xB4590, file 0x116C34): returns 0 once A(f) is set; f=9 always returns 1.
- Availability of a topic, `FUN_000aa7a8` (file 0x10CE4C): unlocked byte at `0xDE2AE + idx*10 + 2` forces availability; u2==4 ("case 4") zeroes the prerequisite accumulator, so records 1, 88-91, 93-97 are available iff their unlock byte is set. Record 92 (u2==0, prereqTech[0]=1) is available once record 1 is completed. Records 88-97 are only unlocked by `FUN_000b4538` (xref on 0xDE620..0xDE67A shows exactly nine WRITE sites, all in `FUN_000b4538`); record 92 has no unlock writer.
- The Alien Dimension (record 1, unlock byte 0xDE2BA): written at VA 0x6E2D6 (file 0xD097A) inside `FUN_0006e098` (file 0xD073C, the city-switch routine; executed when the loaded city index is non-zero) and at VA 0x3AAC9 (file 0x9D16D) inside the gate-arrival handler `FUN_0003a910` when the stored destination city is 0 and `[0xE0C9E]==0` (that word has no code writer, only save/load and the score screen). Either way the first move into the alien city makes the topic available.

INFERRED but strongly supported: the EXE order of play is Sleeping(f4) -> Food(f2) -> Farm(f6) -> Maintenance(f8) -> Incubator(f0) -> Control(f7) -> Spawning(f1) -> Organic(f5) -> Megapod(f3) -> Gate(f9) because that is `succ`, nine of nine transitions agree with the albuild file order used by the engine (GAMESTATE `buildings.xml` accessTopic indices 0-9). Cross-check by briefings: n==2 (Food Chamber, captive Sectoids) and n==7 (Control Chamber, "more desperate") are the two special cases in `FUN_000b32ac`, which is what the briefings describe.

Engine: `LIVE:tools/extractors/extract_buildings.cpp:82` `accessTopic = RESEARCH_ALIEN_BUILDING_{i}` with i = file position, `:83-87` `researchUnlock = RESEARCH_UNLOCK_ALIEN_BUILDING_{i+1}`, `:76,:94` name and crew by `entry.function_idx`. Raid access: `LIVE:game/ui/city/buildingscreen.cpp:137` (HEAD:198) refuses unless `accessTopic->isComplete()`. Unlock on win: `LIVE:battle.cpp:3577-3580` forceComplete over `building->researchUnlock`. Hours/dependencies from the extractor plus `data/common_patch/gamestate/research.xml:1411-1480`; merged GAMESTATE shows `RESEARCH_ALIEN_BUILDING_0` with no `<dependencies>` and `RESEARCH_ALIEN_BUILDING_4` depending on `RESEARCH_THE_ALIEN_DIMENSION` and `RESEARCH_UNLOCK_ALIEN_BUILDING_4` (the patch `op="delete"` is a no-op for list members, per web_research_tree D-9; I did not re-verify the serializer).

### Quantified effect (arithmetic on the observed hours)

| pos | building | f | EXE record | EXE mh | engine topic | engine mh | engine - EXE | cum EXE | cum engine |
|---|---|---|---|---|---|---|---|---|---|
| 1 | Sleeping | 4 | 92 | 30,000 (needs The Alien Dimension) | AB_0 | 38,000 (no prereq) | +8,000 | 30,000 | 38,000 |
| 2 | Food | 2 | 90 | 32,000 | AB_1 | 42,000 | +10,000 | 62,000 | 80,000 |
| 3 | Farm | 6 | 94 | 34,000 | AB_2 | 32,000 | -2,000 | 96,000 | 112,000 |
| 4 | Maintenance | 8 | 96 | 36,000 | AB_3 | 46,000 | +10,000 | 132,000 | 158,000 |
| 5 | Incubator | 0 | 88 | 38,000 | AB_4 | 30,000 (+ The Alien Dimension) | -8,000 | 170,000 | 188,000 |
| 6 | Control | 7 | 95 | 40,000 | AB_5 | 44,000 | +4,000 | 210,000 | 232,000 |
| 7 | Spawning | 1 | 89 | 42,000 | AB_6 | 34,000 | -8,000 | 252,000 | 266,000 |
| 8 | Organic | 5 | 93 | 44,000 | AB_7 | 40,000 | -4,000 | 296,000 | 306,000 |
| 9 | Megapod | 3 | 91 | 46,000 | AB_8 | 36,000 | -10,000 | 342,000 | 342,000 |
| 10 | Gate | 9 | 97 | 48,000 | AB_9 | 48,000 | 0 | 390,000 | 390,000 |

- Per-step lab wait at 900 mh/h: EXE 33.3, 35.6, 37.8, 40.0, 42.2, 44.4, 46.7, 48.9, 51.1, 53.3 h (monotone); engine 42.2, 46.7, 35.6, 51.1, 33.3, 48.9, 37.8, 44.4, 40.0, 53.3 h. At 450 mh/h double.
- Maximum cumulative lead the engine adds to the player's research debt: +26,000 mh after building 4 (29 h at 900 mh/h); it unwinds to 0 by building 9.
- First raid: EXE needs, after the first crossing, The Alien Dimension (6,000, Quantum) then record 92 (30,000, BioChem), serial = 36,000 mh = 40 h at 900 mh/h or 80 h at 450 mh/h. Engine: 38,000 mh with no prerequisite, so it can finish before any craft can cross. The engine therefore removes 40-80 h from the post-crossing critical path.
- Research score awarded per topic differs by position the same way (EXE rec 92 = 300 vs engine AB_0 = 380); the ten scores sum to 3,900 either way.
- UFOpaedia pages attached to the topics: EXE entry (8,k) belongs to the building with name-table index k; the engine's patch re-points names to campaign order (cosmetic).

## 5. F4 crossing capability (per-craft flag)

OBSERVED:

- `FUN_00060e80` (file 0xC3524): `return *(short *)(&DAT_00128602 + type*0x7e) != 0` which is `vehicle_data[type] + 0x1A` (VA 0x1285E8 base; +0x1A is `top_speed` in `tools/extractors/common/vehicle.h`). Used at VA 0x3A9CE (file 0x9D072) in the gate-arrival handler `FUN_0003a910` (file 0x9CFB4): a vehicle exactly on a gate tile with the gate-order state (X-COM: state byte `+0x1610A4+3 == 2`) crosses only if the predicate is true; if it is false the loop simply continues (`JZ 0x0003ab21`): no crash, no relocation. For alien craft the same handler runs with `owner==1`.
- vehicle_data +0x16, +0x18 and +0x1A, read from file 0x189C8C + 126*i: types 0-9 and 20-24 are non-zero in all three fields, every other type (including Valkyrie 18 and Hawk 19) is zero in all three. Values +0x16/+0x18/+0x1A: Probe 8/8/14, Scout 2/2/12, Transporter 2/2/8, FastAttack 5/5/20, Destroyer 3/3/12, Assault 2/2/8, Bomber 3/3/14, Escort 4/4/16, Battleship 1/1/12, Mothership 1/1/8, Dimension Probe 6/6/19, Biotrans 3/3/15, Explorer 4/4/16, Retaliator 5/5/15, Annihilator 7/7/18. `+0x16 == +0x18` in all 33 records.
- Runtime copy (`FUN_0005d6e4`, file 0xBFD88): vehicle `+0xFC = vd+0x16`, `+0xFE = vd+0x18`, `+0x100 = vd+0x1A`; the stat recompute `FUN_00077160` (file 0xD9804) then writes `+0xFE := +0xFC` (acceleration from engine power / weight, clamped 1..10) and `+0x100` from the engine. The per-frame vehicle update `FUN_000395e4` (file 0x9BF72-0x9BFD8, listing in call4) uses `+0xFC` to raise the current speed (`+0x6E += accel * dt / 2`) and `+0xFE` in the braking branch (`speed -= decel * ...` against a stopping-distance test), capped by `+0x102`. So `+0x18` is the braking rate, equal to the acceleration in every record, and not a gate flag. CONTROL: the same `disp:fe` instrument found these readers.

Engine: `LIVE:tools/extractors/extract_vehicles.cpp:337` `canEnterDimensionGate = (dimension_travel != 0)` with `dimension_travel` = struct +0x18; `LIVE:game/state/city/vehicle.cpp:3613-3631` `hasDimensionShifter()`; gate use at `LIVE:game/state/city/vehiclemission.cpp:1380-1445`. The engine set equals the EXE set for all 33 types. Verdict: match; the field label is wrong but harmless. Web claim "per-craft EXE flag" is true in effect, false in mechanism.

### F5 Dimension Shifter equipment

OBSERVED: vehicle equipment record 48 "Dimension Shifter" exists (file 0x18B1C8, general record 12 at 0x18B64E, `dimension_shifting` field at 0x18B65E = 100; research record exists as `RESEARCH_DIMENSION_SHIFTER`, hidden/cut). Direct xrefs over general-record offsets (`xr:129ED2:12`): +0x0, 0x2, 0x4, 0x6, 0x8, 0xA, 0xC, 0xE all have readers; +0x10 (VA 0x129EE2) has none by direct address. Base-register readers of the table: `FUN_0007679c` (VA 0x76D8C, `CMP word ptr [ECX+0x10],0`, file 0xD9430) and `FUN_0008a7c0` (VA 0x8B130, file 0xED7D4); both build equipment description lines (strings from group 0x25 via `FUN_00063a00`; the sibling teleporting field +0xE is handled in the same function). CONTROL: shielding (+0xA) readers include the stat recompute `FUN_00077160`, so the instrument sees gameplay readers when they exist. Verdict for the equipment branch of `hasDimensionShifter`: engine superset, no EXE gameplay use.

### F6 behaviour of an incapable craft at a gate

EXE: nothing happens (F4). Engine: `OpenApoc.NewFeature.CrashingDimensionGate` defaults to **true** (`LIVE:framework/options.cpp:473-476`) and `LIVE:vehiclemission.cpp:1384-1393` crashes the craft (`v.crash`) or parks it. Invented behaviour; harmless unless the harness sends a Valkyrie/Hawk to a gate.

## 6. F9 UFO growth gate and F10 Control Chamber mass assault

### Growth gate (OBSERVED)

`FUN_0006edac` (file 0xD1450), weekly growth. First instruction: `CMP byte ptr [0x000DE62A],0` (VA 0x6EDB1, file 0xD1455) / `JNZ` to the epilogue. Otherwise: week = `[0xE0E7C] - 1` clamped to 0..15; for each of the 10 UFO types `count += week_table[week][type]` (weekly rows at VA 0xF3980 = file 0x155024, 20 bytes per row), then clamp to the cap table at VA 0xF396C (file 0x155010); together these are the extracted `UFO_growth_rates` table. Callers: calendar `FUN_0004ac40` (VA 0x4ADF7), `FUN_0004ac24`, and new-game `FUN_000b94c0` (VA 0xB95F5), so week-1 growth is also applied at game start. 0xDE62A is the unlock byte of record 89, written only by `FUN_000b4538` case n=7 = destroyed building f=7 = **Control Chamber**. xref evidence: reads of 0xDE62A are `FUN_0006edac` (growth), `FUN_0004a33c`, `FUN_000b45b8`. Reads of 0xDE63E (the Organic Factory's destroyed marker, f=5) are `FUN_0004a33c` and `FUN_000b45b8` only. CONTROL: xref of 0xF396C/0xF3980 returns exactly `FUN_0006edac`; the known writer 0xDE62A in `FUN_000b4538` is found by the same query.

Engine: `LIVE:game/state/gamestate.cpp:1617-1630` `updateUfoGrowth` returns early unless `UFOGrowth::craftFactoryIntact` (`LIVE:game/state/rules/city/ufogrowth.cpp:51-71`), which tests `BUILDINGFUNCTION_ORGANIC_FACTORY` alive and fails open if none exists. Called at game start too (`LIVE:gamestate.cpp:714`).

Effect: the EXE stops growth after building #6 (Control), the engine after #8 (Organic). INFERRED: at the pace in the web research (about 6.4 days per building, LP data) growth stops about 13 days earlier in the original, and the two buildings in between (Spawning, Organic) are raided against a fleet that is no longer growing.

### Mass assault (OBSERVED)

- Arm: `FUN_000b32ac` at VA 0xB33A6 (file 0x115A4A): `CMP [0x244048],7 / JNZ / MOV dword [0xF3C78],1` (file 0x115A53). Only non-zero writer in the executable code (other writers of 0xF3C78 write EBX/EBP = 0: `FUN_0006d384` file 0xCFBDD, `FUN_00070938` file 0xD3068).
- Consume, `FUN_0006cfb4` (file 0xCF658, incursion scheduler; also the consumer of `UFO_mission_patterns` at 0xF3AC0, `slot = rand16(9)` at file 0xCF69B) invoked with EAX=0 at invasion-timer expiry (`FUN_0006d384` VA 0x6D3C6-0x6D3C8, file 0xCFA6A): `if (F3C78 != 0 && param == 0) { FUN_00070938(); if (F3C78 != 0) { [0xF3C7C]=1; [0xD505C]=1 /*incursion running*/; skip the normal incursion } }`.
- `FUN_00070938` (file 0xD2FDC): builds table 0x1A44A4[1..9] by round robin, one unit at a time, up to the stock `0x143D40[type]` (alien-dimension stock) with a total cap of 20 (0x14); if fewer than 10 were allocated, `F3C78 := 0` (no assault).
- `FUN_0006d384` alternate branch (VA 0x6D44A..0x6D540, file 0xCFAEE..): when `F3C78 != 0 && [0xD5060]==0` it spawns one UFO per step of the lowest-numbered type with a non-zero allocation, decrements the allocation, increments the city's per-type count, and clears F3C78 when the allocation is exhausted.
- `FUN_0006da88` (spawn, file 0xD012C): for a UFO spawned in mass mode, the health-withdraw percent (`+0x168`) is not applied, and the order bytes are set to `+0x16E=0x32`, `+0x170=2`, `+0x171=0x14`, `+0x168=-1` (same as role 4).

Not decoded: what those UFOs target; whether `0x143D40` stock is reduced for assault craft (no write seen in `FUN_00070938`, `FUN_0006d384`); exact meaning of role 2 / counter 0x14 in this mode.

Engine: none. `grep CONTROL_CHAMBER|Control Chamber|apocalypse` over `game/` and `tools/` returns only the UI welcome string. The Control Chamber is an ordinary building.

## 7. F11 Food Chamber and the Mutant Alliance (side finding, overturns a docs "closed negative")

OBSERVED in `FUN_000b32ac` (same success branch), VA 0xB3345-0xB33A0 (file 0x1159E9-0x115A44): when `[0x244048]==2` (Food Chamber): `org0[+0x2921]=0`; `rel[24][0] := max(rel[24][0],100)` (byte at 0x16EEC8; 0x16EC28 + 24*28); `rel[0][24] := that value` (0x16EC40); `FUN_0005faf0(24, 1, -50)` then `rel[24][1] := -100` (0x16EEC9); `[0x1409C8]:=0`. Org 24 = Mutant Alliance per the name table order in the index. So a successful Food Chamber raid makes the Mutant Alliance allied with X-COM and hostile to the Aliens; the doc claim that no battle result reaches the org relation matrix is wrong for this one site (the earlier census counted `FUN_000b32ac` as a relation writer but did not tie it to a battle result). Engine: nothing (grep above).

## 8. F12-F15 other observations

- F12 arrival auto-routing: `FUN_0006eea4` (file 0xD1548) places an arriving vehicle; in the alien dimension (`[0xD5060]!=0`) it then loops the city's buildings in array order (`[0x183A68]` count, `IMUL 0xE2`, VA 0x6F15A-0x6F17F, file 0xD17FE-0xD1823), calls `FUN_0004a33c`, and orders the craft to the first accessible building (`+0xCF=0`, `+0xD0=idx`); with none it patrols a random point near the gate (`+0xCF=2`). Engine: none (web_alien_campaign notes GH-761 says the same; the harness must issue the order).
- F13 transit/return timing: not resolved. OBSERVED: crossing sets vehicle `+0x174=0xFF` (city byte) and `+0x172=dest` in `FUN_0003a910`; `FUN_000105b8` (main loop, file 0x72C5C) turns 0xFF into 0xFE at the end of an iteration, and `FUN_0006d384` (VA 0x6D544-0x6D7C3) places vehicles whose city byte is 0xFE and `+0x172` equals the current city via `FUN_0006eea4`, with no countdown in any of these. `FUN_0006e098` (city switch) reloads the city and tops up UFOs to the per-type stock `DAT_00143A6C + city*0x2D4`; `FUN_000105b8` removes alien vehicles of a city that holds no X-COM craft (`FUN_00058280` loop). The UFOpaedia/GH-753 claims "arrive at midnight" and "at least 24 h" are not visible in these functions. Engine: switch at day change (`LIVE:game/ui/tileview/cityview.cpp:2846-2891`), return delay 1-2 h (`LIVE:vehiclemission.cpp:1423`), entry delay 1-2 s. Most promising next lead: `FUN_00010908` (VA 0x10908, file 0x72FAC, frame driver called from `FUN_000105b8`, decompiled in call9 but not read for time gating) and `FUN_000114cc` (calendar caller of `FUN_0003a910`).
- F14 bad-debts defeat: `FUN_00060ea4(1)` (file 0xC3548) is called at the very end of the weekly funding function `FUN_000941dc` (VA 0x94A52, file 0xF70F6), after salaries/maintenance and the balance commit (file 0xF70DD). It tests `*(int*)(org0 + 8) < -5000` (`CMP dword ptr [EAX+8],0xFFFFEC78` at file 0xC3557) and shows `X-COM IS DEFEATED` with the "Due to bad debts" text (group 0x2C item 0x14). Engine: no such check (`LIVE:game/state/city/base.cpp:155-159` is the only GameLost source; no balance test in `gamestate.cpp`).
- F15 base loss defeat: `FUN_00060ea4(0)` called from `FUN_000b3114` (VA 0xB3178, file 0x11581C) after a base slot is freed: defeat when none of the 16 base slots (stride 0x2BE, flag byte at 0xD9443) remains. Engine equivalent: `base.cpp:155-159` (last base). Match.

### F16 the dimension fleet is regenerated at full health on every entry (OBSERVED structure, partial)

- `FUN_0006e098` (file 0xD073C), after loading the destination city, for each of the 10 UFO types takes the stored count `word [0x143A6C + city*0x2D4 + type*2]`, subtracts the live vehicles of that type already in the city (vehicle slot array VA 0x160FD8, stride 0x276, city byte `+0x174`, type word `+0x2`), and spawns the remainder with `FUN_0006de64` (VA 0x6E4F9 onward in the decompile; `uVar8` loop). Spawned vehicles are freshly initialised by `FUN_0005d6e4`.
- `FUN_000105b8` (main loop, file 0x72C5C), in the branch for a city index other than 0 that holds no X-COM vehicle, calls `FUN_00058280` for every alien-owned (`+0x12C == 1`) vehicle in that city, i.e. an unvisited dimension holds no vehicle records, only counts.
- Engine: alien-city vehicles persist as ordinary `Vehicle` objects while the human city is current, and GH-1317 (web_alien_campaign 6.3) reports damage persisting; `updateUfoGrowth` spawns real vehicles into `CITYMAP_ALIEN` (`LIVE:game/state/gamestate.cpp:1617-1700`).
- Not observed: whether killing a UFO in the dimension decrements the stored count (and so permanently shrinks the stock). That decides whether "clear the dimension before the Control Chamber falls" works; it is the most useful follow-up for F10.

## 9. Engine file:line index

| topic | LIVE | HEAD (if different) |
|---|---|---|
| alien building -> record binding | `tools/extractors/extract_buildings.cpp:76,82-90,94` | same |
| EXE record naming for ten "Alien building" rows | `tools/extractors/extract_research.cpp:80-90` | same |
| patch hours/deps | `data/common_patch/gamestate/research.xml:1411-1480` | same |
| victory/unlock/score on raid win | `game/state/battle/battle.cpp:3571-3600, 3659-3661` | `3571-3593, 3652-3654` |
| raid access | `game/ui/city/buildingscreen.cpp:137` | `:198` |
| setCurrentCity unlock | `game/state/gamestate.cpp:331-337` | `:330-336` |
| crossing predicate | `game/state/city/vehicle.cpp:3613-3631`, `tools/extractors/extract_vehicles.cpp:337` | `vehicle.cpp:3558-3576` |
| gate use / crash / delays | `game/state/city/vehiclemission.cpp:1375-1445` | `:1372-1442` |
| day-change dimension switch | `game/ui/tileview/cityview.cpp:2846-2891` | `:2771-2816` |
| UFO growth gate | `game/state/gamestate.cpp:1617-1630`, `game/state/rules/city/ufogrowth.cpp:51-71` | `gamestate.cpp:1584-1597` |
| GameLost | `game/state/city/base.cpp:155-159` | same |

## 10. Fix sketches

- F2/F3 (D-1): in `extract_buildings.cpp:82` bind `accessTopic = RESEARCH_ALIEN_BUILDING_{entry.function_idx}`; at `:83-87` set `researchUnlock` to the topic of the *next file-position building's* function index (`RESEARCH_UNLOCK_ALIEN_BUILDING_{f_{i+1}}`, last building victory only); re-key the hidden `UNLOCK_ALIEN_BUILDING_*` topics and the `ALIEN_BUILDING_*` dependencies to function index in `extract_research.cpp`/patch: record 92 (`ALIEN_BUILDING_4`) depends only on `RESEARCH_THE_ALIEN_DIMENSION` and has no unlock flag; all other nine depend on `RESEARCH_UNLOCK_ALIEN_BUILDING_{f}` where that flag is force-completed by destroying the predecessor (f -> succ(f) above). Do it as one change (web_research_tree item 17 reaches the same conclusion); update `tests/test_city_rules.cpp:1278-1329` (HEAD only; the test file is not in the live snapshot). Check after: introspection `topic RESEARCH_ALIEN_BUILDING_4` (the Sleeping Chamber's record after the re-key is `_4`) must report unsatisfied until `RESEARCH_THE_ALIEN_DIMENSION` completes.
- F9: change `craftFactoryIntact` to test `BUILDINGFUNCTION_CONTROL_CHAMBER` (keep the fail-open for a missing building).
- F10: add `GameState::massAssaultArmed` set when a RaidAliens win has `building->function == BUILDINGFUNCTION_CONTROL_CHAMBER`; at the invasion roll, if armed and (alien-city UFO count of types except Probe, at most 20, round robin) >= 10, launch them as one incursion and clear the flag; if fewer than 10, clear it. Needs the mission and targets (blocked_on_re).
- F11: on a successful Food Chamber raid set Mutant Alliance to org relations 100 / 100 (X-COM) and -100 (Aliens).
- F14: after `weeklyPlayerUpdate` payroll, raise GameLost when `player->balance < -5000`.
- F6: set `OpenApoc.NewFeature.CrashingDimensionGate` to false for the benchmark (the original does nothing).
- F12: auto-order a craft arriving in the alien dimension to the first building for which the raid test passes (engine array order = albuild order).

## 11. Open questions left

1. F13 transit timing (above).
2. Targets/behaviour of mass-assault UFOs and whether they deplete the dimension stock.
3. How `FUN_00010908` / `FUN_000105b8` decide which city is simulated when X-COM craft are in both cities, and whether the human city is paused (web claim: yes).
4. Whether the tactical side (TACP.EXE) reports success for the Gate Generator on all-generators-destroyed or on all-aliens-dead: only UFO2P's consumer of the flag was read.
5. The Dimension Gates research (record 0): no direct-address reader found, none in the crossing test; whether it gates gate visibility in the UI was not examined (low confidence "not required").
