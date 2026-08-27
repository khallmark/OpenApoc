#!/usr/bin/env python3
"""Fast, isolated tactical-battle testing via Skirmish mode.

Running a full campaign to reach a single battle costs game-days of simulated time and drags in
everything else the driver does -- research, purchasing, recovery -- as noise around the one
thing actually being tested. Skirmish mode is meant to fight a single battlescape with a chosen
map and a chosen alien force in under a minute of wall-clock, which is what iterating on combat
tactics (squad size, retreat threshold, movement pattern) actually needs.

ROOT CAUSE FOUND AND FIXED -- and it was TWO swallowed commands, not one:

    const auto commandsThisFrame = stageCommands;   // a copy
    for (const StageCmd &cmd : commandsThisFrame) { ... }
    stageCommands.clear();                          // discards anything queued DURING processing

StageStack::pop() calls resume() on the stage it uncovers, so every resume() runs INSIDE that
drain, and anything it queues is appended after the copy and thrown away. Two stages on this path
did exactly that:

  * SelectForces::resume() queued a POP of itself, so Skirmish underneath could be reached.
  * Skirmish::resume() called loadBattle(), and every branch of it ends in a REPLACEALL.

Fixing only the first would have produced no battle either: the stage genuinely returned to
"Skirmish" and its REPLACEALL was discarded in the same drain. That is precisely the
"loadBattle never transitions anywhere" this file used to report, and why the earlier diagnosis
-- which named only SelectForces -- was half an answer.

The repair is in those two stages, NOT in the framework: resume() sets a flag, update() issues the
command. update() runs before the copy is taken, so it survives, and deferring by one frame is the
entire cost. The framework's discard is deliberately left alone -- it is load-bearing for stage
teardown, since StageStack::clear() resumes every stage it is about to destroy, and the naive
repair of preserving mid-drain commands to the next frame was tried, built green, passed 37/37 and
broke opening Skirmish at all. Do not re-apply it.

Verified A/B on 2026-08-26. Both binaries built from the SAME working tree, differing only in the
four files above (the control was produced by `git checkout HEAD --` on them, so a third party's
concurrent edits elsewhere in the tree are present in both and cannot explain the difference).
Same driver, same map row 0, same alien mix, one round each:

    control (fix reverted)   did not reach a battle stage (at SelectForces), 45s deadline expired
    fixed                    resolved -- 14 of 17 survived, mission_type=base_defense

Reproduced on a second fixed-binary run: round 1 resolved, 14 survivors.

A green build and 37/37 were true of the reverted framework attempt as well; the trace above is
the evidence that discriminates, and a build result is not.

OPEN, AND NEWLY REACHABLE: --rounds 2 does not work. Round 1 fights and resolves; round 2 runs
its setup to completion (the log shows a second "Adding new agents to base BASE_SKIRMISH" /
"Resetting base inventory") and then the process disappears -- the driver's next status query gets
ConnectionRefusedError, with nothing in game.log and no macOS crash report. Reproduced 2/2. This is
not a regression: nobody could reach a second skirmish battle before, because they could not reach
the first. Cause not established. The likeliest place to look is goToBattle re-running against a
BASE_SKIRMISH that round 1 already populated, and battle generation then faulting on a threadpool
worker -- the same async-generation mechanism named below. Until it is understood, run one round
per process.

Also now real, and worth a decision: Escape out of AEquipScreen no longer leaves the player parked
on SelectForces, it proceeds into the battle. That is what the original code asked for -- resume()
popped unconditionally -- it simply never executed. There is currently no cancel path once
BUTTON_OK has been pressed.

CURRENT STATUS: setup is fully driven and reliable (Skirmish -> pick a map -> SelectForces ->
name an alien mix -> AEquipScreen), and the battle now starts. One battlemap (BATTLEMAP_43sleep)
was separately observed to throw during generation on a background thread ("Failed to place
mandatory sectors...", "Exception occurred in threadpool: vector"). That is a genuine pre-existing
bug in battlemap generation, untouched by this fix and still open -- a run that dies there is a
different fault from the stage cascade, and should be reported as such.

None of this touches the main campaign. City missions reach battle through a different code path
entirely -- loadBattleBuilding from BuildingScreen/AlertScreen, not Skirmish::goToBattle -- and
that path has been fighting real missions (wins, losses, retreats) throughout. SelectForces and
Skirmish are constructed nowhere outside this mode (ingameoptions.cpp:266, skirmish.cpp:634), so
the blast radius of the fix is Skirmish mode and nothing else.

There is no way to reach Skirmish from a cold MainMenu: it lives behind InGameOptions, which only
exists once a game is running. So a run is: boot -> new game -> Escape into InGameOptions ->
BUTTON_SKIRMISH -> MapSelector -> SelectForces -> fight -> read the result -> repeat with a fresh
force selection, all inside the same process. Reported findings apply to the shared battle code
(win_battle, unit selection, retreat) that the full campaign also uses.
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
    """From an already-running game, reach Skirmish's map selector."""
    st = d.status()
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
    time.sleep(1.0)
    return d.status().stage == "Skirmish"


