#!/usr/bin/env python3
"""Generates the original particle textures and curves of the AGR ENGINE effect presets.

editor/resources/templates/assets/particles/:
    spark.png       bright core with a soft glow and 4 thin rays (sparks, hits, coins)
    smoke.png       soft cloudy puff (smoke, dust, steam)
    flame.png       teardrop flame, white-hot base (fire, torches)
    star.png        4-point magic star with a halo (magic, pickups, healing)
    explosion.png   4 x 4 flipbook: a fireball that blooms, breaks up and turns to smoke
    fire / smoke / explosion / sparks / magic / dust .agrparticle   over-lifetime size and color

White or near-white pictures: the colors come from the gradients, so one texture serves many effects.
Deterministic (fixed seeds).   python3 tools/generate_particle_art.py
"""
import json
import math
import os
import random

from PIL import Image, ImageFilter

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "editor", "resources", "templates", "assets", "particles")


def value_noise(size, cells, seed):
    """Smooth periodic value noise in 0..1 (bilinear-cubic interpolation of a random grid)."""
    rnd = random.Random(seed)
    grid = [[rnd.random() for _ in range(cells)] for _ in range(cells)]
    out = [[0.0] * size for _ in range(size)]
    for y in range(size):
        gy = y / size * cells
        y0 = int(gy) % cells
        fy = gy - int(gy)
        fy = fy * fy * (3 - 2 * fy)
        for x in range(size):
            gx = x / size * cells
            x0 = int(gx) % cells
            fx = gx - int(gx)
            fx = fx * fx * (3 - 2 * fx)
            a = grid[y0][x0] + (grid[y0][(x0 + 1) % cells] - grid[y0][x0]) * fx
            b = grid[(y0 + 1) % cells][x0] + (grid[(y0 + 1) % cells][(x0 + 1) % cells] - grid[(y0 + 1) % cells][x0]) * fx
            out[y][x] = a + (b - a) * fy
    return out


def fbm(size, seed, octaves=4):
    layers = [value_noise(size, 4 * 2 ** o, seed + o) for o in range(octaves)]
    out = [[0.0] * size for _ in range(size)]
    total = sum(0.5 ** o for o in range(octaves))
    for y in range(size):
        for x in range(size):
            out[y][x] = sum(layers[o][y][x] * 0.5 ** o for o in range(octaves)) / total
    return out


def save_rgba(name, size, pixel):
    img = Image.new("RGBA", (size, size))
    px = img.load()
    for y in range(size):
        for x in range(size):
            r, g, b, a = pixel(x, y)
            px[x, y] = (int(max(0, min(1, r)) * 255), int(max(0, min(1, g)) * 255), int(max(0, min(1, b)) * 255), int(max(0, min(1, a)) * 255))
    img.save(os.path.join(OUT, name))
    return img


def spark():
    n = 64
    def p(x, y):
        dx, dy = (x + 0.5) / n * 2 - 1, (y + 0.5) / n * 2 - 1
        r = math.hypot(dx, dy)
        glow = max(0.0, 1 - r) ** 2.2
        core = math.exp(-(r / 0.12) ** 2)
        rays = max(math.exp(-(dx / 0.035) ** 2) * max(0, 1 - abs(dy)), math.exp(-(dy / 0.035) ** 2) * max(0, 1 - abs(dx))) * 0.8
        a = min(1.0, glow * 0.7 + core + rays)
        return 1.0, 1.0, 1.0, a
    save_rgba("spark.png", n, p)


def smoke():
    n = 128
    noise = fbm(n, 11)
    def p(x, y):
        dx, dy = (x + 0.5) / n * 2 - 1, (y + 0.5) / n * 2 - 1
        r = math.hypot(dx, dy)
        falloff = max(0.0, 1 - r) ** 1.3
        a = max(0.0, min(1.0, falloff * (0.35 + 1.2 * noise[y][x]) - 0.08))
        shade = 0.75 + 0.25 * noise[(y + 20) % n][x]
        return shade, shade, shade, a
    save_rgba("smoke.png", n, p)


def flame():
    n = 64
    noise = fbm(n, 23, 3)
    def p(x, y):
        u, v = (x + 0.5) / n * 2 - 1, (y + 0.5) / n  # v: 0 top, 1 bottom
        width = 0.62 * math.sin(math.pi * min(1.0, v * 1.05)) ** 0.8 * (0.4 + 0.6 * v)
        d = abs(u) / max(width, 1e-3)
        body = max(0.0, 1 - d) ** 0.8 * min(1.0, v * 3.0)
        body *= 0.75 + 0.5 * noise[y][x]
        hot = max(0.0, 1 - math.hypot(u / 0.35, (v - 0.78) / 0.25))
        return 1.0, 1.0, 1.0, min(1.0, body + hot * 0.5)  # white: the gradient colors it
    save_rgba("flame.png", n, p)


