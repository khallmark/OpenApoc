#!/usr/bin/env python3
"""Socket-free contracts for the cold skirmish validation path."""

from __future__ import annotations

import json
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


check_equal(
    oa_skirmish.parse_alien_specs([]),
    {"popper": 4, "skeletoid": 4},
    "an omitted force specification must resolve to the documented nonempty default",
)
check_equal(
    oa_skirmish.parse_alien_specs(["anthropod=3", "skeletoid"]),
    {"anthropod": 3, "skeletoid": 1},
    "known alien names and implicit unit counts must parse exactly",
)
for invalid_spec in ("anthropod=0", "anthropod=-1", "unknown=4", "popper=", "popper=many"):
    check_raises(
        ValueError,
        lambda invalid_spec=invalid_spec: oa_skirmish.parse_alien_specs([invalid_spec]),
        f"invalid alien force {invalid_spec!r} must fail before engine launch",
    )
for invalid_force in ({}, {"unknown": 4}, {"popper": 0}, {"popper": -1}, {"popper": 1.5}):
    check_raises(
        ValueError,
        lambda invalid_force=invalid_force: oa_skirmish.fight_skirmish(
            ColdDriver("Skirmish"), invalid_force
        ),
        f"direct skirmish force {invalid_force!r} must fail closed before UI mutation",
    )
invalid_run_driver = ColdDriver("Skirmish")
check_raises(
    ValueError,
    lambda: oa_skirmish.run_one(invalid_run_driver, {"unknown": 4}),
    "the full direct skirmish cycle must validate forces before opening the UI",
)
check_equal(invalid_run_driver.clicked, [],
            "invalid direct forces must not click into skirmish setup")


class SkirmishMainGame:
    process_exit = None

    def __init__(self, *_args, **_kwargs):
        self._run_binary = Path("unused-openapoc")
        self.argv = []

    def start(self, **_kwargs):
        pass

    def stop(self):
        return self.process_exit


class SkirmishMainHarness:
    def __init__(self, *_args, **_kwargs):
        pass


class SkirmishMainDriver:
    def __init__(self, *_args, **_kwargs):
        self.checks = {}

    def say(self, _message: str):
        pass

    def status(self):
        return oa_play.Status("CityView", 1280, 720, "")


class SkirmishMainReceipt:
    last_finish = None

    def __init__(self, *_args, **_kwargs):
        self.finished = False

    def record_binary_snapshot(self, *_args, **_kwargs):
        pass

    def event(self, *_args, **_kwargs):
        pass

    def finish(self, outcome, exit_code, **detail):
        self.finished = True
        self.__class__.last_finish = {
            "outcome": outcome, "exit_code": exit_code, **detail,
        }


def skirmish_main_for(process_exit: oa_play.ProcessExitResult,
                      validation: bool) -> tuple[int, dict | None]:
    patch_names = (
        "GameProcess", "Harness", "Driver", "RunReceipt", "create_run_directory",
        "run_provenance", "new_game", "run_one",
    )
    originals = {name: getattr(oa_skirmish, name) for name in patch_names}
    original_argv = sys.argv
    try:
        SkirmishMainGame.process_exit = process_exit
        SkirmishMainReceipt.last_finish = None
        oa_skirmish.GameProcess = SkirmishMainGame
        oa_skirmish.Harness = SkirmishMainHarness
        oa_skirmish.Driver = SkirmishMainDriver
        oa_skirmish.RunReceipt = SkirmishMainReceipt
        oa_skirmish.create_run_directory = lambda root, *_args, **_kwargs: Path(root)
        oa_skirmish.run_provenance = lambda *_args, **_kwargs: {}
        oa_skirmish.new_game = lambda *_args, **_kwargs: None
        oa_skirmish.run_one = lambda *_args, **_kwargs: {
            "result_kind": "gameplay",
            "outcome": "resolved",
            "decided": True,
            "won": True,
            "stage": "BattleView",
            "reason": "",
            "battle": {},
            "aliens": {"anthropod": 1},
            "map_row": 0,
            "before": {},
        }
        with tempfile.TemporaryDirectory() as tmp:
            sys.argv = [
                "oa_skirmish.py", "--repo", tmp, "--out", tmp,
                "--rounds", "1", "--alien", "anthropod=1", "--seed", "17",
            ] + (["--validation"] if validation else [])
            result_code = oa_skirmish.main()
            return result_code, SkirmishMainReceipt.last_finish
    finally:
        sys.argv = original_argv
        for name, original in originals.items():
            setattr(oa_skirmish, name, original)


