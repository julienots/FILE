#!/usr/bin/env python3
"""LOST KINGDOM — import a RIGGED humanoid GLB (Tripo auto-rig, Mixamo-like skeleton) into the game.

Unlike tools/import_character.py (static T-pose meshes, weights guessed from the geometry), this tool
reuses the model's own skin:
  1. reads the mesh, its skin (JOINTS_0 / WEIGHTS_0) and the bind pose of its skeleton;
  2. re-poses the bind pose into the game's rest pose with real linear-blend skinning
     (arms hanging along the body, legs straight under the hips, feet flat);
  3. simplifies the mesh to a mobile budget (quadric edge collapse that keeps the UV seams),
     then carries the skin weights over from the nearest full-resolution vertex;
  4. maps the model's bones to the game's 21-bone humanoid skeleton (game/Characters.h), keeping the two
     strongest influences, so every animation of the game plays on it;
  5. writes assets/models/<name>.lkm (LKM2, see engine/gfx/Model.h) and <name>.jpg (base colour).

The model's own animation clips are NOT imported: the game animates its skeleton procedurally.

Usage: python3 -I tools/import_rigged.py model.glb hero_ninja [--height 1.8] [--tris 22000] [--tex 1024] [--preview out.png]
Needs numpy, scipy, pillow and pymeshlab.
"""
import argparse, io, json, os, re, struct, sys
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")

NAMES = ["root", "spine", "chest", "neck", "head", "shL", "uarmL", "larmL", "handL", "shR", "uarmR", "larmR", "handR",
         "thighL", "shinL", "footL", "thighR", "shinR", "footR", "propR", "propL"]
PARENT = [-1, 0, 1, 2, 3, 2, 5, 6, 7, 2, 9, 10, 11, 0, 13, 14, 0, 16, 17, 12, 8]
B = {n: i for i, n in enumerate(NAMES)}

# model bone name (regex, case-insensitive, L_/R_ already replaced by the side) -> game bone
MAP = [
    (r"^(boneroot|hips?|pelvis|root)$", "root"),
    (r"^(waist|spine)$", "spine"),
    (r"^(spine0?1|spine0?2|spine2|chest|upperchest)$", "chest"),
    (r"^neck", "neck"),
    (r"^(head|facialbone|.*eye|jaw|headtop.*)$", "head"),
    (r"^S_clavicle$|^S_shoulder$", "sh"),
    (r"^S_upperarm", "uarm"),
    (r"^S_(forearm|elbow|lowerarm)", "larm"),
    (r"^S_(hand|thumb|index|mid|ring|pinky)", "hand"),
    (r"^S_(thigh|upleg)", "thigh"),
    (r"^S_(calf|knee|leg)", "shin"),
    (r"^S_(foot|toe)", "foot"),
]


