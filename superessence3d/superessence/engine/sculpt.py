"""Sculpture, peinture de sommets et peinture de poids.

Le viewport envoie des traits de pinceau (points d'impact en coordonnées monde,
normale, rayon, force). Le moteur déplace réellement les sommets du maillage
de base Blender (numpy + KD-tree), avec atténuation douce.
"""
from __future__ import annotations

import bpy
import numpy as np
from mathutils import Vector, kdtree

from .colors import to_linear
from .core import CommandError, Params, command
from .util import ensure_object_mode, get_mesh_object

BRUSHES = ["draw", "inflate", "smooth", "flatten", "pinch", "grab", "crease", "clay", "scrape", "noise"]


def _stroke_points(p: Params, obj):
    pts = p.list("points")
    if not pts:
        raise CommandError("Le trait ne contient aucun point.")
    if len(pts) > 5000:
        raise CommandError("Trait trop long (max 5000 points).")
    inv = obj.matrix_world.inverted()
    inv3 = inv.to_3x3()
    out = []
    for pt in pts:
        try:
            if isinstance(pt, dict):
                pos = Vector([float(x) for x in pt["position"]])
                nrm = Vector([float(x) for x in pt.get("normal", (0, 0, 1))])
                pressure = float(pt.get("pressure", 1.0))
            else:
                pos = Vector([float(x) for x in pt[:3]])
                nrm = Vector([float(x) for x in pt[3:6]]) if len(pt) >= 6 else Vector((0, 0, 1))
                pressure = 1.0
        except (TypeError, ValueError, KeyError, IndexError):
            raise CommandError("Point de trait mal formé.") from None
        lp = inv @ pos
        ln = (inv3 @ nrm)
        if ln.length < 1e-9:
            ln = Vector((0, 0, 1))
        out.append((lp, ln.normalized(), max(0.0, min(2.0, pressure))))
    return out


def _local_radius(obj, radius_world: float) -> float:
    s = obj.matrix_world.to_scale()
    return radius_world / max(1e-6, (abs(s.x) + abs(s.y) + abs(s.z)) / 3)


def _falloff(d, r):
    x = np.clip(1.0 - d / r, 0.0, 1.0)
    return x * x * (3 - 2 * x)  # smoothstep


def _neighbors(me):
    """Liste d'adjacence (pour le lissage)."""
    ne = len(me.edges)
    ev = np.empty(ne * 2, dtype=np.int64)
    me.edges.foreach_get("vertices", ev)
    return ev.reshape(-1, 2)


