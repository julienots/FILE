#!/usr/bin/env python3
"""LOST KINGDOM — import a car or a motorbike GLB (Tripo...) as a drivable vehicle with spinning wheels.

Tripo vehicles are one solid mesh. This tool:
  1. turns it so the front faces +Z (--rotate), centres it, puts the tyres on y = 0;
  2. simplifies it (tools/simplify_glb.py, UV seams kept);
  3. finds each wheel from the geometry: the tyre's contact patch on the ground gives its position and width,
     a ring search in the side plane gives its radius; the tyre and rim (not the fork, the swingarm or the hub
     parts beside it) become a separate bone;
  4. writes assets/models/vehicles/<name>.lkm (LKM2): joint 0 = body origin, joint 1 = seat (rider), joints 2..
     = wheel centres (front wheels first); bone 0 = body, bone 2.. = wheels. And <name>.jpg.

Usage: python3 -I tools/import_vehicle.py model.glb car --kind car [--rotate 270] [--length 4.4] [--preview out.png]
       python3 -I tools/import_vehicle.py model.glb moto --kind moto --rotate 180 --length 2.1
GLBs compressed with EXT_meshopt_compression must be decoded first (gltf-transform, see docs/TRIPO_PROMPTS.md).
"""
import argparse, os, struct, sys
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from simplify_glb import load, decimate   # noqa: E402


