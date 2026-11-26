"""Admin panel: a web dashboard to manage VMs, run tasks, approve actions and edit the model config.

    KUSH_ADMIN_PASSWORD=... kush-agent admin --config config/models.yaml --port 8080

Serves a single-page Arabic (RTL) UI from ./static and a JSON API under /api/. Login with the admin
password sets a signed, HttpOnly, SameSite=Strict session cookie. State-changing requests must also
carry the `X-Kush: 1` header (blocks cross-site form posts). Listen on 127.0.0.1 and put an HTTPS
reverse proxy (Caddy / nginx) in front of it on a VPS.
"""

from __future__ import annotations

import hashlib
import hmac
import json
import os
import re
import secrets
import shutil
import threading
import time
import uuid
from dataclasses import dataclass, field
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Callable
from urllib.parse import parse_qs, urlparse

import yaml

from ..agent.loop import Agent
from ..agent.policy import Decision, Policy
from ..models.base import Action
from ..models.router import Router, RouterConfig
from ..tools.computer import Computer
from ..vm.accel import detect
from ..vm.qemu import VM, VMConfig, state_root
from ..vm.qmp import QMPClient

STATIC = Path(__file__).parent / "static"
NAME_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.-]{0,40}$")
SESSION_TTL = 12 * 3600


# ---------------------------------------------------------------------------------------------
# Approvals coming from running agents, answered from the browser
# ---------------------------------------------------------------------------------------------
@dataclass
class PendingApproval:
    id: str
    run_id: str
    action: str
    input: dict[str, Any]
    reason: str
    created: float
    event: threading.Event = field(default_factory=threading.Event, repr=False)
    allowed: bool = False

    def public(self) -> dict[str, Any]:
        return {"id": self.id, "run_id": self.run_id, "action": self.action, "input": self.input,
                "reason": self.reason, "created": self.created}


class ApprovalQueue:
    def __init__(self, timeout: float = 600):
        self.timeout = timeout
        self.items: dict[str, PendingApproval] = {}
        self.lock = threading.Lock()

    def approver_for(self, run_id: str, stop: threading.Event) -> Callable[[Action, Decision], bool]:
        def approve(action: Action, decision: Decision) -> bool:
            p = PendingApproval(uuid.uuid4().hex[:10], run_id, action.name, action.input, decision.reason, time.time())
            with self.lock:
                self.items[p.id] = p
            deadline = time.time() + self.timeout
            while not p.event.wait(0.5):
                if stop.is_set() or time.time() > deadline:
                    break
            with self.lock:
                self.items.pop(p.id, None)
            return p.allowed
        return approve

    def answer(self, pid: str, allowed: bool) -> bool:
        with self.lock:
            p = self.items.get(pid)
        if not p:
            return False
        p.allowed = allowed
        p.event.set()
        return True

    def list(self) -> list[dict[str, Any]]:
        with self.lock:
            return [p.public() for p in sorted(self.items.values(), key=lambda x: x.created)]


# ---------------------------------------------------------------------------------------------
# Runs
# ---------------------------------------------------------------------------------------------
@dataclass
class Run:
    id: str
    vm: str
    task: str
    model: str
    chain: list[str]
    created: float
    status: str = "running"
    report: dict[str, Any] | None = None
    stop: threading.Event = field(default_factory=threading.Event, repr=False)

    def public(self) -> dict[str, Any]:
        return {"id": self.id, "vm": self.vm, "task": self.task, "model": self.model, "chain": self.chain,
                "created": self.created, "status": self.status, "report": self.report}


