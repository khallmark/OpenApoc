# Playing OpenApoc through the harness

How to drive a full Novice campaign to victory with no human input and no cheating. Everything
here was learned by doing it and being wrong first; each entry says what actually happens, not
what the UI appears to offer.

**Status: victory not yet achieved.** The chain below is mapped end to end and every step up to
and including base construction is verified working in a live game. Manufacturing, vehicle
equipping, portal travel and the alien raids are specified but not yet driven. This file is
rewritten as steps are proven.

## Ground rules

No `CheatOptions`, no debug hotkeys, no writing to `GameState`. Only inputs a player could
produce: mouse events, key presses, and named actions that invoke the same `Control::click()` a
real press raises. Two tempting shortcuts are deliberately refused:

- `F1` in the city or battle latches `debugHotkeyMode` permanently; subsequent right-clicks fire
  debug commands and corrupt the campaign.
- `F10` on `BaseScreen` zeroes every facility's build time instantly. The campaign waits out
  construction instead.

Read-only `GS` queries are not cheating — they are how the driver checks its own work. Every one
of them exists because some action reported success while changing nothing.

## The rule that matters most

**A command returning `OK` means the command was delivered, not that the game did anything.**
Almost every defect in this driver has been the same shape: an action reports success, the game
ignores it, and the campaign runs for hours on a false premise. Verify every action against
engine state:

| Action | Verify with |
| --- | --- |
| Started research | `gs research` — `man_hours_progress` must *rise*, not merely be assigned |
| Ordered a recovery | `gs selected` — the craft's `mission=` must change |
| Won a battle | `gs battle` `player_won` — a debriefing appears for losses too |
| Hired someone | `gs agents` — `soldiers` / `agents_player` |
| Bought something | `gs stores` — and wait, purchases arrive as cargo days later |
| Built a facility | `gs facilities` — the `base=` list gains an entry with a build time |

## Victory, precisely

Win a `RaidAliens` battle against the one building with `victory=true`
(`battle.cpp:3506-3592`). That is `BUILDING_DIMENSION_GATE_GENERATOR` in `CITYMAP_ALIEN`, gated
behind `RESEARCH_ALIEN_BUILDING_9`.

`RaidAliens` requires `building->owner == ORG_ALIEN`, which only alien-city buildings are.
Infiltrated human-city buildings route to `AlienExtermination` instead and never check `victory`.

The chain, strictly in order:

1. `RESEARCH_ADVANCED_WORKSHOP` → unlocks `FACILITYTYPE_ADVANCED_WORKSHOP` (a Large workshop).
2. Build it. `MANUFACTURE_DIMENSION_SHIFTER` requires `required_lab_size Large`; the starting
   base has only a small workshop.
3. Manufacture `VEQUIPMENTTYPE_DIMENSION_SHIFTER` — 50,000 man-hours, $14,000 per unit, charged
   per unit. It has **no research prerequisite**: the patch data deletes its only dependency.
4. Fit it to a craft. `Vehicle::hasDimensionShifter()` is the sole gate on crossing.
5. Order that craft into a Dimension Gate → `VehicleMission::gotoPortal`. Without a shifter the
   craft is stranded or crashes on arrival.
6. For `i` in 0..9: research `RESEARCH_ALIEN_BUILDING_i`, then raid alien building `i`. Winning
   force-completes the unlock for `i+1`. Building 9 ends the game.

`RESEARCH_UNLOCK_DIMENSION_GATES` and `RESEARCH_UNLOCK_ALIEN_DIMENSION` are auto-completed dummy
topics that gate nothing mechanical — do not wait on them.

`gs alien_buildings` reports all ten with their access topic, whether it is researched (`open=`),
and which one wins. Re-query after every raid rather than assuming an order.

## Research — the thing that silently does nothing

Assigning a project does **not** start research. `Lab::update` returns immediately when
`getTotalSkill()` is zero (`research.cpp:445-449`), and skill comes from agents in
`lab->assigned_agents`. An unstaffed lab shows a current project and advances zero man-hours for
ever. This invalidated every research claim made before it was found: eight game-days in,
`RESEARCH_DIMENSION_GATES` was still at 0/5000.

