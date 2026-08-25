# A2 follow-up · Panic psi upkeep diverges from prior art

**Status: OPEN. Locked to the code, not resolved.**

Writing the A2 lock test surfaced a genuine mismatch between
[psionics.txt](../../tools/extractors/docs/psionics.txt) and
`BattleUnit::getPsiCost` ([battleunit.cpp:35](../../game/state/battle/battleunit.cpp#L35)).

Upkeep is debited once per `TICKS_PER_PSI_CHECK`, which is half a second
([battleunit.cpp:3771](../../game/state/battle/battleunit.cpp#L3771)).

| Attack | Initial (code) | psionics.txt | Upkeep/check (code) | psionics.txt implies | |
|---|---|---|---|---|---|
| Control | 32 | 32 | 4 → **8/sec** | 8/sec | match |
| **Panic** | 10 | 10 | **3 → 6/sec** | **4/sec** (2/check) | **DIVERGES** |
| Stun | 16 | 16 | 5 → **10/sec** | 10/sec | match |
| Probe | 8 | 8 | 3 → **6/sec** | 6/sec | match |

**All four initial costs match. Three of four upkeeps match exactly. Only Panic differs.**

## Why it is not "fixed"

A single outlier among seven correct values looks like a transcription slip — `3` typed for `2`.
That is an **inference, not evidence**, and it could equally run the other way: `psionics.txt` is
prior-art, opening with *"Vanilla seems to work this way"*, and is not a recovered table. Neither
side is authoritative, and **TACP has not been asked**.

Per the [prime directive](../parity-guide.md#0-prime-directive), a prior-art document does not
license editing a shipped constant, and a shipped constant does not license editing the document.
So the lock test asserts **current behaviour** (3 per check), with the divergence recorded in a
comment at the constant. Editing `battleunit.cpp` now fails the test — which makes any future
change deliberate instead of silent.

## To close it

Bind the psi upkeep debit in TACP and read the Panic value directly. Whichever side is wrong, fix
that side and cite the binary + generation + file offset.

Until then this row stays open. **Do not resolve it by argument.**

## Two testability gaps found alongside

Both block *verification*, not behaviour, and both have precedent in this tree —
`TacticalAIVanilla::retreatChancePercent` was already given a seam for exactly this reason
(see [test_tactical_ai_retreat.cpp](../../tests/test_tactical_ai_retreat.cpp)).

1. **`getPsiCost` has internal linkage.** It is `static` at namespace scope in the header with its
   only definition in the `.cpp`, so no test can call it. The A2 case
   `psi_costs_match_prior_art` therefore asserts against mirrored constants and **can never fail** —
   it is provenance, not coverage. The case that does execute production code is
   `psi_upkeep_per_second`, which drives `updatePsi()`.
2. **`UnitAIVanilla::getWeaponDecision` / `getGrenadeDecision` are private**, and the only public
   entry routes through `canAttackUnit()` → `hasLineToUnit()` → `map.findCollision()`, requiring a
   populated battle map that no test in this suite builds. **A4 as originally specified cannot be
   closed without a seam.** The landed `test_unit_ai_priority.cpp` locks the formula's math contract
   and the AOE net-damage rule against synthetic tuples — clearly labelled as *not* executing game
   code — plus a real-data sanity guard over every fireable weapon's
   `(fire_delay, damage, accuracy)`.

Adding both seams is a small, precedented change to `game/`. It was out of scope for the
tests-only task that found them.
