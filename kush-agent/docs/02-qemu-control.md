# 02 — التحكم في QEMU

## أمر التشغيل الذي يبنيه `VM.command()`
مثال على لينكس بدون تسريع (كما في بيئة التطوير السحابية):
```bash
qemu-system-x86_64 -name kush -machine pc -m 2048 -smp 2 \
  -accel tcg,thread=multi -cpu qemu64 \
  -qmp unix:~/.kush-agent/vms/kush/qmp.sock,server=on,wait=off \
  -vnc unix:~/.kush-agent/vms/kush/vnc.sock -display none -vga std \
  -usb -device usb-tablet \
  -drive file=kush.qcow2,format=qcow2,if=ide,cache=writeback \
  -cdrom bootcd.iso -boot d \
  -netdev user,id=n0 -device e1000,netdev=n0 \
  -chardev socket,id=ga,path=~/.kush-agent/vms/kush/ga.sock,server=on,wait=off \
  -serial null -serial chardev:ga \
  -sandbox on,obsolete=deny,elevateprivileges=deny,spawn=deny,resourcecontrol=deny
```

| الخيار | لماذا |
|---|---|
| `-machine pc`، `if=ide`، `e1000` | الأكثر استقراراً مع ReactOS/Kush. virtio متاح بـ `--disk-if virtio` بعد التأكد من التعريفات |
| `-usb -device usb-tablet` | فأرة بإحداثيات **مطلقة** (0..32767) ← النقرة تقع في المكان المطلوب بالضبط |
| `-serial null -serial chardev:ga` | COM1 فارغ (قد يستخدمه مصحّح أخطاء النواة)، و**COM2** قناة kushagent |
| `-display none` + `-vnc` | لا نافذة على الخادم؛ البث للبشر عبر VNC |
| `-sandbox on,...` | seccomp: يمنع QEMU من تشغيل برامج أو رفع صلاحياته (لينكس فقط) |
| `-loadvm ready` | البدء من لقطة جاهزة بدل الإقلاع الكامل |

على **ويندوز** تُستخدم منافذ TCP بدل unix sockets: QMP على `127.0.0.1:47000`، القناة على `47001`، VNC على `47002`.

## قنوات التحكم

### 1) QMP (الأساسية)
بروتوكول JSON. بعد الاتصال يرسل QEMU تحية، ونرسل `qmp_capabilities`، ثم الأوامر:

| الهدف | الأمر |
|---|---|
| صورة الشاشة | `screendump {"filename": "...", "format": "png"}` (QEMU ≥ 7.1، وإلا PPM) |
| تحريك الفأرة | `input-send-event` بأحداث `abs` على المحور x و y (0..32767) |
| ضغط زر | حدث `btn` بـ `left/right/middle/wheel-up/wheel-down/wheel-left/wheel-right` |
| مفتاح | حدث `key` بـ qcode، مثل `ret`، `ctrl`، `a` |
| لقطة | `human-monitor-command` بـ `savevm ready` / `loadvm ready` |
| الحالة | `query-status` |

ميزة QMP: يعمل **بدون أي برنامج داخل النظام**، حتى أثناء الإقلاع أو عند تعطل النظام.

### 2) VNC
للمشاهدة البشرية فقط (والبث للمتصفح لاحقاً عبر noVNC). مثال:
```bash
socat TCP-LISTEN:5901,reuseaddr,fork UNIX-CONNECT:$HOME/.kush-agent/vms/kush/vnc.sock &
vncviewer localhost:5901
```

### 3) قناة النظام الضيف (kushagent)
للأشياء التي لا يقدر عليها QMP: كتابة **العربي** (QMP يرسل مفاتيح فيزيائية بتخطيط US فقط)، تشغيل أوامر وقراءة ناتجها، قائمة النوافذ، نقل الملفات.
انظر [06-guest-agent-protocol.md](06-guest-agent-protocol.md).

## الإحداثيات
الصورة التي يراها النموذج قد تكون مصغّرة (الحد الافتراضي 1568px للضلع الأطول، و4784 visual token).
`ScreenGeometry` يحوّل: إحداثيات الصورة ← بكسلات الشاشة الحقيقية ← مدى usb-tablet (0..32767).
`zoom` يقص منطقة بدقة كاملة، **لكن الإحداثيات تبقى بإطار الشاشة الكاملة**.

## الأقراص والصورة الذهبية
- `--disk x.qcow2 --disk-size 20G`: قرص جديد فارغ (للتثبيت).
- `--base-image golden.qcow2 --disk user1.qcow2`: نسخة **Copy-on-Write** فوق الصورة الذهبية، تُنشأ في أقل من ثانية.
- `kush-agent vm snapshot NAME save ready` ثم `vm start ... --loadvm ready`: يرجع الجهاز لنفس الحالة في ثوانٍ.

## التسريع
| المسرّع | المضيف | الشرط |
|---|---|---|
| KVM | Linux | `/dev/kvm` موجود ويمكن فتحه، والمعالج يكشف vmx/svm |
| WHPX | Windows فقط | VT-x مفعّل في BIOS + ميزة "Windows Hypervisor Platform" |
| HVF | macOS | Hypervisor.framework |
| TCG | أي مضيف | محاكاة بالبرنامج، أبطأ 5–20 مرة |

`kush-agent accel` يعرض المسرّع المختار والسبب. للإجبار: `--accel tcg` أو `KUSH_ACCEL=tcg`.
**مؤكد:** بيئة التطوير السحابية الحالية بدون vmx/svm ولا `/dev/kvm` ← TCG فقط. الإنتاج يحتاج KVM، وللسرعة محلياً: ويندوز + WHPX (لابتوب L440).
