# Weekly government funding, cutoff and upkeep

**BOUND:** UFO2P.EXE ISO non-4 (SHA-256 `99f8787d5f1cb532e2620af2130bffb2558a869f9d4540d7fcf1b7487833d00f`),
funding routine at file `0xF6880`, score rollover at file `0xFA234`, finance/upkeep at file
`0xFB114`, alien-building score at file `0x1159E4` with its table at file `0x191B7C`.

Static investigation, Ghidra 12.1.4 `-readOnly -noanalysis` on the `OpenApocOG` project, plus an
interpreter over the decoded funding block (7,128 cases: every tier boundary, rounding,
Government balances −3…1 000 000, relations −51/−50/0, previous-weeks score −2401/−2400/0). No
runtime session of the original. Code offsets below are physical file offsets; for this code object
file = VA + `0x626A4`, for its file-backed data file = VA + `0x616A4`.

## Order of operations each Monday

At the weekday-0 midnight transition the calendar (file `0xAD2E4`) calls the funding routine
(file `0xAD496`) and, later, the score rollover (file `0xAD4B8`). Within the funding routine:

1. Every organisation is paid its income. X-COM is credited the **old** income F first (file
   `0xF6AB2`), **unless its balance is already ≥ 2 000 000 000** (file `0xF6AA8`:
   `CMP EAX,0x77359400; JGE` skips the credit; it does not saturate).
2. The Government also receives its civilian tax (file `0xF6B08`) before any cap is computed.
3. If the cutoff latch is clear, the routine sums the eight categories separately: W, this week
   (file `0xF6EAB`), and H, the accumulated previous weeks (file `0xF6EBA`). It never adds W to H
   before the cutoff test.
4. **Cutoff** if the Government's relation to X-COM is below −50 (file `0xF6E9C`, signed byte
   `[Government][X-COM]`, not the reverse) **or H < −2400** (file `0xF6FBF`, `CMP ECX,-2400`;
   ECX is H). On cutoff: the old-income credit is taken back unconditionally (file `0xF7090`),
   income = 0 (file `0xF709B`), and the latch word is set (file `0xF70A2`). The latch is saved and
   loaded with the game and has no clearing writer.
5. Otherwise the score tier adjusts F (table below). Next income =
   `max(0, min(F + delta, G/2))`, where G is the Government's balance after its own payments
   (half computed at file `0xF6FD5`). With G/2 ≤ 0 the delta is cleared and the cap is −F
   (file `0xF7025`). The result is stored as X-COM's income (file `0xF7083`) and the Government is
   debited **that new income** (file `0xF70B7`). The credit to X-COM (old F) and the Government's
   debit (new income) differ whenever funding changes; that is the original's accounting.
6. Salaries and upkeep are deducted (finance screen, file `0xF70CD`; committed at file `0xF70DD`),
   also in weeks after funding has ended.

A first bad week therefore cuts the income but cannot end funding: H is still the old total. The
following Monday tests H including that week, however well the intervening week went.

## Tier table (file `0xF6EC7`–`0xF6FA4`)

Sequential independent comparisons with strict inequalities. The most extreme matching tier wins,
and every tier divides the unchanged F (signed division, truncating toward zero).

| Week score W | Delta |
|---|---|
| W > 12 800 | F / 4 |
| 6 400 < W ≤ 12 800 | F / 5 |
| 3 200 < W ≤ 6 400 | F / 8 |
| 1 600 < W ≤ 3 200 | F / 12 |
| 800 < W ≤ 1 600 | F / 16 |
| 400 < W ≤ 800 | F / 20 |
| 0 ≤ W ≤ 400 | 0 |
| −400 ≤ W < 0 | −F / 15 |
| −800 ≤ W < −400 | −F / 10 |
| −1 600 ≤ W < −800 | −F / 5 |
| W < −1 600 | −F / 4 |

Nothing in the funding block or the upkeep computation reads difficulty (stored at file `0x142340`).
Difficulty sets only the starting balance (140 000 − 10 000·d) and income (92 000 − 3 000·d),
which `data/difficultyN_patch/gamestate/organisations.xml` already match.

## Upkeep (file `0xFB114`)

Salary: 200 × (pay byte + 1) per active X-COM person; the ordinary templates give 600 and 800,
which `agent_salary` already matches. Maintenance: occupied bases, facilities 1..33, **only when the
facility's construction byte is 0** (file `0xFB211`). Construction stores the remaining build time
in that byte, so a facility still being built costs nothing.

## The eighth score category: Alien Buildings Destroyed

The category labels (group `0x25`) are Tactical Missions, Research Completed, Alien Incidents in
City, UFOs Shot Down, X-COM Craft Shot Down, UFO Incursions, Damage to City, **Alien Buildings
Destroyed** (item `0x9B`). After a won alien-city building mission, the battle-result routine
(file `0x115950`) adds category 7 (file `0x1159E4`) with the value read from a ten-entry int32 table
(file `0x191B7C`). The index is the word at offset `0xAA` of the 226-byte building record
(file `0x1159CE`–`0x1159D8`), which is the `.bld` entry's `function_idx`: the alien-building
name-table order, not the `.bld` entry order.

| Function index | Building | Score |
|---|---|---|
| 0 | Incubator Chamber | 400 |
| 1 | Spawning Chamber | 400 |
| 2 | Food Chamber | 300 |
| 3 | Megapod Chamber | 500 |
| 4 | Sleeping Chamber | 300 |
| 5 | Organic Factory | 250 |
| 6 | Alien Farm | 250 |
| 7 | Control Chamber | 350 |
| 8 | Maintenance Factory | 300 |
| 9 | Dimension Gate Generator | 500 |

This is separate from the battle's own tactical score.

## What OpenApoc does with this

- `GameState::weeklyPlayerUpdate` follows steps 1–6, including the 2-billion credit guard and the
  asymmetric credit and debit, and records a `FundingAssessment` for the report screens.
  `WeeklyFundingScreen` renders that record instead of recomputing it from balances the transfer
  has already changed. It shows the termination reason in the week funding is cut, which it
  previously skipped.
- The week's score rolls over in the simulation, not in the report screen, so the week resets
  whether or not the screen is shown, also after funding has ended.
- Upkeep skips facilities with `buildTime > 0`; the finance sheet matches.
- `GameScore::alienBuildingsDestroyed` is serialized, counted in totals and shown on the score
  sheet. `BuildingFunction::destroyedScore` carries the table above
  (`data/common_patch/gamestate/building_functions.xml`) and the RaidAliens win in `battle.cpp`
  credits it.
- `tests/test_weekly_funding.cpp` checks the documented cases. The cap and debit, cutoff timing,
  2-billion guard and unfinished-upkeep cases fail on the previous implementation.

OpenApoc keeps a running lifetime total alongside the week, rather than the original's
previous-weeks accumulator; H is `totalScore − weekScore` at the time of the assessment.
