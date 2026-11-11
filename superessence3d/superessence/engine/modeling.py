"""Modélisation polygonale (mode édition) et modificateurs Blender.

Les opérations d'édition utilisent bmesh, l'API de Blender pour la topologie :
extrusion, inset, bevel, subdivision, suppression, déplacement, fusion, etc.
Les éléments sont désignés par leurs indices Blender (envoyés par le viewport).
"""
from __future__ import annotations

import math

import bmesh
import bpy
from mathutils import Matrix, Vector

from .core import CommandError, Params, command
from .util import (apply_modifiers, ensure_object_mode, get_mesh_object, get_object, override, rna_properties,
                   rna_set, select_only, target_objects)


def _indices(p: Params, key: str, n: int) -> list[int]:
    raw = p.list(key, [])
    try:
        idx = sorted({int(i) for i in raw})
    except (TypeError, ValueError):
        raise CommandError(f"« {key} » doit être une liste d'indices entiers.") from None
    if idx and (idx[0] < 0 or idx[-1] >= n):
        raise CommandError(f"Indice hors limites dans « {key} » (max {n - 1}).")
    return idx


def _bm(obj):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    bm.edges.ensure_lookup_table()
    bm.faces.ensure_lookup_table()
    return bm


def _finish(obj, bm, extra: dict | None = None) -> dict:
    bm.normal_update()
    bm.to_mesh(obj.data)
    obj.data.update()
    bm.free()
    out = {"name": obj.name, "vertices": len(obj.data.vertices), "faces": len(obj.data.polygons)}
    if extra:
        out.update(extra)
    return out


def _faces_from(p: Params, bm, required=True):
    idx = _indices(p, "faces", len(bm.faces))
    if not idx and p.bool("all", False):
        return list(bm.faces)
    if not idx and required:
        raise CommandError("Aucune face sélectionnée (paramètre « faces », ou all=true).")
    return [bm.faces[i] for i in idx]


def _new_face_indices(bm, faces) -> list[int]:
    bm.faces.index_update()
    return [f.index for f in faces if f.is_valid]


