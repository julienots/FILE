"""Générateurs « hard surface » : bâtiments, véhicules, armes fictives, objets et décors."""
from __future__ import annotations

import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

from .builder import (Model, add_bevel, add_subsurf, box, curve_points, cylinder, displace_detail, ellipse_section,
                      ellipsoid, join, lathe, loft, membrane, obj_from_bm, sweep, taper)


def _col(spec, key, default):
    c = spec.get("colors", {}).get(key)
    return (*c[:3], 1.0) if c else (*default, 1.0)


# ---------------------------------------------------------------------------
# Bâtiments
# ---------------------------------------------------------------------------
def _facade_wall(name, width, height, floors, windows, depth=0.25, door=True, window_w=0.9, window_h=1.2):
    """Mur avec de vraies ouvertures en retrait (inset + extrusion vers l'intérieur)."""
    bm = bmesh.new()
    cols = max(1, windows) * 2 + 1
    rows = floors * 2 + 1
    xs = [0.0]
    cell_w = width / max(1, windows)
    for i in range(max(1, windows)):
        x0 = i * cell_w
        m = (cell_w - window_w) / 2
        xs += [x0 + m, x0 + m + window_w]
    xs.append(width)
    floor_h = height / floors
    zs = [0.0]
    for f in range(floors):
        z0 = f * floor_h
        sill = floor_h * 0.35 if f > 0 or not door else floor_h * 0.35
        zs += [z0 + sill, min(z0 + sill + window_h, z0 + floor_h * 0.9)]
    zs.append(height)
    grid = [[bm.verts.new((x - width / 2, 0, z)) for x in xs] for z in zs]
    faces = {}
    for r in range(len(zs) - 1):
        for c in range(len(xs) - 1):
            faces[(r, c)] = bm.faces.new((grid[r][c], grid[r][c + 1], grid[r + 1][c + 1], grid[r + 1][c]))
    openings = []
    door_col = (len(xs) - 1) // 2 if windows % 2 == 1 else 1
    for f in range(floors):
        for i in range(max(1, windows)):
            r = 1 + f * 2
            c = 1 + i * 2
            if f == 0 and door and c == door_col:
                continue
            openings.append(faces[(r, c)])
    door_face = None
    if door:
        # porte : fusionne la cellule du rez-de-chaussée de la colonne centrale
        c = door_col
        door_face = bmesh.utils.face_join([faces[(0, c)], faces[(1, c)]]) if (0, c) in faces else None
    for fc in openings + ([door_face] if door_face else []):
        if fc is None:
            continue
        res = bmesh.ops.extrude_face_region(bm, geom=[fc])
        verts = [g for g in res["geom"] if isinstance(g, bmesh.types.BMVert)]
        bmesh.ops.translate(bm, verts=verts, vec=Vector((0, depth * 0.6, 0)))
        bmesh.ops.delete(bm, geom=[fc], context="FACES_ONLY")
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    obj = obj_from_bm(name, bm, smooth=False)
    so = obj.modifiers.new("Épaisseur", "SOLIDIFY")
    so.thickness = depth
    so.offset = 1
    return obj, openings


def build_building(m: Model, spec: dict):
    sub = spec.get("subtype", "house")
    feats = spec.get("features", {})
    wall_preset = spec.get("material") or {"house": "brick", "tower": "stone", "castle": "stone", "temple": "stone",
                                           "modern": "plain", "shop": "wood"}.get(sub, "stone")
    wall_mat = m.material("murs", wall_preset, color=_col(spec, "primary", {"brick": (0.6, 0.3, 0.2), "stone": (0.55, 0.52, 0.48),
                                                                           "wood": (0.5, 0.32, 0.18), "plain": (0.85, 0.85, 0.82)}.get(wall_preset, (0.6, 0.55, 0.5))))
    roof_mat = m.material("toit", "plain" if sub == "modern" else "stone",
                          color=_col(spec, "secondary", (0.45, 0.15, 0.1) if sub in ("house", "shop") else (0.25, 0.27, 0.32)),
                          roughness=0.7)
    glass = m.material("vitres", "glass", color=(0.6, 0.75, 0.85, 1.0))
    trim = m.material("boiseries", "wood", color=(0.35, 0.2, 0.1, 1.0))
    floors = int(spec.get("floors", {"house": 2, "tower": 5, "castle": 2, "temple": 1, "modern": 8, "shop": 2}.get(sub, 2)))
    if sub == "castle":
        return _castle(m, spec, wall_mat, roof_mat)
    if sub == "tower":
        return _tower(m, spec, wall_mat, roof_mat, glass, floors)
    if sub == "temple":
        return _temple(m, spec, wall_mat, roof_mat)
    fh = 3.0
    W = {"house": 8.0, "shop": 7.0, "modern": 14.0}.get(sub, 8.0)
    D = {"house": 6.5, "shop": 6.0, "modern": 12.0}.get(sub, 6.0)
    H = floors * fh
    wins_w = max(1, int(W / 2.4))
    wins_d = max(1, int(D / 2.6))
    walls = []
    for k, (w, rot, off) in enumerate([(W, 0, (0, -D / 2, 0)), (W, math.pi, (0, D / 2, 0)),
                                       (D, -math.pi / 2, (W / 2, 0, 0)), (D, math.pi / 2, (-W / 2, 0, 0))]):
        wall, _ = _facade_wall(f"{m.root.name}_Mur", w, H, floors, wins_w if k < 2 else wins_d,
                               door=(k == 0), depth=0.3, window_w=1.4 if sub == "modern" else 1.0,
                               window_h=2.0 if sub == "modern" else 1.3)
        wall.rotation_euler.z = rot
        wall.location = off
        walls.append(wall)
    for w in walls:
        bpy.context.view_layer.update()
        w.data.transform(w.matrix_basis)
        w.matrix_basis = Matrix.Identity(4)
    wall_obj = join(walls, f"{m.root.name}_Murs")
    m.add(wall_obj, wall_mat, part="murs")
    # vitres et planchers
    glass_obj = box(f"{m.root.name}_Vitres", (W - 0.5, D - 0.5, H - 0.4), (0, 0, H / 2))
    m.add(glass_obj, glass, part="vitres")
    floor_obj = box(f"{m.root.name}_Sol", (W, D, 0.25), (0, 0, 0.125))
    m.add(floor_obj, trim if sub != "modern" else wall_mat, part="sol")
    # toit
    if sub == "modern":
        roof = box(f"{m.root.name}_Toit", (W + 0.4, D + 0.4, 0.4), (0, 0, H + 0.2), bevel=0.05)
    else:
        rh = D * 0.45
        bm = bmesh.new()
        o = 0.5
        v = [bm.verts.new(p) for p in [(-W / 2 - o, -D / 2 - o, H), (W / 2 + o, -D / 2 - o, H), (W / 2 + o, D / 2 + o, H),
                                       (-W / 2 - o, D / 2 + o, H), (-W / 2 - o, 0, H + rh), (W / 2 + o, 0, H + rh)]]
        for f in [(v[0], v[1], v[5], v[4]), (v[2], v[3], v[4], v[5]), (v[1], v[2], v[5]), (v[3], v[0], v[4]),
                  (v[3], v[2], v[1], v[0])]:
            bm.faces.new(f)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        roof = obj_from_bm(f"{m.root.name}_Toit", bm, smooth=False)
        so = roof.modifiers.new("Épaisseur", "SOLIDIFY")
        so.thickness = 0.2
        if feats.get("chimney", True) and sub == "house":
            ch = box(f"{m.root.name}_Cheminée", (0.7, 0.7, rh + 1.2), (W * 0.25, D * 0.15, H + rh / 2 + 0.4))
            m.add(ch, wall_mat, part="cheminée")
    m.add(roof, roof_mat, part="toit")
    door = box(f"{m.root.name}_Porte", (1.0 if wins_w % 2 else 1.0, 0.08, 2.2), (0, -D / 2 + 0.15, 1.1), bevel=0.02)
    m.add(door, trim, part="porte")
    return wall_obj


