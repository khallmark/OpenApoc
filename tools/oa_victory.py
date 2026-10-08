#!/usr/bin/env python3
"""Play OpenApoc from a Novice start toward victory, unattended and without cheating.

Victory in this engine is precise: raid BUILDING_DIMENSION_GATE_GENERATOR in the alien dimension
and win the tactical mission. Battle::exitBattle sets GameWon only for the one building
carrying `victory`, and it sits behind RESEARCH_ALIEN_BUILDING_9 -- the end of a chain where each
alien building must be raided and won to unlock research for the next. That is months of game
time and many tactical missions, so this runner is built to survive rather than to be quick:

  * it checkpoints through the harness SAVE command and resumes via --Game.Load,
  * it never aborts a battle,
  * it uses no CheatOptions, no debug hotkeys and no direct GameState mutation. Every action is
    a click or a keypress a player could make.

The play loop is the chain that took the longest to get working, in the order it has to happen:
crew a craft, intercept UFOs, shoot them down, recover the wrecks (the only source of alien
artifacts, and therefore the root of the whole research tree), and fight the battles that result.
"""

from __future__ import annotations

import argparse
import json
import os
import signal
import time
import traceback
from pathlib import Path

from oa_play import (
    add_runner_options,
    configure_runner,
    set_speed,
    free_port,
    Driver,
    GameProcess,
    Harness,
    HarnessError,
    assign_research,
    build_second_base,
    sell_ground_fleet,
    sell_surplus_loot,
    buy_interceptor,
    equip_craft,
    verify_battle_capabilities,
    build_facility,
    return_to_city,
    goto_portal,
    raid_alien_building,
    raid_infiltrated_building,
    manufacture,
    gate_craft_project,
    craft_flags,
    buy_vehicles,
    arm_squad,
    unarmed_at_base,
    hire_engineers,
    hire_scientists,
    hire_soldiers,
    _flying_crewed,
    crew_transport,
    clear_attack_orders,
    intercept_ufos,
    new_game,
    recover_crash_sites,
    resolve_strategy,
    win_battle,
)
from oa_strategy import Strategy, add_strategy_option

BATTLE_STAGES = ("BattleBriefing", "BattlePreStart", "BattleView", "BaseDefenseScreen")
MAX_RESTARTS = 40
# One recovery order per wreck is enough; re-issuing every second only fights the craft's own
# pathing and floods the log.
RECOVER_COOLDOWN_S = 90.0
# Re-issuing an attack order every pass only re-arms a craft that is already flying at the
# target, and it starves the game of the frames it needs to actually resolve the fight.
INTERCEPT_COOLDOWN_S = 25.0
CREW_COOLDOWN_S = 60.0
# Topics finish and labs fall idle; an idle lab is research that is not happening. Re-checking
# costs a few seconds of game time and is the difference between a campaign that advances up the
# alien-building chain and one that stops after its first two topics.
RESEARCH_COOLDOWN_S = 90
BASE_COOLDOWN_S = 120.0
# How often to go and clear a building aliens were reported entering. Promptly: a crew grows and
# spreads every hour, and once an organisation's infiltrationValue reaches 200 it is taken over
# outright (organisation.cpp:84-92). The window to act is right after the drop, before the company
# is theirs.
INFIL_COOLDOWN_S = 45.0
# Below this, with funding cut and nobody armed, a campaign cannot recover: no income, and not
# enough left to buy a single weapon.
BANKRUPT_FLOOR = 5000
# How many armed fliers to keep in reserve. The guide puts the mainstay at "10 or so,
# strategically placed around the city", expecting to lose two or three a battle -- they are
# cheap enough that attrition does not matter. That is a RESERVE, not a strike group: every
# projectile that hits a building costs relation with its owner, so intercepts still go out two
# craft at a time. Large fleet, small sortie.
AIR_PATROL = 8
# Soldiers are lost permanently, and the guide is emphatic that under-hiring is the more common
# mistake: "It's better to hire too many troops than to have too few ... It's best to assume
# you'll have 10 guys healing and another few deceased." Ten fit soldiers was a squad with no
# depth -- one bad mission and base defences were being fought by scientists.
MIN_SOLDIERS = 18
# Fewer than this and an incident is not worth answering: the squad dies and the score hit from
# losing it dwarfs the hit from declining.
MIN_SQUAD = 6
# Below this much total lab skill the research chain is crawling and worth spending money on.
MIN_LAB_SKILL = 1100
CHECKPOINT_EVERY_S = 300.0
# Government relation below which a UFO over the city is left alone rather than shot at: every shot
# that misses and hits a building costs relation with its owner, and below -50 funding is cut for
# good. This was a bare 25 inside city_turn; it is a constant so the strategy genome can tune it.
INTERCEPT_MIN_RELATION = 25
# Armed agents needed before an infiltrated building is worth a sweep, and before the clock is
# slowed to keep pace with the infiltration (both were a bare 3), and the city clock speed used
# while buildings await a sweep (a bare 3 -- 30 game-minutes per five wall-minutes).
INFIL_MIN_ARMED = 3
INFIL_SPEED = 3
# How often the end-of-run metrics cache is refreshed. The game is gone on the crash and
# restart-give-up paths, so the numbers the learner scores have to be captured while it is alive.
METRICS_EVERY_S = 30.0
FINAL_SCHEMA = 1


