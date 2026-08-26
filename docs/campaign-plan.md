# X-COM: Apocalypse — Unified Campaign Plan

A single end-to-end plan for winning X-COM: Apocalypse **as OpenApoc implements it**, merged
from three community guides and reconciled against this repository's source and data.

**Sources merged**

| Tag | Source |
|---|---|
| **NKF** | *NKF's X-COM Apocalypse: Starter's Guide* (ufopaedia.org) |
| **LIL** | *Lilura1 — X-COM Apocalypse Walkthrough / How to Play* |
| **AOW** | *AllOutWar's X-Com Apocalypse Guide* (ufopaedia.org) |
| **WONG** | *Roger Wong's Apocalypse FAQ* — vendored in this repo at [wongFaq.txt](../tools/extractors/docs/wongFaq.txt) |
| **CODE** | OpenApoc source and `data/common_patch/` game data, cited by `file:line` |

**CODE outranks everything else.** Where a guide describes vanilla behaviour that OpenApoc does not
reproduce, the code wins and the divergence is recorded in [§13](#13-corrections-to-the-source-guides).

## Evidence tags

Every mechanical claim carries one of:

- **[V]** *Verified* — read out of this repo's source or data, with a citation.
- **[D]** *Diverges* — a source guide says X; OpenApoc does Y.
- **[NI]** *Not implemented* — a source guide describes something absent from OpenApoc.
- **[S]** *Source-only* — from WONG/NKF/LIL/AOW, describing data that lives on the original game
  CD and is therefore not checkable from this repo alone.
- **[DERIVED]** *Computed* — the underlying mechanism is **[V]**, but the conclusion is worked out
  from it under stated assumptions rather than read directly. Treat as a strong prediction, not as
  ground truth.

## Scope and baseline

- **Difficulty 0 (Novice)** unless stated. Difficulty is a parameter throughout; the deltas are
  isolated in [§14](#14-difficulty-deltas). Difficulty 0 is what `tests/CMakeLists.txt` already
  wires up, so the guide and the harness share one baseline.
- Times are **in-game** time. Real-time-vs-turn-based tactical choice is discussed in
  [§12](#12-combat-doctrine); the strategic layer is identical either way.
- This plan assumes OpenApoc built with original game assets extracted.
- **Two trees, and the difference matters.** This document was first written against `master`.
  It has since been re-verified against (a) the original `UFO2P.EXE` / `TACP.EXE` via the Ghidra
  work in `OpenApoc-og-research`, and (b) the live development branch **`khallmark/FixShitUp`**,
  which carries `docs/original-game/` and a large body of EXE-accurate fixes. Where the three
  disagree, **the EXE is the authority**, the FixShitUp behaviour is what you will actually play,
  and `master`-only defects are called out as such in
  [§13](#13-corrections-to-the-source-guides) and [§15](#15-known-gaps-in-openapoc).
- **A campaign harness already exists** on that branch — `tools/oa_play.py` (~4 100 lines) and
  `tools/oa_victory.py`, driving the real UI over a localhost socket
  (`framework/harness.{cpp,h}`). This guide is a specification for that runner's policy, not a
  proposal to build one.
- **Companion document: [playing-the-game.md](playing-the-game.md)** — how to *drive* the harness,
  written from live runs. It owns operational reality (ground rules, named actions, crashes, what
  silently does nothing). This document owns the *rules of the game*: what the engine does on each
  tick boundary, what the recovered EXE tables say, and what the campaign calendar is. Where the
  two touch, `playing-the-game.md` wins on "what happens when you click", and this one wins on
  "what the original game's data says".

---

## Table of contents

1. [Canonical name glossary](#1-canonical-name-glossary)
2. [The clock — standing orders by tick boundary](#2-the-clock--standing-orders-by-tick-boundary)
3. [The campaign spine — the alien order of battle](#3-the-campaign-spine--the-alien-order-of-battle)
4. [The research critical path](#4-the-research-critical-path)
5. [Day 0 — Tuesday 7 March 2084](#5-day-0--tuesday-7-march-2084)
6. [Week 1 — Tue 7 Mar to Sun 12 Mar](#6-week-1--tue-7-mar-to-sun-12-mar)
7. [Weeks 2–4 — the Transporter window](#7-weeks-24--the-transporter-window)
8. [Weeks 5–8 — escalation and the manufacturing pivot](#8-weeks-58--escalation-and-the-manufacturing-pivot)
9. [Weeks 9–15 — the Overspawn era](#9-weeks-915--the-overspawn-era)
10. [The Alien Dimension — the ten-building sequence](#10-the-alien-dimension--the-ten-building-sequence)
11. [Economy — exact rules](#11-economy--exact-rules)
12. [Combat doctrine](#12-combat-doctrine)
13. [Corrections to the source guides](#13-corrections-to-the-source-guides)
14. [Difficulty deltas](#14-difficulty-deltas)
15. [Known gaps in OpenApoc](#15-known-gaps-in-openapoc)
16. [AI fidelity — and the "learning AI" question](#16-ai-fidelity--and-the-learning-ai-question)

---

## 1. Canonical name glossary

The three guides use overlapping names for distinct things. Fusing any of these pairs breaks the
tech chain. Resolve names against this table before acting on any advice.

| Canonical name | What it is | Not to be confused with |
|---|---|---|
| **Bio-Transport Module** — `RESEARCH_BIO-TRANSPORT_MODULE` (BioChem) | Vehicle equipment. Lets a craft carry live/dead aliens home. Market $950, 4 000 man-hours. | **Biotrans** |
| **Biotrans** — `RESEARCH_BIO-TRANSPORT` (Quantum Physics, order 20110) | Interdimensional troop-carrier *craft*. Market $34 000, 35 000 man-hours. Requires Dimension Probe + UFO Type 3. | **Bio-Transport Module** |
| | *A driver matching on the substring `BIO-TRANSPORT` hits the wrong topic. Match the full id.* | |
| **Dimension Probe** | Unmanned interdimensional *craft*; the gateway research for Advanced Workshop and Biotrans. | **Dimension Missile Launcher** |
| **Dimension Missile Launcher / Missile** | Vehicle weapon and its ammo. Unrelated to dimensional travel. | **Dimension Probe** |
| **Personal Disruptor Shield** | Agent-worn shield (alien loot). | **Small/Large Disruption Shield** |
| **Small / Large Disruption Shield** | *Vehicle* shield modules. | **Personal Disruptor Shield** |
| **Disruptor Armor** (X-COM Armor) | Manufactured agent armour, five pieces. | Either shield |
| **Advanced Biochemistry Lab** | Biochem facility. Unlocked by researching *any* alien or autopsy. | **Advanced Quantum Physics Lab**, **Advanced Workshop** |
| **Advanced Quantum Physics Lab** | Physics facility. Unlocked by *any one* of Alien Control System / Energy Source / Propulsion System. | the other two |
| **Advanced Workshop** | Engineering facility. Requires Dimension Probe. | the other two |
| **Alien Containment** | Base facility holding live specimens. **Present in the starting base** — you do not need to build one on day 1. | Bio-Transport Module |

**[V]** Starting facilities are fixed data: `FACILITYTYPE_VEHICLE_REPAIR_BAY ×1,
ALIEN_CONTAINMENT ×1, BIOCHEMISTRY_LAB ×1, LIVING_QUARTERS ×3, MEDICAL_BAY ×1, PSI-GYM ×1,
QUANTUM_PHYSICS_LAB ×1, STORES ×2, TRAINING_AREA ×1, WORKSHOP ×1`
— [data/common_patch/gamestate.xml:114](../data/common_patch/gamestate.xml#L112).

You start with a Training Area **and** a Psi-Gym. Training can begin at 12:00 on day one.

---

## 2. The clock — standing orders by tick boundary

OpenApoc runs the strategic layer on five nested boundaries
(`GameState::update`, [game/state/gamestate.cpp:1269](../game/state/gamestate.cpp#L1269)).
Everything a player or an automated agent must do hangs off one of them. This is the spine of the
whole plan: learn this table and the day-by-day sections below become mechanical.

`TICKS_PER_SECOND = 144` (selected observational 36-TPS compatibility base × 4 multiplier) —
[game/state/gametime.h:18](../game/state/gametime.h#L18).

### 2.1 Every second — `updateEndOfSecond`

*Source: [game/state/gamestate.cpp:1353](../game/state/gamestate.cpp#L1353)*

| Engine does | You do |
|---|---|
| Building cargo ticks; per-vehicle and per-agent per-second updates | Nothing scheduled. This is the air-combat resolution grain. |

### 2.2 Every five minutes — `updateEndOfFiveMinutes`

*Source: [game/state/gamestate.cpp:1375](../game/state/gamestate.cpp#L1375)*

| Engine does | You do |
|---|---|
| **[V]** Each non-taken-over org accumulates 5 min toward a takeover roll. A roll fires every **125 minutes** (`TICKS_PER_TAKEOVER_ATTEMPT`, [organisation.h:19](../game/state/shared/organisation.h#L19)) and succeeds when `rand(0,200) < infiltrationValue` ([organisation.cpp](../game/state/shared/organisation.cpp), `takeOver`). At most one org falls per 5-minute tick. | Treat **any** non-zero infiltration as an active loss risk — see [§2.7](#27-the-infiltration-rule). |
| **[V]** Vehicles parked at a player base reload/refuel one equipment item per tick | Park damaged/dry craft at a base and leave them ~1 hour, not "a moment". |
| **[V]** Buildings accumulate toward a detection attempt; at Novice one attempt per **70 minutes** per building (`TICKS_PER_DETECTION_ATTEMPT[0]`, [building.h:17](../game/state/city/building.h#L17)). A detected building is silent for 6 hours afterwards (`TICKS_DETECTION_TIMEOUT`). The loop **breaks after the first newly detected building**. | Do not wait for alerts. Detection is throttled and lossy; active patrolling ([§2.7](#27-the-infiltration-rule)) beats it. |

### 2.3 Every hour — `updateEndOfHour`

*Source: [game/state/gamestate.cpp:1458](../game/state/gamestate.cpp#L1458)*

Order matters; this is the exact sequence:

| # | Engine does | You do |
|---|---|---|
| 1 | **[V]** `Agent::updateHourly` — heal and train | See [§2.5](#25-healing-and-training--the-hourly-engine) |
| 2 | **[V]** `Lab::update` — every lab advances `man_hours_progress` by **`sum(skill of assigned agents)` per hour** ([research.cpp](../game/state/city/research.cpp), `Lab::update`) | Never leave a lab idle. An unassigned scientist is a pure loss of `skill` man-hours per hour. |
| 3 | **[V]** `City::hourlyLoop` → `repairVehicles`, then `updateInfiltration` → **for every building, `Building::alienGrowth`** ([city.cpp:557](../game/state/city/city.cpp#L557)) — aliens breed *and* try to spread to a random building within 15 tiles | **This is the clock that punishes you.** Every hour a drop site is left alive, it can seed a neighbour. |
| 4 | **[V]** `Organisation::updateInfiltration` — recompute each org's infiltration from the alien crew in its buildings, divide by `42 − difficulty`, then subtract 1 more on odd hours. Clamped `[0,200]`. Recovered from `UFO2P` `FUN_0007fcc0` @ VA `0x7FCC0` / file `0xD2364`. | Natural decay is **−12/day at every difficulty**. Infiltration you allow today is still killing you tomorrow. |

**Alien spread, precisely** ([building.cpp](../game/state/city/building.cpp), `alienMovement`): pick up to 15
buildings whose centres are within 15 tiles; choose one at random; compute how many aliens want to
move (`movementPercent + rand(0,30)` each); then move them all with probability
`15 + 3 × movingCount (+20 if the target's owner is Friendly/Allied to the aliens)`.
A site with a dozen aliens is effectively certain to seed a neighbour within the hour.

### 2.4 Every day, 00:00 — `updateEndOfDay`

*Source: [game/state/gamestate.cpp:1478](../game/state/gamestate.cpp#L1478)*

| Engine does | You do |
|---|---|
| **[V]** Every facility under construction: `buildTime--`. Reaches 0 → complete. `buildTime` is in **days**. | Start construction as early in a day as possible; the first decrement lands at the next midnight regardless. |
| **[V]** `updateVehicleAgentPark`, `updateHirableAgents` — the recruitment pool is regenerated per organisation from `hirableAgentTypes` min/max daily growth ([organisations.xml](../data/common_patch/gamestate/organisations.xml)). A listed hiree has a **33 %** chance of being gone by the end of the day (`CHANGE_HIREE_GONE`, [organisation.h:22](../game/state/shared/organisation.h#L22)). | **Check the recruitment pool every day just after 00:00.** A candidate you defer has a 1-in-3 chance of vanishing. |
| **[V]** `updateDailyInfiltrationHistory` — pushes today's infiltration onto each org's history | This is what the infiltration graph plots. It samples **once a day**; the graph lags reality by up to 24 h. |
| **[V]** `updateRelations`; orgs with `initiatesDiplomacy` may raise a bribe demand | Read the relations delta daily; a −15 swing is the bribe trigger. |
| **[V]** `Agent::updateDaily` → `recentlyFought = false` | **This is the real midnight mechanic.** An agent who fought today cannot heal at all until midnight clears the flag. |
| **[V]** `City::dailyLoop` → scenery repair, workforce recalculation | Collateral damage you caused is repaired at the owner's expense — see [§11.5](#115-city-damage). |

### 2.5 Healing and training — the hourly engine

**[V]** `Agent::updateHourly` ([agent.cpp](../game/state/shared/agent.cpp)):

- The agent must be **at their home base** — either standing in it, or aboard a vehicle parked in it.
- **Healing**: `healingProgress += 80 / max(100, medicalUsage)`; +1 HP per whole point.
  At or under Medical capacity that is **0.8 HP/hour = 19.2 HP/day**. Blocked entirely while
  `recentlyFought` is set.
- **Training**: `trainPhysical/trainPsi` are fed `TICKS_PER_HOUR × 100 / max(100, usage)` ticks;
  a training *roll* costs `4 × TICKS_PER_HOUR`. At or under capacity that is **6 rolls per day**.
- **Over-capacity is a straight divisor.** 200 % Training usage halves training speed. Under-capacity
  gives no bonus.

**[V]** Each physical roll independently tries each stat with
`if (rand(0,100) >= stat) stat++` — so **the improvement chance is `(100 − stat) %`**.
Stamina uses `rand(0,2000) >= stamina → +20`.

The consequence is the single most important recruiting fact in the game:

> **Low-stat agents improve far faster than high-stat ones.** A 20-Accuracy rookie gains at 80 %
> per roll; an 80-Accuracy veteran gains at 20 %.

Integrating `dStat/dt = 6 × (100 − stat)/100` per day:

| From → to | Days of continuous training |
|---|---|
| 40 → 60 | ≈ 7.6 |
| 40 → 80 | ≈ 18.3 |
| 40 → 90 | ≈ 29.9 |
| 60 → 90 | ≈ 23.1 |

**Standing order: every agent not in the field or in transit is set to Physical training, from
12:00 on day one, forever.** This is worth more than any purchase you can make in week 1.

**Combat is the other stat engine, and it is not optional.**
`BattleUnit::processExperience` ([battleunit.cpp:4171](../game/state/battle/battleunit.cpp#L4171))
runs after every battle and awards stats from what the agent actually *did*:

| Earned by | Grants |
|---|---|
| Landing a hit | +1 Accuracy XP |
| Reaction fire | +1 Reactions XP |
| Psi use | +1 (or +2) Psi Attack / Psi Energy XP |
| Being under fire and holding | +1 Bravery XP |

Accuracy, Reactions, Psi and Bravery each roll independently against their own XP. Health, Speed,
Stamina and Strength get a **percentile** boost — `(100 − stat) / 10`, scaled by the agent type's
improvement rate — but only once total secondary XP exceeds `100 / improvementPercentagePhysical`.
Psi is capped at 3 × the agent's initial value, exactly as in the gym; everything else caps at 100.

So a front-line agent improves on **two** tracks at once, and the day-count table above is a floor,
not a forecast — it describes an agent who only ever trains. **Rotate agents through missions;
do not build a squad of gym-only reservists.**

**Improvement rate is a property of the race**
([agent_types.xml](../data/common_patch/gamestate/agent_types.xml)):

| Agent type | Physical | Psi | Can use Training Area / Psi Gym |
|---|---|---|---|
| Human | 100 % | 100 % | **yes** |
| Hybrid | 100 % | 100 % | **yes** |
| Android | **10 %** | **0 %** | **no** |

Two corrections fall out of this. **[D]** AOW says androids "cannot improve their stats" — they
cannot *train* at all, but they do still gain from combat at one-tenth rate, so a veteran android
is meaningfully better than a fresh one. And **hybrids have exactly the same improvement rates as
humans**: their psi advantage comes entirely from *starting* psi stats interacting with the
3 × initial cap, not from a better learning rate.

Psi training is different — `trainPsi` caps at **3 × the agent's initial** psi stats and gains
`psi_energy / 20` per success, so it rewards agents who *start* with high psi (hybrids), the
opposite of physical training.

### 2.6 Every week, Monday 00:00 — `updateEndOfWeek`

*Source: [game/state/gamestate.cpp:1649](../game/state/gamestate.cpp#L1649)*

**[V]** The game begins **Tuesday 7 March 2084** (`GAME_START`, [gametime.cpp:16](../game/state/gametime.cpp#L16))
at 12:00 (`GameTime::midday()`), and `addTicks` rolls the week when `days % 7 == 6` — i.e. at
**midnight entering Monday**. Week 1 is therefore a short week: Tue 12:00 → Sun 23:59.

Order of operations at the roll:

1. `updateOrgFinances` — every org books income; government takes 10 % of civilian wages.
2. `updateUfoGrowth` — **the alien fleet is reinforced**. See [§3](#3-the-campaign-spine--the-alien-order-of-battle).
3. `updateItemMarket` — every market price and stock level is re-rolled. See [§11.1](#111-the-market).
4. `weeklyPlayerUpdate` — government income paid, then **salaries and base upkeep deducted**.
5. `City::weeklyLoop` → `generatePortals` — **the dimension gates move**.

**[V]** Weekly overheads at game start: 10 Soldiers × $600 + 15 technical staff × $800 =
**$18 000/week in salary alone**, before facility upkeep
([gamestate.xml:58](../data/common_patch/gamestate.xml#L58)).

**Sunday 23:59 is your last chance** to fire staff before the weekly payroll, and to buy stock
before the market re-rolls.

### 2.7 The infiltration rule

Put together, §2.2–§2.4 give one non-negotiable operating rule:

- Aliens breed and spread **hourly**.
- Infiltration decays at only **−12/day** at Novice.
- Every org rolls for permanent alien takeover **every 125 minutes**, with probability
  `infiltrationValue / 200`.

Takeover is **probabilistic, not a threshold** — there is no "safe below half" line. The correct
target is **zero**, and the correct response time to a confirmed drop site is measured in game
*hours*, not days.

> Sizing note: a naive "infiltration 20 ⇒ ~70 %/day" calculation assumes infiltration holds steady
> across the day, which the hourly decay contradicts. Use the per-roll rule
> (`P = infiltrationValue / 200` every 125 minutes) against the actual decay curve rather than a
> fixed daily figure.

---

## 3. The campaign spine — the alien order of battle

Everything else in this plan is subordinate to this section. Money, training and base building are
recoverable. **Missing a UFO type is not.**

### 3.1 UFO type numbering

***[V]***

From [data/common_patch/gamestate/research.xml:625](../data/common_patch/gamestate/research.xml#L616):

| Type | Ship | Type | Ship |
|---|---|---|---|
| 1 | Probe | 6 | **Assault Ship** |
| 2 | Scout | 7 | Bomber |
| 3 | **Transporter** | 8 | Escort |
| 4 | Fast Attack Ship | 9 | Battleship |
| 5 | **Destroyer** | 10 | Mothership |

Note types 5 and 6: **Destroyer is 5, Assault Ship is 6.** Community write-ups frequently swap
these, which mis-dates the Explorer and Retaliator deadlines.

### 3.2 Weekly fleet reinforcement

***[V]***

`updateUfoGrowth` ([gamestate.cpp:1536](../game/state/gamestate.cpp#L1536)) reads
`UFO_GROWTH_<week>`, falling back to `UFO_GROWTH_DEFAULT`, and spawns into `CITYMAP_ALIEN`
up to `UFO_GROWTH_LIMIT` minus the fleet already sitting there.

Recovered from `UFO2P` `UFO_growth_rates` @ file `0x155010` / VA `0x128C10` (10 × uint16 caps +
15 weekly rows + DEFAULT footer), extracted by `tools/extractors/common/ufogrowth.h`; reference copy
at [original-game/exe-tables/ufo_growth_lists.xml](original-game/exe-tables/ufo_growth_lists.xml):

| Week | Reinforcements added |
|---|---|
| 1 | Probe ×9, Scout ×9 |
| 2 | **Transporter ×3**, Scout ×4, Probe ×2, Fast Attack ×2 |
| 3 | **Transporter ×2**, Assault ×3, Scout ×4, Fast Attack ×2, Probe ×2 |
| 4 | **Transporter ×1**, Destroyer ×2, Assault ×2, Scout ×2, Fast Attack ×1, Probe ×1 |
| 5 | Bomber ×2, Assault ×1, Destroyer ×1, Fast Attack ×1, Scout ×1 |
| 6 | Bomber ×1, Destroyer ×1, Escort ×1, Fast Attack ×1, Scout ×1 |
| 7 | **Battleship ×1**, Bomber ×1, Destroyer ×1, Escort ×1, Scout ×1 |
| 8 | **Mothership ×1**, Assault ×1, Bomber ×1, Escort ×1, Scout ×1 |
| 9 | Battleship ×1, Escort ×1 |
| 10 | Bomber ×1, Scout ×1 |
| 11 | Battleship ×1, Mothership ×1, Assault ×1, Fast Attack ×1 |
| 12 | Bomber ×1, Escort ×1 |
| 13 | Battleship ×1, Fast Attack ×1, Scout ×1 |
| 14 | Mothership ×1, Escort ×1 |
| 15 | Battleship ×1 |
| 16+ | *DEFAULT*: Battleship ×1, Bomber ×1, Escort ×1, Mothership ×1 |

Per-type standing caps (`UFO_GROWTH_LIMIT`): Probe 15, Scout 15, Assault 6, Bomber 6, Destroyer 6,
Escort 6, Fast Attack 6, Transporter 6, Battleship 4, Mothership 2.

**Last spawn week by type:** Probe wk 4 · **Transporter wk 4** · Assault wk 11 · Destroyer wk 6 ·
Fast Attack wk 13 · Scout wk 13 · everything else recurs forever via DEFAULT.

### 3.3 What the aliens will *do* each week

***[V]***

`invasion()` ([gamestate.cpp:888](../game/state/gamestate.cpp#L888)) rolls a mission type from
`UFO_MISSION_PREFERENCE_<week>` (falling back to DEFAULT), then picks the **lowest-numbered
priority** incursion package the standing fleet can afford.

Recovered from `UFO2P` `UFO_mission_patterns` @ file `0x155164` (20 × 10 uint16 — 19 weeks
plus DEFAULT; IDs 3 = Infiltration, 1 = Attack, 2 = Subversion, 5 = Overspawn), reference copy at
[original-game/exe-tables/ufo_mission_preference.xml](original-game/exe-tables/ufo_mission_preference.xml):

| Weeks | Infiltration | Attack | Subversion | Overspawn |
|---|---|---|---|---|
| 1–3 | 100 % | — | — | — |
| 4 | 90 % | 10 % | — | — |
| 5–6 | 80 % | 10 % | 10 % | — |
| 7 | 70 % | 10 % | 10 % | 10 % |
| 8 | 50 % | 10 % | 20 % | 20 % |
| 9–11 | 40 % | 20 % | 20 % | 20 % |
| 12 | 30 % | 20 % | 30 % | 20 % |
| 13 | 30 % | **30 %** | 20 % | 20 % |
| 14 | **40 %** | 20 % | 20 % | 20 % |
| 15–18 | 30 % | 20 % | 20 % | 30 % |
| 19 | 20 % | 30 % | 20 % | 30 % |
| 20+ | 10 % | 20 % | 40 % | 30 % |

Week 13 is an **Attack** peak, not a general spike, and week 14 is the *calmest* week after week 8.
The escalation is smooth throughout — there is no cliff.

> **Correction.** An earlier version of this guide reported week 13 as a savage spike
> (40 % Subversion / 30 % Overspawn) because `master`'s shipped
> `data/common_patch/gamestate/ufo_mission_preference.xml` **omits week 13 entirely**, so
> `invasion()` falls through to the endgame DEFAULT row. That is a `master` data defect, not the
> original game. The same file also gets **week 14 wrong** (Infiltration 30 / Overspawn 30 instead
> of 40 / 20). Both are fixed on `FixShitUp`, which extracts the table from the EXE
> (`extract_ufo_mission_preference.cpp`) instead of shipping a hand-copied patch.

### 3.4 Incursion cadence

***[V]*** — recovered from `UFO2P` `FUN_0006d384` @ VA `0x6D384` / file `0xCFA28`, with the initial
write at `FUN_000ad148` @ file `0xAD231`:

```
delay = 0x2F7600 + rand[0..0xB04] * 0x870 + rand[0..0xE10] * 0x24
coefficient ratio = 86400 : 60 : 1
```

The formula and coefficient ratio are verified. The wall-clock translation below uses the
repository/community-observed 36-TPS compatibility canon; the coefficients alone do not bind
absolute cadence (18 TPS preserves the ratio and doubles every duration). Under the selected
36-TPS interpretation, the delay is `24 h + rand[0..2820] minutes + rand[0..3600] seconds`, or
24–72 hours.

- **The same formula schedules the first incursion and every one after it.** There is no special
  opening wave.
- Under the selected 36-TPS interpretation, the first incursion is **between Wed 8 March 12:00 and
  Fri 10 March 12:00** — 24 to 72 hours after the 12:00 start.
- Under that same interpretation, there is **one wave every 1–3 days.**

> **Correction.** Earlier versions of this guide said the first wave lands at 22:00–24:00 on day 0,
> and that the cadence is 1–4 days. Both came from `master`, which invents a `10 h + rand(0…2 h)`
> opening delay and uses `24 h + rand(0…72 h)` (= 24–96 h) thereafter under the same selected
> timing interpretation. `FixShitUp` implements the EXE formula for both
> ([gametime.h:49](../game/state/gametime.h#L49)).
> **Day 0 has no UFO wave at all** — see [§5](#5-day-0--tuesday-7-march-2084).

### 3.5 The real Transporter deadline

*Mechanism **[V]**; conclusion **[DERIVED]** — see the assumption below.*

This is the most important derived result in the document, and none of the three guides states it.

Incursion packages are keyed by priority in a `std::map<int, …>`; the loop takes the **first**
(lowest-numbered, i.e. *strongest*) package the fleet can afford, and breaks. Transporters appear
only in the weak Infiltration packages, priorities 12 and 15–18
([original-game/exe-tables/ufo_incursions.xml](original-game/exe-tables/ufo_incursions.xml)):

| Priority | Composition |
|---|---|
| 11 | Assault ×2, escort Fast Attack ×3 |
| **12** | **Transporter ×1** + Assault ×1, escort Scout ×3 |
| **15** | **Transporter ×1** + Destroyer ×1, escort Fast Attack ×2 |
| **16** | **Transporter ×2**, escort Fast Attack ×2 |
| **17** | **Transporter ×1**, escort Fast Attack ×2 |
| **18** | **Transporter ×1** + Scout ×2 |
| 19–20 | Scout/Probe only |

The *mechanism* above is read straight from the code and data. The *conclusion* below is computed
from it by walking the fleet forward under an explicit idealisation — **no UFO losses, and the
weekly growth lists applied in full**. A real campaign shooting things down will differ; that is
precisely the lever this section is about. Treat the week numbers as a strong prediction to be
confirmed by the harness's §3.5 oracle, not as a measured fact.

Walking the fleet forward with **no losses**:

- **Week 1** — fleet is Probe ×9, Scout ×9. Best affordable Infiltration package is priority 19.
  **No Transporter can appear in week 1.**
- **Week 2** — Transporter ×3 and Fast Attack ×2 arrive. Best affordable becomes **priority 16
  (Transporter ×2)**. Transporters fly.
- **Week 3** — Assault ×3 arrive. **Priority 11 (Assault ×2) is now affordable and outranks every
  Transporter package.** Transporters stop being sent.

> **Therefore (DERIVED): week 2 is the Transporter window, and you keep it open into weeks 3–4 only
> by destroying Assault Ships and Destroyers as fast as they arrive.** Every Assault Ship you leave
> alive is a lock on the package the aliens will choose next.

This is the precise mechanism behind WONG §2.2.6 ("I failed to research UFO type XYZ and now I'm
stuck") — and it reframes air combat. Interception is not just loot and score; **it holds the alien
order of battle down at a tier that still sends you the ships you need to capture.**

Corollary, and a direct contradiction of AOW's "just let them run through the city": passive play
does not merely cost you loot. It permanently escalates the incursion tier and can render the
campaign unwinnable.

### 3.6 UFO recovery and what it unlocks

***[V]***

Recovering *any* of the ten UFO types force-completes **four** unlocks
([tools/extractors/extract_vehicles.cpp:149](../tools/extractors/extract_vehicles.cpp#L149)):

- `RESEARCH_UNLOCK_ALIEN_CRAFT_CONTROL_SYSTEMS`
- `RESEARCH_UNLOCK_ALIEN_CRAFT_ENERGY_SOURCE`
- `RESEARCH_UNLOCK_ALIEN_CRAFT_PROPULSION`
- `RESEARCH_UNLOCK_UFO_TYPE_<n>`

**[D]** LIL and AOW both claim UFO Type 3 specifically gates the three alien craft systems. It does
not — **a week-1 Probe unlocks all three.** Type 3 is still hard-required, but as an input to
Bio-Transport (the craft), not to the systems. Practical effect: **get the Advanced Quantum Physics
Lab started in week 1**, not week 2.

Two recovery paths, both granting the unlocks:

- Unmanned UFO (no `battle_map`) — granted on recovery
  ([cityview.cpp:4622](../game/ui/tileview/cityview.cpp#L4622)).
- Crewed UFO — granted when you **win the crash-site battle**
  ([battle.cpp:3581](../game/state/battle/battle.cpp#L3581)).

A crewed UFO you shoot down but never board unlocks nothing. **[V]** Only **one** transport may
enter a crash site, so keep a full-capacity carrier on standby.

---

### 3.7 The Organic Factory shuts the tap off

***[V]*** — and this is the single largest strategic lever no source guide mentions.

`updateUfoGrowth` returns immediately unless `UFOGrowth::craftFactoryIntact` is true
([gamestate.cpp:1545](../game/state/gamestate.cpp#L1545),
[ufogrowth.cpp:51](../game/state/rules/city/ufogrowth.cpp#L51)). That predicate walks
`CITYMAP_ALIEN` looking for a **living `BUILDINGFUNCTION_ORGANIC_FACTORY`**.

The Organic Factory is **alien-dimension building #7** ([§10.2](#102-the-sequence)). Its Ufopaedia
entry is the giveaway: it "produces strange organic mushrooms which grow into Alien craft."

> **Destroy the Organic Factory and the weekly UFO reinforcement stops permanently.** No more
> Battleships, no more Motherships, no more DEFAULT-list top-ups. The alien fleet becomes a fixed,
> shrinking pool that you can grind down to nothing.

This reframes the whole back half of the campaign. The nine post-entry raids are not a uniform
grind — **#7 is the one that ends the war of attrition**, and everything after it is cleanup
against a fleet that can no longer replace its losses. If you are struggling to hold the city while
working through the alien dimension, the answer is to push *harder* to reach #7, not to fall back
and defend.

Note the fail-open branch: if no Organic Factory exists in the state at all, `craftFactoryIntact`
returns `true`. The gate only bites once the building exists and has been destroyed.

---

## 4. The research critical path

### 4.1 Rates

***[V]***

`Lab::update` advances a project by **`sum(skill of assigned agents)` man-hours per game hour**
(`ticks_per_progress_hour = TICKS_PER_HOUR / totalSkill`). `Lab::getTotalSkill` **skips any agent
not currently at their home base** — a scientist in transit contributes nothing.

Worked examples at 10 staff × 90 skill = 900 man-hours/hour:

| Project | Man-hours | Elapsed |
|---|---|---|
| Bio-Transport Module (mfr) | 4 000 | 4.4 h |
| Toxigun (mfr) | 3 000 | 3.3 h |
| Biotrans craft (mfr) | 35 000 | ~1.6 days |
| Annihilator (mfr) | 100 000 | ~4.6 days |
| Alien Building 0 research | 38 000 | ~1.8 days |

Because scientist skill is **static** (unlike agent stats, [§2.5](#25-healing-and-training--the-hourly-engine)),
hiring high-skill technical staff is straightforwardly correct — this is where LIL's and AOW's
"hire 90+" advice is right, and where applying the same rule to *agents* is wrong.

### 4.2 Quantum Physics tree

***[S/WONG §4.1]** cross-checked against repo data*

**Free on recovery / capture:** all UFO types · Alien Control System · Alien Energy Source ·
Alien Propulsion System · Light Disruptor Beam · Disruptor Gun · Boomeroid · Vortex Mine ·
Personal Shield · Personal Cloaking Field · Personal Teleporter · Dimension Launcher (+ Missile).

**Derived:**

```
Alien {Energy Source | Control System | Propulsion System}  →  Advanced Quantum Physics Lab   [V]
Alien Energy Source + Control System + Propulsion System    →  Dimension Probe
Dimension Probe                                             →  Advanced Workshop
Dimension Probe + UFO Type 3  (Transporter)                 →  Biotrans
Biotrans        + UFO Type 5  (Destroyer)                   →  Explorer
Explorer        + UFO Type 6  (Assault Ship)                →  Retaliator
Retaliator      + UFO Type 9  (Battleship)                  →  Annihilator

Light Disruptor Beam → Medium Disruptor Beam → Heavy Disruptor Beam → Large Disruption Shield
Medium Disruptor Beam → Small Disruption Shield
Large Disruption Shield → Cloaking Field
Light Disruptor Beam → {Disruptor Bomb, Stasis Bomb, Teleporter, Disruptor Multi-Bomb}
Disruptor Gun → Devastator Cannon
Disruptor Bomb → Advanced Control System
Disruptor Gun + Small Disruption Shield + Personal Shield → X-COM (Disruptor) Armor
```

**[V]** `RESEARCH_ADVANCED_QUANTUM_PHYSICS_LAB` depends on **Any** of the three alien systems
([research.xml](../data/common_patch/gamestate/research.xml)) — one recovered UFO is enough.

**Note [D]:** AOW warns "the disruptor is necessary before the ship shields can be researched."
That is the *Medium Disruptor Beam → Small Disruption Shield* edge above, not the Disruptor Gun.

### 4.3 Biochemistry tree

***[S/WONG §4.2]** cross-checked*

**Free on capture:** any alien (live or corpse) · Brainsucker Launcher · Entropy Launcher/Pod.

```
any alien or autopsy                     → Advanced Biochemistry Lab            [V]
Multiworm Egg / Multiworm / Hyperworm
  (research + autopsy)                   → The Alien Genetic Structure
The Alien Genetic Structure              → Biological Warfare
Biological Warfare                       → Toxigun + Toxin Type A               [V]
Multiworm + Hyperworm + Chrysalis
  (research + autopsy)                   → The Alien Life Cycle                 [V]
Biological Warfare + The Alien Life Cycle→ Toxin Type B
Toxin B + The Real Alien Threat          → Toxin Type C
Toxin C + Queenspawn (+ autopsy)         → Alien Gas
```

**[V]** In OpenApoc's data, `RESEARCH_ADVANCED_BIOCHEMISTRY_LAB` takes **Any** of a 26-entry list
covering every alien and every autopsy — so the very first corpse you bring home unlocks it.
LIL's "Brainsucker Autopsy is the easiest way" is true but incidental; take whichever you get first.

**[V]** `THE_ALIEN_LIFE_CYCLE` requires **All** of Multiworm, Multiworm Autopsy, Hyperworm,
Hyperworm Autopsy, Chrysalis, Chrysalis Autopsy. `THE_REAL_ALIEN_THREAT` requires **All** of
thirteen species plus their autopsies — it is a long-campaign project, and **Toxin B is sufficient
to win**; Toxin C is optional.

### 4.4 Priority order

1. **Bio-Transport Module** (biochem) — day 0. Without it you bring home no specimens and the whole
   biochem tree stays shut.
2. **Alien craft systems** (physics) — as soon as any UFO is recovered. Unlocks Advanced Quantum Lab.
3. **Advanced Biochemistry Lab** — as soon as any alien is researched. **8 days to build [S/LIL]**;
   start it the day it appears.
4. **Advanced Quantum Physics Lab** — usually needs a second base for the space.
5. **Dimension Probe** → **Advanced Workshop** → **Biotrans**.
6. **Biological Warfare** → Toxigun + Toxin A, then Toxin B.
7. **Medium → Small Disruption Shield**, then **X-COM Armor**.
8. **Explorer → Retaliator → Annihilator** as the matching UFO types come in.

**Do not build a Dimension Probe.** **[S/AOW]** It is the research gateway to the Advanced Workshop,
but the craft itself is a poor product (profit/hour 0.104) and the Biotrans supersedes it.

---

## 5. Day 0 — Tuesday 7 March 2084

The clock starts at **12:00**. Two things are already true before you touch anything:

- **[V]** One building has been seeded with `initialInfiltration = true`
  ([gamestate.cpp:696](../game/state/gamestate.cpp#L696)). Your first mission site already exists.
- **[V]** `updateEndOfWeek(true)` has already run: week-1 market stock is set and the alien fleet
  has been seeded with Probe ×9 + Scout ×9.

**[V]** `firstDetection = true` makes the first detection roll *forced*
([building.cpp](../game/state/city/building.cpp), `updateDetection`), so the seeded site is guaranteed
to be found on its first attempt — **~70 minutes in, at roughly 13:10.**

| Time | Action |
|---|---|
| **12:00** | **Set every agent to Physical training.** Free, compounding, and the largest single lever in the game ([§2.5](#25-healing-and-training--the-hourly-engine)). |
| 12:00 | Assign **all 5 biochemists** to the Biochemistry Lab → research **Bio-Transport Module**. |
| 12:00 | Assign **all 5 physicists** to the Quantum Physics Lab. Nothing important is available yet — take Dimension Gates so the lab is not idle. **[D]** LIL calls this "of no game-play consequence"; it is, because an idle lab wastes `skill` man-hours every hour. |
| 12:00 | Engineers have nothing to build. Leave them assigned; they will start the moment Bio-Transport Module lands. |
| 12:05 | **Liquidate the ground fleet.** Sell the Wolfhound APC and the Stormdog, and their ammunition. Ground vehicles cannot reach crash sites and die instantly if the road under them is destroyed **[S/WONG §2.3.5, AOW]**. |
| 12:05 | **[S/AOW]** Run the *Stormdog chop shop*: buy the available Stormdogs, strip engine and weapon, sell the parts **and** the hulls. Verify the margin in-session before repeating — it is a data-driven quirk, not a guarantee. |
| 12:10 | Buy **hoverbikes** with the proceeds and arm them with Janitor missiles **[S/WONG §2.2.2, AOW]**. Sell the auto-cannons and their 300 rounds that ship with each bike. |
| 12:10 | Queue base construction: a second **Biochemistry Lab**, a second **Quantum Physics Lab**, **Living Quarters**. You already have Alien Containment, a Training Area and a Psi-Gym — do not rebuild them ([§1](#1-canonical-name-glossary)). |
| 12:15 | Equip the strike team: full Megapol armour, Laser Sniper Gun, Lawpistol, **Stun Grapple** (unlimited ammo), medi-kit, 2 AP and 2 stun grenades. Watch the encumbrance bar; strength gates the load. |
| ~13:10 | **First alert.** Fly the Valkyrie to the site and **Investigate** — never *Raid*, which turns the building's owner hostile. |
| 13:10+ | First battle: Anthropods with Brainsucker Launchers, plus Brainsuckers. No lethal alien ranged weapons. Kill the Brainsuckers first; a successful brainsuck permanently removes the agent. |
| after | You have no Bio-Transport Module yet, so no specimens come home. Kill freely; the artefacts still return via the Cargo Module. |
| hourly | Check the drop site's neighbours. Growth and spread run **every hour** ([§2.3](#23-every-hour--updateendofhour)). |
| *(no UFO wave today)* | The first incursion is scheduled 24–72 h out — **Wed 8 Mar 12:00 at the earliest** ([§3.4](#34-incursion-cadence)). Day 0 is entirely yours: spend it on training, research and the first ground mission, not waiting at the sky. |
| 23:55 | Sweep: every agent who is not injured is training; every lab has a project; all injured agents are at a base with a Medical Bay so healing resumes when `recentlyFought` clears at 00:00. |
| Wed 8 – Fri 10 | **First UFO incursion lands somewhere in this window.** Probe/Scout only, unmanned, trivially killed. Recover at least one — it unlocks all three alien craft systems ([§3.6](#36-ufo-recovery-and-what-it-unlocks)). |

### Beginner traps, from the source guides

- **[NKF]** Do **not** unload agents into a building before investigating. Highlighting them is
  enough. Agents left behind after a mission are stranded — fatal in the alien dimension, where the
  building and its landing pad are destroyed on victory.
- **[NKF]** Craft fly themselves. Do **not** carry agents on fighters. Keep troops on a designated
  transport only.
- **[NKF]** *Investigate* vs *Raid*: Raid starts a fight with the building's security and inflicts a
  severe relations penalty.

---

## 6. Week 1 — Tue 7 Mar to Sun 12 Mar

Week 1 runs **Tue 12:00 → Sun 23:59** (5½ days). The week rolls at **Mon 13 Mar 00:00**.

**Threat profile:** 100 % Infiltration; Probes and Scouts only; **no Transporters possible**
([§3.5](#35-the-real-transporter-deadline)). This is the cheapest week in the campaign — spend it
on compounding assets, not on hoarding cash.

### Daily standing orders (every day, all campaign)

| When | Do |
|---|---|
| **00:00–00:05** | Review the recruitment pool. Hire technical staff at 90+ skill if you have Living Quarters space; hire agents on **availability, not stats** ([§2.5](#25-healing-and-training--the-hourly-engine)). A deferred candidate has a 33 % chance of vanishing. |
| 00:05 | Confirm every lab has a project and every idle agent is training. |
| 00:05 | Confirm injured agents are at a base with Medical capacity — `recentlyFought` has just cleared and healing resumes. |
| Hourly | Check the infiltration graph and the city map for new alien activity. |
| On drop | Respond **within the hour**. Mark neighbouring buildings; park a spare hoverbike on a site you intend to revisit **[NKF]**. |
| On alert | Investigate. Never Raid. |
| 23:55 | End-of-day sweep as above. |

### Week 1 milestones

| Day | Target |
|---|---|
| Tue 7 (D0) | Bio-Transport Module research started; ground vehicles sold; hoverbikes bought; first mission cleared; first UFO recovered. |
| Wed 8 | **Bio-Transport Module researched.** Engineers manufacture one (4 000 man-hours ≈ 4.4 h at 900 skill); fit it to the Valkyrie. Physicists start **Alien Energy Source / Control System / Propulsion System** from the recovered UFO. |
| Wed 8 | First **live capture** with Stun Grapples. Any alien or autopsy unlocks the Advanced Biochemistry Lab. |
| Thu 9 | **Advanced Biochemistry Lab available → build it immediately.** ~8 days of construction **[S/LIL]** means starting Thursday lands it around 17 March. |
| Thu–Fri | Second Biochem Lab and second Quantum Physics Lab complete. Staff them. |
| Fri 10 | With one alien system researched, the **Advanced Quantum Physics Lab** is available. Base space is usually the constraint — plan the second base now. |
| Sat 11 | Multiworm capture becomes the biochem priority (Genetic Structure → Biological Warfare → Toxigun). **[NKF]** Multiworms are hard to stun; wear them down with conventional fire first. |
| **Sun 12, 23:00** | **Weekly close-out.** See below. |
| **Sun 12, 23:59** | Fire dead weight *before* payroll. Rehire after 00:00 Monday and the week is effectively free **[S/AOW]**. |

### The Sunday 23:00 close-out (every week)

1. **Buy what you need before the market re-rolls.** Especially conventional ammunition; supply is
   finite and prices only move against you on X-COM-made goods ([§11.1](#111-the-market)).
2. **Fire under-performing technical staff** (skill is static — they will never improve).
   **Do not fire agents for low stats** — they improve fastest.
3. Queue facility construction — `buildTime` decrements at midnight either way.
4. Confirm all craft are repaired, rearmed and dispersed ([§12.1](#121-air-combat)).

**[V]** Government income is paid, then salary and base upkeep are deducted, at the Monday roll —
in that order — so a negative balance on Sunday night is not automatically fatal.

---

## 7. Weeks 2–4 — the Transporter window

**This is the part of the campaign you cannot redo.**

### Week 2 (Mon 13 Mar – Sun 19 Mar) — the window opens

**[V]** Transporter ×3 join the alien fleet. Priority 16 (Transporter ×2, Fast Attack escort ×2)
becomes the aliens' best affordable Infiltration package.

| Priority | Action |
|---|---|
| **1** | **Recover a Transporter (UFO Type 3).** Shoot it down and *board the crash site* — a crewed UFO you never enter unlocks nothing ([§3.6](#36-ufo-recovery-and-what-it-unlocks)). Keep a full-capacity transport on standby at all times this week; only one transport may enter a crash site. |
| **2** | **Kill every Assault Ship and Destroyer on sight.** Each one that survives raises the incursion tier and pushes Transporters out of the rotation from week 3 ([§3.5](#35-the-real-transporter-deadline)). |
| 3 | Advanced Biochemistry Lab under construction. |
| 4 | Buy a **second base** **[S/AOW]**. You will need the floor space for the Advanced Quantum Physics Lab, and a second launch point widens city coverage. |
| 5 | Multiworm → Genetic Structure → Biological Warfare. |

**Air doctrine this week [S/WONG §2.3.1, AOW]:** scramble everything at once and focus fire one UFO
at a time, nearest first. Kill the troop carriers before the escorts. Fly squadrons at different
altitudes — four craft per tile, one per altitude band — to avoid friendly fire.

### Weeks 3–4 — holding the window open

Assault Ships (wk 3) and Destroyers (wk 4) arrive. **Every one you leave alive locks the aliens
into a stronger package and shortens your Transporter window.** Aggressive interception is now a
tech-tree action, not an economic one.

| Week | Also do |
|---|---|
| 3 | **Advanced Quantum Physics Lab** built and staffed. Dimension Probe research begins. Toxigun manufacture begins the moment Biological Warfare completes. |
| 4 | **Last week Transporters spawn at all.** If you have not recovered one, this is your final chance — prioritise it over everything, including score. Mission preference adds Attack (10 %): the aliens will start destroying buildings. |
| 4 | Destroyers appear — **UFO Type 5**, needed for the Explorer. Begin collecting. |

### If you miss the Transporter

**[S/WONG §2.2.6]** There is no in-game recovery. Six Transporters exist at cap and none respawn
after week 4. If they are all destroyed-without-boarding or never deployed, Biotrans is unreachable,
which means no interdimensional craft, which means the alien dimension is unreachable and the
campaign is unwinnable. **Restart.** Take a hard save at the end of week 2 and name it something you
will recognise **[S/LIL]**.

---

## 8. Weeks 5–8 — escalation and the manufacturing pivot

**Threat:** Subversion enters at week 5, **Overspawn at week 7**, and by week 8 Infiltration is only
half the roll. Battleships arrive week 7, Motherships week 8.

| Week | Focus |
|---|---|
| 5 | Toxigun + Toxin A in production. Bombers arrive. Hoverbikes start dying in numbers — treat them as consumables. |
| 6 | **Last Destroyer spawn week.** Secure UFO Type 5 if you have not. Escorts arrive. |
| 7 | **First Battleship (UFO Type 9)** and **first Overspawn risk.** Buy or build heavier interceptors — a Hawk Air Warrior or the first Retaliator. |
| 8 | **First Mothership.** Assault Ships return this week — a second chance at UFO Type 6 if week 3–5 slipped. |

### The manufacturing pivot

***[S/AOW, WONG §16]***

Once you can manufacture Toxiguns and craft, **engineering becomes the primary economy**, not
research. Move your best staff accordingly. Ranked by profit per labour hour
([wongFaq.txt](../tools/extractors/docs/wongFaq.txt) §16):

| Item | Total cost | Market | Profit | Profit/hour |
|---|---|---|---|---|
| **Stasis Bomb** | $1 312 | $2 650 | $1 338 | **0.669** |
| **Biotrans** | $13 960 | $34 000 | $20 040 | **0.572** |
| Toxigun | $1 368 | $2 780 | $1 412 | 0.470 |
| Multi Launcher | $4 560 | $9 260 | $4 700 | 0.470 |
| X-COM Body / Head Shield | $1 712 | $3 480 | $1 770 | 0.465 |
| Disruptor Launcher / Bomb | — | — | — | 0.444 |
| **Annihilator** | $55 600 | $100 000 | $44 400 | 0.444 |
| Personal Teleporter | $7 400 | $18 200 | $10 800 | 0.432 |
| Retaliator | $39 200 | $70 000 | $30 800 | 0.410 |
| Dimension Probe | $7 400 | $10 000 | $2 600 | 0.104 |
| **Bio-Transport Module** | **$924** | **$950** | **$26** | **0.006** |

**[D]** LIL advises manufacturing spare Bio-Transport Modules "to sell off for profit." At $26 a
unit for 4 000 labour hours it is the **worst** product in the game — 95× less profitable than a
Stasis Bomb. AOW is right: build exactly the one you need. **[D]** LIL also names Biotrans as a
money-maker; that part is correct, and it is the second-best in the game.

Watch the price-crash rule in [§11.1](#111-the-market) before dumping inventory.

---

## 9. Weeks 9–15 — the Overspawn era

Growth thins out to one or two ships a week; the standing fleet is now Battleships, Motherships,
Bombers and Escorts. Mission preference is dominated by Subversion and Overspawn.

| Week | Note |
|---|---|
| 9–11 | 20 % Overspawn every wave. Assault Ships return in week 11 — **the last Assault Ship spawn**, and your final chance at UFO Type 6 → Retaliator. |
| 12 | Subversion peaks at 30 %. |
| **13** | **Attack peak** — 30 % Attack, the highest until week 19 ([§3.3](#33-what-the-aliens-will-do-each-week)). Aliens come to destroy buildings, not to infiltrate: keep interceptors dispersed and expect city-damage score hits. Week 14 genuinely eases off (Infiltration back to 40 %) — that dip is real. |
| 14–15 | Last scheduled Mothership (14) and Battleship (15). From week 16 the DEFAULT list repeats forever. |

**Standing goals for this phase**

1. **Retaliator fleet.** Three Medium Disruptor Beams, unlimited ammo, unlimited fuel, shield space.
   Park them permanently in the sky at the city corners **[S/AOW]**.
2. **X-COM (Disruptor) Armor** for every agent — needs Disruptor Gun + Small Disruption Shield +
   Personal Shield.
3. **Toxin B** — Biological Warfare + The Alien Life Cycle. **Toxin B is enough to win.** Toxin C
   requires The Real Alien Threat (thirteen species plus autopsies) and is optional **[S/AOW]**.
4. **Financial self-sufficiency** — one workshop on Biotrans/Stasis Bombs pays for everything else.
5. **Annihilator** once Retaliator + UFO Type 9 are in hand.

**Base defence — and a large correction to all three guides. [D]**

WONG §7.2, NKF and AOW all build base defence around security stations and chokepoints. **Neither
works in OpenApoc:**

- **Security Stations are inert.** `FacilityType` has no weapon, turret or defence field;
  `FACILITYTYPE_SECURITY_STATION` is a buildable tile and nothing more.
- **You cannot position defenders.** `Battle::initialUnitSpawn` picks each defender's tile with
  `pickRandom` over the map's spawn blocks, and `BaseDefenseScreen` calls `beginBattle` with the
  whole base roster — there is no pre-placement step, so no chokepoint plan is expressible.

That leaves exactly one lever that does work: **arming people.**

- **A base with no armed defender is destroyed automatically the moment it is attacked.** Never
  leave one undefended, and never run on a single base — buy a second early purely as a
  failure-recovery hedge.
- **Technical staff cannot fight.** Scientists, biochemists, engineers and physicists have no
  inventory, cannot use weapons and cannot kneel. They are not defensive strength; they are
  casualties who also gut your research when they die.
- Every base still has one tile representing your personnel; if the roof collapses on it, everyone
  inside dies **[S/WONG §7.2]**. Slums collapse catastrophically; warehouses absorb more.

*These four points come from `docs/playing-the-game.md`, which established them by losing
campaigns to each of them in turn.*

---

## 10. The Alien Dimension — the ten-building sequence

### 10.1 Getting in

***[V]***

1. Build a dimension-capable craft — **Biotrans** (or Explorer/Retaliator/Annihilator).
2. **Fly it to `CITYMAP_ALIEN`.** `GameState::setCurrentCity` force-completes that city's
   `researchUnlock` ([gamestate.cpp:328](../game/state/gamestate.cpp#L328)), which is
   `RESEARCH_UNLOCK_ALIEN_DIMENSION` ([extractors.cpp:335](../tools/extractors/extractors.cpp#L335)).
   **You cannot research the alien dimension until you have physically been there.**
3. Research **The Alien Dimension**, then **Alien Building 0** (38 000 man-hours ≈ 1.8 days at
   900 biochem skill).

### 10.2 The sequence

***[V]***

Ten buildings, strictly linear. Raiding building *n* force-completes
`RESEARCH_UNLOCK_ALIEN_BUILDING_<n+1>`, which gates the research that lets you enter building *n+1*
([extract_buildings.cpp:82](../tools/extractors/extract_buildings.cpp#L82),
[research.xml:1415](../data/common_patch/gamestate/research.xml#L1415)).

| # | Building | Entry gated by |
|---|---|---|
| 0 | **Sleeping Chamber** | The Alien Dimension |
| 1 | **Food Chamber** | raid #0 |
| 2 | **Alien Farm** | raid #1 |
| 3 | **Maintenance Factory** | raid #2 |
| 4 | **Incubator** | raid #3 |
| 5 | **Control Chamber** | raid #4 |
| 6 | **Spawning Chamber** | raid #5 |
| 7 | **Organic Factory** | raid #6 |
| 8 | **Megapod Chamber** | raid #7 |
| 9 | **Dimension Gate Generator** | raid #8 — **`victory = true`** |

**[V]** Building 9 carries the `victory` flag ([building.h:95](../game/state/city/building.h#L95));
winning that raid is the win condition ([battle.cpp:3519](../game/state/battle/battle.cpp#L3519)).
On `FixShitUp` this raises `GameEventType::AliensDefeated` and plays `smk/wingame2.smk`; on
`master` the flag is read and then discarded. See [§15](#15-known-gaps-in-openapoc).

**This is a ten-building campaign, not the five or six the guides describe.** LIL documents five
trips, ending at building 4 (Incubator); AOW reports never passing the third building. Plan for
roughly twice the endgame the source material covers — see [§10.4](#104-the-endgame-calendar).

### 10.3 Doctrine inside

***[S/WONG §8, LIL]***

- **Do not loiter.** Reinforcements arrive through orange teleporter gates roughly every 10 seconds.
  Get in, destroy the objectives, get out. Vortex Mines will destroy the teleporters themselves.
- **Only the large marked objects need destroying** — with one exception: in the **Alien Farm**,
  every white cube must go. LIL needed about a dozen Vortex Mines there and rates it the hardest map.
- **Escort the transport both ways.** Retaliators or Annihilators split alien fire on the way in;
  on the way out, wait for a gap in the UFO patrol before launching.
- **Never leave an agent behind.** The building — and its landing pad — is destroyed on victory, so
  survivors on the ground are unrecoverable **[NKF]**.
- **Corpses are a minefield.** Dead aliens drop live mines and grenades that detonate when shot
  **[LIL]**.
- **Ammunition:** "if you think you have enough, you don't" **[LIL]**. Toxigun + Toxin B is your
  primary; Devastator Cannons for walls and long range; Vortex Mines for objectives.

---

### 10.4 The endgame calendar

*Every week number here is **[DERIVED]*** — computed from the verified rates in §4.1 and the
verified gate structure in §10.1–§10.2, assuming a campaign that hit the §3 deadlines and runs
10 staff at ~90 skill per lab. Treat these as the checkpoints a harness should assert against, and
as a schedule to be beaten, not as measured results.

**Getting to the door**

| Milestone | Earliest plausible | Gated by |
|---|---|---|
| Alien craft systems researched | **wk 1–2** | any recovered UFO (§3.6) |
| Advanced Quantum Physics Lab built and staffed | **wk 3** | any one alien system; base space |
| Dimension Probe researched | **wk 4–5** | all three alien systems |
| Advanced Workshop built | **wk 6** | Dimension Probe; construction time |
| **Biotrans researched** | **wk 7–8** | Dimension Probe **+ UFO Type 3** — the §3.5 chokepoint |
| First Biotrans manufactured | **+1.6 days** | 35 000 man-hours at ~900 skill |
| **First flight into `CITYMAP_ALIEN`** | **wk 8–12** | a dimension-capable craft that survives the trip |
| *The Alien Dimension* researched | **+hours** | unlocked by arriving (§10.1) |
| **Alien Building 0 researched** | **wk 9–13** | 38 000 man-hours ≈ 1.8 days |
| **Sleeping Chamber raided** | **wk 10–14** | the first raid |

The wide band on "first flight" is real and is the honest part of this estimate: it depends on
whether you have Toxin B, X-COM Armor and escorts ready, and going in early with none of them is
how campaigns die. **Do not enter before Toxin B.**

**The nine remaining raids**

Each building is a research topic gated on the previous raid, so the chain is strictly serial. The
research topics for buildings 1–9 carry no `man_hours` override in the repo data
([research.xml:1427](../data/common_patch/gamestate/research.xml#L1427) onward), so budget them at
Building 0's scale as a planning figure — roughly **1.5 weeks per building**, covering research,
re-equipping, healing and the raid itself. The Alien Farm (#2) is the documented outlier and should
be budgeted at double **[S/LIL]**.

| # | Building | Target week |
|---|---|---|
| 0 | Sleeping Chamber | 14 |
| 1 | Food Chamber | 15 |
| 2 | **Alien Farm** — budget double | 17 |
| 3 | Maintenance Factory | 19 |
| 4 | Incubator | 20 |
| 5 | Control Chamber | 22 |
| 6 | Spawning Chamber | 23 |
| 7 | **Organic Factory** — UFO growth stops here | 25 |
| 8 | Megapod Chamber | 26 |
| 9 | **Dimension Gate Generator — victory** | **28** |

**Budget: week 30.** Past week 15 the alien growth list is the fixed DEFAULT loop (§3.2) and the
city threat stops escalating, so a campaign that is healthy at week 15 does not get harder — it
just gets longer. A run still short of the Sleeping Chamber at week 20 is stuck, not slow, and the
place to look is the §3 deadlines rather than the endgame.

**This is the thinnest section in the document, and deliberately so.** No source guide covers it:
LIL documents five trips, ending at building 4 (Incubator), and reports the Alien Farm taking over
three hours of real play with two agents lost; AOW reports never passing the third building; WONG §8 is fifteen lines. Buildings
5–9 are described by the repo's data and Ufopaedia text but by none of the three walkthroughs.
The structure above is verified; the pacing is an estimate that the harness exists to replace with
measurements.

## 11. Economy — exact rules

### 11.1 The market

***[V]***

`EconomyInfo::update` runs at every Monday roll
([economyinfo.cpp](../game/state/city/economyinfo.cpp)). It behaves **differently** for goods
manufactured by X-COM and goods manufactured by anyone else.

**Third-party goods** (Marsec, Megapol, General Metro…):

```
lastStock    = currentStock
averageStock = (minStock + maxStock) / 2
currentStock = clamp(rand(0, averageStock + lastStock), minStock, maxStock)
price        ×= 97–100 % if stock > average, ×100–103 % if stock < average   (week > 1)
price         = clamp(price, basePrice × 0.5, basePrice × 2.0)
```

**[D]** AOW says "whatever we haven't bought at week's end is lost forever." That is not what the
code does. Stock is **re-rolled**, and unbought stock *raises* the upper bound of next week's roll
(`averageStock + lastStock`). Buying a line out therefore **lowers** next week's expected stock.
The practical advice — buy scarce consumables before the roll — survives, but the reason is that
prices drift with scarcity, not that stock evaporates.

**X-COM-manufactured goods** — this is where selling gets punished:

```
soldThisWeek = max(0, currentStock - lastStock)      // your sales inflate currentStock
if soldThisWeek > 2 × maxStock : price ×= 85–95 %
elif soldThisWeek >     maxStock : price ×= 90–95 %
elif soldThisWeek > maxStock / 2 : price ×= 95–97 %
price = clamp(price, basePrice / 2, basePrice)
```

**[V]** X-COM craft carry `maxStock = 2` ([wongFaq.txt](../tools/extractors/docs/wongFaq.txt) §16.1.9;
loaded verbatim by [extract_economy.cpp](../tools/extractors/extract_economy.cpp)). So for a Biotrans:

| Sold in one week | Price effect |
|---|---|
| 1 | none |
| 2 | ×95–97 % |
| 3–4 | ×90–95 % |
| 5+ | ×85–95 % |

Price never recovers — the ceiling is `basePrice` and only ever ratchets down to the
`basePrice / 2` floor ($17 000 for a Biotrans).

> **Rule: sell at most one X-COM-manufactured craft per week.** Dumping ten Biotrans converts a
> permanent $34 000 income stream into a permanent $17 000 one. None of the three guides states
> this, and AOW's "unloaded 99 of them to the market" advice is actively harmful for anything
> X-COM manufactures. It remains fine for *alien loot*, which X-COM does not manufacture.

**[V]** Buying is a state-level operation (`Organisation::purchase`,
[organisation.cpp:229](../game/state/shared/organisation.cpp#L229)); it decrements `currentStock`.
Selling adds to `currentStock` from the UI layer
([buyandsellscreen.cpp:470](../game/ui/base/buyandsellscreen.cpp#L470)) — see
[§15](#15-known-gaps-in-openapoc).

### 11.2 Income and funding

***[V]***

At each Monday roll (`weeklyPlayerUpdate`, [gamestate.cpp:1668](../game/state/gamestate.cpp#L1668)):

1. Funding is cut off permanently if the government is Hostile **or** `totalScore < −2400`.
2. Income is capped at half the government's balance.
3. `player->balance += income`, then `income += income / fundingModifier`.
4. Salaries and facility upkeep are deducted.

`calculateFundingModifier` ([gamestate.cpp:1860](../game/state/gamestate.cpp#L1860)) scans
`weekly_rating_rules` **without breaking**, so the *last* matching entry wins. Given the data order
in [gamestate.xml:16](../data/common_patch/gamestate.xml#L16), that means:

| Weekly score | Effective modifier | Income change |
|---|---|---|
| > 400 | 20 | **+5 %** |
| 0 … 400 | 0 | none |
| < 0 | −15 | **−6.7 %** |

**[D]** The tiering is inert. A 20 000-point week and a 401-point week both yield +5 %. The data
clearly intends "highest threshold wins" (+25 % above 12 800); a missing `break` collapses it.
Flagged in [§15](#15-known-gaps-in-openapoc). **Practical effect: clear the +400 bar every week and
stop optimising score for funding** — score still matters, but through
[§11.4](#114-score-drives-alien-tech), not through your budget.

### 11.3 Cash tactics

***[S/AOW]***

- Sell alien loot aggressively — Boomeroids, Devastator Cannons, Disruptor Guns, Personal Shields.
  X-COM does not manufacture these, so the price-crash rule does not apply.
- Stun rather than explode. Blowing up a Personal Shield destroys $5 770 of sellable goods.
- Sell surplus fuel; parked craft burn none.
- Need an item at another base? Sell it at one and buy it at the other — no net cost.
- Need Janitor missiles? Buy a hovercar, strip the launcher, sell the hull back.
- **Vehicles do not come back.** Sell six hovercars and those six are gone from the market for good.
  Equipment can be rebought within the same week; vehicles cannot.
- Fire on Sunday 23:59, hire on Monday 00:01 — one week of salary avoided.

### 11.4 Score drives alien tech

***[V]***

OpenApoc implements "alien tech levels" as two independent score gates:

- **UFO weapons** scale with `totalScore.craftShotDownUFO`
  ([vehicle.cpp:3873](../game/state/city/vehicle.cpp#L3873)) — an alien craft only mounts equipment
  whose `scoreRequirement` you have already exceeded.
- **Alien infantry equipment** scales with `totalScore.tacticalMissions`
  ([agent.cpp:166](../game/state/shared/agent.cpp#L166)).

So shooting down UFOs arms future UFOs, and winning ground missions arms future aliens. This is the
grain of truth behind AOW's warning that raiding "boosts alien technologies faster."

**But do not conclude you should play passively.** [§3.5](#35-the-real-transporter-deadline) shows
that not shooting UFOs down lets the fleet accumulate and permanently escalates the incursion
*tier*, which is far more dangerous than the equipment gate. Shoot them down.

**[D]** AOW claims raiding organisations raises their guard tech level. Human org guard equipment is
keyed to `org->tech_level` ([agent.cpp:173](../game/state/shared/agent.cpp#L173)), and nothing in the
campaign code path ever writes `tech_level` — only the skirmish screen does
([skirmish.cpp:539](../game/ui/skirmish/skirmish.cpp#L539)). **[NI]** In OpenApoc, raiding does not
escalate human guards.

### 11.5 City damage

***[V]***

Destroying scenery subtracts `type->value` from both `totalScore` and `weekScore`
([scenery.cpp:1338](../game/state/city/scenery.cpp#L1338)) — and the owning organisation pays for the
repair at the next daily loop ([city.cpp:588](../game/state/city/city.cpp#L588)), which sours relations.
Prefer precision weapons in the city; save the Devastator Cannons for the alien dimension.

---

## 12. Combat doctrine

### 12.1 Air combat

- **Numbers over quality, early [S/WONG §2.3.1, AOW].** Hoverbikes are small, cheap and rarely hit;
  ~6–8 incursions is a typical lifespan. Run ten of them, dispersed, plus a few hovercars.
- **Focus fire.** Scramble everything and kill one UFO at a time, nearest first.
- **Kill the carriers first.** Dropships are slower and fewer than escorts.
- **Missiles only against slow targets [S/WONG].** Against fast UFOs missiles run out of fuel;
  use beam weapons.
- **Altitude discipline.** Four craft can occupy one tile at four different altitudes; spreading
  them vertically prevents friendly fire.
- **Free parking [NKF].** Craft may sit in any city building indefinitely without burning fuel.
  Disperse them around the map — and put a third to a half at the Transtellar spaceport, whose many
  bays let them all launch on one scramble order **[S/WONG §2.3.2]**.
- **Repair only at base.** Send damaged craft home, let them sit an hour, redeploy.
- **Never carry agents on fighters [NKF].**

### 12.2 Ground combat

- **Real-time vs turn-based** is chosen per battle. Real-time allows firing two weapons at once
  **[S/WONG]**; turn-based is safer against Poppers and Brainsuckers **[S/WONG §2.4.7]**.
  Both are viable; pick one and get good at it **[NKF]**.
- **Aimed or snap shots, never auto-fire [S/WONG §2.2.4].** Ammunition is finite and auto-fire is
  the fastest way to run dry.
- **Kneel or crawl to shoot.** Accuracy improves substantially.
- **Fight on your ground.** Hold a corridor or doorway and let the aliens walk into your line of
  fire. Use smoke as mobile cover.
- **Androids** cannot be brainsucked or dominated (max psi defence) and make ideal point units early
  and psi-specialist counters later. They cannot improve their stats **[AOW]**.
- **Hybrids** have weak health but train psi well — the only unit type for which high starting psi
  matters ([§2.5](#25-healing-and-training--the-hourly-engine)).
- **Squad composition [AOW]:** roughly 60 % humans (they train best), plus androids as shock troops
  and a small hybrid psi cadre. Hire generously — expect ten agents healing and several dead at any
  time.
- **Capture live [S/WONG §2.4.6].** Stun Grapples have unlimited ammo. Poppers, Psimorphs,
  Micronoids, Megaspawn and Skeletoids need a spare agent standing on the stunned body to stop them
  waking or self-destructing **[AOW]**.
- **Brainsuckers first, always.** A successful suck permanently converts the agent.
- **Verticality is a weapon.** Explosives destroy floors; enemies fall through. So do your agents —
  LIL lost most of a squad to a Spitter rupturing the floor from above.
- **Retreat is a legitimate move.** Every battlescape has blue exit tiles at its edges; standing an
  agent on one extracts them safely. No mission is worth a trained agent — fighting every mission
  to the death is what produces a losing spiral. **[V]**
- **Turn-based is safer for a scripted player** on ordinary missions; the pause-per-action removes
  the reaction-time disadvantage a click-driven runner has in real time.

---

## 13. Corrections to the source guides

The reconciliations worth knowing before you follow any single guide verbatim.

| # | Guide claim | OpenApoc reality | Tag |
|---|---|---|---|
| 1 | "12 am is when any healing is done" **[NKF]** | Healing is **hourly** (0.8 HP/h). Midnight only clears `recentlyFought`, which *unblocks* healing for agents who fought today. | **[D]** |
| 2 | "Training benefits apply if at full health at midnight" **[NKF]** | Training is **hourly** and has **no health gate**. It requires being at the home base and under-capacity facilities. | **[D]** |
| 3 | "If infiltration rises above the half-way line the org will be taken over" **[NKF]** | Takeover is a **probabilistic roll every 125 min**: `rand(0,200) < infiltrationValue`. There is no threshold and no safe band. | **[D]** |
| 4 | "Sell off average agents, buy >85 skillpoint" **[LIL]** | Correct for **technical staff** (skill is static). **Wrong for agents** — improvement chance is `(100 − stat) %`, so low-stat recruits train fastest. AOW has this right. | **[D]** |
| 5 | "Manufacture several Bio-Transport Modules and sell them for profit" **[LIL]** | $26 profit for 4 000 labour hours — profit/hour **0.006**, the worst item in the game. Build one. | **[D]** |
| 6 | Bio-Transport Module ≡ Biotrans | **Two different things** — a $950 vehicle module and a $34 000 craft. Fusing them breaks the tech chain. | **[D]** |
| 7 | "UFO Type 3 unlocks the three alien craft systems" **[LIL, AOW]** | **Any** recovered UFO unlocks all three. A week-1 Probe is enough. Type 3 is required for *Biotrans*, not for the systems. | **[D]** |
| 8 | "Unbought stock is lost forever at week's end" **[AOW]** | Stock is **re-rolled**, and unbought stock raises next week's upper bound. Buying a line out lowers next week's expected stock. | **[D]** |
| 9 | "Unload 99 Boomeroids to the market" **[AOW]** | Fine for alien loot. **Ruinous for anything X-COM manufactures** — selling >maxStock/2 in a week permanently ratchets the price toward `basePrice/2`. | **[D]** |
| 10 | "Just let the UFOs run through the city" **[AOW]** | Passive play lets the alien fleet accumulate, which raises the incursion tier and **removes Transporters from the rotation** — potentially unwinnable. | **[D]** |
| 11 | "Raiding boosts organisation guard tech" **[AOW]** | `org->tech_level` is never written outside the skirmish screen. Raiding does not escalate human guards. | **[NI]** |
| 12 | Endgame is ~6 alien buildings **[LIL, AOW]** | **Ten**, strictly linear, ending at the Dimension Gate Generator. | **[D]** |
| 13 | "Score at end of week for more funding" **[WONG §2.2.3]** | Funding adjustment is effectively **binary** (+5 % / 0 / −6.7 %) because `calculateFundingModifier` never breaks out of its loop. | **[D]** |
| 14 | Build a second Alien Containment early **[LIL]** | One is present in the starting base. Build a second only when capacity actually binds. | **[D]** |
| 15 | *This guide, v1*: "week 13 is a difficulty spike" | A `master` **data defect** — week 13 is missing from the shipped patch. The EXE has it, and it is an Attack peak (30 %), not a general spike. Week 14 was also wrong. | **[D]** |
| 16 | *This guide, v1*: "first UFO wave lands 22:00–24:00 on day 0; cadence 1–4 days" | `master` invents the opening delay. The EXE uses one formula throughout: **24–72 h**. Day 0 has no wave. | **[D]** |
| 17 | *This guide, v1*: "infiltration decay scales with difficulty, up to −108/day" | The difficulty term lives in the **divisor** (`42 − difficulty`), which makes infiltration accrue *faster*. Decay is **−12/day at every difficulty**. | **[D]** |
| 18 | All three guides: the alien dimension is a uniform grind | **Destroying the Organic Factory (#7) permanently stops weekly UFO growth** ([§3.7](#37-the-organic-factory-shuts-the-tap-off)). No guide mentions it. | **[V]** |
| 19 | All three guides: agents improve by training | Combat awards stats too, on a second track ([§2.5](#25-healing-and-training--the-hourly-engine)). Gym-only agents improve strictly slower than agents who also fight. | **[V]** |
| 20 | "Androids cannot improve their stats" **[AOW]** | They cannot **train** (`canTrain=false`) but they do gain from combat at **10 %** rate. Hybrids have the *same* rates as humans; their psi edge is starting stats, not learning rate. | **[D]** |
| 21 | Marketing / folklore: "the AI learns" | It does not. Fixed behaviour modes; the only `EXPERIEN.DAT` machinery is **your** agents' post-battle stat gain. See [§16](#16-ai-fidelity--and-the-learning-ai-question). | **[NI]** |
| 22 | "Researching Dimension Gates is of no gameplay consequence" **[LIL]** | True for its output, false operationally: an idle lab wastes `totalSkill` man-hours **every hour**. Always have a project queued. | **[D]** |

---

## 14. Difficulty deltas

Difficulty is `state.difficulty`, 0 (Novice) … 4 (Superhuman). Everything above assumes 0.

| System | Effect of higher difficulty | Reference |
|---|---|---|
| **City map** | Selects one of five maps (`citymap1`…`citymap5`) — different building layouts and base sites. Note the enum offset: `Difficulty::DIFFICULTY_1 = 0` ([extractors.h:43](../tools/extractors/extractors.h#L43)), and the `difficulty0` submod that `tests/CMakeLists.txt` loads maps to it ([main.cpp:45](../tools/extractors/main.cpp#L45)) — so **Novice is `state.difficulty == 0` and uses `citymap1`**, and every difficulty-indexed number in this document is the `[0]` entry. | [extractors.cpp:361](../tools/extractors/extractors.cpp#L361) **[V]** |
| **Building detection** | Slower: 70 / 80 / 85 / 95 / 110 minutes per attempt. **Alien sites are found later on higher difficulty.** | [building.h:17](../game/state/city/building.h#L17) **[V]** |
| **Detection value** | `detectionValue *= function->detectionWeight − 2 × difficulty` — detection is also less likely to succeed. | `Building::detect` **[V]** |
| **Infiltration accrual** | **Faster**, not slower: the accrual divisor is `42 − difficulty`, so Superhuman accrues ~11 % more per hour than Novice. Decay is a flat −1 on odd hours (**−12/day**) at *every* difficulty. | `Organisation::updateInfiltration`; `UFO2P FUN_0007fcc0` **[V]** |
| **Starting relations** | Worse: relation to the player is adjusted by `10 − 5 × difficulty`; positive inter-org relations improve and negative ones worsen by up to `3×`/`5×` difficulty. | [gamestate.cpp:621](../game/state/gamestate.cpp#L621) **[V]** |
| **Bribe events** | Rarer at high difficulty: gated on `rand(0,100) > (difficulty + 1) × 10`. | `updateEndOfDay` **[V]** |
| **Alien equipment sets** | Extracted per difficulty. | `extractAlienEquipmentSets` **[V]** |
| **Craft equipment score gates** | Per-difficulty: `cequip_score_req_data` @ `0x1421C4` is 5 × 5 uint32 and `equipDefaultEquipment` reads the **difficulty column**, so UFOs arm up at different `craftShotDownUFO` thresholds per level. | `tools/extractors/common/vequipment.h` **[V]** |
| **Number of bases / starting funds / enemy counts** | 6 base sites on Easy up to 8 on Superhuman; $140 000 at Novice; more aliens per mission. | **[S/NKF, LIL]** |

**Score progression and tech unlock speed [S/NKF]:** harder levels field more enemies, so score
accrues faster, so the `scoreRequirement` gates in [§11.4](#114-score-drives-alien-tech) open sooner.
The alien arms race is genuinely faster on higher difficulty; the *calendar* in
[§3](#3-the-campaign-spine--the-alien-order-of-battle) is not.

---

## 15. Known gaps in OpenApoc

Re-checked against **`khallmark/FixShitUp`**. Several items from this document's first version were
`master`-only and are now resolved.

### Resolved on `FixShitUp`

| Was | Now |
|---|---|
| No victory state (`LogError("You won, but we have no screen for that yet LOL!")`) | Implemented — `GameEventType::AliensDefeated` → `smk/wingame2.smk` → main menu ([battle.cpp:3590](../game/state/battle/battle.cpp#L3590)) |
| `calculateFundingModifier` never breaks, collapsing six tiers to a binary outcome | Fixed — the **tightest** matching band wins ([gamestate.cpp:1860](../game/state/gamestate.cpp#L1860)) |
| Overspawn falls back to a plain building attack | Implemented |
| `UFO_MISSION_PREFERENCE_13` missing; week 14 wrong | Extracted from the EXE instead of hand-copied |
| Invasion delay invented (`10 h` opener, 24–96 h cadence) | EXE formula, 24–72 h throughout |
| Infiltration difficulty term in the wrong place | `42 − difficulty` divisor, matching `FUN_0007fcc0` |
| UFO growth unconditional | Gated on a living Organic Factory ([§3.7](#37-the-organic-factory-shuts-the-tap-off)) |

### Still open

| # | Gap | Evidence |
|---|---|---|
| 1 | **Alien AI never takes cover.** `getTakeCoverMovement` returns null, so Normal/Cautious/Evasive collapse to kneel-or-prone. The largest fidelity gap in the game. | [§16.2](#162-what-openapoc-implements-and-what-it-does-not) |
| 2 | **Wounded penalties and AI medkit use** are absent — no recovered TU/accuracy constants. | gap matrix, issue #265 |
| 3 | **Cloak tick thresholds** and **Entropy Enzyme** spread constants unbound; `HAZARD_SPREAD_CHANCE` is still a made-up number. | gap matrix |
| 4 | **`TACDATA/EXPERIEN.DAT` is not extracted.** `processExperience` is X-COM 1/2 prior-art carrying its own `FIXME: Ensure correct`, so agent progression is *plausible*, not *faithful*. | [battleunit.cpp:4171](../game/state/battle/battleunit.cpp#L4171) |
| 5 | **Multi-tile unit pathing/drawing** has no recovered TACP table. | gap matrix |
| 6 | **Vehicle attack ladder** (100/80/50/10) and `Rules of engagement` remain unbound. | gap matrix |
| 7 | **Bribe / diplomatic-rift dollar formulas** unbound, so [§11.3](#113-cash-tactics) diplomacy advice is directional only. | gap matrix |
| 8 | Score still accrues partly in `cityview.cpp`, and `weekScore.reset()` still lives in the weekly funding screen — harmless for UI-driven play, but it means score is a property of the *UI* being open. | `game/ui/tileview/cityview.cpp` |

Items 1–4 are **blocked on reverse-engineering evidence, not on effort.** Inventing the constants
would reintroduce exactly the made-up numbers `docs/original-game/` exists to eliminate.

## 16. AI fidelity — and the "learning AI" question

The original game's marketing promised an AI that learns. **It does not, and it never did.**

### 16.1 What is actually in the binary

***[V]*** — from the Ghidra work in `OpenApoc-og-research` over `TACP.EXE` / `UFO2P.EXE`, and from
[original-game/subsystems/battle-ai.md](original-game/subsystems/battle-ai.md).

The only adaptation machinery anywhere in the binaries is `TACDATA/EXPERIEN.DAT` and the printable
string **`Experience Processing`** — and that is the *agent* post-battle stat award described in
[§2.5](#25-healing-and-training--the-hourly-engine), i.e. **your** units getting better, not the
aliens'. There is no printable `learn`, `adapt`, `evolve`, or `remember` in either executable, no
persistent per-encounter weighting, and nothing that carries tactical state between battles.

The alien AI is a **fixed behaviour set**, specified in
[tools/extractors/docs/ai.txt](../tools/extractors/docs/ai.txt) and confirmed by TACP printable
strings (`Cautious mode`, `Aggressive mode`, `Kneel down`, `Unit has gone berserk`,
`Unit critically wounded`, `Unit under fire`):

| Mode | Behaviour |
|---|---|
| Default | Always on. Turn to the focused/closest enemy; turn toward incoming fire from an unseen shooter; attack unless on Cease Fire. |
| Vanilla | Idle movement to a random unoccupied LOS block; weapon/grenade/melee selection by a priority score; random morale-driven retreat. |
| Normal | Take cover against the threat if better cover exists, else kneel. Potshots from cover. |
| Cautious | Always take cover; prone if possible, else kneel. Potshots from cover. |
| Aggressive | **Nothing** — deliberately a no-op, so Default + Vanilla drive the unit. |
| Panic Run / Panic Freeze / Berserk | Morale states: drop items and flee / do nothing / fire at random targets. |

What looks like escalating alien competence across a campaign is not learning. It is two score
gates ([§11.4](#114-score-drives-alien-tech)) swapping in better *equipment*, plus the weekly fleet
and mission-preference tables ([§3.2](#32-weekly-fleet-reinforcement),
[§3.3](#33-what-the-aliens-will-do-each-week)) sending bigger *ships*. The decision-making is
identical in week 1 and week 30.

### 16.2 What OpenApoc implements, and what it does not

From [original-game/openapoc-gap-matrix.md](original-game/openapoc-gap-matrix.md)
and OpenApoc issue #265:

| Behaviour | Status | Note |
|---|---|---|
| Vanilla attack priority (`CTH × DAMAGE / TIME`) | implemented | prior-art from `ai.txt`; medium confidence |
| Default AI (turn, return fire) | implemented | |
| Aggressive | implemented as a no-op | correct — matches `ai.txt` |
| Panic Run / Freeze / Berserk | implemented | |
| Tactical retreat chance | implemented | `neutralizedPercent − 50`; the older formula was always zero |
| Group teleporter move | implemented | full-teleporter units teleport, the rest walk |
| Mind Shield (+30, cap 200) | implemented | locked to TACP `FUN_0009b780` @ `0x9B780` |
| **Normal / Cautious cover + potshots** | **missing** | `getTakeCoverMovement` returns null — units never actually take cover |
| **Evasive seek-cover** | **missing** | kneel/prone fallback only |
| **Wounded move / shoot penalty** | **missing** | string `Unit critically wounded` exists; no recovered constants |
| **Wounded medkit use in cover** | **missing** | no AI heal path |
| **Cloak thresholds** | **missing** | type `0x0a` extracted, tick thresholds unbound |
| **Entropy Enzyme constants** | **missing** | `HAZARD_SPREAD_CHANCE` still a made-up number |
| **Multi-tile unit path / draw** | **missing** | no TACP table recovered |

**The tactically important gap is cover.** Because `getTakeCoverMovement` returns null, aliens
never break line of sight, never fall back to hard cover, and never trade shots from a defended
position — they close and they shoot. That makes the guide's core ground doctrine
([§12.2](#122-ground-combat) — hold a corridor and let them walk into your fire) **more effective
in OpenApoc than it was in the original**, and it means a squad tuned against OpenApoc's aliens is
tuned against a meaningfully softer opponent than the 1997 game presented.

Two honest consequences:

- Any difficulty tuning derived from play against current OpenApoc is optimistic, and will need
  revisiting when cover lands.
- Several of these gaps are **blocked on evidence, not effort** — the constants are simply not
  recovered from TACP yet. Guessing them would reintroduce exactly the invented numbers this
  project has been removing.
