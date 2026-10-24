# 04 - Engine fidelity: what the rebuilt game gets wrong, and the order to fix it (Superhuman, engine difficulty 4)

**Summary.** The Superhuman strategy (files 01-03) targets `UFO2P.EXE` / `TACP.EXE`. The final-verdict summary contains 193 findings: 58 match, 81 diverge, 25 engine-missing, 12 engine-bug, 16 unbound and one partly compared. Unknowns are not proven mismatches; `UFO_GROWTH_STOP` consolidates existing rows rather than adding a 194th mechanic. This file orders the fixes by their effect on the player: Group A removes artificial hardship, Group B removes engine advantages, and Group C covers mixed effects or bookkeeping. Each fix names evidence, implementation ownership, a regression check and its strategy impact. Proposed fidelity flags and telemetry keep engine changes separate from AI improvements.

Four things to know before reading on:

1. **The benchmark's biggest depressions are engine artefacts.** LIVE starts at 180,000 with 10,000 weekly income instead of 100,000 and 80,000 (`G4`: Monday net about -12,900 instead of +57,100 [DERIVED]); organisations flip at any infiltration above 0 every 125 minutes instead of only above 50% displayed every 300 (`T1+T2`: a +2/h Senate infestation flips the Government in a median 19 h against about 87 h [DERIVED]); stray fire costs 5 times the original's relation (`F02+F05`). `failure_forensics.md` section 2 ranks the takeover first (50 of 162 runs) and base defence second.
2. **The committed lineage has moved.** The brief treats LIVE as uncommitted work. Observed 2026-10-08T07:39Z: the LIVE worktree is branch `khallmark/develop-reboot` at `bce89743`; its former in-flight work is six commits above `fda36d6c` (`cf56132f`, `879d0d73`, `904abc3a`, `5500ce24`, `42c48e7f`, `bce89743`), including the funding rewrite, the base-defence hunk, score category 7 and `tests/test_weekly_funding.cpp`. This branch (`khallmark/strategy-engine-plan`) is documentation on `fda36d6c` and cannot run the benchmark. **Every engine fix lands on a branch created from `develop-reboot`** (section 6.1).
3. **A fix is not done until a test fails without it, and tests assert order** (1.3, 2.4).
4. **Nothing here invents a constant.** An unbound original value leaves the engine unchanged and goes to the RE backlog (section 7).

**Reading order.** Section 1: principles, classification, key index. Section 2: measurement machinery. Sections 3, 4, 5: Groups A, B, C. Section 6: landing order, gates, collision plan. Section 7: RE backlog. Section 8: documents and tests that are wrong today. Appendix A: the critical, high and medium findings, each classified once (low-impact ones are in section 5.2).

Evidence tags: [EXE] binary (physical file offset or validation ID); [ENGINE] code (`HEAD:` = this branch at `fda36d6c`; `LIVE:` = the 2026-10-08T00:53Z snapshot of the benchmark tree, now committed on `develop-reboot`); [DERIVED] arithmetic shown; [MEASURE] unknown, measurement named; [UNBOUND] original evidence absent, RE lead named. Superhuman numbers (button 5 = difficulty 4) throughout.

---

## 1. Principles, classification and the key index

### 1.1 Principles

| # | Principle | Consequence |
|---|---|---|
| P1 | **The EXE is the authority.** | Every constant in a fix cites an EXE offset or validation ID. An [UNBOUND] original value is not written; it goes to section 7. |
| P2 | **Upstream wins unless improved.** | Keep upstream names, data model and APIs (`Organisation::takeOver`, `GameEventType`, `BuildError`, `ConfigOption*`); add EXE fidelity only where demonstrably better (EXE evidence, a test upstream fails, a concrete bug) and record the divergence in the commit message. `BD09` adds a predicate; it does not change `isRelatedTo`. |
| P3 | **A fix is not done until a test fails without it.** | Proven by a control run (2.4), not by reading. |
| P4 | **Tests assert order, not presence.** | Where the original defines a sequence (pass order, first-match, visit order, event order) the test asserts the observed sequence; examples in each block. |
| P5 | **No behaviour the original lacks.** | Adjudicated non-fixes stay unfixed: `F-M11` persistence, `TD6` guard-gear bias, removing the starter assignment, a cap of 3 stations. |
| P6 | **Data fixes are fixes.** | Extractor and patch changes (`T8`, `RR-12`, `VC-2+VC-3+RR-14`, `TD8`, `SCORE-EQLEVEL`) change generated, untracked `base_gamestate`: regenerate, stamp, record the hash (2.6). |
| P7 | **One fix, one commit, one toggle; small fixes are verified by test and counter, not by the benchmark.** | Bundles only where the evidence says the pieces are one change; the benchmark sees only large effects (2.5). |

### 1.2 How findings were classified

The sign of the effect on a Superhuman player decides the group. **A0** already fixed in the committed `develop-reboot` lineage (choose the right base, no code); **A-now** engine harsher, inside the horizon the benchmark reaches (runs lose by day 12-45); **A-late** harsher but inert before the alien dimension or week 7; **B** engine softer; **C** neutral, mixed, bookkeeping, or a pair that must land together. The brief's "expect" lists were checked against the evidence and moved as follows.

- **`F15b` is B, not A**: accepting a rift offer turns a relation in (0, 25) into 100 (`organisation.cpp:1230`); the original gives `M + 25`. **`VC-3` joins `VC-2` and `RR-14` in C**: one adjudicated change; 390,000 man-hours in both rule sets, only the shape differs.
- **`SCORE-C2-TRIGGER` is split**: forced detection of the player's own crewed buildings and the unlimited first detection are harsher (`SCORE-C2/A`, A); the species gate and halving are mixed (`SCORE-C2/C`, C). 05 and 06 use one `SCORE-C2` key = `SCORE-C2/A`.
- **`UFO_GROWTH_STOP` and `VC-10` are one C pair**: the growth stop is two raids late (harsher) and the wave is absent (softer); either alone makes the sixth raid easier or harder than the original. **`TD17` is C**: more captured-alien score also raises the alien equipment tier. **`V-AG-01` is B**: six roll sets a day against one is easier for the player. **`VC-1` is A-late**: the engine cannot be won by the original's path and demands a kill-all under 4 times the reinforcement rate (`TD12`).
- **`BD04+TD10+TD8+TD9` is B on the net** (soldiers-first roster, uncapped turrets and stronger invented guns outweigh the missing armour; [MEASURE]). **`RR-09+RR-10+F-M12` stay one key** although `RR-10` is mixed; the caps are all easier.

Direction legend (Appendix A): **H** engine harsher; **S** softer; **M** mixed or neutral; **U** original unbound.

### 1.3 What "done" means

One commit touching only its block's files (stage by path, never `git add -A`); its option defaults to the original's behaviour once the gate passes and the legacy branch is reproducible with the option off (2.1); its test is red with the option off and green with it on (`WILL_FAIL` control, 2.4) and asserts order where the original defines one; an extractor change comes with regenerated `base_gamestate`, a test that fails on stale data and the gamestate hash in the manifest (2.6); its telemetry counters (2.7) exist.

### 1.4 Key index

Keys are the strings other files cite; headings use them verbatim. Atoms are the booleans of the `fidelity.fixes` map of `06-benchmark.md` 2.1 (option `OpenApoc.Fidelity.<Atom>`, name rule in 2.1); a bundle is atoms that land as one commit. Test = the CTest cases that must be red without the atom (2.4). Target tree is both HEAD and LIVE except: A0 is committed in `879d0d73`; `G4` and `G8` are handed to the owner of the funding rewrite; `F6` is held behind a measurement; `BD14b`, `F17`, `V-AG-14` have no code. "Collides" lists the files the 87-file LIVE diff (snapshot 00:53Z) also edited: they need a rebase on `khallmark/develop-reboot`; all other files of the fix land without conflict. RE = last part blocked on reverse engineering (section 7).

| Label | Key | Group | Atoms (aliases) | Collides (LIVE-edited) | RE | Test |
|---|---|---|---|---|---|---|
| A0.1 | `BD01b` | A0 | BD01a, BD01b (SCORE-C2-VALUE) | - | - | aliens_reaching_a_base_do_not_start_a_defence (879d0d73) |
| A0.2 | `G2/D1-D7` | A0 | G2 (G1, G3, G9, SCORE-C7, SCORE-FUNDING-CONSUMES) | - | - | cutoff_uses_the_previous_weeks, cutoff_boundaries (879d0d73) |
| A1 | `G4` | A-now | G4 | gamestate.cpp | - | new_game_funding |
| A2 | `T1+T2` | A-now | T1, T2 (F16b) | gamestate.cpp, gamestate_serialize.xml | - | takeover_roll, takeover_cadence_order |
| A3 | `F02+F05` | A-now | F02, F05, F01, T13 (F06) | scenery.cpp, vehicle.cpp | - | scenery_hit_intent, vehicle_downed_relation_adjustment (rewritten), ufo_crash_threshold |
| A4 | `F11b` | A-now | F11b (F12) | battleunit.cpp | - | kill_victim_not_own_ally |
| A5 | `SCORE-C6` | A-now | SCORE-C6 | vehicle.cpp | - | falling_vehicle_is_not_charged |
| A6 | `SCORE-C2/A` | A-now | SCORE-C2A (SCORE-C2-TRIGGER, BD07, F11_building_detection) | building.cpp, alertscreen.cpp | - | detection_owner_bypass, detection_first_window |
| A7 | `SCORE-C1` | A-now | SCORE-C1 | cityview.cpp | - | research_score_credited_in_update |
| A8 | `BD03` | A-now | BD03 | vehiclemission.cpp | - | ufo_attackers |
| A9 | `BD06` | A-now | BD06 (F12_alien_movement) | building.cpp | - | alien_spread_gate |
| A10 | `BD14a+BD16` | A-now | BD14a, BD16 (BD17 kept) | battle.cpp | - | base_die_rehomes_with_second_base, base_die_last_base_ends |
| AL1 | `TD12` | A-late | TD12 (TD13) | battle.cpp | TD14 | reinforcement_one_per_expiry |
| AL2 | `VC-1` | A-late | VC-1 | battle.cpp | - | raid_success_by_objective |
| AL3 | `T4/F14` | A-late | F14 (T4, F21, VC-11) | battle.cpp | - | food_chamber_frees_mutant_alliance |
| AL4 | `BD02` | A-late | BD02 | gamestate.cpp | - | overspawn_no_defence |
| B1 | `TD1+TD2+TD3` | B | TD1, TD2, TD3 | battleunit.cpp, battle.cpp | - | difficulty_scaling_table |
| B2 | `BD04+TD10+TD8+TD9` | B | BD04, TD10, TD8, TD9 (BD18) | - | TD9, TD11, BD20, TD20 | roster_by_presence, budget_with_stations, class_dilution, turret_record |
| B3 | `BD08` | B | BD08, BD10, BD11 | - | P(base) | raid_reaches_defend_the_base_without_save_load, incident_histogram |
| B4 | `G8` | B | G8 (VC-14) | gamestate.cpp | - | bankruptcy |
| B5 | `RR-09+RR-10+F-M12` | B | RR-09, RR-10, F-M12 (V-AG-07, RR-11) | gamestate.cpp | - | pool_rule, caps_include_employees, lab_cap_6_small_4_large |
| B6 | `F-M1+F-M2+F-M3` | B | F-M1, F-M2, F-M3 (F-M4, G12) | gamestate.cpp | - | stock_law, price_pass_order, selector |
| B7 | `F-M7` | B | F-M7 | - | - | craft_sale_value |
| B8 | `T10` | B | T10 | - | - | taken_over_guard_tier |
| B9 | `T8` | B | T8 (T16) | - | - | initial_aliens_range |
| B10 | `RR-12+RR-13` | B | RR-12, RR-13 | cityview.cpp, vehicle.cpp, battle.cpp | won on retreat | ufo_recovery_unlocks |
| B11 | `F9` | B | F9 | gamestate.cpp | - | escort_gear_gate |
| B12 | `F6` | B | F6 | gamestate.cpp | double drain | first_incursion_window |
| B13 | `F15b` | B | F15b (F15a match) | - | - | sign_treaty (extended) |
| B14 | `V-AG-01` | B | V-AG-01, V-AG-02, V-AG-04 (V-AG-03, V-AG-05) | - | - | training_one_set_per_midnight |
| B15 | `BD13` | B | BD13 | building.cpp | baseline data | base_collapse_90_percent |
| B16 | `BD14b` | B | BD14b | - | worth | - |
| C1 | `VC-2+VC-3+RR-14` | C | VC-2, VC-3, RR-14 | test_city_rules.cpp | - | alien_building_chain_order |
| C2 | `UFO_GROWTH_STOP+VC-10` | C | UFO_GROWTH_STOP, VC-10 (RR-15, VC-9, F4, F5) | gamestate.cpp, battle.cpp | targets, stock | growth_stop_control_chamber, mass_assault_gate |
| C3 | `BD09` | C | BD09 (F13-thresholds, F-M15, G13) | building.cpp, gamestate.cpp | - | hostile_inclusive_threshold |
| C4 | `F11` | C | F11 | battleunit.cpp | S byte | per_kill_constants |
| C5 | `SCORE-C2/C` | C | SCORE-C2C (T12) | building.cpp | - | detection_formula_worked_cases |
| C6 | `F7` | C | F7 (F8) | gamestate.cpp, vehiclemission.cpp | - | withdrawal_band_per_hull |
| C7 | `TD17+SCORE-C0` | C | TD17, SCORE-C0, TD18 | battle.cpp, battleunit.cpp | item table | tactical_leadership_clamp |
| C8 | `F17` | C | F17 | - | first term | - |
| CD | `V-AG-14` | C | V-AG-14 | - | features | - |

The Atoms column is the authoritative list for `FIDELITY_FIXES` (the lint of `06-benchmark.md` 2.1 reads it). Mapping from the atom names of 05 and 06: `SCORE-C2` = `SCORE-C2A`; `T12` is the halving fix inside `SCORE-C2C`; `F5` = `VC-10`; `F16b` = `T1`+`T2`; `VC-9` = `RR-15` = `F4` = `UFO_GROWTH_STOP`; `VC-14` = `G8`; `V-AG-07` = `RR-10`; `BD04+TD10+TD8` (facts sheet) = `BD04+TD10+TD8+TD9` (brief); `F06` follows from `F05` and `F01` (not an atom). Closed, not in the RE backlog: `RR-11` (closed by `F-M12`), `T16` (closed by `T8`), `F17` (bound at `FUN_00097234`; only the first term of the price is open).

### 1.5 The two trees and how lines are cited

| Item | Value |
|---|---|
| HEAD | `khallmark/strategy-engine-plan`, `fda36d6c` plus two documentation commits; no harness. |
| LIVE snapshot | 2026-10-08T00:53Z copy of the benchmark tree: HEAD plus 87 modified files. All `LIVE:` lines are from it. |
| LIVE now | 07:39Z: `develop-reboot` at `bce89743`, no engine or harness source uncommitted. 16 files changed since the snapshot, among them `gamestate.cpp` (+28 lines after line 190, +35 after about 1393), `vehicle.cpp`, `vehiclemission.cpp` (about 520 changed lines), `gamestate_serialize.xml`, `alertscreen.cpp`. **Re-resolve every `LIVE:gamestate.cpp:N` for N > 190 by symbol**: `updateEndOfWeek(true)` `LIVE:714` is now 742; the `craftFactoryIntact` gate `LIVE:1626` now 1661; `previousWeeks < -2400` `LIVE:1793` now 1828. |
| Build identity | The manifest of `06-benchmark.md` 2.1 records `engine_sha256`, `gamestate_sha256` and the fidelity vector. |

---

## 2. Fidelity versioning and measurement

**Summary.** Each fix atom lands behind its own runtime option, so the control arm is the same binary with the option off. The record that carries the fix set into `final.json` is defined by `06-benchmark.md` 2.1; this file owns what feeds it: the atom list, options, requirement relation, vector order, tests and counters. The benchmark sees only large effects; every other fix is proven by its test and a named counter.

### 2.1 One toggle per atom

| Item | Rule |
|---|---|
| Atom | The unit of the `fidelity.fixes` map of `06-benchmark.md`: one register ID (or alias cluster) that can be switched on its own. A bundle (section 1.4) is atoms that land as one commit; its sub-steps are separate commits behind their atoms' options. |
| Option | `ConfigOptionBool("OpenApoc.Fidelity", "<Atom>", ...)` in `framework/options.cpp`, in one delimited block next to `RngSeed`, listed in `dumpOption`; atom name = ID with `-` and `/` deleted and `+` as `_`. The file is LIVE-touched: the block is its own commit. `game/state/fidelity.cpp` (new, `06-benchmark.md` 8.2) holds `FIDELITY_FIXES` and the readout of `gs fidelity`. |
| Read | Cached once per game in a non-serialised `GameState::fidelity` struct filled in `initState()`, as `skipTurboCalculations` is at `LIVE:gamestate.cpp:325`; static functions without a `GameState` take the flag as a parameter. |
| Set | On the command line, as the harness already does with `--Config.Save=0 --Config.Read=0 --OpenApoc.NewFeature.SeedRng=0` (`tools/oa_play.py:6`); never with `Options::*.set()` (it rewrites `settings.conf` on exit). |
| Default | OFF while under test; ON once the gate passes (section 6). The legacy branch is deleted after two benchmark generations default-on and a clean A/A; the `WILL_FAIL` control becomes a golden test. |
| Requires | `F-M2` requires `F-M3`; `UFO_GROWTH_STOP` requires `VC-10`; `F14` requires `VC-1`; `VC-3` and `RR-14` require `VC-2`; `TD8` requires `TD1`; `F06` follows `F05` and `F01`. The lint of `06-benchmark.md` 2.1 rejects a vector that breaks one. |

### 2.2 Vectors and shims

