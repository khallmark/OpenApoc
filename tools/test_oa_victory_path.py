#!/usr/bin/env python3
"""Campaign gate chain decisions with fake harness replies; no game or state writes."""
from contextlib import ExitStack
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace
from unittest.mock import Mock, patch

import oa_play
import oa_victory


class ResearchDriver:
    def __init__(self, project="-", accepted=True):
        self.stage = "CityView"
        self.project = project
        self.accepted = accepted
        self.selected = None
        self.new_projects = 0
        self.h = Mock()
        self.h.gs.side_effect = self.gs
        self.h.send.side_effect = self.send
        self.h.control.side_effect = self.control
        self.say = Mock()
        self.shot = Mock()

    def status(self):
        return oa_play.Status(self.stage, 1280, 720, "")

    def wait_for(self, stage, *args):
        assert self.stage == stage
        return self.status()

    def gs(self, query):
        if query == "research_options":
            return {"lab": "LAB_1", "type": "engineering", "current": self.project,
                    "detail": "0=MANUFACTURE_BIO-TRANSPORT,done=0,big=0,running=0,affordable=1"}
        if query == "research":
            return {"labs_detail": "LAB_1:engineering:built:staff=2:skill=900:size=large:" +
                    ("idle" if self.project == "-" else self.project + "(100/35000)"),
                    "assignable": "1", "labs_busy": str(int(self.project != "-")),
                    "assignable_busy": str(int(self.project != "-"))}
        if query == "facilities":
            return {"base": "FACILITYTYPE_ADVANCED_WORKSHOP:0"}
        if query == "funds":
            return {"balance": "25000"}
        if query.startswith("topic "):
            return {"found": "1", "hidden": "0", "deps_satisfied":
                    str(int(query.endswith("BIO-TRANSPORT"))), "cost": "12000"}
        return {}

    def send(self, command):
        if "LABS set" in command:
            return "OK" if command == "control LIST_LARGE_LABS set 0" else "ERR end of list"
        return "OK"

    def control(self, name, op, value):
        if name in ("LIST_LARGE_LABS", "LIST_SMALL_LABS"):
            if name != "LIST_LARGE_LABS" or value != "0":
                raise oa_play.HarnessError("end of list")
        elif name == "LIST":
            self.selected = int(value)

    def click_id(self, name, st):
        if name == "BUTTON_SHOW_BASE":
            self.stage = "BaseScreen"
        elif name == "BUTTON_BASE_RES_AND_MANUF":
            self.stage = "ResearchScreen"
        elif name == "BUTTON_RESEARCH_NEWPROJECT":
            self.new_projects += 1
            self.stage = "ResearchSelect"
        elif name == "BUTTON_OK":
            if self.stage == "ResearchSelect":
                if self.accepted:
                    self.project = "MANUFACTURE_BIO-TRANSPORT"
                self.stage = "ResearchScreen"
            else:
                self.stage = "CityView"
        return True


def test_research_priorities_gate_craft_before_weapons_and_buildings_in_order():
    p = oa_play.PRIORITY_RESEARCH
    chain = ["DIMENSION_PROBE", "UFO_TYPE_3", "BIO-TRANSPORT", "UFO_TYPE_5", "EXPLORER",
             "UFO_TYPE_6", "RETALIATOR", "UFO_TYPE_9", "ANNIHILATOR"]
    indices = [p.index("RESEARCH_" + name) for name in chain]
    assert indices == sorted(indices)
    assert p.index("RESEARCH_ADVANCED_QUANTUM_PHYSICS_LAB") < min(indices)
    assert p.index("RESEARCH_ADVANCED_WORKSHOP") < min(indices)
    assert max(indices) < p.index("RESEARCH_DISRUPTOR_GUN")
    assert [t for t in p if t.startswith("RESEARCH_ALIEN_BUILDING_")] == [
        f"RESEARCH_ALIEN_BUILDING_{n}" for n in range(10)]
    assert len(p) == len(set(p))