skirmish_unclean_exits = (
    oa_play.ProcessExitResult(True, 0, False, False, 0, "exited rc=0 before stop"),
    oa_play.ProcessExitResult(True, 7, False, False, 7, "exited rc=7"),
    oa_play.ProcessExitResult(True, None, True, True, -9, "killed by signal 9"),
)
for validation in (False, True):
    mode = "validation" if validation else "ordinary finite"
    result_code, receipt_finish = skirmish_main_for(
        oa_play.ProcessExitResult(True, None, True, False, 0, "exited rc=0"), validation
    )
    check_equal(result_code, 0, f"{mode} Skirmish must accept a clean engine shutdown")
    if validation:
        check_equal(receipt_finish.get("engine_exit", {}).get("clean_shutdown"), True,
                    "validation receipt must record the clean Skirmish shutdown")
    for process_exit in skirmish_unclean_exits:
        result_code, receipt_finish = skirmish_main_for(process_exit, validation)
        check_equal(result_code, 1,
                    f"{mode} Skirmish must reject {process_exit.status!r}")
        if validation:
            check_equal(receipt_finish.get("outcome"), "process_error",
                        "validation receipt must classify a dirty Skirmish exit")
            check_equal(receipt_finish.get("engine_exit", {}).get("clean_shutdown"), False,
                        "validation receipt must preserve the dirty Skirmish exit")


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
        return oa_play.ProcessExitResult(
            False, None, False, False, None, "engine never started"
        )


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
check("cold start failed" in terminal.get("reason", ""),
      "worker shutdown handling must preserve the startup failure evidence")


class WorkerGame:
    process_exit = None

    def __init__(self, *_args, **_kwargs):
        pass

    def start(self, **_kwargs):
        pass

    def stop(self):
        return self.process_exit


class WorkerHarness:
    def __init__(self, *_args, **_kwargs):
        pass


class WorkerDriver:
    def __init__(self, *_args, **_kwargs):
        self.checks = {}


def worker_terminal_for(process_exit: oa_play.ProcessExitResult) -> dict:
    originals = (oa_arena.GameProcess, oa_arena.Harness, oa_arena.Driver, oa_arena.new_game)
    try:
        WorkerGame.process_exit = process_exit
        oa_arena.GameProcess = WorkerGame
        oa_arena.Harness = WorkerHarness
        oa_arena.Driver = WorkerDriver
        oa_arena.new_game = lambda *_args, **_kwargs: None
        with tempfile.TemporaryDirectory() as tmp:
            messages: queue.Queue = queue.Queue()
            stopped = threading.Event()
            stopped.set()
            oa_arena.run_worker(
                0,
                17901,
                Path(tmp),
                Path(tmp),
                {"anthropod": 1},
                SimpleNamespace(seed=1),
                queue.Queue(),
                messages,
                threading.Lock(),
                Path(tmp) / "battles.jsonl",
                stopped,
            )
            return messages.get_nowait()
    finally:
        oa_arena.GameProcess, oa_arena.Harness, oa_arena.Driver, oa_arena.new_game = originals


clean_terminal = worker_terminal_for(
    oa_play.ProcessExitResult(True, None, True, False, 0, "exited rc=0")
)
check_equal(clean_terminal.get("status"), "clean",
            "a clean worker shutdown must remain successful")