@command("modify_geometry", label="Modifier la géométrie")
def cmd_modify_geometry(engine, p: Params):
    """Opérations d'édition polygonale sur un maillage.

    operation :
      extrude (faces, distance) · inset (faces, thickness, depth) · bevel (edges ou faces, width, segments)
      subdivide (faces ou all, cuts, smooth) · delete (faces / vertices / edges)
      translate (vertices ou faces, offset) · scale (faces/vertices, factor) · push (faces/vertices, distance le long des normales)
      merge (vertices -> centre) · merge_by_distance (distance) · triangulate · fill_holes (sides)
      recalc_normals · flip_normals · smooth (vertices ou tout, factor, iterations)
      solidify (thickness) · mirror (axis x/y/z, merge) · spin (axis, angle, steps) · bridge (edges)
      randomize (amount, seed) · loop_cut (edge, cuts)
    """
    obj = get_mesh_object(p.str("name"))
    ensure_object_mode()
    op = p.str("operation", choices=[
        "extrude", "inset", "bevel", "subdivide", "delete", "translate", "scale", "push", "merge",
        "merge_by_distance", "triangulate", "fill_holes", "recalc_normals", "flip_normals", "smooth",
        "solidify", "mirror", "spin", "bridge", "randomize", "loop_cut"])
    bm = _bm(obj)
    extra = {}

    if op == "extrude":
        faces = _faces_from(p, bm)
        dist = p.float("distance", 0.3, lo=-1000, hi=1000)
        individual = p.bool("individual", False)
        if individual:
            res = bmesh.ops.extrude_discrete_faces(bm, faces=faces)
            new_faces = res["faces"]
            for f in new_faces:
                bmesh.ops.translate(bm, verts=list(f.verts), vec=f.normal * dist)
        else:
            avg_n = sum((f.normal for f in faces), Vector()) / len(faces)
            res = bmesh.ops.extrude_face_region(bm, geom=faces)
            verts = [g for g in res["geom"] if isinstance(g, bmesh.types.BMVert)]
            new_faces = [g for g in res["geom"] if isinstance(g, bmesh.types.BMFace)]
            direction = Vector(p.vec("direction")) if p.has("direction") else avg_n.normalized()
            bmesh.ops.translate(bm, verts=verts, vec=direction * dist)
            bmesh.ops.delete(bm, geom=faces, context="FACES_ONLY")
        extra["selected_faces"] = _new_face_indices(bm, new_faces)

    elif op == "inset":
        faces = _faces_from(p, bm)
        res = bmesh.ops.inset_region(bm, faces=faces, thickness=p.float("thickness", 0.05, lo=0, hi=1000),
                                     depth=p.float("depth", 0.0, lo=-1000, hi=1000),
                                     use_even_offset=True) if not p.bool("individual", False) else \
            bmesh.ops.inset_individual(bm, faces=faces, thickness=p.float("thickness", 0.05, lo=0, hi=1000),
                                       depth=p.float("depth", 0.0, lo=-1000, hi=1000), use_even_offset=True)
        extra["selected_faces"] = _new_face_indices(bm, faces)

    elif op == "bevel":
        if p.has("edges"):
            geom = [bm.edges[i] for i in _indices(p, "edges", len(bm.edges))]
        elif p.has("faces") or p.bool("all", False):
            faces = _faces_from(p, bm)
            geom = list({e for f in faces for e in f.edges})
        else:
            raise CommandError("Indiquez « edges » ou « faces » (ou all=true) pour le bevel.")
        if not geom:
            raise CommandError("Aucune arête à biseauter.")
        bmesh.ops.bevel(bm, geom=geom, offset=p.float("width", 0.05, lo=0, hi=1000), offset_type="OFFSET",
                        segments=p.int("segments", 2, lo=1, hi=64), profile=p.float("profile", 0.5, lo=0, hi=1),
                        affect="EDGES", clamp_overlap=True)

    elif op == "subdivide":
        faces = _faces_from(p, bm, required=False) or list(bm.faces)
        edges = list({e for f in faces for e in f.edges})
        bmesh.ops.subdivide_edges(bm, edges=edges, cuts=p.int("cuts", 1, lo=1, hi=20), use_grid_fill=True,
                                  smooth=p.float("smooth", 0.0, lo=0, hi=1))

    elif op == "delete":
        if p.has("faces"):
            bmesh.ops.delete(bm, geom=[bm.faces[i] for i in _indices(p, "faces", len(bm.faces))], context="FACES")
        elif p.has("edges"):
            bmesh.ops.delete(bm, geom=[bm.edges[i] for i in _indices(p, "edges", len(bm.edges))], context="EDGES")
        elif p.has("vertices"):
            bmesh.ops.delete(bm, geom=[bm.verts[i] for i in _indices(p, "vertices", len(bm.verts))], context="VERTS")
        else:
            raise CommandError("Indiquez faces, edges ou vertices à supprimer.")

    elif op in ("translate", "scale", "push", "smooth", "merge", "randomize"):
        verts = _verts_from(p, bm, allow_all=op in ("smooth", "randomize"))
        if op == "translate":
            bmesh.ops.translate(bm, verts=verts, vec=Vector(p.vec("offset")))
        elif op == "scale":
            f = p.vec("factor", 1.0) if not isinstance(p.raw("factor"), (int, float)) else (p.float("factor"),) * 3
            c = sum((v.co for v in verts), Vector()) / len(verts)
            for v in verts:
                d = v.co - c
                v.co = c + Vector((d.x * f[0], d.y * f[1], d.z * f[2]))
        elif op == "push":
            d = p.float("distance", 0.1, lo=-1000, hi=1000)
            bm.normal_update()
            for v in verts:
                v.co += v.normal * d
        elif op == "smooth":
            for _ in range(p.int("iterations", 1, lo=1, hi=200)):
                bmesh.ops.smooth_vert(bm, verts=verts, factor=p.float("factor", 0.5, lo=0, hi=1),
                                      use_axis_x=True, use_axis_y=True, use_axis_z=True)
        elif op == "merge":
            c = sum((v.co for v in verts), Vector()) / len(verts)
            bmesh.ops.pointmerge(bm, verts=verts, merge_co=c)
        else:
            import random
            rnd = random.Random(p.int("seed", 0))
            a = p.float("amount", 0.02, lo=0, hi=100)
            for v in verts:
                v.co += Vector((rnd.uniform(-a, a), rnd.uniform(-a, a), rnd.uniform(-a, a)))

    elif op == "merge_by_distance":
        before = len(bm.verts)
        bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=p.float("distance", 0.0001, lo=0, hi=10))
        extra["removed_vertices"] = before - len(bm.verts)

    elif op == "triangulate":
        faces = _faces_from(p, bm, required=False) or list(bm.faces)
        bmesh.ops.triangulate(bm, faces=faces)

    elif op == "fill_holes":
        before = len(bm.faces)
        bmesh.ops.holes_fill(bm, edges=bm.edges, sides=p.int("sides", 0, lo=0, hi=10000))
        extra["filled"] = len(bm.faces) - before

    elif op == "recalc_normals":
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)

    elif op == "flip_normals":
        faces = _faces_from(p, bm, required=False) or list(bm.faces)
        bmesh.ops.reverse_faces(bm, faces=faces)

    elif op == "solidify":
        faces = _faces_from(p, bm, required=False) or list(bm.faces)
        bmesh.ops.solidify(bm, geom=faces, thickness=p.float("thickness", 0.05, lo=-100, hi=100))

    elif op == "mirror":
        axis = {"x": 0, "y": 1, "z": 2}[p.str("axis", "x", choices=["x", "y", "z"])]
        geom = list(bm.verts) + list(bm.edges) + list(bm.faces)
        bmesh.ops.mirror(bm, geom=geom, axis=axis, merge_dist=p.float("merge", 0.001, lo=0, hi=1))

    elif op == "spin":
        faces = _faces_from(p, bm, required=False)
        geom = (list({v for f in faces for v in f.verts}) + list({e for f in faces for e in f.edges}) + faces) \
            if faces else list(bm.verts) + list(bm.edges) + list(bm.faces)
        axis = Vector(p.vec("axis", (0.0, 0.0, 1.0)))
        bmesh.ops.spin(bm, geom=geom, cent=Vector(p.vec("center", (0.0, 0.0, 0.0))), axis=axis,
                       angle=math.radians(p.float("angle", 360, lo=-3600, hi=3600)),
                       steps=p.int("steps", 12, lo=1, hi=512), use_duplicate=False)
        bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0001)

    elif op == "bridge":
        edges = [bm.edges[i] for i in _indices(p, "edges", len(bm.edges))]
        if len(edges) < 2:
            raise CommandError("Il faut au moins deux arêtes (deux boucles) à relier.")
        bmesh.ops.bridge_loops(bm, edges=edges)

    elif op == "loop_cut":
        e_idx = p.int("edge", lo=0, hi=len(bm.edges) - 1)
        ring = _edge_ring(bm.edges[e_idx])
        bmesh.ops.subdivide_edgering(bm, edges=ring, cuts=p.int("cuts", 1, lo=1, hi=64), profile_shape="SMOOTH",
                                     smooth=0.0)
        extra["ring_edges"] = len(ring)

    return {**_finish(obj, bm, extra), "operation": op, "_label": f"{op} · {obj.name}"}


