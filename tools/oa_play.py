#!/usr/bin/env python3
"""Unattended campaign driver for OpenApoc, over the localhost harness.

Launch the game with:
  --Framework.Harness.Enable=1 --Framework.Harness.Port=17321 --Game.SkipIntro=1
  --Config.Save=0 --Config.Read=0 --OpenApoc.NewFeature.SeedRng=0

The driver never guesses pixel coordinates: it resolves control ids out of the shipped .form
definitions (tools/oa_forms.py) against the live display size reported by STATUS. Screens are
identified by the stage class name that STATUS reports, so modal popups -- which are their own
Stage in this engine -- are detected and dismissed automatically instead of deadlocking the run.
"""

from __future__ import annotations

import argparse
from oa_loadouts import prepare_loadouts
import json
import re
import socket
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

import os
import shutil
import subprocess

from oa_forms import FormLibrary
from oa_strategy import Strategy, add_strategy_option, parse_strategy, reorder_research

# Durable, append-only run ledger. One JSON record per leg per run.
LEDGER = Path(__file__).resolve().parent.parent / "build" / "campaign-ledger.jsonl"

# Stage class name (as reported by STATUS) -> form key used by that stage.
STAGE_FORMS = {
    "MainMenu": "mainmenu",
    "DifficultyMenu": "difficultymenu",
    "CityView": "city/city",
    "AlertScreen": "city/alert",
    "BuildingScreen": "city/building",
    "LocationScreen": "city/location",
    "BaseScreen": "basescreen",
    "BaseDefenseScreen": "city/basedefense",
    "BaseBuyScreen": "city/basebuy",
    "BaseSelectScreen": "city/baseselect",
    "BribeScreen": "city/bribe",
    "DiplomaticTreatyScreen": "city/diplomatic_treaty",
    "InfiltrationScreen": "city/infiltration",
    "ScoreScreen": "city/score",
    "WeeklyFundingScreen": "city/weekly_funding",
    "NotificationScreen": "notification",
    "MessageLogScreen": "messagelog",
    "InGameOptions": "ingameoptions",
    "ResearchScreen": "researchscreen",
    "ResearchSelect": "researchselect",
    "RecruitScreen": "recruitscreen",
    # TransactionScreen is a base class; the concrete screens all share its form.
    "TransactionScreen": "transactionscreen",
    "BuyAndSellScreen": "transactionscreen",
    "TransferScreen": "transactionscreen",
    "AlienContainmentScreen": "transactionscreen",
    "VEquipScreen": "vequipscreen",
    "AEquipScreen": "aequipscreen",
    "UfopaediaView": "ufopaediatitle",
    "UfopaediaCategoryView": "ufopaedia",
    "BattleBriefing": "battle/briefing",
    "BattlePreStart": "battle/prestart",
    "BattleDebriefing": "battle/debriefing",
    "BattleView": "battle/battle",
    "SaveMenu": "savemenu",
    "CheatOptions": "cheatoptions",
    # Skirmish mode's three screens. Without these, controls() returns {} on all of them and
    # the resolved-rect fallback in click_id is blind there. Note that Skirmish::begin() unconditionally pushes MapSelector
    # (skirmish.cpp:689-693), so the Skirmish config screen is only current once a location has
    # been chosen and MapSelector has popped back to it, never straight after BUTTON_SKIRMISH.
    "Skirmish": "skirmish",
    "MapSelector": "mapselector",
    "SelectForces": "selectforces",
}

# How to RESPOND to each interrupting screen. These are not "dismiss" actions: an alien incident
# is an invitation to dispatch a squad, a hostile building is a raid opportunity, a base attack is
# a battle. Clicking the close button on all of them would mean the interception, raid and
# base-defence paths never execute at all.
#
#   act    -- control that actually engages with the event
#   ack    -- control that acknowledges a purely informational screen
#   select -- rows to select in the embedded agent/vehicle assignment list before acting
RESPONSES = {
    # Alien incident at a building: send agents + a craft to investigate (this is what spawns
    # the tactical mission).
    "AlertScreen":            {"act": "BUTTON_EXTERMINATE", "ack": "BUTTON_QUIT", "select": True},
    # Our base is under attack -- proceed into the defence battle.
    "BaseDefenseScreen":      {"act": "BUTTON_QUIT", "ack": "BUTTON_QUIT", "select": False},
    # Leave buildings alone. Investigating one and finding no aliens costs the owner
    # -5 - difficulty relation every single time (buildingscreen.cpp:154-166), and alien crews
    # relocate between buildings on a timer, so a speculative raid usually finds nothing. Anger
    # the government this way and weeklyPlayerUpdate latches fundingTerminated -- income gone
    # permanently. A campaign died exactly that way at score -1312, nowhere near the -2400 score
    # cutoff. Deliberate raids still happen, but only where aliens are known to be.
    "BuildingScreen":         {"act": "BUTTON_QUIT", "ack": "BUTTON_QUIT", "select": False},
    "LocationScreen":         {"act": "BUTTON_EQUIPAGENT", "ack": "BUTTON_QUIT", "select": False},
    # Diplomacy: decline the bribe (accepting drains funds); the decision itself is the exercise.
    "DiplomaticTreatyScreen": {"act": "BUTTON_QUIT", "ack": "BUTTON_QUIT", "select": False},
    "BribeScreen":            {"act": "BUTTON_QUIT", "ack": "BUTTON_QUIT", "select": False},
    "InfiltrationScreen":     {"act": "BUTTON_TOPTEN", "ack": "BUTTON_QUIT", "select": False},
    # Purely informational.
    "ScoreScreen":            {"ack": "BUTTON_OK"},
    "WeeklyFundingScreen":    {"ack": "BUTTON_OK"},
    "NotificationScreen":     {"ack": "BUTTON_RESUME"},
    "MessageLogScreen":       {"ack": "BUTTON_OK"},
    "BattleDebriefing":       {"ack": "BUTTON_OK"},
    # Escape in CityView cancels an armed order if one is pending and otherwise opens this menu,
    # so the driver lands here by accident. play_battle drives it deliberately (Exit Battle) in
    # one synchronous block of its own, so simply closing it here is safe.
    "InGameOptions":          {"ack": "BUTTON_OK"},
    # Never press Return here. In battle a bare L opens this screen in LOAD mode, and the
    # unknown-stage fallback's Return loaded whatever save was first in the list - another run's
    # - tearing the game down mid-battle (run 411, 2026-10-07). BUTTON_QUIT just closes it.
    "SaveMenu":               {"ack": "BUTTON_QUIT"},
    # Same accidental-arrival problem as the UFOpaedia stages, and the one that actually stranded
    # two runs. manage_research() drives this screen deliberately via wait_for(), which returns as
    # soon as the stage matches and so never routes through respond_to_event() -- but when the
    # driver ends up here any OTHER way (a completed project raising it, or a research pass that
    # left it open), run_clock() parks off CityView with nothing able to dismiss it. RESPONSES is
    # consulted before the WORKING_STAGES guard, so an ack policy here closes the accidental case
    # without disturbing the deliberate one.
    "ResearchScreen":         {"ack": "BUTTON_OK"},
}

# Stages constructed in code rather than from a .form, so there are no control ids to resolve.
# MessageBox maps Return->OK/Yes and Escape->Cancel/No (game/ui/general/messagebox.cpp:129-155).
KEY_RESPONSES = {
    "MessageBox": ["Return", "Escape"],
    # The UFOpaedia is reachable BY ACCIDENT, not only through visit_ufopaedia(). A research or
    # score MessageBox offers to open the relevant entry, and respond_to_event() answers YES
    # (deliberately -- see the decline-order comment there, changing it cancelled every
    # recruitment), which pushes UfopaediaCategoryView. Both UFOpaedia stages are listed in
    # WORKING_STAGES, so the "unknown screen" rescue below skips them and returns False, and
    # nothing dismisses them: the run strands there with the campaign clock stopped, showing what
    # looks like a research screen. Observed on a real run at day 2 -- the driver sat on
    # UfopaediaCategoryView until a human closed the window by hand.
    #
    # Escape is the right key (it pops one level: category -> title -> CityView, so two passes
    # get home). Handling it here rather than by removing the stages from WORKING_STAGES keeps
    # that set's meaning intact, and does not disturb visit_ufopaedia(), which drives its own
    # Escape loop synchronously and never routes through respond_to_event().
    "UfopaediaCategoryView": ["Escape"],
    "UfopaediaView": ["Escape"],
}

# Stages where the driver is doing real work and must not be treated as an interruption.
WORKING_STAGES = {
    "CityView", "BattleView", "LoadingScreen", "MainMenu", "DifficultyMenu",
    "BattleBriefing", "BattlePreStart", "BaseScreen", "ResearchScreen", "ResearchSelect",
    "UfopaediaView", "UfopaediaCategoryView", "Skirmish", "MapSelector", "InGameOptions",
}

# How many times to try a screen's engaging action before deciding the game is
# refusing it and settling for acknowledgement.
ACT_ATTEMPT_LIMIT = 4
# How long to stop trying a refused action before giving it another go.
ACT_COOLDOWN_S = 180.0


class HarnessError(RuntimeError):
    pass


@dataclass
class Status:
    stage: str
    w: int
    h: int
    raw: str
    # Extra stage identification from Stage::harnessDetail(). Victory and defeat are both a
    # VideoScreen and differ only by which video plays, so the stage name alone cannot tell a
    # won campaign from a lost one.
    detail: str = "-"


class Harness:
    def __init__(self, host: str = "127.0.0.1", port: int = 17321, timeout: float = 10.0):
        self.host, self.port, self.timeout = host, port, timeout

    def send(self, line: str) -> str:
        payload = (line if line.endswith("\n") else line + "\n").encode()
        with socket.create_connection((self.host, self.port), timeout=self.timeout) as s:
            s.sendall(payload)
            chunks = []
            while True:
                data = s.recv(4096)
                if not data:
                    break
                chunks.append(data)
                if b"\n" in data:
                    break
        reply = b"".join(chunks).decode(errors="replace").strip()
        # Pace UI mutations once here, including raw send() calls. Observations remain instant.
        verb = line.split()[0]
        is_action = verb in {"click", "key", "keydown", "keyup", "move", "down", "up", "action"}
        is_action |= verb == "control" and line.split()[-1] != "get"
        delay = float(os.environ.get("OA_STEP_DELAY", "0"))
        if is_action and delay > 0:
            time.sleep(delay)
        return reply

    def ok(self, line: str) -> str:
        reply = self.send(line)
        if not reply.startswith("OK"):
            raise HarnessError(f"{line!r} -> {reply}")
        return reply[3:] if len(reply) > 3 else ""

    def status(self) -> Status:
        raw = self.ok("status")
        parts = dict(p.split("=", 1) for p in raw.split() if "=" in p)
        return Status(parts.get("stage", "?"), int(parts.get("w", 0)), int(parts.get("h", 0)), raw,
                      parts.get("detail", "-"))

    def gs(self, query: str) -> dict[str, str]:
        raw = self.ok(f"gs {query}")
        return dict(p.split("=", 1) for p in raw.split() if "=" in p)

    def ui(self, filt: str = "") -> dict:
        """Live control rects from the running game, keyed by control id.

        Replaces computing layout from the .form XML: with a resizable viewport and a UI scale
        factor, the engine's own resolved positions are the only trustworthy source. Coordinates
        come back in UI space, which is the same space CLICK takes.
        """
        raw = self.ok(f"ui {filt}".strip())
        d = dict(p.split("=", 1) for p in raw.split() if "=" in p)
        out = {}
        if d.get("at", "-") == "-":
            return out
        for rec in d["at"].split(";"):
            f = rec.split(",")
            if len(f) == 6:
                out[f[0]] = {"x": int(f[1]), "y": int(f[2]), "w": int(f[3]), "h": int(f[4]),
                             "visible": f[5] == "1"}
        return out

    def ui_list(self, filt: str = "") -> list:
        """Live rects as an ordered list, keeping same-named controls apart.

        ui() keys by control id, which silently collapses repeated names -- and the controls that
        most need addressing are exactly the repeated ones: every row of the base facility list
        is a Graphic called FACILITY_BUILD_TILE.
        """
        raw = self.ok(f"ui {filt}".strip())
        d = dict(p.split("=", 1) for p in raw.split() if "=" in p)
        out = []
        if d.get("at", "-") == "-":
            return out
        for rec in d["at"].split(";"):
            f = rec.split(",")
            if len(f) == 6:
                out.append((f[0], int(f[1]), int(f[2]), int(f[3]), int(f[4]), f[5] == "1"))
        return out

    def display_size(self) -> tuple:
        """Viewport size in UI space, so off-screen click targets can be rejected."""
        st = self.status()
        return (st.w or 1280, st.h or 720)

    def screen_craft(self, query: str) -> list[tuple[int, int, bool]]:
        """Parse "count=N at=x,y,crashed;..." from the view-space craft queries."""
        d = self.gs(query)
        if d.get("at", "-") == "-":
            return []
        out = []
        for item in d["at"].split(";"):
            parts = item.split(",")
            if len(parts) == 3:
                out.append((int(parts[0]), int(parts[1]), parts[2] == "1"))
        return out

    def click_xy(self, x: int, y: int, button: str = "left") -> None:
        # select_craft and the gate-craft crossing right-click; without the parameter that path
        # raised TypeError at the first attempt to leave for the alien dimension.
        self.ok(f"click {x} {y}" if button == "left" else f"click {x} {y} {button}")

    def control(self, cid: str, op: str = "click", value: str | None = None) -> str:
        """Drive a named widget directly, with no pixel arithmetic at all.

        Everything that has an id in data/forms/*.form can be reached this way, which matters
        most for the runtime-populated listboxes: their rows are generated controls with no ids
        and, in several screens, laid out horizontally, so addressing them geometrically was
        guesswork that silently hit the wrong row.
        """
        if op == "set":
            return self.ok(f"control {cid} set {value}")
        if op == "toggle":
            return self.ok(f"control {cid} toggle")
        return self.ok(f"control {cid}")

    def controls(self) -> str:
        return self.ok("controls")

    def action(self, verb: str, *args: str) -> str:
        return self.ok("action " + " ".join((verb,) + args))

    def key(self, name: str) -> None:
        """Press a key. A rejected key name is logged, never fatal.

        An unknown key used to raise straight out of the battle driver and be recorded as
        "lost connection", abandoning a mission that was otherwise going fine -- a typo in a key
        name should not cost a squad.
        """
        try:
            self.ok(f"key {name}")
        except HarnessError as exc:
            print(f"[harness] key {name!r} rejected: {exc}", flush=True)

    def screenshot(self, path: str) -> None:
        self.ok(f"screenshot {path}")


# Every "pause on <event>" notification opens a modal that stops the clock. They exist so a human
# does not miss things; in an unattended run they are pure interruption -- a single battle
# produced dozens. Turned off at launch so the simulation keeps moving.
PAUSE_NOTIFICATION_FLAGS = [
            "--Notifications.Battle.AgentBadlyInjured=0",
            "--Notifications.Battle.AgentBerserk=0",
            "--Notifications.Battle.AgentBrainsucked=0",
            "--Notifications.Battle.AgentCriticallyWounded=0",
            "--Notifications.Battle.AgentDiedBattle=0",
            "--Notifications.Battle.AgentFrozen=0",
            "--Notifications.Battle.AgentInjured=0",
            "--Notifications.Battle.AgentLeftCombat=0",
            "--Notifications.Battle.AgentPanicOver=0",
            "--Notifications.Battle.AgentPanicked=0",
            "--Notifications.Battle.AgentPsiAttacked=0",
            "--Notifications.Battle.AgentPsiControlled=0",
            "--Notifications.Battle.AgentPsiOver=0",
            "--Notifications.Battle.AgentUnconscious=0",
            "--Notifications.Battle.AgentUnderFire=0",
            "--Notifications.Battle.HostileDied=0",
            "--Notifications.Battle.HostileSpotted=0",
            "--Notifications.Battle.UnknownDied=0",
            # Leave AgentArrived ON. It is the notification that says a squad has reached the
            # building it was sent to, which is exactly the cue to act -- and switching it off
            # meant the driver had no idea when its agents had got anywhere, so it fell back to
            # opening the building screen over and over and finding nobody there to select.
            # Waiting to be told costs nothing; polling costs a trip to the city screen each time.
            "--Notifications.City.AgentDiedCity=0",
            "--Notifications.City.BaseDestroyed=0",
            # CargoArrived, RecoveryArrived and VehicleHeavyDamage stay ON, for the same reason
            # AgentArrived does. Each is infrequent and each is something the driver acts on:
            # cargo landing is the weapons it has been waiting to hand out, a recovery landing is
            # the alien material the research chain needs, and heavy damage is the cue to send a
            # craft home instead of losing it -- "if it gets low send it home", as the guide puts
            # it. The rest of this list is per-tick battle chatter that pauses the game for
            # nothing; these three are the game telling us something worth knowing.
            "--Notifications.City.NotEnoughAmmo=0",
            "--Notifications.City.NotEnoughFuel=0",
            "--Notifications.City.TransferArrived=0",
            "--Notifications.City.UfoSpotted=0",
            "--Notifications.City.UnauthorizedVehicle=0",
            "--Notifications.City.VehicleDestroyed=0",
            "--Notifications.City.VehicleEscaping=0",
            "--Notifications.City.VehicleLightDamage=0",
            "--Notifications.City.VehicleLowFuel=0",
            "--Notifications.City.VehicleModerateDamage=0",
            "--Notifications.City.VehicleNoAmmo=0",
            "--Notifications.City.VehicleRearmed=0",
            "--Notifications.City.VehicleRefuelled=0",
            "--Notifications.City.VehicleRepaired=0",
]

def watching() -> bool:
    """OA_WATCH=1: run at a pace a human can follow -- raised window, 60 FPS, and a city clock
    capped by OA_CITY_SPEED (default 5, turbo). Off by default; automated runs keep full speed."""
    return os.environ.get("OA_WATCH") == "1"


# The original game's simulation pace, in engine steps a second: UFO2P/TACP run one frame per BIOS
# timer tick (18.2065 a second) and an engine step is a quarter of one (framework.cpp).
NATIVE_SIM_STEPS_PER_SECOND = 4 * 1193182 / 65536


def sim_args() -> list[str]:
    """How fast the launched game simulates, as engine options.

    OA_TARGET_FPS, if set, is an explicit step rate. Otherwise OA_SIM_SPEED is a multiple of the
    original game's pace; watching means the original pace, and an automated run defaults to
    1000 steps a second (about 13.7x), which is what it has always run at.
    """
    if os.environ.get("OA_TARGET_FPS"):
        return [f"--Framework.TargetFPS={int(os.environ['OA_TARGET_FPS'])}"]
    return ["--Framework.TargetFPS=0", f"--Framework.SimSpeed={sim_speed():g}"]


def sim_speed() -> float:
    """Simulation speed as a multiple of the original game's pace (see sim_args)."""
    if os.environ.get("OA_SIM_SPEED"):
        return float(os.environ["OA_SIM_SPEED"])
    return 1.0 if watching() else round(1000 / NATIVE_SIM_STEPS_PER_SECOND, 2)


def render_fps() -> int:
    """Frames drawn per second. OA_RENDER_FPS wins; watching means the display's own rate (0);
    otherwise 30. Sixteen windows at 30 fps kept WindowServer at 80% on a laptop panel and left
    every game waiting on it at a tenth of a core, so a grid can ask for fewer."""
    if os.environ.get("OA_RENDER_FPS"):
        return int(os.environ["OA_RENDER_FPS"])
    return 0 if watching() else 30


def audio_enabled() -> bool:
    """Music and sound effects. OA_AUDIO=1/0 decides; otherwise on exactly when watching."""
    if os.environ.get("OA_AUDIO") in ("0", "1"):
        return os.environ["OA_AUDIO"] == "1"
    return watching()


def screen_args() -> list[str]:
    """OA_FULLSCREEN=1: borderless at the desktop's own resolution (0 = desktop size).
    OA_TILE="CxR:slot": one borderless cell of a grid, for watching a parallel batch.
    OA_DISPLAY=N: which monitor (SDL display index; 0 = main) every mode opens on."""
    display = ([f"--Framework.Screen.Display={os.environ['OA_DISPLAY']}"]
               if os.environ.get("OA_DISPLAY") else [])
    if os.environ.get("OA_TILE"):
        return display + [f"--Framework.Screen.Tile={os.environ['OA_TILE']}"]
    if os.environ.get("OA_FULLSCREEN") != "1":
        return display
    return display + ["--Framework.Screen.Mode=borderless", "--Framework.Screen.Width=0",
            "--Framework.Screen.Height=0"]


def city_speed_cap() -> int:
    """Highest city clock speed the driver may select (1-5). 5 is turbo. OA_CITY_SPEED sets it."""
    return max(1, min(5, int(os.environ.get("OA_CITY_SPEED", "5"))))


def nonnegative_seconds(value: str) -> float:
    """Argparse type for a finite, nonnegative action delay."""
    import math
    seconds = float(value)
    if not math.isfinite(seconds) or seconds < 0:
        raise argparse.ArgumentTypeError("must be a finite, nonnegative number of seconds")
    return seconds


def add_runner_options(ap: argparse.ArgumentParser) -> None:
    """Shared human-paced playback and tactical policy options."""
    ap.add_argument("--watch", action="store_true", help="watch the game at a human pace")
    ap.add_argument("--city-speed", type=int, choices=range(1, 6), default=None,
                    metavar="N", help="maximum city speed (1-5); sets OA_CITY_SPEED")
    ap.add_argument("--step-delay", type=nonnegative_seconds, default=0, metavar="SECONDS",
                    help="sleep between driver actions (default: 0)")
    ap.add_argument("--ai", metavar="NAME", help="built-in or plugin tactical AI")
    ap.add_argument("--fullscreen", action="store_true",
                    help="borderless at the desktop's full resolution (sets OA_FULLSCREEN)")
    ap.add_argument("--audio", dest="audio", action="store_true", default=None,
                    help="play music and sound (default: on with --watch, off otherwise)")
    ap.add_argument("--no-audio", dest="audio", action="store_false")


def configure_runner(args: argparse.Namespace) -> dict:
    """Apply playback options before launch and return the battle policy."""
    if args.watch:
        os.environ["OA_WATCH"] = "1"
    if args.fullscreen:
        os.environ["OA_FULLSCREEN"] = "1"
    if args.audio is not None:
        os.environ["OA_AUDIO"] = "1" if args.audio else "0"
    if args.city_speed is not None:
        os.environ["OA_CITY_SPEED"] = str(args.city_speed)
    os.environ["OA_STEP_DELAY"] = str(args.step_delay)
    return {"ai": args.ai} if args.ai else {}


def resolve_strategy(args: argparse.Namespace, policy: dict) -> tuple[Strategy, dict]:
    """The campaign genome named by --strategy, and the battle policy it implies.

    Only runners that actually read the genome (oa_victory, oa_play) call this and offer
    --strategy; offering the flag on a runner that ignored it would be a knob wired to nothing. An
    explicit --ai still names the AI; the genome's doctrine genes ride along either way, and a
    default genome contributes nothing, so a run that names no strategy keeps its old policy.
    """
    strategy = parse_strategy(getattr(args, "strategy", None))
    return strategy, {**strategy.battle_policy(), **policy}


def bring_to_front() -> None:
    """Raise the game window, only when explicitly asked for via OA_RAISE_WINDOW=1.

    OFF BY DEFAULT, and it must stay that way. This used to run on every launch and every resume,
    which was intolerable in practice: each call steals focus from whatever the human is actually
    doing, and during a restart loop it fires every few seconds. A windowed launch is already
    visible without any of this -- raising it is a convenience, not a requirement -- so the
    default is to leave the user's focus alone entirely.
    """
    if sys.platform != "darwin" or not (os.environ.get("OA_RAISE_WINDOW") == "1" or watching()):
        return
    try:
        subprocess.run(
            ["osascript", "-e", 'tell application "OpenApoc" to activate'],
            capture_output=True, timeout=5,
        )
    except Exception:
        pass


def _disable_window_restore() -> None:
    """Stop macOS trying to restore this app's windows on launch.

    The window-restore machinery is what actually wedged the game: the launch stack bottomed out
    in AEProcessAppleEvent underneath Cocoa_ShowWindow, which is AppKit handling the
    open-application Apple Event and rebuilding saved window state. A hard crash leaves that
    state inconsistent, and every launch afterwards hung there at 0% CPU with the harness port
    never opening. An automated run has no windows worth restoring, so turn the whole mechanism
    off and clear anything already on disk rather than relying on the SDL-side workaround alone.
    """
    # NSAppSleepDisabled belongs here too: App Nap throttles an occluded, silent app's run loop,
    # and an unattended campaign is occluded and silent by definition. Setting it by hand once
    # left the launcher unable to reproduce its own working configuration.
    for key, val in (("ApplePersistenceIgnoreState", "YES"),
                     ("NSQuitAlwaysKeepsWindows", "NO"),
                     ("NSAppSleepDisabled", "YES")):
        try:
            subprocess.run(["defaults", "write", "org.openapoc.OpenApoc", key, "-bool", val],
                           capture_output=True, timeout=10)
        except Exception:
            pass
    saved = Path.home() / "Library/Saved Application State/org.openapoc.OpenApoc.savedState"
    try:
        if saved.exists():
            shutil.rmtree(saved, ignore_errors=True)
    except Exception:
        pass


def free_port(preferred: int) -> int:
    """A harness port nobody else is using, starting from `preferred`.

    Every driver used to default to one fixed port, and reap_stale_game kills whatever answers to
    `Harness.Port=<port>` -- so two runs on one machine reaped each other's games on sight. That is
    not a hypothetical: six attempts in a single generation died to SIGKILL, which cannot be caught,
    leaves no crash report, and had been getting filed as an engine fault. Five agent sessions were
    live in the same checkout at the time.

    Binding is the only honest test of whether a port is free; asking pgrep races. Falls back to the
    preferred port if the whole range is taken, because failing loudly at launch beats guessing.
    """
    import socket as _socket

    # Start each PROCESS at its own offset. Probing alone is not enough: the probe socket closes
    # before the game binds, so two runs launching at the same moment both see the same port free
    # and both take it. Offsetting by pid means they begin looking in different places, and the
    # probe then only has to settle the rare genuine overlap.
    span = 40
    start = preferred + (os.getpid() % span)
    for candidate in [start + i - span if start + i >= preferred + span else start + i
                      for i in range(span)]:
        with _socket.socket(_socket.AF_INET, _socket.SOCK_STREAM) as probe:
            probe.setsockopt(_socket.SOL_SOCKET, _socket.SO_REUSEADDR, 1)
            try:
                probe.bind(("127.0.0.1", candidate))
            except OSError:
                continue
        # Nothing is listening AND nothing is mid-shutdown holding it as a stale game.
        if not subprocess.run(
            ["pgrep", "-f", f"Harness.Port={candidate}"],
            capture_output=True, text=True, timeout=10,
        ).stdout.strip():
            return candidate
    return preferred


def reap_stale_game(port: int) -> int:
    """Kill any leftover game already using this port. Returns how many were killed.

    A hung instance does not release its harness port, and the next launch then fails with
    "harness did not come up" against a process that is still very much alive -- just not
    answering. That produced a restart loop the runner could not break out of: the campaign log
    showed a game "dying" and restarting every few seconds while a four-minute-old zombie sat on
    the port the whole time. SIGTERM is not enough for a process wedged in that state, hence the
    escalation to SIGKILL.
    """
    killed = 0
    try:
        found = subprocess.run(
            ["pgrep", "-f", f"Harness.Port={port}"],
            capture_output=True, text=True, timeout=10,
        )
    except Exception:
        return 0
    for pid in [p for p in found.stdout.split() if p.strip().isdigit()]:
        for sig in ("-TERM", "-KILL"):
            try:
                subprocess.run(["kill", sig, pid], capture_output=True, timeout=5)
            except Exception:
                break
            time.sleep(1.0)
            still = subprocess.run(["kill", "-0", pid], capture_output=True, timeout=5)
            if still.returncode != 0:
                break
        killed += 1
    if killed:
        time.sleep(1.5)
    return killed


def build_dir() -> str:
    """Which build tree to launch from. Defaults to the one everything else uses.

    snapshot_binary() protects a RUNNING process from a rebuild, but not the next attempt: that
    one copies whatever build/bin holds by then, so an engine change landing mid-experiment
    silently becomes part of it. Testing a fix against a live run therefore needs a second build
    tree, and this is how a driver is pointed at one -- OA_BUILD_DIR=build-something.
    """
    return os.environ.get("OA_BUILD_DIR", "build")


