#!/usr/bin/env python3
"""Adversarial co-adaptation: X-COM and the aliens each learn against the other.

Technique: regret matching, the update rule at the core of counterfactual regret
minimisation (Hart & Mas-Colell 2000). Chosen deliberately over the alternatives:

  * It is the standard algorithm for two-player zero-sum games and provably converges to a
    minimax equilibrium in the average strategy. "Both sides adapt to each other" is exactly the
    problem it solves, rather than something bolted onto a single-agent learner.
  * It needs no neural network, no gradients, no training corpus and no language model -- it is
    arithmetic over a regret table, so it runs inside the harness, is deterministic given a seed,
    and every step is inspectable.
  * It is testable against known ground truth. Rock-paper-scissors has equilibrium (1/3, 1/3,
    1/3); a correct implementation must find it. That is a real correctness check, not a vibe.

WHY NOT a bandit: UCB or Thompson sampling assume a STATIONARY opponent. Here the opponent is
also learning, so the reward distribution moves under you and a bandit chases its own tail. Regret
matching is built for exactly that non-stationarity.

WHAT IT LEARNS. Not "how to shoot" -- the doctrines are fixed and hand-written, each a plain
TacticalAI. What co-adapts is WHICH doctrine to field, as a probability distribution, given that
the opponent is simultaneously learning which of theirs to field. If X-COM starts winning by
holding range, the alien side's regret for its aggressive rush accumulates and its mix shifts
toward something that punishes range. That is mutual adaptation, and it is measurable in the
mixes.

NO CHEATING, structurally: this module imports no engine and no sockets. It sees an outcome
number per battle and nothing else -- not positions, not the opponent's current mix, not the
opponent's chosen doctrine beyond what the battle result implies.
"""

from __future__ import annotations

import json
import random
from dataclasses import dataclass, field
from pathlib import Path


class RegretMatcher:
    """One side's learner: cumulative regrets -> a mixed strategy over its doctrines.

    Regret matching in its standard form. For each action a:

        strategy[a] = max(regret[a], 0) / sum(max(regret, 0))     (uniform if all <= 0)

    After a round in which we played `played` and received `utility`, and would have received
    `counterfactual[a]` had we played a instead, each action's regret accumulates by
    (counterfactual[a] - utility). Actions we would have done better with gain regret and become
    more likely; the rest decay out of the mix.

    The AVERAGE strategy over all rounds is what converges to equilibrium, not the current one --
    the current strategy oscillates and reading it as "the answer" is the classic misuse.
    """

    def __init__(self, actions: list[str], seed: int = 0, explore: float = 0.05):
        self.actions = list(actions)
        # Exploration floor. Textbook regret matching assumes EXACT counterfactuals - it is told
        # what every alternative would have paid. Here they are sampled: a pair that is never
        # played has no payoff history, so it contributes zero regret, so it never becomes
        # attractive, so it is never played. That is self-starving, and it is not hypothetical -
        # a matching-pennies matchup locked both sides onto pure strategies within three rounds
        # because one of the four pairs was never sampled once.
        #
        # Mixing a little uniform into the SAMPLING distribution keeps every pair reachable.
        # The average strategy is left untouched, so the reported equilibrium is not skewed by
        # exploration; only the play distribution is.
        self.explore = float(explore)
        self.regret = {a: 0.0 for a in self.actions}
        self.strategy_sum = {a: 0.0 for a in self.actions}
        self.rng = random.Random(seed)
        self.rounds = 0

    def strategy(self) -> dict:
        pos = {a: max(self.regret[a], 0.0) for a in self.actions}
        total = sum(pos.values())
        if total <= 0:
            n = len(self.actions)
            return {a: 1.0 / n for a in self.actions}
        return {a: pos[a] / total for a in self.actions}

    def average_strategy(self) -> dict:
        total = sum(self.strategy_sum.values())
        if total <= 0:
            n = len(self.actions)
            return {a: 1.0 / n for a in self.actions}
        return {a: self.strategy_sum[a] / total for a in self.actions}

    def sample(self) -> str:
        s = self.strategy()
        if self.explore > 0.0:
            n = len(self.actions)
            s = {a: (1.0 - self.explore) * s[a] + self.explore / n for a in self.actions}
        # Accumulate BEFORE sampling: every round contributes to the average, including ones
        # whose outcome we never observe because the run was cut short.
        for a in self.actions:
            self.strategy_sum[a] += s[a]
        self.rounds += 1
        r = self.rng.random()
        acc = 0.0
        for a in self.actions:
            acc += s[a]
            if r <= acc:
                return a
        return self.actions[-1]

    def observe(self, played: str, utility: float, counterfactual: dict) -> None:
        for a in self.actions:
            self.regret[a] += counterfactual.get(a, utility) - utility

    def to_dict(self) -> dict:
        return {"actions": self.actions, "regret": self.regret,
                "strategy_sum": self.strategy_sum, "rounds": self.rounds}

    @classmethod
    def from_dict(cls, d: dict, seed: int = 0) -> "RegretMatcher":
        m = cls(d["actions"], seed=seed)
        m.regret = {a: float(d["regret"].get(a, 0.0)) for a in m.actions}
        m.strategy_sum = {a: float(d["strategy_sum"].get(a, 0.0)) for a in m.actions}
        m.rounds = int(d.get("rounds", 0))
        return m