def _verts_from(p: Params, bm, allow_all=False):
    if p.has("vertices"):
        verts = [bm.verts[i] for i in _indices(p, "vertices", len(bm.verts))]
    elif p.has("faces"):
        verts = list({v for i in _indices(p, "faces", len(bm.faces)) for v in bm.faces[i].verts})
    elif p.has("edges"):
        verts = list({v for i in _indices(p, "edges", len(bm.edges)) for v in bm.edges[i].verts})
    elif allow_all or p.bool("all", False):
        verts = list(bm.verts)
    else:
        raise CommandError("Indiquez vertices, edges ou faces (ou all=true).")
    if not verts:
        raise CommandError("Aucun sommet visé.")
    return verts


def _edge_ring(edge):
    """Anneau d'arêtes passant par une arête (quads uniquement), pour la coupe en boucle."""
    ring = [edge]
    seen = {edge}
    for start_loop in edge.link_loops[:2]:
        loop = start_loop
        while True:
            f = loop.face
            if len(f.verts) != 4:
                break
            opp = loop.link_loop_next.link_loop_next
            e = opp.edge
            if e in seen:
                break
            ring.append(e)
            seen.add(e)
            if len(e.link_loops) < 2:
                break
            nxt = e.link_loops[0] if e.link_loops[0] != opp else e.link_loops[1]
            loop = nxt
    return ring


