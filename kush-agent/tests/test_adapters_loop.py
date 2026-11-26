import json
from types import SimpleNamespace as NS

import httpx
from PIL import Image

from kush_agent.agent.loop import Agent
from kush_agent.agent.policy import Policy, approve_all
from kush_agent.models.anthropic_adapter import AnthropicAdapter
from kush_agent.models.base import NOT_EXECUTED, Action, ModelError
from kush_agent.models.openai_compat import OpenAICompatAdapter
from kush_agent.tools.computer import ToolResult
from kush_agent.vm.screen import process


def shot(tmp_path):
    p = tmp_path / "s.png"
    Image.new("RGB", (640, 480), "blue").save(p)
    return process(str(p))


class FakeComputer:
    def __init__(self, image, fail_on=None):
        self.calls, self.image, self.fail_on = [], image, fail_on

    def run(self, name, inp):
        self.calls.append((name, inp))
        if name == self.fail_on:
            return ToolResult("boom", is_error=True)
        return ToolResult(image=self.image) if name == "screenshot" else ToolResult("OK")


# --- Anthropic ------------------------------------------------------------------------
class FakeMessages:
    def __init__(self, responses):
        self.responses, self.calls = list(responses), []

    def create(self, **kw):
        self.calls.append({**kw, "messages": list(kw["messages"])})
        return self.responses.pop(0)


def tool_use(i, name, inp):
    return NS(type="tool_use", id=i, name=name, input=inp, toolset_name="computer")


def anthropic_client(responses):
    m = FakeMessages(responses)
    return NS(messages=m, beta=NS(messages=m)), m


def test_anthropic_adapter_flow(tmp_path):
    client, m = anthropic_client([
        NS(content=[NS(type="text", text="looking"), tool_use("t1", "screenshot", {}),
                    tool_use("t2", "left_click", {"coordinate": [5, 5]})], stop_reason="tool_use", usage=None),
        NS(content=[NS(type="text", text="All done")], stop_reason="end_turn", usage=None),
    ])
    ad = AnthropicAdapter(client=client)
    comp = FakeComputer(shot(tmp_path))
    rep = Agent(comp, run_dir=str(tmp_path / "run")).run("open notepad", [ad])
    assert rep.status == "done" and rep.final_text == "All done" and rep.actions == 2
    first = m.calls[0]
    assert first["tools"] == [{"type": "computer_toolset_20260801"}]
    assert first["fallbacks"] == "default" and first["betas"] == ["server-side-fallback-2026-07-01"]
    results = m.calls[1]["messages"][-1]["content"]
    assert all(r["toolset_name"] == "computer" for r in results)
    assert results[0]["content"][0]["type"] == "image"
    assert results[1]["content"] == [{"type": "text", "text": "OK"}]
    lines = (tmp_path / "run" / "steps.jsonl").read_text().splitlines()
    assert json.loads(lines[-1])["status"] == "done"
    assert (tmp_path / "run" / "shot-0001.png").exists()


def test_batch_stops_after_failure(tmp_path):
    client, m = anthropic_client([
        NS(content=[tool_use("a", "left_click", {}), tool_use("b", "type", {"text": "x"})], stop_reason="tool_use", usage=None),
        NS(content=[NS(type="text", text="ok")], stop_reason="end_turn", usage=None),
    ])
    comp = FakeComputer(shot(tmp_path), fail_on="left_click")
    Agent(comp).run("t", [AnthropicAdapter(client=client, fallbacks=False)])
    results = m.calls[1]["messages"][-1]["content"]
    assert results[0]["is_error"] and results[1]["content"] == NOT_EXECUTED
    assert comp.calls == [("left_click", {})]


