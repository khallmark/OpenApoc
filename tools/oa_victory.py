#!/usr/bin/env python3
"""Play OpenApoc from a Novice start toward victory, unattended and without cheating.

Victory in this engine is precise: raid BUILDING_DIMENSION_GATE_GENERATOR in the alien dimension
and win the tactical mission. Battle::exitBattle sets AliensDefeated only for the one building
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
import time
import traceback
from pathlib import Path

from oa_play import (
    Driver,
    GameProcess,
    Harness,
    HarnessError,
    assign_research,
    build_facility,
    return_to_city,
    buy_equipment,
    buy_vehicles,
    stock_for_template,
    equip_squad,
    hire_scientists,
    hire_soldiers,
    crew_transport,
    clear_attack_orders,
    intercept_ufos,
    new_game,
    recover_crash_sites,
    win_battle,
)

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
RESEARCH_COOLDOWN_S = 45.0
# Soldiers are lost permanently. Below this many fit soldiers there is no squad left to send.
MIN_SOLDIERS = 10
# Fewer than this and an incident is not worth answering: the squad dies and the score hit from
# losing it dwarfs the hit from declining.
MIN_SQUAD = 6
# Below this much total lab skill the research chain is crawling and worth spending money on.
MIN_LAB_SKILL = 700
CHECKPOINT_EVERY_S = 300.0


class Victory:
    def __init__(self, repo: Path, out: Path, port: int, difficulty: int = 1):
        self.repo, self.out, self.port = Path(repo), Path(out), port
        self.difficulty = difficulty
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
        self.alert_refusals = 0
        self.last_research = 0.0
        self.last_hire = 0.0
        self.stuck_since = 0.0
        self.last_build = 0.0
        self.built_workshop = False
        self.last_equip = 0.0
        self.last_craft = 0.0
        self.last_score_warn = 0.0
        self.last_checkpoint = 0.0
        self.best_crashed = 0

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

    def say(self, msg: str) -> None:
        line = f"[{time.strftime('%H:%M:%S')}] {msg}"
        print(line, flush=True)
        with open(self.out / "victory.log", "a") as f:
            f.write(line + "\n")

    def save(self, why: str) -> None:
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
        self.game = GameProcess(self.repo, self.port, self.out / "game.log", extra=extra)
        self.game.start(wait_s=240)
        self.d = Driver(Harness(port=self.port), self.repo / "data/forms",
                        shots=self.out / "shots", verbose=True)
        self.d.checks = {}
        self.d.say = lambda m: self.say(m)
        if resume:
            self.say("resumed from checkpoint")
            self.d.wait_for(("CityView",), 240)
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
            stock_for_template(self.d, qty=12)
            crew_transport(self.d)
            # Capture the squad loadout now, while the starting ten are all home and armed.
            # It persists in GameState, so every later recruit can be equipped from it.
            equip_squad(self.d, agents=20, apply=False)
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
            self.say(f"game died - restarting from checkpoint (#{self.restarts}, attempt {attempt}/3)")
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
            outcome = win_battle(self.d, budget_s=2400)
        except (HarnessError, OSError) as exc:
            outcome = f"lost connection: {exc}"
        if outcome == "resolved":
            self.progress["wins"] += 1
        self.say(f"=== mission #{n} outcome: {outcome} (wins {self.progress['wins']}) ===")
        self.flush()
        if self.alive():
            self.save(f"after mission {n}")

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

        v = self.d.h.gs("vehicles")
        crashed = int(v.get("ufos_crashed", "0") or 0)
        in_city = int(v.get("ufos_in_city", "0") or 0)
        crewed = int(v.get("crewed", "0") or 0)

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
            if skill < MIN_LAB_SKILL and time.time() - self.last_hire > 180.0:
                # Scientists get dispatched to incidents along with everyone else and die there,
                # which silently throttles the whole research chain.
                self.last_hire = time.time()
                self.say(f"lab skill down to {skill} - recruiting scientists")
                hire_scientists(self.d, want=4)
            if idle > 0 or skill < MIN_LAB_SKILL:
                self.say(f"{idle} lab(s) idle, skill {skill}, {done} topics complete - reassigning")
                assign_research(self.d)
            self.progress["research_complete"] = done
            self.flush()

        # The Large workshop is the gate on MANUFACTURE_DIMENSION_SHIFTER, which is the gate on
        # reaching the alien dimension at all. It only appears once RESEARCH_ADVANCED_WORKSHOP
        # lands, so keep checking.
        if not self.built_workshop and time.time() - self.last_build > 90.0:
            self.last_build = time.time()
            fac = self.d.h.gs("facilities")
            if "FACILITYTYPE_ADVANCED_WORKSHOP" in fac.get("offer", ""):
                if "FACILITYTYPE_ADVANCED_WORKSHOP" in fac.get("base", ""):
                    self.built_workshop = True
                    self.say("advanced workshop already present")
                elif build_facility(self.d, "FACILITYTYPE_ADVANCED_WORKSHOP"):
                    self.built_workshop = True
                    self.record("advanced_workshop_built")
                    self.say("=== advanced workshop under construction ===")

        # Arm whoever is unarmed. This is not housekeeping: an unarmed agent in a base defence
        # is a free kill, and losing the base loses the campaign outright.
        ag = self.d.h.gs("agents")
        armed = int(ag.get("armed", "0") or 0)
        soldiers = int(ag.get("soldiers", "0") or 0)
        if armed < soldiers and time.time() - self.last_equip > 150.0:
            self.last_equip = time.time()
            self.say(f"{armed} armed of {soldiers} soldiers - equipping")
            if equip_squad(self.d, agents=24) <= 0:
                # Stores ran dry rather than the mechanism failing: the market only restocks so
                # much per week, so keep re-ordering the loadout's own item types.
                stock_for_template(self.d, qty=12)

        # Replace losses, and arm them if the armoury can.
        fit = int(ag.get("soldiers_fit", "0") or 0)
        if fit < MIN_SOLDIERS and time.time() - self.last_hire > 180.0:
            # Only recruit people we can actually arm. An unarmed agent is not a neutral
            # addition: in a base defence every person present is dropped into the fight whether
            # they can shoot or not, and the campaign that was lost went down with 21 mostly
            # unarmed bodies against 11 aliens. The run that is healthy right now is healthy
            # precisely because it never diluted its ten armed veterans -- armed 10 of 10.
            stock = int(self.d.h.gs("stores").get("weapons", "0") or 0)
            if stock <= 0:
                self.last_hire = time.time()
                self.say(f"{fit} fit soldiers but no weapons in stores - buying before hiring")
                stock_for_template(self.d, qty=12)
            else:
                self.last_hire = time.time()
                want = min(MIN_SOLDIERS - fit + 2, stock)
                self.say(f"only {fit} fit soldiers, {stock} weapons in stock - recruiting {want}")
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
        mine = int(v.get("player_vehicles", "0") or 0)
        if mine < 4 and time.time() - self.last_craft > 240.0:
            self.last_craft = time.time()
            self.say(f"down to {mine} craft - buying replacements")
            buy_vehicles(self.d, want=2)

        if in_city > 0:
            if time.time() - self.last_intercept > INTERCEPT_COOLDOWN_S:
                self.last_intercept = time.time()
                intercept_ufos(self.d)
            # canTurbo() is false while hostiles are up, so ask for the fastest speed that is
            # actually granted. Without this the clock sat still for minutes at a time.
            self.d.h.key("4")
        else:
            # No live UFO left in the city, so anything still holding an attack order is just
            # pinning canTurbo() false and freezing the clock.
            t = self.d.h.gs("turbo")
            if int(t.get("attack_missions", "0") or 0) > 0:
                clear_attack_orders(self.d)
            self.d.h.key("5")  # turbo; canTurbo() downgrades it by itself when combat is live

    def victorious(self) -> bool:
        try:
            st = self.d.status()
        except OSError:
            return False
        if st.stage != "VideoScreen":
            return False
        # Both endings are a VideoScreen: AliensDefeated plays wingame2.smk, XComDefeated plays
        # lose1.smk (cityview.cpp:4686-4699), and the intro is a VideoScreen too. Treating any
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

    def run(self, max_hours: float) -> int:
        deadline = time.time() + max_hours * 3600
        self.start()
        last_report = 0.0

        while time.time() < deadline:
            if not self.alive():
                if not self.restart():
                    return 1
                continue
            try:
                st = self.d.status()
                if st.stage == "VideoScreen" and self.victorious():
                    return 0 if self.progress.get("ended") == "victory" else 1
                if st.stage == "VideoScreen":
                    # Some other cutscene (the intro, most likely). Skip it and carry on.
                    self.d.h.key("Escape")
                    time.sleep(0.5)
                    continue
                if st.stage in BATTLE_STAGES:
                    self.fight(st.stage)
                    continue
                if st.stage == "AlertScreen":
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
                    fit_now = int(self.d.h.gs("agents").get("soldiers_fit", "0") or 0)
                    if fit_now < MIN_SQUAD:
                        self.say(f"only {fit_now} fit soldiers; not dispatching a token force")
                        if not self.d.click_id("BUTTON_QUIT", st):
                            self.d.h.key("Escape")
                        time.sleep(0.6)
                        continue
                    n = self.d.select_assignment_rows(st)
                    self.d.click_id("BUTTON_EXTERMINATE", st)
                    time.sleep(1.2)
                    cur = self.d.status()
                    for _ in range(4):
                        if cur.stage != "MessageBox":
                            break
                        self.d.h.key("Return")
                        time.sleep(0.4)
                        cur = self.d.status()
                    if cur.stage == "AlertScreen":
                        if not self.d.click_id("BUTTON_QUIT", cur):
                            self.d.h.key("Escape")
                        self.alert_refusals += 1
                        self.say(f"incident dispatch refused ({self.alert_refusals}); dismissed")
                    else:
                        self.say(f"squad dispatched to incident ({n} rows)")
                    time.sleep(0.5)
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
            except HarnessError as exc:
                self.say(f"harness error: {exc}")
                time.sleep(1.0)
            except OSError as exc:
                self.say(f"connection lost: {exc}")
                if not self.restart():
                    return 1
            except Exception:
                self.say("unexpected error:\n" + traceback.format_exc())
                time.sleep(2.0)

        self.say(f"time budget reached; progress: {self.progress}")
        return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=17800)
    ap.add_argument("--repo", default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument("--out", default=None)
    ap.add_argument("--difficulty", type=int, default=1, help="1 = Novice")
    ap.add_argument("--hours", type=float, default=72.0)
    args = ap.parse_args()
    repo = Path(args.repo)
    out = Path(args.out) if args.out else repo / "build/victory"
    v = Victory(repo, out, args.port, args.difficulty)
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
