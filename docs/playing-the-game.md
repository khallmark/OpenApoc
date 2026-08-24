# Playing OpenApoc through the harness

How to drive a full Novice campaign to victory with no human input and no cheating. Everything
here was learned by doing it and being wrong first; each entry says what actually happens, not
what the UI appears to offer.

**Status: victory not yet achieved.** The campaign has been losing for strategic reasons as much
as mechanical ones — see Strategy below. The chain below is mapped end to end and every step up to
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
   per unit, and it needs a **Large** workshop.

   **Do not trust the XML here.** `data/common_patch/gamestate/research.xml` carries an
   `op="delete"` on this project's research dependency, which reads as "no prerequisite" — and a
   strategy pass concluded exactly that. Asking the running engine says otherwise:
   `gs topic MANUFACTURE_DIMENSION_SHIFTER` reports `deps_satisfied=0` on a fresh game. The
   common_patch files are a *patch over data extracted from the player's own copy of the original
   game*, so the merged truth is not readable from this repo at all. Use `gs topic <ID>`.

   Its dependency, `RESEARCH_DIMENSION_SHIFTER`, is `hidden=1` and 190,000 man-hours. Hidden
   topics are permanently excluded from the manual research list (researchselect.cpp:234), so it
   can never be researched by hand — it has to be force-completed by an event.

   **That event is recovering a UFO.** `CityView` force-completes every topic in the recovered
   craft type's `researchUnlock` list (cityview.cpp:4376-4379). `gs ufo_types` shows all ten
   alien craft types carry the same set: `RESEARCH_UNLOCK_ALIEN_CRAFT_CONTROL_SYSTEMS`,
   `..._ENERGY_SOURCE`, `..._PROPULSION`, and `RESEARCH_UNLOCK_UFO_TYPE_<n>`. Those open the
   alien-craft research chain, which is what eventually reaches the shifter — matching the player
   guides' insistence that you must shoot down *and recover* UFOs, not merely destroy them.

   So the recovery chain is not a side quest, it is the critical path: crew a craft, intercept,
   shoot down, recover, research what the recovery unlocked, repeat.

   **Two UFO types can be recovered without a battle at all.** `UfoRecoveryBegin` only starts a
   tactical mission when the craft type has a `battle_map`; otherwise it fires
   `UfoRecoveryUnmanned` and force-completes the same research for free
   (cityview.cpp:4363-4380). `gs ufo_types` reports the flag per type, and on this data
   `VEHICLETYPE_ALIEN_PROBE` and `VEHICLETYPE_ALIEN_SCOUT` are battle-free while the other eight
   are not. Since every type unlocks the same three alien-craft topics (plus its own
   `RESEARCH_UNLOCK_UFO_TYPE_<n>`), a driver that is losing soldiers in crash-site fights can
   advance the tech chain by prioritising Probes and Scouts and declining the rest.
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

  **Templates are not a general arming mechanism.** `processTemplate` strips the agent and
  re-equips the template's *exact* item types from stores. If the armoury holds different weapons
  than the captured loadout names, every application takes a weapon away and gives nothing back:
  measured, `applied to 12 agents; armed 4->4` with 25 weapons in stores. Abort after a few
  applications that do not raise `armed`. Arming recruits with arbitrary stock still needs the
  Shift+mousedown quick-equip, whose ground-inventory rects are computed at runtime and are not
  in the .form file.

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

## Strategy (from player guides, not from the source)

