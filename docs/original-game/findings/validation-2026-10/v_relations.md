# v_relations: organisation-relation deltas caused by the player (engine vs original EXE)

Scope: every way the player (or the aliens, in the player's presence) moves the 28x28 organisation relation matrix, compared between
the benchmark engine (LIVE snapshot of `khallmark-develop-reboot`, HEAD `fda36d6c`) and the original executables.
Binaries: `UFO2P.EXE` ISO non-4 (CRC `0x4749ffc1`, city layer) and `TACP.EXE` ISO non-4 (CRC `0xfebbe39e`, tactical layer).
Static analysis only (headless Ghidra 12.1.4 on private copies, `-readOnly -noanalysis`; plus raw-byte scans of `OGR/canonical/*.EXE` with capstone). No game launched.

Legend: OBSERVED = text/bytes/instructions read; INFERRED = my interpretation (always marked). File offsets are PHYSICAL bound-file offsets
(UFO2P code `file = VA + 0x626A4`, data `VA + 0x616A4`; TACP code `file = VA + 0x5AAA4`, data `VA + 0x4FAA4`); every function entry below was
checked against its Watcom prologue bytes in the canonical EXE (receipts in section 9). Where Ghidra's own `file=` column in my scratch logs
says otherwise (TACP rows were printed with the UFO2P delta by my script) the corrected value is used here.

---------------------------------------------------------------------------------------------------------------------------------------

## 0. Headline (events first, interpretation second)

OBSERVED
1. The original has ONE relation-delta primitive, `UFO2P FUN_0005faf0` (file `0xC2194`, args `EAX=holder A, EDX=target B, BX=signed delta`). It writes
   `m[A][B] = clamp(m[A][B] + delta, -100, 100)`, then (human city only) a **reputation ripple** to every other organisation, then mirrors X-COM's row
   (`m[0][j] = m[j][0]`) if A or B is X-COM. The engine's `Organisation::adjustRelationTo(..., applyReputationRipple)` is a faithful float port of it.
2. The deltas the engine uses for scenery hits and downed vehicles are NOT the EXE's. Decoded EXE values:
   * building hit, X-COM firer, **intentional** fire: owner->firer `-5` if the tile survives, `-25` if it is destroyed (one call, not -5 then -20).
   * building hit, **stray** fire (the vehicle is not ordered to attack that building and X-COM did not manually fire): `0` if the tile survives, `-5` if destroyed.
   * vehicle goes below its crash threshold (once per vehicle): victim org -> attacker org `-20` flat; vehicle destroyed outright: `-10` (+2 the other way when the firer is not X-COM). No hostility test.
   * "+5 (cap 95)" (index open Q2): when a tile is destroyed by a firer that is neither X-COM nor Alien, the firer's own view of the owner gets `+5` if `< 95`.
3. Alien-fired damage does not touch any X-COM relation (the written cell is `m[owner][Alien]`), only score category 6. City damage relation loss lands on the building OWNER's view of the firer; third parties only via the ripple.
4. Empty-building investigation costs the owner `-(5 + difficulty)` against X-COM (`-9` at Superhuman), in `FUN_000b426c`: identical to the engine's number.
5. Per-kill relation lives in **TACP.EXE** `FUN_00060720` (file `0xBB1C4`), not in UFO2P: victim org -> killer org `-3*S` (victim not hostile to killer) or `-S/2` (hostile); every org allied (>=75) to the victim `-S/2` toward the killer; every org hostile (<=-50) to the victim `+S/4` toward the killer; `S` = byte `unit+0x44` (INFERRED: agent-type `score`). The engine's -30/-5/-5/+2 are exactly this rule with `S = 10`.
6. There is no periodic relation drift in the EXE (complete writer census, section 4). Relations only move on events; the only X-COM-initiated increase found is the hourly "diplomatic rift" offer `FUN_00096bc8`.
7. The takeover is `UFO2P FUN_0006fd9c` (file `0xD2440`; the lab's C2 note called it "new organisation founding"): human city only, called every 5 game minutes (FUN_0004ab04, same tick that drives the 24-72 h invasion timer), one candidate org per call (cursor `[0xF3DB4]`, 2..0x3D, so each real org is examined once per 60 calls = 300 game minutes). It takes the org over iff `rand16(0..100) < infiltration - 100` (`infiltration` = word-array at VA `0x13F4C8`, stride 0xE0, range 0..200). The relation effect equals the engine's `takeOver` (-100 both ways with X-COM, +100 both ways with the Aliens, 90 with other taken-over orgs) but the trigger does NOT: the EXE cannot take over any org with infiltration <= 100 (displayed 50%), reaches 49.5% per attempt at 150, 99% at 200; the engine rolls `rand[0,200) < infiltration` every 125 minutes (50% per attempt already at 100, 25% at 50).

INFERRED (clearly separate)
* The engine over-punishes stray hits on buildings during dogfights (-5 per stray hit that the EXE charges 0) and under-rewards UFO kills (Government +0.6 vs +2). Both push the Government relation the wrong way for a "keep Government friendly" strategy.

---------------------------------------------------------------------------------------------------------------------------------------

## 1. Method

1. Read `exe_evidence_index.md` sections 3, 5.3, 7, 8, 9, 10 and `income_engine_exe_check.md` 3b. Reused the lab's prior raw decompile `OGR/export/city_damage_00054a28_raw.c`.
2. Private Ghidra copies `v_relations` (UFO2P) and `v_relations_2` (TACP). Own scripts in the copy dirs: `RelAll.java` (full decompile + listing, call-site windows `calls:<fn>:<back>:<fwd>`), `OperandScan.java` (scan initialised memory for 32-bit values in a range, report containing instruction/function, as Ghidra shows them with fixups applied), `TacScan.java` (idiom census). 12 UFO2P runs + 6 TACP runs (each ~10 s; the "2 minutes" in the brief did not apply).
3. Completeness controls (the brief asks for a control when claiming absence):
   * Raw `E8/E9` scan of the whole UFO2P code object for calls to `0x5FAF0` finds exactly the 17 known call sites and no others (control: all 17 known sites are recovered; so the 17 are complete for direct calls). Same scan: `FUN_0005faa8` has 2 callers, `FUN_00096bc8` 2, `FUN_00099130` 1, `FUN_000ac348` 1, `FUN_000b426c` 1.
   * Operand scan for the matrix (VA `0x16EC20..0x16EF38`) and its two shadow arrays (`0x16F070..0x16F3A0`) over the whole code object including undisassembled regions: 400 hits across ~45 functions plus undisassembled code. Every hit was classified read/write (section 4). Found two relation writers the lab's census missed (`FUN_0007abf0`, `FUN_00096bc8`), one more direct writer inside `FUN_00054a28` and the launcher `FUN_000ac348` (the latter was already in `income_engine_exe_check.md`).
   * Same technique on TACP found the matrix at VA `0x1D3428` (28 bytes per row) and its complete writer set (section 5).
4. A reference model of the primitive (`validate/_sim_exe_relations.py`, port of the listing) was run on the EXE's starting matrix (file `0x189040`, Superhuman bias applied to the X-COM column, zero jitter) to turn deltas into per-event effects on Government/Megapol/Cult views of X-COM (section 7 table).

---------------------------------------------------------------------------------------------------------------------------------------

## 2. The primitive, `UFO2P FUN_0005faf0` (file `0xC2194`)

OBSERVED listing (VA, abridged; holder `A=SI`, target `B=DI`, delta `BX`):

```
0005fb1b TEST SI,SI ; JZ 5fc30         ; A == X-COM(0): no change
0005fb24 CMP SI,DI  ; JZ 5fc30         ; A == B: no change
0005fb2d CMP SI,4   ; JZ 5fb46         ; A == Cult of Sirius -> test B
0005fb33 IMUL ECX,EDX,0x1b6 ; CMP byte [orgs+ECX+0x11],0 ; JZ 5fb50   ; org A flag +0x11 clear -> apply
0005fb46 CMP DI,1 ; JZ 5fc30           ; (Cult, or A flagged) and B == Alien: skip
0005fb5c CMP EDX,0x64 ; ... MOV byte [EAX],0x64 / 0x9c / ADD byte [EAX],DL    ; clamp the SUM to [-100,100]
0005fb76 CMP byte [0xd5060],0 ; JNZ 5fc30                                       ; ripple only in the human city
loop k=0..26, skip k==A, k==B, k==X-COM(0), (k==4 or flagA) && B==Alien:
   r  = m[k][A]            ; af = trunc(r/2); if af<0: af = trunc(af/2)
   rip = trunc(af*delta/100) ; n = m[k][B]+rip ; if -100 < n < 100: m[k][B] = n     ; boundary => silently dropped
0005fc30 if (B==0 || A==0) CALL 5faa8 (mirror)
```
`FUN_0005faa8` (file `0xC214C`): forces `m[0][1] = m[1][0] = -100`, then `m[0][j] = m[j][0]` for j=0..26. OBSERVED.

Org record flag `+0x11` is the **taken-over flag** (INFERRED, strongly: `FUN_0007abf0` sets it, `FUN_00096bc8` refuses to make rift offers to flagged orgs, the primitive protects `m[flagged][Alien]`).

Engine (LIVE `game/state/shared/organisation.cpp:1020-1076`, identical at HEAD): same structure with floats: `allyFactor = rel/2; if (<0) /=2; ripple = allyFactor*value/100`, skip if the result is outside (-100,100); mirror of X-COM's row at the end.

Differences (OBSERVED code vs OBSERVED code):
| aspect | EXE | engine |
|---|---|---|
| arithmetic | integer, `IDIV` truncates toward zero; `rel/2` and `af/2` truncate | float, no truncation. Small events: EXE ripple is 0 whenever `|af*delta| < 100` (e.g. `af=19, delta=-5` -> 0; engine -0.95) |
| ripple city | human city only (`[0xD5060]==0`) | any city, only suppressed during a battle |
| who ripples | every call site (all 17) | only callers passing `true` (scenery, vehicle downed). Investigation (`buildingscreen`), incidents, bribes pass `false` |
| ripple targets | k = 0..26 (not X-COM(0), not Civilian(27)) | all orgs except holder, target, player (includes Civilian) |
| holder X-COM | `A==0` is a no-op (X-COM's row is only the mirror of what others think) | `adjustRelationTo` on player is allowed but then overwritten by the mirror: equivalent |
Verdict: `diverges` (low) for the primitive itself.

---------------------------------------------------------------------------------------------------------------------------------------

## 3. Per-site decode of the 17 call sites of `FUN_0005faf0`

Delta register `EBX`, holder `EAX`, target `EDX` (Watcom `regparm3`). File offsets of the CALL instruction.

| # | caller | CALL file | A (holder) | B (target) | delta | trigger (OBSERVED) |
|---|---|---|---|---|---|---|
| 1 | `FUN_00054a28` (projectile hit, vehicle branch) | `0xB8141` | victim vehicle org | firer org | **-10** (`BB F6FFFFFF` @ `0xB8132`) | victim slot freed by this hit (outright kill); victim org != firer org |
| 2 | `FUN_00054a28` | `0xB815A` | firer org | victim org | **+2** (`BB 02000000` @ `0xB8151`) | same hit, only if firer org != X-COM |
| 3 | `FUN_00054a28` (scenery branch) | `0xB8A6A` | building owner (`bldg+0xC8`) | firer org | **-25** (`BB E7FFFFFF` @ `0xB8A5B`) | tile destroyed AND intentional fire; owner != firer |
| 4 | `FUN_00054a28` | `0xB8AD5` | building owner | firer org | **-5** (`BB FBFFFFFF` @ `0xB8ACA`) | tile survived + intentional, OR tile destroyed + stray; owner != firer |
| 5 | `FUN_00057c8c` (vehicle damage applier) | `0xBA913` | victim org | attacker org | **-20** (`BB ECFFFFFF` @ `0xBA8FC`) | health crosses below crash constitution on this hit (was not already below); projectile's firer vehicle still alive |
| 6 | `FUN_00041644` (vehicle/scenery obstruction) | `0xA4216` | building owner | **Alien (hard-coded 1)** | **-25** (`BB E7FFFFFF` @ `0xA420B`) | flying vehicle in collision state 5 clears a building tile; owner != 1 |
| 7 | `FUN_000b426c` | `0x116986` | building owner | X-COM (0) | **-(5+difficulty)** (`B8 FBFFFFFF; SUB EAX,diff` @ `0x11696D`) | mission start (`FUN_000ac08c`), kind 0, human city, building has no alien population (13 species bytes at `+0xD4` all zero) |
| 8 | `FUN_000b32ac` (post-battle) | `0x115A29` | 24 (Mutant Alliance) | 1 (Alien) | -50 | alien-city event `[0x244048]==2` (Food Chamber) |
| 9, 10 | `FUN_000b32ac` | `0x115B42`, `0x115E13` | building owner | X-COM (0) | tiered by `worth` N (section 6b) | mission types 1 and 4, human city |
| 11-17 | `FUN_00092470` (scripted incidents) | `0xF4C86 0xF4D6D 0xF4E5B 0xF4F44 0xF5032 0xF5326 0xF5454` | victim org (or X-COM = no-op) | raider org | -5, -25(A=0), -15, -25(A=0), -25, -25 (conditional), `-50 - m[B][A]` | org-vs-org raid incidents; `0xF4D6D` and `0xF4F44` have A = X-COM and are no-ops |

Count check: 4 + 1 + 1 + 1 + 3 + 7 = 17 = the raw `E8` census.

Decoded condition tree for the scenery sites (VA, OBSERVED, abridged):

```
5605c CMP byte [0xd5060],0 ; JNZ 5645d              ; alien city: whole relation+score block skipped
56063..  find building by collision XY (building records stride 0xE2, bounds +2/+4 x, +6/+8 y)
56121 firer = [projectile+0x15a]; if firer slot == -1: skip block
56140 local_d0 = [firer+0x12c] (firer org) ; local_124 = byte [building+0xC8] (owner)
5615f CMP DI,SI : building idx vs word [firer+0x15a] (firer's target building)  -> equal AND byte [firer+0xCF]==2  => INTENTIONAL
5616d else: firer org == 0 AND [0xD4E28]==9 AND firer == player's selected vehicle                                   => INTENTIONAL (manual fire)
        else -> 563fc : if owner != firer and tile_destroyed: CALL prim(owner, firer, -5)   ; STRAY
56383 INTENTIONAL: if owner == firer: nothing
      tile_destroyed==0 -> 56418: prim(owner, firer, -5)
      tile_destroyed!=0 -> prim(owner, firer, -25) ; if firer not in {0,1}: if m[firer][owner] < 95: m[firer][owner] += 5
```
`tile_destroyed` = result of `FUN_0005bd08` (randomised damage vs constitution at tile record `+0x0B`; 0xFF = indestructible), saved at VA `0x55FEF`. The category-6 score call at `0x56458` is after all of this and gated only by firer org in {0,1} (already known).

---------------------------------------------------------------------------------------------------------------------------------------

## 4. Complete matrix-writer census (UFO2P)

Raw operand scan, every hit classified. WRITE sites (matrix `m` or shadows `0x16F078` / `0x16F388`):

| function | file | what it writes |
|---|---|---|
| `FUN_0005faf0` / `FUN_0005faa8` | `0xC2194` / `0xC214C` | the primitive and the X-COM-row mirror |
| `FUN_00054a28` | `0xB70CC` | directly: `m[0][firer org] = -50` (VA `0x55BF1`, file `0xB8295`) when an X-COM vehicle is hit by a vehicle of an org X-COM does not consider hostile (X-COM's own view only; later overwritten by the mirror) |
| `FUN_000ac348` | `0x10E9EC` | mission launcher: types 3/4 force X-COM's row cell toward the target org to `-60` if above (and the shadow) |
| `FUN_000b32ac` | `0x115950` | `m[24][0]` and `m[0][24]` set to 100 on the Food Chamber event; final `CALL 5faa8` (mirror) on every exit |
| `FUN_00092470` | `0xF4B14` | second element of each incident: `m[B][A] += 2/7/12` directly |
| `FUN_00092e10` | `0xF54B4` | relation-matrix screen; cycles ONE cell of X-COM's row (-100 -> -49 -> 0 -> 50 -> 100) and its shadow (a row-0 editor; row 0 is overwritten by the mirror) |
| `FUN_00096bc8` | `0xF926C` | hourly diplomatic-rift offer: accept path sets `m[org][0]` (section 8) |
| `FUN_00099130` | `0xFB7D4` | new-game difficulty init of the whole matrix (section 9) |
| `FUN_0006fd9c` | `0xD2440` | **alien takeover event** (the lab called it new-organisation founding): sets the org flag, infiltration := 200 and the -100/+100/90 relations; section 8c |
| `FUN_0007abf0` | `0xDD294` | second takeover / liberation writer (undisassembled in Ghidra; caller unresolved) |
| region around VA `0xBA8A8` (undisassembled) | `0x11CF4C` | new-game matrix clear/diagonal init (`MOV EAX,0x16EC28 ... [..]=0x64`) |
| `FUN_000a4054` / `FUN_000a3234`, `FUN_00014854` | | state load/save and shadow resync |

READ-ONLY users (hostility/alliance tests etc.): `FUN_00091db0`, `FUN_00091de4`, `FUN_00092060`, `FUN_000962cc`, `FUN_00041644` (copies a cell to the shadow), `FUN_000941dc` (weekly funding reads `m[2][0]`), `FUN_0006cfb4`, the AI/targeting helpers `FUN_00049f7c/4e948/50470/5074c/510d0`, the relation UI screens `FUN_000978c0/97abc/97b14/9379c/93a30/93c84/93dd8/93f24`, and the undisassembled pieces at VA `0x714E3`, `0x7153D`, `0x7AC78`, `0x95397`, `0x95409`, `0x9729B-0x97803`. None of them is periodic.

Result (OBSERVED, with the controls above): **no hourly/daily/weekly writer drifts relations toward a mean.** The only periodic writer is `FUN_00096bc8` (hourly, one organisation per call) and it only acts after a fall of more than 15. Engine `Organisation::updateRelations` (`organisation.cpp:694-707`) is a snapshot only (it never writes `current_relations`): **match**.

---------------------------------------------------------------------------------------------------------------------------------------

## 5. TACP.EXE: the per-kill relation rule, `FUN_00060720` (file `0xBB1C4`)

OBSERVED: TACP holds its own copy of the matrix at VA `0x1D3428` (file `0x222ECC`), loaded by `FUN_00014CAC` (file `0x6F750`) and written back by `FUN_00015494` (file `0x6FF38`, `size 0x310`), so tactical changes persist into the city layer (UFO2P `FUN_000B1DC0` -> `FUN_000A4054` loads them). TACP matrix writers (operand scan `0x1D3420..0x1D3740`, all hits classified): `FUN_00060720` (kills), `FUN_0005D25C` (file `0xB7D00`; at VA `0x5E09B` stores `-50` into the X-COM row when an X-COM unit is shot by a non-hostile org: INFERRED same purpose as the UFO2P direct write), `FUN_00021924` (debug cell cycler: 100/-45/0/-100). Everything else is a reader (`FUN_00021998` hostile test, `FUN_000219cc` allied test, AI scorers).

Helpers (OBSERVED):
* `FUN_00021998(a,b)` (file `0x7C43C`): `m[a][b] <= byte[0x2818B8]`; byte at file `0x2D135C` = `0xCE` (-50). Decompiler folds it to `m < -0x31`. **Hostile = relation <= -50.**
* `FUN_000219cc(a,b)` (file `0x7C470`): `m[a][b] >= byte[0x2818B9]`; byte at file `0x2D135D` = `0x4B` (75). **Allied = relation >= 75.**
* UFO2P has the same two bytes: file `0x188E88` = `0xCE`, `0x188E89` = `0x4B` (`FUN_00091db0`, `FUN_00091de4`).

`FUN_00060720(AX = victim unit idx, DX = killer unit idx or -1)` after reading org ids (`victimOrg = word[unit+0x20]`, `killerOrg = word[killer+0x20]`, `S = byte[victim+0x44]`):

```
if killer == -1: no relation change at all (fire, bleeding, many explosions, accidents)
(team score)  killer hostile to victim (m[k][v] <= -50)  ? +S to kill score : -S
if killerOrg == victimOrg: stop
if victimOrg != 27 (Civilian):
    victim hostile to killer (m[v][k] <= -50) ? m[v][k] -= floor(S/2)   : m[v][k] -= 3*S       ; floor at -100
for o = 0..27, o != victimOrg, o != 27:
    if m[o][v] >= 75  : m[o][k] -= floor(S/2)           ; floor at -100   (allies of the victim)
    elif m[o][v] <= -50: m[o][k] += floor(S/4)          ; cap 100         (enemies of the victim)
```
(VA `0x608C3-0x60AB8` in the listing; constants: `IMUL AH*3` at `0x609ED`, `SAR 1` at `0x609B1/0x60A3E`, `SAR 2` at `0x60A88`.)

Engine (LIVE `game/state/battle/battleunit.cpp:5075-5101`, identical at HEAD): `-5` if `Hostile` else `-30`; allies (`Allied`) `-5`, enemies (`Hostile`) `+2`. These equal the EXE for `S = 10` (10/2=5, 3*10=30, 10/4=2). Agent-type scores in the generated gamestate: all human NPC/agent types 10; Brainsucker 4, Chrysalis 3, Hyperworm 5, Multiworm 8, Popper 8, Spitter 8, Anthropod 10, Skeletoid 14, Micronoid 15, Megaspawn 30, Psimorph 30, Queenspawn 50 (`agent_types.xml`).

Divergences (OBSERVED code vs OBSERVED code unless marked):
1. Constants scale with `S` in the EXE (INFERRED `S` = agent-type score because the same byte feeds the team score that the engine takes from `agent->type->score`). Killing a Skeletoid (14) with Government hostile to Aliens gives Government->X-COM `+3` (EXE) vs `+2` (engine); a Queenspawn `+12` vs `+2`.
2. Same-org kill (friendly fire): EXE stops; engine does not (section 10, F12).
3. Victim Civilian: EXE skips the direct term (but runs the ripple loop); engine applies the direct -30/-5 to Civilian->killer (harmless) and then the loop.
4. Threshold boundary: EXE hostile `<= -50`; engine `Hostile` is `< -50` (floats).
5. Order: EXE tests "allied" first then "hostile"; engine "hostile" first (mutually exclusive, no effect).

---------------------------------------------------------------------------------------------------------------------------------------

## 6. Post-battle relation hit and raid launch

### 6a. Launch (`FUN_000ac348`, file `0x10E9EC`, called only from `FUN_000ac08c`, file `0x10E730`)
OBSERVED mission kinds (`ac08c(EAX=object idx, EDX=kind)`): kind 0 = "Investigate Building" button (handler VA `0x713E4`, file `0xD3A88`; `EDX` is 0 at the call, VA `0x71503`); kind 3 = "Raid Building" button in the human city (handler VA `0x71820`, file `0xD3EC4`, call at VA `0x7186B`; in the alien city the same button passes kind 0); kinds 1 and 5 come from UFO arrival `FUN_0003a910` (crash site / UFO at an X-COM base); kind 2 is a vehicle event, and `ac08c` itself rewrites 2 -> 4 when the event record (`[DAT_00170ebc + idx*10]`) has type 3. Kind 4 is therefore org/event-initiated, reached from the 5-minute tick (`FUN_0004aa7c` -> `FUN_00099e04`, which fires when the incident dispatcher sets flag `[0x127798]`), not from a player button (INFERRED, medium confidence).
OBSERVED effects in `FUN_000ac348`:
* kind 3: reads `m[0][owner]` (`dword[0x16EC25+owner]>>24`); if it is above -60 stores `0xC4` (-60) into `m[0][owner]` and the shadow (VA `0xAC5C6..`, listing in `income_engine_exe_check.md` 3b: file `0x10EC6A/0x10EC71`). **X-COM's row only.** The post-battle handler `FUN_000b32ac` ends with an unconditional `CALL FUN_0005faa8` (mirror) that rewrites `m[0][j] = m[j][0]`, so this -60 is transient (INFERRED from the end-of-function call; the lab doc says "persistent").
* kind 4: `if (m[org][0] > -60) { m[org][0] = -60 }` (address `0x16EC25 + org*0x1c`, i.e. the ORG's own view of X-COM, column 0, which the mirror does NOT rewrite). So kind 4 is a persistent "org attacks X-COM -> its relation to X-COM is at most -60". Because kind 4 is event-initiated (org attacking an X-COM base), the player cannot trigger it with a button; it is a downstream effect once an org is already hostile. The engine has no counterpart (an org raid on an X-COM base only raises `DefendTheBase`).
`RaidHostileAction` (-200, `buildingscreen.cpp:248-250`, HEAD `:309-311`, default off) has no EXE basis.

### 6b. Tiered hit from `FUN_000b32ac` (file `0x115950`), mission types 1 and 4, human city
OBSERVED:
```
N = dword[0x244090]                        ; "worth"
owner.balance (org record +8) -= 50*N      ; stored back if non-zero
delta = 0
if N > 50:
    if N < 251: delta = -((N-50)/20)                    ; -1 .. -10
    else:       delta = max(-50, -10 - (N-250)/50)
if delta != 0: prim(owner, X-COM, delta)
```
Only UFO2P writers of `[0x244090]` are the mission-record initialiser `FUN_000aff9c` (stores 0, file `0x112923`) and an undisassembled twin (`0x11E9EA..`, stores 0). The record is round-tripped through TACP (state file), so TACP may write it. I could not locate the TACP alias (the `0x1ED294` block in TACP is a different record). **`exe_unbound`**: if `N` is ever non-zero the original charges a casualty/collateral-scaled relation hit that the engine has no equivalent of; if it is always 0 the whole branch is dead and the engine is right. Most promising next lead: dump TACP `FUN_00015494`'s four chunks and diff against UFO2P's post-battle reader order; or search TACP for stores into the chunk that holds the mission score (UFO2P field `0x24408C`, read first in `FUN_000b32ac`).

### 6c. Food Chamber event (same function)
OBSERVED: when `[0x244048] == 2` in the alien city: `m[24][0] = max(m[24][0], 100)`, `m[0][24] = m[24][0]`, `prim(24, 1, -50)`. INFERRED: alien building index 2 is the Food Chamber; Mutant Alliance becomes friendly. The engine has no such rule (grep of `game/` finds Mutant Alliance only in the infiltration screen). This contradicts the "NOT BOUND" verdict of `C2-secondary-objectives.md` 2.5 for the building-destroyed trigger (the Sectoid-rescue trigger is still not seen).

---------------------------------------------------------------------------------------------------------------------------------------

## 7. Effect table (EXE primitive run on the starting matrix, Superhuman bias, zero jitter, human city)

Start: Government->X-COM 65, Megapol->X-COM 65, Cult->X-COM -60 (`_sim_exe_relations.py`). "dGov" = change of Government->X-COM caused by the event. Engine column = what LIVE does for the same event (float model, hand-computed).

| event (X-COM is the firer / killer) | EXE: owner->X-COM | EXE: dGov (when owner is not Gov) | engine: owner->X-COM | engine: dGov (not owner) | verdict |
|---|---|---|---|---|---|
| intentional hit on Gov bldg, tile survives | -5 | (is owner) | -5 | (is owner) | match |
| intentional hit on Gov bldg, tile destroyed | **-25** | (is owner) | -25 (-5 then -20) | (is owner) | match in total |
| **stray** hit on Gov bldg, tile survives | **0** | (is owner) | **-5** | (is owner) | engine over-punishes |
| **stray** hit on Gov bldg, tile destroyed | **-5** | (is owner) | **-25** | (is owner) | engine over-punishes x5 |
| intentional hit on Megapol bldg, survives / destroyed | -5 / -25 | **-1 / -9** | -5 / -25 | -1.875 / -9.375 | match up to truncation |
| stray destroy of a Megapol tile | -5 | -1 | -25 | -9.375 | engine over-punishes |
| X-COM downs a Gov vehicle (crash) | **-20** | (is owner) | -30 (-5 only if Gov already hostile) | (is owner) | diverges |
| X-COM destroys a Gov vehicle outright | **-10** | (is owner) | -30 (again, unless `crashed`) | (is owner) | diverges |
| X-COM downs a Megapol vehicle | -20 | -7 | -30 | -11.25 | diverges |
| X-COM downs a UFO | n/a | **+2** (Megapol +2, Cult -10) | n/a | **+0.625** (Megapol +0.625, Cult -2.5) | diverges (~3-4x) |
| X-COM destroys a UFO outright | n/a | +1 | n/a | +0.625 | diverges |
| investigate empty Gov building (Superhuman, d=4) | -9 | (is owner) | -9 | (is owner) | match |
| investigate empty Megapol building | -9 | **-3** | -9 | 0 (no ripple) | ripple missing |
| alien fire hits a Gov building | `m[Gov][Alien]` only | 0 | same cell | 0 | match |

Engine numbers from `validate/_sim_engine_relations.py` (float model of `adjustRelationTo`), EXE numbers from `validate/_sim_exe_relations.py` (integer model of the listing), both on the EXE starting matrix with the Superhuman -10 X-COM-column bias and zero jitter (Gov->X-COM 65).

Numbers of events to reach the funding cutoff (`< -50`, from 65, i.e. -116 total): EXE: 5 intentional destroyed tiles or 24 intentional surviving hits or 24 stray destroyed tiles or 6 downed Gov vehicles; engine: 5 destroyed tiles (any intent) or 24 hits (any intent, stray included) or 4 downed Gov vehicles (-30 each).

---------------------------------------------------------------------------------------------------------------------------------------

## 8. Rift offers, bribes, takeover

### 8a. Diplomatic rift offer, `FUN_00096bc8` (file `0xF926C`) - BOUND now (index called the bribe/rift formula NOT BOUND)
Called from the hourly handler `FUN_0004abd4` (file `0xAD278`; human city only) and from `FUN_0004ac40`. One organisation per call (round-robin cursor `[0x127998]`, 27 organisations => each org is examined every 27 game hours). OBSERVED conditions: `snapshot[org] - m[org][0] > 15` (snapshot byte array at `0x16F388+org*28`, refreshed at the end of every visit), `m[org][0] < 25`, org flag `+0x11 == 0`, `org != 1`, `rand16(0..100) > 10*(difficulty+1)` (49.5% at Superhuman). Offer `= max(1, -m/2)*(d+1) * clamp(|org.balance|,1000,400000)/1000 + 1000*(d+1) + rand(0..2000*d+4000)`. If X-COM balance < offer: "insufficient funds" dialog. Accept: X-COM -offer, org +offer, `m[org][0] = (m<0) ? 0 : m+25` (VA `0x970ff-0x97118`, file `0xF97A3-0xF97BC`).
Engine: `gamestate.cpp:1585-1596` (daily), `organisation.cpp:1164` (`diplomaticRiftOffer`): conditions and cost formula **match** (cadence daily vs per-org 27 h). Resolution `signTreatyWith` (`organisation.cpp:1230`): `myRelation > 0 ? max(myRelation+25, 100) : 0` -> any positive relation jumps to 100 (alliance); the EXE gives `m+25` (and offers only occur below 25). `engine_bug`.

### 8b. On-demand bribe
No UFO2P function writes the matrix with a cost term other than 8a (writer census, section 4; all 17 primitive call sites listed in section 3 have none). The engine's `BribeScreen` / `costOfBribeBy` / `bribedBy` (`bribescreen.cpp`, `organisation.cpp:1100-1160`) has no EXE writer. The 8 bribe UI strings exist but my census finds no consumer: **`exe_unbound`** (the original may not have an on-demand bribe at all; or it goes through a path I could not see, e.g. an indirect call). A strategy that depends on buying Government relation on demand is unsafe until this is settled.

### 8c. Takeover: the real trigger is `FUN_0006fd9c` (file `0xD2440`)
OBSERVED (listing + decompile, VA):
```
6fda2 CMP byte [0xD5060],0 ; JNZ exit                  ; human city only
6fdaf cand = [0xF3DB4] ; if org[cand].flag(+0x11) != 0 or cand >= 27: skip to cursor advance
6fde5 EDX = infiltration[cand] - 100                   ; word array at VA 0x13F4C8, stride 0xE0
6fdeb rand16_inclusive(100) ; CMP rand,EDX ; JGE skip  ; take over iff rand(0..100) < infiltration-100
6fe29 infiltration[cand] = 200 (0xC8) ; org[cand].flag(+0x11) = 1
      m[Alien][cand] = 100 ; m[cand][Alien] = 100 ; m[cand][X-COM] = -100 ; m[X-COM][cand] = -100
      for k in 0..26 with k != cand and org[k].flag != 0: m[cand][k] = m[k][cand] = 90 (0x5A)
      then the modal "ALIEN TAKEOVER" dialog (string group calls via FUN_00063A00)
cursor: [0xF3DB4] += 1 ; if > 0x3D: = 2                ; 60-step cycle, only slots < 27 act
```
Called once per `FUN_0004ab04` (file `0xAD1A8`), which `FUN_0004aa7c` calls when its minute counter `% 5 == 0`. Cross-check (OBSERVED arithmetic): the same `FUN_0004ab04` subtracts `0x2A30` (10 800) from the invasion timer per call and the timer is reloaded with `0x2F7600 + ...` ticks; `0x2F7600 / 0x2A30 = 288` calls = 24 h, so one call = 5 game minutes. Therefore each of the 25 real organisations (indices 2..26) gets one takeover roll per 300 game minutes.
Engine: `Organisation::takeOver` (`organisation.cpp:84-108`, HEAD identical): `rand[0,200) >= infiltration -> return` (P = infiltration/200), attempted every 125 min per org (`organisation.h:19`, `gamestate.cpp:1424-1437`); relation effects identical to the EXE list above.
Verdict: effect `match`; trigger `diverges` (critical for the kill chain: P_EXE = max(0, inf-100)/101 per 5 h versus P_engine = inf/200 per 2.08 h, i.e. at inf=100 the engine takes over with 50% per attempt and the EXE never; at inf=150 the per-hour hazard is 0.36 (engine) versus 0.099 (EXE)).
Second takeover writer `FUN_0007abf0` (file `0xDD294`, undisassembled, entry `53 51 52 56 57 55 83 EC 04 89`): same flag and the same -100/+100/90 values for the org's row (without the column writes or the infiltration reset), plus a liberation branch (flag := 0, `m[org][Alien] = -50`, `m[org][X-COM] = +50`) decided by comparing two sums over a 180-record table (VA `0x144918`, stride 0xCE; org at `+0x21`, class flag `+0x20`, value `+0x22 > 4`). Its only caller is `FUN_0007acfc` (file `0xDD3A0`), which has no `E8/E9/Jcc` caller and no absolute or object-relative pointer in the image (the lone `0x6ACFC` hit at file `0x116E5B` is the displacement of a data table, not a pointer). Either dead code or reached by a computed jump; unresolved (lead).

### 8d. Org-vs-org incidents (`FUN_00092470`)
OBSERVED: case 1/2/3: `prim(victim, raider, -5/-15/-25)` then `m[raider][victim] += 2/7/12`. Engine `Organisation::RaidMission::execute` (`organisation.cpp:1276-1340`): -5/+2, -15/+7, -25/+12: **match**. The `-25` "X-COM base found" path calls `prim(A=0, ...)` because the owner byte is 0: a no-op for relations (mirror only); the flag at VA `0x127798` triggers the ticker. Engine: base owner path pushes `DefendTheBase` with no relation change: **match**.

---------------------------------------------------------------------------------------------------------------------------------------

## 9. Starting relations (FUN_00099130, file `0xFB7D4`) and receipts

For every cell `a in 1..26, b in 0..26`, template `t` (file `0x189040`, 28x28 int32) not equal to 100: `v = t + rand(0..20) - 10`; if `b == 0` and `d != 0`: `v += 10 - 5d`; else if `d != 0`: `v > 0 ? v += rand(0..3d) : (b != 1) ? v -= rand(0..5d)`; clamp +/-100; store to `m` and to the rift baseline `0x16F388`. Engine `gamestate.cpp:615-660` is the same recipe with two differences: the sign test uses the un-jittered value and jitter is applied last; and `d == 0` is handled identically (no bias). Superhuman Government->X-COM: 75 - 10 +/- 10 = 55..75: match. Boundary: Government->Alien template -50 +/-10, hostile for 11/21 seeds in the EXE (<= -50) vs 10/21 in the engine.

Receipts (canonical EXE, physical offset, first bytes):
UFO2P: `FUN_0005faf0` `0xC2194` `5156575583ec08`; `FUN_0005faa8` `0xC214C`; `FUN_00054a28` `0xB70CC` `535152565755`; `FUN_00057c8c` `0xBA330` `56575581ec8c`; `FUN_00041644` `0xA3CE8`; `FUN_000b426c` `0x116910`; `FUN_000b32ac` `0x115950`; `FUN_000ac348` `0x10E9EC`; `FUN_00096bc8` `0xF926C`; `FUN_00099130` `0xFB7D4`; `FUN_0007abf0` `0xDD294`; `FUN_0007acfc` `0xDD3A0` `52c680c3000000`; `FUN_0004abd4` `0xAD278`.
TACP: `FUN_00060720` `0xBB1C4` `5351565755` + `83ec14`; `FUN_00021998` `0x7C43C`; `FUN_000219cc` `0x7C470`; `FUN_0005d25c` `0xB7D00`; save `0x6FF38`; load `0x6F750`.

---------------------------------------------------------------------------------------------------------------------------------------

## 10. Findings (engine vs EXE)

Engine references are LIVE; HEAD differs only by line offset (scenery.cpp -9/-10, vehicle.cpp -25, buildingscreen.cpp +61); the relation code is textually identical.

F01 relation primitive and ripple - diverges (low). Section 2.
F02 scenery hit by X-COM, intentional vs stray - diverges (critical). Engine `scenery.cpp:1197-1226`: relation `-5` for EVERY hit when `!type->commonProperty && building && firerVehicle` (`:1215`), `intentional` (`:1216-1219`) only gates `building->underAttack`; destroying hit adds `-20` (`:1268`). EXE section 3 rows 3-4.
F03 third-party firer `+5` to firer->owner (cap 95) - engine_missing (low; irrelevant for X-COM firers).
F04 owner-only / alien-fire - match. Direct cell is `m[owner][firer]`; alien firer changes `m[owner][Alien]` only (no X-COM relation); score charged separately (category 6, already known).
F05 vehicle downed / destroyed - diverges (high). Engine `vehicle.cpp:2115-2140` with callers `:2026 die(!crashed)`, `:2043 crash`, `:2088 startFalling`: -5 if victim already hostile, else -30, up to twice. EXE: -20 once at crash threshold; -10 outright (+2); no hostility test; a crashed vehicle later killed gets -10 again (engine skips when `crashed`).
F06 positive Government effect of UFO kills - diverges (medium-high). EXE ripple +2 per UFO downed, +1 per outright kill (Government dislikes Aliens, af=-12); engine +0.625 (value -5, float). Also Cult -10/-5.
F07 vehicle/scenery obstruction - engine_missing (low): `FUN_00041644` charges owner->Alien -25; no X-COM relation effect.
F08 empty building investigation - match on delta (-(5+d)), on the three message variants and on "no battle launched"; diverges only on the missing ripple (low). Engine `buildingscreen.cpp:167-195`. EXE: relation hit in `FUN_000b426c` (kind 0 only; its `TEST ECX,ECX; JNZ` returns for kinds 2 and 4), message selection in handler VA `0x713E4` from the tier function `FUN_00093C48` (file `0xF62EC`) before/after: strings group 0x1D items 0x1F/0x20/0x21 consumed at VA `0x715B9/0x7160A/0x7164E`, title "No Hostile Forces Discovered" group 0x25 item 0x14C at VA `0x715CF/...` (control: same resolver scan finds "Damage to City" group 0x25 item 0x9A at VA `0x98120`).
F09a raid launch (kind 3) - match in effect (transient X-COM-row -60; the lab doc said "persistent"). F09b kind 4 (org-initiated) persistent `m[org][0] <= -60` - engine_missing (low; not player-triggerable). `RaidHostileAction` -200: no EXE basis.
F10 post-battle tiered hit with `worth` - exe_unbound / engine_missing, blocked on RE.
F11 per-kill rule - diverges (medium): constants scale with unit score S; engine fixed at S=10.
F11b victim counted as its own ally - engine_bug (medium): the loop at `battleunit.cpp:5089-5101` excludes only the attacker and Civilian, so the victim org passes `isRelatedTo(self)` = 100 = Allied and takes an extra -5 toward the killer on every kill (engine victim total -35 not hostile / -10 hostile, EXE -30 / -5; EXE skips `o == victimOrg`, `CMP BX,[ESP+8]` at VA `0x60A02`, file `0xBB4A6`).
F12 friendly fire (killerOrg == victimOrg) - engine_bug (low-medium): engine `battleunit.cpp:5075-5101` still runs the allies/enemies loop with the player as "attacker": orgs allied (>=75) to X-COM lose 5 toward X-COM, orgs hostile to X-COM gain 2, per X-COM-on-X-COM kill. EXE returns before any relation change.
F13 hostile/allied/tier thresholds and funding strictness - diverges (low): EXE hostile `<= -50`, allied `>= 75` (bytes `0xCE`/`0x4B`); UI tier function `FUN_00093C48` (file `0xF62EC`): Allied >= 75, Friendly >= 25, Neutral >= -24, Unfriendly >= -49, Hostile <= -50 (so -25 is already Unfriendly); funding cutoff `< -50` (`CMP EAX,-0x32; JGE`, so -50 does not cut funding); engine `isRelatedTo` hostile `< -50`, unfriendly `< -25`, floats.
F14 no periodic drift - match.
F15 rift offer - match on trigger/cost; engine_bug on resolution (`max(m+25,100)`).
F16a takeover relation values - match. F16b takeover trigger/probability/cadence - diverges (critical), now BOUND (section 8c). F16c second writer FUN_0007abf0 with liberation branch - engine_missing, caller unresolved.
F17 on-demand bribe - exe_unbound.
F18 incidents - match.
F19 starting relations - match (boundary differences).
F20 "attacked by non-hostile org -> X-COM row -50" (UFO2P `0xB8295`, TACP `0xB8B3F`) - engine_missing (low).
F21 Food Chamber -> Mutant Alliance - engine_missing (low).
F22 stun - not examined in TACP (`StunHostileAction` default off in engine).

## 11. Open leads (for whoever continues)
1. `worth` (UFO2P `[0x244090]`) alignment: TACP's saved block at VA `0x1ED294` is `0xD4` bytes (`FUN_00015494`); compare with the UFO2P post-battle reader's chunk size; if the bases align `worth` (offset `0x70`) maps to TACP `0x1ED304` (no TACP instruction references it, which would make the branch dead, unproven).
2. `FUN_0007acfc` / `FUN_0007abf0` caller (computed jump?) and the identity of the 180-record table at VA `0x144918`.
3. Stun/knock-out relation effect in TACP (`FUN_0005F860` damage applier; only the kill path calls `FUN_00060720` with a real killer).
4. Whether the original has an on-demand bribe at all (no writer found).
5. Rift-offer dialog button semantics in `FUN_00096BC8` (three buttons; only the accept path was decoded).
6. `FUN_00092060` (org raid manpower / unauthorised vehicle) reads hostility; its use of the matrix was not decoded.

## 12. Limits
* Static only. `S` = agent-type score is inferred (the unit byte `+0x44` is the quantity both the EXE team score and the engine's casualty score use). The "intentional" test fields (`vehicle+0x15A` target building, `+0xCF == 2`, `[0xD4E28] == 9`) are named by usage; the engine's `manualFire` / `AttackBuilding` is the obvious counterpart but the exact UI mode behind `[0xD4E28]==9` is not traced.
* `worth` (N) writer in TACP not found; takeover trigger table roster not identified.
* No dynamic confirmation. 4-build (`UFO2P4/TACP4`) behaviour not checked.
