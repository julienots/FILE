"""Générateurs organiques : créatures, monstres, animaux et personnages.

Construction réelle par graphe de squelette (Skin + Subdivision), tubes effilés, membranes
et lofts. Chaque modèle garde son squelette (rigging automatique exact) et sa spécification
(modification ultérieure par texte ou par Claude).
"""
from __future__ import annotations

import math

from mathutils import Vector

from .builder import (Model, SkinBody, add_subsurf, box, curve_points, cylinder, displace_detail, ellipsoid, join,
                      lathe, loft, ellipse_section, membrane, obj_from_bm, sweep, taper)

# ---------------------------------------------------------------------------
# Préréglages d'espèces (proportions relatives ; la taille finale est ajustée ensuite)
# ---------------------------------------------------------------------------
QUAD = {
    #            longueur corps, longueur pattes, rayon corps, cou, angle cou, tête, museau, queue, rayon queue, patte, oreilles, couleur, matière
    "dragon": dict(body=1.6, leg=0.7, r=0.46, neck=0.85, neck_up=0.6, head=0.26, snout=0.3, tail=2.4, tail_r=0.26,
                   leg_r=0.15, ears=0, color=(0.15, 0.3, 0.12), mat="scales", sprawl=0.12),
    "beast": dict(body=1.4, leg=0.8, r=0.38, neck=0.45, neck_up=0.35, head=0.24, snout=0.2, tail=1.0, tail_r=0.12,
                  leg_r=0.13, ears=0, color=(0.3, 0.12, 0.1), mat="organic", sprawl=0.05),
    "hydra": dict(body=1.5, leg=0.6, r=0.42, neck=1.1, neck_up=0.7, head=0.17, snout=0.25, tail=1.8, tail_r=0.22,
                  leg_r=0.13, ears=0, color=(0.1, 0.32, 0.25), mat="scales", sprawl=0.15),
    "griffin": dict(body=1.3, leg=0.75, r=0.33, neck=0.4, neck_up=0.8, head=0.18, snout=0.2, tail=1.0, tail_r=0.07,
                    leg_r=0.09, ears=0, color=(0.6, 0.45, 0.25), mat="fur", sprawl=0.03),
    "wolf": dict(body=1.0, leg=0.6, r=0.2, neck=0.3, neck_up=0.55, head=0.14, snout=0.17, tail=0.6, tail_r=0.07,
                 leg_r=0.055, ears=2, color=(0.42, 0.42, 0.44), mat="fur", sprawl=0.0),
    "cat": dict(body=0.95, leg=0.5, r=0.18, neck=0.22, neck_up=0.6, head=0.13, snout=0.06, tail=0.85, tail_r=0.05,
                leg_r=0.05, ears=2, color=(0.85, 0.45, 0.12), mat="fur", sprawl=0.0),
    "horse": dict(body=1.4, leg=1.1, r=0.3, neck=0.65, neck_up=1.0, head=0.15, snout=0.38, tail=0.7, tail_r=0.07,
                  leg_r=0.065, ears=2, color=(0.4, 0.22, 0.1), mat="fur", sprawl=0.0),
    "bear": dict(body=1.2, leg=0.55, r=0.38, neck=0.2, neck_up=0.3, head=0.2, snout=0.14, tail=0.12, tail_r=0.06,
                 leg_r=0.13, ears=2, color=(0.3, 0.18, 0.09), mat="fur", sprawl=0.02),
    "deer": dict(body=1.1, leg=0.95, r=0.22, neck=0.55, neck_up=1.1, head=0.12, snout=0.2, tail=0.15, tail_r=0.04,
                 leg_r=0.045, ears=2, color=(0.55, 0.36, 0.2), mat="fur", sprawl=0.0),
    "cow": dict(body=1.5, leg=0.75, r=0.4, neck=0.35, neck_up=0.4, head=0.17, snout=0.22, tail=0.7, tail_r=0.04,
                leg_r=0.09, ears=2, color=(0.9, 0.88, 0.85), mat="fur", sprawl=0.0),
    "rabbit": dict(body=0.55, leg=0.22, r=0.17, neck=0.08, neck_up=0.7, head=0.12, snout=0.05, tail=0.06, tail_r=0.05,
                   leg_r=0.05, ears=2, color=(0.65, 0.55, 0.45), mat="fur", sprawl=0.0, long_ears=True),
    "lizard": dict(body=0.9, leg=0.25, r=0.15, neck=0.15, neck_up=0.1, head=0.11, snout=0.15, tail=1.4, tail_r=0.09,
                   leg_r=0.045, ears=0, color=(0.25, 0.45, 0.15), mat="scales", sprawl=0.3),
}
BODY_COLORS_BEAST = (0.28, 0.08, 0.08)


def _c(v, default):
    return tuple(v[:3]) if v else default


def _scale(spec, key):
    return float(spec.get("scales", {}).get(key, 1.0))


def _material_for(m: Model, spec, default_preset, default_color, key="corps"):
    preset = spec.get("material") or default_preset
    color = _c(spec.get("colors", {}).get("primary"), default_color)
    color2 = spec.get("colors", {}).get("secondary")
    kw = dict(color=(*color, 1.0))
    if color2:
        kw["color2"] = (*color2[:3], 1.0)
    if spec.get("colors", {}).get("veins"):
        kw["veins"] = (*spec["colors"]["veins"][:3], 1.0)
    if spec.get("wet"):
        kw["wet"] = 0.6
    if spec.get("glow") and preset not in ("emission", "lava"):
        glow = spec.get("colors", {}).get("veins") or spec.get("colors", {}).get("secondary") or (1.0, 0.3, 0.05)
        kw["veins"] = (*glow[:3], 1.0)
        kw["emission"] = (*glow[:3], 1.0)
        kw["emission_strength"] = 0.0
    if preset == "scales" and spec.get("detail") == "high":
        kw["scale"] = 1.4
    return m.material(key, preset, **kw)


def _eye_material(m: Model, spec):
    col = _c(spec.get("colors", {}).get("eyes"), (0.95, 0.75, 0.1))
    glow = bool(spec.get("glow_eyes"))
    return m.material("yeux", "eye", color=(*col, 1.0), emission=(*col, 1.0) if glow else None,
                      emission_strength=12.0 if glow else 0.0)


def _horn_material(m: Model, spec):
    col = _c(spec.get("colors", {}).get("horns"), (0.85, 0.8, 0.68))
    if spec.get("glow") and spec.get("colors", {}).get("horns"):
        return m.material("cornes", "emission", color=(*col, 1.0), emission_strength=4.0)
    return m.material("cornes", "plain", color=(*col, 1.0), roughness=0.4, color2=(col[0] * 0.5, col[1] * 0.45, col[2] * 0.4, 1))


def _dark_material(m: Model, key="noir"):
    return m.material(key, "plain", color=(0.02, 0.02, 0.02, 1.0), roughness=0.3)


# ---------------------------------------------------------------------------
# Parties communes
# ---------------------------------------------------------------------------
def add_eyes(m: Model, spec, head_center: Vector, head_r: float, forward: Vector, count: int, bone: str,
             size: float = 0.2, spread: float = 0.55, height: float = 0.3):
    if count <= 0:
        return []
    mat = _eye_material(m, spec)
    pupil_mat = _dark_material(m, "pupille")
    glow = bool(spec.get("glow_eyes"))
    r = head_r * size * _scale(spec, "eyes")
    eyes = []
    fwd = forward.normalized()
    side = Vector((1, 0, 0))
    up = fwd.cross(side).normalized()
    if up.z < 0:
        up = -up
    rows = max(1, math.ceil(count / 2))
    k = 0
    for row in range(rows):
        n_in_row = min(2, count - k)
        z_off = height - row * 0.35
        for s in ([1, -1] if n_in_row == 2 else [0]):
            pos = head_center + fwd * (head_r * 0.78) + side * (s * head_r * spread * (1 - row * 0.1)) + up * (head_r * z_off)
            # pousse l'œil sur la surface
            pos += (pos - head_center).normalized() * (r * 0.15)
            e = ellipsoid(f"{m.root.name}_œil", (r, r, r), pos, segments=16, rings=8)
            m.add(e, mat, bone=bone, part="yeux")
            eyes.append(e)
            if not glow:
                pdir = ((pos - head_center).normalized() * 0.6 + fwd * 0.4).normalized()
                pp = pos + pdir * (r * 0.75)
                pu = ellipsoid(f"{m.root.name}_pupille", (r * 0.45, r * 0.45, r * 0.45), pp, segments=10, rings=6)
                m.add(pu, pupil_mat, bone=bone, part="yeux")
            k += 1
    return eyes


def add_horns(m: Model, spec, head_center: Vector, head_r: float, count: int, bone: str, style: str = "curved"):
    if count <= 0:
        return []
    mat = _horn_material(m, spec)
    s = _scale(spec, "horns")
    horns = []
    pairs = max(1, math.ceil(count / 2))
    k = 0
    for row in range(pairs):
        n = min(2, count - k)
        for side in ([1, -1] if n == 2 else [0]):
            base = head_center + Vector((side * head_r * (0.45 + 0.1 * row), head_r * (0.1 + 0.35 * row),
                                         head_r * (0.65 - 0.15 * row)))
            length = head_r * (2.2 - 0.4 * row) * s
            if style == "ram":
                d = Vector((side * 0.6, 0.6, 0.5))
                pts = curve_points(base, d, length * 1.3, 12, bend_axis=(side * 0.2, -0.3, 1) if side else (1, 0, 0),
                                   bend=math.radians(300) * (1 if side >= 0 else -1))
            elif style == "antler":
                return add_antlers(m, spec, head_center, head_r, bone)
            else:
                d = Vector((side * 0.35, 0.75, 0.6))
                pts = curve_points(base, d, length, 8, bend_axis=(1, 0, 0), bend=math.radians(-55))
            radii = taper(len(pts), head_r * 0.2 * min(1.4, s ** 0.5), 0.0, power=0.9)
            h = sweep(f"{m.root.name}_corne", pts, radii, sides=8, tip=True)
            m.add(h, mat, bone=bone, part="cornes")
            add_subsurf(h, 1)
            horns.append(h)
            k += 1
    return horns


