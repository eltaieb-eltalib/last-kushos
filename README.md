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

## تجربة Firefox 160.0.1 (64-bit) على Kush 1.1 ❌ (محتاج ويندوز 10)

المصدر: `https://ftp.mozilla.org/pub/firefox/releases/160.0.1/win64/en-US/Firefox Setup 160.0.1.exe` (مجاني، 95.8 MB).

| # | الفحص | النتيجة | الحل |
|---|-------|---------|------|
| 1 | نوع ملف التثبيت | `Firefox Setup 160.0.1.exe` هو **PE32 (32-bit، i386)، مضغوط بـ UPX** (غلاف NSIS)، يعني Kush الـ 64-bit (ما فيه WoW64) غالبًا حيرفضه زي SPSS | نفك الملف بدل ما نشغّله |
| 2 | فك الملف بـ `7z x` | ✅ 76 ملف (362 MB)، ومنهم **`core/firefox.exe` بنسخة 64-bit (PE32+ x86-64)**، و `setup.exe` | ننسخ مجلد `core` لـ Kush ونشغّل `firefox.exe` مباشرة |
| 3 | تشغيل `Firefox Setup 160.0.1.exe` على Kush | ❌ **"Machine Type Mismatch"** (`docs/firefox/01-installer-machine-type-mismatch.png`)، زي ما توقعنا | الفك بـ 7z (الخطوة 2) |
| 4 | `xcopy /E /I D:\core C:\Firefox` | ✅ 75 ملف اتنسخت | |
| 5 | تشغيل `C:\Firefox\firefox.exe` | ❌ **"api-ms-win-core-version-l1-1-1.dll was not found"** (`docs/firefox/02-api-ms-win-core-version-missing.png`) | Firefox 160 محتاج ويندوز 10 أو أحدث، وبيستعمل مكتبات "API set" (`api-ms-win-*`) ناقصة في Kush. ⏳ الحل: نضيف الـ API sets الناقصة (forwarders لـ `version.dll` و `kernel32`/`kernelbase`) |
| 6 | ليه الـ API set ناقص؟ | Kush فيه جدول API sets (`sdk/lib/apisets/apisets.table.c`، 1248 مدخل) وفيه `api-ms-win-core-version-l1-1-1`، لكنه معلّم `APISET_WIN10`. الـ loader (`ntdll/rtl/libsupp.c`، `LdrpApisetVersion`) **بيفعّل الـ API sets بس لما البرنامج مصنّف ويندوز 10** (`RosGetProcessCompatVersion`)، وفايرفوكس بيشتغل بإصدار Kush العادي (NT 5.2) | نشغّله بطبقة التوافق `WIN10RTM` |
| 7 | `set __COMPAT_LAYER=WIN10RTM` وتشغيل فايرفوكس | ⚠️ تقدّم: الـ API set اتفعّل واتحوّل، لكن لـ `kernel32.dll`، فطلع **"Entry Point Not Found: GetFileVersionInfoSizeW in api-ms-win-core-version-l1-1-1.dll"** (`docs/firefox/03-win10-layer-entry-point-not-found.png`) | في ويندوز 10 الدوال دي في `kernelbase.dll`، وفي Kush مكانها `version.dll`. ⏳ الحل: نحوّل `api-ms-win-core-version-*` لـ `version.dll` في جدول الـ API sets |
| 8 | فحص كامل لكل الدوال اللي فايرفوكس بيستوردها ومقارنتها بـ Kush (`tools/firefox_api_audit.py`، النتيجة في `docs/firefox/api-audit-firefox160.txt`) | ❌ **الفجوة كبيرة:** 126 دالة ناقصة في `kernel32.dll` (SRW locks، `AddDllDirectory`، thread pool…)، و 32 في `ntdll.dll` (ALPC بتاع الـ sandbox)، و 12 في `advapi32.dll` (ETW، `RegDeleteTreeW`…)، و 8 في `user32.dll` (DPI)، و ~40 API set ما ليها مدخل Win10 في الجدول، و ~13 API set بتتحوّل لمكتبة ما فيها الدالة، و 9 مكتبات ما موجودة أصلًا (`dxgi`، `dcomp`، `mf`، `mfplat`، `coremessaging`، `bcp47langs`، `profapi`، `ktmw32`، `rometadata`) | شوف الاستنتاج تحت |

