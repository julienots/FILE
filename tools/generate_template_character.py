#!/usr/bin/env python3
"""Generates "sprout.glb", the animated character of the "Vitrine 3D" template (AGR ENGINE).

An original, procedural creature: a segmented stem skinned to a chain of 4 joints, a round
head and two leaves (rigidly bound to the top joint), with two animation clips:
  * "Idle"  - a slow sway (3 s loop)
  * "Dance" - a fast sway with a bounce (1.2 s loop)
glTF 2.0 binary, no textures (material factors). Deterministic; standard library only.

    python3 tools/generate_template_character.py
"""
import json
import math
import os
import struct

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "editor", "resources", "templates", "assets", "3d", "sprout.glb")

SEG_H = 0.35          # joint spacing (m)
JOINTS = 4            # Root, Stem1, Stem2, Top
RINGS = 24            # stem rings along the height
SIDES = 16


def stem_radius(t):   # t: 0 (bottom) .. 1 (top)
    return 0.16 * (1.0 - 0.45 * t)


positions, normals, uvs, joints, weights, indices = [], [], [], [], [], []
submeshes = []  # (first index, count, material)


def add_vertex(p, n, uv, j, w):
    positions.append(p)
    normals.append(n)
    uvs.append(uv)
    joints.append(j)
    weights.append(w)
    return len(positions) - 1


# ---- stem: rings skinned between neighbouring joints
height = SEG_H * (JOINTS - 1)
first = len(indices)
for r in range(RINGS + 1):
    t = r / RINGS
    y = t * height
    s = y / SEG_H
    j0 = min(int(s), JOINTS - 1)
    j1 = min(j0 + 1, JOINTS - 1)
    f = s - j0 if j1 != j0 else 0.0
    rad = stem_radius(t)
    for k in range(SIDES + 1):
        a = 2 * math.pi * k / SIDES
        n = (math.cos(a), 0.0, math.sin(a))
        add_vertex((rad * n[0], y, rad * n[2]), n, (k / SIDES, 1 - t), (j0, j1, 0, 0), (1 - f, f, 0, 0))
for r in range(RINGS):
    for k in range(SIDES):
        a = r * (SIDES + 1) + k
        b = a + SIDES + 1
        indices += [a, b, a + 1, a + 1, b, b + 1]
submeshes.append((first, len(indices) - first, 0))

# ---- head: a sphere on the top joint
first = len(indices)
top = JOINTS - 1
center = (0.0, height + 0.18, 0.0)
R = 0.24
base = len(positions)
LAT, LON = 12, 20
for i in range(LAT + 1):
    v = i / LAT
    th = math.pi * v
    for k in range(LON + 1):
        u = k / LON
        ph = 2 * math.pi * u
        n = (math.sin(th) * math.cos(ph), math.cos(th), math.sin(th) * math.sin(ph))
        add_vertex((center[0] + R * n[0], center[1] + R * n[1], center[2] + R * n[2]), n, (u, v), (top, 0, 0, 0), (1, 0, 0, 0))
for i in range(LAT):
    for k in range(LON):
        a = base + i * (LON + 1) + k
        b = a + LON + 1
        indices += [a, a + 1, b, a + 1, b + 1, b]
submeshes.append((first, len(indices) - first, 1))

# ---- leaves: two flat double-sided blades on the middle joint
first = len(indices)
for side in (-1, 1):
    y0 = SEG_H * 1.3
    pts = [(0.0, y0, 0.0), (side * 0.55, y0 + 0.25, 0.08), (side * 0.30, y0 + 0.05, -0.12)]
    for normal_sign in (1, -1):
        n = (0.0, normal_sign * 1.0, 0.0)
        ids = [add_vertex(p, n, (0.5, 0.5), (1, 2, 0, 0), (0.6, 0.4, 0, 0)) for p in pts]
        if (normal_sign > 0) == (side > 0):
            indices += [ids[0], ids[1], ids[2]]
        else:
            indices += [ids[0], ids[2], ids[1]]
submeshes.append((first, len(indices) - first, 2))


# ---- animations
def quat_axis_angle(axis, angle):
    s = math.sin(angle / 2)
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(angle / 2))


def quat_mul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def clip(duration, keys, amp_z, amp_x, bounce):
    times = [duration * i / keys for i in range(keys + 1)]
    rot = {j: [] for j in range(1, JOINTS)}
    root_t = []
    for t in times:
        ph = 2 * math.pi * t / duration
        for j in range(1, JOINTS):
            qz = quat_axis_angle((0, 0, 1), amp_z * math.sin(ph - 0.6 * j))
            qx = quat_axis_angle((1, 0, 0), amp_x * math.sin(2 * ph - 0.4 * j))
            rot[j].append(quat_mul(qz, qx))
        root_t.append((0.0, bounce * abs(math.sin(ph)), 0.0))
    return times, rot, root_t


clips = [("Idle",) + clip(3.0, 36, 0.10, 0.04, 0.0), ("Dance",) + clip(1.2, 24, 0.35, 0.15, 0.12)]

# ---- binary buffer
blob = bytearray()
buffer_views, accessors = [], []


def align():
    while len(blob) % 4:
        blob.append(0)