The driver kept losing for reasons that are not harness defects at all. Distilled from UFOpaedia
(NKF's Starter's Guide, AllOutWar's Guide, the Base Defense / Agents / Relations pages), the
Wikibooks and StrategyWiki guides, the Roger Wong and Loodwig GameFAQs FAQs, and Lilura1's
playthrough notes.

**Technical personnel cannot fight.** Scientists, biochemists, engineers and quantum physicists
have no inventory, cannot use weapons, cannot kneel, and simply die or get captured. They are not
defensive strength. Sending "whoever is standing in the building" to an incident kills them and
guts research at the same time — exactly what happened here: player agents 25 → 18, lab staffing
5/5/5 → 2/3/0.

**A base with no armed defender is destroyed automatically the moment it is attacked.** Never let
any base sit undefended, and never run on a single base — buy a second early purely as a
failure-recovery hedge. This is precisely how the first full run ended: a base defence fought by
21 unarmed people.

**Armour is not optional.** Buy a full Megapol suit — helmet, chest, both arms, legs — for every
agent from day one; it is cheap and immediately available. "Under no circumstances should an agent
be sent into combat without armour." Marsec body armour (grants flight) is worth switching to
later.

**Security Stations do not work in OpenApoc — ignore that advice.** The guides make much of them,
but checking this codebase: `FacilityType` (game/state/rules/city/facilitytype.h) has no weapon,
turret or defence field at all, and grepping "security" across base.cpp / facility.cpp finds
nothing. `FACILITYTYPE_SECURITY_STATION` exists as a buildable tile and is otherwise inert here.

**You also cannot position your defenders.** `Battle::initialUnitSpawn()` picks each defending
unit's starting tile with `pickRandom` over the map's spawn blocks, and `BaseDefenseScreen` just
calls `beginBattle()` with the base roster — there is no pre-placement screen, so no chokepoint
plan is drivable. `fillMap()` also notes base defences "never prevent entrance or exit from any
vector", so the single-breach-corridor idea does not apply.

Which leaves *arming people* as the one base-defence lever that actually exists here.

**Squad**: cap around six combat agents. Standard early loadout is a weapon plus two spare clips,
two AP grenades, two stun grenades, and a medi-kit. Early weapons: Megapol Lawpistol or Marsec
M4000 Machine Gun.

**Turn-based is safer for a scripted player** on ordinary missions; real-time is preferable for
base defence, where Security Stations fire on their own regardless.

**Retreat is a legitimate move.** Every battlescape has blue exit tiles at its edges; standing an
agent on one extracts them safely. "No mission is important enough to lose good men on." Fighting
every mission to the death is what produces a losing spiral.

**Money** comes from raiding as much as from UFOs. The Cult of Sirius is permanently hostile by
design, so raiding them costs no diplomacy. Sell researched alien tech.

**Watch for a research naming trap.** `RESEARCH_BIO-TRANSPORT_MODULE` (BioChem) is the craft
module for carrying live specimens. `RESEARCH_BIO-TRANSPORT` (Quantum Physics, order 20110) is a
*different* topic that unlocks the Biotrans inter-dimensional craft. A driver matching on the
substring "BIO-TRANSPORT" will target the wrong one.

`FACILITYTYPE_ALIEN_CONTAINMENT` has no dependency in this data — it is buildable from turn one,
with no research chain to start first.

**Reaching the alien dimension** is a chain, not one unlock: research Dimension Gates, then shoot
down *and recover* a UFO Type 3 Transporter, which is what opens the alien-craft research leading
to dimension travel. Treat capturing a Transporter as a hard objective once Type 3s appear.

Sources: [UFOpaedia Apocalypse](https://www.ufopaedia.org/index.php/X-COM:_Apocalypse),
[Wikibooks strategy guide](https://en.wikibooks.org/wiki/X-COM:_Apocalypse),
[StrategyWiki](https://strategywiki.org/wiki/X-COM:_Apocalypse).

## Skirmish mode (for isolated battle testing)

`tools/oa_skirmish.py` drives Skirmish for fast, isolated combat testing instead of running a
full campaign just to reach one battle. Reaching Skirmish needs a game already running --
InGameOptions -> BUTTON_SKIRMISH -- there is no cold-boot entry from MainMenu.

**A map row is not chosen by ListBox selection.** MapSelector never listens for
ListBoxChangeSelected at all; each row is an inert Control holding a Label plus a small nested
GraphicButton, and only clicking that button calls `Skirmish::setLocation`. This needed the
named-action layer to address a grandchild, not just a child --
`CONTROL LISTBOX_MAPS item <row> item 1 click` -- so `CONTROL <id> item <N>` now supports any
number of chained hops, not just one.

**Whether SelectForces even appears depends on location type.** A Base row always customizes. A
UFO row skips SelectForces entirely and goes straight to AEquipScreen for boarding loadout. An
alien Building row uses its own preset crew and skips SelectForces too, *unless*
`CUSTOMISE_FORCES` is checked before clicking Skirmish's own `BUTTON_OK` -- check it regardless
of which location type is picked.

**Skirmish battles do not currently start.** Setup is fully reliable up through AEquipScreen, but
the actual transition into BattleView does not happen -- see the status note at the top of
`tools/oa_skirmish.py` for what was actually verified. This looks like a genuine pre-existing bug
in Skirmish's own battlemap generation (a threadpool exception was directly observed on one map),
separate from the main campaign's battle path, which is unaffected and has been fighting real
missions all session.

## Ask the engine, not the XML

`gs topic <ID>` reports what the running game actually believes about a research or manufacture
topic: type, complete, hidden, started, whether it needs a Large lab, whether its dependencies are
satisfied *right now*, man-hours progress and cost. Verified readings on a fresh Novice start:

| topic | reachable now? |
| --- | --- |
| `RESEARCH_ALIEN_BUILDING_0` | yes — `deps_satisfied=1`, 38,000 hours |
| `RESEARCH_DIMENSION_GATES` | yes — `deps_satisfied=1`, 5,000 hours |
| `RESEARCH_ADVANCED_WORKSHOP` | **no** — `deps_satisfied=0`, despite being a plain Physics topic |
| `MANUFACTURE_DIMENSION_SHIFTER` | **no** — `deps_satisfied=0`, needs a Large lab |
| `RESEARCH_DIMENSION_SHIFTER` | **never manually** — `hidden=1`, 190,000 hours; unlocked only by recovering UFOs |

`gs ufo_types` answers "which craft unlocks what when recovered" — also unreadable from the repo,
since vehicle types come from the extractor rather than `data/`.

This is the tool to reach for before planning any research route. Reading the repo's XML produced
a confidently wrong plan; one query would have caught it.

## What actually ends campaigns: funding

Not score directly, and not battles. `GameState::weeklyPlayerUpdate` latches `fundingTerminated`
the first week that **either** lifetime `totalScore` drops below **-2400** **or** the government's
relation to the player turns Hostile (below -50). It zeroes income and **nothing in the codebase
ever resets it** (gamestate.cpp:1668-1680). Income 0 is not a slump to trade out of; the campaign
is over from that moment.

Most runs here died by the *relation* route, not the score one — funding terminated at -1312 and
again at -1783, both well short of -2400. Two causes, both self-inflicted:

- A generic screen responder that mapped `BuildingScreen` to `BUTTON_RAID`, investigating any
  building it happened to land on.
- Answering every alien alert. **Investigating a building and finding no aliens costs its owner
  `-5 - difficulty` relation** (buildingscreen.cpp:154-166), and alien crews *relocate between
  buildings on a timer*, so a good share of alerts are stale by the time a squad arrives. Thirty-
  nine answered alerts took the government from +85 to -100.

`AlertScreen` exposes its building and that building's current crew via `Stage::harnessDetail()`,
so an alert whose building is already empty can be declined. `gs infiltrated` lists buildings that
genuinely hold crew, plus the live government relation. Watch `gs funds` for
`funding_terminated`, `margin_to_cutoff`, and the seven score buckets separately — the bleeding
category is otherwise pure guesswork.

Measured contrast on two runs: answering everything gave `gov_relation -100`, score -1783,
funding dead. Declining stale alerts gave `gov_relation +98`, score **+296**, funding intact.

## Use the right craft for the job

Three separate failures, one root cause — not checking a craft can do what is being asked:

- Interception selected whatever was in the vehicle list, including `Stormdog` and
  `Wolfhound APC`, which are **road vehicles** and cannot engage an airborne UFO.
- Crewing loaded the squad onto a road vehicle, so every recovery was refused with
  `mission: none` and the research chain stalled completely.
- Interception then scrambled the *crewed transport* into dogfights, risking the squad and the
  only airframe that can collect a wreck.

`gs interceptors` reports `flying`, `armed`, `crew` and `shifter` per craft in vehicle-list order.
Use it before ordering anything. Note also that each plain click on the vehicle list *replaces*
the selection — Ctrl+click is what adds (cityview.cpp:340).

## Running a campaign

```bash
python3 tools/oa_victory.py --port 17800 --hours 72
```

Checkpoints every few minutes and resumes from the checkpoint after a crash. Do not `pkill -f
MacOS/OpenApoc` while it runs — that kills its game and forces a restart; kill by port instead.

Useful queries: `gs research`, `gs alien_buildings`, `gs facilities`, `gs agents`, `gs stores`,
`gs templates`, `gs vehicles`, `gs selected`, `gs crashes`, `gs turbo`, `gs battle`.
