#!/usr/bin/env python3
"""LOST KINGDOM — import an AI-generated horse (GLB, standing, unrigged) as a game quadruped.

The generated model (Tripo...) is a static mesh. This tool:
  1. turns it so the head faces +Z, puts the hooves on the ground and the saddle point at x = z = 0;
  2. scales it so the back (saddle) is at the height of the game's quadruped skeleton (0.72, the mount
     scale then makes it a real horse: 0.72 x 2.0 = 1.44 m at the saddle);
  3. places the 21 joints of the game's quadruped skeleton (game/Characters.cpp, enum QB) on the horse's
     anatomy (given in the model's own coordinates below, read off a side view of the mesh);
  4. computes skin weights (2 bones per vertex) along the mesh surface with region rules (each leg only
     moves its own hoof, the tail never binds to the hind legs);
  5. writes assets/models/<name>.lkm (engine/gfx/Model.h) and <name>.jpg.

Usage: python3 tools/import_quadruped.py horse.glb horse_mount [--preview out.png]
Anatomy defaults fit the Tripo horse delivered with 2.3.3 (head towards +X, 0.857 tall).
"""
import argparse, json, os, struct, sys
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
sys.path.insert(0, HERE)
from import_character import load_glb, seg_dist  # noqa: E402

NAMES = ["root", "spine", "chest", "neck", "head", "jaw", "tail1", "tail2", "tail3",
         "fl1", "fl2", "fl3", "fr1", "fr2", "fr3", "bl1", "bl2", "bl3", "br1", "br2", "br3"]
PARENT = [-1, 0, 1, 2, 3, 4, 0, 6, 7, 2, 9, 10, 2, 12, 13, 0, 15, 16, 0, 18, 19]
B = {n: i for i, n in enumerate(NAMES)}