def test_workshops_only_select_explicit_gate_projects_or_stay_idle():
    d = ResearchDriver()
    original = d.gs
    def gs(query):
        if query == "research_options":
            return {"type": "engineering", "current": "-", "detail":
                    "0=MANUFACTURE_DIMENSION_SHIFTER,done=0,big=0|"
                    "1=MANUFACTURE_DISRUPTOR_GUN,done=0,big=0|"
                    "2=MANUFACTURE_BIO-TRANSPORT,done=0,big=0"}
        return original(query)
    d.h.gs.side_effect = gs
    assert oa_play.pick_topic_rows(d) == [(2, "MANUFACTURE_BIO-TRANSPORT")]
    with patch.object(oa_play, "gate_craft_project", return_value=""):
        assert oa_play.pick_topic_rows(d) == []
    with patch.object(oa_play, "return_to_city"), patch.object(oa_play.time, "sleep"):
        assert oa_play.assign_research(d)
    assert d.selected == 2
    assert d.new_projects == 1


def test_gate_manufacture_preflight_and_best_available():
    d = ResearchDriver()
    assert oa_play.gate_craft_project(d) == "MANUFACTURE_BIO-TRANSPORT"
    original = d.gs
    for query, reply in (
            ("interceptors", {"detail": "0:Bio:flying=1,shifter=1,crew=0"}),
            ("research", {"labs_detail": "LAB_1:engineering:built:MANUFACTURE_EXPLORER(10/55000)"}),
            ("facilities", {"base": "FACILITYTYPE_ADVANCED_WORKSHOP:3"}),
            ("funds", {"balance": "11999"}),
            ("topic MANUFACTURE_BIO-TRANSPORT", {"found": "1", "hidden": "1", "deps_satisfied": "1"}),
            ("topic MANUFACTURE_BIO-TRANSPORT", {"found": "1", "hidden": "0", "deps_satisfied": "0"})):
        d.h.gs.side_effect = lambda q, query=query, reply=reply: reply if q == query else original(q)
        assert oa_play.gate_craft_project(d) == "", query
    def all_available(q):
        if q.startswith("topic "):
            return {"found": "1", "hidden": "0", "deps_satisfied": "1", "cost": "12000"}
        return original(q)
    d.h.gs.side_effect = all_available
    assert oa_play.gate_craft_project(d) == "MANUFACTURE_ANNIHILATOR"


def test_manufacture_never_restarts_or_discards_a_busy_project():
    for project in ("MANUFACTURE_BIO-TRANSPORT", "MANUFACTURE_DISRUPTOR_ARMOR"):
        d = ResearchDriver(project)
        with patch.object(oa_play.time, "sleep"):
            assert not oa_play.manufacture(d)
        assert d.new_projects == 0
        assert d.project == project
    for accepted in (True, False):
        d = ResearchDriver(accepted=accepted)
        with patch.object(oa_play.time, "sleep"):
            assert oa_play.manufacture(d) is accepted
        assert d.new_projects == 1
        if accepted:
            d.h.control.assert_any_call("MANUFACTURE_QUANTITY_SLIDER", "set", "1")


def test_manufacture_filters_small_labs_running_projects_and_unaffordable_rows():
    for flag in ("big=1", "running=1", "affordable=0"):
        d = ResearchDriver()
        original = d.gs
        def gs(query):
            result = original(query)
            if query == "research_options":
                result["detail"] = "0=MANUFACTURE_BIO-TRANSPORT,done=0,big=0,running=0,affordable=1," + flag
            return result
        d.h.gs.side_effect = gs
        with patch.object(oa_play.time, "sleep"):
            assert not oa_play.manufacture(d)
        assert d.new_projects == 0


