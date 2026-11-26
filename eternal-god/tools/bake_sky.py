#!/usr/bin/env python3
"""Convertit les ciels HDR Poly Haven (série Qwantani, un même lieu à toutes les heures)
en ressources mobiles pour le cycle jour/nuit :
- img/sky/<clé>.jpg  : fond de ciel 2048x1024 (exposé, tonemappé, sRGB)
- img/sky/<clé>.hdr  : version 256x128 en RGBE pour l'éclairage d'environnement (IBL)
- img/sky/sky.json   : position mesurée du soleil, couleur d'horizon (brouillard), couleur du soleil
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "tools/.cache/polyhaven/hdri")
OUT = os.path.join(ROOT, "app/src/main/assets/game/img/sky")

# clé, fichier, heure du jeu (0..1), luminosité cible du fond
KEYS = [
    ("dawn", "qwantani_dawn_puresky", 0.232, 0.30),
    ("sunrise", "qwantani_sunrise_puresky", 0.262, 0.42),
    ("morning", "qwantani_morning_puresky", 0.33, 0.55),
    ("noon", "qwantani_noon_puresky", 0.50, 0.58),
    ("afternoon", "qwantani_late_afternoon_puresky", 0.66, 0.55),
    ("sunset", "qwantani_sunset_puresky", 0.735, 0.45),
    ("dusk", "qwantani_dusk_2_puresky", 0.775, 0.26),
    ("night", "qwantani_night_puresky", 0.86, 0.05),
]


def read_hdr(path):
    with open(path, "rb") as f:
        data = f.read()
    # en-tête
    pos = 0
    while True:
        end = data.index(b"\n", pos)
        line = data[pos:end]
        pos = end + 1
        if line.startswith(b"-Y") or line.startswith(b"+Y"):
            parts = line.split()
            h, w = int(parts[1]), int(parts[3])
            break
    img = np.zeros((h, w, 4), dtype=np.uint8)
    buf = np.frombuffer(data, dtype=np.uint8)
    for y in range(h):
        if buf[pos] == 2 and buf[pos + 1] == 2:
            pos += 4
            for c in range(4):
                x = 0
                row = img[y, :, c]
                while x < w:
                    n = int(buf[pos]); pos += 1
                    if n > 128:
                        n -= 128
                        row[x:x + n] = buf[pos]; pos += 1
                    else:
                        row[x:x + n] = buf[pos:pos + n]; pos += n
                    x += n
        else:  # non compressé
            img[y] = buf[pos:pos + w * 4].reshape(w, 4); pos += w * 4
    e = img[..., 3].astype(np.int32)
    scale = np.where(e > 0, np.ldexp(1.0, e - 136), 0.0)
    return img[..., :3].astype(np.float32) * scale[..., None]


def write_hdr(path, rgb):
    h, w, _ = rgb.shape
    m = rgb.max(axis=2)
    e = np.zeros((h, w), dtype=np.int32)
    mant = np.zeros((h, w), dtype=np.float64)
    nz = m > 1e-32
    mant[nz], e[nz] = np.frexp(m[nz])
    scale = np.where(nz, mant * 256.0 / np.maximum(m, 1e-32), 0)
    out = np.zeros((h, w, 4), dtype=np.uint8)
    out[..., :3] = np.clip(rgb * scale[..., None], 0, 255).astype(np.uint8)
    out[..., 3] = np.where(nz, e + 128, 0).astype(np.uint8)
    with open(path, "wb") as f:
        f.write(b"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n" + f"-Y {h} +X {w}\n".encode())
        f.write(out.tobytes())


def aces(x):
    a, b, c, d, e = 2.51, 0.03, 2.43, 0.59, 0.14
    return np.clip((x * (a * x + b)) / (x * (c * x + d) + e), 0, 1)


def srgb(x):
    return np.where(x <= 0.0031308, 12.92 * x, 1.055 * np.power(np.maximum(x, 0), 1 / 2.4) - 0.055)


def lum(rgb):
    return rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722


def main():
    os.makedirs(OUT, exist_ok=True)
    meta = {"keys": []}
    for key, name, t, target in KEYS:
        hdr = read_hdr(os.path.join(SRC, name + ".hdr"))
        h, w, _ = hdr.shape
        L = lum(hdr)
        # le soleil = pixel le plus lumineux (si nettement au-dessus du ciel)
        y, x = np.unravel_index(np.argmax(L), L.shape)
        sky_med = float(np.median(L[: h // 2]))
        has_sun = L[y, x] > sky_med * 200 and y < h // 2
        el = (0.5 - (y + 0.5) / h) * math.pi
        az = ((x + 0.5) / w) * 2 * math.pi  # convention three.js : u = atan(dir.z, dir.x) / 2π + 0.5
        sun = None
        if has_sun:
            # direction comme dans equirectUv de three.js
            phi = az - math.pi  # atan2(z, x)
            sun = [math.cos(el) * math.cos(phi), math.sin(el), math.cos(el) * math.sin(phi)]
            sy = max(0, y - 3); sx = max(0, x - 3)
            sc = hdr[sy:y + 4, sx:x + 4].reshape(-1, 3).mean(axis=0)
            sun_col = (sc / max(1e-6, sc.max())).tolist()
        else:
            sun_col = [0.6, 0.7, 1.0]
        # exposition : la médiane du ciel supérieur vise la luminosité cible
        expo = target / max(1e-6, sky_med) * 0.42
        ldr = srgb(np.clip(hdr * expo, 0, 1))
        img = Image.fromarray((ldr * 255 + 0.5).astype(np.uint8))
        img.resize((2048, 1024), Image.LANCZOS).save(os.path.join(OUT, key + ".jpg"), quality=86, optimize=True)
        # couleur d'horizon (bande de 0 à 6°) pour le brouillard, en sRGB
        y0, y1 = int(h * (0.5 - 6 / 180)), int(h * 0.5)
        hor = ldr[y0:y1].reshape(-1, 3).mean(axis=0)
        zen = ldr[: int(h * 0.15)].reshape(-1, 3).mean(axis=0)
        # IBL basse résolution (énergie relative conservée, soleil atténué)
        rgb_small = np.stack([np.array(Image.fromarray(np.clip(hdr[..., c] * expo, 0, 60).astype(np.float32)).resize((256, 128), Image.BOX)) for c in range(3)], axis=2)
        write_hdr(os.path.join(OUT, key + ".hdr"), rgb_small.astype(np.float32))
        meta["keys"].append({
            "key": key, "t": t, "sun": sun, "sunColor": sun_col, "sunAzimuth": az if has_sun else None,
            "horizon": hor.tolist(), "zenith": zen.tolist(), "exposure": expo,
        })
        print(key, "soleil" if has_sun else "sans soleil", "élévation %.1f°" % math.degrees(el) if has_sun else "", "horizon", np.round(hor, 2))
    with open(os.path.join(OUT, "sky.json"), "w") as f:
        json.dump(meta, f, indent=1)


if __name__ == "__main__":
    sys.exit(main())
