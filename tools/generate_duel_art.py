#!/usr/bin/env python3
"""Generates the original cartoon art of the "Duel des Tours" template (AGR ENGINE).

Everything is built procedurally here (no third-party assets), in a chunky mobile-cartoon style:
big heads, round shapes, smooth normals (clean outlines), flat colors (toon lighting).

  * games/duel/knight.glb, archer.glb, giant.glb, goblin.glb - skinned humanoids sharing one
    skeleton (Hips, Spine, Head, ArmL, ArmR, LegL, LegR) with three clips:
      "Idle" (breathing, 2 s loop), "Walk" (0.8 s loop), "Attack" (0.7 s).
    Two meshes: "Skin" (fixed colors) and "Body" (team-colored parts, white material: the
    MeshRenderer color tints it, the game script paints it blue or red).
  * games/duel/tower.glb, king_tower.glb - stone towers: "Base" (fixed colors) and "Roof" (team).
  * games/duel/*.agrctrl - one state machine per character (Idle / Walk / Attack).
  * games/duel/grass.png, path.png, stone.png - painted, tileable textures.

glTF 2.0 binary, material factors only. Deterministic; needs Pillow for the textures.

    python3 tools/generate_duel_art.py
"""
import json
import math
import os
import random
import struct

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "editor", "resources", "templates", "assets", "games", "duel")
os.makedirs(OUT, exist_ok=True)

# ----------------------------------------------------------------------------- math helpers


def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def norm(a):
    l = math.sqrt(dot(a, a))
    return (a[0] / l, a[1] / l, a[2] / l) if l > 1e-12 else (0.0, 1.0, 0.0)


def rot_matrix(axis, angle):
    x, y, z = norm(axis)
    c, s, t = math.cos(angle), math.sin(angle), 1 - math.cos(angle)
    return ((t * x * x + c, t * x * y - s * z, t * x * z + s * y),
            (t * x * y + s * z, t * y * y + c, t * y * z - s * x),
            (t * x * z - s * y, t * y * z + s * x, t * z * z + c))


def apply(m, v): return (dot(m[0], v), dot(m[1], v), dot(m[2], v))


def quat(axis, angle):
    s = math.sin(angle / 2)
    a = norm(axis)
    return (a[0] * s, a[1] * s, a[2] * s, math.cos(angle / 2))


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def euler(x=0.0, y=0.0, z=0.0):
    return qmul(qmul(quat((0, 1, 0), y), quat((1, 0, 0), x)), quat((0, 0, 1), z))


# ----------------------------------------------------------------------------- mesh builder


class Mesh:
    """Vertices with smooth normals, grouped in submeshes (one per material)."""

    def __init__(self):
        self.pos, self.nrm, self.uv, self.jnt, self.wgt = [], [], [], [], []
        self.idx = {}  # material index -> triangle indices

    def grid(self, rows, cols, point, material, joint, center, wrap=True, rot=None, offset=(0, 0, 0)):
        """Parametric surface point(i / rows, j / cols); normals from the grid tangents (smooth),
        oriented away from `center`. Degenerate rows (poles) use the direction from the center."""
        P = [[point(i / rows, j / cols) for j in range(cols + 1)] for i in range(rows + 1)]
        base = len(self.pos)
        for i in range(rows + 1):
            for j in range(cols + 1):
                jp = (j + 1) if j < cols else (1 if wrap else cols)
                jm = (j - 1) if j > 0 else (cols - 1 if wrap else 0)
                du = sub(P[i][jp], P[i][jm])
                dv = sub(P[min(i + 1, rows)][j], P[max(i - 1, 0)][j])
                n = cross(dv, du)
                outward = sub(P[i][j], center)
                if dot(n, n) < 1e-14:
                    n = outward
                elif dot(n, outward) < 0:
                    n = mul(n, -1)
                n = norm(n)
                p = P[i][j]
                if rot:
                    p = add(apply(rot, sub(p, center)), center)
                    n = apply(rot, n)
                self.pos.append(add(p, offset))
                self.nrm.append(n)
                self.uv.append((j / cols, i / rows))
                self.jnt.append((joint, 0, 0, 0))
                self.wgt.append((1.0, 0.0, 0.0, 0.0))
        # Winding: counter-clockwise seen from outside (back-face culling, outlines). The
        # parametrization decides it, so compare the triangles with the outward normals and flip
        # the whole surface when most of them disagree.
        quads = []
        agree = 0
        for i in range(rows):
            for j in range(cols):
                a = base + i * (cols + 1) + j
                b = a + cols + 1
                quads.append((a, b))
                g = cross(sub(self.pos[b], self.pos[a]), sub(self.pos[a + 1], self.pos[a]))
                agree += 1 if dot(g, self.nrm[a]) > 0 else (-1 if dot(g, g) > 1e-16 else 0)
        tris = self.idx.setdefault(material, [])
        for a, b in quads:
            tris += [a, b, a + 1, a + 1, b, b + 1] if agree >= 0 else [a, a + 1, b, a + 1, b + 1, b]

    def blob(self, center, radii, material, joint=0, power=1.0, rot=None, rows=10, cols=16):
        """Superellipsoid: power 1 = ellipsoid, < 1 = rounded box (cushion), > 1 = pinched."""
        def f(c, e): return math.copysign(abs(c) ** e, c)

        def point(v, u):
            th = math.pi * v
            ph = 2 * math.pi * u
            x = f(math.sin(th), power) * f(math.cos(ph), power)
            y = f(math.cos(th), power)
            z = f(math.sin(th), power) * f(math.sin(ph), power)
            return (center[0] + radii[0] * x, center[1] + radii[1] * y, center[2] + radii[2] * z)
        self.grid(rows, cols, point, material, joint, center, True, rot)

    def lathe(self, profile, center, material, joint=0, cols=28, rot=None):
        """Surface of revolution around Y: profile = [(radius, height), ...] from bottom to top."""
        rows = len(profile) - 1

        def point(v, u):
            r, h = profile[round(v * rows)]
            ph = 2 * math.pi * u
            return (center[0] + r * math.cos(ph), center[1] + h, center[2] + r * math.sin(ph))
        mid = (center[0], center[1] + sum(h for _, h in profile) / len(profile), center[2])
        self.grid(rows, cols, point, material, joint, mid, True, rot)

    def tube(self, curve, radius, material, joint=0, rows=16, cols=10):
        """Tube of `radius` along curve(t), t in 0..1 (bows, handles)."""
        def frame(t):
            a, b = curve(max(t - 0.01, 0)), curve(min(t + 0.01, 1))
            tan = norm(sub(b, a))
            side = norm(cross(tan, (1, 0, 0) if abs(tan[0]) < 0.9 else (0, 1, 0)))
            return tan, side, cross(tan, side)

        def point(v, u):
            c = curve(v)
            _, s, b = frame(v)
            ph = 2 * math.pi * u
            return add(c, add(mul(s, radius * math.cos(ph)), mul(b, radius * math.sin(ph))))
        base = len(self.pos)
        self.grid(rows, cols, point, material, joint, curve(0.5), True)
        # Normals: from the curve, not from the far-away center.
        for i in range(rows + 1):
            c = curve(i / rows)
            for j in range(cols + 1):
                k = base + i * (cols + 1) + j
                self.nrm[k] = norm(sub(self.pos[k], c))


