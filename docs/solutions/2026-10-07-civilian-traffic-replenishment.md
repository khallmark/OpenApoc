# Civilian traffic replenishment from observed city runs

Tracking: [OPE-25](https://linear.app/littleblackhat/issue/OPE-25/keep-civilian-traffic-replenishing-without-thinning-the-city).

Speed 5 advanced five minutes at once but dispatched only one of the ten crossed traffic
intervals. A new game reproduced sparse early traffic through that actual UI path. Human-city
coarse updates now advance between every 30-second boundary, allowing trips to move and finish
before the next admission. The first game hour in the preserved-startup turbo observations averaged
4.833 road vehicles in the earlier control and 30.333 in the `8837a45` strictly eligible hour.
That improvement establishes early population, not healthy movement throughout a day. The later
reviewed head `685032b9` reproduces another sustained queue, recorded below.

The earlier dispatcher borrowed an idle vehicle of the selected type from an organisation's park.
It therefore could not send a trip when that inventory was missing or busy. The original
dispatcher allocates a new slot. OpenApoc now creates a temporary vehicle for the trip,
preserves its outward and return journeys, and retires it after the mission queue finishes.
This supplies traffic independently of purchased fleets without accumulating parked vehicles.

## What was reproduced

The preserved dense saves did not show permanent gridlock in the source baseline. Every
initial road vehicle moved during six-hour runs. A normal new game, entered through the
actual MainMenu and DifficultyMenu, immediately stocked 330 parked vehicles, including five
player vehicles. It sustained traffic in the source baseline at Speed 4 and six-tick updates.
Those observations did **not** exercise Speed 5; they initially missed the coarse-update defect.
Complete disappearance was not reproduced, but sparse new-game traffic was reproduced at Speed 5.

The unstocked regression fixture, which deliberately stops before `fillOrgStartingProperty`,
did reproduce the dispatch dependency: the baseline sent zero trips without purchased NPC
vehicles. The corrected dispatcher sends trips from that same fixture. This proves inventory
independence; it does not prove that a normal UI new game has empty organisation parks.

The Desktop checkout's existing application was an August build with embedded revision
`771ba686`. Symbol and string checks found neither `City::dispatchAmbientTraffic` nor the
recent traffic batch logging. The newer reboot build did contain those symbols. No observed
process established which executable the user had launched, so the old application is a
possible launch mismatch, not a confirmed cause of the reported failure.

## Speed 5 reproduction and correction

`updateTurbo()` calls `update(43200)`, crossing ten 4320-tick traffic boundaries. The dispatcher
call tested the interval count as a boolean. Even the fresh-allocation draft still sent just
one batch per five-minute frame. The correction subdivides human-city coarse updates at the
traffic boundaries and runs the existing state update for each slice. It preserves vehicle
progress, cleanup, the admission cap and each draw's clock, rather than submitting ten batches
at the frame endpoint. Ordinary small updates, alien-city updates and battle updates keep
their existing paths. `updateAfterTurbo()` still runs once per outer turbo frame.

The same saved UI startup and RNG were resumed with actual `updateTurbo()` calls, including
their normal post-turbo movement. The before executable uses the pre-review traffic code at
`0ea2a77b`; the after executable includes the cadence, destination-loss and route-cache fixes.
Every frame advanced exactly 43200 ticks, and both six-hour runs advanced exactly 3110400 ticks.

| Measure | Before cadence correction | After cadence correction |
| --- | ---: | ---: |
| Dispatch opportunities over six hours | 72 | 720 |
| Mean road population in first hour, 12 frame checkpoints | 4.833 | 30.083 |
| Mean all-map population in first hour | 6.833 | 32.333 |
| Road vehicles after five game minutes | 6 | 23 |
| Road vehicles after twenty game minutes | 4 | 30 |
| Mean road population over six hours after startup exclusion | 25.239 | 33.028 |

Separate full-day turbo runs advanced exactly 12441600 ticks and logged 288 versus 2880
dispatch opportunities. Their post-startup road means were 26.077 and 35.111, respectively.
Traffic from organisation missions is separate from ambient admission, so total mapped
vehicles can exceed the ambient limit. These are individual process observations, subject to
the pointer-ordering uncertainty described below.

These runs are an **intermediate candidate**, not proof of healthy flow. The day diagnostic
exposed Autotaxi `VEHICLE_1047` and Police Car `VEHICLE_136` blocking each other between adjacent
T junctions at `{87,39,2}` and `{87,38,2}`. Their mission stopped counters reached 11622356 and
11644024 ticks, corroborating waits longer than 22 game hours. The original blocker record is
retained in the aggregate JSON. The straight-only U-turn breaker cannot run on either tile.
The stopped-car branch used the old sprite approach facing rather than its known outgoing
route direction; beginning to wait therefore changed conflict classification and could prevent
both cars from ever starting again.

The rendered UI confirms the early effect: fresh difficulty-1, seed-1 games reached the first
alien alert at the same clock, 6393601 (12:20). The old app had **8** vehicles on the map; the
rebuilt app had **32**. Both started with 330 parked vehicles. The alert was acknowledged with
the real `BUTTON_QUIT`, then the clock was paused and each owned QA process exited. A prematurely
copied app was rejected because its hash matched the old app; it is excluded from these results.
The accepted after-app SHA-256 is
`1012e5c0cd41d2a3f97ef97449bf5702fa2d3b5dcde8a8a1757f427f6dd329e1`.

Turbo observations sample only at outer frame endpoints. Their arrival counters can miss trips
completed within a frame. A `stationary_12000` record in this mode means no net displacement
between five-minute snapshots; it cannot establish continuous immobility. The fine-step
stationary measurements below must not be generalized to turbo. Coarse sub-stepping also makes
timed invasions, organisation missions and periodic fuel hooks occur between slices, closer to
their ordinary-speed chronology, rather than deferring them all to the five-minute endpoint.

The intermediate observer directly called `updateTurbo()` even when `canTurbo()` was false;
the six-hour candidate logged one such error. Its long diagnostics are therefore not an
uninterrupted eligible UI run. The committed observer now checks eligibility in `turbo` mode,
stops with the exact clock when turbo becomes unavailable, and reserves the explicit
`turbo-unchecked` mode for such diagnostics. It reports unavailable-frame counts and the actual
mission stopped counters, so high occupancy cannot conceal a longstanding blocker.

Own turbo checkpoints and receipt hashes are in
[civilian-traffic-turbo-observations.csv](civilian-traffic-turbo-observations.csv) and
[civilian-traffic-turbo-summary.json](civilian-traffic-turbo-summary.json).

## Earlier junction correction (`8837a45`)

The junction departure correction keeps the known outgoing heading when a car waits, consistently
with the existing leaving-tile branch. Unknown routes retain the facing fallback, and entering
cars retain their full entry/exit conflict classification. The exact observed positions, facings
and reciprocal next tiles reproduce the stopped pair in a real-map test; its unrecorded onward
straight continuations are explicitly an inferred minimal fixture. The old code keeps both cars
stationary. The correction reaches both target tiles and completes both missions at six-tick and
4320-tick movement steps, while retaining same-exit queues and crossing-entrant blocking.

A separate final first-hour simulation checked `canTurbo()` before every frame. It advanced
exactly 518400 ticks, logged 120 dispatch batches and no unavailable frames, and averaged
**30.333 road vehicles / 33.167 all-map vehicles** across its twelve checkpoints. The earlier
pre-review first-hour control averaged 4.833 / 6.833. The final rendered UI at the first alert's
same 12:20 clock contained **34** mapped vehicles, compared with **8** before correction.
Its QA process was paused after acknowledgement, captured and closed.

Final long coarse diagnostics retained the same saved input and RNG. Their results are separate
from the intermediate candidate above:

| Measure | Final six-hour run | Final full-day run |
| --- | ---: | ---: |
| Exact clock advance | 3110400 | 12441600 |
| Logged dispatch opportunities | 720 | 2880 |
| Mean road population after startup exclusion | 32.761 | 30.080 |
| Mean all-map population after startup exclusion | 34.394 | 31.742 |
| Road/flyer vehicles at end | 32/0 | 32/2 |
| Frames with turbo unavailable, explicitly unchecked | 0 | 2 |
| Maximum sampled mission stopped counter | 31913 | 359706 |
| Checkpoints with a mission stopped counter >=12000 | 14 | 37 |
| Long mission waits at end | 0 | 0 |

The full day has no final blocker records. Queues still occur: its maximum sampled stopped
counter is 41.63 game minutes, and those waits recovered before the final checkpoint. The
six-hour endpoint has one position-stationary diagnostic but a maximum current-mission stopped
counter of only one tick. Different counters must not be collapsed into a claim that all waits
are absent. Both long runs use `turbo-unchecked`; the two unavailable frames in the day keep
that run distinct from an uninterrupted gameplay Speed 5 observation.

Both dense saves also advanced another exact 3110400 ticks using six-tick updates. All 147 and
157 initial road vehicles changed position. The final bench road mean was 30.400, with 3241
observed target arrivals, no long stationary checkpoints and no final blockers. Old2's road
mean was 30.906, with 3043 arrivals and no final blockers. Old2 still had recovered stationary
episodes at 23 of 255 post-startup checkpoints, up to three cars; its sampled mission stopped
counters stayed below 1369 ticks. Their cause is not established. Own receipt hashes and
aggregates are in [civilian-traffic-final-stress-summary.json](civilian-traffic-final-stress-summary.json).

That head's full CTest run passed **68/68**. The two new cache and junction tests have retained
red controls, and the final outgoing C++ scope passes clang-format 18. Secret scans and
ignored-binary hygiene cover the complete outgoing commit range and deliberately included files.

## Reviewed-head negative observation (`685032b9`)

The next head preserves authored recurring missions during old-save migration and uses the
following route node when shortcut or turbo movement classifies a junction exit. It passes
69/69 tests and automated PR review, but a new full-day observation still fails movement.
The day advances exactly 12441600 ticks and records zero unavailable turbo frames. It ends
with 36 road vehicles and two flyers, yet nine current missions have waited at least 12000 ticks;
the maximum is 5470956 ticks, **10.55 game hours**. Twenty-four road vehicles are endpoint-stationary.
Its road mean of 34.756 is therefore not evidence that the traffic is healthy.

The saved endpoint reproduces all 289 original census/event rows. Its SHA-256 is
`4046ee8b78ab32a9e51fff9149f497b86d4c026864b8d37d3ff8a1db44fccc88`.
Twenty cars occupy the two T junctions `{82,39,2}` and `{84,39,2}` and the intervening straight
tile `{83,39,2}`. A blocker cycle runs from vehicle 1693 to 1653 to 1709 to 136 and back to 1693.
Several other cars repeatedly replace their route and reset the mission stopped counter without
changing position. Resuming the captured queue for an hour with six-tick movement leaves
**0/20** cars two tiles from their starting positions and **0/20** completing their original
location targets. That physical-progress check catches failure concealed by route-counter resets.

The road recovery guard rejects every tile with a side connection. The original listing does
not impose that condition: at VA `0x324b9`, `FUN_00032428` rejects terminal road records (byte 1
equals 2), then accepts straight trajectory IDs 2, 7, 8 and 13. A straight trajectory through
a T junction or crossroads remains eligible. Sprite facing is also not an authoritative
trajectory indicator in OpenApoc's coarse movement, where the observed blocked cars can retain
their previous approach facing. A private candidate using the nonterminal, opposite-connection
guard gets all twenty cars to their original targets; its broader observations must establish
the candidate's acceptance separately.

The head-specific five-run checkpoint data, source and receipt hashes, hourly populations and
negative blockers are retained in
[civilian-traffic-reviewed-observations.csv](civilian-traffic-reviewed-observations.csv) and
[civilian-traffic-reviewed-summary.json](civilian-traffic-reviewed-summary.json). The rendered
`685032b9` app has 36 mapped vehicles at the same 12:20 alert clock as the earlier eight-vehicle
control. Its improved early density does not override the captured later failure.

## Rejected recovery candidates

The nonterminal/opposite-connection guard with adjacent-node cleanup recovers the captured
twenty-car queue. A draft that cleaned up every road path also appeared healthy after a day,
but it removed the deliberately repeated TakeOff entrance goal. That result is rejected:
altering building departure can change the observed traffic without fixing ordinary movement.

Preserving Land and TakeOff paths exposed another failure in the same fresh-game day.
Police vehicle 135 waited at `{64,46,2}` with path `{64,46,2} -> {64,47,2} -> {64,46,2}`;
civilian vehicle 813 waited at `{64,47,2}` and planned north, then west. The former tile is
a T junction with east/south/west connections, the latter a north/west corner. The maximum
mission wait was 12072477 ticks, **23.29 game hours**, and 25 cars were endpoint-stationary.
Neither car's blocked heading has an opposite connection for another recovery turn.

The recovery builder excludes the direct return direction when selecting a branch, but its
cached shortest route can immediately return through the junction it just left. Forcing that
branch creates the corner reversal above. A viable correction must reject an immediately
returning branch, preserve the original target and entrance movement, and demonstrate physical
progress from both captured failures. A clean population endpoint or reset stopped counter is
insufficient acceptance evidence.

## Recovery correction

Road recovery now permits a connected straight departure through a nonterminal junction,
using the planned heading and opposite connections as OpenApoc's approximation of the original
trajectory field. The selected branch node is appended once. A cached branch route that
immediately returns through the retreat junction is rejected instead of forcing that reversal.

Ordinary road movement also normalizes old adjacent duplicate nodes and removes only a leading
`A -> B -> A` excursion when `A` is the vehicle's actual owning tile. The existing shortcut
already intended to skip that excursion, but the obsolete `B` admission could block before
the shortcut ran. Moving that normalization ahead of admission repairs the persisted queue.
The actual next road step still receives its lane and crossing checks. Future excursions are
retained; Land and TakeOff keep their deliberately repeated entrance goals.

The frozen private candidate has the same production algorithms as the tracked source; its
only functional instrumentation adds completion logging. Both full saved-state queues resume
without reseeding or replacing their missions. The earlier queue has **20/20** cars move at
least two tiles and reach their captured first targets; the second has **27/27**. Counts are
unique captured vehicle/target pairs, confirmed with `target == owning tile` and
`pickedNearest == false`, rather than raw repeated completion lines. Ambient retirement is
counted only after the original target completes.

Eight separate fresh full-day candidate processes each advance exactly 12441600 ticks and end
with no road mission stopped counters at or above 12000 ticks. Final traffic is 29–32 road
vehicles plus 1–5 flyers. Their maximum sampled mission stopped counters are 24330–32629 ticks,
about **2.82–3.78 game minutes**. Three runs sample one bike at the same position five minutes
apart while it has westward velocity, no wait and no blocker: this endpoint alias is retained
as stationarity, not turned into a claim of continuous immobility. These are candidate screening
receipts; the central material-head observations remain a separate dataset.

The regression embeds all twenty observed queue members, complete routes, targets and poses,
plus the later reciprocal pair. It requires physical movement and completed original targets.
Only after confirmed arrival does the twenty-car fixture use the real Land callback to free a
shared entrance. The frozen `685032b9` control fails all 22 observed targets; the intermediate
scoped control clears the first twenty but fails the later pair. The correction passes both,
protects subsequent crossing admission and future route excursions, checks terminal/corner
rejection, and exercises actual TakeOff entrance movement and Land membership.
The central integration build and full CTest run pass **70/70** tests (39.40 seconds).

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
rules. The fine-step candidate runs below use the continuous-return implementation before the
review-driven cadence, destination-loss and cache fixes. Each run verifies its observed clock delta, rather than
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

## Earlier six-hour fine-step comparison

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

## Earlier full-day fine-step observation

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

The pre-review full CTest run passed **65/65**. The expanded traffic test checks head-on passing,
the right-hand lane, queuing and U-turns, six hours of unstocked replenishment, completed-trip
retirement, bounded pending traffic, clean building references, independent permanent fleet
purchases, destroyed-pad rejection and cleanup, and a packed marker save roundtrip with the
false default for older records. Player ownership, cargo and passengers have retirement
guards in production code. The follow-up lifecycle regression now exercises actual takeoff and
movement, then destroys every usable destination pad through `Scenery::die`. It checks silent
retirement and slot release, and protects player vehicles, cargo, passengers and permanent
fleets. A low road entrance is removed in a separate controlled case because the engine preserves
bottom scenery even when `die` is called. Destruction of a future return destination does not
retire a vehicle still travelling toward its accessible outward destination.

The follow-up turbo regression observes ten dispatches at distinct clocks across an hour
boundary and twelve real `updateTurbo()` frames, checking cap, movement and visible population.
The U-turn regression fills the real route cache to 100000 entries, forces a later candidate
search to clear it, and verifies that the already-selected shortest route survives. The fix keeps
an owned copy of that route rather than a pointer into cache storage.

The integrated review fixes passed all **67/67** targets. A further six-hour fine-step run
advanced exactly 3110400 ticks, averaged 28.600 road and 31.031 all-map vehicles after startup,
and had no long stationary road vehicles at any of its 255 post-startup checkpoints. This run
precedes the adjacent-junction correction and does not erase the negative turbo diagnostic.

A separate rendered new game ran for 120 wall seconds at the ordinary simulation multiplier.
Its clock advanced 52,488 ticks (6.075 game minutes), its final census contained 30 vehicles
on the map, and it remained in CityView. This exercises the actual menu and renderer path;
the six-hour and full-day measurements above exercise the full state simulation without
rendering each frame.

## Reproducing and reviewing

Earlier fine-step aggregate observations are committed in [civilian-traffic-observations.csv](civilian-traffic-observations.csv)
and [civilian-traffic-summary.json](civilian-traffic-summary.json). Original EXEs, ISO images,
Ghidra databases and saved game assets remain excluded from Git. Full local receipts are
under `/tmp/openapoc-traffic-observed/`.

Build with `ENABLE_TESTS=ON`, then run the census with an existing compatible save:

```sh
build/bin/traffic_census SAVE 3110400 6 --Framework.Data=DATA --Framework.CD=CD_ISO --Config.Read=0
build/bin/traffic_census SAVE 12441600 6 --Framework.Data=DATA --Framework.CD=CD_ISO --Config.Read=0
build/bin/traffic_census SAVE 3110400 43200 turbo --Framework.Data=DATA --Framework.CD=CD_ISO --Config.Read=0 --Logger.FileLevel=3
build/bin/traffic_census SAVE 12441600 43200 turbo --Framework.Data=DATA --Framework.CD=CD_ISO --Config.Read=0 --Logger.FileLevel=3
ctest --test-dir build --output-on-failure
```

`turbo` requires a save on a five-minute boundary and stops if gameplay makes turbo unavailable.
Use the explicitly diagnostic `turbo-unchecked` mode to reproduce the uninterrupted coarse
receipt; it counts and reports every unavailable frame. Such a run must not be presented as
an uninterrupted UI Speed 5 observation.

This PR includes the unpublished road-lane/civilian-mix prerequisite `bce89743` and the
replenishment fix, based on `khallmark/develop-reboot`. It does not claim parity for on-demand
taxis, the original cross-city 80-slot pool, or successful-arrival retirement. Intentional
traffic suppression during an alien incursion remains governed by the original relationship
gate. Longer campaigns, different difficulties and the user's exact launch path remain outside
the observed evidence above.
