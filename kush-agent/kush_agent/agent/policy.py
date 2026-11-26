"""Policy engine: every action from every model passes through here before it touches the VM.

Decisions: allow | ask (needs human approval) | deny. Rules come from YAML so operators can
tighten them without code changes. Text visible on screen is never a source of permission.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Any, Callable

from ..models.base import Action

DEFAULT_RULES: dict[str, Any] = {
    "deny_actions": [],
    "ask_actions": [],
    # key combinations that need approval (normalised to lower case, '+'-joined)
    "ask_keys": ["ctrl+alt+delete", "ctrl+alt+del"],
    # regexes on typed text that need approval (card numbers, destructive commands, ...)
    "ask_text_patterns": [
        r"\b(?:\d[ -]?){13,19}\b",                     # payment card numbers
        r"(?i)\b(format|diskpart|del\s+/[sq]|rd\s+/s|rmdir\s+/s|shutdown)\b",
        r"(?i)\bpassword\b|كلمة\s*(?:ال)?مرور",
    ],
    "deny_text_patterns": [],
}


@dataclass
class Decision:
    verdict: str  # allow | ask | deny
    reason: str = ""


@dataclass
class Policy:
    rules: dict[str, Any] = field(default_factory=lambda: dict(DEFAULT_RULES))

    @classmethod
    def from_dict(cls, d: dict[str, Any] | None) -> "Policy":
        rules = dict(DEFAULT_RULES)
        rules.update(d or {})
        return cls(rules)

    def check(self, action: Action) -> Decision:
        r = self.rules
        if action.name in r["deny_actions"]:
            return Decision("deny", f"action {action.name} is disabled by policy")
        text = str(action.input.get("text") or "")
        if action.name == "type":
            for p in r["deny_text_patterns"]:
                if re.search(p, text):
                    return Decision("deny", f"typed text matches a forbidden pattern ({p})")
            for p in r["ask_text_patterns"]:
                if re.search(p, text):
                    return Decision("ask", f"typed text looks sensitive ({p})")
        if action.name in ("key", "hold_key"):
            combo = "+".join(part.strip().lower() for part in text.split("+"))
            if combo in r["ask_keys"]:
                return Decision("ask", f"key combination {combo}")
        if action.name in r["ask_actions"]:
            return Decision("ask", f"action {action.name} requires approval")
        return Decision("allow")


Approver = Callable[[Action, Decision], bool]


def deny_all(action: Action, decision: Decision) -> bool:
    return False


def approve_all(action: Action, decision: Decision) -> bool:
    return True


def terminal_approver(action: Action, decision: Decision) -> bool:
    try:
        ans = input(f"\n[approval] {action.name} {action.input} — {decision.reason}. Allow? [y/N] ")
    except EOFError:
        return False
    return ans.strip().lower() in ("y", "yes", "نعم", "ا")
