#!/usr/bin/env python3
"""Génère les ressources 3D intégrées à BodyMotion Studio :
 - mannequin.glb : personnage de démonstration riggé (squelette humanoïde complet, doigts, visage avec blendshapes ARKit)
 - ground.glb    : sol quadrillé pour le viewport
Usage : python3 tools/make_assets.py  (écrit dans app/src/main/assets/)
"""
import io, json, math, os, struct
import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "app", "src", "main", "assets")


class Glb:
    def __init__(self):
        self.bin = bytearray()
        self.j = {"asset": {"version": "2.0", "generator": "BodyMotion Studio assets"}, "bufferViews": [], "accessors": [], "buffers": []}

    def view(self, data: bytes, target=None):
        while len(self.bin) % 4: self.bin.append(0)
        v = {"buffer": 0, "byteOffset": len(self.bin), "byteLength": len(data)}
        if target: v["target"] = target
        self.bin += data
        self.j["bufferViews"].append(v)
        return len(self.j["bufferViews"]) - 1

    def acc(self, arr, typ, ctype=5126, target=None, minmax=False):
        arr = np.ascontiguousarray(arr)
        dt = {5126: np.float32, 5125: np.uint32, 5123: np.uint16}[ctype]
        data = arr.astype(dt).tobytes()
        n = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[typ]
        a = {"bufferView": self.view(data, target), "componentType": ctype, "count": int(arr.size // n), "type": typ}
        if minmax:
            r = arr.reshape(-1, n)
            a["min"] = [float(x) for x in r.min(0)]; a["max"] = [float(x) for x in r.max(0)]
        self.j["accessors"].append(a)
        return len(self.j["accessors"]) - 1

    def save(self, path):
        while len(self.bin) % 4: self.bin.append(0)
        self.j["buffers"] = [{"byteLength": len(self.bin)}]
        js = json.dumps(self.j, separators=(",", ":")).encode()
        js += b" " * ((4 - len(js) % 4) % 4)
        total = 12 + 8 + len(js) + 8 + len(self.bin)
        with open(path, "wb") as f:
            f.write(struct.pack("<III", 0x46546C67, 2, total))
            f.write(struct.pack("<II", len(js), 0x4E4F534A)); f.write(js)
            f.write(struct.pack("<II", len(self.bin), 0x004E4942)); f.write(self.bin)


# ---------------------------------------------------------------- squelette (T-pose, mètres, face +Z)
J = []  # (name, parent, global position)
def joint(name, parent, pos):
    J.append((name, parent, np.array(pos, dtype=np.float64))); return len(J) - 1

hips = joint("Hips", -1, (0, 0.95, 0))
spine = joint("Spine", hips, (0, 1.05, -0.005))
chest = joint("Chest", spine, (0, 1.19, -0.01))
upper = joint("UpperChest", chest, (0, 1.32, -0.01))
neck = joint("Neck", upper, (0, 1.46, -0.005))
head = joint("Head", neck, (0, 1.55, 0.0))
joint("HeadTop_End", head, (0, 1.76, 0.0))
for side, sx in (("Left", 1), ("Right", -1)):
    sh = joint(f"{side}Shoulder", upper, (sx * 0.035, 1.415, -0.01))
    ua = joint(f"{side}UpperArm", sh, (sx * 0.175, 1.415, -0.015))
    la = joint(f"{side}LowerArm", ua, (sx * 0.445, 1.415, -0.02))
    hd = joint(f"{side}Hand", la, (sx * 0.695, 1.415, -0.015))
    fingers = {
        "Thumb": [(0.715, 1.395, 0.03), (0.74, 1.385, 0.05), (0.762, 1.38, 0.064), (0.78, 1.377, 0.074)],
        "Index": [(0.775, 1.415, 0.028), (0.815, 1.415, 0.03), (0.84, 1.415, 0.031), (0.862, 1.415, 0.032)],
        "Middle": [(0.778, 1.416, 0.006), (0.822, 1.416, 0.006), (0.851, 1.416, 0.006), (0.875, 1.416, 0.006)],
        "Ring": [(0.774, 1.415, -0.015), (0.813, 1.415, -0.016), (0.839, 1.415, -0.017), (0.86, 1.415, -0.018)],
        "Little": [(0.765, 1.413, -0.034), (0.795, 1.413, -0.037), (0.814, 1.413, -0.039), (0.83, 1.413, -0.04)],
    }
    for f, pts in fingers.items():
        p = hd
        for k, seg in enumerate(["Proximal", "Intermediate", "Distal", "End"]):
            x, y, z = pts[k]
            p = joint(f"{side}{f}{seg}" if seg != "End" else f"{side}{f}_End", p, (sx * x, y, z))
    ul = joint(f"{side}UpperLeg", hips, (sx * 0.09, 0.915, 0))
    ll = joint(f"{side}LowerLeg", ul, (sx * 0.095, 0.505, 0.005))
    ft = joint(f"{side}Foot", ll, (sx * 0.1, 0.085, -0.01))
    tb = joint(f"{side}Toes", ft, (sx * 0.105, 0.018, 0.115))
    joint(f"{side}Toes_End", tb, (sx * 0.107, 0.018, 0.18))

names = [j[0] for j in J]
idx = {n: i for i, n in enumerate(names)}
children = {i: [k for k, j in enumerate(J) if j[1] == i] for i in range(len(J))}


# ---------------------------------------------------------------- géométrie
class MeshBuilder:
    def __init__(self):
        self.p, self.n, self.j, self.idx = [], [], [], []

    def add(self, verts, normals, faces, bone):
        base = len(self.p)
        self.p += list(verts); self.n += list(normals); self.j += [bone] * len(verts)
        self.idx += [base + f for f in faces]

    def ellipsoid(self, center, radii, rot, bone, seg=18, rings=12):
        V, N, F = [], [], []
        for r in range(rings + 1):
            th = math.pi * r / rings
            for s in range(seg + 1):
                ph = 2 * math.pi * s / seg
                u = np.array([math.sin(th) * math.cos(ph), math.cos(th), math.sin(th) * math.sin(ph)])
                V.append(center + rot @ (u * radii))
                N.append(rot @ (u / radii) / np.linalg.norm(u / radii))
        for r in range(rings):
            for s in range(seg):
                a = r * (seg + 1) + s; b = a + seg + 1
                F += [a, a + 1, b, a + 1, b + 1, b]
        self.add(V, N, F, bone)

    def capsule(self, a, b, r0, r1, bone, seg=18):
        d = b - a; L = np.linalg.norm(d); z = d / L
        x = np.cross(z, [0, 0, 1] if abs(z[2]) < 0.9 else [1, 0, 0]); x /= np.linalg.norm(x); y = np.cross(z, x)
        V, N, F = [], [], []
        rings = 10
        for k in range(rings + 1):
            t = k / rings
            # profil légèrement bombé (muscle) puis rétréci aux extrémités
            r = (r0 * (1 - t) + r1 * t) * (1 + 0.12 * math.sin(math.pi * t))
            c = a + d * t
            for s in range(seg + 1):
                ph = 2 * math.pi * s / seg
                nrm = x * math.cos(ph) + y * math.sin(ph)
                V.append(c + nrm * r); N.append(nrm)
        for k in range(rings):
            for s in range(seg):
                i0 = k * (seg + 1) + s; i1 = i0 + seg + 1
                F += [i0, i0 + 1, i1, i0 + 1, i1 + 1, i1]
        self.add(V, N, F, bone)
        self.ellipsoid(a, np.array([r0, r0, r0]), np.eye(3), bone, 12, 8)


def rotm(axis, ang):
    axis = np.array(axis, float); axis /= np.linalg.norm(axis)
    K = np.array([[0, -axis[2], axis[1]], [axis[2], 0, -axis[0]], [-axis[1], axis[0], 0]])
    return np.eye(3) + math.sin(ang) * K + (1 - math.cos(ang)) * K @ K

P = lambda n: J[idx[n]][2]
body = MeshBuilder(); joints = MeshBuilder()
# Torse
body.ellipsoid((P("Hips") + P("Spine")) / 2 + [0, -0.02, 0], np.array([0.15, 0.14, 0.1]), np.eye(3), idx["Hips"])
body.ellipsoid((P("Spine") + P("Chest")) / 2, np.array([0.135, 0.13, 0.09]), np.eye(3), idx["Spine"])
body.ellipsoid((P("Chest") + P("UpperChest")) / 2 + [0, 0.01, 0], np.array([0.155, 0.14, 0.1]), np.eye(3), idx["Chest"])
body.ellipsoid(P("UpperChest") + [0, 0.035, 0], np.array([0.175, 0.1, 0.095]), np.eye(3), idx["UpperChest"])
body.capsule(P("Neck"), P("Head"), 0.05, 0.045, idx["Neck"])
for s in ("Left", "Right"):
    body.capsule(P(f"{s}UpperArm"), P(f"{s}LowerArm"), 0.055, 0.045, idx[f"{s}UpperArm"])
    body.capsule(P(f"{s}LowerArm"), P(f"{s}Hand"), 0.043, 0.033, idx[f"{s}LowerArm"])
    hc = P(f"{s}Hand") + (P(f"{s}MiddleProximal") - P(f"{s}Hand")) * 0.5
    body.ellipsoid(hc, np.array([0.05, 0.018, 0.042]), np.eye(3), idx[f"{s}Hand"])
    for f in ("Thumb", "Index", "Middle", "Ring", "Little"):
        segs = ["Proximal", "Intermediate", "Distal"]
        for k, sg in enumerate(segs):
            a = P(f"{s}{f}{sg}"); b = P(f"{s}{f}{segs[k + 1]}") if k < 2 else P(f"{s}{f}_End")
            r = 0.0105 if f != "Little" else 0.009
            body.capsule(a, b, r, r * 0.85, idx[f"{s}{f}{sg}"], seg=8)
    body.capsule(P(f"{s}UpperLeg"), P(f"{s}LowerLeg"), 0.078, 0.058, idx[f"{s}UpperLeg"])
    body.capsule(P(f"{s}LowerLeg"), P(f"{s}Foot"), 0.055, 0.04, idx[f"{s}LowerLeg"])
    fc = (P(f"{s}Foot") + P(f"{s}Toes")) / 2 + [0, -0.03, 0]
    body.ellipsoid(fc, np.array([0.048, 0.035, 0.1]), rotm([1, 0, 0], -0.18), idx[f"{s}Foot"])
    body.ellipsoid(P(f"{s}Toes") + [0, -0.004, 0.025], np.array([0.045, 0.018, 0.045]), np.eye(3), idx[f"{s}Toes"])
# Articulations lumineuses
for n in ["LeftShoulder", "RightShoulder", "LeftLowerArm", "RightLowerArm", "LeftHand", "RightHand", "LeftLowerLeg", "RightLowerLeg", "LeftFoot", "RightFoot", "LeftUpperLeg", "RightUpperLeg", "Neck"]:
    r = 0.03 if "Hand" in n or "Foot" in n or n == "Neck" else 0.04
    target = n if "Shoulder" not in n else n.replace("Shoulder", "UpperArm")
    joints.ellipsoid(P(target), np.array([r, r, r]), np.eye(3), idx[target], 12, 8)

# Tête + visage (maillage séparé avec blendshapes)
headC = P("Head") + np.array([0, 0.1, 0.012])
skin = MeshBuilder(); feat = MeshBuilder()
skin.ellipsoid(headC, np.array([0.092, 0.115, 0.1]), np.eye(3), idx["Head"], 24, 16)
eyeL = headC + [0.035, 0.018, 0.088]; eyeR = headC + [-0.035, 0.018, 0.088]
mouth = headC + [0, -0.048, 0.088]
browL = headC + [0.036, 0.048, 0.085]; browR = headC + [-0.036, 0.048, 0.085]
parts = {}
def featpart(name, c, radii, rot=np.eye(3)):
    s = len(feat.p); feat.ellipsoid(c, np.array(radii), rot, idx["Head"], 14, 10); parts[name] = (s, len(feat.p), np.array(c))
featpart("eyeL", eyeL, [0.014, 0.014, 0.008]); featpart("eyeR", eyeR, [0.014, 0.014, 0.008])
featpart("mouth", mouth, [0.026, 0.006, 0.01])
featpart("browL", browL, [0.019, 0.004, 0.006], rotm([0, 0, 1], -0.12)); featpart("browR", browR, [0.019, 0.004, 0.006], rotm([0, 0, 1], 0.12))

def target_for(func):
    fp = np.array(feat.p); d = np.zeros_like(fp)
    func(fp, d); return d
def squash(part, sy):
    def f(fp, d):
        s, e, c = parts[part]; d[s:e, 1] = (fp[s:e, 1] - c[1]) * (sy - 1)
    return f
def mouth_open(fp, d):
    s, e, c = parts["mouth"]; d[s:e, 1] = (fp[s:e, 1] - c[1]) * 3.5 - 0.012
def brows_up(fp, d):
    for p in ("browL", "browR"):
        s, e, c = parts[p]; d[s:e, 1] = 0.014
def smile(fp, d):
    s, e, c = parts["mouth"]; x = fp[s:e, 0] - c[0]; d[s:e, 1] = (x / 0.026) ** 2 * 0.01; d[s:e, 0] = x * 0.25
morphs = [("eyeBlinkLeft", target_for(squash("eyeL", 0.08))), ("eyeBlinkRight", target_for(squash("eyeR", 0.08))),
          ("jawOpen", target_for(mouth_open)), ("browInnerUp", target_for(brows_up)), ("mouthSmileLeft", target_for(smile))]


# ---------------------------------------------------------------- écriture GLB
g = Glb()
nodes = []
for i, (n, p, pos) in enumerate(J):
    t = pos - (J[p][2] if p >= 0 else 0)
    nd = {"name": n, "translation": [float(x) for x in t]}
    if children[i]: nd["children"] = children[i]
    nodes.append(nd)
jointList = list(range(len(J)))
ibm = []
for (n, p, pos) in J:
    m = np.eye(4); m[:3, 3] = -pos; ibm.append(m.T.reshape(-1))  # colonne-major
ibm_acc = g.acc(np.array(ibm), "MAT4")

g.j["materials"] = [
    {"name": "Mannequin", "pbrMetallicRoughness": {"baseColorFactor": [0.58, 0.62, 0.68, 1], "metallicFactor": 0.15, "roughnessFactor": 0.42}},
    {"name": "Articulations", "pbrMetallicRoughness": {"baseColorFactor": [0.08, 0.75, 0.86, 1], "metallicFactor": 0.0, "roughnessFactor": 0.35}, "emissiveFactor": [0.02, 0.18, 0.22]},
    {"name": "Visage", "pbrMetallicRoughness": {"baseColorFactor": [0.04, 0.045, 0.06, 1], "metallicFactor": 0.0, "roughnessFactor": 0.3}},
]

def prim(mb, mat, extra_targets=None):
    p = np.array(mb.p, np.float32); nn = np.array(mb.n, np.float32)
    jj = np.zeros((len(mb.j), 4), np.uint16); jj[:, 0] = mb.j
    ww = np.zeros((len(mb.j), 4), np.float32); ww[:, 0] = 1
    pr = {"attributes": {"POSITION": g.acc(p, "VEC3", target=34962, minmax=True), "NORMAL": g.acc(nn, "VEC3", target=34962),
                         "JOINTS_0": g.acc(jj, "VEC4", 5123, target=34962), "WEIGHTS_0": g.acc(ww, "VEC4", target=34962)},
          "indices": g.acc(np.array(mb.idx, np.uint32), "SCALAR", 5125, target=34963), "material": mat}
    if extra_targets is not None: pr["targets"] = extra_targets
    return pr

def zero_targets(n):
    return [{"POSITION": g.acc(np.zeros((n, 3), np.float32), "VEC3", minmax=True)} for _ in morphs]

g.j["meshes"] = [
    {"name": "Body", "primitives": [prim(body, 0), prim(joints, 1)]},
    {"name": "Face", "primitives": [prim(skin, 0, zero_targets(len(skin.p))),
                                     prim(feat, 2, [{"POSITION": g.acc(d.astype(np.float32), "VEC3", minmax=True)} for _, d in morphs])],
     "weights": [0.0] * len(morphs), "extras": {"targetNames": [m[0] for m in morphs]}},
]
nodes.append({"name": "Mannequin_Body", "mesh": 0, "skin": 0})
nodes.append({"name": "Mannequin_Face", "mesh": 1, "skin": 0})
nodes.append({"name": "BodyMotion_Mannequin", "children": [0, len(J), len(J) + 1]})
g.j["nodes"] = nodes
g.j["skins"] = [{"name": "Armature", "joints": jointList, "inverseBindMatrices": ibm_acc, "skeleton": 0}]
g.j["scenes"] = [{"nodes": [len(nodes) - 1]}]; g.j["scene"] = 0
os.makedirs(OUT, exist_ok=True)
g.save(os.path.join(OUT, "mannequin.glb"))

# ---------------------------------------------------------------- sol
img = Image.new("RGB", (512, 512), (16, 18, 23))
d = ImageDraw.Draw(img)
for k in range(0, 512, 64):
    d.line([(k, 0), (k, 511)], fill=(30, 34, 43), width=2); d.line([(0, k), (511, k)], fill=(30, 34, 43), width=2)
d.line([(0, 0), (0, 511)], fill=(44, 52, 64), width=3); d.line([(0, 0), (511, 0)], fill=(44, 52, 64), width=3)
buf = io.BytesIO(); img.save(buf, "PNG")
g2 = Glb()
S = 12.0; R = S / 2  # 12 m, une case majeure = 2 m (8 lignes par case de texture)
pos = np.array([[-S, 0, -S], [S, 0, -S], [S, 0, S], [-S, 0, S]], np.float32)
nrm = np.array([[0, 1, 0]] * 4, np.float32)
uv = np.array([[0, 0], [R, 0], [R, R], [0, R]], np.float32)
ind = np.array([0, 2, 1, 0, 3, 2], np.uint32)
g2.j["images"] = [{"bufferView": g2.view(buf.getvalue()), "mimeType": "image/png"}]
g2.j["samplers"] = [{"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}]
g2.j["textures"] = [{"source": 0, "sampler": 0}]
g2.j["materials"] = [{"name": "Sol", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicFactor": 0.0, "roughnessFactor": 0.92}}]
g2.j["meshes"] = [{"name": "Ground", "primitives": [{"attributes": {"POSITION": g2.acc(pos, "VEC3", minmax=True), "NORMAL": g2.acc(nrm, "VEC3"), "TEXCOORD_0": g2.acc(uv, "VEC2")},
                                                     "indices": g2.acc(ind, "SCALAR", 5125), "material": 0}]}]
g2.j["nodes"] = [{"name": "Ground", "mesh": 0}]
g2.j["scenes"] = [{"nodes": [0]}]; g2.j["scene"] = 0
g2.save(os.path.join(OUT, "ground.glb"))
print("ok", len(J), "os")
