# v_research_rates: research, manufacturing, facility rates and caps (UFO2P.EXE vs OpenApoc)

Binary: `UFO2P.EXE`, ISO non-4 generation (CRC `0x4749ffc1`). Ghidra project copy `v_research_rates` (read-only, `-noanalysis`). Engine: LIVE snapshot (`live_src`), HEAD `fda36d6c` where it differs. Nothing was launched; no constant is invented.

Conventions. **OBSERVED** = decompile or listing text, a byte read from the canonical file, or an engine source line. **INFERRED** = my reading of observed facts. File offsets are physical: code `file = VA + 0x626A4`, data `file = VA + 0x616A4` (index section 3). Every function entry below was byte-checked against `OGR/canonical/UFO2P.EXE` (Watcom prologue `53 51 52 56 57 55` or the leaf start). Raw operand bytes in code are object-relative (`+0xD0000` in Ghidra), so the file bytes show e.g. `dc 0f 09 00` for Ghidra VA `0x160fdc`.

## 0. Results at a glance

| id | rule | verdict | strategy impact |
|---|---|---|---|
| RR-01 | research progress = sum of assigned scientists' skill per game hour | match | low (confirms the formula) |
| RR-02 | who counts as staff (at base, healthy) | diverges (health check missing) | low |
| RR-03 | lab staff cap 5 small / 10 large | match | medium (bounds per-lab speed) |
| RR-04 | large topics need a large lab | match | low |
| RR-05 | scientist/engineer skill is static | match | low |
| RR-06 | manufacturing progress, completions, craft slots, cost timing | diverges (minor) | low |
| RR-07 | facility build time, 1 per day at midnight, unusable until 0 | match | medium |
| RR-08 | repair bay rate (12 per bay) | diverges (per-vehicle cap) | low-medium |
| RR-09 | staff caps 100 / 50 / 50 / 50 and 180 slots | engine_missing | **high** |
| RR-10 | daily hire-pool arrivals | diverges | medium |
| RR-11 | per-base lab count cap "6 small + 4 large" | exe_unbound (no check found) | medium |
| RR-12 | unmanned (Probe/Scout) recovery unlock | diverges | **high** |
| RR-13 | manned recovery unlock needs a won mission | engine_bug | high |
| RR-14 | alien-building research chain: first topic gated by The Alien Dimension, per-building hours | diverges (order matches) | high |
| RR-15 | weekly UFO growth stops when the **Control Chamber** falls (mechanism high, building identity medium) | diverges | **high** |
| RR-16 | research score per topic | match | low |
| RR-17 | hourly work paused while the view is the alien dimension | exe_unbound (partial) | medium |

## 1. Method and controls

Scripts (kept in `validate/v_research_rates_work/`): `ScanOps.java` (every 32-bit value in a range, anywhere in `.object1`, with the owning instruction and function), `WriteScan.java` (every instruction that writes `[reg + disp]`), `Lst.java` (listing with file offsets), plus `DecompOnly.java`, `Callers.java` from the shared set. About 30 headless calls at 25 to 45 s each; outputs `scan*.txt`, `dec*.txt`, `lst*.txt`, `ws*.txt`, `cal*.txt` in the same folder.

Tables located by constant scan (research_data VA `0xDD7DC`, 28 B x 99; per-topic state VA `0xDE2B0`, 10 B x 99; manufacturing_data VA `0xDE690`, 50 B x 43; facility_data VA `0xDC0F0`, 16 B x 20). Layout of the 10-byte research state, OBSERVED from the readers/writers: `+0` unlock/forced-available flag, `+1` base index, `+2` lab cell index (`0xFF` none, `99` after completion), `+4` u32 progress (man-hours), `+8` complete flag.

Control arms:
- **Table decode.** EXE `research_data`: 99 records, 26 large, sum of skillHours `3,093,000`. Engine `research.xml` (non-manufacture, non-unlock): 124 rows, 26 large, sum `3,093,000`. EXE `manufacturing_data`: 42 manufacturable, 19 large, minimum 800. Engine: 42, 19, minimum 800. Skill ranges of the three technical hires (EXE `agent_type_data` base + inc: 60 + 40) equal engine `agent_types.xml` (60 to 100).
- **Writer scans are not blind.** `WriteScan` for `[reg+0x56]` finds the construction-byte writers; for `[reg+0xC6]` it finds the lab-assignment writers; the constant scan finds the two indexed UFO-type unlock writers and the three direct system-flag writers. Each expected positive was found.
- **Offsets.** Byte reads at `0x10D32C, 0x10DAE4, 0xCE314, 0xCF104, 0xDCB18, 0x10E730, 0x1156B0, 0x115950, 0x116BDC, 0xD1450, 0x10D2DC` all start at the function's first instructions.

### Clock structure (new, needed by several findings)

