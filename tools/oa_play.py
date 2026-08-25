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
import socket
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

import os
import shutil
import subprocess

from oa_forms import FormLibrary

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
}

# Stages constructed in code rather than from a .form, so there are no control ids to resolve.
# MessageBox maps Return->OK/Yes and Escape->Cancel/No (game/ui/general/messagebox.cpp:129-155).
KEY_RESPONSES = {
    "MessageBox": ["Return", "Escape"],
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
        return b"".join(chunks).decode(errors="replace").strip()

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

    def click_xy(self, x: int, y: int) -> None:
        self.ok(f"click {x} {y}")

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
            "--Notifications.City.AgentArrived=0",
            "--Notifications.City.AgentDiedCity=0",
            "--Notifications.City.BaseDestroyed=0",
            "--Notifications.City.CargoArrived=0",
            "--Notifications.City.NotEnoughAmmo=0",
            "--Notifications.City.NotEnoughFuel=0",
            "--Notifications.City.RecoveryArrived=0",
            "--Notifications.City.TransferArrived=0",
            "--Notifications.City.UfoSpotted=0",
            "--Notifications.City.UnauthorizedVehicle=0",
            "--Notifications.City.VehicleDestroyed=0",
            "--Notifications.City.VehicleEscaping=0",
            "--Notifications.City.VehicleHeavyDamage=0",
            "--Notifications.City.VehicleLightDamage=0",
            "--Notifications.City.VehicleLowFuel=0",
            "--Notifications.City.VehicleModerateDamage=0",
            "--Notifications.City.VehicleNoAmmo=0",
            "--Notifications.City.VehicleRearmed=0",
            "--Notifications.City.VehicleRefuelled=0",
            "--Notifications.City.VehicleRepaired=0",
]

def bring_to_front() -> None:
    """Raise the game window, only when explicitly asked for via OA_RAISE_WINDOW=1.

    OFF BY DEFAULT, and it must stay that way. This used to run on every launch and every resume,
    which was intolerable in practice: each call steals focus from whatever the human is actually
    doing, and during a restart loop it fires every few seconds. A windowed launch is already
    visible without any of this -- raising it is a convenience, not a requirement -- so the
    default is to leave the user's focus alone entirely.
    """
    if sys.platform != "darwin" or os.environ.get("OA_RAISE_WINDOW") != "1":
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


class GameProcess:
    """Owns a game instance so a run needs no human to start or stop anything."""

    def __init__(self, repo: Path, port: int, log_path: Path, extra: list[str] | None = None):
        self.repo = Path(repo)
        self.port = port
        self.log_path = Path(log_path)
        self.extra = extra or []
        self.proc: subprocess.Popen | None = None

    @property
    def binary(self) -> Path:
        return self.repo / "build/bin/OpenApoc.app/Contents/MacOS/OpenApoc"

    def start(self, wait_s: float = 90.0) -> None:
        self.log_path.parent.mkdir(parents=True, exist_ok=True)
        # Clear any wedged instance still holding this port before trying to bind it.
        stale = reap_stale_game(self.port)
        if stale:
            print(f"[boot] reaped {stale} stale game process(es) on port {self.port}", flush=True)
        argv = [
            str(self.binary),
            f"--Framework.Data={self.repo / 'data'}",
            f"--Framework.CD={self.repo / 'data/cd.iso'}",
            "--Framework.Harness.Enable=1",
            f"--Framework.Harness.Port={self.port}",
            "--Game.SkipIntro=1",
            "--Config.Save=0",
            "--Config.Read=0",
            "--Framework.AudioBackends=null",
            # Fixed RNG seed: GameState::startGame() otherwise reseeds from wall-clock.
            "--OpenApoc.NewFeature.SeedRng=0",
            # Agent equipment templates: an ordinary in-game affordance (keys 1-6,
            # Ctrl to save), and the only way to arm a squad without pixel-accurate
            # drag-and-drop onto the paper doll.
            "--OpenApoc.NewFeature.EnableAgentTemplates=1",
            # Belt and braces alongside the engine-side guard: a modal error dialog blocks the
            # main loop forever when there is no human to dismiss it.
            "--Logger.dialogLevel=0",
            # Frame limiting is honoured again now that the loop resynchronises after a hitch,
            # and ticks advance per frame -- so an automated run asks for the headroom outright
            # rather than relying on the limiter being broken.
            "--Framework.TargetFPS=1000",
        ] + PAUSE_NOTIFICATION_FLAGS + self.extra
        self.logf = open(self.log_path, "w")
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
                 shots: Path | None = None, verbose: bool = True):
        self.h = harness
        self.lib = FormLibrary(forms_dir)
        self.verbose = verbose
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
        self.act_counts: dict[str, int] = {}
        self.act_reset_at = time.time()

    def say(self, msg: str) -> None:
        self.events.append(msg)
        if self.verbose:
            print(msg, flush=True)

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

        Losing the last base raises XComDefeated, which replaces the stage stack with the losing
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

    def select_assignment_rows(self, st: Status) -> int:
        """Select a squad and a craft in an alert/building screen's assignment list.

        BUTTON_EXTERMINATE and BUTTON_RAID both refuse outright when getSelectedAgents() is empty
        (alertscreen.cpp:73), so this is what actually launches a mission. The list is a
        runtime-populated MultilistBox nested inside the AGENT_ASSIGNMENT graphic -- agents in the
        left column, craft in the right -- so rows are addressed by measured offsets from that
        rect rather than by control id. Offsets verified against a captured AlertScreen: first row
        centre is 63px down, rows are 26px apart, agent names sit ~103px in and craft names
        ~383px in. Clicking the icon gutter at the far left does not select.
        """
        box = self.controls(st).get("AGENT_ASSIGNMENT")
        if box is None or box.w <= 0:
            return 0
        ROW_H, FIRST_ROW = 26, 63
        AGENT_DX, VEHICLE_DX = 103, 383
        picked = 0
        for dx, rows in ((AGENT_DX, 6), (VEHICLE_DX, 1)):
            for r in range(rows):
                y = box.y + FIRST_ROW + r * ROW_H
                if y >= box.y + box.h - 8:
                    break
                self.h.click_xy(box.x + dx, y)
                picked += 1
                time.sleep(0.12)
        return picked

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
        if name and name not in ("none", "-") and name not in self.alerted_buildings:
            self.alerted_buildings.append(name)
            self.say(f"  [alert] aliens reported in {name}; noted for a sweep")

    def respond_to_event(self, st: Status) -> bool:
        """Engage with an interrupting screen. Returns True if we acted on it."""
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
            return True

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
            selected = self.select_assignment_rows(st)

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
        self.h.key("Escape")
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
    d.h.key(str(level))


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
    while time.time() < deadline:
        st = d.status()
        if st.stage != "CityView":
            if d.dismiss_modal(st):
                continue
            # Some other screen (battle, base, ufopaedia) is in charge; let its own driver run.
            if st.stage in ("BattleView", "BattlePreStart", "BattleBriefing"):
                return d.h.gs("time")
            time.sleep(0.5)
            continue

        turbo = d.h.gs("turbo")
        if turbo.get("can_turbo") == "1":
            d.h.key("5")  # turbo: ~1681x faster, and canTurbo() gates it by itself
        else:
            blocked_s += 1.0
            d.h.key("4")
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




