# C2 follow-up · Alien-dimension secondary objectives (Queen capture, Sectoid rescue, evacuation)

Three claims raised by [C2-organic-factory-objectives.md](C2-organic-factory-objectives.md) §"the
residual gap", taken from the ten TACP alien-building briefings (file `0x2E0C09`–`0x2E1C98`).
Read that file first — it already establishes the primary destroy-objective mechanic as correctly
data-driven, and records a prior wrong hypothesis in this same area.

**Verdicts**

| Claim | Verdict |
|---|---|
| 1. Spawning Chamber — live capture of the Queen | **Generic live-alien recovery is the whole mechanic** — a real consumer exists (a research-topic gate + a score field) and the Queen participates in it, but it is identical for all 13 alien species. Nothing in the binary singles out the Queen. |
| 2. Food Chamber — Sectoid rescue ⇒ Mutant Alliance | **NOT BOUND** |
| 3. Sleeping Chamber / Dimension Gate Generator — evacuation phase | **NOT BOUND** — the anchoring string is real UI text, but it belongs to the ordinary "Abort Mission?" confirmation dialog, not a scripted post-disable timer. |

---

## 1. Claim 1 — Spawning Chamber, live capture of the Queen

**"Whilst the primary objective is the destruction of the Queen and all Alien Eggs, the live
capture of the Alien Queen would be a vicious insult to the Aliens."**

### 1.1 The mechanism, recovered from `UFO2P.EXE`

`RESEARCH_DATA` (`UFO2P.EXE`, file `0x13EE80`–`0x13F954`, 99 × 28-byte records, `research.h`) has
a `prereqType`/`prereq` item-gate pair already extracted and wired by
[`extract_research.cpp`](../../../tools/extractors/extract_research.cpp)'s `addResearchItemGate` +
`ufo2pAlienLifeformItemId` (`research.h`): `prereqType == 3` means "alien lifeform item required",
and `prereq` is `0..14` for the **live** specimen or `+15` for the **dead** specimen of one of 13
species (plus Brainsucker Pod / Overspawn).

Reading the raw bytes directly from the canonical binary (CRC32 `0x4749ffc1`, matches the task
brief) confirms the two Queenspawn records:

| Record | Name | File offset | `prereqType` | `prereq` | Resolves to | `score` | `skillHours` |
|---|---|---|---|---|---|---|---|
| 32 | Queenspawn Autopsy | `0x13F200` | 3 | `26` (`11+15`) | `AEQUIPMENTTYPE_QUEENSPAWN_DEAD` | 450 | 15000 |
| 33 | Queenspawn | `0x13F21C` | 3 | `11` | `AEQUIPMENTTYPE_QUEENSPAWN_ALIVE` | 450 | 25000 |

So `RESEARCH_QUEENSPAWN` (record 33 — the "interrogation" topic, i.e. the *extra* research beyond
the autopsy) genuinely requires the **live** Queenspawn specimen as an item prerequisite. Bringing
her back alive is the only way to unlock it; a dead Queenspawn only unlocks the autopsy topic.
This is a real, recovered, file-cited consumer of "live capture" — a **research unlock**, one of
the three consumer types the task asked about.

### 1.2 It is not Queen-specific — it is the whole-roster pattern

Dumping every `prereqType == 3` record (all 26 of them, covering the 13 species with a live/dead
pair) shows the Queenspawn rows are structurally unexceptional:

```
idx name                   prereq  live/dead species              score   hrs  u1 u2
 32 Queenspawn Autopsy       26      DEAD  QUEENSPAWN               450  15000  0  0
 33 Queenspawn                11      LIVE  QUEENSPAWN               450  25000  0  0
 20 Psimorph Autopsy          25      DEAD  PSIMORPH                 250  12000  0  0
 21 Psimorph                  10      LIVE  PSIMORPH                 250  20000  0  0
 24 Megaspawn Autopsy         24      DEAD  MEGASPAWN                200  12000  0  0
 25 Megaspawn                  9      LIVE  MEGASPAWN                200  20000  0  0
 30 Micronoid Autopsy         27      DEAD  MICRONOID_AGGREGATE      300  20000  0  0
 31 Micronoid                 12      LIVE  MICRONOID_AGGREGATE      300  26000  0  0
 ... (all 13 species follow the identical pattern)
```

Every species: same `score` for its live/dead pair, live topic always costs more `skillHours`
than the autopsy (Queenspawn: +10000; Psimorph/Megaspawn: +8000; Micronoid: +6000 — no special
premium for the Queen), `unknown1`/`unknown2` both `0` (no unusual combinator flag). The Queen's
record is not an outlier in any field. **Generic live/dead specimen gating is the entire
mechanic** — the briefing's "vicious insult" framing is flavour text on top of a mechanic every
alien species already has, not a unique reward.

