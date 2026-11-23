# Prompt لتنفيذ WoW64 في Kush 1.1 (تشغيل برامج 32-bit) انطلاقًا من فرع TheNNX

انسخ النص ده كله وابدأ بيه session جديد لوكيل برمجة (Claude Code أو Muse Code أو غيره) على المستودع:
**https://github.com/eltaieb-eltalib/last-kushos**

---

إنت شغال على **Kush 1.1**، نظام تشغيل مبني على ReactOS (amd64). المستودع
**https://github.com/eltaieb-eltalib/last-kushos** فيه **النظام كامل** (كل الكود وكل تعديلات Kush)، فبتبني منه
مباشرة.

**المهمة:** تضيف لـ Kush طبقة **WoW64**، عشان يشغّل برامج **32-bit (i386)** على النظام الـ 64-bit زي ويندوز بالظبط.
**ما بتبدأ من الصفر:** بتاخد شغل WoW64 الموجود في فرع **`wow64sync`** بتاع **Marcin Jabłoński (TheNNX)**:
**https://github.com/TheNNX/reactos/tree/wow64sync**، وتدمجه في Kush وتكمّله لحد ما يشتغل.

الهدف العملي: ملفات التثبيت دي تفتح **بالضغط عليها مباشرة** وتكمّل التثبيت وتعمل الأيقونات لوحدها:
- `Firefox Setup 160.0.1.exe` (غلاف NSIS بنسخة 32-bit)، من
  `https://ftp.mozilla.org/pub/firefox/releases/160.0.1/win64/en-US/`
- `vcredist_x64.exe` (Visual C++ 2008 SP1 x64، غلافه 32-bit)
- `SPSS_Statistics_25_win64.exe` (InstallShield، غلافه 32-bit)، **لو** المستخدم وفّر الملف. ما تنزّل برامج
  مدفوعة أو مقرصنة.

حاليًا أي برنامج 32-bit في Kush بيطلّع **"Machine Type Mismatch"**، والتفاصيل في README (أقسام Firefox وSPSS).

## اللي اتعرف عن فرع TheNNX (اتفحص في 23 نوفمبر 2026، اتأكد منه تاني)

- آخر commit وقت الفحص: `b03f7c52f29ba6e510a9f1b09d7b48be2967b994` (11 أكتوبر 2026، "Start working on WOW64CPU").
  الفرع لسه شغال عليه، فخد **آخر** commit فيه وسجّل رقمه بالظبط في `docs/wow64/README.md`.
- الفرع مبني فوق ReactOS الرسمي عند commit `e6d2efe9` (4 أغسطس 2026)، وفوقه حوالي 130 commit من TheNNX:
  **311 ملف، +26,900 سطر، −2,050 سطر.** أهمها:
  - `dll/wow64` (حوالي 11 ألف سطر، جزء منه من Wine في `dll/wow64/wine`)، و`dll/wow64win` (حوالي 10.5 ألف سطر)،
    و`sdk/lib/rtl/wow64.c`، و`sdk/include/reactos/wow64.h`، و`sdk/include/reactos/drivers/wow64`.
  - تعديلات في `win32ss` (58 ملف)، و`dll/win32/kernel32` (21 ملف)، و`dll/ntdll`، و`subsystems/csr`، و`ntoskrnl`،
    وشوية drivers (AFD والشبكة) وملفات CMake.
  - **بناء i386 تابع** بيطلع مع بناء الـ amd64 ويملا `SysWOW64` (`sdk/cmake/wow64.cmake`، وخيارات `-wow64` و
    `-nowow64` في `spec2def`).
- **الحالة:** تجريبي، وما اندمج في ReactOS الرسمي. سجله بيذكر نداءات النظام، والواجهة، والشبكة، والـ exceptions،
  وTask Manager. ما في تقرير منشور يأكد إن برامج 32-bit بتشتغل عليه بثبات، فاعتبر كل حاجة غير مجرّبة لحد ما تجربها.
- **فرع مرفوض:** `https://github.com/mateo-cogeanu/ReactOS` فيه نفس الشغل مدموج مع UEFI وAHCI، لكن README بتاعه بيقول
  إن تشغيل برامج 32-bit "متوقف بشكل ثابت في تهيئة أول عملية" بسبب `__debugbreak()` في `Wow64LdrpInitialize`، والإصلاح
  ده موجود عند TheNNX (commit 10 أكتوبر). **ما تاخد منه.** Kush عنده UEFI وAHCI بتاعته.