# Anatomy in the SOURCE model frame: (forward along +X, up +Y). Legs get a lateral offset LEG_W.
ANAT = {
    "root": (-0.29, 0.52), "spine": (-0.07, 0.53), "chest": (0.12, 0.50),
    "neck": (0.21, 0.56), "head": (0.37, 0.74), "jaw": (0.45, 0.62),
    "tail1": (-0.39, 0.57), "tail2": (-0.44, 0.42), "tail3": (-0.46, 0.26),
    "fl1": (0.15, 0.42), "fl2": (0.15, 0.21), "fl3": (0.16, 0.06),
    "bl1": (-0.32, 0.43), "bl2": (-0.37, 0.23), "bl3": (-0.35, 0.06),
}
TIPS = {"jaw": (0.49, 0.58), "tail3": (-0.47, 0.10), "fl3": (0.17, 0.0), "fr3": (0.17, 0.0), "bl3": (-0.34, 0.0), "br3": (-0.34, 0.0)}
SADDLE_X, SADDLE_Y, LEG_W, BELLY_Y = -0.05, 0.60, 0.075, 0.36
SKEL_SADDLE = 0.72          # game quadruped root height (unit skeleton)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("glb"); ap.add_argument("name")
    ap.add_argument("--tex", type=int, default=512)
    ap.add_argument("--preview")
    ap.add_argument("--fwd", default="x", choices=["x", "-x", "z", "-z"], help="2.9.0 axis the animal's head points to in the source")
    ap.add_argument("--anat", help="2.9.0 JSON file: {anat:{bone:[fwd,up]}, tips:{...}, saddle:[fwd,up], leg_w, belly_y} for another animal")
    a = ap.parse_args()
    global ANAT, TIPS, SADDLE_X, SADDLE_Y, LEG_W, BELLY_Y
    head_above = None
    if a.anat:
        cfg = json.load(open(a.anat))
        head_above = cfg.get("head_above")   # 2.9.0 antlers / horns: everything above this height, ahead of the chest, rides the head
        ANAT = {k: tuple(v) for k, v in cfg["anat"].items()}; TIPS = {k: tuple(v) for k, v in cfg["tips"].items()}
        SADDLE_X, SADDLE_Y = cfg["saddle"]; LEG_W = cfg["leg_w"]; BELLY_Y = cfg["belly_y"]
    P, N, UV, I, tex = load_glb(a.glb)
    P = P.copy(); N = N.copy()
    # bring the source to the tool's frame: head towards +X, side along Z
    if a.fwd != "x":
        def turn(V):
            x, y, z = V[:, 0].copy(), V[:, 1].copy(), V[:, 2].copy()
            if a.fwd == "-x": return np.stack([-x, y, -z], 1)
            if a.fwd == "z": return np.stack([z, y, -x], 1)
            return np.stack([-z, y, x], 1)   # -z
        P = turn(P); N = turn(N)
    P[:, 1] -= P[:, 1].min()
    k = SKEL_SADDLE / SADDLE_Y

    def xf(x, y, z):   # source (x fwd, y up, z side) -> game (X side, Y up, Z fwd), recentred and scaled
        return np.array([-z, y, x - SADDLE_X]) * k

    Pg = np.stack([-P[:, 2], P[:, 1], P[:, 0] - SADDLE_X], 1) * k
    Ng = np.stack([-N[:, 2], N[:, 1], N[:, 0]], 1)
    J = np.zeros((21, 3)); tips = {}
    for n, (x, y) in ANAT.items():
        if n[0] in "fb" and n[1] in "l":
            J[B[n]] = xf(x, y, -LEG_W)                 # game left = +X = source -Z
            J[B[n.replace("l", "r", 1)]] = xf(x, y, LEG_W)
        else:
            J[B[n]] = xf(x, y, 0)
    for n, (x, y) in TIPS.items():
        side = 0 if n[0] not in "fb" else (-LEG_W if n[1] == "l" else LEG_W)
        tips[B[n]] = xf(x, y, side)
    H = Pg[:, 1].max()

    # ---- region-masked distance to each bone segment ---------------------------------------------------
    child = {}
    for i, p in enumerate(PARENT):
        if p >= 0 and p not in child and NAMES[i] not in ("tail1", "fl1", "fr1", "bl1", "br1", "jaw"):
            child[p] = i
    child[B["head"]] = B["jaw"]
    big = 1e6
    D = np.full((len(Pg), 21), big)
    for bi in range(21):
        end = J[child[bi]] if bi in child else tips.get(bi, J[bi] + np.array([0, -0.05, 0]))
        D[:, bi] = seg_dist(Pg, J[bi], end)
    X, Y, Z = Pg[:, 0], Pg[:, 1], Pg[:, 2]
    zMid = (J[B["fl1"], 2] + J[B["bl1"], 2]) * 0.5
    legs = {"fl": ["fl1", "fl2", "fl3"], "fr": ["fr1", "fr2", "fr3"], "bl": ["bl1", "bl2", "bl3"], "br": ["br1", "br2", "br3"]}
    low = Y < BELLY_Y * k
    tailZone = Z < J[B["tail1"], 2] - 0.04 * k
    for key, bones in legs.items():
        front, left = key[0] == "f", key[1] == "l"
        mine = low & ((Z > zMid) == front) & ((X > 0) == left)
        if not front: mine &= ~tailZone
        for n in bones: D[~mine & low, B[n]] = big        # a low vertex only follows its own leg
        for n in bones[1:]: D[~low, B[n]] = big           # the body never follows the shin / hoof
    body = [B[n] for n in ("root", "spine", "chest", "neck", "head", "jaw")]
    for bi in body: D[low & ~(Y > BELLY_Y * k - 0.03), bi] = big
    for n in ("tail1", "tail2", "tail3"): D[~tailZone, B[n]] = big
    for key, bones in legs.items():
        if key[0] == "b":
            for n in bones: D[tailZone & (np.abs(X) < 0.05 * k), B[n]] = big
    dead = (D >= big / 2).all(1)
    if dead.any():
        print(f"  {int(dead.sum())} vertices matched no rule, nearest bone")
        for bi in range(21):
            end = J[child[bi]] if bi in child else tips.get(bi, J[bi])
            D[dead, bi] = seg_dist(Pg[dead], J[bi], end)

    # ---- geodesic spread (as tools/import_character.py) ------------------------------------------------
    from scipy.sparse import coo_matrix
    from scipy.sparse.csgraph import dijkstra
    key = np.round(Pg / 1e-5).astype(np.int64)
    _, weld = np.unique(key, axis=0, return_inverse=True); weld = weld.reshape(-1)
    nW = weld.max() + 1
    e = np.concatenate([I[:, [0, 1]], I[:, [1, 2]], I[:, [2, 0]]]); e = weld[e]; e = e[e[:, 0] != e[:, 1]]
    Pw = np.zeros((nW, 3)); Pw[weld] = Pg
    lens = np.linalg.norm(Pw[e[:, 0]] - Pw[e[:, 1]], axis=1)
    graph = coo_matrix((np.concatenate([lens, lens]), (np.concatenate([e[:, 0], e[:, 1]]), np.concatenate([e[:, 1], e[:, 0]]))), shape=(nW, nW)).tocsr()
    Dw = np.full((nW, 21), big); np.minimum.at(Dw, weld, D)
    nearest = Dw.argmin(1); dnear = Dw.min(1)
    G = np.full((nW, 21), np.inf)
    for bi in range(21):
        r = 0.05 * H if bi not in body else 0.14 * H
        seeds = np.where((nearest == bi) & (dnear < r))[0]
        if len(seeds) == 0: seeds = np.where(nearest == bi)[0][:1]
        if len(seeds) == 0: continue
        G[:, bi] = dijkstra(graph, indices=seeds, min_only=True)
    G[Dw >= big / 2] = np.inf
    unreached = ~np.isfinite(G).any(1); G[unreached] = Dw[unreached]
    G = G[weld]
    order = np.argsort(G, axis=1)[:, :2]
    g0 = G[np.arange(len(Pg)), order[:, 0]]; g1 = G[np.arange(len(Pg)), order[:, 1]]
    blend = 0.06 * H
    w1 = np.where(np.isfinite(g1), 0.5 * np.clip(1 - (g1 - g0) / blend, 0, 1), 0)
    W0 = 1 - w1
    b0, b1 = order[:, 0], np.where(np.isfinite(g1), order[:, 1], order[:, 0])
    if head_above is not None:
        hm = (Pg[:, 1] > head_above * k) & (Pg[:, 2] > J[B["spine"], 2] - 0.1 * k)
        b0 = np.where(hm, B["head"], b0); b1 = np.where(hm, B["head"], b1); W0 = np.where(hm, 1.0, W0)
        print(f"  head_above: {int(hm.sum())} vertices ride the head")
    print(f"  skinning: {nW} welded nodes, {int(unreached.sum())} without surface path")
    for n in NAMES: print(f"   {n:6s} {int((b0 == B[n]).sum()):5d} verts  joint {np.round(J[B[n]], 3)}")

    outDir = os.path.join(ROOT, "assets", "models"); os.makedirs(outDir, exist_ok=True)
    path = os.path.join(outDir, a.name + ".lkm")
    nv, ni = len(Pg), I.size
    with open(path, "wb") as f:
        f.write(b"LKM2"); f.write(struct.pack("<IIIf", nv, ni, 21, H))
        f.write(J.astype("<f4").tobytes())
        f.write(Pg.astype("<f4").tobytes())
        Ng /= np.maximum(np.linalg.norm(Ng, axis=1, keepdims=True), 1e-9)
        f.write(np.clip(np.round(Ng * 127), -127, 127).astype(np.int8).tobytes())
        f.write(np.clip(np.round(np.stack([UV[:, 0] % 1.0001, UV[:, 1] % 1.0001], 1) * 65535), 0, 65535).astype("<u2").tobytes())
        f.write(np.stack([b0, b1, np.round(W0 * 255), np.full(nv, 255)], 1).astype(np.uint8).tobytes())   # 255: never metal
        f.write(I.astype("<u4").reshape(-1).tobytes())
    print(f"wrote {path}: {nv} vertices, {ni // 3} triangles, {os.path.getsize(path) / 1e6:.2f} MB, height {H:.3f}")
    if tex is not None:
        tp = os.path.join(outDir, a.name + ".jpg")
        tex.resize((a.tex, a.tex), Image.LANCZOS).save(tp, quality=90, optimize=True)
        print(f"wrote {tp}")

    if a.preview:
        rng = np.random.default_rng(5); pal = (rng.random((21, 3)) * 200 + 40).astype(int)
        S = 560; img = Image.new("RGB", (S * 2, S), (30, 30, 36)); d = ImageDraw.Draw(img)
        sc = S / 1.6
        for vi, (ax, dep) in enumerate(((2, 0), (0, 2))):
            def to2(p): return (vi * S + S / 2 + p[..., ax] * sc, S - 20 - p[..., 1] * sc)
            depth = Pg[I].mean(1)[:, dep]
            for t in np.argsort(depth):
                tri = I[t]; X2, Y2 = to2(Pg[tri])
                d.polygon(list(zip(X2, Y2)), fill=tuple(int(v) for v in pal[np.bincount(b0[tri], minlength=21).argmax()]))
            for i in range(21):
                X2, Y2 = to2(J[i]); d.ellipse([X2 - 3, Y2 - 3, X2 + 3, Y2 + 3], fill=(255, 255, 255))
                if PARENT[i] >= 0:
                    X3, Y3 = to2(J[PARENT[i]]); d.line([X2, Y2, X3, Y3], fill=(255, 255, 0), width=2)
        img.save(a.preview); print("preview", a.preview)


if __name__ == "__main__":
    main()