### خطة تشغيل Firefox 160 نفسه (قيد التنفيذ)

الفحص الذكي (`tools/firefox_api_audit.py`) لأول سلسلة تحميل (`firefox.exe` + `mozglue.dll` + `msvcp140` +
`vcruntime140`) لقى **97 دالة ناقصة**، من 3 أنواع:

1. **API sets ما معلّمة لويندوز 10** في `apisets.table.c` (synch، file، memory، processthreads، registry،
   security، sysinfo، version، winrt-string…)، فما بتتفعّل حتى بطبقة `WIN10RTM`.
2. **API sets بتتحوّل للمكتبة الغلط:** registry و security لازم يمشوا لـ `advapi32.dll`، و version لـ
   `version.dll`، و winrt و winrt-string لـ `combase.dll`.
3. **دوال `kernel32`/`ntdll` الحديثة:** اكتشاف مهم: **معظمها موجود في الكود ومخفي.** `kernel32.spec`
   و `ntdll.spec` معلّمين `AcquireSRWLockExclusive` و `CompareStringEx` و `CreateThreadpoolWork`
   و `GetTickCount64` و `RtlAcquireSRWLockExclusive`… بـ `-version=0x600+`، و Kush اتبنى بـ
   `DLL_EXPORT_VERSION=0x502` (ويندوز 2003). ReactOS عنده "وضع NT6" بيصدّر الدوال دي مباشرة.
   الدوال اللي ما موجودة في أي مكان لسه: `CreateFile2`، `CopyFile2`، `CreateEventExW`،
   `CreateSemaphoreExW`، `WaitOnAddress`/`WakeByAddress*`، `SetDefaultDllDirectories`،
   `*ProcThreadAttributeList`، `Get/SetProcessMitigationPolicy`، `SetThreadInformation`،
   `QueryUnbiasedInterruptTime(Precise)`، `RtlQueryPerformanceCounter`، `Wer*RuntimeExceptionModule`…

**الخطوات:**
- **أ.** ✅ بناء Kush بـ `DLL_EXPORT_VERSION=0x600` (Vista): `cmake ... -DDLL_EXPORT_VERSION=0x600` ثم
  `ninja bootcd`، **نجح بالكامل** (ISO 340 MB) مع تعديلات ب و ج و د. مشاكل البناء اللي ظهرت واتحلّت:
  تعارض `CopyFile2` مع التعريف الرسمي (بقى بيستعمل `COPYFILE2_EXTENDED_PARAMETERS`)، و 4 دوال طلعت
  موجودة أصلًا في `kernel32` (`CreateWaitableTimerExA/W`، `FlushProcessWriteBuffers`،
  `GetCurrentPackageId`) فاتشالت من `kush_win8.c`.
- **ب.** ✅ تصليح `sdk/lib/apisets/apisets.table.c`: علامة `APISET_WIN10` اتضافت لـ **332** API set كانت
  Win7/8/8.1 بس (ويندوز 10 بيدعمها)، و **9** اتحوّلت للمكتبة الصحيحة (registry و security و sddl ←
  `advapi32`، و version ← `version.dll`، و winrt و winrt-string و winrt-error ← `combase.dll`، و psapi ←
  `psapi.dll`)، واتضاف `api-ms-win-core-libraryloader-l1-2-1` و `api-ms-win-core-realtime-l1-1-1`.
  الجدول لسه مرتّب (1111 مدخل، فحص الترتيب بيمر).