# ----------------------------------------------------------------------------- glTF writer


def write_glb(path, meshes, materials, nodes, skins=None, animations=None, ibm=None):
    """meshes: [(name, Mesh, skinned)], nodes: glTF node dicts (mesh / skin indices already set),
    animations: [(name, times, {node: ("rotation"|"translation", values)})]."""
    blob = bytearray()
    views, accessors = [], []

    def align():
        while len(blob) % 4:
            blob.append(0)

    def acc(fmt, items, comp, typ, target=None, minmax=False):
        align()
        off = len(blob)
        for it in items:
            blob.extend(struct.pack(fmt, *it) if isinstance(it, (tuple, list)) else struct.pack(fmt, it))
        v = {"buffer": 0, "byteOffset": off, "byteLength": len(blob) - off}
        if target:
            v["target"] = target
        views.append(v)
        a = {"bufferView": len(views) - 1, "componentType": comp, "count": len(items), "type": typ}
        if minmax:
            a["min"] = [min(p[i] for p in items) for i in range(3)]
            a["max"] = [max(p[i] for p in items) for i in range(3)]
        accessors.append(a)
        return len(accessors) - 1

    F, US, UI = 5126, 5123, 5125
    gl_meshes = []
    for name, m, skinned in meshes:
        attrs = {"POSITION": acc("<3f", m.pos, F, "VEC3", 34962, True), "NORMAL": acc("<3f", m.nrm, F, "VEC3", 34962),
                 "TEXCOORD_0": acc("<2f", m.uv, F, "VEC2", 34962)}
        if skinned:
            attrs["JOINTS_0"] = acc("<4H", m.jnt, US, "VEC4", 34962)
            attrs["WEIGHTS_0"] = acc("<4f", m.wgt, F, "VEC4", 34962)
        prims = [{"attributes": attrs, "indices": acc("<I", tris, UI, "SCALAR", 34963), "material": mat}
                 for mat, tris in sorted(m.idx.items())]
        gl_meshes.append({"name": name, "primitives": prims})
    doc = {"asset": {"version": "2.0", "generator": "AGR ENGINE tools/generate_duel_art.py"}, "scene": 0,
           "scenes": [{"nodes": [0]}], "nodes": nodes, "meshes": gl_meshes,
           "materials": [{"name": n, "pbrMetallicRoughness": {"baseColorFactor": list(c) + [1], "metallicFactor": 0, "roughnessFactor": r}}
                         for n, c, r in materials]}
    if skins:
        s = dict(skins)
        s["inverseBindMatrices"] = acc("<16f", ibm, F, "MAT4")
        doc["skins"] = [s]
    if animations:
        doc["animations"] = []
        for name, times, channels in animations:
            t = acc("<f", times, F, "SCALAR")
            accessors[t]["min"], accessors[t]["max"] = [times[0]], [times[-1]]
            samplers, chans = [], []
            for node, (target, values) in channels.items():
                fmt, typ = ("<4f", "VEC4") if target == "rotation" else ("<3f", "VEC3")
                samplers.append({"input": t, "output": acc(fmt, values, F, typ), "interpolation": "LINEAR"})
                chans.append({"sampler": len(samplers) - 1, "target": {"node": node, "path": target}})
            doc["animations"].append({"name": name, "samplers": samplers, "channels": chans})
    align()
    doc["buffers"] = [{"byteLength": len(blob)}]
    doc["bufferViews"], doc["accessors"] = views, accessors
    js = json.dumps(doc, separators=(",", ":")).encode()
    while len(js) % 4:
        js += b" "
    total = 12 + 8 + len(js) + 8 + len(blob)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, total))
        f.write(struct.pack("<II", len(js), 0x4E4F534A) + js)
        f.write(struct.pack("<II", len(blob), 0x004E4942) + bytes(blob))
    tris = sum(len(t) for _, m, _ in meshes for t in m.idx.values()) // 3
    print(f"{os.path.relpath(path)}: {sum(len(m.pos) for _, m, _ in meshes)} vertices, {tris} triangles, {total} bytes")


