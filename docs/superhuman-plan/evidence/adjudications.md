# Adjudications of refuted/contested findings

Final word on each disputed validation finding. Supersedes the validator verdict in _validation_register.md for these IDs.

| key | id | final verdict | final impact | confidence | target tree | disputed point |
|---|---|---|---|---|---|---|
| v_takeover | T4 | engine_missing | medium | high | both | T4 asserts that nothing in UFO2P.EXE can reverse an alien takeover. The only liberation code the validator found was dead (FUN_0007abf0). The exe-lens refuter s |
| v_takeover | T8 | diverges | medium | high | both | Whether T8's "match" survives its exhaustive claim that difficulty enters takeover-relevant play "only via" five items (infiltration divisor 42-d, starting rela |
| v_takeover | T10 | engine_missing | medium | high | HEAD | Verdict engine_missing is not disputed. The dispute is the write-up around it: (1) blocked_on_re=true, i.e. that the "three composition tables" still need extra |
| v_relations | F05-vehicle-downed | diverges | high | high | both | Whether the EXE charges -20 (crash threshold) for X-COM downing a Government/Megapol vehicle, and whether the engine can charge such a vehicle twice. The valida |
| v_relations | F08-empty-building-investigation | diverges | low | high | both | Whether F08 (Investigate Building on a building with no aliens) is a "match" or a divergence. The validator said match (impact medium) but its own text lists "n |
| v_relations | F14-no-periodic-drift | match | high | high | none | The verdict itself was not in dispute: both refuters voted refuted=true but verdict_should_be unchanged/match. Two supporting claims were disputed. (a) Whether  |
| v_gov_income | G2-cutoff-latch | match | high | high | HEAD | Whether the G2-cutoff-latch finding stands. Both refuters accept the mechanic and the LIVE port. The exe lens marked it refuted only because the strategy_rule_a |
| v_base_defence | BD02 | diverges | medium | high | both | Both lenses agree on the EXE facts for roles 2 and 3. They differ on whether the engine's Overspawn routing widens the set of UFOs that start a base defence, an |
| v_base_defence | BD04 | diverges | medium | high | both | The roster divergence itself was not contested; the refuter disputed two parts of the validator's write-up. (a) The fix item "cap active stations at 3", which t |
| v_base_defence | BD08 | engine_bug | high | high | both | Whether BD08's verdict "match" (hostile-org raids on X-COM bases exist and are scheduled with the same structure in engine and UFO2P.EXE, fix_sketch "none for s |
| v_base_defence | BD14a | diverges | high | medium | both | Whether BD14a stays "match, fix none, with two bases the loss is survivable", or the engine diverges because Base::die kills every agent and docked craft in the |
| v_research_rates | RR-10 | diverges | medium | high | both | The verdict "diverges" is not in dispute. The disagreement is over (1) the validator's magnitude, "engine arrivals roughly 2-3x the EXE" (2.5-4.5 vs 1.5 per tec |
| v_agents | V-AG-05 | diverges | low | high | both | Whether UFO2P.EXE auto-assigns training to the 10 starting soldiers (validator: no, so the engine's gamestate.cpp:802/803 pre-assignment diverges and the harnes |
| v_agents | V-AG-14 | engine_missing | medium | high | both | Not the verdict class (validator and the engine-lens refuter both say engine_missing). The dispute is whether the validator's written EXE description and fix sk |
| v_victory_chain | VC-1 | diverges | high | high | both | Whether VC-1 'match' survives once the success flag UFO2P [0x244084]==1 (the validator's INFERRED 'battle success') is tied to its TACP producer. Engine lens: t |
| v_victory_chain | VC-12 | diverges | low | high | both | Whether the engine gives an arriving craft any order in the alien dimension (validator: "Absent: arriving craft take no target", engine_missing, medium), and wh |
| v_victory_chain | VC-16 | diverges | low | high | both | Does the EXE reset the alien-dimension fleet on every visit (validator: counts, respawned full-health, kill effect unobserved, blocked on RE, medium impact, reb |
| v_ufo_consumers | F2_ufo_incursion_package_fit | diverges | low | high | both | Whether the engine's fleet-fit test in GameState::invasion() is equivalent to UFO2P.EXE FUN_0006cfb4's fit test (validator: match, "same algorithm"). The engine |
| v_ufo_consumers | F9_alien_craft_gear_score_gates | engine_bug | medium | high | HEAD | Whether the engine's alien-craft gear gate (shields, cloak, teleporter on lifetime UFO-kill score) is applied to every UFO at launch the way the EXE applies it. |
| v_ufo_consumers | F14_food_chamber_mutant_alliance_reset | engine_missing | medium | high | both | The core claim (Food Chamber raid reverses the Mutant Alliance takeover in the EXE, and the engine has no such code) was not disputed. The engine-lens refuter f |
| v_market | F-M2 | diverges | high | high | both | The core mechanic (week-0 price cut driven by post-decay standing stock vs the engine's weekly sold delta) was not disputed by either refuter. The dispute was o |
| v_market | F-M11 | diverges | low | high | both | Is the EXE's raid-launch write (X-COM's relation to the raided owner forced to <= -60) persistent for the rest of the campaign, so that the engine's transient - |
| v_market | F-M12 | engine_missing | medium | high | both | The engine-lens refuter agreed the verdict is engine_missing but disputed three claims in the validator's write-up. (1) The validator said the EXE function enfo |
| v_tactical_difficulty | TD6 | diverges | low | high | both | Whether UFO2P/TACP pick human org-guard gear through a difficulty-biased 5-slot equipment-set bucket (rec[+0x53+bucket], thresholds <5,<9,<12,<14), so that the  |
| v_tactical_difficulty | TD17 | diverges | medium | high | both | Not the verdict, which both sides call "diverges". The dispute is over three supporting claims in the validator's exe_behaviour. (1) Whether the debrief hides t |
| v_tactical_difficulty | TD19 | diverges | low | high | both | Whether TD19 ("live-capture transfer needs a craft with Bio-Transport capacity") is a clean match with fix none, as the validator ruled, or diverges. The refute |
| v_score | SCORE-C1 | engine_bug | low | high | both | Whether SCORE-C1 (Category 1 Research Completed score) is a clean "match / fix none". The validator said yes (value, cadence and once-per-topic all match; movin |
| v_score | SCORE-C2-TRIGGER | engine_bug | medium | high | both | Three things were disputed. (1) Cadence: the validator said the EXE visits each building every ~N_buildings minutes independent of difficulty, so the engine's T |
| v_score | SCORE-C5 | match | high | high | none | Whether SCORE-C5's engine-side text is correct. The validator said the engine has no damaged-UFO withdrawal, so an EXE withdrawal "would add further charges" an |
| v_score | SCORE-C6 | diverges | medium | high | both | Whether the engine's vehicle-plow-through Damage to City charge (vehicle.cpp, VehicleMover::updateFalling) matches EXE writer 2 (FUN_00041644, file 0xA423D). En |
| v_score | SCORE-C0 | diverges | low | high | both | Whether the validator's EXE formula for the Category 0 tactical score is complete. The validator said the Total is combat rating - casualties + leadership + equ |
| v_score | SCORE-UNLOCK-CRAFT | diverges | low | high | both | Whether UFO2P.EXE latches the nine score-unlock flags (alien craft module gating) only at the Monday rollover, so the EXE lags the engine's live read by up to a |
| v_score | SCORE-EQLEVEL | diverges | low | high | both | Whether SCORE-EQLEVEL can be called "match, fix: none" when its own exe_behaviour includes "a random row of 10 per weapon/grenade/equipment group", given that t |
| consistency | UFO_GROWTH_STOP | diverges | high | high | both | Which destroyed alien building stops weekly UFO growth in UFO2P.EXE, in which direction the engine diverges, and whether the 'destroyed byte' framing and the al |

## v_takeover / T4 -> engine_missing (medium, high)

**Disputed point.** T4 asserts that nothing in UFO2P.EXE can reverse an alien takeover. The only liberation code the validator found was dead (FUN_0007abf0). The exe-lens refuter said a live reversal exists in FUN_000b32ac, which clears org 24 (Mutant Alliance) on a won Food Chamber raid. The engine-lens refuter did not find it.

**Ruling.** The exe-lens refuter is right and the validator's "match" does not hold. I confirmed the reversal exists and is reachable in retail play.

**The writer.** Bytes read from the canonical UFO2P.EXE confirm the clear. At file 0x1159E9, `cmp [0x244048],2` tests the building function index. At file 0x1159FD, `mov byte [eax+0x2921],0` clears the flag, and 0x2921 is 24*0x1B6+0x11. Org 24 is Mutant Alliance in the OG order. Function index 2 is the Food Chamber in the EXE's alien-building name order.

**Why the project's "dead branch" claim is wrong.** The branch is gated on `[0x244084]==1`. Earlier notes (O2-cargo-seize-pass2) call that field always 0 because its only absolute-address stores are zero stores. But that dword is the +0x98 field of a 0xD4-byte scenario record at VA 0x243FEC. The record is bulk-loaded from `scenario/scenarir.dat` by FUN_000b1d9c at VA 0xB1D9C. That function is called from startup at VA 0x1028F when the command line is `TACTICAL`. TACP.EXE writes the file: FUN_00022f90 stores +0x98 = 1 when X-COM has live units and no opponents are alive, and the end-of-battle writer dumps the record with a "wb" fopen.

**Nothing zeroes the field in between.**
- The loads after the scenarir read (objectr, unitr, diplor) land at 0x2390F8 to 0x241D98, 0x241D98 to 0x243FD0, and the relation matrix. None reaches 0x244084.
- The save loader has 130 reads and none targets 0x2440xx.
- The two zero-writers (FUN_000aff9c and 0xBC2B8) are not reachable from the post-battle prelude. The reset routine at 0xBC2B8 is reached only through FUN_000BA5F0, a UI button callback.
- UFO2P4.EXE has identical code.

**Cross-check inside the project.** LIVE battle.cpp:3581-3587 already ports "Alien Buildings Destroyed" from the same `[0x244084]==1` block, citing file 0x1159E4.

**What the reversal does.** On a won Food Chamber raid it sets the Mutant Alliance flag to 0 and its infiltration to 0, sets both X-COM-Mutant relations to 100, and sets Mutant->Aliens to -100. It also fires when the Mutant Alliance was never taken over. The engine's `takenOver` is only ever set to true, and `game/` has no Mutant Alliance or Food Chamber logic.

**What still stands.** Takeover of Government and every other org stays permanent, so the "prevention is the only strategy" rule holds with a one-org exception. The impact is medium because the Food Chamber payoff gives the Mutant Alliance a guaranteed alliance (relation +100), which gates Hybrid hiring.

**Engine.** `Organisation::takeOver` (organisation.cpp:84-108) sets `takenOver = true`, `militarized = true` and infiltration 200, and writes the relations. `takenOver` is set only there and never reset. The hourly update pins infiltration to 200 (organisation.cpp:639-642) and the bribe screen refuses (bribescreen.cpp:68-77). There is no ORG_MUTANT_ALLIANCE logic, only the infiltration-screen list at infiltrationscreen.cpp:176. The RaidAliens/playerWon branch in battle.cpp (HEAD 3571, LIVE 3578) does not touch organisations. The readers of `takenOver` are the takeover roll skip, the bribe-request suppression (gamestate.cpp:1590), the building detection halving (building.cpp:807) and the bribe refusal. None is a loss condition or a score term.

**EXE.** In UFO2P.EXE non-4, FUN_000b32ac (VA 0xB32AC, file 0x115950) runs after a won raid in the alien dimension, when the event type is 1 or 4 and `[0x244084]==1`. If the building function index is 2 (Food Chamber), it does the following.
- Clears the Mutant Alliance takenOver byte (file 0x1159FD).
- Floors M[24][X-COM] to 100 (file 0x115A09) and mirrors it into M[X-COM][24] (file 0x115A1F).
- Calls FUN_0005faf0(24, Aliens, -50) (file 0x115A29). It writes only M[24][Aliens] and skips the ripple in the alien dimension.
- Forces M[24][Aliens] to -100 (file 0x115A3B).
- Sets infiltration[24] = 0 (VA 0xB33A0, file 0x115A44).

Index 7 (Control Chamber) sets `[0xF3C78]` instead. The generic liberation function FUN_0007abf0 is dead code, as the validator said. Every other org's takeover is permanent (hourly pin FUN_0006fcc0, no decay).

**Evidence.** Addresses are UFO2P.EXE non-4 unless marked. Code file offset = VA + 0x626A4. Data file offset = VA + 0x616A4. I re-read the bytes with dd and a LE fixup parse plus capstone. No Ghidra copy was created.
- **Clear and gate:**
  - 0x1159E9 `83 3d 48 40 17 00 02`.
  - 0x1159FD `c6 80 21 29 00 00 00`.
  - 0x115A44 `89 15 c8 09 07 00` (infiltration[24] = 0).
  - The gate `cmp [0x244084],1` is at file 0x11599A.
- **Absolute writers of 0x244084:** only the zero stores at VA 0xB0296 (file 0x11293A) and VA 0xBC3C1 (file 0x11EA65). Absolute-address scans all agree on this, which is why earlier work called the field dead.
- **Record loader:**
  - FUN_000b1d9c (file 0x114440) loads `scenario/scenarir.dat` into 0x243FEC via FUN_00013cb8.
  - The filename string is at file 0x1357E4.
  - The call is at VA 0x1028F (file 0x72933), taken when `[0x4CBC]==2`.
  - That variable is set at VA 0x10091 when the command-line arg equals the string at VA 0xD0008, "TACTICAL".
- **Loads that follow and the unreachable zero-writers:**
  - FUN_000b1dc0 loads objectr.dat (0x8CA0 bytes), unitr.dat (0x2238 bytes, ending at 0x243FD0) and diplor.dat (the relation matrix).
  - Its callees cannot reach FUN_000aff9c, ac08c, ac348 or 0xBC2B8.
- **TACP.EXE** (data file = VA + 0x4FAA4, code file = VA + 0x5AAA4, checked against fixup operand bytes):
  - FUN_00022f90 (file 0x7DA34) stores 1 at VA 0x23092 (file 0x7DB36).
  - The writer at VA 0x15838-0x15851 (file 0x7B2DC) calls FUN_00013e64 with mode string "wb" (VA 0xE0600) to write 0xD4 bytes to scenarir.dat.
- **Relation primitive:** FUN_0005faf0 at VA 0x5FAF0, with the alien-dimension ripple skip at 0x5FB76.
- **UFO2P4.EXE** has the same code (clear at file 0x1164ED, zero stores at 0x113313 and 0x11F919).
- **Engine corroboration:** LIVE battle.cpp:3581-3587 ports the score from the same block.
- **Engine absence:** takenOver is set only at organisation.cpp:90 and there is no Mutant Alliance code in game/.
- **Notes file:** validate/adj_v_takeover_T4.md.

**Fix (target both).** Add the Food Chamber payoff in the `Battle::finishBattle` RaidAliens branch. That is `case MissionType::RaidAliens`, `if (playerWon)`, at battle.cpp HEAD ~3571 and LIVE ~3578. If `building->function` is BUILDINGFUNCTION_FOOD_CHAMBER, and to match the EXE the engine's Food Chamber objective is met (the EXE gate is TACP FUN_00025310, a jump table on the function index), then for ORG_MUTANT_ALLIANCE:
- set `takenOver = false`;
- set `militarized = militarizedFromType(<org type>)`;
- set `infiltrationValue = 0`;
- `establishRelation(player, 100)` in both directions (mutant<->X-COM);
- `mutant->establishRelation(aliens, -100)` in that one direction only, with no ripple (the EXE skips it in the alien dimension).

Do this even if the org was never taken over. Do not run the generic -50 delta path after the flag is cleared.

Add a test: a won Food Chamber raid frees a taken-over Mutant Alliance and sets relation 100, while a Government takeover stays permanent. Reopen O2-cargo-seize-pass2 section 3 and C2-secondary-objectives section 2 (the Food Chamber to Mutant Alliance row) and amend the validator write-up: the extra clear site at VA 0xB3359 and infiltration[24] = 0 at VA 0xB33A0. Leave HEAD's missing `destroyedScore` hunk out of this fix; it is a separate gap.

## v_takeover / T8 -> diverges (medium, high)

**Disputed point.** Whether T8's "match" survives its exhaustive claim that difficulty enters takeover-relevant play "only via" five items (infiltration divisor 42-d, starting relations, detection weight, raid timer, bribe terms), and its strategy rule that Superhuman "only speeds infiltration growth by 42/38". The refuter found an additional difficulty reader, the new-game alien seeding in FUN_00015400, and says the engine reproduces it with smaller ranges.

**Ruling.** The refuter is right on the point in dispute, and the finding has to be split.

SURVIVES (still a match): the takeover roll and its cadence have no difficulty term in the EXE or the engine. The five listed items also match. Nothing in this ruling changes that.

DOES NOT SURVIVE: the clause "difficulty enters only via ...", and the strategy rule built on it. The EXE has a sixth difficulty reader, the new-game alien seeding, and the engine seeds it too small at every difficulty.

I re-derived this from the UFO2P non-4 bytes myself rather than trusting the refuter.

1. FUN_00015400 has two halves. The first half builds the X-COM start building. The second half seeds aliens into a random building: it retries while the owner's relation to X-COM is below 0, then writes crew[1] = 1 + rand16(d/2+1) and crew[5] = 1 + rand16(d/2+2). rand16(n) is inclusive [0,n], so the ranges are crew[1] in 1..d/2+2 and crew[5] in 1..d/2+3. At Superhuman (d=4) that is 1..4 and 1..5.
2. The function has exactly one caller, a call at VA 0xB9502 inside new-game init FUN_000b94c0. Nothing in the later init callees that I scanned (FUN_000799dc, FUN_00010380, FUN_0006bdac) stores to the building crew bytes. I did not trace every callee, so "nothing clears the seed" rests on that scan.
3. The species mapping is OBSERVED, not inferred. The EXE's per-building crew sum, FUN_0006fc34, multiplies crew byte k by the dword table at file 0x15541C. That table reads [0,1,1,1,0,2,2,2,1,1,3,1,2,0,0,2]. The engine extractor reads its per-agent speeds from AGENT_INFILTRATION_SPEED_OFFSET_START = 1397788 = 0x15541C, the same table. It sets infiltrationID 1 for Brainsucker and 5 for Anthropod (extract_agent_types.cpp:444 and :509). So EXE crew[1] and crew[5] are the engine's Brainsucker and Anthropod by index.
4. The engine extractor writes Vec2{1, d/2+2} for both species (extract_agent_types.cpp:1335 and :1337, identical in HEAD and LIVE). The seeding consumes it through randBoundsExclusive, which is [x, y-1] (HEAD gamestate.cpp:705-709, LIVE :706-710). So the engine seeds 1..d/2+1 for both species: 1..3 each at Superhuman. The generated gamestate.xml shows initial_aliens key 4 = {1,4} for both species. No difficulty patch (data/difficulty*_patch) or common_patch carries its own initial_aliens ranges.
5. The path is reachable. difficultymenu.cpp:35 and mainmenu.cpp:31 both call GameState::startGame(), and tools/oa_play.py:678 notes the harness relies on startGame's seeding behaviour. A new-game benchmark therefore runs the under-seeded engine path.

Size of the gap at d=4. Mean seed numerator (crew times alien speed, summed) is 8.5 in the EXE and 6.0 in the engine; the maximum is 14 versus 9. The gap matters more than the mean suggests because the hourly modifier is an integer division of numerator x building-function speed x org speed by 38, and the odd-hour decay of 1 keeps running. Building-function speeds in the EXE are mostly 2 to 3 and org speeds 1 to 4, so a product of 4 to 9 is typical. Over that range the chance the seeded org's modifier reaches 1 or more is 0.00 to 0.78 in the engine against 0.40 to 0.90 in the EXE. At a product of 4 the engine never grows the seed on its own and the EXE does 40% of the time. That is why I keep the impact at medium rather than low.

Corrected strategy rule. Superhuman does two things: it speeds hourly infiltration growth by 42/38 versus Beginner, and it seeds a larger starting infestation (Brainsucker 1..4, Anthropod 1..5), which the engine under-seeds. It does not make takeover rolls likelier or more frequent.

Cross-finding conflict to resolve in the plan. T16 (v_takeover) says FUN_00015400 is only the X-COM start building and "places no alien crews", and marks where initial building crews come from as exe_unbound. Those bytes contradict that: the second half of the same function seeds crew[1] and crew[5]. T16's open question is answered by this ruling, so the plan should update T16 rather than carry two contradictory records.

**Engine.** Setup in GameState::startGame seeds one random building of CITYMAP_HUMAN, retrying while the owner's relation to the player is below 0. Each (agent type, Vec2) entry in initial_aliens[difficulty] is applied as current_crew[type] = randBoundsExclusive(rng, x, y), i.e. uniform on [x, y-1]. The extractor sets both AGENTTYPE_BRAINSUCKER and AGENTTYPE_ANTHROPOD to Vec2{1, difficulty/2+2}, so each gets 1..d/2+1: at Superhuman, 1..3 for both. The building is then flagged initialInfiltration = true. Code is HEAD gamestate.cpp:705-709 and LIVE :706-710. Extractor lines are identical in both trees: tools/extractors/extract_agent_types.cpp:1332-1337. Generated data confirms initial_aliens key 4 = {1,4} for both species. The takeover roll has no difficulty term (organisation.cpp:84-109), and the infiltration divisor is 42 - difficulty (organisation.cpp:648).

**EXE.** UFO2P non-4 FUN_00015400 (VA 0x15400, file 0x77AA4). After building the X-COM start building, it runs a retry loop (VA 0x15865-0x1589B) that picks a random building until M[owner][X-COM] is at least 0. It then writes building crew[1] = 1 + rand16(d/2+1), a range of 1..d/2+2, and crew[5] = 1 + rand16(d/2+2), a range of 1..d/2+3, where d is the high word of the dword at VA 0xE0C9A. At Superhuman: crew[1] in 1..4 and crew[5] in 1..5. rand16 is FUN_0005d1d8, which returns a value mod (n+1), so the range is inclusive [0,n]. The speed table at file 0x15541C gives crew[1] speed 1 and crew[5] speed 2. The takeover roll FUN_0006fd9c has no difficulty read. The infiltration divisor is 42-d (VA 0x6FCE3). No other reader of the difficulty word writes crew bytes.

**Evidence.** All offsets are physical file offsets in UFO2P non-4 (CRC 0x4749ffc1, extractor-canonical). Code file offset = VA + 0x626A4. I checked each item with an offline capstone disassembly of canonical/UFO2P.EXE; I used no Ghidra copy.
- Difficulty reads: `mov edx,[0xE0C9A]; sar edx,16` at VA 0x1589D (file 0x77F41) and VA 0x158DE (file 0x77F82).
- Seeds: `inc bl; mov [edx+eax*2+0xD5],bl` at VA 0x158D7 (file 0x77F7B) for crew[1], and `...+0xD9` at VA 0x1591A (file 0x77FBE) for crew[5]. Building stride is 0xE2 on base [0xB276C].
- Retry loop at VA 0x15865-0x1589B (file 0x77F09-0x77F3F): `cmp byte [owner*0x1C + raw 0x9EC28],0; jl` is the M[owner][X-COM] >= 0 test.
- rand16 FUN_0005d1d8 (file 0xBF87C): the tail `inc ecx; idiv ecx; mov eax,edx` is at file 0xBF8F3 and 0xBF8F9, giving [0,n].
- Single caller: VA 0xB9502 (file 0x11BBA6) in FUN_000b94c0, right after FUN_00099130 at VA 0xB94FD.
- Crew layout and speeds: FUN_0006fc34 (file 0xD22D8) reads crew[k] at [bld+0xD4+k] for k=0..13 and multiplies by the dword table at file 0x15541C. That equals the engine's AGENT_INFILTRATION_SPEED_OFFSET_START = 1397788 in tools/extractors/common/agent.h.
- Divisor read (hourly infiltration, FUN_0006fcc0, file 0xD2364): `mov edi,[0xE0C9A]` at VA 0x6FCE3 (file 0xD2387). The finding's cite of 0xD238B falls mid-instruction, so use 0xD2387.
- Engine: HEAD extract_agent_types.cpp:1335,1337 (identical in LIVE); HEAD gamestate.cpp:705-709 and LIVE :706-710; library/xorshift.h:133-136 (randBoundsExclusive = randBoundsInclusive(min, max-1)); organisation.cpp:648. Generated gamestate.xml initial_aliens key 4 = {1,4} for both species. No initial_aliens or Brainsucker override in data/difficulty*_patch or in LIVE data/common_patch. Reachability: difficultymenu.cpp:35 and mainmenu.cpp:31 call startGame().
- Side observations, not part of this ruling:
  - gamestate.cpp:704/709 loops `|| counter >= giveUpCount`, which keeps retrying rather than giving up.
  - The EXE's second building pick uses rand16(count), which is inclusive of count (one past the last index), while its first pick uses count-1. That is a negligible out-of-range case (about 1 in the building count).

**Fix (target both).** Change the two extractor lines so the exclusive bounds equal the EXE's inclusive ranges. In tools/extractors/extract_agent_types.cpp, BRAINSUCKER at line 1335 becomes Vec2<int>{1, difficulty / 2 + 3} (inclusive 1..d/2+2), and ANTHROPOD at line 1337 becomes Vec2<int>{1, difficulty / 2 + 4} (inclusive 1..d/2+3). This is a data-only change and needs no change to gamestate.cpp. The file is identical in HEAD and LIVE, so the same two-line edit applies to both. Then regenerate base_gamestate, which is generated and untracked; a source edit alone changes nothing at runtime. The fix is exact, with no new reverse engineering needed, so blocked_on_re is false. Add a unit check that initial_aliens[4] reads {1,5} for Brainsucker and {1,6} for Anthropod under exclusive bounds. Separately, in the plan record T8 as: takeover roll and cadence match (kept), the starting infestation diverges (this fix), and update T16 to cite FUN_00015400's second half as the source of day-1 crews.

## v_takeover / T10 -> engine_missing (medium, high)

**Disputed point.** Verdict engine_missing is not disputed. The dispute is the write-up around it: (1) blocked_on_re=true, i.e. that the "three composition tables" still need extracting; (2) the fix sketch being only an "approximation"; (3) the role label "defender composition", which the validator marked INFERRED.

**Ruling.** The refuter is right on all three. Verdict stays engine_missing, impact medium. I re-checked these myself and they hold.

(a) FUN_000b46c4 bytes. The physical file offsets match the refuter's. UFO2P non-4 file 0x116D68 starts with the Watcom prologue 53 51 52 56 57 55. Bytes 0x116DB9-0x116DEC decode to: tier = byte[org+6] + [0xE0E7C] - 1; if byte[org+0x11] != 0 then tier + 3; clamp below to 0 and at 12 or above to 11.

(b) The flag. Org +0x11 is the taken-over flag. The takeover roll writes it, the bribe screen refuses on it, and the dev editor labels it "like alien". The org is the unit's owner, which is the building owner in FUN_000aec70 (ad93c is called with byte[bldg+0xC8]).

(c) The tables are not new RE. UFO2P file 0x19C3A0 for 1680 bytes is byte-identical to TACP file 0x302FEC (3157996). That is AGENT_EQUIPMENT_SET_SCORE_HUMAN_DATA_OFFSET_START, which the engine already extracts as EQUIPMENTSET_HUMAN_1..12. The layout matches AgentEquipmentSetScoreDataHuman: weapons[10][12] at 6 bytes each, grenades[10][12] at 4 bytes each, equipment[10][12][2] as words. The EXE strides 72, 6, 48 and 4 agree with it. So blocked_on_re must be false.

(d) The role is per-unit initial loadout, not a defender composition table. FUN_000b46c4 is gated on the agent type's inventory flag (type+0x44, copied to unit+0x4E). It is the EXE original of the human branch at agent.cpp:171-175. Its only two callers are FUN_000aec70 (owner guards) and FUN_000aedb0 (org attacker).

(e) The baselines agree. Engine set index is tech_level-1, because HUMAN_i has min_score i+1 and HUMAN_12 has max INT_MAX. tech_level starts at starting_tech_level+1 (extract_organisations.cpp:50), and the OrganisationData struct has starting_tech_level at +6 and average_guards at +7. EXE tier at game start is start + 1 - 1, so both give start. The weekly +1 and the cap at tier 11 agree too.

(f) The fix is exact. Passing tech_level+3 needs no clamp, because HUMAN_12 absorbs any score of 12 or more.

Residual caveat: the agent.cpp human branch also equips map guardians (battlemap.cpp:1220), spawnUnit, and hire-pool agents, which are stripped. The EXE gives +3 only to the two guard-queue paths. Whether guardian units get it in the EXE is unverified.

**Engine.** No takenOver term anywhere in loadout generation. agent.cpp:171-175 (LIVE = HEAD, byte-identical) does EquipmentSet::getForType(state, Human, org->tech_level)->generateEquipmentList(state) for every org except player, aliens and civilians. tech_level = starting_tech_level+1 (extract_organisations.cpp:50), then +1 weekly capped at max human min_score = 12 (HEAD gamestate.cpp:1707-1722, LIVE 1740-1755). takenOver is read only in gamestate.cpp, building.cpp, organisation.cpp and bribescreen.cpp, never in agent, battlemap or aequipmenttype code. Guards come from battlemap.cpp:382, via createAgent(state, owner, guard type) with owner = building->owner.

**EXE.** UFO2P non-4 FUN_000b46c4 sets tier = org[+6] + weekCounter[0xE0E7C] - 1, adds 3 if org[+0x11] (taken over) is non-zero, and clamps to 0..11. It runs only for units whose agent-type inventory flag is set. It rolls rand16(9) for the weapon row, the grenade row and the gear row, and reads column `tier` of each table. Unit cap is 500. Called only from FUN_000aec70 (owner guards) and FUN_000aedb0 (org attacker). A taken-over org's guards are therefore armed from a set up to 3 tiers higher; Government at start tech 3 gets HUMAN_6 instead of HUMAN_3.

**Evidence.** All offsets are physical file offsets in UFO2P.EXE non-4 (CRC 0x4749ffc1) unless noted; VA is the Ghidra program VA I used (code file = VA + 0x626A4).
- FUN_000b46c4 is at file 0x116D68 (VA 0xB46C4); I read the prologue bytes 535152565755 there.
- The tier bytes at 0x116DB9 onward decode to: mov eax,[0xE0E7C]; movzx esi,[edx+6]; sub eax,edi (=1); add esi,eax; mov bl,[edx+0x11]; test bl,bl; jz; lea edx,[esi+3]; clamp <0 to 0; cmp 0xC, jl, else 0xB. VA range 0xB4715-0xB4748.
- Tables: file 0x19C3A0, 1680 bytes (Ghidra VA 0x13ACFC). It equals TACP file 0x302FEC (3157996), checked byte for byte in Python.
- First bytes of the table are 06 00 07 00 04 00 | 06 00 07 00 03 00 | 06 00 07 00 03 00 | 08 00 09 00 03 00.
- Raw listings: validate/r_v_takeover_9_0_call1.txt (b46c4, aec70, aedb0, ad5ac) and call2.txt (ad93c callers; ptr scans empty for b46c4, aec70 and aedb0). Callers of b46c4 are VA 0xAED66 and 0xAEE4A.
- ad93c is called from 8 sites in 6 functions (adffc, ae218, ae628, aebd8, aec70, aedb0). Only aec70 and aedb0 follow with b46c4, so b46c4 is specific to those two paths.
- Engine: extractor extract_agent_equipment.cpp:1130-1131 sets min_score = (i==0 ? INT_MIN : i+1) and max_score = (i==11 ? INT_MAX : i+2). aequipmenttype.h:263 isAppropriate is score >= min && score < max. extract_organisations.cpp:50 sets tech_level = start+1.
- HEAD battlemap.cpp:340-382 and 1220 and agent.cpp:171-175 are identical between LIVE and HEAD. organisation.cpp/.h and aequipmenttype.cpp/.h are identical too.

**Fix (target HEAD).** One-line change, no new RE. In agent.cpp (the human branch at line 171-175), compute `int tier = org->tech_level + (org->takenOver ? 3 : 0);` and pass it to EquipmentSet::getForType(state, Human, tier). No clamp is needed, because HUMAN_12 has max_score INT_MAX and absorbs any score of 12 or more, matching the EXE clamp to tier 11. Add a regression test showing that a taken-over org with tech_level 4 draws from HUMAN_7 instead of HUMAN_4.

Optional strictness: the EXE gives +3 only to raid-guard and org-attacker units. To avoid over-arming map guardians (battlemap.cpp:1220) or spawnUnit units, add a bool parameter to createAgent and set it only from the otherParticipants loop at battlemap.cpp:382. Whether guardians get +3 in the EXE is unverified.

Separate finding, not T10: the EXE rolls rand16(9) over all 10 rows per category, so a guard gets grenades or gear with probability n_nonempty/10. The engine picks only among non-empty rows, because the extractor drops zero rows (extract_agent_equipment.cpp:1073-1105), so it always gives some. This assumes rand16(9) is inclusive over [0,9]; I did not re-verify that.

## v_relations / F05-vehicle-downed -> diverges (high, high)

**Disputed point.** Whether the EXE charges -20 (crash threshold) for X-COM downing a Government/Megapol vehicle, and whether the engine can charge such a vehicle twice. The validator said -20 or -10, with 6 downed Gov craft in the EXE versus 4 in the engine. The engine refuter said -10 only and 12 versus 4, and that the double charge is unreachable. The EXE refuter left the claim unchanged.

**Ruling.** The engine-lens refuter is right on the disputed numbers, and the divergence still stands. FUN_00057c8c charges the -20 only when health after a non-lethal hit is below the vehicle type's crash-constitution word. I read that table straight from canonical UFO2P.EXE (CRC 0x4749ffc1): types 0-9, the UFOs, are nonzero (15, 35, 150, 150, 200, 200, 100, 80, 300, 350), and every type 10-33 is 0. That covers police cars, Megapol, space liner, Marsec, civilian and X-COM craft. A non-lethal hit leaves health of at least 1, so for a Gov or Megapol craft the -20 gate (health below 0) can never be true. The -20 is therefore a UFO-only rule. A Government or Megapol craft costs exactly one relation charge: when X-COM's projectile frees the slot, FUN_00054a28 calls prim(victim org, firer org, -10). The reverse +2 is skipped when the firer is X-COM, so only the -10 applies. The validator's "-20 crash" rows and the "6 downed Gov craft" figure are wrong for non-UFO victims. The right figure is -10 each, about 12 kills to reach the -116 funding cutoff, versus 4 in the engine. The EXE refuter's refinement (1) is also wrong for non-UFO craft, because there is no crash-only sequence for them. The double charge is unreachable with default data. Non-UFO crash_health is 0, so applyDamage's crash()/startFalling-with-attacker branch needs health <= 0, which already goes to die(). A UFO that crashes is skipped by die()'s !crashed test. The optional crashing_vehicles mod would make it reachable, but it is not enabled by default. Two further points. First, the sign flips when the victim org is already hostile to X-COM: the engine charges -5 against the EXE's -10, so the engine is milder there. Second, UFO victims also diverge: the EXE charges -20 at the crash threshold and -10 if later finished by a hit, while the engine charges -5 once (Aliens are always at -100, so hostile). That feeds the Government ripple divergence (about +2 in the EXE against +0.6 in the engine). The EXE refuter's refinements (2) X-COM-victim no-op and (3) score once-gate are valid and belong in the fix. Impact stays high because the engine charges 3x more per outright-downed Gov or Megapol craft, and the UFO-side divergence applies in every game. No log evidence was found for how often X-COM downs Gov craft in practice.

**Engine.** LIVE game/state/city/vehicle.cpp:2115-2140 (HEAD :2090-2115, byte-identical body) adjustRelationshipOnDowned does three things. (a) It books craftShotDownUFO or craftShotDownXCom score. (b) It calls owner->adjustRelationTo(attacker->owner, -5, ripple) if owner->isRelatedTo(attacker owner) == Hostile (relation < -50, organisation.cpp:1089). (c) Otherwise it calls adjustRelationTo(..., -30, ripple). Call sites: die() under `attacker && !crashed` (LIVE :2024-2026, HEAD :2001), crash() (LIVE :2043, HEAD :2018) and startFalling() (LIVE :2088, HEAD :2063). A non-null attacker comes only from applyDamage via handleCollision (LIVE :2692-2760 and :2813). The default extracted data has crash_health = 0 for all non-UFO types (extractor sets crash_health = v.crash_constitution, extract_vehicles.cpp:335; common_patch has no crash_health override). For Gov, Megapol and other non-UFO craft, the only reachable path is die(attacker) when health <= 0, giving one -30 (or -5 if already hostile) with ripple. For UFOs, crash() charges once (-5, because Aliens are hostile) and die() then skips it because the craft is crashed. Collision damage passes a null attacker and charges nothing. adjustRelationTo has no player-holder guard, so X-COM craft lost to alien fire still charge player->attacker -5 and ripple to third parties.

**EXE.** UFO2P.EXE ISO non-4 (CRC 0x4749ffc1). (a) FUN_00057c8c (physical 0xBA330, VA+0x626A4) charges -20 only when health after a non-lethal hit is below the crash-constitution word [type*0x7E + 0x128618] and the pre-hit health was not already below it (once per vehicle). It then calls prim(victim org, firer org, -20) if the projectile's firer vehicle is live (CALL at file 0xBA913, imm at 0xBA8FC). That word is nonzero only for UFO types 0-9, so the -20 applies to UFO victims (Aliens) only. (b) The lethal path (damage >= health) calls only FUN_000983ec (score, once-gated) and FUN_00058280 (frees the slot). It has no relation call. (c) FUN_00054a28 (physical file offsets 0xB8132-0xB815A) tests whether the victim slot was freed, the firer is live, and victim org != firer org. It then calls prim(victim org, firer org, -10), and prim(firer org, victim org, +2) only if the firer org is not X-COM. There is no hostility test. If the victim is X-COM, A == 0 and the primitive FUN_0005faf0 exits immediately (a no-op). Non-UFO craft never crash and are only destroyed outright: -10 for Gov or Megapol, with the Gov ripple of about -3 for a Megapol craft. UFOs get -20 at the crash threshold and another -10 if finished by a hit.

**Evidence.** I confirmed all of the following myself. Crash-constitution table read from canonical/UFO2P.EXE (CRC 0x4749ffc1): vehicle_data at file 0x189C8C, stride 0x7E, field +0x30 (word at VA 0x128618 + type*0x7E, file 0x189CBC + type*0x7E). Types 0-9 give 15, 35, 150, 150, 200, 200, 100, 80, 300, 350. Types 10-33 are all 0x0000. Bytes at the gate: file 0xBA8A8 = 66 3b 04 5d 18 86 05 00 7d (CMP AX,[EBX*2+0x58618]; JGE, the raw operand equals VA 0x128618 - 0xD0000). File 0xBA859 holds the once-gate load, and the 0x57ea0 `CMP AX,SI; JL` lethal-versus-non-lethal split is in the listing. The call-site bytes are at 0xBA8FC (bb ec ff ff ff = -20), 0xBA913 (call), 0xB8132 (bb f6 ff ff ff = -10), 0xB8141 (call), 0xB8151 (bb 02 00 00 00 = +2) and 0xB815A (call). The listing is og-research:export/strategy-plan-2026-10-08/validate/r/r41_full.txt. FUN_00057c8c lethal path 0x57ea9-0x57fb3 has no prim call. FUN_00054a28 0x55a44-0x55ab6 shows CALL 57c8c, slot-freed test, firer-live test, org-differ test, -10, skip +2 when the firer org is 0, then +2. The primitive FUN_0005faf0 (r41_prim.txt) returns at once when SI == 0 or SI == DI. Engine side: LIVE vehicle.cpp:2024-2026, 2043, 2088, 2115-2140, 2692-2760 and organisation.cpp:1020-1076, and HEAD vehicle.cpp:2001, 2018, 2063, 2090-2115 (diff of the function body shows identical). The mod data/mods/crashing_vehicles is the only source of non-UFO crash_health and is a separate opt-in mod (CMakeLists.txt:135), not on by default.

**Fix (target both).** Rework adjustRelationshipOnDowned and its call sites in vehicle.cpp. (1) Remove the Hostile -5 / else -30 split. (2) For an outright kill by a hit (die with an attacker and a live attacker vehicle), when victim org != attacker org and victim org != player: owner->adjustRelationTo(attacker org, -10, ripple). If attacker org != player, also attacker org->adjustRelationTo(owner, +2, ripple). Do not skip when the craft is already crashed, so a hulk finished by a hit gets this charge as well. (3) In crash() (UFO victims only, since crash_health > 0), on first crossing strictly below crash_health (the EXE test is `<`, the engine uses `<=`), charge owner->adjustRelationTo(attacker org, -20, ripple) once per vehicle. Leave startFalling() with no relation charge, since it is unreachable by default. (4) Skip the relation adjustment entirely when the victim's owner is the player, because the EXE prim is a no-op for A == X-COM and the engine currently ripples third-party views of the attacker org. (5) Keep the score booking (craftShotDownUFO or craftShotDownXCom) behind its own once-per-vehicle flag, separate from the relation charge. The EXE gates score at +0x272, and dropping the `!crashed` gate would double-book it. (6) Add a test: a Gov craft killed in one hit drops Gov->X-COM by 10, not 30, and a Megapol craft gives about -10 with a Gov ripple of about -3. A UFO crossing its crash threshold gives -20 on Aliens->X-COM, and the finishing hit adds -10.

## v_relations / F08-empty-building-investigation -> diverges (low, high)

**Disputed point.** Whether F08 (Investigate Building on a building with no aliens) is a "match" or a divergence. The validator said match (impact medium) but its own text lists "no ripple" for the engine versus "with ripple" for the EXE. The engine-lens refuter said diverges (low) on three grounds: (1) the EXE ripples and the engine does not; (2) the EXE launch gate (FUN_000b300c) ignores population indices 0 and 4, while the engine's foundAlien counts all crew; (3) the engine's alert/arrival route has no crew check and no relation cost, with the EXE alert route unresolved.

**Ruling.** The refuter is right: the verdict is diverges, low. The "match" label contradicts the validator's own record (finding text says "with ripple" for the EXE and "no ripple" for the engine; write-up line 287 says "diverges only on the missing ripple"; effect table line 225 says "ripple missing"). The core mechanic is not in dispute and stays: delta -(5+difficulty) (-9 at Superhuman), human city only, penalty only when all 13 population bytes are zero, no battle for a truly empty building, and the three message strings. That is still a medium-impact strategy rule: the plan should keep "don't investigate buildings without confirmed crew". Two real divergences remain.

(1) Ripple. I disassembled FUN_000b426c: it acts only for kind 0 and [0xD5060]==0, then calls FUN_0005faf0 at VA 0xB42E2. The primitive's ripple loop runs under `cmp [0xD5060],0; jne skip` at VA 0x5FB76, so every EXE hit ripples to every org's view of X-COM. The engine call passes three arguments, so applyReputationRipple takes its default of false (organisation.h:225 at HEAD, :224 in LIVE).

(2) Launch gate. This one is real, but the Egg/Chrysalis meaning of indices 0 and 4 is INFERRED, though strongly corroborated. For kind 0 in the human city, FUN_000b300c skips indices 0 and 4, and FUN_000ac08c needs both FUN_000b2e60 and FUN_000b300c non-zero before calling FUN_000ac348. FUN_000b2e60 is a "player vehicle at the building" scan, not an alien test. The penalty gate in FUN_000b426c tests all 13 bytes. So an EXE building holding only index 0 or 4 gets no penalty and no battle, and the handler shows the "less favorably" string because the tier did not change. The engine's foundAlien starts a battle in that case. The index meaning is corroborated by FUN_000ae218 zeroing indices 0, 4 and 0xB for UFO crews (VA 0xAE3EF-0xAE400), by the extractor's 13-type order (0 Egg, 4 Chrysalis, 11 Queenspawn), and by the "Stationary aliens" grouping.

(3) Alert/arrival route. My scan of all E8 callers of FUN_000ac08c resolves most of this. Callers are: kind 5 at VA 0x3AE76, 0x3B079 and 0x99E2D; kind 1 at 0x3B266; kind 2 at 0x60E58; kind 0 at 0x71503 (Investigate button); kind 0 or 3 at 0x7186B (Raid button). The kind-2 caller at 0x60E58 is the vehicle-arrival handler: it calls FUN_000b300c with kind 2, shows a prompt if hostile forces are found, then calls FUN_000ac08c with kind 2. Otherwise it calls FUN_00073080. FUN_000b426c is a no-op for kind 2. So the EXE arrival route pays no relation cost, which matches the engine's CommenceInvestigation route (no cost; gated by building->detected). The engine's -(5+d) therefore applies only through the BuildingScreen Investigate button, as in the EXE. The comments at alertscreen.cpp:40-44 and gamestateintrospect.cpp:~978 saying that answering an alert costs -5-difficulty are stale. They also cite wrong lines (buildingscreen.cpp:154-166).

Impact is low because only the size of the divergence is small. The Government -3 for a Megapol-owned building is computed (Gov->Megapol 75 -> af=37, trunc(37*-9/100) = -3), not read from a save. The egg-only case is rare.

**Engine.** LIVE game/ui/city/buildingscreen.cpp:167-219 (HEAD :228-281; identical text). BUTTON_EXTERMINATE with owner != aliens: foundAlien = any current_crew entry > 0 (LIVE :170-178; HEAD :231-240). If none, and the owner is not the player, adjustRelationTo(*state, player, -5 - state->difficulty) with three arguments (LIVE :186; HEAD :247), so applyReputationRipple defaults to false (organisation.h:224/225). No ripple to other orgs; only the owner's view of X-COM moves, plus the X-COM mirror. Message variants: unfriendly/hostile if the tier changed, else "less favorably". If any crew > 0, including Egg and Chrysalis, a battle launches. The alert/arrival route (AlertScreen -> CommenceInvestigation -> CityView::initiateBuildingMission, cityview.cpp:4857-4915 and :4049-4058) has no relation cost; it is gated only by building->detected.

**EXE.** UFO2P.EXE ISO non-4. The Investigate Building handler (VA 0x713E4) calls FUN_000ac08c with kind 0 at VA 0x71503. FUN_000ac08c calls FUN_000b426c (kind 0, 2 or 4) at VA 0xAC2A1. That function acts only when kind==0, [0xD5060]==0 and all 13 population bytes at building+0xD4 are zero. It then calls the primitive FUN_0005faf0 at VA 0xB42E2 with (owner, X-COM, -(5+word[0xE0C9C])), which is -9 at Superhuman if the word is 4 (INFERRED). The primitive ripples every time: `cmp [0xD5060],0; jne skip` at VA 0x5FB76, k=1..26 except the owner and X-COM, each m[k][X-COM] += trunc(af*delta/100) with af=m[k][owner]/2 (halved again if negative), skipped if the result reaches +/-100. For a Megapol-owned building the Government's view of X-COM moves about -3 (computed from the template, not read from a save). The battle launch needs both FUN_000b2e60 (a player-vehicle-on-site scan) and FUN_000b300c non-zero (VA 0xAC2A6-0xAC2B2). For kind 0 in the human city, FUN_000b300c's loop (VA 0xB30ED) skips population indices 0 and 4 (Egg and Chrysalis, inferred) and returns 1 only if another index is non-zero. A building holding only index 0 or 4 gets no penalty, no battle, and the "less favorably" string. The arrival route is kind 2 (call at VA 0x60E58); FUN_000b426c does nothing there. The strings are group 0x1D items 0x1F, 0x20 and 0x21, under the title "No Hostile Forces Discovered".

**Evidence.** I disassembled the canonical UFO2P.EXE with capstone; physical file offset = Ghidra VA + 0x626A4 for code. No Ghidra copy was needed.
- FUN_000b426c: VA 0xB426C, file 0x116910. Kind test `test ecx,ecx; jne` at 0xB428B. City test `cmp byte [0x5060],0; jne` at 0xB428F. 13-byte loop at 0xB429A-0xB42BD. Delta build at 0xB42C3-0xB42D3. CALL 0x5FAF0 at 0xB42E2, file 0x116986.
- FUN_0005faf0: ripple gate `803d6050000000 0f85` at VA 0x5FB76, file 0xC221A. Loop k=0..0x1A with idiv 100 at 0x5FC09.
- FUN_000b300c: VA 0xB300C. Kind-0 human-city path reaches the loop at 0xB30ED, which skips idx 0 (`test eax,eax; je`) and idx 4 (`cmp eax,4; je`) and checks byte [edx+0xD4] for the rest.
- FUN_000ac08c: VA 0xAC08C. Calls to b2e60 and b300c store to [esp+0x5C] and [esp+0x58]. b426c is called for kinds 0, 2 and 4 at 0xAC2A1. The gate `837c245c00 74.. 837c245800 74..` at 0xAC2A6-0xAC2B2 precedes CALL 0xAC348 at 0xAC2F7.
- Callers of ac08c (E8 scan over the code object): 0x3AE76, 0x3B079, 0x99E2D (kind 5); 0x3B266 (kind 1); 0x60E58 (kind 2, vehicle arrival); 0x71503 (kind 0, Investigate); 0x7186B (kind 0 or 3, Raid).
- FUN_000ae218: zeroes indices 0, 4 and 0xB at VA 0xAE3EF-0xAE400 for kind 5 (UFO crew).
- Extractor: extract_agent_types.cpp maps Egg=0, Brainsucker=1, Multiworm=2, Hyperworm=3, Chrysalis=4, Anthropod=5, Skeletoid=6, Spitter=7, Popper=8, Megaspawn=9, Psimorph=10, Queenspawn=11, Micronoid=12.
- Engine: LIVE buildingscreen.cpp:167-219 and HEAD :228-281 are identical text. adjustRelationTo is identical in both trees. organisation.h:224/225 gives the default false.
Notes are in validate/adj_v_relations_F08-empty-building-investigation.md.

**Fix (target both).** 1) Pass applyReputationRipple=true at LIVE buildingscreen.cpp:186 / HEAD :247. This is necessary but not exact parity. F01 still applies: the EXE ripple is integer-truncated and human-city-gated (so it is 0 when |af*9| < 100), while the engine ripple is float and includes Civilians. 2) Optional, mark INFERRED: split the BuildingScreen gate. Charge the penalty only when no crew entry is > 0 (all 13 bytes zero, EXE-faithful). Start the battle for the Investigate button only if some crew entry other than AGENTTYPE_MULTIWORM_EGG or AGENTTYPE_CHRYSALIS is > 0. Otherwise show "No Hostile Forces Discovered" with the "less favorably" string and no penalty. Do not apply this to the Raid button (kind 3 always launches in the EXE). 3) Fix the stale comments at alertscreen.cpp:40-44 and gamestateintrospect.cpp:~978: the -5-difficulty cost applies only to the BuildingScreen Investigate button, not to alert-arrival investigations (EXE kind 2 pays no relation), and the cited lines buildingscreen.cpp:154-166 are wrong.

## v_relations / F14-no-periodic-drift -> match (high, high)

**Disputed point.** The verdict itself was not in dispute: both refuters voted refuted=true but verdict_should_be unchanged/match. Two supporting claims were disputed. (a) Whether the 'complete matrix-writer census' was complete. (b) Whether the strategy rule 'recovery only through rift offers, alien kills, UFO kills or the +25 accept path' is true.

**Ruling.** Verdict 'match' stands. Census: incomplete, but no missed writer is periodic. Strategy rule: wrong, replace it. Neither engine nor EXE has a periodic or mean-reverting writer of the relation matrix.

Engine side (I checked): LIVE and HEAD organisation.cpp are byte-identical. Organisation::updateRelations (organisation.cpp:694-707) only writes previous_relations. It is called daily at HEAD gamestate.cpp:1553 and LIVE gamestate.cpp:1586. Every current_relations writer is an event site, listed in my notes.

EXE side: my own capstone scan of UFO2P.EXE gives 933 candidate dwords, the same count as refuter 1. The writer set is the validator's list plus FUN_00097234 (the on-demand bribe, file 0xF98D8) and FUN_000978c0 (the alliance request, file 0xF9F64).
- Both missed writers store to the matrix only after a player click. The bribe's button compare is at file 0xF9E4A, with the write at 0xF9E80. The alliance request's accept test is at 0xFA109, with the writes at 0xFA119. They are not drift. The alliance request is called every frame from the main loop (file 0x72EEB), but its write is behind the accept return.
- The hourly rift offer FUN_00096bc8 writes m only after the accept-button compare at file 0xF9774. Its always-executed store at file 0xF98B8 refreshes the shadowB baseline, not m.
- TACP.EXE writers are the debug cycler, FUN_0005D25C (the -50 store at file 0xB8B3F), FUN_00060720 (kills) and load/save. The apparent write at file 0x7E1E9 is a misaligned decode of a read.

Why confidence is high: a drift writer would have to be one of the classified writer functions. Every one is event-, threshold- or click-gated, and none writes unconditionally on a timer.

Not re-derived by me: the pruned periodic call-graph BFS (both refuters agree on it), the gating of FUN_00092470 (incidents) and FUN_0006FD9C (takeover), and the bribe cost formula. My own unpruned call-graph closure explodes through the dialog engine and is not evidence either way.

final_impact stays 'high'. Nobody disputed it. It is the strategic weight of the rule, not an engine defect, and no engine change follows.

Corrected rule: a Government relation lost to collateral damage or kills never heals passively, in either the EXE or the engine. Recovery paths:
1. Accept the hourly rift offer when relation is below 25 and has fallen more than 15 against the per-org baseline. The relation goes to 0 if negative, otherwise +25.
2. Buy it back on demand through Diplomacy 'Offer settlement' (FUN_00097234). It is refused if the org is alien-taken-over, alien-allied (relation 75 or more) or already allied.
3. Accept an alliance request (relation goes to 100). It is offered only when relation is already 25..74 and has risen more than 15 over baseline, or X-COM and the org share more than 2 common enemies.
4. Event rewards, such as the ripple from alien and UFO kills.

Follow-ons for other findings, not F14 fixes:
- F17 (on-demand bribe) should change from exe_unbound to bound at FUN_00097234. The engine has the bribe at bribescreen.cpp:30,136 and organisation.cpp:1129.
- The alliance request is engine_missing in both trees. OrganisationRequestAlliance is enum-only (gameeventtypes.h:64), and signTreatyWith's only caller passes forceAlliance=false (diplomatictreatyscreen.cpp:65).
- Possible quirk for that follow-on: the decline branch of FUN_000978c0 changes no state, so the dialog may re-prompt every frame.

**Engine.** Organisation::updateRelations (HEAD and LIVE organisation.cpp:694-707) copies current_relations into previous_relations once a day and returns the player's delta. It never writes current_relations. Callers: HEAD gamestate.cpp:1553, LIVE gamestate.cpp:1586, inside the end-of-day loop. current_relations is written only by event code: adjustRelationTo (organisation.cpp:1024 with ripple at :1054), the X-COM/alien mirror (:1065-1074), establishRelation (:1081), post-battle sync (battle.cpp:3187-3207), battlemap.cpp:1543 and new-game init (gamestate.cpp:659). No periodic drift or recovery exists.

**EXE.** UFO2P.EXE and TACP.EXE (ISO non-4) have no hourly, daily or weekly mean-reversion writer of the 28x28 relation matrix (UFO2P VA 0x16EC28, TACP VA 0x1D3428). Relations move only on events: hits and kills via FUN_0005faf0, incidents, takeover, mission launch, event dispatch and the post-battle load. They also move on three player-clicked paths: the hourly rift offer (FUN_00096bc8), the on-demand bribe (FUN_00097234) and the alliance request (FUN_000978c0). FUN_00096bc8 refreshes only a shadow baseline hourly (one org per call, 27-hour cycle), not the matrix. FUN_00097234 and FUN_000978c0 are the two writers missing from the validator's census.

**Evidence.** Binaries: UFO2P.EXE ISO non-4 (CRC 0x4749ffc1), code file = VA + 0x626A4. TACP.EXE ISO non-4 (CRC 0xfebbe39e), code file = VA + 0x5AAA4. All offsets are physical file offsets; the Python+capstone scan found 933 candidate dwords.

Verified by me:
- Raw E8/E9 scan: 17 callers of FUN_0005faf0. FUN_00097234 is reached only by the E9 at file 0xB2B0E (thunk VA 0x50450, whose pointer is pushed at VA 0x53568). FUN_000978c0 is reached only by the E8 at file 0x72EEB. FUN_00096bc8 is called by the hourly handler at file 0xAD28B.
- FUN_00096bc8: accept compare at file 0xF9774 (m written only after it); baseline store (shadowB[org] = m) at 0xF98B8.
- FUN_00097234 (bribe): bytes at the entry 0xF98D8 are 53515256575583ec68. Button compare at file 0xF9E4A; matrix write at 0xF9E80; m[0][org] raised at 0xF9EA5.
- FUN_000978c0 (alliance request): accept test at file 0xFA109; writes of 100 at 0xFA119 onward.
- Other matrix-address hits: matrix-to-shadow copies in FUN_00014854, a post-battle load at VA 0xB1DEE, a save at VA 0xAC333, a read at VA 0x15894. None writes m on a timer.
- TACP: the write at file 0x7E1E9 is a misdecoded read.
- Engine: LIVE and HEAD organisation.cpp identical; the writer list is in the engine behaviour above.

Inherited from the refuters and sibling findings: the pruned periodic call-graph BFS (it reaches only 0x96BC8, 0x92470, 0x6FD9C and 0x99E04>0xAC08C>{0xAC348, 0xB426C}), the gating of FUN_00092470 and FUN_0006FD9C, and the bribe cost formula. Notes: og-research:export/strategy-plan-2026-10-08/validate/adj_v_relations_F14-no-periodic-drift.md. No Ghidra copy was created, so nothing to delete.

**Fix (target none).** None for F14: the engine already matches. Documentation fixes only. First, correct the finding's exe_behaviour and exe_evidence text. Drop the 'complete census' claim and add FUN_00097234 (file 0xF98D8) and FUN_000978c0 (file 0xF9F64) as player-clicked writers. Second, replace the strategy_rule_affected text with the four-path rule in the ruling. For other findings: change F17 from exe_unbound to bound (FUN_00097234; compare the engine's costOfBribeBy step quantisation and balance clamp against it). Open a new engine_missing finding for the alliance request, covering the unraised OrganisationRequestAlliance event and the 25..74 trigger that lifts the relation to 100.

