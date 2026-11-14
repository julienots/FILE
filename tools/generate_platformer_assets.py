#!/usr/bin/env python3
"""Generates the original sprites of the complete 2D platformer template (AGR ENGINE): spring,
spiky hedgehog, jumping fish, cannon, moving platform, crumbling block, crystal slot.
Drawn here (4x supersampled, then downsampled); deterministic. Writes assets/games/ only.

    python3 tools/generate_platformer_assets.py      # requires Pillow
"""
import os

from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "editor", "resources", "templates", "assets", "games")
S = 4
OUTLINE = (34, 30, 44, 255)


def cv(w, h=None):
    h = h or w
    img = Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))
    return img, ImageDraw.Draw(img)


def put(img, name):
    w, h = img.size
    img.resize((w // S, h // S), Image.LANCZOS).save(os.path.join(OUT, name + ".png"), optimize=True)


def sh(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c[:3]) + (c[3],)


def el(d, cx, cy, rx, ry, fill, outline=None, width=0):
    d.ellipse([(cx - rx) * S, (cy - ry) * S, (cx + rx) * S, (cy + ry) * S], fill=fill, outline=outline, width=width * S)


def rc(d, x0, y0, x1, y1, fill, r=0, outline=None, width=0):
    d.rounded_rectangle([x0 * S, y0 * S, x1 * S, y1 * S], radius=r * S, fill=fill, outline=outline, width=width * S)


def pg(d, pts, fill, outline=None, width=0):
    d.polygon([(x * S, y * S) for x, y in pts], fill=fill, outline=outline, width=width * S)


def ln(d, pts, fill, width):
    d.line([(x * S, y * S) for x, y in pts], fill=fill, width=width * S, joint="curve")


def eyes(d, cx, cy, gap, r):
    for sx in (-1, 1):
        el(d, cx + sx * gap, cy, r, r * 1.15, (255, 255, 255, 255))
        el(d, cx + sx * gap + r * 0.3, cy + r * 0.2, r * 0.55, r * 0.6, (30, 30, 40, 255))


def spring():
    img, d = cv(64)
    rc(d, 6, 52, 58, 62, (90, 90, 100, 255), r=3, outline=OUTLINE, width=2)
    for i in range(4):
        y = 48 - i * 8
        ln(d, [(14, y), (50, y - 4)], (200, 200, 210, 255), 4)
    rc(d, 4, 10, 60, 20, (230, 70, 60, 255), r=4, outline=OUTLINE, width=2)
    rc(d, 10, 12, 30, 15, (255, 160, 150, 255), r=2)
    put(img, "spring")


def spiky():
    img, d = cv(128)
    body = (150, 100, 70, 255)
    for i in range(9):
        x = 20 + i * 11
        pg(d, [(x - 8, 70), (x, 22 + (i % 2) * 10), (x + 8, 70)], (90, 70, 60, 255), OUTLINE, 2)
    el(d, 64, 82, 48, 34, body, OUTLINE, 3)
    el(d, 98, 86, 16, 14, (230, 190, 150, 255), OUTLINE, 2)
    el(d, 112, 86, 4, 4, OUTLINE)
    eyes(d, 92, 76, 6, 6)
    rc(d, 34, 108, 50, 118, sh(body, 0.7), r=4)
    rc(d, 76, 108, 92, 118, sh(body, 0.7), r=4)
    put(img, "spiky")


def fish():
    img, d = cv(96)
    c = (80, 170, 230, 255)
    pg(d, [(10, 30), (26, 48), (10, 66)], sh(c, 0.8), OUTLINE, 2)
    el(d, 54, 48, 34, 22, c, OUTLINE, 3)
    pg(d, [(44, 28), (60, 14), (66, 30)], sh(c, 0.8), OUTLINE, 2)
    el(d, 72, 42, 6, 6, (255, 255, 255, 255), OUTLINE, 1)
    el(d, 74, 43, 3, 3, OUTLINE)
    ln(d, [(80, 56), (86, 54)], OUTLINE, 2)
    for x in (38, 48, 58):
        ln(d, [(x, 40), (x + 4, 48), (x, 56)], sh(c, 1.25), 2)
    put(img, "fish")


def cannon():
    img, d = cv(96)
    rc(d, 26, 30, 94, 62, (70, 70, 80, 255), r=10, outline=OUTLINE, width=3)
    rc(d, 84, 26, 96, 66, (50, 50, 60, 255), r=4, outline=OUTLINE, width=2)
    el(d, 36, 66, 22, 22, (140, 90, 50, 255), OUTLINE, 3)
    el(d, 36, 66, 8, 8, (90, 60, 30, 255))
    rc(d, 10, 80, 64, 92, (110, 75, 45, 255), r=3, outline=OUTLINE, width=2)
    put(img, "cannon")


def platform():
    img, d = cv(192, 48)
    rc(d, 2, 6, 190, 30, (160, 110, 60, 255), r=6, outline=OUTLINE, width=3)
    for x in (48, 96, 144):
        ln(d, [(x, 8), (x, 28)], (110, 75, 40, 255), 3)
    rc(d, 20, 30, 34, 44, (120, 120, 130, 255), r=2, outline=OUTLINE, width=2)
    rc(d, 158, 30, 172, 44, (120, 120, 130, 255), r=2, outline=OUTLINE, width=2)
    put(img, "platform")


def crumble():
    img, d = cv(64)
    rc(d, 2, 2, 62, 62, (150, 140, 130, 255), r=4, outline=OUTLINE, width=3)
    ln(d, [(10, 14), (26, 30), (22, 46), (34, 58)], (90, 80, 75, 255), 3)
    ln(d, [(46, 6), (40, 24), (54, 40)], (90, 80, 75, 255), 3)
    rc(d, 6, 6, 58, 12, (190, 180, 170, 255), r=3)
    put(img, "crumble")


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    spring()
    spiky()
    fish()
    cannon()
    platform()
    crumble()
    print("platformer sprites written")