# ----------------------------------------------------------------------------- the shared skeleton
# Model faces +Z, Y up, feet at y = 0. Joint index: position in JOINTS (model-space rest position).
JOINTS = [  # name, parent, model-space position
    ("Hips", None, (0.0, 0.50, 0.0)),
    ("Spine", 0, (0.0, 0.72, 0.0)),
    ("Head", 1, (0.0, 1.00, 0.0)),
    ("ArmL", 1, (0.31, 0.90, 0.0)),
    ("ArmR", 1, (-0.31, 0.90, 0.0)),
    ("LegL", 0, (0.13, 0.46, 0.0)),
    ("LegR", 0, (-0.13, 0.46, 0.0)),
]
HIPS, SPINE, HEAD, ARML, ARMR, LEGL, LEGR = range(7)


def local_translation(j):
    name, parent, p = JOINTS[j]
    return p if parent is None else sub(p, JOINTS[parent][2])


def joint_pos(j): return JOINTS[j][2]


# Colors (sRGB factors are written as-is: the engine treats material colors as sRGB).
SKIN = (1.0, 0.80, 0.64)
DARK = (0.13, 0.09, 0.14)
WHITE = (1.0, 1.0, 1.0)
TEAM = (0.97, 0.97, 0.97)       # tinted by the MeshRenderer color
STEEL = (0.78, 0.82, 0.88)
GOLD = (1.0, 0.78, 0.22)
LEATHER = (0.45, 0.28, 0.16)
WOOD = (0.62, 0.40, 0.20)
GOBLIN = (0.45, 0.82, 0.32)


def face(m, mats, head_c, r, eye_y=0.02, eye_dx=0.10, eye_size=1.0, brows=False):
    """Big cartoon eyes (dark with a white glint) on the front of a head of radius r."""
    for sx in (-1, 1):
        ex, ey = sx * eye_dx * r / 0.27, head_c[1] + eye_y
        ez = head_c[2] + math.sqrt(max(r * r - (ex - head_c[0]) ** 2 - eye_y ** 2, 0.0)) - 0.015
        m.blob((ex, ey, ez), (0.045 * eye_size, 0.065 * eye_size, 0.03), mats["eye"], HEAD)
        m.blob((ex + 0.015 * eye_size, ey + 0.025 * eye_size, ez + 0.022), (0.016 * eye_size,) * 3, mats["glint"], HEAD, rows=6, cols=8)
        if brows:
            m.blob((ex, ey + 0.11, ez - 0.01), (0.07, 0.02, 0.025), mats["eye"], HEAD,
                   rot=rot_matrix((0, 0, 1), -sx * 0.35), rows=6, cols=10)


def arms_and_legs(skin, team, mats, sleeve_team=True, hand_r=0.085, leg_mat="boot", thick=1.0):
    for j, sx in ((ARML, 1), (ARMR, -1)):
        s = joint_pos(j)
        (team if sleeve_team else skin).blob((s[0] + sx * 0.02, s[1] - 0.15, s[2]), (0.085 * thick, 0.19, 0.085 * thick),
                                            mats["team"] if sleeve_team else mats["skin"], j)
        skin.blob((s[0] + sx * 0.03, s[1] - 0.36, s[2] + 0.02), (hand_r,) * 3, mats["skin"], j)
    for j in (LEGL, LEGR):
        p = joint_pos(j)
        skin.blob((p[0], p[1] - 0.20, p[2]), (0.09 * thick, 0.2, 0.09 * thick), mats["pants"], j)
        skin.blob((p[0], 0.08, p[2] + 0.04), (0.11 * thick, 0.09, 0.15 * thick), mats[leg_mat], j)


