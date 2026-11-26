"""Launch and manage a QEMU virtual machine with the control channels the agent needs.

Every VM gets a state directory holding:
    qmp.sock   QMP control socket (screenshots, input, snapshots)
    vnc.sock   VNC display (live view for humans; bridged to the browser by noVNC)
    ga.sock    guest-agent channel (serial port inside the guest)
    vm.json    the configuration used to start it
    qemu.pid / qemu.log
On Windows hosts TCP ports on 127.0.0.1 are used instead of unix sockets.
"""

from __future__ import annotations

import json
import os
import platform
import shutil
import signal
import subprocess
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path

from .accel import AccelInfo, detect
from .qmp import QMPClient

IS_WINDOWS = platform.system() == "Windows"


def state_root() -> Path:
    return Path(os.environ.get("KUSH_STATE_DIR", Path.home() / ".kush-agent")) / "vms"


@dataclass
class VMConfig:
    name: str
    memory_mb: int = 2048
    cpus: int = 2
    disk: str | None = None            # qcow2 for this VM (created if missing)
    base_image: str | None = None      # golden image: disk becomes a copy-on-write overlay on it
    disk_size: str = "20G"             # used when creating an empty disk
    iso: str | None = None             # CD-ROM (installer / live CD)
    boot: str = "c"                    # c = disk, d = cdrom
    machine: str = "pc"                # pc (i440fx) suits ReactOS/Kush best
    disk_if: str = "ide"               # ide | virtio (virtio needs guest drivers)
    nic: str = "e1000"                 # e1000 | virtio-net-pci | none
    vga: str = "std"
    guest_channel: str = "serial"      # serial (COM port in guest) | virtio
    loadvm: str | None = None          # start from this internal snapshot
    accel: str | None = None           # force kvm|whpx|hvf|tcg; None = autodetect
    tcp_base_port: int = 47000         # Windows hosts: qmp=+0, guest=+1, vnc display=+2-5900
    extra_args: list[str] = field(default_factory=list)
    qemu_binary: str = "qemu-system-x86_64"


