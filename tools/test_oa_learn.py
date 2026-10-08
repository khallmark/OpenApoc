#!/usr/bin/env python3
"""Tests for the campaign genome, the end-of-run summary and the cross-run learner.

No game and no sockets. The load-bearing claims, each with a test that fails without its fix:

  * a gene reaches the value that actually controls behaviour (not merely parsed and stored);
  * defaults are today's constants, and a default strategy asks the battle code for nothing;
  * final.json is written on EVERY way a run can end;
  * the learner's update step matters: on a synthetic landscape with a known optimum it beats
    random search, and it does so deterministically from a seed.
"""
import json
import os
import random
import signal
import subprocess
import sys
import textwrap
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace
from unittest.mock import DEFAULT, MagicMock, patch

import oa_learn
import oa_play
import oa_strategy
import oa_victory
from oa_play import Status
from oa_strategy import DEFAULTS, GENES, Strategy, parse_strategy, reorder_research

TOOLS = Path(__file__).resolve().parent


# ---------------------------------------------------------------------------
# Strategy
# ---------------------------------------------------------------------------

def test_defaults_are_todays_constants():
    """The genome's defaults must equal the runner's own constants, or "defaults = today's
    behaviour" is a claim nothing checks."""
    assert DEFAULTS["air_patrol"] == oa_victory.AIR_PATROL
    assert DEFAULTS["min_soldiers"] == oa_victory.MIN_SOLDIERS
    assert DEFAULTS["min_squad"] == oa_victory.MIN_SQUAD
    assert DEFAULTS["min_lab_skill"] == oa_victory.MIN_LAB_SKILL
    assert DEFAULTS["intercept_min_relation"] == oa_victory.INTERCEPT_MIN_RELATION
    assert DEFAULTS["infil_min_armed"] == oa_victory.INFIL_MIN_ARMED
    assert DEFAULTS["infil_speed"] == oa_victory.INFIL_SPEED
    import inspect
    assert inspect.signature(oa_play.crew_transport).parameters["garrison"].default is None
    assert DEFAULTS["garrison"] == 4          # crew_transport's historical default
    assert DEFAULTS["research_order"] == "balanced"
    # A default strategy hands the battle code the empty policy it always got.
    assert Strategy().battle_policy() == {}
    assert Strategy().is_default() and Strategy().key() == "default"
    assert 6 <= len(GENES) <= 15


def test_strategy_validation_and_canonical_identity():
    for bad in ({"no_such_gene": 1}, {"air_patrol": 99}, {"air_patrol": 4.5},
                {"research_order": "sideways"}, {"fire_mode": "laser"}):
        try:
            Strategy(bad)
        except ValueError:
            pass
        else:
            raise AssertionError(f"{bad} must be rejected")
    # Doctrine genes are inert unless the veteran AI is on: same effect, same identity.
    a = Strategy({"behaviour": "aggressive", "withdraw_ratio": 3.0})
    assert a.key() == "default" and a == Strategy()
    b = Strategy({"tactical_ai": "veteran", "behaviour": "aggressive"})
    assert b.key() == "tactical_ai=veteran,behaviour=aggressive"
    assert b != Strategy({"tactical_ai": "veteran"})
    # 6 and 6.0 are one setting.
    assert Strategy({"tactical_ai": "veteran", "withdraw_ratio": 100}).key() == \
        Strategy({"tactical_ai": "veteran", "withdraw_ratio": 100.0}).key()
    # An in-range numeric value that is not on the search grid is allowed for a hand-written run.
    assert Strategy({"air_patrol": 10})["air_patrol"] == 10


def test_parse_strategy_inline_and_file():
    assert parse_strategy(None).is_default()
    assert parse_strategy('{"air_patrol": 12}')["air_patrol"] == 12
    with TemporaryDirectory() as tmp:
        p = Path(tmp) / "s.json"
        p.write_text(json.dumps({"research_order": "victory_first"}))
        assert parse_strategy(str(p))["research_order"] == "victory_first"
    for bad in ("/no/such/file.json", "[1,2]", '{"air_patrol": '):
        try:
            parse_strategy(bad)
        except ValueError:
            pass
        else:
            raise AssertionError(f"{bad!r} must be rejected")


def test_battle_policy_reaches_the_veteran_ai_constructor():
    """Doctrine genes must land in the AI object win_battle builds, not stop at a dict."""
    s = Strategy({"tactical_ai": "veteran", "behaviour": "aggressive", "withdraw_ratio": 3.0,
                  "fire_mode": "auto"})
    policy = s.battle_policy()
    assert policy["ai"] == "veteran" and policy["fire_mode"] == "auto"
    brain, caps = oa_play.build_battle_ai(policy)
    assert brain.behaviour == "aggressive" and brain.withdraw_ratio == 3.0
    assert brain.fire_mode == "auto"
    # fire_mode alone is a policy without an AI: _fight_battle applies it, build_battle_ai does not.
    only = Strategy({"fire_mode": "aimed"}).battle_policy()
    assert only["fire_mode"] == "aimed" and "ai" not in only
    assert oa_play.build_battle_ai(only) == (None, None)