@command("select_linked_faces", history=False)
def cmd_select_linked(engine, p: Params):
    """Renvoie toutes les faces connectées aux faces données (sélection « liée »)."""
    obj = get_mesh_object(p.str("name"))
    bm = _bm(obj)
    seeds = [bm.faces[i] for i in _indices(p, "faces", len(bm.faces))]
    seen = set(seeds)
    stack = list(seeds)
    while stack:
        f = stack.pop()
        for e in f.edges:
            for nf in e.link_faces:
                if nf not in seen:
                    seen.add(nf)
                    stack.append(nf)
    out = sorted(f.index for f in seen)
    bm.free()
    return {"faces": out}


@command("select_by_normal", history=False)
def cmd_select_by_normal(engine, p: Params):
    """Renvoie les faces orientées vers une direction (ex. [0,0,1] = faces du dessus)."""
    obj = get_mesh_object(p.str("name"))
    d = Vector(p.vec("direction", (0.0, 0.0, 1.0))).normalized()
    thr = math.cos(math.radians(p.float("angle", 30, lo=0, hi=180)))
    faces = [f.index for f in obj.data.polygons if f.normal.dot(d) >= thr]
    return {"faces": faces}


# ---------------------------------------------------------------------------
# Modificateurs (tous les modificateurs Blender, via introspection RNA)
# ---------------------------------------------------------------------------
MODIFIER_ALIASES = {
    "subdivision": "SUBSURF", "subsurf": "SUBSURF", "subdivision_surface": "SUBSURF",
    "boolean": "BOOLEAN", "mirror": "MIRROR", "array": "ARRAY", "bevel": "BEVEL", "solidify": "SOLIDIFY",
    "decimate": "DECIMATE", "remesh": "REMESH", "displace": "DISPLACE", "smooth": "SMOOTH",
    "skin": "SKIN", "wireframe": "WIREFRAME", "triangulate": "TRIANGULATE", "weld": "WELD",
    "screw": "SCREW", "curve": "CURVE", "simple_deform": "SIMPLE_DEFORM", "cast": "CAST", "wave": "WAVE",
    "shrinkwrap": "SHRINKWRAP", "lattice": "LATTICE", "armature": "ARMATURE", "edge_split": "EDGE_SPLIT",
    "multires": "MULTIRES", "build": "BUILD", "laplacian_smooth": "LAPLACIANSMOOTH",
    "corrective_smooth": "CORRECTIVE_SMOOTH", "weighted_normal": "WEIGHTED_NORMAL",
}