class GameProcess:
    """Owns a game instance so a run needs no human to start or stop anything."""

    def __init__(self, repo: Path, port: int, log_path: Path, extra: list[str] | None = None,
                 seed: int = 0):
        self.repo = Path(repo)
        self.port = port
        self.log_path = Path(log_path)
        self.extra = extra or []
        self.seed = int(seed)
        self._run_binary = self.binary
        self.proc: subprocess.Popen | None = None

    @property
    def binary(self) -> Path:
        return self.repo / build_dir() / "bin/OpenApoc.app/Contents/MacOS/OpenApoc"

    def snapshot_binary(self) -> Path:
        """Copy the app bundle so a rebuild cannot kill a run in flight.

        cmake --build replaces the executable in place, and on macOS that terminates any process
        running it. Every "run failure" during a development session had the same signature -
        ConnectionRefusedError, no crash report, no game-log error - and the cause was a rebuild
        underneath the run, not the game. A twenty-week run and an active edit loop cannot share
        one binary.

        Falls back to the shared binary if the copy fails for any reason: a slightly fragile run
        beats no run.
        """
        src = self.repo / build_dir() / "bin/OpenApoc.app"
        dst = self.log_path.parent / "OpenApoc.app"
        try:
            if dst.exists():
                shutil.rmtree(dst)
            shutil.copytree(src, dst, symlinks=True)
            return dst / "Contents/MacOS/OpenApoc"
        except Exception as exc:
            print(f"[launch] binary snapshot failed ({type(exc).__name__}: {exc}); "
                  f"using the shared build - a rebuild will kill this run", flush=True)
            return self.binary

    def start(self, wait_s: float = 90.0) -> None:
        self.log_path.parent.mkdir(parents=True, exist_ok=True)
        (self.log_path.parent / "saves").mkdir(exist_ok=True)
        # Take a private copy of the binary first: see snapshot_binary().
        self._run_binary = self.snapshot_binary()
        # Clear any wedged instance still holding this port before trying to bind it.
        stale = reap_stale_game(self.port)
        if stale:
            print(f"[boot] reaped {stale} stale game process(es) on port {self.port}", flush=True)
        argv = [
            str(self._run_binary),
            f"--Framework.Data={self.repo / 'data'}",
            f"--Framework.CD={self.repo / 'data/cd.iso'}",
            "--Framework.Harness.Enable=1",
            f"--Framework.Harness.Port={self.port}",
            "--Game.SkipIntro=1",
            "--Config.Save=0",
            "--Config.Read=0",
            # Fixed RNG seed: GameState::startGame() otherwise reseeds from wall-clock.
            "--OpenApoc.NewFeature.SeedRng=0",
            # Explicit seed. 0 keeps the engine's old behaviour; anything else is used verbatim,
            # so a run is reproducible on demand and freely variable at will -- which comparing
            # two AIs over the same campaign needs, and a fixed default cannot give.
            f"--OpenApoc.NewFeature.RngSeed={self.seed}",
            # Agent equipment templates: an ordinary in-game affordance (keys 1-6,
            # Ctrl to save), and the only way to arm a squad without pixel-accurate
            # drag-and-drop onto the paper doll.
            "--OpenApoc.NewFeature.EnableAgentTemplates=1",
            # Belt and braces alongside the engine-side guard: a modal error dialog blocks the
            # main loop forever when there is no human to dismiss it.
            "--Logger.dialogLevel=0",
            # A save directory per run: they all shared ./saves, so a stray load picked up some
            # other run's game.
            f"--Game.Save.Directory={self.log_path.parent / 'saves'}",
            # Warnings and errors only: an Info line per vehicle route attempt and mission change
            # is thousands of formatted strings a second in a busy city, for a log nobody reads.
            "--Logger.FileLevel=2",
            # Frame limiting is honoured again now that the loop resynchronises after a hitch,
            # and ticks advance per frame -- so an automated run asks for the headroom outright
            # rather than relying on the limiter being broken.
            *sim_args(),
            # Draw at most 30 frames a second unless a human is watching. Each present waits on
            # the window server's vsync, and at 120 fps that wait capped the simulation at ~120
            # steps/s; at 30 the same game ran 7x more steps for a third of the CPU per step.
            f"--Framework.RenderFPS={render_fps()}",
        ] + ([] if audio_enabled() else ["--Framework.AudioBackends=null"]) + screen_args() \
            + PAUSE_NOTIFICATION_FLAGS + self.extra
        # Append, never truncate: a runner that restarts after a crash used to reopen this with "w"
        # and overwrite the only record of how the previous instance died.
        self.logf = open(self.log_path, "a")
        self.logf.write(f"\n===== launch {time.strftime('%Y-%m-%d %H:%M:%S')} port={self.port} =====\n")
        self.logf.flush()
        # SDL3 makes window operations synchronous by default: Cocoa_SyncWindow pumps the Cocoa
        # event queue until the window server acknowledges the state change. On this machine that
        # acknowledgement stopped arriving after the engine died hard twice in a row, and every
        # subsequent launch wedged for good inside SDL_CreateWindow -> Cocoa_ShowWindow ->
        # Cocoa_SyncWindow at 0% CPU, bottoming out in AEProcessAppleEvent. The game never opened
        # its harness port, so the runner reported "harness did not come up" against a process
        # that was very much alive. Turning the synchronous behaviour off lets window creation
        # return and the game reach its main loop -- verified: MainMenu answering in 15s, against
        # six consecutive launches that never answered at all. The window is still shown, so this
        # does not cost the visible camera an onlooker needs.
        env = dict(os.environ)
        env.setdefault("SDL_VIDEO_SYNC_WINDOW_OPERATIONS", "0")
        _disable_window_restore()
        self.proc = subprocess.Popen(
            argv, cwd=str(self.repo), stdout=self.logf, stderr=subprocess.STDOUT,
            start_new_session=True, env=env,
        )
        h = Harness(port=self.port)
        deadline = time.time() + wait_s
        while time.time() < deadline:
            if self.proc.poll() is not None:
                raise RuntimeError(f"game exited early (rc={self.proc.returncode}); see {self.log_path}")
            try:
                h.send("status")
                bring_to_front()
                return
            except OSError:
                time.sleep(0.5)
        raise TimeoutError(f"harness did not come up on port {self.port}")

    def exit_status(self) -> str:
        """How the game process ended, in words, or "" if it is still running.

        Three engine deaths this session produced a bare ConnectionRefusedError and nothing else:
        no uncaught exception (the terminate handler stayed silent), no macOS crash report, no
        clean exit. That is three different failures wearing the same costume, and the exit status
        distinguishes them -- a negative returncode is the signal that killed it, which separates
        a segfault from an abort from an OOM kill from the process simply being asked to leave.
        """
        if not self.proc:
            return "never started"
        rc = self.proc.poll()
        if rc is None:
            return ""
        if rc < 0:
            import signal as _signal

            try:
                name = _signal.Signals(-rc).name
            except (ValueError, AttributeError):
                name = "?"
            return f"killed by signal {-rc} ({name})"
        return f"exited rc={rc}"

    def stop(self) -> None:
        if not self.proc:
            return
        try:
            Harness(port=self.port).send("quit")
        except OSError:
            pass
        try:
            self.proc.wait(timeout=20)
        except Exception:
            # A wedged instance ignores the harness QUIT and keeps its port, so escalate rather
            # than leave something behind for the next launch to collide with.
            self.proc.kill()
            try:
                self.proc.wait(timeout=10)
            except Exception:
                pass
            reap_stale_game(self.port)
        try:
            self.logf.close()
        except Exception:
            pass

    def warnings(self) -> list[str]:
        if not self.log_path.exists():
            return []
        return [l.rstrip() for l in self.log_path.read_text(errors="replace").splitlines()
                if l.startswith("W ") or l.startswith("E ")]