### 1.3 Already correctly implemented in OpenApoc

This is not a gap. `extract_agent_types.cpp:1161-1240` builds `AEQUIPMENTTYPE_QUEENSPAWN_ALIVE`
and `_DEAD` for `UNIT_TYPE_QUEENSPAWN` (`AgentType::liveSpeciesItem` / `deadSpeciesItem`, `a->score
= data.score` — the score itself **is** a recovered per-agent constant from `AgentTypeData`).
`Battle::endBattle` (`battle.cpp:2833-2845`) awards `score.liveAlienCaptured +=
u->agent->type->liveSpeciesItem->score` for any alien recovered alive (with bio-storage present),
and the item then satisfies `addResearchItemGate`'s `agentItemsRequired`/`agentItemsConsumed` for
whichever topic needs it. Capturing the Queen alive already does everything the binary's data
supports it doing.

**One thing not to cite as original-game evidence:** `extract_agent_types.cpp:1232`
(`deadItem->score = 0`) is OpenApoc's own extractor-authored value, not a recovered binary
constant — the loop only confirmed `a->score = data.score` (line 1097) as real. Separately, the
binary's own DEBRIEFING screen (§2.1 below) has a distinct **`#Alien corpses`** score line that
OpenApoc does not currently implement as its own `BattleScore` field at all (`battle.h` has
`combatRating`, `casualtyPenalty`, `friendlyFire`, `liveAlienCaptured`, `equipmentCaptured`,
`equipmentLost` — no corpse count). That is a real, separate gap, unrelated to the Queen
specifically, and out of scope for this write-up.

### 1.4 Verdict

**BOUND as a generic mechanism; NOT BOUND as anything Queen-specific.** No score bonus, research
unlock, or organisation-relation change was found that keys on the Queen in particular. Implement
nothing new — the existing `liveSpeciesItem`/`deadSpeciesItem` + `addResearchItemGate` pipeline
already produces the correct behaviour for her, identically to every other alien species.

---

## 2. Claim 2 — Food Chamber, Sectoid rescue ⇒ Mutant Alliance

**"Whilst the rescue of any Sectoids will guarantee an Alliance with the Mutants, the primary
objective is to destroy the Alien heat and light sources..."**

### 2.1 The TACP debrief screen has no rescue/captive field

TACP's DEBRIEFING/SCORE screen strings (file `0x13231C`–`0x1323A8`) are a complete, fixed list:
`Combat rating`, `Casualty penalty`, `Leadership bonus`, `#Alien corpses`, `Live Aliens captured`,
`Equipment captured`, `Equipment lost`, `Total`, `Mission performance`, framed by `DEBRIEFING` /
`SCORE`. There is no "civilians rescued", "hostages saved", or "Sectoids recovered" line anywhere
in this list, nor anywhere else in TACP's string pool (`Civilian` at `0x2DF018` is the generic
unit-ownership label used on the unit info panel, and `hostage`/`prisoner` do not occur at all).
If rescuing Sectoids were tallied for anything, the mission scorecard — which already tracks
"Live Aliens captured" separately — is where it would show up, and it does not.

### 2.2 No "Sectoid" unit type exists in the roster; only one string mentions them

The entire alien-monster roster extracted from `UFO2P.EXE` (`UNIT_TYPE_MULTIWORM_EGG` through
`UNIT_TYPE_MICRONOID`, `extract_agent_types.cpp:61-73`) has no "Sectoid" entry, and
`CrewData` (`crew.h`, alien-building crew table `UFO2P.EXE` file `0x191BA4`–`0x191E08`) has no
captive/civilian count field alongside its 13 species counts. The string "Sectoid" occurs **once**
in the whole of `TACP.EXE`: the Food Chamber briefing prose itself (file `0x2E0EF0`). There is no
second copy anywhere that would indicate a distinct unit, message, or UI label for a captive
Sectoid.