class Victory:
    def __init__(self, repo: Path, out: Path, port: int, difficulty: int = 1,
                 battle_policy: dict | None = None, seed: int = 0,
                 strategy: Strategy | None = None, single_campaign: bool = False):
        self.repo, self.out, self.port = Path(repo), Path(out), port
        self.difficulty = difficulty
        self.battle_policy = dict(battle_policy or {})
        self.seed = seed
        # The campaign genome (tools/oa_strategy.py). The default is exactly the constants this
        # file had before it existed, so a run that names none behaves as it always did.
        self.strategy = strategy or Strategy()
        # One campaign per run: a defeat ends the run instead of starting the next campaign. The
        # cross-run learner scores one genome on one campaign; letting a lost run roll into a fresh
        # one would blend two campaigns' outcomes into a single reward.
        self.single_campaign = single_campaign
        self.out.mkdir(parents=True, exist_ok=True)
        (self.out / "shots").mkdir(exist_ok=True)
        self.checkpoint = self.out / "victory.save"
        self.progress_path = self.out / "progress.json"
        self.progress = self._load()
        self.game: GameProcess | None = None
        self.d: Driver | None = None
        self.restarts = 0
        self.last_recover = 0.0
        self.last_intercept = 0.0
        self.last_crew = 0.0
        self.last_research = 0.0
        self.last_hire = 0.0
        self.stuck_since = 0.0
        self.last_build = 0.0
        self.last_equip = 0.0
        self.last_craft = 0.0
        self.last_score_warn = 0.0
        self.last_endgame = 0.0
        self.last_checkpoint = 0.0
        self.last_base_try = 0.0
        self.last_vequip = 0.0
        self.last_loot_sale = 0.0
        self.last_infil_raid = 0.0
        self.turbo_held = 0
        self.last_vequip_outcome = ""
        self.last_defer_log = 0.0
        self.caps_checked = False
        self.last_solvency_check = 0.0
        self.last_deferred_why = ""
        self.second_base = False
        self.best_crashed = 0
        # End-of-run summary state (see write_final).
        self.final_path = self.out / "final.json"
        self.exit_reason = ""
        self.finished: list[dict] = []        # progress of campaigns that ended before this one
        self.metrics: dict = {}
        self.last_sample = 0.0
        self.started_at = time.time()

    # -- durability -------------------------------------------------------
    def _load(self) -> dict:
        if self.progress_path.exists():
            try:
                prev = json.loads(self.progress_path.read_text())
            except ValueError:
                prev = None
            if prev:
                # A campaign that reached victory or defeat is a finished result, not something
                # to resume into. Keep it -- runs that end in failure are still real outcomes and
                # are worth preserving rather than overwriting -- and begin a new one beside it.
                if prev.get("ended"):
                    stamp = time.strftime("%Y%m%d-%H%M%S")
                    archive = self.out / f"campaign-{prev['ended']}-{stamp}.json"
                    archive.write_text(json.dumps(prev, indent=1))
                    if self.checkpoint.exists():
                        self.checkpoint.rename(self.out / f"campaign-{prev['ended']}-{stamp}.save")
                    print(f"[campaign] archived finished run ({prev['ended']}) to {archive.name}",
                          flush=True)
                else:
                    return prev
        return {"battles": 0, "wins": 0, "ufos_down": 0, "recoveries": 0, "restarts": 0}

    def flush(self) -> None:
        self.progress_path.write_text(json.dumps(self.progress, indent=1))

    def record(self, key: str, value=None) -> None:
        """Note an endgame milestone in progress.json. Was called four times and never defined, so
        each milestone raised AttributeError and abandoned the rest of that city turn."""
        milestones = self.progress.setdefault("milestones", [])
        if key not in milestones:
            milestones.append(key)
        if value is not None:
            self.progress[key] = value
        self.flush()

    def say(self, msg: str) -> None:
        line = f"[{time.strftime('%H:%M:%S')}] {msg}"
        print(line, flush=True)
        with open(self.out / "victory.log", "a") as f:
            f.write(line + "\n")

    def save(self, why: str) -> None:
        # Only ever checkpoint from the city. "after mission N" fired while the debriefing was
        # still up, so the save captured a battle mid-teardown -- resuming it loaded a degenerate
        # battle (ten units, all flagged retreated, zero hostiles) and the game died on the spot,
        # twice in a row, each death overwriting nothing but costing a restart. A stale but
        # loadable checkpoint beats a fresh poisoned one, so skip rather than overwrite, and let
        # the periodic save take it as soon as the city is back.
        try:
            stage = self.d.status().stage
        except (HarnessError, OSError) as exc:
            self.say(f"checkpoint FAILED ({why}): {exc}")
            return
        if stage != "CityView":
            # Retry soon, but not on every pass of the loop: zeroing the timer outright made the
            # periodic save fire again immediately and fill the log with one deferral line per
            # iteration while the driver sat on a recruit screen.
            if self.last_deferred_why != why or time.time() - self.last_defer_log > 30.0:
                self.say(f"checkpoint deferred ({why}): stage is {stage}, not CityView")
                self.last_defer_log = time.time()
                self.last_deferred_why = why
            self.last_checkpoint = time.time() - (CHECKPOINT_EVERY_S - 30.0)
            return
        try:
            self.d.h.ok(f"save {self.checkpoint}")
            self.last_checkpoint = time.time()
            self.say(f"checkpoint ({why})")
        except (HarnessError, OSError) as exc:
            self.say(f"checkpoint FAILED ({why}): {exc}")

    # -- lifecycle --------------------------------------------------------
    def start(self) -> None:
        resume = self.checkpoint.exists()
        extra = [f"--Game.Load={self.checkpoint}"] if resume else []
        self.game = GameProcess(self.repo, self.port, self.out / "game.log", extra=extra,
                                seed=self.seed)
        self.game.start(wait_s=240)
        self.d = Driver(Harness(port=self.port), self.repo / "data/forms",
                        shots=self.out / "shots", verbose=True, battle_policy=self.battle_policy,
                        strategy=self.strategy)
        self.d.checks = {}
        self.d.say = lambda m: self.say(m)
        # Log the effective genome on every (re)start so a log alone proves which strategy a run
        # was playing, and that it reached the Driver the helpers read it from.
        self.say(f"[strategy] {self.strategy.key()} | genes {self.strategy.to_json()} | "
                 f"battle policy {self.battle_policy or 'engine default'}")
        if resume:
            self.say("resumed from checkpoint")
            # A checkpoint is written wherever the campaign happened to be, including partway
            # through a tactical mission -- so a resume can legitimately land in BattleView.
            # Waiting only for CityView stalled the whole runner for the full 240s timeout
            # against a save that had loaded perfectly well into a battle the main loop is
            # perfectly capable of fighting. Accept either playable stage.
            self.d.wait_for(("CityView", "BattleView"), 240)
            # STATUS reporting CityView only means the stage transitioned, not that resuming a
            # save has finished settling: three restarts in a row died with "connection reset by
            # peer" the moment the very first post-resume action fired, which is the same shape
            # as the initState segfault fixed earlier this session -- work still running after
            # the stage that depends on it having fully landed. Confirm the GS pipeline itself is
            # actually answering before acting on anything.
            settled = False
            for _ in range(20):
                try:
                    self.d.h.gs("time")
                    settled = True
                    break
                except (HarnessError, OSError):
                    time.sleep(1.0)
            if not settled:
                self.say("resumed but GS queries never came up; treating as unhealthy")
                raise RuntimeError("resume did not settle")
        else:
            self.say(f"fresh campaign, difficulty {self.difficulty}")
            new_game(self.d, self.difficulty)
            assign_research(self.d)
            # Stock the armoury immediately. Unarmed personnel are not neutral -- they are
            # casualties: the campaign was lost when a base defence pitted 21 mostly-unarmed
            # agents against 11 aliens and every one of them died, taking the base with it.
            # "Like every guide states - sell off ground vehicles." They cannot reach a crash
            # site or cross broken road, so they are upkeep with no capability, and the proceeds
            # fund the fleet that does the work. Do it before anything else is bought.
            sold = sell_ground_fleet(self.d)
            self.say(f"sold {sold} ground vehicle line(s) to fund the air fleet")
            arm_squad(self.d, agents=self.armoury_size())
            crew_transport(self.d)
            self.save("campaign start")

    def alive(self) -> bool:
        if not self.game or self.game.proc.poll() is not None:
            return False
        try:
            self.d.h.send("status")
            return True
        except OSError:
            return False

    def restart(self) -> bool:
        # A single failed attempt used to end the whole unattended run: restart() returned False,
        # run() took that as final and exited -- even when the checkpoint itself loads fine in
        # isolation, which was confirmed directly after this happened. The actual cause was
        # transient (port contention, a slow-to-die previous process), not a broken save. A run
        # that ends because of that is not a real result and should not be the last word --
        # retrying a few times before giving up produces a genuine conclusion far more often than
        # bailing on the first hiccup.
        if self.restarts >= MAX_RESTARTS:
            self.say(f"too many restarts ({self.restarts}); stopping")
            return False
        if not self.checkpoint.exists():
            self.say("no checkpoint to resume from")
            return False
        # A checkpoint written by an already-unstable process can load into a game that dies on
        # the first interaction -- verified directly: hire_scientists runs cleanly on a fresh
        # game but killed the resumed one three times running. Past a few consecutive failures,
        # the save itself is the problem, so retire it and start a genuinely new campaign rather
        # than restart-looping into the same broken state for ever.
        if self.restarts >= 4 and self.checkpoint.exists():
            stamp = time.strftime("%Y%m%d-%H%M%S")
            self.checkpoint.rename(self.out / f"unstable-{stamp}.save")
            self.say("checkpoint keeps producing a game that dies immediately; retiring it "
                     "and starting a fresh campaign")
            self.progress = {"battles": 0, "wins": 0, "ufos_down": 0, "recoveries": 0,
                             "restarts": self.restarts}
            self.flush()
            try:
                self.start()
                return True
            except Exception as exc:
                self.say(f"fresh start failed: {exc}")
                return False

        for attempt in range(1, 4):
            self.restarts += 1
            self.progress["restarts"] = self.progress.get("restarts", 0) + 1
            self.flush()
            how = self.game.exit_status() if self.game else "unknown"
            self.say(f"game died ({how or 'still running'}) - restarting from checkpoint "
                     f"(#{self.restarts}, attempt {attempt}/3)")
            try:
                if self.game:
                    self.game.stop()
            except Exception:
                pass
            time.sleep(3.0 * attempt)
            try:
                self.start()
                return True
            except Exception as exc:
                self.say(f"restart attempt {attempt} failed: {exc}")
        return False

    # -- play -------------------------------------------------------------
    def fight(self, stage: str) -> None:
        self.progress["battles"] += 1
        n = self.progress["battles"]
        self.say(f"=== tactical mission #{n} ({stage}) ===")
        try:
            self.d.h.ok(f"screenshot {self.out}/shots/battle{n:03d}.png")
        except (HarnessError, OSError):
            pass
        try:
            # Prove the harness can actually do everything, once, against a real battle. It
            # perturbs the one mission it runs in -- it changes stance, throws, probes -- which is
            # the cost of testing a UI for real rather than asserting it works.
            if os.environ.get("OA_VERIFY_CAPS") == "1" and not self.caps_checked:
                self.caps_checked = True
                try:
                    self.progress["capabilities"] = verify_battle_capabilities(self.d)
                    self.flush()
                except (HarnessError, OSError) as exc:
                    self.say(f"capability check failed: {exc}")
            outcome = win_battle(self.d, budget_s=900)
        except (HarnessError, OSError) as exc:
            outcome = f"lost connection: {exc}"
        if outcome == "resolved":
            self.progress["wins"] += 1
        self.say(f"=== mission #{n} outcome: {outcome} (wins {self.progress['wins']}) ===")
        self.flush()
        if self.alive():
            self.save(f"after mission {n}")

    def armoury_size(self) -> int:
        """How many of each loadout item to stock: one per soldier, plus spares.

        A fixed twelve was written when a roster was ten and never revisited. Once hiring started
        working the roster reached fifteen and stores held ten weapons, so nine of fifteen
        soldiers went into the field with nothing -- the armoury has to follow the payroll.
        """
        try:
            fit = int(self.d.h.gs("agents").get("soldiers", "0") or 0)
        except (HarnessError, OSError):
            fit = 12
        return max(12, fit + 6)

    def dimension_turn(self) -> bool:
        """Advance the legitimate endgame, or keep an alien-city return out of base routines."""
        alien = self.d.h.gs("alien_buildings")
        fleet = craft_flags(self.d)
        gate = [f for _, f in fleet if f.get("shifter") == "1"]
        squad = [f for f in gate if int(f.get("crew", "0")) > 0]
        city = alien.get("current_city", "CITYMAP_HUMAN")
        if self.progress.get("crossing_to") == "CITYMAP_ALIEN":
            if city == "CITYMAP_ALIEN":
                self.progress.pop("crossing_to", None)
                self.record("crossed_to_alien_dimension")
                self.say("=== CROSSING INTO THE ALIEN DIMENSION: CITYMAP_ALIEN confirmed ===")
                self.last_endgame = 0.0
            elif not squad:
                self.progress.pop("crossing_to", None)
                self.flush()
            else:
                if time.time() - self.last_endgame > 60.0:
                    self.last_endgame = time.time()
                    goto_portal(self.d)
                    if getattr(self.d, "loadout_blocked", False) is True:
                        self.progress.pop("crossing_to", None)
                        self.flush()
                        return False  # Keep procurement/research running while still at home.
                set_speed(self.d, 4)
                set_speed(self.d, 5)
                return True
        if self.progress.get("returning_home"):
            if city == "CITYMAP_HUMAN" and any(f.get("home") == "1" for f in squad):
                self.progress.pop("returning_home", None)
                self.record("squad_returned_home")
                self.say("=== ALIEN RAID SQUAD HOME: ready for research and the next raid ===")
            elif not squad:
                self.progress.pop("returning_home", None)
                self.flush()
                self.say("  [portal] returning squad lost; rebuilding the assault capability")
            else:
                if (city == "CITYMAP_ALIEN" and time.time() - self.last_endgame > 60.0
                        and any(f.get("city") == city and f.get("transit") == "0"
                                and f.get("portal") == "0" for f in squad)):
                    self.last_endgame = time.time()
                    goto_portal(self.d, "CITYMAP_HUMAN")
                set_speed(self.d, 4)
                set_speed(self.d, 5)
                return True
        if city == "CITYMAP_ALIEN":
            if not any(f.get("city") == city and f.get("transit") == "0" for f in squad):
                # The view can lag the craft's automatic return. Let the engine finish it.
                if (time.time() - self.last_endgame > 60.0
                        and any(f.get("city") == city and f.get("transit") == "0"
                                and f.get("portal") == "0" for f in gate)):
                    self.last_endgame = time.time()
                    self.say("  [portal] empty assault craft returning home to collect soldiers")
                    goto_portal(self.d, "CITYMAP_HUMAN")
                set_speed(self.d, 4)
                set_speed(self.d, 5)
                return True
            if time.time() - self.last_endgame > 60.0:
                self.last_endgame = time.time()
                if int(alien.get("raidable", "0") or 0) > 0:
                    outcome = raid_alien_building(self.d)
                    self.say(f"=== ALIEN BUILDING RAID: {outcome} ===")
                    if outcome in ("resolved", "lost", "returned"):
                        self.progress["returning_home"] = True
                        if outcome == "resolved":
                            self.progress["alien_buildings_taken"] = (
                                self.progress.get("alien_buildings_taken", 0) + 1)
                            self.record("alien_building_raided")
                            self.say("=== ALIEN RAID WON: engine returning squad through the gate ===")
                        self.flush()
                    return True
                self.progress["returning_home"] = True
                self.flush()
                self.say("  [portal] no surviving building open; returning home for research")
                goto_portal(self.d, "CITYMAP_HUMAN")
            set_speed(self.d, 4)
            set_speed(self.d, 5)
            return True
        # These actions are retryable. Recorded milestones never suppress a new attempt.
        if time.time() - self.last_endgame <= 60.0:
            return False
        self.last_endgame = time.time()
        if not gate:
            want = gate_craft_project(self.d)
            if want and manufacture(self.d, want, 1):
                self.record("gate_craft_manufacture_started", want)
                self.say(f"=== GATE CRAFT MANUFACTURE STARTED: {want} x1 ===")
            else:
                # assign_research may have started this same explicit project on a prior turn.
                labs = self.d.h.gs("research").get("labs_detail", "")
                for project in ("MANUFACTURE_BIO-TRANSPORT", "MANUFACTURE_EXPLORER",
                                "MANUFACTURE_RETALIATOR", "MANUFACTURE_ANNIHILATOR"):
                    if (project in labs and
                            self.progress.get("gate_craft_manufacture_started") != project):
                        self.record("gate_craft_manufacture_started", project)
                        self.say(f"=== GATE CRAFT MANUFACTURE IN PROGRESS: {project} ===")
                        break
            return False
        if "gate_craft_ready" not in self.progress.get("milestones", []):
            self.record("gate_craft_ready")
            self.say("=== GATE CRAFT READY: owned craft reports shifter=1 ===")
        ready = [f for f in squad if int(f.get("crew", "0")) >= self.strategy["cross_min_crew"]]
        if not ready and time.time() - self.last_crew > CREW_COOLDOWN_S:
            self.last_crew = time.time()
            crew_transport(self.d)
            squad = [f for _, f in craft_flags(self.d)
                     if f.get("shifter") == "1" and int(f.get("crew", "0")) > 0]
            ready = [f for f in squad
                     if int(f.get("crew", "0")) >= self.strategy["cross_min_crew"]]
        if ready and int(alien.get("raidable", "0") or 0) > 0:
            self.progress["crossing_to"] = "CITYMAP_ALIEN"
            self.flush()
            if goto_portal(self.d):
                self.progress.pop("crossing_to", None)
                self.last_endgame = 0.0
                self.say("=== CROSSING INTO THE ALIEN DIMENSION: CITYMAP_ALIEN confirmed ===")
                self.record("crossed_to_alien_dimension")
            elif getattr(self.d, "loadout_blocked", False) is True:
                self.progress.pop("crossing_to", None)
                self.flush()
                return False
            return True  # A pending crossing must retain its crew and its portal order.
        return False

    def city_turn(self) -> None:
        # Watch the actual game-over condition. fundingTerminated latches for good the first week
        # lifetime score drops below -2400, so this is a countdown that has to be tracked, not a
        # number to notice afterwards.
        money = self.d.h.gs("funds")
        margin = int(money.get("margin_to_cutoff", "99999") or 99999)
        if money.get("funding_terminated") == "1":
            if not self.progress.get("funding_lost"):
                self.progress["funding_lost"] = True
                self.flush()
                self.say("FUNDING TERMINATED PERMANENTLY - income is 0 for the rest of this "
                         "campaign; it cannot be recovered")
        elif time.time() - self.last_score_warn > 120.0:
            rel = int(self.d.h.gs("infiltrated").get("gov_relation", "0") or 0)
            if rel < 20:
                # The other latch: government relation going Hostile terminates funding just as
                # permanently as the score cutoff, and speculative building investigations are
                # what drives it down.
                self.say(f"government relation down to {rel} - funding is at risk from relations, "
                         f"not score")
                self.last_score_warn = time.time()
        if (not money.get("funding_terminated") == "1" and margin < 1200
                and time.time() - self.last_score_warn > 120.0):
            self.last_score_warn = time.time()
            self.say(f"score {money.get('score_total')} - {margin} from permanent funding cutoff "
                     f"(incidents={money.get('incidents')} damage={money.get('city_damage')} "
                     f"ufos_downed={money.get('ufos_downed')})")

        # Dimension work runs before any base/economy action. CityView can be either city;
        # return_to_city only pops UI screens and never transports the squad between dimensions.
        if self.dimension_turn():
            return

        v = self.d.h.gs("vehicles")
        crashed = int(v.get("ufos_crashed", "0") or 0)
        in_city = int(v.get("ufos_in_city", "0") or 0)
        # Flying craft carrying a real squad, not vehicles-with-a-soldier: gs vehicles counts a road
        # bike with one soldier aboard, which kept the re-crew gate shut while every recovery
        # was refused.
        crewed = _flying_crewed(self.d)

        if crashed > self.best_crashed:
            self.progress["ufos_down"] += crashed - self.best_crashed
            self.best_crashed = crashed
            self.say(f"UFO down (total wrecks {crashed})")

        if crewed == 0 and time.time() - self.last_crew > CREW_COOLDOWN_S:
            # Without a Soldier aboard a craft, recoverVehicle is refused outright and the whole
            # artifact chain stalls, so this is worth re-doing whenever it lapses. It only works
            # while the craft is parked in the same building as the agents, so after a mission it
            # will fail until the craft gets home -- which is why this must not short-circuit the
            # rest of the turn. Returning here early stopped the clock entirely and the runner
            # sat retrying a drop that could never succeed.
            self.last_crew = time.time()
            if crew_transport(self.d) == 0:
                self.say("could not crew a craft yet (craft probably still out)")

        if crewed > 0 and crashed > 0 and time.time() - self.last_recover > RECOVER_COOLDOWN_S:
            self.last_recover = time.time()
            if recover_crash_sites(self.d):
                self.progress["recoveries"] += 1
                self.flush()

        # Clear infiltrated buildings. This is the mechanism that has been ending these
        # campaigns: alien crews raise their owner's infiltrationValue every hour and spread to
        # neighbours if left alone, most buildings are the government's, and government relation
        # below -50 cuts funding outright. Two runs died at gov_relation -78 and -80 with 19 and
        # 39 buildings infiltrated while the driver read that number and did nothing. A ground
        # raid costs no relation with the owner, so the only real cost is the squad's time.
        # A building sweep does not need a full squad -- most reported crews are one to five
        # aliens, and requiring six armed agents meant the raid never ran at all once the squad
        # had taken losses: observed at 4 armed of 11 soldiers with eight buildings infiltrated
        # and the government relation falling. Three is enough to clear a small crew, and letting
        # the infiltration stand is what actually ends campaigns.
        # No cooldown while buildings are known to be waiting. The cooldown exists to stop the
        # driver trekking to the city screen for nothing; when there is a confirmed address it is
        # just delay, and delay is what loses the infiltration race.
        armed_now = int(self.d.h.gs("agents").get("armed", "0") or 0)
        due = INFIL_COOLDOWN_S if not self.d.alerted_buildings else 0.0
        if armed_now >= self.strategy["infil_min_armed"] and \
                time.time() - self.last_infil_raid >= due:
            self.last_infil_raid = time.time()
            outcome = raid_infiltrated_building(self.d)
            if outcome != "nothing-reported":
                self.say(f"infiltration raid: {outcome}")
                self.progress["infil_raids"] = self.progress.get("infil_raids", 0) + 1
                self.flush()

        # A second base is the cheapest insurance in the game. GameLost is raised on exactly
        # one condition -- player_bases.empty() (base.cpp:150-159) -- so with two bases, losing
        # one to a base defence that goes badly no longer ends the campaign. The last three runs
        # all ended that way. Funding termination is a much milder thing than it looks: it only
        # zeroes income (gamestate.cpp:1668-1680), and a campaign with money banked can still
        # research and manufacture its way to victory afterwards. Buy it as soon as it is
        # affordable, ahead of any other spending.
        if not self.second_base and time.time() - self.last_base_try > BASE_COOLDOWN_S:
            self.last_base_try = time.time()
            site = self.d.h.gs("centre_on_basesite")
            if site.get("centred") == "1" and int(site.get("bases", "1") or 1) >= 2:
                self.second_base = True
                self.say("second base already established")
            elif site.get("affordable") == "1":
                outcome = build_second_base(self.d)
                self.say(f"second base: {outcome}")
                if outcome == "bought":
                    self.second_base = True
                    self.progress["bases"] = 2
                    self.flush()
                    self.save("second base")

        # Keep every lab busy and staffed. assign_research staffs first, then fills idle labs.
        if time.time() - self.last_research > RESEARCH_COOLDOWN_S:
            self.last_research = time.time()
            r = self.d.h.gs("research")
            idle = int(r.get("assignable", "0") or 0) - int(r.get("assignable_busy", "0") or 0)
            done = int(r.get("complete", "0") or 0)
            skill = 0
            for part in r.get("labs_detail", "").split("|"):
                for kv in part.split(":"):
                    if kv.startswith("skill="):
                        skill += int(kv.split("=")[1] or 0)
            min_lab_skill = self.strategy["min_lab_skill"]
            empty_lab = any(":built:" in lab and ":staff=0:" in lab
                            for lab in r.get("labs_detail", "").split("|"))
            if (skill < min_lab_skill or empty_lab) and time.time() - self.last_hire > 180.0:
                # Scientists get dispatched to incidents along with everyone else and die there,
                # which silently throttles the whole research chain.
                self.last_hire = time.time()
                self.say(f"lab skill {skill} < min_lab_skill {min_lab_skill}, empty={empty_lab} "
                         "- recruiting scientists and engineers")
                hire_scientists(self.d, want=4)
                # And engineers, which nothing was hiring at all. The workshop is what pays for
                # the campaign once there is anything worth manufacturing, and the guide says to
                # make space for them rather than wait.
                hire_engineers(self.d, want=4)
            # Only make the trip when there is something to assign. A research pass takes tens of
            # seconds -- walking both lab lists, opening ResearchSelect per lab -- and it was
            # firing every 45 seconds regardless, so the driver lived on the research screen with
            # the clock barely moving: an hour of wall time produced 0 battles, 0 UFOs downed and
            # 0 recoveries. startable counts topics that could actually be picked right now, so
            # when it is zero the whole trip is wasted and the game is better off left running.
            startable = int(r.get("startable", "0") or 0)
            if (idle > 0 and startable > 0) or skill < min_lab_skill or empty_lab:
                self.say(f"{idle} lab(s) idle, {startable} startable, skill {skill}, "
                         f"{done} complete - reassigning")
                assign_research(self.d)
            elif idle > 0:
                self.say(f"{idle} lab(s) idle but nothing startable - waiting for unlocks")
            self.progress["research_complete"] = done
            self.flush()

        # Inspect live facilities, including construction, rather than latching a one-shot
        # flag. A failed placement, a destroyed base, or a resumed save must remain retryable.
        if time.time() - self.last_build > 90.0:
            self.last_build = time.time()
            for facility, milestone, label in (
                    ("FACILITYTYPE_ADVANCED_QUANTUM_PHYSICS_LAB", "advanced_physics_lab_built",
                     "advanced quantum physics lab"),
                    ("FACILITYTYPE_ADVANCED_WORKSHOP", "advanced_workshop_built", "advanced workshop")):
                fac = self.d.h.gs("facilities")
                if facility in fac.get("base", ""):
                    if (f"{facility}:0" in fac.get("base", "") and
                            milestone + "_ready" not in self.progress.get("milestones", [])):
                        self.record(milestone + "_ready")
                        self.say(f"=== {label} ready ===")
                elif facility in fac.get("offer", "") and build_facility(self.d, facility):
                    self.record(milestone)
                    self.say(f"=== {label} under construction ===")

        # Arm whoever is unarmed. This is not housekeeping: an unarmed agent in a base defence
        # is a free kill, and losing the base loses the campaign outright.
        ag = self.d.h.gs("agents")
        armed = int(ag.get("armed", "0") or 0)
        soldiers = int(ag.get("soldiers", "0") or 0)
        # Only soldiers standing in a base can be equipped from its stores. `armed < soldiers`
        # alone also counted the ones away on a mission or still in transit, so the pass retried
        # every forty seconds against people it could not reach.
        unarmed_home = unarmed_at_base(ag)
        # Refit veterans too: research unlocks and armor upgrades matter even when everyone
        # already carries a gun. The role planner orders only deficits, including inbound stock.
        if time.time() - self.last_equip > 240.0 or (unarmed_home and time.time() - self.last_equip > 40.0):
            self.last_equip = time.time()
            arm_squad(self.d, agents=self.armoury_size())

        # Replace losses, and arm them if the armoury can.
        fit = int(ag.get("soldiers_fit", "0") or 0)
        min_soldiers = self.strategy["min_soldiers"]
        if fit < min_soldiers and time.time() - self.last_hire > 180.0:
            # Only recruit people we can actually arm. An unarmed agent is not a neutral
            # addition: in a base defence every person present is dropped into the fight whether
            # they can shoot or not, and the campaign that was lost went down with 21 mostly
            # unarmed bodies against 11 aliens. The run that is healthy right now is healthy
            # precisely because it never diluted its ten armed veterans -- armed 10 of 10.
            stock = int(self.d.h.gs("stores").get("weapons", "0") or 0)
            if stock <= 0:
                self.last_hire = time.time()
                self.say(f"{fit} fit soldiers but no weapons in stores - buying before hiring")
                arm_squad(self.d, agents=self.armoury_size())
            else:
                self.last_hire = time.time()
                want = min(min_soldiers - fit + 2, stock)
                self.say(f"{fit} fit soldiers < min_soldiers {min_soldiers}, {stock} weapons in "
                         f"stock - recruiting {want}")
                hire_soldiers(self.d, want=want)

        # Follow the action in the city too, so a watching human sees what the driver is doing
        # rather than an empty corner of the map.
        try:
            self.d.h.gs("centre_on_selected")
        except (HarnessError, OSError):
            pass

        # Replace lost craft. Interceptor attrition costs twice: fewer craft means UFOs go
        # unintercepted, and unintercepted UFOs mean aliens infiltrating buildings, which is what
        # actually drives score into the ground.
        # Count what can actually fight in the air, not just what is parked. Every campaign
        # starts with road vehicles only, so "four craft" was a comfortable-looking number that
        # meant no air capability whatsoever: ufos_downed sat at 0 while incursions and city
        # damage -- the two buckets that actually end these runs -- climbed unopposed.
        fliers = 0
        for _, flags in craft_flags(self.d):
            if flags.get("flying") == "1" and flags.get("armed") == "1":
                fliers += 1
        mine = int(v.get("player_vehicles", "0") or 0)
        # Two interceptors cannot cover a city. Measured: 39 buildings infiltrated by day 17,
        # government relation down to -80 -- past the Hostile threshold that terminates funding
        # outright (gamestate.cpp:1668-1680) -- while the fleet stood at two Phoenixes and only
        # eleven UFOs had been shot down. Infiltration is what turns the government hostile, and
        # interception is the only thing that prevents it, so buy a real patrol. At $12,607 for
        # two guns they are cheap next to losing the campaign.
        air_patrol = self.strategy["air_patrol"]
        if fliers < air_patrol and time.time() - self.last_craft > 240.0:
            self.last_craft = time.time()
            self.say(f"{fliers} armed flier(s) < air_patrol {air_patrol} of {mine} craft - "
                     f"buying air cover")
            if not buy_interceptor(self.d, want=2) and mine < 3:
                buy_vehicles(self.d, want=1)
        # Turn captured gear into money. The guide treats recovered equipment as the campaign's
        # real income -- worth more than government funding -- and it otherwise piles up in stores
        # doing nothing. Keeps one specimen of anything unresearched so the tech tree never
        # stalls on a sold artifact.
        if time.time() - self.last_loot_sale > 420.0:
            self.last_loot_sale = time.time()
            sold = sell_surplus_loot(self.d, keep=1)
            if sold:
                self.say(f"sold {sold} surplus loot line(s)")

        # Fit the best gun in stores, whatever the fleet size. A craft flying with its default
        # armament while Lancer 7000s sit in the warehouse is how interceptors kept dying.
        if time.time() - self.last_vequip > 300.0:
            self.last_vequip = time.time()
            outcome = equip_craft(self.d)
            # Log every outcome, including the boring ones: "nothing to fit" and "never got
            # there" look identical from outside, and suppressing them hid which it was.
            if outcome != self.last_vequip_outcome or outcome == "fitted":
                self.say(f"craft weapons: {outcome}")
                self.last_vequip_outcome = outcome

        if in_city > 0:
            if time.time() - self.last_intercept > INTERCEPT_COOLDOWN_S:
                self.last_intercept = time.time()
                # Every shot that misses a UFO and hits a building costs 5 relation with its
                # owner, and 20 if it destroys the tile (scenery.cpp:1158-1186). Most buildings
                # are the government's, and government relation below -50 terminates funding
                # outright. Watching that number fall from 100 to 8 in three game-days while
                # city_damage reached -2603 is the whole argument: once relations are slipping,
                # an unintercepted UFO costs less than the collateral of engaging it over a
                # dense city. Let it go and take it at the crash site instead.
                try:
                    rel = int(self.d.h.gs("infiltrated").get("gov_relation", "100") or 100)
                except (HarnessError, OSError):
                    rel = 100
                min_rel = self.strategy["intercept_min_relation"]
                if rel < min_rel:
                    self.say(f"government relation {rel} < intercept_min_relation {min_rel}: "
                             f"holding fire over the city rather than shooting the buildings we "
                             f"are paid to protect")
                else:
                    intercept_ufos(self.d)
            # Turbo, whenever the engine will grant it. GameState::updateTurbo advances
            # TURBO_TICKS -- five game-minutes -- per frame, against six ticks per frame at
            # Speed4. Measured with modals actively cleared: 1,627 ticks/s at Speed4 versus
            # 2,735,162 at Speed5, a 1681x difference, or a game-day in about four seconds
            # instead of two hours. Reaching the alien dimension needs game-months of research,
            # so this is the difference between victory being reachable and arithmetically
            # impossible.
            #
            # An earlier reading of this said turbo froze the clock permanently. That was wrong:
            # the clock stops on any stage that is not CityView, and an undismissed AlertScreen
            # had appeared during the measurement. Turbo only pays while the city is actually the
            # current stage, which is the same reason loitering on sub-screens is expensive.
            # Do not fast-forward through an invasion. Turbo advances five game-minutes per
            # frame, and measured here it put TWO GAME-DAYS between one raid and the next -- that
            # is roughly fifty hourly ticks of alien growth and spreading (City::updateInfiltration
            # -> alienGrowth -> alienMovement) for every building we cleared. No response rate can
            # win that race. A player does not accelerate time while aliens are loose in the city,
            # and neither should this: when something is known to need clearing, run at ordinary
            # speed so the response keeps pace with the threat.
            # Hold normal speed only while we can ACT on the queue. The first version held it
            # whenever anything was pending, and since the queue never empties that meant the
            # campaign ran permanently at speed 3 -- thirty game-minutes per five wall-minutes,
            # far too slow to ever reach the research and manufacturing the endgame needs. Slowing
            # down to fight infiltration is right; slowing down while unable to fight it is just
            # losing on a different axis.
            pending = len(self.d.alerted_buildings)
            try:
                can_act = (int(self.d.h.gs("agents").get("armed", "0") or 0)
                           >= self.strategy["infil_min_armed"])
            except (HarnessError, OSError):
                can_act = False
            if pending and can_act:
                if self.turbo_held != pending:
                    self.say(f"{pending} building(s) awaiting a sweep - holding speed "
                             f"{self.strategy['infil_speed']} rather than fast-forwarding through "
                             f"the invasion")
                    self.turbo_held = pending
                set_speed(self.d, self.strategy["infil_speed"])
            else:
                self.turbo_held = 0
                set_speed(self.d, 5 if self.d.h.gs("turbo").get("can_turbo") == "1" else 4)
        else:
            # No live UFO left in the city, so anything still holding an attack order is just
            # pinning canTurbo() false and freezing the clock.
            t = self.d.h.gs("turbo")
            if int(t.get("attack_missions", "0") or 0) > 0:
                clear_attack_orders(self.d)
            set_speed(self.d, 5)  # nothing hostile left; turbo is safe and ~1681x faster

    def next_campaign(self, why: str) -> bool:
        """Retire the finished run and begin a new one. False if there is no time left to bother.

        A defeat used to end the whole unattended session, which is the wrong shape for a run
        whose purpose is to reach a victory: the value of forty-eight hours is that it can absorb
        several complete campaigns, each one starting with everything learned from the last. The
        campaign that just ended is archived intact by _load() on the next start, so nothing is
        lost by moving on.
        """
        self.say(f"campaign over ({why}); starting the next one")
        try:
            self.game.stop()
        except Exception:
            pass
        time.sleep(3.0)
        # _load() archives a run whose "ended" is set and clears the checkpoint, so the next
        # start() sees no checkpoint and begins a fresh campaign.
        try:
            if self.checkpoint.exists():
                stamp = time.strftime("%Y%m%d-%H%M%S")
                self.checkpoint.rename(self.out / f"campaign-{self.progress.get('ended', why)}"
                                                  f"-{stamp}.save")
        except OSError as exc:
            self.say(f"could not archive checkpoint: {exc}")
        # The campaign that just ended goes into the end-of-run summary before progress is reset;
        # without this, final.json described only the LAST campaign and a run that lost one and
        # started another reported the second as if the first never happened.
        self.finished.append(dict(self.progress, metrics=dict(self.metrics)))
        self.metrics = {}
        self.progress = {"battles": 0, "wins": 0, "ufos_down": 0, "recoveries": 0,
                         "restarts": 0, "research_complete": 0}
        self.flush()
        self.caps_checked = False
        self.second_base = False
        self.last_solvency_check = time.time()
        try:
            self.start()
        except Exception as exc:
            self.say(f"could not start the next campaign: {exc}")
            return False
        return True

    def bankrupt(self) -> bool:
        """True when the campaign is finished even though the engine has not said so.

        GameLost fires only when the LAST base is lost (base.cpp:150-159), so a run whose
        funding has been cut and whose treasury is empty simply limps: no income, nothing to buy
        weapons with, and a squad that cannot be armed. Seen at day 26 -- funding terminated, $249,
        every facility gone, score -6147 -- with the runner unable to call it either way.

        This lives outside victorious() deliberately: that method is only consulted once the stage
        is already a VideoScreen, which is exactly the case this condition never reaches.
        """
        if time.time() - self.last_solvency_check < 60.0:
            return False
        self.last_solvency_check = time.time()
        try:
            f = self.d.h.gs("funds")
            a = self.d.h.gs("agents")
        except (HarnessError, OSError):
            return False
        if f.get("funding_terminated") != "1":
            return False
        if int(f.get("balance", "0") or 0) >= BANKRUPT_FLOOR:
            return False
        if int(a.get("armed", "0") or 0) > 0:
            return False
        self.progress["ended"] = "bankrupt"
        self.flush()
        self.say(f"campaign bankrupt - funding cut, ${f.get('balance')} left, nobody armed, "
                 f"score {f.get('score_total')}; recording defeat")
        return True

    def victorious(self) -> bool:
        try:
            st = self.d.status()
        except OSError:
            return False
        if st.stage != "VideoScreen":
            return False
        # Both endings are a VideoScreen once the driver OKs the VICTORY/DEFEAT message box:
        # GameWon plays wingame2.smk, GameLost plays lose1.smk (CityView's GameWon/GameLost
        # handler), and the intro is a VideoScreen too. Treating any
        # VideoScreen as a win reported victory on day 8 of a campaign with one recovery and no
        # alien research at all, right after a base defence. Only the winning video counts.
        detail = (st.detail or "-").lower()
        if "wingame" in detail:
            self.progress["ended"] = "victory"
            self.flush()
            self.say(f"VICTORY - aliens defeated ({st.detail})")
            return True
        if "lose" in detail:
            self.progress["ended"] = "defeat"
            self.flush()
            self.say(f"campaign lost ({st.detail})")
            return True
        return False

    # -- end-of-run summary ----------------------------------------------------------------------
    def sample_metrics(self, force: bool = False) -> bool:
        """Refresh the cached end-of-run numbers from read-only gs queries.

        The cross-run learner scores a run from final.json, and by the time a run ends the game is
        often gone: it crashed, the restart budget ran out, or the campaign was lost and the stage
        stack no longer answers gs. So the numbers are captured WHILE the game is alive, every
        METRICS_EVERY_S, and write_final falls back to this cache when a fresh read is impossible.
        Returns True when at least the clock was read.
        """
        now = time.time()
        if not force and now - self.last_sample < METRICS_EVERY_S:
            return False
        self.last_sample = now
        if self.d is None:
            return False

        def q(name: str) -> dict:
            try:
                return self.d.h.gs(name) or {}
            except (HarnessError, OSError, ValueError):
                return {}

        def num(src: dict, key: str, default=None):
            try:
                return int(src.get(key, ""))
            except (TypeError, ValueError):
                return default

        t, f, r = q("time"), q("funds"), q("research")
        a, inf, b = q("agents"), q("infiltrated"), q("bases")
        day = num(t, "day")
        if day is None:
            return False
        m = self.metrics
        m["day"] = day
        m["max_day"] = max(day, m.get("max_day", 0))
        for key, src, field in (("score_total", f, "score_total"), ("balance", f, "balance"),
                                ("ufos_downed", f, "ufos_downed"),
                                ("tactical_missions", f, "tactical"),
                                ("research_complete", r, "complete"), ("armed", a, "armed"),
                                ("soldiers", a, "soldiers"), ("gov_relation", inf, "gov_relation"),
                                ("bases", b, "bases")):
            v = num(src, field)
            if v is not None:
                m[key] = v
        if "research_complete" in m:
            # Research done before the first sample is the starting tech, not the campaign's work.
            m.setdefault("research_complete_start", m["research_complete"])
        if f.get("funding_terminated") == "1":
            m["funding_terminated"] = True
            # The day the money stopped is the day the campaign effectively ended, however long
            # the limping husk survives after it.
            m.setdefault("funding_lost_day", day)
        elif f:
            m["funding_terminated"] = False
        m["sampled_at"] = round(now - self.started_at, 1)
        return True

    def finish(self, reason: str, rc: int) -> int:
        """Name why run() is returning. Every return path goes through here, so final.json can
        say what ended the run rather than leaving the learner to guess from a missing file."""
        self.exit_reason = reason
        return rc

    def build_final(self) -> dict:
        ended = self.progress.get("ended")
        return {
            "schema": FINAL_SCHEMA,
            "exit": self.exit_reason or "interrupted",
            "campaign_ended": ended,
            "seed": self.seed,
            "difficulty": self.difficulty,
            "strategy": self.strategy.canonical(),
            "strategy_key": self.strategy.key(),
            "battle_policy": self.battle_policy,
            "single_campaign": self.single_campaign,
            "wall_seconds": round(time.time() - self.started_at, 1),
            "restarts": self.restarts,
            "metrics": dict(self.metrics),
            "progress": dict(self.progress),
            "earlier_campaigns": list(self.finished),
        }

    def write_final(self) -> None:
        """Write final.json next to progress.json. Called from run()'s finally, so it happens on
        every exit path -- including an exception and a SIGTERM turned into SystemExit -- and it
        never raises: a failure to summarise must not mask the failure being summarised."""
        try:
            source = "cached"
            try:
                if self.d is not None and self.game is not None and self.alive():
                    if self.sample_metrics(force=True):
                        source = "live"
            except Exception:
                pass
            if not self.metrics:
                source = "none"
            final = self.build_final()
            final["metrics_source"] = source
            tmp = self.final_path.with_suffix(".json.tmp")
            tmp.write_text(json.dumps(final, indent=1))
            tmp.replace(self.final_path)
            self.say(f"final summary written ({self.exit_reason or 'interrupted'}, "
                     f"{source} metrics): {self.final_path}")
        except Exception as exc:
            try:
                print(f"[final] could not write {self.final_path}: {exc}", flush=True)
            except Exception:
                pass

    def run(self, max_hours: float) -> int:
        """Play until victory, the time budget, or an unrecoverable failure; always summarise."""
        self.started_at = time.time()
        try:
            return self._run(max_hours)
        except (SystemExit, KeyboardInterrupt):
            self.exit_reason = self.exit_reason or "signal"
            raise
        except BaseException:
            self.exit_reason = self.exit_reason or "crash"
            raise
        finally:
            self.write_final()

    def _run(self, max_hours: float) -> int:
        deadline = time.time() + max_hours * 3600
        # start() can raise: it waits for stages and issues STATUS, and if the game dies during a
        # resume those calls raise ConnectionRefusedError from inside wait_for, outside the loop's
        # own guard. That killed the whole unattended run outright -- 48 hours of budget ended by
        # one refused socket -- so treat a failed start the same way as a game that dies later.
        for attempt in range(4):
            try:
                self.start()
                break
            except (HarnessError, OSError, TimeoutError, RuntimeError) as exc:
                self.say(f"start failed ({exc}); retrying ({attempt + 1}/4)")
                try:
                    self.game.stop()
                except Exception:
                    pass
                # A checkpoint that kills the game on every resume is not worth four attempts.
                # Retire it after the second failure and let the next attempt begin a fresh
                # campaign: a lost run costs hours, but looping on an unloadable save costs the
                # whole session and produces nothing at all.
                if attempt >= 1 and self.checkpoint.exists():
                    stamp = time.strftime("%Y%m%d-%H%M%S")
                    try:
                        self.checkpoint.rename(self.out / f"unloadable-{stamp}.save")
                        self.say("checkpoint keeps killing the game on load; retiring it and "
                                 "starting a fresh campaign")
                        self.progress = {"battles": 0, "wins": 0, "ufos_down": 0,
                                         "recoveries": 0, "restarts": 0,
                                         "research_complete": 0}
                        self.flush()
                        self.second_base = False
                    except OSError as rexc:
                        self.say(f"could not retire checkpoint: {rexc}")
                time.sleep(5.0)
        else:
            self.say("could not start the game at all")
            return self.finish("start_failed", 1)
        last_report = 0.0

        while time.time() < deadline:
            try:
                if not self.alive():
                    if not self.restart():
                        return self.finish("restart_failed", 1)
                    continue
            except (HarnessError, OSError, TimeoutError) as exc:
                self.say(f"liveness check failed ({exc}); restarting")
                if not self.restart():
                    return self.finish("restart_failed", 1)
                continue
            try:
                self.sample_metrics()
                st = self.d.status()
                if st.stage == "VideoScreen" and self.victorious():
                    if self.progress.get("ended") == "victory":
                        return self.finish("victory", 0)
                    if self.single_campaign:
                        return self.finish("defeat", 0)
                    if not self.next_campaign("defeat"):
                        return self.finish("next_campaign_failed", 1)
                    continue
                if self.bankrupt():
                    if self.single_campaign:
                        return self.finish("bankrupt", 0)
                    if not self.next_campaign("bankruptcy"):
                        return self.finish("next_campaign_failed", 1)
                    continue
                if st.stage == "VideoScreen":
                    # Some other cutscene (the intro, most likely). Skip it and carry on.
                    self.d.h.key("Escape")
                    time.sleep(0.5)
                    continue
                if st.stage in BATTLE_STAGES:
                    self.fight(st.stage)
                    continue
                if st.stage == "AlertScreen":
                    # Record the building BEFORE doing anything else with the alert. This branch
                    # handles AlertScreen itself and never reaches respond_to_event, so the
                    # note_alert hook there never fired once -- the driver's only honest source of
                    # "where are the aliens" was silently dead, and no infiltration raid ever had
                    # an address to go to.
                    self.d.note_alert(st)
                    # EXTERMINATE is refused outright when no craft can take the squad -- with a
                    # MessageBox, after which the alert is still up. Retrying it on the next pass
                    # produced an endless dispatch/refuse loop that froze the clock for as long
                    # as the alert stood. Try once, then get out of the way and let the city run;
                    # the incident will come back around when a craft is free.
                    # Do not send a token force. Mission #2 of the losing run put four agents
                    # against twenty-three aliens and lost all four; the guides put a working
                    # squad at around six.
                    #
                    # Correcting an earlier comment here that was simply wrong about the engine:
                    # declining an incident does NOT cost score. The -30 alienIncidents penalty is
                    # charged when a building's alien crew is *detected*, before any response, and
                    # is not refunded or repeated based on what the player does about it. What a
                    # wiped squad costs is far worse -- dead agents also drag the tactical score
                    # negative even on a mission that is won.
                    # Do not answer an alert whose aliens have already left. Investigating a
                    # building and finding nothing costs its owner -5 - difficulty relation
                    # (buildingscreen.cpp:154-166), crews relocate on a timer, and 39 such
                    # investigations drove the government from +85 to -100 -- Hostile -- which
                    # latches fundingTerminated permanently. STATUS reports the alert's building
                    # and its current crew via the stage detail hook.
                    detail = st.detail or ""
                    if "crew=0" in detail:
                        self.say(f"alert building is already empty ({detail}); not investigating")
                        if not self.d.click_id("BUTTON_QUIT", st):
                            self.d.h.key("Escape")
                        time.sleep(0.5)
                        continue

                    fit_now = int(self.d.h.gs("agents").get("soldiers_fit", "0") or 0)
                    min_squad = self.strategy["min_squad"]
                    if fit_now < min_squad:
                        self.say(f"only {fit_now} fit soldiers < min_squad {min_squad}; not "
                                 f"dispatching a token force")
                        if not self.d.click_id("BUTTON_QUIT", st):
                            self.d.h.key("Escape")
                        time.sleep(0.6)
                        continue
                    # The remembered incident can be dispatched from BuildingScreen after
                    # the ordinary UI refit. AlertScreen itself cannot buy or issue equipment.
                    if not self.d.click_id("BUTTON_QUIT", st):
                        self.d.escape_key(st.stage)
                    time.sleep(0.5)
                    if self.d.status().stage == "CityView":
                        outcome = raid_infiltrated_building(self.d)
                        self.say(f"incident after role refit: {outcome}")
                    continue
                if st.stage == "CityView":
                    self.stuck_since = 0.0
                    self.city_turn()
                elif not self.d.dismiss_modal(st):
                    # dismiss_modal deliberately does nothing on "working" stages -- the
                    # UFOpaedia among them -- because a test that is exercising those screens
                    # wants to stay. A campaign does not: finishing a research topic opens
                    # UfopaediaCategoryView, and the runner sat in it with the clock stopped.
                    # That only became reachable once research started completing at all.
                    if not self.stuck_since:
                        self.stuck_since = time.time()
                    elif time.time() - self.stuck_since > 6.0:
                        self.say(f"stranded on {st.stage}; backing out to the city")
                        # Pop the whole stack rather than one screen: the stack can be several
                        # deep (ResearchScreen over a leftover BuildingScreen), and clearing one
                        # layer per attempt just bounced between them with the clock stopped.
                        return_to_city(self.d)
                        self.stuck_since = time.time()
                    time.sleep(0.4)
                else:
                    self.stuck_since = 0.0

                if time.time() - self.last_checkpoint > CHECKPOINT_EVERY_S:
                    self.save("periodic")
                if time.time() - last_report > 300:
                    last_report = time.time()
                    t = self.d.h.gs("time")
                    self.say(f"progress {self.progress} | {t}")
                time.sleep(0.8)
            except TimeoutError as exc:
                self.say(f"timed out waiting on a stage: {exc}")
                time.sleep(1.0)
            except HarnessError as exc:
                self.say(f"harness error: {exc}")
                time.sleep(1.0)
            except OSError as exc:
                self.say(f"connection lost: {exc}")
                if not self.restart():
                    return self.finish("restart_failed", 1)
            except Exception:
                self.say("unexpected error:\n" + traceback.format_exc())
                time.sleep(2.0)

        self.say(f"time budget reached; progress: {self.progress}")
        return self.finish("time_budget", 0)


