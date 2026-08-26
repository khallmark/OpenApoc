#!/usr/bin/env python3
"""Pure tests for truthful unattended-run outcomes and receipts."""

from __future__ import annotations

import json
import tempfile
from pathlib import Path

import oa_play


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


# The engine uses the same stage for both endings. Validation must classify the video detail,
# never infer a win from VideoScreen alone.
check_equal(
    oa_play.classify_terminal_status(
        oa_play.Status("VideoScreen", 1280, 720, "", "wingame2.smk")
    ),
    "victory",
    "the winning video must be victory",
)
check_equal(
    oa_play.classify_terminal_status(
        oa_play.Status("VideoScreen", 1280, 720, "", "lose1.smk")
    ),
    "defeat",
    "the losing video must be defeat",
)
check_equal(
    oa_play.classify_terminal_status(
        oa_play.Status("VideoScreen", 1280, 720, "", "data/videos/WINGAME2.SMK")
    ),
    "victory",
    "the exact winning basename may arrive as a full path",
)
check_equal(
    oa_play.classify_terminal_status(
        oa_play.Status("VideoScreen", 1280, 720, "", "mystery.smk")
    ),
    "unexpected_terminal",
    "an unrecognised terminal video must fail closed",
)
for misleading in ("wingame3.smk", "wingame-preview.smk", "notlosebutintro.smk"):
    check_equal(
        oa_play.classify_terminal_status(
            oa_play.Status("VideoScreen", 1280, 720, "", misleading)
        ),
        "unexpected_terminal",
        f"the ending allowlist must reject {misleading}",
    )
check_equal(
    oa_play.classify_terminal_status(oa_play.Status("CityView", 1280, 720, "")),
    None,
    "a playable stage must not be terminal",
)


# Only an explicitly reached target credits a leg. Transitions are useful, but they are not time
# progress; timeout variants are failures even when some ticks moved.
for outcome, credits in (
    (oa_play.AdvanceOutcome.REACHED, True),
    (oa_play.AdvanceOutcome.TRANSITION, False),
    (oa_play.AdvanceOutcome.PARTIAL, False),
    (oa_play.AdvanceOutcome.PARKED, False),
    (oa_play.AdvanceOutcome.TIMEOUT, False),
    (oa_play.AdvanceOutcome.TERMINAL, False),
):
    result = oa_play.AdvanceResult(
        outcome=outcome,
        start_ticks=0,
        target_ticks=100,
        end_ticks=100 if credits else 50,
        stage="CityView",
    )
    check_equal(result.reached, credits, f"{outcome.value} credit policy")

compat = oa_play.AdvanceResult(
    outcome=oa_play.AdvanceOutcome.REACHED,
    start_ticks=0,
    target_ticks=100,
    end_ticks=100,
    stage="CityView",
    time_state={"ticks": "100", "day": "2"},
)
check_equal(dict(compat), {"ticks": "100", "day": "2"},
            "AdvanceResult must retain the old read-only time mapping surface")


check_equal(oa_play.require_validation_seed(17, validation=True), 17,
        "a positive explicit validation seed must be accepted")
check_equal(oa_play.require_validation_seed(0, validation=False), 0,
        "ordinary interactive use may retain the engine default seed")
check_raises(ValueError, lambda: oa_play.require_validation_seed(0, validation=True),
            "validation must reject the wall-clock/default seed")


# Tactical outcomes have one central taxonomy. Only actual wins and losses are decisions.
for outcome, decided, won in (
    (oa_play.BattleOutcome.RESOLVED, True, True),
    (oa_play.BattleOutcome.LOST, True, False),
    (oa_play.BattleOutcome.RETURNED, False, False),
    (oa_play.BattleOutcome.WRONG_MODE, False, False),
    (oa_play.BattleOutcome.TIMEOUT, False, False),
):
    result = oa_play.BattleResult(outcome, started_with=10, survivors=7)
    check_equal(result.decided, decided, f"{outcome.value} decision policy")
    check_equal(result.won, won, f"{outcome.value} win policy")
    check_equal(oa_play.battle_is_decided(result.as_dict()), decided,
                f"serialized {outcome.value} decision policy")
    if decided:
        check_equal(oa_play.require_decided_battle(result), outcome,
                    f"{outcome.value} must be scoreable")
    else:
        check_raises(ValueError, lambda result=result: oa_play.require_decided_battle(result),
                     f"{outcome.value} must not be scoreable")