def load(path):
    b = open(path, "rb").read()
    if b[:4] != b"glTF":
        sys.exit("not a GLB file")
    off, ch = 12, []
    while off < len(b):
        cl, ct = struct.unpack("<I4s", b[off:off + 8]); ch.append((ct, b[off + 8:off + 8 + cl])); off += 8 + cl
    j = json.loads(ch[0][1]); binc = ch[1][1]

    def acc(i, norm=False):
        a = j["accessors"][i]; bv = j["bufferViews"][a["bufferView"]]
        n = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[a["type"]]
        dt = {5126: np.float32, 5123: np.uint16, 5125: np.uint32, 5121: np.uint8}[a["componentType"]]
        o = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
        size = n * np.dtype(dt).itemsize; stride = bv.get("byteStride", 0)
        if stride and stride != size:
            raw = np.frombuffer(binc, np.uint8, stride * a["count"], o).reshape(a["count"], stride)
            r = raw[:, :size].copy().view(dt).reshape(a["count"], n)
        else:
            r = np.frombuffer(binc, dt, a["count"] * n, o).reshape(a["count"], n)
        if norm and dt != np.float32:
            r = r.astype(np.float64) / np.iinfo(dt).max
        return r if n > 1 else r[:, 0]

    node = next(n for n in j["nodes"] if "mesh" in n and "skin" in n)
    prims = j["meshes"][node["mesh"]]["primitives"]
    Ps, Ns, UVs, Js, Ws, Is, base = [], [], [], [], [], [], 0
    for pr in prims:
        at = pr["attributes"]
        P = acc(at["POSITION"]).astype(np.float64)
        Ps.append(P); Ns.append(acc(at["NORMAL"]).astype(np.float64))
        UVs.append(acc(at["TEXCOORD_0"]).astype(np.float64))
        Js.append(acc(at["JOINTS_0"]).astype(np.int64)); Ws.append(acc(at["WEIGHTS_0"], True).astype(np.float64))
        Is.append(acc(pr["indices"]).astype(np.int64) + base); base += len(P)
    m = j["materials"][prims[0]["material"]]
    t = m["pbrMetallicRoughness"]["baseColorTexture"]["index"]
    img = j["images"][j["textures"][t]["source"]]; bv = j["bufferViews"][img["bufferView"]]
    tex = Image.open(io.BytesIO(binc[bv.get("byteOffset", 0):bv.get("byteOffset", 0) + bv["byteLength"]])).convert("RGB")
    sk = j["skins"][node["skin"]]
    ibm = acc(sk["inverseBindMatrices"]).reshape(-1, 4, 4).transpose(0, 2, 1).astype(np.float64)   # column-major
    names = [j["nodes"][ji].get("name", "") for ji in sk["joints"]]
    parentOf = {c: p for p, n in enumerate(j["nodes"]) for c in n.get("children", [])}
    jointIdx = {ji: k for k, ji in enumerate(sk["joints"])}
    parents = [jointIdx.get(parentOf.get(ji, -1), -1) for ji in sk["joints"]]
    W = np.concatenate(Ws); W /= np.maximum(W.sum(1, keepdims=True), 1e-9)
    return (np.concatenate(Ps), np.concatenate(Ns), np.concatenate(UVs), np.concatenate(Js), W,
            np.concatenate(Is).reshape(-1, 3), tex, names, parents, np.linalg.inv(ibm))


def game_bone(name):
    n = name.strip()
    side = ""
    mm = re.match(r"^(L|R|Left|Right)[_ ]?(.*)$", n)
    if mm and mm.group(2):
        side = "L" if mm.group(1).startswith("L") else "R"; n = "S_" + mm.group(2)
    for pat, g in MAP:
        if re.match(pat, n, re.I):
            return B[g] if g in B else (B[g + side] if side else None)
    return None


