# Handoff — adaptive AI for OpenApoc

Written 2026-08-27, after several days' work, at the point where the person doing it had
stopped seeing the wood for the trees. Read the "How this went wrong" section before starting.

---

## The goal, verbatim

> First implement original decompiled logic perfectly. Then implement complete adversarial
> learning between both x-com and aliens. They both adapt to each others strategies. First
> assure game parity and then implement this adversarial AI using modern techniques (no LLM)

Standing constraints, all from the user:

- **DO NOT CHEAT.** The AI may use only information a human player can see on screen. This is
  the load-bearing constraint — every measurement is worthless without it.
- Never commit `depot_7661/`, original EXEs, or Ghidra `.rep`/`.gpr` databases.
- Decompiler listings only under `docs/original-game/`, citing binary + generation + **file
  offset** (never a bare Ghidra VA — they drift between imports).
- No extracted list tables under `data/common_patch/gamestate/` — `loadGame` appends vectors and
  doubles spawn counts.
- **A row is not done until the lock test fails without the change.** No invented constants.

---

## Where things actually stand

### Working and verified

**Parity (goal part 1) — done.** 38/38 C++ tests pass. The recovered constants are not merely
implemented but *reachable from production code*, which was checked explicitly:

| symbol | reached from |
|---|---|
| `exposureScore` | `unitaihelper.cpp:229,232` |
| `getTakeCoverMovement` | `unitaivanilla.cpp` ×3, `unitaibehavior.cpp:69` |
| `withdrawBandEntered` | `vehicle.cpp:2195` |
| `UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE` | `gamestate.cpp:1017, 1085, 1138` |

`unitaivanilla.cpp` is the *alien* AI, so the recovered TACP cover metric runs for the aliens in
every battle.

**The harness.** `tools/oa_*.py`. A localhost socket in the engine plus a Python driver that
plays a full campaign with no human. Split three ways on purpose:

    oa_capabilities.py   what is mechanically possible
    oa_ai.py             what to do about it (pure, no engine, no sockets)
    oa_executor.py       the bridge + plugin loading

`oa_ai.py` cannot touch the game. That is what makes doctrine testable in milliseconds and
swappable wholesale (`tools/ai_plugins/`).

**The learner.** `tools/oa_adversarial.py` — competitive co-evolution, UCB1 allocation, Elo,
Hall of Fame. Pure; no game. `oa_adversarial_arena.py` connects it to real battles.

**Last measured run (seed 801, stopped early at 11 battles):** 11 battles, 11 wins, 0 losses,
0 no-contests. Best of the session by a wide margin.

### Not established

**Adaptation.** This is the open half of the goal. Across four full runs:

- X-COM's side shows a directional trend and converges genes (`stance=run`, `fire_mode=aimed`
  held for five generations in one run).
- The alien side wanders. `cover_bias` went 0.00 → 0.75 → 1.00 on one seed and looked like an
  arms race; on an independent seed it went 0.00 → 0.50 → 0.00 → 0.00. **It did not replicate.**

**The likely reason, and it is not the learner.** The alien genome reaches the engine through
`OpenApoc.AlienAI.Behaviour`, `.CoverBiasPercent` and `.GrenadeBiasPercent`. X-COM has eight live
genes plus a doctrine layer. The sides are not matched in expressive power, and `behaviour_mix`
pinned at `mixed` in every generation of one run. **Widening the alien config surface is the
highest-value next step for the stated goal.**

---

## How this went wrong, which matters more than the code

Read this before touching anything.

**1. The dominant bug class is "written, never called."** Six instances found:
`raid_infiltrated_building`, `crew_transport`, `make_ai`, `build_second_base`/`build_facility`,
`enterBattle`, and the AI plugin layer itself. Each produced *no error* — the capability simply
never happened, and the symptom was a missing event, which no stack trace shows.

Audit: parse `tools/oa_*.py` for top-level defs, grep each name across `tools/`, flag any with no
reference outside its own `def` line.