def _castle(m, spec, wall_mat, roof_mat):
    S = 18.0
    th = 9.0
    parts = []
    walls = []
    for k in range(4):
        a = k * math.pi / 2
        wall = box("rempart", (S, 1.6, 7.0), (0, 0, 3.5), bevel=0.05)
        cren = []
        for i in range(int(S / 1.6)):
            cren.append(box("créneau", (0.8, 1.6, 0.9), (-S / 2 + 0.8 + i * 1.6, 0, 7.45)))
        wall = join([wall] + cren)
        wall.data.transform(Matrix.Rotation(a, 4, "Z") @ Matrix.Translation((0, -S / 2, 0)))
        walls.append(wall)
    m.add(join(walls, f"{m.root.name}_Remparts"), wall_mat, part="remparts")
    roofs = []
    for k in range(4):
        x = (S / 2) * (1 if k in (0, 3) else -1)
        y = (S / 2) * (1 if k < 2 else -1)
        t = cylinder("tour", 2.4, th, (x, y, th / 2), segments=20)
        cr = [box("créneau", (0.7, 0.7, 0.9), (x + 2.2 * math.cos(i * math.pi / 6), y + 2.2 * math.sin(i * math.pi / 6), th + 0.45))
              for i in range(12)]
        m.add(join([t] + cr, f"{m.root.name}_Tour"), wall_mat, part="tours")
        roofs.append(cylinder("toit", 2.8, 4.5, (x, y, th + 2.25), segments=20, radius_top=0.01))
    keep = box(f"{m.root.name}_Donjon", (8, 8, 14), (0, 2, 7), bevel=0.05)
    m.add(keep, wall_mat, part="donjon")
    roofs.append(cylinder("toit", 6.0, 5.0, (0, 2, 16.5), segments=4, radius_top=0.01))
    roofs[-1].rotation_euler.z = math.pi / 4
    bpy.context.view_layer.update()
    roofs[-1].data.transform(roofs[-1].matrix_basis)
    roofs[-1].matrix_basis = Matrix.Identity(4)
    m.add(join(roofs, f"{m.root.name}_Toits"), roof_mat, part="toits")
    gate = box(f"{m.root.name}_Porte", (3.0, 0.3, 4.0), (0, -S / 2 - 0.9, 2.0), bevel=0.05)
    m.add(gate, m.material("porte", "wood", color=(0.3, 0.18, 0.08, 1)), part="porte")
    return keep


def _tower(m, spec, wall_mat, roof_mat, glass, floors):
    r = 3.0
    H = floors * 3.2
    tower = lathe(f"{m.root.name}_Tour", [(0, 0), (r * 1.15, 0), (r * 1.1, 1.0), (r, 1.4), (r * 0.92, H),
                                          (r * 1.1, H + 0.2), (r * 1.1, H + 1.0), (0, H + 1.0)], 28, smooth=False)
    m.add(tower, wall_mat, part="tour")
    wins = []
    for f in range(1, floors):
        for k in range(4):
            a = k * math.pi / 2 + f * 0.4
            wins.append(box("fenêtre", (0.9, 0.5, 1.3), (math.cos(a) * r * 0.93, math.sin(a) * r * 0.93, f * 3.2 + 1.2)))
            wins[-1].rotation_euler.z = a + math.pi / 2
            bpy.context.view_layer.update()
            wins[-1].data.transform(wins[-1].matrix_basis)
            wins[-1].matrix_basis = Matrix.Identity(4)
    if wins:
        m.add(join(wins, f"{m.root.name}_Fenêtres"), glass, part="fenêtres")
    roof = lathe(f"{m.root.name}_Toit", [(0, H + 1.0), (r * 1.3, H + 1.0), (0.05, H + 1.0 + r * 2.2)], 28, smooth=False)
    m.add(roof, roof_mat, part="toit")
    return tower


def _temple(m, spec, wall_mat, roof_mat):
    W, D = 12.0, 18.0
    steps = [box("marche", (W + 2 - i, D + 2 - i, 0.4), (0, 0, 0.2 + 0.4 * i)) for i in range(3)]
    base = join(steps, f"{m.root.name}_Soubassement")
    m.add(base, wall_mat, part="soubassement")
    cols = []
    for i in range(6):
        for y in (-D / 2 + 1, D / 2 - 1):
            x = -W / 2 + 1 + i * (W - 2) / 5
            cols.append(lathe("colonne", [(0, 0), (0.6, 0), (0.6, 0.3), (0.45, 0.4), (0.4, 6.0), (0.55, 6.2), (0.55, 6.5), (0, 6.5)], 16))
            cols[-1].location = (x, y, 1.2)
    for j in range(1, 5):
        for x in (-W / 2 + 1, W / 2 - 1):
            cols.append(lathe("colonne", [(0, 0), (0.6, 0), (0.6, 0.3), (0.45, 0.4), (0.4, 6.0), (0.55, 6.2), (0.55, 6.5), (0, 6.5)], 16))
            cols[-1].location = (x, -D / 2 + 1 + j * (D - 2) / 5, 1.2)
    for c in cols:
        bpy.context.view_layer.update()
        c.data.transform(c.matrix_basis)
        c.matrix_basis = Matrix.Identity(4)
    m.add(join(cols, f"{m.root.name}_Colonnes"), wall_mat, part="colonnes")
    ent = box(f"{m.root.name}_Entablement", (W + 1, D + 1, 1.0), (0, 0, 8.2))
    m.add(ent, wall_mat, part="entablement")
    bm = bmesh.new()
    v = [bm.verts.new(p) for p in [(-W / 2 - 0.6, -D / 2 - 0.6, 8.7), (W / 2 + 0.6, -D / 2 - 0.6, 8.7),
                                   (W / 2 + 0.6, D / 2 + 0.6, 8.7), (-W / 2 - 0.6, D / 2 + 0.6, 8.7),
                                   (0, -D / 2 - 0.6, 11.0), (0, D / 2 + 0.6, 11.0)]]
    for f in [(v[0], v[1], v[4]), (v[2], v[3], v[5]), (v[1], v[2], v[5], v[4]), (v[3], v[0], v[4], v[5]), (v[3], v[2], v[1], v[0])]:
        bm.faces.new(f)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    roof = obj_from_bm(f"{m.root.name}_Fronton", bm, smooth=False)
    m.add(roof, roof_mat if spec.get("colors", {}).get("secondary") else wall_mat, part="toit")
    return base