Staffing: on `ResearchScreen`, selecting a row in `LIST_UNASSIGNED` assigns that scientist to the
lab being viewed. The list re-populates after each assignment, so index 0 always names the next.

```
CONTROL LIST_SMALL_LABS set <i>      # choose the lab (both lab lists are HORIZONTAL)
CONTROL LIST_UNASSIGNED set 0        # repeat until it refuses (lab full) or nobody is left
CONTROL BUTTON_RESEARCH_NEWPROJECT
CONTROL LIST set <row>               # row from gs research_options, not a guess
CONTROL BUTTON_OK
```

Rate: man-hours accrue at `skill` per game-hour. Five scientists give roughly 400, so a
9,000-hour topic takes about a game-day and a 38,000-hour one about four.

Pick topics by name. `gs research_options` mirrors `ResearchSelect`'s own filtering and ordering,
so the index it reports is the index to select. Note it also lists `required_lab_size Large`
topics that a small lab will refuse with a message box — the UI does not filter those out.

`labs=5` is a trap: two of those `Lab` objects have no built facility behind them and a third is
an engineering Workshop, which takes manufacturing rather than research. Use `assignable` and
`assignable_busy`.

**Scientists leak.** The squad sent to an incident is drawn from whoever is standing in the
building, scientists included, and they die there. Observed: player agents 25 → 18, lab staffing
5/5/5 → 2/3/0 over a handful of missions. Re-hire and re-staff on a timer rather than trying to
identify roles in a runtime-generated dispatch list.

Finishing a topic opens `UfopaediaCategoryView`, which is in `WORKING_STAGES` — so the generic
modal dismisser deliberately ignores it and the campaign clock stops. Back out explicitly.

## The named-action layer

`CONTROL <id>` invokes a widget by id; `CONTROL <id> set <v>` drives a CheckBox / ScrollBar /
TextEdit / ListBox index; `CONTROLS` lists named widgets. Two extensions matter because the
widgets that do the real work have no ids at all:

- `CONTROLS <id>` — enumerate a control's children by position, with type and whether `set` can
  drive them.
- `CONTROL <id> item <N> [op] [value]` — operate on the Nth child. `set` falls through to the
  first settable descendant, which is how a purchase quantity is reached: the row is a bare
  Control wrapping an unnamed ScrollBar.

**Selection is MouseDown, clicking is MouseClick, and they are not the same.** A `ListBox`
changes selection only on MouseDown. `Control::click()` raises MouseClick, which it ignores. So
`CONTROL <list> item <N> click` selects nothing while reporting success — this is why
`ListBox::selectItemByIndex` exists and why `set` is the right verb for any list.

## City operations

**Crewing a craft.** `recoverVehicle` is refused unless the selected craft carries a Soldier.

- The assignment widget only exists on screens embedding `city/agentassignment.form`.
  *Right*-click your own base to get `BuildingScreen`; a left-click gives `BaseScreen`, which has
  no widget.
- The dragged list is built on the first MouseMove more than 5px from the press, not on the press
  — press-and-release transfers nobody. Issue a stepped path.
- `MultilistBox` toggles: re-clicking a selected row deselects it. Select once.

**Interception and recovery** both need the order armed first: a plain click only runs
`orderSelect`; `CitySelectionState::AttackVehicle` (via `BUTTON_VEHICLE_ATTACK`) is what routes
the next click into `orderAttack`, where the recovery branch lives. Centre on the target first —
`ufos_screen` reports craft anywhere in the city, including well outside the viewport, and a
click there hits nothing.

**The clock.** `canTurbo()` is false while *any* vehicle holds an Attack mission — including your
own. A wing left circling after its target went down pins the game at normal speed for ever,
which costs about ten real hours per game-day instead of seconds. Recall idle attackers with
`BUTTON_GOTO_BASE`.

## Battles

`BattleView` binds no number keys — `SDLK_1` is not handled, so squad hotkeys do nothing. Units
are selected by clicking them, and **Ctrl+click** adds to the selection (capped at six).

