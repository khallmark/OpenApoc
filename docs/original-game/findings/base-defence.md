# What starts a base defence

**BOUND:** UFO2P.EXE ISO non-4. Static investigation only: headless Ghidra `-readOnly -noanalysis`
on the `OpenApocOG` project, plus a byte scan for callers of the tactical launcher. There was no
runtime session of the original. Offsets are physical file offsets unless marked VA; for this code
object, file = VA + `0x626A4`.

## The launcher and its kinds

`FUN_000ac08c(target, kind, attackerOrg, vehicle)` (file `0x10E730`) launches every tactical
mission. It writes `scenario/*.dat` and runs TACP. Its kinds are:

| Kind | Mission |
|---|---|
| 0 | investigate (from the UI) |
| 1 | UFO crash |
| 2 | incident response |
| 3 | raid |
| 4 | organisation incident |
| 5 | base defence |

A byte scan finds exactly seven callers of `FUN_000ac08c`. Only three pass kind 5.

## The three paths to a base defence

1. **Infiltration UFO** (role 3), `FUN_0003a910` at VA `0x3AE76`.
   - It fires when the UFO lands (wait `+0x70` = `0x24`) and its target building is an active
     X-COM base.
   - There is no roll and no "known to aliens" requirement.
   - The crew comes from the UFO type's drop table at VA `0xDD0EC` (species 0, 4 and `0xB` zeroed,
     capped at 36).
   - `FUN_00070140` picks the target: a random non-destroyed building within
     `2·max(dx,dy) + min(dx,dy) ≤ 60` of the UFO. No owner filter was seen.
2. **Subversion UFO** (role 2), at VA `0x3B079`.
   - If the base is already exposed (base record `+0x2BC`), the UFO starts a kind-5 mission. The
     global `DAT_000e0cc0` would also allow it, but nothing ever sets it.
   - Otherwise `FUN_0007062c` marks the base as known, with certainty.
   - `FUN_000702e4` targets one exposed base per incursion, and otherwise a random building.
3. **Hostile organisation attack.** `FUN_00092470` (file `0xF4B14`) raises incident types 2 and 3
   on an X-COM base building, through `FUN_00099e04`.
   - The generator `FUN_00092060` (file `0xE4704`) runs for organisations whose relation to X-COM
     is −50 or worse.
   - Chance is about `X / (counter + 1)`, where `X = max(1, longterm − current)` and the counter
     starts at `0x50 − 2·difficulty`. The countdown is `rand(240) + 10`.
   - Cadence is daily, inferred from its caller (the day-end step).
   - Types 2 and 3 are rare for ordinary organisations (table VA `0x1277C6`). They are common for
     Megapol, organisations 20–22 and organisations the aliens have taken over (table VA
     `0x1277A8`).

**Launch gate.** If no X-COM agent is at the base, the base is deleted with only a ticker
message. There is no fight.

**Outcome.** A win costs X-COM 50 × destroyed map parts in funds. Retreat or loss deallocates the
base (`FUN_000b3114`), which matches the TACP briefing text (file `0x2E0931`).

## What does not start one

- **Alien movement.** The hourly spread between buildings, `FUN_0006f7f8`, and the post-mission
  relocation, `FUN_0006f738` (file `0xD1DDC`), only set the base's known flag. Neither starts a
  mission or deletes an unmanned base.
- **Detection.** `FUN_0006f964` can raise an incident in a base building. It is an ordinary
  kind-2 incident, not kind 5.

## The known-to-aliens flag

What sets it:
- `FUN_0007062c` (the subversion UFO, above), always.
- Alien movement into the base's building: `rand(0..100) < 5 × moved count`. `FUN_0006f738` rolls
  this once per species.
- The 5% roll in `FUN_0005fddc` cannot be reached: its only caller is guarded by
  `FUN_000705f8 == −1`.

What clears it: only `FUN_0006779c` (file `0xC9E40`), when a new base is built in that slot.

## What the player can do

- Shoot infiltration and subversion UFOs down before they land.
- Keep agents in the base. An unmanned base is lost without a fight, but that only happens when
  one of the three paths fires.
- Keep hostile organisations above −50.
- Reduce the alien populations in nearby buildings, which cuts the exposure rolls.

The base owner's relation to the aliens does not matter: the alien-relation bonus in movement uses
the owner of the building the aliens leave.

Separately, a base building reduced below 10% of its value by projectile hits in the city is
deleted without a mission (`FUN_00054a28` → `FUN_000b3114`).

## OpenApoc

### Fixed

`Building::alienMovement` and `Building::detect` (`game/state/city/building.cpp`) used to start
`DefendTheBase` when aliens moved into, or were detected in, a base's building. They also deleted
the base outright if it was unmanned. Neither path exists in the original.

Over about 25 minutes of a 16-campaign learner grid, all 12 logged base defences came from alien
movement, and none from a UFO landing.

Now:
- movement only rolls the exposure;
- detection raises the ordinary `AlienSpotted` alert, as it does in any other building.

`tests/test_city_rules.cpp` (`test_aliens_reaching_a_base_do_not_start_a_defence`) checks this.
Aliens move into an unmanned base's building and are then detected there. The test asserts that the
base survives both steps, and that detection marks the building as detected, as it would any other
building.

### Still divergent

- `vehiclemission.cpp`: an infiltration UFO landing on a base rolls exposure and deposits its
  aliens in the building. The original goes straight to the drop-table attack.
- `Building::alienMovement`'s relation bonus:

  | | Owner it tests | Threshold | Moved count |
  |---|---|---|---|
  | OpenApoc | the destination's | "Friendly" | not required |
  | Original | the source's | taken over, or relation above 74, adds 20 percentage points | at least 3 |
- `organisation.cpp`'s attack incident types 2 and 3 match the original.

## Not determined

- The cadence of `FUN_00092060`; daily is inferred.
- Whether an alien incident in a base building can launch anything other than a kind-2
  investigation.
