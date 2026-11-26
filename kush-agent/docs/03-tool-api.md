# 03 — Tool API: الأفعال الـ 17

لغة أفعال **واحدة** لكل النماذج، مطابقة لأعضاء أداة Anthropic `computer_toolset_20260801`.
الإحداثيات دائماً بكسلات **آخر صورة شاشة كاملة** أُرسلت للنموذج، والأصل أعلى اليسار.

| # | الفعل | المدخلات | التنفيذ عندنا |
|---|---|---|---|
| 1 | `screenshot` | `{}` | `screendump` ← تصغير ← PNG |
| 2 | `zoom` | `region: [x0,y0,x1,y1]` | قص بالدقة الكاملة ثم تصغير |
| 3 | `left_click` | `coordinate?`, `text?` (مفاتيح مساعدة) | تحريك مطلق + ضغط/رفع |
| 4 | `right_click` | مثل left_click | |
| 5 | `middle_click` | مثل left_click | |
| 6 | `double_click` | مثل left_click | ضغطتان بفاصل 60ms |
| 7 | `triple_click` | مثل left_click | |
| 8 | `left_click_drag` | `start_coordinate`, `coordinate`, `text?` | ضغط ← 10 خطوات تحريك ← رفع |
| 9 | `mouse_move` | `coordinate` | |
| 10 | `left_mouse_down` | `{}` | |
| 11 | `left_mouse_up` | `{}` | |
| 12 | `cursor_position` | `{}` | يرجع `X=.., Y=..` (آخر موضع أرسلناه) |
| 13 | `scroll` | `scroll_direction` (up/down/left/right), `scroll_amount`, `coordinate?`, `text?` | أزرار العجلة |
| 14 | `type` | `text` | مفاتيح US عبر QMP؛ أي حرف غير موجود (عربي…) عبر kushagent |
| 15 | `key` | `text` مثل `Return`، `ctrl+s`، `alt+Tab`، `super+r`؛ `repeat?` | |
| 16 | `hold_key` | `text`, `duration` (≤300) | |
| 17 | `wait` | `duration` (≤300) | |

## النتيجة (`ToolResult`)
- `screenshot` و `zoom`: صورة PNG.
- `cursor_position`: نص.
- الباقي: `OK`.
- الخطأ: `is_error=true` مع وصف قصير. في الدفعة: أول خطأ يوقف الباقي، ويُرد عليها بـ
  `Not executed: an earlier computer action in this turn failed.`

## أسماء المفاتيح المدعومة
`Return/Enter, Escape, BackSpace, Tab, space, Delete, Insert, Home, End, Page_Up, Page_Down, Up, Down, Left, Right,
shift, ctrl, alt, super/win, F1..F12, Print, Pause, Menu, caps_lock` وأي حرف أو رمز على لوحة US.

## الاستخدام المباشر
```bash
kush-agent vm do kush double_click '{"coordinate": [40, 60]}'
kush-agent vm do kush key '{"text": "super+r"}'
kush-agent vm do kush scroll '{"scroll_direction": "down", "scroll_amount": 5}'
kush-agent vm do kush zoom '{"region": [0, 0, 300, 200]}' -o zoom.png
```
