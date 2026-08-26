#!/usr/bin/env python3
"""Co-evolutionary adversarial learning: X-COM and the aliens adapt to each other.

No LLM, no gradients, no neural net. The evaluation signal here is a *battle* - expensive
(seconds to minutes), noisy (one map, one seed, one roll of the dice), and only comparative
(policy A beat policy B on this occasion). That rules out gradient methods and rules IN the
family this file implements:

  * COMPETITIVE CO-EVOLUTION. Two populations, X-COM and alien, each scored only by how it does
    against the *current* other side. Neither has an absolute fitness function, because there
    isn't one - "good tactics" is entirely relative to what you are fighting.

  * HALL OF FAME. Each side keeps an archive of past champions and is evaluated partly against
    them. Without this, co-evolution cycles: side A learns a counter to B's current strategy, B
    counters that, and A drifts back to something it already lost with. Archives make progress
    monotone-ish rather than a merry-go-round. (Rosin & Belew's competitive fitness sharing; the
    same reason self-play systems keep frozen past opponents.)

  * UCB1 BANDIT ALLOCATION. Battles are the scarce resource, so match-ups are chosen by upper
    confidence bound rather than round-robin: a pairing whose outcome is uncertain is worth more
    than re-confirming one already decided 9-1. This is what makes a few hundred battles produce
    a usable ranking instead of a thin uniform smear.

  * ELO on top, so the two sides' strengths are comparable across generations even though every
    match is relative.

Everything in this file is pure. It never touches the game: it proposes match-ups, consumes
results, and returns updated populations. That is what lets the whole scheme be tested in
milliseconds (tools/test_oa_adversarial.py) and re-run deterministically from a seed.
"""

from __future__ import annotations

import json
import math
import random
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Optional

# ---------------------------------------------------------------------------
# Genomes: the tunable surface of each side
# ---------------------------------------------------------------------------

# X-COM knobs map onto the AI layer's own doctrine parameters (tools/oa_ai.py).
XCOM_GENES = {
    "fire_mode": ["snap", "aimed", "auto"],
    "stance": ["run", "walk", "kneel"],
    "behaviour": ["aggressive", "normal", "evasive"],
    "move_mode": ["group", "individual"],
    "reserve": ["none", "snap", "aimed", "auto"],
    "pull_back_at": [0.25, 0.4, 0.5, 0.65, 0.8],
    "min_spacing": [1, 2, 3, 4],
    "withdraw_ratio": [2.0, 3.0, 4.5, 6.0, 100.0],
    "focus_fire": [True, False],
    "priority_targets": [True, False],
}

# Alien knobs map onto the ENGINE's own behaviour surface - the modes ai.txt already describes
# (Aggressive / Normal / Cautious, morale thresholds, cover preference). These are exposed as
# config options so the alien side can be varied without patching C++ per experiment, and
# critically WITHOUT giving either side information it should not have.
ALIEN_GENES = {
    "behaviour_mix": ["aggressive", "normal", "cautious", "mixed"],
    "cover_bias": [0.0, 0.25, 0.5, 0.75, 1.0],
    "morale_floor": [10, 25, 40, 55],
    "grenade_bias": [0.0, 0.25, 0.5, 0.75],
    "advance_bias": [0.0, 0.25, 0.5, 0.75, 1.0],
}


@dataclass
class Policy:
    side: str                       # "xcom" | "alien"
    genes: dict
    elo: float = 1200.0
    battles: int = 0
    wins: float = 0.0
    born: int = 0                   # generation

    @property
    def name(self) -> str:
        bits = ",".join(f"{k}={self.genes[k]}" for k in sorted(self.genes))
        return f"{self.side}[{bits}]"

    @property
    def win_rate(self) -> float:
        return self.wins / self.battles if self.battles else 0.0


def random_policy(side: str, rng: random.Random, generation: int = 0) -> Policy:
    table = XCOM_GENES if side == "xcom" else ALIEN_GENES
    return Policy(side=side, genes={k: rng.choice(v) for k, v in table.items()}, born=generation)


def mutate(p: Policy, rng: random.Random, generation: int, rate: float = 0.34) -> Policy:
    """Change a few genes. Deliberately coarse: with battles this expensive, a fine-grained
    search wastes evaluations exploring differences too small for a noisy signal to resolve."""
    table = XCOM_GENES if p.side == "xcom" else ALIEN_GENES
    genes = dict(p.genes)
    changed = False
    for k, options in table.items():
        if rng.random() < rate:
            alt = [o for o in options if o != genes[k]]
            if alt:
                genes[k] = rng.choice(alt)
                changed = True
    if not changed:                                  # never emit a pure clone
        k = rng.choice(list(table))
        alt = [o for o in table[k] if o != genes[k]]
        if alt:
            genes[k] = rng.choice(alt)
    return Policy(side=p.side, genes=genes, elo=p.elo, born=generation)


