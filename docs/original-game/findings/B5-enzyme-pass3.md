# B5 · Entropy Enzyme, pass 3 — closing pass 2's open thread

## Verdict: the "type 1 vs. type 3" question was malformed

Neither is Enzyme — the field is a damage-type index, and Entropy's damage type is structurally
excluded from the whole dispatcher.

Pass 2 left one open thread: trace `EDI`'s origin at the struct-creation site inside
`FUN_0003D9E4` (the point where a byte later becomes `DAT_003009A0`, the field
`FUN_0007D67C` dispatches on) and follow it to whoever chooses that byte. This session closed that
thread completely: proved — bit-for-bit, register and stack slot by register and stack slot — the
one link pass 2 had flagged as an unconfirmed inference, followed the producer chain all the way
back to the game's item catalog, and identified what the field actually *is*.

**`DAT_003009A0` is not a bespoke "blast/effect-kind selector" with an arbitrary 0–6 enumeration.**
It is the item's **`damage_type` catalog index** (0–18, the same enum OpenApoc's own extractor
already defines as `DT_SMOKE`/`DT_AG`/`DT_INCENDARY`/.../`DT_ENTROPY`), and `FUN_0007D67C`'s
`CMP AL,0x6 / JA default` is simply a bounds check that gives real ground-overlay/hazard behavior
to damage types 0–5 and treats everything else (6, and 7 upward) as a no-op. Ten independent
category→table entries were named against `tools/extractors/docs/hexa.txt`'s Appendix D and all ten
land on semantically correct values (§5) — including a direct hit confirming `DAT_003009A0`'s
values 0/1/2/3 are literally `DT_SMOKE`/`DT_AG`/`DT_INCENDARY`/`DT_STUNGAS`, not an independent
enum pass 2 had to guess at.

