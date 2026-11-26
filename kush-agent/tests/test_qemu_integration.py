"""Boots a real QEMU (no disk: SeaBIOS / 'no bootable device' screen) and drives it over QMP."""
import shutil

import pytest

from kush_agent.tools.computer import Computer
from kush_agent.vm.qemu import VM, VMConfig

pytestmark = pytest.mark.integration


@pytest.fixture
def vm(tmp_path, monkeypatch):
    if not shutil.which("qemu-system-x86_64"):
        pytest.skip("qemu-system-x86_64 not installed")
    monkeypatch.setenv("KUSH_STATE_DIR", str(tmp_path / "state"))
    v = VM(VMConfig(name="itest", memory_mb=128, cpus=1, nic="none"))
    v.start()
    yield v
    v.stop()
    assert not v.is_running()


def test_screenshot_and_input(vm):
    with vm.qmp() as q:
        assert q.status()["status"] in ("running", "prelaunch")
        comp = Computer(q, settle=0)
        r = comp.run("screenshot", {})
        assert r.image and r.image.png.startswith(b"\x89PNG")
        assert r.image.geometry.real_w >= 640
        for name, inp in [
            ("left_click", {"coordinate": [100, 100]}),
            ("double_click", {"coordinate": [10, 10], "text": "shift"}),
            ("scroll", {"scroll_direction": "down", "scroll_amount": 2}),
            ("key", {"text": "ctrl+alt+Delete"}),
            ("type", {"text": "Hello, Kush!"}),
            ("left_click_drag", {"start_coordinate": [5, 5], "coordinate": [50, 50]}),
            ("zoom", {"region": [0, 0, 200, 100]}),
        ]:
            res = comp.run(name, inp)
            assert not res.is_error, (name, res.text)
        assert comp.run("cursor_position", {}).text == "X=50, Y=50"
        bad = comp.run("type", {"text": "مرحبا"})
        assert bad.is_error