@command("sculpt_object", label="Sculpter")
def cmd_sculpt(engine, p: Params):
    """Applique un trait de sculpture : brush (draw, inflate, smooth, flatten, pinch, grab, crease, clay, scrape,
    noise), points [{position, normal}] en coordonnées monde, radius (m), strength, invert, symmetry_x.

    Pour un sculpt sans viewport (Claude), `points` peut contenir quelques positions ; `iterations` répète le trait."""
    obj = get_mesh_object(p.str("name"))
    ensure_object_mode()
    brush = p.str("brush", "draw", choices=BRUSHES)
    radius = _local_radius(obj, p.float("radius", 0.2, lo=0.0005, hi=1000))
    strength = p.float("strength", 0.5, lo=0, hi=10)
    if p.bool("invert", False):
        strength = -strength
    sym = p.bool("symmetry_x", False)
    iterations = p.int("iterations", 1, lo=1, hi=50)
    pts = _stroke_points(p, obj)
    grab_delta = None
    if brush == "grab":
        if not p.has("grab_offset"):
            raise CommandError("Le pinceau grab nécessite « grab_offset » (déplacement monde [x,y,z]).")
        grab_delta = obj.matrix_world.inverted().to_3x3() @ Vector(p.vec("grab_offset"))
    me = obj.data
    nv = len(me.vertices)
    if nv == 0:
        raise CommandError("Maillage vide.")
    co = np.empty(nv * 3, dtype=np.float64)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    me.update()
    nrm = np.empty(nv * 3, dtype=np.float64)
    me.vertices.foreach_get("normal", nrm)
    nrm = nrm.reshape(-1, 3)
    edges = _neighbors(me) if brush in ("smooth",) else None
    if sym:
        pts = pts + [(Vector((-a.x, a.y, a.z)), Vector((-n.x, n.y, n.z)), pr) for a, n, pr in pts]
    tree = kdtree.KDTree(nv)
    for i in range(nv):
        tree.insert(co[i], i)
    tree.balance()
    touched = set()
    unit = radius * 0.15
    for _ in range(iterations):
        for center, normal, pressure in pts:
            hits = tree.find_range(center, radius)
            if not hits:
                continue
            idx = np.fromiter((h[1] for h in hits), dtype=np.int64)
            dist = np.fromiter((h[2] for h in hits), dtype=np.float64)
            w = _falloff(dist, radius) * pressure
            touched.update(idx.tolist())
            c = np.array(center)
            n = np.array(normal)
            if brush in ("draw", "clay"):
                k = 1.0 if brush == "draw" else 0.6
                co[idx] += np.outer(w * strength * unit * k, n)
                if brush == "clay":  # aplatit légèrement le dessus
                    plane_d = (co[idx] - c) @ n
                    co[idx] -= np.outer(w * 0.3 * np.clip(plane_d - unit, 0, None), n)
            elif brush == "inflate":
                co[idx] += nrm[idx] * (w * strength * unit)[:, None]
            elif brush == "crease":
                co[idx] -= np.outer(w * strength * unit * 0.8, n)
                to_c = c - co[idx]
                co[idx] += to_c * (w * 0.15 * abs(strength))[:, None]
            elif brush == "pinch":
                to_c = c - co[idx]
                co[idx] += to_c * (w * 0.2 * strength)[:, None]
            elif brush in ("flatten", "scrape"):
                avg = np.average(co[idx], axis=0, weights=w + 1e-9)
                d = (co[idx] - avg) @ n
                if brush == "scrape":
                    d = np.clip(d, 0, None)
                co[idx] -= np.outer(d * w * min(1.0, abs(strength)), n)
            elif brush == "noise":
                rng = np.random.default_rng(len(touched))
                co[idx] += nrm[idx] * (rng.normal(0, 1, len(idx)) * w * strength * unit * 0.5)[:, None]
            elif brush == "grab":
                co[idx] += np.outer(w, np.array(grab_delta))
            elif brush == "smooth":
                sel = np.zeros(nv, dtype=bool)
                sel[idx] = True
                acc = np.zeros_like(co)
                cnt = np.zeros(nv)
                a, b = edges[:, 0], edges[:, 1]
                np.add.at(acc, a, co[b])
                np.add.at(acc, b, co[a])
                np.add.at(cnt, a, 1)
                np.add.at(cnt, b, 1)
                avg = acc[idx] / np.maximum(cnt[idx], 1)[:, None]
                f = np.clip(w * min(1.0, abs(strength)), 0, 1)[:, None]
                co[idx] = co[idx] * (1 - f) + avg * f
        if brush == "grab":
            break
    me.vertices.foreach_set("co", co.astype(np.float32).ravel())
    me.update()
    return {"name": obj.name, "brush": brush, "affected_vertices": len(touched), "_label": f"Sculpter ({brush})"}


@command("remesh_for_sculpt", label="Densifier pour sculpter")
def cmd_remesh_for_sculpt(engine, p: Params):
    """Remaillage voxel (densité uniforme) pour sculpter des détails fins. voxel_size en mètres."""
    obj = get_mesh_object(p.str("name"))
    from .util import apply_modifiers
    m = obj.modifiers.new("Remaillage", "REMESH")
    m.mode = "VOXEL"
    m.voxel_size = p.float("voxel_size", 0.02, lo=0.001, hi=10)
    m.use_smooth_shade = True
    est = (obj.dimensions.x * obj.dimensions.y + obj.dimensions.y * obj.dimensions.z + obj.dimensions.x * obj.dimensions.z) * 2 / (m.voxel_size ** 2)
    if est > 3_000_000:
        obj.modifiers.remove(m)
        raise CommandError(f"Taille de voxel trop petite : ~{int(est)} faces estimées. Augmentez voxel_size.")
    apply_modifiers(obj, only=[m.name])
    return {"name": obj.name, "vertices": len(obj.data.vertices), "faces": len(obj.data.polygons)}