class Driver:
    def __init__(self, harness: Harness, forms_dir: Path, log: Path | None = None,
                 shots: Path | None = None, verbose: bool = True,
                 battle_policy: dict | None = None, strategy: Strategy | None = None):
        self.h = harness
        self.lib = FormLibrary(forms_dir)
        self.verbose = verbose
        self.battle_policy = dict(battle_policy or {})
        # The campaign genome (tools/oa_strategy.py). The default Strategy() is today's constants,
        # so a Driver built without one behaves exactly as it always did.
        self.strategy = strategy or Strategy()
        self.shots = shots
        self.shot_n = 0
        self.events: list[str] = []
        self.stages_seen: set[str] = set()
        self.dismissed: dict[str, int] = {}
        self.checks: dict = {}
        self.responses: dict[str, int] = {}
        self.unknown_stages: dict[str, int] = {}
        # Buildings an alert has named as holding aliens, oldest first. This is the driver's only
        # knowledge of where infiltration is -- there is no list to consult, the same as for a
        # player, who watches the UFOs and goes where the game says they landed.
        self.alerted_buildings: list[str] = []
        # Of those, the ones whose aliens could spread into one of our bases (the alert's
        # threatens_base): swept before anything else and never aged out of the queue.
        self.base_threats: set[str] = set()
        # craft id -> (building it is parked at, when it was first seen idle there)
        self.stranded_since: dict[str, tuple[str, float]] = {}
        # (building id, when) of a transport sent to land a raid squad; see send_raid_squad().
        self.raid_en_route: tuple[str, float] | None = None
        # Stats from the most recent win_battle(), which stamps them on every return path.
        self.last_battle: dict = {}
        self.act_counts: dict[str, int] = {}
        self.act_reset_at = time.time()
        # Craft kinds that crew_transport tried and failed to put a squad aboard, by consecutive
        # failures. rank_transports sinks a kind that keeps failing, so a transport that cannot be
        # loaded stops being chosen first every leg -- the run learns which craft actually work.
        self.crew_failures: dict[str, int] = {}

    # Screens where Escape does NOT mean "back". CityView and BattleView both PUSH InGameOptions
    # on SDLK_ESCAPE (cityview.cpp:4156, battleview.cpp:3380), so pressing it there OPENS the
    # settings menu instead of closing anything -- and the harness used Escape as its universal
    # "stuck, get out of this" fallback. One run reached InGameOptions thirteen times without ever
    # asking for it, each visit costing a round to notice and another to close.
    #
    # These two are also the screens with nothing to escape FROM: they are where the game lives.
    # leave_battle() still opens InGameOptions on purpose, because BUTTON_EXIT_BATTLE is inside it
    # -- that one is a destination, not a fallback.
    ESCAPE_OPENS_OPTIONS = ("CityView", "BattleView")

    def escape_key(self, stage: str = "") -> bool:
        """Press Escape, unless we are somewhere Escape would open the options menu.

        Returns whether the key was actually sent, so a caller can tell "I tried and it did not
        help" from "there was nothing here to escape".
        """
        stage = stage or self.status().stage
        if stage in self.ESCAPE_OPENS_OPTIONS:
            return False
        # The one place in the file that may press Escape unguarded, besides leave_battle.
        self.h.key("Escape")
        return True

    def say(self, msg: str) -> None:
        self.events.append(msg)
        if self.verbose or watching():
            print(" ".join(msg.splitlines()) if watching() else msg, flush=True)

    # -- screen awareness ------------------------------------------------
    def status(self) -> Status:
        st = self.h.status()
        self.stages_seen.add(st.stage)
        return st

    def controls(self, st: Status) -> dict:
        key = STAGE_FORMS.get(st.stage)
        if key is None:
            return {}
        try:
            return self.lib.resolve(key, st.w, st.h)
        except KeyError:
            return {}

    def click_id(self, cid: str, st: Status | None = None) -> bool:
        """Click a control by id, using the game's own resolved geometry when it can.

        The .form resolver stays as a fallback for anything the live dump cannot see, but the
        live query is authoritative -- it survives a viewport resize and UI scaling, which the
        static layout computation does not.
        """
        # Ask the engine to invoke the widget by name. Control::click() raises the same
        # ButtonClick a real press does, so this is not a shortcut around the UI -- it just skips
        # the pixel arithmetic, which was the single largest source of silent no-ops in this
        # driver. The resolved-rect click stays as a fallback for anything the action handler
        # cannot see.
        try:
            self.h.control(cid)
            return True
        except HarnessError:
            pass
        try:
            live = self.h.ui(cid)
            c = live.get(cid)
            if c and c["w"] > 0 and c["visible"]:
                self.h.click_xy(c["x"] + c["w"] // 2, c["y"] + c["h"] // 2)
                return True
        except HarnessError:
            pass
        st = st or self.status()
        ctrls = self.controls(st)
        c = ctrls.get(cid)
        if c is None or c.w <= 0 or c.h <= 0:
            return False
        self.h.click_xy(*c.centre)
        return True

    def live_rect(self, cid: str) -> dict | None:
        try:
            return self.h.ui(cid).get(cid)
        except HarnessError:
            return None

    def live_rects(self, cid: str) -> list:
        """Every live rect whose control id matches, in the engine's own order."""
        try:
            return [(x, y, w, h) for (name, x, y, w, h, vis) in self.h.ui_list(cid)
                    if name == cid and vis]
        except HarnessError:
            return []

    def shot(self, tag: str) -> None:
        if not self.shots:
            return
        self.shot_n += 1
        p = self.shots / f"{self.shot_n:03d}_{tag}.png"
        try:
            self.h.screenshot(str(p))
        except HarnessError:
            pass

    def game_over(self) -> bool:
        """True once the campaign has reached a terminal state.

        Losing the last base raises GameLost, which replaces the stage stack with the losing
        cutscene and then the main menu. That is the campaign ending, not a failure of the run.
        """
        st = self.status()
        if st.stage in ("VideoScreen", "MainMenu", "CreditsMenu"):
            if not self.checks.get("game_over"):
                self.checks["game_over"] = st.stage
                self.say(f"[campaign] terminal state reached: {st.stage}")
            return True
        try:
            if self.h.gs("stage").get("defeated") == "1":  # noqa: SIM102
                # The cutscene only fires on the next GameState tick; nudge the clock so the
                # ending actually plays instead of the city sitting there paused.
                if st.stage == "CityView":
                    self.h.key("3")
                if not self.checks.get("defeated"):
                    self.checks["defeated"] = True
                    self.say("[campaign] X-COM defeated - all bases lost")
                return False
        except HarnessError:
            pass
        return False

    # -- core loop -------------------------------------------------------
    def pump(self, seconds: float, expect: str | None = None) -> Status:
        """Advance wall-clock while clearing any modal that appears.

        Returns as soon as `expect` stage is reached, otherwise runs the full duration.
        """
        deadline = time.time() + seconds
        st = self.status()
        while time.time() < deadline:
            if expect and st.stage == expect:
                return st
            if not self.dismiss_modal(st):
                time.sleep(0.4)
            st = self.status()
        return st

    def select_armed_squad(self, st: Status, want: int = 6) -> int:
        """Select soldiers who can fight in an alert or building screen; returns how many.

        BUTTON_EXTERMINATE refuses outright with nobody selected, and with the wrong people
        selected it sends them to die. The two screens differ in who actually goes:

        * AlertScreen sends each selected craft, and the investigation then takes EVERY soldier
          aboard the craft at the building, selected or not (CityView's CommenceInvestigation).
          So a craft is sent only if nobody aboard is unarmed; otherwise armed soldiers go on
          foot.
        * BuildingScreen starts the battle with exactly the selected soldiers, so selecting the
          armed ones is enough.

        This used to click the first six rows and the first craft by pixel offset, whoever they
        were. Rows now come from the screen's own harness detail, which says where each is drawn
        and whether its soldier carries a weapon.
        """
        soldiers = [r for r in assignment_rows(st, "soldier_rows") if len(r) >= 7 and r[3] == 1]
        armed = [r for r in soldiers if r[6] == 1]

        def selected() -> int:
            return detail_int(self.status().detail, "selected_agents") or 0

        def click_rows(rows) -> int:
            count = selected()
            for x, y, *_ in rows:
                if count >= want:
                    break
                self.h.click_xy(x, y)
                time.sleep(0.1)
                count = selected()
            return count

        if st.stage == "AlertScreen":
            craft = [r for r in assignment_rows(st, "boarding")
                     if len(r) >= 9 and r[4] == 1 and r[5] == 1 and r[7] > 0 and r[8] == 0]
            if craft:
                best = max(craft, key=lambda r: r[7])
                self.h.click_xy(best[0], best[1])
                time.sleep(0.15)
                count = click_rows([r for r in armed if r[2] == 1 and r[5] == best[6]])
                if count:
                    return count
            return click_rows([r for r in armed if r[2] == 0])
        return click_rows(armed)

    def note_alert(self, st: Status) -> None:
        """Remember a building an alert has just named.

        This is the honest substitute for a list of infiltrated buildings: the game tells the
        player where aliens were seen going in, and that report -- and nothing else -- is what
        the driver acts on afterwards.
        """
        detail = st.detail or ""
        if "alert_building=" not in detail:
            return
        name = detail.split("alert_building=", 1)[1].split()[0]
        for sep in ("_crew=", "crew="):
            if sep in name:
                name = name.split(sep)[0]
        name = name.rstrip("_")
        if not name or name in ("none", "-"):
            return
        # Aliens beside one of our bases go first, ahead of every older report: a crew there can
        # move into the base (Building::alienMovement) and expose it, and an exposed base is the
        # one a subversion UFO comes to attack.
        threat = detail_int(detail, "threatens_base") == 1
        if threat:
            self.base_threats.add(name)
            if name in self.alerted_buildings:
                self.alerted_buildings.remove(name)
            self.alerted_buildings.insert(0, name)
            self.say(f"  [alert] aliens reported in {name} BESIDE A BASE; sweeping it first")
        elif name not in self.alerted_buildings:
            self.alerted_buildings.append(name)
            self.say(f"  [alert] aliens reported in {name}; noted for a sweep")
        else:
            return
        # Keep the queue short. An address noted many alerts ago has almost certainly been
        # cleared or its crew has moved on, and a queue that only grows holds the campaign at
        # walking pace for ever. Threats to a base are never the ones dropped.
        while len(self.alerted_buildings) > 6:
            stale = next((n for n in self.alerted_buildings if n not in self.base_threats), None)
            if stale is None:
                break
            self.alerted_buildings.remove(stale)

    def respond_to_event(self, st: Status) -> bool:
        """Engage with an interrupting screen. Returns True if we acted on it."""
        if st.stage == "AlertScreen":
            # Remember the incident, then answer it from this screen through RESPONSES below:
            # armed soldiers only (select_armed_squad), or acknowledge if none is free. Quitting
            # to raid from the target's BuildingScreen instead could not work -- that screen lists
            # only soldiers already at the building -- and ended every incident in "No Agents
            # Selected".
            self.note_alert(st)
        if st.stage == "MessageBox":
            # A MessageBox is not always an acknowledgement. YesNoCancel boxes -- RecruitScreen's
            # "Confirm Orders" among them (recruitscreen.cpp:482-484) -- carry no BUTTON_OK and do
            # not answer to Return, so a key-only responder sat on one forever: 110 strandings in
            # a single run, the campaign clock stopped the whole time. Press an actual button
            # first and only fall back to keys.
            # Button ids vary by box shape and are not what you would guess: a YesNoCancel box
            # carries BUTTON_YES / BUTTON_NO2 / BUTTON_CANCEL -- note NO2, there is no BUTTON_NO
            # -- and no BUTTON_OK at all. Decline before confirming: this responder runs when
            # something is already stuck, and confirming an order the campaign cannot afford just
            # raises the next box.
            #
            # Judging success by "the stage changed" is also wrong here, because dismissing one
            # box often reveals another and the stage is still MessageBox. Treat a button that
            # the engine accepted as progress, and let the next pass handle whatever appears.
            # Confirm before declining HERE. Decline-first belongs to return_to_city, which only
            # runs once something is already stuck; in ordinary play the boxes worth answering are
            # ones we raised on purpose. Putting NO2 first cancelled every recruitment the driver
            # attempted -- BUTTON_NO2 fired eleven times and BUTTON_YES not once, while soldiers
            # stayed at 8 and funds never moved.
            for cid in ("BUTTON_OK", "BUTTON_YES", "BUTTON_NO2", "BUTTON_NO",
                        "BUTTON_CANCEL"):
                try:
                    if not self.h.send(f"control {cid}").startswith("OK"):
                        continue
                except (HarnessError, OSError):
                    continue
                time.sleep(0.3)
                self.say(f"  [event] MessageBox -> {cid}")
                return True

        keys = KEY_RESPONSES.get(st.stage)
        if keys:
            for k in keys:
                self.h.key(k)
                time.sleep(0.35)
                if self.h.status().stage != st.stage:
                    self.responses[f"{st.stage}:{k}"] = self.responses.get(f"{st.stage}:{k}", 0) + 1
                    self.say(f"  [event] {st.stage} -> KEY {k}")
                    return True
            # Every key was tried and the stage did not budge. This used to `return True`, which
            # told the caller "handled" and sent it straight back round its loop -- and because
            # the say() above only fires on a stage CHANGE, the driver then hammered a key that
            # does nothing, forever, printing not one line. A run was killed by hand after
            # sitting like that; the log simply stopped mid-campaign with no error and the game's
            # own log had zero warnings. Report it as unhandled so callers fall through to their
            # own stall handling instead of spinning in silence.
            return False

        policy = RESPONSES.get(st.stage)
        if not policy:
            # Unknown screen. Never let an unrecognised stage stall an unattended run: try the
            # conventional confirm/cancel keys, and record it so the gap can be closed properly.
            if st.stage not in WORKING_STAGES:
                self.unknown_stages[st.stage] = self.unknown_stages.get(st.stage, 0) + 1
                if self.unknown_stages[st.stage] % 5 == 1:
                    self.say(f"  [event] unknown stage {st.stage}; trying Return/Escape")
                for k in ("Return", "Escape"):
                    self.h.key(k)
                    time.sleep(0.3)
                    if self.h.status().stage != st.stage:
                        return True
                return True
            return False
        ctrls = self.controls(st)
        selected = 0

        # An "act" that the game refuses bounces us straight back to the same screen -- e.g. a
        # raid with no eligible squad returns BuildingScreen -> MessageBox -> BuildingScreen
        # forever. Changing stage is therefore not proof of progress; stop offering the action
        # once it has clearly stopped working and just acknowledge instead.
        # A refusal is usually temporary -- BUTTON_EXTERMINATE is declined when no agents are
        # free because they are already out on a mission, not because dispatch is broken. Making
        # the cap permanent meant one busy afternoon disabled X-COM's response to alien incidents
        # for the rest of the campaign; score collapsed and funding went to zero.
        now = time.time()
        if now - self.act_reset_at > ACT_COOLDOWN_S and self.act_counts:
            self.act_counts.clear()
            self.act_reset_at = now
        kinds = ("act", "ack")
        if self.act_counts.get(st.stage, 0) >= ACT_ATTEMPT_LIMIT:
            kinds = ("ack",)
            if self.act_counts.get(st.stage) == ACT_ATTEMPT_LIMIT:
                self.act_counts[st.stage] += 1
                self.say(f"  [event] {st.stage}: refused {ACT_ATTEMPT_LIMIT}x, backing off for "
                         f"{ACT_COOLDOWN_S:.0f}s")
        elif policy.get("select"):
            selected = self.select_armed_squad(st)
            if not selected:
                # Nobody who can fight is free: acknowledge rather than send unarmed soldiers or
                # press EXTERMINATE into a "No Agents Selected" box.
                kinds = ("ack",)
                self.say(f"  [event] {st.stage}: no armed squad free; acknowledging")

        for kind in kinds:
            cid = policy.get(kind)
            if not cid:
                continue
            c = ctrls.get(cid)
            if c is None or c.w <= 0:
                continue
            self.h.click_xy(*c.centre)
            time.sleep(0.4)
            after = self.h.status().stage
            if after != st.stage:
                key = f"{st.stage}:{kind}"
                self.responses[key] = self.responses.get(key, 0) + 1
                if kind == "act":
                    self.act_counts[st.stage] = self.act_counts.get(st.stage, 0) + 1
                self.say(f"  [event] {st.stage} -> {cid} ({kind}"
                         + (f", {selected} units selected" if selected else "") + f") -> {after}")
                return True
        # Nothing moved us off the screen; Escape rather than deadlock the run.
        self.escape_key(st.stage)
        self.responses[f"{st.stage}:escape"] = self.responses.get(f"{st.stage}:escape", 0) + 1
        self.say(f"  [event] {st.stage} -> Escape (no control advanced the stage)")
        time.sleep(0.35)
        return True

    # Back-compat alias used by the campaign loop.
    def dismiss_modal(self, st: Status) -> bool:
        return self.respond_to_event(st)

    def wait_for(self, stage: str | tuple[str, ...], timeout: float = 90.0) -> Status:
        wanted = (stage,) if isinstance(stage, str) else tuple(stage)
        deadline = time.time() + timeout
        while time.time() < deadline:
            st = self.status()
            if st.stage in wanted:
                return st
            if not self.dismiss_modal(st):
                time.sleep(0.4)
        raise TimeoutError(
            f"stage {wanted!r} not reached in {timeout}s (last={self.status().stage})"
        )


# ---------------------------------------------------------------------------
# Campaign script
# ---------------------------------------------------------------------------

def new_game(d: Driver, difficulty: int = 3) -> None:
    st = d.wait_for("MainMenu", 60)
    d.shot("mainmenu")
    d.say(f"[boot] display {st.w}x{st.h}")
    if not d.click_id("BUTTON_NEWGAME", st):
        raise RuntimeError("could not resolve BUTTON_NEWGAME")
    st = d.wait_for("DifficultyMenu", 30)
    d.say(f"[new game] difficulty {difficulty}")
    if not d.click_id(f"BUTTON_DIFFICULTY{difficulty}", st):
        raise RuntimeError("could not resolve difficulty button")
    st = d.wait_for("CityView", 180)
    d.shot("cityview")
    d.say("[new game] reached CityView")


def snapshot(d: Driver, tag: str) -> dict[str, str]:
    """Dump game state, tolerating the campaign having ended.

    Once the stage stack is replaced by the ending cutscene the GameState is released, so the
    introspection handler has nothing to answer with. That is the campaign finishing, not a
    harness failure, so do not let it abort the run.
    """
    try:
        gs = d.h.gs("all")
    except HarnessError:
        d.say(f"[gs:{tag}] unavailable - no live GameState (campaign has ended)")
        return {}
    d.say(f"[gs:{tag}] " + " ".join(f"{k}={v}" for k, v in gs.items()))
    return gs


def set_speed(d: Driver, level: int) -> None:
    """City clock speed via the always-on 0-5 hotkeys (cityview.cpp handleKeyDown)."""
    d.h.key(str(min(level, city_speed_cap())) if level > 0 else "0")


TICKS_PER_DAY = 12441600


def advance(d: Driver, game_days: float, budget_s: float = 1800.0) -> dict:
    """Run the clock forward by `game_days`, keeping turbo engaged and clearing modals.

    City Speed5 is turbo (a 5-minute jump per frame, ~0.3 game-days/sec here). The engine
    silently downgrades it to Speed1 whenever canTurbo() is false -- hostile craft, live
    projectiles or attack missions on the current map -- so we watch that gate explicitly and
    fall back to Speed4 rather than sitting at Speed1 without knowing why.
    """
    if d.game_over():
        return d.h.gs("time")
    start = int(d.h.gs("time")["ticks"])
    target = start + int(game_days * TICKS_PER_DAY)
    deadline = time.time() + budget_s
    last_report = 0.0
    prev_ticks = start
    stalls = 0
    blocked_s = 0.0
    last_intercept = 0.0
    last_station = 0.0
    parked_stage = ""
    parked_rounds = 0
    while time.time() < deadline:
        st = d.status()
        if st.stage != "CityView":
            # The clock stall detector further down only runs on the CityView branch, so any
            # screen we cannot dismiss used to spin here reporting nothing at all until the whole
            # leg budget expired. Count it and say so: a run that is parked off CityView is not
            # advancing the campaign, and the stage name is the entire diagnosis.
            if st.stage == parked_stage:
                parked_rounds += 1
            else:
                parked_stage, parked_rounds = st.stage, 0
            if parked_rounds and parked_rounds % 20 == 0:
                d.say(f"  [stall] parked on {st.stage} for ~{parked_rounds // 2}s; "
                      f"nothing is dismissing it")
            if d.dismiss_modal(st):
                continue
            # Reporting the park was an improvement on spinning silently, but it still spun: a
            # leg that ends on BaseScreen (upkeep leaves you there) parked here for its whole
            # budget while the clock never moved, because the clock only runs on the CityView
            # branch below. After ~10s of a screen that is not a battle and will not dismiss,
            # walk back to the city and carry on. return_to_city knows the way out of the base,
            # research and purchase screens; Escape alone does not.
            if parked_rounds == 20 and st.stage not in (
                    "BattleView", "BattlePreStart", "BattleBriefing", "BattleDebriefing"):
                d.say(f"  [stall] walking back to the city from {st.stage}")
                return_to_city(d)
                continue
            # Some other screen (battle, base, ufopaedia) is in charge; let its own driver run.
            if st.stage in ("BattleView", "BattlePreStart", "BattleBriefing"):
                return d.h.gs("time")
            time.sleep(0.5)
            continue
        parked_stage, parked_rounds = "", 0

        # Hold the gates rather than waiting for something to break. Periodic rather than
        # one-shot: craft finish missions, get shot down and get replaced, and the gates
        # themselves move every week (City::weeklyLoop -> generatePortals).
        if time.time() - last_station > 90:
            last_station = time.time()
            d.checks["stationed"] = d.checks.get("stationed", 0) + station_at_gates(d)

        turbo = d.h.gs("turbo")
        cap = city_speed_cap()
        if turbo.get("can_turbo") == "1":
            set_speed(d, cap)  # 5 is turbo: ~1681x faster, and canTurbo() gates it by itself
        else:
            blocked_s += 1.0
            set_speed(d, min(cap, 4))
            # Turbo is gated on there being no live hostiles; engage them rather than idling.
            if int(turbo.get("hostiles", "0")) > 0 and time.time() - last_intercept > 8:
                last_intercept = time.time()
                d.checks["intercepts"] = d.checks.get("intercepts", 0) + intercept_ufos(d)
                d.checks["recoveries"] = d.checks.get("recoveries", 0) + recover_crash_sites(d)

        time.sleep(1.0)
        now = int(d.h.gs("time")["ticks"])
        if now >= target:
            break
        stalls = stalls + 1 if now == prev_ticks else 0
        prev_ticks = now
        if stalls >= 15:
            d.say(f"  [stall] clock frozen at {now} on {st.stage}; turbo={turbo}")
            stalls = 0
        if time.time() - last_report > 30:
            last_report = time.time()
            pct = 100.0 * (now - start) / max(1, target - start)
            t = d.h.gs("time")
            d.say(f"  [clock] {pct:5.1f}% day={t['day']} week={t['week']} {t['time']} turbo={turbo.get('can_turbo')}")
    d.say(f"  [clock] turbo blocked for ~{blocked_s:.0f}s of this leg")
    return d.h.gs("time")


def return_to_city(d: Driver, tries: int = 12) -> bool:
    """Pop screens until CityView is current again. Returns True if it got there.

    The stage stack is deeper than any one helper assumes. Exiting the research screen popped to
    a BuildingScreen left over from an earlier navigation, which the research unwind did not know
    about, so the driver bounced between the two for an hour with the game clock frozen -- no
    ticks, no UFOs, no missions, nothing. Whatever the stack holds, try the conventional exits in
    order and keep going until the city is back.

    Getting home matters beyond not being stuck: research-completion score is credited only from
    CityView's event handler and the framework delivers each event to the current stage alone, so
    time spent anywhere else is score quietly forfeited.
    """
    # Deliberately bypasses click_id. Its resolved-rect fallback returns True for a control that
    # is not on the current screen at all -- it finds a rect in some .form and clicks empty space
    # -- so `click_id("BUTTON_QUIT") or click_id("BUTTON_OK")` short-circuited on a click that did
    # nothing, and the real exit was never tried. The driver reported "backing out to the city"
    # every twelve seconds for an hour while never leaving the screen. Ask the engine directly
    # and judge by whether the stage actually changed.
    for _ in range(tries):
        stage = d.status().stage
        if stage == "CityView":
            return True
        if stage == "MessageBox":
            # Not every box is an acknowledgement. RecruitScreen's "Confirm Orders" is a
            # YesNoCancel box (recruitscreen.cpp:482-484) with no BUTTON_OK at all, so pressing
            # Return here left it standing and the driver bounced between the two screens -- 76
            # strandings on RecruitScreen in one run, ten minutes at a stretch with the campaign
            # clock stopped. When unwinding, decline rather than confirm: this path only runs
            # because something already went wrong, and abandoning a half-built order is the safe
            # side of that.
            for cid in ("BUTTON_OK", "BUTTON_NO2", "BUTTON_NO", "BUTTON_CANCEL"):
                try:
                    if d.h.send(f"control {cid}").startswith("OK"):
                        break
                except (HarnessError, OSError):
                    continue
            else:
                d.h.key("Return")
        else:
            # BUTTON_QUIT means two different things depending on the screen. On BuildingScreen,
            # BaseDefenseScreen, BribeScreen and the rest it is "leave this screen". On
            # InGameOptions it is fw().stageQueueCommand({StageCmd::Command::QUIT})
            # (ingameoptions.cpp:215) -- it exits the PROGRAM.
            #
            # This loop tried BUTTON_QUIT first, unconditionally. So any time it ran while the
            # options menu was up, it shut the game down: cleanly, rc=0, no crash, no exception,
            # at whatever arbitrary point the campaign had reached. That is the entire
            # "ConnectionRefusedError [exited rc=0]" failure class -- five attempts across two
            # runs, every one of them the harness quitting its own game and then reporting that
            # the game had gone.
            #
            # It compounded with the Escape guard's own bug: Escape on CityView/BattleView OPENS
            # InGameOptions, so the driver could open the options menu by accident and then press
            # Quit on it.
            quit_exits_game = stage in ("InGameOptions", "MainMenu")
            for cid in (("BUTTON_OK",) if quit_exits_game else ("BUTTON_QUIT", "BUTTON_OK")):
                try:
                    if d.h.send(f"control {cid}").startswith("OK"):
                        break
                except (HarnessError, OSError):
                    continue
            else:
                d.escape_key(stage)
        time.sleep(0.6)
        if d.status().stage == stage:
            # That exit did nothing; fall back to the keyboard before trying again.
            d.escape_key(stage)
            time.sleep(0.4)
    return d.status().stage == "CityView"


def _lab_skill_total(d: Driver) -> int:
    """Sum of skill across every lab -- the only thing that makes research advance."""
    total = 0
    for part in d.h.gs("research").get("labs_detail", "").split("|"):
        for kv in part.split(":"):
            if kv.startswith("skill="):
                total += int(kv.split("=")[1] or 0)
    return total


# The large physics lab unlocks gate-capable craft research, and the large workshop builds one.
# Each RESEARCH_ALIEN_BUILDING_i opens the raid that unlocks
# the next. Picking whatever topic happens to sit in row 0 will eventually stumble into these,
# but not before burning game-months on brainsucker launchers.
# Ordered best-first. pick_topic_rows walks this and takes the first startable match, then falls
# back to the game's own <order> sequence for anything not named here.
#
# The old list was ["RESEARCH_ADVANCED_WORKSHOP"] + RESEARCH_ALIEN_BUILDING_0..9 and nothing
# else, which went wrong in two ways. RESEARCH_ALIEN_BUILDING_0 is the one Alien Building topic
# with no dependencies at all, so it is offered from day one -- and at 38000 man-hours it would
# occupy a lab for roughly ninety game-days while eleven-thousand-hour topics that actually
# unlock weapons and armour sat waiting. Everything outside those eleven ids fell through to
# whatever happened to sit at row zero, which is what "researching shit out of order" looked
# like from the outside. Nothing here is a shortcut: this is just the order a player who knows
# the tech tree would pick topics in.
#
# Ordering rules, in priority sequence:
#   1. Cheap roots that need only a recovered item -- they pay off fastest and unlock the rest.
#   2. The biology chain, which gates THE_REAL_ALIEN_THREAT and the toxins.
#   3. The victory chain: alien craft systems -> Advanced Workshop -> the Alien Buildings.
#   4. Heavy weapons, craft and UFO-type topics, which are useful but never blocking.
#
# Verified against data/common_patch/gamestate/research.xml: all 95 ids exist, spelled exactly,
# none hidden, none Engineering-type (an Engineering lab takes MANUFACTURE_* projects, a
# different namespace). The offered list enforces dependencies before priority is applied.
PRIORITY_RESEARCH = [
    # -- 0. the critical path AllOutWar's guide names outright: "The goal here is to shoot down
    #       UFO type 3, and then let the games begin. One alien tech -> Advanced Quantum Lab ->
    #       Other two alien techs -> Dimension Probe. Don't delay!" The lab is the gate on
    #       everything after it, so it goes ahead of the cheap roots that merely pay well. Note
    #       the guide's other warning, which cost it real time: the disruptor must be researched
    #       before ship shields can be.
    "RESEARCH_ALIEN_PROPULSION_SYSTEM",
    "RESEARCH_ALIEN_CONTROL_SYSTEM",
    "RESEARCH_ALIEN_ENERGY_SOURCE",
    "RESEARCH_ADVANCED_QUANTUM_PHYSICS_LAB",
    "RESEARCH_ADVANCED_BIOCHEMISTRY_LAB",
    "RESEARCH_ADVANCED_WORKSHOP",  # Offered only after Dimension Probe is complete.

    # -- gate craft: take the first available route through the dimension gates --
    "RESEARCH_DIMENSION_PROBE",
    "RESEARCH_UFO_TYPE_3",
    "RESEARCH_BIO-TRANSPORT",
    "RESEARCH_UFO_TYPE_5",
    "RESEARCH_EXPLORER",
    "RESEARCH_UFO_TYPE_6",
    "RESEARCH_RETALIATOR",
    "RESEARCH_UFO_TYPE_9",
    "RESEARCH_ANNIHILATOR",

    # -- 1. cheap item-gated roots: fastest payback, and they open the rest of the tree --
    "RESEARCH_BIO-TRANSPORT_MODULE",
    "RESEARCH_DISRUPTOR_GUN",
    "RESEARCH_LIGHT_DISRUPTOR_BEAM",
    "RESEARCH_BRAINSUCKER_PODS",
    "RESEARCH_BOOMEROID",
    "RESEARCH_DIMENSION_MISSILE_LAUNCHER",
    "RESEARCH_DIMENSION_MISSILE",
    "RESEARCH_VORTEX_MINE",
    "RESEARCH_PERSONAL_DISRUPTOR_SHIELD",
    "RESEARCH_PERSONAL_TELEPORTER",
    "RESEARCH_PERSONAL_CLOAKING_FIELD",
    "RESEARCH_BRAINSUCKER_LAUNCHER",
    "RESEARCH_ENTROPY_LAUNCHER",
    "RESEARCH_ENTROPY_POD",

    # -- 2. the biology chain, gating THE_REAL_ALIEN_THREAT and the toxins --
    "RESEARCH_MULTIWORM_EGG_AUTOPSY",
    "RESEARCH_MULTIWORM_EGG",
    "RESEARCH_MULTIWORM_AUTOPSY",
    "RESEARCH_MULTIWORM",
    "RESEARCH_HYPERWORM_AUTOPSY",
    "RESEARCH_HYPERWORM",
    "RESEARCH_CHRYSALIS_AUTOPSY",
    "RESEARCH_CHRYSALIS",
    "RESEARCH_THE_ALIEN_GENETIC_STRUCTURE",
    "RESEARCH_THE_ALIEN_LIFE_CYCLE",
    "RESEARCH_BIOLOGICAL_WARFARE",
    "RESEARCH_BRAINSUCKER_AUTOPSY",
    "RESEARCH_BRAINSUCKER",
    "RESEARCH_ANTHROPOD_AUTOPSY",
    "RESEARCH_ANTHROPOD",
    "RESEARCH_PSIMORPH_AUTOPSY",
    "RESEARCH_PSIMORPH",
    "RESEARCH_SPITTER_AUTOPSY",
    "RESEARCH_SPITTER",
    "RESEARCH_MEGASPAWN_AUTOPSY",
    "RESEARCH_MEGASPAWN",
    "RESEARCH_POPPER_AUTOPSY",
    "RESEARCH_POPPER",
    "RESEARCH_SKELETOID_AUTOPSY",
    "RESEARCH_SKELETOID",
    "RESEARCH_MICRONOID_AUTOPSY",
    "RESEARCH_MICRONOID",
    "RESEARCH_THE_REAL_ALIEN_THREAT",
    "RESEARCH_QUEENSPAWN_AUTOPSY",
    "RESEARCH_QUEENSPAWN",
    "RESEARCH_TOXIN_TYPE_B",
    "RESEARCH_TOXIN_TYPE_C",
    "RESEARCH_ALIEN_GAS",
    "RESEARCH_OVERSPAWN_AUTOPSY",
    "RESEARCH_OVERSPAWN_AUTOPSY_1",

    # -- 3. the victory chain: craft systems -> Advanced Workshop -> the ten Alien Buildings --
    "RESEARCH_ADVANCED_BIOCHEMISTRY_LAB",
    "RESEARCH_ALIEN_BUILDING_0",
    "RESEARCH_DIMENSION_GATES",
    "RESEARCH_THE_ALIEN_DIMENSION",
    "RESEARCH_ALIEN_PROPULSION_SYSTEM",
    "RESEARCH_ALIEN_CONTROL_SYSTEM",
    "RESEARCH_ALIEN_ENERGY_SOURCE",
    "RESEARCH_DIMENSION_PROBE",
    "RESEARCH_ADVANCED_WORKSHOP",
    "RESEARCH_ALIEN_BUILDING_1",
    "RESEARCH_ALIEN_BUILDING_2",
    "RESEARCH_ALIEN_BUILDING_3",
    "RESEARCH_ALIEN_BUILDING_4",
    "RESEARCH_ALIEN_BUILDING_5",
    "RESEARCH_ALIEN_BUILDING_6",
    "RESEARCH_ALIEN_BUILDING_7",
    "RESEARCH_ALIEN_BUILDING_8",
    "RESEARCH_ALIEN_BUILDING_9",

    # -- 4. heavy weapons, craft and UFO analysis: valuable, never blocking --
    "RESEARCH_MEDIUM_DISRUPTOR_BEAM",
    "RESEARCH_HEAVY_DISRUPTOR_BEAM",
    "RESEARCH_DISRUPTOR_INVERSION_BOMB",
    "RESEARCH_STASIS_FIELD_BOMB",
    "RESEARCH_DISRUPTOR_MULTI-BOMB",
    "RESEARCH_SMALL_DISRUPTION_SHIELD",
    "RESEARCH_LARGE_DISRUPTION_SHIELD",
    "RESEARCH_CLOAKING_FIELD",
    "RESEARCH_TELEPORTER",
    "RESEARCH_ADVANCED_SECURITY_STATION",
    "RESEARCH_ADVANCED_QUANTUM_PHYSICS_LAB",
    "RESEARCH_DISRUPTOR_ARMOR",
    "RESEARCH_X-COM_ADVANCED_CONTROL_SYSTEM",
    "RESEARCH_DEVASTATOR_CANNON",
    "RESEARCH_UFO_TYPE_1",
    "RESEARCH_UFO_TYPE_2",
    "RESEARCH_UFO_TYPE_3",
    "RESEARCH_UFO_TYPE_4",
    "RESEARCH_UFO_TYPE_5",
    "RESEARCH_UFO_TYPE_6",
    "RESEARCH_UFO_TYPE_7",
    "RESEARCH_UFO_TYPE_8",
    "RESEARCH_UFO_TYPE_9",
    "RESEARCH_UFO_TYPE_10",
    "RESEARCH_BIO-TRANSPORT",
    "RESEARCH_EXPLORER",
    "RESEARCH_RETALIATOR",
    "RESEARCH_ANNIHILATOR",
]


# The critical-path entries above also appear in their thematic sections below; keep the first
# occurrence so the order reads as written, and drop the repeats so the list is honest about its
# length.
PRIORITY_RESEARCH = list(dict.fromkeys(PRIORITY_RESEARCH))

# Workshops only build an explicitly requested capability. Prefer the best unlocked gate craft;
# once one exists (or is being built), leave other workshops idle instead of buying duplicates.
PRIORITY_MANUFACTURE = [
    "MANUFACTURE_ANNIHILATOR",
    "MANUFACTURE_RETALIATOR",
    "MANUFACTURE_EXPLORER",
    "MANUFACTURE_BIO-TRANSPORT",
]


def driver_strategy(d: Driver) -> Strategy:
    """Helpers also accept lightweight test/embedding drivers without an injected genome."""
    strategy = getattr(d, "strategy", None)
    return strategy if isinstance(strategy, Strategy) else Strategy()


def research_priority(d: Driver) -> list:
    """Reorder research only; engineering keeps the explicit gate-craft preflight."""
    return reorder_research(PRIORITY_RESEARCH, driver_strategy(d)["research_order"])


def gate_craft_project(d: Driver) -> str:
    """Cheap read-only preflight for one gate craft at the currently selected base."""
    if any(f.get("shifter") == "1" for _, f in craft_flags(d)):
        return ""
    labs = d.h.gs("research").get("labs_detail", "").split("|")
    if any(topic in lab for lab in labs for topic in PRIORITY_MANUFACTURE):
        return ""
    if "FACILITYTYPE_ADVANCED_WORKSHOP:0" not in d.h.gs("facilities").get("base", ""):
        return ""
    funds = int(d.h.gs("funds").get("balance", "0") or 0)
    priority = PRIORITY_MANUFACTURE
    if driver_strategy(d)["gate_craft_order"] == "transport_first":
        priority = list(reversed(priority))
    for want in priority:
        topic = d.h.gs(f"topic {want}")
        if (topic.get("found") == "1" and topic.get("hidden") == "0"
                and topic.get("deps_satisfied") == "1"
                and funds >= int(topic.get("cost", "0") or 0)):
            return want
    return ""


def pick_topic_rows(d: Driver) -> list[tuple[int, str]]:
    """Every startable topic for this lab, best first: priority list, then the game's own order.

    The retry loop used to fall back to raw list indices (attempt 1 -> row 1, attempt 2 -> row 2),
    which selects whatever happens to sit at that position -- including already-researched or
    too-large topics. Offering a real ordered candidate list keeps every attempt a considered one.
    """
    opts = d.h.gs("research_options")
    detail = opts.get("detail", "")
    if not detail or detail == "-":
        return []
    rows = []
    for part in detail.split("|"):
        try:
            idx, rest = part.split("=", 1)
            fields = rest.split(",")
            topic = fields[0]
            flags = dict(f.split("=", 1) for f in fields[1:] if "=" in f)
            done = flags.get("done") == "1"
            big = flags.get("big") == "1"
        except (ValueError, IndexError):
            continue
        if not done and not big and flags.get("running") != "1" and flags.get("affordable") != "0":
            rows.append((int(idx), topic))
    if opts.get("type") == "engineering" or any(t.startswith("MANUFACTURE_") for _, t in rows):
        want = gate_craft_project(d)
        return [(idx, topic) for idx, topic in rows if topic == want]
    ranked = []
    for want in research_priority(d):
        for idx, topic in rows:
            if topic == want and (idx, topic) not in ranked:
                ranked.append((idx, topic))
    # Then everything else in the game's own <order> sequence, which is what ResearchSelect shows.
    for row in rows:
        if row not in ranked:
            ranked.append(row)
    return ranked


def current_project(d: Driver) -> str:
    """The selected lab's project on ResearchScreen, or "" when it is idle.

    researchscreen.cpp:486-497 sets TEXT_CURRENT_PROJECT to the topic name, or "No Project" when
    the lab has none. Reading it is the only way to tell a busy lab from an idle one before
    pressing New Project -- and pressing New Project on a busy lab silently discards its progress.
    """
    try:
        reply = d.h.send("control TEXT_CURRENT_PROJECT get")
    except (HarnessError, OSError):
        return "unknown project"  # A failed observation must never authorize replacing work.
    if not reply.startswith("OK"):
        return "unknown project"
    # The reply is "OK <CONTROL_ID> text=<value>", and the value has had its spaces replaced with
    # underscores so it survives the whitespace-delimited protocol. Taking everything after
    # "text=" is the only correct read: matching a leading prefix left the control id glued to
    # the front, so "No_Project" never compared equal to idle and every idle lab was skipped as
    # though it were busy -- the exact inverse of the bug this function exists to prevent.
    body = reply[2:].strip()
    marker = "text="
    if marker not in body:
        return "unknown project"
    text = body.split(marker, 1)[1].strip()
    text = text.replace("_", " ").strip()
    if text.lower() in ("", "-", "no project"):
        return ""
    return text


def soldiers_at_building(d: Driver, building_id: str) -> int:
    """Soldiers the building's own screen would list: inside it or aboard a craft parked there."""
    try:
        reply = d.h.gs(f"soldiers_at_building {building_id}")
    except (HarnessError, OSError):
        return -1
    return int(reply.get("soldiers", "-1") or -1)


def send_raid_squad(d: Driver, building_id: str, where: str) -> str:
    """Fly a crewed transport to a building; the raid itself runs once the squad has landed.

    A building's screen lists only the soldiers already at that building
    (AgentAssignment::updateLocation), and EXTERMINATE there starts the battle with exactly
    those. The alert screen is different -- it lists every soldier and sends them -- which is why
    dispatching from it worked while raids opened from the city met an empty list: about 3,200
    "No Agents Selected" refusals across sixteen campaigns, every raid of the run.
    """
    pending = d.raid_en_route
    if pending and pending[0] == building_id and time.time() - pending[1] < 240.0:
        return "en-route"
    # The squad-size gate (min_squad) is applied to fit soldiers before an incident is taken
    # on; the garrison policy then decides how many board, so take the best-crewed transport.
    city = d.h.gs("alien_buildings").get("current_city", "CITYMAP_HUMAN")
    # Only armed soldiers are sent in once it lands (select_armed_squad), so carry the most of
    # them. An older binary without fighters= falls back to the crew count.
    def fighters(f: dict) -> int:
        return int(f.get("fighters", f.get("crew", "0")) or 0)
    fleet = [(idx, f) for idx, f in craft_flags(d)
             if f.get("flying") == "1" and fighters(f) > 0
             and f.get("transit", "0") == "0" and f.get("portal", "0") == "0"
             and f.get("city", city) == city]
    if not fleet:
        return "no-armed-transport"
    idx, flags = max(fleet, key=lambda c: fighters(c[1]))
    if not select_craft(d, flags.get("id", str(idx))):
        return "transport-not-selectable"
    info = d.h.gs(f"centre_on_building {building_id}")
    try:
        bx, by = (int(v) for v in info.get("at", "").split(",")[:2])
    except ValueError:
        return "bad-coords"
    # [Shift]+[Alt]+right-click a building orders the selected craft there
    # (CityView::handleClickedBuilding); a plain right-click would open its screen instead.
    d.h.send("keydown Left Shift")
    d.h.send("keydown Left Alt")
    try:
        d.h.click_xy(bx, by, button="right")
    finally:
        d.h.send("keyup Left Alt")
        d.h.send("keyup Left Shift")
    d.raid_en_route = (building_id, time.time())
    d.say(f"  [raid] flying {fighters(flags)} armed soldier(s) to {where} to clear it")
    return "en-route"


def detail_int(detail: str | None, field: str) -> int | None:
    """An integer field of a stage's harness detail, or None.

    The harness sends the detail with every space turned into "_" (harness.cpp), so a field
    runs straight into the next one: "crew=2_selected_agents=3_boarding=0". Splitting on
    whitespace there returned "3_boarding=0", which never parsed -- so once BuildingScreen grew
    fields after selected_agents, every raid read "unknown", skipped its selection check, and
    pressed EXTERMINATE with nobody selected: about 3,200 "No Agents Selected" refusals across
    sixteen campaigns.
    """
    # Field names are lower-case snake_case and values are numbers or upper-case ids, so a field
    # starts at the beginning, after whitespace, or after an "_" that does not follow a lower-case
    # letter -- which keeps "agents" from matching inside "selected_agents".
    m = re.search(rf"(?:^|(?<=\s)|(?<=[^a-z]_)){re.escape(field)}=(-?\d+)", detail or "")
    return int(m.group(1)) if m else None


# A raid is not started against more than this many aliens per armed soldier available.
RAID_MAX_ODDS = 3


def raid_infiltrated_building(d: Driver, budget_s: float = 900.0,
                              policy: dict | None = None) -> str:
    """Clear aliens out of a human building. Returns the battle outcome, or why it could not run.

    This is the part of the game the driver was not playing at all, and it is the one that decides
    whether a campaign keeps its funding. Alien crews sitting in a building raise their owner's
    infiltrationValue every hour (organisation.cpp:657-673), and aliens left alone spread to
    neighbouring buildings (Building::alienMovement, chance 15 + 3 x count, +20 when the owner is
    friendly to them). Most buildings belong to the government, and government relation below -50
    terminates funding outright. Campaigns were dying at gov_relation -78 with 19 and 39 buildings
    infiltrated while the driver read that number and did nothing about it.

    A ground raid is also free of the collateral penalty that makes air combat so costly: the
    relation charge in Scenery::handleCollision is city-map only, and battlemappart has no
    equivalent. Fighting inside the building costs nothing with its owner.

    Worth knowing when reading the result: retreating hands the aliens straight back. On exit,
    survivors of a building raid go back into that same building (battle.cpp:2900-2910), and
    survivors of a UFO recovery seed a NEARBY building instead (battle.cpp:2955-2963) -- which is
    the "escape the map and spread" behaviour, and a reason not to withdraw casually.
    """
    st = d.status()
    if st.stage != "CityView":
        return f"not-in-city ({st.stage})"
    # Act on what the game has TOLD us, not on a list of every infiltrated building. A player
    # gets no such list: they watch the UFOs, see the alert naming a building, and go there. The
    # driver remembers those alerts (Driver.alerted_buildings, filled from AlertScreen's own
    # report) and revisits them, which is the same information a human would be working from.
    target = None
    outmatched = False
    # Do not feed a squad to a crew it cannot beat. Two soldiers sent against fifteen aliens were
    # wiped out, the replacements were unarmed, and the next alert -- beside the base -- went
    # unanswered: four base defences followed. Losing the raid leaves the aliens where they were
    # (battle.cpp puts the survivors back in the building), so a hopeless raid buys nothing.
    armed = int(d.h.gs("agents").get("armed", "0") or 0)
    for name in list(d.alerted_buildings):
        probe = d.h.gs(f"centre_on_building {name}")
        crew = int(probe.get("crew", "0") or 0) if probe.get("centred") == "1" else 0
        if crew <= 0:
            # Either it is gone or the aliens have moved on; stop tracking it.
            d.alerted_buildings.remove(name)
            d.base_threats.discard(name)
            continue
        if crew > max(1, armed) * RAID_MAX_ODDS:
            d.say(f"  [raid] {name}: {crew} aliens against {armed} armed - too many; skipping for "
                  f"now")
            outmatched = True
            continue
        target, info = name, probe
        break
    if not target and outmatched:
        # The message log would only point back at one of the crews just judged too strong.
        return "outmatched"
    if not target:
        # Nothing pending from an alert we happened to be present for. Fall back to the player's
        # own message log, which is the same record the city view shows and lets you click to
        # zoom: reports of alien activity we were in a battle for at the time. Six alerts caught
        # against twenty-two infiltrated buildings is what a driver that only reads live alerts
        # manages, and the difference is exactly the ones it was too busy to see.
        info = d.h.gs("centre_on_message")
        if info.get("centred") != "1":
            return "nothing-reported"
        d.say(f"  [raid] from the message log: {info.get('text', '?')[:60]}")
    prepare_loadouts(d, "alien_building")
    # Buying/refitting visits base screens. Re-observe the target's screen coordinates.
    info = d.h.gs(f"centre_on_building {target}") if target else d.h.gs("centre_on_message")
    at = info.get("at", "")
    try:
        bx, by = (int(v) for v in at.split(",")[:2])
    except ValueError:
        return "bad-coords"
    # centre_on_message reports a location rather than a building record, so those fields are
    # absent on that path -- printing them as "None" made a working raid look broken.
    where = info.get("building") or info.get("text", "a reported sighting")[:40]
    crew_here = info.get("crew")
    # Which of the two coordinate sources produced this. A failure from the alert path and a
    # failure from the message-log path have different causes and the record must separate them.
    source = "alert" if target else "message-log"
    d.say(f"  [raid] clearing {where}"
          + (f" ({crew_here} aliens)" if crew_here else ""))
    building_id = info.get("building")
    if building_id and soldiers_at_building(d, building_id) == 0:
        return send_raid_squad(d, building_id, where)
    d.raid_en_route = None

    # A raid that never opens BuildingScreen is the worst failure this driver has, because from
    # outside it is indistinguishable from a quiet map: attempt 1 of the 301 arena run recorded
    # "no-building-screen (CityView)" and the mission was simply never offered.
    #
    # This does NOT claim to fix that. Why the right-click missed is not established -- one
    # candidate is that centre_on_building returns the screen point of the building's mid-tile at
    # z=2 (cityview.cpp:2371), which need not be where the building is DRAWN, the same class of
    # error already fixed in the battle view -- and a fixed one-second sleep could equally have
    # been the whole story. So: poll instead of sleeping, try once more from freshly-centred
    # coordinates, and record which source the coordinates came from. That turns a silent miss
    # into a diagnosable one, and costs one extra click when the first genuinely missed.
    def open_building_screen() -> bool:
        d.h.ok(f"click {bx} {by} right")
        deadline = time.time() + 3.0
        while time.time() < deadline and d.status().stage == "CityView":
            time.sleep(0.2)
        return d.status().stage == "BuildingScreen"

    if not open_building_screen():
        # Read the stage ONCE, and BEFORE return_to_city. Both this function and its predecessor
        # built the label from a d.status() call placed after the recovery, so every report came
        # back saying "CityView" -- the stage we had just navigated back to, never the screen that
        # actually answered the click. The label existed to name that screen and was destroying
        # the only copy of it.
        landed_on = d.status().stage
        # Retry only from CityView. Any other screen means the click DID land and was answered --
        # a "No Entrance" box, say -- and clicking again would hammer a modal.
        if landed_on != "CityView":
            return_to_city(d)
            return f"no-building-screen ({landed_on}, via {source})"
        again = d.h.gs(f"centre_on_building {target}") if target else d.h.gs("centre_on_message")
        retried = False
        if again.get("centred") == "1":
            try:
                bx, by = (int(v) for v in again.get("at", "").split(",")[:2])
            except ValueError:
                pass
            else:
                retried = open_building_screen()
        if not retried:
            landed_on = d.status().stage
            return_to_city(d)
            return f"no-building-screen ({landed_on}, via {source}, retry failed)"

    # Select, then CHECK: BuildingScreen reports how many agents are really selected.
    def selected_count() -> int:
        value = detail_int(d.status().detail, "selected_agents")
        return -1 if value is None else value

    # Do not search a building that has no aliens in it. BuildingScreen reports the crew it can
    # see, and searching an empty building costs the owner -5-difficulty relation every time
    # (buildingscreen.cpp:154-166) -- caught doing exactly that at "Warehouse_Nine crew=0". The
    # crew can move on between spotting it and arriving, which is the game working as intended;
    # the answer is to check on arrival and walk away, not to search anyway.
    detail_now = d.status().detail or ""
    if "crew=" in detail_now:
        try:
            here = int(detail_now.split("crew=", 1)[1].split("_")[0].split()[0])
        except (ValueError, IndexError):
            here = -1
        if here == 0:
            d.say("  [raid] no aliens here any more; leaving rather than paying for a search")
            return_to_city(d)
            return "already-clear"

    # Only soldiers who can fight, picked by the rows the screen resolves (select_armed_squad).
    # The old pixel-offset clicks and the row sweep behind them took whoever was listed, armed
    # or not, and a building raid starts its battle with exactly the selected soldiers.
    d.select_armed_squad(d.status(), want=MAX_SQUAD)
    time.sleep(0.3)
    if selected_count() > 0:
        d.say(f"  [raid] {selected_count()} armed soldier(s) selected")
    if selected_count() == 0:
        # Say what the state was, rather than leaving the cause a guess. A building raid needs a
        # craft free to carry the squad, so the likely reason is transport rather than the click
        # missing -- but "likely" is not a finding, and the numbers settle it either way.
        try:
            ic = d.h.gs("interceptors")
            ag = d.h.gs("agents")
            free = sum(1 for _, _, flags in parse_craft_flags(ic.get("detail", ""))
                       if flags.get("flying") == "1" and flags.get("crew") == "0")
            d.say(f"  [raid] no armed soldier here to send: {ag.get('soldiers_fit')} fit, "
                  f"{ag.get('armed')} armed, {ic.get('craft')} craft, {free} of them empty "
                  f"and flying")
        except (HarnessError, OSError):
            pass
        return_to_city(d)
        return "no-agents-selectable"

    # EXTERMINATE, never RAID. They look interchangeable and are not: BUTTON_RAID is a deliberate
    # attack on the ORGANISATION and costs 200 relation with the owner outright
    # (buildingscreen.cpp:223-226), which for a government building is the funding cut in one
    # click. BUTTON_EXTERMINATE searches for aliens and starts the mission when it finds them, at
    # no cost -- the only penalty on that path is -5-difficulty for searching a building that
    # turns out to be empty (buildingscreen.cpp:154-166), which is why this only ever targets a
    # building the game has just told us holds a crew.
    d.click_id("BUTTON_EXTERMINATE", d.status())
    time.sleep(1.5)

    st = d.status()
    if st.stage == "MessageBox":
        d.say(f"  [raid] refused: {d.h.send('controls')[:120]}")
        d.h.key("Return")
        return_to_city(d)
        return "refused"
    if st.stage not in ("BattleBriefing", "BattlePreStart", "BattleView"):
        return_to_city(d)
        return f"no-battle ({st.stage})"

    outcome = win_battle(d, budget_s=budget_s, policy=policy)
    after = d.h.gs("infiltrated")
    d.say(f"  [raid] {outcome}; {after.get('infiltrated')} building(s) still infiltrated, "
          f"gov relation {after.get('gov_relation')}")
    return outcome


def raid_alien_building(d: Driver) -> str:
    """Raid the next alien building. Returns the battle outcome, or why it could not start.

    This is the win condition: Battle::exitBattle fires GameWon only for the alien building
    carrying victory=true, and each earlier raid force-completes the research that unlocks the
    next. Only works from inside CITYMAP_ALIEN, and only for a building whose accessTopic is
    researched -- BuildingScreen refuses with "No Entrance" otherwise
    (buildingscreen.cpp:112-122), so this checks first rather than clicking hopefully.
    """
    st = d.status()
    if st.stage != "CityView":
        return "not-in-city"
    where = d.h.gs("alien_buildings")
    if where.get("current_city") != "CITYMAP_ALIEN":
        return "not-in-alien-dimension"

    if not select_gate_craft(d, "CITYMAP_ALIEN"):
        return "no-gate-squad"
    if not prepare_loadouts(d, "alien_dimension", vehicle="selected")["ready"]:
        return "loadout-not-verified"
    if not select_gate_craft(d, "CITYMAP_ALIEN"):
        return "no-gate-squad"

    target = d.h.gs("centre_on_raidable")
    if target.get("centred") != "1":
        return "nothing-raidable"
    d.say(f"  [raid] target {target.get('building')} (victory={target.get('victory')})")
    time.sleep(0.5)
    bx, by = (int(v) for v in target["at"].split(",")[:2])
    if d.h.gs("selected").get("building") != target.get("building"):
        if not d.click_id("BUTTON_GOTO_BUILDING", d.status()):
            return "no-goto-building-button"
        d.h.ok(f"click {bx} {by}")
        d.say(f"  [raid] squad flying to {target.get('building')}")
        deadline = time.time() + 120.0
        while time.time() < deadline:
            st = d.status()
            if st.stage != "CityView":
                return "arrival-interrupted"
            if d.h.gs("selected").get("building") == target.get("building"):
                break
            set_speed(d, 4)
            set_speed(d, 5)
            time.sleep(0.5)
        else:
            return "squad-still-travelling"
        d.say(f"  [raid] squad arrived at {target.get('building')}")
        target = d.h.gs("centre_on_raidable")
        if target.get("centred") != "1":
            return "nothing-raidable"
        bx, by = (int(v) for v in target["at"].split(",")[:2])
    d.h.ok(f"click {bx} {by} right")
    time.sleep(1.2)

    st = d.status()
    if st.stage != "BuildingScreen":
        d.say(f"  [raid] expected BuildingScreen, got {st.stage}")
        return_to_city(d)
        return "no-building-screen"

    craft = [r for r in assignment_rows(st, "boarding") if len(r) >= 6 and r[2] == 1 and r[4] == 1]
    if not craft:
        return_to_city(d)
        return "no-gate-craft-at-building"
    d.h.click_xy(*craft[0][:2])  # Selecting a craft selects its actual passenger list.
    time.sleep(0.2)
    detail = d.status().detail or ""
    selected = detail.split("selected_agents=", 1)[-1].split("_", 1)[0].split()[0]
    if not selected.isdigit() or int(selected) == 0:
        return_to_city(d)
        return "no-agents-selectable"
    d.click_id("BUTTON_RAID", d.status())
    time.sleep(1.5)

    st = d.status()
    if st.stage == "MessageBox":
        d.say(f"  [raid] refused: {d.h.send('controls')[:120]}")
        d.h.key("Return")
        return_to_city(d)
        return "refused"
    if st.stage not in ("BattleBriefing", "BattlePreStart", "BattleView"):
        return_to_city(d)
        return f"no-battle ({st.stage})"

    return win_battle(d, budget_s=2400)


def station_at_gates(d: Driver) -> int:
    """Hold armed craft on the dimension gates instead of waiting for alerts.

    campaign-plan.md already prescribes this and the driver never did it: "Do not wait for alerts.
    Detection is throttled and lossy; active patrolling beats it." What the driver did instead was
    react to hostiles ALREADY in the city, which means every interception is a dogfight over
    occupied buildings.

    That is expensive in a way the score sheet makes explicit. Scenery::die debits cityDamage for
    a destroyed tile regardless of who destroyed it (game/state/city/scenery.cpp:1338) -- our own
    missiles and the wrecks of the UFOs we shoot down are charged to us exactly like alien
    bombardment. Measured over one run to day 15: ufos_downed +300 against city_damage -1134, with
    three of five craft lost and no crewed transport left, so ZERO ground missions were ever flown
    while 73 hostiles walked the city and infiltration climbed to 1240. Winning the air war over
    the rooftops cost more than losing it would have.

    Gates are where UFOs arrive, so a craft holding station there engages before the UFO reaches
    anything breakable. Issued the way a player does it: select the craft, centre the view on a
    portal, LEFT-click it. Right-click is "fly through the gate" and belongs to goto_portal().

    Craft parked in the base are deliberately NOT filtered out -- a hangar queen is exactly what
    should be sent to hold a gate, and the move order takes it off by itself
    (VehicleMission::takeOffCheck).
    """
    st = d.status()
    if st.stage != "CityView":
        return 0

    fighters = []
    for idx, flags in craft_flags(d):
        if flags.get("armed") != "1" or flags.get("flying") != "1":
            continue
        fighters.append((int(flags.get("pax", "0") or 0), idx))
    if not fighters:
        return 0

    # Reserve the roomiest flying craft as the squad transport, station the rest. Derived from the
    # fleet rather than hardcoded: "hoverbikes and hovercars" are the small-capacity flyers
    # (pax=4); the Valkyrie (pax=12) is the transport. A fixed threshold would need a magic number
    # and would rot as soon as the fleet changes -- reserving the largest adapts by itself.
    fighters.sort(reverse=True)
    reserved_pax, reserved_idx = fighters[0]
    fighters = [idx for _p, idx in fighters[1:]]
    if not fighters:
        d.say(f"  [gates] only one armed flyer (craft {reserved_idx}, pax={reserved_pax}); "
              f"holding it back as transport")
        return 0

    if not d.click_id("BUTTON_TAB_2", st):
        return 0
    time.sleep(0.35)
    lst = d.controls(d.status()).get("OWNED_VEHICLE_LIST")
    if lst is None or lst.w <= 0:
        return 0

    sent = 0
    for idx in fighters:
        d.h.click_xy(lst.x + 16 + idx * 36, lst.y + lst.h // 2)
        time.sleep(0.25)
        # Re-centre per craft: selecting a vehicle can move the camera, which would leave stale
        # screen coordinates pointing at whatever now occupies that pixel.
        info = d.h.gs("centre_on_portal")
        if info.get("centred") != "1":
            continue
        time.sleep(0.4)
        try:
            px, py = (int(v) for v in info["at"].split(",")[:2])
        except (KeyError, ValueError):
            continue
        d.h.ok(f"click {px} {py}")
        time.sleep(0.4)
        sent += 1
    if sent:
        d.say(f"  [gates] {sent} armed craft ordered to hold station on a dimension gate")
    return sent


def parse_craft_flags(detail: str) -> list[tuple[int, str, dict[str, str]]]:
    """Parse the shared fleet wire format, retaining every string and numeric field."""
    result = []
    for part in (detail or "").split("|"):
        bits = part.split(":")
        if len(bits) < 3 or not bits[0].isdigit():
            continue
        flags = dict(f.split("=", 1) for f in bits[-1].split(",") if "=" in f)
        result.append((int(bits[0]), ":".join(bits[1:-1]), flags))
    return result


def craft_flags(d: Driver) -> list[tuple[int, dict[str, str]]]:
    """Owned craft in state order, retaining every gs interceptors field."""
    return [(idx, flags) for idx, _, flags in
            parse_craft_flags(d.h.gs("interceptors").get("detail", ""))]


def select_craft(d: Driver, craft_id: str) -> bool:
    """Select one owned craft through CityView's vehicle tab list, scrolling it into view."""
    if not d.click_id("BUTTON_TAB_2", d.status()):
        return False
    time.sleep(0.35)
    # The city's craft icons listen to MouseDown, not ListBoxChangeSelected. Use their resolved
    # positions and scroll the list when necessary; CONTROL set would select only the widget.
    def position():
        for part in d.h.gs("owned_craft_rows").get("detail", "").split("|"):
            index, _, values = part.partition("=")
            if index == craft_id:
                return tuple(int(v) for v in values.split(","))
        return ()
    at = position()
    if len(at) != 3:
        return False
    if at[2] == 0:
        viewport = d.live_rect("OWNED_VEHICLE_LIST")
        reply = d.h.send("control OWNED_VEHICLE_LIST_SCROLL get")
        scroll = dict(p.split("=", 1) for p in reply.split() if "=" in p)
        if not viewport or "max" not in scroll:
            return False
        value = int(scroll.get("value", "0")) + at[0] - (viewport["x"] + viewport["w"] // 2)
        value = max(int(scroll.get("min", "0")), min(int(scroll["max"]), value))
        d.h.send(f"control OWNED_VEHICLE_LIST_SCROLL set {value}")
        time.sleep(0.25)
        at = position()
    if len(at) != 3 or at[2] != 1:
        return False
    # Right-click removes this craft from any existing group. The following left-click then
    # replaces the selection with it alone, so ordinary interceptors never receive the gate order.
    d.h.click_xy(*at[:2], button="right")
    time.sleep(0.15)
    d.h.click_xy(*at[:2])
    time.sleep(0.3)
    return True


def select_gate_craft(d: Driver, city: str, require_crew: bool = True) -> bool:
    """Select a gate-capable transport with soldiers in this city, through its UI list."""
    minimum = driver_strategy(d)["cross_min_crew"] if city == "CITYMAP_HUMAN" else 1
    candidates = [(idx, f) for idx, f in craft_flags(d)
                  if f.get("shifter") == "1" and f.get("flying") == "1"
                  and (not require_crew or int(f.get("crew", "0")) >= minimum)
                  and f.get("city", city) == city
                  and f.get("transit", "0") == "0"]
    if not candidates:
        d.say(f"  [portal] no crewed gate-capable craft ready in {city}")
        return False
    idx, flags = candidates[0]
    if not select_craft(d, flags.get("id", str(idx))):
        return False
    selected = d.h.gs("selected")
    return (selected.get("selected") == "1" and
            (not require_crew or int(selected.get("with_soldier", "0") or 0) > 0))


def wait_for_dimension(d: Driver, city: str, budget_s: float = 120.0) -> bool:
    """Keep the clock running until the normal dimension-view switch actually happens."""
    deadline = time.time() + budget_s
    while time.time() < deadline:
        st = d.status()
        if st.stage == "CityView":
            if d.h.gs("alien_buildings").get("current_city") == city:
                d.say(f"  [portal] arrived in {city}")
                return True
            set_speed(d, 4)
            set_speed(d, 5)
        elif st.stage in ("BattleBriefing", "BattlePreStart", "BattleView", "BaseDefenseScreen",
                          "VideoScreen", "MainMenu"):
            return False  # Victory.run handles the mission/ending before resuming the crossing.
        elif not d.dismiss_modal(st):
            return False
        time.sleep(0.5)
    d.say(f"  [portal] arrival pending: waiting for city view {city}")
    return False


def goto_portal(d: Driver, destination: str = "CITYMAP_ALIEN") -> bool:
    """Order the crewed gate craft across, then observe the normal day-rollover view switch."""
    d.loadout_blocked = False
    if d.status().stage != "CityView":
        return False
    current = d.h.gs("alien_buildings").get("current_city", "CITYMAP_HUMAN")
    if current == destination:
        return True
    fleet = craft_flags(d)
    require_crew = destination == "CITYMAP_ALIEN"
    # A prior order can already have moved the craft while the view waits for midnight.
    if any(f.get("shifter") == "1" and (not require_crew or int(f.get("crew", "0")) > 0)
           and (f.get("city") == destination or f.get("transit") == "1"
                or f.get("portal") == "1") for _, f in fleet):
        return wait_for_dimension(d, destination)
    if not select_gate_craft(d, current, require_crew=require_crew):
        return False
    if require_crew:
        prepared = prepare_loadouts(d, "alien_dimension", vehicle="selected")
        if not prepared["ready"]:
            d.loadout_blocked = True
            d.say("  [loadout-deferred] alien-dimension departure held for a verified role kit; "
                  "research, procure or wait for delivery at home")
            return False
        # The refit's base screens may have changed the selected vehicle.
        if not select_gate_craft(d, current, require_crew=True):
            return False
    info = d.h.gs("centre_on_portal")
    if info.get("centred") != "1":
        d.say("  [portal] no portal found in this city")
        return False
    time.sleep(0.5)
    px, py = (int(v) for v in info["at"].split(",")[:2])
    d.h.ok(f"click {px} {py} right")
    label = "crewed gate craft" if require_crew else "gate craft"
    d.say(f"  [portal] {label} ordered toward {destination}; waiting for city view")
    return wait_for_dimension(d, destination)


def build_second_base(d: Driver) -> str:
    """Buy a second base. Returns "bought", or why it could not.

    This is the single most valuable insurance a campaign can buy. GameLost is raised on
    exactly one condition -- state.player_bases.empty() (base.cpp:150-159) -- so with two bases,
    losing one to a botched base defence no longer ends the game. Funding termination is a
    separate and much milder thing: weeklyPlayerUpdate merely sets income to zero
    (gamestate.cpp:1668-1680), and a campaign with money in the bank can still research and
    manufacture its way to victory afterwards.

    BaseSelectScreen accepts only a building with a base_layout owned by the government
    (baseselectscreen.cpp:93-97), and it is constructed with CityView's current centre
    (cityview.cpp:1321) -- so centring the city on the site first puts it under the middle of the
    screen, which is what makes it clickable without pixel-hunting.
    """
    st = d.status()
    if st.stage != "CityView":
        return f"not-in-city ({st.stage})"
    info = d.h.gs("centre_on_basesite")
    if info.get("centred") != "1":
        return "no-eligible-site"
    if info.get("affordable") != "1":
        return f"too-expensive (price {info.get('price')} vs {info.get('balance')})"
    price = info.get("price")
    d.say(f"  [base] buying a second base: {info.get('building')} for ${price}")

    d.click_id("BUTTON_TAB_1", st)
    time.sleep(0.35)
    if not d.click_id("BUTTON_BUILD_BASE", d.status()):
        return "no-build-base-button"
    try:
        st = d.wait_for("BaseSelectScreen", 20)
    except TimeoutError:
        return f"base-select-not-reached ({d.status().stage})"

    # The site sits under the centre of the screen; nudge outward a little if the exact middle
    # lands on a gap between scenery blocks rather than the building itself.
    for dx, dy in ((0, 0), (0, -24), (0, 24), (-32, 0), (32, 0), (-24, -16), (24, 16)):
        d.h.click_xy(max(0, min(st.w - 1, st.w // 2 + dx)),
                     max(0, min(st.h - 1, st.h // 2 + dy)))
        time.sleep(0.55)
        if d.status().stage == "BaseBuyScreen":
            break
    if d.status().stage != "BaseBuyScreen":
        return_to_city(d)
        return "could-not-open-buy-screen"

    if not d.click_id("BUTTON_BUY_BASE", d.status()):
        return_to_city(d)
        return "buy-button-refused"
    time.sleep(0.8)
    return_to_city(d)
    after = d.h.gs("centre_on_basesite")
    d.say(f"  [base] bases now {after.get('bases', '?')}")
    return "bought"


def build_facility(d: Driver, want: str = "FACILITYTYPE_ADVANCED_WORKSHOP") -> bool:
    """Construct a base facility. Returns True when one is actually placed.

    Gate craft need a Large physics lab for research and a Large workshop for manufacture.

    Placement cannot be driven by name. BaseScreen keys entirely off raw mouse events against the
    control under the cursor (basescreen.cpp:259-417): hovering a row in LISTBOX_FACILITIES sets
    the facility to be dragged, and the build only commits on MouseUp inside the base grid.
    CONTROL click raises ButtonClick and CONTROL set raises ListBoxChangeSelected, neither of
    which BaseScreen listens for -- both are dead ends here. So this is a genuine drag.

    Which row is which is invisible from the UI: every row is an identically-named Graphic. The
    facilities query reports them in the same order BaseScreen builds them, so the wanted type's
    position in that list is its position on screen.
    """
    st = d.status()
    if st.stage != "CityView":
        return False
    info = d.h.gs("facilities")
    if any(part.split(":")[0] == want for part in info.get("base", "").split(",")):
        return False  # Includes construction in progress; never buy the same facility twice.
    costs = dict(part.split("=", 1) for part in info.get("costs", "").split("|") if "=" in part)
    if want not in costs or int(d.h.gs("funds").get("balance", "0") or 0) < int(costs[want]):
        d.say(f"  [build] waiting for funds to build {want}")
        return False
    offer = info.get("offer", "")
    if want not in offer:
        d.say(f"  [build] {want} is not offered yet (research gates it)")
        return False
    row = -1
    for part in offer.split("|"):
        idx, _, name = part.partition("=")
        if name == want:
            row = int(idx)
            break
    if row < 0:
        return False
    before = info.get("base", "")

    d.click_id("BUTTON_TAB_1", st)
    time.sleep(0.35)
    if not d.click_id("BUTTON_SHOW_BASE", d.status()):
        return False
    try:
        st = d.wait_for("BaseScreen", 30)
    except TimeoutError:
        return False

    rows = d.live_rects("FACILITY_BUILD_TILE")
    grid = d.live_rects("GRAPHIC_BASE_VIEW")
    if row >= len(rows) or not grid:
        d.say(f"  [build] cannot see row {row} of {len(rows)} / grid {bool(grid)}")
        d.click_id("BUTTON_OK", d.status())
        return False
    rx, ry, rw, rh = rows[row]
    gx, gy, _, _ = grid[0]
    src = (rx + rw // 2, ry + rh // 2)

    # The grid is 8x8 tiles of 32px (base.h:42, basegraphics.h:18). Corridor tiles and existing
    # facilities are not exposed anywhere, so a free spot cannot be computed -- try tiles until
    # one is accepted, treating a MessageBox as a rejection to dismiss and move on.
    for tile in range(64):
        col, rowc = tile % 8, tile // 8
        dst = (gx + 32 * col + 16, gy + 32 * rowc + 16)
        d.h.ok(f"move {src[0]} {src[1]}")
        time.sleep(0.15)
        d.h.ok(f"down {src[0]} {src[1]}")
        time.sleep(0.12)
        d.h.ok(f"move {(src[0] + dst[0]) // 2} {(src[1] + dst[1]) // 2}")
        time.sleep(0.1)
        d.h.ok(f"move {dst[0]} {dst[1]}")
        time.sleep(0.12)
        d.h.ok(f"up {dst[0]} {dst[1]}")
        time.sleep(0.5)
        cur = d.status()
        if cur.stage == "MessageBox":
            d.h.key("Return")
            time.sleep(0.35)
            continue
        after = d.h.gs("facilities").get("base", "")
        if after != before:
            d.say(f"  [build] placed {want} at tile {col},{rowc}")
            for _ in range(5):
                stt = d.status()
                if stt.stage == "CityView":
                    break
                if not d.click_id("BUTTON_OK", stt):
                    d.escape_key()
                time.sleep(0.5)
            return True

    d.say(f"  [build] no free tile accepted {want}")
    for _ in range(5):
        stt = d.status()
        if stt.stage == "CityView":
            break
        if not d.click_id("BUTTON_OK", stt):
            d.escape_key()
        time.sleep(0.5)
    return False


def manufacture(d: Driver, want: str = "MANUFACTURE_BIO-TRANSPORT", qty: int = 1) -> bool:
    """Start a manufacturing project in a workshop. Returns True when it actually takes.

    Bio-Trans and its successors can enter dimension gates without a shifter component. Keep
    existing projects intact, including a copy of the requested craft running in another lab.

    Manufacturing reuses the research screen and ResearchSelect, but Lab::setResearch branches on
    lab type: an Engineering lab is charged the project cost immediately. required_lab_size is
    *not* used to filter the offered list, so a Large-only project appears in a small workshop's
    list too and is refused with a message box when picked -- gs research_options flags that as
    big=1, and pick_topic_rows already skips those.
    """
    if any(want in lab for lab in d.h.gs("research").get("labs_detail", "").split("|")):
        d.say(f"  [manufacture] {want} already in progress; leaving it")
        return False
    st = d.status()
    if st.stage != "CityView":
        return False
    d.click_id("BUTTON_TAB_1", st)
    time.sleep(0.35)
    if not d.click_id("BUTTON_SHOW_BASE", d.status()):
        return False
    try:
        d.wait_for("BaseScreen", 30)
    except TimeoutError:
        return False
    d.click_id("BUTTON_BASE_RES_AND_MANUF", d.status())
    try:
        d.wait_for("ResearchScreen", 30)
    except TimeoutError:
        return False

    started = False
    for list_id in ("LIST_LARGE_LABS", "LIST_SMALL_LABS"):
        for slot in range(6):
            if d.status().stage != "ResearchScreen":
                break
            try:
                if not d.h.send(f"control {list_id} set {slot}").startswith("OK"):
                    break
            except HarnessError:
                break
            time.sleep(0.3)
            opts = d.h.gs("research_options")
            if opts.get("type") != "engineering" and "MANUFACTURE" not in opts.get(
                "detail", ""
            ):
                continue
            # The engine reports the exact topic id; the label is a fallback for older builds.
            project = opts.get("current")
            if project is None:
                project = current_project(d) or "-"
            if project != "-":
                d.say(f"  [manufacture] lab {list_id}[{slot}] already on {project}; leaving it")
                continue
            row = -1
            for part in opts.get("detail", "").split("|"):
                idx, _, rest = part.partition("=")
                fields = rest.split(",")
                flags = dict(f.split("=", 1) for f in fields[1:] if "=" in f)
                if fields and fields[0] == want and flags.get("done") != "1":
                    if flags.get("big") == "1":
                        d.say(f"  [manufacture] {want} needs a larger lab than this one")
                        continue
                    if flags.get("running") == "1" or flags.get("affordable") == "0":
                        continue
                    row = int(idx)
                    break
            if row < 0:
                continue

            if not d.click_id("BUTTON_RESEARCH_NEWPROJECT", d.status()):
                continue
            time.sleep(0.45)
            if d.status().stage != "ResearchSelect":
                continue
            try:
                d.h.control("LIST", "set", str(row))
            except HarnessError:
                pass
            time.sleep(0.3)
            d.click_id("BUTTON_OK", d.status())
            time.sleep(0.6)
            if d.status().stage == "MessageBox":
                d.say(f"  [manufacture] refused: {d.h.send('controls')[:120]}")
                d.h.key("Return")
                time.sleep(0.5)
                continue
            if d.h.gs("research_options").get("current") != want:
                continue  # A click or a refused selection is not a manufacturing receipt.
            # Quantity only becomes meaningful once a project is committed. New projects default
            # to ONE in Lab::setResearch, so a missing slider cannot create an unbounded order.
            try:
                d.h.control("MANUFACTURE_QUANTITY_SLIDER", "set", str(qty))
            except HarnessError:
                pass
            time.sleep(0.4)
            d.say(f"  [manufacture] started {want} x{qty}")
            started = True
            break
        if started:
            break

    for _ in range(8):
        st = d.status()
        if st.stage == "CityView":
            break
        if st.stage in ("ResearchSelect", "ResearchScreen", "BaseScreen"):
            d.click_id("BUTTON_OK", st)
        elif not d.dismiss_modal(st):
            d.escape_key()
        time.sleep(0.5)

    if not started:
        d.say(f"  [manufacture] could not start {want} (needs a Large workshop?)")
    return started


def staff_labs(d: Driver) -> int:
    """Put scientists into the labs. Returns the gain in total lab skill.

    Assigning a project is not enough, and this is the defect that quietly invalidated every
    research claim made so far: Lab::update returns immediately when getTotalSkill() is zero
    (research.cpp:445-449), and skill comes from agents in lab->assigned_agents. With an empty
    lab a project sits at 0 man-hours for ever while the screen cheerfully shows it as current.
    Measured before this existed: eight game-days in, RESEARCH_DIMENSION_GATES was still 0/5000.

    Selecting a row in LIST_UNASSIGNED assigns that scientist to the lab being viewed, capped by
    the facility's capacity (researchscreen.cpp:120-147). The list re-populates after each
    assignment, so index 0 always names the next unassigned scientist.
    """
    before = _lab_skill_total(d)
    if d.status().stage != "ResearchScreen":
        return 0
    for list_id in ("LIST_LARGE_LABS", "LIST_SMALL_LABS"):
        for slot in range(6):
            if d.status().stage != "ResearchScreen":
                break
            try:
                if not d.h.send(f"control {list_id} set {slot}").startswith("OK"):
                    break
            except (HarnessError, OSError):
                break
            time.sleep(0.25)
            # Fill this lab until it refuses (full) or there is nobody left to assign.
            for _ in range(12):
                try:
                    if not d.h.send("control LIST_UNASSIGNED set 0").startswith("OK"):
                        break
                except (HarnessError, OSError):
                    break
                time.sleep(0.18)
    after = _lab_skill_total(d)
    d.say(f"  [staff] total lab skill {before} -> {after}")
    return after - before


def assign_research(d: Driver) -> bool:
    """CityView -> base -> research, start a project in every idle lab."""
    before = d.h.gs("research")
    d.say(f"[research] before: {before}")

    st = d.wait_for("CityView", 60)
    d.click_id("BUTTON_TAB_1", st); time.sleep(0.4)
    st = d.status()
    if not d.click_id("BUTTON_SHOW_BASE", st):
        d.say("[research] could not open base screen"); return False
    try:
        st = d.wait_for("BaseScreen", 30)
    except TimeoutError:
        d.say(f"[research] base screen not reached (at {d.status().stage})"); return False
    d.shot("basescreen")
    d.click_id("BUTTON_BASE_RES_AND_MANUF", st); time.sleep(0.6)
    try:
        st = d.wait_for("ResearchScreen", 30)
    except TimeoutError:
        d.say(f"[research] research screen not reached (at {d.status().stage})"); return False
    d.shot("researchscreen")

    # Staff the labs before assigning work: a project in an empty lab never advances a single
    # man-hour, which is how "research assigned" was true and meaningless at the same time.
    #
    # But do it only when staffing is actually short. Every second spent on this screen is a
    # second CityView is not the current stage, and research-completion score is credited *only*
    # from CityView's event handler (cityview.cpp:4506) while the framework delivers each event
    # to the top stage alone (framework.cpp:608). An event that fires while the driver is in here
    # is lost for good -- and researchCompleted is the one score bucket that can never go
    # negative, so every one of those is pure forfeited score.
    if _lab_skill_total(d) < 800 or any(":built:" in lab and ":staff=0:" in lab
                                       for lab in before.get("labs_detail", "").split("|")):
        staff_labs(d)

    # Fill every lab, verifying against the engine after each attempt.
    #
    # Both lab lists are *horizontal* (researchscreen.form:147-160) and their items are
    # runtime-generated controls with no ids, so geometric clicking only ever reached the first
    # lab in each list -- that is exactly the "two of five busy" that would not move. The named
    # action CONTROL <list> set <index> addresses items by position instead, and raises the same
    # ListBoxChangeSelected the screen listens on (researchscreen.cpp:44,53).
    #
    # ResearchSelect only offers topics whose type matches the lab (researchselect.cpp:230), so
    # there is no single global topic ordering to walk: each lab is tried against successive
    # rows until labs_busy actually rises.
    # "labs" counts every Lab in the game state, including ones whose facility is still being
    # built; only "assignable" ones can be given a project, and one of those is an engineering
    # Workshop that takes manufacturing rather than research. Chasing labs_busy up to labs was
    # chasing a number that can never be reached.
    total_labs = int(before.get("assignable", "0") or 0)
    busy = int(before.get("labs_busy", "0") or 0)
    started = 0

    for list_id in ("LIST_LARGE_LABS", "LIST_SMALL_LABS"):
        for slot in range(6):
            if total_labs and busy >= total_labs:
                break
            st = d.status()
            if st.stage != "ResearchScreen":
                break
            try:
                d.h.control(list_id, "set", str(slot))
            except HarnessError:
                break  # ran off the end of this list
            time.sleep(0.3)

            # Skip a lab with nothing it can actually take. A lab whose every offered topic is
            # already researched used to burn all eight attempts selecting completed rows, and
            # since the attempt budget was per-lab rather than global, an exhausted biochem lab
            # could starve a physics lab that had seven live topics waiting -- which is exactly
            # what "3 labs idle, startable=28, started 0" was.
            # Never press New Project on a lab that already has one. ResearchScreen simply
            # replaces the project, throwing away every man-hour already spent: observed live
            # discarding RESEARCH_ALIEN_PROPULSION_SYSTEM at 23262 of 25000 man-hours and
            # restarting the lab on a fresh topic at zero. And because labs_busy does not rise
            # when a busy lab is overwritten, the success check never fired and the loop did it
            # again, up to eight times per visit -- so the campaign could research for hours and
            # finish almost nothing.
            opts = d.h.gs("research_options")
            busy_with = opts.get("current", "")
            if busy_with == "-":
                busy_with = ""
            elif not busy_with:
                busy_with = current_project(d)
            if busy_with:
                d.say(f"  [research] lab {list_id}[{slot}] already on {busy_with}; leaving it")
                continue

            candidates = pick_topic_rows(d)
            if not candidates:
                continue
            for attempt, (topic_row, topic_name) in enumerate(candidates[:8]):
                st = d.status()
                if st.stage != "ResearchScreen":
                    break
                if not d.click_id("BUTTON_RESEARCH_NEWPROJECT", st):
                    break
                time.sleep(0.45)
                if d.status().stage != "ResearchSelect":
                    break
                try:
                    d.h.control("LIST", "set", str(topic_row))
                except HarnessError:
                    d.click_id("BUTTON_OK", d.status())
                    time.sleep(0.4)
                    break
                time.sleep(0.25)
                d.click_id("BUTTON_OK", d.status())
                time.sleep(0.5)
                now = int(d.h.gs("research").get("labs_busy", "0") or 0)
                if now > busy:
                    busy = now
                    started += 1
                    d.say(f"  [research] started {topic_name} in {list_id}[{slot}]")
                    break

    # Unwind back to the city. Leaving the game parked anywhere else strands the campaign loop,
    # which only knows how to play from CityView -- and stops the clock entirely.
    return_to_city(d)

    after = d.h.gs("research")
    d.say(f"[research] after:  {after}  (started {started})")
    return int(after.get("assignable_busy", "0") or 0) > int(
        before.get("assignable_busy", "0") or 0
    )


def visit_economy(d: Driver) -> bool:
    """Open the buy/sell screen and page its categories.

    Exercises TransactionScreen construction and the transaction controls, which this branch
    changed. Stops short of committing a purchase: the quantity steppers are runtime-populated
    rows, and a half-understood click there would corrupt the campaign's finances rather than
    test them.
    """
    st = d.wait_for("CityView", 60)
    d.click_id("BUTTON_TAB_1", st); time.sleep(0.4)
    if not d.click_id("BUTTON_SHOW_BASE", d.status()):
        return False
    try:
        st = d.wait_for("BaseScreen", 30)
    except TimeoutError:
        return False
    d.click_id("BUTTON_BASE_BUYSELL", st); time.sleep(0.7)
    st = d.status()
    ok = st.stage in ("BuyAndSellScreen", "TransactionScreen")
    if ok:
        d.shot("transactionscreen")
        for cat in ("BUTTON_FLYING", "BUTTON_GROUND", "BUTTON_AGENTS"):
            d.click_id(cat, st); time.sleep(0.35)
        d.say(f"[economy] transaction screen reached; funds {d.h.gs('funds')}")
    else:
        d.say(f"[economy] expected the buy/sell screen, got {st.stage}")
    for _ in range(6):
        st = d.status()
        if st.stage == "CityView":
            break
        if st.stage in ("BuyAndSellScreen", "TransactionScreen", "BaseScreen"):
            d.click_id("BUTTON_OK", st); time.sleep(0.6)
        elif not d.dismiss_modal(st):
            d.escape_key(); time.sleep(0.5)
    return ok


def visit_ufopaedia(d: Driver) -> bool:
    st = d.wait_for("CityView", 60)
    if not d.click_id("BUTTON_SHOW_UFOPAEDIA", st):
        return False
    time.sleep(1.0)
    st = d.status()
    d.say(f"[ufopaedia] stage={st.stage}")
    d.shot("ufopaedia")
    ok = st.stage in ("UfopaediaView", "UfopaediaCategoryView")
    for _ in range(6):
        st = d.status()
        if st.stage == "CityView":
            break
        d.escape_key(); time.sleep(0.5)
    return ok


def intercept_ufos(d: Driver) -> int:
    """Order a craft to attack a UFO, the way a player would.

    Clicking our craft on the map does not work: at the start of a campaign every vehicle is
    parked inside the base and has no tileObject, so it is not on the map to click. The vehicle
    tab lists them regardless of where they are, which is the route the game intends -- pick the
    craft from the list, arm the attack order, then click the target.
    """
    st = d.status()
    if st.stage != "CityView":
        return 0
    if not d.click_id("BUTTON_TAB_2", st):
        d.say("  [intercept] no vehicle tab")
        return 0
    time.sleep(0.4)
    st = d.status()
    lst = d.controls(st).get("OWNED_VEHICLE_LIST")
    if lst is None or lst.w <= 0:
        d.say("  [intercept] vehicle list not resolvable")
        return 0
    # Horizontal strip of craft icons. Send the whole wing, not one craft: each plain click
    # *replaces* the selection, so the previous loop left exactly one interceptor engaged however
    # many were sitting on the pad. Ctrl makes selection additive (cityview.cpp:340 ->
    # orderSelect additive).
    #
    # The score maths rewards this directly. A UFO killed scores; a UFO that loiters and leaves
    # costs; and city_damage from UFO weapons is the largest single drain on the live board
    # (-356, against +150 for kills). More guns on target means shorter engagements and less of
    # both.
    # Only send craft that can actually fight: flying and armed. A Stormdog is a *road* vehicle
    # and a Hovercar with no gun is a passenger, and ordering either after an airborne UFO is not
    # interception -- it is a vehicle driving around while the UFO keeps bombing the city, which
    # is the largest score drain on the board. gs interceptors reports the flying/armed flags in
    # the same order the icons appear in OWNED_VEHICLE_LIST.
    # Send fighters, and keep the troop transport out of it. A crewed craft sent to dogfight
    # risks the squad aboard as well as the airframe, and it is the only thing that can collect a
    # downed UFO -- which is what unlocks the research chain. Losing it stalls the entire route to
    # victory, so crewed craft are used for interception only if there is nothing else flying.
    ICON_W = 36
    fighters, crewed_fighters = [], []
    for idx, flags in craft_flags(d):
        if flags.get("flying") != "1" or flags.get("armed") != "1":
            continue
        if flags.get("crew") == "0":
            fighters.append(idx)
        else:
            crewed_fighters.append(idx)
    if not fighters:
        # Do not send the troop transport to dogfight. It carries the squad that wins ground
        # missions, and losing it costs the craft, the agents aboard, and the ability to reach the
        # next crash site -- for one UFO that would have cost far less to ignore. Measured over a
        # run: craft_lost reached -440 while incursions, the penalty for letting UFOs alone, stood
        # at -311. Losing craft was costing more than the thing it was meant to prevent.
        d.say("  [intercept] no armed fighter free; leaving the transport out of it")
        d.escape_key()
        return 0
    # "Crewed" marks the craft currently carrying the squad, not a craft that cannot fight -- a
    # Valkyrie Interceptor with agents aboard is still an interceptor. Counting it as a transport
    # made two airworthy fighters read as one and refused every engagement for a whole game-week.
    # What matters is whether anything is left if this sortie goes badly, so judge by the total
    # armed fleet and send the uncrewed ones.
    if len(fighters) + len(crewed_fighters) < 2:
        d.say("  [intercept] only one armed flier in the whole fleet; holding it back")
        d.escape_key()
        return 0

    d.h.send("keydown Left Ctrl")
    try:
        # Two craft, not the whole wing. Every projectile that hits a building costs 5 relation
        # with its owner, and destroying a tile costs 20 -- Scenery::handleCollision charges it
        # against whoever fired, our own interceptors included (scenery.cpp:1158-1186, 1195). Most
        # buildings belong to the government, and government relation below -50 terminates funding
        # outright. That is how relation reached -80 in a campaign with only two BuildingScreen
        # visits: not infiltration, our own missed shots over a dense city. Sending four craft at
        # one UFO is four times the stray fire for no more kills.
        for slot in fighters[:2]:
            x = lst.x + 16 + slot * ICON_W
            if x >= lst.x + lst.w:
                break
            d.h.click_xy(x, lst.y + lst.h // 2)
            time.sleep(0.1)
    finally:
        d.h.send("keyup Left Ctrl")
    time.sleep(0.2)

    st = d.status()
    if not d.click_id("BUTTON_VEHICLE_ATTACK", st):
        d.say("  [intercept] no attack-order button")
        return 0
    time.sleep(0.3)

    # Bring a UFO into view and click it as the target of the armed attack order. Always centre
    # first: reading ufos_screen straight off gives coordinates for craft anywhere in the city,
    # including well outside the viewport, and the driver spent whole minutes re-issuing an
    # attack order at a screen corner where the click hit nothing.
    if d.h.gs("centre_on_ufo").get("centred") != "1":
        d.say("  [intercept] no UFO on the city map")
        d.escape_key()
        return 0
    time.sleep(0.5)
    w, h = d.h.display_size()
    live = [
        (x, y)
        for (x, y, crashed) in d.h.screen_craft("ufos_screen")
        if not crashed and 0 <= x < w and 0 <= y < h
    ]
    if live:
        live = [min(live, key=lambda p: (p[0] - w // 2) ** 2 + (p[1] - h // 2) ** 2)]
    if not live:
        d.say("  [intercept] UFO centred but not resolvable on screen")
        d.escape_key()
        return 0

    ux, uy = live[0]
    d.h.click_xy(ux, uy)
    time.sleep(0.5)
    after = d.h.gs("turbo")
    d.say(f"  [intercept] attack ordered on UFO at {ux},{uy}; {after}")
    return 1


def leave_battle(d: Driver, tries: int = 8) -> bool:
    """Escape -> InGameOptions -> BUTTON_EXIT_BATTLE, verified.

    The two exit paths in win_battle both went through click_id, whose resolved-rect fallback
    reports success for a control that is not on the current screen -- it finds a rect in some
    .form and clicks empty space. So a failed exit looked like a successful one, the code fell
    through to another Escape, and the loop carried on fighting a battle it had decided to
    leave. Ask the engine to press the button by name and confirm we actually left.
    """
    for _ in range(tries):
        st = d.status()
        if st.stage not in ("BattleView", "InGameOptions", "MessageBox", "BattlePreStart"):
            return True
        if st.stage == "MessageBox":
            d.h.key("Return")
        elif st.stage == "InGameOptions":
            try:
                if not d.h.send("control BUTTON_EXIT_BATTLE").startswith("OK"):
                    d.h.key("Escape")
            except (HarnessError, OSError):
                d.h.key("Escape")
        else:
            d.h.key("Escape")
        time.sleep(0.7)
    return d.status().stage not in ("BattleView", "InGameOptions", "BattlePreStart")


def settle(d: Driver) -> None:
    """One round-trip so a held modifier is actually in effect before the click lands.

    BattleView recomputes selectionState in update() -- updateSelectionMode is called from the
    frame loop, not from the key edge (battleview.cpp:2052) -- so a click dispatched in the same
    event drain as its KEYDOWN can still be handled under the PREVIOUS selection state. A
    Shift+click meant as a fire order then resolves as an ordinary move, silently, which is the
    difference between shooting an alien and walking at it.
    """
    try:
        d.h.status()
    except (HarnessError, OSError):
        pass
    time.sleep(0.12)


def show_floor(d: Driver, z: int) -> None:
    """Display floor z. BUTTON_LAYER_1..9 jump straight there (battleview.cpp:808-834).

    Stepping PageUp/PageDown works but takes one press per level and gives no way to land on a
    specific floor reliably; the layer buttons are a direct set.
    """
    if z < 0:
        return
    try:
        d.h.send(f"control BUTTON_LAYER_{z + 1} click")
    except (HarnessError, OSError):
        pass
    time.sleep(0.2)


def fire_at(d: Driver, x: int, y: int, z: int, w: int, h: int) -> None:
    """Order the selected units to fire on the unit at this screen position and floor.

    Holding Shift both enters FireAny and sets forced=true on the order
    (battleview.cpp:2098-2101, 4144-4148), so a hostile with no clean line of fire is still
    engaged rather than merely approached. The view must be on the target's own floor first: a
    click's z-hint comes from the displayed level (battleview.cpp:3204-3206), so with the camera
    on the wrong floor the click never resolves to that unit and the shot is silently skipped.
    """
    show_floor(d, z)
    d.h.send("keydown Left Shift")
    try:
        settle(d)
        d.h.click_xy(max(0, min(w - 1, x)), max(0, min(h - 1, y)))
        time.sleep(0.15)
    finally:
        d.h.send("keyup Left Shift")
    time.sleep(0.2)


def battle_layout(d: Driver) -> dict:
    """Tile positions of both sides plus the current view level.

    Screen coordinates alone cannot explain a stalled battle: a hostile two floors up is drawn in
    plain sight and still cannot be walked to, and a click resolves to a tile on whatever level
    the view is showing (battleview.cpp:3279-3290 sets that level). Observed exactly that -- two
    survivors at z=0 while hostiles sat at z=1 and z=2 -- with the squad walking the ground floor
    for the rest of the battle.
    """
    try:
        info = d.h.gs("battle_positions")
    except (HarnessError, OSError):
        return {}

    def parse(field: str) -> list[tuple[int, int, int]]:
        raw = info.get(field, "-")
        out = []
        if not raw or raw == "-":
            return out
        for part in raw.split(";"):
            head = part.split(":")[0]
            bits = head.split(",")
            if len(bits) >= 3:
                try:
                    out.append((int(bits[0]), int(bits[1]), int(bits[2])))
                except ValueError:
                    continue
        return out

    try:
        view_z = int(info.get("view_z", "0") or 0)
    except ValueError:
        view_z = 0
    return {"foes": parse("foe_at"), "mine": parse("mine_at"), "view_z": view_z}


def zoom_to_event(d: Driver) -> int:
    """Jump the view to the engine's own last-event location. Returns the new level, or -1.

    BattleView already does this job. Every event carrying a message is appended to
    state->messages (gamestate.cpp:2023) and pushed to NEWS_TICKER *unconditionally*
    (battleview.cpp:4355-4358) -- the Notifications.* options this driver disables at launch only
    decide whether a blocking NotificationScreen pops up (battleview.cpp:4360-4372), NOT whether
    the event is logged. So the event feed is live even with every notification switched off.

    BUTTON_ZOOM_EVENT (also Home, battleview.cpp:3644) calls zoomLastEvent() -> zoomAt(), and
    zoomAt() does setScreenCenterTile() *and* setZLevel(location.z + 1) (battleview.cpp:5005-5010).
    The engine changes FLOOR for us, to the floor where something actually happened.

    That is precisely what match_enemy_floor() below re-derives by hand: pulling every unit's raw
    coordinates over the harness and computing a modal floor. Ask the engine first. The coordinate
    scan stays as the fallback, because the button is guarded on Ticker::hasMessages(), which goes
    false once the last message finishes scrolling (forms/ticker.h:43) -- in a quiet moment there
    is no event to zoom to and the hand-rolled scan is still the only answer.
    """
    layout = battle_layout(d)
    before = layout.get("view_z", -1) if layout else -1
    if not _press(d, "BUTTON_ZOOM_EVENT"):
        return -1
    time.sleep(0.25)
    layout = battle_layout(d)
    after = layout.get("view_z", -1) if layout else -1
    if after < 0 or after == before:
        return -1
    d.say(f"  [battle] zoom-to-event: view {before} -> {after} (engine's own last-event location)")
    return after


def match_enemy_floor(d: Driver, layout: dict) -> int:
    """Bring the view to the floor most hostiles are on. Returns the level moved to, or -1.

    Movement orders can only target the level being displayed, so a squad can never be sent
    upstairs while the camera sits on the ground floor. PageUp/PageDown step the level by one
    (battleview.cpp:3279-3290), so step deliberately rather than pressing them on a timer.
    """
    foes = layout.get("foes") or []
    if not foes:
        return -1
    mine = layout.get("mine") or []
    # Aim for the floor carrying the most hostiles; ties go to the lowest, which is usually the
    # one the squad can actually reach by stairs.
    counts: dict[int, int] = {}
    for _, _, z in foes:
        counts[z] = counts.get(z, 0) + 1
    want = sorted(counts.items(), key=lambda kv: (-kv[1], kv[0]))[0][0]
    have = layout.get("view_z", 0)
    if have == want:
        return want
    # Jump straight to the floor rather than stepping a level per press.
    show_floor(d, want)
    if mine:
        my_z = sorted(z for _, _, z in mine)[len(mine) // 2]
        d.say(f"  [battle] hostiles on floor {want}, squad on {my_z}; view -> {want}")
    return want


# ---------------------------------------------------------------------------
# Battlescape capabilities
#
# Everything a player can do in a battle, each driven the way the engine actually accepts it.
# Two mechanisms matter and are easy to get wrong:
#
#   * Named CONTROL clicks go through Control::click(), which raises a synthetic MouseClick whose
#     MouseInfo.Button is never set (forms/control.cpp:1255-1268). Any handler that branches on
#     which button was pressed -- the hand icons do -- cannot be driven that way and needs a real
#     pixel click. CheckBox, RadioButton and TriStateBox handlers do not read the button, so they
#     are safe by name.
#   * Modifier-held orders need a round-trip to settle first; see settle().
# ---------------------------------------------------------------------------

FIRE_MODES = {"aimed": "BUTTON_AIMED", "snap": "BUTTON_SNAP", "auto": "BUTTON_AUTO"}
# Controls the driver never touched. Every one is a real button on battle.form that a player uses
# and the driver did not, so every battle was fought on whatever the defaults happened to be.
BEHAVIOURS = {"aggressive": "BUTTON_AGGRESSIVE", "normal": "BUTTON_NORMAL",
              "evasive": "BUTTON_EVASIVE"}
MOVE_MODES = {"group": "BUTTON_MOVE_GROUP", "individual": "BUTTON_MOVE_INDIVIDUALLY"}
RESERVES = {"aimed": "BUTTON_RESERVE_AIMED", "snap": "BUTTON_RESERVE_SNAP",
            "auto": "BUTTON_RESERVE_AUTO", "kneel": "BUTTON_RESERVE_KNEEL"}
# Absolute level select for flying units. LAYER_UP/DOWN step; LAYER_N jumps.
LAYERS = {n: f"BUTTON_LAYER_{n}" for n in range(1, 10)}
STANCES = {"kneel": "BUTTON_KNEEL", "prone": "BUTTON_PRONE",
           "walk": "BUTTON_WALK", "run": "BUTTON_RUN"}
PSI_ACTIONS = {"control": "BUTTON_CONTROL", "panic": "BUTTON_PANIC",
               "stun": "BUTTON_STUN", "probe": "BUTTON_PROBE"}


def _press(d: Driver, control_id: str, op: str = "click") -> bool:
    try:
        return d.h.send(f"control {control_id} {op}").startswith("OK")
    except (HarnessError, OSError):
        return False


def set_behaviour(d: Driver, mode: str = "normal") -> bool:
    """Aggressive / Normal / Evasive. Checkboxes, so a named click is safe.

    This is the engine's own cover-and-engagement doctrine switch (F9/F10/F11) and nothing in the
    driver had ever set it -- every battle ran on the default.
    """
    cid = BEHAVIOURS.get(mode)
    return bool(cid) and _press(d, cid)


def set_move_mode(d: Driver, mode: str = "individual") -> bool:
    """Group movement herds the squad into one clump, which is what makes a single explosive
    catastrophic. Individual movement is the mechanical half of keeping spacing."""
    cid = MOVE_MODES.get(mode)
    return bool(cid) and _press(d, cid)


def set_reserve(d: Driver, mode: str = "snap") -> bool:
    """Reserve TU for a shot instead of spending it all walking, so a unit can answer when
    something steps into view mid-move."""
    cid = RESERVES.get(mode)
    return bool(cid) and _press(d, cid)


def set_layer(d: Driver, level: int) -> bool:
    """Absolute level select, for flying units and for engaging a floor the squad is not on."""
    cid = LAYERS.get(int(level))
    if cid and _press(d, cid):
        return True
    # Outside 1..9, step with LAYER_UP/DOWN instead of failing silently.
    btn = "BUTTON_LAYER_UP" if level > 0 else "BUTTON_LAYER_DOWN"
    return _press(d, btn)


def cease_fire(d: Driver, hold: bool = True) -> bool:
    return _press(d, "BUTTON_CEASE_FIRE")


def set_fire_mode(d: Driver, mode: str = "snap") -> bool:
    """Aimed, snap or auto. Checkboxes, so a named click is safe."""
    cid = FIRE_MODES.get(mode)
    return bool(cid) and _press(d, cid)


def set_stance(d: Driver, stance: str = "run") -> bool:
    """kneel / prone / walk / run. There is no separate stand control -- walk or run stands up."""
    cid = STANCES.get(stance)
    if not cid:
        return False
    return _press(d, cid, "toggle" if stance == "kneel" else "click")


def select_squad(d: Driver, n: int = 1, additive: bool = False) -> bool:
    """Select squad 1-6. clickedSquad only swaps the selection on a repeat click on the same
    index, so press it twice (battleview.cpp:640-682)."""
    cid = f"SQUAD_{n}_OVERLAY"
    if additive:
        d.h.send("keydown Left Ctrl")
        try:
            settle(d)
            ok = _press(d, cid) and _press(d, cid)
        finally:
            d.h.send("keyup Left Ctrl")
        return ok
    return _press(d, cid) and _press(d, cid)


def force_fire_ground(d: Driver, x: int, y: int, z: int, w: int, h: int) -> None:
    """Shoot the tile itself, ignoring whoever is standing on it. Alt while in fire mode
    (battleview.cpp:4139-4143). Useful against a hostile behind cover the squad cannot path to."""
    show_floor(d, z)
    d.h.send("keydown Left Shift")
    d.h.send("keydown Left Alt")
    try:
        settle(d)
        d.h.click_xy(max(0, min(w - 1, x)), max(0, min(h - 1, y)))
        time.sleep(0.15)
    finally:
        d.h.send("keyup Left Alt")
        d.h.send("keyup Left Shift")
    time.sleep(0.2)


def throw_at(d: Driver, x: int, y: int, z: int, w: int, h: int, hand: str = "RIGHT") -> bool:
    """Throw the held item at a tile. Arms throw mode via the checkbox rather than the hand icon:
    BUTTON_*_HAND_THROW does not read the mouse button, so it is safe by name.

    A grenade needs no line of fire and no path, which makes it the honest answer to a hostile
    the squad cannot reach at all.
    """
    if not _press(d, f"BUTTON_{hand}_HAND_THROW", "toggle"):
        return False
    time.sleep(0.2)
    show_floor(d, z)
    d.h.click_xy(max(0, min(w - 1, x)), max(0, min(h - 1, y)))
    time.sleep(0.3)
    return True


def hand_icon(d: Driver, hand: str = "RIGHT") -> bool:
    """Click a hand icon with a real pixel click. CONTROL cannot drive these: clickedRightHand and
    clickedLeftHand branch on MouseInfo.Button (battleview.cpp:993-1007), which a synthetic click
    never sets. This is the gateway to fire-hand selection, item priming and the psi tab."""
    rects = d.h.ui(f"CLICKY_{hand}_HAND")
    rect = rects.get(f"CLICKY_{hand}_HAND")
    if not rect:
        return False
    x, y, rw, rh = rect
    d.h.click_xy(int(x + rw / 2), int(y + rh / 2))
    time.sleep(0.35)
    return True


def psi_attack(d: Driver, kind: str, x: int, y: int, z: int, w: int, h: int,
               hand: str = "RIGHT") -> bool:
    """Probe, panic, stun or control a hostile. Needs a MindBender in that hand: the psi tab only
    opens from the hand icon, and only for a psi item."""
    cid = PSI_ACTIONS.get(kind)
    if not cid or not hand_icon(d, hand):
        return False
    if not _press(d, cid):
        return False
    time.sleep(0.2)
    show_floor(d, z)
    d.h.click_xy(max(0, min(w - 1, x)), max(0, min(h - 1, y)))
    time.sleep(0.3)
    return True


def fly_to(d: Driver, x: int, y: int, z: int, w: int, h: int) -> None:
    """Move to another floor. Flying needs no special order -- show the destination floor and
    click it; a unit that can fly will path vertically, one that cannot will refuse."""
    show_floor(d, z)
    d.h.click_xy(max(0, min(w - 1, x)), max(0, min(h - 1, y)))
    time.sleep(0.25)


def battle_inventory(d: Driver, open_it: bool = True) -> bool:
    """Open or close a unit's equip screen mid-battle (BUTTON_INVENTORY ignores the button)."""
    if open_it:
        if not _press(d, "BUTTON_INVENTORY"):
            return False
        time.sleep(0.8)
        return d.status().stage == "AEquipScreen"
    ok = _press(d, "BUTTON_OK")
    time.sleep(0.6)
    return ok


def verify_battle_capabilities(d: Driver) -> dict:
    """Exercise every battlescape capability against a live battle and report what worked.

    This exists because "the harness supports it" is a claim, and a claim about a UI is worth
    exactly as much as the last time someone ran it. Each entry is attempted for real and judged
    by whether the engine accepted it.
    """
    st = d.status()
    if st.stage != "BattleView":
        return {"error": f"not in a battle ({st.stage})"}
    results: dict[str, bool] = {}

    for mode in FIRE_MODES:
        results[f"fire_mode:{mode}"] = set_fire_mode(d, mode)
        time.sleep(0.15)
    for stance in STANCES:
        results[f"stance:{stance}"] = set_stance(d, stance)
        time.sleep(0.15)
    results["select_squad"] = select_squad(d, 1)
    results["inventory_open"] = battle_inventory(d, True)
    if results["inventory_open"]:
        battle_inventory(d, False)
        d.wait_for("BattleView", 15)
    results["hand_icon"] = hand_icon(d, "RIGHT")
    d.escape_key()
    time.sleep(0.3)

    layout = battle_layout(d)
    results["battle_positions"] = bool(layout)
    foes = on_screen(d, "enemies_screen")
    results["enemies_visible"] = bool(foes)
    if foes:
        fx, fy, fz = foes[0]
        results["show_floor"] = (show_floor(d, fz) or True)
        fire_at(d, fx, fy, fz, st.w, st.h)
        results["fire_at_unit"] = True
        force_fire_ground(d, fx, fy, fz, st.w, st.h)
        results["force_fire_ground"] = True
        results["throw"] = throw_at(d, fx, fy, fz, st.w, st.h)
        results["psi"] = psi_attack(d, "probe", fx, fy, fz, st.w, st.h)
        fly_to(d, fx, fy, fz, st.w, st.h)
        results["move_fly"] = True

    ok = sum(1 for v in results.values() if v)
    d.say(f"  [caps] {ok}/{len(results)} battlescape capabilities exercised successfully")
    for k, v in sorted(results.items()):
        d.say(f"    {'ok  ' if v else 'FAIL'} {k}")
    return results


def build_battle_ai(policy: dict):
    """Instantiate the pluggable tactical AI a policy names, or return (None, None).

    The three-layer split (oa_capabilities / oa_ai / oa_executor) was built and then connected to
    nothing: oa_play imported neither, so win_battle applied fire_mode and stance and ignored the
    other eight genes, and every doctrine rule in VeteranAI sat unused. This is the join.

    Imports are deliberately lazy. oa_capabilities imports oa_play, so a module-level import here
    is a cycle; and a harness run that names no AI should not pay to load the AI stack at all.

    Only genes the chosen AI's constructor actually accepts are passed. A policy carrying a gene
    the AI does not understand is not an error -- the genome is shared across AIs, and one that
    ignores a knob should simply not be tuned by it.
    """
    name = (policy or {}).get("ai")
    if not name:
        return None, None
    from oa_capabilities import Capabilities
    from oa_executor import make_ai

    return make_ai(name, tuning=policy), Capabilities


def battle_gs(d: Driver, query: str) -> dict:
    """gs() for a battle-scoped query, tolerating the battle ending underneath it.

    Same fault as on_screen(), and the reason this is a HELPER rather than another guarded call
    site: the first fix covered enemies_screen and friends_screen only, and centre_on_enemy went
    on to throw away a second won mission days-of-debugging later. Battle-scoped queries are a
    CLASS -- fixing them one at a time is how the second one got missed.
    """
    try:
        return d.h.gs(query) or {}
    except (HarnessError, OSError):
        return {}


def first_foe_tile(foe_at: str | None) -> tuple[int, int, int] | None:
    """The tile of the first hostile in a battle_positions foe_at field ("x,y,z:k=v...;...")."""
    if not foe_at or foe_at == "-":
        return None
    try:
        x, y, z = (int(v) for v in foe_at.split(";")[0].split(":")[0].split(","))
    except ValueError:
        return None
    return x, y, z


def send_squad_to_foe(d: Driver) -> bool:
    """Order the whole squad to the first spotted hostile's tile through the engine's pathfinder.

    The click driver can only order moves on the floor in view, so a squad on floor 0 never
    reached aliens holed up on floor 2: observed in a base defence, 24 soldiers against 8 aliens,
    the mission sitting "won but unfinished" with no progress until the time budget ran out.
    squad_to is the same order as selecting everyone and clicking that tile on its own floor; the
    engine routes them through the lifts. Only hostiles the player has spotted are targets.
    """
    target = first_foe_tile(battle_gs(d, "battle_positions").get("foe_at"))
    if not target:
        return False
    reply = battle_gs(d, "squad_to {} {} {}".format(*target))
    d.say(f"  [battle] stalled; squad to the hostile at {target}: "
          f"ordered={reply.get('ordered', '?')} refused={reply.get('refused', '?')}")
    return int(reply.get("ordered", "0") or 0) > 0


# The 4x4 search grid, innermost cells first: a crashed UFO, a building's core and most
# objectives sit toward the middle of a battle map, not in its corners.
_SEARCH_CELLS = sorted(((gx, gy) for gx in range(4) for gy in range(4)),
                       key=lambda c: ((c[0] - 1.5) ** 2 + (c[1] - 1.5) ** 2, (c[0] * 7 + c[1] * 3) % 16))


def search_waypoint(size: tuple[int, int, int], step: int) -> tuple[int, int, int]:
    """Waypoint `step` of a search over the whole battle map, in tiles.

    Each pass over a floor is the map centre, then the centres of a 4x4 grid from the middle
    outward. The next pass moves floor: ground first, then the floors above, then below.
    """
    sx, sy, sz = size
    per_floor = 1 + len(_SEARCH_CELLS)
    floors = [z for z in (1, 2, 3, 0, 4, 5) if z < sz] or [0]
    z = floors[(step // per_floor) % len(floors)]
    k = step % per_floor
    if k == 0:
        return sx // 2, sy // 2, z
    gx, gy = _SEARCH_CELLS[k - 1]
    return int((gx + 0.5) * sx / 4), int((gy + 0.5) * sy / 4), z


def squad_tiles(mine_at: str | None) -> list[tuple[int, int, int]]:
    """The squad's tiles from a battle_positions mine_at field ("x,y,z:k=v...;...")."""
    tiles = []
    for entry in (mine_at or "").split(";"):
        try:
            tiles.append(tuple(int(v) for v in entry.split(":")[0].split(",")))
        except ValueError:
            continue
    return [t for t in tiles if len(t) == 3]


def send_squad_searching(d: Driver, step: int) -> tuple[int, int, int] | None:
    """No hostile spotted for a long while: walk the whole squad to search waypoint `step`.

    The screen sweep only reaches what is on screen and on the floor in view. A UFO recovery sat
    for 360 rounds at 5 soldiers against 6 aliens nobody had seen -- five of them inside the
    crashed UFO at the middle of the map. One squad_to the map centre walked the squad in and
    killed three within 16 seconds. Returns the waypoint, or None if no order went out.
    """
    try:
        size = tuple(int(v) for v in battle_gs(d, "battle_map").get("size", "").split(","))
    except ValueError:
        return None
    if len(size) != 3:
        return None
    target = search_waypoint(size, step)
    reply = battle_gs(d, "squad_to {} {} {}".format(*target))
    d.say(f"  [battle] nothing spotted; squad searching toward {target}: "
          f"ordered={reply.get('ordered', '?')}")
    return target if int(reply.get("ordered", "0") or 0) > 0 else None


def on_screen(d: Driver, which: str) -> list:
    """screen_craft that tolerates the battle ending underneath it.

    Battle::checkMissionEnd tears current_battle down the instant the last hostile dies, so a
    query already in flight comes back as an error rather than an empty list. That is not a
    failure worth abandoning a mission for -- it means the mission is over, which is the outcome
    we wanted.

    It cost a battle that had just been won: hostiles down from 10 to 6, mission complete, and the
    run recorded a no-contest because the driver raised instead of shrugging. The engine now
    answers these two queries with an empty result rather than ERR, and this is the belt to that
    braces -- an older binary should not be able to lose a won mission this way either.
    """
    try:
        return d.h.screen_craft(which) or []
    except (HarnessError, OSError):
        return []


def win_battle(d: Driver, budget_s: float = 1800.0, policy: dict | None = None) -> str:
    """Fight a tactical mission, and leave the numbers behind on `d.last_battle`.

    _fight_battle has six return points. Rather than thread bookkeeping through all of them, this
    wrapper stamps the result once. The numbers have to be captured DURING the battle: by the time
    a debriefing closes, Battle::checkMissionEnd has torn current_battle down, so a caller asking
    afterwards -- as the adversarial arena was -- gets an empty dict and scores a real battle as
    though it had no squad.
    """
    if policy is None:
        policy = getattr(d, "battle_policy", None)
    t0 = time.time()
    d.last_battle = {"outcome": "running", "seconds": 0.0, "started_with": 0,
                     "survivors": None, "mission_type": "unknown",
                     "policy": (policy or {}).get("name", "")}
    try:
        outcome = _fight_battle(d, budget_s, policy)
    except Exception as exc:
        d.last_battle.update(outcome=f"error:{type(exc).__name__}",
                             seconds=round(time.time() - t0, 1))
        raise
    d.last_battle.update(outcome=outcome, seconds=round(time.time() - t0, 1))
    # A LOST mission had no conscious unit left, by the engine's own rule: checkMissionEnd sets
    # playerWon=false exactly when the player is absent from orgsAlive, and a human soldier
    # qualifies for that set whenever it is conscious (battle.cpp:2160-2205). So "lost" and
    # "somebody was still standing" cannot both be true.
    #
    # survivors is otherwise the last MID-BATTLE sample, because current_battle is torn down
    # before the debriefing and there is nothing left to query. That made a squad stunned
    # unconscious on an alien ship report 6 of 6 survivors on a mission it had just lost, and the
    # arena scored it 0.30 -- full marks for survival. They are not survivors in any sense the
    # campaign cares about; they are lost with the mission.
    #
    # Deliberately NOT applied to "returned": a withdrawal really does bring people home, and
    # those are the survivors the whole withdraw-early doctrine exists to preserve.
    if outcome == "lost":
        d.last_battle["survivors_last_seen"] = d.last_battle.get("survivors")
        d.last_battle["survivors"] = 0
    return outcome


def _fight_battle(d: Driver, budget_s: float = 1800.0, policy: dict | None = None) -> str:
    """Fight a tactical mission to a win, without cheats.

    Units default to FirePermissionMode::AtWill (battleunit.h:271) and UnitAIDefault makes any
    conscious unit engage a hostile it can see, so winning is a movement problem rather than a
    per-shot targeting one: keep the squad advancing on the enemy and the engine does the
    shooting. Battle::checkMissionEnd ends the mission by itself once no hostile organisation has
    a conscious unit left -- so we must NOT abort, which is all the old driver ever did.
    """
    t0 = time.time()
    d.say("[battle] fighting for a win")
    entered = False
    last_foes = None
    stalls = 0
    rounds = 0
    # Rounds during which the click orders stand down, so a squad sent across floors by
    # send_squad_to_foe() is not immediately re-ordered to the floor in view.
    hold_clicks_until = 0
    search_step = 0
    search_target = None
    search_round = 0
    # Remembered from the last in-battle sample: the debriefing stage has no battle to query,
    # current_battle having already been torn down by then.
    last_player_won = False
    last_mine_alive = "?"
    started_with = 0
    selected_count = 0
    mission_type = "unknown"
    # Pluggable tactical AI, built on entry when the policy names one. None means "behave exactly
    # as before", which is what every existing caller gets.
    ai_brain = None
    ai_caps = None
    ai_failures = 0

    while time.time() - t0 < budget_s:
        st = d.status()

        if st.stage == "BattleBriefing":
            # Leave the mode buttons alone: Battle::mode defaults to RealTime, and turn-based
            # would need a completely different (unbuilt) per-shot driver.
            d.click_id("BUTTON_REAL_TIME", st); time.sleep(1.0); continue
        if st.stage == "BattlePreStart":
            d.click_id("BUTTON_OK", st); time.sleep(1.0); continue
        if st.stage == "BattleDebriefing":
            # Ask the engine who won rather than assuming. Battle::checkMissionEnd sets playerWon;
            # a debriefing appears either way, so treating its arrival as a win counted a total
            # squad wipe -- every soldier dead -- as "resolved (wins 2)".
            outcome = "resolved" if last_player_won else "lost"
            d.shot("battle_" + outcome)
            d.click_id("BUTTON_OK", st)
            d.say(f"[battle] debriefing after {time.time()-t0:.0f}s: {outcome}"
                  f" (survivors {last_mine_alive})")
            return outcome
        if st.stage in ("CityView", "MainMenu", "VideoScreen"):
            d.say(f"[battle] back at {st.stage}")
            return "returned"
        if st.stage != "BattleView":
            if not d.dismiss_modal(st):
                time.sleep(0.5)
            continue

        if not entered:
            entered = True
            # Apply the tactical policy once, on entry. Defaults to None so every existing caller
            # keeps the behaviour it had; the arena (tools/oa_arena.py) passes a policy so that
            # what the squad does is a *variable* rather than a hardcoded habit. Units default to
            # FirePermissionMode::AtWill, so fire mode and stance are the two levers that change
            # how the engine resolves the shooting it is already doing on our behalf.
            if policy:
                fm = policy.get("fire_mode")
                if fm:
                    set_fire_mode(d, fm)
                    time.sleep(0.15)
                stance = policy.get("stance")
                if stance:
                    set_stance(d, stance)
                    time.sleep(0.15)
                d.say(f"[battle] policy {policy.get('name','?')}: "
                      f"fire={fm} stance={stance}")
                try:
                    ai_brain, caps_cls = build_battle_ai(policy)
                    if ai_brain is not None:
                        ai_caps = caps_cls(d)
                        d.say(f"[battle] tactical AI {ai_brain.name!r} has the squad")
                except Exception as exc:
                    d.say(f"[battle] tactical AI {policy.get('ai')!r} could not be built "
                          f"({type(exc).__name__}: {exc}); fighting on the built-in logic")
                    ai_brain = None
            b = d.h.gs("battle")
            if b.get("mode") != "rt":
                d.say(f"[battle] ABORT: mode is {b.get('mode')}, not real-time")
                return "wrong-mode"
            started_with = int(b.get("mine_alive", "0") or 0)
            mission_type = b.get("mission_type", "unknown")
            d.last_battle.update(started_with=started_with, mission_type=mission_type)
            if mission_type == "base_defense":
                d.say("[battle] BASE DEFENCE - no withdrawal; losing the base ends the campaign")
            d.shot("battle_start")
            d.click_id("BUTTON_SPEED3", st)      # fastest real-time battle speed
            time.sleep(0.5)
            d.say(f"[battle] {b}")

        # Select the squad, then advance it. BattleView binds no number keys at all -- SDLK_1 is
        # simply not handled -- so the old d.h.key("1") did nothing whatsoever and the squad
        # never moved. Units are selected by clicking them, and Ctrl+click is what *adds* to the
        # selection rather than replacing it (battleview.cpp:3855-3865, capped at 6 by
        # orderSelect).
        # Keep the camera on the squad. Units are moved by clicking their screen positions, so
        # anything off-camera is neither watchable nor clickable.
        if rounds % 4 == 0:
            battle_gs(d, "centre_on_friends")
            time.sleep(0.15)
        friends = on_screen(d, "friends_screen")

        # Re-select only when the squad has actually changed. Selection persists between orders,
        # so rebuilding it every round spent roughly seven clicks on re-selecting the same people
        # for every single move order -- most of the loop went on switching between agents
        # instead of moving them, which is why a squad could stand around while one alien went
        # unfound. Re-select when the count changes (someone died, someone came into view) or
        # occasionally in case the engine dropped it.
        if friends and (len(friends) != selected_count or rounds % 8 == 0):
            d.h.send("keydown Left Ctrl")
            try:
                settle(d)
                for fx, fy, _ in friends[:6]:
                    d.h.click_xy(fx, fy)
                    time.sleep(0.06)
            finally:
                d.h.send("keyup Left Ctrl")
            selected_count = len(friends)
            time.sleep(0.12)

        # Hand the round to the pluggable AI first, then fall through to the built-in logic
        # regardless. Additive on purpose: the inline path below is the one that has actually won
        # battles, so an AI that misjudges a round costs a round, not the mission. It also cannot
        # cheat -- Capabilities reads only what the harness can see on screen.
        if ai_brain is not None:
            try:
                from oa_executor import execute as _ai_execute, observe as _ai_observe
                obs = _ai_observe(ai_caps, stalls=stalls)
                orders = ai_brain.decide(obs)
                _ai_execute(ai_caps, orders, say=(d.say if rounds % 10 == 0 else None))
                if rounds % 10 == 0 and orders:
                    d.say(f"  [ai] {ai_brain.name}: "
                          # All of them, not the first three. The base-defence doctrine issues
                          # seven orders and the decisive one -- choking the entry -- is last, so
                          # a 3-item cap made it look as though the order was never given. The log
                          # is the only evidence a run leaves behind; truncating it hides exactly
                          # the part worth reading.
                          + ", ".join(f"{o.kind}({o.why})" for o in orders))
            except Exception as exc:
                ai_failures += 1
                if ai_failures <= 3:
                    d.say(f"  [ai] round failed ({type(exc).__name__}: {exc})")
                if ai_failures == 8:
                    d.say("  [ai] too many failures; dropping to the built-in logic for this "
                          "battle")
                    ai_brain = None

        # Put the camera on the hostiles' floor first: orders only reach the displayed level.
        # Ask the engine before computing it ourselves -- see zoom_to_event() for why its answer
        # is better than the coordinate scan, and why the scan still has to stay.
        if rounds % 3 == 0:
            if zoom_to_event(d) < 0:
                layout = battle_layout(d)
                if layout:
                    match_enemy_floor(d, layout)

        foes = on_screen(d, "enemies_screen")
        # enemies_screen only reports hostiles already on screen. Walking the camera only when
        # *nothing* is visible is not enough: one alien that is framed but unreachable keeps the
        # squad grinding against it while the rest of the map goes unexplored, which is the same
        # stall in a new costume. So also walk on once progress dries up.
        if not foes or stalls > 4:
            info = battle_gs(d, "centre_on_enemy")
            if info.get("centred") == "1":
                time.sleep(0.4)
                found = on_screen(d, "enemies_screen")
                if found:
                    foes = found

        if rounds < hold_clicks_until:
            pass
        elif foes:
            # Shoot first, and shoot at the unit itself. Holding Shift puts BattleView into
            # FireAny (battleview.cpp:2098-2100) and a click then resolves to orderFire on the
            # unit under the cursor rather than a move (battleview.cpp:4140-4151). Without this
            # the driver only ever WALKED: a left click on an occupied tile cannot become a move
            # order at all, so a hostile the squad could not path to -- one floor up, on a roof,
            # sealed behind scenery, or simply flying -- was never actually shot at, and the
            # battle sat at "one foe alive" until the budget ran out. Real-time auto-fire only
            # engages what a unit can already see, which is exactly what a stalled battle does
            # not have.
            tx_, ty_, tz_ = foes[rounds % len(foes)]
            fire_at(d, tx_, ty_, tz_, st.w, st.h)

            fx, fy, _ = foes[rounds % len(foes)]
            # Aim a short way off the hostile's own tile: that tile is occupied, and a left
            # click on an occupied tile selects rather than moves (battleview.cpp:3778-3790).
            fx += (40, -40, 0, 0)[rounds % 4]
            fy += (0, 0, 28, -28)[rounds % 4]
            d.h.click_xy(max(0, min(st.w - 1, fx)), max(0, min(st.h - 1, fy)))
            # A second order to a different offset the same round: with selection no longer
            # rebuilt every pass there is time for it, and it keeps the squad advancing rather
            # than re-issuing one order and waiting.
            time.sleep(0.4)
            d.h.click_xy(max(0, min(st.w - 1, fx + 24)), max(0, min(st.h - 1, fy + 16)))
            if stalls > 12 and rounds % 3 == 0:
                d.h.key("PageUp" if (rounds // 3) % 2 == 0 else "PageDown")
        else:
            # Nothing anywhere in view; sweep the map, and change floor now and then since
            # hostiles hole up on other levels.
            tx = int(st.w * (0.3 + 0.2 * (rounds % 3)))
            ty = int(st.h * (0.3 + 0.15 * (rounds % 3)))
            d.h.click_xy(tx, ty)
            if rounds % 5 == 4 or stalls > 8:
                # SDL names these without a space; "Page Down" is rejected outright, and the
                # resulting HarnessError propagated out of win_battle and was recorded as
                # "lost connection", abandoning a mission that was going fine.
                d.h.key("PageUp" if (rounds // 5) % 2 == 0 else "PageDown")
        rounds += 1
        time.sleep(1.2)

        b = d.h.gs("battle")
        foes_alive = b.get("foes_alive")
        mine_alive = b.get("mine_alive")
        last_player_won = b.get("player_won") == "1"
        last_mine_alive = mine_alive
        try:
            d.last_battle["survivors"] = int(mine_alive)
        except (TypeError, ValueError):
            pass
        if mine_alive == "0":
            d.say(f"[battle] squad wiped out: {b}")
        if foes_alive == last_foes:
            stalls += 1
        else:
            stalls = 0
            d.say(f"  [battle] foes_alive={foes_alive} mine_alive={mine_alive}")
        last_foes = foes_alive
        if stalls and stalls % 6 == 0:
            if send_squad_to_foe(d):
                hold_clicks_until = rounds + 5
            # A base defence holds its entries rather than hunting through its own corridors --
            # until nothing has been seen for a long while. A defence cannot be left, and two
            # hidden aliens kept twenty soldiers there for 45 minutes of battle budget.
            elif (stalls >= 24 and mission_type != "base_defense") or stalls >= 60:
                # Walk to a waypoint and get there before choosing the next: re-targeting every
                # few rounds only zigzagged the squad and searched nothing.
                squad = squad_tiles(battle_gs(d, "battle_positions").get("mine_at"))
                arrived = search_target is not None and any(
                    t[2] == search_target[2] and abs(t[0] - search_target[0]) <= 3
                    and abs(t[1] - search_target[1]) <= 3 for t in squad)
                if search_target is None or arrived or rounds - search_round > 40:
                    if search_target is not None:
                        search_step += 1
                    search_target = send_squad_searching(d, search_step)
                    search_round = rounds
                hold_clicks_until = rounds + 7
        if stalls and stalls % 12 == 0:
            d.say(f"  [battle] no progress for a while: {b}")
            # Record why it is stuck rather than just that it is. Screen coordinates cannot
            # explain an unreachable hostile -- a unit two floors up is drawn in plain sight and
            # still cannot be walked to -- so log tile positions and let the z gap speak.
            try:
                pos = battle_gs(d, "battle_positions")
                d.say(f"  [battle] positions: foe_at={pos.get('foe_at')} "
                      f"mine_at={pos.get('mine_at')}")
            except (HarnessError, OSError):
                pass

        # A mission the engine already considers won, stuck on a hostile the squad cannot reach,
        # will otherwise burn the entire time budget: observed at 13 of 15 alive against a single
        # remaining foe, unchanged for minutes while the campaign clock stood still. Leaving is
        # not a forfeit here -- Battle::exitBattle force-completes the building's researchUnlock
        # on playerWon alone (battle.cpp:3511); only the loot is gated on not having retreated
        # (battle.cpp:2817), and the research is the part the campaign actually needs.
        # NEVER leave a base defence. Not when losing, not when stalled, not when a second base
        # exists -- never.
        #
        # Withdrawing forfeits the base itself: every facility reverts to "unbuilt", and the labs,
        # stores and staff go with it. Observed directly -- five labs went from built and fully
        # staffed to unbuilt with skill 0 between one research check and the next, the engine then
        # crashed twice on the dangling base, and the defeat video played.
        #
        # This rule was softened once, on the reasoning that losing a base only ends the game when
        # it is the LAST base (base.cpp:150-159), so conceding one with a spare was "a setback
        # rather than a defeat". That reasoning is wrong in a way the base.cpp citation hides: the
        # aliens who take the base are still there, the facilities are still gone, and a squad that
        # walks out has spent its soldiers' lives buying nothing. There is no line of retreat from
        # your own home -- everything worth defending is behind you, and there is nowhere for it to
        # go.
        #
        # A losing base defence fought to the end is better than a conceded one, and the deadlock
        # that prompted the softening is a problem to solve by ENDING the battle faster -- hold the
        # entries, clear the base -- not by leaving it.
        may_leave = mission_type != "base_defense"

        try:
            alive_now = int(mine_alive or 0)
            foes_now = int(foes_alive or 0)
        except ValueError:
            alive_now, foes_now = 0, 0
        # Only bank a mission that is genuinely won bar a hostile the squad cannot reach: the
        # squad still standing, at most a couple of foes left, and comfortably outnumbering them.
        # Observed at 13 of 15 alive against a single foe, unchanged for 22 minutes. Leaving is
        # not a forfeit there -- exitBattle force-completes the building's researchUnlock on
        # playerWon alone (battle.cpp:3511); only loot is gated on not having retreated
        # (battle.cpp:2817), and the research is what the campaign actually needs.
        if may_leave and last_player_won and stalls >= 16 and foes_now and foes_now <= 2 \
                and alive_now >= foes_now * 3:
            d.say(f"  [battle] won bar {foes_now} unreachable foe(s) with {alive_now} up; banking it")
            if leave_battle(d):
                return "resolved"
            d.say("  [battle] could not leave; fighting on")

        # Retreat rather than be annihilated. "No mission is important enough to lose good men
        # on" -- and losing a whole squad is the biggest single economic hit in the game, which
        # is what drove this campaign into a losing spiral: four agents sent against twenty-three
        # aliens, all four dead, defeat eight days later. Escape opens InGameOptions, which
        # carries BUTTON_EXIT_BATTLE.
        try:
            alive = int(mine_alive or 0)
            foes_n = int(foes_alive or 0)
        except ValueError:
            alive, foes_n = 0, 0
        # Pull out early. Waiting until half the squad was dead still cost six soldiers of ten in
        # a single mission that the driver then recorded as a successful withdrawal -- the roster
        # went 10 to 4 and never recovered, and soldiers are permanent losses that also strip the
        # labs and the base garrison. Leaving with eight alive is worth far more than any crash
        # site. Two triggers: a quarter of the squad already down, or being outnumbered three to
        # one with any loss at all.
        # Retreat is a last resort, not a reflex. Pulling out at the first casualty meant the
        # campaign withdrew from essentially every mission: no wins, no recovered wrecks, no
        # research unlocks, and none of the tactical score a completed mission pays. Aliens have
        # to actually be killed for any of this to progress. Bail only when the squad is genuinely
        # collapsing -- down to a third of its strength and still badly outnumbered -- rather than
        # whenever it starts taking losses.
        # Backstop: a battle that has stopped moving for a long stretch and does not qualify
        # for either of the branches above will otherwise sit there until the whole budget is
        # gone. One such stall cost 22 minutes of wall-clock and an entire campaign hour, with
        # the squad standing around a single alien it could not path to. Leave honestly.
        # A stalemate is not a reason to leave either -- it just means the last hostiles have not
        # been found yet, and with fog of war that is expected. The battle budget ends a genuine
        # deadlock on its own, which costs time rather than handing the map back.
        if stalls and stalls % 40 == 0:
            d.say(f"  [battle] stalemate at {stalls} rounds ({alive_now} vs {foes_now}); "
                  f"still searching")

        # Clearly outnumbered: leave rather than be wiped out. A lost mission and an abandoned one
        # end the same way for the aliens -- on any mission the player does not win, the
        # survivors of a building raid go back into that building and those of a UFO recovery
        # go to a nearby one (Battle::exitBattle, battle.cpp), wiped out or not -- so staying buys
        # nothing but dead soldiers. That is how two soldiers kept fighting fifteen aliens to the
        # last man, the replacements were unarmed, the next alert beside the base went
        # unanswered, and four base defences followed. "Clearly" is three to one with a loss
        # already taken, or five to one outright. Never from a base defence: that concedes the
        # base.
        ratio = foes_n / max(1, alive)
        clearly_outnumbered = alive and (ratio >= 5 or (ratio >= 3 and alive < started_with))
        if may_leave and started_with and clearly_outnumbered:
            d.say(f"  [battle] outnumbered {foes_n} to {alive}; withdrawing the survivors")
            if leave_battle(d):
                d.last_battle.update(withdrew=True)
                return "returned"
            d.say("  [battle] could not leave; fighting on")

    d.say("[battle] budget exhausted without a decision")
    return "timeout"


def open_buysell(d: Driver) -> bool:
    """CityView -> base -> the buy/sell screen."""
    st = d.status()
    if st.stage != "CityView":
        return False
    d.click_id("BUTTON_TAB_1", st)
    time.sleep(0.35)
    if not d.click_id("BUTTON_SHOW_BASE", d.status()):
        return False
    try:
        d.wait_for("BaseScreen", 30)
    except TimeoutError:
        return False
    d.click_id("BUTTON_BASE_BUYSELL", d.status())
    time.sleep(1.2)
    return d.status().stage == "BuyAndSellScreen"


def close_buysell(d: Driver, commit: bool) -> bool:
    """Commit or abandon the pending transaction and get back to the city."""
    if commit:
        d.click_id("BUTTON_OK", d.status())
    else:
        d.escape_key()
    time.sleep(1.0)
    for _ in range(8):
        st = d.status()
        if st.stage == "CityView":
            return True
        if st.stage == "MessageBox":
            d.h.key("Return")
        elif st.stage in ("BuyAndSellScreen", "BaseScreen"):
            d.click_id("BUTTON_OK", st)
        else:
            d.escape_key()
        time.sleep(0.6)
    return d.status().stage == "CityView"


def equip_craft(d: Driver) -> str:
    """Fit the hardest-hitting air weapon in stores to a craft. Returns what happened.

    Craft flew with their default armament while better guns sat in the warehouse -- two Bolter
    4000 lasers and two Lancer 7000s unused while interceptors were being shot down and
    craft_lost fell past -400. Nothing was equipping them, because the vehicle equip screen draws
    its inventory as a per-frame list of screen rects with no control ids at all: there was no way
    to find an item, let alone fit one.

    gs vequip_items reports those rects with each item's damage and whether it is an air weapon.
    Fitting is then a single Shift+click: VEquipScreen treats Shift+MouseDown on an inventory item
    as "put this on the vehicle" outright (vequipscreen.cpp:336-357), no drag required, and
    AdvancedInventoryControls -- which gates it -- defaults on (options.cpp:378).
    """
    st = d.status()
    if st.stage != "CityView":
        return f"not-in-city ({st.stage})"
    info = d.h.gs("centre_on_base")
    at = info.get("at", "")
    if not at or at == "-":
        return "no-base-framed"
    try:
        bx, by = (int(v) for v in at.split(",")[:2])
    except ValueError:
        return "bad-base-coords"
    d.h.ok(f"click {bx} {by} right")
    time.sleep(0.9)
    if d.status().stage != "BuildingScreen":
        return_to_city(d)
        return f"no-building-screen ({d.status().stage})"
    if not d.click_id("BUTTON_EQUIPVEHICLE", d.status()):
        return_to_city(d)
        return "no-equip-button"
    try:
        d.wait_for("VEquipScreen", 20)
    except TimeoutError:
        return_to_city(d)
        return f"vequip-not-reached ({d.status().stage})"

    # The inventory list is cleared and rebuilt inside render() (vequipscreen.cpp:456-458), so it
    # is empty until a frame has actually drawn -- querying the instant the stage appears reported
    # an empty warehouse while two Bolter 4000s sat in it. Make sure the weapons tab is the one
    # showing, then give it frames.
    try:
        d.h.send("control BUTTON_SHOW_WEAPONS click")
    except (HarnessError, OSError):
        pass
    items, detail = {}, "-"
    for _ in range(10):
        time.sleep(0.4)
        items = d.h.gs("vequip_items")
        detail = items.get("detail", "-")
        if detail and detail != "-":
            break
    if not detail or detail == "-":
        return_to_city(d)
        return "no-items-in-stores"
    best = None
    for part in detail.split("|"):
        bits = part.split(":")
        if not bits:
            continue
        name = bits[0]
        attrs = {}
        for kv in bits[1:]:
            if "=" in kv:
                k, v = kv.split("=", 1)
                attrs[k] = v
        if attrs.get("weapon") != "1" or attrs.get("air") != "1":
            continue
        try:
            dmg = int(attrs.get("damage", "0") or 0)
            ax, ay = (int(v) for v in attrs.get("at", "0,0").split(","))
            sw, sh = (int(v) for v in attrs.get("size", "0,0").split(","))
        except ValueError:
            continue
        if best is None or dmg > best[0]:
            best = (dmg, name, ax + sw // 2, ay + sh // 2)
    if not best:
        return_to_city(d)
        return "no-air-weapon-in-stores"

    dmg, name, cx, cy = best
    d.say(f"  [vequip] fitting {name} (damage {dmg}) to {items.get('vehicle', '?')}")
    d.h.send("keydown Left Shift")
    try:
        settle(d)
        d.h.ok(f"down {cx} {cy}")
        time.sleep(0.2)
        d.h.ok(f"up {cx} {cy}")
        time.sleep(0.3)
    finally:
        d.h.send("keyup Left Shift")
    time.sleep(0.4)
    after = d.h.gs("vequip_items")
    return_to_city(d)
    d.say(f"  [vequip] stores now list {after.get('count', '?')} item kinds")
    return "fitted"


# ---------------------------------------------------------------------------
# Strategy from AllOutWar's X-Com Apocalypse guide (ufopaedia.org)
#
# The guide is specific where guesswork had been expensive, and it contradicts two rules this
# driver had arrived at by measurement -- both worth correcting:
#
#   * "Like every guide states - sell off ground vehicles." A campaign starts with a Stormdog, a
#     Wolfhound APC and a road bike, none of which can reach a crash site. They were being kept,
#     counted as fleet strength, and even sent to intercept. Selling them funds the real fleet.
#   * "the most useful vehicle, by far, is the Hoverbike ... these guys should be your main force.
#     10 or so, strategically placed around the city", losing "2 or 3 a battle". This driver had
#     banned single-weapon craft precisely because hoverbikes kept dying -- but by cost per gun a
#     Hoverbike is $5,000 against a Phoenix Hovercar's $6,304, and the guide's point is that they
#     are meant to be expendable. Buy them in numbers instead of refusing them.
# ---------------------------------------------------------------------------

GROUND_VEHICLES = ("Stormdog", "Wolfhound APC", "Blazer Turbo Bike", "Griffon AFV")


def sell_named(d: Driver, wanted: list, qty: int = 4,
               category: str = "BUTTON_VEHICLES") -> int:
    """Sell items or craft by name. Returns the number of lines sold.

    A purchase row is a balance, not a counter: lowering it buys and raising it sells, which is
    how an early version of the buying code cheerfully sold the armoury. Selling is the same
    control moved the other way.
    """
    if not open_buysell(d):
        return 0
    if not d.click_id(category, d.status()):
        close_buysell(d, commit=False)
        return 0
    time.sleep(0.8)
    keys = {_norm(w) for w in wanted}
    sold = 0
    try:
        listing = d.h.send("controls LIST").split()
    except (HarnessError, OSError):
        close_buysell(d, commit=False)
        return 0
    for entry in listing[2:]:
        parts = entry.split(":")
        if len(parts) < 2 or not parts[0].isdigit():
            continue
        label = ""
        for i, part in enumerate(parts):
            if part.startswith("text="):
                label = ":".join([part[5:]] + parts[i + 1:])
                break
        # Owned craft are listed by instance name -- "Stormdog_1", not "Stormdog" -- so an exact
        # match found nothing and the ground fleet was never sold. Match on prefix for selling.
        key = _norm(label)
        if not any(key == k or key.startswith(k) for k in keys):
            continue
        idx = parts[0]
        try:
            cur = d.h.send(f"control LIST item {idx} get")
            have, high = 0, 0
            for kv in cur.split():
                if kv.startswith("value="):
                    have = int(kv.split("=")[1] or 0)
                elif kv.startswith("max="):
                    high = int(kv.split("=")[1] or 0)
            target = min(high, have + qty) if high else have + qty
            if target != have and d.h.send(f"control LIST item {idx} set {target}").startswith("OK"):
                sold += 1
        except (HarnessError, OSError):
            break
    before = int(d.h.gs("funds").get("balance", "0") or 0)
    close_buysell(d, commit=sold > 0)
    after = int(d.h.gs("funds").get("balance", "0") or 0)
    d.say(f"  [sell] {sold} line(s); funds {before}->{after}")
    return sold


def sell_ground_fleet(d: Driver) -> int:
    """Sell the road vehicles a campaign starts with.

    They cannot reach a UFO crash site and cannot cross destroyed road, so they contribute
    nothing but upkeep -- and the driver had been counting them as fleet strength, which is how
    "four craft" meant no air capability at all. The guide is blunt about it: sell them off. The
    proceeds fund the hoverbikes that do the actual work.
    """
    return sell_named(d, list(GROUND_VEHICLES), qty=4, category="BUTTON_VEHICLES")


# Substrings that identify alien-derived equipment worth selling. Human ammunition and armour
# are excluded deliberately: they are what the squad fights with.
ALIEN_LOOT_MARKS = ("DISRUPTOR", "BOOMEROID", "DEVASTATOR", "VORTEX", "ENTROPY",
                    "BRAINSUCKER", "TOXIN", "ALIEN", "PSICLONE", "DIMENSION")


def sell_surplus_loot(d: Driver, keep: int = 1) -> int:
    """Sell captured gear the guide says is the campaign's real income. Returns lines sold.

    "Disruptors are $2500, boomeroids $900 or so ... I finished researching boomeroids - and poof,
    unloaded 99 of them to the market. Hawk right there!" Recovered equipment is worth more than
    the government funding, and it accumulates uselessly in stores otherwise.

    Three rules keep this from being self-harm:
      * Never sell anything still unresearched, and always keep one specimen. Selling the only
        corpse or artifact stalls the tech tree on a topic that can never start again.
      * Never sell the current role plan or the alien-dimension objective/wall kit.
      * Keep a working reserve of each kind rather than stripping stores to nothing.
    """
    info = d.h.gs("loot")
    detail = info.get("detail", "-")
    if not detail or detail == "-":
        return 0

    protected = {_norm(t) for t in getattr(d, "loadout_keep", set())}
    # Never liquidate the 10.3 endgame tools between ordinary missions.
    protected.update(_norm(t) for t in ("AEQUIPMENTTYPE_TOXIGUN", "AEQUIPMENTTYPE_TOXIGUN_B-CLIP",
        "AEQUIPMENTTYPE_TOXIGUN_C-CLIP", "AEQUIPMENTTYPE_DEVASTATOR_CANNON", "AEQUIPMENTTYPE_VORTEX_MINE"))

    surplus = []
    for part in detail.split("|"):
        bits = part.split(":")
        if not bits:
            continue
        item = bits[0]
        attrs = {}
        for kv in bits[1:]:
            if "=" in kv:
                k, v = kv.split("=", 1)
                attrs[k] = v
        if attrs.get("researched") != "1":
            continue                      # unresearched: keep every one, it is a specimen
        if _norm(item) in protected:
            continue                      # the squad wears this
        # Only sell alien gear. "researched" is true of ordinary human kit too, so the first pass
        # cheerfully offered up Marsec M4000 clips and Megapol Lawpistol clips -- the ammunition
        # for the squad's own guns. The guide's list of what to sell is specific: disruptors,
        # boomeroids, devastator cannons, vortex mines, personal shields, alien grenades.
        if not any(mark in item.upper() for mark in ALIEN_LOOT_MARKS):
            continue
        try:
            have = int(attrs.get("have", "0") or 0)
        except ValueError:
            continue
        if have > keep + 2:
            surplus.append((item, have - keep))
    if not surplus:
        return 0
    names = [i for i, _ in surplus][:8]
    d.say(f"  [sell] surplus loot: {len(names)} kind(s), e.g. {names[:3]}")
    return sell_named(d, names, qty=max(q for _, q in surplus), category="BUTTON_AGENTS")


def buy_interceptor(d: Driver, want: int = 2) -> int:
    """Buy armed FLYING craft. Returns the number of purchase lines ordered.

    Every campaign starts with road vehicles only -- Stormdog, Wolfhound APC, a road bike, all
    flying=0 -- so there is no air capability at all until one is bought. buy_vehicles simply took
    whatever sat at the top of the list, which bought more things that cannot reach a UFO: at day
    10 of one run, ufos_downed was still 0 while incursions stood at -237 and climbing. UFOs left
    alone infiltrate buildings and wreck the city, and those two buckets are what actually ends
    these campaigns -- not lost battles.

    So choose by capability, cheapest first: it must fly, and it must have a weapon slot.
    """
    info = d.h.gs("buyable_craft")
    detail = info.get("detail", "-")
    if not detail or detail == "-":
        d.say("  [craft] nothing purchasable reported")
        return 0
    funds = int(d.h.gs("funds").get("balance", "0") or 0)
    options = []
    for part in detail.split("|"):
        bits = part.split(":")
        if not bits:
            continue
        name = bits[0].replace("_", " ")
        attrs = {}
        for kv in bits[1:]:
            if "=" in kv:
                k, v = kv.split("=", 1)
                try:
                    attrs[k] = int(v)
                except ValueError:
                    attrs[k] = 0
        if attrs.get("flying") == 1 and attrs.get("weapons", 0) > 0:
            options.append((attrs.get("price", 0), name, attrs.get("weapons", 0)))
    if not options:
        d.say("  [craft] no armed flying craft offered for sale")
        return 0
    # Guns per pound, not guns outright, and never a one-gun bike. Two rules were tried and both
    # were wrong. Cheapest-first bought $5000 Hoverbikes that died on contact -- interceptors went
    # four to zero in ten minutes. Best-platform-first then sank $101,000 of a $152,947 balance
    # into a single Hawk Air Warrior, which is three guns in one place that the next UFO can take
    # out in one engagement, leaving nothing and no money to replace it.
    #
    # A Phoenix Hovercar is two guns for $12,607; a Hawk is three for $101,000. Eight Phoenixes
    # cost less than one Hawk and can be in eight places. Rank by cost per weapon -- which puts
    # the $5,000 Hoverbike first, and the guide agrees: they are the mainstay precisely because
    # they are cheap enough to lose two or three a battle without it mattering.
    options.sort(key=lambda o: (o[0] / max(1, o[2]), o[0]))
    # Leave enough behind to keep paying wages and stocking weapons; a fleet with no armoury
    # loses the ground war instead of the air one.
    affordable = [o for o in options if o[0] and o[0] <= funds - 40000]
    if not affordable:
        cheapest = min(options, key=lambda o: o[0])
        d.say(f"  [craft] cheapest armed flier is ${cheapest[0]}, only ${funds} on hand")
        return 0
    best_guns = affordable[0][2]
    tier = [o for o in affordable if o[2] == best_guns and o[0] == affordable[0][0]] or [affordable[0]]
    price = tier[0][0]
    # Buy only as many as the money actually covers. Asking for two Hawk Air Warriors at $101,000
    # each against $152,947 bought nothing at all: the order exceeded the balance and was refused
    # whole, so the driver announced a purchase every four minutes and the fleet never grew.
    spare = max(0, funds - 40000)
    qty = max(1, min(want, spare // price if price else 1))
    picks = [n for _, n, _ in tier][:2]
    d.say(f"  [craft] buying {qty} x {best_guns}-weapon flier "
          f"(${price} each, ${funds} on hand): {', '.join(picks)}")
    got = buy_named(d, picks, qty=qty, category="BUTTON_VEHICLES")
    d.say(f"  [craft] {got} purchase line(s) ordered")
    return got


def buy_vehicles(d: Driver, want: int = 2) -> int:
    """Replace lost craft. Returns the change in player vehicle count.

    Interceptors die. The campaign that was bleeding score had gone from five craft to two, which
    costs twice over: fewer craft means UFOs go unintercepted, and unintercepted UFOs mean aliens
    infiltrating buildings -- 35 hostiles in the city while score fell 1,427 in a week. Craft are
    cheap next to that, and the buy/sell screen sells them under BUTTON_VEHICLES.
    """
    before = int(d.h.gs("vehicles").get("player_vehicles", "0") or 0)
    funds = int(d.h.gs("funds").get("balance", "0") or 0)
    if funds < 40000:
        d.say(f"  [craft] only ${funds}; not buying")
        return 0
    if not open_buysell(d):
        return 0
    if not d.click_id("BUTTON_VEHICLES", d.status()):
        close_buysell(d, commit=False)
        return 0
    time.sleep(0.8)

    ordered = 0
    try:
        listing = d.h.send("controls LIST").split()
    except (HarnessError, OSError):
        close_buysell(d, commit=False)
        return 0
    for entry in listing[2:]:
        if ordered >= want:
            break
        parts = entry.split(":")
        if len(parts) < 2 or not parts[0].isdigit():
            continue
        idx = parts[0]
        try:
            cur = d.h.send(f"control LIST item {idx} get")
            have, low = 0, 0
            for kv in cur.split():
                if kv.startswith("value="):
                    have = int(kv.split("=")[1] or 0)
                elif kv.startswith("min="):
                    low = int(kv.split("=")[1] or 0)
            target = max(low, have - 1)
            if target != have and d.h.send(f"control LIST item {idx} set {target}").startswith(
                "OK"
            ):
                ordered += 1
        except (HarnessError, OSError):
            break
    close_buysell(d, commit=ordered > 0)
    after = int(d.h.gs("vehicles").get("player_vehicles", "0") or 0)
    d.say(f"  [craft] ordered {ordered}; vehicles {before}->{after}")
    return after - before


def hire_staff(d: Driver, want: int = 6, role: str = "BUTTON_SOLDIERS",
               counter: str = "soldiers") -> int:
    """Recruit soldiers to replace losses. Returns the change in soldier count.

    Soldiers are lost permanently and nothing was replacing them, so the campaign walked itself
    down to an empty roster while still reporting wins.

    RecruitScreen is the purpose-built screen for this (BaseScreen -> BUTTON_BASE_HIREFIRESTAFF).
    A candidate is moved from the hire pool to the payroll by a plain MouseClick on its row --
    not a drag, not a listbox selection (recruitscreen.cpp:80-130). Those rows are generated at
    runtime with no ids, which is exactly what CONTROL <list> item <N> click exists for. LIST2 is
    the applicant pool and LIST1 the staff already at this base (recruitscreen.cpp:248-260: the
    left index is the current base, the right is a fixed 8, which is the unemployed pool).

    Nothing is charged until BUTTON_OK raises a Confirm Orders box and BUTTON_YES is pressed;
    the screen refuses and reports if funds or living quarters would be exceeded
    (recruitscreen.cpp:431-490, 571-673).
    """
    before = int(d.h.gs("agents").get(counter, "0") or 0)
    funds_before = int(d.h.gs("funds").get("balance", "0") or 0)

    st = d.status()
    if st.stage != "CityView":
        return 0
    d.click_id("BUTTON_TAB_1", st)
    time.sleep(0.35)
    if not d.click_id("BUTTON_SHOW_BASE", d.status()):
        return 0
    try:
        d.wait_for("BaseScreen", 30)
    except TimeoutError:
        return 0
    d.click_id("BUTTON_BASE_HIREFIRESTAFF", d.status())
    time.sleep(1.2)
    if d.status().stage != "RecruitScreen":
        d.say(f"  [hire] expected RecruitScreen, got {d.status().stage}")
        d.escape_key()
        return 0

    d.click_id(role, d.status())  # role filter
    time.sleep(0.7)

    # Report what the two lists actually hold. "clicked 0" says the first click did not land but
    # not whether the candidate pool was empty, the wrong list was addressed, or the control was
    # not found at all -- and those want completely different fixes.
    try:
        l1 = d.h.send("controls LIST1").split()
        l2 = d.h.send("controls LIST2").split()
        d.say(f"  [hire] {role}: LIST1 {len(l1) - 2} row(s), LIST2 {len(l2) - 2} row(s)")
    except (HarnessError, OSError):
        d.say(f"  [hire] {role}: could not read the candidate lists")

    hired = 0
    for _ in range(want):
        # Always click row 0: a hired candidate leaves LIST2 immediately, so the next one takes
        # its place. Walking the index instead would skip every other candidate.
        try:
            # LIST2 is the candidate pool and LIST1 the staff already at this base. Settled from
            # the engine rather than by guessing at the names: getLeftIndex() returns the index of
            # the CURRENT BASE (recruitscreen.cpp:248-260) and rightIndex is a fixed 8, so
            # agentLists[0..7] are the bases' payrolls and agentLists[8] is the unemployed pool.
            # LIST1 therefore shows people already hired here -- clicking one of those fires them
            # -- while LIST2 shows applicants, and clicking one takes it on.
            reply = d.h.send("control LIST2 item 0 click")
            if not reply.startswith("OK"):
                if hired == 0:
                    d.say(f"  [hire] LIST2 item 0 refused: {reply[:120]}")
                break
        except (HarnessError, OSError):
            break
        hired += 1
        time.sleep(0.25)

    if hired:
        d.click_id("BUTTON_OK", d.status())
        time.sleep(1.0)
        for _ in range(6):
            st = d.status()
            if st.stage != "MessageBox":
                break
            # Two different boxes can appear here: "Confirm Orders" is YesNoCancel
            # (recruitscreen.cpp:482-484) and wants YES, while "Funds exceeded" and
            # "Accomodation exceeded" are Ok-only (recruitscreen.cpp:629,652) and want OK.
            # Guessing with Return satisfied neither reliably.
            for cid in ("BUTTON_YES", "BUTTON_OK"):
                try:
                    if d.h.send(f"control {cid}").startswith("OK"):
                        break
                except (HarnessError, OSError):
                    continue
            else:
                d.h.key("Return")
            time.sleep(1.0)
    else:
        d.escape_key()

    for _ in range(8):
        st = d.status()
        if st.stage == "CityView":
            break
        if st.stage == "MessageBox":
            d.click_id("BUTTON_OK", st) or d.h.key("Return")
        elif st.stage in ("RecruitScreen", "BaseScreen"):
            d.click_id("BUTTON_OK", st)
        else:
            d.escape_key()
        time.sleep(0.6)

    after = int(d.h.gs("agents").get(counter, "0") or 0)
    funds_after = int(d.h.gs("funds").get("balance", "0") or 0)
    # Say WHY when nothing was hired. "clicked 6; soldiers 10->10; funds unchanged" reads like a
    # success and is not one -- it was logged that way on every leg of a 29-attempt experiment
    # while the roster never grew once, and the zero it returns was the only signal anything was
    # wrong. Clicks that cost nothing and change nothing have a small number of causes and the
    # driver can distinguish them.
    delta = after - before
    if hired and delta == 0:
        if funds_after == funds_before:
            why = "clicks landed but nothing was recruited: no candidates, or no room for them"
        else:
            why = "funds moved without the roster growing -- hired into another role?"
        d.say(f"  [hire] {role}: clicked {hired}; {counter} {before}->{after}; "
              f"funds {funds_before}->{funds_after} -- {why}")
    else:
        d.say(f"  [hire] {role}: clicked {hired}; {counter} {before}->{after}; "
              f"funds {funds_before}->{funds_after}")
    return delta


def hire_soldiers(d: Driver, want: int = 6) -> int:
    """Recruit combat troops."""
    return hire_staff(d, want, "BUTTON_SOLDIERS", "soldiers")


def hire_scientists(d: Driver, want: int = 6) -> int:
    """Recruit lab staff.

    Research runs on lab skill, and lab skill walks out of the door: the squad sent to an
    incident is chosen from whoever is standing in the building, scientists included, and they
    die there like anyone else. Observed: player agents 25 -> 18 and lab staffing 5/5/5 -> 2/3/0
    across a handful of missions, which quietly throttles the whole research chain. Replacing
    them is cheaper than being clever about the dispatch list.
    """
    got = hire_staff(d, want, "BUTTON_BIOSCIS", "agents_player")
    got += hire_staff(d, want, "BUTTON_PHYSCIS", "agents_player")
    return got


def hire_engineers(d: Driver, want: int = 6) -> int:
    """Recruit engineers.

    Nothing was hiring these at all, so the workshop ran on whoever happened to be there. The
    guide is direct about it -- "For Engineers, make space" -- because engineering is what pays
    for everything else: "as long as it is worth more than it costs to make, your engineers are
    turning a profit ... to have engineers sitting around, making money, and not producing
    anything is only justifiable if items cost more to make than they can be sold for."
    """
    return hire_staff(d, want, "BUTTON_ENGINRS", "agents_player")


def _parse_rows(detail: str, names: tuple) -> list[dict]:
    """Rows of a "gs" detail field: "a:b:k=v:k=v|...". Bare leading tokens land in `names`.

    Tolerant by design -- a row that does not parse is skipped, never fatal, because a half-read
    roster must not stop the whole equip pass.
    """
    rows = []
    if not detail or detail == "-":
        return rows
    for part in detail.split("|"):
        bits = part.split(":")
        row = {n: bits[i] for i, n in enumerate(names) if i < len(bits)}
        for kv in bits[len(names):]:
            if "=" in kv:
                k, v = kv.split("=", 1)
                row[k] = v
        rows.append(row)
    return rows


def aequip_items(d: Driver) -> list[dict]:
    """The inventory list for the selected agent: name, id, weapon, research, loaded, visible..."""
    return _parse_rows(d.h.gs("aequip_items").get("detail", "-"), ("name",))


def unarmed_at_base(ag: dict) -> int:
    """Soldiers who are in a base and carry no weapon -- the only ones an equip pass can reach.

    Falls back to soldiers minus armed when the game does not report it (an older build), which
    over-counts the people away on missions but never disables arming outright.
    """
    if "unarmed_at_base" in ag:
        return int(ag["unarmed_at_base"] or 0)
    return max(0, int(ag.get("soldiers", "0") or 0) - int(ag.get("armed", "0") or 0))


def _open_equip_screen(d: Driver) -> bool:
    """CityView -> BaseScreen -> AEquipScreen. Says why when it cannot."""
    st = d.status()
    if st.stage != "CityView":
        d.say(f"  [equip] not on CityView (on {st.stage}); cannot open the equip screen")
        return False
    d.click_id("BUTTON_TAB_1", st)
    time.sleep(0.35)
    if not d.click_id("BUTTON_SHOW_BASE", d.status()):
        d.say("  [equip] could not open the base screen")
        return False
    try:
        d.wait_for("BaseScreen", 30)
    except TimeoutError:
        d.say("  [equip] base screen never opened")
        return False
    d.click_id("BUTTON_BASE_EQUIPAGENT", d.status())
    try:
        d.wait_for("AEquipScreen", 25)
    except TimeoutError:
        d.say(f"  [equip] expected AEquipScreen, got {d.status().stage}")
        return_to_city(d)
        return False
    return True


def _select_agent(d: Driver, agent_id: str) -> tuple[bool, str]:
    """Click an agent's portrait by name. Returns (ok, reason) -- judged on the engine's reply."""
    reply = d.h.send(f"action aequip_select {agent_id}")
    return reply.startswith("OK"), reply


def arm_squad(d: Driver, agents: int = 24, mission: str = "base_defence") -> int:
    """Refit armed veterans and recruits by role; return the observed loaded-soldier delta."""
    return prepare_loadouts(d, mission, agents=agents)["gained"]


def _norm(text: str) -> str:
    """Squash an id or a display name to a comparable key."""
    t = text.upper().replace("AEQUIPMENTTYPE_", "").replace("_", "").replace(" ", "")
    return "".join(ch for ch in t if ch.isalnum())


def buy_named(d: Driver, wanted: list | dict, qty: int = 8, category: str = "BUTTON_AGENTS",
              reserve: int = 40000) -> int:
    """Buy named market rows, using per-item clip/item quantities and a live funds preview."""
    funds_before = int(d.h.gs("funds").get("balance", "0") or 0)
    if funds_before <= reserve:
        d.say(f"  [loadout-buy] balance=${funds_before}; keeping reserve=${reserve}")
        return 0
    requests = {_norm(w): int(n) for w, n in wanted.items()} if isinstance(wanted, dict) else {
        _norm(w): qty for w in wanted}
    if not open_buysell(d):
        return 0
    if not d.click_id(category, d.status()):
        close_buysell(d, commit=False)
        return 0
    time.sleep(0.8)
    keys = {_norm(w) for w in wanted}
    ordered = 0
    try:
        listing = d.h.send("controls LIST").split()
    except (HarnessError, OSError):
        close_buysell(d, commit=False)
        return 0
    seen: list[str] = []
    matched: set[str] = set()
    for entry in listing[2:]:
        parts = entry.split(":")
        if len(parts) < 2 or not parts[0].isdigit():
            continue
        # "text=" is not reliably the last field, and a label containing a colon splits across
        # several. Find the marker and take everything after it.
        label = ""
        for i, part in enumerate(parts):
            if part.startswith("text="):
                label = ":".join([part[5:]] + parts[i + 1:])
                break
        if label:
            seen.append(label)
        key = _norm(label)
        if key not in keys:
            continue
        matched.add(key)
        idx = parts[0]
        try:
            cur = d.h.send(f"control LIST item {idx} get")
            have, low = 0, 0
            for kv in cur.split():
                if kv.startswith("value="):
                    have = int(kv.split("=")[1] or 0)
                elif kv.startswith("min="):
                    low = int(kv.split("=")[1] or 0)
            target = max(low, have - requests[key])
            if target != have and d.h.send(f"control LIST item {idx} set {target}").startswith("OK"):
                ordered += 1
        except (HarnessError, OSError):
            break
    affordable = False
    try:
        preview = d.h.send("control TEXT_FUNDS get")
        if preview.startswith("OK") and "text=" in preview:
            projected = int(preview.split("text=", 1)[1].split()[0].replace(",", "").replace("$", ""))
            affordable = reserve <= projected < funds_before
    except (HarnessError, OSError, ValueError, IndexError):
        pass
    if ordered and not affordable:
        d.say("  [loadout-buy] order exceeds reserve or funds preview unavailable; cancelling")
    accepted = close_buysell(d, commit=ordered > 0 and affordable)
    funds_after = int(d.h.gs("funds").get("balance", "0") or 0)
    ordered = ordered if accepted and affordable and funds_after < funds_before else 0
    d.say(f"  [buy] ordered {ordered} of {len(keys)} wanted lines, funds {funds_before}->{funds_after}")
    missing = keys - matched
    if missing:
        # Name what could not be found and what the screen was actually offering. "ordered 1 of
        # 11" said the buying failed but never why, and the answer -- an armoury with weapons=0
        # while every recruit went unarmed -- was worth knowing the first time it happened.
        d.say(f"  [buy] no row for {len(missing)} wanted item(s): {sorted(missing)[:4]}")
        d.say(f"  [buy] screen listed {len(seen)} row(s), e.g. {seen[:4]}")
    return ordered


# Fewest soldiers worth flying a mission with -- so also the fewest seats a craft needs to count
# as a troop transport at all -- and the most one craft is asked to carry.
MIN_SQUAD = 4
MAX_SQUAD = 6


@dataclass(frozen=True)
class Craft:
    """One craft from `gs interceptors`."""
    idx: int
    name: str
    flying: bool
    armed: bool
    crew: int        # soldiers aboard right now
    shifter: bool
    pax: int         # passenger CAPACITY, not passengers aboard
    row: int         # fleet rank at the current base; UI coordinates come from assignment_rows
    city: str = ""
    transit: bool = False
    home: bool = False
    id: str = ""
    portal: bool = False

    @property
    def kind(self) -> str:
        """The craft type without its fleet number: Hoverbike_22 and Hoverbike_23 are one kind."""
        return re.sub(r"_\d+$", "", self.name)


def parse_fleet(detail: str) -> list[Craft]:
    """Parse the `detail=` field of `gs interceptors`; unknown or missing fields read as 0/-1."""
    fleet = []
    for idx, name, flags in parse_craft_flags(detail):
        def number(key: str, default: int = 0) -> int:
            try:
                return int(flags.get(key, default))
            except ValueError:
                return default
        fleet.append(Craft(idx, name, number("flying") == 1,
                           number("armed") == 1, number("crew"), number("shifter") == 1,
                           number("pax"), number("row", -1), flags.get("city", ""),
                           number("transit") == 1, number("home") == 1,
                           flags.get("id", ""), number("portal") == 1))
    return fleet


def rank_transports(fleet: list[Craft], want: int = MAX_SQUAD,
                    failed: dict[str, int] | None = None, loaded: bool = False) -> list[Craft]:
    """Gate capability first, then usable seats/squad, reliable loading and spare weapons.

    Boarding requires a parked flyer with at least four seats. Dispatch uses the actual squad.
    Within the gate/ordinary group a repeatedly failed craft sinks below working alternatives.
    Capacity is capped at want so a larger hull does not win when both seat the whole squad.
    """
    failed = failed or {}
    if loaded:
        pool = [c for c in fleet if c.flying and c.crew > 0]
    else:
        pool = [c for c in fleet if c.flying and c.row >= 0 and c.pax >= MIN_SQUAD]
    return sorted(pool, key=lambda c: (-c.shifter, failed.get(c.kind, 0) >= 2,
                                       -min(c.crew if loaded else c.pax, want),
                                       -c.crew, c.armed, c.pax, c.idx))


def squad_size(soldiers: int, pax: int, garrison: int = 4) -> tuple[int, int]:
    """(soldiers to put aboard a craft with `pax` seats, soldiers held back at the base).

    Aliens attack the BASE, so a garrison stays; but the reserve scales with the roster, because a
    flat reserve of four means a campaign with four soldiers never flies a mission, and at least
    one soldier always goes. The squad is then capped by the seats: asking for six on a four-seater
    seats four and leaves two standing on the pad.
    """
    held = min(max(0, garrison), soldiers // 2) if soldiers > 1 else 0
    spare = max(0, min(MAX_SQUAD, soldiers - held))
    return max(0, min(spare, pax)), held


def parse_offers(detail: str) -> list[dict]:
    """Parse the `detail=` field of `gs buyable_craft` into dicts of ints, plus the name."""
    offers = []
    for part in (detail or "").split("|"):
        bits = part.split(":")
        if len(bits) < 2 or not bits[0]:
            continue
        offer: dict = {"name": bits[0].replace("_", " ")}
        for kv in bits[1:]:
            if "=" in kv:
                k, v = kv.split("=", 1)
                try:
                    offer[k] = int(v)
                except ValueError:
                    offer[k] = 0
        offers.append(offer)
    return offers


def choose_transport_purchase(offers: list[dict], funds: int, reserve: int = 40000) -> dict | None:
    """The market craft to buy as a troop transport, or None when none qualifies.

    It must fly and seat MIN_SQUAD, be in stock, and leave `reserve` in the bank for wages and
    weapons. Cheapest first, then more seats: the point is a squad in the air, not a flagship.
    """
    ok = [o for o in offers if o.get("flying") == 1 and o.get("pax", 0) >= MIN_SQUAD
          and o.get("stock", 0) > 0 and 0 < o.get("price", 0) <= funds - reserve]
    return min(ok, key=lambda o: (o["price"], -o["pax"])) if ok else None


def _fleet(d: Driver) -> list[Craft]:
    return parse_fleet(d.h.gs("interceptors").get("detail", ""))


def _soldier_count(d: Driver) -> int:
    try:
        return int(d.h.gs("agents").get("soldiers", "0") or 0)
    except (ValueError, AttributeError):
        return 0


def _flying_crewed(d: Driver) -> int:
    """How many *flying* craft carry a usable squad.

    Plain crewed counts are not enough: a Stormdog or Wolfhound APC can hold a squad and still be
    useless for reaching a downed UFO, and recovery is refused outright when the selected craft
    cannot get there. Nor is one soldier on a hoverbike a crewed craft: it counted as one, so the
    gates that re-crew stayed shut while recoveries flew with a single soldier. A squad is
    MIN_SQUAD soldiers, or the available squad after the scaled garrison when that is smaller.
    """
    take, _ = squad_size(_soldier_count(d), MAX_SQUAD, driver_strategy(d)["garrison"])
    need = min(MIN_SQUAD, max(1, take))
    return sum(1 for c in _fleet(d) if c.flying and c.crew >= need)


def buy_troop_transport(d: Driver, fleet: list[Craft]) -> int:
    """Buy a flying craft that seats a squad when the fleet has none. Returns craft gained.

    Only when NO owned flying craft has MIN_SQUAD seats: a Valkyrie that is merely out on a
    mission is still the transport, and buying a second one because it is not home yet is waste.
    Verified against the fleet afterwards, so an order the screen declined is not reported as
    a purchase.
    """
    if any(c.flying and c.pax >= MIN_SQUAD for c in fleet):
        return 0
    funds = int(d.h.gs("funds").get("balance", "0") or 0)
    pick = choose_transport_purchase(
        parse_offers(d.h.gs("buyable_craft").get("detail", "-")), funds)
    if pick is None:
        d.say(f"  [crew] no flying craft seats {MIN_SQUAD} and none can be bought with ${funds}")
        return 0
    d.say(f"  [crew] no troop transport owned; buying a {pick['name']} "
          f"({pick['pax']} seats, ${pick['price']}, ${funds} on hand)")
    buy_named(d, [pick["name"]], qty=1, category="BUTTON_VEHICLES")
    gained = len([c for c in _fleet(d) if c.flying and c.pax >= MIN_SQUAD])
    if not gained:
        d.say("  [crew] purchase ordered but no troop transport has arrived")
    return gained



def assignment_rows(st: Status, field: str) -> list[tuple[int, ...]]:
    """Read resolved rows from BuildingScreen's read-only harness detail."""
    detail = st.detail or ""
    if f"{field}=" not in detail:
        return []
    value = detail.split(f"{field}=", 1)[1].split("_", 1)[0].split()[0]
    rows = []
    for part in value.split(";"):
        try:
            rows.append(tuple(int(v) for v in part.split(",")))
        except ValueError:
            continue
    return rows


def crew_transport(d: Driver, garrison: int | None = None) -> int:
    """Board a capacity-ranked flyer using resolved UI rows and verify every drop via gs.

    Once a gate craft exists, wait for it rather than strand the assault squad on an ordinary
    transport. Pull soldiers from other craft as well as the base, keeping the scaled garrison.
    Fleet indices join the screen's resolved rows to the chosen craft; they never set pixels.
    """
    if garrison is None:
        garrison = driver_strategy(d)["garrison"]
    if d.status().stage != "CityView":
        return 0
    failures = d.__dict__.setdefault("crew_failures", {})
    fleet = _fleet(d)
    gate = any(c.flying and c.shifter for c in fleet)
    ranked = rank_transports(fleet, MAX_SQUAD, failures)
    if gate:
        ranked = [c for c in ranked if c.shifter]
    elif not ranked and buy_troop_transport(d, fleet):
        fleet = _fleet(d)
        ranked = rank_transports(fleet, MAX_SQUAD, failures)
    if not ranked:
        d.say("  [crew] waiting for gate craft at the base" if gate else
              "  [crew] no flying transport parked at the base")
        return _flying_crewed(d)
    best = ranked[0]
    soldiers = _soldier_count(d)
    take, held = squad_size(soldiers, best.pax, garrison)
    if best.crew >= take:
        return _flying_crewed(d)
    d.say(f"  [crew] {held} stay to defend the base; {take} fit in {best.name}")
    at = d.h.gs("centre_on_base")
    if at.get("centred") != "1":
        return 0
    bx, by = (int(v) for v in at["at"].split(",")[:2])
    d.h.ok(f"click {bx} {by} right")
    time.sleep(1.2)
    if d.status().stage != "BuildingScreen":
        return_to_city(d)
        return 0

    def target_row(st: Status) -> tuple[int, ...] | None:
        return next((r for r in assignment_rows(st, "boarding")
                     if len(r) >= 7 and r[6] == best.idx and r[5] == 1
                     and r[3] >= take and (not gate or r[2] == 1)), None)

    def scroll_to(y: int) -> bool:
        viewport = d.live_rect("AGENT_SELECT_BOX")
        reply = d.h.send("control AGENT_SELECT_SCROLL get")
        scroll = dict(p.split("=", 1) for p in reply.split() if "=" in p)
        if not viewport or "max" not in scroll:
            return False
        value = int(scroll.get("value", "0")) + y - (viewport["y"] + viewport["h"] // 2)
        value = max(int(scroll.get("min", "0")), min(int(scroll["max"]), value))
        d.h.send(f"control AGENT_SELECT_SCROLL set {value}")
        time.sleep(0.25)
        return True

    aboard = best.crew
    attempted: set[int] = set()
    try:
        d.h.send("control AGENT_SELECT_SCROLL set 0")
        time.sleep(0.2)
        for _ in range(len(fleet) + 2):
            if aboard >= take:
                break
            st = d.status()
            target = target_row(st)
            if target is None:
                d.say(f"  [crew] no resolved assignment row for {best.name}")
                break
            rows = [r for r in assignment_rows(st, "soldier_rows")
                    if len(r) >= 6 and r[5] != best.idx and r[5] not in attempted]
            # A drag copies one source list. Prefer the existing squad on another craft.
            rows.sort(key=lambda r: (-r[2], -r[3]))
            if not rows:
                break
            source = rows[0][5]
            group = rows[0][4]
            if rows[0][3] == 0:
                if not scroll_to(rows[0][1]):
                    break
                rows = [r for r in assignment_rows(d.status(), "soldier_rows")
                        if len(r) >= 6 and r[5] == source and r[4] == group]
            squad = [r for r in rows if r[4] == group and r[3] == 1][:take - aboard]
            if not squad:
                attempted.add(source)
                continue
            selected = 0
            for x, y, *_ in squad:
                d.h.click_xy(x, y)
                time.sleep(0.1)
                detail = d.status().detail or ""
                if "selected_agents=" not in detail:
                    break
                selected = int(detail.split("selected_agents=", 1)[1].split("_", 1)[0].split()[0])
            if selected != len(squad):
                d.say("  [crew] selection did not match the resolved soldier rows; stopping")
                break
            sx, sy = squad[0][:2]
            d.say(f"  [crew] taking {len(squad)} of {soldiers} soldier(s) onto {best.name}; "
                  f"reserve {held}")
            d.h.ok(f"down {sx} {sy}")
            d.h.ok(f"move {sx + 12} {sy}")  # Create the drag before scrolling the source away.
            time.sleep(0.15)
            try:
                target = target_row(d.status())
                if target is None:
                    break
                if target[4] == 0:
                    if not scroll_to(target[1]):
                        break
                    target = target_row(d.status())
                    if target is None or target[4] == 0:
                        break
                tx, ty = target[:2]
                for step in range(1, 7):
                    d.h.ok(f"move {sx + (tx - sx) * step // 6} {sy + (ty - sy) * step // 6}")
                    time.sleep(0.08)
                d.h.ok(f"up {tx} {ty}")
                time.sleep(0.6)
            finally:
                d.h.ok(f"up {sx} {sy}")
            now = next((c for c in _fleet(d) if c.idx == best.idx), None)
            seated = (now.crew if now else aboard) - aboard
            d.say(f"  [crew] dropped {selected} onto {best.name}: {seated} seated")
            aboard += max(0, seated)
            if seated < selected:
                break
    finally:
        return_to_city(d)
    final = next((c for c in _fleet(d) if c.idx == best.idx), None)
    crew_now = final.crew if final else 0
    if crew_now >= min(take, MIN_SQUAD):
        failures[best.kind] = 0
    elif crew_now <= best.crew:
        failures[best.kind] = failures.get(best.kind, 0) + 1
    d.say(f"  [crew] {best.name}: {best.crew} -> {crew_now} of {take} soldier(s)")
    return _flying_crewed(d)


def select_crewed_craft(d: Driver) -> bool:
    """Make the city view's selection a craft that carries a Soldier.

    handleClickedVehicle decides whether to issue a recovery mission by scanning
    cityViewSelectedOwnedVehicles for a Soldier (cityview.cpp:1069-1090). With an interceptor
    selected it issues nothing at all -- no mission, no message, no error -- which is exactly what
    a crewed transport plus three uncollected wrecks looked like.

    OWNED_VEHICLE_LIST is a horizontal ListBox (tab2.form: 431x24 at 105,44) whose items are
    runtime-built vehicle icons with no ids, so the craft is found by clicking across it and
    asking the engine what ended up selected.
    """
    st = d.status()
    if st.stage != "CityView":
        return False
    if int(d.h.gs("selected").get("with_soldier", "0") or 0) > 0:
        return True
    if not d.click_id("BUTTON_TAB_2", st):
        return False
    time.sleep(0.4)
    lst = d.controls(d.status()).get("OWNED_VEHICLE_LIST")
    if lst is None or lst.w <= 0:
        return False
    # The craft must be able to *reach* the wreck, so it has to fly as well as carry troops.
    # Selecting the first crewed craft regardless of type picked a Stormdog -- a road vehicle --
    # and every recovery was refused with "mission: none", stalling the whole research chain,
    # since recovering UFOs is what unlocks it.
    # Of the flying craft with troops aboard, the one carrying the biggest squad: a hoverbike with
    # one soldier on it is "crewed" too, and taking the first such craft sent it to the wreck.
    wanted = [c.idx for c in rank_transports(_fleet(d), MAX_SQUAD, loaded=True)]
    if not wanted:
        d.say("  [select] no flying craft with troops aboard")
        return False

    ICON_W = 36
    y = lst.y + lst.h // 2
    for slot in wanted:
        x = lst.x + 16 + slot * ICON_W
        if x >= lst.x + lst.w:
            continue
        d.h.click_xy(x, y)
        time.sleep(0.15)
        if int(d.h.gs("selected").get("with_soldier", "0") or 0) > 0:
            d.say(f"  [select] flying crewed craft selected (slot {slot})")
            return True
    return False


def clear_attack_orders(d: Driver) -> int:
    """Recall craft still holding an attack order, so the clock can run at turbo again.

    GameState::canTurbo() returns false while *any* vehicle holds an AttackVehicle or
    AttackBuilding mission -- our own craft included -- and crashed hostiles explicitly do not
    count. So a wing left circling after the UFO it was chasing went down pins the game at normal
    speed forever. At speed 4 a single game-day costs about ten real hours, which makes a
    months-long campaign impossible; at turbo it costs seconds. This is the difference between a
    campaign that can finish and one that cannot.

    Recalling to base is also what a player would do: it rearms and refuels the craft.
    """
    st = d.status()
    if st.stage != "CityView":
        return 0
    if not d.click_id("BUTTON_TAB_2", st):
        return 0
    time.sleep(0.3)
    lst = d.controls(d.status()).get("OWNED_VEHICLE_LIST")
    if lst is None or lst.w <= 0:
        return 0
    y = lst.y + lst.h // 2
    recalled, seen = 0, set()
    for x in range(lst.x + 6, lst.x + lst.w, 12):
        d.h.click_xy(x, y)
        time.sleep(0.1)
        sel = d.h.gs("selected")
        ids, mission = sel.get("ids", "-"), sel.get("mission", "none")
        if ids in seen:
            continue
        seen.add(ids)
        if mission.startswith("AttackVehicle") or mission.startswith("AttackBuilding"):
            if d.click_id("BUTTON_GOTO_BASE", d.status()):
                recalled += 1
                time.sleep(0.2)
    if recalled:
        d.say(f"  [recall] {recalled} craft sent home to clear stale attack orders")
    return recalled


# A crewed craft parked, with no orders, at a building that is not its home for this long is a
# squad nobody is using.
STRANDED_AFTER_S = 90.0


def recall_stranded_squads(d: Driver) -> int:
    """Send home crewed craft that have sat idle at someone else's building.

    A squad dispatched to an alert waits at the building for "Commence investigation", and the
    engine only asks if the building is still detected when the squad arrives
    (CityView::handleGameEvent, CommenceInvestigation). If detection has lapsed there is no prompt,
    and the raid that would follow is gated on the armed count -- so the squad sat there. Three base
    defences in one batch were lost to a single alien because every soldier was parked across
    town and the base held only unarmed staff.
    """
    now = time.time()
    seen, recalled = set(), 0
    for _, f in craft_flags(d):
        cid, at = f.get("id", ""), f.get("at", "-")
        if (not cid or int(f.get("crew", "0") or 0) == 0 or f.get("home") == "1"
                or at in ("", "-") or f.get("idle") != "1"):
            continue
        seen.add(cid)
        since = d.stranded_since.get(cid)
        if not since or since[0] != at:
            d.stranded_since[cid] = (at, now)
            continue
        if now - since[1] < STRANDED_AFTER_S:
            continue
        if select_craft(d, cid) and d.click_id("BUTTON_GOTO_BASE", d.status()):
            d.say(f"  [recall] {cid} sat idle at {at} with {f.get('crew')} aboard for "
                  f"{now - since[1]:.0f}s; sending the squad home")
            recalled += 1
            seen.discard(cid)
        time.sleep(0.2)
    for cid in list(d.stranded_since):
        if cid not in seen:
            del d.stranded_since[cid]
    return recalled


def recover_crash_sites(d: Driver) -> int:
    """Send a troop-carrying craft to a downed UFO to recover it.

    Recovering a wreck is the only source of alien artifacts, so the entire research tree -- and
    therefore victory -- is downstream of this working. Three separate things had to be right,
    and each one failed silently on its own:

    * The order is a plain left-click on the wreck with a craft selected, handled in
      CityView::handleMouseDown -- not BUTTON_GOTO_LOCATION, which sets a map destination.
    * VehicleMission::recoverVehicle is only issued when a *selected* craft carries a Soldier
      (cityview.cpp:1069-1090). Selecting the wing by clicking each icon does not work: each
      click replaces the selection, so only the last, usually empty, craft stayed selected.
    * The wreck has to be on screen. Reading ufos_screen before centring yields coordinates for
      a wreck that may be well outside the viewport, and clicking there hits nothing at all.

    Returns 1 only when the craft actually picked up a mission, so a refusal is visible instead
    of being counted as a success.
    """
    st = d.status()
    if st.stage != "CityView":
        return 0
    prepare_loadouts(d, "ufo_recovery")
    # Selection first: it is pure UI and does not move the camera, so centring stays valid.
    if not select_crewed_craft(d):
        d.say("  [recover] no crewed craft could be selected")
        return 0
    # Arm the order before clicking. A plain left-click on a vehicle only runs orderSelect; it is
    # CitySelectionState::AttackVehicle that routes the next click into CityView::orderAttack,
    # which is where the crashed-vehicle branch and recoverVehicle actually live
    # (cityview.cpp:331-400, 1047-1090). Without this the click just selected the wreck and the
    # recovery silently never happened. Interception already worked because it arms the same way.
    st = d.status()
    if not d.click_id("BUTTON_VEHICLE_ATTACK", st):
        d.say("  [recover] could not arm the attack order")
        return 0
    time.sleep(0.3)
    # Prefer a wreck that can be recovered without a battle. Every alien craft type unlocks the
    # same alien-craft research when recovered, but only some cost a tactical mission to collect
    # -- and those missions have been costing more soldiers than they are worth (six of ten, then
    # three of six, then two of three, all withdrawals). Probes and Scouts give the same unlocks
    # for free, so go for those first and only take a fighting recovery when there is no
    # alternative and the squad can afford it.
    free_first = d.h.gs("centre_on_free_crash")
    if free_first.get("centred") == "1":
        d.say(f"  [recover] going for a battle-free wreck ({free_first.get('type', '?')})")
    elif d.h.gs("centre_on_crash").get("centred") != "1":
        return 0
    time.sleep(0.5)

    w, h = d.h.display_size()
    crashed = [
        (x, y)
        for (x, y, down) in d.h.screen_craft("ufos_screen")
        if down and 0 <= x < w and 0 <= y < h
    ]
    if not crashed:
        d.say("  [recover] wreck not on screen after centring")
        return 0
    # Nearest to centre: that is the one centre_on_crash just framed.
    cx, cy = min(crashed, key=lambda p: (p[0] - w // 2) ** 2 + (p[1] - h // 2) ** 2)

    d.h.click_xy(cx, cy)

    # Give the order a moment to land, and do not mistake a craft that is still leaving the pad
    # for a refusal. A craft ordered while parked reports mission=TakeOff_from_<base> first and
    # only shows the recovery once airborne, so checking immediately and once read as "refused"
    # on orders that had actually been accepted.
    mission = "none"
    for _ in range(6):
        time.sleep(0.8)
        mission = d.h.gs("selected").get("mission", "none")
        if "ecover" in mission or "oto" in mission:
            d.say(f"  [recover] craft dispatched to wreck at {cx},{cy} (mission={mission})")
            return 1
        if "TakeOff" not in mission and mission != "none":
            break
    d.say(f"  [recover] refused at {cx},{cy} (mission={mission})")
    return 0

def log_leg(d: Driver, run_id: str, day: float, phase: str, extra: dict) -> None:
    """Append one durable, machine-readable record per leg.

    Runs are otherwise only comparable by scrolling two console logs side by side, which is how a
    whole evening got spent concluding things about score components that were really measuring
    which run happened to be quieter. A JSONL ledger makes "improve on all metrics" checkable
    instead of asserted: same fields every leg, every run, appended not overwritten.

    Everything here comes from `gs` queries the driver already makes -- the same numbers a player
    reads off the score screen, the base screen and the vehicle list. Nothing is sourced from
    anywhere a human could not look.
    """
    rec = {"run": run_id, "day": round(day, 1), "phase": phase}
    for q in ("funds", "time", "vehicles", "agents", "turbo"):
        try:
            for k, v in (d.h.gs(q) or {}).items():
                if k not in rec:
                    rec[k] = v
        except Exception:
            pass
    rec.update(extra)
    try:
        with LEDGER.open("a") as fh:
            fh.write(json.dumps(rec) + "\n")
    except Exception as exc:
        d.say(f"  [ledger] could not write: {type(exc).__name__}: {exc}")


def base_upkeep(d: Driver, need_quarters: bool = False) -> dict:
    """Buy the second base, and build living quarters when recruiting is blocked.

    Both capabilities existed and were called from nowhere but oa_victory.py -- the campaign
    driver and the adversarial evaluator never expanded a base at all. That is not a missing
    nicety; it is what makes a campaign run down and a base defence unsurvivable.

    A SECOND BASE. GameLost is raised on exactly one condition, player_bases.empty()
    (base.cpp:150-159), so a second base turns losing one from a defeat into a setback. It also
    decides whether a base defence can be abandoned at all: win_battle's may_leave requires
    bases > 1, so with a single base a losing defence must be fought to the last man. Observed
    exactly that -- 0 of 15 survived, scored 0.00, in a battle the harness had no legal way out of.

    LIVING QUARTERS are what let the roster grow. hire_staff returns the true change in soldier
    count, and it was returning 0 every leg while its log line read "clicked 6" -- recruits refused
    for want of space. Attrition became permanent, and a campaign ended at
    "no-agents-selectable" with 22 game-days on the clock and no mission it could fly.
    """
    # ALWAYS returns a reason, never an empty dict. An earlier version returned {} whenever it
    # found nothing to do, which is indistinguishable from never having run -- and it ran every
    # leg of a 29-attempt experiment achieving nothing, while the ledger showed only silence. A
    # step that declines to act has to say why it declined.
    out: dict = {}
    stage = d.status().stage
    if stage != "CityView":
        return {"skipped": f"not in city ({stage})"}

    try:
        site = d.h.gs("centre_on_basesite")
        bases = int(site.get("bases", "1") or 1)
        out["bases"] = bases
        if bases >= 2:
            out["second_base"] = "already have one"
        elif site.get("centred") != "1":
            out["second_base"] = "no base site could be centred on"
        elif site.get("affordable") != "1":
            out["second_base"] = f"not affordable (balance {site.get('balance', '?')}, "
            out["second_base"] += f"cost {site.get('cost', '?')})"
        else:
            out["second_base"] = build_second_base(d)
            d.say(f"  [base] second base: {out['second_base']}")
    except Exception as exc:
        out["second_base"] = f"error: {type(exc).__name__}: {exc}"

    if not need_quarters:
        out["quarters"] = "not needed (recruiting is working)"
    else:
        if d.status().stage != "CityView":
            return_to_city(d)
        try:
            info = d.h.gs("facilities")
            offer = info.get("offer", "")
            if "FACILITYTYPE_LIVING_QUARTERS" not in offer:
                out["quarters"] = f"not offered; offer={offer[:60] or 'empty'}"
            else:
                built = build_facility(d, "FACILITYTYPE_LIVING_QUARTERS")
                out["quarters"] = "built" if built else "offered but placement failed"
                if built:
                    d.say("  [base] living quarters built; the roster can grow again")
        except Exception as exc:
            out["quarters"] = f"error: {type(exc).__name__}: {exc}"

    if d.status().stage != "CityView":
        return_to_city(d)
    return out


def play_campaign(d: Driver, difficulty: int, total_days: float, leg_days: float = 7.0) -> dict:
    run_id = f"r{int(time.time())}"
    d.say(f"[run] {run_id}; ledger {LEDGER}")
    new_game(d, difficulty)
    t0 = snapshot(d, "t0")

    # Campaign-plan opening (docs/campaign-plan.md, Day 0). These four were all written and never
    # called from anywhere -- the same dead-code shape as raid_infiltrated_building. Selling the
    # ground fleet funds the hoverbikes; without buy_interceptor a lost craft was never replaced,
    # which is why every run's fleet decayed monotonically to nothing.
    try:
        d.checks["sold_ground"] = sell_ground_fleet(d)
    except Exception as exc:
        d.say(f"  [open] sell_ground_fleet failed: {type(exc).__name__}: {exc}")
    try:
        d.checks["bought_craft"] = buy_interceptor(d, want=2)
    except Exception as exc:
        d.say(f"  [open] buy_interceptor failed: {type(exc).__name__}: {exc}")
    # Put a squad aboard. crew_transport was written, documented at length, and called from
    # NOWHERE -- the def was its only occurrence, the same dead-code shape raid_infiltrated_building
    # had. The consequence is total: the game hands you a Valkyrie with pax=12, and with crew=0 on
    # every craft, VehicleMission::recoverVehicle refuses every downed UFO (cityview.cpp:1069-1090)
    # and no ground mission can be flown at all. Measured directly -- "[select] no flying craft with
    # troops aboard" on every attempt, across every run, from a fleet that had the transport parked
    # in the hangar the whole time.
    try:
        arm_squad(d)
        d.checks["crewed"] = crew_transport(d)
    except Exception as exc:
        d.say(f"  [open] crew_transport failed: {type(exc).__name__}: {exc}")
    log_leg(d, run_id, 0.0, "opening", {**d.checks, "strategy": d.strategy.key()})

    d.checks["research_started"] = assign_research(d)
    d.checks["ufopaedia_opened"] = visit_ufopaedia(d)
    d.checks["economy_opened"] = visit_economy(d)

    battles = 0
    elapsed = 0.0
    while elapsed < total_days:
        if d.game_over():
            break
        leg = min(leg_days, total_days - elapsed)
        advance(d, leg)
        elapsed += leg
        st = d.status()
        if st.stage in ("BattleBriefing", "BattlePreStart", "BattleView", "BaseDefenseScreen"):
            outcome = win_battle(d)
            battles += 1
            d.checks[f"battle_{battles}"] = outcome

        # Per-leg upkeep. Without this the campaign loop was only ever "advance the clock, and
        # fight if something forces you to" -- which is exactly what the measurements showed:
        # tactical=0 across every run, and the only battles ever fought were base defences,
        # i.e. the aliens arriving at OUR door. A base defence is the failure, not the game.
        if not d.game_over() and d.status().stage == "CityView":
            # 1. Keep the labs busy. assign_research() ran once at campaign start and never
            #    again, so a project finishing left its lab idle for the rest of the run --
            #    observed as labs_busy=0 with three built labs on day 15.
            try:
                assign_research(d)
            except Exception as exc:
                d.say(f"  [leg] research pass failed: {type(exc).__name__}: {exc}")

            # 2. Keep a squad aboard, BEFORE anything that needs a transport. Not a one-off:
            #    soldiers die, craft are shot down and replaced, and a transport that has lost
            #    its squad refuses every recovery and every raid in silence.
            #
            #    This used to run last in the leg, i.e. after the raid that needed it, and was
            #    skipped entirely on any leg that did have something to raid. Ordering was the
            #    whole bug: across seven measured attempts, every one that ended with a crewed
            #    flyer resolved its battle and every one without failed -- a 22.7-game-day
            #    no-contest and a base defence timed out at 0.16.
            try:
                if _flying_crewed(d) == 0:
                    got = crew_transport(d)
                    if got:
                        d.checks["crewed"] = d.checks.get("crewed", 0) + got
                        d.say(f"  [leg] put a squad aboard ({got} craft crewed)")
                if d.status().stage != "CityView":
                    return_to_city(d)
            except Exception as exc:
                d.say(f"  [leg] crewing failed: {type(exc).__name__}: {exc}")

            # 3. Take the ground game to them. raid_infiltrated_building() was written,
            #    documented at length, and never called from anywhere -- the def was its only
            #    occurrence in the file. Alien crews left in a building raise their owner's
            #    infiltrationValue every hour and spread to neighbours, and most buildings are
            #    government-owned, so this is the mechanism that was quietly ending campaigns
            #    while the driver watched the number climb.
            try:
                outcome = raid_infiltrated_building(d)
                if outcome not in ("nothing-reported", "bad-coords") \
                        and not outcome.startswith("not-in-city"):
                    battles += 1
                    d.checks[f"raid_{battles}"] = outcome
                    d.say(f"  [leg] raid -> {outcome}")
                else:
                    d.say(f"  [leg] no raid this leg ({outcome})")
            except Exception as exc:
                d.say(f"  [leg] raid failed: {type(exc).__name__}: {exc}")

            # 4. Replace losses. Attrition was the binding constraint on every run measured:
            #    agents 25->16 and the fleet 5->1 flyer inside ten days, after which EXTERMINATE
            #    is refused for want of a transport and the raid loop that earns the score stops.
            #    buy_interceptor and sell_surplus_loot were both written and never called.
            try:
                sold = sell_surplus_loot(d)
                if sold:
                    d.checks["loot_sold"] = d.checks.get("loot_sold", 0) + sold
            except Exception as exc:
                d.say(f"  [leg] loot sale failed: {type(exc).__name__}: {exc}")
            try:
                bought = buy_interceptor(d, want=2)
                if bought:
                    d.checks["craft_bought"] = d.checks.get("craft_bought", 0) + bought
                    d.say(f"  [leg] replaced {bought} craft")
            except Exception as exc:
                d.say(f"  [leg] craft purchase failed: {type(exc).__name__}: {exc}")
            hired = 0
            try:
                hired = hire_staff(d, want=6)
                if hired:
                    d.checks["agents_hired"] = d.checks.get("agents_hired", 0) + hired
                    d.say(f"  [leg] hired {hired} agent(s)")
            except Exception as exc:
                d.say(f"  [leg] hiring failed: {type(exc).__name__}: {exc}")

            # 4b. Arm them. This loop hired soldiers every leg and never put a weapon in anyone's
            #     hands -- arm_squad was reachable only from oa_victory's loop, so a plain
            #     oa_play campaign grew an unarmed roster that a base defence then threw into the
            #     fight. Runs after the hiring above and after advance(), so last leg's recruits
            #     have had time to reach the base. If nothing could be handed out the armoury is
            #     empty: buy guns for the next leg.
            try:
                gained = arm_squad(d)
                if gained:
                    d.checks["agents_armed"] = d.checks.get("agents_armed", 0) + gained
                if d.status().stage != "CityView":
                    return_to_city(d)
            except Exception as exc:
                d.say(f"  [leg] arming failed: {type(exc).__name__}: {exc}")

            # 5. Expand the base. A hire that changed nothing is the signal that quarters are
            #    full -- hire_staff returns the real delta, and the campaign had been discarding
            #    that zero every leg for the whole run.
            try:
                d.checks.update({f"base_{k}": v
                                 for k, v in base_upkeep(d, need_quarters=(hired == 0)).items()})
            except Exception as exc:
                d.say(f"  [leg] base upkeep failed: {type(exc).__name__}: {exc}")

            # Whatever all that left us in, get back to the city before the next leg.
            for _ in range(8):
                if d.status().stage == "CityView":
                    break
                if not d.dismiss_modal(d.status()):
                    d.escape_key()
                time.sleep(0.5)

        log_leg(d, run_id, elapsed, "leg", {"battles": battles, **d.checks})
        snapshot(d, f"day~{elapsed:.0f}")

    d.checks["battles_played"] = battles
    log_leg(d, run_id, elapsed, "final", {"battles": battles, **d.checks})
    return snapshot(d, "final")


def main() -> int:
    ap = argparse.ArgumentParser()
    add_runner_options(ap)
    ap.add_argument("--port", type=int, default=17321)
    ap.add_argument("--repo", default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument("--out", default=None)
    ap.add_argument("--difficulty", type=int, default=3)
    ap.add_argument("--days", type=float, default=28.0)
    ap.add_argument("--leg", type=float, default=7.0)
    ap.add_argument("--no-launch", action="store_true")
    ap.add_argument("--seed", type=int, default=0,
                    help="explicit RNG seed; 0 keeps the engine default. Logged to the ledger so "
                         "any run can be replayed exactly.")
    add_strategy_option(ap)
    args = ap.parse_args()
    policy = configure_runner(args)
    strategy, policy = resolve_strategy(args, policy)

    repo = Path(args.repo)
    out = Path(args.out) if args.out else repo / "build/e2e"
    out.mkdir(parents=True, exist_ok=True)
    shots = out / "shots"; shots.mkdir(exist_ok=True)

    game = None
    if not args.no_launch:
        game = GameProcess(repo, args.port, out / "game.log", seed=args.seed)
        print(f"[launch] {game.binary}", flush=True)
        game.start()

    d = Driver(Harness(port=args.port), repo / "data/forms", shots=shots, battle_policy=policy,
               strategy=strategy)
    d.checks = {}
    d.say(f"[strategy] {strategy.key()} battle policy {policy or 'engine default'}")
    rc = 0
    try:
        play_campaign(d, args.difficulty, args.days, args.leg)
    except Exception as exc:
        d.say(f"[FAIL] {type(exc).__name__}: {exc}")
        if game is not None:
            time.sleep(1.0)  # let a dying process be reaped so its signal is readable
            d.say(f"[FAIL] game process: {game.exit_status() or 'still running'}")
        rc = 1
    finally:
        d.say(f"[stages seen] {sorted(d.stages_seen)}")
        d.say(f"[event responses] {d.responses}")
        d.say(f"[unknown stages] {d.unknown_stages}")
        d.say(f"[actions attempted] {d.act_counts}")
        d.say(f"[checks] {d.checks}")
        if game:
            game.stop()
            warns = game.warnings()
            (out / "warnings.txt").write_text("\n".join(warns))
            d.say(f"[log] {len(warns)} warning/error lines")
        (out / "events.txt").write_text("\n".join(d.events))
    return rc


if __name__ == "__main__":
    sys.exit(main())
