# Ground vehicle occupancy: one vehicle per road tile, and what a blocked vehicle does

Binary `canonical/UFO2P.EXE` (ISO non-4), Ghidra project
`OpenApoc-og-research/ghidra_projects/OpenApocOG`, image base 0. Ghidra 12.1.4 was run with
`-noanalysis -readOnly`. Receipts are in `og-research/export/`:

- `traffic_01.stdout.log`: decompiles of `0x41d80`, `0x3c694`, `0x3de08`, `0x409e0`, `0x4087c` and
  `0x40e54`.
- `traffic_02.stdout.log`: the listing `0x39f40-0x3a4d0`, and decompiles of `0x3f704`, `0x58280` and
  `0x41644`.
- `traffic_03.stdout.log`: decompiles of `0x386f8`, `0x38678` and `0x41ef8`.

## What the EXE does

`FUN_000395E4` is the per-frame city vehicle update. It covers 0x50 slots at `0x160FD8` with
stride `0x276`.

1. **Prospective box.** Before stepping to the next path node, the vehicle builds a box. The box
   covers the current tile and the next node, plus the vehicle size (`+0x40/+0x42/+0x44`). See
   `0x3a039-0x3a08f`.
2. **Occupancy scan.** This is `0x3a093-0x3a133`. The scan considers every other kind-1 or kind-2
   slot that is active and in the same city (`+0x174`). It checks the prospective box against that
   vehicle's *reserved box* (`+0x56..+0x60`). On overlap it calls `FUN_00041d80(me, other)` at
   `0x3a11d`.
3. **Self-overlap exemption.** `FUN_00041d80` returns 1 iff `other`'s reserved box overlaps *my
   current tile box*, and in that case the vehicle is not blocked. A vehicle already overlapping
   the tile I stand on never blocks me. Only one purely ahead does.
4. **Stationary blocker on my destination.** This is `0x3a142-0x3a1a4`. If the blocker is parked
   on my destination tile and my `+0x12e` is below the type threshold, `FUN_000409e0`
   spiral-searches for a free nearby target.
5. **Local replan around vehicles.** The kind-2 call is at `0x3a210`. `FUN_0003f704(me, blocker)`
   builds a ±8-tile grid and marks the following impassable:
   - non-road, out-of-map and gate areas;
   - with a blocker, every other vehicle's reserved box.

   It tries up to 26 (`0x1a`) candidate walks of up to 8 cardinal steps. Each step takes the first
   direction, from a 26-byte row of the precomputed table `DAT_00182a64` (via `FUN_000386f8`),
   whose next cell is free, unvisited and passes `FUN_00041ef8`.

   Candidates are scored by `FUN_00038678` against the target, minus `0x18` when a walk ends on it.
   The lowest score wins, even if it ends farther away than the start. The whole walk becomes the
   path (`+0x7e`), and the step is retried once (`0x3a25e`).
6. **Boxed in → wait.** At `0x3a21e-0x3a257` the path resets to the current position. Then
   `0x3a290` sets `+0x70 = 0xc`, a 12-count wait, and shrinks the reserved box to the current tile.
7. **Countdown.** While `+0x70 != 0` the vehicle neither moves nor replans. The count drops by half
   the speed setting each frame (1 every other frame at speed 1, 1 at speed 2, 2 at speed 4, 3 at
   speed 6) while the clock gains the whole setting, so the wait is 24 vanilla ticks at any speed:
   96 OpenApoc ticks.
8. **Reserved boxes.** Moving vehicles reserve the union of their current and next tile
   (`0x3a322-0x3a373`).
9. **No lane offset.** Position is tile centre plus `dirTable[dir] * progress/12`. It is one vehicle
   per road tile, with no lanes.
10. **Slot deactivation.** `FUN_00058280` deactivates a slot only when a *blocker-less* replan fails
    after the path is exhausted (`0x3a016`). A vehicle block never kills a vehicle.

`FUN_00038678(a, b)` computes a distance from the per-axis |Δ| values:

```
h = 2·max(dx, dy) + min(dx, dy)
D = max(h, 2·dz) + min(h, 2·dz) / 2
```

Precisely, D = (4·max + 2·min) / 4.

## Inferences (not proven)

- **Vehicle kinds.** Kind 2 is a road vehicle and kind 1 a flyer. Kind 2 replans with the
  cardinal-only `FUN_0003f704`. `city_damage_findings.md` labels `FUN_00041644` as flying, but it
  runs after kind-2 moves here.