def test_policy_blocks_without_approval(tmp_path):
    client, m = anthropic_client([
        NS(content=[tool_use("a", "key", {"text": "ctrl+alt+Delete"})], stop_reason="tool_use", usage=None),
        NS(content=[NS(type="text", text="ok")], stop_reason="end_turn", usage=None),
    ])
    comp = FakeComputer(shot(tmp_path))
    Agent(comp, Policy()).run("t", [AnthropicAdapter(client=client)])
    assert comp.calls == [] and "Blocked by policy" in m.calls[1]["messages"][-1]["content"][0]["content"]
    client, m = anthropic_client([
        NS(content=[tool_use("a", "key", {"text": "ctrl+alt+Delete"})], stop_reason="tool_use", usage=None),
        NS(content=[NS(type="text", text="ok")], stop_reason="end_turn", usage=None),
    ])
    comp = FakeComputer(shot(tmp_path))
    Agent(comp, Policy(), approve_all).run("t", [AnthropicAdapter(client=client)])
    assert comp.calls == [("key", {"text": "ctrl+alt+Delete"})]


class Broken:
    name = "broken"

    def start(self, task, system):
        pass

    def step(self):
        raise ModelError("api down")


def test_fallback_to_next_model(tmp_path):
    client, m = anthropic_client([NS(content=[NS(type="text", text="done")], stop_reason="end_turn", usage=None)])
    rep = Agent(FakeComputer(shot(tmp_path))).run("t", [Broken(), AnthropicAdapter(client=client)])
    assert rep.status == "done" and rep.models_used == ["broken", "claude"]
    assert "api down" in m.calls[0]["messages"][0]["content"]


def test_refusal_moves_on(tmp_path):
    c1, _ = anthropic_client([NS(content=[], stop_reason="refusal", stop_details=NS(category="cyber"), usage=None)])
    c2, _ = anthropic_client([NS(content=[NS(type="text", text="fine")], stop_reason="end_turn", usage=None)])
    rep = Agent(FakeComputer(shot(tmp_path))).run("t", [AnthropicAdapter(client=c1, name="a"), AnthropicAdapter(client=c2, name="b")])
    assert rep.status == "done" and rep.models_used == ["a", "b"]


def test_step_limit(tmp_path):
    loop = [NS(content=[tool_use(f"t{i}", "wait", {"duration": 0})], stop_reason="tool_use", usage=None) for i in range(5)]
    client, _ = anthropic_client(loop)
    rep = Agent(FakeComputer(shot(tmp_path)), max_steps=3).run("t", [AnthropicAdapter(client=client)])
    assert rep.status == "limit" and rep.steps == 3


# --- OpenAI-compatible -------------------------------------------------------------
def test_openai_compat_adapter(tmp_path):
    seen = []
    replies = [
        {"choices": [{"finish_reason": "tool_calls", "message": {"role": "assistant", "content": None, "tool_calls": [
            {"id": "c1", "type": "function", "function": {"name": "screenshot", "arguments": "{}"}},
            {"id": "c2", "type": "function", "function": {"name": "type", "arguments": json.dumps({"text": "hi"})}}]}}]},
        {"choices": [{"finish_reason": "stop", "message": {"role": "assistant", "content": "finished"}}]},
    ]

    def handler(req):
        seen.append(json.loads(req.content))
        assert req.headers["authorization"] == "Bearer k"
        return httpx.Response(200, json=replies.pop(0))

    ad = OpenAICompatAdapter("http://m/v1", "vision", api_key="k", http=httpx.Client(transport=httpx.MockTransport(handler)))
    rep = Agent(FakeComputer(shot(tmp_path))).run("t", [ad])
    assert rep.status == "done" and rep.final_text == "finished"
    msgs = seen[1]["messages"]
    assert [m["role"] for m in msgs[-3:]] == ["tool", "tool", "user"]
    assert msgs[-1]["content"][0]["image_url"]["url"].startswith("data:image/png;base64,")
    assert {f["function"]["name"] for f in seen[0]["tools"]} >= {"screenshot", "left_click", "zoom", "wait"}