MODIFIER_LABELS_FR = {
    "SUBSURF": "Subdivision Surface", "BOOLEAN": "Booléen", "MIRROR": "Miroir", "ARRAY": "Réseau (Array)",
    "BEVEL": "Biseau (Bevel)", "SOLIDIFY": "Épaisseur", "DECIMATE": "Décimation", "REMESH": "Remaillage",
    "DISPLACE": "Déplacement", "SMOOTH": "Lissage", "SKIN": "Peau (Skin)", "WIREFRAME": "Fil de fer",
    "TRIANGULATE": "Triangulation", "WELD": "Soudure", "SCREW": "Vis (Screw)", "SIMPLE_DEFORM": "Déformation",
    "CAST": "Projection (Cast)", "WAVE": "Vague", "SHRINKWRAP": "Rétraction", "ARMATURE": "Armature",
    "EDGE_SPLIT": "Séparation d'arêtes", "LAPLACIANSMOOTH": "Lissage laplacien",
    "CORRECTIVE_SMOOTH": "Lissage correctif", "WEIGHTED_NORMAL": "Normales pondérées", "MULTIRES": "Multirésolution",
}


def _modifier_type(t: str) -> str:
    tt = MODIFIER_ALIASES.get(t.lower(), t.upper())
    valid = {i.identifier for i in bpy.types.Modifier.bl_rna.properties["type"].enum_items}
    if tt not in valid:
        raise CommandError(f"Type de modificateur inconnu : « {t} ».")
    return tt


def describe_modifier(m) -> dict:
    return {"name": m.name, "type": m.type, "label": MODIFIER_LABELS_FR.get(m.type, m.type),
            "show_viewport": m.show_viewport, "show_render": m.show_render,
            "properties": rna_properties(m, skip={"show_viewport", "show_render", "show_in_editmode",
                                                  "show_on_cage", "show_expanded", "is_active", "use_pin_to_last",
                                                  "use_apply_on_spline", "is_override_data_local"})}


@command("list_modifier_types", history=False)
def cmd_modifier_types(engine, p: Params):
    """Types de modificateurs disponibles pour les maillages."""
    items = bpy.types.Modifier.bl_rna.properties["type"].enum_items
    mesh_ok = {"SUBSURF", "BOOLEAN", "MIRROR", "ARRAY", "BEVEL", "SOLIDIFY", "DECIMATE", "REMESH", "DISPLACE",
               "SMOOTH", "SKIN", "WIREFRAME", "TRIANGULATE", "WELD", "SCREW", "SIMPLE_DEFORM", "CAST", "WAVE",
               "SHRINKWRAP", "ARMATURE", "EDGE_SPLIT", "LAPLACIANSMOOTH", "CORRECTIVE_SMOOTH", "WEIGHTED_NORMAL",
               "MULTIRES", "BUILD", "CURVE", "LATTICE", "OCEAN", "SCREW", "MASK", "UV_PROJECT"}
    return {"types": [{"type": i.identifier, "label": MODIFIER_LABELS_FR.get(i.identifier, i.name)}
                      for i in items if i.identifier in mesh_ok]}


@command("get_modifiers", history=False)
def cmd_get_modifiers(engine, p: Params):
    """Modificateurs d'un objet avec toutes leurs propriétés modifiables."""
    o = get_object(p.str("name"))
    return {"name": o.name, "modifiers": [describe_modifier(m) for m in o.modifiers]}


@command("add_modifier", label="Ajouter un modificateur")
def cmd_add_modifier(engine, p: Params):
    """Ajoute un modificateur Blender (subdivision, boolean, mirror, array, bevel, solidify, decimate, remesh...).

    `settings` : propriétés à définir (ex. {"levels": 2} pour SUBSURF, {"operation": "DIFFERENCE", "object": "Cube"}
    pour BOOLEAN, {"count": 5, "relative_offset_displace": [1.2,0,0]} pour ARRAY, {"use_axis": [true,false,false]} pour MIRROR).
    """
    objs = target_objects(p)
    mtype = _modifier_type(p.str("type"))
    settings = p.dict("settings", {})
    created = []
    for o in objs:
        if o.type != "MESH":
            raise CommandError(f"« {o.name} » n'est pas un maillage.")
        m = o.modifiers.new(p.str("modifier_name", MODIFIER_LABELS_FR.get(mtype, mtype), max_len=60), mtype)
        if m is None:
            raise CommandError(f"Blender n'a pas pu ajouter le modificateur {mtype} à « {o.name} ».")
        _modifier_defaults(m)
        for k, v in settings.items():
            rna_set(m, k, v)
        if mtype == "BOOLEAN" and m.object is not None and p.bool("hide_cutter", True):
            m.object.display_type = "WIRE"
            m.object.hide_render = True
        created.append(m.name)
        if p.bool("apply", False):
            apply_modifiers(o, only=[m.name])
    return {"objects": [o.name for o in objs], "modifiers": created,
            "_label": f"Modificateur {MODIFIER_LABELS_FR.get(mtype, mtype)}"}