check_equal(oa_play.battle_is_decided({"outcome": "invented"}), False,
            "unknown battle outcomes must fail closed")


class FakeExitedProcess:
    def exit_status(self) -> str:
        return "killed by signal 11 (SIGSEGV)"


class FakeLiveProcess:
    def exit_status(self) -> str:
        return ""


kind, detail = oa_play.classify_connection_failure(
    FakeExitedProcess(), ConnectionRefusedError("connection refused")
)
check_equal(kind, "process_error", "a dead engine must not be mislabeled as transport failure")
check("SIGSEGV" in detail, "process failure evidence must include the engine exit status")
kind, detail = oa_play.classify_connection_failure(
    FakeLiveProcess(), ConnectionResetError("connection reset")
)
check_equal(kind, "transport_error", "a live engine socket failure is a transport failure")


clean_exit = oa_play.ProcessExitResult(True, None, True, False, 0, "exited rc=0")
late_exit = oa_play.ProcessExitResult(True, 0, False, False, 0, "exited rc=0 before stop")
crashed_exit = oa_play.ProcessExitResult(True, 9, False, False, 9, "exited rc=9")
forced_exit = oa_play.ProcessExitResult(True, None, True, True, -9, "killed by signal 9")
check(clean_exit.clean_shutdown, "a requested zero exit must be clean")
check(not late_exit.clean_shutdown, "an unsolicited zero exit before stop is not clean")
check(not crashed_exit.clean_shutdown, "an engine that exited before stop is not clean")
check(not forced_exit.clean_shutdown, "a forced kill is not clean")
oa_play.require_clean_process_exit(clean_exit, "test engine")
check_raises(
    RuntimeError,
    lambda: oa_play.require_clean_process_exit(late_exit, "test engine"),
    "an unsolicited pre-stop zero exit must veto engine-owned success",
)
check_raises(
    RuntimeError,
    lambda: oa_play.require_clean_process_exit(crashed_exit, "test engine"),
    "a pre-stop nonzero exit must veto engine-owned success",
)
check_raises(
    RuntimeError,
    lambda: oa_play.require_clean_process_exit(forced_exit, "test engine"),
    "a forced kill must veto engine-owned success",
)
check_equal(
    oa_play.reconcile_validation_process_exit(True, "victory", "won", 0, clean_exit),
    ("victory", "won", 0),
    "a clean engine exit must preserve validation success",
)
reconciled = oa_play.reconcile_validation_process_exit(
    True, "victory", "won", 0, crashed_exit
)
check_equal(reconciled[0], "process_error",
            "a pre-stop engine exit must override nominal validation success")
check_equal(reconciled[2], 1, "an unclean validation shutdown must be nonzero")


class FakeManagedProcess:
    def __init__(self, returncode=None):
        self.returncode = returncode
        self.killed = False

    def poll(self):
        return self.returncode

    def wait(self, timeout):
        self.returncode = 0
        return self.returncode

    def kill(self):
        self.killed = True
        self.returncode = -9


class FakeQuitHarness:
    sent = []

    def __init__(self, port):
        self.port = port

    def send(self, command):
        self.sent.append((self.port, command))
        return "OK"


