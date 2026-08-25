# Method · TACP string regions, and why xref-anchoring gives false negatives

**Applies to every TACP investigation. Read before anchoring on a printable string.**

A zero-xref result on a TACP display string is **not evidence of a missing consumer**. It is the
expected result for that entire class of string, and treating it as a negative has already produced
one wrong verdict in this project.

## Two regions, two access patterns

### Packed message pool — `0x2DE000`–`0x2E2FFF` (non-4)

Variable-length, null-terminated, tightly packed: consecutive offsets differ by **exactly
`strlen + 1`**. Holds UI labels and combat messages.

| Offset | String |
|---|---|
| `0x2DFE51` | `Cautious mode` |
| `0x2DFE5F` | `Aggressive mode` |
| `0x2DFEAE` | `Kneel down` |
| `0x2E01C4` | `Reserve TUs for kneel` |
| `0x2E0204` | `TU cost per wound: ` |
| `0x2E0400` | `Unit critically wounded` |
| `0x2E0438` | `Unit under fire` |
| `0x2E048E` | `Unit has gone beserk` — **one `r`** |
| `0x2E2690` | `Entropy Enzyme` |

Entries in a packed pool are reached through a pointer/offset table or a computed index.
**Individual strings here can never carry a direct code xref.**

> **Correction (B1 run, 24 Aug):** "never" is too strong — falsified by direct counter-example.
> A live sparse pointer table sits at object2 `0x292D18`–`0x292DEC` (non-4 file
> `0x2E27BC`–`0x2E2890`), holding ~30 absolute 4-byte pointers into this exact pool, each with a
> real, single `getReferencesTo` xref back to the table slot. Confirmed entries include `Ammo Clip`
> (`0x2DF55B`), `Weight:`, `Pause`, `Health`, `Rookie`, `Psi-drain`, `Explosive`, `Smoke`,
> `BLANK`, `Monday`, `MISSION BRIEFING`, and — the important one — `Hostile unit spotted`
> (no-colon copy, `0x2E03B1`) and `The following units will be lost if left in combat zone:`
> (`0x2E0361`), both combat/status **message** strings, the same category as `Unit under fire` /
> `Unit has gone beserk`. So message-category pool strings *can* carry direct xrefs — it depends on
> whether that specific string happens to be bound to an individually-initialized global `char*`
> (this table's likely origin: the linker packing separately-declared `char *label = "...";`
> globals together) rather than reached through the id/token/ordinal mechanism this document
> argues for. **Revise the "never" claim to**: a pool string with zero xrefs is not informative on
> its own, but a *sibling in the same pool, same category, with a confirmed xref* is a real,
> checkable positive control — use one before writing off a target as consumer-less. Full detail,
> including the table dump and the B1 anchor strings' absence from it (checked against the whole
> `.object2` segment, not just this table): `B1-cover-metric.md` §2.3–§2.4.

### Asset-name table — `0x2F2000`–`0x2F3400` (non-4)

Fixed **`0x2E` (46-byte) stride**. Internal resource-load keys, not display text.

| Offset | String |
|---|---|
| `0x2F205A` | `cultboss` |
| `0x2F20B6` | `senator` |
| `0x2F2B50` | `ToxiGun` |
| `0x2F2F44` | `Medi-kit` |
| `0x2F302A` | `legs1` |

These **do** carry direct xrefs, because they are looked up to load resources.

## The trap

Using an asset-name string as the "known-positive control" for a pool-string search **validates the
tooling but not the inference**. The control returns xrefs; the target returns none; the searcher
concludes the consumer is absent. It is not — the two strings are reached by different mechanisms.

Compounding it, **the same text appears in both regions**: `Medi-kit` at `0x2DEAB3`, `0x2DF620`
*and* `0x2F2F44`; `Personal Cloaking Field` at `0x2DEA7A` and (lowercase `field`) `0x2DF5F2`.
Anchoring on the wrong copy guarantees a false negative.

Note also the misspelling `beserk`, and `Personal Cloaking field` with a lowercase `f` — exact-match
searches miss both.

## Use structural entry instead

1. **Find the pool's index/offset table.** Look for code loading an address at or just below
   `0x2DFE0E`, or a table of 32-bit values landing inside `0x2DE000`–`0x2E2FFF`. That table bridges
   message id → code and unlocks the whole class.
2. **Enter from the struct, not the text.** Find where a field is *written*, then enumerate its
   *readers*. Examples: the AI mode field for cover behaviour; extracted equipment type ids
   (`tools/extractors/common/aequipment.h`) for gadget consumers; the already-bound hazard functions
   for enzyme.
