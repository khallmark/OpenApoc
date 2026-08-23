# Timing and ticks per second

Original city and battle logic is documented by the OpenApoc community as **36 TPS** for simulation.

OpenApoc mixes 36, 60, and 144 in different subsystems ([issue 997](https://github.com/OpenApoc/OpenApoc/issues/997)). That skews weapon rates, explosion radii, movement, armor checks, and agent training.

Evidence kind: community observation plus inconsistent constants in [game/state](../../../game/state). Not a decompiler listing. Neither UFO2P nor TACP has a printable “36 ticks” banner; TACP does expose TU reservation and `Fire rate` strings that will skew if the tick base is wrong.

Fix incrementally, one system at a time, validated against original play — not a single global rewrite.