@command("vertex_paint", label="Peinture de sommets")
def cmd_vertex_paint(engine, p: Params):
    """Peint les couleurs de sommets : points (monde), radius, color, strength. fill=true remplit tout l'objet."""
    obj = get_mesh_object(p.str("name"))
    me = obj.data
    layer_name = p.str("layer", "Couleur")
    ca = me.color_attributes.get(layer_name)
    if ca is None:
        ca = me.color_attributes.new(layer_name, "FLOAT_COLOR", "POINT")
        arr = np.ones(len(me.vertices) * 4, dtype=np.float32)
        ca.data.foreach_set("color", arr)
    me.color_attributes.active_color = ca
    try:
        me.attributes.default_color_name = ca.name
    except AttributeError:
        pass
    color = np.array(to_linear(p.color("color", (1, 0, 0, 1))), dtype=np.float32)
    strength = p.float("strength", 1.0, lo=0, hi=1)
    n = len(ca.data)
    cols = np.empty(n * 4, dtype=np.float32)
    ca.data.foreach_get("color", cols)
    cols = cols.reshape(-1, 4)
    if p.bool("fill", False):
        cols[:] = cols * (1 - strength) + color * strength
        touched = n
    else:
        if ca.domain != "POINT":
            raise CommandError("La peinture par pinceau nécessite un calque de couleurs par sommet (POINT).")
        radius = _local_radius(obj, p.float("radius", 0.1, lo=0.0005, hi=1000))
        pts = _stroke_points(p, obj)
        co = np.empty(len(me.vertices) * 3, dtype=np.float64)
        me.vertices.foreach_get("co", co)
        co = co.reshape(-1, 3)
        tree = kdtree.KDTree(len(co))
        for i, c in enumerate(co):
            tree.insert(c, i)
        tree.balance()
        touched_set = set()
        for center, _n, pr in pts:
            for (_co, i, d) in tree.find_range(center, radius):
                f = float(_falloff(np.array([d]), radius)[0]) * strength * pr
                cols[i] = cols[i] * (1 - f) + color * f
                touched_set.add(i)
        touched = len(touched_set)
    ca.data.foreach_set("color", cols.ravel())
    me.update()
    _ensure_vertex_color_in_material(obj, ca.name)
    return {"name": obj.name, "layer": ca.name, "affected": touched, "_label": "Peinture de sommets"}


def _ensure_vertex_color_in_material(obj, layer):
    """Relie le calque de couleurs au matériau pour que le rendu Blender l'affiche aussi."""
    me = obj.data
    if not me.materials or me.materials[0] is None:
        mat = bpy.data.materials.new(f"{obj.name}_couleurs")
        if mat.node_tree is None:
            mat.use_nodes = True
        me.materials.append(mat)
    mat = me.materials[0]
    nt = mat.node_tree
    if nt is None:
        return
    if any(n.type == "VERTEX_COLOR" and n.layer_name == layer for n in nt.nodes):
        return
    bsdf = next((n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"), None)
    if bsdf is None:
        return
    vc = nt.nodes.new("ShaderNodeVertexColor")
    vc.layer_name = layer
    vc.location = (-400, 300)
    bc = bsdf.inputs["Base Color"]
    if bc.is_linked:
        mix = nt.nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.blend_type = "MULTIPLY"
        mix.inputs[0].default_value = 1.0
        nt.links.new(bc.links[0].from_socket, mix.inputs[6])
        nt.links.new(vc.outputs["Color"], mix.inputs[7])
        nt.links.new(mix.outputs[2], bc)
    else:
        nt.links.new(vc.outputs["Color"], bc)


@command("weight_paint", label="Peinture de poids")
def cmd_weight_paint(engine, p: Params):
    """Peint l'influence d'un os (groupe de sommets) : group, points, radius, weight (0..1), mode (replace, add,
    subtract, smooth)."""
    obj = get_mesh_object(p.str("name"))
    gname = p.str("group")
    g = obj.vertex_groups.get(gname)
    if g is None:
        g = obj.vertex_groups.new(name=gname)
    target = p.float("weight", 1.0, lo=0, hi=1)
    mode = p.str("mode", "add", choices=["replace", "add", "subtract", "smooth"])
    radius = _local_radius(obj, p.float("radius", 0.1, lo=0.0005, hi=1000))
    strength = p.float("strength", 0.5, lo=0, hi=1)
    pts = _stroke_points(p, obj)
    me = obj.data
    tree = kdtree.KDTree(len(me.vertices))
    for v in me.vertices:
        tree.insert(v.co, v.index)
    tree.balance()
    gi = g.index

    def current(i):
        for ge in me.vertices[i].groups:
            if ge.group == gi:
                return ge.weight
        return 0.0
    touched = {}
    for center, _n, pr in pts:
        for (_co, i, d) in tree.find_range(center, radius):
            f = float(_falloff(np.array([d]), radius)[0]) * strength * pr
            touched[i] = max(touched.get(i, 0.0), f)
    for i, f in touched.items():
        w = current(i)
        if mode == "replace":
            nw = w * (1 - f) + target * f
        elif mode == "add":
            nw = min(1.0, w + target * f)
        elif mode == "subtract":
            nw = max(0.0, w - target * f)
        else:
            neigh = [current(e.vertices[1] if e.vertices[0] == i else e.vertices[0])
                     for e in me.edges if i in e.vertices][:16]
            nw = w * (1 - f) + (sum(neigh) / len(neigh) if neigh else w) * f
        g.add([i], nw, "REPLACE")
    return {"name": obj.name, "group": g.name, "affected": len(touched), "_label": f"Poids {g.name}"}
