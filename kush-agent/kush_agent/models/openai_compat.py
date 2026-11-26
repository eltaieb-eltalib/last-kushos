"""Adapter for any OpenAI-compatible Chat Completions endpoint with vision + function calling.

Covers hosted models that expose this API shape and local servers (Ollama, vLLM, LM Studio,
llama.cpp server) running vision models. The computer actions are exposed as ordinary
functions; screenshots go back as an image in a user message because tool messages carry text only.
"""

from __future__ import annotations

import json
import os
from typing import Any

import httpx

from ..tools.computer import ToolResult
from .base import Action, ModelAdapter, ModelError, Step

_XY = {"type": "array", "items": {"type": "integer"}, "minItems": 2, "maxItems": 2,
       "description": "[x, y] in pixels of the latest screenshot"}
_MODS = {"type": "string", "description": "optional modifier keys held during the action, e.g. 'shift' or 'ctrl+shift'"}


def _fn(name: str, desc: str, props: dict[str, Any] | None = None, required: list[str] | None = None) -> dict[str, Any]:
    return {"type": "function", "function": {"name": name, "description": desc, "parameters": {
        "type": "object", "properties": props or {}, "required": required or []}}}


FUNCTIONS = [
    _fn("screenshot", "Capture the full screen."),
    _fn("zoom", "Return a region of the screen at higher detail.", {"region": {
        "type": "array", "items": {"type": "integer"}, "minItems": 4, "maxItems": 4,
        "description": "[x0, y0, x1, y1]"}}, ["region"]),
    *[_fn(n, f"{n.replace('_', ' ')} at a position (or at the cursor).", {"coordinate": _XY, "text": _MODS})
      for n in ("left_click", "right_click", "middle_click", "double_click", "triple_click")],
    _fn("left_click_drag", "Press at start_coordinate, drag to coordinate, release.",
        {"start_coordinate": _XY, "coordinate": _XY, "text": _MODS}, ["start_coordinate", "coordinate"]),
    _fn("mouse_move", "Move the pointer without clicking.", {"coordinate": _XY}, ["coordinate"]),
    _fn("left_mouse_down", "Press the left button at the cursor."),
    _fn("left_mouse_up", "Release the left button at the cursor."),
    _fn("cursor_position", "Return the cursor position."),
    _fn("scroll", "Scroll the mouse wheel.", {
        "scroll_direction": {"type": "string", "enum": ["up", "down", "left", "right"]},
        "scroll_amount": {"type": "integer", "description": "wheel clicks"},
        "coordinate": _XY, "text": _MODS}, ["scroll_direction", "scroll_amount"]),
    _fn("type", "Type text at the keyboard focus.", {"text": {"type": "string"}}, ["text"]),
    _fn("key", "Press a key or combination, e.g. 'Return', 'ctrl+s', 'alt+Tab'.",
        {"text": {"type": "string"}, "repeat": {"type": "integer"}}, ["text"]),
    _fn("hold_key", "Hold a key for some seconds.", {"text": {"type": "string"}, "duration": {"type": "number"}},
        ["text", "duration"]),
    _fn("wait", "Wait some seconds.", {"duration": {"type": "number"}}, ["duration"]),
]


class OpenAICompatAdapter(ModelAdapter):
    def __init__(self, base_url: str, model: str, api_key: str | None = None, api_key_env: str | None = None,
                 max_tokens: int = 4096, keep_images: int = 3, timeout: float = 120.0,
                 http: httpx.Client | None = None, name: str = "openai_compat"):
        self.base_url = base_url.rstrip("/")
        self.model = model
        self.api_key = api_key or (os.environ.get(api_key_env) if api_key_env else None)
        self.max_tokens = max_tokens
        self.keep_images = keep_images
        self.http = http or httpx.Client(timeout=timeout)
        self.name = name
        self.messages: list[dict[str, Any]] = []

    def start(self, task: str, system: str) -> None:
        self.messages = [{"role": "system", "content": system}, {"role": "user", "content": task}]

    def add_user_message(self, text: str) -> None:
        self.messages.append({"role": "user", "content": text})

    def _prune_images(self) -> None:
        """We own this history format, so old screenshots can be replaced to bound the context."""
        seen = 0
        for msg in reversed(self.messages):
            if msg["role"] != "user" or not isinstance(msg.get("content"), list):
                continue
            for part in msg["content"]:
                if part.get("type") == "image_url":
                    seen += 1
                    if seen > self.keep_images:
                        part.clear()
                        part.update({"type": "text", "text": "[older screenshot removed]"})

    def step(self) -> Step:
        self._prune_images()
        headers = {"Content-Type": "application/json"}
        if self.api_key:
            headers["Authorization"] = f"Bearer {self.api_key}"
        body = {"model": self.model, "messages": self.messages, "tools": FUNCTIONS,
                "tool_choice": "auto", "max_tokens": self.max_tokens}
        try:
            r = self.http.post(f"{self.base_url}/chat/completions", headers=headers, json=body)
            r.raise_for_status()
            data = r.json()
        except Exception as e:
            raise ModelError(f"{self.name}: {type(e).__name__}: {e}") from e
        choice = data["choices"][0]
        msg = choice["message"]
        self.messages.append({k: v for k, v in msg.items() if k in ("role", "content", "tool_calls") and v is not None})
        actions = []
        for call in msg.get("tool_calls") or []:
            try:
                args = json.loads(call["function"].get("arguments") or "{}")
            except json.JSONDecodeError:
                args = {"__invalid_json__": call["function"].get("arguments")}
            actions.append(Action(call["id"], call["function"]["name"], args))
        usage = data.get("usage") or {}
        err = "response truncated (length)" if choice.get("finish_reason") == "length" and not actions else None
        return Step(actions=actions, text=msg.get("content") or "", done=not actions, error=err, usage=usage)

    def add_results(self, results: list[tuple[Action, ToolResult]]) -> None:
        images = []
        for action, res in results:
            if res.image is not None and not res.is_error:
                content = "screenshot attached below"
                images.append(res.image)
            else:
                content = ("ERROR: " if res.is_error else "") + (res.text or "OK")
            self.messages.append({"role": "tool", "tool_call_id": action.id, "content": content})
        if images:
            self.messages.append({"role": "user", "content": [
                {"type": "image_url", "image_url": {"url": f"data:image/png;base64,{img.b64()}"}} for img in images]})