3. **Enter from a sibling that is already bound.** The fire work
   (`FUN_0007c110`, `FUN_0007ad94`, `FUN_0007ae18`, `FUN_0007b3dc`) is a live entry point into the
   whole hazard subsystem.

## Wording rule for negative verdicts

A `NOT BOUND` verdict must state **which structural method** was exhausted:

- Not acceptable: *"the string had no xrefs, therefore no consumer exists."*
- Acceptable: *"no consumer found via pool-table entry and unit-mode-field reader enumeration;
  here is what each ruled out."*

`NOT BOUND` remains a successful outcome — but only when reached by a method capable of finding a
positive.

## Four methods exhausted — the access is almost certainly token-based

The B3 investigation went well past xrefs and still found nothing, on TACP:

1. `getReferencesTo` on the pool address and a ±0x100 window — **0**.
2. Raw 4-byte pointer scan across **all** memory for that address and for every mapped copy — **0**.
3. Full executable-code scalar-operand scan for **any** instruction whose immediate lands anywhere
   in `[0x28E000, 0x292000]` — the ~16 KB span covering all three parallel pools. **6 hits, all in
   an unrelated preceding data blob, none inside the pools.**
4. All 128 `SCASB`/`REPNE SCASB` sites (48 functions) cross-referenced against functions touching
   the pool window — 2 candidates, both decompiled, both reading a *different* table.

That is not "the xref was empty". That is four independent methods converging on zero, which
means **the string address is never formed as an immediate anywhere in the binary.**

The investigator's own caveat names the mechanism: *"accessed through a register-relative
computation whose base is built at runtime rather than as a single scalar immediate — that would
evade all four of my methods."*

**Hypothesis, now only partly supported:** display text is addressed by token/ordinal and resolved
at runtime. It survives for the *specific* strings above, but it is **not** a blanket property of
the pool — the B1 correction found a live pointer table serving ~30 pool strings, including
combat-message siblings of these very targets. A simpler competing explanation is now equally
live: **these particular strings are unused in the shipped build.**

The discriminator is cheap: take a **category-matched sibling in the same pool** and check whether
*it* is referenced. If the sibling is and the target is not, the target is dead rather than
indirected, and a `NOT BOUND` is properly grounded.

### Leverage, revised

A resolver, *if one exists*, would sit between the project and **B1, B3, K1, G1 and B5**. That
fan-out is still the argument for chasing it — but the pointer-table counter-example means it may
not exist at all, and "these strings are dead in the shipped build" would explain every negative
just as well. **Run the sibling-control check per row before investing in the general hunt.**

Angles not yet exhausted:

- **The scans may have used the wrong base.** They centred on `0x2DFE0E` (`Cautious mode`), which
  is *mid-pool*. A resolver holds the pool's **true start** — loaded once at init into a global.
  One immediate, stored once, is all there would be.
- **Ordinal-first, not address-first.** Count NUL-terminated entries from the true base to get a
  message's ordinal, then look for an emitter called with small integer constants.
  `Unit injured` / `Unit badly injured` / `Unit critically wounded` are consecutive in the pool, so
  a call site passing N, N+1, N+2 on three severity branches is an unmistakable signature —
  searchable with no address involved.
- **Anchor on a message with a findable trigger.** `Deployment Failed` fires on spawn-placement
  failure, which is structurally locatable without any string. However that path names its message
  **is** the resolver protocol.

## The same pattern exists in UFO2P

The B1 investigation found this in TACP; the O1 investigation independently hit it in UFO2P. All
eight diplomacy/bribe/rift UI strings there also have zero bound xrefs, and that agent named a
probable cause: **`FUN_00092470` stashes string-table tokens before calling `FUN_00063a00`.**

If that is the indirection, it is the UFO2P analogue of the TACP pool table — a token/id is stored
and resolved later, so no code ever points at the string body. **Binding `FUN_00092470` /
`FUN_00063a00` would unlock string-anchored entry for the whole binary**, and is probably the
single highest-leverage unexplored target in this backlog. Unchased as of this run.

Treat "zero xrefs on a UI string" as the *expected* result in **both** binaries until proven
otherwise.

## Leads noticed while mapping, not yet chased

- **`TU cost per wound: ` @ `0x2E0204`** — direct evidence the original modelled a per-wound TU
  cost (parity item B3). It sits with siblings `TU cost per shot: ` `0x2E00EF`,
  `TU cost to activate: ` `0x2E01DB`, `TU cost to use: ` `0x2E01F2`,
  `TU cost per attempt: ` `0x2E02F7`. Whatever formats that label is standing on the computation.
- **`Grow_swap(%lx)` @ `0x1CA8E`** — debug format string; the name suggests growth-stage swapping,
  possibly the Organic Factory embryo stage machine.