def crossover(a: Policy, b: Policy, rng: random.Random, generation: int) -> Policy:
    """Uniform crossover. Two policies that each beat different opponents may combine into one
    that beats both -- the whole reason to keep a population rather than hill-climb a single
    incumbent."""
    assert a.side == b.side, "cannot cross policies from opposing sides"
    genes = {k: (a.genes[k] if rng.random() < 0.5 else b.genes[k]) for k in a.genes}
    return Policy(side=a.side, genes=genes, elo=(a.elo + b.elo) / 2.0, born=generation)


# ---------------------------------------------------------------------------
# Elo
# ---------------------------------------------------------------------------

def expected(ra: float, rb: float) -> float:
    return 1.0 / (1.0 + 10.0 ** ((rb - ra) / 400.0))


def update_elo(a: Policy, b: Policy, score_a: float, k: float = 24.0) -> None:
    """score_a in [0,1]: 1 win, 0 loss, 0.5 draw. Applied symmetrically."""
    ea = expected(a.elo, b.elo)
    a.elo += k * (score_a - ea)
    b.elo += k * ((1.0 - score_a) - (1.0 - ea))
    a.battles += 1
    b.battles += 1
    a.wins += score_a
    b.wins += 1.0 - score_a


# ---------------------------------------------------------------------------
# Match-up selection: UCB1 over pairings
# ---------------------------------------------------------------------------

@dataclass
class Pairing:
    xi: int
    ai: int
    plays: int = 0
    xcom_score: float = 0.0     # summed score for the X-COM side

    @property
    def mean(self) -> float:
        return self.xcom_score / self.plays if self.plays else 0.5


