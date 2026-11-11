#!/usr/bin/env python3
"""Generates the original terrain textures of the project templates (AGR ENGINE).

  * terrain/grass.png, rock.png, dirt.png, sand.png - 256 x 256, seamless (tileable in both
    directions), sRGB colors. Layers 0..3 of a new Terrain (Create > Terrain).

Procedural (periodic value noise), deterministic. Requires numpy and Pillow.
"""
import os

import numpy as np
from PIL import Image

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "editor", "resources", "templates", "assets", "terrain")
N = 256


def noise(cells, octaves, seed):
    """Value noise periodic over the image (seamless tiling)."""
    rng = np.random.default_rng(seed)
    total = np.zeros((N, N))
    amp, norm = 1.0, 0.0
    for o in range(octaves):
        c = cells * 2 ** o
        grid = rng.random((c, c))
        coords = np.arange(N) / N * c
        i0 = np.floor(coords).astype(int)
        f = coords - i0
        f = f * f * (3 - 2 * f)
        i1 = (i0 + 1) % c
        a = grid[np.ix_(i0, i0)]
        b = grid[np.ix_(i0, i1)]
        cc = grid[np.ix_(i1, i0)]
        d = grid[np.ix_(i1, i1)]
        fx = f[None, :]
        fy = f[:, None]
        total += amp * ((a * (1 - fx) + b * fx) * (1 - fy) + (cc * (1 - fx) + d * fx) * fy)
        norm += amp
        amp *= 0.5
    return total / norm


def save(name, rgb):
    img = np.clip(rgb * 255.0, 0, 255).astype(np.uint8)
    Image.fromarray(img, "RGB").save(os.path.join(OUT, name), optimize=True)


def colorize(n, low, high):
    low, high = np.array(low)[None, None, :], np.array(high)[None, None, :]
    return low * (1 - n[..., None]) + high * n[..., None]


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    # Grass: green with lighter blades (fine noise) and darker patches.
    base = noise(4, 5, 1)
    blades = noise(32, 2, 2)
    g = colorize(np.clip(base * 0.6 + blades * 0.6 - 0.1, 0, 1), (0.18, 0.32, 0.10), (0.42, 0.60, 0.22))
    save("grass.png", g)
    # Rock: gray with cracks (ridged noise).
    r = noise(6, 5, 3)
    ridges = 1.0 - np.abs(noise(10, 3, 4) * 2.0 - 1.0)
    rock = colorize(np.clip(r * 0.7 + 0.2, 0, 1), (0.30, 0.29, 0.28), (0.62, 0.60, 0.57)) * (1.0 - 0.45 * ridges[..., None] ** 6)
    save("rock.png", rock)
    # Dirt: warm brown with pebbles.
    d = noise(8, 5, 5)
    pebbles = (noise(48, 1, 6) > 0.78).astype(float) * 0.25
    dirt = colorize(np.clip(d + pebbles, 0, 1), (0.28, 0.19, 0.12), (0.50, 0.36, 0.24))
    save("dirt.png", dirt)
    # Sand: pale with soft ripples.
    x = np.arange(N) / N
    ripples = 0.5 + 0.5 * np.sin((x[None, :] * 12 + noise(4, 2, 7) * 2.0) * 2 * np.pi)
    sand = colorize(np.clip(noise(16, 3, 8) * 0.5 + ripples * 0.4, 0, 1), (0.70, 0.62, 0.45), (0.88, 0.81, 0.62))
    save("sand.png", sand)
    print("terrain textures written to", OUT)