def humanoid(kind):
    skin, team = Mesh(), Mesh()
    materials = [("Skin", SKIN, 0.7), ("Eye", DARK, 0.4), ("Glint", WHITE, 0.3), ("Pants", (0.32, 0.26, 0.36), 0.8),
                 ("Boot", LEATHER, 0.7), ("Steel", STEEL, 0.3), ("Gold", GOLD, 0.35), ("Wood", WOOD, 0.7),
                 ("Leather", LEATHER, 0.7), ("Team", TEAM, 0.6)]
    mats = {n.lower(): i for i, (n, _, _) in enumerate(materials)}
    if kind == "goblin":
        materials[mats["skin"]] = ("Skin", GOBLIN, 0.7)
    head_c, head_r = (0.0, 1.17, 0.02), 0.27
    if kind == "knight":
        arms_and_legs(skin, team, mats)
        team.blob((0, 0.70, 0), (0.27, 0.27, 0.21), mats["team"], SPINE, power=0.55)            # tunic
        skin.blob((0, 0.51, 0), (0.28, 0.05, 0.22), mats["leather"], HIPS, power=0.6)            # belt
        skin.blob((0, 0.51, 0.215), (0.06, 0.045, 0.02), mats["gold"], HIPS, power=0.6)          # buckle
        skin.blob(head_c, (head_r,) * 3, mats["skin"], HEAD)
        face(skin, mats, head_c, head_r, eye_y=-0.01)
        skin.blob((0, 1.27, -0.01), (0.30, 0.20, 0.30), mats["steel"], HEAD)                      # helmet dome
        skin.blob((0, 1.17, 0.0), (0.31, 0.04, 0.31), mats["steel"], HEAD, power=0.8)             # helmet rim
        team.blob((0, 1.50, -0.04), (0.05, 0.12, 0.14), mats["team"], HEAD)                       # plume
        h = add(joint_pos(ARMR), (-0.03, -0.36, 0.02))
        skin.blob(add(h, (0, 0.06, 0.07)), (0.13, 0.025, 0.04), mats["gold"], ARMR, power=0.6)   # guard
        skin.blob(add(h, (0, 0.36, 0.07)), (0.04, 0.30, 0.014), mats["steel"], ARMR, power=0.7)  # blade
        hl = add(joint_pos(ARML), (0.03, -0.32, 0.02))
        skin.blob(add(hl, (0.06, 0.0, 0.10)), (0.25, 0.27, 0.04), mats["gold"], ARML)             # shield rim
        team.blob(add(hl, (0.06, 0.0, 0.13)), (0.21, 0.23, 0.04), mats["team"], ARML)             # shield face
        skin.blob(add(hl, (0.06, 0.0, 0.17)), (0.06, 0.06, 0.03), mats["gold"], ARML)             # boss
    elif kind == "archer":
        arms_and_legs(skin, team, mats, thick=0.9)
        team.blob((0, 0.68, 0), (0.24, 0.29, 0.19), mats["team"], SPINE, power=0.6)               # tunic
        team.blob((0, 0.45, 0), (0.27, 0.10, 0.21), mats["team"], HIPS, power=0.8)                # skirt
        skin.blob(head_c, (head_r,) * 3, mats["skin"], HEAD)
        face(skin, mats, head_c, head_r, eye_y=-0.01, eye_size=1.1)
        team.blob((0, 1.24, -0.07), (0.30, 0.28, 0.29), mats["team"], HEAD)                       # hood
        team.blob((0, 1.32, -0.30), (0.08, 0.08, 0.14), mats["team"], HEAD,
                  rot=rot_matrix((1, 0, 0), 0.6))                                                 # hood tip
        skin.blob((0.0, 0.88, -0.20), (0.08, 0.20, 0.08), mats["leather"], SPINE,
                  rot=rot_matrix((0, 0, 1), 0.4))                                                 # quiver
        for k in range(3):
            skin.blob((-0.06 + k * 0.05, 1.10, -0.22), (0.02, 0.05, 0.02), mats["glint"], SPINE, rows=6, cols=8)
        hl = add(joint_pos(ARML), (0.03, -0.34, 0.06))

        def bow(t):
            a = (t - 0.5) * 2.4
            return (hl[0], hl[1] + 0.40 * math.sin(a), hl[2] + 0.16 * math.cos(a) - 0.04)
        skin.tube(bow, 0.025, mats["wood"], ARML)
    elif kind == "giant":
        arms_and_legs(skin, team, mats, sleeve_team=False, hand_r=0.14, thick=1.35)
        team.blob((0, 0.70, 0), (0.38, 0.32, 0.28), mats["team"], SPINE, power=0.65)              # shirt
        skin.blob((0, 0.50, 0), (0.37, 0.07, 0.28), mats["leather"], HIPS, power=0.6)             # belt
        skin.blob((0, 0.50, 0.28), (0.08, 0.06, 0.02), mats["gold"], HIPS, power=0.6)
        hc = (0.0, 1.13, 0.06)
        skin.blob(hc, (0.22, 0.21, 0.21), mats["skin"], HEAD)
        face(skin, mats, hc, 0.21, eye_y=0.0, eye_dx=0.08, eye_size=0.8, brows=True)
        skin.blob(add(hc, (0, -0.07, 0.19)), (0.07, 0.05, 0.05), mats["skin"], HEAD)             # nose
    elif kind == "goblin":
        arms_and_legs(skin, team, mats, sleeve_team=False, thick=0.85)
        team.blob((0, 0.66, 0), (0.22, 0.24, 0.18), mats["team"], SPINE, power=0.6)               # vest
        skin.blob((0, 0.48, 0), (0.21, 0.07, 0.17), mats["leather"], HIPS, power=0.8)
        hc = (0.0, 1.10, 0.04)
        skin.blob(hc, (0.30, 0.25, 0.27), mats["skin"], HEAD)
        face(skin, mats, hc, 0.27, eye_y=0.03, eye_dx=0.11, eye_size=1.15)
        skin.blob(add(hc, (0, -0.05, 0.27)), (0.06, 0.05, 0.08), mats["skin"], HEAD)             # nose
        for sx in (-1, 1):                                                                        # long ears
            skin.blob(add(hc, (sx * 0.36, 0.04, -0.02)), (0.16, 0.05, 0.08), mats["skin"], HEAD,
                      rot=rot_matrix((0, 0, 1), sx * 0.45))
        h = add(joint_pos(ARMR), (-0.03, -0.36, 0.02))
        skin.blob(add(h, (0, 0.20, 0.06)), (0.03, 0.17, 0.012), mats["steel"], ARMR, power=0.7)  # dagger
        skin.blob(add(h, (0, 0.04, 0.06)), (0.07, 0.02, 0.03), mats["leather"], ARMR, power=0.6)
    return skin, team, materials


