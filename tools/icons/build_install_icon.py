#!/usr/bin/env python3
"""Kush 1.1: generate the original installer icon (procedural, no external art).

Rounded gradient tile (Kush teal -> blue) with a white down-arrow into a tray -
the classic "install app" metaphor, Fluent-styled. Writes a multi-size .ico
with BMP entries (ReactOS cannot read PNG-compressed icon entries).
"""
import sys
from PIL import Image, ImageDraw

SIZES = [(16, 16), (20, 20), (24, 24), (32, 32), (48, 48), (256, 256)]
S = 256
SS = 4  # supersample

TEAL = (13, 115, 119)
BLUE = (26, 98, 228)


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def vgrad(w, h, ctop, cbot):
    img = Image.new("RGB", (1, h))
    for y in range(h):
        img.putpixel((0, y), lerp(ctop, cbot, y / max(h - 1, 1)))
    return img.resize((w, h))


def rounded_mask(w, h, r):
    m = Image.new("L", (w, h), 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, w - 1, h - 1], radius=r, fill=255)
    return m


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "kush-install.ico"
    W = S * SS
    tile = vgrad(W, W, (32, 178, 170), (26, 98, 228)).convert("RGBA")
    mask = rounded_mask(W, W, int(W * 0.18))
    img = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    img.paste(tile, (0, 0), mask)

    d = ImageDraw.Draw(img)
    white = (255, 255, 255, 255)
    cx = W // 2
    # arrow shaft
    shaft_w = int(W * 0.10)
    d.rounded_rectangle([cx - shaft_w // 2, int(W * 0.16), cx + shaft_w // 2, int(W * 0.50)],
                        radius=shaft_w // 2, fill=white)
    # arrow head
    hw = int(W * 0.20)
    d.regular_polygon((cx, int(W * 0.62), hw), 3, rotation=180, fill=white)
    # tray
    tray_w = int(W * 0.56)
    tray_h = int(W * 0.075)
    d.rounded_rectangle([cx - tray_w // 2, int(W * 0.74), cx + tray_w // 2, int(W * 0.74) + tray_h],
                        radius=tray_h // 2, fill=white)

    img = img.resize((S, S), Image.LANCZOS)
    img.save(out, sizes=SIZES, bitmap_format="bmp")
    print("wrote", out)


if __name__ == "__main__":
    main()