- **ج.** ⏳ كتابة الدوال الما موجودة (الكود اترفع، البناء والتجربة جاريين):
  - `dll/win32/kernel32/kush/kush_win8.c` (جديد): **31 دالة** ويندوز 7/8/10 مبنية على دوال Kush الموجودة:
    `CreateFile2`، `CopyFile2`، `CreateEventExW/A`، `CreateWaitableTimerExW/A`، `SetWaitableTimerEx`،
    `WaitOnAddress`، `WakeByAddressSingle/All`، `TryAcquireSRWLockShared`، `SetDefaultDllDirectories`،
    `AddDllDirectory`، `RemoveDllDirectory`، `Initialize/Update/DeleteProcThreadAttributeList`،
    `Get/SetProcessMitigationPolicy`، `Get/SetThreadInformation`، `SetFileInformationByHandle`،
    `GetLogicalProcessorInformationEx`، `FlushProcessWriteBuffers`، `GetCurrentPackageFullName/Id`،
    `QueryUnbiasedInterruptTime(Precise)`، `QueryInterruptTimePrecise`، `Wer(Un)RegisterRuntimeExceptionModule`.
    الحاجات الما موجودة في Kush أصلًا (packages، mitigation policies، WER) بترجع نفس نتيجة ويندوز لما
    الخاصية مقفولة.
  - `kernel32.spec`: الدوال الجديدة بتتصدّر، و **68 دالة** فايرفوكس محتاجها والكود بتاعها موجود اتشال منها
    شرط الإصدار (SRW locks، thread pool، `InitOnce*`، `CompareStringEx`، `GetTickCount64`،
    `GetSystemTimePreciseAsFileTime`، `K32EnumProcessModules`…).
  - `ntdll.spec` + `rtl/libsupp.c`: `RtlTryAcquireSRWLockExclusive` اتصدّرت، واتكتبت
    `RtlQueryPerformanceCounter` و `RtlQueryPerformanceFrequency`.
  - البناء بـ `DLL_EXPORT_VERSION=0x600` (Vista) عشان دوال `-version=0x600+` تتصدّر.
- **د.** ✅ تصنيف فايرفوكس كويندوز 10 تلقائيًا:
  - `sdk/tools/xml2sdb`: دعم `<LAYER_REF NAME="..."/>` جوّا `<EXE>` (بيكتب `TAG_LAYER` + `TAG_NAME`،
    و `apphelp` بيحلّه بالاسم في `SdbpAddExeLayers`).
  - `media/sdb/sysmain.xml`: `firefox.exe` و `plugin-container.exe` و `crashreporter.exe` ← طبقة
    `WIN10RTM` (بتبلّغ عن NT 10.0 وبتفعّل API sets بتاعة ويندوز 10 في الـ loader).
- **هـ.** ✅ نفس الشي لـ `xul.dll` (المحرك). الفحص على البناء الجديد لقى 32 دالة ناقصة، واتكتبت كلها:
  - `apisets.table.c`: بعض الأسماء في الجدول بحروف مختلطة (`api-MS-Win-Security-Base-L1-1-0`)، فاتصلّحت
    المقارنة: security-base ← `advapi32`، و psapi ← `kernel32` (فيها `K32*`)، و path ← `kernelbase_ros`
    (فيها `PathCch*`)، و shcore-scaling ← `shcore.dll`.
  - `kush_win8.c` (دورة 2): `GetOverlappedResultEx`، `PowerCreate/Set/ClearRequest`، `ResolveLocaleName`،
    `GetCurrentApplicationUserModelId`، `GetSystemCpuSetInformation`، و forwarders في `kernel32.spec`:
    `OpenProcessToken`/`OpenThreadToken`/`SetThreadToken` ← `advapi32`.
  - `dll/win32/advapi32/kush/kush_etw.c` (جديد): ETW (`EventRegister`، `EventWrite`، `EventWriteTransfer`،
    `EventWriteEx`، `EventSetInformation`، `EventUnregister`، `EventEnabled`، `EventProviderEnabled`،
    `EventActivityIdControl`)، و `RegCreateKeyTransactedW`/`RegOpenKeyTransactedW` (من غير transaction).
  - `sdk/lib/rtl/amd64/dynfntbl.c`: `RtlAddGrowableFunctionTable` و `RtlGrowFunctionTable` و
    `RtlDeleteGrowableFunctionTable` (محتاجها JIT بتاع JavaScript لتسجيل unwind data)، ومتصدّرة من `ntdll`.
  - `dll/win32/cfgmgr32/kush_cm.c`: `CM_Register_Notification`/`CM_Unregister_Notification`.
  - `dll/win32/ktmw32/` **(مكتبة جديدة)**: `CreateTransaction`، `CommitTransaction`، `RollbackTransaction`.
  - ✅ **البناء نجح، والفحص (`tools/firefox_load_chain_audit.py`) ما لقى أي دالة ناقصة** في سلسلة
    تحميل `firefox.exe` ولا `xul.dll`.
