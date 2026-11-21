# خطة تصميم Kush Fluent

الهدف: نخلي شكل Kush حديث على نفس **مبادئ** Fluent Design بتاعة Windows 11: Mica، Acrylic، زوايا دائرية،
أيقونات Fluent، وخطوط حديثة. **كل التعديلات على مستوى النظام** (win32k، uxtheme، explorer، dwmapi)، فأي برنامج
بيستفيد منها تلقائيًا.

> **ملاحظة قانونية:** بنعمل تصميم **أصلي** على نفس المبادئ، ما بننسخ Windows 11. ما بنستعمل خط Segoe UI ولا خلفيات
> وشعارات Microsoft، لأنها ما مسموح توزيعها. بنستعمل بس مصادر مفتوحة مسموح توزيعها:
> - **الأيقونات:** Fluent UI System Icons، رخصة MIT (`github.com/microsoft/fluentui-system-icons`).
> - **الخطوط:** Selawik، رخصة OFL (خط Microsoft مفتوح المصدر، بنفس مقاسات Segoe UI)، و Noto Sans Arabic، رخصة OFL، للعربي.

---

## الوضع الحالي في Kush (نقطة البداية)

| الجزء | الموجود | الملفات |
|---|---|---|
| الثيمات | `uxtheme` بيدعم `.msstyles`، وفي ثيمات Lautus و Lunar و Mizu و Modern و Blackshade | `dll/win32/uxtheme/`، `media/themes/` |
| رسم إطار النافذة | `uxtheme` بيرسم شريط العنوان والإطار (themehooks) | `dll/win32/uxtheme/nonclient.c`، `themehooks.c` |
| الشفافية | **layered windows** موجودة (`UpdateLayeredWindow`)، لكن ما في compositor زي DWM | `win32ss/user/ntuser/layered.c` |
| DWM API | `dwmapi` كله stubs تقريبًا | `dll/win32/dwmapi/dwmapi_main.c` |
| الواجهة | explorer: شريط المهام، Start، الساعة، الـ tray | `base/shell/explorer/` |
| الخطوط | FreeSans و Source Sans Pro و Arial-compatible، ورسم بـ FreeType | `media/fonts/`، `win32ss/gdi/ntgdi/freetype.c` |

**أهم استنتاج:** Kush ما فيه compositor، والحاجات اللي محتاجة compositor (زي blur حقيقي للنوافذ اللي ورا) هي الأصعب.
عشان كده الخطة مبنية بحيث كل مرحلة تشتغل **من غير compositor**، والـ compositor جاي في الآخر.

---

## المرحلة 0: الأساس (Design Tokens)

جدول قيم موحد يقرأ منه كل النظام بدل ما الألوان والمقاسات تكون مكتوبة في كل مكان.

- **الألوان:** فاتح وغامق، ولون مميز (accent) واحد بيتولّد منه 6 درجات.
- **الزوايا:** النوافذ 8px، والقوائم والـ flyouts 8px، والأزرار وحقول الكتابة 4px.
- **المسافات:** على شبكة 4px (4، 8، 12، 16، 24).
- **مكانها:** `HKCU\Software\Kush\Theme` (`AppsUseLightTheme`، `AccentColor`، `CornerRadius`).
  `uxtheme` بيقرأها ويديها لـ explorer وللبرامج.
- **التحقق:** بنغيّر لون الـ accent من الـ registry، ولازم يتغير في شريط العنوان وStart وشريط المهام من غير إعادة بناء.

## المرحلة 1: الخطوط (Typography)