Winning is a movement problem: units fire at will on their own, so the job is putting people
where they can see hostiles. Aim a short way *off* the target's tile — a click on an occupied
tile selects rather than moves.

`enemies_screen` only reports hostiles already on screen. Survivors scattered across the map are
invisible; `centre_on_enemy` walks the camera to one that is not framed. Consult it when nothing
is visible *and* when progress dries up, or one unreachable alien will hold the whole mission.

Ask the engine who won: `Battle::playerWon`. A debriefing appears however the mission ends, so
treating its arrival as a win records a total squad wipe as a victory.

## Logistics

- **Hire** on `RecruitScreen` (`BUTTON_BASE_HIREFIRESTAFF`). A candidate moves to the payroll on
  a plain click on its row in `LIST2`; nothing is charged until `BUTTON_OK` raises Confirm Orders
  and `BUTTON_YES` is pressed. Role filters: `BUTTON_SOLDIERS`, `BUTTON_BIOSCIS`,
  `BUTTON_PHYSCIS`, `BUTTON_ENGINRS`.
- **Buy** on the buy/sell screen, where `BUTTON_AGENTS` means agent *equipment* — the role
  buttons are hidden there and belong to RecruitScreen. A row's scrollbar is a **balance across
  both sides of the trade, not an order size**, and *raising* it sells: writing `have + qty` took
  stores from 60 items to 30 and put money back in the bank while the log claimed a purchase.
  Read the current value with `CONTROL LIST item <N> get`, then move it *down* to buy. Commit with
  `BUTTON_OK` and confirm. Goods arrive as cargo a couple of game-days later.
- **Equip** via the engine's equipment templates (`--OpenApoc.NewFeature.EnableAgentTemplates=1`).
  Ctrl+`<n>` stores the shown agent's loadout, a bare `<n>` strips and re-equips every selected
  agent from base stores. Two traps: `AGENT_SELECT_BOX` is a ListBox, so use `set` not `click`;
  and applying an *empty* template disarms people — captured from the wrong row it took armed
  from 10 down to 4. Verify with `gs templates` before applying, and stop if `armed` falls.

## Base construction

Genuinely a drag, not a named action. `BaseScreen` keys off raw mouse events against whatever
control is under the cursor: hovering a `LISTBOX_FACILITIES` row chooses what to drag, and the
build commits on MouseUp inside the grid. `CONTROL click` and `CONTROL set` are both dead ends.

```
MOVE <row centre>            # hover selects the facility type
DOWN <row centre>
MOVE <tile centre>
UP   <tile centre>           # commits
```

Every row is an identically-named Graphic, so `gs facilities` reports the buildable types in the
same order `BaseScreen` builds its list — position in that list is position on screen. Use
`live_rects` (not `ui()`, which collapses same-named controls). The grid is 8×8 tiles of 32px
inside `GRAPHIC_BASE_VIEW`. Corridor layout and occupancy are not exposed, so try tiles and treat
a MessageBox as a rejection.

## Crashes

macOS crash reports from this game are worth reading, not ignoring. A run of 23 of them all had
the same signature -- SIGSEGV at address 0x8 in `GameState::initState()` on a thread-pool worker
under `BootUp::update()` -- and that is the save-loading path. `initState` dereferenced several
references a loaded save may not have resolved, so a slightly incomplete object graph killed the
process instead of failing visibly. That is fixed; if the signature reappears, look for a new
unguarded reference on the same path.

Note that macOS does not write a report for SIGKILL, so a report means a genuine fault. The
driver already sends `QUIT` and waits before killing anything, so a clean shutdown produces none.

## Running a campaign

```bash
python3 tools/oa_victory.py --port 17800 --hours 72
```

Checkpoints every few minutes and resumes from the checkpoint after a crash. Do not `pkill -f
MacOS/OpenApoc` while it runs — that kills its game and forces a restart; kill by port instead.

Useful queries: `gs research`, `gs alien_buildings`, `gs facilities`, `gs agents`, `gs stores`,
`gs templates`, `gs vehicles`, `gs selected`, `gs crashes`, `gs turbo`, `gs battle`.
