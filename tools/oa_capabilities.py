#!/usr/bin/env python3
"""What the harness CAN DO. No decisions live here.

This is one half of a deliberate split:

    oa_capabilities.py   what is mechanically possible   (this file)
    oa_ai.py             what we choose to do about it   (pure, no engine)
    oa_executor.py       the bridge, plus plugin loading

Every method below is a *capability*: press this control, read that state. None of them decides
anything. If a method contains a threshold, a preference or a priority, it is in the wrong file --
that belongs to an AI. The test for whether code belongs here is simple: could a completely
different AI, with opposite doctrine, still want this exact method? If no, it is a decision.

Why the split exists: the battle loop had grown so that "read the screen", "decide what to do" and
"click the thing" were the same forty lines. That made the AI impossible to test without a booted
game, impossible to compare against another AI, and impossible to replace. It also hid real bugs --
several capabilities were written, documented and never called by anything, and nobody noticed for
weeks because there was no inventory of what the harness could do.

That inventory is now this file.
"""

from __future__ import annotations

import time
from typing import Optional

import oa_play as P


class Capabilities:
    """The harness's mechanical surface, as one object an AI-executor can hold.

    Wraps a Driver. Every call is a thin, named pass-through to the underlying control press or
    state query, so the set of things the harness can do is readable in one place instead of
    scattered across four thousand lines.
    """

    def __init__(self, d):
        self.d = d

    # -- observation -------------------------------------------------------
    # Everything here is information a human player has on screen. Nothing reads a field the UI
    # does not show; that constraint is the whole reason the runs mean anything.

    def stage(self) -> str:
        return self.d.status().stage

    def battle_state(self) -> dict:
        try:
            return self.d.h.gs("battle") or {}
        except Exception:
            return {}

    def battle_positions(self) -> dict:
        try:
            return self.d.h.gs("battle_positions") or {}
        except Exception:
            return {}

    def enemies_on_screen(self) -> list:
        try:
            return self.d.h.screen_craft("enemies_screen") or []
        except Exception:
            return []

    def friends_on_screen(self) -> list:
        try:
            return self.d.h.screen_craft("friends_screen") or []
        except Exception:
            return []

    # -- unit control ------------------------------------------------------

    def set_fire_mode(self, mode: str) -> bool:
        return P.set_fire_mode(self.d, mode)

    def set_stance(self, stance: str) -> bool:
        return P.set_stance(self.d, stance)

    def set_behaviour(self, mode: str) -> bool:
        return P.set_behaviour(self.d, mode)

    def set_move_mode(self, mode: str) -> bool:
        return P.set_move_mode(self.d, mode)

    def set_reserve(self, mode: str) -> bool:
        return P.set_reserve(self.d, mode)

    def set_layer(self, level: int) -> bool:
        return P.set_layer(self.d, level)

    def cease_fire(self, hold: bool = True) -> bool:
        return P.cease_fire(self.d, hold)

    # -- view --------------------------------------------------------------

    def zoom_to_event(self) -> int:
        return P.zoom_to_event(self.d)

    def show_floor(self, z: int) -> None:
        P.show_floor(self.d, z)

    # -- orders ------------------------------------------------------------

    def select_units(self, n: int) -> int:
        """Ctrl-click up to n friendly units currently on screen. Returns how many were clicked."""
        friends = self.friends_on_screen()[:n]
        if not friends:
            return 0
        self.d.h.send("keydown Left Ctrl")
        try:
            P.settle(self.d)
            for fx, fy, *_ in friends:
                self.d.h.click_xy(fx, fy)
                time.sleep(0.06)
        finally:
            self.d.h.send("keyup Left Ctrl")
        return len(friends)

    def attack_at(self, sx: int, sy: int) -> bool:
        """Shift-click: FireAny, so the click resolves to a shot rather than a move order.

        A plain left click on an occupied tile cannot become a move at all, which is why an
        unshifted driver only ever WALKED and never engaged anything it could not path to.
        """
        try:
            self.d.h.send("keydown Left Shift")
            P.settle(self.d)
            self.d.h.click_xy(sx, sy)
            return True
        except Exception:
            return False
        finally:
            try:
                self.d.h.send("keyup Left Shift")
            except Exception:
                pass

    def move_to(self, sx: int, sy: int) -> bool:
        try:
            self.d.h.click_xy(sx, sy)
            return True
        except Exception:
            return False

    def sweep(self, step: int) -> bool:
        """Send the selected units to a point on a search pattern, a different one each step.

        Two orders per call, to opposite quadrants, because the squad hunts as individuals: one
        destination sends everyone to the same corner, which searches one room with eight soldiers
        and leaves the rest of the map alone.

        The pattern walks a 5x5 grid of the viewport using steps that are coprime with it (7 and
        11 against 25), so successive calls land far apart and the whole grid is covered before
        anything repeats -- rather than the neat rows a naive x+1 sweep produces, which re-search
        the same wall for twenty rounds.
        """
        try:
            st = self.d.status()
            if not st.w or not st.h:
                return False
            cells = 5
            gx, gy = (step * 7) % cells, (step * 11) % cells
            x = int(st.w * (gx + 0.5) / cells)
            y = int(st.h * (gy + 0.5) / cells)
            self.d.h.click_xy(max(0, min(st.w - 1, x)), max(0, min(st.h - 1, y)))
            time.sleep(0.2)
            # The mirrored point, so the squad splits rather than clumping on one destination.
            self.d.h.click_xy(max(0, min(st.w - 1, st.w - x)), max(0, min(st.h - 1, st.h - y)))
            return True
        except Exception:
            return False

    def withdraw(self) -> bool:
        """Leave the battle. Note the cost: survivors of a building raid go back into that same
        building, so this hands the aliens their position back."""
        try:
            return P._press(self.d, "BUTTON_OK")
        except Exception:
            return False
