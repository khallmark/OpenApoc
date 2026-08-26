# Timing and ticks per second

Community observation of the original city and battle sim is **36 TPS**. Neither UFO2P nor TACP has a printable “36 ticks” banner or another recovered absolute cadence binding.

UFO2P non-4 `FUN_0006d384` @ VA `0x6D384` / file `0xCFA28` computes an invasion delay with coefficients `0x24`, `0x870`, and `0x2F7600`. Their ratio is exactly 1:60:86400, so under the 36-TPS interpretation they are one second, one minute, and one day. This corroborates the interpretation, but does not uniquely determine it: at 18 TPS the same coefficients are exactly two seconds, two minutes, and two days. Until an absolute clock or cadence binding is recovered, 36 TPS remains the observational compatibility base.

OpenApoc uses that on purpose as `VANILLA_TICKS_PER_SECOND = 36` × `TICKS_MULTIPLIER = 4` → `TICKS_PER_SECOND = 144` ([gametime.h](../../../game/state/gametime.h)). Extracted delays that already multiply by 4 are not automatically wrong.

City Speed1 now zeros city ticks on alternate `CityView::update` frames ([cityview.cpp](../../../game/ui/tileview/cityview.cpp)), matching vanilla half-rate Speed1.

Issue [997](https://github.com/OpenApoc/OpenApoc/issues/997) is a per-mechanic audit: which rates already scale, which still run 4× too fast, and which constants remain invented or hardcoded. `FUEL_TICKS_PER_SECOND` now tracks `TICKS_PER_SECOND`; `FUEL_TICKS_PER_UNIT` scales with the resolution multiplier while preserving its multiplier-4 calibration, but the original provenance of that 40000 threshold remains unknown.

Recovered TACP fire overlays now use a global real-time scheduler matching
`FUN_0007b7f8`: every four OpenApoc ticks become one vanilla scheduler
iteration; each iteration processes `(mapY×mapZ)/72` complete X rows before
advancing the 36-count item-contact pass. Turn-based round wrap runs the
original 400-iteration batch with item contacts and without unit contacts.
Generic fire placement, spread RNG, and unit fire intensity are still unbound.

TACP strings `Fire rate` and the TU-reservation copy will skew if a given mechanic uses the wrong base.
