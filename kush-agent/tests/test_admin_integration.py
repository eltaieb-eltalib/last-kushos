"""Admin panel end-to-end against a real QEMU, with a scripted model instead of a paid API."""
import shutil
import threading
import time

import httpx
import pytest

from kush_agent.admin.app import Panel, serve
from kush_agent.models.base import Action, ModelAdapter, Step

pytestmark = pytest.mark.integration

CONF = """
os_name: test
models:
  deepseek: {provider: openai_compat, base_url: "https://api.deepseek.com", model: deepseek-flash, api_key_env: DEEPSEEK_API_KEY}
  claude: {provider: anthropic, model: claude-opus-5-5}
routes:
  - {match: "ثبّت", model: claude}
default: deepseek
fallback: [deepseek, claude]
"""


class Scripted(ModelAdapter):
    def __init__(self, name):
        self.name, self.n = name, 0

    def start(self, task, system):
        self.n = 0

    def step(self):
        self.n += 1
        if self.n == 1:
            return Step([Action("a", "screenshot", {}), Action("b", "key", {"text": "ctrl+alt+Delete"})], "looking")
        return Step(text="finished", done=True)

    def add_results(self, results):
        self.results = results


@pytest.fixture
def panel_url(tmp_path, monkeypatch):
    if not shutil.which("qemu-system-x86_64"):
        pytest.skip("qemu not installed")
    monkeypatch.setenv("KUSH_STATE_DIR", str(tmp_path / "state"))
    cfg = tmp_path / "models.yaml"
    cfg.write_text(CONF, encoding="utf-8")
    panel = Panel(str(cfg), "pw", adapter_factory=lambda router, name: Scripted(name))
    srv = serve(panel, "127.0.0.1", 0)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    yield f"http://127.0.0.1:{srv.server_address[1]}", panel
    srv.shutdown()
    for v in panel.list_vms():
        panel.vm_power(v["name"], "stop")


def test_admin_flow(panel_url):
    base, panel = panel_url
    c = httpx.Client(base_url=base, headers={"X-Kush": "1"}, timeout=60)
    assert "Kush Agent" in httpx.get(base + "/").text
    assert c.get("/api/overview").status_code == 401
    assert c.post("/api/login", json={"password": "bad"}).status_code == 401
    r = c.post("/api/login", json={"password": "pw"})
    assert r.status_code == 200 and "HttpOnly" in r.headers["set-cookie"]
    # CSRF header is required for state changes
    assert httpx.post(base + "/api/vms", json={}, cookies=c.cookies).status_code == 403

    ov = c.get("/api/overview").json()
    assert ov["accel"]["name"] in ("tcg", "kvm") and ov["models"][0]["name"] == "deepseek"
    assert c.post("/api/vms", json={"name": "bad name"}).status_code == 400
    assert c.post("/api/vms", json={"name": "k1", "memory_mb": 256, "cpus": 1, "disk_size": "64M"}).status_code == 200
    assert c.get("/api/vms").json()[0]["running"] is True

    shot = c.get("/api/vms/k1/screenshot")
    assert shot.content[:4] == b"\x89PNG" and int(shot.headers["X-Width"]) > 0
    assert c.post("/api/vms/k1/actions", json={"name": "left_click", "input": {"coordinate": [10, 10]}}).json()["is_error"] is False
    assert c.post("/api/vms/k1/snapshot", json={"op": "save", "tag": "ready"}).json()["ok"]

    assert c.get("/api/route", params={"prompt": "ثبّت فايرفوكس"}).json()["model"] == "claude"
    run = c.post("/api/runs", json={"vm": "k1", "task": "open notepad"}).json()
    assert run["model"] == "deepseek" and run["chain"] == ["deepseek", "claude"]
    assert c.post("/api/runs", json={"vm": "k1", "task": "x"}).status_code == 400  # one task per VM

    for _ in range(100):  # the agent waits for approval of ctrl+alt+delete
        pend = c.get("/api/approvals").json()
        if pend:
            break
        time.sleep(0.1)
    assert pend and pend[0]["action"] == "key"
    assert c.post(f"/api/approvals/{pend[0]['id']}", json={"allow": True}).json()["ok"]

    for _ in range(100):
        d = c.get(f"/api/runs/{run['id']}").json()
        if d["status"] != "running":
            break
        time.sleep(0.1)
    assert d["status"] == "done" and d["report"]["final_text"] == "finished"
    shots = [s["image"] for s in d["steps"] if s.get("image")]
    assert c.get(f"/api/runs/{run['id']}/{shots[0]}").content[:4] == b"\x89PNG"
    assert [s["policy"] for s in d["steps"] if s["kind"] == "action"] == ["allow", "ask"]
    assert c.get("/api/runs").json()[0]["id"] == run["id"]

    assert c.put("/api/config", json={"text": "models: {}"}).status_code == 400
    text = c.get("/api/config").json()["text"]
    assert c.put("/api/config", json={"text": text.replace("default: deepseek", "default: claude")}).json()["ok"]
    assert c.get("/api/route", params={"prompt": "hi"}).json()["model"] == "claude"
    assert c.get("/api/runs/../../etc/passwd").status_code == 404
    assert c.post("/api/vms/k1/stop").json()["ok"]