- **Backing out.** Nothing compares a candidate with the start position, so a car whose only free
  way is backwards backs out as far as its walk goes. That is what lets a queue unwind from its
  tail.
- **Livelock.** The EXE can livelock on a dead-straight single-width road with no side exit within
  8 tiles.
- **Vehicle cap.** The EXE holds at most 80 vehicles across both cities. A long OpenApoc learner
  save had about 148 on one map.

## OpenApoc

Before this change OpenApoc had the block (events 2-3), from fork commit `b31e0a28`, but none of
the response. A blocked car cleared its path and re-requested the same vehicle-blind route from
`City::findShortestPath` every step. It could never get past a head-on pair. Upstream OpenApoc
has no occupancy check at all: vehicles drive through each other.

### What a blocked ground vehicle does now

`VehicleMission::advanceAlongPath` asks `respondToBlocker` about the vehicle on the next tile:

| The blocker is | Response | Source |
|---|---|---|
| going the same way, or across | wait 8 ticks and try the step again, keeping the route | not the EXE (which replans) |
| coming the other way (its next tile is ours) | **drive through it** | not the EXE (see below) |
| parked, idle, at the end of its path, or boxed in | the EXE's local replan, `planAroundVehicles` (event 5) | `FUN_0003f704` |

When the replan finds no way out, the vehicle waits `BLOCKED_WAIT_TICKS` (event 6-7). Unlike the
EXE, it keeps its route and moves as soon as the next tile comes free (`boxedIn`,
`nextStepIsFree`). After `PASS_THROUGH_AFTER_BLOCKS` walks round vehicles without getting past
the tile it was stopped from entering, a vehicle drives through, as upstream always does.

A non-player road vehicle whose vehicle-blind route takes it nowhere is removed
(`Vehicle::stranded`, set in `setPathTo`), as the EXE deactivates its slot (event 10).

### Why head-on cars pass

The EXE's response to a head-on pair is the local replan. With OpenApoc's traffic it does not
resolve. A long learner game had 130-150 ground vehicles on one map, against the EXE's 80 across
both cities. Single-lane two-way stretches such as x=48, y=53-70 on the human city then carried
queues in both directions. Each car gave way, came back and gave way again to the next car coming
the other way; one Blazer Turbo Bike reversed 363 times on one tile pair. Variants measured on the
same save, 60,000 ticks at Speed4, before head-on passing:

| Head-on response | Moves that undid the one before | Worst car, reversals on one tile pair |
|---|---|---|
| replan every block (EXE event 5) | 26.8% | 363 |
| one of the pair, by name, waits; the other replans | 19.9% | 363 |
| + a boxed-in car moves as soon as its way clears | 15.0% | 108 |
| + the car that gave way waits for the other to clear its way back | 10.8-13.3% | 16-17 |
| **pass** | **0.6%** | **2** |

With passing, the number of ground vehicles stalled (position unchanged, wanting to move) fell to
0 by tick 39,600 and stayed there, and route requests fell to 0.001 a tick. The "waits for the
other to clear its way back" rule made no measurable difference once cars passed, so it is not
kept.

On a save from the user's game (`learner-g000-r0001`, day 14), 30,000 ticks:

| | before stranded removal | after |
|---|---|---|
| moves that undid the one before | 2.8% | 0.6% |
| worst car, reversals on one tile pair | 27 | 4 |
| ground vehicles removed from the map | 1 | 11 |

The 10 extra removals were cars routed to the end of a severed road. With nowhere to go they were
re-issued the same destination every few seconds, and every car routed toward that side of the
break shuttled round them.

### Approximations and gaps

- **Candidate seeding.** There is one candidate per first move, instead of the 26 rows of
  `DAT_00182a64`, which are not extracted.
- **Event 4** (stationary blocker on the destination) is not implemented. Its gate compares
  `+0x12e` with a per-type value at `DAT_00128618 + type*0x7e`. The same field is reset in
  `FUN_00058280` and gates pathfinding choices in `FUN_0003c694`. It looks like current
  health against the type's maximum, not a blocked-step counter. That is an inference.
- The same response for flyers (`FlyingVehicleTileHelper` has the block too) is not implemented.
- The 80-slot cap is not implemented.

Regression test: `tests/test_ground_vehicle_traffic.cpp`. The head-on pair must get past each
other with no reversal. It fails with the head-on response set back to the replan.
