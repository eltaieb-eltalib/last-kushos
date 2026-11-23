#!/usr/bin/env python3
"""Kush 1.1 (phase 6): original, colorful, script-generated shell icon set.

No third-party art: every icon is drawn here with PIL primitives
(rounded rects, gradients, polygons) at 4x supersample and downscaled
with LANCZOS, so provenance is the script itself. Multi-size ICO output
uses BMP entries (ReactOS's icon loader rejects PNG-compressed entries).

Usage:
    python3 build_shell32_icons.py <out-dir> [--sheet <preview.png>]
"""
import math, os, sys
from PIL import Image, ImageDraw, ImageFont

S = 256
SS = 4          # supersample factor
W = S * SS
SIZES = [(16, 16), (24, 24), (32, 32), (48, 48), (256, 256)]
FONT_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

TEAL = (13, 115, 119)
TEAL_HI = (32, 178, 170)


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def vgrad(w, h, ctop, cbot):
    img = Image.new("RGB", (1, h))
    img.putdata([lerp(ctop, cbot, y / max(1, h - 1)) for y in range(h)])
    return img.resize((w, h))


def rrect_mask(w, h, r):
    m = Image.new("L", (w, h), 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, w - 1, h - 1], radius=r, fill=255)
    return m


def grad_rrect(img, box, radius, ctop, cbot):
    """Paste a vertical-gradient rounded rect into img at box (canvas coords)."""
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    g = vgrad(w, h, ctop, cbot).convert("RGBA")
    img.paste(g, (x0, y0), rrect_mask(w, h, radius))


def new_canvas():
    return Image.new("RGBA", (W, W), (0, 0, 0, 0))


def finish(img):
    return img.resize((S, S), Image.LANCZOS)


def sc(v):
    """scale a 256-space coordinate to canvas space"""
    return int(round(v * SS))


def box(x0, y0, x1, y1):
    return [sc(x0), sc(y0), sc(x1), sc(y1)]


# ---------------------------------------------------------------- icons

def icon_folder():
    img = new_canvas()
    # back: darker amber with raised tab
    d = ImageDraw.Draw(img)
    grad_rrect(img, box(28, 52, 228, 196), sc(18), (219, 148, 32), (197, 122, 14))
    d.rectangle(box(28, 84, 228, 120), fill=(0, 0, 0, 0))  # carve handled by front
    # front: light gold gradient
    grad_rrect(img, box(28, 76, 228, 204), sc(16), (255, 224, 130), (255, 183, 43))
    d = ImageDraw.Draw(img)
    d.line(box(28, 84, 228, 84), fill=(255, 240, 190, 255), width=sc(3))
    return finish(img)


def icon_folder_open():
    img = new_canvas()
    grad_rrect(img, box(28, 52, 228, 196), sc(18), (219, 148, 32), (197, 122, 14))
    d = ImageDraw.Draw(img)
    # white paper peeking
    d.rounded_rectangle(box(70, 60, 196, 130), radius=sc(6), fill=(252, 252, 252, 255))
    # tilted front trapezoid
    front = vgrad(W, W, (255, 224, 130), (255, 183, 43)).convert("RGBA")
    mask = Image.new("L", (W, W), 0)
    ImageDraw.Draw(mask).polygon(
        [(sc(20), sc(206)), (sc(52), sc(96)), (sc(236), sc(96)), (sc(204), sc(206))],
        fill=255)
    img.paste(front, (0, 0), mask)
    return finish(img)


def icon_computer():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    # frame
    grad_rrect(img, box(28, 48, 228, 176), sc(12), (66, 70, 78), (38, 40, 45))
    # screen
    grad_rrect(img, box(38, 58, 218, 166), sc(7), (14, 116, 144), (34, 211, 190))
    # stand + base
    d.rounded_rectangle(box(112, 178, 144, 200), radius=sc(3), fill=(70, 74, 82, 255))
    d.rounded_rectangle(box(84, 200, 172, 210), radius=sc(5), fill=(70, 74, 82, 255))
    return finish(img)


def icon_drive():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    grad_rrect(img, box(36, 76, 220, 180), sc(16), (113, 120, 130), (60, 64, 72))
    d.rounded_rectangle(box(52, 148, 204, 162), radius=sc(6), fill=(31, 33, 38, 255))
    d.rounded_rectangle(box(52, 148, 120, 162), radius=sc(6), fill=TEAL + (255,))
    d.ellipse(box(186, 96, 202, 112), fill=(74, 222, 128, 255))
    return finish(img)


def icon_disc():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    g = vgrad(W, W, (240, 243, 247), (176, 184, 196)).convert("RGBA")
    m = Image.new("L", (W, W), 0)
    ImageDraw.Draw(m).ellipse(box(28, 28, 228, 228), fill=255)
    img.paste(g, (0, 0), m)
    # subtle color arcs
    for i, col in enumerate([(255, 120, 120, 90), (120, 200, 255, 90), (150, 255, 170, 90)]):
        d.arc(box(44 + i * 6, 44 + i * 6, 212 - i * 6, 212 - i * 6),
              start=200 + i * 40, end=320 + i * 40, fill=col, width=sc(10))
    d.ellipse(box(104, 104, 152, 152), fill=(230, 233, 238, 255))
    d.ellipse(box(116, 116, 140, 140), fill=(0, 0, 0, 0))
    return finish(img)