`FUN_0004ac40` (file `0xAD2E4`; decompile + listing `lst12.txt`) is the per-frame calendar step. OBSERVED:
- a tick accumulator `DAT_000d4d6e += DAT_000d4d58`; at `>= 0x24` (36) one game second passes (engine `TICKS_PER_SECOND` = 36 matches);
- per **minute**: `FUN_0004aa7c`; every 5th minute `FUN_0004ab04` (org incident dispatcher `FUN_00092470`, org founding, and the invasion countdown `DAT_000d5068 -= 0x2a30` = 10 800 ticks = 5 min x 60 x 36); every 30th minute `FUN_0006bdac` + `FUN_0006bf04` (facility census, healing/training accumulators);
- per **hour** (file `0xAD4E8..0xAD519`, gated `DAT_000d5060 == 0`): `FUN_0006f3a8`, `FUN_0006f7d4`, `FUN_00096bc8`, **`FUN_000aac88` research**, **`FUN_000ab440` manufacturing**, `FUN_000962cc` (org park refill), **`FUN_0006ca60` repair bays**, `FUN_0006fcc0` (infiltration), `FUN_00010380`;
- per **day** (hour wraps to 0, file `0xAD4A5..0xAD4B8`, gated `DAT_000d5060 == 0`): `FUN_0007a474` (hire pool), `FUN_0006fd6c` (infiltration history shift), `FUN_00097b90` (score rollover; it tests `weekday == 0` itself, file `0xFA23A`);
- per **Monday** (file `0xAD47B..0xAD4A0`): week counter `DAT_000e0e7c++`, `FUN_000941dc` (funding), **`FUN_0006edac` (UFO growth)**, `FUN_000967a4`;
- the facility construction countdown `FUN_0006bc70` is **not** in this function: it is called from `FUN_000105b8` (file `0x72EE6`) after `FUN_00010908`, the clock loop, returns because the weekday changed.

`DAT_000d5060` is the current city index (`FUN_0006e098` builds the map name `citymap` + `('A' + DAT_000d5060)`; vehicle and agent records carry the same byte at `+0x174` / `+0x7c`).

## 2. Findings

### RR-01 Research progress per hour (match)

Engine (LIVE = HEAD for these lines): `Lab::getTotalSkill` sums `current_stats.{biochem,physics}_skill` of assigned agents with `currentBuilding == homeBuilding` (`game/state/city/research.cpp:411-436`); `Lab::update(TICKS_PER_HOUR, ...)` is called once per game hour from `GameState::updateEndOfHour` (`gamestate.cpp:1547` LIVE, `:1514` HEAD) and converts skill to integer "progress hours" via `ticks_per_progress_hour = TICKS_PER_HOUR / skill` (`research.cpp:455`). Completion at `man_hours_progress >= man_hours`.

EXE, OBSERVED (`FUN_000aac88`, file `0x10D32C`, listing `lst1.txt`): for each of the 99 topics with `state+2 != 0xFF` and `state+8 == 0`, for each of the 180 agent slots that pass `FUN_000aac38` and whose base byte (`agent+0x24`) equals `state+1` and whose lab-cell word (`agent+0xC6`) equals `state+2`: `state.progress += byte(agent + 0x49)` when the topic's `researchGroup` byte (record `+0x18`) is 0 (BioChem), `+= byte(agent + 0x4A)` when it is 1 (Quantum Physics) (file `0x10D3D3-0x10D3F3`, then group 1 branch from `0x10D774`). After each addition `if (skillHours <= progress)` (`CMP EAX,[EBX+0xDD7F0]` at file `0x10D442`) the topic completes and progress is set to `skillHours`. Runs once per game hour.

So the EXE rule is the engine rule: man-hours per game hour = sum of assigned scientists' skill points. INFERRED: the engine's integer-tick conversion yields `129600 / (S * floor(129600 / S))` times the nominal rate; at S <= 1000 (a full large lab, 10 x 100) that is +0.47 % at most. Negligible.

**No difficulty factor.** The difficulty word (VA `0xE0C9C`, file `0x142340`; some readers load the dword at `0xE0C9A` and shift right 16) is read by 14 functions in a constant scan over `[0xE0C99, 0xE0CA0)` (`dif2.txt`), among them the hourly infiltration routine `FUN_0006fcc0` (`MOV EDI,[0xE0C9A]; SAR EDI,16` at file `0xD2387`, the control: it is the known reader behind `42 - difficulty`). `FUN_000aac88`, `FUN_000aac38`, `FUN_000ab440`, `FUN_0006ca60`, `FUN_0006bc70`, `FUN_0007a474` and `FUN_0006edac` are **not** among the readers. The `DAT_000d5054 == 4/5/6` compares inside the progress code (file `0x10D3FC`, `0x10DBB7`) only set the screen-redraw flag `DAT_00183a78` when the lab screen is open; they are not a rate term. So research, manufacturing, build time, repair, hiring caps and UFO growth are the same at every difficulty.

Strategy rule: "hours to finish a topic = skillHours / (staff x skill)"; usable as is, difficulty-independent.

### RR-02 Who counts as staff (diverges, low)

Engine: only `agent->currentBuilding == agent->homeBuilding` (`research.cpp:415-420`).