def _modifier_defaults(m):
    if m.type == "SUBSURF":
        m.levels = 2
        m.render_levels = 2
    elif m.type == "BEVEL":
        m.width = 0.03
        m.segments = 3
        m.limit_method = "ANGLE"
    elif m.type == "SOLIDIFY":
        m.thickness = 0.05
    elif m.type == "REMESH":
        m.mode = "VOXEL"
        m.voxel_size = 0.05
    elif m.type == "BOOLEAN":
        m.solver = "EXACT"


@command("set_modifier", label="Régler un modificateur")
def cmd_set_modifier(engine, p: Params):
    """Modifie les propriétés d'un modificateur existant (settings), sa visibilité ou son ordre (move: up/down)."""
    o = get_object(p.str("name"))
    m = o.modifiers.get(p.str("modifier"))
    if m is None:
        raise CommandError(f"Modificateur introuvable : « {p.str('modifier')} ».")
    for k, v in p.dict("settings", {}).items():
        rna_set(m, k, v)
    if p.has("show_viewport"):
        m.show_viewport = p.bool("show_viewport")
        m.show_render = m.show_viewport
    if p.has("move"):
        i = list(o.modifiers).index(m)
        j = i - 1 if p.str("move", choices=["up", "down"]) == "up" else i + 1
        if 0 <= j < len(o.modifiers):
            o.modifiers.move(i, j)
    if p.has("new_name"):
        m.name = p.str("new_name", max_len=60)
    return {"modifier": describe_modifier(m), "_label": f"Régler {m.name}"}


@command("apply_modifier", label="Appliquer un modificateur")
def cmd_apply_modifier(engine, p: Params):
    """Applique un modificateur (ou tous avec all=true) : la géométrie devient définitive.

    Raccourci : si `type` est donné sans modificateur existant, il est ajouté avec `settings` puis appliqué.
    """
    objs = target_objects(p)
    applied = []
    for o in objs:
        if o.type != "MESH":
            continue
        if p.bool("all", False):
            n = apply_modifiers(o)
            applied.append(f"{o.name}: {n}")
        elif p.has("modifier"):
            if o.modifiers.get(p.str("modifier")) is None:
                raise CommandError(f"Modificateur introuvable sur « {o.name} ».")
            apply_modifiers(o, only=[p.str("modifier")])
            applied.append(f"{o.name}: {p.str('modifier')}")
        elif p.has("type"):
            mtype = _modifier_type(p.str("type"))
            m = o.modifiers.new(mtype, mtype)
            _modifier_defaults(m)
            for k, v in p.dict("settings", {}).items():
                rna_set(m, k, v)
            apply_modifiers(o, only=[m.name])
            applied.append(f"{o.name}: {mtype}")
        else:
            raise CommandError("Indiquez « modifier », « type » ou all=true.")
    return {"applied": applied, "objects": [o.name for o in objs]}


@command("remove_modifier", label="Retirer un modificateur")
def cmd_remove_modifier(engine, p: Params):
    """Retire un modificateur sans l'appliquer."""
    o = get_object(p.str("name"))
    m = o.modifiers.get(p.str("modifier"))
    if m is None:
        raise CommandError("Modificateur introuvable.")
    o.modifiers.remove(m)
    return {"name": o.name}