`06-benchmark.md` 2.3 defines F0 (the LIVE snapshot vector: BD01a/b and the funding rewrite carried, every other atom false, `G4` regressed). Each step of section 6 that turns atoms on creates the next vector (F1 = F0 + `G4`, F2 = F1 + `T1`, `T2`, ...) in `bench/vectors.json`. A shim of `06-benchmark.md` 2.1 switches itself off when its atom is true; blocks name the shim they retire.

### 2.3 Three kinds of arm

A **flag arm** (C++ behaviour: same binary and data, option 0 against 1); a **data arm** (extractor or patch data: `T8`, the `RR-12` extractor half, the `VC-2` chain, `TD8`/`TD9` data, `BD13` baseline, `VC-1` objective sets, `SCORE-EQLEVEL`: same binary, two `base_gamestate` trees, `gamestate_sha256`); **no arm** (bookkeeping invisible in a run: unit test and counter only). Seeds follow the learner (distinct, non-zero: `LIVE:tools/oa_learn.py` `seed_for`); `06-benchmark.md` 1.6 found seed pairing gives bookkeeping, not variance reduction; an A/A precedes the first A/B on any new vector (its 3.5).

### 2.4 Red/green proof and test shape

For each atom's test `X`, CTest registers `X` (option 1, must pass) and `X_control` (option 0, `set_tests_properties(X_control PROPERTIES WILL_FAIL TRUE)`): if the control passes, the test cannot tell the fix from the old code and the build fails. Data fixes run the control against the pre-fix data. `check_fidelity_keys.py` (`06-benchmark.md` 2.1) requires every atom of the index to have a test named for it; the names are in the key index. `tests/CMakeLists.txt` is LIVE-touched: entries go in one fenced block at the end, committed last.

| Shape rule (P4) | Example |
|---|---|
| Assert the sequence the original defines | `T2`: orgs tested in 300 minutes are 2, 3, ..., 26, then 35 idle ticks, then 2 again. |
| Assert pass order across a table | `F-M2`: every row's stock pass completes before any price pass; price reads post-decay stock. |
| Assert the order of side effects | `BD14a`: re-homing before the kill loops; `G4`: `City::initCity` before the first funding pass; `G8`: bankruptcy after overheads, not at game start; `F9`: equip after `enterDimensionGate`. |
| Assert a chain, not its members | `VC-2`: unlocks visit functions 4, 2, 6, 8, 0, 7, 1, 5, 3, 9, each only by winning the previous. |
| Assert once, not at least once | `F02`: a destroyed intentional tile is one `adjustRelationTo` of -25; the integer ripple of -25 differs from the ripples of -5 and -20 summed. |

### 2.5 What the benchmark can detect

`06-benchmark.md` 1.2, 1.6, 3.6: reward noise is large (SD 800-880 on current runs), seed pairing does not reduce variance, and endpoints are game-day milestone indicators judged by a group-sequential design with looks at 16, 32, 48, 64 runs per arm (the F0 baseline of `healthy@21` is about 0.10). Benchmark-sized fixes (survival endpoint plus the mechanism counter; budget up to 64 runs per arm): `G4`, `T1+T2`, `BD14a+BD16` (two-base arms), `BD08` with `BD04+TD10+TD8+TD9`, `TD1+TD2+TD3`. All others (including `F02+F05`: 8 of 50 relation messages were slow slides) get the red/green test plus the named counter and no promised delta.

### 2.6 Data provenance