EXE, OBSERVED (`FUN_000aac38`, file `0x10D2DC`): `agent+0x7D == 1`, `agent+0x7C == DAT_000d5060` (current city), `agent+0x21 == 0` (employed by X-COM), `agent+0x23 != 0` (not a soldier), `word agent+0x7E == base_record(agent base).+2` (physically in the home base building), and **`agent+0x3C == agent+0x3B`**. INFERRED: `+0x3B/+0x3C` is max/current health: recruit creation `FUN_000791e0` writes both from one value (`MOV [ECX+0x3C],AL; MOV [ECX+0x3B],AL`), and the healing census `FUN_0006bdac`/`FUN_0006bf04` treats `+0x3C < +0x3B` as "hurt". So **an injured scientist or engineer contributes nothing** in the EXE; the engine lets them work.

Fix sketch: in `Lab::getTotalSkill` skip agents with `modified_stats.health < current_stats.health` (the same test `Base::getCapacityUsed` uses for the medical bay, `base.cpp:535`). Impact is low outside base-defence aftermath. blocked_on_re: false.

### RR-03 Staff cap per lab (match)

Engine: `assigned_agents.size() >= type->capacityAmount` (`game/ui/base/researchscreen.cpp:171-173`, HEAD identical); capacity 5 / 10 comes from `facility_data` (Lab/Advanced Lab).

EXE, OBSERVED (assignment click handler, file `0x10A028-0x10A321`): counts valid agents (`FUN_000aac38`) with base == selected base and `agent+0xC6 == selected cell`, then `CMP CX,5` (small, file `0x10A272`, bytes `66 83 f9 05`) or `CMP CX,0xA` (large, file `0x10A28D`, bytes `66 83 f9 0a`) and refuses with a message when full. The cap is a code immediate, not read from `facility_data`. The EXE counts only valid agents toward the cap; the engine counts all assigned.

Consequence: one large lab tops out at 10 x 100 = 1000 man-hours per hour, small 500.

### RR-04 Large-lab gate (match)

EXE, OBSERVED (`FUN_000a97a8`, file `0x10BE4C`): the topic list sets status 5 ("needs a larger facility") when `DAT_0012efed (selected lab is advanced) < research_data.labSize` and the topic is otherwise available; the same function's manufacturing branch compares against `manufacturing_data +0x30` (labSize, u16 at record +48) the same way. The topic is shown but not startable. Engine: the list shows every available topic (`researchselect.cpp:228-245` computes `too_large` but ignores it, FIXME: colour), and selecting one raises the message box "PROJECT TOO LARGE" and keeps the old selection (`researchselect.cpp:93-103`), plus a `LogAssert` in `Lab::setResearch` (`research.cpp:352`). So both show-and-refuse; no visibility divergence. Counts equal (control above).

### RR-05 Skill never grows (match)

OBSERVED: the only code that writes `agent+0x49 / 0x4A / 0x4B` is recruit creation `FUN_000791e0` (VA `0x79719 / 0x7972B / 0x7973D`, `base + rand(0..inc)` from `agent_type_data`); `WriteScan 49 4a 4b` finds no other register-relative writer, and a constant scan of the agent array's absolute skill addresses (`0x144961..0x144963`) shows five reads and no write. Soldiers' training writes other fields. Engine: skills are set once in `Agent` generation (`agent.cpp:140-145`) and read in `getSkill`; no growth. The hire roll equals `60 + rand(0..40)`.

Strategy rule: the skill you hire is the skill you keep; hire the top of the visible bar.

### RR-06 Manufacturing (diverges, low)

Engine (`research.cpp:441-520`): same sum-of-skill progress; after each hourly update **at most one** item completes (`manufacture_man_hours_invested -= man_hours`), stores-full halts via `ManufactureHalted`, per-unit cost is charged at start (`setResearch`) and after each completion when `balance >= cost`.

EXE, OBSERVED (`FUN_000ab440`, file `0x10DAE4`, listing `lst1.txt`): 100 job slots of 16 bytes at VA `0x141668` (`[0]` base, `[1]` cell, `[2]` topic, `+4` units left, `+8` done, `+0xC` progress). For each active slot, for each of the 180 agents passing `FUN_000aac38` with matching base/cell: `progress += byte(agent+0x4B)`; if `progress >= skillHours` the unit completes (`progress -= skillHours; done++; left--`) **inside the agent loop**, so up to one completion per assigned engineer per hour. For craft (`itemType == 3`) the add is refused unless `FUN_00034b78` finds **at least 10 free vehicle slots out of 80** and `FUN_0005d68c` returns a slot; otherwise the job waits. After a completion with units left: stop if `balance <= cost` or `FUN_000a9acc` (stores space for the item class) fails (file `0x10DE0E` is the call), else `balance -= cost`; the first unit's cost is charged at job start (`SUB [balance],[0xDE6B8 + idx*50]`, file `0x10870A-0x108715`). In `manufacturing_data` the three part-type bytes that `FUN_000aa204` tests (record `+0x12..+0x14`, read as `dword >> 24`) are `0xFF` for all 43 records, so the "enough parts" path (string `You do not have enough parts to make this item`) is unused. The rest of the padding is not all `0xFF`: records 31 and 32 (Dimension Missile Launcher, Dimension Missile) carry a u16 `55` at `+0x0E` next to `techRequired = 54` (research 54 = Dimension Missile Launcher, 55 = Dimension Missile, names at file `0x14E3BA`). That reads like a second tech requirement; the engine gates both items only on `RESEARCH_DIMENSION_MISSILE_LAUNCHER` (`research.xml`). Consumer of `+0x0E` not found (the constant-displacement scan has no hit at `VA 0xDE69E`; `FUN_000aa5e4` reads `+0x0C`). Lead: `FUN_000aa5e4` / `FUN_000aac0c`.