- **و.** ⏳ التجربة الفعلية في Kush (الدوال اللي بتتحمّل وقت التشغيل بـ `GetProcAddress` والـ delay-load
  ممكن تطلّع مشاكل تانية).

**الاستنتاج:** Firefox 160 **ما بيشتغل على Kush 1.1 هسي**. هو مبني لويندوز 10 وما فوق (Mozilla وقّفت دعم
ويندوز 7 و 8 بعد Firefox 115)، و Kush بيقلّد Windows Server 2003 مع جزء من دوال Vista و 7. المشكلة ما
في مكتبة واحدة، الفجوة **مئات الدوال**.

**طرق الحل:**
1. **الأسرع (موصى بيه): Firefox ESR 115 بنسخة 64-bit.** آخر نسخة بتدعم ويندوز 7، فبتحتاج دوال أقل بكتير،
   ومعظمها موجود في `kernel32_vista` و API sets بتاعة Win7 في Kush. الرابط:
   `https://ftp.mozilla.org/pub/firefox/releases/115.18.0esr/win64/en-US/` (نفس طريقة الفك بـ 7z).
2. **متوسط:** نصلّح الأشياء السهلة اللي ظهرت في الفحص، وده بيفيد كل البرامج الحديثة مش فايرفوكس بس:
   - `apisets.table.c`: `api-ms-win-core-version-*` → `version.dll`، و `api-ms-win-core-psapi` → `psapi.dll`،
     و `api-ms-win-core-path` → `kernelbase`/`shlwapi`، وإضافة علامة `APISET_WIN10` للـ API sets الأساسية
     (synch، processthreads، libraryloader، file، memory…) اللي معلّمة Win7/8 بس.
   - تصنيف `firefox.exe` كويندوز 10 تلقائيًا (مدخل في `media/sdb/sysmain.xml`، أو دعم `LAYER_REF` جوّا
     `EXE` في `xml2sdb` و `apphelp`)، بدل `set __COMPAT_LAYER=WIN10RTM`.
   - تصدير دوال Vista/7 من `kernel32.dll` (forwarders لـ `kernel32_vista.dll`).
3. **كبير (أسابيع):** تطبيق بقية دوال ويندوز 10 (ALPC في `ntdll`، و DPI في `user32`، و `dxgi`/`dcomp`،
   و Media Foundation) عشان النسخ الحديثة زي 160 تشتغل.

## ملخص الحالة (للمتابعة)

| الموضوع | الحالة | المكان في الكود |
|---------|--------|-----------------|
| SPSS 25 على Kush | ✅ بيثبت ويفتح ملفات `.sav` ويعمل تحليل (QEMU) | `ntoskrnl/mm/region.c`، `section.c`، `ARM3/virtual.c`، `vadnode.c`، `boot/bootdata/hivesys.inf` (`IBM_JAVA_OPTIONS=-Xint`) |
| رجفة الماوس | ✅ اتصلّحت في الكود، ⏳ محتاجة تأكيد على جهاز الـ Acer | `drivers/input/i8042prt/mouse.c`، `registry.c` |
| تعريف NVMe | ✅ القرص بيتعرّف، والتثبيت عليه والإقلاع منه شغّالين (QEMU)، ⏳ التجربة على جهاز حقيقي | `drivers/storage/port/nvme/`، `drivers/storage/port/storport/storport.c`، `fdo.c`، `txtsetup.sif`، `hivesys.inf` |
| الكيبورد والماوس PS/2 و USB، وكرت الشاشة (VBE/GOP)، و SATA/IDE/USB | ✅ موجودين في النظام من قبل | (ReactOS) |
| تاتشباد I2C | ❌ ما في تعريف (`hidi2c`) | — |

**الخطوات الجاية بالترتيب:**
1. ✅ ~~نكمّل تثبيت Kush على قرص NVMe في QEMU ونقلع منه~~ (اتعمل).
2. نجرب على جهاز الـ Acer: رجفة الماوس، والـ SSD (SATA أو NVMe).
3. نرجّع الـ JIT بتاع Java (نشيل `-Xint`) بعد ما نعرف سبب الحلقة في `j9jit28.dll`.
4. نصلّح فتح ملف `.sav` من سطر الأوامر (crash في `MSVCR100.dll`).
5. تعريف تاتشباد I2C (لو التاتشباد في الـ Acer من النوع ده).