**Under that mapping, "type 1" is Alien Gas and "type 3" is Stun Gas — neither is Enzyme.**
`DT_ENTROPY = 16` (`tools/extractors/extract_agent_equipment.cpp:54`), which is *outside*
`FUN_0007D67C`'s valid `0–6` range — the same bounds check that turns damage type 6 (`PSI-Grenade`,
confirmed §5) into a no-op turns damage type 16 into a no-op too, structurally, regardless of
whether an Entropy item's damage-type byte ever reaches this table at all. It doesn't reach it
anyway: §5 also shows the two catalog items with "Entropy" in their name (Entropy Launcher, Entropy
Pod) are gated out of this table before the dispatcher is ever reached, for two different reasons.
Two independent structural facts — Entropy's damage-type value is out of the dispatcher's range,
*and* Entropy's items are filtered out of the table upstream of the dispatcher — both point the same
way: **the original game does not implement an Entropy Enzyme ground overlay through this
mechanism at all.** That is also exactly why OpenApoc's own extractor has to synthesize a damage
type for it that isn't in the real `damage_type_names` table
(`tools/extractors/extract_agent_equipment.cpp:280–286`, comment: *"extra enzyme entry for the
purpose of implementing the entropy launcher"*).

The original task framing — "which of type 1 or type 3 is Enzyme" — assumed Enzyme was one of the
two gas-overlay slots. It isn't; both slots are accounted for by other items, and the field itself
turns out to already be OpenApoc's own `damage_type` enum. See §6 for what remains genuinely open
(Entropy Pod's actual `flag==2` dispatch path, which is a different table entirely and wasn't
traced to a terminus — if Entropy produces any battlescape effect in the original game, it happens
there, not in the overlay-type table this document is about).

---

## 0. Binary, environment, method

`OpenApoc-og-research/canonical/TACP.EXE`, CRC32 `0xfebbe39e` (same file cited by pass 1 and pass
2). Ghidra project `ghidra_projects/OpenApocOG_TACP.rep`, queried `-noanalysis -process TACP.EXE`
(reused existing analysis; nothing re-analyzed or modified). All new `.java` query scripts this
session were written to a scratch directory outside the research lab and referenced via an
additional `-scriptPath`; nothing was written into `OpenApoc-og-research/scripts/` or
`OpenApoc-og-research/export/`. The lab's own `QueryFunctions.java` and `DumpListingRange.java`
were reused read-only, with `-log` redirected to the scratch directory. The shared Ghidra project
was contended by other concurrent sessions during this run (`Unable to lock project!` on two
separate attempts); both cleared within seconds and queries were simply retried, not forced.

All citations below were read live from the loaded Ghidra image (`Memory.getByte`/`getShort`,
`getInstructionAt`, the decompiler), never by computing a file offset and reading the raw EXE —
per this project's standing method warning that the LE image is bound non-contiguously and that
approach produces confident false negatives even against correct citations.

**`.object1` (code) file-offset delta reconfirmed** for every function this session where
`QueryFunctions.java`'s signature match (`bound_file`) succeeded: `file = VA + 0x5AAA4`.
Independently confirmed this session: `FUN_0006efc0→0xC9A64`, `FUN_0006f0c8→0xC9B6C`,
`FUN_0006f3b0→0xC9E54`, `FUN_00058fe8→0xB3A8C`, `FUN_0003c1c4→0x96C68`, `FUN_0008f338→0xE9DDC`.
Separately, `FUN_0007D2C4→0xD7D68` and `FUN_0007D350→0xD7DF4` (pass 2's own independently-confirmed
offsets) were re-verified this session with no change. **Derived** (no independent signature match
this session, offset = `VA + 0x5AAA4` by the same delta, not independently confirmed):
`FUN_00021008→0x7BAAC`. (`FUN_0006fb0c` and `FUN_000bca9c` were enumerated as `DAT_0011f118`
xrefs in §2 but are not load-bearing for any claim in this document; their file offsets are not
cited.)

**`.object2` (data) file-offset delta was *not* established or assumed this session.** The item
tables cited in §3–§5 (`0x2b1ef4`, `0x2b2854`, `0x2b261e`) live in `.object2`
(`start=000e0000 end=0033f0af`, confirmed via `Memory.getBlock`). Per the method warning, no file
offset is cited for these — they are cited by VA only, read live against the loaded `.object2`
block, not computed.

---

## 1. Closing pass 2's open thread: `EDI` is the AI order-queue entry itself

`FUN_0003D9E4` (VA `0x3D9E4`, derived file `0x98488`, 9598 bytes) decompiled cleanly this session
(it had only been read via raw listing before). Its outer loop is:

```c
puVar10 = &DAT_0011f118;
do {
  if (*(char *)(puVar10 + 1) == '\0') goto LAB_0003ff22;
  ...
  switch(*(char *)(puVar10 + 1)) {   // task-type byte, struct offset +4
    case '\x01': ...
    ...
    case '\x13':                     // 0x13 = 19, the LAST case in the switch
      ... pcVar20 (a free slot in DAT_001c6f70) ...
      *pcVar20 = *(char *)((int)puVar10 + 0xe);   // <-- this is the read pass 2 left unresolved
      ...
```

`puVar10` **is** `EDI` from pass 2's raw-listing read at file `0x995AC` — it is the loop cursor
over an AI order queue at `DAT_0011f118` (stride `0x16` = 22 bytes, confirmed by the loop's own
`(int)puVar10 + 0x16` increment elsewhere in the function), not an item/ammo catalog pointer.
`*(char *)(puVar10 + 1)` — pointer arithmetic on an `undefined4 *`, i.e. **struct offset +4** — is
the order's task-type byte; case `0x13` is the *only* switch case that creates a new
`DAT_001C6F70` hazard-tracking-struct entry, copying struct offset **+0xE** (single byte) into the
new entry's byte 0. Struct offset +0xE is read repeatedly across many other switch cases too
(confirmed a general-purpose order-payload field, not hazard-specific in itself).

## 2. The sole producer of task-type `0x13`

`QueryOrderQueueXrefs.java` (`getReferencesTo` on `DAT_0011f118`, VA `0x11F118`) found 18 direct
references across 8 functions: the allocator `FUN_00021008`; two save/serialize functions
(`FUN_0006efc0`, `FUN_0006f0c8`, both producing task-type `0xF`, unrelated); a network
deserializer (`FUN_0006f3b0`); the consumer `FUN_0003D9E4` itself; and `FUN_0008e694`,
`FUN_000bca9c` (not order-13 producers, not pursued further).

To find who writes task-type `0x13` specifically, `QueryTaskType13Writers.java` scanned every
`MOV byte ptr [reg+0x4],0x13` instruction across the whole `.object1` listing (not a scan of
`DAT_0011f118`'s xrefs, since `FUN_00021008`'s callers write through their own returned pointer,
not through the literal `DAT_0011f118` address — this is exactly the class of indirection the
project's `QueryDataRange.java` warning already flags). **Exactly one hit in the entire binary**:

```
00058ffa MOV byte ptr [EAX + 0x4],0x13    FUN_00058fe8 (VA 0x58FE8, file 0xB3A8C, confirmed)
```

`FUN_00058fe8`'s full body:

```c
void __regparm1 FUN_00058fe8(undefined2 param_1, short param_2, undefined4 param_3, undefined4 param_4)
{
  uVar3 = FUN_00021008();          // allocate a queue slot
  iVar2 = (int)uVar3;
  *(undefined1 *)(iVar2 + 4) = 0x13;
  *(undefined2 *)(iVar2 + 0xe) = *(undefined2 *)(extraout_ECX + 0x20);   // <-- the origin
  *(undefined1 *)(iVar2 + 6) = *(undefined1 *)(extraout_ECX + 4);
  ...
}
```

`extraout_ECX` is an incoming (register-convention) parameter Ghidra couldn't name formally. The
value that becomes the order's offset-`+0xE` field — and therefore, via §1, `DAT_003009A0` — is a
**16-bit word read from `[ECX + 0x20]`**, where `ECX` is a pointer supplied by the caller. Only the
low byte of this word ever reaches `DAT_003009A0` (§1's read is a single byte); every value found
in §5 fits in a byte, so no truncation issue arises in practice.

`FUN_00058fe8` has exactly 3 real callers (plus one at VA `0x59843` resolved to `function=-`, not
pursued): two call sites in `FUN_0003c1c4` (VA `0x3C1C4`, file `0x96C68`, confirmed, 2481 bytes)
and one in `FUN_0008f338` (VA `0x8F338`, file `0xE9DDC`, confirmed, 1502 bytes).

## 3. `ECX` traced to a 38-byte item-type-definition table

Raw disassembly at both `FUN_0003c1c4` call sites (VA `0x3C780` and `0x3CA22`) shows the identical
setup immediately before `CALL 0x00058FE8`:

```
MOV ECX,0x2b1ef4
ADD ECX,EAX          ; EAX = itemDefIndex * 0x26 (38), computed a few instructions earlier
```

`FUN_0008f338`'s decompile independently names the same base symbolically:
`local_24 = &DAT_002b1ef4 + (itemDefIndex) * 0x26;` — two independently-compiled call sites in two
different functions agree on the same table, same stride, same field offset. `ECX` is therefore a
pointer into a **38-byte-stride (`0x26`) table at VA `0x2B1EF4`** (`.object2`), and
`FUN_00058fe8` reads that table's **offset +0x20** (word) as the value seeding `DAT_003009A0`.

`itemDefIndex` itself comes from two more levels of indirection, both confirmed by matching raw
disassembly against the decompiler's own naming:

```
iVar3 = (ground-item-struct byte at DAT_000ea2bc[index*0x30 + 4]) * 0x12     // "category" lookup
flag  = DAT_002b2854[iVar3]                          // 18-byte-stride record, byte 0
sub   = DAT_002b2855[iVar3]                           // same record, byte 1
itemDefIndex = ( *(int*)(DAT_002b261e + sub*0x10) ) >> 0x10   // 16-byte-stride table, upper word of dword @ offset 0
```

This path is only taken when `flag == 1` **and** `itemDefIndex != -1` (both explicit gates in the
decompiled condition). `FUN_0008f338`'s own header confirms the exact same "category" byte source
via a different route: `iVar3 = (char)(&DAT_000ea2c0)[param_2*0x30] * 0x12;`
(`DAT_000ea2c0 = DAT_000ea2bc + 4`, so this is the identical field).

**Table extent, by neighbour-address collision, not by eyeballing where the data "looks clean":**
`0x2B1EF4` to the next table `0x2B261E` spans `0x72A` bytes = at most 48 entries of 38 bytes;
`0x2B261E` to `0x2B2854` spans `0x236` bytes = at most 35 entries of 16 bytes. A full dump of the
first 44 entries of the 38-byte table showed small, clean, in-range values; entries beyond that
(up to the 48-entry hard cap) were not needed for any anchor used below.

## 4. The joint pass 2 flagged as unconfirmed is now proven, bit-for-bit

Pass 2 identified `FUN_000584D0` (VA `0x584D0`, derived file `0xB2F74`) → `FUN_0007D2C4` (VA
`0x7D2C4`, file `0xD7D68`, confirmed) → `FUN_0007D350` (VA `0x7D350`, file `0xD7DF4`, confirmed) as
the passthrough chain feeding `DAT_003009A0`, but explicitly flagged the argument-position mapping
as *"an inference, not a confirmed link"* — specifically, whether the byte reaching `DAT_003009A0`
is the hazard-struct's byte 0 (`*pbVar13`) or its byte+2 (`pbVar13[2]`).

Traced this session by matching each function's own prologue stack-shadow size against the actual
`[ESP+N]` offsets used, at one of `FUN_000584D0`'s five call sites (VA `0x58527`–`0x58557`):

- `FUN_000584D0` pushes 4 values before `CALL 0x7D2C4`, in program order: (1) struct+0xE word, (2)
  struct+0x14 word, (3) **struct byte 0** (`XOR EAX,EAX; MOV AL,[ESI]; PUSH EAX`, VA `0x58541`,
  pushed 3rd-of-4 — 2nd-closest to the return address), (4) struct byte+2 (VA `0x5854D`, closest to
  the return address).
- `FUN_0007D2C4`'s prologue shadow is exactly `0xC` bytes (`PUSH ESI; PUSH EDI; SUB ESP,0x4`). Its
  own byte-sized stack reads at `[ESP+0x10]` and `[ESP+0x14]` (VA `0x7D2E0`, `0x7D327`) therefore
  map to caller pushes (4) and (3) respectively — i.e. `[ESP+0x14]` = **struct byte 0**.
- `FUN_0007D2C4` re-pushes `[ESP+0x14]`'s value (struct byte 0, unmutated — the *adjusted*
  `[ESP+0x10]` value is pushed second and becomes a different field) as the first of two final
  pushes before `CALL 0x7D350` (VA `0x7D32B`).
- `FUN_0007D350`'s prologue shadow is `0x1C` bytes (`PUSH ESI/EDI/EBP; SUB ESP,0x10`). Its
  `MOV CH,byte ptr [ESP+0x24]` (VA `0x7D356`) is the *first* of the two final pushes — i.e. the
  higher stack offset — which is exactly struct byte 0: `FUN_0007D2C4`'s own reader offset (`0x14`)
  plus `FUN_0007D350`'s shadow (`0x1C`) minus `FUN_0007D2C4`'s shadow (`0xC`) is `0x24`, matching
  exactly; the other push maps to `+0x20`, matching the decompile's `param_4`/`DAT_0030099F`, not
  `param_5`.
- `0007d3df MOV byte ptr [0x003009a0],CH` — `DAT_003009A0 = CH = param_5` = struct byte 0.

**`DAT_003009A0` = the hazard-struct's byte 0**, which is exactly what pass 2 guessed but could not
prove. It is also exactly the field `FUN_0003D9E4`'s case `0x13` writes from the order-queue's
`+0xE` field (§1), which is exactly the field `FUN_00058fe8` writes from `[ECX+0x20]` (§2–§3). The
full chain, item table to overlay selector, is now proven end to end.

## 5. Anchoring the table: `w20` is `damage_type`, not a bespoke enum — and where Entropy falls out

`tools/extractors/docs/hexa.txt`, "Appendix D: Agent Equipment Values" (0x00–0x56, 87 entries) is
this project's own hex-indexed catalog dump. Cross-referencing every category index whose
`flag==1` chain resolves to a valid `itemDefIndex`, dumped this session via
`QueryCategoryTableFull.java` and `QueryItemDefTable.java` (all values read live from `.object2`,
each row independently re-derived by hand from the raw dump, not taken on trust):

| Category (hex/dec) | Item name (Appendix D) | subindex | itemDefIndex | table+0x20 (`w20`) | `DT_*` name |
|---|---|---|---|---|---|
| `0x01` / 1 | Megapol AP Grenade | 0 | 0 | 4 | `DT_EXPLOSIVE` |
| `0x02` / 2 | Megapol Stun Grenade | 1 | 1 | **3** | `DT_STUNGAS` |
| `0x03` / 3 | Megapol Smoke Grenade | 2 | 2 | **0** | `DT_SMOKE` |
| `0x04` / 4 | Marsec Proximity Mine | 3 | 3 | 4 | `DT_EXPLOSIVE` |
| `0x05` / 5 | Marsec High Explosive | 4 | 4 | 4 | `DT_EXPLOSIVE` |
| `0x1B` / 27 | Alien Gas Grenade | 13 (`0xD`) | 31 (`0x1F`) | **1** | `DT_AG` |
| `0x1F` / 31 | PSI-Grenade | 15 (`0xF`) | 18 | 6 | `DT_PSIBLAST` |
| `0x2B` / 43 | Boomeroid | 21 (`0x15`) | 24 | 4 | `DT_EXPLOSIVE` |
| `0x31` / 49 | Vortex Mine | 26 (`0x1A`) | 29 | 4 | `DT_EXPLOSIVE` |
| `0x3B` / 59 | Incendiary Grenade | 27 (`0x1B`) | 35 (`0x23`) | **2** | `DT_INCENDARY` |

Every one of the ten is semantically exact: all four explosives (AP Grenade, Proximity Mine, High
Explosive, Boomeroid, Vortex Mine) land on `4`; PSI-Grenade — a psionic effect with no gas/fire
cloud — lands on `6`, the same value `FUN_0007D67C`'s own jump table treats as a no-op (§2 of pass
2: "case 6 is a no-op"); and the three gas items plus the fire item land on exactly the values their
names predict. Checking those four against `tools/extractors/extract_agent_equipment.cpp:39–52`
confirms the enum identity outright, not just a numeric coincidence:

```
#define DT_SMOKE 0
#define DT_AG 1
#define DT_INCENDARY 2
#define DT_STUNGAS 3
...
#define DT_PSIBLAST 6
...
#define DT_ENTROPY 16
```

The mapping extends cleanly to the two dispatcher cases this table doesn't need to reach at all,
closing out all six of `FUN_0007D67C`'s real bodies against the `DT_*` enum with zero leftovers and
zero conflicts: pass 2 (§2 of that document) describes **case 4** as "terrain-destruction pass ...
unit-damage loop ... writes **no** overlay byte at all" — exactly what a plain kinetic/HE explosive
(`DT_EXPLOSIVE = 4`) should do — and **case 5** as "unit-damage loop only ... writes no overlay
byte" — exactly `DT_STUNGUN = 5`, a direct-hit stun weapon with no persistent cloud. Combined with
case 6 (`DT_PSIBLAST`, confirmed a no-op above), all six cases in the dispatcher's jump table match
their `DT_*` identity: **0=Smoke, 1=Alien Gas, 2=Fire, 3=Stun Gas, 4=Explosive, 5=Stun Gun,
6=PSI-Blast (no-op)**. This is not four anchors generalized to six; it is six independent case
bodies, each described by pass 2 before this session ever proposed the `damage_type` reading, all
agreeing with it.

**`DAT_003009A0` is the item's `damage_type` index, full stop — `FUN_0007D67C`'s six-case jump
table is a bounds-checked subset of the same 0–18 `DT_*` space OpenApoc's extractor already
enumerates, not a separate 4-value overlay-type enum.** This also **corrects pass 2's labeling of
case 0 as a "clear" write**: it is Smoke's own encoder (`FUN_0007AD90`, mask `0`), structurally
identical to cases 1/2/3 (same map-cell loop, same blast-intensity gate, no unit-damage call) —
pass 2 documented that structural identity itself without drawing the conclusion. That resolves
pass 2's "total consumer-side symmetry between 1 and 3" finding as expected rather than curious:
0/1/2/3 are four sibling gas/fire overlays (Smoke, Alien Gas, Fire, Stun Gas) consumed by identical
code because they are the same kind of thing, not four arbitrarily-numbered special cases.

**So "type 1" is Alien Gas and "type 3" is Stun Gas.** The task's framing — "which of type 1 or
type 3 is Enzyme" — presupposed Enzyme was one of these two slots. It is neither. Both slots are
already named, by items with no thematic connection to Entropy at all.

Now the two Entropy items:

| Category (hex/dec) | Item name | `flag` (table byte 0) | Result |
|---|---|---|---|
| `0x2E` / 46 | Entropy Launcher | 1 | subindex 24 → `DAT_002B261E[24]` dword's upper word = **`-1`** (invalid). Gated out; never reaches `FUN_00058fe8` via this path. |
| `0x3A` / 58 | Entropy Pod | **2** | Fails the `flag==1` gate entirely — a *different* code path (see §6). |

**Neither Entropy item reaches this table via the standard `flag==1` chain**, and even if one did,
`DT_ENTROPY = 16` (`tools/extractors/extract_agent_equipment.cpp:54`) sits well outside
`FUN_0007D67C`'s valid `0–6` range — the same bounds check (`CMP AL,0x6 / JA`) that turns PSI-Grenade's
confirmed `6` into a no-op (above) would turn `16` into the identical no-op. Two independent
structural reasons, not one: the category gate excludes Entropy upstream, and the dispatcher's own
bounds check would exclude it even if the gate didn't. (`w20 == 16` does appear elsewhere in the
38-byte table's other, un-named entries — e.g. itemDefIndex 27, 38, 39, 40, 42 in this session's
full dump — confirming `DT_ENTROPY` is a real, present value in this catalog; it is simply never
reachable through the flag==1 chain for an item literally named "Entropy," and would be filtered out
by the dispatcher regardless.)