# ---------------------------------------------------------------------------
# Véhicules
# ---------------------------------------------------------------------------
def _wheel(name, r, w, loc, mat_tire, mat_rim, m):
    tire = cylinder(f"{name}_Pneu", r, w, loc, segments=32, axis="X")
    add_bevel(tire, r * 0.15, 3, 30)
    rim = cylinder(f"{name}_Jante", r * 0.62, w * 1.04, loc, segments=24, axis="X")
    m.add(tire, mat_tire, part="roues")
    m.add(rim, mat_rim, part="roues")


def build_vehicle(m: Model, spec: dict):
    sub = spec.get("subtype", "car")
    body_col = _col(spec, "primary", {"car": (0.7, 0.05, 0.04), "truck": (0.1, 0.3, 0.6), "spaceship": (0.85, 0.86, 0.88),
                                      "boat": (0.9, 0.9, 0.88), "plane": (0.85, 0.86, 0.88), "tank": (0.3, 0.35, 0.2),
                                      "bike": (0.1, 0.1, 0.12)}.get(sub, (0.6, 0.6, 0.6)))
    paint = m.material("carrosserie", spec.get("material") or ("metal" if sub != "boat" else "plastic"), color=body_col,
                       roughness=0.25, metallic=0.6 if sub in ("car", "truck", "spaceship", "plane") else 0.2)
    glass = m.material("vitres", "glass", color=(0.15, 0.2, 0.25, 1.0))
    tire = m.material("pneus", "plastic", color=(0.03, 0.03, 0.03, 1.0), roughness=0.85)
    rim = m.material("jantes", "metal", color=(0.75, 0.76, 0.78, 1.0))
    if sub in ("car", "truck"):
        L = 4.4 if sub == "car" else 6.5
        Wd = 1.85 if sub == "car" else 2.4
        Hb = 0.75 if sub == "car" else 1.2
        prof = [(-0.5, 0.45, 0.25), (-0.47, 0.75, 0.5), (-0.3, 0.85, 0.75), (0.0, 0.88, 0.8), (0.3, 0.88, 0.8),
                (0.45, 0.82, 0.7), (0.5, 0.6, 0.45)]
        secs = []
        for t, wf, hf in prof:
            y = t * L
            secs.append(ellipse_section((0, y, 0.35 + Hb * hf / 2), Wd / 2 * wf, Hb * hf / 2, 16, squareness=0.7, y=y))
        body = loft(f"{m.root.name}_Carrosserie", secs)
        add_subsurf(body, 2)
        m.add(body, paint, part="carrosserie")
        # habitacle
        cab_len = 0.45 if sub == "car" else 0.25
        cab_y0 = -0.1 * L if sub == "car" else -0.42 * L
        csecs = []
        for t, wf, hf in [(0, 0.82, 0.0), (0.2, 0.75, 0.9), (0.75, 0.72, 1.0), (1.0, 0.8, 0.0)]:
            y = cab_y0 + t * cab_len * L
            csecs.append(ellipse_section((0, y, 0.35 + Hb * 0.8 + 0.28 * hf), Wd / 2 * wf, 0.28 * max(hf, 0.15), 12,
                                         squareness=0.6, y=y))
        cab = loft(f"{m.root.name}_Habitacle", csecs)
        add_subsurf(cab, 1)
        m.add(cab, glass, part="vitres")
        if sub == "truck":
            cargo = box(f"{m.root.name}_Benne", (Wd, L * 0.55, 1.6), (0, L * 0.18, 0.35 + Hb * 0.8 + 0.8), bevel=0.04)
            m.add(cargo, paint, part="benne")
        r = 0.36 if sub == "car" else 0.5
        for y in (-0.32 * L, 0.32 * L):
            for side in (1, -1):
                _wheel(f"{m.root.name}_Roue", r, 0.26, (side * (Wd / 2 - 0.05), y, r), tire, rim, m)
        lights = m.material("phares", "emission", color=(1, 0.95, 0.8, 1), emission_strength=8)
        for side in (1, -1):
            hl = ellipsoid("phare", (0.15, 0.04, 0.07), (side * Wd * 0.33, -L / 2 + 0.03, 0.75), 12, 6)
            m.add(hl, lights, part="phares")
        return body
    if sub == "spaceship":
        L = 12.0
        secs = [ellipse_section((0, -L / 2 + t * L, 1.6), r * 1.3, r, 16, y=-L / 2 + t * L)
                for t, r in [(0, 0.05), (0.08, 0.45), (0.25, 0.9), (0.55, 1.1), (0.85, 1.0), (1.0, 0.75)]]
        hull = loft(f"{m.root.name}_Coque", secs)
        add_subsurf(hull, 2)
        m.add(hull, paint, part="coque")
        wing_mat = m.material("ailes", "metal", color=_col(spec, "secondary", (0.35, 0.37, 0.42)))
        wings = []
        for side in (1, -1):
            bm = bmesh.new()
            pts = [(side * 1.0, -1.0, 1.4), (side * 6.5, 3.0, 1.1), (side * 6.8, 4.2, 1.1), (side * 1.0, 4.5, 1.5)]
            bm.faces.new([bm.verts.new(p) for p in pts])
            w = obj_from_bm("aile", bm, smooth=False)
            so = w.modifiers.new("Épaisseur", "SOLIDIFY")
            so.thickness = 0.25
            wings.append(w)
        m.add(join(wings, f"{m.root.name}_Ailes"), wing_mat, part="ailes")
        glow = m.material("réacteurs", "emission", color=_col(spec, "eyes", (0.2, 0.6, 1.0)), emission_strength=15)
        for side in (1, -1):
            e = cylinder(f"{m.root.name}_Réacteur", 0.55, 2.4, (side * 1.6, L / 2 - 0.6, 1.5), axis="Y", segments=20)
            m.add(e, wing_mat, part="réacteurs")
            g = cylinder(f"{m.root.name}_Flamme", 0.42, 0.1, (side * 1.6, L / 2 + 0.62, 1.5), axis="Y", segments=20)
            m.add(g, glow, part="réacteurs")
        canopy = ellipsoid(f"{m.root.name}_Cockpit", (0.7, 1.6, 0.55), (0, -2.5, 2.4), 20, 10)
        m.add(canopy, glass, part="cockpit")
        return hull
    if sub == "boat":
        L = 8.0
        secs = []
        for t, w, d in [(0, 0.02, 0.8), (0.15, 0.6, 0.9), (0.45, 1.0, 1.0), (0.8, 0.95, 0.95), (1.0, 0.85, 0.9)]:
            y = -L / 2 + t * L
            sec = []
            for k in range(13):
                a = math.pi * k / 12
                sec.append((math.cos(a) * 1.4 * w, y, 1.2 - math.sin(a) * 1.1 * d))
            secs.append(sec)
        hull = loft(f"{m.root.name}_Coque", secs, closed=False, cap=False)
        so = hull.modifiers.new("Épaisseur", "SOLIDIFY")
        so.thickness = 0.08
        add_subsurf(hull, 1)
        m.add(hull, paint, part="coque")
        deck = box(f"{m.root.name}_Pont", (2.6, L * 0.85, 0.1), (0, L * 0.05, 1.1))
        m.add(deck, m.material("pont", "wood", color=(0.55, 0.38, 0.2, 1)), part="pont")
        mast = cylinder(f"{m.root.name}_Mât", 0.08, 7, (0, -0.5, 4.6), segments=12)
        m.add(mast, m.material("mât", "wood", color=(0.45, 0.3, 0.15, 1)), part="mât")
        sail = membrane(f"{m.root.name}_Voile", [[(0, -0.45, 1.6), (0, -0.45, 4.5), (0, -0.45, 7.8)],
                                                  [(0.25, 2.6, 1.6), (0.35, 1.6, 4.2), (0.1, 0.0, 7.0)]], thickness=0.02)
        m.add(sail, m.material("voile", "fabric", color=_col(spec, "secondary", (0.95, 0.93, 0.88))), part="voile")
        return hull
    if sub == "plane":
        L = 10.0
        fus = lathe(f"{m.root.name}_Fuselage", [(0, 0), (0.4, 0.4), (0.7, 1.5), (0.75, 4.0), (0.6, 7.5), (0.2, 9.6), (0, 10)], 20)
        fus.rotation_euler.x = math.pi / 2
        fus.location = (0, -L / 2, 2.0)
        bpy.context.view_layer.update()
        fus.data.transform(fus.matrix_basis)
        fus.matrix_basis = Matrix.Identity(4)
        add_subsurf(fus, 1)
        m.add(fus, paint, part="fuselage")
        parts = []
        for side in (1, -1):
            bm = bmesh.new()
            bm.faces.new([bm.verts.new(p) for p in [(side * 0.6, -1.5, 1.8), (side * 5.5, 0.2, 1.9),
                                                     (side * 5.5, 1.0, 1.9), (side * 0.6, 0.8, 1.8)]])
            parts.append(obj_from_bm("aile", bm, smooth=False))
            bm = bmesh.new()
            bm.faces.new([bm.verts.new(p) for p in [(side * 0.2, 3.8, 2.1), (side * 2.0, 4.6, 2.15),
                                                     (side * 2.0, 5.0, 2.15), (side * 0.2, 4.9, 2.1)]])
            parts.append(obj_from_bm("stabilisateur", bm, smooth=False))
        bm = bmesh.new()
        bm.faces.new([bm.verts.new(p) for p in [(0, 3.6, 2.5), (0, 4.5, 4.0), (0, 5.0, 4.0), (0, 4.95, 2.5)]])
        parts.append(obj_from_bm("dérive", bm, smooth=False))
        for p in parts:
            so = p.modifiers.new("Épaisseur", "SOLIDIFY")
            so.thickness = 0.12
        m.add(join(parts, f"{m.root.name}_Voilure"), paint, part="ailes")
        canopy = ellipsoid(f"{m.root.name}_Cockpit", (0.5, 1.0, 0.45), (0, -3.0, 2.6), 16, 8)
        m.add(canopy, glass, part="cockpit")
        return fus
    if sub == "tank":
        hull = box(f"{m.root.name}_Caisse", (3.2, 6.5, 1.2), (0, 0, 1.2), bevel=0.15)
        m.add(hull, paint, part="caisse")
        turret = cylinder(f"{m.root.name}_Tourelle", 1.2, 0.8, (0, 0.3, 2.2), segments=20)
        add_bevel(turret, 0.08, 2)
        m.add(turret, paint, part="tourelle")
        barrel = cylinder(f"{m.root.name}_Canon", 0.14, 4.0, (0, -2.0, 2.25), axis="Y", segments=16)
        m.add(barrel, paint, part="canon")
        for side in (1, -1):
            tr = box(f"{m.root.name}_Chenille", (0.7, 6.8, 1.0), (side * 1.85, 0, 0.5), bevel=0.3, segments=4)
            m.add(tr, tire, part="chenilles")
        return hull
    # moto / vélo
    frame = sweep(f"{m.root.name}_Cadre", [(0, -0.6, 0.35), (0, -0.3, 0.8), (0, 0.4, 0.75), (0, 0.6, 0.35)],
                  [0.05, 0.05, 0.05, 0.05], sides=10)
    m.add(frame, paint, part="cadre")
    for y in (-0.7, 0.7):
        _wheel(f"{m.root.name}_Roue", 0.33, 0.12, (0, y, 0.33), tire, rim, m)
    seat = box(f"{m.root.name}_Selle", (0.25, 0.45, 0.08), (0, 0.2, 0.85), bevel=0.03)
    m.add(seat, tire, part="selle")
    return frame