**2. The second dominant class is ORDERING.** Four instances, all in one evening: crewing after
the raid that needed it; base upkeep in the wrong branch; the move order issued before selecting
who moves; withdrawal checked after the hunt that swallows it. In every case the logic was
correct and its *position* was wrong, and in every case the rule "ran" every round achieving
nothing.

**Tests must assert ORDER, not presence.** `"select_units" in acts` was true the entire time
the civilians were dying.

**3. Debug leftovers killed the engine twice.** `if (true)` on a Ctrl-click tile dump
(segfault); an unguarded three-deep deref in a fall diagnostic; a contentless `LogWarning` in a
hot path that crowded out every useful log tail. A diagnostic that kills the process is worse
than none.

**4. The diagnostic chain is the most valuable thing built.** Typed threadpool exceptions →
`std::set_terminate` handler → signal handler for SIGSEGV/SIGBUS/SIGILL/SIGFPE →
`GameProcess.exit_status()` naming the signal → per-attempt JSONL with log tails. None fixed
anything alone. Together they separated one symptom — "the game died" — into four distinct
causes, three of them real bugs and one (SIGKILL) not a bug at all but two runs on one machine
colliding on a shared port.

**Do not delete these. They are why anything was findable.**

**5. Where the previous agent lost the thread.** Restarting the measurement run on every fix —
eight or nine times — so no run ever finished. Narrating battle-by-battle instead of at
generation boundaries. Reporting most on whatever had most recently gone wrong, which made a 9%
mission type look like the whole campaign. Twice asserting a fix worked when only its API had
changed, because the "prove it fails without the change" check produced an ImportError rather
than a behavioural failure.

**Rules that would have helped:** finish the run; report at generation boundaries only; prove a
fix behaviourally, never by an import error; and when a hypothesis about your own code is
plausible, check it before writing it into a commit message.

---

## Mechanics

    cd tools
    python3 oa_campaign.py --days 30                 # play a campaign
    python3 oa_adversarial_arena.py --generations 10 --battles-per-gen 9 --pop 3 --seed 801

`--battles-per-gen` must be >= pop² or UCB never covers the pairing grid before the population
turns over. With pop 3 that is 9.

Ports auto-select per process (`free_port`); do not pin `--port` if another run may be live.

Each attempt writes `build/adversarial/battles.jsonl` — outcome, score, squad, mission type,
game-days, raids, `game_exit`, log tail. **Read this before theorising.** Generation summaries in
`generations.jsonl`. Prior runs under `build/adversarial-archive/`.

Tests: `ctest` in `build/` (38), and `python3 test_oa_{ai,harness,adversarial}.py` in `tools/`.

Throughput: ~5 min per battle, of which ~50% is campaign setup reaching a battle. A working
Skirmish would roughly halve it — see `tools/oa_skirmish.py`, which documents the root cause
(the framework stage-command drain discards commands queued during processing) **and why the
obvious fix is wrong** — it was tried, built green, and broke a working path.

---

## Open work, in the order I would take it

1. **Widen the alien config surface.** Morale, grenade and advance behaviour as real engine
   options the way `CoverBiasPercent` is. Without this, "both sides adapt" cannot be shown,
   because one side has two knobs and the other has nine.
2. **Finish a ten-generation run without restarting it.** Nothing in the record is a completed,
   uncontaminated measurement on current code.
3. **The `map::at` SIGABRT.** Named with a stack in `Framework::run` (see `36cf3070`); throw site
   still inlined. Recurs roughly 1 in 20 attempts.
4. **Scoring weights.** `utility()` is 70% win / 30% survivors. Never ruled on by the user, and
   it decides what the search optimises. Ask before changing.
5. **Skirmish.** Highest throughput win; hardest fix. Read the docstring first.

## PRs open on the fork

- **#4** (draft) — engine socket → `main`
- **#9** (draft) — Python driver → `harness/socket-infrastructure`

Testing happens on the unified branch (`fix/ios-zero-viewport`, which despite its name carries
everything); the split branches exist only for review.
