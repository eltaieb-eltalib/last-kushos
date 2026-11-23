# Prompt لتنفيذ WoW64 في Kush 1.1 (تشغيل برامج 32-bit)

انسخ النص ده كله وابدأ بيه session جديد لوكيل برمجة (Claude Code أو Muse Code أو غيره) على المستودع:
**https://github.com/eltaieb-eltalib/last-kushos**

---

إنت شغال على **Kush 1.1**، نظام تشغيل مبني على ReactOS (amd64). المستودع
**https://github.com/eltaieb-eltalib/last-kushos** فيه **النظام كامل** (كل الكود وكل تعديلات Kush)، فبتبني منه
مباشرة من غير أي مستودع تاني.

**المهمة:** تضيف لـ Kush طبقة **WoW64**، عشان يشغّل برامج **32-bit (i386)** على النظام الـ 64-bit زي ويندوز بالظبط.
الهدف العملي: ملفات التثبيت دي تفتح **بالضغط عليها مباشرة** وتكمّل التثبيت وتعمل الأيقونات لوحدها:
- `Firefox Setup 160.0.1.exe` (غلاف NSIS بنسخة 32-bit)، من
  `https://ftp.mozilla.org/pub/firefox/releases/160.0.1/win64/en-US/`
- `vcredist_x64.exe` (Visual C++ 2008 SP1 x64، غلافه 32-bit)
- `SPSS_Statistics_25_win64.exe` (InstallShield، غلافه 32-bit)، **لو** المستخدم وفّر الملف. ما تنزّل برامج
  مدفوعة أو مقرصنة.

حاليًا أي برنامج 32-bit بيطلّع **"Machine Type Mismatch"**، والتفاصيل في README (أقسام Firefox وSPSS).

## قواعد ثابتة

1. **رد دايمًا بالعربي.**
2. **ما تسألني أي سؤال.** القرارات اللي ليها خيار معقول، خدها واكتبها في README. اسأل بس لو في حاجة مستحيلة من غيري.
3. **كل الشغل على مستوى النظام** (الكيرنل، `ntdll`، `wow64*.dll`، `kernel32`، المكتبات)، مش حل لبرنامج واحد.
   **ممنوع** تفك ملفات التثبيت أو تعمل استثناء لاسم برنامج معيّن. البرنامج الـ 32-bit لازم يشتغل زي ما هو.
4. **اشتغل على `main` مباشرة.** كل خطوة صغيرة شغالة: commit و push لـ `main` على طول، وما تجمّع شغل كتير من غير رفع.
5. **وثّق كل حاجة في `README.md` بالعربي:** المشكلة، والسبب الجذري، والحل، والصور، وحدود الاختبار. وحدّث جدول
   **"الحالة"** في `docs/wow64/README.md` (اعمله في أول مرحلة) بعد كل مرحلة.
6. **حل المشاكل من جذورها.** لو لقيت باق في الكيرنل أو أي مكان تاني، صلّحه في مكانه، وافحص الصنف كله مش أول حالة بس.
7. **قانوني:** مسموح تنقل كود من **Wine** (رخصة LGPL، متوافقة مع ReactOS)، خصوصًا `dlls/wow64`، و`dlls/wow64cpu`،
   و`dlls/wow64win`. اكتب مصدر أي كود منقول في أول الملف. **ممنوع** أي كود أو ملفات من Microsoft.
8. ما ترفع أي ملفات خاصة بالمستخدم (رخصة SPSS، ملفات بيانات، صور فيها محتواها)، ولا ملفات إعدادات الاختبار
   (`user.js`، `policies.json`، شهادات الـ proxy).

## نقاط البداية في الكود (اتأكد منها، وصحّحها في الخطة لو لقيتها مختلفة)

- **رفض الـ 32-bit:** `dll/win32/kernel32/client/proc.c` حوالي السطر 3526: `ImageInformation.Machine` بيتقارن بـ
  `SharedUserData->ImageNumberLow/High`، وبعدها `ERROR_EXE_MACHINE_TYPE_MISMATCH`. وفي نفس الملف `PebAddressWow64 = 0`
  مع تعليق `TODO: WOW64 is not supported yet`.
- **الكيرنل فيه بداية:** `ntoskrnl/ke/amd64/kiinit.c` (سطر 228) بيضبط MSR الـ `STAR` بمقطع `KGDT64_R3_CMCODE`، و
  `context.c` و`except.c` بيعرفوا الـ CS بتاع وضع 32-bit (compatibility mode)، و`Wow64Process` موجود في
  `ntoskrnl/ps/query.c` و`ntoskrnl/mm/ARM3/procsup.c`.
