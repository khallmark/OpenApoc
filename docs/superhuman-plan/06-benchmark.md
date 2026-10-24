# 06 - Benchmark and learner plan (Superhuman = engine difficulty 4, UI button 5)

**Summary.** How the running benchmark must change so that the strategy in 01-03, executed by the harness AI of 05, can be measured instead of guessed, and so that gains from a better AI stay separable from gains caused by the engine becoming more like the original `UFO2P.EXE`/`TACP.EXE`. Every number is Superhuman unless stated.

1. **Today's benchmark cannot rank strategies, and it crowns noise.** Three penalty terms (base defences, bases lost, defeat) are 97.9% of the pooled reward variance over 115 finals, yet the defence counters exist in only 35 of them; current-harness noise is SD 436-592 (survivors) and 801-882 (all exits). On a flat landscape the live learner reports a champion beating defaults by a median +332, above +200 in 77% of sessions (section 1).
2. **Engine-fidelity versioning (section 2).** `final.json` records a fidelity vector: one boolean per fix key of `04-engine-fidelity.md`, the rule-changing options and the harness shims. A fix A/B differs in exactly one fidelity key, an AI A/B in exactly one AI setting; every fix and every AI milestone is measured against a control arm on identical seeds; nothing is pooled across vectors. All baselines below belong to the pre-fix vector F0 and are re-measured by an A/A at every new vector.
3. **Protocol (3).** One declared delta on a frozen build; budgets in game days; wall-capped runs right-censored; a provenance gate (binary, gamestate, tools, fidelity, live difficulty, day-0 funding fingerprint); a mandatory 16-run A/A; arms sized unpaired; four pre-registered looks (16/32/48/64 per arm, Pocock p < 0.0182): 95% power for +30 points at the F0 baseline `healthy@21` p0 about 0.10, 65% for +20. **The ladder resolves big strategy changes, not knob tweaks.**
4. **Milestones (4).** 34 outcome milestones, 9 harness-health diagnostics and the additional R7 specimen-delivery diagnostic, each DEFINED on human-visible state and measured from the policy channel's recorded `ui` block; engine truth is audit only; each row names the fidelity keys its baseline depends on. R7 does not change the proposed reward weights.
5. **Reward (5), learner (6), ladder (7).** Mechanical reward fixes (v3.1) can land at once; the Milestone Index (v4) and every weight change are **proposals that need the user's ruling**. At most 4 live genes and a judge before the Hall of Fame. T0 unit tests, TP probes, T1 scenario saves, T2 to day 21, T3 to day 42, T4 to victory.
6. **Baselines to beat (F0, Monday-mapped Kaplan-Meier, n = 115):** funding intact at Monday d7 / d14 / d21 / d28 = **0.97 / 0.69 / 0.10 / 0.00**; Government not taken over at day 15 / 21 / 25 = **0.74 / 0.21 / 0.07**.

## Contents

0. Conventions
1. What the current benchmark measures
2. Engine-fidelity versioning
3. Benchmark protocol
4. Milestone metrics tied to the strategy calendar
5. Reward: mechanical fixes, versions, Milestone Index proposal
6. Learner changes
7. The staged ladder
8. Files and functions to change
9. Stale or wrong in existing documents
10. Interfaces with the other sections, and decisions needed
- Open measurements

## 0. Conventions

| Item | Rule |
|---|---|
| Difficulty | Superhuman = UI button 5 = engine `difficulty` 4. `oa_victory.py --difficulty` defaults to 1, `oa_play.py` to 3, `oa_learn.py` to 5; the provenance gate reads the live value (3.2). |
| Trees | **HEAD** = historical `fda36d6c`. **LIVE** = October 7–8 research snapshots, not current source. The former in-flight work later entered six local commits through `bce89743`; funding/base-trigger work is in `879d0d73`. Bare source lines cite the snapshot. Re-resolve symbols on the chosen implementation base; later commits do not establish historical binary identity or remote publication. |
| Tags | [EXE] binary evidence (offset or validation ID); [ENGINE] code or data (`LIVE:`/`HEAD:` path:line, harness files included); [GUIDE: name, URL]; [DERIVED] my arithmetic or a count over the run artifacts (population named); [MEASURE] unknown, with how the harness measures it; [UNBOUND] EXE evidence absent, RE lead named. Statistics are recomputable with `oa_stats.py` and the A/A (8); no scratch script is part of this plan. |
| Rules | Strategy-facing rules read WHEN (a human-visible observable) THEN (an action) UNTIL (an observable); procedural protocol rules (P, FP) use WHEN/THEN. |
| "Engine today" | A marked callout where the rebuilt game differs from the EXE in a way that changes what the harness or benchmark must do now; it names the fix key of `04-engine-fidelity.md` that removes it. One strategy is written, against the original rules. |
| Companion files | 01 opening, economy, bases; 02 research and tech; 03 military doctrine (`03-military-doctrine.md`); `04-engine-fidelity.md`; 05 harness AI plan (`05-harness-ai.md`). Section numbers and rule IDs of 01-03 (BS-2, RG(k), INF, BASE-2, M-n) are cited as they stood on 2026-10-08; a changed date edits the `MILESTONES` table (4) and no code. |
| Research files | `failure_forensics.md`, `harness_audit.md` (defects D1-D19, hidden reads), `docs_digest.md`, `income_*.md`, `tech_tree.md`, `web_*.md`, archived with the corpus in `og-research:export/strategy-plan-2026-10-08/research/`. EXE facts are cited by register ID (T1, G4, BD08, ...) from `evidence/resolved-facts.md`, which supersedes the register where `adjudications.md` ruled. |
| Days | Harness Day N starts at midnight (N >= 2); the game starts Tuesday noon. Weekly funding and score roll at 00:00 of Day 7k, the first at Day 7 00:00, 5.5 days after the start [ENGINE gametime.cpp:240-262; EXE G1]. |
| IDs | Milestones G, E, R, U, C, B, V, PE, HH; protocol P1-P11 and FP1-FP9; reward fixes F1-F13; rulings RUL-1..7; decisions DEC-1..5; scenarios S1-S8; fidelity vector `fv`. |

## 1. What the current benchmark measures

### 1.1 Reward variance is penalty variance

[DERIVED] 115 `final.json` under the benchmark run directories (the pinned population; later `learning-fast` runs excluded so the numbers reproduce), re-scored with the live `reward()` ([ENGINE] `tools/oa_learn.py:112-174`, weights `:82-105`): mean -65.5, SD 628, median +166; exits 106 `signal`, 8 `defeat`, 1 `bankrupt`. Every count below is dated to this population.

| Term (v3 = live weights) | Mean contribution | Share of reward variance | Reading |
|---|---|---|---|
| `base_defences` (-400 each) | -125.2 | **37.3%** | counts being attacked, including defences that were won |
| `defeat` (-1000) | -78.3 | **35.1%** | 8 of 115 runs |
| `bases_lost` (-800 each) | -48.7 | **25.5%** | includes the loss that ends the game; double-counts with `defeat` |
| `survival` (8/day x `funded_days`) | +103.1 | -2.1% | pays days only until `funding_lost_day`, itself a sampled day (`oa_victory.py:1033-1037`) |
| `score` (0.05/pt), `research` (12/topic), `infil_raids` (2 each; counts polls, 1.4), `second_base` (+20; constant, the harness buys it on day 0), `alive_at_end` (+40; earned by 0 of 115, it fires only on `exit == "time_budget"`, `oa_learn.py:143`), `battles_won` / `ufos_down` / `recoveries` | -57.9 / +70.1 / +39.9 / +19.8 / 0.0 / +11.5 | 3.2% / 1.0% / 0.0% / 0.0% / 0.0% / <= 0.1% | the positive terms are small; the counters under-count (1.4) |

**Population caveat.** `progress.base_defences`/`bases_lost` are written only by the harness versions behind the `control-seed11`, `treatment1-seed11` and `show` families: **35 of 115 finals**; the other 80 cannot incur the -400/-800 terms (12 of them fought a `BaseDefenseScreen` mission anyway). Among the 35: mean -617, SD 891, variance shares defeat 38.2% / defences 32.3% / bases lost 30.5%; among the 80: mean +175.5, SD 169.7. The pooled reward-vs-`max_day` fit over the 106 `signal` runs (R2 0.01, slope -4.9/day) is a Simpson artefact of that mix: within the counter-bearing versions the slope is +17.6/day (R2 0.12, n = 28), within the older ones +7.2/day (R2 0.31, n = 78). The objective is weakly pro-longevity within a version; what it cannot do is reward executing a strategy, because its dominant term is a rare event (9 of 115 runs ended `defeat`/`bankrupt`).

### 1.2 Noise and minimum detectable difference

[DERIVED] SD of reward by family (16 parallel windows): older versions without the base-defence counters 35-60 (`build/learning` 44.9, `build-ms` 60.0, `build-funding` 44.6, `show-old1` 35.1; median day 9-19); current versions `control-seed11` **800.8** (mean -454.1, median day 21.5, 87.0 s per game day) and `treatment1-seed11` **882.4** (mean -558.1, day 20, 69.5 s). Pooled within family: 482.9 (all exits, n = 115) and 272.5 (`signal` only, n = 106). The small SDs belong to versions that cannot incur the -400/-800 terms, not to short runs; current-harness runs that reach day 17+ have SD 436-592 (survivors) and 801-882 (all exits). The learner does not assume 150: `noise_sd()` (`oa_learn.py:219-233`) measures the within-genome SD, and `DEFAULT_SD = 150` / `MIN_SD = 25` (`:181-182`) are a cold-start prior and floor.

MDE for two-sided alpha .05, power .8 is `2.8 x SD x sqrt(2/n)` [DERIVED]: at n = 16 per arm 148 (SD 150), 269 (272), 478 (483), about 590 (592, `control` survivors) and about 870 (882, `treatment1`); at n = 64 74 / 135 / 239. Quote the current-harness SDs for any MDE on `reward()`; 272 is optimistic.

### 1.3 The learner crowns noise

