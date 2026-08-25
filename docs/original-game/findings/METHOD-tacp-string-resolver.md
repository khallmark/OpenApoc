# Method · TACP combat-message resolver — status after the B3 cross-check

**Read [METHOD-tacp-string-regions.md](METHOD-tacp-string-regions.md) first.** That file is the
primary, actively-maintained writeup of the packed message pool, the asset-name table, the B1
false-negative correction, and the confirmed sparse pointer table at object2 `0x292D18`–
`0x292DEC`. This file records only what RE-wounds (B3/G1) added on top of it, so the two don't
drift out of sync by duplicating the same table dump twice.

## What this file adds

The coordinator asked whether the confirmed pointer table explains any of the B3 wound-message
strings. It does not, and — more usefully — its proposed same-category control doesn't either:

| String | In the `0x292D18`–`0x292DEC` table? | Own direct xref |
|---|---|---|
| `Unit critically wounded` (`0x2E0400`) | No | 0 |
| `TU cost per wound: ` (`0x2E0204`) | No | 0 |
| `TU cost per shot: ` (`0x2E00EF`) — proposed control | **No** | **0** |

Full table contents (all ~24 non-null entries, dumped `0x292C00`–`0x292E40` to confirm the table's
true start is `0x292D18` exactly — everything below that address is the string bytes of the
table's own first entry, `PLEASE PUT THE XCOM CD BACK IN THE DRIVE`, not more pointers):

`Empty`, ` per unit`, `X-COM`, `All your units are unconscious or dead..`, ` January, `, `Health`,
`Alien Egg`, `Rookie`, `Psi-drain`, `Explosive`, `Ammo Clip`, `Weight:`, `Pause`, `The following
units will be lost if left in combat zone:`, `Hostile unit spotted`, `Search the building for
Alien life forms...`, `MISSION BRIEFING`, `Smoke`, `BLANK`, `Monday`.

Two trailing slots (`0x292DF4`, `0x292DF8`) hold the literal value `1`, not a string pointer, with
real xrefs (4 and 7) — read as a small counter/flag pair immediately after the pointer array, not
as further table entries. Not chased further.

**Conclusion:** the whole `TU cost per X` family (wound / shot / throw / use / activate / attempt)
is absent from this mechanism as a block, not selectively — so its absence doesn't discriminate
between "wound-cost path is genuinely unused in the shipped build" and "this table simply isn't
the resolver for combat/action-cost messages, only for UI-widget labels and one-shot system
dialogs." The table's real contents (short widget labels, `MISSION BRIEFING`, the CD-check
message) support the latter: this looks like linker-packed individual `char *` globals for a
different subsystem than whatever prints combat-log text during play.

## Angles from METHOD-tacp-string-regions.md — status

Of the three angles listed there:

1. **Pool's true start / index table** — largely answered for *this* table (start is `0x292D18`
   exact, confirmed by dumping past both boundaries). The wound-message pool itself still has no
   known base or index table; this table isn't it.
2. **Ordinal-first, call site with N/N+1/N+2** — not attempted. Given angle 1's result (the one
   *known* resolver mechanism in this pool is per-string linker globals, not an ordinal-indexed
   array), an ordinal scheme would have to be a *second*, still entirely unfound mechanism.
   Reasonable next step for whoever picks this back up, but it's a fresh, unbounded search with no
   anchor yet — not something this investigation could responsibly time-box further.
3. **`Deployment Failed` / spawn-placement-failure trigger** — checked its own direct xref only
   (0, consistent with everything else in the pool); did not pursue the "find the trigger
   structurally, see how it names its message" plan. That is a standalone RE task (locating the
   spawn/deployment code with no string anchor at all) or comparable scope to B1's cover-metric
   search, and belongs with whichever agent is already working battle-setup/deployment code rather
   than being started fresh here.

## Transferable lesson: verify a control in-project before trusting it

This session and the parallel B1 session each burned a round on an unverified "positive control":
B1 borrowed a cross-project address that turned out to sit at a uniform slide from this project's
copy; here, `TU cost per shot: ` was proposed as a same-pool, same-category sibling of `TU cost
per wound: ` on the strength of resemblance alone, and turned out to have zero xrefs and no
table entry either — it was never actually a positive.

**Category or textual resemblance is not sufficient to certify a control.** Before using any
string as a "known-good" baseline for a negative result, check its own xref count and (if a
resolver table is in play) its own table membership in *this* project, same as the target. If the
control also comes back empty, it was never a control — the negative result it was meant to
validate is not established, and the search needs a different anchor entirely.

## Verdict

**NOT BOUND — the confirmed pool-pointer-table mechanism does not explain the wound/TU-cost
message family; no second resolver mechanism was found or ruled out.** B3 stays closed as a
recorded negative (see [B3-G1-wounds-gadgets.md](B3-G1-wounds-gadgets.md)) on the strength of the
five methods listed there. The general resolver hunt (angles 2 and 3 above) is not exhausted and
is left open for whoever next has a concrete anchor into it — recommend starting from
[B1-cover-metric.md](B1-cover-metric.md), which owns the deeper investigation of this pool and the
pointer table's origin.