def pick_map(d: Driver, row: int = 0) -> bool:
    """Skirmish -> BUTTON_SELECTMAP -> MapSelector -> pick a map by row -> back to Skirmish.

    A map row is not chosen by ListBox selection -- MapSelector never listens for
    ListBoxChangeSelected at all (mapselector.cpp:154-173). Each row is an inert Control holding
    a Label plus a small GraphicButton child; only clicking that button calls
    Skirmish::setLocation and actually records a choice. `set <row>` merely highlights a row and
    changes nothing, which let BUTTON_OK silently re-open MapSelector on the next click instead
    of proceeding to SelectForces -- reached with `item <row> item 1 click`.
    """
    if not d.click_id("BUTTON_SELECTMAP", d.status()):
        return False
    time.sleep(0.8)
    if d.status().stage != "MapSelector":
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
            # DO NOT press anything here. SelectForces pops ITSELF one frame after being
            # resumed (selectforces.cpp, resume() sets the flag and update() issues the POP), so
            # once AEquipScreen closes the cascade runs on its own: SelectForces pops ->
            # Skirmish::resume() -> Skirmish::update() -> loadBattle(). Pressing BUTTON_OK
            # instead re-enters goToBattle, which pushes AEquipScreen again -- an infinite setup
            # loop, observed fifteen times in a row when this was "fixed" that way.
            if seen_equip and time.time() - last_ok > 5.0:
                last_ok = time.time()
                d.say("  [skirmish] equip done; waiting for the engine's own pop cascade")
        else:
            break
        time.sleep(0.7)

    if d.status().stage not in ("BattleBriefing", "BattlePreStart", "BattleView"):
        d.say(f"  [skirmish] did not reach a battle stage (at {d.status().stage})")
        return "setup-failed"

    return win_battle(d, budget_s=budget_s, policy=policy)


def run_one(d: Driver, aliens: dict, map_row: int = 0, real_time: bool = True,
            budget_s: float = 300.0, policy: dict | None = None) -> dict:
    """One full cycle: open skirmish, pick a map, fight, return a result summary."""
    if not open_skirmish(d):
        return {"outcome": "could-not-open-skirmish"}
    if not pick_map(d, map_row):
        d.say("  [skirmish] map selection failed; using whatever was already chosen")
    battle_before = d.h.gs("battle")
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
    ap.add_argument("--port", type=int, default=0,
                    help="harness port; 0 picks a free one near 17900")
    ap.add_argument("--repo", default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument("--out", default=None)
    ap.add_argument("--rounds", type=int, default=5, help="how many battles to fight in a row")
    ap.add_argument("--alien", action="append", default=[],
                     help="name=count, repeatable, e.g. --alien popper=6 --alien brainsucker=3")
    ap.add_argument("--map-row", type=int, default=0)
    ap.add_argument("--budget", type=float, default=300.0, help="seconds per battle")
    args = ap.parse_args()
    args.port = args.port or free_port(17900)

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
            if st.stage == "CityView":
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