## v_gov_income / G2-cutoff-latch -> match (high, high)

**Disputed point.** Whether the G2-cutoff-latch finding stands. Both refuters accept the mechanic and the LIVE port. The exe lens marked it refuted only because the strategy_rule_affected sentence ("one bad week cannot end funding; two consecutive bad weeks ... end it") is wrong. The engine lens kept the finding as unchanged but corrected the same sentence to a cumulative test.

**Ruling.** The finding stands with verdict match, but strategy_rule_affected must be replaced, and both refuters are right about that. I read the UFO2P.EXE bytes directly (capstone, ISO non-4) and did not need Ghidra. In the cutoff block the "H" register (ECX) is the sum of the eight second dwords of the score entries ([i*8+0x10C60]). ECX is not written between the sum loop at 0xF6EC5 and CMP ECX,0xFFFFF6A0 at 0xF6FBF. The current-week sum W (EBX) is only used for the tier ladder. A full scan of the code region for memory operands in 0x10C58-0x10CA8 finds only these writers of the score array: the generic add at 0xFAA85 (first dword), the category-3 and category-4 stores at 0xFAAE1 and 0xFAB15 (first dwords), and the rollover at 0xFA249-0xFA263. The rollover does col1 += col0, then col0 = 0, and it is the only writer of col1. On both the day-change path (funding call at 0xAD496, rollover at 0xAD4B8) and the time-skip path, funding runs before the rollover. So H at the Monday-n assessment is the cumulative total of weeks 1..n-1, and the week just ending never enters the -2400 test. "Two consecutive bad weeks" is neither necessary nor sufficient. "One bad week cannot end funding" is false: one week below -2400 cuts funding the next Monday, and nothing scored in between can prevent it. Corrected rule: the score leg cuts funding permanently at a Monday if the lifetime total of all completed weeks is below -2400 (strict), so keep it at or above -2400 at every Monday midnight. The week being assessed only sets the tier adjustment, at most -F/4. The relation leg is tested on the current signed byte of the Government-to-X-COM relation, and it cuts funding if that is below -50. A week-1 cut can only come from the relation leg because H is 0 then. Counterexamples from the rollover bytes: -3000 then +5000 cuts at Monday 2; -1300 twice first cuts at Monday 3; five weeks of -500 cut at Monday 6; +3000 then -5000 gives H = -2000 at Monday 3 and no cut. LIVE implements all of this. HEAD does not: it tests the lifetime total including the current week (D1), and the UI re-tests it.

**Engine.** LIVE (gamestate.cpp:1770-1801, 1836) computes previousWeeks = totalScore.getTotal() - weekScore.getTotal() at :1773. Every score writer updates both totals in lockstep (battle.cpp:3211-12 and 3585-86; scenery.cpp:1193-94; vehicle.cpp:2120-27; vehiclemission.cpp:1402-03; cityview.cpp:4847-48 and 4924-25). The only exception is the skirmish-only restore at battle.cpp:3192 and skirmish.cpp:80-81, 135-136. getTotal sums all 8 categories (:2163). The old income is credited first unless the balance is 2e9 or more (:1787-1790). The cut test is `hostile || previousWeeks < -2400` (:1793), where hostile is government->isRelatedTo(player) == Hostile, i.e. the Government-held float relation x < -50 (organisation.cpp:1089). On a cut the credit is undone, income is 0, and fundingTerminated = true (:1796-1798). Nothing else writes fundingTerminated, it is serialized, and weekScore.reset() runs after the test (:1836). LIVE's weeklyfundingscreen has no re-test. HEAD (fda36d6c base) tests `totalScore.getTotal() < -2400` at HEAD:game/state/gamestate.cpp:1737-1738, which includes the current week. It re-tests it at HEAD:game/ui/city/weeklyfundingscreen.cpp:61 and resets weekScore in the UI at :120.

**EXE.** UFO2P.EXE ISO non-4, funding routine FUN_000941dc at file 0xF6880. If the latch word [raw 0x10CA0] is non-zero, jump to 0xF70BD and skip the whole assessment (cmp word [0x10ca0],0 at 0xF6E6E). Otherwise: the relation byte is the high byte of the dword at raw 0x9EC5D, i.e. matrix[2][0] with base raw 0x9EC28 and stride 0x1C, read with sar eax,24 and tested `cmp eax,-0x32; jge` at 0xF6E93-0xF6E9F, which flags a cut if the signed byte is below -50. The loop at 0xF6EAB-0xF6EC5 accumulates W (EBX) from [i*8+0x10C5C] and H (ECX) from [i*8+0x10C60] for i = 0..7. The tier ladder at 0xF6EC7-0xF6FBD uses only EBX. `cmp ecx,0xFFFFF6A0; jge` at 0xF6FBF flags a cut if H < -2400, with H alone and never H+W. Either flag leads to 0xF7088. On a cut: balance -= old income (sub [eax+8],edx at 0xF7090, unconditional), income := 0 (0xF709B), latch := 1 (mov word [0x10ca0],cx at 0xF70A2), and X-COM +0xE2 := [esi+8] at 0xF70A9. Then the Government is debited the new income, which is 0. The rollover FUN_00097b90 at file 0xFA234 is gated on weekday word [0x4D64]==0 and does col1 += col0, col0 = 0. It runs after funding at 0xAD4B8, and on the time-skip path funding at 0xAD2D3 comes before the rollover at 0x7427C. The latch has no clearing writer visible in the code (stores at 0xF70A2 only); pointer-immediate save/load block I/O at 0x106492 and 0x1072AB is not excluded.

**Evidence.** UFO2P.EXE ISO non-4, physical file offsets (code file = VA + 0x626A4, data file = VA + 0x616A4). Latch word: raw operand 0x10CA0 = Ghidra VA 0xE0CA0 = file 0x142344.
- 0xF6E6E: `66 83 3d a0 0c 01 00 00; 0f 85` (latch guard).
- 0xF6E82-0xF6E9F: `a1 5d ec 09 00`, `c1 f8 18`, `83 f8 ce 7d 08` (relation byte < -50).
- 0xF6EAF-0xF6EC5: H/W sum loop.
- 0xF6FBF: `81 f9 a0 f6 ff ff 7d 08` (H < -2400). I scanned 0xF6EC7-0xF6FBF and found no write to ECX; the only register-writing/clobbering instructions there are the `idiv ebp` tier divisions.
- 0xF7088-0xF70A9: cut block.
- 0xFA23A-0xFA26E: rollover.
- 0xAD478-0xAD4B8: weekday-0 path, funding before rollover.
- Operand scan over 0x72000-0x131000: reads of the score array at 0xC3642, 0xC3907, 0xFA8D4, 0x10FA6E and 0x112960 all sum col0 + col1, so the two columns partition the lifetime total.
- Latch references: 0xF6E6E (read) and 0xF70A2 (write) only.
- Control: -2400 found at 0xF6FC1.

Engine: LIVE snapshot (SNAPSHOT_INFO HEAD fda36d6c) game/state/gamestate.cpp:1765-1836 and game/state/gamestateintrospect.cpp:281-294, whose own comment calls the cutoff a countdown. HEAD:game/state/gamestate.cpp:1735-1741 and HEAD:game/ui/city/weeklyfundingscreen.cpp:61 and :120.

Read-only check of the live worktree (khallmark/develop-reboot): the funding rework with previousWeeks is now committed as 879d0d73 ("City simulation: UFO2P funding, base defences and traffic; cheaper ticks"), an ancestor of that branch's current tip bce89743. That commit is not on the branch under test (HEAD).

No Ghidra copy was created, so nothing to delete.

**Fix (target HEAD).** Engine: no change to the LIVE logic. HEAD should take the funding hunk of 879d0d73 (khallmark/develop-reboot), not the whole cherry-pick, since that commit is large and mixed. The hunk is: previousWeeks = totalScore.getTotal() - weekScore.getTotal(); test `hostile || previousWeeks < -2400` after crediting the old income; on a cut, undo the credit, set income to 0 and fundingTerminated = true; move weekScore.reset() into GameState after the test; drop the re-test at HEAD:game/ui/city/weeklyfundingscreen.cpp:61 and the UI reset at :120. Separate plan or doc correction (not an engine fix): replace strategy_rule_affected with the cumulative, one-Monday-lag rule from the ruling. Fidelity-only leftovers, not verdict-changing: LIVE tests a float relation x < -50 while the EXE tests a signed byte (G1c); the cut-path store to X-COM +0xE2 at 0xF70A9 is not modelled and is probably display-only; the EXE skips funding and rollover while byte [0x5060] is non-zero (the alien dimension), which the engine does not model.

## v_base_defence / BD02 -> diverges (medium, high)

**Disputed point.** Both lenses agree on the EXE facts for roles 2 and 3. They differ on whether the engine's Overspawn routing widens the set of UFOs that start a base defence, and so whether the verdict "match" can stand. The engine lens flagged this in its caveat 3 but did not trace the role-5 handler. The EXE lens traced it and said "diverges".

**Ruling.** Diverges, narrowly. The refuter's correction stands. The Infiltration half (role 3: reason 5, no exposure read, no deposit, no roll) and the Subversion half (role 2: exposure-gated) are correct and match the engine. The trigger set does not match: the engine starts base defences from Overspawn UFOs and the EXE never does.

I verified this myself. In FUN_0003a910 the role-5 branch (VA 0x3AEDE-0x3AFC8) calls only FUN_000635f0 (doodad), FUN_00070cc0 (spawn a vehicle), FUN_00070a2c, FUN_0003b4b8, FUN_00058280 and FUN_00059148. It never calls FUN_000ac08c, FUN_000705f8 or FUN_0005fddc. An E8 hit at VA 0x3AF45 in a raw scan is a false positive (SUB EAX,3).

The deferred reason-5 caller FUN_00099e04 is set only by the organisation-incident function FUN_00092470, not by anything in the UFO crew path. So Overspawn has no route to a base defence.

Overspawn craft get role 5 because the role word on UFO_mission_data records 40-44 is 10, and FUN_0005df1c maps role-table row 10 to +0x15C=5. The role-5 handler spawns a type-0x21 vehicle with role 4 instead of landing crew. The attack slot on O3 (role word 5) maps to +0x15C=1, the ordinary Attack handler.

The engine routes both the Overspawn primary and the Overspawn attackList through infiltrateOrSubvertBuilding(subvert=false). It can pick the X-COM base, deposit crew, roll 5% exposure, and then push DefendTheBase or call Base::die. The code is identical in HEAD and LIVE.

Impact is medium, not high. The strategy rule players act on is unchanged: Infiltration ignores exposure, and exposure gates only Subversion landing and targeting. The fix removes an extra base-defence source. Overspawn takes 1 of 10 weekly slots in week 7, 2 in weeks 8-14, and 3 from week 15 on. That is mostly after the benchmark window, but material in long games.

Confidence is high on the facts. The "diverges" label is scoped to the Overspawn routing, so the Infiltration and Subversion trigger code needs no change. The other BD findings (BD03, BD05, BD15) are unaffected.

**Engine.** Both HEAD and LIVE send Overspawn UFOs through the Infiltration landing, and the validator's scope (crew and DefendTheBase push) holds as read. In GameState::startIncursion, HEAD gamestate.cpp:1068-1074 (LIVE 1069-1075) maps the Overspawn primary to VehicleMission::infiltrateOrSubvertBuilding(subvert=false). HEAD :1157-1161 (LIVE 1158-1162) maps the Overspawn attackList to the same call. The mission (InfiltrateSubvert case 1, HEAD vehiclemission.cpp:2611-2650, LIVE 2614-2665) picks any alive building within the distance limit (LIVE 3275-3297), deposits crew, rolls a 5% exposure, then calls Base::die if the base has no agents or pushes DefendTheBase. The Infiltration (subvert=false) and Subversion (known base only) branches match the EXE.

**EXE.** UFO2P non-4, FUN_0003a910 at file 0x9CFB4, branches on vehicle +0x15C.
- Role 3 (Infiltration): VA 0x3AE2D-0x3AEDE. If FUN_000705f8(target) is not -1, it sets the arrived flag, zeroes DAT_000e0cc0, and calls FUN_000ac08c with reason 5 (VA 0x3AE76, file 0x9D51A). There is no exposure read, no deposit and no roll.
- Role 2 (Subversion): VA 0x3AFC8-0x3B0A3. Reason 5 (VA 0x3B079, file 0x9D71D) fires only if FUN_000705a8 returns 1. Otherwise FUN_0007062c just sets the exposure word.
- Role 5 (Overspawn): VA 0x3AEDE-0x3AFC8 (file 0x9D582-0x9D66C). It plays the doodad, spawns a type-0x21 vehicle in a free slot via FUN_00070cc0, and sets that vehicle's +0x15C=4. It assigns a target with FUN_00070a2c and FUN_0003b4b8, then marks itself arrived. It makes no FUN_000ac08c, FUN_000705f8 or FUN_0005fddc call, so there is no deposit and no base defence.
- Role words 10 (primary) and 5 (attack slot) on UFO_mission_data records 40-44 become +0x15C=5 and 1 (Attack) via FUN_0005df1c.

**Evidence.** Binary: UFO2P.EXE non-4 (ISO generation). Code file offset = VA + 0x626A4; data file offset = VA + 0x616A4.