check_equal(clean_terminal.get("engine_exit", {}).get("clean_shutdown"), True,
            "worker terminal evidence must carry the typed clean exit")

unclean_worker_exits = (
    oa_play.ProcessExitResult(True, 0, False, False, 0, "exited rc=0 before stop"),
    oa_play.ProcessExitResult(True, 7, False, False, 7, "exited rc=7"),
    oa_play.ProcessExitResult(True, None, True, True, -9, "killed by signal 9"),
)
for process_exit in unclean_worker_exits:
    terminal = worker_terminal_for(process_exit)
    check_equal(terminal.get("status"), "error",
                f"unclean worker exit {process_exit.status!r} must veto arena success")
    check_equal(terminal.get("engine_exit", {}).get("clean_shutdown"), False,
                "worker terminal evidence must preserve the unclean typed exit")


class ArenaMainGame:
    process_exit = None

    def __init__(self, *_args, **_kwargs):
        pass

    def start(self, **_kwargs):
        pass

    def stop(self):
        return self.process_exit


class ArenaMainDriver:
    def __init__(self, *_args, **_kwargs):
        self.checks = {}

    def say(self, _message: str):
        pass

    def status(self):
        return oa_play.Status("CityView", 1280, 720, "")


def arena_main_for(process_exit: oa_play.ProcessExitResult, probe: bool) -> tuple[int, dict]:
    patch_names = (
        "GameProcess", "Harness", "Driver", "new_game", "fight", "recover", "all_policies",
    )
    originals = {name: getattr(oa_arena, name) for name in patch_names}
    original_argv = sys.argv
    try:
        ArenaMainGame.process_exit = process_exit
        oa_arena.GameProcess = ArenaMainGame
        oa_arena.Harness = WorkerHarness
        oa_arena.Driver = ArenaMainDriver
        oa_arena.new_game = lambda *_args, **_kwargs: None
        oa_arena.fight = lambda *_args, **_kwargs: {
            "policy": "snap/run",
            "fire_mode": "snap",
            "stance": "run",
            "aliens": {"anthropod": 1},
            "map_row": 0,
            "result_kind": "gameplay",
            "outcome": "resolved",
            "decided": True,
            "stage": "BattleView",
            "reason": "",
            "seconds": 12.0,
            "squad_start": 10,
            "squad_end": 8,
            "survivor_frac": 0.8,
            "mission_type": "extermination",
        }
        oa_arena.recover = lambda *_args, **_kwargs: True
        oa_arena.all_policies = lambda: [
            {"fire_mode": "snap", "stance": "run", "name": "snap/run"}
        ]
        with tempfile.TemporaryDirectory() as tmp:
            sys.argv = [
                "oa_arena.py", "--repo", tmp, "--out", tmp,
                "--generations", "1", "--battles-per-policy", "1",
                "--alien", "anthropod=1",
            ] + (["--probe"] if probe else [])
            result_code = oa_arena.main()
            records = [
                json.loads(line)
                for line in (Path(tmp) / "battles.jsonl").read_text().splitlines()
            ]
            return result_code, records[-1]
    finally:
        sys.argv = original_argv
        for name, original in originals.items():
            setattr(oa_arena, name, original)


for probe in (True, False):
    path_name = "probe" if probe else "sequential arena"
    result_code, terminal_record = arena_main_for(
        oa_play.ProcessExitResult(True, None, True, False, 0, "exited rc=0"), probe
    )
    check_equal(result_code, 0, f"{path_name} must accept a clean process exit")
    check_equal(terminal_record.get("engine_exit", {}).get("clean_shutdown"), True,
                f"{path_name} ledger must carry clean process-exit evidence")
    for process_exit in unclean_worker_exits:
        result_code, terminal_record = arena_main_for(process_exit, probe)
        check_equal(result_code, 1,
                    f"{path_name} must reject {process_exit.status!r}")
        check_equal(terminal_record.get("engine_exit", {}).get("clean_shutdown"), False,
                    f"{path_name} ledger must carry unclean process-exit evidence")


