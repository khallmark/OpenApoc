# City vehicle movement and civilian traffic: road vehicles, flyers and ATVs

Binary `canonical/UFO2P.EXE` (ISO non-4), Ghidra project
`OpenApoc-og-research/ghidra_projects/OpenApocOG`, image base 0. Ghidra 12.1.4 was run with
`-noanalysis -readOnly`. Receipts are in `og-research/export/`:

- `traffic_01..03.stdout.log`: `FUN_000395E4`'s helpers (`0x41d80`, `0x3c694`, `0x3de08`,
  `0x409e0`, `0x4087c`, `0x40e54`, `0x3f704`, `0x58280`, `0x41644`, `0x386f8`, `0x38678`,
  `0x41ef8`) and its listing `0x39f40-0x3a4d0`.
- `traffic_04..08.stdout.log`: callers of the slot allocator `FUN_0005d68c`, the park counters
  `DAT_00170ec0`, the vehicle array `0x160FD8`, and the vehicle initializer `FUN_0005d6e4`.
- `traffic_09..13.stdout.log`: the city frame loop `FUN_00010908` around `0x10f0d`, the road
  vehicle update `FUN_000303e4` and everything it calls.
- `traffic_14..17.stdout.log`: the traffic generators `FUN_00034860`, `FUN_0007a730` and
  `FUN_0003280c`, their scheduler `FUN_0006d384`, the cap `FUN_00034b14`, and the tables
  `0x2FB40`, `0xD6A00` and `0xE6A30` (`DumpRoadTables.java`, `DumpWords.java`).

An earlier version of this note described `FUN_000395E4` as the road-vehicle mover. It is not:
road vehicles never run it.

## Vehicle kinds

The vehicle initializer copies the type's `vehicle_data.movement_type` into the slot's `+0x02`
(`FUN_0005d6e4`, `0x5d70e-0x5d775`: `MOV DX,[0x1285e8 + type*0x7e + 2]; MOV [EBP+2],DX`). The
extractor reads that field as 0 = Road and 1 = Flying or UFO
(`tools/extractors/extract_vehicles.cpp:158,191`); its remaining branch is ATV. Every update
below dispatches on `+0x02`. EXE type indices follow the name table at string `0x1495a2`: 0-9 the
alien craft, 10 Police Hovercar, 11 Airtaxi, 12 Rescue Transport, 13 Construction Vehicle,
14 Airtrans, 15 Space Liner, 16 Phoenix Hovercar, 17 Hoverbike, 18 Valkyrie, 19 Hawk, ...,
25 Autotaxi, 26 Autotrans, 27 Police Car, 28 Civilian Car, 29 Stormdog, 30 Wolfhound APC,
31 Blazer Turbo Bike, 32 Griffon AFV.

The city frame loop `FUN_00010908` runs `FUN_000303e4`, the road vehicles (kind 0), at `0x10f0d`.
It then runs `FUN_000395E4`, kinds 1 and 2, at `0x10f12`.

## Road vehicles (kind 0)

`FUN_000303e4` walks the 80 slots and takes kind 0 in the current city. For each it runs
`FUN_00033818` (the step), `FUN_00032428` (stuck handling) and `FUN_000325a8` (position).

1. **Movement.** A car moves tile to tile along the road tile's `connection[dir]`. The road
   record is `DAT_00153068 + scenery*0x34`: byte 0 = road, byte 1 = junction, bytes 2-5 = NESW
   connections. At a junction the car takes the next direction of its stored route
   (`+0x72[+0xce]`); elsewhere it takes the one exit that is not back. Inside a tile it follows a
   trajectory chosen by entry side and exit heading: `+0x62 = entry + exit*4`, plus 0x10 or 0x20
   for slopes. The trajectories are in the table at `0xD6A00`: 8-byte entries of a point list,
   length and speed, with points (dx, dy, dz, facing) in a 32-unit tile.
2. **Two lanes, right-hand traffic.** The straight trajectories run at a fixed off-centre line.
   Northbound (2) is at x = 20 and southbound (8) at x = 11. Eastbound (7) is at y = 20 and
   westbound (13) at y = 11. That is ±4.5 about 15.5, to the right of the way of travel.
   Trajectories 0, 5, 10 and 15 are U-turns within the tile.
3. **Same tile** (`FUN_00031a4c`). Another road vehicle blocks me only if it is on my tile with
   **my heading** (`+0x66`) and ahead of me in progress (`+0x64`). I take its speed and set the
   blocked flag `+0x104`.
4. **Next tile** (`FUN_00031be0`). A road vehicle on my next tile caps my speed if its heading is
   my next heading (`+0xfa`): I queue behind it. One with any other heading holds me up only on
   a **junction** tile. There it does so only where the 16×16 trajectory table `DAT_000e6a30`
   (`[mine*16 + theirs]`) says the two ways cross. For example, northbound straight (2) clears
   southbound straight (8), the left turn W→S (11) and the right turn N→W (12).
