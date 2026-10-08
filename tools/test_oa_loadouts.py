#!/usr/bin/env python3
"""Role policy, market budget and observed slot verification; no game launch."""
from collections import Counter
from copy import deepcopy
from unittest.mock import patch

import oa_loadouts as policy
import oa_play


def item(ident, kind, **attrs):
    return dict(id=ident, name=ident, kind=kind, slot="Body", armor=0, weight=1, flight=0,
                damage=0, range=0, accuracy=0, fire_ticks=1, capacity=1, recharge=0,
                burst=1, explosive=0, damage_type="-", ammo=[], research=1, usable=1,
                price=100, market=100, store_space=1, stores={"B": 1000}, incoming={},
                modifiers={}, **{}) | attrs


def catalog():
    items = [item("RIFLE", "Weapon", weight=5, ammo=["CLIP"]),
             item("CLIP", "Ammo", capacity=10, damage=30, range=40, accuracy=90,
                  fire_ticks=100, damage_type="DAMAGETYPE_LASER_BEAM"),
             item("FAST", "Weapon", weight=8, ammo=["FAST_CLIP"]),
             item("FAST_CLIP", "Ammo", capacity=20, damage=40, range=30, accuracy=70,
                  fire_ticks=20, damage_type="DAMAGETYPE_ARMOR_PIERCING"),
             item("AEQUIPMENTTYPE_DEVASTATOR_CANNON", "Weapon", weight=8, damage=70,
                  range=50, accuracy=75, fire_ticks=160, capacity=34, recharge=1,
                  damage_type="DAMAGETYPE_DISRUPTOR_BEAM", modifiers={"TERRAIN": 100}),
             item("AEQUIPMENTTYPE_TOXIGUN", "Weapon", weight=5,
                  ammo=["AEQUIPMENTTYPE_TOXIGUN_B-CLIP"]),
             item("AEQUIPMENTTYPE_TOXIGUN_B-CLIP", "Ammo", capacity=16, damage=65,
                  range=33, accuracy=40, fire_ticks=32, damage_type="DAMAGETYPE_TOXIN_B", modifiers={"TERRAIN": 25}),
             item("SCANNER", "MotionScanner"), item("MED", "MediKit"),
             item("SMOKE", "Grenade", damage_type="DAMAGETYPE_SMOKE"),
             item("GRENADE", "Grenade", damage=50, explosive=1, damage_type="DAMAGETYPE_EXPLOSIVE"),
             item("AEQUIPMENTTYPE_VORTEX_MINE", "Grenade", damage=150, explosive=1,
                  damage_type="DAMAGETYPE_EXPLOSIVE")]
    for slot in policy.ARMOR_SLOTS:
        items += [item("LIGHT_" + slot, "Armor", slot=slot, weight=2, armor=30),
                  item("PROTECT_" + slot, "Armor", slot=slot, weight=10, armor=70)]
    items += [item("FLIGHT", "Armor", flight=1, weight=12, armor=35)]
    return {e["id"]: e for e in items}


def roster(n=6):
    return [dict(id=f"A{i}", base="B", home="B", vehicle="V", speed=90-i, strength=30+i,
                 accuracy=70+i, kit=[]) for i in range(n)]


def encode_items(cat):
    return "|".join(e["id"] + ":" + ":".join(
        f"{k}=" + ("+".join(f"{a}~{b}" for a, b in v.items()) or "-" if isinstance(v, dict)
                   else "+".join(v) or "-" if isinstance(v, list) else str(v))
        for k, v in e.items() if k != "id") for e in cat.values())


def encode_roster(agents):
    return "|".join(r["id"] + ":" + ":".join(f"{k}={v}" for k, v in r.items()
                   if k not in ("id", "kit")) + ":kit=" + ("+".join(
                       f"{e['id']}~{e['slot']}~{e['payload']}~{e['rounds']}~{e['armor']}"
                       for e in r["kit"]) or "-") for r in agents)


