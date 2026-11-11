#!/usr/bin/env python3
"""Generates the original art of the game templates (AGR ENGINE): sprites, icons, card frame
and a top-down tileset. Everything is drawn procedurally here (no third-party assets).

Requires Pillow. Run from anywhere; writes editor/resources/templates/assets/games/.
Deterministic (fixed seed): re-running rewrites identical files.
"""
import json
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

random.seed(42)
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "editor", "resources", "templates", "assets", "games")
os.makedirs(OUT, exist_ok=True)
S = 4  # supersampling


def canvas(w, h=None):
    h = h or w
    img = Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))
    return img, ImageDraw.Draw(img)


def save(img, name):
    w, h = img.size
    img.resize((w // S, h // S), Image.LANCZOS).save(os.path.join(OUT, name + ".png"), optimize=True)


def shade(c, k, a=None):
    return tuple(max(0, min(255, int(v * k))) for v in c[:3]) + (a if a is not None else (c[3] if len(c) > 3 else 255),)


def ell(d, cx, cy, rx, ry, fill, outline=None, width=0):
    d.ellipse([(cx - rx) * S, (cy - ry) * S, (cx + rx) * S, (cy + ry) * S], fill=fill, outline=outline, width=width * S)


def rect(d, x0, y0, x1, y1, fill, r=0, outline=None, width=0):
    d.rounded_rectangle([x0 * S, y0 * S, x1 * S, y1 * S], radius=r * S, fill=fill, outline=outline, width=width * S)


def poly(d, pts, fill, outline=None, width=0):
    d.polygon([(x * S, y * S) for x, y in pts], fill=fill, outline=outline, width=width * S)


def line(d, pts, fill, width):
    d.line([(x * S, y * S) for x, y in pts], fill=fill, width=width * S, joint="curve")


def shadow(d, cx, cy, rx, ry):
    ell(d, cx, cy, rx, ry, (0, 0, 0, 70))


def eyes(d, cx, cy, gap, r, look=(0, 0)):
    for sx in (-1, 1):
        ell(d, cx + sx * gap, cy, r, r * 1.15, (255, 255, 255, 255))
        ell(d, cx + sx * gap + look[0] * r * 0.4, cy + look[1] * r * 0.4, r * 0.55, r * 0.6, (30, 30, 40, 255))


OUTLINE = (34, 30, 44, 255)

# ------------------------------------------------------------------ characters
def slime(name, body):
    img, d = canvas(128)
    shadow(d, 64, 116, 40, 8)
    d.chord([20 * S, 36 * S, 108 * S, 132 * S], 180, 360, fill=body, outline=OUTLINE, width=3 * S)
    rect(d, 20, 82, 108, 112, body, r=14)
    line(d, [(22, 84), (22, 104), (34, 112), (94, 112), (106, 104), (106, 84)], OUTLINE, 3)
    ell(d, 46, 58, 10, 6, shade(body, 1.35, 200))
    eyes(d, 64, 80, 15, 8, (0, 0.3))
    save(img, name)


slime("slime", (110, 200, 90, 255))
slime("creep", (220, 90, 90, 255))


def hero_top(name, tunic, hair):
    img, d = canvas(128)
    shadow(d, 64, 112, 30, 8)
    rect(d, 40, 60, 88, 108, tunic, r=14, outline=OUTLINE, width=3)
    rect(d, 30, 66, 44, 96, shade(tunic, 0.8), r=7, outline=OUTLINE, width=2)
    rect(d, 84, 66, 98, 96, shade(tunic, 0.8), r=7, outline=OUTLINE, width=2)
    ell(d, 64, 44, 26, 26, (246, 205, 170, 255), OUTLINE, 3)
    d.chord([38 * S, 16 * S, 90 * S, 60 * S], 180, 360, fill=hair, outline=OUTLINE, width=3 * S)
    eyes(d, 64, 46, 10, 5)
    rect(d, 46, 98, 82, 104, shade(tunic, 0.6), r=2)
    save(img, name)


hero_top("hero_top", (70, 120, 220, 255), (120, 72, 40, 255))
hero_top("npc", (160, 90, 190, 255), (230, 220, 210, 255))
hero_top("survivor", (90, 140, 80, 255), (60, 40, 30, 255))
hero_top("customer", (230, 150, 70, 255), (50, 40, 35, 255))


def zombie():
    img, d = canvas(128)
    shadow(d, 64, 112, 30, 8)
    skin = (140, 190, 120, 255)
    rect(d, 40, 60, 88, 108, (90, 80, 110, 255), r=12, outline=OUTLINE, width=3)
    rect(d, 22, 64, 46, 76, skin, r=6, outline=OUTLINE, width=2)
    rect(d, 82, 64, 106, 76, skin, r=6, outline=OUTLINE, width=2)
    ell(d, 64, 44, 26, 26, skin, OUTLINE, 3)
    ell(d, 54, 44, 6, 6, (250, 240, 120, 255))
    ell(d, 74, 44, 6, 6, (250, 240, 120, 255))
    line(d, [(52, 58), (58, 55), (64, 58), (70, 55), (76, 58)], OUTLINE, 2)
    save(img, "zombie")


zombie()


def bat():
    img, d = canvas(128)
    body = (90, 70, 120, 255)
    for sx in (-1, 1):
        poly(d, [(64, 58), (64 + sx * 58, 40), (64 + sx * 48, 62), (64 + sx * 54, 78), (64 + sx * 34, 70), (64 + sx * 20, 84)], shade(body, 0.85), OUTLINE, 3)
    ell(d, 64, 64, 22, 22, body, OUTLINE, 3)
    poly(d, [(48, 48), (52, 30), (58, 46)], body, OUTLINE, 2)
    poly(d, [(80, 48), (76, 30), (70, 46)], body, OUTLINE, 2)
    ell(d, 56, 62, 5, 5, (255, 90, 90, 255))
    ell(d, 72, 62, 5, 5, (255, 90, 90, 255))
    save(img, "bat")


bat()


def blob():  # multiplayer: white, tinted per player by the sprite color
    img, d = canvas(128)
    shadow(d, 64, 112, 34, 8)
    ell(d, 64, 66, 42, 42, (245, 245, 245, 255), OUTLINE, 4)
    ell(d, 48, 48, 12, 8, (255, 255, 255, 255))
    eyes(d, 64, 64, 14, 8, (0.5, 0))
    save(img, "blob")


blob()

# ------------------------------------------------------------------ items
def coin():
    img, d = canvas(64)
    ell(d, 32, 32, 26, 26, (240, 190, 40, 255), OUTLINE, 3)
    ell(d, 32, 32, 18, 18, (255, 215, 80, 255))
    rect(d, 28, 18, 36, 46, (230, 170, 30, 255), r=3)
    ell(d, 24, 22, 6, 4, (255, 245, 200, 220))
    save(img, "coin")


def heart():
    img, d = canvas(64)
    c = (230, 60, 80, 255)
    ell(d, 22, 24, 14, 14, c)
    ell(d, 42, 24, 14, 14, c)
    poly(d, [(9, 28), (55, 28), (32, 56)], c)
    ell(d, 20, 20, 5, 4, (255, 200, 210, 220))
    save(img, "heart")


def gem(name, c):
    img, d = canvas(64)
    poly(d, [(32, 4), (56, 24), (32, 60), (8, 24)], c, OUTLINE, 3)
    poly(d, [(32, 4), (44, 24), (32, 60), (20, 24)], shade(c, 1.25))
    poly(d, [(8, 24), (56, 24), (44, 24), (32, 4), (20, 24)], shade(c, 1.4))
    save(img, name)


coin()
heart()
gem("gem", (80, 200, 240, 255))
gem("crystal", (190, 110, 240, 255))


def flag():
    img, d = canvas(128)
    rect(d, 30, 8, 38, 124, (200, 200, 210, 255), r=3, outline=OUTLINE, width=2)
    ell(d, 34, 8, 7, 7, (250, 210, 60, 255), OUTLINE, 2)
    poly(d, [(38, 16), (108, 32), (38, 60)], (230, 70, 70, 255), OUTLINE, 3)
    poly(d, [(38, 16), (70, 24), (38, 38)], (250, 120, 110, 255))
    save(img, "flag")


flag()


def bullet():
    img, d = canvas(32)
    ell(d, 16, 16, 12, 12, (255, 230, 120, 120))
    ell(d, 16, 16, 7, 7, (255, 250, 220, 255))
    save(img, "bullet")


bullet()


def tower():
    img, d = canvas(128)
    shadow(d, 64, 114, 40, 10)
    rect(d, 28, 64, 100, 114, (130, 130, 150, 255), r=8, outline=OUTLINE, width=3)
    for x in range(34, 96, 16):
        rect(d, x, 72, x + 10, 80, (100, 100, 120, 255), r=2)
    ell(d, 64, 56, 30, 30, (70, 110, 200, 255), OUTLINE, 3)
    rect(d, 58, 6, 70, 52, (60, 60, 70, 255), r=4, outline=OUTLINE, width=3)
    ell(d, 54, 48, 8, 6, (150, 190, 250, 200))
    save(img, "tower")


tower()


def tree():
    img, d = canvas(128)
    shadow(d, 64, 116, 36, 9)
    rect(d, 56, 76, 72, 116, (120, 80, 50, 255), r=4, outline=OUTLINE, width=3)
    for (x, y, r, k) in [(64, 46, 36, 1.0), (40, 62, 24, 0.9), (88, 62, 24, 0.9), (64, 30, 24, 1.15)]:
        ell(d, x, y, r, r, shade((70, 150, 70, 255), k), OUTLINE, 3)
    ell(d, 56, 26, 8, 5, (160, 220, 140, 200))
    save(img, "tree")


tree()


def bush():
    img, d = canvas(128)
    shadow(d, 64, 110, 40, 9)
    for (x, y, r) in [(44, 78, 26), (84, 78, 26), (64, 62, 30)]:
        ell(d, x, y, r, r, (60, 140, 70, 255), OUTLINE, 3)
    for (x, y) in [(46, 66), (70, 52), (86, 74), (58, 86), (78, 90)]:
        ell(d, x, y, 7, 7, (220, 50, 80, 255), OUTLINE, 2)
    save(img, "bush")


bush()


def campfire(lit):
    img, d = canvas(128)
    for a in (-0.5, 0.5):
        cx, cy = 64, 100
        dx, dy = math.cos(a) * 40, math.sin(a) * 10
        line(d, [(cx - dx, cy - dy), (cx + dx, cy + dy)], (120, 80, 50, 255), 12)
    for x in range(30, 100, 14):
        ell(d, x, 108, 7, 6, (130, 130, 140, 255), OUTLINE, 2)
    if lit:
        poly(d, [(64, 20), (88, 70), (78, 98), (50, 98), (40, 70)], (250, 140, 40, 255))
        poly(d, [(64, 46), (78, 80), (70, 98), (58, 98), (50, 80)], (255, 220, 90, 255))
    save(img, "campfire" if lit else "campfire_out")


campfire(True)
campfire(False)


def log_item():
    img, d = canvas(64)
    rect(d, 6, 22, 58, 42, (140, 95, 55, 255), r=10, outline=OUTLINE, width=3)
    ell(d, 50, 32, 8, 10, (200, 160, 110, 255), OUTLINE, 2)
    save(img, "log")


log_item()


def crop(stage):
    img, d = canvas(128)
    stem = (70, 160, 60, 255)
    if stage == 0:
        for x in (48, 64, 80):
            ell(d, x, 96, 5, 4, (200, 170, 90, 255), OUTLINE, 2)
    else:
        h = [0, 34, 58, 72][stage]
        line(d, [(64, 112), (64, 112 - h)], stem, 6)
        for i in range(stage + 1):
            y = 104 - i * (h // (stage + 1))
            for sx in (-1, 1):
                poly(d, [(64, y), (64 + sx * 26, y - 14), (64 + sx * 10, y - 4)], shade(stem, 1.1), OUTLINE, 2)
        if stage == 3:
            for (x, y) in [(64, 34), (46, 52), (82, 52)]:
                ell(d, x, y, 13, 13, (240, 120, 40, 255), OUTLINE, 3)
                ell(d, x - 4, y - 4, 4, 3, (255, 210, 170, 220))
    save(img, f"crop_{stage}")


for s in range(4):
    crop(s)


def shop():
    img, d = canvas(256)
    shadow(d, 128, 236, 100, 14)
    rect(d, 36, 110, 220, 236, (210, 180, 140, 255), r=10, outline=OUTLINE, width=4)
    rect(d, 96, 150, 160, 236, (140, 90, 60, 255), r=6, outline=OUTLINE, width=3)
    rect(d, 48, 130, 86, 170, (150, 210, 240, 255), r=4, outline=OUTLINE, width=3)
    rect(d, 170, 130, 208, 170, (150, 210, 240, 255), r=4, outline=OUTLINE, width=3)
    for i in range(8):
        c = (220, 70, 70, 255) if i % 2 == 0 else (250, 245, 240, 255)
        poly(d, [(24 + i * 26, 76), (50 + i * 26, 76), (46 + i * 26, 120), (28 + i * 26, 120)], c, OUTLINE, 3)
    rect(d, 78, 36, 178, 76, (250, 220, 120, 255), r=10, outline=OUTLINE, width=4)
    ell(d, 128, 56, 12, 12, (140, 90, 60, 255), OUTLINE, 3)
    save(img, "shop")


shop()


def plot():
    img, d = canvas(128)
    rect(d, 8, 8, 120, 120, (150, 200, 120, 255), r=12, outline=(110, 160, 90, 255), width=4)
    for i in range(0, 4):
        line(d, [(24 + i * 26, 30), (24 + i * 26, 98)], (130, 180, 100, 255), 3)
    rect(d, 52, 52, 76, 76, (255, 255, 255, 160), r=4)
    line(d, [(64, 56), (64, 72)], (90, 140, 80, 255), 4)
    line(d, [(56, 64), (72, 64)], (90, 140, 80, 255), 4)
    save(img, "plot")


plot()

# ------------------------------------------------------------------ cards and icons
def card_frame():
    img, d = canvas(256, 360)
    rect(d, 6, 6, 250, 354, (250, 244, 228, 255), r=22, outline=OUTLINE, width=6)
    rect(d, 22, 22, 234, 210, (228, 220, 200, 255), r=14)
    rect(d, 22, 226, 234, 336, (240, 234, 216, 255), r=12)
    save(img, "card")


card_frame()


def icon(name, draw):
    img, d = canvas(128)
    draw(d)
    save(img, name)


icon("icon_sword", lambda d: (poly(d, [(64, 8), (76, 24), (76, 84), (52, 84), (52, 24)], (210, 215, 230, 255), OUTLINE, 3),
                              rect(d, 34, 84, 94, 96, (200, 160, 60, 255), r=4, outline=OUTLINE, width=3),
                              rect(d, 58, 96, 70, 120, (120, 80, 50, 255), r=3, outline=OUTLINE, width=3)))
icon("icon_shield", lambda d: (poly(d, [(64, 10), (108, 26), (102, 78), (64, 118), (26, 78), (20, 26)], (80, 130, 210, 255), OUTLINE, 4),
                               poly(d, [(64, 28), (88, 38), (84, 72), (64, 96)], (150, 190, 250, 255))))
icon("icon_potion", lambda d: (ell(d, 64, 80, 34, 34, (220, 60, 90, 255), OUTLINE, 4),
                               rect(d, 52, 14, 76, 50, (200, 230, 240, 255), r=4, outline=OUTLINE, width=3),
                               ell(d, 52, 70, 8, 6, (255, 190, 200, 220))))
icon("icon_bolt", lambda d: poly(d, [(72, 6), (30, 70), (60, 70), (48, 122), (98, 52), (66, 52)], (250, 210, 60, 255), OUTLINE, 4))
icon("icon_miner", lambda d: (ell(d, 64, 70, 34, 34, (190, 140, 90, 255), OUTLINE, 4), rect(d, 30, 30, 98, 46, (250, 200, 60, 255), r=8, outline=OUTLINE, width=3),
                              eyes(d, 64, 72, 12, 6)))
icon("icon_drill", lambda d: (poly(d, [(64, 120), (40, 50), (88, 50)], (170, 180, 200, 255), OUTLINE, 4),
                              rect(d, 36, 14, 92, 52, (230, 120, 50, 255), r=8, outline=OUTLINE, width=4)))
icon("icon_factory", lambda d: (rect(d, 14, 60, 114, 116, (150, 120, 110, 255), r=6, outline=OUTLINE, width=4),
                                rect(d, 80, 16, 100, 62, (120, 100, 95, 255), r=3, outline=OUTLINE, width=3),
                                poly(d, [(14, 60), (44, 40), (44, 60), (74, 40), (74, 60)], (170, 140, 130, 255), OUTLINE, 3)))

# ------------------------------------------------------------------ car (top view, racing minimap / UI)
def car_icon():
    img, d = canvas(128)
    rect(d, 34, 10, 94, 118, (220, 60, 60, 255), r=20, outline=OUTLINE, width=4)
    rect(d, 42, 34, 86, 60, (160, 210, 240, 255), r=8)
    for (x, y) in [(28, 30), (100, 30), (28, 96), (100, 96)]:
        rect(d, x - 6, y - 12, x + 6, y + 12, (40, 40, 45, 255), r=4)
    save(img, "car_icon")


car_icon()

# ------------------------------------------------------------------ top-down tileset (pixel art, 32 px)
T = 32
tiles = Image.new("RGBA", (8 * T, 2 * T), (0, 0, 0, 0))
td = ImageDraw.Draw(tiles)


def noise(ox, oy, base, var=0.1, dots=70):
    td.rectangle([ox, oy, ox + T - 1, oy + T - 1], fill=base + (255,))
    for _ in range(dots):
        x, y = ox + random.randrange(T - 1), oy + random.randrange(T - 1)
        td.rectangle([x, y, x + 1, y + 1], fill=shade(base, 1 + random.uniform(-var, var)))


grass, grass2, path, water, wall, sand, forest, floor = (96, 168, 78), (88, 158, 72), (176, 140, 92), (64, 130, 200), (120, 120, 132), (220, 200, 140), (60, 120, 60), (170, 120, 80)
# 0 grass, 1 grass with tufts, 2 path, 3 water, 4 stone wall, 5 sand, 6 forest floor, 7 wood floor
noise(0, 0, grass)
noise(T, 0, grass2)
for _ in range(6):
    x, y = T + random.randrange(4, 28), random.randrange(6, 28)
    td.line([x, y, x - 2, y - 4], fill=shade(grass2, 1.3))
    td.line([x, y, x + 2, y - 4], fill=shade(grass2, 1.3))
noise(2 * T, 0, path, dots=90)
noise(3 * T, 0, water, var=0.06, dots=30)
for i in range(3):
    td.line([3 * T + 4 + i * 9, 8 + i * 8, 3 * T + 12 + i * 9, 8 + i * 8], fill=shade(water, 1.3))
noise(4 * T, 0, wall, dots=40)
for i in range(4):
    y = i * 8 + 7
    td.line([4 * T, y, 5 * T - 1, y], fill=shade(wall, 0.7))
    xo = 4 * T + (6 if i % 2 else 18)
    td.line([xo, y - 7, xo, y], fill=shade(wall, 0.7))
noise(5 * T, 0, sand, dots=60)
noise(6 * T, 0, forest, dots=90)
noise(7 * T, 0, floor, dots=30)
for i in range(4):
    td.line([7 * T, i * 8 + 7, 8 * T - 1, i * 8 + 7], fill=shade(floor, 0.7))
# 8 flowers, 9 farmland, 10 wet farmland, 11 rock, 12 fence, 13 bridge, 14 cobble, 15 dark (void)
noise(0, T, grass)
for _ in range(7):
    x, y = random.randrange(3, 28), T + random.randrange(3, 28)
    td.rectangle([x, y, x + 2, y + 2], fill=random.choice([(250, 240, 120, 255), (250, 150, 180, 255), (255, 255, 255, 255)]))
noise(T, T, (130, 90, 60), dots=40)
for i in range(4):
    td.line([T, T + i * 8 + 3, 2 * T - 1, T + i * 8 + 3], fill=shade((130, 90, 60), 0.75))
noise(2 * T, T, (95, 65, 45), dots=40)
for i in range(4):
    td.line([2 * T, T + i * 8 + 3, 3 * T - 1, T + i * 8 + 3], fill=shade((95, 65, 45), 0.75))
noise(3 * T, T, grass)
td.ellipse([3 * T + 5, T + 7, 3 * T + 27, T + 27], fill=(140, 140, 150, 255), outline=(70, 70, 80, 255))
noise(4 * T, T, grass)
td.rectangle([4 * T, T + 10, 5 * T - 1, T + 13], fill=(160, 110, 70, 255))
td.rectangle([4 * T, T + 20, 5 * T - 1, T + 23], fill=(160, 110, 70, 255))
for x in (4 * T + 4, 4 * T + 26):
    td.rectangle([x, T + 6, x + 3, T + 28], fill=(130, 90, 55, 255))
noise(5 * T, T, water, var=0.05, dots=20)
td.rectangle([5 * T, T + 4, 6 * T - 1, T + 27], fill=(150, 104, 62, 255))
for i in range(4):
    td.line([5 * T + i * 8 + 4, T + 4, 5 * T + i * 8 + 4, T + 27], fill=(110, 76, 46, 255))
noise(6 * T, T, (150, 150, 155), dots=40)
for i in range(4):
    td.line([6 * T, T + i * 8 + 7, 7 * T - 1, T + i * 8 + 7], fill=(110, 110, 115, 255))
td.rectangle([7 * T, T, 8 * T - 1, 2 * T - 1], fill=(30, 34, 40, 255))
tiles.save(os.path.join(OUT, "topdown_tiles.png"), optimize=True)

frames = [{"name": f"tile_{i}", "x": (i % 8) * T, "y": (i // 8) * T, "w": T, "h": T} for i in range(16)]
sheet = {"format": "agr.spritesheet", "version": 1, "texture": "assets://games/topdown_tiles.png", "textureWidth": 8 * T,
         "textureHeight": 2 * T, "pixelsPerUnit": T, "pixelArt": True, "frames": frames, "clips": []}
with open(os.path.join(OUT, "topdown_tiles.agrsheet"), "w") as f:
    json.dump(sheet, f, indent=2)
    f.write("\n")

print("game template art written to", os.path.normpath(OUT))
