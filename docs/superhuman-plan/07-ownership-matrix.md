# 07 — Rule ownership, dependencies and audit decisions

This is a **proposed implementation map**, not an assignment to a person or evidence
that the capabilities exist. The [master policy](README.md) owns precedence;
[01](01-opening-economy-bases.md), [02](02-research-and-endgame.md) and
[03](03-military-doctrine.md) own thresholds. [04](04-engine-fidelity.md) owns
fix atoms and sequencing. [05](05-harness-ai.md) owns planner orders, capabilities,
queries and milestones. [06](06-benchmark.md) owns outcome definitions.

## Master orders have an implementation owner and an observable outcome

Owner abbreviations describe responsibilities within existing modules first:
**P** strategic planner (`oa_strategy_ai` proposal); **X** capabilities/executor and
`oa_play`; **Q** engine human-surface queries and observation parsers; **T** tactical
`oa_ai` plus campaign adapter; **B** recorder, provenance and benchmark/learner.
One row may need several owners, but its first owner integrates the result.
M0–M10, SV/EC/PR/OP, CAP and Q identifiers are from 05. Metrics are from 06 unless
explicitly labelled diagnostic. Fix lists are relevant dependencies, not a claim that
every named fix must precede development; 04 GA and 05 M10 govern accepted comparisons.

| Master order | Owner; 05 order / milestone / capability | 04 dependencies | 06 outcome or diagnostic |
|---|---|---|---|
| SO-01 | B; M0; provenance, Q10 | step 0 metadata, G4; full vector | HH, start fingerprint, attributable runs |
| SO-02 | X; OP-01, M3/M4; Q4 | TD1–TD3 for battle outcomes | B2, HH equip failures |
| SO-03 | P/Q/X; SV-01/05, M3; Q1/Q11 | T1/T2, T8, BD06, SCORE-C2, F08 | C3, G2, B4 |
| SO-04 | P/Q/X; SV-02/03, M3; CAP-15/19 | T1/T2, T8, SCORE-C2 | G1/G2; graph-up/no-target hours |
| SO-05 | P/X/Q; SV-04, M3/M5; CAP-18, Q2/Q4 | BD03/04/08/17, TD8/9/10 | B2/B4; startup exceptions separately logged |
| SO-06 | T/X; SV-11, OP-02, M3/M9 | BD01a/b, BD14a/16, TD1–TD3 | B3, casualties and unresolved defences |
| SO-07 | P/Q; SV-06, M3; Q9/Q14 | G2/D1–D7 (A0), G4, G13, SCORE-C1/C2/C6 | G4/G5/G6, E1 |
| SO-08 | P/X; SV-07, M3/M6; Q9, CAP-10 | G4/G8, market atoms | E2/E5/E6 |
| SO-09 | P/X; SV-10, M3; CAP-12 | F15b/F17, F06 | G1/G4; payment and label deltas |
| SO-10 | P/X; SV-08, PR-04, M3/M8; CAP-14, Q8 | F02/F05/F06/F7/F9, SCORE-C6 | U3, G5/G6; city damage |
| SO-11 | P/X; SV-09, M1/M2; CAP-13 | F6 is an open cadence dependency | HH, C3 latency |
| SO-12 | P/X/Q; EC-01/02, M5; CAP-01/02/04, Q5 | RR-02/09/10, V-AG-08 | R2/R3/R4, HH9 |
| SO-13 | P/X; EC-03, PR-01, M5/M8; CAP-02/03 | F-M12, RR-12/13 | R1/R2/R5 |
| SO-14 | P/X; EC-02/03, PR-01, M5/M8; CAP-02/04/05 | F-M12, RR-02/03/07/09/10 | R2/R5/R6 |
| SO-15 | P/X/Q; EC-04, M5; CAP-01/03/05/06/18 | BD08/13/14a/16; BD17 probe below | B1/B2/B4/B5 |
| SO-16 | P/X/T; EC-07, M2b/M7; CAP-07/08, CAP-17 only for stun arm | T1/T2/T10, TD1–TD3, F-M1/2/9/10; F-M13/14 open | C1/C2, E3 |
| SO-17 | P/X; EC-05, M6; CAP-09, Q13 | RR-06, F-M1/2/3/6/7 | E2/E3/E4; realised margin per engineer-week |
| SO-18 | P/X; EC-06, M6; CAP-10, Q6/Q9 | F-M1/2/3/5/6/7 | E2/E3/E5; retained specimens |
| SO-19 | P/X; PR-01, M8; CAP-20 | RR-02/12/13, VC-2/3, RR-14 | R5; idle lab hours |
| SO-20 | P/X; PR-02, M8; CAP-08, Q7/Q8 | RR-12/13, F7 | U1/U2, R5 |
| SO-21 | P/X; PR-02/04, M8; Q7/Q8 | RR-12/13, F7/F9 | U2 by UFO type, R5 |
| SO-22 | T/X/P; PR-03, M7/M8; CAP-09/16/17, Q12 | TD17/19; TD20 open | R7, U2; delivery not stun count |
| SO-23 | P/X; PR-06/07, M9; CAP-11, Q4 | V-AG-01–06 | PE1/PE2; casualties |
| SO-24 | P/X; PR-04/05, M8; CAP-14 | F7/F9, F-M7 | U3, E6; service/readiness ledger |
| SO-25 | P/X; PR-01/08, M6/M8; CAP-09/20 | RR-12/13, VC-2/3/6/7/8, RR-14 | V2, R5/R6 |
| SO-26 | T/X/P; PR-08, M8; CAP-17 | VC-1/2/3, RR-14, TD12 | V1, R5; objectives and evacuation |
| SO-27 | P/X; PR-08, M8; Q8 | UFO_GROWTH_STOP, VC-10/F5 | V3b, B3, G5/G6; wave ledger |
| SO-28 | T/X/B; PR-08, M8; CAP-17 | VC-1, TD12 | V3; victory screen |
| SO-29 | B/X; OP-03, M0/M1/M2 | record vector, no gameplay fix prerequisite | HH, all milestone event sources |
| SO-30 | B; M0/M1/M10; gene admission G1–G5 | GA, then each changed atom | preregistered 06 endpoints, censoring/guardrails |