@dataclass
class Matchup:
    """One battle's result, from X-COM's point of view."""

    xcom_doctrine: str
    alien_doctrine: str
    xcom_utility: float          # +1 decisive win .. -1 decisive loss
    detail: dict = field(default_factory=dict)


class AdversarialTrainer:
    """Both sides learning simultaneously against each other.

    Zero-sum by construction: the aliens' utility is the negation of X-COM's. That is what makes
    regret matching's convergence guarantee apply, and it is a fair model here -- a battle X-COM
    wins is one the aliens lost.

    Counterfactuals are the honest weak point and the docstring says so rather than hiding it. In
    a real battle we observe the payoff for the pair actually played and cannot replay the same
    battle with a different doctrine. So unobserved alternatives are estimated from the running
    average payoff that doctrine has earned against this same opponent doctrine, and are left at
    the observed utility (zero regret) until there is any history at all. This is the standard
    sampled-CFR compromise; it slows convergence and does not bias it, and it means early rounds
    carry little information -- which is why the average strategy, not the current one, is what
    should be read.
    """

    def __init__(self, xcom_doctrines: list[str], alien_doctrines: list[str],
                 state_path: Path | None = None, seed: int = 0):
        self.xcom = RegretMatcher(xcom_doctrines, seed=seed)
        self.alien = RegretMatcher(alien_doctrines, seed=seed + 1)
        self.state_path = Path(state_path) if state_path else None
        self.history: list[Matchup] = []
        # payoff[(xcom_doctrine, alien_doctrine)] -> [sum, count]
        self.payoff: dict = {}
        if self.state_path and self.state_path.exists():
            self.load()

    def next_matchup(self) -> tuple[str, str]:
        return self.xcom.sample(), self.alien.sample()

    def _cf(self, side: str, played: str, opponent_doctrine: str, observed: float) -> dict:
        """Estimated payoff for each of our doctrines against this opponent doctrine.

        The action actually PLAYED is pinned to the observed utility, not to its historical
        average. Regret is defined as (counterfactual - actual), so the played action's regret
        must be exactly zero; feeding it a historical mean instead injects spurious self-regret
        proportional to how far this battle fell from that doctrine's average. In a deterministic
        two-doctrine matchup that was enough to lock both sides onto pure strategies -- the
        matching-pennies test caught it, where the true equilibrium is 50/50 on both sides.
        """
        matcher = self.xcom if side == "xcom" else self.alien
        out = {}
        for a in matcher.actions:
            if a == played:
                out[a] = observed
                continue
            key = (a, opponent_doctrine) if side == "xcom" else (opponent_doctrine, a)
            s, n = self.payoff.get(key, (0.0, 0))
            if n == 0:
                out[a] = observed          # no history: no regret either way
            else:
                avg = s / n
                out[a] = avg if side == "xcom" else -avg
        return out

    def record(self, m: Matchup) -> None:
        self.history.append(m)
        key = (m.xcom_doctrine, m.alien_doctrine)
        s, n = self.payoff.get(key, (0.0, 0))
        self.payoff[key] = (s + m.xcom_utility, n + 1)

        self.xcom.observe(m.xcom_doctrine, m.xcom_utility,
                          self._cf("xcom", m.xcom_doctrine, m.alien_doctrine, m.xcom_utility))
        self.alien.observe(m.alien_doctrine, -m.xcom_utility,
                           self._cf("alien", m.alien_doctrine, m.xcom_doctrine,
                                    -m.xcom_utility))
        if self.state_path:
            self.save()

    def report(self) -> str:
        def fmt(d):
            return "  ".join(f"{k}={v:.2f}" for k, v in sorted(d.items(), key=lambda kv: -kv[1]))
        return (f"after {len(self.history)} battles\n"
                f"  X-COM mix : {fmt(self.xcom.average_strategy())}\n"
                f"  alien mix : {fmt(self.alien.average_strategy())}")

    def save(self) -> None:
        self.state_path.parent.mkdir(parents=True, exist_ok=True)
        self.state_path.write_text(json.dumps({
            "xcom": self.xcom.to_dict(),
            "alien": self.alien.to_dict(),
            "payoff": [{"x": k[0], "a": k[1], "sum": v[0], "n": v[1]}
                       for k, v in self.payoff.items()],
            "battles": len(self.history),
        }, indent=1))

    def load(self) -> None:
        d = json.loads(self.state_path.read_text())
        self.xcom = RegretMatcher.from_dict(d["xcom"])
        self.alien = RegretMatcher.from_dict(d["alien"], seed=1)
        self.payoff = {(r["x"], r["a"]): (r["sum"], r["n"]) for r in d.get("payoff", [])}