def clips():
    def sample(duration, keys, pose, loop=True):
        times = [duration * i / keys for i in range(keys + 1)]
        tracks = {}
        for t in times:
            for j, (path, value) in pose(t / duration).items():
                tracks.setdefault(j, (path, []))[1].append(value)
        return times, tracks

    def rest(j): return local_translation(j)

    def idle(p):
        ph = 2 * math.pi * p
        return {HIPS: ("translation", add(rest(HIPS), (0, 0.012 * math.sin(ph), 0))),
                SPINE: ("rotation", euler(x=0.03 * math.sin(ph))),
                HEAD: ("rotation", euler(x=-0.03 * math.sin(ph), y=0.05 * math.sin(ph * 0.5))),
                ARML: ("rotation", euler(z=0.08 + 0.04 * math.sin(ph))),
                ARMR: ("rotation", euler(z=-0.08 - 0.04 * math.sin(ph)))}

    def walk(p):
        ph = 2 * math.pi * p
        s = math.sin(ph)
        return {HIPS: ("translation", add(rest(HIPS), (0, 0.04 * abs(math.sin(ph)), 0))),
                SPINE: ("rotation", euler(x=0.08, y=0.10 * s)),
                HEAD: ("rotation", euler(x=-0.05, y=-0.08 * s)),
                LEGL: ("rotation", euler(x=-0.65 * s)), LEGR: ("rotation", euler(x=0.65 * s)),
                ARML: ("rotation", euler(x=0.55 * s, z=0.12)), ARMR: ("rotation", euler(x=-0.55 * s, z=-0.12))}

    def attack(p):
        # Wind up (0 -> 0.35), strike (0.35 -> 0.55), recover.
        if p < 0.35:
            k = math.sin(p / 0.35 * math.pi / 2)
            arm, twist = -2.3 * k, -0.35 * k
        elif p < 0.55:
            k = (p - 0.35) / 0.2
            arm, twist = -2.3 + 3.0 * k, -0.35 + 0.6 * k
        else:
            k = (p - 0.55) / 0.45
            arm, twist = 0.7 * (1 - k), 0.25 * (1 - k)
        return {HIPS: ("translation", rest(HIPS)), SPINE: ("rotation", euler(x=0.1, y=twist)),
                ARMR: ("rotation", euler(x=arm, z=-0.15)), ARML: ("rotation", euler(x=0.3, z=0.2)),
                LEGL: ("rotation", euler(x=-0.25)), LEGR: ("rotation", euler(x=0.2))}

    return [("Idle",) + sample(2.0, 24, idle), ("Walk",) + sample(0.8, 16, walk), ("Attack",) + sample(0.7, 14, attack)]


def write_character(kind):
    skin, team, materials = humanoid(kind)
    # Nodes: 0 Model, 1 Skin (mesh 0), 2 Body (mesh 1), 3.. joints.
    nodes = [{"name": "Model", "children": [1, 2, 3]}, {"name": "Skin", "mesh": 0, "skin": 0}, {"name": "Body", "mesh": 1, "skin": 0}]
    for j, (name, parent, _) in enumerate(JOINTS):
        node = {"name": name, "translation": list(local_translation(j))}
        kids = [3 + k for k, (_, p, _) in enumerate(JOINTS) if p == j]
        if kids:
            node["children"] = kids
        nodes.append(node)
    ibm = []
    for _, _, p in JOINTS:
        ibm.append([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -p[0], -p[1], -p[2], 1])
    anims = []
    for name, times, tracks in clips():
        anims.append((name, times, {3 + j: (path, vals) for j, (path, vals) in tracks.items()}))
    write_glb(os.path.join(OUT, kind + ".glb"), [("Skin", skin, True), ("Body", team, True)], materials, nodes,
              skins={"name": "Rig", "joints": [3 + j for j in range(len(JOINTS))], "skeleton": 3}, animations=anims, ibm=ibm)
    ctrl = {
        "version": 1, "entry": "Idle",
        "parameters": [{"name": "moving", "type": "bool", "default": 0.0}, {"name": "attack", "type": "trigger", "default": 0.0}],
        "states": [{"name": s, "clip": f"assets://games/duel/{kind}.glb#{s}", "spriteClip": "", "speed": 1.0, "position": [i * 220, 0]}
                   for i, s in enumerate(["Idle", "Walk", "Attack"])],
        "transitions": [
            {"from": "Idle", "to": "Walk", "conditions": [{"param": "moving", "op": "true", "value": 0.0}], "exitTime": -1.0, "duration": 0.12},
            {"from": "Walk", "to": "Idle", "conditions": [{"param": "moving", "op": "false", "value": 0.0}], "exitTime": -1.0, "duration": 0.15},
            {"from": "*", "to": "Attack", "conditions": [{"param": "attack", "op": "true", "value": 0.0}], "exitTime": -1.0, "duration": 0.05},
            {"from": "Attack", "to": "Idle", "conditions": [], "exitTime": 0.95, "duration": 0.15},
        ],
    }
    with open(os.path.join(OUT, kind + ".agrctrl"), "w") as f:
        json.dump(ctrl, f, indent=2)
        f.write("\n")