## Detailed rule catalogue inherits those owners

Each row maps a rule from its named appendix to the master row(s), including their
capabilities, engine dependencies and metrics. This avoids copying the same owner list
hundreds of times. A mapping is not permission to omit that rule's WHEN/THEN/UNTIL.
Prose procedures and measurement IDs are addressed after the catalogue.

### 01 — Economy, staff, bases and raids

| Rule | Master owner(s) | Distinction |
|---|---|---|
| ST-0 | SO-13 | conditional pair, not day-zero default |
| ST-1 | SO-12 | soldier hire and beds |
| ST-2 | SO-12, SO-16 | raid/garrison shortfall |
| ST-3 | SO-13 | useful parallel capacity |
| ST-4 | SO-14 | hire-ahead; 02 H-1 wins for useful empty seats |
| ST-5 | SO-17 | engineer count/product test |
| ST-6 | SO-12 | engine-only pool workaround |
| ST-7 | SO-12 | replace unavailable staff |
| ST-8 | SO-12 | skill upgrade |
| BS-1 | SO-15 | required 2x2 space |
| BS-2 | SO-15 | affordable insurance |
| BS-3 | SO-15 | shock response; BS-P still required |
| BS-4 | SO-15, SO-17 | proven production and room for B3 |
| BS-5 | SO-08, SO-15 | cut-mode exception only |
| BS-P | SO-15, SO-05 | stage, buy, disembark/read back |
| DEF-1 | SO-05, SO-15 | two stations, then dilution check |
| DEF-2 | SO-05 | staff dilution |
| DEF-3 | SO-05 | physical armed presence |
| DEF-4 | SO-06 | never retreat a defence |
| DEF-5 | SO-03 | located infestation near base |
| G-C1 | SO-16, SO-05 | eight raiders beyond depth |
| G-C2 | SO-16 | cargo and seats |
| G-C3 | SO-16 | live temple/RAID button |
| G-C4 | SO-16, SO-23 | single raid and healing |
| G-C5 | SO-16 | tactical-score tripwire |
| G-C6 | SO-05 | garrison after dispatch |
| G-C7 | SO-16, SO-03 | task priority |
| G-C8 | SO-16 | default takeover stop; optional continuation arm |
| SELL-1 | SO-18 | shared item-row pace |
| SELL-1b | SO-18 | optional after stock probe |
| SELL-2 | SO-18, SO-08 | cash-need liquidation |
| SELL-3 | SO-18 | hold on price fall |
| SELL-4 | SO-18 | week-positive items |
| SELL-5 | SO-18 | undamaged craft |
| DP-1 | SO-09 | Government settlement |
| DP-2 | SO-09 | necessary supplier |
| DP-3 | SO-09 | never pay ineligible org |
| DP-4 | SO-09 | named rift offers |
| DP-5 | SO-09, SO-16 | loot-funded diplomacy |
| C-1 | SO-08 | CUT mode |
| C-2 | SO-08, SO-12 | dismiss idle staff only after useful reassignment fails |
| C-3 | SO-08 | unused facility upkeep |
| C-4 | SO-08, SO-18 | spare assets before essential carrier |
| C-5 | SO-08, SO-17 | income must cover burn |
| C-6 | SO-08 | Monday bankruptcy floor |
| RG | SO-08 | all purchase commitments |
| SO-F1 | SO-08, SO-18 | Sunday payroll |
| SO-F2 | SO-18, SO-29 | price/stock series |
| SO-F3 | SO-07 | funding report |
| SO-F4 | SO-08 | purchase gate |
| SO-F5 | SO-07, SO-08 | completed-weeks cutoff |
| SO-R1 | SO-16 | raid eligibility |
| SO-R2 | SO-16, SO-22 | stun-only requires CAP-17 |
| SO-R3 | SO-18, SO-29 | verify loot proceeds |
| SO-R4 | SO-16 | guard-tier readiness |
| SO-R5 | SO-16 | window urgency never overrides readiness |

