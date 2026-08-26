#!/usr/bin/env python3
"""Fast, isolated tactical-battle testing via Skirmish mode.

Running a full campaign to reach a single battle costs game-days of simulated time and drags in
everything else the driver does -- research, purchasing, recovery -- as noise around the one
thing actually being tested. Skirmish mode is meant to fight a single battlescape with a chosen
map and a chosen alien force in under a minute of wall-clock, which is what iterating on combat
tactics (squad size, retreat threshold, movement pattern) actually needs.

CURRENT STATUS: MainMenu has a first-class Skirmish button, and the robot drives that cold path
without manufacturing a campaign. The present framework drains a snapshot of pending StageCmds.
If a stage's `resume()` queues another command during that drain, the new command is cleared with
the snapshot instead of being processed. Two commands in the Skirmish lifecycle are therefore
lost: SelectForces::resume() queues its own POP after AEquipScreen closes, and, if SelectForces is
manually popped, Skirmish::resume()->loadBattle() queues the battle transition. The tool reports
either condition as `setup_failure`, never as a tactical loss.

This R0 result-truth slice is intentionally expected-red for a complete cold battle on its own.
Once the live-FIFO StageCmd fix is integrated, the same one-round validation command becomes a
required-green battle smoke. A separate observed map-generation exception remains a distinct
setup/engine failure; it is not evidence that a battle was fought or lost.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
from dataclasses import dataclass
from pathlib import Path

from oa_play import (
    BattleResult,
    Driver,
    GameProcess,
    Harness,
    HarnessError,
    RunReceipt,
    classify_connection_failure,
    create_run_directory,
    new_game,
    reconcile_validation_process_exit,
    require_positive,
    require_validation_seed,
    run_provenance,
    win_battle,
)

# Slider ids from data/forms/selectforces.form, keyed by the same short names players know them
# by. Values are per-mission counts; the sliders themselves cap out well above what is useful for
# a single test.
ALIEN_SLIDERS = {
    "anthropod": "NUM_ANTHROPOD_SLIDER",
    "brainsucker": "NUM_BSK_SLIDER",
    "chrysalis": "NUM_CHRYS_SLIDER",
    "egg": "NUM_EGG_SLIDER",
    "hyperworm": "NUM_HYPERWORM_SLIDER",
    "megaspawn": "NUM_MEGA_SLIDER",
    "micronoid": "NUM_MICRO_SLIDER",
    "multiworm": "NUM_MULTIWORM_SLIDER",
    "popper": "NUM_POPPER_SLIDER",
    "psimorph": "NUM_PSI_SLIDER",
    "skeletoid": "NUM_SKEL_SLIDER",
    "spitter": "NUM_SPITTER_SLIDER",
}


@dataclass(frozen=True)
class SkirmishAttempt:
    """Keep setup/transition failures out of the tactical outcome namespace."""

    result_kind: str
    outcome: str | None
    stage: str
    reason: str = ""
    decided: bool = False
    started_with: int = 0
    survivors: int | None = None
    mission_type: str = "unknown"
    seconds: float = 0.0

    @classmethod
    def setup_failure(cls, stage: str, reason: str) -> "SkirmishAttempt":
        return cls("setup_failure", None, stage, reason)

    @classmethod
    def gameplay(cls, result: BattleResult, stage: str) -> "SkirmishAttempt":
        return cls(
            "gameplay",
            result.outcome.value,
            stage,
            decided=result.decided,
            started_with=result.started_with,
            survivors=result.survivors,
            mission_type=result.mission_type,
            seconds=result.seconds,
        )

    def as_dict(self) -> dict:
        return {
            "result_kind": self.result_kind,
            "outcome": self.outcome,
            "stage": self.stage,
            "reason": self.reason,
            "decided": self.decided,
            "started_with": self.started_with,
            "survivors": self.survivors,
            "mission_type": self.mission_type,
            "seconds": self.seconds,
        }


def setup_failure(d: Driver, reason: str) -> SkirmishAttempt:
    try:
        stage = d.status().stage
    except Exception:
        stage = "unavailable"
    d.say(f"  [skirmish] setup failed at {stage}: {reason}")
    return SkirmishAttempt.setup_failure(stage, reason)


def open_skirmish(d: Driver) -> bool:
    """Reach Skirmish from the cold MainMenu or an already-running game."""
    st = d.status()
    if st.stage == "MainMenu":
        if not d.click_id("BUTTON_SKIRMISH", st):
            d.say("  [skirmish] MainMenu Skirmish button was unavailable")
            return False
        try:
            # Skirmish::begin() immediately pushes MapSelector. The old snapshot-drain loop can
            # lose that nested PUSH and leave Skirmish current; both are valid states from which
            # pick_map() can continue, while PR B makes MapSelector the deterministic result.
            return d.wait_for(("MapSelector", "Skirmish"), 240).stage in (
                "MapSelector", "Skirmish"
            )
        except TimeoutError:
            d.say(f"  [skirmish] cold entry stalled on {d.status().stage}")
            return False
    if st.stage not in ("CityView", "BattleView"):
        d.say(f"  [skirmish] need an in-progress game, got {st.stage}")
        return False
    d.h.key("Escape")
    time.sleep(0.8)
    if d.status().stage != "InGameOptions":
        d.say(f"  [skirmish] Escape did not open InGameOptions ({d.status().stage})")
        return False
    if not d.click_id("BUTTON_SKIRMISH", d.status()):
        return False
    try:
        return d.wait_for(("MapSelector", "Skirmish"), 240).stage in (
            "MapSelector", "Skirmish"
        )
    except TimeoutError:
        return False


def pick_map(d: Driver, row: int = 0) -> bool:
    """Skirmish -> BUTTON_SELECTMAP -> MapSelector -> pick a map by row -> back to Skirmish.

    A map row is not chosen by ListBox selection -- MapSelector never listens for
    ListBoxChangeSelected at all (mapselector.cpp:154-173). Each row is an inert Control holding
    a Label plus a small GraphicButton child; only clicking that button calls
    Skirmish::setLocation and actually records a choice. `set <row>` merely highlights a row and
    changes nothing, which let BUTTON_OK silently re-open MapSelector on the next click instead
    of proceeding to SelectForces -- reached with `item <row> item 1 click`.
    """
    st = d.status()
    if st.stage == "Skirmish":
        if not d.click_id("BUTTON_SELECTMAP", st):
            return False
        time.sleep(0.8)
        st = d.status()
    if st.stage != "MapSelector":
        return False
    try:
        if not d.h.send(f"control LISTBOX_MAPS item {row} item 1 click").startswith("OK"):
            return False
    except HarnessError:
        return False
    time.sleep(0.5)
    return d.status().stage == "Skirmish"


def fight_skirmish(d: Driver, aliens: dict, real_time: bool = True,
                    budget_s: float = 300.0,
                    policy: dict | None = None) -> SkirmishAttempt:
    """Configure a force and return a setup or gameplay result with no overlap.

    aliens: {short name from ALIEN_SLIDERS: count}. Anything not named is left at zero by
    unchecking DEFAULT_ALIENS first -- otherwise the screen fills in its own default mix, which
    defeats the point of testing one specific threat in isolation.
    """
    st = d.status()
    if st.stage != "Skirmish":
        return setup_failure(d, "not_on_skirmish_screen")
    # Whether BUTTON_OK reaches SelectForces at all depends on the location type and this
    # checkbox (skirmish.cpp:530-545): a Base always customizes, a UFO never does (it goes to
    # AEquipScreen for boarding loadout instead), and an alien Building uses its own preset crew
    # and skips SelectForces entirely *unless* CUSTOMISE_FORCES is checked first. Since the whole
    # point here is dialling in a specific alien mix, force it on regardless of location type.
    try:
        d.h.control("CUSTOMISE_FORCES", "set", "true")
    except HarnessError:
        pass
    time.sleep(0.2)
    if not d.click_id("BUTTON_OK", st):
        return setup_failure(d, "skirmish_ok_unavailable")
    time.sleep(1.0)
    if d.status().stage != "SelectForces":
        d.say(f"  [skirmish] expected SelectForces, got {d.status().stage}")
        return setup_failure(d, "select_forces_not_reached")

    try:
        d.h.control("DEFAULT_ALIENS", "set", "false")
    except HarnessError:
        pass
    time.sleep(0.2)
    for name, count in aliens.items():
        slider = ALIEN_SLIDERS.get(name)
        if not slider:
            d.say(f"  [skirmish] unknown alien type {name!r}, skipping")
            continue
        try:
            d.h.control(slider, "set", str(count))
        except HarnessError as exc:
            d.say(f"  [skirmish] could not set {slider}: {exc}")
        time.sleep(0.1)

    d.click_id("BUTTON_OK", d.status())

    # One more setup screen can follow: AEquipScreen, to arm the player's own squad before the
    # raid begins, the same as a real BuildingScreen raid would show. win_battle does not know
    # this stage at all, so without handling it here the driver just sits pressing Escape/Return
    # against a real screen that needs BUTTON_OK.
    #
    # goToBattle generates the battlemap on a threadpool worker (the same mechanism that, when it
    # throws, produces the "Exception occurred in threadpool" signature seen in this engine's
    # crash reports elsewhere), and generation for a full map with LOS precomputation is not
    # instant. The screen can appear to sit on SelectForces for a while that is actually
    # generation still running in the background, not a stall -- so this gives it real time
    # rather than declaring failure quickly.
    # Skirmish::resume() fires loadBattle() only once BOTH AEquipScreen and SelectForces have
    # popped back to Skirmish (skirmish.cpp:698-702). AEquipScreen is pushed BY goToBattle, so
    # dismissing it returns us to SelectForces -- which then has to be dismissed a SECOND time.
    #
    # This loop used to sleep on SelectForces and nothing else, so it waited out its whole budget
    # against a screen one button press would have cleared, and the failure was recorded as
    # "loadBattle never transitions" -- an engine bug that was not there. The engine log said as
    # much all along: goToBattle ran to completion ("Resetting base inventory") and the
    # no-location diagnostic never fired, so a location WAS set and the lambda simply never got
    # its second pop.
    #
    # Re-press on a cadence rather than every poll: goToBattle rebuilds the base inventory each
    # time it runs, and hammering OK re-enters it repeatedly for no gain.
    deadline = time.time() + 45.0
    seen_equip = False
    last_ok = 0.0
    while time.time() < deadline:
        st = d.status()
        if st.stage in ("BattleBriefing", "BattlePreStart", "BattleView"):
            break
        if st.stage == "MessageBox":
            # Empty force, no map chosen, etc. -- surface it rather than hang on it.
            d.say(f"  [skirmish] setup message: {d.h.send('controls')[:150]}")
            d.h.key("Return")
        elif st.stage == "AEquipScreen":
            seen_equip = True
            d.click_id("BUTTON_OK", st)
        elif st.stage == "SelectForces":
            # DO NOT press anything here. SelectForces::resume() pops ITSELF
            # (selectforces.cpp:321), so once AEquipScreen closes the cascade runs on its own:
            # SelectForces pops -> Skirmish::resume() -> loadBattle(). Pressing BUTTON_OK instead
            # re-enters goToBattle, which pushes AEquipScreen again -- an infinite setup loop,
            # observed fifteen times in a row when this was "fixed" that way.
            if seen_equip and time.time() - last_ok > 5.0:
                last_ok = time.time()
                d.say("  [skirmish] equip done; waiting for the engine's own pop cascade")
        else:
            break
        time.sleep(0.7)

    final_stage = d.status().stage
    if final_stage not in ("BattleBriefing", "BattlePreStart", "BattleView"):
        if seen_equip and final_stage == "SelectForces":
            reason = "select_forces_resume_pop_not_applied"
        elif seen_equip and final_stage == "Skirmish":
            reason = "skirmish_load_battle_transition_not_applied"
        else:
            reason = "battle_stage_not_reached"
        return setup_failure(d, reason)

    result = win_battle(d, budget_s=budget_s, policy=policy)
    return SkirmishAttempt.gameplay(result, final_stage)


def battle_snapshot(d: Driver) -> dict | None:
    """Return pre-battle state when a GameState-backed query handler exists."""
    try:
        return d.h.gs("battle")
    except HarnessError as exc:
        if "ERR no gamestate" not in str(exc):
            raise
        return None


def run_one(d: Driver, aliens: dict, map_row: int = 0, real_time: bool = True,
            budget_s: float = 300.0, policy: dict | None = None) -> dict:
    """One full cycle: open skirmish, pick a map, fight, return a result summary."""
    if not open_skirmish(d):
        attempt = setup_failure(d, "could_not_open_skirmish")
        return {**attempt.as_dict(), "aliens": aliens, "map_row": map_row, "before": {}}
    if not pick_map(d, map_row):
        attempt = setup_failure(d, "map_selection_failed")
        return {**attempt.as_dict(), "aliens": aliens, "map_row": map_row, "before": {}}
    battle_before = battle_snapshot(d)
    attempt = fight_skirmish(d, aliens, real_time=real_time, budget_s=budget_s,
                             policy=policy)
    return {
        **attempt.as_dict(),
        "aliens": aliens,
        "map_row": map_row,
        "before": battle_before,
    }


def recover_for_next_skirmish(d: Driver, tries: int = 8) -> bool:
    """Return to a stage from which open_skirmish() can start another attempt."""
    for _ in range(tries):
        st = d.status()
        if st.stage in ("CityView", "MainMenu"):
            return True
        if st.stage == "BattleDebriefing":
            d.click_id("BUTTON_OK", st)
        elif not d.dismiss_modal(st):
            d.h.key("Escape")
        time.sleep(0.6)
    return d.status().stage in ("CityView", "MainMenu")


def append_result(path: Path, result: dict) -> None:
    """Append and durably flush one attempt before another battle can start."""
    with path.open("a") as stream:
        stream.write(json.dumps(result, sort_keys=True) + "\n")
        stream.flush()
        os.fsync(stream.fileno())


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", type=int, default=17900)
    ap.add_argument("--repo", default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument("--out", default=None)
    ap.add_argument("--rounds", type=int, default=5, help="how many battles to fight in a row")
    ap.add_argument("--alien", action="append", default=[],
                     help="name=count, repeatable, e.g. --alien popper=6 --alien brainsucker=3")
    ap.add_argument("--map-row", type=int, default=0)
    ap.add_argument("--budget", type=float, default=300.0, help="seconds per battle")
    ap.add_argument("--seed", type=int, default=0,
                    help="explicit RNG seed; required and nonzero in validation mode")
    ap.add_argument("--validation", action="store_true",
                    help="write immutable evidence and fail closed on setup/driver errors")
    ap.add_argument("--cold-main-menu", action="store_true",
                    help="use MainMenu's first-class Skirmish entry instead of starting a game")
    args = ap.parse_args()
    if args.rounds <= 0:
        ap.error("--rounds must be positive")
    try:
        require_positive(args.budget, "--budget")
    except ValueError as exc:
        ap.error(str(exc))

    try:
        seed = require_validation_seed(args.seed, args.validation)
    except ValueError as exc:
        print(f"oa_skirmish: {exc}", file=sys.stderr)
        return 2

    aliens = {}
    for spec in args.alien:
        name, _, count = spec.partition("=")
        aliens[name] = int(count or 1)
    if not aliens:
        aliens = {"popper": 4, "skeletoid": 4}  # a reasonable default mixed threat

    repo = Path(args.repo)
    out_root = Path(args.out) if args.out else repo / "build/skirmish"
    out = create_run_directory(out_root, "skirmish", seed) if args.validation else out_root
    out.mkdir(parents=True, exist_ok=True)
    receipt = RunReceipt(
        out, run_provenance(repo, seed, args.validation, "oa_skirmish")
    ) if args.validation else None

    game = GameProcess(
        repo,
        args.port,
        out / "game.log",
        seed=seed,
        require_binary_snapshot=args.validation,
    )
    d = None
    results = []
    outcome = "not_started"
    exit_code = 1
    detail = ""
    try:
        game.start(wait_s=240)
        if receipt:
            receipt.record_binary_snapshot(game._run_binary, game.argv)
            receipt.event("started", stage="MainMenu", cold_main_menu=args.cold_main_menu)
        d = Driver(
            Harness(port=args.port),
            repo / "data/forms",
            shots=out / "shots",
            verbose=True,
        )
        d.checks = {}
        if not args.cold_main_menu:
            new_game(d, 1)

        ledger = out / "results.jsonl"
        for i in range(args.rounds):
            d.say(f"=== skirmish round {i + 1}/{args.rounds}: {aliens} ===")
            result = run_one(d, aliens, map_row=args.map_row, budget_s=args.budget)
            result["round"] = i + 1
            results.append(result)
            append_result(ledger, result)
            if receipt:
                receipt.event("skirmish_attempt", **result)
            d.say(f"    -> {result['result_kind']} / {result['outcome']}"
                  f" ({result['reason'] or result['stage']})")
            if result["result_kind"] != "gameplay":
                outcome = "setup_failed"
                detail = f"{result['stage']}: {result['reason']}"
                break
            if not result.get("decided", False):
                outcome = "gameplay_incomplete"
                detail = str(result["outcome"])
                break
            if i + 1 < args.rounds and not recover_for_next_skirmish(d):
                recovery = setup_failure(d, "could_not_recover_for_next_round").as_dict()
                recovery.update(round=i + 2, aliens=aliens, map_row=args.map_row, before={})
                results.append(recovery)
                append_result(ledger, recovery)
                if receipt:
                    receipt.event("skirmish_attempt", **recovery)
                outcome = "setup_failed"
                detail = f"{recovery['stage']}: {recovery['reason']}"
                break
        else:
            outcome = "completed"
            exit_code = 0

        gameplay = [r for r in results if r["result_kind"] == "gameplay"]
        decided = [r for r in gameplay if r.get("decided", False)]
        setup_failures = [r for r in results if r["result_kind"] == "setup_failure"]
        wins = sum(1 for r in decided if r["outcome"] == "resolved")
        losses = sum(1 for r in decided if r["outcome"] == "lost")
        d.say(f"=== summary: {len(decided)} decided battles ({wins} won, {losses} lost), "
              f"{len(gameplay) - len(decided)} incomplete, "
              f"{len(setup_failures)} setup failures, {len(results)} attempts ===")
    except TimeoutError as exc:
        outcome, detail = "timeout", str(exc)
    except HarnessError as exc:
        outcome, detail = "protocol_error", str(exc)
    except OSError as exc:
        outcome, detail = classify_connection_failure(game, exc)
    except RuntimeError as exc:
        outcome, detail = "process_error", str(exc)
    except Exception as exc:  # validation evidence must name unexpected driver failures
        outcome, detail = "unexpected_error", f"{type(exc).__name__}: {exc}"
    finally:
        stage = "unavailable"
        if d is not None:
            try:
                stage = d.status().stage
            except Exception:
                pass
        process_exit = game.stop()
        outcome, detail, exit_code = reconcile_validation_process_exit(
            args.validation, outcome, detail, exit_code, process_exit
        )
        if receipt and not receipt.finished:
            receipt.finish(
                outcome,
                exit_code,
                stage=stage,
                detail=detail,
                attempts=len(results),
                gameplay=sum(1 for result in results
                             if result["result_kind"] == "gameplay"),
                decided_battles=sum(1 for result in results if result.get("decided", False)),
                incomplete_battles=sum(1 for result in results
                                       if result["result_kind"] == "gameplay"
                                       and not result.get("decided", False)),
                setup_failures=sum(1 for result in results
                                   if result["result_kind"] == "setup_failure"),
                engine_exit=process_exit.as_dict(),
            )
    if exit_code:
        print(f"oa_skirmish: {outcome}: {detail or 'run did not complete'}", file=sys.stderr)
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