def write_tower(name, scale, king):
    base, roof = Mesh(), Mesh()
    materials = [("Stone", (0.72, 0.68, 0.63), 0.9), ("StoneDark", (0.56, 0.53, 0.58), 0.9), ("Door", (0.40, 0.25, 0.14), 0.8),
                 ("Gold", GOLD, 0.35), ("Team", TEAM, 0.6), ("Window", DARK, 0.5)]
    R = 0.95 * scale
    prof = [(R * 1.08, 0.0), (R * 1.08, 0.12 * scale), (R, 0.2 * scale), (R * 0.94, 1.5 * scale), (R * 1.02, 1.62 * scale),
            (R * 1.08, 1.75 * scale), (R * 1.08, 1.86 * scale), (R * 0.2, 1.87 * scale)]
    base.lathe(prof, (0, 0, 0), 0)
    for k in range(10):                                                                           # crenellations
        a = 2 * math.pi * k / 10
        c = (math.cos(a) * R * 0.96, 2.0 * scale, math.sin(a) * R * 0.96)
        base.blob(c, (0.17 * scale, 0.16 * scale, 0.17 * scale), 1, power=0.45, rot=rot_matrix((0, 1, 0), -a), rows=8, cols=12)
    for k in range(7):                                                                            # stone rings
        a = 2 * math.pi * (k + 0.5) / 7
        base.blob((math.cos(a) * R * 0.97, 0.75 * scale + 0.25 * (k % 2) * scale, math.sin(a) * R * 0.97),
                  (0.16 * scale, 0.09 * scale, 0.16 * scale), 1, power=0.5, rot=rot_matrix((0, 1, 0), -a), rows=6, cols=10)
    base.blob((0, 0.38 * scale, R * 0.93), (0.25 * scale, 0.38 * scale, 0.08 * scale), 2, power=0.7)  # door
    base.blob((0, 1.18 * scale, R * 0.93), (0.12 * scale, 0.17 * scale, 0.05 * scale), 5, power=0.7)  # window
    # Team roof: a cone over the top, a banner pole, and for the king a golden crown.
    roof.lathe([(R * 0.9, 0.0), (R * 0.85, 0.08 * scale), (R * 0.45, 0.55 * scale), (R * 0.12, 0.85 * scale), (0.0, 0.92 * scale)],
               (0, 2.0 * scale, 0), 4)
    if king:
        for k in range(6):
            a = 2 * math.pi * k / 6
            base.lathe([(0.11, 0.0), (0.06, 0.18), (0.0, 0.26)], (math.cos(a) * 0.32, 2.85 * scale, math.sin(a) * 0.32), 3, cols=10)
        base.lathe([(0.36, 0.0), (0.36, 0.12)], (0, 2.83 * scale, 0), 3)
    else:
        base.blob((0, 3.05 * scale, 0), (0.03, 0.22, 0.03), 2, rows=6, cols=8)
        roof.blob((0.16, 3.16 * scale, 0), (0.15, 0.09, 0.015), 4, power=0.7, rows=6, cols=10)
    nodes = [{"name": "Model", "children": [1, 2]}, {"name": "Base", "mesh": 0}, {"name": "Roof", "mesh": 1}]
    write_glb(os.path.join(OUT, name + ".glb"), [("Base", base, False), ("Roof", roof, False)], materials, nodes)


# ----------------------------------------------------------------------------- textures


