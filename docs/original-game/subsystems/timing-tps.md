# Timing and ticks per second

Community observation of the original city and battle sim is **36 TPS**. Neither UFO2P nor TACP has a printable “36 ticks” banner.

OpenApoc uses that on purpose as `VANILLA_TICKS_PER_SECOND = 36` × `TICKS_MULTIPLIER = 4` → `TICKS_PER_SECOND = 144` ([gametime.h](../../../game/state/gametime.h)). Extracted delays that already multiply by 4 are not automatically wrong.

City Speed1 now zeros city ticks on alternate `CityView::update` frames ([cityview.cpp](../../../game/ui/tileview/cityview.cpp)), matching vanilla half-rate Speed1.

Issue [997](https://github.com/OpenApoc/OpenApoc/issues/997) is a per-mechanic audit: which rates already scale, which still run 4× too fast, and which constants (`HAZARD_SPREAD_CHANCE`, enzyme/fire ticks, `FUEL_TICKS_PER_SECOND`) are still invented or hardcoded.

TACP strings `Fire rate` and the TU-reservation copy will skew if a given mechanic uses the wrong base.