Differences: (a) throughput above one item per hour is possible in the EXE when staff skill sum exceeds the item's skillHours, i.e. only the five cheapest items (800 to 1600 man-hours: Toxin A/B/C, Alien Gas Grenade, AG missiles) with a staffed Advanced Workshop (10 x 60..100); the engine caps at 1 per hour and a backlog accumulates uselessly; (b) the 10-free-vehicle-slot rule for craft is absent; (c) the EXE halts at `balance <= cost`, the engine at `balance < cost`.

Fix sketch: in `Lab::update` allow `while (invested >= man_hours && quantity left)` completions, bounded by staff count. Low priority. blocked_on_re: false.

### RR-07 Facility build time (match)

Engine: `facility->buildTime = type->buildTime` (`base.cpp:335`), decremented in `GameState::updateEndOfDay` (`gamestate.cpp:1559-1572` LIVE), labs are listed only at `buildTime == 0` (`researchscreen.cpp:72-88`, `base.cpp:621`).

EXE, OBSERVED: placement (file `0xCAFE2-0xCB031` for 1x1, `0xCB08E-0xCB1D6` for 2x2) loads `facility_data.build_time` low byte (`MOV DL,[EBP+6]`, file `0xCAFE2`) and stores it in the per-cell construction byte (`[cell + 0x56]`, i.e. base record `+0x58 + cell`; a 2x2 stores it in all four cells and tags the other three cells `0xC9/0xCA/0xCB`, so the facility id sits in one cell only). `FUN_0006bc70` (file `0xCE314`) decrements every non-zero construction byte of all 16 bases x 64 cells by one (`DEC DH`, file `0xCE354`) and raises a "complete" message at zero; it is called once per game day at the weekday change. Everything that matters tests the byte for zero: the lab UI (`(&DAT_000d9484)[cell] == 0`), the facility census `FUN_0006bdac` (capacity totals), repair bays `FUN_0006cacc`, upkeep `FUN_00098a70` (index 5.1). A `DAT_000e0ca4` debug flag zeroes it.

Cadence and unit match: days, decrement at midnight. Lab 5 d, Advanced Lab 8 d, Workshop 6 d, Advanced Workshop 9 d, Living Quarters 3 d.

### RR-08 Vehicle repair (diverges, low-medium)

Engine (`City::repairVehicles`, `city.cpp:779-826` LIVE, `:759` HEAD, body identical; called hourly from `City::hourlyLoop`): repairPoints = sum of repair-bay capacity, where the extractor sets 12 per bay (`extract_facilities.cpp:37`, "raw capacity 1 -> 12"); `repairPerVehicle = max(1, points / vehicles)` with **no upper bound**, two distribution passes; only vehicles with `homeBuilding == currentBuilding`.

EXE, OBSERVED (`FUN_0006ca60` file `0xCF104`, `FUN_0006cacc` `0xCF170`, `FUN_0006cb18` `0xCF1BC`, `FUN_0006cb8c` `0xCF230`; raw bytes at `0xCF138`: `6b d2 0c ... 83 f8 0c 7e 05 b8 0c 00 00 00 85 c0 75 05 b8 01 00 00 00`): hourly, per base: `bays` = number of completed (construction byte 0) cells with facility id `0x19`; because a 2x2 facility has its id in one cell only (RR-07), `bays` = number of repair bays. `needing` = X-COM vehicles docked in that base with constitution below the type maximum. `rate = bays * 12 / needing` (integer), **capped at 12**, minimum 1; every needing vehicle gets `+rate` once, clamped to the maximum. No bays: no repair.

So the 12 per bay is EXE-grounded (the "extractor assumption" is confirmed), but the EXE also caps a single vehicle at 12 per hour; the engine lets one damaged craft absorb all bays' points (2 bays: 24 per hour vs EXE 12). The EXE repairs any X-COM vehicle docked at the base (the engine requires home == current).

Fix sketch: `repairPerVehicle = std::clamp(points / n, 1, 12)`, single pass, test `currentBuilding == base building`. blocked_on_re: false.

### RR-09 Staff caps (engine_missing, HIGH)

Engine: none. `recruitscreen.cpp:604-680` checks funds and living quarters only; `Organisation::updateHirableAgents` (`organisation.cpp:541-626`) has no ceiling.

