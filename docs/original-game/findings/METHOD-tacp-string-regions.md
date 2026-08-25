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
