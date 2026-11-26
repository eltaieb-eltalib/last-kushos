"""External control API (REST): lets any outside program or model drive a VM directly.

    GET  /health
    GET  /vms                              -> list of VMs
    GET  /vms/{name}/screenshot            -> image/png
    POST /vms/{name}/actions   {"name": "left_click", "input": {"coordinate": [100, 200]}}
                               -> {"is_error": false, "text": "OK", "image_png_b64": null, "width": .., "height": ..}

Every request needs `Authorization: Bearer <token>` where the token is $KUSH_API_TOKEN.
Actions pass through the same Policy as the built-in agent; actions that need approval are refused
(there is no human on this channel) unless the server was started with --approve-all.
"""

from __future__ import annotations

import base64
import hmac
import json
import os
import re
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any

from .agent.policy import Policy
from .models.base import Action
from .tools.computer import Computer
from .vm.qemu import VM, state_root


class ControlServer:
    def __init__(self, token: str, policy: Policy | None = None, approve_all: bool = False):
        if not token:
            raise ValueError("an API token is required (set KUSH_API_TOKEN)")
        self.token = token
        self.policy = policy or Policy()
        self.approve_all = approve_all
        self._computers: dict[str, Computer] = {}
        self._lock = threading.Lock()

    def computer(self, name: str) -> Computer:
        with self._lock:
            comp = self._computers.get(name)
            if comp is None:
                vm = VM.load(name)
                if not vm.is_running():
                    raise FileNotFoundError(f"VM {name!r} is not running")
                comp = Computer(vm.qmp())
                self._computers[name] = comp
            return comp

    def do(self, name: str, action: str, inp: dict[str, Any]) -> dict[str, Any]:
        d = self.policy.check(Action("api", action, inp))
        if d.verdict == "deny" or (d.verdict == "ask" and not self.approve_all):
            return {"is_error": True, "text": f"Blocked by policy: {d.reason}"}
        comp = self.computer(name)
        with self._lock:
            res = comp.run(action, inp)
        out: dict[str, Any] = {"is_error": res.is_error, "text": res.text, "image_png_b64": None}
        if res.image:
            out["image_png_b64"] = res.image.b64()
            out["width"], out["height"] = res.image.geometry.shown_w, res.image.geometry.shown_h
        return out

    def handler(self) -> type[BaseHTTPRequestHandler]:
        srv = self

        class H(BaseHTTPRequestHandler):
            def _auth(self) -> bool:
                got = self.headers.get("Authorization", "")
                if hmac.compare_digest(got, f"Bearer {srv.token}"):
                    return True
                self._json(401, {"error": "unauthorized"})
                return False

            def _json(self, code: int, obj: Any) -> None:
                body = json.dumps(obj, ensure_ascii=False).encode()
                self.send_response(code)
                self.send_header("Content-Type", "application/json; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def log_message(self, fmt: str, *args: Any) -> None:
                pass

            def do_GET(self) -> None:
                if self.path == "/health":
                    return self._json(200, {"ok": True})
                if not self._auth():
                    return
                if self.path == "/vms":
                    root = state_root()
                    names = sorted(p.name for p in root.iterdir()) if root.exists() else []
                    return self._json(200, [{"name": n, "running": VM.load(n).is_running()}
                                            for n in names if (root / n / "vm.json").exists()])
                m = re.fullmatch(r"/vms/([\w.-]+)/screenshot", self.path)
                if m:
                    try:
                        res = srv.do(m.group(1), "screenshot", {})
                    except FileNotFoundError as e:
                        return self._json(404, {"error": str(e)})
                    png = base64.b64decode(res["image_png_b64"])
                    self.send_response(200)
                    self.send_header("Content-Type", "image/png")
                    self.send_header("Content-Length", str(len(png)))
                    self.end_headers()
                    self.wfile.write(png)
                    return
                self._json(404, {"error": "not found"})

            def do_POST(self) -> None:
                if not self._auth():
                    return
                m = re.fullmatch(r"/vms/([\w.-]+)/actions", self.path)
                if not m:
                    return self._json(404, {"error": "not found"})
                try:
                    n = int(self.headers.get("Content-Length", "0"))
                    body = json.loads(self.rfile.read(min(n, 1 << 20)) or b"{}")
                    res = srv.do(m.group(1), body["name"], body.get("input") or {})
                except FileNotFoundError as e:
                    return self._json(404, {"error": str(e)})
                except (KeyError, ValueError) as e:
                    return self._json(400, {"error": f"bad request: {e}"})
                self._json(200, res)

        return H

    def serve(self, host: str = "127.0.0.1", port: int = 8765) -> ThreadingHTTPServer:
        return ThreadingHTTPServer((host, port), self.handler())


def main_serve(host: str, port: int, approve_all: bool, policy: Policy | None = None) -> None:
    server = ControlServer(os.environ.get("KUSH_API_TOKEN", ""), policy, approve_all).serve(host, port)
    print(f"kush-agent API listening on http://{host}:{port}")
    server.serve_forever()