def test_main_threads_strategy_to_victory_and_play():
    argv = [oa_victory.__file__, "--port", "18234", "--strategy",
            '{"air_patrol": 12, "tactical_ai": "veteran", "behaviour": "evasive"}',
            "--single-campaign"]
    with patch.object(sys, "argv", argv), patch.dict(os.environ), \
            patch.object(oa_victory, "Victory") as victory:
        victory.return_value.run.return_value = 0
        assert oa_victory.main() == 0
        kw = victory.call_args.kwargs
        assert kw["strategy"]["air_patrol"] == 12 and kw["single_campaign"] is True
        assert kw["battle_policy"]["ai"] == "veteran"
        assert kw["battle_policy"]["behaviour"] == "evasive"
    # Without --strategy nothing changes: no strategy genes, empty policy.
    with patch.object(sys, "argv", [oa_victory.__file__, "--port", "18234"]), \
            patch.dict(os.environ), patch.object(oa_victory, "Victory") as victory:
        victory.return_value.run.return_value = 0
        oa_victory.main()
        assert victory.call_args.kwargs["strategy"].is_default()
        assert victory.call_args.kwargs["battle_policy"] == {}

    seen = {}

    def fake_campaign(d, *args):
        seen["strategy"], seen["policy"] = d.strategy, d.battle_policy
        return {}
    with TemporaryDirectory() as tmp, \
            patch.object(sys, "argv", [oa_play.__file__, "--no-launch", "--out", tmp,
                                       "--strategy", '{"research_order": "weapons_first",'
                                       '"fire_mode": "aimed"}']), \
            patch.dict(os.environ), patch.object(oa_play, "play_campaign", fake_campaign):
        oa_play.main()
    assert seen["strategy"]["research_order"] == "weapons_first"
    assert seen["policy"]["fire_mode"] == "aimed"


def test_research_order_changes_the_first_pick():
    detail = "|".join(f"{i}={t},done=0,big=0" for i, t in enumerate(
        ["RESEARCH_BRAINSUCKER_PODS", "RESEARCH_DISRUPTOR_GUN", "RESEARCH_ALIEN_BUILDING_1",
         "RESEARCH_ADVANCED_WORKSHOP"]))

    def first(order):
        d = SimpleNamespace(strategy=Strategy({"research_order": order}),
                            h=SimpleNamespace(gs=lambda q: {"detail": detail}))
        return oa_play.pick_topic_rows(d)[0][1]

    picks = {o: first(o) for o in ("balanced", "victory_first", "weapons_first")}
    assert picks["victory_first"] == "RESEARCH_ADVANCED_WORKSHOP", picks
    assert picks["weapons_first"] == "RESEARCH_DISRUPTOR_GUN", picks
    offered = {"RESEARCH_BRAINSUCKER_PODS", "RESEARCH_DISRUPTOR_GUN",
               "RESEARCH_ALIEN_BUILDING_1", "RESEARCH_ADVANCED_WORKSHOP"}
    assert picks["balanced"] == next(t for t in oa_play.PRIORITY_RESEARCH if t in offered), picks
    # Reordering only moves topics; it never drops or invents one.
    base = oa_play.PRIORITY_RESEARCH
    for order in ("victory_first", "weapons_first"):
        assert sorted(reorder_research(base, order)) == sorted(base)


def test_garrison_reaches_crew_transport():
    from test_oa_crew import FakeDriver, FakeBase, FakeVehicle
    def held(garrison_gene):
        base = FakeBase([FakeVehicle("Transport_1", True, 12)], 20)
        d = FakeDriver(base)
        d.strategy = Strategy({"garrison": garrison_gene})
        with patch.object(oa_play.time, "sleep"):
            assert oa_play.crew_transport(d) == 1
        assert base.vehicles[0].crew == 6
        return [m for m in d.said if "stay to defend" in m]
    assert "2 stay to defend" in held(2)[0], held(2)
    assert "8 stay to defend" in held(8)[0], held(8)
    # Small rosters expose the amount actually boarded, rather than only the log.
    for garrison, expected in ((2, 6), (8, 5)):
        base = FakeBase([FakeVehicle("Transport_1", True, 12)], 10)
        d = FakeDriver(base)
        d.strategy = Strategy({"garrison": garrison})
        with patch.object(oa_play.time, "sleep"):
            oa_play.crew_transport(d)
        assert base.vehicles[0].crew == expected


# ---------------------------------------------------------------------------
# city_turn: each gene steers the decision it names
# ---------------------------------------------------------------------------

HELPERS = ("raid_alien_building", "goto_portal", "manufacture", "crew_transport",
           "recover_crash_sites", "raid_infiltrated_building", "build_second_base",
           "hire_scientists", "hire_engineers", "assign_research", "build_facility",
           "arm_squad", "hire_soldiers", "buy_interceptor",
           "buy_vehicles", "sell_surplus_loot", "equip_craft", "intercept_ufos", "set_speed",
           "clear_attack_orders")


def base_data(**over):
    data = {
        "time": {"day": "12", "ticks": "1"},
        "funds": {"balance": "100000", "margin_to_cutoff": "5000", "funding_terminated": "0",
                  "score_total": "300", "ufos_downed": "4", "tactical": "2"},
        "infiltrated": {"gov_relation": "80"},
        "alien_buildings": {"current_city": "CITYMAP", "raidable": "0"},
        "interceptors": {"detail": "|".join(f"{i}:x:flying=1,armed=1,crew=0" for i in range(8))},
        "vehicles": {"ufos_crashed": "0", "ufos_in_city": "0", "crewed": "1",
                     "player_vehicles": "9"},
        "agents": {"armed": "20", "soldiers": "20", "soldiers_fit": "25"},
        "research": {"assignable": "0", "assignable_busy": "0", "complete": "7",
                     "labs_detail": "a:skill=2000", "startable": "0"},
        "stores": {"weapons": "10"},
        "turbo": {"can_turbo": "1", "attack_missions": "0"},
        "bases": {"bases": "1"},
    }
    data.update(over)
    return data


class FakeHarness:
    def __init__(self, data):
        self.data = data

    def gs(self, q):
        return dict(self.data.get(q, {}))

    def send(self, cmd):
        return "OK"

    def key(self, k):
        pass

    def ok(self, cmd):
        return "ok"


