#!/usr/bin/env python3
"""LOST KINGDOM — shrink a heavy static GLB (Tripo exports are often ~2 million triangles) before
tools/import_character.py.

  1. welds the UV-seam duplicates and keeps the main body: disconnected pieces that float away from it
     (orbiting planets, sparkles...) are dropped, pieces touching the body are kept;
  2. optionally replaces a bent leg with the mirror image of the straight one (--mirror-leg L copies the
     character's left leg, +X, onto the right one below --mirror-below x height): Tripo often generates
     heroes "floating" with one knee raised, which the game's walk cannot use;
  3. quadric edge collapse down to --tris triangles, keeping the UV seams;
  4. writes a plain GLB (positions, normals, UVs, indices, base colour JPEG) that import_character.py reads.

Usage: python3 -I tools/simplify_glb.py in.glb out.glb [--tris 20000] [--keep-floating]
Needs numpy, scipy, pillow and pymeshlab.
"""
import argparse, io, json, struct, sys
import numpy as np
from PIL import Image


def load(path):
    b = open(path, "rb").read()
    off, ch = 12, []
    while off < len(b):
        cl, ct = struct.unpack("<I4s", b[off:off + 8]); ch.append(b[off + 8:off + 8 + cl]); off += 8 + cl
    j = json.loads(ch[0]); binc = ch[1]

    def acc(i):
        a = j["accessors"][i]; bv = j["bufferViews"][a["bufferView"]]
        n = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[a["type"]]
        dt = {5126: np.float32, 5123: np.uint16, 5125: np.uint32, 5121: np.uint8, 5122: np.int16, 5120: np.int8}[a["componentType"]]
        o = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
        size = n * np.dtype(dt).itemsize; stride = bv.get("byteStride", 0)
        if stride and stride != size:   # interleaved vertex buffers (gltf-transform, gltfpack...)
            raw = np.frombuffer(binc, np.uint8, stride * (a["count"] - 1) + size, o)
            raw = np.lib.stride_tricks.as_strided(raw, shape=(a["count"], size), strides=(stride, 1))
            r = raw.copy().view(dt).reshape(a["count"], n)
        else:
            r = np.frombuffer(binc, dt, a["count"] * n, o).reshape(a["count"], n)
        if a.get("normalized") and dt != np.float32:
            r = np.maximum(r.astype(np.float64) / np.iinfo(dt).max, -1.0)
        return r if n > 1 else r[:, 0]

    Ps, UVs, Is, base, tex = [], [], [], 0, None
    for node in j["nodes"]:
        if "mesh" not in node:
            continue
        for pr in j["meshes"][node["mesh"]]["primitives"]:
            P = acc(pr["attributes"]["POSITION"]).astype(np.float64)
            Ps.append(P); UVs.append(acc(pr["attributes"]["TEXCOORD_0"]).astype(np.float64))
            Is.append(acc(pr["indices"]).astype(np.int64) + base); base += len(P)
            if tex is None and "material" in pr:
                t = j["materials"][pr["material"]].get("pbrMetallicRoughness", {}).get("baseColorTexture")
                if t is not None:
                    im = j["images"][j["textures"][t["index"]]["source"]]; bv = j["bufferViews"][im["bufferView"]]
                    o = bv.get("byteOffset", 0); tex = Image.open(io.BytesIO(binc[o:o + bv["byteLength"]])).convert("RGB")
    return np.concatenate(Ps), np.concatenate(UVs), np.concatenate(Is).reshape(-1, 3), tex


