#!/usr/bin/env python3
"""Finalise les atlas d'imposteurs : propage la couleur dans les zones transparentes (évite les halos
sombres des mipmaps) et compresse en WebP pour l'APK."""
import json
import os
import shutil
import sys

import numpy as np
from PIL import Image

SRC = sys.argv[1]
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "app/src/main/assets/game/img/veg")


def dilate(rgb, mask, it=24):
    rgb = rgb.astype(np.float32)
    m = mask.astype(np.float32)
    for _ in range(it):
        acc = np.zeros_like(rgb); cnt = np.zeros_like(m)
        for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0), (1, 1), (-1, -1), (1, -1), (-1, 1)):
            acc += np.roll(np.roll(rgb * m[..., None], dy, 0), dx, 1)
            cnt += np.roll(np.roll(m, dy, 0), dx, 1)
        fill = (m == 0) & (cnt > 0)
        rgb[fill] = acc[fill] / cnt[fill][:, None]
        m = np.where(fill, 1.0, m)
    return rgb.clip(0, 255).astype(np.uint8)


os.makedirs(OUT, exist_ok=True)
for f in sorted(os.listdir(SRC)):
    if not f.endswith(".json"):
        continue
    name = f[:-5]
    alb = np.array(Image.open(os.path.join(SRC, f"{name}_albedo.png")).convert("RGBA"))
    nor = np.array(Image.open(os.path.join(SRC, f"{name}_normal.png")).convert("RGBA"))
    mask = alb[..., 3] > 127
    alb[..., :3] = dilate(alb[..., :3], mask)
    nor[..., :3] = dilate(nor[..., :3], nor[..., 3] > 127)
    Image.fromarray(alb, "RGBA").save(os.path.join(OUT, f"{name}_albedo.webp"), quality=90, method=6)
    Image.fromarray(nor[..., :3], "RGB").save(os.path.join(OUT, f"{name}_normal.webp"), quality=92, method=6)
    shutil.copy(os.path.join(SRC, f), os.path.join(OUT, f))
    print(name, "ok")
