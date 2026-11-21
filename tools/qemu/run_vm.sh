#!/bin/bash
# Kush 1.1: boot an installed Kush disk image headless in QEMU and open cmd.
# Usage: run_vm.sh <workdir> <disk.raw> [iso]
#   <workdir> gets mon.sock (QEMU monitor) and serial.log (all DbgPrint output).
# Then drive it with:
#   python3 vm.py <workdir> shot:name        -> <workdir>/name.png (screenshot)
#   python3 vm.py <workdir> sleep:5 ret      -> wait, press Enter (QEMU sendkey names)
#   python3 typ.py <workdir> $'text\n'       -> type text
#   python3 click.py <workdir> X Y           -> left click at screen X,Y
W=$1; DISK=$2; ISO=${3:-}; T=$(cd "$(dirname "$0")" && pwd)
pkill -9 -f qemu-system; sleep 1; rm -f "$W/serial.log"
qemu-system-x86_64 -m 4096 -cpu max -smp 2 -drive file="$DISK",format=raw ${ISO:+-cdrom "$ISO"} -boot c \
  -vga std -display none -serial file:"$W/serial.log" -monitor unix:"$W/mon.sock",server,nowait -daemonize
# Boot takes ~3-4 min under TCG (no KVM). Dismiss any dialog, open Run -> cmd.
python3 "$T/vm.py" "$W" sleep:220 esc sleep:2 meta_l-r sleep:6
python3 "$T/typ.py" "$W" $'cmd\n'; python3 "$T/vm.py" "$W" sleep:12
