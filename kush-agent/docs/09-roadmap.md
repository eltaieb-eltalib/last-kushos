# 09 — خارطة الطريق وما أُنجز

| المرحلة | العمل | الحالة |
|---|---|---|
| 0 | تشغيل QEMU، QMP (صورة/فأرة/كيبورد)، اكتشاف التسريع | ✅ |
| 0 | حلقة وكيل + نموذج خارجي | ✅ (مختبرة بنماذج وهمية) |
| 0 | تجربة "افتح Notepad واكتب" على Kush حقيقي بنموذج حقيقي | ⏳ تحتاج ISO + مفتاح API |
| 1 | kushagent.exe (أوامر، عربي، نوافذ، ملفات) | 🟡 مكتوب ويُبنى؛ لم يُجرّب داخل Kush |
| 1 | صورة ذهبية qcow2 من `bootcd.iso` آلياً (CI) + لقطة `ready` | ⏳ |
| 2 | Node Agent (CLI + REST حالياً) | 🟡 أساس موجود (`vm` + `serve`) |
| 2 | منسّق عدة خوادم + Warm Pool | ⏳ |
| 3 | Control Plane متعدد المستخدمين + واجهة ويب + noVNC | ⏳ |
| 4 | Planner/Verifier، استرجاع تلقائي من اللقطات، Eval Suite | ⏳ |
| 4.2 | قنوات Instinct (رسائل، مكالمات، بريد، موافقات) | ⏳ [التصميم](08-instinct-channels.md) |
| 4.5 | نماذج خارجية + موجّه + fallback | ✅ |
| 4.5 | REST API للتحكم الخارجي | ✅ |
| 4.5 | MCP Server + SDK | ⏳ |
| 5 | الإنتاج: KVM إلزامي، عزل، مراقبة، فوترة | ⏳ |
| 6 | Linux و Windows كأنظمة ضيفة | ⏳ (الكود عام؛ يتغير `os_name` وإعدادات الجهاز) |

## الخطوات التالية المباشرة
1. تنزيل `bootcd.iso` من إصدارات Kush وتشغيله: `kush-agent vm start kush --iso bootcd.iso --disk kush.qcow2 --boot d`.
2. تثبيت Kush، نسخ kushagent، حفظ لقطة `ready`.
3. أول تشغيل حقيقي: `kush-agent run kush "افتح Notepad واكتب Hello" --config config/models.yaml`.
4. على L440: نفس الخطوات مع WHPX ومقارنة الزمن مع TCG.
5. سكربت CI لبناء الصورة الذهبية.
