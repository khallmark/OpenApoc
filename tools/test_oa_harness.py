#!/usr/bin/env python3
"""Harness tests that need no game: pure logic on the Driver.

Small, but each one pins a bug that cost a real run.
"""

import oa_play


def test_harness_regressions():
    FAILED = []


    def check(cond, msg):
        if not cond:
            FAILED.append(msg)


    class FakeHarness:
        """Records keys instead of sending them."""

        def __init__(self):
            self.keys = []

        def key(self, k):
            self.keys.append(k)
            return "OK"


    class FakeDriver(oa_play.Driver):
        """A Driver with no socket, no forms and no game -- only the key path under test."""

        def __init__(self, stage):
            self.h = FakeHarness()
            self._stage = stage

        def status(self):
            return oa_play.Status(stage=self._stage, w=1280, h=720, raw="")


    # --- Escape must not open the settings menu ----------------------------------
    # CityView and BattleView both PUSH InGameOptions on SDLK_ESCAPE (cityview.cpp:4156,
    # battleview.cpp:3380). Escape is not "back" there, it is "open settings" -- and the harness used
    # it as its universal "stuck, get out of this" fallback. One run reached InGameOptions thirteen
    # times without ever asking for it, each visit costing a round to notice and another to close.
    for stage in ("CityView", "BattleView"):
        d = FakeDriver(stage)
        sent = d.escape_key()
        check(sent is False, f"escape_key must refuse on {stage}, it would open the options menu")
        check(d.h.keys == [], f"…and must not send the key at all on {stage}, sent {d.h.keys}")

    # Everywhere else Escape genuinely means back, and refusing would strand the run on that screen.
    for stage in ("BuildingScreen", "ResearchScreen", "UfopaediaView", "BaseScreen", "InGameOptions"):
        d = FakeDriver(stage)
        check(d.escape_key() is True, f"escape_key must still work on {stage}")
        check(d.h.keys == ["Escape"], f"…and send exactly one Escape on {stage}, sent {d.h.keys}")

    # The stage may be passed in to save a round-trip; it must be honoured, not ignored.
    d = FakeDriver("BuildingScreen")
    check(d.escape_key("CityView") is False, "an explicit stage argument must be respected")
    check(d.h.keys == [], "…and must suppress the key")

    # leave_battle is the one deliberate user of raw Escape: BUTTON_EXIT_BATTLE lives inside
    # InGameOptions, so opening it there is the point. Guard against a future tidy-up routing it
    # through escape_key, which would make leaving a battle impossible.
    import inspect

    src = inspect.getsource(oa_play.leave_battle)
    check('d.h.key("Escape")' in src,
          "leave_battle must keep raw Escape -- it opens InGameOptions on purpose")
    check("escape_key" not in src, "…and must not be routed through the guard")

    # No fallback elsewhere may still press Escape directly. Exactly two places are allowed to:
    # leave_battle, where opening InGameOptions is the goal, and escape_key itself, which IS the
    # guard. Anything else is a fallback that can open the settings menu by accident, which is the
    # bug this whole file exists to prevent coming back.
    whole = inspect.getsource(oa_play)
    raw_total = whole.count('d.h.key("Escape")') + whole.count('self.h.key("Escape")')
    allowed = src.count('d.h.key("Escape")') + inspect.getsource(oa_play.Driver.escape_key).count(
        'self.h.key("Escape")')
    check(raw_total == allowed,
          f"{raw_total - allowed} unguarded Escape call(s) remain outside leave_battle/escape_key")
    check(inspect.getsource(oa_play.Driver.escape_key).count('self.h.key("Escape")') == 1,
          "escape_key must press the key itself exactly once -- it recursed into itself when a "
          "blanket replace rewrote its own body, and every guarded call then blew the stack")

    # --- battle-scoped queries must tolerate the battle ending -------------------
    # Battle::checkMissionEnd tears current_battle down the instant the last hostile dies, so a query
    # already in flight comes back an error. Twice now that raised out of win_battle and threw away a
    # mission that had just been WON -- first enemies_screen, then centre_on_enemy, because the first
    # fix patched the instance rather than the class.
    class DyingHarness(FakeHarness):
        def gs(self, q):
            raise oa_play.HarnessError(f'gs {q} -> ERR unknown query "{q}"')

        def screen_craft(self, q):
            raise oa_play.HarnessError(f'gs {q} -> ERR unknown query "{q}"')


    dead = FakeDriver("BattleView")
    dead.h = DyingHarness()
    for q in ("centre_on_enemy", "centre_on_friends", "battle_positions", "battle"):
        check(oa_play.battle_gs(dead, q) == {},
              f"battle_gs must return {{}} when the battle has gone, for {q}")
    for q in ("enemies_screen", "friends_screen"):
        check(oa_play.on_screen(dead, q) == [],
              f"on_screen must return [] when the battle has gone, for {q}")

    # No battle-scoped query in the battle loop may still call gs directly -- that is precisely how
    # the second one was missed.
    whole = inspect.getsource(oa_play)
    for q in ("centre_on_enemy", "centre_on_friends"):
        check(f'd.h.gs("{q}")' not in whole,
              f"{q} must go through battle_gs, not raw gs")
    for q in ("enemies_screen", "friends_screen"):
        check(f'd.h.screen_craft("{q}")' not in whole,
              f"{q} must go through on_screen, not raw screen_craft")

    # --- never press Quit on the options menu ------------------------------------
    # BUTTON_QUIT means "leave this screen" on BuildingScreen, BribeScreen and friends, and
    # "exit the program" on InGameOptions (ingameoptions.cpp:215 -> StageCmd::Command::QUIT).
    # return_to_city tried it FIRST, unconditionally, so any run that reached the options menu shut
    # its own game down: cleanly, rc=0, no crash -- and then reported that the game had gone. That was
    # the whole "ConnectionRefusedError [exited rc=0]" failure class, five attempts across two runs.
    src_rtc = inspect.getsource(oa_play.return_to_city)
    check("quit_exits_game" in src_rtc,
          "return_to_city must special-case the screens where Quit exits the program")
    check('"InGameOptions"' in src_rtc and '"MainMenu"' in src_rtc,
          "…and must name both of them")
    # The dangerous ordering must be gone: BUTTON_QUIT may never be the first thing tried unguarded.
    check('("BUTTON_QUIT", "BUTTON_OK")' in src_rtc and '("BUTTON_OK",) if quit_exits_game' in src_rtc,
          "on a quit-exits screen it must try BUTTON_OK only")

    # The response table must not route InGameOptions at BUTTON_QUIT either.
    opts = oa_play.RESPONSES.get("InGameOptions", {})
    check("BUTTON_QUIT" not in opts.values(),
          f"InGameOptions must never be acked with BUTTON_QUIT, got {opts}")

    # --- a lost mission has no survivors ------------------------------------------
    # survivors is the last MID-BATTLE sample; current_battle is torn down before the debriefing, so
    # there is nothing left to query. A squad stunned unconscious therefore reported 6 of 6 on a
    # mission it had just LOST, and the arena scored that 0.30 -- full marks for survival.
    # checkMissionEnd sets playerWon=false exactly when no player unit is conscious
    # (battle.cpp:2160-2205), so "lost" and "somebody still standing" cannot both be true.
    src_wb = inspect.getsource(oa_play.win_battle)
    check('if outcome == "lost"' in src_wb, "win_battle must zero survivors on a loss")
    check('survivors_last_seen' in src_wb, "…and keep the stale reading under its own name")
    check('"returned"' in src_wb,
          "…and say why a withdrawal is exempt -- those really are survivors")

    import oa_adversarial_arena as _A
    scores = {
        "won intact":   _A.utility("resolved", 6, 6),
        "won pyrrhic":  _A.utility("resolved", 6, 1),
        "withdrew":     _A.utility("returned", 6, 4),
        "lost":         _A.utility("lost", 6, 0),
    }
    check(scores["won intact"] > scores["won pyrrhic"] > scores["withdrew"] > scores["lost"],
          f"scoring must order outcomes as a campaign would: {scores}")
    check(scores["lost"] == 0.0, f"a wipe scores zero, got {scores['lost']}")

    # --- a base defence is never abandoned ---------------------------------------
    # Not when losing, not when stalled, not when a second base exists. Withdrawing forfeits the base:
    # every facility reverts to unbuilt and the labs, stores and staff go with it. The rule was
    # softened once -- "losing a base only ends the game when it is the LAST base" -- and that
    # reasoning is wrong in a way the citation hides: the aliens who take it are still there, the
    # facilities are still gone, and the squad has spent lives buying nothing.
    src_wb = inspect.getsource(oa_play)
    check('may_leave = mission_type != "base_defense"' in src_wb,
          "a base defence must never be leavable")
    check("bases_now" not in src_wb,
          "the base COUNT must not enter the decision at all -- owning a spare is not a reason to "
          "concede your home")

    # --- the transport never takes the whole garrison ----------------------------
    # Aliens attack the BASE. A base defended by scientists and engineers is not defended at all --
    # observed 15 units in a base defence, every one unarmed, killed one at a time by a single alien.
    # Whoever flew out was not home when it happened.
    #
    # But the reserve must SCALE. A flat "hold back four" means a campaign with four soldiers never
    # flies a mission, which is the same cannot-fight failure in a different costume. Every roster
    # size must both send someone and keep someone.
    # Calls the real sizing function, not a copy of its formula: a copy keeps passing while the
    # driver does something else. A twelve-seater, so the seats never bind and the garrison rule does.
    def _crew_split(soldiers, garrison=4):
        take, _held = oa_play.squad_size(soldiers, 12, garrison)
        return take, max(0, soldiers - take)

    for _n in (12, 10, 6, 4, 3, 2):
        _take, _home = _crew_split(_n)
        check(_take >= 1, f"{_n} soldiers must still be able to fly a mission, take={_take}")
        check(_home >= 1, f"{_n} soldiers must leave someone to defend the base, home={_home}")
        check(_take <= 6, f"a transport holds six, tried to take {_take}")

    # A single soldier flies -- holding them back defends a base that will lose anyway for want of
    # any mission ever being flown.
    check(_crew_split(1) == (1, 0), f"one soldier flies, got {_crew_split(1)}")
    # A full roster fills the transport and still leaves a real garrison.
    check(_crew_split(12) == (6, 6), f"a full roster fills the craft AND garrisons, got {_crew_split(12)}")

    check("garrison" in inspect.getsource(oa_play.crew_transport),
          "crew_transport must reserve a garrison rather than taking the first six")

    # A stalled battle sends the squad to the first spotted hostile's tile, on whatever floor.
    check(oa_play.first_foe_tile("67,52,2:sx=432:sy=228:kind=A;10,8,0:sx=1") == (67, 52, 2),
          "first_foe_tile reads the first hostile's x,y,z")
    check(oa_play.first_foe_tile("-") is None and oa_play.first_foe_tile(None) is None,
          "no spotted hostile, no target")

    class SquadHarness:
        def __init__(self):
            self.queries = []

        def gs(self, q):
            self.queries.append(q)
            if q == "battle_positions":
                return {"foe_at": "67,52,2:sx=432:sy=228"}
            return {"ordered": "24", "refused": "0"}

    class SquadDriver:
        def __init__(self):
            self.h = SquadHarness()
            self.said = []

        def say(self, msg):
            self.said.append(msg)

    _sq = SquadDriver()
    check(oa_play.send_squad_to_foe(_sq) and "squad_to 67 52 2" in _sq.h.queries,
          f"the squad is ordered to the hostile's own tile and floor, got {_sq.h.queries}")

    # The search starts at the map centre, covers every cell before repeating, then moves floor.
    _wp = [oa_play.search_waypoint((100, 100, 4), _s) for _s in range(34)]
    check(_wp[0] == (50, 50, 1), f"the search starts at the map centre, got {_wp[0]}")
    check(len({w[:2] for w in _wp[1:17]}) == 16, "one pass visits all 16 grid cells")
    check(all(w[2] == 1 for w in _wp[:17]) and all(w[2] == 2 for w in _wp[17:34]),
          "a whole pass stays on one floor, then the next pass moves up")
    check(max(abs(w[0] - 50) + abs(w[1] - 50) for w in _wp[1:5]) <
          min(abs(w[0] - 50) + abs(w[1] - 50) for w in _wp[13:17]),
          "inner cells come before the corners")
    check(all(0 <= c < 100 for _s in range(64) for c in oa_play.search_waypoint((100, 100, 2), _s)[:2])
          and all(oa_play.search_waypoint((100, 100, 2), _s)[2] < 2 for _s in range(64)),
          "waypoints stay on the map")
    check(oa_play.squad_tiles("27,58,1:sx=-1;8,43,1:sx=432") == [(27, 58, 1), (8, 43, 1)]
          and oa_play.squad_tiles("-") == [], "squad_tiles reads mine_at")

    # Stage details arrive with spaces turned into underscores.
    _detail = "building=BUILDING_WAREHOUSE_ONE_crew=2_selected_agents=3_boarding=0_soldier_rows=5"
    check(oa_play.detail_int(_detail, "selected_agents") == 3,
          f"selected_agents must parse when more fields follow it, got "
          f"{oa_play.detail_int(_detail, 'selected_agents')}")
    check(oa_play.detail_int(_detail, "crew") == 2 and oa_play.detail_int(_detail, "boarding") == 0,
          "other fields parse the same way")
    check(oa_play.detail_int(_detail, "agents") is None,
          "a field name must not match inside another (agents vs selected_agents)")
    check(oa_play.detail_int(None, "crew") is None, "no detail, no value")

    # The gate-craft selection right-clicks through click_xy; it must reach the wire as such.
    _sent = []
    _h = oa_play.Harness(port=1)
    _h.ok = lambda line: _sent.append(line) or "OK"
    _h.click_xy(10, 20)
    _h.click_xy(30, 40, button="right")
    check(_sent == ["click 10 20", "click 30 40 right"], f"click_xy sends {_sent}")

    # select_armed_squad never puts an unarmed soldier into a fight.
    class SquadScreen:
        """A fake alert/building screen: rows as the harness reports them, clicks select."""

        def __init__(self, stage, boarding, soldiers):
            self.stage, self.boarding, self.soldiers = stage, boarding, soldiers
            self.clicked = []

        def detail(self):
            boarding = ";".join(",".join(map(str, r)) for r in self.boarding) or "-"
            soldiers = ";".join(",".join(map(str, r)) for r in self.soldiers) or "-"
            return (f"alert_building=X crew=2 owner=Y selected_agents="
                    f"{sum(1 for r in self.soldiers if r[:2] in self.clicked)} "
                    f"boarding={boarding} soldier_rows={soldiers}").replace(" ", "_")

    class SquadHarness:
        def __init__(self, screen):
            self.screen = screen

        def click_xy(self, x, y, button="left"):
            self.screen.clicked.append((x, y))

    class SquadPicker(oa_play.Driver):
        def __init__(self, screen):
            self.screen = screen
            self.h = SquadHarness(screen)

        def status(self):
            return oa_play.Status(stage=self.screen.stage, w=800, h=600, raw="",
                                  detail=self.screen.detail())

    # boarding: x,y,shifter,pax,visible,flying,fleet,aboard,unarmed
    # soldier_rows: x,y,in_craft,visible,group,fleet,armed
    _boarding = [(500, 100, 0, 6, 1, 1, 0, 2, 1),   # craft 0: one of two aboard unarmed
                 (500, 200, 0, 6, 1, 1, 1, 3, 0)]   # craft 1: all three armed
    _soldiers = [(100, 100, 0, 1, 0, -1, 1), (100, 126, 0, 1, 0, -1, 0),   # on foot
                 (100, 152, 0, 1, 0, -1, 1),
                 (520, 110, 1, 1, 1, 0, 1), (520, 136, 1, 1, 1, 0, 0),    # aboard craft 0
                 (520, 210, 1, 1, 2, 1, 1), (520, 236, 1, 1, 2, 1, 1),    # aboard craft 1
                 (520, 262, 1, 1, 2, 1, 1)]
    unarmed_at = {r[:2] for r in _soldiers if r[6] == 0}

    _alert = SquadScreen("AlertScreen", _boarding, _soldiers)
    _n = SquadPicker(_alert).select_armed_squad(SquadPicker(_alert).status())
    check(_alert.clicked[0] == (500, 200) and _n == 3,
          f"an alert sends the fully armed craft with its crew, got {_alert.clicked} ({_n})")
    check((500, 100) not in _alert.clicked and not unarmed_at & set(_alert.clicked),
          f"never the craft carrying an unarmed soldier, nor an unarmed soldier: {_alert.clicked}")

    _mixed = SquadScreen("AlertScreen", _boarding[:1], [r for r in _soldiers if r[5] != 1])
    _n = SquadPicker(_mixed).select_armed_squad(SquadPicker(_mixed).status())
    check(_n == 2 and set(_mixed.clicked) == {(100, 100), (100, 152)},
          f"with no fully armed craft, armed soldiers go on foot: {_mixed.clicked} ({_n})")

    _bld = SquadScreen("BuildingScreen", _boarding, _soldiers)
    _n = SquadPicker(_bld).select_armed_squad(SquadPicker(_bld).status())
    check(_n == 6 and not unarmed_at & set(_bld.clicked),
          f"a building raid takes the armed soldiers only: {_bld.clicked} ({_n})")

    _none = SquadScreen("AlertScreen", [], [(100, 100, 0, 1, 0, -1, 0)])
    check(SquadPicker(_none).select_armed_squad(SquadPicker(_none).status()) == 0
          and not _none.clicked, "nobody armed: nobody selected")

    # --- Aliens beside a base are swept first --------------------------------------------------
    # A crew next to a base can move in and expose it to the UFOs that attack bases, so its alert
    # jumps the queue and is never the one aged out of it.
    class AlertDriver(oa_play.Driver):
        def __init__(self):
            self.alerted_buildings, self.base_threats, self.events = [], set(), []
            self.verbose = False

    def alert(name, threat):
        return oa_play.Status("AlertScreen", 1280, 720, "",
                              f"alert_building={name}_crew=3_owner=ORG_X_threatens_base={threat}"
                              f"_selected_agents=0")

    _q = AlertDriver()
    for i in range(5):
        _q.note_alert(alert(f"Far_{i}", 0))
    _q.note_alert(alert("Next_Door", 1))
    check(_q.alerted_buildings[0] == "Next_Door" and "Next_Door" in _q.base_threats,
          f"a base threat goes to the head of the sweep queue: {_q.alerted_buildings}")
    for i in range(5, 12):
        _q.note_alert(alert(f"Far_{i}", 0))
    check("Next_Door" in _q.alerted_buildings and len(_q.alerted_buildings) == 6,
          f"the queue stays short but never drops a base threat: {_q.alerted_buildings}")
    _q.note_alert(alert("Far_11", 1))
    check(_q.alerted_buildings[0] == "Far_11",
          f"an address already queued moves up when it turns out to threaten a base: "
          f"{_q.alerted_buildings}")

    # --- No raid against odds the squad cannot win --------------------------------------------
    # Two soldiers sent against fifteen aliens were wiped out and the base defences followed.
    class RaidHarness:
        def __init__(self, crews, armed):
            self.crews, self.armed, self.asked = crews, armed, []

        def gs(self, q):
            self.asked.append(q)
            if q == "agents":
                return {"armed": str(self.armed)}
            if q.startswith("centre_on_building "):
                name = q.split(" ", 1)[1]
                return {"centred": "1", "crew": str(self.crews.get(name, 0)), "building": name,
                        "at": "10,10"}
            return {}

    class RaidDriver(AlertDriver):
        def __init__(self, crews, armed, queue):
            super().__init__()
            self.h = RaidHarness(crews, armed)
            self.alerted_buildings = list(queue)

        def status(self):
            return oa_play.Status("CityView", 1280, 720, "")

    _r = RaidDriver({"Big": 15}, 2, ["Big"])
    check(oa_play.raid_infiltrated_building(_r) == "outmatched",
          "fifteen aliens against two armed soldiers is not raided")
    check(not any(q == "centre_on_message" for q in _r.h.asked),
          f"…and the message log is not used to raid it anyway: {_r.h.asked}")
    check(_r.alerted_buildings == ["Big"], "…but the address stays queued for when the squad grows")

    # --- Idle squads parked across town are sent home ------------------------------------------
    # Three base defences were lost to one alien each because every soldier sat in a craft parked
    # at an investigated building with no orders, and the base held only unarmed staff.
    class FleetHarness:
        def __init__(self, crafts):
            self.crafts = crafts

        def gs(self, q):
            if q == "interceptors":
                return {"detail": "|".join(
                    f"{i}:Craft_{i}:" + ",".join(f"{k}={v}" for k, v in c.items())
                    for i, c in enumerate(self.crafts))}
            return {}

    class FleetDriver(AlertDriver):
        def __init__(self, crafts):
            super().__init__()
            self.h = FleetHarness(crafts)
            self.stranded_since, self.sent_home = {}, []

        def status(self):
            return oa_play.Status("CityView", 1280, 720, "")

        def click_id(self, control, st=None):
            if control == "BUTTON_GOTO_BASE":
                self.sent_home.append(self._selected)
            return True

    def craft(cid, **kw):
        return {"flying": 1, "armed": 0, "crew": 4, "home": 0, "id": cid, "at": "BUILDING_X",
                "idle": 1, **kw}

    _fleet = FleetDriver([craft("STRANDED"), craft("HOME", home=1, at="BUILDING_BASE"),
                          craft("BUSY", idle=0), craft("EMPTY", crew=0)])
    _clock = [1000.0]
    _real_time, _real_select, _real_sleep = oa_play.time.time, oa_play.select_craft, oa_play.time.sleep
    oa_play.time.time = lambda: _clock[0]
    oa_play.time.sleep = lambda s: None
    oa_play.select_craft = lambda d, cid: (setattr(d, "_selected", cid) or True)
    try:
        check(oa_play.recall_stranded_squads(_fleet) == 0 and not _fleet.sent_home,
              "a squad that has only just parked is left alone")
        _clock[0] += oa_play.STRANDED_AFTER_S + 1
        check(oa_play.recall_stranded_squads(_fleet) == 1 and _fleet.sent_home == ["STRANDED"],
              f"only the idle crewed craft parked away from home is recalled: {_fleet.sent_home}")
        _moved = FleetDriver([craft("STRANDED")])
        oa_play.recall_stranded_squads(_moved)
        _moved.h.crafts[0]["at"] = "BUILDING_Y"
        _clock[0] += oa_play.STRANDED_AFTER_S + 1
        check(oa_play.recall_stranded_squads(_moved) == 0,
              "a craft that has moved on to another building starts its clock again")
    finally:
        oa_play.time.time, oa_play.select_craft, oa_play.time.sleep = _real_time, _real_select, _real_sleep

    assert not FAILED, "FAILED:\n" + "\n".join(FAILED)


if __name__ == "__main__":
    test_harness_regressions()
    print("all harness tests passed (Escape guard)")
