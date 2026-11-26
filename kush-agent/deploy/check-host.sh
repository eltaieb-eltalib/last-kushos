#!/usr/bin/env bash
# Run on a VPS *before* relying on it: does it support hardware virtualization (KVM)?
#   curl -fsSL <raw-url>/check-host.sh | bash      or      bash check-host.sh
set -u
ok()   { printf '  \033[32m✔\033[0m %s\n' "$*"; }
bad()  { printf '  \033[31m✘\033[0m %s\n' "$*"; }
info() { printf '  • %s\n' "$*"; }

echo "== kush-agent host check =="
. /etc/os-release 2>/dev/null && info "OS: ${PRETTY_NAME:-unknown}"
info "CPU cores: $(nproc)   RAM: $(awk '/MemTotal/ {printf "%.1f GB", $2/1048576}' /proc/meminfo)   Free disk (/): $(df -h / | awk 'NR==2 {print $4}')"
virt=$(systemd-detect-virt 2>/dev/null || echo unknown); info "This machine is: ${virt} (none = bare metal)"

kvm_ok=0
if grep -qE '\b(vmx|svm)\b' /proc/cpuinfo; then ok "CPU exposes VT-x/AMD-V (vmx/svm)"; else bad "CPU does NOT expose vmx/svm -> no nested virtualization"; fi
if [ -e /dev/kvm ]; then
  ok "/dev/kvm exists"; kvm_ok=1
else
  bad "/dev/kvm missing"
  grep -qE '\b(vmx|svm)\b' /proc/cpuinfo && info "try: sudo modprobe kvm_intel || sudo modprobe kvm_amd"
fi

echo
if [ "$kvm_ok" = 1 ]; then
  ok "RESULT: KVM available -> VMs run at near-native speed. Good for production."
else
  bad "RESULT: only TCG (software emulation, roughly 5-20x slower)."
  info "Kush still runs (it is light), but every step of the agent will be slow."
  info "Ask the provider for 'nested virtualization' / 'KVM inside VPS', or use a dedicated (bare-metal) server."
fi
