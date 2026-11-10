#!/usr/bin/env python3
"""Generates the original pixel-art used by the editor's 2D templates (AGR ENGINE).

All art is drawn procedurally here (no third-party assets): forest tileset, the
"AGR-1" robot hero sprite sheet, parallax backgrounds and a street lamp.
Requires Pillow. The .agrsheet descriptors next to the images are hand written.
"""
from PIL import Image, ImageDraw
import random, math
random.seed(7)
import os
# Run from the repository root. Deterministic (fixed seeds): re-running rewrites identical files.
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "editor", "resources", "templates", "assets", "2d") + os.sep
T = 32
def shade(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c[:3]) + ((c[3],) if len(c) == 4 else (255,))

ts = Image.new("RGBA", (8 * T, 3 * T), (0, 0, 0, 0))
d = ImageDraw.Draw(ts)
dirt, dirt_dark, grass, stone, wood = (122, 82, 52), (92, 60, 38), (88, 170, 72), (120, 126, 138), (150, 104, 62)
def noise_tile(ox, oy, base, var=0.12, dots=60):
    d.rectangle([ox, oy, ox + T - 1, oy + T - 1], fill=base + (255,))
    for _ in range(dots):
        x, y = ox + random.randrange(T - 1), oy + random.randrange(T - 1)
        d.rectangle([x, y, x + 1, y + 1], fill=shade(base, 1 + random.uniform(-var, var)))
# row 0: 0 grass top, 1 dirt, 2 dark dirt, 3 stone bricks, 4 stone top, 5 wood platform, 6 leaves, 7 empty
noise_tile(0, 0, dirt)
d.rectangle([0, 0, T - 1, 9], fill=grass + (255,))
for x in range(0, T, 3): d.rectangle([x, 9, x + 1, 9 + random.randrange(2, 5)], fill=shade(grass, 0.8))
d.rectangle([0, 0, T - 1, 2], fill=shade(grass, 1.25))
noise_tile(T, 0, dirt)
noise_tile(2 * T, 0, dirt_dark, dots=80)
noise_tile(3 * T, 0, stone, dots=40)
for i in range(3):
    y = 8 + i * 9; d.line([3 * T, y, 4 * T - 1, y], fill=shade(stone, 0.75))
    xo = 3 * T + (8 if i % 2 else 20); d.line([xo, y - 8, xo, y], fill=shade(stone, 0.75))
noise_tile(4 * T, 0, stone, dots=40)
d.rectangle([4 * T, 0, 5 * T - 1, 5], fill=shade(stone, 1.25))
d.rectangle([5 * T, 4, 6 * T - 1, 15], fill=wood + (255,))
d.rectangle([5 * T, 4, 6 * T - 1, 6], fill=shade(wood, 1.25))
d.line([5 * T, 15, 6 * T - 1, 15], fill=shade(wood, 0.6))
for x in (5 * T + 6, 6 * T - 8): d.rectangle([x, 16, x + 2, 24], fill=shade(wood, 0.7))
for _ in range(90):
    x, y = 6 * T + 3 + random.randrange(T - 6), 3 + random.randrange(T - 6)
    d.ellipse([x - 3, y - 3, x + 3, y + 3], fill=shade((70, 140, 60), random.uniform(0.7, 1.2)))
# row 1: 8 grass corner left, 9 grass corner right, 10 flowers, 11 rock, 12 bush
noise_tile(0, T, dirt)
d.rectangle([0, T, T - 1, T + 9], fill=grass + (255,)); d.rectangle([0, T, 5, 2 * T - 1], fill=grass + (255,))
noise_tile(T, T, dirt)
d.rectangle([T, T, 2 * T - 1, T + 9], fill=grass + (255,)); d.rectangle([2 * T - 6, T, 2 * T - 1, 2 * T - 1], fill=grass + (255,))
for i, col in enumerate([(240, 200, 60), (230, 90, 110), (150, 170, 250)]):
    x = 2 * T + 7 + i * 9
    d.line([x, 2 * T - 1, x, 2 * T - 10], fill=(60, 130, 50)); d.ellipse([x - 3, 2 * T - 15, x + 3, 2 * T - 9], fill=col)