- **إنشاء العملية والـ PEB/TEB:** `ntoskrnl/ps/process.c` (`PspCreateProcess`)، و`ntoskrnl/mm/ARM3/procsup.c`
  (`MmCreatePeb` و`MmCreateTeb`).
- **قراية ملفات PE32:** `ntoskrnl/mm/section.c` (فيه `IMAGE_NT_OPTIONAL_HDR32_MAGIC`).
- **الـ loader:** `dll/ntdll/ldr/ldrinit.c` (فيه `Wow64ExecOptionsKey`).

## المراحل (بالترتيب، وكل مرحلة ليها اختبار قبول لازم يعدّي قبل اللي بعدها)

**0. الخطة والبيئة:** اكتب `docs/wow64/README.md`: التصميم، والمراحل دي، وجدول الحالة. وجهّز بيئة البناء والاختبار
(تحت).

**1. الكيرنل: عملية 32-bit حقيقية.**
- `section.c` و`ps`: تحميل صورة PE32 في عملية 64-bit مع علامة WoW64 على العملية (`Wow64Process`).
- عمل `PEB32` و`TEB32` جنب الـ PEB والـ TEB الـ 64-bit، وstack بنسخة 32-bit، وتحميل `ntdll` الـ 64-bit و`ntdll` الـ
  32-bit (من `SysWOW64`).
- الانتقال لوضع 32-bit (CS = `KGDT64_R3_CMCODE`)، والـ context بين 32 و64 bit
  (`NtQueryInformationThread`/`ThreadWow64Context`)، والـ exceptions اللي بتحصل في كود 32-bit.
- `kernel32/proc.c`: يقبل `IMAGE_FILE_MACHINE_I386` ويملا `PebAddressWow64`، و`IsWow64Process` يرجع القيمة الصح.
- **اختبار القبول:** برنامج 32-bit صغير بيرجع exit code معيّن، من غير ما يستعمل أي مكتبة غير `ntdll`، يشتغل ويرجع الكود.

**2. `wow64.dll` و`wow64cpu.dll`: نداءات `ntdll`.**
- انقل التصميم من Wine: `wow64cpu` بيعمل الانتقال 32↔64، و`wow64` بيحوّل كل نداء نظام من `ntdll` الـ 32-bit (المؤشرات،
  والأحجام، و`UNICODE_STRING`، و`OBJECT_ATTRIBUTES`، و`IO_STATUS_BLOCK`...) وينادي النداء الـ 64-bit.
- أرقام نداءات النظام في ReactOS مختلفة عن ويندوز: ولّد جداول التحويل من ملفات الـ syscall في ReactOS نفسه
  (`ntoskrnl/sysfuncs.lst` لـ `ntdll`، و`win32ss/w32ksvc32.h` و`w32ksvc64.h` لـ win32k)، مش من أرقام ويندوز.
- **اختبار القبول:** برامج 32-bit في سطر الأوامر (`hello32.exe` بـ `printf`، وبرنامج بيقرا ويكتب ملفات، وبرنامج بيعمل
  threads) تشتغل في `cmd` وتطلع النتيجة الصح.

**3. `wow64win.dll`: نداءات الواجهة (win32k).**
- تحويل نداءات `NtUser*` و`NtGdi*` (حوالي 1000). ده **أكبر جزء**: ابدأ بالمستعملة فعلًا (اعمل أداة بتطبع النداءات
  اللي برامج التجربة بتعملها)، والباقي بعدين.
- الـ callbacks من الكيرنل للـ user mode (window procedures) لازم تتحول لـ 32-bit كمان.
- **اختبار القبول:** برنامج 32-bit فيه `MessageBox` ونافذة عادية، وNotepad بنسخة 32-bit من ReactOS i386، يفتحوا
  ويترسموا ويستجيبوا للكيبورد والماوس.

**4. مكتبات 32-bit والتحويل.**
- بناء نسخة **i386** من ReactOS (مجلد بناء تاني بـ `-DARCH=i386`) ونسخ مكتباتها لـ `C:\ReactOS\SysWOW64` في الـ ISO
  (عدّل `boot/bootdata` وملفات `add_cd_file`). وحدّث `.github/workflows/build.yml` عشان CI يبني الاتنين.
- تحويل مسارات الملفات (`System32` ← `SysWOW64` لبرامج 32-bit، و`Program Files (x86)`) والريجستري (`Wow6432Node`).
- **اختبار القبول:** `Firefox Setup 160.0.1.exe` يفتح بالضغط عليه، ويثبّت Firefox، ويعمل أيقونة في سطح المكتب وقائمة
  Start، وFirefox يفتح منها ويحمّل `google.com`. وبعدها `vcredist_x64.exe` يثبّت مكتبات VC++ 2008 في `WinSxS`.

