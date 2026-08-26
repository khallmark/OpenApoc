# TPS is a 4× scalar over the observational canon

OpenApoc intentionally selects the repository/community-observed **36 TPS** compatibility base for
the original city and battle simulation. UFO2P's recovered invasion-delay coefficients are in the
exact unit-free ratio **1:60:86400**, which corroborates that interpretation but does not establish
absolute cadence: 18 TPS would preserve the same ratio with two-second, two-minute, and two-day
units.

OpenApoc sets

`TICKS_PER_SECOND = VANILLA_TICKS_PER_SECOND * TICKS_MULTIPLIER` → 36 × 4 = 144.

There is no printable “36 ticks” string or recovered external-clock binding in UFO2P or TACP. The
engineering hazard is the multiplier plus hardcoded copies that do not scale with it;
`FUEL_TICKS_PER_SECOND` now derives from `TICKS_PER_SECOND` rather than retaining its historical
literal 144.

Do not flip the global and walk away. Rebase one subsystem at a time against
original observation. Locks: `test_gametime`, `test_battle_hazard`.