def add_antlers(m: Model, spec, head_center, head_r, bone):
    mat = _horn_material(m, spec)
    parts = []
    for side in (1, -1):
        base = head_center + Vector((side * head_r * 0.4, head_r * 0.2, head_r * 0.7))
        main = curve_points(base, Vector((side * 0.5, 0.4, 1.0)), head_r * 4.0, 10, bend_axis=(0, 1, 0),
                            bend=math.radians(-25 * side))
        segs = [sweep(f"{m.root.name}_bois", main, taper(len(main), head_r * 0.12, head_r * 0.03), sides=6, tip=False)]
        for i in (3, 5, 7):
            p0 = main[i]
            br = curve_points(p0, Vector((side * 0.2, -0.6, 0.8)), head_r * 1.4, 4, bend_axis=(1, 0, 0),
                              bend=math.radians(20))
            segs.append(sweep(f"{m.root.name}_bois", br, taper(len(br), head_r * 0.07, 0.0), sides=6, tip=True))
        a = join(segs, f"{m.root.name}_bois")
        m.add(a, mat, bone=bone, part="cornes")
        parts.append(a)
    return parts


def add_spikes(m: Model, spec, points: list, radii: list, up_dirs: list | None = None, mat=None, scale=1.0):
    if not points:
        return None
    if mat is None:
        col = _c(spec.get("colors", {}).get("secondary") or spec.get("colors", {}).get("horns"), (0.8, 0.75, 0.6))
        if spec.get("glow"):
            mat = m.material("pointes", "emission", color=(*col, 1.0), emission_strength=4.0)
        else:
            mat = m.material("pointes", "plain", color=(*col, 1.0), roughness=0.35)
    s = _scale(spec, "spikes") * scale
    cones = []
    for i, (p, r) in enumerate(zip(points, radii)):
        up = Vector((0, 0.35, 1)) if up_dirs is None else Vector(up_dirs[i])
        base = Vector(p) + Vector((0, 0, r * 0.75))
        length = r * 0.9 * s
        pts = curve_points(base - up.normalized() * r * 0.15, up, length, 3, bend_axis=(1, 0, 0), bend=math.radians(-25))
        cones.append(sweep("pointe", pts, taper(len(pts), r * 0.28 * s ** 0.5, 0), sides=6, tip=True, flatten=0.45))
    obj = join(cones, f"{m.root.name}_Pointes")
    m.add(obj, mat, part="pointes")
    return obj


def add_claws(m: Model, spec, feet: list, size: float, forward=Vector((0, -1, 0))):
    if not feet:
        return None
    col = _c(spec.get("colors", {}).get("horns"), (0.12, 0.1, 0.08))
    mat = m.material("griffes", "plain", color=(*col, 1.0), roughness=0.25)
    cl = []
    for f in feet:
        f = Vector(f)
        for k in (-1, 0, 1):
            base = f + Vector((k * size * 0.9, -size * 0.8, size * 0.3))
            pts = curve_points(base, forward + Vector((k * 0.15, 0, -0.1)), size * 1.4, 4, bend_axis=(1, 0, 0),
                               bend=math.radians(70))
            cl.append(sweep("griffe", pts, taper(len(pts), size * 0.3, 0), sides=6, tip=True, flatten=0.7))
    obj = join(cl, f"{m.root.name}_Griffes")
    m.add(obj, mat, part="griffes")
    return obj


def add_teeth(m: Model, spec, jaw_front: Vector, width: float, size: float, bone: str):
    mat = m.material("dents", "plain", color=(0.95, 0.92, 0.82, 1), roughness=0.2)
    teeth = []
    for k in range(-3, 4):
        x = k / 3.0 * width
        base = jaw_front + Vector((x, abs(k) * size * 0.6, 0))
        long = size * (1.8 if abs(k) == 2 else 1.0)
        pts = [base, base + Vector((0, 0, -long * 0.6)), base + Vector((0, -size * 0.1, -long))]
        teeth.append(sweep("dent", pts, [size * 0.3, size * 0.2, 0], sides=5, tip=True))
    obj = join(teeth, f"{m.root.name}_Crocs")
    m.add(obj, mat, bone=bone, part="crocs")
    return obj


