"""Telegram + WhatsApp gateway against a real Panel and QEMU, with mocked chat APIs and a scripted model."""
import hashlib
import hmac
import json
import shutil
import time

import httpx
import pytest

from kush_agent.admin.app import Panel
from kush_agent.channels.gateway import Gateway, build_gateway
from kush_agent.channels.telegram import TelegramChannel
from kush_agent.channels.whatsapp import WhatsAppChannel
from tests.test_admin_integration import CONF, Scripted

pytestmark = pytest.mark.integration


class FakeAPI:
    """Records outgoing calls; serves queued Telegram updates."""

    def __init__(self):
        self.sent, self.updates = [], []

    def __call__(self, req: httpx.Request) -> httpx.Response:
        path = req.url.path
        if path.endswith("/getUpdates"):
            ups, self.updates = self.updates, []
            return httpx.Response(200, json={"ok": True, "result": ups})
        if path.endswith("/media"):
            self.sent.append(("media", path, b""))
            return httpx.Response(200, json={"id": "MEDIA1"})
        ctype = req.headers.get("content-type", "")
        body = json.loads(req.content) if ctype.startswith("application/json") else req.content
        self.sent.append((path.rsplit("/", 1)[-1], path, body))
        return httpx.Response(200, json={"ok": True, "result": {}})

    def texts(self):
        out = []
        for kind, _, b in self.sent:
            if isinstance(b, dict):
                out.append(b.get("text") if isinstance(b.get("text"), str) else
                           (b.get("text") or {}).get("body") or json.dumps(b, ensure_ascii=False))
        return [t for t in out if t]


def wait(cond, timeout=20):
    end = time.time() + timeout
    while time.time() < end:
        if cond():
            return True
        time.sleep(0.1)
    return False


@pytest.fixture
def panel(tmp_path, monkeypatch):
    if not shutil.which("qemu-system-x86_64"):
        pytest.skip("qemu not installed")
    monkeypatch.setenv("KUSH_STATE_DIR", str(tmp_path / "state"))
    cfg = tmp_path / "models.yaml"
    cfg.write_text(CONF, encoding="utf-8")
    p = Panel(str(cfg), "pw", adapter_factory=lambda router, name: Scripted(name))
    p.create_vm({"name": "k1", "memory_mb": 256, "cpus": 1, "disk_size": "64M"})
    yield p
    p.vm_power("k1", "stop")


def tg_msg(uid, text, upd=1):
    return {"update_id": upd, "message": {"message_id": upd, "chat": {"id": uid}, "from": {"id": uid, "first_name": "T"}, "text": text}}


def test_telegram_flow(panel):
    api = FakeAPI()
    gw = Gateway(panel, {"default_vm": "k1"})
    tg = TelegramChannel("TOKEN", gw.handle, api_base="http://tg.test", http=httpx.Client(transport=httpx.MockTransport(api)))
    gw.add_channel(tg, [111])

    api.updates = [tg_msg(999, "hello", 1)]  # stranger
    tg.poll_once()
    assert "غير مصرّح" in api.texts()[-1] and "999" in api.texts()[-1]

    api.updates = [tg_msg(111, "/help", 2)]
    tg.poll_once()
    assert "/screen" in api.texts()[-1]

    api.updates = [tg_msg(111, "الشاشة", 3)]
    tg.poll_once()
    assert api.sent[-1][0] == "sendPhoto"

    api.updates = [tg_msg(111, "افتح المفكرة", 4)]
    tg.poll_once()
    assert "بدأت المهمة" in api.texts()[-1]
    # the scripted model presses ctrl+alt+delete -> approval buttons arrive in the chat
    assert wait(lambda: any(k == "sendMessage" and isinstance(b, dict) and "reply_markup" in b for k, _, b in api.sent))
    btn = next(b for k, _, b in api.sent if isinstance(b, dict) and "reply_markup" in b)
    data = btn["reply_markup"]["inline_keyboard"][0][0]["callback_data"]
    assert data.startswith("ap:") and data.endswith(":1")
    api.updates = [{"update_id": 5, "callback_query": {"id": "cq1", "from": {"id": 111}, "message": {"chat": {"id": 111}}, "data": data}}]
    tg.poll_once()
    assert wait(lambda: any("اكتملت" in t for t in api.texts()))
    assert "finished" in next(t for t in api.texts() if "اكتملت" in t)
    assert wait(lambda: api.sent[-1][0] == "sendPhoto")
    assert any(t.startswith("💬 looking") for t in api.texts())
    run = next(iter(panel.runs.values()))
    assert run.owner == "telegram:111" and run.status == "done"


