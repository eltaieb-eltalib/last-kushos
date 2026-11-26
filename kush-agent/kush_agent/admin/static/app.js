"use strict";
// Kush Agent admin panel. Vanilla JS, no build step. All server text is inserted with textContent.

const $ = (s, el = document) => el.querySelector(s);
const view = $("#view");
const render = (...kids) => view.replaceChildren(...kids.flat().filter((k) => k != null && k !== false));
let timers = [];
const clearTimers = () => { timers.forEach(clearInterval); timers = []; };

// ---------- helpers ---------------------------------------------------------------------------
function h(tag, attrs = {}, ...kids) {
  const el = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) {
    if (v == null || v === false) continue;
    if (k.startsWith("on")) el.addEventListener(k.slice(2), v);
    else if (k === "class") el.className = v;
    else el.setAttribute(k, v === true ? "" : v);
  }
  for (const kid of kids.flat()) if (kid != null && kid !== false) el.append(kid instanceof Node ? kid : document.createTextNode(String(kid)));
  return el;
}
function toast(msg) {
  const t = $("#toast"); t.textContent = msg; t.classList.add("show");
  clearTimeout(toast._t); toast._t = setTimeout(() => t.classList.remove("show"), 3200);
}
async function api(path, opts = {}) {
  const init = { method: opts.method || "GET", headers: { "X-Kush": "1" } };
  if (opts.body !== undefined) { init.headers["Content-Type"] = "application/json"; init.body = JSON.stringify(opts.body); }
  const r = await fetch(path, init);
  if (r.status === 401 && path !== "/api/login") { showLogin(); throw new Error("يجب تسجيل الدخول"); }
  if (opts.raw) { if (!r.ok) throw new Error((await r.json()).error || r.statusText); return r; }
  const data = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(data.error || r.statusText);
  return data;
}
const fmtTime = (t) => t ? new Date(t * 1000).toLocaleString("ar", { dateStyle: "short", timeStyle: "short" }) : "—";
const fmtSize = (b) => b > 1e9 ? (b / 1e9).toFixed(1) + " GB" : (b / 1e6).toFixed(0) + " MB";
const STATUS = { running: ["يعمل", "run"], done: ["اكتمل", "ok"], failed: ["فشل", "bad"], limit: ["بلغ الحد", "warn"], stopped: ["أُوقف", "warn"], interrupted: ["انقطع", "warn"] };
const statusPill = (s) => { const [t, c] = STATUS[s] || [s, ""]; return h("span", { class: "pill " + c }, t); };
const ACTION_AR = { screenshot: "صورة الشاشة", zoom: "تكبير", left_click: "نقرة", right_click: "نقرة يمين", double_click: "نقرة مزدوجة", triple_click: "نقرة ثلاثية", middle_click: "نقرة وسطى", left_click_drag: "سحب", mouse_move: "تحريك", scroll: "تمرير", type: "كتابة", key: "مفتاح", hold_key: "ضغط مستمر", wait: "انتظار", cursor_position: "موضع المؤشر", left_mouse_down: "ضغط", left_mouse_up: "رفع" };

// ---------- theme -----------------------------------------------------------------------------
function applyTheme(t) { if (t) document.documentElement.dataset.theme = t; else delete document.documentElement.dataset.theme; }
try { applyTheme(localStorage.getItem("kush-theme")); } catch (e) {}
$("#theme").onclick = () => {
  const dark = document.documentElement.dataset.theme === "dark" ||
    (!document.documentElement.dataset.theme && matchMedia("(prefers-color-scheme: dark)").matches);
  const next = dark ? "light" : "dark"; applyTheme(next);
  try { localStorage.setItem("kush-theme", next); } catch (e) {}
};

// ---------- auth ------------------------------------------------------------------------------
function showLogin() { clearTimers(); $("#app").hidden = true; $("#login").hidden = false; $("#password").focus(); }
$("#login-form").onsubmit = async (e) => {
  e.preventDefault(); $("#login-err").textContent = "";
  try { await api("/api/login", { method: "POST", body: { password: $("#password").value } }); $("#password").value = ""; boot(); }
  catch (err) { $("#login-err").textContent = err.message === "wrong password" ? "كلمة المرور غير صحيحة" : err.message; }
};
$("#logout").onclick = async () => { await api("/api/logout", { method: "POST" }).catch(() => {}); showLogin(); };