The closest actual unit is `UNIT_TYPE_GREY` (index 15, agent-type name table `UFO2P.EXE` file
`0x153C0F`–`0x153EA0`, confirmed by reading the raw bytes: entry 15, at file `0x153CD6`, is
literally `"Alien Grey"` —
`AgentType::Role` civilian, hireable staff, the same category as `Android`). OpenApoc's own
`data/common_patch/gamestate/organisations.xml` assigns `AGENTTYPE_ALIEN_GREY` as
`ORG_CIVILIAN`'s `guard_types_alien` (the pool used to populate "civilian" spawn slots inside
alien-owned buildings), and `extract_battlescape_map.cpp:453` sets `also_allow_civilians = true`
specifically for `mapRootName == "41food"`'s enemy-designated spawn blocks. **Both of these are
prior OpenApoc code, not re-derived by this session** — they read the LOS-block map asset file for
`41food`, which is not part of the canonical EXE set in the research lab (only `TACP.EXE`/
`UFO2P.EXE`/etc. are present; the `.lof`-style per-map spawn data lives in ISO game-data files this
session had no access to). They are cited here as context, not as binary evidence for a reward.

### 2.3 The relation-write primitive, re-verified for exclusivity

[O1-O2-M1-city.md](O1-O2-M1-city.md) already walked all 17 call sites (6 functions) of
`FUN_0005faf0` (`UFO2P.EXE`, file `0xC2194` — the binary's clamped `[-100,100]` org-relation-matrix
writer, confirmed VA `0x5FAF0`) to their roots, finding none touch a battle result. That check
covered the primitive's *callers*, not whether it is the matrix's *only writer*. This session
closed that gap by scanning every `getReferencesTo` on the relation matrix `DAT_0016ec28`'s own
address range in `UFO2P.EXE` directly. Three writers exist beyond `FUN_0005faf0` itself, all now
examined:

1. **`FUN_0005faa8`** (VA `0x5FAA8`, object-page file `0x4FAA7`, no clean `.image` match) — a small
   row-shift/reset helper (initialises two matrix bytes to `-100`, then shifts a 27-entry row left
   by one slot), called from exactly two places: `FUN_0005faf0` itself, and `FUN_000b32ac`.
2. **`FUN_000b32ac`** (the O2 cargo/event dispatcher, file `0x115950`) writes the matrix directly
   at `DAT_0016ec40` — already fully examined in O1-O2-M1-city.md §2 and found unrelated to any
   tactical-battle outcome (its four event types trace back to the UFO-mission/alien-incident
   family rooted at `FUN_0003a910`, not to `Cargo::seize` or any building-raid result).
3. **`FUN_0006fd9c`** (VA `0x6FD9C`, object-page file `0x5FD9B`, no clean `.image` match) — not
   previously examined by O1. Decompiled
   this session: it cycles a candidate org index (`DAT_000f3db4`, wrapping `2..0x3D`), and where
   that org's "active" flag (org record `+0x11`) is still unset, it initialises the *entire* new
   org's relation row against every other active org (`100`/`-100`/`0x5a` depending on the other
   org's own active flag) and then drives a modal "new organisation" announcement dialog
   (`FUN_00063a00` string-display calls inside a blocking input loop). Its **sole caller** is
   `FUN_0004ab04` — the exact same root O1 already identified as `FUN_00092470`'s (the scripted
   diplomatic-incident dispatcher) only caller. This is a sibling incident type in the same
   periodic scripted-city-event system O1 already traced, not a battle-triggered write: it
   initialises a *newly founded* organisation's starting relations, it does not adjust an
   *existing* one in response to anything.

No writer found this session or in O1 reads a tactical-battle-result signal. `FUN_0005faf0`'s
entire reachable write surface — the primitive itself, its row-shift helper, the O2 cargo
dispatcher, and the new-organisation-founding event — terminates in scripted UFO/alien-incident
AI, infiltration detection, cargo/population transfer, and periodic city announcements. None of
it is reachable from a TACP battle outcome.

### 2.4 Mutant Alliance is real, but relation with it is not battle-triggered here

`ORG_MUTANT_ALLIANCE` is a real organisation (org index 18, `UFO2P.EXE` file `0x14AFFF` —
`"Mutant Alliance"`), and `wongFaq.txt` documents that an angry Mutant Alliance blocks recruiting
"Hybrids" — i.e. relation *tier* with this org gates something, through the ordinary
diplomacy/relation system already covered by O1. Nothing found this session (or in O1) connects
that relation value to a specific battle event; it moves only through the scripted-incident /
infiltration-detection paths O1 already traced.

### 2.5 Verdict

**NOT BOUND.** Searched and ruled out: (a) the TACP debrief scorecard, which has no
rescue/civilian field despite tracking "Live Aliens captured" separately; (b) the TACP string pool,
which contains "Sectoid" exactly once, in the briefing prose, with no second copy as a UI/game
message; (c) the alien-building `CrewData` table, which has no captive-civilian count field; (d) an
independent re-scan of the relation matrix's own address range in `UFO2P.EXE`, finding all four
writers into it (`FUN_0005faf0`, its helper `FUN_0005faa8`, the O2 dispatcher `FUN_000b32ac`, and
the new-organisation event `FUN_0006fd9c`) and confirming none of them carries a signal from a
tactical-battle result. Per the prime directive, do not invent a Sectoid-rescue-to-relation
effect; the "guaranteed Alliance" line is unimplemented narrative promise, same category as the
still-open O1 bribe/rift gap.

