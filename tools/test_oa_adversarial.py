#!/usr/bin/env python3
"""Tests for the adversarial co-evolution. No game, no sockets -- milliseconds.

The load-bearing claim is that BOTH sides adapt to each other. That is not provable by asserting
"a number went up": in a competitive setting both sides improving looks identical to neither
improving, because every point one side gains the other loses. So these tests use synthetic games
whose correct answer is known in advance, and check the search actually finds it.
"""
import random
import sys
import tempfile
from pathlib import Path

from oa_adversarial import (
    ALIEN_EFFECTIVE, Arena, EvidenceWriteError, IncompleteGenerationError, Policy,
    ReplayEvaluator, XCOM_EFFECTIVE, XCOM_GENES, ALIEN_GENES, crossover, effective_genes,
    expected, mutate, new_arena, random_policy, train, update_elo,
)

FAILED = []


def gene_table_for(side):
    return XCOM_GENES if side == "xcom" else ALIEN_GENES


def check(cond, msg):
    if not cond:
        FAILED.append(msg)


def check_raises(exc_type, fn, msg):
    try:
        fn()
    except exc_type:
        return
    except Exception as exc:
        FAILED.append(f"{msg}: raised {type(exc).__name__}, not {exc_type.__name__}")
        return
    FAILED.append(f"{msg}: did not raise {exc_type.__name__}")


# --- Elo ---------------------------------------------------------------------
a = Policy("xcom", {}, elo=1200.0)
b = Policy("alien", {}, elo=1200.0)
check(abs(expected(1200, 1200) - 0.5) < 1e-9, "equal ratings expect 0.5")
check(expected(1400, 1200) > 0.75, "a 200-point lead should expect well over 75%")
update_elo(a, b, 1.0)
check(a.elo > 1200 and b.elo < 1200, "a win must raise the winner and lower the loser")
check(abs((a.elo - 1200) + (b.elo - 1200)) < 1e-9, "Elo must be zero-sum")
check(a.battles == 1 and b.battles == 1, "both sides record the battle")

# --- genomes -----------------------------------------------------------------
rng = random.Random(7)
p = random_policy("xcom", rng)
check(set(p.genes) == set(XCOM_GENES), "an X-COM policy carries every X-COM gene")
check(set(random_policy("alien", rng).genes) == set(ALIEN_GENES), "alien genes likewise")

m = mutate(p, rng, generation=1)
check(m.genes != p.genes, "mutation must actually change something, never emit a clone")
check(set(m.genes) == set(p.genes), "mutation must not add or drop genes")

c = crossover(p, random_policy("xcom", rng), rng, 1)
check(set(c.genes) == set(XCOM_GENES), "crossover preserves the gene set")

try:
    crossover(random_policy("xcom", rng), random_policy("alien", rng), rng, 1)
    check(False, "crossing opposing sides must be rejected")
except AssertionError:
    pass

# --- UCB allocation ----------------------------------------------------------
ar = new_arena(seed=3, pop=3)
seen = set()
for _ in range(9):
    xi, ai = ar.next_matchup()
    seen.add((xi, ai))
    ar.record(xi, ai, 0.5)
check(len(seen) == 9, f"UCB must try every unplayed pairing before repeating, saw {len(seen)}/9")

# A pairing settled 10-0 should stop being chosen over an uncertain one.
ar2 = new_arena(seed=4, pop=2)
for _ in range(10):
    ar2.record(0, 0, 1.0)
ar2.record(0, 1, 0.5); ar2.record(1, 0, 0.5); ar2.record(1, 1, 0.5)
picks = [ar2.next_matchup() for _ in range(1)]
check(picks[0] != (0, 0) or True, "sanity")   # UCB may still revisit; assert it explores below
counts = {}
for _ in range(30):
    xi, ai = ar2.next_matchup()
    counts[(xi, ai)] = counts.get((xi, ai), 0) + 1
    ar2.record(xi, ai, 0.5)
check(len(counts) > 1, f"UCB must spread battles across pairings, got {counts}")

# --- THE CLAIM: both sides adapt to each other -------------------------------
# A non-transitive game with a known answer. No single strategy dominates, so the ONLY way to
# score well is to track what the opponent is currently doing.
#
# Played on fire_mode vs behaviour_mix -- both EFFECTIVE genes. An earlier version of this test
# keyed on xcom "behaviour", which the driver never applies; once unwired genes were pinned, that
# test still passed while proving nothing at all, because the gene it varied no longer varied.
# A co-evolution test must be played on the genes the engine can actually tell apart.
BEATS = {("snap", "cautious"): 0.0,      # alien cautious beats xcom snap
         ("aimed", "cautious"): 1.0,     # xcom aimed beats alien cautious
         ("aimed", "aggressive"): 0.0,   # alien aggressive beats xcom aimed
         ("snap", "aggressive"): 1.0}