# ---------------------------------------------------------------------------
# Armes fictives
# ---------------------------------------------------------------------------
def build_weapon(m: Model, spec: dict):
    sub = spec.get("subtype", "sword")
    metal = m.material("lame", spec.get("material") if spec.get("material") in ("metal", "gold", "rust", "crystal", "ice", "lava") else "metal",
                       color=_col(spec, "primary", (0.75, 0.77, 0.8)))
    grip = m.material("poignée", "leather", color=_col(spec, "secondary", (0.25, 0.12, 0.06)))
    gold = m.material("garde", "gold", color=(1.0, 0.75, 0.3, 1.0))
    glow = spec.get("glow") or spec.get("material") == "emission"
    if sub in ("sword", "dagger"):
        L = 1.0 if sub == "sword" else 0.35
        w = 0.05 if sub == "sword" else 0.035
        prof = [(-1, 0), (0, 0.18), (1, 0), (0, -0.18)]  # losange : arête centrale
        blade = sweep(f"{m.root.name}_Lame", [(0, 0, 0.2), (0, 0, 0.2 + L * 0.6), (0, 0, 0.2 + L * 0.92), (0, 0, 0.2 + L)],
                      [w, w * 0.95, w * 0.7, 0.0], profile=prof, tip=True, smooth=False)
        m.add(blade, metal, part="lame")
        guard = box(f"{m.root.name}_Garde", (0.28 if sub == "sword" else 0.12, 0.05, 0.04), (0, 0, 0.19), bevel=0.012)
        m.add(guard, gold, part="garde")
        g = cylinder(f"{m.root.name}_Poignée", 0.02, 0.17, (0, 0, 0.085), segments=12)
        m.add(g, grip, part="poignée")
        pommel = ellipsoid(f"{m.root.name}_Pommeau", (0.032, 0.032, 0.032), (0, 0, -0.01), 12, 8)
        m.add(pommel, gold, part="pommeau")
        if glow:
            gm = m.material("runes", "emission", color=_col(spec, "eyes", (0.3, 0.7, 1.0)), emission_strength=6)
            rune = box(f"{m.root.name}_Runes", (0.008, 0.012, L * 0.5), (0, 0, 0.2 + L * 0.35))
            m.add(rune, gm, part="runes")
        return blade
    if sub in ("axe", "hammer", "spear", "scythe", "staff"):
        handle_len = {"axe": 0.9, "hammer": 0.9, "spear": 2.0, "scythe": 1.8, "staff": 1.7}[sub]
        wood = m.material("manche", "wood", color=_col(spec, "secondary", (0.4, 0.25, 0.12)))
        handle = cylinder(f"{m.root.name}_Manche", 0.022 if sub != "staff" else 0.03, handle_len, (0, 0, handle_len / 2), segments=12)
        m.add(handle, wood if sub != "spear" else wood, part="manche")
        top = handle_len
        if sub == "axe":
            bm = bmesh.new()
            pts = [(0.0, 0, top - 0.25), (0.22, 0, top - 0.32), (0.26, 0, top - 0.1), (0.24, 0, top + 0.02), (0.0, 0, top - 0.06)]
            bm.faces.new([bm.verts.new(p) for p in pts])
            head = obj_from_bm(f"{m.root.name}_Fer", bm, smooth=False)
            so = head.modifiers.new("Épaisseur", "SOLIDIFY")
            so.thickness = 0.03
            so.offset = 0
            add_bevel(head, 0.004, 1)
        elif sub == "hammer":
            head = box(f"{m.root.name}_Tête", (0.28, 0.13, 0.13), (0, 0, top - 0.05), bevel=0.012)
        elif sub == "spear":
            head = sweep(f"{m.root.name}_Pointe", [(0, 0, top), (0, 0, top + 0.12), (0, 0, top + 0.3)], [0.04, 0.035, 0.0],
                         profile=[(-1, 0), (0, 0.2), (1, 0), (0, -0.2)], tip=True, smooth=False)
        elif sub == "scythe":
            pts = curve_points((0, 0, top - 0.05), (1, 0, 0.1), 0.85, 10, bend_axis=(0, 1, 0), bend=math.radians(-70))
            head = sweep(f"{m.root.name}_Lame", pts, taper(len(pts), 0.07, 0.0), profile=[(-1, 0), (0, 0.08), (1, 0), (0, -0.08)],
                         tip=True, smooth=False)
        else:
            orb_mat = m.material("orbe", "crystal", color=_col(spec, "eyes", (0.4, 0.2, 0.9)), emission_strength=3)
            head = ellipsoid(f"{m.root.name}_Orbe", (0.07, 0.07, 0.07), (0, 0, top + 0.1), 16, 8)
            m.add(head, orb_mat, part="orbe")
            claws = [sweep("griffe", curve_points((math.cos(a) * 0.04, math.sin(a) * 0.04, top - 0.02),
                                                  (math.cos(a) * 0.4, math.sin(a) * 0.4, 1), 0.2, 4, bend_axis=(-math.sin(a), math.cos(a), 0),
                                                  bend=math.radians(-50)), [0.012, 0.01, 0.008, 0.005, 0.0], sides=6, tip=True)
                     for a in (0, 2.1, 4.2)]
            m.add(join(claws, f"{m.root.name}_Couronne"), gold, part="couronne")
            return handle
        m.add(head, metal, part="tête")
        return head
    if sub == "bow":
        pts = [(0, -0.12 * math.sin(math.pi * t) * 0 + 0.0, 0) for t in range(1)]
        arc = [Vector((0, 0.18 * math.sin(math.pi * k / 16) ** 0.8, -0.7 + 1.4 * k / 16)) for k in range(17)]
        bow = sweep(f"{m.root.name}_Arc", arc, [0.012 + 0.012 * math.sin(math.pi * k / 16) for k in range(17)], sides=8)
        m.add(bow, m.material("bois", "wood", color=_col(spec, "primary", (0.45, 0.28, 0.12))), part="arc")
        string = sweep(f"{m.root.name}_Corde", [arc[0], arc[-1]], [0.002, 0.002], sides=4)
        m.add(string, m.material("corde", "fabric", color=(0.9, 0.88, 0.8, 1)), part="corde")
        return bow
    if sub == "shield":
        sh = lathe(f"{m.root.name}_Bouclier", [(0, 0.06), (0.25, 0.05), (0.45, 0.02), (0.5, 0.0), (0.5, -0.02), (0, -0.02)], 32)
        sh.rotation_euler.x = math.pi / 2
        bpy.context.view_layer.update()
        sh.data.transform(sh.matrix_basis)
        sh.matrix_basis = Matrix.Identity(4)
        m.add(sh, m.material("bois", "wood", color=_col(spec, "primary", (0.45, 0.28, 0.12))), part="bouclier")
        boss = ellipsoid(f"{m.root.name}_Umbo", (0.1, 0.06, 0.1), (0, -0.07, 0), 16, 8)
        rim_t = bpy.data.meshes.new("x")
        m.add(boss, metal, part="umbo")
        rim_o = cylinder(f"{m.root.name}_Cerclage", 0.51, 0.06, (0, 0, 0), axis="Y", segments=32)
        so = rim_o.modifiers.new("Épaisseur", "SOLIDIFY")
        m.add(rim_o, metal, part="cerclage")
        bpy.data.meshes.remove(rim_t)
        return sh
    # arme à feu / blaster fictif
    body = box(f"{m.root.name}_Corps", (0.06, 0.32, 0.1), (0, 0, 0.18), bevel=0.01)
    m.add(body, metal, part="corps")
    barrel = cylinder(f"{m.root.name}_Canon", 0.018, 0.25, (0, -0.27, 0.2), axis="Y", segments=16)
    m.add(barrel, metal, part="canon")
    g = box(f"{m.root.name}_Crosse", (0.05, 0.07, 0.15), (0, 0.08, 0.07), bevel=0.01)
    g.rotation_euler.x = -0.25
    m.add(g, grip, part="crosse")
    gm = m.material("énergie", "emission", color=_col(spec, "eyes", (0.2, 0.9, 1.0)), emission_strength=8)
    cell = cylinder(f"{m.root.name}_Cellule", 0.022, 0.12, (0, 0.02, 0.25), axis="Y", segments=12)
    m.add(cell, gm, part="cellule")
    return body