### 02 — Research, specimens and endgame

| Rule | Master owner(s) | Distinction |
|---|---|---|
| F-1 | SO-13 | ST-0 default; early arm separate |
| F-2 | SO-14 | advanced Physics |
| F-3 | SO-14, SO-17 | Advanced Workshop |
| F-4 | SO-14 | advanced Bio |
| F-5 | SO-14 | parallel specimen work |
| F-6 | SO-05, SO-19 | optional advanced station |
| F-7 | SO-15 | no 2x2 space; no four-visitor shortcut |
| F-8 | SO-14, SO-22 | specimen transfer to lab base |
| F-9 | SO-14, SO-15 | small/large lab caps |
| H-1 | SO-12 | best available into useful seats |
| H-2 | SO-14 | hire ahead |
| H-3 | SO-12 | replace weak staff |
| H-4 | SO-12 | healthy onsite staff |
| H-5 | SO-12 | pool/population margins |
| H-6 | SO-12, SO-08 | reassign; CUT exception |
| R-0 | SO-19, SO-18 | topic reachability and item keep-list |
| P-1 | SO-19, SO-20 | three manned systems |
| P-2 | SO-19, SO-14 | AQPL unlock |
| P-3 | SO-19, SO-14 | Probe then Workshop |
| P-4 | SO-19, SO-21 | Transporter type topic |
| P-5 | SO-19, SO-25 | Bio-Transport craft |
| P-6 | SO-19, SO-25 | dimension topic |
| P-7 | SO-19 | optional beam/station |
| P-8 | SO-19 | legitimate UFO filler |
| P-9 | SO-19, SO-22 | survival equipment |
| P-10 | SO-19 | Dimension Gates filler |
| B-1 | SO-19, SO-22 | day-zero module topic |
| B-2 | SO-19, SO-22 | first specimen unlock |
| B-3 | SO-19, SO-14 | ABL |
| B-4 | SO-19, SO-26 | next building topic |
| B-5 | SO-19, SO-22 | Toxin B chain |
| B-6 | SO-19 | reachable Bio filler only |
| SP-1 | SO-16, SO-22 | specimen raid, not funding raid |
| SP-2 | SO-09, SO-22 | non-hostile guard protection |
| K-0 | SO-03, SO-22 | do not delay seed alert for module |
| K-1 | SO-17, SO-22 | three-module target; fit first immediately |
| K-2 | SO-22 | module/containment/loadout |
| K-3 | SO-21, SO-22 | Transporter captures |
| K-4 | SO-22 | abort capture safely |
| K-5 | SO-22, SO-19 | stored specimen then research |
| K-6 | SO-22 | containment capacity |
| K-7 | SO-22, SO-14 | transfer specimen |
| K-8 | SO-06, SO-22 | base-defence bio loot; TD20 open |
| K-9 | SO-22, SO-29 | stun score is not delivery |
| U-0 | SO-20, SO-22 | ready recovery transport |
| U-1 | SO-20 | first manned unlock |
| U-2 | SO-20 | recovery ETA |
| U-3 | SO-20, SO-19 | verify won mission unlock |
| U-4 | SO-21 | Transporter window |
| U-5 | SO-10, SO-21 | crash versus destruction |
| U-6 | SO-21 | repeat recoveries optional |
| U-7 | SO-10, SO-24 | score-gated UFO kit |
| U-8 | SO-20, SO-05 | refuse unsafe recovery |
| U-9 | SO-20 | retreat loses unlock/loot |
| U-10 | SO-20, SO-14 | loot at correct lab base |
| G-1 | SO-25 | fit first hull |
| G-2 | SO-25 | token crossing |
| G-3 | SO-25, SO-19 | dimension then first building |
| G-4 | SO-26, SO-05 | raid readiness |
| G-5 | SO-26 | land; do not destroy building from air |
| G-6 | SO-26, SO-19 | next topic within three hours |
| G-7 | SO-26 | rebuild after loss |
| G-8 | SO-25, SO-27 | reread portals and fleet |
| G-A | SO-26 | buildings 1–2 readiness |
| G-B | SO-26 | buildings 3–6 readiness |
| G-C | SO-26 | buildings 7–10 readiness |
| W-1 | SO-27, SO-19 | Control topic |
| W-2 | SO-27 | visible-evidence stock estimate |
| W-3 | SO-27 | bounded launch-delay experiment |
| W-4 | SO-27, SO-05 | retaliation readiness |
| W-5 | SO-27 | hedge window |
| W-6 | SO-27, SO-10 | fight wave |
| W-7 | SO-27 | unbound first-entry-strike hedge |