---

## 3. Claim 3 — evacuation phase after disabling a building

**Sleeping Chamber:** "After disabling the building, all Agents must exit ... as soon as
possible." **Dimension Gate Generator:** "Upon disabling the building it is imperative that all
Agents evacuate as a matter of urgency. Our forces must return to the Earth dimension before the
final Dimension Gate closes forever."

### 3.1 No countdown/timer text exists anywhere in TACP

Searched the full TACP string pool for turn-countdown, self-destruct, or time-limit language
(`turns remain`, `turns left`, `self destruct`, `countdown`, `time limit`, `closes`): the **only**
hit is the Dimension Gate Generator briefing's own "before the final Dimension Gate closes
forever" — pure narrative, no numeric countdown ever surfaces to the player. A scripted forced-exit
timer would need *some* UI text to announce it; none exists.

### 3.2 The positive-control anchor, walked to its actual reader

[METHOD-tacp-string-resolver.md](METHOD-tacp-string-resolver.md) records `"The following units
will be lost if left in combat zone:"` (file `0x2E0361`) as a confirmed member of the live
pointer table at object2 `0x292D18`–`0x292DEC` (file `0x2E27BC`–`0x2E2890`) — exactly the kind of
"per-agent penalty for units still in the map" phrasing Claim 3 is asking about. This session
walked it to ground:

1. **Table slot.** The string is table entry index 37 (`(0x292dac − 0x292d18) / 4`), address
   `0x292dac`, file `0x2E2850` (delta `+0x4FAA4`, cross-checked against the table's own established
   file citation — the string body itself lands at file `0x2E0361`, matching the METHOD doc's
   citation exactly).
2. **The resolver.** `FUN_0005b770` (VA `0x5B770`, object-page file `0x4B76F`, no `.image` match)
   is the *only* function in the whole binary referencing the table's base address. Decompiled:
   `char * __regparm3 FUN_0005b770(uint index, ushort skip)` — indexes the table by `index`, then
   walks forward `skip` further NUL-terminated strings in the packed pool from that base. It has
   **~309 call sites** across nearly the entire binary (a generic "fetch message N, optionally
   offset by M" utility, like the ~49-caller `FUN_000b523c` this project already learned not to
   trust as distinguishing — see O1-O2-M1-city.md §3.3).
3. **Isolating candidate calls, with a documented false positive.** Scanning all 309 call sites
   for one where the resolver's index register is loaded with the literal `37` first flagged a
   call inside `FUN_000b8ebc` (VA `0xb93da`, file `0x113960` — a ~150-call-site generic UI-message
   dispatcher, itself not distinguishing, in the same "huge caller count" trap as
   `FUN_000b523c` in O1-O2-M1-city.md §3.3). Replicating `FUN_0005b770`'s own skip-forward
   algorithm on the live loaded bytes showed that call's actual `skip` operand (`37`, in a
   *different* register than the index) walks the pool to `"Start Turn"`, not our target — a
   confirmed false match, discarded. A second, stricter pass (only accepting a `MOV EAX,imm`
   immediately feeding the call, since the decompiled signature is
   `char * __regparm3 FUN_0005b770(uint index, ushort skip)` and Ghidra resolves `index` to EAX)
   found exactly one such site: `TACP.EXE` file `0x90354` (`FUN_000358b0`, VA `0x358b0`), at
   instruction VA `0x35c5b` (`MOV EAX,0x25`) immediately before `CALL 0x5b770` at VA `0x35c6c`.
   `FUN_000358b0` has only two callers in the whole binary (unresolved addresses
   `0xa9050`/`0xbc198`), consistent with a discrete UI action rather than a polled system — a
   plausible candidate. **Not fully confirmed as the display fetch**, though: the same call site
   also sets `EDX,0x17c` (380), and if `EDX` is genuinely the resolver's `skip` parameter here (the
   register-to-parameter mapping was not independently re-derived for this specific call, only
   assumed from the decompiled signature), skipping 380 strings forward from entry 37 lands on
   empty bytes past the end of the pool, not a live string — so this call site is reported as a
   candidate, not as the proven trigger.
