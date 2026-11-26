"""Client for kushagent.exe, the small agent that runs inside the guest OS.

Transport: the VM's `ga` chardev (unix socket or TCP on the host) wired to a serial port (COM2)
or a virtio-serial port in the guest. Protocol: one JSON object per line.

    -> {"id": 1, "method": "run", "params": {"cmd": "cmd /c ver", "timeout_ms": 10000}}
    <- {"id": 1, "result": {"exit_code": 0, "output": "..."}}
    <- {"id": 1, "error": "message"}

Methods: ping, run, type_unicode, list_windows, read_file, write_file (see docs/guest-agent-protocol.md).
"""

from __future__ import annotations

import base64
import json
import socket
import time
from typing import Any


class GuestError(RuntimeError):
    pass


class GuestClient:
    def __init__(self, address: str | tuple[str, int], timeout: float = 30.0):
        self.address = address
        self.timeout = timeout
        self.sock: socket.socket | None = None
        self._buf = b""
        self._id = 0

    def connect(self) -> "GuestClient":
        if isinstance(self.address, tuple):
            self.sock = socket.create_connection(self.address, timeout=self.timeout)
        else:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.settimeout(self.timeout)
            s.connect(self.address)
            self.sock = s
        return self

    def close(self) -> None:
        if self.sock:
            self.sock.close()
            self.sock = None

    def __enter__(self) -> "GuestClient":
        return self.connect() if self.sock is None else self

    def __exit__(self, *exc: object) -> None:
        self.close()

    def _readline(self, deadline: float) -> bytes:
        assert self.sock is not None
        while b"\n" not in self._buf:
            self.sock.settimeout(max(0.05, deadline - time.time()))
            try:
                chunk = self.sock.recv(65536)
            except socket.timeout as e:
                raise GuestError("guest agent did not answer (is kushagent.exe running in the guest?)") from e
            if not chunk:
                raise GuestError("guest channel closed")
            self._buf += chunk
        line, self._buf = self._buf.split(b"\n", 1)
        return line

    def call(self, method: str, params: dict[str, Any] | None = None, timeout: float | None = None) -> Any:
        if self.sock is None:
            self.connect()
        assert self.sock is not None
        self._id += 1
        req = {"id": self._id, "method": method, "params": params or {}}
        self.sock.sendall(json.dumps(req, ensure_ascii=True).encode() + b"\n")
        deadline = time.time() + (timeout or self.timeout)
        while True:
            line = self._readline(deadline).strip()
            if not line:
                continue
            try:
                msg = json.loads(line.decode("utf-8", errors="replace"))
            except json.JSONDecodeError:
                continue  # noise on the serial line (boot messages etc.)
            if msg.get("id") != self._id:
                continue
            if "error" in msg:
                raise GuestError(f"{method}: {msg['error']}")
            return msg.get("result")

    # -- typed helpers --------------------------------------------------------------
    def ping(self) -> dict[str, Any]:
        return self.call("ping", timeout=5)

    def run(self, cmd: str, timeout_ms: int = 30000) -> dict[str, Any]:
        return self.call("run", {"cmd": cmd, "timeout_ms": timeout_ms}, timeout=timeout_ms / 1000 + 5)

    def type_unicode(self, text: str) -> None:
        self.call("type_unicode", {"text": text})

    def list_windows(self) -> list[dict[str, Any]]:
        return self.call("list_windows") or []

    def read_file(self, path: str) -> bytes:
        res = self.call("read_file", {"path": path})
        return base64.b64decode(res["data"])

    def write_file(self, path: str, data: bytes) -> None:
        self.call("write_file", {"path": path, "data": base64.b64encode(data).decode()})