// ---------- views -----------------------------------------------------------------------------
async function overview() {
  const o = await api("/api/overview");
  const stat = (k, v, extra) => h("div", { class: "card stat" }, h("div", { class: "k" }, k), h("div", { class: "v" }, v), extra || null);
  const accelNote = o.accel.hardware ? null : h("div", { class: "note" },
    "هذا الخادم يعمل بدون تسريع بالهاردوير (TCG): الأجهزة ستكون بطيئة. ", h("span", { class: "mono" }, o.accel.reason),
    ". للسرعة استخدم خادماً يدعم KVM (Nested Virtualization أو Dedicated).");
  const qemuNote = o.qemu ? null : h("div", { class: "note" }, "QEMU غير مثبت على الخادم: ثبّته بـ apt install qemu-system-x86 qemu-utils");
  const models = h("table", {}, h("thead", {}, h("tr", {}, h("th", {}, "الاسم"), h("th", {}, "المزوّد"), h("th", {}, "الموديل"), h("th", {}, "المفتاح"))),
    h("tbody", {}, o.models.map((m) => h("tr", {}, h("td", {}, h("b", {}, m.name)), h("td", {}, m.provider), h("td", { class: "mono" }, m.model || "—"),
      h("td", {}, m.key_present === null ? h("span", { class: "pill" }, "لا يحتاج") : m.key_present ? h("span", { class: "pill ok" }, "موجود") :
        h("span", { class: "pill bad" }, "ناقص: ", h("span", { class: "mono" }, m.key_env)))))));
  render(
    h("h1", {}, "نظرة عامة"), h("p", { class: "sub" }, "حالة الخادم والأجهزة والمهام."),
    accelNote, qemuNote,
    h("div", { class: "grid stats" },
      stat("التسريع", o.accel.name.toUpperCase(), h("span", { class: "pill " + (o.accel.hardware ? "ok" : "warn") }, o.accel.hardware ? "هاردوير" : "برمجي")),
      stat("الأجهزة العاملة", `${o.vms_running} / ${o.vms_total}`),
      stat("مهام تعمل الآن", o.runs_active, h("span", { class: "muted" }, `من ${o.runs_total}`)),
      stat("موافقات تنتظر", o.approvals)),
    h("div", { class: "card" }, h("h2", {}, "النماذج"), h("div", { class: "tbl-wrap" }, models)));
}

async function vms() {
  const [list, imgs] = await Promise.all([api("/api/vms"), api("/api/images")]);
  const opt = (filter) => [h("option", { value: "" }, "— بدون —"), ...imgs.images.filter(filter).map((i) => h("option", { value: i.name }, `${i.name} (${fmtSize(i.size)})`))];
  const form = h("form", { class: "form" },
    h("label", {}, "الاسم", h("input", { name: "name", required: true, placeholder: "kush1", pattern: "[A-Za-z0-9][A-Za-z0-9_.-]*" })),
    h("label", {}, "ISO (تثبيت/تشغيل حي)", h("select", { name: "iso" }, opt((i) => i.name.endsWith(".iso")))),
    h("label", {}, "صورة ذهبية (qcow2)", h("select", { name: "base_image" }, opt((i) => !i.name.endsWith(".iso")))),
    h("label", {}, "الذاكرة (MB)", h("input", { name: "memory_mb", type: "number", value: 2048, min: 256, step: 256 })),
    h("label", {}, "المعالجات", h("input", { name: "cpus", type: "number", value: 2, min: 1, max: 16 })),
    h("label", {}, "حجم القرص", h("input", { name: "disk_size", value: "20G" })),
    h("button", { class: "btn primary", type: "submit" }, "إنشاء وتشغيل"));
  form.onsubmit = async (e) => {
    e.preventDefault();
    const d = Object.fromEntries(new FormData(form)); d.memory_mb = +d.memory_mb; d.cpus = +d.cpus;
    try { await api("/api/vms", { method: "POST", body: d }); toast("تم تشغيل الجهاز"); route(); } catch (err) { toast("خطأ: " + err.message); }
  };
  const power = async (name, op) => {
    if (op === "delete" && !confirm(`حذف الجهاز ${name} وقرصه نهائياً؟`)) return;
    try { await api(`/api/vms/${encodeURIComponent(name)}/${op}`, { method: "POST" }); toast("تم"); route(); } catch (err) { toast("خطأ: " + err.message); }
  };
  const rows = list.map((v) => h("tr", {},
    h("td", {}, h("a", { href: `#/vm/${encodeURIComponent(v.name)}` }, h("b", {}, v.name))),
    h("td", {}, v.running ? h("span", { class: "pill ok" }, "يعمل") : h("span", { class: "pill" }, "متوقف"), " ", v.busy ? h("span", { class: "pill run" }, "ينفّذ مهمة") : null),
    h("td", { class: "mono" }, v.accel), h("td", {}, `${v.memory_mb} MB · ${v.cpus} CPU`), h("td", { class: "mono" }, v.iso || v.disk || "—"),
    h("td", {}, h("div", { class: "row" },
      v.running ? h("button", { class: "btn sm", onclick: () => power(v.name, "stop") }, "إيقاف") : h("button", { class: "btn sm", onclick: () => power(v.name, "start") }, "تشغيل"),
      h("a", { class: "btn sm", href: `#/vm/${encodeURIComponent(v.name)}` }, "الشاشة"),
      h("button", { class: "btn sm danger", onclick: () => power(v.name, "delete") }, "حذف")))));
  render(
    h("h1", {}, "الأجهزة"), h("p", { class: "sub" }, "أجهزة QEMU الافتراضية. ضع ملفات ISO وqcow2 في: ", h("span", { class: "mono" }, imgs.dir)),
    h("div", { class: "card", style: "margin-bottom:16px" }, h("h2", {}, "جهاز جديد"), form),
    h("div", { class: "card" }, list.length ? h("div", { class: "tbl-wrap" }, h("table", {},
      h("thead", {}, h("tr", {}, ["الاسم", "الحالة", "التسريع", "الموارد", "القرص/ISO", ""].map((t) => h("th", {}, t)))), h("tbody", {}, rows)))
      : h("p", { class: "muted" }, "لا توجد أجهزة بعد.")));
}

