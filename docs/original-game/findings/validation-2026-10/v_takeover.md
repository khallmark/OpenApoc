# v_takeover: alien takeover of organisations, EXE vs OpenApoc

Binary: `UFO2P.EXE` ISO non-4 (CRC `0x4749ffc1`) is the authority; the Pentium `UFO2P4.EXE` was byte-checked for the roll only.
Engine: LIVE snapshot (`live_src`, HEAD `fda36d6c` + uncommitted). `organisation.cpp`, `organisation.h`, `bribescreen.cpp`, `gameevent.cpp` are byte-identical LIVE vs HEAD. `gamestate.cpp` and `building.cpp` differ (LIVE has the funding rewrite), so both line numbers are given.

Citation rule: binary + generation + PHYSICAL FILE OFFSET, then "VA" = Ghidra program VA I used. Code: file = VA + `0x626A4`; data: file = VA + `0x616A4` (exe_evidence_index §3). Every function entry quoted was prologue-checked (`53 51 52 56 57 55`). BSS addresses (no file backing) are given as VA only.

OBSERVED = bytes / decompile / code lines read. INFERRED = my reading, marked.

---

## 0. Bottom line

1. **The EXE takeover roll has a hard threshold the engine lacks.** `FUN_0006fd9c` rolls `rand16(100) < infiltration - 100` (inclusive range 0..100). With the raw infiltration value `v` in 0..200 (UI shows `v/2`), nothing can happen until `v > 100` (UI above 50%). Engine: `rand[0,200) < v`, i.e. any `v > 0` has a chance (`organisation.cpp:86`, LIVE = HEAD).
2. **The EXE cadence is also slower.** One organisation is tested per 5 game-minutes from a 60-slot rotation (counter 2..61, only slots 2..26 are real orgs), so each org is tested **once per 300 game-minutes**. Engine: once per 125 min per org (`organisation.h:19`). The engine comment "original checked one org every 5 minutes, which resulted in 125 minutes per check" is wrong: it assumed a 25-slot cycle; the cycle is 60 slots.
3. Net effect (Monte Carlo, section 6, INFERRED from the two rules plus the engine's growth formula which matches the EXE): for a sustained Government infestation of +2 raw/hour (about 10 anthropods in the Senate), median time to takeover is **19 h in the engine vs about 87 h (3.6 days) in the EXE**; for +1/h it is 34 h vs about 236 h (10 days); at +7/h (a 27-alien deposit in the Senate) it is 9 h vs 25 h. Infestations below the +1/h floor (hourly numerator < 38) never reach takeover in either. OBSERVED facts that carry the argument: (a) raw infiltration <= 100 (UI <= 50%) has takeover probability exactly 0 in the EXE; (b) the benchmark saves hold a non-taken-over Government at raw v = 1-14 ("imminent takeovers" in failure_forensics §3), which the EXE would leave untouched for days; (c) a heavy deposit can still flip the Government within about a day in the EXE, so this is a delay and a visible warning, not immunity. Do not read the benchmark's day 7-20 takeovers as impossible in the original; read them as roughly 3-7x too early for typical infestations.
4. Everything else about takeover matches: effects on relations (-100/-100 with X-COM, +100/+100 with Aliens, +90/+90 with other taken-over orgs), infiltration pinned at 200 every hour, **no reversal path in the live code**, bribe refusal, Government included (it is the FIRST slot of the rotation), funding cutoff via relation < -50 on the next Monday.
5. Engine misses: taken-over orgs get **+3 tier** in the EXE's building-guard composition (`FUN_000b46c4`); the relation primitive freezes taken-over/Cult-of-Sirius opinions of the Aliens; the detection halving is an `if/else-if` in the EXE (engine stacks three halvings).
6. No difficulty term anywhere in the roll. Difficulty enters via the infiltration divisor `42 - d` (38 at Superhuman, engine matches), starting relations (`FUN_00099130`, engine matches), detection weight `-2d`.

---

## 1. Method and tooling (what was actually done)

- **Offline**: Python + capstone (from the uv cache) reading `OGR/canonical/UFO2P.EXE` read-only. Helper scripts in `.../scratchpad/validate/tools/` (`xdis.py` disassembler with VA/file annotation, `calls.py` E8 call-site scan, `scan.py` / `orgflag*.py` operand scans, `pool*.py` string-pool decode, `sim.py` Monte Carlo).
- **Ghidra** (private copy `v_takeover`, read-only, no analysis): 8 headless calls, each 8-15 s (the "2 min per call" estimate in the brief only applies to the first project copy). Custom script `VT.java` (kept in the copy dir; copied to `validate/tools/VT.java`) with modes `dec` (decompile+callers), `ptr` (relocated-pointer scan of Ghidra memory), `ref`, `refrange`, `r11`/`w11` (all instructions touching a `+0x11` byte), `pool` (string pool group/item), `asm`, `str`, `rd`. Raw outputs: `validate/ghq_batch2..8.txt`.
- **Controls** used:
  - string pool decode: `pool:9:2` resolves to "Government" and `pool:22:78/79` to the ALIEN TAKEOVER title/body in Ghidra memory, matching my offline decode of the pool table at file `0x154E38`.
  - "no inbound reference" claims: the same E8/E9/Jcc/relocated-pointer scans find ~20 callers of the live `FUN_0007ad3c` and the jump table at VA `0x4A784` for target `0x4A896`.
  - flag-writer scan: the Ghidra `w11` scan finds the known writer at VA `0x6FE3B` (positive control) plus the init and the dev editor.
- **Caveat found on the way (affects all later agents)**: code pointers in jump tables are ZERO in the file (relocated by the loader), so raw-file pointer scans for function addresses are invalid; use Ghidra `ptr:`. Ghidra also has **no function** over the bribe screen (VA `0x97234`-`0x97600`) or over the "unit-kill island" (VA `0x7ABF0`-`0x7AE5F`), so Ghidra-only xref sets miss readers there; I used raw operand scans seeded on `imul ..,0x1b6` and `mov eax,[0xAFB4C]` for those.

## 2. Data layout recovered (OBSERVED unless marked)

| Item | Where | Evidence |
|---|---|---|
| Org index order | 0 X-COM, 1 Alien, 2 Government, 3 Megapol, 4 Cult of Sirius, ... 26 Technocrats, 27 Civilian | `pool:9:2` = "Government" (Ghidra memory); names group 9 at file `0x14AF11` |
| Runtime org array | pointer at VA `0x17FB4C` (BSS), record stride `0x1B6`, built from `organisation_data` (file `0x141468`, 18-byte records) by `FUN_00014da0` (VA `0x14DA0`, file `0x77444`) | stores `[rec+0]`,`[+2]`,`[+6]`,`[+7]`,`[+8]`,`[+0xC]`,`[+0x10]` from the table and `mov byte [rec+0x11],0` at VA `0x15016` (file `0x776BA`) |
| **Taken-over flag** | org record byte **`+0x11`**; 0/1 | written 1 by the roll (VA `0x6FE3B`, file `0xD24DF`); read by the functions in section 4; the developer ORGANISATION DATA editor labels this exact field "**like alien 1%i**" (string at file `0x133258`, editor `FUN_00021ddc` VA `0x21DDC`, file `0x84480`, reads `[ebp+0x11]` at VA `0x22124`) |
| Other record fields (editor labels) | `+0` "Type", `+6` "Level", `+7` "Security", `+8` "balance", `+0xC` "external income", `+0x10` "rebuildrate" | editor strings at file `0x1331DC`-`0x133258` (INFERRED mapping by order) |
| Infiltration array | dwords at VA `0x13F4C8 + org*0xE0` (BSS); row = 56 dwords: `[0]` current, `[1..55]` daily history (shifted by `FUN_0006fd6c`, file `0xD2410`) | hourly routine, takeover, UI (`FUN_0008903c` prints `raw/2`: VA `0x8945A`-`0x89467`, `sar eax,1`) |
| Relation matrix | VA `0x16EC28`, byte `[holder*28 + target]`, clamp +-100; second copy at `+0x760` | `FUN_00099130` stores both (VA `0x99211`, `0x99217`) |
| Alien-dimension flag (INFERRED) | byte VA `0xD5060` (file `0x136704`); every 5-minute / hourly handler returns early if non-zero; written by dimension-gate init `FUN_0006cdd0`, `FUN_0006e098`, new-game init (cleared) | `ref:d5060` writers; role inferred |
| Takeover rotation counter | dword VA `0xF3DB4` (file `0x155458`, static init **2**) | only writes: VA `0x70122` (`inc`) and `0x7012D` (`mov ...,2`), all in `FUN_0006fd9c` |
| Hostile / allied predicates | `FUN_00091db0` (file `0xF4454`) true iff `M <= (signed byte at VA 0x1277E4 = file 0x188E88 = 0xCE = -50)`; `FUN_00091de4` (file `0xF4488`) true iff `M >= (VA 0x1277E5 = file 0x188E89 = 0x4B = +75)` | bytes read |
| Week counter | dword VA `0xE0E7C` (file `0x142520`), set to 1 at new game (VA `0xB95CC`), +1 on each Monday assessment (`FUN_0004ac40`) | used by tech tier and UFO pattern row |

---

## 3. T1-T2: the takeover roll (`FUN_0006fd9c`)

**Location.** `FUN_0006fd9c`: VA `0x6FD9C`, physical **file `0xD2440`** (first bytes `53 51 52 56 57 55`), size 930. Single caller: `FUN_0004ab04` (file `0xAD1A8`) at VA `0x4AB31` (file `0xAD1D5`), which itself is called from `FUN_0004aa7c` (file `0xAD120`) at VA `0x4AACB` when `minute % 5 == 0`, and `FUN_0004aa7c` is called by the clock `FUN_0004ac40` (file `0xAD2E4`) at VA `0x4AE82` with `eax = [0xD4D6A]` (the minutes word) after every minute rollover (VA `0x4AE7A`-`0x4AE82`, file `0xAD51E`). Clock layout (VA): `0xD4D6E` sub-second ticks (36 = 1 s), `0xD4D6C` seconds, `0xD4D6A` minutes, `0xD4D68` hours, `0xD4D66` day-of-month, `0xD4D64` weekday.

Decompile (Ghidra, Watcom registers confused but equivalent to my capstone read), trimmed to the decision:

```
if (DAT_000d5060 == 0) {
  if (*(char*)(orgs + 0x11 + C*0x1b6) == 0 && C < 0x1b) {        // C = DAT_000f3db4
     uVar6 = FUN_0005d1d8(100);                                   // rand16 inclusive [0,100]; edx preserved = infil[C]-100
     if (rand < infil[C] - 100) {                                 // asm: cmp eax,edx ; jge skip   (file 0xD248F..0xD24A3)
        infil[C] = 200;  flag[C] = 1;  <relation writes>;  <modal dialog>
     }
  }
  C = C + 1;  if (C > 0x3d) C = 2;                                // file 0xD27BF..0xD27D5
}
```

Assembly receipts (UFO2P non-4): bytes `b8 64 00 00 00 83 ea 64 e8 .. .. .. ..` (mov eax,100 / sub edx,100 / call rand16) at **file `0xD248F`**; `25 ff ff 00 00 39 d0 0f 8d ..` (and eax,0xffff / cmp eax,edx / jge skip) immediately after. In `UFO2P4.EXE` the identical sequence is at **file `0xD2303`** (flag store `c6 44 02 11 01` at `0xD235B` vs `0xD24DF` non-4), so the rule is the same in both generations.

`FUN_0005d1d8` (file `0xBF87C`) returns `(mix) % (n+1)`: inclusive `[0, n]` (the `inc` before `idiv` at file `0xBF8F3`; `n` clamped to >= 0). Hence P(success) = `(v-100)/101` for `100 < v <= 200`, 0 for `v <= 100`.

**T1 results (per attempt / per day, `v` raw):**

| v (UI %) | engine p/attempt (125 min) | engine P/day (11.52 att.) | EXE p/attempt (300 min) | EXE P/day (4.80 att.) |
|---|---|---|---|---|
| 10 (5%) | 0.050 | 0.446 | 0 | 0 |
| 20 (10%) | 0.100 | 0.703 | 0 | 0 |
| 40 (20%) | 0.200 | 0.924 | 0 | 0 |
| 100 (50%) | 0.500 | 1.000 | 0 | 0 |
| 101 | 0.505 | 1.000 | 0.010 | 0.047 |
| 120 (60%) | 0.600 | 1.000 | 0.198 | 0.653 |
| 150 (75%) | 0.750 | 1.000 | 0.495 | 0.962 |
| 200 | 1.000 | 1.000 | 0.990 | 1.000 |

**T2 cadence / eligibility (OBSERVED + arithmetic).**
- One call per 5-minute tick; slot counter `C` runs 2..61 (60 values); only `C < 27` does anything, so orgs **2..26 (Government .. Technocrats)** are tested, once each per 60 ticks = **300 game-minutes**; X-COM (0), Aliens (1) and Civilian (27) are never tested.
- Government is slot 2: the first roll of a new game (counter static-initialised to 2) is the Government's, at the first 5-minute tick.
- `C` and the whole routine are skipped while `[0xD5060] != 0` (INFERRED: while the player is in the alien dimension), so no rolls and no counter advance there.
- A taken-over slot is skipped (flag test before the roll); the slot still consumes its tick.
- Orgs with org-speed 0 (S.E.L.F.) keep `v = 0` and can never be rolled successfully (table at file `0x1424B0`).
- Engine: `Organisation::updateTakeOver` accumulates ticks and calls `takeOver()` every `TICKS_PER_TAKEOVER_ATTEMPT` = 125 min per org with a random initial phase (`gamestate.cpp:610-611` HEAD / `611-612` LIVE, `organisation.cpp:715-722`), driven from `updateEndOfFiveMinutes` (`gamestate.cpp:1423-1436` HEAD / `1456-1470` LIVE, which also `break`s after the first takeover per tick, immaterial). It is not suppressed in the alien dimension.

## 4. Effects of takeover (T3-T5), all OBSERVED in `FUN_0006fd9c`

Matrix write receipts (VA / file), `M[holder][target]`, `i` = taken-over org:

| Write | VA | file | value |
|---|---|---|---|
| `infil[i] = 200` | `0x6FE29` | `0xD24CD` | 0xC8 |
| `flag[i] = 1` | `0x6FE3B` | `0xD24DF` | 1 |
| `M[Alien][i]` (`[ebx+0x9EC44]`) | `0x6FE42` | `0xD24E6` | +100 |
| `M[i][Alien]` (`[esi+0x9EC29]`) | `0x6FE4A` | `0xD24EE` | +100 |
| `M[i][X-COM]` (`[esi+0x9EC28]`) | `0x6FE52` | `0xD24F6` | -100 (0x9C) |
| `M[X-COM][i]` (`[ebx+0x9EC28]`) | `0x6FE5A` | `0xD24FE` | -100 |
| loop `e = 0..26`, `e != i`, `flag[e] != 0`: `M[i][e]`,`M[e][i]` | `0x6FE62`-`0x6FE91` | `0xD2506`-`0xD2535` | +90 (0x5A) |

- All relation writes are absolute stores of the new value (not deltas); no ripple. Aliens' own flag is 0 at start (`mov byte [rec+0x11],0` for all 27 orgs, VA `0x15016`), so the +90 loop never overwrites the Alien entries. Engine `Organisation::takeOver` (`organisation.cpp:84-108`): `establishRelation` -100 both ways with the player, +100 both ways with Aliens, +90 both ways with other taken-over orgs: **match**.
- After the writes the EXE opens a **modal** "ALIEN TAKEOVER" screen: title = group 22 item 78 (call VA `0x6FF28`, file `0xD25CC`, string file `0x14D331`), organisation name = group 9 item `C` (VA `0x6FF8E`, file `0xD2632`), body = group 22 item 79 (VA `0x6FFBB`, file `0xD265F`, string file `0x14D340`), then waits for a click (loop on `[0x1A1138]`, VA `0x70078`-`0x700ED`). The engine pushes a never-pausing toast (`gameevent.cpp:349-364`, `cityview.cpp:4707-4720`). UI-level only.
- **Funding consequence (T5).** `M[Government][X-COM]` = -100 < -50 satisfies the Monday cutoff test (`FUN_000941dc`, file `0xF6E9C`, per exe_evidence_index §5.1), which sets income 0 and the permanent latch. Engine HEAD `gamestate.cpp:1735-1741` and LIVE `1792-1799` both cut funding when `government->isRelatedTo(player) == Hostile` (`x < -50`): **match**. Only open point: the cutoff reads the relation on Monday; the EXE relation primitive still allows later positive deltas toward X-COM from a taken-over org (T13), so the relation can in principle drift above -50 before the next Monday only through such deltas; I found no mechanism producing 50 points in a week.

**T4 permanence / reversibility (OBSERVED).**
- Hourly `FUN_0006fcc0` (file `0xD2364`, flag test `cmp byte [esi+eax+0x11],1` at file `0xD2374`, `mov [ebx+0x6F4C8],0xC8` at file `0xD237B`) re-pins `infil = 200` for a flagged org every hour. Engine `organisation.cpp:639-642` same.
- Writers of the flag byte, from the Ghidra `w11` scan and raw scans over every `imul ..,0x1B6` / `mov eax,[0xAFB4C]` neighbourhood: (a) init `0` (VA `0x15016`), (b) the roll `1` (VA `0x6FE3B`), (c) developer editor `FUN_00021ddc` (VA `0x22671`, reached via the "Data Entry Menu" `FUN_00021c30`, caller `FUN_0001eabc` <- `FUN_0001e4c0`; authoring tool, not a gameplay path; retail reachability not established), (d) **`FUN_0007abf0`** (file `0xDD294`, writes `1` at VA `0x7AC64` and `0`, "liberation", at VA `0x7ACD2`). (d) is the only code that can clear the flag from gameplay data: it sums `rec[+0x22]` over the org's records in the 180-entry table at VA `0x144918` (stride `0xCE`; `+0x21` owner, `+0x20` nonzero = killed) with `+0x22 > 4`, and takes over if killed-weight > alive-weight, otherwise clears the flag and sets `M[i][Alien] = -50`, `M[i][X-COM] = +50`. **It is dead code**: its only caller is `FUN_0007acfc` (file `0xDD3A0`), which has no E8/E9/Jcc caller, no relocated pointer in Ghidra memory (control in section 1), and sits in a region Ghidra never disassembled. Control for the "no pointer" claim: the only raw-file hit of the value `0x6ACFC` (file `0x68DC6`) decodes as an LE fixup record `07 10 B7 07 02 FC AC 06 00` (type 32-bit offset, internal reference, page offset `0x7B7`, **target object 2**, target offset `0x6ACFC`), i.e. the data-table displacement operand of `FUN_000b46c4` at VA `0xB47B7`; it is not a code pointer. LE header at file `0x37F34` (fixup record table `0x389A5`). Verdict: **no live liberation path; no post-takeover decay.**
- X-COM cannot buy its way back: the bribe screen test (file `0xF9A0A`-`0xF9C01`, VA `0x97366`/`0x97538`/`0x9755D`) refuses when `flag != 0` ("under Alien control", group 22 item 119) or when `M[org][Alien] >= 75` ("Alien friends", item 120, string file `0x14D7C8` / `0x14D82A`). Engine `bribescreen.cpp:68-77`: match.
- Engine: `takenOver` is never reset (`grep` finds only the declaration default); match.

## 5. Infiltration growth, difficulty, relations init (T6-T9)

- **T6 hourly growth (OBSERVED, re-verified).** `FUN_0006fcc0` (file `0xD2364`): for orgs 0..26 `v += FUN_0006fc34(org) / (42 - d)` (signed `idiv`, file `0xD23A5`), `d = hi-word of dword [0xE0C9A]` (file `0x14233E`; VA `0x6FCE7`-`0x6FCF1`), minus 1 if the hours word `[0xD4D68]` is odd (file `0xD23AD`-`0xD23C6`), clamp to 200 then to 0. `FUN_0006fc34` (file `0xD22D8`): over buildings (stride 0xE2) owned by the org (`+0xC8`), `sum_k crew[k] * alienSpeed[k]` (table VA `0xF3D78`, file `0x15541C`) `* functionSpeed[bld.function]` (VA `0xE0D48`, file `0x1423EC`) `* orgSpeed[org]` (VA `0xE0E0C`, file `0x1424B0`). Engine `organisation.cpp:625-668`: **match** (the relative order of the sucker-growth step and the hourly step inside the hour is not compared; in the EXE `FUN_0006f3a8` runs before `FUN_0006fcc0` in the same hourly block, file `0xAD4F1` then `0xAD514`).
- **T7 sucker growth.** `FUN_0006f3a8` (VA `0x6F3A8`, file `0xD1A4C`), species case 1 only: `infil[owner] += grown * orgSpeed * functionSpeed`, clamp 200 (VA `0x6F474`-`0x6F496`). Engine LIVE `building.cpp:880-887` multiplies by `growthInfiltration` which is 1 only for the Brainsucker in `agent_types.xml`: **match**. The infiltration array has no other writer (raw scan of every `0x6F4C8..0x6F4C8+27*0xE0` operand: roll, hourly, sucker growth, history shift, UI/graph readers, save/load `FUN_000a3234` / `FUN_000a4054`). **No mission result, relation event or threshold crossing writes infiltration**; this also means the index's "FUN_00057c8c = infiltration detection relation hit" label is wrong: `FUN_00057c8c` (file `0xBA330`) is vehicle damage application with the downed-vehicle relation call at VA `0x5826F`.
- **T8 Superhuman/difficulty (OBSERVED).** The roll function never touches `[0xE0C9A]`. Difficulty-dependent items near takeover: divisor `42-d`; `FUN_00099130` starting relations; detection weight `building_detection_weight - 2d` (`FUN_0006f964`, file `0xD2104`-`0xD210B`; engine `building.cpp:797` LIVE): match; org raid timer `80 - 2d` (`FUN_00092060`, file `0xF4928`-`0xF4936`: `mov edx,0x50 ... mov [0x1277A4],edx` after subtracting `2*d`, VA `0x92284`-`0x92292`; engine `organisation.cpp:377`): match. Superhuman = d = 4: divisor 38; Government start relation 55..75 (below).
- **T9 `FUN_00099130`** (file `0xFB7D4`, single caller VA `0xB94FD` inside new-game init `FUN_000b94c0`, file `0x11BB64`). For rows `i` in 1..26 (skips row 0 = X-COM and row 27), cols `j` in 0..26 (skips `j = 27`), skipping starting values of exactly 100: `r = start[i][j] (int32 at file 0x189040)`, `r += rand16(20) - 10`; if `d != 0`: if `j == 0` `r += 10 - 5d`; else if `r > 0` `r += rand16(3d)`; else if `j != 1` `r -= rand16(5d)`; clamp +-100; store to both matrix copies. Engine LIVE `gamestate.cpp:626-647` (HEAD `625-646`): same shape. **Differences: ordering only** (the EXE applies the +-10 noise first and then tests `r > 0`; the engine tests `>= 0` before the noise). Government->X-COM: 75 + [-10,10] - 10 = **55..75** at Superhuman in both. Not in `FUN_00099130`: any initial alien crews. All 27 infiltration values and flags are zero at new game (BSS + init `mov byte [+0x11],0`); initial crews, if any, are not placed here (lead).
- Note `FUN_000b94c0` also calls `FUN_000941dc` with `AL=1` twice (VA `0xB953C`, file `0x11BBE0`) interleaved with `FUN_000962cc`, versus `AL=0` from the weekly path (VA `0x4AC2F`). This is the answer to the funding agent's D18 "does the original run a funding pass at new game": a call exists, but what AL=1 does is not decoded here.

## 6. Quantified impact (T1+T2): time to Government takeover

Monte Carlo (`validate/tools/sim.py`, 4000 runs per row, hourly add `a = floor(S/38)` with the odd-hour -1, engine roll every 125 min p = v/200, EXE roll every 300 min p = (v-100)/101 for v > 100, random roll phase; start v = 0, S = hourly numerator before the divisor):

| a (+/h) | S range / example | engine median h | engine P by 24h / 48h | EXE median h | EXE P by 48h / 7d / 14d |
|---|---|---|---|---|---|
| 1 | 38-75 | 34 | 0.30 / 0.76 | 236 | 0 / 0 / 1.00 |
| 2 | 76-113 (10 anthropods in the Senate = 100) | 19 | 0.68 / 0.99 | 87 | 0 / 1.00 / 1.00 |
| 3 | 114-151 | 15 | 0.86 / 1.00 | 56 | 0.18 / 1.00 / 1.00 |
| 7 | 266-303 (27 aliens x2 x Senate 5 = 270) | 9 | 1.00 / 1.00 | 25 | 1.00 / 1.00 / 1.00 |
| 0 | S < 38 | never | never | never | never |

So the EXE gives the player roughly 3-7x longer between "infestation starts" and "org lost", and a clear **observable trigger: an org's displayed infiltration (raw/2, graph/UFOPaedia) must exceed 50%** before it can flip. Consistent with `failure_forensics.md` §3: of 166 benchmark saves no non-taken-over org sits at a mid value (15..199) because the engine flips it as soon as `v` reaches the teens; the EXE rule would leave such orgs alive for days.

## 7. Other findings (T10-T14)

- **T10 tech tier +3 (engine_missing).** `FUN_000b46c4` (VA `0xB46C4`, file `0x116D68`; callers `FUN_000aec70` / `FUN_000aedb0`, the average-guards rolls at file `0x111314` / `0x111454`): `tier = org[+6] + weekCounter - 1; if org[+0x11] != 0 tier += 3; clamp 0..11` (VA `0xB4715`-`0xB4748`, file `0x116DB9`-`0x116DEC`), then picks entries from three composition tables (VA `0x13ACFE`, `0x13AFCC`, `0x13B1AA`, indexed by tier and a random row) and spawns them into the pending battle (count cap 500). INFERRED role: defender composition for org-building missions. Engine: `tech_level = starting_tech_level + 1` then +1 weekly capped at the largest human equipment-set `min_score` (`gamestate.cpp:1740-1755` LIVE; `extract_organisations.cpp:50`) and used for guard equipment at `agent.cpp:173`; there is no takenOver term anywhere (`grep takenOver` has no hit in agent/battlemap code).
- **T11 bribe refusal**: see T4, match.
- **T12 detection halving (diverges, low).** `FUN_0006f964` (file `0xD2008`): `value *= buildingWeight - 2d; /100`; then `if M[owner][Alien] >= 75 { value/2 } else if M[owner][X-COM] <= -50 { value/2 }` (VA `0x6FA8E`-`0x6FAC1`, file `0xD2132`-`0xD2165`, via `FUN_00091de4` / `FUN_00091db0`); then `if flag { value/2 }` (VA `0x6FADF`, file `0xD2183`); roll `rand16(100) < value - 10`. Engine `building.cpp:797-810` LIVE (`776-789` HEAD) applies the Allied halving AND the Hostile-to-player halving independently, then the takenOver halving. A taken-over org (+100 to Aliens, -100 to X-COM) is therefore x1/4 in the EXE and x1/8 in the engine. (Also: EXE roll range is `[0,100]`, engine `[0,100)`; and the EXE hostile predicate is `<= -50`, the engine's `x < -50`.)
- **T13 relation primitive gates (engine_missing, low).** `FUN_0005faf0` (file `0xC2194`): no change at all if holder is X-COM or holder == target (VA `0x5FB1B`-`0x5FB2D`); if the holder is **Cult of Sirius (index 4) or taken over** and the target is the Aliens, the change is skipped (VA `0x5FB2D`-`0x5FB4A`), and the reputation ripple is skipped when (target is Aliens) and (third org is Cult or the holder is taken over) (VA `0x5FB97`-`0x5FBB6`). Ripple factor: third org `C` with opinion `M[C][A]`: `f = M/2` (positive) or `M/4` (negative), `M[C][B] += f*delta/100` only if the result stays strictly inside (-100,100); X-COM never rippled. Engine `adjustRelationTo` (`organisation.cpp:1020-1073`) implements the ripple, X-COM skip and boundary drop but not the Aliens-target freeze for Cult/taken-over orgs.
- **T14 raid table (match).** `FUN_00092060` (file `0xF4704`) picks the "military" mission table iff the org index is 3 or 20..22 **or** the taken-over flag is set (VA `0x92303`). Engine: `militarized` from organisation type 1/3 plus `takeOver()` sets it (`organisation.cpp:91, 403-410`).
- Alien/UFO targeting avoids taken-over owners (`FUN_0006cfb4` VA `0x6D25E`, `FUN_00070a2c` VA `0x70A9A`, `FUN_000702e4` VA `0x70472`, `FUN_0006f7f8` movement chance +20 for taken-over or aliens-allied owner, VA `0x6F8A3`): engine uses relation checks; not diffed line-by-line (relations make it equivalent for taken-over orgs).

## 8. Side findings for other agents

- **UFO weekly pattern consumer found (D15):** `FUN_0006cfb4` (VA `0x6CFB4`, file `0xCF658`; callers `FUN_0004ab04` at VA `0x4AB50` and `FUN_0006d384`): row = `clamp(weekCounter-1, 0, 19)`, column = `rand16(9)`, value read at Ghidra VA `0xF3AC0` (file `0x155164`, `UFO_mission_patterns`): 1 -> incursion indices 20..29 (Attack), 2 -> 30..39 (Subversion), 5 -> 40..44 (Overspawn), other -> 0..19 (Infiltration); first index whose three ship slots all fit available counts wins; override `DAT_000E0CC0` forces type 2 on the first draw. Matches the engine description in exe_evidence_index §5.6; the "never validly examined" status can be closed for the pattern table (growth table consumer not examined).
- `FUN_00096bc8` (file `0xF926C`): org-initiated bribe requests; skips taken-over orgs and the Aliens; engine `gamestate.cpp:1557` HEAD / `1590` LIVE equivalent in shape; bribe cost has `+5000 + 3000*d` at VA `0x97340`-`0x97359`, which the engine reproduces at `organisation.cpp:1154-1155` (closes part of the "bribe formula NOT BOUND" row; the first term of the formula is in the undisassembled screen code `VA 0x97234..`).
- Hostile threshold in the EXE is `M <= -50` for AI predicates but `M < -50` (strict) for the funding cutoff; the engine uses `x < -50` for both (T12 note).

## 9. What I could not establish

- Reachability of the developer "Data Entry Menu" from retail play (only the call chain `FUN_0001e4c0 -> FUN_0001eabc -> FUN_00021c30 -> FUN_00021ddc` was walked).
- Initial alien crews at new game (not in `FUN_00099130`, `FUN_000b94c0`'s direct calls were skimmed only: `0x15400` places the X-COM start building; `0x799dc` initialises the 180 personnel records).
- Exact role of `[0xD5060]` (named "alien dimension flag" from its writers).
- The sub-second tick-to-wall-clock mapping at game speeds (cadence is stated in game minutes, which the engine's clock also uses).
- Whether positive relation deltas toward X-COM can lift a taken-over org above -50 within a week (no source found).

## 10. Engine fix sketches (for the engine-fix plan)

Existing tests: `tests/test_organisation.cpp` (HEAD) locks only the hourly clamp / decay / divisor (`test_taken_over_infiltration_clamp` line 129, `test_infiltration_hourly_ufo2p_rules` 150, `test_infiltration_divisor_uses_difficulty` 196); `next-implementation.md` item 8 says the "hourly takeover behaviour" is locked by `test_organisation`. No test calls `takeOver` / `updateTakeOver` (grep over `tests/`), so T1/T2 need new tests and the existing ones stay valid (they cover T4/T6 which already match).

- **T1 (roll).** `Organisation::takeOver` (`organisation.cpp:84-88`): replace `randBoundsExclusive(rng,0,200) >= infiltrationValue` by `randBoundsInclusive(rng,0,100) >= infiltrationValue - 100` (return without effect). No other change to the body.
- **T2 (cadence).** Remove `TICKS_PER_TAKEOVER_ATTEMPT` / `ticksTakeOverAttemptAccumulated` / the random init (`organisation.h:19,130`, `gamestate.cpp:611-612` LIVE). Add one serialized `GameState` counter `takeoverSlot` (init 2). In `updateEndOfFiveMinutes` (LIVE `gamestate.cpp:1456`): if the player is in the human city: `slot = takeoverSlot; takeoverSlot = (slot + 1 >= 62) ? 2 : slot + 1; if (slot < 27) { org = organisation with exeOrgIndex == slot; if (org && !org->takenOver) org->takeOver(*this); }`. `exeOrgIndex` already exists (`organisation.h:156`). Net: each org tested every 300 game-minutes, Government first.
- **T10 (+3 tier).** Approximation without new RE: in `agent.cpp:173` use `min(org->tech_level + (org->takenOver ? 3 : 0), maxHumanSetLevel)`. Exact fidelity needs the `FUN_000b46c4` composition tables at Ghidra VA `0x13ACFE` / `0x13AFCC` / `0x13B1AA` extracted (blocked on RE).
- **T12.** `building.cpp:797-810` LIVE: make the hostile halving an `else` of the allied halving; hostile predicate `<= -50` for AI predicates.
- **T13.** `adjustRelationTo` (`organisation.cpp:1020`): early-return when `thisOrg` is X-COM or `thisOrg == other`; skip when (`thisOrg` is Cult of Sirius (exeOrgIndex 4) or taken over) and `other` is the Aliens; skip ripple targets per the rule in section 7.
- **T15 (harness).** The EXE shows a modal; `game.log` has no takeover line (failure_forensics §0.6), so the harness cannot see the event except via `final.json` / saves or the relation message; log the `AlienTakeover` event in the harness side.
