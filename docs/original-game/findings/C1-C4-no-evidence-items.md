# C1–C4 · Class C re-examination

**Verdicts**

- **C1 umbilical — NOT BOUND (confirmed absent).** Close the row.
- **C2 UFO mushrooms — REOPENED. Evidence found in TACP that the gap matrix missed.** The row's
  premise was wrong; see below.
- **C3 late-campaign bombing — NOT BOUND.** No trigger string or table.
- **C4 city-wide "Apocalypse" attack — NOT BOUND (confirmed absent).** Close the row.

Method: direct search of the pre-dumped string tables in
`OpenApoc-og-research/export/strings/{UFO2P,TACP}_strings.txt`, both generations.

---

## C1 · Umbilical collapse — confirmed absent

`umbilical` (any case): **0 hits in UFO2P, 0 hits in TACP.**

The gap matrix recorded "no printable `umbilical` in UFO2P"; it is absent from *both* binaries.
There is no string, therefore no xref, therefore no consumer to find. **Close as "not in the
original as described."** The feature is player folklore.

## C2 · UFO mushrooms — the matrix premise is wrong

`mushroom`: **0 hits in UFO2P** (as the matrix says) — but **1 hit in TACP**, which the matrix does
not record because it only searched UFO2P.

TACP non-4, file offset `0x2E1468`:

> "The Organic Factory provides a construction center for Alien UFOs. In their initial stage of
> development the UFOs resemble small mushroom-like objects. These objects increase in size until
> they reach the colossal sizes of the UFOs we have encountered. When fully grown the UFOs detach
> themselves from their stem and become fully functional Alien attack vessels. **All embryonic UFOs
> must be destroyed.**"

This is the Organic Factory Ufopaedia entry, and it changes the row:

1. The mushrooms are **battlescape objects inside the Organic Factory map**, not cityscape
   feedback. The matrix files C2 under `game/state/city` — **wrong subsystem.**
2. "All embryonic UFOs must be destroyed" is phrased as a **mission objective**, the same shape as
   the other alien-building destroy-objectives.
3. OpenApoc's growth gate `UFOGrowth::craftFactoryIntact`
   ([ufogrowth.cpp:51](../../game/state/rules/city/ufogrowth.cpp#L51)) keys on the **building being
   alive**. The original text implies the objective is the **embryonic UFOs**, which may be a
   distinct set of destroyable objects with their own completion condition.

**This is now a Class B row, not Class C.** Follow-up, in order:

1. Check whether the Organic Factory battle map (alien building #7) defines destroyable
   embryo objects, and whether OpenApoc spawns them.
2. Find the TACP reader for that objective set and compare its completion condition against
   `craftFactoryIntact`.
3. Only then decide whether the city-side gate should key on the embryos rather than the building.

Related lead, unexamined: TACP `Grow_swap(%lx)` at file `0x1CA8E` is a debug-format string whose
name suggests growth-stage swapping. It may be the embryo stage machine, or unrelated.

**Do not** add a cityscape mushroom visual. Whatever this row becomes, the evidence points at the
battlescape.

## C3 · Large-UFO bombing after first alien-dimension entry — not bound

No trigger string. No recovered table. The escalation players remember is adequately explained by
mechanisms already implemented — the weekly `UFO_GROWTH_*` lists and `UFO_MISSION_PREFERENCE_*`
shifting toward Attack and Overspawn (see
[campaign-plan.md §3.2–§3.3](../campaign-plan.md#32-weekly-fleet-reinforcement)). **No separate
trigger is needed to explain the observed behaviour**, which is itself evidence against one
existing.

Close unless a trigger table turns up incidentally. Do not build a designed replacement without
labelling it `openapoc-todo / designed`.

## C4 · City-wide "Apocalypse" attack — confirmed absent

`apocalypse` (any case): 6 hits in UFO2P, 2 in TACP. **Every one is UI or title copy:**

| Offset | Binary | String |
|---|---|---|
| `0x132B3C` | UFO2P | `QUIT THE APOCALYPSE DESIGNER/EDITOR` |
| `0x14A5F0` | UFO2P | `Temple of the Apocalypse` (Cult of Sirius building name) |
| `0x14DCCC` | UFO2P | `Welcome to X-COM Apocalypse` |
| `0x1539E3` | UFO2P | virtual-memory warning |
| `0x154353` | UFO2P | `Quit X-COM Apocalypse` |
| `0x131418` | TACP | `QUIT THE APOCALYPSE DESIGNER/EDITOR` |
| `0x2DFBA8` | TACP | `Quit X-COM Apocalypse` |

None names a city-wide attack event. **Close as "not in the original as described."**
