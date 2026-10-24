# 01 - Opening, economy, staffing, bases and income (Superhuman, engine difficulty 4)

**Summary.** One strategy, written against the original game's rules (UFO2P.EXE / TACP.EXE), for the first 16 weeks of a Superhuman campaign: the day-0 build order, cash flow, hiring, base expansion and the three income pillars the project owner asked for (sell what we make, raid the Cult of Sirius and sell the loot, expand to more bases). Where the rebuilt game differs in a way the harness must handle now, a marked **Engine today:** callout gives the engine rule and the fix key that removes it (`04-engine-fidelity.md`). Rules are `WHEN <thing a player can see> THEN <action> UNTIL <thing a player can see>`.

1. **Solvency first.** The original opens with $100,000 and a stored $80,000/week Senate income, charges nothing at new game and bills $22,900/week of overheads, so the first Monday (Day 7 00:00, 5.5 days after the Tuesday-noon start) nets +$57,100 [EXE v_gov_income/G4, G9]. Funding ends permanently at a Monday if Government->X-COM relation is below -50 or the lifetime score of COMPLETED weeks is below -2,400; the game ends if the balance is below -$5,000 after a Monday [EXE G2, G8]. **Engine today:** LIVE opens at $180,000 with $10,000/week (fix G4) and has no bad-debt defeat (fix G8): read the first weekly funding screen, plan from it, and enforce the -$5,000 floor in the harness.
2. **Day 1:** sell Stormdog, Wolfhound and idle ammo (+$34,000 in the original, +$40,080 in the engine), build 2 Security Stations and 2 Living Quarters, demolish the Psi-Gym, hire 4 soldiers, research Bio-Transport Module and Dimension Gates, hire no technicians. The illustrative budget leaves $65,240 and a $25,400/week burn before the full military and research plan (1.4).
3. **Research is thin in week 1.** Only two topics are startable in the original; the first alien-building topic needs The Alien Dimension after a gate crossing [EXE VC-2/VC-3]. The engine's day-0 +380 filler is engine-only and never counted on.
4. **Pillar - Cult raid loop.** `BUTTON_RAID` (never EXTERMINATE) with a craft carrying a Cargo Module; planning yield $30,000 gross / $27,000 net per raid; the clean window is "Cult infiltration graph at or below 50%" (a taken-over Cult arms guards three tiers higher); in LIVE, where takeovers come early, treat Days 1-10 as the window. Launching costs no lasting relation; only kills do, and nobody but the Aliens has a stake in the Cult.
5. **Cash, weeks 1-4** (section 1.4): at Day 28, $229k funded with no raids, $482k with ten raids; if funding is cut at Monday #3, $69k and $322k. The Quantum lab pair and a second Valkyrie must pass the Runway Gate first.
6. **Bases.** Buy B2 (Warehouse Eight, $20,000) for Large-lab room and as insurance only after cash and garrison gates pass. Default to two Security Stations; field at least 8 soldiers early, 10 from week 3, 12 under major threats (03 §7.6). Personnel budget is `36 - min(12, 4 x stations)`; technicians dilute soldiers. Hostile organisations can raid bases from the first days in the original; continuous historical engine runs miss those raids (BD08).
7. **Staffing.** Hire technicians only into a waiting seat; arrivals are a flat 1.5 per class per day (ten seats at skill 85 take about 17 days). Stay inside the original caps: 100 agents / 50 per technical class / 180 persons, 6 small and 4 large labs-or-workshops per base.
8. **Manufacture-and-sell.** Craft sales never lower a craft price [EXE F-M6]: the Bio-Transport line (10 engineers, 3.84 hulls/week, about $84,000/week gross margin) is the one unbounded line. Everything else is eroded by standing stock: sell each week-0 type at about 0.075 x max a week or in one lump (SELL-1), or everything on arrival when cash is short (SELL-2).
9. **Funding protection** (section 5) is the G2 rule exactly: whatever the Score screen's lifetime total reads at Monday #n becomes the cutoff test value at Monday #(n+1), and nothing scored in between can change it.

**Contents.** 0 Conventions, clock, parameter sheet - 1 Opening and weeks 1-4 - 2 Staffing - 3 Base expansion and defence - 4 Income pillars (4.1 Cult raids, 4.2 manufacture-and-sell, 4.3 selling policy, 4.4 other organisations, 4.5 diplomacy) - 5 Funding protection - 6 Calendar, weeks 1-16 - 7 Standing-order index - 8 Corrections to `docs/campaign-plan.md` - 9 Harness requirements - Open measurements.

Sibling sections: 02 research and technology, `03-military-doctrine.md`, `04-engine-fidelity.md` (fix keys), `05-harness-ai.md`, `06-benchmark.md`. Authority for every rule: `evidence/resolved-facts.md`, then `evidence/adjudications.md`, then `evidence/validation_register.md` and the write-ups `docs/original-game/findings/validation-2026-10/v_*.md`.

## 0. Conventions, clock, parameter sheet

### 0.1 Evidence tags and citations

| Tag | Meaning |
|---|---|
| [EXE] | Bound to the original binary: physical file offset, or validation ID such as `v_takeover/T1`. UFO2P code offset = VA + 0x626A4, data = VA + 0x616A4 |
| [ENGINE] | Historical code or data: `LIVE:path:line` means the October 7–8 research snapshot; `HEAD:path:line` means `fda36d6c`. A bare line is LIVE. The work later entered local commits through `bce89743`; see the README provenance table. These are not current-source line numbers |
| [GUIDE: name, URL] | Community or online source, paraphrased; never more than 15 words quoted |
| [DERIVED] | Arithmetic on tagged inputs; the arithmetic is shown |
| [MEASURE] | Unknown; the harness measures it as described in Open measurements |
| [UNBOUND] | EXE evidence absent; the reverse-engineering lead is named |

`ASSUMPTION(M-n)` marks a planning number with no source; `M-n` is the measurement that replaces it. Research files are cited by name and section (for example `failure_forensics.md` section 3). Superhuman = UI button 5 = engine difficulty `d` = 4; numbers depend on difficulty where stated.

### 0.2 Clock (all dates in this document)

| Term | Definition | Evidence |
|---|---|---|
| Day N | Harness day = floor(ticks / 12,441,600) + 1; the campaign starts Day 1 12:00 (Tuesday); the brief's "Day 0" is Day 1 12:00-24:00 | [ENGINE HEAD:gametime.h:9-15, gametime.cpp:271] |
| Monday #k | 00:00 of Day 7k (Day 7, 14, 21, 28 ...): funding (before the score rollover and before overheads), market stock and price pass, UFO growth, salaries and upkeep, org tech +1 | [EXE v_gov_income/G1, G9; FUN_000941dc file 0xF6880] |
| Game-week k | Week 1 = Day 1 12:00 to Day 7 00:00 (5.5 days); week k >= 2 = Day 7(k-1) to Day 7k. The score of week k is assessed at Monday #k | [EXE G1] |
| Facility ready | Placed on Day d with T build-days: usable at 00:00 of Day d+T (counter drops 1 per midnight); capacity and upkeep count only finished facilities | [EXE G9, RR-07; FUN_00098A70 @0xFB211] |

### 0.3 Parameter sheet (original rules; LIVE differences are called out where they matter)

| Parameter | Value | Evidence |
|---|---|---|
| Opening | Funds $100,000, stored weekly income $80,000; no salary or upkeep charged at new game; first Monday nets +$57,100 | [EXE v_gov_income/G4, FUN_000b94c0 @0x11BC47, 0x11BC5D] |
| Starting roster | 10 soldiers ($600/wk) + 5 physicists + 5 biochemists + 5 engineers ($800/wk) = $18,000/wk; 30 beds (3 Living Quarters), 25 used | [EXE G9: salary = 200 x (payByte + 1)]; [ENGINE data/common_patch/gamestate.xml] |
| Starting upkeep | $4,900/wk: 2 Access Lifts 400, repair bay 500, containment 400, biochem lab 100, 3 Living Quarters 900, medical 800, Psi-Gym 900, Quantum lab 100, 2 Stores 200, training area 500, workshop 100 | [EXE G9 facility_data @0x13D794]; [ENGINE, observed: 13 logged LIVE Mondays net -12,900 = 10,000 - 22,900] |
| **Fixed burn at start** | **$22,900/wk** | [DERIVED 18,000 + 4,900] |
| Hire fee | One week's salary at confirm ($600 soldier / $800 technician), the salary again at the next Monday; fire penalty $0; a free Living Quarters bed is required. Whether pay grows with experience is unbound (agent byte +0x57) | [EXE V-AG-07, G14] |
| Facility cost / build days / upkeep per week | Living Quarters $5,000 / 3 d / $300 (10 beds); Stores $1,000 / 2 d / $100; Security Station $2,000 / 2 d / $200; Workshop $6,000 / 6 d / $100 (5 engineers); Quantum or Biochemistry Lab $10,000 / 5 d / $100 (5); Medical Bay $12,000 / 4 d / $800 (10 patients); Training Area $8,000 / 5 d / $500 (10); Psi-Gym $15,000 / 6 d / $900; Alien Containment $7,000 / 4 d / $400 (20); Repair Bay (2x2) $9,000 / 5 d / $500; Advanced Workshop (2x2) $12,000 / 9 d / $200 (10); Advanced Quantum Lab (2x2) $25,000 / 8 d / $200 (10); Advanced Biochemistry Lab (2x2) $20,000 / 8 d / $200 (10); Advanced Security Station $8,000 | [EXE facility_data @0x13D794, byte-verified; RR-03, RR-07] |
| Placement and cancel | Placement debits the full cost; cancelling a facility under construction refunds `cost x remaining / buildTime`; a finished facility refunds 0 | [EXE G9: refund add at 0xCC6E8] |
| Prices | One price per market row for buy and sell, credited at once; selling to a hostile manufacturer is allowed, BUYING from a manufacturer whose view of X-COM is <= -50 is blocked; unresearched alien artifacts price 0 | [EXE F-M5] |
| Craft sale value | `(type price - default equipment) x health / constitution + installed equipment`; no ammo term | [EXE F-M7, FUN_0008da9c @0xF0140] |
| Hire and lab caps | Employed + listed applicants per class: agents 100, biochemists 50, engineers 50, physicists 50, inside a 180-person table. A base refuses a 7th small lab/workshop or a 5th large one (counts independent, per building, including under construction) | [EXE RR-09, F-M12 (FUN_00069270, string 0x152252)] |
| Hostile / allied | AI predicates: hostile <= -50, allied >= 75. The weekly funding cutoff is the strict relation < -50 | [EXE BD09, G13] |
| Funding tiers | 1.5; strict, most extreme wins, result STORED as the next weekly income F | [EXE G1 @0xF6EC7-0xF6FA4] |
| Cutoff and defeat | Section 5.1 (G2); defeat when balance < -5,000 after a Monday's overheads | [EXE G2, G8] |

### 0.4 Design principles

| # | Principle | Consequence |
|---|---|---|
| P1 | Plan from the original's numbers and read the screen | Every purchase passes RG (1.4). Before the first funding report use `S = 0` for spending permission; the start balance is a provenance clue, not proof of recurring income |
| P2 | No decision may rest on engine-only behaviour | Frozen Psiclone price, free weekly trickles, a day-0 alien-building topic, ammo in craft sale value, no hire or lab caps are never relied on |
| P3 | Only what a human sees; measure before scale | Relation bands, the infiltration graph, funds, the Weekly Funding and Score screens, Buy/Sell stock and price, recruit skills, the base grid; each growth step has an `M-n` threshold |
| P4 | Never leave a base without armed people present | Section 3 |

## 1. Opening and weeks 1-4

### 1.1 What we start with

| Asset | Quantity | Value when sold (original) | Decision |
|---|---|---|---|
| Cash | $100,000 [EXE G4] | - | **Historical LIVE:** about $180,000 (G4). Record this fingerprint, but use `S = 0` in RG until the first funding report |
| Stormdog (road, Airguard) | 1 | About $6,000: `(type price - default equipment) x health/constitution + installed equipment`, no ammo term [EXE F-M7] | Sell Day 1. **Engine today:** pays $10,830 (adds 600 Airguard rounds x $8 and fuel; fix F-M7) |
| Wolfhound APC (GLM, Cargo Module) | 1 | About $9,000 [DERIVED same rule] | Sell Day 1. Engine pays $10,250 (12 GLM x $100, 5 fuel x $10). Together the engine's $21,080 is the benchmark's observed first sale |
| Airguard 500 / GLM 50 / Elerium-115 500 in stores | - | $4,000 / $5,000 / $10,000 at the shown price | Sell Day 1 (their users are sold) |
| Janitor missiles 150; Fusion Powerfuel 1,000; spare Bolter x2, Lancer x2; grenades 20 AP / 20 stun / 20 smoke | - | $16,500; $10,000; $5,000; $3,000 | Keep (Valkyrie and both Phoenix carry Janitor arrays; engines burn fuel; air plan; loadouts) |
| Valkyrie (Lancer, Janitor array, Passenger + Cargo Module) | 1 | About $75,000+ | Keep: the only cargo-carrying raid transport at start. Read passengers from `gs interceptors` (corpus says 8 and 8+4) |
| Phoenix hovercar (4 passengers, one free general slot) | 2 | $12,000 each | Keep; fit a $300 Cargo Module to each in week 1 (loot lands on any player craft at the building that has cargo capacity) |

[ENGINE data/common_patch/gamestate.xml `initial_vehicles`, `initial_vehicle_ammo`; economy.xml prices; HEAD:battle.cpp:3259-3270 (cargo carrier); `income_cult_raids.md` section 3.6]. Valuation rows marked "about" are [DERIVED] from the EXE formula with full health and default equipment installed.

### 1.2 Day 1 (12:00-24:00), step by step

