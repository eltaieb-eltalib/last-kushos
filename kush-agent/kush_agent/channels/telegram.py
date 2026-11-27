"""Telegram bot channel using the Bot API with long polling (no public URL needed).

Create a bot with @BotFather, put its token in $TELEGRAM_BOT_TOKEN, and add your numeric user id
to `channels.telegram.allowed_users` (the bot tells unknown users their id).
"""

from __future__ import annotations

import threading
from typing import Callable

import httpx

from .base import Channel, Incoming

MAX_TEXT = 4000  # Telegram limit is 4096 characters


class TelegramChannel(Channel):
    name = "telegram"

    def __init__(self, token: str, on_message: Callable[[Incoming], None],
                 api_base: str = "https://api.telegram.org", http: httpx.Client | None = None,
                 poll_timeout: int = 30):
        if not token:
            raise ValueError("telegram: missing bot token")
        self.base = f"{api_base.rstrip('/')}/bot{token}"
        self.on_message = on_message
        self.http = http or httpx.Client(timeout=poll_timeout + 15)
        self.poll_timeout = poll_timeout
        self.offset = 0
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self.connected = False
        self.last_error: str | None = None

    # -- outgoing -------------------------------------------------------------------------------
    def _call(self, method: str, **kw) -> dict:
        r = self.http.post(f"{self.base}/{method}", **kw)
        data = r.json()
        if not data.get("ok"):
            raise RuntimeError(f"telegram {method}: {data.get('description')}")
        return data.get("result")

    def send_text(self, chat_id: str, text: str) -> None:
        for i in range(0, max(len(text), 1), MAX_TEXT):
            self._call("sendMessage", json={"chat_id": chat_id, "text": text[i:i + MAX_TEXT] or "."})

    def send_photo(self, chat_id: str, png: bytes, caption: str = "") -> None:
        self._call("sendPhoto", data={"chat_id": chat_id, "caption": caption[:1000]},
                   files={"photo": ("screen.png", png, "image/png")})

    def send_buttons(self, chat_id: str, text: str, buttons: list[tuple[str, str]]) -> None:
        kb = {"inline_keyboard": [[{"text": t, "callback_data": d} for t, d in buttons]]}
        self._call("sendMessage", json={"chat_id": chat_id, "text": text[:MAX_TEXT], "reply_markup": kb})

    # -- incoming -------------------------------------------------------------------------------
    def handle_update(self, u: dict) -> None:
        self.offset = max(self.offset, u["update_id"] + 1)
        if "callback_query" in u:
            cq = u["callback_query"]
            try:
                self._call("answerCallbackQuery", json={"callback_query_id": cq["id"]})
            except Exception:
                pass
            msg = cq.get("message") or {}
            self.on_message(Incoming("telegram", str(msg.get("chat", {}).get("id", cq["from"]["id"])),
                                     str(cq["from"]["id"]), button=cq.get("data"),
                                     name=cq["from"].get("first_name", "")))
            return
        msg = u.get("message") or u.get("edited_message")
        if not msg or "text" not in msg:
            if msg:
                self.send_text(str(msg["chat"]["id"]), "أرسل المهمة كنص من فضلك.")
            return
        self.on_message(Incoming("telegram", str(msg["chat"]["id"]), str(msg["from"]["id"]), msg["text"],
                                 name=msg["from"].get("first_name", "")))

    def poll_once(self) -> int:
        updates = self._call("getUpdates", json={"offset": self.offset, "timeout": self.poll_timeout,
                                                 "allowed_updates": ["message", "callback_query"]})
        self.connected, self.last_error = True, None
        for u in updates:
            try:
                self.handle_update(u)
            except Exception as e:  # one bad update must not stop the bot
                self.last_error = f"{type(e).__name__}: {e}"
        return len(updates)

    def _loop(self) -> None:
        backoff = 1
        while not self._stop.is_set():
            try:
                self.poll_once()
                backoff = 1
            except Exception as e:
                self.connected, self.last_error = False, f"{type(e).__name__}: {e}"
                self._stop.wait(backoff)
                backoff = min(backoff * 2, 60)

    def start(self) -> None:
        if self._thread is None:
            self._thread = threading.Thread(target=self._loop, name="telegram", daemon=True)
            self._thread.start()

    def stop(self) -> None:
        self._stop.set()