d.ellipse([3 * T + 4, T + 16, 4 * T - 4, 2 * T - 1], fill=shade(stone, 0.9)); d.ellipse([3 * T + 8, T + 19, 3 * T + 16, T + 24], fill=shade(stone, 1.2))
d.ellipse([4 * T + 2, T + 10, 5 * T - 3, 2 * T - 1], fill=(60, 128, 56)); d.ellipse([4 * T + 8, T + 13, 4 * T + 17, T + 20], fill=(90, 170, 80))
# row 2: 16 water, 17 water surface, 18 crate, 19 sign, 20 mushroom
d.rectangle([0, 2 * T, T - 1, 3 * T - 1], fill=(40, 110, 170, 230))
d.rectangle([T, 2 * T + 6, 2 * T - 1, 3 * T - 1], fill=(40, 110, 170, 230)); d.rectangle([T, 2 * T + 6, 2 * T - 1, 2 * T + 8], fill=(150, 210, 240, 255))
d.rectangle([2 * T + 2, 2 * T + 2, 3 * T - 3, 3 * T - 3], fill=wood + (255,), outline=shade(wood, 0.55), width=2)
d.line([2 * T + 2, 2 * T + 2, 3 * T - 3, 3 * T - 3], fill=shade(wood, 0.55), width=2)
d.rectangle([3 * T + 14, 2 * T + 14, 3 * T + 17, 3 * T - 1], fill=shade(wood, 0.7)); d.rectangle([3 * T + 4, 2 * T + 4, 4 * T - 5, 2 * T + 16], fill=wood + (255,))
d.rectangle([4 * T + 13, 2 * T + 16, 4 * T + 18, 3 * T - 1], fill=(235, 225, 200)); d.ellipse([4 * T + 5, 2 * T + 6, 5 * T - 6, 2 * T + 20], fill=(220, 70, 60)); d.ellipse([4 * T + 10, 2 * T + 9, 4 * T + 13, 2 * T + 12], fill=(255, 240, 230))
ts.save(D + "forest_tiles.png")

# hero "AGR-1": small original robot with an amber visor. frames 0-3 idle, 4-9 run
frames = 10
hero = Image.new("RGBA", (frames * T, T), (0, 0, 0, 0))
h = ImageDraw.Draw(hero)
body, body_dark, visor = (220, 222, 228), (150, 156, 168), (245, 165, 36)
def robot(ox, bob, legs):
    y0 = 6 + bob
    h.rounded_rectangle([ox + 7, y0, ox + 24, y0 + 15], radius=5, fill=body, outline=body_dark)
    h.rectangle([ox + 10, y0 + 4, ox + 22, y0 + 8], fill=visor)
    h.rectangle([ox + 18, y0 + 5, ox + 20, y0 + 6], fill=(255, 240, 200))
    h.line([ox + 15, y0, ox + 15, y0 - 4], fill=body_dark); h.ellipse([ox + 13, y0 - 6, ox + 17, y0 - 2], fill=visor)
    h.rectangle([ox + 9, y0 + 16, ox + 22, y0 + 19], fill=body_dark)
    for i, dx in enumerate(legs):
        lx = ox + 11 + i * 7 + dx
        h.rectangle([lx, y0 + 20, lx + 3, 28], fill=body_dark); h.rectangle([lx - 1, 29, lx + 4, 31], fill=(70, 74, 84))
for i, bob in enumerate([0, 1, 1, 0]): robot(i * T, bob, (0, 0))
for i, (a, b) in enumerate([(-3, 3), (-1, 1), (2, -2), (3, -3), (1, -1), (-2, 2)]): robot((4 + i) * T, 1 if i % 3 == 1 else 0, (a, b))
hero.save(D + "hero.png")

W, H = 512, 256
sky = Image.new("RGBA", (W, H))
sd = ImageDraw.Draw(sky)
for y in range(H):
    t = y / H
    sd.line([0, y, W, y], fill=(int(28 + 70 * t), int(30 + 60 * t), int(62 + 70 * t), 255))
for _ in range(80):
    x, y = random.randrange(W), random.randrange(int(H * 0.6))
    sd.point((x, y), fill=(255, 255, 230, 255))
sd.ellipse([380, 40, 420, 80], fill=(250, 240, 210, 255))
sky.save(D + "bg_sky.png")
def ridge(path, color, base, amp, freq, seed):
    random.seed(seed)
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    g = ImageDraw.Draw(img)
    phase = random.uniform(0, 6)
    pts = [(0, H)]
    for x in range(0, W + 1, 4):
        y = base - amp * (0.6 * math.sin(x / W * 2 * math.pi * freq + phase) + 0.4 * math.sin(x / W * 2 * math.pi * freq * 3 + phase * 2))
        pts.append((x, y))
    pts.append((W, H))
    g.polygon(pts, fill=color)
    img.save(D + path)
ridge("bg_mountains.png", (52, 58, 96, 255), 150, 60, 2, 3)  # integer frequencies: tiles horizontally
ridge("bg_hills.png", (38, 70, 66, 255), 200, 30, 3, 5)

lamp = Image.new("RGBA", (T, 2 * T), (0, 0, 0, 0))
l = ImageDraw.Draw(lamp)
dark, metal, shine = (40, 42, 50), (128, 132, 146), (176, 180, 192)
l.rectangle([13, 19, 18, 63], fill=dark); l.rectangle([14, 20, 17, 62], fill=metal); l.line([14, 20, 14, 62], fill=shine)
l.rectangle([9, 59, 22, 63], fill=dark); l.rectangle([10, 60, 21, 62], fill=metal)
l.polygon([(7, 7), (24, 7), (15, 0)], fill=dark); l.polygon([(9, 6), (22, 6), (15, 2)], fill=metal)
l.rectangle([8, 6, 23, 21], fill=dark); l.rectangle([10, 8, 21, 19], fill=(255, 214, 128)); l.rectangle([12, 10, 19, 17], fill=(255, 240, 200))
lamp.save(D + "lamp.png")
print("2D template art written to", D)