async function vmDetail(name) {
  const enc = encodeURIComponent(name);
  const img = h("img", { alt: `شاشة الجهاز ${name}` });
  const wrap = h("div", { class: "screen-wrap" }, h("div", { class: "empty" }, "جاري تحميل الشاشة…"));
  let size = { w: 1, hgt: 1 }, live = true, url = null;
  const refresh = async () => {
    if (!live || document.hidden) return;
    try {
      const r = await api(`/api/vms/${enc}/screenshot`, { raw: true });
      size = { w: +r.headers.get("X-Width"), hgt: +r.headers.get("X-Height") };
      const nu = URL.createObjectURL(await r.blob()); img.src = nu; if (url) URL.revokeObjectURL(url); url = nu;
      if (!img.isConnected) wrap.replaceChildren(img);
    } catch (err) { wrap.replaceChildren(h("div", { class: "empty" }, "لا يمكن عرض الشاشة: " + err.message)); }
  };
  const act = async (name_, input) => {
    try { const r = await api(`/api/vms/${enc}/actions`, { method: "POST", body: { name: name_, input } }); if (r.is_error) toast("خطأ: " + r.text); setTimeout(refresh, 400); }
    catch (err) { toast("خطأ: " + err.message); }
  };
  const coords = (e) => { const b = img.getBoundingClientRect(); return [Math.round((e.clientX - b.left) * size.w / b.width), Math.round((e.clientY - b.top) * size.hgt / b.height)]; };
  img.addEventListener("click", (e) => act("left_click", { coordinate: coords(e) }));
  img.addEventListener("dblclick", (e) => { e.preventDefault(); act("double_click", { coordinate: coords(e) }); });
  img.addEventListener("contextmenu", (e) => { e.preventDefault(); act("right_click", { coordinate: coords(e) }); });
  const text = h("input", { placeholder: "اكتب نصاً ثم Enter (الإنجليزي عبر QMP، العربي يحتاج kushagent)", style: "flex:1;min-width:200px" });
  text.addEventListener("keydown", (e) => { if (e.key === "Enter" && text.value) { act("type", { text: text.value }); text.value = ""; } });
  const keyBtn = (label, k, ask) => h("button", { class: "btn sm", type: "button", onclick: () => { if (!ask || confirm(`إرسال ${k}؟`)) act("key", { text: k }); } }, label);
  const liveBtn = h("button", { class: "btn sm", type: "button", onclick: () => { live = !live; liveBtn.textContent = live ? "إيقاف التحديث" : "تشغيل التحديث"; refresh(); } }, "إيقاف التحديث");

  const snapTag = h("input", { value: "ready", style: "width:120px" });
  const snap = async (op) => { try { await api(`/api/vms/${enc}/snapshot`, { method: "POST", body: { op, tag: snapTag.value } }); toast(op === "save" ? "حُفظت اللقطة" : "استُرجعت اللقطة"); refresh(); } catch (err) { toast("خطأ: " + err.message); } };

  const models = await api("/api/models").catch(() => []);
  const task = h("textarea", { placeholder: "مثال: افتح Notepad واكتب Hello from Kush Agent ثم احفظ الملف على سطح المكتب", required: true });
  const model = h("select", {}, h("option", { value: "" }, "تلقائي (حسب الـ prompt)"), models.map((m) => h("option", { value: m.name }, m.name)));
  const routeInfo = h("p", { class: "muted", style: "margin:0" });
  let rt; task.addEventListener("input", () => { clearTimeout(rt); rt = setTimeout(async () => {
    if (!task.value.trim()) return (routeInfo.textContent = "");
    try { const d = await api(`/api/route?prompt=${encodeURIComponent(task.value)}&model=${encodeURIComponent(model.value)}`); routeInfo.textContent = `النموذج: ${d.model} (${d.reason}) — البدائل: ${d.chain.join(" ← ")}`; } catch (e) { routeInfo.textContent = ""; }
  }, 350); });
  const taskForm = h("form", { class: "grid" }, h("label", {}, "المهمة", task), h("div", { class: "row" }, model, h("button", { class: "btn primary", type: "submit" }, "تشغيل الوكيل")), routeInfo);
  taskForm.onsubmit = async (e) => {
    e.preventDefault();
    try { const r = await api("/api/runs", { method: "POST", body: { vm: name, task: task.value, model: model.value } }); location.hash = `#/run/${r.id}`; }
    catch (err) { toast("خطأ: " + err.message); }
  };

  render(
    h("div", { class: "row spread" }, h("div", {}, h("h1", {}, name), h("p", { class: "sub" }, "انقر على الشاشة للنقر، دبل كليك، وكليك يمين. التحديث كل ثانيتين.")), h("a", { href: "#/vms" }, "← كل الأجهزة")),
    h("div", { class: "grid two" },
      h("div", { class: "card" }, wrap,
        h("div", { class: "row", style: "margin-top:10px" }, text),
        h("div", { class: "keys" }, keyBtn("Enter", "Return"), keyBtn("Esc", "Escape"), keyBtn("Tab", "Tab"), keyBtn("⌫", "BackSpace"), keyBtn("Win", "super"),
          keyBtn("Win+R", "super+r"), keyBtn("Alt+Tab", "alt+Tab"), keyBtn("Alt+F4", "alt+F4"), keyBtn("Ctrl+Alt+Del", "ctrl+alt+Delete", true), liveBtn)),
      h("div", { class: "grid" },
        h("div", { class: "card" }, h("h2", {}, "مهمة للوكيل"), taskForm),
        h("div", { class: "card" }, h("h2", {}, "اللقطات (Snapshots)"),
          h("div", { class: "row" }, snapTag, h("button", { class: "btn sm", onclick: () => snap("save") }, "حفظ"), h("button", { class: "btn sm", onclick: () => snap("load") }, "استرجاع")),
          h("p", { class: "muted" }, "تحتاج قرص qcow2. احفظ لقطة بعد الإقلاع لتسريع كل تشغيل قادم.")))));
  refresh(); timers.push(setInterval(refresh, 2000));
}