original_harness = oa_play.Harness
try:
    oa_play.Harness = FakeQuitHarness
    managed = oa_play.GameProcess(Path("."), 17321, Path("unused.log"))
    managed.proc = FakeManagedProcess()
    stopped = managed.stop()
    check(stopped.clean_shutdown, "GameProcess.stop must report a requested zero exit as clean")
    check_equal(FakeQuitHarness.sent[-1], (17321, "quit"),
                "GameProcess.stop must request engine shutdown before waiting")

    already_dead = oa_play.GameProcess(Path("."), 17322, Path("unused.log"))
    already_dead.proc = FakeManagedProcess(7)
    stopped_dead = already_dead.stop()
    check_equal(stopped_dead.pre_stop_returncode, 7,
                "GameProcess.stop must preserve a pre-existing engine exit")
    check(not stopped_dead.quit_requested,
          "GameProcess.stop must not claim QUIT was sent to an already-dead engine")
finally:
    oa_play.Harness = original_harness

with tempfile.TemporaryDirectory() as tmp:
    missing_repo = Path(tmp) / "missing-repo"
    strict_process = oa_play.GameProcess(
        missing_repo,
        17321,
        Path(tmp) / "strict" / "game.log",
        require_binary_snapshot=True,
    )
    check_raises(RuntimeError, strict_process.snapshot_binary,
        "validation must fail closed when its private binary snapshot cannot be made")


class FakeClock:
    def __init__(self):
        self.now = 1.0

    def time(self) -> float:
        return self.now

    def sleep(self, seconds: float) -> None:
        self.now += seconds


class FakeAdvanceHarness:
    def __init__(self, clock: FakeClock, ticks_per_second: int):
        self.clock = clock
        self.ticks_per_second = ticks_per_second

    def gs(self, query: str) -> dict[str, str]:
        if query == "time":
            ticks = int((self.clock.now - 1.0) * self.ticks_per_second)
            return {"ticks": str(ticks), "day": "1", "week": "1", "time": "00:00"}
        if query == "turbo":
            return {"can_turbo": "1", "hostiles": "0"}
        raise AssertionError(f"unexpected query: {query}")

    def key(self, _key: str) -> None:
        pass


class FakeAdvanceDriver:
    def __init__(self, stages: list[oa_play.Status], ticks_per_second: int):
        self.clock = FakeClock()
        self.h = FakeAdvanceHarness(self.clock, ticks_per_second)
        self.stages = list(stages)
        self.last_stage = self.stages[-1]
        self.checks: dict = {}

    def status(self) -> oa_play.Status:
        if self.stages:
            self.last_stage = self.stages.pop(0)
        return self.last_stage

    def game_over(self) -> bool:
        return False

    def dismiss_modal(self, _status: oa_play.Status) -> bool:
        return False

    def say(self, _message: str) -> None:
        pass


def run_fake_advance(stages: list[oa_play.Status], ticks_per_second: int,
                     budget_s: float = 2.0, game_days: float = 1.0) -> oa_play.AdvanceResult:
    driver = FakeAdvanceDriver(stages, ticks_per_second)
    original_time, original_sleep = oa_play.time.time, oa_play.time.sleep
    original_ticks_per_day = oa_play.TICKS_PER_DAY
    try:
        oa_play.time.time = driver.clock.time
        oa_play.time.sleep = driver.clock.sleep
        oa_play.TICKS_PER_DAY = 100
        return oa_play.advance(driver, game_days, budget_s=budget_s)
    finally:
        oa_play.time.time = original_time
        oa_play.time.sleep = original_sleep
        oa_play.TICKS_PER_DAY = original_ticks_per_day


city = oa_play.Status("CityView", 1280, 720, "")
battle = oa_play.Status("BattleView", 1280, 720, "")
base = oa_play.Status("BaseScreen", 1280, 720, "")
winning = oa_play.Status("VideoScreen", 1280, 720, "", "wingame2.smk")

reached = run_fake_advance([city], ticks_per_second=100)
check_equal(reached.outcome, oa_play.AdvanceOutcome.REACHED,
        "advance must report a reached target")
transition = run_fake_advance([city, battle], ticks_per_second=0)
check_equal(transition.outcome, oa_play.AdvanceOutcome.TRANSITION,
        "battle handoff must be a transition, not a completed leg")