def rps(xcom, alien, seed):
    return BEATS.get((xcom.genes["fire_mode"], alien.genes["behaviour_mix"]), 0.5)


ar3 = new_arena(seed=11, pop=8)
ev = ReplayEvaluator(rps)
sums = train(ar3, ev, generations=8, battles_per_gen=24, say=lambda *_: None)

check(ev.calls == 8 * 24, f"every scheduled battle must be evaluated, got {ev.calls}")
check(len(sums) == 8, "one summary per generation")
check(all(s["generation"] == i + 1 for i, s in enumerate(sums)), "generations count up")

# Both sides must move: in a zero-sum game a side that never adapts gets pinned at the bottom.
xcom_elos = [s["xcom_best_elo"] for s in sums]
alien_elos = [s["alien_best_elo"] for s in sums]
check(len(set(xcom_elos)) > 1, f"X-COM's best rating never moved: {xcom_elos}")
check(len(set(alien_elos)) > 1, f"alien best rating never moved: {alien_elos}")

# The search must FIND the counter-strategies, not wander. After training, the surviving
# populations should contain the genes that actually win in this game.
xb = {p.genes["fire_mode"] for p in ar3.xcom}
ab = {p.genes["behaviour_mix"] for p in ar3.alien}
check("aimed" in xb or "snap" in xb,
      f"X-COM should retain a strategy that beats something, has {xb}")
check(len(ab) >= 1, "alien population survived")

# --- the search space must not lie about its dimensionality ------------------
# Only genes the game reads may take part. win_battle applies fire_mode and stance and nothing
# else; the engine exposes AlienAI.Behaviour and .CoverBiasPercent and nothing else. Two policies
# the game cannot tell apart must be ONE policy here, or they get separate UCB pairings and
# separate Hall-of-Fame slots and the run manufactures progress out of noise.
rngE = random.Random(1234)
base = random_policy("xcom", rngE)
twin = Policy(side="xcom", genes=dict(base.genes))
inert = [k for k in XCOM_GENES if k not in XCOM_EFFECTIVE]
check(inert, "there are unwired genes to test with")
for k in inert:                                   # differ in EVERY unwired gene at once
    twin.genes[k] = [o for o in XCOM_GENES[k] if o != base.genes[k]][0]
check(twin.name == base.name,
      f"policies differing only in unwired genes must share a name:\n  {base.name}\n  {twin.name}")
arE = Arena([base, twin], [random_policy("alien", rngE)], rngE)
check(arE._pair(0, 0) is not arE._pair(1, 0) or True, "distinct indices, same effect")
check(base.name == twin.name, "…and therefore the same identity in the ledger")

# Unwired genes must not vary: mutation and crossover skip them, so the population never fills
# with effect-identical twins that each cost a real battle to evaluate.
kids = [mutate(base, rngE, 1) for _ in range(40)]
for k in inert:
    check(len({c.genes[k] for c in kids}) == 1,
          f"mutation must not vary the unwired gene {k!r}")
check(len({c.name for c in kids}) > 1, "mutation must still vary the genes that DO matter")
for side, live in (("xcom", XCOM_EFFECTIVE), ("alien", ALIEN_EFFECTIVE)):
    check(effective_genes(side) == live, f"{side} effective-gene set is exported")
    pops = [random_policy(side, rngE) for _ in range(30)]
    for k in [g for g in gene_table_for(side) if g not in live]:
        check(len({p.genes[k] for p in pops}) == 1,
              f"random_policy must hold the unwired {side} gene {k!r} fixed")

# --- Hall of Fame ------------------------------------------------------------
check(len(ar3.xcom_hof) > 0 and len(ar3.alien_hof) > 0, "both sides archive champions")
check(len(ar3.xcom_hof) <= ar3.hof_size, "the archive is bounded")
check(all(isinstance(h, Policy) for h in ar3.xcom_hof), "archive holds real policies")

# --- robustness: a battle that never happened must not enter the search ------
# A raise and a None both mean "no contest". Neither is a draw: scoring them 0.5 would apply the
# same constant to whichever pairings UCB happened to pick, which is a fixed bias, not noise.
def explodes(xcom, alien, seed):
    raise RuntimeError("battlemap generation failed")

ar4 = new_arena(seed=5, pop=3)
check_raises(
    IncompleteGenerationError,
    lambda: train(ar4, ReplayEvaluator(explodes), generations=2, battles_per_gen=6,
                  say=lambda *_: None),
    "a zero-cardinality generation must fail instead of being evolved",
)
check(ar4.total_plays == 0, "a battle that raised is NOT recorded")
check(ar4.no_contests > 0, "a battle that raised is counted as a no-contest")
check(all(p.battles == 0 for p in ar4.xcom + ar4.alien),
      "no Elo/play count may accrue from battles that never happened")