async function runs() {
  const list = await api("/api/runs");
  render(h("h1", {}, "المهام"), h("p", { class: "sub" }, "كل تشغيلات الوكيل. شغّل مهمة جديدة من صفحة أي جهاز."),
    h("div", { class: "card" }, list.length ? h("div", { class: "tbl-wrap" }, h("table", {},
      h("thead", {}, h("tr", {}, ["المهمة", "الجهاز", "النموذج", "الحالة", "الوقت"].map((t) => h("th", {}, t)))),
      h("tbody", {}, list.map((r) => h("tr", {}, h("td", {}, h("a", { href: `#/run/${r.id}` }, r.task.length > 70 ? r.task.slice(0, 70) + "…" : r.task)),
        h("td", {}, r.vm), h("td", { class: "mono" }, r.model), h("td", {}, statusPill(r.status)), h("td", { class: "muted" }, fmtTime(r.created)))))))
      : h("p", { class: "muted" }, "لا توجد مهام بعد.")));
  if (list.some((r) => r.status === "running")) timers.push(setInterval(() => { if (!document.hidden) route(true); }, 4000));
}

async function runDetail(id) {
  const r = await api(`/api/runs/${encodeURIComponent(id)}`);
  const steps = r.steps.filter((s) => ["step", "action", "model", "model_error"].includes(s.kind)).map((s) => {
    if (s.kind === "model") return h("li", {}, h("div", { class: "meta" }, "بدأ النموذج"), h("b", { class: "mono" }, s.model));
    if (s.kind === "model_error") return h("li", {}, h("span", { class: "pill bad" }, "خطأ النموذج"), " ", h("span", { class: "mono" }, s.error));
    if (s.kind === "step") return s.text || s.error ? h("li", {}, h("div", { class: "meta" }, `${s.model} · خطوة ${s.n}`), s.text ? h("div", { class: "text" }, s.text) : null, s.error ? h("div", { class: "err" }, s.error) : null) : null;
    return h("li", {}, h("div", { class: "row" }, h("b", {}, ACTION_AR[s.name] || s.name), h("span", { class: "mono muted" }, JSON.stringify(s.input)),
      s.policy === "ask" ? h("span", { class: "pill warn" }, "احتاج موافقة") : s.policy === "deny" ? h("span", { class: "pill bad" }, "ممنوع") : null, s.error ? h("span", { class: "pill bad" }, s.text || "خطأ") : null),
      s.image ? h("img", { src: `/api/runs/${encodeURIComponent(id)}/${s.image}`, alt: "لقطة", loading: "lazy" }) : null);
  }).filter(Boolean);
  const rep = r.report || {};
  const stop = async () => { try { await api(`/api/runs/${encodeURIComponent(id)}/stop`, { method: "POST" }); toast("طُلب الإيقاف"); } catch (e) { toast("خطأ: " + e.message); } };
  render(
    h("div", { class: "row spread" }, h("div", {}, h("h1", {}, "مهمة"), h("p", { class: "sub" }, r.task)), h("div", { class: "row" },
      r.status === "running" ? h("button", { class: "btn danger", onclick: stop }, "إيقاف") : null, h("a", { href: `#/vm/${encodeURIComponent(r.vm)}` }, "شاشة الجهاز"), h("a", { href: "#/runs" }, "← المهام"))),
    h("div", { class: "grid stats" },
      h("div", { class: "card stat" }, h("div", { class: "k" }, "الحالة"), h("div", {}, statusPill(r.status))),
      h("div", { class: "card stat" }, h("div", { class: "k" }, "النموذج"), h("div", { class: "v mono", style: "font-size:18px" }, r.model)),
      h("div", { class: "card stat" }, h("div", { class: "k" }, "الخطوات / الأفعال"), h("div", { class: "v" }, `${rep.steps ?? "…"} / ${rep.actions ?? "…"}`)),
      h("div", { class: "card stat" }, h("div", { class: "k" }, "المدة"), h("div", { class: "v" }, rep.seconds != null ? `${rep.seconds}s` : "…"))),
    rep.final_text ? h("div", { class: "note info" }, rep.final_text) : null,
    rep.error ? h("div", { class: "note" }, rep.error) : null,
    h("div", { class: "card" }, h("h2", {}, "الخطوات"), steps.length ? h("ul", { class: "steps" }, steps) : h("p", { class: "muted" }, "بانتظار أول خطوة…")));
  if (r.status === "running") timers.push(setInterval(() => { if (!document.hidden) route(true); }, 3000));
}

