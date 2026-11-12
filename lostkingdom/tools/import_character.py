#!/usr/bin/env python3
"""LOST KINGDOM — import an AI-generated humanoid (GLB, T-pose, unrigged) into the game.

The generated models (Tripo, Meshy...) are static meshes. This tool:
  1. normalises the mesh (height in metres, feet on the ground, body axis at x = z = 0, facing +Z);
  2. finds the joints from the mesh itself (arm line, wrist, shoulders, hips, knees, ankles, neck);
  3. computes skin weights (2 bones per vertex) against the game's humanoid skeleton
     (same bones and hierarchy as game/Characters.cpp, so every existing animation plays on it);
  4. lowers the arms from the T-pose into the game's rest pose (arms down) with those weights;
  5. writes assets/models/<name>.lkm (compact binary, see engine/gfx/Model.h) and <name>.jpg.

Usage: python3 tools/import_character.py model.glb hero [--height 1.8] [--tex 1024] [--preview out.png]
"""
import argparse, io, json, os, struct, sys
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")

# Game humanoid skeleton (game/Characters.h enum HB) ---------------------------------------------
NAMES = ["root", "spine", "chest", "neck", "head", "shL", "uarmL", "larmL", "handL", "shR", "uarmR", "larmR", "handR",
         "thighL", "shinL", "footL", "thighR", "shinR", "footR", "propR", "propL"]
PARENT = [-1, 0, 1, 2, 3, 2, 5, 6, 7, 2, 9, 10, 11, 0, 13, 14, 0, 16, 17, 12, 8]
B = {n: i for i, n in enumerate(NAMES)}


def load_glb(path):
    b = open(path, "rb").read()
    if b[:4] != b"glTF":
        sys.exit("not a GLB file")
    off, ch = 12, []
    while off < len(b):
        cl, ct = struct.unpack("<I4s", b[off:off + 8]); ch.append((ct, b[off + 8:off + 8 + cl])); off += 8 + cl
    j = json.loads(ch[0][1]); binc = ch[1][1] if len(ch) > 1 else b""

    def acc(i):
        a = j["accessors"][i]; bv = j["bufferViews"][a["bufferView"]]
        n = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[a["type"]]
        dt = {5126: np.float32, 5123: np.uint16, 5125: np.uint32, 5121: np.uint8}[a["componentType"]]
        o = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
        stride = bv.get("byteStride", 0)
        if stride and stride != n * np.dtype(dt).itemsize:
            raw = np.frombuffer(binc, dtype=np.uint8, count=stride * a["count"], offset=o).reshape(a["count"], stride)
            r = raw[:, :n * np.dtype(dt).itemsize].copy().view(dt).reshape(a["count"], n)
        else:
            r = np.frombuffer(binc, dtype=dt, count=a["count"] * n, offset=o).reshape(a["count"], n)
        return r if n > 1 else r[:, 0]

    Ps, Ns, UVs, Is, base = [], [], [], [], 0
    tex = None
    for node in j.get("nodes", []):
        if "mesh" not in node:
            continue
        if any(k in node for k in ("matrix", "rotation", "scale")):
            print("warning: node transform ignored (", node.get("name"), ")")
        for pr in j["meshes"][node["mesh"]]["primitives"]:
            at = pr["attributes"]
            P = acc(at["POSITION"]).astype(np.float64)
            N = acc(at["NORMAL"]).astype(np.float64) if "NORMAL" in at else np.zeros_like(P)
            UV = acc(at["TEXCOORD_0"]).astype(np.float64) if "TEXCOORD_0" in at else np.zeros((len(P), 2))
            I = acc(pr["indices"]).astype(np.int64) if "indices" in pr else np.arange(len(P))
            Ps.append(P); Ns.append(N); UVs.append(UV); Is.append(I + base); base += len(P)
            if tex is None and "material" in pr:
                m = j["materials"][pr["material"]]
                t = m.get("pbrMetallicRoughness", {}).get("baseColorTexture")
                if t is not None:
                    img = j["images"][j["textures"][t["index"]]["source"]]
                    bv = j["bufferViews"][img["bufferView"]]
                    o = bv.get("byteOffset", 0)
                    tex = Image.open(io.BytesIO(binc[o:o + bv["byteLength"]])).convert("RGB")
    if not Ps:
        sys.exit("no mesh in file")
    P = np.concatenate(Ps); N = np.concatenate(Ns); UV = np.concatenate(UVs); I = np.concatenate(Is).reshape(-1, 3)
    return P, N, UV, I, tex


