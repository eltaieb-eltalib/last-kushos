# 07 — واجهة REST للتحكم من الخارج

لأي برنامج أو وكيل خارجي يريد التحكم في الجهاز بنفسه (بدون حلقة الوكيل المدمجة).

```bash
export KUSH_API_TOKEN=$(openssl rand -hex 24)
kush-agent serve --port 8765 --config config/models.yaml   # السياسة تُقرأ من القسم policy
```

| الطلب | الوصف |
|---|---|
| `GET /health` | بدون توكن: `{"ok": true}` |
| `GET /vms` | قائمة الأجهزة وحالتها |
| `GET /vms/{name}/screenshot` | صورة PNG |
| `POST /vms/{name}/actions` | تنفيذ فعل من الأفعال الـ 17 |

```bash
curl -s -H "Authorization: Bearer $KUSH_API_TOKEN" \
     -d '{"name": "left_click", "input": {"coordinate": [120, 300]}}' \
     http://127.0.0.1:8765/vms/kush/actions
# {"is_error": false, "text": "OK", "image_png_b64": null}
```
رد `screenshot`/`zoom` يحتوي `image_png_b64` و`width` و`height` (أبعاد الصورة التي تُحسب بها الإحداثيات).

- كل فعل يمر على نفس **Policy**. الأفعال التي تحتاج موافقة تُرفض (لا يوجد إنسان على هذه القناة) إلا مع `--approve-all`.
- الأكواد: `401` توكن خاطئ، `404` جهاز غير موجود أو متوقف، `400` طلب غير صالح.

## مخطط
- **MCP Server**: نفس الأفعال كأدوات MCP، ليتحكم في Kush أي عميل MCP (Claude Desktop، Claude Code…).
- توكن لكل جهاز بصلاحيات (مشاهدة فقط / تحكم كامل) ومدة صلاحية.
- WebSocket لبث الأحداث.
