#!/usr/bin/env python3
"""LOST KINGDOM — import a static building / object GLB (Tripo, Meshy...) as a world model.

  1. optionally (--drop-floating) drops pieces floating away from the main object (sparkles...);
  2. simplifies to --tris triangles (UV seams kept), see tools/simplify_glb.py;
  3. centres it (bounding-box centre on x/z, base at y = 0), front = +Z (the side facing the camera in Tripo);
  4. writes assets/models/props/<name>.lkm (LKM2 with a single root joint, flagged "not metal") and <name>.jpg.

The game fits the model into the footprint of the structure it replaces (game/Props.cpp, structureFootprint),
so its absolute size does not matter. Names: house, house_2, house_3, inn, stonehouse, longhouse, forge,
chapel, tower, well, stall, tent (and _2 / _3 variants of each).

Usage: python3 -I tools/import_prop.py model.glb house [--tris 12000] [--tex 1024] [--rotate 0|90|180|270] [--preview out.png]
"""
import argparse, io, os, struct, sys
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from simplify_glb import load, drop_floating, decimate   # noqa: E402

NAMES = ["house", "inn", "stonehouse", "longhouse", "forge", "chapel", "tower", "well", "stall", "tent"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("glb"); ap.add_argument("name")
    ap.add_argument("--tris", type=int, default=12000)
    ap.add_argument("--tex", type=int, default=1024)
    ap.add_argument("--rotate", type=int, default=0, choices=[0, 90, 180, 270], help="turn the model so its entrance faces +Z")
    ap.add_argument("--drop-floating", action="store_true", help="drop pieces that do not touch the main one (off by default: roofs and chimneys are often separate pieces)")
    ap.add_argument("--preview", default="")
    ap.add_argument("--out-dir", default="", help="instead of assets/models/props (tests)")
    a = ap.parse_args()
    base = a.name.split("_")[0]
    if base not in NAMES or not (a.name == base or a.name in (base + "_2", base + "_3")):
        sys.exit(f"name must be one of {NAMES} (optionally with _2 or _3)")
    P, UV, I, tex = load(a.glb)
    if tex is None:
        sys.exit("the model has no base colour texture")
    print(f"input: {len(P)} vertices, {len(I)} triangles")
    if a.drop_floating:
        I = drop_floating(P, I)
    used = np.unique(I); P = P.copy()
    if a.rotate:
        r = np.radians(a.rotate); c, s = np.cos(r), np.sin(r)
        P[:, [0, 2]] = np.stack([c * P[:, 0] + s * P[:, 2], -s * P[:, 0] + c * P[:, 2]], 1)
    lo, hi = P[used].min(0), P[used].max(0)
    P[:, 0] -= 0.5 * (lo[0] + hi[0]); P[:, 2] -= 0.5 * (lo[2] + hi[2]); P[:, 1] -= lo[1]
    if len(I) > a.tris:
        Pd, Nd, UVd, Id = decimate(P, UV, I, a.tris)
    else:
        keep = np.unique(I); remap = np.full(len(P), -1); remap[keep] = np.arange(len(keep))
        Pd, UVd, Id = P[keep], UV[keep], remap[I]
        fn = np.cross(Pd[Id[:, 1]] - Pd[Id[:, 0]], Pd[Id[:, 2]] - Pd[Id[:, 0]]); Nd = np.zeros_like(Pd)
        for c in range(3): np.add.at(Nd, Id[:, c], fn)
        Nd /= np.maximum(np.linalg.norm(Nd, axis=1, keepdims=True), 1e-12)
    ext = Pd.max(0) - Pd.min(0)
    print(f"model: {len(Pd)} vertices, {len(Id)} triangles, {ext[0]:.2f} x {ext[1]:.2f} x {ext[2]:.2f} (any unit: the game fits it)")
    out = a.out_dir or os.path.join(HERE, "..", "assets", "models", "props"); os.makedirs(out, exist_ok=True)
    path = os.path.join(out, a.name + ".lkm")
    nv = len(Pd)
    with open(path, "wb") as f:
        f.write(b"LKM2"); f.write(struct.pack("<IIIf", nv, Id.size, 1, float(ext[1])))
        f.write(np.zeros(3, "<f4").tobytes())
        f.write(Pd.astype("<f4").tobytes())
        f.write(np.clip(np.round(Nd * 127), -127, 127).astype(np.int8).tobytes())
        f.write(np.clip(np.round(np.stack([UVd[:, 0] % 1.0001, UVd[:, 1] % 1.0001], 1) * 65535), 0, 65535).astype("<u2").tobytes())
        # one bone, full weight; flag 255 = "not metal" (painted stone and wood would otherwise read as steel)
        f.write(np.tile(np.array([0, 0, 255, 255], np.uint8), (nv, 1)).tobytes())
        f.write(Id.astype("<u4").reshape(-1).tobytes())
    tp = os.path.join(out, a.name + ".jpg")
    tex.resize((a.tex, a.tex), Image.LANCZOS).save(tp, quality=88, optimize=True)
    print(f"wrote {path} ({os.path.getsize(path) / 1e6:.2f} MB) and {tp} ({os.path.getsize(tp) / 1e6:.2f} MB)")
    if a.preview:
        S = 480; img = Image.new("RGB", (S * 2, S), (30, 30, 36)); d = ImageDraw.Draw(img)
        tx = np.asarray(tex.resize((256, 256)))
        H = max(ext); 
        for vi, (ax, sgn) in enumerate(((0, 1), (2, -1))):
            def to2(p): return (vi * S + S / 2 + sgn * p[..., ax] * S / H * 0.85, S - 20 - p[..., 1] * S / H * 0.85)
            depth = Pd[Id].mean(1)[:, 2 if ax == 0 else 0] * (1 if ax == 0 else -1)
            for t in np.argsort(depth):
                tri = Id[t]; X, Y = to2(Pd[tri]); uv = UVd[tri].mean(0)
                c = tx[int(np.clip(uv[1] % 1 * 255, 0, 255)), int(np.clip(uv[0] % 1 * 255, 0, 255))]
                sh = 0.55 + 0.45 * max(0.0, float(Nd[tri].mean(0) @ np.array([0.3, 0.6, 0.75])))
                d.polygon(list(zip(X, Y)), fill=tuple(int(v * sh) for v in c))
        img.save(a.preview); print("preview", a.preview)


if __name__ == "__main__":
    main()
