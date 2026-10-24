# 02 - Research, captures and the endgame chain (Superhuman)

**Summary.** The research, tech-progression, capture, UFO-recovery and alien-dimension plan for Superhuman (engine difficulty 4), written against the ORIGINAL game's rules (UFO2P.EXE / TACP.EXE). Where the rebuilt engine differs in a way that changes what the harness must do today, a marked **Engine today:** callout says so and names the fix in `04-engine-fidelity.md` that removes it. Rules read `WHEN <observable a human can see> THEN <action> UNTIL <observable>`.

1. **Cheap in hours, expensive in events.** Victory needs 168,000 Physics, 390,000 BioChem and 35,000 Engineering man-hours plus $51,100 of facilities, hull and capture modules [DERIVED, 3.1]. No item or specimen gates it. Four events do: E1 (the first WON ground mission against a MANNED UFO), E3 (a WON mission against an Alien Transporter, often the same battle), the 9-midnight Advanced Workshop build, and the first gate crossing.
2. **Unlocks** [EXE RR-12, RR-13]. A Probe or Scout recovery opens only its own UFO-type topic and gives no loot. The three alien-system topics open only after a won manned ground mission. Manned UFOs exist from Monday D5.5 (week 2), so E1 cannot come before about D5.6.
3. **Calendar** [DERIVED, 4]. With two extra Quantum labs ready by about D6.5, Biotrans hull S/T/L = D24.2 / D27.2 / D36.2; first raid D27.8 / D32.8 / D43.8; Control Chamber raid D42.8 / D57.8 / D78.8; victory D54.8 / D77.8 / D109.8 (preparation 0 / 48 / 105 h per building). Without the pair every date is 7 days later; order it by D1.0.
4. **The chain** [EXE VC-2, VC-3, RR-14]. The first crossing makes The Alien Dimension (6,000 Quantum) available; it gates the Sleeping Chamber topic (30,000); each later topic costs 2,000 more and unlocks only when the previous raid is won. Order: Sleeping, Food, Alien Farm, Maintenance, Incubator, Control Chamber, Spawning, Organic Factory, Megapod, Gate.
5. **Raid success = objective tiles destroyed, then you may leave** [EXE VC-1]; killing every alien without that does not count. Reinforcements: one unit per timer expiry [EXE TD12].
6. **Side effects.** Raid 2 (Food) frees the Mutant Alliance. Raid 6 (Control Chamber) stops weekly UFO growth permanently and arms a one-shot wave of up to 20 UFOs at the next invasion-timer expiry, cancelled if fewer than 10 ships can be gathered; plan for it (8.6). The Organic Factory (raid 8) stops nothing.
7. **Caps and capture** [EXE RR-09, RR-10, F-M12, TD17, TD19]: 6 small + 4 large labs/workshops per base, 50 persons per technical class, 180 persons, a flat 1.5 applicants per class per day, no relation coupling, no skill growth; in a UFO recovery only the dispatched craft is tested for a Bio-Transport Module, and stunned aliens score whether or not they are stored.
8. **LIVE differs on almost every gate above** (unmanned unlocks, a day-0 first topic, kill-all raids, no wave, uncapped hiring); section 9 scores deadlines, and every engine-only shortcut is a callout, not a plan.

**Contents**