5. **Two cars entering the same junction tile** (`FUN_00031f1c`). Priority goes to the faster car,
   then the one farther along, then the lower slot. The same conflict table applies.
6. **Opposite-direction cars on ordinary road never block each other.** One vehicle per road tile
   is not the road rule.
7. **Stuck breaker** (`FUN_00032428`). While speed `+0x6e` is 0, the count `+0x70` gains half the
   speed setting a frame. At `0x3d` (61), on a straight trajectory (2, 7, 8, 0xd), the car turns
   round. Its heading goes ±2 and its trajectory becomes the same-side U-turn (8→0, 0xd→5, 2→10,
   7→0xf). It also sets path index `+0xce = 0x31`. The clock gains the whole setting a frame, so
   61 counts is 122 vanilla ticks, 488 OpenApoc ticks. Any movement resets the count.
8. **No road to the destination.** A road vehicle whose route request fails is deactivated:
   `FUN_00034860` calls `FUN_00058280` at `0x34af2` when `FUN_0004dd14` fails at `0x34a4b`.

Inferences: the tile `FUN_00031be0` tests for "junction" is the mover's next tile, and
`+0xce = 0x31` makes the route be planned again at the next junction. Neither is verified.

## Flyers and ATVs (kinds 1 and 2): `FUN_000395E4`

The update covers the 0x50 slots at `0x160FD8`, stride `0x276`, and skips kind 0.

1. **Prospective box.** Before stepping to the next path node, the vehicle builds a box over the
   current tile and the next node, plus its size (`+0x40/+0x42/+0x44`; `0x3a039-0x3a08f`).
2. **Occupancy scan** (`0x3a093-0x3a133`). The box is checked against the reserved box
   (`+0x56..+0x60`) of every other active kind-1 or kind-2 slot in the city. On overlap the scan
   calls `FUN_00041d80(me, other)` at `0x3a11d`.
3. **Self-overlap exemption.** `FUN_00041d80` returns 1 iff the other's reserved box overlaps my
   *current* tile box, and then I am not blocked.
4. **Stationary blocker on my destination** (`0x3a142-0x3a1a4`). `FUN_000409e0` spiral-searches
   for a free nearby target.
5. **Local replan.** The kind-1 call is `FUN_0003c694`, a 26-direction 3D planner. The kind-2 call
   is `FUN_0003f704(me, blocker)` at `0x3a210`.
   - It builds a ±8-tile grid and marks impassable the non-road, out-of-map and gate cells, and,
     with a blocker, every other vehicle's reserved box.
   - It tries up to 26 greedy walks of up to 8 cardinal steps. Each step takes the first direction,
     from a row of the table `DAT_00182a64` (via `FUN_000386f8`), whose next cell is free,
     unvisited and passes `FUN_00041ef8`.
   - Walks are scored by the distance `FUN_00038678` (`h = 2·max(dx,dy) + min(dx,dy)`, folded the
     same way with `2·dz`), less `0x18` for ending on the target. The lowest score wins, even one
     farther than the start.
   - The whole walk becomes the path, and the step is retried once (`0x3a25e`).
6. **Boxed in → wait.** The path resets to the current position (`0x3a21e-0x3a257`). Then
   `0x3a290` sets `+0x70 = 0xc`, a 12-count wait (96 OpenApoc ticks), and shrinks the reserved box
   to the current tile.
7. **Slot deactivation.** `FUN_00058280` deactivates a slot when a *blocker-less* replan fails
   after the path is exhausted (`0x3a016`).

## Civilian traffic

`FUN_0006d384`, in the human city only, counts `DAT_000d508c` down by the speed setting each
frame. At 0 it reloads the count with `0x438` (1080 vanilla ticks, 4320 OpenApoc ticks) and calls
`FUN_00034860`, which **spawns fresh vehicles**.

- **Batch size.** It requests `B/2 + rand(0..B)` vehicles. B is from the 24-word table at
  `0x2FB40` (`4 2 2 2 2 2 4 8 12 8 6 4 10 8 6 4 4 12 12 8 4 4 4 4`), indexed by the word at
  `0xD4D68`. That index is taken to be the hour, an inference.
- **Cap** (`FUN_00034b14`). The batch is cut to the free slots less a reserve of 45 minus X-COM's
  vehicles (25 once X-COM has 20). So at most about 35 other vehicles are about at once.
