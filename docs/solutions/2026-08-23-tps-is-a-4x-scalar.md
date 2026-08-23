# TPS is a 4× scalar, not a missing table

Original city/battle simulation is 36 ticks per second. OpenApoc sets

`TICKS_PER_SECOND = VANILLA_TICKS_PER_SECOND * TICKS_MULTIPLIER` → 36 × 4 = 144.

There is no printable “36 ticks” string in UFO2P or TACP. The lie is the
multiplier plus hardcoded copies (`FUEL_TICKS_PER_SECOND = 144`).

Do not flip the global and walk away. Rebase one subsystem at a time against
original observation. Locks: `test_gametime`, `test_battle_hazard`.