# ---------------------------------------------------------------------------
# Objets
# ---------------------------------------------------------------------------
def build_prop(m: Model, spec: dict):
    sub = spec.get("subtype", "barrel")
    rng = random.Random(int(spec.get("seed", 1)))
    wood = m.material("bois", "wood", color=_col(spec, "primary", (0.5, 0.32, 0.16)))
    metal = m.material("métal", "metal", color=(0.35, 0.35, 0.37, 1.0), roughness=0.45)
    if sub == "barrel":
        prof = [(0, 0), (0.28, 0), (0.33, 0.25), (0.35, 0.45), (0.33, 0.65), (0.28, 0.9), (0, 0.9)]
        b = lathe(f"{m.root.name}_Tonneau", prof, 24)
        m.add(b, wood, part="douelles")
        hoops = [cylinder("cercle", r + 0.008, 0.04, (0, 0, z), segments=24) for r, z in ((0.3, 0.1), (0.345, 0.32), (0.345, 0.58), (0.3, 0.8))]
        m.add(join(hoops, f"{m.root.name}_Cercles"), metal, part="cercles")
        return b
    if sub in ("crate", "chest"):
        c = box(f"{m.root.name}_Caisse", (0.8, 0.6 if sub == "chest" else 0.8, 0.5 if sub == "chest" else 0.8),
                (0, 0, 0.25 if sub == "chest" else 0.4), bevel=0.015)
        m.add(c, wood, part="caisse")
        if sub == "chest":
            lid = cylinder(f"{m.root.name}_Couvercle", 0.3, 0.8, (0, 0, 0.5), axis="X", segments=24)
            bm = bmesh.new()
            bm.from_mesh(lid.data)
            bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(0, 0, 0.5),
                                   plane_no=(0, 0, 1), clear_inner=True)
            bmesh.ops.holes_fill(bm, edges=bm.edges, sides=64)
            bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
            bm.to_mesh(lid.data)
            bm.free()
            m.add(lid, wood, part="couvercle")
            straps = [box("sangle", (0.06, 0.62, 0.82), (x, 0, 0.42)) for x in (-0.28, 0.28)]
            m.add(join(straps, f"{m.root.name}_Ferrures"), m.material("or", "gold", color=(1, 0.75, 0.3, 1)), part="ferrures")
        else:
            planks = [box("planche", (0.82, 0.04, 0.1), (0, -0.41, z)) for z in (0.15, 0.4, 0.65)]
            m.add(join(planks, f"{m.root.name}_Planches"), wood, part="planches")
        return c
    if sub == "table":
        top = box(f"{m.root.name}_Plateau", (1.6, 0.9, 0.06), (0, 0, 0.75), bevel=0.01)
        legs = [box("pied", (0.07, 0.07, 0.72), (x, y, 0.36)) for x in (-0.72, 0.72) for y in (-0.37, 0.37)]
        m.add(top, wood, part="plateau")
        m.add(join(legs, f"{m.root.name}_Pieds"), wood, part="pieds")
        return top
    if sub == "chair":
        seat = box(f"{m.root.name}_Assise", (0.45, 0.45, 0.05), (0, 0, 0.45), bevel=0.01)
        back = box(f"{m.root.name}_Dossier", (0.45, 0.05, 0.5), (0, 0.2, 0.72), bevel=0.01)
        legs = [box("pied", (0.04, 0.04, 0.45), (x, y, 0.225)) for x in (-0.2, 0.2) for y in (-0.2, 0.2)]
        m.add(seat, wood, part="assise")
        m.add(back, wood, part="dossier")
        m.add(join(legs, f"{m.root.name}_Pieds"), wood, part="pieds")
        return seat
    if sub == "lamp":
        base = lathe(f"{m.root.name}_Socle", [(0, 0), (0.12, 0), (0.12, 0.03), (0.03, 0.06), (0.025, 0.45), (0, 0.45)], 20)
        m.add(base, metal, part="socle")
        glass = m.material("verre", "glass", color=(1, 0.9, 0.7, 1))
        cage = lathe(f"{m.root.name}_Verre", [(0, 0.45), (0.09, 0.47), (0.11, 0.6), (0.08, 0.72), (0, 0.74)], 20)
        m.add(cage, glass, part="verre")
        flame = ellipsoid(f"{m.root.name}_Flamme", (0.025, 0.025, 0.05), (0, 0, 0.58), 10, 6)
        m.add(flame, m.material("flamme", "emission", color=_col(spec, "eyes", (1, 0.6, 0.2)), emission_strength=20), part="flamme")
        return base
    if sub == "bottle":
        kw = spec.get("keyword") or ""
        col = _col(spec, "primary", (0.2, 0.8, 0.3))
        prof = [(0, 0), (0.12, 0), (0.15, 0.08), (0.15, 0.18), (0.05, 0.3), (0.04, 0.38), (0.05, 0.4), (0.0, 0.4)]
        if kw in ("vase", "amphore", "jarre", "jar", "urne", "urn", "pot"):
            prof = [(0, 0), (0.12, 0), (0.22, 0.2), (0.24, 0.35), (0.14, 0.55), (0.1, 0.62), (0.14, 0.68), (0, 0.68)]
        bottle = lathe(f"{m.root.name}_Flacon", prof, 24)
        is_vase = kw in ("vase", "amphore", "jarre", "jar", "urne", "urn", "pot")
        mat = m.material("céramique", "plain", color=col, roughness=0.3) if is_vase else \
            m.material("verre", "glass", color=col)
        m.add(bottle, mat, part="flacon")
        if not is_vase:
            liquid = lathe(f"{m.root.name}_Liquide", [(0, 0.01), (0.13, 0.01), (0.14, 0.08), (0.14, 0.17), (0.0, 0.17)], 24)
            m.add(liquid, m.material("liquide", "emission" if spec.get("glow") else "glass", color=col,
                                     emission_strength=3 if spec.get("glow") else 0), part="liquide")
            cork = cylinder(f"{m.root.name}_Bouchon", 0.04, 0.06, (0, 0, 0.42), segments=12)
            m.add(cork, wood, part="bouchon")
        return bottle
    if sub == "rock":
        bm = bmesh.new()
        bmesh.ops.create_icosphere(bm, subdivisions=3, radius=0.6)
        for v in bm.verts:
            v.co *= 1 + rng.uniform(-0.12, 0.12)
            v.co.z *= 0.7
        r = obj_from_bm(f"{m.root.name}_Rocher", bm)
        displace_detail(r, strength=0.15, scale=0.4, kind="CLOUDS", name="Roche")
        add_subsurf(r, 1)
        m.add(r, m.material("roche", "stone", color=_col(spec, "primary", (0.45, 0.43, 0.4))), part="rocher")
        return r
    if sub in ("tree", "bush"):
        return _tree(m, spec, rng, small=sub == "bush")
    if sub == "mushroom":
        stem = lathe(f"{m.root.name}_Pied", [(0, 0), (0.08, 0), (0.06, 0.25), (0.07, 0.4), (0, 0.4)], 16)
        cap = lathe(f"{m.root.name}_Chapeau", [(0, 0.35), (0.3, 0.35), (0.28, 0.45), (0.15, 0.55), (0, 0.58)], 24)
        m.add(stem, m.material("pied", "plain", color=(0.92, 0.88, 0.8, 1)), part="pied")
        m.add(cap, m.material("chapeau", "plain", color=_col(spec, "primary", (0.8, 0.1, 0.05))), part="chapeau")
        dots = [ellipsoid("point", (0.03, 0.03, 0.015), (0.18 * math.cos(a), 0.18 * math.sin(a), 0.5), 8, 4) for a in range(0, 6)]
        m.add(join(dots, f"{m.root.name}_Points"), m.material("points", "plain", color=(0.95, 0.95, 0.9, 1)), part="points")
        return cap
    if sub == "crystal":
        objs = []
        for k in range(5):
            a = k * 1.3
            h = rng.uniform(0.4, 0.9)
            d = Vector((math.cos(a) * 0.35, math.sin(a) * 0.35, 1)).normalized() if k else Vector((0, 0, 1))
            pts = [Vector((0, 0, 0)), d * h * 0.75, d * h]
            objs.append(sweep("cristal", pts, [0.1, 0.1, 0.0], sides=6, tip=True, smooth=False))
        c = join(objs, f"{m.root.name}_Cristaux")
        m.add(c, m.material("cristal", "crystal", color=_col(spec, "primary", (0.5, 0.3, 0.95)), emission_strength=1.5), part="cristaux")
        return c
    if sub == "well":
        w = lathe(f"{m.root.name}_Margelle", [(0.55, 0), (0.75, 0), (0.75, 0.8), (0.55, 0.8)], 24, smooth=False)
        m.add(w, m.material("pierre", "stone", color=(0.55, 0.52, 0.48, 1)), part="margelle")
        posts = [box("poteau", (0.1, 0.1, 1.6), (x, 0, 1.6)) for x in (-0.7, 0.7)]
        m.add(join(posts, f"{m.root.name}_Poteaux"), wood, part="poteaux")
        return w
    if sub == "fence":
        posts = [box("poteau", (0.1, 0.1, 1.1), (x * 1.0, 0, 0.55)) for x in range(-2, 3)]
        rails = [box("traverse", (4.2, 0.05, 0.1), (0, 0, z)) for z in (0.4, 0.85)]
        f = join(posts + rails, f"{m.root.name}_Clôture")
        m.add(f, wood, part="clôture")
        return f
    # couronne / anneau
    ring = cylinder(f"{m.root.name}_Couronne", 0.12, 0.06, (0, 0, 0.03), segments=32, cap=False)
    so = ring.modifiers.new("Épaisseur", "SOLIDIFY")
    so.thickness = 0.012
    spikes = [sweep("pointe", [(0.12 * math.cos(a), 0.12 * math.sin(a), 0.06), (0.12 * math.cos(a), 0.12 * math.sin(a), 0.14)],
                    [0.02, 0.0], sides=6, tip=True) for a in [k * math.pi / 4 for k in range(8)]]
    c = join([ring] + spikes, f"{m.root.name}_Couronne")
    m.add(c, m.material("or", "gold", color=(1, 0.75, 0.3, 1)), part="couronne")
    return c


