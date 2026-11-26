# 11 — التثبيت على VPS واستخدام DeepSeek

## 1) قبل شراء الـ VPS: أهم شرط
الوكيل يشغّل Kush داخل QEMU. السرعة تعتمد على **KVM** (التسريع بالهاردوير):

| نوع الخادم | KVM؟ | النتيجة |
|---|---|---|
| VPS عادي بدون Nested Virtualization | ❌ | TCG فقط: يعمل لكنه **أبطأ 5–20 مرة**، وكل خطوة للوكيل ستأخذ ثوانٍ أطول |
| VPS يدعم **Nested Virtualization** / "KVM inside VPS" | ✅ | سرعة قريبة من الطبيعي |
| خادم مخصص (Dedicated / Bare-metal) | ✅ | الأفضل للإنتاج وتعدد الأجهزة |

**اسأل المزوّد قبل الدفع:** "هل الـ VPS يدعم nested virtualization ويظهر فيه `/dev/kvm`؟"
وبعد الشراء شغّل فوراً (ولو النتيجة سلبية اطلب الاسترجاع أو الترقية):
```bash
bash deploy/check-host.sh
```

**الموارد المقترحة لبداية بجهاز Kush واحد أو اثنين:** 4 أنوية، 8GB RAM، 60GB قرص، Ubuntu 24.04.
كل جهاز Kush يحتاج تقريباً 1–2GB RAM و2 vCPU.

## 2) التثبيت (أمر واحد)
```bash
git clone <رابط المستودع> && cd kush-agent       # أو انسخ مجلد kush-agent للخادم
sudo bash deploy/install.sh                          # اللوحة على 127.0.0.1:8080
# أو مع دومين و HTTPS تلقائي (وجّه سجل A للدومين لعنوان الـ VPS أولاً):
sudo DOMAIN=panel.example.com bash deploy/install.sh
```
السكربت:
- يثبّت QEMU و Python ويُنشئ مستخدماً `kush` (ضمن مجموعة `kvm`).
- ينسخ البرنامج إلى `/opt/kush-agent` مع بيئة Python خاصة.
- يُنشئ `/etc/kush-agent/models.yaml` (الإعدادات) و `/etc/kush-agent/env` (الأسرار، صلاحية 600) **ويطبع كلمة مرور اللوحة**.
- يُنشئ خدمة systemd اسمها `kush-admin` (الأجهزة تبقى تعمل حتى لو أُعيد تشغيل اللوحة).
- مع `DOMAIN`: يثبّت Caddy بشهادة HTTPS تلقائية ويجعل الكوكي `Secure`.

بدون دومين، افتح اللوحة عبر نفق SSH (أأمن من فتح منفذ للإنترنت):
```bash
ssh -L 8080:127.0.0.1:8080 root@IP_الخادم     # ثم افتح http://localhost:8080
```

| المسار | المحتوى |
|---|---|
| `/etc/kush-agent/env` | كلمة مرور اللوحة، المفاتيح (`DEEPSEEK_API_KEY`...) |
| `/etc/kush-agent/models.yaml` | النماذج والقواعد (قابل للتعديل من اللوحة) |
| `/var/lib/kush-agent/images/` | ضع هنا `bootcd.iso` والصور الذهبية `.qcow2` |
| `/var/lib/kush-agent/vms/` | الأجهزة وأقراصها |
| `/var/lib/kush-agent/runs/` | سجلات المهام ولقطاتها |

أوامر مفيدة: `systemctl status kush-admin`، `journalctl -u kush-admin -f`، `systemctl restart kush-admin`.

## 3) DeepSeek
DeepSeek يوفّر واجهة متوافقة مع OpenAI على `https://api.deepseek.com`، والموديل **`deepseek-flash`** يقبل الصور
(حسب توثيق DeepSeek الحالي)، فيعمل مع محوّل `openai_compat` بدون أي كود جديد:

```yaml
models:
  deepseek:
    provider: openai_compat
    base_url: "https://api.deepseek.com"
    model: "deepseek-flash"
    api_key_env: DEEPSEEK_API_KEY
default: deepseek
fallback: [deepseek, claude]     # اختياري: نموذج احتياطي لو فشل DeepSeek
```
ثم ضع المفتاح في `/etc/kush-agent/env`:
```
DEEPSEEK_API_KEY=sk-...
```
و `systemctl restart kush-admin`. صفحة "نظرة عامة" تُظهر هل المفتاح موجود.

**تنبيهات صادقة:**
- لم أتمكن من تجربة DeepSeek فعلياً هنا (لا يوجد مفتاح). التوثيق لا يذكر صراحة أن **استدعاء الدوال (tools) مع الصور معاً** مدعوم على `deepseek-flash`؛ جرّب مهمة بسيطة أولاً.
- النماذج العامة (غير المدرّبة على التحكم في الحاسوب) غالباً أقل دقة في تحديد مكان النقر من نماذج Computer Use المتخصصة.
  لذلك القواعد في `routes` تتيح إرسال المهام البسيطة لـ DeepSeek والمعقدة لنموذج آخر، و`fallback` يكمل إن فشل.
- راقب الاستهلاك: كل خطوة ترسل صورة شاشة (حتى ~1024 توكن للصورة عند DeepSeek).

## 4) أول تشغيل
1. ارفع `bootcd.iso` لـ Kush إلى `/var/lib/kush-agent/images/` (`scp bootcd.iso root@IP:/var/lib/kush-agent/images/ && chown kush: ...`).
2. اللوحة ← **الأجهزة** ← جهاز جديد ← اختر الـ ISO ← إنشاء وتشغيل.
3. افتح شاشة الجهاز وأكمل تثبيت Kush بالنقر على الشاشة نفسها.
4. بعد الإقلاع: احفظ لقطة `ready`.
5. اكتب مهمة واضغط **تشغيل الوكيل**، وتابع الخطوات والموافقات.

## الأمان
- لا تفتح المنفذ 8080 للإنترنت مباشرة: استخدم نفق SSH أو Caddy مع HTTPS.
- فعّل جدار ناري: `ufw allow OpenSSH && ufw allow 443 && ufw enable` (و 80 لشهادة Caddy).
- كلمة مرور قوية في `env`، وتغييرها يتطلب `systemctl restart kush-admin`.