**بيئة البناء والتجربة:** ReactOS `344ebfe5` + ملفات المستودع ده فوقه، `cmake -G Ninja -DARCH=amd64`
بـ mingw-w64 (نسخة posix)، و `ninja bootcd`. التجربة في QEMU (TCG) بخيار الإقلاع Debug، وأدوات
التشخيص في `tools/kdbg/`.

## التعريفات (Drivers) في Kush 1.1

الهدف: تعريفات قياسية جوّا النظام للماوس والكيبورد وكرت الشاشة والهاردسكات (HDD، SSD، NVMe)،
وحل مشكلة رجفة الماوس. كل مرحلة بتترفع في `main` مع توثيقها هنا.

### الوضع قبل الشغل (فحص المصدر والـ ISO)

| النوع | التعريف | موجود في النظام؟ |
|-------|---------|------------------|
| كيبورد PS/2 + ماوس/تاتشباد PS/2 | `i8042prt` + `kbdclass` + `mouclass` | ✅ |
| كيبورد وماوس USB | `usbxhci`/`usbehci`/`usbohci`/`usbuhci` + `hidusb` + `kbdhid` + `mouhid` | ✅ |
| ماوس serial | `sermouse` | ✅ |
| كرت الشاشة (قياسي) | `vgapnp.sys` (VBE + UEFI GOP، مع تصليح Kush) + `vga.sys` + `framebuf.dll` | ✅ |
| HDD/SSD بنظام SATA (AHCI) | `storahci` (Kush فعّله) | ✅ |
| HDD/SSD بنظام IDE | `pciide` + `uniata` + `atapi` | ✅ |
| فلاشات وهاردسكات USB | `usbstor` | ✅ |
| **SSD بنظام NVMe** | **ما في** ← اتضاف `nvme.sys` (المرحلة 2) | ❌ ← ✅ |
| تاتشباد I2C (لابتوبات حديثة) | ما في (`hidi2c`) | ❌ |

### المرحلة 1: رجفة الماوس (الماوس بيهتز في مكانه وبيقفز) ✅

**الأعراض (من المستخدم):** الماوس بيعمل اهتزاز، وفي مكانه، وبيقفز من محل لمحل.

**السبب (في `drivers/input/i8042prt`):** ماوس/تاتشباد PS/2 بيرسل كل حركة كـ packet من 3 أو 4 bytes.
لو byte واحد ضاع، الـ driver بيقرا الـ bytes الجاية في غير مكانها (byte الـ X كأنه أزرار، والـ Y كأنه X)،
فبتطلع حركات كبيرة غلط (قفز) وضغطات وهمية:
1. **زمن إعادة المزامنة كان 2 ثانية** (`DefaultMouseSynchIn100ns = 20000000`)، يعني الـ stream بيفضل
   غلط طول ما الماوس بيتحرك، وما بيتصلّح إلا لما يقيف ثانيتين.
2. **فحص أول byte ضعيف:** بيفحص bit 3 بس، فأي byte فيه البت ده بيتقبل كبداية packet.

**التصليح:**
- `registry.c`: زمن إعادة المزامنة بقى **50 ms** (bytes الـ packet الواحد بتجي في أقل من كده).
- `mouse.c`: لو أول byte فيه bits الـ overflow (6 و 7) مرفوعة، ده غالبًا byte في غير مكانه، فبنتجاهله
  وبنزامن مع الجاي (زي `psmouse` في Linux).

**الفحص:**
| الفحص | النتيجة |
|-------|---------|
| بناء `i8042prt.sys` | ✅ بدون أخطاء |
| تثبيته على Kush في QEMU والإقلاع | ✅ |
| تحريك الماوس للركن ثم +200 يمين و +100 تحت | ✅ المؤشر وصل (200، 100) بالضبط، ما في قفز (`docs/drivers/01-mouse-after-fix-exact-position.png`) |
| الرجفة نفسها | ⏳ ما بتظهر في QEMU (الماوس المحاكي ما بيضيّع bytes)، **التأكيد النهائي على جهاز الـ Acer** |

### المرحلة 2: تعريف NVMe (للـ SSD من نوع NVMe M.2): شغّال، والنظام بيقلع من NVMe ✅

