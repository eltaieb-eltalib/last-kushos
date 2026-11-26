import pytest

from kush_agent.agent.policy import Policy
from kush_agent.models.base import Action
from kush_agent.models.router import Router, RouterConfig

CFG = {
    "models": {
        "claude": {"provider": "anthropic", "model": "claude-opus-5-5"},
        "local": {"provider": "openai_compat", "base_url": "http://x/v1", "model": "m"},
    },
    "routes": [{"match": "install|ثبّت", "model": "claude"}, {"match": "notepad|اكتب", "model": "local"}],
    "default": "claude",
    "fallback": ["claude", "local"],
}


def r():
    return Router(RouterConfig.from_dict(CFG))


def test_alias_wins():
    d = r().route("@local install firefox")
    assert d.model == "local" and d.prompt == "install firefox" and d.chain == ["local", "claude"]


def test_rules_arabic():
    assert r().route("ثبّت فايرفوكس").model == "claude"
    assert r().route("افتح المفكرة و اكتب سلام").model == "local"
    assert r().route("something else").reason == "default"


def test_unknown_alias_is_text():
    d = r().route("@nobody hi")
    assert d.model == "claude" and d.prompt == "@nobody hi"


def test_override_and_validation():
    assert r().route("x", override="local").model == "local"
    with pytest.raises(ValueError):
        r().route("x", override="zzz")
    with pytest.raises(ValueError):
        RouterConfig.from_dict({**CFG, "default": "missing"})


def test_build_adapters():
    rt = r()
    assert type(rt.build("local")).__name__ == "OpenAICompatAdapter"
    assert type(rt.build("claude")).__name__ == "AnthropicAdapter"


def test_policy():
    p = Policy()
    assert p.check(Action("1", "left_click", {"coordinate": [1, 2]})).verdict == "allow"
    assert p.check(Action("1", "key", {"text": "ctrl+alt+Delete"})).verdict == "ask"
    assert p.check(Action("1", "type", {"text": "4111 1111 1111 1111"})).verdict == "ask"
    assert p.check(Action("1", "type", {"text": "format c:"})).verdict == "ask"
    assert p.check(Action("1", "type", {"text": "hello"})).verdict == "allow"
    p2 = Policy.from_dict({"deny_actions": ["left_click_drag"]})
    assert p2.check(Action("1", "left_click_drag", {})).verdict == "deny"