def icon_document():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    grad_rrect(img, box(64, 32, 192, 224), sc(8), (255, 255, 255), (228, 232, 238))
    # folded corner
    d.polygon([(sc(160), sc(32)), (sc(192), sc(64)), (sc(160), sc(64))],
              fill=(198, 205, 216, 255))
    for i in range(4):
        y = 88 + i * 26
        w = 100 if i < 3 else 64
        d.rounded_rectangle(box(84, y, 84 + w, y + 9), radius=sc(4),
                            fill=(59, 130, 246, 255))
    return finish(img)


def icon_recycle_bin():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    # rim
    d.rounded_rectangle(box(56, 62, 200, 82), radius=sc(8), fill=(148, 157, 170, 255))
    d.rounded_rectangle(box(104, 50, 152, 66), radius=sc(6), fill=(148, 157, 170, 255))
    # body trapezoid
    body = vgrad(sc(160), sc(140), (226, 230, 236), (170, 178, 190)).convert("RGBA")
    mask = Image.new("L", (sc(160), sc(140)), 0)
    ImageDraw.Draw(mask).polygon(
        [(sc(4), 0), (sc(156), 0), (sc(140), sc(138)), (sc(20), sc(138))], fill=255)
    img.paste(body, (sc(48), sc(84)), mask)
    # paper sticking out
    d.rounded_rectangle(box(96, 40, 150, 64), radius=sc(4), fill=(252, 252, 252, 255))
    # teal recycle hint: two arrows as arcs
    d.arc(box(88, 110, 168, 190), start=300, end=80, fill=TEAL + (255,), width=sc(8))
    return finish(img)


def icon_control_panel():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    grad_rrect(img, box(36, 44, 220, 212), sc(24), (250, 251, 253), (214, 219, 227))
    knobs = [(0.72, (59, 130, 246)), (0.38, (245, 158, 11)), (0.58, (239, 68, 68))]
    for i, (pos, col) in enumerate(knobs):
        y = 88 + i * 44
        d.rounded_rectangle(box(64, y - 4, 192, y + 4), radius=sc(4),
                            fill=(203, 209, 218, 255))
        cx = 64 + int((192 - 64) * pos)
        d.ellipse(box(cx - 13, y - 13, cx + 13, y + 13), fill=col + (255,))
    return finish(img)


def icon_globe():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    g = vgrad(W, W, (96, 165, 250), (29, 78, 216)).convert("RGBA")
    m = Image.new("L", (W, W), 0)
    ImageDraw.Draw(m).ellipse(box(30, 30, 226, 226), fill=255)
    img.paste(g, (0, 0), m)
    line = (219, 234, 254, 255)
    d.ellipse(box(84, 30, 172, 226), outline=line, width=sc(6))
    d.line(box(30, 128, 226, 128), fill=line, width=sc(6))
    d.ellipse(box(30, 30, 226, 226), outline=line, width=sc(6))
    d.arc(box(30, 66, 226, 190), start=0, end=180, fill=line, width=sc(5))
    return finish(img)


def icon_search():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    # glass
    d.ellipse(box(36, 36, 176, 176), outline=TEAL + (255,), width=sc(26))
    d.ellipse(box(62, 62, 150, 150), fill=(219, 244, 244, 110))
    # handle
    d.line([(sc(162), sc(162)), (sc(224), sc(224))], fill=(55, 60, 70, 255), width=sc(26))
    d.ellipse(box(211, 211, 237, 237), fill=(55, 60, 70, 255))
    return finish(img)


def icon_help():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    g = vgrad(W, W, (96, 165, 250), (37, 99, 235)).convert("RGBA")
    m = Image.new("L", (W, W), 0)
    ImageDraw.Draw(m).ellipse(box(30, 30, 226, 226), fill=255)
    img.paste(g, (0, 0), m)
    f = ImageFont.truetype(FONT_BOLD, sc(150))
    d.text((sc(128), sc(122)), "?", font=f, fill=(255, 255, 255, 255), anchor="mm")
    return finish(img)


def icon_settings():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    cx, cy = W // 2, W // 2
    slate = (100, 108, 120, 255)
    # base disc
    d.ellipse([cx - sc(64), cy - sc(64), cx + sc(64), cy + sc(64)], fill=slate)
    # 8 teeth as thick spokes with rounded caps
    for i in range(8):
        ang = math.pi * i / 4 - math.pi / 2
        x0 = cx + int(sc(52) * math.cos(ang)); y0 = cy + int(sc(52) * math.sin(ang))
        x1 = cx + int(sc(94) * math.cos(ang)); y1 = cy + int(sc(94) * math.sin(ang))
        d.line([(x0, y0), (x1, y1)], fill=slate, width=sc(24))
        d.ellipse([x1 - sc(12), y1 - sc(12), x1 + sc(12), y1 + sc(12)], fill=slate)
    d.ellipse([cx - sc(34), cy - sc(34), cx + sc(34), cy + sc(34)], fill=(60, 64, 72, 255))
    d.ellipse([cx - sc(18), cy - sc(18), cx + sc(18), cy + sc(18)], fill=TEAL_HI + (255,))
    return finish(img)