def write_glb(path, P, N, UV, I, tex_jpg):
    bins, views, accs = b"", [], []

    def add(arr, target, typ, ctype, minmax=False):
        nonlocal bins
        data = arr.tobytes(); pad = (-len(bins)) % 4; bins += b"\0" * pad
        views.append({"buffer": 0, "byteOffset": len(bins), "byteLength": len(data), **({"target": target} if target else {})})
        bins += data
        a = {"bufferView": len(views) - 1, "componentType": ctype, "count": len(arr), "type": typ}
        if minmax: a["min"] = arr.min(0).tolist(); a["max"] = arr.max(0).tolist()
        accs.append(a); return len(accs) - 1

    ip = add(P.astype("<f4"), 34962, "VEC3", 5126, True); inn = add(N.astype("<f4"), 34962, "VEC3", 5126)
    iu = add(UV.astype("<f4"), 34962, "VEC2", 5126); ii = add(I.astype("<u4").reshape(-1), 34963, "SCALAR", 5125)
    pad = (-len(bins)) % 4; bins += b"\0" * pad
    views.append({"buffer": 0, "byteOffset": len(bins), "byteLength": len(tex_jpg)}); bins += tex_jpg
    pad = (-len(bins)) % 4; bins += b"\0" * pad
    j = {"asset": {"version": "2.0", "generator": "lostkingdom simplify_glb"}, "scene": 0, "scenes": [{"nodes": [0]}],
         "nodes": [{"mesh": 0}], "meshes": [{"primitives": [{"attributes": {"POSITION": ip, "NORMAL": inn, "TEXCOORD_0": iu}, "indices": ii, "material": 0}]}],
         "materials": [{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}], "textures": [{"source": 0}],
         "images": [{"bufferView": len(views) - 1, "mimeType": "image/jpeg"}],
         "buffers": [{"byteLength": len(bins)}], "bufferViews": views, "accessors": accs}
    js = json.dumps(j).encode(); js += b" " * ((-len(js)) % 4)
    with open(path, "wb") as f:
        f.write(struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(js) + 8 + len(bins)))
        f.write(struct.pack("<I4s", len(js), b"JSON")); f.write(js)
        f.write(struct.pack("<I4s", len(bins), b"BIN\0")); f.write(bins)