def test_whatsapp_webhook(panel):
    api = FakeAPI()
    gw = Gateway(panel, {"default_vm": "k1", "progress": False})
    wa = WhatsAppChannel("TOK", "PNID", "appsecret", "verifyme", gw.handle, api_base="http://wa.test",
                         http=httpx.Client(transport=httpx.MockTransport(api)))
    gw.add_channel(wa, ["+249 912 345678"])

    code, _, body = wa.webhook("GET", {"hub.mode": ["subscribe"], "hub.verify_token": ["verifyme"], "hub.challenge": ["42"]}, {}, b"")
    assert (code, body) == (200, b"42")
    assert wa.webhook("GET", {"hub.mode": ["subscribe"], "hub.verify_token": ["nope"], "hub.challenge": ["42"]}, {}, b"")[0] == 403

    def post(payload):
        raw = json.dumps(payload).encode()
        sig = "sha256=" + hmac.new(b"appsecret", raw, hashlib.sha256).hexdigest()
        return wa.webhook("POST", {}, {"X-Hub-Signature-256": sig}, raw)[0]

    def text_msg(frm, text):
        return {"entry": [{"changes": [{"value": {"contacts": [{"wa_id": frm, "profile": {"name": "U"}}],
                                                   "messages": [{"from": frm, "type": "text", "text": {"body": text}}]}}]}]}

    assert wa.webhook("POST", {}, {"X-Hub-Signature-256": "sha256=bad"}, b"{}")[0] == 401
    assert post(text_msg("249912345678", "افتح المفكرة")) == 200
    assert wait(lambda: any(k == "messages" and b.get("type") == "interactive" for k, _, b in api.sent if isinstance(b, dict)))
    inter = next(b for k, _, b in api.sent if isinstance(b, dict) and b.get("type") == "interactive")
    assert inter["to"] == "249912345678" and inter["messaging_product"] == "whatsapp"
    reply_id = inter["interactive"]["action"]["buttons"][1]["reply"]["id"]  # "رفض"
    assert reply_id.endswith(":0")
    assert post({"entry": [{"changes": [{"value": {"messages": [{"from": "249912345678", "type": "interactive",
                                                                   "interactive": {"type": "button_reply", "button_reply": {"id": reply_id, "title": "رفض"}}}]}}]}]}) == 200
    assert wait(lambda: any("اكتملت" in t for t in api.texts()))
    assert any("تم الرفض" in t for t in api.texts())
    assert wait(lambda: any(k == "media" for k, _, _ in api.sent))  # final screenshot uploaded
    run = next(iter(panel.runs.values()))
    steps = (panel.runs_root / run.id / "steps.jsonl").read_text()
    assert "Blocked by policy" in steps  # the rejected ctrl+alt+delete never reached the VM

    post(text_msg("111", "hi"))  # not on the allow-list
    assert wait(lambda: any("غير مصرّح" in t for t in api.texts()))


def test_build_gateway_from_config(panel, monkeypatch):
    monkeypatch.setenv("WA_T", "t")
    monkeypatch.setenv("WA_S", "s")
    monkeypatch.setenv("WA_V", "v")
    assert build_gateway(panel, {}) is None
    gw = build_gateway(panel, {"channels": {"whatsapp": {"enabled": True, "token_env": "WA_T", "app_secret_env": "WA_S",
                                                          "verify_token_env": "WA_V", "phone_number_id": "1", "allowed_numbers": ["1"]}}})
    assert "/webhooks/whatsapp" in panel.webhooks and gw.status()[0]["name"] == "whatsapp"
    assert panel.overview()["channels"][0]["allowed"] == 1


def test_webhook_reachable_without_login(panel, monkeypatch):
    import threading

    from kush_agent.admin.app import serve

    for k in ("WA_T", "WA_S", "WA_V"):
        monkeypatch.setenv(k, "v")
    build_gateway(panel, {"channels": {"whatsapp": {"enabled": True, "token_env": "WA_T", "app_secret_env": "WA_S",
                                                     "verify_token_env": "WA_V", "phone_number_id": "1"}}})
    srv = serve(panel, "127.0.0.1", 0)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    base = f"http://127.0.0.1:{srv.server_address[1]}"
    try:
        r = httpx.get(base + "/webhooks/whatsapp", params={"hub.mode": "subscribe", "hub.verify_token": "v", "hub.challenge": "abc"})
        assert r.status_code == 200 and r.text == "abc"
        assert httpx.post(base + "/webhooks/whatsapp", content=b"{}").status_code == 401  # unsigned
        assert httpx.get(base + "/webhooks/other").status_code == 404
        assert httpx.get(base + "/api/overview").status_code == 401  # the rest still needs login
    finally:
        srv.shutdown()