| Game time | Step | Cost / gain | Why, and what NOT to do |
|---|---|---|---|
| 12:00 | Read `funds`, `time`, `bases`, `agents`, `interceptors` (Valkyrie passengers and cargo), `facilities` | 0 | Confirms the start state; passengers decide the raid squad size |
| 12:02 | BioChem: Bio-Transport Module (9,000 mh, +80), all 5 staff. Physics: Dimension Gates (5,000 mh, +50), all 5 staff | 0 | At about 430 mh/h: module done Day 2 about 09:00, Gates Day 1 about 23:40. Then wait for legitimate bodies or won manned recovery topics [RR-12]. Never use the engine's day-zero alien-building filler: the original needs a crossing, The Alien Dimension, then the first building topic [VC-2/VC-3] |
| 12:05 | Training: the 10 starters are pre-assigned Combat training; do NOT press `BUTTON_AGENT_PHYSICAL` on them (it toggles training off); hires start unassigned | 0 | [EXE V-AG-05 adjudicated: FUN_000799dc stores]. One roll set per agent per game day, wounded agents do not train [EXE V-AG-01, V-AG-05]. **Engine today:** six roll sets per day (fix V-AG-01): stat growth in engine logs is 2-6x too fast, budget at 1/2-1/6. Training policy belongs to section 03 |
| 12:10 | Base screen, in order: (1) demolish the Psi-Gym; (2) place 2 Security Stations on corridor tiles next to the Access Lift; (3) place 2 Living Quarters on free tiles (the freed Psi-Gym tile first) | -$4,000, -$10,000; upkeep -900 +400 +600 | Psi-Gym: $900/week, refund 0 once built; humans start with psi energy 5-20 and gain 0 psi attack/defence per success until energy reaches 20, hybrids reach about 80 in 28 days [EXE V-AG-04]: skip the demolition only if section 03 commits to a hybrid psi cadre. Security Station: 4 turrets each in a base defence (3.5). Living Quarters: the 6th hire needs one; only finished quarters count (ready Day 4 00:00). No second base today |
| 12:25 | Buy/Sell: sell Stormdog, Wolfhound, Airguard x500, GLM x50, Elerium-115 x500 | +$34,000 | One pass; prices are flat within a week. Engine pays +$40,080 |
| 12:35 | Recruit: hire 4 soldiers (29 of 30 beds); no scientists or engineers | -$2,400 | A technician without a lab seat produces nothing; keep one bed spare |
| 12:45 | Arm the 4 recruits (they arrive with no equipment): Megapol 5-piece armour ($1,800) plus weapon and clips (about $1,140 on average); fit a $300 Cargo Module to each Phoenix | -$11,760, -$600 | Order early: an undelivered purchase is refunded after 6 game hours [ENGINE HEAD:vehicle.h:24, organisation.cpp:616-619] |
| 12:55 | Air purchases owned by section 03 (placeholder $40,000: 8 Hoverbikes plus Janitor arrays; section 03's own figure $49,600 lowers every later balance by $9,600 and flips no decision in 1.4) | -$40,000 | ASSUMPTION(M-9) |
| 13:00 | Save. Ledger line: balance **$65,240**, burn **$25,400/week** (22,900 + 400 + 600 - 900 + 2,400) | | [DERIVED, 1.4] |
| by about 13:50 | First alert (-30): the incident sweep covers the 109 buildings of the Superhuman city in about 109 minutes and detection weight is x100 until 16:00 on Day 1; section 03 owns the response, nothing here waits for it | | [EXE v_score/SCORE-C2, v_base_defence/BD07] |
| 18:00 | Verify delivery of the recruits' kit; re-order if missing (Dimension Gates finish about 23:40: Physics idles) | | |
| Day 2 about 09:00 | Bio-Transport Module done (+80). Engineering lab: manufacture 3 Bio-Transport Modules ($700 each, 4,000 mh, about 9.3 h each; one per capture craft plus a spare) | -$2,100 | Do not sell them (margin $250 per 4,000 mh). BioChem lab: the next startable topic by score density (5.3); none exists until the first alien body arrives |

Not done on Day 1, on purpose: a second base (the benchmark bought one on Day 1 in 98 of 98 logs, left it empty and lost it silently in at least 13 runs, `failure_forensics.md` section 9); extra labs or workshops (no startable topic keeps them busy); extra scientists or engineers; Psi training. Section 02's Quantum Lab pair (order F-1) is NOT ordered on Day 1: in the original the Physics lab has nothing to do until a manned UFO is recovered and week-1 packages are Probes and Scouts [EXE F1, RR-12]; its trigger is ST-0 (2.2).

### 1.3 Days 2-6 and the weekly rhythm

| When | Trigger (observable) | Action |
|---|---|---|
| Day 2 onward | Gate G-C (4.1.3) holds: >= 8 fit armed raiders beyond the home garrison, a carrier with cargo and passengers >= raiders | Launch Cult raid #1 as an instrumented probe; log every ledger column |
| by Day 7 | A free tile exists and another station preserves the fielded target after staff dilution | Consider a third station; two remain the default (03 §7.6) |
| Day 4 00:00 | Living Quarters x2 finished (50 beds) AND (cumulative Cult loot >= $30,000 AND RG(6) passes, or ST-2's no-raid branch) | Hire 6 soldiers (fee $3,600 + kit $17,640) |
| any time | A lab sits idle > 2 game hours with a startable topic | Assign by score density (5.3) |
| Day 6 12:00 and every Sunday 12:00 | Balance < next payroll + $10,000 | Liquidate surplus loot and ammo; if still short sell one Phoenix (keep the Valkyrie) |
| Day 7 00:00 (Monday #1) | Weekly Funding screen opens | Read income (expect $80,000) and the week's score `W` and tier; closing the screen is a pure UI action in LIVE (HEAD resets the week in the UI, fix G2/D7) |
| Day 7 00:01-06:00 | After the funding screens | Routine hire window: the fee is paid now and the next salary 7 days later |

### 1.4 Cash flow, weeks 1-4 [DERIVED]

**Sensitivity model, not an executable schedule.** The following historical arithmetic assumes income before its first confirmation, 6/4 garrisons, a $40,000 air allowance and early raids. It does not fund all of 02's facilities or 03's military plan. In the actual policy, 14 soldiers leave only 6 beyond an 8-soldier garrison, so the eight-person Cult raid waits for more hires. Recompute cash, beds, kits and payroll from the fielded targets of 03 §7.6 before authorizing any purchase; do not use these balances to claim that B2 or a calendar deadline is affordable.

**Inputs.** Opening $100,000; Day-1 sales +$34,000 (15,000 craft + 19,000 ammo); Day-1 capex: Security Stations 4,000, Living Quarters 10,000, Cargo Modules 600, 4 hires 2,400, 4 kits 11,760, section 03 air envelope 40,000 = 68,760; balance **$65,240**, burn **$25,400/week**. Cult raid: gross $30,000, attrition and clips $3,000, net $27,000 [ASSUMPTION(M-1, M-3), see 4.1.1]; ten raids on Days 2, 3, 5, 8, 9, 11, 12, 15, 18, 24. Senate credit $80,000 at each Monday while funded (tier effects ignored, 1.5). Day-4 hire of 6 soldiers (-$3,600 fee, -$17,640 kit, burn +$3,600) only if loot >= $30,000 and RG passes. **B2** (BS-2, 3.3): -$33,000 (Warehouse Eight 20,000, 2 Living Quarters 10,000, Stores 1,000, Security Station 2,000), burn +$1,100, plus hiring any shortfall to **18 soldiers** (8 raiders plus garrison floors 6 at B1 and 4 at B2; each hire -$3,540, burn +$600), tried on Day 10 and then at each Monday until it passes.

**Runway Gate RG(k)** - applied to every discretionary purchase above $5,000. Allowed iff all hold:
(a) `balance_after >= burn_after + 10,000`;
(b) `balance_after - (burn_after + 10,000) >= k x max(0, burn_after - S)`, where `S` is the last Senate credit actually received, zero before Monday #1 or after a cut; `k` = 6 by default, 3 for the **income-path list** (Advanced Quantum Lab, Advanced Workshop and the technicians who staff them);
(c) for a single purchase above $30,000 also `balance_after >= 4 x burn_after`: four Mondays of overheads, the arithmetic of the bankruptcy floor (G8, `resolved-facts.md` section 3.5; section 5.5), because a funding cut is announced only on the Monday it happens.
Loot is deliberately excluded from `S` (non-recurring). `k` is a policy constant, not a measurement.

**Balance after each Monday's credit and overheads (original rules):**

| Scenario | Mon #1 (D7) | Mon #2 (D14) | Mon #3 (D21) | Mon #4 (D28) | Burn at D28 | Soldiers at D28 | +6 hired | B2 bought |
|---|---|---|---|---|---|---|---|---|
| Funded, no raids | 119,840 | 127,280 | 178,380 | 229,480 | 28,900 | 18 | no | Day 14 |
| Funded, 10 raids | 176,000 | 300,900 | 404,800 | 481,700 | 30,100 | 20 | Day 4 | Day 10 |
| Funding cut at Monday #3, no raids | 119,840 | 127,280 | 98,380 | 69,480 | 28,900 | 18 | no | Day 14 |
| Funding cut at Monday #3, 10 raids | 176,000 | 300,900 | 324,800 | 321,700 | 30,100 | 20 | Day 4 | Day 10 |

**Engine today (G4).** LIVE opens $80,000 higher and $6,080 richer from the sale (engine ammo term) but credits only $10,000 per Monday, so its balance, before the RG verdicts below change what is bought, is the table value plus $86,080 minus $70,000 per Monday elapsed: ahead by $16,080 at Monday #1, behind by $53,920 at #2, $123,920 at #3 and $193,920 at #4, and RG verdicts flip: without Cult income B2 never passes (`S` = 10,000) and LIVE holds $89,720 at Day 28 with a $25,400 burn (it loses about $15,400/week, the benchmark's rank-4 cash starvation, `failure_forensics.md` sections 2 and 10); with ten raids LIVE holds $287,780.

Reading it [DERIVED]:
* **Funded, the Senate alone covers the burn ladder** (22,900 -> 25,400 -> 29,000 with six more soldiers -> 30,100 with B2 -> 47,100 with Advanced Quantum + Advanced Biochemistry labs, 20 technicians and 2 Living Quarters -> 55,600 with an Advanced Workshop, 10 engineers and a Living Quarters -> 64,100 with a second Advanced Workshop). The pillars pay for capex and are the insurance for a cut, when self-income must cover the ladder.
* **A cut with no income is a countdown.** The game ends at the first Monday that leaves the balance below -$5,000: survivable Mondays after the cut Monday = `floor((balance_after_cut_Monday + 5,000) / burn)`. The no-raid cut row (98,380, burn 28,900) loses at Monday #7 (Day 49); the ten-raid row holds 11 weeks of burn.
* Not in the table, each through RG: Advanced Quantum Lab $25,000 + Advanced Biochemistry Lab $20,000 + 20 technicians' fees $16,000 ($61,000, burn +$17,000/week); section 02's Quantum Lab pair (-$20,000, +$200/week; its 10 physicists -$8,000 and +$8,000/week); section 03's second Valkyrie in week 2 (about $75,300): it passes only in the ten-raid rows (balance after $225,600 at Monday #2), rule (c) blocks it in the no-raid rows, and without (c) the cut/no-raid case would end at -$9,320 at Monday #4, game over under G8; garrison above plan ($4,040 capex and $630/week per soldier); the 3 Bio-Transport Modules ($2,100); section 03's $49,600 air buy instead of $40,000 (all balances -$9,600).

### 1.5 What a score tier is worth

Tier change to the stored income at F = $80,000, by the assessed week's score `W` (strict, most extreme wins): > 12,800 +F/4 (+20,000); > 6,400 +F/5 (+16,000); > 3,200 +F/8 (+10,000); > 1,600 +F/12 (+6,667); > 800 +F/16 (+5,000); > 400 +F/20 (+4,000); 0..400 none; -1..-400 -F/15 (-5,333); -401..-800 -F/10 (-8,000); -801..-1,600 -F/5 (-16,000); below -1,600 -F/4 (-20,000) [EXE G1 @0xF6EC7-0xF6FA4]. The result is STORED as the next weekly income, so tiers compound: four consecutive -20% weeks take $80,000 to $32,768. The benchmark's measured drift of about -910 a week is the -F/5 tier every week (`web_economy_politics.md` section 1, `failure_forensics.md` section 4). **Engine today:** F is $10,000 in LIVE, so every tier is one eighth of this and score matters there mainly for the -2,400 cutoff.

## 2. Staffing

### 2.1 Salary versus output

| Role | Weekly wage | Hire fee | Seat capex | Loaded $/week | Output |
|---|---|---|---|---|---|
| Soldier | $600 | $600 | kit about $2,940 (armour $1,800 + weapon and clips about $1,140) + bed $500 | about $630 | raids, garrison, base defence |
| Physicist / Biochemist | $800 | $800 | small lab $2,000 per seat, Advanced $2,000-2,500; bed $500 | about $850 | `skill` man-hours per game hour |
| Engineer | $800 | $800 | Workshop $1,200 per seat, Advanced $1,200; bed $500 | about $850 | same, engineering skill |

[EXE G9 wages; RR-05: skill is set once at recruit creation as `60 + rand(0..40)` and never changes; RR-01: hours to finish = `skillHours / (staff x skill)`, no difficulty term; RR-03: 5 seats per small lab, 10 per large]. Pay growth with experience is unbound (G14).

Break-evens [DERIVED]:
* One staff-week at skill 80 = 80 x 168 = **13,440 man-hours**. Engineer break-even margin = $850 / 13,440 = **$0.063 per man-hour**; a product qualifies for engineers only if its gross margin per man-hour is at least **$0.15** (2.4x cover). Qualifying: Disruptor Gun $0.214, Boomeroid $0.203, Devastator $0.275, Personal Disruptor Shield $0.269, Toxigun $0.527, Stasis Bomb $0.725, Bio-Transport craft $0.629. Not qualifying: Bio-Transport Module $0.062 (`income_manufacturing.md` section 3).
* Scientists are worth **score and time**, not cash: a never-idle 5-staff BioChem lab does 67,200 man-hours/week, which at topic score densities of 0.010-0.025 per man-hour is +670 to +1,680 score/week (autopsies 0.019-0.025, species 0.013-0.017). Physics topics are 3-5x less dense (alien systems 0.006, Dimension Probe 0.007, Advanced labs 0.005); Physics labs exist for the unlock chain, not for score (`tech_tree.md` section 11).
* Bio-Transport, one engineer-week = 13,440 x $0.629 = **$8,450 gross margin** against $850 (9.9x). Craft sales never lower a craft price in the original [EXE F-M6], so staff the line to its lab limit (ST-5).

### 2.2 The hire pool and the hiring triggers

**Original rule** [EXE RR-10, V-AG-07, FUN_0007a474, once per game day in the human city and at new game]. For each of the four classes (agents, biochemists, engineers, physicists): remove `rand[0, floor(n/3)+1]` of the `n` listed applicants (first in slot order); then add `min(rand[0,3], cap - (employed + listed))` new applicants, each with an origin organisation uniform over organisations 2-22 and NO relation, hostility or takeover test; plus up to `rand[0,1]` Hybrids if Mutant Alliance->X-COM >= 0 and `rand[0,1]` Androids if S.E.L.F. >= 0 (charged to class 0). Mean **1.5 arrivals per technical class per day**; standing pool about 6-7 per technical class and about 12 listed agents. Skill is uniform 60-100, so the share at or above 85 / 80 / 75 / 70 is 39% / 51% / 63% / 76%.

| Floor | Qualifying arrivals/day | Expected days to see 10 seats | 5 seats |
|---|---|---|---|
| >= 85 | 0.59 | 17.1 | 8.5 |
| >= 80 | 0.77 | 13.0 | 6.5 |
| >= 75 | 0.95 | 10.5 | 5.3 |
| >= 70 | 1.13 | 8.8 | 4.4 |

[DERIVED: days = seats / (1.5 x share)]; about 2 qualifying applicants (at 85) are already listed on Day 1, so subtract about 2 seats. **Engine today (RR-10, fix RR-10):** 33% of every hirable agent leaves daily and each organisation rolls with chance `25 + relation` (nothing at all while hostile or taken over): 1.59 / 2.08 / 2.58 / 3.56 / 4.57 arrivals per class per day at relation -20 / -10 / 0 / +10 / +20 [ENGINE LIVE:organisation.cpp:541-626], i.e. 1.7x the original early and starving after takeovers; there are no caps. The harness plans the flat 1.5 and, in LIVE only, lengthens every hire-ahead window by half once six or more "alien takeover" notices have been seen (ST-6).

| ID | WHEN (observable) | THEN | UNTIL |
|---|---|---|---|
| ST-0 | A manned UFO is sighted or Day 7 arrives; RG(6) passes for $20,000 + $200/week; two free tiles exist; adding both labs leaves at most 6 small labs/workshops | Place the two Quantum Labs (5 build-days). Hire their 10 physicists after completion only when RG(6) passes for $8,000 + $8,000/week and a non-Gates topic is startable or a manned recovery is under way; otherwise re-test each midnight | Seats = 10 |
| ST-1 | Day 1 | Hire 4 soldiers (<= 5 free beds, one bed spare); zero scientists or engineers | Day 4 (Living Quarters finished) |
| ST-2 | Day >= 4, both Living Quarters finished, and either (cumulative Cult loot >= $30,000 and RG(6) passes) or (no raid yet, spare soldiers < 8, RG(3) passes: raiders are the income path) | Hire toward the computed shortfall, up to 6 in this batch ($3,540 fee + kit each); if RG fails, wait for confirmed funding | Raiders (8-10) plus the soldiers needed to meet 03 §7.6 fielded targets at every base |
| ST-3 | >= 2 topics of one research type are startable AND every lab of that type is busy AND RG(6) passes AND a free 1x1 exists AND the base holds fewer than 6 small labs/workshops | Build lab #2 of that type ($10,000, ready in 5 days) and hire 5 of that role | Small-lab total at 6 per base |
| ST-4 | The research that unlocks an Advanced lab has <= 8 days of work left at its lab's rate, or has just completed | Place the 2x2 the hour the research completes (RG(3) for Advanced Quantum Lab and Advanced Workshop, RG(6) for Advanced Biochemistry Lab). Open the hire-ahead window: take every candidate with role skill >= 85 for 3 days, >= 80 for days 4-6, >= 75 for days 7-9, >= 70 afterwards; stop at 10 seats minus staff moved over from idle labs | Seats = 10 at completion (build 8-9 days) |
| ST-5 | An Advanced Workshop is finished AND a product passes the product test (4.2.3) | Staff **10 engineers** for a craft line (craft sales never cut the price, F-M6). For non-craft products staff 5 and let the SELL-1 pace (4.3) cap volume. Add a second Advanced Workshop (with its own 10 engineers) only after the first line has sold at least 2 hulls at the full price and the cash reserve rule holds | 10 engineers per workshop |
| ST-6 | **Engine today only:** >= 6 "alien takeover" notices have been seen | Hire-ahead windows x 1.5 and skill floors -5 (supplier organisations fall and a hostile or taken-over organisation supplies nobody in LIVE) | Takeovers stop or seats are full |
| ST-7 | A staffed lab loses a member (death, transfer, injury) | Replace within one pool refresh at the current floor. An injured scientist adds nothing in the original [EXE RR-02] (LIVE still counts them) | Seat full |
| ST-8 | A candidate with skill >= floor + 5 exists AND a member has skill <= floor - 15 AND the lab is on a running topic's critical path | Hire the candidate, fire the weak member after the topic completes (fee only, penalty $0) | - |

### 2.3 Picking the candidate

Read each candidate's role skill on the Recruit screen and take the **highest** (the benchmark takes row 0 every time, `harness_audit.md` section 4.2). Floors: Advanced-lab seat 85 (relaxing per ST-4), small-lab and Workshop seats 80; soldiers are section 03's pick. The oldest listed applicants leave first, so take the best candidate the morning they appear; hiring right after the Monday 00:00 payroll is cheapest. **Fire-Sunday / rehire-Monday is not free**: the fee is a week's salary, so it is break-even at best and discards the person's training (`docs/campaign-plan.md` says otherwise).

### 2.4 Beds, caps, and who may not be fielded

| Item | Rule | Evidence |
|---|---|---|
| Beds | Every agent homed in a base uses one bed, soldiers included; a hire needs a free quarters slot ("Accomodation exceeded" otherwise); only finished Living Quarters count | [EXE V-AG-07]; [ENGINE HEAD:recruitscreen.cpp:104-120, base.cpp:568-575] |
| Quarters rule | Build Living Quarters when `free_beds < 3 + hires planned within 4 days`, >= 3 midnights before the first hire that needs it; `LQ = ceil((agents + 5) / 10)`. Planned headcount: Day 1 29 -> Day 4 35 -> Advanced pair staffed 55 -> Advanced Workshop with 10 engineers 65 -> with extra garrisons 70-80 | [DERIVED] |
| Personnel caps | Employed + listed applicants: agents 100 (hybrids and androids included), biochemists 50, engineers 50, physicists 50, all inside a 180-person table, so the binding limit is 180. Hybrids and androids arrive only while Mutant Alliance / S.E.L.F. relation >= 0. "20 agents without technical staff" and auto-sack by ratio were not found in the function | [EXE RR-09, RR-10; FUN_0007a474 caps at file 0xDA9A4] |
| Lab and workshop caps | Per base: at most **6 small** (Biochemistry Lab, Quantum Lab, Workshop together) and **4 large** (Advanced Biochemistry, Advanced Quantum, Advanced Workshop together); counts are independent, per building, and include facilities under construction; Alien Containment is uncapped; the build palette refuses with the "Regulation 44" message, and its funds test is strict (balance must exceed cost). The start base already holds 3 small | [EXE F-M12 adjudicated: FUN_00069270, string at file 0x152252] |
| **Engine today** | No personnel or lab caps (about 35 small or 9 large labs fit per base; fix RR-09 and F-M12): the harness self-limits to the table above | [ENGINE LIVE:recruitscreen.cpp:604-680, HEAD:base.cpp:268-306] |
| Never field technicians | In the original every employed agent LOCATED at a base defends it, armed or not (3.5); an incident dispatch takes whoever stands in the building. Keep technicians out of dispatch pools; observed lab staffing 5/5/5 fell to 2/3/0 after incidents (section 03 owns the mechanism) | [EXE BD04]; [ENGINE, observed: `docs/playing-the-game.md` lines 160-163] |

## 3. Base expansion and defence

### 3.1 Facts that drive the base policy

| # | Fact | Evidence |
|---|---|---|
| F1 | A bought base is an empty shell (Access Lift $200/week only: no beds, Stores, Medical Bay, labs or staff). Price = `min(w,8) x min(h,8) x $2,000` of the building footprint; any Government-owned building with a base layout may be bought, also while the Government is hostile or after a funding cut. Only 8 sites have layouts at Superhuman and the start base is one of them, so 7 are purchasable. EXE price and refusal rule: [UNBOUND] (refusal strings exist; lead: the purchase handler near FUN_00015400) | [ENGINE HEAD:base.cpp:62-85, game/ui/city/basebuyscreen.cpp:26-34, gamestate.cpp:724-748] |
| F2 | Capacities (beds, Stores, labs, containment, medical) are per base; hires go to the current base; lab progress counts only agents homed in the lab's base and present; a specimen a topic needs must be in the lab's own base inventory. Battle loot lands in the home base of the carrying craft; selling works from any base. Transfers are slow (taxi, Transtellar ferry, refused if Transtellar is hostile): prefer local hires and local sale | [ENGINE HEAD:base.cpp:502-627, research.cpp:107-158,412-421, battle.cpp:3397-3544, agent.cpp:411-423] |
| F3 | **Presence.** In the original a base is destroyed when an attack event fires with no live agent present; any employed agent located there, or a class-0 agent aboard a craft docked there, prevents it [EXE BD17]. **Engine today:** the destroy test is `building->currentAgents.empty()` (LIVE:vehiclemission.cpp:2644, organisation.cpp:1298, :1322) and an agent aboard a parked craft is not in that set, so soldiers sitting in a parked craft do NOT protect the base (`04-engine-fidelity.md` keeps BD17 as a match; this aboard-craft difference is not indexed). The harness disembarks: drag each agent from the craft list to the building list in the agent-assignment panel (HEAD:agentassignment.cpp:375-392), or hire locally | [EXE BD17]; [ENGINE as cited] |
| F4 | **What attacks a base** [EXE BD01a, BD02, BD15]: an Infiltration UFO landing over an active base attacks at once, with `P(base) = 1 / (alive buildings within D <= 60)` per landing per base; a Subversion UFO attacks only if the base's exposure word is non-zero, else it sets it; a hostile organisation's Raid or Storm against a random live X-COM building (3.6). Alien movement into a base and alien detection of one never start a defence. More bases change neither funding nor score and each adds one candidate; `GameLost` needs ALL bases gone | [ENGINE LIVE:building.cpp:835-844,1029-1040; HEAD:base.cpp:150-159] |
| F5 | Manufactured non-craft items land in the lab's base; if Stores would overflow, the batch is lost and its man-hours reset (engine rule, EXE unbound); craft need no Stores | [ENGINE HEAD:research.cpp:526-577] |

**Historical engine (BD01a/BD01b):** HEAD starts defences from alien movement/detection; LIVE removed those triggers. The change later landed in local `879d0d73` (04 A0). Verify ancestry and the regression test on the selected implementation base; do not re-commit a supposed uncommitted hunk. Historical binaries remain unattributed. Overspawn-triggered defences remain a separate BD02 issue.

### 3.2 The eight sites

| Site | Price | Cells | Free 2x2 (empty) | Type | Nearest temple | Fit |
|---|---|---|---|---|---|---|
| Warehouse Eight | **$20,000** | 29 | 4 | warehouse (sturdy) | Sanity 54 | **Research + factory base**: AQPL + 2 Advanced Workshops + Advanced Biochemistry = 16 tiles, + 3 Living Quarters, 2 Security Stations, 2 Stores, lift = 24 of 29 |
| Warehouse Ten | **$20,000** | 24 | 5 | warehouse | Doom 18 | Factory #2 / strike base; a single lift neighbour: one Security Station seals the only exit |
| Lovecraft Block | $32,000 | 32 | 7 | slums (weak) | Sanity 24 | Capacity, weak walls; only if both warehouses are excluded |
| Warehouse Eleven | $32,000 | 22 | 4 | warehouse | Doom 21 | 4 tiles from Ten: never both |
| Warehouse One | $56,000 | 32 | 7 | warehouse | Apocalypse 18 | B4 candidate |
| Thomas More Tower / Grimoire Block | $80,000 | 32 / 28 | 6 / 5 | slums (collapses easily / road-loop bug) | Humility 26 / Sanity 22 | Avoid |
| Warehouse Two | $98,000 | 22 | 3 | warehouse | Apocalypse 18 | Overpriced |

[ENGINE `income_base_expansion.md` section 3.1; `web_bases_facilities.md` section 6.1; Manhattan distances DERIVED from the building bounds there; [GUIDE: UFOpaedia Base Locations, https://ufopaedia.org/Base_Locations_(Apocalypse); UFOpaedia Base Layouts, https://ufopaedia.org/Base_Layouts_(Apocalypse)] agree that warehouses are durable and slums are not]. Distances: Eight-Ten 74, Eight-Lovecraft 32. Keep bases >= 30 apart where possible: alien spread reaches the 15 nearest intact buildings in a 15-tile box, and 3+ aliens entering a base building in one hourly move set its exposure [EXE BD06].

```
site chooser (executable)
candidates = Government-owned buildings with a base layout (centre_on_basesite lists them with price)
drop any candidate within Manhattan 30 of an owned base, and Warehouse Eleven if Ten is owned (and vice versa)
B2: prefer Warehouse Eight; else Ten; else Lovecraft; else Warehouse One
B3: prefer Warehouse Ten (if B1/B2 are not within 30); else Lovecraft; else Warehouse One
if B1 = Eight: B2 = Ten.   if B1 in {Ten, Eleven}: B2 = Eight.
slums only when no warehouse remains, and then only with RG(6) after adding 2 extra Security Stations
verify after purchase: funds delta == listed price, and the new base appears in `bases`
```

### 3.3 Triggers (WHEN / THEN / UNTIL)

| ID | WHEN | THEN | UNTIL / note |
|---|---|---|---|
| BS-1 | A Large-lab-unlocking topic (Advanced Quantum Lab, Advanced Workshop, Advanced Biochemistry Lab) has <= 48 game hours of work left at its lab's rate, or just completed, AND no owned base has a free 2x2 (base screen grid), AND the purchase passes RG(3) (income path) or RG(6) | Buy **B2** and run BS-P so the Advanced lab is placed the hour its research completes | Same hour. Each day of delay is a day on the Bio-Transport line (section 02 F-7). The start base has a free 2x2 in only about 21% of draws (0.2%-46%) [DERIVED `income_base_expansion.md` section 4.3; ENGINE HEAD:base.cpp:268-306] |
| BS-2 | Day >= 10, RG(6) passes for purchase + kit + staff, B2 not owned, and fit-armed soldiers cover 8 raiders plus each base's computed garrison | Stage the garrison, then buy B2 as insurance and beds via BS-P | Reconsider at each Monday; Day 28 never overrides cash or garrison gates |
| BS-3 | A base-defence battle or incident alert names a base AND B2 not owned | Buy B2 within 24 hours if RG(6) passes | 8 of 9 benchmark game-overs were lost base defences (`failure_forensics.md` section 2) |
| BS-4 | Advanced Workshop #1 staffed, no usable space remains, every garrison present for 7 days, RG passes, and soldiers cover 8 raiders plus three computed garrisons | Stage the new garrison, then buy B3 via BS-P | - |
| BS-5 | Funding terminated | Buy nothing except BS-1 on the income path, with `S = 0`. No B4: each extra base adds a fully costed garrison and another attack target | - |

**Purchase protocol BS-P (the benchmark's fatal omission).**
1. Before clicking BUY, fly enough fit-armed soldiers to meet the new base's computed garrison to the purchase site, without stripping any existing garrison or the reserved sortie squad. Verify arrival; keep them homed at B1 until beds exist.
2. Click BUY; verify funds delta == price.
3. Immediately disembark the staged garrison and read back its presence before advancing time. If the UI cannot stage or disembark this way, stop expansion and resolve 05 OQ13; do not budget an unprotected hour. Recheck every 6 game hours and before every dispatch.
4. Place two Security Stations near the lift ($4,000, +2 days), Living Quarters x2 ($10,000, +3 days), and Stores ($1,000, +2 days). Add stations only if the fielded target still holds after dilution.
5. At +3 days re-home the visitors or hire the garrison locally. Never let the disembarked count in B2 fall below the floor during the swap: hire first, re-home one at a time, keep the carrier parked until the last replacement stands in the building.
6. Queue the Advanced labs and Workshop as soon as step 3 is verified (waiting for step 5 costs the Bio-Transport line a day per day); queue the Stores in the same hour (3.7).

A purchased base is a legitimate Raid/Storm target at once in the original [EXE BD08], so the window between BUY and the first Security Station is the dangerous one.

### 3.4 What goes into each base

| Base | Role | Build list | Do NOT build |
|---|---|---|---|
| **B1** (random start) | HQ: hangar, repair, medical, training, containment, small labs; computed garrison + raiders; 5/5/5 technicians | Two stations, two new quarters, demolish Psi-Gym; further quarters from actual headcount; advanced facility only with free 2x2 | Second Repair Bay (enemy entry point) |
| **B2** | Research + factory: 5-10 physicists, 10 engineers, 10 biochemists, computed garrison | Two stations; quarters for actual headcount, not a fixed three; two Stores; containment if specimens needed; AQPL, AW, ABL; reserve fourth large slot for AW #2 | Repair Bay, Psi-Gym, Training; add Medical only with a funded healing need |
| **B3** | Third Advanced Workshop, overflow beds and strike staging; 10 engineers plus computed garrison | Two stations, quarters from headcount, two Stores, AW #3 after BS-4 | Repair Bay |

Under the 4-large-per-base cap B2 tops out at Advanced Quantum + Advanced Biochemistry + two Advanced Workshops (20 engineers); a third Advanced Workshop is the real reason for B3. Home the capture/loot craft at the base whose labs consume what it brings (F2); default Valkyrie and one Phoenix in B1, a second bio-capable craft in B2 once Advanced Biochemistry runs.

### 3.5 The defence field, Security Stations and the garrison

**Rules of the original** [EXE BD04, TD10, TD7, TD8; FUN_000ac7d0 file 0x10EE74, FUN_000b42ec file 0x116990, TACP FUN_00024110]:
* **Who defends.** Every employed agent LOCATED at the base, plus class-0 agents aboard craft docked there; armament is never tested, so unarmed technicians defend and die. Four classes: agents, biochemists, engineers, physicists.
* **Budget.** `T = min(12, 4 x finished Security Stations)` (basic 0x17 and Advanced 0x18). Personnel budget = `36 - T`. If the present total exceeds it, EACH class is cut to `floor(count x budget / total)`, taking the first n of each class in array order. The battle also stops at 36 X-COM units and 60 units in total, turrets included.
* **Turrets.** One per map tile with feature byte 0xE6-0xED: 4 per station sector. Unit record 5 scaled at Superhuman: **580 HP, TU 69, reactions 51, accuracy 67, armour 57**. Basic station = Megapol Plasma Gun + 2 Megapol Plasma Pods (pod damage 35, accuracy 55, delay 120, range 900); Advanced = Disruptor Gun (42, 30, 128, 800). Real shot counts, ammunition and the spawn count beyond 3 stations are [UNBOUND] (TD11, BD20; leads `FUN_00059f54`, `FUN_000b44a4`). There is NO cap of 3 stations in the original: 12 is only the roster reservation.
* **Fielded soldiers** = `s` if `s + t <= 36 - T`, else `floor(s x (36 - T) / (s + t))` per class. With three stations (budget 24) and technicians split over three classes [DERIVED]:

| Soldiers present | Technicians present | Soldiers fielded | Technicians fielded |
|---|---|---|---|
| 6 | 15 | 6 | 15 |
| 10 | 15 | 9 | 12 |
| 12 | 30 | 6 | 15 |
| 22 | 30 | 10 | 12 |

  To field 10 soldiers once `s + t > 24` needs `t <= 1.4 s`: a base with 30 technicians needs about 22 soldiers present. Technicians cannot be moved cheaply, so the levers are fewer technicians per base, more soldiers homed where the labs are, and three stations (12 turrets are worth more than the 12 roster slots they reserve).
* **Outcome.** A defence that ends without success (loss OR retreat) removes the base; success keeps it [EXE BD14a, FUN_000b32ac]. If another base exists, every agent and craft homed there is RE-HOMED to the next active base (the game ends when the last base goes). A base building also collapses when more than 90% of its tile value is gone [EXE BD13]. A won defence debits about $50 per damaged tile with no funds check [EXE BD14b, inferred]; RG floor (a) covers it.

**Engine today** (fixes BD04+TD10, TD8+TD9, BD14a+BD16, BD13). LIVE drafts docked-craft agents, then EVERY soldier-role agent in the base, then non-soldier staff by HOME base even when away or in transit, capped at 36, soldiers first, no proportional scaling (battlemap.cpp:205-256). Turrets (:1214-1221) are 4 per finished station, uncapped, not charged to the 36, armour 0, unscaled, with invented weapons (laser 26/95/42, disruptor 70/75/40; agent_types.xml:1039-1228); there is no repair bill. Losing a base kills every agent and craft present even with a second base, leaves away units with a dangling home, and the building dies when `buildingParts` is empty (base.cpp:134-142; building.cpp:1086-1113,1165). Size from the original's table, never from engine logs, and never lose a base.

| ID | WHEN | THEN | UNTIL |
|---|---|---|---|
| DEF-1 | A base has fewer than 2 finished Security Stations | Place two near the lift; add more only after verifying the fielded target with the new reservation and staff count (03 §7.6); log M-12 | Two finished, target holds |
| DEF-2 | Soldiers `s` and technicians `t` present give `s + t > 36 - T` and fielded soldiers fall below the floor `G` | Home more soldiers there, move technicians out, or hire locally | Fielded >= `G` |
| DEF-3 | `present_armed(base) < G` (present = physically in the building, disembarked in LIVE; armed = armour + loaded weapon + 2 spare clips; the benchmark's `armed` counts any weapon item) | Do not dispatch from that base; recall or hire locally. Zero present for > 1 game hour is a P0 alarm | Restored |
| DEF-4 | A base defence is in progress | Fight to win; never retreat (retreat removes the base in the original) | Won |
| DEF-5 | An alert or investigation shows an infestation within about 14 tiles of a base | Clear it before 3 aliens can step into the base building in one hourly move (section 03) | Graph falls |

**Garrison `G` follows 03 §7.6.** Require 8 fielded armed soldiers early, 10 from week 3, and 12 under major threats. Solve the fielded formula above for the required number present after each dispatch, staffing change or station build. One disembarked soldier is an emergency no-empty-base floor, not a sufficient garrison. These are conservative policy targets, not demonstrated win guarantees; re-fit after TD1-TD3 and the defence fixes. UFO attackers use the static crew table excluding Egg, Chrysalis and Queenspawn (BD03); organisation raids cap at 20 (BD12). LIVE instead uses the building's full alien crew.

### 3.6 Hostile-organisation raids on X-COM bases

**Original** [EXE BD08 adjudicated, BD09-BD12; FUN_00092060 file 0xF4704]. Once per game day, for each organisation A (2-26) and target B (0-26 except the Aliens): the pair passes if `M[A][B] <= -50` and A has a building; pressure = `max(1, snapshot - M[A][B])`; trigger iff pressure > `rand16(timer)` and one of 10 incident slots is free; a trigger sets the timer to `80 - 2d` = 72, otherwise it drops by 1; the delay is `rand16(240)+10` units of 5 minutes (50-1,250 minutes); the incident runs against a uniformly random LIVE building of B (a purchased base counts at once). The snapshot is refreshed only at start, once per organisation every 27 hours (the rift-offer visit) and on save/load, so a relation drop keeps pressure high for up to 27 hours. Only incident codes 2 (Raid) and 3 (Storm) attack a base; on a non-base X-COM building they are a no-op. Per trigger at Superhuman: Cult and most corporations Raid **25.0%**; Megapol Raid 36.8% + Storm 25.0%; Psyke, Diablo, Osiron and any taken-over organisation Raid 29.1% + Storm 16.4%. X-COM's pool at the start is the start base plus the Recyclotorium, so `P(base | Raid or Storm)` = **1/2 with one base, 2/3 with two** (1 if the owner remap at VA 0x1017C empties the non-base pool: [UNBOUND]).

Per Cult trigger [DERIVED]: a base is attacked with probability 25.0% x 1/2 = **12.5%** (16.7% with two bases); once the Cult is taken over, 45.5% x 1/2 = 22.7% (30.3%). The Cult starts at -70..-50 toward X-COM, so it is hostile from Day 1 and the timer starts at 0, so triggers come early; each lethal Cult raid lowers its view by about 5 per kill, keeping pressure high for up to 27 hours. Plan a base-attack risk of roughly 7-9% per lethal Cult raid, assuming about one Cult trigger within a day of each raid; the real frequency is [MEASURE] M-15.

**Engine today (BD08, high, both trees).** The target comes from `Organisation::buildings` (LIVE:organisation.cpp:71-80), which `City::initCity` fills once, before the start base and `BaseBuyScreen` change `owner` (basebuyscreen.cpp:93). In a CONTINUOUS campaign X-COM's pool is only the Recyclotorium, so `RaidMission::execute` takes the else branch (organisation.cpp:1308-1338: relation +7/+12 and an event, never `DefendTheBase`); base raids appear only after a save/load rebuilt the list (100 benchmark saves: 49 pending raids on X-COM buildings, 48 on the Recyclotorium, 1 on a base). Other gaps: flat incident maps (Cult Raid 10% versus 25%) [BD10], `<` versus `<=` at -50 [BD09], non-base Raid/Storm applying relations [BD11]. **Consequence:** logged base-defence frequency understates the original and garrison sizing from engine logs under-prepares; keep the garrison armed from Day 1 whether or not LIVE ever sends the raid (fix BD08; regression test: purchase then Raid reaches `DefendTheBase` without save/load).

### 3.7 Stores traps and the sell-through rule

| Trap | Mechanism | Rule |
|---|---|---|
| Factory base with no Stores | A completed non-craft batch is destroyed and its man-hours reset (engine rule, F5) | Every workshop base has Stores >= 1 (10,000 units) before the first batch; craft lines are exempt |
| Unsold loot | A Cult raid brings about 10 kits (about 60 units each) plus 8-16 Psiclones (8 units each), about 700 units; Disruptor Gun 15, Boomeroid 10, Devastator 30, Personal Disruptor Shield 30, Toxigun 15, Heavy Disruptor Beam 160, Large Disruption Shield 90 | Sell the same day (SELL-2 in cash-need mode, else SELL-1 pace); keep `used <= 80%` in every base; an over-capacity base keeps items but blocks purchases and transfers |
| Specimens | An unresearched artifact shows $0 and the original pays $0 [EXE F-M5]; a topic needs one in stores | Never plan income on them; keep 1 of each until its research AND manufacture topics are done |
| Home-base drift | Loot lands in the craft's home base | A craft homed in a base without Stores is forbidden |

[ENGINE HEAD:research.cpp:526-577, buyandsellscreen.cpp:162, transactioncontrol.cpp:624-633; `economy_tables.md` sections 7.3-7.5; [GUIDE: UFOpaedia Known Bugs, https://ufopaedia.org/Known_Bugs_(Apocalypse)]].

## 4. Income pillars

### 4.1 Pillar: the Cult of Sirius raid loop

#### 4.1.1 What one raid pays

| Component | Value | Evidence |
|---|---|---|
| Loot spots | The Temple of Sirius map (`36CHURCH`, the only map the six Cult buildings use) holds **23 loot spots** (A7, B6, C10). Each spot draws `rand(4)` of 5 slots: A = Lawpistol, AP grenade, Psiclone, Proximity Mine, Psiclone; B = Psiclone, HE, Auto Cannon, Psiclone, Psiclone; C = Psiclone x5. Value per spot at base price: A $1,934, B $3,100, C $4,500 (Psiclone $4,500). Whole map $77,138 (16.4 Psiclone); spots per generated battle are NOT re-derived: 14.8 spots = $49,637, 12 spots = $39,536 (8.4 Psiclone), an engine-side map simulation gave 7 or 13 slots (mean $35.6k). **Plan on $30,000 gross** until M-1 | [EXE v_market/F-M9, FUN_000b3f14 file 0x1165B8, table 0x192004]; [ENGINE HEAD:battlemap.cpp:1161-1173]; `income_engine_exe_check.md` section 3a, `income_cult_raids.md` section 2.5. [GUIDE: UFOpaedia Psiclone, https://ufopaedia.org/Psiclone; GameFAQs strategy guide by Loodwig, https://gamefaqs.gamespot.com/pc/36014-x-com-apocalypse/faqs/54424] report about $45,000 and 8-10 Psiclones per raid |
| Loot gate | Mission type 3 (Raid Building) and an X-COM<->owner relation <= -50; the launch first forces X-COM->owner <= -60, so the gate passes by construction. **Engine today:** no hostility gate (every RaidHumans battle places loot; harmless for the Cult) | [EXE F-M9, F-M11] |
| Cargo requirement | Unless a party craft has cargo capacity (a Cargo Module) ALL cargo loot is lost. The original's loot recovery is [UNBOUND] (F-M14, TACP end-of-mission item recovery): treat the Cargo Module as a hard requirement until disproved | [ENGINE LIVE:battle.cpp:3259-3270,3296-3326; `EnforceCargoLimits` false so presence only] |
| Guard kits | 7-12 guards (mean 9.55), 10% Cult Leaders; a dead or knocked-out guard drops everything. Kit value per raid by tier in 4.1.2. **Engine today (TD1+TD2+TD3):** guards are scaled in the original (+16% TU, HP, reactions, stamina; human building-security HP 46-58; shown accuracy 33-50) and unscaled in LIVE, so raids are bloodier than engine logs show | [EXE BD12, TD1; ENGINE HEAD:organisation.cpp:37-48] |
| Price at sale | Psiclone $4,500 and Elerium $3,000 FALL with standing stock in the original (4.2.1). Shortfall against a frozen price, selling the whole weekly haul: 1 raid/week 1.9% over 6 weeks, 2/week 6.8%, 3/week 12.7%; at 26 weeks 24.7%, 36.3%, 40.0%. **Engine today (F-M1+F-M2):** frozen at base | [EXE F-M2 adjudicated; `v_market.md` section 5.2] |
| Tactical score | +10 per Cultist kill, +3 per Psiclone looted, leadership: about **+250 per lethal raid, +60 per stun-only raid** [DERIVED]; feeds the weekly score and the alien kit level (4.1.5) | [ENGINE HEAD:battle.cpp:2984-2992; `income_cult_raids.md` section 3.8] |
| Relations | The raid-launch write (X-COM->Cult <= -60) is TRANSIENT: the post-battle mirror wipes it, so launching costs nothing lasting. The Cult is already hostile (-70..-50 in all 21 seeds). Persistent costs are kills: a hostile Cult guard killed lowers Cult->X-COM by `S/2` = 5 (`S` = 10). Government and Megapol view the Cult at 0 and only the Aliens are allied to it (+75), so a Cult kill moves no third party. The raided organisation's post-raid "worth" debit and tiered relation hit are [UNBOUND] (F-M13) | [EXE F-M11 adjudicated, F11; starting matrix file 0x189040]; lead: who repopulates the mission descriptor at VA 0x244020 |
| **Planning yield** | **$30,000 gross, $3,000 attrition and clips, $27,000 net** per lethal raid at Cult tier 2-4; multiply by 0.93-0.99 in the first 6 weeks | ASSUMPTION(M-1, M-3) |

#### 4.1.2 The window and the kit schedule

**Rule.** WHEN the Cult's ALIEN INFILTRATION graph (Organisations tab) reads at or below 50% THEN the Cult cannot flip (the original flips an organisation only when displayed infiltration exceeds 50%, tested once per 300 game minutes with probability `(v - 100)/101` for raw `v`) - raid at the base tier. WHEN the graph rises above 50% THEN raid at once if G-C holds: a flip is permanent, arms guards three tiers higher (tier + 3) and puts aliens in the temple [EXE T1, T2, T4, T10]. The Cult is not seeded at game start (the seeded building must belong to an owner with relation >= 0, and the Cult is negative); its infiltration comes from UFO landings and from spread out of infestations of 5 or more aliens [EXE T8, BD06]. Bribing the Cult is impossible in every state (it is allied to the Aliens).

| Days | Tier (not taken over) | Kit value per raid | Tier if taken over | Kit value |
|---|---|---|---|---|
| 1-6 | 2 | $9.8k | 5 | $13.3k |
| 7-13 | 3 | $11.0k | 6 | $15.5k (Disruptor Gun enters kits) |
| 14-20 | 4 | $12.6k | 7 | $17.0k |
| 21-27 | 5 | $13.3k | 8 | $21.4k |
| 28-34 | 6 | $15.5k | 9 | $78.6k* |
| 35-41 / 42-48 | 7 / 8 | $17.0k / $21.4k | 10 / 11 | $91.3k* / $107.3k* |
| 49+ | 9-11 (the original clamps at 11; the engine reaches 12, $116.2k) | $78.6k-$107.3k* | 11 | $107.3k* |

\* Engine-as-extracted: every guard carries a $5,770 Personal Disruptor Shield; see 4.1.6. Kit value = 9.55 guards x expected kit per guard at base price (tier 2 $1,024, 3 $1,156, 4 $1,322, 5 $1,391, 6 $1,627, 7 $1,784, 8 $2,244, 9 $8,228, 10 $9,558, 11 $11,235, 12 $12,167) [DERIVED; ENGINE `income_cult_raids.md` sections 2.4 and equipment_sets.xml HUMAN_2..9]; tier = org start tech + week counter - 1, +3 if taken over [EXE T10, TD6]. The extractor drops empty grenade and equipment rows (TD6 low), so grenade and equipment values are upper bounds.

**Engine today (T1+T2, T10).** LIVE flips an organisation at any infiltration above 0 every 125 minutes, so the first nonzero reading of the Cult graph means "imminent", and the window is the observed Days 1-10: of 98-166 end-state saves the Cult was taken over in 0 of 21 before Day 7, 10 of 25 in Days 7-10, 8 of 14 in Days 10-14, 18 of 20 in Days 14-20 (`income_cult_raids.md` section 4.8). LIVE also omits the +3 tiers on takeover (one line in agent.cpp:171-175), so every taken-over row above is understated there. After T1+T2 the window is the graph, not the calendar.

#### 4.1.3 Gate G-C (all must hold before every raid)

| # | Condition (observable) |
|---|---|
| G-C1 | At least 8 fit raiders with armour and loaded weapons, after every computed garrison stays behind and disembarked. Fourteen soldiers leave only six beyond the early eight-person target; wait for hires, never borrow garrison depth by default |
| G-C2 | One flying carrier with a **Cargo Module** installed (`cargo > 0` on the interceptors/boarding row) and passengers >= raiders; the corpus gives the Valkyrie 8 and 8+4 passengers (read it). Fallback: both Phoenixes, each with a $300 Cargo Module and 4 raiders, flown to the same temple (any single craft at the building with cargo space is enough). **No cargo carrier = the whole haul is lost** [ENGINE HEAD:battle.cpp:4038-4049] |
| G-C3 | The target temple is alive and its building screen opens with the RAID button enabled |
| G-C4 | At most one raid in flight; the last raid's wounded are healed (Medical Bay free beds >= wounded; about 20 HP/day per wounded agent, 2.4) or replaced |
| G-C5 | Tactical score `T < cult_t_cap` (default 2,000) or `cash < 1 payroll` (4.1.5) |
| G-C6 | No base has `present_armed < G` after the raiders leave (3.5) |
| G-C7 | None of section 03's higher-ranked tasks is pending (base defence in progress, Government-tier infiltration sweeps, the first Transporter recovery, arms-vendor sweeps and other crash recoveries rank above a Cult raid; other sweeps below it); a raid in flight is not recalled. Carve-out: if cash < 1 payroll the raid moves ahead of the last two |
| G-C8 | Default: stop raids after Cult takeover (notice, graph at 100%, or relation -100 with bribes refused). Optional `cult_raid_taken_over=continue` experiment only: squad >= 10 with armour >= 40 and section 03's clearance for a three-way fight; all garrison and casualty gates still apply |

#### 4.1.4 Procedure (per raid)

1. Choose the temple nearest the carrier's base (all six are the same raid). Board the raiders and send the carrier (select squad, `Shift+Alt+right-click` or `BUTTON_GOTO_BUILDING`); poll "soldiers at building" > 0; abort after 4 game hours.
2. Right-click the temple -> BuildingScreen -> select the carrier's passengers -> assert `selected_agents > 0` -> press **BUTTON_RAID**. **Never BUTTON_EXTERMINATE**: it fights the Alien organisation, whose loot table is empty, so it pays $0 in organisation loot; the benchmark's only building-raid function presses EXTERMINATE [ENGINE HEAD:tools/oa_play.py:1764-1772, game/state/rules/battle/battlemap.cpp:1161]. Any comment saying RAID costs -200 relation is stale: that cost is behind `RaidHostileAction`, default off [ENGINE HEAD:framework/options.cpp:552-553].
3. Expect BattleBriefing; if a MessageBox appears read it and abort. Fight (section 03). No explosives indoors (they destroy loot). Abort if more than 40% of the squad is down within 2 game minutes - ASSUMPTION(M-3); retreating destroys the loot.
4. On victory fly home with the loot; sell per 4.3 the same day and verify `funds delta ~ sum(count x price)`. A won battle with **zero** Psiclone delta means the cargo carrier was missing: **stop the loop** and fix G-C2.
5. Ledger row: guards, kills, own KIA/WIA, items by type, Psiclones, funds delta, Cult relation band before/after, `T` delta, game minutes door to door, Government relation band.
6. Next raid no earlier than the heal/re-arm of G-C4. Raids per game day is unknown (M-2); the cash tables assume 10 raids in 28 days, not a rate.

**Continue / stop gates** [ASSUMPTION(M-1, M-3)]: after raid #1 stop if own KIA >= 3 or the loot delta is 0 on a win; after raid #2 continue only if the mean net funds delta >= **$15,000** and mean own KIA <= 1.5; stop on a base-raid event at any base (log the attacking organisation first), on a Cult takeover while the squad is < 10 or armour < 40, and when `T >= cult_t_cap`.

#### 4.1.5 Tactical-score budget (the alien kit ladder)

Alien infantry gear is set by the lifetime Tactical Missions score `T` (category 0, this week + previous); looted guard kits and alien kills both add to it. Thresholds at Superhuman: **1,000, 2,200, 4,000, 6,600, 10,000, 18,000, 28,000** (level = number passed). Each of the weapon, grenade and equipment groups draws its own uniform row 0..9 and an empty row issues nothing: grenades with probability 0.3 / 0.6 / 0.8 / 1.0 at levels 1-4, equipment 0.7 at level 4 and 1.0 from level 5 [EXE SCORE-EQLEVEL, TD4; table file 0x19BEB4]. **Engine today:** the extractor drops the empty rows, so LIVE issues grenades every time at levels 1-4 and equipment from level 4 (low: tactical score stays far below 1,000 in benchmark runs).

Weapon draw by level in the extracted sets [ENGINE HEAD:agent.cpp:165-168, extract_agent_equipment.cpp:72-170]: level 0 Brainsucker Launcher 100%; level 1 (`T` 1,000-2,199) **Disruptor Gun 70%**, which is the specimen for the Disruptor Gun topic (research 40,000 mh, +100); level 2 Disruptor 80% and Boomeroid grenades (specimen for Boomeroid, 40,000, +150); level 3 Devastator 50% (Devastator Cannon, 38,000 plus Disruptor Gun); level 4 (`T` 6,600+) Devastator 60%, Vortex Mine and a **Personal Disruptor Shield on every unit** (the extractor lists it 7 times where [GUIDE: UFOpaedia Tech Levels, https://www.ufopaedia.org/index.php?title=Tech_Levels_(Apocalypse)] says 70%; specimens Vortex Mine 46,000 and shield 60,000, +200). [ENGINE HEAD:agent.cpp:165-168, extract_agent_equipment.cpp:72-170]. Which alien species draw from these sets is not decoded. **Policy** [DERIVED]: 4 lethal Cult raids = about 1,000 `T` = level 1 (the cheapest specimen); 9 raids = about 2,250 = level 2. Seek `T >= 1,000` by about Day 10; gene `cult_t_cap` = **2,000** (stay under 2,200 until section 03 certifies armour and shields); at the cap switch to stun-only (+60 each; needs Stun Grapples $400 and a non-lethal policy the harness lacks, H12) or pause, unless `cash < 1 payroll`. Looted Disruptor Guns share the weekly pace with the factory (SELL-1). Aliens are stronger than engine logs suggest (+16% TU/HP, +64% natural armour): a mean-42 Disruptor hit no longer one-shots a Popper [EXE TD1, TD2].

#### 4.1.6 The second peak (guards at tier >= 9)

Kit value jumps from $21k (tier 8) to $78.6k (tier 9) and $107k (tier 11, the original's clamp) **in the engine as extracted**: every guard carries a $5,770 Personal Disruptor Shield (9.55 per raid = $55k), 4 of 10 weapons are Disruptor Guns, 1 of 10 a Devastator, plus Vortex Mines and Boomeroids [ENGINE equipment_sets.xml HUMAN_9]. HUMAN_9 lists only two equipment entries, both the shield, so the generator gives it to every guard; the same extraction lists ALIEN_5's shield 7 times where UFOpaedia says 70%, so the original probably gives the shield to about 2 guards in 10 (kit about $34k per raid, not $78.6k) [DERIVED; UNBOUND until the 0x19C3A0 table is read per tier]. The peak arrives on Day 49 for a free Cult and on **Day 28 for a taken-over Cult** (tier + 3). Value to us: the only cheap Personal Disruptor Shield specimen supply and one lump of about 9.5 shields (SELL-1). Gate: stay out until section 03 reports the squad can beat shielded guards (heavy weapons, own Devastator/Disruptor, >= 10 soldiers with armour >= 40). Other tier-4 organisations reach tier 9 on Day 35.

#### 4.1.7 Risks that are not diplomatic

* **Counter-raid on a base:** Cult Raid 25.0% per trigger x P(base) 1/2 (2/3 with two bases) = 12.5% (16.7%) per trigger, about 7-9% per lethal raid; 22.7% (30.3%) per trigger once taken over (3.6). Mitigation: computed garrison and two stations by default (3.5); stop on any base-raid event.
* **Travel:** an armed craft auto-fires on craft of an organisation it is hostile to (the Cult park holds 7 Hawk, 8 Valkyrie, 11 Phoenix, 5 Hoverbike); none of 43 logged craft losses was a Cult kill, but the benchmark never loitered near the Cult. Measure `craft_lost` per raid (M-2).
* **Aliens in the temple:** 3 of 6 temples held crews at Day 14.5 in one engine save: read the Cult graph first; accept three-way fights only under G-C8.
* **The loot is thrown away:** no cargo carrier, a retreat or a loss: G-C2; never retreat with the loot unless the squad is lost.

### 4.2 Pillar: manufacture and sell

#### 4.2.1 The market model (original)

Each Monday `FUN_000941dc` runs the stock pass over ALL market rows and then the price pass over ALL rows, even when funding is terminated. The rule is chosen by the row's first word `week == 0` (X-COM-makeable and alien goods, Psiclone, Elerium, AG missiles, craft), not by manufacturer; third-party items (`week > 0`) random-walk their price within +-3% a week and ignore our sales [EXE F-M1, F-M4].

| Item | Original rule (week-0 rows) | Evidence |
|---|---|---|
| Stock decay | Each Monday `s -= s/8`; then `r = rand(0..100)`: `r > 40` -> `s -= s/5`; `r > 70` -> `s -= s/3` (integer). Mean survival 0.70 a week; stocks 1-2 never decay, 3-7 barely | [EXE F-M3, FUN_00096538 @0xF8BDC] |
| Price cut | From the POST-decay stock `s`: `e = 0`; if `floor(max/2) <= s` then `e = rand(0..2)+3`; if `max <= s` then `e = rand(0..5)+5`; if `2 max <= s` then `e = rand(0..5)+10`; `price = trunc(price x (100 - e)/100)`, clamp `[base/2, 2 base]`. The price never rises; a cut is permanent; `lastStock` is not read. A sale adds its units to stock at once but pays `price x units` at the price SHOWN, so a lump of any size is paid in full and the cut hits LATER sales | [EXE F-M2 adjudicated, FUN_0009667c @0xF8D20] |
| Craft | A craft sale writes NO market stock; craft rows have min 0, max 2, stock 0, so `floor(max/2) <= s` is never true and **craft prices never fall** from sales | [EXE F-M6 survived] |
| Cumulative effect | The loss is 1-10% over 6 weeks of selling, 18-27% by 13 and 33-40% by 26; 100 Psiclone dumped at once leave the price at 0.755 after 26 weeks (Toxigun 0.503); cadence is neutral to mildly favourable to lumps (selling the pile every 1, 2, 4 or 13 weeks realises 0.838, 0.832, 0.848, 0.874): cumulative volume per type is punished, not cadence | [EXE F-M2; `v_market.md` section 5.1] |
| **Engine today (F-M1+F-M2, F-M3)** | X-COM-made week-0 goods use a weekly SOLD DELTA (`max(0, stock - lastStock)`), decay x0.80 / x0.66, cuts 95-97 / 90-95 / 85-95%, clamp `[base/2, base]`; **15 priced week-0 items not made by X-COM are FROZEN at base forever**: Psiclone $4,500 (max 40), Elerium $3,000 (40), Mini-launcher AG missile $900 (30), Heavy-launcher AG missile $700 (15) and 11 craft. Trickles are free and a dump costs one cut (about 0.899), so engine cash from repeatable loot is overstated beyond about six weeks | [ENGINE LIVE:economyinfo.cpp:16-88, gamestate.cpp:1692,1707] |

#### 4.2.2 What is reachable, and when

Gross margin = base price - materials; hours from `research.xml`; 5 engineers = 67,200 man-hours/week (400 mh/h assumed, M-6b) [ENGINE `income_manufacturing.md` section 3; `economy_tables.md` section 5.3; EXE manufacturing table @0x13FD34, identical for 42 rows].

| Product (lab) | Margin / unit | $/man-h | Units/wk at 5 eng | Research chain (man-hours) | Gate besides research | Earliest day [ASSUMPTION(M-5)] |
|---|---|---|---|---|---|---|
| Bio-Transport Module (small) | $250 | 0.062 | 16.8 | 9,000 (Day 1) | none | Day 2; **do not sell**, build 3 |
| Disruptor Gun (small) | $1,500 | 0.214 | 9.6 | 40,000 | 1 specimen | Specimen Day ~10 (`T` >= 1,000), research 3.9 days: **Day ~15** |
| Devastator Cannon / Personal Disruptor Shield (small) | $3,300 / $3,770 | 0.275 / 0.269 | 5.6 / 4.8 | 78,000 incl. Disruptor Gun / 60,000 | 1 specimen each | `T` >= 4,000 or tier 8 (Day 42): Day ~50; tier 9 (Day 49, or Day 28 if the Cult is taken over): Day ~56 |
| **Bio-Transport craft** (large) | **$22,000** | 0.629 | 1.9 (3.84 at 10) | 162,004 (122,004 + Advanced Quantum 16,000 + Advanced Workshop 24,000) | A Transporter (UFO type 3) recovered through a WON ground mission (weeks 2-4, 6 ever); Advanced Quantum Lab and Advanced Workshop built | **Day ~40-58** [DERIVED: the engine-rules estimate of Day 35-45 plus the 5-13 day shift of RR-12/RR-13] |
| Stasis Field Bomb (large) / Toxigun (small) | $1,450 / $1,580 | 0.725 / 0.527 | 33.6 / 22.4 | 86,000 + Advanced Workshop chain / 57,000 + Advanced Biochemistry | beam and launcher specimens / dead Hyperworm | Day 45+ / Day 30+ |
| Explorer / Retaliator / Annihilator (large) | $22,000 / $35,000 / $50,000 | 0.40 / 0.47 / 0.50 | 1.2 / 0.9 / 0.67 | 160k / 204k / 260k (UFO types 5, 6, 9) | UFO recoveries | Day 50+ |

**Engine today (RR-12+RR-13).** LIVE unlocks the three alien-system topics on ANY recovery, including an unmanned Probe or Scout (all ten types carry the flags, extract_vehicles.cpp:147-156) and an aborted manned recovery (battle.cpp HEAD:3639-3649, LIVE:3646-3656), so its chain starts 5-13 days earlier than the original, which needs a manned UFO recovered through a won ground mission (`web_research_tree.md` D-2). Do not tune the Bio-Transport date on LIVE logs.

Reading [DERIVED]: **before about Day 35 manufacturing is not an income pillar** (at the SELL-1 pace a Disruptor Gun line yields about $400 a week of margin, a Devastator line $3,300, a Personal Disruptor Shield line $7,500); the opening pillar is the Cult loop and the manufacturing pillar is the **Bio-Transport line**, because it alone is un-eroded and unbounded. The benchmark never manufactured anything for sale: 2,451 of 2,451 engineering-lab snapshots read `idle` (`income_manufacturing.md` section 0 (point 7)).

#### 4.2.3 The product test and the Bio-Transport line

**Product test (all must hold):** research done; specimen or gate satisfied; gross margin per man-hour >= $0.15; Stores space >= 2 x the batch footprint (non-craft); cash >= `2 x largest unit cost + next payroll + $10,000` (a job halts when the balance cannot pay the next unit: the original halts at `balance <= cost`, charged at job start and each completion [EXE RR-06]); a Buy/Sell price is displayed (hidden artifacts have none); for non-craft goods the batch fits the SELL-1 pace over four weeks or one lump of at most `max`; craft jobs wait unless at least 10 of the 80 vehicle slots are free.

**Bio-Transport line** [EXE F-M6]: craft sales never move a craft price and a new hull sells for `(34,000 - default equipment) x health/constitution + installed equipment`, about $34,000 against $12,000 of materials. Ten engineers on one Advanced Workshop (assumed 800 man-hours/hour, M-6b) make 134,400 / 35,000 = **3.84 hulls a week = $84,480 a week gross margin** (materials $46,080, wages $8,000); the same lab on Explorer, Retaliator or Annihilator makes about 2.4 / 1.8 / 1.3 hulls ($53.8k / $62.7k / $67.0k a week). The first Bio-Transport hull is the gate craft (it can cross the dimension gate [EXE VC-6]): keep one, sell the rest on completion day. Where a finished craft is placed and how many may wait is [UNBOUND]; selling on completion day avoids the question.

### 4.3 Loot and product selling policy

**Selling rules (F-M2 adjudicated).** The original charges for STANDING stock per type: a sale pays the price shown, adds its units to stock, and the cut lands on later sales. So sell each week-0 type at a pace of about **0.075 x max per week**, or in **one lump per type**, spread across many types, and never trickle one type weekly. Max stock is not displayed; the observable is the type's price on the market screen: if it fell on a Monday, hold further sales of that type. Pace and lump sizes [EXE F-M2 replay; DERIVED from it]: a lump of `max` leaves Psiclone at 0.939 after 26 weeks, 100 units 0.757, 200 units 0.607; for max <= 10 a lump keeps the price sliding for weeks because stocks of 3-7 barely decay.

| Item | Base price | Max stock | **Pace per week** (0.075 x max) | One-lump cap (= max) | `L` (optional zero-cut limit) |
|---|---|---|---|---|---|
| Psiclone / Elerium pod | $4,500 / $3,000 | 40 | **3** | 40 | 21 |
| Personal Teleporter | $18,200 | 40 | 3 | 40 | 21 |
| Mini / Heavy AG missile | $900 / $700 | 30 / 15 | 2 / 1 | 30 / 15 | 16 / 6 |
| Personal Disruptor Shield | $5,770 | 30 | 2 | 30 | 16 |
| Vortex Mine | $1,130 | 24 | 2 | 24 | 12 |
| Devastator Cannon / Stasis Field Bomb | $4,500 / $2,650 | 20 | 1 | 20 | 10 |
| Boomeroid | $840 | 14 | 1 | 14 | 6 |
| Disruptor Gun | $2,100 | 10 | 1 per 4 weeks | 10 | 4 |
| Toxigun | $2,780 | 8 | 1 per 4-6 weeks | 8 | 3 |
| Bio-Transport and every craft | - | 2 | no limit (price never falls) | - | - |

Pace from the adjudication (about 3 a week for max 40, 1 a week for max 20, one per 4-6 weeks for max 8; the rest scaled, rounded). Max stocks: [EXE F-M2 table parse: Toxigun 8, Personal Teleporter 40, Psiclone 40, Elerium 40, AG missiles 30 and 15, Stasis Bomb 20]; the others are [ENGINE economy.xml, equal to the published appendix in 136 of 140 rows, `economy_tables.md` section 9.1; [GUIDE: Wong's Apocalypse FAQ, appendix 16.1.9, https://gamefaqs.gamespot.com/pc/36014-x-com-apocalypse/faqs/1731]]; the harness reads max from that table because it is not displayed.

| ID | WHEN | THEN | UNTIL |
|---|---|---|---|
| SELL-1 | A week-0 type is in stock (default rule) | Sell at most the pace above per week, or one lump of at most `max` once; hold the rest in Stores. Spread surplus over types | The type is exhausted |
| SELL-1b | **Optional, only after M-8 passes** (a sale raises the displayed market stock `D` by exactly the units sold and the price holds while `D <= L`) | Sell at most `L - D` units at any time: the smallest possible decay leaves `s - floor(s/8)`, and a cut needs post-decay stock >= `floor(max/2)`, so `L` (the largest `s` with `s - floor(s/8) < floor(max/2)`) can never be cut [DERIVED]. Expected steady volume about `0.3 x L` a week (6 Psiclone) | `D >= L` |
| SELL-2 | **Exception to "never trickle", cash-need mode:** cash is below 6 weeks of burn, OR funding is cut, OR an income-path purchase waits on RG | Sell the WHOLE haul on arrival. Justified by the horizon numbers: selling the whole weekly Cult haul costs about 7% of Psiclone proceeds over 6 weeks at 2 raids a week (13% at 3), 33-40% by 26 weeks [EXE F-M2, `v_market.md` section 5.2] | Cash >= 6 weeks of burn and no waiting purchase |
| SELL-3 | A type's displayed price on a Monday is below its Sunday price, or below 0.85 x base | Hold further sales of that type, even in cash-need mode unless cash < 1 payroll | The price holds for two Mondays; then resume at the SELL-1 pace |
| SELL-4 | Human guard kit and other week > 0 items | Sell at once (+-3% a week, unaffected by our sales) | - |
| SELL-5 | A craft completes | Sell on completion day, undamaged (value scales with health, F-M7), after reserving the gate craft | - |

**Engine today (F-M1+F-M2):** Psiclone, Elerium and AG missiles are frozen and trickles are free, so SELL-1 only costs patience in LIVE, and for X-COM-made goods the engine charges the weekly sold delta, which SELL-1 also satisfies. The 1.4 ten-raid rows count every raid's loot as sold on arrival (cash-need mode); once cash passes 6 weeks of burn the default pace would hold part of each Psiclone haul, so later rows are cash plus inventory.

**Policy by item class.**

| Item class | Rule | WHEN / HOW MUCH | KEEP |
|---|---|---|---|
| Static loot (Psiclone, Elerium pod, AG missiles) | Week-0 falling rule | SELL-1 pace or one lump; the whole haul on arrival in cash-need mode (SELL-2) | 0 |
| Human guard kit and surplus air stock (Lawpistol, M4000, Auto Cannon, Plasma Gun, launchers, grenades, Proximity Mines, clips; Janitor, fuel, spare Bolter/Lancer) | Week > 0 rows: +-3% a week, independent of our sales | Same day, all except the squad template quota; air stock only when `cash < 1 payroll` | The roster's unmet need and the air plan |
| Unresearched artifacts (Disruptor Gun, Boomeroid, Devastator, shields, Vortex Mine, Cloaking Field, Teleporter) | The UI shows $0 and the original pays $0: **never plan income on them** | Never | All until research (and manufacture topic, if wanted) completes |
| Researched artifacts, manufactured goods, craft equipment and ammo (Stasis Bomb, beams, shields) | Week-0 | SELL-1, sharing the weekly pace with the factory's output of the same item | 1 specimen of each until its research is done |
| Craft (Bio-Transport, Explorer, Retaliator, Annihilator) | No stock effect | Completion day, all but the gate craft | The gate craft |
| Scrapped recovered-UFO equipment | Credited at 25% of the item's price [ENGINE; not EXE-bound] | The day the recovery is won, by class | Items a topic gates on |

Timing: price and stock change only at Monday 00:00 (stock pass, then price pass); everything sold since the previous tick counts at the next one. Log the Sunday 23:00 and Monday 00:10 price and stock (SO-F2).

### 4.4 Raids on other organisations

| Organisation class | Why not, or when | Rule |
|---|---|---|
| Government, Megapol, Transtellar, Marsec, Superdynamics, Cyberweb, General Metro, Solmine | Sponsors and suppliers: buying from a manufacturer whose view of X-COM is <= -50 is blocked and a Government below -50 ends funding. A non-hostile guard killed costs -30 on the owner's view of X-COM, then -5 once hostile [EXE F11] | **Never** |
| Diablo, Osiron, Psyke | Hiring is relation-blind in the original (RR-10), so hostile ones cost no supply there (**Engine today:** a hostile organisation stops supplying). Their raid-back table is the military one (3.6) | Off by default. Optional gene `org_raid_specimen`: only if no Disruptor Gun specimen is in stores by Day 21 (tier-4 organisations reach tier 6 on Day 14; P(at least 1 of about 13 guards carries one) = 94.5%) |
| An organisation whose "alien takeover" notice has appeared | Relation -100, bribes refused, supplier gone | WHEN the notice names X AND section 03 has ordered a sweep of one of X's buildings because aliens are known inside THEN press BUTTON_RAID instead of EXTERMINATE: X's guards join the fight either way (HEAD:battlemap.cpp:296-310), so RAID adds the loot at no extra enemy. Never raid a taken-over organisation's building with no known alien crew: it adds the owner's guards (Megapol 12-20, Government 10-17), and EXTERMINATE on an empty one is the -9 "empty investigation" |
| Stun-raids on friendly organisations ("free loot") | The launch write is transient (F-M11) and a stun raid kills nobody, but the raided organisation's post-raid "worth" debit and tiered relation hit are UNBOUND (F-M13). [GUIDE: UFOpaedia Stun Raid, https://ufopaedia.org/Stun_Raid] says nobody turns hostile if nobody is hurt | **Do not use** for any go/no-go |

### 4.5 Diplomacy and bribes

Four relation-recovery levers exist and nothing recovers passively [EXE F14]: (1) the hourly **rift offer** (`FUN_00096bc8`, one organisation per call, each examined every 27 hours): needs `snapshot - M > 15`, `M < 25`, the organisation not taken over and `rand16(0..100) > 10 (d+1)` (49.5%); price `max(1, -M/2) x (d+1) x clamp(|org balance|, 1000, 400000)/1000 + 1000 (d+1) + rand(0..2000 d + 4000)`, about $65-77k for a rich organisation at -60; accepting sets the relation to 0 if negative, else +25 [EXE F15a] (**Engine today, F15b:** any positive relation jumps to 100); (2) the **on-demand bribe** (Diplomacy "Offer settlement", `FUN_00097234`): refused if the organisation is taken over, allied to the Aliens (>= 75) or already allied; the `+5000 + 3000 d` = $17,000 term is confirmed, the rest compared only partly [EXE F17]; (3) the **alliance request** (`FUN_000978c0`; engine: enum only); (4) **ripple rewards**, about +2 Government and Megapol per UFO downed [EXE F06]. The harness reads the price off the screen and never computes it.

Indicative engine price `5 x improvement x clamp(org balance/1000, 1, 400) + $17,000` (`improvement` = points to the next band edge -50 / -25 / +25 / +75) [ENGINE HEAD:organisation.cpp:1129-1226]: 10 points costs $37,000 for a rich organisation (balance >= $400k) and $22,000 for a poor one ($100k); 50 points (-100 -> -50) $117,000 / $42,000. A relation point of a rich organisation costs about $2,000: an empty-building investigation (-9) about $18,000, an intentional destroyed tile (-25) about $50,000. The bribe screen is reachable by choice (Organisations tab -> organisation -> `BUTTON_BRIBE`, HEAD:cityview.cpp:1811-1820); the benchmark closes the event offer unpaid.

| ID | WHEN | THEN |
|---|---|---|
| DP-1 | Government band is Unfriendly or worse (or fell 2 bands in a week), it is NOT taken over, and it is before Monday 00:00 | Pay the Bribe-screen price if <= 35% of cash (a Hostile Government lifted to -50 is not below -50, so no cutoff) |
| DP-2 | A supplier we need (Marsec, Megapol, Superdynamics, Transtellar, Cyberweb) is hostile and not taken over | Pay if <= 15% of cash and section 02 names the supply as needed |
| DP-3 | The organisation is the Cult, taken over, or allied to the Aliens | Never (the screen refuses) |
| DP-4 | A rift offer appears | Accept only for Government, Transtellar, Marsec or Megapol and only if `cash >= 3 x cost` |
| DP-5 | Cult-raid cash arrives while a DP-1/DP-2 target is open | Spend loot there first: one raid pays one 25-point bribe of a poor organisation. [GUIDE: a Hard-difficulty player paid $32,000 to move Transtellar from Hostile to Unfriendly, https://steamcommunity.com/app/7660/discussions/0/1862741826500772553/] |

## 5. Funding protection

Section 03 owns the military half (keeping the Government from being taken over, base defence, interception). This section owns what money and the score ledger can do: the cutoff test exactly as the original runs it, the weekly score budget, the Government's relation as an asset with a price, and the ladder after a cut.

### 5.1 The cutoff, exactly (G2 adjudicated)

At each Monday-midnight rollover `FUN_000941dc` (file 0xF6880) assesses funding BEFORE the score rollover and before overheads [EXE v_gov_income/G1, G2; adjudications.md]:

1. If the permanent latch word is set, the assessment is skipped (overheads and the bankruptcy check still run).
2. Otherwise funding is **cut** if (a) the **relation leg**: the signed byte Government->X-COM is below -50 (strict: -50 does not cut, and the organisations screen already reads "Hostile" at -50, one point before the cut); or (b) the **score leg**: `H` is below -2,400 (strict), where `H` is the lifetime total of all COMPLETED weeks (the sum of the eight "previous weeks" score words), tested alone and never as `H + W`. The week being assessed never enters the test.
3. On a cut: balance -= the old income (the credit is undone), income := 0, latch := 1 for ever (no clearing writer exists).
4. Otherwise the tier delta from this week's score `W` (1.5) is applied to the old income `F` and the result is stored.

Because the rollover runs after the test, **whatever the Score screen's lifetime total (previous weeks + this week) reads at Monday #n 00:00 becomes `H` at Monday #(n+1)**, and nothing scored in between can change it. Counterexamples from the rollover bytes: weeks of -3,000 then +5,000 cut at Monday #2; -1,300 twice first cut at Monday #3; five weeks of -500 cut at Monday #6; +3,000 then -5,000 leaves `H` = -2,000 at Monday #3 and no cut. "One bad week cannot end funding" is false (one week below -2,400 cuts the next Monday), and "two consecutive bad weeks" is neither necessary nor sufficient. A week-1 cut can only come from the relation leg, because `H` is 0 at Monday #1.

**What the harness tracks (Score screen):** `P` = previous-weeks total (this decides the COMING Monday and is already fixed: if `P < -2,400` the cut is locked in, go to the ladder 5.5 now) and `LT` = `P` + this week so far (this becomes `H` at the Monday after the coming one, and can still be moved until the coming Monday 00:00). Margin `m = LT + 2,400`:

| Mode | Margin `m` | Behaviour |
|---|---|---|
| GREEN | >= 1,500 | Normal |
| AMBER | 500-1,499 | Research only score-dense topics; Cult raids move to the front of the DISCRETIONARY queue but never ahead of section 03's priorities 1-5; no new score-negative discretionary activity |
| RED | < 500 | Queue research by score density, Cult raids every 2 days when G-C holds, finish any research that completes within 48 hours first, skip every purchase that does not add score or income |

Neither mode imposes a blanket hold-fire: a UFO that leaves the city costs -12..-175 (5.2) against 0-40 for a building hit, and after a takeover the Government relation is pinned at -100, so a hold-fire has no relation benefit and only grows the Incursions column (`failure_forensics.md` section 4: -888 to -3,145 per save). Fire discipline (target selection over open ground) is section 03's. Section 03's lifetime-score governor reads the same ledger (`LT`); its bands govern military actions and these govern spending.

**Engine today.** LIVE implements all of this (`previousWeeks = totalScore.getTotal() - weekScore.getTotal()`, test `hostile || previousWeeks < -2400` at LIVE:gamestate.cpp:1765-1801, 1793; 0 mismatches in 393,120 differential cases) except two fidelity leftovers: the relation is a float `x < -50` (organisation.cpp:1089; G13) and the alien-dimension skip is not modelled. **HEAD diverges** (222,714 cases): it tests the lifetime total INCLUDING the current week (D1) and re-tests and resets the week in the UI (D7: HEAD:weeklyfundingscreen.cpp:61, :120); fix = take the funding hunk of commit `879d0d73` (on `khallmark/develop-reboot`, not this branch). Keeping `LT >= -2,400 + guard` at every Monday midnight protects under both rules. Observed in the benchmark (lifetime total including the current week): 29% of Day 15-18 saves, 67% of Day 19-25 saves and 80% of Day 26+ saves were already below -2,400; only 1 of 166 runs ended on a clean score-only cut (`failure_forensics.md` sections 4 and 10).

### 5.2 The score ledger (what moves `W`, with sign and size)

| Category | Writer and size (original) | Sign | Who controls it | Engine today |
|---|---|---|---|---|
| 0 Tactical missions | After each battle: `+v` per hostile kill, `-v` for a non-hostile or own loss, combat rating, leadership (zero if `30 x casualty points > 1000`), live and stunned aliens regardless of storage; about +250 per lethal Cult raid, +60 per stun-only raid | + | Raid cadence (here), section 03 | No leadership clamp, live aliens only with storage (low) |
| 1 Research | `research_data.score` once per topic; 99 topics sum 21,010; examples: Dimension Gates +50, Bio-Transport Module +80, Anthropod autopsy +200, Hyperworm autopsy +150, Alien Control System +150, Dimension Probe +80, Advanced Workshop +80; alien-building topics +300..+480 | + | Queue order (here) | Credited in the UI handler and lost when another stage is pushed in the same frame, about 0.6% of completions (fix SCORE-C1) |
| 2 Incidents | -30 per new building incident, whatever the answer | - | Section 03 / 02 (kill drop ships) | **SCORE-C2:** an infested X-COM-owned building is force-detected and bills -30 about every 7 hours; big single-species stacks in third-party buildings are rarely detected (12 Anthropods in a Corporate HQ: 2.3% per visit against 32.3%) |
| 3 UFOs shot down | `+hull score` iff the dead craft is Alien and the firer X-COM, once per vehicle: Probe 50, Scout 100, Transporter 150, Fast Attack 200, Destroyer 250, Assault 300, Bomber 300, Escort 250, Battleship 500, Mothership 700. Lifetime 1,000 / 2,000 / 4,000 / 8,000 here arms UFO shields, large shields, cloaks and teleporters from the next midnight [EXE F9] | + | Section 02 | Match (vehicle.cpp:2120) |
| 4 X-COM craft lost | `-score` for any destruction: Valkyrie 150, Hawk 250, Annihilator 550, Retaliator 450 | - | Section 02 | Only when an attacker is known (low) |
| 5 Incursions | `-floor(score/4)` per alien ship leaving the human city through a gate, withdrawn ships included: 12, 25, 37, 50, 62, 75, 75, 62, 125, 175 | - | Section 02 | Match |
| 6 City damage | `-byte[tile+0x0C]` per projectile hit on a human-city building footprint by X-COM OR Alien fire, destroyed or not: 0-40, about 9 on average, most often 20 [C6 adjudicated] | - | Section 03 (fire discipline over buildings) | Projectile writer matches; **SCORE-C6:** LIVE also charges every falling or crashing vehicle that plows through building scenery (vehicle.cpp LIVE:1027-1030) which the original levies only for the Overspawn: part of the engine's city-damage sink is engine-only |
| 7 Alien buildings | `+table[f]` on a successful dimension raid: 250-500 each, 3,550 for all ten | + | Endgame | LIVE only (battle.cpp:3578-3587) |

Measured drift in the benchmark (39 saves at Day >= 14): incursions -1,640, city damage -1,342, alerts -618, research +708, UFO kills +187, craft lost -86, tactical +67 = -2,723, about **-910 a week net**(`web_economy_politics.md` section 1); some of the alert and city-damage sink is engine-only (SCORE-C2, SCORE-C6).

### 5.3 Weekly score budget (target `W >= +400` every week)

| Source | Per-week size | Plan |
|---|---|---|
| Research, BioChem lab (5 staff, 67,200 mh) | +670 (density 0.010) to +1,680 (0.025) when never idle | Order topics by **score per man-hour**: autopsies (0.019-0.025) > species with a live specimen (0.013-0.017) > Bio-Transport Module (0.009) > Physics (alien systems 0.006, Probe 0.007, Advanced labs 0.005). In the original the BioChem lab has no startable topic until the first alien body arrives |
| Research, Physics lab | +170 to +470 | Gated by UFO recoveries; accept |
| Cult raids | +250 each (lethal) | 2 a week = +500 |

**Week 1 without the engine's day-0 filler** [DERIVED, ASSUMPTION(M-6)]: Bio-Transport Module +80 and Dimension Gates +50 = +130 from research, plus an Anthropod autopsy (+200, 8,000 mh = 18.6 hours at 430 mh/h) if a body reaches the lab by Day 3, plus 2-3 lethal Cult raids (+500..+750), less 2-4 alerts (-60..-120) and any escaping Probe or Scout (-12 / -25): +330 to +1,000. Reaching `W >= +400` in week 1 therefore needs at least two lethal Cult raids or an early autopsy; research alone does not. An estimate of +510 for week 1 counts the engine's +380 alien-building filler and is not available in the original. A week above +800 raises the stored income by F/16 = +$5,000 for good (1.5).

### 5.4 The Government as an asset

* **Band, not integer.** The Organisations tab shows Allied / Friendly / Neutral / Unfriendly / Hostile (>= 75 / 25 / -24 / -49 / <= -50). The Government starts at 55-75. Check **every Sunday 12:00** and after every takeover notice: the band must be Neutral or better; a fall of 2 bands in a week -> evaluate DP-1 at once.
* **What costs relation** (X-COM acts; `resolved-facts.md` section 4.1):

| Event | Original | Engine today |
|---|---|---|
| Intentional hit on a building, tile survives / destroyed | owner -5 / -25 (one call) | -5 / -5 then -20 |
| **Stray** hit, tile survives / destroyed | **0 / -5** | -5 / -25 (fix F02: over-punishes) |
| Government/Megapol craft destroyed outright | victim -10 once (-20 is UFO-only) | -30 (fix F05) |
| Investigate Building on a building with all 13 crew bytes zero | owner -(5 + d) = -9, with ripple (Megapol building: Government about -3) | -9, no ripple |
| UFO downed by X-COM | Government/Megapol about +2 each | about +0.6 (fix F06) |

  From Government 65 the cutoff (-116 away) is about 5 intentional destroyed tiles, 24 intentional surviving hits, 24 stray destroyed tiles or about 12 Government craft in the original, against 5 destroyed tiles of any intent, 24 hits of any intent or 4 craft in the engine [DERIVED]. **Rule:** never fire at buildings the squad is not ordered to attack (free in the original, -5 per destroyed tile in the engine), never shoot Government or Megapol craft, never Investigate without confirmed crew.
* The Government pays at most half its treasury per assessment: irrelevant at Superhuman (it held $1.0-4.2M in all 164 saves) [EXE G5]; do not protect civilian income for the cap.
* **After a takeover nothing restores it**: relation -100 both ways, bribes and rift offers refused, the cut arrives at the next Monday [EXE T3, T5] (the only reversible takeover is the Mutant Alliance, by a won Food Chamber raid). Go to 5.5. **Engine today (T1+T2):** treat ANY nonzero Government infestation as imminent; the 50% margin does not exist yet.

### 5.5 Funding-cut ladder and the bankruptcy floor

| Step | WHEN | THEN |
|---|---|---|
| C-1 | The Weekly Funding screen shows termination, OR the Government is taken over, OR `P < -2,400` | Mode CUT: stop ST-1/2/3/6 hires and BS-2/4; keep ST-4/5 only if the facility is on the income path and RG(3) passes with `S` = 0 |
| C-2 | Idle technicians (no seat or no startable topic) for > 3 days | Fire them (penalty $0): saves $800/week each |
| C-3 | Any facility with upkeep and no user (Psi-Gym if not yet removed, Training Area with < 3 trainees, a second Medical Bay) | Demolish (finished facilities refund 0; stops upkeep) |
| C-4 | Balance < 4 payrolls | Sell craft in this order: spare Phoenix, spare Bolter/Lancer stock, then hold the Valkyrie; sell stock in cash-need mode (SELL-2) |
| C-5 | Always in CUT mode | Required self-income >= burn ($26-30k a week early, $47-64k with staff): one Cult raid a week or the first Bio-Transport hull a week covers the early burn |
| C-6 | Every Sunday | **Bankruptcy floor.** The game ends at the first Monday that leaves the balance below -$5,000 [EXE G8, FUN_00060ea4 @0xC3548]. Survivable Mondays after the cut Monday = `floor((balance_after_cut_Monday + 5,000) / burn)`: $100,000 with $22,900 burn gives four; the no-raid cut row of 1.4 loses at Monday #7. IF `balance - burn < -5,000` for the coming Monday THEN liquidate (craft, stock, staff) before it |

Salaries and upkeep are charged every Monday even after funding ends [EXE FUN_00098A70 @0xFB114]. **Engine today (G8):** there is no bankruptcy path (`GameLost` only when the last base dies, HEAD:base.cpp:155-159) and 4 of 164 logged campaigns went below -$5,000 and carried on; fix G8 after G4. The harness enforces the -$5,000 floor itself so that results transfer to the original.

## 6. Calendar of economic milestones, weeks 1-16

Targets are gates, not promises: "on track" means the 1.4 rows (ASSUMPTION(M-n)). The benchmark's median campaign ends around Day 13-20, so everything after week 4 is conditional on surviving.

| Game-week (days) | Fixed facts | Economic milestones | Go / no-go |
|---|---|---|---|
| **W1** (Day 1 noon to Day 7 midnight) | No Monday yet; early alert, Probe/Scout growth, Cult tier 2 | Verify opening ledger, two stations, research +130 and legitimate autopsies, manufacture capture modules. Cult raids wait for eight raiders beyond garrison and funded hires; may slip past week 1 | G-C and RG; Sunday cash >= payroll + $10k |
| **W2** (D7-13) | **Monday #1**: funding credit, market pass, tier +1 (Cult 3); Transporters (3) grow: a manned UFO becomes possible | Read income (expect $80,000), `W`, tier; ST-0 trigger; Day-4 hires; `T` about 1,000 after 4 lethal raids; BS-2 checks from Day 10 | RG every purchase; M-8 after the first Psiclone sales |
| **W3** (Day 14-20) | Monday #2; Cult tier 4; tier-4 orgs reach tier 6 | Disruptor specimen research; B2 only through BS-P with full garrison; Physics advanced path requires won manned recovery | Raise fielded target to 10; `T` vs `cult_t_cap` |
| **W4** (D21-27) | **Monday #3** (LIVE's median funding cut was Day 15.5, an artefact of T1+T2); Attack missions begin (9/1/0/0); Cult tier 5; Transporter growth ends (3+2+1 = 6 ever) | Balances per 1.4; first Disruptor Gun sales at the SELL-1 pace; ST-4 hire-ahead closes with 10 seats | Margin mode (5.1); RG(3) for income-path items |
| **W5** (Day 28-34) | Monday #4: Subversion begins, exposure matters; Cult tier 6 | Staff AQPL, research Bio-Transport when UFO3 and Probe topics complete; two stations and computed garrison at each base; use actual balances, not 1.4 forecasts | BS-3 if attacked; cut ladder if needed |
| **W6** (D35-41) | **Monday #5**; Cult tier 7; tier-4 organisations reach tier 9 | Advanced Workshop built: Bio-Transport manufacture with **10 engineers** (ST-5) when the product test passes; hull #1 = gate craft | Reserve `2 x $12,000 + payroll + $10,000` |
| **W7** (D42-48) | **Monday #6**; Cult tier 8; Overspawn missions begin | Hull #2 sold on completion day (craft price should read unchanged on Monday #7); Devastator and shield specimens if tier-8 kits or `T >= 4,000` | Read the craft price on Monday #7 |
| **W8** (D49-55) | **Monday #7**; free Cult reaches tier 9: second peak (4.1.6; a taken-over Cult reached it on Day 28); Mothership grows | Advanced Workshop #2 in B2's spare 2x2 with 10 engineers; hulls sold weekly; shield research from the first captured shield | Gate in 4.1.6 |
| **W9-W10** (D56-69) | Mondays #8-#9; Cult tier 10-11 (clamp) | Craft plus 3+ goods cycling (SELL-1); self-income (loot + product margin) **>= burn** ($47-64k) | Two weeks below burn -> ladder C-2..C-4 |
| **W11-W12** (D70-83) | Mondays #10-#11; Cult tier 11 | Second Advanced Workshop staffed; target self-income **>= 1.2 x burn**; B3 operating | No B4 (BS-5) |
| **W13-W16** (D84-111) | Steady state | Reserve >= 6 weeks of burn; hiring only into free seats; caps respected (2.4) | Monthly RG re-run on open plans |

## 7. Standing-order index

All triggers run on the **game clock** (the benchmark gates on wall-clock seconds, `harness_audit.md` section 3.1). The loop a driver runs, one line per rule (detail in the cited section): **money** - SO-F1 Sunday 12:00 `cash >= next payroll + $10,000`, else liquidate surplus (1.3); SO-F2 Sunday 23:00 and Monday 00:10 log price and stock of held items (4.3); SO-F3 Monday 00:05 read Weekly Funding and Score (`W`, tier, income, `P`, `LT`) (5.1); SO-F4 purchase > $5,000: RG(a)(b)(c) (1.4); SO-F5 `m = LT + 2,400` modes, and `P < -2,400`, termination or a Government takeover: ladder C-1..C-6 plus the Sunday bankruptcy-floor check (5.1, 5.5). **Raids** - SO-R1 Day >= 2 and G-C1..G-C8 hold: Cult raid with BUTTON_RAID (4.1); SO-R2 `T >= cult_t_cap`: stun-only or pause (4.1.5); SO-R3 loot lands: sell per SELL-1/2, verify the funds delta, zero Psiclone delta on a win -> stop (4.3); SO-R4 guards at tier >= 9: no raid unless section 03 certifies the squad (4.1.6); SO-R5 Cult graph above 50%: raid at once if G-C holds (4.1.2). **Staff, bases, goods** - ST-0..ST-8 (2.2), BS-1..BS-5 with BS-P (3.3), DEF-1..DEF-5 every 6 game hours (3.5), Stores before any workshop batch (3.7), the product test and SELL-1..5 (4.2, 4.3), DP-1..DP-5 (4.5).

## 8. Where `docs/campaign-plan.md` is wrong or stale for this section

Stale there (cite symbols, not line numbers), corrected above: selling at most one craft a week and "the crash rule does not apply to loot" (craft never fall, Psiclone and Elerium do: 4.2.1); a binary funding modifier and a lifetime cutoff including the current week (1.5, 5.1); "nothing writes `tech_level`" (4.1.2); fire-Sunday/rehire-Monday (2.3); $18,000 starting overheads and a $100,000 opening without the $22,900 burn (0.3); 900 man-hours/hour per lab (starting labs run 410-430); second labs and quarters in one Day-0 queue (quarters and Security Stations first); a second base in "week 2" without BS-P; "Investigate, never Raid" (true for infiltrated human buildings only: Cult temples pay only through BUTTON_RAID); spare Bio-Transport Modules as a product (margin $250 per 4,000 man-hours); hiring at 90+ skill every midnight (P = 27%, 1.5 arrivals per class per day); inert Security Stations (4 turrets each); "every agent trains Physical forever" (starters are pre-assigned Combat, hires start unassigned). The claim that `RESEARCH_ALIEN_BUILDING_0` needs the alien dimension is CORRECT for the original (The Alien Dimension 6,000 mh, then 30,000 [EXE VC-2, VC-3]) and wrong only in the engine.

## 9. Harness requirements

Everything must be a click, a keypress or a number a human can see. Status from `harness_audit.md` sections 3.3 and 7.

* **H1** `raid_org_building(building, squad, carrier)`: select the squad, fly the carrier, open BuildingScreen, press BUTTON_RAID, win, verify cargo arrived, write the 4.1.4 ledger (NONE; only `raid_infiltrated_building`, which presses EXTERMINATE). **H2** carrier read-out (`cargo > 0`, passengers) and fitting a Cargo Module (NONE). **H3** organisation read-out: relation band, takeover notice, infiltration graph, building list (NONE).
* **H4** multi-base operations: `select_base`, `bases_detail` (beds, Stores, agents homed/present/armed, labs per base), `base_grid`, `disembark(base)`, `build_kit`, `demolish`, `hire_into(base, role, n, min_skill)`, `transfer` (NONE). **H11** garrison readiness: `present_armed` (armed = armour + LOADED weapon + 2 spare clips) and the fielded-soldier formula (3.5) (NONE).
* **H5** Buy/Sell reader: price and stock of every row, per-line quantities, the pace table, game-clock sell pass, price snapshots (partial: `sell_named`, Psiclone-only `sell_surplus_loot`, wall clock). **H6** Weekly Funding and Score readers (`W`, tier, income, `P`, `LT`, `m`) and the Day-1 funds read (LIVE has `margin_to_cutoff` for one rule). **H7** bribe flow: Organisations tab -> BUTTON_BRIBE -> read price (NONE).
* **H8** research manager on the game clock: idle-lab watcher, score-density queue, hire-ahead timers, acknowledging research-complete notices, since the score is credited there (partial). **H9** manufacturing manager: a topic in several workshops, quantity 1-50, batch from the SELL-1 pace, cash-reserve guard (partial). **H10** recruit reader: stats, maximum role skill, Monday window, fee/bed pre-check, own caps (NONE; always row 0, hires stall at 30 beds).
* **H12** non-lethal raid policy and `cult_t_cap`; **H13** training assignment for hires without toggling starters off; **H14** game-time triggers instead of wall-clock gates; **H15** specimen keep-list and squad-template quota (all NONE).
* **H16** benchmark reward: stop rewarding `second_base` (a constant earned in minute 1) and `infil_raids` polls; add self-funded income per week, labs idle-free %, lumps sold; do not charge -800 for a base lost with no garrison present (LIVE `oa_learn.py` has the old terms). **H17** own-limit enforcement while the engine lacks them: -$5,000 floor (G8), hire caps (RR-09), 6 small / 4 large labs per base (F-M12), no purchase that fails RG (NONE).

New genes: `cult_t_cap` 2000; `org_raid_stun` off; `org_raid_specimen` off; `runway_weeks` 6; garrison fielded targets 8/10/12 per 03 §7.6 (present headcounts derived from staff and stations); `bribe_gov_max_cash_pct` 35; `cash_need_weeks` 6; `price_backstop` 0.85; `cut_reserve_mondays` 4.

## Open measurements

Every [MEASURE] item, in the order to run them, with what it replaces and the threshold a gate uses.

| ID | Measure | How | Replaces | Threshold used by a gate |
|---|---|---|---|---|
| M-14 | Which world LIVE is (G4) | Day-1 funds and the first Weekly Funding screen | The `S` in RG | About 100,000 / 80,000 = original; about 180,000 / 10,000 = LIVE regression |
| M-1 | Cult net loot per raid and loot spots per battle (7, 13, 14.8 or 23) | Funds delta after sale, items by type, Psiclone count; count loot items on generated Temple battles (HEAD:battlemap.cpp:548-880) | $30,000 gross / $27,000 net | Continue iff mean net >= $15,000 after raid #2 |
| M-2 | Cult raids per game day, door-to-door minutes, craft lost per raid | Ledger per raid | Ten raids in 28 days | None (cadence) |
| M-3 | Own attrition and guard lethality | KIA/WIA per raid; the minute at which > 40% are down | $3,000 attrition; abort rule | Continue iff mean KIA <= 1.5 |
| M-5 | Specimen timeline | Day each of Disruptor Gun, Boomeroid, Devastator and shield first enters stores, and from which source | Day ~10 / 40 / 50 / 56 | None |
| M-6 | Weekly score components | `W` by category each Monday | The +330..+1,000 week-1 estimate | `W >= +400` |
| M-6b | Real staffed workshop rate | Man-hours/hour with 5 and with 10 staffed engineers (large-lab ceiling 1,000/h) | The assumed 400 and 800 | Hulls/week in 4.2.3 |
| M-8 | Displayed market stock equals the stock the Monday pass reads | Sell n of a week-0 type, read stock before and after, then price and stock at Monday 00:10 | Switches SELL-1b on | Stock rises by exactly n; price unchanged when stock <= `L` |
| M-10 | Recruit pool | Arrivals per day and count at skill >= 85 per role over 14 days | 1.5 arrivals, 39% >= 85 | Hire-ahead window |
| M-11 | Base purchase | Price delta; pre-purchase staging and immediate disembark read-back; raid eligibility without save/load | F4, BD08, OQ13 | Full computed garrison verified before time advances; otherwise stop expansion |
| M-12 | Security Stations in a real defence | Turrets spawned (3, 4+ stations), kills, fielded soldiers against the 3.5 formula | Unbound turret shots and spawn count | Turret count > 0 |
| M-15 | Base-attack frequency per lethal Cult raid | Base defences per Cult raid after fix BD08 (or after a save/load in LIVE) | The 7-9% estimate | Garrison floors |
| M-16 | Government survival after T1+T2; week-1 air and loadout spend (M-9) | Re-run the benchmark after the fix; ledger of sections 02/03 purchases | The Day 15.5 median cut; the $40,000 placeholder | B2 timing |

**Unbound (no EXE evidence; leads):** base-purchase price and refusal rule (handler near `FUN_00015400`); the post-raid `worth` debit and tiered relation hit (F-M13; who repopulates the mission descriptor at VA 0x244020) and loot recovery and cargo capacity (F-M14; TACP end-of-mission recovery); turret shots, ammunition and spawn count beyond 3 stations (TD11, BD20; `FUN_00059f54`, `FUN_000b44a4`); empty equipment rows at tier >= 9 for human guards (table file 0x19C3A0; TD6); pay-byte growth (G14; agent `+0x57`); where a finished craft is placed (completion handler near `FUN_000aa5e4`); `P(base | Raid or Storm)` 1/2 versus 1 (owner remap at VA 0x1017C versus `FUN_00014854`).
