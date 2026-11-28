#!/usr/bin/env python3
"""Draws the two layers of the launcher icon: res/drawable-nodpi/ic_launcher_{background,foreground}.png.

The icon is an original picture (a striped sun over a neon grid, the look of the hub's Synthwave City
backdrop). Android lays the layers of an adaptive icon on a 108 x 108 dp square and shows only the middle 72 x 72 dp
(the launcher decides the mask: circle, squircle...), so the sun stays well inside that. Run it again after changing a
colour:  python3 android/make_icon.py   (needs Pillow; the PNGs are committed, the build does not run this).
"""
import os
import sys

from PIL import Image, ImageDraw

SIZE = 432  # 108 dp at xxxhdpi: one bitmap is enough, Android scales a bitmap layer to its square
SS = 3  # draw 3x bigger and shrink: smooth edges without anti-aliasing code
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "res", "drawable-nodpi")


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(len(a)))


def background():
    n = SIZE * SS
    img = Image.new("RGB", (n, n))
    px = ImageDraw.Draw(img)
    top, mid, bottom = (14, 6, 40), (62, 12, 96), (12, 4, 32)
    horizon = int(n * 0.60)
    for y in range(n):  # sky: night blue to violet at the horizon, then dark ground
        if y < horizon:
            c = lerp(top, mid, (y / horizon) ** 1.6)
        else:
            c = lerp(bottom, (30, 8, 64), 1 - (y - horizon) / (n - horizon))
        px.line([(0, y), (n, y)], fill=c)
    neon, glow = (0, 229, 255), (255, 61, 139)
    px.line([(0, horizon), (n, horizon)], fill=glow, width=3 * SS)
    # the ground grid: horizontal lines bunch up toward the horizon, the others fan out from a vanishing point
    for i in range(1, 9):
        y = horizon + int((n - horizon) * (i / 8) ** 2.2)
        px.line([(0, y), (n, y)], fill=neon, width=2 * SS)
    vx = n // 2
    for i in range(-10, 11):
        px.line([(vx, horizon), (vx + i * n // 7, n)], fill=neon, width=2 * SS)
    return img.resize((SIZE, SIZE), Image.LANCZOS)


def foreground():
    n = SIZE * SS
    sun = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    cx, cy, r = n // 2, int(n * 0.395), int(n * 0.205)
    gradient = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    gp = ImageDraw.Draw(gradient)
    for y in range(cy - r, cy + r + 1):  # yellow at the top, hot pink at the bottom
        t = (y - (cy - r)) / (2 * r)
        gp.line([(0, y), (n, y)], fill=lerp((255, 214, 77), (255, 45, 130), t) + (255,))
    mask = Image.new("L", (n, n), 0)
    mp = ImageDraw.Draw(mask)
    mp.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
    for i in range(5):  # slits cut out of the lower half, thicker toward the bottom
        y0 = cy + int(r * (0.12 + 0.17 * i))
        mp.rectangle([cx - r, y0, cx + r, y0 + int(r * (0.04 + 0.025 * i))], fill=0)
    sun.paste(gradient, (0, 0), mask)
    return sun.resize((SIZE, SIZE), Image.LANCZOS)


def main():
    os.makedirs(OUT, exist_ok=True)
    background().save(os.path.join(OUT, "ic_launcher_background.png"), optimize=True)
    foreground().save(os.path.join(OUT, "ic_launcher_foreground.png"), optimize=True)
    print("wrote", OUT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