partial = run_fake_advance([city], ticks_per_second=10)
check_equal(partial.outcome, oa_play.AdvanceOutcome.PARTIAL,
        "deadline after some ticks must be partial")
parked = run_fake_advance([city, base], ticks_per_second=0, budget_s=1.0)
check_equal(parked.outcome, oa_play.AdvanceOutcome.PARKED,
        "deadline off CityView must identify the parked stage")
terminal = run_fake_advance([winning], ticks_per_second=0)
check_equal(terminal.outcome, oa_play.AdvanceOutcome.TERMINAL,
        "ending video must stop advance as a terminal transition")
check_equal(terminal.reason, "victory", "advance terminal detail classification")
check_raises(
    ValueError,
    lambda: run_fake_advance([city], ticks_per_second=100, game_days=0.0),
    "advance must reject a zero-day workload",
)
check_raises(
    ValueError,
    lambda: run_fake_advance([city], ticks_per_second=100, game_days=-1.0),
    "advance must reject a negative-day workload",
)
check_raises(
    ValueError,
    lambda: run_fake_advance([city], ticks_per_second=100, budget_s=0.0),
    "advance must reject a zero wall-clock budget",
)


# Validation runs get collision-free directories, an append-only event stream, and one atomic
# terminal receipt. These files are the evidence a campaign gate consumes.
with tempfile.TemporaryDirectory() as tmp:
    base = Path(tmp)
    run_a = oa_play.create_run_directory(base, "campaign", seed=17)
    run_b = oa_play.create_run_directory(base, "campaign", seed=17)
    check(run_a != run_b, "campaign run directories must be unique")
    check(run_a.parent == base and run_b.parent == base,
          "run directories must stay beneath the requested output root")

    receipt = oa_play.RunReceipt(run_a, {"seed": 17, "git_head": "abc123"})
    receipt.event("started", stage="MainMenu")
    snapshot_binary = run_a / "OpenApoc.app/Contents/MacOS/OpenApoc"
    snapshot_binary.parent.mkdir(parents=True)
    snapshot_binary.write_bytes(b"exact executable bytes")
    receipt.record_binary_snapshot(snapshot_binary, [str(snapshot_binary), "--seed=17"])
    receipt.event("advanced", ticks=100)
    receipt.finish("victory", 0, stage="VideoScreen", detail="wingame2.smk",
                   engine_exit=clean_exit.as_dict())

    events = [json.loads(line) for line in (run_a / "events.jsonl").read_text().splitlines()]
    terminal = json.loads((run_a / "terminal.json").read_text())
    check_equal([event["event"] for event in events],
                ["started", "binary_snapshot", "advanced", "terminal"],
                "events must remain append-only through terminal state")
    check_equal(terminal["outcome"], "victory", "terminal receipt outcome")
    check_equal(terminal["exit_code"], 0, "terminal receipt exit code")
    check_equal(terminal["engine_exit"]["pre_stop_returncode"], None,
                "receipt must preserve pre-stop process state")
    check_equal(terminal["engine_exit"]["returncode"], 0,
                "receipt must preserve the actual engine return code")
    check_equal(terminal["provenance"]["seed"], 17, "terminal receipt provenance")
    check_equal(terminal["provenance"]["run_binary"], str(snapshot_binary.resolve()),
                "receipt must identify the private executable that actually ran")
    check_equal(
        terminal["provenance"]["run_binary_sha256"],
        oa_play.file_sha256(snapshot_binary),
        "receipt must hash the private executable after it is copied",
    )
    check_equal(terminal["provenance"]["engine_argv"][1], "--seed=17",
                "receipt must preserve the exact engine launch arguments")
    check(not any(run_a.glob("*.tmp-*")), "atomic receipt must not leave a temporary file")
    check_raises(RuntimeError, lambda: receipt.finish("defeat", 1),
                 "a terminal receipt must be immutable once written")


if FAILED:
    print(f"FAILED {len(FAILED)}:")
    for failure in FAILED:
        print("  -", failure)
    raise SystemExit(1)

print("all unattended-run result tests passed")
