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
    "TransactionScreen": "transactionscreen",
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


class HarnessError(RuntimeError):
    pass


@dataclass
class Status:
    stage: str
    w: int
    h: int
    raw: str


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
        return Status(parts.get("stage", "?"), int(parts.get("w", 0)), int(parts.get("h", 0)), raw)

    def gs(self, query: str) -> dict[str, str]:
        raw = self.ok(f"gs {query}")
        return dict(p.split("=", 1) for p in raw.split() if "=" in p)

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
            # Belt and braces alongside the engine-side guard: a modal error dialog blocks the
            # main loop forever when there is no human to dismiss it.
            "--Logger.dialogLevel=0",
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
            self.proc.kill()
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
        st = st or self.status()
        ctrls = self.controls(st)
        c = ctrls.get(cid)
        if c is None or c.w <= 0 or c.h <= 0:
            return False
        x, y = c.centre
        self.h.click_xy(x, y)
        return True

    def shot(self, tag: str) -> None:
        if not self.shots:
            return
        self.shot_n += 1
        p = self.shots / f"{self.shot_n:03d}_{tag}.png"
        try:
            self.h.screenshot(str(p))
        except HarnessError:
            pass

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
        """Pick a craft and a squad in an alert/building screen's embedded assignment list.

        The list is a runtime-populated multilistbox mounted inside the AGENT_ASSIGNMENT graphic,
        agents in the left column and vehicles in the right, so rows are addressed geometrically
        off that rect.
        """
        box = self.controls(st).get("AGENT_ASSIGNMENT")
        if box is None or box.w <= 0:
            return 0
        picked = 0
        ROW_H, FIRST_ROW = 26, 63
        for col_dx, rows in ((300, 1), (50, 4)):   # one vehicle, up to four agents
            for r in range(rows):
                y = box.y + FIRST_ROW + r * ROW_H
                if y >= box.y + box.h:
                    break
                self.h.click_xy(box.x + col_dx, y)
                picked += 1
                time.sleep(0.15)
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
        kinds = ("act", "ack")
        if self.act_counts.get(st.stage, 0) >= ACT_ATTEMPT_LIMIT:
            kinds = ("ack",)
            if self.act_counts.get(st.stage) == ACT_ATTEMPT_LIMIT:
                self.act_counts[st.stage] += 1
                self.say(f"  [event] {st.stage}: action refused {ACT_ATTEMPT_LIMIT}x, "
                         f"acknowledging from now on")
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

    def wait_for(self, stage: str, timeout: float = 90.0) -> Status:
        deadline = time.time() + timeout
        while time.time() < deadline:
            st = self.status()
            if st.stage == stage:
                return st
            if not self.dismiss_modal(st):
                time.sleep(0.4)
        raise TimeoutError(f"stage {stage!r} not reached in {timeout}s (last={self.status().stage})")


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
    gs = d.h.gs("all")
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

    started = 0
    for list_id in ("LIST_LARGE_LABS", "LIST_SMALL_LABS"):
        for row in range(3):
            st = d.status()
            if st.stage != "ResearchScreen":
                break
            if not click_list_row(d, list_id, row, st):
                continue
            time.sleep(0.4)
            st = d.status()
            d.click_id("BUTTON_RESEARCH_NEWPROJECT", st); time.sleep(0.6)
            st = d.status()
            if st.stage != "ResearchSelect":
                continue
            d.shot(f"researchselect_{list_id}_{row}")
            if click_list_row(d, "LIST", 0, st, item_h=20):
                time.sleep(0.4)
            st = d.status()
            d.click_id("BUTTON_OK", st); time.sleep(0.8)
            started += 1

    # Unwind back to the city.
    for _ in range(6):
        st = d.status()
        if st.stage == "CityView":
            break
        if st.stage in ("ResearchSelect", "ResearchScreen", "BaseScreen"):
            d.click_id("BUTTON_OK", st); time.sleep(0.6)
        elif not d.dismiss_modal(st):
            d.h.key("Escape"); time.sleep(0.5)

    after = d.h.gs("research")
    d.say(f"[research] after:  {after}  (attempted {started} assignments)")
    return after.get("labs_busy", "0") != before.get("labs_busy", "0")


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
    ok = st.stage == "TransactionScreen"
    if ok:
        d.shot("transactionscreen")
        for cat in ("BUTTON_FLYING", "BUTTON_GROUND", "BUTTON_AGENTS"):
            d.click_id(cat, st); time.sleep(0.35)
        d.say(f"[economy] transaction screen reached; funds {d.h.gs('funds')}")
    else:
        d.say(f"[economy] expected TransactionScreen, got {st.stage}")
    for _ in range(6):
        st = d.status()
        if st.stage == "CityView":
            break
        if st.stage in ("TransactionScreen", "BaseScreen"):
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
    # Horizontal strip of craft icons; take the first slot.
    d.h.click_xy(lst.x + 16, lst.y + lst.h // 2)
    time.sleep(0.3)

    st = d.status()
    if not d.click_id("BUTTON_VEHICLE_ATTACK", st):
        d.say("  [intercept] no attack-order button")
        return 0
    time.sleep(0.3)

    # Bring a UFO into view and click it as the target of the armed attack order.
    ufos = d.h.screen_craft("ufos_screen")
    live = [(x, y) for (x, y, crashed) in ufos if not crashed]
    if not live:
        if d.h.gs("centre_on_ufo").get("centred") != "1":
            d.say("  [intercept] no UFO on the city map")
            d.h.key("Escape")
            return 0
        time.sleep(0.5)
        live = [(x, y) for (x, y, crashed) in d.h.screen_craft("ufos_screen") if not crashed]
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


def play_battle(d: Driver, budget_s: float = 300.0) -> str:
    """Drive a tactical mission from briefing to debriefing without human input.

    Real-time mode, max battle speed, and periodic move orders so units actually traverse the
    map (exercising pathfinding, hazards and unit AI). If the mission has not resolved inside
    the budget we use the engine's own debug retreat hotkey (F1 then Shift+K) to force an
    ending rather than letting the run hang.
    """
    t_start = time.time()
    d.say("[battle] entering tactical mission")
    entered = False
    forced = False
    while time.time() - t_start < budget_s:
        st = d.status()
        if st.stage == "BattleBriefing":
            d.click_id("BUTTON_REAL_TIME", st)
            time.sleep(1.0)
            continue
        if st.stage == "BattlePreStart":
            d.click_id("BUTTON_OK", st)
            time.sleep(1.0)
            continue
        if st.stage == "BattleDebriefing":
            d.shot("battle_debrief")
            d.click_id("BUTTON_OK", st)
            d.say(f"[battle] debriefing reached after {time.time()-t_start:.0f}s")
            return "resolved"
        if st.stage == "BattleView":
            if not entered:
                entered = True
                d.shot("battle_start")
                d.click_id("BUTTON_SPEED3", st)   # fastest real-time battle speed
                time.sleep(0.5)
            # Nudge the squad around the map so movement/pathing/hazards get exercised.
            #
            # NOTE: this used to send "Tab" here as a "cycle selected unit" hotkey, but in
            # BattleView Tab is actually bound to BUTTON_TOGGLE_STRATMAP (see
            # game/ui/tileview/battleview.cpp, SDLK_TAB case: it clicks the strategic-map
            # toggle, not a unit-cycle command). Sending it every loop iteration was flipping
            # the view between tactical and strategic roughly every 2s, so a large fraction of
            # the "click cx cy right" move orders below were being issued into whichever map
            # happened to be showing rather than reliably into the tactical view. There is no
            # dedicated unit-cycle hotkey in this build, so just drop the erroneous keypress
            # rather than mis-target a different one.
            cx, cy = st.w // 2, int(st.h * 0.42)
            d.h.send(f"click {cx} {cy} right")   # right click = move order
            time.sleep(2.0)
            if time.time() - t_start > budget_s * 0.7 and not forced:
                forced = True
                d.say("[battle] budget nearly spent; forcing retreat via debug hotkey")
                d.h.key("F1")                       # arm debug hotkeys
                d.h.ok("keydown Left Shift")
                d.h.key("k")                        # Shift+K retreats all units
                d.h.ok("keyup Left Shift")
            continue
        if d.dismiss_modal(st):
            continue
        # Back in the city: the mission finished and we already cleared the debriefing.
        if st.stage == "CityView":
            d.say("[battle] returned to city")
            return "returned"
        time.sleep(0.5)
    d.say("[battle] budget exhausted without resolution")
    return "timeout"


def play_campaign(d: Driver, difficulty: int, total_days: float, leg_days: float = 7.0) -> dict:
    new_game(d, difficulty)
    t0 = snapshot(d, "t0")

    d.checks["research_started"] = assign_research(d)
    d.checks["ufopaedia_opened"] = visit_ufopaedia(d)
    d.checks["economy_opened"] = visit_economy(d)

    battles = 0
    elapsed = 0.0
    while elapsed < total_days:
        leg = min(leg_days, total_days - elapsed)
        advance(d, leg)
        elapsed += leg
        st = d.status()
        if st.stage in ("BattleBriefing", "BattlePreStart", "BattleView", "BaseDefenseScreen"):
            outcome = play_battle(d)
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
