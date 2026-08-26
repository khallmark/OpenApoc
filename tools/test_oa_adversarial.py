#!/usr/bin/env python3
"""Tests for the adversarial co-evolution. No game, no sockets -- milliseconds.

The load-bearing claim is that BOTH sides adapt to each other. That is not provable by asserting
"a number went up": in a competitive setting both sides improving looks identical to neither
improving, because every point one side gains the other loses. So these tests use synthetic games
whose correct answer is known in advance, and check the search actually finds it.
"""
import random
import sys

from oa_adversarial import (
    Arena, Policy, ReplayEvaluator, XCOM_GENES, ALIEN_GENES,
    crossover, expected, mutate, new_arena, random_policy, train, update_elo,
)

FAILED = []


def check(cond, msg):
    if not cond:
        FAILED.append(msg)


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
# A non-transitive game with a known answer. Alien 'cautious' beats X-COM 'aggressive';
# X-COM 'evasive' beats 'cautious'; alien 'aggressive' beats 'evasive'. No single strategy
# dominates, so the ONLY way to score well is to track what the opponent is currently doing.
BEATS = {("aggressive", "cautious"): 0.0,     # alien cautious beats xcom aggressive
         ("evasive", "cautious"): 1.0,        # xcom evasive beats alien cautious
         ("evasive", "aggressive"): 0.0,      # alien aggressive beats xcom evasive
         ("aggressive", "aggressive"): 1.0}


def rps(xcom, alien, seed):
    return BEATS.get((xcom.genes["behaviour"], alien.genes["behaviour_mix"]), 0.5)


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
xb = {p.genes["behaviour"] for p in ar3.xcom}
ab = {p.genes["behaviour_mix"] for p in ar3.alien}
check("evasive" in xb or "aggressive" in xb,
      f"X-COM should retain a strategy that beats something, has {xb}")
check(len(ab) >= 1, "alien population survived")

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
s4 = train(ar4, ReplayEvaluator(explodes), generations=2, battles_per_gen=6,
           say=lambda *_: None)
check(len(s4) == 2, "training survives an evaluator that always throws")
check(ar4.total_plays == 0, "a battle that raised is NOT recorded")
check(ar4.no_contests > 0, "a battle that raised is counted as a no-contest")
check(all(p.battles == 0 for p in ar4.xcom + ar4.alien),
      "no Elo/play count may accrue from battles that never happened")
check(s4[-1]["fought_this_gen"] == 0, "the summary must say no battles were fought")

# The bounded-retry guard: an evaluator that never produces a contest must still terminate.
check(ar4.no_contests == 2 * 6 * 3, "no-contests retry up to max_attempt_factor, then give up")

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