**المصدر:** تعريف **OpenFabrics Alliance NVMe for Windows** (OFA nvmewin، رخصة BSD من Intel و IDT و LSI)
من `github.com/gigaherz/nvmewin`. ده StorPort miniport (نفس نوع `storahci`)، وحطيناه في
`drivers/storage/port/nvme/` (~21,000 سطر).

**التعديلات عشان يتبني بأدوات Kush/ReactOS:**
| المشكلة | التصليح |
|---------|---------|
| WPP tracing (`*.tmh`، `DoTraceMessage`) ما موجود | `nvme_tracing.h` بقى stubs، وملفات `.tmh` فاضية |
| `nvmeMofData.h` بيتولّد من MOF بأداة `wmimofck` | كتبته يدوي من `nvmeMofData.mof` (GUIDs وهياكل WMI) |
| تعارض `srb.h` مع `storport.h` | `precomp.h` بيعرّف `_NTSRB_` قبل `scsiwmi.h` |
| أنواع SAL (`HW_STARTIO`، `HW_TIMER_EX`…) ما موجودة | معرّفة في `precomp.h` |
| التعريف بيستهدف StorPort بتاع Windows 8 | `precomp.h` بيعرّف `NTDDI_VERSION = Win8` |
| `__try/__finally` بتاع MSVC | اتحوّلت لـ PSEH (`_SEH2_TRY/_SEH2_FINALLY/_SEH2_END`) |
| أنواع الإرجاع (`SNTI_STATUS`) و `DriverEntry` ما متطابقة مع GCC | اتصلّحت |
| `nvme.rc` بيضمّن ملف MOF مترجم (`.bmf`) | `nvme.rc` جديد بنمط ReactOS (الأصلي محفوظ في `nvme_ofa_original.rc.txt`) |
| `StorPortQuerySystemTime` و `memcpy_s` و `ScsiPortWmi*` ما بتتربط | `kush_compat.c` (WMI بيرجع "غير مدعوم"، والتخزين ما محتاجه) |

**التسجيل في النظام:**
- `txtsetup.sif`: نسخ `nvme.sys`، و `PCI\CC_010802 = nvme` (فئة NVMe: class 01، subclass 08، prog-if 02)، و `nvme = nvme.sys` في `[SCSI.Load]`.
- `hivesys.inf`: `CriticalDeviceDatabase\PCI#CC_010802` → `nvme`، و service `nvme` (Start=0، Group "SCSI Miniport").