`data/mods/base/base_gamestate` is generated by `extract-data`, untracked, and differs by branch; the default build target overwrites it (a green gate went red on seven `test_city_rules` cases when master's extractor produced it). For every data fix: CI runs `extract-data` in a clean tree first; the extractor writes a stamp (hash of `tools/extractors/` plus of the output) that data-dependent tests assert, so stale data is red, not green; the manifest records `gamestate_sha256`; the commit lists the regeneration command.

### 2.7 Telemetry

`06-benchmark.md` 8.2 owns `CampaignStats` and the `tele_*` queries and names `damageByFirer{xcom, alien, vehicleFall}`, `incidentsByOwner{xcom, other}`, `takeovers{org, tick}`, `baseDefences{trigger, outcome}`, `basesDiedSilently`, `growthStoppedAtRaid`, sales, wages, upkeep. This plan adds the counters its fixes need (each closes a [MEASURE] item; until then the harness derives the facts from snapshot differences):

| Counter | Serves | Closes |
|---|---|---|
| `takeoverTests[slot]` | `T2` | Cadence order; Government survival with `takeovers` |
| `relationDelta[org][source]` (`intentionalHit`, `strayHit`, `craftDowned`, `kill`, `rift`, `bribe`, `ripple`) | `F02+F05`, `F11b`, `F11` | Share of Government loss from stray fire |
| `baseDefences` extended with `attackers`, `defendersPresent`, `turrets` | `BD03`, `BD08`, `BD14a+BD16`, `BD04+TD10+TD8+TD9`, `BD02` | Frequency and outcome; P(base given Raid/Storm) |
| `incursionsLaunched[week][role]` | `F6` | 3.5 against 7 a week |
| `alienSpread{attempts, totalMovedLt3, moves}` | `BD06` | Effect of the spread gate |
| `marketCuts{item, week, postDecayStock, price}` | `F-M1+F-M2+F-M3`, `F-M7` | Pacing rule (0.075 x max a week) |
| `lootSpots{owner, building, count}`, `labRate{lab, staff, manHoursPerHour}`, `hirePool{class, arrivals, size}` | Cult pillar; `RR-09+RR-10+F-M12` | Loot spots per Temple raid; staffed workshop rate; 1.5 arrivals |
| `trainingRollSets`, `ufoWithdrawals{type, band}` | `V-AG-01`, `F7` | Midnight cadence; hulls that break off |

---

## 3. Group A - removes artificial hardship (engine harsher than the original)

**Summary.** Sixteen fixes. Two (A0) need only the right base branch. Ten (A-now) act inside the horizon the benchmark reaches (runs lose by day 12-45) and land first, one at a time, each with its own control arm. Four (A-late) cannot move today's benchmark and ride with the endgame bundle. Order in A-now: the economy regime (`G4`; arms with different tick-0 income are not comparable), the takeover clock (`T1+T2`), the relation and score sinks, then the base-defence group.

Fields: Verdict (IDs, final verdict, review), Original [EXE] (summarised from `resolved-facts.md`, section given), Engine today (HEAD and LIVE lines), Target (tree; files also edited in LIVE are marked * in the key index), Change (sketch), Test (red before, green after), Strategy (rules changed), Effect (expected benchmark effect, only [DERIVED], [MEASURE] or counted), Order, and Blocked on RE where set. Every fix is written against `khallmark/develop-reboot`; `LIVE:` lines are the snapshot's (section 1.5). Section numbers of 01, 02 and 03 are cited as of 2026-10-08, as `06-benchmark.md` does.

### A0. Already fixed in the committed `develop-reboot` lineage

#### A0.1 `BD01b` (with `BD01a`, `SCORE-C2-VALUE`)

- **Verdict.** `BD01b` critical, engine_bug, survived (HEAD only); `BD01a` critical, match (the LIVE hunk); `SCORE-C2-VALUE` medium, match.
- **Original [EXE].** A base defence starts only at `FUN_000ac348` reason 5, via `FUN_000ac08c` (file `0x10E730`, 7 call sites, reason 5 at VA `0x3AE76`, `0x3B079`, `0x99E2D`): an Infiltration UFO landing, a Subversion landing on an exposed base, a hostile organisation's Raid or Storm. Movement (`FUN_0006f7f8`) and detection (`FUN_0006f964`) never call it (`resolved-facts.md` 6.1).
- **Engine today.** HEAD `building.cpp:815-826` (`detect` calls `base->die` or raises `DefendTheBase`) and `:1022-1036` (movement does the same); 13 of 15 logged benchmark defences were movement-triggered (`failure_forensics.md` section 9). LIVE snapshot `:835-844, 1029-1040`: detection raises `AlienSpotted`, movement only rolls exposure.
- **Target.** Take the committed hunk: base on `develop-reboot` (`879d0d73`). No new code.
- **Test.** `879d0d73` adds `aliens_reaching_a_base_do_not_start_a_defence` (`test_city_rules.cpp`): aliens move into an unmanned base building and a **forced** `detect(state, true)` runs; it asserts the base still exists. Strengthen it to assert order and the absence of the event: movement, then non-forced detection, then zero `DefendTheBase` events, the base alive, exactly one `AlienSpotted`. Red on HEAD's `building.cpp`.
- **Strategy.** `03` 7.1, 7.6: drops "garrison against aliens walking in": a garrison is needed against UFO landings and hostile-organisation Raid/Storm only; WHEN an infestation is within about 14 tiles of a base THEN clear it before 3 aliens can step in at once (exposure matters only for Subversion targeting).
- **Effect.** None on the benchmark (it runs the LIVE code); any HEAD-based run is invalid without it (82 `BaseDefenseScreen` missions, mostly 1-2 aliens against unarmed staff).

#### A0.2 `G2/D1-D7` (with `G1`, `G3`, `G9`, `SCORE-C7`, `SCORE-FUNDING-CONSUMES`)

- **Verdict.** All match on LIVE (0 mismatches in 393,120 differential cases); `G2` adjudicated; HEAD diverges (222,714 cases).
- **Original [EXE].** `FUN_000941dc` (file `0xF6880`), Monday midnight: credit the old income; cut if `M[Government][X-COM]` < -50 (strict byte) or `H` < -2400 with `H` = completed previous weeks only; on a cut undo the credit, income := 0, latch; else tier delta on the old income from week score `W`; next income = min(F + delta, trunc(G/2)) **stored**; Government debited it; then overheads; bankruptcy check last (`resolved-facts.md` 3.1-3.2).
- **Engine today.** HEAD: D1 lifetime total incl. the current week; D2 uncapped stored income; D3 symmetric transfer; D4 no 2e9 guard; D5 seven categories; D6 upkeep on unfinished facilities; D7 the funding screen re-tests (`gamestate.cpp:1737-1763`, `weeklyfundingscreen.cpp:33-121`).
- **Target.** The funding hunk of `879d0d73` (not the whole commit, per the `G2` ruling); present on `develop-reboot`.
- **Test.** `tests/test_weekly_funding.cpp` (new in `879d0d73`); add the order: credit, cutoff on `H` only, delta, cap and store, Government debit, overheads. Cases: -3000 then +5000 cuts at Monday 2; five weeks of -500 cut at Monday 6; +3000 then -5000 leaves `H` = -2000 at Monday 3, no cut.
- **Strategy.** `01` 5.1: WHEN a Monday approaches THEN completed-week score must be at least -2400 and Government to X-COM at least -50 (the org screen shows "Hostile" at -50, one point before the cut at -51).
- **Effect.** None on LIVE; HEAD can cut a week early or fail to cut after a late recovery.

### A-now. Inside the benchmark horizon

#### A1. `G4` - new-game funding pass

- **Verdict.** `G4-newgame-funding-pass` critical, engine_bug, survived. A regression of an old ordering defect paired with the `879d0d73` funding rewrite: hand to the owner of that rewrite.
- **Original [EXE].** `FUN_000b94c0` runs `FUN_000941dc` twice in quiet mode (no report, no balance commit) and then overwrites X-COM: balance := 140,000 - 10,000 d, income := 92,000 - 3,000 d (file `0x11BC47`, `0x11BC5D`): **100,000 and 80,000** at Superhuman. No salary or upkeep at new game; the first Monday nets +57,100 (`resolved-facts.md` 3.3).
- **Engine today.** `startGame()` calls `updateEndOfWeek(true)` (`LIVE:gamestate.cpp:714`, now 742; HEAD `:713`), running `updateOrgFinances()` and `weeklyPlayerUpdate()` **before** `initState()` runs `City::initCity` (`city.cpp:157`), the only filler of `Organisation::buildings`. At day 0 the Government has no income or tax (balance 20,000, `G/2` = 10,000); LIVE stores the capped income (a permanent 10,000) after crediting the old 80,000: 180,000 / 10,000; HEAD caps only the payment: 110,000 / 80,000. Seen in 131 of 131 LIVE and 32 of 32 HEAD campaigns.
- **Target.** LIVE lineage (`gamestate.cpp`, LIVE-touched and churned). HEAD is moot.
- **Change.** In `startGame()` skip `updateOrgFinances()` and `weeklyPlayerUpdate()` inside `updateEndOfWeek(true)` (keep `updateUfoGrowth()`, `updateItemMarket()`); `initState()`'s two `updateOrgFinances()` calls (`LIVE:315-316`) remain as the warm-up; leave X-COM at the difficulty patch's 100,000 / 80,000. Stopgap: save and restore both around the call.
- **Test.** `test_weekly_funding.cpp`, new `new_game_funding`: order (no funding pass before `City::initCity` has filled `Organisation::buildings`); tick 0 balance 100,000, income 80,000; after the first Monday balance 157,100 [DERIVED 100,000 + 80,000 - 22,900: the old income is credited whatever W is] and, with `weekScore` zeroed just before the rollover, income 80,000 (otherwise income = F + tier(W): the forced first detection bills -30 in days 1-3 and any negative W cuts F/15). Control sees 180,000 / 10,000.
- **Strategy.** `01` 0.3, 1.4: the opening tables collapse to the 100,000 / 80,000 case; "plan from the first weekly screen" becomes the `start_fingerprint` provenance assertion.
- **Effect.** Weekly net -12,900 to +57,100 [DERIVED +70,000]; opening cash 180,000 to 100,000 once. The benchmark economy runs at one eighth of Superhuman income today. Benchmark-sized (income at Monday 1 is exact; cash starvation is rank 5 in `failure_forensics.md`).
- **Order.** First. Before `G8`.

#### A2. `T1+T2` - takeover roll and cadence

- **Verdict.** `T1` critical, `T2` high, diverges, survived; `F16b` is the same finding; `T3`, `T5`, `F16a` (effects, Government loss ends funding) match and inherit it.
- **Original [EXE].** `FUN_0006fd9c` (file `0xD2440`), every 5 game minutes, tests **one** organisation: slot counter 2..61, only slots 2..26 act (Government first, Technocrats last), so each org is tested once per 300 minutes; takeover iff `rand16(0..100) < v - 100` for raw `v` 0..200 (UI shows `v/2`): probability `(v-100)/101`, exactly 0 for `v` <= 100; human city only; no difficulty term (`resolved-facts.md` 2.1).
- **Engine today.** `organisation.cpp:84-88`: P = v/200 per attempt for any v > 0; `TICKS_PER_TAKEOVER_ATTEMPT` = 125 minutes per org with a random phase (`organisation.h:19`; `LIVE:gamestate.cpp:611-612`), driven from `updateEndOfFiveMinutes` (`LIVE:1456-1470`), in every city. [DERIVED, `v_takeover.md` 6] median hours to flip the Government: +2 raw/h (10 Anthropods in the Senate) 19 h against about 87 h; +1/h 34 against 236; +7/h (a 27-alien deposit) 9 against 25. Engine per-day probability 16% at v = 3, 45% at 10, 70% at 20; the original 0 below the 50% mark.
- **Target.** Both. `organisation.cpp/.h` clean; `gamestate.cpp`, `gamestate_serialize.xml` LIVE-touched and churned.
- **Change.** (1) `organisation.cpp:84-88`: `if (!forced && randBoundsInclusive(state.rng, 0, 100) >= infiltrationValue - 100) return;`. (2) Delete `TICKS_PER_TAKEOVER_ATTEMPT`, the accumulator and the random phase. (3) Serialised `GameState::takeoverSlot` (init 2; old saves default 2); in `updateEndOfFiveMinutes`, only in the human city: `slot = takeoverSlot; takeoverSlot = (slot + 1 >= 62) ? 2 : slot + 1; if (slot < 27)` take over the org with `exeOrgIndex == slot` (`organisation.h:156`) unless taken. (4) Counters `takeoverTests`, `takeovers`.
- **Test.** New `tests/test_takeover.cpp`. `takeover_roll`: v in {0, 1, 50, 100} give exactly zero takeovers in 20,000 trials; v = 150 gives 49.5% within a 4-sigma band; 200 gives 99.0%; `forced` unchanged. `takeover_cadence_order`: with every acting org at v = 100 (none can flip) and `takeoverTests[slot]` incremented on every slot visit before the taken-over and roll checks, 120 five-minute ticks show orgs 2, 3, ..., 26 one per tick, then 35 idle ticks, then 2 again; X-COM, Aliens, Civilian never; nothing in the alien city. The hourly tests in `test_organisation.cpp` (:129, :150, :196) stay; no test calls `takeOver`, which hid this.
- **Strategy.** `03` 2, 3.4: WHEN displayed infiltration is at most 50% THEN no roll is possible; above it, flip-eligible every 300 minutes: clear crews now; a flip is permanent except the Mutant Alliance (`T4/F14`). The "any non-zero Government infestation is imminent" stance belongs to the engine without the flag.
- **Effect.** Rank-1 killer: Government takeover was the first irreversible event in 50 (+3) of 162 runs, in 73% of runs reaching day 15 (`failure_forensics.md` 2); day 7-20 takeovers are 3-7 times too early [DERIVED]. Benchmark-sized (day of takeover and of cut). [MEASURE] survival curve after the fix.
- **Order.** Second.

#### A3. `F02+F05` - relation bookkeeping: stray fire, downed craft, ripple

- **Verdict.** `F02` critical, `F05` high (adjudicated), `F06` high (falls out of F05 and F01), `F01` low; diverges.
- **Original [EXE].** Primitive `FUN_0005faf0`: integer ripple `f = M[k][A]/2` (halved again if negative), `M[k][B] += trunc(f x delta/100)`, human city, not X-COM or Civilian. Building hit: **intentional** (ordered attack or manual fire) surviving -5, destroyed -25 in one call; **stray** 0 / -5. Non-UFO craft destroyed outright: victim -10 once (+2 on the firer's view if not X-COM), no hostility test. UFO downed (below its crash word, once): Aliens -20; finishing hit -10 (`resolved-facts.md` 4.1).
- **Engine today.** `scenery.cpp` LIVE `:1197-1226` `handleCollision`: every hit takes -5 (`:1215`), a destroying hit another -20 (`applyDamage` `:1268`); `intentional` only gates `underAttack`. `vehicle.cpp` LIVE `:2115-2140` `adjustRelationshipOnDowned`: -5 if already hostile else -30, float ripple. Despite calls from `die`, `crash` and `startFalling`, a double charge is unreachable with default data; the optional `crashing_vehicles` mod can make it reachable (F05 adjudication). [DERIVED] distance to the funding cutoff from Government 65 (needs -116): original about 5 intentional destroyed tiles, 24 intentional hits, 24 stray destroyed tiles or about 12 Government craft; engine 5 destroyed tiles of any intent, 24 hits, 4 craft.
- **Target.** Both; `scenery.cpp`, `vehicle.cpp` LIVE-touched; `organisation.cpp` clean.
- **Change.** Three commits, one option. (a) `Scenery::handleCollision`: compute `intentional` (manual fire or the firer's mission front is `AttackBuilding` on this building) first; own org: nothing; intentional: one `adjustRelationTo(destroyed ? -25 : -5, ripple)`; stray: `destroyed ? -5 : 0`; delete the x4 call in `applyDamage`; human city only. (b) `adjustRelationshipOnDowned`: drop the -5 / -30 split; outright kill (victim not the attacker's, not the player's): victim to attacker -10 with ripple, attacker to victim +2 if the attacker is not X-COM; `crash()` (UFO victims) on the first crossing strictly below `crash_health` (original `<`, engine `<=`): -20 once; `startFalling()` nothing; skip when the victim is the player's; keep score booking behind its own once-per-vehicle flag. (c) Integer-truncating ripple in `adjustRelationTo`, excluding X-COM and Civilian; last, it rewrites test expectations.
- **Test.** `tests/test_org_relations.cpp`. **Rewrite** `vehicle_downed_relation_adjustment` (:353-440): it pins -30, float ripples -11.1 / -7.5 / +7.5 and "-30 not -45". After (b), (c) a Government craft killed by Megapol gives victim to attacker -10 and third-party ripples at relation 74, 50, 0, -100 to the victim of -3, -2, 0, +2 [DERIVED: 74/2 = 37, x -10/100 truncates to -3; 25 gives -2; -100/2 = -50, halved -25, gives +2; check against `FUN_0005faf0` before pinning]. New `scenery_hit_intent` (four cases; a destroyed intentional tile is **one** call of -25, since the ripple of -25 differs from the sum for -5 and -20) and `ufo_crash_threshold` (one -20 on crossing strictly below crash health, -10 on the finishing hit, no second crossing). Control red.
- **Strategy.** `01` 5.2, `03` 4.3: stray fire costs 0 / -5 (surviving / destroyed tile), not -5 / -25; a Government craft -10, not -30 (retires shims `no_stray_fire`, `no_gov_craft_fire`). Relation budget from 65: about (relation + 50)/5 stray destroyed tiles. The harness's `intercept_min_relation` is an engine-profile parameter until this lands.
- **Effect.** Losses 2-5 times smaller on the same actions [DERIVED]; each UFO downed worth about +2 Government relation against about +0.6. Not benchmark-sized: 8 of the 50 relation messages were slow slides, 42 were takeover steps (`failure_forensics.md` 3). Evidence: test plus `relationDelta[org][source]`; [MEASURE] stray-fire share.
- **Order.** After `T1+T2`; (c) last; `F11b` rides with it.

#### A4. `F11b` - the victim organisation is not its own ally (with `F12`)

- **Verdict.** `F11b` medium, engine_bug, survived; `F12` low.
- **Original [EXE].** TACP `FUN_00060720`: same organisation: stop; the victim org is excluded from the ally loop; victim's view of the killer `-3S` (not hostile) or `-S/2` (hostile), `S` = 10 for humans.
- **Engine today.** `battleunit.cpp:5075-5101`: the loop excludes only `attackerOrg` and Civilian; `getRelationTo(self)` is 100, so the victim is its own ally and takes a second -5: -35 (not hostile) or -10 (hostile).
- **Target.** Both; `battleunit.cpp` (4 LIVE lines).
- **Change.** `org.first != ourOrg.id` at `:5090`; skip the block when `attackerOrg == ourOrg` or no attacker.
- **Test.** `test_org_relations.cpp` `kill_victim_not_own_ally`: Government at 60 ends at 30, not 25; at -60 ends at -65, not -70; X-COM killing X-COM changes nothing (order: direct penalty once, then the ally loop).
- **Strategy / effect.** A kill of a Government, Megapol or other-org unit costs 30, not 35. Unit-test sized; rides with A3.

#### A5. `SCORE-C6` - falling vehicles are not charged Damage to City

- **Verdict.** `SCORE-C6` medium, diverges, adjudicated (the validator's "match" split: projectile writer matches, vehicle writer does not). `F07`: no fix.
- **Original [EXE].** Two category-6 writers. The projectile writer `FUN_00054a28` (call at file `0xB8AFC`) matches. The vehicle writer `FUN_00041644` (call at file `0xA423D`) has two callers (VA `0x397A7`, `0x3A3A1`), both behind `cmp word [esi+2],2`; slot `+2` is the movement type: 1 for types 0-24 (all flying craft, X-COM and UFO), 0 for ground, 2 only for the Overspawn (type 33). A falling or crashing craft never charges.
- **Engine today.** `VehicleMover::updateFalling`, `LIVE:vehicle.cpp:1027-1030` (HEAD `:1002-1005`, added by `fda36d6c`): `chargeCityDamage(type->value)` when a plow-through succeeds on building scenery; any owner, any class.
- **Target.** Both; `vehicle.cpp` LIVE-touched and churned.
- **Change.** Delete the block and its comment; keep `chargeCityDamage` for the projectile path. Correct the `fda36d6c` text and `city-damage-scoring.md` (section 8).
- **Test.** `test_city_damage_score.cpp` has one case and calls `chargeCityDamage` directly (:43). Add `falling_vehicle_is_not_charged`: an X-COM craft and a UFO fall through building scenery; the tile dies, `cityDamage` is unchanged. Control red.
- **Strategy.** `03` 4.3, 4.8: a crash over a building costs score nothing; a projectile hit on a human-city building tile costs 0-40 (about 9 per footprint hit, most often 20).
- **Effect.** Removes an engine-only part of the city-damage sink (46.9% of all negative points: -151,482 over 131 saves at day 8 or later); the share is unmeasured: `damageByFirer.vehicleFall`.
- **Order.** Any time in A-now.

#### A6. `SCORE-C2/A` - incident detection of the player's own buildings

- **Verdict.** `SCORE-C2-TRIGGER` medium, engine_bug, adjudicated (high refuted to medium): this is the harsher half; `BD07` (forced detection of own buildings) and `F11_building_detection_probability` are aliases (formula half: `SCORE-C2/C`, C5).
- **Original [EXE].** `FUN_0006f964` every 5 game minutes over 5 buildings round-robin (68/78/84/96/109 buildings at difficulty 0-4), skips buildings with an alert timer, stops after the first incident. **No owner bypass**: X-COM-owned buildings roll the same formula; X-COM Base and Temple of Sirius have weight 0 (file `0x155354`, indices 36-37) and never fire. The x100 on the crew sum applies only 12:00-16:00 of day 1. Incident: -30, alert timer 360 minutes (`resolved-facts.md` 11.2).
- **Engine today.** `building.cpp:158` (both trees) passes `state.firstDetection || owner == state.getPlayer()` as `forced`, skipping both rolls for any crewed X-COM-owned building: each 6 h timeout plus the attempt interval, about 7 h, raises `AlertScreen` and -30 (up to -120 a day per building); `firstDetection` has no time limit (`LIVE:gamestate.cpp:717`). The `AlertScreen` EXTERMINATE route turns any mission at a player base into a `BaseDefense` battle against the resident crew (`battlemap.cpp:176`; stale text `alertscreen.cpp:60-62,84`). 18 of 103 sampled saves had a crewed X-COM building; the adjudication puts the engine at about 1.46 times the original on X-COM-owned buildings.
- **Target.** Both; `building.cpp`, `alertscreen.cpp` LIVE-touched.
- **Change.** Remove `owner == state.getPlayer()` at `:158`; limit `firstDetection` to 12:00-16:00 of day 1 and apply the x100 inside it; fix the stale `AlertScreen` text and `threatens_base`; drop the EXTERMINATE-on-own-base route (a Base-function building has weight 0, so nothing starts it; whether aliens enter X-COM buildings at all is [UNBOUND], lead: `FUN_000240a4`, `FUN_000a12f4`).
- **Test.** New `tests/test_detection.cpp`: a crewed X-COM-owned non-base building over 20,000 attempts follows the formula, not one per 7 h; a Base-function building never fires in 10,000; order across the window: at 15:59 on day 1 a one-alien crew is detected with probability near 1, at 16:01 with the plain formula.
- **Strategy.** `03` 3.3 sensors: an infested X-COM base building no longer pops an alert every 7 hours; the harness must not use that as a sensor. WHEN an alert names a third-party building THEN it is infested (unchanged).
- **Effect.** Removes -30 about every 7 h per infested crewed X-COM building. Unit-test sized; `incidentsByOwner.xcom`.
- **Order.** With `BD06` (same file, one rebase).

#### A7. `SCORE-C1` - research score is credited in the simulation

- **Verdict.** `SCORE-C1` low, engine_bug, adjudicated (listed in A: the engine loses score the original keeps; tiny).
- **Original / engine.** The original adds `research_data.score` once per topic in the simulation. The engine credits it in the UI handler (`LIVE:cityview.cpp:4924-4925`, HEAD `:4849-4850`) and loses it when another stage is pushed in the same frame (about 0.6% of completions); the -30 for `AlienSpotted` is credited the same way (`:4847`).
- **Target / change.** Both. Add the score in `Lab::update` (`research.cpp`, clean) before `fw().pushEvent(...)`; delete the two UI lines and do the same for `alienIncidents`; keep the dialog in the UI.
- **Test.** `test_research.cpp` `research_score_credited_in_update`: a stage pushed in the same frame as a completion; credited exactly once. Control red.
- **Strategy / effect.** None; the staffing guard in `oa_play.py` (~2617-2622) can go. About 0.6% of completions regain score; test-sized. **Order.** Step 5 (6.3).

#### A8. `BD03` - who attacks in a UFO-origin base defence

- **Verdict.** `BD03` high, diverges, survived.
- **Original [EXE].** Attackers are the static per-UFO-type crew table (file `0x13E790`) minus Egg, Chrysalis and Queenspawn, at most 36 aliens and 60 units; resident aliens do not fight; nothing is deposited. Crew to attackers: Probe 4 to 4, Scout 13 to 10, Transporter 20 to 15, Fast Attack 10 to 10, Destroyer 22 to 17, Assault 27 to 22, Bomber 8 to 8, Escort 13 to 9, Battleship 20 to 15, Mothership 24 to 19 (`resolved-facts.md` 6.2).
- **Engine today.** `battlemap.cpp:182-190`: the attackers are the building's whole `current_crew`: the UFO crew deposited at `vehiclemission.cpp:2631-2634` including eggs, plus resident aliens; only the 36 cap.
- **Target.** Both; `battlemap.cpp` clean, `vehiclemission.cpp` LIVE-touched and heavily churned (rebase first).
- **Change.** Build the list from `v->type->crew_deposit` minus {egg, chrysalis, queenspawn}, cap 36 aliens / 60 total (shared with B2); do not deposit into the building.
- **Test.** New `tests/test_base_defence_force.cpp` `ufo_attackers`: the ten types in table order give 4, 10, 15, 10, 17, 22, 8, 9, 15, 19; residents do not join and stay; the building's crew is unchanged.
- **Strategy.** `03` 7.2, 7.6 garrison: size for 4-22 attackers (UFO) or 5-20 (organisation raid, `BD12` match), not 27 or more.
- **Effect.** Assault 27 to 22 (-19%), Escort 13 to 9, Mothership 24 to 19 [DERIVED]; `baseDefences{attackers}`.
- **Order.** With `BD14a+BD16`; before B2.

#### A9. `BD06` - alien movement between buildings

- **Verdict.** `BD06` high, diverges, survived (alias `F12_alien_movement`). Direction harsher with medium confidence: [MEASURE].
- **Original [EXE].** `FUN_0006f7f8`, hourly per building: `moved[k] = crew[k] x (movePercent[k] + r)/100` with **one shared** `r = rand16(30)`; only if the total moved is at least 3; then probability `15 + 3 x total`, plus 20 if the **source** owner is taken over or allied to the Aliens (>= 75), to one of the first 15 intact buildings in array order inside |dx| < 15, |dy| < 15. Infestations of 4 or fewer never spread (`resolved-facts.md` 2.2).
- **Engine today.** `building.cpp:970-1042`: per-species draw `rand[0..30]`; moves if the total is above 0; +20 if the **destination** owner is friendly or allied (>= 25); the 15 nearest intact buildings by Manhattan distance up to 15 (`building.h:23-26`, `rankNearbyIntact` :907-937).
- **Target / change.** Both; `building.cpp` LIVE-touched. Gate on total >= 3; one shared roll; bonus from the source; candidates = first 15 intact buildings in building order inside the window.
- **Test.** New `tests/test_alien_spread.cpp`: a crew with total below 3 never spreads in 100,000 hourly trials; probability at n = 3 is 24%; destination always in the first-15 set in array order; the +20 follows the source.
- **Strategy.** `03` 7.1: infestations of 4 or fewer never spread; clearing buildings within 14 tiles of a base is the exposure-avoidance radius.
- **Effect.** Infested buildings in the saves rise 1.5 (day 1-7), 10.9, 19.2, 28.4, 41.2 (`failure_forensics.md` 3); the share due to spread of small crews is [MEASURE] (`alienSpread`).
- **Order.** After `SCORE-C2/A`.

#### A10. `BD14a+BD16` - losing a base does not kill the people

- **Verdict.** `BD14a` high, diverges, adjudicated (retreat sub-case medium confidence); `BD16` medium, engine_missing, survived.
- **Original [EXE].** A base is lost when a defence ends without success (loss **or retreat**): `FUN_000b32ac` type 2 keeps it only if `[0x244084]` is 1, else `FUN_000b3114` removes it (game over at the last base). **No agent or craft is killed**: all employed agents and craft homed there are re-homed round-robin (`FUN_000b3270`) to the next active base (survivors of a retreat returning alive is INFERRED). A base with no live agent present is destroyed (`BD17`, match).
- **Engine today.** `Base::die` (`base.cpp:134-142`) calls `Agent::die` / `Vehicle::die` on everything in `currentAgents` / `currentVehicles`; away units keep a dangling home; it also collapses the building. The removal rule matches (`battle.cpp` HEAD `:3596-3613`, LIVE `:3603-3620`).
- **Target.** Both; `base.cpp` clean, `battle.cpp` LIVE-touched.
- **Change.** In the base-defence loss branch (or a battle-loss variant of `Base::die`): if another player base exists, re-home every living agent homed here (present or away) and every craft homed here to the next base round-robin, move present agents and docked craft into it, skip the kill loops; keep the cargo refund, `GameLost` at zero bases and the owner change. `building->collapse()` stays a separate question (no `FUN_000b3114` evidence).
- **Test.** Extend `test_base_die.cpp` (`base_die_with_occupants` checks only that `die` does not crash; keep it). `base_die_rehomes_with_second_base`: two bases, three agents and a craft present, two agents away; order: re-homing before any kill loop; afterwards all five agents and the craft alive, homed in the second base. `base_die_last_base_ends`: one base, `GameLost`, agents dead.
- **Strategy.** `01` 3.5, `03` 7.5, 7.9: a second base is real insurance (a loss costs the building and stores, not people); a single base is still game over; never retreat from a defence (a retreat loses the base in both rule sets; the engine-only part of shim `never_retreat_base` goes).
- **Effect.** Applies to campaigns with two or more bases: 23 of 35 two-base runs that ended with one base had no recorded defence (`failure_forensics.md` 2, rank 3). `baseDefences{outcome}`, `basesDiedSilently`.
- **Order.** After `BD03`. Benchmark-sized for two-base arms only.

### A-late. Harsher, but inert before the alien dimension or week 7

#### AL1. `TD12` - one reinforcement per timer expiry (with `TD13`)

- **Verdict.** `TD12` medium, diverges, survived; `TD13` low; `TD14` (intervals) [UNBOUND].
- **Original [EXE].** Alien-building maps only. TACP `FUN_000243c0` spawns exactly one unit per expiry and resets the timer whether or not a unit was made; type from the distinct species present among Anthropod, Skeletoid, Spitter, Popper; pad = an intact 2 x 2 ground block (208/209 over 210/211); scaled like any unit (`resolved-facts.md` 5.2).
- **Engine today.** `battle.cpp:1974-1999`, `:2333-2385`: four spawns per expiry (a FIXME marks it); type from `guard_types_reinforcements`; pads flagged 208..215; cap alive hostiles < 36.
- **Target / change.** Both; `battle.cpp` LIVE-touched. One spawn per expiry, timer reset regardless, type and pad as above; keep the fork's intervals (20, 18, 35, 10, 40, 20, 12, 10, 30, 5 s) until `TD14` is bound.
- **Test.** New `tests/test_battle_reinforcements.cpp`: N expiries give N units, not 4N; the timer resets with no free pad; the type is always a species present.
- **Strategy / effect.** `02` 8.3, `03` 6.4: a long raid faces one reinforcement per interval: finish the objective, not the clean-up. Pressure falls to a quarter [DERIVED]. Land with `VC-1`.

#### AL2. `VC-1` - raid success is the objective, not the body count

- **Verdict.** `VC-1` high, diverges, adjudicated (validator "match" refuted).
- **Original [EXE].** TACP `FUN_00022f90` sets the result flag when X-COM has live units and no opponents, then **clears** it if the map's objective tiles are not destroyed (`FUN_00025310`): killing every alien without destroying the objective does not succeed; destroying it and evacuating does. On success `FUN_000b32ac` credits C7, collapses the building, unlocks the next topic and, for function 9, shows the victory text (`resolved-facts.md` 8.1).
- **Engine today.** `GameWon` needs `Battle::playerWon` (a conscious unit and no conscious hostile): `battle.cpp` `:3573`, HEAD `:3652-3654`, LIVE `:3659-3661`; objective tiles never feed it. A Gate raid is won by killing every alien and cannot be won by destroying the generators and leaving. Objective sets differ for four buildings (`extractors.cpp:121-131`): Gate engine `{71,73..85}` against `{76,77,78,138}`; function 3 `{213..216,223..226}`; 5 `{138..158}`; 8 `{142,143,144}`.
- **Target.** Both; `battle.cpp` (LIVE adds the C7 block `:3578-3587`) and the extractor (data arm).
- **Change.** In the RaidAliens branch replace the `playerWon` gate by `success = buildingDisabled || tryDisableBuilding()` for unlocks, collapse, C7 and `victory = building->victory && success`; keep `playerWon` for loot and the lose path. All hostiles dead must not end the battle while the objective is open; retreat after the objective counts as success. Correct the four objective sets; move LIVE's C7 block behind the same gate.
- **Test.** New `tests/test_raid_success.cpp`: order: a Gate map with generators destroyed and all units evacuated gives `GameWon`, then C7, unlock, collapse; an all-aliens-dead map with generators intact gives none and the battle continues. Stamp.
- **Strategy.** `02` 8.4, `03` 6.4: WHEN raiding an alien building THEN destroy the objective tiles (the generators for the Gate) and evacuate; the squad needs explosives or heavy weapons (retires shim `alien_building_mode = kill_all`).
- **Effect.** Makes the Gate raid winnable by the original's path. **Order.** With `TD12`, `T4/F14`, `BD02`, `UFO_GROWTH_STOP+VC-10`.

#### AL3. `T4/F14` - a won Food Chamber raid frees the Mutant Alliance (with `F21`, `VC-11`)

- **Verdict.** `T4` medium, engine_missing, adjudicated (the validator's "no reversal path" is superseded); `F14_food_chamber_mutant_alliance_reset` medium.
- **Original [EXE].** A won Food Chamber raid (function 2) runs `FUN_000b32ac`: clears organisation 24's taken-over byte, floors `M[24][X-COM]` to 100 (mirrored), sets `M[24][Aliens]` to -100 (no ripple), infiltration[24] := 0; fires even if never taken over; retaken only once raw infiltration exceeds 100 again. Every other takeover is permanent (`resolved-facts.md` 2.1).
- **Engine today.** `takenOver` is never cleared (`organisation.cpp:84-108`).
- **Target / change.** Both; `battle.cpp` RaidAliens won branch (HEAD `~3571`, LIVE `~3578`, after the C7 hunk). When the function is the Food Chamber and the raid succeeded (the `VC-1` gate): Mutant Alliance `takenOver = false`, `militarized = militarizedFromType`, `infiltrationValue = 0`, relation 100 both ways with X-COM, `establishRelation(aliens, -100)` one way, no ripple.
- **Test.** In `test_takeover.cpp`: a won Food Chamber raid frees a taken-over Alliance (relation 100, flag clear, infiltration 0); a Government takeover stays permanent (control); no retake roll while raw infiltration is at most 100 (order: reset, then silence).
- **Strategy / effect.** `03` 8.6: the Food Chamber (2nd raid) reopens hybrid supply; bribes cannot repair any other takeover. Unreachable before the 2nd raid. **Order.** After `VC-1`, `T1+T2`.

#### AL4. `BD02` - Overspawn ships do not start base defences

- **Verdict.** `BD02` medium, diverges, adjudicated (validator "match" corrected).
- **Original [EXE].** Overspawn UFOs (role 5; 1 of 10 weekly slots in week 7, 2 in weeks 8-14, 3 from 15) spawn a type-0x21 vehicle and mark themselves arrived with no deposit (`resolved-facts.md` 6.1).
- **Engine today.** Overspawn goes through `infiltrateOrSubvertBuilding` (`gamestate.cpp` HEAD `:1068-1074`, `:1157-1161`; LIVE `:1069-1075`, `:1158-1162`) and can deposit crew, call `Base::die` or raise `DefendTheBase`.
- **Target / change.** Both; `gamestate.cpp` LIVE-touched. Stop calling `infiltrateOrSubvertBuilding` for the Overspawn primary (interim: land and idle; a faithful version needs `FUN_00070e6c`, `FUN_00070cc0`); delete the Overspawn branch of the attack list and always use `attackBuilding`.
- **Test.** `test_base_exposure.cpp`: an Overspawn incursion over an X-COM base yields no `DefendTheBase` and no deposited crew (order: event queue empty after the landing; building crew unchanged).
- **Strategy / effect.** `03` 7.1: Overspawn pressure from week 7 starts no base defence. Inert before day 43. **Order.** Endgame bundle (step 15).

---

## 4. Group B - adds missing difficulty (engine softer than the original)

**Summary.** Sixteen fixes that make the engine as hard as Superhuman really is, so a strategy that works on the engine also works on the original. They land after the AI work is measured on the fixed Group-A build (section 6), because they change every fight and every cash table. Order by benchmark effect (combat strength and base defence first, then the bankruptcy floor, research chain, caps, market, slow rules); `F6` waits for its measurement and `BD14b` for reverse engineering. Fields as in section 3.

#### B1. `TD1+TD2+TD3` - hostile units are scaled by difficulty

- **Verdict.** `TD2` critical (natural armour), `TD1` high (TU, health, stamina, reactions), `TD3` medium (accuracy, psi); engine_missing, survived.
- **Original [EXE].** Every non-X-COM unit is scaled at creation, `(base + rand(0..inc)) x (100 + kD)/100`, truncating (TACP `FUN_0001f830`, file `0x7A2D4`; UFO2P `FUN_000ad93c`, file `0x10FFE0`). Superhuman: TU, health, stamina, reactions x1.16; psi energy x1.08, psi attack x1.04 (cap 100); psi defence `(base + 2D) x 1.04` (cap 100); natural armour x1.64 per zone where `rec[+0x45]` is non-zero; raw accuracy byte x0.84 (raises shown accuracy under `100 - byte`). X-COM agents never; turrets, reinforcements, brood spawns are (`resolved-facts.md` 5.1).
- **Engine today.** None: no `difficulty` read in `game/state/battle`, `rules/battle`, `shared/agent*.cpp`; stats from `AgentGenerator::createAgent` (`agent.cpp:121-149`); armour read unscaled at `battleunit.cpp:1824`, applied `:1912-1913`; accuracy stored as `100 - byte` (`extract_agent_types.cpp:1007-1008`); Psimorph keeps psi energy 170.
- **Target.** Both; `agent.cpp`, `battlemap.cpp` clean; `battleunit.cpp` (4 LIVE lines), `battle.cpp` LIVE-touched.
- **Change.** A post-pass `AgentGenerator::scaleHostile(agent, difficulty)` called from the battle-side creation sites for every non-X-COM unit: alien crews in `fillMap`, guards (`battlemap.cpp:382`), guardians (`:1220`), reinforcements (`battle.cpp:2333-2385`), brood spawns, turrets. It must not key on `owner != player`: a hireable agent is created with its origin organisation as owner. Accuracy `acc = 100 - floor((100 - acc) x (100 - 4D)/100)`; armour per zone through a serialised per-agent `armourScalePercent` (default 100; old saves load unscaled) read at `battleunit.cpp:1824`, because `AgentType` is shared.
- **Test.** New `tests/test_difficulty_scaling.cpp`, D = 4, from `v_tactical_difficulty.md` 2.2: Anthropod TU 64, HP 69, reactions 25, stamina 69, armour 8/9/9/9/8, accuracy 0 to 16; Skeletoid 99/39/53/150, 22/26/22/22/24, 30 to 42; Popper HP 40, armour 11; Megaspawn HP 301, armour 45/60/49/49/55; Queenspawn HP 696, armour 49; Psimorph psi energy 100; Micronoid armour unchanged; turret 580/69/51/67/57. Order: a hostile-org agent is hired, then fought: unscaled. A mean-42 Disruptor hit leaves a Popper alive (42 - 11 = 31 < 40).
- **Strategy.** `03` 5: plan against +16% TU/HP and +64% armour; armour-piercing and energy weapons gain value; "one-shot Poppers with the Disruptor" is invalid.
- **Effect.** Superhuman aliens are Novice-strength today. Benchmark-sized (squad survival). [MEASURE] kills per magazine and casualties per mission.
- **Order.** First of B; B2 depends on it.

#### B2. `BD04+TD10+TD8+TD9` - the base-defence roster, turret record and weapons

- **Verdict.** `TD10` critical, `TD8` high, `TD9` medium, survived; `BD04` medium, adjudicated ("cap stations at 3" removed); `BD18` low; `TD7` (count, placement) matches; `TD11`, `BD20` [UNBOUND].
- **Original [EXE].** `FUN_000ac7d0`: every employed agent **located** at the base plus class-0 agents in craft docked there; armament never tested. Budget `36 - T`, `T = min(12, 4 x finished Security Stations)`; over budget each of the four classes is cut to `floor(count x budget/total)`, first n in array order; 36 X-COM units, 60 in total. Turrets (TACP `FUN_00024110`): 4 per station sector (23) and 4 per Advanced (24); roster takes slots first; **no cap of 3 stations**. Turret = record 5: 500 HP, TU 60, reactions 44, armour 35, scaled 580/69/51/67/57; sector 23 Megapol Plasma Gun + 2 Plasma Pods (35, accuracy 55, delay 120, range 900), sector 24 Disruptor Gun (42, 30, 128, 800); shot counts undecoded (`resolved-facts.md` 6.4).
- **Engine today.** `battlemap.cpp:205-256`: docked-craft agents, then every Soldier-role agent in the base, then non-soldier staff by `homeBuilding->base` even if away; soft cap 36; soldiers first. Turrets from `guardianLocations` (`:1214-1221`), 4 per station, uncapped, uncharged to the 36; armour 0, unscaled, invented guns (laser 26/95/42, disruptor 70/75/40; `agent_types.xml:1039-1228`, `agent_equipment.xml:83-210`); the extractor labels record 5 `AGENTTYPE_CORPORATE_BOSS`. Sign mixed: [MEASURE].
- **Target.** Both; `battlemap.cpp` clean; turret and weapon data are patch data (data arm).
- **Change.** (1) Draft by presence (`currentBuilding == base`, craft docked there), drop the `homeBuilding->base` clause. (2) Budget `36 - min(12, 4 x finished stations)` (`buildTime == 0`, sectors 38/39). (3) Over budget scale each class to `floor(count x budget/total)`, first n in array order. (4) Optional 60-unit cap. (5) Do not cap stations. (6) Rebind record 5 as the base turret, armour 35 per zone (57 after B1); sector 23 Megapol Plasma Gun + 2 pods, sector 24 Disruptor Gun; drop the invented guns.
- **Test.** `test_base_defence_force.cpp` (with A8): `roster_by_presence` (away staff not drafted); `budget_with_stations` (3 stations leave 24 slots; a 4th changes nothing, is not capped); `class_dilution` (10 soldiers + 40 staff with T = 12: budget 24, total 50, 4 soldiers, each staff class cut by the same factor, first n in array order); `turret_record` (580 HP, armour 57, weapon ids at D = 4). Data stamp for the turret half.
- **Strategy.** `03` 7.4, 7.6, 7.7: WHEN a base may be attacked THEN keep people physically present at or below `36 - 4 x stations` (24 with 3 or more), send staff away first, keep 10-12 armed soldiers; count each station as 4 turrets of 580 HP, armour 57, weaker guns than the engine shows; stations are neither inert nor capped at 3 (retires shim `garrison_by_presence`).
- **Effect.** Garrison sizing from engine logs does not transfer; outcome measured with B3 (`baseDefences{defendersPresent, turrets, outcome}`).
- **Order.** After B1 and A8. **Blocked on RE** for turret shot counts and slot exhaustion with 4+ stations (section 7).

#### B3. `BD08` - hostile organisations raid X-COM bases (with `BD10`, `BD11`)

- **Verdict.** `BD08` high, **engine_bug**, adjudicated (validator "match" refuted); `BD10` high (flat incident maps); `BD11` low; `BD12` matches; `BD09` is C3.
- **Original [EXE].** `FUN_00092060` (file `0xF4704`) once per game day: A in 2..26, B in 0..26 except 1; pair passes if `M[A][B]` <= -50 and A has a building; pressure `max(1, snapshot - M)`; trigger iff pressure > `rand16(timer)` and one of 10 slots is free (timer := `80 - 2d` = 72 on a trigger, else -1); delay `rand16(240) + 10` units of 5 minutes, against a uniformly random **live** building of B (a purchased base counts at once). Only codes 2 (Raid) and 3 (Storm) attack a base; on a non-base X-COM building they are a no-op. Per trigger, when the building is a base: Cult and most corporations Raid 25.0%; Megapol Raid 36.8% + Storm 25.0%; Psyke/Diablo/Osiron/taken-over 29.1% + 16.4%. The pool at the start is the start base plus the Recyclotorium: P(base given Raid/Storm) is 1/2 (one base), 2/3 (two) or 1 if the owner remap at VA `0x1017C` empties the non-base pool [UNBOUND] (`resolved-facts.md` 6.3).
- **Engine today.** `setRaidMissions` (`organisation.cpp:350-430`) has the right structure, but the target comes from `Organisation::buildings` (`:71-80`), filled only by `City::initCity`; the start base, `BaseBuyScreen` (`basebuyscreen.cpp:93`) and `Base::die` change `owner` without touching it. In a continuous campaign the pool is the Recyclotorium and `RaidMission::execute` takes the else branch (`:1308-1338`: relation +7/+12, never `DefendTheBase`); base raids appear only after a save and load. Census of 100 saves: 49 pending raids on X-COM buildings, 48 on the Recyclotorium, 1 on a base. Also: timer reset before the null-target check; no 10-slot cap; Aliens iterated as B; flat incident maps (`extract_organisations.cpp:366-397`: Cult Raid 10% against 25%, Megapol 30% against 61.8%); `:399` overwrites `neutral_low_manpower`.
- **Target.** Both; `organisation.cpp` clean; incident tables are extractor data (data arm; the file moved since the snapshot).
- **Change.** (1) `pickRandomBuilding` and the `setRaidMissions` source list scan `city->buildings` by `owner.id` (fixes former-owner staleness; leave other consumers of `buildings`). (2) Raid/Storm on a non-base X-COM building is a no-op. (3) Skip a pair with no target before the timer reset; non-hostile pairs never touch the timer; 10-slot cap that still decrements when full; skip B == Aliens; a raid snapshot separate from `previous_relations` (rotate one X-COM-column cell per game hour mod 27). (4) `BD10`: the two EXE byte tables and the biased draw. (5) Fix the alias at `:399`.
- **Test.** `raid_reaches_defend_the_base_without_save_load` (on the `test_city_rules.cpp` fixture): purchase a base, force a hostile Cult, advance until a Raid or Storm, observe `DefendTheBase` with no save and load; after a reload the same target is picked (order: identical scan before and after). Plus `raid_on_non_base_is_noop`, `raid_pair_skipped_before_timer_reset`, `raid_slot_cap`, `incident_histogram` (25.0 / 36.8 + 25.0 / 29.1 + 16.4 within tolerance). Stamp for the tables.
- **Strategy.** `01` 3.6, `03` 7.3: WHEN the Cult, Government or Megapol is hostile (-50 or lower) THEN expect Raid/Storm base attacks from the first days (the Cult starts at -70..-50 in all 21 seeds; about 25% x P(base) per Cult trigger) and keep an armed garrison UNTIL that organisation's relation to X-COM is above -50. A continuous engine run never shows them.
- **Effect.** Base-defence frequency rises to the original's; with B2 it changes the outcome mix (`baseDefences{trigger}`). P(base) stays [UNBOUND].
- **Order.** Right after B1; measured with B2 so a lenient roster does not meet more defences.

#### B4. `G8` - bankruptcy ends the game (with `VC-14`)

- **Verdict.** `G8-bankruptcy` high, engine_missing, survived; `VC-14` is the same rule.
- **Original [EXE].** `FUN_00060ea4` (file `0xC3548`), last act of every funding pass: "X-COM IS DEFEATED, bad debts" when the balance is **strictly** below -5,000; no week counter, no difficulty term. With funding cut, 100,000 cash and 22,900 overheads the Mondays after the cut give 77,100, 54,200, 31,300, 8,400, -14,500: defeat on the 5th [DERIVED] (`resolved-facts.md` 3.5).
- **Engine today.** None; `GameLost` only at the last base (`base.cpp:155-159`, `cityview.cpp:5149`); overheads unconditional (`LIVE:gamestate.cpp:1864`). 4 of 164 logged campaigns sat below -5,000 after a Monday and played on (one reached -70,457 by day 45).
- **Target / change.** Both; `gamestate.cpp` LIVE-touched; hand to the funding owner. After the overhead line in `weeklyPlayerUpdate` (skipped at `gameStart`): `if (player->balance < -5000)` raise game-lost with a bad-debts reason and the original's text.
- **Test.** `test_weekly_funding.cpp` `bankruptcy`: order (check after funding and overheads in the same Monday, not at game start); -5,000 survives, -5,001 ends; the five-Monday sequence ends on the 5th. `test_game_end.cpp` (`defeat_messages` :13) gets the message.
- **Strategy.** `01` 5.5 austerity: WHEN funding is cut THEN about four Mondays of cash remain: sell stock and stop hiring and building the Monday the cut is seen UNTIL non-Senate income covers the overheads (funding never resumes); the harness keeps its own -5,000 floor (shim `bad_debt_floor`) until this lands.
- **Effect.** Ends campaigns that limp on below the floor. Test-sized. **Order.** After `G4` and GA.

#### B5. `RR-09+RR-10+F-M12` - personnel caps, the hire pool and the lab cap

- **Verdict.** `RR-09` high, survived; `RR-10` (same as `V-AG-07`) medium, adjudicated ("cap the mean at 1.5" rejected); `F-M12` medium, adjudicated; `RR-11` closed by `F-M12`. `RR-10` is mixed in sign (kept in the bundle).
- **Original [EXE].** `FUN_0007a474` daily, per class (agents cap 100, biochemists/engineers/physicists 50, employed plus listed, inside 180 slots): remove `rand[0, floor(n/3)+1]`, add `min(rand[0,3], cap - (employed + remaining))`, origin uniform 2..22, no relation or takeover test; plus `rand[0,1]` hybrids if Mutant Alliance to X-COM >= 0 and androids if S.E.L.F. >= 0. Mean 1.5 arrivals per technical class per day. `FUN_00069270` ("Regulation 44", file `0x152252`): a 7th small or 5th large lab is refused; 6 small across Biochemistry Lab, Quantum Lab, Workshop (`0x1A`, `0x1C`, `0x20`); 4 large across the Advanced types (`0x1B`, `0x1D`, `0x21`); counted per building including under construction; strict funds test (`resolved-facts.md` 7.1-7.2).
- **Engine today.** No caps (`recruitscreen.cpp:604-680`; `organisation.cpp:541-626`); 33% of every hirable agent leaves daily; a hostile organisation produces nothing; chance `25 + relation`: 1.59 / 2.08 / 2.58 / 3.56 / 4.57 arrivals per class per day at relation -20 / -10 / 0 / +10 / +20. No lab cap (`base.cpp:268-306`): about 35 small or 9 large labs fit.
- **Target.** Both; `organisation.cpp`, `recruitscreen.cpp`, `base.cpp`, `basescreen.cpp` clean; two daily call sites in `gamestate.cpp`.
- **Change.** (1) One daily pass per class as above; delete the 33% churn and per-org attempts; location rule independent of the origin org's buildings. (2) Block hiring past the class cap and the 180 total. (3) `BuildError::TooManyLabs` (`base.h:43-53`) plus a message-box case near `basescreen.cpp:401` (else the drop fails silently); only when not `free`; count by capacity type (Chemistry, Physics, Workshop) and size class, including under construction; refuse at 6 small or 4 large.
- **Test.** New `tests/test_personnel_caps.cpp`: `pool_rule` (long replay: mean 1.5 per class per day, never above the cap, origin uniform 2..22, still arriving after the Government is taken over); order: a hire at the cap is refused before funds are charged; `lab_cap_6_small_4_large` (the 7th small and the 5th large refused; caps independent).
- **Strategy.** `01` 2.2, 2.4, `03` 8.1: at most 6 small + 4 large labs per base, 50 per technical class, 180 persons, a flat 1.5 arrivals per class per day; no staff from Megapol/Government relation; hybrids/androids only while Mutant Alliance / S.E.L.F. >= 0 (retires shims `hire_caps`, `lab_caps`).
- **Effect.** Easier than the original early, harsher after takeovers (rarer after `T1+T2`). Test-sized (`hirePool`, `labRate`). **Order.** Before the staffing tables of `01`/`02` are tuned.

#### B6. `F-M1+F-M2+F-M3` - market stock and price rules (with `F-M4`, `G12`)

- **Verdict.** `F-M1` high, survived; `F-M2` high, adjudicated; `F-M3` medium, must land with `F-M2` (the price pass on the old decay cuts only about max/12 a week); `F-M4`, `G12` low.
- **Original [EXE].** Monday `FUN_000941dc`: a stock pass over **all** rows, then a price pass over **all** rows, even with funding terminated; rule chosen by the record's first word `week == 0`, not by manufacturer. Stock: `s -= s/8`; `r = rand(0..100)`; `r > 40`: `s -= s/5`; `r > 70`: `s -= s/3`. Price from post-decay stock: `stock >= floor(max/2)`: `e = rand(0..2)+3`; `>= max`: `rand(0..5)+5`; `>= 2max`: `rand(0..5)+10`; `price = trunc(price x (100 - e)/100)`, clamp `[base/2, 2 base]`, never rises, `lastStock` unused. A sale adds to stock at once but pays the price shown (`resolved-facts.md` 10.1).
- **Engine today.** `economyinfo.cpp:16-88`; selector `manufacturer == player` (`gamestate.cpp` HEAD `:1674`, LIVE `:1707`). X-COM-made goods: weekly sold delta, decay x0.80/x0.66, cuts 95-97 / 90-95 / 85-95%. **15 priced week-0 items not made by X-COM are frozen at base price**: Psiclone $4,500 (max 40), Elerium $3,000 (max 40), the two AG missiles ($900 max 30, $700 max 15) and 11 craft. Trickles are free; a dump costs one cut (about 0.899).
- **Target.** Both; `economyinfo.cpp`, `test_economy.cpp` clean; `updateItemMarket` LIVE-touched (small).
- **Change.** Drop the `xcom` parameter and the selector; branch on `weekAvailable == 0`; keep the random walk for `weekAvailable != 0`; two passes over all rows; truncating arithmetic; remove `lastStock`/`soldThisWeek`.
- **Test.** Rewrite `tests/test_economy.cpp`: `xcom_stock_bands` (:52) hard-codes the 80% / 66% law and the `lastStock` copy; replace by `stock_law`: from 100 the exact outcomes are 88 (r <= 40, 41 of 101 values), 71 (r 41..70, 30), 48 (r 71..100, 30) [DERIVED 100 - 12; 88 - 17; 71 - 23]. `price_pass_order`: every stock pass completes before any price pass; price reads post-decay stock; three tiers, clamp, never rises. `selector`: Psiclone, Elerium, both AG missiles and week-0 craft take this path. `craft_sale_adds_no_stock` (`F-M6`). The signature change touches the five existing cases (:23-139) and `aequip_market_week0` (`test_city_rules.cpp:2578`).
- **Strategy.** `01` 4.2, 4.3: sell each week-0 type at most about 0.075 x max a week (about 3 for max 40, 1 for max 20, one per 4-6 weeks for max 8) or in one lump per type, spread across types; WHEN a type's market price fell on a Monday THEN stop trickling it UNTIL it is sold as one lump (the price never rises).
- **Effect.** Cult-loot income is overstated beyond about 6 weeks: 10.6 Psiclone a week for 26 weeks is $1.24 M engine against $0.93 M original (-25%); 16.4 a week $1.92 M against $1.27 M (-34%) [DERIVED]; inside 6 weeks the loss is 1-10%. Test-sized. **Order.** Before the income loops of `01` are tuned beyond week 6.

#### B7. `F-M7` - craft sale valuation

- **Verdict.** `F-M7` medium, diverges, survived (health field inferred).
- **Original / engine.** `FUN_0008da9c`: (type price - default equipment) x `health/constitution` + installed equipment; no ammunition term. Engine (`transactioncontrol.cpp:552-585`): type price + installed + loaded ammunition x ammunition price - default equipment; no health term.
- **Target / change.** Both; clean file. `price = (typePrice - sumDefaultEquip) x health/maxHealth + sumInstalledEquip`; remove the ammunition term.
- **Test.** `craft_sale_value`: half-health craft sells for half the hull; loaded missiles add nothing; undamaged unchanged.
- **Strategy / effect.** `01` 4.2: sell undamaged craft; crash-recovered UFOs and shot-up Valkyries are worth less than the engine says. Test-sized. **Order.** Step 11, with B6.

#### B8. `T10` - guards of taken-over organisations are three tiers stronger

- **Verdict.** `T10` medium, engine_missing, adjudicated (exact, no RE).
- **Original / engine.** `FUN_000b46c4`: tier = organisation start tech + week counter - 1, **+3 if taken over**, clamp 0..11; three independent `rand(9)` draws; no difficulty read. Engine `agent.cpp:171-175`: no takeover term.
- **Target / change.** Both; clean file. `int tier = org->tech_level + (org->takenOver ? 3 : 0);` into `EquipmentSet::getForType(state, Human, tier)` (no clamp: `HUMAN_12` has `max_score` `INT_MAX`). Optional: a bool on `createAgent` set only from `battlemap.cpp:382`.
- **Test.** A taken-over organisation with `tech_level` 4 draws from HUMAN_7, not HUMAN_4; order: the set changes at the next guard generation after the takeover.
- **Strategy / effect.** `03` 6.1, 6.3: raiding a taken-over organisation's building meets defenders up to 3 tiers stronger: over-equip or avoid. Test-sized. **Order.** Step 10.

#### B9. `T8` - the day-1 alien crews are larger

- **Verdict.** `T8` medium, diverges, adjudicated; `T16` closed by it.
- **Original / engine.** `FUN_00015400` picks a random building whose owner's relation to X-COM is >= 0 and writes Brainsucker `1 + rand(d/2+1)` (1..4) and Anthropod `1 + rand(d/2+2)` (1..5) at d = 4. Engine 1..3 each: the extractor writes an exclusive bound (`extract_agent_types.cpp:1335, 1337`; `gamestate.cpp` HEAD `:705-709`, LIVE `:706-710`).
- **Target / change.** Both (same two-line extractor edit; data arm): line 1335 `Vec2<int>{1, difficulty/2 + 3}`; line 1337 `Vec2<int>{1, difficulty/2 + 4}`; regenerate `base_gamestate`.
- **Test.** `initial_aliens[4]` reads {1,5} for Brainsucker and {1,6} for Anthropod under exclusive bounds; stale data fails by the stamp.
- **Strategy / effect.** `03` 2, 3.4: one random building starts infested with up to 4 Brainsuckers and 5 Anthropods: read the infiltration graphs from day 1. Test-sized. **Order.** Step 7, with B1.

#### B10. `RR-12+RR-13` - which UFO recoveries unlock the alien-system research

- **Verdict.** `RR-12` high, diverges, survived (unmanned-loot half medium confidence); `RR-13` medium, engine_bug, survived ("won" on a retreat medium confidence).
- **Original [EXE].** Only Probe and Scout are unmanned (`FUN_000b300c`); recovering one (`FUN_000ac08c`) sets only that type's own topic flag and removes the craft: no loot, none of the three alien-system flags. Records 76, 77, 75 are written only in `FUN_000b32ac`, manned branch, only if `[0x244084]` is 1 (won); the loot loop shares the guard (`resolved-facts.md` 7.3).
- **Engine today.** `extract_vehicles.cpp:147-156` gives all ten types the three unlocks; Probe and Scout (no battle map, `:248-253`) take the unmanned branch and force-complete the list (`cityview.cpp` HEAD `:4743-4750`, LIVE `:4818-4825`), and `loadUnmannedUfoLoot` (`vehicle.cpp:1660`) hands over the equipment. A manned recovery unlocks with no `playerWon` test (`battle.cpp` HEAD `:3639-3649`, LIVE `:3646-3656`). The alien chain starts 5-13 days early (`web_research_tree.md` D-2).
- **Target.** Both; extractor clean (data arm); `cityview.cpp`, `vehicle.cpp`, `battle.cpp` LIVE-touched.
- **Change.** Emit the three unlocks only for types with a battle map (index 2 and up), keep `UFO_TYPE_{i+1}` for all; remove or zero `loadUnmannedUfoLoot` (arrival handler `FUN_0003A910` not read for grants); wrap the unlock loop in `if (playerWon)`, `vehicle->die` outside. Regenerate.
- **Test.** `test_city_rules.cpp` `unmanned_ufo_loot` (:2179) pins the loot handover as free cargo: delete or invert once confirmed. New `ufo_recovery_unlocks`: a Probe recovery completes only its type topic; a manned recovery with `playerWon` false unlocks nothing; a won one completes the three, then the type topic. Stamp.
- **Strategy.** `02` 7.2: WHEN a Probe or Scout is downed THEN expect only its own topic; the alien-system chain needs a manned UFO recovered through a won ground mission; the engine's 5-13-day head start is not in the original.
- **Effect.** The research critical path moves later by those days [DERIVED]. Test-sized. **Order.** With C1 (one regeneration). **Blocked on RE** only for "won on retreat" (section 7 row 9).

#### B11. `F9` - escort and attack launches carry the score-gated gear

- **Verdict.** `F9_alien_craft_gear_score_gates` medium, engine_bug, adjudicated.
- **Original [EXE].** Nine latch flags set at every game midnight by `FUN_00097b90` (table file `0x142174`): lifetime UFO Shot Down score 1,000 / 2,000 / 4,000 / 8,000 gives Small Disruption Shield (id 44), Large (45), Cloaking Field (46), Teleporter (47); every incursion craft (primary, escort, attack) is created by one routine that blanks locked equipment. 1,000 points = 20 Probes, 10 Scouts or 2 Battleships (`resolved-facts.md` 8.7).
- **Engine today.** `equipDefaultEquipment` runs only in the primary launch loop (`gamestate.cpp` HEAD `:1033`, LIVE `:1034`), `placeVehicle` and non-player portal crossing; escort and attack launches never equip: 59 of 175 shipped launch slots (34%).
- **Target / change.** Both; `gamestate.cpp` LIVE-touched. `invader->equipDefaultEquipment(*this)` right after `enterDimensionGate` in the escort loop (HEAD `:1100`, LIVE `:1101`) and the attack loop (HEAD `:1153`, LIVE `:1154`); one helper for the three loops; no RNG consumed.
- **Test.** At Superhuman set `craftShotDownUFO` = 2000, pre-place an Escort created at score 0, launch with an escort slot, assert the large shield (order: equip after gate entry); the control at 0 asserts none.
- **Strategy.** `03` 4.7: WHEN the UFOs Shot Down total (week plus previous) reaches 1,000 / 2,000 / 4,000 / 8,000 THEN UFOs launched after the next midnight carry shields, large shields, cloaks, teleporters; small kills trade against armed UFOs later (latches never clear).
- **Effect.** Engine fleets are weaker once thresholds pass; inert at today's horizon. **Order.** Step 12.

#### B12. `F6` - incursion cadence (held behind a measurement)

- **Verdict.** `F6_invasion_timer_cadence` high, diverges, survived; confidence medium, double drain INFERRED.
- **Original [EXE].** `FUN_0006cfb4`: timer after each incursion `24 h + rand(0..2820 min) + rand(0..3600 s)`; first timer `10 h + rand(0..2 h)`. INFERRED: two code paths drain the counter (per frame and per 5 minutes), halving the interval to about 12-36 h with the first UFO 5-6 h after the noon start (`resolved-facts.md` 8.6).
- **Engine today.** `gamestate.cpp:718-722` (HEAD `:717`) and `:910` (HEAD `:909`): first timer and every reset 24-72 h; single drain.
- **Change.** Do not change until measured. (1) `incursionsLaunched[week][role]`: confirm 3.5 a week in a headless engine run. (2) Confirm the double drain by observation of the original (a recorded game, or `FUN_0004AB04` running 288 times a game day). (3) If confirmed: first timer `10 h + rand(0..7200 s)`, halve every reset.
- **Test (after 3).** The first launch falls 10-12 h after the noon start (order: before the first reset); intervals 12-36 h.
- **Strategy / effect.** `01` 3.5, `03` 7.6: garrison from week 1 and intercept from the first evening. About twice the incursion pressure if confirmed [DERIVED 3.5 against 7 a week]. **Order.** Last of B; section 7 row 5.

#### B13. `F15b` - rift acceptance restores M + 25, not 100 (with `F15a`)

- **Verdict.** `F15b-rift-resolution` high, engine_bug, survived; `F15a` matches. Group B, not A as the brief expected: the engine is more generous (section 1.2).
- **Original [EXE].** Hourly `FUN_00096bc8`, one organisation per call (each every 27 h): offer needs `snapshot - M > 15`, `M < 25`, not taken over, `rand16(0..100) > 10(d+1)` (49.5%); accepting sets the relation to 0 if negative, else +25 (`resolved-facts.md` 4.1).
- **Engine today.** `organisation.cpp:1230` `signTreatyWith`: `forceAlliance ? 100 : (myRelation > 0 ? max(myRelation + 25, 100) : 0)`: a relation in (0, 25) jumps to 100; it writes the current and long-term relation (the raid-pressure baseline).
- **Target / change.** Both; clean file. Non-alliance path `newValue = myRelation < 0 ? 0 : myRelation + 25`, org to player then mirror.
- **Test.** `test_diplomacy.cpp` `sign_treaty` (:65) covers negative to 0 and alliance to 100 only; add 10 to 35, 0 to 25, -20 to 0; long-term baseline equals the new value.
- **Strategy / effect.** `01` 4.5: a relation in (0, 25) after a drop is restored to 0..+49, never 100; four recovery paths exist (rift offer, on-demand settlement, alliance request, kill ripples). Test-sized. **Order.** Step 12.

#### B14. `V-AG-01` - training cadence, capacity and psi clamps (with `V-AG-02`, `V-AG-04`, `V-AG-03`, `V-AG-05`)

- **Verdict.** `V-AG-01` high, `V-AG-02`, `V-AG-04` medium, `V-AG-03` low; `V-AG-05` low, adjudicated: the starter assignment is right, do not remove it.
- **Original [EXE].** `FUN_0006bf04` (4 calls per game hour): a person at the home base adds `v = min(10, floor(100F/N))` to one accumulator (`F` finished Training Areas or Psi-gyms of capacity 10, `N` trainees); `FUN_0006c058` runs **once per game midnight**: accumulator >= 160 gives one roll set, then zero. Combat set: reactions, speed, strength, health +1 each with `P = (100 - stat)/100`; accuracy raw -1 with `(raw - 4)/100`; stamina raw +1 with `(200 - raw)/200`. Psi: `P = 3 x initial - current` (%), caps 100 / 99 / 100. Wounded persons heal instead and do not count; the 10 starters are pre-assigned Combat (`resolved-facts.md` 9.1).
- **Engine today.** `agent.cpp:23-26`, `:1105-1127`: six roll sets per game day at usage <= 100%, slowing as `100/usage`; injured agents train; `trainPsi` unclamped (hybrid about 122); accuracy cap 100; stamina +2 raw. The starter assignment matches (`gamestate.cpp:802/803`).
- **Target / change.** Both; `agent.cpp`, `base.cpp` clean. `updateHourly`: at home, not wounded, assigned: `progress += 4v` per game hour; `updateDaily`: if `progress >= 160` one roll set, zero it; clamp psi; gate on `modified_stats.health >= current_stats.health` in `agent.cpp` and `Base::getCapacityUsed` (`base.cpp:535-554`); keep `StatGrowthMultiplier` on the accrual; leave `gamestate.cpp:802/803`; never press `BUTTON_AGENT_PHYSICAL`.
- **Test.** New `tests/test_agent_training.cpp`: at most one roll set per midnight (order: at midnight, not hourly); full rate to `N = 10F`; one set a day holds to `N = 50F` for an agent home about 20 h; wounded agents neither train nor count; psi clamps. Mean-field stat 25 after 7/14/28/56 days: 30/35/43/57 against 51/68/86/97 [DERIVED, register].
- **Strategy / effect.** `03` 8.2, 8.3: budget growth at 1/2 to 1/6 of what engine logs show; one Training Area plus one Psi-gym covers a roster; heal the wounded first. Test-sized (`trainingRollSets`). **Order.** Step 12.

#### B15. `BD13` - a base building dies at 90% value loss

- **Verdict.** `BD13` high, diverges, survived.
- **Original / engine.** The original: dead when `10 x sum(General tile values) < original sum` (verified for all 109 and 96 buildings of two maps), zeroing the resident crew. Engine `building.cpp:1086-1113, 1165`: dies only when no building-part tile remains (`isBuildingPart` = tile value > 5, `extract_city_scenery.cpp:222`).
- **Target / change.** Both; `building.cpp` LIVE-touched; the per-building baseline is data (BLD word `+0x0C`, or recomputed): data arm. Port the value-sum test; keep the crew clear; leave the crew-quarters rule.
- **Test.** New `tests/test_base_collapse.cpp`: 91% destroyed is dead, 89% alive; order: crew zeroed with the death; baselines for the 109- and 96-building maps equal the verified sums. Stamp.
- **Strategy / effect.** `03` 7.5: bombardment (Attack UFOs from week 4) destroys a base at 90% loss: prefer sturdy sites. **Order.** After A10; baseline derivation for the other three maps is data work (section 7 row 17).

#### B16. `BD14b` - repair bill after a won base defence (blocked on RE)

- `BD14b` medium, engine_missing, survived. A won defence debits `50 x worth` with no funds check ($50 per damaged tile inferred); `worth` is the shared mission-record word at UFO2P VA `0x244090`, writer unbound (`resolved-facts.md` 6.4). Engine: no charge (`battle.cpp:3603-3606`). **Not written**; do not guess the unit. Strategy `01`: do not budget a repair bill; it is an unmeasured cost of each successful defence. Lead in section 7 row 1. **Order.** Section 7 row 1.

---

## 5. Group C - neutral, mixed or bookkeeping fidelity

**Summary.** Two fixes get full blocks because other files depend on them: the research chain (`VC-2+VC-3+RR-14`) and the Control Chamber pair (`UFO_GROWTH_STOP+VC-10`). Six smaller fixes follow in compact form, then the low-impact table. Group C lands after B, except that the research chain is pulled forward with `RR-12+RR-13` and the Control Chamber pair rides with the endgame bundle.

#### C1. `VC-2+VC-3+RR-14` - the alien-building research chain

- **Verdict.** `VC-2`, `VC-3` high, `RR-14` medium; diverges, survived; adjudicated as **one change**. `VC-4`, `VC-5` match. Group C: the total is 390,000 man-hours in both rule sets and only the shape differs (the brief expected `VC-3` in B).
- **Original [EXE].** Function `f` is gated on research record `88 + f`; destroying `f` unlocks record `88 + succ(f)`, `succ`: 4 to 2, 2 to 6, 6 to 8, 8 to 0, 0 to 7, 7 to 1, 1 to 5, 5 to 3, 3 to 9 (`FUN_000b4538`): Sleeping, Food Chamber, Alien Farm, Maintenance Factory, Incubator, Control Chamber, Spawning, Organic Factory, Megapod, Gate. Only record 92 (Sleeping, first raid) has a topic prerequisite: The Alien Dimension (record 1, 6,000 man-hours), available at the first move into the alien city. Hours for raid step k = 0..9: 30,000 + 2,000 k (scores 300..480). First-raid path after the first crossing: 6,000 + 30,000 = 36,000 man-hours, 40 h at 900 an hour (`resolved-facts.md` 8.2).
- **Engine today.** `extract_buildings.cpp:76, 82-94` binds `accessTopic = RESEARCH_ALIEN_BUILDING_{i}` by `albuild` **file position**: right order, wrong hours, prerequisite and score per step (campaign positions 1..10: 38, 42, 32, 46, 30, 44, 34, 40, 36, 48 thousand). Step 1 has no prerequisite and is startable on day 0; The Alien Dimension gates the 5th; debt peaks at +26,000 after the 4th (`research.xml:1411-1480`). Cumulative thousands after k raids: original 30, 62, 96, 132, 170; engine 38, 80, 112, 158, 188.
- **Target.** Both; extractors and patch data clean (data arm); `test_city_rules.cpp` LIVE-touched.
- **Change.** `extract_buildings.cpp:82`: `accessTopic = RESEARCH_ALIEN_BUILDING_{function_idx}`; `:83-87`: `researchUnlock` = successor's topic per `succ`; re-key the hidden `UNLOCK_ALIEN_BUILDING_*` topics and dependencies to function index in `extract_research.cpp` and the patch (record 92 depends only on `RESEARCH_THE_ALIEN_DIMENSION` and has no unlock flag; the other nine depend on their unlock flag, force-completed by destroying the predecessor); keep `victory` on function 9 and the `setCurrentCity` unlock (`LIVE:gamestate.cpp:331-337`, matches `FUN_0006e098`). One change; do not split.
- **Test.** `test_city_rules.cpp`: `alien_building_exe_rows` (:1307) checks the **topics** (record-indexed; correct) and passes while the binding is wrong: keep, add binding assertions. `alien_building4_keeps_table_prereq` (:1278) asserts `RESEARCH_UNLOCK_ALIEN_BUILDING_4` is a dependency: that pins the engine's patch; invert. New `alien_building_chain_order`: after The Alien Dimension only the Sleeping topic is startable; winning each raid unlocks exactly the successor, in the order 4, 2, 6, 8, 0, 7, 1, 5, 3, 9; a topic whose predecessor was not won is not startable; `RESEARCH_ALIEN_BUILDING_0` not startable on day 0. Stamp; control on stale data.
- **Strategy.** `02` 8.1: WHEN the first crossing has happened THEN research The Alien Dimension (6,000) then the Sleeping topic (30,000); each later topic unlocks only by winning the previous raid and costs 2,000 more; do not pre-research the first building before the crossing. The day-0 probe of `02` remains as the control-arm detector; retires shim `no_early_alien_building_research`.
- **Effect.** Totals unchanged; shape and the pre-research shortcut change [DERIVED]. **Order.** With `RR-12+RR-13`.

#### C2. `UFO_GROWTH_STOP+VC-10` - the Control Chamber stops growth and arms a wave

- **Verdict.** `UFO_GROWTH_STOP` high, diverges, adjudicated (same finding as `RR-15`, `VC-9`, `F4_ufo_growth_stop_building`); `VC-10` critical and `F5` high, engine_missing, survived, part blocked on RE. One pair, one option: the engine's growth stop is two raids later than the original's and the original's wave is absent, so changing either alone makes the sixth raid easier or harder than the original.
- **Original [EXE].** Weekly growth `FUN_0006edac` starts with `if byte [0xDE62A] != 0 skip`, written only by `FUN_000b4538` case 7: winning the **Control Chamber (function 7, the 6th raid)**, permanently; the Organic Factory (function 5, the 8th) has no effect. Else `pool[t] = min(pool[t] + row[t], cap[t])`, `row = clamp(week - 1, 0, 15)`, caps (Probe..Mothership) 15, 15, 6, 6, 6, 6, 6, 6, 4, 2. Winning the Control Chamber also sets `[0xF3C78]`: at the next invasion-timer expiry `FUN_0006cfb4` calls `FUN_00070938`: up to 20 ships round-robin over UFO types 1-9 (no Probes) from the dimension stock; fewer than 10 gathered: flag clears, nothing launches; else one spawns per step with role 9 (never withdraws) on mission 4 (a random building not allied to the Aliens, owner not taken over); a Mothership has 50% promotion to role 10 (`resolved-facts.md` 8.4-8.5).
- **Engine today.** `UFOGrowth::craftFactoryIntact` (`ufogrowth.cpp:51-70`; `gamestate.cpp` HEAD `:1593`, LIVE `:1626`, now 1661) is true while a living Organic Factory exists in `CITYMAP_ALIEN` (fail-open), justified by UFOpaedia prose (`ufogrowth.h:21-23`): growth stops about 13 days (two raids) late [INFERRED at about 6.4 days a building]. No wave.
- **Target.** Both; `ufogrowth.cpp` clean; `gamestate.cpp`, `battle.cpp` LIVE-touched.
- **Change.** (1) `craftFactoryIntact` tests `BUILDINGFUNCTION_CONTROL_CHAMBER` alive, keeps the fail-open, is renamed; comment cites `FUN_0006edac` (file `0xD1455`) and `FUN_000b4538` case 7 (file `0x116C21`); optionally a latched serialised bool set in the RaidAliens success branch. Do not gate on `RESEARCH_UNLOCK_ALIEN_BUILDING_6` (C1 renumbers it); no case-5 flag. (2) `GameState::massAssaultArmed`, set at the same moment; in `invasion()` when armed gather up to 20 living `ORG_ALIEN` vehicles of types 1-9 in `CITYMAP_ALIEN` round-robin; fewer than 10: clear, launch nothing; else launch each with `attackBuilding` on a random building not allied to the Aliens and not taken over, `withdrawHealthPercent` 0, clear the flag (launched ships leave as real vehicles, so the stock decrements). Target filter (`FUN_00070acc`, `FUN_00070a2c`) and the original's stock decrement at VA `0x143D40` are [UNBOUND]; the interim is labelled so in code.
- **Test.** Update the HEAD-only tests that pin the wrong gate: `test_city_rules.cpp` `organic_factory_gates_ufo_growth` (:1756) and `test_gamestate_ufo_growth_wiring.cpp` (:86, :94); retarget to the Control Chamber and add the control: killing the Organic Factory leaves growth running; killing the Control Chamber stops it from the next Monday growth pass, not retroactively (order). Wave: 12 ships of types 1-9 at the next expiry launch 12 and clear the flag; 9 launch nothing and clear it; 25 launch exactly 20, round-robin over types 1-9, no Probe.
- **Strategy.** `02` 8.6: WHEN reinforcements must be capped THEN raid to the Control Chamber (6th) in the original; the engine stops nothing until the Organic Factory (8th). WHEN preparing the 6th raid THEN thin the standing fleet first UNTIL fewer than 10 ships of types 1-9 remain in the dimension: the wave is cancelled if fewer than 10 ships of types 1-9 remain in the dimension (the stock is on no screen; [MEASURE] it as UFOs seen plus weekly growth minus kills), else expect up to 20 ships against random buildings, bases included, within one invasion interval.
- **Effect.** Not reachable before the 6th raid. **Order.** Endgame bundle, after `VC-1`, `TD12`, `T4/F14`, `BD02`. **Blocked on RE** for targets and decrement.

#### C3. `BD09` - inclusive hostile predicate for raids and detection (with `F13-thresholds`, `F-M15`, `G13`)

`BD09` medium, others low. [EXE] AI predicates (`FUN_00091db0`: raids, detection halving, park refill, loot gate) use hostile **<= -50** and allied (`FUN_00091de4`) **>= 75**; the weekly funding cutoff is strict **< -50**. Engine: `isRelatedTo` (`organisation.cpp:1085-1095`) uses a float `x < -50` for everything. **Fix:** a separate inclusive predicate for `setRaidMissions`, the `Building::detect` halving and the park refill; funding stays strict; `isRelatedTo` unchanged (P2); integer relations from A3 (c) remove the float-against-byte point. **Test:** relation exactly -50: the raid pair passes and detection halves, funding does not cut. One relation point wide; test-sized. **Strategy:** keep sponsors above -50 for raids and detection, the Government at -50 or above for funding.

#### C4. `F11` - per-kill relation constants

`F11-per-kill-rule` high, diverges, survived. [EXE] TACP `FUN_00060720`, `S` = victim score byte (humans 10): same org: stop; victim's view of the killer `-3S` (not hostile) or `-S/2` (hostile); orgs allied (>= 75) to the victim `-S/2`; orgs hostile to the victim (<= -50) `+S/4`. Engine `battleunit.cpp:5075-5101`: -30 / -5, -5, +2, equal only for `S` = 10. **Fix:** `S = agent->type->score` with inclusive tests from C3 (whether `byte[unit+0x44]` is the type score for every type is an RE check, section 7 row 16). **Test:** kills of Anthropod (S 10), Skeletoid (14), Psimorph or Megaspawn (30), Queenspawn (50) raise Government to X-COM by 2, 3, 7, 12 when the Government is hostile to the Aliens [DERIVED floor(S/4)] (the Government starts at about -50 +- 10 toward the Aliens: hostile in about 52% of original seeds, 48% of engine seeds). Test-sized; lands with `F11b`, after C3.

#### C5. `SCORE-C2/C` - detection formula parity (with `T12`)

The formula half of `SCORE-C2-TRIGGER` / `BD07` / `F11_building_detection_probability` (medium) and `T12` (low). [EXE] species by reservoir sampling over those present (`rand16(100) < 100/k`: the first present species passes 100/101); `value = sum x (weight - 2d)/100`; halve **once** if the owner's relation to the Aliens >= 75 or to X-COM <= -50, again if taken over; incident iff a species was chosen and `rand16(100) < value - 10` (inclusive); cadence stays (within 3%). Engine: gate `100/(count+1)`, exclusive rolls, up to three independent halvings (/8 against /4 for a taken-over org). **Fix** in the non-forced branch of `Building::detect`. **Test:** 12 Anthropods in a Corporate HQ 32.3% a visit (engine 2.3%); 3 in the Senate 2.9% (0.75%); a taken-over hostile owner gets /4. Mixed sign: 4-13 times fewer incidents on big single-species stacks, about 10% fewer on typical crews, but later warning. Test-sized.

#### C6. `F7` - the withdrawal threshold comes from the package slot (with `F8`)

`F7_withdrawal_threshold_source` medium, engine_bug, survived; `F8` matches. [EXE] threshold `constitution x type_percent[slot]/100` (10-50, `FUN_0006da88`); a UFO in the band `[crash_constitution, threshold)` (inclusive lower bound, checked about every half game second) turns for the nearest gate, pays the incursion penalty and rejoins the pool; escorts following a companion and role-9 ships never withdraw. Engine: withdrawal exists (commit `34adbf58`; `vehicle.cpp` HEAD `:2167-2182`, LIVE `:2192-2207`) with the same charge, but the threshold is the **role** table `UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE` (`gamestate.cpp` HEAD `:1021, :1089, :1142`; only 10 and 30 seen in 16 saves); `incursionTypeThreshold` (`vehiclemission.cpp` LIVE `:3564`) is the original's formula and only a test calls it. Over-withdraws Infiltration Assault/Battleship, under-withdraws Subversion, Transporter, Fast Attack, Destroyer. **Fix:** `withdrawPercent = slot.typePercent` in the three launch loops via `incursionTypeThreshold`; role table for role 9/10 only; fix the `clampIncursionScatter` misreading. **Test:** per hull the band is non-empty iff `type->health x typePercent/100 > crash_health`; the role-table tests (`test_ufoincursion.cpp` :32, :52) stay; `test_ufo_withdraw_band` (`test_city_rules.cpp:809`) claims the role table drives incursions: fix the comment, assert the source. **Strategy** (`03` 4): WHEN a UFO is in its band THEN it flees in about half a second unless killed: finish it at once (an escape costs `floor(score/4)`).

#### C7. `TD17+SCORE-C0` - tactical score bookkeeping (with `TD18`)

`TD17` medium, `SCORE-C0`, `TD18` low; adjudicated. [EXE] Total = combat rating + value of incapacitated non-alien hostiles - casualties + leadership + value of incapacitated Aliens credited **regardless of storage** + captured - lost equipment; leadership 0 if `30 x casualtyPoints > 1000` (4 dead agents), else `base x (100 - 3C)/100`; civilian kills cost their value (`resolved-facts.md` 9.4, 11.1). Engine: leadership `(100 + 3 x casualtyPenalty) x (combat + friendlyFire + liveAlienCaptured)/100`, unclamped (negative from 4 dead); no civilian penalty; live aliens only with storage (`battle.cpp:2886-2905`); equipment charged at destruction (`battleitem.cpp:34`). **Fix:** clamp leadership to 0 at 34 casualty points; positive-credit base; credit stunned, egg and chrysalis aliens with or without storage (`else score.combatRating += ...`); `-v` for non-hostile non-own kills; brainsuck `-2v` plus casualty `-v`; end-state equipment scoring once the item table is decoded. **Test:** 4 dead gives leadership 0, 3 dead 91% of base (order: credit, then leadership base). Mixed sign: more score, but the alien equipment thresholds (1,000 / 2,200 / 4,000 / 6,600) arrive sooner. Test-sized.

#### C8. `F17` and `CD`

`F17-on-demand-bribe` high, exe_unbound in the register, now partly bound (`FUN_00097234`, file `0xF98D8`): refused if taken over, allied to the Aliens or already allied; the `+5000 + 3000 d` term is confirmed; the first term is not compared. The engine's `BribeScreen` exists (`cityview.cpp:1805-1819`); no change until the first term is compared (section 7 row 12). `V-AG-14` medium, adjudicated: the original logs encounters to `EXPERIEN.DAT`, refits `BRAIN.DAT` each debrief and plans from the model; the engine has heuristic modes only. **Do not port**; correct `campaign-plan.md` (section 8); do not assume alien tactics are static across a campaign.

### 5.1 Match findings that still carry an action

`F-M6` (regression test, B6), `F14-no-periodic-drift` and `SCORE-C5` (documents, section 8), `V-AG-05` (do not remove the starter assignment, B14); `TD7`, `F15a`, `BD12`, `BD17`: none.

### 5.2 Low-impact findings (every non-match finding of impact low, once)

Direction: H engine harsher, S softer, M mixed or neutral, U unbound. "Handled in" gives the block, "table" means no code, "S7 row n" the RE backlog.

| ID | Dir | Handled in; note and action |
|---|---|---|
| `BD05` | H | table. Drop the 5% post-landing exposure roll (`vehiclemission.cpp:2637-2642`) with `BD03`. |
| `BD11` | S | B3 `BD08` |
| `BD15` | M | table. None; the original picks uniformly within D <= 60, the engine within Manhattan 30 (`vehiclemission.cpp:3275-3295`). |
| `BD18` | S | B2 `BD04+TD10+TD8+TD9` |
| `F-M11` | S | table. None (adjudicated): the original's -60 is also wiped by the mirror; persistence would add behaviour it lacks. |
| `F-M15` | M | C3 `BD09` |
| `F-M4` | S | B6 `F-M1+F-M2+F-M3` |
| `F-M8` | U | S7 row 16 |
| `F01-primitive-ripple` | H | A3 `F02+F05` |
| `F03-third-party-plus5` | M | table. None: the +5 to a third-party firer is irrelevant to X-COM play. |
| `F07-vehicle-scenery-obstruction` | M | table. None: the original's writer fires only for the Overspawn; the engine has no Overspawn actor. |
| `F08-empty-building-investigation` | S | table. Pass the ripple flag at `buildingscreen.cpp:186` (HEAD `:247`); fix stale comments with `SCORE-C2/A`. |
| `F09b-kind4-org-initiated` | S | table. None now: an org attack should persist `M[org][X-COM] <= -60`; add with `BD08` if wanted. |
| `F10_ground_item_gates_tactical_score` | U | S7 row 13 |
| `F12-friendly-fire` | H | A4 `F11b` |
| `F12_alien_movement_between_buildings` | H | A9 `BD06` |
| `F13-thresholds` | M | C3 `BD09` |
| `F20-attacked-by-nonhostile-row0` | S | table. None: X-COM's row set to -50 when attacked by a non-hostile organisation. |
| `F21-food-chamber-mutant-alliance` | H | AL3 `T4/F14` |
| `F22-stun-relation` | U | S7 row 16 |
| `F2_ufo_incursion_package_fit` | M | table. Independent per-slot fit test in `invasion()` (`gamestate.cpp:957-970`); guard the empty list. |
| `G10-construction-countdown-cadence` | U | S7 row 16 |
| `G11-park-refill-cadence` | M | table. None (daily against hourly refill). |
| `G12-market-passes-in-funding` | S | B6 `F-M1+F-M2+F-M3` |
| `G13-relation-float-vs-byte` | M | C3 `BD09` |
| `G14-agent-pay-byte` | U | S7 row 16 |
| `G6-government-income-model` | M | table. None: the Government cap does not bind at Superhuman. |
| `G7-nongov-org-income-compounding` | M | table. None: other organisations' balances inflate 2-3x by day 28; ignore. |
| `RR-02` | S | table. Gate lab staff on full health (`research.cpp:415-420`). |
| `RR-06` | M | table. None; measure the staffed workshop rate (`labRate`). |
| `RR-08` | S | table. Cap repair at 12 per bay per hour per vehicle. |
| `RR-18` | U | S7 row 16 |
| `SCORE-C0` | M | C7 `TD17+SCORE-C0` |
| `SCORE-C1` | H | A7 `SCORE-C1` |
| `SCORE-C4` | S | table. Charge a craft lost with no attacker (hook the destroy and crash paths). |
| `SCORE-EQLEVEL` | H | table. Push all 10 rows in `extract_agent_equipment.cpp:1073-1105` (data arm); inert below tactical score 1,000. |
| `SCORE-EQPENALTY` | U | S7 rows 6, 13 |
| `SCORE-UNLOCK-CRAFT` | M | table. Optional daily latch (nine saved bytes) in `updateEndOfDay`; inert at today's horizon. |
| `T12` | S | C5 `SCORE-C2/C` |
| `T13` | M | table. Freeze taken-over and Cult views of the Aliens, with `F02+F05` (c). |
| `T15` | M | table. None: takeover toast against a modal screen; the harness reads relations. |
| `TD13` | H | AL1 `TD12` |
| `TD14` | U | S7 rows 6, 13 |
| `TD18` | M | C7 `TD17+SCORE-C0` |
| `TD19` | S | table. Skip the nearby-vehicle pool for `UfoRecovery` (`battle.cpp:3982`), or set `AllowNearbyVehicleLootPickup=0`. |
| `TD5` | U | S7 rows 6, 13 |
| `TD6` | M | table. None (adjudicated): the weekly tech-level model is the original's own. |
| `V-AG-03` | S | B14 `V-AG-01` |
| `V-AG-05` | S | B14 `V-AG-01` |
| `V-AG-11` | S | table. Halve the speed gain (`battleunit.cpp:4459-4462`). |
| `V-AG-12` | S | table. None (bravery slightly easier). |
| `V-AG-15` | U | S7 row 16 |
| `V-AG-16` | M | table. None (rank thresholds). |
| `VC-11` | H | AL3 `T4/F14` |
| `VC-12` | H | table. Auto-route in `ArriveFromDimensionGate`; the harness already issues the order. |
| `VC-16` | S | table. Heal alien vehicles on the city switch (`setCurrentCity`); keep wound-and-return strategies off. |
| `VC-7` | M | table. None (harmless superset). |
| `VC-8` | M | table. Set `OpenApoc.NewFeature.CrashingDimensionGate=false` for benchmark runs. |

Count: 58 low-impact non-matching findings.


---

## 6. Sequencing

**Summary.** One branch from the committed benchmark lineage; a registry commit first; Group A one fix at a time with a gate between fixes; one bundled measurement at the end of A-now; the AI work is measured on that build; then Group B, C and the endgame bundle. The group order is the default, not a law: a strategy file that needs a later fix earlier names it in its engine profile and the fix is pulled forward. 6.0 reconciles the Gate A of the harness AI plan (05) with the groups.

### 6.0 Gate A of the harness AI plan

**Gate A = GA after step 6: A0 plus A-now.** This is the frozen, partial-fidelity AI-measurement baseline of 05 §1.2 and 06 §2.6. It is not original-game certification. Earlier F0 diagnostics are tagged `pre_gate_a`. Steps 7–9 subsequently add `TD1+TD2+TD3`, `T8`, `BD08`, `BD04+TD10+TD8+TD9` and `G8`; each changes the fidelity vector and requires M10 re-baselining, including new garrison and battle-policy measurements. Do not call the post-step-9 vector Gate A or pool it with GA.

### 6.1 Which tree the fixes reach, and how

| Question | Answer |
|---|---|
| Where is each fix developed? | Branch `khallmark/engine-fidelity` created from `khallmark/develop-reboot` (at or after `bce89743`; check `git merge-base --is-ancestor 879d0d73 HEAD`). Not on `khallmark/strategy-engine-plan`: it lacks the harness and the `879d0d73` hunks. |
| How does a fix reach the benchmarked binary? | One commit per fix on that branch; the benchmark builds the committed state; `build_id.engine_sha256` and `build_id.fidelity` identify the binary (`06-benchmark.md` 2.1). Earlier runs came from a moving uncommitted tree and are not attributable (`failure_forensics.md` section 0). |
| Who must be asked? | The author of `879d0d73` owns the funding region of `gamestate.cpp`: `G4` and `G8` go to that owner as patch plus test, or land by agreement. The harness work (05, `06-benchmark.md`) hooks into `gamestateintrospect.cpp`, `cityview.cpp`, `alertscreen.cpp`, `buildingscreen.cpp`, `weeklyfundingscreen.cpp`, `scorescreen.cpp`: only `alertscreen.cpp` (A6) and `cityview.cpp` (A7, B10) overlap fix files. |
| Uncommitted LIVE edits again? | Work in a separate `git worktree`; hand the owner a patch. At 07:39Z there were none. |

### 6.2 Collision rules

1. One fix, one commit, one toggle; stage by explicit path, never `git add -A`; no bare `git stash` (shared stack): use a WIP commit.
2. Re-resolve snapshot line numbers by symbol (1.5); A8 (the deposit in `vehiclemission.cpp`) and AL4 rebase first. Fixes in clean files (`T10`, `F-M7`, `F15b`, `V-AG-01`, `T8`, `VC-2+VC-3+RR-14`, the code halves of `BD08` and `RR-09+RR-10+F-M12`) land in any order.
3. `framework/options.cpp` (option block) and `tests/CMakeLists.txt` (fenced test block) are their own commits, edited last.

### 6.3 The landing list and its gates

| Step | Fix | Gate before the next step |
|---|---|---|
| 0 | Registry: branch, `fidelity.cpp`, option block, `gs fidelity`, `CampaignStats` skeleton | A/A of 16 runs with every flag off reproduces the baseline within noise; a run without a fidelity record is rejected; `start_fingerprint` shows 180,000 / 10,000. |
| 1 | A0 `BD01b`, `G2/D1-D7` | Present; tests strengthened to assert order. |
| 2 | `G4` | `new_game_funding` red/green; first CityView 100,000 / 80,000; Monday-1 income 80,000 in every run of the arm. |
| 3 | `T1+T2` | `takeover_roll`, `takeover_cadence_order` red/green; takeover day and survival curve under the sequential design of `06-benchmark.md` 3.6; no takeover while displayed infiltration is at most 50%. |
| 4 | `F02+F05` (a)(b), `F11b`, then (c) | Red/green; `relationDelta` shows stray fire at 0 / -5 and a Government craft at -10. |
| 5 | `SCORE-C6`, `SCORE-C2/A`, `SCORE-C1` | Red/green; `damageByFirer.vehicleFall` is 0; no 7-hour incident pattern; weekly score components compared. |
| 6 | `BD03`, `BD14a+BD16`, `BD06` | Red/green; `baseDefences` shows crew-table attackers and re-homing; two-base arm. |
| **GA** | End of A-now | Bundle comparison (all A-now flags on against the step-1 build), on the survival endpoint of `06-benchmark.md` 3.6 (sequential, up to 64 runs per arm). Freeze it as the AI-measurement baseline. |
| 7 | `TD1+TD2+TD3` with `T8` (one data regeneration) | Red/green; stamp; casualties per mission and day-1 crews before and after. |
| 8 | `BD08` with `BD04+TD10+TD8+TD9` | Measured together (`baseDefences{trigger, outcome}`); garrison rules of `03` re-fitted (the re-baseline milestone of the harness AI plan). |
| 9 | `G8` | After `G4` and GA; defeat on the fifth Monday after a cut in the scripted scenario. Re-baseline with M10; Gate A was reached after step 6. |
| 10 | `RR-12+RR-13` with `VC-2+VC-3+RR-14`, `T10` | One data regeneration; stamp; chain-order test; `ALIEN_BUILDING_0` not startable on day 0. |
| 11 | `RR-09+RR-10+F-M12`, `F-M1+F-M2+F-M3`, `F-M7` | Red/green; `hirePool` mean 1.5; price cut in `marketCuts` after a pacing breach. |
| 12 | `F15b`, `V-AG-01`, `BD13`, `F9` | Red/green. |
| 13 | `F6` | Only after the engine's 3.5 a week is confirmed and the original's double drain observed (section 7). |
| 14 | `BD09`, `F11`, `SCORE-C2/C`, `F7`, `TD17+SCORE-C0` | Test-sized; batch in any order. |
| 15 | Endgame bundle: `TD12`, `VC-1` (with objective sets), `T4/F14`, `BD02`, then `UFO_GROWTH_STOP+VC-10` | A scripted whole-chain test on a scenario save: first crossing, The Alien Dimension, Sleeping raid, ..., Food Chamber raid frees a taken-over Mutant Alliance, Control Chamber raid stops growth and arms the wave, Gate raid won by destroying the generators and evacuating (chain 4, 2, 6, 8, 0, 7, 1, 5, 3, 9). Before the first benchmark that reaches the dimension. |
| - | Blocked: `BD14b`, unbound parts of B2 and C2, `F17` | Section 7. |

### 6.4 Dependencies

`G8` after `G4`; B2 after B1 and A8; `F-M2` with `F-M3` and `UFO_GROWTH_STOP` with `VC-10` in one commit each; `T4/F14` after `VC-1` and `T1+T2`; `VC-1` after `TD12`; `F02+F05` (c) after (a), (b); `F11` after `BD09` and `F11b`; `BD13` after `BD14a+BD16`. The ledger row of each step (key, commit, vector label, CTest names, arms, run counts, evidence) is `FIDELITY_FIXES` plus `bench/vectors.json`.


---

## 7. RE backlog

**Summary.** Items the validation could not bind (`exe_unbound`) or that stop a fix from being finished (`blocked_on_re`), ordered by what each would change in the strategy, with the decisive next lead from the validators (`validation_register.md`, "Validator RE leads"). Offsets are physical file offsets unless a VA is given (code = VA + `0x626A4`, data = VA + `0x616A4`). Closed, do not chase: `RR-11`, `T16`, the bribe, the Food Chamber reversal. The generic liberation `FUN_0007abf0` is dead code by the `T4` ruling; row 12 only asks that a computed-jump caller be excluded.

| # | Item | Decides | Blocks | Decisive next lead |
|---|---|---|---|---|
| 1 | Post-raid `worth` debit and tiered relation hit (`F-M13`, `F10-postbattle-worth-tier`); unit of the repair bill (`BD14b`) | Whether raiding a sponsor drains its funds and raises hostility beyond the kills (the Cult pillar assumes neither); the cost of a won defence | `BD14b` | Who repopulates the mission descriptor at VA `0x244020`-`0x2440B0` across the UFO2P / TACP boundary: `CALL 0xBFA7A` / `0xBF24C` sites with non-literal destination; readers of `CANPLAY/CANUSE/CANUNIT.DAT` (file `0x131DC0`-`0x131DD8`); TACP `FUN_00015494` (file `0x6FF38`, block at VA `0x1ED294`) against `FUN_000B1DC0` -> `FUN_000A4054` -> `FUN_000B21A8` / `FUN_000B27B4`; the arguments of the two `FUN_0005faf0` calls in `FUN_000b32ac`. |
| 2 | Loot recovery and cargo capacity (`F-M14`) | Whether the raid craft needs a Cargo Module (the engine discards all loot otherwise, `battle.cpp:3259-3270`); treated as required | none | TACP end-of-mission item recovery: the `savegame.dat` string hit at VA `0xE30E5`, `FUN_00011A90`, `FUN_00014008`. |
| 3 | Control Chamber wave: targets, stock decrement, dimension stock (`VC-10`, `F5`, `VC-16`) | Size and aim of the 6th-raid wave; whether shooting UFOs in the dimension thins the stock | `UFO_GROWTH_STOP+VC-10` (part) | `FUN_0006d994`; mass-mode bytes of `FUN_0006da88` (`+0x16E` = 0x32, `+0x170` = 2, `+0x171` = 0x14), `FUN_00070a2c`, `FUN_00070acc`; whether `FUN_00058280` or another removal path decrements VA `0x143D40` / `0x143A6C`. |
| 4 | P(base given Raid/Storm): 1/2 or 1 (`BD08`) | Garrison frequency estimate | none | The owner remap at VA `0x1017C` against loader `FUN_00014854`. |
| 5 | Incursion double drain (`F6`) | 3.5 or 7 incursions a week; first UFO at 24-72 h or 5-6 h | `F6` | `FUN_0004AB04` runs 288 times a game day; an observation of the original; engine counter `incursionsLaunched`. |
| 6 | Reinforcement intervals (`TD14`) | Pacing of dimension raids (the fork's 5-40 s are folklore) | none | The UFO2P writer of the tactical header block (TACP VA `0x1ED294`; difficulty `[0x1ED340]`, interval `[0x1ED344]`): `FUN_00016c67`, `FUN_000b3ee0`, `FUN_000b0380`, `FUN_000ac304`. |
| 7 | Turret shot counts, station slot exhaustion (`TD9`, `TD11`, `BD20`); base-defence capture storage (`TD20`) | Value of a station; whether stations beyond 3 add turrets when the 60 slots run out; whether a base needs Alien Containment | `BD04+TD10+TD8+TD9` (weapons) | `FUN_00059f54` slot argument (2 for the pods); 37BASE turret creation against the roster count; `FUN_000b44a4`, the virtual carrier `[0x1304CC]`. |
| 8 | Dimension timing and clock (`VC-13`, `RR-17`) | Sortie cadence; whether dimension days advance the human calendar | none | `FUN_00010908` (file `0x72FAC`, main loop `FUN_000105b8`), `FUN_000114cc`, `DAT_000e1320`, `FUN_0007ee84`, `FUN_0006e098`. |
| 9 | Success flag on a retreat; new home base choice (`BD14a`, `RR-13`, `BD16`) | Whether a retreat after the objective counts as success in a UFO recovery; which base takes the people | `BD14a+BD16` (choice), `RR-12+RR-13` | TACP writer of `[0x244084]` (`FUN_000aff9c`, file `0x11293A`); `FUN_000b3270`. |
| 10 | Guard composition tables, equipment-set mapping (`T10`, `TD6`) | Exact strength of guards of taken-over organisations | none (+3 is exact) | `FUN_000b46c4` tables (VA `0x13ACFE`, `0x13AFCC`, `0x13B1AA`); set ids 13..18 and 8..12 onto the fork's sets; `FUN_00024e5c` against `FUN_000250f8`. |
| 11 | Accuracy direction (`TD3`) | Sign of the accuracy scaling | none | The hit-chance function using accuracy byte `+0x8E` (`FUN_00055400` computes `100 - byte`; firing code undecoded). |
| 12 | Bribe price first term; any takeover reversal (`F17`, `F16c`) | Whether money can buy back a relation; whether a taken-over Government can be freed | none | `FUN_00097234` (file `0xF98D8`) against `costOfBribeBy`; `FUN_0007abf0` / `FUN_0007acfc` (files `0xDD294`, `0xDD3A0`): computed jumps, the 180-record table at VA `0x144918`. |
| 13 | Alien tier handicap, ground-item gates (`TD5`, `SCORE-EQPENALTY`, `F10_ground_item_gates_tactical_score`) | Alien equipment beyond the score thresholds; items 42/50/51/52 | none | The 40 x 14-byte per-city table at VA `0x143A7E` (`FUN_000ad38c`, `FUN_0006e608`, `FUN_0006ea78`); `FUN_000ad148` (table file `0x1568EC`). |
| 14 | Attack incursion target choice (`F13_attack_incursion_target_choice`) | Whether Attack packages prefer bases | none | `FUN_00070acc`, `FUN_0005d288`. |
| 15 | XP counters, AI features (`V-AG-13`, `V-AG-14`) | Whether target practice farms accuracy XP; how alien tactics change over a campaign | none | `FUN_0005D25C` (`0x5DD90`-`0x5DE55`), `FUN_0006871C`, `FUN_000655D0`; `FUN_0009a77c`, `FUN_000988e0`, `FUN_000999e8`. |
| 16 | Smaller: `G14` (writers of agent `+0x57`), `G10` (`FUN_0006bc70`), `RR-18` (`manufacturing_data +0x0E`, `FUN_000aa5e4`), `F-M8` (`FUN_00090d68`), `F22` (TACP `FUN_0005F860`), `V-AG-15`, `BD07` (`FUN_000240a4`, `FUN_000a12f4`), `F11` (is `byte[unit+0x44]` the type score?) | Minor | none | as listed |
| 17 | Data work, not RE: intact-value baseline of every base building for all five maps (`BD13`) | The 90% rule | `BD13` | Derive offline from the ISO's `CITYMAP.DAT`, `CITYMAPn`, `CITYMAPn.BLD` (verified for maps 4 and 5). |

### 7.1 Measurements that replace assumptions

The [MEASURE] items of `resolved-facts.md` 13 are closed by the counters of 2.7: incursions per week (`incursionsLaunched`, 3.5 expected for a single drain), staffed workshop rate (`labRate` with 5 and 10 staffed engineers; `01` assumes 400 man-hours an hour), loot spots per Temple raid (`lootSpots`), Government survival after `T1+T2` (`takeovers`, the sequential design of `06-benchmark.md` 3.6), the stray-fire share (`relationDelta`) and the crash share of city damage (`cityDamage{vehiclePlow}`); the dimension fleet stock is read as UFOs seen plus weekly growth minus kills.


---

## 8. Doc corrections

**Summary.** Statements in documents, comments and tests that the validation shows to be wrong, with the correction and the supporting finding. Lines are this branch's (HEAD, `fda36d6c`); `879d0d73` on `develop-reboot` also edited `campaign-plan.md`, `playing-the-game.md` and the gap matrix, so re-resolve by heading there and coordinate with the owner. This plan makes none of these edits.

### 8.1 `docs/campaign-plan.md`

| Lines | Wrong claim | Correction | Finding |
|---|---|---|---|
| 133, 271-280, 1159 | Takeover is "probabilistic, not a threshold": a roll every 125 minutes, `rand(0,200) < infiltrationValue`; "no safe band". | One org per 5 game minutes, each tested every 300; takeover iff `rand16(0..100) < v - 100`: nothing can happen until the displayed value exceeds 50%. The 125-minute rule is the engine's divergence. | `T1`, `T2` |
| 482-507 (3.7), 871-873, 947-949, 1174, 1220 | "Destroy the Organic Factory and UFO reinforcement stops"; tables label the Organic Factory "raid #6", the Control Chamber "raid #4", mixing function index and raid order. | The Control Chamber (function 7, the **6th** raid) stops growth; the Organic Factory (function 5, the 8th) does not; it also arms a wave of up to 20 ships. Raid order 4, 2, 6, 8, 0, 7, 1, 5, 3, 9; label both index spaces. | `UFO_GROWTH_STOP`, `VC-10` |
| 819-823 | "Security Stations are inert"; cap advice. | A finished station reserves 4 roster slots (up to 12) and adds 4 turrets (580 HP, armour 57); no cap of 3. | `TD7`, `TD10`, `BD04` |
| 1014 | "Sell at most one X-COM-manufactured craft per week." | Craft sales add no market stock; the pacing rule is for week-0 goods and alien loot (about 0.075 x max a week). | `F-M6`, `F-M2` |
| 1046 vs table near 1215 | "The tiering is inert ... a missing `break`" and, later, "tightest band wins". | The most extreme tier wins, strict and sequential: W above 400 +F/20, 800 +F/16, 1600 +F/12, 3200 +F/8, 6400 +F/5, 12800 +F/4; below 0 -F/15, -400 -F/10, -800 -F/5, -1600 -F/4. | `G1` |
| 1177, 1229, 1240-1262 | "The AI does not learn"; `EXPERIEN.DAT` is only the soldier award; "FIXME: Ensure correct" and the `battleunit.cpp#L4171` link. | It logs encounters to `EXPERIEN.DAT`, refits `BRAIN.DAT` each debrief and plans from the model; the soldier award is `FUN_000b3f74`; the FIXME is not in the code. Do not port. | `V-AG-14` |
| 165, 1193, 1232 | Bribe and rift formulas unbound. | Rift trigger and price match (`FUN_00096bc8`); the bribe is `FUN_00097234`; four recovery paths. | `F15a`, `F17`, `F14-no-periodic-drift` |

### 8.2 `docs/playing-the-game.md`

| Lines | Wrong claim | Correction | Finding |
|---|---|---|---|
| 74-79, 121-125 | Victory is a won `RaidAliens` battle on the building with `victory=true`, "gated behind `RESEARCH_ALIEN_BUILDING_9`"; "for `i` in 0..9 research building `i`, raid building `i`"; `RESEARCH_UNLOCK_*` "gate nothing". | Topics bind by function index, unlocked by the chain 4, 2, 6, 8, 0, 7, 1, 5, 3, 9; a raid succeeds when the objective tiles are destroyed, not when every alien is dead; after `C1` the unlock topics are the chain's gates. | `VC-1`, `VC-2`, `VC-3` |
| 284-285 | "The alien AI does not learn; destroying the Organic Factory stops UFO growth." | See 8.1. | `V-AG-14`, `UFO_GROWTH_STOP` |
| 310-312 vs 328 | "Security Stations do not work ... inert" and, 16 lines later, "Security Stations fire on their own". | They fire: 4 turrets and 4 roster slots each. | `TD7`, `TD10` |
| 334-335 | "The Cult is permanently hostile, so raiding them costs no diplomacy." | The launch forces -60 transiently (wiped by the next mirror); kills cost relation (hostile guard -S/2, not hostile -3S). | `F-M11`, `F11` |
| 401-406 | Funding latches when "lifetime `totalScore` drops below -2400" (`gamestate.cpp:1668-1680`); runs died "by the relation route ... self-inflicted". | The test is the sum of completed previous weeks only, one Monday late, or Government to X-COM strictly below -50; HEAD's test included the current week (`D1`); 42 of 50 relation drops were takeovers. | `G2`, `failure_forensics.md` 3 |

### 8.3 Gap matrix, findings, index, comments

| Place | Wrong or stale | Correction |
|---|---|---|
| `openapoc-gap-matrix.md` 27, 85 | Growth stops with the Organic Factory. | Control Chamber. |
| `openapoc-gap-matrix.md` 36 | "Picks the tightest band." | Most extreme tier (`G1`). |
| `openapoc-gap-matrix.md` 38; `next-implementation.md` item 7 (line 27) | "Bound xrefs to VA `0x128D64` are empty"; pattern consumer "not a recovered `FUN_*`". | Never validly examined; the consumer is `FUN_0006cfb4` (file `0xCF658`), growth `FUN_0006edac`. |
| `openapoc-gap-matrix.md` 94 | City-wide attack after the Control Chamber: "copy only; no recovered schedule". | Bound: flag `[0xF3C78]`, `FUN_0006cfb4` -> `FUN_00070938`. |
| `openapoc-gap-matrix.md` 15, 17 | Bribe/rift "still unbound"; loot table `0x192184` "28 x 60". | Rift price matches, bribe bound; table at `0x192004`, 27 rows x 60 B. |
| `next-implementation.md` item 8 (line 29) | "`test_organisation` locks hourly takeover behaviour"; takeover is hourly `FUN_0007fcc0`. | The tests lock clamp, decay and divisor only; takeover is `FUN_0006fd9c` (file `0xD2440`); hourly growth is `FUN_0006fcc0` (file `0xD2364`). |
| `findings/C1-C4-no-evidence-items.md` 9, 74-88 | C4 city-wide attack "confirmed absent; close the row". | Present (`VC-10`). |
| `findings/C2-secondary-objectives.md` 13, 100-123 | Food Chamber to Mutant Alliance "NOT BOUND". | Bound in `FUN_000b32ac` (clear site VA `0xB3359`, infiltration[24] := 0 at `0xB33A0`). |
| `findings/O2-cargo-seize-pass2.md` 4-16 | `[0x244084]` always 0; branch dead; `worth` always 0. | The dword is `+0x98` of a 0xD4-byte scenario record loaded from `scenario/scenarir.dat` by `FUN_000b1d9c`; the Food Chamber branch is live; `worth` stays unbound. |
| `research/exe_evidence_index.md` (archived with the corpus) | `FUN_00057c8c` (file `0xBA330`) as an "infiltration detection relation hit"; D9, D10, D11, D15, D18 open; "`FUN_000941dc` is org budgets, not the market"; withdrawal "absent"; hourly routine `FUN_0007fcc0 @ VA 0x7FCC0`. | `FUN_00057c8c` is vehicle damage with the downed-vehicle relation call at VA `0x5826F`. D9 decoded, D10 bound (`FUN_0006fd9c`), D11 implemented (`34adbf58`), D15 bound, D18 resolved (`FUN_000b94c0`); `FUN_000941dc` holds the market passes; the hourly routine is `FUN_0006fcc0`; add the vehicle plow-through to "not correct". |
| `city-damage-scoring.md` 53 (`develop-reboot`); `fda36d6c` message and `vehicle.cpp` comment | Falling-vehicle plow-through charges regardless of owner (`FUN_00041644`, VA `0x41B99`). | Only the Overspawn (movement type 33) reaches that writer (`SCORE-C6`); the commit cannot be amended: record it in the fix commit. |
| `organisation.h:14-18`, `organisation.cpp:631` (with `T1+T2`); `alertscreen.cpp:40-44, 60-62, 84`, `gamestateintrospect.cpp` ~978, `buildingscreen.cpp:154-166`, `ufogrowth.h:21-23` (with `SCORE-C2/A`, `UFO_GROWTH_STOP`) | "25 orgs x 5 minutes = 125 minutes per check"; `FUN_0007fcc0 @ VA 0x7FCC0`; a crew arriving in a base starts a defence; `-5 - difficulty` applies to alert-arrival investigations; the growth gate cites UFOpaedia text. | 60 slots (25 act): 300 minutes; the hourly function is `FUN_0006fcc0`; neither defence claim holds (`BD01a`, `F08`); cite `FUN_0006edac`, `FUN_000b4538` case 7. |

### 8.4 Tests that pin wrong behaviour or leave it unguarded

| Test | Place | Pins | Fix |
|---|---|---|---|
| `organic_factory_gates_ufo_growth` | `test_city_rules.cpp:1756` (reg. :3105) | Killing the Organic Factory stops growth | `C2` |
| UFO growth wiring | `test_gamestate_ufo_growth_wiring.cpp:86, 94` | Looks up the Organic Factory | `C2` |
| `alien_building4_keeps_table_prereq` | `test_city_rules.cpp:1278` | A dependency on `RESEARCH_UNLOCK_ALIEN_BUILDING_4` | `C1` (invert) |
| `alien_building_exe_rows` | `test_city_rules.cpp:1307` | Topic rows (correct); passes while the binding is wrong | `C1` (add binding) |
| `unmanned_ufo_loot` | `test_city_rules.cpp:2179` | Unmanned recovery hands over loot | `B10` |
| `vehicle_downed_relation_adjustment` | `test_org_relations.cpp:353` | -30, float ripple -11.1 / -7.5 / +7.5 | `A3` (rewrite) |
| `xcom_stock_bands` and four siblings | `test_economy.cpp:23, 40, 52, 96, 139` | 80% / 66% law, `lastStock`, the `xcom` parameter | `B6` (rewrite) |
| `ufo_withdraw_band` | `test_city_rules.cpp:809` | Numbers right; comment says the role table drives incursions | `C6` |
| `who_is_charged` | `test_city_damage_score.cpp:16` | Calls `chargeCityDamage` directly; vehicle path untested | `A5` |
| `base_die_with_occupants` | `test_base_die.cpp:54` | Only that `die` does not crash | `A10` (add re-homing) |
| `sign_treaty` | `test_diplomacy.cpp:65` | Negative to 0, alliance to 100 only | `B13` |
| (none) | `test_organisation.cpp` | No test calls `takeOver` / `updateTakeOver` | `A2` |
| `mirror_fires_during_battle` | `test_org_relations.cpp:188` | Mid-battle mirror; changes only if the optional `F-M11` fix is taken | `F-M11` |

---

## Appendix A. Classification of the critical, high and medium findings

Every non-matching finding (final, adjudicated verdict) of impact critical, high or medium, once; the verdict is in the block; low-impact findings are in 5.2. Group = the letters of Where (A0, A, AL = A-late, B, C; `S7 row n` = RE backlog, CD = documents only). Dir: H harsher, S softer, M mixed or neutral, U unbound.

| ID | Imp | Dir | Where |
|---|---|---|---|
| `BD01b` | crit | H | A0.1 |
| `F02-scenery-hit-intent-gating` | crit | H | A3 |
| `F16b-takeover-trigger` | crit | H | A2 |
| `G4-newgame-funding-pass` | crit | H | A1 |
| `T1` | crit | H | A2 |
| `TD10` | crit | S | B2 |
| `TD2` | crit | S | B1 |
| `VC-10` | crit | M | C2 |
| `BD03` | high | H | A8 |
| `BD06` | high | H | A9 |
| `BD14a` | high | H | A10 |
| `F05-vehicle-downed` | high | H | A3 |
| `F06-ufo-kill-ripple` | high | H | A3 |
| `T2` | high | H | A2 |
| `VC-1` | high | H | AL2 |
| `BD08` | high | S | B3 |
| `BD10` | high | S | B3 |
| `BD13` | high | S | B15 |
| `F-M1` | high | S | B6 |
| `F-M2` | high | S | B6 |
| `F15b-rift-resolution` | high | S | B13 |
| `F6_invasion_timer_cadence` | high | S | B12 |
| `G8-bankruptcy` | high | S | B4 |
| `RR-09` | high | S | B5 |
| `RR-12` | high | S | B10 |
| `TD1` | high | S | B1 |
| `TD8` | high | S | B2 |
| `V-AG-01` | high | S | B14 |
| `VC-14` | high | S | B4 |
| `F11-per-kill-rule` | high | M | C4 |
| `F17-on-demand-bribe` | high | U | C8 |
| `F4_ufo_growth_stop_building` | high | M | C2 |
| `F5_control_chamber_mass_launch` | high | M | C2 |
| `RR-15` | high | M | C2 |
| `UFO_GROWTH_STOP` | high | M | C2 |
| `VC-2` | high | M | C1 |
| `VC-3` | high | M | C1 |
| `VC-9` | high | M | C2 |
| `BD07` | med | H | A6 |
| `BD16` | med | H | A10 |
| `F11_building_detection_probability` | med | H | A6 |
| `F11b-victim-self-ally` | med | H | A4 |
| `SCORE-C2-TRIGGER` | med | H | A6 |
| `SCORE-C6` | med | H | A5 |
| `BD02` | med | H | AL4 |
| `F14_food_chamber_mutant_alliance_reset` | med | H | AL3 |
| `T4` | med | H | AL3 |
| `TD12` | med | H | AL1 |
| `BD04` | med | S | B2 |
| `BD14b` | med | S | B16 |
| `F-M12` | med | S | B5 |
| `F-M3` | med | S | B6 |
| `F-M7` | med | S | B7 |
| `F9_alien_craft_gear_score_gates` | med | S | B11 |
| `RR-10` | med | S | B5 |
| `RR-11` | med | S | B5 |
| `RR-13` | med | S | B10 |
| `T10` | med | S | B8 |
| `T16` | med | S | B9 |
| `T8` | med | S | B9 |
| `TD3` | med | S | B1 |
| `TD9` | med | S | B2 |
| `V-AG-02` | med | S | B14 |
| `V-AG-04` | med | S | B14 |
| `V-AG-07` | med | S | B5 |
| `BD09` | med | M | C3 |
| `F7_withdrawal_threshold_source` | med | M | C6 |
| `RR-14` | med | M | C1 |
| `TD17` | med | M | C7 |
| `V-AG-14` | med | M | CD |
| `BD20` | med | U | S7 row 7 |
| `F-M13` | med | U | S7 row 1 |
| `F-M14` | med | U | S7 row 2 |
| `F10-postbattle-worth-tier` | med | U | S7 row 1 |
| `F13_attack_incursion_target_choice` | med | U | S7 row 14 |
| `F16c-liberation-branch` | med | U | S7 row 12 |
| `RR-17` | med | U | S7 row 8 |
| `TD11` | med | U | S7 row 7 |
| `TD20` | med | U | S7 row 7 |
| `V-AG-13` | med | U | S7 row 15 |
| `VC-13` | med | U | S7 row 8 |

Count: 81 findings (critical 8, high 30, medium 43).