def test_unreadable_current_project_never_looks_idle():
    d = Mock()
    for reply in ("ERR missing control", "OK missing-text-field"):
        d.h.send.return_value = reply
        assert oa_play.current_project(d)
    d.h.send.return_value = "OK TEXT_CURRENT_PROJECT text=No_Project"
    assert oa_play.current_project(d) == ""


def test_facility_preflight_counts_pending_construction_and_checks_funds():
    want = "FACILITYTYPE_ADVANCED_QUANTUM_PHYSICS_LAB"
    for base, funds in ((want + ":8", "100000"), (want + ":0", "100000"), ("-", "24999")):
        d = portal_driver("")
        d.h.gs.side_effect = lambda q: {"facilities": {"base": base, "offer": "0=" + want,
                                                      "costs": want + "=25000"},
                                       "funds": {"balance": funds}}.get(q, {})
        assert not oa_play.build_facility(d, want)
        d.click_id.assert_not_called()


def portal_driver(detail):
    d = Mock()
    d.status.return_value = oa_play.Status("CityView", 1280, 720, "")
    d.click_id.return_value = True
    d.h.gs.side_effect = lambda q: {
        "interceptors": {"detail": detail},
        "selected": {"with_soldier": "6", "selected": "1"},
        "alien_buildings": {"current_city": "CITYMAP_HUMAN"},
        "centre_on_portal": {"centred": "1", "at": "400,300,0"},
        "owned_craft_rows": {"detail": "0=120,650,1|1=156,650,1|2=192,650,1"},
    }.get(q, {})
    return d


def test_portal_selects_crewed_gate_craft_and_waits_for_view():
    detail = ("0:Valk:flying=1,shifter=0,crew=6,city=CITYMAP_HUMAN|"
              "1:Bio:flying=1,shifter=1,crew=0,city=CITYMAP_HUMAN|"
              "2:Explorer:flying=1,shifter=1,crew=6,city=CITYMAP_HUMAN,transit=0,ui=2")
    d = portal_driver(detail)
    with patch.object(oa_play.time, "sleep"), \
            patch.object(oa_play, "wait_for_dimension", return_value=False) as wait:
        assert not oa_play.goto_portal(d)  # Order accepted is not an arrival receipt.
    assert [(c.args, c.kwargs) for c in d.h.click_xy.call_args_list] == [
        ((192, 650), {"button": "right"}), ((192, 650), {})]
    d.h.ok.assert_called_once_with("click 400 300 right")
    wait.assert_called_once_with(d, "CITYMAP_ALIEN")
    d = portal_driver("0:Bio:flying=1,shifter=1,crew=0")
    with patch.object(oa_play.time, "sleep"):
        assert not oa_play.goto_portal(d)
    d.h.click_xy.assert_not_called()
    d.h.ok.assert_not_called()


def test_portal_refuses_to_order_a_group_and_can_return_an_empty_gate_craft():
    d = portal_driver("0:Bio:flying=1,shifter=1,crew=6,city=CITYMAP_HUMAN")
    original = d.h.gs.side_effect
    d.h.gs.side_effect = lambda q: {"selected": "2", "with_soldier": "6"} if q == "selected" else original(q)
    with patch.object(oa_play.time, "sleep"):
        assert not oa_play.goto_portal(d)
    d.h.ok.assert_not_called()
    d = portal_driver("0:Bio:flying=1,shifter=1,crew=0,city=CITYMAP_ALIEN")
    original = d.h.gs.side_effect
    def gs(q):
        if q == "alien_buildings":
            return {"current_city": "CITYMAP_ALIEN"}
        if q == "selected":
            return {"selected": "1", "with_soldier": "0"}
        return original(q)
    d.h.gs.side_effect = gs
    with patch.object(oa_play.time, "sleep"), patch.object(oa_play, "wait_for_dimension", return_value=True):
        assert oa_play.goto_portal(d, "CITYMAP_HUMAN")
    d.h.ok.assert_called_once_with("click 400 300 right")


