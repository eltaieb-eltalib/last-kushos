"""Claude adapter: uses Anthropic's native computer-use toolset (`computer_toolset_20260801`).

The toolset entry takes no name and no display size; Claude's calls arrive as `tool_use` blocks
whose `name` is the action and which carry `toolset_name: "computer"`, possibly several per turn.
Each result must echo `toolset_name`. Screenshots we return are already sized to the model limits.
"""

from __future__ import annotations

from typing import Any

from ..tools.computer import ToolResult
from .base import Action, ModelAdapter, ModelError, Step

TOOLSET = "computer_toolset_20260801"
FALLBACK_BETA = "server-side-fallback-2026-07-01"


class AnthropicAdapter(ModelAdapter):
    def __init__(self, model: str = "claude-opus-5-5", effort: str = "high", max_tokens: int = 16000,
                 fallbacks: bool = True, enable_zoom: bool = True, client: Any | None = None,
                 api_key: str | None = None, base_url: str | None = None, name: str = "claude"):
        if client is None:
            import anthropic

            kwargs: dict[str, Any] = {}
            if api_key:
                kwargs["api_key"] = api_key
            if base_url:
                kwargs["base_url"] = base_url
            client = anthropic.Anthropic(**kwargs)
        self.client = client
        self.model = model
        self.effort = effort
        self.max_tokens = max_tokens
        self.fallbacks = fallbacks
        self.enable_zoom = enable_zoom
        self.name = name
        self.system = ""
        self.messages: list[dict[str, Any]] = []
        self._toolset_of: dict[str, str | None] = {}

    def start(self, task: str, system: str) -> None:
        self.system = system
        self.messages = [{"role": "user", "content": task}]

    def add_user_message(self, text: str) -> None:
        self.messages.append({"role": "user", "content": text})

    def _tools(self) -> list[dict[str, Any]]:
        entry: dict[str, Any] = {"type": TOOLSET}
        if not self.enable_zoom:
            entry["configs"] = {"zoom": {"enabled": False}}
        return [entry]

    def _create(self):
        params: dict[str, Any] = dict(
            model=self.model,
            max_tokens=self.max_tokens,
            system=self.system,
            tools=self._tools(),
            messages=self.messages,
            output_config={"effort": self.effort},
        )
        if self.fallbacks:
            # On a safety decline the API re-runs the turn on a fallback model chosen by category.
            return self.client.beta.messages.create(betas=[FALLBACK_BETA], fallbacks="default", **params)
        return self.client.messages.create(**params)

    def step(self) -> Step:
        try:
            resp = self._create()
        except Exception as e:  # network, rate limit, bad request ...
            raise ModelError(f"{self.name}: {type(e).__name__}: {e}") from e

        # Pass every block back unchanged (thinking, fallback and tool_use blocks included).
        self.messages.append({"role": "assistant", "content": resp.content})
        usage = {}
        if getattr(resp, "usage", None) is not None:
            usage = {"input_tokens": getattr(resp.usage, "input_tokens", 0),
                     "output_tokens": getattr(resp.usage, "output_tokens", 0)}

        texts, actions = [], []
        for block in resp.content:
            btype = getattr(block, "type", None)
            if btype == "text":
                texts.append(block.text)
            elif btype == "thinking" and getattr(block, "thinking", ""):
                texts.append(block.thinking)
            elif btype == "tool_use":
                self._toolset_of[block.id] = getattr(block, "toolset_name", None)
                actions.append(Action(block.id, block.name, dict(block.input or {})))
        text = "\n".join(t for t in texts if t)

        if resp.stop_reason == "refusal":
            details = getattr(resp, "stop_details", None)
            cat = getattr(details, "category", None) if details else None
            return Step(text=text, done=True, error=f"model declined (refusal, category={cat})", usage=usage)
        if resp.stop_reason == "max_tokens" and not actions:
            return Step(text=text, done=True, error="response truncated (max_tokens)", usage=usage)
        if resp.stop_reason == "pause_turn":
            return Step(text=text, usage=usage)  # continue the same turn on the next step()
        return Step(actions=actions, text=text, done=not actions, usage=usage)

    def add_results(self, results: list[tuple[Action, ToolResult]]) -> None:
        blocks = []
        for action, res in results:
            block: dict[str, Any] = {"type": "tool_result", "tool_use_id": action.id}
            toolset = self._toolset_of.get(action.id)
            if toolset:
                block["toolset_name"] = toolset
            if res.is_error:
                block["is_error"] = True
                block["content"] = res.text or "error"
            elif res.image is not None:
                block["content"] = [{"type": "image", "source": {
                    "type": "base64", "media_type": "image/png", "data": res.image.b64()}}]
            else:
                block["content"] = [{"type": "text", "text": res.text or "OK"}]
            blocks.append(block)
        if blocks:
            self.messages.append({"role": "user", "content": blocks})