def _on_sigterm(signum, frame):
    raise SystemExit(143)


def main() -> int:
    ap = argparse.ArgumentParser()
    add_runner_options(ap)
    ap.add_argument("--port", type=int, default=0,
                    help="harness port; 0 picks a free one near 17800")
    ap.add_argument("--repo", default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument("--out", default=None)
    ap.add_argument("--difficulty", type=int, default=1, help="1 = Novice")
    ap.add_argument("--hours", type=float, default=72.0)
    ap.add_argument("--seed", type=int, default=0,
                    help="RNG seed (0 = engine default); give parallel runs different seeds")
    ap.add_argument("--single-campaign", action="store_true",
                    help="end the run when the campaign is lost instead of starting the next one "
                         "(what the cross-run learner scores)")
    add_strategy_option(ap)
    args = ap.parse_args()
    policy = configure_runner(args)
    strategy, policy = resolve_strategy(args, policy)
    args.port = args.port or free_port(17800)
    repo = Path(args.repo)
    out = Path(args.out) if args.out else repo / "build/victory"
    v = Victory(repo, out, args.port, args.difficulty, battle_policy=policy, seed=args.seed,
                strategy=strategy, single_campaign=args.single_campaign)
    # A learner stops a run with SIGTERM when its wall-clock budget is spent. Python's default
    # handler kills the process without running finally blocks, which would skip final.json and
    # leave the game orphaned; turning it into SystemExit lets run()'s finally summarise.
    signal.signal(signal.SIGTERM, _on_sigterm)
    try:
        return v.run(args.hours)
    finally:
        try:
            if v.alive():
                v.save("shutdown")
            v.game.stop()
        except Exception:
            pass


if __name__ == "__main__":
    raise SystemExit(main())
