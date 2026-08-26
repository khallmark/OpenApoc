#!/usr/bin/env python3
"""Socket-free contracts for the cold skirmish validation path."""

from __future__ import annotations

import queue
import sys
import tempfile
import threading
from pathlib import Path
from types import SimpleNamespace

import oa_arena
import oa_adversarial_arena
import oa_play
import oa_skirmish


FAILED: list[str] = []


def check(condition: bool, message: str) -> None:
    if not condition:
        FAILED.append(message)


def check_equal(actual, expected, message: str) -> None:
    if actual != expected:
        FAILED.append(f"{message}: expected {expected!r}, got {actual!r}")


def check_raises(exc_type, fn, message: str) -> None:
    try:
        fn()
    except exc_type:
        return
    except Exception as exc:  # pragma: no cover - failure reporting path
        FAILED.append(f"{message}: raised {type(exc).__name__}, not {exc_type.__name__}")
        return
    FAILED.append(f"{message}: did not raise {exc_type.__name__}")


class ColdDriver:
    """MainMenu driver with a selectable current or live-FIFO landing stage."""

    def __init__(self, landing_stage: str):
        self.stage = "MainMenu"
        self.landing_stage = landing_stage
        self.clicked: list[str] = []

    def status(self) -> oa_play.Status:
        return oa_play.Status(self.stage, 1280, 720, "")

    def click_id(self, control: str, _status=None) -> bool:
        self.clicked.append(control)
        if control == "BUTTON_SKIRMISH":
            self.stage = self.landing_stage
            return True
        return False

    def wait_for(self, expected, _seconds: float) -> oa_play.Status:
        choices = (expected,) if isinstance(expected, str) else expected
        if self.stage not in choices:
            raise TimeoutError(f"{self.stage} not in {choices}")
        return self.status()

    def say(self, _message: str) -> None:
        pass


class SnapshotHarness:
    def __init__(self, error: str | None = None):
        self.error = error

    def gs(self, _query: str):
        if self.error:
            raise oa_play.HarnessError(self.error)
        return {"battle": "ready"}


for landing_stage in ("Skirmish", "MapSelector"):
    cold = ColdDriver(landing_stage)
    check(oa_skirmish.open_skirmish(cold),
          f"cold MainMenu must accept the {landing_stage} landing stage")
    check_equal(cold.clicked, ["BUTTON_SKIRMISH"],
                "cold entry must not manufacture an in-progress campaign")
    check_equal(cold.status().stage, landing_stage,
                "cold entry must preserve the engine's observed landing stage")

snapshot_driver = ColdDriver("Skirmish")
snapshot_driver.h = SnapshotHarness()
check_equal(oa_skirmish.battle_snapshot(snapshot_driver), {"battle": "ready"},
            "available pre-battle state must be preserved")
snapshot_driver.h = SnapshotHarness("'gs battle' -> ERR no gamestate (query handler not installed yet)")
check_equal(oa_skirmish.battle_snapshot(snapshot_driver), None,
            "cold pre-battle snapshot may be unavailable before GameState exists")

snapshot_driver.h = SnapshotHarness("'gs battle' -> ERR malformed state")
try:
    oa_skirmish.battle_snapshot(snapshot_driver)
    check(False, "unexpected pre-battle query failures must not be suppressed")
except oa_play.HarnessError:
    pass


setup = oa_skirmish.SkirmishAttempt.setup_failure(
    stage="SelectForces",
    reason="select_forces_resume_pop_not_applied",
)
record = setup.as_dict()
check_equal(record["result_kind"], "setup_failure",
            "a lost lifecycle transition must be setup failure")
check_equal(record["outcome"], None,
            "setup failure must not occupy the gameplay outcome field")
check_equal(record["stage"], "SelectForces", "setup failure must preserve the stuck stage")


scored = oa_arena.score([
    record,
    {"result_kind": "gameplay", "outcome": "resolved", "survivor_frac": 0.5,
     "seconds": 12.0},
    {"result_kind": "gameplay", "outcome": "timeout", "survivor_frac": 1.0,
     "seconds": 240.0},
    {"result_kind": "gameplay", "outcome": "returned", "survivor_frac": 1.0,
     "seconds": 1.0},
])
check_equal(scored["battles"], 1,
            "arena score must include only decided battles in its denominator")
check_equal(scored["setup_failures"], 1,
            "arena score must report setup failures separately")
check_equal(scored.get("incomplete_battles"), 2,
            "arena score must report entered but undecided battles separately")
check_equal(scored["win_rate"], 1.0,
            "setup failures and non-decisions must not be scored as losses")

short = oa_arena.score([
    {"result_kind": "gameplay", "outcome": "resolved", "survivor_frac": 1.0,
     "seconds": 12.0},
], expected_battles=2)
check_equal(short.get("complete"), False,
            "an arena policy batch must not complete below its requested battle count")
