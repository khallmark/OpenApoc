#!/usr/bin/env python3
"""The campaign genome: the strategy levers a cross-run learner may turn, and nothing else.

oa_victory.py and oa_play.py are full of constants (how many fliers to keep, how many soldiers to
hire, when to hold fire over the city). Each one is a decision somebody made once, from one or two
observed runs, and never revisited. A learner cannot tune a constant it cannot reach, so this
module is the single place that names the ones worth tuning, gives each its legal values, and says
which runner actually reads it.

Three rules keep it honest:

  * DEFAULTS ARE TODAY'S BEHAVIOUR. Strategy() with no arguments carries exactly the constants the
    runners had before this file existed, and its battle policy is EMPTY -- so a run that names no
    strategy takes the same code path it always did. tools/test_oa_learn.py pins every default
    against the runner's own constant, so a drift fails a test rather than a campaign.
  * NO DEAD KNOBS. Every gene below is read by a traced call site; the "read by" field names it.
    A gene nothing reads would be a dimension of the search space that cannot change the outcome,
    and the learner would spend real campaigns (hours each) telling identical genomes apart --
    the failure oa_adversarial.py documents at length for its own unwired genes.
  * INERT GENES ARE CANONICALISED AWAY. The doctrine genes only mean anything when the veteran
    tactical AI is switched on. Strategy.key() resets the ones that are not active to their
    default, so two genomes the game cannot tell apart share one identity and one pool of evidence.

Nothing here imports the game, the harness or oa_play: it is plain data plus validation, which is
what lets the learner and its tests run in milliseconds with no game.
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Optional

from oa_adversarial import XCOM_EFFECTIVE, XCOM_GENES


@dataclass(frozen=True)
class Gene:
    default: Any
    values: tuple                      # the values the learner searches over
    read_by: str                       # call site(s); the "no dead knobs" audit trail
    runners: tuple = ("victory",)      # which main loops honour it: "victory", "play_campaign"
    # Genes that only matter when another gene has a particular value. A gene whose condition is
    # false is INERT: the game cannot see it, so it must not split identities or collect evidence.
    requires: Optional[tuple] = None   # (other_gene, value)
    numeric: bool = False              # numeric genes accept any value inside [min, max] of values

    def accepts(self, value: Any) -> bool:
        if self.numeric:
            return (isinstance(value, (int, float)) and not isinstance(value, bool)
                    and min(self.values) <= value <= max(self.values))
        return value in self.values


# Order matters: a gene's `requires` target must come before it, so Strategy.active() can resolve
# in one pass and the learner can sample prerequisites first.
GENES: dict[str, Gene] = {
    # -- campaign levers (read by oa_victory.Victory.city_turn / run) ------------------------------
    "air_patrol": Gene(
        8, (4, 6, 8, 12),
        "Victory.city_turn: armed fliers below this -> buy_interceptor", numeric=True),
    "min_soldiers": Gene(
        18, (12, 18, 24, 30),
        "Victory.city_turn: fit soldiers below this -> hire_soldiers", numeric=True),
    "min_squad": Gene(
        6, (3, 4, 6, 8),
        "Victory.run AlertScreen: fewer fit soldiers than this -> decline the incident",
        numeric=True),
    "intercept_min_relation": Gene(
        25, (0, 15, 25, 40),
        "Victory.city_turn: government relation below this -> hold fire over the city",
        numeric=True),
    "min_lab_skill": Gene(
        1100, (700, 1100, 1600, 2200),
        "Victory.city_turn: total lab skill below this -> hire scientists and engineers",
        numeric=True),
    "infil_min_armed": Gene(
        3, (2, 3, 5, 8),
        "Victory.city_turn: armed agents needed to raid an infiltrated building and to slow the "
        "clock for it", numeric=True),
    "infil_speed": Gene(
        3, (3, 4, 5),
        "Victory.city_turn: city clock speed while buildings await a sweep", numeric=True),
    # -- shared with oa_play.play_campaign through the Driver --------------------------------------
    "garrison": Gene(
        4, (2, 4, 6, 8),
        "oa_play.crew_transport: soldiers kept home when a craft is crewed",
        runners=("victory", "play_campaign"), numeric=True),
    "research_order": Gene(
        "balanced", ("balanced", "victory_first", "weapons_first"),
        "oa_play.pick_topic_rows via research_priority", runners=("victory", "play_campaign")),
    # -- endgame levers; the defaults preserve the current manufacture and crossing path ----------
    "gate_craft_order": Gene(
        "best_unlocked", ("best_unlocked", "transport_first"),
        "oa_play.gate_craft_project: best unlocked hull or earliest troop transport",
        runners=("victory", "play_campaign")),
    "cross_min_crew": Gene(
        1, (1, 4, 6),
        "Victory.dimension_turn and oa_play.select_gate_craft: outbound squad threshold; "
        "return trips always accept survivors", runners=("victory", "play_campaign"), numeric=True),
    # -- battle doctrine: reaches win_battle through the battle policy -----------------------------
    "tactical_ai": Gene(
        None, (None, "veteran"),
        "oa_play.build_battle_ai: None keeps the engine's own unit AI (today's behaviour)",
        runners=("victory", "play_campaign")),
    "fire_mode": Gene(
        None, (None,) + tuple(XCOM_GENES["fire_mode"]),
        "oa_play._fight_battle: set_fire_mode on entry (None leaves the game's default)",
        runners=("victory", "play_campaign")),
    "behaviour": Gene(
        "normal", tuple(XCOM_GENES["behaviour"]),
        "VeteranAI(behaviour=) via oa_executor.make_ai", runners=("victory", "play_campaign"),
        requires=("tactical_ai", "veteran")),
    "withdraw_ratio": Gene(
        6.0, tuple(XCOM_GENES["withdraw_ratio"]),
        "VeteranAI(withdraw_ratio=) via oa_executor.make_ai", runners=("victory", "play_campaign"),
        requires=("tactical_ai", "veteran")),
}

# The doctrine genes name VeteranAI constructor arguments; keep them aligned with the ones
# oa_adversarial has already established the executor can act on, so a gene is never added here
# that the AI layer would silently drop.
assert all(g in XCOM_EFFECTIVE for g in ("fire_mode", "behaviour", "withdraw_ratio"))

DEFAULTS: dict[str, Any] = {name: g.default for name, g in GENES.items()}


class Strategy:
    """A validated assignment of every gene. Immutable by convention; build a new one to change."""

    def __init__(self, genes: Optional[dict] = None):
        genes = dict(genes or {})
        unknown = sorted(set(genes) - set(GENES))
        if unknown:
            # A typo'd gene silently running at its default is a one-armed experiment that reports
            # success; refuse instead.
            raise ValueError(f"unknown strategy gene(s) {unknown}; known: {sorted(GENES)}")
        for name, value in genes.items():
            gene = GENES[name]
            if not gene.accepts(value):
                raise ValueError(f"strategy gene {name}={value!r} is not allowed; "
                                 f"values: {list(gene.values)}")
            if isinstance(gene.default, (int, float)) and not isinstance(gene.default, bool):
                # 6 and 6.0 are the same setting but different keys; settle on the default's type.
                if isinstance(gene.default, int) and float(value) != int(value):
                    raise ValueError(f"strategy gene {name}={value!r} must be a whole number")
                genes[name] = type(gene.default)(value)
        self.genes = {**DEFAULTS, **genes}

    def __getitem__(self, name: str) -> Any:
        return self.genes[name]

    def __eq__(self, other: object) -> bool:
        return isinstance(other, Strategy) and self.key() == other.key()

    def __hash__(self) -> int:
        return hash(self.key())

    def active(self, name: str) -> bool:
        req = GENES[name].requires
        return req is None or self.genes[req[0]] == req[1]

    def canonical(self) -> dict:
        """The genes as the game can tell them apart: inert ones reset to their default."""
        return {n: (self.genes[n] if self.active(n) else GENES[n].default) for n in GENES}

    def key(self) -> str:
        """Stable identity of this genome's EFFECT, e.g. 'air_patrol=12,tactical_ai=veteran'."""
        c = self.canonical()
        bits = [f"{n}={c[n]}" for n in GENES if c[n] != GENES[n].default]
        return ",".join(bits) or "default"

    def non_default(self) -> dict:
        c = self.canonical()
        return {n: c[n] for n in GENES if c[n] != GENES[n].default}

    def is_default(self) -> bool:
        return not self.non_default()

    def battle_policy(self) -> dict:
        """The tactical policy this strategy implies -- EMPTY when it asks for nothing.

        Empty matters: _fight_battle applies fire_mode/stance only `if policy:` and builds an AI
        only when policy["ai"] is set, so a default strategy must hand the battle code the same
        empty dict it always got. When the veteran AI is chosen the doctrine genes ride along,
        because they are constructor arguments and have nowhere else to go.
        """
        c = self.canonical()
        policy: dict = {}
        if c["tactical_ai"]:
            policy["ai"] = c["tactical_ai"]
            policy["behaviour"] = c["behaviour"]
            policy["withdraw_ratio"] = c["withdraw_ratio"]
        if c["fire_mode"]:
            policy["fire_mode"] = c["fire_mode"]
        if policy:
            policy["name"] = f"strategy[{self.key()}]"
        return policy

    def to_json(self) -> str:
        return json.dumps(self.canonical(), sort_keys=True)


