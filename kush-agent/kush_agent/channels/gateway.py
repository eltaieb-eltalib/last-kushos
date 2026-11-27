"""Messaging gateway: chat with the agent from Telegram or WhatsApp, Instinct-style.

A text message becomes a task on the user's VM. The agent's progress notes, approval questions
(with buttons) and the final result with a screenshot come back to the same chat.

Config (in models.yaml):

    channels:
      default_vm: kush1
      progress: true                   # send the model's notes while it works
      telegram:
        enabled: true
        token_env: TELEGRAM_BOT_TOKEN
        allowed_users: [123456789]     # numeric Telegram user ids
      whatsapp:
        enabled: true
        token_env: WHATSAPP_TOKEN
        phone_number_id: "1234567890"
        app_secret_env: WHATSAPP_APP_SECRET
        verify_token_env: WHATSAPP_VERIFY_TOKEN
        allowed_numbers: ["249912345678"]   # international format, digits only
"""

from __future__ import annotations

import json
import os
import re
import threading
from typing import Any

from .base import Channel, Incoming

HELP = """أهلاً! أنا وكيل Kush. أرسل لي أي مهمة وسأنفذها على جهازك، مثل:
• افتح Notepad واكتب قائمة المشتريات
• @deepseek افتح المتصفح   (لاختيار نموذج معيّن)

الأوامر:
/screen أو «الشاشة» — صورة الشاشة الآن
/status أو «الحالة» — حالة المهمة الحالية
/stop أو «إيقاف» — إيقاف المهمة
/vms أو «الأجهزة» — قائمة الأجهزة
/vm اسم — اختيار الجهاز
عند طلب موافقة: اضغط الزر أو أرسل «نعم» / «لا»."""

STATUS_AR = {"running": "تعمل", "done": "اكتملت ✅", "failed": "فشلت ❌", "limit": "بلغت الحد ⚠️",
             "stopped": "أُوقفت ⏹", "interrupted": "انقطعت"}
ACTION_AR = {"type": "كتابة", "key": "ضغط مفتاح", "hold_key": "ضغط مستمر", "left_click": "نقرة",
             "left_click_drag": "سحب", "scroll": "تمرير"}

_CMD = [
    (re.compile(r"^(/start|/help|مساعدة|help)$", re.I), "help"),
    (re.compile(r"^(/screen|الشاشة|شاشة|screen)$", re.I), "screen"),
    (re.compile(r"^(/status|الحالة|حالة|status)$", re.I), "status"),
    (re.compile(r"^(/stop|إيقاف|ايقاف|وقف|stop)$", re.I), "stop"),
    (re.compile(r"^(/vms|الأجهزة|الاجهزة)$", re.I), "vms"),
    (re.compile(r"^(?:/vm|جهاز)\s+([\w.-]+)$", re.I), "vm"),
    (re.compile(r"^(نعم|اي|أيوه|ايوه|yes|y|موافق)$", re.I), "yes"),
    (re.compile(r"^(لا|no|n|رفض)$", re.I), "no"),
]