[DERIVED] On a flat landscape (each genome's reward N(0, 272) independent of its genes; 5 generations x 16 runs; 200 sessions through the live `propose`/`champion`) the apparent champion-minus-defaults gain has median **+332** (mean +354) and exceeds +100 in 87% and +200 in 77% of sessions; the learner-believed SD has median 316 (true 272). Mechanism: `champion()` takes `max(mean - 1.0 x sd/sqrt(n))` over genomes with n >= 2 (`oa_learn.py:244-250`) against a defaults mean of 2-3 runs; the winner's curse alone is E[max of 16 normals] = 1.77 SD, so the gain grows with SD. The defect is selection on a noisy maximum, not an underestimated SD.

### 1.4 Metric dictionary audit

Several recorded fields are not what their names say: `funds.*` and `metrics.ufos_downed` are score points, not counts; `progress.infil_raids` is a poll counter (14,577 "raids" over 162 logs, 58 resolved, `failure_forensics.md` §8); `progress.recoveries` reads 0 in all 31 recent runs although 7 recovery battles were fought (`gs selected` after the stage change raises `ERR unknown query`, `harness_audit.md` D18); `metrics.armed` counts any weapon holder ("armed == soldiers" coexists with 15 of 15 unarmed, `failure_forensics.md` §5); `progress.bases` is a ratchet stuck at 2; `progress.bases_lost` scores any non-resolved defence outcome, a game crash included, as -800; `funding_lost_day` is the first *sampled* day; `exit == "signal"` mixes a time limit, a learner interrupt and a foreign SIGTERM (committed `2f8e5529` invalidated foreign signals, LIVE's `make_record`, `oa_learn.py:662-676`, does not). Each has a fix and a lock test in 5.1.

### 1.5 Selection bias, the wall-clock confound and reward eras

- **History holds only dead campaigns.** Only 9 of 162 runs reached a `history.jsonl`, all `defeat`/`bankrupt` (`failure_forensics.md` §11). In every family the `signal` finals were written within 0-3 s of each other with one shared `wall_seconds` (361-3,889 s) and four families have no `history.jsonl`: one mass stop of a whole generation (`execute()`'s `except BaseException` path, `oa_learn.py:650-658`, SIGTERMs every child and appends no record on purpose), so a legacy family's "wall budget" is the instant someone interrupted the learner. `final.json` is the source of truth; `history.jsonl` is rebuilt by `ingest` (5.2).
- **Wall-clock confound.** Median 37-353 s per game day (R2(day ~ wall) = 0.20). Turbo is granted only with no hostile aggressive vehicle in the city and no attack mission (`game/state/gamestate.cpp:1174-1211`), rare after day ~5, so "days reached" rewards fewer UFOs, not play quality; 13 of 35 base defences ended on the 900-s `win_battle` budget; gates are `time.time()` timers; `restart()` rewinds game time while counters keep their values (`oa_victory.py:321-374`).
- **Reward eras** (not comparable): A `show-old2` (defeat -100, no defence terms); B `show`/`control` (-100 / -100 / -300); C = v3 (-1000 / -400 / -800); `show/r0000` is -143 under B and -1,843 under v3. A re-score applies only the terms a run recorded, so the 80 finals without defence counters are `partial`.

### 1.6 Seed pairing gives bookkeeping, not variance reduction

[DERIVED] `learning-control-seed11` and `learning-treatment1-seed11` share seed and genome at all 15 comparable indices but ran on different binaries: Government hostile at the last sample was concordant in 9/15 pairs against 9.1 expected (Fisher p 1.00); funding terminated 12/15 against 7.5 (p 0.041, driven by shared run length). **Size every comparison as unpaired; use common seeds for bookkeeping and stratification; measure the intra-seed correlation in the A/A (3.5).** At a T1+T2 vector this must be re-measured, not assumed.

### 1.7 Where the baseline starts (the numbers a new strategy must beat)

**All rows belong to fidelity vector F0 or to older mixed builds** (section 2.3): engine today the Government flips at any infiltration above 0 (T1+T2 absent), LIVE builds start at 180,000 with 10,000 weekly income (G4 regression), HEAD-like builds at 110,000 / 80,000. Re-measure every row at every new vector.

| Quantity | F0 value | Source |
|---|---|---|
| P(funding intact) at Monday d7 / d14 / d21 / d28 | **0.97 / 0.69 / 0.10 / 0.00** (runs that reached day T: 114 / 45 / 23 / 10) | [DERIVED] Kaplan-Meier over 115 finals; event = the Monday on or before `funding_lost_day` (cuts happen only at the Monday roll, EXE G1), censor at `max_day`. The sampled-day version (0.97 / 0.80 / 0.19 / 0.00) puts 7 cuts on the wrong side of their Monday and overstates survival |
| same, HEAD-like 80k-income builds (n = 30) vs LIVE 10k-income builds (n = 85) | d14 0.79 vs 0.64; d21 0.00 vs 0.12 | [DERIVED] Monday-mapped; the 10k builds are the G4 regression, the 80k builds also test lifetime score including the current week (HEAD D1, `resolved-facts.md` §3.1) |
| P(Government not taken over) at day 7 / 10 / 12 / 14 / 15 / 16 / 18 / 21 / 25 | 0.97 / 0.95 / 0.87 / 0.75 / 0.74 / 0.69 / 0.42 / 0.21 / 0.07; median takeover day 16 (quartiles 12-18) | [DERIVED] Kaplan-Meier over 148 runs' last checkpoints, takeover day read from the Government's daily `infiltrationHistory`; 4 of 50 takeovers pre-date the history window |
| runs with >= 1 base defence / with a base lost | **22 / 35** and **7 / 35** among the 35 finals that record them | [DERIVED]; the defences are mostly alien-movement-triggered in HEAD-like builds (BD01b), so this is not an original-rules rate (4.3, B3) |
| median X-COM balance, 10k-income builds, day 8-11 / 12-15 / 16-22 / 23+ | 113k / 65k / 36k / 12k | [DERIVED] `failure_forensics.md` §10 |

## 2. Engine-fidelity versioning

**Summary.** Two things move at once: the AI (05) and the engine (`04-engine-fidelity.md`). Several fixes change a baseline by more than any plausible AI gain: G4 restores an 8x larger weekly income, T1+T2 delays takeovers 3-7x, TD1-TD3 make Superhuman aliens 16-64% tougher, VC-2/3 reorder the research schedule, BD08 lets hostile organisations raid bases. Without a record of which fixes a binary carries, a "win" can be an engine change and a "loss" can be an AI that was tuned on rules the original does not have. This section makes the separation structural.

### 2.1 The record: `fidelity` in the manifest and in `final.json`

| Field | Content | Source |
|---|---|---|
| `fidelity.fixes` | one boolean per fix **atom** in 04 §1.4, normalizing its aliases (e.g. F5 = VC-10, not a second flag). Include A0 funding/base-trigger changes already committed in `879d0d73`. A T1+T2 bundle is two booleans. The selected build, not today's checkout or an old diff, determines the flags | compiled into the binary (`FIDELITY_FIXES`, proposed build-metadata owner); read by `gs fidelity` |
| `fidelity.options` | rule-changing options in force: `CrashingDimensionGate` (default true; the benchmark must launch it false, VC-8), `AllowNearbyVehicleLootPickup` (default true, TD19), `EnforceCargoLimits` (default false, F-M14), and a hash of all non-default `OpenApoc.*` options | `gs fidelity`, launch command line |
| `fidelity.shims` | harness-side substitutes active: `bad_debt_floor` (G8), `hire_caps` (RR-09/RR-10), `lab_caps` (F-M12), `market_pacing` (F-M1/F-M2), `no_stray_fire` (F02), `no_gov_craft_fire` (F05), `never_retreat_base` (BD14a), `garrison_by_presence` (BD04), `alien_building_mode = kill_all` (VC-1), `no_early_alien_building_research` (VC-2) | the orchestrator computes `shims = f(fixes)`; a shim switches itself off when its fix key is true |
| `fidelity.vector_sha256`, `.label` | sha256 of the canonical JSON of the three blocks; label from `bench/vectors.json` (F0, F1, ...; `unknown` if unlabelled) | |
| `fidelity.declared`, `.observed`, `.checks` | the launch spec's vector, what `gs fidelity` returned, and the behavioural checks below | |

- `fidelity.fixes` is 05's `engine_fixes` map (05 M0: commit ancestry plus the dirty-diff check, confirmed by the M2b fix-presence probes); this file adds the binary's own report (`gs fidelity`, carried by 05's Q10 `build`), the options, the shims and the vector hash, and a declared/observed mismatch is `provenance_mismatch`. `gs fidelity` is build metadata, not game information: only the provenance writer and the `Recorder` read it. The policy never reads `fixes` or provenance: it receives `--shims` from the orchestrator, as it receives `--difficulty`, and 05's profile genes (`fire_profile=auto`, G5) resolve from that `--shims` list, so the policy sees configuration, not the binary. A test AST-scans the policy modules for `gs fidelity` and provenance reads.
- **Lint, both ways** (written-but-never-called is this project's dominant bug class): every key in `FIDELITY_FIXES` exists in the `04` index with a test named for it, and every key the index marks landed is compiled in. A fix commit that does not add its key fails `check_fidelity_keys.py`.
- **Behavioural cross-checks.** At the first `CityView` and first Monday the Recorder compares `(balance, income)` with the fingerprint of 2.2 against the declared G4; every row of 2.4 names a human-visible check that must flip in the A/A of a vector claiming the key. A declared fix whose check does not flip is `inert_fix`: the vector is void and the fix is treated as unlanded.

### 2.2 The day-0 funding fingerprint (three regimes, not two)

[EXE G4] Superhuman starts at **100,000 balance and 80,000 weekly income**; the first Monday nets +57,100 (80,000 less 22,900 of salaries 18,000 plus upkeep 4,900). Observed at the first `CityView` (balance / income): **100,000 / 80,000** = original behaviour, G4 landed; **110,000 / 80,000** = HEAD ordering without the funding rewrite (32 logged campaigns); **180,000 / 10,000** = **LIVE with the G4 regression** (131 of 131 logged LIVE campaigns), whose first Monday nets about -12,900.

**Engine today (G4).** `startGame()` runs the funding pass before `initCity` has filled `Organisation::buildings`, and LIVE's rewrite stores the capped income, so the benchmark economy runs at one eighth of Superhuman income: any plan affordability, E-family baseline or cash-starvation rank in `failure_forensics.md` §10 is measured on it. Plan from the first weekly screen, never from 80,000. Fix: skip the funding pass in `startGame()`; land before G8. Any other fingerprint, or one that contradicts the declared G4, aborts the run `provenance_mismatch`.

### 2.3 Vectors, labels and legacy runs

A **fidelity vector** is the immutable triple (`fixes`, `options`, `shims`). **F0** names the historical LIVE source profile: A0 carried, other planned fixes absent, G4 regression present. It is not a reconstructed manifest for every old binary. Define actual flags/options from a new manifest-bearing build; baselines are `bench/baselines/<vector_sha256>.json`, produced by an A/A there. **Legacy finals have no manifest:** a 180,000 / 10,000 start is only `F0-like`, 110,000 / 80,000 only `H-like`, and mixed or other fingerprints are `unknown`. All three remain descriptive legacy cohorts, excluded from certified baselines and power estimates. A start fingerprint alone cannot identify every fix.

### 2.4 The register: what each fix does to the benchmark, and how to see that it landed

"Check" is a human-visible observable the A/A of a vector must show. State is at the 2026-10-08 snapshot; "Moves" is the expected direction of the F0 baselines of 1.7.

| Fix key | State | Original vs engine today | Check (visible) | Moves |
|---|---|---|---|---|
| T1+T2 | absent | flips only above 50% displayed, one test per 300 min, probability `(v-100)/101` / any infiltration above 0, every 125 min | no takeover while the graph is at or below 50% | G1, G2, `healthy@21` p0 up; C1 window longer; S2 band changes (7.3) |
| G4 | regression | 100,000 / 80,000 at start (2.2) | tick-0 fingerprint; first Monday credit 80,000 | E1, E2, E5, G4: funding lasts longer; cash starvation (rank 4) shrinks |
| G8 | absent | game ends at balance below -5,000 after a Monday / never | game ends at that check | terminal exits, E5 |
| F02+F05 | absent | stray fire 0 / -5 per surviving / destroyed tile, non-UFO craft kill -10 once / -5 and -25, -30 | relation change after a scripted stray-fire probe (TP) | G1 relation leg, G4, G6 |
| BD08 | absent (both trees) | organisation Raid/Storm reaches a purchased base in a continuous run / only after save/load | `base_defence` with an organisation trigger and no reload | B3 up |
| BD01a/b | LIVE carries, HEAD does not | defences only from UFO landings and organisation Raid/Storm / HEAD also from alien movement and detection | no `base_defence` with a movement trigger | B3, B4 down |
| BD14a+BD16, BD13 | absent | a lost or retreated base re-homes its people, collapse at 90% of tile value / engine kills everyone present, collapse at a `value > 5` heuristic | survivors still on the roster after a base loss with another base alive | B3 severity, PE1 |
| BD04+TD10+TD8+TD9 | absent | defenders by presence, budget `36 - 4 x stations` (max 12), turrets 580 HP armour 57 / soldiers first plus away staff, uncapped unarmoured turrets with invented guns | defender count on the base-attack screen equals present agents within budget | B2 sizing, B3 |
| TD1+TD2+TD3 | absent | hostile units +16% TU/HP/reactions, natural armour x1.64 / unscaled | an Anthropod's HP on its unit screen in a probe battle | every battle outcome, C2, B3, V1 |
| F-M1+F-M2+F-M3 | absent | price falls with standing stock (about 0.075 x max per type per week is safe) / Psiclone, Elerium, AG missiles frozen, trickles free | Psiclone price falls on a Monday after stock accumulates | E2, E3, E4, C2 after about week 6: down |
| VC-1 | absent | raid succeeds when the objective is destroyed and the squad evacuates / needs every alien dead | a Gate raid ends won with live aliens left | V1, V3 |
| VC-2+VC-3+RR-14 | absent | topics gated per function, The Alien Dimension (6,000) first, then 30,000 + 2,000 per step / 38,000 first, no prerequisite | first alien-building topic absent from the research screen before the first crossing | R5, V1, V2 later |
| UFO_GROWTH_STOP | absent | growth stops at the Control Chamber (6th raid) / at the Organic Factory (8th) | (audit) `CampaignStats.growthStoppedAtRaid` = 6 | V-family, interception load after raid 6 |
| VC-10+F5 | absent | Control Chamber win arms a one-shot wave of up to 20 UFOs unless fewer than 10 can be gathered / none | 10 or more new UFO alerts at the next invasion timer after raid 6 | survival through raid 6 |
| RR-12+RR-13 | absent | alien-system topics only from a WON manned recovery / any recovery, even aborted | system topics stay unlisted after a Probe recovery | U1, U2, R5, V2: 5-13 days later |
| SCORE-C2 | absent | no forced detection of X-COM-owned crewed buildings, big third-party stacks detected within a few sweeps / forced detection of them, big stacks rarely detected | no repeating -30 incident at an infested X-COM base | G6 incidents, C3 timing |
| SCORE-C6 | absent | falling vehicles charge no city damage / any that plows through scenery | no charge for a crash over scenery in a probe | G5, G6 damage down |
| RR-09+RR-10+F-M12 | absent | caps 100/50/50/50, 180 persons, 6 small + 4 large labs per base, flat 1.5 arrivals per class per day / none, relation-coupled | recruit screen never above the caps; 7th small lab refused | R1-R4 |
| V-AG-01 | absent | one training roll set per day / six | a trained agent's 28-day gain is 1/2 to 1/6 | PE1, B2 |
| TD12 | absent | one reinforcement per interval / four | reinforcement count in an alien-building probe | V1 |
| F9 | absent | escorts and attack-list UFOs carry the score-gated shields, cloaks, teleporters / 34% of slots do not | shields on escorts after 1,000 kill-score | U-family |
| G2 (D1-D7) | LIVE correct, HEAD diverges | funding cutoff on lifetime completed weeks, strict `< -2400`, relation strict `< -50` | cut timing as the worked examples of `evidence/resolved-facts.md` §3.1 | cut dates of `H-like` runs |
| T8 | absent | initial seeding Brainsucker 1..4, Anthropod 1..5 / 1..3 each | first infested count on day 1 | S1, early G2 |

### 2.5 Comparison types and rules

| Type | Arms differ in exactly | Held identical | Answers |
|---|---|---|---|
| **AI A/B** | one AI setting (a `plan` preset or one gene, default-off in the control's build) | binary, vector, shims, tools, seeds | does the AI change help at this vector |
| **Fix A/B** | one fidelity atom (or one bundle that lands as one commit) | genome, tools sha, seeds, **shims and policy rules pinned to the control's** (Engine-today rules kept) | what the rule change does to outcomes with the same policy |
| **Retune A/B** | the policy setting that adapts to the new rule (for example relaxing the Government response once only a displayed 50% is flip-eligible) | the new vector | does the policy gain from knowing the original rule |
| **2x2 certificate** | AI (old, new) x vector (before, after) | seeds | the decomposition in FP7 |

- **FP1 (extends P2).** A fix A/B differs in exactly one atom: `engine_sha256` differs; `gamestate_sha256` only when the atom regenerates `base_gamestate` (T8, RR-12, SCORE-EQLEVEL); `tools_sha`, genome, seeds, shims identical. Preferred: a runtime option `OpenApoc.Fidelity.<Key>` in one binary; otherwise two binaries from one commit with and without the fix commit. The vector is read from the binary either way.
- **FP2.** Every arm's observed vector must equal the pre-registered one, else the comparison is void (P4).
- **FP3. No pooling across vectors.** `genome_table` and `update_model` pool only runs with equal `vector_sha256` and equal `engine_sha256` (05 M0 rule; a rebuild with the same vector is declared `equivalent` only by a 16-run A/A showing no shift), as they already filter on `reward_version`; a genome's F0 reward is a prior at a later vector, never evidence; the Hall of Fame stores the vector; a trend chart has one series per vector.
- **FP4. Re-baseline.** A landed fix voids the stored baseline for that vector. An A/A at the new vector precedes the next A/B; each primary is pre-registered per vector and one whose p0 leaves [0.15, 0.85] is re-selected or declared an exception (3.6).
- **FP5. Separability.** A fix A/B keeps the policy constant, so its effect is the rule's effect on a policy that did not know about it; retuning is a separate Retune A/B on the new vector. Pinned or retuned fix A/Bs is **RUL-7, the user's ruling (10.2)**; this plan assumes pinned.
- **FP6. Every AI milestone has a control.** Each decision-changing 05 milestone (M3 survival, M5-M7 the income pillars, M8 progression, M9 training) is adopted only through an AI A/B against the previously adopted build on the same seed list at the current vector; 05 M10 re-runs them as paired arms at the final vector. The no-behaviour-change milestones (M0, M1, M2 `plan=legacy` parity) are certified by a parity test (identical decisions on recorded `ui` snapshots) plus a 16-run A/A showing no shift.
- **FP7. Ledger and waterfall.** `bench/ledger.jsonl` holds one row per comparison (`cid`, type, arms, vectors, endpoint, p_control, p_treatment, CI, looks, verdict, guardrails). The movement of an endpoint from B0 (F0, legacy AI) to today is the sum of steps, each against its own control, never one before/after. With `p_ij` for AI `i` (0 legacy, 1 new) at vector `j` (0 old, 1 new): AI effect `p10 - p00` at the old vector and `p11 - p01` at the new; fix effect `p01 - p00` with the old AI and `p11 - p10` with the new; interaction `p11 - p10 - p01 + p00`.
- **FP8. Same seeds** in control and treatment (bookkeeping, stratification, divergence-day measurement). A fix that changes tick-0 state (G4, T8) makes the arms diverge at once and gains nothing from pairing; one that changes only rare events (T1+T2, BD08) may stay identical until the first such event, which the A/A divergence day measures. **FP9.** Scenario fixtures carry their vector and are refused on another (7.3).

### 2.6 What the benchmark needs landed, and what it can do before

`04-engine-fidelity.md` owns the fix order; these are the benchmark's constraints on it.

- G4 before any economy or funding primary is trusted, and before G8: at F0 the economy is one eighth of the original, and G8's four-Monday runway (100,000 - 22,900 n is below -5,000 at n = 5) needs the right start.
- T1+T2 before a Government-takeover primary is trusted: F0 takeovers on day 7-20 are 3-7x too early, and the original needs a displayed value above 50% before it rolls at all.
- BD08 and BD01a/b before a base-defence penalty or garrison milestone is calibrated: engine logs understate raids on bases (BD08) and HEAD-like logs overstate defences (BD01b).
- TD1-TD3 before battle-doctrine genes are frozen: doctrine learned at F0 is Novice-strength doctrine.
- F-M1+F-M2, VC-2/3, RR-12/13 before E-family targets and R5/V2 due days are final: they move sale income after about week 6 and the research and crossing dates by 5-13 days.

**Gate A (04 §6.0, 05 §6.2) = GA after step 6, A0 plus A-now.** M3+ AI-versus-baseline verdicts first count on that frozen vector. Earlier F0 diagnostics are tagged `pre_gate_a`. The first 2x2 measures AI improvement on this partial-fidelity engine; it does not certify original-game parity or victory. Steps 7–9 then add hostile-unit scaling and seeding, organisation raids and defence rosters, and bankruptcy. Run M10 controls and re-fit garrisons for each changed vector; never pool those runs with GA. Pin policy while measuring each engine fix on the route to GA.

Deliverables of this section: `gs fidelity`, `FIDELITY_FIXES`, the lint, `OpenApoc.Fidelity.<Key>` options where cheap (engine, 8.2); the provenance block, `ingest` classification, the `oa_bench` vector check and the 2x2 decomposition in `oa_stats.py` (harness, 8.1).

## 3. Benchmark protocol

### 3.1 Standing rules

| # | WHEN | THEN |
|---|---|---|
| P1 | two runs are compared | they differ in **exactly one declared thing** (2.5): an AI setting on an identical build and vector, or one fidelity atom with the policy pinned. A strategy change enters as a gene or `plan` preset default-off in the control's own build; two code revisions are never the A and the B. |
| P2 | the comparison is an engine change | see FP1 (2.5): one fidelity atom differs, and `gamestate_sha256` only when that atom regenerates `base_gamestate`. |
| P3 | a comparison starts | `bench/<cid>/PREREG.json` exists (arms, build ids, **fidelity vector**, type, tier, `until_day`, endpoints, seeds, looks, boundaries, decision rule, guardrails) and its sha256 is stamped into every manifest; `oa_bench` refuses to launch without it. |
| P4 | two runs of one arm differ in `build_id` or observed vector | the comparison is **void**; re-run from scratch. |
| P5 | an arm is mid-comparison | never restarted, re-seeded or topped up; read only at the pre-registered looks (`docs/HANDOFF.md` "How this went wrong" #5: eight restarts, no run ever finished). |
| P6 | an arm's infrastructure-failure rate exceeds 15%, or the arms differ by more than 10 points | void (differential attrition bias). |
| P7 | a run's policy queried outside the human-visible allow-list (`harness_audit.md` §5.3 and 05 §5.3: H1a/H1b exact `crew=`, H2 global `foes`, H3 integer relation, H5, H7) | tag `hidden_reads`; arms carry the same read set; the list is 05's `HUMAN_SURFACE` registry with its `test_oa_no_cheat.py` ratchet. 05 removes or replaces them, which empties the tag by construction. |
| P8 | `restarts > 0` or a reload rewound game time | tag `rewound`, keep the run in the intent-to-treat primary (restarts are not random: the `map::at` SIGABRT is about 1 in 20 attempts and restarts track battle count), report an excluding sensitivity analysis, guard the restart rate. Rows written before the restart with tick > T_c are `superseded`, `derive_milestones` keeps the last-written row per game day and `progress` counters are recomputed from events. |
| P9 | every 3 comparisons, or after a change to machine, tile layout or concurrency | re-run the control as a canary (`--baseline-every`, `oa_learn.py:768`); drift beyond 1 SE on a primary voids the stored baseline. |
| P10 | a run ends `aborted` | not scored, not an arm outcome; re-run with the same seed and genome in a fresh out dir (`.retry`, `oa_learn.py:727-728`). Mass stops are this machine's normal failure and must not drop a generation. |
| P11 | a comparison is registered | PREREG names the type and vector; the observed vector must match (FP2); declared fix atoms show their manipulation check in the A/A. |

### 3.2 Run identity and the provenance gate

`manifest.json` (05's M0 `final.json.provenance`, embedded in schema 2) is the run's identity; a run without a complete manifest is inadmissible. One module (05's `oa_provenance.py`), extended here by the PREREG fields and `fidelity`.

| Field | How computed | Why |
|---|---|---|
| `build_id.engine_sha256` | sha256 of the executed `OpenApoc` binary (each run uses a per-run `.app` copy deleted at `ProcessRun.close`, `oa_learn.py:581`: hash at start) | binaries from one day came from an uncommitted tree (`failure_forensics.md` §0.2); a git sha is not an identity |
| `build_id.gamestate_sha256` | sha256 over sorted `(relpath, sha256)` of `data/mods/base/base_gamestate/**` and `data/difficulty4_patch/**` | `base_gamestate` is generated, untracked and differs by branch |
| `build_id.tools_sha` | commit plus content hashes of the full tools diff and untracked tools files | the historical snapshot was dirty; later local commits do not identify the tools used by each old run |
| `fidelity` | 2.1 | which rules the binary carries |
| `difficulty_engine` | live `gs equipment_catalog` -> `difficulty=` (`gamestateintrospect.cpp:262`) must equal 4 or the run aborts `provenance_mismatch` | the UI button / engine index trap |
| `start_fingerprint` | first `CityView`: `funds.balance`, `funds.income` (2.2), Government label, main-base building id, first infested building and owner (audit); first Monday: the credited funding | the income regime is a different game |
| `seed`, `seed_stream`, `genome`, `tier`, `until_day`, `wall_cap_s`, `parallel_windows`, `tile_slot`, `host`, `started_at`, `prereg_sha256`, `comparison_id`, `arm` | launch spec, `OA_TILE`, `os.uname()` | load covariate, P3 |

### 3.3 Budgets, exits and censoring

Game days are the unit; harness cadence moves to game time (`GameClock`, 8), so the same genome on the same seed makes the same decisions at the same game times regardless of load.

Tiers (`--until-day`; wall cap is a safety net only; typical wall at 80 s per game day): **T1** scenario, start day + 4..12, 6-16 min, operational genes and failure-mode fixes (7.3); **T2** day **21**, about 33 min, the first strategy filter (Government and funding at day 21); **T3** day 42, about 65 min, economy and capacity (labs, income, Transporter window); **T4** victory or day 120, 3-5 h, the victory chain. Wall caps 30 / 90 / 180 min / 8 h.

| `exit` | Class | Scored | Analysis role |
|---|---|---|---|
| `victory` | terminal | yes | event at day |
| `defeat` (GameLost: last base destroyed) | terminal | yes | event; all later milestones **failed** |
| `bankrupt` = balance strictly below -5,000 at the first poll after a Monday roll [EXE G8 `FUN_00060ea4`] | terminal | yes | event; declared by the Recorder (`harness_declared`) until the engine ends the game itself (fix G8), then `defeat` with cause `bankrupt`. Replaces the harness rule (funding cut, balance < 5,000, nobody armed; `oa_victory.py:929-958`, `harness_audit.md` D13) |
| `day_budget` | complete | yes | observed through `until_day` |
| `wall_cap` (today's `time_budget` exit at `--hours`, `oa_victory.py:1310`, renamed) | **censored** | yes | observed through `last_day`; later milestones **unobserved**, never failed |
| `foreign_signal` | infra | no | infra rate |
| `aborted` (orchestrator interrupt, mass stop) | infra, re-run (P10) | no | not an arm outcome |
| `start_failed`, `restart_failed`, `next_campaign_failed`, `crash`, `provenance_mismatch`, `inert_fix`, `harness_stall` (no game-time progress for 10 wall-minutes) | infra | no | P6 |

**Engine today (G8).** The engine has no bad-debt defeat (`GameLost` only when the last base dies, `game/state/shared/base.cpp:155-159`); 4 of 164 logged campaigns went below -5,000 and played on. With funding cut, 100,000 cash and 22,900 weekly overheads the balance is below -5,000 four Mondays after the cut [DERIVED, EXE G8]. The scorer stops crediting a run at that Monday (later milestones failed, not unobserved) even if the engine continues.

- WHEN the runner receives SIGTERM THEN it reads `<out>/STOP_BY_ORCHESTRATOR`, a one-line file holding the reason written just before the signal: `wall_cap` (`execute()` when `time.time() > handle.deadline`, `oa_learn.py:633-638`) or `aborted` (`ProcessRun.stop()` from the `except BaseException` path, `:650-658`); absent -> `foreign_signal`. A bare presence flag is not enough: `ProcessRun.stop()` serves both paths, and an `aborted` run mistaken for `wall_cap` would enter as censored.
- WHEN `metrics.day >= until_day` AND the stage is `CityView` THEN write `final.save`, one last timeline row and `finish("day_budget")`; an open battle is finished first (at most 900 s) (`Victory._run` after `sample_metrics()`, `oa_victory.py:1161`).
- **Censoring rule** for a milestone due on day D_m: attained -> observed; not attained and (`last_day >= D_m` or terminal exit) -> failed; not attained and `last_day < D_m` and not terminal -> **unobserved** (Kaplan-Meier censored at `last_day`). A run that ran out of wall time is never counted as survived or failed.

### 3.4 Seeds

Two streams, `sha256("label:i") mod 2e9 + 1` (never 0, "engine default", `game/state/gamestate.cpp:593`): **DEV** (48 seeds; iteration, harvest, A/A) and **FRESH** (a counter; confirmation only; `seeds_used.jsonl` refuses reuse). A batch is 16 windows = 8 seeds x 2 arms with randomised slots (`OA_TILE`) so both arms see the same load. Common seeds are bookkeeping only (1.6).

### 3.5 The A/A (mandatory before any A/B on a new build, machine layout **or fidelity vector**)

16 runs = 8 DEV seeds x 2 replicates, defaults genome, same build and vector, T2 horizon. Outputs: **`divergence_day`** per pair (first day whose timeline rows differ in balance, infiltrated count or completed research; if the median is at or beyond D_m the replicates stay identical until D_m and pairing is valid for m); **ICC** of seed per indicator (below 0.1 = bookkeeping); a **null distribution** (random 8/8 split x 10,000: Fisher p and absolute difference, calibrating the analysis code path); infra rate, `s_day`, battles and incursions per game week (measures the F6 cadence); **`p0`** of every endpoint and each fix atom's manipulation check (voids a vector with an inert fix). The result is `bench/baselines/<vector_sha256>.json` and replaces the placeholders of 1.7.

### 3.6 Primary endpoints, power and sequential looks

Primary endpoints are **game-day milestone indicators** (4.3), pre-registered **per vector** with p0 preferably in [0.15, 0.85]. At F0 the T2 primary `healthy@21` is a declared exception (p0 about 0.10): the sequential design keeps 95% power for +30 points and 0.5% false efficacy there, and its mirror `healthy@14` (p0 0.69) is a guardrail so a treatment cannot win by moving only the late endpoint.

| Tier | Primary | Baseline p0 |
|---|---|---|
| T1 S2 | Government **not** taken over four game days later (fixture band per vector, 7.3) | F0: infiltration reached 200 within 4 days in 16 of 18 takeovers (`harness_audit.md` §6.1); other vectors [MEASURE] |
| T2 | `healthy@21` = Government intact (G1) AND funding intact at the Monday of Day 21 (G4) AND at least one base alive | F0: **at most 0.10** (a conjunction cannot exceed the funding-intact 0.10). After T1+T2 and G4 the A/A re-selects `healthy@D` for the smallest D in {21, 28, 35} with p0 inside the window |
| T3 / T4 | `healthy@42` + `econ@42` (non-Senate income, E2); days to alien-dimension crossing / victory (log-rank) | T3 about 0.00 at F0: no proportion test resolves a difference from zero, but at 64 per arm 7 or more successes against 0 cross the boundary (Fisher p 0.013 against 0.0182) [DERIVED]; fund T3 only after a T2 result lifts the baseline |

**Pre-registered group-sequential design** (default): looks at n = 16, 32, 48, 64 per arm (2, 4, 6, 8 batches of 8+8), two-sided Fisher exact with Pocock nominal p < 0.0182 per look (overall alpha .05) [DERIVED, 3,000 simulations per row]:

| True p0 -> p1 | P(stop for efficacy) | mean n per arm |
|---|---|---|
| 0.27 -> 0.27 (A/A) | **0.012** (0.012 harm) | 63 |
| 0.27 -> 0.52 / 0.57 / 0.67 | 0.69 / **0.85** / 0.99 | 49 / 43 / 31 |
| **0.10 -> 0.10 (A/A at the F0 T2 baseline)** | **0.005** (0.007 harm) | 64 |
| **0.10 -> 0.30 / 0.40 / 0.50** | 0.65 / **0.95** / 0.998 | 51 / 38 / 27 |

**What the ladder can and cannot do.** It resolves *large* effects, roughly +25 to +30 points at 64 per arm. A +10-point effect (0.27 -> 0.37) needs about 340 per arm by normal approximation [DERIVED] (680 runs, 23-32 hours at T2): test hand-designed strategy changes of the size 01-03 target (Government intact at day 21 in 21% of F0 runs toward at least 70%), not parameter nudges, which belong to T1 scenarios. A fix A/B has the same budget, so a fix is certified by its manipulation check plus a large-effect endpoint; small side effects are exploratory. Time-to-event endpoints use the log-rank test: required events `4(z_a+z_b)^2 / ln(HR)^2` = **247 / 66 / 26** for HR 0.7 / 0.5 / 0.33 [DERIVED].

### 3.7 Analysis and the decision rule

`tools/oa_stats.py` (new, stdlib only): Wilson and Newcombe intervals, exact Fisher and McNemar, Kaplan-Meier, log-rank, bootstrap, permutation, `pocock_nominal(K)`, `n_per_arm(p0, p1)`, `decompose_2x2`. `oa_bench compare <cid>` prints per endpoint and arm: n admitted / censored / infra, proportion with Wilson CI, difference with Newcombe CI, Fisher p, Kaplan-Meier at the due day, log-rank p; then guardrails and the A/A reference.

- WHEN the efficacy boundary is crossed on the primary at a look AND every guardrail and P6 hold AND at least 90% of treatment runs show the manipulation check (AI A/B: `decisions_changed` of the treatment's genes or the rule classes of a `plan` preset, 6.2; fix A/B: the fix's visible check, 2.4) THEN adopt: the treatment becomes the baseline of its vector and an A/A repeats on the next batch.
- WHEN n_max is reached without crossing THEN publish "no effect >= Delta80 detectable" with the CI; never extend the same comparison with more seeds; a further attempt needs a changed treatment.
- Intent-to-treat is primary; per-protocol secondary. One primary per comparison; secondary milestones use Holm correction and are exploratory.
- **Guardrails** (lower Newcombe bound of the difference above -0.15): funding intact at Monday d14; bases alive at d14; `healthy@14`; **garrisoned base-days (B2) and silent base losses (B4 stays 0)**, so buying bases cannot pass by leaving them empty (rank 3, 23 of 35 one-base endings had no recorded defence); **`empty_sweeps` (C3)**, reported separately for Government-owned buildings; **sell pacing** (below); weeks with W < 0 (G6); cap compliance (HH9); infra, abort, restart and stall rates; hidden-read set; wall seconds per game day within x1.5.
- **Sell pacing guardrail.** [EXE F-M2] Safe selling is about 0.075 x max stock per type per week or one lump per type; the max is not displayed, so the observable is the type's price on the market screen. WHEN a type's displayed price fell on a Monday THEN hold further sales of that type UNTIL a Monday passes with its displayed price unchanged (the original never raises a price; stock decays about 30% a week, so the cuts stop once stock is below half the maximum); a sale of that type while the hold is active is a pacing violation (`pacing_violations`, a guardrail, never a reward term). **Engine today (F-M1+F-M2):** Psiclone, Elerium and the AG missiles are frozen at base price and weekly trickles are free, so a treatment that trickles one frozen type looks better in the engine than it can in the original; E2/E3 are reported with and without the 15 frozen-priced goods (`E3x`), and a selling treatment must not regress E3x.

### 3.8 Cost model

wall(run) = `until_day` x `s_day` + battles x about 5 min (capped at 15 min by the 900-s `win_battle` budget; 0.9 missions per run in 162 logs, `failure_forensics.md` §8). `s_day` = 80 s (87.0 control, 69.5 treatment; section 1.2); the A/A replaces it, per build and vector (commit `af6af16b` changed busy-city throughput 2.6x per its message, and fixes that keep the Government alive or grow the economy add UFOs, battles and decisions per day). A comparison of 4 looks and n_max 64 per arm (8 batches of 16 windows) costs T1 about 1.3 h, T2 **4.5-6 h**, T3 9-11 h, T4 25-40 h (finalists only), and a 2x2 certificate at T2 9-12 h.

## 4. Milestone metrics tied to the strategy calendar

### 4.1 Rule M0: milestones are defined on human-visible state

- **M0.** Every milestone is DEFINED on state a player can read: a screen (organisation, infiltration graph, finance, score, market, research, base, agent roster, crash-site list), an alert, a message, a modal screen. The `Recorder` fills the `ui` block of each timeline row from 05's human-surface queries (Q1 `org`, Q2 `bases_detail`, Q4 `agent_roster`, Q6 `market`, Q9 `ledger`); `derive_milestones` reads only `ui` and `events`. The `truth` block (`tele_*`: raw infiltration, `takenOver`, exact crews, hidden unlock flags) is **audit only**: a disagreement sets `truth_disagrees` on the row, counted per arm as a harness-health diagnostic, and never changes a status. The AI and the scorer see the same facts.
- WHEN a query name starts with `tele_` THEN only `Recorder` (holding the raw `Harness`) may call it; the policy-facing `Driver.h` is a `PolicyHarness` wrapper that raises `CheatError` on `tele_*` and records every name in `queries_by_name`. A test AST-scans `oa_victory.py`, `oa_play.py`, `oa_loadouts.py` for `tele_` literals both ways: no policy call, and every `tele_*` handler is called by `Recorder`. `final.json.hidden_reads` = `queries_by_name` minus the ratified allow-list (`harness_audit.md` §5.3 and 05 §5.3 list them: exact `crew=`, global `foes`, integer relation; RUL-6).
- **Due days are provisional.** Each Due cell cites the rule of 01-03 that owns the date or says "no source"; `campaign-plan.md` §10.4 is Novice-based and unused. All dates live in `tools/oa_milestones.py: MILESTONES`, so a changed calendar changes no code; timed credit (4.4) already scores lateness, so no slack is added.
- **Pillar tags.** The user's direction maps to `P-BASE` (B1-B5), `P-SELL` (E2-E4) and `P-STEAL` (C1-C2, E3); `oa_bench compare` prints a per-pillar sub-vector beside the primary, so a pillar cannot be starved while the headline improves.

### 4.2 Recording

Five artifacts. `timeline.jsonl`: one row (1-2 KB, about 100 KB per 60-day run) at the first `CityView` poll after each game-day 00:00, plus a **Sunday 23:30 pre-roll row** and the first post-roll poll (isolating the roll's balance change) and one row on every exit. `events.jsonl`: `{tick, day, kind, ...}` written at the action site and by snapshot diffs, kinds `takeover`, `funding_cut`, `raid{building, owner, outcome, survivors}`, `sweep`, `empty_sweep{building, owner}`, `base_defence{trigger, start, end, outcome}`, `base_lost{cause}`, `recovery{ufo_type, manned, won}`, `ufo_shot_down{type}`, `research_done{name}`, `facility_built`, `mfg`, `sale{category, proceeds}`, `hire{role}`, `death{role}`. `ledger.jsonl`: `{tick, kind, source, delta}` around every harness spend or sale (`balance` read before and after; today's `[sell] ... funds 110000->131080` log line made structured). `checkpoints/d{DDD}.save`: the gamestate zip (1.3 MB) at Sunday 23:00 game time, `CityView` only; scenario-fixture source (7.3), pruned after the comparison closes. `milestones.json`: at exit, `derive_milestones(timeline, events)` gives `{id: {attained_day, value, credit, status}}`, the pillar sub-vector and `truth_disagrees`.

**Timeline row (schema 1).** Header `t{ticks, day, week, dow, wall_s}`. The **`ui` block** is the only block T0 fixtures may use (7.2): funds, last week's income, funding-terminated flag, score total and week with the eight category points (margin to -2,400 derived as the harness already does, `oa_victory.py:558-560`); per organisation the label and `inf_pct` (InfiltrationScreen percent, raw / 2, `organisation.cpp:670`); UFOs in the city and crashed; per base beds, facilities, labs (type, size, staff, skill, project), `present_armed_verified`, `present_staff`, `present_total`; research done plus `topic <name>` polls of the 20 victory-path entries; fleet; market prices of sold types. The **`truth` block** (audit only) holds raw infiltration and `takenOver`, infiltrated-building and alien counts, `known_to_aliens`, hidden unlock-flag polls; the **`harness` block** stage seconds and query deltas. "Present" = `currentBuilding` is the base and `currentVehicle` empty; "armed-verified" = loaded primary, 2+ spare clips, five armour pieces, medi-kit (03 §7); `armed_loaded` is the weaker diagnostic. A test asserts the `Snapshot` fields used by `decide()` are a subset of the `ui` keys and of `HUMAN_SURFACE`. The new engine queries (`tele_orgs`, `tele_bases`, `tele_finance`, `tele_stats`, `tele_agents`, `tele_start`, `tele_building`; `fidelity` via 05's Q10, 2.1) sit beside `introspectGameState` (`gamestateintrospect.cpp:724`); each `tele_X` returns a superset of its policy twin (05 Q1, Q2, Q9, Q4).

### 4.3 The milestone table

"Reader" = can the current harness produce it (y / p partial / n). Baselines are fidelity vector F0 (1.7) unless stated. **Fidelity dependence** names the fix keys of 2.4 whose landing moves the baseline.

#### G - Governance and survival (`failure_forensics.md` §2, ranks 1-4)

| ID | Human-visible definition | Due | Source / reader | F0 baseline | Fidelity |
|---|---|---|---|---|---|
| G1 | **Government intact at end of day D**: fails at the first of the takeover notification / message-log line naming it (05 Q1 `takeover_notice`, Q11), or its relation label Hostile together with its infiltration graph at 100%. Hostile alone can come from damage and is recoverable; a takeover is permanent and sets the relation to -100 | D = 15, 30, 60 (task brief) | org screen, graph, `takeover_notice`, event `takeover`; p | intact 0.74 / 0.21 / 0.07 at day 15 / 21 / 25, median takeover day 16 (1.7) | **T1+T2**, F02+F05 |
| G2 | Government infiltration graph: peak displayed %, hours above 0%, hours **above 50%** | sweep every Government-owned infested building within the response times of 03 §3 (INF rules) of any non-zero display | InfiltrationScreen (Q1); n | 0, then 200 within 4 days in 16 of 18 takeovers (`harness_audit.md` §6.1) | **T1+T2**, T8 |
| G3 | Organisations taken over by day D, and which of Diablo, Gravball, Megapol, Osiron, Psyke | record only | org screens; n | mean 0.1 / 1.8 / 4.2 / 10.0 / 14.6 / 17.2 by day bucket (`failure_forensics.md` §3) | T1+T2 |
| G4 | **Funding intact on the Monday of week N**: the weekly funding screen still pays | Mondays 14, 21, 28, then 56, 84 (01 §6: Monday #3 is the median cut) | `funds.funding_terminated` on the first row after the roll; y | **0.69 / 0.10 / 0.00** at d14 / d21 / d28 | G4, T1+T2, F02+F05, SCORE-C2/C6 |
| G5 | Score margin to the cutoff, weekly minimum. [EXE G2] funding is cut at a Monday if lifetime score of COMPLETED weeks `H` is strictly below -2,400 or Government->X-COM strictly below -50; the week being assessed never counts and `H + W` is never tested alone (a bad week is paid one Monday late) | margin above 1,200 (harness warning level, `oa_victory.py:576`); weekly W >= +400 (01 §5.3) | score screen, "previous" column and this week; y (log only) | mean total score -2,020 at d15-18, 29% already below -2,400 | SCORE-C2/C6, F02+F05, G2 |
| G6 | Weekly score components and W. W < 0 costs F/15 of next income, < -400 F/10, < -800 F/5, < -1,600 F/4; W > 400 adds F/20 rising to F/4 above 12,800 | W >= 0; target +400 | score screen; y, misnamed (`pts_`) | damage 46.9% of negatives, incursions 34.2%, incidents 13.9% | **SCORE-C6**, **SCORE-C2** |

**G2 risk model (original rules).** [EXE T1/T2] Nothing happens at or below a displayed 50% (raw `v` at most 100). Above it, one test per 300 minutes (4.8 per day) takes the organisation over with probability `(v-100)/101`; constant-`v` illustrations [DERIVED]: displayed 51% = 2.0% per test and 9% per day, 60% = 19.8% and 65%, 75% = 49.5% and 96%, 100% = 99%. A takeover is permanent (only a won Food Chamber raid frees the Mutant Alliance, T4). WHEN any sponsor's graph exceeds 50% THEN clear its alien crews now UNTIL the graph falls day over day (about 6 displayed points per day once the crews are gone, EXE T6).

**Engine today (T1+T2).** Any raw value above 0 can flip an organisation, tested every 125 minutes at a random phase (`game/state/organisation.cpp:84-108`): displayed 1% (raw 2) = 10.9% per day, 3% = 29.6%, 6% = 51%; 10 Anthropods in the Senate (+2/h) flip the Government in a median 19 h against about 87 h. So at F0 treat any nonzero Government infestation as imminent; the 50% margin does not exist yet, and F0 takeovers on day 7-20 are 3-7x too early.

**Engine today (F02+F05, SCORE-C2, SCORE-C6).** The engine charges -5 per stray hit and -25 per destroyed stray tile (original 0 and -5) and -30 per downed Government craft (original -10), so relation budgets are 2-5x tighter than the original's and stray fire must be avoided (G1 relation leg, G4). It bills -30 about every 7 hours for each crewed X-COM-owned building with aliens in it (the original never forces that detection) while large single-species stacks in third-party buildings are rarely detected, and it charges Damage to City for any falling vehicle that plows through building scenery (the original charges only the Overspawn): the incident and damage shares in G6 include charges the original never levies.

#### E - Economy: independence from Senate funding (user pillar: sell what we make)

| ID | Human-visible definition | Due | Source / reader | F0 baseline | Fidelity |
|---|---|---|---|---|---|
| E1 | Senate funding credited on the Monday funding screen | diagnostic | weekly funding screen; p | 10,000 (LIVE) or 80,000 (HEAD-like); original 80,000 | **G4** |
| **E2** | **Operating cash flow per week** = ledger proceeds tagged `sale` (loot, craft, manufactured goods, Cult loot) minus the Monday outgoings (salaries + upkeep = E1 minus the balance change across the roll). Capex is excluded (E6). The headline independence number | **sustained**: E2 >= 0 in two consecutive completed weeks, the second ending by Day 69 (01 §6 W10); 1.2 x burn by Day 83 (W12). Cult loot alone can make weeks 1-3 positive before the Cult is taken over (01 W3): that must not count | ledger + finance screen; n. Not observable at T2/T3: T3 reads E3/E4 | before any sales **-22,900 per week** (salaries 18,000 + upkeep 4,900) [EXE G9]; the harness's loot sales add an unmeasured amount | F-M1+F-M2, RR-09, G4 |
| E3 | Non-Senate sales by source (loot, craft, manufactured goods, Cult loot) and `E3x` (excluding the 15 frozen-priced goods, 3.7) | Record realised income under readiness gates. The 01 sensitivity model's $90k gross by Day 7/$210k by Day 14 assumes an optimistic raid cadence, not a spending entitlement or default deadline. A Temple raid is $39.5k-77.1k gross at base price at 12-23 loot spots [EXE F-M9]; M-1 must measure actual yield | ledger; n | one +21,080 ground-fleet sale on day 0 (98/98 logs) | **F-M1+F-M2**, TD1-TD3 |
| E4 | Manufacturing for profit started: an Engineering lab running a product that is sold, not the gate craft. [EXE F-M6] craft sales add no stock and never erode: craft and slow high-value goods are the unbounded line (Stasis Bomb 0.725, Bio-Transport 0.629 margin per man-hour) | first profit product by about Day 15, Bio-Transport line by about Day 35-45 (01 §4.2.2); `income_manufacturing.md` §0.8 window Day 10-30 | `research.labs_detail` + event `mfg`; p | **0 of 98 logs** contain `[manufacture]`; engineering labs idle in 2,451/2,451 snapshots | F-M1+F-M2, RR-06 |
| E5 | Runway in weeks = balance / (-E2) when E2 < 0; bankruptcy event | reserve >= 6 weeks of burn from Day 84; before that 01's Runway Gate RG(k) on each purchase | timeline; p | median balance 113k / 65k / 36k / 12k at d8-11 / 12-15 / 16-22 / 23+; 6 runs negative | **G8**, G4 |
| E6 | Capex per week by category; idle cash (above a floor for 3+ days while labs or hires wait) | diagnostic | ledger; n | median $56,192 unspent at last sample | G4 |

**Engine today (G4+G8).** The first Monday nets about -12,900 instead of +57,100 and the engine never ends a run for debt. WHEN the funding screen shows the cut THEN stop hiring and building and sell stock UNTIL the run ends `bankrupt`: the cut latch is permanent in the original, and about four Mondays of cash remain (3.3).

#### R - Capacity and research

| ID | Human-visible definition | Due | Source / reader | F0 baseline | Fidelity |
|---|---|---|---|---|---|
| R1 | Labs built by type and size across all bases. [EXE F-M12] per base at most 6 small and 4 large labs and workshops in total | Default: pair within 5 build-days of 01 ST-0 and RG approval; record actual order/completion. Day-zero F-1 is a separate comparison arm, not the default deadline. Advanced dates are R6 | base screen (Q2); p | starting set in 150 of 153 saves; AQPL in 3; a second lab never | **RR-09+RR-10+F-M12** |
| R2 | Seats staffed / total, and lab skill (an injured scientist adds nothing) | staff = capacity within 1 game day of a lab's completion (02 H-1/H-2) | `research.labs_detail`; y | 1,159 skill, 15 staff, constant | RR-10, RR-02 |
| R3 | Scientists, engineers, soldiers by base. [EXE RR-09] caps 100 soldiers, 50 per technical class, 180 persons; flat technical arrivals about 1.5 per class per day | 02 §3.3: 15 technicians initially, 25 after the pair is built and hired into, about 45 at the conditional target crossing D27.5. Reforecast after ST-0/RG delays. Soldier demand uses actual fielded-garrison dilution, not a fixed total roster | roster; p | 25 start agents; hires stall at 30 (134 of 207 failed hires, `failure_forensics.md` §6) | **RR-09+RR-10** |
| R4 | Living-quarter headroom = 10 x finished quarters - agents homed; a hire needs a free slot | never below 3 + hires planned within 4 days (01 §2.4) | base screen; n | 30 beds for 25 agents; second base 0 | |
| R5 | Day each victory-path topic completes, keyed on the **research-screen topic name** (20 entries, `tech_tree.md` §5.1) and, for alien buildings, on the **building function name**, never the engine's `ALIEN_BUILDING_k` index (VC-2 moves the binding from file position to function) | per 02 §3.1/§3.3. Original: The Alien Dimension (6,000 man-hours) only after the first crossing, then raid `k`'s topic costs 30,000 + 2,000 k (33.3 to 53.3 h at 900/h) [DERIVED]; the chain totals 396,000 man-hours = 16.5 days of one maxed large lab and is serial (each unlock waits on the previous raid) | `topic <name>` polls, research screen; y | 3 / 4 / 9 / 11 topics by run-end day 1-10 / 11-17 / 18-25 / 26+ | **VC-2+VC-3+RR-14**, RR-12+RR-13 |
| R6 | Advanced BioChem, Advanced QPL, Advanced Workshop built | 02 target model: D14.5 / D21.5 / D25.5; stretch D11.5 / D18.5 / D22.5. Conditional on recoveries, full staffing, space and the early F-1 pair. Reforecast for ST-0 delays or RG failures; these are not measured dates | facilities; p | AQPL 2 of 69 runs, workshop 0 of 69 | F-M12 |
| R7 | Required live specimens delivered into containment, by species; capture-to-storage success fraction | 02 K rules; verify the required specimen is actually stored, not merely stunned on the battlefield | containment screen and debrief; diagnostic | no trustworthy historical delivery counter | TD17, TD19; M8 CAP-17 |

**Engine today (VC-2+VC-3+RR-14, RR-09+RR-10+F-M12).** The first alien-building topic is 38,000 man-hours with no prerequisite and can start on day 0 (done about Day 6 at 430 man-hours/h); it does not exist in the original and the strategy does not use it: research it only after the first crossing and do not credit an early finish. Nothing stops the harness hiring past 50 per class or building a 7th small lab; HH9 counts it.

#### U - Artifacts and UFOs (the Transporter window)

| ID | Human-visible definition | Due | Source / reader | F0 baseline | Fidelity |
|---|---|---|---|---|---|
| U1 | First UFO recovery battle won (any type) | Day 7 at the latest (02's unlock cases); the first forced detection arrives within 110 minutes of 12:00 on Day 1 (01 §1.2) | crash-site list (UFO class label), debrief, event `recovery`; p | 7 recovery battles in 31 runs (2 won, 3 lost, 2 withdrawn) | RR-12+RR-13 |
| U2 | **First manned UFO recovered through a WON ground mission**, in particular the Alien Transporter. [EXE RR-12/13] a Probe or Scout recovery unlocks only its own topic; the Alien Control System, Energy Source and Propulsion System topics are written only by a won manned ground mission | by Day 28: Transporters join the fleet only in growth weeks 2-4 (3 + 2 + 1, cap 6) and never again [DERIVED from the growth lists the EXE and engine share, `v_ufo_consumers.md` F3, `tech_tree.md` §0.10]; destroying them instead of recovering can close the route to the Bio-Transport | event `recovery{ufo_type, manned, won}`, new topics on the research screen; p | unknown, probably about 0 | **RR-12+RR-13** |
| U3 | UFOs shot down, wrecks seen, recovery battles by outcome; lifetime kill score against 1,000 / 2,000 / 4,000 / 8,000 (shield / large shield / cloak / teleporter gates, EXE F9) | no source (03 §6, crash sites) | crash-site list, debrief, `fight()` by `mission_type`, score screen (the engine counters are audit only); p (D18 swallows recoveries) | 36 `UFO down` events over 31 runs; `recoveries` reads 0 | F9, F7 |

**Engine today (RR-12+RR-13).** A Probe/Scout recovery force-completes the three system topics and a lost or aborted manned recovery still unlocks (`game/state/battle/battle.cpp` HEAD :3639-3649, LIVE :3646-3656), so the engine tech path runs 5-13 days early: the harness must win a manned ground mission, the benchmark credits only `won`, and the audit flag `unlocked_without_win` counts disagreements. A UFO in its withdrawal band flees within about half a second: finish wounded UFOs at once (an escape costs `floor(score/4)`, EXE F7/F8). **Engine today (F9):** escort and attack-list UFOs launch without the score-gated shields, cloaks and teleporters (34% of launch slots), so once kill-score thresholds pass engine fleets are weaker than the original's and interception margins tuned on them are optimistic.

#### C - Cult of Sirius raids and sweeps (user pillar: steal to sell)

| ID | Human-visible definition | Due | Source / reader | F0 baseline | Fidelity |
|---|---|---|---|---|---|
| C1 | **First Cult raid won**: a Raid Building ground mission at a Cult-owned building ended resolved. The Cult starts hostile in all 21 seeds (relation -70..-50), so the loot gate passes by construction and the launch's -60 is transient [EXE T9, F-M11] | First safe opportunity with eight raiders beyond computed garrisons and a cargo craft. Day 7/three raids before Monday is an optimistic 01 model, not a readiness override. Default window ends at takeover (F0: 18 of 20 saves by Day 14-20, `income_cult_raids.md` §4.8); continued three-way raids require the separately tagged G-C8 arm | event `raid{owner}` (add `building=`, `owner=` to `describeBattle`); n | 0 (the harness presses EXTERMINATE only, `oa_play.py:1940-1956`) | **T1+T2**, TD1-TD3 |
| C2 | Cult raids won; loot **realised** (sold value tagged `source=cult`; the whole haul is lost without a craft carrying a Cargo Module present, `income_cult_raids.md` §3.6); squad losses per raid | mean KIA <= 1.5 and net loot >= $15,000 per raid after raid #2 (01 gates M-1/M-3, replaced by measured values) | events, ledger; n | 0 | F-M1+F-M2, TD1-TD3; post-raid `worth` debit [UNBOUND] (F-M13) |
| C3 | Sweeps resolved and **distinct infested buildings cleared** (alien icon gone from the building screen); hours alert to cleared. A sweep counts only if the alert or building screen showed the alien icon at launch. `empty_sweeps` = Investigate Building on a crew-zero building: owner -9 **with ripple to other organisations**; the alert/arrival route costs nothing; an Egg- or Chrysalis-only building costs nothing and starts no battle [EXE F08] | Government-owned infested buildings cleared within 03 §3's response times; `empty_sweeps` = 0 | event `sweep`; n | 58 resolved of 14,577 polls (0.4%) | F02+F05/F08 |

**Engine today (TD1+TD2+TD3).** Superhuman aliens and guards are unscaled in the engine (original +16% TU/HP/reactions/stamina, +64% natural armour, an accuracy rule), so engine raid casualties and one-shot assumptions are optimistic.

#### B - Bases and defence (user pillar: build new bases)

| ID | Human-visible definition | Due | Source / reader | F0 baseline | Fidelity |
|---|---|---|---|---|---|
| B1 | Bases owned and **alive**, ever owned, and those satisfying B2; credit counts only garrisoned bases | record the conditional 01 BS-2 purchase date and any RG/readiness deferral; Day 28 is a target, never an override. A third requires BS-4; every purchase requires BS-P staging | base list (Q2); y (use live `bases`, not the ratchet) | 2 from day 0 in 98 of 98 starts (the empty Warehouse Eight); never a third | BD08, BD01a/b |
| **B2** | **Each base garrisoned**: armed-verified agents present at 00:00 >= G; fraction of base-days garrisoned. **B2b**: present total (all classes) <= 36 - 4 x finished Security Stations (reserve capped at 12); above it each class is cut proportionally (10 soldiers + 40 staff field about 4 soldiers) and unarmed staff defend and die [EXE BD04] | G per base from 03 §7 (BASE rules); stage before purchase and verify the new garrison immediately, before advancing time (01 BS-P). Attackers: 4-10 (Probe, Scout, Bomber, Fast Attack) to 22 (Assault) from UFOs, 5-20 from organisations [EXE BD03/BD12] | roster location + loaded-weapon flag + base screen; n | **100 of 100** two-base saves (day >= 3) have a base with 0 loaded-armed agents present; **19 of 43** one-base saves have 0 at their only base [DERIVED, joined by presence] | **BD04+TD10+TD8+TD9** |
| B3 | Base defences: n, resolved, lost, timed out (900 s), **by trigger** (UFO landing / organisation Raid or Storm / other); minutes; casualties. Never withdraw: a retreat is a lost base in both rule sets | none lost | BASE ATTACK dialog, `fight()` outcomes; p | 22 of 35 had >= 1 (7 lost a base); 13 of 35 ended by timeout (`harness_audit.md` D19) | **BD01a/b, BD08, BD14a+BD16, BD13** |
| B4 | Silent base loss: base count falls with no defence mission. [EXE BD17] a base with no live agent present when a UFO landing or Raid/Storm fires is destroyed; it also collapses when over 90% of its tile value is gone [BD13] | 0 | live `bases` drop with no `base_defence` event that hour; n | at least 13 suspected, in builds with the HEAD movement/detection triggers | BD01b, BD17, BD13 |
| B5 | Planned facilities complete per base. No cap of 3 stations exists in the original; each finished station adds 4 turrets [EXE BD04] | main base: 2 Security Stations by Day 3 00:00, 2 Living Quarters by Day 4 00:00 (01 §1.2-1.3); each new base: 2 Stations +2 days, 2 Quarters +3, Stores +2 after purchase (BS-P) | facilities; n | none beyond the 14 starting facilities | BD04 |

**Engine snapshots (BD01a/b, BD08, BD14a+BD16, BD04+TD10+TD8+TD9).** HEAD invented movement/detection defences (13 of 15 logged benchmark defences); LIVE removed them, later committed in local `879d0d73`. This source correction does not identify old binaries. Remaining audited differences: Raid/Storm misses purchased bases until save/load; losing a defence kills present agents/craft rather than re-homing them; rosters and turret armour/weapons differ. BD08 therefore understates continuous-run risk. The EXE conditional base-target fractions are about 1/2 with one base and 2/3 with two; whether owner remapping makes this 1 remains UNBOUND (`FUN_00014854`, VA `0x1017C`). Keep garrison parameters provisional until step 8.

#### V - Victory chain, PE - people, HH - harness health

| ID | Human-visible definition | Due | Source / reader | F0 baseline | Fidelity |
|---|---|---|---|---|---|
| V1 | Alien buildings destroyed (count, days), by function. [EXE VC-1] a raid succeeds when the objective tiles are destroyed and the squad evacuates; Food Chamber frees the Mutant Alliance; Control Chamber stops weekly UFO growth and arms retaliation | 02 target model: first raid D32.8, Control Chamber D57.8, victory D77.8; stretch/late victory D54.8/D109.8. Reforecast for actual lab staffing and ST-0/RG delays. One human run crossed around day 30 and won around day 99 [GUIDE: GuavaMoment LP, https://lparchive.org/X-COM-Apocalypse/], difficulty unstated; not a Superhuman guarantee | debrief, next topic appearing; `progress.alien_buildings_taken`; objective and kill-all outcomes separately tagged; p | 0 | **VC-1, VC-2+VC-3+RR-14, TD12, TD1-TD3** |
| V2 | A crossing-capable craft ready (Bio-Transport or another with the crossing flag; no Dimension Shifter needed, EXE VC-6/VC-7) and the first crossing | 02 target model: hull D27.2, crossing D27.5, subject to the same conditions as R6/V1. D means elapsed days, not harness Day N | `progress.milestones` (`oa_victory.py:210-219`); y | 0 of 69 | RR-12+RR-13, VC-8 |
| V3 | Victory day; V3b Control Chamber raid won (UFO growth stopped) | | `wingame` video, event; y | 0 | **UFO_GROWTH_STOP, VC-10+F5, VC-1** |
| PE1 | Soldiers alive, veterans, deaths per week | | roster; n | roster swings 2 -> 13 -> 5 -> 10; 0-7 soldiers at the end of long runs | **V-AG-01** (original growth 1/2 to 1/6 of the engine's), TD1-TD3 |
| PE2 | Share of soldiers assigned to training and at home. The 10 starters are pre-assigned Combat in the original and the engine (`gamestate.cpp:802-803`); never press Physical for them [EXE V-AG-05] | | roster; n | starters only (`harness_audit.md` §4.15) | V-AG-01 |
| HH1-HH9 | wall share outside `CityView`; battle timeouts; equip-pass failures (100 + 168 + 79 + 51 log lines); hire failures; restarts and rewinds; seconds per game day; `hidden_reads`; queries per game day; **HH9 cap compliance** (employed + listed applicants above 100 / 50 / 50 / 50 or 180 persons; a base above 6 small or 4 large labs and workshops) | | `Recorder`; p | | RR-09+RR-10+F-M12 |

**Engine today (VC-1, UFO_GROWTH_STOP, VC-10+F5).** `GameWon` and the alien-building unlocks need `Battle::playerWon` (every alien dead, a conscious unit): the engine cannot win a Gate raid by destroying the generators and evacuating, so until VC-1 lands the harness must kill every alien and keep a conscious unit. Engine growth stops two raids later (Organic Factory, 8th) and the Control Chamber wave never comes: do not plan around the thinned-fleet trick. Launch with `CrashingDimensionGate=false` (VC-8) or an incapable craft at a gate is destroyed. **Engine today (TD12, V-AG-01):** alien-building reinforcements arrive four per interval (original one), so engine Gate-class raids face about four times the pressure; training gives six roll sets per agent per day (original one), so stat growth in engine logs is 2-6x too fast: budget at 1/2 to 1/6.

34 outcome milestones (G 6, E 6, R 6, U 3, C 3, B 5, V 3, PE 2), 9 harness-health diagnostics and the additional R7 delivery diagnostic. R7 has no reward weight pending DEC-1. First-priority readers: the structured ledger (E2, E3), `topic` polling by name (R5), the `recovery`/`raid`/`base_defence` events, and the per-base presence join (R1, R4, B2, B5).

### 4.4 From milestones to a score vector

Each milestone yields a `status` (attained / failed / unobserved, 3.3) and a **credit in [0,1]**: *deadline* 1 if attained by D else 0 (G1, G4, B1 garrisoned only, U2, R1, E2 sustained); *timed* 1 by D, linear to 0.25 at 2D, then 0 (U1, R6, C1, E4); *level* `min(value / target, 1)` at the checkpoint (E2 second target, R2, B2 base-days); *count* `min(n / cap, 1)` (V1, C2). The **primary output is the vector** `{id: credit/status}` per run, aggregated per arm as a proportion attained with a CI (3.7); a scalar (the Milestone Index, 5.3) only steers the learner. Terminal exits fail every later milestone, `wall_cap` leaves them unobserved, and the index is normalised over observed milestones.

## 5. Reward: mechanical fixes, versions, Milestone Index proposal

**Anything here that changes a weight, a penalty or the meaning of the objective is a PROPOSAL and needs the user's ruling** (`docs/HANDOFF.md` item 4: scoring weights were never ruled on; "ask before changing"). The mechanical fixes of 5.1 correct quantities without touching weights or intent; 5.3 cannot land without a ruling.

### 5.1 Mechanical fixes (reward v3.1: v3 weights, corrected quantities)

| # | Fix | Where | Lock test |
|---|---|---|---|
| F1 | delete `alive_at_end` (0 of 115 earned it; `day_budget` / `wall_cap` replace it) | `oa_learn.py:143-144` | `test_reward_has_no_dead_terms` (every term non-zero in some fixture) |
| F2 | delete `second_base` (constant in 99.1%); B1/B2 carry the information | `:153-154` | same |
| F3 | `infil_raids` counts polls -> `sweeps_resolved` = distinct infested buildings that showed the alien icon at launch and were cleared after a won extermination (weight 2, cap 30); sweeps at an empty building are `empty_sweeps`, a guardrail (-9 relation each, with ripple) | stop incrementing at `oa_victory.py:638-645,1258-1262`; increment in `fight()` on `mission_type == "extermination"` and `resolved` | `test_polling_is_not_a_raid` (100 `en-route` outcomes leave 0), `test_empty_sweep_is_not_a_sweep` |
| F4 | `recoveries` lost to D18 -> count in `fight()` from `mission_type == "ufo_recovery"` and `resolved`; `recover_crash_sites` catches the post-stage `HarnessError` | `oa_victory.py:612-616`, `oa_play.py:5030` | `test_recovery_counted_when_stage_changes_mid_poll` |
| F5 | `wins`/`battles` miss raid battles -> count every `BattleBriefing` in `fight()` | `oa_victory.py:377,399` | order-asserting test (HANDOFF: tests assert ORDER, not presence) |
| F6-F7 | `ufos_down` high-water mark -> `wrecks_seen` (ids from `gs crashes`), reward uses `ufos_recovered`; `funding_lost_day` is the first sampled day -> exact day from the `funding_cut` event / Monday row | `oa_victory.py:596-598`, `:1033-1037` | |
| F8 | `armed` counts any weapon -> `armed_loaded` (diagnostic), `armed_verified` (garrison measure), counted by presence | `gamestateintrospect.cpp:559-615` | `test_armed_verified_requires_clips_and_presence` |
| F9 | `exit == "signal"` ambiguous -> exit taxonomy (3.3); infra exits `valid=False` but recorded and counted | `oa_learn.py:122-128`, `make_record:662-676`, `oa_victory.py:1310` | `test_foreign_sigterm_is_not_scored` (resurrects the `2f8e5529` test) |
| F10 | `funds.*` are score points -> `pts_*`, old keys kept one release marked deprecated | `sample_metrics`, `build_final` | `test_final_json_names_points_as_points` |
| F11-F12 | `progress.bases` ratchet -> live `bases` and `bases_ever`; wall-clock throughput scores days -> days from `until_day` and exact ticks | `oa_victory.py:665`, `oa_learn.py:139-142` | `test_same_game_days_same_reward_regardless_of_wall` |
| F13 | `bases_lost` counts any outcome but `resolved`/`timeout` (a crash scored -800) -> bases actually gone (live drop + `base_lost{cause}`); `defence_unresolved` reported not scored; a crash during a defence is infra. A `withdrawn` outcome that removed the base is a real loss in both rule sets (EXE BD14a) and the live drop catches it | `oa_victory.py:402-410` | `test_crash_during_defence_is_not_a_lost_base` |

### 5.2 Versioned rewards; history that can always be re-scored

New `tools/oa_reward.py`: `WEIGHTS = {1, 2, 3, "3.1", 4}`, `reward(final, version)`, `weights_hash(version)`, `CURRENT`. Each history record stamps `schema` 2, `reward_version`, `weights_hash`, `facts` (the subset of `final.json` the reward reads, so a record re-scores without its run directory), `build_id`, **`fidelity` (vector hash and label)**, `tier`, `until_day`, `comparison_id`, `arm`, `censored` and `run_uid` = sha256(seed, genome key, started_at). `oa_learn.py rescore --root R --version N` rewrites `rescored-vN.jsonl`; `update_model` and `genome_table` refuse to mix versions **or vectors** (FP3). `oa_learn.py ingest --scan DIR...` appends the records `execute()` never wrote: mass-stopped finals as `legacy_censored`, finals lacking a v3.1/v4 field `partial`, legacy runs classified by day-0 fingerprint (2.3); `partial` and `unknown` records are never pooled with complete ones. Era fixtures (`tools/testdata/final_era{A,B,C}.json`) pin `test_reward_versions_reproduce_history` (`show/r0000`: -143 under v2, -1,843 under v3). `final.json` schema 2 renames `time_budget` to `wall_cap`, adds `day_budget`, `exit_class`, `pts_*`, `armed_loaded`, `bases_ever`, `infil_polls`, `sweeps_resolved`, `empty_sweeps`, `wrecks_seen`, `ufos_recovered`, `defence_unresolved`, and `manifest`, `fidelity`, `milestones`, `gene_decisions`, `hidden_reads`, `queries_by_name`, `restarts`, `rewound`, `s_per_game_day`.

### 5.3 Proposal: the Milestone Index (reward v4) - NEEDS USER RULING

The strategy era needs an objective that rewards *executing the calendar*, is defined for censored runs, has no poll counters and is not dominated by three penalty terms. Anti-gaming: sweeps count only with aliens present, a base counts only when garrisoned, nothing rewards holding fire, and selling cannot be credited by trickling a frozen-price good (3.7).

**Tiers T1-T3:** `reward = 1000 x MI`, `MI = sum_i w_i credit_i / sum_{i observed} w_i` (4.4). **T4:** `1000 x MI` plus the unchanged victory terms (`10000 + 10 x (400 - day)`, 150 per alien building up to 10, 300 for the crossing). Weights are identical across vectors; the Index is never compared across vectors.

| Group (proposed weight) | Milestones and weights (percent of the Index; sum 100) |
|---|---|
| Governance 30 | G1@15 **8**, G1@30 **8**, G1@60 **6**; G4 at Mondays d14 / d28 / d56 / d84 **2** each |
| Economy 20 | E2 sustained by Day 69 **6**; E2 at 1.2 x burn by Day 83 **6**; E4 manufacturing started **4**; first manufactured goods sold **4** |
| Capacity and research 20 | R1 labs by the pre-registered ST-0-relative deadline **4**; R2 **2**; R4 **2**; R6 **2** each (three items); R5 the 20 entries, time-credited, **6** in total |
| Artifacts 10 | U1 **3**; U2 first manned recovery won by d28 **5**; U3 two recovery battles won by d35 **2** (placeholder date) |
| Cult and sweeps 8 | C1 **3**; C2 **2**; C3 Government-owned infested buildings cleared within the response times of 03 §3 with `empty_sweeps` = 0 **3** |
| Bases and defence 12 | B1 (garrisoned only) **3**; B2 garrisoned base-days **4**; B3 no base lost and no defence failed through d42 **5** (placeholder date) |

Properties (each a test): every term counts a distinct event or a state-day, never a poll; no term is constant across runs; earlier attainment never scores less; credit already attained is never lost later in the same run.

| # | Question (all need the user) | Current v3 | Proposal |
|---|---|---|---|
| RUL-1 | replace `survival_per_day` (8/day) by milestones? | 8/day while funded; slope +7 to +18 per day within a version (1.1) | yes: G1/G4 carry survival |
| RUL-2 | base-defence penalty | -400 per defence fought, -800 per base lost, -1000 defeat (37.3% / 25.5% / 35.1% of pooled variance) | penalise **lost or unresolved** defences, not being attacked: the expansion pillar makes more bases and so more defences, and a per-defence-fought term taxes exactly that. Fought-and-won 0; 900-s timeout unresolved; a crash is infra (F13). Keep or drop the flat defeat penalty (the Index zeroes later milestones) |
| RUL-3..7 | Index weights; time-credit shape (linear to 0.25 at 2D); `score_per_point` (0.05, clamp +-3000); which allow-list defines a clean run (P7); fix A/Bs with pinned or retuned policy (FP5) | | as tabled; keep score out of the Index (already inside G4/G5) and report it as a guardrail; 05's `HUMAN_SURFACE` with the 05 §5.3 H-reads tagged until removed; pinned, retune as a separate A/B |

**Scalar versus vector.** Decisions between arms use the pre-registered primary indicator (3.6), never the Index, which only steers the learner; its variance is measured in the A/A and 1.2 is recomputed before any learner budget is spent.

## 6. Learner changes

### 6.1 Pause the all-gene Thompson loop; the learner proposes, the judge decides

15 genes, 16 runs per generation, SD 272-483 pooled and 436-882 in the current harness: a (gene, value) cell needs `n = 15.7 (SD/Delta)^2` runs, so with SD 272 and a 12-run cell the smallest detectable difference is 311 reward points, about the size of the defeat penalty. Tightening the champion rule does not help [DERIVED, flat landscape, SD 272]: the live rule (`c = 1`, `min_evals = 2`, 5 generations) gives a +332 median apparent gain with P(> +200) 0.76; `c = 2` gives +325 and 0.73; `c = 2`, `min_evals = 5`, 10 generations +342 and 0.75. Adaptive re-evaluation keeps spending on lucky genomes, so **only an independent, pre-registered confirmation on fresh seeds removes the bias.**

- WHEN the learner names a candidate THEN it enters the Hall of Fame only after `oa_bench` ran it against the control at the current vector and the efficacy boundary was crossed; `model.json` stores `judge{cid, p, ci, looks, vector}`; unjudged = `candidate`, never `champion`.
- WHEN a phase starts THEN at most **4 genes are live**; the rest are frozen and canonicalised away (extends `Strategy.canonical`, `oa_strategy.py:164-166`).
- Acceptance test `test_learner_does_not_crown_noise`: flat landscape, SD 272, 5 x 16; live code fails it (77% above +200); the new loop must show an unjudged Hall of Fame and a judge false-positive rate <= 3% over 1,000 simulated sessions (3.6: 1.2%).
- Re-enable Thompson sampling only when the A/A is done and the scalar's SD measured, one structural change passed the judge, live genes <= 4 and every live cell has >= 12 runs. Genes whose right value depends on a fidelity-sensitive rule are re-screened at each new vector (05 G5 profile genes).

### 6.2 Gene lifecycle: every gene traced to a call site, measured to fire

`Gene` gains `horizon_day`, `tier`, `moves` (milestone ids) and `status` (`live`/`frozen`/`proposed`). **Counterfactual counter:** the bare gene reads (`oa_victory.py:789,530,537,638,680,745,831,866,875,1222`) become `self.strategy.decide(name, fn)`, which evaluates `fn(value)` and `fn(default)` and increments `decisions_changed[name]` when they differ (mirrored into `progress.json`); `final.json["gene_decisions"] = {gene: {reads, decisions_changed}}`. **Manipulation check:** a treatment run whose non-default gene has `decisions_changed == 0` is `treatment_inert`; the A/B reports the inert fraction (>= 90% must have fired) and the learner credits evidence only to the effective key. For a `plan` preset the check counts orders in 05's `decisions.jsonl` whose `rule` belongs to the preset. **Lint** `tools/check_dead_genes.py`: every gene's `read_by` is reachable from `Victory._run`, no access bypasses `decide()`, every gene has an order-sensitive test (pattern `test_air_patrol_gates_interceptor_purchase`, `test_oa_learn.py:297`); `check_dead_defs.py` is extended to methods and excludes tests from the reference set (`harness_audit.md` D15). **No gene without a call site:** it enters `GENES` in the commit that adds its call site, `decide()` test and fired-counter assertion; one that never fires within its tier's horizon in the A/A is demoted to `frozen`.

### 6.3 The existing 15 genes

| Gene (default; values) | Fires by day 21? | Disposition |
|---|---|---|
| `air_patrol` (8; 4,6,8,12), `min_soldiers` (18; 12,18,24,30; saturates at 30 beds), `min_squad` (6), `infil_min_armed` (3), `infil_speed` (3) | yes, from day 0 (`oa_victory.py:789,745,1222,638,866,875`) | live, T1/T2; `min_soldiers` couples to R4 |
| `garrison` (4; 2,4,6,8) | yes (`crew_transport`); bypassed by `select_armed_squad` (`harness_audit.md` D5) | live, T1; redefined **per base**; sizing is vector-dependent (BD04, TD10) |
| `intercept_min_relation` (25; 0,15,25,40) | only below the threshold; after a takeover the relation is pinned near -100 (EXE T3) so every value 0-40 holds fire permanently (972 "holding fire" lines, 84% at relation <= -50), letting incursions grow 5x (-166 to -3,145 per day bucket, `failure_forensics.md` §4) | **frozen at 25** and read only by `plan=legacy`; other plans use `fire_profile` (6.4). In the original a UFO kill ripples about +2 relation per UFO with the Government and Megapol (engine +0.6, EXE F06), so shooting UFOs raises those relations |
| `research_order` (balanced) | yes | live at T3 only; `min_lab_skill` (1100) is inert (start skill 1,159 > 1,100): **frozen** |
| `gate_craft_order`, `cross_min_crew` | need an Advanced Workshop and a crossing-capable craft (no Dimension Shifter, EXE VC-7); 0 of 69 runs got there | **frozen** (T4 only) |
| `tactical_ai`, `fire_mode`, `behaviour`, `withdraw_ratio` | every battle | **frozen** at the battle arena's champion (about 5 min per battle against about 33 min per T2 campaign); re-run the arena after TD1-TD3 (DEC-4, 10.2) |

### 6.4 05's new genes: phases, tiers and the milestones they move

05 §3.7 and Appendix A own the gene definitions, their `requires` plan, the horizon test (G3) and the **profile genes** (G5: a gene whose right value depends on an engine rule defaults from the provenance vector and is canonicalised away once the fix is present, so the learner never samples a dimension a fix has made constant). This file assigns tiers and phases and demands a milestone per gene (a gene that moves none is not added).

| Gene group (05 §3.7) | Moves | Tier |
|---|---|---|
| `plan` {legacy, contain, contain_econ, full} | all | T2 -> T3 -> T4 |
| `fire_profile`, `hold_fire_label`, `hold_fire_max_h` (replace `intercept_min_relation` outside `plan=legacy`) | G6, G1 | **T1 (S8), then T2**: S8 starts after the takeover and cannot tell "hold only while the Government is intact" from "never hold"; T2 can |
| `squad_floor`, `squad_floor_gov`, `alert_sla_h`, `sweep_draw`, `garrison_fielded`, `garrison_floor` | G1, G2, C3, B2, B3 | T1 |
| `expansion_day`, `expansion_b3_day` (BS-1..BS-4 gates; Day 28 is a reconsideration target, not an unconditional deadline) | B1, B2, B4, R1 | T3 or later if gates cannot fire sooner; T2 only with a demonstrated horizon effect |
| `cult_raid`, `cult_raid_*` (a Cargo-Module craft at the building or the loot is lost) | C1, C2, E3 | T2/T3 |
| `factory_mode`, `engineer_target`, `sell_mode`, `reserve_weeks` | E2-E4 | T3 |
| `training` (never presses Physical for the starters, who are pre-assigned Combat, EXE V-AG-05; growth budgeted at 1/2 to 1/6 of engine logs, V-AG-01) | PE1, PE2, B2 | T2 |

This file asks 05 to add three genes it lacks: `recovery_priority` {any, manned_first, transporter_first} (`recover_crash_sites` target choice, `oa_play.py:5030`; U1, U2; T2), `lab_plan` {start, 2x2, 2x2+adv} and `quarters_buffer` {0, 10, 20} (`build_facility`, `oa_play.py:2333`; never above 6 small / 4 large per base; R1, R2, R4; T2).

**Presets are the unit the ladder resolves** (effects of tens of points); the structural flags are components, ablated one at a time inside the winning preset when a milestone regresses. **Hard constraints (not genes):** every base purchase enforces 01 BS-P and 03 §7: stage the full computed garrison, buy and immediately disembark/read back before time advances, then build two stations and the kit; never strip another base to do so. A gene may vary how many bases and when, never whether to garrison. Only 03 INF-0's explicit six-out/four-home startup exception and separately tagged risk arms may draw below normal depth. No preset leaves a base empty, withdraws from a base defence (EXE BD14a) or exceeds the caps (HH9). **Phases:** 0 (T2 then T3) the flagship `plan` preset against `plan=legacy`; 1 (T1) hold-fire, floor and sweep genes; 2 (T2/T3 or later when needed) expansion, factory, Cult, training and lab ablations, each as a two-arm A/B with a demonstrated horizon effect.

### 6.5 Learner mechanics

**L1** `DEFAULT_SD` 150 -> 480 and `MIN_SD` 25 -> the measured value per tier (`oa_learn.py:181-182`; cold start only). **L2** `champion()` -> `candidate()`, `c = 2`, `min_evals = 5`, Hall of Fame only through the judge (`:244-250`, `update_model:471-498`). **L3** a control arm in >= 25% of slots of every generation, not every third (`propose:329-366`). **L4** `propose()` samples only genes with `status == live` at the run's tier and `horizon_day <= until_day`. **L5** `plan_specs` stores `tier`, `until_day`, `build_id`, `reward_version` and the fidelity vector so `--resume` cannot continue under another objective or engine (`:683-707`); the interrupt path writes an `aborted` stub per live run (`execute :604-659`).

## 7. The staged ladder

### 7.1 Tiers

T1-T4 are the budgets of 3.3; T1 scenario saves are screening evidence that never promotes a genome (6.1). Below them: **T0** unit tests of pure policy functions on recorded `ui` snapshots (no game, suite under 5 s); **TP** probes, one-purpose scripted runs on the existing `Driver` (05 M2b `oa_probe.py`) plus the same facts recorded passively in every campaign, which measure the parameters 01-03 tagged `ASSUMPTION(M-n)` or [MEASURE] and the manipulation check of each fidelity atom (2.4); a stamped fact sheet replaces an assumption before any T2 comparison that depends on it.

### 7.2 T0: pure policy, recorded snapshots

Today the policy is interleaved with I/O inside `Victory.city_turn` (`oa_victory.py:553-885`) and tests need `FakeHarness`/`FakeDriver` (`test_oa_learn.py:223-296`). T0 needs 05's seam `observe() -> Snapshot` (an immutable dataclass whose fields are a subset of the `ui` block of 4.2) and `decide(snapshot, strategy, clock) -> [Action]`. Golden snapshots are the `ui` blocks of real `timeline.jsonl` rows; a test fails if a fixture carries a key outside the allow-list. Executable tests (ORDER, not presence):

- WHEN `gov.inf_pct > 0` and a Government-owned building is on the alert queue (the alert shows its owner) THEN the first `Sweep` targets it. The order holds at every vector; what changes is how long the harness may wait (F0: any nonzero value is flip-eligible; original: above 50%).
- WHEN a base has `present armed-verified < G(base)` THEN no `Dispatch` takes soldiers from it and no base purchase is issued; WHEN a `BaseDefenseScreen` battle is open THEN no `Withdraw` is issued (EXE BD14a).
- WHEN the Government is taken over (takeover notice, or Hostile with the graph at 100%) THEN `intercept` actions are not suppressed by any hold-fire rule (the `oa_victory.py:828-837` gate fails this today for every gene value 0-40).
- WHEN an alert's alien icon is off or the building already shows cleared THEN no `Sweep` or `EXTERMINATE` press is issued (-9 relation with ripple per empty investigation).
- WHEN `clock.next_monday_roll_in_h <= 1` and the score margin to -2,400 is < 600 THEN the Sunday close-out actions precede the roll; WHEN funding is terminated THEN no hire, build or purchase is issued and a sell is [EXE G8: four Mondays of cash remain]; WHEN a type's displayed price fell on a Monday THEN no sale of that type UNTIL a Monday passes with its price unchanged (3.7).

### 7.3 T1: scenario saves

**Mechanics.** A fixture is a `CityView` save plus `scenario.json` (`build_id`, **fidelity vector**, `day`, `seed`, preconditions, derived harness `flags` such as `second_base`). A run copies it into a fresh out dir as `victory.save`; `Victory.start()` resumes it (`oa_victory.py:256-310`) and asserts the manifest before acting. **`CityView` saves only** (`Victory.save` refuses other stages, `:226-253`); battle fixtures, when unavoidable, load **paused** and the loader presses `BUTTON_SPEED3` first. A fixture is refused on another build **or vector** (FP9). Harvest rules read engine state from saves offline; that selects fixtures and is not a milestone.

**Replicates need an engine option.** `rng` is serialised (`gamestate_serialize.xml:59`) and `RngSeed` is read only in `GameState::startGame()` (`gamestate.cpp:593`), so replicates of one save share the engine RNG and differ only through harness timing. Requirement: `OpenApoc.NewFeature.ReseedOnLoad=<n>` calling `rng.seed(n)` after a load, passed by `GameProcess` next to `--OpenApoc.NewFeature.RngSeed` (`oa_play.py:683`). Without it every T1 result must state that replicates are not independent futures.

| ID | Observable state at load / harvest rule | Horizon and outcome | Replicates | Corpus today |
|---|---|---|---|---|
| S1 | day 6-10, Government graph at 0, >= 5 infested buildings (`gov_inf == 0, infiltrated >= 5`) | to day 18: Government intact; distinct buildings cleared | 24+ per arm | 25 saves (older builds) |
| S2 (F0 band) | day 7-16, Government raw infiltration 1-7 | 4 days: not taken over. Engine today (T1+T2): constant-`inf` takeover probability `1-(1-inf/200)^(11.52 x 4)` = 21% at 1, 50% at 3, 81% at 7, 96% at 14 [DERIVED], so a wide band lets fixture variance swamp the treatment; equal replicates of every fixture in both arms, **analysed stratified by fixture** | 4 fixtures x 7 | |
| S2' (T1+T2 vectors) | day 7-16, Government displayed 51-55% (raw 102-110) | 4 days: original-rules probability `1-(1-(v-100)/101)^19.2` = 32% at raw 102, 87% at 110 [DERIVED, constant `v`]; exactly 0 at or below 100, near 1 above 110 | 28 per arm | harvest at the new vector |
| S3 | day 7-14, >= 8 infested buildings, an **empty second base** within 15 tiles of an infested one | 5 days: base alive; silent loss events | 24+ | 45 broad matches |
| S4-S7 (need code that does not exist yet) | S4 day about 14, base defence open with 15-25 unarmed staff vs 1-2 aliens (battle save, paused); S5 day about 20, Transporter window with none recovered; S6 day about 30, Cult temple located, squad and Cargo-Module craft ready; S7 day about 35, Advanced Workshop built and a product researched | S4 to resolution (resolved? casualties); S5 8 days (U2 by day 28); S6 3 days (C1, C2, casualties); S7 7 days (E4, E2) | 16-24+ | none |
| S8 | day 15-25, Government taken over (Hostile, graph at 100%), >= 3 UFOs in the city, armed fliers (`gov_taken, ufos_in_city >= 3`) | 5 days: `pts_incursions`, city damage, craft lost per day; arms differ only in the hold-fire profile | 24+ | 32 saves (older builds) |

The corpus holds usable S1/S2/S8 saves from assorted binaries and vectors: **regenerate them** with `oa_scenarios.py harvest <runs-dir> --scenario S1` from the weekly checkpoints of control-arm T2 runs at the target vector; pass criteria are pre-registered per scenario.

**Probes (TP): what each 01 measurement needs from the `Recorder`.** M-14 income case: start fingerprint (2.2) and E1 at Monday #1. M-1 Cult net loot and M-3 KIA per raid: `raid{owner=cult}` event, `source=cult` ledger rows, `survivors`, and the balance before launch and after debrief (bounds the unbound `worth` debit, F-M13). M-5 specimen timeline: first game day each specimen or item appears in the stores. M-7/M-8 price probes: 05 Q6 `market` rows at Sunday 23:00 and Monday 00:10. M-10 recruit pool: 05 Q5 per role, daily (checks the flat 1.5 arrivals per class per day, EXE RR-10). M-11 base purchase: event `base_bought{price, funds_delta, minutes_to_garrison}`. M-12 turrets: `base_defence` event with turrets seen and kills. Fidelity checks (2.4): stray-fire relation probe, Probe/Scout recovery topic probe, Psiclone price probe, 7th-small-lab refusal.

## 8. Files and functions to change

Line numbers are for the LIVE snapshot and drift; each row names the function. A row is not done until its lock test fails without it, proved behaviourally, not by an ImportError (`docs/HANDOFF.md` #5).

### 8.1 Harness (Python, `tools/`)

05 §6.3 owns the harness changes behind its milestones (clock, executor, sweep queue, loadouts, D18). This file creates the files below and needs five hooks of its own (LIVE function names, lines as of the snapshot).

- **New files.** `oa_reward.py`; `oa_milestones.py` (`MILESTONES`, `derive_milestones`, `credit`; last-written row per game day wins); `oa_telemetry.py` (`Recorder`, the only caller of `tele_*`); `oa_stats.py`; `oa_bench.py` (`prereg`, `launch` in interleaved 8+8 batches on randomised slots, `looks`, `compare`, `aa`, `ledger`); `oa_savescan.py` (presence join, completion test, wreck list; a prototype parsed 160 of 163 existing saves in 9.0 s [DERIVED]); `oa_scenarios.py` (`harvest`, `verify`, `run`); `check_dead_genes.py`; `check_fidelity_keys.py`. 05 owns `GameClock`; this file requires `clock.due(name, game_minutes)` keyed on `gs time` ticks and adds no second implementation.
- **Hook 1, `oa_victory.py` `main :1317-1353`, `_on_sigterm :1313`, `start :256-310`, `_run :1145-1306`.** `--until-day`, `--wall-cap-hours` (`--hours` alias, reaching it exits `wall_cap`), `--tier`, `--scenario`, `--manifest`, `--shims`; read the reason in `STOP_BY_ORCHESTRATOR`; write `final.json.provenance` with `fidelity`; abort `provenance_mismatch` unless engine difficulty and `gs fidelity` match the arm; start fingerprint; the day-budget exit; declare `bankrupt` when balance < -5,000 after a Monday (`bankrupt :929-958`, marked `harness_declared`).
- **Hook 2, `oa_play.py` `Driver.gs :222`; `GameProcess :619-742`.** Route through `HUMAN_SURFACE`, count `queries_by_name`, raise on `tele_*`; pass `ReseedOnLoad` and `--OpenApoc.Fidelity.*`; record the binary hash.
- **Hook 3, `oa_victory.py` `fight :376-414`, `sample_metrics :986-1042`, `build_final :1050`, `save :226-253`, `restart :321-374`.** Count every battle once and emit the 4.2 events; schema 2; weekly `checkpoints/d{DDD}.save` (CityView only); `progress["rewound"]` and superseded rows; delete the poll counters at `:596-598`, `:612-616`, `:638-645`, `:1258-1262`.
- **Hook 4, `oa_learn.py`.** `reward :112-174` and `REWARD_WEIGHTS :82-105` move to `oa_reward.py`; `make_record :662-676` and `execute :604-659` store the 5.2 fields and every exit class; `ProcessRun :531-581` passes `--until-day` and `--shims`; `noise_sd`, `champion`, `update_model`, `propose`, `plan_specs` get L1-L5; subcommands `ingest`, `rescore`, `ab`.
- **Hook 5, `oa_strategy.py` `Gene :39-55`, `Strategy :129-203`.** `horizon_day`, `tier`, `moves`, `status`, `decide(name, fn)`, `canonical(tier)`, `effective_key()`.
- **Lock tests** (each fails on the pre-change tree, proved behaviourally rather than by an ImportError), for example `test_fidelity_mismatch_aborts`, `test_wall_cap_is_censored_not_survived`, `test_policy_cannot_read_telemetry` (AST scan both ways), `test_reward_versions_reproduce_history`, `test_learner_does_not_crown_noise`, `test_learner_refuses_mixed_vectors`, `test_bench_voids_mixed_vectors`, and known-value tests of `oa_stats` (Pocock K = 4 -> 0.0182; Fisher on the 1.6 table -> 0.041).

### 8.2 Engine (C++), harness-support only (separate from `04-engine-fidelity.md`)

`game/state/gamestateintrospect.cpp` (`introspectGameState :724-1472`): `tele_orgs`, `tele_bases`, `tele_finance`, `tele_stats`, `tele_agents`, `tele_start`, `tele_building`, the metadata query `fidelity`; `armed_loaded` in `describeAgents :558-616`; `building=`/`owner=` in `describeBattle :618-688`; `pts_` names in `describeFunds :274-299`. `game/state/fidelity.cpp` (new): `FIDELITY_FIXES`, option readout, `OpenApoc.Fidelity.<Key>` gates where cheap. `game/state/gamestate.h/.cpp` and `gamestate_serialize.xml`: a serialised, read-only `CampaignStats` (shot-down / recovered counts by type and `won`, `raids{owner, outcome}`, `baseDefences{trigger, outcome}`, `basesDiedSilently`, `damageByFirer{xcom, alien, vehicleFall}` isolating SCORE-C6, `incidentsByOwner{xcom, other}` isolating SCORE-C2, sales, wages, upkeep, manufactured counts, `takeovers{org, tick}`, `growthStoppedAtRaid`); audit only, and until it exists the harness derives the same facts by snapshot diffs. `framework/options.cpp` (next to `RngSeed :492`) and `GameState::loadGame`: `OpenApoc.NewFeature.ReseedOnLoad` -> `rng.seed(n)` after a load.

### 8.3 Order of work (each step has an exit test; none is a rewrite)

Everything that alters what the harness *does* lands **before** the A/A: the A/A must measure the build that will serve as the control. Steps 1-2 are 05's M0-M2; the engine fixes of `04` run beside them (05 §6.4).

1. **Provenance with the fidelity block, exit taxonomy, `--until-day`, `STOP_BY_ORCHESTRATOR`**, `fidelity.cpp` with the two-way lint (05 M0/M1; L6-L7). Exit: a 21-day run ends `day_budget`; a foreign SIGTERM ends `foreign_signal`; an interrupted generation leaves `aborted` stubs; the F0 manifest records declared and observed vector.
2. **`GameClock` cadence, `Recorder`, `tele_orgs`/`tele_bases`, `oa_savescan`, milestone derivation** (no policy change). Exit: `milestones.json` for a smoke run with G1, G2, G4, R5, U1, U2, B1, B2 non-empty.
3. **Freeze `baseline-v0`, run the A/A at F0** (16 runs, T2). Exit: `bench/baselines/<F0>.json`; 1.7 and 3.6 regenerated from it.
4. **Reward v3.1, `oa_reward`, `ingest`/`rescore`** (parallel with 2-3). Exit: era fixtures reproduce; all 115 finals ingested and classified by fingerprint.
5. **Scenario harvest S1/S2/S8, `ReseedOnLoad`, the observe/decide/execute seam.** Exit: 28 replicates of S2 with the A/A's null distribution; the first cheap A/B is the hold-fire profile on S8.
6. **First AI A/B (T2) at F0:** the flagship `plan` preset against `plan=legacy` in the same build, 4 looks, shims on (`pre_gate_a`: a diagnostic). Fix A/Bs follow `04`'s order: each fix gets its manipulation check, a new A/A and a pinned-policy A/B.
7. **First 2x2 certificate** at the first Gate-A vector (2.6); 05 M10 re-runs the M3-M9 comparisons there.
8. Only then: the Milestone Index (after the ruling), `CampaignStats`, and the (<= 4-gene) learner.

## 9. Stale or wrong in existing documents and comments (benchmark-relevant)

| Where | Says | Reality |
|---|---|---|
| `oa_learn.py` "15 genes, all traced to a call site"; `oa_strategy.py` "NO DEAD KNOBS"; `reward()` "each capped so no one term can swamp the rest" | every gene is live; terms are balanced | `min_lab_skill` is inert (start skill 1,159 > 1,100); `gate_craft_order`/`cross_min_crew` cannot fire before an Advanced Workshop and a crossing-capable craft (0 of 69 runs); `intercept_min_relation` is permanently engaged after a takeover; three penalty terms are 97.9% of the variance |
| `oa_learn.py` weights `milestone_workshop` "gate on the dimension shifter", `milestone_shifter` | a Dimension Shifter gates the crossing | the equipment is read only by UI text builders; a craft crosses iff its crossing flag is set (EXE VC-6/VC-7): the terms reward a milestone that does not matter |
| `failure_forensics.md` §0.6/§11 and `harness_audit.md` R2 "runs are cut off by the learner's wall clock", "survival rewards game days reached"; `oa_learn.py` "history.jsonl: one record per finished run" | a per-run budget; 8 per day alive | one mass stop per generation (1.5); 8 per *funded* day, and within a version reward rises with days reached (1.1); 106 of 115 finals have no record |
| `campaign-plan.md` §10.4 calendar and "the original's alien AI does not learn"; `docs/baseline-2026-10.md` "funds 130000"; `income_base_expansion.md` §0.7 base schedule; `income_cult_raids.md` §0.2 "Hostile in 20/21 draws" | Novice start (140,000, 92,000/week); static alien AI | Superhuman starts at 100,000 with 80,000 weekly (EXE G4; the benchmark logs 180,000); the original refits `BRAIN.DAT` across battles (EXE V-AG-14); 01 BS-2 and 03 §7 supersede the schedule; all 21 seeds start the Cult at -70..-50 (EXE T9, F19) |
| `Victory.city_turn` "a ground raid costs no relation with the owner"; `alertscreen.cpp:60-62,84` | | a *poll* is counted as a raid (0.4% resolved); Investigate at a crew-zero building costs -9 with ripple, while the alert/arrival route costs nothing in both (EXE F08) |

## 10. Interfaces with the other sections, and decisions needed

### 10.1 Interfaces

- **05 owns** the `engine_fixes` map and `oa_provenance.py`, `GameClock`, `HUMAN_SURFACE` + `test_oa_no_cheat.py`, Q1-Q14 and the hidden-read removals. `fidelity.fixes` is 05's `engine_fixes`; this file adds the binary's self-report (carried by Q10 `build`), options, shims, vector hash and PREREG fields, and the Recorder-only `tele_*` rule; no second implementation of any of them. 05 §3.7's `fire_profile=auto` and other G5 profile genes must resolve from `--shims`, not from provenance, so the policy never reads the manifest (to be aligned in 05).
- **This file specifies** `--until-day`, the `day_budget` exit and the game-day checkpoints; 05 M1 lands the minimal version if this has not, one implementation. Counters: 05's `infil_polls`/`infil_battles` plus the verified `sweeps_resolved`/`empty_sweeps` (F3).
- **Benchmark rule.** 05 §6.1 rule 4 defers to this file: a 16-window batch suffices for the A/A and operational scenarios; a survival verdict uses the 3.6 sample sizes (16 per arm reach 80% power only for about a +55-point move, exact Fisher 0.79 for 0.10 -> 0.60); a one-armed result is never accepted; `--until-day 28` is an exploratory T2 variant, the primary stays at Day 21 where F0 is 0.10 rather than 0.00.
- **Scoring after the funding cut.** 05 C-21 `effective_end_day` = first of `funding_lost_day` and `bad_debt_end`; this file scores to `bad_debt_end` (the `bankrupt` exit) only: funding loss alone does not end the original game and E2 independence is the user's pillar. **Gate A** (05 §6.2): AI-versus-baseline verdicts count only at a Gate-A vector (2.6); earlier ones are `pre_gate_a`.
- **01-03 and `04`.** Every Due cell names its rule and a change edits `MILESTONES` only; 03 §11 (KPIs and experiments) should reference G1-G3, C3, B2-B4, U3, `empty_sweeps` rather than redefine them; fix atoms and order are `04`'s (the keys of 2.1, the constraints of 2.6).

### 10.2 Decisions needed from the user (each blocks something named)

| # | Decision | Blocks |
|---|---|---|
| DEC-1 | Reward rulings RUL-1..RUL-7 (5.3): stop penalising defences *fought*; Index weights; pinned vs retuned fix A/Bs | reward v4, the learner's objective, fix certification |
| DEC-2 | Is `healthy@D` the right primary per vector, or which failure mode should T2 guard once T1+T2 makes Government loss rarer? | PREREG per vector |
| DEC-3 | Ratify the human-visible allow-list now, or require removal of the 05 §5.3 hidden reads before runs count as "no cheat" (RUL-6) | P7, the cleanliness label of every result |
| DEC-4 | Freeze the four battle-doctrine genes at the battle arena's values and re-run the arena after TD1-TD3? | the campaign search space |
| DEC-5 | Machine and time budget (hours per week; is 16 windows fixed? a T2 comparison is 4.5-6 h, a 2x2 certificate 9-12 h); harness-support commits (`CampaignStats`, `tele_*`, `fidelity`, `ReseedOnLoad`) ahead of `04`; strategy changes as default-off flags and shims that switch off as fixes land | every schedule in 3.8, the Recorder, FP1, P1 |

## Open measurements

[MEASURE] and [UNBOUND] items this section depends on. Each is produced by the A/A (3.5), a TP probe (7.3) or a `Recorder` field; none is assumed.

| Item | Why it matters | How measured / lead |
|---|---|---|
| Incursion rate (F6 double drain: about 3.5 versus 7 per week) and the first UFO's hour | cost model, turbo, intercept load | incursions per game week in the A/A [MEASURE] |
| `p0` of every endpoint at every new vector (including Government survival after T1+T2), `divergence_day`, seed ICC, SD of the Milestone Index, `s_day`, the harness's own loot-sale income (E2) and the frozen-priced share of E3 | power, primary selection (3.6), pairing validity, learner budget, cost model | A/A per vector [MEASURE] |
| Real staffed workshop rate (`evidence/resolved-facts.md` §10.3 assumes 400 man-hours/h) | E4, F-M6 margins | man-hours/h with 5 and 10 staffed engineers [MEASURE] |
| Loot spots per Temple of Sirius raid (12-23); net loot and KIA per raid (01 M-1, M-3) | C2, E3 | count loot items on generated raids (`battlemap.cpp:548-880`); TP raid probe [MEASURE] |
| Post-raid `worth` debit and tiered relation hit (F-M13, F10); loot recovery and cargo capacity (F-M14) | C2 net value | balance before launch vs after debrief; [UNBOUND] leads: who repopulates the mission descriptor at VA `0x244020`, TACP `FUN_00015494`, TACP end-of-mission item recovery |
| Initial alien fleet at new game and Transporter supply (growth adds 3 + 2 + 1 in weeks 2-4) | U2 window | first Transporter sighting day and count in the A/A [MEASURE] |
| Whether `topic <name>` answers for every visible victory-path entry and whether "present armed-verified" and `inf_pct` are on screen (05 `surface_proof`) | R5, B2, G2 | harness probe [MEASURE] |
| Original-trigger rate of base defences and silent losses at a BD01a/b + BD08 vector | B3, B4, RUL-2 | A/A [MEASURE] |
| P(base given Raid/Storm): 1/2 versus 1 | garrison expectations | [UNBOUND] lead: `FUN_00014854` against the owner remap at VA `0x1017C` |
| Alien-building reinforcement intervals (TD14); base-defence capture (TD20); dimension return timing (VC-13, RR-17); mass-assault targets (VC-10) | V1, V3 | [UNBOUND] leads: UFO2P tactical-start-file writer; `FUN_000b44a4`; `FUN_00010908`, `FUN_000114cc`, `FUN_000105b8`; `FUN_0006d994`, writers of `0x143D40` |
| Hold-fire profile before a takeover (hold only while the Government is intact versus never hold) | G6, G1 | T2 [MEASURE]; S8 cannot separate them |
