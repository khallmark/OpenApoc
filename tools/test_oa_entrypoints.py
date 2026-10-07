#!/usr/bin/env python3
"""Runner wiring and watch controls, using fake game processes and socket replies."""
from contextlib import ExitStack, redirect_stdout
from io import StringIO
import os
from pathlib import Path
import sys
from tempfile import TemporaryDirectory
from types import SimpleNamespace
from unittest.mock import Mock, patch

import check_dead_defs
import oa_campaign
import oa_play
import oa_skirmish
import oa_victory


def test_runner_main_options_reach_battle_policy():
    for module in (oa_play, oa_campaign, oa_victory, oa_skirmish):
        with TemporaryDirectory() as tmp, ExitStack() as stack:
            out = Path(tmp)
            # Exercise the real restart construction path without fresh-game UI setup.
            for name in ("campaign.save", "victory.save"):
                (out / name).touch()
            stack.enter_context(patch.dict(os.environ, {}, clear=True))
            stack.enter_context(redirect_stdout(StringIO()))
            argv = [module.__file__, "--out", tmp, "--watch", "--city-speed", "2",
                    "--step-delay", "0.01", "--ai", "skirmisher"]
            stack.enter_context(patch.object(sys, "argv", argv))
            game = Mock()
            game.binary = "fake-game"
            game.warnings.return_value = []
            def start(**kw):
                # These must already be set when the game starts.
                assert os.environ["OA_WATCH"] == "1"
                assert os.environ["OA_CITY_SPEED"] == "2"
                assert os.environ["OA_STEP_DELAY"] == "0.01"
            game.start.side_effect = start
            launch = stack.enter_context(patch.object(module, "GameProcess", return_value=game))
            stack.enter_context(patch.object(oa_play.Driver, "wait_for"))
            stack.enter_context(patch.object(oa_play.Driver, "status", return_value=oa_play.Status(
                stage="CityView", w=1280, h=720, raw="")))
            stack.enter_context(patch.object(oa_play.Harness, "gs", return_value={}))
            stack.enter_context(patch.object(oa_play.Harness, "ok", return_value=""))
            fight = stack.enter_context(patch.object(oa_play, "_fight_battle", return_value="resolved"))
            ports = None
            if module is not oa_play:
                ports = stack.enter_context(patch.object(module, "free_port", return_value=19001))
            if module is oa_play:
                stack.enter_context(patch.object(module, "play_campaign", side_effect=
                                              lambda d, *args: oa_play.win_battle(d)))
            elif module is oa_skirmish:
                stack.enter_context(patch.object(module, "new_game"))
                def run_one(d, *args, **kw):
                    return {"outcome": oa_play.win_battle(d)}
                rounds = stack.enter_context(patch.object(module, "run_one", side_effect=run_one))
            else:
                cls = module.Campaign if module is oa_campaign else module.Victory
                def run(c, *args):
                    c.start()
                    oa_play.win_battle(c.d)
                    # Reconstruct the driver, as after a dead game: the policy must survive.
                    c.start()
                    oa_play.win_battle(c.d)
                    return 0
                stack.enter_context(patch.object(cls, "run", run))
            assert module.main() == 0
            assert os.environ["OA_CITY_SPEED"] == "2"
            assert os.environ["OA_STEP_DELAY"] == "0.01"
            assert fight.call_count == (2 if module in (oa_campaign, oa_victory) else 1)
            assert all(call.args[2] == {"ai": "skirmisher"} for call in fight.call_args_list)
            if ports:
                ports.assert_called_once()
                assert launch.call_args.args[1] == 19001
            if module is oa_skirmish:
                assert rounds.call_count == 1


def test_victory_port_override():
    with ExitStack() as stack:
        stack.enter_context(patch.object(sys, "argv", [oa_victory.__file__, "--port", "18234"]))
        stack.enter_context(patch.dict(os.environ))
        ports = stack.enter_context(patch.object(oa_victory, "free_port"))
        runner = stack.enter_context(patch.object(oa_victory, "Victory"))
        runner.return_value.run.return_value = 0
        assert oa_victory.main() == 0
        ports.assert_not_called()
        assert runner.call_args.args[2] == 18234


def test_policy_override_and_plugin_tuning():
    d = SimpleNamespace(battle_policy={"ai": "veteran"})
    with patch.object(oa_play, "_fight_battle", return_value="resolved") as fight:
        oa_play.win_battle(d)
        assert fight.call_args.args[2] == {"ai": "veteran"}
        oa_play.win_battle(d, policy={})
        assert fight.call_args.args[2] == {}  # Explicit policy wins, even when empty.
    brain, caps = oa_play.build_battle_ai({"ai": "skirmisher", "break_at": 3.25,
                                          "irrelevant_gene": 123})
    assert brain.break_at == 3.25
    assert caps is not None
    assert oa_play.build_battle_ai({}) == (None, None)
    try:
        oa_play.build_battle_ai({"ai": "not-an-ai"})
    except KeyError:
        pass
    else:
        raise AssertionError("unknown AI names must be rejected")


