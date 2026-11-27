"""WhatsApp channel using Meta's WhatsApp Business Cloud API.

Incoming messages arrive on a webhook, so the admin panel must be reachable over HTTPS
(e.g. https://panel.example.com/webhooks/whatsapp behind Caddy). Needed from the Meta app:
  - a permanent access token            -> $WHATSAPP_TOKEN
  - the phone number id                 -> channels.whatsapp.phone_number_id
  - the app secret (signs webhooks)     -> $WHATSAPP_APP_SECRET
  - a verify token you choose           -> $WHATSAPP_VERIFY_TOKEN (typed into the Meta dashboard too)
WhatsApp only lets a business reply freely within 24 hours of the user's last message, which fits
this use: the user writes first, the agent answers.
"""

from __future__ import annotations

import hashlib
import hmac
import json
import threading
from typing import Any, Callable

import httpx

from .base import Channel, Incoming

MAX_TEXT = 4000      # WhatsApp text body limit is 4096
MAX_BUTTON_TITLE = 20


class WhatsAppChannel(Channel):
    name = "whatsapp"

    def __init__(self, token: str, phone_number_id: str, app_secret: str, verify_token: str,
                 on_message: Callable[[Incoming], None], api_version: str = "v23.0",
                 api_base: str = "https://graph.facebook.com", http: httpx.Client | None = None):
        missing = [n for n, v in (("token", token), ("phone_number_id", phone_number_id),
                                  ("app_secret", app_secret), ("verify_token", verify_token)) if not v]
        if missing:
            raise ValueError(f"whatsapp: missing {', '.join(missing)}")
        self.token = token
        self.verify_token = verify_token
        self.app_secret = app_secret.encode()
        self.base = f"{api_base.rstrip('/')}/{api_version}/{phone_number_id}"
        self.on_message = on_message
        self.http = http or httpx.Client(timeout=30)
        self.connected = False
        self.last_error: str | None = None

    # -- outgoing -------------------------------------------------------------------------------
    def _headers(self) -> dict[str, str]:
        return {"Authorization": f"Bearer {self.token}"}

    def _send(self, to: str, payload: dict[str, Any]) -> None:
        body = {"messaging_product": "whatsapp", "recipient_type": "individual", "to": to, **payload}
        r = self.http.post(f"{self.base}/messages", headers=self._headers(), json=body)
        if r.status_code >= 400:
            self.last_error = r.text[:300]
            raise RuntimeError(f"whatsapp send: {r.status_code} {r.text[:300]}")
        self.connected = True

    def send_text(self, chat_id: str, text: str) -> None:
        for i in range(0, max(len(text), 1), MAX_TEXT):
            self._send(chat_id, {"type": "text", "text": {"body": text[i:i + MAX_TEXT] or "."}})

    def send_photo(self, chat_id: str, png: bytes, caption: str = "") -> None:
        r = self.http.post(f"{self.base}/media", headers=self._headers(),
                           data={"messaging_product": "whatsapp", "type": "image/png"},
                           files={"file": ("screen.png", png, "image/png")})
        if r.status_code >= 400:
            raise RuntimeError(f"whatsapp media upload: {r.status_code} {r.text[:300]}")
        media_id = r.json()["id"]
        self._send(chat_id, {"type": "image", "image": {"id": media_id, "caption": caption[:1000]}})

    def send_buttons(self, chat_id: str, text: str, buttons: list[tuple[str, str]]) -> None:
        self._send(chat_id, {"type": "interactive", "interactive": {
            "type": "button", "body": {"text": text[:1024]},
            "action": {"buttons": [{"type": "reply", "reply": {"id": d, "title": t[:MAX_BUTTON_TITLE]}}
                                   for t, d in buttons[:3]]}}})

    # -- webhook --------------------------------------------------------------------------------
    def verify_signature(self, raw: bytes, header: str | None) -> bool:
        if not header or not header.startswith("sha256="):
            return False
        good = hmac.new(self.app_secret, raw, hashlib.sha256).hexdigest()
        return hmac.compare_digest(header[7:], good)

    def webhook(self, method: str, query: dict[str, list[str]], headers: Any, raw: bytes) -> tuple[int, str, bytes]:
        """Plug into the admin panel at /webhooks/whatsapp."""
        if method == "GET":  # subscription check from the Meta dashboard
            q = {k: v[0] for k, v in query.items()}
            if q.get("hub.mode") == "subscribe" and hmac.compare_digest(q.get("hub.verify_token", ""), self.verify_token):
                return 200, "text/plain", q.get("hub.challenge", "").encode()
            return 403, "text/plain", b"forbidden"
        if method != "POST":
            return 405, "text/plain", b"method not allowed"
        if not self.verify_signature(raw, headers.get("X-Hub-Signature-256")):
            return 401, "text/plain", b"bad signature"
        try:
            payload = json.loads(raw or b"{}")
        except ValueError:
            return 400, "text/plain", b"bad json"
        for msg in self.parse(payload):
            # answer Meta immediately; handling (screenshots, starting runs) happens in the background
            threading.Thread(target=self._dispatch, args=(msg,), daemon=True).start()
        self.connected = True
        return 200, "text/plain", b"ok"  # always 200 quickly, or Meta retries

    def _dispatch(self, msg: Incoming) -> None:
        try:
            self.on_message(msg)
        except Exception as e:
            self.last_error = f"{type(e).__name__}: {e}"

    @staticmethod
    def parse(payload: dict[str, Any]) -> list[Incoming]:
        out = []
        for entry in payload.get("entry") or []:
            for change in entry.get("changes") or []:
                value = change.get("value") or {}
                names = {c.get("wa_id"): (c.get("profile") or {}).get("name", "") for c in value.get("contacts") or []}
                for m in value.get("messages") or []:
                    sender = str(m.get("from", ""))
                    inc = Incoming("whatsapp", sender, sender, name=names.get(sender, ""))
                    if m.get("type") == "text":
                        inc.text = (m.get("text") or {}).get("body", "")
                    elif m.get("type") == "interactive":
                        reply = (m.get("interactive") or {}).get("button_reply") or {}
                        inc.button = reply.get("id")
                    elif m.get("type") == "button":
                        inc.button = (m.get("button") or {}).get("payload")
                    else:
                        inc.text = ""  # images, voice notes ... not supported yet
                    out.append(inc)
        return out
