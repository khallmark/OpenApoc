#!/usr/bin/env python3
"""Crewing tests that need no game: craft ranking, squad sizing, and a model of the assignment screen.

The model (FakeBase) encodes how the real BuildingScreen behaves, as read from agentassignment.cpp
and Agent::enterVehicle and confirmed live: a drop seats only as many agents as the craft has
seats, reports success regardless, and clears the selection; seated agents leave the unassigned
list and show as extra rows under their craft, which pushes every later craft down the screen.
Each case below pins a way crewing went wrong on a real run.
"""

import oa_play
from unittest.mock import patch
from oa_forms import Control
from oa_play import (MIN_SQUAD, MAX_SQUAD, Craft, buy_troop_transport, choose_transport_purchase,
                     crew_transport, parse_fleet, parse_offers, rank_transports, squad_size)

# Verbatim from a live `gs interceptors` / `gs buyable_craft` at campaign start.
START_FLEET = ("0:Valkyrie_Interceptor_1:flying=1,armed=1,crew=0,shifter=0,pax=12,row=0|"
               "1:Stormdog_1:flying=0,armed=1,crew=0,shifter=0,pax=4,row=1|"
               "2:Phoenix_Hovercar_1:flying=1,armed=1,crew=0,shifter=0,pax=4,row=2|"
               "3:Phoenix_Hovercar_2:flying=1,armed=1,crew=0,shifter=0,pax=4,row=3|"
               "4:Wolfhound_APC_1:flying=0,armed=1,crew=0,shifter=0,pax=14,row=4")
START_MARKET = ("Blazer_Turbo_Bike:flying=0:weapons=0:price=2000:stock=4:pax=1|"
                "Griffon_AFV:flying=0:weapons=1:price=16000:stock=3:pax=4|"
                "Hoverbike:flying=1:weapons=1:price=5000:stock=4:pax=2|"
                "Phoenix_Hovercar:flying=1:weapons=2:price=12000:stock=4:pax=4|"
                "Stormdog:flying=0:weapons=1:price=6000:stock=12:pax=4|"
                "Valkyrie_Interceptor:flying=1:weapons=2:price=75000:stock=5:pax=8|"
                "Wolfhound_APC:flying=0:weapons=1:price=9000:stock=3:pax=14")


class FakeVehicle:
    def __init__(self, name, flying, pax, armed=True, shifter=False, crew=0, away=False):
        self.name, self.flying, self.pax, self.armed = name, flying, pax, armed
        self.shifter, self.crew, self.away = shifter, crew, away