### 03 — Military doctrine

| Rule | Master owner(s) | Distinction |
|---|---|---|
| INF-0 | SO-03, SO-05 | explicit first-day seed exception, four home |
| INF-1 | SO-03 | completed visible drop |
| INF-2 | SO-03 | stale-alert triage |
| INF-3 | SO-04 | Government, located targets only |
| INF-4 | SO-03, SO-04 | supplier, located targets only |
| INF-5 | SO-03 | re-housed crew alert |
| INF-6 | SO-04, SO-29 | graph history |
| INF-7 | SO-04 | deleted, no implementation |
| INF-8 | SO-08, SO-10 | permanent takeover response |
| AIR-1 | SO-10 | scramble |
| AIR-2 | SO-10 | target ranking |
| AIR-3 | SO-10 | attitude/decoy |
| AIR-4 | SO-10, SO-05 | agent-free interceptors |
| AIR-5 | SO-10 | withdrawal burst |
| AIR-6 | SO-24 | break and service |
| AIR-7 | SO-10 | guided hover shot |
| AIR-8 | SO-10 | no permanent-cut hold |
| AIR-9 | SO-24 | score-gated procurement |
| AIR-10 | SO-24 | reload stock |
| AIR-11 | SO-10 | no non-UFO targets |
| GND-1 | SO-20 | recover within ETA |
| GND-2 | SO-02, SO-05 | kit and garrison gate |
| GND-3 | SO-21 | Transporter craft prerequisite |
| GND-4 | SO-20, SO-22 | dispatched craft modules |
| GND-5 | SO-16 | Cult RAID |
| GND-6 | SO-29 | battle ledger |
| BASE-1 | SO-05, SO-15 | stations |
| BASE-2 | SO-05 | never empty/diluted |
| BASE-3 | SO-02, SO-05 | lethal kit at each base |
| BASE-4 | SO-05 | hostility shock |
| BASE-5 | SO-06 | landing preparation |
| BASE-6 | SO-05, SO-10 | exposed-base cover |
| BASE-7 | SO-02 | reserve kit |
| BASE-8 | SO-15 | BS-P expansion |
| BASE-9 | SO-05 | recalculate fielded roster |
| AGT-1 | SO-12, SO-05 | soldier hire gate/emergency floor |
| AGT-1b | SO-12 | candidate ranking; H-1 governs technicians |
| AGT-2 | SO-23 | training assignment |
| AGT-3 | SO-23 | healing |
| AGT-4 | SO-02 | post-hire equip verification |
| AGT-5 | SO-12, SO-23 | android/hybrid mix |
| AGT-6 | SO-02, SO-24 | vendor buy-ahead |
| TIME-1 | SO-11 | observable-aware speed |
| TIME-2 | SO-11, SO-29 | daily/hourly routines |
| SCORE-1 | SO-07, SO-10 | Sunday governor |