class EvaluatorGame:
    process_exit = None

    def __init__(self, *_args, **_kwargs):
        pass

    def start(self, **_kwargs):
        pass

    def stop(self):
        return self.process_exit


class EvaluatorHarness:
    def __init__(self, *_args, **_kwargs):
        pass

    def gs(self, query: str) -> dict:
        if query == "time":
            return {"ticks": "0"}
        raise AssertionError(query)


class EvaluatorDriver:
    def __init__(self, harness, *_args, **_kwargs):
        self.h = harness
        self.checks = {}

    def status(self):
        return oa_play.Status("BattleView", 1280, 720, "")


def adversarial_score_for(process_exit: oa_play.ProcessExitResult,
                          block_ledger: bool = False) -> tuple[float | None, dict]:
    patch_names = (
        "GameProcess", "Harness", "Driver", "new_game", "sell_ground_fleet",
        "buy_interceptor", "crew_transport", "assign_research", "win_battle",
        "_flying_crewed",
    )
    originals = {name: getattr(oa_adversarial_arena, name) for name in patch_names}
    try:
        EvaluatorGame.process_exit = process_exit
        oa_adversarial_arena.GameProcess = EvaluatorGame
        oa_adversarial_arena.Harness = EvaluatorHarness
        oa_adversarial_arena.Driver = EvaluatorDriver
        oa_adversarial_arena.new_game = lambda *_args, **_kwargs: None
        oa_adversarial_arena.sell_ground_fleet = lambda *_args, **_kwargs: 0
        oa_adversarial_arena.buy_interceptor = lambda *_args, **_kwargs: 0
        oa_adversarial_arena.crew_transport = lambda *_args, **_kwargs: 0
        oa_adversarial_arena.assign_research = lambda *_args, **_kwargs: 0
        oa_adversarial_arena._flying_crewed = lambda *_args, **_kwargs: 1
        oa_adversarial_arena.win_battle = lambda *_args, **_kwargs: oa_play.BattleResult(
            oa_play.BattleOutcome.RESOLVED, started_with=10, survivors=8, seconds=12.0
        )
        with tempfile.TemporaryDirectory() as tmp:
            if block_ledger:
                (Path(tmp) / "battles.jsonl").mkdir()
            arena = oa_adversarial_arena.new_arena(seed=3, pop=2)
            evaluator = oa_adversarial_arena.CampaignEvaluator(
                Path(tmp), Path(tmp), 17960, budget_s=1.0, leg_days=1.0, verbose=False
            )
            score = evaluator.evaluate(arena.xcom[0], arena.alien[0], seed=17)
            records = [json.loads(line) for line in (Path(tmp) / "battles.jsonl").read_text().splitlines()]
            return score, records[-1]
    finally:
        for name, original in originals.items():
            setattr(oa_adversarial_arena, name, original)


clean_score, clean_record = adversarial_score_for(
    oa_play.ProcessExitResult(True, None, True, False, 0, "exited rc=0")
)
check(clean_score is not None, "a decided battle with a clean exit must remain scoreable")
check_equal(clean_record.get("engine_exit", {}).get("clean_shutdown"), True,
            "adversarial evidence must preserve a clean typed process exit")
check_raises(
    oa_adversarial_arena.EvidenceWriteError,
    lambda: adversarial_score_for(
        oa_play.ProcessExitResult(True, None, True, False, 0, "exited rc=0"),
        block_ledger=True,
    ),
    "an unwritable battle ledger must prevent an adversarial score from escaping",
)

unclean_adversarial_exits = (
    oa_play.ProcessExitResult(True, 0, False, False, 0, "exited rc=0 before stop"),
    oa_play.ProcessExitResult(True, 7, False, False, 7, "exited rc=7"),
    oa_play.ProcessExitResult(True, None, True, True, -9, "killed by signal 9"),
)
for process_exit in unclean_adversarial_exits:
    invalid_score, invalid_record = adversarial_score_for(process_exit)
    check_equal(invalid_score, None,
                f"unclean post-battle exit {process_exit.status!r} must veto scoring")
    check_equal(invalid_record.get("score"), None,
                "the adversarial ledger must not retain a score invalidated by shutdown")
    check_equal(invalid_record.get("engine_exit", {}).get("clean_shutdown"), False,
                "the adversarial ledger must preserve unclean process-exit evidence")