class Panel:
    """Everything the HTTP layer needs; kept separate so it is easy to test."""

    def __init__(self, config_path: str, password: str, secret: str | None = None,
                 runs_root: str | None = None, images_dir: str | None = None,
                 adapter_factory: Callable[[Router, str], Any] | None = None):
        if not password:
            raise ValueError("set KUSH_ADMIN_PASSWORD")
        self.config_path = config_path
        self.password = password
        self.secret = (secret or secrets.token_hex(32)).encode()
        base = state_root().parent
        self.runs_root = Path(runs_root or base / "runs")
        self.images_dir = Path(images_dir or base / "images")
        self.images_dir.mkdir(parents=True, exist_ok=True)
        self.runs: dict[str, Run] = {}
        self.approvals = ApprovalQueue()
        self.adapter_factory = adapter_factory or (lambda router, name: router.build(name))
        self._qmp: dict[str, QMPClient] = {}
        self._computers: dict[str, Computer] = {}
        self._lock = threading.Lock()
        self._failed_logins = 0
        self.conf: dict[str, Any] = {}
        self.reload_config()

    # -- config -------------------------------------------------------------------------------
    def reload_config(self) -> None:
        with open(self.config_path, encoding="utf-8") as f:
            conf = yaml.safe_load(f) or {}
        RouterConfig.from_dict(conf)  # validate
        self.conf = conf

    def router(self) -> Router:
        return Router(RouterConfig.from_dict(self.conf))

    def config_text(self) -> str:
        return Path(self.config_path).read_text(encoding="utf-8")

    def save_config(self, text: str) -> None:
        conf = yaml.safe_load(text) or {}
        RouterConfig.from_dict(conf)
        Policy.from_dict(conf.get("policy"))
        tmp = Path(self.config_path).with_suffix(".tmp")
        tmp.write_text(text, encoding="utf-8")
        tmp.replace(self.config_path)
        self.conf = conf

    def models(self) -> list[dict[str, Any]]:
        out = []
        for name, spec in (self.conf.get("models") or {}).items():
            env = spec.get("api_key_env") or ("ANTHROPIC_API_KEY" if spec.get("provider") == "anthropic" else None)
            out.append({"name": name, "provider": spec.get("provider"), "model": spec.get("model"),
                        "base_url": spec.get("base_url"), "key_env": env,
                        "key_present": bool(os.environ.get(env)) if env else None})
        return out

    # -- auth ---------------------------------------------------------------------------------
    def login(self, password: str) -> str | None:
        if not hmac.compare_digest(password.encode(), self.password.encode()):
            self._failed_logins += 1
            time.sleep(min(5, 0.5 * self._failed_logins))
            return None
        self._failed_logins = 0
        exp = str(int(time.time()) + SESSION_TTL)
        return f"{exp}.{hmac.new(self.secret, exp.encode(), hashlib.sha256).hexdigest()}"

    def valid_session(self, token: str | None) -> bool:
        if not token or "." not in token:
            return False
        exp, sig = token.split(".", 1)
        good = hmac.new(self.secret, exp.encode(), hashlib.sha256).hexdigest()
        return hmac.compare_digest(sig, good) and exp.isdigit() and int(exp) > time.time()

    # -- VMs ----------------------------------------------------------------------------------
    def list_vms(self) -> list[dict[str, Any]]:
        root = state_root()
        out = []
        for p in sorted(root.iterdir()) if root.exists() else []:
            if not (p / "vm.json").exists():
                continue
            vm = VM.load(p.name)
            c = vm.config
            busy = any(r.vm == p.name and r.status == "running" for r in self.runs.values())
            out.append({"name": p.name, "running": vm.is_running(), "accel": vm.accel.name, "memory_mb": c.memory_mb,
                        "cpus": c.cpus, "iso": Path(c.iso).name if c.iso else None,
                        "disk": Path(c.disk).name if c.disk else None, "busy": busy})
        return out

    def images(self) -> list[dict[str, Any]]:
        return [{"name": p.name, "size": p.stat().st_size} for p in sorted(self.images_dir.iterdir())
                if p.suffix.lower() in (".iso", ".qcow2", ".img")]

    def _image(self, name: str | None) -> str | None:
        if not name:
            return None
        p = (self.images_dir / name).resolve()
        if p.parent != self.images_dir.resolve() or not p.exists():
            raise ValueError(f"image {name!r} not found in {self.images_dir}")
        return str(p)

    def create_vm(self, d: dict[str, Any]) -> dict[str, Any]:
        name = str(d.get("name", ""))
        if not NAME_RE.match(name):
            raise ValueError("invalid VM name (letters, digits, - _ . ; max 41)")
        if (state_root() / name / "vm.json").exists():
            raise ValueError(f"VM {name!r} already exists")
        disk = str(state_root() / name / "disk.qcow2")
        cfg = VMConfig(name=name, memory_mb=int(d.get("memory_mb", 2048)), cpus=int(d.get("cpus", 2)),
                       disk=disk, base_image=self._image(d.get("base_image")), iso=self._image(d.get("iso")),
                       disk_size=str(d.get("disk_size", "20G")), boot="d" if d.get("iso") and not d.get("base_image") else "c")
        if not re.fullmatch(r"\d+[MG]", cfg.disk_size) or not (128 <= cfg.memory_mb <= 65536) or not (1 <= cfg.cpus <= 32):
            raise ValueError("bad disk size / memory / cpus")
        VM(cfg).start()
        return {"name": name}

    def vm_power(self, name: str, op: str) -> None:
        vm = VM.load(name)
        with self._lock:
            q = self._qmp.pop(name, None)
            self._computers.pop(name, None)
        if q:
            q.close()
        if op == "start":
            vm.start()
        elif op == "stop":
            for r in self.runs.values():
                if r.vm == name:
                    r.stop.set()
            vm.stop()
        elif op == "delete":
            vm.stop()
            shutil.rmtree(vm.dir, ignore_errors=True)
        else:
            raise ValueError("bad op")

    def qmp(self, name: str) -> QMPClient:
        with self._lock:
            q = self._qmp.get(name)
            if q is None:
                vm = VM.load(name)
                if not vm.is_running():
                    raise FileNotFoundError(f"VM {name!r} is not running")
                q = self._qmp[name] = vm.qmp()
            return q

    def computer(self, name: str) -> Computer:
        q = self.qmp(name)
        with self._lock:
            c = self._computers.get(name)
            if c is None:
                c = self._computers[name] = Computer(q)
            return c

    def snapshot(self, name: str, op: str, tag: str) -> None:
        if not NAME_RE.match(tag):
            raise ValueError("invalid snapshot name")
        q = self.qmp(name)
        (q.savevm if op == "save" else q.loadvm)(tag)

    # -- runs ---------------------------------------------------------------------------------
    def start_run(self, vm: str, task: str, model: str | None = None, max_steps: int = 60) -> Run:
        if not task.strip():
            raise ValueError("empty task")
        if any(r.vm == vm and r.status == "running" for r in self.runs.values()):
            raise ValueError(f"VM {vm!r} is already running a task")
        router = self.router()
        decision = router.route(task, model or None)
        adapters = [self.adapter_factory(router, n) for n in decision.chain]
        q = self.qmp(vm)
        run = Run(time.strftime("%Y%m%d-%H%M%S-") + uuid.uuid4().hex[:4], vm, decision.prompt, decision.model,
                  decision.chain, time.time())
        self.runs[run.id] = run
        run_dir = self.runs_root / run.id
        run_dir.mkdir(parents=True, exist_ok=True)
        agent = Agent(Computer(q), Policy.from_dict(self.conf.get("policy")),
                      self.approvals.approver_for(run.id, run.stop), max_steps=max_steps,
                      run_dir=str(run_dir), os_name=self.conf.get("os_name", "Kush OS (based on ReactOS)"),
                      stop_event=run.stop)

        def work() -> None:
            try:
                rep = agent.run(decision.prompt, adapters)
                run.report = dict(rep.__dict__)
                run.status = rep.status
            except Exception as e:  # never kill the panel
                run.status, run.report = "failed", {"error": f"{type(e).__name__}: {e}"}
            (run_dir / "run.json").write_text(json.dumps(run.public(), ensure_ascii=False, indent=2), encoding="utf-8")

        (run_dir / "run.json").write_text(json.dumps(run.public(), ensure_ascii=False, indent=2), encoding="utf-8")
        threading.Thread(target=work, name=f"run-{run.id}", daemon=True).start()
        return run

    def list_runs(self) -> list[dict[str, Any]]:
        out = {r.id: r.public() for r in self.runs.values()}
        if self.runs_root.exists():
            for p in self.runs_root.iterdir():
                if p.name not in out and (p / "run.json").exists():
                    try:
                        d = json.loads((p / "run.json").read_text(encoding="utf-8"))
                        if d.get("status") == "running":
                            d["status"] = "interrupted"  # panel restarted while it ran
                        out[p.name] = d
                    except (OSError, ValueError):
                        pass
        return sorted(out.values(), key=lambda r: r.get("created", 0), reverse=True)

    def run_detail(self, rid: str) -> dict[str, Any]:
        if not re.fullmatch(r"[\w-]+", rid):
            raise FileNotFoundError(rid)
        run = next((r for r in self.list_runs() if r["id"] == rid), None)
        if run is None:
            raise FileNotFoundError(rid)
        steps = []
        f = self.runs_root / rid / "steps.jsonl"
        if f.exists():
            for line in f.read_text(encoding="utf-8").splitlines():
                try:
                    steps.append(json.loads(line))
                except ValueError:
                    pass
        return {**run, "steps": steps}

    def run_file(self, rid: str, fname: str) -> bytes:
        if not re.fullmatch(r"[\w-]+", rid) or not re.fullmatch(r"shot-\d{4}\.png", fname):
            raise FileNotFoundError(fname)
        return (self.runs_root / rid / fname).read_bytes()

    def stop_run(self, rid: str) -> None:
        r = self.runs.get(rid)
        if not r:
            raise FileNotFoundError(rid)
        r.stop.set()

    def overview(self) -> dict[str, Any]:
        a = detect()
        vms = self.list_vms()
        runs = self.list_runs()
        return {"accel": {"name": a.name, "hardware": a.hardware, "reason": a.reason},
                "vms_total": len(vms), "vms_running": sum(v["running"] for v in vms),
                "runs_total": len(runs), "runs_active": sum(r["status"] == "running" for r in runs),
                "approvals": len(self.approvals.list()), "models": self.models(),
                "qemu": bool(shutil.which("qemu-system-x86_64"))}


