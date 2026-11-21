#!/usr/bin/env python3
"""Kush 1.1: generate the original "Kush Flow" wallpaper (procedural, no external art).

Soft light background with layered, glossy blue ribbons that sweep around a
point right of centre. Usage: gen_wallpaper.py out.bmp [width height] [dark]
"""
import sys, math
import numpy as np
from PIL import Image, ImageFilter

out = sys.argv[1]
W = int(sys.argv[2]) if len(sys.argv) > 2 else 1920
H = int(sys.argv[3]) if len(sys.argv) > 3 else 1080
dark = len(sys.argv) > 4 and sys.argv[4] == 'dark'

y, x = np.mgrid[0:H, 0:W].astype(np.float32)
u, v = x / W, y / H

# Background: soft diagonal gradient.
if dark:
    c0, c1 = np.array([14, 18, 38]), np.array([30, 40, 78])
else:
    c0, c1 = np.array([214, 226, 246]), np.array([176, 196, 236])
t = np.clip(0.55 * u + 0.45 * v, 0, 1)[..., None]
img = c0 * (1 - t) + c1 * t

# Ribbons: bands of a swirl field around a centre, each with soft edges and a
# specular highlight along its crest, composited back-to-front.
cx, cy = 0.60 * W, 0.56 * H
dx, dy = (x - cx) / H, (y - cy) / H
r = np.sqrt(dx * dx + dy * dy) + 1e-6
a = np.arctan2(dy, dx)

palette_light = [(36, 86, 214), (20, 110, 235), (60, 140, 250), (14, 70, 190), (90, 165, 255), (26, 98, 228)]
palette_dark = [(40, 90, 220), (70, 120, 255), (30, 70, 200), (110, 150, 255), (50, 100, 240), (80, 60, 210)]
palette = palette_dark if dark else palette_light

rng = np.random.default_rng(7)

def smoothstep(e0, e1, z):
    t = np.clip((z - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)

# Petal ribbons: each layer is a closed curve r = R(theta) built only from
# integer harmonics of theta, so there is no seam at theta = +-pi.
layers = 7
for i in range(layers):
    col = np.array(palette[i % len(palette)], dtype=np.float32)
    base = 0.62 - i * 0.075                       # outer layers first (back to front)
    k = 3 if i % 2 == 0 else 2
    rot = i * 0.9
    R = base * (1 + 0.16 * np.sin(k * a + rot) + 0.06 * np.cos((k + 2) * a - rot))
    edge = 0.0035 + 0.0008 * i
    inside = 1 - smoothstep(R - edge, R + edge, r)         # filled petal
    # light from the upper left: brighter where the surface faces it
    nx, ny = np.cos(a), np.sin(a)
    facing = 0.5 + 0.5 * (-0.6 * nx - 0.8 * ny)
    depth = np.clip(r / np.maximum(R, 1e-3), 0, 1)          # 0 centre .. 1 rim
    shade = 0.78 + 0.32 * facing - 0.18 * depth ** 2
    rim = np.exp(-((r - R) ** 2) / (2 * (0.0045 + 0.0008 * i) ** 2))   # glossy rim light
    colour = col[None, None, :] * shade[..., None] + 255 * (0.55 * rim * facing)[..., None]
    colour = np.clip(colour, 0, 255)
    alpha = (inside * (0.88 - 0.05 * i))[..., None]
    # soft drop shadow of this layer onto what is behind it
    shadow = (1 - smoothstep(R - 0.01, R + 0.05, r)) * (1 - inside) * 0.22
    img = img * (1 - shadow[..., None])
    img = img * (1 - alpha) + colour * alpha

im = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), 'RGB')
im = im.filter(ImageFilter.GaussianBlur(0.6))
# subtle grain so gradients do not band on 16/24-bit displays
noise = rng.normal(0, 1.6, (H, W, 1)).astype(np.float32)
arr = np.clip(np.asarray(im).astype(np.float32) + noise, 0, 255).astype(np.uint8)
Image.fromarray(arr, 'RGB').save(out)
print('wrote', out, W, H)