def _tree(m, spec, rng, small=False):
    bark = m.material("écorce", "bark", color=(0.32, 0.22, 0.14, 1))
    leaves = m.material("feuillage", "organic", color=_col(spec, "primary", (0.15, 0.42, 0.1)),
                        color2=(0.08, 0.25, 0.05, 1), bump=0.8)
    H = 1.2 if small else 6.0
    kw = spec.get("keyword") or ""
    if kw in ("sapin", "pine"):
        trunk = cylinder(f"{m.root.name}_Tronc", H * 0.04, H, (0, 0, H / 2), segments=10, radius_top=H * 0.01)
        m.add(trunk, bark, part="tronc")
        cones = [cylinder("étage", H * (0.32 - 0.06 * k), H * 0.35, (0, 0, H * (0.3 + 0.18 * k)), segments=14, radius_top=0.01)
                 for k in range(4)]
        c = join(cones, f"{m.root.name}_Feuillage")
        displace_detail(c, strength=H * 0.02, scale=H * 0.05, kind="CLOUDS")
        m.add(c, leaves, part="feuillage")
        return trunk
    branches = []
    blobs = []

    def grow(p, d, length, r, depth):
        pts = curve_points(p, d, length, 4, bend_axis=Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), 0)),
                           bend=rng.uniform(-0.4, 0.4))
        branches.append(sweep("branche", pts, taper(len(pts), r, r * 0.6), sides=8))
        if depth == 0:
            blobs.append((pts[-1], length * 0.9))
            return
        for k in range(2 if depth < 3 else 3):
            nd = (Vector(d) + Vector((rng.uniform(-0.9, 0.9), rng.uniform(-0.9, 0.9), rng.uniform(0.1, 0.6)))).normalized()
            grow(pts[-1], nd, length * 0.68, r * 0.6, depth - 1)

    grow(Vector((0, 0, 0)), Vector((0, 0, 1)), H * (0.25 if small else 0.38), H * (0.03 if small else 0.06), 2 if small else 3)
    trunk = join(branches, f"{m.root.name}_Tronc")
    m.add(trunk, bark, part="tronc")
    foliage = []
    for c, s in blobs:
        bm = bmesh.new()
        bmesh.ops.create_icosphere(bm, subdivisions=2, radius=s * 0.8)
        for v in bm.verts:
            v.co = v.co * (1 + rng.uniform(-0.15, 0.15)) + c
        foliage.append(obj_from_bm("feuillage", bm))
    f = join(foliage, f"{m.root.name}_Feuillage")
    displace_detail(f, strength=H * 0.03, scale=H * 0.06, kind="CLOUDS")
    m.add(f, leaves, part="feuillage")
    return trunk


