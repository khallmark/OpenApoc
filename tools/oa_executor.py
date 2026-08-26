#!/usr/bin/env python3
"""The bridge: observe -> ask an AI -> execute. Plus plugin loading.

    oa_capabilities.py   what is mechanically possible
    oa_ai.py             what we choose to do about it (pure)
    oa_executor.py       this file -- turns Actions into capability calls

Nothing here decides anything either. It builds an Observation from what the harness can see,
hands it to whichever AI is loaded, and dispatches the returned Actions. Swapping the AI changes
how X-COM fights; it does not touch this file.

PLUGGING IN YOUR OWN X-COM AI
-----------------------------
Drop a file in tools/ai_plugins/ that defines a subclass of oa_ai.TacticalAI:

    # tools/ai_plugins/my_doctrine.py
    from oa_ai import TacticalAI, Action

    class MyDoctrine(TacticalAI):
        name = "my_doctrine"
        def decide(self, obs):
            if obs.foes_alive > obs.mine_alive:
                return [Action("set_behaviour", "evasive", "outnumbered")]
            return [Action("focus_fire", (obs.foes[0].x, obs.foes[0].y, obs.foes[0].z), "engage")]

Then run with `--ai my_doctrine`. Discovery is by class attribute `name`, so the filename does not
matter and one file may define several. Nothing needs registering by hand.

Because Observation and Action are plain data and the AI touches no sockets, a plugin is testable
with no game running at all -- see tools/test_oa_ai.py, which exercises thirty doctrine rules in
milliseconds.
"""

from __future__ import annotations

import importlib.util
import inspect
import sys
from pathlib import Path

from oa_ai import REGISTRY, Action, Observation, TacticalAI, Unit

PLUGIN_DIR = Path(__file__).resolve().parent / "ai_plugins"


def load_plugins(verbose: bool = False) -> dict:
    """Import every module in ai_plugins/ and register any TacticalAI subclass it defines."""
    found = dict(REGISTRY)
    if not PLUGIN_DIR.is_dir():
        return found
    for path in sorted(PLUGIN_DIR.glob("*.py")):
        if path.name.startswith("_"):
            continue
        try:
            spec = importlib.util.spec_from_file_location(f"ai_plugins.{path.stem}", path)
            mod = importlib.util.module_from_spec(spec)
            sys.modules[spec.name] = mod
            spec.loader.exec_module(mod)
        except Exception as exc:
            print(f"[ai] plugin {path.name} failed to load: {type(exc).__name__}: {exc}",
                  flush=True)
            continue
        for _, obj in inspect.getmembers(mod, inspect.isclass):
            if issubclass(obj, TacticalAI) and obj is not TacticalAI:
                nm = getattr(obj, "name", None)
                if nm and nm not in found:
                    found[nm] = obj
                    if verbose:
                        print(f"[ai] loaded plugin AI {nm!r} from {path.name}", flush=True)
    return found


def make_ai(name: str, **kw) -> TacticalAI:
    table = load_plugins()
    cls = table.get(name)
    if cls is None:
        raise KeyError(f"unknown AI {name!r}; available: {sorted(table)}")
    return cls(**kw)


def observe(caps, stalls: int = 0, last_event_z=None) -> Observation:
    """Build an Observation from what the harness can see. No interpretation, no judgement."""
    pos = caps.battle_positions()
    st = caps.battle_state()

    def parse(field):
        """Entries are "x,y,z" followed by colon-separated key=value fields.

        The old parser took the FIRST colon field as the unit kind, which meant kind was the
        string "large=0" -- so VeteranAI's priority rules, which look for "popper" and
        "brainsucker" in it, could never have matched anything. Parse the fields by name.
        """
        out = []
        raw = (pos or {}).get(field, "-")
        if not raw or raw == "-":
            return out
        for part in raw.split(";"):
            chunks = part.split(":")
            bits = chunks[0].split(",")
            if len(bits) < 3:
                continue
            fields = {}
            for c in chunks[1:]:
                if "=" in c:
                    key, _, val = c.partition("=")
                    fields[key] = val
            try:
                out.append((int(bits[0]), int(bits[1]), int(bits[2]), fields))
            except ValueError:
                continue
        return out

    def as_int(fields, key):
        try:
            v = int(fields.get(key, -1))
        except ValueError:
            return None
        return None if v < 0 else v

    mine = [Unit(1000 + i, x, y, z, kind=f.get("kind", ""),
                 sx=as_int(f, "sx"), sy=as_int(f, "sy"))
            for i, (x, y, z, f) in enumerate(parse("mine_at"))]
    foes = [Unit(2000 + i, x, y, z, hostile=True, kind=f.get("kind", ""),
                 sx=as_int(f, "sx"), sy=as_int(f, "sy"))
            for i, (x, y, z, f) in enumerate(parse("foe_at"))]
    try:
        view_z = int((pos or {}).get("view_z", 0) or 0)
    except ValueError:
        view_z = 0
    # Are we still in the mission? Battle::checkMissionEnd ends it by itself once no hostile
    # organisation has a conscious unit left, so still being in it means something is still out
    # there. Anything other than an explicit "0" counts as yes: the failure that matters is
    # standing still on a live map, not searching a dead one for a few extra seconds.
    return Observation(mine=mine, foes=foes, view_z=view_z, stalls=stalls,
                       hostiles_remain=str(st.get("in_battle", "1")) != "0",
                       mission_type=st.get("mission_type", "unknown"),
                       mode=st.get("mode", "rt"), last_event_z=last_event_z)