class FakeDriver:
    def __init__(self, data, strategy, alerted=(), stage=None):
        self.h = FakeHarness(data)
        self.strategy = strategy
        self.alerted_buildings = list(alerted)
        self.said, self.calls = [], []
        self.stage = stage or Status("CityView", 1280, 720, "")
        self.checks, self.battle_policy = {}, {}

    def say(self, msg):
        self.said.append(msg)

    def status(self):
        return self.stage

    def note_alert(self, st):
        pass

    def click_id(self, *a, **k):
        return True

    def select_assignment_rows(self, st):
        self.calls.append("select_assignment_rows")
        return 3

    def dismiss_modal(self, st):
        return True


def make_victory(tmp, strategy=None, data=None, alerted=(), stage=None, **kw):
    v = oa_victory.Victory(Path(tmp), Path(tmp) / "out", 19999,
                           strategy=Strategy(strategy or {}), **kw)
    v.d = FakeDriver(data if data is not None else base_data(), v.strategy, alerted, stage)
    v.second_base = True
    v.built_workshop = True
    return v


def turn(strategy, data, alerted=()):
    """One city_turn with every game-touching helper mocked; returns (mocks, said)."""
    with TemporaryDirectory() as tmp, patch.multiple(oa_victory, **{h: DEFAULT for h in HELPERS}) \
            as m:
        m["raid_infiltrated_building"].return_value = "nothing-reported"
        m["equip_craft"].return_value = "nothing"
        for name in ("buy_interceptor", "buy_vehicles", "hire_soldiers", "sell_surplus_loot",
                     "crew_transport", "arm_squad"):
            m[name].return_value = 0
        v = make_victory(tmp, strategy, data, alerted)
        v.city_turn()
        log = (Path(tmp) / "out" / "victory.log").read_text().splitlines()
        return m, v.d.said + log


def test_air_patrol_gates_interceptor_purchase():
    five = base_data(interceptors={"detail": "|".join(
        f"{i}:x:flying=1,armed=1,crew=0" for i in range(5))})
    m, _ = turn({}, five)
    assert m["buy_interceptor"].called, "5 fliers < default air_patrol 8 must buy"
    m, said = turn({"air_patrol": 4}, five)
    assert not m["buy_interceptor"].called, "5 fliers >= air_patrol 4 must not buy"
    m, said = turn({"air_patrol": 12}, base_data())
    assert m["buy_interceptor"].called and any("air_patrol 12" in s for s in said), said


def test_min_soldiers_gates_hiring():
    data = base_data(agents={"armed": "20", "soldiers": "20", "soldiers_fit": "20"})
    m, _ = turn({}, data)
    assert not m["hire_soldiers"].called, "20 fit >= default min_soldiers 18"
    m, said = turn({"min_soldiers": 24}, data)
    assert m["hire_soldiers"].call_args.kwargs == {"want": 6}, m["hire_soldiers"].call_args
    assert any("min_soldiers 24" in s for s in said), said


def test_intercept_min_relation_gates_firing_over_the_city():
    data = base_data(vehicles={"ufos_crashed": "0", "ufos_in_city": "1", "crewed": "1",
                               "player_vehicles": "9"}, infiltrated={"gov_relation": "20"})
    m, said = turn({}, data)
    assert not m["intercept_ufos"].called, "relation 20 < default 25 holds fire"
    assert any("holding fire" in s for s in said)
    m, _ = turn({"intercept_min_relation": 15}, data)
    assert m["intercept_ufos"].called, "relation 20 >= 15 intercepts"


def test_infil_min_armed_gates_the_sweep():
    data = base_data(agents={"armed": "4", "soldiers": "4", "soldiers_fit": "25"})
    m, _ = turn({}, data, alerted=["BUILDING_X"])
    assert m["raid_infiltrated_building"].called, "4 armed >= default 3"
    m, _ = turn({"infil_min_armed": 5}, data, alerted=["BUILDING_X"])
    assert not m["raid_infiltrated_building"].called, "4 armed < infil_min_armed 5"


def test_infil_speed_is_the_clock_speed_while_buildings_await_a_sweep():
    data = base_data(vehicles={"ufos_crashed": "0", "ufos_in_city": "1", "crewed": "1",
                               "player_vehicles": "9"},
                     agents={"armed": "4", "soldiers": "4", "soldiers_fit": "25"})
    for gene, expect in ((None, 3), (5, 5), (4, 4)):
        m, _ = turn({} if gene is None else {"infil_speed": gene}, data, alerted=["B"])
        speeds = [c.args[1] for c in m["set_speed"].call_args_list]
        assert speeds == [expect], (gene, speeds)


def test_min_lab_skill_gates_scientist_hiring():
    data = base_data(research={"assignable": "0", "assignable_busy": "0", "complete": "7",
                               "labs_detail": "a:skill=1500", "startable": "0"})
    m, _ = turn({}, data)
    assert not m["hire_scientists"].called, "skill 1500 >= default 1100"
    m, said = turn({"min_lab_skill": 1600}, data)
    assert m["hire_scientists"].called and any("min_lab_skill 1600" in s for s in said), said


def test_min_squad_gates_incident_dispatch():
    alert = Status("AlertScreen", 1280, 720, "", "crew=5")
    data = base_data(agents={"armed": "5", "soldiers": "5", "soldiers_fit": "5"})

    def dispatched(strategy):
        # Dispatch leaves the alert for the city and raids from there, after the role refit;
        # declining also leaves it, but never raids.
        with TemporaryDirectory() as tmp, patch.object(oa_victory.time, "sleep"), \
                patch.object(oa_victory, "raid_infiltrated_building",
                             return_value="raided") as raid:
            v = make_victory(tmp, strategy, data, stage=alert)
            v.start = lambda: None
            v.game = MagicMock()
            v.alive = lambda: True
            v.city_turn = lambda: None

            def click_id(control, *a, **k):
                if control == "BUTTON_QUIT":
                    v.d.stage = Status("CityView", 1280, 720, "")
                return True
            v.d.click_id = click_id
            v.run(0.00002)
            return raid.called

    assert not dispatched({}), "5 fit < default min_squad 6: decline"
    assert dispatched({"min_squad": 3}), "5 fit >= min_squad 3: dispatch"