class Gateway:
    def __init__(self, panel: Any, conf: dict[str, Any] | None = None):
        self.panel = panel
        self.conf = conf or {}
        self.channels: dict[str, Channel] = {}
        self.allowed: dict[str, set[str]] = {}
        self.selected_vm: dict[str, str] = {}
        self.lock = threading.Lock()
        panel.run_listeners.append(self.on_run_event)
        panel.approvals.listeners.append(self.on_approval)
        panel.channels_status = self.status

    # -- wiring ---------------------------------------------------------------------------------
    def add_channel(self, ch: Channel, allowed: list[Any]) -> None:
        self.channels[ch.name] = ch
        self.allowed[ch.name] = {re.sub(r"\D", "", str(a)) if ch.name == "whatsapp" else str(a) for a in allowed or []}

    def status(self) -> list[dict[str, Any]]:
        return [{"name": n, "connected": getattr(c, "connected", None), "error": getattr(c, "last_error", None),
                 "allowed": len(self.allowed.get(n, ()))} for n, c in self.channels.items()]

    @staticmethod
    def key(channel: str, chat_id: str) -> str:
        return f"{channel}:{chat_id}"

    def _ch_for(self, owner: str | None) -> tuple[Channel, str] | None:
        if not owner or ":" not in owner:
            return None
        name, chat = owner.split(":", 1)
        ch = self.channels.get(name)
        return (ch, chat) if ch else None

    def _say(self, ch: Channel, chat: str, text: str) -> None:
        try:
            ch.send_text(chat, text)
        except Exception as e:
            ch.last_error = f"{type(e).__name__}: {e}"  # type: ignore[attr-defined]

    def _vm_for(self, k: str) -> str | None:
        vm = self.selected_vm.get(k) or self.conf.get("default_vm")
        if vm:
            return vm
        vms = [v["name"] for v in self.panel.list_vms() if v["running"]]
        return vms[0] if len(vms) == 1 else None

    def _active_run(self, k: str) -> Any:
        return next((r for r in self.panel.runs.values() if r.owner == k and r.status == "running"), None)

    # -- incoming -------------------------------------------------------------------------------
    def handle(self, msg: Incoming) -> None:
        ch = self.channels.get(msg.channel)
        if ch is None:
            return
        uid = re.sub(r"\D", "", msg.user_id) if msg.channel == "whatsapp" else msg.user_id
        if uid not in self.allowed.get(msg.channel, set()):
            self._say(ch, msg.chat_id, f"غير مصرّح لك باستخدام هذا الوكيل.\nمعرّفك: {msg.user_id}\n"
                                       "أرسله لمدير اللوحة ليضيفه إلى القائمة المسموح بها.")
            return
        k = self.key(msg.channel, msg.chat_id)
        if msg.button:
            return self._button(ch, msg.chat_id, k, msg.button)
        text = (msg.text or "").strip()
        if not text:
            return self._say(ch, msg.chat_id, "أرسل المهمة كنص (الصور والرسائل الصوتية غير مدعومة بعد).")
        for rx, cmd in _CMD:
            m = rx.match(text)
            if m:
                return self._command(ch, msg.chat_id, k, cmd, m)
        self._task(ch, msg.chat_id, k, text)

    def _button(self, ch: Channel, chat: str, k: str, data: str) -> None:
        m = re.fullmatch(r"ap:(\w+):([01])", data)
        if not m:
            return
        p = self.panel.approvals.get(m.group(1))
        run = self.panel.runs.get(p.run_id) if p else None
        if not p or not run or run.owner != k:
            return self._say(ch, chat, "انتهت صلاحية هذا الطلب.")
        allow = m.group(2) == "1"
        self.panel.approvals.answer(p.id, allow)
        self._say(ch, chat, "تمت الموافقة ✅" if allow else "تم الرفض ❌")

    def _command(self, ch: Channel, chat: str, k: str, cmd: str, m: re.Match[str]) -> None:
        if cmd == "help":
            return self._say(ch, chat, HELP)
        if cmd == "vms":
            vms = self.panel.list_vms()
            cur = self._vm_for(k)
            lines = [f"{'▶ ' if v['name'] == cur else '• '}{v['name']} — {'يعمل' if v['running'] else 'متوقف'}" for v in vms]
            return self._say(ch, chat, "\n".join(lines) or "لا توجد أجهزة.")
        if cmd == "vm":
            name = m.group(1)
            if name not in [v["name"] for v in self.panel.list_vms()]:
                return self._say(ch, chat, f"لا يوجد جهاز باسم {name}.")
            self.selected_vm[k] = name
            return self._say(ch, chat, f"تم اختيار الجهاز {name}.")
        if cmd == "screen":
            vm = self._vm_for(k)
            if not vm:
                return self._say(ch, chat, "لم يُحدَّد جهاز. أرسل /vms ثم /vm اسم")
            try:
                shot = self.panel.computer(vm).capture()
                ch.send_photo(chat, shot.png, f"شاشة {vm}")
            except Exception as e:
                self._say(ch, chat, f"تعذّر أخذ صورة الشاشة: {e}")
            return
        if cmd == "status":
            run = self._active_run(k)
            if not run:
                return self._say(ch, chat, "لا توجد مهمة تعمل الآن.")
            return self._say(ch, chat, f"المهمة: {run.task}\nالنموذج: {run.model}\nالحالة: {STATUS_AR.get(run.status, run.status)}")
        if cmd == "stop":
            run = self._active_run(k)
            if not run:
                return self._say(ch, chat, "لا توجد مهمة تعمل الآن.")
            run.stop.set()
            return self._say(ch, chat, "جاري الإيقاف…")
        if cmd in ("yes", "no"):
            pend = [p for p in self.panel.approvals.list() if (r := self.panel.runs.get(p["run_id"])) and r.owner == k]
            if not pend:
                return self._task(ch, chat, k, m.group(0))
            self.panel.approvals.answer(pend[-1]["id"], cmd == "yes")
            return self._say(ch, chat, "تمت الموافقة ✅" if cmd == "yes" else "تم الرفض ❌")

    def _task(self, ch: Channel, chat: str, k: str, text: str) -> None:
        vm = self._vm_for(k)
        if not vm:
            return self._say(ch, chat, "لم يُحدَّد جهاز لك. أرسل /vms ثم /vm اسم_الجهاز")
        if self._active_run(k):
            return self._say(ch, chat, "عندك مهمة تعمل الآن. انتظر انتهاءها أو أرسل /stop")
        try:
            run = self.panel.start_run(vm, text, owner=k)
        except Exception as e:
            return self._say(ch, chat, f"تعذّر بدء المهمة: {e}")
        self._say(ch, chat, f"بدأت المهمة على {vm} بالنموذج {run.model} ⏳\nأرسل /screen لرؤية الشاشة أو /stop للإيقاف.")

    # -- events from the panel ------------------------------------------------------------------
    def on_run_event(self, run: Any, kind: str, data: dict[str, Any]) -> None:
        target = self._ch_for(run.owner)
        if not target:
            return
        ch, chat = target
        if kind == "step" and data.get("text") and self.conf.get("progress", True):
            self._say(ch, chat, "💬 " + data["text"][:700])
        elif kind == "model_error":
            self._say(ch, chat, f"⚠️ النموذج {data.get('model')} فشل، أجرّب البديل…")
        elif kind == "finished":
            status = STATUS_AR.get(run.status, run.status)
            parts = [f"المهمة {status}"]
            if data.get("final_text"):
                parts.append(data["final_text"])
            if data.get("error") and run.status != "done":
                parts.append(f"السبب: {data['error']}")
            self._say(ch, chat, "\n".join(parts))
            try:
                shot = self.panel.computer(run.vm).capture()
                ch.send_photo(chat, shot.png, "الشاشة الآن")
            except Exception:
                pass

    def on_approval(self, p: Any) -> None:
        run = self.panel.runs.get(p.run_id)
        target = self._ch_for(run.owner if run else None)
        if not target:
            return
        ch, chat = target
        what = ACTION_AR.get(p.action, p.action)
        detail = json.dumps(p.input, ensure_ascii=False)[:300]
        try:
            ch.send_buttons(chat, f"🔐 الوكيل يطلب موافقتك\n{what}: {detail}\nالسبب: {p.reason}",
                            [("موافقة", f"ap:{p.id}:1"), ("رفض", f"ap:{p.id}:0")])
        except Exception:
            self._say(ch, chat, f"🔐 الوكيل يطلب موافقتك: {what} {detail}\nأرسل «نعم» أو «لا».")


