# v_gov_income: Government income dynamics and X-COM funding, engine vs UFO2P.EXE

Scope: weekly funding (bound), the new-game funding pass, the tick-0 income cut seen in LIVE saves, the Government balance as a cap, bankruptcy, and salary/upkeep/facility costs.
Binary: `UFO2P.EXE`, ISO non-4 (CRC `0x4749ffc1`, SHA-256 `99f8787d5f1cb532...`, verified by the Ghidra script's `getExecutableSHA256`). No game was launched. No file outside `scratchpad/validate` and the private Ghidra copy was written.

Labels used below. **OBSERVED** = bytes, listing text, decompile text, code lines, save or log contents read in this session. **INFERRED** = my reading of observed facts. **LIVE** = the frozen snapshot of the develop-reboot worktree that the benchmark plays (HEAD `fda36d6c` + uncommitted). **HEAD** = committed `fda36d6c` in REPO. Where unlabelled, LIVE and HEAD agree.

## 0. Answers in one page

1. **LIVE implements the EXE weekly funding block exactly, except that the Government relation is a float rather than a signed byte (G13)** (`LIVE:game/state/gamestate.cpp:1765-1865`; tier table `data/common_patch/gamestate.xml:16-57`; `calculateFundingModifier` `:2000-2023`). A differential test of my transcription of the EXE block against LIVE found **0 mismatches in 393,120 cases**; the same test shows **HEAD diverges in 222,714** (control arm). HEAD deviations D1 (cutoff period), D2 (cap stored), D3 (credit old F / debit new F), D4 (2e9 guard), D5 (8th score category), D6 (upkeep of unfinished facilities), D7 (UI re-derives state) are all fixed in LIVE.
2. **The original does run the funding routine at new game: twice, in a quiet mode, and then overwrites X-COM's balance and income.** `FUN_000b94c0` (file `0x11BB64`) calls `FUN_000941dc` (file `0xF6880`) in a 2-iteration loop with `AL=1`. `AL!=0` skips the funding report, the salary/upkeep screen and the balance commit. After the loop the routine stores `balance = 140000 - 10000*d` and `income = 92000 - 3000*d` (file `0x11BC47`, `0x11BC5D`). At Superhuman X-COM therefore starts at exactly **100,000 / 80,000** whatever the passes did.
3. **LIVE's tick-0 income of 10,000 is an engine bug relative to the EXE** (verdict `engine_bug`, impact critical). `GameState::startGame()` calls `updateEndOfWeek(true)` (`gamestate.cpp:714`) before `initState()` fills `Organisation::buildings` (`city.cpp:157`), so the Government has no income and no tax at that moment, G = 20,000, the cap is 10,000 and it is **stored** (LIVE) and a windfall of the old 80,000 is credited. Observed: 131 logged LIVE-arm campaigns start with funds **180,000** (the 5 tick-0 LIVE saves show income **10,000**); 32 logged HEAD-arm campaigns start with **110,000** (the 2 tick-0 saves show income 80,000); the EXE gives **100,000 / 80,000**. The benchmark economy is at 1/8 of the real Superhuman income.
4. **Government balance does not bind at Superhuman after warm-up.** 164 save files (162 end-of-run saves, 1 mid-run `unstable` save, 1 copy `bench.save`): 0 non-terminated saves where G/2 is below the stored X-COM income; minimum Government balance 1,012,103 (G/2 = 506,051) against a maximum possible income of about 100,000 (80,000 + 25 %). Government inflow is about 1.2 to 1.3 M per week (tax about 1.0 M + building net about 0.2 M). The cap in LIVE only ever bound at tick 0, because of the bug in point 3. The per-building income model is not "unbound": the whole model is in `FUN_000941dc` and the Government path of the engine port matches it; one other part (non-Government orgs) does not (finding G5).
5. **The original has a bankruptcy defeat, and the engine has none.** `FUN_00060ea4` (file `0xC3548`) is called as the last act of every `FUN_000941dc` pass with `EAX=1`; it declares "X-COM IS DEFEATED, bad debts" when the X-COM balance is **strictly below -5000** (signed 32-bit compare, file `0xC3557`). There is no N-week counter. The constant occurs exactly once in the whole file. In the 164 logged harness campaigns, 4 reached a post-Monday balance below -5000 and kept playing (-6,193, -11,364, -15,057, -17,557).
6. **Overheads are applied once a week, at Monday midnight, after funding**, through `[0xD892C]` (one writer, one reader). Upkeep counts only completed facilities. Placement charges the full build cost; cancelling refunds `cost * remaining / buildTime`. LIVE matches all of this; HEAD charges upkeep on unfinished facilities. The construction-counter decrement cadence is not established.

## 1. Method, address conversion and controls

Tools: `ghq.sh v_gov_income UFO2P VGov.java ...` (private project copy; script `ghidra/copies/v_gov_income/scripts/VGov.java`; each call took 8 to 14 s); direct byte reads of `OGR/canonical/UFO2P.EXE`; capstone (uv cache) for disassembly; Python scans of an instruction sweep of the code region (`validate/work/ins.pkl`, 228,455 instructions, file `0x72000-0x131000`).

Conversion (UFO2P non-4, from exe_evidence_index section 3): code `file = VA + 0x626A4`; file-backed data `file = VA + 0x616A4`; raw data operands in instruction bytes are `VA - 0xD0000`. Receipt for this session: Ghidra printed `FUN_000b94c0 entry_bytes=5351525657e866bdffff...` at `file_off=0x11bb64`; the raw file bytes at `0x11BB64` are the same (`53 51 52 56 57 e8 66 bd ff ff`). Disassembly addresses below are file offsets; the Ghidra VA is the file offset minus `0x626A4`.

Controls (a negative or a constant is only claimed where the same instrument finds a known positive):

| # | Instrument | Control result |
|---|---|---|
| C1 | raw `E8/E9` rel32 scan vs Ghidra `getReferencesTo` | callers of `FUN_000941dc`: scan = `0xAD2D3, 0xAD496, 0x11BBE0`; Ghidra = `FUN_0004ac24`, `FUN_0004ac40`, `FUN_000b94c0`. Callers of `FUN_00060ea4`: scan `0xF70F6, 0x11581C`; Ghidra = `FUN_000941dc`, `FUN_000b3114`. Identical. |
| C2 | string-pool parser (group table at file `0x154E38`, groups of NUL-terminated strings) | reproduces group `0x25` item `0x9B` = "Alien Buildings Destroyed" at `0x151634` and `Monday` = (36, 0) at `0x150905`, both from the earlier funding finding |
| C3 | dword scan for `-5000` | the same scan finds `-2400` at the known `CMP ECX,0xFFFFF6A0` (`0xF6FC1`) and `2,000,000,000` at the known guard (`0xF6AAB`); `-5000` occurs once in the file |
| C4 | differential test `validate/work/funding_diff.py` | LIVE 0 / HEAD 222,714 mismatches (section 2) |
| C5 | writer scan for `[org0+0xC]` | finds the three known writers (`0xF7083`, `0xF709B`, `0x11BC5D`) and no others |
| C6 | three-way day-0 funds (runner logs) | HEAD arm 110,000 as predicted by the HEAD code path (section 4) |

## 2. Q1: LIVE vs the EXE funding block

### 2.1 EXE block (OBSERVED, `FUN_000941dc`, file `0xF6880`, tail `0xF6FD0-0xF7104`)

```
0f6889  mov byte ptr [esp+0x44], al        ; AL = mode (0 = Monday, 1 = new game)
0f6fd5  mov eax,[edx+0x374]                ; Government balance (org 2: 2*0x1B6+8)
0f6fdb..0f6fe2  sar/sub/sar 1              ; G/2, truncating toward zero
0f6fe4  test eax,eax ; jle 0xf7025         ; G/2 <= 0 : delta := 0, cap := -F
0f6fef  sub eax,edx                        ; cap = G/2 - (F + delta)  (edx = delta + F + ebp, ebp = 0)
0f6ff5  test ; jle ; xor edx,edx           ; if cap > 0 then cap := 0
0f7038  cmp byte ptr [esp+0x44],0 ; jne 0xf7060   ; report only in mode 0 (call 0xFAB28 at 0xF705B)
0f7060  cmp [esp+0x14],0 ; jne 0xf7088     ; hostile flag
0f7067  cmp [esp+0x10],0 ; jne 0xf7088     ; H < -2400 flag
0f7083  mov [eax+0xc],ecx                  ; income := F + delta + cap            (stored, persistent)
0f7088  sub [eax+0x8],edx                  ; cutoff: balance -= old income (unconditional)
0f709b  mov dword ptr [eax+0xc],0 ; 0f70a2 mov word [0x10ca0],cx   ; income := 0 ; latch := 1
0f70b7  sub [eax+0x374],edx                ; Government balance -= NEW income
0f70bd  cmp byte ptr [esp+0x44],0 ; jne 0xf70e0   ; mode 1 skips the finance screen:
0f70cd  call 0xfc4dc                       ; salaries + upkeep -> [0xD892C]
0f70dd  mov [edx+8],eax                    ; balance := [0xD892C]   (the only commit)
0f70f6  call 0xc3548  (EAX=1 at 0xF70F1)   ; bankruptcy / game-over check
```

Tiers (decompile `OGR/export/funding_FUN_000941dc.c`, re-checked against the `jle/jl` immediates): `W>400 /20`, `>800 /16`, `>1600 /12`, `>3200 /8`, `>6400 /5`, `>12800 /4`, `<0 -F/15`, `<-400 -F/10`, `<-800 -F/5`, `<-1600 -F/4`, as sequential `if`s, so the most extreme match wins. `W` is the sum of the eight this-week score words; `H` the eight previous-weeks words; `H` is tested alone (`CMP ECX,0xFFFFF6A0` at `0xF6FBF`). This reproduces the earlier finding (`OGR/export/funding_findings.md`); nothing here contradicts it.

### 2.2 LIVE (OBSERVED, `LIVE:game/state/gamestate.cpp`)

| EXE step | LIVE line |
|---|---|
| credit old F unless balance >= 2e9 | `:1787-1790` |
| hostile = Gov->X-COM `< -50`; `H < -2400`; undo credit, income 0, latch | `:1792-1801` |
| delta from OLD F by the tier table | `:1808-1809`, `:2000-2023` |
| `G/2 <= 0` clears delta and sets cap `-F`; else cap `G/2 - (F+delta)` | `:1811-1822` |
| store capped income; debit Government by the NEW income | `:1823-1824` |
| `H = totalScore - weekScore` (all increments update both: `battle.cpp:3211-12, 3585-86`, `scenery.cpp:1193-94`, `vehicle.cpp:2120-27`, `vehiclemission.cpp:1402-03`, `cityview.cpp:4847-48, 4924-25`) | `:1773` |
| upkeep skips `buildTime > 0` | `:1858` |
| 8th category in `W` | `gamestate.cpp:2165-2166` |

### 2.3 Differential test (control arm included)

`validate/work/funding_diff.py` codes the EXE block above and the LIVE and HEAD functions independently (integer C semantics: truncating division, sequential-`if` tiers) and compares the post-state (X-COM balance, stored income, Government balance, latch) over F in {0, 1, 7, 10000, 10500, 80000, 92000, 123457}, W at every tier boundary +/-1, H in {-5000, -2401, -2400, -2399, -100, 0, 500}, relation in {-100, -51, -50, 0, 75}, G in {-3, 0, 1, 2, 19999, 20000, 20001, 100000, 159999, 160000, 160001, 1e6, 2.1e6}, balance in {100000, 1999999999, 2000000000}: **393,120 cases. LIVE 0 mismatches. HEAD 222,714.** The HEAD mismatches split as: latch/cutoff period (D1), income not capped (D2), Government debited the paid amount (D3), the 2e9 guard (D4). Caveats: both sides are my own transcriptions (the EXE side from the listing above, not an emulator); the float-vs-signed-byte edge at relation -50 is outside the integer cases (finding G1c).

### 2.4 HEAD deviations that LIVE fixes (OBSERVED by diff)

D1 `HEAD:gamestate.cpp:1737-1738` tests lifetime `totalScore`; D2 `:1749-1753` caps the payment, `:1759-1762` stores uncapped; D3 `:1756-1757` symmetric transfer of the capped payment; D4 no 2e9 guard; D5 `getTotal` at `:2090` sums 7 categories; D6 `:1786` charges every facility; D7 `HEAD:game/ui/city/weeklyfundingscreen.cpp:33-37` returns early on termination, `:61` re-tests `totalScore`, `:80-85` re-caps from the post-debit balance, `:121` resets the week score in the UI. Data for D5 (LIVE `data/common_patch/building_functions.xml`): destroyed-score values 400, 400, 300, 500, 300, 250, 250, 350, 300, 500 for incubator, spawning, food, megapod, sleeping, organic factory, alien farm, control chamber, maintenance factory, dimension gate generator = the EXE table at file `0x191B7C`.

## 3. Q2: the new-game funding pass (D18)

### 3.1 EXE (OBSERVED, `FUN_000b94c0`, file `0x11BB64`; Ghidra confirms the loop shape)

```
11bba1  call 0xfb7d4        ; FUN_00099130  difficulty-adjusted starting relations (before the passes)
11bbd7  xor edx,edx         ; i = 0
11bbd9  mov ebx,1
11bbde  mov eax,ebx         ; EAX = 1  -> mode argument of FUN_000941dc
11bbe0  call 0xf6880        ; FUN_000941dc   (funding, mode 1)
11bbe5  inc edx
11bbe6  call 0xf8970        ; FUN_000962cc   (org vehicle-park refill)
11bbeb  cmp dx,2 ; jl 0x11bbde          ; exactly 2 iterations
11bbf6  mov eax,[0x10c9a] ; sar eax,16  ; difficulty d
11bbfe  imul edx,eax,0x2710 ; 11bc04 imul eax,eax,0xbb8
11bc19  mov ebx,[0xfdcc]  ; 140000
11bc32  sub ebx,edx ; 11bc47 mov [edx+8],ebx      ; X-COM balance := 140000 - 10000 d
11bc4a  mov ebx,[0xfdd0]  ; 92000
11bc56  sub ebx,eax ; 11bc5d mov [edx+0xc],ebx     ; X-COM income  := 92000 - 3000 d
11bc94  call 0xce450 ; 11bc99 call 0xd1450          ; FUN_0006bdac ; FUN_0006edac
```

Ghidra decompile (`FUN_000b94c0`): `do { FUN_000941dc(); FUN_000962cc(); } while (extraout_DX < 2);` followed by the two stores.

What mode 1 does inside `FUN_000941dc` (OBSERVED gating at `0xF7038`, `0xF70BD`, `0xF70E0`): the funding report, the finance screen (salaries + upkeep) and the balance commit are all skipped; the organisation payment loop, Government tax, wage/hiring model, both market passes (`FUN_000964d0`, `FUN_00096614` at `0xF6E64/0xF6E69`), the cutoff test, the cap and the Government debit are not gated and do run.

Callers of `FUN_000b94c0`: `FUN_000b9178` (the main-menu loop, `switch(DAT_001392ec)`) in case 1 at `0x11BAD8` (the ordinary new game: it first calls `FUN_00062740`) and in case 3 at `0x11BB37` (a second start option, not identified). Both run the same function, so there is no start path that skips the passes.

### 3.2 Consequences

- **OBSERVED:** the X-COM stores come after the loop, so the X-COM side of both passes is discarded. Start state at Superhuman is exactly 100,000 / 80,000.
- **INFERRED:** the Government-side effects persist (two debits of `F_next`, about 92,000 each if the X-COM +0xC still held the table value 92,000 at that time; the 92,000 and 140,000 are the same constants the formula subtracts from, file `0x141474`, `0x141470`). Also persistent: two park-refill calls, two market passes, wage/workforce settling. OpenApoc's two `updateOrgFinances()` calls in `initState` (`LIVE:gamestate.cpp:315-316`, HEAD `:314-315`) are the analogue of the economy part ("run two weeks of funding so workers move around").
- No salary or upkeep is charged at new game in the EXE (mode 1).

### 3.3 Engine

LIVE and HEAD both call `updateEndOfWeek(true)` from `startGame()` (`gamestate.cpp:714` LIVE, `:713` HEAD), which runs `weeklyPlayerUpdate()` unconditionally (`:1738` LIVE, `:1705` HEAD). The EXE also assesses funding at new game, but then restores X-COM's finances; the engine does not restore them. That is the divergence.

## 4. Q3: the tick-0 cut, reproduced

### 4.1 Observed numbers

Day-0 saves (game time 6,220,800 ticks = day 1.50 in `ticks/12441600 + 1`) from `build-funding/**/victory.save` (`validate/work/scan_saves.py`, output `validate/work/saves_scan.txt`):

| Arm | saves at tick 0 | X-COM `income` | `previousWeekIncome` | Government balance | Government building income |
|---|---|---|---|---|---|
| LIVE (`build-funding`) | 5 (`learning-control-seed11/r0010`, `learning-serial-prefix/r0000`, `learning-show-old1/r0004, r0006`, `learning-show/r0010`) | **10,000** | 80,000 | 2,059,003 to 2,105,003 | 155,633 |
| HEAD-like (`build/learning`) | 2 (`r0006`, `r0010`) | **80,000** | 80,000 | 2,103,003 and 2,105,003 | 155,633 |

Starting campaign funds (first `[sell] ... funds A->B` line of every runner log; `grep -l` over all `build*/**/runs/*/runner.log`): **`funds 180000->` in 131 logs, `funds 110000->` in 32 logs, no log with 100000.**

Three-way table (Superhuman, difficulty value 4):

| | X-COM balance | X-COM income | source |
|---|---|---|---|
| EXE | 100,000 | 80,000 | stores at `0x11BC47`, `0x11BC5D` |
| HEAD | 110,000 | 80,000 | OBSERVED 32 logs; arithmetic: avail = 20,000/2 = 10,000 < 80,000, so payment = 10,000 (`gamestate.cpp:1749-1757`), income untouched (`:1762`, modifier 0) |
| LIVE | 180,000 | 10,000 | OBSERVED 131 logs and 5 saves; arithmetic: credit 80,000 (`:1789`), cap `-70,000` (`:1818-1822`), store 10,000 (`:1823`), Government debit 10,000 (`:1824`) |

### 4.2 Mechanism (OBSERVED code, INFERRED ordering consequence)

1. `mainmenu.cpp:31-34` and `difficultymenu.cpp:35-38` run `startGame()`, then `initState()`, then `fillPlayerStartingProperty()`, then `fillOrgStartingProperty()`.
2. `startGame()` ends with `updateEndOfWeek(true)` (`:714`): `updateOrgFinances()` then `weeklyPlayerUpdate()`.
3. `updateOrgFinances()` loops `org->buildings` (`gamestate.cpp:1883-1892`). That vector is built only in `City::initCity` (`city.cpp:157`, `organisation.h:239` "not serialized, set in initCity"), which `initState()` calls (`gamestate.cpp:227`) after `startGame()`. At step 2 it is empty: no building income, no `totalCivilianIncome`, so no tax. The Government stays at its XML balance 20,000.
4. `weeklyPlayerUpdate()` therefore sees `G = 20,000`, `avail = 10,000`, stores 10,000 (the exact value observed), and credits the old 80,000. No agents or bases exist yet (`fillPlayerStartingProperty` is later), so overheads are 0: 100,000 + 80,000 = 180,000, exactly the starting funds in the logs.
5. `initState()` then runs `updateOrgFinances()` twice (`:315-316`), after `initCity`: the Government gains tax about 1.07 M and building income about 0.16 M per pass, reaching the observed 2.06 to 2.14 M at tick 0. Too late for the stored income.

### 4.3 Verdict and fix sketch

`engine_bug` relative to the EXE.

**Primary (faithful) fix.** In `startGame()` do not call `updateOrgFinances()` or `weeklyPlayerUpdate()` at all (keep `updateUfoGrowth()` and `updateItemMarket()` if wanted: the EXE also runs the market inside the passes and `FUN_0006edac` after them). Let `initState()`'s two `updateOrgFinances()` calls (`:315-316`, run after `initCity` has filled `org->buildings`) be the warm-up, optionally followed by the two Government debits of `F_next`; leave X-COM at the difficulty-patch values (100,000 / 80,000), which is what the EXE's stores at `0x11BC47/0x11BC5D` guarantee. Why not just restore afterwards: the day-0 `updateOrgFinances()` also runs with `org->buildings` empty, so `populationWorking` is 0 and `averageWage` is forced to 1 (`gamestate.cpp:1896-1897`), and steps 3-5 then run over `humanCity->buildings` with `minimumWage = 30`, `expectedWage = 1` and wage steps of x1.05 to x1.2 (INFERRED from the code; not measured). OpenApoc thus performs three warm-up passes, one of them malformed, where the EXE performs two.

**Stopgap.** Capture `player->balance` and `player->income` before `updateEndOfWeek(true)` and restore them after. This fixes X-COM (and removes HEAD's +10,000) but leaves the malformed economy pass in place.

**Ordering.** Land this before the bankruptcy rule (G8): with the 10,000 income still in force, LIVE campaigns would hit bad-debt defeats the original would not produce. Not blocked on RE; the only open point (what X-COM +0xC held before the stores) does not affect the result.

Knock-on (OBSERVED): every LIVE campaign's X-COM income is 10,000 +/- tier steps. Net cash change across 140 logged Mondays (cash before the weekly screen to the first cash figure after it): median -13,565, mode -12,900 (13 Mondays) = 10,000 income - 18,000 salaries (10 soldiers x 600 + 15 specialists x 800) - 4,900 upkeep (starting base, all `buildTime 0`, from `player_bases.xml` of a day-1.5 save). With the EXE's 80,000 the same Monday is +57,100.

## 5. Q4: Government income model and the cap

### 5.1 EXE economic model (OBSERVED, `FUN_000941dc`, org loop file `0xF69B9-0xF6ACE`)

Building record stride `0xE2` (count word at `0xB3A68`): `+0xBE` maintenance (u16), `+0xC0` maximum workers, `+0xC2` workers, `+0xC4` wage, `+0xC6` income per worker, `+0xC8` owner byte.

```
0f69c3  mov esi,[edx+0xc]          ; local := org income (+0xC)
0f69c6  cmp bx,2 ; jne ; xor esi,esi   ; Government (org 2): start from 0
        per building with owner == this org:
0f69f5  owner 0 (X-COM): +0xC0, +0xC2, +0xBE := 0 ; counted, no contribution
0f6a25  esi -= maintenance
0f6a37  workers > max : excess goes to the unemployed pool [0xEEF8], workers := max
0f6a7e  esi += (incomePerWorker - wage) * workers ; edi += workers ; [esp+0x20] += wage * workers
0f6aa5  if balance < 2,000,000,000 : balance += esi        ; 0xF6AA8 / 0xF6AB2
0f6ab5  mov [edx+0x1b2],esi        ; last-week total stored in +0x1B2 ; +0xC is NOT written
0f6ad4  avgWage = wageBill / workers ; 0f6ae9 tax = wageBill / 10
0f6b00  Government +0x378 (income, display) := tax ; 0f6b08 Government balance += tax
0f6b1c  cmp eax,0x1e ; floor on the wage threshold := 30
```

Then (decompile): workers below the wage floor leave (`workers = workers*wage/floor`); five hiring rounds at an expected wage of `avg*12/10` falling by `avg/10` per round, with literal wage thresholds 20/30/40/50 (`0x14, 0x1E, 0x28, 0x32`) scaling the unemployed pool joining; then wages move by workforce ratio (`<60 %` x1.20, `<80 %` x1.10, `<90 %` x1.05, `==max` x0.95, capped by `incomePerWorker - maintenance/max`).

Input to the cap: Government balance `G` = its previous balance + its own building net (can be negative per building) + 10 % of the civilian wage bill (all non-X-COM owners), then minus `F_next`, minus its park-refill purchases (`FUN_000962cc` starts at org index 2, so the Government is included).

### 5.2 Engine port vs EXE

`LIVE:gamestate.cpp:1868-2000` (`updateOrgFinances`) is a step-for-step port: Government income reset, `calculateIncome = workers*(ipc-wage) - maintenance`, tax `/10`, `std::max(averageWage, 30)`, five hiring rounds, wage steps. Differences:

| Item | EXE | Engine | Impact |
|---|---|---|---|
| non-Government org income | stored `+0xC` is a constant base; building net is local and not persisted | `org->income += calculateIncome()` persists (`:1889`; only `government->income = 0` is reset, `:1875`) | G5 below |
| hiring thresholds | literal 20/30/40/50 | 30/45/60/75 % of `civilianSalary` (65 in the human city) = 19/29/39/48 | small shift in who rehires; fidelity only |
| bought base building stays in the former owner's `buildings` list | owner byte decides; X-COM-owned fields zeroed every pass | `basebuyscreen.cpp:93` changes `owner`, list not updated until reload | about -1.1 k per warehouse per week, negligible against 1.0 M |
| organisations looped | indices 0..26 (Civilian excluded) | all except player and aliens | no Civilian-owned buildings in the data |
| park refill cadence | hourly | daily (`gamestate.cpp:1578`) | G6 |

### 5.3 Measured Government balance and stored X-COM income (OBSERVED, 164 saves)

`validate/work/saves_scan.json` / `saves_scan.txt` (script `scan_saves.py`). By game day (cross-section: one final save per run; not a single trajectory):

| day | n | Gov balance min / median / max | Gov building income (median) | X-COM stored income, non-terminated: min / median / max |
|---|---|---|---|---|
| 1.5 | 15 | 2,059,003 / 2,103,003 / 2,144,003 | 155,633 | 10,000 / 10,000 / 80,000 (80,000 only in the HEAD-like arm) |
| 5 to 7 | 2 | 1,252,503 / 1,331,253 / 1,410,003 | 155,633 | 10,000 |
| 7 to 10 | 43 | 1,805,131 / 2,192,051 / 2,453,991 | 196,943 | 9,334 / 10,000 / 84,000 |
| 10 to 14 | 36 | 1,337,059 / 1,752,857 / 1,980,019 | 196,943 | 10,000 / 10,000 / 84,000 |
| 14 to 21 | 41 | 1,012,103 / 1,943,723 / 3,555,484 | 238,247 | 7,500 / 9,000 / 74,667 |
| 21 to 30 | 19 | 1,265,991 / 2,148,800 / 3,676,606 | 269,760 | 6,720 / 6,735 / 6,750 |
| 30 to 50 | 4 | 1,822,548 / 2,077,280 / 4,179,217 | 303,012 | none (terminated) |

Cap test: of the non-terminated saves, **0** have `G/2 < max(stored income, previousWeekIncome)`. Minimum `G/2` 506,051 (`learning-treatment1-seed11/r0010`, day 20.7). Civilian wage bill from `buildings.xml` of three saves: 10.68 M (day 1.5, 173,547 workers), 10.05 M (day 7.2), 9.53 M (day 14.0), tax 1.07 M, 1.01 M, 0.95 M. First Monday raises the balance by about 1.03 M (1,252,503 at day 6.01 to 2,279,991 at day 7.47 in the same build).

Stored X-COM income moves only in tier steps (10,000 -> 10,500 -> ... -> 9,450 -> 9,000 -> 8,000 -> 7,500); none of the movement is the cap. In the HEAD-like arm the income stays 80,000 / 84,000 for the same reason in reverse.

### 5.4 Does G/2 bind in the original at Superhuman?

**Engine result, OBSERVED:** no. **EXE trajectory: INFERRED**, with a cadence caveat: the EXE refills organisation vehicle parks hourly (finding G6), the engine daily, so the Government's spending between Mondays could differ. The cap needs `G < 2 (F + delta)`, i.e. below about 160,000 to 200,000 at 80,000 to 100,000 income, while the Government takes in about 1.0 to 1.3 M per week and holds 1.0 to 4.2 M in every save. To bind it, the civilian wage bill and workforce would have to collapse by roughly an order of magnitude for several weeks, or the Government would have to spend more than 1 M a week on vehicles. Strategy consequence, scoped to this one mechanism: the Government's liquidity cap (`G/2`) is not a practical constraint at Superhuman, so civilian income need not be protected *for the cap's sake*. That does not make city damage free: it still costs score through category 6 (which feeds both `W` and `H`) and Government relation, and relation `< -50` or `H < -2400` is what ends funding. The only observed binding of the cap is the tick-0 bug.

### 5.5 Note on the Government's 20,000 start

`organisation_data` record 2: funds 20,000, income 0 (GAMESTATE `organisations.xml`: `ORG_GOVERNMENT bal 20000`). Every other org carries a base income field (Marsec 120,000, Solmine 130,000, Cult of Sirius 15,000, ...); the Government's is ignored by the loop (`xor esi,esi`).

## 6. Q5: bankruptcy

OBSERVED, `FUN_00060ea4` (file `0xC3548`), Ghidra decompile:

```
iVar4 = 0;
if (param_1 == 1) { if (*(int *)(DAT_0017fb4c + 8) < -5000) iVar4 = 1; }     // X-COM balance, signed
else { /* scan 16 base slots, stride 0x2BE: none active -> iVar4 = 2 */ }
if (iVar4 != 0) { FUN_00060f4c(...); DAT_000d4cbc = ...; FUN_000102fc(); FUN_000befb1(); }
```

Listing: `0c3557  cmp dword ptr [eax+8], 0xffffec78 ; jge 0xc359c ; mov edx,1`. End screen `FUN_00060f4c` (file `0xC35F0`) picks string group `0x2C` item `0x14` for reason 1 and item `0x15` for reason 2: item 20 (file `0x153F9A`) "Due to bad debts the X-COM organization has been closed down. All operations have ceased, bases dismantled and personnel discharged. ...", item 21 (`0x154069`) "All X-COM bases have been destroyed ...", titles item 17 `X-COM IS DEFEATED` (`0x153F62`) and item 18 `THE ALIENS ARE DEFEATED` (`0x153F74`).

Callers (control C1): `FUN_000941dc` at `0xF70F6` (`mov eax,1` at `0xF70F1`) and `FUN_000b3114` at `0x11581C` (`xor eax,eax`, base-slot deallocation, reason 2). So the bankruptcy test runs once per `FUN_000941dc` pass, after the overhead commit (`0xF70DD`), in Monday mode and also in the new-game passes (harmless there), and also after funding has been cut. The threshold `-5000` occurs once in the file (control C3). There is no week counter and no difficulty term.

Engine: no bankruptcy path in LIVE or HEAD. `GameLost` is raised only when the last base dies (`LIVE:game/state/city/base.cpp:155-159`; `cityview.cpp:5149` handles it). Balance is a plain `int`; overheads are applied unconditionally (`gamestate.cpp:1864`).

Observed in the 164 logged harness campaigns: balances below zero in 5; below -5000 right after a Monday in 4, all of which kept playing. The cleanest case is from the 80,000-income arm, so it is not an artefact of G4: `build-ms/learning/r0005` (income 80,000 until funding was cut for Government hostility on the Monday logged at 05:46:47; Government taken over, relation -93 to -100) went 17,336 -> **-11,364** over the Monday acked at 06:14:37 and ends at day 29.0 with balance -38,464 and `fundingTerminated` true. The three LIVE-arm cases are `learning-control-seed11/r0001` 15,107 -> -6,193 (final.json day 34, balance -6,193, `campaign_ended` null), `learning-show/r0002` 12,643 -> -15,057 on the Monday funding was terminated (continues to -70,457 at day 45) and `learning-treatment1-seed11/r0001` 8,143 -> -17,557; their balance paths are depressed by G4's 10,000 income, so under the EXE's 80,000 they would have been different. What can be said: **under the EXE's -5000 rule, balances like these end the game**; the engine lets them continue. A fifth (-2,280) does not qualify.

Fix sketch (engine_missing): after `player->balance = ... - totalSalary - basesCosts;` in `weeklyPlayerUpdate` (skip when `gameStart`), if `player->balance < -5000` raise a game-lost event with a "bad debts" reason (text above). Not blocked on RE. **Requires the G4 fix first** (see section 4.3).

Strategy rule affected: the cutoff is not "survivable at a loss". Taking the cutoff Monday as Monday 1, no income afterwards, about 22,900 of overheads and 100,000 of cash with no other spending, the balance at Monday n is 100,000 - 22,900 n, which first falls below -5,000 at n = 5, the fourth Monday after the cutoff (a week-1 cutoff can only be relational, because `H` is 0 then). Every dollar of cash and every sold item delays that by a fraction of a week.

## 7. Q6: overheads and facility costs

OBSERVED (EXE):
- **Timing:** `FUN_0004ac40` at the weekday-0 midnight transition (file `0xAD494`, `xor eax,eax` then call) and the time-skip wrapper `FUN_0004ac24` (file `0xAD2C8`, call at `0xAD2D3`, only from `0x74277`). Sequence: credit old F (inside the org loop), funding assessment, overheads, bankruptcy check; then `FUN_0006edac`, `FUN_000967a4` (market), and later `FUN_00097b90` (score rollover, file `0xAD4B8`). First Monday after a Tuesday-noon start is 5.5 days in; engine rule `days % 7 == 6` (`gametime.cpp:252-257`) gives the same.
- **Salary:** `FUN_00098a70` (file `0xFB114`): 180 agent records (stride `0xCE`), active if the byte at `+0x7D != -1` and owner byte `+0x21 == 0`; wage `200 * (byte(+0x57) + 1)`; bucketed by class byte `+0x23`. Stock templates use 2 and 3: 600 and 800 (`agent_salary` in `data/common_patch/gamestate.xml`). Whether the pay byte changes with experience is **not** established.
- **Upkeep:** 16 base records (stride `0x2BE`), 8x8 cells, a cell counts if `cell[0x16]` in 1..0x21 (continuation cells hold markers `>= 0xC9`, so multi-cell facilities count once) **and the construction byte `cell[0x56] == 0`** (file `0xFB211`); value = signed 16-bit maintenance from the 16-byte facility table at VA `0xDC016`.
- **Commit:** only `FUN_000941dc` writes the result into the balance: `[0xD892C]` has one writer (`0xFB7C1`) and one reader (`0xF70D8`). `FUN_00097b90` also calls the finance screen daily (`0xFA279`, with the flag word `[0x577E6]` = 0) but never stores the result, so overheads are not deducted daily.
- **Placement:** full build cost debited at placement (`sub [edx+8],ebx` at `0xCB1CB`); the build time is copied into the counter bytes of all four cells (`0xCB183-0xCB1A6`).
- **Cancel:** `refund = cost * counter / build_time` (`0xCC6BC-0xCC6E8`, `div ecx` with `cl = [struct+6]` = build time).
- **Countdown:** `0xCE347-0xCE359` decrements each non-zero counter by one and posts a "construction complete" message when it reaches 0; its single caller is `FUN_000105b8` at file `0x72EE6` (called from `FUN_00010010` at `0x7296C`). The loop period of `FUN_000105b8` is **not** established, so "one day per decrement" is the data-sheet reading ("Days to build"), not a measured cadence.

Engine (LIVE): same sequence; `basesCosts` gated on `f->buildTime == 0` (`gamestate.cpp:1858`); placement debit `base.cpp:333`; prorated refund `base.cpp:473-476` (`buildCost * buildTime / type->buildTime`, identical form); countdown in `updateEndOfDay` (`gamestate.cpp:1565-1572`). HEAD charges `weeklyCost` for unfinished facilities (`:1786`). OBSERVED: the starting base has no unfinished facility (upkeep 4,900 either way), so D6 matters only for facilities built during play. In the EXE the counters are zeroed when the global word `[0x10CA4]` is non-zero (`0xCB1A9-0xCB1BE`: instant build; the meaning of that flag is not established). The engine's starting facilities are placed with `free = true` (`base.cpp:83` lift, `:205` layout), so they start complete.

## 8. Per-finding detail

Finding ids match the structured return.

- **G1 funding tiers/credit/debit** (match LIVE; diverges HEAD). Sections 2.1-2.4.
- **G1b cutoff test** (match LIVE; diverges HEAD D1). `H = totalScore - weekScore` equals the EXE's previous-weeks sum because every score writer updates both; the one exception is the skirmish launcher (`skirmish.cpp:80-81, 135-136`), not used in campaigns.
- **G1c relation edge** (diverges, fidelity only). The EXE tests a signed byte `< -50` (`0xF6E9C`, `83 f8 ce 7d 08`); the engine tests a float `x < -50` (`organisation.cpp:1089`). A value of -50.5 is hostile in the engine and cannot exist in the EXE. Saves hold fractional relations (e.g. 95.0875015).
- **G2 new-game pass** (engine_bug). Section 3 and 4.
- **G3 Government cap** (match; does not bind). Section 5.3-5.4.
- **G4/G6 per-building and Government income model** (diverges, low impact). The Government path (income reset, per-building net, 10 % tax, wage floor 30, five hiring rounds, wage steps) matches the EXE; the differences are the hiring thresholds (19/29/39/48 vs literal 20/30/40/50) and the stale `org->buildings` after a base purchase. Section 5.1-5.2.
- **G5/G7 non-Government org income compounding** (diverges). Confidence medium-high: the weekly loop is observed not to write `+0xC` for non-Government orgs, but the writer scan (C5) only covered the `[0xAFB4C]` -> `+0xC` pattern for X-COM, so other writers elsewhere were not audited. EXE: `+0xC` constant; the engine adds each week's building net into the persistent field. OBSERVED in saves: `ORG_MARSEC` income 200,296 (day 1.5), 259,516 (day 7), 322,411 (day 14-17), 388,981 (day 21), 459,226 (day 31); `ORG_GENERAL_METRO` 656,115, 1,036,615, 1,428,395, 1,831,455, 2,245,795; base values in the EXE table are 120,000 (Marsec) and the like, so the EXE's weekly pay stays near base + building net (about 160,000 to 180,000 for Marsec, INFERRED from the 40,148 building net per pass implied by day 1.5 and the 59,000 to 63,000 later steps). Impact: organisation balances, and so `parkPurchaseBudget` and fleets, inflate 2 to 3 times by day 28. No effect on the Government or X-COM. Fix: use a local accumulator like the EXE (`iVar13`), keep `org->income` as the constant base.
- **G6 organisation park refill cadence** (diverges). `FUN_000962cc` (file `0xF8970`): for orgs 2..26, debit `balance * (hostile*2 + scalar)/100` up front, 50 random buys against per-type caps, refund the unspent part. EXE callers: hourly block of `FUN_0004ac40` (`0xAD50A`), the time-skip wrapper (`0xAD29A`), and twice at new game (`0x11BBE6`). Engine port (`organisation.cpp:883-985`, `buyFromParkSpawnTable`) is faithful but runs daily (`gamestate.cpp:1578`, `updateEndOfDay`). Effect: slower replacement of lost Government/organisation vehicles.
- **G7 bankruptcy** (engine_missing). Section 6.
- **G8 overheads, upkeep gate, facility costs** (match LIVE; HEAD diverges). Section 7.
- **G9 market passes inside the funding routine** (diverges, low). The EXE runs `FUN_000964d0` and `FUN_00096614` (loops over 59, 87, 49, 34 items, file `0xF8B74`, `0xF8CB8`) inside every `FUN_000941dc`: weekly, and twice at new game. The engine runs `updateItemMarket()` once at new game and weekly. The market validator independently reached the same reading (`validate/v_market.md`: `FUN_000941dc` calls `FUN_000964d0` / `FUN_00096614`, and new game runs `FUN_000941dc(1)` + `FUN_000962cc` twice). The earlier index's statement that `FUN_000941dc` is "org budgets, not the market" is incomplete.

## 9. Open leads (not resolved here)

1. Cadence of the facility construction countdown: `FUN_0006bc70` (file `0xCE314`), single caller `FUN_000105b8` (`0x72EE6`).
2. Pay byte `+0x57` of agent records: writers and whether it changes with rank.
3. Value of X-COM `+0xC` and `+8` just before the stores at `0x11BC47/0x11BC5D` (initialisation by `FUN_000b5230` / `FUN_000a57fc` from `organisation_data`); no effect on results.
4. Per-organisation scalar table at VA `0x127804` and hostile threshold `DAT_001277E4` used by `FUN_000962cc`; and what the pre-funding hooks do to Government balance (the market loops do not touch funds in the decompile).
5. The latch word `[0x10CA0]` has no clearing writer, but a bulk clear during new-game init (`FUN_000b5230`, `FUN_000a57fc`) was not excluded.

## 10. Reproduction

```
# EXE side (read-only)
cd .../scratchpad/validate/work
PYTHONPATH=/Users/khallmark/.cache/uv/archive-v0/6vCKkmsTlG51yACwfAtfC python3 xdis.py 0x11BB64 0x11BCA4   # new game
PYTHONPATH=... python3 xdis.py 0xC3548 0xC35C8                                                         # bankruptcy
python3 pool.py 0x2c 30                                                                               # end-game strings
.../ghidra/ghq.sh v_gov_income UFO2P VGov.java dec:b94c0 fx:941dc dec:60ea4                           # Ghidra confirmation
# engine side
python3 scan_saves.py          # day-by-day Government and X-COM finance fields (164 saves)
python3 funding_diff.py        # EXE vs LIVE vs HEAD differential test
```

Files: `validate/work/{scan_saves.py, saves_scan.json, saves_scan.txt, funding_diff.py, xdis.py, pool.py, ghidra1.full, ghidra2.full, ins.pkl, org_drift.json}`.