def seg_dist(p, a, b):
    ab = b - a
    t = np.clip(((p - a) @ ab) / max(ab @ ab, 1e-9), 0, 1)
    return np.linalg.norm(p - (a + t[:, None] * ab), axis=1)


def rot_z(angle):
    c, s = np.cos(angle), np.sin(angle)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("glb"); ap.add_argument("name")
    ap.add_argument("--height", type=float, default=1.8)
    ap.add_argument("--tex", type=int, default=1024)
    ap.add_argument("--preview", default="")
    a = ap.parse_args()

    P, N, UV, I, tex = load_glb(a.glb)
    # ---- normalise: feet on the ground, target height, body axis centred ------------------------
    P[:, 1] -= P[:, 1].min()
    H0 = P[:, 1].max()
    P *= a.height / H0
    H = a.height
    x, y, z = P[:, 0], P[:, 1], P[:, 2]
    legs = (y > 0.05 * H) & (y < 0.22 * H)
    P[:, 0] -= 0.5 * (x.max() + x.min())          # T-pose is symmetric: centre on the bounding box
    P[:, 2] -= np.median(z[legs])
    x, y, z = P[:, 0], P[:, 1], P[:, 2]
    N = N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)

    # ---- landmarks --------------------------------------------------------------------------------
    maxX = np.abs(x).max()
    if maxX < 0.6 * H * 0.5:
        print("warning: arms do not look horizontal (T-pose expected); weights may be poor")
    armBand = np.abs(x) > 0.55 * maxX
    yArm = np.median(y[armBand]); zArm = np.median(z[armBand])
    # torso edge: first |x| slice (from the centre) that contains nothing far below the arm line
    xEdge = maxX
    for xs in np.arange(0.05 * H, maxX, 0.01):
        m = (np.abs(x) >= xs) & (np.abs(x) < xs + 0.02)
        if m.sum() and y[m].min() > yArm - 0.12 * H:
            xEdge = xs; break
    chestBand = (y > yArm - 0.17 * H) & (y < yArm - 0.07 * H)
    shoulderX = min(xEdge * 0.85, np.percentile(np.abs(x[chestBand]), 95) + 0.02 * H)
    wristX = maxX - 0.085 * H
    elbowX = shoulderX + 0.5 * (wristX - shoulderX)
    lower = (y > 0.08 * H) & (y < 0.3 * H)
    legX = np.median(np.abs(x[lower])); legZ = np.median(z[lower])
    hipY, kneeY, ankleY = 0.51 * H, 0.27 * H, 0.06 * H
    rootY = hipY + 0.02 * H
    bodyZ = 0.5 * zArm + 0.5 * legZ               # spine axis between the arm line and the legs

    J = np.zeros((21, 3))
    J[B["root"]] = (0, rootY, bodyZ)
    J[B["spine"]] = (0, rootY + 0.065 * H, bodyZ)
    J[B["chest"]] = (0, rootY + 0.17 * H, bodyZ)
    J[B["neck"]] = (0, max(yArm + 0.05 * H, rootY + 0.28 * H), bodyZ - 0.01 * H)
    J[B["head"]] = (0, J[B["neck"]][1] + 0.055 * H, bodyZ)
    for s, side in ((1, "L"), (-1, "R")):
        J[B["sh" + side]] = (s * 0.045 * H, yArm + 0.02 * H, zArm * 0.5 + bodyZ * 0.5)
        J[B["uarm" + side]] = (s * shoulderX, yArm, zArm)
        J[B["larm" + side]] = (s * elbowX, yArm, zArm)
        J[B["hand" + side]] = (s * wristX, yArm, zArm)
        J[B["thigh" + side]] = (s * legX * 0.9, hipY, legZ)
        J[B["shin" + side]] = (s * legX, kneeY, legZ)
        J[B["foot" + side]] = (s * legX, ankleY, legZ - 0.01 * H)
    J[B["propL"]] = J[B["handL"]] + (0.04 * H, 0, 0.01 * H)
    J[B["propR"]] = J[B["handR"]] + (-0.04 * H, 0, 0.01 * H)
    for n in NAMES:
        print(f"  {n:7s} {np.round(J[B[n]], 3)}")

    # ---- bone segments (T-pose) and skin weights ---------------------------------------------------
    tips = {B["head"]: J[B["head"]] + (0, 0.12 * H, 0),
            B["handL"]: np.array([maxX, yArm, zArm]), B["handR"]: np.array([-maxX, yArm, zArm]),
            B["footL"]: J[B["footL"]] + (0, -0.04 * H, 0.09 * H), B["footR"]: J[B["footR"]] + (0, -0.04 * H, 0.09 * H)}
    child = {B["root"]: B["spine"], B["spine"]: B["chest"], B["chest"]: B["neck"], B["neck"]: B["head"],
             B["shL"]: B["uarmL"], B["uarmL"]: B["larmL"], B["larmL"]: B["handL"], B["shR"]: B["uarmR"], B["uarmR"]: B["larmR"], B["larmR"]: B["handR"],
             B["thighL"]: B["shinL"], B["shinL"]: B["footL"], B["thighR"]: B["shinR"], B["shinR"]: B["footR"]}
    skinBones = [i for i in range(21) if i not in (B["propL"], B["propR"])]
    D = np.full((len(P), 21), 1e9)
    for bi in skinBones:
        end = J[child[bi]] if bi in child else tips[bi]
        D[:, bi] = seg_dist(P, J[bi], end)
    # region rules ------------------------------------------------------------------------------
    big = 1e9
    left, right = x > 0, x <= 0
    armL = ["shL", "uarmL", "larmL", "handL"]; armR = ["shR", "uarmR", "larmR", "handR"]
    legL = ["thighL", "shinL", "footL"]; legR = ["thighR", "shinR", "footR"]
    for n in armL + legL: D[right, B[n]] = big
    for n in armR + legR: D[left, B[n]] = big
    inArm = np.abs(x) > shoulderX + 0.02 * H
    for n in ["root", "spine", "neck", "head"] + legL + legR: D[inArm, B[n]] = big
    notArm = np.abs(x) < shoulderX - 0.03 * H
    for n in ["larmL", "handL", "larmR", "handR"]: D[notArm, B[n]] = big
    for n in ["uarmL", "uarmR"]: D[np.abs(x) < shoulderX - 0.07 * H, B[n]] = big
    belowHips = y < hipY - 0.04 * H
    for n in ["chest", "neck", "head"] + armL + armR: D[belowHips, B[n]] = big
    aboveHips = y > rootY + 0.06 * H
    for n in legL + legR: D[aboveHips, B[n]] = big
    # cape / long coat behind the body: follows the torso, never the legs
    cape = (z < bodyZ - 0.075 * H) & (y > kneeY - 0.05 * H) & ~inArm
    for n in legL + legR + armL[1:] + armR[1:]: D[cape, B[n]] = big
    order = np.argsort(D, axis=1)[:, :2]
    d0 = D[np.arange(len(P)), order[:, 0]]; d1 = D[np.arange(len(P)), order[:, 1]]
    w0 = 1.0 / np.maximum(d0, 1e-4) ** 4; w1 = np.where(d1 < big / 2, 1.0 / np.maximum(d1, 1e-4) ** 4, 0)
    W0 = w0 / (w0 + w1)
    b0, b1 = order[:, 0], np.where(d1 < big / 2, order[:, 1], order[:, 0])

    # ---- T-pose -> rest pose (arms down) using the weights -------------------------------------------
    angle = np.pi / 2 - 0.08
    R = {B[n]: rot_z(-angle) for n in ["uarmL", "larmL", "handL", "propL"]}
    R.update({B[n]: rot_z(angle) for n in ["uarmR", "larmR", "handR", "propR"]})
    pivot = {}
    for s in "LR":
        for n in ["uarm", "larm", "hand", "prop"]:
            pivot[B[n + s]] = J[B["uarm" + s]]

    def pose(points, normals, bones):
        outP, outN = points.copy(), normals.copy()
        for bi, Rm in R.items():
            m = bones == bi
            if m.any():
                outP[m] = (points[m] - pivot[bi]) @ Rm.T + pivot[bi]
                outN[m] = normals[m] @ Rm.T
        return outP, outN

    P0, N0 = pose(P, N, b0)
    P1, N1 = pose(P, N, b1)
    Pb = P0 * W0[:, None] + P1 * (1 - W0[:, None])
    Nb = N0 * W0[:, None] + N1 * (1 - W0[:, None])
    Nb /= np.maximum(np.linalg.norm(Nb, axis=1, keepdims=True), 1e-9)
    Jb = J.copy()
    for bi in R:
        Jb[bi] = (J[bi] - pivot[bi]) @ R[bi].T + pivot[bi]

    # ---- write -------------------------------------------------------------------------------------------
    outDir = os.path.join(ROOT, "assets", "models"); os.makedirs(outDir, exist_ok=True)
    path = os.path.join(outDir, a.name + ".lkm")
    nv, ni = len(Pb), I.size
    with open(path, "wb") as f:
        f.write(b"LKM1"); f.write(struct.pack("<IIIf", nv, ni, 21, H))
        f.write(Jb.astype("<f4").tobytes())
        f.write(Pb.astype("<f4").tobytes())
        f.write(np.clip(np.round(Nb * 127), -127, 127).astype(np.int8).tobytes())
        uv16 = np.clip(np.round(np.stack([UV[:, 0] % 1.0001, UV[:, 1] % 1.0001], 1) * 65535), 0, 65535).astype("<u2")
        f.write(uv16.tobytes())
        f.write(np.stack([b0, b1, np.round(W0 * 255)], 1).astype(np.uint8).tobytes())
        f.write(I.astype("<u4").reshape(-1).tobytes())
    print(f"wrote {path}: {nv} vertices, {ni // 3} triangles, {os.path.getsize(path) / 1e6:.2f} MB")
    if tex is not None:
        tp = os.path.join(outDir, a.name + ".jpg")
        tex.resize((a.tex, a.tex), Image.LANCZOS).save(tp, quality=88, optimize=True)
        print(f"wrote {tp}: {a.tex}px, {os.path.getsize(tp) / 1e6:.2f} MB")

    if a.preview:
        # front & side views of the rest pose, coloured by dominant bone, with joints
        rng = np.random.default_rng(3); pal = (rng.random((21, 3)) * 200 + 40).astype(int)
        S = 520; img = Image.new("RGB", (S * 2, S), (30, 30, 36)); d = ImageDraw.Draw(img)
        for vi, (ax, sgn) in enumerate(((0, 1), (2, -1))):
            def to2(p): return (vi * S + S / 2 + sgn * p[..., ax] * S / H * 0.9, S - 20 - p[..., 1] * S / H * 0.9)
            depth = Pb[I].mean(1)[:, 2 if ax == 0 else 0] * (1 if ax == 0 else -1)
            for t in np.argsort(depth):
                tri = I[t]; X, Y = to2(Pb[tri])
                c = pal[np.bincount(b0[tri], minlength=21).argmax()]
                d.polygon(list(zip(X, Y)), fill=tuple(int(v) for v in c))
            for i in range(21):
                X, Y = to2(Jb[i]); d.ellipse([X - 3, Y - 3, X + 3, Y + 3], fill=(255, 255, 255))
                if PARENT[i] >= 0:
                    X2, Y2 = to2(Jb[PARENT[i]]); d.line([X, Y, X2, Y2], fill=(255, 255, 0), width=2)
        img.save(a.preview); print("preview", a.preview)


if __name__ == "__main__":
    main()