Unnumbered procedures are not orphan features: 01's product test and Stores gate belong
to SO-17; 03's tactical drills and capture drill to SO-06/22/26 (T, M8/M9); its
endgame kit and squad table to SO-26. The M-* probes in 01, M-S2-* probes in 02 and
E/HR/KPI lists in 03 are evidence/measurement requirements, not extra standing orders.
Their named rules inherit the owners above; M0/M1 record them, M2b owns economy probes,
M5 base-purchase/presence, M6 production, M8 capture/objective/transport, M9 training,
and M10 before/after fidelity comparisons. Unknowns remain unknown until those probes run.

## Namespaces must not silently alias each other

| Ambiguous name | Meaning and canonical mapping |
|---|---|
| 02 E1 / E3 | Manned-system / Transporter unlock cases; map to 06 U2 with UFO type and 06 R5, not 06 E1/E3 cash metrics |
| 02 U-1 / U-2 | Standing orders for manned recovery / ETA; not identically numbered benchmark outcomes |
| 06 U1 / U2 | Any won recovery / won manned recovery; Transporter is an explicit U2 subtype |
| 05 M-A / M-B | Survival-to-day-21 / Government takeover time → 06 survival endpoint and G1 |
| 05 M-C / M-D / M-E | Alert latency / lost base defences / funding-cut Monday → 06 C3 / B3 / G4, with E5 runway diagnostic |
| 01 M-1 etc. / 05 M1 etc. | Measurement probes / implementation milestones; keep appendix prefix |
| 02 F-1 / 06 F1 | Lab-building order / reward correction; unrelated |
| 04 G4 / 06 G4 | Start-funding fix / funding-survival metric; unrelated identifiers |
| 03 E-* / 06 E* | Military experiments / economy metrics; never join on the bare ID |
| HEAD / LIVE | Historical source versions defined in README; neither means the current worktree |
| GA / Gate A | 04 step 6, A0 + A-now; partial-fidelity AI baseline, not post-step-9 original parity |

## Reconciled defaults and explicit exceptions

1. **Cash and calendar:** RG and readiness override every printed date. The D27.2 hull,
   D27.5 crossing and D77.8 victory are conditional arithmetic targets. ST-0 defers the
   lab pair by default; the early F-1 arm is separate. E3/C1 raid totals are sensitivity
   assumptions, not income already available to spend.
2. **Garrison:** 8/10/12 fielded targets include staff dilution; one disembarked soldier
   prevents an empty base but is not adequate defence. 03 INF-0's first-day six-out,
   four-home response is the explicit startup exception. Broader `sweep_draw=always`
   and Cult `floor_quiet` are optional risk arms, never the default funding strategy.