async function approvals() {
  const list = await api("/api/approvals");
  const answer = async (id, allow) => { try { await api(`/api/approvals/${id}`, { method: "POST", body: { allow } }); toast(allow ? "تمت الموافقة" : "تم الرفض"); route(); } catch (e) { toast("خطأ: " + e.message); } };
  render(h("h1", {}, "الموافقات"), h("p", { class: "sub" }, "أفعال حساسة طلبها الوكيل وتنتظر قرارك (تُرفض تلقائياً بعد 10 دقائق)."),
    list.length ? h("div", { class: "grid" }, list.map((a) => h("div", { class: "card" },
      h("div", { class: "row spread" }, h("div", {}, h("b", {}, ACTION_AR[a.action] || a.action), " ", h("span", { class: "mono muted" }, JSON.stringify(a.input))),
        h("div", { class: "row" }, h("button", { class: "btn primary", onclick: () => answer(a.id, true) }, "موافقة"), h("button", { class: "btn danger", onclick: () => answer(a.id, false) }, "رفض"))),
      h("p", { class: "muted", style: "margin:6px 0 0" }, "السبب: ", a.reason, " · ", h("a", { href: `#/run/${a.run_id}` }, "المهمة")))))
      : h("div", { class: "card" }, h("p", { class: "muted" }, "لا توجد موافقات معلّقة.")));
  timers.push(setInterval(() => { if (!document.hidden) route(true); }, 3000));
}