class VM:
    def __init__(self, config: VMConfig):
        self.config = config
        self.dir = state_root() / config.name
        self.accel: AccelInfo = detect(forced=config.accel)

    # -- addresses ------------------------------------------------------------------
    @property
    def qmp_address(self) -> str | tuple[str, int]:
        return ("127.0.0.1", self.config.tcp_base_port) if IS_WINDOWS else str(self.dir / "qmp.sock")

    @property
    def guest_address(self) -> str | tuple[str, int]:
        return ("127.0.0.1", self.config.tcp_base_port + 1) if IS_WINDOWS else str(self.dir / "ga.sock")

    @property
    def vnc_address(self) -> str:
        if IS_WINDOWS:
            return f"127.0.0.1:{self.config.tcp_base_port + 2 - 5900}"
        return f"unix:{self.dir / 'vnc.sock'}"

    def _chardev(self, ident: str, addr: str | tuple[str, int]) -> str:
        if isinstance(addr, tuple):
            return f"socket,id={ident},host={addr[0]},port={addr[1]},server=on,wait=off"
        return f"socket,id={ident},path={addr},server=on,wait=off"

    # -- disks ----------------------------------------------------------------------
    def prepare_disk(self) -> None:
        c = self.config
        if not c.disk or os.path.exists(c.disk):
            return
        qemu_img = shutil.which("qemu-img") or "qemu-img"
        Path(c.disk).parent.mkdir(parents=True, exist_ok=True)
        if c.base_image:
            cmd = [qemu_img, "create", "-f", "qcow2", "-b", os.path.abspath(c.base_image), "-F", "qcow2", c.disk]
        else:
            cmd = [qemu_img, "create", "-f", "qcow2", c.disk, c.disk_size]
        subprocess.run(cmd, check=True, capture_output=True)

    # -- command line ---------------------------------------------------------------
    def command(self) -> list[str]:
        c = self.config
        cmd = [c.qemu_binary, "-name", c.name, "-machine", c.machine, "-m", str(c.memory_mb), "-smp", str(c.cpus)]
        cmd += self.accel.qemu_args()
        cmd += ["-qmp", self._qmp_arg(), "-vnc", self.vnc_address, "-display", "none", "-vga", c.vga]
        cmd += ["-usb", "-device", "usb-tablet"]  # absolute pointer: exact clicks
        if c.disk:
            cmd += ["-drive", f"file={c.disk},format=qcow2,if={c.disk_if},cache=writeback"]
        if c.iso:
            cmd += ["-cdrom", c.iso]
        cmd += ["-boot", c.boot]
        if c.nic == "none":
            cmd += ["-nic", "none"]
        else:
            cmd += ["-netdev", "user,id=n0", "-device", f"{c.nic},netdev=n0"]
        cmd += ["-chardev", self._chardev("ga", self.guest_address)]
        if c.guest_channel == "virtio":
            cmd += ["-device", "virtio-serial", "-device", "virtserialport,chardev=ga,name=org.kush.agent.0"]
        else:
            # COM1 is often used by the ReactOS/Kush kernel debugger, so the agent gets COM2.
            cmd += ["-serial", "null", "-serial", "chardev:ga"]
        if c.loadvm:
            cmd += ["-loadvm", c.loadvm]
        if not IS_WINDOWS:
            cmd += ["-sandbox", "on,obsolete=deny,elevateprivileges=deny,spawn=deny,resourcecontrol=deny"]
        cmd += c.extra_args
        return cmd

    def _qmp_arg(self) -> str:
        a = self.qmp_address
        if isinstance(a, tuple):
            return f"tcp:{a[0]}:{a[1]},server=on,wait=off"
        return f"unix:{a},server=on,wait=off"

    # -- lifecycle ------------------------------------------------------------------
    def pid(self) -> int | None:
        try:
            return int((self.dir / "qemu.pid").read_text().strip())
        except (OSError, ValueError):
            return None

    def is_running(self) -> bool:
        pid = self.pid()
        if not pid:
            return False
        try:
            os.kill(pid, 0)
        except OSError:
            return False
        return True

    def start(self, wait: float = 30.0) -> "VM":
        if self.is_running():
            raise RuntimeError(f"VM {self.config.name} is already running (pid {self.pid()})")
        self.dir.mkdir(parents=True, exist_ok=True)
        for sock in ("qmp.sock", "ga.sock", "vnc.sock"):
            (self.dir / sock).unlink(missing_ok=True)
        self.prepare_disk()
        (self.dir / "vm.json").write_text(json.dumps(asdict(self.config), indent=2))
        log = open(self.dir / "qemu.log", "ab")
        kwargs = {"creationflags": 0x00000200} if IS_WINDOWS else {"start_new_session": True}
        proc = subprocess.Popen(self.command(), stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, **kwargs)
        (self.dir / "qemu.pid").write_text(str(proc.pid))
        deadline = time.time() + wait
        while time.time() < deadline:
            if proc.poll() is not None:
                raise RuntimeError(f"QEMU exited with code {proc.returncode}; see {self.dir / 'qemu.log'}")
            try:
                with QMPClient(self.qmp_address, timeout=5).connect(retries=1):
                    return self
            except Exception:
                time.sleep(0.2)
        raise TimeoutError(f"QMP did not come up within {wait}s; see {self.dir / 'qemu.log'}")

    def qmp(self) -> QMPClient:
        return QMPClient(self.qmp_address).connect()

    def stop(self, timeout: float = 10.0) -> None:
        if not self.is_running():
            return
        try:
            with self.qmp() as q:
                q.quit()
        except Exception:
            pass
        pid = self.pid()
        deadline = time.time() + timeout
        while pid and self.is_running() and time.time() < deadline:
            time.sleep(0.1)
        if pid and self.is_running():
            os.kill(pid, signal.SIGKILL if not IS_WINDOWS else signal.SIGTERM)
        (self.dir / "qemu.pid").unlink(missing_ok=True)

    @classmethod
    def load(cls, name: str) -> "VM":
        path = state_root() / name / "vm.json"
        if not path.exists():
            raise FileNotFoundError(f"no VM named {name!r} (looked for {path})")
        return cls(VMConfig(**json.loads(path.read_text())))