3. **Expansion:** BS-P stages the full computed garrison before buying, then verifies
   disembarked presence before time advances. Build two stations. B3 follows BS-4's
   production, space, cash and seven-day garrison gates; one observed defence is a
   diagnostic, not an extra mandatory trigger. Do not gate B2 on the hull it enables.
4. **Staff:** fill useful critical seats with best available applicants under H-1.
   ST-4/AGT-1b skill floors apply only to discretionary selection, not idle critical
   seats. Reassign idle staff first; CUT mode may dismiss them if no useful seat exists.
5. **Targets:** no default blind Senate/supplier sweeps. The 03 §3.6 blind-search budget
   is a separate experiment. Cult takeover stops default raids; G-C8 continuation and
   the stun-only arm need separate tags/readiness, with CAP-17 required for the latter.
6. **Endgame:** M8 CAP-17 owns tactical capture, objective destruction, evacuation and
   tactical hidden-read removal. M7 owns the raid/cargo plumbing; it does not imply a
   working capture AI. Three Bio modules are a K-1 target; PR-03's first module is the
   immediate recovery prerequisite, not a conflicting limit of one.
7. **Counters:** battle entry counts an attempt only. U1/U2 require a won recovery;
   R7 requires delivery into containment. Neither a stage transition nor a stunned
   enemy proves the intended outcome. The additional R7 is diagnostic until weights
   are ruled on; do not silently change the proposed Milestone Index.
8. **Source custody:** the local commits through `bce89743` supersede old instructions
   to commit/wait for LIVE. They do not certify historical binaries or publication on
   origin. Fix-presence probes and hashes remain necessary. Private binaries/databases
   stay outside Git; the external archive is a reproduction dependency, not a new file
   to vendor into this plan.

## Open evidence has a follow-up owner

| Open item | Owner / milestone | Required evidence and safe interim policy |
|---|---|---|
| BD17 docked-craft presence | Q/X with engine base-defence owner; M5 P-C, 04 step 8 | Compare empty base, docked combat agent, disembarked combat agent and staff-only cases at landing and Raid/Storm; repeat before/after purchase and save/load. Distinguish battle roster inclusion from the emptiness check. The register's match verdict and later snapshot discrepancy remain separately recorded; disembark the full garrison meanwhile. Amend verdict only after evidence review, not by silently changing the 193 census. |
| F-M13/14 raid worth and cargo | X/B; M2b P-A | Funds, item and label deltas with/without cargo; do not invent a post-raid debit. |
| TD20 base-defence capture | T/X; M8 | Resolve a defence with a live specimen and containment; verify actual storage separately from score. |
| TD14 reinforcement timing | engine tactical owner + T; 04 step 15 / M8 | Timestamp spawned reinforcements per building in the original and selected engine; no invented original interval. |
| F6 city cadence, VC-13/RR-17 dimension time | Q/B/X; M1 and M8 | Observe ticks, invasion times and lab/build progress across sorties; distinguish city pause from travel time. |
| Static-prior/human-surface permission | user plus Q/B; 05 OQ4–7, 06 DEC-3 | Document the visible surface or approved published prior; hidden exact crews, global foes and stock remain prohibited in clean runs. |
| Reward, endpoint, tactical genes, budget | user plus B; 06 DEC-1/2/4/5 | Decide before implementing rewards or starting expensive experiments; documentation completion does not authorize runs. |

## Audit scope

The 2026-10-24 local audit passed: 35 Markdown destinations/anchors across 12 plan and
evidence documents; all 30 master orders mapped exactly once; all 174 detailed rule IDs
covered (01: 56, 02: 73, 03: 45). The 193-row census agrees with README. The 24 classes
in 05 contain 21 engine dependencies, 23 capability gaps, 23 policy gaps and 20 with
all three. Source history confirms the six local commits through `bce89743`; remote
publication was not checked. The cross-section decisions above were reviewed manually.

This audit does not revalidate every EXE finding, rerun the
historical statistics, fetch every public URL, or certify a campaign win. Raw evidence
remains in the README's external archive. Proposed tests and measurements are not passed
tests or observed outcomes.
