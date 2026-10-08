# Timing and ticks per second

Community observation of the original city and battle sim is **36 TPS**. Neither UFO2P nor TACP has a printable “36 ticks” banner.

OpenApoc uses that on purpose as `VANILLA_TICKS_PER_SECOND = 36` × `TICKS_MULTIPLIER = 4` → `TICKS_PER_SECOND = 144` ([gametime.h](../../../game/state/gametime.h)). Extracted delays that already multiply by 4 are not automatically wrong.

## Real-time pace (UFO2P.EXE and TACP.EXE, non-4)

**Frame limiter.** Both games run one frame per BIOS timer tick. They do not use vsync or the PIT
rate.
- The city loop `FUN_00010908` reads INT 1Ah AH=0 at frame start and stores the sample in
  `[0xB3BA8]`. It spins at `0x110F5-0x11142` until the counter reaches sample+1.
- Tactical `FUN_00011620` does the same, spinning at `0x11887-0x118D7`.
- The `in al,0x3DA` retrace waits (UFO2P `0x29374`, TACP `0x3491C`) have no references and are dead
  code.
- The PIT is set to 30 Hz for a timer event, probably sound, and chains the BIOS tick at about
  18.2 Hz.

So the ceiling is 1193182/65536 = **18.2065 frames a second**. Below that the rate is CPU-bound.

**City clock.** `FUN_0004AC40` runs once per frame. It adds `DAT_000D4D58` to the tick word and
wraps at 36 (`0x4AC69`). The speed buttons set that value (`0x50ACC-0x50C68`):

| City speed | Setting | Game-seconds per real second |
|---|---|---|
| Pause | 0 | 0 |
| Speed 1 | 1 | 0.506 |
| Speed 2 | 2 | 1.011 (about real time) |
| Speed 3 | 4 | 2.02 |
| Speed 4 | 6 | 3.03 |
| Turbo | 0x258 | see below |

**Speed 1 is not half-rate.** The clock advances every frame at Speed 1, with no parity gate around
`FUN_0004AC40`. What does run on alternate ticks are individual subsystems that test the tick
parity:
- the jump tables at `0x108C0`, `0x108D8` and `0x108F0`;
- the speed-multiplier countdowns in `0x395E4`;
- `0x121D8`.

These run once per 2 vanilla ticks at every speed. OpenApoc used to halve the whole clock at Speed 1
(`vanillaCitySpeed1Ticks`), which made it 2.4x too slow. That halving has been removed.

**Turbo.** `DAT_000D5064 = 1` routes the loop to `FUN_000114CC`. That path skips the BIOS wait and
rounds the clock up to the next 5-minute boundary on every iteration. It is unthrottled.

**Tactical.** The speed buttons post 0/1/2/4 (`0xA7D5C-0xA7E1C`), stored in `DAT_000E6C24`.
`FUN_0003CD58` adds that to the tactical tick word each frame.

**OpenApoc.** An OpenApoc tick is a quarter of a vanilla tick. CityView advances {1,2,4,6} ticks
per step and BattleView {1,2,4}. So the original pace is four steps per original frame:
**72.826 steps a second** (`framework.cpp`, `NATIVE_SIM_STEPS_PER_SECOND`).

`Framework.SimSpeed` multiplies that rate. `Framework.TargetFPS` (default 0) overrides it with an
explicit rate. Rendering runs separately at `Framework.RenderFPS`, which defaults to the display
rate.

Measured with defaults:
- Speed 1: 72.9 ticks a second (original 72.8).
- Speed 2: 145.6 (original 145.7).
- Speed 4: 436.9 (original 437.0).

The old default of 60 steps a second was 0.82x the original pace at speeds 2-4 and 0.41x at
Speed 1.

Issue [997](https://github.com/OpenApoc/OpenApoc/issues/997) is a per-mechanic audit: which rates already scale, which still run 4× too fast, and which constants (`HAZARD_SPREAD_CHANCE`, enzyme/fire ticks, `FUEL_TICKS_PER_SECOND`) are still invented or hardcoded.

Recovered TACP fire overlays now use a global real-time scheduler matching
`FUN_0007b7f8`: every four OpenApoc ticks become one vanilla scheduler
iteration; each iteration processes `(mapY×mapZ)/72` complete X rows before
advancing the 36-count item-contact pass. Turn-based round wrap runs the
original 400-iteration batch with item contacts and without unit contacts.
Generic fire placement, spread RNG, and unit fire intensity are still unbound.

TACP strings `Fire rate` and the TU-reservation copy will skew if a given mechanic uses the wrong base.