def rot_between(a, b):
    a = a / np.linalg.norm(a); b = b / np.linalg.norm(b)
    v = np.cross(a, b); c = float(a @ b)
    if np.linalg.norm(v) < 1e-9:
        return np.eye(3)
    vx = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
    return np.eye(3) + vx + vx @ vx * (1 / (1 + c))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("glb"); ap.add_argument("name")
    ap.add_argument("--height", type=float, default=1.8)
    ap.add_argument("--tris", type=int, default=22000)
    ap.add_argument("--tex", type=int, default=1024)
    ap.add_argument("--arm-out", type=float, default=0.10, help="arm angle from the vertical in the rest pose (rad)")
    ap.add_argument("--preview", default="")
    a = ap.parse_args()

    P, N, UV, Jt, Wt, I, tex, names, parents, bindW = load(a.glb)
    nb = len(names)
    print(f"model: {len(P)} vertices, {len(I)} triangles, {nb} bones")
    jp = bindW[:, :3, 3].copy()                      # bind-pose joint positions
    k = {n: i for i, n in enumerate(names)}

    def find(*cands):
        for c in cands:
            for n, i in k.items():
                if n.lower() == c.lower():
                    return i
        sys.exit("bone not found: " + "/".join(cands))

    # ---- re-pose the bind pose into the game's rest pose (world-space deltas per joint) ---------------
    Q = {}   # joint -> extra world rotation about the joint, applied after its parent's delta
    D = [None] * nb

    order = []
    seen = set()
    def visit(i):
        if i in seen: return
        if parents[i] >= 0: visit(parents[i])
        seen.add(i); order.append(i)
    for i in range(nb): visit(i)

    def solve():
        for i in order:
            p = parents[i]
            Dp = D[p] if p >= 0 else np.eye(4)
            M = Dp.copy()
            if i in Q:
                pos = (Dp @ np.append(jp[i], 1))[:3]
                T = np.eye(4); T[:3, :3] = Q[i]; T[:3, 3] = pos - Q[i] @ pos
                M = T @ Dp
            D[i] = M

    def posed(i):
        return (D[i] @ np.append(jp[i], 1))[:3]

    chains = []
    for s, sgn in (("L", 1), ("R", -1)):
        ua, fa, hd = find(s + "_Upperarm", s + "UpperArm"), find(s + "_Forearm", s + "ForeArm"), find(s + "_Hand", s + "Hand")
        th, ca, ft = find(s + "_Thigh", s + "UpLeg"), find(s + "_Calf", s + "Leg"), find(s + "_Foot", s + "Foot")
        armDir = np.array([sgn * np.sin(a.arm_out), -np.cos(a.arm_out), 0.0])
        chains.append((ua, fa, armDir)); chains.append((fa, hd, armDir))
        chains.append((th, ca, np.array([0, -1.0, 0]))); chains.append((ca, ft, np.array([0, -1.0, 0])))
        chains.append(("flat", ft, None))
    for c in chains:
        solve()
        if c[0] == "flat":
            ft = c[1]                                    # undo the leg rotation on the foot: sole stays flat
            Q[ft] = np.linalg.inv(D[parents[ft]][:3, :3]) if parents[ft] >= 0 else np.eye(3)
            continue
        j0, j1, target = c
        cur = posed(j1) - posed(j0)
        Q[j0] = rot_between(cur, target) @ Q.get(j0, np.eye(3))
    solve()
    # skinning (linear blend, 4 influences)
    Dm = np.stack(D)                                     # nb x 4 x 4
    Ph = np.concatenate([P, np.ones((len(P), 1))], 1)
    Pp = np.zeros_like(P); Np = np.zeros_like(N)
    for c in range(Jt.shape[1]):
        Mi = Dm[Jt[:, c]]
        Pp += Wt[:, c:c + 1] * np.einsum("vij,vj->vi", Mi, Ph)[:, :3]
        Np += Wt[:, c:c + 1] * np.einsum("vij,vj->vi", Mi[:, :3, :3], N)
    Jp = np.array([posed(i) for i in range(nb)])

    # ---- normalise: feet on the ground, target height, hips over the origin ------------------------------
    hip = Jp[find("Hip", "Hips")]
    Pp[:, 0] -= hip[0]; Pp[:, 2] -= hip[2]; Jp[:, 0] -= hip[0]; Jp[:, 2] -= hip[2]
    y0 = Pp[:, 1].min(); Pp[:, 1] -= y0; Jp[:, 1] -= y0
    s = a.height / Pp[:, 1].max(); Pp *= s; Jp *= s
    H = a.height

    # ---- game bones -----------------------------------------------------------------------------------
    g_of = np.array([game_bone(n) if game_bone(n) is not None else -1 for n in names])
    for i in range(nb):                                  # unmapped bones inherit their parent's game bone
        p = i
        while g_of[p] < 0 and parents[p] >= 0: p = parents[p]
        g_of[i] = g_of[p] if g_of[p] >= 0 else B["root"]
    G = np.zeros((len(P), 21))
    for c in range(Jt.shape[1]):
        np.add.at(G, (np.arange(len(P)), g_of[Jt[:, c]]), Wt[:, c])
    J = np.zeros((21, 3))
    pick = {"root": ("Hip", "Hips"), "spine": ("Waist", "Spine"), "chest": ("Spine01", "Spine1"), "neck": ("NeckTwist01", "Neck"), "head": ("Head",)}
    for g, c in pick.items(): J[B[g]] = Jp[find(*c)]
    for sd in "LR":
        J[B["sh" + sd]] = Jp[find(sd + "_Clavicle", sd + "Shoulder")]
        J[B["uarm" + sd]] = Jp[find(sd + "_Upperarm", sd + "UpperArm")]
        J[B["larm" + sd]] = Jp[find(sd + "_Forearm", sd + "ForeArm")]
        J[B["hand" + sd]] = Jp[find(sd + "_Hand", sd + "Hand")]
        J[B["thigh" + sd]] = Jp[find(sd + "_Thigh", sd + "UpLeg")]
        J[B["shin" + sd]] = Jp[find(sd + "_Calf", sd + "Leg")]
        J[B["foot" + sd]] = Jp[find(sd + "_Foot", sd + "Foot")]
        try:
            palm = Jp[find(sd + "_Mid1", sd + "HandMiddle1")]
        except SystemExit:
            palm = J[B["hand" + sd]] + (J[B["hand" + sd]] - J[B["larm" + sd]]) * 0.35
        J[B["prop" + sd]] = 0.55 * palm + 0.45 * J[B["hand" + sd]]
    for n in NAMES:
        print(f"  {n:7s} {np.round(J[B[n]], 3)}")

    # ---- simplify -----------------------------------------------------------------------------------
    import pymeshlab
    from scipy.spatial import cKDTree
    ms = pymeshlab.MeshSet()
    ms.add_mesh(pymeshlab.Mesh(vertex_matrix=Pp, face_matrix=I.astype(np.int32), v_tex_coords_matrix=UV))
    ms.compute_texcoord_transfer_vertex_to_wedge()
    ms.meshing_merge_close_vertices(threshold=pymeshlab.PercentageValue(0.001))   # weld UV-seam duplicates: one surface
    ms.meshing_remove_duplicate_faces(); ms.meshing_remove_null_faces()
    ms.meshing_decimation_quadric_edge_collapse_with_texture(targetfacenum=a.tris, qualitythr=0.4, extratcoordw=1.0,
                                                             preserveboundary=True, boundaryweight=1.0, optimalplacement=True,
                                                             preservenormal=True, planarquadric=True)
    mm = ms.current_mesh()
    V2 = mm.vertex_matrix(); F2 = mm.face_matrix().astype(np.int64); WUV = mm.wedge_tex_coord_matrix().reshape(-1, 3, 2)
    # smooth normals on the welded surface
    fn = np.cross(V2[F2[:, 1]] - V2[F2[:, 0]], V2[F2[:, 2]] - V2[F2[:, 0]])
    VN = np.zeros_like(V2)
    for c in range(3): np.add.at(VN, F2[:, c], fn)
    VN /= np.maximum(np.linalg.norm(VN, axis=1, keepdims=True), 1e-12)
    # split corners by (vertex, uv) -> render vertices
    corner_v = F2.reshape(-1); corner_uv = WUV.reshape(-1, 2)
    ck = np.concatenate([corner_v[:, None].astype(np.float64), np.round(corner_uv * 65535)], 1)
    _, cfirst, cinv = np.unique(ck, axis=0, return_index=True, return_inverse=True); cinv = cinv.reshape(-1)
    vid = corner_v[cfirst]
    Pf, Nf, UVf, If = V2[vid], VN[vid], corner_uv[cfirst], cinv.reshape(-1, 3)
    # skin weights from the nearest full-resolution vertex
    _, nn = cKDTree(Pp).query(Pf, k=3)
    Gf = G[nn].mean(1)
    print(f"simplified: {len(Pf)} vertices, {len(If)} triangles")

    order2 = np.argsort(-Gf, axis=1)[:, :2]
    w0 = Gf[np.arange(len(Pf)), order2[:, 0]]; w1 = Gf[np.arange(len(Pf)), order2[:, 1]]
    W0 = w0 / np.maximum(w0 + w1, 1e-9)
    b0, b1 = order2[:, 0], np.where(w1 > 1e-4, order2[:, 1], order2[:, 0])
    # The ninja's outfit is cloth and leather: no steel shading, except on light grey hard parts (blades).
    texArr = np.asarray(tex.resize((512, 512)), np.float32) / 255
    u = np.clip((UVf[:, 0] % 1) * 511, 0, 511).astype(int); v = np.clip((UVf[:, 1] % 1) * 511, 0, 511).astype(int)
    col = texArr[v, u]; sat = col.max(1) - col.min(1); lum = col.mean(1)
    cloth = ~((sat < 0.06) & (lum > 0.55))

    outDir = os.path.join(ROOT, "assets", "models"); os.makedirs(outDir, exist_ok=True)
    path = os.path.join(outDir, a.name + ".lkm")
    with open(path, "wb") as f:
        f.write(b"LKM2"); f.write(struct.pack("<IIIf", len(Pf), If.size, 21, H))
        f.write(J.astype("<f4").tobytes())
        f.write(Pf.astype("<f4").tobytes())
        f.write(np.clip(np.round(Nf * 127), -127, 127).astype(np.int8).tobytes())
        f.write(np.clip(np.round(np.stack([UVf[:, 0] % 1.0001, UVf[:, 1] % 1.0001], 1) * 65535), 0, 65535).astype("<u2").tobytes())
        f.write(np.stack([b0, b1, np.round(W0 * 255), np.where(cloth, 255, 0)], 1).astype(np.uint8).tobytes())
        f.write(If.astype("<u4").reshape(-1).tobytes())
    print(f"wrote {path}: {os.path.getsize(path) / 1e6:.2f} MB, cloth {cloth.mean() * 100:.0f}%")
    tp = os.path.join(outDir, a.name + ".jpg")
    tex.resize((a.tex, a.tex), Image.LANCZOS).save(tp, quality=88, optimize=True)
    print(f"wrote {tp}: {os.path.getsize(tp) / 1e6:.2f} MB")

    if a.preview:
        rng = np.random.default_rng(3); pal = (rng.random((21, 3)) * 200 + 40).astype(int)
        S = 520; img = Image.new("RGB", (S * 3, S), (30, 30, 36)); d = ImageDraw.Draw(img)
        texs = np.asarray(tex.resize((512, 512)))
        for vi, (ax, sgn, mode) in enumerate(((0, 1, "tex"), (0, 1, "bone"), (2, -1, "bone"))):
            def to2(p): return (vi * S + S / 2 + sgn * p[..., ax] * S / H * 0.9, S - 20 - p[..., 1] * S / H * 0.9)
            depth = Pf[If].mean(1)[:, 2 if ax == 0 else 0] * (1 if ax == 0 else -1)
            for t in np.argsort(depth):
                tri = If[t]; X, Y = to2(Pf[tri])
                if mode == "tex":
                    uv = UVf[tri].mean(0); c = texs[int(np.clip((1 - uv[1] if False else uv[1]) % 1 * 511, 0, 511)), int(uv[0] % 1 * 511)]
                    shade = 0.55 + 0.45 * max(0.0, float(Nf[tri].mean(0) @ np.array([0.3, 0.5, 0.8])))
                    c = tuple(int(x * shade) for x in c)
                else:
                    c = tuple(int(x) for x in pal[np.bincount(b0[tri], minlength=21).argmax()])
                d.polygon(list(zip(X, Y)), fill=c)
            if mode == "bone":
                for i in range(21):
                    X, Y = to2(J[i]); d.ellipse([X - 3, Y - 3, X + 3, Y + 3], fill=(255, 255, 255))
                    if PARENT[i] >= 0:
                        X2, Y2 = to2(J[PARENT[i]]); d.line([X, Y, X2, Y2], fill=(255, 255, 0), width=2)
        img.save(a.preview); print("preview", a.preview)


if __name__ == "__main__":
    main()
