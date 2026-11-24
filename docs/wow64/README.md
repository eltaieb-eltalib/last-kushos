# WoW64 في Kush 1.1: تشغيل برامج 32-bit (24 نوفمبر 2026)

أخدنا شغل WoW64 من فرع [`TheNNX/reactos` wow64sync](https://github.com/TheNNX/reactos/tree/wow64sync)
(Marcin Jabłoński، حوالي 311 ملف و27 ألف سطر) ودمجناه في Kush 1.1 (ReactOS 344ebfe5).
بعد الدمج، أي برنامج 32-bit كان بيقع. اتتبعنا الأعطال واحد واحد وصلّحناها من جذورها.

## النتيجة

| الاختبار | النتيجة |
|---|---|
| `X:\reactos\SysWOW64\cmd.exe /c "ver & echo WOW64-CMD-OK"` (cmd بنسخة x86) | ✅ شغال |
| `wow64test.exe` (12 اختبار: مؤشرات 32-bit، `IsWow64Process`، `GetNativeSystemInfo`، الملفات، الذاكرة، الخيوط، SEH، `CreateProcess`، الريجستري) | ✅ **12/12** |
| `guitest32.exe` (نافذة 32-bit فيها نص وزرار وcombobox، وMessageBox) | ✅ شغال |
| برامج 64-bit (سطح المكتب، explorer، cmd 64) في نفس البناء | ✅ شغالة |

صور الشاشة:
- [`screenshots/01-syswow64-cmd.png`](screenshots/01-syswow64-cmd.png): cmd بنسخة 32-bit
- [`screenshots/02-wow64test-12-of-12.png`](screenshots/02-wow64test-12-of-12.png): نجح 12 من 12
- [`screenshots/03-gui-32bit.png`](screenshots/03-gui-32bit.png): نافذة 32-bit
- [`screenshots/04-messagebox-32bit.png`](screenshots/04-messagebox-32bit.png): MessageBox من برنامج 32-bit

برامج الاختبار موجودة في [`tools/wow64_tests/`](../../tools/wow64_tests/).

## البناء

```
cmake ... -DARCH=amd64 -DBUILD_WOW64_ENABLED=1 -DWOW64_MINGW_TOOLCHAIN_SUFFIX=-win32
ninja bootcd
```

البناء ده محتاج كمان مترجم i686 (`i686-w64-mingw32-gcc`، نسخة الخيوط win32).
الـ ISO بيطلع فيه `reactos\SysWOW64` (حوالي 824 ملف 32-bit)، بالإضافة لـ `wow64.dll` و`wow64win.dll` و`wow64cpu.dll` (64-bit).
فيه مفتاح لإيقاف WoW64 من غير إعادة بناء: `HKLM\SYSTEM\CurrentControlSet\Control\Kush\Wow64`، القيمة `Enabled`
(0 يوقفه، والافتراضي 1).

## المشاكل وحلولها (بالترتيب اللي كانت بتوقّف بيه عملية 32-bit)

| # | العَرَض | السبب الجذري | الحل | الملف |
|---|---|---|---|---|
| 1 | أخطاء متكررة جوه `LdrpTryWow64Exception` لحد stack overflow، وبعدها BSOD | `KiUserExceptionDispatcher` (amd64) كان فيه `pop rdx / pop rcx` من غير ما يكون في `push` قبلها، فـ RSP بيتزحزح 16 بايت جوه الـ CONTEXT | شلنا الـ pop ومرّرنا المعاملات صريحة | `dll/ntdll/dispatch/amd64/dispatch.S` |
| 2 | الخيط "بينط" لـ `kernel32!BaseProcessStartup` بنسخة 64-bit | فضل `__debugbreak()` من مرحلة التطوير في `Wow64LdrpInitialize` وفي غيرها، فكل عملية بتوقف على breakpoint | شلنا الـ debugbreak (و`FIXME` بقى بيكتب في السجل وبس) | `dll/wow64/*.c` |
| 3 | تنفيذ عند العنوان 0 (916 مرة) وبعدها stack overflow | مسار SysWOW64 مكتوب ثابت `C:\ReactOS\SysWOW64`. على الـ Live CD (`X:\reactos`) أو أي مسار تثبيت تاني، `ntdll` الـ 32-bit ما بيتحمّل، فكل نقط الدخول بتفضل NULL | المسار بيتحسب وقت التشغيل من `SystemRoot`، والعملية بتتقفل بنظافة لو ntdll ما اتحمّلش | `dll/wow64/wow64.c`، `redirection.*`، `entrypoint.c` |
| 4 | الخيط برضه بيروح لعنوان 64-bit مش متعمله map | `BaseProcessStart`/`BaseThreadStart(Thunk)` كانوا متصدّرين بس لـ `-version=0x502`، وKush بيبني بنسخة تصدير أحدث، فترجمة نقطة الدخول ما بتلاقيهمش | شلنا قيد النسخة | `dll/win32/kernel32/kernel32.spec` |
| 5 | كل syscall من كود 32-bit بيقع وهو راجع | `push x2` في stubs الـ syscall اتجمّع كـ `push dword ptr [x2]` (FF 35)، وده في وضع 64-bit عنوان نسبي لـ RIP | `push offset x2` (68 imm32) | `sdk/include/asm/syscalls.inc` |
| 6 | `user32.dll` بيفشل في التهيئة (`0xc0000142`، `RegisterClientPFN failed`) | `RtlInitializeNtUserPfn` (جديد في upstream بعد شغل TheNNX): `PFNCLIENT` في بناء SysWOW64 فيه خانات 8 بايت، وntdll شايلها 4 بايت، فالأحجام عمرها ما بتتطابق | التسجيل ده بيتخطّى في بناء SysWOW64 (wow64win بيمسك الجداول الـ 32-bit بنفسه) | `win32ss/user/user32/controls/regcontrol.c` |
| 7 | AV في `Wow64Trampoline` بعد ما التهيئة الـ 32-bit ترجع | `LdrInitializeThunk` الـ 32-bit بيعمل `xor ebp, ebp`، و`EnterApc32` كان بيحفظ r12 بس، فبعد الرجوع RBP = 0 | بقينا نحفظ ونرجّع كل المسجّلات non-volatile | `dll/wow64/srasm.S` |
| 8 | تنفيذ على ذاكرة فاضية عند `BaseProcessStartThunk` | ‏kernel32 الـ 32-bit بيتحمّل على عنوان تاني (relocation)، والترجمة كانت محسوبة على العنوان المفضّل | العنوان بيتحسب من قائمة الموديولات الـ 32-bit (RVA + العنوان الحقيقي) | `dll/wow64/entrypoint.c` |
| 9 | ESI وEDI بيتبدّلوا في السياق الـ 32-bit | خطأ نسخ في `Wow64CopyContext64To32` | اتصحّح، ومعاه أعلام الـ ContextFlags (قيم i386 مش AMD64) ونسخ حالة FPU/SSE | `dll/wow64/wow64.c` |
| 10 | `EXCEPTION_RECORD` بيتكتب فوقه وهو بيتبعت لـ dispatcher الـ 32-bit | `CallOrJump32` كان inline، فـ GCC ممكن يحط بداية الـ stack الـ 32-bit فوق المتغيرات اللي بنبعتها | `noinline`: الـ frame بتاعها دايمًا تحت متغيرات اللي ناداها | `dll/wow64/ros_wow64_private.h` |
| 11 | كل استثناء 32-bit اتعالج بيتحوّل لـ "exception 0x0" | `wow64_NtContinue` كان معتمد على `BTCpuSetContext` (‏`ThreadWow64Context`)، والكيرنل ما بيطبّقهاش، فـ `ZwContinue` كان بيرجع وخلاص | بيحوّل لسياق native بـ CS=0x23 وبينادي `NtContinue` | `dll/wow64/apc.c` |
| 12 | الـ heap بيطفح في `RedirectPath` (مع `CreateProcess`) | المقارنة كانت بتقرا بعد `Length` بتاع الاسم، و`Length - From.Length` بيطلع سالب | فحص للطول وفاصل المسار، و`MaximumLength` بقى بيتظبط | `dll/wow64/redirection.c` |
| 13 | BSOD ‏0x1E في win32k لما نافذة 32-bit تتحمّل | المؤشرات الحديثة في Kush (‏.ani) فيها مصفوفة إطارات `CURSORDATA`، وكانت بتتبعت لـ win32k من غير تحويل | كل إطار بيتحوّل من 32 لـ 64 | `dll/wow64win/user_ros.c` |
| 14 | BSOD ‏0x1E بدل ما العملية بس تتقفل، لما الـ stack يخلص من أخطاء متكررة | الكيرنل كان بيحاول يكتب على stack مستخدم خلصان، ومسار الـ SEH جوه الكيرنل فشل | بنفحص حدود الـ stack من الـ TEB قبل ما نكتب، ولو خلصان الاستثناء بيروح second chance والعملية بس هي اللي تتقفل | `ntoskrnl/ke/amd64/except.c` |
| 15 | `GetSystemWow64Directory` بيرجّع `C:\ReactOS\SysWOW64` | مكتوب ثابت | `%SystemRoot%\SysWOW64` | `dll/win32/kernel32/client/path.c` |
| 16 | لو `wow64.dll` ما اتحمّلش بننادي مؤشر NULL | مافيش فحص | العملية بتتقفل برسالة | `dll/ntdll/ldr/ldrinit.c` |
| 17 | الأخطاء ما بتبانش | `DPRINT1` و`ASSERT` بيتشالوا في بناء Release | جوه wow64 بقوا بيكتبوا في السجل التسلسلي | `dll/wow64/ros_wow64_private.h` |

| 18 | برنامج تثبيت Firefox (32-bit) بيقف وبيطلع من غير ما يفك أي ملف. في السجل: `RtlpWaitForCriticalSection ... Deadlock` على `LdrpLoaderLock` بنسختيه (64 و32) | **عكس ترتيب الأقفال**: الخيط الجديد بياخد loader lock الـ 64-bit، وبعدين يشغّل تهيئة الخيط الـ 32-bit كلها (وهي بتاخد lock الـ 32-bit). في نفس الوقت الخيط الرئيسي ماسك lock الـ 32-bit (‏LoadLibrary لـ ws2_32/rpcrt4) وطالب lock الـ 64-bit، فالاتنين بيستنوا بعض للأبد | `LdrpInitializeThread` بيسيب lock الـ 64-bit قبل ما يدخل WOW64 | `dll/ntdll/ldr/ldrinit.c` |

الإصلاحات بتاعة البناء نفسه (i386 SysWOW64 بـ GCC 13، وتعارضات الدمج الـ 24) موثّقة في commits الفرع
(`b5a49c3b6`، `f6abaea4a`، `78cabbb8e`).

## المشاكل اللي لسه ما اتحلّتش

| # | المشكلة | الأثر | الخطوة الجاية |
|---|---|---|---|
| 1 | **الـ CI بيبني من غير WoW64** | الـ ISO اللي بيطلع من GitHub Actions ما فيهوش `SysWOW64`. لازم نبني محلي بـ `-DBUILD_WOW64_ENABLED=1` | نضيف مترجم i686 (‏RosBE i386 أو mingw-w64 win32) للـ workflow ونشغّل الفلاج |
| 2 | **فك تراجع (unwind) الـ SEH جوه الكيرنل بيفشل في حالات معيّنة** (`__C_specific_handler` ← `RtlUnwindEx` بيرجع ← `int3` ← 0x1E) | أي خطأ كيرنل بيتمسك بـ SEH في الحالات دي بيوقّع الجهاز كله. الحالتين اللي ظهروا (#13 و#14) اتقفلوا من جذرهم، لكن السبب في الـ unwinder لسه موجود | نتتبّع `RtlpUnwindInternal` في الكيرنل (حدود الـ stack لخيوط GUI/callbacks) |
| 3 | `ThreadWow64Context` مش متطبّق في الكيرنل | `Get/SetThreadContext` من/على عملية 32-bit (debuggers، بعض الـ installers) مش حيشتغل | نطبّقها في `NtQuery/SetInformationThread` |
| 4 | syscall ‏`NtSetInformationThread` (0xEE) مالوش thunk في wow64 | تحذير في السجل، والنداء بيرجع `STATUS_NOT_IMPLEMENTED` | نضيف thunk |
| 5 | Common Controls 6 (manifest) ما بتتلقاش في SysWOW64 | البرامج اللي عايزة comctl32 v6 بتشتغل بالشكل القديم | نضيف assembly الـ comctl32 v6 للـ WinSxS الـ 32-bit |
| 6 | حالة الـ x87/SSE في `Wow64CopyContext32To64` لسه ناقصة في بعض المسارات (FloatSave القديم) | برامج 32-bit بتستخدم x87 مع استثناءات ممكن تتأثر | نكمّل التحويل |
| 7 | ما جربناش لسه برامج تثبيت حقيقية (Firefox Setup 32-bit، InstallShield بتاع SPSS، vcredist x86) | ما نقدرش نقول إنها شغالة قبل ما نجرّبها | الاختبار الجاي |
| 9 | **في وضع الـ Live CD مجلد `TEMP` (`X:\reactos\TEMP`) للقراية بس**، فبرنامج تثبيت Firefox بيطلع "Can not create temp folder archive" | المشكلة دي مش من WoW64، وبرامج 64-bit كمان بتتأثر بيها. على النظام المتثبّت الـ TEMP بيتكتب فيه عادي | للتجربة على الـ Live CD: `set TEMP=C:\tmp` (على هارد)، أو نعمل RAM disk للـ TEMP في وضع الـ Live |
| 8 | الاختبار كله اتعمل على QEMU (‏Live CD)، مش على جهاز حقيقي ولا بعد التثبيت على الهارد | | نجرّب على نسخة متثبّتة وعلى جهاز حقيقي |