**التجربة: برنامج تثبيت Kush في QEMU مع قرص NVMe بس (`-device nvme`، 8 GB)، وكل مشكلة وحلها:**
| # | النتيجة / سجل الـ serial | السبب | التصليح |
|---|--------------------------|-------|---------|
| 1 | ❌ "Setup could not find a harddisk" (`docs/drivers/02-nvme-setup-no-disk-before.png`)، والسجل وقف عند `NVMeEnumNumaCores: Failed to retrieve number of group` | StorPort بتاع Kush ما بيطبّق `StorPortExtendedFunction` لمجموعات المعالجات و NUMA وغيرها | `storport.c`: أضفت `GetCurrentProcessorNumber` و `GetActiveGroupCount` و `GetGroupAffinity` و `GetActiveNodeCount` و `GetNodeAffinity` و `GetHighestNodeNumber` و `AllocateContiguousMemorySpecifyCacheNode` و `FreeContiguousMemorySpecifyCache` و `FreeTimer` و `Acquire/ReleaseMSISpinLock` |
| 2 | ❌ STOP 0x7E في `storport.sys!PortFdoPnp+0x331` | `IoConnectInterruptEx` في الـ kernel بيرجع نجاح لـ MSI **من غير ما يعمل حاجة** ("UNIMPLEMENTED")، و StorPort بيقرا جدول MSI وهو NULL. **(الباق ده كان ممكن يضرب `storahci` كمان)** | `fdo.c`: لو الجدول NULL ده "ما في MSI"، وبنرجع لـ line interrupt عادي |
| 3 | ❌ `NVMeDriverFatalError` error 0x2 في state 37 (Set Features) | الـ controller رفض **Interrupt Coalescing** (اختياري في مواصفات NVMe)، والتعريف بيعتبره خطأ قاتل | `nvmeInit.c`: بقى تحذير والتشغيل بيكمّل |
| 4 | ❌ error 0x4 (`QUEUE_ALLOC_FAILURE`)، و `CDW11 = 0xFFFFFFFF` | بدون MSI (`Msg granted=0`)، عدد الـ queues = `min(cores, 0) - 1` = **65535**، فالـ controller رفض | `nvmeInit.c`: أقل عدد 1 (interrupt واحد) |
| 5 | ✅ الـ controller اشتغل: admin queues، RDY، Identify للـ controller والـ namespace، و I/O queue، و `NVMeStartComplete`، و `StorPortReady`. بعدها ❌ النظام وقف في DPC جوّا `StorPortExtendedFunction` ← `KeSetTimerEx` | `ExtFunctionRequestTimer` بيدّي `KeSetTimer` وقت **موجب**، وده تاريخ **مطلق** (1601)، فالـ timer بيضرب فورًا كل مرة، والتعريف بيعيد تشغيله كل ثانية من جوّا الـ callback: حلقة DPC بلا نهاية | `storport.c`: الوقت بقى سالب (نسبي) |
| 6 | ❌ STOP 0x7E في `storport.sys!PortPdoIssueRequest` عند أول قراءة من القرص | `DmaAdapter` = NULL: StorPort بيعمله بس جوّا `StorPortGetUncachedExtension`، وتعريف NVMe ما بيناديها | `fdo.c`: إنشاء الـ DMA adapter بعد `HwFindAdapter` (PASSIVE_LEVEL) من `PortConfig` |
| 7 | ✅ **REPORT LUNS و INQUIRY و READ CAPACITY اشتغلوا** (`LastLBA=0xffffff`، 512 byte = 8 GB)، و**برنامج التثبيت شاف القرص**: "8189 MB Harddisk 0 [RAW]"، "Unpartitioned space 8188 MB" (`docs/drivers/03-nvme-setup-disk-found.png`) | | |

| 8 | ✅ **الكتابة على NVMe شغّالة:** برنامج التثبيت عمل partition (MBR، FAT32، 8188 MB) وهيّأه، وبدأ ينسخ ملفات النظام (27% بعد 5 دقايق في QEMU) (`docs/drivers/04-nvme-setup-copying-files.png`) | | |

| 9 | ✅ **النسخ خلص، والنظام أعاد التشغيل و قلع من قرص NVMe:** وصل للمرحلة التانية من التثبيت ("Welcome to the ReactOS Setup Wizard")، والشاشة مكتوب فيها `C:\ReactOS` (`docs/drivers/05-nvme-booted-installed-system.png`). السجل فيه `NVMeRunning: StorPortReady` **مرتين** (برنامج التثبيت + النظام المثبّت)، والنظام بيقرا ملفاته من `C:\ReactOS` وبيرسل أوامر FLUSH للقرص | | |

**الحالة:** ✅ **تعريف NVMe شغّال بالكامل في QEMU:** بيتعرّف على القرص، وبيكتب ويقرا، وKush بيتثبّت على قرص NVMe و**بيقلع منه**. ⏳ الخطوة الجاية: التجربة على جهاز حقيقي فيه SSD من نوع NVMe.

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

15. **تحليل حقيقي (معيار النجاح): Frequencies على الملف.**
    - File ← New ← Syntax، وكتبت `FREQUENCIES VARIABLES=Var_2 Var_3 Var_4 Var_5 Var_6.` وشغّلتها (Ctrl+A ثم Ctrl+R).
    - ✅ **التحليل اشتغل** بعد ~90 ثانية: الـ Output Viewer عرض جدول **Statistics**
      (N Valid = 30 و Missing = 0 للخمسة متغيّرات) و **Frequency Table** لكل متغيّر (العمر،
      المؤهل العلمي، التخصص، المسمى الوظيفي، سنوات الخبرة) فيها Frequency و Percent و
      Valid Percent و Cumulative Percent، والعناوين والقيم العربية ظاهرة صاح.
    - **يعني SPSS 25 بيشتغل على Kush 1.1 من البداية للنهاية: تثبيت، ورخصة، وفتح ملف، وتحليل.**

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
| تحليل Frequencies | ✅ اشتغل (Statistics + 5 Frequency Tables) |
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
