#!/usr/bin/env python3
"""Generates the original 3D assets of the editor's "Vitrine 3D" template (AGR ENGINE).

Everything is procedural (no third-party assets):
  * crate.glb  - a 1 m crate with embedded base color, normal and metallic-roughness
                 textures (wood planks, metal corners, amber AGR stripe)
  * rock.glb   - a smooth low-poly rock (displaced icosphere)
  * *.agrmat   - PBR materials (gold, copper, red plastic, rubber, ceramic, neon)
Deterministic: re-running rewrites identical files. Requires Pillow and numpy.
"""
import io
import json
import math
import os
import struct

import numpy as np
from PIL import Image

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "editor", "resources", "templates", "assets", "3d")
rng = np.random.default_rng(11)


def png_bytes(arr):
    buf = io.BytesIO()
    Image.fromarray(arr).save(buf, format="PNG", optimize=True)
    return buf.getvalue()


# ----------------------------------------------------------------- crate textures
N = 256
y, x = np.mgrid[0:N, 0:N].astype(np.float32)
height = np.zeros((N, N), np.float32)
base = np.zeros((N, N, 3), np.float32)
rough = np.full((N, N), 0.72, np.float32)
metal = np.zeros((N, N), np.float32)

# Planks: 4 horizontal boards with grooves and grain.
plank = (y // (N / 4)).astype(int)
grain = 0.08 * np.sin(x / 7.0 + plank * 1.7 + 3.0 * np.sin(y / 23.0)) + 0.04 * rng.standard_normal((N, N)).astype(np.float32)
wood = np.array([0.55, 0.36, 0.20], np.float32)
base[:] = wood * (1.0 + grain[..., None]) * (0.92 + 0.08 * (plank % 2))[..., None]
groove = (np.abs((y % (N / 4)) - 0.0) < 3) | (np.abs((y % (N / 4)) - N / 4) < 3)
height -= groove * 0.6
base[groove] *= 0.55

# Diagonal brace with the amber AGR stripe.
d = np.abs((x - y)) / math.sqrt(2.0)
brace = d < 18
base[brace] = np.array([0.62, 0.42, 0.24]) * (1.0 + grain[brace, None] * 0.5)
height[brace] += 0.5
stripe = d < 6
base[stripe] = np.array([0.96, 0.65, 0.14])
rough[stripe] = 0.45

# Metal frame and corner plates.
border = (x < 20) | (x >= N - 20) | (y < 20) | (y >= N - 20)
corner = ((x < 52) | (x >= N - 52)) & ((y < 52) | (y >= N - 52))
frame = border | corner
base[frame] = np.array([0.42, 0.44, 0.47]) * (1.0 + 0.05 * rng.standard_normal((frame.sum(), 1)))
height[frame] += 1.0
rough[frame] = 0.38
metal[frame] = 1.0
for cx, cy in [(30, 30), (N - 31, 30), (30, N - 31), (N - 31, N - 31)]:  # rivets
    r = np.hypot(x - cx, y - cy)
    rivet = r < 6
    height[rivet] += (1.0 - r[rivet] / 6.0) * 1.2
    rough[rivet] = 0.25

base_img = (np.clip(base, 0, 1) ** (1 / 1.0) * 255).astype(np.uint8)  # authored directly in sRGB
# Normal map (tangent space, +Y up = toward the top of the image).
h = height * 3.0
dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
dy = (np.roll(h, 1, 0) - np.roll(h, -1, 0)) * 0.5  # image rows grow downward
n = np.stack([-dx, -dy, np.ones_like(h)], -1)
n /= np.linalg.norm(n, axis=-1, keepdims=True)
normal_img = ((n * 0.5 + 0.5) * 255).astype(np.uint8)
mr = np.zeros((N, N, 3), np.uint8)
mr[..., 1] = (np.clip(rough, 0, 1) * 255).astype(np.uint8)  # G = roughness
mr[..., 2] = (np.clip(metal, 0, 1) * 255).astype(np.uint8)  # B = metallic


# ----------------------------------------------------------------- GLB writer
class Glb:
    def __init__(self):
        self.bin = bytearray()
        self.views = []
        self.accessors = []

    def view(self, data, target=None):
        while len(self.bin) % 4:
            self.bin.append(0)
        v = {"buffer": 0, "byteOffset": len(self.bin), "byteLength": len(data)}
        if target:
            v["target"] = target
        self.bin += data
        self.views.append(v)
        return len(self.views) - 1

    def accessor(self, arr, kind, component, target, minmax=False):
        view = self.view(arr.tobytes(), target)
        acc = {"bufferView": view, "componentType": component, "count": int(arr.shape[0]), "type": kind}
        if minmax:
            acc["min"] = arr.min(0).tolist()
            acc["max"] = arr.max(0).tolist()
        self.accessors.append(acc)
        return len(self.accessors) - 1

    def save(self, path, doc):
        doc["buffers"] = [{"byteLength": len(self.bin)}]
        doc["bufferViews"] = self.views
        doc["accessors"] = self.accessors
        js = json.dumps(doc, separators=(",", ":")).encode()
        js += b" " * ((4 - len(js) % 4) % 4)
        while len(self.bin) % 4:
            self.bin.append(0)
        total = 12 + 8 + len(js) + 8 + len(self.bin)
        with open(path, "wb") as f:
            f.write(struct.pack("<III", 0x46546C67, 2, total))
            f.write(struct.pack("<II", len(js), 0x4E4F534A) + js)
            f.write(struct.pack("<II", len(self.bin), 0x004E4942) + bytes(self.bin))


def mesh_primitive(glb, pos, nrm, uv, idx):
    a_pos = glb.accessor(np.asarray(pos, np.float32), "VEC3", 5126, 34962, True)
    a_nrm = glb.accessor(np.asarray(nrm, np.float32), "VEC3", 5126, 34962)
    a_uv = glb.accessor(np.asarray(uv, np.float32), "VEC2", 5126, 34962)
    a_idx = glb.accessor(np.asarray(idx, np.uint16), "SCALAR", 5123, 34963)
    return {"attributes": {"POSITION": a_pos, "NORMAL": a_nrm, "TEXCOORD_0": a_uv}, "indices": a_idx, "material": 0}


# Crate: cube 1 m, base at y = 0, each face maps the whole texture.
pos, nrm, uv, idx = [], [], [], []
faces = [((1, 0, 0), (0, 0, -1)), ((-1, 0, 0), (0, 0, 1)), ((0, 1, 0), (1, 0, 0)), ((0, -1, 0), (1, 0, 0)), ((0, 0, 1), (1, 0, 0)),
         ((0, 0, -1), (-1, 0, 0))]
for n_, t_ in faces:
    n_ = np.array(n_, np.float32)
    t_ = np.array(t_, np.float32)
    b_ = np.cross(n_, t_)
    c = n_ * 0.5 + np.array([0, 0.5, 0], np.float32)
    base_i = len(pos)
    for (su, sv, u, v) in [(-1, -1, 0, 1), (1, -1, 1, 1), (1, 1, 1, 0), (-1, 1, 0, 0)]:
        pos.append(c + (t_ * su + b_ * sv) * 0.5)
        nrm.append(n_)
        uv.append((u, v))
    idx += [base_i + i for i in (0, 1, 2, 0, 2, 3)]
glb = Glb()
prim = mesh_primitive(glb, pos, nrm, uv, idx)
images = []
for arr in (base_img, normal_img, mr):
    images.append({"bufferView": glb.view(png_bytes(arr)), "mimeType": "image/png"})
crate = {
    "asset": {"version": "2.0", "generator": "AGR ENGINE tools/generate_template_3d_art.py"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [{"name": "Crate", "mesh": 0}],
    "meshes": [{"name": "Crate", "primitives": [prim]}],
    "materials": [{
        "name": "Crate Wood",
        "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicRoughnessTexture": {"index": 2},
                                 "metallicFactor": 1.0, "roughnessFactor": 1.0},
        "normalTexture": {"index": 1, "scale": 1.0},
    }],
    "textures": [{"source": 0}, {"source": 1}, {"source": 2}],
    "images": images,
}
os.makedirs(OUT, exist_ok=True)
glb.save(os.path.join(OUT, "crate.glb"), crate)

# Rock: subdivided icosahedron displaced by smooth noise, flat-ish bottom.
t = (1 + 5 ** 0.5) / 2
verts = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t), (t, 0, -1), (t, 0, 1), (-t, 0, -1),
         (-t, 0, 1)]
