# OpenApoc parity guide

Every gap between the original X-COM: Apocalypse binaries and this fork that is **still open on
`khallmark/FixShitUp`**, with the work needed to close each one.

Extends [openapoc-gap-matrix.md](openapoc-gap-matrix.md) (what the gaps are) and
[next-implementation.md](next-implementation.md) (which are next). This file is the *how*.

---

## 0. Prime directive

> **A row is not done until the lock test fails without the change. Items with no recovered
> constant or consumer must not have one invented.**
> — [next-implementation.md](next-implementation.md)

This is the constraint that shapes every entry below, and it is why most of this guide is not
C++ instructions. The project has spent significant effort *removing* invented numbers
(`micronoidRainChance`, the role-8 arrival RNG, the always-zero retreat formula). Re-adding a
plausible guess to close a row is a regression even when it makes the game feel better.

So each gap is classified by **what is actually blocking it**:

| Class | Meaning | The work is |
|---|---|---|
| **A** | Evidence exists, or none is needed. | Write code and a lock test. |
| **B** | Blocked on reverse engineering. A constant, table or consumer is not yet bound. | Ghidra first ([§2](#2-workflow-b--binding-a-constant)), then code. |
| **C** | No evidence exists in the binaries at all — the feature is prior-art or folklore. | A product decision, not an implementation task. |

**Do not promote a Class B row to Class A by picking a number that looks right.** If the binding
work fails, the correct outcome is that the row stays open.

### Run results — `khallmark/parity-implementation`

R&D and implementation run of 2026-08-24. Raw verdicts in [findings/](findings/); run log in
[findings/STATUS.md](findings/STATUS.md).

| Item | Verdict | Outcome |
|---|---|---|
| **V2** ground-vehicle order defect | **FIXED** | Root cause found in `VehicleMission::setPathTo`: a severed road yields a *non-empty but short* path that fell through both give-up branches — near targets crashed an undamaged vehicle, far targets looped forever. Lock test on the real extracted city. Suite 31/31. |
| **F1** hazard spread RNG | **BOUND** | `FUN_0001eee8` + `FUN_0007b0d0` decompiled, neighbour offset table recovered. `HAZARD_SPREAD_CHANCE` is **not a percent** — it is `RNG(0..10) + inherited baseline` vs a per-map-part resistance byte. Resistance values not yet decoded, so the constant cannot be deleted yet. |
| **B5** enzyme | **PARTIAL** | Structural hypothesis **confirmed**: the tile overlay byte carries ≥3 parallel hazard types over one structure with a shared placement engine. Fire is type 2. Type 1 vs 3 (Enzyme vs Gas/Smoke) not recoverable — dispatch variable has zero static xrefs. **Not guessed.** |
| **K1** cloak | **NOT BOUND** | No reader of the `agent_general_data` type field found. Raised a separate audit item — see below. |
| **O1** bribe / rift | **NOT BOUND** | The binary's one relation-adjustment primitive `FUN_0005faf0` was walked to its roots; none touch an org funds field. Stays prior-art. |
| **O2** cargo seize | **PARTIAL** | Org `+8` **confirmed a funds field, not relation**. Event-type → cargo mapping still unbound; there is no path from `Building::updateCargo`'s seize check into any of the four event types. `Cargo::seize` still must not be wired from it. |
| **M1** city music | **CONSUMER BOUND, TRIGGER NOT** | A real traced path reaches the tension-tier state machine from city-side mission completion — but that machine already runs every tick, so the call may be a redundant re-evaluate, and tier 3 (which holds `ACTION.RAW`) has no bound driver. **Not wired.** |
| **B1** cover metric | **RE-OPENED** | First verdict rested on an invalid control; see below. Agent resumed with structural entry. |
| **C1** umbilical · **C4** Apocalypse attack | **CLOSED** | Confirmed absent from *both* binaries. |
| **C3** late-campaign bombing | **CLOSED** | No trigger; escalation already explained by the weekly growth and preference tables. |
| **C2** mushrooms | **RECLASSIFIED** | Objective *mechanic* already works. Residual: ten TACP briefings unextracted. |

Two audit items this run produced, both about **existing** claims rather than open rows:

1. **TACP string-anchoring gives false negatives.** `0x2DE000`–`0x2E2FFF` is a packed
   variable-length pool whose entries can never carry a direct xref; `0x2F2000`–`0x2F3400` is a
   fixed-stride asset-name table whose entries can. Using one as a control for the other produced a
   wrong `NOT BOUND`. See [findings/METHOD-tacp-string-regions.md](findings/METHOD-tacp-string-regions.md).
   **Every future TACP negative must name the structural method exhausted, not cite absent xrefs.**
2. **The Mind Shield citation does not resolve.** The gap matrix records Mind Shield as
   `implemented / high` on `TACP FUN_0009b780 @ 0x9B780`. That cites a **Ghidra VA twice** rather
   than a file offset, contrary to this folder's own rule, and `0x9B780` lands mid-function inside
   `FUN_0009b058` in the bound project. The *behaviour* (+30, cap 200) is not disputed here — the
   *evidence* is unverifiable as written. **Re-verify and restate the citation as a file offset.**

### Status at a glance

The gap matrix has **52 rows; 32 are clean**. The 20 open rows expand to **23 work items** here,
because three matrix rows split (the fire row separates from enzyme; the incursion row separates the
mission counter from base exposure) and two Class C items come from
[subsystems/city-alien-dimension.md](subsystems/city-alien-dimension.md) rather than the matrix.

| Class | Items | IDs |
|---|---|---|
| **A — implementable now** | 4 | A1 multi-tile units · A2 psionics parity · A3 TU-reservation parity · A4 attack-priority parity |
| **B — blocked on RE** | 15 | B1 cover · B2 evasive · B3 wounded penalty · B4 wounded medkit · B5 enzyme · F1 fire remainder · K1 cloak · G1 dead gadgets · V1 vehicle dodge · V2 ground engagement · O1 bribe/rift · O2 cargo seize · M1 city music · U1 mission counter · U2 base exposure |
| **C — no evidence** | 4 | C1 umbilical · C2 mushroom feedback · C3 late bombing · C4 Apocalypse attack |

Class B dominates, and that is the honest shape of the remaining work: **this is a reverse-
engineering backlog with a small amount of C++ attached**, not the other way round.

---

## 1. Workflow A — closing a code-only gap

1. **Write the lock test first**, in `tests/`, and confirm it **fails**. A test that passes before
   the change proves nothing.
2. Register it in [tests/CMakeLists.txt](../../tests/CMakeLists.txt) with `add_openapoc_test(name)`,
   or `add_openapoc_test(name ARGS ${CMAKE_SOURCE_DIR}/data/mods/base/data/submods/org.openapoc.base/difficulty0 ${CMAKE_SOURCE_DIR}/data/mods/base/base_gamestate)`
   when the test needs a loaded campaign.
3. Use [tests/test_helpers.h](../../tests/test_helpers.h): `applyDeterministicTestConfig()`,
   `loadStartedGameState(state, common, gamestate)`, `TEST_CHECK`, `TEST_REQUIRE`,
   `runTestSuite({{"name", fn}, …})`.
4. Implement.
5. Re-run; the test must now pass and must still fail if you revert the change.
6. Update [openapoc-gap-matrix.md](openapoc-gap-matrix.md) and re-run
   `python3 tools/regen_compare_report.py`.

## 2. Workflow B — binding a constant

This is the bottleneck for eleven rows. The lab is the sibling repo
`/Users/khallmark/Desktop/Code/OpenSource/OpenApoc-og-research`; **never copy EXEs, ISOs or
`.rep` databases into this tree** ([README.md](README.md)).

```bash
cd /Users/khallmark/Desktop/Code/OpenSource/OpenApoc-og-research
./scripts/ghidra_env.sh --print                      # verify JDK 21 + Ghidra 12.1.3
./scripts/import_le.sh canonical/TACP.EXE            # bound LE, community LX loader
./scripts/import_le.sh canonical/UFO2P.EXE 0x13EE80  # optional file-offset dump
```

Rules that are easy to get wrong:

- Use `-processor x86:LE:32:default -cspec gcc` so the LX loader wins. **Do not** unbind the
  executable — every extractor offset in this repo is a *bound-file* offset.
- **Do not** force analysis of the DOS4GW stub.
- Ghidra's displayed addresses are **virtual addresses, not file offsets.** Every citation must
  carry binary + generation + file offset. Use `labels/ufo2p_rebase.csv` / `tacp_rebase.csv` and
  `labels/offset_to_va.txt` to convert.
- The ISO **non-4** pair is extractor-canonical. The Steam depot ships the **`4`** build under the
  un-suffixed names. Confirm at the non-4 offset, then relocate on the 4 build —
  [exe_slide.h](../../tools/extractors/common/exe_slide.h) maps UFO2P `0xdbd3b41d` → `+0xE00` and
  TACP `0x3ec9c268` → `−0x2200`, but **never assume a global slide**; `crew_ufo_downed` is the
  known exception.

The binding sequence for any unknown:

1. **String first.** `grep` `export/strings/TACP_strings.txt` (or `UFO2P_strings.txt`). A printable
   label is the strongest cheap evidence. **Absence is also evidence** — it is why C1/C2 are Class C.
2. **Xref the string.** If bound xrefs are empty, the string is UI copy and proves nothing about a
   consumer. Several rows died here already (`Rules of engagement` `0x152D10`, `Evade Fire`,
   `Reload time:` `0x151086`).
3. **Find the reader, not the table.** A table with no consumer cannot be wired. The recurring
   failure mode in this project is finding plausible bytes and guessing their meaning.
4. **Decompile the consumer** with `scripts/DecompileSites.java` / `QueryFunctions.java`, adapting
   an existing `Query*.java` — they are short and templated.
5. **Confirm on both generations** before writing an extractor.
6. **Write the extractor** in `tools/extractors/`, with the struct in `tools/extractors/common/`.
7. **Do not** put extracted list tables under `data/common_patch/gamestate/` — `loadGame` *appends*
   vectors, which silently doubles fleet and spawn counts. Reference dumps belong in
   [exe-tables/](exe-tables/).

**Evidence ranking, strongest first:** `table` → `string` → `xref` → `decompiler` → `prior-art` →
`openapoc-todo`.

---

## 3. Class A — implementable now

### A1 · Multi-tile (large) unit pathing and drawing

*Gap matrix: "Multi-tile unit path / draw — missing (prior-art only), medium. Evidence:
openapoc-todo; no TACP path/draw table recovered." Issue #265.*

**This is the one Class A row that is real engine work rather than a test.** It needs no recovered
constant: large units are an OpenApoc capability gap, not a fidelity-of-numbers gap. Megaspawn and
Multiworm are `bodyType->large`, and the code paths that handle them are marked provisional.

**Current behaviour**

- `BattleUnit::isLarge()` — [battleunit.h:641](../../game/state/battle/battleunit.h#L641).
- [battleunit.h:677](../../game/state/battle/battleunit.h#L677) carries
  `// FIXME: This likely won't work properly for large units`.
- [battleunit.cpp:636](../../game/state/battle/battleunit.cpp#L636) and
  [:1045](../../game/state/battle/battleunit.cpp#L1045) apply ad-hoc "offset search for large units
  as they can get caught up in ground".

**Steps**

1. **Enumerate the breakage first.** Add a scratch test that spawns a large unit and drives it
   through: path across a 1-tile gap, path through a door, occupy/vacate, LOS from each occupied
   tile, projectile collision against each occupied tile, shadow/draw order. Record which fail.
   Do not start fixing before this list exists — the FIXME is vague and the real defects may be
   fewer or different than expected.
2. **Make occupancy explicit.** A large unit occupies a 2×2×2 block. Introduce
   `BattleUnit::occupiedTiles()` returning the full set, and route every
   `tileObject->setPosition` / occupancy query through it rather than through the origin tile.
3. **Pathfinding.** In `game/state/tilemap/`, the cost function must reject a step whose destination
   *block* is not entirely passable, not merely its origin tile. This is where the "caught up in
   ground" offsets are compensating for a wrong query — remove them once the block query is correct,
   and confirm the offsets were the only thing holding it together.
4. **LOS and targeting.** Visibility to a large unit should succeed if any occupied tile is visible;
   line-of-fire from one should originate at the unit's centre, not its origin corner.
5. **Draw order.** Large units must sort against scenery per occupied tile.

**Lock test** — `tests/test_battle_large_unit.cpp`, registered with the two gamestate `ARGS`:

- `large_unit_occupies_block` — all tiles of the 2×2×2 block report the unit.
- `large_unit_path_rejects_narrow_gap` — a 1-tile corridor is not pathable.
- `large_unit_los_any_tile` — visibility succeeds from any occupied tile.
- `large_unit_no_ground_snag` — a straight walk across flat floor produces no vertical correction.

**Do not** invent a TACP "large unit table". None is recovered, and none is needed — this is engine
geometry, not a game-balance constant.

### A2 · Psionics timing parity

*Gap matrix: "implemented (parity unverified), medium."*

The numbers are already recovered — in prior-art, not the binary — and are implemented. What is
missing is anything that **holds them in place**. `TICKS_PER_PSI_CHECK = TICKS_PER_SECOND / 2`
([battleunit.h:41](../../game/state/battle/battleunit.h#L41)) is exactly the kind of constant that
a later TPS refactor silently breaks.

From [psionics.txt](../../tools/extractors/docs/psionics.txt), applied each half-second:

| Attack | Initial cost | Upkeep / sec |
|---|---|---|
| Control | 32 | 8 |
| Panic | 10 | 4 |
| Stun | 16 | 10 |
| Probe | 8 | 6 |
| *(regen)* | — | +1 |

**Steps**

1. Write the lock test below and confirm it passes today — this is the rare case where a passing
   test is the deliverable, because it is a **freeze**, not a fix.
2. Assert against the `psionics.txt` values by name, with the file cited in a comment, so a future
   editor sees the provenance.
3. Add an assertion that `TICKS_PER_PSI_CHECK * 2 == TICKS_PER_SECOND`, so the half-second cadence
   fails loudly under a TPS change rather than drifting.

**Lock test** — `tests/test_psionics.cpp`: `psi_costs_match_prior_art`,
`psi_upkeep_per_second`, `psi_regen_one_per_second`, `psi_check_cadence_is_half_second`.

**Do not** change any of these numbers to "feel better". They are prior-art, marked as such; the
test's job is to make a future change deliberate.

### A3 · TU reservation parity

*Gap matrix: "implemented (parity unverified), high."*

TACP strings `TUs reserved for kneeling` / `aimed` / `snap` / `auto` confirm the four modes exist;
the reserve logic is in `battleunit.cpp`. Nothing pins the reserved amounts.

**Steps**

1. `tests/test_tu_reservation.cpp`, driving a unit with each `ReserveMode`.
2. Assert that a unit which has reserved for aimed fire is refused a move that would drop TUs below
   the aimed-shot cost, and permitted the move at exactly cost + 1.
3. Assert kneel reservation composes with weapon reservation rather than replacing it.

**Do not** attempt to bind reservation costs to TACP — the four strings have empty bound xrefs.
This row is closed by *verification*, and its confidence stays `medium` regardless.

### A4 · Vanilla attack-priority parity

*Gap matrix: "implemented, medium — prior-art ai.txt + unitaivanilla.cpp (CTH × DAMAGE / TIME)."*

Same shape: implemented from [ai.txt](../../tools/extractors/docs/ai.txt), unverified.

**Steps**

1. `tests/test_unit_ai_priority.cpp` constructing a unit with two weapons and a known target.
2. Assert the higher `CTH × DAMAGE / TIME` weapon is chosen, and that the ordering inverts when
   the factors invert. Ordering is the contract; the absolute score is not.
3. Assert AOE weapons are rejected when a friendly is inside the blast radius (`ai.txt`, Vanilla AI).

---

## 4. Class B — blocked on reverse engineering

Each entry gives the binding target first. **The RE step is the deliverable**; the code that
follows is usually small.

### B1 · Cautious / Normal cover + potshots — *the highest-value gap in the game*

*Gap matrix: missing (prior-art only), **high** confidence. Issue #265.
[next-implementation.md](next-implementation.md) item 1.*

**Why it matters more than its row suggests.** Because cover does not work, `Normal`, `Cautious`
and `Evasive` all collapse to kneel-or-prone. Aliens close and shoot; they never break line of
sight or trade from a defended position. Every tactical difficulty judgement made against current
OpenApoc — squad size, armour thresholds, casualty budgets, and the whole ground doctrine in
[campaign-plan.md §12.2](../campaign-plan.md#122-ground-combat) — is measured against a
meaningfully softer opponent than the 1997 game. **Closing this re-tunes the game.**

**Current behaviour.** `UnitAIHelper::getTakeCoverMovement`
([unitaihelper.cpp:142](../../game/state/battle/ai/unitaihelper.cpp#L142)) rolls the take-cover
chance — `33% × sqrt(visible enemies)`, assuming 3 when none are seen — and then returns `nullptr`
with the comment *"Cover-tile search is not implemented. Callers fall through to kneel / prone."*
Three call sites depend on it: [unitaibehavior.cpp:69](../../game/state/battle/ai/unitaibehavior.cpp#L69),
[unitaivanilla.cpp:603](../../game/state/battle/ai/unitaivanilla.cpp#L603) and
[:676](../../game/state/battle/ai/unitaivanilla.cpp#L676).

**Blocker.** No printable `cover` or `potshot` in TACP. There is no recovered metric for *what makes
a tile good cover*.

**RE steps**

1. TACP printable strings that **are** bound: `Cautious mode`, `Aggressive mode`, `Kneel down`,
   `Reserve TUs for kneel`, `Unit under fire` (`0x2E0438`), `Unit has gone berserk` (`0x2DF134`).
   Start at `Cautious mode` and `Unit under fire` and walk **callers**, not the string.
2. The target is a function that, given a unit and a threat direction, **scores or selects a tile**.
   Signature to look for: takes a unit pointer and a position/direction, loops over neighbouring
   tiles, and compares an accumulated per-tile integer.
3. The per-tile input is most likely an existing map-part field. Cross-check the battle map-part
   tables in [tools/extractors/common/](../../tools/extractors/common/) for an unmapped byte —
   candidates are any `unknown*` on the map-part struct that correlates with wall/solidity.
4. If a scoring function is found, capture: the metric, the search radius, the tie-break, and
   whether the unit kneels/prones on arrival.
5. If **no** scoring function exists after this walk, record that negative result in the gap matrix
   and **leave `getTakeCoverMovement` returning null**. A "reasonable" cover heuristic would be an
   invented mechanic wearing the original's name.

**Implementation, once the metric is bound**

1. Add `UnitAIHelper::scoreCoverTile(GameState&, BattleUnit&, Vec3<int> tile, Vec3<float> threat)`
   returning the recovered metric.
2. In `getTakeCoverMovement`, after the existing chance roll, search tiles within the recovered
   radius, reject tiles that cost more TUs than the unit has, and select the best score; return an
   `AIMovement` with `movementMode` per `ai.txt` — **Cautious prones if it can, Normal kneels.**
3. Potshots are a separate behaviour: after arriving in cover, fire and return to cover. Model as an
   `AIMovement` + `AIAction` pair; do not fold it into the movement.

**Lock test** — extend [tests/test_tactical_ai_retreat.cpp](../../tests/test_tactical_ai_retreat.cpp),
whose current lock is *"retreat + panic run only"*:

- `cautious_unit_takes_cover` — a Cautious unit under fire with a scoring tile available returns a
  non-null movement toward it.
- `normal_unit_kneels_without_cover` — with no scoring tile, falls back to kneel (today's behaviour).
- `cover_prefers_higher_metric` — given two candidates, the higher-scored tile wins.

**Do not** implement "move away from the enemy" or "move behind the nearest wall" as a stand-in.
Both are inventions, and both would make the row *look* closed.

### B2 · Evasive seek-cover

*Gap matrix: missing (prior-art only), medium.*

Strictly downstream of B1 — same missing metric, plus `Evasive` prones where `Normal` kneels
([ai.txt](../../tools/extractors/docs/ai.txt)). **Do not start before B1 lands.** There is no
printable `evasive`, so even the mode name is prior-art.

Once B1 exists: pass the cautious flag through so `Evasive` calls
`getTakeCoverMovement(state, u, /*forced*/ true)` and sets `MovementMode::Prone` on arrival when
`isMovementStateAllowed(Prone)`.

**Lock test** — `evasive_unit_prones_in_cover` in the same file as B1.

### B3 · Wounded move / shoot penalty

*Gap matrix: missing (prior-art only), medium.*

**Evidence.** TACP string `Unit critically wounded`. Fatal wounds already exist and tick:
`BattleUnit::addFatalWound` ([battleunit.cpp:1610](../../game/state/battle/battleunit.cpp#L1610)),
applied on `TICKS_PER_WOUND_EFFECT` — itself carrying
`// FIXME: Seems to correspond to vanilla behavior, but ensure it's right`
([battleunit.h:42](../../game/state/battle/battleunit.h#L42)).

**Blocker.** No recovered TU or accuracy constants. The original penalised a wounded unit; the size
of the penalty is unknown.

**RE steps**

1. Xref `Unit critically wounded` (TACP). If bound xrefs are empty, it is a message-only string —
   record that and stop.
2. Otherwise find the wound counter on the unit struct and locate **readers** other than the
   damage-over-time tick. A reader inside the TU or accuracy computation is the target.
3. Capture whether the penalty is per-wound or per-wounded-body-part, and whether it is a
   subtraction or a percentage.

**Implementation, once bound**

1. Apply in `BattleUnit::getMaxTU()` / the accuracy path, not at the call sites, so every consumer
   inherits it.
2. Body-part-specific effects (leg wounds → movement, arm wounds → accuracy) must come from the
   binding, not from plausibility.

**Lock test** — `tests/test_battle_wounds.cpp`: `wounded_unit_tu_penalty`,
`wounded_unit_accuracy_penalty`, `healed_unit_penalty_clears`.

### B4 · Wounded medkit use in cover

*Gap matrix: missing (prior-art only), medium.*

Depends on **both** B1 (cover) and B3 (knowing a unit is impaired). The AI has no heal path at all.

**Blocker.** No AI cover-heal path and no constants — specifically, no recovered threshold for
*when* a unit decides to heal instead of fight.

**Implementation, once B1 and B3 land**

1. In `UnitAIBehavior::think`, before the cover check: if the unit has fatal wounds, carries a
   Medi-kit, and is in cover, emit an `AIAction` using the item.
2. `BattleUnit::useItem` already handles `Type::MediKit`; reuse it rather than adding a second path.
3. The decision threshold must be recovered. Until then this row stays open even if the plumbing
   is written.

**Lock test** — `wounded_ai_heals_in_cover` in `tests/test_battle_use_item.cpp`.

### B5 · Entropy Enzyme

*Gap matrix: missing (prior-art only), **high**. [next-implementation.md](next-implementation.md)
item 2.*

**Evidence.** TACP strings `Entropy Enzyme`, RAW path `GASEXPLS`.
[version01readme.txt](../../tools/extractors/docs/version01readme.txt) lists enzyme as an
approximation.

**Current behaviour — both constants are invented.**

- `#define HAZARD_SPREAD_CHANCE 10 // out of 100` —
  [battlehazard.h:14](../../game/state/battle/battlehazard.h#L14), directly under
  `// FIXME: This is a MADE UP VALUE!` on [line 12](../../game/state/battle/battlehazard.h#L12).
  Consumed at [battlehazard.cpp:396](../../game/state/battle/battlehazard.cpp#L396) and
  [:425](../../game/state/battle/battlehazard.cpp#L425).
- `TICKS_PER_ENZYME_EFFECT = TICKS_PER_SECOND / 9` —
  [battleunit.h:45](../../game/state/battle/battleunit.h#L45).

**RE steps**

1. The fire work already bound a large part of this subsystem — `FUN_0007c110` (item resist),
   `FUN_0007ad94` (type-2 overlay decode), `FUN_0007ae18` (27-byte power lookup),
   `FUN_0007b3dc` (stage/extinction). **Enzyme almost certainly shares the hazard tick.** Start by
   checking whether enzyme is a *type* on the same structure rather than a separate system.
2. The unbound generic path is `FUN_0001eee8` / `FUN_0007b0d0` (placement / spread RNG). Bind those
   and `HAZARD_SPREAD_CHANCE` disappears as a concept — spread becomes a recovered neighbour
   selection, not a flat percentage.
3. Enzyme's distinguishing behaviour is armour damage. Look for a reader of the armour value on the
   hazard-contact path, separate from the health path.

**Implementation, once bound**

1. Delete `HAZARD_SPREAD_CHANCE` outright. Do not retune it.
2. Route generic hazards through the recovered placement/spread functions, as fire already is via
   `Battle::updateFireScheduler`.
3. Replace `TICKS_PER_ENZYME_EFFECT` with the recovered cadence.

**Lock test** — extend [tests/test_battle_hazard.cpp](../../tests/test_battle_hazard.cpp)
(existing locks: `fire_hazard_item_resist`, `fire_overlay_power_progression`,
`fire_overlay_terrain_threshold`, `fire_scheduler_state_machine`) with `enzyme_armour_damage`,
`enzyme_tick_cadence`, `hazard_spread_uses_recovered_rng`.

**Do not** tune `HAZARD_SPREAD_CHANCE` to a nicer number. It is not a parameter; it is a placeholder
that should cease to exist.

### F1 · Fire — the remaining unbound quarter

*Gap matrix: **partial**, high. [next-implementation.md](next-implementation.md) item 2.*

Most of fire is bound and is some of the best work in the tree —
`Battle::updateFireScheduler` implements real-time `FUN_0007b7f8` (one vanilla iteration per four
OpenApoc ticks; `(mapY × mapZ) / 0x48` complete X rows via persistent Y/Z cursors; then
check/reset/run/increment the `0x24` item-contact counter), and turn-based round wrap runs
`FUN_000b8c50`'s 400-iteration batch with item contacts and no unit contacts.

**Still unbound:** unit fire intensity · generic placement/spread RNG (`FUN_0001eee8` /
`FUN_0007b0d0`) · `HAZARD_SPREAD_CHANCE` (shared with B5) · `ttl` and AOE expansion ticks.

**RE steps**

1. Unit intensity — find the reader of the per-unit fire value distinct from
   `TICKS_PER_FIRE_EFFECT` ([battleunit.h:46](../../game/state/battle/battleunit.h#L46)).
2. Neighbour selection — `FUN_0007b0d0` is the target. Capture the neighbour ordering and the RNG
   call, then confirm `FUN_0001eee8` is the generic placement entry.
3. `unused01` at catalog `+0xF` allocates a **type-4 doodad** on burn-out
   (`FUN_000598d4` @ file `0xB4378` → `FUN_00058e84` @ `0xB3928`). It is a **visual slot with no
   damage reader** — do not wire damage from it.

**Do not** invent placement seed 10, map `age`/`power`, or an `INC` cadence. Overlay `INC` is
`FUN_0007b3dc` on the tile overlay byte at `0xF3738` and nothing else.

### K1 · Personal Cloaking Field

*Gap matrix: missing (prior-art only), **high**.*

**Current behaviour.** `CLOAK_TICKS_REQUIRED_UNIT = TICKS_PER_SECOND * 2`
([battleunit.h:64](../../game/state/battle/battleunit.h#L64)) — the comment cites *"Yataka Shimaoka
on forums"*, i.e. **community folklore, not the binary**. The accumulator is at
[battleunit.cpp:2114](../../game/state/battle/battleunit.cpp#L2114): it increments while a
`CloakingField` is in either hand and resets to zero otherwise.
`BattleUnit::useItem` returns `false` for `Type::CloakingField`
([battleunit.cpp:5144](../../game/state/battle/battleunit.cpp#L5144)) — the item is passive.

**Blocker.** Cloak tick thresholds unbound. TACP type `0x0a` is extracted; the *timing* is not.

**RE steps**

1. Xref `Personal Cloaking Field` in TACP. Note the gap matrix already records this string as
   having **empty bound function xrefs** — expect to work from the equipment type instead.
2. Find readers of equipment type `0x0a` on the unit-update path. The target is the comparison
   against an accumulator — the original's equivalent of `CLOAK_TICKS_REQUIRED_UNIT`.
3. Capture both edges: how long inaction takes to *gain* concealment, and what *breaks* it.
   OpenApoc currently breaks it only on unequip; the original very likely breaks it on firing.

**Implementation, once bound**

1. Replace the constant, and delete the forum attribution comment.
2. If firing breaks cloak, reset the accumulator in the fire path, not in `updateStateAndStats`.

**Lock test** — `tests/test_battle_cloak.cpp`: `cloak_engages_after_recovered_delay`,
`cloak_breaks_on_fire`, `cloak_resets_on_unequip`.

### G1 · Dead gadgets — MultiTracker, Vortex Analyzer, Disruptor Shield

*Gap matrix: "implemented (Mind Shield locked; other gadgets prior-art only), high."
[next-implementation.md](next-implementation.md) item 6.*

Mind Shield is bound (+30, cap 200, TACP `FUN_0009b780` @ `0x9B780`). The rest are inert:
`BattleUnit::useItem` returns `false` for `MultiTracker`, `VortexAnalyzer`, `DisruptorShield`,
`CloakingField`, `AlienDetector`, `StructureProbe` and `DimensionForceField`
([battleunit.cpp:5141](../../game/state/battle/battleunit.cpp#L5141)).

**Blocker.** No confirmed consumer for any of them.

**RE steps.** For each type in turn: locate the extracted type id in
[aequipment.h](../../tools/extractors/common/aequipment.h), then search TACP for readers of that id
on the unit-update or UI-refresh path. **A type id with no reader is a dead gadget in the original
too** — that is a legitimate finding, and it should be recorded in the matrix rather than papered
over with a UI effect.

**Do not** invent a scan radius, a detection UI, or a shield regen formula.

### V1 · Vehicle attack-mode dodge (100 / 80 / 50 / 10)

*Gap matrix: "implemented (voxel dodge extracted; attack ladder unbound), high."
[next-implementation.md](next-implementation.md) item 3.*

**Current behaviour.** [vehicle.cpp:170](../../game/state/city/vehicle.cpp#L170) carries
`// FIXME: Read vehicle engagement rules, instead for now chance to dodge is flat
100% / 80% / 50% / 10% depending on behavior`, then a switch mapping `AttackMode` →
`Evasive 100 / Defensive 80 / Standard 50 / Aggressive 10`.

**Blocker.** `Rules of engagement` at `0x152D10` has **empty bound xrefs**, and `Evade Fire` xrefs
are empty too. There is no recovered engagement table.

**Critical trap, already documented:** `loftemps_index` at `+0x28` (sample `79 00` @ `0x189CB4`) is
**not** a dodge percentage. [hexa.txt](../../tools/extractors/docs/hexa.txt) labels it "chance to
evade bullets"; that label is wrong. It is the loftemps voxelmap index, used by
[extract_vehicles.cpp](../../tools/extractors/extract_vehicles.cpp) for misaligned voxelmaps.
**Do not map values 119–151 onto percentages** — that is the single most tempting wrong move in
this row, and `next-implementation.md` calls it out by name.

**RE steps.** Find a reader that branches on the vehicle's attack-mode field and produces a
comparison value. Absent that, the four numbers stay as they are, flagged.

**Lock test.** None until an engagement table exists — a test over invented constants only freezes
the invention.

### V2 · Ground vehicle engagement / large footprint

*Gap matrix: "implemented (lanes + footprint; engagement unbound), high." Issue #785.*

Lanes and footprint work (`connection[dir]` + occupancy; `size_x`/`size_y` from `vehicle_data`).
The engagement table is the same unbound `Rules of engagement` as V1.
[version01readme.txt](../../tools/extractors/docs/version01readme.txt) warns that ordering ground
vehicles can crash — **that is a live defect and is independently fixable**: add a regression test
that issues a ground-vehicle move order across a destroyed road segment and asserts no crash and a
terminating path.

### O1 · Org-org bribe and diplomatic-rift dollar formulas

*Gap matrix: "implemented (table + raid snapshot order; formulas unbound), medium." Issue #996.
[next-implementation.md](next-implementation.md) item 5.*

The directed `organisation_starting_relationships` table is extracted, and raid ordering is correct
(raid uses `long_term − current` **before** `updateRelations` snapshots). The **coefficients** in
`Organisation::costOfBribeBy` and `diplomaticRiftOffer` are unbound.

**RE steps.** Find the reader that turns a relationship delta into a currency amount. Beware
`FUN_000941dc` @ file `0xF6880` — it is weekly **org budget fractions** (stride `0x1B6`, 27 orgs),
**not** the bribe formula, and not `EconomyInfo::update` either.

**Lock test** — extend [tests/test_diplomacy.cpp](../../tests/test_diplomacy.cpp) and
[tests/test_organisation.cpp](../../tests/test_organisation.cpp).

**Do not** invent a weekly-drift formula.

### O2 · `Cargo::seize` diplomacy

*Gap matrix: "implemented (refund + restock; seize diplomacy unbound), high."*

`Cargo::refund` credits the buyer only; the seller keeps the `settleMarketPurchase` credit
([vehicle.cpp:3990](../../game/state/city/vehicle.cpp#L3990)). `Cargo::seize` restocks the market
but its relation effect is a FIXME.

**The trap.** `FUN_000b32ac` @ VA `0xB32AC` / file `0x115950` is a **4-way event dispatcher**
(`[0x174024]`) that computes `worth × 50` into org `+8` plus a tiered extra
(`worth > 50 → −(worth−50)/20`; `worth ≥ 250 → −10 − (worth−250)/50`, floor `−50`) via
`FUN_0005faf0` @ file `0xC2194`. **It is not a cargo-seize entry.** Neither the event type → cargo
mapping nor whether `+8` is funds or relation is bound. `next-implementation.md` states plainly:
*do not wire `Cargo::seize` from `worth×50`.*

**RE step.** Bind `[0x174024]` event types to their sources. Until then the row stays open.

### M1 · City action music

*Gap matrix: "implemented (battle Action; city trigger unbound), high." Issue #618.
[next-implementation.md](next-implementation.md) item 4.*

Battle Action playlist is wired. UFO2P has `Action music`, `getnextmusic` and the `/MUSIC/GROUP_*`
RAW catalog, but **empty bound xrefs for the city combat mix** — catalog-only.

**RE step.** Find the city-side caller of the music selector. **Pass criterion: city mix only after
a bound consumer.** Triggering city action music on "a hostile vehicle is nearby" would be a
plausible invention with no evidence behind it.

### U1 · UFO mission counter `+0x171` transition

*Gap matrix: folded into "UFO mission / incursion tables", high.
[next-implementation.md](next-implementation.md) item 9.*

Nearly all of `UFO_mission_data` (45 × 42 @ `0x13DDFC`) is consumed: `follow_slot`, spawn XY,
`type_percent` × constitution, the `scatter > 50 → 10` clamp, and `+0x1B`.

**A correction worth repeating:** hexa's `building_function` label for `+0x1B` is **wrong**.
`FUN_0006da88` @ file `0xD030B` copies it to vehicle `+0x171`, and `FUN_0003a910` @ object-page file
`0x2A90F` decrements it whenever the UFO reaches a mission destination; zero advances target/mission
state. It is extracted as `mission_counter` — and incursion A1 proves values of 12, which exceed the
ten gate slots, so it cannot be a gate index.

**Unbound:** the exact OpenApoc transition when the counter hits zero, and `FUN_000588f8` @
object-page file `0x488F7`, which gates `+0x168` against constitution `+0x12e`.

**Lock test** — [tests/test_city_rules.cpp](../../tests/test_city_rules.cpp) already locks
`ufo_incursion_table`, `ufo_incursion_spawn_xy`, `ufo_incursion_follow_type`. Add
`ufo_mission_counter_transition` once bound.

**Do not** add a building-function filter, and do not invent an `acquireTargetBuilding` name filter.

### U2 · Base-exposure leftovers

*[next-implementation.md](next-implementation.md) items 8 and 10.*

Most of this subsystem is now bound and is worth reading as a model of how these rows get closed:
`+0x2BC` is the terminal word of each 0x2BE-byte, 16-entry X-COM Base runtime record, serialized as
`Base::knownToAliens` plus a reusable `Base::ufo2pSlot` allocated from the first hole. Normal UFO
deposit (`FUN_0005fddc`) exposes a base on inclusive `rand16(100) < 5`; alien movement
(`FUN_0006f7f8`) uses `< moved_count × 5`; every primary Subversion craft consumes inclusive
`rand16(15)` and cyclically scans the 16 persistent slots. The invented `micronoidRainChance`
takeover path was **removed**.

**Unbound:** the global override `DAT_000e0cc0` (which makes *all* active bases pass the exposure
predicate — its campaign lifecycle is unknown), and the event-dispatch full-transfer writer
`FUN_0006f738`, whose event types 1 and 4 are unmapped.

**Do not** restore an arrival percentage — that was the invented mechanic already deleted once.

---

## 5. Class C — no evidence exists

These four are in the matrix at **low** confidence with `missing (prior-art only)`. They come from
player-facing folklore and manuals, not from the binaries. **Each needs a product decision, not an
implementation plan** — and the honest default for all four is to close them as "not in the
original as described".

| ID | Feature | Evidence status |
|---|---|---|
| **C1** | Umbilical collapse | **No printable `umbilical` in UFO2P.** |
| **C2** | UFO mushrooms as next-week spawn feedback | ~~No printable `mushroom` in UFO2P~~ — **RECLASSIFIED to Class B.** UFO2P has none, but **TACP does**, at file `0x2E1468`. See [findings/C1-C4-no-evidence-items.md](findings/C1-C4-no-evidence-items.md). |
| **C3** | Large-UFO bombing after first alien-dimension entry | No recovered trigger table. |
| **C4** | City-wide "Apocalypse" attack after the control centre dies | No recovered trigger table. |

**C2 has been reclassified.** The matrix recorded "no printable `mushroom` in UFO2P" — true, but it
only searched UFO2P. **TACP has one**, at file offset `0x2E1468`: the Organic Factory Ufopaedia
entry, which describes embryonic UFOs growing on stems and ends *"All embryonic UFOs must be
destroyed."*

That is a **mission objective in the battlescape**, not cityscape feedback — so the row was also
filed under the wrong subsystem (`game/state/city`). OpenApoc's growth gate
`UFOGrowth::craftFactoryIntact` ([ufogrowth.cpp:51](../../game/state/rules/city/ufogrowth.cpp#L51))
keys on the **building** being alive; the original text implies the objective is the **embryos**,
which may carry their own completion condition. That gate is strategically decisive — see
[campaign-plan.md §3.7](../campaign-plan.md#37-the-organic-factory-shuts-the-tap-off) — so the
distinction matters. Investigation steps are in
[findings/C1-C4-no-evidence-items.md](findings/C1-C4-no-evidence-items.md). **Do not add a
cityscape mushroom visual.**

C3 and C4 are the only Class C rows that could reasonably be built as **declared original design**
rather than parity: both are late-campaign escalation, and both would need a new data table
(`docs/original-game/exe-tables/` is *not* the right home — a designed feature belongs in
`data/common_patch/`, clearly labelled as OpenApoc-authored). If that is done, it must be marked
**openapoc-todo / designed**, never `implemented`, so the matrix does not claim parity it does not
have.

---

## 6. Suggested order

Dependency-ordered, not value-ordered. Value is in the notes.

| # | Row | Class | Why here |
|---|---|---|---|
| 1 | A2 psionics, A3 TU reservation, A4 attack priority | A | Cheap. Freezes three `parity unverified` rows before a TPS or AI refactor moves them silently. |
| 2 | V2 ground-vehicle crash regression | A | A live crash, fixable without any binding. |
| 3 | **B1 cover metric** | B | **Highest value in the guide.** Gates B2 and B4, and re-tunes every tactical difficulty judgement. Do this before any balance work. |
| 4 | B5 + F1 hazard RNG | B | One binding session closes both; deletes `HAZARD_SPREAD_CHANCE`. |
| 5 | A1 multi-tile units | A | Self-contained engine work, no binding. Good parallel task. |
| 6 | K1 cloak | B | Small, isolated, replaces a forum-sourced constant. |
| 7 | B3 → B4 wounded | B | B4 needs B1 and B3. |
| 8 | U1, U2, O1, O2, G1, M1, V1 | B | Long-tail bindings; several may end as recorded negative results. |
| 9 | C1, C3, C4 | C | Confirmed absent from both binaries — close them. **C2 is no longer Class C**; it moves into the Class B queue with a real TACP string behind it. |

**Realistic expectation:** several Class B rows will end as *"no consumer exists in the original"*.
That is a successful outcome and should be written into the matrix as such. The failure mode this
guide exists to prevent is closing a row with a number nobody recovered.
