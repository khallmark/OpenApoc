# TPS is a 4× scalar over the observational canon

OpenApoc intentionally selects the repository/community-observed **36 TPS** compatibility base for
the original city and battle simulation. The canonical ISO non-4 UFO2P
[`FUN_0006d384`](../original-game/binaries/ufo2p.md#systems-still-thin-in-openapoc) evidence at VA
`0x6D384` / file `0xCFA28` contains the recovered invasion-delay coefficients. Their exact
unit-free ratio **1:60:86400** corroborates that interpretation but does not establish absolute
cadence: 18 TPS would preserve the same ratio with two-second, two-minute, and two-day units.

OpenApoc sets

`TICKS_PER_SECOND = VANILLA_TICKS_PER_SECOND * TICKS_MULTIPLIER` → 36 × 4 = 144.

There is no printable “36 ticks” string or recovered external-clock binding in UFO2P or TACP. The
engineering hazard is the multiplier plus hardcoded copies that do not scale with it;
`FUEL_TICKS_PER_SECOND` now derives from `TICKS_PER_SECOND` rather than retaining its historical
literal 144.

Do not flip the global and walk away. Rebase one subsystem at a time against
original observation. Locks: `test_gametime`, `test_battle_hazard`.
