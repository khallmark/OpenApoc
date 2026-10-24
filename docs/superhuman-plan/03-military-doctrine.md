# 03 — Military doctrine: infiltration control, air war, raids, base defence, agents (Superhuman)

**Summary.** The military half of the Superhuman plan: keep the Government and the sponsors from being taken over, fight the air and ground wars, hold the bases, run the agents. It is written against the original game (`UFO2P.EXE` / `TACP.EXE`, Superhuman = UI button 5 = engine difficulty 4); a marked **Engine today:** callout appears wherever the rebuilt game differs in a way that changes what the harness must do now, with the fix key of `04-engine-fidelity.md`.

1. **Infiltration control is survival rule #1.** An organisation can be taken over only while its displayed ALIEN INFILTRATION reading is above 50%, then with probability (v-100)/101 per 300-minute test. Keep every graph below 50%, Government first. A flip is permanent (exception: the Mutant Alliance, freed by a won Food Chamber raid); a Government flip cuts funding at the next Monday and leaves about four Mondays of cash. **Engine today:** any nonzero reading can flip an org every 125 minutes, so until T1+T2 land treat any nonzero Government reading as imminent.
2. **The race is hours to days.** A Transporter, Destroyer or Assault drop into the Senate crosses 50% in a median 19-24 h and must be cleared within 12-16 h; a Scout drop gives about 28 h; speed-2 Government buildings (8 of 11) almost never cross, but a Megapol Corporate HQ (product 8) or an Osiron class-3 building (product 9) is faster than the Senate. Above 50% clearing is not enough (18% flip chance still at 55%, 58% at 60%).
3. **Sensors:** the UFO hovering over a building, the graph slope (projected hours to 50%), the incident alert, the relation labels. An empty sweep costs the owner -9 relation; UFO kills (about +2 Government each), rift offers and bribes refill it.
4. **Air war:** kill the droppers, guided missiles first, order attacks on UFOs only, no hold-fire gate, a score governor on the original's one-Monday-lag cutoff (lifetime total at or above -2,400), watch the 1,000/2,000/4,000/8,000 UFO-kill gates.
5. **Tactical:** original Superhuman aliens are +16% TU/HP/reactions and +64% natural armour; the engine's are Novice-strength, so engine kill rates are overstated and squad sizes here are floors.
6. **Ground:** real time only; the alien-system unlocks need a won manned-UFO mission; Cult raids are the loot source; alien-building raids are won by destroying the objective, and the 6th raid permanently stops UFO growth.
7. **Base defence:** a base is attacked only by an Infiltration/Subversion landing or a hostile org's Raid/Storm; the defence fields everyone present, unarmed staff included, scaled to `36 - min(12, 4 x finished Security Stations)`, so a staff-heavy base fields few soldiers; a lost or retreated defence loses the base.
8. **Agents:** one training roll set per agent per day (the engine gives six), a flat hire pool (1.5 arrivals per technical class per day, caps 100/50/50/50 and 180), 20 HP/day healing, arm before hiring.

Every squad size and engagement margin is `[DERIVED]` until the benchmark fits it; the last section lists every `[MEASURE]` item.

## Contents

1. Conventions and tags
2. The takeover race: EXE rules and the numbers derived from them
3. Infiltration-control doctrine (INF-0 to INF-8)
4. Air war and the score governor (AIR-1 to AIR-11)
5. Tactical difficulty: what Superhuman aliens really are
6. Ground operations: sweeps, crash sites, Cult raids, alien dimension, capture
7. Base defence (BASE-1 to BASE-9) and expansion
8. Agents: hiring, training, healing, kit, roster (AGT-1 to AGT-6)
9. Standing-order index and precedence
10. Harness requirements
11. KPIs and experiments
12. Open measurements (last section)

---

## 1. Conventions and tags

**Tags.** `[EXE]` bound to the original binary (physical file offset, or the validation ID such as v_takeover/T1); `[ENGINE]` OpenApoc code or data (`LIVE:`/`HEAD:` path:line; a bare path is LIVE, what the benchmark runs); `[GUIDE: name, URL]` online source, paraphrased; `[DERIVED]` arithmetic or simulation with the model stated; `[MEASURE]` unknown, the harness measures it; `[UNBOUND]` EXE evidence absent, the lead is named. Benchmark observations come from engine builds and are tagged `[ENGINE]` with the research file and section (for example `failure_forensics.md` section 3). Numbers are `d` = 4 unless a formula is shown. Rules read `WHEN <player-visible observable> THEN <action> UNTIL <observable>`; no rule reads a hidden count.

**Trees.** HEAD = historical engine base `fda36d6c`; LIVE = the October 7–8 research snapshots. The former in-flight work later entered local commits through `bce89743`; neither label means the current checkout or identifies historical binaries. See the master README for provenance and remote-publication limits. UFO2P code offset = VA + `0x626A4`, data = VA + `0x616A4`.

**Index spaces.** Alien-building function index `f`: 0 Incubator, 1 Spawning, 2 Food Chamber, 3 Megapod, 4 Sleeping, 5 Organic Factory, 6 Alien Farm, 7 Control Chamber, 8 Maintenance Factory, 9 Gate Generator. The **raid order** visits `f` = 4, 2, 6, 8, 0, 7, 1, 5, 3, 9 (1st Sleeping ... 10th Gate); this appendix says "Nth raid" or the name, never a bare index. Orgs: 0 X-COM, 1 Aliens, 2 Government, 3 Megapol, 4 Cult of Sirius, 20 Psyke, 21 Diablo, 22 Osiron, 24 Mutant Alliance, 26 Technocrats, 27 Civilian. Relation tiers on screen: Allied >= 75, Friendly >= 25, Neutral >= -24, Unfriendly >= -49, Hostile <= -50. The AI predicates (raids, detection halving, park refill) use hostile <= -50; the weekly funding cutoff uses strictly below -50.

## 2. The takeover race: EXE rules and the numbers derived from them

### 2.1 Rules in force (original game, Superhuman)

| ID | Rule | Evidence |
|---|---|---|
| T1 growth | Hourly, every org: `v += N / 38` (signed integer division, divisor `42 - d`), then `v -= 1` on odd game hours, clamp 0..200. `N` = sum over the org's buildings of (sum of count x alienSpeed) x functionSpeed x orgSpeed; the floor is taken on the **sum over all the org's buildings**, so crews in different buildings add before the division. `N` < 38 adds 0: a cleared org loses 12 raw = 6 displayed points a day. Species speed: Egg 0, Brainsucker 1, Multiworm 1, Hyperworm 1, Chrysalis 0, Anthropod 2, Skeletoid 2, Spitter 2, Popper 1, Megaspawn 1, Psimorph 3, Queenspawn 1, Micronoid 2. Function speed: Senate 5, Corporate HQ 4, Offices/Apartments/Luxury Apartments/Hotel/Police/Hospital/School/Mall/Space Port/Sensodrome/Procreation Park/Temple of Sirius 3, Warehouse/Slums/Recyclotorium/Car Park/Power/factories 2. Org speed: Government 1, Cult 4, S.E.L.F. 0, others 1-4. | [EXE] `FUN_0006fcc0` file `0xD2364`, `FUN_0006fc34` `0xD22D8`; tables `0x15541C`, `0x1423EC`, `0x1424B0` (v_takeover/T6) |
| T2 brainsucker kick | Each Brainsucker that "grows" in the hourly pass adds `functionSpeed x orgSpeed` to `v` **undivided** (Government: Senate +5, class-3 +3, class-2 +2), before the hourly step. Nothing else writes infiltration: no mission result, relation event, kill or score. | [EXE] `FUN_0006f3a8` `0xD1A4C` case 1 (v_takeover/T7). Other species' growth chances (Egg 20%/h, worms 12%, Chrysalis 12%, adult species 2%/h) are engine extractor constants (`extract_agent_types.cpp:409-494`) [UNBOUND; lead: the other cases of `FUN_0006f3a8`] |
| T3 roll | Human city only. `FUN_0006fd9c` runs every 5 game minutes and tests ONE org from a 60-slot rotation (slots 2..26 are orgs, Government first), so each org is tested once per **300 game minutes**. Taken over iff `rand(0..100) < v - 100` (inclusive): probability `(v-100)/101`, **exactly 0 for `v` <= 100 (displayed <= 50%)**. X-COM, Aliens, Civilian are never tested; S.E.L.F. can never flip; no difficulty term; skipped in the alien dimension. | [EXE] file `0xD2440`, roll bytes `0xD248F` (v_takeover/T1, T2) |
| T4 effects | Absolute stores: infiltration := 200, flag := 1; Aliens<->org +100; org<->X-COM -100; +90 with every other taken-over org; modal "ALIEN TAKEOVER" screen. Follow-ons: the Government's -100 fails the funding relation leg next Monday; bribes and rift/alliance offers refused; raid table = military; detection halved once more; guards armed from tier + 3. **Hire arrivals ignore takeover** (8.1). | [EXE] v_takeover/T3, T5, T10-T14 |
| T5 permanence | Re-pinned to 200 every hour; no decay; the generic liberation routine is dead code. **One exception:** a WON Food Chamber raid (2nd in the raid order) clears the Mutant Alliance's flag, sets its relation to X-COM to +100 and its infiltration to 0 (retaken only after `v` exceeds 100 again). | [EXE] v_takeover/T4; `FUN_000b32ac` (F14 adjudicated) |
| T6 seeding | At new game one random building whose owner's relation to X-COM is >= 0 gets Brainsucker 1..4 and Anthropod 1..5 (speed-sum 3-14). Start relations: Government 55-75, Cult -70..-50 (hostile in all 21 seeds). | [EXE] `FUN_00015400` (T8 adjudicated) |
| T7 spread | Hourly per building: `moved[k] = crew[k] x (movePercent[k] + r)/100`, one shared `r = rand(0..30)`, movePercent 40,40,33,33,0,33,33,33,33,0,0,0,30 by crew index; only if total moved >= 3; then with probability `15 + 3 x total` (+20 if the **source** owner is taken over or allied to the Aliens >= 75) to one of the first 15 intact buildings in array order inside a 15-tile box (abs dx < 15 and abs dy < 15). Infestations of 4 or fewer never spread. | [EXE] `FUN_0006f7f8` (BD06) |
| T8 visible | The per-org ALIEN INFILTRATION graph (`v/2`, integer division: 0% can hide raw 1; 55 daily points; up to 10 orgs per view), the same percent on the org's Ufopaedia entry, the alert/ticker, the relation label. The takeover is modal but the harness log has no takeover line (`failure_forensics.md` section 0.6): read the label or org screen. | [EXE] `FUN_0008903c`; [ENGINE] `infiltrationscreen.cpp:19-60`, `ufopaediacategoryview.cpp:290-309` |
| T9 detection | `FUN_0006f964` every 5 min over 5 buildings round robin: with 109 buildings each is visited about every 109 min; buildings with an alert timer are skipped. Species chosen by reservoir sampling (k-th present species passes with `rand(0..100) < 100/k`; the first passes about 99%); `sum = Σ count x detectionWeight` (by crew index 1,1,3,1,1,3,3,3,2,10,8,20,1); `value = sum x (buildingWeight - 2d)/100` (Senate 155, Police 135, Corporate HQ 130, Luxury Apartments 130, Offices 120, Warehouse 100, Slums 95; X-COM Base and Temple of Sirius 0); halve ONCE if the owner's relation to Aliens >= 75 OR to X-COM <= -50, once more if taken over; incident iff a species was chosen and `rand(0..100) < value - 10`. On day 1, 12:00-16:00 the sum is x100 (the seed is reported within about 110 min). Incident: -30 score, alert timer 360 min. X-COM base buildings never fire. | [EXE] `FUN_0006f964` `0xD2008`, weights file `0x155354` (SCORE-C2; v_score 4.3). Weights not quoted from the EXE are the extractor's copy (`extract_buildings.cpp:40`) [UNBOUND; lead: dump `0x155354` by function id] |
| T10 empty | Exterminate/Investigate on a non-X-COM building whose 13 crew bytes are all zero: no battle, owner relation to X-COM -(5 + d) = **-9** WITH ripple (a Megapol building also costs the Government about -3). Egg/Chrysalis-only = no penalty, no battle. The alert/arrival route costs nothing. | [EXE] `FUN_000b426c` (F08 adjudicated) |

**Engine today** (fix T1+T2): `rand[0,200) < v` every 125 min per org with a random phase (`organisation.cpp:84-108`, `organisation.h:19`), also in the alien dimension, so **any `v` above 0 can flip an org**. Median time to a Government flip for a sustained infestation: +2 raw/h (10 Anthropods in the Senate, N = 100) 19 h vs about 87 h in the original; +1/h 34 h vs 236 h; +7/h (27 aliens in the Senate) 9 h vs 25 h [DERIVED, v_takeover section 6]; benchmark takeovers on day 7-20 are 3-7x too early. Also: seeds are 1..3 of each species (`extract_agent_types.cpp:1335,1337`; fix T8); spread fires at any total > 0 (`building.cpp:970-1042`; fix BD06); detection gates on `100/(count+1)` with up to three halvings (`building.cpp:771-840`; fix SCORE-C2), so big single-species stacks are 4-13x under-reported (12 Anthropods in a Corporate HQ: 32% per visit vs 2.5%) while an infested crewed X-COM building is force-detected (`building.cpp:158`) and bills -30 about every 7 h; an empty investigation is -9 without ripple and Egg/Chrysalis-only starts a battle (fix F08).

### 2.2 What each UFO deposit is worth to the aliens

`crew speed-sum x functionSpeed x orgSpeed = S`; `g = floor(S/38)` is the hourly gain; net per hour is `g - 0.5`. Crew compositions are the extracted deposit tables (totals equal the EXE crew table, v_base_defence/F03). Government org speed is 1.

| UFO | Aliens deposited | Crew speed-sum | g in **Senate** (x5) | g in class-3 (x3) | g in class-2 (x2) |
|---|---|---|---|---|---|
| Probe | 4 | 7 | 35 -> **0** | 21 -> 0 | 14 -> 0 |
| Escort | 13 | 14 | 70 -> 1 | 42 -> 1 | 28 -> 0 |
| Scout | 13 | 16 | 80 -> **2** | 48 -> 1 | 32 -> 0 |
| Fast Attack / Bomber | 10 / 8 | 16 | 80 -> 2 | 48 -> 1 | 32 -> 0 |
| Transporter | 20 | 23 | 115 -> **3** | 69 -> 1 | 46 -> 1 |
| Battleship | 20 | 28 | 140 -> 3 | 84 -> 2 | 56 -> 1 |
| Destroyer | 22 | 29 | 145 -> 3 | 87 -> 2 | 58 -> 1 |
| Mothership | 24 | 33 | 165 -> 4 | 99 -> 2 | 66 -> 1 |
| Assault Ship | 27 | 35 | 175 -> **4** | 105 -> 2 | 70 -> 1 |

Crews breed toward higher speed (eggs hatch into worms, worms into Hyperworms, Chrysalises into Brainsuckers/Anthropods/Spitters/Poppers) and Brainsuckers add undivided kicks, so `g` is a lower bound for egg/worm-rich crews (Scout, Transporter, Destroyer, Assault) after the first hours; part of the deposit may relocate (T7) `[DERIVED]`.

### 2.3 Derived timing (Monte Carlo, original roll)

