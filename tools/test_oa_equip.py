#!/usr/bin/env python3
"""Equip-pass logic against a fake game: who gets selected, what is sent, what is believed.

The fake models the three things that made the old pixel-driven pass report "handed out 4
weapon(s); armed 10->10": a roster longer than the portrait list can show, soldiers who are
away from the base (their inventory is empty), and a driver that counted attempts, not effects.
"""
from unittest.mock import patch

import oa_play
import oa_victory

FIREARM = ("Megapol_Auto_Cannon", "AEQUIPMENTTYPE_MEGAPOL_AUTO_CANNON")
GRAPPLE = ("Megapol_Stun_Grapple", "AEQUIPMENTTYPE_MEGAPOL_STUN_GRAPPLE")


class FakeGame:
    """Roster + armoury, speaking just enough of the harness protocol."""

    def __init__(self, at_base_armed=10, at_base_unarmed=4, away_unarmed=2, firearms=8,
                 grapples=8, loaded=True, lie=False, refuse=""):
        self.agents = {}
        for i in range(at_base_armed):
            self.agents[f"AGENT_{i:02d}"] = {"base": 1, "weapons": 1}
        for i in range(away_unarmed):
            self.agents[f"AGENT_A{i:02d}"] = {"base": 0, "weapons": 0}
        # Recruits are listed last: the real list is in id order, and new ids sort after old.
        for i in range(at_base_unarmed):
            self.agents[f"AGENT_Z{i:02d}"] = {"base": 1, "weapons": 0}
        self.firearms, self.grapples, self.loaded = firearms, grapples, loaded
        self.lie, self.refuse = lie, refuse
        self.selected = None
        self.sent = []

    # -- model -------------------------------------------------------------------------
    def armed(self):
        return sum(1 for a in self.agents.values() if a["weapons"] > 0)

    def unarmed_at_base(self):
        return sum(1 for a in self.agents.values() if a["base"] and a["weapons"] == 0)

    def items(self):
        if not self.selected or not self.agents[self.selected]["base"]:
            return "-"
        rows = []
        if self.firearms:
            rows.append(f"{FIREARM[0]}:id={FIREARM[1]}:at=149,395:screen=149,395:size=17,16:"
                        f"weapon=1:research=1:loaded={int(self.loaded)}:visible=1:count={self.firearms}")
        if self.grapples:
            rows.append(f"{GRAPPLE[0]}:id={GRAPPLE[1]}:at=170,395:screen=170,395:size=17,16:"
                        f"weapon=1:research=1:loaded=1:visible=1:count={self.grapples}")
        return "|".join(rows) or "-"

    # -- protocol ----------------------------------------------------------------------
    def gs(self, query):
        if query == "agents":
            return {"soldiers": str(len(self.agents)), "armed": str(self.armed()),
                    "unarmed_at_base": str(self.unarmed_at_base())}
        if query == "stores":
            return {"weapons": str(self.firearms + self.grapples)}
        if query == "aequip_agents":
            rows = [f"{i}:{aid}:base={a['base']}:weapons={a['weapons']}:equipment={a['weapons']}:"
                    f"selected={int(aid == self.selected)}:conscious=1"
                    for i, (aid, a) in enumerate(self.agents.items())]
            return {"count": str(len(rows)), "detail": "|".join(rows)}
        if query == "aequip_items":
            return {"count": "0", "detail": self.items()}
        raise AssertionError(f"unexpected gs {query!r}")

    def send(self, line):
        self.sent.append(line)
        words = line.split()
        if words[:2] == ["action", "aequip_select"]:
            if words[2] not in self.agents:
                return f'ERR unknown agent "{words[2]}"'
            self.selected = words[2]
            return f"OK agent={words[2]} mode=Base"
        if words[:2] == ["action", "aequip_equip"]:
            if self.refuse:
                return self.refuse
            agent = self.agents[self.selected]
            if not agent["base"]:
                return "ERR not at a base"
            if words[2] == FIREARM[1] and self.firearms:
                self.firearms -= 1
            elif words[2] == GRAPPLE[1] and self.grapples:
                self.grapples -= 1
            else:
                return f"ERR {words[2]} is not in the inventory list"
            if not self.lie:
                agent["weapons"] += 1
            return f"OK equipped={words[2]} agent={self.selected}"
        raise AssertionError(f"unexpected send {line!r}")

    def ok(self, line):
        reply = self.send(line)
        assert reply.startswith("OK"), reply
        return reply


class FakeHarness:
    def __init__(self, game):
        self.game = game

    def gs(self, query):
        return self.game.gs(query)

    def send(self, line):
        return self.game.send(line)

    def ok(self, line):
        return self.game.ok(line)

    def key(self, name):
        self.game.sent.append(f"key {name}")


class FakeDriver:
    def __init__(self, game):
        self.game = game
        self.h = FakeHarness(game)
        self.events = []

    def say(self, msg):
        self.events.append(msg)


def run_arm(game):
    d = FakeDriver(game)
    with patch.object(oa_play, "_open_equip_screen", return_value=True) as opened, \
            patch.object(oa_play, "return_to_city", return_value=True), \
            patch.object(oa_play.time, "sleep"):
        delta = oa_play.arm_agents_directly(d, agents=24)
    return d, delta, opened


def selected_ids(game):
    return [l.split()[2] for l in game.sent if l.startswith("action aequip_select")]


def test_arms_every_unarmed_soldier_at_base_even_beyond_the_visible_rows():
    # 16 soldiers; the portrait list shows eight, and the four recruits are the last four rows.
    game = FakeGame()
    d, delta, _ = run_arm(game)
    assert delta == 4, d.events
    assert game.armed() == 14 and game.unarmed_at_base() == 0
    assert selected_ids(game) == [f"AGENT_Z{i:02d}" for i in range(4)], \
        "only the unarmed soldiers at a base are selected -- not the veterans, not the absent"
    assert not any("armed 10->10" in e for e in d.events), d.events
    assert any("armed 4 of 4" in e and "armed 10->14" in e for e in d.events), d.events


