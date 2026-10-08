# v_tactical_difficulty: tactical difficulty scaling and base-defence / raid battle rules vs the original executables

Key: `v_tactical_difficulty`. Binaries: `TACP.EXE` and `UFO2P.EXE`, ISO "non-4" generation (CRC `0xfebbe39e` / `0x4749ffc1`). Date of work 2026-10-07. Nothing was launched; Ghidra ran read-only on private copies; the two byte checks that needed original game data were done by reading the user's `cd.iso` with `bsdtar` into the scratchpad (maps `37SEC23/24.SMP`, `BUILDING.DAT`), never the EXEs.

Tags: **OBSERVED** = decompile/listing text, table bytes, or LIVE code lines I read. **INFERRED** = my reading of observed facts. Citations are binary + generation + PHYSICAL file offset; "VA" is the Ghidra address I queried, converted with TACP code = VA + 0x5AAA4, TACP data = VA + 0x4FAA4, UFO2P code = VA + 0x626A4, UFO2P data = VA + 0x616A4 (exe_evidence_index section 3). Every function entry cited was prologue-checked against the file (section 7 table).

## 0. Bottom line

1. **The EXE does scale hostile units by difficulty, and the fork does not.** One formula, `value = (base + rand(0..inc)) * (100 + k*D) / 100` with integer division, is applied at unit creation in TACP (`FUN_0001f830`, file `0x7A2D4`) and in UFO2P (`FUN_000ad93c`, file `0x10FFE0`). `D` = difficulty 0..4 (Superhuman = 4). Multipliers: TU, HP, stamina, reactions `k=+4` (+16% at D4); psi energy `+2` (cap 100); psi attack `+1` (cap 100); psi defence `(base+2D)*(100+D)/100` (cap 100); natural armour `+16` (+64% at D4) on all zones; and the raw accuracy byte `-4` (the byte is scaled by `(100-4D)%`; under the extractor's and the stat routine's `100 - byte` convention this **raises shown accuracy at higher difficulty**, section 2.2). This confirms the numbers in `web_superhuman_speedrun.md` row 4 but reverses the sign reading of the accuracy term. Fork: no `difficulty` reference anywhere in `game/state/battle`, `game/state/rules/battle`, `game/state/shared/agent*.cpp` (grep of LIVE). Only X-COM agents (copied from the persistent roster by `FUN_000acc48`) are not scaled.
2. **Base turrets exist in the EXE and are spawned by TACP, 4 per Security / Advanced Security sector, from map feature bytes `0xE6..0xED`.** The fork's turret count and positions are right. Almost everything else about the fork's turrets differs: the EXE unit record is the same record the fork calls `AGENTTYPE_CORPORATE_BOSS` (500 HP, armour 35 on every zone, scaled by TACP to 580 HP / armour 57 at Superhuman; the fork's turrets have armour 0), and the weapons are `Megapol Plasma Gun` + 2 `Megapol Plasma Pod` (Security Station) and `Disruptor Gun` (Advanced), not the fork's invented 26-damage laser and 70-damage disruptor beam.
3. **The base-defence roster rule is the biggest base-defence divergence.** UFO2P reserves `T = min(4 x completed security stations, 12)` unit slots for turrets, caps X-COM personnel at `36 - T`, and when more people are present **cuts all four personnel classes (agents, biochemists, mechanics, physicists) proportionally**, so staff displace soldiers. The fork takes up to 36 soldiers first, then staff, and adds turrets on top. The "only 3 security stations active" guide claim is NOT found as a rule; what is bound is the 12-slot reservation.
4. **Reinforcements: the EXE spawns exactly one unit per timer expiry; the fork spawns four.** They exist only in the ten alien-dimension buildings in both (OBSERVED for the EXE side: the 2 x 2 ground-id pad 208/209 over 210/211 that `FUN_00024528` requires occurs in the original sector maps `39INCUB`..`48GATE` only, checked over all 477 `.SMP` files on the ISO; the spawner is also gated on mission-type fields 1/1). EXE species pool and pad rule also differ (section 5).
5. **Stun/unconscious rule matches the fork.** Capture score and storage differ in detail: the EXE's TACP score does not test for any carrier; the fork zeroes `liveAlienCaptured` without storage (section 7).

Confidence on the identity of the TACP difficulty global is stated in section 2.1: it is INFERRED (usage parity), not proven by locating its UFO2P writer.

---

## 1. Method and controls

| Step | What | Result |
|---|---|---|
| Table locate | Byte-searched UFO2P `agent_type_data` (file `0x155574`, 47 x 106 B) inside TACP | Found at TACP file `0x2FFAF8` (VA `0x2B0054`); **the full 47 x 106 byte tables are byte-identical in the two binaries** (control: same data, so the two code paths act on the same records) |
| Readers | `getReferencesTo` over the TACP table range (script `Vtd.java`, `bytes:` command) | readers `FUN_0001f830` (48 refs), `FUN_00024018`, `FUN_00024e5c`, `FUN_00023678`, `FUN_0002370c`, `FUN_0001f0f4` |
| Difficulty global | `FUN_0001f830` reads dword `[0x1ED340]` 10 times; `FUN_00024e5c` and `FUN_000250f8` once each | raw-byte control: scanning TACP code for the relocated operand `40 d3 10 00` finds exactly **12** operands (VAs `0x1F963 ... 0x2511B`), matching the 12 Ghidra xrefs, so there is no unseen reader |
| UFO2P parity | UFO2P difficulty word VA `0xE0C9C` (file `0x142340`) read as dword `[0xE0C9A] >> 16`; `FUN_000ad93c` reads it 11 times with the **same multipliers** | see 2.1 |
| Decompile caveat | Ghidra constant-folded `[0x1ED340]`/`[0xE0C9A]` (initialised read-only data, value 0) so the C shows `* 100 / 100`. **All formulas below are from the instruction listings, not the C.** This is also why `FUN_00022f90` and `FUN_000ac7d0` needed listing reads (section 7, 5) |
| RNG | UFO2P `FUN_0005d1d8(n)` and TACP `FUN_0001eee8(n)` both return `x % (n+1)`, i.e. inclusive `[0, n]` | listings read |

Generation caveat: everything here is the ISO non-4 build; the Steam `4` builds (`TACP4.EXE`, `UFO2P4.EXE`) were not examined and no equivalence is claimed.

Reproduce: `ghq.sh <copy> TACP|UFO2P Vtd.java full:<hexva> | dec:<hexva> | ref:<hexva>:<len> | bytes:<hex>:<len> | raw:<hexva>:<n>` (script saved at `scratchpad/validate/work_vtd/Vtd.java`; raw query logs are `scratchpad/validate/vtd_*.txt`; the temporary ISO extraction of `37SEC23/24/22/25.SMP` and the `.SMP` scan was deleted after use). About 35 short headless calls (8 to 90 s each) were used across the two copies.

---

## 2. Finding group A: difficulty scaling

### 2.1 Where TACP reads difficulty, and how sure we are that it is difficulty

**OBSERVED.** TACP global dword `[0x1ED340]` (VA; inside the 212-byte block the scenario loader fills at `0x1ED294`; TACP has no writer, it is loaded) is read by exactly three functions:

- `FUN_0001f830` (TACP file `0x7A2D4`): unit creation. Ten reads, each `MOV EAX,[0x001ed340]` then a multiplier, see 2.2.
- `FUN_00024e5c` (file `0x7F900`): human-NPC loadout, `MOV AX,[0x1ed340]; ADD EAX,0xe; CALL rand` (listing at VA `0x24E69`).
- `FUN_000250f8` (file `0x7FB9C`): alien loadout tier, `MOV EBP,[0x1ed340]; LEA ECX,[EBP*8]; SUB ECX,EBP; SHL ECX,2; ... CMP EAX,[ECX+0x2b305c]` (VA `0x25119`), i.e. row `D` of a 5 x 7 dword table.

**INFERRED, high confidence: `[0x1ED340]` is the difficulty (0 = Novice ... 4 = Superhuman).** Evidence:

1. The five-row table at TACP file `0x302B00` (VA `0x2B305C`) is byte-identical to UFO2P's table at file `0x19BEB4` (VA `0x13A810`, read by `FUN_000ad38c` as `[0xE0C9A]>>16 * 0x1C + 0x13A810 + i*4`). Row 4 = `1000, 2200, 4000, 6600, 10000, 18000, 28000`, exactly the Superhuman alien-tech thresholds in `web_superhuman_speedrun.md` row 5; row 0 = `1000, 2200, 4000, 7000, 12000, 22000, 42000` (Novice). A global that selects row `D` of this table is the difficulty.
2. `FUN_0001f830` (TACP) and `FUN_000ad93c` (UFO2P) apply the same multipliers to the same agent record fields, and UFO2P's version reads the word that `FUN_0006fcc0` (hourly infiltration) uses as `42 - D`.
3. UFOpaedia's published per-level increments (0.04 / 0.02 / 0.01 / -0.04 / 0.16) equal the multipliers decoded here to the digit.

**Not done:** the UFO2P writer that stores difficulty into the tactical header was not located (the header's block layout in TACP does not match the UFO2P `savegame.00` writer order I parsed; see section 10). Without it, "TACP receives difficulty via the mission file" is the assumption that fits, not a traced fact.

### 2.2 The formula (OBSERVED, listing of `FUN_0001f830`, TACP file `0x7A2D4`; same shape in UFO2P `FUN_000ad93c`, file `0x10FFE0`)

For each stat: `rec = agent_type_data[type]`, `b = rec base byte`, `i = rec inc byte`, `r = rand(i)` (inclusive), `D = difficulty`:

| Stat (rec offset) | TACP stores to unit | Formula | Cap | Listing evidence (TACP VA) |
|---|---|---|---|---|
| Speed / TU (`+2`,`+3`) | `+0x58,+0x59,+0x5A` bytes | `(b+r) * (100 + 4D) / 100` | none (byte) | `0x1FBE4`: `MOV EAX,[ed340]; SHL EAX,2; ADD EAX,0x64; IMUL` |
| Health (word `+4`, word `+6`) | `+0x62,+0x64` words | `(b+r) * (100 + 4D) / 100` | none | `0x1FA8D`: `[ed340]; SHL 2; ADD EBP(=100)` |
| Stamina (`+0xA`,`+0xB`) | `+0x68,+0x69` | `(b+r) * (100 + 4D) / 100` | none | `0x1FB58` |
| Reactions (`+0xC`,`+0xD`) | `+0x6C,+0x6D` | `(b+r) * (100 + 4D) / 100` | none | `0x1FA43` |
| Psi energy (`+0x12`,`+0x13`) | `+0x74,+0x75` | `(b+r) * (100 + 2D) / 100` | **100** | `0x1F962`: `ADD EAX,EAX; ADD EAX,0x64`; `0x1F981 CMP EAX,EBP; JLE; MOV ECX,EBP` |
| Psi attack (`+0x14`,`+0x15`) | `+0x88` | `(b+r) * (100 + D) / 100` | **100** | `0x1F9A9` |
| Psi defence (`+0x16`,`+0x17`) | `+0x89,+0x8A` | `((b+r) + 2D) * (100 + D) / 100` | **100** | `0x1F9EE..0x1FA02` (`ADD ECX,ECX` then `ADD`, then `IMUL` by `100+D`) |
| **Accuracy byte (`+0x1A`,`+0x1B`)** | `+0x8E` | `(b+r) * (100 - 4D) / 100` | none | `0x1FAC7`: `SHL EAX,2; MOV EDX,EBP; SUB EDX,EAX; IMUL` |
| Bravery (`+0x10`,`+0x11`), Strength (`+0xE`,`+0xF`) | `+0x73`, `+0x70/+0x71` | `b + r` | none | plain `ADD` after `rand` (`0x1FB1E`) |
| Natural armour (6 words `+0x46..+0x50`) | `+0x46..+0x50` | `a * (100 + 16D) / 100` **only if `rec[+0x45] != 0`**, else 12 bytes `0xFF` | none | loop `0x1FD37`: `SHL EAX,4; ADD EAX,0x64; IMUL ECX,EAX; ... IDIV`; branch at `0x1FD12 CMP byte [rec+0x45],0` |

At D = 4: TU/HP/stamina/reactions x1.16; psi energy x1.08; psi attack x1.04; psi def `(x+8)*1.04`; accuracy byte x0.84; armour x1.64. Division is `IDIV` (truncation).

**Accuracy direction (blocking point; resolved).** The extractor stores the fork's accuracy stat as `100 - byte` (`tools/extractors/extract_agent_types.cpp:1007-1008`, LIVE and HEAD identical), and an original TACP routine does the same: at VA `0x55574` (file `0xB0018`, inside `FUN_00055400`, file `0xAFEA4`) the code is `MOV AL,[EDX+0x8e]; MOV EDX,0x64; SUB EDX,EAX` (stores `100 - current accuracy byte` to `[0x2D144C]/[0x2D1444]`) and, a few instructions later, `MOV EAX,[EAX+0x492]; SAR EAX,0x10` then `100 - that` (the word at unit `+0x494`). `FUN_0001f830` writes unit `+0x494` as a copy of the creation-time `+0x8E` byte (decompile `*(ushort*)(piVar13+0x125) = byte[+0x8e]`), so the routine computes `100 - current` and `100 - initial` accuracy, which is what a current-versus-original stats display does (INFERRED label: I did not decode who calls it). The Wong FAQ's "Accuracy 65+20" for recruits is the raw byte (x-com agent rec 0 = 65, inc 20), and the fork's Android patch uses 30..50 = `100 - (50+20 .. 50)`. So, under the fork's own convention, which that routine matches, the byte is an inaccuracy value. Scaling it by `(100 - 4D)%` therefore **raises** shown accuracy (under that convention): Anthropod shown accuracy 0 -> 16, Skeletoid 30 -> 42, Spitter 38 -> 48, Megaspawn 50 -> 58, Psimorph 30 -> 42, human guards 20..40 -> 33..50 at D4. UFOpaedia's "accuracy -4% per level" (relayed in `web_superhuman_speedrun.md` row 4) is the multiplier, mis-read as a reduction of the displayed accuracy. INFERRED: effective hit chance rises with difficulty; the shot-resolution function itself was not decoded. The fix sketch (scale the stored byte, i.e. `acc = 100 - floor((100-acc)*(100-4D)/100)`) is correct under the fork's convention whichever way the real hit chance turns out.

**Psi energy cap (new).** Both creators clamp psi energy at 100, so the Psimorph's table value 170 becomes 100 at creation at every difficulty (the Wong table value 170 is the raw table). The fork keeps 170. Low impact.

**Species table at Superhuman (all computed from the EXE record, truncating division; `inc` is 0 for all alien species so values are deterministic):**

| unit (agent_type_data rec) | TU 0->4 | HP 0->4 | Reactions 0->4 | Stamina 0->4 | Accuracy shown (fork / EXE@D4) | Armour head,body,rarm,larm,leg 0 -> 4 |
|---|---|---|---|---|---|---|
| Anthropod (39) | 56 -> 64 | 60 -> 69 | 22 -> 25 | 60 -> 69 | 0 / 16 | 5/6/6/6/5 -> 8/9/9/9/8 |
| Skeletoid (40) | 86 -> 99 | 34 -> 39 | 46 -> 53 | 130 -> 150 | 30 / 42 | 14/16/14/14/15 -> 22/26/22/22/24 |
| Spitter (41) | 55 -> 63 | 65 -> 75 | 35 -> 40 | 65 -> 75 | 38 / 48 | 7/7/7/7/6 -> 11/11/11/11/9 |
| Popper (42) | 110 -> 127 | 35 -> 40 | 8 -> 9 | 110 -> 127 | 100 / 100 | 7/7/7/7/7 -> 11/11/11/11/11 |
| Brainsucker (35) | 90 -> 104 | 20 -> 23 | 5 -> 5 | 140 -> 162 | 100 / 100 | 5/5/5/5/5 -> 8/8/8/8/8 |
| Multiworm (36) | 45 -> 52 | 340 -> 394 | 25 -> 29 | 70 -> 81 | 50 / 58 | 3/8/8/8/7 -> 4/13/13/13/11 |
| Hyperworm (37) | 82 -> 95 | 44 -> 51 | 30 -> 34 | 50 -> 58 | 30 / 42 | 7/7/7/7/7 -> 11/11/11/11/11 |
| Chrysalis (38) | 30 -> 34 | 86 -> 99 | 5 -> 5 | 80 -> 92 | 100 / 100 | 12 x5 -> 19 x5 |
| Multiworm Egg (34) | 30 -> 34 | 80 -> 92 | 24 -> 27 | 80 -> 92 | 60 / 67 | 5 x5 -> 8 x5 |
| Megaspawn (43) | 50 -> 58 | 260 -> 301 | 15 -> 17 | 55 -> 63 | 50 / 58 | 28/37/30/30/34 -> 45/60/49/49/55 |
| Psimorph (44) | 36 -> 41 | 420 -> 487 | 30 -> 34 | 90 -> 104 | 30 / 42 | 10 x5 -> 16 x5 |
| Queenspawn (45) | 45 -> 52 | 600 -> 696 | 20 -> 23 | 80 -> 92 | 60 / 67 | 30 x5 -> 49 x5 |
| Micronoid (46) | 30 -> 34 | 80 -> 92 | 30 -> 34 | 80 -> 92 | 60 / 67 | no natural armour (flag byte 0) |
| rec 5, the base turret (5) | 60 -> 69 | 500 -> 580 | 44 -> 51 | 80 -> 92 | 60 / 67 | 35 x5 -> 57 x5 |

Human org guards have `inc` ranges and natural armour too: Building security TU 60..75 -> 69..87, HP 40..50 -> 46..58, armour 20/20/16/16/20 -> 32/32/26/26/32; Cultist armour 16/16/12/12/18 -> 26/26/19/19/29; Police 18/25/20/20/18 -> 29/41/32/32/29.

### 2.3 Which units are scaled (OBSERVED unless noted)

- **UFO2P roster builder** `FUN_000ac348` (file `0x10E9EC`; mission type `param_2`) builds the battle unit list in a 60-slot table (`DAT_002440d4 < 0x3c`, hostile side counter `DAT_00244528 < 0x24` = 36). Aliens, civilians, org guards, UFO crew are created through `FUN_000ad93c` (callers `FUN_000adffc`, `FUN_000ae218`, `FUN_000aebd8`, `FUN_000aec70`, `FUN_000aedb0`, `FUN_000ae628`): scaled.
- **X-COM agents are copied, not created**: `FUN_000acc48` (file `0x10F2EC`, only caller `FUN_000acbb4`) copies the persistent agent's stored stats byte for byte into the roster slot. Not scaled. The hire path (`FUN_000791e0`, the city-level person generator) does not read difficulty.
- **TACP dynamic spawns** use `FUN_0001f830`: base turrets (`FUN_00024110`), reinforcements (`FUN_000243c0`), brood-style spawns (`FUN_0006bfe0` with type 2 = Brainsucker; `FUN_0006e180` with type 4 = Hyperworm; roles INFERRED from the type ids only), skirmish script (`FUN_000ab758`). All scaled; there is no exemption in the TACP function.
- **UFO2P exemption for image id 0x15 (the turret record) is moot.** `FUN_000ad93c` multiplies TU, stamina, reactions, accuracy, health and armour by 100 instead of the scaled factor when `rec[+0x68] == 0x15` (listing `0xADAB2 CMP SI,0x15`, `0xADB10 CMP DI,0x15`, `0xADB6A`, `0xADCDB`, `0xADDAE`, `0xADE2A`), but turrets are created by TACP `FUN_00024110`, which has no such test. So at D4 turrets are scaled. (UFOpaedia's "Security Station turrets get higher stats" agrees.)

### 2.4 Engine behaviour (LIVE; HEAD identical)

`AgentGenerator::createAgent` rolls `s.health = randBoundsInclusive(rng, min.health, max.health)` etc. and stores `initial_stats/current_stats/modified_stats = s` (`game/state/shared/agent.cpp:121-149`); no difficulty term. grep `difficulty` over `game/state/battle`, `game/state/rules/battle`, `game/state/shared/agent*.cpp`, `game/state/rules/aequipmenttype.cpp` returns nothing; difficulty is used only in `gamestate.cpp` (relations, infiltration divisor, bribe odds), `building.cpp:155,797` (detection), `organisation.cpp:377,648,1154-1178`, `vehicle.cpp:3940`. Natural armour: `agent->type->armor.at(bodyPart)` unscaled (`game/state/battle/battleunit.cpp:1824`), subtracted at `battleunit.cpp:1912-1913` (`damage = dealDamage(...) - armorValue`).

### 2.5 Impact, worked example (INFERRED from the observed numbers)

A Popper at D4 has HP 40 / armour 11 in the EXE (fork 35 / 7). A Disruptor Gun hit of mean 42 damage does `42 - 7 = 35` in the fork (exactly one-shot) and `42 - 11 = 31` in the EXE (no longer one-shot); the fork's "outright energy kill avoids the brood" doctrine in `web_tactical.md` section 2 point 3 would be unreliable at Superhuman. A Skeletoid (fork armour 14..16, 34 HP) has 22..26 armour and 39 HP: small-arms damage that the fork lets through is absorbed. Conversely the fork's aliens are less accurate than the EXE's (Anthropod 0 vs 16 shown accuracy).

### 2.6 Alien equipment tier by difficulty (thresholds match; handicap unbound)

- OBSERVED: thresholds per difficulty are identical in TACP (`FUN_000250f8`) and UFO2P (`FUN_000ad38c`, file `0x10FA30`) and equal the fork's extracted difficulty4 sets (min_score 1000..28000 at Superhuman; per `web_superhuman_speedrun.md` row 5). The compared score in UFO2P is `[0xE0C5C] + [0xE0C60]` = category 0 "Tactical Missions" this-week + previous (score array = 8 records x {week, previous}, exe_evidence_index 5.2). The fork uses `state.totalScore.tacticalMissions` (`game/state/shared/agent.cpp:165-166`): **match**.
- OBSERVED, not in fork: UFO2P `FUN_000ad38c` then subtracts a handicap from the tier: `sVar3 = FUN_0006e608(1,5)` (count of occupied entries in row 1 of a 2 x 40 x 14-byte table at VA `0x143A7E`), `sVar4 = FUN_0006ea78(1)` (count of those with a field equal to a register value), and `if (sVar4 < sVar3/10) tier -= (sVar3/10 - sVar4)` (listing `0xAD406-0xAD430`). What that table is was not identified. **exe_unbound.**

### 2.7 Human NPC gear bucket is difficulty-biased (diverges; medium confidence)

- OBSERVED UFO2P `FUN_000ae1d4` (file `0x110878`): `bucket = FUN_000638dc(16, 4)` mapped by thresholds `<5 ->0, <9 ->1, <12 ->2, <14 ->3, else 4`. `FUN_000638dc(n=16,k=4)` (file `0xC5F80`) = sum of 4 `rand(0..16)`, `/2`, minus `((4 - D) * 16)/4`, rejection-sampled into `[0,16]`; it reads `[0xE0C9A]>>16`. Exact bucket distribution (computed by enumeration): D0 `66.7/26.2/5.9/1.0/0.2 %`, D4 `1.5/11.3/23.0/23.0/41.2 %`.
- OBSERVED TACP `FUN_00024e5c`: `bucket = f(rand(D+14))` with the same thresholds: D0 `33/27/20/13/7 %`, D4 `26/21/16/11/26 %`.
- The bucket indexes `rec[+0x53 .. +0x57]` (five equipment-set ids per unit type, `0xFF` = none). OBSERVED in the table: only the nine human org types have distinct ids (Gang leader 13,14,15,16,17; Cult Leader 14,14,14,14,15; Politician 13 x5; Chief of Police 14,14,15,15,15; Corporate hood 14,14,15,16,17; Police 13,14,14,15,15; Gangster and Cultist 14..18; Building security 13,13,14,15,17) plus Anthropod `8..12` and Skeletoid `8,8,9,10,11`; every other record has five identical ids or `0xFF`. So the bias does something only for those types.
- Fork: human guard equipment = `EquipmentSet::getForType(Human, org->tech_level)` with `tech_level` +1 per week (`game/state/shared/agent.cpp:169-173`, `gamestate.cpp:1754`). No difficulty term, no per-type 5-slot bucket. **INFERRED divergence**; I did not find an EXE consumer of an organisation tech level for tactical gear. Which of the two (bucket vs alien tier) Anthropod/Skeletoid use was not resolved.

---

## 3. Finding group B: base-defence turrets

### 3.1 Spawning, count and placement: match

- OBSERVED TACP `FUN_00010294` (main), file `0x6B268` (VA `0x107C4`): `CMP [0x1ED2C8],0; JZ; CMP [0x1ED2CC],2; JNZ; CALL FUN_00024110`. `FUN_00024110` (file `0x7EBB4`) scans every tile; a tile whose 4th byte (the feature byte) is in `0xE6..0xED` (230..237) creates a unit with `FUN_0001f830(EAX=0x15, EDX=0 [X-COM side], EBX=1)` and clears the byte. `[0x1ED2CC] == 2` is the mission-type field for this path (INFERRED: base defence; the reinforcement spawner uses `[0x1ED2CC] == 1`, section 5); `[0x1ED2C8]` is a header flag, both in the 212-byte block (meaning of the flag not decoded).
- OBSERVED original data (extracted from `cd.iso`): `MAPS/37BASE/37SEC23.SMP` has exactly four cells with feature byte `232 (0xE8)` at (7,4,3), (8,4,3), (7,11,3), (8,11,3); `37SEC24.SMP` has four cells with `234 (0xEA)` at (4,4,3), (11,4,3), (4,11,3), (11,11,3); sector 22 and 25 have none. These are the fork's `guardianLocations` coordinates (`tools/extractors/extract_battlescape_map.cpp:593-611`, `battlemap.cpp:1214-1222`). **Count and positions match.** Other maps use 230..237 as ordinary scenery features, so the function must be (and is) gated by the mission type.
- OBSERVED: sector 23 = facility type `0x17`, sector 24 = `0x18` (UFO2P `FUN_000b42ec` counts exactly those two types, section 4).

### 3.2 The turret unit record (diverges)

- OBSERVED: `FUN_0001f830` finds the record by `rec[+0x68] == param_1`; `0x15 = 21` selects **record 5** (stationary: `movement_type 0`, 500 HP, TU 60, reactions 44, strength 10, psi def 100, inaccuracy byte 40, armour 35 on all six words, flag byte `+0x45 = 2`, `score 0`). The same function zeroes the move-cost field `[unit+0xFE]` for image ids `1, 5, 0xC, 0x15` (egg, chrysalis, queenspawn, turret: the stationary units).
- OBSERVED in the fork: the extractor labels record 5 `AGENTTYPE_CORPORATE_BOSS` (names array index 5; civilian body type, `generated agent_types.xml`: HP 500, accuracy 60, reactions 44, armour 35), and the patch hand-writes `AGENTTYPE_X-COM_BASE_TURRET_{LASER,DISRUPTOR}` with the same 500/60/44 stats but **armour 0** on every zone (`data/common_patch/gamestate/agent_types.xml:1039-1228`). INFERRED: the name table label is a mislabel; record 5 is the turret (image id 0x15 is what the turret spawner asks for). Consequence: the fork's "Corporate Boss" is probably a stray 500-HP civilian and the turrets lack their 35 natural armour.
- OBSERVED scaling: TACP scales this record with no exemption (2.3): **HP 580, TU 69, reactions 51, internal inaccuracy 33 (shown accuracy 67), armour 57 on every zone at Superhuman.** Fork: HP 500, accuracy 60, armour 0 (and unscaled).

### 3.3 Turret weapons (diverges; medium-high confidence)

- OBSERVED `FUN_00024110` listing `0x24325-0x24370`: after creation, with `unit[+0xEE] = featureByte - 0xE6` (0..7): if it is 2, three calls of `FUN_00059f54(unit, item, slot)`: `(0x10, slot 0)` then `(0x11, slot 2)` twice; if it is 4, `(0x29, slot 0)`. `FUN_00059f54` (file `0xB49F8`) is the give-item routine used by the loadout code (`FUN_00024e5c`).
- OBSERVED item ids: TACP `AgentEquipmentData` rows (18 bytes, VA `0x2B2852 + id*0x12`, file `0x3022F6 + id*0x12`; the fork's own `AGENT_EQUIPMENT_DATA_OFFSET_START` is the same file offset): `0x10` type weapon, `0x11` type general (ammo), `0x29` type weapon; UFO2P `agent_equipment_names` (the index the extractor itself uses for TACP set records, `extract_agent_equipment.cpp:120-125`) names them `0x10 Megapol Plasma Gun`, `0x11 Megapol Plasma Pod`, `0x29 Disruptor Gun`. The base sector 23 turrets (k=2) therefore carry a **plasma gun + two plasma pods**; sector 24 turrets (k=4) a **Disruptor Gun**.
- Item stats from the EXE-extracted generated data (`agent_equipment.xml`): Plasma Pod `max_ammo 42, damage 35, accuracy 55, fire_delay 120, range 900`; Disruptor Gun `max_ammo 28, recharge 1, damage 42, accuracy 30, fire_delay 128, range 800`.
- Fork (LIVE `data/common_patch/gamestate/agent_equipment.xml:83-210`): laser `damage 26, accuracy 95, fire_delay 42, max_ammo 30, recharge 1` for sector 23, disruptor beam `damage 70, accuracy 75, fire_delay 40, max_ammo 34, recharge 1` for sector 24. `web_tactical.md` GH#750 (plasma 42/95/84) is a third set of numbers, not EXE-derived.
- INFERRED: slot argument 2 = backpack pocket; whether a pod is auto-loaded into the gun, and how many shots a plasma turret really has, was not decoded. Treat "2 pods x 42 shots" as an upper bound.

### 3.4 Roster slot reservation and the "3 active stations" claim (partially bound)

See section 4. OBSERVED: no counter in `FUN_00024110`; no counter in the base-sector placement loop `FUN_000b38b4` (file `0x115F58`, which iterates the 8 x 8 facility grid and places sector `type` for every completed cell). The only cap is `min(4 x stations, 12)` slot reservation. The web "only three security facilities are active, extras chosen at random, replacements do not activate" is **not found as a rule**; 12 turret slots = 3 stations is consistent with it but the selection logic is absent from every function read. exe_unbound for the 4th+ station (turrets beyond slot availability would simply fail to create when `FUN_0001f830` finds no free slot among 60).

---

## 4. Finding group C: base-defence force composition (the large divergence)

OBSERVED UFO2P `FUN_000ac7d0` (file `0x10EE74`; listing read, not the decompile, whose `case 0: break` is wrong) called from `FUN_000ac348` for the base-defence mission case:

1. Scan the 180-slot persistent person table (VA `0x144918`, stride `0xCE`). A person qualifies when the slot is in use, flag byte `+0x21 == 0`, the city byte matches the current city (`+0x7C == DAT_000d5060`), and either located in this base (`loc type 1`, base index matches) or aboard a craft that is in this base's building **and is class 0** (`loc type 3`, class byte `+0x23 == 0`). Class byte `+0x23` is switched through a 4-entry jump table at VA `0xAC7B0` whose four targets are `INC ECX / INC EDI / INC ESI / INC word [ESP+0x14]`: **all four classes (0 agent, 1 biochemist, 2 mechanic, 3 physicist) are counted**, soldiers included.
2. `T = FUN_000b42ec(base)` (file `0x116990`): over the 8 x 8 grid, every cell with construction byte `+0x56 == 0` and facility type `+0x16 in {0x17, 0x18}` adds **4**. `T` is clamped to 12.
3. `limit = 0x24 - T` (36 - T). If `total (the four counts summed) > limit`, **each of the four counts is replaced by `count * limit / total`** (integer division, `IMUL` then `IDIV`, listing `0xAC8DD-0xAC935`).
4. The second loop admits the first N persons of each class in table order, then `DAT_002440d4 += T` reserves T slots out of the 60 for turrets.
5. Aliens (or org guards) are created afterwards by `FUN_000ae218(mode 5)` / `FUN_000aedb0` into the same 60-slot table: hostile total is limited by `DAT_002440d4 < 0x3c` (60) and a separate `DAT_00244528 < 0x24` (36).

Worked cases (computed): 3 stations (T = 12), 20 agents + 10 biochemists + 20 mechanics + 10 physicists = 60 people: limit 24, each class x 24/60 -> 8 + 4 + 8 + 4. With 1 station (T = 4), limit 32: agents 20*32/60 = 10. With 24 X-COM people and T = 12 the roster takes 36 of 60 slots and the attackers are limited to 24.

**Fork (LIVE `game/state/rules/battle/battlemap.cpp:178-256`, unchanged in HEAD):** vehicle crews first, then every Soldier-role agent in the base building, capped at `MAX_UNITS_PER_SIDE = 36` (`battle.h:28`); only if fewer than 36, non-combat staff (by `homeBuilding->base`, not by current location); turret agents are created separately (`battlemap.cpp:1214-1222`) and are not counted; attacking aliens capped at 36 (`battlemap.cpp:138-149`). No proportional dilution, no turret reservation, no 60-slot total.

Strategy meaning (INFERRED from the observed rule): in the EXE, a research-heavy base with many staff and a few soldiers fields a **diluted** defence, and every Security Station you build lowers the human cap by 4 (up to -12) while adding turrets that are statistically tougher (3.2) but weaker offensively than the fork's. A garrison designed on the fork's rule (all soldiers first) will under-count the risk.

---

## 5. Finding group D: reinforcements

OBSERVED TACP `FUN_000243c0` (file `0x7EE64`), called every frame from `FUN_0006be18` (file `0xC68BC`; listing: `CMP [0xE4C20],0` then, only if `[0x1ED2F4] == 1 && [0x1ED2F4] == [0x1ED2CC] && [0x27A0EA] == 0`, `CALL 0x243C0` at VA `0x6BE50`), and in turn-based mode from the 400-iteration world-tick loop of `FUN_000b8c50` (file `0x1136F4`, call at VA `0xB8E85`). So the spawner is on only for mission-type fields `1/1` (the turret path uses `[0x1ED2CC] == 2`; INFERRED: 1 = alien-building mission, 2 = base defence):

- `if (DAT_0027ad10 == 0) return;` timer; `if (DAT_0027ad10 > DAT_000e6c24) { DAT_0027ad10 -= DAT_000e6c24; return; }` (decrement by the elapsed amount); otherwise:
  - `FUN_00024528` finds a spawn pad: scans from a random (x,y,z) start, wrapping, for a **2 x 2 block of ground-tile ids `0xD0,0xD1` over `0xD2,0xD3` (208..211; byte 0 of the 4-byte tile record, the same ground ids the fork's `reinforcementSpawners` table lists as 208..215)** in the map; returns -1 if none intact.
  - `FUN_00024780` picks the type: collects the distinct unit-type ids in `6..9` (Anthropod, Skeletoid, Spitter, Popper per the `image` ids of the table) among units in the table (live; plus freed slots whose second word still holds a type 6..9: INFERRED reading), then `rand` over that distinct set; -1 if empty.
  - `FUN_00024474` frees a slot if the 60-slot table is full; then **one** `FUN_0001f830(type, EDX=1 [hostile])`; `FUN_00024890` places it on the pad and equips it (via the D-biased loadout, 2.6/2.7).
  - **Reset: `DAT_0027ad10 = [0x1ED344]`, executed whether or not a unit was created** (listing `0x2445D-0x24462`).
- So the EXE adds **one unit per interval**. There is no loop in `FUN_000243c0`.
- OBSERVED: `[0x1ED344]` has exactly one reader (this function) and no code writer: a header field. The ten per-building intervals (20, 18, 35, 10, 40, 20, 12, 10, 30, 5 s in the fork's order) were NOT found in either EXE (contiguous and strided integer-array scans for both orderings, widths 1/2/4, scales 1..400, strides 2..64; plus 44 map directories on the ISO have no extra file). **exe_unbound**; the fork's values come from Wong's FAQ table 16.1.6 (`tools/extractors/extractors.cpp:108-119`, `wongFaq.txt:2450-2495`).
- Fork (LIVE `game/state/battle/battle.cpp:1974-1999` timer, `:2333-2385` `spawnReinforcements`): spawns **4** per expiry ("FIXME: Proper spawning algorithm for reinforcements, for now spawn 4 randoms", `:2370`), type `pickRandom(locationOwner->guard_types_reinforcements)` = Anthropod/Spitter/Skeletoid (`organisations.xml`), pads = map parts flagged by feature ids `208..215` (`tools/extractors/extractors.cpp:95-106`), capped by alive hostiles < 36. Only maps with `reinforcementsInterval > 0` have it: the ten alien-dimension buildings (`BATTLEMAP_39..48`, generated `battle_maps/*.xml`). Base defence and city raids have no reinforcements in either implementation.

Verdict: cadence diverges (4 vs 1), type rule diverges (present-species pool incl. Popper vs fixed list), pad rule differs in detail (intact 2 x 2 ground pattern 208..211 vs any part with ids 208..215 not destroyed), interval source unbound.

---

## 6. Finding group E: stun, capture, containment

### 6.1 Unconscious rule and recovery: match

- OBSERVED TACP `FUN_00022f90` (file `0x7DA34`) and `FUN_000b4154`: a unit is conscious iff `health != 0 && stun < health` (unit `+0x64` health, `+0x80` stun; `CMP DX,[EAX+0x109f50]; JLE` skips units with `health <= stun`).
- OBSERVED `FUN_00066474` (file `0xC0F18`), per-unit tick: while `stun != 0`, stun is decremented by 1 per call; when it falls below health the unit stands (`FUN_0004D3F0/490/434` by body type). The same function carries the Disruptor regen and the Mind Shield (`+30`, cap 200, inline here; this is the real location of the rule the docs cite as `FUN_0009b780`, which does not resolve) and psi-energy +1 per call. Its real-time-second cadence is the one already established through Disruptor regen (exe_evidence_index 5.10).
- Fork (LIVE `battleunit.cpp:1509-1516`, `:2505-2510` -1 per `TICKS_PER_SECOND`, `:1690` clamp `stunPower + maxHealth - stunDamage`): **match.**

### 6.2 Which species can be captured: match

OBSERVED UFO2P `FUN_000b21a8` (file `0x11484C`): hostile roster entries (record word `+0x1E == 1`, the creator's side argument, INFERRED to mean alien side) with species id `< 0xE` (1..13, all thirteen alien species) are all eligible for transfer. TACP `FUN_00022f90` treats Egg (1) and Chrysalis (5) as captured without being stunned and does not let them or the turret (0x15) block victory. No species-specific exclusion exists. (Queenspawn live capture has no special reward, exe_evidence_index 5.10.)

### 6.3 Capture score in the tactical total (diverges, medium confidence)

OBSERVED TACP `FUN_00022f90` listing (the decompile dropped blocks `0x2324B-0x233B7`; the listing is read in full):

1. Outcome: `[0x1ED32C] = +1` iff some X-COM-side unit is conscious and no hostile unit is conscious (eggs, chrysalises and turrets excluded from both counts); `-1` if only hostiles remain; else 0.
2. Combat rating `[0x27A088]` = sum of row `s` of an 8 x 8 kill-score matrix (`[0x279F68 + s*0x20 + i*4]`); leadership base = sum of the positive entries.
3. On a win, each hostile unit with `health != 0 && (stun >= health || type in {1,5})` adds its `score` byte (`unit+0x44`, the table `score`: Anthropod 10 ... Queenspawn 50; identical to the fork's live-species item scores) to the leadership base **and**, if the unit's side is not 1 (aliens), also to combat rating and total. So stunned **aliens** feed only the leadership base here.
4. Casualty penalty `[0x27A08C] = -casualties`. Leadership bonus = `base * (100 - 3*casualties) / 100`, and **0 if `30 * casualties > 1000`** (`CMP EAX,0xfffffc18; JGE`).
5. A third loop (gated by win, `[0x1ED2C8] == 1`, side != 1) adds each stunned/egg/chrysalis alien's `score` to the total, into the "Live Aliens captured" cell `[0x27A098]` if header flag `[0x1ED350] != 0`, else into combat rating. The debrief draws rows 0,1,2,5,6,7 and skips rows 3 (`#Alien corpses`, never written) and 4 (`Live Aliens captured`), so the line is never shown but is in the total.
6. A second credit loop (win, `[0x1ED2F4] == [0x1ED2CC] == 1`) adds the score of conscious hostile units that `FUN_00022f4c` excludes from the victory test (fled or out of the objective area; not decoded) to the total. No test of any craft, module or containment appears anywhere in the function.

Fork (LIVE `battle.cpp:2880-2910` `liveAlienCaptured += liveSpeciesItem->score` **only if `playerHasAlienStorage`** (Bio-Transport craft present or base defence with Alien Containment); `battle.cpp:4073-4079` total = combatRating + casualtyPenalty + leadershipBonus + liveAlienCaptured + equipment terms, leadership = `(100 + 3*casualtyPenalty) * (combatRating + friendlyFire + liveAlienCaptured)/100`, no cutoff).

Differences: (a) the fork withholds live-capture score when no storage exists, the EXE's TACP total does not (the header flag only selects the sub-line); (b) the fork has no leadership cutoff (it goes negative when `casualtyPenalty < -33`); (c) counting is otherwise structurally similar (captures appear in both the leadership base and the direct term). This matters because the tactical total goes into score category 0, which drives alien tech tier (2.6) and funding. Medium confidence because the meaning of header flags `[0x1ED2C8]`, `[0x1ED350]` is not decoded.

### 6.4 Live-capture storage requirement (match for craft; base defence unbound)

- OBSERVED UFO2P `FUN_00072e40` (file `0xD54E4`): for a vehicle, sums over its 45 equipment slots (`vehicle + 0x18D + slot`) the `alien_space` word (offset +6 of the 18-byte general-equipment record at file `0x18B576`) of every installed general module. Control: scanning the table, the only module with non-zero `alien_space` is **Bio-Transport Module = 20** (Cargo Module 50 cargo, Passenger Module 4 passengers have 0). The class table (VA `0x1296A4`, stride 0x18) and capacity table (VA `0x129ED8`, stride 0x12) used by the function are the `VehicleEquipmentData` array at file `0x18AD48` and the `alien_space` column at `0x18B57C`.
- OBSERVED `FUN_000b21a8` / `FUN_000b27b4` (files `0x11484C`, `0x114E58`): for a won mission, each captured alien is stored only if the candidate craft at the site (`FUN_000b26c0/FUN_000b2724` iterate craft by site) has `FUN_00072e40 > 0`; otherwise the next craft is tried; if none the transfer fails (`bVar5` flag). The fork requires a Bio-Transport craft too (`battle.cpp:2886-2905`, `isBioCarrierPresent` `:4060`): **match for craft**.
- **Base defence**: `DAT_00244024 == 2` sets the candidate to `-1` and the loop breaks immediately, so `FUN_000b21a8` stores nothing for base defence. A separate routine runs first: `FUN_000b44a4` (file `0x116B48`, called from `FUN_000b1dc0` when the mission type is 2) allocates a free slot (of 80) in the vehicle array on a win and records it at `[0x1304CC]`, apparently a virtual carrier for loot. How captured aliens reach base Alien Containment, and whether the EXE tests containment capacity at that point, was not decoded. **exe_unbound.** The fork requires `defendedBase->getCapacityTotal(Aliens) > 0` (`battle.cpp:3684-3700`); do not call that matching.

### 6.5 Side finding (unrequested, observed): post-battle money and relation effects

UFO2P `FUN_000b32ac` (file `0x115950`): for mission type 1 (and type 4 on a raid), the owning organisation's balance is changed by `DAT_00244090 * -0x32` and a relation delta of `0` if `DAT_00244090 <= 0x32`, `-(DAT_00244090-0x32)/0x14` up to 250, then `-10 - (x-250)/50` capped at -50, via `FUN_0005faf0`; for mission type 2 on a win X-COM's balance (`DAT_0017fb4c+8`) is reduced by `DAT_00244090 * 50`. `DAT_00244090` is some per-mission damage tally (not identified). If it is the building-damage count, low-damage (stun-only) raids stay below the threshold of 50 and cost nothing. Lead, not a finding.

---

## 7. Citation table (physical offsets, prologue bytes verified)

TACP non-4 (code = VA + 0x5AAA4): `FUN_0001f830` file `0x7A2D4` `5156575583ec1889`; `FUN_00024110` `0x7EBB4`; `FUN_000243c0` `0x7EE64`; `FUN_00024528` `0x7EFCC`; `FUN_00024780` `0x7F224`; `FUN_00024e5c` `0x7F900`; `FUN_000250f8` `0x7FB9C`; `FUN_00022f90` `0x7DA34`; `FUN_000b4154` `0x10EBF8`; `FUN_00066474` `0xC0F18`; `FUN_00059f54` `0xB49F8`; `FUN_00010294` `0x6AD38`; `FUN_000b3d54` (debrief) `0x10E7F8`; `FUN_0001eee8` `0x7998C`; stat panel `100 - byte` at VA `0x55574` -> `0xB0018`. Data (= VA + 0x4FAA4): `agent_type_data` `0x2FFAF8`; alien tier table `0x302B00`; difficulty global `0x1ED340` has no file bytes (loaded).

UFO2P non-4 (code = VA + 0x626A4): `FUN_000ad93c` `0x10FFE0`; `FUN_000ad38c` `0x10FA30`; `FUN_000ac348` `0x10E9EC`; `FUN_000ac7d0` `0x10EE74`; `FUN_000b42ec` `0x116990`; `FUN_000acc48` `0x10F2EC`; `FUN_000638dc` `0xC5F80`; `FUN_000ae1d4` `0x110878`; `FUN_000b21a8` `0x11484C`; `FUN_000b27b4` `0x114E58`; `FUN_00072e40` `0xD54E4`; `FUN_000b32ac` `0x115950`; `FUN_000b44a4` `0x116B48`; `FUN_000ae218` `0x1108BC`; `FUN_000adffc` `0x1106A0`. Data (= VA + 0x616A4): `agent_type_data` `0x155574`; alien tier table `0x19BEB4`; difficulty word `0x142340` (VA `0xE0C9C`; savegame stream offset 437116).

Original data (ISO `XCOM3/MAPS/37BASE`): `37SEC23.SMP` feature byte 232 x4; `37SEC24.SMP` feature byte 234 x4 (coordinates in 3.1).

LIVE engine lines cited: `game/state/shared/agent.cpp:121-149,165-173`; `game/state/battle/battleunit.cpp:1509-1516,1690,1824,1912-1913,2505-2510`; `game/state/battle/battle.cpp:1974-1999,2333-2385,2880-2935,3240-3420,3684-3700,4073-4079` (LIVE; HEAD differs only by 7 lines after line 3580 in `battle.cpp`, no relevant change); `game/state/rules/battle/battlemap.cpp:138-256,1010-1011,1214-1222`; `tools/extractors/extract_agent_types.cpp:1007-1008`, `extractors.cpp:95-119`, `extract_battlescape_map.cpp:593-611`; `data/common_patch/gamestate/agent_equipment.xml:83-210`, `agent_types.xml:1039-1228`.

## 8. Strategy implications at Superhuman (INFERRED from sections above)

Base defence:
- Expect harder aliens than the fork shows on HP/TU/armour (Skeletoid armour 22..26, Megaspawn 45..60, Multiworm HP 394) but note the fork's aliens also miss more; net effect for a defensive firefight is ambiguous, the armour and HP rise is certain.
- Do not rely on one-shotting Poppers with Disruptor weapons.
- Staff dilution: keep the number of people physically in the base near or below `36 - 4*stations` (max 24 with 3+ stations) or accept proportional loss of soldiers. Prefer sending staff elsewhere before a likely attack; the rule counts only people located in the base building, plus class-0 agents aboard craft parked there.
- Stations: each completed Security Station adds 4 turrets, each Advanced one 4 more; the first 3 stations' turrets are reserved. The stations lower the human cap by up to 12. Turret quality is high on defence (580 HP, armour 57) but their weapons are plasma gun (35 damage, 55 accuracy per pod stats) and Disruptor Gun (42 damage, 30 accuracy), not the fork's 26/95 and 70/75, so the fork over-states turret firepower.
- Captured aliens in base defence: do not assume containment is required or sufficient (unbound); a Bio-Transport craft is required for captures elsewhere.

Raids and alien-dimension:
- Org guards (Cult, Marsec, Megapol, Gangster) are scaled too (HP 46..58, armour +64% at D4) and their gear skews toward the top equipment sets at Superhuman (about 64% of rolls in the two best buckets at roll time in UFO2P). The fork's weekly org tech-level model understates early guard gear on hard difficulty and overstates growth later.
- Reinforcements apply only to the alien dimension and are one unit per interval, not four.

## 9. Engine-fix sketch (summary; per-finding detail in the structured output)

1. Add `difficulty` to `AgentGenerator::createAgent` (or a battle-side post-pass for non-X-COM agents) with the table in 2.2: stats x(100+4D)% etc., psi caps 100, armour x(100+16D)% when the type has natural armour, inaccuracy byte x(100-4D)% (i.e. stored accuracy = `100 - floor((100-acc) * (100-4D)/100)`); exempt X-COM agents.
2. Re-bind record 5 as the base turret type and give it armour 35; use Megapol Plasma Gun + 2 pods (sector 23) and Disruptor Gun (sector 24) instead of the invented turret weapons.
3. Rewrite the base-defence player roster per section 4 (turret reservation, `36-T` cap, proportional dilution across the four roles, 60-unit total).
4. Reinforcements: one unit per expiry, pool = distinct present species among {Anthropod, Skeletoid, Spitter, Popper}, intact 2x2 pad pattern, reset timer on every expiry.
5. Capture score: do not gate `liveAlienCaptured` on storage; add the leadership cutoff (`30*casualties > 1000 -> 0`).
6. Alien-tier handicap and human-gear bucket: blocked on RE (sections 2.6, 2.7).

## 10. Not done / leads (ordered)

1. UFO2P writer of the tactical header (difficulty at `0x1ED340`, interval `0x1ED344`, flags `0x1ED2C8/0x1ED350`): the TACP block descriptor table (VA `0x27F00C`, 10-byte entries `{size,addr,flag}`; entry 4 = the 1966-byte `BUILDING.DAT`, entry 5 = 212 bytes at `0x1ED294`) does not line up with the `savegame.00` writer order (`FUN_000a3234`, `FUN_000a4054`) I parsed. The tactical start file is written by a path I did not find (`FUN_00016c67`, `FUN_000b3ee0`, `FUN_000b0380`, `FUN_000ac304` are candidates).
2. Reinforcement intervals: not in either EXE; check the header builder above.
3. `FUN_0006e608/FUN_0006ea78` tier handicap table at VA `0x143A7E`.
4. Base-defence capture routing (`FUN_000b44a4`, `[0x1304CC]`) and whether Alien Containment capacity is enforced.
5. Org tech level consumer in tactical gear; confirm the bucket-to-set mapping for Anthropod/Skeletoid.
6. Hit-chance function using the unit accuracy byte (confirm the direction numerically): `FUN_00055400`-area stat panel computes `100 - byte`; the firing code was not decoded.
7. Turret ammo semantics of `FUN_00059f54` slot argument 2 and the plasma turret's real shot count.
8. `DAT_00244090` meaning (section 6.5).
