# 06 — kushagent.exe: البرنامج داخل Kush

## لماذا؟
QMP يرسل **مفاتيح فيزيائية** بتخطيط US، فلا يستطيع كتابة العربي ولا قراءة ناتج أمر. `kushagent.exe` برنامج Win32 صغير (C)
يعمل داخل Kush/ReactOS/Windows ويتكلم مع المضيف عبر منفذ تسلسلي.

## البناء
```bash
cd guest/kushagent
make            # kushagent64.exe (x86_64-w64-mingw32-gcc)
make kushagent32.exe   # إن توفر i686-w64-mingw32-gcc
make test       # اختبار كود البروتوكول على المضيف
```
مُختبر: البناء بدون تحذيرات، واختبار البروتوكول (JSON، العربي، الإيموجي، base64).
**غير مختبر بعد:** التشغيل الفعلي داخل Kush.

## التثبيت داخل Kush
1. انسخ `kushagent64.exe` إلى `C:\Program Files\kushagent\` (عبر قرص ISO ثانٍ أو داخل الصورة الذهبية).
2. شغّله **في جلسة المستخدم** (وليس كخدمة في session 0، لأن `SendInput` و`EnumWindows` تحتاج سطح المكتب الظاهر):
   ```
   reg add HKLM\Software\Microsoft\Windows\CurrentVersion\Run /v kushagent /d "\"C:\Program Files\kushagent\kushagent64.exe\" \\.\COM2"
   ```
3. على المضيف: `kush-agent guest kush ping`.

القناة الافتراضية **COM2** (QEMU: `-serial null -serial chardev:ga`). مع `--guest-channel virtio` المنفذ هو
`\\.\Global\org.kush.agent.0` (يحتاج تعريف virtio-serial داخل النظام).

## البروتوكول
سطر JSON واحد لكل طلب ولكل رد، UTF-8. المضيف يرسل النص غير اللاتيني بصيغة `\uXXXX`.

```
-> {"id": 1, "method": "ping", "params": {}}
<- {"id":1,"result":{"ok":true,"agent":"0.1.0","computer":"KUSH-PC","os_version":"5.2.3790"}}
<- {"id":1,"error":"unknown method"}
```

| الطريقة | المدخلات | الناتج |
|---|---|---|
| `ping` | — | `ok`, `agent`, `computer`, `os_version` |
| `run` | `cmd` (سطر أوامر كامل)، `timeout_ms` (افتراضي 30000) | `exit_code`, `timed_out`, `output` (stdout+stderr، ≤256KB، محوّل من OEM إلى UTF-8) |
| `type_unicode` | `text` | يكتب بـ `SendInput(KEYEVENTF_UNICODE)`؛ `\n` يُرسل كمفتاح Enter حقيقي |
| `list_windows` | — | `[{hwnd, title, class, rect:[l,t,r,b], foreground}]` للنوافذ الظاهرة |
| `read_file` | `path` | `size`, `data` (base64، ≤16MB) |
| `write_file` | `path`, `data` (base64) | `ok` |

الأسطر التي ليست JSON (مثل رسائل الإقلاع على المنفذ) يتجاهلها العميل.

## العميل في بايثون
```python
from kush_agent.vm.guest import GuestClient
with GuestClient(vm.guest_address) as g:
    g.ping(); g.run("cmd /c ver"); g.type_unicode("مرحبا"); g.list_windows()
```
`kush-agent run ... --guest` يربط الوكيل بـ kushagent، فيُكتب أي نص عربي من النموذج عبره تلقائياً.

## الخطوات القادمة
- إضافته لشجرة بناء Kush (`modules/` أو `base/applications/`) ليُضمَّن في `bootcd.iso`.
- `clipboard_get/set` و`ui_tree` (شجرة عناصر الواجهة) لنقرات أدق.
