# kush-agent — منصة الوكيل الذكي السحابي على Kush OS

مساعد شخصي على طريقة **Instinct**: كمبيوتر كامل يعمل داخل **QEMU**، ونماذج ذكاء اصطناعي **خارجية** تتحكم فيه **من الخارج**
(صورة الشاشة ← تفكير ← ضغط وكتابة)، ويُختار النموذج حسب الـ prompt. نظام التشغيل الأساسي هو **Kush OS** (المبني على ReactOS)، وأنظمة أخرى لاحقاً.

> الخطة الكاملة: [`docs/00-PLAN.md`](docs/00-PLAN.md)

## الحالة الحالية (المرحلة 0 + أجزاء من 1 و2 و4.5)

| الجزء | الحالة | ملاحظة |
|---|---|---|
| تشغيل QEMU + اكتشاف التسريع (KVM / WHPX / HVF / TCG) | ✅ يعمل ومختبر | اختبار حقيقي على QEMU 8.2 بـ TCG |
| التحكم عبر QMP: صورة الشاشة، الفأرة، الكيبورد، العجلة | ✅ يعمل ومختبر | usb-tablet بإحداثيات مطلقة |
| اللقطات `savevm` / `loadvm` | ✅ يعمل ومختبر | يحتاج قرص qcow2 |
| Tool API (17 فعل مطابقة لـ `computer_toolset_20260801`) | ✅ يعمل ومختبر | |
| محوّل Claude (أداة Computer Use الأصلية) | ✅ مكتوب ومختبر بعميل وهمي | **لم يُجرّب بمفتاح API حقيقي بعد** |
| محوّل أي نموذج متوافق مع OpenAI (GPT، Ollama، vLLM، نموذجك) | ✅ مكتوب ومختبر بخادم وهمي | |
| الموجّه حسب الـ prompt (`@alias`، قواعد YAML، fallback) | ✅ مختبر | يدعم العربي |
| محرك القواعد والموافقات (Policy) | ✅ مختبر | |
| حلقة الوكيل + سجل الخطوات واللقطات + التبديل لنموذج احتياطي | ✅ مختبر | |
| واجهة الأوامر `kush-agent` | ✅ مختبرة على QEMU حقيقي | |
| REST API للتحكم الخارجي | ✅ مختبرة على QEMU حقيقي | |
| **لوحة الإدارة** (ويب، عربي): أجهزة، شاشة حية بالنقر، مهام، موافقات، إعدادات | ✅ مختبرة على QEMU حقيقي ومتصفح | [12-admin-panel.md](docs/12-admin-panel.md) |
| تثبيت على VPS بأمر واحد (systemd + HTTPS اختياري) | 🟡 السكربت جاهز | لم يُجرّب على VPS حقيقي بعد |
| DeepSeek (`deepseek-flash` يقبل الصور) | 🟡 إعداد جاهز | لم يُجرّب بمفتاح حقيقي |
| `kushagent.exe` داخل النظام (أوامر، كتابة عربي، النوافذ، الملفات) | 🟡 يُبنى بدون أخطاء (mingw x64) | **لم يُشغَّل داخل Kush بعد** |
| صورة Kush ذهبية + إقلاع Kush فعلي تحت الوكيل | ⏳ التالي | |
| MCP Server، بوابة الرسائل (واتساب/SMS)، بريد الوكيل | ⏳ مخطط | انظر [خارطة الطريق](docs/09-roadmap.md) |

## على VPS (الأسهل)

```bash
bash deploy/check-host.sh          # هل الخادم يدعم KVM؟
sudo bash deploy/install.sh        # يثبّت كل شيء ويطبع كلمة مرور لوحة الإدارة
```
التفاصيل واستخدام DeepSeek: [`docs/11-deploy-vps.md`](docs/11-deploy-vps.md).

## البدء السريع (محلياً)