- إضافة Selawik (عادي و Semibold و Bold) و Noto Sans Arabic لـ `media/fonts/`.
- **FontSubstitutes:** `MS Shell Dlg 2` و `Segoe UI` يشاوروا على Selawik، وللعربي fallback على Noto Sans Arabic.
- خط النظام في `NONCLIENTMETRICS` (القوائم وشريط العنوان والرسائل): Selawik 9pt.
- **أحجام ثابتة** زي سلم Fluent: Caption 12، Body 14، Subtitle 20، Title 28 (بالبكسل عند 96 DPI).
- **التنعيم:** تفعيل grayscale anti-aliasing في FreeType (`freetype.c`) كإعداد افتراضي.
- **التحقق:** صور QEMU لنافذة فيها عربي وإنجليزي مع بعض، والنص لازم يكون من اليمين لليسار وما فيه مربعات.

## المرحلة 2: ثيم Kush Fluent (`.msstyles`)

- ثيم جديد `media/themes/KushFluent/` بنسختين فاتح وغامق، مبني بنفس طريقة الثيمات الموجودة.
- **الأدوات (controls):** أزرار وcheckbox وradio وحقول كتابة وقوائم منسدلة بزوايا 4px. الزوايا الدائرية هنا **مجانية**،
  لأن msstyles بيستعمل صور 9-grid بهوامش ثابتة، والزاوية جوه الصورة.
- **Scrollbars:** رفيعة (زي Fluent)، وبتتعرض أكتر لما الماوس يقرب.
- **شريط العنوان:** من غير خطوط وتدرجات، وأزرار التصغير/التكبير/القفل مسطحة، ولون القفل أحمر عند المرور.
- الصور بتتولد بسكربت (`tools/theme/gen_fluent_parts.py`) من الـ tokens، فتغيير الـ radius أو اللون بيعيد توليد الثيم كله.
- **التحقق:** نفتح Control Panel و Notepad و Firefox و SPSS بالثيم الجديد ونقارن الصور.

## المرحلة 3: الزوايا الدائرية للنوافذ (على مستوى النظام)

- **في win32k:** أي نافذة top-level عندها إطار، وما مكبّرة أو ملصوقة على حافة الشاشة، بتاخد region بزوايا 8px تلقائيًا
  (نفس آلية `SetWindowRgn` لكن من النظام). المكبّرة بترجع مربعة زي Windows 11.
- **التنعيم:** حافة الـ region خشنة (pixel واحد). عشان كده `uxtheme/nonclient.c` بيرسم خط إطار 1px بلون نص شفاف
  على الزاوية، فبتبان ناعمة.
- **القوائم وtooltips والـ popups:** زوايا 8px بنفس الطريقة، في `win32ss/user/ntuser/menu.c`.
- **استثناءات:** البرامج اللي عندها region خاصة بيها (`SetWindowRgn` من البرنامج) ما بنلمسها. ومفتاح registry لإيقاف الميزة.
- **API:** `DwmSetWindowAttribute(DWMWA_WINDOW_CORNER_PREFERENCE)` في `dwmapi` بيشتغل حقيقي، فأي برنامج يقدر
  يطلب زوايا مربعة أو صغيرة.
- **التحقق:** صور بعد التكبير والتصغير والسحب، وقياس إن الرسم ما بقى أبطأ بشكل ملحوظ.

## المرحلة 4: Mica (من غير compositor)

Mica في Windows 11 هو **لون الخلفية (wallpaper) بعد blur قوي وتقليل تشبّع**، وبيبان تحت النافذة حسب مكانها على الشاشة.
ما بيعرض النوافذ اللي ورا، فبنقدر نعمله من غير compositor:

1. لما الخلفية تتغير (explorer)، بنحسب **مرة واحدة** صورة: الخلفية بعد blur كبير (box blur × 3) + tint بلون الثيم +
   تقليل التشبع. الصورة بتتخزن في section مشترك بين كل البرامج.
2. النوافذ اللي بتطلب Mica (`DwmSetWindowAttribute(DWMWA_SYSTEMBACKDROP_TYPE, DWMSBT_MAINWINDOW)`)، ونوافذ النظام
   (explorer و Control Panel) كإعداد افتراضي: `uxtheme` بيرسم خلفية شريط العنوان والـ client من الجزء المقابل لمكانها على الشاشة.