class FakeBase:
    """The base's BuildingScreen and the harness replies around it."""

    ROW_H, FIRST_ROW, BOX = 26, 63, Control("graphic", "AGENT_ASSIGNMENT", None, "0", "0", "0", "0")

    def __init__(self, vehicles, free_soldiers, funds=110000, market=START_MARKET):
        self.vehicles, self.free, self.funds, self.market = vehicles, free_soldiers, funds, market
        self.stage = "CityView"
        self.sel_free: set[int] = set()
        self.sel_crew: dict[str, int] = {}     # vehicle name -> soldiers selected in its list
        self.drag = None
        self.scroll = 0
        self.clicks: list[str] = []
        self.BOX.x, self.BOX.y, self.BOX.w, self.BOX.h = 150, 126, 500, 340

    # -- the screen's rows: parked craft in fleet order, each followed by its crew ----------
    def parked(self):
        return [v for v in self.vehicles if not v.away]

    def rows(self):
        """[(y, kind, vehicle)] -- 'veh' rows, and 'crew' rows beneath their craft."""
        out, slot = [], 0
        for v in self.parked():
            out.append((self.BOX.y + self.FIRST_ROW + slot * self.ROW_H - self.scroll, "veh", v))
            slot += 1
            for _ in range(v.crew):
                out.append((self.BOX.y + self.FIRST_ROW + slot * self.ROW_H - self.scroll, "crew", v))
                slot += 1
        return out

    def hit(self, y):
        for ry, kind, v in self.rows():
            if abs(y - ry) <= self.ROW_H // 2:
                return kind, v
        return None, None

    def selected(self):
        return len(self.sel_free) + sum(self.sel_crew.values())

    # -- harness surface used by crew_transport --------------------------------------------
    def gs(self, q):
        if q == "agents":
            return {"soldiers": str(self.free + sum(v.crew for v in self.vehicles))}
        if q == "funds":
            return {"balance": str(self.funds)}
        if q == "buyable_craft":
            return {"detail": self.market}
        if q == "centre_on_base":
            return {"centred": "1", "at": "100,100,0"}
        if q == "interceptors":
            parts, row = [], 0
            for i, v in enumerate(self.vehicles):
                r = -1 if v.away else row
                row += 0 if v.away else 1
                parts.append(f"{i}:{v.name}:flying={int(v.flying)},armed={int(v.armed)},"
                             f"crew={v.crew},shifter={int(v.shifter)},pax={v.pax},row={r}")
            return {"detail": "|".join(parts)}
        raise AssertionError(f"unexpected gs query {q}")

    def ok(self, line):
        bits = line.split()
        self.clicks.append(line)
        if bits[0] == "click" and bits[-1] == "right":
            self.stage = "BuildingScreen"
        elif bits[0] == "down":
            self._down(int(bits[1]), int(bits[2]))
        elif bits[0] == "up":
            self._up(int(bits[2]))
        return ""

    def click_xy(self, x, y):
        self.clicks.append(f"click {x} {y}")
        kind, v = self.hit(y)
        if x < self.BOX.x + 250:                      # unassigned list
            r = (y + self.scroll - self.BOX.y - self.FIRST_ROW + self.ROW_H // 2) // self.ROW_H
            if 0 <= r < self.free:
                self.sel_free ^= {r}
        elif kind == "crew":                          # one craft's own crew list
            seated = self.sel_crew.get(v.name, 0)
            if seated < v.crew:
                self.sel_crew[v.name] = seated + 1

    def _down(self, x, y):
        if x < self.BOX.x + 250 and self.sel_free:
            self.drag = ("free", len(self.sel_free))
        else:
            kind, v = self.hit(y)
            if kind == "crew" and self.sel_crew.get(v.name):
                self.drag = (v.name, self.sel_crew[v.name])

    def _up(self, y):
        if not self.drag:
            return
        src, n = self.drag
        self.drag = None
        kind, target = self.hit(y)
        if kind != "veh":
            return
        # Agent::enterVehicle returns silently when the craft is full; the handler still clears.
        for _ in range(n):
            if target.crew < target.pax:
                if src == "free":
                    self.free -= 1
                else:
                    next(v for v in self.vehicles if v.name == src).crew -= 1
                target.crew += 1
        self.sel_free.clear()
        self.sel_crew.clear()


class FakeHarness:
    def __init__(self, base):
        self.b = base

    def gs(self, q):
        return self.b.gs(q)

    def ok(self, line):
        return self.b.ok(line)

    def send(self, line):
        if line == "control AGENT_SELECT_SCROLL get":
            return f"OK SCROLL value={self.b.scroll} min=0 max=1000"
        if line.startswith("control AGENT_SELECT_SCROLL set "):
            self.b.scroll = int(line.rsplit(" ", 1)[1])
        elif line == "control BUTTON_QUIT":
            self.b.stage = "CityView"
        return "OK"

    def click_xy(self, x, y):
        self.b.click_xy(x, y)


class FakeDriver(oa_play.Driver):
    def __init__(self, base):
        self.b, self.h = base, FakeHarness(base)
        self.said: list[str] = []

    def say(self, msg):
        self.said.append(msg)

    def status(self):
        box = self.b.BOX
        visible = lambda y: int(box.y <= y < box.y + box.h)
        boarding, soldiers = [], []
        for y, kind, v in self.b.rows():
            idx = self.b.vehicles.index(v)
            if kind == "veh":
                boarding.append(f"{box.x + 383},{y},{int(v.shifter)},{v.pax},"
                                f"{visible(y)},{int(v.flying)},{idx}")
            else:
                soldiers.append(f"{box.x + 403},{y},1,{visible(y)},{idx + 1},{idx}")
        for i in range(self.b.free):
            y = box.y + self.b.FIRST_ROW + i * self.b.ROW_H - self.b.scroll
            soldiers.append(f"{box.x + 103},{y},0,{visible(y)},0,-1")
        detail = (f"building=Warehouse_One_selected_agents={self.b.selected()}_"
                  f"boarding={';'.join(boarding) or '-'}_soldier_rows={';'.join(soldiers) or '-'}")
        return oa_play.Status(self.b.stage, 864, 542, detail, detail)

    def live_rect(self, cid):
        box = self.b.BOX
        return {"x": box.x, "y": box.y, "w": box.w, "h": box.h}

    def controls(self, st):
        return {"AGENT_ASSIGNMENT": self.b.BOX}

    def click_id(self, cid, st=None):
        if cid == "BUTTON_QUIT":
            self.b.stage = "CityView"
        return True

    def escape_key(self, stage=""):
        return False


def start_base(**kw):
    vehicles = [FakeVehicle("Valkyrie_Interceptor_1", True, 12),
                FakeVehicle("Stormdog_1", False, 4),
                FakeVehicle("Phoenix_Hovercar_1", True, 4),
                FakeVehicle("Phoenix_Hovercar_2", True, 4),
                FakeVehicle("Wolfhound_APC_1", False, 14)]
    return FakeBase(vehicles, kw.pop("free", 10), **kw)


def crew_of(base, name):
    return next(v.crew for v in base.vehicles if v.name == name)


def test_parse_and_rank():
    fleet = parse_fleet(START_FLEET)
    assert len(fleet) == 5 and fleet[0].pax == 12 and fleet[0].row == 0 and fleet[1].flying is False
    assert fleet[2].kind == "Phoenix_Hovercar"
    assert parse_fleet("-") == [] and parse_fleet("") == []
    # An older binary with no row= field reads as "not parked", never as row 0.
    assert parse_fleet("0:A_1:flying=1,armed=0,crew=0,shifter=0,pax=8")[0].row == -1

    # Capacity first: at the start the Valkyrie IS the troop transport, "Interceptor" or not.
    assert rank_transports(fleet)[0].name == "Valkyrie_Interceptor_1"
    # Road craft never qualify, whatever they seat; neither does anything that is not parked here.
    assert not {"Stormdog_1", "Wolfhound_APC_1"} & {c.name for c in rank_transports(fleet)}
    out = [Craft(0, "Valkyrie_Interceptor_1", True, True, 0, False, 12, -1),
           Craft(1, "Hoverbike_22", True, True, 0, False, 2, 0),
           Craft(2, "Blazer_Turbo_Bike_3", False, False, 0, False, 1, 1),
           Craft(3, "Phoenix_Hovercar_1", True, True, 0, False, 4, 2)]
    # Valkyrie out, row 0 a hoverbike: the old "row 0" pick; the ranking says Phoenix.
    assert [c.name for c in rank_transports(out)] == ["Phoenix_Hovercar_1"]
    assert rank_transports([c for c in out if c.pax < MIN_SQUAD]) == []

    # Seats are capped at the squad wanted, so a bigger hull does not outrank a six-seater.
    six = Craft(0, "Six_1", True, False, 0, False, 6, 0)
    big = Craft(1, "Big_1", True, False, 0, False, 14, 1)
    assert rank_transports([big, six], want=6)[0] is six        # equal capped seats: smaller hull
    # ...then a dimension shifter breaks the tie, which is what carries a squad through the gate.
    shifty = Craft(2, "Shifty_1", True, True, 0, True, 6, 2)
    assert rank_transports([six, big, shifty], want=6)[0] is shifty
    # Gate capability wins even when its hull seats fewer soldiers.
    small = Craft(3, "Small_1", True, False, 0, True, 4, 3)
    assert rank_transports([small, six], want=6)[0] is small
    # Unarmed over armed at equal capacity, so interceptors stay free to intercept.
    armed = Craft(4, "Gun_1", True, True, 0, False, 6, 0)
    assert rank_transports([armed, six], want=6)[0] is six
    # A kind that has failed to load twice sinks below everything else.
    assert rank_transports([six, armed], want=6, failed={"Six": 2})[0] is armed
    assert rank_transports([six, armed], want=6, failed={"Six": 1})[0] is six

    # Dispatching: only flyers that carry soldiers, biggest squad first.
    crewed = [Craft(0, "Valkyrie_1", True, True, 1, False, 12, 0),
              Craft(1, "Phoenix_1", True, True, 4, False, 4, 1),
              Craft(2, "Stormdog_1", False, True, 6, False, 4, 2),
              Craft(3, "Phoenix_2", True, True, 0, False, 4, 3)]
    assert [c.name for c in rank_transports(crewed, loaded=True)] == ["Phoenix_1", "Valkyrie_1"]


def test_squad_size():
    assert squad_size(10, 12) == (6, 4)      # full roster: fill the squad, keep a garrison
    assert squad_size(10, 4) == (4, 4)       # a four-seater carries four, not six
    assert squad_size(10, 2) == (2, 4)
    assert squad_size(1, 12) == (1, 0)       # one soldier still flies
    for n in (12, 10, 6, 4, 3, 2):
        take, held = squad_size(n, 12)
        assert 1 <= take <= MAX_SQUAD and held >= 1, (n, take, held)
        assert take + held <= n
    assert squad_size(8, 3) == (3, 4)


def test_purchase_choice():
    offers = parse_offers(START_MARKET)
    assert next(o for o in offers if o["name"] == "Phoenix Hovercar")["pax"] == 4
    pick = choose_transport_purchase(offers, 110000)
    assert pick["name"] == "Phoenix Hovercar"            # cheapest flyer that seats a squad
    assert choose_transport_purchase(offers, 52000)["name"] == "Phoenix Hovercar"   # $12k + reserve
    assert choose_transport_purchase(offers, 51999) is None      # the reserve is not spent
    # Bikes seat too few and road craft do not fly, so they are never offered however cheap.
    only_small = [o for o in offers if o["name"] in ("Hoverbike", "Stormdog", "Blazer Turbo Bike")]
    assert choose_transport_purchase(only_small, 500000) is None


def test_boards_the_transport_not_row_zero():
    # Start of a campaign: the Valkyrie is home and seats the whole squad.
    base = start_base()
    d = FakeDriver(base)
    assert crew_transport(d) == 1
    assert crew_of(base, "Valkyrie_Interceptor_1") == 6 and base.free == 4

    # The Valkyrie is out. Row 0 is now a Stormdog; the old driver filled it (four seats), then
    # put what was left on a Phoenix and called it crewed. Nothing may go on the road craft.
    base = start_base()
    base.vehicles[0].away = True
    d = FakeDriver(base)
    assert crew_transport(d) == 1
    assert crew_of(base, "Phoenix_Hovercar_1") == 4
    assert crew_of(base, "Stormdog_1") == 0 and crew_of(base, "Wolfhound_APC_1") == 0
    assert crew_of(base, "Phoenix_Hovercar_2") == 0 and base.free == 6


def test_never_loads_bikes():
    base = FakeBase([FakeVehicle("Blazer_Turbo_Bike_1", False, 1, armed=False),
                     FakeVehicle("Hoverbike_22", True, 2),
                     FakeVehicle("Hoverbike_23", True, 2)], 10, funds=20000)
    d = FakeDriver(base)
    # No transport owned and no money for one: crewing refuses rather than seating two soldiers.
    assert crew_transport(d) == 0
    assert all(v.crew == 0 for v in base.vehicles)
    assert base.clicks == [], f"must not touch the screen: {base.clicks}"


def test_recovers_a_squad_stranded_on_the_wrong_craft():
    # The 0 -> 0 state: every soldier already aboard road vehicles, so the unassigned list is
    # empty and a driver that only drags from it moves nobody, however often it tries.
    base = start_base(free=0)
    base.vehicles[1].crew = 4       # Stormdog, row 1
    base.vehicles[4].crew = 6       # Wolfhound, row 4: pushed down four rows by the Stormdog's crew
    d = FakeDriver(base)
    assert crew_transport(d) == 1
    assert crew_of(base, "Valkyrie_Interceptor_1") == 6
    assert crew_of(base, "Stormdog_1") + crew_of(base, "Wolfhound_APC_1") == 4
    assert base.selected() == 0


def test_buys_a_transport_when_none_is_owned():
    base = FakeBase([FakeVehicle("Stormdog_1", False, 4), FakeVehicle("Hoverbike_22", True, 2)], 10)
    bought = []

    def fake_buy(d, wanted, qty=8, category="BUTTON_AGENTS"):
        bought.append((wanted, qty, category))
        base.vehicles.append(FakeVehicle("Phoenix_Hovercar_9", True, 4))
        return 1

    real = oa_play.buy_named
    oa_play.buy_named = fake_buy
    try:
        d = FakeDriver(base)
        assert crew_transport(d) == 1
    finally:
        oa_play.buy_named = real
    assert bought == [(["Phoenix Hovercar"], 1, "BUTTON_VEHICLES")]
    assert crew_of(base, "Phoenix_Hovercar_9") == 4 and crew_of(base, "Hoverbike_22") == 0

    # A transport that is merely out on a mission is not a reason to buy another.
    base = FakeBase([FakeVehicle("Valkyrie_Interceptor_1", True, 12, away=True),
                     FakeVehicle("Hoverbike_22", True, 2)], 10)
    oa_play.buy_named = fake_buy
    try:
        before = len(bought)
        assert crew_transport(FakeDriver(base)) == 0
        assert len(bought) == before and base.clicks == []
    finally:
        oa_play.buy_named = real


def test_a_purchase_that_never_arrives_is_not_reported():
    base = FakeBase([FakeVehicle("Hoverbike_22", True, 2)], 10)
    d = FakeDriver(base)
    real = oa_play.buy_named
    oa_play.buy_named = lambda d, wanted, qty=8, category="": 1       # order accepted, nothing came
    try:
        assert buy_troop_transport(d, parse_fleet(base.gs("interceptors")["detail"])) == 0
    finally:
        oa_play.buy_named = real
    assert any("has arrived" in m for m in d.said)


def test_crewed_means_a_squad_on_a_flyer():
    def crewed(vehicles, soldiers_home=0):
        return oa_play._flying_crewed(FakeDriver(FakeBase(vehicles, soldiers_home)))

    # One soldier on a hoverbike is not a crewed craft; neither is a squad on a road vehicle.
    assert crewed([FakeVehicle("Hoverbike_22", True, 2, crew=1)], soldiers_home=9) == 0
    assert crewed([FakeVehicle("Stormdog_1", False, 4, crew=4)], soldiers_home=6) == 0
    assert crewed([FakeVehicle("Phoenix_Hovercar_1", True, 4, crew=4)], soldiers_home=6) == 1
    # A roster smaller than a squad: everyone aboard is the squad.
    assert crewed([FakeVehicle("Hoverbike_22", True, 2, crew=2)], soldiers_home=0) == 1


def test_every_caller_reaches_the_ranking():
    import inspect
    import oa_victory
    # The gates that decide whether to crew must read the same definition of "crewed", and the
    # campaign runner's own gate must not read gs vehicles' crewed (it counts road craft).
    src = inspect.getsource(oa_victory)
    assert "crewed = _flying_crewed(self.d)" in src and 'get("crewed"' not in src
    assert "rank_transports" in inspect.getsource(oa_play.select_crewed_craft)
    assert "rank_transports" in inspect.getsource(oa_play.crew_transport)
    assert "garrison" in inspect.getsource(oa_play.crew_transport)


def test_full_fleet_format_preserves_endgame_fields():
    detail = ("7:Bio:Transport_1:flying=1,armed=0,crew=4,shifter=1,pax=6,row=2,"
              "city=CITYMAP_ALIEN,transit=1,home=0,id=VEHICLE_42,portal=1,extra=keep")
    craft = parse_fleet(detail)[0]
    assert (craft.idx, craft.name, craft.row, craft.city, craft.id) == (
        7, "Bio:Transport_1", 2, "CITYMAP_ALIEN", "VEHICLE_42")
    assert craft.shifter and craft.transit and craft.portal and not craft.home
    flags = oa_play.parse_craft_flags(detail)[0][2]
    assert flags["extra"] == "keep" and flags["city"] == "CITYMAP_ALIEN"
    assert flags["row"] == "2" and flags["id"] == "VEHICLE_42"


def test_gate_transport_wins_capacity_and_waits_when_away():
    base = FakeBase([FakeVehicle("Ordinary_1", True, 12, crew=6),
                     FakeVehicle("Bio_1", True, 4, shifter=True)], 6)
    d = FakeDriver(base)
    assert crew_transport(d) == 1
    assert base.vehicles[1].crew == 4 and base.vehicles[0].crew == 2 and base.free == 6
    base = FakeBase([FakeVehicle("Ordinary_1", True, 12),
                     FakeVehicle("Bio_1", True, 4, shifter=True, away=True)], 12)
    assert crew_transport(FakeDriver(base)) == 0 and base.clicks == []


def test_drop_refusal_is_verified_and_changes_the_next_choice():
    base = start_base()
    real_drop = base._up
    def refuse(y):
        kind, target = base.hit(y)
        if target is base.vehicles[0]:
            base.drag = None
            base.sel_free.clear()
            base.sel_crew.clear()
        else:
            real_drop(y)
    base._up = refuse
    d = FakeDriver(base)
    for _ in range(2):
        assert crew_transport(d) == 0
    assert d.crew_failures["Valkyrie_Interceptor"] == 2
    assert crew_transport(d) == 1 and base.vehicles[2].crew == 4
    assert base.vehicles[0].crew == 0


def test_partial_squad_is_topped_up_without_dragging_it_onto_itself():
    base = FakeBase([FakeVehicle("Transport_1", True, 12, crew=2)], 8)
    assert crew_transport(FakeDriver(base)) == 1
    assert base.vehicles[0].crew == 6 and base.free == 4


def test_offscreen_craft_is_resolved_again_after_scrolling():
    base = FakeBase([FakeVehicle(f"Road_{i}", False, 1) for i in range(15)] +
                    [FakeVehicle("Transport_1", True, 12)], 10)
    assert crew_transport(FakeDriver(base)) == 1
    assert base.vehicles[-1].crew == 6 and base.free == 4
    assert all(v.crew == 0 for v in base.vehicles[:-1])


def test_garrison_and_small_roster_share_the_crewed_definition():
    assert squad_size(0, 12) == (0, 0)
    base = FakeBase([FakeVehicle("Transport_1", True, 12)], 6)
    assert crew_transport(FakeDriver(base)) == 1
    assert base.vehicles[0].crew == 3 and base.free == 3


if __name__ == "__main__":
    for name, fn in sorted(globals().items()):
        if name.startswith("test_") and callable(fn):
            with patch.object(oa_play.time, "sleep"):
                fn()
            print("ok", name)
    print("all crew tests passed")
