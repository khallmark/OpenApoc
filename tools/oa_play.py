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
    # A building we can raid.
    "BuildingScreen":         {"act": "BUTTON_RAID", "ack": "BUTTON_QUIT", "select": True},
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
        self.ok(f"key {name}")

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
        self.proc = subprocess.Popen(
            argv, cwd=str(self.repo), stdout=self.logf, stderr=subprocess.STDOUT,
            start_new_session=True,
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

    def respond_to_event(self, st: Status) -> bool:
        """Engage with an interrupting screen. Returns True if we acted on it."""
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
            d.h.key("5")
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
PRIORITY_RESEARCH = ["RESEARCH_ADVANCED_WORKSHOP"] + [
    f"RESEARCH_ALIEN_BUILDING_{i}" for i in range(10)
]


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

            wanted = pick_topic_row(d)
            for attempt in range(8):
                topic_row = wanted if attempt == 0 and wanted >= 0 else attempt
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
                    break

    # Unwind back to the city. Leaving the game parked on the research screen strands the
    # campaign loop, which only knows how to play from CityView.
    for _ in range(8):
        st = d.status()
        if st.stage == "CityView":
            break
        if st.stage in ("ResearchSelect", "ResearchScreen", "BaseScreen"):
            d.click_id("BUTTON_OK", st)
            time.sleep(0.5)
        elif not d.dismiss_modal(st):
            d.h.key("Escape")
            time.sleep(0.4)

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
    # Horizontal strip of craft icons. Send the whole wing, not one craft: a lone interceptor
    # loses to escorted UFOs, and a campaign that trades its fleet away without a single kill
    # has no artifacts to research and no score to keep its funding.
    ICON_W = 36
    for slot in range(4):
        x = lst.x + 16 + slot * ICON_W
        if x >= lst.x + lst.w:
            break
        d.h.click_xy(x, lst.y + lst.h // 2)
        time.sleep(0.15)
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
        if rounds % 2 == 0:
            d.h.gs("centre_on_friends")
            time.sleep(0.2)
        friends = d.h.screen_craft("friends_screen")
        if friends:
            d.h.send("keydown Left Ctrl")
            try:
                for fx, fy, _ in friends[:6]:
                    d.h.click_xy(fx, fy)
                    time.sleep(0.08)
            finally:
                d.h.send("keyup Left Ctrl")
            time.sleep(0.15)

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
            fx, fy, _ = foes[rounds % len(foes)]
            # Aim a short way off the hostile's own tile: that tile is occupied, and a left
            # click on an occupied tile selects rather than moves (battleview.cpp:3778-3790).
            fx += (40, -40, 0, 0)[rounds % 4]
            fy += (0, 0, 28, -28)[rounds % 4]
            d.h.click_xy(max(0, min(st.w - 1, fx)), max(0, min(st.h - 1, fy)))
        else:
            # Nothing anywhere in view; sweep the map, and change floor now and then since
            # hostiles hole up on other levels.
            tx = int(st.w * (0.3 + 0.2 * (rounds % 3)))
            ty = int(st.h * (0.3 + 0.15 * (rounds % 3)))
            d.h.click_xy(tx, ty)
            if rounds % 5 == 4:
                d.h.key("Page Up" if (rounds // 5) % 2 == 0 else "Page Down")
        rounds += 1
        time.sleep(2.5)

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
        if started_with and alive and foes_n > alive * 2 and alive <= max(1, started_with // 2):
            d.say(f"  [battle] retreating: {alive} left of {started_with} against {foes_n}")
            d.h.key("Escape")
            time.sleep(0.8)
            if d.click_id("BUTTON_EXIT_BATTLE", d.status()):
                time.sleep(1.5)
                for _ in range(5):
                    stt = d.status()
                    if stt.stage not in ("BattleView", "InGameOptions", "MessageBox"):
                        break
                    if not d.dismiss_modal(stt):
                        d.h.key("Return")
                    time.sleep(0.6)
                return "withdrew"
            d.h.key("Escape")

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
    the hire pool, LIST1 the current staff (recruitscreen.form:153,179).

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

    hired = 0
    for _ in range(want):
        # Always click row 0: a hired candidate leaves LIST2 immediately, so the next one takes
        # its place. Walking the index instead would skip every other candidate.
        try:
            if not d.h.send("control LIST2 item 0 click").startswith("OK"):
                break
        except (HarnessError, OSError):
            break
        hired += 1
        time.sleep(0.25)

    if hired:
        d.click_id("BUTTON_OK", d.status())
        time.sleep(1.0)
        for _ in range(4):
            st = d.status()
            if st.stage != "MessageBox":
                break
            # Confirm Orders is a Yes/No/Cancel box; press the button rather than guessing a key.
            if not d.click_id("BUTTON_YES", st):
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
    for entry in listing[2:]:
        parts = entry.split(":")
        if len(parts) < 2 or not parts[0].isdigit():
            continue
        label = parts[-1][5:] if parts[-1].startswith("text=") else ""
        if _norm(label) not in keys:
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
            target = max(low, have - qty)
            if target != have and d.h.send(f"control LIST item {idx} set {target}").startswith("OK"):
                ordered += 1
        except (HarnessError, OSError):
            break
    funds_before = int(d.h.gs("funds").get("balance", "0") or 0)
    close_buysell(d, commit=ordered > 0)
    funds_after = int(d.h.gs("funds").get("balance", "0") or 0)
    d.say(f"  [buy] ordered {ordered} of {len(keys)} wanted lines, funds {funds_before}->{funds_after}")
    return ordered


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
    before = int(d.h.gs("vehicles").get("crewed", "0") or 0)
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

        crewed = int(d.h.gs("vehicles").get("crewed", "0") or 0)
        if crewed > before:
            d.say(f"  [crew] squad boarded a craft on row {row}")
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
    d.say(f"  [crew] crewed craft {before} -> {crewed}")
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
    y = lst.y + lst.h // 2
    for x in range(lst.x + 6, lst.x + lst.w, 12):
        d.h.click_xy(x, y)
        time.sleep(0.12)
        if int(d.h.gs("selected").get("with_soldier", "0") or 0) > 0:
            d.say(f"  [select] crewed craft selected at x={x}")
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
    if d.h.gs("centre_on_crash").get("centred") != "1":
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
    time.sleep(0.8)
    sel = d.h.gs("selected")
    mission = sel.get("mission", "none")
    if "ecover" in mission or "oto" in mission:
        d.say(f"  [recover] craft dispatched to wreck at {cx},{cy} (mission={mission})")
        return 1
    d.say(f"  [recover] refused at {cx},{cy} ({sel})")
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
