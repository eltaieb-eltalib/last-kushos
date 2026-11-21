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

**النتيجة: SPSS 25 بيشتغل على Kush 1.1 ✅** بالرخصة: شاشة البداية، وSPSS Statistics Viewer، و**Data Editor** بكل القوائم (Analyze، Graphs…)، و"IBM SPSS Statistics Processor is ready" (`24-spss25-data-editor-works.png`). المطلوب: تصليحات الـ kernel (الخطوات 9 و 10) + `IBM_JAVA_OPTIONS=-Xint` (الخطوة 13).

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
| 5 | `licenseinit.exe`: "MSVCR90.dll was not found" | مكتبة VC++ 2008 x64 ما متثبّتة، لأن `vcredist_x64.exe` غلاف 32-bit وفشل (Machine Type Mismatch) | `vc_red.msi` اتثبّت لكن ما سجّل في WinSxS، فاستخدمنا private assemblies (`tools/make_vc90_private.py`) | اتحلّت ✅ (CRT + MFC + ATL + OpenMP) |
| 6 | الـ MSI بيتخطّى custom actions بنسخة 32-bit (`ISSELFREG.DLL`، `ISSetup.dll`، `IsChainPackages.dll`) | `msi.dll` في ReactOS amd64 ما بيقدر يشغّل DLLs بنسخة 32-bit (ما في WoW64) | تسجيل مكونات COM يدويًا بـ `regsvr32` (64-bit)، أو WoW64 على المدى الطويل | مفتوح ❌ |
| 7 | **Fatal System Error 0x1E** أثناء تثبيت `vc_red.msi` من الـ CD | خطأ في الـ kernel (exception `0x80000016`) بعد `ole32` ما قدر يقفل الملف على الـ CD (error 158) | تشخيص: بيحصل بس لما الـ CD يتغيّر والنظام شغّال (`0x80000016` = `STATUS_VERIFY_REQUIRED` ما اتعالج في amd64). حل مؤقت: وصّل القرص قبل الإقلاع. الحل الدائم: تصليح معالجة verify في cdfs/kernel | اتشخّص ✅ / التصليح مفتوح |
| 10 | SPSS بيقفل لما ملف `.sav` يتمرّر في سطر الأوامر | access violation في `MSVCR100.dll+0x36ADF` | مؤقتًا: File ← Open من جوّا SPSS | مفتوح ❌ |
| 9 | SPSS بياخد 98% CPU وما بيفتح (سطح المكتب شكله متجمّد) | حلقة لا نهائية في thread بتاع Java JIT (`j9jit28.dll`) | `IBM_JAVA_OPTIONS=-Xint` في `hivesys.inf` | **اتحلّت ✅** |
| 8 | سطح المكتب بيتجمّد بعد ما `stats.exe` يبدأ (نافذة SPSS ما بتترسم) | **deadlock في الـ kernel (Mm):** Java J9 بيعمل `VirtualProtect` على الـ shared classes cache (ملف memory-mapped)، و `MmProtectSectionView` بياخد page fault وهو ماسك الـ address space lock، وبقية الـ threads (منها thread الواجهة) بتقيف على نفس الـ lock | `IBM_JAVA_OPTIONS=-Xshareclasses:none` كمتغيّر نظام في `boot/bootdata/hivesys.inf` | **اتحلّت ✅** (الحل الجذري في الـ kernel في الخطة) |
| 4 | **"Machine Type Mismatch"**: `The image file ...SPSS_Statistics_25_win64.exe is valid, but is for a machine type other than the current machine` | برنامج التثبيت (InstallShield bootstrapper) هو **32-bit (PE32 / i386)** رغم إن SPSS نفسه 64-bit، و ReactOS amd64 **ما فيه WoW64** فما بيشغّل أي برنامج 32-bit | اتجاوزناه: استخرجنا حزمة الـ MSI الـ 64-bit وثبّتناها بـ `msiexec` (المحاولة 2 تحت) | اتجاوزناه ✅ |

