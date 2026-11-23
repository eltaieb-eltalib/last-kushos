#!/usr/bin/env python3
"""Kush 1.1 (phase 6): build .ico files from Fluent UI System Icons (MIT).

Reads SVGs from an unpacked @fluentui/svg-icons package and writes
multi-size .ico files (16/20/24/32/48/256). Glyphs are recolored
deliberately: Fluent SVG paths carry no fill attribute, so the color is
injected into the first <path> element; a missing injection is an error,
never a silent black icon.

Usage:
    python3 build_fluent_icons.py <fluent-package-dir> <out-dir> [--theme light|dark]

The icon mapping lives in ICONS below; shell32/index-preserving wiring
consumes the generated files in a later step of phase 6.
"""
import io, os, re, sys

import cairosvg
from PIL import Image

SIZES = [(16, 16), (20, 20), (24, 24), (32, 32), (48, 48), (256, 256)]

# Glyph color per theme: dark glyphs on light chrome, light on dark.
THEME_COLORS = {"light": "#1B1B1B", "dark": "#F3F3F3"}
ACCENT = "#0D7377"  # Kush teal

# name -> (svg file, color role). Roles: 'accent' or 'theme'.
ICONS = {
    "kush-start":   ("apps_24_regular.svg",           "accent"),
    "kush-search":  ("search_24_regular.svg",         "theme"),
    "kush-power":   ("power_24_regular.svg",          "theme"),
    "kush-cmd":     ("window_console_20_regular.svg", "theme"),
    "kush-notepad": ("notebook_24_regular.svg",       "theme"),
    "kush-wordpad": ("document_text_24_regular.svg",  "theme"),
    "kush-paint":   ("paint_brush_24_regular.svg",    "theme"),
    "kush-calc":    ("calculator_24_regular.svg",     "theme"),
    "kush-files":   ("folder_24_regular.svg",         "theme"),
}


def colorize(svg_text, color):
    if "fill=" in svg_text:
        out, n = re.subn(r'fill="[^"]*"', 'fill="%s"' % color, svg_text, count=1)
    else:
        out, n = re.subn(r"<path ", '<path fill="%s" ' % color, svg_text, count=1)
    if n != 1:
        raise SystemExit("color injection failed for an icon; refusing a silent black glyph")
    return out


def main():
    pkg, outdir = sys.argv[1], sys.argv[2]
    theme = "light"
    if "--theme" in sys.argv:
        theme = sys.argv[sys.argv.index("--theme") + 1]
    theme_color = THEME_COLORS[theme]
    os.makedirs(outdir, exist_ok=True)
    for name, (svg, role) in ICONS.items():
        color = ACCENT if role == "accent" else theme_color
        src = open(os.path.join(pkg, "icons", svg)).read()
        src = colorize(src, color)
        png = cairosvg.svg2png(bytestring=src.encode(), output_width=256, output_height=256)
        img = Image.open(io.BytesIO(png)).convert("RGBA")
        img.save(os.path.join(outdir, "%s.ico" % name), sizes=SIZES, bitmap_format="bmp")
        print("wrote %s.ico (%s, %s)" % (name, svg, color))


if __name__ == "__main__":
    main()