def icon_photos():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    grad_rrect(img, box(32, 48, 224, 208), sc(14), (255, 255, 255), (222, 226, 233))
    # sky
    grad_rrect(img, box(44, 60, 212, 196), sc(10), (125, 211, 252), (56, 150, 216))
    d.ellipse(box(150, 74, 188, 112), fill=(255, 214, 90, 255))  # sun
    # mountains
    d.polygon([(sc(44), sc(196)), (sc(112), sc(118)), (sc(168), sc(196))],
              fill=(34, 160, 110, 255))
    d.polygon([(sc(110), sc(196)), (sc(162), sc(140)), (sc(212), sc(196))],
              fill=(22, 122, 84, 255))
    return finish(img)


def icon_music():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    grad_rrect(img, box(40, 40, 216, 216), sc(30), (192, 132, 252), (126, 34, 206))
    # note: stem + flag + head
    d.line([(sc(150), sc(70)), (sc(150), sc(165))], fill=(255, 255, 255, 255), width=sc(12))
    d.polygon([(sc(150), sc(70)), (sc(196), sc(88)), (sc(196), sc(112)), (sc(150), sc(96))],
              fill=(255, 255, 255, 255))
    d.ellipse(box(104, 148, 154, 186), fill=(255, 255, 255, 255))
    return finish(img)


def icon_downloads():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    grad_rrect(img, box(40, 40, 216, 216), sc(30), (240, 253, 244), (209, 236, 216))
    # arrow
    d.line([(sc(128), sc(64)), (sc(128), sc(140))], fill=(22, 163, 74, 255), width=sc(22))
    d.polygon([(sc(128), sc(158)), (sc(96), sc(120)), (sc(160), sc(120))],
              fill=(22, 163, 74, 255))
    # tray
    d.rounded_rectangle(box(70, 172, 186, 190), radius=sc(8), fill=TEAL + (255,))
    return finish(img)


def icon_run():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    g = vgrad(W, W, (250, 251, 253), (205, 212, 222)).convert("RGBA")
    m = Image.new("L", (W, W), 0)
    ImageDraw.Draw(m).ellipse(box(34, 34, 222, 222), fill=255)
    img.paste(g, (0, 0), m)
    d.polygon([(sc(104), sc(78)), (sc(104), sc(178)), (sc(188), sc(128))],
              fill=TEAL + (255,))
    return finish(img)


ICONS = {
    "sh-folder": icon_folder,
    "sh-folder-open": icon_folder_open,
    "sh-computer": icon_computer,
    "sh-drive": icon_drive,
    "sh-disc": icon_disc,
    "sh-document": icon_document,
    "sh-recycle-bin": icon_recycle_bin,
    "sh-control-panel": icon_control_panel,
    "sh-globe": icon_globe,
    "sh-search": icon_search,
    "sh-help": icon_help,
    "sh-settings": icon_settings,
    "sh-photos": icon_photos,
    "sh-music": icon_music,
    "sh-downloads": icon_downloads,
    "sh-run": icon_run,
}


def make_sheet(outdir, names):
    cols = 4
    cell = 132
    rows = (len(names) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * cell, rows * (cell + 18)), (243, 243, 243))
    d = ImageDraw.Draw(sheet)
    f = ImageFont.truetype(FONT_BOLD, 12)
    for i, name in enumerate(names):
        ico = Image.open(os.path.join(outdir, name + ".ico"))
        ico.size = (48, 48)
        im = ico.convert("RGBA").resize((48, 48), Image.LANCZOS)
        x, y = (i % cols) * cell, (i // cols) * (cell + 18)
        d.rectangle([x + 6, y + 6, x + cell - 6, y + cell - 6], fill=(255, 255, 255),
                    outline=(220, 220, 220))
        sheet.paste(im, (x + (cell - 48) // 2, y + (cell - 48) // 2 - 4), im)
        d.text((x + cell // 2, y + cell + 2), name, font=f, fill=(60, 60, 60), anchor="ma")
    return sheet


def main():
    outdir = sys.argv[1]
    os.makedirs(outdir, exist_ok=True)
    for name, fn in ICONS.items():
        img = fn()
        img.save(os.path.join(outdir, name + ".ico"), sizes=SIZES, bitmap_format="bmp")
        print("wrote %s.ico" % name)
    if "--sheet" in sys.argv:
        path = sys.argv[sys.argv.index("--sheet") + 1]
        make_sheet(outdir, list(ICONS)).save(path)
        print("wrote sheet", path)


if __name__ == "__main__":
    main()