def build_gateway(panel: Any, conf: dict[str, Any]) -> Gateway | None:
    """Create the gateway and its channels from the `channels` config section. Returns None if none enabled."""
    cc = conf.get("channels") or {}
    tg, wa = cc.get("telegram") or {}, cc.get("whatsapp") or {}
    if not (tg.get("enabled") or wa.get("enabled")):
        return None
    gw = Gateway(panel, cc)
    env = lambda spec, key, default: os.environ.get(spec.get(key, default), "")  # noqa: E731
    if tg.get("enabled"):
        from .telegram import TelegramChannel

        ch = TelegramChannel(env(tg, "token_env", "TELEGRAM_BOT_TOKEN"), gw.handle)
        gw.add_channel(ch, tg.get("allowed_users") or [])
        ch.start()
    if wa.get("enabled"):
        from .whatsapp import WhatsAppChannel

        ch = WhatsAppChannel(env(wa, "token_env", "WHATSAPP_TOKEN"), str(wa.get("phone_number_id", "")),
                             env(wa, "app_secret_env", "WHATSAPP_APP_SECRET"),
                             env(wa, "verify_token_env", "WHATSAPP_VERIFY_TOKEN"), gw.handle,
                             api_version=wa.get("api_version", "v23.0"))
        gw.add_channel(ch, wa.get("allowed_numbers") or [])
        panel.webhooks["/webhooks/whatsapp"] = ch.webhook
    return gw
