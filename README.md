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
- **Default boot entry = "Live (Debug)"** (`boot/bootdata/bootcd.ini`) —
  verified in QEMU (amd64, TCG): the plain "Live Environment" entry still hits
  STOP 0x7E in `bootvid.dll` (no `/DEBUG`, so the KUSH-DBG `DbgPrint` traces
  don't slow the race down), while "Live (Debug)" (`/DEBUG /DEBUGPORT=COM1`)
  boots to the Kush 1.1 desktop. Revert to `DefaultOS=LiveImg` once the
  bootvid race is fixed.

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

## تقرير تجربة SPSS 25 (64-bit) على Kush 1.1

**النتيجة: SPSS 25 ما اشتغل.** برنامج التثبيت نفسه ما قدر يبدأ.

بيئة التجربة: Kush 1.1 مبني من `main` (ReactOS `344ebfe5` + تعديلات Kush)،
amd64 Release، مثبّت على قرص 20 GB (FAT32) في QEMU (بدون KVM، 3 GB RAM).
الملف المجرّب: `SPSS_Statistics_25_win64.exe` (IBM SPSS Statistics 25.0.0.0،
InstallShield، 735 MB)، اتوصّل للنظام كـ CD (`D:`) واتشغّل من `cmd.exe`.
الصور في `docs/spss25-test/`.

### المشاكل وطرق حلها

| # | المشكلة | السبب | الحل | الحالة |
|---|---------|-------|------|--------|
| 1 | الـ Live CD بخيار "Live Environment" بيعطي شاشة زرقاء STOP 0x7E في `bootvid.dll` | بدون `/DEBUG` تتبعات KUSH-DBG ما بتشتغل، فالـ race في bootvid بيظهر | الخيار الافتراضي في `boot/bootdata/bootcd.ini` بقى `LiveImg_Debug` | اتصلّح ✅ (اتأكدنا في QEMU) |
| 2 | النظام بعد التثبيت على الهارد بيقيف في شاشة سودا (الـ kernel عالق في bootvid، 640x480) | نفس سبب #1: `freeldr.ini` اللي بيكتبه الـ Setup بيختار `ReactOS` (بدون debug) | `base/setup/lib/bootsup.c`: الـ Setup بقى يخلّي `ReactOS_Debug` الافتراضي دايمًا | اتصلّح في الكود ✅ (التجربة اتعملت بتعديل `freeldr.ini` يدويًا وفتح سطح المكتب) |
| 3 | النظام بيبلّغ عن "NT 5.2" (Windows Server 2003) | ReactOS بيقلّد Windows 2003 | `media/sdb/sysmain.xml`: برامج SPSS بتاخد version lie لويندوز 7 SP1 | مضاف، لكن ما وصلنا لمرحلة نجربه فيها |
| 4 | **"Machine Type Mismatch"**: `The image file ...SPSS_Statistics_25_win64.exe is valid, but is for a machine type other than the current machine` | برنامج التثبيت (InstallShield bootstrapper) هو **32-bit (PE32 / i386)** رغم إن SPSS نفسه 64-bit، و ReactOS amd64 **ما فيه WoW64** فما بيشغّل أي برنامج 32-bit | ما اتحل لسه. شوف الخطة تحت | **مانع رئيسي** ❌ |

## الخطة المستقبلية لتشغيل SPSS 25 على Kush

المشكلة الأساسية إن برنامج التثبيت 32-bit. الخطة مرتّبة من الأسهل للأصعب:

### المرحلة 1: تجاوز برنامج التثبيت الـ 32-bit (الأقرب)
1. على جهاز ويندوز عادي: استخرج الحزمة الـ 64-bit الجوّا برنامج التثبيت:
   `SPSS_Statistics_25_win64.exe /extract_all:C:\spss25`
   أو شغّل برنامج التثبيت وانسخ الملفات اللي بيفكّها في `%TEMP%` قبل ما تقفله.
   الناتج المتوقّع: ملف `IBM SPSS Statistics 25.msi` (64-bit) وملفات معاه.
2. انقل المجلد لـ Kush (CD أو USB) وشغّل: `msiexec /i "IBM SPSS Statistics 25.msi"`.
   `msiexec.exe` في Kush 64-bit، فما بيحتاج WoW64.
3. لو الـ MSI رفض بسبب شرط ويندوز 7 (`VersionNT >= 601`):
   - أضف قاعدة توافق لـ `msiexec.exe` في `media/sdb/sysmain.xml` (زي قاعدة SPSS) أو
     في الـ registry: `HKLM\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers`
     القيمة `C:\ReactOS\system32\msiexec.exe` = `WIN7SP1`.
   - أو احذف صف الشرط من جدول `LaunchCondition` في الـ MSI بأداة Orca.

### المرحلة 2: المكتبات اللي SPSS 25 محتاجها بعد التثبيت
1. **Visual C++ 2015-2017 x64** (`vcruntime140.dll`, `msvcp140.dll`, `api-ms-win-crt-*`):
   ثبّت `vc_redist.x64.exe` (برنامج 64-bit)، وأصلح أي دالة ناقصة في ReactOS
   (`ucrtbase`/`kernel32`) تظهر في سجل الـ serial.
2. **Java 8 x64 المرفقة مع SPSS** (`jre\bin`): أول اختبار يكون
   `"C:\Program Files\IBM\SPSS\Statistics\25\JRE\bin\java.exe" -version`.
   لو Java ما اشتغلت، واجهة SPSS ما حتشتغل.
3. شغّل `stats.exe` وسجّل أي خطأ (الصورة + سجل الـ serial بخيار Debug).

### المرحلة 3: لو المرحلة 1 فشلت
- **نسخ مجلد جاهز:** ثبّت SPSS 25 (برخصة سليمة) على ويندوز 7/10 64-bit، وانسخ
  `C:\Program Files\IBM\SPSS\Statistics\25` ومعاه مكتبات VC++ لـ Kush، وجرّب
  `stats.exe` مباشرة. ممكن تحتاج مفاتيح registry من الجهاز الأصلي.
- **نسخة Kush 32-bit (i386):** ReactOS 32-bit أنضج، وبيشغّل برامج التثبيت الـ 32-bit.
  IBM عندها `SPSS_Statistics_25_win32.exe`، فالمسار ده
  (Kush i386 + SPSS 25 32-bit) واقعي، لكن محتاج نقل تعديلات Kush (UEFI/GOP/AHCI)
  لنسخة i386.

### المرحلة 4: الحل الجذري (مشروع كبير)
- إضافة **WoW64** لـ ReactOS amd64 عشان يشغّل البرامج الـ 32-bit. ده شغل كبير جدًا
  في الـ kernel و ntdll، ولسه ما موجود في ReactOS upstream.

### معيار النجاح
SPSS 25 يتثبّت، و `stats.exe` يفتح، ويقرأ ملف `.sav`، ويطلع جدول Frequencies.