def test_portal_does_not_reorder_a_craft_already_across_or_in_transit():
    for flags in ("city=CITYMAP_ALIEN,transit=0", "city=CITYMAP_HUMAN,transit=1",
                  "city=CITYMAP_HUMAN,transit=0,portal=1"):
        d = portal_driver("0:Bio:flying=1,shifter=1,crew=6," + flags)
        with patch.object(oa_play, "wait_for_dimension", return_value=True) as wait:
            assert oa_play.goto_portal(d)
        wait.assert_called_once_with(d, "CITYMAP_ALIEN")
        d.h.click_xy.assert_not_called()
        d.h.ok.assert_not_called()


def test_portal_uses_stable_craft_id_and_scrolls_to_its_actual_icon():
    d = portal_driver("8:Bio:flying=1,shifter=1,crew=6,city=CITYMAP_HUMAN,id=VEHICLE_42")
    original = d.h.gs.side_effect
    row = ["VEHICLE_42=900,650,0"]
    def gs(query):
        return {"detail": row[0]} if query == "owned_craft_rows" else original(query)
    d.h.gs.side_effect = gs
    d.live_rect.return_value = {"x": 100, "y": 600, "w": 430, "h": 24}
    def send(command):
        if command.endswith(" get"):
            return "OK SCROLL value=0 min=0 max=600"
        row[0] = "VEHICLE_42=315,650,1"
        return "OK"
    d.h.send.side_effect = send
    with patch.object(oa_play.time, "sleep"):
        assert oa_play.select_gate_craft(d, "CITYMAP_HUMAN")
    assert [(c.args, c.kwargs) for c in d.h.click_xy.call_args_list] == [
        ((315, 650), {"button": "right"}), ((315, 650), {})]
    d.h.send.assert_any_call("control OWNED_VEHICLE_LIST_SCROLL set 585")


def test_dimension_wait_observes_rollover_and_yields_to_battles():
    d = portal_driver("")
    d.h.gs.side_effect = [{"current_city": "CITYMAP_HUMAN"},
                          {"current_city": "CITYMAP_HUMAN"},
                          {"current_city": "CITYMAP_ALIEN"}]
    with patch.object(oa_play.time, "sleep"), patch.object(oa_play, "set_speed") as speed:
        assert oa_play.wait_for_dimension(d, "CITYMAP_ALIEN")
    assert d.h.gs.call_count == 3
    assert speed.call_count == 4
    d.status.return_value = oa_play.Status("BattleView", 1280, 720, "")
    assert not oa_play.wait_for_dimension(d, "CITYMAP_ALIEN")


def test_crew_chooses_gate_craft_even_when_an_ordinary_craft_has_the_squad():
    d = Mock()
    stage = [oa_play.Status("CityView", 1280, 720, "")]
    d.status.side_effect = lambda: stage[0]
    boarded = [False]
    def gs(q):
        if q == "centre_on_base":
            return {"centred": "1", "at": "400,300,0"}
        if q == "agents":
            return {"soldiers": "12"}
        if q == "interceptors":
            return {"detail": "0:Valk:flying=1,crew=6,shifter=0|"
                    f"1:Bio:flying=1,crew={6 if boarded[0] else 0},shifter=1"}
        return {}
    d.h.gs.side_effect = gs
    def ok(command):
        if command == "click 400 300 right":
            stage[0] = oa_play.Status("BuildingScreen", 1280, 720, "", detail=
                "selected_agents=0_boarding=300,80,0,6,1,1;300,250,1,12,1,1_"
                "soldier_rows=300,110,1,1,1;300,136,1,1,1;100,80,0,1,0")
        if command == "up 300 250":
            boarded[0] = True
        return "OK"
    d.h.ok.side_effect = ok
    with patch.object(oa_play.time, "sleep"), patch.object(oa_play, "return_to_city"):
        assert oa_play.crew_transport(d) == 2
    d.h.ok.assert_any_call("up 300 250")
    assert boarded[0]
    assert [c.args for c in d.h.click_xy.call_args_list] == [(300, 110), (300, 136)]