**Model [DERIVED].** Hourly Brainsucker kick and species growth, `v += floor(S/38)`, odd-hour -1, clamp; roll every 300 min with random phase, flip with probability `(v-100)/101` for `v` > 100; 400-600 trials per cell; no relocation, no player action. `S` uses the **product P = function speed x org speed**: Government (org speed 1) Senate 5, class-3 3, class-2 2; Megapol/Marsec/Superdynamics/Nanotech (org speed 2) class-2 4, class-3 6, Corporate HQ 8; Osiron/Diablo/Psyke (org speed 3) class-2 6, class-3 9, Corporate HQ 12 (org speeds as extracted from table `0x1424B0` `[UNBOUND]`). The species-growth constants are the engine's and are not EXE-validated: model A keeps their 2%/h die-off of adult aliens, model B removes it; **"clear by" is the tighter of the two**, so read hours as plus or minus 50% and refit from graph observations (E6).

**(i) Last hour a full clearing is still safe, by product** ("clear by" = last 4-hour step at which removing the whole crew leaves at most 1% eventual flip chance; ">96" = crosses 50% only after 4+ days; "-" = no accrual):

| Drop | P = 3 | P = 4 | P = 5 (Senate) | P = 6 | P = 8 | P = 9 | P = 12 | Senate: median h to >50% (A / B) |
|---|---|---|---|---|---|---|---|---|
| Probe | - | - | - | - | >96 | >96 | >96 | - / - |
| Fast Attack | >96 | 72 | 52 | 40 | 28 | 24 | 16 | none / 64 |
| Bomber | >96 | >96 | 68 | 68 | 40 | 40 | 20 | none / 67 |
| Scout | 48 | 36 | 28 | 24 | 16 | 12 | 8 | 39 / 33 |
| Escort | 60 | 40 | 32 | 24 | 20 | 16 | 12 | 53 / 39 |
| Transporter | 32 | 24 | **16** | 16 | 8 | 8 | 4 | 24 / 22 |
| Battleship | 32 | 24 | 20 | 16 | 12 | 12 | 8 | 30 / 24 |
| Destroyer | 32 | 20 | **16** | 12 | 8 | 8 | 4 | 24 / 21 |
| Mothership | 32 | 24 | 16 | 16 | 12 | 8 | 8 | 26 / 22 |
| Assault | 24 | 16 | **12** | 8 | 8 | 4 | 4 | **19** / 17 |

