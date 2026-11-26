"""Pick a model for a prompt.

Priority:
  1. an explicit `@alias` at the start of the prompt (e.g. "@local open Notepad")
  2. the first `routes` rule whose regex matches the prompt
  3. `default`
`fallback` lists the models to try next if the chosen one fails (API errors, refusals).

Example config (YAML):

    models:
      claude: {provider: anthropic, model: claude-opus-5-5, effort: high}
      local:  {provider: openai_compat, base_url: "http://localhost:11434/v1", model: "qwen2.5vl:7b"}
    routes:
      - {match: "install|تثبيت|ثبّت", model: claude}
      - {match: "notepad|اكتب", model: local}
    default: claude
    fallback: [claude, local]
"""

from __future__ import annotations

import os
import re
from dataclasses import dataclass, field
from typing import Any

import yaml

from .base import ModelAdapter

_ALIAS = re.compile(r"^\s*@([\w.-]+)\s*")


@dataclass
class Route:
    pattern: re.Pattern[str]
    model: str


@dataclass
class RouterConfig:
    models: dict[str, dict[str, Any]]
    routes: list[Route] = field(default_factory=list)
    default: str | None = None
    fallback: list[str] = field(default_factory=list)

    @classmethod
    def from_dict(cls, d: dict[str, Any]) -> "RouterConfig":
        models = d.get("models") or {}
        if not models:
            raise ValueError("config has no models")
        routes = [Route(re.compile(r["match"], re.IGNORECASE), r["model"]) for r in d.get("routes") or []]
        default = d.get("default") or next(iter(models))
        cfg = cls(models, routes, default, list(d.get("fallback") or []))
        for name in [default, *cfg.fallback, *(r.model for r in routes)]:
            if name not in models:
                raise ValueError(f"route/default/fallback refers to unknown model {name!r}")
        return cfg

    @classmethod
    def load(cls, path: str) -> "RouterConfig":
        with open(path, encoding="utf-8") as f:
            return cls.from_dict(yaml.safe_load(f) or {})


@dataclass
class Decision:
    model: str
    prompt: str          # prompt with any @alias removed
    reason: str
    chain: list[str]     # model first, then fallbacks (deduplicated)


class Router:
    def __init__(self, config: RouterConfig):
        self.config = config

    def route(self, prompt: str, override: str | None = None) -> Decision:
        cfg = self.config
        text = prompt
        if override:
            if override not in cfg.models:
                raise ValueError(f"unknown model {override!r}; known: {', '.join(cfg.models)}")
            model, reason = override, "chosen explicitly (--model)"
        elif (m := _ALIAS.match(prompt)) and m.group(1) in cfg.models:
            model, reason, text = m.group(1), f"@{m.group(1)} in prompt", prompt[m.end():]
        else:
            for r in cfg.routes:
                if r.pattern.search(prompt):
                    model, reason = r.model, f"rule /{r.pattern.pattern}/"
                    break
            else:
                model, reason = cfg.default, "default"
        chain = [model] + [f for f in cfg.fallback if f != model]
        return Decision(model, text.strip(), reason, chain)

    def build(self, name: str) -> ModelAdapter:
        return build_adapter(name, self.config.models[name])


def build_adapter(name: str, spec: dict[str, Any]) -> ModelAdapter:
    spec = dict(spec)
    provider = spec.pop("provider")
    if "api_key_env" in spec and provider == "anthropic":
        spec["api_key"] = os.environ.get(spec.pop("api_key_env"))
    if provider == "anthropic":
        from .anthropic_adapter import AnthropicAdapter

        return AnthropicAdapter(name=name, **spec)
    if provider in ("openai_compat", "openai", "ollama", "vllm"):
        from .openai_compat import OpenAICompatAdapter

        return OpenAICompatAdapter(name=name, **spec)
    raise ValueError(f"unknown provider {provider!r} for model {name!r}")
