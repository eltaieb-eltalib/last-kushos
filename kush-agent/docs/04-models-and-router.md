# 04 — النماذج الخارجية والموجّه حسب الـ prompt

## المحوّلات (Adapters)
كل محوّل يحوّل محادثة مزوّد معيّن إلى نفس الواجهة: `start(task, system)` ← `step()` ← `add_results(...)`.

### Claude — `provider: anthropic`
- يستخدم أداة Computer Use **الأصلية**: `{"type": "computer_toolset_20260801"}` (بدون اسم أو أبعاد شاشة، بدون beta header).
- الأفعال تأتي كـ `tool_use` اسمها هو الفعل، وتحمل `toolset_name: "computer"`، وقد تأتي عدة أفعال في رد واحد.
- كل نتيجة ترجع ومعها `toolset_name`، والصور جاهزة بحجم مناسب (الـ API لا يصغّر الصور).
- الافتراضي: `claude-opus-5-5`، `effort: high`.
- `fallbacks: true` (الافتراضي): إذا رفض النموذج دوراً لأسباب أمنية، الـ API يعيد الدور على نموذج بديل
  (`server-side-fallback-2026-07-01` + `fallbacks: "default"`). أطفئه بـ `fallbacks: false`.
- كل الكتل (thinking، tool_use، fallback) ترجع للمحادثة **كما هي** بدون تعديل.
- المفتاح من `ANTHROPIC_API_KEY` أو `api_key_env`.

### أي نموذج متوافق مع OpenAI — `provider: openai_compat`
- يعمل مع أي خادم `/v1/chat/completions` يدعم الصور واستدعاء الدوال: نماذج مستضافة، أو محلية عبر Ollama / vLLM / LM Studio.
- الأفعال الـ 17 تُعرض كدوال (functions) بنفس الأسماء والمدخلات.
- الصور ترجع في رسالة `user` (لأن رسائل `tool` نصية فقط)، ويُحتفظ بآخر 3 صور فقط لتقليل التكلفة.
- `api_key_env` لاسم متغير البيئة الذي يحمل المفتاح.

## الموجّه (Router)
الأولوية:
1. `--model alias` من سطر الأوامر.
2. `@alias` في **بداية** الـ prompt: `@local افتح المفكرة` (يُحذف من النص قبل إرساله).
3. أول قاعدة `routes` يطابق تعبيرها النمطي الـ prompt (بدون حساسية لحالة الأحرف، يدعم العربي).
4. `default`.

ثم تُبنى **سلسلة**: النموذج المختار ثم `fallback` بالترتيب (بدون تكرار). إذا فشل نموذج (خطأ API أو رفض)،
يستلم التالي المهمة بمحادثة جديدة ومعه ملخص: ماذا حدث وما آخر ملاحظات النموذج السابق.

```yaml
models:
  claude: {provider: anthropic, model: claude-opus-5-5, effort: high}
  local:  {provider: openai_compat, base_url: "http://localhost:11434/v1", model: "qwen2.5vl:7b"}
  mine:   {provider: openai_compat, base_url: "https://...", model: "...", api_key_env: MY_KEY}
routes:
  - {match: "install|تثبيت|ثبّت", model: claude}
  - {match: "notepad|اكتب", model: local}
default: claude
fallback: [claude, local]
```

```bash
kush-agent route "ثبّت Firefox" --config config/models.yaml
# {"model": "claude", "reason": "rule /install|تثبيت|ثبّت/", "chain": ["claude", "local"], ...}
```

## إضافة مزوّد جديد
اكتب صنفاً يرث `ModelAdapter` (`models/base.py`) وأضف اسمه في `build_adapter` (`models/router.py`).
الشرط الوحيد: أن يُرجع `Step` بأفعال من الأسماء الـ 17.

## ما لم يُختبر بعد
المحوّلان مختبران بعميل وهمي يحاكي شكل الردود. **لم تتم تجربة حقيقية بمفتاح API** في هذه البيئة (لا يوجد مفتاح).
