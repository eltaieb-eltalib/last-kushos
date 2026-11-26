import pytest
from PIL import Image

from kush_agent.vm import accel, keys
from kush_agent.vm.screen import ABS_MAX, ScreenGeometry, fit_size, process, visual_tokens


def test_parse_combo():
    assert keys.parse_combo("ctrl+alt+Delete") == ["ctrl", "alt", "delete"]
    assert keys.parse_combo("Return") == ["ret"]
    assert keys.parse_combo("super+r") == ["meta_l", "r"]
    assert keys.parse_combo("+") == ["kp_add"] or keys.parse_combo("+") == ["equal"]


def test_text_events_shift():
    batches = keys.text_events("Hi!")
    first = [e["data"]["key"]["data"] for e in batches[0]]
    assert first == ["shift", "h", "h", "shift"]
    assert [e["data"]["down"] for e in batches[0]] == [True, True, False, False]
    assert batches[2][1]["data"]["key"]["data"] == "1"


def test_arabic_not_typeable():
    assert not keys.is_typeable("مرحبا")
    with pytest.raises(keys.KeyError_):
        keys.text_events("مرحبا")


def test_unknown_key():
    with pytest.raises(keys.KeyError_):
        keys.parse_combo("nosuchkey")


def test_fit_size_limits():
    w, h = fit_size(3840, 2160)
    assert max(w, h) <= 1568 and visual_tokens(w, h) <= 4784
    assert fit_size(1024, 768) == (1024, 768)


def test_geometry_mapping():
    g = ScreenGeometry(real_w=2048, real_h=1536, shown_w=1024, shown_h=768)
    assert g.to_real(512, 384) == (1024, 768)
    assert g.to_abs(0, 0) == (0, 0)
    assert g.to_abs(1024, 768) == (ABS_MAX, ABS_MAX)


def test_process_zoom_keeps_frame(tmp_path):
    p = tmp_path / "s.png"
    Image.new("RGB", (800, 600), "white").save(p)
    full = process(str(p))
    assert (full.geometry.shown_w, full.geometry.shown_h) == (800, 600)
    z = process(str(p), region=(0, 0, 100, 100))
    assert z.geometry.real_w == 800


def test_accel_no_kvm(tmp_path):
    cpu = tmp_path / "cpuinfo"
    cpu.write_text("flags\t\t: fpu sse2\n")
    a = accel.detect(system="Linux", kvm_path=str(tmp_path / "nokvm"), cpuinfo_path=str(cpu))
    assert a.name == "tcg" and not a.hardware
    assert accel.detect(system="Windows").name == "whpx"
    assert "whpx" in accel.detect(system="Windows").qemu_args()


def test_accel_forced(monkeypatch):
    monkeypatch.setenv("KUSH_ACCEL", "kvm")
    assert accel.detect().name == "kvm"