EXE, OBSERVED (`FUN_0007a474`, file `0xDCB18`, daily): a 4-dword constant block copied from VA `0x78300` (file `0xDA9A4`, bytes `64 00 00 00 32 00 00 00 32 00 00 00 32 00 00 00` = **100, 50, 50, 50**) is indexed by agent class `agent+0x23` (0 agent, 1 biochemist, 2 mechanic/engineer, 3 quantum physicist; the classes match the lab classes in `FUN_000a97a8`). The function counts, per class, people who are employed (`+0x21 == 0`) **plus applicants** (`+0x21 != 0 && +0xB0 != 0`), and creates new applicants only while `count < limit`: `n = min(rand16(0..3), limit - count)`. The agent table has exactly **180 slots** (`0xB4` in every loop; stride `0xCE` from VA `0x144918`) and `FUN_000791e0` returns an out-of-range index (180) when no slot is free; every caller tests `< 0xB4`. So employed + pool is bounded by 100 / 50 / 50 / 50 and by 180 in total.

Strategy rule affected: research and manufacturing capacity ceilings. 50 biochemists = five fully staffed Advanced Biochem Labs; 50 physicists = five Advanced Quantum Labs; 50 engineers = five Advanced Workshops; soldiers 100; total 180 (100 + 50 + 50 + 50 = 250 > 180, so the slot count binds first when all four classes are large). At skill 60..100 (mean 80) the ceiling is about 4000 man-hours per hour per technical class. In the engine the roster is unbounded (recruit screen only needs funds + housing).

Fix sketch: in `Organisation::updateHirableAgents` stop spawning for a class when `(employed + hirable) >= cap`; in `recruitscreen.cpp` refuse hires beyond cap and beyond 180 total persons (staff + pool). Not found (see leads): the UFOpaedia "20 agents when no technical staff" and "auto-sack by ratio" rules. blocked_on_re: false for the caps; true for the two extra rules.

### RR-10 Daily hire pool (diverges, medium)

OBSERVED in the same function (listing `lst7.txt`): each day, per class, every applicant is first thinned: `removed = rand16(0 .. count/3 + 1)`; then `new = min(rand16(0..3), limit - (employed + remaining applicants))` applicants are created (origin organisation code `rand16(0..0x14) + 2`, not relation-weighted); agents get up to two extra rolls: `rand16(0..1)` Hybrids if the Mutant Alliance row of the relation matrix (`0x16EEC8`, = `[24][0]`) is `>= 0`, `rand16(0..1)` Androids if S.E.L.F. (`0x16EEAC`, `[23][0]`) is `>= 0`. Mean arrivals per technical class: 1.5 per day; pool equilibrium about 6.

Engine (`organisation.cpp:541-626`, `organisations.xml` hirableAgentTypes): per hiring organisation, `attempts` rolls with chance `25 + relation` (or `(50 + relation) / 2` below 0), nothing for hostile organisations; 33 % of every hirable agent leaves daily. INFERRED from the XML: Biochemists 10 attempts, Mechanics 10, Physicists 10 per day across organisations, so 2.5 to 4.5 arrivals per class per day at relation 0 to 20, roughly two to three times the EXE rate.

Impact: how fast a roster (RR-09) can be built. Fix sketch: replace by the EXE rule or at least cap the arrival mean at 1.5 per class per day. blocked_on_re: false (the random-draw argument order `FUN_0005d1d8(n)` = inclusive `[0,n]` is documented in index section 9).

### RR-11 Per-base lab count cap "6 small + 4 large" (exe_unbound)

UFOpaedia and guides state a maximum of six small and four large labs/workshops per base. Engine: none. EXE: **no check found**. Examined: the facility placement click handler (file `0xCA6A4-0xCB400`, `lst9.txt`, `lst6.txt`: only cell-validity compares `0xC8/0xC9/0xCA/0x10`), the demolish check `FUN_0006c724`, the facility info panel `FUN_0006c2bc`, the build-menu builder `FUN_00069084` (lists by availability flag `record+0xD` only), and the per-type census table (VA `0x13E250`, 8 B per type per base) whose readers are display and demolish code. Labs are keyed by their grid cell index `0..63`, so the physical bound is the 8x8 base. Global bounds that do exist: at most 99 simultaneous research projects (one per topic) and 100 manufacturing jobs (`0x64` loops).

Not proven absent (the click path before `0xCA6A4` and the grid-draw code were skimmed, not read in full). Next lead: base screen `FUN_0004f290` (reads the census table and the research state) and the lab-selector draw `FUN_000a7ed4`/`FUN_000a8634`; a control would be a known limit such as the 8x8 bound. Strategy: plan with the 8x8 grid as the only EXE-proven bound; treat 6+4 as folklore.

### RR-12 Unmanned UFO recovery (diverges, HIGH)