Bytes read directly:
- FUN_0003a910 prologue at file 0x9CFB4: 53515256575583ec.
- Role-5 compare at file 0x9D582: 6683bf5c010000050f85dc00.
- Reason-5 calls to FUN_000ac08c at file 0x9D51A (role 3) and 0x9D71D (role 2).
- Spawn call to FUN_00070cc0 at file 0x9D5FB. The +0x15C=4 store sits at VA 0x3AF7D, file 0x9D621.
- Raw E8 scan of VA 0x3AEDE-0x3AFC8 gives callees 635F0, 70CC0, 70A2C, 3B4B8, 58280 and 59148. The hit at 0x3AF45 is a false positive.
- Role table rows (word 0 of each row) at file 0x18EFF2 + role*0x94: row 5 gives 1, row 7 gives 3, row 8 gives 2, row 10 gives 5 (file 0x18F5BA reads 0500).
- UFO_mission_data role words at record +0xC. Records 40, 41, 43 and 44 read 0a00 0b00 0b00. Record 42 (O3) reads 0a00 0b00 0500 at file 0x13E4EC. The extractor puts role 5 in a non-Attack incursion into attackList (tools/extractors/extract_ufo_incursions.cpp:84-98), so O3 gets 2 bombers there.

Ghidra listings (copy adj_7, since deleted):
- FUN_0005df1c reads +0x166 and writes +0x15C from the table.
- FUN_0006da88 stores the role byte to +0x166 at VA 0x6DB98, then dispatches on +0x15C at VA 0x6DD1B-0x6DD77: 4 goes to 70A2C and 3B35C, 1 to 3B35C, 3 to 70140, 2 to 702E4, 5 to 70E6C.
- The role 1/4 handler at VA 0x3ADB2 makes no reason-5 call.

Reason-5 closure:
- The refuters' raw scan of FUN_000ac08c callers gives VA 0x3AE76, 3B079, 3B266 (reason 1, X-COM branch), 60E58, 71503, 7186B and 99E2D.
- The 99E2D caller (FUN_00099e04) is gated on flag VA 0x127798. A raw operand scan finds writers only at VA 0x926A7-0x92897 (the organisation-incident dispatcher FUN_00092470).

Mix: the weekly 10-slot row at file 0x155164 has Overspawn at 1 slot in week 7, 2 in weeks 8-14, 3 in weeks 15-19 and the default row.

Engine: HEAD:game/state/gamestate.cpp:1068-1074 and 1157-1161; LIVE:game/state/gamestate.cpp:1069-1075 and 1158-1162; HEAD:game/state/city/vehiclemission.cpp:2611-2650; LIVE:game/state/city/vehiclemission.cpp:2614-2665 and 3275-3297.

Notes: validate/adj_v_base_defence_BD02.md.

**Fix (target both).** Two edits in gamestate.cpp, identical in HEAD and LIVE. This is not blocked on RE.
(a) Overspawn primary (HEAD 1068-1074, LIVE 1069-1075). Stop calling infiltrateOrSubvertBuilding for Overspawn, so it never deposits crew, calls Base::die or pushes DefendTheBase. Interim: land and snooze with no deposit. A faithful version needs FUN_00070e6c and FUN_00070cc0 decoded, plus a type-0x21 craft with role 4 that is sent toward target buildings.
(b) Overspawn attackList (HEAD 1157-1161, LIVE 1158-1162). Delete the `if (missionType == Overspawn)` branch and always use attackBuilding, because the EXE gives these craft +0x15C=1 (Attack, role word 5).
Leave the Infiltration, Subversion and DefendTheBase code unchanged. Add a test that an Overspawn incursion over an X-COM base yields no DefendTheBase event and no deposited crew.

## v_base_defence / BD04 -> diverges (medium, high)

**Disputed point.** The roster divergence itself was not contested; the refuter disputed two parts of the validator's write-up. (a) The fix item "cap active stations at 3", which the validator presented as an EXE-validated rule. (b) The structured field blocked_on_re=false, although the validator's own text left the turret side open.

**Ruling.** Verdict stays "diverges" at medium impact. I upheld the refuter on both disputed items. The "cap active stations at 3" fix item is dropped, because the EXE has no station cap. The clamp to 12 is only an agent-budget reservation. The roster fix is not blocked on RE. The turret side is mostly resolved: the roster loads before the turrets, and the remaining gap is small and non-blocking.

I re-read the UFO2P listings and the TACP call structure, and found no station counter anywhere in the EXE. UFO2P map assembly loads the sector of every finished cell with no counter or compare. TACP spawns one turret for every map tile with feature byte 0xE6..0xED. Each spawn takes the first free slot in the 60-slot unit table, and a spawn is skipped when no slot is free.

The load order is settled by the call structure. The only caller of the slot-taking function FUN_000ab758 is FUN_000ab46c. FUN_00010294 calls FUN_000ab46c at VA 0x1069F, before unit placement FUN_000a60ec at 0x107BF and the turret spawn FUN_00024110 at 0x107D6. I did not read the body of FUN_000ab46c, so "this is the roster loader" is inferred from its call position. Roster units therefore take slots first, and turrets fill what is left.

UFO2P holds back min(12, 4 x finished stations) of the 60 slots (the turret count is added to its unit total, capped at 60), so at least 12 slots stay free for turrets. With 4 or more stations, extra turrets spawn only while slots remain. The practical limit is 60 minus the loaded units, not 3 stations.

Two further corrections. The engine roster block is battlemap.cpp:205-256, not 211-250. The 36 cap is soft: the `break` in the vehicle loop exits only the inner loop, and the soldier loop pushes before it checks the count, so the engine roster can exceed 36 with several docked craft.

The impact stays at medium because over-budget garrisons are the main effect, and they only matter when a base holds more than 24 present agents. The EXE scales each class by budget/total. A base with 10 soldiers and 40 staff would field about 4 soldiers, while the engine fields all 10 soldiers plus 26 staff. The class meaning (class 0 = combat, 1 to 3 = staff) is inferred, but OpenApoc's AgentType::Role order Soldier, Physicist, BioChemist, Engineer matches it.

**Engine.** LIVE = HEAD (battlemap.cpp is byte-identical in both trees). BattleMap::createBattle, battlemap.cpp:205-256, drafts in this order:
1. Every agent in each docked craft (currentVehicles' currentAgents), any role.
2. Every Soldier-role agent whose currentBuilding is the base building.
3. Non-soldier staff whose homeBuilding->base is the base, even if they are away. This includes newly hired or transferred staff still travelling, because Agent::hire and Agent::transfer set homeBuilding at once and the agent then walks there.
The cap is MAX_UNITS_PER_SIDE=36 (battle.h:28), and it is soft (see ruling). Soldiers are taken before staff, so there is no proportional scaling.

Turrets are created separately from sector guardianLocations (battlemap.cpp:1214-1221), appended to the same agents list. Every finished Security Station sector (1x1, 4 guardians each) adds 4 turrets, with no cap. Turrets are not charged against the 36, because fillSquads counts only allowsDirectControl agents (battlemap.cpp:1343). Unfinished facilities are skipped (battlemap.cpp:969-972). The EXE builds a construction sector for them instead.

Net: up to 36 agents plus 4 turrets per station.

**EXE.** UFO2P non-4, FUN_000ac7d0, called only by FUN_000ac348 at VA 0xAC3F4 (reason 5):
- Eligible agents have +0x7D != 0xFF, +0x21 == 0 and +0x7C == [0xD5060]. They are either at the base (+0x7D==1 and +0x7E == base id) or class-0 agents in a craft docked there (+0x7D==3, +0x23==0, craft base word == base id). Presence-based. Armament is never tested.
- Agents are counted into 4 classes by +0x23 (0..3). Class 0 is the combat class; classes 1 to 3 are inferred to be the three staff roles.
- Turrets T = FUN_000b42ec: +4 for each finished cell (+0x56==0) of type 0x17 or 0x18 (Security Station or Advanced Security Station). T is clamped to 12 (0xAC8C6-0xAC8D3) and used only as a reservation.
- Agent budget = 0x24 - T = 36 - min(12, 4 x stations). If the present total exceeds the budget, each class count becomes floor(count x budget / total) and the first n agents of each class in array order are taken.
- FUN_000acbb4 also stops at 36 X-COM units ([0x2440C0] >= 0x24) or 60 total units ([0x2440D4] >= 0x3C). The clamped T is added to [0x2440D4] at 0xACA4E-0xACA5D, so turrets use up total-unit slots.

Turrets themselves come from the TACP map, not from T. FUN_000b38b4 loads the sector of every finished cell with no station counter. TACP FUN_00024110 spawns one turret per feature byte 0xE6..0xED. Each spawn takes a slot in the 60-slot table via FUN_0001f830, after the roster is loaded, and is skipped if no slot is free. Unfinished cells map to construction sector 0x22 or 0x23.

**Evidence.** Binary: UFO2P.EXE non-4. File offset = VA + 0x626A4. Listings are from the refuter's dumps in scratchpad/validate/r_v_base_defence_4_0_work/q1.txt and q4.txt, read by me. Own TACP queries, copy adj_8 (since deleted), returned callers only.
- FUN_000ac7d0 at file 0x10EE74. Pass 1 filter and class jump table at 0xAC7FD-0xAC8B2. Call to FUN_000b42ec at 0xAC8BC. Clamp at 0xAC8C6-0xAC8D3. Budget `MOV EDX,0x24; SUB EDX,EBP`. Scaling at 0xAC8F0-0xAC935. Pass 2 at 0xAC94D-0xACA48 (calls FUN_000acbb4 with EDX=2). Turret add to [0x2440D4] at 0xACA4E-0xACA5D.
- FUN_000b42ec at file 0x116990. Grid walk: `CMP byte [EDX+0x56],0` then type byte `[EDX+0x16]` equal to 0x17 or 0x18, each adding `ADD EBX,4` (0xB4306-0xB4321).
- FUN_000acbb4 at file 0x10F258. Limits `CMP ECX,0x3C` and `CMP [0x2440C0],0x24`.
- FUN_000b38b4 at file 0x115F58. Per-cell loop 0xB3A30-0xB3AD2 has no station counter. Unfinished cells map to 0x22 or 0x23 at 0xB3A5E-0xB3A86.
- TACP.EXE non-4 (file = VA + 0x5AAA4). FUN_00024110 at file 0x7EBB4 (turret spawn: compares AH against 0xE6 and 0xED, calls FUN_0001f830 with EAX=0x15, EBX=1). FUN_0001f830 scans 60 slots (stride 0x49C from 0x109ED0). My allocator-caller query (Callers.java, TACP) shows the allocator's callers are FUN_00024110, FUN_000243c0, FUN_0006bfe0, FUN_0006e180 and FUN_000ab758. The first two, FUN_000243c0 and FUN_00024110, plus FUN_0006bfe0 and FUN_0006e180, are reached from in-battle or map-feature paths. FUN_000ab758 has one caller, FUN_000ab46c, called at VA 0x1069F in FUN_00010294. In FUN_00010294, VA 0x1069F precedes FUN_000a60ec at 0x107BF and FUN_00024110 at 0x107D6.
- Engine: LIVE:game/state/rules/battle/battlemap.cpp:205-256, 969-972 and 1214-1221 (identical at HEAD). battle.h:28 sets MAX_UNITS_PER_SIDE=36. agenttype.h:197 defaults allowsDirectControl to false. agent.cpp:403-418 (hire and transfer set homeBuilding immediately) and agent.cpp:1058-1063 (non-soldiers walk back to their home building) show that away staff exist.
- Notes: scratchpad/validate/adj_v_base_defence_BD04.md.

**Fix (target both).** All in BattleMap::createBattle (game/state/rules/battle/battlemap.cpp:205-256), plus a facility count. LIVE and HEAD are identical, so edit the file once and make sure both trees get it.
1. Draft by presence, not by home. Take Soldier-role agents with currentBuilding == base building, agents inside craft docked at the base (soldier role only), and non-soldier staff with currentBuilding == base building. Drop the homeBuilding->base clause so away or in-transit staff are not drafted.
2. Count finished Security Station and Advanced Security Station facilities (buildTime == 0, sector 38 or 39). Set the agent budget to 36 - min(12, 4 x finished stations).
3. If the present total exceeds the budget, scale each of the four classes (Soldier, Physicist, BioChemist, Engineer) to floor(count x budget / total) and take the first n agents of each class in array order. This replaces the current soldiers-first fill.
4. Optionally add the 60-unit total cap. Minus the clamped turret reservation, limit X-COM units plus aliens to 60 so turrets beyond 12 only spawn while slots remain. The engine currently allows 36 + 36 + turrets.
5. Do NOT cap stations at 3. Keep turret creation from sector guardianLocations uncapped, subject to the 60-slot ceiling in step 4.
6. Leave the unfinished-facility treatment to a separate item (the EXE builds a construction sector).

## v_base_defence / BD08 -> engine_bug (high, high)

**Disputed point.** Whether BD08's verdict "match" (hostile-org raids on X-COM bases exist and are scheduled with the same structure in engine and UFO2P.EXE, fix_sketch "none for structure") survives. Engine-lens refuter: should be engine_bug (the engine's target pool omits bases). EXE-lens refuter: should be diverges (snapshot semantics, slot cap, timer handling). Both agree it is not "match".

**Ruling.** "match" is refuted. The engine-lens defect is real and decisive, so the verdict is engine_bug. The EXE-lens divergences are also real but secondary; track them as sub-items of the same fix.

Primary defect, CONFIRMED in both trees. The engine picks an X-COM raid target from Organisation::buildings (organisation.cpp:71-80, organisation.h:241, marked "not serialized"). That list is filled only by City::initCity (city.cpp HEAD:150 / LIVE:157). initState runs before fillPlayerStartingProperty (difficultymenu.cpp:36-40). The start base (gamestate.cpp HEAD:752 / LIVE:753), BaseBuyScreen (basebuyscreen.cpp:93) and Base::die (base.cpp:145) all write building->owner without touching the list. So in a continuous campaign X-COM's pool is only the non-base BUILDING_RECYCLOTORIUM_TWO. RaidMission::execute then takes its else-branch (organisation.cpp:1308-1338): no DefendTheBase, only relation +7/+12 and an event. A save/load rebuilds the pool through initCity, so engine behaviour differs between a continuous run and a reloaded one. The EXE scans all buildings live (FUN_00091f70), and a base purchase sets owner byte 0 (FUN_00015400 at VA 0x15516).

My own census of 100 benchmark victory.save files (97 with an X-COM base) found 49 pending raid missions aimed at X-COM-owned buildings: 48 on the Recyclotorium and 1 on a base.

I independently re-confirmed the EXE-lens divergences from the FUN_00092060 listing and an operand census of the 0x16F388 array:
- The snapshot array is not a daily all-pairs snapshot. Its writers are the new-game copy, the FUN_00099130 start init, one column-0 cell per hourly FUN_00096BC8 call (cursor mod 27), and save/load.
- A hostile pair whose target org B has no building is skipped without touching the timer. The engine resets the timer first.
- The incident table has 10 slots, and a full table still decrements the timer. The engine list is unbounded.
- The EXE excludes B = Aliens. The engine iterates all organisations.
- A without a building falls back to a random building, then to org strength. The validator's "skip unless A has a building" is wrong.

Where I do not follow the engine-lens refuter: its "EXE X-COM pool is purchased bases only, so P(base | Raid/Storm) = 1" is NOT established. The owner 0 -> 2 remap loop at VA 0x1017C runs in main before the building loader FUN_00014854 sets the building count [0xB3A68], so it is probably a no-op. The EXE value is 1/2 (one base plus the Recyclotorium) or 1. That is unresolved and does not affect the verdict, because the engine's value is 0 either way.

**Engine.** LIVE = HEAD for all logic cited. setRaidMissions (organisation.cpp:350-430, daily call HEAD gamestate.cpp:1551 / LIVE:1584) iterates the alphabetical std::map and tests hostility with isRelatedTo (< -50). Pressure is max(1, previous_relations - current), where previous_relations is refreshed for every pair daily by updateRelations (organisation.cpp:682-705). The timer is reset to 80 - 2*difficulty on a trigger, before the target-building null check (:374-387). raid_missions is unbounded. The target comes from otherOrg->pickRandomBuilding, i.e. the stale init-time Organisation::buildings list, so purchased and start bases are never chosen in a continuous run (X-COM pool = Recyclotorium only), and a base bought from the Government stays in the Government's list. RaidMission::execute (:1285-1338) fires DefendTheBase or Base::die only when target->owner == player && target->base. On the Recyclotorium it applies relation changes and an event instead. Also at organisation.cpp:399, `auto &raidMissionChance = rules.neutral_low_manpower;` followed by assignment copies INTO the shared table. A sample save (g000-r0005) holds the military_low map there instead of the shipped {None 70, Treaty 20, Attack 10}. This does not affect X-COM-targeted draws (defender strength 0 gives the High or Normal bucket) but corrupts AI-vs-AI draws.

**EXE.** UFO2P.EXE (non-4) FUN_00092060 (file 0xF4704) runs once a day (single caller FUN_0006BC70 at VA 0x6BD9B). A runs 2..26 in index order (Government, Megapol before the Cult), B runs 0..26 except 1. A pair passes only if the signed byte matrix[A][B] <= -50 (0xCE at VA 0x1277E4). Pressure = max(1, snap[A][B] - matrix[A][B]). A trigger needs pressure > rand16(timer) and a free slot out of 10 (FUN_00091E68, table at 0x13E280). On a trigger the timer is set to 80 - 2*difficulty, the delay is rand16(240)+10 five-minute units, and the incident type comes from table A or B via the biased FUN_000638DC draw. A failed check, or a full slot table, does `dec [0x1277A4]`. A non-hostile pair, or B with no building, is skipped with the timer untouched. The target is a live FUN_00091F70 scan of building owner bytes, so purchased bases count immediately. The executor FUN_00092470 runs every 5 minutes. Types 2 (Raid) and 3 (Storm) on an owner-0 building with FUN_000705F8 != -1 set the flags, and FUN_00099E04 turns them into FUN_000AC08C reason 5 (base attack). The snap array at VA 0x16F388 is written only by the new-game copy, the start init, one column-0 cell per hourly FUN_00096BC8 call, and save/load. Org-to-org cells are never refreshed after start.

**Evidence.** ENGINE (cite by tree; organisation.cpp/.h, basebuyscreen.cpp and base.cpp are identical in HEAD and LIVE):
- organisation.cpp:71-80, 350-430, 399, 682-705, 1285-1338; organisation.h:241
- city.cpp HEAD:150 / LIVE:157
- gamestate.cpp start-base owner write HEAD:752 / LIVE:753; setRaidMissions call HEAD:1551 / LIVE:1584
- basebuyscreen.cpp:93; base.cpp:145; difficultymenu.cpp:36-40
- no `buildings` member in the Organisation entry of gamestate_serialize.xml
- shipped raid tables at tools/extractors/extract_organisations.cpp:366-397
- 100-save census: 48 pending raids on Recyclotorium, 1 on a base. In g000-r0011 the single base hit is a Mutant Alliance Attack on WAREHOUSE_TEN, original owner ORG_GOVERNMENT in 77 of 100 saves. Mutant to X-COM is -100 and Mutant to Government is -42. The likely cause is a pool rebuilt after a reload, but a former-owner stale-list path cannot be excluded.

EXE (UFO2P.EXE non-4, physical file offset = VA + 0x626A4 code, + 0x616A4 data; capstone over canonical/UFO2P.EXE):
- FUN_00092060 file 0xF4704 (prologue 53515256575583ec): non-hostile skip 0x9221C; B-no-building skip 0x921A1 -> 0x92423; slot call 0x92222; pressure 0x92234-0x92253; trigger JLE 0x9226E; no-slot test 0x92276; timer reset 0x9227C-0x92292; DEC 0x9241D
- FUN_00091E68: slot scan, 10 entries, stride 8
- FUN_00091F70 file 0xF4614: live owner-byte scan
- FUN_00015400: base purchase `mov byte [eax+0xC8],0` at VA 0x15516
- snapshot VA 0x16F388 operand census (window 0x9F368 up): reads 0x92238, 0x96BE5, 0x978E7; writes 0x149CD, 0x99213, 0x97214 (FUN_00096BC8 tail, cursor [0x127998] mod 27, called from 0x4AE57/0x4ABE7), 0xA38E0/0xA46F9 (save/load, 0x310 bytes)
- day-end call chain 0x102C8 -> 0x10842 -> 0x6BD9B
- UNVERIFIED and left explicit: the remap loop at VA 0x1017C probably no-op; the refuter's 95% vs 0.4% first-trigger-order figure was not re-derived; type 6 (0x92A15) has no owner test and its effect on X-COM bases is unexamined.
No Ghidra copy was created, so there is nothing to delete.

**Fix (target both).** 1) Primary, one patch for both trees: replace the list lookup with a live scan, as FUN_00091F70 does.
- In Organisation::pickRandomBuilding, and in the setRaidMissions source-building list, iterate city->buildings and filter `b->owner.id == id`, instead of using the init-time `buildings` vector.
- This also fixes the former-owner staleness. Patching each owner-write site (BaseBuyScreen, fillPlayerStartingProperty, Base::die) would leave the former owner's list stale.
- Blast radius: other consumers of `buildings` share the stale list (hiree location ~564-569, vehicle park ~1347/1375/1519). Keep that out of scope, or sync those consumers separately.
2) RaidMission::execute: a Raid or Storm on an X-COM-owned NON-base building should be a no-op, as in the EXE (owner 0 and FUN_000705F8 == -1). The engine currently applies +7/+12 relations and an event.
3) Parity items in setRaidMissions:
- skip a pair with no target building BEFORE resetting the timer
- keep non-hostile pairs from touching the timer
- add a 10-slot cap on pending raid missions that still decrements the timer when full
- skip B == Aliens
- separate the raid snapshot from the daily previous_relations: copy at start, rotate one X-COM-column cell per game hour mod 27, and keep org-to-org cells at their start values
- source building = first flagged building, then random, then raidingStrength
- consider EXE index iteration order (A = 2..26)
4) Fix the alias at organisation.cpp:399 (use a const pointer or copy) so neutral_low_manpower is no longer overwritten.
5) Regression tests: base purchase followed by a Raid/Storm trigger must reach DefendTheBase without a save/load cycle, and the same run before and after save/load must pick the same targets. Update the strategy note: P(base | Raid/Storm) is 1/2 or 1 in the EXE, not a fixed 12.5 percent.

## v_base_defence / BD14a -> diverges (high, medium)

**Disputed point.** Whether BD14a stays "match, fix none, with two bases the loss is survivable", or the engine diverges because Base::die kills every agent and docked craft in the lost base (retreated survivors included), whereas the original only removes the base and re-homes its people and craft.

**Ruling.** The two lenses are not in factual conflict. They checked different halves. (1) Base removal on a non-win, and game over when the last base goes, is a match. I confirmed it in the listings: FUN_000b32ac type 2 keeps the base only if [0x244084]==1, otherwise FUN_000705f8 and then FUN_000b3114 remove it. FUN_000705f8 is a pure building-to-base lookup, which I checked in a Ghidra run. The engine does the same in the BaseDefense branch of battle.cpp (HEAD 3596-3613, LIVE 3603-3620). (2) What happens to the occupants diverges. FUN_000b3114 (listing 0xB3114-0xB326E) contains no kill call. Its only calls are FUN_00060ea4 (game-over scan), FUN_000bf910 (zero the 0x2BE base record) and FUN_000b3270 (round-robin to the next active base). It re-homes every employed agent whose home word (+0x24) is the dead base, and every X-COM craft whose home word (+0x2E) is the dead base. The compared and written fields are the same. Nothing else in the post-battle path kills them either. FUN_000b44a4 only acts on a win. FUN_000b27b4 and FUN_000b1ec8 never read the success word (b1ec8 only builds a message). The agent write-back in FUN_000b21a8 never reads it for agent entries. The engine's Base::die (base.cpp:134-142, identical in HEAD and LIVE) copies building->currentAgents and calls Agent::die on each, then copies currentVehicles and calls Vehicle::die on each. Agents never leave currentAgents during a base defence (no leaveBuilding in that path), and retreat never removes them. So every survivor and every docked craft dies. The validator's "fix: none" and "the loss is survivable with two bases" are therefore wrong for the engine. BD16/F16 files only the away agents and craft, and its fix says "re-home away", so the present occupants fall through. Ruling: keep the base-removal rule as a match, but file BD14a as diverges with a fix, merged with BD16. This follows the engine lens's corrected claim. The exe lens's confirmation of the removal rule does not conflict with it. Medium-confidence part: that retreated soldiers specifically come back alive in the original is inferred, because the TACP unit write-back for a fled unit was not traced. The non-fielded staff, docked craft and unconscious units cases do not depend on it.

**Engine.** Base-defence loss or retreat: abortMission runs killStrandedUnits and then retreats the rest, so playerWon=false and Base::die(state,false) runs (battle.cpp BaseDefense branch, HEAD 3596-3613, LIVE 3603-3620). Base::die refunds cargo, kills every agent in building->currentAgents (Agent::die sets health 0), kills every craft in building->currentVehicles (Vehicle::die also kills the agents aboard), sets owner=Government, calls building->collapse(), erases the base, and raises GameLost only if no player base remains. No re-homing happens. Present scientists, engineers and soldiers die, as do docked craft. Away units keep a dangling homeBuilding (BD16).

**EXE.** UFO2P non-4 FUN_000b32ac type 2 removes the base unless [0x244084]==1. FUN_000b3114 sets building +0xC8=2 (Government) and +0xC9=0, sets base record +0x15=0, decrements [0xD4D3C], calls the game-over scan FUN_00060ea4, and zeroes the base record. If a base remains, it clears that base's event rows (0xDE2AE) and project rows (0x141665). It then re-homes every X-COM craft whose word +0x2E is the dead base, and every employed agent whose word +0x24 is the dead base, to FUN_000b3270(). That function is round-robin from cursor [0x130958] to the next active base. Nobody is killed. Agent write-back (FUN_000b21a8) does not depend on the success word. The building is not collapsed. The last base gone gives game over ('All X-COM bases have been destroyed').

**Evidence.** UFO2P.EXE non-4 generation, physical file offset = Ghidra VA + 0x626A4. FUN_000b3114: file 0x1157B8, listing 0xB3114-0xB326E, in validate/r_v_base_defence_14_0_work/c1.txt:974. Re-home loops: 0xB31F6-0xB3228 (craft, stride 0x276 at 0x160FD8, compares and writes word +0x2E) and 0xB3230-0xB3268 (agents, stride 0xCE at 0x144918, +0x21==0, compares and writes word +0x24). FUN_000b3270: file 0x115914, in c6.txt. FUN_000b32ac: file 0x115950, type-2 branch 0xB350B-0xB3552. FUN_000b21a8: file 0x11484C, 0xB21C9-0xB265D; its reads of [0x244084] are only in the non-agent branch at 0xB23F7 and 0xB2517. FUN_000b44a4: c1.txt:692. FUN_000705f8, FUN_000b27b4 and FUN_000b1ec8: validate/adj_10_g1.txt (my own Ghidra run, copy adj_10, deleted). Engine: base.cpp:87-167 (kill loops at 134-142, collapse at 145-146), identical in HEAD and LIVE. Agent::die at shared/agent.cpp:963. Vehicle::die at city/vehicle.cpp:1925. finishBattle sends agents home with gotoBuilding but never removes them from currentAgents (battle.cpp ~2866). abortMission is at battle.cpp:2167 in HEAD. HEAD and LIVE differ in battle.cpp only by a 7-line RaidAliens hunk at LIVE 3581-3587 (unrelated).

**Fix (target both).** Merge with BD16 into one change. In the BaseDefense-loss branch of Battle::finishBattle, or in a Base::die variant used only for battle loss, check whether another player base exists before killing anyone. If one does, re-home every living agent with homeBuilding == this building (present or away) and every X-COM craft homed here to the next remaining base, round-robin as FUN_000b3270 does, and skip the Agent::die and Vehicle::die loops for them. Move present agents and docked craft into the new base building. Keep refunding or re-routing cargo, keep GameLost when no base remains, and keep the owner=Government step. Whether to keep building->collapse() is a separate question. FUN_000b3114 has no collapse call, so it is probably an engine addition, and no finding covers it. Do not change the building-collapse callers (BD13) without separate EXE evidence for that path. Set BD14a's strategy rule to: retreat or loss costs the base building, its facilities and stores, but with a second base the original keeps all personnel and craft.

## v_research_rates / RR-10 -> diverges (medium, high)

**Disputed point.** The verdict "diverges" is not in dispute. The disagreement is over (1) the validator's magnitude, "engine arrivals roughly 2-3x the EXE" (2.5-4.5 vs 1.5 per technical class per day); (2) the direction and time profile of the strategy impact, since the refuter says the engine is over-generous only early and starves the roster after takeovers; and (3) the fix sketch's fallback of capping the arrival mean at 1.5 per class per day.

**Ruling.** The refuter is right on all three points. The validator's description of the EXE rule is accurate and I re-verified it.

1. Magnitude. The engine's 2.5-4.5 per class per day is correct arithmetic but only at relation 0 to +20. At relation 0 it is 2.58 per day, about 1.7x the EXE's 1.5. +10 and +20 give 3.56 and 4.57, and -20 gives 1.59. The "2-3x" holds only at relation +10 to +20, which the refuter's benchmark saves do not reach.
2. Direction. The EXE rule does not read any org's relation or takeover state for the four classes. The engine's arrival chance depends on the hiring org's relation, and `Organisation::takeOver` sets that relation to -100, which is Hostile, so a taken-over org produces zero arrivals. So the engine can over-supply early and under-supply once takeovers land. The refuter's day-bucket numbers (about 1.4x early, parity at days 10-20, about 0.5x after day 20) are INFERRED. They come from end-of-run relation snapshots run through the engine formula, not from observed arrivals.
3. Fix. A bare mean cap is wrong. With the engine's 33% daily churn and arrivals at 1.5 per day, my simulation gives a standing pool of about 4.6 against the EXE's about 7.0. The whole rule has to be replaced.

I also closed the refuter's open caveat. In the hire commit, the relation helpers FUN_00091DB0 and FUN_00091DE4 are called with constant org operands to gate the post-hire move FUN_0007A730. They are not a test of the applicant's origin org, and the hire-list builder FUN_0007CED8 has no relation filter. Nothing downstream in the EXE filters applicants by origin-org relation.

Impact stays medium. Supply is relation-coupled and takeover-starved in the engine, but flat in the EXE. That changes how fast the roster can grow toward the caps, though a pool of about 7 either way is available at relation 0.

**Engine.** Organisation::updateHirableAgents runs once per organisation each end of day and once at new game. In the following, "attempts" is the hirableAgentTypes `second` value from organisations.xml.
- Each org first removes each of its own hirable agents with probability 33% (CHANGE_HIREE_GONE).
- If the org's relation to X-COM is below -50 (Hostile, which includes every taken-over org), it spawns nothing.
- Otherwise, for each hirable type it rolls `attempts` times. Each roll is randBoundsInclusive(0,100) <= chance, where chance = 25 + relation, or (50 + relation) / 2 when relation is below 0.
- The attempts sum to 10 each for Biochemist, Mechanic and Physicist, 6 for Human, 2 for Hybrid (Mutant Alliance) and 2 for Android (S.E.L.F.).
- Arrivals per technical class per day (my simulation): 1.59 at relation -20, 2.08 at -10, 2.58 at 0, 3.56 at +10, 4.57 at +20.
- The standing pool is about 4.8, 6.3, 7.8, 10.8 and 13.8 respectively.
- Arrivals are placed in a building of the originating org, so the origin org is tied to the relation roll.

**EXE.** UFO2P.EXE FUN_0007A474 runs on each day rollover (FUN_0004AC40 and FUN_0004AC0C, gated on city index 0) and at new-game seeding (FUN_000799DC). It makes one pass over 4 classes: 0 = agents, with a cap of 100, and 1-3 = technical, with a cap of 50 each. The limits table at file 0xDA9A4 reads (100, 50, 50, 50).
- Thinning: the applicants in a class are counted (slot used, owner +0x21 != 0, flag +0xB0 != 0). Then removed = rand[0, floor(n/3)+1] inclusive, taken from the first applicants in slot order.
- Arrival: n = min(rand[0,3], cap - (employed + remaining applicants)). Each new applicant gets an origin org uniform in 2..22 via rand[0,0x14]+2.
- No org relation, hostility or takeover test applies to any of the four classes.
- Hybrids and Androids: after the loop, if the Mutant Alliance relation (VA 0x16EEC8, row 24 of the matrix at 0x16EC28 with stride 0x1C) is >= 0, add rand[0,1] Hybrids, capped by the class-0 room. Likewise add rand[0,1] Androids if the S.E.L.F. relation (VA 0x16EEAC) is >= 0.
- Result: mean 1.5 arrivals per technical class per day, independent of diplomacy. The standing pool is about 7.0 after arrivals and about 5.5 before, from my simulation.

**Evidence.** OBSERVED (high confidence):
- EXE: UFO2P.EXE, ISO non-4, CRC 0x4749ffc1. FUN_0007A474 is at physical file 0xDCB18 (VA 0x7A474, +0x626A4), prologue bytes `53 51 52 56 57 55 83` confirmed in the canonical EXE.
- Limits table: VA 0x78300 = file 0xDA9A4 = (100, 50, 50, 50), read from the canonical EXE.
- Listing, re-read in my own Ghidra copy:
  - Count: file 0xDCB4A-0xDCB6C.
  - Thinning with FUN_0005D1D8(n) = X % (n+1), inclusive [0,n]: file 0xDCB7F-0xDCBEA, removal via FUN_0007AD3C.
  - Recount with the `JZ 0x7a57d` branch that counts owner==0 or applicant: file 0xDCC10-0xDCC2F.
  - Arrival `MOV EAX,3; CALL 0x5D1D8` at file 0xDCC3D, `min(rand[0,3], room)`, origin `rand[0,0x14]+2` into FUN_000791E0.
  - Hybrid and Android gates are signed byte compares at file 0xDCCCE and 0xDCD4B, with raw operands c8ee0900 and acee0900 = VA 0x16EEC8 and 0x16EEAC after the +0xD0000 fixup.
  - These are the only two relation reads in the function.
- FUN_000791E0 never calls the relation helpers (not in the FUN_00091DB0 or FUN_00091DE4 caller lists).
- Hire commit FUN_0007EA7C (file 0xE1120): it calls FUN_00091DB0 with constant operand pairs (0,9) and (9,0), where the helper tests matrix[a*28+b] < -49. This gates the post-hire move FUN_0007A730. It is a fixed-org hostility gate, not a filter on the applicant's origin org. The hire list builder FUN_0007CED8 has no relation filter and caps each class at 0x14.
- Engine: `organisation.cpp:541-626` is byte-identical in LIVE and HEAD, and the whole file is identical.
  - `organisation.h:22` sets CHANGE_HIREE_GONE = 33.
  - Hostile is relation < -50 (`isRelatedTo`).
  - `Organisation::takeOver` (`organisation.cpp:90-95`) sets the relation to X-COM to -100, so a taken-over org gets zero attempts.
  - Call sites: LIVE `gamestate.cpp:1579` (daily) and `:576` (new game); HEAD `gamestate.cpp:1546` and `:575`.
  - Attempt sums come from `data/common_patch/gamestate/organisations.xml`, identical in both trees.
- Arithmetic and simulation (50k-200k days) for the EXE and engine rates above. A hypothetical engine churn with EXE-style arrivals gives a pool of about 4.6.

INFERRED (medium):
- The benchmark time profile: about 2.1 per technical class on days 0-10, about 1.3-1.5 on days 10-20, and about 0.65-0.85 after day 20, with 84-95% of saves below 1.5.
- It is the refuter's, computed from relation snapshots in 100 victory.save files through the engine formula. These are not observed arrivals.
- I could only re-aggregate their saved JSON (it reproduces the day-bucket means). I could not re-open the saves; access to the build-funding directory was denied.
- Label slip: the refuter's text calls learning-control-seed11/g000-r0001 a "day-34" save, but their own JSON gives 21.8 days. The Mechanic rate of 0.26 for that save matches the JSON. The starvation mechanism is proven by the code (takeover gives -100, which is Hostile, which gives zero attempts), not by which day the save was taken.

**Fix (target both).** Replace the whole updateHirableAgents rule with the EXE rule. Do not cap the mean at 1.5 while keeping the 33% churn; that would give a pool of about 4.6 against the EXE's about 7.0.