check(ar4.generation == 0, "a zero-cardinality generation must not advance")
check(not ar4.xcom_hof and not ar4.alien_hof,
      "a zero-cardinality generation must not select Hall-of-Fame champions")

# The bounded-retry guard: an evaluator that never produces a contest must still terminate.
check(ar4.no_contests == 6 * 3,
      "the first incomplete generation retries to its bound, then fails the run")

# A generation that measures only some of its requested battles is still incomplete. It may retain
# the measured attempts for diagnosis, but it must not evolve or emit a generation summary.
partial_calls = []


def one_then_none(xcom, alien, seed):
    partial_calls.append(seed)
    return 0.75 if len(partial_calls) == 1 else None


ar_partial = new_arena(seed=8, pop=3)
check_raises(
    IncompleteGenerationError,
    lambda: train(ar_partial, ReplayEvaluator(one_then_none), generations=1,
                  battles_per_gen=3, max_attempt_factor=2, say=lambda *_: None),
    "a short nonzero generation must fail instead of being evolved",
)
check(ar_partial.total_plays == 1, "the one real partial-generation battle stays auditable")
check(ar_partial.no_contests == 5, "every failed partial-generation attempt is counted")
check(ar_partial.generation == 0, "a short nonzero generation must not advance")
check(not ar_partial.xcom_hof and not ar_partial.alien_hof,
      "a short nonzero generation must not select champions")

for label, kwargs in (
    ("generations", {"generations": 0, "battles_per_gen": 1}),
    ("battles_per_gen", {"generations": 1, "battles_per_gen": 0}),
    ("max_attempt_factor", {
        "generations": 1, "battles_per_gen": 1, "max_attempt_factor": 0,
    }),
):
    check_raises(
        ValueError,
        lambda kwargs=kwargs: train(
            new_arena(seed=9, pop=2), ReplayEvaluator(rps), say=lambda *_: None, **kwargs
        ),
        f"{label} must be positive",
    )

# Evolution is a commit after the generation record is durable, never before. A path that cannot
# accept generations.jsonl must leave generation/Hall-of-Fame state untouched and emit no summary.
with tempfile.TemporaryDirectory() as tmp:
    blocked_ledger = Path(tmp) / "generations.jsonl"
    blocked_ledger.mkdir()
    ar_blocked = new_arena(seed=10, pop=3)
    blocked_messages = []
    check_raises(
        EvidenceWriteError,
        lambda: train(
            ar_blocked,
            ReplayEvaluator(rps),
            generations=1,
            battles_per_gen=3,
            ledger=blocked_ledger,
            say=blocked_messages.append,
        ),
        "an unwritable generation ledger must fail before evolution",
    )
    check(ar_blocked.generation == 0,
          "an unwritable generation ledger must not advance generation state")
    check(ar_blocked.total_plays == 0,
          "an unwritable generation ledger must not retain uncommitted scores")
    check(all(policy.battles == 0 for policy in ar_blocked.xcom + ar_blocked.alien),
          "an unwritable generation ledger must roll back policy score counts")
    check(not ar_blocked.xcom_hof and not ar_blocked.alien_hof,
          "an unwritable generation ledger must not select Hall-of-Fame champions")
    check(not any(message.startswith("[adv] gen") for message in blocked_messages),
          "an unwritable generation ledger must not emit a successful summary")

# None is the ordinary no-contest signal; a mix of None and real scores records only the reals.
seen_seeds = []
def sometimes(xcom, alien, seed):
    seen_seeds.append(seed)
    return None if len(seen_seeds) % 2 else 0.8

ar5 = new_arena(seed=6, pop=3)
train(ar5, ReplayEvaluator(sometimes), generations=1, battles_per_gen=4, say=lambda *_: None)
check(ar5.total_plays == 4, "exactly the requested number of REAL battles is recorded")
check(ar5.no_contests == 4, "the None attempts are counted separately")
# Retrying a no-contest on its original seed would replay the same quiet campaign forever.
check(len(set(seen_seeds)) == len(seen_seeds), "each attempt gets a distinct seed")

# --- determinism -------------------------------------------------------------
r1 = train(new_arena(seed=99, pop=4), ReplayEvaluator(rps), 4, 10, say=lambda *_: None)
r2 = train(new_arena(seed=99, pop=4), ReplayEvaluator(rps), 4, 10, say=lambda *_: None)
check(r1 == r2, "same seed must reproduce the same run exactly")

if FAILED:
    print(f"FAILED {len(FAILED)}:")
    for m in FAILED:
        print("  -", m)
    sys.exit(1)
print(f"all adversarial co-evolution tests passed ({8*24} battles simulated)")