def test_gives_each_agent_a_grapple_after_the_gun():
    game = FakeGame(at_base_unarmed=2)
    run_arm(game)
    equips = [l.split()[2] for l in game.sent if l.startswith("action aequip_equip")]
    assert equips == [FIREARM[1], GRAPPLE[1]] * 2, equips


def test_does_not_believe_an_ok_that_changed_nothing():
    # aequip_equip says OK and the game state does not move: that is a failure, not a success.
    game = FakeGame(lie=True)
    d, delta, _ = run_arm(game)
    assert delta == 0
    assert any("OK from aequip_equip but armed stayed 10" in e for e in d.events), d.events
    assert not any("armed 4 of 4" in e for e in d.events), d.events


def test_reports_why_the_screen_refused():
    game = FakeGame(refuse="ERR AEQUIPMENTTYPE_MEGAPOL_AUTO_CANNON does not fit any free slot")
    d, delta, _ = run_arm(game)
    assert delta == 0
    summary = d.events[-1]
    assert "armed 0 of 4" in summary and "does not fit any free slot" in summary, summary


def test_unarmed_soldiers_who_are_away_do_not_open_the_screen():
    game = FakeGame(at_base_unarmed=0)
    d, delta, opened = run_arm(game)
    assert delta == 0
    opened.assert_not_called()
    assert game.sent == []
    assert any("nobody unarmed is at a base (2 soldier(s) unarmed but away)" in e
               for e in d.events), d.events


def test_empty_armoury_does_not_open_the_screen():
    game = FakeGame(firearms=0, grapples=0)
    d, delta, opened = run_arm(game)
    assert delta == 0
    opened.assert_not_called()
    assert any("no weapons in stores" in e for e in d.events), d.events


def test_prefers_a_gun_the_base_can_load():
    items = oa_play._parse_rows(
        "Lawpistol:id=AEQUIPMENTTYPE_LAWPISTOL:weapon=1:research=1:loaded=0:visible=1|"
        "M4000:id=AEQUIPMENTTYPE_M4000:weapon=1:research=1:loaded=1:visible=1|"
        "Grapple:id=AEQUIPMENTTYPE_MEGAPOL_STUN_GRAPPLE:weapon=1:research=1:loaded=1", ("name",))
    assert oa_play._pick_firearm(items)["id"] == "AEQUIPMENTTYPE_M4000"
    assert oa_play._pick_grapple(items)["id"] == "AEQUIPMENTTYPE_MEGAPOL_STUN_GRAPPLE"
    only_unloaded = items[:1]
    assert oa_play._pick_firearm(only_unloaded)["id"] == "AEQUIPMENTTYPE_LAWPISTOL"
    unresearched = oa_play._parse_rows("Alien:id=A:weapon=1:research=0:loaded=1", ("name",))
    assert oa_play._pick_firearm(unresearched) is None


def test_template_pass_selects_by_id_and_skips_veterans_and_absentees():
    game = FakeGame()
    d = FakeDriver(game)

    def press(name):
        game.sent.append(f"key {name}")
        if name == "1" and game.selected and game.agents[game.selected]["base"]:
            game.agents[game.selected]["weapons"] = 1

    d.h.key = press
    d.h.gs = lambda q: ({"detail": "1:items=14,weapons=1,types=X"} if q == "templates"
                        else game.gs(q) | ({"weapons": "8"} if q == "stores" else {}))
    with patch.object(oa_play, "_open_equip_screen", return_value=True), \
            patch.object(oa_play, "return_to_city", return_value=True), \
            patch.object(oa_play.time, "sleep"):
        delta = oa_play.equip_squad(d, agents=24)
    assert delta == 4, d.events
    assert selected_ids(game) == [f"AGENT_Z{i:02d}" for i in range(4)]
    assert not any(l.startswith("control AGENT_SELECT_BOX") for l in game.sent)


def test_arm_squad_falls_back_to_direct_when_the_template_arms_nobody():
    d = FakeDriver(FakeGame())
    with patch.object(oa_play, "template_weapon_in_stock", return_value=True), \
            patch.object(oa_play, "equip_squad", return_value=0) as template, \
            patch.object(oa_play, "arm_agents_directly", return_value=3) as direct:
        assert oa_play.arm_squad(d) == 3
    template.assert_called_once()
    direct.assert_called_once()


def test_arm_squad_is_a_no_op_when_everyone_at_base_is_armed():
    d = FakeDriver(FakeGame(at_base_unarmed=0))
    with patch.object(oa_play, "equip_squad") as template, \
            patch.object(oa_play, "arm_agents_directly") as direct:
        assert oa_play.arm_squad(d) == 0
    template.assert_not_called()
    direct.assert_not_called()


def test_arming_is_reachable_from_both_runner_loops():
    """arm_squad was written, documented and reachable from nowhere in oa_play's own loop."""
    import inspect
    assert "arm_squad(" in inspect.getsource(oa_play.play_campaign)
    assert "arm_squad(" in inspect.getsource(oa_victory.Victory)


def test_unarmed_count_falls_back_for_an_older_game():
    assert oa_play.unarmed_at_base({"soldiers": "14", "armed": "10"}) == 4
    assert oa_play.unarmed_at_base({"soldiers": "14", "armed": "10", "unarmed_at_base": "1"}) == 1


if __name__ == "__main__":
    for name, test in list(globals().items()):
        if name.startswith("test_"):
            test()
    print("all equip tests passed")