class FakeDriver:
    def __init__(self, cat=None, agents=None, lie=False, refuse=False):
        self.cat = deepcopy(cat or catalog())
        self.agents = deepcopy(agents or roster())
        self.selected = self.agents[0]
        self.h = self
        self.events, self.sent = [], []
        self.lie, self.refuse = lie, refuse

    def say(self, msg):
        self.events.append(msg)

    def gs(self, query):
        if query == "equipment_catalog":
            return dict(purchase_base="B", difficulty="0", detail=encode_items(self.cat))
        if query == "loadout_agents":
            return dict(selected_vehicle="V", detail=encode_roster(self.agents))
        if query == "funds":
            return dict(balance="100000")
        if query == "aequip_items":
            return dict(detail="|".join(f"{e['name']}:id={i}:count={e['stores'].get('B', 0)}"
                        for i, e in self.cat.items()))
        raise AssertionError(query)

    def send(self, line):
        self.sent.append(line)
        bits = line.split()
        if bits[0] == "control":
            return "OK"
        verb, ident = bits[1:3]
        if verb == "aequip_select":
            self.selected = next(r for r in self.agents if r["id"] == ident)
            return "OK selected"
        if self.refuse:
            return "ERR no free slot"
        if self.lie:
            return "OK equipped"
        kit = self.selected["kit"]
        e = self.cat[ident]
        if verb == "aequip_unequip":
            old = next(k for k in kit if k["id"] == ident)
            kit.remove(old)
            e["stores"]["B"] += old["rounds"] if e["kind"] == "Ammo" else 1
            if old["payload"] != "-":
                self.cat[old["payload"]]["stores"]["B"] += old["rounds"]
        elif verb == "aequip_reload":
            gun = next(k for k in kit if k["id"] == ident)
            ammo = self.cat[bits[3]]
            if gun["payload"] != "-":
                self.cat[gun["payload"]]["stores"]["B"] += gun["rounds"]
            gun.update(payload=ammo["id"], rounds=ammo["capacity"])
            ammo["stores"]["B"] -= ammo["capacity"]
        elif verb == "aequip_equip":
            slot = e["slot"] if e["kind"] == "Armor" else "General"
            if e["kind"] == "Weapon":
                slot = "RightHand" if not any(k["slot"] == "RightHand" for k in kit) else "LeftHand"
            payload, rounds = "-", e["capacity"]
            if e["ammo"]:
                payload = e["ammo"][-1]
                rounds = self.cat[payload]["capacity"]
                self.cat[payload]["stores"]["B"] -= rounds
            e["stores"]["B"] -= rounds if e["kind"] == "Ammo" else 1
            kit.append(dict(id=ident, slot=slot, payload=payload, rounds=rounds, armor=e["armor"]))
        else:
            raise AssertionError(line)
        return "OK"

    def ok(self, line):
        return self.send(line)


def run_pass(d, mission="base_defence"):
    with patch.object(oa_play, "_open_equip_screen", return_value=True), \
            patch.object(oa_play, "return_to_city"), patch.object(oa_play, "buy_named"):
        return policy.prepare_loadouts(d, mission)


def test_roles_follow_squad_size_stats_and_defence():
    for size in range(1, 25):
        assigned = policy.assign_roles(roster(size), "ufo_recovery")
        assert len(assigned) == size and "assault" in assigned.values()
        assert assigned.get("A0") == ("scout" if size > 1 else "assault")
    assert "heavy" not in policy.assign_roles(roster(), "base_defence").values()
    assert set(policy.assign_roles(roster(), "ufo_recovery").values()) == {
        "scout", "support", "heavy", "assault"}


def test_runtime_stats_choose_flight_and_protection_without_sets():
    cat = catalog()
    plans = policy.plan_loadouts(cat, roster(), "ufo_recovery")
    scout = next(p for p in plans if p.role == "scout")
    assault = next(p for p in plans if p.role == "assault")
    assert scout.armor["Body"] == "FLIGHT" and scout.armor["Helmet"] == "LIGHT_Helmet"
    assert scout.weapon == "RIFLE" and scout.extras["SCANNER"] == 1
    assert all(i.startswith("PROTECT_") for i in assault.armor.values())
    assert assault.weapon == "FAST"  # firing efficiency, not raw damage
    cat["LIGHT_Body"]["armor"] = 100  # modded values change the ranking at runtime
    assert next(p for p in policy.plan_loadouts(cat, roster(), "ufo_recovery")
                if p.role == "assault").armor["Body"] == "LIGHT_Body"


def test_dimension_doctrine_is_research_gated_and_carries_more_ammo():
    cat = catalog()
    for e in cat.values():
        if "TOXIGUN" in e["id"]:
            e["research"] = e["usable"] = 0
    locked = policy.catalog_from_reply(dict(detail=encode_items(cat)))
    assert all(p.weapon != "AEQUIPMENTTYPE_TOXIGUN" for p in policy.plan_loadouts(
        locked, roster(), "alien_dimension"))
    assert any("Toxin B" in g for p in policy.plan_loadouts(locked, roster(), "alien_dimension")
               for g in p.gaps)
    normal = policy.plan_loadouts(catalog(), roster(), "ufo_recovery")
    alien = policy.plan_loadouts(catalog(), roster(), "alien_dimension", difficulty=4)
    assert all(p.weapon == "AEQUIPMENTTYPE_TOXIGUN" and p.clips == 7
               for p in alien if p.role != "heavy")
    heavy = next(p for p in alien if p.role == "heavy")
    assert heavy.weapon == "AEQUIPMENTTYPE_DEVASTATOR_CANNON"
    assert heavy.armor["Body"] == "FLIGHT"
    assert all(p.extras["AEQUIPMENTTYPE_VORTEX_MINE"] >= 2 for p in alien)
    assert max(p.clips for p in normal) < max(p.clips for p in alien)