3. لما النافذة تتحرك، بيتعاد رسم شريط العنوان (`WM_MOVE` → NC repaint)، والجزء من الصورة بيتغير معاها.
4. النافذة غير النشطة بترجع للون ثابت من غير Mica، زي Windows 11.

- **التحقق:** نسحب نافذة فوق خلفية ملونة، ولازم لون شريط العنوان يتغير مع المكان.

## المرحلة 5: Acrylic (على مرحلتين)

Acrylic محتاج blur **حي** للمحتوى اللي ورا النافذة، وده محتاج compositor. عشان كده:

- **5أ: Acrylic ثابت (من غير compositor).** شريط المهام وStart والـ flyouts بتبقى فوق سطح المكتب غالبًا، فبنعمل الـ
  Acrylic بتاعها من صورة الخلفية بعد blur (نفس صورة المرحلة 4، لكن blur أخف) + **texture ضجيج** خفيف (noise 2%) + tint
  شبه شفاف. الشكل بيطلع مطابق في الاستعمال العادي.
- **5ب: compositor حقيقي (مرحلة كبيرة).**
  - كل نافذة top-level بترسم في سطح (surface) منفصل، زي ما `layered.c` بيعمل للـ layered windows حاليًا، لكن لكل النوافذ،
    ومن وراه مفتاح تفعيل.
  - thread في win32k بيركّب الأسطح على الشاشة. تحت النوافذ اللي عندها Acrylic بيعمل blur للمحتوى اللي وراها.
  - `DwmEnableBlurBehindWindow` و `DwmExtendFrameIntoClientArea` في `dwmapi` بقوا حقيقيين، فبرامج كتير بتستفيد.
  - **شرط البداية:** قياس الأداء في QEMU، وblur بالـ SSE2 (box blur منفصل أفقي ورأسي) عشان ما يبطّء الجهاز.

## المرحلة 6: أيقونات Fluent

- سكربت `tools/icons/build_fluent_icons.py` بيحوّل SVG من Fluent UI System Icons (MIT) لملفات `.ico` بكل المقاسات
  (16 و 20 و 24 و 32 و 48 و 256).
- استبدال أيقونات `shell32` و `explorer` و Control Panel، مع **الحفاظ على نفس أرقام الأيقونات (indices)** عشان البرامج
  اللي بتطلب أيقونة بالرقم ما تتكسر.
- أيقونات نسختين فاتح وغامق حسب الثيم.
- **التحقق:** صورة My Computer و Start وشريط المهام، والتأكد إن أيقونات برامج زي SPSS وFirefox ما اتأثرت.

## المرحلة 7: الواجهة (Shell)

- **شريط المهام:** خيار توسيط الأيقونات، ارتفاع 48px، ومؤشر تحت البرنامج النشط بلون الـ accent
  (`taskband.cpp`، `taskswnd.cpp`، `traywnd`).
- **Start:** تصميم جديد: مربع بحث فوق، شبكة برامج مثبتة، "كل البرامج"، وزر الطاقة تحت (`startmnu.cpp`، `startmnusite.cpp`).
- **Flyouts:** الساعة والصوت والشبكة بزوايا 8px و Acrylic ثابت.
- **التحقق:** صور + استعمال بالكيبورد بس (Win، الأسهم، Enter) عشان سهولة الوصول.

## المرحلة 8: الوضع الغامق على مستوى النظام

- Windows 10/11 فيه APIs في `uxtheme` بالأرقام (ordinals 132 لحد 135: `ShouldAppsUseDarkMode`، `AllowDarkModeForWindow`،
  `SetPreferredAppMode`، `FlushMenuThemes`)، وبرامج زي Firefox و VS Code و Notepad++ بتستعملها.
- تنفيذها في `uxtheme` حسب `AppsUseLightTheme`، مع `DWMWA_USE_IMMERSIVE_DARK_MODE` في `dwmapi` عشان شريط العنوان يبقى غامق.
- **التحقق:** نغيّر الوضع، ولازم Firefox و explorer يتغيروا من غير ما يتقفلوا.