def add_bat_wing(m: Model, spec, shoulder: Vector, side: int, span: float, idx: int, parent_bone: str, mat):
    """Aile membraneuse (bras + doigts + membrane), avec ses os."""
    S = shoulder
    E = S + Vector((side * span * 0.32, span * 0.05, span * 0.28))
    W = E + Vector((side * span * 0.3, -span * 0.08, span * 0.06))
    finger_dirs = [Vector((side * 0.75, -0.55, 0.25)), Vector((side * 0.9, 0.0, -0.05)),
                   Vector((side * 0.75, 0.45, -0.25)), Vector((side * 0.4, 0.75, -0.45))]
    finger_len = [span * 0.62, span * 0.58, span * 0.5, span * 0.42]
    tips = []
    fingers = []
    for d, L in zip(finger_dirs, finger_len):
        pts = curve_points(W, d, L, 5, bend_axis=(0, 0, 1), bend=math.radians(12 * side))
        fingers.append(pts)
        tips.append(pts[-1])
    hip_line = curve_points(W, (S + Vector((side * span * 0.05, span * 0.65, -span * 0.12))) - W,
                            ((S + Vector((side * span * 0.05, span * 0.65, -span * 0.12))) - W).length, 5)
    arm_line = curve_points(W, S - W, (S - W).length, 5)
    rows = fingers + [hip_line, arm_line]
    # rangées intermédiaires avec affaissement (membrane tendue entre les doigts)
    import bmesh
    bm = bmesh.new()
    w_vert = bm.verts.new(W)
    all_rows = []
    for a, b in zip(rows[:-1], rows[1:]):
        for t in (0.0, 0.34, 0.67):
            row = []
            for j in range(1, len(a)):
                pa, pb = Vector(a[j]), Vector(b[j])
                p = pa.lerp(pb, t)
                sag = math.sin(math.pi * t) * (j / (len(a) - 1)) * span * 0.08
                inward = (W - p).normalized() * sag * 0.6
                p = p + Vector((0, 0, -sag)) + inward
                row.append(p)
            all_rows.append(row)
    all_rows.append([Vector(p) for p in rows[-1][1:]])
    vrows = [[bm.verts.new(p) for p in row] for row in all_rows]
    for ra, rb in zip(vrows[:-1], vrows[1:]):
        bm.faces.new((w_vert, ra[0], rb[0]))
        for j in range(len(ra) - 1):
            bm.faces.new((ra[j], ra[j + 1], rb[j + 1], rb[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mem = obj_from_bm(f"{m.root.name}_Membrane", bm)
    so = mem.modifiers.new("Épaisseur", "SOLIDIFY")
    so.thickness = span * 0.012
    so.offset = 0
    add_subsurf(mem, 1)
    wing_mat = m.materials.get("membrane")
    if wing_mat is None:
        col = _c(spec.get("colors", {}).get("wings") or spec.get("colors", {}).get("secondary"), None)
        if col is None:
            base = _c(spec.get("colors", {}).get("primary"), (0.15, 0.3, 0.12))
            col = tuple(min(1.0, c * 1.4 + 0.05) for c in base)
        wing_mat = m.material("membrane", "organic", color=(*col, 1.0),
                              color2=(col[0] * 0.5, col[1] * 0.5, col[2] * 0.5, 1.0), bump=0.6,
                              veins=(col[0] * 0.4, col[1] * 0.4, col[2] * 0.4, 1.0))
    sname = "G" if side > 0 else "D"
    sk = "L" if side > 0 else "R"
    m.add(mem, wing_mat, part="ailes")
    bones_tubes = [sweep("os_aile", [S, E, W], [span * 0.045, span * 0.035, span * 0.03], sides=8)]
    for f in fingers:
        bones_tubes.append(sweep("doigt", f, taper(len(f), span * 0.022, span * 0.004), sides=6, tip=False))
    arm_obj = join(bones_tubes, f"{m.root.name}_Aile.{sname}.{idx}")
    add_subsurf(arm_obj, 1)
    m.add(arm_obj, mat, part="ailes")
    b1 = m.bone(f"aile.{sname}.{idx}.1", S, E, parent_bone, f"wing_{sk}_1")
    b2 = m.bone(f"aile.{sname}.{idx}.2", E, W, b1, f"wing_{sk}_2")
    m.bone(f"aile.{sname}.{idx}.3", W, tips[1], b2, f"wing_{sk}_3")
    return [mem, arm_obj]


def add_feather_wing(m: Model, spec, shoulder: Vector, side: int, span: float, idx: int, parent_bone: str, mat):
    S = shoulder
    E = S + Vector((side * span * 0.35, span * 0.05, span * 0.12))
    W = E + Vector((side * span * 0.3, -span * 0.02, 0))
    T = W + Vector((side * span * 0.35, span * 0.1, -span * 0.05))
    arm = sweep("bras_aile", [S, E, W, T], [span * 0.06, span * 0.05, span * 0.04, span * 0.02], sides=8)
    feathers = [arm]
    n = 14
    for k in range(n):
        t = k / (n - 1)
        if t < 0.33:
            base = S.lerp(E, t / 0.33)
        elif t < 0.66:
            base = E.lerp(W, (t - 0.33) / 0.33)
        else:
            base = W.lerp(T, (t - 0.66) / 0.34)
        length = span * (0.28 + 0.25 * t)
        d = Vector((side * (0.25 + 0.6 * t), 1.0 - 0.5 * t, -0.08)).normalized()
        tip = base + d * length
        w = span * 0.06
        prof = [(-1, 0), (-0.5, 0.15), (0.5, 0.15), (1, 0), (0.5, -0.1), (-0.5, -0.1)]
        f = sweep("plume", [base, base.lerp(tip, 0.5), tip], [w * 0.6, w, w * 0.2], profile=prof, tip=False)
        feathers.append(f)
    wing = join(feathers, f"{m.root.name}_Aile.{'G' if side > 0 else 'D'}.{idx}")
    m.add(wing, mat, part="ailes")
    sname = "G" if side > 0 else "D"
    sk = "L" if side > 0 else "R"
    b1 = m.bone(f"aile.{sname}.{idx}.1", S, E, parent_bone, f"wing_{sk}_1")
    b2 = m.bone(f"aile.{sname}.{idx}.2", E, W, b1, f"wing_{sk}_2")
    m.bone(f"aile.{sname}.{idx}.3", W, T, b2, f"wing_{sk}_3")
    return [wing]


def add_tentacles(m: Model, spec, center: Vector, radius: float, count: int, length: float, mat, parent_bone: str,
                  downward=True, start_k=0):
    objs = []
    s = _scale(spec, "tentacles")
    for k in range(count):
        a = 2 * math.pi * (k + 0.5) / count
        d = Vector((math.cos(a), math.sin(a), -0.9 if downward else 0.2)).normalized()
        base = center + Vector((math.cos(a) * radius, math.sin(a) * radius, 0))
        pts = curve_points(base, d, length * s, 14, bend_axis=Vector((-math.sin(a), math.cos(a), 0)),
                           bend=math.radians(-160 if downward else 120), droop=0.0)
        radii = taper(len(pts), radius * 0.35 * min(1.5, s ** 0.5), radius * 0.02, power=0.8)
        t = sweep(f"{m.root.name}_Tentacule", pts, radii, sides=10, tip=True)
        add_subsurf(t, 1)
        m.add(t, mat, part="tentacules")
        objs.append(t)
        prev = parent_bone
        nb = 4
        step = (len(pts) - 1) // nb
        for i in range(nb):
            prev = m.bone(f"tentacule.{start_k + k + 1}.{i + 1}", pts[i * step],
                          pts[min((i + 1) * step, len(pts) - 1)], prev, f"tentacle_{start_k + k + 1}_{i + 1}")
    return objs


# ---------------------------------------------------------------------------
# Plan de corps quadrupède (dragon, bêtes, mammifères, lézards)
# ---------------------------------------------------------------------------
def build_quadruped(m: Model, spec: dict, preset: str):
    P = dict(QUAD[preset])
    feats = spec.get("features", {})
    build = spec.get("build")
    if build == "heavy":
        P["r"] *= 1.3
        P["leg_r"] *= 1.3
    elif build == "thin":
        P["r"] *= 0.8
        P["leg_r"] *= 0.8
    if spec.get("style") == "cartoon":
        P["head"] *= 1.6
        P["leg"] *= 0.8
    body_s = _scale(spec, "body")
    L, H, R = P["body"] * body_s, P["leg"] * _scale(spec, "legs"), P["r"] * body_s
    sb = SkinBody()
    z = H + R * 0.55
    # colonne
    spine_pts = [Vector((0, L / 2, z)), Vector((0, L / 6, z + R * 0.08)), Vector((0, -L / 6, z + R * 0.12)),
                 Vector((0, -L / 2, z + R * 0.15))]
    spine_r = [(R * 0.8, R * 0.85), (R * 0.95, R), (R, R * 1.05), (R * 0.92, R * 0.95)]
    spine = sb.chain(None, spine_pts, spine_r)
    sb.roots.add(spine[0])
    m.bone("racine", (0, 0, 0), (0, 0, R * 0.5), None, "root")
    m.bone("bassin", spine_pts[0], spine_pts[1], "racine", "spine")
    m.bone("colonne", spine_pts[1], spine_pts[2], "bassin", "spine")
    m.bone("torse", spine_pts[2], spine_pts[3], "colonne", "chest")
    # cous et têtes
    heads = max(1, int(feats.get("heads", 1) or 1))
    head_infos = []
    neck_len = P["neck"] * _scale(spec, "neck")
    head_r = P["head"] * _scale(spec, "heads") * (1.0 if heads == 1 else 0.85)
    for h in range(heads):
        spread = 0 if heads == 1 else (h / (heads - 1) - 0.5) * 1.6
        n_seg = 4 if neck_len > 0.5 else 2
        direction = Vector((spread * 0.6, -1.0, P["neck_up"])).normalized()
        start = spine_pts[-1]
        pts = curve_points(start, direction, neck_len, n_seg, bend_axis=(1, 0, 0), bend=math.radians(-15 if neck_len > 0.6 else 0))[1:]
        nr = [(max(R * (0.68 - 0.3 * i / n_seg), head_r * 0.75), max(R * (0.72 - 0.3 * i / n_seg), head_r * 0.8)) for i in range(n_seg)]
        neck = sb.chain(spine[-1], pts, nr)
        hc = pts[-1] + Vector((0, -head_r * 0.6, head_r * 0.15))
        snout_dir = Vector((spread * 0.2, -1, -0.15)).normalized()
        snout = hc + snout_dir * (head_r * 0.5 + P["snout"])
        hv = sb.chain(neck[-1], [hc, snout], [(head_r, head_r * 0.92), (head_r * 0.55, head_r * 0.45)])
        suffix = "" if heads == 1 else f".{h + 1}"
        nb = m.bone(f"cou{suffix}", start, pts[-1], "torse", "neck")
        hb = m.bone(f"tête{suffix}", pts[-1], snout, nb, "head")
        head_infos.append((hc, head_r, snout_dir, hb, snout))
    # queue(s)
    tails = int(feats.get("tails", 1 if P["tail"] > 0.2 else 0) if feats.get("tails") is not None else (1 if P["tail"] > 0.05 else 0))
    tail_tip_pts = []
    tail_pts_all = []
    for t in range(tails):
        spread = 0 if tails == 1 else (t / (tails - 1) - 0.5) * 1.0
        n_seg = 8 if P["tail"] > 1 else 5
        tl = P["tail"] * _scale(spec, "tails")
        pts = curve_points(spine_pts[0], Vector((spread, 1.0, -0.25)), tl, n_seg, bend_axis=(1, 0, 0),
                           bend=math.radians(25))[1:]
        radii = [(P["tail_r"] * (1 - i / n_seg) ** 0.8 + 0.01,) * 2 for i in range(n_seg)]
        radii[0] = (max(radii[0][0], R * 0.55),) * 2
        sb.chain(spine[0], pts, radii)
        prev = "bassin"
        bones_per = 3 if n_seg <= 5 else 4
        step = max(1, n_seg // bones_per)
        anchors = [spine_pts[0]] + [pts[min(len(pts) - 1, (i + 1) * step - 1)] for i in range(bones_per)]
        for i in range(bones_per):
            prev = m.bone(f"queue{'' if tails == 1 else '.' + str(t + 1)}.{i + 1}", anchors[i], anchors[i + 1], prev,
                          f"tail_{i + 1}")
        tail_tip_pts.append(pts[-1])
        tail_pts_all.append((pts, [r[0] for r in radii]))
    # pattes
    n_legs = int(feats.get("legs", 4) or 0)
    n_legs = max(0, min(n_legs + (n_legs % 2), 12))
    pairs = n_legs // 2
    feet = []
    leg_r = P["leg_r"]
    sprawl = P["sprawl"]
    for pi in range(pairs):
        t = 0 if pairs == 1 else pi / (pairs - 1)
        y = -L / 2 * 0.85 + t * L * 0.85 if pairs > 1 else 0.0
        front = t < 0.5 if pairs > 1 else True
        for side in (1, -1):
            hip = Vector((side * R * 0.55, y, z - R * 0.35))
            knee = Vector((side * (R * 0.62 + sprawl), y + (0.05 if front else -0.12) * H, H * 0.52))
            ankle = Vector((side * (R * 0.6 + sprawl * 1.2), y + (0.0 if front else 0.1) * H, H * 0.12))
            foot = Vector((side * (R * 0.6 + sprawl * 1.2), y - 0.14 * H - leg_r, leg_r * 0.6))
            attach = spine[1] if not front else spine[2]
            if pairs > 2:
                attach = spine[min(3, max(0, round((1 - t) * 3)))]
            legv = sb.chain(attach, [hip, knee, ankle, foot],
                            [(leg_r * 2.1, leg_r * 2.6), (leg_r * 1.2, leg_r * 1.3), (leg_r * 0.8, leg_r * 0.8),
                             (leg_r * 0.85, leg_r * 0.6)])
            sname = "G" if side > 0 else "D"
            sk = "L" if side > 0 else "R"
            fb = "front" if (pi % 2 == 0) else "back"
            if pairs == 2:
                fb = "front" if front else "back"
            fbn = "avant" if fb == "front" else "arrière"
            label = f"{fbn}{'' if pairs <= 2 else str(pi + 1)}"
            par = "torse" if front else "bassin"
            b1 = m.bone(f"patte.{label}.{sname}", hip, knee, par, f"leg_{sk}_{fb}_upper")
            b2 = m.bone(f"jarret.{label}.{sname}", knee, ankle, b1, f"leg_{sk}_{fb}_lower")
            m.bone(f"pied.{label}.{sname}", ankle, foot, b2, f"leg_{sk}_{fb}_foot")
            feet.append(foot)
    quality_subdiv = int(spec.get("_subdiv", 2))
    body = sb.build(f"{m.root.name}_Corps", subdiv=quality_subdiv)
    default_mat = "scales" if feats.get("scales") else ("fur" if feats.get("fur") else P["mat"])
    body_mat = _material_for(m, spec, default_mat, _c(None, P["color"]))
    m.add(body, body_mat, part="corps")
    if feats.get("scales") and spec.get("detail") == "high":
        displace_detail(body, strength=R * 0.025, scale=R * 0.12, kind="VORONOI", name="Écailles")
    # têtes : yeux, cornes, oreilles, crocs
    for (hc, hr, sd, hb, snout) in head_infos:
        add_eyes(m, spec, hc, hr, sd, int(feats.get("eyes", 2) if feats.get("eyes") is not None else 2), hb,
                 size=0.24 if preset in ("cat", "rabbit") or spec.get("style") == "cartoon" else 0.2,
                 spread=0.6, height=0.35)
        if feats.get("horns"):
            style = "antler" if preset == "deer" else ("ram" if spec.get("subtype") == "demon" else "curved")
            add_horns(m, spec, hc, hr, int(feats["horns"]), hb, style=style)
        elif preset == "deer" and feats.get("horns") is None:
            add_antlers(m, spec, hc, hr, hb)
        if P.get("ears"):
            ear_mat = m.materials.get("corps")
            for side in (1, -1):
                tall = 2.6 if P.get("long_ears") else 1.0
                base = hc + Vector((side * hr * 0.55, hr * 0.15, hr * 0.7))
                pts = [base, base + Vector((side * hr * 0.15, hr * 0.1, hr * 0.5 * tall)),
                       base + Vector((side * hr * 0.2, hr * 0.15, hr * 0.85 * tall))]
                ear = sweep(f"{m.root.name}_Oreille", pts, [hr * 0.3, hr * 0.22, 0], sides=8, tip=True, flatten=0.35)
                add_subsurf(ear, 1)
                m.add(ear, ear_mat, bone=hb, part="oreilles")
        if feats.get("fangs") or preset in ("dragon", "beast", "hydra") or spec.get("category") == "creature":
            add_teeth(m, spec, snout + Vector((0, hr * 0.15, -hr * 0.35)), hr * 0.35, hr * 0.12, hb)
    # pointes dorsales (dos + queue)
    if feats.get("spikes"):
        pts, rad = [], []
        for i in range(len(spine_pts) - 1):
            for t in (0.25, 0.75):
                pts.append(spine_pts[i].lerp(spine_pts[i + 1], t))
                rad.append(R * 0.55)
        for tp, tr in tail_pts_all:
            for p, r in zip(tp[:-1], tr[:-1]):
                pts.append(p)
                rad.append(max(r, R * 0.15) * 0.9)
        add_spikes(m, spec, pts, rad)
    # griffes
    if feats.get("claws") or preset in ("dragon", "beast", "bear", "hydra", "griffin"):
        add_claws(m, spec, feet, leg_r * 0.5)
    # ailes
    n_wings = int(feats.get("wings", 0) or 0)
    if n_wings:
        pairs_w = max(1, math.ceil(n_wings / 2))
        span = L * 1.4 * _scale(spec, "wings")
        wing_parts_mat = m.materials.get("corps")
        for k in range(pairs_w):
            shoulder_y = -L * 0.3 + k * L * 0.45
            s_span = span * (1 - 0.3 * k)
            for side in (1, -1):
                sh = Vector((side * R * 0.5, shoulder_y, z + R * 0.75))
                if preset == "griffin" or feats.get("feathers"):
                    fmat = m.material("plumes", "fur", color=(*_c(spec.get("colors", {}).get("wings"), (0.55, 0.42, 0.28)), 1.0))
                    add_feather_wing(m, spec, sh, side, s_span, k + 1, "torse", fmat)
                else:
                    add_bat_wing(m, spec, sh, side, s_span, k + 1, "torse", wing_parts_mat)
    # tentacules (sous le torse)
    n_t = int(feats.get("tentacles", 0) or 0)
    if n_t:
        add_tentacles(m, spec, Vector((0, -L * 0.1, z - R * 0.6)), R * 0.45, n_t, H * 1.1,
                      m.materials.get("corps"), "torse")
    return body


# ---------------------------------------------------------------------------
# Autres plans de corps
# ---------------------------------------------------------------------------
def build_serpent(m: Model, spec: dict):
    feats = spec.get("features", {})
    L = 4.0 * _scale(spec, "body")
    R = 0.12 * _scale(spec, "body") * (1.4 if spec.get("build") == "heavy" else 1.0)
    n = 24
    pts = [Vector((math.sin(i / n * math.pi * 3) * 0.45 * (i / n), -L / 2 + L * i / n, R * 0.9)) for i in range(n + 1)]
    radii = [R * (1.0 - 0.95 * max(0, (i - n * 0.55) / (n * 0.45))) if i > 2 else R * (0.8 + 0.1 * i) for i in range(n + 1)]
    sb = SkinBody()
    head_c = pts[0] + Vector((0, -R * 1.2, R * 0.3))
    snout = head_c + Vector((0, -R * 1.4, -R * 0.1))
    chain = sb.chain(None, [snout, head_c] + pts, [(R * 0.6, R * 0.45), (R * 1.35, R * 1.0)] +
                     [(r, r * 0.85) for r in radii])
    sb.roots.add(chain[2])
    body = sb.build(f"{m.root.name}_Corps", subdiv=int(spec.get("_subdiv", 2)))
    mat = _material_for(m, spec, "scales", (0.2, 0.35, 0.1))
    m.add(body, mat, part="corps")
    m.bone("racine", (0, 0, 0), (0, 0, R), None, "root")
    prev = m.bone("tête", head_c, snout, "racine", "head")
    nb = 8
    step = n // nb
    prev = "racine"
    for i in range(nb):
        prev = m.bone(f"corps.{i + 1}", pts[i * step], pts[(i + 1) * step], prev, f"tail_{i + 1}")
    add_eyes(m, spec, head_c, R * 1.2, Vector((0, -1, 0)), int(feats.get("eyes", 2) or 2), "tête", size=0.22,
             spread=0.7, height=0.4)
    if feats.get("horns"):
        add_horns(m, spec, head_c, R * 1.2, int(feats["horns"]), "tête")
    if feats.get("spikes"):
        add_spikes(m, spec, pts[1:-3:2], radii[1:-3:2])
    if feats.get("wings"):
        for side in (1, -1):
            add_bat_wing(m, spec, pts[3] + Vector((side * R * 0.6, 0, R * 0.6)), side, 1.2 * _scale(spec, "wings"), 1,
                         "corps.1", mat)
    return body


def build_octopus(m: Model, spec: dict):
    feats = spec.get("features", {})
    s = _scale(spec, "body")
    sb = SkinBody()
    c = Vector((0, 0, 1.4 * s))
    top = Vector((0, 0.35 * s, 2.3 * s))
    v = sb.chain(None, [c, top], [(0.55 * s, 0.6 * s), (0.42 * s, 0.4 * s)])
    sb.roots.add(v[0])
    body = sb.build(f"{m.root.name}_Manteau", subdiv=int(spec.get("_subdiv", 2)))
    mat = _material_for(m, spec, "organic", (0.5, 0.12, 0.18))
    m.add(body, mat, part="corps")
    m.bone("racine", (0, 0, 0), (0, 0, 0.3), None, "root")
    m.bone("tête", c, top, "racine", "head")
    n_t = int(feats.get("tentacles", 8) or 8)
    add_tentacles(m, spec, c + Vector((0, 0, -0.35 * s)), 0.4 * s, n_t, 2.0 * s, mat, "tête")
    add_eyes(m, spec, c + Vector((0, 0, 0.15 * s)), 0.55 * s, Vector((0, -1, 0)), int(feats.get("eyes", 2) or 2),
             "tête", size=0.25, spread=0.7, height=0.2)
    return body


def build_blob(m: Model, spec: dict):
    import bmesh
    feats = spec.get("features", {})
    s = _scale(spec, "body")
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=4, radius=0.8 * s)
    for v in bm.verts:
        v.co.z = v.co.z * 0.75 + 0.6 * s
        if v.co.z < 0.05:
            v.co.z = 0.05 + (v.co.z - 0.05) * 0.1
    body = obj_from_bm(f"{m.root.name}_Corps", bm)
    displace_detail(body, strength=0.12 * s, scale=0.5 * s, kind="CLOUDS", name="Bosses")
    add_subsurf(body, 1)
    mat = _material_for(m, spec, "organic", (0.3, 0.7, 0.2))
    if not spec.get("material"):
        mat = m.material("gelée", "organic", color=(*_c(spec.get("colors", {}).get("primary"), (0.3, 0.75, 0.2)), 1),
                         wet=0.9, bump=0.5)
    m.add(body, mat, part="corps")
    m.bone("racine", (0, 0, 0), (0, 0, 0.3), None, "root")
    m.bone("corps", (0, 0, 0.3), (0, 0, 1.1 * s), "racine", "chest")
    m.bone("tête", (0, 0, 1.1 * s), (0, -0.3, 1.2 * s), "corps", "head")
    add_eyes(m, spec, Vector((0, 0, 0.75 * s)), 0.8 * s, Vector((0, -1, 0.1)), int(feats.get("eyes", 2) or 2), "tête",
             size=0.18, spread=0.5, height=0.35)
    if feats.get("tentacles"):
        add_tentacles(m, spec, Vector((0, 0, 0.6 * s)), 0.6 * s, int(feats["tentacles"]), 1.2 * s, mat, "corps",
                      downward=False)
    return body


def build_bird(m: Model, spec: dict):
    feats = spec.get("features", {})
    s = _scale(spec, "body")
    sb = SkinBody()
    tail_base = Vector((0, 0.35 * s, 0.75 * s))
    chest = Vector((0, -0.15 * s, 0.85 * s))
    neck = Vector((0, -0.3 * s, 1.1 * s))
    head = Vector((0, -0.38 * s, 1.25 * s))
    v = sb.chain(None, [tail_base, Vector((0, 0.1 * s, 0.8 * s)), chest, neck, head],
                 [(0.12 * s, 0.1 * s), (0.22 * s, 0.24 * s), (0.24 * s, 0.26 * s), (0.11 * s, 0.11 * s),
                  (0.13 * s, 0.13 * s)])
    sb.roots.add(v[1])
    feet = []
    for side in (1, -1):
        hip = Vector((side * 0.1 * s, 0.05 * s, 0.7 * s))
        knee = Vector((side * 0.11 * s, 0.1 * s, 0.4 * s))
        ankle = Vector((side * 0.1 * s, 0.02 * s, 0.08 * s))
        sb.chain(v[1], [hip, knee, ankle], [(0.08 * s,) * 2, (0.035 * s,) * 2, (0.02 * s,) * 2])
        feet.append((ankle, side))
        sk, sn = ("L", "G") if side > 0 else ("R", "D")
        m.bone(f"cuisse.{sn}", hip, knee, "corps", f"leg_{sk}_upper")
        m.bone(f"tibia.{sn}", knee, ankle, f"cuisse.{sn}", f"leg_{sk}_lower")
    body = sb.build(f"{m.root.name}_Corps", subdiv=int(spec.get("_subdiv", 2)))
    col = _c(spec.get("colors", {}).get("primary"), (0.35, 0.25, 0.18))
    mat = m.material("plumage", spec.get("material") or "fur", color=(*col, 1.0))
    m.add(body, mat, part="corps")
    m.bone("racine", (0, 0, 0), (0, 0, 0.3 * s), None, "root")
    m.bone("corps", tail_base, chest, "racine", "chest")
    m.bone("cou", chest, neck, "corps", "neck")
    m.bone("tête", neck, head + Vector((0, -0.15 * s, 0)), "cou", "head")
    beak_mat = m.material("bec", "plain", color=(0.95, 0.7, 0.15, 1), roughness=0.3)
    beak = sweep(f"{m.root.name}_Bec", [head + Vector((0, -0.09 * s, -0.01 * s)), head + Vector((0, -0.2 * s, -0.03 * s)),
                                        head + Vector((0, -0.28 * s, -0.07 * s))], [0.05 * s, 0.03 * s, 0], sides=8, tip=True)
    m.add(beak, beak_mat, bone="tête", part="bec")
    add_eyes(m, spec, head, 0.13 * s, Vector((0, -1, 0)), int(feats.get("eyes", 2) or 2), "tête", size=0.25,
             spread=0.75, height=0.25)
    toes = []
    for ankle, side in feet:
        for k in (-1, 0, 1):
            toes.append(sweep("orteil", [ankle, ankle + Vector((k * 0.05 * s, -0.08 * s, -0.06 * s)),
                                         ankle + Vector((k * 0.08 * s, -0.13 * s, -0.075 * s))],
                              [0.015 * s, 0.012 * s, 0.004 * s], sides=6, tip=False))
        toes.append(sweep("orteil", [ankle, ankle + Vector((0, 0.07 * s, -0.07 * s))], [0.012 * s, 0.006 * s], sides=6))
    m.add(join(toes, f"{m.root.name}_Pattes"), beak_mat, part="pattes")
    wmat = m.material("plumes", "fur", color=(*_c(spec.get("colors", {}).get("wings") or spec.get("colors", {}).get("secondary"), col), 1.0))
    n_w = max(2, int(feats.get("wings", 2) or 2))
    for k in range(max(1, n_w // 2)):
        for side in (1, -1):
            add_feather_wing(m, spec, chest + Vector((side * 0.15 * s, 0.05 * s + k * 0.12 * s, 0.1 * s)), side,
                             1.0 * s * _scale(spec, "wings") * (1 - 0.2 * k), k + 1, "corps", wmat)
    # queue en éventail
    tail_feathers = []
    for k in range(7):
        a = (k / 6 - 0.5) * 0.9
        d = Vector((math.sin(a), math.cos(a), -0.1))
        tip = tail_base + d * 0.4 * s
        tail_feathers.append(sweep("plume", [tail_base, tail_base.lerp(tip, 0.6), tip], [0.03 * s, 0.05 * s, 0.01 * s],
                                   profile=[(-1, 0), (0, 0.15), (1, 0), (0, -0.1)]))
    m.add(join(tail_feathers, f"{m.root.name}_Queue"), wmat, part="queue")
    m.bone("queue.1", tail_base, tail_base + Vector((0, 0.35 * s, -0.04 * s)), "corps", "tail_1")
    return body


def build_fish(m: Model, spec: dict):
    feats = spec.get("features", {})
    s = _scale(spec, "body")
    is_shark = spec.get("keyword") in ("requin", "shark")
    L = 1.6 * s
    secs = []
    profile = [(0.0, 0.02), (0.08, 0.12), (0.25, 0.2), (0.45, 0.21), (0.65, 0.16), (0.82, 0.08), (0.95, 0.035)]
    for t, r in profile:
        secs.append(ellipse_section((0, 0, 0.5 * s), r * s * 0.55, r * s, 16, y=-L / 2 + t * L))
    body = loft(f"{m.root.name}_Corps", secs)
    add_subsurf(body, int(spec.get("_subdiv", 2)))
    mat = _material_for(m, spec, "skin" if is_shark else "scales", (0.5, 0.55, 0.6) if is_shark else (0.85, 0.5, 0.15))
    m.add(body, mat, part="corps")
    m.bone("racine", (0, 0, 0), (0, 0, 0.2 * s), None, "root")
    m.bone("tête", (0, -L * 0.1, 0.5 * s), (0, -L / 2, 0.5 * s), "racine", "head")
    m.bone("corps.1", (0, -L * 0.1, 0.5 * s), (0, L * 0.2, 0.5 * s), "racine", "tail_1")
    m.bone("corps.2", (0, L * 0.2, 0.5 * s), (0, L * 0.45, 0.5 * s), "corps.1", "tail_2")
    m.bone("corps.3", (0, L * 0.45, 0.5 * s), (0, L * 0.7, 0.5 * s), "corps.2", "tail_3")
    fin_mat = m.material("nageoires", "organic", color=(*_c(spec.get("colors", {}).get("secondary"),
                                                           tuple(min(1, c * 1.2) for c in _c(spec.get("colors", {}).get("primary"), (0.85, 0.5, 0.15)))), 1.0), bump=0.3)
    tail_root = Vector((0, L * 0.45, 0.5 * s))
    fins = [membrane("caudale", [[tail_root, tail_root + Vector((0, 0.22 * s, 0.28 * s)), tail_root + Vector((0, 0.32 * s, 0.4 * s))],
                                 [tail_root, tail_root + Vector((0, 0.18 * s, 0)), tail_root + Vector((0, 0.24 * s, 0))],
                                 [tail_root, tail_root + Vector((0, 0.22 * s, -0.28 * s)), tail_root + Vector((0, 0.32 * s, -0.4 * s))]],
                    thickness=0.01 * s)]
    top = Vector((0, -0.05 * L, 0.5 * s + 0.2 * s))
    fins.append(membrane("dorsale", [[top + Vector((0, -0.15 * s, 0)), top + Vector((0, -0.1 * s, 0.1 * s)), top + Vector((0, 0.02 * s, 0.3 * s if is_shark else 0.15 * s))],
                                     [top + Vector((0, 0.15 * s, 0)), top + Vector((0, 0.17 * s, 0.05 * s)), top + Vector((0, 0.2 * s, 0.08 * s))]],
                         thickness=0.01 * s))
    for side in (1, -1):
        b = Vector((side * 0.09 * s, -0.25 * L, 0.42 * s))
        fins.append(membrane("pectorale", [[b, b + Vector((side * 0.12 * s, 0.05 * s, -0.08 * s)), b + Vector((side * 0.22 * s, 0.12 * s, -0.12 * s))],
                                           [b + Vector((0, 0.08 * s, 0)), b + Vector((side * 0.08 * s, 0.14 * s, -0.04 * s)), b + Vector((side * 0.12 * s, 0.2 * s, -0.06 * s))]],
                             thickness=0.008 * s))
    fo = join(fins, f"{m.root.name}_Nageoires")
    m.add(fo, fin_mat, part="nageoires")
    add_eyes(m, spec, Vector((0, -L * 0.38, 0.55 * s)), 0.12 * s, Vector((0, -1, 0)), 2, "tête", size=0.35,
             spread=0.9, height=0.2)
    if is_shark or feats.get("fangs"):
        add_teeth(m, spec, Vector((0, -L * 0.47, 0.44 * s)), 0.05 * s, 0.015 * s, "tête")
    return body


def build_arthropod(m: Model, spec: dict):
    feats = spec.get("features", {})
    s = _scale(spec, "body")
    kw = spec.get("keyword") or ""
    is_spider = kw in ("araignee", "spider", "arachnide") or spec.get("subtype") == "spider_monster"
    is_crab = kw in ("crabe", "crab", "scorpion")
    n_legs = int(feats.get("legs", 8 if (is_spider or is_crab) else 6) or 6)
    col = _c(spec.get("colors", {}).get("primary"), (0.08, 0.06, 0.05) if is_spider else (0.4, 0.15, 0.08))
    mat = m.material("carapace", spec.get("material") or "plastic", color=(*col, 1.0), roughness=0.3)
    parts = []
    thorax_c = Vector((0, -0.1 * s, 0.45 * s))
    head_c = Vector((0, -0.42 * s, 0.45 * s))
    abd_c = Vector((0, 0.45 * s, 0.55 * s))
    if is_spider:
        parts.append(ellipsoid("céphalothorax", (0.28 * s, 0.33 * s, 0.2 * s), thorax_c))
        parts.append(ellipsoid("abdomen", (0.4 * s, 0.5 * s, 0.38 * s), abd_c))
        head_c = thorax_c + Vector((0, -0.25 * s, 0.05 * s))
    elif is_crab:
        parts.append(ellipsoid("carapace", (0.55 * s, 0.38 * s, 0.18 * s), thorax_c))
        head_c = thorax_c + Vector((0, -0.3 * s, 0.05 * s))
    else:
        parts.append(ellipsoid("tête", (0.15 * s, 0.15 * s, 0.13 * s), head_c))
        parts.append(ellipsoid("thorax", (0.18 * s, 0.25 * s, 0.15 * s), thorax_c))
        parts.append(ellipsoid("abdomen", (0.24 * s, 0.42 * s, 0.2 * s), abd_c))
    body = join(parts, f"{m.root.name}_Corps")
    add_subsurf(body, 1)
    m.add(body, mat, part="corps")
    m.bone("racine", (0, 0, 0), (0, 0, 0.2 * s), None, "root")
    m.bone("corps", abd_c, thorax_c, "racine", "chest")
    m.bone("tête", thorax_c, head_c + Vector((0, -0.1 * s, 0)), "corps", "head")
    legs = []
    pairs = max(1, n_legs // 2)
    for i in range(pairs):
        t = i / max(1, pairs - 1)
        for side in (1, -1):
            base = thorax_c + Vector((side * 0.2 * s, (-0.18 + 0.36 * t) * s, 0))
            a = math.radians(-50 + 100 * t)
            out = Vector((side * math.cos(a), math.sin(a), 0))
            knee = base + out * 0.45 * s + Vector((0, 0, 0.35 * s))
            foot = base + out * 0.95 * s + Vector((0, 0, -0.45 * s))
            pts = [base, base.lerp(knee, 0.5), knee, knee.lerp(foot, 0.5), foot]
            legs.append(sweep("patte", pts, [0.05 * s, 0.045 * s, 0.04 * s, 0.03 * s, 0.01 * s], sides=6, tip=False))
            sk, sn = ("L", "G") if side > 0 else ("R", "D")
            fb = "front" if i % 2 == 0 else "back"
            b1 = m.bone(f"patte.{i + 1}.{sn}", base, knee, "corps", f"leg_{sk}_{fb}_upper")
            m.bone(f"tarse.{i + 1}.{sn}", knee, foot, b1, f"leg_{sk}_{fb}_lower")
    if is_crab:
        for side in (1, -1):
            b = thorax_c + Vector((side * 0.4 * s, -0.3 * s, 0))
            e = b + Vector((side * 0.3 * s, -0.25 * s, 0.1 * s))
            legs.append(sweep("pince", [b, e], [0.08 * s, 0.1 * s], sides=8))
            legs.append(ellipsoid("pince", (0.13 * s, 0.2 * s, 0.09 * s), e + Vector((0, -0.18 * s, 0))))
    leg_obj = join(legs, f"{m.root.name}_Pattes")
    m.add(leg_obj, mat, part="pattes")
    n_eyes = int(feats.get("eyes", 8 if is_spider else 2) or 2)
    add_eyes(m, spec, head_c, 0.16 * s, Vector((0, -1, 0.2)), n_eyes, "tête", size=0.22 if n_eyes <= 2 else 0.14,
             spread=0.5, height=0.4)
    if not is_spider and not is_crab or feats.get("antennae"):
        ants = []
        for side in (1, -1):
            pts = curve_points(head_c + Vector((side * 0.05 * s, -0.08 * s, 0.1 * s)), Vector((side * 0.4, -1, 0.8)),
                               0.6 * s, 6, bend_axis=(1, 0, 0), bend=math.radians(60))
            ants.append(sweep("antenne", pts, taper(len(pts), 0.012 * s, 0.004 * s), sides=5))
        m.add(join(ants, f"{m.root.name}_Antennes"), mat, bone="tête", part="antennes")
    if feats.get("wings"):
        wmat = m.material("ailes_insecte", "glass", color=(0.8, 0.9, 1.0, 1.0))
        for side in (1, -1):
            b = thorax_c + Vector((side * 0.1 * s, 0, 0.12 * s))
            w = membrane("aile", [[b, b + Vector((side * 0.5 * s, 0.2 * s, 0.05 * s)), b + Vector((side * 0.9 * s, 0.5 * s, 0.05 * s))],
                                  [b + Vector((0, 0.15 * s, 0)), b + Vector((side * 0.4 * s, 0.6 * s, 0)), b + Vector((side * 0.6 * s, 0.8 * s, 0))]],
                         thickness=0.004 * s)
            sk = "L" if side > 0 else "R"
            m.add(w, wmat, bone=m.bone(f"aile.{'G' if side > 0 else 'D'}", b, b + Vector((side * 0.6 * s, 0.3 * s, 0)),
                                       "corps", f"wing_{sk}_1"), part="ailes")
    return body


# ---------------------------------------------------------------------------
# Humanoïdes (personnages, bipèdes monstrueux)
# ---------------------------------------------------------------------------
HUMANOID_TRAITS = {
    "orc": dict(color=(0.3, 0.45, 0.2), build="heavy", tusks=True, ears="pointed"),
    "orque": dict(color=(0.3, 0.45, 0.2), build="heavy", tusks=True, ears="pointed"),
    "troll": dict(color=(0.35, 0.42, 0.35), build="heavy", tusks=True, height=2.6),
    "ogre": dict(color=(0.55, 0.5, 0.35), build="heavy", tusks=True, height=2.8),
    "gobelin": dict(color=(0.35, 0.55, 0.2), build="thin", ears="pointed", height=1.1, head=1.3),
    "goblin": dict(color=(0.35, 0.55, 0.2), build="thin", ears="pointed", height=1.1, head=1.3),
    "elfe": dict(build="thin", ears="pointed", height=1.85), "elf": dict(build="thin", ears="pointed", height=1.85),
    "nain": dict(build="heavy", beard=True, height=1.3, head=1.15), "dwarf": dict(build="heavy", beard=True, height=1.3, head=1.15),
    "zombie": dict(color=(0.45, 0.55, 0.4), build="thin"), "squelette": dict(color=(0.88, 0.85, 0.75), build="thin"),
    "skeleton": dict(color=(0.88, 0.85, 0.75), build="thin"),
    "sorcier": dict(robe=True, hat=True, beard=True), "wizard": dict(robe=True, hat=True, beard=True),
    "mage": dict(robe=True, hat=True), "sorciere": dict(robe=True, hat=True), "witch": dict(robe=True, hat=True),
    "chevalier": dict(armor=True), "knight": dict(armor=True), "paladin": dict(armor=True), "soldat": dict(armor=True),
    "soldier": dict(armor=True), "guerrier": dict(armor=True, build="heavy"), "warrior": dict(armor=True, build="heavy"),
    "viking": dict(armor=True, beard=True, build="heavy"),
    "robot": dict(robot=True), "androide": dict(robot=True), "android": dict(robot=True), "cyborg": dict(robot=True),
    "astronaute": dict(robot=True), "astronaut": dict(robot=True),
    "demon": dict(color=(0.55, 0.06, 0.04), build="heavy", height=2.4, tail=True),
    "golem": dict(color=(0.45, 0.43, 0.4), build="heavy", height=3.0, material="stone", robot_blocky=True),
    "alien": dict(color=(0.4, 0.6, 0.5), build="thin", head=1.5, material="organic"),
    "enfant": dict(height=1.2, head=1.25), "child": dict(height=1.2, head=1.25), "garcon": dict(height=1.3, head=1.2),
    "fille": dict(height=1.3, head=1.2), "boy": dict(height=1.3, head=1.2), "girl": dict(height=1.3, head=1.2),
    "vampire": dict(color=(0.85, 0.82, 0.8), build="thin"), "momie": dict(color=(0.75, 0.68, 0.5), material="fabric"),
}


def build_humanoid(m: Model, spec: dict):
    feats = spec.get("features", {})
    kw = spec.get("keyword") or ""
    traits = dict(HUMANOID_TRAITS.get(kw, {}))
    if spec.get("subtype") in ("demon", "golem", "alien") and not traits:
        traits = dict(HUMANOID_TRAITS.get(spec["subtype"], {}))
    build = spec.get("build") or traits.get("build", "normal")
    cartoon = spec.get("style") == "cartoon"
    if traits.get("robot") or traits.get("robot_blocky"):
        return build_robot(m, spec, traits)
    H = 1.8  # taille de construction ; ajustée ensuite
    head_ratio = (1 / 4.0 if cartoon else 1 / 7.5) * traits.get("head", 1.0) * _scale(spec, "heads")
    head_r = H * head_ratio * 0.5
    w = {"thin": 0.85, "normal": 1.0, "heavy": 1.3}[build] * _scale(spec, "body")
    arm_s = _scale(spec, "arms")
    leg_s = _scale(spec, "legs")
    sb = SkinBody()
    hip_z = H * 0.53 * leg_s / max(leg_s, 1) if leg_s <= 1 else H * 0.53
    pelvis = Vector((0, 0, hip_z))
    spine = Vector((0, 0.01, H * 0.64))
    chest = Vector((0, 0.0, H * 0.74))
    neck = Vector((0, 0.01, H * 0.84))
    head_c = Vector((0, -0.01, H * 0.84 + head_r * 1.15))
    head_top = head_c + Vector((0, 0, head_r * 0.55))
    core = sb.chain(None, [pelvis, spine, chest, neck, head_c, head_top],
                    [(0.16 * w, 0.12 * w), (0.15 * w, 0.11 * w), (0.2 * w, 0.13 * w), (0.06 * w, 0.06 * w),
                     (head_r * 0.85, head_r * 0.92), (head_r * 0.62, head_r * 0.62)])
    sb.roots.add(core[0])
    # nez
    sb.chain(core[4], [head_c + Vector((0, -head_r * 0.85, -head_r * 0.12))], [(head_r * 0.14, head_r * 0.14)])
    m.bone("racine", (0, 0, 0), (0, 0, 0.12), None, "root")
    m.bone("bassin", pelvis, spine, "racine", "spine")
    m.bone("torse", spine, chest, "bassin", "chest")
    m.bone("cou", chest, neck, "torse", "neck")
    m.bone("tête", neck, head_top, "cou", "head")
    hands = []
    n_arms = int(feats.get("arms", 2) or 2)
    arm_pairs = max(1, n_arms // 2)
    for k in range(arm_pairs):
        dz = -k * 0.09 * H
        for side in (1, -1):
            sh = Vector((side * 0.21 * w, 0.01, H * 0.81 + dz))
            el = Vector((side * (0.22 * w + 0.3 * arm_s * 0.55), 0.03, H * 0.8 + dz - 0.27 * arm_s))
            wr = Vector((side * (0.24 * w + 0.35 * arm_s * 0.55), -0.04, H * 0.8 + dz - 0.52 * arm_s))
            hd = wr + Vector((side * 0.02, -0.03, -0.09))
            sb.chain(core[2], [sh, el, wr, hd], [(0.065 * w, 0.06 * w), (0.045 * w, 0.045 * w),
                                                 (0.035 * w, 0.03 * w), (0.045, 0.02)])
            sn, sk = ("G", "L") if side > 0 else ("D", "R")
            suf = "" if k == 0 else f".{k + 1}"
            b0 = m.bone(f"épaule.{sn}{suf}", chest + Vector((side * 0.05, 0, 0.05 + dz)), sh, "torse", f"shoulder_{sk}")
            b1 = m.bone(f"bras.{sn}{suf}", sh, el, b0, f"arm_{sk}_upper")
            b2 = m.bone(f"avant-bras.{sn}{suf}", el, wr, b1, f"arm_{sk}_lower")
            m.bone(f"main.{sn}{suf}", wr, hd, b2, f"arm_{sk}_hand")
            hands.append((hd, side))
    feet = []
    for side in (1, -1):
        hp = Vector((side * 0.1 * w, 0, hip_z - 0.04))
        kn = Vector((side * 0.11 * w, -0.02, hip_z * 0.52))
        an = Vector((side * 0.11 * w, 0.02, 0.08))
        toe = Vector((side * 0.11 * w, -0.17, 0.03))
        sb.chain(core[0], [hp, kn, an, toe], [(0.095 * w, 0.09 * w), (0.06 * w, 0.06 * w), (0.045 * w, 0.045),
                                              (0.05, 0.03)])
        sn, sk = ("G", "L") if side > 0 else ("D", "R")
        b1 = m.bone(f"cuisse.{sn}", hp, kn, "bassin", f"leg_{sk}_upper")
        b2 = m.bone(f"tibia.{sn}", kn, an, b1, f"leg_{sk}_lower")
        m.bone(f"pied.{sn}", an, toe, b2, f"leg_{sk}_foot")
        feet.append(toe)
    body = sb.build(f"{m.root.name}_Corps", subdiv=int(spec.get("_subdiv", 2)))
    default_col = traits.get("color", (0.82, 0.6, 0.48))
    preset = spec.get("material") or traits.get("material") or ("organic" if spec.get("category") == "creature" else "skin")
    if feats.get("scales"):
        preset = "scales"
    mat = _material_for(m, spec, preset, default_col)
    m.add(body, mat, part="corps")
    # yeux, oreilles, cheveux
    add_eyes(m, spec, head_c, head_r, Vector((0, -1, 0)), int(feats.get("eyes", 2) if feats.get("eyes") is not None else 2),
             "tête", size=0.24 if cartoon else 0.17, spread=0.42, height=0.12)
    ear_style = traits.get("ears")
    for side in (1, -1):
        base = head_c + Vector((side * head_r * 0.86, 0.02, -head_r * 0.05))
        if ear_style == "pointed":
            pts = [base, base + Vector((side * head_r * 0.3, head_r * 0.15, head_r * 0.3)),
                   base + Vector((side * head_r * 0.55, head_r * 0.3, head_r * 0.7))]
            r = [head_r * 0.2, head_r * 0.14, 0]
        else:
            pts = [base, base + Vector((side * head_r * 0.12, head_r * 0.05, head_r * 0.1))]
            r = [head_r * 0.2, head_r * 0.15]
        ear = sweep(f"{m.root.name}_Oreille", pts, r, sides=8, tip=ear_style == "pointed", flatten=0.4)
        add_subsurf(ear, 1)
        m.add(ear, mat, bone="tête", part="oreilles")
    hair_col = spec.get("colors", {}).get("secondary")
    if spec.get("category") == "character" and kw not in ("squelette", "skeleton", "zombie", "robot", "alien", "golem") \
            and not traits.get("hat"):
        hair = ellipsoid(f"{m.root.name}_Cheveux", (head_r * 1.05, head_r * 1.08, head_r * 0.85),
                         head_c + Vector((0, head_r * 0.12, head_r * 0.25)), segments=20, rings=10)
        _cut_below(hair, head_c.z + head_r * 0.0, keep_back=head_c.y + head_r * 0.2)
        displace_detail(hair, strength=head_r * 0.06, scale=head_r * 0.25, kind="CLOUDS", name="Mèches")
        hmat = m.material("cheveux", "fur", color=(*_c(hair_col, (0.12, 0.07, 0.04)), 1.0))
        m.add(hair, hmat, bone="tête", part="cheveux")
    if traits.get("beard"):
        beard = sweep(f"{m.root.name}_Barbe", [head_c + Vector((0, -head_r * 0.6, -head_r * 0.45)),
                                                head_c + Vector((0, -head_r * 0.75, -head_r * 1.0)),
                                                head_c + Vector((0, -head_r * 0.6, -head_r * 1.7))],
                      [head_r * 0.55, head_r * 0.45, 0.0], sides=10, tip=True, flatten=0.6)
        add_subsurf(beard, 1)
        m.add(beard, m.material("barbe", "fur", color=(*_c(hair_col, (0.55, 0.52, 0.5)), 1.0)), bone="tête", part="barbe")
    if traits.get("tusks"):
        for side in (1, -1):
            b = head_c + Vector((side * head_r * 0.3, -head_r * 0.75, -head_r * 0.5))
            t = sweep("défense", [b, b + Vector((side * head_r * 0.05, -head_r * 0.1, head_r * 0.3))],
                      [head_r * 0.08, 0], sides=6, tip=True)
            m.add(t, m.material("dents", "plain", color=(0.95, 0.92, 0.82, 1), roughness=0.2), bone="tête", part="crocs")
    if traits.get("robe"):
        rc = _c(spec.get("colors", {}).get("secondary") or spec.get("colors", {}).get("primary"), (0.15, 0.15, 0.45))
        robe = lathe(f"{m.root.name}_Robe", [(0.0, 0.02), (0.36 * w, 0.02), (0.3 * w, H * 0.3), (0.22 * w, H * 0.55),
                                              (0.19 * w, H * 0.72), (0.16 * w, H * 0.8), (0.07, H * 0.84)], 24)
        add_subsurf(robe, 1)
        m.add(robe, m.material("robe", "fabric", color=(*rc, 1.0)), part="vêtements")
    if traits.get("hat"):
        hc = _c(spec.get("colors", {}).get("secondary"), (0.15, 0.15, 0.45))
        hat = lathe(f"{m.root.name}_Chapeau", [(0.0, 0), (head_r * 2.0, 0), (head_r * 1.9, head_r * 0.08),
                                                (head_r * 0.95, head_r * 0.15), (head_r * 0.6, head_r * 1.2),
                                                (0.0, head_r * 2.4)], 24)
        hat.location = head_c + Vector((0, 0, head_r * 0.45))
        m.add(hat, m.material("chapeau", "fabric", color=(*hc, 1.0)), bone="tête", part="vêtements")
    if traits.get("armor") or spec.get("material") == "metal":
        metal = m.material("armure", "metal", color=(*_c(spec.get("colors", {}).get("secondary"), (0.7, 0.72, 0.75)), 1.0))
        pieces = []
        chest_plate = ellipsoid("plastron", (0.2 * w, 0.15 * w, 0.2), Vector((0, -0.01, H * 0.72)), 20, 12)
        _cut_below(chest_plate, H * 0.6)
        pieces.append(chest_plate)
        for side in (1, -1):
            pad = ellipsoid("épaulière", (0.11 * w, 0.11 * w, 0.07), Vector((side * 0.24 * w, 0.01, H * 0.81)), 14, 8)
            _cut_below(pad, H * 0.79)
            pieces.append(pad)
        armor = join(pieces, f"{m.root.name}_Armure")
        so = armor.modifiers.new("Épaisseur", "SOLIDIFY")
        so.thickness = 0.012
        m.add(armor, metal, bone="torse", part="armure")
        helm = ellipsoid(f"{m.root.name}_Casque", (head_r * 1.12, head_r * 1.15, head_r * 1.08),
                         head_c + Vector((0, 0.01, head_r * 0.12)), 20, 12)
        _cut_below(helm, head_c.z - head_r * 0.2)
        so = helm.modifiers.new("Épaisseur", "SOLIDIFY")
        so.thickness = 0.01
        m.add(helm, metal, bone="tête", part="armure")
    # attributs de créature sur un bipède
    if feats.get("horns"):
        add_horns(m, spec, head_c, head_r, int(feats["horns"]), "tête",
                  style="ram" if spec.get("subtype") == "demon" else "curved")
    if feats.get("wings"):
        pairs_w = max(1, math.ceil(int(feats["wings"]) / 2))
        for k in range(pairs_w):
            for side in (1, -1):
                add_bat_wing(m, spec, Vector((side * 0.12 * w, 0.12 + 0.05 * k, H * (0.76 - 0.06 * k))), side,
                             H * 0.9 * _scale(spec, "wings") * (1 - 0.15 * k), k + 1, "torse", mat)
    tails = int(feats.get("tails", 1 if traits.get("tail") else 0) or 0)
    tail_pts_all = []
    for t in range(tails):
        spread = 0 if tails == 1 else (t / (tails - 1) - 0.5)
        pts = curve_points(pelvis + Vector((0, 0.08, -0.02)), Vector((spread, 1, -0.6)), H * 0.6 * _scale(spec, "tails"), 8,
                           bend_axis=(1, 0, 0), bend=math.radians(70))
        radii = taper(len(pts), 0.05 * w, 0.008)
        tail = sweep(f"{m.root.name}_Queue", pts, radii, sides=10, tip=True)
        add_subsurf(tail, 1)
        m.add(tail, mat, part="queue")
        prev = "bassin"
        for i in range(3):
            prev = m.bone(f"queue{'' if tails == 1 else '.' + str(t + 1)}.{i + 1}", pts[i * 3 if i else 0],
                          pts[min((i + 1) * 3, len(pts) - 1)], prev, f"tail_{i + 1}")
        tail_pts_all.append((pts, radii))
    if feats.get("tentacles"):
        add_tentacles(m, spec, Vector((0, 0, hip_z - 0.05)), 0.12 * w, int(feats["tentacles"]), H * 0.5, mat, "bassin")
    if feats.get("spikes"):
        pts = [Vector((0, 0.1 * w, z)) for z in (H * 0.6, H * 0.66, H * 0.72, H * 0.78)]
        rad = [0.1 * w] * len(pts)
        for tp, tr in tail_pts_all:
            pts += tp[1:-2]
            rad += [max(r, 0.03) for r in tr[1:-2]]
        add_spikes(m, spec, pts, rad, up_dirs=[(0, 1, 0.4)] * 4 + [(0, 0.3, 1)] * (len(pts) - 4))
    if feats.get("claws") or spec.get("category") == "creature":
        add_claws(m, spec, [h for h, _ in hands], 0.02)
    return body


def _cut_below(obj, z_world, keep_back=None):
    """Supprime la partie d'un maillage sous une hauteur (casques, cheveux, armures)."""
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    kill = [v for v in bm.verts if (obj.matrix_world @ v.co).z < z_world and
            (keep_back is None or (obj.matrix_world @ v.co).y < keep_back)]
    if kill:
        bmesh.ops.delete(bm, geom=kill, context="VERTS")
    bm.to_mesh(obj.data)
    bm.free()


def build_robot(m: Model, spec: dict, traits: dict):
    """Personnage mécanique (robot, golem de pierre) en pièces rigides, chaque pièce liée à son os."""
    feats = spec.get("features", {})
    H = 1.9
    blocky = traits.get("robot_blocky")
    col = _c(spec.get("colors", {}).get("primary"), traits.get("color", (0.7, 0.72, 0.75)))
    if blocky:
        mat = m.material("pierre", spec.get("material") or "stone", color=(*col, 1.0))
    else:
        mat = m.material("métal", spec.get("material") or "metal", color=(*col, 1.0))
    joint = m.material("articulations", "metal", color=(0.15, 0.15, 0.17, 1.0), roughness=0.4)
    w = 1.3 if (spec.get("build") == "heavy" or blocky) else 1.0
    bev = 0.06 if blocky else 0.02

    def part(name, size, loc, bone, material=mat):
        o = box(f"{m.root.name}_{name}", size, loc, bevel=bev, segments=2 if not blocky else 1)
        if blocky:
            displace_detail(o, strength=0.03, scale=0.15, kind="CLOUDS", name="Roche")
        m.add(o, material, bone=bone, part=name)
        return o

    m.bone("racine", (0, 0, 0), (0, 0, 0.1), None, "root")
    m.bone("bassin", (0, 0, H * 0.5), (0, 0, H * 0.6), "racine", "spine")
    m.bone("torse", (0, 0, H * 0.6), (0, 0, H * 0.8), "bassin", "chest")
    m.bone("tête", (0, 0, H * 0.84), (0, 0, H * 0.98), "torse", "head")
    part("Bassin", (0.36 * w, 0.22 * w, 0.14), (0, 0, H * 0.52), "bassin")
    part("Torse", (0.5 * w, 0.3 * w, 0.36), (0, 0, H * 0.7), "torse")
    head = part("Tête", (0.24, 0.24, 0.22), (0, 0, H * 0.91), "tête")
    part("Cou", (0.08, 0.08, 0.08), (0, 0, H * 0.84), "tête", joint)
    eye_mat = _eye_material(m, {**spec, "glow_eyes": True, "colors": {**spec.get("colors", {}),
                                                                        "eyes": spec.get("colors", {}).get("eyes", (0.2, 0.8, 1.0))}})
    visor = box(f"{m.root.name}_Visière", (0.18, 0.02, 0.04), (0, -0.125, H * 0.92), bevel=0.008)
    m.add(visor, eye_mat, bone="tête", part="yeux")
    for side in (1, -1):
        sn, sk = ("G", "L") if side > 0 else ("D", "R")
        x = side * 0.33 * w
        m.bone(f"bras.{sn}", (x, 0, H * 0.82), (x + side * 0.05, 0, H * 0.6), "torse", f"arm_{sk}_upper")
        m.bone(f"avant-bras.{sn}", (x + side * 0.05, 0, H * 0.6), (x + side * 0.07, -0.02, H * 0.4), f"bras.{sn}", f"arm_{sk}_lower")
        part(f"Épaule.{sn}", (0.16 * w, 0.18 * w, 0.14 * w), (x, 0, H * 0.82), f"bras.{sn}")
        part(f"Bras.{sn}", (0.1 * w, 0.1 * w, 0.2), (x + side * 0.025, 0, H * 0.71), f"bras.{sn}")
        part(f"AvantBras.{sn}", (0.12 * w, 0.12 * w, 0.2), (x + side * 0.06, -0.01, H * 0.5), f"avant-bras.{sn}")
        part(f"Main.{sn}", (0.09 * w, 0.1 * w, 0.1), (x + side * 0.07, -0.02, H * 0.37), f"avant-bras.{sn}", joint)
        lx = side * 0.12 * w
        m.bone(f"cuisse.{sn}", (lx, 0, H * 0.48), (lx, 0, H * 0.26), "bassin", f"leg_{sk}_upper")
        m.bone(f"tibia.{sn}", (lx, 0, H * 0.26), (lx, 0, H * 0.05), f"cuisse.{sn}", f"leg_{sk}_lower")
        part(f"Cuisse.{sn}", (0.15 * w, 0.16 * w, 0.22), (lx, 0, H * 0.37), f"cuisse.{sn}")
        part(f"Genou.{sn}", (0.1, 0.1, 0.06), (lx, -0.01, H * 0.26), f"tibia.{sn}", joint)
        part(f"Tibia.{sn}", (0.14 * w, 0.15 * w, 0.24), (lx, 0, H * 0.15), f"tibia.{sn}")
        part(f"Pied.{sn}", (0.15 * w, 0.28, 0.07), (lx, -0.06, 0.035), f"tibia.{sn}")
    if feats.get("horns"):
        add_horns(m, spec, Vector((0, 0, H * 0.91)), 0.13, int(feats["horns"]), "tête")
    return head


# ---------------------------------------------------------------------------
# Point d'entrée
# ---------------------------------------------------------------------------
CREATURE_PLANS = {
    "dragon": ("quad", "dragon"), "hydra": ("quad", "hydra"), "griffin": ("quad", "griffin"),
    "beast": ("quad", "beast"), "kraken": ("octopus", None), "blob": ("blob", None), "demon": ("humanoid", None),
    "golem": ("humanoid", None), "alien": ("humanoid", None), "spider_monster": ("arthropod", None),
}
ANIMAL_PLANS = {
    "wolf": ("quad", "wolf"), "cat": ("quad", "cat"), "horse": ("quad", "horse"), "bear": ("quad", "bear"),
    "deer": ("quad", "deer"), "cow": ("quad", "cow"), "rabbit": ("quad", "rabbit"), "lizard": ("quad", "lizard"),
    "bird": ("bird", None), "fish": ("fish", None), "snake": ("serpent", None), "spider": ("arthropod", None),
}


def build_organic(m: Model, spec: dict):
    cat = spec.get("category")
    sub = spec.get("subtype")
    plan = spec.get("body_plan")
    preset = None
    if plan is None:
        if cat == "character":
            plan = "humanoid"
        elif cat == "animal":
            plan, preset = ANIMAL_PLANS.get(sub, ("quad", "wolf"))
        else:
            plan, preset = CREATURE_PLANS.get(sub, ("quad", "beast"))
            kw = spec.get("keyword") or ""
            if kw in ("minotaure", "minotaur", "loup-garou", "werewolf", "troll", "ogre"):
                plan, preset = "humanoid", None
            if kw in ("serpent", "basilic", "basilisk") or (spec.get("features", {}).get("legs") == 0 and sub != "blob"):
                plan = "serpent"
    else:
        preset = spec.get("species") or ("dragon" if plan == "quad" else None)
    if plan == "quad":
        if preset not in QUAD:
            preset = "beast"
        if cat == "creature" and preset == "beast" and not spec.get("features", {}).get("horns") is not None:
            spec.setdefault("features", {}).setdefault("horns", 2)
        if preset == "lizard" and (spec.get("keyword") in ("tortue", "turtle")):
            spec.setdefault("features", {})["shell"] = True
        body = build_quadruped(m, spec, preset)
        if spec.get("features", {}).get("shell"):
            shell = ellipsoid(f"{m.root.name}_Carapace", (0.35, 0.5, 0.25), Vector((0, 0, 0.32)), 24, 12)
            _cut_below(shell, 0.3)
            m.add(shell, m.material("carapace", "stone", color=(0.3, 0.35, 0.15, 1)), part="carapace")
    elif plan == "serpent":
        body = build_serpent(m, spec)
    elif plan == "octopus":
        body = build_octopus(m, spec)
    elif plan == "blob":
        body = build_blob(m, spec)
    elif plan == "bird":
        body = build_bird(m, spec)
    elif plan == "fish":
        body = build_fish(m, spec)
    elif plan == "arthropod":
        body = build_arthropod(m, spec)
    else:
        body = build_humanoid(m, spec)
    spec["body_plan"] = plan
    if preset:
        spec["species"] = preset
    return body
