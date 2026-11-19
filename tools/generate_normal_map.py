#!/usr/bin/env python3
"""Generates a normal map for a 2D texture (AGR ENGINE lit sprites and tilemaps).

The relief is estimated from the picture itself:
  - brightness: light pixels stand out, dark ones (mortar, cracks, outlines) sink in;
  - silhouette: a bevel rounds the edges of opaque shapes (sprites with transparency);
  - tiles (--tile N): each N x N cell of a tileset is treated on its own, so tiles do not
    bleed into each other (they stay seamless; --tile-bevel also rounds every tile's border).

Output: <name>_n.png next to the input (OpenGL convention: green points up), to put in
SpriteRenderer.normalMap or Tilemap.normalMap. Only Pillow is needed.

    python3 tools/generate_normal_map.py assets/2d/forest_tiles.png --tile 32
    python3 tools/generate_normal_map.py hero.png --strength 3 --bevel 3
"""
import argparse
import math
import os

from PIL import Image


def height_field(img, tile, bevel, detail, tile_bevel, tileable=False):
    w, h = img.size
    px = img.load()
    lum = [[0.0] * w for _ in range(h)]
    alpha = [[0.0] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            lum[y][x] = (0.299 * r + 0.587 * g + 0.114 * b) / 255.0
            alpha[y][x] = a / 255.0
    # 3x3 box blur of the brightness inside each cell: softer, less noisy relief.
    def cell(x, y):
        return (x // tile, y // tile) if tile else (0, 0)
    blurred = [[0.0] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            total, count = 0.0, 0
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    xx, yy = x + dx, y + dy
                    if 0 <= xx < w and 0 <= yy < h and cell(xx, yy) == cell(x, y) and alpha[yy][xx] > 0.5:
                        total += lum[yy][xx]
                        count += 1
            blurred[y][x] = total / count if count else 0.0
    # Bevel: distance (pixels) to the nearest transparent pixel or cell border, capped.
    height = [[0.0] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            if alpha[y][x] <= 0.5:
                continue
            d = bevel
            for dy in range(-bevel, bevel + 1):
                for dx in range(-bevel, bevel + 1):
                    dist = math.hypot(dx, dy)
                    if dist >= d:
                        continue
                    xx, yy = x + dx, y + dy
                    inside_image = 0 <= xx < w and 0 <= yy < h
                    if not inside_image and (tile or tileable):
                        continue  # the atlas / repeat border is not an edge of the shape
                    outside = not inside_image or alpha[yy][xx] <= 0.5
                    if tile_bevel and tile and not outside:
                        outside = cell(xx, yy) != cell(x, y)
                    if outside:
                        d = dist
            edge = min(d, bevel) / bevel if bevel else 1.0
            height[y][x] = math.sin(edge * math.pi * 0.5) + detail * blurred[y][x]
    return height, alpha


def normal_map(img, tile=0, strength=2.0, bevel=2, detail=1.0, tile_bevel=False, tileable=False):
    img = img.convert("RGBA")
    w, h = img.size
    height, alpha = height_field(img, tile, bevel, detail, tile_bevel, tileable)
    out = Image.new("RGBA", (w, h))
    po = out.load()

    def hgt(x, y, cx, cy):
        # Clamp to the pixel's own cell (no slope across tile borders but the bevel).
        if tile:
            x = min(max(x, cx * tile), cx * tile + tile - 1)
            y = min(max(y, cy * tile), cy * tile + tile - 1)
        if tileable:  # repeating texture: the relief wraps around
            return height[y % h][x % w]
        x = min(max(x, 0), w - 1)
        y = min(max(y, 0), h - 1)
        return height[y][x]

    for y in range(h):
        for x in range(w):
            cx, cy = (x // tile, y // tile) if tile else (0, 0)
            # Sobel gradient; image y grows downward, normal map green points up.
            gx = (hgt(x + 1, y - 1, cx, cy) + 2 * hgt(x + 1, y, cx, cy) + hgt(x + 1, y + 1, cx, cy)
                  - hgt(x - 1, y - 1, cx, cy) - 2 * hgt(x - 1, y, cx, cy) - hgt(x - 1, y + 1, cx, cy))
            gy = (hgt(x - 1, y + 1, cx, cy) + 2 * hgt(x, y + 1, cx, cy) + hgt(x + 1, y + 1, cx, cy)
                  - hgt(x - 1, y - 1, cx, cy) - 2 * hgt(x, y - 1, cx, cy) - hgt(x + 1, y - 1, cx, cy))
            nx, ny, nz = -gx * strength / 4.0, gy * strength / 4.0, 1.0
            inv = 1.0 / math.sqrt(nx * nx + ny * ny + nz * nz)
            nx, ny, nz = nx * inv, ny * inv, nz * inv
            a = 255 if alpha[y][x] > 0.5 else 0
            po[x, y] = (int((nx * 0.5 + 0.5) * 255 + 0.5), int((ny * 0.5 + 0.5) * 255 + 0.5), int((nz * 0.5 + 0.5) * 255 + 0.5), a)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input")
    ap.add_argument("--output", help="default: <input>_n.png")
    ap.add_argument("--tile", type=int, default=0, help="cell size of a tileset / sprite sheet (0: one picture)")
    ap.add_argument("--strength", type=float, default=2.0, help="relief strength")
    ap.add_argument("--bevel", type=int, default=2, help="rounded edge width in pixels")
    ap.add_argument("--detail", type=float, default=1.0, help="weight of the brightness relief")
    ap.add_argument("--tileable", action="store_true", help="repeating texture (walls, floors): the relief wraps at the borders")
    ap.add_argument("--tile-bevel", action="store_true", help="also bevel the border of every tile (blocks, crates) instead of seamless tiles")
    args = ap.parse_args()
    out = normal_map(Image.open(args.input), args.tile, args.strength, args.bevel, args.detail, args.tile_bevel, args.tileable)
    path = args.output or os.path.splitext(args.input)[0] + "_n.png"
    out.save(path)
    print(path)


if __name__ == "__main__":
    main()