# ---------------------------------------------------------------------------
# final.json on every exit path
# ---------------------------------------------------------------------------

def run_exit(configure, strategy=None, stage=None, data=None, hours=0.00002, **kw):
    """Run Victory.run with a scripted fake; returns (rc or exception, final.json dict)."""
    with TemporaryDirectory() as tmp, patch.object(oa_victory.time, "sleep"):
        v = make_victory(tmp, strategy, data, stage=stage, **kw)
        v.start = lambda: None
        v.game = MagicMock()
        v.alive = lambda: True
        v.city_turn = lambda: None
        configure(v)
        try:
            outcome = v.run(hours)
        except BaseException as exc:             # noqa: BLE001 - the point is what survives
            outcome = exc
        final = json.loads((Path(tmp) / "out" / "final.json").read_text())
        return outcome, final


def test_final_json_time_budget_has_live_metrics_and_the_genome():
    rc, f = run_exit(lambda v: None, {"air_patrol": 12})
    assert rc == 0 and f["exit"] == "time_budget" and f["metrics_source"] == "live"
    assert f["metrics"]["max_day"] == 12 and f["metrics"]["score_total"] == 300
    assert f["metrics"]["research_complete_start"] == 7
    assert f["strategy"]["air_patrol"] == 12 and f["strategy_key"] == "air_patrol=12"
    assert f["schema"] == oa_victory.FINAL_SCHEMA and f["seed"] == 0


def test_final_json_victory_and_defeat_and_bankrupt():
    rc, f = run_exit(lambda v: None, stage=Status("VideoScreen", 1, 1, "", "wingame2.smk"))
    assert rc == 0 and f["exit"] == "victory" and f["campaign_ended"] == "victory"
    rc, f = run_exit(lambda v: None, stage=Status("VideoScreen", 1, 1, "", "lose1.smk"),
                     single_campaign=True)
    assert rc == 0 and f["exit"] == "defeat" and f["campaign_ended"] == "defeat"
    # Without single-campaign a defeat rolls into the next campaign (today's behaviour).
    calls = []
    rc, f = run_exit(lambda v: setattr(v, "next_campaign", lambda why: calls.append(why) or False),
                     stage=Status("VideoScreen", 1, 1, "", "lose1.smk"))
    assert calls == ["defeat"] and rc == 1 and f["exit"] == "next_campaign_failed"
    broke = base_data(funds={"balance": "100", "funding_terminated": "1", "margin_to_cutoff": "-5",
                             "score_total": "-2500"},
                      agents={"armed": "0", "soldiers": "3", "soldiers_fit": "3"})
    rc, f = run_exit(lambda v: None, data=broke, single_campaign=True)
    assert rc == 0 and f["exit"] == "bankrupt" and f["campaign_ended"] == "bankrupt"
    assert f["metrics"]["funding_lost_day"] == 12 and f["metrics"]["funding_terminated"] is True


def test_final_json_when_the_run_cannot_continue():
    def boom():
        raise RuntimeError("no game")
    def no_game(v):
        v.start = boom
        v.d = None
    rc, f = run_exit(no_game)
    assert rc == 1 and f["exit"] == "start_failed" and f["metrics_source"] == "none"

    def dead(v):
        v.alive = lambda: False
        v.restart = lambda: False
        v.metrics = {"day": 5, "max_day": 5, "score_total": -10}
    rc, f = run_exit(dead)
    assert rc == 1 and f["exit"] == "restart_failed"
    assert f["metrics_source"] == "cached" and f["metrics"]["max_day"] == 5, f

    def crash(v):
        def alive():
            raise RuntimeError("boom")
        v.alive = alive
    exc, f = run_exit(crash)
    assert isinstance(exc, RuntimeError) and f["exit"] == "crash"

    def interrupted(v):
        # The signal lands once, mid-run; the summary written afterwards must still succeed.
        fired = []

        def alive():
            if not fired:
                fired.append(1)
                raise SystemExit(143)
            return True
        v.alive = alive
    exc, f = run_exit(interrupted)
    assert isinstance(exc, SystemExit) and f["exit"] == "signal"


def test_final_json_survives_a_real_sigterm():
    """Python's default SIGTERM does not run finally blocks; main() installs a handler that does."""
    child = textwrap.dedent(f"""
        import sys, time
        sys.path.insert(0, {str(TOOLS)!r})
        import signal, oa_victory, test_oa_learn as t
        from pathlib import Path
        out = Path(sys.argv[1])
        v = t.make_victory(sys.argv[1])
        v.out = out / "out"
        v.start = lambda: None
        v.game = t.MagicMock()
        v.alive = lambda: True
        v.city_turn = lambda: time.sleep(0.2)
        signal.signal(signal.SIGTERM, oa_victory._on_sigterm)
        print("READY", flush=True)
        v.run(1.0)
    """)
    with TemporaryDirectory() as tmp:
        proc = subprocess.Popen([sys.executable, "-c", child, tmp], stdout=subprocess.PIPE,
                                text=True)
        assert proc.stdout.readline().strip() == "READY"
        proc.send_signal(signal.SIGTERM)
        proc.wait(timeout=30)
        final = json.loads((Path(tmp) / "out" / "final.json").read_text())
        assert final["exit"] == "signal", final