verts = [np.array(v, np.float64) / np.linalg.norm(v) for v in verts]
tris = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8), (3, 9, 4),
        (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
for _ in range(3):
    cache = {}
    new = []

    def mid(a, b):
        key = (min(a, b), max(a, b))
        if key not in cache:
            m = verts[a] + verts[b]
            verts.append(m / np.linalg.norm(m))
            cache[key] = len(verts) - 1
        return cache[key]

    for a, b, c in tris:
        ab, bc, ca = mid(a, b), mid(b, c), mid(c, a)
        new += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
    tris = new
V = np.array(verts)
bumps = rng.standard_normal((6, 3))
disp = 1.0 + sum(0.12 * np.sin(V @ k * 2.2 + i) for i, k in enumerate(bumps))
P = V * disp[:, None] * np.array([0.9, 0.55, 0.75])
P[:, 1] = np.maximum(P[:, 1], -0.25)  # sits on the ground
P[:, 1] += 0.25
F = np.array(tris)
# Smooth normals (counter-clockwise outward faces).
N_ = np.zeros_like(P)
fn = np.cross(P[F[:, 1]] - P[F[:, 0]], P[F[:, 2]] - P[F[:, 0]])
for k in range(3):
    np.add.at(N_, F[:, k], fn)
N_ /= np.linalg.norm(N_, axis=1, keepdims=True)
UV = np.stack([np.arctan2(V[:, 2], V[:, 0]) / (2 * math.pi) + 0.5, 0.5 - np.arcsin(np.clip(V[:, 1], -1, 1)) / math.pi], 1)
glb = Glb()
prim = mesh_primitive(glb, P, N_, UV, F.reshape(-1))
rock = {
    "asset": {"version": "2.0", "generator": "AGR ENGINE tools/generate_template_3d_art.py"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [{"name": "Rock", "mesh": 0}],
    "meshes": [{"name": "Rock", "primitives": [prim]}],
    "materials": [{"name": "Rock", "pbrMetallicRoughness": {"baseColorFactor": [0.32, 0.31, 0.30, 1.0], "metallicFactor": 0.0,
                                                            "roughnessFactor": 0.85}}],
}
glb.save(os.path.join(OUT, "rock.glb"), rock)


# ----------------------------------------------------------------- materials
def material(name, color, metallic, roughness, emissive=(0, 0, 0), intensity=1.0):
    return {"format": "agr.material", "version": 1, "name": name, "baseColor": list(color) + [1.0], "baseColorTexture": "",
            "metallic": metallic, "roughness": roughness, "metallicRoughnessTexture": "", "normalTexture": "", "normalScale": 1.0,
            "occlusionTexture": "", "occlusionStrength": 1.0, "emissive": list(emissive) + [1.0], "emissiveIntensity": intensity,
            "emissiveTexture": "", "alphaMode": "opaque", "alphaCutoff": 0.5, "doubleSided": False, "unlit": False, "uvScale": [1.0, 1.0],
            "uvOffset": [0.0, 0.0]}


for fname, m in {
    "gold.agrmat": material("Or brossé", (1.0, 0.78, 0.34), 1.0, 0.3),
    "copper.agrmat": material("Cuivre", (0.95, 0.64, 0.54), 1.0, 0.45),
    "plastic_red.agrmat": material("Plastique rouge", (0.85, 0.12, 0.10), 0.0, 0.35),
    "rubber.agrmat": material("Caoutchouc", (0.08, 0.08, 0.09), 0.0, 0.95),
    "ceramic.agrmat": material("Céramique", (0.92, 0.92, 0.90), 0.0, 0.08),
    "neon.agrmat": material("Néon ambre", (0.2, 0.12, 0.02), 0.0, 0.5, (0.96, 0.65, 0.14), 4.0),
}.items():
    with open(os.path.join(OUT, fname), "w", encoding="utf-8") as f:
        json.dump(m, f, indent=2, ensure_ascii=False)
print("3D template art written to", OUT)