Entropy Pod's `flag==2` routes both `FUN_0003c1c4` and `FUN_0008f338` to an entirely separate
12-byte table (`DAT_002B2E7A`) and a different dispatch (`FUN_0004091C` / `FUN_000ABFA8` /
`FUN_000A5494`) — confirmed via the exhaustive caller list of `FUN_00021008` (§2) that none of
those three functions call `FUN_00058fe8`, so this path cannot produce a task-type-`0x13` order
regardless of what it does. Entropy Launcher's invalid `itemDefIndex` makes `FUN_0008f338` fall
through to a block that allocates task-type `0x11` orders (the K1/cloak-related order type from
pass 2's `run_k1_order_trace.sh` work) via a *different* direct index into the same `0x2B1EF4`
table (ground-item-struct offset `+0xB`, not the category-chain index) — not task-type `0x13`, and
not pursued to a terminus this session.

This is corroborated by `tools/extractors/extract_agent_equipment.cpp:280–286`: OpenApoc's own
extractor has to *synthesize* an extra `DT_ENTROPY`-derived damage-type entry
(`DamageType::EffectType::Enzyme`) beyond the real `damage_type_names->count()`, with the comment
*"extra enzyme entry for the purpose of implementing the entropy launcher."* "Entropy Enzyme" is
not an Appendix-D item at all — it is the effect concept behind the Entropy Launcher/Pod, and this
session's tracing shows structurally why the original game's own extractors couldn't find a native
slot for it in the same table the gas/smoke/fire family uses: **the Entropy weapons' detonation
code does not funnel through this table, and even the abstract damage-type value that represents
Entropy is outside the range this table's consumer treats as meaningful.**

## 6. What was ruled out, what's now bound, and what remains genuinely open

Ruled out / resolved this session:
- **Pass 2's unconfirmed joint** (§4): now proven bit-for-bit, not an inference.
- **A literal/catalog-lookup producer for `DAT_003009A0`** (pass 2's suspicion): confirmed true —
  it *is* catalog-driven, three indirection levels deep, and the field is `damage_type` itself.
- **Pass 2's "type 0 = clear" label** (§5): wrong; type 0 is Smoke's own encoder. Corrected.
- **The original "which of 1/3 is Enzyme" framing**: malformed. **Bound: type 1 = Alien Gas
  (`DT_AG`), type 3 = Stun Gas (`DT_STUNGAS`), type 0 = Smoke (`DT_SMOKE`), type 2 = Fire
  (`DT_INCENDARY`).** Entropy is none of the four — its damage type (`DT_ENTROPY = 16`) is outside
  the dispatcher's valid range, and the catalog items named "Entropy" don't reach this table's
  `flag==1` chain at all (§5).
- **Assigning Enzyme to type 1 or type 3 by symmetry or by count** (the trap the task warned
  against): not done — the resolution here doesn't pick one of the two by inference, it names all
  four slots independently and shows Entropy isn't a candidate for any of them.

Genuinely open, if resumed (none of these bear on the type-0/1/2/3 naming above, which is now
closed):
1. **Entropy Pod's actual `flag==2` dispatch** (§5): `DAT_002B2E7A[subindex*0xC]` byte routes to one
   of `FUN_0004091C`, `FUN_000ABFA8`, `FUN_000A5494` depending on its value. None of these were
   decompiled this session. If Entropy Pod produces *any* battlescape effect, it happens here — this
   is the actual "how does Entropy Enzyme work" question, now cleanly separated from the overlay-type
   table this document resolves. Worth checking whether it's direct unit damage/stat-drain (no
   ground hazard at all, consistent with "Enzyme" being a per-target instability effect rather than
   an environmental cloud) or something else.
2. **Entropy Launcher's `-1` fallback path** in `FUN_0008f338` (§5): uses ground-item-struct offset
   `+0xB` as a *direct* index into the same `0x2B1EF4` table, producing task-type `0x11` orders.
   Whether this ever indirectly reaches a battlescape effect through a different route was not
   checked.
3. The `0x59843` caller of `FUN_00058fe8` that Ghidra resolved to `function=-` (§2) was not
   identified.
4. This session named 10 of the (at most 48) entries in the `0x2B1EF4` table. The remaining entries
   were not cross-referenced against Appendix D; doing so would fully catalog which items carry
   which `damage_type`, which is useful background but not required for this row.