1. Do one daily pass per class (the three technical classes cap 50, agents cap 100), run from the same daily hook.
2. Thin: remove rand[0, floor(n/3)+1] of the class's current applicants, in stable order.
3. Add min(rand[0,3], cap - (employed + remaining applicants)) new applicants. Pick each one's origin org uniformly from the EXE's 21 codes (2..22), with no relation or takeover test.
4. For the agent class, also add rand[0,1] Hybrids if Mutant Alliance's relation to X-COM is >= 0, and rand[0,1] Androids if S.E.L.F.'s is >= 0. Both are counted against the class-0 room.
5. Delete the 33% CHANGE_HIREE_GONE churn. The per-org `hirableAgentTypes` attempt counts in organisations.xml stop driving arrivals; keep them only if something else needs them.
6. Choose the hiree's location by a rule that does not depend on the origin org having buildings, because that org may be hostile or taken over.

The Hybrid/Android sub-rule is detail on top of the main fix. The change touches `organisation.cpp:541-626` and the two `gamestate.cpp` call sites. Both trees need the same edit.

## v_agents / V-AG-05 -> diverges (low, high)

**Disputed point.** Whether UFO2P.EXE auto-assigns training to the 10 starting soldiers (validator: no, so the engine's gamestate.cpp:802/803 pre-assignment diverges and the harness must press BUTTON_AGENT_PHYSICAL) versus the refuter's claim that the EXE does auto-assign Combat training (so the default assignment matches). Secondary: whether the injured-trainee divergence survives.

**Ruling.** The refuter is right on the default assignment. The validator's negative came from a [reg+0xC8] operand scan that cannot see the static-base form [EDX+0x1449E0] the new-game generator uses. The injured-trainee half of the finding stands. The original medium rating is too high, so I set the impact to low. Wounded agents lose about 1 to 2 days of training per wound, and the trainee-count dilution only applies when over capacity. The finding is narrowed to eligibility: wounded agents must not train, and must not count toward trainee usage. The 'starting soldiers are not training until the player assigns them' rule and the fix of removing the initial assignment are both wrong and must be dropped.

**Engine.** LIVE and HEAD are the same for agent.cpp and base.cpp, with gamestate.cpp differing by one line offset. agent.cpp:1105-1127 (Agent::updateHourly) runs the Train block whenever trainingAssignment != None, with no health check. The Heal block (:1090-1103) is a separate if, so an injured agent heals and trains in the same hour. base.cpp:535-554 counts every Physical- or Psi-assigned agent with that home building toward Training/Psi usage, wounded or not. gamestate.cpp:802 (HEAD) / :803 (LIVE) assigns Psi if initial psi_energy > 30, else Physical, for every initial agent whose type has canTrain. Only AGENTTYPE_X-COM_AGENT_HUMAN is availableAtTheGameStart, and its psi_energy range is 5..20, so the Psi branch never fires and all 10 start Physical. Technicians and the android have canTrain=false. Agent::hire (agent.cpp:402-409) leaves the assignment None, and assignTraining requires Soldier role and canTrain.

**EXE.** The new-game generator FUN_000799dc sets +0xC8 = 2 (Combat training) on all 10 starting soldiers. It makes six calls to FUN_000791e0 with type 0 (human), in loops of 2, 2, 2, 2, 1 and 1, each followed by a store of 2 to +0xC8. The 15 staff (3 loops of 5, types 1 to 3) get no store. Hired applicants stay at 0 (Not assigned), so hire matches the engine. In the +0xC8 encoding, 1 means Psi and 2 means Combat. The accrual routine FUN_0006bf04 compares current health +0x3C with max +0x3B: if cur < max it accumulates healing only, and the training accumulators are the else branch. FUN_0006bdac counts wounded agents into a separate wounded array and counts only unwounded class-0 agents into the combat and psi trainee arrays. In the original, wounded agents therefore never train and never dilute training capacity.

**Evidence.** I checked everything below directly with capstone on canonical UFO2P.EXE. It is the ISO non-4 build with CRC 0x4749ffc1, and the code file offset is VA + 0x626A4. No Ghidra copy was needed, and I created no adj_12 copy.

1. FUN_000799dc is at file 0xDC080 and its prologue bytes are 53 51 52 56 57 55.
2. Six stores `mov byte [edx+0x749E0], ch|bl` follow `mov ch|bl, 2`. The raw displacement 0x749E0 + 0xD0000 = VA 0x1449E0, and 0x1449E0 - 0x144918 (person table base) = 0xC8. Stores are at VA 0x79A72, 0x79C17, 0x79DBC, 0x79F36, 0x7A0A1, 0x7A1BC, which are file 0xDC116, 0xDC2BB, 0xDC460, 0xDC5DA, 0xDC745, 0xDC860. The raw bytes at those offsets are 88 aa|9a e0 49 07 00.
3. Each store follows `call 0x791e0` with EAX=0, EDX=0. The loop bounds are `cmp bx|si|dx,2; jl` four times (2 each), then two single-iteration blocks, giving 10. The staff loops pass EAX=1, 2 and 3 with no +0xC8 store.
4. An E8 call scan gives the chain 0x1029D to FUN_000b9178 to FUN_000b94c0, which calls FUN_000799dc at 0xB9507.
5. FUN_0006bf04 is at file 0xCE5A8 and starts `mov ebx,0x74918`. At 0x6BF90 `mov dl,[ebx+0x3c]; mov dh,[ebx+0x3b]; cmp dl,dh; jae 0x6bfcd`. Below the compare, the cur < max path accumulates healing. At 0x6BFCD it reads +0xC8, treating 1 as psi and 2 as combat.
6. FUN_0006bdac is at file 0xCE450. At 0x6BEAB `mov cl,[+0x3b]; cmp cl,[+0x3c]; jbe`, the wounded path increments [+0x6E2F4] and skips the trainee increments at +0x6E2FC (combat) and +0x6E304 (psi).
7. FUN_0006c058 at 0x6C099-0x6C0AA computes cur = min(max, cur + A0/48) on +0x3C and +0x3B, confirming which byte is current.
8. Engine: LIVE and HEAD agent.cpp and base.cpp are identical (diff clean). I read agent.cpp:61-71, :394-409 and :1090-1127, gamestate.cpp:793-805, base.cpp:520-560, and data/common_patch/gamestate/agent_types.xml:465, 477, 537, 540, 782 and 978.

The one inferred point is that nothing zeroes +0xC8 on the starting agents after generation. The refuter's static-base scan found no such writer.

**Fix (target both).** Do NOT remove or alter the initial assignment. The engine's effective default (all 10 starting soldiers Physical) matches the EXE's Combat. Optionally collapse the dead Psi branch at gamestate.cpp:802/803 to an unconditional Physical.

Change only the wounded-trainee behaviour, in two places:

1. Agent::updateHourly (agent.cpp:~1105): compute `bool wounded = modified_stats.health < current_stats.health;` once. Keep the heal block gated as `wounded && !recentlyFought`. Guard the Train block with `!wounded`, so trainPhysical and trainPsi never run on a wounded agent. The EXE has no recentlyFought exception for the training gate.

2. Base::getCapacityUsed, Training and Psi cases (base.cpp:~535-554): add `&& a.second->modified_stats.health >= a.second->current_stats.health`, so wounded trainees do not inflate usage.

Strategy-rule correction for the plan: the starting soldiers already train from day 0, so the harness must not press BUTTON_AGENT_PHYSICAL on them. Heal wounded agents first, because a Medical Bay is needed for them to resume training.

## v_agents / V-AG-14 -> engine_missing (medium, high)

**Disputed point.** Not the verdict class (validator and the engine-lens refuter both say engine_missing). The dispute is whether the validator's written EXE description and fix sketch are accurate. The validator described FUN_000a1324 as a merge from TMPEXP.DAT, WEAPEXP.DAT as 17,160 u16, FUN_0009f2b0 as an EXPERIEN processor, and the writes as debrief-only. It also described a C++ 'engine comment' saying 'NOT extracted' and called the BRAIN/WEAPEXP consumers undecoded. The refuter marked the finding 'refuted' on those details while keeping the class. It also left one link unproven: whether the learned state is persisted back to BRAIN.DAT in single-player.

**Ruling.** The class stands and the refuter's factual corrections stand. I re-checked each against listings and decompiles, and I closed the learning loop, which neither side had done. The validator's text is wrong on those points. The finding is not refuted in substance: the original has a complete cross-battle learning pipeline that the engine lacks entirely, and docs/campaign-plan.md denies it. Keep engine_missing and medium. The impact is unquantified and the fix is docs-only for now. The effect on alien action choice is real but its size is undecoded, so do not attempt an engine port yet.

**Engine.** OpenApoc has no learned-AI state. Alien tactics are heuristic modes in game/state/battle/ai/* (tacticalai, unitaivanilla, unitaibehavior, unitaihardcore, aidecision). The 'genome' text in unitaibehavior.cpp is an external training harness's config knob, not an in-game store. Exact-pattern greps for EXPERIEN.DAT, WEAPEXP, BRAIN.DAT, TMPEXP and BRAIN0 across game/, framework/, library/, tools/ and data/ find nothing, in HEAD and LIVE. The only claim is in docs/campaign-plan.md (same text in both trees). HEAD lines: 1177 (folklore row 21, 'the AI learns: It does not'), 1229 (Still-open item 4, 'EXPERIEN.DAT is not extracted'), 1247-1249 (section 16.1, 'nothing carries tactical state between battles'). LIVE lines: 1192, 1244, 1262 (LIVE is HEAD plus 15 earlier lines). The finding's 'engine comment' does not exist in C++ in either tree. The soldier XP award is BattleUnit::processExperience (battleunit.cpp:4434, called battle.cpp:3060). It is a separate mechanism, unaffected by this finding, and the doc's '#L4171' link is stale.

**EXE.** TACP.EXE (ISO non-4) contains a closed, persistent learning loop. It is live in single-player: the argv 'DEMO' flag (DAT_0027A805), the network flag (DAT_001B7BAC) and the 'TRAIN' flag are all 0 in a normal campaign.

1. Sampling. The per-unit AI planner FUN_00095490 (file 0xEFF34) evaluates candidate action types (switch cases 0..0xB, calling FUN_000988e0, FUN_000999e8, FUN_0009a424, FUN_0009b058, FUN_0009bb5c). Each evaluation runs the model evaluator FUN_000a0fb0 (6 callers). It takes a 25-feature vector, uses 10-bin per-feature tables plus a 25x25 table, and returns a normalised distribution. The callers reduce it to an expected value over 3 outcome classes. The evaluators FUN_0009a60c, FUN_00098530, FUN_00099ce0 and FUN_0009b7e0 are called from FUN_00095490 and feed the action choice. The choice has a random branch: a random value below 800 takes the best-scoring action.

2. Logging. FUN_000a1148 claims one of 100 slots in a ring at VA 0x276BD4 (stride 0x72) and sets state byte 1. FUN_000a124c sets state 2 and stores the outcome. FUN_000a12a4 appends state-2 slots to TACDATA/EXPERIEN.DAT (open flags 0x231), clears the byte and sets DAT_0027985C=1. The flush runs inside the AI step, time-gated: when DAT_002A1760 is below the battle clock it calls FUN_000a12a4, then FUN_0009ec78 (writes WEAPEXP.DAT) if DAT_001B7BAC==0, then sets the next time to now+500. It is not debrief-only.

3. Record layout, confirmed on the depot file. 0x72 bytes: u8 state (=2 on disk), u8 pad, i32 id, i32 n (25, 17, 16, 12 or 9), i32 feat[25], i32 outcome at +0x6E (values 0..5). Depot file: 7,889 records, state byte 2 in every record. BRAIN.DAT is 555,900 B = 25 x 0x56DC, header (101, 25, 6).

4. Debrief. FUN_000b3b28 runs, when DEMO==0 and net==0 and DAT_0027985C!=0: the 'Experience Processing' string, then FUN_000a1324, then FUN_000a15f4, then FUN_00013e64 with TACDATA/BRAIN.DAT, buffer 0x1EF058, size 0x87B7C.
 - FUN_000a1324 is NOT a merge from TMPEXP.DAT. It caps the log. Count = size/0x72. If count <= 10000 it does nothing. Otherwise it renames EXPERIEN.DAT to TMPEXP.DAT, recreates EXPERIEN.DAT, copies back in 100-record chunks skipping the first count-10000 records, and deletes TMPEXP.DAT. The newest 10,000 records are kept.
 - FUN_000a15f4 is the refit. It makes three passes over EXPERIEN.DAT: per-feature min and range, histogram bins, then a time-decayed exp(-0.0004 x age) weighting. It builds the 25x25 table, calls the matrix routine FUN_000a2474 (error 0xFFFFFFFA on failure), and rebuilds the in-memory brain array.
 - FUN_00013e64 is a file write (open, FUN_000c4fe6 which tests the stream write bit 0x2, close). It writes the whole brain array to BRAIN.DAT. This is the link neither side had shown.

5. Battle init. FUN_00014948 fills two byte tables with 0x32 for 0x4308 iterations each (VA 0x1CAE18 and 0x1CF120) and calls FUN_0009ed00 to load WEAPEXP.DAT. WEAPEXP.DAT is 34,320 B of two 0x4308-byte tables, NOT 17,160 u16; 34,194 bytes of the depot copy equal 0x32. It then does a findfirst on TACDATA/BRAIN.DAT. If the file is absent it runs the refit and saves BRAIN.DAT. If present it loads it into 0x1EF058 and counts the brains.

6. FUN_0009f2b0 is not an EXPERIEN processor. Its only caller is the decision-time feature builder FUN_0009a77c.

7. UFO2P: FUN_000a57fc (new-game routine FUN_000b94c0), FUN_000a555c (caller FUN_000a3234) and FUN_000a565c (caller FUN_000a4054) all reference the TACDATA WEAPEXP/BRAIN/EXPERIEN names; the first also references the pristine WEAPEXP0/BRAIN0 names. The roles (new-game reset, per-save copy and restore) are inferred from string references only.

Still undecoded: the meaning of the 25 features per sample id, the outcome classes, and the size of the effect on the alien action mix. The 80/20 random branch looks like epsilon-greedy exploration; that reading is inferred.

**Evidence.** All addresses are TACP.EXE, ISO non-4 generation (CRC 0xfebbe39e). Physical file offset = code VA + 0x5AAA4; data strings use the data rule VA + 0x4FAA4 (derived, not byte-compared).
- FUN_000a1324 (file 0xFBDC8). Findfirst at 0xA1336 (file 0xFBDDA). DIV by 0x72, then CMP ECX,0x2710 at 0xA1372 (file 0xFBE16). Rename via FUN_000c66c4 (INT 21h AH=56h at 0xC66D0) called at 0xA1388 (file 0xFBE2C). CMP ESI,[ESP+0x2C] at 0xA1420 (file 0xFBEC4). Delete of TMPEXP at 0xA148A. The string at VA 0xE32C4 is TACDATA/TMPEXP.DAT and VA 0xE32AC is TACDATA/EXPERIEN.DAT.
- Debrief FUN_000b3b28 (file 0x10E5CC): MOV EBX,0x87B7C at 0xB3BFD; call FUN_000a1324 at 0xB3C11 (file 0x10E6B5); call FUN_000a15f4 at 0xB3C16 (file 0x10E6BA); MOV EAX,0xE44AC ('TACDATA/BRAIN.DAT') at 0xB3C20 and MOV EDX,0x1EF058 at 0xB3C1B; call FUN_00013e64 at 0xB3C25 (file 0x10E6C9). Guard: 0xB3BD8 DEMO flag, 0xB3BE1 net flag, 0xB3BEA DAT_0027985C.
- FUN_00013e64 (file 0x6F... see VA 0x13E64, file 0x6F... = 0x13E64+0x5AAA4 = 0x6E908): 56 B, open, FUN_000c4fe6, close. FUN_000c4fe6 (file 0x11BA8A) starts with TEST byte [ECX+0xC],0x2, the stream write flag, so it is fwrite.
- Battle init FUN_00014948 (file 0x6F3EC): byte-store 0x32 loops at 0x14AED/0x14B06 (file 0x6F591/0x6F5AA) with CMP 0x4308 at 0x14AF4/0x14B0D; MOV EAX,0xE0C98 ('TACDATA/BRAIN.DAT') at 0x14B1C (file 0x6F5C0); findfirst at 0x14B23; refit call at 0x14B2C (file 0x6F5D0); MOV EBX,0x87B7C at 0x14B31; save call at 0x14B40 (file 0x6F5E4). 0x87B7C = 25 x 0x56DC = 555,900.
- FUN_000a15f4 (file 0xFC098, 3,315 B). Opens EXPERIEN.DAT at 0xA1B23 and 0xA208B; three passes of reads of 0x72; exp(-0.0004 x age) via f2xm1/fscale. Callers: 0x14B2C (FUN_00014948) and 0xB3C16 (FUN_000b3b28).
- Flush gate in FUN_00095490 (file 0xEFF34): call FUN_000a12a4 at 0x9556F (file 0xF0013), compare with DAT_002A1760 at 0x9556B, DAT_001B7BAC test at 0x95574, call FUN_0009ec78 at 0x9557D. FUN_000a0fb0 callers: 0x9022E, 0x98DB2, 0x9A6E5, 0x98569, 0x99D23, 0x9B819.
- Argv: 'DEMO' (VA 0xE00EC) compared at 0x10442, stores DAT_0027A805=1 at 0x1044B; 'TRAIN' (VA 0xE00F4) compared at 0x10462, stores DAT_000E6D66=1 at 0x1046B.
- Depot data (depot_7661/XCOMA/TACDATA, Steam copy, mtimes Aug 22 21:36): EXPERIEN.DAT 899,346 B = 7,889 x 114, state byte 2 in all 7,889; ids 1..11, 35, 51 and 101..112; n in {25, 17, 16, 12, 9}; outcome in 0..5. WEAPEXP.DAT 34,320 B, 34,194 bytes equal 0x32. BRAIN.DAT 555,900 B = 25 x 0x56DC, header 0x65, 0x19, 0x06. The depot files are shipped initial state, not played copies.
- UFO2P (file = VA + 0x626A4): FUN_000a57fc references the strings at VA 0xD32D4, 0xD32E8, 0xD32FC, 0xD3314 and 0xD332C; it is called only from FUN_000b94c0 at 0xB94CA.
- Engine: no match for the exact patterns EXPERIEN.DAT, WEAPEXP, BRAIN.DAT, TMPEXP, BRAIN0 in game/, framework/, library/, tools/ or data/ in either tree. Docs hits at HEAD:docs/campaign-plan.md:1177, 1229, 1247-1249 and LIVE:docs/campaign-plan.md:1192, 1244, 1262.
- Not done: the feature builders (FUN_0009a77c family), the outcome classes, and how FUN_000a2474 is parameterised. The UFO2P copy/restore roles are inferred from string references.

**Fix (target both).** Docs only; no C++ change now.
1. Edit docs/campaign-plan.md in HEAD (lines 1177, 1229 and the section 16 intro/16.1 text at ~1244-1262 including 'It does not, and it never did' and 'The decision-making is identical in week 1 and week 30') and apply the same edit to LIVE (offsets +15, uncommitted: coordinate with the other agent rather than overwriting).
   - Replace the claim with: the original's alien action selection is driven by a learned model.
   - EXPERIEN.DAT is the sample log, capped to the newest 10,000 records at each debrief.
   - BRAIN.DAT is the 25-slot model, refit from the whole log with a time-decayed weighting and rewritten at every single-player debrief when new samples were logged.
   - WEAPEXP.DAT is two 0x4308-byte tables.
   - The state is saved and restored with the campaign (the UFO2P roles are inferred).
   - The engine's heuristic AI is a known divergence.
   Fix the stale processExperience '#L4171' link and the 'FIXME: Ensure correct' claim, which is not in the code. Keep the soldier award (FUN_000b3f74) described separately.
2. Engine: do not implement. This is blocked on decoding the 25 features per sample id, the outcome classes, and the effect on the action mix.
3. Decisive next check: decode FUN_0009a77c and its callers (the feature builders) and FUN_000988e0 / FUN_000999e8. Alternatively, measure the alien action-mix change between the shipped BRAIN.DAT and one refit after N battles.
4. For the Superhuman plan: do not assume alien tactics are static across a campaign.

## v_victory_chain / VC-1 -> diverges (high, high)

**Disputed point.** Whether VC-1 'match' survives once the success flag UFO2P [0x244084]==1 (the validator's INFERRED 'battle success') is tied to its TACP producer. Engine lens: the success predicate differs (EXE: objective complete; engine: playerWon = kill-all). EXE lens: the chain and building identity match, and the success predicate is a separate tactical finding.

**Ruling.** The engine lens is right: the verdict should be 'diverges'. Both lenses agree on the EXE facts. They disagree on scope. The victory trigger is the f=9 building identity AND the raid-success predicate. VC-1's own EXE chain and its 'strategy_rule_affected' text depend on the success flag. The validator left that flag INFERRED and listed its TACP producer as open question 4. I read it, and it is not the engine's playerWon. No other finding covers the alien-building raid-success predicate (BD14a covers base defence only), so scoping it out would drop a high-impact gap. Split: building identity matches (word at building+0xAA == 9, name table index 9, albuild position 9 are all the Dimension Gate Generator, and the chain b32ac -> b3d4c -> 60f24 -> 61214 is single-caller at every link). The success predicate diverges. The EXE grants success for an alien-owned, type-1 raid only when the map's objective tiles are destroyed. The engine grants it only when no hostile normal unit is alive and a conscious, non-retreated player unit exists.

**Engine.** GameWon is raised only if Battle::playerWon is true and the building has victory set (HEAD battle.cpp:3573 gate, :3581 victory = building->victory, :3652-3654 GameWon; LIVE :3573, :3588, :3659-3661). playerWon is assigned only in Battle::checkMissionEnd (battle.cpp:2235/2240). It is true iff the player org has a conscious unit and no hostile org has a conscious 'normal' unit. A retreated unit is not conscious (battleunit.cpp:1511-1516), so a full evacuation gives playerWon=false. Objectives (tryDisableBuilding, battle.cpp:2272ff; checkIfBuildingDisabled raises a BuildingDisabled event) never feed playerWon or exitBattle; buildingDisabled is read only for the end-of-mission message (battleview.cpp LIVE:2271, HEAD:2219). The engine can therefore win a Gate raid with the generators intact (kill every alien). It cannot win by destroying them and evacuating: no unlock, no collapse, no score, no victory. The same rule applies to all ten alien-building raids. The engine's 48gate objective set {71,73..85} (extractors.cpp:131) differs from the EXE row {76,77,78,138}. Rows for 42megapd, 44organ and 47maint also differ from EXE rows f3, f5 and f8. Rows f0, f1, f2, f4, f6, f7 match.

**EXE.** For a raid in the alien dimension, on an alien-owned building ([0x1ED2F4]==1) with mission type 1 ([0x1ED2CC]==1), the TACP result flag [0x1ED32C] (the scenarir.dat result flag; it is [0x244084] in UFO2P) is 1 only if the objective check FUN_00025310 passes. That check is the 'no objective tile left' scan of the map for the building's function index. FUN_00022f90 (the evaluator) sets 1 when X-COM is alive and no hostile is alive, then clears it to 0 if the objective is incomplete, so the mission keeps running even when every alien is dead. If X-COM has no units but hostiles remain, it sets -1, then upgrades to 1 if the objective is complete. FUN_00015494 (the result writer) unconditionally sets flag=1 when owner==1, type==1 and the objective is complete. It runs on every mission exit, because the tail of FUN_00010294 calls it whenever [0x1ED2C8]!=0. Destroying the generators and then evacuating therefore succeeds, which matches the in-game briefing. Killing all aliens without destroying the generators does not. UFO2P FUN_000b32ac then requires [0xD5060]!=0 and [0x244084]==1 before calling FUN_000b3d4c(building), which calls FUN_00060f24 -> FUN_00061214 only if the word at building+0xAA == 9. Alien-dimension raids are type 1: the launch call at VA 0x7186B passes kind 0 when [0xD5060]!=0.

**Evidence.** All offsets are physical offsets in ISO non-4 files. I read the TACP.EXE bytes and tables myself (CRC 0xfebbe39e), and ran a Ghidra copy for FUN_00010294. 1) TACP FUN_00015494 (entry file 0x6FF38): raw bytes at 0x70227 are 8b3df4d21000 83ff01 0f856a000000 3b3dccd21000 7562 e871fb0000 85c0 7406 893d2cd31000, i.e. mov edi,[0x1ed2f4]; cmp edi,1; cmp edi,[0x1ed2cc]; call 0x25310; if nonzero then [0x1ed32c]:=edi. 2) TACP FUN_00022f90 (entry 0x7DA34): the bytes at 0x7DB25-0x7DBAA match the flag logic in the engine-behaviour and exe-behaviour fields. The Ghidra decompile drops these blocks as 'unreachable', so the listing is the evidence. 3) TACP FUN_00025310 (entry 0x7FDB4): jump table at file 0x7FD8C holds ten dwords, f=1 -> 0x15442 (Queen check FUN_00025740) and every other f -> 0x15331 (tile scan against row 0x281FA8+f*0x58). Objective table at file 0x2D1A4C, stride 0x58, 22 dwords per row. Row 9, at file 0x2D1D64, is {76,77,78,138}. The latch [0x27AD14] makes the check return 1 immediately for a map with no objective tiles. 4) TACP FUN_00010294 tail: VA 0x10A89 'cmp [0x1ed2c8],0; call 0x15494' (call at file 0x6B536). FUN_00015494's only caller is FUN_00010294, so every mission exit passes through it. 5) Block offsets match: type +0x38 (TACP 0x1ED2CC = UFO2P 0x244024), function index +0x5C (0x1ED2F0 = 0x244048), owner +0x60 (0x1ED2F4 = 0x24404C), flag +0x98 (0x1ED32C = 0x244084); the block is 0xD4 bytes, written by TACP and loaded by UFO2P FUN_000b1d9c. 6) UFO2P raid launch (FUN_000ac08c call sites): 0x7186B (file 0xD3F0F) passes EDX=0 when [0xD5060]!=0, else 3. The other sites pass 5 (base defence), 1 (UFO) or 0 (0x71503). FUN_000aff9c maps kind 5 to type 2, kind 3 to type 4, kind 1 to type 3 and anything else to type 1. No path launches kind 3 in the alien dimension. 7) UFO2P chain as in the validator: FUN_000b3d4c entry file 0x1163F0, cmp [edi+0xAA],9 at 0x116482, call at 0x11648C; FUN_000b32ac entry 0x115950, call at 0x1159AF. Engine side verified in the files above (LIVE and HEAD identical for this logic). Notes at validate/adj_v_victory_chain_VC-1.md.

**Fix (target both).** 1) In Battle::exitBattle, RaidAliens branch (HEAD battle.cpp:3573, LIVE :3573), replace the playerWon gate with objective success: success = buildingDisabled || tryDisableBuilding() (true also when the map had no objectives to begin with, which matches the EXE latch). Use it for the unlocks, collapse, cat-7 score and the victory assignment (victory = building->victory && success). Keep playerWon for loot (:2884) and for the lose path. 2) Make the mission-end rule for alien-dimension raids match FUN_00022f90 and FUN_00011c14. 'All hostiles dead' must not end the battle (and so cannot give success) while the objective is incomplete. Retreat after the objective is complete must still count as success, so evacuating after buildingDisabled succeeds. The battleview message at battleview.cpp:2271 already treats buildingDisabled as a win. 3) Correct the objective sets for 42megapd (f3 {213..216,223..226}), 44organ (f5 {138..158}), 47maint (f8 {142,143,144}) and 48gate (f9 {76,77,78,138}) in extractors.cpp:121-131. Rows f0, f1, f2, f4, f6, f7 already match, which supports the same tile-id space. 4) Add a headless test: a gate map with generators destroyed and all units evacuated gives GameWon, and an all-aliens-dead map with generators intact does not. LIVE adds a cat-7 score block inside the same branch (battle.cpp:3578-3587), so coordinate edits with the in-flight work.

## v_victory_chain / VC-12 -> diverges (low, high)

**Disputed point.** Whether the engine gives an arriving craft any order in the alien dimension (validator: "Absent: arriving craft take no target", engine_missing, medium), and whether the building auto-order affects the first arrival.

**Ruling.** The refuter is right and the validator's engine half is wrong. The EXE half holds: both sides agree on it, and I re-read the saved Ghidra listing of FUN_0006eea4 myself.

The engine does order arriving craft. ArriveFromDimensionGate case 0 ends in takePositionNearPortal, which queues a random gotoLocation within +/-2 tiles of the portal (LIVE:game/state/city/vehiclemission.cpp:2693-2708 and 3580-3585; HEAD:2672-2696 and 3432-3438). The same case issues gotoBuilding only for the player's craft arriving in CITYMAP_HUMAN. That equals the EXE's branch 2, a one-shot goto (+0xCF=2) to a random point around the arrival position (-5 to +4 tiles).

What is truly missing is branch 1. The EXE sends the craft to the first building in array order that passes FUN_0004a33c, with the entrance tile picked by FUN_0004db84. The engine has no such code anywhere in the arrival path.

The "medium" strategy impact does not hold, for two reasons. First, FUN_0004a33c needs the completed byte of research record 88+f to be non-zero and the building not yet destroyed. At the first crossing no alien-building research can be complete. Records 88-97 other than 92 can only be unlocked by destroying the previous building, and record 92 needs record 1, which the first crossing makes available. So the first arrival also takes the random-point fallback in the original. Second, the building order only saves the player a click on later arrivals. Nothing in game state changes except flight start time, and the harness already clicks BUTTON_GOTO_BUILDING (tools/oa_play.py raid_alien_building).

Two wording errors in the finding: the fallback is a one-shot goto, not a "patrol". Also, when FUN_0004db84 returns valid=0 the EXE issues no order at all and does not fall back.

**Engine.** On arrival in any city, ArriveFromDimensionGate case 0 calls leaveDimensionGate, then branches. A player craft arriving in CITYMAP_HUMAN gets gotoBuilding. Otherwise, when incursionZoneMode >= 0, takeIncursionSpawnPosition runs. In the default case (zoneMode -1, which the player's crossing uses at LIVE:vehiclemission.cpp:1432), takePositionNearPortal queues gotoLocation to a random tile within +/-2 of the portal. There is no building targeting. HEAD and LIVE are identical here; LIVE's other edits to the file are unrelated.

**EXE.** FUN_0006eea4 (UFO2P non-4, file 0xD1548) is called from FUN_0006d384 for each vehicle placed in the city after a crossing, with no owner test. It places the craft at the gate or a random clear tile and resets its state. If the current city byte [0xD5060] is 0 it calls FUN_00051348. Otherwise (alien dimension) it loops buildings in array order (stride 0xE2, count word [0x183A68]) and calls FUN_0004a33c on each. FUN_0004a33c returns 1 iff the building's destroyed marker is zero and the completed byte of research record 88+f is non-zero. On the first hit it calls FUN_0004db84 for the nearest entrance tile. If valid, it sets +0xCF=0 and +0xD0=idx and calls FUN_0004e0d4 to set the destination. If valid=0 it issues no order. If no building passes, it sets +0xCF=2 with x/y = rand(10)+pos-5 (a one-shot goto to a random point) and calls FUN_0004e0d4.

**Evidence.** EXE, from the validator's saved Ghidra listing at validate/v_victory_chain_work/call3.txt lines 291-620 (UFO2P non-4, file offset = VA + 0x626A4):
- FUN_0006eea4 is at file 0xD1548.
- The [0xD5060]==0 branch to FUN_00051348 is at VA 0x6F141-0x6F14A.
- The building loop is at VA 0x6F15A-0x6F17F (file 0xD17FE-0xD1823): IMUL 0xE2, [0x18276C], CALL 0x4A33C, bound [0x183A68].
- The found branch is VA 0x6F1E6-0x6F22B: it calls FUN_0004db84, tests valid=0 at [ESP+6], then sets +0xCF=0 and +0xD0=DI.
- The fallback is VA 0x6F191-0x6F1E4 (file 0xD1835-0xD1888): +0xCF=2, rand(10)+x-5, rand(10)+y-5.
- FUN_0004a33c is at file 0xAC9E0 (call2.txt lines 493-570): each handler checks "marker byte == 0 and the completed byte of record 88+f != 0".
- Research data, validator section 2: record 92 (Sleeping Chamber) has prereq [1], and the other alien-building records 88-97 are unlock-only (u2=4), so no building passes at the first crossing.

Engine:
- LIVE:vehiclemission.cpp:2693-2708 (case 0 branches) and 3580-3585 (takePositionNearPortal).
- LIVE:vehiclemission.cpp:1432 (default zoneMode on the player's crossing).
- HEAD:vehiclemission.cpp:2672-2696 and 3432-3438 are the same code.
- A raidable predicate already exists as nextRaidableAlienBuilding, at LIVE:game/state/gamestateintrospect.cpp:700 and HEAD:466 (accessTopic->isComplete() and isAlive()).
- The harness issues its own order in tools/oa_play.py raid_alien_building.

**Fix (target both).** In VehicleMission::ArriveFromDimensionGate case 0, add a branch before the takePositionNearPortal fallback. If v.city.id == "CITYMAP_ALIEN", walk the city's buildings and take the first alien-owned building with accessTopic->isComplete() && isAlive(). For that building, queue VehicleMission::gotoBuilding(state, v, building). Otherwise keep the existing takePositionNearPortal fallback. Optionally widen that fallback from +/-2 to the EXE's -5..+4 range.

Do not reuse nextRaidableAlienBuilding as the core rule, because it sits in the harness introspection layer. Move the predicate into game/state/city or Building and have both call it. Keep the first-arrival behaviour (nothing researched, so random point), and keep the existing human-city gotoBuilding branch. This is a low-priority convenience, because the harness already issues the order.

## v_victory_chain / VC-16 -> diverges (low, high)

**Disputed point.** Does the EXE reset the alien-dimension fleet on every visit (validator: counts, respawned full-health, kill effect unobserved, blocked on RE, medium impact, rebuild as counts), or is the fleet size a persistent stock that kills, incursions and growth act on, with only survivors' health and position reset (refuter)?

**Ruling.** The refuter is right on mechanism, impact, blocked_on_re and fix. The validator is right that the label is `diverges` and that the EXE respawns survivors fresh. I checked the key bytes and decompile myself.

1. Kills do decrement the stored count, so it is not blocked on RE. FUN_00058280 is the vehicle-removal routine. For an owner==1 vehicle with type word at +4 below 10, it does `DEC word [byte[0xD5060]*0x2D4 + type*2 + 0x143A6C]` (file 0xBA971-0xBA996). The destroy handlers FUN_0005f700 (0xC1DA4) and FUN_0005f894 (0xC1F38) bump kill stats and then call it. Alien gate crossings in FUN_0003a910 also do stock[dest]++ then this removal. Incursions subtract from the dimension stock: FUN_0006cfb4 does `SUB word [type*2+0x143D40]` at 0xCF800. Weekly growth (FUN_0006edac) adds to it. That the two death handlers are the in-city kill path is an inference from the kill-stat writes.

2. Leaving does not shrink the stock, and entering respawns survivors fresh. FUN_0006e098 calls FUN_00070694 (0xD2D38) when it leaves a non-zero city (0xD08A9-0xD08B7). That routine only writes word[+2]=0xFFFF on that city's alien slots and has no write to 0x143A6C. On entry to a non-zero city (CMP [0xD5060],0 at 0xD0BA2), the loop at 0xD0BC0-0xD0C17 spawns (stock[type] - live of that type) via FUN_0006de64. Those vehicles are freshly initialised: FUN_0005d6e4 copies vehicle_data +0x2E into slot +0x12E and sets the owner at +0x12C (0xC0280-0xC02AA). The fleet size is the persistent stock; only health, shield and position are reset.

3. The cleanup loop the validator cited (FUN_000105b8, 0x72E56-0x72E94) cannot normally drain the stock. The outer loop handles city 1 and then city 0 on each pass. The city-0 step calls FUN_0006e098 when the loaded city is non-zero (0x72D83-0x72D98), which does the silent unload first. By the next pass the loaded city is 0, so city 1 has no live alien slots. FUN_00058280 also decrements stock[[0xD5060]], the loaded city rather than the vehicle's city, so a straggler would hit stock[0]. The only way to reach a draining cleanup is a pass that re-enters with [0xD4CBC]!=0 and no X-COM craft left in city 1. That is an edge case I could not prove reachable. It does not change the ruling.

4. The engine has the same pool semantics. Alien vehicles in CITYMAP_ALIEN are the pool. GameState::invasion takes them (LIVE:gamestate.cpp:902, 928-1000, sets city to the human city and heals via equipDefaultEquipment). updateUfoGrowth adds capped by UFO_GROWTH_LIMIT minus the fleet present (LIVE:1617-1688). Killed vehicles are removed. The real difference is that setCurrentCity (LIVE:331-337) resets nothing, vehicles outside the current city are not updated (LIVE:1360-1375), and nothing heals them. Hourly repairVehicles (LIVE city.cpp:778) only touches vehicles in a building with home==current, and placeVehicle(position) spawns airborne with no home building (city.cpp:1003). The daily guard that skips CITYMAP_ALIEN covers only scenery. So damaged survivors stay damaged and keep their positions. Health is written only on damage (vehicle.cpp:2730), crash/die, and equipDefaultEquipment (:3952).

5. Impact is low, not medium. Shooting UFOs in the dimension is worthwhile in both. Available stock is growth minus incursions minus kills in both. Only wound-and-return tactics differ, and the engine is easier for the player there.

6. Unresolved: the swap cadence. The EXE appears to swap cities on every outer pass whenever X-COM craft are in the dimension, rebuilding the fleet each time it swaps into city 1. The pass period (d4d62>>16) is inferred to be a day, consistent with the engine's day-change switch at LIVE:cityview.cpp:2846-2891, but not proven. I also did not trace whether a shot-down UFO that has not yet self-destructed still counts in the engine pool.

**Engine.** Alien-dimension UFOs are ordinary Vehicle objects in CITYMAP_ALIEN. Kills remove them, invasion() consumes them (LIVE:gamestate.cpp:902), and updateUfoGrowth adds capped growth (LIVE:1617-1688). setCurrentCity (LIVE:331-337, HEAD:330-337) resets nothing. Survivors keep their damage and positions across visits and are not updated while another city is current (LIVE:1360-1375). Health is written only on damage, crash/die and equipDefaultEquipment. Hourly repair only reaches vehicles docked in a home building, which dimension UFOs are not.

**EXE.** The dimension is a persistent per-type stock at VA 0x143D40 (0x143A6C + city*0x2D4). Kills, gate crossings and incursions decrement it, and weekly growth adds to it. On switching out, FUN_00070694 frees the live alien slots silently with no stock change. On each switch into city 1, FUN_0006e098 spawns (stock - live) fresh vehicles via FUN_0006de64 at full HP, with new positions and missions. The fleet size persists; survivors' health, shield and position are reset on every swap into the dimension. The FUN_000105b8 cleanup loop is normally a no-op because the silent unload runs first.

**Evidence.** All offsets are UFO2P.EXE physical file offsets, and I read the listings and decompiles in the validator's and refuter's raw outputs rather than re-running Ghidra. Kill decrement: FUN_00058280 DEC at 0xBA996 (decompile condition owner==1, type<10, stock[d5060][type]--); kill callers FUN_0005f700 at 0xC1DA4 and FUN_0005f894 at 0xC1F38. Silent unload: 0xD08A9-0xD08B7 into FUN_00070694 at 0xD2D38, which writes word[+2]=0xFFFF and nothing to 0x143A6C. Top-up: CMP [0xD5060],0 at 0xD0BA2, then 0xD0BC0-0xD0C17, with type read as word +4 at 0xD0BE6 and owner 1 passed at FUN_0006de64 (0xD0508). Fresh HP: 0xC0280 sets +0x12C and 0xC02AA sets +0x12E from vehicle_data +0x2E. Incursion SUB: 0xCF800. Outer loop order and switch-back: 0x72D83-0x72D98, cleanup loop 0x72E56-0x72E94. Engine: LIVE:game/state/gamestate.cpp:331-337, 902-1000, 1360-1375, 1551, 1617-1688; LIVE:game/state/city/city.cpp:331-335, 778-815, 1003; LIVE:game/state/city/vehicle.cpp:2730, 3952; LIVE:game/ui/tileview/cityview.cpp:2846-2891. HEAD line shifts only: setCurrentCity HEAD:330, invasion HEAD:901, updateUfoGrowth HEAD:1584. Raw sources: scratchpad/validate/r15/call1-5.txt and scratchpad/validate/v_victory_chain_work/call2.txt, call5.txt and call6.txt.

**Fix (target both).** Do not rebuild the dimension as counts. In GameState::setCurrentCity, when the new city is CITYMAP_ALIEN, set health = getMaxHealth() and shield = getMaxShield() for every alien-owned vehicle in that city. Optionally also reposition survivors and clear their missions, as the EXE's FUN_0006de64 does. This runs on the day-switch path (LIVE:cityview.cpp:2891), which matches the EXE's per-swap rebuild. Keep wound-and-return strategies disabled in the harness. Open sub-checks: bind the pass period of FUN_000105b8/FUN_00010908, and whether the [0xD4CBC]!=0 re-entry can reach a draining cleanup.

## v_ufo_consumers / F2_ufo_incursion_package_fit -> diverges (low, high)

**Disputed point.** Whether the engine's fleet-fit test in GameState::invasion() is equivalent to UFO2P.EXE FUN_0006cfb4's fit test (validator: match, "same algorithm"). The engine subtracts the package's primary, attack and escort counts cumulatively from a per-type vehicle limit. The EXE tests each of the 3 slots independently against the pool.

**Ruling.** The refuter is right: not a strict match. The structure matches (priority order, the type ranges, no retry, timer reset regardless, all 45 records), but the fit test differs for any record that repeats a craft type across slots. That is exactly one record, index 3 = UFO_INCURSION_I4, so the real impact is low.

I checked the EXE bytes myself. Disassembling UFO2P.EXE non-4 at file 0xCF765-0xCF80F with capstone gives, for each slot 0..2:
- load craft[slot]; if it is -1, count the slot as passed;
- otherwise `mov bx,[pool+craft*2]; cmp bx,[rec+slot*2+6]; jl skip` (signed word compare against count[slot]); on pass `mov esi,edi`, so ESI is a pass counter;
- after the 3 slots, `cmp si,3` decides.

The loop never decrements the pool and keeps no running total. Only after a package is chosen does the loop at 0xCF7ED-0xCF80D do `sub word [pool+craft*2],count` once per slot, with no clamp.

I read the data directly from the bytes at file 0x13DDFC. Record 3 = craft (8,7,8), count (1,1,1), role (7,11,5). A scan of all 45 records finds record 3 is the only one that repeats a craft, so the divergence is confined to it. The ufo_incursions.xml dump gives UFO_INCURSION_I4 = primaryList BATTLESHIP 1, escortList ESCORT 1, attackList BATTLESHIP 1. The lists are `std::vector<std::pair<UString,int>>`, and `invasion()` does `limits[v.first] -= v.second` over all three lists, so the engine needs Battleship >= 2 and Escort >= 1. The EXE needs Battleship >= 1 and Escort >= 1.

The window is narrow. Mothership has to be 0, which fails I1 and I2. I3 (Battleship 1, Escort 2) has to fail, so Escort is exactly 1. Battleship has to be exactly 1, otherwise both rules agree, and the roll has to be Infiltration. In that state the EXE launches I4 and takes pool[Battleship] from 1 to 0 to -1. The engine rejects I4 and falls through to later Infiltration packages.

The EXE's spawn routine FUN_0006da88 scans for a free vehicle record (`high word == -1` at VA 0x160FD8) and fills it from per-type template tables, so it creates hulls on demand. A phantom second Battleship is therefore likely, and a negative pool then blocks Battleship packages until growth refills it. That second point is inferred from the listing, not traced end to end.

The validator's own sentence "Equivalent, given that the engine's three lists are the EXE's three slots split by role" is the mistake: splitting by role does not preserve the per-slot rule. The refuter's scope note is also correct. The fit loop is skipped when the Control Chamber flag DAT_000f3c78 is set, which is F5 and a separate finding.

**Engine.** LIVE:game/state/gamestate.cpp:946-990 (HEAD:945-989) is identical code, shifted by one line. It builds `priority -> incursion` filtered by primaryMission. For each incursion in priority order it copies `vehicleLimits` (count of ORG_ALIEN vehicles in CITYMAP_ALIEN per type) and subtracts every entry of primaryList, attackList and escortList cumulatively (LIVE :958-970, HEAD :957-969). It rejects the package if any per-type limit goes negative, so the test is the sum of needed counts per type <= available. The first fit wins. If none fits it returns, and the timer was already reset (LIVE :910). The spawn loops at LIVE ~:1012-1160 take `invaders[type].front()` and pop it, which is undefined behaviour on an empty list if a package were ever accepted with a shortfall. The engine pool can never go negative.

**EXE.** UFO2P.EXE non-4, FUN_0006cfb4 (file 0xCF658). For idx from the type's range start to its limit, rec = UFO_mission_data + idx*0x2a. It passes a slot if craft == -1 or `pool[craft] >= count[slot]` (signed word compare, no running total). It selects the record if all 3 slots pass (`cmp si,3`). On selection it does `pool[craft] -= count[slot]` per slot, unclamped and sequential, so a repeated craft can drive the pool negative. No retry; the caller resets the timer either way.

**Evidence.** OBSERVED, UFO2P.EXE ISO non-4, file offsets (code file = VA + 0x626A4):
- Fit loop, 0xCF785-0xCF7BC: per-slot `movsx ebp,[eax]` (craft); `cmp ebp,-1; je pass`; `mov bx,[ebp*2+0x73d40]` (pool, operand = VA 0x143D40 - 0xD0000); `cmp bx,[eax+6]` (count[slot]); `jl skip`; `mov esi,edi` (pass counter); `inc edx; cmp dx,3; jl loop`; `cmp si,3; jne next-record`. Capstone disassembly of the canonical file, matching validate/v_ufo_consumers_work/call2.clean lines 386-420. No pool write inside the loop.
- Pool decrement, 0xCF7ED-0xCF80D: `sub word [ebx*2+0x73d40],dx` per slot with craft != -1.
- Record 3 at file 0x13DE7A: `0800 0700 0800 | 0100 0100 0100 | 0700 0b00 0500 ...` = craft (8,7,8), count (1,1,1), role (7,11,5). A scan of all 45 records finds this is the only one with a repeated craft.
- docs/original-game/exe-tables/ufo_incursions.xml lines 668-708: UFO_INCURSION_I4 = primary BATTLESHIP 1, escort ESCORT 1, attack BATTLESHIP 1, priority 4.
- Extractor: tools/extractors/extract_ufo_incursions.cpp puts role 11 in escortList, role 5 on a non-Attack mission in attackList, otherwise primaryList. The lists are `std::vector<std::pair<UString,int>>` (game/state/rules/city/ufoincursion.h:57-59).
- Engine cumulative subtraction: HEAD:game/state/gamestate.cpp:957-979; LIVE:958-980. The invasion() region is byte-identical between HEAD and LIVE (diff of lines 880-1000 vs 881-1001 is empty). Only the line offset differs.
- Worked trace with M=0, E=1, B=1: EXE slots (B>=1, E>=1, B>=1) all pass, so I4 launches and pool[B] goes 1 -> 0 -> -1. Engine limits[B] = 1-1-1 = -1, so I4 is rejected.
- Growth table at file 0x155010 (caps 15,15,6,6,6,6,6,6,4,2): Escort gains in weeks 6,7,8,9,12,14,16+; Battleship in weeks 7,9,11,13,15,16+; Mothership in weeks 8,11,14,16+. So the state B=1, E=1, M=0 is reachable, mostly after hull losses. This was not simulated.
- FUN_0006da88 (file 0xD012C, call2.clean 1063-1130) scans for a free vehicle record and initialises it from type templates. It does not recycle a pool vehicle. The phantom hull is INFERRED from this, not traced end to end.
- No Ghidra copy was created for this adjudication, so there is nothing to delete.

**Fix (target both).** In `GameState::invasion()`, change the fit test to match the EXE. For each entry in primaryList, attackList and escortList, require `vehicleLimits[type] >= count` independently, with no cumulative subtraction. The first priority-ordered package where every entry passes wins.

Then handle the overdraw at spawn. When `invaders[type]` is empty or shorter than the count, either create a new hull of that type (EXE parity: FUN_0006da88 allocates one) or cap and skip it. Guard `invaders[type].front()` and `pop_front()` either way, because they are undefined behaviour on an empty list. To track the EXE's negative pool exactly the engine would also need an overdraw-debt counter, since pool[Battleship] goes to -1 in the EXE. That is optional given the narrow window.

Do not add an engine-side workaround in the data. Fix both trees; the region is identical. Priority is low: one record, narrow state.

## v_ufo_consumers / F9_alien_craft_gear_score_gates -> engine_bug (medium, high)

**Disputed point.** Whether the engine's alien-craft gear gate (shields, cloak, teleporter on lifetime UFO-kill score) is applied to every UFO at launch the way the EXE applies it. The validator said match because Vehicle::equipDefaultEquipment is "called at launch" (gamestate.cpp:1034). The engine-lens refuter said it is called only in the primary-list launch loop and never for escort-list or attack-list ships.

**Ruling.** The refuter is right and the validator's "match" is overturned. The thresholds, columns, quantity (lifetime craftShotDownUFO) and EXE table were all correct and are not in dispute. The defect is that the engine skips the gate for escort-list and attack-list launches. In the EXE every launched craft is created by one routine with owner 1, so the gate applies to every slot of an incursion package. Strategy rule corrected: at Superhuman, primary-list UFOs launched after the thresholds get gated gear (small shield 1000, large shield 2000, cloak 4000, teleporter 8000 kill-score). Escort and attack-list UFOs get it only if they were created, last ported out, or last launched as primary after the threshold was crossed. A pool ship created at score 0 never has gated gear on its first escort or attack sortie.

**Engine.** GameState::invasion() launches three lists. equipDefaultEquipment (which clears equipment and loot, skips alien items while totalScore.craftShotDownUFO < etype->scoreRequirementFor(difficulty), then restores shield and health) is called only in the primary loop: LIVE gamestate.cpp:1034, HEAD :1033. The escort loop (enterDimensionGate at LIVE :1101, HEAD :1100) and the attack loop (LIVE :1154, HEAD :1153) call enterDimensionGate and setMission but never equip. A grep of equipDefaultEquipment in LIVE shows only three callers: gamestate.cpp:1034, city.cpp:984 (City::placeVehicle, used by updateUfoGrowth) and vehiclemission.cpp:1437 (port-out into the alien city, HEAD :1434). Vehicle::enterDimensionGate and leaveDimensionGate do not equip. Escort and attack ships therefore keep the gear from pool creation or their last port-out. Shipped incursion data (45 packages) has 116 primary, 54 escort and 5 attack ship-slots, so 59 of 175 (34%) launch ungated. 22 of the 54 escort slots are Escort hulls, whose loadout {15,45,46,47} has three gated items. invasion() and Vehicle::equipDefaultEquipment are byte-identical in HEAD and LIVE.

**EXE.** UFO2P.EXE (non-4) FUN_0006d384 runs a spawn state machine over the selected 42-byte package record. It steps slot 0..2 (craft[slot] != -1) with a per-slot count and calls FUN_0006da88 once per craft, with no distinction between primary, escort and attack. FUN_0006da88 finds a free vehicle-table slot and calls FUN_0005d6e4 (vehicle init) with the owner argument as the constant 1. FUN_0005d6e4 stores that owner at vehicle +0x12C, then for owner 1 filters the 45 equipment-id bytes at vehicle_data +0x51 through FUN_000b3e84. That function maps ids 44..48 to latch flags 4..8 and blanks any id whose flag is 0. FUN_00097b90 sets the flags once per game midnight: UFOs Shot Down (week + previous) >= table[row][difficulty], and flags are never cleared. The EXE has no persistent UFO vehicles in a pool; pool[] is a per-type counter, so every launched craft, escorts included, gets fresh gate-filtered gear at creation.

**Evidence.** Engine: grep of equipDefaultEquipment over LIVE (live_src/game) and HEAD (game/) gives exactly the three callers above. Sed range diffs show invasion() (LIVE :902-1174, HEAD :901-1173) and Vehicle::equipDefaultEquipment identical in both trees. Reference ufo_incursions.xml (docs/original-game/exe-tables), summed by list, gives primary 116, escort 54, attack 5. EXE, raw bytes of canonical/UFO2P.EXE (code file = VA + 0x626A4): at file 0xD0187 `bb 01 00 00 00 89 f1` (MOV EBX,1; MOV ECX,ESI) and at 0xD0191 `e8 f2 fb fe ff` (CALL FUN_0005d6e4), inside FUN_0006da88 (file 0xD012C). FUN_0005d6e4 prologue (file 0xBFD88) saves EBX at [ESP+0x24], stores it to [EBP+0x12C] at file 0xC0280, and at file 0xC0475 reads `66 8b bd 2c 01 00 00` (MOV DI,[EBP+0x12C]) then `CMP DI,1` / `JNZ` at 0xC0486. The filter call is `e8 83 60 05 00` at 0xC04A0 (FUN_000b3e84). The validator's saved listings (validate/v_ufo_consumers_work/call2.clean, call3.clean) show FUN_0006da88's only callers are FUN_0006d384 at VA 0x6D4A7 and 0x6D58A, both inside the spawn state machine. Not observed at runtime: no benchmark save is above 1000 kill-score (max 700), so the effect is established by code reading, not by an above-threshold run. The refuter's other F9 checks (9x5 table, jump table, daily latch, no other category-3 writer) were not re-done here and were not disputed.

**Fix (target HEAD).** Add `invader->equipDefaultEquipment(*this);` right after `invader->enterDimensionGate(*this);` in the escort loop (HEAD gamestate.cpp:1100; LIVE :1101) and in the attack loop (HEAD :1153; LIVE :1154), matching the primary loop. A shared lambda or helper for the three loops is cleaner. It consumes no RNG, and the refill of health and shield matches the EXE creating a fresh craft at launch. Add a test: at Superhuman set totalScore.craftShotDownUFO to 2000, pre-place a pool Escort created at score 0, launch an incursion with an escort slot, and assert it carries the large shield; a control arm at 0 asserts it does not. The optional daily-midnight latch from the original finding remains a separate, minor timing refinement (up to 24 h) and is not the main fix.

## v_ufo_consumers / F14_food_chamber_mutant_alliance_reset -> engine_missing (medium, high)

**Disputed point.** The core claim (Food Chamber raid reverses the Mutant Alliance takeover in the EXE, and the engine has no such code) was not disputed. The engine-lens refuter flagged two content errors. (1) The validator labelled `[0x1409C8] := 0` as an unknown flag. (2) The fix sketch (clear takenOver and set two relations) was called incomplete and said to be undone by the engine's own takeover logic.

**Ruling.** Verdict engine_missing stands. Both refuter corrections are right and are folded into the corrected claim below. I re-checked each with capstone on the canonical UFO2P.EXE (CRC 0x4749ffc1). I did not need Ghidra and created no copy.

(1) `[0x1409C8]` is infiltration[24].current. The hourly infiltration routine (file 0xD2364) uses `[ebx+0x6f4c8]` with `add ebx,0xE0` per org. The takeover roll (file 0xD24CD) and the UI display (file 0xEBAFB, shown as raw/2) use the same array. 0x6F4C8 + 24*0xE0 = 0x709C8, and that raw operand is exactly the store at file 0x115A44.

(2) The sketch would not work as written. In the engine, `takeOver()` (organisation.cpp:84-92) returns early only if `rand[0,200) >= infiltrationValue`. It sets `infiltrationValue = 200`, and `updateInfiltration` (:639-642) pins it at 200 while `takenOver` is set. The takeover tick (`updateEndOfFiveMinutes`, gamestate.cpp:1456-1470) skips an org only while `takenOver` is true. If only the flag is cleared and `infiltrationValue` stays 200, the next attempt (125 minutes or less away, organisation.h:19) succeeds with probability 1 and undoes the reset.

Everything else in the finding verified:
- Guard chain: file 0x11596C-0x1159A1.
- `cmp [0x244048],2` at file 0x1159E9.
- The org 24 taken-over byte (+0x2921) cleared at 0x1159FD.
- M[24][X-COM] floored to 100, mirrored to M[X-COM][24].
- `FUN_0005faf0(24,1,-50)`, then M[24][Alien] = -100.
- Name order, with the Food Chamber at index 2 and the score table 400 400 300 500 ... giving it 300.
- Org index 24 = Mutant Alliance.
- The engine extractor names the building function from the same `function_idx` (extract_buildings.cpp:76), so the engine id is BUILDINGFUNCTION_FOOD_CHAMBER.

That `[0x244084]==1` means the raid was won, and that `FUN_0005faf0` skips its ripple in the alien dimension, are not re-derived here. I took them from adj_v_takeover_T4, which reached the same engine_missing verdict on the same EXE block.

Impact stays medium. A taken-over Mutant Alliance sits at relation -100 (Hostile), which stops hybrid hiring (organisation.cpp:575+). The EXE restores it. The effect is limited to one raid type and one organisation.

**Engine.** Nothing clears a takeover and nothing handles ORG_MUTANT_ALLIANCE, in either tree (organisation.cpp is byte-identical). `Organisation::takeOver` (organisation.cpp:84-108) sets `takenOver`, `militarized` and `infiltrationValue = 200`. `takenOver` is written only at :90. It is read at gamestate.cpp:1461/1466/1590, building.cpp:807 and bribescreen.cpp:68. `updateInfiltration` (:639-642) pins 200 while `takenOver` is set. After a won RaidAliens battle, battle.cpp (LIVE 3578-3595, HEAD 3571-3589) runs researchUnlock, `victory = building->victory` and `building->collapse`, with no organisation or relation change. LIVE alone adds a 7-line `destroyedScore` hunk to this block.

**EXE.** `FUN_000b32ac` (file 0x115950) runs when `[0x244024]` is 1, or 4 with `[0x5060] != 0`, and `[0x5060] != 0` and `[0x244084] == 1` (a won alien-dimension raid). After crediting the destruction score (call 0x983dc, eax = 7), if the destroyed building's function word `[0x244048] == 2` (Food Chamber) it does this unconditionally, even if the Alliance was never taken over:
- Clears the org 24 (Mutant Alliance) taken-over byte (org table +0x2921).
- Sets M[24][X-COM] to 100 and copies it to M[X-COM][24].
- Calls `FUN_0005faf0(24,1,-50)`, then forces M[24][Alien] = -100.
- Stores 0 to infiltration[24].current (raw 0x709C8, Ghidra 0x1409C8).

Left untouched: M[Alien][24], the +90 ties to other taken-over orgs, and the daily infiltration history. The EXE's takeover roll is `rand16(100) < infil - 100` (file 0xD2440 onward), so after the reset the Alliance can only be retaken once raw infiltration exceeds 100.

**Evidence.** UFO2P.EXE, ISO non-4 (CRC 0x4749ffc1). Code file = VA + 0x626A4; data file = VA + 0x616A4. Raw data operand = Ghidra VA - 0xD0000. All offsets are physical file offsets. I disassembled canonical/UFO2P.EXE with capstone.
- Block: file 0x11596C-0x115A62, with these key addresses:
  - `cmp [0x244048],2`: 0x1159E9
  - org 24 taken-over byte (+0x2921) cleared: 0x1159FD
  - M[24][X-COM] floor: 0x115A09
  - mirror to M[X-COM][24]: 0x115A1F
  - `FUN_0005faf0` call: 0x115A29
  - M[24][Alien] set to -100: 0x115A3B
  - `mov [0x709c8],0`: 0x115A44
- Infiltration array:
  - hourly routine file 0xD2364-0xD2409: `[ebx+0x6f4c8]`, `add ebx,0xE0` at 0xD23F3, `add esi,0x1B6`, loop to 0x1A
  - takeover roll store at 0xD24CD (`mov [eax+0x6f4c8],0xC8`, with the org byte +0x11 set in the same block)
  - UI read at 0xEBAFB-0xEBB0B (SAR 1)
- Strings: alien-building name table at file 0x14AD23 (Food Chamber is the 3rd entry, index 2). Org name table at file 0x14AF11 (index 24 = Mutant Alliance). Score table at file 0x191B7C = (400,400,300,500,300,250,250,350,300,500).
- Engine: organisation.cpp:84-108, 639-642 and 712-ff; organisation.h:19; gamestate.cpp:1456-1470; extract_buildings.cpp:76; recruit gating in organisation.cpp:575+. The hook is LIVE battle.cpp:3578-3595 and HEAD battle.cpp:3571-3589.
- The `[0x244084]==1` meaning (TACP writes scenario record +0x98 = 1 when the side won; record loaded from scenarir.dat) is from og-research:export/strategy-plan-2026-10-08/validate/adj_v_takeover_T4.md. I did not re-derive it.
- My notes: og-research:export/strategy-plan-2026-10-08/validate/adj_v_ufo_consumers_F14_food_chamber_mutant_alliance_reset.md

**Fix (target both).** In the RaidAliens && playerWon block of battle.cpp, add the following when `building->function.id == "BUILDINGFUNCTION_FOOD_CHAMBER"`. It applies whether or not the Alliance was ever taken over.
- Look up the Mutant Alliance as `StateRef<Organisation> mutant{&state, "ORG_MUTANT_ALLIANCE"}`.
- Set `mutant->takenOver = false`.
- Set `mutant->infiltrationValue = 0`. Without this the Alliance is retaken within 125 minutes or less.
- Set `mutant->militarized = Organisation::militarizedFromType(mutant->organizationType)`. This is inferred: the EXE has no equivalent field, but the engine's `takeOver()` sets it.
- Call `mutant->establishRelation(state.getPlayer(), 100)` and `state.getPlayer()->establishRelation(mutant, 100)`.
- Call `mutant->establishRelation(state.getAliens(), -100)` in that direction only. Leave aliens->mutant alone, and do not call `adjustRelationTo` with a ripple.

Placement: in LIVE, after the `destroyedScore` hunk (the EXE does the score first). In HEAD, after the researchUnlock loop.

Follow-up, separate from this fix: the engine's takeover roll (`rand[0,200) < infil`, `v > 0` suffices) is far easier than the EXE's (`rand16(100) < infil - 100`), so a reset Alliance is retaken much sooner in the engine. That belongs to the v_takeover finding.

## v_market / F-M2 -> diverges (high, high)

**Disputed point.** The core mechanic (week-0 price cut driven by post-decay standing stock vs the engine's weekly sold delta) was not disputed by either refuter. The dispute was over the validator's derived strategy statements: (1) a ~0.2 x max per week full-price selling rate; (2) "dump in lumps, not on a timer is wrong against the EXE for every week-0 item"; (3) the "at least 3 units/wk of a max-40 item is unaffected" inequality.

**Ruling.** Verdict stays 'diverges' and impact stays 'high'. The mechanic, engine description and fix direction are confirmed; the three derived strategy statements are wrong and must be replaced as follows. I confirmed these with my own capstone disassembly of the canonical UFO2P.EXE, my own parse of the EXE tables, and my own Monte Carlo (1500 runs per cell).

(1) The 0.2 x max rate is wrong. At 8/wk a max-40 item realises only 0.835 of base over 26 weeks and ends at 0.629, which is the very rate the finding called full price. In my replay a max-40 item holds full price only up to about 3/wk (n=4: 0.995/0.982; n=5: 0.970/0.915; n=6: 0.927/0.808). A max-20 item holds about 1/wk (n=2: 0.986/0.957; n=3: 0.883/0.710). A max-8 item (Toxigun) has no safe steady weekly rate (1/wk: 0.757/0.509; 2/wk: 0.667/0.500) because a stock of 1-2 never decays and 3-7 barely does. Sparse selling of one Toxigun every 4 weeks realises 0.990 over 26 weeks (0.920 over 52); every 6 weeks realises 1.000 (0.985 over 52). Safe pacing is therefore about 0.075 x max per week for large-max items, and much less for small-max items.

(2) 'Dump in lumps is wrong' is wrong. In the EXE a lump is paid at the shown price: the sale handler adds price x delta to the balance, and the price cut only lands on the next Monday tick. The EXE penalty falls on later sales of that type. After a 100-unit dump the 26-week price is 0.755 (Psiclone-class) and 0.503 (Toxigun), against 0.900 in the engine. The finding never compared a lump with a timer. My cadence replay (same arrivals, sell the whole pile every K weeks, Psiclone-class 8/wk, realised/ideal) gives K=1/2/4/13 = 0.838/0.832/0.848/0.874 in the EXE, so cadence is neutral to mildly favourable to lumps there. Caveat: the K=13 edge is partly a horizon artifact, because fewer Monday ticks follow the last lump, so do not read it as 'lumps win'. The engine inverts this: weekly sales are free (1.000) while lumps cost about 9% at K=4 (0.909) and about 5% at K=13 (0.950). The correct strategic statement is that the EXE punishes cumulative volume per item type relative to max stock whatever the cadence. Sell a one-off haul in a single lump per type, do not trickle the same type weekly, and expect the engine to make weekly trickles look free when the EXE does not.

(3) Typo: 'at least 3' should read at most 3 per week for max 40.

Impact stays 'high': this is the economy lever behind manufacturing and loot income, and the 4 frozen aequip types (Psiclone 4500/40, Elerium 3000/40, AG missiles 900/30 and 700/15) are the main raid-loot items. Horizon caveat: over 6 weeks the EXE loss is only about 1-10% (Psiclone-class 8/wk 0.993; Stasis max-20 8/wk 0.943; Toxigun 4/wk 0.895). It grows to 18-27% by 13 weeks and 33-40% by 26 weeks, so it matters for campaign-length play but little for a 2-6 week benchmark.

**Engine.** LIVE and HEAD economyinfo.cpp:16-88 are byte-identical (diff clean). EconomyInfo::update(state, xcom) takes the X-COM branch only when item->manufacturer == player. updateItemMarket is at HEAD:game/state/gamestate.cpp:1659, with the manufacturer test at HEAD:1674, and at LIVE:game/state/gamestate.cpp:1692, with the test at LIVE:1707. The function bodies, extracted by name from each tree and diffed, are identical (updateEndOfWeek too). Rest of the X-COM branch (economyinfo.cpp:29-61): soldThisWeek = max(0, currentStock - lastStock), read before decay; lastStock = currentStock; engine decay is rnd<30 -> x0.80, rnd<60 -> x0.66, else unchanged. The price is multiplied by rand 95..97% if sold > max/2, 90..95% if sold > max, and 85..95% if sold > 2*max, with the tiers not cumulative and the cuts rounded with lround. The result is clamped to [base/2, base]. The non-X-COM branch requires weekAvailable != 0. So week-0 items not made by X-COM (aequip Psiclone, Elerium, the two AG missiles, plus 11 craft; 15 priced items in all) match neither branch and are frozen at base price forever. A one-off dump is penalised once, to about 0.899.

**EXE.** The EXE selects the rule by the record's first word, week == 0 (FUN_0009667c, cmp word [eax],0; je 0x966F5), not by manufacturer. For week 0, the cut is driven by the post-decay standing stock at record +0xE. e=0; if floor(max/2) <= stock then e=rand(0..2)+3; if max <= stock then e=rand(0..5)+5; if 2*max <= stock then e=rand(0..5)+10, with a later tier overriding rather than adding. price = trunc(price*(100-e)/100), then clamp to [base/2, 2*base]. The price never rises for week 0, and lastStock (+0x12) is not read. The Monday tick (FUN_000941dc) calls the stock pass for all rows, then the price pass for all rows (call 0x964D0, then call 0x96614), unconditionally and before the funding-termination test at 0x947CA. The stock decay (FUN_00096538) applies only to rows with min == 0, which is all 78 week-0 rows I checked across the three tables: s -= s/8, then if r>40 s -= s/5, and if r>70 s -= s/3, with r=rand(0..100); the mean survival is about 0.70 per week. A sale adds the sold units to +0xE (FUN_0008f7f4, add [ebp+0xE],eax at VA 0x8FB0F) and the player is paid price x delta at the shown price (FUN_0008e89c, imul at VA 0x8E975).

**Evidence.** All addresses are UFO2P.EXE ISO non-4; I read the canonical copy read-only with capstone, so no Ghidra copy was created (no copies/adj_20 exists). Code file offset = VA + 0x626A4; data file offset = VA + 0x616A4. Prologue 53515256575589c3 confirmed at file 0xF8D20.
- Price tick FUN_0009667c, file 0xF8D20 (VA 0x9667C). Week test at VA 0x96684. Tier compares at VA 0x96709, 0x96727 and 0x96743 (cmp threshold,stock / jg skip, so the cut applies when threshold <= stock). rand calls at VA 0x96712, 0x9672E and 0x9674C (each result overwrites ecx). imul/idiv by 100 at VA 0x96765-0x96775. Clamp at VA 0x9677A-0x9679D.
- Stock tick FUN_00096538, file 0xF8BDC (VA 0x96538). Decay at VA 0x965B9-0x9660A.
- Stock-all FUN_000964d0, file 0xF8B74. Price-all FUN_00096614, file 0xF8CB8. Rand FUN_0005d1d8, file 0xBF87C, inclusive.
- Weekly FUN_000941dc, file 0xF6880: call 0x964D0 at VA 0x947C0, call 0x96614 at VA 0x947C5, funding test at 0x947CA.
- Commit FUN_0008f7f4, file 0xF1E98. Add to +0xE at VA 0x8FB0F.
- Table parse (EconomyData layout, 22 bytes: week, base, min, max, curPrice, curStock, lastStock). aequip 33 (file 0x18EABE) is 0/2780/0/8 (Toxigun). aequip 51 (0x18EC4A) is 18200/40. aequip 71 and 74 are 3480/20. aequip 84 (0x18EF20) is 4500/40 (Psiclone). vammo 9 (0x18E39A) is 2650/20. In the aequip table, 27 rows are week 0 and priced. Of these, 23 are X-COM-made (manufacturer 0 in TACP.EXE AgentEquipmentData at 3154678 + 18*i) and 4 are not: aequip 19 (700/15), 23 (900/30), 84 (4500/40) and 85 (3000/40).
- Engine: LIVE=HEAD economyinfo.cpp:29-61; HEAD:game/state/gamestate.cpp:1674 versus LIVE:1707.
- Replays (my own sim, 1500 runs, 26 weeks): the numbers quoted in the ruling, plus 100-unit dumps (Psiclone 0.755, Toxigun 0.503, against 0.900 in the engine). They reproduce both refuters' tables within Monte Carlo noise. The Toxigun sparse-selling figures (1 per 4 or 6 weeks) were independently reproduced.

**Fix (target both).** 1. In EconomyInfo::update / GameState::updateItemMarket, drop the manufacturer==player test. Select the rule by weekAvailable == 0 (EXE record word 0). Keep the existing non-zero-week random-walk branch for weekAvailable != 0.
2. For weekAvailable == 0, run the EXE order across all market rows: first a stock pass over every row, then a price pass over every row (FUN_000964d0 then FUN_00096614).
3. Stock pass (this is F-M3's decay law and must land together with this fix; the F-M2 price pass on the engine's current x0.80/x0.66 decay would cut at only about max/12 per week). Apply it only if minStock == 0 and stock != 0: s -= s/8; r = rand(0..100); if r>40 then s -= s/5; if r>70 then s -= s/3 (integer truncation).
4. Price pass, reading the post-decay currentStock: e=0; if (maxStock/2 <= stock) e=randInc(0,2)+3; if (maxStock <= stock) e=randInc(0,5)+5; if (2*maxStock <= stock) e=randInc(0,5)+10; price = (price*(100-e))/100 with integer truncation (not lround); clamp [base/2, 2*base]. Never raise the price.
5. Do not read lastStock or soldThisWeek for the decision.
6. Check that Psiclone, Elerium, the Mini and Heavy launcher AG missiles and the week-0 craft rows now go through this path.
7. Strategy guidance to carry into the plan: pace each week-0 type at or below about 0.075 x max per week (about 3/wk for max 40, about 1/wk for max 20, about 1 per 4-6 weeks for max 8). Do not trickle the same type weekly. Single lumps are neutral to mildly favourable in the EXE, but a dumped type's price stays cut for good.

## v_market / F-M11 -> diverges (low, high)

**Disputed point.** Is the EXE's raid-launch write (X-COM's relation to the raided owner forced to <= -60) persistent for the rest of the campaign, so that the engine's transient -100 is a campaign-level strategy divergence of medium impact? The validator inferred persistence only because the save routine writes the matrices. The refuter says the post-battle mirror wipes it.

**Ruling.** The refuter is right on the central point and the validator's persistence claim is wrong. I confirmed the whole chain from raw file bytes in canonical/UFO2P.EXE and from the recorded listings.

1. Launch write. The type-3 branch (listing VA 0xAC5A4-0xAC5DB; file bytes at 0x10EC48 match the listing) reads AL=[bldg+0xC8], compares byte[0x16EC28+owner] with -60 and stores 0xC4 there and at 0x16F078+owner. Matrix row 0 starts at 0x16EC28 and the row stride is 0x1C, so this is m[0][owner], X-COM's own row. The type-4 branch (which writes m[org][0]) is the only persistent launcher effect, and it is org/event-initiated.
2. Mirror. FUN_0005faa8 (file 0xC214C) forces m[0][1] = m[1][0] = -100. It then loops m[0][j] := m[j][0] for j=0..26: bytes `8a9a28ec0900 83c21c 889827ec0900 83f81b 7ceb`. That overwrites the cell the launcher wrote.
3. The mirror always runs on return. FUN_000b32ac ends with `e80cc3faff` (CALL 0x5FAA8 at VA 0xB3797) and then a single RET at 0xB37A5. Its only caller is FUN_000b1dc0 (CALL at 0xB1E79), which FUN_00010010 calls at 0x102A4. Raw call-target checks resolve each of these. [0x244020] is only ever stored as 1 by FUN_000aff9c. The other store, at 0xBC2BD, is a separate undisassembled initializer that writes 2, so it is not the building-raid path. Only a [0x244020]==2 record would divert to FUN_000b9178.
4. The sibling validator v_relations already reached the same result (§6a: transient).
5. Engine side. Both trees are identical on the relevant lines. exitBattle restores X-COM's row from every org's view (battle.cpp:3199-3208). So both EXE and engine leave X-COM->owner equal to the owner's view of X-COM at the end of the battle. Nothing persists in either, and no engine fix for persistence is warranted.

I rated the real residual divergence as intra-battle only and low impact.
- EXE: TACP has no mirror (matrix writers are the loader, saver, debug cycler, FUN_0005d25c and the kill rule). The kill rule FUN_00060720 writes the victim's view m[v][k], not X-COM's row. So X-COM->owner stays <= -60 for the whole tactical session.
- Engine: Step 08 sets -100, but commit 58b3d53a (Sep 2026) made the X-COM mirror unconditional, "even mid-battle" (organisation.cpp:1060-1076). It replaced the old `!state.current_battle` guard. The first X-COM kill of that org's unit calls ourOrg->adjustRelationTo(player, ...) (battleunit.cpp:5081/5086), and that re-mirrors X-COM's row to the org's own view.
- Consequence for a friendly org (view of X-COM above -50 after the kill penalty): player->isRelatedTo(org) stops being Hostile mid-battle. That affects kill-score credit (battleunit.cpp:5110), the mission-end test (battle.cpp:2248, reached via dropDown at battleunit.cpp:4789), and targeting.
- For the benchmark's main raid target the effect is nil. The Cult starts at -50, a kill takes it to -80, and the mirror leaves it hostile. This part is code-reading only; no run was made.

Two further corrections. The type-3 mapping is observed: the Raid handler at VA 0x71820 passes EDX=3 when owner != 0 and [0xD5060]==0, via the call at 0x7186B, so blocked_on_re is not needed for that. The write also sits before the loot gate FUN_00091db0(0,owner) || (owner,0) at 0xAC73E/0xAC74B, so its job is to force hostility for the gate and the battle, which is what Step 08 does.

**Engine.** Step 08 of battlemap.cpp sets state.getPlayer()->current_relations[target_organisation] = -100 (LIVE and HEAD game/state/rules/battle/battlemap.cpp:1542-1543, identical). Organisation::adjustRelationTo (organisation.cpp:1056-1076, identical in both trees) re-mirrors X-COM's whole row from each org's own view whenever X-COM is involved, including mid-battle. The first X-COM kill of the raided org's unit triggers it (battleunit.cpp:5081/5086/5095/5099). Battle::exitBattle (battle.cpp:3199-3208) restores X-COM's row from every org's view at battle end. So the forced hostility is transient in the engine. It lasts from battle start to the first X-COM-involved relation change, not the whole battle. Nothing persists across the battle. RaidHostileAction (buildingscreen.cpp:248 in LIVE, 309 in HEAD) is off by default.

**EXE.** Launcher type 3 (the player's Raid button in the human city) stores -60 into m[0][owner] (X-COM's own row) and into the shadow byte 0x16F078+owner, and only if the value was above -60. This happens before the loot gate, the descriptor setup and the save. TACP has no mirror, and the kill rule writes only the victim's view. So X-COM->owner stays <= -60 for the whole tactical session. On return to the city layer, FUN_00010010 -> FUN_000b1dc0 -> FUN_000b32ac always ends with CALL FUN_0005faa8, which rewrites m[0][j] := m[j][0] and wipes the -60. It is not persistent. The only persistent launcher write is type 4 (m[org][0] = -60), which is org/event-initiated and which the mirror never touches. The shadow matrix 0x16F078 is read only by relation UI screens and the init/save/load blocks. That is INFERRED: FUN_00014854's 0x7F fill and FUN_00041644 were not decoded.

**Evidence.** UFO2P.EXE ISO non-4 (file = VA + 0x626A4 for code).
- Mirror FUN_0005faa8: file 0xC214C, bytes `53 52 a1 26ec0900 ... 8a9a 28ec0900 83c21c 889827ec0900 83f81b 7ceb 5a 5b c3`, read directly from canonical/UFO2P.EXE.
- Mirror at FUN_000b32ac tail: file 0x115E34, `...e8a1bffbff e80cc3faff 83c40c 5d5f5e5a595b c3`. The CALL at VA 0xB3797 resolves to 0x5FAA8 and the RET is at 0xB37A5. The refuter's census shows only that one RET, and only JNZ 0xB3665 and 0xB3672 branch into the tail, both to 0xB3797.
- Call chain, all resolved from raw rel32: VA 0x102A4 -> 0xB1DC0, 0xB1E79 -> 0xB32AC, 0xB3797 -> 0x5FAA8, 0x5FC3A -> 0x5FAA8, 0x7186B -> 0xAC08C, 0xAC2F7 -> 0xAC348, 0xB1DCB -> 0xA4054.
- FUN_000b1db0 (file 0x114454) is `cmp [0x244020],2`. The refuter's Ghidra scan found only FUN_000aff9c (file 0x112674) storing it, always 1. A separate undisassembled function at VA 0xBC2B8 stores 2, so it is a different record kind.
- Type-3 launcher branch, file bytes: `31c0 8a83c8000000 8b9025ec0900 c1fa18 83fac4 7e16 b5c4 88a828ec0900`, so m[0][owner] = 0xC4 when > -60.
- Raw outputs: validate/r_v_market_10_0_M11a..M11d5.txt and r_v_market_10_0_M11t_tacp.txt.
- v_relations.md §6a (line 185), §5 and line 66 reach the same transient conclusion. The TACP per-kill rule FUN_00060720 writes m[victim][killer].
- Engine code: battlemap.cpp:1542, organisation.cpp:1056-1076, battle.cpp:3199-3208 and 2181-2253, battleunit.cpp:5057-5110 and 4789.
- Commit 58b3d53a replaced `if (!state.current_battle && other == getPlayer())` with an unconditional mirror.
- No Ghidra copy was created.

**Fix (target both).** Do NOT add persistence or a directed X-COM row. The EXE's -60 is itself wiped by the post-battle mirror, so that would add behaviour the original lacks. The proposed "xcomView[org]" cell is unnecessary. Persistence needs no engine change.

Optional, low priority (an intra-battle fidelity fix): keep Step 08's hostility alive for the whole battle, because the EXE's TACP has no mirror. Either skip the X-COM-row half of the mirror in adjustRelationTo while state.current_battle is set (the pre-58b3d53a behaviour; exitBattle already re-syncs the row), or store a battle-scoped target_organisation and re-apply min(row, -60) after each mirror. If the first option is taken, update tests/test_org_relations.cpp test_mirror_fires_during_battle, which asserts the mid-battle mirror. The hostility threshold (F-M15) is a separate item.

## v_market / F-M12 -> engine_missing (medium, high)

**Disputed point.** The engine-lens refuter agreed the verdict is engine_missing but disputed three claims in the validator's write-up. (1) The validator said the EXE function enforcing the lab and workshop cap was not located, so the fix is blocked on reverse engineering. (2) The validator described the cap as "4 large workshops plus 6 small ones". (3) The validator's fix sketch was incomplete.

**Ruling.** The refuter is right on all three points, and the verdict engine_missing stands. I re-derived each point from the canonical UFO2P.EXE bytes and the engine source. (1) The enforcement function is located: it is UFO2P.EXE FUN_00069270, so blocked_on_re should be false. (2) The caps are shared across lab kinds and counted per building, not per workshop. (3) The fix sketch needs the extra items listed in final_fix_sketch. Impact stays medium because the refuter did not dispute it. The cap bounds per-base research and manufacture throughput at 6 small and 4 large labs or workshops. The engine allows about 35 small or 9 large per base, so a plan can concentrate capacity in ways the original forbids. The current harness never builds a second facility of the same type, so nothing is hitting this today.

**Engine.** Base::canBuildFacility checks only bounds, tile occupancy and money, at base.cpp:268-306. No lab or workshop count exists anywhere under game/, tools/ or tests/. The text "Regulation 44" appears nowhere in the code. The three files that matter (base.cpp, base.h, basescreen.cpp) are byte-identical between LIVE and HEAD. The BuildError enum at base.h:43-53 has no member for this rule. The BaseScreen drop handler (basescreen.cpp:371-416) switches on that enum, so a new member would need its own MessageBox case. Buildable area is 20-36 tiles per layout, so one base can hold roughly 35 small or 9 large labs or workshops.

**EXE.** UFO2P.EXE FUN_00069270 is the build-palette pick handler, called only from the facility-menu loop FUN_00069084. It runs a funds test, then a count test, and refuses with the Regulation 44 message when the count test fails.

- **Funds test:** it refuses unless balance is strictly greater than cost. The engine allows balance equal to cost, a minor side divergence outside F-M12.
- **Counting:** for a small pick (type index 0x1A, 0x1C or 0x20) it counts cells in the 8x8 base grid, at VA 0xD9444 + base*0x2BE, whose byte is any of those three. For a large pick (0x1B, 0x1D or 0x21) it counts cells holding any of those three. Other types leave the count at 0.
- **Refusal:** if the picked record's size byte at +5 equals 1, it refuses when the count is 6 or more. Otherwise it refuses when the count is 4 or more.
- **Types covered:**

| Type index | Facility | Cost |
|---|---|---|
| 0x1A | Biochemistry Lab | 10000 |
| 0x1C | Quantum Physics Lab | 10000 |
| 0x20 | Workshop | 6000 |
| 0x1B | Advanced Biochemistry Lab | 20000 |
| 0x1D | Advanced Quantum Physics Lab | 25000 |
| 0x21 | Advanced Workshop | 12000 |

- **Shared caps:** the caps are shared across lab kinds: 6 small in total across Biochemistry, Quantum and Workshop, and 4 large in total across the three Advanced types. They are not 6 plus 4 workshops of their own.
- **Count unit:** the count is per building. Placement writes the type byte only in the origin cell and markers 0xC9, 0xCA and 0xCB in the other three cells of a 2x2. Markers never equal a type index.
- **Under construction:** facilities under construction count, because the type byte is written at placement.
- **Not covered:** Alien Containment (0x1E, 0x1F) is not capped.

**Evidence.** I verified everything below directly from og-research:canonical/UFO2P.EXE (1,702,206 bytes, ISO non-4), with capstone disassembly of the physical file. No Ghidra copy was needed.

**Function and bytes**
- Code rule: file = VA + 0x626A4.
- FUN_00069270 is at VA 0x69270 = file 0xCB914. The prologue bytes `51 56 57 55 83 ec 0c 66 8b 35` are at file 0xCB914.
- Type index is (record - 0xC010) >> 4, where 0xC010 is the raw operand for the record table at VA 0xDC010.
- `cmp eax,ebx; jle 0x693ee` is the funds test.
- Counting loops: file 0xCB95F-0xCB993 for the small types and file 0xCB9A6-0xCB9DA for the large types. Both read `[base*0x2BE + row*8 + col + 0x9444]`.
- Size test: `80 7D 05 01 75` (cmp byte ptr [ebp+5],1) at file 0xCB9DC.
- Refusal tests: `83 FA 06 7D 31` (cmp edx,6; jge) at file 0xCB9E9, and `83 FA 04 7D C8` (cmp edx,4; jge) at file 0xCBA52.
- Shared refusal path at VA 0x6937b pushes string-pool group 0x25, item 0x167 through resolver FUN_00063A00.
- Caller: `call 0x69270` at VA 0x69204 = file 0xCB8A8, inside the FUN_00069084 loop over records VA 0xDC120-0xDC230.

**Strings**
- File 0x152252 reads: "Regulation 44(1)(e) of the health and safety at work (labs and workshops) act (2074) prohibits the construction of more than 6 small or more than 4 large labs or workshops in any one basement. Contractors therefore refuse to carry out the proposed illegal construction work."
- "Funds exceeded" and "The proposed construction work is not possible with your available funds." follow at items 0x168 and 0x169.

**Type table**
- Facility records at file 0x13D6B4, stride 16, cost at +0 and size at +5.
- The extractor table starts at file 0x13D794, which is record 14. Names from the string table at file 0x14A66C map as: 0x1A Biochemistry Lab (10000, size 1), 0x1B Advanced Biochemistry Lab (20000, size 2), 0x1C Quantum Physics Lab (10000, 1), 0x1D Advanced Quantum Physics Lab (25000, 2), 0x20 Workshop (6000, 1), 0x21 Advanced Workshop (12000, 2), and 0x1E and 0x1F Alien Containment (not capped).

**Placement code (per-building counting)**
- The placement code at VA 0x68990-0x68B1E uses grid pointer `0x942E + base*0x2BE` with `+0x16`, the same grid as the counter.
- `mov [ebx+0x16],al` writes the type in the origin cell. `[ebx+0x17]=0xC9`, `[ebx+0x1F]=0xCA` and `[ebx+0x1E]=0xCB` mark the other three cells.
- `sub [edx+8],ebx` deducts the cost only after the checks pass.

**Engine**
- Engine: base.cpp:268-306 (identical in LIVE and HEAD), basescreen.cpp:371-416, base.h:43-53. A grep of game/, tools/ and tests/ for "Regulation", "illegal construction" and any lab-count rule found nothing.
- Class mapping: facility_types.xml gives capacityType Chemistry, Physics or Workshop to exactly these six types. size 1 vs 2 separates the small and large classes, which is the same split as the EXE's index sets.

**Fix (target both).** 1. Add a BuildError member, for example TooManyLabs, in base.h:43-53, and a matching MessageBox case in basescreen.cpp near line 401. Without that case the drop fails silently.
2. In Base::canBuildFacility, apply the rule only when !free. The EXE enforces it in the UI pick, so scripted base creation (base.cpp:77/191/203) must stay exempt.
3. Rule: if type->capacityType is Chemistry, Physics or Workshop, count this base's facilities with those capacity types and the same size class (size == 1 vs size > 1). Refuse when the count is 6 or more for small, or 4 or more for large. The count must include facilities still under construction, which are already in `facilities` from buildFacility.
4. Use the EXE text as the message, with title "Funds exceeded"-style wording of your choice. The body is the Regulation 44 string at file 0x152252.
5. Optional, outside F-M12: the EXE funds test is strict (balance greater than cost), while base.cpp:301 allows balance equal to cost.
6. Tests: add a unit test that builds 6 small labs or workshops in a mix and rejects the 7th. Add one that builds 4 large and rejects the 5th. Add one showing small and large caps are independent.
7. Update the strategy note in v_market.md: the cap is 6 small and 4 large across all labs and workshops combined, and blocked_on_re is false.

## v_tactical_difficulty / TD6 -> diverges (low, high)

**Disputed point.** Whether UFO2P/TACP pick human org-guard gear through a difficulty-biased 5-slot equipment-set bucket (rec[+0x53+bucket], thresholds <5,<9,<12,<14), so that the engine's difficulty-free weekly tech_level model diverges. It does not: the bucket never reaches human guards.

**Ruling.** The engine-lens refuter is right about the mechanism, and the validator's central claim is wrong. The original game has no difficulty-biased gear bucket for human org guards. The weekly tech-level model the validator called a divergence is the EXE's own model, and the EXE does have an org-tech gear consumer, FUN_000b46c4. The finding keeps the label "diverges" only on smaller grounds. The fork rolls guard gear over the survivors of its extractor's empty-row compaction, which overstates grenades and tier 9-12 equipment, so impact drops from medium to low. blocked_on_re is false. The refuter's other residual, the missing +3 taken-over tier, is already v_takeover T10 and should not be counted again.

**Engine.** LIVE and HEAD are identical for the relevant code. Agent::createAgent gives human org guards EquipmentSet::getForType(Human, org->tech_level)->generateEquipmentList (agent.cpp:173-174). tech_level is starting_tech_level+1 (extract_organisations.cpp:50), +1 per week, capped at the highest human min_score (12) (LIVE gamestate.cpp:1742-1754, HEAD 1709-1721). HUMAN_k selects at tech_level k through min_score/max_score windows (extract_agent_equipment.cpp:1130-1131). There is no difficulty term, which matches the EXE. The extractor builds each set from only the non-empty rows: weapons (:1083, clip_amount raised from 0 to 1), grenades (:1093), equipment (:1101). generateEquipmentList (aequipmenttype.cpp:62-92) picks uniformly among the survivors and always gives a grenade or equipment item when any row survived.

**EXE.** OBSERVED, UFO2P non-4 (CRC 0x4749ffc1). FUN_000aec70 and FUN_000aedb0 create the guards: unit type 13, or 12/10/11 by org, with the count from average_guards. Each guard goes through FUN_000ad93c, which sets stats and calls no gear function, then FUN_000b46c4. b46c4 computes tier = clamp(org[+6] + week[0xE0E7C] - 1, 0, 11), plus 3 if org[+0x11] (taken over). It then makes three independent rand(9) draws, with rand inclusive [0,n]. Each draw picks a row from the 12-tier human score table at UFO2P file 0x19C3A0, which is byte-identical to TACP file 0x302FEC over 1680 bytes. An empty row gives nothing and is not re-rolled. There is no difficulty read in b46c4, aec70 or aedb0.

The bucket FUN_000ae1d4 is called only from FUN_000adffc and FUN_000ae218. Both build alien types 34..46 and gate on agent_type_data rec+0x44 at file 0x155574 + type*0x6a. If it is 0, the unit goes to ae1d4 plus FUN_000ad148. If it is non-zero, it goes to FUN_000ad38c (alien score tier). Human org types 4 and 6-13 have rec+0x44 = 1 (read from the file), as do Anthropod and Skeletoid. Every inventory-0 type has five identical set ids, so the bucket never changes an outcome.

Row occupancy per tier 0..11: weapons 10/10 in every tier. Grenades non-empty 3,4,5,5,6,7,7,7,8,8,9,9 of 10, so P(grenade) = 30,40,50,50,60,70,70,70,80,80,90,90 %. Equipment non-empty 0 in tiers 0-7 and 2,3,5,8 in tiers 8-11, so P = 20,30,50,80 % there. The fork gives 100% in both cases. One weapon row (tier 4, row 9) has weapon 16, clip 17, count 0, and the fork raises that to 1 clip.

TACP side: FUN_00024e5c is called only by FUN_00024890 (reinforcement spawn from FUN_000243c0) and FUN_0006e180 (constant type 4, the Hyperworm). Both are inventory-0 units with identical set ids. FUN_000250f8, the alien score tier, is called only by FUN_00024890.

The 0xAE9EC path is an org-tech-biased 5-slot bucket, not a difficulty one. It is undefined in Ghidra, its caller 0xAE878 has no direct CALL, JMP or pointer reference, and reachability is unresolved (possibly dead code).

**Evidence.** Primary re-derivation.
- Ghidra decompile and listing of UFO2P FUN_000adffc, FUN_000ae218, FUN_000ae1d4, FUN_000ad148, FUN_000ad38c, FUN_000b46c4, FUN_000aec70, FUN_000aedb0, FUN_000aebd8, FUN_000ae628, FUN_000af444 and FUN_000af40c. Also TACP FUN_00024890, FUN_000250f8, FUN_000243c0, FUN_0006bfe0 and FUN_0006e180.
- xrefs: ae1d4 callers are adffc (1) and ae218 (3). b46c4 callers are aec70 and aedb0. TACP 24e5c callers are 24890 and 6e180. 250f8 caller is 24890. FUN_000638dc callers are 92060, ae1d4 and 63951 (no callers, a 2000-sample test histogram).
- Difficulty readers of 0xE0C9A/0xE0C9C are 97b90, 6f964, 6fcc0, 92060, 638dc, 96bc8, ad38c, ad93c, aff9c, b426c, b94c0, 15400, 99130, 14854, 7ee84, 3a910, 97c50 and a3234/a4054. b46c4, aec70, aedb0, af444, af40c and aefd0 are not on it.
- All four callers of the item-adder FUN_000ad5ac were checked. ad148 and ad38c reach only alien types. b46c4 has no difficulty term. af444 is a post-roster pass from FUN_000aefd0 and adds an item when FUN_000af40c finds no type-0x33 map object for the owner, with no difficulty read.
- FUN_0005d1d8 decompiles to `... % (param_1 + 1)`, so rand(9) gives 0..9.
- Python byte reads of UFO2P.EXE confirm the agent_type_data table, the occupancy counts and the table equality with TACP.
- Fork tier mapping verified: set i has min_score i+1 (i=0 is INT_MIN) and max_score i+2, so tech_level k picks HUMAN_k. tech_level = start+1+completed weeks, and the cap of 12 equals the EXE's clamp at index 11.
- LIVE and HEAD diff clean on agent.cpp, aequipmenttype.cpp, extract_agent_equipment.cpp and extract_organisations.cpp.
- Notes: scratchpad/validate/adj_v_tactical_difficulty_TD6.md.

**Fix (target both).** blocked_on_re is false.
1. Do NOT add a difficulty term or a per-type 5-slot table to human guard gear. The EXE has none, and the TD6 fix_sketch should be dropped.
2. Optional, low priority. Make the rolls match the EXE's three independent 10-row draws. Either keep the empty rows in the extracted sets, or store nonEmpty/10 per category and skip the category when the draw lands on an empty row. That means changing the grenade and equipment pushes at extract_agent_equipment.cpp:1093 and 1101, and generateEquipmentList (aequipmenttype.cpp:74-92) to draw over 10 slots. Also remove the clip_amount 0 to 1 raise at :1083 for the one count-0 row. Regenerate equipment_sets.xml.
3. The +3 taken-over tier is already tracked as v_takeover T10. Do not list it again here.
4. Optionally model the unresolved 0xAE9EC leader path only if it is shown reachable.

## v_tactical_difficulty / TD17 -> diverges (medium, high)

**Disputed point.** Not the verdict, which both sides call "diverges". The dispute is over three supporting claims in the validator's exe_behaviour. (1) Whether the debrief hides the "Live Aliens captured" row. (2) Whether the header flags [0x1ED2C8] and [0x1ED350] are undecoded. (3) Whether the fix is blocked on RE, which depends on (2). The refuter also corrects the third loop's side test.

**Ruling.** Uphold the refuter on every disputed point and keep "diverges". Medium impact is unchanged.

I checked each point against the canonical EXE bytes (physical offsets) plus the Ghidra listing and xref dumps already in the record.

(a) Total credit. TACP FUN_00022f90 loop 3 (VA 0x2333A-0x233C7) adds the alien's score byte (unit+0x44) to the total register EBX before it tests [0x1ED350]. File 0x7DE4D is `01d3` (ADD EBX,EDX), then `85ed 7408` (TEST EBP,EBP; JZ). The flag only picks which display cell gets the score: [0x27A098] "Live Aliens captured" if set, else [0x27A088] combat rating. No test of craft, module or containment appears anywhere in the function.

(b) Leadership base. Loop 2 does `ADD ESI,EDX` (file 0x7DC94, `01d6`) before its org test. Every stunned, egg or chrysalis hostile therefore feeds the leadership base, also regardless of storage.

(c) Side test. Loop 3 only processes units whose org word is 1, the Alien org: `663bb8f09e0200 7551` (CMP DI,[unit+0x20]; JNZ skip) with EDI=1. The validator wrote "side != 1" in v_tactical_difficulty.md section 6.3 item 5, and that is wrong. Loop 2 is the complement: `6683ff01 7410` skips aliens for the combat-rating and total add. Stunned aliens therefore feed the leadership base in loop 2 and the direct total term in loop 3.

(d) Flags are decoded.
- [0x1ED2C8] is the launch mode. UFO2P writes the constant 1: `c705 20401700 01000000` at file 0x112672, in FUN_000aff9c. The xref dump shows that is the only writer of 0x244020.
- [0x1ED2C8] also gates the campaign export FUN_00015494, which hands the total back to UFO2P. Whenever a tactical total reaches category 0 at all, the capture loop is therefore active.
- [0x1ED350] maps to UFO2P [0x2440A8], the storage flag from the craft loop at file 0x1129C0-0x112A1B. For mission types other than 2 it is set when FUN_00072e40 (alien_space) is above 0, which means a Bio-Transport. For type 2 it is set without any capacity test if the craft slot from [0x1304CC] is not -1.
- The base-defence stale-slot reading is not fully proven, but it can only change which display line the points land on, never the total.

(e) Debrief. Row 3 is always skipped (`83fe03 0f84`). Row 4 is skipped only if [0x1ED2C8] != 1: `83fe04 750d 833dc8d21000 01 0f85` at file 0x10E89D. In campaign play the line is therefore drawn: it shows the score with storage and 0 without. The validator's "never shown, hidden cell" is wrong.

(f) The total reaches score category 0 unmodified. FUN_00015494 stores the total to [0x1ED334], and UFO2P FUN_000b32ac at file 0x115950 loads [0x24408C] and calls the category updater FUN_000983dc with EAX=0 (Tactical Missions).

(g) Engine. LIVE and HEAD battle.cpp:2886-2905 add liveSpeciesItem->score to liveAlienCaptured only under playerHasAlienStorage. That variable feeds both getTotal() and getLeadershipBonus(). combatRating changes only in BattleUnit::die() (battleunit.cpp:5114), so a stunned alien earns nothing without storage. The score values equal the EXE's (extract_agent_types.cpp:1218 sets liveItem->score = a->score).

(h) Net gap per stunned, egg or chrysalis alien on a win without storage: the EXE credits score + score*(100-3c)/100, where c is the casualty count, and the fork credits 0. With storage the two agree.

(i) blocked_on_re must be false. The fix needs no unrecovered constant, and the flag meanings only affect labelling.

Cross-references. TD18 owns the leadership cutoff (30*c>1000 gives 0), so do not duplicate it here. SCORE-C0 overlaps because it covers the category 0 formula. The refuter's side lead is that loop 2 also credits stunned non-alien hostiles (for example raid guards) to combat rating, total and leadership base. The fork's liveAliens covers only owner==aliens. That is separate from TD17 and needs its own check.

**Engine.** LIVE:game/state/battle/battle.cpp:2886-2905 (HEAD:2886-2905, identical): inside endBattle, in the playerWon && !winnerHasRetreated block, `playerHasAlienStorage = isBioCarrierPresent(playerVehicles) || isBaseDefenseWithStorage(Aliens)`. For each live alien with a liveSpeciesItem, `score.liveAlienCaptured += liveSpeciesItem->score` only if playerHasAlienStorage. The same loop still files the live item into bioLoot or cargoLoot.

liveAlienCaptured feeds both getLeadershipBonus() = (100+3*casualtyPenalty)*(combatRating+friendlyFire+liveAlienCaptured)/100 and the direct term in getTotal() (LIVE:4073-4079, HEAD:4066-4072).

getTotal() is added to tacticalMissions at endBattle (LIVE and HEAD :3211-3212).

combatRating is incremented only in BattleUnit::die() (battleunit.cpp:5114), so without storage a stunned alien contributes 0 to the total and to the leadership base.

HEAD and LIVE are identical here. LIVE's only battle.cpp difference is an unrelated alienBuildingsDestroyed insertion at :3581-3587.

**EXE.** TACP FUN_00022f90 (file 0x7DA34), on a win ([0x1ED32C]==1):

- Loop 2 (VA 0x2316C-0x23212): for each hostile unit that is stunned (health <= stun) or type 1 or 5, the unit's score byte is added to the leadership base ESI, with no condition on storage or on side. If the unit's org is not 1, it also goes to combat rating and the total.
- Loop 3 (VA 0x2333A-0x233C7, file 0x7DDDE-0x7DE6B): for units with org word == 1 (Alien), health != 0, [0x1ED2C8]==1, and stunned or type 5 or 1, `ADD EBX,EDX` adds the score to the total unconditionally. Then, if header [0x1ED350] != 0, it also adds to [0x27A098] "Live Aliens captured"; otherwise it adds to [0x27A088] combat rating. That second add is display only.
- The leadership bonus is base*(100-3c)/100, forced to 0 if 30*c > 1000. The total is EBX, stored at [0x27A0A4], exported at [0x1ED334], and added to score category 0 by UFO2P FUN_000b32ac.
- [0x1ED2C8] is the launch mode, constant 1 from UFO2P FUN_000aff9c. [0x1ED350] is the storage flag (Bio-Transport at the site, or the base-defence stale-slot rule) and only selects the display line.
- Debrief FUN_000b3d54 skips row 3 and draws row 4 whenever [0x1ED2C8]==1.
- Net per capture without storage: score + score*(100-3c)/100 reaches the total. With storage the amount is identical.

**Evidence.** Binary: TACP.EXE (non-4 generation). Physical offsets, verified with xxd on OpenApoc-og-research/canonical (TACP code = VA+0x5AAA4, UFO2P code = VA+0x626A4).

TACP:
- 0x7DE4D: `01d3 85ed 7408 011598a01900 eb06 011588a01900`. The total add precedes the flag test.
- 0x7DDDE-0x7DE15: `bf01000000 ... 6683f901 0f84 ... 3b3d2cd31000 755a 663bb8f09e0200 7551 668b90349f0200 6685d2 7445 3b3dc8d21000 753d ...`. These are the win, org==1 and [0x1ED2C8]==1 gates.
- 0x7DC94: `66 83 ff 01 74 10` (file 0x7DC96) follows `01d6` (ADD ESI,EDX). The leadership-base add precedes the org test.
- 0x7DD95-0x7DDAB: leadership cutoff, `3d18fcffff 7d04 31f6`.
- Debrief FUN_000b3d54, file 0x10E895 and 0x10E89D: `83fe03 0f84 62010000` (row 3 always skipped) and `83fe04 750d 833dc8d21000 01 0f85` (row 4 skipped only if [0x1ED2C8] != 1).
- Listing: validate/vtd_t17.txt lines 287-515.
- Export: FUN_00015494 stores DAT_0027a0a4 to _DAT_001ed334 (r_v_td17_work/tacp_full_15494_b3d54.txt:171, 452-454).

UFO2P:
- 0x112672: `c705 20401700 01000000` writes [0x244020]=1. The xref dump has FUN_000aff9c as its only WRITE.
- 0x1129C0-0x112A1B: storage-flag loop. It exits if the flag is set or the craft index is -1. For type 2 it jumps to `89e9` (MOV ECX,EBP) and sets the flag with no capacity test. Otherwise it calls FUN_00072e40 and sets the flag if the result is > 0.
- 0x112CE8: `bbd4000000 baec3f1700`, the 212-byte block copy from 0x243FEC.
- 0x115950: `... 8b158c401700 e814 51feff`. It loads [0x24408C] and calls the updater with EAX=0.
- UFO2P xref dumps: ufo_refs.txt and ufo_refs2.txt.

Engine: LIVE battle.cpp:2886-2905 and :4073-4079; battleunit.cpp:5114; extract_agent_types.cpp:1218.

**Fix (target both).** In Battle::finishBattle, in the live-alien loop under `playerWon && !winnerHasRetreated` (LIVE and HEAD battle.cpp:~2899-2903), credit each live alien's `agent->type->liveSpeciesItem->score` to the total regardless of storage:
- If playerHasAlienStorage, add it to score.liveAlienCaptured, as now.
- Else add it to score.combatRating, which mirrors the EXE's fallback display cell.

Because getLeadershipBonus() already sums combatRating+liveAlienCaptured, this yields the EXE's double role: one count in the direct total term and one in the leadership base. Keep the storage gate only for the bioLoot and cargoLoot transfer.

The EXE credits only stunned, egg or chrysalis aliens, which is what the engine's liveAliens list approximates. If an unstunned alien can survive a win in the engine, restrict the credit to stunned or egg or chrysalis units to match. Note that the EXE credits retreated or out-of-area units through a separate loop that is not part of TD17.

Do not mix in the leadership cutoff here, because TD18 owns it (30*c>1000 gives bonus 0). Optionally label the debrief to match the EXE: show "Live Aliens captured" as 0 without storage and the score with storage. Set blocked_on_re=false.

## v_tactical_difficulty / TD19 -> diverges (low, high)

**Disputed point.** Whether TD19 ("live-capture transfer needs a craft with Bio-Transport capacity") is a clean match with fix none, as the validator ruled, or diverges. The refuter agreed the capacity gate itself matches. The dispute is the candidate-craft set. The finding says the EXE tries "the next craft at the site" when the first has no bio capacity. The refuter says that holds only for building missions, and that for UFO recovery (EXE mission type 3) the EXE considers only the single dispatched craft. The engine pools nearby player vehicles there.

**Ruling.** The refuter is right on the narrow point, so the verdict is diverges at low impact. The gate itself is a match, and I re-verified it. `FUN_00072e40` sums alien_space over a craft's 45 slots. In the 13-record general-equipment table at file 0x18B576, only record 6 (Bio-Transport Module) has alien_space non-zero, and it is 20. `FUN_000915a0` stores into a keyed counter table with no per-craft capacity check, which matches the engine's unlimited load when EnforceCargoLimits is off. The "match / fix none" claim fails only for UFO-recovery eligibility.

In `FUN_000b21a8`, both the dead-alien loop (kind 5) and the live-alien loop (kind 4) read the candidate craft as follows:
- For type 3 it is `[0x244088]` only.
- On a failed capacity test, type 3 sets `ESI=-1` (0xb24c1-0xb24cf and 0xb25f3-0xb2601) and never calls `FUN_000b2724`.
- Other types call `FUN_000b2724` to try the next craft in the building.
- The loop then ends with the transfer failed.

The cargo and vehicle-loot loops in `FUN_000b27b4` follow the same pattern for type 3 (0xb2903-0xb2911, 0xb2a35-0xb2a3d, 0xb2bbf-0xb2bc8). I traced EDI to confirm this. It is the constant 3 at 0xb29b3 and -1 at 0xb2acd, so ESI ends as -1.

Engine side, `Battle::getPlayerVehicles` adds `player_craft`, then every player vehicle flying in the city within VEHICLE_NEARBY_THRESHOLD of the UFO. That block is gated on `OpenApoc.NewFeature.AllowNearbyVehicleLootPickup`, which defaults to true. The benchmark launcher `tools/oa_play.py` passes `--Config.Read=0` and overrides only SeedRng, RngSeed and EnableAgentTemplates, so the default applies. I confirmed this with a plain grep across both trees. `~/Library/Application Support/OpenApoc/settings.conf` exists but has no NewFeature lines, and it is not read under `--Config.Read=0` anyway.

A separate Bio-Transport craft parked near a crashed UFO therefore saves the captures in the engine but not in the original. The engine is more lenient, so every EXE-valid plan still works there. That is why the impact is low.

Type 3 is UFO recovery. I rate this strongly supported, not directly observed, and it is a step up from the refuter's untraced inference. `FUN_000aff9c` has exactly one caller, `FUN_000ac348`, reached through `FUN_000ac08c`. The relevant site is `FUN_0003a910`, 0x3b266, which passes EDX=1. It is reached when a craft with `+0x12c==0` and order `[+2]==1` reaches its target vehicle `[+0xce]`, with EBX=-1. For EDX=1, `FUN_000aff9c` skips the building lookup, leaves `[0x244044]=-1`, and sets `[0x244024]=3`.

Two corrections to the validator's write-up:
- Base defence (type 2) is exempt from the craft test: EBX is forced to 1 and the candidate is the virtual carrier `[0x1304cc]` from `FUN_000b44a4`. After the battle, `FUN_000b1dc0` calls `FUN_00091678` (delivery) on that slot. So "stores nothing / exe_unbound" is wrong, and "bring a Bio-Transport craft for every capture mission" is wrong for base defence in both the EXE and the engine. The engine's `isBaseDefenseWithStorage` matches in spirit. That `FUN_00091678` tests Alien Containment is the refuter's reading and is not re-verified here.
- The "next craft is tried" clause is true only for building missions, types 0/1/4 and anything that goes through `FUN_000b26c0`.

**Engine.** The bio-carrier gate exists. `isBioCarrierPresent` (LIVE battle.cpp:4058) tests `Vehicle::getMaxBio() > 0` (vehicle.cpp:3728). Bio loot is cleared at battle.cpp:3260-3284 unless a carrier is present or the mission is a base defence with Alien Containment. The load loop at ~3493-3530 uses the first vehicle with bio capacity and has no limit while EnforceCargoLimits is false.

The pool is built by `getPlayerVehicles` (LIVE battle.cpp:3989-4018, HEAD :3982-4011, identical in both trees). It starts with `player_craft`. With `AllowNearbyVehicleLootPickup` true (the default, options.cpp:455-458 in LIVE and :447 in HEAD), UfoRecovery also adds any player vehicle in the city, not docked, within VEHICLE_NEARBY_THRESHOLD of the UFO. Other missions add the player vehicles in the mission building. The same pool feeds `isCargoCarrierPresent`, so cargo and vehicle loot are affected too.

**EXE.** Capacity is `FUN_00072e40` > 0 (the sum of alien_space across general modules, 20 for the Bio-Transport Module only). Candidate craft by mission type:
- Building types (0/1/4 via `FUN_000b26c0` then `FUN_000b2724`): every X-COM craft (`+0x12c==0`) in the mission building is tried in turn. This matches the engine's building pool.
- Type 3 (UFO recovery): only `[0x244088]`, which is inferred to be the craft that reached the UFO. After one failed test the candidate is -1, the transfer fails and the aliens are lost. This applies to alien bodies, live aliens and cargo/vehicle loot alike.
- Type 2 (base defence): the craft test is skipped. A virtual carrier slot `[0x1304cc]` is used, and delivery goes through `FUN_00091678`.

**Evidence.** I read the Ghidra listings the refuter saved in `scratchpad/validate/r18_0_q*.txt` and added two new headless queries of my own. All offsets are UFO2P non-4; code file offset = VA + 0x626A4 and data file offset = VA + 0x616A4. The Ghidra copy is deleted.

**Capacity function and table**
- `FUN_00072e40`: file 0xD54E4, prologue `53515689c685c075` re-read from the raw bytes. It loops `CMP AX,0x2d` over `veh+0x18d+slot`, requires class byte `[idx*0x18+0x1296a4]==2`, and adds the word at `[0x129ed8+idx*0x12]`. 0x129ed8 + 0x616A4 = file 0x18B57C.
- I dumped the general-equipment table at file 0x18B576 (18-byte records) directly. Column word[3] is non-zero only for record 6, where it is 20.
- `FUN_000915a0` (VA 0x915a0): scan of a 256 x 8-byte keyed table at VA 0x144118 (listing in r18_0_q1.txt). It has no capacity check.

**Candidate selection in `FUN_000b21a8`** (file 0x11484C, listing in r18_0_q1.txt)
- Type 2 gives ESI=`[0x1304cc]` and EBX is forced to 1 at 0xb2442 and 0xb2574.
- Type 3 gives ESI=`[0x244088]` (0xb241c, 0xb254e).
- Anything else calls `FUN_000b26c0`.
- On a failed test, type 3 sets ESI=-1 (0xb24c1-0xb24cf, 0xb25f3-0xb2601). Other types call `FUN_000b2724`.

**`FUN_000b27b4`** (file 0x114E58): the same structure at 0xb2903-0xb2911, 0xb2a35-0xb2a3d and 0xb2bbf-0xb2bc8.

**Where type 3 and `[0x244088]` come from** (`FUN_000aff9c`, file 0x112640)
- 0xb0112-0xb0141: EDX==1 sets `[0x244024]=3`. EDX==5 sets 2, EDX==3 sets 4, anything else sets 1.
- 0xaffa9-0xaffce: for EDX==1 or 6, EAX is zeroed, so the building lookup is skipped and `[0x244044]` stays -1.
- 0xb0205-0xb021e: for EDX==1, `[0x244064]=param_1` and `[0x244088]=ECX` (the fourth argument).

**Call chain**
- New this session: `FUN_000aff9c` has one caller, `FUN_000ac348` (0xac6e1). Its only caller is `FUN_000ac08c` (0xac2f7). The EDX=1 site is `FUN_0003a910` at 0x3b266. There EBX=-1 and ECX=`[ESP+0xa]>>16`, which is preserved across `FUN_000b2e18`. The preceding checks are `[EDI+0x12c]==0` (X-COM craft), `[EDI+2]==1`, and EAX=`[EDI+0xce]>>16` (the target vehicle). The EDX=5 sites at 0x3ae76 and 0x3b079 go to base defence.
- INFERRED: that ECX is the arriving craft's own index. I did not trace it through `FUN_000ac348`, whose ECX is saved at `[ESP]` on entry.

**Base defence**
- `FUN_000b1dc0` (file 0x114464) calls `FUN_000b44a4` for type 2.
- At 0xb1e94-0xb1eae it calls `FUN_00091678` on the virtual carrier slot.

**Engine settings**
- `tools/oa_play.py` lines 668-700 pass `--Config.Read=0` and set only SeedRng, RngSeed and EnableAgentTemplates.
- The AllowNearbyVehicleLootPickup option appears only in options.cpp/h, `battle.cpp` and `moreoptions.cpp` in both trees.

**Fix (target both).** In `Battle::getPlayerVehicles` (HEAD game/state/battle/battle.cpp:3982, LIVE :3989), skip the nearby-vehicle block when `mission_type == UfoRecovery`, so only `player_craft` is returned. Leave the building-mission branch (`mission_location_building->currentVehicles`) alone, because it matches the EXE's `FUN_000b26c0`/`FUN_000b2724` walk.

The alternative is to have `tools/oa_play.py` pass `--OpenApoc.NewFeature.AllowNearbyVehicleLootPickup=0` for benchmark runs. That also stops the nearby-pickup feature in building missions, and the option's own cargo/vehicle-loot pooling goes with it.

Doc corrections for the engine-fix plan:
- Rewrite the strategy rule as "in UFO recovery, the craft that carries the squad must itself carry the Bio-Transport Module. In building missions, any X-COM craft in the building will do. Base defence needs no craft, only Alien Containment."
- Drop the §6.4 claims "stores nothing for base defence" and "exe_unbound".

## v_score / SCORE-C1 -> engine_bug (low, high)

**Disputed point.** Whether SCORE-C1 (Category 1 Research Completed score) is a clean "match / fix none". The validator said yes (value, cadence and once-per-topic all match; moving the add out of the UI handler is optional). Both refuters agreed the EXE half is right but said the engine's delivery path loses credits, so the verdict should be engine_bug and the fix required.

**Ruling.** Refuters upheld, with the impact set to low. The value half is a match. The delivery half is an engine bug: the EXE credits unconditionally inside the simulation, and the engine credits only if CityView happens to be the top stage when the event is delivered. The score value, the once-per-topic rule, the hours gate and the 99-topic table (sum 21,010) all match the EXE. The engine is wrong about when the credit happens, not how much it is. The fix is a small required change, not an optional one. Impact is low: the observed loss is about 0.6% of completions, roughly 1% of research points. The validator's "high" rating describes how much research score matters to strategy, not how much the divergence matters.

**Engine.** Lab::update (research.cpp:485-508 in both trees) marks the topic complete and calls fw().pushEvent(GameResearchEvent). The only writer of totalScore.researchCompleted and weekScore.researchCompleted is the UI handler CityView::handleGameStateEvent (LIVE cityview.cpp:4924-4925, HEAD cityview.cpp:4849-4850). Framework::processEvents sends each queued event only to ProgramStages.current() (LIVE framework.cpp:784, HEAD framework.cpp:706). Stage commands are applied after update() within the same frame, and the event is dispatched in the next frame. If any stage is pushed in the frame in which the lab completes, the event goes to that stage and the score is never added. CityView::eventOccurred does not swallow game events (Control::eventIsWithin returns false for them). BattleView::handleGameStateEvent has no ResearchCompleted case. The separate harness workaround at tools/oa_play.py:2617-2622 shows the project already knows about this failure mode.

**EXE.** UFO2P.EXE (non-4) FUN_000aac88, entry file 0x10D32C, adds the score inside the simulation once per topic. The completed flag byte [idx*10+0xE2B8] is tested at 0x10D360 and set at 0x10D58C (group 0) or 0x10D936 (group 1). In the same basic block it runs "mov dx,[ebp+0xd7ee]; mov eax,1; call 0xFAA80" (calls at file 0x10D5B0 and 0x10D950). The score is the u16 at record +0x12. 0xFAA80 is "and eax,0xffff; add [eax*8+0x10c5c],edx; ret", which credits category 1 for the week. Nothing depends on the UI or on which screen is open.

**Evidence.** EXE, checked myself by direct capstone disassembly of OGR/canonical/UFO2P.EXE (no Ghidra copy was created):
- The Watcom push prologue at file 0x10D32C is confirmed.
- The "cmp byte [eax+0xe2b8],0 / jne" entry guard at 0x10D360 is confirmed.
- Group 0 runs "mov byte [ebx+0xe2b8],dh" at 0x10D58C, "mov dx,[ebp+0xd7ee]" at 0x10D59E, "mov eax,1" at 0x10D5A5, and "call 0xfaa80" at 0x10D5B0.
- Group 1 runs "mov dx,[ebp+0xd7ee]" and "mov byte [ebx+0xe2b8],cl" at 0x10D936 (the mov dx sits just before it), then "mov eax,1" at 0x10D945 and "call 0xfaa80" at 0x10D950.
- The updater at 0xFAA80 is "and eax,0xffff; add [eax*8+0x10c5c],edx; ret".
- The 99 records of 28 bytes at file 0x13EE80 sum to 21,010 (record 0 is Dimension Gates: 50 score, 5000 hours; records 88 and 97 are 380 and 480 score).

Engine, checked directly:
- cityview.cpp:4917-4925 (LIVE) is the only writer; there is no score credit in research.cpp.
- research.cpp:440-515 is byte-identical in HEAD and LIVE. HEAD's cityview.cpp:4849-4850 holds the same two score lines.
- framework.cpp:784 (LIVE) and 706 (HEAD) dispatch to current() only.
- Control::eventIsWithin (forms/control.cpp:1142-1156) returns false for game events.
- The only other UI-handler-only score writer is alienIncidents at cityview.cpp:4847 (LIVE).

Saves: I unzipped build-funding/learning-control-seed11/runs/g000-r0002/victory.save.
- research.xml has RESEARCH_ALIEN_BUILDING_0 at 38000/38000 (score 380) and RESEARCH_DIMENSION_GATES at 5000/5000 (score 50).
- gamestate.xml has totalScore.researchCompleted = 130 (Bio-Transport 80 plus Dimension Gates 50), so the 380 for AB_0 is missing.
- victory.log shows AB_0 at 29051/38000 at 17:10:00 and "4 complete" at 17:11:30, with no research MessageBox ack in between. The driver was on loadout, crew and hire screens in that window.
- Both refuters scanned the other benchmark saves: 4 of 150 saves with a scored completion are short, each by exactly one topic's score. Over the 103 build-funding saves, 2 of 326 scored completions were lost (460 of 48,790 points).

The loss is observed. The mechanism (the event reaching a stage pushed in the same frame) is inferred from the code, not pinned by an event log.

**Fix (target both).** In Lab::update (research.cpp), in the Physics/BioChem isComplete() branch just before fw().pushEvent(...), add state->totalScore.researchCompleted += lab->current_project->score and the same for state->weekScore.researchCompleted. Then delete the two score lines in the ResearchCompleted case of CityView::handleGameStateEvent (HEAD cityview.cpp:4849-4850, LIVE 4924-4925) so nothing is counted twice. Do the same for alienIncidents (cityview.cpp:4847 LIVE) as the sibling fix. Keep resortTopicList and the RESEARCH COMPLETE dialog in the UI handler. Verify with a regression test that pushes a stage in the same frame as a lab completion and still sees the score credited. After the fix, the oa_play.py staffing guard written around this bug (lines 2617-2622) is no longer needed. Corrections to the finding text: "table average 6.8" is the pooled rate (21,010 over 3,093,000 hours), and the mean of per-topic rates is 10.1. "Teleporter 1.5" is the Personal Teleporter (1.54); the vehicle Teleporter is 2.67. The ten numbers listed for records 88-97 are scores, not man-hours.

## v_score / SCORE-C2-TRIGGER -> engine_bug (medium, high)

**Disputed point.** Three things were disputed. (1) Cadence: the validator said the EXE visits each building every ~N_buildings minutes independent of difficulty, so the engine's TICKS_PER_DETECTION_ATTEMPT table is wrong at Novice to Hard. The engine refuter said the cadence already matches. (2) Impact: the validator said 'high', with the engine under-generating incidents 4-13x for typical crews. The EXE refuter said the benchmark crews give about 1.1x, and that a separate forced path (owner == player) makes the engine over-generate. (3) Whether the species-gate, halving and final-roll formula limbs diverge from FUN_0006f964 at all. Both refuters agreed they do.

**Ruling.** The code-level verdict engine_bug stands, but the finding is refuted in two places and incomplete in one.

Formula limbs: confirmed. I disassembled FUN_0006f964 (VA 0x6F964, physical 0xD2008) myself with capstone from canonical/UFO2P.EXE. The prologue bytes 535152565755 match, and the output agrees line for line with the saved Ghidra listing.
- Species gate: EDI is zeroed at 0x6F9C6 and incremented at 0x6F9F5 only for species whose count byte is non-zero. The test is IDIV by EDI (100/k) against an inclusive 0..100 roll. This is reservoir sampling of which species to report, not a detection gate. The first present species passes 100/101 of the time.
- Roll range: FUN_0005d1d8 returns a value mod (n+1), so rolls are inclusive.
- Halving: FUN_00091de4(org,1) true skips the second test, and FUN_00091db0(org,0) then halves, so (allied-with-aliens OR hostile-to-X-COM) halves once. The org+0x11 taken-over byte halves a second time.
- Thresholds: the bytes at file 0x188E88/9 are 0xCE (-50, signed compare, so <= -50) and 0x4B (75).
- Final test: signed JGE against value-10.
- Table reads: the weight tables at file 0x142374 and 0x155354 read back as claimed.
The engine (LIVE building.cpp:777-832, HEAD :756-) uses 100/(count+1), exclusive rolls, and up to three independent halvings.

Cadence: the validator is wrong. FUN_00014854 (VA 0x14854) builds the filename UFODATA/CITYMAP<'1'+difficulty>.BLD from [0x10C9C]. It then does DIV 0xE2 on the loaded file size into [0xB3A68], which is the count of buildings 0xF3D74 wraps over. I checked the ISO files: CITYMAP1..5.BLD are 15368, 17628, 18984, 21696 and 24634 bytes, which gives exactly 68, 78, 84, 96 and 109 buildings (remainder 0). At 5 buildings per 5 minutes, that is a revisit every 68/78/84/96/109 minutes. The engine table 70/80/85/95/110 is that count rounded to 5, within 3%. The extractor also picks citymap1..5 by difficulty (extractors.cpp:376-392). The EXE-lens vote repeated the 'independent of difficulty' claim and is also wrong on this point. No cadence change is needed, and the validator's fix of one fixed interval would break the lower difficulties.

Impact: 'high' does not survive. The 4-13x figure holds only for large single-species stacks. At Superhuman, 12 Anthropods in a Corporate HQ give value 43, EXE 32.3% per visit against engine 2.3% (about 14x). 3 Anthropods in the Senate give EXE 2.94% against engine 0.75%. The saved Superhuman games show crews that are multi-species and small (6-7 species, counts 1-6). For those, the gate is near-certain in both formulas and the shared value-10 threshold dominates. I recomputed one survey row by hand: a Sensodrome crew with value 48 gives EXE 0.376 and engine 0.3217, as the survey printed. The EXE lens's survey puts aggregate EXE/engine at 1.11 on third-party buildings, with per-save median 1.09. That is an analytic snapshot estimate, not a simulation, but I found no error in it.

Missed in the finding: Building::updateDetection passes state.firstDetection || owner == state.getPlayer() as 'forced' (LIVE building.cpp:158, HEAD :158). That skips the gate and the final roll for any crewed X-COM-owned building, so it fires every 6 h timeout plus attempt interval, about 7 h. In FUN_0006f964 I found no owner bypass. X-COM-owned buildings roll the same formula. An X-COM Base function building has building weight 0 (indices 36 and 37 of the EXE table are Temple of Sirius and X-COM Base), so value <= 0 and it can never fire. Non-base X-COM-owned buildings, such as the owned Recyclotorium seen in the survey, use their normal weight. The EXE lens's snapshot model has the engine at about 1.46x the EXE overall, mostly from this path. It concentrates in the 18 of 103 saves that have a crewed X-COM building. Whether aliens can enter X-COM buildings in the EXE is unbound.

Net: engine_bug is confirmed, impact is medium. The aggregate on typical crews is about -10%, plus a forced-path over-generation, plus large per-case errors for big single-species stacks and taken-over orgs (up to /8 against EXE /4).

**Engine.** Building::detect(state, forced) (LIVE building.cpp:771-840, HEAD :750-). It is invoked from updateDetection (:139-159) once per TICKS_PER_DETECTION_ATTEMPT[difficulty] = 70/80/85/95/110 min per building (building.h:17-19), via GameState::updateEndOfFiveMinutes (gamestate.cpp:1526-1535), which stops after the first detection.
- Not forced: for each species with count>0, a roll randBoundsExclusive(0,100) < 100/(count+1) marks the building spotted. detectionValue = sum(weight*count) * (function->detectionWeight - 2*difficulty) / 100 (integer). It is halved separately for isRelatedTo(Aliens)==Allied, for isRelatedTo(player)==Hostile (x < -50) and for takenOver, so up to /8. The trigger is rand[0,100) < value-10.
- Forced is state.firstDetection (no time limit) or owner == state.getPlayer(). It skips both rolls and fires whenever any crew is present.
- On a hit the engine sets ticksDetectionTimeOut = 6 h. The AlienSpotted handler (cityview.cpp:4840-4852) applies -30 and opens AlertScreen.
- HEAD only: a base building raises DefendTheBase instead of AlienSpotted. LIVE removes that.

**EXE.** UFO2P.EXE non-4, FUN_0006f964 (physical 0xD2008). It is called from FUN_0004ab04 every 5 game minutes and visits 5 buildings round-robin over [0xF3D74] (stride 0xE2, wrapping at the per-difficulty building count [0xB3A68] = CITYMAP<1..5>.BLD size / 0xE2 = 68/78/84/96/109). It skips any building whose alert timer +0xBC is non-zero and stops the batch after the first incident.
- Species loop (0x6F9D2-0x6FA24): k counts the species with count>0. Roll r = rand16(100), inclusive 0..100. If r < floor(100/k), that species becomes the 'chosen' one (reservoir sampling, stored in the alert record). sum += count * alien_detection_weight[species], with weights 1,1,3,1,1,3,3,3,2,10,8,20,1.
- If [0xE0E7C]==1 && weekday==1 && hour<16, the sum is multiplied by 100. That is only the first 4 game hours (12:00-16:00 on day 1).
- value = sum * (building_detection_weight[function] - 2*difficulty) / 100. Table at file 0x155354: X-COM Base and Temple of Sirius = 0, Senate 155, Corporate HQ 130, and so on.
- value is halved ONCE if rel[org][Aliens] >= 75 OR rel[org][X-COM] <= -50, then halved again if org+0x11 (taken over) is set.
- Incident if chosen != -1 and rand16(100) < value-10 (signed). There is no owner==X-COM bypass.
- On incident the EXE sets the building state to 2, calls addScore(2,-30), sets the alert timer to 0x168 (360 min), and pushes the ticker message.

**Evidence.** EXE (UFO2P non-4, physical file offsets; code = VA+0x626A4, data = VA+0x616A4):
- FUN_0006f964 at 0xD2008. Prologue 535152565755 verified. Capstone disassembly from canonical/UFO2P.EXE matches the saved Ghidra listing. Species loop VA 0x6F9D2-0x6FA24 (inc edi at 0x6F9F5, idiv edi at 0x6F9F9). x100 test at 0x6FA26-0x6FA43. Weight scaling at 0x6FA46-0x6FA7A. Halvings at 0x6FA7C-0x6FAF1. Final roll and test at 0x6FAFE-0x6FB12.
- FUN_0005d1d8 (mod n+1, inclusive). FUN_00091de4 (rel >= [0x1277E5]) at 0xF4488. FUN_00091db0 (rel <= [0x1277E4]) at 0xF4454. Bytes at file 0x188E88 and 0x188E89 are 0xCE and 0x4B.
- Weight tables read from the EXE: file 0x142374 gives 1,1,3,1,1,3,3,3,2,10,8,20,1,0. File 0x155354 gives 49 values, with zero at indices 36 and 37 (Temple of Sirius, X-COM Base).
- Cadence: FUN_00014854 (VA 0x14854, physical 0x76EF8) reads the difficulty byte [0x10C9C] and builds the CITYMAP<n> name. At VA 0x14917-0x1492A it does div 0xE2 of the file size and stores the result in word [0xB3A68] (Ghidra VA 0x183A68). Files <extracted ISO>/XCOM3/UFODATA/CITYMAP1..5.BLD: 15368/17628/18984/21696/24634 bytes = 68/78/84/96/109 x 226, remainder 0.
- First-detection window: new-game init at VA 0xB95C1-0xB95D4 sets [0x10E7C]=1 and hour [0x4D68]=0xC.
- Engine: LIVE building.cpp:158 (forced call), 771-840 (detect), 139-159 (updateDetection); HEAD building.cpp:158 and 750-. building.h:13-19 (cadence table and its rounding comment). gamestate.cpp:717 (firstDetection=true) and 1526-1535. cityview.cpp:4840-4852 (AlienSpotted, -30, no player-owner exclusion). organisation.cpp:1085-1105 (Hostile is x < -50, Allied is x >= 75). tools/extractors/extractors.cpp:376-392 (citymap1..5 by difficulty).
- Survey: validate/r_v_score_3_1_work/survey*.py and survey_all.txt, over 103 Superhuman build-funding saves (the EXE lens's analytic snapshot, not a simulation). Aggregate EXE/engine is 1.11 on non-X-COM-owned buildings. I recomputed one row by hand and it matched. Worked examples: 12 Anthropods in a Corporate HQ, EXE 100/101*33/101 = 32.3% against engine 7%*33% = 2.3%. 3 Anthropods in the Senate, EXE 2.94% against engine 0.75%.
- LIVE and HEAD: the !forced block of detect() is identical in both trees. Only the tail differs (the base/DefendTheBase branch).

**Fix (target both).** In Building::detect(), !forced branch:
1. Species gate: replace 100/(count+1) with reservoir sampling over present species. Keep k = number of species with count>0 so far (1-based). Roll rand inclusive [0,100]. If roll < 100/k (integer), set chosen = this species. Spotted is chosen != none, so the first present species passes 100/101. Optionally store the chosen species for the alert.
2. Rolls: use inclusive [0,100] for the gate and for the final test. The final test stays signed: roll < value-10 fires.
3. Halving: compute (relation to Aliens >= 75) OR (relation to X-COM <= -50) and halve ONCE. Then halve once more if owner->takenOver. This replaces the three independent halvings. For the hostile test use <= -50 to match the EXE, since the engine's Hostile is x < -50.
4. Forced path: remove 'owner == state.getPlayer()' from the updateDetection call at building.cpp:158, so X-COM-owned buildings roll the normal formula. X-COM Base-function buildings have detectionWeight 0 and so can never fire, as in the EXE. Restrict firstDetection to the first 4 game hours (day 1, 12:00-16:00) and apply sum*100 only inside that window, instead of a no-time-limit forced detect.
5. Cadence: change nothing. TICKS_PER_DETECTION_ATTEMPT is already within 3% of 68/78/84/96/109. Do NOT replace it with a fixed interval. Optionally use the exact per-map building count as the interval.
6. The patch is the same in HEAD and LIVE.
7. Verify with a unit test of the two worked examples: 12 Anthropods in a Corporate HQ at Superhuman is about 32%, and 3 Anthropods in the Senate is about 2.9%.

## v_score / SCORE-C5 -> match (high, high)

**Disputed point.** Whether SCORE-C5's engine-side text is correct. The validator said the engine has no damaged-UFO withdrawal, so an EXE withdrawal "would add further charges" and fix_sketch was conditional. Both refuters agreed the match verdict stands but said that claim is false. They also objected to "largest measured negative" and to "reached indirectly" for the second EXE call site.

**Ruling.** The refuters are right on the disputed engine claim. The verdict stays "match", and the charge itself needs no fix. HEAD and LIVE both implement the damaged-UFO withdrawal, so the validator's "engine has no withdrawal" sentence, its strategy_impact clause and its conditional fix_sketch are wrong. Ships forced out by damage go through GotoPortal and pay the same charge. The note about the second EXE call site and the "largest negative" qualifier are also correct. The qualifier is unverified by me: it comes from a cached saves scan that neither refuter nor I re-read. The EXE rule and the engine rule both hold on my own check.

**Engine.** The charge is correct. HEAD:game/state/city/vehiclemission.cpp:1395-1400 and LIVE:game/state/city/vehiclemission.cpp:1398-1403 are identical. On a successful GotoPortal port-out of an aliens-owned vehicle in CITYMAP_HUMAN, the engine adds -v.type->score/4 to weekScore.incursions and totalScore.incursions. The `incursions` field is the sixth score field (index 5), matching EXE category 5 (HEAD:game/state/gamestate.h:62-68). For positive scores, C++ truncation of -s/4 equals the EXE's -(s>>2).

The engine does have the damaged-UFO withdrawal, from commit 34adbf58, an ancestor of HEAD (`git merge-base --is-ancestor` is true). Vehicle::withdrawBandEntered (HEAD:game/state/city/vehicle.cpp:2140, LIVE:vehicle.cpp:2165) tests crash_health <= health < maxHealth*percent/100. Vehicle::update (HEAD:vehicle.cpp:2167-2182, LIVE:vehicle.cpp:2192-2207) calls setMission(gotoPortal) for aliens-owned craft in that band. withdrawHealthPercent is set from UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE at HEAD:gamestate.cpp:1031/1099/1152 (LIVE 1032/1100/1153) and is serialized. Withdrawing ships therefore reach the same charge.

**EXE.** In UFO2P.EXE non-4 (CRC 0x4749ffc1), FUN_0003a910 charges any alien ship that leaves through a gate in the human city. Its entry is VA 0x3A910, file 0x9CFB4. It requires the arrived flag [veh+0x16a] != 0 and owner word [veh+0x12c] == 1, and the ship must sit on a gate tile of the current city.

It increments the destination city's per-type counter at [city*0x2D4 + type*2 + raw 0x73A6C]. It then tests byte [raw 0x5060 = VA 0xD5060] and skips unless the current city is 0, which is the human city. Otherwise it computes score>>2 on the zero-extended u16 at raw 0x5862C + type*126, which is floor(score/4). It then runs `mov eax,5; neg edx; call addScore`, and the call is at file 0x9D112.

FUN_000588f8 (the damaged-UFO withdrawal gate) sets the same flag unconditionally once the ship is inside the health band. Withdrawing ships therefore take the same charge. The table at file 0x189CD0 (stride 126) reads 50, 100, 150, 200, 250, 300, 300, 250, 500, 700 for types 0-9. That gives charges of 12, 25, 37, 50, 62, 75, 75, 62, 125, 175. These equal the engine's values.

**Evidence.** I read the bytes of canonical UFO2P.EXE myself. CRC is 0x4749ffc1 and the file is 1,702,206 bytes.
- Score table at file 0x189CD0 with stride 126 reads 50, 100, 150, 200, 250, 300, 300, 250, 500, 700, then 20 and 10 for types 10-11, which are outside the ten alien types.
- File 0x9CFF0 holds `cmp byte [edi+0x16a],0`, then `cmp word [edi+0x12c],1`.
- File 0x9D090-0x9D0F4 holds the owner==1 test, the imul by 0x2D4 and the counter increment at [..+0x73a6c]. It then loads `mov bl,[0x5060]; test bl,bl; jne`, which skips the charge when the current city is not 0.
- File 0x9D0F4-0x9D117 holds `xor edx,edx; mov dx,[eax*2+0x5862c]; mov eax,edx; sar edx,0x1f; shl edx,2; sbb eax,edx; sar eax,2; mov edx,eax; mov eax,5; neg edx; call rel32`. The rel32 resolves to file 0xFAA80, the addScore wrapper, VA 0x983DC + 0x626A4. That confirms the charge is -floor(score/4) in category 5.

Engine: I read HEAD and LIVE vehiclemission.cpp (identical charge block) and vehicle.cpp (withdrawal block, identical at the shifted line numbers). I also confirmed that the withdrawHealthPercent writes exist at three spawn sites, and that 34adbf58 is an ancestor of HEAD.

Not independently re-derived: the day-bucket means of incursions versus city damage. Both refuters cite the cached validate/work/saves_scan.json. They agree incursions exceed city damage only from days 20-24 on, at -1,768 versus -1,415. Days 5-19 show city damage larger. Those means already include withdrawal charges. No Ghidra copy was needed or created.

The second addScore(5) site, file 0x9ECD2 in the routine at file 0x9EC40 (VA 0x3C59C), has no recovered caller. It duplicates the inline block and is not load-bearing. FUN_0003a910's three callers are at VA 0x117F8, 0x118AE and 0x39C6A.

**Fix (target none).** None. No engine change is needed for the charge or for the withdrawal path, in either tree. Correct the finding text before it enters the fix plan:
- Replace "engine has no damaged-UFO withdrawal" with "engine implements withdrawal (commit 34adbf58) and charges withdrawing ships via GotoPortal".
- Drop the conditional "if the withdrawal band is ever implemented, the same charge must apply".
- Strategy rule: a UFO wounded into its withdrawal band but not killed flees and pays -floor(score/4) in both the EXE and the engine. Escort (role 11) and role 9 have empty bands and never flee.
- Qualify "largest measured negative" as true only from day 20 on.
- Say "no caller found" for the second EXE site.
- Update the stale exe_evidence_index entries D11 ("not implemented") and the section 5.6 "absent in OpenApoc" row.

## v_score / SCORE-C6 -> diverges (medium, high)

**Disputed point.** Whether the engine's vehicle-plow-through Damage to City charge (vehicle.cpp, VehicleMover::updateFalling) matches EXE writer 2 (FUN_00041644, file 0xA423D). Engine lens says writer 2 is reachable only for movement_type 2 (the Overspawn), so the engine charge on any falling vehicle is an engine-only extra. Exe lens says it is the falling/crashing mover and the engine placement is right. The projectile writer (file 0xB8AFC) and the wrong-city tile-value histogram were not in dispute: both refuters agree the projectile half matches and the 514x6 / 94x20 data claim is the ALIENMAP table.

**Ruling.** Engine lens is right on writer 2. The projectile half stays a match and the finding's tile-value summary is wrong (ALIENMAP distribution, not CITYMAP). The finding's blanket "match" must be split. Writer 1 (projectile, FUN_00054a28) matches the engine. Writer 2 (FUN_00041644) fires only for the Overspawn, and the engine charges every falling vehicle, so that part diverges. The exe lens read "[v+2]==2" as a falling/crashing mode. It is the vehicle slot's movement_type word, copied from the type record at spawn, and only type 33 (Overspawn) has the value 2. I reproduced every link from raw bytes with capstone; no Ghidra was needed.

**Engine.** Writer 1 (projectile hit): LIVE scenery.cpp:1183-1208 / HEAD 1173-1198 charges type->value on a hit when the city is not the alien city, the scenery is part of a building, and the firer owner is X-COM or the Aliens. This matches the EXE. Writer 2 (vehicle): LIVE vehicle.cpp:1027-1030 / HEAD vehicle.cpp:1002-1005 calls Scenery::chargeCityDamage(type->value) whenever a vehicle's random plow-through succeeds on building scenery inside VehicleMover::updateFalling. updateFalling runs for any vehicle with vehicle.falling set, flying or ground mover, any owner (set by Vehicle::startFalling for any downed craft). There is no class, owner or city gate. Added by commit fda36d6c. The engine has no Overspawn vehicle (NUM_VEHICLES = 33; Overspawn missions are converted to InfiltrateSubvert).

**EXE.** Writer 1: UFO2P.EXE non-4, FUN_00054a28, call at file 0xB8AFC (VA 0x56458): addScore(6, -byte[tileType+0xC]) for a projectile hit in the human city, inside a building footprint, with firer org 0 or 1, whether or not the hit destroys anything. Writer 2: FUN_00041644, call at file 0xA423D (VA 0x41B99), org hard-coded to 1. It has exactly two callers, VA 0x397A7 and 0x3A3A1 (files 0x9BE4B and 0x9CA45), both inside FUN_000395e4 and both behind `cmp word [esi+2],2`. Slot +2 is copied from vehicle_data movement_type at spawn (FUN_0005D6E4 at VA 0x5D770/0x5D775, file 0xBFE14/0xBFE19). movement_type is 1 for types 0-24 (all flying craft, including X-COM craft and UFOs), 0 for 25-32 (ground vehicles) and 2 only for type 33 (Overspawn). So the original charges Damage to City from this path only when an Overspawn tears down building scenery. A crashing X-COM craft, UFO or civilian craft never reaches it. No other writer of category 6 exists.

**Evidence.** All offsets are UFO2P.EXE ISO non-4; physical file offset = VA + 0x626A4 for code.
1. Updater call census: E8/E9 scan for calls to VA 0x983DC (file 0xFAA80) finds 9 sites, all E8. Only 0x41B99 (file 0xA423D) and 0x56458 (file 0xB8AFC) have `mov eax,6` (bytes `b8 06 00 00 00` before the call). A linear disassembly of the whole code region shows the category-6 slots 0x10C8C/0x10C90 are only read (score screen, file 0xFA7D7 and 0xFA803), so there is no inline writer.
2. FUN_00041644 callers: an E8/E9 scan finds exactly two, VA 0x397A7 and 0x3A3A1. No dword pointer to it exists. Guards: `cmp word [esi+2],2; jne` at VA 0x39736 (both jump tables at 0x395B4 and 0x395CC resolve to 0x39736 or to targets inside the guarded block) and `cmp word [esi+2],2; jne` at VA 0x3A398.
3. Slot +2 provenance: FUN_0005D6E4 (file 0xBFD88) does `mov dx,[edx+2]; mov [ebp+2],dx` with edx = 0x585E8 + 126*type, which is vehicle_data at VA 0x1285E8. It then special-cases `cmp dx,0x21` at VA 0x5D787.
4. vehicle_data +2 at file 0x189C8C + 126*i + 2: 1 for i = 0-24, 0 for i = 25-32, 2 for i = 33 only. Type 33 has size 1x1x3, constitution 1800 and weight 24000. Independent corroboration: tools/extractors/docs/hexa.txt:113 and :457 ("movement type, 00 = ground, 01 = flying, 02 = overspawn") and Appendix C "21 Overspawn" (hexa.txt:675).
5. No other route to slot +2 == 2. Of the 112 word stores to [reg+2] in the code, the only immediate `2` is VA 0x92368 (file 0xF4A0C), a 10-case switch that writes [ebx+2] = 0..7 and [ebx+4] = di, an action record and not a vehicle slot. The vehicle-slot writes are the spawn copy and 0xFFFF clears. There are 68 absolute-displacement references into vehicle_data, all reads or compares; no writes.
6. Engine side: HEAD vehicle.cpp:379 and :616 call updateFalling whenever vehicle.falling is set (flying and ground movers). Vehicle::startFalling is at vehicle.cpp:2057-2071. The charge at HEAD :1002-1005 and LIVE :1027-1030 is identical; git log -S shows it came from fda36d6c. tests/test_city_damage_score.cpp calls chargeCityDamage directly (line 43) and does not exercise the vehicle path.
7. The wrong-table data claim is confirmed by both refuters. The 514x6 / 94x20 histogram is ALIENMAP (632 types), which the projectile path never charges. The human CITYMAP (957 types) is mostly 20 (460), 10 (124), 4 (115) and 0 (112), with only 18 types worth 6. Footprint-weighted mean is about 9 per hit, with about 35% of hits costing 0.

**Fix (target both).** 1. In game/state/city/vehicle.cpp, delete the `if (tile->presentScenery->building) { Scenery::chargeCityDamage(...); }` block and its comment in VehicleMover::updateFalling (HEAD ~1000-1005, LIVE ~1025-1030). Falling or crashing craft then cost no Damage to City, as in the original. Keep Scenery::chargeCityDamage and hitChargesCityDamage for the projectile path in scenery.cpp.
2. Correct the docs and commit-message claim (findings/city-damage-scoring.md and the fda36d6c text "a vehicle plowing through building scenery is charged whoever owns it") to say writer 2 is Overspawn-only. Add a test that a plow-through by a falling vehicle leaves cityDamage unchanged.
3. Optional and low priority: if an Overspawn actor is ever added, charge -value to X-COM (org hard-coded 1) when it clears building scenery. The Overspawn first appears around week 8, after the Mothership, so it is not reachable in the benchmark horizon.
4. Correct the finding's strategy rule: a projectile hit on a human-city building tile costs 0 to 40 (about 9 on average per footprint hit, most often 20), not "6, or 20 on big tiles". Craft crashes cost nothing. The benchmark's city-damage sink includes engine-only crash charges; their share is unmeasured.

## v_score / SCORE-C0 -> diverges (low, high)

**Disputed point.** Whether the validator's EXE formula for the Category 0 tactical score is complete. The validator said the Total is combat rating - casualties + leadership + equipment, with the live-alien and corpse lines zero and aliens entering only the leadership base. The engine-lens refuter said Ghidra's decompile dropped two reachable blocks of TACP FUN_00022f90, and that the stunned-alien credit also goes into the Total whether or not containment exists. The verdict 'diverges' itself was never disputed.

**Ruling.** The refuter is right on the formula, so the validator's EXE half is withdrawn. The verdict stays 'diverges' because several differences hold independently of the disputed blocks.

I read the TACP bytes with capstone and the Ghidra full listing (copy adj_30, since deleted); the two agree. Ghidra's 'removing unreachable block' warnings come from the decompiler folding hand-off globals to constants. The listing shows the stunned-alien block (Part F, file 0x7DDCE) and the alive-hostile block (Part D2) as live code. TACP never writes [0x1ED2C8]. The UFO2P campaign launcher writes 1 to it unconditionally, so Part F is live in campaign battles.

Correction to the refuter: UFO2P has a second writer that stores 2 (file 0x11E984). It sits in a scenario initialiser that is only reached through a UI callback, probably the skirmish setup. In that mode Part F does not fire, and the engine already skips score for skirmishes.

Corrections to the validator and the refuter's list:
- The corpse line is always 0, as the validator said.
- Stunned aliens pay v once in the Total and once in the leadership base. The engine has the same two-place structure with the same amounts when containment exists.
- The Total credit does not depend on containment. The test of [0x1ED350] comes after the ADD EBX,EDX and only chooses the display line.

Impact is demoted from the validator's 'medium' to 'low'. Per-battle swings are tens of points, up to roughly 100-150 in a heavy-loss battle. In the 100 benchmark saves the lifetime tactical score averages 0-163 against a 1000 first gear threshold, and the campaign total is about -3000. The H < -2400 funding cliff does not change this: 78% of saves at day 20 or later are already below -2400, so a swing of tens of points flips only marginal weeks. The fixes are cheap and well evidenced, so they are worth doing for fidelity.

Taken from the refuter, not re-read by me (medium confidence):
- civilian v = 10;
- Corporate Boss scores 0 in the EXE and 10 in the engine;
- the engine's *_ALIVE item scores equal the agent scores for all 13 species.

Inferred, not verified: the runtime org-to-side byte table at VA 0x1B1FB1 (zero in the file image). Whether unit+0 is the agent-type index is also inferred, which makes unit types 1 and 5 the X-COM Biochemist and the Corporate Boss.

Open items that do not change the verdict:
- The meaning of types 1, 5 and 0x15 in the 'defeated' predicate. It is moot for Alien-org units and only matters for non-alien hostile VIP types.
- Part D2 and the FUN_00025310 objective gate. The decisive next check is to decode FUN_00025310 (TACP VA 0x25310, file 0x7FDB4) and list which launcher callers pass an edx outside {1,3,5} with an Alien-owned target.
- Whether FUN_00022f90 is re-run immediately before the export.

**Engine.** LIVE and HEAD are identical for every score line; LIVE's only battle.cpp change is a 7-line category-7 addition at 3581 (the other diffs are renames or casts).

BattleScore::getLeadershipBonus() = (100 + 3*casualtyPenalty) * (combatRating + friendlyFire + liveAlienCaptured) / 100, with no clamp (LIVE battle.cpp:4071-4080, HEAD battle.cpp:4064-4073). getTotal() = combatRating + casualtyPenalty + leadership + liveAlienCaptured + equipmentCaptured + equipmentLost. friendlyFire is not in the Total, only in the leadership base.

Score is applied at LIVE battle.cpp:3211-3212 (HEAD same). Inputs:
- combatRating += v only when the attacker is the player and the relation is Hostile (battleunit.cpp:5112-5114).
- casualtyPenalty -= v for each dead player unit (battleunit.cpp:5107), for MIA at battle.cpp:2793, and -v at brainsuck (battleunitmission.cpp:1718).
- friendlyFire -= 10 flat per teamkill (battleunit.cpp:5055).
- A non-hostile, non-own victim killed by the player has no score effect.
- liveAlienCaptured only when the player has alien storage (battle.cpp:2897-2902); nothing for stunned non-alien hostiles.
- equipmentLost is charged at destruction (battleitem.cpp:34); equipmentCaptured at battle.cpp:2852 and 2990.

**EXE.** TACP.EXE non-4, FUN_00022f90 (file 0x7DA34), plus the death handler and the brainsuck conversion.

Per death, with v the dying unit's score byte at unit+0x44:
- If the killer is hostile to the victim, M[killerSide][victimSide] += v; otherwise it is -= v. That covers civilians, other non-hostile sides, and teamkills.
- C[victimSide] += v always.

At scoring time, with Sum(row) = sum over k of M[X-COM][k], C = X-COM casualty points, and state 1 = win:
- Combat rating = Sum(row), the whole X-COM row including negative entries, plus v for each incapacitated non-alien hostile on a win.
- Leadership base = the POSITIVE row entries only, plus v for each incapacitated hostile of any org on a win.
- Leadership = 0 if 30*C > 1000 (C >= 34), else base*(100-3C)/100, truncating.
- Total = Sum(row) + v of incapacitated non-alien hostiles - C + leadership + v of incapacitated Alien units (Part F, credited to the Total regardless of containment) + equipment captured - equipment lost.
- [0x1ED350] only picks whether the Part F amount shows on the Live Aliens line or the Combat Rating line.

Exact 'defeated hostile' predicate: the unit exists, health word [unit+0x34] != 0, and (health <= word [unit+0x50], OR type word [unit+0] == 5, OR type == 1).

Brainsuck conversion: M[alien][victim] += 2v, M[victim][alien] -= v, M[victim][victim] -= v, C[victim] += v. The X-COM side takes about -3v.

Part D2 pays v per remaining conscious hostile, only for forced wins on Alien-targeted type-1 missions (inferred; objective gate FUN_00025310 not decoded).

Equipment is scored on the end state: items held by a conscious holder of a different org, or on the ground after a win or loss. Destroyed items are dropped and never charged.

The UFO2P hand-off adds [0x24408C] as category 0. The block offsets match: Total at +0xA0, win state at +0x98, flag at +0x34.

**Evidence.** All offsets are physical file offsets in the canonical non-4 EXEs, checked with capstone and the Ghidra full listing (TACP FUN_00022f90, FUN_00021998, FUN_00060720, FUN_0006cf64). TACP code file = VA + 0x5AAA4; UFO2P code file = VA + 0x626A4.

TACP.EXE non-4:
- Positive-only leadership base: file 0x7DBF5 (TEST EDI,EDI / JLE / ADD ESI,EDI at 0x7DBF9).
- Leadership clamp CMP EAX,0xFFFFFC18, with EAX = -30C: file 0x7DDA4. JGE at 0x7DDA9, otherwise XOR ESI,ESI at 0x7DDAB.
- Part F starts at file 0x7DDCE.
- The unconditional ADD EBX,EDX (adds to the Total) is at file 0x7DE4D, before TEST EBP,EBP at 0x7DE4F, which only picks the display line.
- Part F gates: unit org word == 1, player org != 1, state [0x1ED32C]==1, [0x1ED2C8]==1.
- Casualty add C[side(victim)] += v: file 0xBB736 (VA 0x60C92).
- Matrix +/- v in the death handler: file 0xBB393-0xBB3FD.
- Brainsuck matrix writes: file 0xC7AEA-0xC7B7E.
- The only TACP references to [0x1ED2C8] are loads and compares.
- Exporter gate CMP [0x1ED2C8],0 at file 0x6B52D, call FUN_00015494 at 0x6B536.
- Part D2 gate and state promotion: file 0x7DB1F-0x7DBB0.

UFO2P.EXE non-4:
- Campaign launcher FUN_000aff9c (file 0x112640) stores [0x244020]=1 at file 0x112672.
- Flag-2 store at file 0x11E984, in the initialiser at file 0x11E95C, sole caller at VA 0xBA747 inside FUN_000ba5f0 (file 0x11CC94).
- Post-battle CMP [0x244020],2: instruction at file 0x114454.
- [0x24404C] store at file 0x11275A.
- Mission-type stores [0x244024] at files 0x1127BB, 0x1127CC, 0x1127D8 and 0x1127E7.

Engine: LIVE battle.cpp:4071-4080 and HEAD battle.cpp:4064-4073 are byte-identical; battleunit.cpp:5055, 5107 and 5112-5114; battle.cpp:2793 and 2897-2902; battleitem.cpp:34; battleunitmission.cpp:1718.

Limits: static analysis only. The org-to-side table is runtime-filled. Not re-read: the civilian and Corporate Boss scores, and the per-species item scores.

**Fix (target both).** In BattleScore and its feeders, in HEAD and LIVE (battle.cpp, battleunit.cpp, battleunitmission.cpp, battle.h):

1. Clamp the leadership bonus to 0 once casualty points reach 34 (4 dead agents at v=10), else base*(100-3C)/100. This is the highest-leverage item.
2. Build the leadership base from positive credits only: hostile kill v plus the v of each defeated hostile on a win. Take friendlyFire out of the base and fold it into the combat rating as -v.
3. Add -v on the combat rating when the player kills a non-hostile, non-own unit (civilians, other non-hostile orgs).
4. Brainsuck: charge the combat rating -2v plus casualty -v (about -3v), instead of casualty -v only.
5. On a win, credit v to the Total and to the base for each defeated hostile. For Alien-org units that is the existing liveAlienCaptured amount, but remove the containment gate from scoring and keep the containment test only for loot or bio-storage. Also add v for defeated non-alien hostiles.
6. Use the exact predicate: the unit exists, health != 0, and (health <= the stun threshold, OR type == 5, OR type == 1).
7. Equipment: switch to end-state scoring and drop the destruction-time charge at battleitem.cpp:34. Held items count when the holder is conscious and of a different org than the owner. Ground items count on a win (captured if the owner is not the player) or a loss (lost if the owner is the player). Re-verify once the item record layout is fully decoded.
8. Optional and conditional: Part D2, the forced-win payout for remaining conscious hostiles. Defer until FUN_00025310 is decoded. Also skip the Corporate Boss v=0 vs 10 change until re-verified.

## v_score / SCORE-UNLOCK-CRAFT -> diverges (low, high)

**Disputed point.** Whether UFO2P.EXE latches the nine score-unlock flags (alien craft module gating) only at the Monday rollover, so the EXE lags the engine's live read by up to a week (validator, medium impact), or on every game-day change, so the lag is at most one game-day (engine-lens refuter, low impact).

**Ruling.** The refuter is right on the disputed point and the finding is downgraded. I disassembled the canonical UFO2P.EXE (non-4, CRC 0x4749ffc1) with capstone and checked three things.

1. Daily call chain. FUN_0004ac40 (entry file 0xAD2E4) handles the day change: the hour counter reaches 0x18 at 0xAD372, then the weekday [0x4d64] is incremented and wraps at 7. The weekday==0 block (`test di,di / jne 0xAD4A5` at 0xAD478) holds only the week counter increment and the funding calls (0xF6880, 0xD1450, 0xF8E48). Every day rejoins at 0xAD4A5, tests [0x5060]==0, and calls 0xDCB18, 0xD2410 and `call 0xFA234` at 0xAD4B8. A second entry, FUN_0004ac0c at 0xAD2B0, ends in `jmp 0xFA234` at 0xAD2C3 and is called from 0x7427C. A scan of every E8/E9 in the file for target 0xFA234 finds exactly those two sites.
2. The weekday gate inside FUN_00097b90. `cmp word [0x4d64],0 / jne 0xFA270` at 0xFA23A skips only the loop that folds week into previous. The latch at 0xFA27E-0xFA2E5 always runs. It tests the lifetime sum (week + previous) against the 9x5 table at file 0x142174, column = word 0x10C9C. The result is `cmp ... / jl skip / mov byte [0x10b84+k],1`, so a flag is set when lifetime >= threshold. The flag bytes start at zero (file 0x142228). The Superhuman column is 1000, 2000, 4000, 8000, 16000.
3. Consumers. FUN_000b3e84 (file 0x116528) gates items 44-48 on flags 4-8. Vehicle creation at 0xC0482-0xC04A0 applies it only when owner [ebp+0x12c]==1 (Alien). The recovery-loot loop at 0x115C79-0x115CF3 walks the vehicle TYPE's 45 slots (record 0x585E8 + type*0x7E, +0x51) and filters them with the flags as they stand at recovery time.

So "evaluated once per Monday rollover" and "the EXE only sees a new total after the next Monday" are false. The flags can lag by at most about 24 game-hours. The validator's strategy_impact of medium rested on a week of lag, so it should be low.

A small real divergence remains, so the verdict stays diverges and not match.
- The engine reads the live lifetime craftShotDownUFO, with no daily latch.
- Engine loot is a snapshot of what passed the gate when the craft was last equipped. EXE loot is the type's static module list filtered at recovery time. A craft built before a threshold crossing and recovered after it gives extra modules in the EXE but not in the engine.
- The EXE has no flag for rows 0-3 (agent items) with any live effect, and row 8 (module 48) is inert. The engine omits both, which is harmless.

The engine is not "re-evaluated continuously". It is evaluated only when a craft is equipped: city.cpp placeVehicle, gamestate.cpp incursion launch, and vehiclemission.cpp non-player portal crossing. Whether the EXE also re-creates or re-equips a craft at portal crossing is unproven. I did not run Ghidra Callers on FUN_0005d6e4.

Benchmark impact is nil. Superhuman runs stay far below the 1,000 first threshold; this figure is validator-reported and the refuter could not re-verify it.

**Engine.** Vehicle::equipDefaultEquipment (LIVE vehicle.cpp:3928-3951, HEAD vehicle.cpp:3883-3906; the function body is identical in both trees). For an Alien-owned craft it skips any default module where state.totalScore.craftShotDownUFO (lifetime, live) < etype->scoreRequirementFor(state.difficulty). Superhuman thresholds are 1000, 2000, 4000 and 8000. `loot.push_back` runs for every module that passes the gate. It runs only when a craft is equipped: city.cpp placeVehicle (LIVE 984, HEAD 963), gamestate.cpp incursion launch (LIVE 1034, HEAD 1033), and vehiclemission.cpp non-player portal crossing (LIVE 1437, HEAD 1434). There is no daily latch and no per-tick path, and craft already in flight never gain a module.

**EXE.** UFO2P.EXE (non-4) sets nine saved one-way flags [raw 0x10B84+k] inside FUN_00097b90 (file 0xFA234), which runs on every game-day change. The weekday==0 branch gates only the week-into-previous fold. Flag k is set when lifetime (week + previous) of category 3 (UFO kills, rows 4-8) or category 0 (tactical, rows 0-3) >= table[k][difficulty]. Flags are never cleared. They are consumed in two places. FUN_000b3e84 (file 0x116528) is called from vehicle creation at file 0xC04A0 for Alien-owned craft and clears locked module slots. It is also called from the recovery-loot builder at file 0x115C82, which filters the type's static module list by the flags at recovery time. Row 8 (module 48) is inert.

**Evidence.** EXE (UFO2P non-4, canonical/UFO2P.EXE, capstone, physical file offsets):
- FUN_0004ac40 at file 0xAD2E4. Day increment at 0xAD372-0xAD3A8. The weekday==0 block spans 0xAD478-0xAD4A0 and holds only the week counter and funding calls. The rejoin at 0xAD4A5 leads to `call 0xFA234` at 0xAD4B8.
- FUN_0004ac0c at 0xAD2B0 ends in `jmp 0xFA234` at 0xAD2C3.
- A scan of all E8/E9 instructions for target 0xFA234 returns only 0xAD4B8 (call) and 0xAD2C3 (jmp).
- FUN_00097b90 at 0xFA234: `cmp [0x4d64],0 / jne 0xFA270` at 0xFA23A guards only the fold loop at 0xFA244-0xFA26E. The latch at 0xFA27E-0xFA2E5 is unconditional.
- Table at file 0x142174 read directly. Superhuman column rows 0-8: 1000, 2000, 4000, 8000, 1000, 2000, 4000, 8000, 16000. Flag bytes at file 0x142228 are all 0.
- FUN_000b3e84 at 0x116528: jump table over item-0x2C in 0..4 tests [0x10b88..0x10b8c].
- Creation at 0xC0475-0xC04A0: `cmp di,1` on [ebp+0x12c].
- Recovery loot at 0x115C79-0x115CF3: type record 0x585E8 + type*0x7E, +0x51, predicate call at 0x115C82.
Engine: LIVE vehicle.cpp:3928-3951 equals HEAD vehicle.cpp:3883-3906 (diff of the two ranges is empty). GameScore::reset at gamestate.cpp:2095-2103 is not applied to totalScore. totalScore.craftShotDownUFO is lifetime and is credited at vehicle.cpp:2095. Not verified: the "benchmark stays below 1,000" claim, and any EXE re-creation of craft at portal crossing (FUN_0005d6e4 callers not queried). No Ghidra copy was created, so there is nothing to delete.

**Fix (target both).** Low priority and optional. If kept as a fidelity item: (1) add 9 saved latch bytes to GameState, with serialization. (2) In GameState::updateEndOfDay (every day, NOT updateEndOfWeek), set latch k when lifetime totalScore.craftShotDownUFO >= table[k][difficulty]. Rows 4-7 map to the existing cequip_score_req_data; row 8 and rows 0-3 may be left out, since they are inert or have no live effect. (3) Have equipDefaultEquipment read the latches instead of the live total. (4) Optionally rebuild UFO recovery loot from the type's initial_equipment_list filtered by the latches at recovery time, instead of the creation-time `loot` snapshot. Do not add a Monday-only latch; that would introduce a divergence the EXE does not have. Correct the finding text: "latches every game-day, not weekly" and "evaluated at craft creation, incursion launch and portal crossing, not continuously".

## v_score / SCORE-EQLEVEL -> diverges (low, high)

**Disputed point.** Whether SCORE-EQLEVEL can be called "match, fix: none" when its own exe_behaviour includes "a random row of 10 per weapon/grenade/equipment group", given that the engine's per-group draw is not over 10 rows. The engine refuter argues that empty grenade and equipment rows are dropped by the extractor, so the engine issues gear more often than the EXE.

**Ruling.** Refuter upheld. The level-selection half is a true match: the 5x7 threshold table, the lifetime tactical-score input, the species gating and the count-of-thresholds-<=-score rule all agree. The gear-draw half diverges, and the finding's own exe_behaviour asserts that half. In UFO2P (FUN_000ad38c) and TACP (FUN_000250f8) each of the three groups draws its own uniform row 0..9. An empty row issues nothing (amount<=0 for grenades, both words zero for equipment). The engine extractor drops empty grenade and equipment rows, so the engine draws only among the non-empty rows and issues something every time any row is non-empty. The validator never examined the engine's sampling. The impact is low, not medium. The benchmark's tactical score stays far below the 1000 first threshold, so aliens sit at level 0, where neither side issues grenades or equipment. The divergence matters only for runs or human play that push tactical score past 1000, where the engine over-arms aliens.

**Engine.** The extractor tools/extractors/extract_agent_equipment.cpp (identical in LIVE and HEAD) loops 10 rows per level and pushes a grenade entry only if grenade_idx>0 && grenade_amount>0. It pushes an equipment entry only if either equipment word is >0. Weapons are pushed if weapon_idx>0, and all 10 rows are filled at every level. EquipmentSet::generateEquipmentList (aequipmenttype.cpp:58-88) then picks uniformly among whatever was pushed with randBoundsExclusive(0,size), and issues it every time the vector is non-empty. Level choice is agent.cpp:165-167: EquipmentSet::getForType(state, Alien, state.totalScore.tacticalMissions), with bands min<=score<max. Resulting engine P(issue) is 1.0 for grenades at levels 1-4 and for equipment at level 4 and above. Compaction leaves 3, 6, 8 and 10 grenade entries at levels 1-4, and 7 equipment entries at level 4.

**EXE.** Each of three groups (weapon, grenade, equipment) calls the RNG with param 9. The RNG at file 0xBF87C ends in a divide that returns a value mod (param+1), so the row is uniform over 0..9. UFO2P FUN_000ad38c draws at file 0x10FADC (weapon), 0x10FB83 (grenade) and 0x10FBE8 (equipment). TACP FUN_000250f8 draws at 0x7FBF2, 0x7FCB3 and 0x7FD24. Empty rows add nothing: the grenade loop is guarded by `test eax,eax; jle` on the amount word (UFO2P 0x10FBBC, TACP 0x7FCEA), and equipment is guarded per word by `test edx,edx; je` (UFO2P 0x10FC0D, TACP 0x7FD60). The level loop at UFO2P 0x10FA6E-0x10FA99 counts thresholds <= (cat0 week + previous). The Superhuman row 1000/2200/4000/6600/10000/18000/28000 is at UFO2P file 0x19BEB4. Non-empty rows out of 10, as weapons/grenades/equipment per level: L0 10/0/0, L1 10/3/0, L2 10/6/0, L3 10/8/0, L4 10/10/7, L5-L7 10/10/10. P(grenade) is 0.3, 0.6, 0.8 and 1.0 at levels 1-4. P(equipment) is 0.7 at level 4 and 1.0 at levels 5-7.

**Evidence.** I re-derived this independently from the EXE bytes with capstone, not from the refuter's write-up. UFO2P.EXE (non-4, canonical) was disassembled over file 0x10FA30-0x10FD00, and the RNG at 0xBF87C-0xBF902. I decoded the tables at file 0x19BF40 (weapons, 48-byte row stride, 6 bytes per level), 0x19C120 (grenades, 32-byte row stride, 4 bytes per level) and 0x19C260 (equipment, 32/4), and counted non-empty rows per level; the counts match the refuter's. Weapon rows have clip_amount>=1 whenever clip_idx>0, so the engine's max(amount,1) causes no weapon divergence. TACP.EXE (non-4) was disassembled over file 0x7FB9C-0x7FD88, showing the same three independent `mov eax,9` draws and the same empty-row skips. The engine side was read at HEAD: tools/extractors/extract_agent_equipment.cpp:135-143, game/state/rules/aequipmenttype.cpp:58-88, game/state/shared/agent.cpp:165-167 and game/state/rules/aequipmenttype.h:263. diff -q shows these files are byte-identical in LIVE and HEAD. v_score.md line 223 supports the low impact: tactical score stays far below the 1000 first threshold in the benchmark runs.

**Fix (target both).** Keep the 10-row denominator. Preferred: in extract_agent_equipment.cpp, push all 10 grenade and equipment rows per level for the alien sets. An empty row becomes a GrenadeData with a null grenade and amount 0, or an EquipmentData with an empty list. generateEquipmentList already issues nothing for amount 0 or an empty list, so it needs no change, and it keeps drawing uniformly over a size-10 vector. Alternative: keep the compaction and have generateEquipmentList draw r in [0,10) and issue list[r] only if r < list.size(), which requires the set to be stored in row order. Check that serialization tolerates empty StateRefs. Regenerate base_gamestate and the equipment sets after the change, because the extractor output is generated and untracked and a stale save keeps the old sets. Update unit and golden tests that count entries. Optional follow-up, not part of this finding: the human-set path (extractor lines ~1093/1101) compacts the same way, and a human-set comparison against FUN_000ad148 was not done. Strategy-benchmark priority is low.

## consistency / UFO_GROWTH_STOP -> diverges (high, high)

**Disputed point.** Which destroyed alien building stops weekly UFO growth in UFO2P.EXE, in which direction the engine diverges, and whether the 'destroyed byte' framing and the alien-building index binding hold. The three findings (VC-9, RR-15, F4) name the same building and the same mechanism. F4's validator text reversed the direction, mislabelled case 5, and was refuted by both lenses.

**Ruling.** All three findings give one answer. The building is the Control Chamber, which is function index 7 in name-table order and the 6th raid of the forced 10-raid chain. VC-9 and RR-15 are correct on direction. F4 as originally written was wrong, and both refuters were right to refute it, while keeping the verdict 'diverges'.

What was wrong in F4:
- The direction was reversed. The EXE cuts growth two raids EARLIER than the engine. It does not cut it later.
- The raid-order number for the Organic Factory was wrong. It is the 8th raid. Position 5 is only its name-table index.
- Case 5 was mislabelled. Byte 0xDE63E is research record 91, the Megapod topic's unlock flag, not an 'Organic Factory destroyed' flag. Drop the fix-sketch idea of adding a case-5 flag.

What was wrong in RR-15: the write citation 'VA 0xB4587 / file 0x116C2B' is case 8's operand. The case-7 write is VA 0xB457D, file 0x116C21.

Index spaces. 'Index 7' means function index or name-table order f. '6th' means raid order, f = 4,2,6,8,0,7,1,5,3,9. The two must not be mixed.

Engine side. The engine stops growth when the Organic Factory (BUILDINGFUNCTION_ORGANIC_FACTORY, raid 8) is dead, and fails open if none exists. This rests only on UFOpaedia flavour text (ufogrowth.h:21-23), not on EXE code.

Binding (the VC-2 question). For this gate the engine binds correctly. In extract_buildings.cpp the function id, name, crew, battle map and score are all keyed by function_idx, the same key the EXE reads at building+0xAA. So BUILDINGFUNCTION_CONTROL_CHAMBER really is f=7. VC-2's file-position mis-binding affects only accessTopic and researchUnlock numbering, and does not affect this gate. The fix must therefore key on the function id. It must not key on RESEARCH_UNLOCK_ALIEN_BUILDING_6. That topic coincides with the Control Chamber win today, but the VC-2 re-key would renumber it.

Impact stays high, as all seven votes held. The growth pool at 0x143D40 is drained by launches (FUN_0006cfb4). After raid 6 the EXE's reserve can only drain, while the engine keeps refilling it for two more raids.

**Engine.** GameState::updateUfoGrowth returns early only when UFOGrowth::craftFactoryIntact() is false (LIVE game/state/gamestate.cpp:1626, HEAD :1593). craftFactoryIntact (ufogrowth.cpp:51-70, byte-identical in LIVE and HEAD) scans CITYMAP_ALIEN buildings whose function.id is BUILDINGFUNCTION_ORGANIC_FACTORY. It returns true if any is alive, and returns !sawFactory otherwise, so it fails open. The gate is reached from updateEndOfWeek (LIVE :1734, HEAD :1701), which is called weekly and once at new game.

A won RaidAliens battle (battle.cpp:3571-3600) forceCompletes the building's researchUnlock and calls building->collapse(). That makes Building::isAlive() (countActiveTiles()>0) false at once. Nothing in game/ watches CONTROL_CHAMBER.

In the generated buildings.xml the chain by accessTopic is Sleeping 0, Food 1, Farm 2, Maintenance 3, Incubator 4, Control 5, Spawning 6, Organic 7, Megapod 8, Gate 9. That is the same chain as the EXE. The Control Chamber is therefore the 6th raid and the Organic Factory the 8th.

**EXE.** UFO2P.EXE, ISO non-4 (CRC 0x4749ffc1). FUN_0006edac begins with CMP byte [0xDE62A],0 / JNZ to its epilogue. If the byte is non-zero, the whole weekly add loop is skipped. The loop computes min(count[t] + row[t], cap[t]) on the stock at 0x143D40. The same routine runs every Monday after funding and once at new game; both paths pass through the flag test.

Byte 0xDE62A is byte0 of research-status record 89 (0xDE2B0 + 10*89). Its initial value is 0. It is written only by FUN_000b4538 case 7, which is MOV byte [0xDE62A],1.

FUN_000b4538 has one caller, FUN_000b32ac at VA 0xB33BE. That call runs on a won raid, under these guards:
- the alien-dimension flag [0xD5060] is non-zero;
- the mission type is 1 (or 4);
- the success flag [0x244084] equals 1.

The argument is [0x244048], which FUN_000aff9c stores as the word at building+0xAA, the function_idx. Function index 7 is the Control Chamber. The same win also sets DAT_000f3c78 = 1 (VC-10, a separate finding), and the destruction score is credited to category 7.

The nine writes (cases 0-8) form the raid-unlock chain. Each one sets the next building's research-topic unlock, not a per-building destroyed flag. Case 5 writes 0xDE63E, which is record 91, the Megapod topic. Growth never reads that byte. No code clears 0xDE62A, so the cut-off is permanent.

Result: when the Control Chamber (6th raid) is won, weekly growth stops permanently. The Organic Factory (8th raid) has no growth effect.

**Evidence.** All offsets are physical file offsets in canonical UFO2P.EXE (CRC 0x4749ffc1). Code file = VA + 0x626A4. Data file = VA + 0x616A4. I re-checked each point myself with Python raw-byte reads and Ghidra listings on copy adj_33, since deleted.

- Gate. File 0xD1450 reads 53 51 52 56 57 | 80 3d 2a e6 00 00 00 | 75 57. The JNZ lands at file 0xD14B5, which holds 5f 5e 5a 59 5b c3 (pop..ret). The Ghidra listing shows CMP byte [0xDE62A],0 at VA 0x6EDB1 and JNZ 0x6EE11. The initial value at file 0x13FCCE is 0, and the record-89 bytes at 0x13F954+890 are 00 00 ff ....
- Writer table. The jump table at file 0x116BB4 has entries 0xA4545, 454D, 4555, 455D, 4565, 456D, 4575, 457D, 4585, 458C (+0x10000 = VA). Case 7 at VA 0xB457D, file 0x116C21, is c6 05 2a e6 00 00 01 c3. Cases 0-8 write 0xDE666, DE652, DE65C, DE67A, DE634, DE63E, DE670, DE62A, DE620. Case 9 is a bare RET (c3).
- Raw scan. A whole-file scan for the operand 0xE62A (2a e6 00 00) finds exactly 4 hits: three CMPs and one write. The CMPs are at file 0xACAE1 (FUN_0004a33c), 0xD1457 (FUN_0006edac) and 0x116D00 (FUN_000b45b8). The write is at 0x116C23, case 7. For 0xE63E the hits are two reads (0xACAAB, 0x116CE4) and one write (0x116C13).
- Chain. 0xDE62A is record 89 and 0xDE63E is record 91. The case-to-record map is 0->95, 1->93, 2->94, 3->97, 4->90, 5->91, 6->96, 7->89, 8->88. Since 88+g is building g's topic, the raid order is f = 4,2,6,8,0,7,1,5,3,9. That puts f=7 in 6th place and f=5 in 8th.
- Call path. The FUN_000b32ac listing shows the guards at VA 0xB32E9-0xB32FD. The Mutant-alliance branch is at 0xB3345 (f==2). The f==7 sets [0xF3C78] at VA 0xB33A6-0xB33AF. Then MOV EAX,[0x244048]; CALL 0xB4538 at VA 0xB33B9-0xB33BE (file 0x115A62). FUN_000aff9c, VA 0xB009C-0xB00AB: MOV EBX,[EAX+0xA8]; SAR EBX,16; MOV [0x244048],EBX.
- Names. The name table at file 0x14AD23 reads Incubator, Spawning, Food, Megapod, Sleeping, Organic Factory, Alien Farm, Control Chamber, Maintenance Factory, Dimension Gate Generator. TACP briefing index 7 is Control Chamber. The extractor map list index f-1+39 gives 46contrl for f=7 and 44organ for f=5 (extractors.cpp:15-70, 136-150).
- Engine. LIVE gamestate.cpp:1617-1630 and HEAD :1584-1597 call craftFactoryIntact. ufogrowth.cpp:51-70 and ufogrowth.h are identical in LIVE and HEAD. extract_buildings.cpp:76-91 names, maps and crews by function_idx, but binds accessTopic and researchUnlock by file position i. The generated buildings.xml shows Control Chamber = RESEARCH_ALIEN_BUILDING_5 and Organic Factory = RESEARCH_ALIEN_BUILDING_7. battle.cpp:3571-3600 and building.cpp:1065-1165 (isAlive at 1161) show the collapse path. A grep finds no CONTROL_CHAMBER reference in game/ code.

Notes are at validate/adj_v_ufo_consumers_F4_ufo_growth_stop_building.md.

**Fix (target both).** In game/state/rules/city/ufogrowth.cpp, change craftFactoryIntact to test function.id == "BUILDINGFUNCTION_CONTROL_CHAMBER" in CITYMAP_ALIEN. Keep the fail-open, and rename the function for clarity. The caller at gamestate.cpp:1593 (HEAD) / 1626 (LIVE) needs only the rename.

Replace the ufogrowth.h:21-23 comment. It currently cites UFOpaedia Organic Factory flavour text. Cite UFO2P FUN_0006edac file 0xD1455 and FUN_000b4538 case 7 (file 0x116C21) instead.

Update the tests that hard-code the Organic Factory gate, which exist only in HEAD. These are tests/test_city_rules.cpp:1716-1798 (test_organic_factory_gates_ufo_growth) and tests/test_gamestate_ufo_growth_wiring.cpp:86 and :94. Add a control arm that kills the Organic Factory and asserts growth CONTINUES.

Optional exact-parity variant: latch a serialised GameState bool in the RaidAliens playerWon branch (battle.cpp:3571-3600) when building->function is CONTROL_CHAMBER. This avoids counting non-raid tile loss, which the EXE never counts.

Do not gate on RESEARCH_UNLOCK_ALIEN_BUILDING_6, because the VC-2 re-key would renumber it. Do not add a case-5 flag. The mass-launch flag DAT_000f3c78 on the Control Chamber win is a separate item (VC-10).