- **Type** (`FUN_0005d1d8(16)` at `0x348ff`, a roll of 0-16):

  | roll | share | vehicle |
  |---|---|---|
  | 0-5 | 6/17 | Civilian Car |
  | 7-9 | 3/17 | Autotaxi |
  | 14-16 | 3/17 | Blazer Turbo Bike |
  | 6 | 1/17 | Construction Vehicle |
  | 10 | 1/17 | Airtaxi |
  | 11 | 1/17 | Airtrans |
  | 12 | 1/17 | Autotrans |
  | 13 | 1/17 | Rescue Transport |

- **Source and owner.** The vehicle starts at a random building with road data and is owned by
  that building's owner, at least org 2 (`0x349ca-0x349d7`). It is sent to a random building.
- **Alien gate.** While an alien vehicle is about in the human city, only an owner whose relation
  to the aliens exceeds 74 sends any (`FUN_00091de4`, `0x349ec`). The matrix `DAT_0016ec28` holds
  the same -100..100 relations as OpenApoc.
- **Taxis on demand.** `FUN_0003280c`, the people update, calls `FUN_0007a730`. That spawns an
  Autotaxi, or an Airtaxi when the person's `+0x22 == 6`, to carry a person who needs to travel.
- **Hovercars.** Neither generator sends Phoenix Hovercars or Hoverbikes; those launch as building
  defenders (`FUN_0005ca04`). Other callers of `FUN_0005d68c` have not been read:
  `FUN_00015400`, `FUN_00010380`, `FUN_000ab440`, `FUN_000a1f9c` and `FUN_000a238c`.

## OpenApoc

- **Road vehicles** (`VehicleMission::advanceAlongPath`, `roadBlocker`, `roadUTurn`) follow events
  3-7 of the road rules:
  - a car is held up only by a road vehicle on, or entering, its next tile that leaves it the same
    way, or, on a junction tile, one whose way across crosses its own (`DAT_000e6a30`, transcribed);
  - while held up it keeps its route and tries again every `QUEUE_WAIT_TICKS`;
  - after `ROAD_UTURN_TICKS` (488) on a straight it drives back to the last junction, where the
    route is planned again;
  - a road vehicle's goal is `ROAD_LANE_OFFSET` (4.5/32 tile) right of the tile centre.

  Other vehicle kinds do not hold road cars up, as the EXE's road scans see only kind 0.
- **ATVs** keep the kind-2 rules: occupancy, `planAroundVehicles` (`FUN_0003f704`), and the 12-count
  wait. A head-on pair passes, a deliberate deviation measured below.
- **Civilian traffic** is `City::dispatchAmbientTraffic`, run every `AMBIENT_TRAFFIC_TICKS` while the
  human city is current. It uses the batch size, cap, type mix and alien gate above. The
  hand-written per-organisation patterns that sent only cars and bikes are gone from the
  extractor, and `GameState::initState` drops them from older saves. One of them named
  `VEHICLETYPE_AIRRANS`, a type that does not exist. Megapol's police patrols and Transtellar's
  space liners remain.

Deviations:

- **Parked vehicles, not fresh spawns.** OpenApoc's organisations own their vehicles. So a trip
  takes a parked vehicle of the drawn type from a building whose owner owns it, and sends it home
  again if it visited another organisation. A type with none parked is skipped, so the observed
  mix leans away from types few own: only Transtellar owns Autotaxis and Airtaxis.
- **Reachable destinations only.** Road trips go only where the cached road route reaches, and air
  trips only between buildings with landing pads. A road car with no route is removed (event 8).
- **The cap counts this city's map**, not 80 slots across both cities.
- **Taxis on demand** (`FUN_0007a730`) are not implemented; OpenApoc agents travel by other means.

### Measured: head-on passing for ATVs, and how road cars fared before

Before road cars had their own rules, they used the kind-2 rules above. On a long learner game
(130-150 ground vehicles on one map, against the EXE's 80 across both cities), cars gave way, came
back and gave way again on single-lane stretches; one Blazer Turbo Bike reversed 363 times on one
tile pair. Variants measured on the same save, 60,000 ticks at Speed4:

| Head-on response | Moves that undid the one before | Worst car, reversals on one tile pair |
|---|---|---|
| replan every block (EXE kind-2 event 5) | 26.8% | 363 |
| one of the pair, by name, waits; the other replans | 19.9% | 363 |
| + a boxed-in car moves as soon as its way clears | 15.0% | 108 |
| + the car that gave way waits for the other to clear its way back | 10.8-13.3% | 16-17 |
| **pass** | **0.6%** | **2** |

That is why ATVs pass head-on. For road cars, passing is the EXE's own rule.

Regression test: `tests/test_ground_vehicle_traffic.cpp`. It checks four things:

- a head-on pair passes;
- a road car keeps right;
- a car queues behind a stopped one and turns round after `ROAD_UTURN_TICKS`;
- the dispatcher sends Autotaxis, Airtaxis, Construction Vehicles and Rescue Transports as well
  as cars and bikes, and no organisation still schedules civilian trips itself.