Government class-2 buildings (P = 2, Warehouses and Slums: 8 of the Government's 11) matter only for an Assault drop (31% cross within 10 days, clear by 60 h) or a Transporter drop (9% cross, clear by 84 h); every other crew stays below the line. A clear-by of 4 means the crew must be in a battle within about 4-8 h of the drop: such cells (P >= 9 with a large UFO) need the QRF inside the building within about 4 h or interception before the hover (section 4). **Safety factor:** act at half the "clear by" hour (travel, relocation, model error) [DERIVED]. **Engine today** (fix T1+T2): P(flip within 24 h) in the Senate is Scout 0.86, Escort 0.67, Fast Attack 0.54, Bomber 0.47, Transporter 0.97, Destroyer 0.98, Assault 0.99; class-3: Scout 0.55, Transporter 0.81, Assault 0.92 [DERIVED, same crews under the engine roll].

**(ii) Residual risk once the value is above 50% and the crew is gone** (it decays 12 raw a day, so the tests keep running):

| Displayed % when cleared | 51 | 52 | 55 | 60 | 65 | 70 | >= 75 |
|---|---|---|---|---|---|---|---|
| P(still flips before decaying to 50%) | 0.01 | 0.03 | 0.18 | 0.58 | 0.86 | 0.97 | ~1.00 |

**Engine today:** without a threshold, displayed 5% -> 22% eventual flip even with the crew gone, 7% -> 38%, 10% -> 63%, 15% -> 90%, 20% -> 98% [DERIVED, Monte Carlo of `organisation.cpp:84-108`]; hence "any nonzero reading is an emergency" until T1+T2 land.

**(iii) Incident chance per building visit** (original rule, initial composition; mean wait for a report = 109 min / p; silent 6 h after a report; Offices (120) fall between the two right-hand columns, Slums 1-3 points below Warehouse):

| Drop | Senate (155) | Corporate HQ / Luxury Apt (130) | Warehouse (100) |
|---|---|---|---|
| Probe | 0.04 | 0.02 | 0.00 |
| Scout | 0.32 | 0.25 | 0.16 |
| Bomber, Fast Attack, Escort | 0.25-0.29 | 0.19-0.22 | 0.12-0.14 |
| Transporter | 0.52 | 0.42 | 0.29 |
| Battleship, Destroyer | 0.59 | 0.47 | 0.34 |
| Mothership | 0.72 | 0.58 | 0.41 |
| Assault | 0.75 | 0.60 | 0.44 |
| 3 / 4 / 10 / 12 Anthropods only | 0.03 / 0.07 / 0.33 / 0.41 | 0 / 0.04 / 0.26 / 0.32 | 0 / 0.01 / 0.17 / 0.23 |

**Engine today:** the species gate `100/(count+1)` makes big single-species stacks 4-13x rarer (10 Anthropods in the Senate 0.03) and mixed crews about 10% lower: never rely on alerts for stacks on the engine.

**What the tables mean (section 3 derives from them).** (1) The unit of time is hours to a day or two: the worst Senate drops need clearing within 12-16 h, so travel, hover, relocation and readiness fit in half that. (2) **The 50% line is the hard boundary:** below it the flip chance is exactly zero; at 52% it is 3% even if cleared; at 60% it is 58%. The graph rule is a projection (hours to 50%), not a level. (3) **Rate of rise identifies the crew class without hidden counts:** displayed gain per hour is `(g - 0.5)/2` (g = 1 +0.25%/h, 2 +0.75%/h, 3 +1.25%/h, 4 +1.75%/h); a step of +2 to +3% within an hour with no UFO on screen is a Brainsucker kick. (4) Alerts are a useful second sensor in the original (large crews report within one to three visits, 2-5 h). (5) **Prevention beats cleanup** wherever an interceptor can reach the UFO before it hovers; a Probe into the Senate (S = 35 < 38) accrues nothing.

**What the benchmark saw `[ENGINE]`** (engine builds; read as 3-7x early): the Government was taken over in 48 of 166 saves, in 15/28 saves at day 15-18 and 17/24 at day 19-25 (`failure_forensics.md` section 3); the last daily graph values before 200 were 0,0,0,0,2,6,6,6,8,9,11,12,13,15,18,23,37 (of 200), first non-zero day to first 200 day <= 4 days in 16 of 18 runs (`harness_audit.md` section 6.1); other orgs flipped by day 15-19: median 12 of 27 (Osiron 77/166, Megapol 64, Diablo 48, Psyke 28; S.E.L.F. never; `failure_forensics.md` section 6). With T1+T2 landed the same infestations leave the org alive for days (E6 re-measures).

---

## 3. Infiltration-control doctrine

**Design constraint.** The original gives the player hours to a day or two (2.3), a usable but incomplete alert sensor, and a relation cost of -9 per empty sweep. The doctrine therefore (1) puts the primary sensors on the **drop itself** and on the **graph projection**, with alerts as confirmation; (2) keeps a ready squad whose job is to start a battle early (the building's crew leaves it for the battle, so accrual stops); (3) caps blind sweeps with a relation budget that refills from UFO kills; (4) ranks buildings by the gain class they can reach (2.2).

### 3.1 Which organisations to defend, in what order

| Tier | Organisations | Why (original rules) | Org speed | Response window | Blind sweeps? |
|---|---|---|---|---|---|
| **0** | **Government** | A flip sets Government->X-COM to -100: funding is cut at the next Monday (balance minus the old income, income zero, latch forever), about four Mondays of cash remain, bribes are refused, its guards arm from tier + 3, detection in its buildings halves again, it raids bases from the military table. 73% of engine-benchmark runs reaching day 15 had it (`failure_forensics.md` section 3). | 1 [EXE] | 2.3(i) by product | Yes within the budget (3.6), spent down to the funding floor once T50 < 24 h (INF-3) |
| **0b** | **Megapol** (armour, Lawpistol, Laser Sniper, Auto Cannon, grenades, Janitor, Justice), **Marsec** (craft, Rendor, Lineage, Prophet, Retribution, M4000, Marsec armour, Mind Bender), **Superdynamics** (engines, Powerfuel, Cargo Module), **Nanotech** (Medi-kit) | A flipped vendor has -100 toward X-COM; buying from a manufacturer whose own view of X-COM is <= -50 is blocked in the original (selling to it is not) [EXE F-M5]. Megapol is also a raider: Raid 36.8% + Storm 25.0% per trigger (7.3). | 2 | 2.3(i) by product | Only if the org's graph >= 20% (INF-4) |
| **1** | **Osiron, Diablo, Psyke, Gravball League, Mutant Alliance** | The original's hire pool ignores takeover (8.1), so these matter only as raiders (Osiron 12-20 attackers, Diablo 9-16, Psyke 7-12; a flipped org rolls the table with Raid 29.1% + Storm 16.4%), as +3-tier guards, and, for the **Mutant Alliance**, as the hybrid source (its view of X-COM must be >= 0) and the **one reversible takeover** (a won Food Chamber raid, 6.4). | 3 (Osiron, Diablo, Psyke) / 2 / 1 as extracted from table `0x1424B0` | 2.3(i) by product (P = 6-12), evidence only | No |
| **1x** | **S.E.L.F.** (androids) | Org speed 0: cannot be infiltrated [EXE]. Androids arrive only while its view of X-COM is >= 0, so never sweep it, never shoot its buildings (an empty sweep, -9, is the only way to lose it). | 0 | n/a | Never |
| 2 | General Metro (Passenger Module), Cyberweb, Transtellar, Solmine, other corporations | Convenience vendors. | 2 | idle squad | No |
| 3 | **Cult of Sirius** | The Aliens' ally from day 0 and the fastest infiltrator (org speed 4); it is the loot source: **RAID it, never sweep it** (6.3). A flip moves its raids on bases from Raid 25% to Raid 29.1% + Storm 16.4% per trigger and arms its guards from tier + 3. Engine saves: flipped in 0/21 before day 7 and 18/20 by day 14-20 (`income_cult_raids.md` section 4.8), which the original's 50% line makes much slower. | 4 | n/a | Never |

Rules that follow: **INF-6** (sample the 10 Tier 0-1 graphs hourly; they fit exactly one 10-line graph) and **BUY-AHEAD** (8.1).

### 3.2 Government buildings and the gain class each can reach

Owner, function and footprint come from the difficulty-4 extraction of the 109-building human city `[ENGINE data]` (building ownership is itself difficulty-indexed; only this extraction applies); speeds from 2.1; distances are Manhattan between footprint centres `[DERIVED]`.

| Building (Government-owned at start) | Function / speed | Detection weight | Smallest crew speed-sum giving g >= 1 | Distance to the Senate | Notes |
|---|---|---|---|---|---|
| **The Senate** | Senate / **5** | 155 | **8** (4 Anthropods; 8 Brainsuckers/Hyperworms) | 0 | Cannot be bought or avoided. Sweep first. |
| **Eden Mansions** | Luxury Apartments / **3** | 130 | 13 | 21 | Not a base site. |
| **Quadrax Tower** | Offices / **3** | 120 | 13 | 50 | Not a base site. |
| **Warehouse Eleven, Warehouse Ten** | Warehouse / 2 | 100 | 19 | 27, 30 | Base sites 3 tiles apart: a crew in one can relocate to the other. |
| **Grimoire Block, Thomas More Tower** | Slums / 2 | 95 | 19 | 38, 44 | Base sites (Thomas More is the weak start-base layout, 7.7). |
| **Warehouse Two, Lovecraft Block, Warehouse One, Warehouse Eight** | Warehouse / Slums / Warehouse / Warehouse; all speed 2 | 100 / 95 / 100 / 100 | 19 | 79, 84, 85, 86 | Far-corner base sites; a crew here is plausible only after a watched drop. |

Eight base-layout sites plus the Senate, Eden and Quadrax make 11 Government buildings; the random start base becomes X-COM-owned, giving the observed 9-11. Only 3 of 11 have speed >= 3; **8 of 11 are speed 2, where most drops never cross 50% (2.3 i)**. Crews add **across buildings before the division** (T1): two warehouse crews of 5 Anthropods (20 + 20 = 40) are g = 1 together and g = 0 apart, and relocation (T7) can build a Government total out of several small crews, so an infested neighbour of a Government building is a leading indicator.

**Sweep order when the Government line rises and no building is known** (each step is evidence-free and charged to the budget, 3.6): (1) the Senate; (2) Eden Mansions, then Quadrax Tower; (3) the speed-2 set nearest the Senate first (Warehouse Eleven, Warehouse Ten, Grimoire Block, Thomas More Tower), then the far corners; stop when a sweep finds aliens and the rise stops. Rationale `[DERIVED]`: the Senate needs the smallest crew (8) to reach g >= 1, class-3 13, class-2 19. Skip any site X-COM already owns (no relation cost, but see 3.7 for the engine's stale list).

### 3.3 Sensors a human has, and what each is worth

| Sensor (player-visible) | Reads | Latency from drop | Reliability |
|---|---|---|---|
| **S-DROP**: an alien craft stationary over a building footprint with the infiltration doodad (it flies to the roof, hovers, deposits; if it dies in that window nothing is deposited) | which building; UFO type -> crew class | 0 | High; needs game speed <= 3 so the harness sees the hover (`vehiclemission.cpp:2477-2575` [ENGINE]) |
| **S-GRAPH**: ALIEN INFILTRATION point 0 or the org's Ufopaedia percent | displayed `v/2` per org; slope -> g class and **T50 = hours to 50%** | hours (first reading when N >= 38) | Exact; blind to N < 38 crews, which are harmless |
| **S-ALERT**: AlertScreen/ticker (AlienSpotted) | building name, owner, function (not the crew size) | wait = 109 min / p; p from 2.3(iii): about 2-5 h for Transporter-or-larger crews in the Senate, days for Probe or small stacks | Good confirmation in the original; **engine: big single-species stacks are 4-13x rarer, so never the primary trigger** |
| **S-MARK**: red/yellow building circle on the city map | the building currently flagged; dropped hourly once no aliens remain, reset after the 6 h silence | up to 6 h after a report | Marker present means aliens were there at the last hour tick; marker absent proves nothing |
| **S-REL / S-WEEK**: org-screen label; Monday funding and Score screens | label band; weekly and total score by category | n/a / weekly | Coarse / exact |

The harness's current alert path reads `crew=N`, which a player cannot see (the AlertScreen shows owner and function only, `alertscreen.cpp:28-29`): removed (HR-1). "Empty" is judged from S-MARK or the age of the S-DROP.

### 3.4 Triggers and responses

| ID | WHEN | THEN | UNTIL |
|---|---|---|---|
| **INF-0** | Day 0, the first incident alert (the seeded building is reported within about 110 min). The seed is Brainsucker 1-4 + Anthropod 1-5 in one random building whose owner's relation to X-COM is >= 0 `[DERIVED from start relations and ownership: about 1 start in 3 is a Government building, 1 in 3 Megapol, 3% the own base; E14]`. | Send **6 of the 10 starters** that carry a lethal primary (not the two Stun-Grapple carriers; order 2 Lawpistols + 6 clips at 12:00 for them), **keep >= 4 armed at home**, fight at the door with pods in backpacks. In the Senate a seed is S = 15-70, g >= 1 in 12 of 20 equally likely compositions, and every Brainsucker growth adds +5, so any seed is swept at its first alert. There is no 24 h grace: the first UFO timer is 10 h + rand(0..2 h) (about 5-6 h if both drains run, `[MEASURE]` F6; the engine waits 24-72 h) and the first organisation-raid check is the first midnight, mission 50-1250 min later (earliest about 12 h 50 min). The squad returns the moment the battle ends and takes no second task. | battle won, squad home, kit re-verified |
| **INF-1** | S-DROP on building B and the hover **ended with the craft alive** (deposit completed). A UFO destroyed during the hover deposits nothing and never starts a sweep. | Compute B's product `P` = function speed x owner's org speed (2.3, 3.1) and read the clear-by hour `h(U, P)` of 2.3(i) (use the next faster column when `P` falls between two); deadline `D = h/2`. Tier 0/0b: launch the ready QRF within 1 game hour (it may lift off at the first sight of the hover; only the EXTERMINATE press waits for the deposit), open B's building screen, **press EXTERMINATE only if the deposit is < 6 h old or B is flagged (S-MARK)**: the -9 is charged on the press, so a squad that arrives late and finds B unflagged flies home. Tier 1: same procedure with that deadline, only while that org's graph rises or the drop was Transporter-class or larger. Tier 2: only when a squad is idle and the UFO is Transporter or larger. Tier 3/1x: ignore (Cult only if a RAID is planned). **Engine today** (fix T1+T2): `D` = **1 game hour for Tier 0, 3 h for Tier 0b**; any nonzero Government reading can flip within about a day, so the clear-by hours above do not exist. | battle generated (the crew leaves B) -> fight (6.1) |
| **INF-2** | AlertScreen/ticker names building B. | If S-MARK is present for B, or a drop on B or a building within 15 tiles happened in the last 6 h: treat as INF-1. Else if the alert is > 6 h old and the owner's graph is flat: drop it. Never answer a stale queue first-in-first-out (the benchmark's "39 answered alerts took the Government from +85 to -100", `docs_digest.md` section 2.7). | answered or dropped |
| **INF-3** | The hourly Government reading `d` rises above `ref_gov` (initially 0). A killed crew leaves residue that decays 0.5 raw an hour, so a level alone does not fire. | Cancel discretionary tasks; sweep located Government targets from alerts, markers or completed drop-offs. If no target is observable, increase graph/drop-off observation and log `graph_up_no_target_hours`; do not guess the Senate or read hidden crews. Let `r` be the last three samples' hourly rise (at least 0.25), `T50 = (50 - d)/r`; at `T50 < 24 h` or `d >= 45`, claim all safely available squads, retaining base floors. Never cross the -50 funding floor through empty searches. After clearance set `ref_gov = d`, then its minimum observed value, zero when `d = 0`. **Engine today** (T1+T2 absent): any nonzero rising graph gets emergency priority; this changes urgency, not the observable-target requirement. | two declining readings or stable zero; unresolved targets remain queued |
| **INF-4** | A Tier 0b/1 org's reading `d_org >= 20` and above its `ref_org` (same edge rule), or its T50 < 48 h. **Engine today:** `d_org >= 5`. | Sweep only a building located by an alert, drop or flag. Otherwise observe more frequently and record the missing target. No blind highest-function-building search in the default policy; false positives still consume the organisation's relation budget. | flat 3 samples, or two empties in a row |
| **INF-5** | A sweep was won but the owner's graph still rises, or an alert appears within 15 tiles of B inside 6 h. | Treat the new alert as the re-housed crew (aliens that fled by an exit are re-housed in one nearby intact building, `battle.cpp:3019-3030` [ENGINE]) -> INF-2. | graph flat |
| **INF-6** | Every game hour and each 00:00. | Read point 0 and the last 3 daily points of the 10 Tier 0-1 orgs; log rate, T50, `g_est = round(2 x rate + 0.5)`. | always |
| **INF-7** | *Deleted:* a crew with N < 38 is harmless in the original and a larger one shows on the graph, so a discretionary Senate sweep has nothing to find. | | |
| **INF-8** | Government taken over (label Hostile, bribe refused, ticker "ALIEN TAKEOVER"). | Post-takeover mode (3.8). | rest of campaign |

### 3.5 The ready squad (QRF) and squad sizes

* **Two squads, two transports by week 3.** Transport = Valkyrie (8 passengers, 12 with a Passenger Module, Cargo Module for loot; $75,000) `[ENGINE data/common_patch/gamestate.xml:323-397]`; buy the second in week 2 (QRF, manned-UFO recovery and Cult raid all want the same hull). Interceptors never carry agents. **Fallback** if the second Valkyrie fails appendix 01's runway gate: the two Phoenix Hovercars as 4-seat transports; one squad from agents in two craft is untested `[MEASURE]` E12b. QRF 1 sits aboard its transport at the base; parking on the Senate roof is untested `[MEASURE]` E12.
* **Size by the UFO type that made the drop** (never by hidden counts). `[DERIVED]` as 0.45-0.6 x aliens deposited, rounded up for Multiworm/Popper crews, floor 4 for a Probe and 6 otherwise, cap 12; set against unscaled aliens, so under TD1-TD3 (section 5) they are floors; E9 refits.

| Drop | Aliens | Squad | Notes |
|---|---|---|---|
| Probe | 4 | 4 | |
| Bomber / Fast Attack | 8 / 10 | 6 | Fast Attack carries 3 Poppers: no HE adjacent |
| Scout / Escort | 13 | 8 | Scout has 2 Multiworms (394 HP, armour 13 body at D = 4): 2 AP grenades per man |
| Transporter / Battleship | 20 | 10 | |
| Destroyer / Mothership | 22 / 24 | 10 | Mothership drop holds 5 Poppers + 6 Skeletoids |
| Assault Ship | 27 | 12 | 5 Brainsuckers, 5 Skeletoids: androids first (8.5) |

  **Floor rule:** never dispatch fewer than 6 armed-verified soldiers unless the drop was a Probe; benchmark failures were 2 soldiers vs 15 at a crash site (wiped in 16 s) and 5 vs 36 foes, a count that includes the civilians the sweep map adds (`harness_audit.md`, recovery and sweep defects). This replaces `min_squad` 3 and `infil_min_armed` 3.
* **Verification** is on the equip screen (loaded lethal primary + >= 2 spare clips + 5 armour pieces), not "any weapon item" (`gamestateintrospect.cpp:559-615` counts unloaded weapons).
* **Priority when squads compete:** (1) base defence in progress; (2) INF-1/3 on Tier 0; (3) first manned-UFO recovery (appendix 02); (4) INF-1 on Tier 0b; (5) other crash recoveries; (6) Cult raid; (7) Tier 1-2 sweeps.

### 3.6 Blind-sweep budget (relation arithmetic)

**Optional experiment only.** INF-3/4 and 05 SV-02 default to located targets. The evidence-free allowances below describe a separately registered blind-search arm, not permission for the default agent to guess buildings. Even a marked target can be stale; record those empty searches and their relation cost in every arm. No hidden crew count may select targets in either arm.

**The Government's relation is also the funding leg:** the budget below is a hard floor for it (never sweep it below -50; every other limit in this appendix is advisory). An empty sweep costs the owner -9 against X-COM **with ripple** (a Megapol building also costs the Government about 3); a building holding only Eggs or Chrysalises costs nothing and starts no battle. Hostile for raids and detection is <= -50; the funding cutoff needs the Government strictly below -50 (-50 reads "Hostile" and does not yet cut). With `R_lo` the lower edge of the shown label, the consecutive empty sweeps that keep the org above Hostile are `k_safe = floor((R_lo + 49)/9)`:

| Label | R_lo | k_safe | Policy |
|---|---|---|---|
| Allied | 75 | 13 | 5 evidence-free + any evidence-driven |
| Friendly | 25 | 8 | up to 3 empties per emergency |
| Neutral | -24 | 2 | evidence-driven only; stop after 1 empty |
| Unfriendly | -49 | 0 | no sweeps (below 50% the flip chance is zero, and one empty makes the org Hostile) |

**The budget refills.** Relations have no passive drift, but four levers exist and the label is re-read at every decision, so a refill widens the budget at once:
1. **UFO kills.** A UFO downed (below its crash word, once) lifts the Government and Megapol about +2 each through the ripple, an outright kill about +1 [EXE F06; engine +0.6]; a tactical alien kill gives every org hostile to the victim +S/4 toward X-COM [EXE F11]. About 4.5 UFO kills repay one empty sweep.
2. **Rift offers**: hourly, one org per call (each org every 27 h), offered when the org's view of X-COM is < 25, has fallen by more than 15, and a 49.5% roll passes; accepting pays the org and sets the relation to 0 if negative else +25 [EXE `FUN_00096bc8`]. The engine sets any positive relation to 100 (F15b).
3. **On-demand bribe** (Diplomacy "Offer settlement"): refused if the org is taken over, allied to the Aliens or already allied; the screen shows the price; the `5,000 + 3,000 d` term (17,000 at d = 4) is confirmed, the rest `[UNBOUND]` (lead: compare `costOfBribeBy` with `FUN_00097234`).
4. **Alliance request**: relation to 100; offered at 25..74 after a rise > 15 or with > 2 common enemies (engine: enum only).

**Engine today** (fix F02+F05): stray fire (-5 per hit, -25 per destroyed tile), Government craft kills (-30) and a UFO kill worth only +0.6 drain the same budget 2-5x faster: do not spend the last Friendly step on a hunch and keep stray fire at zero. The Government starts at 55-75 (Friendly); the cutoff is 116 points from 65.

### 3.7 Structural measures and things not to do

* **Air interdiction of droppers** (section 4) is the only measure that avoids the clock entirely.
* **Do not leave neighbours infested:** spread needs a moved total >= 3 and reaches only the first 15 intact buildings in array order inside a 15-tile box; an infested block next to the Senate is a feeder. Rank idle-squad sweeps by distance to Tier 0 buildings.
* **Mutant Alliance:** if it flips, the 2nd alien-building raid (Food Chamber) frees it (6.4); no other flip is reversible.
* **Buying a base site.** In the original a purchase moves the building out of the Government's sum. **Engine today:** `Organisation::buildings` is filled once by `City::initCity` (`city.cpp:157`) and neither `BaseBuyScreen` (`basebuyscreen.cpp:93`) nor `Base::die` touches it, so a bought building stays in the Government's list until a save/load (same root cause as BD08, 7.3): expansion does not shrink the Government's attack surface in-session.
* **Not to do:** RAID a Government building (`BUTTON_RAID` with `OpenApoc.Mod.RaidHostileAction` = -200 relation, default off, `buildingscreen.cpp:248-250` [ENGINE]); the "move along" retreat trick that a guide describes `[GUIDE: UFOpaedia Alien Infiltration, https://www.ufopaedia.org/index.php/Alien_Infiltration_(Apocalypse)]` (the engine returns survivors to the same building, `battle.cpp:2956-2995`; the original path is `[UNBOUND]`); answering alerts first-in-first-out or polling the raid function at 1 Hz and counting polls as raids (`harness_audit.md` D1-D3, R1); global infiltrated-building scans (HR-4); holding fire because the Government relation is < 25 (AIR-8).

### 3.8 Post-takeover mode (INF-8)

| Change | Rule |
|---|---|
| Funding ends at the next Monday | Balance minus the old income, income zero, latch forever [EXE G1/G2]. With 100,000 cash and 22,900 overheads the balance falls below -5,000 on the fifth Monday after the cut, which ends the game (4 Mondays of cash) [DERIVED, EXE `FUN_00060ea4`]. WHEN the cut is seen THEN sell stock, stop hiring and building that Monday. **Engine today** (fix G8, after G4): there is no bad-debt defeat, so the harness enforces its own -5,000 floor. |
| Payroll | Soldiers are the largest discretionary line: release trainee places, staff and extra labs first, never armed men below the 8.6 minimum or any garrison floor. |
| Government is Hostile | (a) No Government sweeps without S-DROP/S-MARK evidence: its building guards (average 14, i.e. 10-17, cap 20) arm from tier + 3 and join the opposing side, so a Scout crew of 13 becomes 23-30 foes `[ENGINE battlemap.cpp:298-309; EXE guard tier T10]`; (b) it raids bases at Raid 29.1% + Storm 16.4% per trigger (7.3); (c) detection in its buildings halves once more, so graph-first discipline matters more. |
| Bribes and rift offers refused | The relation cannot be repaired; stop spending relation budget on it. Hold-fire gate: already removed (AIR-8). Base purchase is still allowed `[ENGINE baseselectscreen.cpp:93-97]`. |

---

## 4. Air war and the score governor

**Purpose of the air arm.** (1) Stop infiltration drops before the UFO deposits (a UFO destroyed during the hover completes the animation with nothing deposited; `vehiclemission.cpp` case 0 -> snooze -> case 1 [ENGINE]; the guide agrees `[GUIDE: UFOpaedia Alien Infiltration, https://www.ufopaedia.org/index.php/Alien_Infiltration_(Apocalypse)]`); (2) score: a UFO killed is `+hull score`, one that leaves through a gate is `-score/4`; (3) deny Bomber/Destroyer bombs their buildings; (4) produce wrecks for research (appendix 02). It is not to win the war over the city: damage to the city is charged to X-COM for alien fire too (4.3).

### 4.1 The enemy, per type

Hull data `[ENGINE data: vehicle_types.xml]`; kill score and escape cost `[EXE]` (UFO scores 50, 100, 150, 200, 250, 300, 300, 250, 500, 700 in the order Probe ... Mothership; escape `floor(score/4)`); withdrawal windows `[EXE v_ufo_consumers/F7]`.

| UFO | First week | HP / crash HP | Armour (mean) | Speed | Kill / escape / swing | Weapons | Withdraws while HP is in [crash HP, threshold) with threshold up to |
|---|---|---|---|---|---|---|---|
| Probe | 1 | 80 / 15 | 2 | 14 | 50 / 12 / 62 | Light beam 20, 150 m | 24 or 32 |
| Scout | 1 | 120 / 35 | 4.4 | 12 | 100 / 25 / 125 | Light beam | 36 or 48 (1 HP or 13 HP window) |
| Transporter | 2 | 400 / 150 | 6 | 8 | 150 / 37 / 187 | Light beam | 160 in 1 of 5 slots (10 HP window) |
| Fast Attack | 2 | 500 / 150 | 9 | 20 | 200 / 50 / 250 | Medium beam 40, 300 m | 200 in 3 of 5 slots |
| Assault | 3 | 850 / 200 | 8 | 8 | 300 / 75 / 375 | Medium beam | 170 or 255 |
| Destroyer | 4 | 600 / 200 | 14 | 12 | 250 / 62 / 312 | Disruptor Bomb 94, 450 m | 240 in 2 of 11 slots |
| Bomber | 5 | 700 / 100 | 15 | 14 | 300 / 75 / 375 | Light beam + Multi-Bomb (4 x 65, 250 m) | 140, 210, 280 or 350 |
| Escort | 6 | 500 / 80 | 20 | 16 | 250 / 62 / 312 | Stasis bomb | never while following its primary |
| Battleship | 7 | 1800 / 300 | 17 | 12 | 500 / 125 / 625 | Heavy beam 80, 600 m + Disruptor Bomb | 360, 540 or 720 |
| Mothership | 8 | 2800 / 350 | 15 | 8 | 700 / 175 / 875 | Heavy beam, Multi-Bomb, Stasis | 560 or 840 |

The threshold is `constitution x type_percent[slot]/100` (10-50% by incursion slot, fixed at spawn); the band is checked about every half game second, lower bound inclusive; a UFO in its band turns for the nearest gate, pays the escape cost and rejoins the pool. The slot is not visible, so the harness assumes the widest band of the type: Probe to Fast Attack and Destroyer rarely withdraw; Bomber, Assault, Battleship and Mothership have wide windows, so burst through them (AIR-5). **Engine today** (fix F7): the threshold comes from the role table (10% or 30% observed) instead of `type_percent` (`gamestate.cpp:1021,1089,1142`), over-withdrawing Infiltration Assault/Battleship and under-withdrawing Subversion, Transporter, Fast Attack and Destroyer ships; a narrow window either way.

### 4.2 Weapons: hits to bring a UFO down; ammunition as a dollar cost

`[DERIVED]` from engine weapon data at mean armour, 100% hits, no shield (`-` = cannot damage it):

| Weapon (price; slot; ammo; range) | Probe | Scout | Transp. | Fast A. | Destr. | Assault | Bomber | Escort | Battle. | Mother. |
|---|---|---|---|---|---|---|---|---|---|---|
| Bolter ($1,000; 1x3; recharge; 200 m) | 7 | 11 | 42 | 102 | 520 | 154 | 520 | - | - | - |
| Janitor array ($1,200; 1x3; 24; 250 m; guided) | 4 | 5 | 16 | 27 | 49 | 47 | 66 | 107 | 254 | 297 |
| Rendor ($2,500; 1x2; 140; 250 m) | 4 | 6 | 18 | 32 | 62 | 55 | 83 | 161 | 347 | 379 |
| Prophet array ($1,500; 1x3; 8; 350 m; guided) | 3 | 4 | 13 | 21 | 34 | 37 | 47 | 60 | 161 | 204 |
| Lineage ($6,000, week 3; 1x3; 110; 400 m) | 3 | 5 | 14 | 23 | 38 | 40 | 52 | 72 | 186 | 231 |
| Justice ($800; 1x3; **1**; 1000 m; guided) | 2 | 2 | 6 | 9 | 12 | 16 | 17 | 14 | 46 | 69 |
| Retribution ($1,100; **1x4**; **1**; 1150 m; guided) | 1 | 2 | 5 | 6 | 8 | 11 | 11 | 9 | 30 | 46 |
| Medium beam (hybrid craft; 2x3) | 2 | 3 | 8 | 12 | 16 | 21 | 23 | 21 | 66 | 95 |
| Heavy beam (Annihilator; 4x4) | 1 | 2 | 4 | 5 | 7 | 10 | 9 | 7 | 24 | 38 |

Slots: Hoverbike 1 x (1x3); Phoenix 2 x (1x3); Valkyrie and Hawk 2 x (1x4); Lancer and Retribution (1x4) do not fit a bike or Phoenix. UFO shields (200/400 points, 1 point/s) absorb whole hits; armour applies only to the overflow (`vehicle.cpp:2667-2705`).

**Engagement test `[DERIVED]`.** Engage a type only if the committed craft's hits per sortie >= 2.5 x hits needed (hits per sortie = ammo x 0.76 for Janitor/Prophet, x 0.64 for Rendor; one Janitor bike = 18 hits): Scout 5 hits -> 1 bike; Transporter 16 -> 3 bikes; Fast Attack 27 -> 4 bikes (speed 20 outruns bikes at 14); Assault 47 and Destroyer 49 -> 7 bikes; Bomber 66 -> 10 bikes; Escort/Battleship/Mothership are outside the bike envelope. The 2.5 is a guess for misses, dodge and shields (E2 tunes it). Collateral scales with shots fired (`hits / accuracy`), so guided Janitor (22 damage, about 18 hits per bike sortie) is the default; Justice/Retribution need the fewest hits but carry one round; the Bolter's low damage per shot does not help because city damage is charged per hit.

**Ammunition is a recurring cost.** `[ENGINE]` Reloads draw from the base's missile stock (`vequipment.cpp:200-218`; an empty stock leaves the craft unarmed) and the market sells vehicle ammunition **per missile** (Janitor $110, Prophet $200, Justice $330, Retribution $420, Powerfuel $10, Airguard round $8; `economy_tables.md` section 7.4): a 24-missile Janitor refill is $2,640. The original's basis (per round or magazine) is `[UNBOUND]` (lead: the vehicle-ammo rows of the 22-byte EconomyData table, e.g. vammo 9 at file `0x18E39A`). Missiles per kill = hits / 0.76:Probe 5 ($0.6k), Scout 7 ($0.7k), Transporter 21 ($2.3k), Fast Attack 36 ($3.9k), Assault 62 ($6.8k), Destroyer 65 ($7.1k), Bomber 87 ($9.6k), Escort 141 ($15.5k) per kill with Janitor arrays.

The starting 150 missiles are about 22 Scouts or 7 Transporters. Janitor and Justice are Megapol, Prophet and Retribution Marsec, Powerfuel Superdynamics: **the air arm dies with its vendors** unless stock was bought ahead (AIR-10). The recharging Bolter (Scout 11 hits, Transporter 42) is the ammunition-free hedge the two Phoenixes carry, at about 13 extra rounds of collateral per Scout.

### 4.3 Collateral, fire discipline and escape cost

**City damage (score category 6) [EXE C6 adjudicated].** Every projectile hit on a human-city building footprint, by **X-COM or alien fire**, destroyed tile or not, scores `-byte[tile+0x0C]`: 0 to 40, **mean about 9 per footprint hit, most often 20**. The vehicle-obstruction writer fires only for the Overspawn, so **a falling or crashing craft costs nothing in the original** (it charges only the building owner's relation toward the Aliens, -25). **Engine today** (fix SCORE-C6): the engine also charges every falling vehicle that plows through building scenery (`vehicle.cpp:1027-1030`, added by commit `fda36d6c`), an unmeasured share of the benchmark's city-damage sink; the projectile writer matches (`scenery.cpp:1183-1208`).

**Break-even hits `[DERIVED]`** = swing (kill score + escape cost) / cost per footprint hit: the building hits an engagement may cause before it is score-negative, ignoring relation:

| | Probe | Scout | Transp. | Fast A. | Destr. | Assault | Bomber | Escort | Battle. | Mother. |
|---|---|---|---|---|---|---|---|---|---|---|
| Hits at 9 per hit | 6 | 13 | 20 | 27 | 34 | 41 | 41 | 34 | 69 | 97 |
| Hits at 20 per hit | 3 | 6 | 9 | 12 | 15 | 18 | 18 | 15 | 31 | 43 |

**Relation cost of fire at buildings `[EXE F01-F05]`** (on the building owner's view of the firer, with ripple). Intentional fire (ordered to attack that building, or manual): -5 if the tile survives, -25 if destroyed. Stray fire (an attack order on a UFO whose missile hits a building): **0 if the tile survives, -5 if destroyed**. Alien fire only changes the owner's view of the Aliens. A non-UFO craft (Government, Megapol, police, civilian) destroyed outright costs -10 once. From a Government relation of 65 the funding cutoff (116 points away) needs about 5 intentional destroyed tiles, 24 intentional surviving hits, 24 stray destroyed tiles, or about 12 Government craft. **Engine today** (fix F02+F05): -5 per stray hit, -25 per destroyed stray tile, -30 per downed Government craft: 5 destroyed tiles of any intent, 24 hits or 4 craft reach the cutoff. **Escape cost:** a UFO leaving the human city costs `-floor(score/4)` in category 5 (withdrawn ships included; the benchmark's mean was -16/day at day 9 and -110/day at day 30, `failure_forensics.md` section 4); each alert adds -30.

### 4.4 Rules

* **AIR-1 Scramble.** WHEN a new alien craft appears in the city (UfoSpotted) at game speed <= 3 THEN record its type (HR-5), choose a target (AIR-2), order every armed agent-free flier (<= 13) to attack it, one UFO at a time. UNTIL it is destroyed, leaves, or the committed craft are < 40% HP.
* **AIR-2 Target order.** (a) a craft hovering over a Tier-0/0b building, inside the engagement test (highest priority, exempt from every score band of 4.8); (b) a Stasis Escort when the group can kill it (a frozen bike is dead) `[GUIDE: UFOpaedia Alien Ship Combat, https://www.ufopaedia.org/index.php/Alien_Ship_Combat_(Apocalypse)]`; (c) primaries by deposit speed-sum: Assault 35 > Destroyer 29 > Transporter 23 > Scout = Fast Attack = Bomber 16 > Probe 7; (d) escorts after their primary; (e) nearest first. Outside the engagement test do not engage, and **never fire over a Tier-0/0b roof**: a one-shot Justice/Retribution salvo cannot kill a Battleship (46 / 30 hits), Mothership (69 / 46) or Escort (14 / 9) and its misses (74-78% accuracy) land on the building being protected; the answer to an out-of-envelope UFO that has deposited is INF-1.
* **AIR-3 Attitude.** Strike group `BUTTON_ATTACK_MODE_DEFENSIVE` (standoff = range/2, dodge 80%); a decoy pair (>= 2 bikes) `EVASIVE` (never fires, dodge 100%) draws Bomber/Destroyer fire and drains Multi-Bomb ammunition `[GUIDE: UFOpaedia Alien Bomber, https://ufopaedia.org/Alien_Bomber_(Apocalypse)]`. The dodge percentages are invented in this engine (`vehicle.cpp:198-212`) and the original's engagement rules are `[UNBOUND]` (lead: the vehicle attack-mode handler); test with E4. Altitude: sources conflict `[GUIDE: UFOpaedia Cityscape Fighting, https://www.ufopaedia.org/index.php/Cityscape_Fighting_(Apocalypse)]`; default Standard until E3.
* **AIR-4** Never fly agents on interceptors: a shot-down craft kills them and costs its score (`vehicle.cpp:2090-2113`).
* **AIR-5 Burst the withdraw window.** WHEN the target is a Bomber, Assault, Battleship or Mothership and its estimated HP (type maximum minus the hits seen to land, 4.6, or the health bar if the UI draws one; never the exact `health` field) is within about 100 HP of its widest window (4.1) THEN commit every missile at once; a partial salvo that leaves it inside the window sends it to the gate (`-score/4`). Finish wounded UFOs at once.
* **AIR-6 Break contact.** WHEN a craft is < 40% HP or its magazines are empty THEN `BUTTON_GOTO_BASE` and rearm (instant, but it draws missiles and fuel from base stock; repair is 12 HP/h per Repair Bay). Do not follow a UFO through a gate.
* **AIR-7 Hover shot.** WHEN a drop on building B is in progress and the UFO is inside the envelope THEN fire guided missiles only; if B is Tier 0, hold anything unguided.
* **AIR-8 No hold-fire gate.** The benchmark rule "skip `intercept_ufos` when Government relation < 25" held fire 972 times in 162 logs, 819 with the relation already <= -50, while the mean Incursions score per save fell from -888 (day 15-18) to -3,145 (day 26+) (`failure_forensics.md` section 4). Delete it; never replace it by a hold-fire-below-score rule (4.8).
* **AIR-9 Escalation clock (4.7).** WHEN the lifetime UFO Shot Down score (this week + previous) reaches 800 / 1,800 / 3,800 / 7,800 THEN procurement trigger, never a stop: buy Prophet/Lineage/Justice-class weapons and add the shield to every engagement test (+9 Janitor hits for a 200-point shield, +18 for 400 `[DERIVED]`); expect cloaks at 4,000 and teleporters at 8,000.
* **AIR-10 Ammunition ledger.** WHEN the Stores screen shows fewer Janitor missiles than one full reload of every Janitor-armed craft (24 per array: 264 for 8 bikes + 2 Phoenix + the Valkyrie), or fewer Prophet/Justice/Retribution than their craft need, and the vendor is not Hostile THEN buy up to the weekly market cap (Janitor 50-260) inside appendix 01's runway gate; WHEN BUY-AHEAD triggers (8.1) buy missiles and Powerfuel with the kit; WHEN a missile type is at 0 and its vendor is Hostile THEN fight Probe/Scout/Transporter with the recharge lasers and decline types only missiles can kill. UNTIL stock >= one full reload. Log ammunition per kill (K-AIR4).
* **AIR-11 Fire discipline.** WHEN any craft is ordered to attack THEN the target is an alien craft; never order an attack on a building, never fire manually into the city, never attack a Government/Megapol/police/civilian craft. UNTIL always.

### 4.5 Fleet progression (what the air arm needs; purchases and cash are appendix 01's)

`[DERIVED]` Costs: Hoverbike $5,000 + Janitor array $1,200; Phoenix $12,000; Valkyrie $75,000; Hawk $100,000 from week 2 (`web_city_air.md` section 3.2). **Day 0:** 8 Hoverbikes + Janitor ($49,600), missiles topped to 264 (+114 = $12,540: eight new arrays alone draw 192 of the 150 in stock) and the 2 Phoenixes; the first UFO appears about 10-12 h after the start (5-6 h if both drains run, `[MEASURE]` F6). **Weeks 1-2** (Probe 9, Scout 9; then Transporter 3, Fast Attack 2): 10-12 armed fliers, losses replaced within 24 h, second Valkyrie by the end of week 2. **Week 3** (Assault; its Medium beam kills a bike per hit): >= 10 fliers, Prophet on the Phoenixes, engage with >= 7 Janitor bikes and accept <= 3 bike losses ($15k). **Weeks 4-5** (Destroyer, Bomber): Justice on bikes for salvos; Destroyer needs 7 Janitor bikes or 7 two-array Prophet craft, Bomber 10 bikes + a decoy pair; first Hawk only if cash exceeds 2 weeks of burn. **Weeks 6+** (Escort, Battleship week 7, Mothership week 8): no more bikes; a wing of >= 3 hybrids (Retaliator = 3 Medium beams, Annihilator = Heavy beam) when appendix 02 delivers, until then respond on the ground. Ammunition runs ~$5k in week 1 rising to ~$15-20k a week if ~8 UFOs a week are engaged (K-AIR4 replaces the assumption); a guide reports a Hoverbike lasting about 3 weeks `[GUIDE: AllOutWar guide, https://www.ufopaedia.org/index.php/AllOutWar's_X-Com_Apocalypse_Guide]`, so budget ~3 bikes a week ($18.6k) until K-AIR1.

### 4.6 City-damage measurement

Log per engagement the UFO type, craft, weapon, attack mode, altitude, Damage-to-City delta (read before the attack order and after the UFO is gone) and kill or escape; E2-E4 fit `q` = P(a miss lands on a building).

### 4.7 UFO gear gates and the escalation clock (F9)

Nine one-way latch flags are set at **every game midnight**. Rows 4-8 compare UFO Shot Down score (this week + previous): Superhuman **1,000 Small Disruption Shield, 2,000 Large Disruption Shield, 4,000 Cloaking Field, 8,000 Teleporter**; never cleared. Every craft launched for an incursion (primary, escort and attack slots alike) is created by one routine that blanks locked equipment, so UFOs launched after the next midnight carry the new gear; recovery loot is filtered by the flags at recovery time. 1,000 points = 20 Probes, or 10 Scouts, or 2 Battleships. **Engine today** (fix F9): `equipDefaultEquipment` runs only in the primary launch loop (`gamestate.cpp:1034`), so escort and attack launches never equip (59 of 175 launch slots, 34%): past the thresholds, engine fleets are weaker than the original's and interception margins tuned on them are optimistic. The ground-item rows (Tactical Missions 1000/2000/4000/8000 gating items 42/50/51/52) have no found live effect `[UNBOUND]`.

### 4.8 Score governor

**Cutoff form `[EXE G2 adjudicated]`.** At each Monday funding pass the cut tests `H` < -2,400 strictly, where `H` is the lifetime total of COMPLETED weeks, alone: the week being assessed never enters, so a bad week bites one Monday later (examples: -3,000 then +5,000 cuts at Monday 2; five weeks of -500 cut at Monday 6; +3,000 then -5,000 leaves H = -2,000 at Monday 3 and no cut). The next week cannot repair it. So this week's total becomes next Monday's `H` and the binding constraint is **lifetime total (Score screen previous + this week) >= -2,400 when the week closes**. The harness keeps **>= -2,000 at every Sunday 23:00** (400-point margin). A week-1 cut can only be the relation leg (Government->X-COM < -50). LIVE matches this rule; HEAD tests the lifetime total including the current week and re-tests in the UI (D1-D7), which is stricter and wrong.

**Bands.** Total > -800 green; -800 to -1,600 yellow (one Cult raid per week for about +250 tactical points each only while appendix 01's `tacticalMissions` cap leaves room, since every raid also advances the alien infantry tier, section 5); < -1,600 red; < -2,000 emergency (finish any research about to complete first). **The bands never switch the air arm off and AIR-2(a) is exempt from all of them**; they raise a cut-off on swing per Janitor hit (swing of 4.1 divided by the Janitor hits of 4.2: Probe 15.5, Scout 25, Transporter 11.7, Fast Attack 9.3, Assault 8.0, Destroyer 6.4, Bomber 5.7, Escort 2.9, Mothership 2.9, Battleship 2.5): green engages every type that passes the engagement test (Probe only if a craft is free), yellow only >= 5, red only >= 7.5, emergency only >= 11. Letting a UFO leave is not free (-score/4 now).

**Weekly income modifier `[EXE]`** (on `F`, strict, most extreme wins): `W` > 400 +F/20, > 800 +F/16, > 1,600 +F/12, > 3,200 +F/8, > 6,400 +F/5, > 12,800 +F/4; `W` < 0 -F/15, < -400 -F/10, < -800 -F/5, < -1,600 -F/4. Positive sources: research completion (mean +131 to +1,116 by day bucket, `failure_forensics.md` section 4), UFOs downed, Cult raids, alien-building raids (250-500 each). Benchmark (engine): 78% of saves at day 20 or later are below -2,400; incursions are the largest sink (mean -1,768 at day 20-24), city damage second, incidents third.

---

## 5. Tactical difficulty: what Superhuman aliens really are

**Rule [EXE TD1-TD3].** Every non-X-COM unit is scaled at creation (`FUN_0001f830` in TACP, `FUN_000ad93c` in UFO2P): `value = (base + rand(0..inc)) x (100 + kD)/100`. At D = 4: TU, health, stamina, reactions **x1.16**; psi energy x1.08, psi attack x1.04 (caps 100); psi defence `(base + 2D) x 1.04`; **natural armour x1.64** on every zone (types that have natural armour); the raw accuracy byte x0.84. Under OpenApoc's `100 - byte` convention, which a TACP stat routine also uses, this **raises** shown accuracy (Anthropod 0 -> 16, Skeletoid 30 -> 42, Spitter 38 -> 48, human guards 20..40 -> 33..50) `[UNBOUND: direction inferred from the fork's convention; the hit-chance routine is undecoded]`. X-COM agents are copied from the roster and never scaled; turrets, reinforcements and brood spawns are.

| Unit | TU | HP | Reactions | Armour (head, body, arms, legs) | Notes |
|---|---|---|---|---|---|
| Anthropod | 64 | 69 | 25 | 8, 9, 9, 9, 8 | |
| Skeletoid | 99 | 39 | 53 | 22, 26, 22, 22, 24 | small-arms damage is mostly absorbed |
| Spitter, Hyperworm, Brainsucker | 63, 95, 104 | 75, 51, 23 | 40, 34, 5 | 11 (Spitter legs 9), 11, 8 | |
| Popper | 127 | 40 | 9 | 11 | a mean-42 Disruptor hit no longer one-shots it (42 - 11 = 31) |
| Multiworm | 52 | 394 | 29 | 4, 13, 13, 13, 11 | |
| Megaspawn | 58 | 301 | 17 | 45, 60, 49, 49, 55 | |
| Psimorph | 41 | 487 | 34 | 16 | psi energy capped at 100 |
| Queenspawn | 52 | 696 | 23 | 49 | |
| Base turret (agent record 5) | 69 | 580 | 51 | 57 | accuracy 67 (7.4) |
| Human building security | 69-87 | 46-58 | | 32, 32, 26, 26, 32 | Cultist 26/26/19/19/29; Police 29/41/32/32/29 |

**Consequences.** Plan every fight against +16% HP/TU and +64% armour: prefer the heaviest damage per hit (Auto Cannon AP 40, plasma 35, AP grenades 56) over Lawpistol/M4000 volume against Skeletoids and Megaspawn; do not budget single-hit kills on Poppers or Skeletoids; stunning and capture are harder (6.5); priority targets and worm handling are in 6.1. Squad sizes in 3.5 and the garrison in 7.6 are floors. The original alien AI also **learns across battles** (it logs encounters to `EXPERIEN.DAT`, keeps the newest 10,000 and refits `BRAIN.DAT` at each debrief; features and effect size undecoded) `[EXE V-AG-14 adjudicated]`: do not assume alien tactics stay static over a campaign; the engine uses heuristic modes only (`game/state/battle/ai/*`) and does not port it, and `campaign-plan.md` lines 1177-1249 wrongly say the original does not learn.

**Engine today** (fix TD1+TD2+TD3): `AgentGenerator::createAgent` has no difficulty term and natural armour is unscaled (`agent.cpp:121-149`, `battleunit.cpp:1824,1912-1913`); the engine's Superhuman aliens are Novice-strength (Anthropod HP 60, Skeletoid armour 14-16, Popper HP 35 / armour 7), so every kill rate and one-shot assumption from engine logs is overstated, while the engine's aliens miss more (Anthropod accuracy 0 vs 16).

### Alien and guard equipment tiers [EXE TD4-TD6]

* **Alien infantry level** = the number of thresholds <= lifetime Tactical Missions score (category 0, this week + previous): Superhuman 1,000, 2,200, 4,000, 6,600, 10,000, 18,000, 28,000. Each of the weapon, grenade and equipment groups draws a uniform row 0..9; an empty row issues nothing: grenades appear from level 1 (3 of 10 rows at L1, 6 at L2, 8 at L3, all at L4), equipment from level 4 (7 of 10, all from L5). WHEN the score crosses a threshold THEN aliens gain a tier; a lethal Cult raid adds about 250, so the first tier-up comes after about 4 raids. UFO2P also subtracts a handicap when Organic Factories fall below a tenth of the alien city's structure table `[UNBOUND]` (lead: writers of the 40 x 14-byte table at VA `0x143A7E`). **Engine today:** the extractor drops empty grenade/equipment rows so grenades are issued every time at L1-L4 (`extract_agent_equipment.cpp:1073-1105`); low for the benchmark, whose tactical score stays below 1,000.
* **Human org guards** (`FUN_000b46c4`): tier = org start tech + week counter - 1 (+3 if taken over), clamp 0..11; three independent `rand(9)` draws into a 12-tier table; no difficulty read. So guard kit grows each week and jumps when an org is taken over (3.8).

---

## 6. Ground operations: sweeps, crash sites, Cult raids, alien dimension, capture

### 6.1 Extermination sweeps (city buildings)

* **Mission.** Building screen -> squad rows -> `BUTTON_EXTERMINATE`. Opposing side = the building's crew, the civilians the map adds and (engine) the owner's guards when the owner is Hostile (`battlemap.cpp:288-336`; the EXE builds org guards in `FUN_000aec70`/`FUN_000aedb0` `[UNBOUND for sweeps; lead: their callers for mission kind 0]`): after a takeover add 10-20 guards (3.8). Squad by drop type (3.5), floor 6 armed-verified (4 for a Probe), at most the transport's seats.
* **Roles per 6-8:** 1 heavy (Auto Cannon, **AP clip only**), 1 sniper, 4-5 assault (M4000 or dual Lawpistol), medi-kit on everyone, 2 AP + 2 stun + 1 smoke grenade each; androids first for Brainsucker-heavy crews (Scout, Transporter, Destroyer, Assault deposits hold 2, 3, 4, 5; androids are immune). `oa_loadouts.py` roles stay; add the AP-only rule and 3.5's verification.
* **Mode.** **Real time at game speed 3 with pause on first contact, never turn-based:** Poppers, Brainsuckers and Hyperworms charge and attack in the turn you see them, and turrets fire continuously only in real time `[GUIDE: UFOpaedia Real-Time vs Turn-Based, https://ufopaedia.org/Real-Time_vs_Turn-Based_(Apocalypse)]`. Aimed or snap fire, kneel, 4-man buddy rule, pods into backpacks the moment a launcher carrier dies (the harness already runs real time, `oa_play.py:3248-3308`).
* **Priority targets.** Brainsuckers and Poppers first (24 and 17 of 73 observed deaths), then Psimorph/Micronoid, then Skeletoids (armour 22-26, section 5). A Popper must die outright (Disruptor, Devastator, Toxin) or be stunned; a wounded Popper/Multiworm that bleeds out still detonates / bursts into 4 Hyperworms `[ENGINE battleunit.cpp:2489-2490, FIXME]`. Hyperworms take 150% from explosives and fire; Multiworms (394 HP, armour 13) fall to AP grenades (56).
* **HE and relation.** AP only inside city buildings by default: civilians die to blasts (-10 mission score each) and indoor explosives destroy loot. The EXE also applies a tiered post-battle relation hit and a balance debit to the raided owner for mission types 1 and 4 (what its `worth` counts is `[UNBOUND]`, F10/F-M13); the engine has none: re-test with E7. A tactical kill costs the victim org's view of X-COM -3S (not hostile) or -S/2 (hostile), S = 10 for humans: a hostile guard -5, a non-hostile one -30.
* **Retreat.** Withdraw when the conscious squad is < 50% of start **and** visible hostile units >= conscious squad (visible only, HR-2). **Never abort a base defence** (7.5). A sweep retreat returns survivors to the same building `[ENGINE battle.cpp:2956-2995]` and buys nothing against the clock.
* **After.** Wounded -> Medical Bay; re-run the equip pass; re-sample the owner's graph (INF-5); **GND-6** write one ledger row per sweep, raid or recovery (drop or crew type, squad, kit, result, casualties, graph before/after). A 900 s wall-clock cap ended 13 of 35 base defences by timeout (`harness_audit.md` R3): use game time and stall detection (HR-11).

### 6.2 UFO crash-site recoveries

**Recovery and unlock rules `[EXE RR-12, RR-13]`.** Only the Probe and Scout are unmanned. Recovering one sets only that type's own topic flag: no loot, none of the three alien-system flags. Alien Control System, Energy Source and Propulsion System are written in one place, the manned ground-mission branch, only if the mission is **won**; the craft is removed regardless. **Engine today** (fix RR-12+RR-13): all ten types carry the three unlocks (`extract_vehicles.cpp:147-156`), Probe/Scout force-complete them and hand over equipment as cargo (`cityview.cpp:4818-4825`), and a manned recovery unlocks without a `playerWon` test (`battle.cpp:3646-3656`): the engine tech path runs 5-13 days early (`web_research_tree.md` D-2) and a lost or aborted manned recovery still unlocks. Do not plan on the early path: the original needs a won ground mission on a manned UFO (the first Transporter, week 2).

Crew at a crash site `[ENGINE data crew_downed]`; aliens are scattered over the wreck. Wrecks self-destruct after 10-90 min (Probe, Scout), 15-120 (most types, Assault up to 180) or 60-240 (Battleship, Mothership) `[ENGINE vehiclemission.cpp:44-57; EXE UNBOUND, lead: the crashed-vehicle timer written in the crash branch of FUN_00057c8c]`:

| Wreck | Aliens | Squad (min) | Must include |
|---|---|---|---|
| Probe, Scout | unmanned: no ground battle | 1 agent aboard | - |
| Fast Attack | 11 (6 Anthropod, 2 Brainsucker, 1 Popper, 2 Spitter) | 8 | - |
| Transporter | 15 (4 Anthropod, 2 Chrysalis, 2 Multiworm, 2 Egg, 1 Popper, 4 Spitter) | 8-10 | Bio-Transport Module if a live specimen is wanted |
| Destroyer | 14 (8 Anthropod, 3 Skeletoid, 3 Spitter) | 8-10 | - |
| Assault | 18 (6 Anthropod, 2 Chrysalis, 4 Popper, 2 Skeletoid, 4 Spitter) | 10-12 | grenade stock for Poppers |
| Bomber | 15 (3 Anthropod, **3 Micronoid**, 2 Popper, 7 Skeletoid) | 10 | **>= 4 androids** (Micronoid Psi Attack 65) |
| Escort | 13 (3 Anthropod, **1 Megaspawn**, 2 Popper, 7 Skeletoid) | 10 | Devastator/Toxin; Disruptor armour if possible |
| Battleship | 30 (8 Anthropod, 3 Megaspawn, 3 Micronoid, 5 Popper, **1 Psimorph**, 5 Skeletoid, 5 Spitter) | not before Toxin B + Disruptor armour; 12 incl. >= 6 androids | |
| Mothership | 25 (4 Anthropod, 2 Megaspawn, 3 Micronoid, 5 Popper, **2 Psimorph**, 9 Skeletoid) | same | |

Squads are floors under TD1-TD3. **GND-1** WHEN an alien craft crashes THEN launch the QRF within 10 min if it meets the table minimum (QRF 2, or QRF 1 when idle, stays aboard its transport at the base), unmanned wrecks need one agent; UNTIL the recovery battle is won or the wreck is gone (the benchmark's 36 wrecks produced 7 battles because 9 had "no crewed craft" and 11 click-refused at the screen centre, harness defects D9/D18). **GND-2** WHEN any ground dispatch is ordered THEN squad >= table minimum, kit verified (HR-6), `present >= G` after departure (7.6); decline otherwise. **GND-3** wreck priority is research input: the first manned Transporter (hard gate for the Bio-Transport craft; any won manned recovery unlocks the alien-system topics), then the first Destroyer, Assault, Battleship. The Bio-Transport Module can be researched from day zero. **GND-4** bring a craft with a Cargo Module (loot) and, when a live specimen is wanted, a Bio-Transport Module on the dispatched craft (6.5).

### 6.3 Cult of Sirius raids (the "steal from the Cult and sell" pillar; economics are appendix 01's, the constraints below are military)

| Constraint | Value | Evidence |
|---|---|---|
| Button, gate | `BUTTON_RAID` at the temple (EXTERMINATE passes the Aliens as loot owner and pays nothing). The launch forces X-COM->owner to <= -60, which passes the loot generator's hostility gate by construction; the write is transient. | [EXE F-M10, F-M11 adjudicated]; `battlemap.cpp:1161` |
| Loot | Per loot spot `i = rand(4)` into the owner's 15-slot row; Cult A: Lawpistol, AP grenade, Psiclone, Prox mine, Psiclone; B: Psiclone, HE, Auto cannon, Psiclone, Psiclone; C: Psiclone x5. A Temple of Sirius has 23 spots: about $39-77k gross at base price, 8-16 Psiclone `[MEASURE: spots present per generated battle]`. | [EXE F-M9, F-M10] |
| Cargo | A craft with an installed **Cargo Module** must be at the building or the haul is destroyed (engine: any `getMaxCargo() > 0`); check the Psiclone delta (5-9) after the first raid, stop if 0. | `battle.cpp:3217-3340` [ENGINE]; EXE loot recovery `[UNBOUND]` (F-M14) |
| Squad | >= 8 armed-verified, armour, AP. Guards 7-12 (average 10 x 0.75-1.25), scaled by section 5 (Cultist HP 46-58, armour 26/26/19/19/29), kit tier = org start tech + week - 1 (+3 if taken over). | [EXE T10, BD12]; `income_cult_raids.md` section 2.4 |
| Relations | Cult->X-COM starts -70..-50 (hostile in all 21 seeds): each hostile guard killed costs -5, no other org objects. Never raid a sponsor (a non-hostile guard kill is -30 plus ripple). | [EXE F11, T9] |
| Counter-raids | A hostile Cult schedules raids on X-COM buildings: Raid 25% per trigger (Raid 29.1% + Storm 16.4% once taken over). **The garrison floor of 7.6 stays home.** | [EXE BD10]; 7.3 |
| Score | A lethal raid adds about +250 tactical points (alien infantry tier after about 4 raids), stun-only about +60. | `income_cult_raids.md` section 3.8 |
| Selling | The market cuts price from **standing stock** of X-COM-makeable and alien goods (Psiclone, Elerium, AG missiles): sell each type at most about 0.075 x max stock a week (about 3 Psiclone) or in one lump per type, spread across types (a one-off dump of 100 Psiclone leaves 0.755 of base price after 26 weeks); a type's price falling on a Monday means hold further sales of it. **Engine today** (fix F-M1+F-M2): these goods never lose price and weekly trickles are free, so engine cash overstates repeatable loot income beyond about six weeks. | [EXE F-M1, F-M2 adjudicated] |
| Abort, never | Abort on > 40% squad loss in the first 2 game minutes (suggested, unmeasured, `income_cult_raids.md` section 5). Never heavy explosives indoors (destroys loot); the Cult cannot be bribed (`bribescreen.cpp:68-80`). | |

Priority: below Tier-0 sweeps, base defence and the first manned-UFO recovery (3.5). **GND-5** WHEN a Cult raid is a candidate THEN the table's preconditions hold (>= 8 verified, Cargo Module, garrison home) and the RAID button is used; UNTIL abort, a base raid event, or the tier tripwire.

### 6.4 Alien-dimension building raids (squad doctrine; gating and research are appendix 02)

Raid access needs research record `88 + f` complete; winning building `f` unlocks the next topic in the chain. The Alien Dimension (6,000 man-hours) precedes the first topic (30,000, then +2,000 per step) `[EXE VC-2, VC-3]`. **Engine today** (fix VC-2+VC-3+RR-14): the first topic is 38,000 man-hours with no prerequisite and startable on day 0, bound by file position; the research schedule is early and mis-ordered.

| Raid | Building | Reinforcement interval `[GUIDE: Wong FAQ table 16.1.6, https://gamefaqs.gamespot.com/pc/36014-x-com-apocalypse/faqs]` `[UNBOUND TD14]` | Effect of winning | Hazard / squad note |
|---|---|---|---|---|
| 1st | Sleeping Chamber | 40 s | unlocks the Food Chamber topic | objectives explode; Micronoids; >= 4 androids |
| 2nd | Food Chamber | 35 s | **frees a taken-over Mutant Alliance** | 2 Psimorphs guard the bulbs (burst radius 8); >= 4 androids |
| 3rd | Alien Farm | **12 s** | | every white cube must go; ~12 Vortex Mines; hardest early clock |
| 4th | Maintenance Factory | 30 s | | chain explosions |
| 5th | Incubator | 20 s | | Skeletoids with Dimension Missile Launchers; Disruptor armour |
| 6th | **Control Chamber** | **10 s** | **permanently stops UFO growth**; arms a one-shot mass assault | open ground; thin the dimension fleet first |
| 7th | Spawning Chamber | 18 s | | Queenspawn; Vortex Mines on pads |
| 8th | Organic Factory | 20 s | | 7 levels, collapsing walkways |
| 9th | Megapod Chamber | **10 s** | | 4+16+4+2 alien turrets |
| 10th | Gate Generator | **5 s** | **victory** | 18 generator plants |

* **Success rule `[EXE VC-1 adjudicated]`.** The mission is won when the **objective tiles are destroyed**, then evacuating is fine; killing every alien without destroying them does not win. On success the building collapses, the next topic unlocks, the C7 score is credited (Sleeping 300, Food 300, Alien Farm 250, Maintenance 300, Incubator 400, Control Chamber 350, Spawning 400, Organic Factory 250, Megapod 500, Gate 500) and, for the Gate only, "THE ALIENS ARE DEFEATED" shows. **Engine today** (fix VC-1): the engine wins only by killing every alien and keeping a conscious unit (`battle.cpp:3573,3659-3661`); destroying the generators and evacuating gives no unlock, collapse, score or victory, for all ten buildings. Until fixed, kill everything and stay.
* **Reinforcements `[EXE TD12]`.** One unit per timer expiry, from the species present among Anthropod, Skeletoid, Spitter, Popper, on an intact 2 x 2 ground pad, scaled like any unit; the timer resets whether or not a unit appeared. A long raid faces one reinforcement per interval: finish the objective, not the clean-up. **Engine today** (fix TD12): four per expiry (`battle.cpp:1974-1999,2333-2385`), about four times the pressure, and the intervals themselves are Wong's figures, not EXE data.
* **UFO growth `[EXE UFO_GROWTH_STOP]`.** Weekly growth is skipped permanently once the Control Chamber (6th raid) is won; the Organic Factory has no growth effect. **Engine today** (fix UFO_GROWTH_STOP): the engine gates growth on the Organic Factory (8th raid, `ufogrowth.cpp:51-70`), two raids and about 13 days later.
* **Mass assault `[EXE VC-10, F5]`.** Winning the Control Chamber arms one wave: at the next invasion-timer expiry up to 20 ships (types 1-9, no Probes) launch round-robin from the dimension stock on random buildings, including bases, with role 9 (never withdraw); **if fewer than 10 are gathered the flag clears and nothing launches**. WHEN preparing the 6th raid THEN thin the standing fleet first (that stock is on no screen; `[MEASURE]` it as UFOs seen in the dimension plus weekly growth minus kills). **Engine today** (fix VC-10+F5): no wave is sent; do not plan around the cancellation trick or the retaliation. Targets and stock decrement are `[UNBOUND]` (leads: `FUN_0006d994`, writers of `0x143D40`).
* **Crossing.** Send only crossing-capable craft (the ten alien craft, Dimension Probe, Bio-Transport, Explorer, Retaliator, Annihilator); each visit heals every UFO of the dimension fleet, so wound-and-return fights do not work; set `CrashingDimensionGate` false on the engine (VC-8).
* **Squad.** >= 10 with >= 4 androids (a Psimorph controls a human at Psi Defence 15-30 in 71-85% of tries, an android in 1-4%, 8.5), Toxin B/C or Devastator as primary, Vortex Mines (the only item that destroys orange pads), Disruptor armour once researched, never leave an agent behind. Entry gate: Toxin B researched, Disruptor armour issued, >= 10 soldiers and >= 4 androids available **after** garrisons are met (7.6).

### 6.5 Capture and stun drill (when appendix 02 asks for a live specimen)

* A unit is unconscious iff health != 0 and stun < health; stun decays 1 per second; all thirteen species can be captured; Eggs and Chrysalises count as captured without stun and never block victory `[EXE TD15, TD16]`.
* Storage needs a Bio-Transport Module (`alien_space` 20, the only module with any) on a candidate craft: for building missions any X-COM craft in the mission building; for **UFO recovery only the dispatched craft** (failure loses aliens, bodies and cargo); **base defence skips the craft test** (a virtual carrier; its Alien Containment test is `[UNBOUND TD20]`, lead: `FUN_000b44a4`, `[0x1304CC]`) `[EXE TD19 adjudicated]`. **Engine today** (fix TD19): UFO recovery pools every player vehicle near the site (`battle.cpp:3989-4018`); base defence requires containment capacity (`battle.cpp:3684-3700`).
* Stunned, Egg and Chrysalis aliens credit their score to the tactical total **regardless of storage**, again to the leadership base; leadership = 0 if `30 x casualtyPoints > 1000` (4 dead agents), else `base x (100 - 3C)/100` `[EXE TD17 adjudicated, TD18]`. **Engine today:** the live-capture score is withheld without storage (`battle.cpp:2886-2905`) and there is no leadership clamp.
* Drill: Stun Grapple (90 stun, 3 tiles) and stun grenades (80 gas, ignore shields; 0% on Micronoids and Chrysalis); one spare agent stands on each stunned Popper/Skeletoid/Psimorph/Micronoid/Megaspawn body so it does not rise; never re-stun a stunned Popper; chip Multiworm/Megaspawn/Psimorph to 50-67% HP before grappling. Easiest first: Hyperworm/Chrysalis/Egg, Multiworm (the Toxin B gate), Popper, Skeletoid, Anthropod, Spitter; then Micronoid, Psimorph, Megaspawn, Queenspawn `[GUIDE: AllOutWar guide, https://www.ufopaedia.org/index.php/AllOutWar's_X-Com_Apocalypse_Guide]`.

---

## 7. Base defence

**Scope.** A base with no live agent present when the event fires is destroyed on the spot; a base held by unarmed staff fights and loses; losing the last base is game over. In the benchmark 7 of 162 runs ended that way (`failure_forensics.md` section 9, engine builds that still had HEAD's invented triggers).

### 7.1 What starts a base defence `[EXE BD01a, BD02, BD05, BD06, BD07, BD15]`

A defence is entered from exactly one place (`FUN_000ac348` reason 5, via `FUN_000ac08c`, 7 call sites, byte-scanned for completeness):
1. An **Infiltration UFO** landing over an **active X-COM base** attacks at once: no exposure read, no deposit, no roll.
2. A **Subversion UFO** attacks only if the base's exposure word is non-zero; otherwise it sets the word and nothing happens. Exposure is set permanently by a Subversion landing, by 3 or more aliens stepping into the base building in one hourly move (`rand(100) < 5 x moved`), or by a post-raid transfer. It matters only for Subversion targeting.
3. A hostile org's **Raid (2) or Storm (3)** incident against an X-COM-owned building that is an active base (7.3).
4. **Nothing else**: alien movement into a base building and alien detection of one never start a defence. Overspawn UFOs deposit nothing.

Infiltration UFOs choose uniformly among alive buildings within `D <= 60` of the UFO (`D = 2 max(|dx|,|dy|) + min(|dx|,|dy|)`): **P(this base | one landing) = 1 / (alive buildings within D <= 60)**. At the building centres that is 17-30 buildings (mean 24): Thomas More Tower 30, Grimoire Block 28, Warehouse Eleven 26, Warehouse Eight 25, Warehouse Ten 22, Warehouse Two 19, Lovecraft Block 18, Warehouse One 17, i.e. **3.3-5.9% per landing** `[DERIVED from the extracted building coordinates; the true n is taken at the UFO's position when it picks, MEASURE E10]`. Weeks 1-3 are all Infiltration at about 3.5 (single drain) to 7 (double drain, `[MEASURE]` F6) incursions a week, so before interception the chance that some landing hits a given base in three weeks is about 35-60% `[DERIVED]`. Each added base adds its own 1/n.

**Engine snapshot.** LIVE removed HEAD's invented detection/movement triggers (`building.cpp:835-844, 1029-1040`), later committed in local `879d0d73` (04 A0); no new snapshot commit is needed. HEAD had `detect` and `alienMovement` call `Base::die`/`DefendTheBase` (:815-826, 1022-1036); 13 of 15 logged benchmark defences were movement-triggered. This does not attribute each old binary to a commit. LIVE leftovers: Overspawn can start defences (BD02), a 5% exposure roll after a landing (`vehiclemission.cpp:2637-2642`, BD05), a Manhattan <= 30 target pool (BD15), and forced detection of the player's own crewed buildings raising an `AlertScreen` whose `BUTTON_EXTERMINATE` turns any mission at a base into a BaseDefense battle (`battlemap.cpp:176`): **never press EXTERMINATE on an alert for an X-COM-owned building.** A defence shows as the BASE ATTACK dialog, or "X-COM base destroyed by hostile forces" if no live agent was present.

### 7.2 Attackers `[EXE BD03, BD12]`

* **UFO origin:** the static per-type crew table minus Egg, Chrysalis and Queenspawn, at most 36 aliens and 60 units in all; resident aliens do not fight. Attackers (crew -> attackers): Probe 4 -> 4, Scout 13 -> 10, Transporter 20 -> 15, Fast Attack 10 -> 10, Destroyer 22 -> 17, Assault 27 -> 22, Bomber 8 -> 8, Escort 13 -> 9, Battleship 20 -> 15, Mothership 24 -> 19. With 36 X-COM-side units the hostile side is limited to `60 - 36 = 24`, above every row.
* **Org raid:** `n = average_guards x (rand(0..50) + 75)/100`, capped at 20; Superhuman: Cult 7-12, Psyke 7-12, Diablo 9-16, Government 10-17, Megapol 12-20, Osiron 12-20, most corporations 5-17. Guards are scaled by section 5 and armed by tier + week (+3 if taken over).

**Engine today** (fix BD03): UFO-origin attackers are the building's whole crew including eggs and residents (`battlemap.cpp:182-190`; an Assault fields 27 vs 22). The org formula is identical.

### 7.3 Hostile-organisation raids `[EXE BD08-BD12]`

`FUN_00092060` runs once per game day. For A in 2..26 and B in 0..26 except the Aliens: the pair passes if `M[A][B] <= -50` and A has a building; pressure = `max(1, snapshot - M)`; **trigger iff pressure > rand(0..timer)** and one of 10 incident slots is free. A trigger sets the shared timer to `80 - 2d` = 72, otherwise it drops by 1; it starts at 0, so **the first hostile pair in index order triggers at the first midnight**: the Government (2) and Megapol (3) are tested before the Cult (4), so a Cult->X-COM raid on night 1 is likely (the Cult starts hostile) but not certain `[UNBOUND: start relations of the earlier pairs not computed]`. The mission arrives 50-1250 min later against a uniformly random live building of org B. The snapshot is refreshed only at start, one cell per hourly rift visit (each org every 27 h) and on load. Table A serves Megapol, Psyke, Diablo, Osiron and any taken-over org, else table B; only Raid and Storm attack a base, and on a non-base X-COM building they do nothing. Per trigger, when the random X-COM building is a base:

| Raider | Raid | Storm | Raid or Storm | Base attack per trigger, 1 base / 2 bases |
|---|---|---|---|---|
| Cult, most corporations | 25.0% | - | 25.0% | 12.5% / 16.7% |
| Megapol | 36.8% | 25.0% | 61.8% | 30.9% / 41.2% |
| Psyke, Diablo, Osiron, any taken-over org | 29.1% | 16.4% | 45.5% | 22.7% / 30.3% |

The start pool is the base plus the Recyclotorium (building 44), so P(base | Raid or Storm) = 1/2 with one base, 2/3 with two; it is 1 if the owner remap at VA `0x1017C` empties the non-base pool `[UNBOUND; lead: FUN_00014854 loader vs the remap]`, which would double these figures. **Frequency `[DERIVED]`:** with a pressure of 1 the pair triggers uniformly 1..73 failed checks after each reset, each hostile pair costing one check a day, so one trigger per about 37/n days with n hostile pairs aimed at X-COM (an upper bound on the share: other hostile pairs also use the timer). Expected days between base attacks with one base: n = 1 / 3 / 6 / 12 -> Cult-class 296 / 99 / 49 / 25; taken-over-class 163 / 54 / 27 / 14; Megapol 120 / 40 / 20 / 10 (multiply by 0.75 with two bases). **A takeover is a shock:** the org's view of X-COM falls by 100-165 in one step, so its pressure exceeds any timer value (<= 72) and the pair triggers at the next daily check unless its hourly rift visit refreshed the snapshot first `[DERIVED]`. Hostile means <= -50 (the funding cutoff is strictly below).

**Engine today (BD08, both trees):** the raid target comes from `Organisation::buildings` (`organisation.cpp:71-80`), filled only by `City::initCity`; the start base, `BaseBuyScreen` and `Base::die` change `owner` without touching it, so in a **continuous** run X-COM's pool is only the Recyclotorium and `RaidMission::execute` takes the else branch (relation +7/+12 and an event, never `DefendTheBase`, `organisation.cpp:1308-1338`); base raids appear only after a save/load (census of 100 saves: 49 pending raids on X-COM buildings, 48 on the Recyclotorium, 1 on a base). Logged base-defence frequency therefore understates the original and garrison sizing from engine logs under-prepares. Parity gaps: no 10-slot cap, Aliens iterated as B (BD11), flat incident maps (Cult Raid 10% vs 25%, BD10), `<` for `<=` (BD09). Fix BD08: live scan of `city->buildings` by owner; non-base Raid/Storm a no-op.

### 7.4 Who defends: roster, staff dilution, Security Station turrets `[EXE BD04, TD7-TD10]`

* **Roster** (`FUN_000ac7d0`): every employed agent **located at the base** (not merely homed) plus class-0 agents aboard craft docked there. Armament is never tested: unarmed staff defend and die. Four classes by byte `+0x23` (0 combat; 1-3 technical staff). Budget `B = 36 - T`, `T = min(12, 4 x finished Security Stations)`; if the present total exceeds `B`, **each class is cut to `floor(count x B / total)`**, first n per class in array order. Also 36 X-COM units and 60 units in all; `T` turrets are added to the total.
* **Turrets** (TACP `FUN_00024110`): one per map tile with feature byte `0xE6-0xED` = **4 per Security Station sector (23) and 4 per Advanced one (24)**; roster units take slots first, turrets what is left of 60. There is no cap of 3 stations (the web claim is not found; 12 slots are only the reservation) `[TD11 UNBOUND for the 4th and later station]`. Unit = agent-type record 5: 500 HP, TU 60, reactions 44, armour 35 -> scaled **580 HP, TU 69, reactions 51, accuracy 67, armour 57**. Weapons: sector 23 Megapol Plasma Gun + 2 Megapol Plasma Pods (pod damage 35, accuracy 55, delay 120, range 900); sector 24 Disruptor Gun (42, 30, 128, 800); the real shot counts are undecoded `[UNBOUND TD9, turret ammo]`.
* **Staff dilution `[DERIVED from the roster rule]`.** Researchers and engineers must be physically at their home base to work (appendix 02), so a lab base always has staff present. Soldiers fielded: `F = floor(S x B / (S + Q))` when `S + Q > B` (S = combat agents present, Q = technical staff present; the staff are cut by the same ratio and fight unarmed), else `F = S`. Soldiers `S` needed for `F` = 8 / 12:

| Staff Q present | B = 36 (no station) | B = 32 (1) | B = 28 (2) | B = 24 (3 or more) |
|---|---|---|---|---|
| 10 | 8 / 12 | 8 / 12 | 8 / 12 | 8 / 12 |
| 20 | 8 / 12 | 8 / 12 | 8 / 15 | 10 / 20 |
| 30 | 9 / 15 | 10 / 18 | 12 / 23 | 15 / 30 |
| 40 | 12 / 20 | 14 / 24 | 16 / 30 | 20 / 40 |

  A station converts 4 slots into 4 turrets (580 HP, armour 57) that beat any staff member in those slots, but it lowers `B`; a staff-heavy base needs both stations and a large combat roster, or fewer people present.

**Engine today** (fix BD04+TD10+TD8+TD9): the engine drafts docked-craft agents, then every Soldier-role agent in the base, then non-soldier staff by HOME base even if away or in transit, soft cap 36, soldiers first, no proportional scaling (`battlemap.cpp:205-256`); turrets (`guardianLocations`, 4 per finished station, uncapped, not charged to the 36) are `AGENTTYPE_X-COM_BASE_TURRET_{LASER,DISRUPTOR}` with **armour 0, unscaled, invented guns** (laser 26/95/42, disruptor beam 70/75/40; `agent_types.xml:1039-1228`, `agent_equipment.xml:83-210`). Garrison sizing from engine logs does not transfer: the engine fields more soldiers and softer turrets.

### 7.5 Base loss `[EXE BD13, BD14, BD16, BD17]`

A base is lost when a defence ends without success: **loss or retreat** (the briefing says so). **No agent or craft dies and the building is not collapsed:** the base's owner becomes the Government and every employed agent and X-COM craft homed there is **re-homed round-robin to the next active base** (game over only when the last base goes). A base building also falls by **collapse** when `10 x sum(General tile values) < original sum` (more than 90% of its value gone; verified on all 109 and 96 buildings of the two maps), zeroing the resident crew, so Attack/Bomber bombardment or a crash can destroy a base well before the last tile goes. A WON defence debits `50 x worth` (about $50 per damaged tile, inferred) from X-COM with no funds check `[BD14b]`. **Engine today** (fix BD14a+BD16, BD13): `Base::die` calls `Agent::die`/`Vehicle::die` on everything present, so a second base saves nobody who was there and away units keep a dangling home; the base dies when `buildingParts` is empty (`building.cpp:1086-1113,1165`); no repair bill. **Never let a base defence be lost and never retreat from one** (the base is gone either way).

### 7.6 Garrison policy

* **Armed-verified** = loaded **lethal** primary, >= 2 spare clips, five armour pieces, medi-kit, confirmed on the equip screen. A Stun Grapple or Mind Bender alone is not lethal (2 of the 10 starters carry only a Stun Grapple, `economy_tables.md`); "any weapon item" is not enough (`gamestateintrospect.cpp:559-615` counts unloaded weapons; loadout checks were 648 zero, 337 partial, 23 full of 1,008, `failure_forensics.md` section 5). Technical staff cannot be armed (no inventory, civilian AI).
* **Present** = disembarked in the base building or, in the original, a class-0 agent aboard a docked craft. **Engine today** (BD17/F17): the emptiness test is `currentAgents.empty()` and ignores agents aboard docked craft (`vehiclemission.cpp:2644`, `organisation.cpp:1299,1323`): **keep at least one armed agent disembarked at every base.**
* **Target fielded soldiers `f`** (the 7.4 table gives the roster): **8** while attackers are <= 15 (weeks 1-2: Probe, Scout, Fast Attack, Bomber, Transporter; Cult 7-12); **10** from week 3 (Assault 22, Destroyer 17, Government/Diablo raids 9-17); **12** once a militarized org (Megapol, Osiron, Diablo, Psyke) is Hostile or taken over (12-20 attackers) or Mothership/Battleship landings are possible `[DERIVED from the 7.2 attacker counts and the 8-12 soldier planning band; E9 refits]`. Count the QRF only while it is home (in the original its agents aboard a docked craft defend too).
* **Start-up.** The start roster is 10 soldiers + 15 technicians (appendix 01), so with up to two stations everyone present is fielded; with 6 away on the INF-0 sweep four are home, accepted for the first hours.

### 7.7 Security stations, layout, procedure

* **BASE-1 day 0 and the day each base is bought:** two Security Stations on corridor tiles next to the Access Lift ($2,000, 2 days, $200/week `[ENGINE data facility_types.xml]`); a third and later ones while `F` stays >= `f` after the station (7.4). The Advanced Security Station ($8,000, 4 days, $300/week) needs `RESEARCH_ADVANCED_SECURITY_STATION` (18,000 man-hours). A station under construction is absent from the defence map (the original maps it to a construction sector).
* **Layout** `[GUIDE: UFOpaedia Security Station, https://www.ufopaedia.org/index.php/Security_Station_(Apocalypse)]`: attackers spawn around the Access Lift and inside every Vehicle Repair Bay, technicians in Living Quarters, soldiers in ordinary rooms. So no Repair Bay at non-assault bases, Living Quarters far from the lift, stations beside the lift, corridors free for proximity mines (Marsec $300, week 2). Warehouse Ten's lift has one open neighbour (6,2): a station there seals it; Warehouse Eight: stations at (3,4) and (5,4). The start base is random (1 chance in 8 of the weak Thomas More Tower).
* **Procedure:** real time at speed 3 with pause; soldiers hold inside turret coverage and let hostiles come; no retreat (aborting a base defence can also crash on return, GH#1689); medics at the rear; staff stay put.

### 7.8 Rules

* **BASE-1** (7.7). **BASE-2 Never empty or diluted:** WHEN any dispatch would leave a base with fewer fielded armed soldiers than `f` (7.6, with the remaining people) or no armed agent disembarked THEN shrink or decline it; UNTIL never (the benchmark's `select_armed_squad` bypassed the `garrison` gene and `crew_transport` kept only 2 at home, `oa_play.py:4652-4662`). **BASE-3** arm before hiring (AGT-1).
* **BASE-4 Shock response:** WHEN an organisation turns Hostile or is taken over THEN within 24 h recompute `f`, fill and verify every base, and keep the QRF home for 48 h (the raid arrives 50-1250 min after the daily check).
* **BASE-5 Drop watch:** WHEN S-DROP is on a base building THEN every armed agent there moves to the lift and stations; the defence follows automatically.
* **BASE-6 Search watch (week >= 5):** WHEN a base was passed over by a Subversion UFO (exposed for good) THEN raise `f` to 12 and park an armed interceptor pair above it (kill the primary of the next search incursion).
* **BASE-7 Stores:** >= 2 spare armour sets, 2 spare weapons, 3 clips each per base, checked each Sunday. **BASE-8** expansion: 7.9. **BASE-9 Dilution check:** WHEN a lab is added, staff are hired, or a station is built or lost THEN recompute `F = floor(S x B/(S + Q))` and hold the 7.4 table value; UNTIL `F >= f`.

### 7.9 Expansion (the "build new bases" pillar)

A new base adds production space and a target (+1/n per landing; P(base | Raid) 1/2 -> 2/3 -> 3/4, 7.3); in the original it also makes a lost defence survivable (7.5). **BASE-8 uses 01 BS-P:** require space/production need or affordable insurance, RG on the complete purchase/staff/kit cost, and a staged garrison above every existing base's depth and the QRF. Do not require the Bio-Transport hull before buying the base needed for its Advanced Workshop. Hostility or a calendar date never bypasses these gates. Stage before BUY, disembark and read back the computed §7.6 garrison before time advances, then start two stations, quarters and stores. If the UI cannot verify this sequence, defer (05 OQ13).

Price `min(w,8) x min(h,8) x $2,000` [ENGINE basebuyscreen.cpp:31]: Warehouse Eight/Ten $20,000, Lovecraft/Eleven $32,000, One $56,000, Thomas More/Grimoire $80,000, Two $98,000. Soldier wages alone are $4,800 / $6,000 / $7,200 a week for 8 / 10 / 12; staff dilution can require more. A staff-free base fields every soldier up to `B`; adding labs requires the dilution check, not an assumed fixed garrison. Purchase is not blocked by a Hostile Government [ENGINE baseselectscreen.cpp:93-97]. Role split and budget are owned by 01.

---

## 8. Agents: hiring, training, healing, kit, roster

### 8.1 Hiring: the pool, the caps and what it costs

**Rule `[EXE RR-09, RR-10, V-AG-07, V-AG-08]`.** `FUN_0007a474` runs daily in the human city for four classes: agents (cap 100) and biochemists, engineers, physicists (cap 50 each), inside a 180-slot person table. Per class it removes `rand(0..floor(n/3)+1)` listed applicants, then adds `min(rand(0..3), cap - (employed + remaining))` with an origin org uniform in 2..22 and **no relation, hostility or takeover test**; plus `rand(0..1)` Hybrids if Mutant Alliance->X-COM >= 0 and `rand(0..1)` Androids if S.E.L.F. >= 0. Mean 1.5 arrivals per technical class per day; standing pool about 7 per technical class, about 12 listed agents. Fee = one week's wage (`200 x (payByte + 1)`: 600 soldier, 800 technician), no fire penalty, a free Living Quarters bunk needed. Skills never grow after recruitment (60 + rand(0..40)): hire the top of the bar.

**Engine today** (fix RR-09+RR-10+F-M12): 33% of every listed hire leaves daily; a Hostile org (< -50, every taken-over org) produces nothing; the chance is `25 + relation` (`organisation.cpp:541-626`): 1.59 / 2.08 / 2.58 / 3.56 / 4.57 arrivals per class per day at relation -20 / -10 / 0 / +10 / +20, i.e. 1.7x the original early and starving after takeovers. There are **no caps** (`recruitscreen.cpp:604-680`) and no 6-small / 4-large lab cap per base (`base.cpp:268-306`). The harness self-limits to 100 / 50 / 50 / 50 and 180 persons, plans a flat 1.5 arrivals per class per day, and neither front-loads human hires nor counts on Megapol/Government relation for staff.

* **AGT-1 Hire gate.** WHEN the roster is below the 8.6 target and free bunks >= 1 (Living Quarters hold 10; usage counts every agent homed there; the benchmark failed 186 of 207 hires with 27-30 of 30 bunks used) and stores hold an armour set, a loaded primary and >= 2 clips for every unarmed soldier plus the new ones (BASE-3) and funds >= fee + kit + 4 weeks of that wage and appendix 01's runway gate passes and no cap is reached THEN hire; below the 7.6 floors and the minimum-viable column hire even if the runway gate fails. Build a Living Quarters ($5,000, 3 days, $300/week) whenever free bunks < 3. **AGT-1b:** read the candidate rows (humans by accuracy then reactions, androids by strength and HP, technicians skill >= 90); never take row 0 blindly (`oa_play.py:4316`).
* **AGT-6 BUY-AHEAD.** WHEN Megapol or Marsec shows >= 20% (INF-4) or by day 14, whichever first (**engine today:** >= 3% or day 10) THEN buy the vendor-locked reserve for roster + 8 within Stores capacity (10,000 units per Stores facility, $1,000, 2 days): Megapol armour (stock 24 per piece), Lawpistols and clips, Laser Sniper pods, grenades, Stun Grapples; Marsec M4000 clips; Nanotech medi-kits; AIR-10's missiles and Powerfuel. Buying from a manufacturer whose view of X-COM is <= -50 is blocked, so a flipped vendor sells nothing; stock re-rolls weekly and is capped per line, so buy across two weeks; ordered stock is not stock until the cargo lands (`failure_forensics.md` section 5).
* **Payroll `[EXE G9]`.** Salary and upkeep of finished facilities are debited once a week at Monday midnight after funding; the start roster (10 soldiers + 15 technicians) is 18,000 + 4,900 upkeep = 22,900 a week. **Engine today** (fix G4): LIVE starts at 180,000 with 10,000 income, so the first Monday nets about -12,900 instead of +57,100: read the first weekly screen before any roster step beyond the day-0 hires.

### 8.2 Training

**Rule `[EXE V-AG-01 to V-AG-05]`.** Four accrual calls an hour: a person at the home base adds `v = min(10, floor(100F/N))` per call to one accumulator (`F` = finished Training Areas (combat) or Psi-gyms (psi), capacity 10 each; `N` = trainees of that kind). Conversion runs **once per game midnight**: an accumulator >= 160 gives one roll set, then zero. Full rate holds to `N = 10F`; one set a day still holds to `N = 50F` if the agent is home about 20 h. *Combat set:* reactions, speed, strength, health +1 each with probability `(100 - stat)/100`; accuracy raw -1 with `(raw - 4)/100` (cap 96); stamina raw +1 with `(200 - raw)/200`. *Psi set:* probability `3 x initial - current` (%), attack/defence += psi energy/20 (caps 100/99), energy +1; humans with psi energy < 20 gain nothing, so psi suits hybrids; androids never train. Wounded persons heal instead and do not count as trainees. The 10 starters are pre-assigned Combat; hires start unassigned.

Expected stat after N days at one set a day: `100 - (100 - s0) x 0.99^N`: start 15 -> 21 / 26 / 36 / 52 at 7 / 14 / 28 / 56 days; start 35 -> 39 / 44 / 51 / 63. **Engine today** (fix V-AG-01): six sets a game day at <= 100% usage (`agent.cpp:23-26,1105-1127`; start 15 -> 44 / 64 / 84 at 7 / 14 / 28 days), so engine growth is 2-6x too fast: budget at 1/2 to 1/6 of engine logs. The engine also trains injured agents and leaves psi unclamped (a hybrid overshoots to about 122).

* **AGT-2 Assign.** WHEN a hired Human or Hybrid has no training assignment (check at hire, after every transfer, at 00:00) THEN select it in the city-view agent tab and press `BUTTON_AGENT_PHYSICAL` (`cityview.cpp:1597-1625,1650-1705`); hybrids with psi energy >= 30 get `BUTTON_AGENT_PSI` once a Psi-gym exists; androids never. **Do not press it for the 10 starters** (already Combat, `gamestate.cpp:802-803`). UNTIL always.
* **Capacity.** Training Area $8,000, 5 days, $500/week, 10 places: one per 10 trainees (usage counts every assigned agent homed there, away or not). Psi-gym $15,000, $900/week, idle until a hybrid with psi energy >= 30 exists.

### 8.3 Healing and experience

* **Healing `[EXE V-AG-06]`.** A wounded person at the home base with a finished Medical Bay heals `floor(A0/48)` = **20 HP at each midnight**; one bay serves 10 wounded; no bay, no healing; no recent-fight block. Healing and training run only where the facility exists (`Base::getUsage` returns 999 with no capacity, `base.cpp:649-662`): a second base has neither until built. **Engine today:** 0.8 HP/h (within 4%) but blocked after a fight until the daily rollover.
* **AGT-3.** Do not dispatch an agent below 80% of max health when a healthy substitute exists; count only healthy agents toward `f` and the QRF; build a second Medical Bay when wounded > 10.
* **Experience `[EXE V-AG-10 to V-AG-16]`** (living non-android units, after each debrief): accuracy `raw' = ((raw - 4)(100 - floor(hits/4)))/100 + 4`; reactions + `min(counter/reactions, 5)`; health + `min(floor(killScore/15), 2)`; speed + `min(floor(staminaGain/2), 5)`; bravery +1 tens when `killScore >= 6 x bravery`; strength and psi only at gyms. **Engine today:** speed gain is 2x too generous (`battleunit.cpp:4459-4462`).

### 8.4 Kit by week `[ENGINE data economy.xml / agent_equipment.xml; web_agents_training.md sections 7-8]`

**Week 0-1:** Megapol armour (207, weight 55, $1,800, stock 24 per piece); Lawpistol $300 (24 AP/45, dual in real time), M4000 $800 (21 AP/30), Laser Sniper $900 (26/95), Auto Cannon $1,200 (**AP clip**, 40/40); AP grenade $70 (56), Stun $50, Smoke $30; Stun Grapple $400 (90 stun, 3 tiles); medi-kit $250. **Week 2+:** Marsec armour (173, weight 32, $3,700, flight) for snipers/scouts only; Megapol Plasma Gun $1,600 (35/55); Proximity mine $300. **Week 3+:** Power Sword $400 (75 plasma) against Brainsuckers/Hyperworms. **After appendix 02:** Disruptor armour (345, weight 7; $6,600), Toxin B (80% on Anthropod/Skeletoid/Spitter/Popper) and C (100%), Disruptor Gun, Devastator, Personal Disruptor Shield $5,770; Toxin B is the practical win condition `[GUIDE: AllOutWar guide, https://www.ufopaedia.org/index.php/AllOutWar's_X-Com_Apocalypse_Guide]`. A recruit arrives with nothing ($2,500-3,100 per head plus the fee). Over-encumbered when `4 x strength < weight` (a Megapol set takes a weak human from speed 70 to ~55, `agent.cpp:823-839`): heavy kits go to strong agents (androids STR 55-85).

### 8.5 Psi, humans, hybrids, androids `[ENGINE data agent_types.xml]`

**Human** (HP 40-50, accuracy 15-35, reactions 10-40, speed 70-85, bravery 20-70, strength 40-60, PsiDef 15-30): trains, Brainsucker-vulnerable, controlled by a Psimorph (Psi Atk 85) 85% at PsiDef 15. **Hybrid** (Mutant Alliance view >= 0; HP 30-40, accuracy 20-40, reactions 15-45, speed 60-75, strength 30-40, PsiDef 25-40): trains (Psi-gym), Brainsucker-vulnerable, controlled 61% at PsiDef 40 and 26% at 75 after training. **Android** (S.E.L.F. view >= 0; HP 70-90, accuracy 30-50, reactions 15-40, speed 60-80, bravery 60-100, strength 55-85, PsiDef 100): **no training**, **immune to Brainsuckers**, controlled 1-4% (PsiDef clamps to 99).

Alien psi attack is scaled x1.04 and psi defence `(base + 2D) x 1.04` (section 5); X-COM psi grows only at a Psi-gym, and hybrids (Psi Atk 25-40; Mind Control 42-60% against Anthropods/Skeletoids at Psi Atk 40) are the only psi trainees: plan a Psi-gym only with a Mind Bender user. Androids for garrisons, the front of every QRF squad, Brainsucker-heavy crews, Micronoid/Psimorph/Megaspawn sites and the alien dimension; humans as the trained core; hybrids only with a Psi-gym. **AGT-5:** keep >= 4 androids by week 4 and take every android offered while S.E.L.F. is >= 0 (0-1 a day); hybrids <= 15%.

### 8.6 Roster and payroll targets (headcount; money is appendix 01's)

Combat roster target = per base the 7.4 table value of `S` at its actual staff `Q` and station count, plus QRF 1 (8), QRF 2 (8, from week 3) and 2 spares; $600 per soldier a week. **Affordable only against the original's 100,000 balance and +57,100 first Monday (fix G4).**

| Week | Combat roster target | Weekly payroll | Minimum viable (cash-starved) |
|---|---|---|---|
| 0 | 10 (14 once the day-0 hires are kitted); 4 home + 6 QRF | $6,000 ($8,400) | 10 |
| 1 | 16 | $9,600 | 14 |
| 2-3 | 20-22 | $12,000-13,200 | 18 |
| 4-6 | 28-32 | $16,800-19,200 | 22 |
| 7+ | 32-38 | $19,200-22,800 | 26 |

Each added soldier costs about $2,500-3,100 of gear plus the fee; every 10 agents need a Living Quarters. If funding is cut (INF-8) release trainee places, staff and extra labs first, never armed men below the minimum-viable column or any `f`. **Rules:** AGT-1 hire gate; AGT-2 assign training; AGT-3 wounded rest; **AGT-4** at every roster change run the equip pass from a clean city view (modal screens blocked 168 passes) and verify on the equip screen; AGT-5 androids; AGT-6 BUY-AHEAD.

---

## 9. Standing-order index and precedence

Time is **game time**, never wall-clock. Rules are stated once, in the section named: INF-0..6, 8 (3.4); AIR-1..11 (4.4); GND-1..4 (6.2), GND-5 (6.3), GND-6 (6.1); BASE-1..9 (7.7, 7.8); AGT-1..6 (8). **TIME-1** WHEN any alien craft is in the city, a squad is en route or in a building, or INF-3 is active THEN hold game speed <= 3 (so the hover and hourly samples are seen), speed 5 only when none holds (the engine already disables turbo while a hostile aggressive vehicle is in the city, `gamestate.cpp:1174-1211`). **TIME-2** each hour / 00:00 / Sunday 23:00 THEN run INF-6 / the agent sweep (AGT-2, hire pool, equip verify) / close-out (governor 4.8, garrison audit BASE-2, stores, BUY-AHEAD). **SCORE-1** Sunday 23:00 THEN apply the governor of 4.8 (bands, AIR-2(a) exempt, never a blanket hold-fire).

**Precedence when orders compete:** BASE-2/BASE-4 > INF-3 > INF-1 (Tier 0) > BASE-5 > GND-1 > INF-1 (Tier 0b) > AIR > GND-5 > INF-1 (Tier 1-2) > AGT.

---

## 10. Harness requirements

The AI may use only what a human sees (`docs/xcom-ai-plugins.md`; `harness_audit.md` section 5.3); appendix 05 consolidates the plan. **Hidden reads to remove:** HR-1 exact alien crew counts (`crew=N`, `RAID_MAX_ODDS` `oa_play.py:1794-1815`, `oa_victory.py:1214`) -> squad size by UFO type, emptiness by S-MARK/S-DROP age; HR-2 global hostile counts in battle (`foes`) -> visible units; HR-3 exact relation integers and `margin_to_cutoff` (`intercept_min_relation`, `oa_victory.py:828-837`) -> the label and the 3.6 floors (the Score and Monday funding screens are legitimate); HR-4 `centre_on_infiltrated`, `orgs`, `infiltrated` -> the graph.

**Capabilities to add (each a click or read a player has):** HR-5 UFO type, id, speed, position of every visible alien craft, health only as seen; HR-6 equip verification at delivery; HR-7 an equip pass that survives modal screens (168 blocked passes); HR-8 infiltration reader (point 0 and daily history of the 10 Tier 0-1 orgs; the Ufopaedia entry prints the same percent and label, `ufopaediacategoryview.cpp:290-309`); HR-9 building directory of Tier 0/0b buildings; HR-10 S-DROP sensor (craft positions every frame at speed <= 3 mapped to footprints; emits `(building, UFO type, t_arrive)`, `deposit_completed`, `deposit_prevented`); HR-11 game-time scheduling and stall detection; HR-12 vehicle controls (attack mode, altitude, multi-select <= 13, GOTO_BASE; craft HP, magazines, fuel, weapons); HR-13 training and roster controls; HR-14 base layout read and station placement; HR-15 garrison accounting per base (armed-verified agents inside, staff Q, stations, computed `F`; refuse violating dispatches in `select_armed_squad`, `crew_transport`, `send_raid_squad`, `recover_crash_sites`); HR-16 relations reader; HR-17 Cargo/Bio-Transport Module checks, Psiclone delta after the first Cult raid; HR-18 exception safety across stage changes in `recover_crash_sites` and the recovery click that lands on the screen centre (D9, D18); HR-19 the ledgers of section 11; HR-20 a reward that pays for takeover prevention, not for polls (`infil_raids` counts polling, `survival_per_day` rewards throughput).

---

## 11. KPIs and experiments

Every `[DERIVED]` number gets a measurement; the harness writes one JSON line per event (HR-19); every treatment needs a control arm (a one-armed result cannot tell "the change worked" from "it works either way"); score arms by game-day milestones, not wall seconds.

**KPIs.** K-INF1 max Government reading at day 7/14/21/28 and day of any Tier 0-1 flip; K-INF2 S-DROP -> battle latency by tier; K-INF3 empty sweeps, relation lost; K-INF4 drops by tier and type, outcome; K-AIR1 interceptors lost per week; K-AIR2 kills vs escapes, incursion score per day; K-AIR3 Damage-to-City per engagement; K-AIR4 missiles, fuel, stock-outs; K-GND1 per sweep: type, squad, kit, casualties, victory; K-GND2 wrecks recovered / created, crash-to-launch time; K-BASE1 per defence: route, attacker, armed and staff present, fielded `F`, turrets, outcome; K-BASE2 game-hours any base had fewer than `f` fielded; K-AGT1 accuracy/reactions of trained soldiers by week; K-AGT2 hires attempted/succeeded, bunk refusals, unarmed soldiers at 23:59.

**Experiments.** E1 detection latency per drop; E2 engagement margin 2.5x / 1.5x / 4x; E3 altitude Standard / High / Low; E4 attack mode Defensive / Standard / Aggressive; E5 fraction of watched drops whose building is empty 1 h later; E6 Government reading after a known drop vs the 2.3 simulation; E7 AP grenades vs HE inside owned buildings; E8 do station turrets spawn and fire, how many shots; E9 win rate vs squad/crew ratio under TD1-TD3; E10 alive buildings within D <= 60 at the UFO's target-acquisition position; E11 delay from a flip to the next base raid; E12/E12b QRF 2 parked on the Senate, one squad from two craft, Base->Senate air/road time; E13 Government relation before/after a UFO kill and an alien-kill battle (+2 per UFO downed in the original); E14 seeded-building owner over >= 30 starts vs the 1/3, 1/3 prior.

---

## Open measurements

`[MEASURE]` items the benchmark must settle (each names how):

1. **Incursion rate and first UFO time (F6):** count incursions per game week and the first UFO's arrival in a headless run against 3.5 vs 7 a week and 10-12 h vs 5-6 h. Sets INF-0 timing and the 7.1 landing odds.
2. **Crew-evolution model behind the 2.3 hours:** log the org reading after known drops (E6) and refit the species-growth constants; the tables are plus or minus 50% until then.
3. **Relocation after a drop (E5)** and the **hover duration** of the infiltration doodad, and whether a craft can kill it in that window (AIR-7 budget).
4. **n at the UFO's target-acquisition position (E10)** for D <= 60: sets the real per-landing base odds.
5. **Squad and garrison fit under TD1-TD3 (E9):** win rate against crew size, kit and fielded `F`; turret shots per defence (E8); fielded vs present rosters (K-BASE1).
6. **Engagement margin, altitude, attack mode (E2-E4):** the fraction of misses landing on buildings (`q`), ammunition per kill (K-AIR4), whether the UI draws a health bar for hostile craft.
7. **Positive relation channels (E13):** Government change per UFO downed (original +2) and per alien-kill battle; **seed-building owner (E14)**; **delay from a flip to the next base raid (E11)**.
8. **QRF:** Base->Senate flight time for Valkyrie/Phoenix/Wolfhound, parking on a Government roof (E12), squads from two craft (E12b).
9. **Loot spots per Temple raid** (`battlemap.cpp:548-880`) and the real staffed workshop rate (appendix 01).
10. **Dimension fleet stock** before the 6th raid (UFOs seen in the dimension + weekly growth - kills), to decide whether the mass assault is cancelled.
11. **Government survival after T1+T2:** re-run the benchmark; INF-1/INF-3's engine callouts then expire.
12. **Warehouse detection weight** (100 per the facts sheet, 90 in the extracted table) and the other building weights: dump the table at file `0x155354` by function id.

`[UNBOUND]` leads carried here: reinforcement intervals (TD14: UFO2P tactical-start writer), P(base | Raid/Storm) 1/2 vs 1 (remap at VA `0x1017C` vs `FUN_00014854`), 4th-and-later stations (TD11), turret shot counts, base-defence capture (TD20: `FUN_000b44a4`), the `worth` debit and tiered hit (F-M13/F10: who repopulates the mission descriptor at VA `0x244020`), the vehicle-ammo price basis, the other species-growth cases (`FUN_0006f3a8`), the alien tier handicap (TD5: table at VA `0x143A7E`), mass-assault targets (`FUN_0006d994`), the crashed-UFO expiry.