def test_next_campaign_keeps_the_finished_campaign():
    with TemporaryDirectory() as tmp, patch.object(oa_victory.time, "sleep"):
        v = make_victory(tmp)
        v.game = MagicMock()
        v.start = lambda: None
        v.progress = {"battles": 3, "wins": 2, "ufos_down": 1, "recoveries": 0, "restarts": 0,
                      "ended": "defeat"}
        v.metrics = {"max_day": 30}
        assert v.next_campaign("defeat")
        assert v.finished[0]["battles"] == 3 and v.finished[0]["metrics"]["max_day"] == 30
        assert v.progress["battles"] == 0 and v.metrics == {}


# ---------------------------------------------------------------------------
# Reward
# ---------------------------------------------------------------------------

def summary(**kw):
    s = {"exit": "time_budget", "campaign_ended": None,
         "metrics": {"max_day": 30, "score_total": 200, "research_complete": 10,
                     "research_complete_start": 4, "funding_terminated": False},
         "progress": {"wins": 5, "ufos_down": 6, "recoveries": 3, "infil_raids": 2,
                      "milestones": []}}
    for k, v in kw.items():
        if k in ("metrics", "progress"):
            s[k] = {**s[k], **v}
        else:
            s[k] = v
    return s


def test_reward_components_and_validity():
    r = oa_learn.reward(summary())
    assert r["valid"] and r["breakdown"]["survival"] == 8.0 * 30
    assert r["breakdown"]["research"] == 12.0 * 6, "starting tech is not the campaign's work"
    assert r["breakdown"]["alive_at_end"] == 40.0 and "defeat" not in r["breakdown"]
    assert abs(r["reward"] - sum(r["breakdown"].values())) < 1e-6
    # Invalid runs carry no reward and say why.
    for bad in (None, {"exit": "start_failed"}, {"exit": "crash", "metrics": {}}):
        r = oa_learn.reward(bad)
        assert not r["valid"] and r["reward"] is None and r["why"]
    # Funding cut at day 12: survival stops counting there, however long the husk limped on.
    cut = oa_learn.reward(summary(metrics={"max_day": 60, "funding_lost_day": 12,
                                           "funding_terminated": True}))
    assert cut["breakdown"]["survival"] == 8.0 * 12 and "alive_at_end" not in cut["breakdown"]
    lost = oa_learn.reward(summary(exit="defeat", campaign_ended="defeat"))
    assert lost["breakdown"]["defeat"] < 0 and "alive_at_end" not in lost["breakdown"]


def test_reward_is_monotone_and_victory_dominates():
    base = oa_learn.reward(summary())["reward"]
    for change in ({"metrics": {"max_day": 60}}, {"metrics": {"research_complete": 20}},
                   {"progress": {"wins": 20}}, {"progress": {"ufos_down": 30}},
                   {"progress": {"milestones": ["crossed_to_alien_dimension"]}},
                   {"progress": {"alien_buildings_taken": 2}}):
        assert oa_learn.reward(summary(**change))["reward"] > base, change
    # The best a campaign can possibly do WITHOUT winning: every term at its cap.
    maxed = summary(metrics={"max_day": 400, "score_total": 10 ** 6, "research_complete": 999,
                             "bases": 2},
                    progress={"wins": 999, "ufos_down": 999, "recoveries": 999, "infil_raids": 999,
                              "alien_buildings_taken": 999, "bases": 2,
                              "milestones": ["advanced_workshop_built", "dimension_shifter_started",
                                             "crossed_to_alien_dimension"]})
    best_loss = oa_learn.reward(maxed)["reward"]
    # The WORST victory: slowest allowed, nothing else achieved.
    bare_win = oa_learn.reward(summary(exit="victory", campaign_ended="victory",
                                       metrics={"max_day": 400, "score_total": 0,
                                                "research_complete": 4},
                                       progress={"wins": 0, "ufos_down": 0, "recoveries": 0,
                                                 "infil_raids": 0}))["reward"]
    assert bare_win > best_loss, (bare_win, best_loss)
    fast = oa_learn.reward(summary(exit="victory", campaign_ended="victory",
                                   metrics={"max_day": 100}))["reward"]
    slow = oa_learn.reward(summary(exit="victory", campaign_ended="victory",
                                   metrics={"max_day": 300}))["reward"]
    assert fast > slow, "winning sooner ranks higher"


# ---------------------------------------------------------------------------
# Learner on a synthetic landscape with a known optimum
# ---------------------------------------------------------------------------

BONUS = {"air_patrol": {12: 300, 6: -100}, "tactical_ai": {"veteran": 200},
         "research_order": {"victory_first": 250, "weapons_first": -150},
         "infil_speed": {5: 100, 4: 40}, "min_squad": {3: -200},
         "behaviour": {"aggressive": 150, "evasive": -100}}


def true_value(genes: dict) -> float:
    c = Strategy(genes).canonical()
    return 700.0 + sum(b.get(c[g], 0.0) for g, b in BONUS.items())


OPTIMUM = {"air_patrol": 12, "tactical_ai": "veteran", "research_order": "victory_first",
           "infil_speed": 5, "behaviour": "aggressive"}


def synthetic_final(genes: dict, seed: int) -> dict:
    value = true_value(genes) + random.Random(seed).gauss(0, 80)
    day = max(1, min(365, round(value / 8)))
    return {"exit": "time_budget", "campaign_ended": None, "wall_seconds": 1.0,
            "metrics": {"max_day": day, "score_total": 0, "research_complete": 0,
                        "research_complete_start": 0, "funding_terminated": False},
            "progress": {}}


