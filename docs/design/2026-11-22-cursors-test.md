# اختبار المؤشرات، 22 نوفمبر 2026

- بناء bootcd نجح محلياً؛ تغييرات العرض في user32 فقط، دون تعديل النواة.
- الحزمة الأصلية: light / regular / default، 17 ملفاً بلا تعديل؛ بصماتها في modern-cursor-sha256.txt.
- ISO نهائي محفوظ في مستودع النسخ الاحتياطية الخاص قبل الإقلاع، وأُعيد تنزيله ومطابقة SHA256:
  `337c6a49dd385e48dda36b269af762d0439eaec87123ab5517201c2239a2b162`.
- الموارد القياسية 14 دوراً: 12 CUR + صورتان ثابتتان مأخوذتان من منتصف ANI. الملفات الأصلية كاملة.
- نسخة الاختبار الأولى رسمت ANI داخل نافذة، لكن مؤشر الماوس الفعلي اختفى. ReactOS UserSetCursor لا يحرك ANI،
  ويستخدم صورة الكائن الأب بدل الإطار. تجنبت تعديل النواة باستخدام صورة إطار ثابت واضحة.
- cursorprobe.c بُني مؤقتاً كبرنامج Windows للاختبار وشُغّل من قرص مرن في QEMU؛ لا يُثبت داخل النظام.
- ClassicCursors=1 أعاد الموارد الأصلية في عملية اختبار جديدة، و0 أعاد الحديثة في عملية جديدة.
- Notepad وcmd وControl Panel وExplorer فتحت في نفس الإقلاع النهائي؛ لا KUSH-EXC في سجل الجلسة.
- لم يُختبر تثبيت على جهاز حقيقي أو ضمان توافق كل البرامج.

## مقتطف تحميل الموارد من الإقلاع النهائي

```text
KUSH-DBG ModernCursor CUR id=32100 loaded=1 size=32x32
KUSH-DBG ModernCursor CUR id=32101 loaded=1 size=32x32
KUSH-DBG ModernCursor ANI-static-frame id=32102 loaded=1 size=32x32
KUSH-DBG ModernCursor CUR id=32103 loaded=1 size=32x32
KUSH-DBG ModernCursor CUR id=32104 loaded=1 size=32x32
KUSH-DBG ModernCursor CUR id=32105 loaded=1 size=32x32
KUSH-DBG ModernCursor CUR id=32106 loaded=1 size=32x32
KUSH-DBG ModernCursor CUR id=32107 loaded=1 size=32x32
KUSH-DBG ModernCursor CUR id=32108 loaded=1 size=32x32
KUSH-DBG ModernCursor CUR id=32109 loaded=1 size=32x32
KUSH-DBG ModernCursor CUR id=32110 loaded=1 size=32x32
KUSH-DBG ModernCursor CUR id=32113 loaded=1 size=32x32
KUSH-DBG ModernCursor ANI-static-frame id=32111 loaded=1 size=32x32
KUSH-DBG ModernCursor CUR id=32112 loaded=1 size=32x32
```
