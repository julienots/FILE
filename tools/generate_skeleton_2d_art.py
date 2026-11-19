#!/usr/bin/env python3
"""Generates the 2D skeleton demo of the "Vitrine 2D" template (AGR ENGINE), all original:

    assets/2d/caterpillar.png     one picture of a caterpillar, deformed by 4 bones (Skin2D)
    assets/2d/caterpillar_n.png   its normal map (tools/generate_normal_map.py)
    assets/animations/caterpillar_crawl.agranim   a travelling wave on the bones (inchworm humps)

    python3 tools/generate_skeleton_2d_art.py
"""
import json
import math
import os
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from generate_normal_map import normal_map  # noqa: E402

ASSETS = os.path.join(HERE, "..", "editor", "resources", "templates", "assets")
W, H = 256, 64
S = 4  # supersampling


def caterpillar():
    img = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    segments = 9
    for i in range(segments):  # tail (left) to head (right)
        cx = (18 + i * 24) * S
        cy = 36 * S
        r = (15 + (2 if i % 2 else 0)) * S
        # feet
        d.ellipse([cx - 4 * S, cy + r - 6 * S, cx + 4 * S, cy + r + 2 * S], fill=(62, 120, 48, 255))
        # body segment: darker rim, lighter top, a yellow band
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(70, 150, 60, 255))
        d.ellipse([cx - r + 3 * S, cy - r + 2 * S, cx + r - 3 * S, cy + r - 5 * S], fill=(110, 196, 84, 255))
        d.ellipse([cx - r + 8 * S, cy - r + 4 * S, cx + r - 10 * S, cy - r + 11 * S], fill=(170, 230, 130, 255))
        if i % 2 == 0:
            d.ellipse([cx - 3 * S, cy - 2 * S, cx + 3 * S, cy + 4 * S], fill=(250, 210, 70, 255))
    # head
    hx, hy, hr = 232 * S, 30 * S, 21 * S
    d.ellipse([hx - hr, hy - hr, hx + hr, hy + hr], fill=(78, 160, 66, 255))
    d.ellipse([hx - hr + 3 * S, hy - hr + 2 * S, hx + hr - 3 * S, hy + hr - 5 * S], fill=(126, 206, 92, 255))
    # antennae
    for ax in (-8, 6):
        d.line([hx + ax * S, hy - hr + 3 * S, hx + (ax + 4) * S, hy - hr - 6 * S], fill=(60, 110, 50, 255), width=2 * S)
        d.ellipse([hx + (ax + 1) * S, hy - hr - 10 * S, hx + (ax + 7) * S, hy - hr - 4 * S], fill=(250, 120, 90, 255))
    # eye, cheek, smile
    d.ellipse([hx + 1 * S, hy - 10 * S, hx + 13 * S, hy + 3 * S], fill=(255, 255, 255, 255))
    d.ellipse([hx + 6 * S, hy - 6 * S, hx + 12 * S, hy + 1 * S], fill=(30, 30, 40, 255))
    d.ellipse([hx + 9 * S, hy - 5 * S, hx + 11 * S, hy - 3 * S], fill=(255, 255, 255, 255))
    d.ellipse([hx + 4 * S, hy + 4 * S, hx + 12 * S, hy + 9 * S], fill=(240, 130, 120, 200))
    d.arc([hx + 6 * S, hy + 2 * S, hx + 18 * S, hy + 12 * S], 20, 120, fill=(40, 60, 30, 255), width=2 * S)
    return img.resize((W, H), Image.LANCZOS)


def crawl_clip(bones=4, period=1.6, amplitude=14.0):
    keys_per = 8
    tracks = []
    path = ""
    for b in range(bones):
        path = f"Bone {b + 1}" if b == 0 else f"{path}/Bone {b + 1}"
        keys = []
        for k in range(keys_per + 1):
            t = period * k / keys_per
            # Travelling wave from the tail to the head; the first bone stays still (the tail rests on the ground).
            v = amplitude * (0.0 if b == 0 else 1.0) * math.sin(2 * math.pi * t / period - b * math.pi / 2)
            keys.append({"t": round(t, 4), "v": round(v, 3), "interp": "smooth"})
        tracks.append({"target": path, "component": "Transform", "field": "rotation", "channel": 2, "keys": keys})
    return {"version": 1, "duration": period, "loop": True, "tracks": tracks, "events": []}


if __name__ == "__main__":
    pic = caterpillar()
    pic.save(os.path.join(ASSETS, "2d", "caterpillar.png"))
    normal_map(pic, strength=2.5, bevel=4, detail=0.7).save(os.path.join(ASSETS, "2d", "caterpillar_n.png"))
    with open(os.path.join(ASSETS, "animations", "caterpillar_crawl.agranim"), "w") as f:
        json.dump(crawl_clip(), f, indent=2)
        f.write("\n")
    print("caterpillar.png, caterpillar_n.png, caterpillar_crawl.agranim")