def find_wheel(P, zc_hint, side, L, H, W, moto):
    """Contact patch near (z = zc_hint, x side) -> centre (x, r, z), radius, half width, x range of the tyre."""
    x, y, z = P[:, 0], P[:, 1], P[:, 2]
    low = y < 0.025 * H
    if moto: sel = low & (np.abs(z - zc_hint) < 0.25 * L)
    else: sel = low & (np.abs(z - zc_hint) < 0.22 * L) & (np.sign(x) == side) & (np.abs(x) > 0.15 * W)
    if sel.sum() < 5:
        return None
    zc = float(np.median(z[sel])); xs = x[sel]
    x0, x1 = float(np.percentile(xs, 2)), float(np.percentile(xs, 98))
    xc = 0.5 * (x0 + x1); hw = max(0.5 * (x1 - x0), 0.02 * W)
    band = (x > xc - hw * 1.1) & (x < xc + hw * 1.1) & (np.abs(z - zc) < 0.3 * L) & (y < 0.6 * H)
    best, br = -1, 0.15 * H
    for r in np.linspace(0.12 * H, 0.45 * H, 60):
        d = np.hypot(z[band] - zc, y[band] - r)
        ring = np.sum((d > 0.93 * r) & (d < 1.01 * r))
        outside = np.sum((d > 1.01 * r) & (d < 1.08 * r))       # a real tyre: dense ring, little just outside it
        score = ring - 1.5 * outside
        if score > best: best, br = score, r
    return np.array([xc, br, zc]), float(br), float(hw)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("glb"); ap.add_argument("name")
    ap.add_argument("--kind", choices=["car", "moto"], required=True)
    ap.add_argument("--rotate", type=int, default=0, choices=[0, 90, 180, 270])
    ap.add_argument("--length", type=float, default=0, help="real length in metres (default: car 4.4, moto 2.1)")
    ap.add_argument("--tris", type=int, default=16000)
    ap.add_argument("--tex", type=int, default=1024)
    ap.add_argument("--preview", default="")
    a = ap.parse_args()
    moto = a.kind == "moto"
    P, UV, I, tex = load(a.glb)
    if tex is None: sys.exit("no base colour texture")
    P = P.copy()
    if a.rotate:
        r = np.radians(a.rotate); c, s = np.cos(r), np.sin(r)
        P[:, [0, 2]] = np.stack([c * P[:, 0] + s * P[:, 2], -s * P[:, 0] + c * P[:, 2]], 1)
    used = np.unique(I)
    lo, hi = P[used].min(0), P[used].max(0)
    P -= np.array([(lo[0] + hi[0]) / 2, lo[1], (lo[2] + hi[2]) / 2])
    Lm = a.length or (2.1 if moto else 4.4)
    P *= Lm / (hi[2] - lo[2])
    print(f"input: {len(P)} vertices, {len(I)} triangles")
    Pd, Nd, UVd, Id = decimate(P, UV, I, a.tris)
    ext = Pd.max(0) - Pd.min(0); W, H, L = ext
    print(f"simplified: {len(Pd)} vertices, {len(Id)} triangles; {W:.2f} x {H:.2f} x {L:.2f} m")
    # ---- wheels: front first
    wheels = []
    zs = [0.32 * L, -0.32 * L]
    for zh in zs:
        for side in ([0] if moto else [1, -1]):
            w = find_wheel(Pd, zh, side, L, H, W, moto)
            if w is None: sys.exit(f"wheel not found near z={zh:.2f} side {side}")
            wheels.append(w)
    # the same tyre size on every wheel of an axle (one bad ring fit must not give a giant wheel)
    rmed = float(np.median([w[1] for w in wheels]))
    wheels = [(np.array([w[0][0], rmed if abs(w[1] - rmed) > 0.08 * rmed else w[1], w[0][2]]), rmed if abs(w[1] - rmed) > 0.08 * rmed else w[1], w[2]) for w in wheels]
    if moto: wheels = [(w[0], w[1], max(w[2], 0.07)) for w in wheels]   # rim and spokes, not only the tread
    bone = np.zeros(len(Pd), np.int64)
    for i, (c, r, hw) in enumerate(wheels):
        d = np.hypot(Pd[:, 2] - c[2], Pd[:, 1] - c[1])
        inner = 0.0 if not moto else 0.45 * r        # moto: keep the hub/fork/swingarm static
        m = (d < 1.03 * r) & (d >= inner) & (np.abs(Pd[:, 0] - c[0]) < hw * 1.25) & (bone == 0)
        if moto and i == 0:
            # front fork legs go up and back from the hub: inside the tyre ring, that sector stays with the body
            ang = np.degrees(np.arctan2(Pd[:, 1] - c[1], Pd[:, 2] - c[2]))
            m &= ~((ang > 88) & (ang < 140) & (d < 0.86 * r))
        # whole triangles: a triangle belongs to the wheel only if all its corners do
        bone[m] = 2 + i
        print(f"  wheel {i}: centre ({c[0]:.2f}, {c[1]:.2f}, {c[2]:.2f}) radius {r:.2f} m, half width {hw:.2f} m, {int(m.sum())} vertices")
    tri_b = bone[Id]
    mixed = (tri_b.min(1) != tri_b.max(1))
    # split shared corners: triangles straddling the wheel boundary go to the body
    for t in np.where(mixed)[0]:
        for k in range(3):
            if bone[Id[t, k]] != 0:
                Pd = np.vstack([Pd, Pd[Id[t, k]]]); Nd = np.vstack([Nd, Nd[Id[t, k]]]); UVd = np.vstack([UVd, UVd[Id[t, k]]]); bone = np.append(bone, 0)
                Id[t, k] = len(Pd) - 1
    # seat: highest point of the middle section near the centre line, behind the middle (moto); car: driver seat
    if moto:
        mid = (np.abs(Pd[:, 0]) < 0.12 * W) & (Pd[:, 2] > -0.25 * L) & (Pd[:, 2] < 0.05 * L)
        seat = np.array([0.0, float(np.percentile(Pd[mid, 1], 85)) if mid.any() else 0.75, -0.08 * L])
    else:
        seat = np.array([0.35 * W * 0.5, 0.55 * H, -0.05 * L])
    joints = [np.zeros(3), seat] + [w[0] for w in wheels]
    out = os.path.join(HERE, "..", "assets", "models", "vehicles"); os.makedirs(out, exist_ok=True)
    path = os.path.join(out, a.name + ".lkm")
    nv = len(Pd)
    with open(path, "wb") as f:
        f.write(b"LKM2"); f.write(struct.pack("<IIIf", nv, Id.size, len(joints), float(H)))
        f.write(np.array(joints, "<f4").tobytes())
        f.write(Pd.astype("<f4").tobytes())
        f.write(np.clip(np.round(Nd * 127), -127, 127).astype(np.int8).tobytes())
        f.write(np.clip(np.round(np.stack([UVd[:, 0] % 1.0001, UVd[:, 1] % 1.0001], 1) * 65535), 0, 65535).astype("<u2").tobytes())
        # bone, bone, full weight, flag 0 (paint and chrome may read as metal: vehicles are metal)
        f.write(np.stack([bone, bone, np.full(nv, 255), np.zeros(nv)], 1).astype(np.uint8).tobytes())
        f.write(Id.astype("<u4").reshape(-1).tobytes())
    tex.resize((a.tex, a.tex), Image.LANCZOS).save(os.path.join(out, a.name + ".jpg"), quality=88, optimize=True)
    print(f"wrote {path} ({os.path.getsize(path) / 1e6:.2f} MB), seat ({seat[0]:.2f}, {seat[1]:.2f}, {seat[2]:.2f})")
    if a.preview:
        S = 520; img = Image.new("RGB", (S * 2, S // 2 + 40), (30, 30, 36)); d = ImageDraw.Draw(img)
        pal = [(170, 170, 180), (0, 0, 0), (255, 80, 60), (60, 200, 255), (255, 210, 60), (120, 255, 120)]
        k = S / max(L, W) * 0.9
        for vi, ax in enumerate((2, 0)):          # side view (z horizontal), front view (x horizontal)
            X = vi * S + S / 2 + (-Pd[:, ax] if ax == 2 else Pd[:, ax]) * k; Y = S // 2 + 10 - Pd[:, 1] * k
            depth = Pd[Id].mean(1)[:, 0 if ax == 2 else 2] * (1 if ax == 2 else -1)
            for t in np.argsort(depth):
                b = bone[Id[t]].max(); col = pal[min(b, 5)]
                d.polygon(list(zip(X[Id[t]], Y[Id[t]])), fill=col)
            for j in joints[1:]:
                jx = vi * S + S / 2 + (-j[ax] if ax == 2 else j[ax]) * k; jy = S // 2 + 10 - j[1] * k
                d.ellipse([jx - 4, jy - 4, jx + 4, jy + 4], fill=(255, 255, 255))
        img.save(a.preview); print("preview", a.preview)


if __name__ == "__main__":
    main()