### المحاولة 2: تثبيت حزمة الـ MSI الـ 64-bit مباشرة: SPSS 25 اتثبّت واشتغل ✅

1. استخرجت حزمة الـ MSI من برنامج التثبيت بأداة `tools/isx_extract.py`
   (نسخة Python من ISx لصيغة InstallShield "ISSetupStream"):
   `python3 tools/isx_extract.py "<overlay من 7z>" out/` ← طلع
   `IBM SPSS Statistics 25.msi` (740 MB، **x64**) + ملفات `.mst` للغات.
2. شرط النظام في الـ MSI (`LaunchCondition`) هو XP أو أحدث بس
   (`VersionNT>=501`)، فـ "NT 5.2" **ما بيمنع** التثبيت. ما احتجنا قاعدة توافق لـ `msiexec`.
3. على Kush: `msiexec /i D:\spss25.msi /L*v C:\spss.log`
   - ✅ واجهة InstallShield Wizard فتحت (`02-msi-wizard-welcome.png`).
   - ✅ صفحة الترخيص، اختيار Python (اخترنا No)، مجلد التثبيت
     `C:\Program Files\IBM\SPSS\Statistics\25\`، و "Ready to Install" (`03-...png`).
   - ✅ التثبيت اشتغل (`04-msi-installing.png`).
   - ⚠️ أثناء التثبيت: `licenseinit.exe` (أداة الترخيص) فشلت:
     **"MSVCR90.dll was not found"** (`05-licenseinit-msvcr90-missing.png`).
     السبب: مكتبة Visual C++ 2008 x64 ما متثبّتة.
   - ⚠️ الـ MSI حاول يثبّت `vcredist_x64.exe` (VC++ 2008 x64)، وده كمان **غلاف 32-bit**،
     فطلع "Machine Type Mismatch" (`06-vcredist-x64-32bit-mismatch.png`).
   - ✅ **"InstallShield Wizard Completed": SPSS 25 اتثبّت** في
     `C:\Program Files\IBM\SPSS\Statistics\25\` (`07-msi-install-completed.png`).
   - ⚠️ سجل الـ serial بيوضّح إن الـ MSI **اتخطّى custom actions بنسخة 32-bit**:
     `ISSELFREG.DLL` (تسجيل مكونات COM)، `ISSetup.dll`، `IsChainPackages.dll`:
     `msi/custom.c: "ISSELFREG.DLL" is a 32 bit custom action. Returning as ERROR_SUCCESS`.
     يعني التثبيت **ناقص**: بعض مكونات SPSS ما اتسجّلت.
4. استخرجت `vc_red.msi` + `vc_red.cab` (VC++ 2008 SP1 x64، 9.0.30729.17) من
   `vcredist_x64.exe` بـ 7z، وحاولت أثبّتها على Kush بـ `msiexec` من الـ CD.
   - ❌ **النظام وقع: `Fatal System Error 0x0000001E`** (KMODE_EXCEPTION_NOT_HANDLED)،
     exception `0x80000016` @ `0xFFFFF880759E2B9F`، بعد
     `ole32/filelockbytes.c: no mapping for error 158` (قراءة الـ MSI من الـ CD).
   - من `C:` الـ MSI اشتغل من غير ما يقع، لكن **ما ثبّت VC90 في WinSxS**
     (`C:\ReactOS\WinSxS` ما فيه `*vc90*`)، فـ `stats.exe` لسه بيقول
     "MSVCR90.dll was not found" (`08-stats-exe-msvcr90.png`).
5. **تشخيص الكراش 0x1E:** حصل مرتين في **نفس العنوان** `0xFFFFF880759E2B9F`،
   والمرتين بعد ما **غيّرت الـ CD والنظام شغّال**. `0x80000016` = `STATUS_VERIFY_REQUIRED`
   (الإشارة اللي بيرفعها درايفر الـ CD لما القرص يتغيّر)، والـ kernel في amd64 ما
   بيعالجها. يعني **باق في Kush/ReactOS لتغيير الـ CD**، ما ليه علاقة بـ SPSS.
   الحل المؤقت: وصّل القرص قبل الإقلاع.
6. **حل MSVCR90 (private assembly):** `tools/make_vc90_private.py` بيبني مجلدات
   `Microsoft.VC90.CRT/MFC/ATL/OpenMP/MFCLOC` من `vc_red.cab` بـ manifest نسخته
   `9.0.21022.8` (النسخة اللي SPSS طالبها)، وبتتنسخ جوّا مجلد SPSS (`09-...png`).
   - ✅ بعد CRT: الخطأ اتغيّر لـ `Could not find dependent assembly Microsoft.VC90.MFC`،
     يعني CRT اتحلّت.
   - ✅ بعد إضافة MFC/ATL/OpenMP/MFCLOC: **ما في أي خطأ مكتبات**، و `stats.exe`
     اشتغل: نافذة **"SPSS Statistics"** ظهرت في شريط المهام
     (`10-stats-exe-started-window-in-taskbar.png`).
   - ✅ السجل بيأكّد إن قاعدة التوافق بتاعتنا اتطبّقت على SPSS:
     `ShimInfo(Exe(stats.exe))` → `ShimInfo(Complete)` (تعديل `media/sdb/sysmain.xml`).
7. ❌ **النتيجة الحالية: سطح المكتب كله بيتجمّد بعد ما SPSS يبدأ.** نافذة SPSS ما
   بتترسم، والساعة واقفة، ولوحة المفاتيح ما بتستجيب
   (`11-desktop-frozen-after-stats-start.png`). الـ kernel شغّال (`HLT=1`، والسجل
   بيتحدّث)، يعني المشكلة في الواجهة (win32k/user32) أو في SPSS واقف ينتظر حاجة
   (غالبًا Java أو مكونات COM اللي ما اتسجّلت في مشكلة #6).

8. **تشخيص التجمّد (مشكلة #8):**
   - ✅ Java لوحدها شغّالة: `java -version` (IBM J9 1.8.0 amd64) (`12-java-version-works.png`)،
     وواجهة Java (`policytool`) فتحت عادي (`13-java-gui-policytool-works.png`)، وشاشة بداية شفافة
     (`-splash:` بـ PNG فيها alpha) ما عملت مشكلة.
   - وقت التجمّد الماوس بيتحرك والمعالج idle (`HLT`)، يعني **deadlock** مش بطء.
   - ربطت gdb بالـ VM (`qemu -s`) وقريت هياكل الـ kernel من الذاكرة (`ntkrnlmp.exe`،
     offsets محسوبة من الـ headers): thread الواجهة (820) و 4 threads لـ Java كلهم واقفين في
     `KiAcquireGuardedMutex ← MmNotPresentFault ← MmAccessFault`، وthread 856 (من `j9shr28.dll`،
     الـ shared classes cache) واقف في
     `ZwProtectVirtualMemory → MmProtectSectionView/MmAlterViewAttributes` ووقع في page fault
     وهو ماسك الـ address space lock، فـ `MmNotPresentFault` حاول ياخد نفس الـ lock تاني:
     **self-deadlock في Mm بتاع ReactOS amd64.**
   - **الحل:** نوقف الـ shared classes cache في J9: `-Xshareclasses:none`.
     - ✅ مع `set IBM_JAVA_OPTIONS=-Xshareclasses:none`: شاشة بداية SPSS 25 ظهرت
       (`14-spss25-splash-fixed.png`)، وبعدها **SPSS Statistics Viewer** فتح وسطح المكتب
       شغّال عادي (`15-spss25-viewer-opened-no-license.png`).
     - الحل الدائم في Kush: `boot/bootdata/hivesys.inf` بيضيف
       `IBM_JAVA_OPTIONS=-Xshareclasses:none` كمتغيّر نظام، فبيطبّق على كل برامج IBM Java
       (SPSS 20 وطالع) من غير أي إعداد.

9. **الحل الجذري في الـ kernel (`ntoskrnl/mm/region.c`):** بعد ما ثبّتنا الرخصة، التجمّد رجع
   حتى مع `-Xshareclasses:none`. قريت الـ trap frame من الـ kernel stack بتاع الـ thread الواقف
   (`ZwProtectVirtualMemory`): الخطأ في **`MmAlterViewAttributes+0x45`** على العنوان **`0x68`**،
   يعني NULL pointer: `MmLocateMemoryAreaByAddress` رجّع NULL.
   - **السبب:** في `MmAlterRegion`،
     `CurrentBaseAddress = StartAddress + NewRegion->Length` بيفترض إن `NewRegion` بيبدأ من
     `StartAddress`، وده صحيح بس لما الـ region يتقسم. لما ما يتقسم (الـ region أصلًا عنده نفس
     الحماية) `NewRegion` بيبدأ من `InitialBaseAddress`، فالعنوان **بيتخطّى نهاية الـ view**
     وبيتنادى `MmAlterViewAttributes` على عنوان ما عنده memory area. ولأن `0x68` عنوان
     user، الـ kernel عامله كـ page fault عادي وحاول ياخد الـ address space lock اللي هو
     ماسكه، فحصل **deadlock بدل شاشة زرقاء**.
   - **التصليح:** `region.c` بيحسب العنوان من `InitialBaseAddress` لما ما في تقسيم، و
     `section.c` (`MmAlterViewAttributes`) بيرجع بأمان لو الـ memory area NULL.
   - ✅ بعد التصليح: الـ debugger بيأكّد إن **ما في أي thread واقف في `KiAcquireGuardedMutex`**،
     وكل threads الواجهة في SPSS بتنتظر رسائل عادي.
   - ⏳ لسه في تجمّد تاني منفصل في الشاشة: thread الواجهة في Explorer حالته **Ready** والمعالج
     idle (`HLT`)، يعني غالبًا **باق في الجدولة على أكتر من معالج (SMP)**. قيد التجربة بمعالج واحد.

10. **سبب تاني: ساعة النظام بتموت (`KeTickCount` واقف).** في التجمّد بعد تصليح Mm، قريت
    `KeTickCount` مرتين بفرق 15 ثانية: **ثابت (21516)**، يعني interrupt الساعة (RTC) وقف تمامًا،
    والـ threads بتفضل في حالة Ready والمعالج idle. ده حصل كمان بمعالج واحد (`-smp 1`)،
    يعني مش باق SMP. متغيّرات الـ HAL (`HalpSetClockRate`، `HalpNextClockRate`،
    `HalpCurrentClockRate=10`) بتوضّح إن **ما في تغيير لسرعة الساعة** (ما في `timeBeginPeriod`).
    - **السبب المرجّح:** تتبعات KUSH-DBG في `NtAllocateVirtualMemory` و `MiInsertVadEx` بتطبع
      سطر على الـ serial debugger **لكل عملية حجز ذاكرة**، و Java بتحجز ذاكرة باستمرار من
      threads كتيرة، فالـ interrupts بتكون مقفولة معظم الوقت.
    - **التصليح:** التتبعات بقت تطبع بس وقت `InbvCheckDisplayOwnership()` (يعني وقت ما
      bootvid ماسك الشاشة في الإقلاع، وده الوقت الوحيد اللي محتاجينها فيه حسب الـ README).
    - ✅ النظام لسه بيقلع عادي (ما رجع باق bootvid)، و ✅ **الساعة ما بتموت**: `KeTickCount` بيزيد
      (37605 → 38250 في 10 ثواني) والـ SPSS شغّال.
11. ⏳ **المشكلة الباقية:** الشاشة ما بتتحدّث (ساعة شريط المهام واقفة والكتابة ما بتظهر) رغم إن
    الساعة شغّالة وكل threads الواجهة (explorer و csrss و stats.exe) في `GetMessage` عادي ومافي
    thread واقف في lock. يعني المشكلة غالبًا في رسم الشاشة أو توصيل الـ input في win32k
    (مثلًا window شفافة لـ SPSS "StatsTransparent" فوق الشاشة، أو الـ desktop ما بيترسم).
    الخطوة الجاية: نقرا قائمة النوافذ وحالة الـ foreground/capture في win32k من الذاكرة.

12. **فحوصات التجمّد الباقي (الشاشة ما بتتحدّث):**
    | الفحص | النتيجة | المعنى |
    |-------|---------|--------|
    | `KeTickCount` قبل وبعد 10 ثواني | زاد (37605 → 38250) | ساعة النظام شغّالة ✅ |
    | حالة الـ threads (gdb) | كل threads الواجهة في `GetMessage`، ما في thread في lock | ما في deadlock ✅ |
    | Ctrl+Alt+Del | شاشة Security ظهرت (`17-ctrl-alt-del-works-during-freeze.png`) | لوحة المفاتيح والرسم و Winlogon شغّالين ✅ |
    | Task Manager | سطح المكتب رجع اترسم، والساعة اتحدّثت، و SPSS "Running" مش "Not responding" (`18-...png`) | الواجهة مش واقفة ✅ |
    | تبويب Processes | **`stats.exe` بياخد 98% CPU** (CPU time 9:13، ذاكرة 29 MB) (`19-stats-exe-cpu98.png`) | SPSS **مشغول**، مش متجمّد |
    | عينات RIP من QEMU (12 عينة) | معظمها في نطاق `0x273e000`–`0x2757000` (CPL=3)، وده جوّا `j9jit28.dll` حسب قائمة الـ modules | Java بتعمل **JIT compile** |
    - **الاستنتاج:** "التجمّد" الحالي هو إن SPSS بياخد كل المعالج وهو بيبدأ (Java JIT)، فسطح
      المكتب ما بيترسم. ده بطيء جدًا هنا لأن QEMU بيحاكي المعالج بالـ software (TCG، بدون KVM)،
      وعلى جهاز حقيقي المفروض يكون أسرع بكتير. لسه محتاج نتأكد إن الواجهة بتفتح في النهاية.

13. **الحل النهائي للتجمّد: إيقاف الـ JIT بتاع Java (`-Xint`).**
    | الفحص | النتيجة |
    |-------|---------|
    | عينات RIP + حالة الـ threads (gdb) | thread 880 حالته **Running** ووقته في user mode **73,693 tick (~19 دقيقة)**، والـ RIP جوّا `j9jit28.dll` (0x20e0000–0x2b50000)، وبقية الـ threads منتظرة عادي. والذاكرة ثابتة 29 MB، يعني **حلقة لا نهائية في الـ JIT** مش تقدّم |
    | إنهاء `stats.exe` من Task Manager | CPU نزل لـ 2% وسطح المكتب رجع طبيعي (`20-...png`) |
    | `set IBM_JAVA_OPTIONS=-Xint` وتشغيل SPSS تاني | شاشة البداية بعد ~3 دقايق (`21-xint-splash.png`)، وبعدها **Viewer فتح بكل القوائم و"Processor is ready"** و CPU 1-2% (`22-...png`) |
    | الرخصة (`lservrc`) | ✅ "Your temporary usage period for IBM SPSS Statistics will expire in 3327 days" (`23-license-active-3327-days.png`) |
    | Data Editor | ✅ Untitled1 [DataSet0] فتح بـ Data View و Variable View (`24-spss25-data-editor-works.png`) |
    - **التطبيق الدائم:** `boot/bootdata/hivesys.inf` بقى بيضيف `IBM_JAVA_OPTIONS=-Xint` كمتغيّر نظام
      (بدل `-Xshareclasses:none` اللي ما كفى). `-Xint` بيخلّي Java أبطأ، لكنه شغّال.
    - رسالة "An unknown file type is specified on the command line: ...stats.exe" سببها إني شغّلته بـ
      `start stats.exe` من `cmd` (المسار فيه مسافات)، وما بتأثّر على حاجة.

14. **فتح ملف `.sav` حقيقي (استبيان بالعربي، 20 متغيّر، 19.5 KB):**
    | الفحص | النتيجة |
    |-------|---------|
    | `stats.exe D:\rawia.sav` (الملف في سطر الأوامر) | ❌ SPSS قفل: `KdSendPacket: Got exception 0xc0000005 @ 0x79766ADF` (جوّا `MSVCR100.dll`، اللي بيبدأ من `0x79730000`) (`25-...png`) |
    | `stats.exe` من غير ملف | ✅ شاشة "Welcome to IBM SPSS Statistics" فتحت بعد ~5 دقايق، وفيها "You have 3,327 days left" (`26-welcome-dialog.png`) |
    | "Open another file..." ← Open dialog ← `D:\rawia.sav` | ✅ (`27-open-dialog.png`) **الملف فتح**: `rawia.sav [DataSet1]`، **Visible: 20 of 20 Variables**، النصوص العربية ظاهرة صاح في Data View، وأسماء المتغيرات وعناوين الأسئلة بالعربي ظاهرة في Variable View، و"IBM SPSS Statistics Processor is ready" |
    - صور محتوى الملف ما اترفعت للمستودع لأنها بيانات استبيان خاصة والمستودع عام.
    - **مشكلة جديدة:** فتح ملف من سطر الأوامر (أو بالضغط على ملف `.sav`) بيقفل SPSS. الحل المؤقت:
      افتح SPSS وبعدها File ← Open. الخطوة الجاية: نعرف الدالة اللي في `MSVCR100.dll+0x36ADF`.

**ملاحظة عن ملف الرخصة:** ملف التفعيل (`lservrc`) اتثبّت في الـ VM بس للتجربة، و**ما اترفع للمستودع**،
لأن المستودع عام وأي زول يقدر ينسخ الرخصة.

### أدوات التشخيص (في `tools/kdbg/`)
- `walk_threads.py`: سكربت gdb (مع `qemu -s`) بيقرا الـ processes والـ threads من ذاكرة الـ kernel
  ويطبع حالة كل thread، والـ kernel stack والـ user stack بأسماء الدوال، والـ trap frames.
- `offsets.c`: بيحسب offsets الهياكل (`EPROCESS`، `ETHREAD`، `KTRAP_FRAME`...) من headers بتاعة Kush.
- `ticks.py`: بيقرا `KeTickCount` عشان نعرف إن ساعة النظام شغّالة.

### ملخص الوصلنا ليه

| المرحلة | النتيجة |
|---------|---------|
| تشغيل برنامج التثبيت الأصلي (32-bit) | ❌ Machine Type Mismatch، اتجاوزناها |
| تثبيت حزمة الـ MSI (x64) | ✅ اتثبّت (مع تخطّي custom actions بنسخة 32-bit) |
| مكتبات VC++ 2008 x64 | ✅ اتحلّت بـ private assemblies |
| `stats.exe` يبدأ | ✅ بدأ، والنافذة اتعملت |
| واجهة SPSS تظهر وتشتغل | ✅ مع `-Xint`: Viewer + Data Editor + الرخصة شغّالين |
| حلقة لا نهائية في Java JIT | ✅ اتجاوزناها بـ `-Xint` (الحل الجذري في الخطة) |
| فتح ملف `.sav` حقيقي | ✅ من File ← Open (20 متغيّر، عربي سليم). ❌ من سطر الأوامر بيقفل |
| deadlock في Mm (`region.c`) | ✅ اتصلّح، والـ debugger أكّد |
| ساعة النظام بتموت | ✅ اتصلّحت (تقليل تتبعات KUSH-DBG) |

## الخطة المستقبلية لتشغيل SPSS 25 على Kush

المراحل القديمة **اتنفّذت**: استخراج الـ MSI وتثبيته، وحل مكتبات VC++ 2008، وحل تجمّد الشاشة. SPSS 25 بيفتح.
الخطوات الجاية مرتّبة حسب الأولوية:

### المرحلة 1: إرجاع الـ JIT (عشان SPSS يبقى أسرع)
- `-Xint` بيشغّل Java بالـ interpreter بس، وده أبطأ. الخطوة الجاية نحدّد ليه الـ JIT بيلف:
  نطلّع `j9jit28.dll` ونعمل disassemble للعنوان `j9jit28.dll+0x67764f` اللي الـ thread واقف فيه،
  ونشوف بينتظر في شنو (غالبًا API في ReactOS بترجع قيمة غلط، زي معلومات المعالج أو الذاكرة).
- نجرب خيارات أخف من `-Xint`: `-Xjit:disableAsyncCompilation` أو `-Xnoaot` أو `-Xjit:count=0`.
- `-Xshareclasses:none` ما بقى محتاج، لأن `region.c` اتصلّح.
- بعد كده: نفتح ملف `.sav` ونعمل Frequencies (معيار النجاح الكامل).

### المرحلة 2: مكونات COM اللي ما اتسجّلت (مشكلة #6)
- نطلع قائمة الملفات اللي كان `ISSELFREG.DLL` المفروض يسجّلها (جدول `ISSelfReg` في
  الـ MSI بـ `msiinfo export ... ISSelfReg`)، ونسجّلها يدويًا بـ `regsvr32` (64-bit).

### المرحلة 3: تصليحات في Kush نفسه (تفيد كل البرامج)
1. **msi.dll:** يثبّت Win32 side-by-side assemblies (`MsiAssembly`) في `WinSxS`،
   عشان ما نحتاج private assemblies.
2. **تغيير الـ CD (مشكلة #7):** معالجة `STATUS_VERIFY_REQUIRED` في amd64 بدل الـ bugcheck 0x1E.
3. **bootvid (مشكلة #1/#2):** تصليح الـ race الحقيقي عشان نرجّع خيار الإقلاع العادي.

### المرحلة 4: حلول بديلة لو الطريق ده اتقفل
- **Kush 32-bit (i386) + SPSS 25 32-bit:** ReactOS 32-bit أنضج، وبيشغّل برامج التثبيت
  الـ 32-bit والـ custom actions مباشرة. محتاج نقل تعديلات Kush (UEFI/GOP/AHCI) لنسخة i386،
  وجهاز يقبل Legacy/CSM أو UEFI 32-bit.
- **WoW64 في ReactOS amd64:** بيحل برامج التثبيت والـ custom actions الـ 32-bit، لكنه مشروع
  كبير جدًا وما بيضمن إن واجهة SPSS تشتغل.
- **Remote Desktop:** Kush فيه `mstsc`، فممكن تستخدم SPSS من جهاز ويندوز تاني مباشرة.

### أدوات الاستعملناها (في المستودع)
- `tools/isx_extract.py`: بيطلّع الملفات من برنامج تثبيت InstallShield "ISSetupStream"
  (زي `SPSS_Statistics_25_win64.exe`)، ومنها `IBM SPSS Statistics 25.msi`.
- `tools/make_vc90_private.py`: بيبني مجلدات VC++ 2008 x64 (private assemblies) من `vc_red.cab`.

### معيار النجاح
SPSS 25 يتثبّت، و `stats.exe` يفتح، ويقرأ ملف `.sav`، ويطلع جدول Frequencies.