4. **What the string is actually attached to — the decisive evidence.** Independent of which
   function calls it, reading forward in the packed pool from the string's own body (file
   `0x2E0361`) by NUL-terminator boundaries — on the live loaded bytes of the canonical,
   CRC-matched binary — gives: entry 0 `"The following units will be lost if left in combat
   zone:"`, entry 1 `"Abort mission?"`, entry 2 `"Yes"`, entry 3 `"No"`.

This last point is conclusive regardless of the unresolved call-site question in step 3: the
warning string is packed immediately next to an **"Abort mission? Yes/No"** confirmation dialog,
not a countdown ticker. It is the standard series-wide "some of your units are still in the field,
are you sure you want to leave" prompt shown when the player *chooses* to abort a mission — not a
scripted timer that starts itself once a building is disabled.

### 3.3 Matches existing prior art

OpenApoc already implements the generic version of this: `Battle::checkMissionEnd` /
`winnerHasRetreated` (`battle.h:145`, `battle.cpp:2828`) reduce loot to agent-only recovery when
the player retreats with the map not secured, and units left behind that are not evacuated are
handled through the ordinary `retreat`/`retreated` machinery (`battle.cpp:2117-2119`, `2748-2759`).
This is precisely what the string's own neighbouring dialog describes: leaving while units remain
loses them, as a consequence of *voluntarily* aborting — not a scripted timer unique to alien
buildings.

### 3.4 What was not established

The exact code path from `checkIfBuildingDisabled`/`tryDisableBuilding`-equivalent objective
completion in TACP to *anything* was not traced this session (no TACP-side analogue of that check
was located or searched for beyond the string-pool sweep in §3.1). The claim being tested is
narrower — "is there a scripted evacuation timer/forced-exit distinct from ordinary retreat" — and
the evidence above answers that specific question. A broader confirmation that TACP's own
building-disable logic exists and does nothing extra was not attempted and would need a dedicated
pass (comparable in scope to the B1 cover-metric search) if ever wanted.

### 3.5 Verdict

**NOT BOUND** as a scripted post-disable evacuation timer or forced-exit penalty. The anchoring
string is real, live UI text (unlike most pool strings, its consumer chain — table slot → resolver
→ specific call site — was fully traced to file offsets), but it belongs to the ordinary
"Abort Mission?" retreat-confirmation dialog used throughout the game, which OpenApoc already
models via `winnerHasRetreated` and the retreat/loot-reduction path. The Sleeping Chamber and
Dimension Gate Generator briefings' "evacuate urgently" language is dramatic framing on top of the
same generic abort-mission mechanic every mission has, not a distinct alien-dimension-only phase.

---

## 4. Summary

| Claim | Verdict | Strongest evidence | What remains open |
|---|---|---|---|
| 1. Queen live capture | Generic mechanism only — no Queen-specific consumer | `RESEARCH_QUEENSPAWN` (record 33, file `0x13F21C`, prereq=11=alive) vs `_AUTOPSY` (record 32, file `0x13F200`, prereq=26=dead); identical pattern across all 13 species (§1.2) | Nothing — already correctly implemented via existing `liveSpeciesItem`/`addResearchItemGate` pipeline |
| 2. Sectoid rescue ⇒ Alliance | NOT BOUND | TACP debrief scorecard has no rescue field (file `0x13231C`–`0x1323A8`); relation-matrix writer re-scanned and all four non-primitive writers examined (`FUN_0005faf0` file `0xC2194`, helper `FUN_0005faa8` object-page file `0x4FAA7`, `FUN_000b32ac` file `0x115950`, new-org event `FUN_0006fd9c` object-page file `0x5FD9B`) — none reachable from a tactical-battle result | Whether the original `41food` map data (not in this lab's canonical set) genuinely spawns `AGENTTYPE_ALIEN_GREY` as civilians — OpenApoc's own prior-art code, not re-derived here |
| 3. Post-disable evacuation | NOT BOUND | The evac-warning string's own packed-pool neighbours (file `0x2E0361`+) are literally `"Abort mission?"` / `"Yes"` / `"No"` — decisive regardless of which function calls it; a plausible but not fully confirmed call site was also found (`FUN_000358b0`, file `0x90354`) | Whether TACP has its own `tryDisableBuilding`-equivalent at all (not traced; out of scope for this specific claim); which register truly carries the resolver's `skip` parameter at the `0x90354` call site |

**Do not implement:** a Queen-specific reward beyond the existing generic mechanism, a Sectoid
rescue → relation effect, or an alien-dimension evacuation timer/forced-exit penalty. All three
would be invented numbers or invented triggers with no binary consumer, exactly what the prime
directive forbids.