class FakeHandle:
    live = 0
    peak = 0

    def __init__(self, spec, finish_after, write=True):
        self.spec, self.polls, self.finish_after = spec, 0, finish_after
        self.deadline = float("inf")
        self.stopped = False
        FakeHandle.live += 1
        FakeHandle.peak = max(FakeHandle.peak, FakeHandle.live)
        if write:
            out = Path(spec["out_dir"])
            out.mkdir(parents=True, exist_ok=True)
            (out / "final.json").write_text(json.dumps(
                synthetic_final(spec["genes"], spec["seed"])))

    def poll(self):
        self.polls += 1
        return 0 if self.polls >= self.finish_after else None

    def stop(self):
        self.stopped = True
        self.close()

    def close(self):
        FakeHandle.live = max(0, FakeHandle.live - 1)


def fake_launcher(finish_after=1, write=True, record=None):
    def launch(spec):
        if record is not None:
            record.append(dict(spec))
        return FakeHandle(spec, finish_after, write)
    return launch


def learn(root, gens, n=4, seed=1, parallel=None, **kw):
    msgs = []
    # Pure learner tests do not bind host sockets. The real allocator is tested separately.
    def fake_port(excluded):
        return next(p for p in range(18500, 19000) if p not in excluded)
    with patch.object(oa_learn, "pick_port", side_effect=fake_port):
        model = oa_learn.run_learning(
            oa_learn.Store(root), generations=gens, n=n, seed=seed,
        launch=kw.pop("launch", fake_launcher()), parallel=parallel or n, say=msgs.append,
        poll_s=0.0, **kw)
    return model, msgs


def test_learner_beats_random_search_on_a_known_landscape():
    wins = 0
    for seed in range(1, 7):
        with TemporaryDirectory() as tmp:
            learn(Path(tmp), gens=12, n=4, seed=seed)
            hist = oa_learn.Store(Path(tmp)).history()
        # What the learner PROPOSED late on, judged by the noise-free landscape.
        late = [true_value(r["genes"]) for r in hist if r["gen"] >= 8 and r["kind"] != "baseline"]
        # Control arm: the same number of runs from the sampler with NO model (empty history).
        rng = random.Random(f"control:{seed}")
        control = [true_value(oa_learn.thompson_sample([], rng).canonical())
                   for _ in range(len(late))]
        late_m, control_m = sum(late) / len(late), sum(control) / len(control)
        assert late_m > control_m + 50, (seed, late_m, control_m)
        wins += 1
    assert wins == 6


def test_learner_champion_is_better_than_defaults_and_was_reevaluated():
    with TemporaryDirectory() as tmp:
        model, _ = learn(Path(tmp), gens=12, n=4, seed=3)
        hist = oa_learn.Store(Path(tmp)).history()
    champ, base = model["champion"], model["baseline"]
    assert champ is not None and base is not None
    assert champ["n"] >= 2, "a champion must have been confirmed on a second seed"
    seeds = [r["seed"] for r in hist if r["key"] == champ["key"]]
    assert len(seeds) == len(set(seeds)) >= 2, "re-evaluations run on NEW seeds"
    assert champ["mean"] > base["mean"] + 100, (champ, base)
    assert true_value(champ["genes"]) > true_value(DEFAULTS) + 150
    assert base["n"] >= 2, "defaults are re-measured as the control arm"
    assert [h["key"] for h in model["hall_of_fame"]][-1] == champ["key"]


def test_learner_is_deterministic_and_resume_equals_straight_through():
    def digest(root):
        return [(r["run_id"], r["seed"], r["key"], r["reward"], r["kind"], r["gen"])
                for r in oa_learn.Store(Path(root)).history()]

    with TemporaryDirectory() as a, TemporaryDirectory() as b, TemporaryDirectory() as c:
        learn(Path(a), gens=4, seed=7)
        learn(Path(b), gens=4, seed=7)
        assert digest(a) == digest(b), "same seed must reproduce the same learning run"
        learn(Path(c), gens=2, seed=7)
        learn(Path(c), gens=2, seed=7, resume=True)
        assert digest(c) == digest(a), "2 generations + resume + 2 must equal 4 straight"
        # Another learner seed explores differently.
        with TemporaryDirectory() as d:
            learn(Path(d), gens=4, seed=8)
            assert digest(d) != digest(a)


def test_seeds_are_distinct_and_nonzero_and_ports_do_not_collide():
    with TemporaryDirectory() as tmp:
        specs = []
        learn(Path(tmp), gens=5, n=4, seed=2, launch=fake_launcher(record=specs))
        seeds = [s["seed"] for s in specs]
        assert len(seeds) == 20 and len(set(seeds)) == 20 and 0 not in seeds
        for gen in range(5):
            ports = [s["port"] for s in specs if s["gen"] == gen]
            assert len(set(ports)) == len(ports) and all(18500 <= p <= 18999 for p in ports)
            assert sorted(s["slot"] for s in specs if s["gen"] == gen) == [0, 1, 2, 3]


def test_generation_zero_runs_defaults_as_the_control_arm():
    with TemporaryDirectory() as tmp:
        specs = []
        learn(Path(tmp), gens=1, n=3, seed=5, launch=fake_launcher(record=specs))
    kinds = [s["kind"] for s in specs]
    assert kinds[0] == "baseline" and Strategy(specs[0]["genes"]).is_default()
    assert len({Strategy(s["genes"]).key() for s in specs}) == 3


def test_parallel_limit_is_respected():
    for parallel in (1, 2, 4):
        FakeHandle.live = FakeHandle.peak = 0
        with TemporaryDirectory() as tmp:
            learn(Path(tmp), gens=1, n=4, seed=1, parallel=parallel,
                  launch=fake_launcher(finish_after=3))
        assert FakeHandle.peak == parallel, (parallel, FakeHandle.peak)


