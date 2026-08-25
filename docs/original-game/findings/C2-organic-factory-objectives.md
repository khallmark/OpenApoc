# C2 follow-up · Alien-building destroy objectives

**Verdict: the mechanic is ALREADY IMPLEMENTED and correctly data-driven. One genuine residual gap
remains, and it is cosmetic.**

This file records a hypothesis that was **wrong**, because the negative result is worth keeping:
the next person to read the TACP briefing strings will form the same suspicion.

## The wrong hypothesis

TACP holds ten alien-building mission briefings at file offsets `0x2E0C09`–`0x2E1A76`, each naming
an explicit destroy objective — "all Megapods must be destroyed", "destroy all sleeping units",
"All embryonic UFOs must be destroyed", "Destroy all of the generators to disable the building".

OpenApoc's `playerWon` is set purely by "no hostile organisation has a living normal unit"
([battle.cpp:2167](../../game/state/battle/battle.cpp#L2167)). It looked like the endgame had been
reduced from *destroy the objectives* to *kill everything*.

## Why it is wrong

The objective system exists and is separate from `playerWon`:

- `BattleMapPartType::missionObjective`
  ([battlemapparttype.h:75](../../game/state/rules/battle/battlemapparttype.h#L75)) and
  `AgentType::missionObjective`.
- `Battle::tryDisableBuilding` ([battle.cpp:2199](../../game/state/battle/battle.cpp#L2199)) returns
  false while any objective **unit** or undestroyed objective **map part** survives.
- `Battle::checkIfBuildingDisabled` ([battle.cpp:2189](../../game/state/battle/battle.cpp#L2189))
  raises `GameEventType::BuildingDisabled`.
- `buildingCanBeDisabled` is latched at battle start
  ([battle.cpp:2605](../../game/state/battle/battle.cpp#L2605)), so only maps that ship objectives
  participate.

The objective map-part indices for **all ten** alien buildings are enumerated in
[extractors.cpp:121](../../tools/extractors/extractors.cpp#L121) — `39incub`, `40spawn`, `41food`,
`42megapd`, `43sleep`, `44organ`, `45farm`, `46contrl`, `47maint`, `48gate`.

The one entry that looks like a bug is not one: **`40spawn` has an empty map-part set** because the
Spawning Chamber's objective is a *unit*, not scenery. TACP's briefing says "the primary objective
is the destruction of the Queen and all Alien Eggs", and `UNIT_TYPE_QUEENSPAWN` is flagged
`missionObjective = true` at
[extract_agent_types.cpp:437](../../tools/extractors/extract_agent_types.cpp#L437). The data is
right.

`44organ` (Organic Factory) carries 22 objective parts, indices 137–158 — the embryonic UFOs. The
C2 concern about `craftFactoryIntact` keying on the building rather than the embryos is therefore
also unfounded: destroying the embryos is what disables the building.

## The residual gap — briefing text is not extracted

**NOT BOUND → NOT EXTRACTED.** The ten TACP briefings are not pulled into the game state. Searching
`data/` for "must be destroyed" returns nothing, and
[version01readme.txt:96](../../tools/extractors/docs/version01readme.txt) records "briefing text
does not work yet".

Consequence: the player is never told *what to destroy*. The objectives work; the instructions are
missing. In a ten-mission endgame where each map has a different objective type — sleeping units,
heat/light sources, white blocks, heart units, Megapods, embryos, generators — that is a real
usability loss, and it is the reason players resort to guides for the alien dimension.

**Scope: cosmetic, not mechanical.** No behaviour changes; nothing is mis-simulated.

### Implementation

1. The ten briefings are a contiguous block at TACP non-4 `0x2E0C09`–`0x2E1A76`. Confirm the exact
   record stride and count, then relocate on the 4 build (**do not assume the −0x2200 slide**).
2. Add an extractor writing them to the battle-map or building records, keyed by map name
   (`39incub` … `48gate`) to match the existing `missionObjectives` table.
3. Surface on the pre-battle briefing screen.
4. Also capture the two secondary objectives the briefings name, which OpenApoc does not model:
   **live capture of the Alien Queen** (Spawning Chamber) and **rescuing captive Sectoids**
   (Food Chamber, "will guarantee an Alliance with the Mutants"). Both need their own binding before
   any reward is implemented — **do not invent an alliance effect.**
5. The Sleeping Chamber and Dimension Gate Generator briefings both instruct agents to **evacuate
   after disabling**. Check whether an evacuation phase is modelled or whether `BuildingDisabled`
   simply ends the mission.

Lock test: `test_city_rules` case `alien_building_objectives_present` — assert each of the ten maps
has a non-empty objective set (unit or map-part), so a future extractor change cannot silently drop
one.