def test_crew_waits_for_gate_craft_instead_of_falling_back_to_ordinary_transport():
    d = Mock()
    d.status.side_effect = [oa_play.Status("CityView", 1280, 720, ""),
                           oa_play.Status("BuildingScreen", 1280, 720, ""),
                           oa_play.Status("BuildingScreen", 1280, 720, "", detail=
                               "boarding=300,80,0,6,1,1_soldier_rows=100,80,0,1,0")]
    d.h.gs.side_effect = lambda q: {"interceptors": {"detail": "0:Bio:flying=1,shifter=1,crew=0"},
                                   "centre_on_base": {"centred": "1", "at": "400,300,0"}}.get(q, {})
    with patch.object(oa_play.time, "sleep"), patch.object(oa_play, "return_to_city"):
        assert oa_play.crew_transport(d) == 0
    d.h.click_xy.assert_not_called()


def test_victory_retries_manufacture_and_waits_for_home_after_a_won_raid():
    with TemporaryDirectory() as tmp, ExitStack() as stack:
        v = oa_victory.Victory(Path(tmp), Path(tmp), 1234)
        v.say = Mock()
        v.d = portal_driver("")
        stack.enter_context(patch.object(oa_victory, "gate_craft_project", return_value="MANUFACTURE_BIO-TRANSPORT"))
        manufacture = stack.enter_context(patch.object(oa_victory, "manufacture", side_effect=[False, True]))
        for _ in range(2):
            v.last_endgame = 0
            assert not v.dimension_turn()
        assert manufacture.call_count == 2
        assert v.progress["gate_craft_manufacture_started"] == "MANUFACTURE_BIO-TRANSPORT"
        detail = "0:Bio:flying=1,shifter=1,crew=6,city=CITYMAP_ALIEN,transit=0,home=0"
        v.d.h.gs.side_effect = lambda q: {"alien_buildings": {"current_city": "CITYMAP_ALIEN", "raidable": "2"},
                                        "interceptors": {"detail": detail}}.get(q, {})
        raid = stack.enter_context(patch.object(oa_victory, "raid_alien_building", return_value="resolved"))
        speed = stack.enter_context(patch.object(oa_victory, "set_speed"))
        v.last_endgame = 0
        assert v.dimension_turn()
        assert v.progress["returning_home"]
        assert v.progress["alien_buildings_taken"] == 1
        v.last_endgame = 0
        assert v.dimension_turn()
        assert raid.call_count == 1  # Never interrupt the automatic return with a second raid.
        assert speed.call_count == 2
        detail = detail.replace("CITYMAP_ALIEN", "CITYMAP_HUMAN").replace("home=0", "home=1")
        v.d.h.gs.side_effect = lambda q: {"alien_buildings": {"current_city": "CITYMAP_HUMAN", "raidable": "0"},
                                        "interceptors": {"detail": detail}}.get(q, {})
        assert not v.dimension_turn()
        assert "returning_home" not in v.progress
        assert "squad_returned_home" in v.progress["milestones"]


def test_victory_pending_crossing_blocks_base_work_during_cooldown():
    with TemporaryDirectory() as tmp, patch.object(oa_victory, "set_speed"), \
            patch.object(oa_victory, "goto_portal", return_value=False) as portal:
        v = oa_victory.Victory(Path(tmp), Path(tmp), 1234)
        v.say = Mock()
        v.d = portal_driver("0:Bio:flying=1,shifter=1,crew=6,city=CITYMAP_HUMAN")
        original = v.d.h.gs.side_effect
        v.d.h.gs.side_effect = lambda q: {"current_city": "CITYMAP_HUMAN", "raidable": "1"} if q == "alien_buildings" else original(q)
        assert v.dimension_turn()
        assert v.progress["crossing_to"] == "CITYMAP_ALIEN"
        assert v.dimension_turn()
        assert portal.call_count == 1