def test_invalid_runs_are_recorded_but_not_learned_from():
    with TemporaryDirectory() as tmp:
        learn(Path(tmp), gens=1, n=2, seed=4, launch=fake_launcher(write=False))
        hist = oa_learn.Store(Path(tmp)).history()
        assert len(hist) == 2 and not any(r["valid"] for r in hist)
        assert all(r["reward"] is None and r["why"] for r in hist)
        assert oa_learn.genome_table(hist) == {}
        # With no usable evidence the next generation still proposes (defaults first, again).
        props = oa_learn.propose(hist, 2, 1, 4)
        assert props[0].kind == "baseline"
        model = oa_learn.Store(Path(tmp)).model()
        assert model["runs_valid"] == 0 and model["champion"] is None


def test_wall_clock_limit_stops_a_run_and_records_it():
    handles = []

    def launch(spec):
        h = FakeHandle(spec, finish_after=10 ** 9, write=False)
        h.deadline = 0.0                      # already past
        handles.append(h)
        return h
    with TemporaryDirectory() as tmp:
        learn(Path(tmp), gens=1, n=2, seed=4, launch=launch)
        hist = oa_learn.Store(Path(tmp)).history()
    assert all(h.stopped for h in handles)
    assert all(not r["valid"] and r["note"] == "timed out" for r in hist)


def test_resume_reruns_unfinished_runs_with_the_same_seeds_in_fresh_dirs():
    with TemporaryDirectory() as tmp:
        root = Path(tmp)
        first = []

        def interrupting(spec):
            first.append(dict(spec))
            if len(first) == 3:
                raise KeyboardInterrupt
            return FakeHandle(spec, 1)
        try:
            learn(root, gens=1, n=4, seed=9, launch=interrupting, parallel=1)
        except KeyboardInterrupt:
            pass
        store = oa_learn.Store(root)
        assert len(store.history()) == 2, "only finished runs have a record"
        planned = {s["run_id"]: s["seed"] for s in store.last_plan()[1]}
        again = []
        learn(root, gens=1, n=4, seed=9, resume=True, launch=fake_launcher(record=again))
        redone = [s for s in again if s["gen"] == 0]
        assert [s["seed"] for s in redone] == [planned[s["run_id"]] for s in redone]
        assert len(redone) == 2 and all(".retry" in s["out_dir"] for s in redone)
        ids = [r["run_id"] for r in oa_learn.Store(root).history() if r["gen"] == 0]
        assert sorted(ids) == sorted(planned), "every planned run ends with exactly one record"


def test_fresh_start_refuses_to_mix_with_existing_history():
    with TemporaryDirectory() as tmp:
        learn(Path(tmp), gens=1, n=2, seed=1)
        try:
            learn(Path(tmp), gens=1, n=2, seed=1)
        except SystemExit as exc:
            assert "--resume" in str(exc)
        else:
            raise AssertionError("a second non-resume run must not append to the same history")


def test_persistence_files_and_leaderboard():
    with TemporaryDirectory() as tmp:
        _, msgs = learn(Path(tmp), gens=3, n=4, seed=6)
        root = Path(tmp)
        assert len((root / "history.jsonl").read_text().splitlines()) == 12
        rec = json.loads((root / "history.jsonl").read_text().splitlines()[0])
        for k in ("run_id", "gen", "seed", "genes", "key", "reward", "breakdown", "exit", "valid"):
            assert k in rec, k
        model = json.loads((root / "model.json").read_text())
        assert model["generation"] == 2 and model["runs_total"] == 12
        assert "air_patrol" in model["gene_means"]
        board = (root / "leaderboard.txt").read_text()
        assert "leaderboard after generation 2" in board and "[defaults]" in board
        assert any("leaderboard after generation 0" in m for m in msgs), "printed every generation"
        assert sorted(p.name for p in (root / "plans").iterdir()) == [
            "gen-0000.json", "gen-0001.json", "gen-0002.json"]


def test_mutation_never_clones_and_leaves_inert_genes_alone():
    rng = random.Random(1)
    parent = Strategy({"air_patrol": 12}).canonical()
    for _ in range(200):
        child = oa_learn.mutate(parent, rng)
        assert child.key() != Strategy(parent).key()
        # behaviour is inert while tactical_ai is None, so it must never appear in a child's key.
        if child["tactical_ai"] is None:
            assert "behaviour" not in child.key()


def test_endgame_defaults_and_manufacture_preference_reach_current_helpers():
    assert DEFAULTS["gate_craft_order"] == "best_unlocked"
    assert DEFAULTS["cross_min_crew"] == 1
    def gs(query):
        if query == "interceptors":
            return {"detail": "-"}
        if query == "facilities":
            return {"base": "FACILITYTYPE_ADVANCED_WORKSHOP:0"}
        if query == "funds":
            return {"balance": "500000"}
        if query.startswith("topic "):
            return {"found": "1", "hidden": "0", "deps_satisfied": "1", "cost": "1000"}
        return {}
    d = SimpleNamespace(h=SimpleNamespace(gs=gs), strategy=Strategy())
    assert oa_play.gate_craft_project(d) == "MANUFACTURE_ANNIHILATOR"
    d.strategy = Strategy({"gate_craft_order": "transport_first"})
    assert oa_play.gate_craft_project(d) == "MANUFACTURE_BIO-TRANSPORT"
    # Engineering uses this explicit request, never the scientific research_order.
    def options(query):
        if query == "research_options":
            return {"type": "engineering", "detail": "0=MANUFACTURE_ANNIHILATOR,done=0,big=0|"
                    "1=MANUFACTURE_BIO-TRANSPORT,done=0,big=0"}
        return gs(query)
    d.h.gs = options
    d.strategy = Strategy({"gate_craft_order": "transport_first", "research_order": "weapons_first"})
    assert oa_play.pick_topic_rows(d) == [(1, "MANUFACTURE_BIO-TRANSPORT")]