```bash
# 1) المتطلبات: Python 3.10+ و QEMU
sudo apt install qemu-system-x86 qemu-utils          # Linux
pip install -e ".[dev]"

# 2) أي مسرّع متاح؟
kush-agent accel

# 3) شغّل Kush من الـ ISO (أول مرة: تثبيت على قرص جديد)
kush-agent vm start kush --iso bootcd.iso --disk ~/vms/kush.qcow2 --disk-size 20G --boot d

# 4) جرّب التحكم يدوياً
kush-agent vm screenshot kush -o screen.png
kush-agent vm do kush left_click '{"coordinate": [100, 200]}'
kush-agent vm do kush type '{"text": "notepad"}'
kush-agent vm do kush key '{"text": "Return"}'

# 5) احفظ لقطة "جاهز" بعد الإقلاع لتسريع كل تشغيل قادم
kush-agent vm snapshot kush save ready

# 6) شغّل الوكيل بنموذج خارجي
cp config/models.example.yaml config/models.yaml
export ANTHROPIC_API_KEY=...
kush-agent run kush "افتح Notepad واكتب Hello from the agent" --config config/models.yaml
kush-agent run kush "@local افتح المفكرة" --config config/models.yaml     # نموذج محلي
kush-agent route "ثبّت Firefox" --config config/models.yaml                # أي نموذج سيُختار؟
```

على ويندوز (مثل لابتوب L440 مع VT-x): فعّل **Windows Hypervisor Platform**، ثبّت QEMU لويندوز، والأوامر نفسها تعمل وتختار `whpx` تلقائياً.
التفاصيل في [`docs/10-development.md`](docs/10-development.md).

## هيكل المشروع

```
kush-agent/
├── kush_agent/
│   ├── vm/            # QEMU: accel.py (التسريع) qemu.py (التشغيل) qmp.py (البروتوكول)
│   │                  #       keys.py (المفاتيح) screen.py (الصور والإحداثيات) guest.py (عميل kushagent)
│   ├── tools/         # computer.py: الأفعال الـ 17 (Tool API)
│   ├── models/        # base.py, anthropic_adapter.py, openai_compat.py, router.py
│   ├── agent/         # loop.py (حلقة الوكيل) policy.py (القواعد والموافقات)
│   ├── admin/         # لوحة الإدارة: app.py (الخادم) + static/ (الواجهة)
│   ├── server.py      # REST API للتحكم الخارجي
│   └── cli.py         # واجهة الأوامر kush-agent
├── guest/kushagent/   # kushagent.exe (C/Win32) يعمل داخل Kush
├── config/            # models.example.yaml
├── deploy/            # install.sh, check-host.sh, kush-admin.service, Caddyfile
├── tests/             # pytest (وحدات + تكامل مع QEMU حقيقي)
└── docs/              # التوثيق الكامل بالعربي
```

## التوثيق

| الملف | المحتوى |
|---|---|
| [00-PLAN.md](docs/00-PLAN.md) | الخطة الكاملة للمنصة |
| [01-architecture.md](docs/01-architecture.md) | البنية والمكونات وتدفق البيانات |
| [02-qemu-control.md](docs/02-qemu-control.md) | تشغيل QEMU، QMP، VNC، قناة النظام الضيف، اللقطات، التسريع |
| [03-tool-api.md](docs/03-tool-api.md) | الأفعال الـ 17 ومدخلاتها |
| [04-models-and-router.md](docs/04-models-and-router.md) | النماذج الخارجية والموجّه حسب الـ prompt |
| [05-policy-security.md](docs/05-policy-security.md) | القواعد والموافقات والأمان |
| [06-guest-agent-protocol.md](docs/06-guest-agent-protocol.md) | بروتوكول kushagent.exe وتثبيته في Kush |
| [07-rest-api.md](docs/07-rest-api.md) | واجهة REST للتحكم من الخارج |
| [08-instinct-channels.md](docs/08-instinct-channels.md) | قنوات Instinct: رسائل، مكالمات، بريد الوكيل (مخطط) |
| [09-roadmap.md](docs/09-roadmap.md) | خارطة الطريق وما أُنجز |
| [10-development.md](docs/10-development.md) | التطوير والاختبارات والتشغيل على ويندوز |
| [11-deploy-vps.md](docs/11-deploy-vps.md) | التثبيت على VPS، شرط KVM، استخدام DeepSeek |
| [12-admin-panel.md](docs/12-admin-panel.md) | لوحة الإدارة: الصفحات والأمان والـ API |

## الترخيص
GPL-2.0-or-later (مثل ReactOS / Kush).
