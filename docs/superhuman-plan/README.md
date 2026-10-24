# Beating X-COM: Apocalypse at Superhuman — strategy, AI and engine plan

**Status: documentation assembled; implementation and campaign validation remain proposed.**
Protect the Government and every base, turn Cult loot into staffed research and production,
recover a manned UFO and a Transporter, build a Bio-Transport, then destroy the objectives
in all ten alien buildings. The plan targets **Superhuman: UI button 5, engine difficulty 4**.
It uses original-game rules as the target, with explicit workarounds for the audited rebuilt
engine. It is not evidence that this strategy has already won a campaign.

Analysis coverage: archived guide research and EXE adjudications reviewed; local commit
history checked on 2026-10-24; cross-document ownership, calendar and link checks completed.
No fresh campaign, binary decompilation, or current-engine parity audit was run in this
continuation. Historical statistics retain their original populations and uncertainty.

| Read | Purpose |
|---|---|
| [01 — Opening, economy and bases](01-opening-economy-bases.md) | Purchases, scientists, facilities, solvency, manufacture-and-sell and Cult loot |
| [02 — Research and endgame](02-research-and-endgame.md) | Dependencies, captures, staffed-lab arithmetic and the ten raids |
| [03 — Military doctrine](03-military-doctrine.md) | Infiltration, interception, squads, garrisons, training and healing |
| [04 — Engine fidelity](04-engine-fidelity.md) | Evidence-backed fixes, tests, sequencing and unresolved reverse engineering |
| [05 — Harness AI](05-harness-ai.md) | Human-visible observations, standing orders, capabilities and milestones M0–M10 |
| [06 — Benchmark](06-benchmark.md) | Provenance, controls, censoring, metrics and proposed rewards |
| [07 — Ownership matrix](07-ownership-matrix.md) | Rule-to-owner-to-fix-to-metric traceability and reconciliation decisions |
| [Resolved facts](evidence/resolved-facts.md) / [findings summary](evidence/findings-summary.md) | Canonical rules / all 193 register findings |

## Source versions are historical, not the current working tree

Throughout these appendices, **HEAD means the engine at `fda36d6c`**, the base of this
documentation branch. **LIVE means the research snapshots taken on 2026-10-07/08**, not
whatever a developer currently has checked out. “Engine today”, bare source line numbers,
missing-capability counts and benchmark baselines are observations of those snapshots.
Re-resolve symbols and reproduce a defect on the selected implementation base before coding.

The former uncommitted work subsequently landed in this local six-commit lineage:

| Commit | What the history establishes |
|---|---|
| [`cf56132f`](https://github.com/khallmark/OpenApoc/commit/cf56132f) | Automation, learner, strategy genes and role loadouts |
| [`879d0d73`](https://github.com/khallmark/OpenApoc/commit/879d0d73) | Funding rewrite, base-defence trigger changes, score category 7 and associated tests |
| [`904abc3a`](https://github.com/khallmark/OpenApoc/commit/904abc3a) | Display pacing changes |
| [`5500ce24`](https://github.com/khallmark/OpenApoc/commit/5500ce24) | Driver, armed squads, retreat and further harness changes |
| [`42c48e7f`](https://github.com/khallmark/OpenApoc/commit/42c48e7f) | Retreat/wipe-out research documentation |
| [`bce89743`](https://github.com/khallmark/OpenApoc/commit/bce89743) | Traffic work; end of the lineage reported by the original investigation |

These commit objects exist locally. This is **not a claim that they were pushed to
`origin/khallmark/develop-reboot`**, that the snapshots equal the final commits byte-for-byte,
or that historical benchmark binaries can be attributed to them. The source archive omits
`live_src/` and `live_snapshot/`; historical binaries came from a moving working tree.
Funding/base-trigger work is already committed (04 A0); do not create a “snapshot of another
agent's uncommitted tools” commit or wait for those old edits to land. Future implementation
starts from an agreed committed descendant, with fresh status and ownership checks. This
documentation continuation does not change the separate traffic worktree or PR.

## What the old benchmark actually showed

The failure archive contains **162 primary logs plus four auxiliary runs, 166 checkpoint
saves and 116 finals**. Section 06 pins a different, explicitly selected **115-final**
population; section 05's recent audit uses **31 runs**. Do not combine their denominators.
Of the 116 finals, **107 ended by signal, eight by defeat and one by the harness's bankrupt
classification**. Most “end days” are censored wall-clock stops, not defeat dates. Saves
are last checkpoints, not necessarily final states.

| Observation in the archive | Interpretation / action |
|---|---|
| Government hostility/takeover appeared eventually in 57 of 166 runs; funding was cut in 38. All 47 saved Governments with relation <= -50 had `takenOver=true`. | Takeover, not merely stray fire, is the main observed threat to future funding. Watch the visible infiltration graph and clear located infestations. |
| Eight of nine recorded game-overs ended in a lost base defence; many defenders were unarmed staff. At least 13 silent second-base losses were suspected. | Delivered lethal kit and per-base physical presence must gate every dispatch and purchase. The silent-loss count remains an inference. |
| Builds at or before 05:27 on October 7 retained $80,000 income; builds from 06:32 showed $10,000. | The G4 start-order regression depresses the economy. Do not blame AI policy for an eightfold funding change. |
| 14,577 infiltration polls produced only 58 resolved sweeps; seven recovery battles in the recent audit still recorded zero recoveries. | Repair event semantics before rewarding the counters. |
| No manufacture action in 98 logs; workshops idle in all 2,451 sampled snapshots. | The manufacture-and-sell pillar is missing execution, not just poor product selection. |

One archived log pair makes the problem concrete (`control-seed11/r0001/victory.log`,
lines 914 and 916): “FUNDING TERMINATED PERMANENTLY” followed by
“government relation -100 < intercept_min_relation 40: holding fire”. Holding fire after
a permanent cut sacrifices interceptions without restoring funding. These are archive
quotes, not new observations. Sources: `research/failure_forensics.md` §§0–3, 5–11 and
`research/harness_audit.md`; [06 §1](06-benchmark.md#1-what-the-current-benchmark-measures).

## One campaign policy, with explicit safety and budget gates

**Conflict precedence:** original rules come from adjudications, then resolved facts.
Military readiness and garrison depth come from **03 §7.6**; spend permission comes from
**01's Runway Gate RG(k)**; research dependencies and conditional dates come from **02**;
execution and resource arbitration come from **05**; metric identifiers come from **06**.
No calendar date overrides a safety gate or creates money, staff or specimens.

The economic tables in 01 are sensitivity examples, not a funded complete campaign:
they omit some research facilities, use a $40,000 air allowance rather than 03's $49,600,
and originally assumed 6/4-soldier garrisons. Operational targets are **8 fielded soldiers
per base early, 10 from week 3, 12 under the threats in 03**, after staff dilution, plus
the sortie squad. A newly purchased base must have its garrison staged before purchase;
one disembarked soldier is only the no-empty-base emergency floor, not adequate defence.
Two Security Stations are the default; add more only if fielded soldiers still meet target.
The explicit startup exception is 03 INF-0: six of the ten starters handle the seed
alert while four remain home for the first hours. This does not permit routine dispatch
below depth or Cult raids that borrow the garrison; broader draw policies are risk arms.

The extra Quantum labs are **conditional pre-building**: apply 01 ST-0 (a manned UFO
sighting or Day 7, plus cash and space gates). Section 02's day-zero F-1 model is an
optimistic comparison arm, not the default opening. If the pair starts later, record its
actual completion and reforecast the dependent topics; do not assert the printed dates
still hold. Likewise, do not buy B2 merely because Day 28 arrived. Expansion must pass
RG, readiness and a demonstrated space/production need. Do not require a completed
Bio-Transport hull to buy the base needed to build its Advanced Workshop.

### Calendar: conditions first, dates second

**Day N** means the harness calendar; **D** means elapsed days since Tuesday noon.
Day = floor(D + 0.5) + 1; the first Monday is **Day 7 = D5.5**.

| Period | Campaign objective and release condition |
|---|---|
| Day 1 | Verify start, equip every deployable soldier, start Bio-Transport Module and Dimension Gates research, start quarters and two stations, handle the seed alert. No empty second base; no early alien-building research shortcut. |
| Days 2–7 | Fit capture/cargo modules; heal and train idle soldiers; hire against seats and garrison/raid demand. Start Cult raids only with eight verified raiders beyond garrison depth. Read the first funding report before multi-week commitments. |
| Weeks 2–4 | Win a manned UFO recovery; prioritize the finite Transporter growth window (weeks 2–4). Staff parallel Physics, build advanced labs/workshop when unlocked and affordable, expand only with the staged garrison. |
| Weeks 4–6, conditional | Build and fit the Bio-Transport; make the first crossing, unlock The Alien Dimension, then the first building topic. Manufacture profitable goods and craft; distinguish booked inventory from cash actually received. |
| Alien campaign | Sleeping → Food → Alien Farm → Maintenance → Incubator → Control → Spawning → Organic Factory → Megapod → Gate Generator. Destroy objectives and evacuate; prepare for the Control Chamber retaliation. |
| End condition | Victory screen after the Gate Generator objective, not merely a high reward or a kill-all counter. |

The fully staffed 02 model's **target** is hull D27.2, crossing D27.5, first raid D32.8,
Control Chamber D57.8, victory D77.8. Its stretch/late victory bounds are D54.8/D109.8.
These are arithmetic scenarios with explicit recovery, staffing and per-raid preparation
assumptions, **not measured win dates**. The default delayed lab build, RG failures,
casualties and missing specimens can move them later. An unstated-difficulty human Let's
Play crossed around day 30 and won around day 99; it is context, not a Superhuman guarantee.

### Standing orders

These 30 master orders summarize the detailed rule catalogue; 07 maps each to implementation
ownership, engine dependencies and outcomes. UNTIL specifies a release condition, not a timer
that can bypass a precondition.

| ID | WHEN | THEN | UNTIL |
|---|---|---|---|
| SO-01 | Starting or resuming | Verify difficulty, binary/data/tools identity, fidelity flags and start fingerprint; reject mixed evidence | Provenance passes |
| SO-02 | First deployment or new hire | Verify delivered armour, loaded lethal primary, spare clips and medi-kit | Every claimed soldier is ready |
| SO-03 | Located alert, marker or completed UFO drop-off | Queue a sweep; prioritize Government and threats to bases; never read hidden crew counts | Cleared or visible target disappears |
| SO-04 | Government graph rises | Increase observations and dispatch the largest safe squad to located targets; no blind search | Two declining readings or stable zero |
| SO-05 | Any dispatch, staff change or base purchase | Recompute fielded garrisons and leave an armed soldier disembarked at every base | Floors and depth both hold |
| SO-06 | Base attack | Fight to completion, then heal, rearm and audit losses; no wall-time retreat | Defence resolved |
| SO-07 | Sunday and Monday roll | Read score and income; use completed-weeks score P, not P+W, for imminent cutoff | Report recorded |
| SO-08 | Spending or funding cut | Apply RG; after a cut freeze discretionary growth and sell surplus against payroll | Solvent income or bad-debt end |
| SO-09 | Recoverable Government/supplier relation loss | Read a settlement price; pay only from spendable cash; never bribe a taken-over org | Label improves or offer rejected |
| SO-10 | UFO in city | Intercept with the armed fleet; protect civilians; release an engine-only hold after funding termination | UFO gone |
| SO-11 | Alert/sortie needs attention | Use game-time deadlines and observable-aware speed; pause for UI work | No serviceable urgent task |
| SO-12 | Beds or seats constrain planned hires | Build quarters ahead; hire best available skill into useful seats and soldier demand | Target filled within caps |
| SO-13 | Multiple Physics topics will be available | Apply ST-0, build and staff the pair if RG and space permit | Three staffed small labs or revised schedule |
| SO-14 | Advanced facility unlock approaches | Reserve a 2x2, construct and hire ahead; transfer research items to the lab's base | Useful staffed facility |
| SO-15 | Space/production need or insurance purchase is affordable | Stage a garrison, buy/read back, disembark, build stations/quarters/stores | New base ready without stripping the old |
| SO-16 | Eight raiders beyond garrisons, cargo craft and safe Cult window | RAID a temple, not EXTERMINATE; record casualties and recovered loot | Return, casualty stop or takeover stop |
| SO-17 | Idle staffed workshop and profitable reachable product | Manufacture against observed cost, price, labour and reserve; prioritize undamaged craft for unbounded sales | Batch complete or constraint binds |
| SO-18 | Sale pass or cash emergency | Preserve research specimens/last artifacts; pace each shared item row; verify cash delta | Surplus sold or price/keep limit binds |
| SO-19 | Lab idle | Start the next reachable critical-path topic, then legitimate filler; never cancel busy research blindly | Lab busy or no reachable topic |
| SO-20 | Manned wreck appears | Recover with cargo and adequate soldiers; count system unlock only after a won mission | Debrief and topic read-back |
| SO-21 | Transporter appears in weeks 2–4 | Prioritize disabling and recovering one; retain UFO type and outcome | Transporter topic available |
| SO-22 | Capture mission planned | Verify Bio-Transport Module, containment, specimen need and safe stun opportunity | Specimen delivered or safe abort |
| SO-23 | Soldiers idle or injured | Train unassigned hires; leave starters' Combat assignment intact; heal before dispatch | Ready or deployed |
| SO-24 | Fleet service or upgrade due | Repair, refuel, rearm and buy by combat value under RG | Verified ready fleet |
| SO-25 | Bio-Transport path complete | Manufacture hull, fit crew/cargo, cross once, research The Alien Dimension | First building topic available |
| SO-26 | Next building topic and raid readiness complete | Destroy objective tiles and evacuate; keep engine-only kill-all workaround separately tagged | Successful debrief unlocks next step |
| SO-27 | Preparing Control Chamber raid | Estimate remaining fleet from visible evidence, protect bases and reserve a score/cash cushion for retaliation | Wave resolved or absence observed |
| SO-28 | Gate Generator objective complete | Evacuate and verify victory screen | Recorded campaign victory |
| SO-29 | Every material action | Record human-visible pre/post state, actual outcomes, income sources and checkpoints | Trace durable |
| SO-30 | Comparing an AI change | Freeze engine/vector, run A/A then controlled arms; separate censoring and judge outcomes before promoting genes | Pre-registered decision or inconclusive |

## Engine and AI work must remain measurable separately

**Gate A = 04's GA, immediately after step 6: A0 plus A-now.** This resolves an older
contradiction that also called the post-step-9 build “Gate A”. It is a **partial-fidelity
AI baseline**, not original-game certification. Fix artificial hardship first: G4 funding,
T1/T2 takeover, stray-fire/vehicle relations, score overcharges, excess base attackers,
spread, and re-homing after base loss. A0's funding/base-trigger changes already exist
in the local committed lineage.

Then measure the AI on the frozen GA build. Steps 7–9 add enemy scaling, initial crews,
organisation raids, defence rosters/turrets and bankruptcy; **re-fit battle doctrine and
garrisons there**. Later steps correct markets, hiring, research and training, then the
objective-based endgame. Each changed vector needs fresh controls. No benchmark claim
may describe GA as a faithful original-game win.

| Harness phase | Deliverable / success evidence |
|---|---|
| M0–M1 | Provenance, actual-event counters, game-day budgets/checkpoints and hidden-read audit; old finals cannot silently enter the new history |
| M2 / M2b | Game-time planner with a legacy comparison arm; focused funding, Cult-loot and market probes |
| M3–M4 | Government containment, garrison claims, visible infiltration and resource readers; no hidden tactical counts in certified runs |
| M5–M7 | Multi-base staffing, manufacture/selling and Cult raids; measure realised net income and casualties, not clicks |
| M8–M9 | Recovery/research/fleet, training/healing and endgame dispatch; tactical capture/objective execution is a required owner, not a presumed existing capability |
| M10 | Re-baseline after each fidelity step and retire obsolete workarounds |

For implementation use existing ownership boundaries before adding modules. The proposed
planner and query split in 05 are designs, not instructions to create every named file
regardless of what the selected base already provides. Keep behaviour changes separate
from refactoring. Roll back an isolated fix with its commit or declared fidelity toggle;
never pool its pre/post results. Generated data requires its own hash and regeneration.

## Benchmark acceptance and decisions still needed

[06](06-benchmark.md) proposes game-day tiers: scenario saves, day 21, day 42, then victory.
Record binary, extracted data, harness and option hashes, live difficulty, seed, start
fingerprint and atomic fidelity flags. Common seeds aid bookkeeping; historical matching
seeds did not establish useful variance reduction. Run an A/A on each new configuration;
size comparisons conservatively as unpaired unless correlation is demonstrated.

The proposed 16/32/48/64-per-arm ladder detects large effects, not small knob changes.
Wall-capped runs are right-censored, not wins or defeats. Primary outcomes include Government
and funding survival, garrisoned live bases, realised non-Senate income, useful staffed labs,
won manned recoveries, objective raids and victory. Milestone rewards and the claimed power
need validation by the planned statistical implementation; historical mixed-build curves
are descriptive, not certification baselines.

User decisions before implementation/experiments: 06 DEC-1–5 (reward weights, primary
endpoint, human-visible allow-list, tactical-gene policy, machine/time budget); 05 OQ4
(which published static priors count as human knowledge); assignment of funding/shared-file
ownership; and whether to test the optional riskier Cult draw and day-zero lab pre-build
arms. Defaults above keep the safer garrison policy and conditional lab build. These choices
do not block completing this document. No engine edits, new benchmark runs, pushes, merges
or deployment are part of this documentation task.

## Evidence, review status and private archive

Evidence precedence is [adjudications](evidence/adjudications.md) →
[resolved facts](evidence/resolved-facts.md) →
[validation register](evidence/validation_register.md) and the eleven per-mechanic write-ups
in [original-game findings](../original-game/findings/validation-2026-10/).
The summary inventories **193 findings: 58 match, 81 diverge, 25 engine-missing, 12
engine-bug, 16 unbound, one partly compared**. Unknown findings are not proven mismatches.
Group A/B/C describes fix sequencing, not another evidence verdict; 04 owns that grouping.

“Survived” means two reviewers did not refute the finding, not formal proof.
“Adjudicated” resolves a disputed review. “Single-source-unrefuted” has weaker review.
[DERIVED] numbers depend on stated assumptions; [MEASURE] and [UNBOUND] remain open.
The 34 rulings include the consolidated UFO-growth ruling; it is not a 194th independent
mechanic. BD17's docked-craft discrepancy is a follow-up probe, not a silently reclassified
register row. Unbound reinforcement timing, capture recovery and post-raid worth must not
be implemented from invented constants.

The durable research archive is outside this repository:

```text
/Users/khallmark/Desktop/Code/OpenSource/OpenApoc-og-research/export/strategy-plan-2026-10-08/
  research/    guide digests, failure forensics, economy and technology analyses
  validate/    validation and refutation working material
  synth/       synthesis inputs
  ghidra/      exported evidence, not the project database
  cult_work/   Cult-raid analysis
  SNAPSHOT_INFO
  DIFFSTAT_vs_HEAD.txt
```

The appendix citation `research/foo.md §n` resolves under that directory. Public guide
URLs and paraphrases remain in the appendices/digests; no exhaustive coverage of every
online guide is claimed. Original EXEs, Steam/ISO content and Ghidra `.gpr`/`.rep` databases
stay outside git. The archive's absence on another machine limits reproduction of raw
statistics; the checked-in validation notes and this distinction remain portable.