Engine: the extractor attaches `researchUnlock = {ALIEN_CRAFT_CONTROL_SYSTEMS, ALIEN_CRAFT_ENERGY_SOURCE, ALIEN_CRAFT_PROPULSION, UFO_TYPE_n}` to **all ten** UFO types (`tools/extractors/extract_vehicles.cpp:147-156`, `if (i < 10)`); types 0 (Probe) and 1 (Scout) have no `battle_map` (`:248-253`), so `UfoRecoveryBegin` takes the unmanned branch, force-completes the full list (`cityview.cpp:4743-4750` HEAD, `:4818-4825` LIVE) and `UfoRecoveryUnmanned` additionally hands the UFO's equipment over as cargo (`Vehicle::loadUnmannedUfoLoot`, `vehicle.cpp:1660`).

EXE, OBSERVED:
- `FUN_000b300c` (file `0x1156B0`): for mission kind 1 (UFO) reads the vehicle type word (`MOV DX,[EAX+0x160FDC]`, file `0x1156F1`) and returns **0 ("no mission needed") only when the type is 0 or 1**; every other type returns 1.
- `FUN_000ac08c` (file `0x10E730`, prompt for a downed craft): when kind == 1 and `FUN_000b300c == 0`, it shows a message (string group `0x18`, item `0x29`, plus the vehicle name), executes `MOV byte [(type + 0x4E)*10 + 0xDE2B0], 1` (file `0x10E82C`, bytes `c6 04 45 b0 e2 0d 00 01`) and calls `FUN_00058280` (remove the vehicle). The block (file `0x10E778-0x10E83F`) calls only `FUN_00063A00` (string resolver), `FUN_0002AE1C` (message box) and `FUN_00058280`. It does not write the three alien-system flags and never calls the cargo-add helper `FUN_000915A0`, which the manned success branch calls at file `0x115CC7`.
- The three alien-system unlock bytes are written in exactly one place: `FUN_000b32ac` case "UFO mission" with result 1 (RR-13): `MOV [0xDE5A8],AL`, `[0xDE5B2]`, `[0xDE59E]` at file `0x115C1B/0x115C20/0x115C25`. The addresses are `0xDE2B0 + 10 * {76, 77, 75}`, i.e. the research records `Alien Control System (76)`, `Alien Energy Source (77)`, `Alien Propulsion System (75)` (names read from the EXE name table: records 75 to 77). Those three records have combinator code 4 ("available only through the flag", all of 75..87 and 88..97 except 92).

Confidence is split. **System flags: high.** The three bytes have exactly one writer (`FUN_000b32ac`, manned path with result 1); the constant scan over the research-state table found the other two flag writers (`FUN_000ac08c` file `0x10E82C`, `FUN_000b32ac` file `0x115C55`, both indexed UFO-type writes) as positive controls, and no pointer-walk function in the scan writes the table. So in the original, recovering a Probe or Scout opens only its own UFO-type topic; opening the Advanced Quantum Lab / Dimension Probe chain needs a *manned* craft (types 3 to 10) recovered through a won ground mission. This matches UFOpaedia ("recover any manned UFO") and contradicts the engine hard-code. **No loot from the unmanned branch: medium.** The argument is only that the branch has no grant call; I did not read the arrival handler `FUN_0003A910` (which calls the prompt `FUN_000ac08c`) for grants made before the prompt.

Strategy impact: the engine's week-1 Probe/Scout recovery unlocks the three systems and starts the Dimension Probe chain 5 to 13 days earlier than the original (`web_research_tree.md` D-2, section 5.2).

Fix sketch: `extract_vehicles.cpp`: emit the three system unlocks only for `i >= 2`; keep `UFO_TYPE_{i+1}` for all. Remove or zero `loadUnmannedUfoLoot` (EXE: no loot). The gamestate is generated, so regenerate `base_gamestate` (index: extractor provenance matters) or patch the vehicle types after extraction. blocked_on_re: false.

### RR-13 Manned recovery unlock needs a won mission (engine_bug, confidence medium: the meaning of result 1 is inferred)

Engine: `Battle::exitBattle`, `case MissionType::UfoRecovery`: force-completes the UFO type's `researchUnlock` and kills the vehicle with **no `playerWon` test** (`battle.cpp:3639-3649` HEAD, `:3646-3656` LIVE), unlike `RaidAliens` and `BaseDefense` which test it.

EXE, OBSERVED (`FUN_000b32ac`, file `0x115950`; listing `lst11.txt`): `CMP [0x244024],3` (event type "UFO mission", set only by `FUN_000aff9c` when its mission kind is 1) then `CMP [0x244084],1` (file `0x115C06`) guards the three system flags, the UFO-type flag (`0x115C55`) and the loot loop; the vehicle removal `FUN_00058280` runs regardless. `0x244084` is zeroed at mission setup (`FUN_000aff9c`, VA `0xB0296`) and never written by UFO2P code, so it comes back from the tactical executable. INFERRED: `1` = X-COM won (the same field gates base survival in the base-defence case: result 1 keeps the base, otherwise `FUN_000b3114` deallocates it).

Impact: in the engine an aborted or lost recovery still unlocks the whole chain (a harness could abort at once); in the EXE it unlocks nothing.

