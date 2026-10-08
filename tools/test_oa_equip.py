#!/usr/bin/env python3
"""Named equip path regression coverage now lives in the role planner tests."""
from unittest.mock import patch
import oa_play
import oa_victory
from test_oa_loadouts import FakeDriver, run_pass


def test_named_path_reaches_recruits_beyond_visible_portraits():
    from test_oa_loadouts import roster
    d = FakeDriver(agents=roster(24))
    result = run_pass(d)
    assert result["verified"] == 24 and result["gained"] == 24, d.events
    assert {s.split()[2] for s in d.sent if s.startswith("action aequip_select")} == {
        r["id"] for r in d.agents}
    assert not any("AGENT_SELECT_BOX" in s for s in d.sent)


def test_arm_squad_reports_only_the_observed_delta():
    d = FakeDriver()
    with patch.object(oa_play, "prepare_loadouts", return_value={"gained": 3}) as prepare:
        assert oa_play.arm_squad(d) == 3
    prepare.assert_called_once_with(d, "base_defence", agents=24)


def test_arming_is_reachable_from_both_runner_loops():
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