# ---------------------------------------------------------------------------------------------
# HTTP
# ---------------------------------------------------------------------------------------------
class HTTPError(Exception):
    def __init__(self, code: int, msg: str):
        super().__init__(msg)
        self.code = code


def make_handler(panel: Panel, secure_cookie: bool = False) -> type[BaseHTTPRequestHandler]:
    routes: list[tuple[str, re.Pattern[str], Callable[..., Any]]] = []

    def route(method: str, pattern: str):
        def deco(fn):
            routes.append((method, re.compile(f"^{pattern}$"), fn))
            return fn
        return deco

    @route("GET", r"/api/overview")
    def _ov(h, body, q):
        return panel.overview()

    @route("GET", r"/api/vms")
    def _vms(h, body, q):
        return panel.list_vms()

    @route("POST", r"/api/vms")
    def _vm_create(h, body, q):
        return panel.create_vm(body)

    @route("POST", r"/api/vms/(?P<name>[\w.-]+)/(?P<op>start|stop|delete)")
    def _vm_power(h, body, q, name, op):
        panel.vm_power(name, op)
        return {"ok": True}

    @route("POST", r"/api/vms/(?P<name>[\w.-]+)/snapshot")
    def _snap(h, body, q, name):
        panel.snapshot(name, body.get("op", "save"), str(body.get("tag", "")))
        return {"ok": True}

    @route("POST", r"/api/vms/(?P<name>[\w.-]+)/actions")
    def _act(h, body, q, name):
        res = panel.computer(name).run(str(body.get("name")), body.get("input") or {})
        out = {"is_error": res.is_error, "text": res.text}
        return out

    @route("GET", r"/api/vms/(?P<name>[\w.-]+)/screenshot")
    def _shot(h, body, q, name):
        shot = panel.computer(name).capture()
        return ("image/png", shot.png, {"X-Width": str(shot.geometry.shown_w), "X-Height": str(shot.geometry.shown_h)})

    @route("GET", r"/api/images")
    def _imgs(h, body, q):
        return {"dir": str(panel.images_dir), "images": panel.images()}

    @route("GET", r"/api/models")
    def _models(h, body, q):
        return panel.models()

    @route("GET", r"/api/route")
    def _route(h, body, q):
        d = panel.router().route(q.get("prompt", [""])[0], (q.get("model") or [None])[0])
        return d.__dict__

    @route("GET", r"/api/config")
    def _conf(h, body, q):
        return {"path": panel.config_path, "text": panel.config_text()}

    @route("PUT", r"/api/config")
    def _conf_put(h, body, q):
        panel.save_config(str(body.get("text", "")))
        return {"ok": True}

    @route("GET", r"/api/runs")
    def _runs(h, body, q):
        return panel.list_runs()

    @route("POST", r"/api/runs")
    def _run_start(h, body, q):
        return panel.start_run(str(body.get("vm", "")), str(body.get("task", "")), body.get("model") or None,
                               int(body.get("max_steps", 60))).public()

    @route("GET", r"/api/runs/(?P<rid>[\w-]+)")
    def _run(h, body, q, rid):
        return panel.run_detail(rid)

    @route("GET", r"/api/runs/(?P<rid>[\w-]+)/(?P<fname>shot-\d{4}\.png)")
    def _run_shot(h, body, q, rid, fname):
        return ("image/png", panel.run_file(rid, fname), {})

    @route("POST", r"/api/runs/(?P<rid>[\w-]+)/stop")
    def _run_stop(h, body, q, rid):
        panel.stop_run(rid)
        return {"ok": True}

    @route("GET", r"/api/approvals")
    def _apps(h, body, q):
        return panel.approvals.list()

    @route("POST", r"/api/approvals/(?P<pid>\w+)")
    def _app(h, body, q, pid):
        if not panel.approvals.answer(pid, bool(body.get("allow"))):
            raise HTTPError(404, "approval not found (expired?)")
        return {"ok": True}

    class Handler(BaseHTTPRequestHandler):
        server_version = "kush-admin"

        def log_message(self, fmt: str, *args: Any) -> None:
            pass

        def _send(self, code: int, ctype: str, data: bytes, headers: dict[str, str] | None = None) -> None:
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("X-Frame-Options", "DENY")
            self.send_header("Referrer-Policy", "no-referrer")
            for k, v in (headers or {}).items():
                self.send_header(k, v)
            self.end_headers()
            self.wfile.write(data)

        def _json(self, code: int, obj: Any, headers: dict[str, str] | None = None) -> None:
            self._send(code, "application/json; charset=utf-8", json.dumps(obj, ensure_ascii=False).encode(), headers)

        def _cookie(self) -> str | None:
            for part in (self.headers.get("Cookie") or "").split(";"):
                k, _, v = part.strip().partition("=")
                if k == "kush_session":
                    return v
            return None

        def _body(self) -> dict[str, Any]:
            n = int(self.headers.get("Content-Length") or 0)
            if n > 2 << 20:
                raise HTTPError(413, "request too large")
            raw = self.rfile.read(n) if n else b""
            try:
                return json.loads(raw) if raw else {}
            except ValueError:
                raise HTTPError(400, "invalid JSON")

        def _static(self, path: str) -> None:
            name = "index.html" if path in ("/", "/index.html") else path.lstrip("/")
            f = (STATIC / name).resolve()
            if f.parent != STATIC.resolve() or not f.is_file():
                return self._json(404, {"error": "not found"})
            ctype = {"html": "text/html; charset=utf-8", "js": "text/javascript; charset=utf-8",
                     "css": "text/css; charset=utf-8", "svg": "image/svg+xml"}.get(f.suffix.lstrip("."), "application/octet-stream")
            self._send(200, ctype, f.read_bytes(), {
                "Content-Security-Policy": "default-src 'self'; img-src 'self' blob: data:; style-src 'self' 'unsafe-inline' https://fonts.googleapis.com; font-src https://fonts.gstatic.com; script-src 'self'; frame-ancestors 'none'"})

        def _dispatch(self, method: str) -> None:
            url = urlparse(self.path)
            path = url.path
            try:
                if not path.startswith("/api/"):
                    if method != "GET":
                        raise HTTPError(405, "method not allowed")
                    return self._static(path)
                if method != "GET" and self.headers.get("X-Kush") != "1":
                    raise HTTPError(403, "missing X-Kush header")
                if path == "/api/login" and method == "POST":
                    token = panel.login(str(self._body().get("password", "")))
                    if not token:
                        raise HTTPError(401, "wrong password")
                    flags = "; Secure" if secure_cookie else ""
                    return self._json(200, {"ok": True}, {"Set-Cookie": f"kush_session={token}; HttpOnly; SameSite=Strict; Path=/; Max-Age={SESSION_TTL}{flags}"})
                if path == "/api/logout":
                    return self._json(200, {"ok": True}, {"Set-Cookie": "kush_session=; Max-Age=0; Path=/; HttpOnly; SameSite=Strict"})
                if not panel.valid_session(self._cookie()):
                    raise HTTPError(401, "login required")
                body = self._body() if method in ("POST", "PUT") else {}
                for m, rx, fn in routes:
                    if m == method and (mt := rx.match(path)):
                        res = fn(self, body, parse_qs(url.query), **mt.groupdict())
                        if isinstance(res, tuple):
                            return self._send(200, res[0], res[1], res[2])
                        return self._json(200, res)
                raise HTTPError(404, "not found")
            except HTTPError as e:
                self._json(e.code, {"error": str(e)})
            except FileNotFoundError as e:
                self._json(404, {"error": str(e)})
            except (ValueError, KeyError, yaml.YAMLError) as e:
                self._json(400, {"error": str(e)})
            except Exception as e:
                self._json(500, {"error": f"{type(e).__name__}: {e}"})

        def do_GET(self) -> None:
            self._dispatch("GET")

        def do_POST(self) -> None:
            self._dispatch("POST")

        def do_PUT(self) -> None:
            self._dispatch("PUT")

    return Handler


def serve(panel: Panel, host: str = "127.0.0.1", port: int = 8080, secure_cookie: bool = False) -> ThreadingHTTPServer:
    return ThreadingHTTPServer((host, port), make_handler(panel, secure_cookie))
