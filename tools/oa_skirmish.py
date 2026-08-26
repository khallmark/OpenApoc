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
manually popped, Skirmish::resume()->loadBattle() queues the battle transition.

The complete cold battle is a preserved known-red acceptance until the live-FIFO StageCmd change
lands. A separate observed map-generation exception remains a distinct setup/engine failure; it
is not evidence that a battle was fought or lost.
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

from oa_play import Driver, GameProcess, Harness, HarnessError, new_game, win_battle

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


def open_skirmish(d: Driver) -> bool:
    """Reach Skirmish from the cold MainMenu or an already-running game."""
    st = d.status()
    if st.stage == "MainMenu":
        if not d.click_id("BUTTON_SKIRMISH", st):
            d.say("  [skirmish] MainMenu Skirmish button was unavailable")
            return False
        try:
            # Skirmish::begin() immediately pushes MapSelector. The snapshot-drain loop can lose
            # that nested PUSH and leave Skirmish current; pick_map() can continue from either.
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
                    budget_s: float = 300.0, policy: dict | None = None) -> str:
    """Configure a force via SelectForces and fight it. Returns win_battle's outcome string.

    aliens: {short name from ALIEN_SLIDERS: count}. Anything not named is left at zero by
    unchecking DEFAULT_ALIENS first -- otherwise the screen fills in its own default mix, which
    defeats the point of testing one specific threat in isolation.
    """
    st = d.status()
    if st.stage != "Skirmish":
        d.say(f"  [skirmish] not on the Skirmish screen ({st.stage})")
        return "setup-failed"
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
        return "setup-failed"
    time.sleep(1.0)
    if d.status().stage != "SelectForces":
        d.say(f"  [skirmish] expected SelectForces, got {d.status().stage}")
        return "setup-failed"

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
    # SelectForces::resume() queues its own POP after AEquipScreen closes. The framework currently
    # clears commands queued by resume() while draining a snapshot, so this expected POP can be
    # lost and leave SelectForces current. If SelectForces is manually popped, Skirmish::resume()
    # queues loadBattle's transition into the same broken drain and can remain on Skirmish. Do not
    # press BUTTON_OK again here: that re-enters goToBattle and pushes AEquipScreen forever.
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
            detail = "SelectForces resume POP was not applied"
        elif seen_equip and final_stage == "Skirmish":
            detail = "Skirmish loadBattle transition was not applied"
        else:
            detail = "battle stage was not reached"
        d.say(f"  [skirmish] setup failed at {final_stage}: {detail}")
        return "setup-failed"

    return win_battle(d, budget_s=budget_s, policy=policy)


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
        return {"outcome": "could-not-open-skirmish"}
    if not pick_map(d, map_row):
        d.say("  [skirmish] map selection failed; using whatever was already chosen")
    battle_before = battle_snapshot(d)
    outcome = fight_skirmish(d, aliens, real_time=real_time, budget_s=budget_s,
                             policy=policy)
    return {
        "outcome": outcome,
        "aliens": aliens,
        "map_row": map_row,
        "before": battle_before,
    }


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
    ap.add_argument("--cold-main-menu", action="store_true",
                    help="use MainMenu's first-class Skirmish entry instead of starting a game")
    args = ap.parse_args()

    aliens = {}
    for spec in args.alien:
        name, _, count = spec.partition("=")
        aliens[name] = int(count or 1)
    if not aliens:
        aliens = {"popper": 4, "skeletoid": 4}  # a reasonable default mixed threat

    repo = Path(args.repo)
    out = Path(args.out) if args.out else repo / "build/skirmish"
    out.mkdir(parents=True, exist_ok=True)

    game = GameProcess(repo, args.port, out / "game.log")
    game.start(wait_s=240)
    d = Driver(Harness(port=args.port), repo / "data/forms", shots=out / "shots", verbose=True)
    d.checks = {}
    if not args.cold_main_menu:
        new_game(d, 1)

    results = []
    for i in range(args.rounds):
        d.say(f"=== skirmish round {i + 1}/{args.rounds}: {aliens} ===")
        r = run_one(d, aliens, map_row=args.map_row, budget_s=args.budget)
        results.append(r)
        d.say(f"    -> {r['outcome']}")
        # Escape back to CityView between rounds, whatever state we ended in.
        for _ in range(6):
            st = d.status()
            if st.stage in ("CityView", "MainMenu"):
                break
            if st.stage == "BattleDebriefing":
                d.click_id("BUTTON_OK", st)
            elif not d.dismiss_modal(st):
                d.h.key("Escape")
            time.sleep(0.6)

    wins = sum(1 for r in results if r["outcome"] == "resolved")
    withdrew = sum(1 for r in results if r["outcome"] == "withdrew")
    lost = sum(1 for r in results if r["outcome"] == "lost")
    d.say(f"=== summary: {wins} won, {withdrew} withdrew, {lost} lost, "
          f"{len(results) - wins - withdrew - lost} other/{len(results)} total ===")
    game.stop()
    return 0 if wins > 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