async function models() {
  const [list, conf] = await Promise.all([api("/api/models"), api("/api/config")]);
  const ta = h("textarea", { class: "code", spellcheck: "false" }); ta.value = conf.text;
  const save = async () => { try { await api("/api/config", { method: "PUT", body: { text: ta.value } }); toast("حُفظت الإعدادات"); } catch (e) { toast("خطأ: " + e.message); } };
  const tryRoute = h("input", { placeholder: "جرّب prompt: @local افتح المفكرة", style: "flex:1" });
  const out = h("span", { class: "muted" });
  tryRoute.addEventListener("keydown", async (e) => { if (e.key !== "Enter") return;
    try { const d = await api(`/api/route?prompt=${encodeURIComponent(tryRoute.value)}`); out.textContent = `← ${d.model} (${d.reason})`; } catch (err) { out.textContent = err.message; } });
  render(h("h1", {}, "النماذج والإعدادات"),
    h("p", { class: "sub" }, "المفاتيح لا تُكتب هنا: تُقرأ من متغيرات البيئة على الخادم (مثل DEEPSEEK_API_KEY)."),
    h("div", { class: "card", style: "margin-bottom:16px" }, h("h2", {}, "النماذج المعرّفة"), h("div", { class: "tbl-wrap" }, h("table", {},
      h("thead", {}, h("tr", {}, ["الاسم", "المزوّد", "الموديل", "العنوان", "المفتاح"].map((t) => h("th", {}, t)))),
      h("tbody", {}, list.map((m) => h("tr", {}, h("td", {}, h("b", {}, m.name)), h("td", {}, m.provider), h("td", { class: "mono" }, m.model), h("td", { class: "mono" }, m.base_url || "—"),
        h("td", {}, m.key_present === null ? "—" : m.key_present ? h("span", { class: "pill ok" }, "موجود") : h("span", { class: "pill bad" }, m.key_env))))))),
      h("div", { class: "row", style: "margin-top:12px" }, tryRoute, out)),
    h("div", { class: "card" }, h("div", { class: "row spread" }, h("h2", {}, "ملف الإعدادات ", h("span", { class: "mono muted" }, conf.path)), h("button", { class: "btn primary", onclick: save }, "حفظ")), ta));
}

// ---------- router ----------------------------------------------------------------------------
async function updateBadge() {
  try { const n = (await api("/api/approvals")).length; const b = $("#badge"); b.hidden = !n; b.textContent = n; } catch (e) {}
}
async function route(soft) {
  if (!soft) clearTimers();
  const hash = location.hash.replace(/^#/, "") || "/";
  const [, page, arg] = hash.split("/");
  const key = page || "overview";
  document.querySelectorAll("#nav a").forEach((a) => a.classList.toggle("on", a.dataset.v === ({ vm: "vms", run: "runs" }[key] || key)));
  try {
    if (soft) clearTimers();
    if (key === "overview") await overview();
    else if (key === "vms") await vms();
    else if (key === "vm") await vmDetail(decodeURIComponent(arg));
    else if (key === "runs") await runs();
    else if (key === "run") await runDetail(decodeURIComponent(arg));
    else if (key === "approvals") await approvals();
    else if (key === "models") await models();
    else render(h("h1", {}, "غير موجود"));
  } catch (err) {
    if ($("#app").hidden) return;
    render(h("h1", {}, "خطأ"), h("p", { class: "err" }, err.message));
  }
  if (!soft) view.focus();
}
window.addEventListener("hashchange", () => route());
let badgeTimer;
async function boot() {
  try { await api("/api/overview"); } catch (e) { return; }
  $("#login").hidden = true; $("#app").hidden = false;
  route(); updateBadge(); clearInterval(badgeTimer); badgeTimer = setInterval(updateBadge, 5000);
}
boot();