## قواعد ثابتة

1. **رد دايمًا بالعربي.**
2. **ما تسألني أي سؤال.** القرارات اللي ليها خيار معقول، خدها واكتبها في README. اسأل بس لو في حاجة مستحيلة من غيري.
3. **كل الشغل على مستوى النظام** (الكيرنل، `ntdll`، `wow64*.dll`، `kernel32`، المكتبات)، مش حل لبرنامج واحد.
   **ممنوع** تفك ملفات التثبيت أو تعمل استثناء لاسم برنامج معيّن. البرنامج الـ 32-bit لازم يشتغل زي ما هو.
4. **اشتغل على `main` مباشرة.** كل خطوة صغيرة شغالة: commit و push لـ `main` على طول، وما تجمّع شغل كتير من غير رفع.
5. **وثّق كل حاجة في `README.md` بالعربي:** المشكلة، والسبب الجذري، والحل، والصور، وحدود الاختبار. وحدّث جدول
   **"الحالة"** في `docs/wow64/README.md` (اعمله في أول مرحلة) بعد كل مرحلة.
6. **حل المشاكل من جذورها.** لو لقيت باق في الكيرنل أو في كود TheNNX أو أي مكان تاني، صلّحه في مكانه، وافحص الصنف كله
   مش أول حالة بس.
7. **حافظ على حقوق المؤلفين:** كود TheNNX وWine لازم يدخل باسم مؤلفيه الأصليين (commits بنفس الـ author، أو أقل حاجة
   `Co-Authored-By` لكل commit مدموج)، والـ headers والرخص (GPL/LGPL) تفضل زي ما هي. سجّل في README إن أساس WoW64 من
   TheNNX مع الرابط. **ممنوع** أي كود أو ملفات من Microsoft.
8. **ما تفتح pull request ولا issue ولا تكتب أي حاجة في مستودع TheNNX أو أي مستودع تاني.** لو لقيت إصلاحات مفيدة ليهم،
   سجّلها في `docs/wow64/upstream-fixes.md` بس، والمستخدم يقرر.
9. **مفتاح إيقاف:** WoW64 لازم يتقفل بقيمة في الريجستري (مثلًا `HKLM\SYSTEM\CurrentControlSet\Control\Kush\Wow64`
   `Enabled=0`). لما يكون مقفول، النظام يرجع يرفض برامج 32-bit زي دلوقتي بالظبط وما يحمّل أي حاجة من WoW64، عشان لو
   ظهرت مشكلة المستخدم يقفله من غير ما يعيد التثبيت. جرّب المفتاح في كل مرحلة.
10. ما ترفع أي ملفات خاصة بالمستخدم (رخصة SPSS، ملفات بيانات، صور فيها محتواها)، ولا ملفات إعدادات الاختبار
    (`user.js`، `policies.json`، شهادات الـ proxy).

## نقاط البداية في Kush (اتأكد منها، وصحّحها في الخطة لو لقيتها مختلفة)

- **رفض الـ 32-bit:** `dll/win32/kernel32/client/proc.c` حوالي السطر 3526: `ImageInformation.Machine` بيتقارن بـ
  `SharedUserData->ImageNumberLow/High`، وبعدها `ERROR_EXE_MACHINE_TYPE_MISMATCH`. وفي نفس الملف `PebAddressWow64 = 0`
  مع تعليق `TODO: WOW64 is not supported yet`.
- **الكيرنل فيه بداية:** `ntoskrnl/ke/amd64/kiinit.c` (سطر 228) بيضبط MSR الـ `STAR` بمقطع `KGDT64_R3_CMCODE`، و
  `context.c` و`except.c` بيعرفوا الـ CS بتاع وضع 32-bit، و`Wow64Process` موجود في `ntoskrnl/ps/query.c` و
  `ntoskrnl/mm/ARM3/procsup.c` (`MmCreatePeb` و`MmCreateTeb`).
- **تعديلات Kush الكبيرة اللي حتتعارض مع الدمج:** `win32ss` (إصلاحات طابور الرسائل في `msgqueue.c` و`message.c`، و
  تصميم Fluent)، و`dll/win32/kernel32` (دوال Windows 8+ في `dll/win32/kernel32/kush/kush_win8.c` وغيرها)، و`ntoskrnl` (تشخيص `KUSH-EXC`،
  و`SystemBootEnvironmentInformation`، وإصلاحات Mm)، و`dll/ntdll` (الـ API sets). اقرا README عشان تعرف سبب كل تعديل.

