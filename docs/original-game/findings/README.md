# Parity R&D findings

Raw reverse-engineering results feeding [../parity-guide.md](../parity-guide.md).

One file per investigation. Each opens with a verdict line:

- `BOUND: <summary>` — a real function/constant was located. Cite binary + generation + **file
  offset**, never a raw Ghidra VA.
- `NOT BOUND: <what was searched and ruled out>` — **this is a successful outcome.** A recorded
  negative result ("no consumer exists in the original") closes a parity row honestly and stops
  the next person re-walking the same dead end.

Nothing here may be promoted into `game/` as a constant unless its verdict is `BOUND`. See the
prime directive in [../parity-guide.md](../parity-guide.md#0-prime-directive).
