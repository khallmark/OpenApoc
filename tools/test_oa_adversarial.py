#!/usr/bin/env python3
"""Correctness tests for the adversarial learner. No game, no engine, milliseconds.

The central one is rock-paper-scissors: it has a known unique equilibrium at (1/3, 1/3, 1/3), so
a correct regret-matching implementation MUST find it. That is ground truth rather than a
plausibility check, and it is the reason this algorithm was chosen over something unfalsifiable.
"""
import sys, tempfile
from pathlib import Path
from oa_adversarial import RegretMatcher, AdversarialTrainer, Matchup

FAILED = []
def check(c, m):
    if not c:
        FAILED.append(m)

# --- regret matching finds the RPS equilibrium ------------------------------
RPS = ["rock", "paper", "scissors"]
BEATS = {"rock": "scissors", "paper": "rock", "scissors": "paper"}

def payoff(a, b):
    if a == b:
        return 0.0
    return 1.0 if BEATS[a] == b else -1.0

p1 = RegretMatcher(RPS, seed=1)
p2 = RegretMatcher(RPS, seed=2)
for _ in range(20000):
    a, b = p1.sample(), p2.sample()
    u = payoff(a, b)
    p1.observe(a, u, {x: payoff(x, b) for x in RPS})
    p2.observe(b, -u, {x: payoff(x, a) for x in RPS})

avg = p1.average_strategy()
for act, prob in avg.items():
    check(abs(prob - 1 / 3) < 0.05,
          f"RPS equilibrium: {act} should approach 1/3, got {prob:.3f}")
check(abs(sum(avg.values()) - 1.0) < 1e-6, "strategy must be a distribution")

# --- a dominated action is driven out --------------------------------------
# "always lose" can never be right; its probability must collapse.
D = ["good", "bad"]
m = RegretMatcher(D, seed=3)
for _ in range(2000):
    a = m.sample()
    u = 1.0 if a == "good" else -1.0
    m.observe(a, u, {"good": 1.0, "bad": -1.0})
check(m.average_strategy()["good"] > 0.9,
      f"dominant action should take over, got {m.average_strategy()}")

# --- uniform before any evidence -------------------------------------------
fresh = RegretMatcher(["a", "b", "c", "d"], seed=4)
s = fresh.strategy()
check(all(abs(v - 0.25) < 1e-9 for v in s.values()),
      f"with no regret the mix must be uniform, got {s}")

# --- the two sides genuinely co-adapt --------------------------------------
# Alien "rush" beats X-COM "close"; X-COM "hold" beats "rush". If only X-COM adapted, it would
# settle on hold and stay. Because the ALIENS also adapt, they must move off rush.
XD, AD = ["close", "hold"], ["rush", "creep"]
TABLE = {("close", "rush"): -1.0, ("close", "creep"): +1.0,
         ("hold", "rush"): +1.0, ("hold", "creep"): -1.0}
t = AdversarialTrainer(XD, AD, seed=7)
for _ in range(4000):
    x, a = t.next_matchup()
    t.record(Matchup(x, a, TABLE[(x, a)]))
xm, am = t.xcom.average_strategy(), t.alien.average_strategy()
# This is a matching-pennies payoff: the unique equilibrium is 50/50 on BOTH sides.
for k, v in xm.items():
    check(abs(v - 0.5) < 0.15, f"X-COM mix should approach 50/50, {k}={v:.2f}")
for k, v in am.items():
    check(abs(v - 0.5) < 0.15, f"alien mix should approach 50/50, {k}={v:.2f}")
check(am["rush"] < 0.85,
      f"aliens must move off a punished doctrine -- that is the co-adaptation, got {am}")

# --- persistence ------------------------------------------------------------
with tempfile.TemporaryDirectory() as td:
    p = Path(td) / "state.json"
    t1 = AdversarialTrainer(XD, AD, state_path=p, seed=5)
    for _ in range(200):
        x, a = t1.next_matchup()
        t1.record(Matchup(x, a, TABLE[(x, a)]))
    before = t1.xcom.average_strategy()
    t2 = AdversarialTrainer(XD, AD, state_path=p, seed=5)
    after = t2.xcom.average_strategy()
    check(all(abs(before[k] - after[k]) < 1e-9 for k in before),
          f"learned state must survive a reload: {before} vs {after}")
    check(t2.payoff, "payoff history must reload too")

# --- determinism ------------------------------------------------------------
def run(seed):
    tt = AdversarialTrainer(XD, AD, seed=seed)
    for _ in range(300):
        x, a = tt.next_matchup()
        tt.record(Matchup(x, a, TABLE[(x, a)]))
    return tt.xcom.average_strategy()
check(run(11) == run(11), "same seed must reproduce exactly")
check(run(11) != run(12), "different seeds must explore differently")

if FAILED:
    print(f"FAILED {len(FAILED)}:")
    for f in FAILED:
        print("  -", f)
    sys.exit(1)
print("adversarial learner: all tests passed")