def textures():
    from PIL import Image, ImageDraw, ImageFilter
    S = 256
    rnd = random.Random(7)

    def tile_draw(img, fn):
        """Draws with wrap-around so the texture tiles seamlessly."""
        d = ImageDraw.Draw(img)
        for dx in (-S, 0, S):
            for dy in (-S, 0, S):
                fn(d, dx, dy)

    # Grass: two mown stripes, soft color patches, small darker tufts.
    g = Image.new("RGB", (S, S), (118, 196, 82))
    d = ImageDraw.Draw(g)
    for x in range(0, S, 64):
        d.rectangle([x, 0, x + 31, S], fill=(108, 186, 74))
    patches = [(rnd.randrange(S), rnd.randrange(S), rnd.randrange(14, 34), rnd.choice([(126, 204, 90), (100, 178, 70)])) for _ in range(26)]
    tufts = [(rnd.randrange(S), rnd.randrange(S)) for _ in range(70)]
    for color in ((126, 204, 90), (100, 178, 70)):  # soft patches: blur the mask only (no dark halo)
        mask = Image.new("L", (S, S), 0)
        tile_draw(mask, lambda d, dx, dy: [d.ellipse([x + dx - r, y + dy - r * 0.7, x + dx + r, y + dy + r * 0.7], fill=120)
                                           for x, y, r, c in patches if c == color])
        g.paste(Image.new("RGB", (S, S), color), (0, 0), mask.filter(ImageFilter.GaussianBlur(6)))
    tl = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    tile_draw(tl, lambda d, dx, dy: [d.polygon([(x + dx - 3, y + dy + 3), (x + dx, y + dy - 5), (x + dx + 3, y + dy + 3)], fill=(84, 150, 58, 255))
                                     for x, y in tufts])
    g.paste(tl, (0, 0), tl)
    g.save(os.path.join(OUT, "grass.png"), optimize=True)

    # Path / bridge: warm planks with dark seams and nail dots.
    p = Image.new("RGB", (S, S), (176, 118, 70))
    d = ImageDraw.Draw(p)
    for i, y in enumerate(range(0, S, 32)):
        shade = (176, 118, 70) if i % 2 else (164, 108, 62)
        d.rectangle([0, y, S, y + 31], fill=shade)
        d.line([0, y, S, y], fill=(110, 70, 40), width=3)
        off = (i * 53) % S
        for x in (off, (off + 128) % S):
            d.line([x, y, x, y + 31], fill=(120, 76, 44), width=3)
            for ny in (y + 8, y + 24):
                d.ellipse([x + 5, ny - 2, x + 9, ny + 2], fill=(96, 62, 38))
                d.ellipse([x - 9, ny - 2, x - 5, ny + 2], fill=(96, 62, 38))
    p.save(os.path.join(OUT, "path.png"), optimize=True)

    # Stone wall: rounded cobbles with a dark joint and a light top edge.
    st = Image.new("RGB", (S, S), (98, 92, 104))
    stones = []
    for row, y in enumerate(range(0, S, 32)):  # rows and widths add up to S: seamless tiling
        widths = [64 + rnd.randrange(-14, 15) for _ in range(3)]
        widths.append(S - sum(widths))
        x = (row % 2) * 32
        for w in widths:
            stones.append((x, y, w, 32, rnd.choice([(196, 188, 176), (182, 176, 168), (206, 198, 184)])))
            x += w
    tile_draw(st, lambda d, dx, dy: [d.rounded_rectangle([x + dx + 2, y + dy + 2, x + dx + w - 2, y + dy + h - 2], radius=10, fill=c)
                                     for x, y, w, h, c in stones])
    hl = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    tile_draw(hl, lambda d, dx, dy: [d.rounded_rectangle([x + dx + 8, y + dy + 5, x + dx + w - 10, y + dy + 10], radius=4, fill=(255, 255, 255, 70))
                                     for x, y, w, h, c in stones])
    st.paste(hl, (0, 0), hl)
    st.save(os.path.join(OUT, "stone.png"), optimize=True)
    print("textures: grass.png, path.png, stone.png")


def write_tank():
    """Mini tank (rigid, faces +Z): tracks with wheels, hull, team turret and plate, long gun."""
    base, team = Mesh(), Mesh()
    materials = [("Track", (0.30, 0.28, 0.32), 0.8), ("Wheel", (0.55, 0.55, 0.60), 0.5), ("Hull", (0.52, 0.56, 0.44), 0.6),
                 ("Steel", STEEL, 0.3), ("Gold", GOLD, 0.35), ("Team", TEAM, 0.6), ("Dark", DARK, 0.5)]
    for sx in (-1, 1):
        base.blob((sx * 0.42, 0.20, 0.0), (0.17, 0.19, 0.62), 0, power=0.55)                   # track
        for k in range(4):                                                                      # wheels
            base.blob((sx * 0.60, 0.19, -0.42 + k * 0.28), (0.03, 0.12, 0.12), 1, rows=8, cols=12)
    base.blob((0, 0.42, 0.0), (0.34, 0.17, 0.56), 2, power=0.6)                                 # hull
    team.blob((0, 0.55, -0.05), (0.30, 0.04, 0.46), 5, power=0.5)                               # plate
    team.blob((0, 0.72, -0.05), (0.29, 0.17, 0.31), 5, power=0.75)                              # turret
    base.blob((0.0, 0.90, -0.12), (0.10, 0.04, 0.10), 4, power=0.8)                             # hatch
    base.tube(lambda t: (0.0, 0.74, 0.18 + 0.78 * t), 0.065, 3)                                 # gun
    base.blob((0.0, 0.74, 0.97), (0.09, 0.09, 0.06), 6)                                         # muzzle
    for sx in (-1, 1):                                                                          # headlights
        base.blob((sx * 0.22, 0.45, 0.55), (0.06, 0.05, 0.03), 4, rows=6, cols=10)
    nodes = [{"name": "Model", "children": [1, 2]}, {"name": "Skin", "mesh": 0}, {"name": "Body", "mesh": 1}]
    write_glb(os.path.join(OUT, "tank.glb"), [("Skin", base, False), ("Body", team, False)], materials, nodes)