## المراحل (بالترتيب، وكل مرحلة ليها اختبار قبول لازم يعدّي قبل اللي بعدها)

**0. الخطة والبيئة:** اكتب `docs/wow64/README.md`: الخطة، ورقم commit TheNNX اللي بتاخده، وجدول الحالة. وجهّز بيئة
البناء والاختبار (تحت)، وابني Kush زي ما هو وجرّبه **قبل** أي دمج، عشان يبقى عندك نقطة مقارنة.

**1. الدمج.**
- جيب الفرع: `git fetch https://github.com/TheNNX/reactos wow64sync` (بعمق كافي يوصل `e6d2efe9`).
- طلّع تعديلات TheNNX بس: الفرق بين `e6d2efe9` وآخر commit في `wow64sync`، كـ commits بمؤلفيها (`git format-patch`
  ثم `git am -3`، أو cherry-pick). **ما تدمج تاريخ ReactOS الرسمي كله.**
- في التعارضات: **تعديلات Kush بتكسب**، إلا لو WoW64 محتاج التغيير فعلًا. وقتها ادمج الاتنين، واكتب كل قرار تعارض مهم
  في `docs/wow64/README.md`.
- شغّل البناء الـ i386 التابع لـ `SysWOW64`، وحدّث `.github/workflows/build.yml` عشان CI يبنيه كمان (ممكن الـ ISO يكبر
  حوالي 150 لـ 250MB، اكتب الحجم).
- أضف مفتاح الإيقاف (القاعدة 9).
- **اختبار القبول:** الـ ISO بيتبني محليًا وفي CI، وKush بيقلع ويثبّت زي قبل الدمج (BIOS وUEFI)، و`SysWOW64` فيه مكتبات
  PE32 (اتأكد بـ `file`)، وexplorer وcmd وNotepad وControl Panel وFirefox الـ 64-bit شغالين، ومن غير شاشة زرقاء.

**2. أول عملية 32-bit.**
- الهدف: `C:\ReactOS\SysWOW64\cmd.exe` يفتح ويشتغل. ده بالظبط المكان اللي فرع mateo وقف فيه: تتبّع تهيئة أول عملية
  (`Wow64LdrpInitialize`، و`Wow64InitProcess`، وwow64cpu) بالسجل وgdb لحد ما تعدّي.
- اعمل برامج اختبار 32-bit صغيرة في `tools/wow64_tests/` (تتبني بـ `i686-w64-mingw32-gcc`): exit code، و`printf`،
  وقراية وكتابة ملفات، وthreads، وexceptions (`__try` وSEH)، و`IsWow64Process`، وتحويل مسارات `System32`.
- **اختبار القبول:** `cmd.exe` الـ 32-bit بيطبع نسخته وبينفّذ `dir` و`echo`، وكل برامج الاختبار بتطلع النتيجة الصح،
  ومفتاح الإيقاف بيرجّع "Machine Type Mismatch".

**3. برامج 32-bit فيها نوافذ.**
- `wow64win.dll` (نداءات `NtUser*` و`NtGdi*` والـ callbacks): كمّل الناقص اللي برامج التجربة بتحتاجه. اعمل أداة
  بتطبع أي نداء ما ليه تحويل، عشان تعرف الناقص من القياس مش بالتخمين.
- **اختبار القبول:** برنامج 32-bit فيه `MessageBox` ونافذة عادية، و`notepad.exe` و`taskmgr.exe` من `SysWOW64`، يفتحوا
  ويترسموا صح ويستجيبوا للكيبورد والماوس، والعربي يظهر صح.

**4. برامج التثبيت.**
- تحويل مسارات الملفات (`Program Files (x86)`) والريجستري (`Wow6432Node`) لو ناقصين في فرع TheNNX.
- **اختبار القبول:** `Firefox Setup 160.0.1.exe` يفتح بالضغط عليه، ويثبّت Firefox، ويعمل أيقونة في سطح المكتب وقائمة
  Start، وFirefox يفتح منها ويحمّل `google.com`. وبعدها `vcredist_x64.exe` يثبّت مكتبات VC++ 2008 في `WinSxS`.

**5. التوسع:** InstallShield (SPSS 25 لو الملف موجود)، و`ISSELFREG.DLL` والـ custom actions الـ 32-bit جوه `msiexec`
(كان بيتخطاها)، وبرامج 32-bit قديمة. سجّل كل برنامج اتجرب ونتيجته في جدول في README.