def _screen_of(arg, caps):
    """Screen coordinates for an Action argument, or None if it cannot be resolved.

    Accepts either a ready-made (sx, sy) pair or a (tile_x, tile_y, tile_z) target, which is
    matched against the units battle_positions reports. Returns None rather than guessing: a
    wrong click is worse than a skipped order, and an off-screen unit has sx=-1 by design.
    """
    if not isinstance(arg, (tuple, list)) or len(arg) < 2:
        return None
    try:
        want = tuple(int(v) for v in arg[:3])
    except (TypeError, ValueError):
        return None
    if len(want) < 3:
        return (want[0], want[1])
    obs = observe(caps)
    for u in list(obs.foes) + list(obs.mine):
        if (u.x, u.y, u.z) == want and u.sx is not None and u.sy is not None:
            return (u.sx, u.sy)
    return None


def execute(caps, actions: list, say=None) -> int:
    """Dispatch Actions to capabilities. Returns how many were carried out.

    Unknown action kinds are reported, not silently dropped: a plugin asking for something the
    harness cannot do should hear about it rather than quietly do nothing, which is the failure
    mode that hid four dead capabilities for weeks.
    """
    done = 0
    for a in actions:
        k = a.kind
        try:
            if k == "set_fire_mode":
                ok = caps.set_fire_mode(a.arg)
            elif k == "set_stance":
                ok = caps.set_stance(a.arg)
            elif k == "set_behaviour":
                ok = caps.set_behaviour(a.arg)
            elif k == "set_move_mode":
                ok = caps.set_move_mode(a.arg)
            elif k == "set_reserve":
                ok = caps.set_reserve(a.arg)
            elif k == "set_layer":
                ok = caps.set_layer(a.arg)
            elif k == "cease_fire":
                ok = caps.cease_fire(bool(a.arg))
            elif k == "zoom_event":
                ok = caps.zoom_to_event() >= 0
            elif k == "show_floor":
                caps.show_floor(a.arg); ok = True
            elif k == "select_squad":
                ok = caps.select_units(int(a.arg or 6)) > 0
            elif k in ("attack", "focus_fire"):
                # Shoot the target the AI picked. This used to call attack_at(foes[0]) -- the
                # first hostile on screen -- discarding a.arg entirely, so threat priority was
                # computed every round and thrown away. battle_positions now carries each unit's
                # screen coordinates, so the choice can actually be executed.
                aim = _screen_of(a.arg, caps)
                if aim is None:
                    foes = caps.enemies_on_screen()
                    aim = (foes[0][0], foes[0][1]) if foes else None
                ok = aim is not None and caps.attack_at(aim[0], aim[1])
            elif k == "move":
                aim = _screen_of(a.arg, caps)
                if aim is None:
                    foes = caps.enemies_on_screen()
                    aim = (foes[0][0], foes[0][1]) if foes else None
                ok = aim is not None and caps.move_to(aim[0], aim[1])
            elif k in ("pull_back", "spread"):
                # Per-unit repositioning needs a screen coordinate for that specific unit, which
                # battle_positions gives in TILE space. Declared unsupported rather than faked:
                # a wrong click is worse than a skipped order.
                ok = False
            elif k == "search":
                # Send the squad somewhere it has not been. The AI works in tile space and has no
                # idea how big the window is, so the sweep pattern lives here where the screen is
                # known; the AI supplies only a step counter so successive rounds probe different
                # ground instead of re-clicking one spot.
                ok = caps.sweep(int(a.arg or 0))
            elif k == "withdraw":
                ok = caps.withdraw()
            elif k == "wait":
                ok = True
            else:
                if say:
                    say(f"  [ai] no capability for action {k!r}")
                ok = False
        except Exception as exc:
            if say:
                say(f"  [ai] {k} failed: {type(exc).__name__}: {exc}")
            ok = False
        if ok:
            done += 1
    return done