def test_watch_narration_and_speed_cap():
    d = oa_play.Driver(oa_play.Harness(), Path("data/forms"), verbose=False)
    with patch.dict(os.environ, {}, clear=True), redirect_stdout(StringIO()) as output:
        d.say("quiet decision")
        assert output.getvalue() == ""
        os.environ["OA_WATCH"] = "1"
        d.say("choose a target\nthen send the squad")
        assert output.getvalue() == "choose a target then send the squad\n"
        assert oa_play.target_fps() == 60
        os.environ["OA_CITY_SPEED"] = "2"
        with patch.object(d.h, "key") as key:
            oa_play.set_speed(d, 5)
            oa_play.set_speed(d, 3)
            oa_play.set_speed(d, 0)
            assert [call.args[0] for call in key.call_args_list] == ["2", "2", "0"]


def test_action_delay_excludes_observations():
    commands = ["key 5", "click 10 10", "control BUTTON_OK", "control LIST item 0 set 2",
                "action equip", "keydown Left Shift", "keyup Left Shift", "move 3 4",
                "down 3 4", "up 3 4"]
    queries = ["status", "gs time", "ui", "controls LIST", "control TEXT_FUNDS get"]
    with patch.dict(os.environ, {"OA_STEP_DELAY": "0.25"}), \
            patch.object(oa_play.socket, "create_connection") as connect, \
            patch.object(oa_play.time, "sleep") as sleep:
        connect.return_value.__enter__.return_value.recv.return_value = b"OK\n"
        h = oa_play.Harness()
        for command in commands + queries:
            h.send(command)
        assert sleep.call_count == len(commands)
        assert all(call.args == (0.25,) for call in sleep.call_args_list)
        os.environ["OA_STEP_DELAY"] = "0"
        h.key("5")
        assert sleep.call_count == len(commands)


def test_equipment_affordability():
    for funds, preview, commit in [(40000, None, None), (100000, "OK TEXT_FUNDS text=60,000", True),
                                   (100000, "OK TEXT_FUNDS text=-100", False),
                                   (100000, "OK TEXT_FUNDS text=39999", False),
                                   (100000, "ERR no preview", False)]:
        d = Mock()
        d.h.gs.return_value = {"balance": str(funds)}
        d.h.send.return_value = preview
        with patch.object(oa_play, "open_buysell", return_value=True) as opened, \
                patch.object(oa_play, "buy_category", return_value=2), \
                patch.object(oa_play, "close_buysell") as closed:
            oa_play.buy_equipment(d)
            if commit is None:
                opened.assert_not_called()
                closed.assert_not_called()
            else:
                closed.assert_called_once_with(d, commit=commit)


def test_dead_defs_ignores_documentation_and_detects_indirect_use():
    with TemporaryDirectory() as tmp:
        directory = Path(tmp)
        (directory / "oa_sample.py").write_text('''
def unused():
    return unused()
def documented():
    pass
def direct():
    pass
def indirect():
    pass
class Widget:
    pass
def imported():
    pass
''')
        (directory / "consumer.py").write_text('''
"""documented is mentioned only in a docstring."""
# unused is mentioned only in a comment.
from oa_sample import imported as alias
import oa_sample
x = oa_sample.direct()
y = getattr(oa_sample, "indirect")
z = oa_sample.Widget()
''')
        assert {name for _, _, name in check_dead_defs.dead_definitions(directory)} == {
            "unused", "documented"}


def test_runner_classes_define_every_method_they_call():
    """self.record() was called four times in Victory and never defined: each call raised
    AttributeError, swallowed by the run loop, at exactly the endgame milestones. Fail on any
    self.<name>(...) in a runner class that the class (or its bases) does not provide."""
    import ast
    import inspect
    for module, cls in ((oa_victory, oa_victory.Victory), (oa_campaign, oa_campaign.Campaign)):
        tree = ast.parse(inspect.getsource(cls))
        called = {n.func.attr for n in ast.walk(tree)
                  if isinstance(n, ast.Call) and isinstance(n.func, ast.Attribute)
                  and isinstance(n.func.value, ast.Name) and n.func.value.id == "self"}
        assigned = {t.attr for n in ast.walk(tree) if isinstance(n, ast.Assign)
                    for t in n.targets if isinstance(t, ast.Attribute)
                    and isinstance(t.value, ast.Name) and t.value.id == "self"}
        missing = sorted(c for c in called if not hasattr(cls, c) and c not in assigned)
        assert not missing, f"{cls.__name__} calls undefined methods: {missing}"


if __name__ == "__main__":
    for name, test in list(globals().items()):
        if name.startswith("test_"):
            test()
    print("all runner entrypoint tests passed")
