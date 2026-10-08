# Civilian traffic replenishment from observed city runs

Tracking: [OPE-25](https://linear.app/littleblackhat/issue/OPE-25/keep-civilian-traffic-replenishing-without-thinning-the-city).

The dispatcher borrowed an idle vehicle of the selected type from an organisation's park.
It therefore could not send a trip when that inventory was missing or busy. The original
dispatcher allocates a new slot. OpenApoc now creates a temporary vehicle for the trip,
preserves its outward and return journeys, and retires it after the mission queue finishes.
This supplies traffic independently of purchased fleets without accumulating parked vehicles.

## What was reproduced

The preserved dense saves did not show permanent gridlock in the source baseline. Every
initial road vehicle moved during six-hour runs. A normal new game, entered through the
actual MainMenu and DifficultyMenu, immediately stocked 330 parked vehicles, including five
player vehicles. It sustained traffic in the source baseline too. Complete traffic disappearance
was **not** reproduced in that fresh UI game.

The unstocked regression fixture, which deliberately stops before `fillOrgStartingProperty`,
did reproduce the dispatch dependency: the baseline sent zero trips without purchased NPC
vehicles. The corrected dispatcher sends trips from that same fixture. This proves inventory
independence; it does not prove that a normal UI new game has empty organisation parks.

The Desktop checkout's existing application was an August build with embedded revision
`771ba686`. Symbol and string checks found neither `City::dispatchAmbientTraffic` nor the
recent traffic batch logging. The newer reboot build did contain those symbols. No observed
process established which executable the user had launched, so the old application is a
possible launch mismatch, not a confirmed cause of the reported failure.

## Original-game evidence

The existing Ghidra receipts in `OpenApoc-og-research/export/traffic_04..08` show
the fresh allocation in `FUN_00034860`. In canonical ISO non-4 `UFO2P.EXE`:

| Operation | Virtual address | File offset |
| --- | --- | --- |
| Dispatcher entry | `0x34860` | `0x96f04` |
| Fresh slot allocation call | `0x34a07` | `0x970ab` |
| Allocator target `FUN_0005d68c` | `0x5d68c` | `0xbfd30` |
| Road route request | `0x34a4b` | `0x970ef` |
| Failed-route cleanup | `0x34af2` | `0x97196` |

Allocation call bytes: `e8 80 8c 02 00`. Binary SHA-256:
`99f8787d5f1cb532e2620af2130bffb2558a869f9d4540d7fcf1b7487833d00f`.

The exports establish fresh allocation and building entry. They do not establish successful
arrival retirement in the original game. OpenApoc's bounded temporary-trip lifecycle is an
implementation choice. The original type mix, 4320-tick cadence, hourly batch sizes, vehicle
cap and alien relationship gate remain. Road lanes, junction priority and U-turn handling
come from the prerequisite traffic change; see
[ground-vehicle-occupancy.md](../original-game/findings/ground-vehicle-occupancy.md).

## Observation method

The fresh-game snapshot came from the rendered UI at difficulty 1, seed 1, paused at noon
before starting the clock. Saving did not advance time. Its SHA-256 is
`f00714bbc9e304458c61dbdc78e04b20fc504fbf540f3e9d545cd0d3fb7d20f1`;
saved RNG words are `7251685877758789622` and `4494004282789118709`.

`tests/traffic_census.cpp` loads that save, resumes its saved RNG and existing missions, and
runs full `GameState::update` in six-tick steps. It does not reseed, call `startGame`, change
missions or delete vehicles. The baseline executable was built from
`bce89743db239be164e07d86fb071320baed7d97`, the parked-fleet dispatcher with the latest road
rules. Candidate runs use this change. Each run verifies its observed clock delta, rather than
treating requested simulation duration as proof that the clock advanced.

One game hour is 518,400 ticks. Six hours is **3,110,400 ticks**, not 864,000 ticks; the latter
is only 100 game minutes. Checkpoints are 12,000 ticks apart (83.33 game seconds), plus the
last requested tick. A long stationary car has driving intent but has not moved for that
interval. Means below use every checkpoint after elapsed 60,000 ticks, excluding the first
6.94 game minutes of startup; they are sampled arithmetic means, not continuous-time averages.

The census's `civilian` bucket excludes player, alien and Megapol ownership. Generated traffic
owned by Megapol goes into `police`, including ordinary civilian vehicle types. All-map counts
include road, ATV and flying vehicles; they are the relevant visibility total. Ownership bucket
changes must not be interpreted as equivalent changes in vehicle types.

The controls are the same saved input, RNG and update step, not a promise of a bitwise replay.
Separate candidate six-hour and full-day processes already diverged in density at elapsed
84,000 ticks. Existing organisation mission selection iterates a pointer-keyed building map;
process allocation can change ordering. Its contribution to these differences was not isolated.
The measurements are retained as individual observed runs and are not averaged into a claim
of statistical certainty.

An arrival is counted only when the census observes a mapped vehicle enter its recorded
target building. The return leg can launch in the same update as landing, so some intermediate
arrivals are not observed. Dead exits are separate from confirmed arrival retirements. Retained
vehicle handles prevent ordinary removal from being misreported as a successful arrival.

## Six-hour fresh-game comparison

Both runs advanced from clock 6,220,800 to 9,331,200, exactly six hours, with the same input,
RNG and update step. There are 255 post-warmup checkpoints.

| Sampled measure | Parked-fleet baseline | Corrected continuous return |
| --- | ---: | ---: |
| Mean road population | 29.106 | 28.820 |
| Mean all-map population | 31.447 | 31.271 |
| Mean civilian ownership bucket | 28.118 | 26.420 |
| Mean Megapol ownership bucket | 3.329 | 4.851 |
| Checkpoints with a long stationary road vehicle | 0 | 0 |
| Observed target arrivals | 5,875 | 3,351 |
| Successful temporary-trip retirements | 0 | 2,820 |
| Parked vehicles at end | 297 | 331 |

Road density differs by -0.98% and all-map density by -0.56%. The civilian ownership bucket
is 6.04% lower and the Megapol bucket higher. These are descriptive results from one saved
input, not a statistical significance result. The accepted variant sustains approximately the
same visible population while removing its dependence on the parked fleet.

The candidate saw 2,968 new lifetime vehicle records, but its live city population changed
from 330 to 359 and peaked at 366. Parked population changed from 330 to 331. Lifetime records
are not live population growth: completed generated trips are cleaned up.

Two rejected drafts demonstrate why trip-start counts alone are insufficient. One-way
retirement lowered mean civilian population to 18.83, about a third below the baseline,
despite more dispatch starts. Restoring the return journey with the inherited ten-second
stop raised it to 25.54 but left cars hidden while consuming reserved slots. Continuing the
return as soon as the exit is free produced the accepted result above.

## Full-day observation

The paired full-day run advanced from clock 6,220,800 to 18,662,400, a verified duration of
12,441,600 ticks. It includes 1,032 post-warmup checkpoints, including the final partial interval.

| Sampled measure | Parked-fleet baseline | Corrected continuous return |
| --- | ---: | ---: |
| Mean road population | 25.614 | 26.867 |
| Mean all-map population | 27.842 | 28.985 |
| Road population range | 7–36 | 11–35 |
| Mean civilian ownership bucket | 25.862 | 23.467 |
| Checkpoints with a long stationary road vehicle | 28 | 0 |
| Road/flying vehicles at end | 33/0 | 34/1 |
| Observed target arrivals | 21,103 | 12,672 |
| Successful temporary-trip retirements | 0 | 9,977 |

The corrected city continues supplying traffic through the night and into the following noon.
Its hourly batch sizes still produce quieter periods. The candidate live city population ends
at 392 and peaks at 395, with 357 parked vehicles; permanent fleet purchases and other city AI
can add vehicles independently of the bounded ambient dispatcher. The observation does not
establish persistence beyond this day.

## Dense saves and regression checks

The two preserved dense saves each ran another 3,110,400 ticks in the corrected build.
All 147 and 157 initial road vehicles, respectively, changed position; the end checkpoints
had no long stationary cars. The runs recorded 2,977 and 3,218 target arrivals, including
1,974 and 2,113 completed temporary-trip retirements. Their final road/flyer populations were
32/3 and 28/6. Those are continuation tests of congested inputs, not fresh-game A/B pairs.
The first dense save had temporary long waits at 42 of 255 post-warmup checkpoints, with up
to eight stationary cars, versus none in its baseline. Those waits recovered before the end.
The second dense save had no such checkpoints. Fresh-game zero-stall results must not be
generalized to every congested save.

An additional six-hour run with a checkpoint blocker observer saw no long stationary cars
at any of its 255 post-warmup checkpoints. Endpoint replays did not recover the eight blocked
vehicles from the earlier run. Their traces diverged despite using the same frozen executable,
input and RNG; a specific cause for that earlier recovered wait was not established. The
earlier result is retained, rather than replacing it with the cleaner repetition.

The final full CTest run passed **65/65**. The expanded traffic test checks head-on passing,
the right-hand lane, queuing and U-turns, six hours of unstocked replenishment, completed-trip
retirement, bounded pending traffic, clean building references, independent permanent fleet
purchases, destroyed-pad rejection and cleanup, and a packed marker save roundtrip with the
false default for older records. Player ownership, cargo and passengers have retirement
guards in production code; focused runtime assertions for those three guards were not added.

A separate rendered new game ran for 120 wall seconds at the ordinary simulation multiplier.
Its clock advanced 52,488 ticks (6.075 game minutes), its final census contained 30 vehicles
on the map, and it remained in CityView. This exercises the actual menu and renderer path;
the six-hour and full-day measurements above exercise the full state simulation without
rendering each frame.

## Reproducing and reviewing

Own aggregate observations are committed in [civilian-traffic-observations.csv](civilian-traffic-observations.csv)
and [civilian-traffic-summary.json](civilian-traffic-summary.json). Original EXEs, ISO images,
Ghidra databases and saved game assets remain excluded from Git. Full local receipts are
under `/tmp/openapoc-traffic-observed/`.

Build with `ENABLE_TESTS=ON`, then run the census with an existing compatible save:

```sh
build/bin/traffic_census SAVE 3110400 6 --Framework.Data=DATA --Framework.CD=CD_ISO --Config.Read=0
build/bin/traffic_census SAVE 12441600 6 --Framework.Data=DATA --Framework.CD=CD_ISO --Config.Read=0
ctest --test-dir build --output-on-failure
```

This PR includes the unpublished road-lane/civilian-mix prerequisite `bce89743` and the
replenishment fix, based on `khallmark/develop-reboot`. It does not claim parity for on-demand
taxis, the original cross-city 80-slot pool, or successful-arrival retirement. Intentional
traffic suppression during an alien incursion remains governed by the original relationship
gate. Longer campaigns, different difficulties and the user's exact launch path remain outside
the observed evidence above.