@command("boolean", label="Booléen")
def cmd_boolean(engine, p: Params):
    """Booléen direct entre deux objets : operation union/difference/intersect. Applique et masque l'outil."""
    target = get_mesh_object(p.str("name"))
    cutter = get_mesh_object(p.str("cutter"))
    op = {"union": "UNION", "difference": "DIFFERENCE", "intersect": "INTERSECT"}[
        p.str("operation", "difference", choices=["union", "difference", "intersect"])]
    m = target.modifiers.new("Booléen", "BOOLEAN")
    m.operation = op
    m.object = cutter
    m.solver = "EXACT"
    if p.bool("apply", True):
        apply_modifiers(target, only=[m.name])
        if p.bool("delete_cutter", False):
            bpy.data.objects.remove(cutter, do_unlink=True)
        else:
            cutter.hide_set(True)
            cutter.hide_render = True
    return {"name": target.name, "faces": len(target.data.polygons), "_label": f"Booléen {op.lower()}"}


@command("shade", label="Ombrage")
def cmd_shade(engine, p: Params):
    """Ombrage lisse (smooth), plat (flat) ou automatique selon l'angle (auto, angle en degrés)."""
    objs = [o for o in target_objects(p) if o.type == "MESH"]
    mode = p.str("mode", "smooth", choices=["smooth", "flat", "auto"])
    for o in objs:
        if mode == "flat":
            o.data.shade_flat()
        elif mode == "smooth":
            o.data.shade_smooth()
        else:
            o.data.set_sharp_from_angle(angle=math.radians(p.float("angle", 30, lo=0, hi=180)))
    return {"objects": [o.name for o in objs], "mode": mode}


@command("optimize_mesh", label="Optimiser le maillage")
def cmd_optimize(engine, p: Params):
    """Nettoie et optimise : fusion des doublons, sommets isolés, normales, trous, décimation vers un nombre de triangles cible."""
    objs = [o for o in target_objects(p) if o.type == "MESH"]
    report = []
    for o in objs:
        if p.bool("apply_modifiers", False):
            apply_modifiers(o)
        bm = _bm(o)
        tri_before = sum(len(f.verts) - 2 for f in bm.faces)
        r = {"name": o.name, "triangles_before": tri_before}
        if p.bool("merge", True):
            n0 = len(bm.verts)
            bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=p.float("merge_distance", 0.0001, lo=0, hi=1))
            r["merged_vertices"] = n0 - len(bm.verts)
        if p.bool("remove_loose", True):
            loose = [v for v in bm.verts if not v.link_edges]
            loose_e = [e for e in bm.edges if not e.link_faces]
            bmesh.ops.delete(bm, geom=loose_e, context="EDGES_ONLY") if loose_e else None
            loose = [v for v in bm.verts if v.is_valid and not v.link_edges]
            if loose:
                bmesh.ops.delete(bm, geom=loose, context="VERTS")
            r["removed_loose"] = len(loose) + len(loose_e)
        if p.bool("dissolve_degenerate", True):
            bmesh.ops.dissolve_degenerate(bm, edges=bm.edges, dist=1e-6)
        if p.bool("fill_holes", False):
            n0 = len(bm.faces)
            bmesh.ops.holes_fill(bm, edges=bm.edges, sides=p.int("hole_sides", 0, lo=0))
            r["filled_holes"] = len(bm.faces) - n0
        if p.bool("fix_normals", True):
            bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bm.to_mesh(o.data)
        bm.free()
        o.data.update()
        target = p.int("target_triangles", 0, lo=0, hi=50_000_000)
        cur = sum(len(pl.vertices) - 2 for pl in o.data.polygons)
        if target and cur > target:
            m = o.modifiers.new("Décimation", "DECIMATE")
            m.ratio = max(0.001, target / cur)
            m.use_collapse_triangulate = True
            apply_modifiers(o, only=[m.name])
        r["triangles_after"] = sum(len(pl.vertices) - 2 for pl in o.data.polygons)
        report.append(r)
    return {"report": report, "_label": "Optimiser"}