overfull = oa_arena.score([
    {"result_kind": "gameplay", "outcome": "resolved", "survivor_frac": 1.0,
     "seconds": 12.0},
    {"result_kind": "gameplay", "outcome": "lost", "survivor_frac": 0.0,
     "seconds": 12.0},
], expected_battles=1)
check_equal(overfull.get("complete"), False,
            "an arena policy batch must not complete above its requested battle count")

check_raises(
    ValueError,
    lambda: oa_adversarial_arena.utility("timeout", 10, 10),
    "adversarial utility must reject a non-decision rather than breed from it",
)


class ArenaHarness:
    def gs(self, query: str) -> dict:
        if query == "battle":
            return {"in_battle": "0"}
        raise AssertionError(query)


class ArenaDriver:
    def __init__(self):
        self.h = ArenaHarness()
        self.last_battle = {}


arena_driver = ArenaDriver()
original_run_one = oa_arena.run_one
try:
    def fake_run_one(d, *_args, **_kwargs):
        d.last_battle = {
            "outcome": "resolved",
            "started_with": 10,
            "survivors": 7,
            "seconds": 20.0,
            "mission_type": "extermination",
        }
        return {
            "result_kind": "gameplay",
            "outcome": "resolved",
            "decided": True,
            "stage": "BattleView",
            "reason": "",
        }

    oa_arena.run_one = fake_run_one
    fought = oa_arena.fight(
        arena_driver,
        {"anthropod": 1},
        {"name": "snap/run", "fire_mode": "snap", "stance": "run"},
        map_row=0,
        budget_s=30.0,
    )
finally:
    oa_arena.run_one = original_run_one

check_equal(fought["squad_start"], 10,
            "arena scoring must use the battle record captured before debrief teardown")
check_equal(fought["squad_end"], 7,
            "arena scoring must preserve decided-battle survivors")
check_equal(fought["survivor_frac"], 0.7,
            "arena scoring must derive survivor fraction from the captured battle record")


# The parallel coordinator consumes explicit messages and fails immediately on worker death or a
# partial policy result instead of blocking forever or breeding from partial data.
messages: queue.Queue = queue.Queue()
messages.put({
    "kind": "worker_terminal", "worker": 2, "status": "error", "reason": "boot failed"
})
check_raises(
    oa_arena.ArenaCoordinatorError,
    lambda: oa_arena.collect_policy_results(messages, 1, 1, 1.0),
    "worker startup failure must terminate the coordinator",
)

messages = queue.Queue()
messages.put({
    "kind": "policy_result",
    "fitness": -1_000_000_000.0,
    "policy": {"name": "snap/run"},
    "score": {"generation": 1, "complete": False},
    "error": "incomplete policy batch",
})
check_raises(
    oa_arena.ArenaCoordinatorError,
    lambda: oa_arena.collect_policy_results(messages, 1, 1, 1.0),
    "partial policy records must terminate the coordinator",
)

messages = queue.Queue()
messages.put({
    "kind": "policy_result",
    "fitness": 100.0,
    "policy": {"name": "snap/run"},
    "score": {"generation": 1, "complete": True},
    "error": "",
})
collected = oa_arena.collect_policy_results(messages, 1, 1, 1.0)
check_equal(len(collected), 1, "one complete policy result must satisfy exact cardinality")


class EmptyWorkerQueue:
    def get(self, timeout):
        raise queue.Empty


original_monotonic = oa_arena.time.monotonic
clock_values = iter((0.0, 2.0))
try:
    oa_arena.time.monotonic = lambda: next(clock_values)
    check_raises(
        oa_arena.ArenaCoordinatorError,
        lambda: oa_arena.collect_policy_results(EmptyWorkerQueue(), 1, 1, 1.0),
        "a silent worker must hit the coordinator deadline",
    )
finally:
    oa_arena.time.monotonic = original_monotonic


class FailingWorkerGame:
    def __init__(self, *_args, **_kwargs):
        pass

    def start(self, **_kwargs):
        raise RuntimeError("cold start failed")

    def stop(self):
        pass


original_game_process = oa_arena.GameProcess
try:
    oa_arena.GameProcess = FailingWorkerGame
    with tempfile.TemporaryDirectory() as tmp:
        worker_messages: queue.Queue = queue.Queue()
        oa_arena.run_worker(
            0,
            17901,
            Path(tmp),
            Path(tmp),
            {"anthropod": 1},
            SimpleNamespace(seed=1),
            queue.Queue(),
            worker_messages,
            threading.Lock(),
            Path(tmp) / "battles.jsonl",
            threading.Event(),
        )
        terminal = worker_messages.get_nowait()
finally:
    oa_arena.GameProcess = original_game_process

check_equal(terminal.get("kind"), "worker_terminal",
            "every worker exit must emit a terminal record")
check_equal(terminal.get("status"), "error",
            "cold worker startup failure must be explicit")


if FAILED:
    print(f"FAILED {len(FAILED)}:")
    for failure in FAILED:
        print("  -", failure)
    sys.exit(1)

print("all cold skirmish result tests passed")
