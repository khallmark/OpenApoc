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
        oa_play.Status("VideoScreen", 1280, 720, "", "mystery.smk")
    ),
    "unexpected_terminal",
    "an unrecognised terminal video must fail closed",
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
                     budget_s: float = 2.0) -> oa_play.AdvanceResult:
    driver = FakeAdvanceDriver(stages, ticks_per_second)
    original_time, original_sleep = oa_play.time.time, oa_play.time.sleep
    original_ticks_per_day = oa_play.TICKS_PER_DAY
    try:
        oa_play.time.time = driver.clock.time
        oa_play.time.sleep = driver.clock.sleep
        oa_play.TICKS_PER_DAY = 100
        return oa_play.advance(driver, 1.0, budget_s=budget_s)
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
    receipt.finish("victory", 0, stage="VideoScreen", detail="wingame2.smk")

    events = [json.loads(line) for line in (run_a / "events.jsonl").read_text().splitlines()]
    terminal = json.loads((run_a / "terminal.json").read_text())
    check_equal([event["event"] for event in events],
                ["started", "binary_snapshot", "advanced", "terminal"],
                "events must remain append-only through terminal state")
    check_equal(terminal["outcome"], "victory", "terminal receipt outcome")
    check_equal(terminal["exit_code"], 0, "terminal receipt exit code")
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
