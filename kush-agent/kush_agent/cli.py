"""Command line interface.

    kush-agent accel
    kush-agent vm start kush --iso kush.iso --disk ~/vms/kush.qcow2 --boot d
    kush-agent vm list | status NAME | stop NAME
    kush-agent vm screenshot kush -o screen.png
    kush-agent vm do kush left_click '{"coordinate": [100, 200]}'
    kush-agent vm snapshot kush save ready      (or: load ready)
    kush-agent guest kush ping                  (talk to kushagent.exe in the guest)
    kush-agent route "@local open notepad" --config config/models.yaml
    kush-agent run kush "Open Notepad and type hello" --config config/models.yaml
    kush-agent serve --port 8765                (REST control API, needs KUSH_API_TOKEN)
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

import yaml

from . import __version__
from .vm.accel import detect
from .vm.qemu import VM, VMConfig, state_root


def _load_yaml(path: str | None) -> dict:
    if not path:
        return {}
    with open(path, encoding="utf-8") as f:
        return yaml.safe_load(f) or {}


def cmd_accel(a: argparse.Namespace) -> int:
    info = detect()
    print(json.dumps({"accelerator": info.name, "hardware": info.hardware, "reason": info.reason,
                      "qemu_args": info.qemu_args()}, indent=2))
    return 0


def cmd_vm(a: argparse.Namespace) -> int:
    if a.vm_cmd == "start":
        cfg = VMConfig(name=a.name, memory_mb=a.memory, cpus=a.cpus, disk=a.disk, base_image=a.base_image,
                       disk_size=a.disk_size, iso=a.iso, boot=a.boot, loadvm=a.loadvm, accel=a.accel,
                       nic=a.nic, disk_if=a.disk_if, guest_channel=a.guest_channel)
        vm = VM(cfg)
        print(f"accelerator: {vm.accel.name} ({vm.accel.reason})")
        vm.start()
        print(f"started {a.name} (pid {vm.pid()}); VNC: {vm.vnc_address}; state: {vm.dir}")
        return 0
    if a.vm_cmd == "list":
        root = state_root()
        for p in sorted(root.iterdir()) if root.exists() else []:
            if (p / "vm.json").exists():
                print(f"{p.name}\t{'running' if VM.load(p.name).is_running() else 'stopped'}")
        return 0
    vm = VM.load(a.name)
    if a.vm_cmd == "stop":
        vm.stop()
        print(f"stopped {a.name}")
        return 0
    if not vm.is_running():
        print(f"VM {a.name} is not running", file=sys.stderr)
        return 1
    if a.vm_cmd == "status":
        with vm.qmp() as q:
            print(json.dumps({"name": a.name, "pid": vm.pid(), "accel": vm.accel.name, **q.status()}, indent=2))
        return 0
    from .tools.computer import Computer

    with vm.qmp() as q:
        if a.vm_cmd == "screenshot":
            shot = Computer(q, max_long_edge=a.max_edge).capture()
            Path(a.output).write_bytes(shot.png)
            g = shot.geometry
            print(f"saved {a.output} ({g.shown_w}x{g.shown_h}, guest screen {g.real_w}x{g.real_h})")
            return 0
        if a.vm_cmd == "do":
            res = Computer(q).run(a.action, json.loads(a.input) if a.input else {})
            if res.image:
                out = a.output or "action.png"
                Path(out).write_bytes(res.image.png)
                print(f"image saved to {out}")
            print(("ERROR: " if res.is_error else "") + (res.text or ""))
            return 1 if res.is_error else 0
        if a.vm_cmd == "snapshot":
            (q.savevm if a.op == "save" else q.loadvm)(a.tag)
            print(f"snapshot {a.op} {a.tag}: ok")
            return 0
    return 2


def cmd_guest(a: argparse.Namespace) -> int:
    from .vm.guest import GuestClient

    vm = VM.load(a.name)
    params = json.loads(a.params) if a.params else {}
    with GuestClient(vm.guest_address) as g:
        print(json.dumps(g.call(a.method, params), ensure_ascii=False, indent=2))
    return 0


def cmd_route(a: argparse.Namespace) -> int:
    from .models.router import Router, RouterConfig

    d = Router(RouterConfig.load(a.config)).route(a.prompt, a.model)
    print(json.dumps(d.__dict__, ensure_ascii=False, indent=2))
    return 0


def cmd_run(a: argparse.Namespace) -> int:
    from .agent.loop import Agent
    from .agent.policy import Policy, approve_all, terminal_approver
    from .models.router import Router, RouterConfig
    from .tools.computer import Computer

    conf = _load_yaml(a.config)
    router = Router(RouterConfig.from_dict(conf))
    decision = router.route(a.task, a.model)
    print(f"model: {decision.model} ({decision.reason}); fallback chain: {' -> '.join(decision.chain)}")
    adapters = [router.build(n) for n in decision.chain]

    vm = VM.load(a.name)
    if not vm.is_running():
        print(f"VM {a.name} is not running; start it with `kush-agent vm start`", file=sys.stderr)
        return 1
    guest = None
    if a.guest:
        from .vm.guest import GuestClient, GuestError

        try:
            guest = GuestClient(vm.guest_address, timeout=10).connect()
            guest.ping()
        except (OSError, GuestError) as e:
            print(f"guest agent unavailable ({e}); continuing without it")
            guest = None
    run_dir = Path(a.runs_dir) / time.strftime("%Y%m%d-%H%M%S")

    def show(kind: str, data: dict) -> None:
        if kind == "step" and data.get("text"):
            print(f"[{data['model']} #{data['n']}] {data['text']}")
        elif kind == "action":
            flag = " (ERROR: " + str(data.get("text")) + ")" if data.get("error") else ""
            print(f"  -> {data['name']} {json.dumps(data['input'], ensure_ascii=False)}{flag}")

    with vm.qmp() as q:
        agent = Agent(Computer(q, guest=guest), Policy.from_dict(conf.get("policy")),
                      approve_all if a.yes else terminal_approver, max_steps=a.max_steps,
                      max_seconds=a.max_seconds, run_dir=str(run_dir), os_name=conf.get("os_name", "Kush OS (based on ReactOS)"),
                      on_event=show)
        report = agent.run(decision.prompt, adapters)
    print(json.dumps(report.__dict__, ensure_ascii=False, indent=2))
    return 0 if report.status == "done" else 1


def cmd_serve(a: argparse.Namespace) -> int:
    from .agent.policy import Policy
    from .server import main_serve

    main_serve(a.host, a.port, a.approve_all, Policy.from_dict(_load_yaml(a.config).get("policy")))
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="kush-agent", description="External AI models controlling an OS in QEMU")
    p.add_argument("--version", action="version", version=__version__)
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("accel", help="show the hardware accelerator QEMU will use").set_defaults(fn=cmd_accel)

    vm = sub.add_parser("vm", help="manage virtual machines")
    vs = vm.add_subparsers(dest="vm_cmd", required=True)
    s = vs.add_parser("start")
    s.add_argument("name")
    s.add_argument("--iso")
    s.add_argument("--disk")
    s.add_argument("--base-image")
    s.add_argument("--disk-size", default="20G")
    s.add_argument("--memory", type=int, default=2048)
    s.add_argument("--cpus", type=int, default=2)
    s.add_argument("--boot", default="c", help="c = disk, d = cdrom")
    s.add_argument("--loadvm", help="start from an internal snapshot (e.g. 'ready')")
    s.add_argument("--accel", choices=["kvm", "whpx", "hvf", "tcg"])
    s.add_argument("--nic", default="e1000")
    s.add_argument("--disk-if", default="ide")
    s.add_argument("--guest-channel", default="serial", choices=["serial", "virtio"])
    vs.add_parser("list")
    for c in ("stop", "status"):
        vs.add_parser(c).add_argument("name")
    s = vs.add_parser("screenshot")
    s.add_argument("name")
    s.add_argument("-o", "--output", default="screen.png")
    s.add_argument("--max-edge", type=int, default=1568)
    s = vs.add_parser("do", help="run one computer action")
    s.add_argument("name")
    s.add_argument("action")
    s.add_argument("input", nargs="?", help="JSON input, e.g. '{\"coordinate\": [10, 20]}'")
    s.add_argument("-o", "--output")
    s = vs.add_parser("snapshot")
    s.add_argument("name")
    s.add_argument("op", choices=["save", "load"])
    s.add_argument("tag")
    vm.set_defaults(fn=cmd_vm)

    g = sub.add_parser("guest", help="call a method of the in-guest agent")
    g.add_argument("name")
    g.add_argument("method")
    g.add_argument("params", nargs="?")
    g.set_defaults(fn=cmd_guest)

    r = sub.add_parser("route", help="show which model a prompt would use")
    r.add_argument("prompt")
    r.add_argument("--config", required=True)
    r.add_argument("--model")
    r.set_defaults(fn=cmd_route)

    r = sub.add_parser("run", help="run an autonomous task on a VM")
    r.add_argument("name")
    r.add_argument("task")
    r.add_argument("--config", required=True)
    r.add_argument("--model", help="force a model alias from the config")
    r.add_argument("--max-steps", type=int, default=60)
    r.add_argument("--max-seconds", type=float, default=1800)
    r.add_argument("--runs-dir", default="runs")
    r.add_argument("--guest", action="store_true", help="connect to kushagent.exe for Unicode typing")
    r.add_argument("-y", "--yes", action="store_true", help="approve actions that need approval")
    r.set_defaults(fn=cmd_run)

    s = sub.add_parser("serve", help="REST API for external controllers")
    s.add_argument("--host", default="127.0.0.1")
    s.add_argument("--port", type=int, default=8765)
    s.add_argument("--config")
    s.add_argument("--approve-all", action="store_true")
    s.set_defaults(fn=cmd_serve)
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    return args.fn(args)


if __name__ == "__main__":
    sys.exit(main())