def test_victory_return_order_is_retryable_if_not_already_in_progress():
    with TemporaryDirectory() as tmp, patch.object(oa_victory, "set_speed"), \
            patch.object(oa_victory, "goto_portal", return_value=False) as portal:
        v = oa_victory.Victory(Path(tmp), Path(tmp), 1234)
        v.say = Mock()
        v.d = portal_driver("0:Bio:flying=1,shifter=1,crew=6,city=CITYMAP_ALIEN,transit=0,portal=0")
        original = v.d.h.gs.side_effect
        v.d.h.gs.side_effect = lambda q: {"current_city": "CITYMAP_ALIEN", "raidable": "0"} if q == "alien_buildings" else original(q)
        for _ in range(2):
            v.last_endgame = 0
            assert v.dimension_turn()
        assert portal.call_count == 2
        portal.assert_called_with(v.d, "CITYMAP_HUMAN")


def test_alien_raid_delivers_squad_to_target_before_opening_raid_screen():
    d = Mock()
    stage = ["CityView"]
    landed = [False]
    d.status.side_effect = lambda: oa_play.Status(stage[0], 1280, 720, "", detail=
        "selected_agents=6_boarding=300,80,1,12,1,1")
    target = {"centred": "1", "building": "BUILDING_DIMENSION_GATE_GENERATOR", "victory": "1",
              "at": "400,300,0"}
    def gs(query):
        if query == "alien_buildings":
            return {"current_city": "CITYMAP_ALIEN"}
        if query == "centre_on_raidable":
            return target
        if query == "selected":
            return {"building": target["building"] if landed[0] else "-"}
        return {}
    d.h.gs.side_effect = gs
    def ok(command):
        if command == "click 400 300":
            landed[0] = True
        if command == "click 400 300 right":
            assert landed[0]
            stage[0] = "BuildingScreen"
        return "OK"
    d.h.ok.side_effect = ok
    def click(name, st):
        if name == "BUTTON_RAID":
            stage[0] = "BattleBriefing"
        return True
    d.click_id.side_effect = click
    with patch.object(oa_play.time, "sleep"), patch.object(oa_play, "select_gate_craft", return_value=True), \
            patch.object(oa_play, "win_battle", return_value="resolved") as fight:
        assert oa_play.raid_alien_building(d) == "resolved"
    assert [c.args[0] for c in d.h.ok.call_args_list] == ["click 400 300", "click 400 300 right"]
    d.h.click_xy.assert_called_once_with(300, 80)
    fight.assert_called_once_with(d, budget_s=2400)


def test_victory_run_reaches_dimension_turn_through_city_turn():
    with TemporaryDirectory() as tmp, ExitStack() as stack:
        v = oa_victory.Victory(Path(tmp), Path(tmp), 1234)
        v.say = Mock()
        v.d = portal_driver("")
        v.d.status.side_effect = [oa_play.Status("CityView", 1280, 720, ""),
                                 oa_play.Status("VideoScreen", 1280, 720, "", "wingame2.smk"),
                                 oa_play.Status("VideoScreen", 1280, 720, "", "wingame2.smk")]
        stack.enter_context(patch.object(v, "start"))
        stack.enter_context(patch.object(v, "alive", return_value=True))
        stack.enter_context(patch.object(v, "bankrupt", return_value=False))
        stack.enter_context(patch.object(v, "save"))
        stack.enter_context(patch.object(oa_victory.time, "sleep"))
        dimension = stack.enter_context(patch.object(v, "dimension_turn", return_value=True))
        assert v.run(0.1) == 0
        dimension.assert_called_once()


if __name__ == "__main__":
    for name, test in list(globals().items()):
        if name.startswith("test_"):
            test()
    print("all victory path tests passed")