def add_accessor(fmt, items, comp_type, type_name, target=None, minmax=False):
    align()
    offset = len(blob)
    for it in items:
        blob.extend(struct.pack(fmt, *it) if isinstance(it, (tuple, list)) else struct.pack(fmt, it))
    view = {"buffer": 0, "byteOffset": offset, "byteLength": len(blob) - offset}
    if target:
        view["target"] = target
    buffer_views.append(view)
    acc = {"bufferView": len(buffer_views) - 1, "componentType": comp_type, "count": len(items), "type": type_name}
    if minmax:
        acc["min"] = [min(p[i] for p in items) for i in range(3)]
        acc["max"] = [max(p[i] for p in items) for i in range(3)]
    accessors.append(acc)
    return len(accessors) - 1


FLOAT, USHORT, UINT = 5126, 5123, 5125
a_pos = add_accessor("<3f", positions, FLOAT, "VEC3", 34962, minmax=True)
a_nrm = add_accessor("<3f", normals, FLOAT, "VEC3", 34962)
a_uv = add_accessor("<2f", uvs, FLOAT, "VEC2", 34962)
a_jnt = add_accessor("<4H", joints, USHORT, "VEC4", 34962)
a_wgt = add_accessor("<4f", weights, FLOAT, "VEC4", 34962)
primitives = []
for first, count, mat in submeshes:
    a_idx = add_accessor("<I", indices[first:first + count], UINT, "SCALAR", 34963)
    primitives.append({"attributes": {"POSITION": a_pos, "NORMAL": a_nrm, "TEXCOORD_0": a_uv, "JOINTS_0": a_jnt, "WEIGHTS_0": a_wgt},
                       "indices": a_idx, "material": mat})
# Inverse bind matrices: joint j sits at (0, j * SEG_H, 0) in model space (column-major).
ibm = []
for j in range(JOINTS):
    m = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -j * SEG_H, 0, 1]
    ibm.append(m)
a_ibm = add_accessor("<16f", ibm, FLOAT, "MAT4")

# Nodes: 0 = Sprout (root), 1 = Body (skinned mesh), 2.. = joints Root, Stem1, Stem2, Top.
joint_names = ["Root", "Stem1", "Stem2", "Top"]
nodes = [{"name": "Sprout", "children": [1, 2]}, {"name": "Body", "mesh": 0, "skin": 0}]
for j, name in enumerate(joint_names):
    node = {"name": name, "translation": [0, 0 if j == 0 else SEG_H, 0]}
    if j + 1 < JOINTS:
        node["children"] = [3 + j]
    nodes.append(node)
animations = []
for name, times, rot, root_t in clips:
    a_time = add_accessor("<f", times, FLOAT, "SCALAR")
    accessors[a_time]["min"] = [times[0]]
    accessors[a_time]["max"] = [times[-1]]
    samplers, channels = [], []
    for j in range(1, JOINTS):
        samplers.append({"input": a_time, "output": add_accessor("<4f", rot[j], FLOAT, "VEC4"), "interpolation": "LINEAR"})
        channels.append({"sampler": len(samplers) - 1, "target": {"node": 2 + j, "path": "rotation"}})
    samplers.append({"input": a_time, "output": add_accessor("<3f", root_t, FLOAT, "VEC3"), "interpolation": "LINEAR"})
    channels.append({"sampler": len(samplers) - 1, "target": {"node": 2, "path": "translation"}})
    animations.append({"name": name, "samplers": samplers, "channels": channels})
align()

doc = {
    "asset": {"version": "2.0", "generator": "AGR ENGINE tools/generate_template_character.py"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": nodes,
    "meshes": [{"name": "Sprout", "primitives": primitives}],
    "skins": [{"name": "SproutRig", "joints": [2, 3, 4, 5], "inverseBindMatrices": a_ibm, "skeleton": 2}],
    "animations": animations,
    "materials": [
        {"name": "Stem", "pbrMetallicRoughness": {"baseColorFactor": [0.30, 0.62, 0.22, 1], "metallicFactor": 0, "roughnessFactor": 0.6}},
        {"name": "Head", "pbrMetallicRoughness": {"baseColorFactor": [0.96, 0.62, 0.14, 1], "metallicFactor": 0, "roughnessFactor": 0.35}},
        {"name": "Leaf", "doubleSided": True,
         "pbrMetallicRoughness": {"baseColorFactor": [0.42, 0.78, 0.30, 1], "metallicFactor": 0, "roughnessFactor": 0.7}},
    ],
    "buffers": [{"byteLength": len(blob)}],
    "bufferViews": buffer_views,
    "accessors": accessors,
}
js = json.dumps(doc, separators=(",", ":")).encode()
while len(js) % 4:
    js += b" "
total = 12 + 8 + len(js) + 8 + len(blob)
with open(OUT, "wb") as f:
    f.write(struct.pack("<III", 0x46546C67, 2, total))
    f.write(struct.pack("<II", len(js), 0x4E4F534A) + js)
    f.write(struct.pack("<II", len(blob), 0x004E4942) + bytes(blob))
print(f"{OUT}: {len(positions)} vertices, {len(indices) // 3} triangles, {len(clips)} clips, {total} bytes")
