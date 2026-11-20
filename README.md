# Kush 1.1 — last-kushos

Kush 1.1 is ReactOS (master, commit `344ebfe5`) with a Sudanese identity and the
fixes needed to boot on UEFI-only hardware (Acer). **Source only — no ISO in
this repo.**

## What's in this repo

This repo contains **only the files changed from ReactOS master commit
`344ebfe5`** — the Kush 1.1 modifications. To build the full system:

1. Clone ReactOS and check out the base commit:
   `git clone https://github.com/reactos/reactos.git`
   `git checkout 344ebfe5`
2. Copy every file from this repo over the checkout, preserving paths.
3. Build the 64-bit (amd64) system with RosBE x86_64:
   `configure.sh` then `ninja bootcd` (Release build with the KUSH-DBG
   serial traces kept — see below).
4. Make a GPT USB image (1.5 GB, one EFI System partition, FAT32) and copy
   `uefildr.efi` to `EFI/BOOT/BOOTX64.EFI`, `freeldr.ini` to the root, and the
   `reactos` tree. Write it with Rufus in DD mode, UEFI only, Secure Boot off.

## Modifications (vs ReactOS master 344ebfe5)

- **Sudanese rebrand** — ProductName "Kush 1.1", setup wizard / explorer /
  desktop & sound schemes renamed ReactOS → Kush, 4 Kush BMP wallpapers
  (Kush-Meroe default, Kush-Nile, Kush-Nubian, Kush-Temple, 1280x800), boot
  logo (teal #0D7377 + gold, "Kush 1.1", three pyramids), Lautus accents in
  Kush teal #0D7377. Internal `SOFTWARE\ReactOS` keys and version 0.4.16
  left untouched on purpose.
- **TrapFrame syscall fix** (`ntoskrnl/ke/amd64/traphandler.c`) —
  `KiSystemCallHandler` computed the trap frame 56 bytes too low (MinGW
  `_AddressOfReturnAddress()` vs 7 extra callee-saved registers pushed before
  RBP), corrupting syscall pointer arguments so `ZwAllocateVirtualMemory`
  returned SUCCESS but left Heap NULL. Fix: `TrapFrame = Thread->TrapFrame;`.
- **GOP video fallback** (`win32ss/drivers/miniport/vbe/gop.c`, new file) —
  reads the UEFI GOP framebuffer from the registry
  (`HARDWARE\DESCRIPTION\System\MultifunctionAdapter\0\DisplayController\0`,
  "Configuration Data") and `VBEInitialize` tries it FIRST, so the system
  boots a desktop on UEFI-only hardware (fixes STOP 0xB4). Key detail: a real
  `CM_PARTIAL_RESOURCE_DESCRIPTOR` is 20 bytes, not 32.
- **AHCI / storahci** (`drivers/storage/port/storahci/`) — storahci was built
  but not in the image and not registered; AHCI (PCI CC_0106) was mapped to
  legacy pciide. Enabled in CMakeLists + `hivesys.inf` + `txtsetup.sif`.
  Fixed `AhciStartPort` to always issue COMRESET (it only ran when the port
  was half-running) and extended the DET poll from 30 ms to 500 ms.
  Fixes STOP 0x7B (INACCESSIBLE_BOOT_DEVICE) on SATA SSDs.
- **HAL clock** (`hal/halx86/apic/halinit.c`) — added `HalpInitializeClock();`
  at the end of `HalpInitPhase0` (the kernel hung in the idle loop with no
  timer interrupt).
- **Boot fixes** — `boot/freeldr/freeldr/ntldr/arch/amd64/winldr.c`
  (`MempMapSinglePage` accepts the mapped page for the same frame); build
  fixes in `sdk/include/asm/asm.inc` and `sdk/include/asm/trapamd64.inc`.
- **SPSS compatibility** (`media/sdb/sysmain.xml`) — the built-in
  application-compatibility database (`AppPatch\sysmain.sdb`) now applies the
  Windows 7 SP1 version lie + `GlobalMemoryStatus2GB` to every IBM SPSS
  Statistics executable (`spsswin.exe`, `spss.exe`, `stats.exe`,
  `spssengine.exe`, `statisticsb.exe`, `spssprod.exe`), matched by file name so
  it covers all SPSS releases. ReactOS reports NT 5.2 and SPSS 20+ refuses to
  start below Windows 7. For the SPSS **installer**, right-click it →
  Properties → Compatibility → "Windows 7" before running it.
  Limits (not verified on real SPSS yet): Kush 1.1 is amd64 and ReactOS has
  no WoW64, so only the **64-bit** SPSS builds can run; whether a given SPSS
  version then works depends on ReactOS API coverage.

## Known notes

- **Keep the KUSH-DBG serial traces** in `ntoskrnl/mm/ARM3/heap.c`,
  `virtual.c` and `vadnode.c`. A clean build without them still crashes in
  `bootvid.dll` `VidBitBlt` (STOP 0x7E) — a timing-sensitive race the traces
  hide. The traces are serial-only and harmless.
- **No NVMe driver exists in ReactOS** — NVMe SSDs cannot be supported.
  SATA (AHCI) is what was fixed.
- **QEMU cannot fully verify SATA disk detection** (SSTS.DET stays 0 after
  COMRESET) — the decisive test is on real hardware (Acer). q35 breaks USB
  boot entirely (0x7B) and is not a usable test platform.
- The Meroe wallpaper does not appear on the master amd64 desktop (plain
  blue background) — cosmetic, pending.

## Status

- Last verified image: `kush11-amd64-uefi-ahci.img` (1.5 GB) — GOP works,
  storahci loads and finds the Intel AHCI controller (VEN 8086 DEV 2922,
  6 ports), system boots from USB in QEMU/OVMF. Real-hardware Acer test is
  decisive.
- If the Acer still gives 0x7B on USB boot: try a USB 2.0 port (xHCI issue)
  and report the Acer model and SSD type (SATA vs NVMe).
