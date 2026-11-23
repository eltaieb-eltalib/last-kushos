# وصفة بناء Kush 1.1 (bootcd.iso)

وصفة صغيرة قابلة لإعادة الإنتاج لبناء صورة الـ Live CD من المصدر. بدون أي تفاصيل
بيئة داخلية أو بيانات دخول — المصدر وحده على main كافٍ.

## المتطلبات

- Linux (أي توزيعة حديثة)، ذاكرة 2GB+، حوالي 15GB مساحة فارغة.
- حزم: `git cmake ninja-build gcc g++ bison flex python3` + مترجم MinGW-w64
  (`gcc-mingw-w64` / `g++-mingw-w64`) لأدوات البناء المضيفة، وباقي اعتماديات
  ReactOS الموثقة في وثيقة البناء الرسمية (ReactOS Build Environment مكافئ).

## الخطوات

```sh
git clone <رابط المستودع> kush
cd kush
mkdir build && cd build
../configure.sh -DENABLE_CCACHE=0   # يولّد ملفات Ninja في دليل build منفصل
ninja bootcd                        # الناتج: bootcd.iso
```

## ملاحظات

- `configure.sh` بتتعامل مع CMake بوضع out-of-tree؛ لا تبني داخل شجرة المصدر.
- أول بناء كامل بياخد وقت طويل (آلاف الأهداف)؛ البناات التالية تزايدية.
- `ninja livecd` يبني صورة Live إضافية، و`ninja hybridcd` صورة هجينة.
- للتجربة: أي QEMU حديث (`qemu-system-i386` أو `x86_64` بوضع 32-bit)
  مع قرص خام وذاكرة 512MB+ يكفي — انظر `tools/qemu/`.
- كل تغييرات الواجهة (Kush Fluent) محكومة بمفاتيح قتل في الريجستري تحت
  `HKCU\Software\Kush\Shell` — انظر `docs/design/kush-fluent-plan.md`.
