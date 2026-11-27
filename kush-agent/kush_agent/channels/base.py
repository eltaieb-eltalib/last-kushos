"""Chat channels (Telegram, WhatsApp) and the gateway that turns chat messages into agent tasks."""

from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass


@dataclass
class Incoming:
    channel: str          # "telegram" | "whatsapp"
    chat_id: str          # where to answer (Telegram chat id / WhatsApp number)
    user_id: str          # who sent it (checked against the allow-list)
    text: str = ""
    button: str | None = None   # callback data of a pressed button, e.g. "ap:<id>:1"
    name: str = ""


class Channel(ABC):
    name: str = "channel"

    @abstractmethod
    def send_text(self, chat_id: str, text: str) -> None: ...

    @abstractmethod
    def send_photo(self, chat_id: str, png: bytes, caption: str = "") -> None: ...

    @abstractmethod
    def send_buttons(self, chat_id: str, text: str, buttons: list[tuple[str, str]]) -> None:
        """buttons: [(title, callback_data), ...]"""

    def start(self) -> None:
        """Begin receiving messages (polling thread or webhook registration)."""

    def stop(self) -> None:
        pass