**5. التوسع:** InstallShield (SPSS 25 لو الملف موجود)، و`ISSELFREG.DLL` والـ custom actions الـ 32-bit جوه `msiexec`
(كان بيتخطاها)، وبرامج 32-bit قديمة. سجّل كل برنامج اتجرب ونتيجته في جدول في README.

## بيئة البناء (مرة واحدة)

- ثبّت: mingw-w64 (نسخة **posix**: `x86_64-w64-mingw32-gcc-posix`، و`i686-w64-mingw32-gcc-posix` للمرحلة 4)، و
  cmake، وninja، وbison، وflex، وlibgmp-dev، وqemu-system-x86، وmtools، وpython3-pil، وp7zip-full.
- ابني برا المستودع (مثلًا `../build`):
  `cmake -G Ninja -DARCH=amd64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=<repo>/toolchain-gcc.cmake -DENABLE_CCACHE=0 -DDLL_EXPORT_VERSION=0x600 <repo>`
  وبعدها `ninja bootcd`. اقرا قسم "بيئة البناء" في README، فيه مشاكل البناء المعروفة وحلولها.

## طريقة الشغل (حلقة مغلقة، كل حاجة بتتقاس)

1. اعمل قرص `k.raw` (16GB مثلًا) وثبّت Kush عليه من `bootcd.iso` (خيار "Setup" في قائمة الإقلاع). سوق برنامج التثبيت
   بالكيبورد عن طريق `tools/qemu/vm.py`، **وصوّر الشاشة بعد كل خطوة**.
2. شغّل النظام بـ `tools/qemu/run_vm.sh <workdir> k.raw` (حوالي 4 دقائق تحت TCG، فاستعمل انتظار في الخلفية بدل `sleep`
   طويل). اقرا `tools/qemu/README.md`.
3. `python3 tools/qemu/vm.py <workdir> shot:name` بيطلع صورة: **افتحها وشوفها بنفسك** بعد كل تعديل. `click.py` للضغط،
   و`typ.py` للكتابة.
4. عدّل بسرعة: ابني الملف المتغيّر بس (مثلًا `ninja ntoskrnl` أو `ninja wow64`)، وانسخه جوه القرص بـ `mcopy` والنظام
   **مقفول** (الأمر في `tools/qemu/README.md`)، وأعد الإقلاع.
5. لو حاجة قفلت: اقرا `serial.log`. سطر `KUSH-EXC` بيقول الـ exception والموديول والـ RVA، وبيطبع **stack تقريبي**
   (`KUSH-EXC: stack ...`). لو حصلت شاشة زرقاء: `gdbserver` من QEMU monitor، وحدد الموديول بالبحث عن `MZ` لورا من عنوان
   الكراش، وبعدها `nm` على الملف من البناء لاسم الدالة. **ما تصدّق إن حاجة اشتغلت لحد ما الصورة أو السجل يأكدوا.**
6. **ما تكسر الموجود:** بعد كل مرحلة اتأكد إن explorer وcmd وNotepad وControl Panel وFirefox الـ 64-bit لسه شغالين.

## حاجات معروفة عن البيئة (عشان ما تضيّع وقت)

- `run_vm.sh` بيشغّل QEMU بـ `-cpu max,-avx2`: محاكي QEMU (TCG 8.2) بيعمل fault غلط في تعليمة `vpmaskmovd`، فبرامج
  MSVC الحديثة (زي Firefox) بتقفل عشوائيًا. ده عطل في المحاكي، مش في Kush.
- الكيرنل بيشتغل UP (`ntoskrnl.exe`). أكتر من 4GB رام بتعمل `0x7B`، فخلي الذاكرة 3072MB.
- لو بيئتك فيها proxy بيعيد إصدار شهادات TLS، المتصفح حيرفضها. ده من البيئة، ما تعدّل النظام عشانه.
- `tools/ipc_pipe_test/pipetest.c` اختبار جاهز للـ named pipes بين عمليتين، ممكن تبني منه اختبارات للـ WoW64.

## لما تخلص كل مرحلة

- commit و push لـ `main`.
- README: المرحلة، والملفات اللي اتعدلت، والمشاكل اللي ظهرت وأسبابها الجذرية وحلولها، والصور، وحدود الاختبار.
- حدّث جدول الحالة في `docs/wow64/README.md`.
- كمّل المرحلة اللي بعدها من غير ما تنتظر رد مني.

---
