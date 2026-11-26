"""End-to-end with a real QEMU: CLI start/do/screenshot/stop and the REST control API."""
import base64
import json
import shutil
import threading

import httpx
import pytest

from kush_agent.cli import main
from kush_agent.server import ControlServer

pytestmark = pytest.mark.integration


@pytest.fixture
def running(tmp_path, monkeypatch):
    if not shutil.which("qemu-system-x86_64"):
        pytest.skip("qemu not installed")
    monkeypatch.setenv("KUSH_STATE_DIR", str(tmp_path / "state"))
    assert main(["vm", "start", "e2e", "--memory", "128", "--cpus", "1", "--nic", "none"]) == 0
    yield tmp_path
    assert main(["vm", "stop", "e2e"]) == 0


def test_cli(running, capsys):
    out = running / "s.png"
    assert main(["vm", "screenshot", "e2e", "-o", str(out)]) == 0
    assert out.read_bytes()[:4] == b"\x89PNG"
    assert main(["vm", "do", "e2e", "left_click", '{"coordinate": [20, 20]}']) == 0
    assert main(["vm", "do", "e2e", "type", '{"text": "dir"}']) == 0
    assert main(["vm", "status", "e2e"]) == 0
    assert '"running"' in capsys.readouterr().out
    assert main(["vm", "list"]) == 0


def test_rest_api(running):
    srv = ControlServer("secret").serve("127.0.0.1", 0)
    port = srv.server_address[1]
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    base = f"http://127.0.0.1:{port}"
    try:
        assert httpx.get(f"{base}/vms").status_code == 401
        h = {"Authorization": "Bearer secret"}
        assert httpx.get(f"{base}/vms", headers=h).json() == [{"name": "e2e", "running": True}]
        assert httpx.get(f"{base}/vms/e2e/screenshot", headers=h).content[:4] == b"\x89PNG"
        r = httpx.post(f"{base}/vms/e2e/actions", headers=h, json={"name": "screenshot"}, timeout=60).json()
        assert base64.b64decode(r["image_png_b64"])[:4] == b"\x89PNG" and r["width"] > 0
        r = httpx.post(f"{base}/vms/e2e/actions", headers=h, json={"name": "key", "input": {"text": "ctrl+alt+Delete"}}).json()
        assert r["is_error"] and "policy" in r["text"]
        r = httpx.post(f"{base}/vms/e2e/actions", headers=h, json={"name": "mouse_move", "input": {"coordinate": [3, 4]}}).json()
        assert r == {"is_error": False, "text": "OK", "image_png_b64": None}
        assert httpx.get(f"{base}/vms/nope/screenshot", headers=h).status_code == 404
    finally:
        srv.shutdown()