@command("inspect_model", history=False)
def cmd_inspect(engine, p: Params):
    """Analyse un maillage : polygones, non-manifold, trous, normales, doublons, UV, échelle, matériaux."""
    o = get_mesh_object(p.str("name"))
    dg = bpy.context.evaluated_depsgraph_get()
    use_eval = p.bool("evaluated", True)
    me = o.evaluated_get(dg).to_mesh() if use_eval else o.data
    bm = bmesh.new()
    bm.from_mesh(me)
    try:
        non_manifold_edges = [e for e in bm.edges if not e.is_manifold]
        boundary = [e for e in bm.edges if e.is_boundary]
        wire = [e for e in bm.edges if e.is_wire]
        loose_v = [v for v in bm.verts if not v.link_edges]
        ngons = sum(1 for f in bm.faces if len(f.verts) > 4)
        quads = sum(1 for f in bm.faces if len(f.verts) == 4)
        tris = sum(1 for f in bm.faces if len(f.verts) == 3)
        zero_faces = sum(1 for f in bm.faces if f.calc_area() < 1e-10)
        # doublons (approximation par grille)
        seen = {}
        dup = 0
        for v in bm.verts:
            k = (round(v.co.x, 5), round(v.co.y, 5), round(v.co.z, 5))
            if k in seen:
                dup += 1
            else:
                seen[k] = 1
        islands = _count_islands(bm)
        try:
            volume = bm.calc_volume(signed=True)
        except Exception:
            volume = None
        dims = o.dimensions
        result = {
            "name": o.name,
            "evaluated": use_eval,
            "vertices": len(bm.verts), "edges": len(bm.edges), "faces": len(bm.faces),
            "triangles": sum(len(f.verts) - 2 for f in bm.faces),
            "quads": quads, "tris": tris, "ngons": ngons,
            "non_manifold_edges": len(non_manifold_edges), "boundary_edges": len(boundary),
            "wire_edges": len(wire), "loose_vertices": len(loose_v), "duplicate_vertices": dup,
            "zero_area_faces": zero_faces, "islands": islands,
            "watertight": len(non_manifold_edges) == 0 and len(boundary) == 0,
            "normals_inverted": volume is not None and volume < 0,
            "volume_m3": round(abs(volume), 6) if volume is not None else None,
            "dimensions_m": [round(x, 4) for x in dims],
            "uv_maps": [u.name for u in o.data.uv_layers],
            "materials": [m.name if m else None for m in o.data.materials],
            "modifiers": [m.type for m in o.modifiers],
            "scale_applied": all(abs(s - 1) < 1e-6 for s in o.scale),
        }
        issues = []
        if result["non_manifold_edges"]:
            issues.append(f"{result['non_manifold_edges']} arêtes non-manifold (impression 3D impossible en l'état)")
        if result["boundary_edges"]:
            issues.append(f"{result['boundary_edges']} arêtes de bord : le maillage a des trous")
        if dup:
            issues.append(f"{dup} sommets en double (utilisez optimize_mesh)")
        if result["normals_inverted"]:
            issues.append("normales inversées (utilisez modify_geometry recalc_normals)")
        if not result["uv_maps"]:
            issues.append("aucune carte UV (nécessaire pour les textures exportées)")
        if not result["scale_applied"]:
            issues.append("échelle non appliquée (recommandé avant export moteur de jeu)")
        if ngons:
            issues.append(f"{ngons} n-gones (triangulés à l'export)")
        result["issues"] = issues
        return result
    finally:
        bm.free()
        if use_eval:
            o.evaluated_get(dg).to_mesh_clear()


def _count_islands(bm) -> int:
    seen = set()
    count = 0
    for v in bm.verts:
        if v in seen:
            continue
        count += 1
        stack = [v]
        seen.add(v)
        while stack:
            x = stack.pop()
            for e in x.link_edges:
                y = e.other_vert(x)
                if y not in seen:
                    seen.add(y)
                    stack.append(y)
    return count