## بيئة البناء (مرة واحدة)

- ثبّت: mingw-w64 (نسخة **posix**: `x86_64-w64-mingw32-gcc-posix` و`i686-w64-mingw32-gcc-posix`)، وcmake، وninja،
  وbison، وflex، وlibgmp-dev، وqemu-system-x86، وmtools، وpython3-pil، وp7zip-full.
- ابني برا المستودع (مثلًا `../build`):
  `cmake -G Ninja -DARCH=amd64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=<repo>/toolchain-gcc.cmake -DENABLE_CCACHE=0 -DDLL_EXPORT_VERSION=0x600 <repo>`
  وبعدها `ninja bootcd`. بعد الدمج، اقرا طريقة البناء التابع في `sdk/cmake/wow64.cmake` وREADME فرع TheNNX. واقرا قسم
  "بيئة البناء" في README بتاع Kush، فيه مشاكل البناء المعروفة وحلولها.

## طريقة الشغل (حلقة مغلقة، كل حاجة بتتقاس)

1. اعمل قرص `k.raw` (16GB مثلًا) وثبّت Kush عليه من `bootcd.iso` (خيار "Setup" في قائمة الإقلاع). سوق برنامج التثبيت
   بالكيبورد عن طريق `tools/qemu/vm.py`، **وصوّر الشاشة بعد كل خطوة**.
2. شغّل النظام بـ `tools/qemu/run_vm.sh <workdir> k.raw` (حوالي 4 دقائق تحت TCG، فاستعمل انتظار في الخلفية بدل `sleep`
   طويل). اقرا `tools/qemu/README.md`.
3. `python3 tools/qemu/vm.py <workdir> shot:name` بيطلع صورة: **افتحها وشوفها بنفسك** بعد كل تعديل. `click.py` للضغط،
   و`typ.py` للكتابة.
4. عدّل بسرعة: ابني الملف المتغيّر بس (مثلًا `ninja wow64` أو `ninja ntoskrnl`)، وانسخه جوه القرص بـ `mcopy` والنظام
   **مقفول** (الأمر في `tools/qemu/README.md`)، وأعد الإقلاع.
5. لو حاجة قفلت: اقرا `serial.log`. سطر `KUSH-EXC` بيقول الـ exception والموديول والـ RVA، وبيطبع **stack تقريبي**
   (`KUSH-EXC: stack ...`). لو حصلت شاشة زرقاء: `gdbserver` من QEMU monitor، وحدد الموديول بالبحث عن `MZ` لورا من عنوان
   الكراش، وبعدها `nm` على الملف من البناء لاسم الدالة. **ما تصدّق إن حاجة اشتغلت لحد ما الصورة أو السجل يأكدوا.**
6. **ما تكسر الموجود:** بعد كل مرحلة اتأكد إن explorer وcmd وNotepad وControl Panel وFirefox الـ 64-bit لسه شغالين، وإن
   مفتاح الإيقاف شغال.

## حاجات معروفة عن البيئة (عشان ما تضيّع وقت)

- `run_vm.sh` بيشغّل QEMU بـ `-cpu max,-avx2`: محاكي QEMU (TCG 8.2) بيعمل fault غلط في تعليمة `vpmaskmovd`، فبرامج
  MSVC الحديثة (زي Firefox) بتقفل عشوائيًا. ده عطل في المحاكي، مش في Kush.
- الكيرنل بيشتغل UP (`ntoskrnl.exe`). أكتر من 4GB رام بتعمل `0x7B`، فخلي الذاكرة 3072MB.
- لو بيئتك فيها proxy بيعيد إصدار شهادات TLS، المتصفح حيرفضها. ده من البيئة، ما تعدّل النظام عشانه.
- `tools/ipc_pipe_test/pipetest.c` اختبار جاهز للـ named pipes بين عمليتين، ممكن تبني منه نسخة 32-bit لاختبار WoW64.

## لما تخلص كل مرحلة

- commit و push لـ `main`.
- README: المرحلة، والملفات اللي اتعدلت، والمشاكل اللي ظهرت وأسبابها الجذرية وحلولها، والصور، وحدود الاختبار.
- حدّث جدول الحالة في `docs/wow64/README.md`.
- كمّل المرحلة اللي بعدها من غير ما تنتظر رد مني.

---
