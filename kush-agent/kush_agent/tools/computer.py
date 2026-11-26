"""The Tool API: executes computer actions on a QEMU guest through QMP (and the guest agent).

Action names and inputs follow the 17 members of Anthropic's `computer_toolset_20260801`, which
also serves as the provider-neutral action vocabulary for every other model adapter.
Coordinates are always in the pixel space of the last full screenshot returned to the model.
"""

from __future__ import annotations

import os
import tempfile
import time
from dataclasses import dataclass, field
from typing import Any, Callable

from ..vm import keys
from ..vm.qmp import QMPClient
from ..vm.screen import ScreenGeometry, Screenshot, process

ACTIONS = (
    "screenshot", "zoom", "left_click", "right_click", "middle_click", "double_click", "triple_click",
    "left_click_drag", "mouse_move", "left_mouse_down", "left_mouse_up", "cursor_position",
    "scroll", "type", "key", "hold_key", "wait",
)


class ActionError(RuntimeError):
    pass


@dataclass
class ToolResult:
    text: str | None = None
    image: Screenshot | None = None
    is_error: bool = False
    meta: dict[str, Any] = field(default_factory=dict)


class Computer:
    def __init__(self, qmp: QMPClient, guest: Any | None = None, workdir: str | None = None,
                 max_long_edge: int = 1568, settle: float = 0.4, type_delay: float = 0.01,
                 sleep: Callable[[float], None] = time.sleep):
        self.qmp = qmp
        self.guest = guest  # optional GuestClient for Unicode typing etc.
        self.workdir = workdir or tempfile.mkdtemp(prefix="kush-shot-")
        self.max_long_edge = max_long_edge
        self.settle = settle
        self.type_delay = type_delay
        self.sleep = sleep
        self.geometry: ScreenGeometry | None = None
        self.cursor: tuple[int, int] = (0, 0)  # model coordinates

    # -- dispatch -------------------------------------------------------------------
    def run(self, name: str, inp: dict[str, Any]) -> ToolResult:
        if name not in ACTIONS:
            return ToolResult(f"unknown action {name!r}", is_error=True)
        try:
            return getattr(self, f"_a_{name}")(**(inp or {}))
        except TypeError as e:
            return ToolResult(f"bad input for {name}: {e}", is_error=True)
        except (ActionError, keys.KeyError_) as e:
            return ToolResult(str(e), is_error=True)

    # -- screen ---------------------------------------------------------------------
    def capture(self, region: tuple[int, int, int, int] | None = None) -> Screenshot:
        fd, raw = tempfile.mkstemp(suffix=".png", dir=self.workdir)
        os.close(fd)
        try:
            self.qmp.screendump(raw)
            shot = process(raw, self.max_long_edge, region)
        finally:
            os.unlink(raw)
        self.geometry = shot.geometry
        return shot

    def _geo(self) -> ScreenGeometry:
        if self.geometry is None:
            self.capture()
        assert self.geometry is not None
        return self.geometry

    def _a_screenshot(self) -> ToolResult:
        return ToolResult(image=self.capture())

    def _a_zoom(self, region: list[int]) -> ToolResult:
        if len(region) != 4:
            raise ActionError("region must be [x0, y0, x1, y1]")
        g = self._geo()
        x0, y0 = g.to_real(region[0], region[1])
        x1, y1 = g.to_real(region[2], region[3])
        if x1 <= x0 or y1 <= y0:
            raise ActionError("empty zoom region")
        return ToolResult(image=self.capture((x0, y0, x1 + 1, y1 + 1)))

    # -- pointer --------------------------------------------------------------------
    def _move_events(self, x: float, y: float) -> list[dict[str, Any]]:
        ax, ay = self._geo().to_abs(x, y)
        self.cursor = (int(x), int(y))
        return [{"type": "abs", "data": {"axis": "x", "value": ax}},
                {"type": "abs", "data": {"axis": "y", "value": ay}}]

    @staticmethod
    def _btn(button: str, down: bool) -> dict[str, Any]:
        return {"type": "btn", "data": {"down": down, "button": button}}

    def _with_modifiers(self, text: str | None, events: list[dict[str, Any]]) -> None:
        mods = keys.parse_combo(text) if text else []
        if mods:
            self.qmp.input_events(keys.combo_events(mods, hold=True))
        try:
            self.qmp.input_events(events)
        finally:
            if mods:
                self.qmp.input_events(keys.release_events(mods))

    def _click(self, button: str, count: int, coordinate: list[int] | None = None, text: str | None = None) -> ToolResult:
        if coordinate is not None:
            self.qmp.input_events(self._move_events(*coordinate))
        for i in range(count):
            self._with_modifiers(text, [self._btn(button, True), self._btn(button, False)])
            if i + 1 < count:
                self.sleep(0.06)
        self.sleep(self.settle)
        return ToolResult("OK")

    def _a_left_click(self, coordinate=None, text=None):
        return self._click("left", 1, coordinate, text)

    def _a_right_click(self, coordinate=None, text=None):
        return self._click("right", 1, coordinate, text)

    def _a_middle_click(self, coordinate=None, text=None):
        return self._click("middle", 1, coordinate, text)

    def _a_double_click(self, coordinate=None, text=None):
        return self._click("left", 2, coordinate, text)

    def _a_triple_click(self, coordinate=None, text=None):
        return self._click("left", 3, coordinate, text)

    def _a_left_click_drag(self, start_coordinate, coordinate, text=None):
        mods = keys.parse_combo(text) if text else []
        if mods:
            self.qmp.input_events(keys.combo_events(mods, hold=True))
        try:
            self.qmp.input_events(self._move_events(*start_coordinate))
            self.qmp.input_events([self._btn("left", True)])
            sx, sy = start_coordinate
            ex, ey = coordinate
            steps = 10
            for i in range(1, steps + 1):
                self.qmp.input_events(self._move_events(sx + (ex - sx) * i / steps, sy + (ey - sy) * i / steps))
                self.sleep(0.02)
            self.qmp.input_events([self._btn("left", False)])
        finally:
            if mods:
                self.qmp.input_events(keys.release_events(mods))
        self.sleep(self.settle)
        return ToolResult("OK")

    def _a_mouse_move(self, coordinate):
        self.qmp.input_events(self._move_events(*coordinate))
        return ToolResult("OK")

    def _a_left_mouse_down(self):
        self.qmp.input_events([self._btn("left", True)])
        return ToolResult("OK")

    def _a_left_mouse_up(self):
        self.qmp.input_events([self._btn("left", False)])
        return ToolResult("OK")

    def _a_cursor_position(self):
        return ToolResult(f"X={self.cursor[0]}, Y={self.cursor[1]}")

    def _a_scroll(self, scroll_direction: str, scroll_amount: int = 3, coordinate=None, text=None):
        button = {"up": "wheel-up", "down": "wheel-down", "left": "wheel-left", "right": "wheel-right"}.get(scroll_direction)
        if button is None:
            raise ActionError(f"bad scroll_direction {scroll_direction!r}")
        if coordinate is not None:
            self.qmp.input_events(self._move_events(*coordinate))
        for _ in range(max(1, min(int(scroll_amount), 50))):
            self._with_modifiers(text, [self._btn(button, True), self._btn(button, False)])
            self.sleep(0.02)
        self.sleep(self.settle)
        return ToolResult("OK")

    # -- keyboard -------------------------------------------------------------------
    def _a_type(self, text: str):
        if keys.is_typeable(text):
            for batch in keys.text_events(text):
                self.qmp.input_events(batch)
                self.sleep(self.type_delay)
        elif self.guest is not None:
            # Arabic and other Unicode text: typed inside the guest via SendInput(KEYEVENTF_UNICODE).
            self.guest.type_unicode(text)
        else:
            raise ActionError("text contains characters with no US-layout key and no guest agent is connected")
        self.sleep(self.settle)
        return ToolResult("OK")

    def _a_key(self, text: str, repeat: int = 1):
        qcodes = keys.parse_combo(text)
        for _ in range(max(1, min(int(repeat), 100))):
            self.qmp.input_events(keys.combo_events(qcodes))
            self.sleep(0.03)
        self.sleep(self.settle)
        return ToolResult("OK")

    def _a_hold_key(self, text: str, duration: float):
        qcodes = keys.parse_combo(text)
        self.qmp.input_events(keys.combo_events(qcodes, hold=True))
        try:
            self.sleep(min(float(duration), 300))
        finally:
            self.qmp.input_events(keys.release_events(qcodes))
        return ToolResult("OK")

    def _a_wait(self, duration: float = 1.0):
        self.sleep(min(float(duration), 300))
        return ToolResult("OK")
