"""Minimal synchronous QMP (QEMU Machine Protocol) client.

QMP is a JSON protocol on a socket. After connecting, QEMU sends a greeting; the client sends
`qmp_capabilities` and can then run commands. Asynchronous events can arrive between replies and
are collected in `self.events`.
"""

from __future__ import annotations

import json
import socket
import time
from typing import Any


class QMPError(RuntimeError):
    pass


class QMPClient:
    def __init__(self, address: str | tuple[str, int], timeout: float = 30.0):
        self.address = address
        self.timeout = timeout
        self.sock: socket.socket | None = None
        self._buf = b""
        self._id = 0
        self.events: list[dict[str, Any]] = []
        self.greeting: dict[str, Any] | None = None

    # -- connection -----------------------------------------------------------------
    def connect(self, retries: int = 50, delay: float = 0.1) -> "QMPClient":
        last: Exception | None = None
        for _ in range(retries):
            try:
                if isinstance(self.address, tuple):
                    s = socket.create_connection(self.address, timeout=self.timeout)
                else:
                    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                    s.settimeout(self.timeout)
                    s.connect(self.address)
                self.sock = s
                break
            except OSError as e:  # socket not ready yet
                last = e
                time.sleep(delay)
        else:
            raise QMPError(f"cannot connect to QMP at {self.address}: {last}")
        self.greeting = self._read_message()
        if "QMP" not in self.greeting:
            raise QMPError(f"unexpected QMP greeting: {self.greeting}")
        self.execute("qmp_capabilities")
        return self

    def close(self) -> None:
        if self.sock:
            try:
                self.sock.close()
            finally:
                self.sock = None

    def __enter__(self) -> "QMPClient":
        return self.connect() if self.sock is None else self

    def __exit__(self, *exc: object) -> None:
        self.close()

    # -- protocol -------------------------------------------------------------------
    def _read_message(self) -> dict[str, Any]:
        assert self.sock is not None
        while b"\n" not in self._buf:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise QMPError("QMP connection closed")
            self._buf += chunk
        line, self._buf = self._buf.split(b"\n", 1)
        line = line.strip()
        if not line:
            return self._read_message()
        return json.loads(line)

    def execute(self, command: str, arguments: dict[str, Any] | None = None) -> Any:
        if self.sock is None:
            raise QMPError("not connected")
        self._id += 1
        msg: dict[str, Any] = {"execute": command, "id": self._id}
        if arguments:
            msg["arguments"] = arguments
        self.sock.sendall(json.dumps(msg).encode() + b"\n")
        while True:
            reply = self._read_message()
            if "event" in reply:
                self.events.append(reply)
                continue
            if reply.get("id") != self._id:
                continue
            if "error" in reply:
                err = reply["error"]
                raise QMPError(f"{command}: {err.get('class')}: {err.get('desc')}")
            return reply.get("return")

    def hmp(self, command_line: str) -> str:
        """Run a human-monitor command (used for savevm/loadvm on older QEMU)."""
        return self.execute("human-monitor-command", {"command-line": command_line}) or ""

    # -- convenience ----------------------------------------------------------------
    def status(self) -> dict[str, Any]:
        return self.execute("query-status")

    def screendump(self, path: str, fmt: str = "png") -> None:
        args: dict[str, Any] = {"filename": path}
        if fmt != "ppm":
            args["format"] = fmt  # QEMU >= 7.1
        try:
            self.execute("screendump", args)
        except QMPError:
            if fmt == "ppm":
                raise
            self.execute("screendump", {"filename": path})  # old QEMU: PPM only

    def input_events(self, events: list[dict[str, Any]]) -> None:
        self.execute("input-send-event", {"events": events})

    def savevm(self, name: str) -> str:
        out = self.hmp(f"savevm {name}")
        if out.strip():
            raise QMPError(f"savevm {name}: {out.strip()}")
        return name

    def loadvm(self, name: str) -> None:
        out = self.hmp(f"loadvm {name}")
        if out.strip():
            raise QMPError(f"loadvm {name}: {out.strip()}")

    def quit(self) -> None:
        try:
            self.execute("quit")
        except QMPError:
            pass
