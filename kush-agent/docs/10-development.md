# 10 — التطوير والاختبارات

## التجهيز
```bash
sudo apt install qemu-system-x86 qemu-utils gcc-mingw-w64-x86-64
pip install -e ".[dev]"
```

## الاختبارات
```bash
python -m pytest -q                 # كل الاختبارات (تحتاج qemu-system-x86_64 للتكامل)
python -m pytest -q -m "not integration"   # بدون QEMU
make -C guest/kushagent test        # بروتوكول kushagent
```
| الملف | ماذا يختبر |
|---|---|
| `tests/test_vm_unit.py` | المفاتيح، تحويل الإحداثيات، حدود الصور، اكتشاف التسريع |
| `tests/test_qemu_integration.py` | **QEMU حقيقي**: صورة الشاشة وكل أنواع الإدخال |
| `tests/test_cli_server_integration.py` | **QEMU حقيقي**: أوامر CLI وواجهة REST والتوكن والسياسة |
| `tests/test_router_policy.py` | الموجّه (عربي، @alias، fallback) والسياسة |
| `tests/test_adapters_loop.py` | محوّل Claude (شكل toolset، toolset_name، الدفعات، الرفض) ومحوّل OpenAI والحلقة |
| `tests/test_guest_client.py` | عميل kushagent وكتابة العربي عبره |

الاختبارات الحقيقية تقلع QEMU بدون قرص (شاشة SeaBIOS) بـ TCG، فتعمل في أي بيئة.

## التشغيل على ويندوز (L440 مع VT-x)
1. BIOS: فعّل Intel Virtualization Technology.
2. "Turn Windows features on or off" ← **Windows Hypervisor Platform** ← إعادة تشغيل.
3. ثبّت QEMU لويندوز وأضفه للـ PATH، وPython 3.10+.
4. `pip install -e .` ثم `kush-agent accel` (يجب أن يظهر `whpx`).
5. نفس أوامر `vm start` و`run`. القنوات على `127.0.0.1:47000-47002`.

## مجلد الحالة
`~/.kush-agent/vms/<name>/` (أو `$KUSH_STATE_DIR/vms/<name>/`): `vm.json`، `qemu.pid`، `qemu.log`، والـ sockets.