def star():
    n = 64
    def p(x, y):
        dx, dy = (x + 0.5) / n * 2 - 1, (y + 0.5) / n * 2 - 1
        r = math.hypot(dx, dy)
        ang = math.atan2(dy, dx)
        points = (abs(math.cos(2 * ang)) ** 3) * 0.7 + 0.22  # 4 rounder points (a sparkle)
        shape = max(0.0, 1 - r / points) ** 1.5
        halo = max(0.0, 1 - r) ** 3 * 0.45
        return 1.0, 1.0, 1.0, min(1.0, shape * 1.4 + halo)
    save_rgba("star.png", n, p)


def explosion():
    cell, cols = 128, 4
    frames = cols * cols
    sheet = Image.new("RGBA", (cell * cols, cell * cols))
    noise = fbm(cell, 37)
    noise2 = fbm(cell, 41)
    for f in range(frames):
        t = f / (frames - 1)
        radius = 0.35 + 0.6 * (1 - (1 - t) ** 2.5)
        img = Image.new("RGBA", (cell, cell))
        px = img.load()
        for y in range(cell):
            for x in range(cell):
                dx, dy = (x + 0.5) / cell * 2 - 1, (y + 0.5) / cell * 2 - 1
                r = math.hypot(dx, dy) / radius
                # Turbulent edge that breaks up over time.
                n1 = noise[(y + f * 3) % cell][(x + f * 2) % cell]
                n2 = noise2[y][x]
                edge = 1 - r + (n1 - 0.5) * (0.5 + 0.6 * t)
                density = max(0.0, min(1.0, edge * 3.0)) * max(0.0, 1.0 - t * 0.9 * (0.6 + n2))
                if density <= 0:
                    continue
                heat = max(0.0, 1 - t * 1.6) * max(0.0, 1 - r * 0.9) + 0.25 * max(0.0, 1 - t * 2.5)
                # Continuous ramp: smoke grey -> dark red -> orange -> yellow -> white as the heat rises.
                g = 0.28 + 0.2 * n2
                stops = [(0.0, (g, g, g * 1.05)), (0.15, (0.55, 0.22, 0.12)), (0.4, (1.0, 0.55, 0.15)), (0.7, (1.0, 0.9, 0.6)), (1.0, (1.0, 0.97, 0.85))]
                h = min(1.0, heat)
                for (h0, c0), (h1, c1) in zip(stops, stops[1:]):
                    if h <= h1:
                        k = (h - h0) / (h1 - h0)
                        col = tuple(a + (b - a) * k for a, b in zip(c0, c1))
                        break
                shade = 0.8 + 0.35 * n1
                px[x, y] = tuple(int(max(0, min(1, c * shade)) * 255) for c in col) + (int(density * 255),)
        img = img.filter(ImageFilter.GaussianBlur(0.6))
        sheet.paste(img, ((f % cols) * cell, (f // cols) * cell))
    sheet.save(os.path.join(OUT, "explosion.png"))


def curves(name, size, gradient):
    with open(os.path.join(OUT, name + ".agrparticle"), "w") as fh:
        json.dump({"version": 1, "size": size, "gradient": gradient}, fh, indent=2)
        fh.write("\n")


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    spark()
    smoke()
    flame()
    star()
    explosion()
    curves("fire", [[0, 0.6], [0.25, 1.0], [1, 0.2]],
           [[0, 1, 0.95, 0.7, 0], [0.12, 1, 0.75, 0.25, 1], [0.55, 0.95, 0.3, 0.08, 0.7], [1, 0.4, 0.1, 0.05, 0]])
    curves("smoke", [[0, 0.4], [1, 1.6]], [[0, 0.55, 0.55, 0.6, 0], [0.15, 0.5, 0.5, 0.55, 0.55], [1, 0.35, 0.35, 0.38, 0]])
    curves("explosion", [[0, 0.7], [0.3, 1.1], [1, 1.4]], [[0, 1, 1, 1, 1], [0.7, 1, 1, 1, 0.9], [1, 1, 1, 1, 0]])
    curves("sparks", [[0, 1.0], [1, 0.1]], [[0, 1, 1, 0.85, 1], [0.3, 1, 0.75, 0.3, 1], [1, 0.9, 0.3, 0.1, 0]])
    curves("magic", [[0, 0], [0.15, 1.1], [0.5, 0.8], [1, 0]], [[0, 0.75, 0.9, 1, 1], [0.5, 0.65, 0.45, 1, 0.9], [1, 0.95, 0.5, 1, 0]])
    curves("dust", [[0, 0.5], [1, 1.3]], [[0, 0.8, 0.72, 0.6, 0], [0.2, 0.8, 0.72, 0.6, 0.45], [1, 0.8, 0.72, 0.6, 0]])
    print("particles: 5 textures, 6 curves")