def card_icons():
    """Card portraits (256 px, transparent): the unit's head in the 3D models' style, dark outline."""
    from PIL import Image, ImageDraw
    S, K = 256, 4  # drawn 4x larger, then reduced (smooth edges)
    INK = (40, 26, 44, 255)  # the player's team is blue: team parts of the portraits are blue
    W = 9 * K

    def c(rgb): return tuple(int(v * 255) for v in rgb) + (255,)

    def new():
        img = Image.new("RGBA", (S * K, S * K), (0, 0, 0, 0))
        return img, ImageDraw.Draw(img)

    def ell(d, cx, cy, rx, ry, fill, w=W):
        d.ellipse([(cx - rx) * K, (cy - ry) * K, (cx + rx) * K, (cy + ry) * K], fill=fill, outline=INK, width=w)

    def eyes(d, cx, cy, dx=30, size=1.0, brows=False):
        for sx in (-1, 1):
            ex = cx + sx * dx
            ell(d, ex, cy, 11 * size, 16 * size, INK, 1)
            ell(d, ex + 4 * size, cy - 6 * size, 4.5 * size, 4.5 * size, (255, 255, 255, 255), 1)
            if brows:
                d.line([(ex - 16) * K, (cy - 22 - sx * -6) * K, (ex + 16) * K, (cy - 22 + sx * -6) * K], fill=INK, width=8 * K)

    def mouth(d, cx, cy, w=22):
        d.arc([(cx - w) * K, (cy - 12) * K, (cx + w) * K, (cy + 12) * K], 20, 160, fill=INK, width=6 * K)

    def save(img, name):
        img.resize((S, S), Image.LANCZOS).save(os.path.join(OUT, "card_" + name + ".png"), optimize=True)

    # Knight: steel helmet with a plume, round face.
    img, d = new()
    d.rounded_rectangle([118 * K, 18 * K, 140 * K, 70 * K], radius=10 * K, fill=c((0.30, 0.55, 1.0)), outline=INK, width=W)
    ell(d, 128, 140, 84, 84, c(SKIN))
    d.chord([40 * K, 40 * K, 216 * K, 190 * K], 180, 360, fill=c(STEEL), outline=INK, width=W)
    d.rectangle([40 * K, 110 * K, 216 * K, 124 * K], fill=c(STEEL), outline=INK, width=W)
    eyes(d, 128, 156)
    mouth(d, 128, 190)
    save(img, "knight")
    # Archer: hood with a pointed tip, bow arc on the side.
    img, d = new()
    d.arc([150 * K, 40 * K, 250 * K, 230 * K], 250, 110, fill=c(WOOD)[:3] + (255,), width=12 * K)
    ell(d, 120, 128, 96, 96, c((0.30, 0.55, 1.0)))
    d.polygon([(70 * K, 60 * K), (40 * K, 10 * K), (110 * K, 40 * K)], fill=c((0.30, 0.55, 1.0)), outline=INK, width=W)
    ell(d, 120, 150, 66, 66, c(SKIN))
    eyes(d, 120, 150, dx=26, size=1.1)
    mouth(d, 120, 184, 16)
    save(img, "archer")
    # Giant: wide bald head, frowning brows, big nose.
    img, d = new()
    ell(d, 128, 136, 100, 92, c(SKIN))
    eyes(d, 128, 130, dx=34, size=0.85, brows=True)
    ell(d, 128, 160, 22, 18, c((0.95, 0.72, 0.56)))
    d.line([100 * K, 196 * K, 156 * K, 196 * K], fill=INK, width=7 * K)
    save(img, "giant")
    # Goblin: long ears, big eyes, pointy nose.
    img, d = new()
    for sx in (-1, 1):
        d.polygon([((128 + sx * 70) * K, 110 * K), ((128 + sx * 126) * K, 80 * K), ((128 + sx * 70) * K, 160 * K)],
                  fill=c(GOBLIN), outline=INK, width=W)
    ell(d, 128, 140, 82, 76, c(GOBLIN))
    eyes(d, 128, 128, dx=32, size=1.2)
    d.polygon([(118 * K, 150 * K), (128 * K, 186 * K), (140 * K, 150 * K)], fill=c((0.38, 0.72, 0.28)), outline=INK, width=5 * K)
    mouth(d, 128, 200, 26)
    save(img, "goblin")
    # Mini tank: front view.
    img, d = new()
    d.rounded_rectangle([30 * K, 150 * K, 226 * K, 220 * K], radius=26 * K, fill=c((0.30, 0.28, 0.32)), outline=INK, width=W)
    for k in range(5):
        ell(d, 52 + k * 38, 186, 13, 13, c((0.55, 0.55, 0.60)), 5 * K)
    d.rounded_rectangle([50 * K, 112 * K, 206 * K, 162 * K], radius=20 * K, fill=c((0.52, 0.56, 0.44)), outline=INK, width=W)
    ell(d, 128, 100, 58, 40, c((0.30, 0.55, 1.0)))
    d.rounded_rectangle([118 * K, 18 * K, 138 * K, 90 * K], radius=8 * K, fill=c(STEEL), outline=INK, width=W)
    save(img, "tank")
    # Fireball: flame drop with a bright core.
    img, d = new()
    d.polygon([(128 * K, 10 * K), (196 * K, 120 * K), (60 * K, 120 * K)], fill=(255, 120, 40, 255))
    ell(d, 128, 160, 80, 80, (255, 120, 40, 255))
    d.line([(128 * K, 10 * K), (196 * K, 120 * K)], fill=INK, width=W)
    d.line([(128 * K, 10 * K), (60 * K, 120 * K)], fill=INK, width=W)
    ell(d, 128, 166, 48, 48, (255, 214, 80, 255), 1)
    ell(d, 128, 176, 24, 24, (255, 250, 210, 255), 1)
    save(img, "fireball")
    print("card icons: card_*.png")


if __name__ == "__main__":
    card_icons()
    write_tank()
    for kind in ("knight", "archer", "giant", "goblin"):
        write_character(kind)
    write_tower("tower", 1.0, False)
    write_tower("king_tower", 1.3, True)
    textures()