Fix sketch: wrap the loop in `if (state.current_battle->playerWon)`; keep the vehicle kill outside. blocked_on_re: true for the exact meaning of "won" when the player retreats (TACP result byte not traced).

### RR-14 Alien-building research chain (diverges, order matches, confidence high)

Previously "unbound" (index section 7, `tech_tree.md` section 7 note). OBSERVED:
- Unlock writer `FUN_000b4538` (file `0x116BDC`): `switch(function_idx)` sets the unlock byte of a research record: `0 -> 95, 1 -> 93, 2 -> 94, 3 -> 97, 4 -> 90, 5 -> 91, 6 -> 96, 7 -> 89, 8 -> 88` (bytes `0xDE666, 0xDE652, 0xDE65C, 0xDE67A, 0xDE634, 0xDE63E, 0xDE670, 0xDE62A, 0xDE620`; function 9 sets nothing). It is called at file `0x115A62` after a **won** raid in the alien dimension (`CMP [0x244084],1`), with `function_idx = [0x244048]`.
- `FUN_000b4678` (file `0x116D1C`): a building's own access topic is record `0x58 + function_idx` (`state+8` complete flag); `FUN_000b45b8`: "already raided" = the flag that `FUN_000b4538` would set for it.
- Record 92 is the only one of records 88 to 97 whose combinator is not code 4: `All(tech 1 = The Alien Dimension)`. So the first alien-building topic is record 92 and needs The Alien Dimension; each later topic unlocks only by winning the previous raid.
- Following the map with building function = record - 88 gives the single chain `4 -> 2 -> 6 -> 8 -> 0 -> 7 -> 1 -> 5 -> 3 -> 9` in EXE name order (Incubator 0, Spawning 1, Food 2, Megapod 3, Sleeping 4, Organic Factory 5, Alien Farm 6, Control Chamber 7, Maintenance Factory 8, Dimension Gate Generator 9, names at file `0x14AD23`), i.e. **Sleeping, Food, Alien Farm, Maintenance, Incubator, Control Chamber, Spawning, Organic Factory, Megapod, Dimension Gate Generator**. That is exactly the engine's `albuild.bld` order (`tech_tree.md` section 7). `record = 88 + function` is stated directly by `FUN_000b4678`, is independently supported by the UFOpaedia binding (record 88 carries `ufopaediaGroup 8, entry 0`; the gap matrix reports that the EXE catalog binds page (8,0) to the **Incubator**, function 0), and the resulting ten-step chain visits every function exactly once with the research hours rising monotonically (30 000 to 48 000). The Dimension Gate Generator (function 9) additionally runs `FUN_00060f24` from the collapse routine `FUN_000b3d4c` (`if (word [+0xAA] == 9)`); that function ends the campaign loop (sets the loop-exit state `DAT_000d4cbc`), consistent with the docs' victory trigger but not decoded further.
- Hours of records 88 to 97 (`research_data`): 38 000, 42 000, 32 000, 46 000, **30 000 (92)**, 44 000, 34 000, 40 000, 36 000, 48 000. Along the chain they rise 30 000, 32 000, ..., 48 000.