def drop_floating(P, I):
    """Keeps the main piece and the pieces touching it (see the module docstring)."""
    # ---- weld and split into connected pieces
    from scipy.sparse import coo_matrix
    from scipy.sparse.csgraph import connected_components
    key = np.round(P / 1e-5).astype(np.int64)
    _, weld = np.unique(key, axis=0, return_inverse=True); weld = weld.reshape(-1)
    Fw = weld[I]; n = weld.max() + 1
    e = np.concatenate([Fw[:, [0, 1]], Fw[:, [1, 2]]])
    ncomp, lab = connected_components(coo_matrix((np.ones(len(e)), (e[:, 0], e[:, 1])), shape=(n, n)), directed=False)
    faceLab = lab[Fw[:, 0]]
    if ncomp > 1:
        counts = np.bincount(faceLab, minlength=ncomp)
        main = counts.argmax()
        from scipy.spatial import cKDTree
        mv = np.unique(Fw[faceLab == main]); Pw = np.zeros((n, 3)); Pw[weld] = P
        H = Pw[mv, 1].max() - Pw[mv, 1].min()
        tree = cKDTree(Pw[mv])
        # a piece is kept when it touches the body (a belt, a crown, separate hair spikes),
        # dropped when it floats away from it (orbiting planets, sparkles)
        keep = np.zeros(ncomp, bool); keep[main] = True
        for c in range(ncomp):
            if c == main or counts[c] == 0: continue
            cv = np.unique(Fw[faceLab == c])
            d = tree.query(Pw[cv[:: max(1, len(cv) // 400)]])[0].min()
            keep[c] = d < 0.03 * H
            print(f"  piece {c}: {counts[c]} triangles, {d / H * 100:.1f}% of the height from the body -> {'kept' if keep[c] else 'dropped'}")
        kf = keep[faceLab]
        print(f"pieces: {ncomp}, kept {int(keep.sum())}, dropped {int((~kf).sum())} floating triangles")
        I = I[kf]
    return I

def decimate(P, UV, I, tris):
    """Quadric edge collapse keeping the UV seams; returns render vertices (P, N, UV) and triangles."""
    import pymeshlab
    ms = pymeshlab.MeshSet()
    ms.add_mesh(pymeshlab.Mesh(vertex_matrix=P, face_matrix=I.astype(np.int32), v_tex_coords_matrix=UV))
    ms.compute_texcoord_transfer_vertex_to_wedge()
    ms.meshing_merge_close_vertices(threshold=pymeshlab.PercentageValue(0.001))
    ms.meshing_remove_duplicate_faces(); ms.meshing_remove_null_faces(); ms.meshing_remove_unreferenced_vertices()
    ms.meshing_decimation_quadric_edge_collapse_with_texture(targetfacenum=tris, qualitythr=0.4, extratcoordw=1.0, preserveboundary=True,
                                                             boundaryweight=1.0, optimalplacement=True, preservenormal=True, planarquadric=True)
    m = ms.current_mesh()
    V = m.vertex_matrix(); F = m.face_matrix().astype(np.int64); W = m.wedge_tex_coord_matrix().reshape(-1, 2)
    fn = np.cross(V[F[:, 1]] - V[F[:, 0]], V[F[:, 2]] - V[F[:, 0]])
    VN = np.zeros_like(V)
    for c in range(3): np.add.at(VN, F[:, c], fn)
    VN /= np.maximum(np.linalg.norm(VN, axis=1, keepdims=True), 1e-12)
    cv = F.reshape(-1)
    ck = np.concatenate([cv[:, None].astype(np.float64), np.round(W * 65535)], 1)
    _, first, inv = np.unique(ck, axis=0, return_index=True, return_inverse=True)
    vid = cv[first]
    return V[vid], VN[vid], W[first], inv.reshape(-1).reshape(-1, 3)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("glb"); ap.add_argument("out")
    ap.add_argument("--tris", type=int, default=20000)
    ap.add_argument("--keep-floating", action="store_true")
    ap.add_argument("--mirror-leg", choices=["L", "R"], default=None, help="copy this leg (L = +X) onto the other one")
    ap.add_argument("--mirror-below", type=float, default=0.34, help="height fraction below which the leg is replaced")
    ap.add_argument("--tex", type=int, default=1024)
    a = ap.parse_args()
    P, UV, I, tex = load(a.glb)
    print(f"input: {len(P)} vertices, {len(I)} triangles")
    if not a.keep_floating:
        I = drop_floating(P, I)
    if a.mirror_leg:
        used = P[np.unique(I)]                                   # the kept body only (not the dropped pieces)
        y0 = used[:, 1].min(); H = used[:, 1].max() - y0; cx = 0.5 * (used[:, 0].max() + used[:, 0].min())
        x = P[:, 0] - cx; below = (P[I, 1] - y0 < a.mirror_below * H).all(1)
        sgn = 1 if a.mirror_leg == "L" else -1
        src = below & (sgn * x[I] > 0.002 * H).all(1)          # the straight leg
        dst = below & (sgn * x[I] < -0.002 * H).all(1)         # the bent leg, removed
        Pm = P.copy(); Pm[:, 0] = 2 * cx - P[:, 0]
        nv = len(P)
        P = np.concatenate([P, Pm]); UV = np.concatenate([UV, UV])
        I = np.concatenate([I[~dst], I[src][:, ::-1] + nv])    # mirrored copy, winding flipped
        I = drop_floating(P, I)                                  # leftovers of the bent leg float away now
        print(f"leg: replaced {int(dst.sum())} triangles by {int(src.sum())} mirrored ones (below {a.mirror_below:.2f} of the height)")
    Pd, Nd, UVd, Id = decimate(P, UV, I, a.tris)
    buf = io.BytesIO(); tex.resize((a.tex, a.tex), Image.LANCZOS).save(buf, "JPEG", quality=90)
    write_glb(a.out, Pd, Nd, UVd, Id, buf.getvalue())
    print(f"wrote {a.out}: {len(Pd)} vertices, {len(Id)} triangles")


if __name__ == "__main__":
    main()