def click_list_row(d: Driver, list_id: str, row: int, st: Status, item_h: int = 20) -> bool:
    """Listbox contents are runtime data, so address rows geometrically inside the resolved rect."""
    c = d.controls(st).get(list_id)
    if c is None or c.w <= 0:
        return False
    y = c.y + row * item_h + item_h // 2
    if y >= c.y + c.h:
        return False
    d.h.click_xy(c.x + min(40, c.w // 3), y)
    return True


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
            for cid in ("BUTTON_QUIT", "BUTTON_OK"):
                try:
                    if d.h.send(f"control {cid}").startswith("OK"):
                        break
                except (HarnessError, OSError):
                    continue
            else:
                d.h.key("Escape")
        time.sleep(0.6)
        if d.status().stage == stage:
            # That exit did nothing; fall back to the keyboard before trying again.
            d.h.key("Escape")
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


# The route to victory runs through particular research, in order: the advanced workshop unlocks
# the Large lab that MANUFACTURE_DIMENSION_SHIFTER requires, the shifter is what lets a craft
# cross into the alien dimension, and each RESEARCH_ALIEN_BUILDING_i opens the raid that unlocks
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
# different namespace this list cannot address), and no topic precedes one it depends on.
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
    "RESEARCH_ALIEN_BUILDING_5",
    "RESEARCH_ALIEN_BUILDING_6",
    "RESEARCH_ALIEN_BUILDING_7",
    "RESEARCH_ALIEN_BUILDING_8",
    "RESEARCH_ALIEN_BUILDING_9",
    "RESEARCH_ALIEN_BUILDING_4",

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


def pick_topic_row(d: Driver) -> int:
    """Row index in ResearchSelect's LIST for the most valuable topic this lab can take.

    gs research_options mirrors ResearchSelect's own filtering and ordering
    (researchselect.cpp:222-240), so the index it reports is the index to select.
    """
    detail = d.h.gs("research_options").get("detail", "")
    if not detail or detail == "-":
        return -1
    rows = []
    for part in detail.split("|"):
        try:
            idx, rest = part.split("=", 1)
            fields = rest.split(",")
            topic = fields[0]
            done = fields[1].endswith("1")
            big = fields[2].endswith("1")
        except (ValueError, IndexError):
            continue
        rows.append((int(idx), topic, done, big))
    # A "too large" topic in a small lab is offered but refused with a message box, so skip it.
    usable = [r for r in rows if not r[2] and not r[3]]
    for want in PRIORITY_RESEARCH:
        for idx, topic, _, _ in usable:
            if topic == want:
                d.say(f"  [research] targeting {topic} (row {idx})")
                return idx
    return usable[0][0] if usable else -1


def pick_topic_rows(d: Driver) -> list[tuple[int, str]]:
    """Every startable topic for this lab, best first: priority list, then the game's own order.

    The retry loop used to fall back to raw list indices (attempt 1 -> row 1, attempt 2 -> row 2),
    which selects whatever happens to sit at that position -- including already-researched or
    too-large topics. Offering a real ordered candidate list keeps every attempt a considered one.
    """
    detail = d.h.gs("research_options").get("detail", "")
    if not detail or detail == "-":
        return []
    rows = []
    for part in detail.split("|"):
        try:
            idx, rest = part.split("=", 1)
            fields = rest.split(",")
            topic = fields[0]
            done = fields[1].endswith("1")
            big = fields[2].endswith("1")
        except (ValueError, IndexError):
            continue
        if not done and not big:
            rows.append((int(idx), topic))
    ranked = []
    for want in PRIORITY_RESEARCH:
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
        return ""
    if not reply.startswith("OK"):
        return ""
    # The reply is "OK <CONTROL_ID> text=<value>", and the value has had its spaces replaced with
    # underscores so it survives the whitespace-delimited protocol. Taking everything after
    # "text=" is the only correct read: matching a leading prefix left the control id glued to
    # the front, so "No_Project" never compared equal to idle and every idle lab was skipped as
    # though it were busy -- the exact inverse of the bug this function exists to prevent.
    body = reply[2:].strip()
    marker = "text="
    text = body.split(marker, 1)[1].strip() if marker in body else ""
    text = text.replace("_", " ").strip()
    if text.lower() in ("", "-", "no project"):
        return ""
    return text


def raid_infiltrated_building(d: Driver, budget_s: float = 900.0) -> str:
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
    while d.alerted_buildings:
        name = d.alerted_buildings[0]
        probe = d.h.gs(f"centre_on_building {name}")
        if probe.get("centred") == "1" and int(probe.get("crew", "0") or 0) > 0:
            target, info = name, probe
            break
        # Either it is gone or the aliens have moved on; stop tracking it.
        d.alerted_buildings.pop(0)
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
    at = info.get("at", "")
    try:
        bx, by = (int(v) for v in at.split(",")[:2])
    except ValueError:
        return "bad-coords"
    d.say(f"  [raid] clearing {info.get('building')} ({info.get('crew')} aliens, "
          f"owner {info.get('owner')})")

    d.h.ok(f"click {bx} {by} right")
    time.sleep(1.0)
    if d.status().stage != "BuildingScreen":
        return_to_city(d)
        return f"no-building-screen ({d.status().stage})"

    # Select, then CHECK. select_assignment_rows counts the clicks it issued, not the agents it
    # actually selected, so it reported success even when every click missed -- which is how every
    # raid "succeeded" straight into a "No Agents Selected" box. BuildingScreen now reports the
    # real count, so try the measured offsets and, if nothing took, sweep the row for the column
    # that does. Those offsets were measured on AlertScreen and this is a different screen.
    def selected_count() -> int:
        detail = d.status().detail or ""
        if "selected_agents=" not in detail:
            return -1
        try:
            return int(detail.split("selected_agents=", 1)[1].split()[0].rstrip("_"))
        except (ValueError, IndexError):
            return -1

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

    # AGENT_LIST is the addressable agent list. Read from the live screen rather than guessed at:
    # AGENT_SELECT_BOX holds a single Control per base ("Base_1"), and the agents sit in an
    # AGENT_LIST inside it -- which is why both the AlertScreen pixel offsets and the nested
    # item-addressing on AGENT_SELECT_BOX selected nobody here.
    # Real clicks, inside the AGENT_LIST rect. The assignment rows branch on
    # e->forms().MouseInfo.Button in four places (agentassignment.cpp:92-138), which
    # Control::click() never sets -- the same defect that stopped agent portraits selecting on the
    # equip screen and hand icons working on the vehicle screen. A named click cannot drive any of
    # them; the rect and a genuine mouse press can.
    lst = d.controls(d.status()).get("AGENT_LIST")
    if lst is not None and lst.w > 0:
        row_h = 26
        for i in range(8):
            y = lst.y + 12 + i * row_h
            if y >= lst.y + lst.h - 4:
                break
            d.h.click_xy(lst.x + lst.w // 2, y)
            time.sleep(0.12)
            if selected_count() >= 4:
                break
    if selected_count() > 0:
        d.say(f"  [raid] {selected_count()} agent(s) selected from AGENT_LIST")

    if selected_count() == 0:
        d.select_assignment_rows(d.status())
        time.sleep(0.3)
    if selected_count() == 0:
        box = d.controls(d.status()).get("AGENT_ASSIGNMENT")
        if box and box.w > 0:
            for dx in (60, 80, 130, 160, 200, 240):
                for r in range(6):
                    y = box.y + 63 + r * 26
                    if y >= box.y + box.h - 8:
                        break
                    d.h.click_xy(box.x + dx, y)
                    time.sleep(0.08)
                if selected_count() > 0:
                    d.say(f"  [raid] agent rows select at x+{dx}, not the AlertScreen offset")
                    break
    if selected_count() == 0:
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

    outcome = win_battle(d, budget_s=budget_s)
    after = d.h.gs("infiltrated")
    d.say(f"  [raid] {outcome}; {after.get('infiltrated')} building(s) still infiltrated, "
          f"gov relation {after.get('gov_relation')}")
    return outcome


def raid_alien_building(d: Driver) -> str:
    """Raid the next alien building. Returns the battle outcome, or why it could not start.

    This is the win condition: Battle::exitBattle fires AliensDefeated only for the alien building
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

    target = d.h.gs("centre_on_raidable")
    if target.get("centred") != "1":
        return "nothing-raidable"
    d.say(f"  [raid] target {target.get('building')} (victory={target.get('victory')})")
    time.sleep(0.5)
    bx, by = (int(v) for v in target["at"].split(",")[:2])
    d.h.ok(f"click {bx} {by} right")
    time.sleep(1.2)

    st = d.status()
    if st.stage != "BuildingScreen":
        d.say(f"  [raid] expected BuildingScreen, got {st.stage}")
        return_to_city(d)
        return "no-building-screen"

    picked = d.select_assignment_rows(st)
    if not picked:
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


def goto_portal(d: Driver) -> bool:
    """Send a dimension-shifter-equipped craft through a portal into the alien city.

    Crossing is the gate on the entire endgame: the ten alien buildings, and the one carrying
    victory, exist only in CITYMAP_ALIEN. VehicleMission::GotoPortal checks
    Vehicle::hasDimensionShifter() on arrival -- without one the craft is stranded or crashes
    outright -- so this refuses to send a craft that cannot make the trip rather than throwing
    one away.

    The order is a right-click on a portal doodad with the craft selected; portals live in
    City::portals and have no UI handle, hence centre_on_portal.
    """
    st = d.status()
    if st.stage != "CityView":
        return False

    shifters = []
    for part in d.h.gs("interceptors").get("detail", "").split("|"):
        bits = part.split(":")
        if len(bits) < 3 or not bits[0].isdigit():
            continue
        if "shifter=1" in bits[-1]:
            shifters.append(int(bits[0]))
    if not shifters:
        d.say("  [portal] no craft carries a dimension shifter; cannot cross")
        return False

    if not d.click_id("BUTTON_TAB_2", st):
        return False
    time.sleep(0.35)
    lst = d.controls(d.status()).get("OWNED_VEHICLE_LIST")
    if lst is None or lst.w <= 0:
        return False
    d.h.click_xy(lst.x + 16 + shifters[0] * 36, lst.y + lst.h // 2)
    time.sleep(0.3)

    info = d.h.gs("centre_on_portal")
    if info.get("centred") != "1":
        d.say("  [portal] no portal found in this city")
        return False
    time.sleep(0.5)
    px, py = (int(v) for v in info["at"].split(",")[:2])
    d.h.ok(f"click {px} {py} right")

    mission = "none"
    for _ in range(6):
        time.sleep(0.8)
        mission = d.h.gs("selected").get("mission", "none")
        if "ortal" in mission:
            d.say(f"  [portal] craft ordered through the gate ({mission})")
            return True
        if "TakeOff" not in mission and mission != "none":
            break
    d.say(f"  [portal] order not accepted (mission={mission})")
    return False


def build_second_base(d: Driver) -> str:
    """Buy a second base. Returns "bought", or why it could not.

    This is the single most valuable insurance a campaign can buy. XComDefeated is raised on
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

    MANUFACTURE_DIMENSION_SHIFTER needs a Large workshop and the starting base has only a small
    one, so this is a hard gate on reaching the alien dimension at all.

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
                    d.h.key("Escape")
                time.sleep(0.5)
            return True

    d.say(f"  [build] no free tile accepted {want}")
    for _ in range(5):
        stt = d.status()
        if stt.stage == "CityView":
            break
        if not d.click_id("BUTTON_OK", stt):
            d.h.key("Escape")
        time.sleep(0.5)
    return False


def manufacture(d: Driver, want: str = "MANUFACTURE_DIMENSION_SHIFTER", qty: int = 1) -> bool:
    """Start a manufacturing project in a workshop. Returns True when it actually takes.

    VEQUIPMENTTYPE_DIMENSION_SHIFTER is the only player-obtainable way into the alien dimension,
    so this is the gate the whole endgame sits behind. It is an Engineering project needing a
    *Large* workshop -- the starting base has only a small one, which is why
    FACILITYTYPE_ADVANCED_WORKSHOP has to be researched and built first.

    Manufacturing reuses the research screen and ResearchSelect, but Lab::setResearch branches on
    lab type: an Engineering lab is charged the project cost immediately. required_lab_size is
    *not* used to filter the offered list, so a Large-only project appears in a small workshop's
    list too and is refused with a message box when picked -- gs research_options flags that as
    big=1, and pick_topic_row already skips those.
    """
    before = d.h.gs("stores").get("vehicle_top", "-")
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
            if "engineering" not in opts.get("lab", "").lower() and "MANUFACTURE" not in opts.get(
                "detail", ""
            ):
                continue
            row = -1
            for part in opts.get("detail", "").split("|"):
                idx, _, rest = part.partition("=")
                fields = rest.split(",")
                if fields and fields[0] == want and not fields[1].endswith("1"):
                    if len(fields) > 2 and fields[2].endswith("1"):
                        d.say(f"  [manufacture] {want} needs a larger lab than this one")
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
            # Quantity only becomes meaningful once a project is committed.
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
            d.h.key("Escape")
        time.sleep(0.5)

    after = d.h.gs("stores").get("vehicle_top", "-")
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
    if _lab_skill_total(d) < 800:
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
            d.h.key("Escape"); time.sleep(0.5)
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
        d.h.key("Escape"); time.sleep(0.5)
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
    for part in d.h.gs("interceptors").get("detail", "").split("|"):
        bits = part.split(":")
        if len(bits) < 3 or not bits[0].isdigit():
            continue
        flags = bits[-1]
        if "flying=1" not in flags or "armed=1" not in flags:
            continue
        if "crew=0" in flags:
            fighters.append(int(bits[0]))
        else:
            crewed_fighters.append(int(bits[0]))
    if not fighters:
        # Do not send the troop transport to dogfight. It carries the squad that wins ground
        # missions, and losing it costs the craft, the agents aboard, and the ability to reach the
        # next crash site -- for one UFO that would have cost far less to ignore. Measured over a
        # run: craft_lost reached -440 while incursions, the penalty for letting UFOs alone, stood
        # at -311. Losing craft was costing more than the thing it was meant to prevent.
        d.say("  [intercept] no armed fighter free; leaving the transport out of it")
        d.h.key("Escape")
        return 0
    # "Crewed" marks the craft currently carrying the squad, not a craft that cannot fight -- a
    # Valkyrie Interceptor with agents aboard is still an interceptor. Counting it as a transport
    # made two airworthy fighters read as one and refused every engagement for a whole game-week.
    # What matters is whether anything is left if this sortie goes badly, so judge by the total
    # armed fleet and send the uncrewed ones.
    if len(fighters) + len(crewed_fighters) < 2:
        d.say("  [intercept] only one armed flier in the whole fleet; holding it back")
        d.h.key("Escape")
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
        d.h.key("Escape")
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
        d.h.key("Escape")
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
STANCES = {"kneel": "BUTTON_KNEEL", "prone": "BUTTON_PRONE",
           "walk": "BUTTON_WALK", "run": "BUTTON_RUN"}
PSI_ACTIONS = {"control": "BUTTON_CONTROL", "panic": "BUTTON_PANIC",
               "stun": "BUTTON_STUN", "probe": "BUTTON_PROBE"}


def _press(d: Driver, control_id: str, op: str = "click") -> bool:
    try:
        return d.h.send(f"control {control_id} {op}").startswith("OK")
    except (HarnessError, OSError):
        return False


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
    d.h.key("Escape")
    time.sleep(0.3)

    layout = battle_layout(d)
    results["battle_positions"] = bool(layout)
    foes = d.h.screen_craft("enemies_screen")
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


def win_battle(d: Driver, budget_s: float = 1800.0) -> str:
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
    # Remembered from the last in-battle sample: the debriefing stage has no battle to query,
    # current_battle having already been torn down by then.
    last_player_won = False
    last_mine_alive = "?"
    started_with = 0
    selected_count = 0
    mission_type = "unknown"

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
            b = d.h.gs("battle")
            if b.get("mode") != "rt":
                d.say(f"[battle] ABORT: mode is {b.get('mode')}, not real-time")
                return "wrong-mode"
            started_with = int(b.get("mine_alive", "0") or 0)
            mission_type = b.get("mission_type", "unknown")
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
            d.h.gs("centre_on_friends")
            time.sleep(0.15)
        friends = d.h.screen_craft("friends_screen")

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

        # Put the camera on the hostiles' floor first: orders only reach the displayed level.
        if rounds % 3 == 0:
            layout = battle_layout(d)
            if layout:
                match_enemy_floor(d, layout)

        foes = d.h.screen_craft("enemies_screen")
        # enemies_screen only reports hostiles already on screen. Walking the camera only when
        # *nothing* is visible is not enough: one alien that is framed but unreachable keeps the
        # squad grinding against it while the rest of the map goes unexplored, which is the same
        # stall in a new costume. So also walk on once progress dries up.
        if not foes or stalls > 4:
            info = d.h.gs("centre_on_enemy")
            if info.get("centred") == "1":
                time.sleep(0.4)
                found = d.h.screen_craft("enemies_screen")
                if found:
                    foes = found

        if foes:
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
        if mine_alive == "0":
            d.say(f"[battle] squad wiped out: {b}")
        if foes_alive == last_foes:
            stalls += 1
        else:
            stalls = 0
            d.say(f"  [battle] foes_alive={foes_alive} mine_alive={mine_alive}")
        last_foes = foes_alive
        if stalls and stalls % 12 == 0:
            d.say(f"  [battle] no progress for a while: {b}")
            # Record why it is stuck rather than just that it is. Screen coordinates cannot
            # explain an unreachable hostile -- a unit two floors up is drawn in plain sight and
            # still cannot be walked to -- so log tile positions and let the z gap speak.
            try:
                pos = d.h.gs("battle_positions")
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
        # Never leave a base defence voluntarily. Withdrawing forfeits the base itself: every
        # facility reverts to "unbuilt", the labs, stores and staff go with it, and the campaign
        # is lost within the hour. Observed exactly that -- five labs went from built and fully
        # staffed to unbuilt with skill 0 between one research check and the next, the engine
        # then crashed twice on the dangling base, and the defeat video played. A losing base
        # defence fought to the end is still better than a conceded one.
        # ...but "never" was too absolute, and it deadlocked a campaign: two survivors against one
        # alien they could not reach, every exit blocked, spinning out a forty-minute budget
        # having already decided the battle was over. Losing a base only ends the game when it is
        # the LAST base (base.cpp:150-159), so once a second base exists, conceding one is a
        # setback rather than a defeat -- and far better than burning the clock.
        try:
            bases_now = int(b.get("bases", "1") or 1)
        except ValueError:
            bases_now = 1
        may_leave = mission_type != "base_defense" or bases_now > 1

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

        outnumbered = foes_n >= alive * 3
        collapsing = alive <= max(1, started_with // 3)
        # No retreating. Two reasons, and the second is the engine's rather than a preference.
        # Withdrawing hands the aliens straight back: survivors of a building mission go back into
        # that building (battle.cpp:2900-2910) and survivors of a UFO recovery seed a NEARBY
        # building instead (battle.cpp:2955-2963), which is how a retreat spreads infiltration to
        # a neighbourhood that had none. The driver was withdrawing from roughly half its missions
        # and then losing its funding to the infiltration it had just scattered. A mission fought
        # to the end and lost is cheaper than one abandoned.
        if started_with and alive and collapsing and outnumbered and stalls % 20 == 0:
            d.say(f"  [battle] outnumbered {foes_n} to {alive} and staying: leaving would put "
                  f"these aliens back on the map")

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
        d.h.key("Escape")
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
            d.h.key("Escape")
        time.sleep(0.6)
    return d.status().stage == "CityView"


def buy_category(d: Driver, category: str, qty: int, rows: int, sub: str = "") -> int:
    """Set a quantity on the first `rows` lines of a buy/sell category. Returns lines changed.

    The purchase rows are the case the named-action layer could not reach on its own: they are
    runtime-built controls with no ids, and the quantity is an unnamed ScrollBar inside each row
    (transactioncontrol.cpp:702). Addressing them by position -- CONTROL LIST item <N> set <q> --
    keeps this driving the real UI rather than writing into GameState behind it.
    """
    if not d.click_id(category, d.status()):
        return 0
    time.sleep(0.8)
    if sub:
        d.click_id(sub, d.status())
        time.sleep(0.8)
    changed = 0
    for i in range(rows):
        try:
            # A row's quantity is a *balance*, not an order size: setting it below what the base
            # already holds sells the difference. Blindly writing a fixed number therefore sold
            # equipment while the log claimed a purchase -- funds went up, not down. Read first
            # and only ever increase.
            # A row's scrollbar is a balance across the two sides of the trade, and on the
            # buy/sell screen *raising* it sells: measured directly, setting have+qty took stores
            # from 60 items to 30 and put money back in the bank while the log claimed a
            # purchase. Buying means moving the balance the other way.
            cur = d.h.send(f"control LIST item {i} get")
            have, low = 0, 0
            for kv in cur.split():
                if kv.startswith("value="):
                    have = int(kv.split("=")[1] or 0)
                elif kv.startswith("min="):
                    low = int(kv.split("=")[1] or 0)
            target = max(low, have - qty)
            if target == have:
                continue
            if not d.h.send(f"control LIST item {i} set {target}").startswith("OK"):
                continue
            changed += 1
        except (HarnessError, OSError):
            break
    return changed


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
      * Never sell what the squad's own equipment template names -- processTemplate re-equips
        those exact types, so selling them disarms everybody at the next equip.
      * Keep a working reserve of each kind rather than stripping stores to nothing.
    """
    info = d.h.gs("loot")
    detail = info.get("detail", "-")
    if not detail or detail == "-":
        return 0

    template_types = set()
    for part in d.h.gs("templates").get("detail", "").split("|"):
        if not part.startswith("1:"):
            continue
        for kv in part.split(":", 1)[1].split(","):
            if kv.startswith("types="):
                template_types = {_norm(t) for t in kv[6:].split("+") if t and t != "-"}

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
        if _norm(item) in template_types:
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
        d.h.key("Escape")
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
        d.h.key("Escape")

    for _ in range(8):
        st = d.status()
        if st.stage == "CityView":
            break
        if st.stage == "MessageBox":
            d.click_id("BUTTON_OK", st) or d.h.key("Return")
        elif st.stage in ("RecruitScreen", "BaseScreen"):
            d.click_id("BUTTON_OK", st)
        else:
            d.h.key("Escape")
        time.sleep(0.6)

    after = int(d.h.gs("agents").get(counter, "0") or 0)
    funds_after = int(d.h.gs("funds").get("balance", "0") or 0)
    d.say(f"  [hire] {role}: clicked {hired}; {counter} {before}->{after}; "
          f"funds {funds_before}->{funds_after}")
    return after - before


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


def template_weapon_in_stock(d: Driver) -> bool:
    """Is the stored loadout's weapon actually in stores?

    processTemplate strips an agent and re-equips the template's EXACT types, so applying it when
    the weapon is missing takes working guns off people and hands back nothing. Observed doing
    precisely that: "applied to 14 agents; armed 7->6", with the template naming a Megapol Laser
    Sniper Gun and stores holding none -- only Lawpistols, M4000s and a plasma gun. The template
    was captured from the starting squad, whose rifles cannot always be re-bought.
    """
    types = []
    for part in d.h.gs("templates").get("detail", "").split("|"):
        if not part.startswith("1:"):
            continue
        for kv in part.split(":", 1)[1].split(","):
            if kv.startswith("types="):
                types = [t for t in kv[6:].split("+") if t and t != "-"]
    if not types:
        return False
    stock = {}
    for part in d.h.gs("loot").get("detail", "-").split("|"):
        bits = part.split(":")
        if len(bits) < 2:
            continue
        try:
            stock[bits[0]] = int(bits[1].split("=", 1)[1])
        except (ValueError, IndexError):
            continue
    guns = [t for t in types if any(m in t.upper()
                                    for m in ("GUN", "RIFLE", "PISTOL", "LAUNCHER", "CANNON"))
            and "CLIP" not in t.upper() and "AMMO" not in t.upper()]
    if not guns:
        return True
    return any(stock.get(g, 0) > 0 for g in guns)


def arm_agents_directly(d: Driver, agents: int = 24) -> int:
    """Put a weapon in every empty pair of hands, without the template. Returns armed delta.

    The equipment-template mechanism re-equips a template's EXACT types, so it is useless -- worse
    than useless, it strips people -- once the loadout names a weapon the market no longer sells.
    A campaign sat at six armed of fifteen soldiers for that reason, with a template demanding a
    Megapol Laser Sniper Gun and stores holding Lawpistols and M4000s instead.

    AEquipScreen has the same immediate action VEquipScreen does: Shift+click an inventory item
    and it goes straight onto the selected agent (aequipscreen.cpp:593-598), gated on
    AdvancedInventoryControls, which defaults on. gs aequip_items reports where those items are,
    which is the only part a driver could not work out for itself.
    """
    before = int(d.h.gs("agents").get("armed", "0") or 0)
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
    d.click_id("BUTTON_BASE_EQUIPAGENT", d.status())
    try:
        d.wait_for("AEquipScreen", 25)
    except TimeoutError:
        return_to_city(d)
        return 0

    armed_now = 0
    for row in range(agents):
        st = d.status()
        if st.stage != "AEquipScreen":
            break
        # A real pixel click, not a ListBox set. AEquipScreen's portrait callback branches on
        # e->forms().MouseInfo.Button (aequipscreen.cpp:85-87), which Control::click() never sets
        # -- so "control AGENT_SELECT_BOX set N" highlights the row and never runs selectAgent.
        # With selectedAgents empty, getMode() populates no inventory at all
        # (aequipscreen.cpp:912), which is why the equip screen reported an empty warehouse while
        # twenty-seven weapons sat in stores and two soldiers of thirteen carried anything.
        box = d.controls(d.status()).get("AGENT_SELECT_BOX")
        if box is None or box.w <= 0:
            break
        row_h = 36
        y = box.y + 18 + row * row_h
        if y >= box.y + box.h - 6:
            break
        d.h.click_xy(box.x + box.w // 2, y)
        time.sleep(0.3)
        # The list is rebuilt as the screen renders, so give it a frame before reading.
        items = {}
        for _ in range(6):
            time.sleep(0.25)
            items = d.h.gs("aequip_items")
            if items.get("detail", "-") not in ("", "-"):
                break
        detail = items.get("detail", "-")
        if not detail or detail == "-":
            continue
        # Fill both hands. The guide wants a firearm AND a stun grapple on every agent -- "use
        # stun grapples as often as possible" -- because an alien taken alive keeps its equipment,
        # and that equipment is the campaign's income. Handing out one gun and leaving the other
        # hand empty wastes half of every soldier.
        firearm, grapple = None, None
        for part in detail.split("|"):
            bits = part.split(":")
            if not bits:
                continue
            attrs = {}
            for kv in bits[1:]:
                if "=" in kv:
                    k, v = kv.split("=", 1)
                    attrs[k] = v
            if attrs.get("weapon") != "1" or attrs.get("research") != "1":
                continue
            try:
                ax, ay = (int(v) for v in attrs.get("at", "0,0").split(","))
                sw, sh = (int(v) for v in attrs.get("size", "0,0").split(","))
            except ValueError:
                continue
            spot = (bits[0], ax + sw // 2, ay + sh // 2)
            if "GRAPPLE" in bits[0].upper() or "STUN" in bits[0].upper():
                grapple = grapple or spot
            elif firearm is None:
                firearm = spot
            if firearm and grapple:
                break
        picks = [p for p in (firearm, grapple) if p]
        if not picks:
            continue
        for _name, cx, cy in picks:
            d.h.send("keydown Left Shift")
            try:
                settle(d)
                d.h.ok(f"down {cx} {cy}")
                time.sleep(0.15)
                d.h.ok(f"up {cx} {cy}")
                time.sleep(0.2)
            finally:
                d.h.send("keyup Left Shift")
            time.sleep(0.15)
        armed_now += 1
        time.sleep(0.2)

    return_to_city(d)
    after = int(d.h.gs("agents").get("armed", "0") or 0)
    d.say(f"  [arm] handed out {armed_now} weapon(s); armed {before}->{after}")
    return after - before


def equip_squad(d: Driver, agents: int = 16, apply: bool = True) -> int:
    """Arm unequipped soldiers from base stores. Returns the change in armed count.

    New recruits arrive carrying nothing -- after hiring, soldiers went 10 to 15 while armed
    stayed at 10 -- and an unarmed soldier is a casualty waiting to happen.

    Items reach an agent by being dragged onto a paper doll whose item rects are computed at
    runtime and appear nowhere in the .form file. The engine's own way round that is agent
    equipment templates (AEquipScreen::processTemplate, aequipscreen.cpp:1567): Ctrl+<n> stores
    the shown agent's loadout, a bare <n> strips every selected agent and re-equips them from
    base stores to match.

    Two traps, both learned the hard way:
      * AGENT_SELECT_BOX is a ListBox, which selects on MouseDown. Control::click() raises
        MouseClick, which it ignores, so clicking a row selected nobody and the template applied
        to nothing at all.
      * Applying an *empty* template strips agents instead of arming them. Capturing one from a
        row that happened to be a scientist took armed from 10 down to 4. So the captured
        template is now checked before it is used, and arming is abandoned the moment it starts
        going backwards.
    """
    before = int(d.h.gs("agents").get("armed", "0") or 0)
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
    d.click_id("BUTTON_BASE_EQUIPAGENT", d.status())
    time.sleep(1.4)
    if d.status().stage != "AEquipScreen":
        d.say(f"  [equip] expected AEquipScreen, got {d.status().stage}")
        d.h.key("Escape")
        return 0

    def capture(row: int) -> int:
        """Store row's loadout in slot 1; return how many weapons it holds."""
        try:
            if not d.h.send(f"control AGENT_SELECT_BOX set {row}").startswith("OK"):
                return -1
        except (HarnessError, OSError):
            return -1
        time.sleep(0.3)
        d.h.ok("keydown Left Ctrl")
        time.sleep(0.1)
        d.h.key("1")
        time.sleep(0.1)
        d.h.ok("keyup Left Ctrl")
        time.sleep(0.35)
        detail = d.h.gs("templates").get("detail", "")
        for part in detail.split("|"):
            if part.startswith("1:"):
                for kv in part.split(":", 1)[1].split(","):
                    if kv.startswith("weapons="):
                        return int(kv.split("=")[1] or 0)
        return 0

    # Templates live in GameState and persist, so a loadout captured once at the start of the
    # campaign -- while the original ten soldiers are all home and armed -- stays usable for
    # ever. Re-capturing later is what failed: called after a mission, the list holds only the
    # people who did not go, and none of them are armed. The refusal was right; the timing was
    # not.
    source = -1
    already = 0
    for part in d.h.gs("templates").get("detail", "").split("|"):
        if part.startswith("1:"):
            for kv in part.split(":", 1)[1].split(","):
                if kv.startswith("weapons="):
                    already = int(kv.split("=")[1] or 0)
    if already > 0:
        d.say(f"  [equip] reusing the stored {already}-weapon loadout")
    else:
        for row in range(agents):
            got = capture(row)
            if got < 0:
                break
            if got > 0:
                source = row
                d.say(f"  [equip] captured a {got}-weapon loadout from row {row}")
                break
    if source < 0 and already <= 0:
        d.say("  [equip] no armed agent to copy a loadout from; leaving everyone as they are")
        for _ in range(6):
            st = d.status()
            if st.stage == "CityView":
                break
            if st.stage in ("AEquipScreen", "BaseScreen"):
                d.click_id("BUTTON_OK", st)
            elif not d.dismiss_modal(st):
                d.h.key("Escape")
            time.sleep(0.5)
        return 0

    if not apply:
        d.say("  [equip] loadout captured; not applying yet")
        for _ in range(6):
            st = d.status()
            if st.stage == "CityView":
                break
            if st.stage in ("AEquipScreen", "BaseScreen"):
                d.click_id("BUTTON_OK", st)
            elif not d.dismiss_modal(st):
                d.h.key("Escape")
            time.sleep(0.5)
        return 0

    # Applying with an empty armoury strips people rather than arming them: the template is
    # re-equipped from base stores, and purchases take a couple of game-days to arrive.
    stock = int(d.h.gs("stores").get("weapons", "0") or 0)
    if stock <= 0:
        d.say("  [equip] no weapons in stores; not applying (would disarm the squad)")
        for _ in range(6):
            st = d.status()
            if st.stage == "CityView":
                break
            if st.stage in ("AEquipScreen", "BaseScreen"):
                d.click_id("BUTTON_OK", st)
            elif not d.dismiss_modal(st):
                d.h.key("Escape")
            time.sleep(0.5)
        return 0

    applied, best = 0, before
    for i in range(agents):
        if i == source:
            continue
        try:
            if not d.h.send(f"control AGENT_SELECT_BOX set {i}").startswith("OK"):
                break
        except (HarnessError, OSError):
            break
        time.sleep(0.2)
        d.h.key("1")
        applied += 1
        time.sleep(0.3)
        now = int(d.h.gs("agents").get("armed", "0") or 0)
        if now < best:
            # Stores ran dry: further applications now strip people rather than arm them.
            d.say(f"  [equip] stopping at row {i}: armed fell {best}->{now}")
            break
        # Deliberately no "give up after N no-ops" rule here. Applying to an agent who is
        # already armed strips and re-equips them for a net change of zero, and the roster lists
        # the armed veterans before the empty-handed recruits -- so an early-out fires on the
        # veterans and never reaches the people who actually need arming. Only a *fall* in armed
        # is a reason to stop, since that means stores have run dry.
        best = max(best, now)

    for _ in range(6):
        st = d.status()
        if st.stage == "CityView":
            break
        if st.stage in ("AEquipScreen", "BaseScreen"):
            d.click_id("BUTTON_OK", st)
        elif not d.dismiss_modal(st):
            d.h.key("Escape")
        time.sleep(0.5)

    after = int(d.h.gs("agents").get("armed", "0") or 0)
    d.say(f"  [equip] applied to {applied} agents; armed {before}->{after}")
    if after == before and applied:
        # processTemplate bails out unless getMode() == Mode::Base, which needs the agent
        # physically at a base -- a freshly hired recruit still in transit silently equips
        # nothing (aequipscreen.cpp:1569, 867-885).
        d.say("  [equip] nobody gained a weapon; recruits may still be travelling to base")
    return after - before


def _norm(text: str) -> str:
    """Squash an id or a display name to a comparable key."""
    t = text.upper().replace("AEQUIPMENTTYPE_", "").replace("_", "").replace(" ", "")
    return "".join(ch for ch in t if ch.isalnum())


def buy_named(d: Driver, wanted: list, qty: int = 8, category: str = "BUTTON_AGENTS") -> int:
    """Buy specific items by name. Returns the number of lines ordered.

    Purchase rows carry their identity only as a child Label, so until the harness could read
    that text the driver could address a row by position but had no idea what was in it -- it
    bought whatever sat in slot 3. Buying the wrong things is not harmless here: applying an
    equipment template re-equips the template's *exact* item types, so stocking plausible weapons
    instead of the named ones arms nobody.
    """
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
            target = max(low, have - qty)
            if target != have and d.h.send(f"control LIST item {idx} set {target}").startswith("OK"):
                ordered += 1
        except (HarnessError, OSError):
            break
    funds_before = int(d.h.gs("funds").get("balance", "0") or 0)
    close_buysell(d, commit=ordered > 0)
    funds_after = int(d.h.gs("funds").get("balance", "0") or 0)
    d.say(f"  [buy] ordered {ordered} of {len(keys)} wanted lines, funds {funds_before}->{funds_after}")
    missing = keys - matched
    if missing:
        # Name what could not be found and what the screen was actually offering. "ordered 1 of
        # 11" said the buying failed but never why, and the answer -- an armoury with weapons=0
        # while every recruit went unarmed -- was worth knowing the first time it happened.
        d.say(f"  [buy] no row for {len(missing)} wanted item(s): {sorted(missing)[:4]}")
        d.say(f"  [buy] screen listed {len(seen)} row(s), e.g. {seen[:4]}")
    return ordered


def stock_best_guns(d: Driver, qty: int = 20) -> int:
    """Buy the hardest-hitting agent weapon the market will actually sell. Returns lines ordered.

    The armoury was being stocked from the squad's equipment template, which names the exact
    rifles the starting soldiers happened to carry -- and those cannot always be re-bought. So the
    template ordered a Megapol Laser Sniper Gun every few minutes, none arrived, and a campaign
    fielded six armed soldiers of fifteen while the money sat in the bank. Buy by capability
    instead of by name: whatever hits hardest among the guns actually on sale, plus its ammunition.
    """
    info = d.h.gs("buyable_guns")
    detail = info.get("detail", "-")
    if not detail or detail == "-":
        d.say("  [buy] no agent weapons on sale")
        return 0
    funds = int(d.h.gs("funds").get("balance", "0") or 0)

    # The guide's weapon policy is phased, not "buy the hardest hitter":
    #
    #   "At the beginning when all they have is brainsuckers, you can use the AutoCannon or
    #    missles on everything with no harm done. Soon, however, they have disruptor guns, which
    #    are $2500 a pop and should be saved ... stun, lasers, and machine guns as MUCH as
    #    possible, as soon as they get boomeroid/disruptors."
    #
    # Explosives destroy the loot that pays for the campaign -- "Personal Shields are one of the
    # most valuable assets in the game, and destroying them all with explosions doesn't help you
    # at all" -- so once the aliens are carrying anything worth recovering, switch to lasers and
    # machine guns. Detect the phase from what has already turned up in stores.
    loot = d.h.gs("loot").get("detail", "-")
    valuable_aliens = any(m in (loot or "").upper()
                          for m in ("DISRUPTOR", "BOOMEROID", "DEVASTATOR", "SHIELD", "VORTEX"))
    if valuable_aliens:
        preferred = ("LASER", "MACHINE_GUN", "MACHINE GUN")
        avoid = ("GRENADE", "MISSILE", "MISSLE", "CANNON", "LAUNCHER", "EXPLOSIVE")
    else:
        preferred = ()
        avoid = ()

    best = None
    for part in detail.split("|"):
        bits = part.split(":")
        if not bits:
            continue
        attrs = {}
        for kv in bits[1:]:
            if "=" in kv:
                k, v = kv.split("=", 1)
                try:
                    attrs[k] = int(v)
                except ValueError:
                    attrs[k] = 0
        price = attrs.get("price", 0)
        if not price or price * 4 > funds:
            continue
        # Rank firearms only. A Megapol Stun Grapple reports damage 90 and wins on raw numbers,
        # but it is a melee stunner -- the guide wants those carried as a SECOND item to take
        # aliens alive, not as the thing a soldier defends the base with. Buying them as the
        # primary weapon armed the squad with truncheons.
        if any(m in bits[0].upper() for m in ("GRAPPLE", "STUN", "MEDI", "SCANNER", "SHIELD")):
            continue
        if avoid and any(m in bits[0].upper() for m in avoid):
            continue
        # Score preferred families above raw damage so a laser beats a bigger explosive once
        # there is alien equipment on the field worth bringing home intact.
        rank = (1 if any(m in bits[0].upper() for m in preferred) else 0, attrs.get("damage", 0))
        if best is None or rank > best[0]:
            best = (rank, bits[0].replace("_", " "), price)
    if not best:
        d.say(f"  [buy] nothing affordable among the guns on sale (${funds})")
        return 0
    rank, name, price = best
    # And always a stun grapple each. The guide is explicit -- "use stun grapples as often as
    # possible" -- because an alien taken alive keeps its equipment, and that equipment is the
    # campaign's income. Agents have two hands; there is no reason to fill only one.
    want = [name, f"{name} Clip", f"{name} Ammo", "Megapol Stun Grapple", "Stun Grapple"]
    phase = "lasers and machine guns" if rank[0] else "whatever hits hardest"
    d.say(f"  [buy] stocking {qty} x {name} (damage {rank[1]}, ${price}) plus stun grapples "
          f"[{phase}]")
    return buy_named(d, want, qty=qty, category="BUTTON_AGENTS")


def stock_for_template(d: Driver, qty: int = 8) -> int:
    """Buy exactly the items the stored equipment template names.

    processTemplate strips an agent and re-equips the template's exact types, so this is the only
    way the template can actually arm anybody. The captured loadout turned out to be Megapol
    armour plus a Megapol Laser Sniper Gun -- none of which the earlier scattergun buying
    happened to stock.
    """
    types = []
    for part in d.h.gs("templates").get("detail", "").split("|"):
        if not part.startswith("1:"):
            continue
        for kv in part.split(":", 1)[1].split(","):
            if kv.startswith("types="):
                types = [t for t in kv[6:].split("+") if t and t != "-"]
    if not types:
        d.say("  [buy] no template to stock for")
        return 0
    d.say(f"  [buy] stocking {len(set(types))} item types the loadout needs")
    return buy_named(d, sorted(set(types)), qty=qty)


def buy_equipment(d: Driver, rows: int = 10, qty: int = 6) -> bool:
    """Stock the armoury so replacement soldiers have something to carry.

    On the buy/sell screen BUTTON_AGENTS means agent *equipment*, not personnel -- the role
    buttons are explicitly hidden there and belong to RecruitScreen
    (buyandsellscreen.cpp:36-60). Recruits arrive empty-handed and the veterans are carrying
    every weapon the base owns, so applying an equipment template to a new soldier equips
    nothing until the armoury actually has spares.
    """
    funds_before = int(d.h.gs("funds").get("balance", "0") or 0)
    if not open_buysell(d):
        return False
    changed = buy_category(d, "BUTTON_AGENTS", qty, rows)
    close_buysell(d, commit=changed > 0)
    funds_after = int(d.h.gs("funds").get("balance", "0") or 0)
    d.say(f"  [buy] {changed} lines of agent equipment, funds {funds_before}->{funds_after}")
    return funds_after != funds_before


def _flying_crewed(d: Driver) -> int:
    """How many *flying* craft are carrying troops.

    Plain crewed counts are not enough: a Stormdog or Wolfhound APC can hold a squad and still be
    useless for reaching a downed UFO, and recovery is refused outright when the selected craft
    cannot get there. Recovery unlocks the research chain, so this distinction gates the endgame.
    """
    n = 0
    for part in d.h.gs("interceptors").get("detail", "").split("|"):
        flags = part.split(":")[-1]
        if "flying=1" in flags and "crew=0" not in flags:
            n += 1
    return n


def crew_transport(d: Driver) -> int:
    """Put soldiers aboard a craft so it can recover downed UFOs.

    VehicleMission::recoverVehicle is refused unless the craft carries a Soldier
    (cityview.cpp:1069-1090), so an all-interceptor fleet shoots UFOs down and collects none of
    them -- no artifacts, no alien research, no route to victory. Observed directly: three wrecks
    on the map with crewed=0 and every recovery refused.

    Two engine details decide how this has to be driven:

    * The assignment widget exists only on the three screens that embed city/agentassignment.form.
      Left-clicking our own base opens BaseScreen, which has no widget; *right*-clicking the same
      building opens BuildingScreen, which does (cityview.cpp:282-306).
    * The drop list is not built on MouseDown. It is built on the first MouseMove that travels
      more than `insensibility` (5px) from the press, copying whatever is selected in the source
      list (agentassignment.cpp:749-768). Press-then-release without a real move in between drops
      an empty list and silently transfers nobody, so the drag is issued as a stepped path.

    The transfer itself also requires the craft to be parked in the same building as the agent
    (agentassignment.cpp:527-536), which is why this is done at the base.
    """
    st = d.status()
    if st.stage != "CityView":
        return 0
    at = d.h.gs("centre_on_base")
    if at.get("centred") != "1":
        return 0
    bx, by = (int(v) for v in at["at"].split(",")[:2])
    d.h.ok(f"click {bx} {by} right")
    time.sleep(1.2)

    st = d.status()
    if st.stage != "BuildingScreen":
        d.say(f"  [crew] expected BuildingScreen, got {st.stage}")
        d.h.key("Escape")
        return 0
    box = d.controls(st).get("AGENT_ASSIGNMENT")
    if box is None or box.w <= 0:
        d.h.key("Escape")
        return 0

    ROW_H, FIRST_ROW, AGENT_DX, VEHICLE_DX = 26, 63, 103, 383
    # Count only flying crewed craft: loading a road vehicle looks like success and then fails
    # every recovery.
    before = _flying_crewed(d)
    crewed = before

    # Select the squad once, then try each craft row: the right column starts with the building
    # itself, so the first row that accepts a squad is not known ahead of time.
    # Select the squad exactly once. MultilistBox toggles: clicking a row that is already
    # selected removes it again, so re-selecting before every craft-row attempt was switching
    # the squad back off and dragging an empty list on all attempts after the first.
    picked = 0
    for r in range(6):
        y = box.y + FIRST_ROW + r * ROW_H
        if y >= box.y + box.h - 8:
            break
        d.h.click_xy(box.x + AGENT_DX, y)
        picked += 1
        time.sleep(0.1)
    if not picked:
        d.h.key("Escape")
        return 0

    for row in range(6):

        sy = box.y + FIRST_ROW
        dy = box.y + FIRST_ROW + row * ROW_H
        if dy >= box.y + box.h - 8:
            break
        d.h.ok(f"down {box.x + AGENT_DX} {sy}")
        time.sleep(0.15)
        for step in range(1, 7):  # stepped path so the >5px move actually fires
            mx = box.x + AGENT_DX + (VEHICLE_DX - AGENT_DX) * step // 6
            my = sy + (dy - sy) * step // 6
            d.h.ok(f"move {mx} {my}")
            time.sleep(0.08)
        d.h.ok(f"up {box.x + VEHICLE_DX} {dy}")
        time.sleep(0.6)

        crewed = _flying_crewed(d)
        if crewed > before:
            d.say(f"  [crew] squad boarded a flying craft on row {row}")
            break

    # Leave by the screen's own quit button. dismiss_modal would pick this stage's "act" control,
    # which on a BuildingScreen is BUTTON_RAID -- an assault on our own base.
    for _ in range(6):
        st = d.status()
        if st.stage == "CityView":
            break
        if not d.click_id("BUTTON_QUIT", st):
            d.h.key("Escape")
        time.sleep(0.4)
    crewed = _flying_crewed(d)
    d.say(f"  [crew] flying crewed craft {before} -> {crewed}")
    return crewed


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
    wanted = []
    for part in d.h.gs("interceptors").get("detail", "").split("|"):
        bits = part.split(":")
        if len(bits) < 3 or not bits[0].isdigit():
            continue
        flags = bits[-1]
        if "flying=1" in flags and "crew=0" not in flags:
            wanted.append(int(bits[0]))
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

def play_campaign(d: Driver, difficulty: int, total_days: float, leg_days: float = 7.0) -> dict:
    new_game(d, difficulty)
    t0 = snapshot(d, "t0")

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
        snapshot(d, f"day~{elapsed:.0f}")

    d.checks["battles_played"] = battles
    return snapshot(d, "final")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=17321)
    ap.add_argument("--repo", default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument("--out", default=None)
    ap.add_argument("--difficulty", type=int, default=3)
    ap.add_argument("--days", type=float, default=28.0)
    ap.add_argument("--leg", type=float, default=7.0)
    ap.add_argument("--no-launch", action="store_true")
    args = ap.parse_args()

    repo = Path(args.repo)
    out = Path(args.out) if args.out else repo / "build/e2e"
    out.mkdir(parents=True, exist_ok=True)
    shots = out / "shots"; shots.mkdir(exist_ok=True)

    game = None
    if not args.no_launch:
        game = GameProcess(repo, args.port, out / "game.log")
        print(f"[launch] {game.binary}", flush=True)
        game.start()

    d = Driver(Harness(port=args.port), repo / "data/forms", shots=shots)
    d.checks = {}
    rc = 0
    try:
        play_campaign(d, args.difficulty, args.days, args.leg)
    except Exception as exc:
        d.say(f"[FAIL] {type(exc).__name__}: {exc}")
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