Engine: `RESEARCH_ALIEN_BUILDING_i` (i = albuild order) gets the hours of record `88 + i`: Sleeping 38 000, Food 42 000, Farm 32 000, Maintenance 46 000, Incubator 30 000, Control 44 000, Spawning 34 000, Organic Factory 40 000, Megapod 36 000, DGG 48 000; the first topic has no prerequisite and is offered on day 0, and `THE_ALIEN_DIMENSION` gates the 5th (the engine inherited record 92's prerequisite onto `AB_4`). In the EXE the building raided at step k costs `30 000 + 2 000 k` and step 0 needs the dimension discovery.

Impact: pacing of the whole endgame (the first topic cannot start before a craft has crossed a gate; cumulative hours after k raids are `30,62,96,132,170,...` thousand instead of `38,80,112,158,188,...`); total 390 000 equal. `web_research_tree.md` D-1 is therefore EXE-supported.

Fix sketch (extractor, `extract_buildings.cpp:80-92`): set `accessTopic = RESEARCH_ALIEN_BUILDING_{function_idx}` (record `88 + function_idx`, which carries the right hours/score and the single `The Alien Dimension` prerequisite automatically) and take the unlock chain from the `FUN_000b4538` map instead of `i + 1`: winning function `f` completes the unlock of record `T(f)` with `T = {0:7, 1:5, 2:6, 3:9, 4:2, 5:3, 6:8, 7:1, 8:0}` (as `RESEARCH_UNLOCK_ALIEN_BUILDING_{T(f)}`, with no unlock topic for index 4); keep `victory = true` on function 9. This also moves the day-0 `startable` alien-building topic away (nothing is offered until The Alien Dimension is done). blocked_on_re: false. Confidence: high.

### RR-15 What stops weekly UFO growth (diverges, HIGH, medium confidence)

Engine: `GameState::updateUfoGrowth` returns early when the Organic Factory in `CITYMAP_ALIEN` is dead (`gamestate.cpp:1626` LIVE, `:1593` HEAD; `ufogrowth.cpp:51-70`); justified only by UFOpaedia prose (index section 6).

EXE, OBSERVED (`FUN_0006edac`, file `0xD1450`, listing `lst10.txt`; called every Monday right after funding, also once at new game from `FUN_000b94c0`): `if (byte [0xDE62A] == 0)` { `row = clamp(DAT_000e0e7c - 1, 0, 15)`; for each of 10 UFO types `count[type] = min(count[type] + word [0xF3980 + row*20 + type*2], word [0xF396C + type*2])` } (VA `0xF396C` = the `UFO_growth_rates` caps, `0xF3980` = the weekly add rows; the first valid consumer of those tables, closing index open question 4 for growth). The byte at `0xDE62A` is the unlock flag of research record 89, written only by `FUN_000b4538` case 7 (RR-14): **it becomes 1 when the raid on building function 7 is won**. With the name order of RR-14, function 7 is the **Control Chamber**. The same win sets `DAT_000f3c78 = 1` (file `0x115A53`, only for function 7) which is read by the UFO incursion code (`FUN_0006cfb4`, `FUN_0006d384`, `FUN_0006da88`), a lead for the "Control Chamber makes the aliens desperate" text in TACP at `0x2E1809`.

INFERRED: weekly UFO growth ends when the Control Chamber falls (6th raid on the engine/EXE chain), not when the Organic Factory (8th) falls. In the post-raid handler nothing keys on function 5 except its unlock flag (other code was not searched for function-5 tests). Caveat: the identity function 7 = Control Chamber rests on the extractor and the score table also using EXE name order; the apocalypse-flag coincidence supports it.

Impact: which building to prioritise to cap alien reinforcements; the Organic Factory raid in the engine is 2 steps later than the true cut-off. Fix sketch: `ufogrowth.cpp:craftFactoryIntact` should test `BUILDINGFUNCTION_CONTROL_CHAMBER`; do not model the `DAT_000f3c78` effect until its readers are decoded. blocked_on_re: true for the incursion effect, false for the growth gate.

### RR-16 Research completion score (match)

EXE, OBSERVED (`FUN_000aac88` file `0x10D5B0`): `FUN_000983dc(category 1, research_data.score)` where `score` is the u16 at record `+18` (`MOV DX,[EBP+0xDD7EE]` at file `0x10D59E`); category 1 is "Research Completed" in the funding score (index 5.2). Engine: `cityview.cpp:4924-4925` adds `topic->score`. Equal data (control above). Note the engine awards it in the UI event handler.

### RR-17 Hourly work and the alien-dimension view (exe_unbound, partial)

OBSERVED: the whole hourly block (including research, manufacturing, repair, infiltration) and the daily/weekly blocks are guarded by `DAT_000d5060 == 0` (current city = human city). `FUN_000105b8` iterates over cities, running the clock loop `FUN_00010908` for each city that holds an X-COM craft and then for the human city; on entering the dimension it randomises the clock fields (`FUN_0005d1d8` stores into `0xD4D66/0xD4D6A/0xD4D6C`). Engine: one global `gameTime`; `updateEndOfHour/Day/Week` always run for all cities (`gamestate.cpp:1416-1424`).

INFERRED (low-medium confidence): time spent with the view in the dimension does not advance the human-city calendar, labs, infiltration or funding in the original, whereas the engine advances all of them. Not decoded: how the dimension days interleave with human days and what the clock is restored to. Next lead: `FUN_000105b8` at file `0x72C5C-0x72F55` with `DAT_000e1320`, `FUN_0007ee84`, `FUN_0006e098`. blocked_on_re: true.

## 3. Leads outside the strict mechanic

- **Score-threshold flags.** `FUN_00097b90` (file `0xFA234`) evaluates nine flags (VA `0xE0B84`, daily) against a 9 x 5 table at VA `0xE0AD0` (file `0x142174`): rows 0 to 3 `[2000..16000]`-style thresholds against the **tactical-mission** total (category 0), rows 4 to 8 against the **UFO shot-down** total (category 3), column = difficulty. The extractor's `cequip_score_req_data` (file `0x1421C4`) and the engine (`vehicle.cpp:3883-3898`) model only rows 4 to 8 (items `0x2C..0x30` through `FUN_000b3e84`, also applied to recovered-UFO loot). Rows 0 to 3 are read by `FUN_000b3e24` (flags `0xE0B84..87`); consumer not traced.
- **Un-modelled hire rules.** "20 agents without technical staff" and "auto-sack by ratio": not in `FUN_0007a474`; check `FUN_00078310` (a cursor over the agent table, 5-minute tick) and the Hire/Fire screen handlers.
- **Items 0x2C..0x30 gating loot** (`FUN_000b32ac` loot loop) means a recovered Large Shield or Cloaking Field may not appear in the loot without the score flag; the engine gates creation of default UFO loadouts instead.

## 4. What I did not do

No game, no TACP query, no second-binary (`v_research_rates_2`) call. The Ghidra copy `copies/v_research_rates` is removed at the end.