def parse_strategy(arg: Optional[str]) -> Strategy:
    """Strategy from None (defaults), inline JSON ('{"air_patrol": 12}') or a JSON file path."""
    if not arg:
        return Strategy()
    text = arg.strip()
    if not text.startswith("{"):
        p = Path(text)
        if not p.is_file():
            raise ValueError(f"--strategy {arg!r} is neither inline JSON nor an existing file")
        text = p.read_text()
    try:
        data = json.loads(text)
    except ValueError as exc:
        raise ValueError(f"--strategy is not valid JSON: {exc}") from exc
    if not isinstance(data, dict):
        raise ValueError("--strategy must be a JSON object of gene: value")
    return Strategy(data)


def add_strategy_option(ap) -> None:
    ap.add_argument("--strategy", metavar="JSON|PATH", default=None,
                    help="campaign genome: inline JSON or a path to a JSON file of gene values "
                         f"(genes: {', '.join(GENES)}). Omitted genes keep today's behaviour.")


# -- research ordering ---------------------------------------------------------------------------
# research_order reorders oa_play.PRIORITY_RESEARCH rather than replacing it, so every topic the
# default list names is still named and still reachable -- only what comes first changes.

def _is_victory_chain(topic: str) -> bool:
    return topic.startswith(("RESEARCH_ALIEN_BUILDING_", "RESEARCH_DIMENSION_",
                             "RESEARCH_THE_ALIEN_DIMENSION", "RESEARCH_ALIEN_PROPULSION",
                             "RESEARCH_ALIEN_CONTROL", "RESEARCH_ALIEN_ENERGY",
                             "RESEARCH_ADVANCED_"))


def _is_weapon(topic: str) -> bool:
    return any(k in topic for k in ("DISRUPTOR", "LAUNCHER", "CANNON", "BOOMEROID", "MISSILE",
                                    "BOMB", "SHIELD", "ARMOR", "VORTEX", "ENTROPY"))


_ORDER_PREDICATES: dict[str, Optional[Callable[[str], bool]]] = {
    "balanced": None,
    "victory_first": _is_victory_chain,
    "weapons_first": _is_weapon,
}
assert set(_ORDER_PREDICATES) == set(GENES["research_order"].values)


def reorder_research(priority: list, order: str) -> list:
    """`priority` with the topics `order` favours moved to the front, relative order kept."""
    pred = _ORDER_PREDICATES[order]
    if pred is None:
        return list(priority)
    return [t for t in priority if pred(t)] + [t for t in priority if not pred(t)]