def adversarial_main_with_blocked_ledger(ledger_name: str) -> tuple[int, object]:
    patch_names = (
        "GameProcess", "Harness", "Driver", "new_game", "sell_ground_fleet",
        "buy_interceptor", "crew_transport", "assign_research", "win_battle",
        "_flying_crewed", "new_arena",
    )
    originals = {name: getattr(oa_adversarial_arena, name) for name in patch_names}
    original_argv = sys.argv
    captured = {}
    try:
        EvaluatorGame.process_exit = oa_play.ProcessExitResult(
            True, None, True, False, 0, "exited rc=0"
        )
        oa_adversarial_arena.GameProcess = EvaluatorGame
        oa_adversarial_arena.Harness = EvaluatorHarness
        oa_adversarial_arena.Driver = EvaluatorDriver
        oa_adversarial_arena.new_game = lambda *_args, **_kwargs: None
        oa_adversarial_arena.sell_ground_fleet = lambda *_args, **_kwargs: 0
        oa_adversarial_arena.buy_interceptor = lambda *_args, **_kwargs: 0
        oa_adversarial_arena.crew_transport = lambda *_args, **_kwargs: 0
        oa_adversarial_arena.assign_research = lambda *_args, **_kwargs: 0
        oa_adversarial_arena._flying_crewed = lambda *_args, **_kwargs: 1
        oa_adversarial_arena.win_battle = lambda *_args, **_kwargs: oa_play.BattleResult(
            oa_play.BattleOutcome.RESOLVED, started_with=10, survivors=8, seconds=12.0
        )

        def capture_arena(*args, **kwargs):
            arena = originals["new_arena"](*args, **kwargs)
            captured["arena"] = arena
            return arena

        oa_adversarial_arena.new_arena = capture_arena
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / ledger_name).mkdir()
            sys.argv = [
                "oa_adversarial_arena.py", "--repo", tmp, "--out", tmp,
                "--generations", "1", "--battles-per-gen", "1", "--pop", "2",
                "--budget", "1", "--leg", "1", "--seed", "17", "--quiet",
            ]
            return oa_adversarial_arena.main(), captured["arena"]
    finally:
        sys.argv = original_argv
        for name, original in originals.items():
            setattr(oa_adversarial_arena, name, original)


blocked_battle_rc, blocked_battle_arena = adversarial_main_with_blocked_ledger(
    "battles.jsonl"
)
check_equal(blocked_battle_rc, 1,
            "an unwritable battle ledger must prevent adversarial main success")
check_equal(blocked_battle_arena.total_plays, 0,
            "an unpersisted battle score must not enter the adversarial arena")
check_equal(blocked_battle_arena.generation, 0,
            "an unwritable battle ledger must not advance a generation")

blocked_generation_rc, blocked_generation_arena = adversarial_main_with_blocked_ledger(
    "generations.jsonl"
)
check_equal(blocked_generation_rc, 1,
            "an unwritable generation ledger must prevent adversarial main success")
check_equal(blocked_generation_arena.generation, 0,
            "an unwritable generation ledger must fail before evolution")
check_equal(blocked_generation_arena.total_plays, 0,
            "an unwritable generation ledger must roll back uncommitted scores")
check(not blocked_generation_arena.xcom_hof and not blocked_generation_arena.alien_hof,
      "an unwritable generation ledger must not select champions")


if FAILED:
    print(f"FAILED {len(FAILED)}:")
    for failure in FAILED:
        print("  -", failure)
    sys.exit(1)

print("all cold skirmish result tests passed")