## المرحلة 9: الحركة (Animations)

- **من غير compositor:** fade للقوائم (`AnimateWindow` موجود) وحركة خفيفة لـ Start.
- **مع compositor (بعد 5ب):** فتح وقفل النوافذ بـ fade + scale، وتصغير لشريط المهام.

---

## الشكل المطلوب (المرجع البصري)

المستخدم عايز شكل **بنفس روح وجمال** Windows 11، لكن **أصلي، وما نسخة**:

- **سطح المكتب:** خلفية ناعمة (✅ اتعملت: `Kush-Flow.bmp`، زهرة طبقات زرقاء أصلية بتتولد بـ
  `tools/design/gen_wallpaper.py`، فيها نسخة فاتحة وغامقة، وهي الخلفية الافتراضية في `boot/bootdata/hivedef.inf`).
  أيقونات سطح المكتب بمسافات مريحة وتحتها نص بظل خفيف.
- **شريط المهام:** فاتح وشبه شفاف (Acrylic)، ارتفاعه 48px، **الأيقونات في النص**: زر Start (شعار Kush أصلي، 4
  مربعات أو شكل مستوحى من هوية Kush)، بحث، البرامج المفتوحة، ومؤشر صغير تحت البرنامج النشط. الساعة والتاريخ في سطرين
  يمين، ومعاهم أيقونات الشبكة والصوت.
- **Start:** نافذة في **نص الشاشة فوق شريط المهام**، بزوايا 8px وخلفية Acrylic فاتحة وحد رمادي 1px:
  - فوق: مربع بحث بزوايا دائرية ("Type here to search").
  - **Pinned:** عنوان + زر "All apps >" على اليمين (بيفتح قائمة البرامج الكاملة، يعني الـ Start القديم)، وشبكة 6 أعمدة
    × 3 صفوف: أيقونة 32px وتحتها اسم البرنامج.
  - **Recommended:** عمودين × 3 صفوف: أيقونة + اسم الملف + وقت ("2h ago") من الملفات الأخيرة.
  - تحت: شريط فيه صورة المستخدم (دائرة بأول حرف من الاسم) والاسم، وزر الطاقة على اليمين.
  - تأثير hover: خلفية فاتحة بزوايا 4px. الإغلاق بـ Esc أو بالضغط برا النافذة.
- **النوافذ:** زوايا 8px، شريط عنوان مسطح فاتح (Mica)، وأزرار تحكم مسطحة.

## نقاط البداية في الكود (اتحددت بالفحص)

| الحاجة | المكان |
|---|---|
| فتح الـ Start | `base/shell/explorer/traywnd.cpp`: `PopupStartMenu()` (حوالي السطر 1921). بيتنادى من ضغطة زر Start (`OnNcLButtonDown`/`OnLButtonDown` حوالي 2953)، ومن رسالة فتح الـ Start (`OnOpenStartMenu` حوالي 3120)، وزر Windows بيوصل لنفس المكان |
| أوامر الإيقاف/التشغيل/البحث | `HandleCommand` في `traywnd.cpp`: `TRAYCMD_SHUTDOWN_DIALOG`، `TRAYCMD_RUN_DIALOG`، `IDM_SEARCH`. نافذة جديدة بتبعت `WM_COMMAND` للـ tray |
| ترتيب شريط المهام | `traywnd.cpp`: `ResizeWorker` (زر Start والـ rebar والـ tray notify)، والأزرار في `taskswnd.cpp` |
| خلفية شريط المهام | `traywnd.cpp`: `OnEraseBackground` (حوالي 2580) |
| ملفات explorer | `base/shell/explorer/CMakeLists.txt` (أي ملف جديد بيتضاف لـ `SOURCE`، وبيبدأ بـ `#include "precomp.h"`) |
| إطار النوافذ | `dll/win32/uxtheme/nonclient.c`، `themehooks.c` |
| layered windows | `win32ss/user/ntuser/layered.c` |
| DWM API | `dll/win32/dwmapi/dwmapi_main.c` |
| الخطوط | `media/fonts/`، و FontSubstitutes في `boot/bootdata/hivesft.inf`، والرسم في `win32ss/gdi/ntgdi/freetype.c` |
| الثيمات | `media/themes/` (أمثلة: Modern، Lautus) |

