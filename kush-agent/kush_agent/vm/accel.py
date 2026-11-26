"""Detect which hardware accelerator QEMU can use on this host.

Order of preference: kvm (Linux) -> whpx (Windows) -> hvf (macOS) -> tcg (pure software).
Hardware acceleration needs the CPU to expose virtualization (VT-x / AMD-V) to the host;
cloud VMs without nested virtualization only get TCG.
"""

from __future__ import annotations

import os
import platform
from dataclasses import dataclass


@dataclass(frozen=True)
class AccelInfo:
    name: str  # kvm | whpx | hvf | tcg
    hardware: bool
    reason: str

    def qemu_args(self) -> list[str]:
        if self.name == "kvm":
            return ["-accel", "kvm", "-cpu", "host"]
        if self.name == "whpx":
            # Keep tcg as an automatic fallback if WHPX fails to initialise.
            return ["-accel", "whpx", "-accel", "tcg", "-cpu", "qemu64"]
        if self.name == "hvf":
            return ["-accel", "hvf", "-cpu", "host"]
        return ["-accel", "tcg,thread=multi", "-cpu", "qemu64"]


def _cpu_flags(cpuinfo_path: str = "/proc/cpuinfo") -> set[str]:
    try:
        with open(cpuinfo_path, encoding="utf-8", errors="replace") as f:
            for line in f:
                if line.startswith("flags"):
                    return set(line.split(":", 1)[1].split())
    except OSError:
        pass
    return set()


def detect(
    system: str | None = None,
    kvm_path: str = "/dev/kvm",
    cpuinfo_path: str = "/proc/cpuinfo",
    forced: str | None = None,
) -> AccelInfo:
    """Return the best accelerator available. `forced` (or $KUSH_ACCEL) overrides detection."""
    forced = forced or os.environ.get("KUSH_ACCEL")
    if forced:
        return AccelInfo(forced, forced != "tcg", "forced by configuration")

    system = system or platform.system()
    if system == "Linux":
        flags = _cpu_flags(cpuinfo_path)
        has_virt = bool(flags & {"vmx", "svm"})
        if os.path.exists(kvm_path) and os.access(kvm_path, os.R_OK | os.W_OK):
            return AccelInfo("kvm", True, f"{kvm_path} is available")
        if os.path.exists(kvm_path):
            return AccelInfo("tcg", False, f"{kvm_path} exists but is not accessible (add user to the kvm group)")
        if has_virt:
            return AccelInfo("tcg", False, "CPU has vmx/svm but /dev/kvm is missing (load kvm_intel/kvm_amd)")
        return AccelInfo("tcg", False, "CPU does not expose vmx/svm (no nested virtualization) - software emulation only")
    if system == "Windows":
        # QEMU probes WHPX itself; with "-accel whpx -accel tcg" it falls back if the feature is off.
        return AccelInfo("whpx", True, "Windows host: enable 'Windows Hypervisor Platform' and VT-x in BIOS")
    if system == "Darwin":
        return AccelInfo("hvf", True, "macOS host: Hypervisor.framework")
    return AccelInfo("tcg", False, f"unknown host system {system!r}")