def test_known_enemy_modifiers_and_terrain_affect_rankings():
    cat = catalog()
    cat["CLIP"]["modifiers"] = {"KNOWN_ALIEN": 0}
    cat["FAST_CLIP"]["modifiers"] = {"KNOWN_ALIEN": 200}
    assert policy.weapon_rank(cat["RIFLE"], cat["CLIP"], "assault", "ufo_recovery",
                              ("KNOWN_ALIEN",))[2] == 0
    assert policy.weapon_rank(cat["FAST"], cat["FAST_CLIP"], "assault", "ufo_recovery",
                              ("KNOWN_ALIEN",))[2] > 0
    cat["FAST_CLIP"]["modifiers"]["TERRAIN"] = 0
    assert policy.weapon_rank(cat["FAST"], cat["FAST_CLIP"], "heavy", "ufo_recovery")[1] == 0


def test_order_quantities_round_up_clips_account_for_pending_and_reserve():
    cat = catalog()
    for e in cat.values():
        e["stores"] = {"B": 0, "OTHER_BASE": 10000}
    plan = policy.Loadout("A0", "assault", "ufo_recovery", "B", weapon="RIFLE", ammo="CLIP", clips=3)
    cat["CLIP"]["incoming"] = {"B": 11}
    orders = policy.purchase_orders([plan], roster(1), cat, "B", 41000)
    assert orders == {"RIFLE": 1, "CLIP": 2}
    assert sum(cat[i]["price"] * n for i, n in orders.items()) <= 1000
    assert policy.purchase_orders([plan], roster(1), cat, "B", 40000) == {}
    orders = policy.purchase_orders([plan], roster(1), cat, "B", 40200)
    assert sum(cat[i]["price"] * n for i, n in orders.items()) <= 200
    cat["RIFLE"]["incoming"] = {"B": 1}
    cat["CLIP"]["incoming"] = {"B": 30}
    assert policy.purchase_orders([plan], roster(1), cat, "B", 100000) == {}


def test_refit_verifies_every_slot_and_handles_ok_without_effect():
    d = FakeDriver()
    result = run_pass(d, "alien_dimension")
    assert result["ready"] and result["verified"] == 6 and result["gained"] == 6, d.events
    assert any("[loadout-verified]" in e for e in d.events)
    result = run_pass(d, "alien_dimension")
    assert result["ready"] and result["gained"] == 0, "repeat pass is idempotent"
    for kwargs in ({"lie": True}, {"refuse": True}):
        d = FakeDriver(**kwargs)
        result = run_pass(d)
        assert not result["ready"] and result["verified"] == 0 and result["gained"] == 0
        assert not any("[loadout-verified]" in e for e in d.events)


def test_away_soldiers_are_not_selected_and_wrong_payload_fails_verification():
    d = FakeDriver()
    for r in d.agents:
        r["base"] = "-"
    result = run_pass(d)
    assert not result["ready"] and not any("aequip_select" in s for s in d.sent)
    plan = policy.Loadout("A0", "assault", "ufo_recovery", "B", weapon="RIFLE", ammo="CLIP", clips=1)
    row = roster(1)[0]
    row["kit"] = [dict(id="RIFLE", slot="General", payload="FAST_CLIP", rounds=10, armor=1)]
    assert any("loaded primary" in f for f in policy.verify_loadout(plan, row, catalog()))


def test_selected_vehicle_scope_and_every_crew_member():
    d = FakeDriver(agents=roster(26))
    d.agents[-1]["vehicle"] = "OTHER"
    with patch.object(oa_play, "_open_equip_screen", return_value=True), \
            patch.object(oa_play, "return_to_city"), patch.object(oa_play, "buy_named"):
        result = policy.prepare_loadouts(d, "ufo_recovery", vehicle="selected", agents=2)
    assert result["verified"] == 25 and result["ready"]
    assert not d.agents[-1]["kit"], "unselected crew must stay untouched"
    original = d.gs
    def gs(query):
        reply = original(query)
        if query == "loadout_agents":
            reply["selected_vehicle"] = "-"
        return reply
    d.gs = gs
    assert not policy.prepare_loadouts(d, vehicle="selected")["ready"]


def test_mission_swap_releases_old_ammo_and_verifies_new_payload():
    d = FakeDriver()
    first = run_pass(d, "base_defence")
    assert first["ready"]
    second = run_pass(d, "alien_dimension")
    assert second["ready"], d.events
    for plan in second["plans"]:
        row = next(r for r in d.agents if r["id"] == plan.agent)
        assert not policy.verify_loadout(plan, row, d.cat)
        assert not any(d.cat[e["id"]]["kind"] == "Ammo" and e["id"] != plan.ammo
                       for e in row["kit"])
    assert any("aequip_unequip" in command for command in d.sent)