class Arena:
    """Chooses which match-ups to spend battles on, and folds results back in.

    Pure: it never runs a battle. `next_matchup()` proposes one, `record()` consumes the outcome.
    The caller is whatever can actually fight - the live game, a replay, or a stub in a test.
    """

    def __init__(self, xcom: list, alien: list, rng: random.Random,
                 hof_size: int = 4, explore: float = 1.4):
        self.xcom = xcom
        self.alien = alien
        self.rng = rng
        self.explore = explore
        self.hof_size = hof_size
        self.xcom_hof: list = []
        self.alien_hof: list = []
        self.pairings: dict = {}
        self.total_plays = 0
        self.generation = 0
        self.history: list = []
        # Attempts that produced no battle at all. Counted, never scored -- see train().
        self.no_contests = 0

    def _pair(self, xi: int, ai: int) -> Pairing:
        return self.pairings.setdefault((xi, ai), Pairing(xi, ai))

    def next_matchup(self) -> tuple:
        """UCB1 over (xcom, alien) pairs. Unplayed pairings sort first by construction.

        Sampling opponents from the Hall of Fame as well as the live population is what stops the
        two sides chasing each other in a circle: a policy has to beat what the other side is
        doing NOW *and* what it was doing when it last looked strong.
        """
        alien_pool = list(range(len(self.alien)))
        best, best_key = None, None
        for xi in range(len(self.xcom)):
            for ai in alien_pool:
                p = self._pair(xi, ai)
                if p.plays == 0:
                    key = float("inf")
                else:
                    key = p.mean + self.explore * math.sqrt(
                        math.log(max(1, self.total_plays)) / p.plays)
                if best_key is None or key > best_key:
                    best, best_key = (xi, ai), key
        return best

    def record(self, xi: int, ai: int, xcom_score: float) -> None:
        p = self._pair(xi, ai)
        p.plays += 1
        p.xcom_score += xcom_score
        self.total_plays += 1
        update_elo(self.xcom[xi], self.alien[ai], xcom_score)
        self.history.append({"gen": self.generation, "xi": xi, "ai": ai,
                             "score": xcom_score,
                             "xcom": self.xcom[xi].name, "alien": self.alien[ai].name})

    # -- evolution ---------------------------------------------------------

    def _evolve_side(self, pop: list, hof: list, gen: int) -> list:
        """Keep the top half, refill by crossover+mutation, and archive the champion."""
        ranked = sorted(pop, key=lambda p: (p.elo, p.win_rate), reverse=True)
        champion = ranked[0]
        hof.append(Policy(side=champion.side, genes=dict(champion.genes),
                          elo=champion.elo, born=gen))
        del hof[:-self.hof_size]                      # bounded archive

        keep = ranked[: max(1, len(ranked) // 2)]
        children = []
        while len(keep) + len(children) < len(pop):
            if len(keep) >= 2 and self.rng.random() < 0.5:
                a, b = self.rng.sample(keep, 2)
                children.append(mutate(crossover(a, b, self.rng, gen), self.rng, gen))
            else:
                children.append(mutate(self.rng.choice(keep), self.rng, gen))
        return keep + children

    def evolve(self) -> dict:
        """One generation on BOTH sides. Returns a summary record for the ledger."""
        self.generation += 1
        xb = max(self.xcom, key=lambda p: p.elo)
        ab = max(self.alien, key=lambda p: p.elo)
        summary = {
            "generation": self.generation,
            "battles": self.total_plays,
            "no_contests": self.no_contests,
            "xcom_best": xb.name, "xcom_best_elo": round(xb.elo, 1),
            "xcom_best_wr": round(xb.win_rate, 3),
            "alien_best": ab.name, "alien_best_elo": round(ab.elo, 1),
            "alien_best_wr": round(ab.win_rate, 3),
        }
        self.xcom = self._evolve_side(self.xcom, self.xcom_hof, self.generation)
        self.alien = self._evolve_side(self.alien, self.alien_hof, self.generation)
        self.pairings.clear()          # pairings are indices into a population that just changed
        return summary


def new_arena(seed: int = 0, pop: int = 6) -> Arena:
    rng = random.Random(seed)
    return Arena([random_policy("xcom", rng) for _ in range(pop)],
                 [random_policy("alien", rng) for _ in range(pop)],
                 rng)


# ---------------------------------------------------------------------------
# The training loop
# ---------------------------------------------------------------------------

class Evaluator:
    """Runs one battle and returns the X-COM score in [0,1]. 1 = X-COM won outright.

    Deliberately an interface. The learner must not know or care whether a match was fought by
    the real engine, replayed from a ledger, or produced by a stub -- that separation is what
    lets the whole scheme be tested without a game, and what lets a broken battle path be swapped
    out without touching the learning code.
    """

    def evaluate(self, xcom: Policy, alien: Policy, seed: int):
        """Return the X-COM score in [0,1], or None if NO BATTLE WAS FOUGHT.

        None is not a loss and not a draw -- it is the absence of a measurement, and the caller
        drops it whole. Return it whenever the outcome carries no information about the two
        policies: the budget expired before a mission generated, the process died, the map failed
        to build. Returning a number there would attribute an engine or harness failure to the
        genomes that happened to be on the field.
        """
        raise NotImplementedError


def train(arena: Arena, evaluator: Evaluator, generations: int, battles_per_gen: int,
          ledger: Optional[Path] = None, say=print, base_seed: int = 0,
          max_attempt_factor: int = 3) -> list:
    """Co-evolve both sides. Returns one summary per generation.

    Each generation: spend `battles_per_gen` battles on UCB-chosen match-ups, then evolve BOTH
    populations against what the other side has just become. Neither side has a fixed opponent,
    which is the whole point -- X-COM adapts to the aliens' current strategy and the aliens adapt
    right back.

    A NON-EVENT IS NOT A DRAW. If the evaluator returns None (or raises), no battle was fought,
    and the attempt is dropped whole: no Elo update, no play count, so UCB still reads the pairing
    as unexplored and will come back to it. The earlier version scored these 0.5 "so one broken map
    cannot bias the search" -- but recording a draw IS the bias. Every no-contest scored the same
    constant regardless of which policies were paired, which is a fixed offset applied to whichever
    match-ups happened to draw a quiet campaign. Exclusion is the only neutral handling.

    Seeds advance per ATTEMPT, not per recorded battle. Retrying a no-contest on its original seed
    would replay the same quiet campaign and fail identically forever.
    """
    summaries = []
    attempt = 0
    for _ in range(generations):
        fought = 0
        budget = max(1, battles_per_gen * max_attempt_factor)
        spent = 0
        while fought < battles_per_gen and spent < budget:
            spent += 1
            attempt += 1
            xi, ai = arena.next_matchup()
            seed = base_seed + attempt
            try:
                score = evaluator.evaluate(arena.xcom[xi], arena.alien[ai], seed)
            except Exception as exc:
                say(f"  [adv] attempt {attempt} raised {type(exc).__name__}: {exc} "
                    f"-- no contest, not recorded")
                score = None
            if score is None:
                arena.no_contests += 1
                continue
            arena.record(xi, ai, min(1.0, max(0.0, float(score))))
            fought += 1
        if fought < battles_per_gen:
            say(f"  [adv] generation short: {fought}/{battles_per_gen} battles fought in "
                f"{spent} attempts. Evolving on what was measured; the rest never happened.")
        summary = arena.evolve()
        summary["fought_this_gen"] = fought
        summaries.append(summary)
        say(f"[adv] gen {summary['generation']:>3} "
            f"battles={summary['battles']:<5} "
            f"nc={summary['no_contests']:<4} "
            f"xcom={summary['xcom_best_elo']:>7.1f} alien={summary['alien_best_elo']:>7.1f}")
        if ledger:
            try:
                with Path(ledger).open("a") as fh:
                    fh.write(json.dumps(summary) + "\n")
            except Exception as exc:
                say(f"  [adv] ledger write failed: {type(exc).__name__}: {exc}")
    return summaries


class ReplayEvaluator(Evaluator):
    """Scores from a table of known outcomes. Used by the tests, and by anyone wanting to re-run
    a search over an existing ledger without paying for the battles again."""

    def __init__(self, fn):
        self.fn = fn
        self.calls = 0

    def evaluate(self, xcom: Policy, alien: Policy, seed: int) -> float:
        self.calls += 1
        return self.fn(xcom, alien, seed)