- [0. Conventions](#0-conventions)
- [1. What the validation changed](#1-what-the-validation-changed)
- [2. Mechanics that set the research schedule](#2-mechanics-that-set-the-research-schedule)
- [3. Research queue, facilities and staffing](#3-research-queue-facilities-and-staffing)
- [4. Man-hour arithmetic and when milestones land](#4-man-hour-arithmetic-and-when-milestones-land)
- [5. Topic triage, defence tiers, manufacturing links](#5-topic-triage-defence-tiers-manufacturing-links)
- [6. Live-capture plan](#6-live-capture-plan)
- [7. UFO recovery doctrine](#7-ufo-recovery-doctrine)
- [8. The alien-dimension chain](#8-the-alien-dimension-chain)
- [9. Milestone calendar the benchmark can score](#9-milestone-calendar-the-benchmark-can-score)
- [10. Harness requirements](#10-harness-requirements)
- [Open measurements](#open-measurements)

---

## 0. Conventions

**Difficulty.** Superhuman = UI button 5 = engine `difficulty == 4`. Research, manufacturing, facility, build-time, repair, hire-cap and UFO-growth rules read no difficulty term [EXE RR-01]. What does depend on it here: start funds 100,000 and income 80,000 [EXE G4]; alien unit scaling +16% TU/health/reactions/stamina, +64% natural armour [EXE TD1-TD3]; UFO-kill gear gates 1,000 / 2,000 / 4,000 / 8,000 [EXE SCORE-UNLOCK-CRAFT]; alien infantry gear thresholds 1,000 / 2,200 / 4,000 / 6,600 / 10,000 / 18,000 / 28,000 of lifetime Tactical Missions score [EXE SCORE-EQLEVEL].

**Time.** `D` = elapsed game days since the new game began (Tue 7 Mar 2084 12:00 = D0.0). Harness day = `floor(D + 0.5) + 1`, so Day N starts at midnight `D = N - 1.5` (Day 7 = D5.5) [failure_forensics.md 0.4]. Midnights fall at D = k + 0.5. Game week `n` starts Monday 00:00 at `D = 7n - 8.5` (wk2 D5.5, wk3 D12.5, ... wk12 D75.5); UFO growth, the organisation tech step and funding step then [EXE `FUN_0004ac40`].

**Evidence tags.** `[EXE ...]` original binary: physical file offset (UFO2P code = Ghidra VA + 0x626A4, data = VA + 0x616A4) or register ID (e.g. `RR-12`). `[ENGINE ...]` code or generated data; `LIVE:` is the tree the benchmark runs, `HEAD:` the committed `khallmark/strategy-engine-plan` (fda36d6c), a bare path means they agree. `[GUIDE: KEY]` community source, paraphrased; the key's URL is below. `[DERIVED ...]` computed here, arithmetic or parameters shown. `[MEASURE ...]` unknown; the entry says how the harness measures it, and the learner may tune starting values so marked. `[UNBOUND ...]` EXE evidence absent; a reverse-engineering lead is named.

**Evidence files.** RF = `evidence/resolved-facts.md` (final rules; wins over the register), ADJ = `evidence/adjudications.md`, REG = `evidence/validation_register.md`; per-mechanic write-ups are `docs/original-game/findings/validation-2026-10/v_*.md`.

**Guide keys.** UFOpaedia (ufopaedia.org/): UFO-QP Quantum_Physics_(Apocalypse); UFO-BIO Biochemistry; UFO-CLA Capturing_Live_Aliens_(Apocalypse); UFO-FAC Facilities_(Apocalypse); UFO-DG Dimension_Gates; UFO-AD Alien_Dimension_(Apocalypse); UFO-AB Alien_Buildings_And_Combat_Tactics; UFO-INC index.php?title=UFO_Incursions_(Apocalypse); UFO-TL index.php?title=Tech_Levels_(Apocalypse); UFO-ART index.php?title=Alien_Artifacts_(Apocalypse); AOW AllOutWar%27s_X-Com_Apocalypse_Guide; NKF NKF%27s_X-COM_Apocalypse:_Starter%27s_Guide. LP lparchive.org/X-COM-Apocalypse/ (GuavaMoment, difficulty unstated); GOG-UFO gog.com/forum/xcom_series/xcom_apocalypse_ufo_research; GH-263 github.com/OpenApoc/OpenApoc/issues/263; Roger Wong's FAQ: repo copy `tools/extractors/docs/wongFaq.txt`.

**Research corpus** (by file and section; archived at `og-research:export/strategy-plan-2026-10-08/research/`): `failure_forensics.md` (FF), `harness_audit.md` (HA), `tech_tree.md`/`tech_tree.json` (TT), `economy_tables.md`, `web_research_tree.md` (WRT), `web_alien_campaign.md` (WAC), `web_tactical.md` (WT), `web_city_air.md`, `income_manufacturing.md` (IM), `income_cult_raids.md` (IC).

**Cross-references.** Section 01 = opening, economy and bases (its Runway Gate RG(k), 1.4: a purchase above $5,000 passes only if the balance afterwards covers the weekly burn plus $10,000 plus k weeks of the burn not covered by the last Senate credit; k = 6, or 3 for the Advanced Quantum Lab, Advanced Workshop and their technicians). Section 03 = military doctrine; 05 = harness AI plan; 06 = benchmark protocol. Fix keys (T1+T2, G4, G8, RR-12+RR-13, VC-1 ...) index `04-engine-fidelity.md`.

**Units.** mh = man-hours; a lab with total assigned skill `S` makes `S` mh per game hour. Planning rates: small lab 5 x 80 = **400 mh/h** (measured start labs 379-432 [ENGINE, observed: HA 2.1]), 450 with 90-skill staff; Advanced lab 10 x 85 = **850**, 900 at 90, 950 at 95. Ceiling 500 / 1,000 mh/h per topic [EXE RR-03].

**Index spaces (do not mix).** Function index `f` (EXE name table): 0 Incubator, 1 Spawning, 2 Food, 3 Megapod, 4 Sleeping, 5 Organic Factory, 6 Alien Farm, 7 Control Chamber, 8 Maintenance, 9 Gate Generator. Raid order visits `f` = 4, 2, 6, 8, 0, 7, 1, 5, 3, 9, so Control Chamber = `f` 7 = **6th raid**, Organic Factory = `f` 5 = **8th raid** [EXE UFO_GROWTH_STOP].

---

## 1. What the validation changed

Earlier statements (first draft of this section, `docs/campaign-plan.md`, the harness) that the validation overturned.

| Earlier statement | Ruling now | Used in |
|---|---|---|
| A Probe or Scout recovery unlocks the three alien-system topics; a lost or aborted manned recovery still unlocks | **No.** Unmanned: its own type topic only, no loot. Manned: system topics, type topic and loot only for a WON mission; the wreck is removed either way [EXE RR-12, RR-13] | 3.1, 7 |
| The first alien-building topic is `ALIEN_BUILDING_0`, 38,000 mh, startable on day 0, a free filler | **No.** Sleeping is 30,000 mh and needs The Alien Dimension, which the first crossing makes available [EXE VC-2, VC-3, RR-14] | 3.4, 8.1 |
| Topic hours per step 38/42/32/46/30/44/34/40/36/48k | **30,000 + 2,000 per step**; step score 300 ... 480 [EXE RR-14] | 4.5, 8.1 |
| A raid is won by killing every alien; pads must be destroyed (about 100 Vortex Mines); the Gate is a field-clear | **No.** Success = objective tiles destroyed, then retreat is fine; one reinforcement per interval [EXE VC-1, TD12] | 8.4 |
| The Organic Factory (raid 8) stops UFO growth; the "Apocalypse" attack is unproven | **Control Chamber (raid 6)** stops growth permanently and arms a one-shot wave [EXE UFO_GROWTH_STOP, VC-10, F5] | 8.6 |
| Withdrawal band from the role table (30/20/25/10 %), so Transporters never flee | Threshold = constitution x `type_percent[slot]`/100; Transporter band in 1 of 5 slots, Fast Attack 3 of 5, Destroyer 2 of 11 [EXE F7] | 7.1 |
| UFO gear gates apply to the craft as launched; loot is whatever it carried | Flags latch every game midnight; every launched craft is gated; **loot is filtered by the flags at recovery time** [EXE F9] | 7.3 |
| Captures use any nearby craft | For a UFO recovery **only the dispatched craft** [EXE TD19] | 6.3 |
| A Transporter deposit into the Senate is takeover-lethal in 24 h (p 0.97) | Engine-calibrated. In the original a displayed infiltration above 50% is needed first: a 20-alien deposit flips it in a median 25-87 h, roughly 1-4 days [EXE T1, T2; RF 2.1; 7.3 U-4] | 7.3 |

**Still valid.** The plan's "first flight week 8-12, victory week 28" is far too slow as a target (section 9). The runtime tree is the generated `base_gamestate` (the extractor applies `common_patch`, tools/extractors/main.cpp:84-92; the ABL gate and the Life Cycle gate are in F-4 and B-5); extractor provenance differs by branch, so read both gates on Day 0 (M-S2-16). The harness list `PRIORITY_RESEARCH` omits four hidden ids (harmless) and its trigger `min_lab_skill 1100 < 1159` is inert [HA 4.1-4.3].

---

## 2. Mechanics that set the research schedule

The original rule, then the engine difference that matters today.

| # | Mechanic (original rule) | Evidence | Engine today |
|---|---|---|---|
| M1 | **Progress.** Each game hour a topic gains the sum of its assigned staff's skill (BioChem skill for group 0, Quantum for group 1, engineers' for manufacturing). Counted staff: employed, non-soldier, standing in their home base, **full health**. Hours = mh / (staff x skill) | [EXE RR-01 `FUN_000aac88` file 0x10D32C; RR-02 `FUN_000aac38` 0x10D2DC] | Injured staff still work (research.cpp:415-420; fix RR-02, low) |
| M2 | **Seats.** 5 per small lab, 10 per large (code immediates), valid agents only; a large topic (26 of 99) needs a large lab | [EXE RR-03 file 0x10A272, 0x10A28D; RR-04] | same |
| M3 | **Skill is fixed:** `60 + rand(0..40)` at recruit creation. P(>=85) 39%, P(>=90) 27%, P(>=95) 15% | [EXE RR-05 `FUN_000791e0`] [DERIVED] | same |
| M4 | **Builds.** The construction counter drops 1 at each midnight; usable and upkeep-free only at 0; an order at `t` finishes at the `buildDays`-th midnight after `t`. Lab 5 d, Advanced Lab 8, Workshop 6, Advanced Workshop 9, Living Quarters 3, Alien Containment 4. Full cost at placement; cancelling refunds `cost x remaining/buildTime` | [EXE RR-07 `FUN_0006bc70` 0xCE314; G9] | same (HEAD charges upkeep on unfinished facilities, D6) |
| M5 | **Caps.** At most 6 small and 4 large lab/workshop buildings per base (Biochemistry/Quantum/Workshop share the 6, the Advanced variants the 4), counted under construction. Persons: 100 agents, 50 biochemists, 50 engineers, 50 physicists, each counting employed plus listed applicants, inside 180 slots | [EXE F-M12 `FUN_00069270` 0x152252; RR-09 `FUN_0007a474` 0xDCB18, constants 0xDA9A4] | **No caps** (base.cpp:268-306; recruitscreen.cpp:604-680; organisation.cpp:541-626); fixes F-M12, RR-09; the harness self-limits |
| M6 | **Applicants.** Daily per class: each applicant is removed with `rand[0, floor(n/3)+1]`, then `min(rand[0,3], cap - (employed + applicants))` arrive, origin organisation uniform in 2..22, no relation, hostility or takeover test. Mean 1.5 arrivals per technical class per day, standing pool about 6-7. Fee = one week's pay (600 soldier, 800 technician); firing costs 0; a free living-quarter slot is needed | [EXE RR-10, V-AG-07, G9] | 33% of applicants leave daily; chance `25 + relation` per attempt; hostile and taken-over organisations supply none (organisation.cpp:541-626): 1.7x the original at relation 0, starving after takeovers; fix RR-10 |
| M7 | **Floor space.** 8x8 cell grid; starting facilities on random cells (the 2x2 Repair Bay included); a 2x2 needs four free cells. In 2 of 4 benchmark runs that reached the attempt the start base had no free 2x2 | [ENGINE base.cpp:182-306]; [ENGINE, observed: FF 7] | same |
| M8 | **Item gates** read the lab's own base: life-forms are consumed when a topic first starts; equipment gates need the item held, not consumed; a live topic adds the dead specimen (a Pod adds a live Brainsucker) | [ENGINE research.cpp:107-204, 360-380, 489-506]; EXE reader not validated | - |
| M9 | **One topic, one lab; progress belongs to the topic** (10-byte state: progress +4, lab cell +2). The engine keeps progress on reassignment (research.cpp:316-380); whether the original UI does was not read | [EXE RR-01]; [UNBOUND: assignment handler 0x10A028-0x10A321]; [MEASURE M-S2-1] | progress kept (the harness once saw a project lost: 3.4 R-0) |
| M10 | **Score channels are separate.** Research score (category 1) feeds funding only; alien infantry gear keys on lifetime Tactical Missions score, UFO shields/cloaks/teleporters on lifetime UFO Shot Down score (latched each midnight), guard kits on the weekly tech step. Research never arms the aliens | [EXE FUNDING-CONSUMES, SCORE-EQLEVEL, SCORE-UNLOCK-CRAFT] | research score is credited in the UI handler and lost when another stage is pushed in the same frame (about 0.6%; SCORE-C1) |
| M11 | **Hourly work in the alien dimension.** The hourly block (research, manufacturing, repair, infiltration) and the daily/weekly blocks are guarded by "current city == human city"; time with the view in the dimension may not advance labs, builds or funding | [EXE RR-17 `FUN_0004ac40` 0xAD2E4; UNBOUND: `FUN_000105b8` 0x72C5C, `FUN_00010908`]; [MEASURE M-S2-14] | one global clock; everything advances (gamestate.cpp:1416-1424) |


---

## 3. Research queue, facilities and staffing

### 3.1 The victory path

Hours are `man_hours` of the generated research data [ENGINE TT 5.1, 15; EXE `research_data` file 0x13EE80, 99 records, equal totals]. Phys-S/L = Quantum small/Advanced, Bio-S/L = BioChem, Eng-S/L = Workshop.

| # | Item | Lab | mh | Needs | Score |
|---|---|---|---:|---|---:|
| 0a | `BIO-TRANSPORT_MODULE` (research) | Bio-S | 9,000 | nothing | 80 |
| 0b | `MANUFACTURE_BIO-TRANSPORT_MODULE` x3 | Eng-S | 4,000 each, $700 | 0a | 0 |
| 0c | `DIMENSION_GATES` | Phys-S | 5,000 | startable Day 0; its role in crossing was not found | 50 |
| 1 | **E1**: first WON ground mission against a MANNED UFO (types 3-10) | - | - | squad and a wreck inside its lifetime; the craft's Bio + Cargo modules (6.3) bring home specimens and loot | event |
| 2a-c | `ALIEN_PROPULSION_SYSTEM` / `ALIEN_ENERGY_SOURCE` / `ALIEN_CONTROL_SYSTEM` | Phys-S | 25,000 / 25,000 / 30,000 | E1 | 150 each |
| 3 | `ADVANCED_QUANTUM_PHYSICS_LAB` -> build AQPL ($25,000, 8 d, 2x2) | Phys-S | 16,000 | any one of 2a-c | 80 |
| 4 | `DIMENSION_PROBE` | Phys-S | 12,000 | all of 2a-c | 80 |
| 5 | `ADVANCED_WORKSHOP` -> build AW ($12,000, 9 d, 2x2) | Phys-S | 24,000 | step 4 | 80 |
| 6 | **E3**: WON mission against an Alien Transporter (map `51ufo1`; 6 ever, weeks 2-4) | - | - | may be the same battle as E1 | event |
| 7 | `UFO_TYPE_3` | Phys-S | 12,000 | E3 | 100 |
| 8 | `BIO-TRANSPORT` (craft) | Phys-L | 18,000 | steps 4, 7 and AQPL built | 100 |
| 9 | `MANUFACTURE_BIO-TRANSPORT` x1 ($12,000) | Eng-L | 35,000 | step 8 and AW built | 0 |
| 10 | Fit Passenger Modules; send to a gate; the first move into the alien city makes record 1 available | - | - | a crossing-capable craft (8.2) | event |
| 11 | `THE_ALIEN_DIMENSION` | Phys-S | 6,000 | step 10 | 50 |
| 12 | `ALIEN_BUILDING` step k = 0..9 | Bio-L | 30,000 + 2,000k | k = 0: step 11; k >= 1: the previous raid WON | 300 + 20k |

Totals: **Physics 168,000 mh**, **BioChem 390,000** (+9,000 module), **Engineering 35,000** (+12,000 for three modules). Cash: AQPL $25,000 + AW $12,000 + hull $12,000 + three modules $2,100 = **$51,100**, plus Passenger Modules (5-6 x $400), the two extra Quantum labs ($20,000) and the optional ABL ($20,000). Added upkeep: AQPL + AW $400 a week [DERIVED from economy_tables.md 3.1]. **No item, corpse or live-alien gate lies on the path** [ENGINE TT 0.5].

> **Engine today:** the engine unlocks the three alien-system topics from a Probe/Scout wreck or an aborted manned landing (7.2) and offers `RESEARCH_ALIEN_BUILDING_0` (38,000 mh, no prerequisite) on day 0 (8.1): its tech path runs 5-13 days ahead of the original. The plan does not use these shortcuts. Fixes RR-12+RR-13 and VC-2+VC-3+RR-14; regenerate `base_gamestate` afterwards.

### 3.2 Lab roster and build orders

A lab is added when the work it will do can no longer be done in time by the labs that exist; **an Advanced lab is ordered the same game-hour its topic completes** (8-9 midnights of build against 1-3 days of research) [DERIVED]. Costs are Superhuman base prices. Every purchase passes section 01's Runway Gate (RG(6); RG(3) for the AQPL, AW and their staff); when it fails the research still runs and the build waits for the first Monday or loot event that passes, a day of delay on the AQPL/AW being a day on the Biotrans.

**Default versus comparison arm.** F-1 follows 01 ST-0: start the pair after a manned-UFO sighting or harness Day 7, subject to RG and space. The early F-1 arm below supplies the optimistic arithmetic in §§4 and 9; it is not a day-zero spending order. If construction or staffing is delayed, reforecast all dependent dates from actual completion. No research priority exempts spending from RG. For F-7 use 01 BS-P and 03 §7.6: stage the full computed garrison, not four visitors, before buying; disembark and verify it before time advances. Two stations are the default. If staging cannot be verified, defer the purchase.

| ID | WHEN (observable) | THEN | UNTIL | Cost / payoff |
|---|---|---|---|---|
| F-1 | Default: ST-0 fires and RG(6), the six-small-lab cap and space permit. Early comparison arm: before **D1.0** | Order **2 Quantum Physics Labs** ($10,000 each, 5 midnights), preserving a free 2x2; if that is impossible, first establish a feasible F-7 expansion under BS-P | both built and useful staff seated | $20,000, $100/wk each. Early-arm three-system parallelism gives hull S D24.2 / T D27.2 / L D36.2 versus D31.2 / D34.2 / D43.2 without the pair. These are conditional arithmetic, not default-policy deadlines [DERIVED, 4.1] |
| F-2 | `ADVANCED_QUANTUM_PHYSICS_LAB` completes | Place the AQPL the same game-hour (no free 2x2: F-7) | built (8th midnight) | $25,000, +$200/wk; unlocks Large Physics (BIO-TRANSPORT, Medium Beam, bombs) |
| F-3 | `ADVANCED_WORKSHOP` completes | Place the AW the same game-hour | built (9th midnight) | $12,000, +$200/wk. **The longest wait on the path:** ready 9 midnights after the Probe topic plus about 2.5 d |
| F-4 | Any of the 27 biology topics completes (`BRAINSUCKER_PODS`, 6,000 mh, is one) | Research `ADVANCED_BIOCHEMISTRY_LAB` (16,000) in the same lab; place the ABL at completion | built | $20,000, +$200/wk. Chain research 400 -> 850 mh/h (about 22 days over the ten topics, 4.4); hosts Biological Warfare and Toxin B |
| F-5 | The Bio queue holds >= 2 startable specimen topics and every Bio lab is busy | Order 1 Biochemistry Lab ($10,000, 5 d): the 6th small building in B1 (3 at start + the pair + this); a 7th is refused | built | Toxin B about 0.6 d earlier per extra lab; defer when cash is short |
| F-6 | `ADVANCED_SECURITY_STATION` completes | **Optional.** 1 beside the Access Lift ($8,000, 4 d). It adds the same 4 turrets and 12-slot reservation as a basic station and its Disruptor Gun turret (42 damage, accuracy 30) is not clearly stronger than the basic Plasma Gun + pods | built | idle-lab score filler only (5.4) [EXE TD9, TD10; UNBOUND: `FUN_00059f54` slot argument] |
| F-7 | No owned base has a free 2x2 AND a Large topic is within 2 days of completing | Apply 01 BS-P: RG, staged garrison computed from 03 §7.6, purchase, immediate disembark/read-back, two stations, quarters and stores. Otherwise defer the lab and reforecast | B2 meets its fielded garrison target | $20,000 + shell and actual garrison costs; 48 h delay adds 2.0 d to the dependent hull [DERIVED] |
| F-8 | An Advanced lab sits in a base other than the one holding the item or specimen its next topic needs | Transfer it by the Transfer screen >= 1 game-day before the lab frees, or home the recovery craft there | topic started | topics read the lab's own base (M8) |
| F-9 | A build would make the 7th small or 5th large lab/workshop in one base | Place it in another base (the original refuses) | - | B1 holds 5-6 small + 3 large (AQPL, ABL, AW) [EXE F-M12] |

> **Engine today:** no cap (about 35 small or 9 large labs fit); the harness obeys F-9 itself. LIVE starts with 180,000 and a stored income of 10,000 (EXE: 100,000 and 80,000, +57,100 net on the first Monday): read affordability from the first weekly funding screen, never from 80,000 [EXE G4]. With funding cut about four Mondays of cash remain before a balance below -5,000 ends the original game; the engine has no such defeat, so the harness enforces its own floor [EXE G8].

**Cross-section contract.** F-1 uses ST-0 and RG, without an exemption. Section 01 ST-5 staffs the first hull with 10 engineers at AW completion (five double the 41 h manufacture, +1.7 d). Hire for B2 labs at B2. Section 03 owns the Transporter battle; this section owns its preconditions (7.3, 6.5). Do not build a third extra Quantum lab, a second Workshop before the AW, hidden Advanced Alien Containment/Cells, or a second Repair Bay. A Psi-Gym is optional only under 03's hybrid-training policy, never on the critical path.

### 3.3 Staffing

Staff produce only while seated, standing in their home base and at full health; an unseated technician is $800 a week of nothing.

| Date (D) | Event | Seats built (Phys / Bio / Eng) | Employed, target | New hires | Hire window |
|---|---|---|---|---|---|
| D0 | start | 5 / 5 / 5 | 5 / 5 / 5 | - | - |
| D5.0-D7 | pair built | 15 / 5 / 5 | 15 / 5 / 5 | 10 physicists | from D5.0 or the pair's completion, whichever is later (no manned UFO exists before Monday D5.5); the pool holds 6-7 physicists, so expect 6-7 seats at once and 10 within 2 days |
| first specimen +5 d | optional F-5 lab | 15 / 10 / 5 | 15 / 10 / 5 | 5 biochemists | after the F-5 order |
| ABL built (S D11.5, T D14.5) | | 15 / 20 / 5 | 15 / 15 / 5 | +5 biochemists | from the ABL order (8-day window) |
| AQPL built (S D18.5, T D21.5) | | 25 / 20 / 5 | 20 / 15 / 5 | +5 physicists (5 move from idle small labs) | from the AQPL order |
| AW built (S D22.5, T D25.5) | | 25 / 20 / 15 | 20 / 15 / 10 | +5 engineers (10 seated for the first hull) | from the AW order (9-day window) |

About 45 technicians at the crossing, $36,000 a week in pay, far below the 50-per-class cap. Every agent takes a bed (30 at the start base for 25 people; a hire above 100% is refused); the pair needs 10 free beds at B1 by D5.5, so keep section 01's two Day-1 Living Quarters on schedule and order a sixth by about D8.

| ID | WHEN | THEN | UNTIL |
|---|---|---|---|
| H-1 | Each game day at 00:05 (after the applicant refresh) | For every built lab with staff < seats and a startable topic waiting (or an Advanced lab finishing within its build window): open the Recruit screen **of that lab's base** and hire the highest skill in the lab's discipline. No skill floor while a seat is empty and work waits: a floor of 85 gives only 1.5 x 0.39 = 0.6 qualifying applicants per class per day [EXE RR-10, RR-05; DERIVED] | seats full, beds or RG out |
| H-2 | An Advanced lab is ordered | Hire at once so the complement is seated the day before it finishes (8 days AQPL/ABL, 9 AW). Never hire into a lab that cannot be placed (F-7). A half-staffed 10-seat lab at 5 x 85 loses 425 mh/h | lab staffed |
| H-3 | A candidate's skill >= the weakest member of a critical-path lab + 15 | Hire, assign, fire the weakest after the running topic completes (fee $800, penalty 0): +15 skill on a 5-seat lab = +75 mh/h (+19%) | critical topic done |
| H-4 | A technician is on a transport, in a squad, injured or off-base | Return or replace; never put technicians in raid or alert squads or transfers (a travelling agent produces 0); an injured scientist adds nothing until healed (20 HP/day in a Medical Bay) | - |
| H-5 | Employed + listed applicants of a class reach 45 of 50, or total persons reach 170 of 180 (policy margins below the caps) | Stop hiring that class; the original refuses beyond 50 per class and 180 persons [EXE RR-09] | - |
| H-6 | A seat is unfilled > 3 days AND its lab has no startable topic | Move the seated staff to a lab with work; do not fire | - |

**Staffing precedence:** H-1 fills useful critical-path seats with the best available applicant, without a skill floor, subject to RG, beds and caps. The skill floors in 01 ST-4 apply to discretionary hire-ahead and upgrades, not to leaving a critical seat idle. H-6 moves idle staff to useful work in normal operation; in CUT mode, 01 C-2 permits dismissal after three idle days when no useful reassignment exists. F-8/K-7 transfer specimens; any technician transfer is a planned loss of research time, not permanent exclusion from transfer orders.

> **Engine today:** hiring is relation-coupled and 33% of applicants leave daily, so after takeovers the pools can run dry while the original keeps delivering 1.5 per class per day; there are no caps (organisation.cpp:541-626). The harness follows H-1..H-5 and reads the Recruit lists daily for two weeks (M-S2-13). Fixes RR-09, RR-10.

> **Engine today (base defence and the research campus):** a lost base fight kills every agent and craft present even when a second base exists, so the lab staff die with the base (original: re-homed; BD14a+BD16); hostile-organisation Raid/Storm never reaches an X-COM base in a continuous run, so logged base-defence frequency understates the original (BD08); and the engine roster is soldiers-first with uncapped turrets of invented strength (BD04+TD10+TD8+TD9). Keep the section-03 garrison at every research base and never retreat from a base defence.

### 3.4 Standing orders per lab type from Day 0

**R-0.** (a) Never leave a lab idle with a startable topic: sweep every <= 3 game hours (24 h latency costs 1 day before the crossing and about 10 across the chain, 4.4). (b) Long topics are preemptible fillers only where progress is kept. The engine code keeps a topic's progress when its lab is reassigned (research.cpp:316-380), but the harness author observed a project "discarded" at 23,262 of 25,000 mh after pressing New Project on a busy lab (`tools/oa_play.py` LIVE :2664-2670): until the M-S2-1 test confirms, preempt only fillers with little progress. (c) A Large topic starts only in a Large lab. (d) Never start a life-form topic unless the specimen is in this base's stores; never sell the last unit of an item an unfinished or unstarted topic needs.

**Physics** (one ranked list; an idle lab takes the first startable line not running elsewhere; the longest topic goes to the best-staffed free lab)

| Rank | Topic | Start condition | Notes |
|---|---|---|---|
| P-1 | `ALIEN_CONTROL_SYSTEM` 30k, `ALIEN_PROPULSION_SYSTEM` 25k, `ALIEN_ENERGY_SOURCE` 25k | listed after a won manned recovery | all three in parallel in three labs; Control is the long pole (75 h at 400); one lab serialises to 200 h |
| P-2 | `ADVANCED_QUANTUM_PHYSICS_LAB` 16k | any one system done | above the remaining systems only with a single lab; F-2 at once |
| P-3 | `DIMENSION_PROBE` 12k then `ADVANCED_WORKSHOP` 24k, back to back in one lab | all three systems done | the critical path: best-staffed small lab; H-3 pays here |
| P-4 / P-5 | `UFO_TYPE_3` 12k (E3) / `BIO-TRANSPORT` 18k (L; Probe + UFO3 done, AQPL built) | as stated | BIO-TRANSPORT is the AQPL's first job: 21 h at 850 |
| P-6 | `THE_ALIEN_DIMENSION` 6k | the first crossing has happened | rank 1 at once: it gates the first raid |
| P-7 | `LIGHT_DISRUPTOR_BEAM` 36k (beam item in this base) then `ADVANCED_SECURITY_STATION` 18k | beam item from a MANNED wreck | optional filler (F-6) |
| P-8 | `UFO_TYPE_n` for every flag that exists | Probe/Scout recoveries give `UFO_TYPE_1` 8k and `UFO_TYPE_2` 10k early | 80-200 score each; the only Physics work before E1 besides Gates |
| P-9 | Survival tier by readiness (5.4): `DISRUPTOR_GUN`, `VORTEX_MINE`, `PERSONAL_DISRUPTOR_SHIELD`, `DEVASTATOR_CANNON`, `MEDIUM_DISRUPTOR_BEAM` (L), `SMALL_DISRUPTION_SHIELD` (L), `DISRUPTOR_ARMOR` | the item is in this base | specimens, not capacity, are the limit |
| P-10 | `DIMENSION_GATES` 5k | Day 0 | filler, done D0.5 |

**BioChem**

| Rank | Topic | Start condition | Notes |
|---|---|---|---|
| B-1 | `BIO-TRANSPORT_MODULE` 9k | Day 0 | 22 h at 410 |
| B-2 | Cheapest first-specimen topic in stores: Hyperworm/Spitter/Popper autopsy or `BRAINSUCKER_PODS` (6k) < Brainsucker autopsy (7k) < Anthropod autopsy (8k) | one specimen home | unlocks the ABL; the pod topic leaves a live Brainsucker for `BRAINSUCKER` (9k) and its autopsy (7k) |
| B-3 | `ADVANCED_BIOCHEMISTRY_LAB` 16k | any B-2 topic done | F-4 at once |
| B-4 | `ALIEN_BUILDING` step k | listed (after the previous raid; k = 0 after The Alien Dimension) | **rank 1 in the ABL from the moment it is startable**; start within 3 game hours of the win (G-6) |
| B-5 | Toxin-B set: Egg autopsy, `MULTIWORM`, `HYPERWORM`, `CHRYSALIS` in parallel then their autopsies; Genetic Structure 25k after any Egg/Multiworm/Hyperworm autopsy; `BIOLOGICAL_WARFARE` 26k (L); Life Cycle 30k (six species AND GS); `TOXIN_TYPE_B` 15k (L) after BW + Life Cycle | specimens present | 59k + 25k + 26k + 30k + 15k = 155k mh |
| B-6 | Fillers by score density (5.2) | idle lab | **Bio has nothing startable between the module (D0.9) and the first specimen** (3.5) |

**Engineering**: E-1 `MANUFACTURE_BIO-TRANSPORT_MODULE` x3 as soon as the module research is done ($700, 4,000 mh, about 9 h at 430; not buyable; the benchmark never built one, HA D8); E-2 Toxigun and Toxin A/B in a small workshop once BW / Toxin B are done; E-3 `MANUFACTURE_BIO-TRANSPORT` qty 1 when the AW is built AND `BIO-TRANSPORT` is done (41 h at 850; the batch halts at balance <= cost, so keep $12,000 + the next Monday's overheads from the AW order); E-4 personal weapons as research completes (squad kit first, 5.3); E-5 `MANUFACTURE_DIMENSION_PROBE` **never** (4.5).

### 3.5 Idle-lab policy

Unavoidable idle (accept, measure, do not "fix"): Physics from D0.5 until E1 (only Gates and any Probe/Scout `UFO_TYPE_1/2` are startable); Bio from the module (D0.9) until the first specimen; Engineering after the modules (about D1.3) until the AW exists. Targets: idle-lab-hours (built labs with staff and a startable or filler topic, hours with no running topic) <= 5% of lab-hours D1-D25; reaction latency <= 3 h; seat fill >= 90%. Poll `gs research` on a game-time cadence (hourly within 6 h of a completion, else every 3-6 h).

---

## 4. Man-hour arithmetic and when milestones land

Every date in this section is [DERIVED] from the model below, not observed.

### 4.1 The model and its parameters

A 15-minute-step scheduler over the real topic hours (`tech_tree.json`), the real dependency edges (Life Cycle needs Genetic Structure; the ABL opens on the first autopsy, taken as the 7,000 mh Brainsucker autopsy, conservative), midnight-counted builds and one topic per lab with progress kept on reassignment. The original unlock rules are built in: the three systems and the Light Beam item appear only after E1, E3 gates `UFO_TYPE_3`, no `ALIEN_BUILDING` topic exists before the first crossing, the chain is 30,000 + 2,000k. It is plain arithmetic (about 270 lines); the hand check in 4.2 reproduces its headline numbers. It has no cash, hiring pool, beds, battles, alien response, return delay or frozen clock (M11).

| Parameter | Value |
|---|---|
| Lab rates | small 400, Advanced 850 mh/h (450 / 900 in one sensitivity row) |
| `react` / order latency | 3 h from a lab freeing to its next topic; 2 h from a topic completing to the build order |
| Extra Quantum labs | 2, ordered by D1.0 (ready D5.5), hired from D5.0 or completion with 70% of seats for 48 h; ordering at D0 with full seats gives the same dates |
| Crossing | `F_g` 4 h (craft ready to at the gate); `R_in` 6 h (midnight flip to raid won; battle time is rolled back); `lat` 3 h (raid won to next topic started) |
| Preparation `prep` | 0 / 48 / 105 h per building before each sortie |
| Chain research rate | 850 mh/h (ABL, 10 x 85) |
| Events **S** (stretch) | E1 = E3 = D7.0 (a week-2 Transporter battle won), first specimen D1.6 |
| Events **T** (target) | E1 = D10, E3 = D12, first specimen D4.5 |
| Events **L** (late, LP-like) | E1 = E3 = D19, first specimen D8 |
| Band **A** | the S events with no F-1 pair |

Victory bands: S = events S with prep 0; T = events T with prep 48; L = events L with prep 105. The earliest conceivable E1 is about D5.6 (week-2 growth lands at D5.5; the next incursion follows within 12-36 h if the double drain is real, 24-72 h if not; M-S2-17).

### 4.2 Hand arithmetic

`hours = mh / rate`, at 400 / 850 mh/h: `BIO-TRANSPORT_MODULE` 9,000 = 22.5 / 10.6 h; Propulsion or Energy 25,000 = 62.5 / 29.4 h; Control 30,000 = 75.0 / 35.3 h; AQPL or ABL topic 16,000 = 40.0 / 18.8 h; Probe or `UFO_TYPE_3` 12,000 = 30.0 / 14.1 h; AW topic 24,000 = 60.0 / 28.2 h; `BIO-TRANSPORT` 18,000 = 21.2 h (L only); Biotrans hull 35,000 = 41.2 h (L only); The Alien Dimension 6,000 = 15.0 / 7.1 h; Genetic Structure 25,000 = 62.5 / 29.4 h; Life Cycle 30,000 = 75.0 / 35.3 h; Biological Warfare 26,000 = 30.6 h and Toxin B 15,000 = 17.6 h (L only); Light Disruptor Beam 36,000 = 90 / 42.4 h; Disruptor Gun 40,000 = 100 / 47.1 h; Personal Shield 60,000 = 150 / 70.6 h; Disruptor Armor 85,000 = 212.5 / 100 h. At 450 / 900 (90-skill staff) divide by 1.125 / 1.059.

Critical path from E1 to a hull, three small Physics labs in parallel: `3.1 d (Control) + 1.25 d (Probe) + 2.5 d (AW topic) + 9 midnights (about 8.9 d) + 1.7 d (hull) = 17.5 d`, plus reactions and midnight rounding; E1 = D7.0 gives D24.2. With one Physics lab the systems serialise to 200 h (8.3 d) and the AQPL topic (40 h) sits before the Probe: 24.4 d, hence the +7 days of band A.

### 4.3 Milestone landing table (days D)

| Milestone | S | T | L | A (S events, no F-1) |
|---|---:|---:|---:|---:|
| `BIO-TRANSPORT_MODULE` researched | 0.9 | 0.9 | 0.9 | 0.9 |
| First autopsy (ABL unlocker) / **ABL built** | 4.1 / 11.5 | 7.0 / 14.5 | 10.5 / 18.5 | as S |
| `PROPULSION` / `ENERGY` / `CONTROL` done | 9.6 / 9.6 / 10.1 | 12.6 / 12.6 / 13.1 | 21.6 / 21.6 / 22.1 | 9.6 / 14.1 / 17.4 |
| AQPL topic / **AQPL built** | 11.4 / 18.5 | 14.4 / 21.5 | 23.4 / 30.5 | 11.4 / 18.5 |
| `DIMENSION_PROBE` | 11.5 | 14.5 | 23.5 | 18.8 |
| AW topic / **AW built** | 14.0 / 22.5 | 17.0 / 25.5 | 26.0 / 34.5 | 21.4 / 29.5 |
| `UFO_TYPE_3` | 11.0 | 14.0 | 23.0 | 19.1 |
| `BIO-TRANSPORT` researched | 19.4 | 22.4 | 31.4 | 20.1 |
| **Biotrans manufactured** | **24.2** | **27.2** | **36.2** | **31.2** |
| First crossing (first midnight after hull + 4 h) | 24.5 | 27.5 | 36.5 | 31.5 |
| **First raid won** (prep 0 / 48 / 105) | 27.8 / 29.8 / 31.8 | 30.8 / 32.8 / 34.8 | 39.8 / 41.8 / 43.8 | 34.8 / 36.8 / 38.8 |
| **Control Chamber raid (6th)** | 42.8 / 54.8 / 66.8 | 45.8 / 57.8 / 69.8 | 54.8 / 66.8 / 78.8 | 49.8 / 61.8 / 73.8 |
| **Victory** | 54.8 / 74.8 / 97.8 | 57.8 / 77.8 / 100.8 | 66.8 / 86.8 / 109.8 | 61.8 / 81.8 / 104.8 |

Reading it. (1) The pre-crossing path is bounded by E1 plus the 9-midnight AW build, not by man-hours: the hull moves 1:1 with E1. (2) E3 can slip to about D18 at no cost (the Biotrans research waits for the AQPL, D18.5-21.5); later costs 1 day per day (4.4). (3) Victory is dominated by the ten-raid chain, not by research before it; each extra 48 h of preparation per building adds about 20 days. (4) The Let's Play crossed at D30 and won at D99 on an unstated difficulty [GUIDE: LP]; band L has its shape.

> **Engine today:** a LIVE run that recovers a Probe or Scout at D3-D5 reaches the hull at D22.2 (D26.2 at D7) and can raid at its first crossing [DERIVED, same model with the unmanned unlock and the 38,000 mh first topic], but cannot reliably finish raids 3-10 (8.4). Credit is by deadline: after RR-12+RR-13, VC-2+VC-3+RR-14 and VC-1 the LIVE dates move to this table.

### 4.4 Sensitivities (days added to the hull / to victory)

S events, prep 48 h unless stated.

| Change | Hull | Victory |
|---|---:|---:|
| No F-1 pair (one Physics lab) | +7.0 (also under T and L) | +7.0 |
| Pair ordered at D2.0 / D4.0 / D5.5 instead of by D1.0 (with the hire lag) | S +1.0 / +2.0 / +4.0; T 0 / 0 / +1.0 | same |
| Pair seated at 70% or 50% from E1 until D9 / empty until D9 / empty until D11 | +1.0 / +2.0 / +4.0 | same |
| Lab reaction latency 24 h instead of 3 h (before the crossing) | +1.0 | +1.0 |
| Latency from raid win to next topic start 24 h instead of 3 h (chain) | 0 | **+10.0** |
| Advanced lab placed 48 h late (B2 bought after the topic completes) | +2.0 | +2.0 |
| 450 / 900 mh/h instead of 400 / 850 | -1.1 | -1.0 |
| Chain research rate 400 / 450 / 950 instead of 850 | 0 | +22.0 / +16.0 / -1.0 |
| E3 at D12 / D18 / D22 / D26 instead of D7 | 0 / 0 / +1.1 / +5.1 | 0 / 0 / +1.0 / +5.0 |
| First specimen at D4.5 / D8 instead of D1.6 | 0 | 0 (ABL +3.0 / +6.0 d; Toxin B +1.6 / +5.8 d) |
| A day of return before research can start (frozen human clock, M11) | 0 | +10.0 to +11.0 |
| Manned landing lost or aborted | E1 waits for the next manned UFO | no unlock; the wreck is removed [EXE RR-13] |
| Dimension Probe craft built first | +1.4 | 0 (first raid unchanged) |

### 4.5 The ten-building chain

Research per step at 850 mh/h: 35.3, 37.6, 40.0, 42.4, 44.7, 47.1, 49.4, 51.8, 54.1, 56.5 h; sum 458.8 h = **19.1 days** (40.6 d at 400; 17.1 d at 950). A cycle = research (35-57 h) + reaction 3 h + craft to the gate 4 h + wait to the next midnight (0-24 h) + 6 h to a won raid, which quantises to **3.0 d** at prep 0. After the first crossing you still owe The Alien Dimension (15 h in a small lab, 7 h in the AQPL) and the 30,000 mh first topic (35 h) before the first real sortie.

| Prep per building | First raid | Raid 6 | Victory |
|---|---:|---:|---:|
| 0 h | D27.8 | D42.8 | D54.8 |
| 48 h | D29.8 | D54.8 | D74.8 |
| 105 h (the Let's Play pace) | D31.8 | D66.8 | D97.8 |

(S events.) **A Dimension Probe craft adds nothing:** built in the AW's first free slot it delays the hull 1.4 d, its token crossing coincides with the Biotrans's and the first raid is unchanged [DERIVED]. Before the crossing, by size: AW build 9 d > AQPL/ABL builds 8 d (parallel) > systems + Probe + AW topics about 7 d > hull 1.7 d.

### 4.6 Research score and the funding counterweight

Modelled research score (only this model's topics; species, UFO-type and survival topics add), cumulative at Monday D5.5 / D12.5 / D19.5 / D26.5: S 330 / 1,610 / 3,520 / 3,520; T 250 / 330 / 2,840 / 3,520; L 130 / 330 / 450 / 3,140. Weekly funding adds F/20, F/16, F/12 above a weekly total of 400, 800, 1,600, and the lifetime total of completed weeks must stay at or above -2,400 at every Monday [RF 3.1]; so under the original unlock rule research cannot offset incursion bleed in weeks 2-3 unless E1 is early. Week 3-4 is where the Transporter specimen set pays (autopsies 120-150, Genetic Structure 300, Life Cycle 400).

---

## 5. Topic triage, defence tiers, manufacturing links

### 5.1 Topics by purpose

95 selectable topics, 2,761,000 mh, 19,930 score (99 and 3,093,000 with 4 hidden) [EXE RR-04 control; TT 15]. With every seat full the labs have about 2.2 M mh of Physics + Bio capacity between D15 and D40, so **capacity is not the constraint; specimens, items and ordering are**. MUST: the victory path (19 topics, 558,000 mh, 4,840 score), the module and the ABL. SHOULD: Toxin A/B, Disruptor Gun, Devastator, Personal Shield. Score fillers: species (371,000 mh, 6,180 score, 10-30 per 1,000 mh) and UFO-type topics once the UFO is recovered. Optional: beams and the Small Shield / Armor chain, Vortex Mine, Advanced Security Station, Explorer/Retaliator/Annihilator (8.7). TRAP or never: Teleporter and Cloaking items, Boomeroid, Dimension Missile, the Real Alien Threat / Toxin C / Gas chain (needs 12 species live), Overspawn (no ground unit), the four hidden topics (332,000 mh).

### 5.2 Score density for an idle lab (per 1,000 mh)

Queenspawn autopsy 30.0 (a dead Queen, raid 7); Hyperworm/Spitter/Popper/Anthropod autopsy 25.0; Psimorph 20.8 (L); Skeletoid 20.0; Chrysalis 18.8, Brainsucker 17.1, Pods and live topics 16.7 (a live topic leaves its corpse, so one live specimen feeds both); Dimension Missile 12.5; UFO-type topics 8-10; alien-building steps 10.0 (always first); Gates 10.0, module 8.9, Light Beam 5.6, Devastator 5.3, ASS 4.4; Disruptor Gun 2.5, Shield and Vortex 3.3, Armor 1.8, Personal Teleporter/Cloaking 1.5 (the worst).

### 5.3 Which research unlocks profitable manufacturing

**Selling rules belong to section 01; the facts that govern them:** selling a craft adds no market stock, so craft prices never fall (the one unbounded income line); X-COM-makeable goods, Psiclone, Elerium and AG missiles lose price from standing stock, about 0.075 x maximum stock per type per week being the safe pace, and the observable is a type's price falling on the market screen on a Monday (then hold further sales of that type) [EXE F-M1, F-M2, F-M3, F-M6; RF 10]. Margin per man-hour at base price: Stasis Bomb 0.725, Bio-Transport 0.629, Toxigun 0.527, Annihilator 0.500, Personal Teleporter 0.488, Retaliator 0.467, Explorer 0.400, Dimension Probe 0.160; net per workshop-week at an ASSUMED 400 mh/h: Bio-Transport $37,462, Annihilator $28,692, Retaliator $26,962; cheap fast goods far lower (Toxigun $3,742, AG missiles about $2,300) [EXE F-M6; MEASURE the staffed rate, M-S2-21].

Product lines: the Biotrans hull (35k mh, $12k) clears **$22,000 per hull, 0.629 per man-hour** once the AW is built (the first hull is the gate craft); Toxigun $1,580 (0.527); Disruptor Gun / Devastator / Personal Shield $1,500 / $3,300 / $3,770 (0.214 / 0.275 / 0.269, specimen in stores); Vortex Mine $880; Stasis Bomb $1,450 (0.725, thin market); Explorer / Retaliator / Annihilator are worse per man-hour than the Biotrans (value as escorts, 8.7).

Rule **I-1**: WHEN a manufacture topic completes and its specimen and facility gate is open THEN it enters section 01's product test (margin per man-hour >= $0.15 and the Runway Gate); research order never waits for a profit calculation, because every profitable line is gated by a specimen or the AW, not by lab capacity.

> **Engine today:** Psiclone, Elerium and the AG missiles never lose price (15 priced week-0 items not made by X-COM are frozen at base), weekly trickles are free and a dump costs one cut: cult-loot and weekly-sale cash is overstated beyond about six weeks (fix F-M1+F-M2, with F-M3). Cheap fast goods are overstated 1.2-9x; craft valuation ignores hull health (F-M7).

### 5.4 Defence strength by week and where the specimens come from

Early base defence is purchased gear and garrison (sections 01, 03). "Spec" = a specimen of the item must sit in the lab's base.

| Tier | Research | Gate | Gives | Model date (S / T) |
|---|---|---|---|---|
| D0 | none | none | purchased armour and weapons; basic Security Stations: 4 turrets each (580 HP, armour 57, Megapol Plasma Gun + 2 pods) | - |
| D1 | `LIGHT_DISRUPTOR_BEAM` 36k then `ADVANCED_SECURITY_STATION` 18k | beam item from a MANNED wreck | optional (F-6) | D14.9 / D17.9; D16.7 / D19.7 |
| D1b | `DISRUPTOR_GUN` 40k | Spec: guard kits from tier 6 or alien infantry at tactical score >= 1,000 | squad weapon; unlocks Devastator, Armor | kits below |
| D2 | `BIOLOGICAL_WARFARE` 26k; `TOXIN_TYPE_B` 15k | ABL built; Hyperworm/Multiworm/Egg autopsy; Life Cycle set for Toxin B (6.2) | Toxigun ignores shields; Toxin B is the practical ground win condition [GUIDE: UFO-CLA; WT 6.2] | BW D12.8 / D17.2; Toxin B D17.4 / D20.7 |
| D3 | `DEVASTATOR_CANNON` 38k, `PERSONAL_DISRUPTOR_SHIELD` 60k, `VORTEX_MINE` 46k | Spec: Devastator tier 8, shield and mine tier 9 | sustained fire; passive shield | 1.6-2.9 d each at 850 |
| D4 | `MEDIUM_DISRUPTOR_BEAM` 46k (L) -> `SMALL_DISRUPTION_SHIELD` 60k (L) + shield + gun -> `DISRUPTOR_ARMOR` 85k | Spec: a Medium Beam item (Fast Attack / Assault wreck) and a Small Disruption Shield item (a wreck recovered AFTER lifetime UFO-kill score reached 1,000, 7.3) | Armor 75/75/65/65/65 [GUIDE: AOW; WT 5]; chain 36k + 46k + 60k + 85k = 227k mh = 11 d at 850 | items by about D35 |

> **Engine today:** the engine's turrets are `AGENTTYPE_X-COM_BASE_TURRET_LASER/DISRUPTOR` with armour 0 and invented weapons (laser 26/95/42, disruptor 70/75/40; agent_types.xml:1039-1228, agent_equipment.xml:83-210) and no 36-minus-turrets budget (fixes BD04+TD10, TD8, TD9). In the original every employed person located at the base defends, unarmed staff included, and the soldier share is cut proportionally when the total exceeds `36 - min(12, 4 x finished stations)` (section 03).

**Guard kits.** Guards draw from a 12-row tier table (three independent `rand(9)` draws; empty rows give nothing): tier = start tech + (week counter - 1), **+3 if taken over**, clamp 0..11 [EXE TD6, T10]. A tier `t` above the start level is first reached at `D = 5.5 + 7 (t - start - 1)`; for a taken-over organisation use `t - 3`. Start levels [ENGINE data/difficulty4_patch organisations.xml]: **4** Diablo, Marsec, Osiron, Psyke, Solmine; **3** Government, Megapol, Cyberweb, Mutant Alliance, S.E.L.F.; **2** Cult of Sirius, General Metro, Nanotech, Sensovision, Superdynamics, Transtellar; **1** the rest. Per-guard shares from `equipment_sets.xml` HUMAN_6..11 [ENGINE; upper bounds for the grenade and equipment columns, because the extractor drops empty rows: TD6, SCORE-EQLEVEL]:

| Tier | Disruptor Gun | Boomeroid | Devastator | Vortex Mine | Personal Shield | Personal Cloaking |
|---|---:|---:|---:|---:|---:|---:|
| 6 | 20% | - | - | - | - | - |
| 7 | 30% | 14% | - | - | - | - |
| 8 | 30% | 29% | 10% | - | - | - |
| 9 | 40% | 25% | 10% | 12.5% | up to 100% | - |
| 10 | 40% | 25% | 20% | 25% | up to 67% | up to 33% |
| 11 | 40% | 22% | 30% | 33% | up to 80% | up to 40% |

The Cult (start 2, hostile in all 21 observed seeds [EXE T9, F19]) reaches tier 6 at D26.5, 7 at D33.5, 8 at D40.5, 9 at D47.5; a taken-over Cult is three tiers up (tier 6 at D5.5, 9 at D26.5). With about 9.55 guards per Cult raid [IC 2.4], P(at least one Disruptor Gun at tier 6) = 1 - 0.8^9.55 = 88%. Alien infantry carry Disruptor Guns from level 1 (lifetime Tactical Missions score >= 1,000), Boomeroids from level 2 (2,200), Devastators from level 3 (4,000), Vortex Mines and Shields from level 4 (6,600) [GUIDE: UFO-TL; ENGINE agent.cpp:162-168]; section 01 holds Cult raids below the first thresholds, so that route yields Disruptor Gun and Boomeroid at most.

> **Engine today:** the engine omits the +3 tier for taken-over organisations (agent.cpp:171-175, fix T10), so a taken-over tech-4 organisation (tier 7 at game start, 9 by D12.5 in the original) is a faster specimen source in the original than the engine shows.

| ID | WHEN (observable) | THEN | UNTIL |
|---|---|---|---|
| SP-1 | An organisation shows Hostile (<= -50) or taken over AND the date has reached the kit tier for the specimen (formula above) | Raid it (BUTTON_RAID) with a Cargo-Module carrier homed at the lab base (F-8) | specimen in the lab base |
| SP-2 | The organisation is not hostile | Do not raid it for a specimen unless stun-only: a kill of a non-hostile guard costs that organisation's view of X-COM -3 x 10 = -30 plus a ripple, and the launch-time -60 is wiped by the first relation write [EXE F11, F-M11] | - |

### 5.5 Trap list

Dimension Probe **craft** (25,000 mh; no seats, no schedule gain, 4.5); `PERSONAL_TELEPORTER` 130k, `PERSONAL_CLOAKING_FIELD` 100k, `LARGE_DISRUPTION_SHIELD` 105k, `CLOAKING_FIELD` 80k, `TELEPORTER` 150k (1.5-2.9 score per 1,000 mh); `BOOMEROID`, the Dimension Missile pair; the Real Alien Threat / Toxin C / Alien Gas chain (needs 12 species live); `OVERSPAWN_AUTOPSY`; the four hidden topics; selling an un-researched artifact (it prices at 0 and the specimen is gone [EXE F-M5; GUIDE: UFO-ART]) or the last spare of a gate item.

---

## 6. Live-capture plan

### 6.1 How capture works

| # | Rule | Evidence |
|---|---|---|
| K0.1 | All thirteen species can be captured. A unit is unconscious iff health is not 0 and stun < health; stun decays 1 per second. **Egg and Chrysalis count as captured without stun and never block victory.** Stunned, egg and chrysalis aliens credit their score to the tactical Total **whether or not they are stored** (and to the leadership base); the "Live Aliens captured" line is drawn | [EXE TD15-TD17; TACP `FUN_00022f90` loops 2-3] |
| K0.2 | A captured alien is **stored** only if the candidate craft carries a Bio-Transport Module (`alien_space` 20, the only module with any). Candidates: **building missions = any X-COM craft in the building** (the next craft is tried); **UFO recovery = only the dispatched craft** (failure loses aliens, bodies and cargo); **base defence needs no craft** (virtual carrier). Loading is unlimited | [EXE TD19 `FUN_000b21a8`, `FUN_000b27b4`, `FUN_000915a0`, `FUN_000b44a4`] |
| K0.3 | Unloading needs a finished Alien Containment in the craft's home base. Overflow: the guides say the original destroys the excess; not verified | [GUIDE: UFO-FAC]; [ENGINE vehicle.cpp:1828-1850, 4139-4160]; [UNBOUND TD20: `FUN_000b44a4`, `[0x1304CC]`] |
| K0.4 | **One live specimen feeds two topics:** starting a life-form topic consumes it, completing it adds the dead item (a Pod yields a live Brainsucker) | [ENGINE research.cpp:180-204, 360-380, 489-506] |
| K0.5 | The Bio-Transport Module (2x2) is not buyable; `MANUFACTURE_BIO-TRANSPORT_MODULE` (4,000 mh, $700) after the 9,000 mh research. Cargo Module ($300, 50 cargo) and Passenger Module ($400, +4 pax) are buyable from week 1 | [ENGINE economy.xml, vehicle_equipment.xml] |
| K0.6 | Containment units per live specimen: Anthropod, Brainsucker, Hyperworm, Micronoid, Popper, Skeletoid, Spitter 1; Chrysalis, Egg 2; Multiworm 5; Psimorph 5 (dead 12); Megaspawn 12; Queenspawn, Overspawn 20. One Alien Containment = 20 units ($7,000, 4 d, $400/wk); the start base has one | [ENGINE agent_equipment.xml `store_space`] |
| K0.7 | Leadership bonus = 0 when `30 x casualtyPoints > 1000` (four dead agents), else `base x (100 - 3C)/100` | [EXE TD18] |
| K0.8 | Superhuman aliens exceed the guides' base figures: +16% TU/health/reactions/stamina, +64% natural armour (Megaspawn 301 HP, Queenspawn 696 HP, Anthropod 69 HP); stun needed scales with health | [EXE TD1, TD2] |

> **Engine today:** UFO recovery pools every player vehicle within `VEHICLE_NEARBY_THRESHOLD` of the wreck (`AllowNearbyVehicleLootPickup`, default true; battle.cpp LIVE :3989-4018, HEAD :3982-4011), so a separate Bio craft parked near the wreck saves captures in the engine but not in the original (fix TD19); `liveAlienCaptured` and its score are withheld without storage (battle.cpp:2886-2905, TD17); no leadership clamp (TD18); engine Superhuman aliens are Novice-strength (TD1-TD3), so capture and kill rates are optimistic; base defence needs `getCapacityTotal(Aliens) > 0` (battle.cpp:3684-3700).

### 6.2 Minimal capture sets

One live specimen per species suffices (K0.4); a topic reads the specimens in its own lab's base (R-0d).

| Goal | Topics fed | Specimens | Units | First realistic source | Gate |
|---|---|---|---:|---|---|
| **G0** ABL + first score | one autopsy (6,000 mh: Hyperworm, Spitter, Popper; 7,000 Brainsucker; 8,000 Anthropod) -> ABL | any one corpse, live alien or pod, with a Bio-equipped craft in the mission | 1 | first alert building mission / extermination | module fitted from D1.4 |
| **G0b** Brainsucker set (100 + 120 + 120 score; the pod topic itself unlocks the ABL) | `BRAINSUCKER_PODS` 6k -> `BRAINSUCKER` 9k -> autopsy 7k | 1 pod (pick it up; it cannot hatch in inventory; pods cannot be killed) | 1 | Anthropod launchers of an early mission | module fitted |
| **G1** Toxigun + Toxin A | Genetic Structure (an Egg, Multiworm or Hyperworm autopsy) -> `BIOLOGICAL_WARFARE` (L) | one such corpse | 2 / 5 / 1 | **Transporter crash** | E3 |
| **G2** Toxin B | `MULTIWORM`, `HYPERWORM`, `CHRYSALIS` live + autopsy -> Life Cycle (needs GS) -> `TOXIN_TYPE_B` (needs BW) | 1 live Multiworm, Hyperworm, Chrysalis | 5 + 1 + 2 = **8** | Transporter crash: Multiworm, Chrysalis. **Hyperworm is in no UFO crew:** a violent Multiworm death (R-HW), the Sleeping Chamber (6 Hyperworms) or an infested building | E3; R-HW |
| **G3** species sweep | Egg, Anthropod, Spitter, Popper, Skeletoid, Brainsucker, Micronoid, Psimorph, Megaspawn | 1 live each | Egg 2, Psimorph 5, Megaspawn 12, others 1 | Transporter: Egg, Anthropod, Spitter, Popper; Skeletoid: Destroyer/Assault/Bomber/Escort wrecks; Micronoid: Bomber/Battleship/Mothership; Megaspawn: Escort/Battleship/Mothership/Maintenance Factory; Psimorph: Battleship/Mothership/Food Chamber | week 8+ |
| **G4** Alien Gas | `QUEENSPAWN` live + autopsy (25k + 15k) + Toxin C | the Queen (Spawning, raid 7), about 5 stun grenades; Large Bio lab | 20 | building 7 | optional |

**The Transporter crash is the convergence event.** Its crash crew is 15: Anthropod 4, Chrysalis 2, Multiworm 2, Egg 2, Popper 1, Spitter 4 [ENGINE `vehicle_types.xml` `crew_downed`; the EXE's static deposit crew lists 20 for a Transporter (BD03) and the EXE crash crew was not validated]. It supplies G0, G0b (pods), G1, the Multiworm and Chrysalis of G2 and four species of G3, and it is the only source of `UFO_TYPE_3`. E3 must be a **won** landing by a craft that itself carries a Bio Module, a Cargo Module and enough containment: a lost or aborted landing loses the wreck, specimens and loot at once [EXE RR-13, TD19].

**R-HW (Hyperworm risk).** No UFO crew contains a Hyperworm [ENGINE `vehicle_types.xml`; WRT 4.3]. The engine has `AEQUIPMENTTYPE_MULTIWORM_BURST` and guides report that a violent Multiworm death hatches Hyperworms; whether this fork does is unverified [MEASURE M-S2-5; UNBOUND, WT D10]. Plan for both: (a) in the Transporter battle kill one Multiworm with an explosive or incendiary, stun the other, then stun one hatched Hyperworm (Stun Grapple is 100% on it); (b) with no Hyperworm by D14, G2 waits for the Sleeping Chamber (D28-D32) or an infested building and Toxin B moves from D17-21 to about D35. Genetic Structure and Toxigun (G1) do not need a Hyperworm.

### 6.3 Carriers

Modules = general tiles / 4 (Hoverbike 1 tile, Phoenix 4, Valkyrie 12, Hawk 16, Explorer 12, Retaliator 16, Annihilator 30, Biotrans 32) [ENGINE `vehicle_types.xml`]. **For a UFO recovery the dispatched craft itself must carry the Bio Module, and a Cargo Module for loot** (K0.2; TACP cargo-loot rules are unbound, F-M14, so carry both).

| Craft | Modules | Pax | Fit | Use |
|---|---:|---:|---|---|
| Hoverbike | 0 | 2 | none | unmanned Probe/Scout wrecks only |
| Phoenix | 1 | 4 | Bio **or** Cargo | unmanned wrecks; second carrier in building missions only |
| **Valkyrie** | 3 | 8 | Bio + Cargo + Passenger (12 pax); the start Valkyrie already has Passenger + Cargo (section 01), add the Bio Module | primary recovery craft D1-D25 |
| Hawk Air Warrior (week 2) | 4 | 10 | Bio + Cargo + Passenger x2 (18 pax) | Transporter battle |
| **Biotrans** | 8 | 4 | default 4 Cargo + 4 Bio (no squad); raid fit swaps to Passenger: 4 + 4n pax, up to 36 | the gate craft (8.2) |

### 6.4 Capture method by species (tactics belong to section 03)

Guide figures; multiply HP by 1.16 and natural armour by 1.64 (K0.8). Egg, Chrysalis and Pods need no kill and no stun. Hyperworm: Stun Grapple (100%), avoid fire. Multiworm (340 HP), Psimorph (420), Megaspawn (260): shoot to 50-67% health, then gas or grapple. Popper: grapple 3-4 hits, stand a spare unit on it, never re-stun or leave it bleeding, no AP/HE/incendiary (it explodes). Skeletoid: grapple, blocked by a Personal Disruptor Shield. Micronoid: Stun Grapple only (gas 0%). Queenspawn: about 5 stun grenades [GUIDE: UFO-CLA, AOW, NKF via WT 6.3]. Tools: Megapol Stun Grapple (range about 3, blocked by shields) and Stun Grenade (humans 100%, androids and Micronoid 0%) [WT 5]; `oa_loadouts.py` adds a stunner extra (`:221-224`) but there is no capture workflow (HA 4.6, 4.15).

### 6.5 Capture rules

| ID | WHEN (observable) | THEN | UNTIL |
|---|---|---|---|
| K-0 | The first alert arrives (D0.0-D0.1) and no Bio Module exists (not before D1.4) | Do not delay the response for the module (section 03 decides it). Waiting 29 h lets Brainsuckers mature and raises infiltration; the price of not waiting is only the first mission's bio loot: first specimen D1.6 -> about D4.5, ABL D11.5 -> D14.5, hull unchanged (4.4) | - |
| K-1 | The module research completes (D0.9) | Manufacture **3** at once; fit one to the Valkyrie, one to the next recovery craft, hold one; verify Alien capacity > 0 on each craft screen | 3 fitted |
| K-2 | A mission that may yield specimens is about to launch | Confirm: (a) the **dispatched** craft (UFO recovery) or a craft inside the building shows a Bio Module, plus a Cargo Module for loot; (b) its home base has a finished Alien Containment; (c) free units >= those planned (6.2); (d) >= 1/3 of the squad has a Stun Grapple, all 2 stun grenades; (e) every base still has its section-03 garrison of loaded, disembarked soldiers (the benchmark lost 8 of 9 games to ungarrisoned bases, FF 5, 9) | launch |
| K-3 | The mission is a Transporter crash | Squad >= 10 armoured soldiers (Hawk 18 pax, Valkyrie 12); kill priority Anthropods, Spitters, Popper; capture 1 Multiworm, 1 Chrysalis (do not shoot it), 1 Hyperworm (R-HW), keep both Egg corpses; four dead agents zero the leadership bonus (K0.7). The squad is drawn beyond the garrison floor; if that leaves fewer than the section-03 minimum, send the minimum and accept a smaller set (Multiworm and Chrysalis first) | mission won, not retreated |
| K-4 | Squad strength is below one half, or a target needs more than 2 grapple cycles (starting values, [MEASURE M-S2-19]) | Kill it (Multiworm/Popper: clean energy kill). A retreat from a UFO ground mission loses the UFO, loot and unlock (U-9); retreat only to save the squad | - |
| K-5 | The craft returns with bio cargo | Check the containment screen; if the craft refuses (no containment), build it before the next launch; start the live topics within 6 game-hours in parallel Bio labs: Multiworm (15k), Chrysalis (12k), Hyperworm (9k) | all started |
| K-6 | Containment usage would exceed 100% | Research the largest specimens first (Multiworm 5); plan within capacity even where the engine accepts overflow (M-S2-8) | usage <= 100% |
| K-7 | The specimen's base differs from the next topic's lab base | Transfer (Transtellar non-hostile) >= 1 day before the lab frees, or home the capture craft at the lab base (F-8) | topic started |
| K-8 | A base-defence battle is fought at a base with a finished Alien Containment | The winner gets bio loot without a module (K0.2): a free specimen source; do not retreat | - |
| K-9 | A capture is impossible (no module or containment) | Stunning still credits the Total (K0.1); Eggs and Chrysalis need no stun and score at the mission end | - |

Cost: 3 x $700 modules, $7,000 for a second containment if the Transporter set is kept whole (8 live units + about 19 corpse units), stun kit about $100 per soldier [DERIVED]. Without K-1..K-3 the harness repeats the benchmark: module researched 31/31 runs, manufactured 0/31, ABL never built, no Toxigun (HA D8, 4.1.4).

---

## 7. UFO recovery doctrine

### 7.1 The ten types

Counts are lifetime growth per type and the last week with an addition [EXE UFO_growth table]; a destroyed UFO is not replaced. Crash HP is the EXE `crash_constitution`. **Withdrawal thresholds** are `constitution x type_percent[slot]/100` for the three package slots (HP; a band exists only if the threshold exceeds the crash HP) [EXE F7]. (f) = loot present only if the score flag was set at recovery time (U-7). Wreck life is the engine's prior, `5 x U(a,b)` minutes: Probe/Scout 10-90, Transporter/Fast Attack/Destroyer/Escort 15-120, Bomber 10-120, Assault 15-180, Battleship/Mothership 60-240 [ENGINE vehiclemission.cpp:48-59; UNBOUND, the code says "TODO: find a way how to extract from the game data"].

| T | Craft | HP / crash HP | Thresholds (HP); slots with a band | Recovery | Loot | Ever / last wk |
|---:|---|---|---|---|---|---|
| 1 | Probe | 80 / 15 | 24, 32; 2 of 2 | **unmanned** | none in the original | 14 / wk 4 |
| 2 | Scout | 120 / 35 | 12, 24, 36, 48; 5 of 7 | **unmanned** | none in the original | 25 / wk 13 |
| 3 | **Transporter** | 400 / 150 | 80, 120, 160; **1 of 5** | manned `51ufo1` | Light Beam, Small Shield(f) | **6 / wk 4** (+3, +2, +1) |
| 4 | Fast Attack | 500 / 150 | 150, 200; 3 of 5 | manned `52ufo2` | Medium Beam, Small Shield(f) | 9 / wk 13 |
| 5 | Destroyer | 600 / 200 | 120, 180, 240; 2 of 11 | manned `53ufo3` | Inversion Bomb Launcher, Small Shield(f), Teleporter(f) | 5 / wk 7 |
| 6 | Assault Ship | 850 / 200 | 170, 255; 8 of 11 | manned `54ufo4` | Medium Beam, Small Shield(f), Teleporter(f), Cloaking(f) | 8 / wk 11 |
| 7 | Bomber | 700 / 100 | 140, 210, 280, 350; 8 of 8 | manned `55ufo5` | Light Beam, Multi-Bomb Launcher, Large Shield(f), Teleporter(f), Cloaking(f) | 7 / wk 12, then +1 weekly from wk 16 |
| 8 | Escort | 500 / 80 | follows its companion | manned `56ufo6` | Stasis Launcher, Large Shield(f), Cloaking(f), Teleporter(f) | 6 / wk 14, then weekly |
| 9 | Battleship | 1,800 / 300 | 180, 360, 540, 720; 8 of 9 | manned `57ufo7` | Heavy Beam, Inversion Bomb Launcher, Large Shield(f), Teleporter(f) | 5 / wk 15, then weekly |
| 10 | Mothership | 2,800 / 350 | 560, 840; 10 of 10 | manned `58ufo8` | Heavy Beam, Multi-Bomb, Stasis, Large Shield(f), Teleporter(f) | 3 / wk 14, then weekly |

Escort slots and role-9 (wave) ships do not withdraw by damage [EXE F7, F5]. The week-2 package (Monday D5.5) is two Transporters with two Fast Attack escorts, so the first manned battle is usually a Transporter or a Fast Attack Ship. After E1 and E3, by research value: Fast Attack Ship (Medium Beam and shield for the armour chain, 5.4), Assault Ship, then the Skeletoid/Micronoid/Megaspawn/Psimorph carriers for the Real Alien Threat (week 8+).

### 7.2 Manned versus unmanned recovery (RR-12, RR-13)

Only the Probe and Scout are unmanned (`FUN_000b300c` returns "no mission needed" for types 0-1). Recovering one sets **only that type's topic flag** (`FUN_000ac08c`, file 0x10E82C) and removes the craft: no system flag, no loot (loot half medium confidence; `FUN_0003a910` not checked for grants). The three system topics (records 75-77) have one writer, `FUN_000b32ac`, in the manned UFO-mission branch and **only if the result flag `[0x244084]` is 1 (won)**; the type topic and the loot loop sit under the same guard; the craft is removed regardless [EXE RR-12, RR-13; "won" on a retreat is inferred, medium].

| Consequence | Rule |
|---|---|
| The earliest E1 is the first manned battle won | Prepare from D5.4 (U-1). Probe/Scout wrecks are secondary: recover them only with a spare craft; they give `UFO_TYPE_1/2` (+160 score, 18,000 mh of idle Physics time) and nothing else |
| A manned landing lost or aborted unlocks nothing and **loses the UFO** | Never launch a manned ground mission you cannot win; there are 6 Transporters |
| Wreck equipment comes only from manned wrecks | the Light Beam item for `LIGHT_DISRUPTOR_BEAM` comes from a Transporter |
| Benchmark predicate for E1/E3 | "won debrief + the topic is listed", not "a recovery happened" |

> **Engine today:** all ten types carry the three system unlocks plus the type topic (extract_vehicles.cpp:147-156); a Probe/Scout wreck force-completes all four and hands its equipment to the craft's cargo (cityview.cpp LIVE :4818-4825, HEAD :4743-4750; vehicle.cpp LIVE :1660, HEAD :1635); a manned recovery force-completes the list with no `playerWon` test (battle.cpp LIVE :3646-3656, HEAD :3639-3649), so an aborted landing still unlocks. The chain starts 5-13 days early and the harness's own routing steers toward battle-free Probe/Scout wrecks (gamestateintrospect.cpp:949-970). Fixes RR-12 (system unlocks only for types >= 2, drop unmanned loot, regenerate `base_gamestate`) and RR-13 (`playerWon` guard). Do not retreat from a manned landing to farm the unlock; read the Research list after the first Probe/Scout recovery as the fix check (HR-5).

### 7.3 Doctrine

| ID | WHEN | THEN | UNTIL |
|---|---|---|---|
| U-0 | D0, always | Keep a recovery craft fitted before the first manned UFO exists: Valkyrie (or Hawk), >= 1 Soldier-role agent aboard, Bio + Cargo modules, hangared with the interceptors. A recovery order is refused for a craft with no soldier aboard (the benchmark's 9 "no crewed craft could be selected"; its 11 click refusals at the window centre are a separate defect, HA D9). A soldier sitting in the craft is crew, not garrison [ENGINE vehiclemission.cpp LIVE :3588-3640 `canRecoverVehicle`; EXE eligibility not validated] | always |
| U-1 | Monday D5.5 has passed and the UFO list shows a UFO that is not a Probe or Scout | **Readiness is standing from D5.4:** a capture-capable squad aloft-ready (>= 10 armoured soldiers, Bio + Cargo craft with seats) in addition to the garrison. Engage by section 03's rules, crash the UFO (U-5), win the ground mission. Once a UFO is falling or crashed, every hit taking it to <= 0 destroys the wreck and missiles in flight keep arriving: order the remaining craft off; one Bolter/Lancer hit (6-24) cannot take a Scout or Probe from above its crash threshold to <= 0, so a Bolter Phoenix is the safe finisher [DERIVED; ENGINE vehicle.cpp LIVE :2729-2737, HEAD :2704-2712; `web_city_air.md` 4]. Launch the recovery craft with the interceptors; bring UFOs down over open ground | wreck recovered |
| U-2 | A wreck exists (smoke, "UFO crashed") | Order recovery from the nearest soldier-carrying craft. ETA <= 25 game-minutes for a Probe/Scout (the prior leaves 81% of wrecks), <= 30 for Transporter/Fast Attack/Destroyer/Escort (86%) and Assault Ship (91%), any for Battleship/Mothership; pre-position on the interceptors' side [DERIVED from the prior; M-S2-3] | recovery begins |
| U-3 | A manned ground mission ends | Open the Physics "New Project" list within one game hour. Won and the three system topics listed: E1 done. Not listed: it was not scored a win; log won/lost/aborted from the debrief and treat the next manned UFO as E1 [EXE RR-13] | decided |
| U-4 | A Transporter appears (3 on Monday D5.5, +2 D12.5, +1 D19.5; none after) | **Recovery target first, kill target second;** engage with U-1 readiness. Deposit clock: a Transporter deposit is about 20 aliens, species split unknown (EXE deposit crew table, BD03), so it lies between the +2/h case (10 Anthropods in the Senate) and the +7/h case (27 aliens): the original flips the Government in a median 87 h to 25 h (and 236 h at +1/h), the engine in 19 h to 9 h (34 h at +1/h) [EXE T1, T2, T6; RF 2.1, DERIVED]. There is time to engage and clear, but every UFO that leaves costs floor(score/4) (Transporter -37). Hold fire only in transit and not over a building; engage the moment it hovers over any building; never hold fire after D19.5 [FF 3, 4] | one Transporter wreck recovered |
| U-5 | A UFO is engaged | Damage it until it crashes (below its crash HP, above 0). Inside its band (7.1) it turns for the nearest gate within about half a game second: keep every shooter on it until the wreck message. An escape costs floor(score/4), loses the wreck and returns the ship to the pool. Bands: Transporter 1 of 5 package slots (150-159 HP), Fast Attack 3 of 5, Destroyer 2 of 11 [EXE F7, F8, SCORE-C5] | wreck or kill |
| U-6 | `UFO_TYPE_3` is listed | Further Transporters are optional recoveries (a second specimen set) at the price of a 15-20 alien battle; engagement is not optional (U-4). Stop planning recoveries when none appears on two consecutive Mondays after D26.5 | - |
| U-7 | The Score screen's UFOs Shot Down total (this week + previous) crosses 1,000 / 2,000 / 4,000 / 8,000 | Flags latch at the next midnight and never clear: new UFOs carry the Small Disruption Shield / Large Shield / Cloaking Field / Teleporter, **and a wreck recovered after that midnight yields those items as loot** (the type's static module list filtered by the flags at recovery time). Kill scores: Probe 50, Scout 100, Transporter 150, Fast Attack 200, Destroyer 250, Assault 300, Bomber 300, Escort 250, Battleship 500, Mothership 700; 1,000 = 20 Probes, 10 Scouts or 2 Battleships. Take the Small Shield item from the first recovery after the 1,000 mark [EXE F9, SCORE-UNLOCK-CRAFT `FUN_00097b90` 0xFA234, `FUN_000b3e84`] | item in stores |
| U-8 | An Assault Ship, Destroyer, Battleship or Bomber is engaged and the squad cannot take its crew (14-30 aliens) | Engage by section 03's test and let it crash; **do not recover** unless the squad meets the section-03 minimum: a lost landing loses the squad and the UFO, and the tactical score gained arms the aliens (Disruptor Gun from 1,000, Boomeroid 2,200) [ENGINE agent.cpp:162-168; EXE SCORE-EQLEVEL]. After E1 the only non-optional recovery is the first Transporter | - |
| U-9 | A manned recovery is about to be lost (squad below one quarter, objective unreachable; starting value, [MEASURE M-S2-19]) | Retreating saves the squad only: the UFO, loot and unlock are lost [EXE RR-13] | - |
| U-10 | A manned wreck is recovered | Its equipment arrives as cargo at the recovering craft's home base: confirm it is the base whose lab will research `LIGHT_DISRUPTOR_BEAM`; move specimens by F-8 [ENGINE vehicle.cpp:1635-1654] | item in lab base |

> **Engine today:** (1) The engine charges Damage to City for any falling or crashing vehicle that plows through building scenery (vehicle.cpp LIVE :1027-1030, HEAD :1002-1005; the original charges only the Overspawn, SCORE-C6), -5 relation per stray hit and -25 per destroyed stray tile (original 0 and -5) and -30 for a downed Government craft (original -10): relation budgets are 2-5x tighter; bring crashes down away from buildings and never shoot Government or Megapol craft [EXE F02, F05; fixes F02+F05, SCORE-C6]. (2) Engine escort and attack-list UFOs launch without the gear gate (34% of launch slots) and engine loot is a creation-time snapshot (F9). (3) The engine withdraws by the role table (10 or 30 percent in 16 saves), not `type_percent` (F7). (4) The engine flips an organisation at any infiltration above 0 every 125 minutes: a Transporter deposit takes the Government in a median 19 h at +2 per hour (original about 87 h): treat any nonzero Government infestation as imminent until T1+T2 land. (5) The engine force-detects crewed X-COM-owned buildings, so an infested base bills -30 about every 7 h, while big single-species stacks in third-party buildings are rarely detected (original: the opposite; SCORE-C2).

### 7.4 Windows in game days

E1: earliest about D5.6; each day later moves the hull 1:1 (4.4). Transporters: wk2 (D5.5, +3), wk4 (D19.5, +1), none after; survivors in the dimension can still be sent and the strongest affordable package is chosen first from week 5, so the weekly odds from D26.5 are unmeasured: log the UFO list every Monday [EXE `FUN_0006cfb4` package fit; M-S2-17]. E3 costs the hull nothing if won by D18. Fast Attack Ships D5.5-D26.5, two a week early; Destroyers D19.5-D40.5. If every Transporter is destroyed or never engaged, `UFO_TYPE_3` is unobtainable, no gate-capable troop carrier can exist and the game cannot be won [ENGINE research.xml, vehicle_types.xml; GUIDE: UFOpaedia Transporter page via WAC 2]; a UFO shot down in the alien dimension cannot be recovered [GUIDE: GOG-UFO]. The benchmark records "no Transporter recovered by D30" as failed and keeps a rolling save from just before Monday #1 (HR-15..17). Observables: the UFO name on the list, the "UFO crashed" message and smoke, the craft screen (crew, pax, modules), the Physics list after the debrief, the Score screen's UFOs Shot Down total (`funds.ufos_downed` is score points, FF 11).

---

## 8. The alien-dimension chain

### 8.1 Binding and gating of the ten topics

Building `f` is gated on research record `88 + f` (`FUN_000b4678`, message "Our Agents are unable to find an entrance..."). Winning building `f` unlocks record `88 + succ(f)` (`FUN_000b4538`), `succ` = 4->2, 2->6, 6->8, 8->0, 0->7, 7->1, 1->5, 5->3, 3->9; `f` 9 writes nothing. Record 92 (Sleeping, the first raid) is the only one of 88-97 with a topic prerequisite: **The Alien Dimension** (record 1, 6,000 mh, Quantum), made available by the first move into the alien city (`FUN_0006e098` city switch or the gate-arrival handler `FUN_0003a910`). Hours rise 30,000 + 2,000k, total 390,000 [EXE VC-2, VC-3, RR-14].

| Step k | Building (f) | Record | Topic mh | Topic score | Raid score (C7) | Winning it unlocks | Also |
|---:|---|---:|---:|---:|---:|---|---|
| 0 | Sleeping Chamber (4) | 92 | 30,000, **needs The Alien Dimension** | 300 | 300 | Food | |
| 1 | Food Chamber (2) | 90 | 32,000 | 320 | 300 | Alien Farm | **frees the Mutant Alliance** (below) |
| 2 | Alien Farm (6) | 94 | 34,000 | 340 | 250 | Maintenance | |
| 3 | Maintenance Factory (8) | 96 | 36,000 | 360 | 300 | Incubator | |
| 4 | Incubator Chamber (0) | 88 | 38,000 | 380 | 400 | Control Chamber | |
| 5 | **Control Chamber (7)** | 95 | 40,000 | 400 | 350 | Spawning | **stops UFO growth; arms the wave (8.6)** |
| 6 | Spawning Chamber (1) | 89 | 42,000 | 420 | 400 | Organic Factory | objective check is the Queen |
| 7 | Organic Factory (5) | 93 | 44,000 | 440 | 250 | Megapod | no growth effect |
| 8 | Megapod Chamber (3) | 91 | 46,000 | 460 | 500 | Gate Generator | |
| 9 | Gate Generator (9) | 97 | 48,000 | 480 | 500 | - | **victory** |

Topic scores sum to 3,900 and raid scores to 3,550 (category 7 counts in the weekly and lifetime totals [EXE SCORE-C7]). After the first crossing the first raid costs The Alien Dimension (15 h small lab, 7 h AQPL) then 30,000 mh in the ABL (35 h), serial. A **won Food Chamber raid frees the Mutant Alliance** even if it was never taken over (`FUN_000b32ac` case `f` 2): taken-over flag cleared, relation +100 both ways, Alliance -> Aliens -100, infiltration[24] := 0; Hybrid hires return while its relation to X-COM is >= 0. It can be retaken only once raw infiltration exceeds 100 again [EXE T4, F14].

> **Engine today:** the engine binds `RESEARCH_ALIEN_BUILDING_i` by albuild file position (extract_buildings.cpp:76, 82-94): the same order, wrong hours, prerequisite and score per step. Step 0 is 38,000 mh with no prerequisite, offered on day 0; The Alien Dimension gates the fifth step; hours run 38/42/32/46/30/44/34/40/36/48k, so research debt peaks at +26,000 mh after the fourth step (research.xml:1411-1480). Fix VC-2+VC-3+RR-14 in one change (`accessTopic = RESEARCH_ALIEN_BUILDING_{function_idx}`, unlocks from `succ`, keep `victory` on `f` 9; update tests/test_city_rules.cpp:1278-1329, HEAD only). The Food Chamber reset is missing (fix T4/F14: in the RaidAliens won branch set `takenOver` false and `infiltrationValue` 0 with the relations above and no ripple; clearing the flag alone is undone within 125 minutes).

### 8.2 The crossing craft and standing orders

| Item | Rule | Evidence |
|---|---|---|
| Craft | Crosses iff vehicle-data `+0x1A` is non-zero: the ten alien craft, Dimension Probe, Bio-Transport, Explorer, Retaliator, Annihilator; not Valkyrie, Hawk or other interceptors; an incapable craft at a gate does nothing; the Dimension Shifter is read only by UI text. The Biotrans (600 HP, speed 15) is the gate craft; the Probe has no seats | [EXE VC-6, VC-7 `FUN_00060e80` 0xC3524] |
| Fit | 8 slots: 5 Passenger + 1 Cargo + 1 Bio + 1 slot for a Small Disruption Shield if one fits a general slot (unverified), else a 6th Passenger: 4 + 4 x 5 = **24 pax** (28 with the 6th). Passenger Modules are $400 (week 1) from General Metro: keep it out of the Hostile band (buying is refused when the manufacturer's view of X-COM is <= -50) | [ENGINE `vehicle_types.xml`, `economy.xml`]; [EXE F-M5, F-M15] |
| Gates | Three portals in the human city; re-read their positions every Monday; right-click a portal (the nearest is used) | [ENGINE city.cpp:333, 383-410, 495-545]; [GUIDE: UFO-DG] |
| Arrival | Destination = the first building in array order with record `88 + f` complete and not destroyed; with none (always at the first crossing) a one-shot goto to a random point (-5..+4 tiles) | [EXE VC-12 `FUN_0006eea4` 0xD1548] |
| Return, timing | **Unbound:** no countdown found in the EXE; the guides say midnight arrival and at least a day to return. Time in the dimension may not advance the human calendar (M11). Plan a day of return per sortie and measure | [EXE VC-13, RR-17; UNBOUND `FUN_00010908`, `FUN_000114cc`, `FUN_000105b8`]; [GUIDE: UFO-DG]; [MEASURE M-S2-6] |
| Fleet | A persistent per-type **stock**: kills in the dimension, launches, incursions and gate crossings decrement it, weekly growth adds. On every entry the game respawns `stock - live craft` at **full health** in new positions: no wound-and-return fights; the stock shrinks with every kill | [EXE VC-16] |
| Threat | Patrolling UFOs attack X-COM craft in the dimension; a downed transport kills everyone aboard: arrive and land at once | [GUIDE: UFO-AD; WAC 6.3] |
| Landing | Issue the landing order yourself (BUTTON_GOTO_BUILDING); do not unload agents before the mission starts; do not launch right after a mission ends (the building collapses) | [GUIDE: UFO-AB]; [EXE VC-12] |

> **Engine today:** `CrashingDimensionGate` defaults true (options.cpp:473-476) and crashes an incapable craft: set it false for the benchmark. The engine flips the view at the next midnight after the craft is through (cityview.cpp:2846-2891), returns it 1-2 h after a won raid (vehiclemission.cpp LIVE :2693-2708) and advances all clocks meanwhile; dimension UFOs keep damage and positions across visits (gamestate.cpp:331-337; fix VC-16: heal on switch).

| ID | WHEN | THEN | UNTIL |
|---|---|---|---|
| G-1 | `BIO-TRANSPORT` researched | Buy Passenger Modules (5-6 x $400); keep 1 Cargo + 1 Bio; refit the **first** hull the moment it exists | refit done |
| G-2 | The Biotrans exists and The Alien Dimension is not yet listed | **Token crossing:** send it with 0-1 soldiers to the nearest portal so it arrives, then order it back to a portal | `THE_ALIEN_DIMENSION` is in the Physics list |
| G-3 | `THE_ALIEN_DIMENSION` is listed | Start it in the best free Quantum lab (AQPL: 7 h), then the step-0 topic in the ABL (35 h at 850) | topic complete |
| G-4 | A step topic is complete AND the squad is boarded (the section-03 minimum after garrisons, up to 24-28 seats; 20+ is a target, not a gate) AND the 8.5 kit is issued AND the section-03 garrison is present, loaded and disembarked in every base | Order the craft to the nearest portal; log hours to the view flip and to the won raid (M-S2-6) | arrival |
| G-5 | In the alien city | Order the landing on the building whose topic is complete; never attack buildings from the air (a building with no active tile cannot be raided and the chain would soft-lock) | mission starts |
| G-6 | A raid is won (debrief success, building collapsed) | **Start the next `ALIEN_BUILDING` topic within 3 game hours** in the ABL; refit, rearm and replace casualties during its 1.5-2.4 days so departure = research completion | next departure |
| G-7 | A raid is lost or aborted before the objective is destroyed | The building survives (no collapse, no unlock) and the topic stays complete; do not re-launch until the squad is rebuilt | - |
| G-8 | Each Monday | Re-read portal positions; count UFOs near the portals before dispatch | - |

### 8.3 The ten buildings (raid order)

Crews are preset (`buildings.xml`); intervals and pad counts are fork data (`reinforcementsInterval`, equal to UFOpaedia 10/10, **absent from both EXEs** [UNBOUND TD14: UFO2P tactical-start writer]); objectives are from the per-building pages [GUIDE: UFO-AB], with the EXE objective rows (TACP table file 0x2D1A4C, stride 0x58) for `f` 9, 3, 5, 8 and the Queen check for `f` 1. In the original a reinforcement is one unit per timer expiry, drawn from the species present among Anthropod, Skeletoid, Spitter, Popper, spawned on an intact 2x2 ground pad (tiles 208/209 over 210/211), scaled like any alien [EXE TD12, TD13]. Superhuman scales every non-X-COM unit [EXE TD1-TD3].

| # | Building (map) | Crew (total) | Interval, pads | Objective | Specimens |
|---:|---|---|---|---|---|
| 1 | Sleeping (43sleep) | Anthropod 6, Hyperworm 6, Micronoid 2, Skeletoid 6, Spitter 2 (22) | 40 s, 12 | 4 sleeping units (explode on damage; floor craters) | **Hyperworm x6, Micronoid x2** |
| 2 | Food (41food) | Anthropod 12, Psimorph 2, Skeletoid 6 (20) | 35 s, 9 | 7 bulbs, each a radius-8 incendiary; 3 guarded by 2 Psimorphs | Psimorph x2 |
| 3 | Alien Farm (45farm) | Anthropod 6, Popper 6, Psimorph 2, Skeletoid 12 (26) | **12 s**, 12 | 3 clusters of white cubes (all must go) | Popper x6, Psimorph |
| 4 | Maintenance (47maint) | Anthropod 6, Megaspawn 2, Popper 6, Skeletoid 6 (20) | 30 s, 10 | 13 "heart" units in 3 areas; EXE `f` 8 = {142,143,144} | Megaspawn x2 |
| 5 | Incubator (39incub) | Anthropod 6, Multiworm 4, Skeletoid 12, Spitter 2 (24) | 20 s, 10 | 14 incubators; Skeletoids with Dimension Missile Launchers, stun-gas barrels | Multiworm x4 |
| 6 | **Control Chamber** (46contrl) | Anthropod 12, Skeletoid 12, Spitter 2 (26) | **10 s**, 15 | 32 brain modules | - |
| 7 | Spawning (40spawn) | Anthropod 6, Megaspawn 3, Egg 6, Psimorph 2, Queenspawn 1, Skeletoid 6 (24) | 18 s, 12 | the Queen + eggs; forcefield enclave | Queen (Alien Gas), Megaspawn x3, Egg |
| 8 | Organic Factory (44organ) | Anthropod 12, Psimorph 2, Skeletoid 12, Spitter 2 (28) | 20 s, 12 | 10 embryonic-UFO mushrooms; EXE `f` 5 = {138..158} | Psimorph |
| 9 | Megapod (42megapd) | Anthropod 12, Megaspawn 2, Psimorph 2, Skeletoid 12, Spitter 2 (30) | **10 s**, 8 | 42 pods among 111 plants; EXE `f` 3 = {213..216, 223..226}; turrets 4+16+4+2 | Megaspawn x2, Psimorph |
| 10 | **Gate Generator** (48gate) | Megaspawn 6, Psimorph 6, Skeletoid 18 (30) | **5 s**, 16 | 18 generators + laser web; **EXE `f` 9 = {76,77,78,138}**; destroying them wins | - |


### 8.4 Raid success: destroy the objective tiles, then leave (VC-1)

**Rule** [EXE VC-1; ADJ VC-1]. A raid in the dimension is a type-1 mission. TACP `FUN_00022f90` sets the result flag to 1 when X-COM has live units and no opponents, then **clears it to 0 if the building's objective tiles are not destroyed** (`FUN_00025310`), so the mission goes on with every alien dead. `FUN_00015494` sets success unconditionally when the owner is the Aliens, the type is 1 and the objective is complete, on every mission exit. Hence: (a) **destroying the objective tiles and then evacuating succeeds**; (b) killing every alien with the objective intact does not; (c) if X-COM has no units left but the objective is already complete the result still upgrades to success. On success `FUN_000b32ac` credits C7, collapses the building, unlocks the next topic and, only for `f` 9, shows the victory screen; there is no all-buildings test. Which weapons destroy each building's objective tiles, and how fast, is unmeasured [MEASURE M-S2-9]; pad hardness is not a guide to it.

**Planning consequence.** WHEN the building-disabled message shows THEN the squad may evacuate; the remaining pressure is one reinforcement per interval, not four. There is no pad gate and no mine requirement in the original.

> **Engine today (VC-1, TD12; high):**
>
> - **Success predicate.** The engine needs `Battle::playerWon`: a conscious player unit and no conscious hostile 'normal' unit (battle.cpp:2235/2240; HEAD :3573, :3652-3654, LIVE :3573, :3659-3661). Objective tiles never feed it (`tryDisableBuilding`, :2272ff) and a retreated unit is not conscious. **It wins a raid only by killing every alien while a unit stands with the objective intact, and cannot win by destroying it and evacuating: no unlock, collapse, C7 score or victory, for all ten buildings.** Four objective sets differ from the EXE rows (extractors.cpp:121-131: Gate engine `{71,73..85}` vs EXE `{76,77,78,138}`; `f` 3, 5, 8 as in 8.3).
> - **Reinforcements.** Four randoms per interval at random spawner parts (battle.cpp:1974-1999, 2333-2385, with a FIXME) until alive hostiles reach 36: about 4x the original (TD12).
> - **Pads on LIVE** [ENGINE tilesets `39incub`..`48gate`, battlemappart.cpp:200-218, battleexplosion.cpp:23-29, 248-295; DERIVED]. Every placed pad tile on the nine destructible maps has constitution 120 and is destroyed only by damage strictly greater than that. The Vortex Mine (150) is the only X-COM item above it (Marsec High Explosive 120 is not); one mine on a corner tile clears a 4-tile site at 98 of 100 sites, so raids 3, 6, 9 (12 s, 10 s, 10 s) need about one mine per site (12 / 15 / 8) while raids 1-2 (40 s, 35 s) may fall to attrition. The Gate's pads (constitution 255) cannot be destroyed: it is won only by clearing the field within one 5-second interval and holding fire and stance for the 4 battle seconds of `missionEndTimer`.
> - **Crash hazard H-PAD.** `spawnReinforcements` draws 4 spawn tiles without replacement from intact pad tiles with no conscious unit on them; with 1-3 such tiles the second draw calls `pickRandom` on an empty set and the uncaught `std::runtime_error` terminates the process (battle.cpp:2366-2372; library/xorshift.h:178-190; crashhandler.cpp:36-56). WHEN two or fewer pad sites (4 tiles each) remain THEN make a battle save first (battle saves load paused), keep soldiers off pad tiles and fire the last two sites' charges in one fuse window; "Trying to randomize within empty container" or "FATAL" in the log means reload.
> - **Fix.** VC-1: success = `buildingDisabled || tryDisableBuilding()` for unlocks, collapse, C7 and victory; keep `playerWon` for loot; do not end the battle on all-hostiles-dead while the objective is open; move LIVE's C7 block (battle.cpp:3578-3587) to the same gate. With TD12 and the objective-set corrections this removes the callout; until then score the chain to raid 2 on LIVE and treat later raids as fix-gated.

### 8.5 Research and kit before each building

Squad and kit are section 03's; this fixes the research prerequisites from the dated, specimen-limited capabilities of 5.4.

| Tier | Buildings | In stock before launch | Earliest model date | Failure if missing |
|---|---|---|---|---|
| **G-A** | 1-2 | Biotrans with capture modules; Toxigun + Toxin A (BW) and ideally Toxin B; best purchasable armour; the section-03 squad minimum (>= 10 soldiers incl. >= 4 androids, after garrisons) up to 24-28 seats; Disruptor Gun if a specimen came | BW D12.8 (S), Toxin B D17.4 | Sleeping/Food crews (20-22) overrun a conventional squad |
| **G-B** | 3-6 | G-A + Devastators and Personal Shields as specimens allow (tier 8-9 kits); explosives suited to the objective once M-S2-9 reports | about D40 (Cult tier 8-9) or earlier from taken-over organisations | the Farm (12 s) and Control (10 s) streams; Skeletoid missile launchers at #5 |
| **G-C** | 7-10 | G-B + Disruptor Armor if it exists (227k-mh chain, 5.4 D4) | D46+ | Megapod turrets (#9) and the 30-strong Gate garrison |

The armour chain is the research expression of "do not lose 73 of 98 soldiers" (the Let's Play); if its items never come G-C degrades to purchasable armour + shields + Toxin B/C; log the G-tier at every launch. **Request to section 03:** do not gate every dimension raid on "Disruptor armour issued" (a 327,000 mh chain, realistic D46+): that would move the first raid from about D28 to D46 (about +18 days on every victory date). Toxigun + Toxin A is the minimum for buildings 1-2.

### 8.6 UFO growth stops at the Control Chamber, and the wave that follows

**Rule** [EXE UFO_GROWTH_STOP, RR-15, VC-9, VC-10, F4, F5]. Weekly growth (`FUN_0006edac`, every Monday after funding and once at new game) starts with `if byte [0xDE62A] != 0 skip`. That byte is written only by `FUN_000b4538` case 7, i.e. winning the **Control Chamber (`f` 7, 6th raid)**, and never cleared. Otherwise `pool[t] = min(pool[t] + row[t], cap[t])`, `row = clamp(week - 1, 0, 15)`; a launch removes its package, a withdrawn UFO rejoins, a destroyed UFO is gone until growth refills; growth never removes ships above the cap. The Organic Factory (8th raid) has no growth effect. Weekly additions by type (Probe, Scout, Transporter, Fast Attack, Destroyer, Assault, Bomber, Escort, Battleship, Mothership; caps 15, 15, 6, 6, 6, 6, 6, 6, 4, 2) [EXE table = engine; UFO-INC]:

| Week | Additions | Week | Additions |
|---:|---|---:|---|
| 1 | Probe 9, Scout 9 | 9 | Escort 1, Battleship 1 |
| 2 | Probe 2, Scout 4, **Transporter 3**, Fast Attack 2 | 10 | Scout 1, Bomber 1 |
| 3 | Probe 2, Scout 4, **Transporter 2**, Fast Attack 2, Assault 3 | 11 | Fast Attack 1, Assault 1, Battleship 1, Mothership 1 |
| 4 | Probe 1, Scout 2, **Transporter 1**, Fast Attack 1, Destroyer 2, Assault 2 | 12 | Bomber 1, Escort 1 |
| 5 | Scout 1, Fast Attack 1, Destroyer 1, Assault 1, Bomber 2 | 13 | Scout 1, Fast Attack 1, Battleship 1 |
| 6 | Scout 1, Fast Attack 1, Destroyer 1, Bomber 1, Escort 1 | 14 | Escort 1, Mothership 1 |
| 7 | Scout 1, Destroyer 1, Bomber 1, Escort 1, Battleship 1 | 15 | Battleship 1 |
| 8 | Scout 1, Assault 1, Bomber 1, Escort 1, Mothership 1 | 16+ | Bomber 1, Escort 1, Battleship 1, Mothership 1 weekly |

The same win sets `[0xF3C78] = 1`. At the next invasion-timer expiry in the human city `FUN_0006cfb4` calls `FUN_00070938` instead of picking a package: up to **20** ships round-robin over the nine non-Probe types from the dimension stock, and **if fewer than 10 are gathered the flag clears and nothing launches**. Otherwise `FUN_0006d384` spawns them one per step when the gate volume is clear, each role 9 (never withdraws), mission 4 (a random building not allied to the Aliens whose owner is not taken over, X-COM bases included); a Mothership has a 50% chance of promotion to role 10. One shot. The timer is `24 h + rand(0..2820 min) + rand(0..3600 s)` (24-72 h; about half if the second drain of F6 is real). What the ships do at the building, whether the stock is reduced for assault craft and the targets are undecoded [UNBOUND `FUN_0006d994`, role bytes in `FUN_0006da88`, writers of `0x143D40`].

> **Engine today:** the engine stops growth at the **Organic Factory** (8th raid) and fails open if none exists (`craftFactoryIntact`, ufogrowth.cpp:51-70; gamestate.cpp LIVE :1626, HEAD :1593; justification only UFOpaedia prose, ufogrowth.h:21-23), refilling the fleet two raids (about 13 days) longer. It has **no wave:** the Control Chamber is an ordinary building, so the thinned-fleet cancellation and the retaliation cannot be tested on LIVE. Fixes UFO_GROWTH_STOP (key `BUILDINGFUNCTION_CONTROL_CHAMBER`, update the HEAD-only tests, add a control arm) and VC-10+F5 (`GameState::massAssaultArmed`, consumed in `invasion()`; targets blocked on reverse engineering).

| ID | WHEN | THEN | UNTIL |
|---|---|---|---|
| W-1 | Raid 5 (Incubator) is won | Start the Control Chamber topic (40,000 mh, 47 h at 850) per G-6 and keep the W-2 stock estimate current | raid 6 launched |
| W-2 | Every Monday and after each incursion | Estimate the dimension stock per type: `pool = min(cap, pool + row)` each Monday (rows above), minus every UFO seen launched, plus every UFO seen leaving by a gate (escapes rejoin; a UFO killed in the city after launch does not change the dimension stock, one killed in the dimension does). The wave needs >= 10 non-Probe ships; check the estimate at every sortie against the UFOs visible in the alien city [DERIVED from EXE VC-16] | the wave resolves |
| W-3 | The Control Chamber topic is complete AND the estimate is >= 10 | Delay the launch only if `estimate - 10 <= (kills per week over the last 14 days - this week's growth) x days_of_delay / 7` for at most 7 days (starting value, [MEASURE M-S2-18]); otherwise launch and absorb (W-4). Thin by destroying launched ships, not repelling them. Every kill adds to the UFO Shot Down score and arms later UFOs (U-7), but the drained stock is what cancels the wave | launch decision |
| W-4 | About to launch raid 6 | All hold: (a) interceptors home, repaired, rearmed; (b) every base has its section-03 garrison and Security Stations; (c) the completed-week score total H leaves a cushion `H >= -2,400 + 2 x (worst weekly Damage to City so far)` (starting value; wave damage unmeasured) and Government->X-COM above the -50 cutoff by a margin (starting value -40, [MEASURE M-S2-18]); (d) cash covers the next Monday's overheads plus a repair reserve (section 01 RG); (e) no other raid squad is in the field | launch |
| W-5 | Raid 6 is won | **Hedge window:** interceptors home and dispersed, bases garrisoned, no deep-city operations, no new raid squad. It ends when the wave has launched (a burst of >= 10 UFOs within a few game hours) and every wave ship is dead, or 72 game hours after the win with no burst (the longest timer: cancelled or never armed) | window ends |
| W-6 | The wave is on the map | Wave ships never withdraw: kill them (each adds its hull score; the kill-score thresholds are likely crossed); expect them at random non-allied buildings including X-COM bases; recover wrecks only if the squad is ready (U-8). After the wave the stock only drains and incursion pressure declines | wave dead |
| W-7 | The first crossing day and the next | Cost-free hedge for the guides' "first-entry strike" (Bombers, Destroyers, Assault Ships the day after X-COM first enters the dimension): interceptors home, bases garrisoned; the validated EXE paths show no such trigger [GUIDE: UFO-INC; GH-263 item 27; UNBOUND] | day +2 |

### 8.7 Escort and the optional hybrid chain

Not on the victory path; the risk is the Biotrans's sortie through patrolling UFOs. Log Biotrans HP lost, UFOs that engaged and minutes exposed after each sortie. WHEN a sortie loses more than half the HP or a squad craft is shot down (starting value, [MEASURE M-S2-19]) THEN (a) fit a Small Disruption Shield and shorten exposure, (b) build 2 Dimension Probes as decoys ($6,000, 29 h each in the AW) [GUIDE: Wong FAQ 2.3.4, repo copy], (c) only if both fail Explorer (needs a Destroyer) -> Retaliator (Assault Ship) -> Annihilator (Battleship) as escorts: 90,000 mh research + 55k / 75k / 100k mh manufacture. With no loss in the first 3 raids build no escort. The fleet is healed on every entry (8.2), so the Let's Play's Annihilators (D44-D47) and AllOutWar's claim that 2-3 clear the dimension [GUIDE: LP, AOW] are lower-difficulty anecdotes.

---

## 9. Milestone calendar the benchmark can score

Bands: **S** stretch (events S, prep 0), **T** target (events T, prep 48 h per building), **L** late but human-achievable (events L, prep 105 h), **F** fail line: a milestone not reached by F is `failed`, not `unobserved`, only if the run is alive at F. F = L + 7 days (victory L + 14) unless stated; this is a scoring policy owned by section 06. Every date is [DERIVED] from section 4 unless a source is named; every predicate is something a human can read from the Research, Base, Hangar or Score screens, the Ufopaedia or the debrief. Milestone IDs (R, U, A, D) follow the benchmark protocol (section 06).

> **Engine today:** these are original-rules bands. Until RR-12+RR-13, VC-2+VC-3+RR-14, VC-1 and TD12 land, LIVE runs can reach R5, U1, R13 and A2 earlier (4.3) and cannot complete raids 3-10 reliably (8.4); credit is by deadline, so an early LIVE date is no reason to use the shortcut, and chain milestones beyond raid 2 are fix-gated on LIVE.

### 9.1 Due dates

| ID | Predicate (human-visible) | S | **T** | L | F |
|---|---|---:|---:|---:|---:|
| **R1** labs | Base screen: 3 Quantum labs (start + F-1 pair); ABL, AQPL, AW built | pair D5.5; ABL 11.5, AQPL 18.5, AW 22.5 | pair D7; ABL **14.5**, AQPL **21.5**, AW **25.5** | ABL 18.5, AQPL 30.5, AW 34.5 | AW after D41.5 |
| **R2** seats | staffed / built seats (Research screen); critical-lab rate >= 400 mh/h | 90% from D6.5 | **90% from D7** | 75% | < 60% for 7 days |
| **R5** path topics, day complete | each listed complete (`GATES` 0.5 and module 0.9 in every band; `THE_ALIEN_DIMENSION` = first crossing + 0.5 d; step topics per 9.2) | systems 9.6 / 9.6 / 10.1; AQPL 11.4; Probe 11.5; AW 14.0; UFO3 11.0; BIO-TRANSPORT 19.4 | systems 12.6 / 12.6 / 13.1; AQPL 14.4; Probe 14.5; AW 17.0; UFO3 14.0; BIO-TRANSPORT 22.4 | Probe 23.5; AW 26.0; UFO3 23.0; BIO-TRANSPORT 31.4 | a system not done 10 d after E1; BIO-TRANSPORT after D38.4 |
| **U1** E1 | won debrief **and** the three system topics listed (a Probe/Scout recovery does not count) | D7 | **D10** | D19 | **D26.5** |
| **U2** E3 | won debrief and `UFO_TYPE_3` listed | D7 | **D12** | D19 | **D26.5** (a scoring line; pool survivors may still fly) |
| **U3** manned recoveries | won / lost / aborted landings | >= 1 won by D7 | **>= 1 by D10, >= 2 by D19.5** (E3 plus a Fast Attack Ship) | >= 1 by D19 | 0 won by D26.5 |
| **A2** gate craft ready / first crossing | Biotrans in the hangar; The Alien Dimension in the Physics list | D24.5 | **D27.5** | D36.5 | D45 |
| **A3** victory | victory screen | D54.8 | **<= D77.8** | D109.8 | D124 |
| **R7** capture modules fitted | >= 2 craft show Alien capacity > 0 | D1.4 | **D3** | D6 | D13 |
| **R8** first specimen in containment | containment usage > 0 | D1.6 | **D4.5** | D8 | D15 |
| **R9** ABL researched | Research list | D4.1 | **D7.0** | D10.5 | D17.5 |
| **R10** Toxigun (`BIOLOGICAL_WARFARE`) | Research list | D12.8 | **D17.2** | D22.1 | D29 |
| **R11** Toxin B | Research list | D17.4 | **D20.7** | D26.0 | D33 |
| **R13** Biotrans manufactured | Hangar | D24.2 | **D27.2** | D36.2 | D43 |
| **R14** Control-Chamber readiness | W-4 satisfied at the launch of raid 6; wave resolved per W-5 | raid 6 D42.8 | **D57.8** | D78.8 | D92 |
| **R15** modelled research score (4.6) | Score screen | 1,610 by D12.5; 3,520 by D19.5 | **330 by D12.5; 2,840 by D19.5** | 330 / 450 | < 100 by D12.5 |
| **U4** Fast Attack / Assault Ship recovered | Medium Beam and shield items in stores | D12 | **D26** | D40 | - |
| **D1** idle-lab-hours | as 3.5 | <= 3% | **<= 5%** | <= 15% | > 30% |

R12 (Advanced Security Station, optional, F-6): researched D16.7 / D19.7 / D28.7. Credit shapes proposed (section 06 owns the protocol): R1, R5, R10, R11, R13, U2 = deadline credit at T; R7, R8, R9, U1, A2, R14 = timed credit (1 at T falling linearly to 0.25 at L and 0 at F); R2, R15, D1 = level credit.

### 9.2 Per-building win days and week-by-week state

| Raid | 1 Sleeping | 2 Food | 3 Farm | 4 Maint. | 5 Incub. | 6 Control | 7 Spawn | 8 Organic | 9 Megapod | 10 Gate |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| S (events S, prep 0) | 27.8 | 30.8 | 33.8 | 36.8 | 39.8 | **42.8** | 45.8 | 48.8 | 51.8 | **54.8** |
| T (events T, prep 48 h) | 32.8 | 37.8 | 42.8 | 47.8 | 52.8 | **57.8** | 62.8 | 67.8 | 72.8 | **77.8** |
| L (events L, prep 105 h) | 43.8 | 50.8 | 57.8 | 64.8 | 71.8 | **78.8** | 85.8 | 93.8 | 101.8 | **109.8** |
| A (S events, no F-1, prep 48 h) | 36.8 | 41.8 | 46.8 | 51.8 | 56.8 | 61.8 | 66.8 | 71.8 | 76.8 | 81.8 |
| Let's Play (actual, difficulty unstated) | 35 | 42 | 52 | 59 | 66 | 72 | 78 | 85 | 93 | 99 |

Interpretation: R1, R5, R13 and A2 at T are decided by about D28 by the research and facility policy (section 3) and do not depend on raid luck: compare strategies on them. R14, A1 (buildings 3-10) and A3 depend on battle logistics, specimens and the engine fixes: compare separately, conditioned on R13. "Past F" is `failed` only if the run reached F alive; a run killed at D14 by a Government takeover leaves R5 and the rest `unobserved`.

---

## 10. Harness requirements

"Human-visible" means a player can read or do it on screen; the no-cheat rule applies to the policy, not to the scorer's telemetry (HA 5). Columns: capability, needed by, and the gap in the LIVE `tools/` (HA).

| ID | Capability, and the gap in LIVE `tools/` (HA) |
|---|---|
| **HR-1..3** | Game-time research sweep every <= 3 game hours reading, per lab, the project, progress, staff, skill and the startable list for the lab's own base and type; preempt fillers only; the policy of 10.2 instead of the static `PRIORITY_RESEARCH`. Gap: 90 **wall**-second cadence; dependencies tested against `player_bases.begin()`; `research_options` for the selected lab only; no New Project on a busy lab (`oa_play.py` LIVE :2664-2670) (HA 4.1; TT 15) |
| **HR-4** | Item and specimen inventory per base (equipment, bio items, containment and Stores usage). Gap: `stores`/`loot` cover agent items; `facilities` sees `current_base` only |
| **HR-5** | Fix-check tests logged in `final.json`: (a) is a step-0 `ALIEN_BUILDING` topic startable on Day 0 (engine) or only after The Alien Dimension (original)? (M-S2-11) (b) after the first Probe/Scout recovery are the three systems listed (engine) or not? (c) ABL on Pods alone; Life Cycle needs Genetic Structure (M-S2-16) |
| **HR-6, 7** | Free cells and 2x2 anchors per base, building in any owned base, the 6/4 cap, event-driven build orders within 2 game hours of the trigger. Gap: drag brute force on `current_base`, no free-cell map (`oa_play.py` LIVE :2392), no demolish, ABL never built (HA 4.3) |
| **HR-8** | Hiring by skill into free seats and beds per base, within caps (H-5). Gap: row-0 clicks, inert trigger, "clicked N" on refused clicks (HA 4.2, D6) |
| **HR-9, 10** | Manufacture 3 capture modules; equip Bio/Cargo/Passenger onto a chosen craft and verify the module list; K-2 preconditions. Gap: one weapon per craft per 300 s, `interceptors` shows crew/pax only, module never built (HA D8, D12) |
| **HR-11** | Recovery loop: UFO type and manned class, wreck state, crewed craft ready, stop-fire when falling, ETA vs lifetime, outcome (won/lost/aborted), "topic listed" check. Gap: not exception-safe across the stage change, 11 refusals at the window centre, `centre_on_free_crash` flags Probe/Scout only (HA D9, D18) |
| **HR-12, 13** | Transfers between bases; never sell the last unit an unfinished topic needs. Gap: no transfer capability; `sell_surplus_loot` keeps 1 and sells above `keep + 2` (HA 4.14; IM 7) |
| **HR-14** | Gate sortie driver: refit, Monday portal read, token crossing (G-2), landing order, per-sortie telemetry (HP lost, UFOs engaged, hours to flip / win / return). Gap: exists, never exercised (HA 4.10) |
| **HR-15..17** | Telemetry and saves: lab idle hours and seat fill; daily `topic` poll of the path ids + ABL, BW, TOXIN_B; E1/E3 events with UFO type and outcome; the UFO list every Monday; containment usage; applicant pool daily for 14 days; research-score credit check (SCORE-C1); a rolling save before Monday #1 (D5.4) and weekly (gap: 300 s CityView checkpoint only, HA 4.17) |
| **HR-18** | **LIVE only until VC-1/TD12:** pad handling in a raid: count intact pad sites (2x2 clusters of tiles 208..211), battle save at <= 2 sites, soldiers off pad tiles, log watcher for "FATAL" / "Trying to randomize within empty container" |
| **HR-19** | Wave readiness: the W-2 stock estimate, the W-4 checklist, the W-5 window; log UFO bursts |

### 10.2 The research policy

Every sweep, for each built lab with staff (large first, then rate): never preempt a non-filler; walk the ranked list of 3.4 and take the first topic that is not hidden, complete, running elsewhere or Large-in-a-small-lab, and whose dependencies, items and specimens are present in THIS lab's base; preempt a filler only for a higher rank; an idle lab takes the first startable filler (Bio: species by score density; Physics: Gates, `UFO_TYPE_n`). On completion: AQPL / AW / ABL topic -> build it (F-2..F-4); module -> manufacture 3 (E-1); BIO-TRANSPORT -> Passenger Modules (G-1); won raid -> next `ALIEN_BUILDING` topic in the ABL within 3 h (G-6). Rank, Physics: [Control, Propulsion, Energy] > AQPL > Probe > AW > UFO_TYPE_3 > BIO-TRANSPORT > The Alien Dimension > UFO_TYPE_n > Light Beam > survival tier > fillers. Bio: module > first-specimen topic > ABL > ALIEN_BUILDING step k > Toxin-B set > species by score density. Engineering: module x3 > BIO-TRANSPORT hull > section 01 product lines.

Changed harness behaviour: 3-lab 5/5/5 staffing becomes F-1 and the seat plan; the comment that `ALIEN_BUILDING_0` occupies a Bio lab "for roughly ninety game-days" (`oa_play.py` LIVE :1438) no longer applies; modules are manufactured (K-1); the `gate_craft_order` and `cross_min_crew` genes matter only once the AW exists (about D22).

---

## Open measurements

Each is a [MEASURE]: what is unknown, how the harness measures it, what depends on it. Rules that live only in the original are tagged [UNBOUND] in the text (return timing, wave behaviour, EXE objective hardness, reinforcement intervals, turret shot counts, wreck lifetimes).

| ID | Question | Protocol | Used by |
|---|---|---|---|
| M-S2-1 | Staffed lab rate and progress persistence: exactly the sum of skills per game hour? kept on reassignment? | One 5-staff lab with known skills: read progress after 24 game hours, move one scientist off-base for an hour, switch the topic and back. Read the real mh/h of a 5- and a 10-engineer workshop | M9, R-0b |
| M-S2-3 | Overkill rate (crashed vs destroyed per weapon and UFO type) and wreck lifetimes / recovery ETAs (the engine table is a prior) | Log every shoot-down; time from crash to disappearance or recovery arrival per type | U-1, U-2, 7.1 |
| M-S2-5 | Does a violent Multiworm death hatch Hyperworms in this fork? | One deliberate kill in a Transporter battle; read the specimen list | R-HW, K-3 |
| M-S2-6 | Chain logistics: hours from craft ready to view flip, flip to a won raid, the return, win to next topic start | Per-sortie telemetry (HR-14) | 4.4, 8.2 |
| M-S2-8 | Containment overflow (engine accepts; guides say the original destroys the excess) | Unload more than 20 units; read counts next day | K-6 |
| M-S2-9 | Objective-tile destructibility per building (VC-1): which weapons destroy it, how fast, whether a partly destroyed objective counts. On LIVE also: Marsec High Explosive vs Vortex Mine on a pad site, and the 4-second mission-end timer at the Sleeping Chamber | A battle save before the objective; log weapons, tiles destroyed and the building-disabled message | 8.4 |
| M-S2-11 | Is a step-0 alien-building topic startable on Day 0? (engine binding indicator) | One `topic` query | HR-5 |
| M-S2-13 | Applicant pool at the Superhuman start: size, skills, refresh (original prediction: 6-7 standing per class, 1.5 arrivals a day) | Read the Recruit lists daily D0-D14 | 3.3 |
| M-S2-14 | Does the human-city simulation (labs, builds, funding) pause while the view is in the alien dimension? | Log lab progress and build counters across a sortie against game time | M11, 4.4 |
| M-S2-16 | Is the runtime tree the generated one (ABL opens on Brainsucker Pods alone; Life Cycle needs Genetic Structure)? | On Day 0 read the dependency lists of ADVANCED_BIOCHEMISTRY_LAB and THE_ALIEN_LIFE_CYCLE; extractor provenance differs by branch | 1, B-2, B-5 |
| M-S2-17 | Incursion cadence (3.5 per week single drain or 7 double drain) and the first-incursion time (5-6 h after the noon start vs 24-72 h); Transporter survivors after D19.5 | Count incursions per game week in a headless run; log the UFO list every Monday | 4.1, 7.4 |
| M-S2-18 | The W-2 stock estimate against the visible UFO count in the alien city at each sortie; wave damage once a wave exists, to calibrate the W-4 cushion (2 x the worst weekly Damage to City, relation margin -40) and the W-3 delay rule | Compare estimate and on-map count every sortie | W-2..W-4 |
| M-S2-19 | Squad-strength thresholds for abandoning a capture or a landing (K-4, U-9) | Record squad strength and outcome of every sortie; tune | K-4, U-9 |
| M-S2-21 | Real staffed man-hours/h of the Advanced Workshop with 5 and 10 engineers (first-hull date, Biotrans line income) | Observe progress per hour over a day | 5.3, E-3 |