**اقتراح تنفيذ الـ Start (المرحلة 7):** ملف جديد `base/shell/explorer/kushstart.cpp` فيه نافذة popup
(`WS_POPUP`، `WS_EX_TOOLWINDOW | WS_EX_TOPMOST`) بزوايا من `CreateRoundRectRgn`، وخلفية Acrylic ثابتة: قبل الإظهار
بنصوّر المنطقة اللي وراها من الشاشة بـ `BitBlt`، ونصغّرها ×8، ونعمل box blur، ونكبّرها، ونخلطها بلون فاتح + noise.
البرامج المثبتة من `CSIDL_COMMON_PROGRAMS` و `CSIDL_PROGRAMS` (ملفات `.lnk`)، والأيقونات بـ `SHGetFileInfo`،
والملفات الأخيرة من `CSIDL_RECENT`. `PopupStartMenu()` بتنادي الـ Start الجديد، إلا لو
`HKCU\Software\Kush\Shell\ClassicStartMenu=1`. زر "All apps" بيفتح القائمة القديمة.

**ملاحظة:** شاشة الـ VM في QEMU 800×600. لازم التصميم يتأقلم مع الشاشات الصغيرة (عدد الصفوف حسب المساحة)،
أو نرفع الدقة لـ 1024×768 أو 1280×800 من إعدادات العرض قبل الصور.

## الحالة

| المرحلة | الحالة |
|---|---|
| الخلفية الأصلية Kush Flow | ✅ اتعملت (`modules/wallpapers/Kush-Flow*.bmp`، `tools/design/gen_wallpaper.py`) |
| 0 لحد 9 | ⏳ ما بدأت. الترتيب المقترح: 7 (Start + شريط المهام) ← 0 ← 1 ← 3 ← 4 ← 5أ ← 6 ← 2 ← 8 ← 5ب ← 9 |

## الترتيب والاعتماديات

```
0 الأساس ─┬─ 1 الخطوط ─┬─ 2 الثيم ─┬─ 3 الزوايا ─┬─ 4 Mica ─┬─ 5أ Acrylic ثابت ─┬─ 7 الواجهة
          │            │           │             │          │                   └─ 9 حركة بسيطة
          └─ 8 الغامق ─┘           └─ 6 الأيقونات            └─ 5ب compositor ─── 9 حركة كاملة
```

- المراحل 0 لحد 4 و 5أ و 6 و 7 و 8 **ما محتاجة compositor**، وبتدي حوالي 80% من شكل Fluent.
- المرحلة 5ب (compositor) هي الأكبر، وجاية في الآخر بعد قياس الأداء.

## طريقة الشغل والتحقق (نفس طريقة فايرفوكس)

1. كل مرحلة بتتبني، وبتتنسخ لصورة القرص، وبنصوّر الشاشة في QEMU.
2. صور **قبل وبعد** لنفس الشاشات (سطح المكتب، Notepad، Control Panel، Firefox، SPSS) في `docs/design/`.
3. نتأكد إن البرامج الشغالة (SPSS، Firefox) ما اتكسرت بعد كل مرحلة.
4. قياس أداء الرسم (زمن رسم النافذة) عشان التصميم ما يبطّء النظام.
5. كل مرحلة بترتفع لـ main بـ commits صغيرة، وبتتوثق في README بالعربي.
6. كل ميزة ليها مفتاح في الـ registry لإيقافها، عشان لو سببت مشكلة في جهاز حقيقي.