# ---------------------------------------------------------------------------
# Décors
# ---------------------------------------------------------------------------
def build_scenery(m: Model, spec: dict):
    sub = spec.get("subtype", "terrain")
    rng = random.Random(int(spec.get("seed", 1)))
    size = float(spec.get("terrain_size", 40.0))
    res = int(spec.get("_terrain_res", 96))
    if sub in ("terrain", "forest", "village", "ruins"):
        bm = bmesh.new()
        bmesh.ops.create_grid(bm, x_segments=res, y_segments=res, size=size / 2)
        terrain = obj_from_bm(f"{m.root.name}_Terrain", bm)
        tex = bpy.data.textures.new(f"{m.root.name}_relief", "CLOUDS")
        tex.noise_scale = size * 0.25
        tex.noise_depth = 6
        d = terrain.modifiers.new("Relief", "DISPLACE")
        d.texture = tex
        d.texture_coords = "LOCAL"
        mountain = spec.get("keyword") in ("montagne", "mountain", "canyon")
        d.strength = size * (0.35 if mountain else 0.08)
        d.mid_level = 0.3 if not mountain else 0.2
        kw = spec.get("keyword") or ""
        tmat = m.material("sol", "ground" if kw not in ("desert",) else "stone",
                          color=_col(spec, "primary", (0.8, 0.7, 0.45) if kw == "desert" else (0.25, 0.4, 0.12)),
                          color2=(0.35, 0.28, 0.18, 1))
        m.add(terrain, tmat, part="terrain")
        dg = bpy.context.evaluated_depsgraph_get()
        ev = terrain.evaluated_get(dg).to_mesh()
        heights = [(v.co.x, v.co.y, v.co.z) for v in ev.vertices]
        terrain.evaluated_get(dg).to_mesh_clear()

        def ground(x, y):
            best = min(heights[:: max(1, len(heights) // 3000)], key=lambda h: (h[0] - x) ** 2 + (h[1] - y) ** 2)
            return best[2]
        if sub == "forest":
            n = int(spec.get("count", 18))
            for i in range(n):
                x, y = rng.uniform(-size * 0.4, size * 0.4), rng.uniform(-size * 0.4, size * 0.4)
                sub_m = Model(f"Arbre", {"seed": rng.randint(0, 99999), "keyword": rng.choice(["chene", "sapin"]),
                                         "colors": {}}, m.collection)
                _tree(sub_m, sub_m.spec, rng)
                sub_m.root.location = (x, y, ground(x, y) - 0.1)
                s = rng.uniform(0.7, 1.3)
                sub_m.root.scale = (s, s, s)
                sub_m.root.parent = m.root
                for o in sub_m.parts:
                    m.parts.append(o)
                m.parts.append(sub_m.root)
        elif sub == "village":
            for i in range(int(spec.get("count", 6))):
                a = i / 6 * 2 * math.pi
                x, y = math.cos(a) * size * 0.25, math.sin(a) * size * 0.25
                sub_m = Model("Maison", {"subtype": "house", "seed": i, "colors": {}, "floors": rng.choice([1, 2])}, m.collection)
                build_building(sub_m, sub_m.spec)
                sub_m.root.location = (x, y, ground(x, y) - 0.2)
                sub_m.root.rotation_euler.z = a + math.pi / 2
                sub_m.root.scale = (0.6, 0.6, 0.6)
                sub_m.root.parent = m.root
                m.parts.extend(sub_m.parts + [sub_m.root])
        elif sub == "ruins":
            stone = m.material("pierre", "stone", color=(0.6, 0.57, 0.5, 1))
            pieces = []
            for i in range(10):
                x, y = rng.uniform(-8, 8), rng.uniform(-8, 8)
                h = rng.uniform(1, 5)
                col = lathe("colonne", [(0, 0), (0.5, 0), (0.4, 0.2), (0.38, h), (0, h)], 14, smooth=False)
                col.location = (x, y, ground(x, y) - 0.2)
                col.rotation_euler = (rng.uniform(-0.15, 0.15), rng.uniform(-0.15, 0.15), 0)
                bpy.context.view_layer.update()
                col.data.transform(col.matrix_basis)
                col.matrix_basis = Matrix.Identity(4)
                pieces.append(col)
            for i in range(4):
                x, y = rng.uniform(-8, 8), rng.uniform(-8, 8)
                pieces.append(box("mur", (rng.uniform(2, 5), 0.6, rng.uniform(1, 3)), (x, y, ground(x, y) + 0.5)))
            r = join(pieces, f"{m.root.name}_Ruines")
            displace_detail(r, strength=0.06, scale=0.3, kind="CLOUDS", name="Érosion")
            m.add(r, stone, part="ruines")
        return terrain
    # donjon / grotte : salle fermée
    stone = m.material("pierre", "stone", color=_col(spec, "primary", (0.4, 0.38, 0.36)))
    W, D, H = 12.0, 16.0, 5.0
    floor = box(f"{m.root.name}_Sol", (W, D, 0.3), (0, 0, -0.15))
    walls = [box("mur", (W, 0.6, H), (0, D / 2, H / 2)), box("mur", (0.6, D, H), (W / 2, 0, H / 2)),
             box("mur", (0.6, D, H), (-W / 2, 0, H / 2)), box("mur", (W * 0.35, 0.6, H), (-W * 0.33, -D / 2, H / 2)),
             box("mur", (W * 0.35, 0.6, H), (W * 0.33, -D / 2, H / 2))]
    room = join([floor] + walls, f"{m.root.name}_Salle")
    displace_detail(room, strength=0.05, scale=0.3, kind="CLOUDS", name="Usure")
    m.add(room, stone, part="salle")
    pillars = [cylinder("pilier", 0.45, H, (x, y, H / 2), segments=12) for x in (-3, 3) for y in (-4, 0, 4)]
    m.add(join(pillars, f"{m.root.name}_Piliers"), stone, part="piliers")
    fire = m.material("feu", "emission", color=(1, 0.5, 0.15, 1), emission_strength=25)
    torches = []
    for x in (-W / 2 + 0.4, W / 2 - 0.4):
        for y in (-4, 4):
            t = cylinder("torche", 0.05, 0.5, (x, y, 2.4), segments=8)
            torches.append(t)
            flame = ellipsoid(f"{m.root.name}_Flamme", (0.08, 0.08, 0.15), (x, y, 2.75), 8, 6)
            m.add(flame, fire, part="flammes")
            light = bpy.data.objects.new(f"{m.root.name}_Lueur", bpy.data.lights.new("Lueur", "POINT"))
            light.data.energy = 120
            light.data.color = (1, 0.6, 0.3)
            light.location = (x * 0.9, y, 2.9)
            m.collection.objects.link(light)
            light.parent = m.root
            m.parts.append(light)
    m.add(join(torches, f"{m.root.name}_Torches"), m.material("bois", "wood", color=(0.3, 0.2, 0.1, 1)), part="torches")
    return room
