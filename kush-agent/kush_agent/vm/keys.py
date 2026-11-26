"""Translate key names (xdotool style, as used by the computer-use toolset) and text into QEMU qcodes.

QEMU's `input-send-event` sends *physical keys* (qcodes), not characters, so typing text means
mapping each character to a key (+ shift) on a US keyboard layout. Characters with no key on that
layout (Arabic, emoji, ...) cannot be typed this way; they go through the guest agent instead.
"""

from __future__ import annotations

from typing import Any

# Named keys -> qcode. Lookup is case-insensitive.
_NAMED: dict[str, str] = {
    "return": "ret", "enter": "ret", "kp_enter": "kp_enter",
    "escape": "esc", "esc": "esc",
    "backspace": "backspace", "back_space": "backspace",
    "tab": "tab", "space": "spc",
    "delete": "delete", "del": "delete", "insert": "insert",
    "home": "home", "end": "end",
    "page_up": "pgup", "pageup": "pgup", "prior": "pgup",
    "page_down": "pgdn", "pagedown": "pgdn", "next": "pgdn",
    "up": "up", "down": "down", "left": "left", "right": "right",
    "shift": "shift", "shift_l": "shift", "shift_r": "shift_r",
    "ctrl": "ctrl", "control": "ctrl", "control_l": "ctrl", "ctrl_l": "ctrl",
    "control_r": "ctrl_r", "ctrl_r": "ctrl_r",
    "alt": "alt", "alt_l": "alt", "alt_r": "alt_r", "altgr": "alt_r",
    "super": "meta_l", "super_l": "meta_l", "super_r": "meta_r", "win": "meta_l",
    "meta": "meta_l", "cmd": "meta_l",
    "caps_lock": "caps_lock", "num_lock": "num_lock", "scroll_lock": "scroll_lock",
    "print": "print", "printscreen": "print", "sysrq": "sysrq",
    "pause": "pause", "menu": "menu",
    "minus": "minus", "equal": "equal", "plus": "kp_add",
    "comma": "comma", "period": "dot", "dot": "dot", "slash": "slash",
    "backslash": "backslash", "semicolon": "semicolon", "apostrophe": "apostrophe",
    "grave": "grave_accent", "bracketleft": "bracket_left", "bracketright": "bracket_right",
}
for _i in range(1, 13):
    _NAMED[f"f{_i}"] = f"f{_i}"

# US layout: character -> (qcode, needs_shift)
_CHARS: dict[str, tuple[str, bool]] = {" ": ("spc", False), "\n": ("ret", False), "\t": ("tab", False)}
for _c in "abcdefghijklmnopqrstuvwxyz":
    _CHARS[_c] = (_c, False)
    _CHARS[_c.upper()] = (_c, True)
for _c in "0123456789":
    _CHARS[_c] = (_c, False)
for _c, _q in zip(")!@#$%^&*(", "0123456789"):
    _CHARS[_c] = (_q, True)
for _plain, _shifted, _q in [
    ("-", "_", "minus"), ("=", "+", "equal"), ("[", "{", "bracket_left"), ("]", "}", "bracket_right"),
    ("\\", "|", "backslash"), (";", ":", "semicolon"), ("'", '"', "apostrophe"),
    ("`", "~", "grave_accent"), (",", "<", "comma"), (".", ">", "dot"), ("/", "?", "slash"),
]:
    _CHARS[_plain] = (_q, False)
    _CHARS[_shifted] = (_q, True)

MODIFIERS = {"shift", "shift_r", "ctrl", "ctrl_r", "alt", "alt_r", "meta_l", "meta_r"}


class KeyError_(ValueError):
    pass


def key_to_qcode(name: str) -> str:
    n = name.strip()
    if len(n) == 1:
        if n in _CHARS:
            return _CHARS[n][0]
        raise KeyError_(f"no US-layout key for {n!r}")
    q = _NAMED.get(n.lower())
    if q is None:
        raise KeyError_(f"unknown key name {name!r}")
    return q


def parse_combo(combo: str) -> list[str]:
    """'ctrl+shift+Escape' -> ['ctrl', 'shift', 'esc']. A lone '+' is the plus key."""
    if combo == "+":
        return [key_to_qcode("+")]
    parts = [p for p in combo.split("+") if p != ""]
    if not parts:
        raise KeyError_("empty key combination")
    return [key_to_qcode(p) for p in parts]


def is_typeable(text: str) -> bool:
    return all(c in _CHARS for c in text)


def _key_event(qcode: str, down: bool) -> dict[str, Any]:
    return {"type": "key", "data": {"down": down, "key": {"type": "qcode", "data": qcode}}}


def combo_events(qcodes: list[str], hold: bool = False) -> list[dict[str, Any]]:
    """Press all keys in order, release in reverse order (or only press when hold=True)."""
    events = [_key_event(q, True) for q in qcodes]
    if not hold:
        events += [_key_event(q, False) for q in reversed(qcodes)]
    return events


def release_events(qcodes: list[str]) -> list[dict[str, Any]]:
    return [_key_event(q, False) for q in reversed(qcodes)]


def text_events(text: str) -> list[list[dict[str, Any]]]:
    """One event batch per character (so callers can pace them)."""
    batches = []
    for ch in text:
        if ch == "\r":
            continue
        if ch not in _CHARS:
            raise KeyError_(f"character {ch!r} cannot be typed with QMP key events; use the guest agent")
        q, shift = _CHARS[ch]
        batches.append(combo_events(["shift", q] if shift else [q]))
    return batches
