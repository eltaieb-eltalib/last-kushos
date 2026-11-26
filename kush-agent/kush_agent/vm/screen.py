"""Screenshot post-processing: fit images to the model's limits and map coordinates back.

The model sees a (possibly downscaled) screenshot and answers with coordinates in that image's
pixel space. `ScreenGeometry` converts those to real guest pixels, and to the 0..32767 absolute
range used by QEMU's usb-tablet.
"""

from __future__ import annotations

import base64
import io
import math
from dataclasses import dataclass

from PIL import Image

ABS_MAX = 0x7FFF  # QEMU absolute pointer range: 0..32767

# computer_toolset_20260801 limits on current models: long edge <= 2576 px and
# ceil(w/28)*ceil(h/28) <= 4784 visual tokens. We default lower to save tokens.
DEFAULT_MAX_LONG_EDGE = 1568
DEFAULT_MAX_TOKENS = 4784


def visual_tokens(w: int, h: int) -> int:
    return math.ceil(w / 28) * math.ceil(h / 28)


def fit_size(w: int, h: int, max_long_edge: int = DEFAULT_MAX_LONG_EDGE,
             max_tokens: int = DEFAULT_MAX_TOKENS) -> tuple[int, int]:
    scale = min(1.0, max_long_edge / max(w, h))
    nw, nh = max(1, int(w * scale)), max(1, int(h * scale))
    while visual_tokens(nw, nh) > max_tokens:
        scale *= 0.97
        nw, nh = max(1, int(w * scale)), max(1, int(h * scale))
    return nw, nh


@dataclass
class ScreenGeometry:
    real_w: int
    real_h: int
    shown_w: int
    shown_h: int

    def to_real(self, x: float, y: float) -> tuple[int, int]:
        rx = round(x * self.real_w / self.shown_w)
        ry = round(y * self.real_h / self.shown_h)
        return min(max(rx, 0), self.real_w - 1), min(max(ry, 0), self.real_h - 1)

    def to_abs(self, x: float, y: float) -> tuple[int, int]:
        """Model coordinates -> QEMU absolute axis values."""
        rx, ry = self.to_real(x, y)
        ax = round(rx * ABS_MAX / max(self.real_w - 1, 1))
        ay = round(ry * ABS_MAX / max(self.real_h - 1, 1))
        return ax, ay

    def to_shown(self, rx: float, ry: float) -> tuple[int, int]:
        return round(rx * self.shown_w / self.real_w), round(ry * self.shown_h / self.real_h)


@dataclass
class Screenshot:
    png: bytes
    geometry: ScreenGeometry

    def b64(self) -> str:
        return base64.b64encode(self.png).decode()


def process(raw_path: str, max_long_edge: int = DEFAULT_MAX_LONG_EDGE,
            region: tuple[int, int, int, int] | None = None) -> Screenshot:
    """Load a QEMU screendump (PNG or PPM), optionally crop a region (real pixels), and resize."""
    with Image.open(raw_path) as im:
        im = im.convert("RGB")
        real_w, real_h = im.size
        if region:
            im = im.crop(region)
        w, h = im.size
        nw, nh = fit_size(w, h, max_long_edge)
        if (nw, nh) != (w, h):
            im = im.resize((nw, nh), Image.LANCZOS)
        buf = io.BytesIO()
        im.save(buf, format="PNG", optimize=True)
    if region:
        # A zoom image does not change the coordinate frame; geometry stays that of the full screen.
        geo = ScreenGeometry(real_w, real_h, *fit_size(real_w, real_h, max_long_edge))
    else:
        geo = ScreenGeometry(real_w, real_h, nw, nh)
    return Screenshot(buf.getvalue(), geo)