def test_pending_orders_do_not_claim_readiness_or_strip_veterans():
    d = FakeDriver()
    assert run_pass(d)["ready"]
    old = deepcopy(d.agents[0]["kit"])
    # Dimension kit is researched and inbound, but has not physically arrived.
    for ident in ("AEQUIPMENTTYPE_TOXIGUN", "AEQUIPMENTTYPE_TOXIGUN_B-CLIP"):
        d.cat[ident]["stores"]["B"] = 0
        d.cat[ident]["incoming"]["B"] = 100
    result = run_pass(d, "alien_dimension")
    assert not result["ready"] and result["gained"] == 0
    primary = next(e for e in old if e["slot"] in policy.HANDS)
    assert primary in d.agents[0]["kit"], "working primary stays until replacement arrives"


def test_new_recruits_get_local_fallback_while_ideal_is_inbound():
    d = FakeDriver()
    for ident in ("AEQUIPMENTTYPE_TOXIGUN", "AEQUIPMENTTYPE_TOXIGUN_B-CLIP"):
        d.cat[ident]["stores"]["B"] = 0
        d.cat[ident]["incoming"]["B"] = 100
    result = run_pass(d, "alien_dimension")
    assert not result["ready"] and result["gained"] == 6
    assert all(any(e["slot"] in policy.HANDS and e["rounds"] > 0 for e in r["kit"])
               for r in d.agents)


def test_selected_squad_cannot_budget_another_soldiers_equipped_supply():
    cat = catalog()
    for e in cat.values():
        e["stores"] = {"B": 0}
    agents = roster(2)
    agents[1]["kit"] = [dict(id="RIFLE", slot="RightHand", payload="CLIP", rounds=30, armor=1)]
    plan = policy.Loadout("A0", "assault", "ufo_recovery", "B", weapon="RIFLE", ammo="CLIP", clips=3)
    assert policy.purchase_orders([plan], agents, cat, "B", 100000) == {"RIFLE": 1, "CLIP": 3}


def test_named_buyer_uses_each_quantity_and_cancels_unknown_preview():
    from unittest.mock import Mock
    for preview, commit in (("OK text=41000", True), ("OK text=39999", False), ("ERR absent", False)):
        d = Mock()
        d.h.gs.side_effect = [{"balance": "50000"}, {"balance": "41000" if commit else "50000"}]
        def send(command):
            if command == "controls LIST":
                return "OK LIST 0:ROW:text=Rifle 1:ROW:text=Clip"
            if command == "control TEXT_FUNDS get":
                return preview
            if command.endswith(" get"):
                return "OK value=0 min=-100 max=100"
            return "OK"
        d.h.send.side_effect = send
        with patch.object(oa_play, "open_buysell", return_value=True), \
                patch.object(oa_play, "close_buysell", return_value=True) as close, \
                patch.object(oa_play.time, "sleep"):
            count = oa_play.buy_named(d, {"AEQUIPMENTTYPE_RIFLE": 2, "AEQUIPMENTTYPE_CLIP": 5})
        d.h.send.assert_any_call("control LIST item 0 set -2")
        d.h.send.assert_any_call("control LIST item 1 set -5")
        close.assert_called_once_with(d, commit=commit)
        assert count == (2 if commit else 0)


def test_sell_named_keeps_the_existing_selling_path_working():
    from unittest.mock import Mock
    d = Mock()
    d.h.gs.return_value = {"balance": "50000"}
    d.h.send.side_effect = ["OK LIST 0:ROW:text=Stormdog_1", "OK value=0 max=4", "OK"]
    with patch.object(oa_play, "open_buysell", return_value=True), \
            patch.object(oa_play, "close_buysell") as close, patch.object(oa_play.time, "sleep"):
        assert oa_play.sell_named(d, ["Stormdog"], qty=1) == 1
    d.h.send.assert_any_call("control LIST item 0 set 1")
    close.assert_called_once_with(d, commit=True)


def test_damaged_planned_armor_does_not_verify_until_replaced():
    d = FakeDriver()
    assert run_pass(d)["ready"]
    row = d.agents[0]
    body = next(e for e in row["kit"] if e["slot"] == "Body")
    body["armor"] = 1
    plan = policy.plan_loadouts(d.cat, d.agents, "base_defence")[0]
    assert any("Body expected" in f for f in policy.verify_loadout(plan, row, d.cat))
    assert run_pass(d)["ready"]
    assert next(e for e in row["kit"] if e["slot"] == "Body")["armor"] == d.cat[body["id"]]["armor"]


if __name__ == "__main__":
    for name, test in list(globals().items()):
        if name.startswith("test_"):
            test()
    print("all loadout tests passed")