def test_crossing_gene_delays_departure_and_allows_survivors_home():
    from test_oa_victory_path import portal_driver
    for minimum, should_cross in ((1, True), (4, False)):
        with TemporaryDirectory() as tmp, patch.object(oa_victory, "goto_portal", return_value=True) as go, \
                patch.object(oa_victory, "crew_transport", return_value=0), \
                patch.object(oa_victory, "set_speed"):
            detail = "0:Bio:flying=1,shifter=1,crew=2,city=CITYMAP_HUMAN,transit=0"
            data = base_data(interceptors={"detail": detail},
                             alien_buildings={"current_city": "CITYMAP_HUMAN", "raidable": "1"})
            v = make_victory(tmp, {"cross_min_crew": minimum}, data)
            assert v.dimension_turn() == should_cross
            assert go.called == should_cross
    d = portal_driver("0:Bio:flying=1,shifter=1,crew=2,city=CITYMAP_HUMAN")
    d.strategy = Strategy({"cross_min_crew": 4})
    assert not oa_play.select_gate_craft(d, "CITYMAP_HUMAN")
    d = portal_driver("0:Bio:flying=1,shifter=1,crew=2,city=CITYMAP_ALIEN")
    d.strategy = Strategy({"cross_min_crew": 4})
    with patch.object(oa_play.time, "sleep"):
        assert oa_play.select_gate_craft(d, "CITYMAP_ALIEN")


def test_empty_lab_still_recruits_above_the_gene_threshold():
    data = base_data(research={"assignable": "2", "assignable_busy": "1", "complete": "7",
                               "labs_detail": "A:built:staff=0:skill=0|B:built:staff=10:skill=5000",
                               "startable": "0"})
    m, _ = turn({"min_lab_skill": 700}, data)
    assert m["hire_scientists"].called and m["hire_engineers"].called
    assert m["assign_research"].called


def test_gate_craft_milestones_are_scored_by_the_learner():
    base = oa_learn.reward(summary())["reward"]
    for milestone in ("gate_craft_manufacture_started", "gate_craft_ready"):
        result = oa_learn.reward(summary(progress={"milestones": [milestone]}))
        assert result["breakdown"]["shifter"] == 100 and result["reward"] == base + 100


def test_launcher_passes_grid_audio_difficulty_seed_and_build_environment():
    with TemporaryDirectory() as tmp, patch.dict(os.environ, {
            "OA_DISPLAY": "2", "OA_BUILD_DIR": "/tmp/alternate-build", "OA_TILE": "old:9"}), \
            patch.object(oa_learn.subprocess, "Popen") as popen, \
            patch.object(oa_play, "reap_stale_game"):
        repo = Path(tmp)
        spec = {"out_dir": str(repo / "run"), "genes": Strategy().canonical(),
                "port": 18501, "seed": 42, "slot": 3}
        run = oa_learn.process_launcher(repo, 1, 5, "4x4", 60)(spec)
        try:
            cmd = popen.call_args.args[0]
            env = popen.call_args.kwargs["env"]
            assert cmd[cmd.index("--difficulty") + 1] == "5"
            assert cmd[cmd.index("--seed") + 1] == "42"
            assert "--no-audio" in cmd and "--single-campaign" in cmd
            assert env["OA_TILE"] == "4x4:3" and env["OA_DISPLAY"] == "2"
            assert env["OA_BUILD_DIR"] == "/tmp/alternate-build"
            with patch.dict(os.environ, env, clear=True):
                game = oa_play.GameProcess(repo, spec["port"], repo / "game.log")
                assert str(game.binary).startswith("/tmp/alternate-build/")
                assert oa_play.screen_args() == ["--Framework.Screen.Display=2",
                                                 "--Framework.Screen.Tile=4x4:3"]
                argv = [oa_victory.__file__, "--no-audio"]
                # The port probe shells out to pgrep, and Popen is mocked above.
                with patch.object(sys, "argv", argv), patch.object(oa_victory, "Victory") as v, \
                        patch.object(oa_victory, "free_port", return_value=17800):
                    v.return_value.run.return_value = 0
                    assert oa_victory.main() == 0
                assert not oa_play.audio_enabled()
        finally:
            run.close()


def test_port_allocator_skips_taken_and_busy_ports_without_host_sockets():
    tried = []
    probe = MagicMock()
    def bind(addr):
        tried.append(addr[1])
        if addr[1] == 18501:
            raise OSError("busy")
    probe.__enter__.return_value = probe
    probe.bind.side_effect = bind
    with patch.object(oa_learn.socket, "socket", return_value=probe), \
            patch.object(oa_learn.os, "getpid", return_value=0):
        assert oa_learn.pick_port({18500}, 18500, 18502) == 18502
    assert tried == [18501, 18502]


def test_parallel_runs_never_share_a_grid_slot():
    active = set()
    class SlottedHandle(FakeHandle):
        def __init__(self, spec):
            assert spec["slot"] not in active
            active.add(spec["slot"])
            super().__init__(spec, finish_after=5 if spec["slot"] == 0 else 1)
        def close(self):
            active.discard(self.spec["slot"])
            super().close()
    with TemporaryDirectory() as tmp:
        learn(Path(tmp), gens=1, n=6, parallel=4, tile_cells=2, launch=SlottedHandle)
    assert not active


if __name__ == "__main__":
    for name, test in list(globals().items()):
        if name.startswith("test_"):
            test()
    print("all campaign-learning tests passed")
