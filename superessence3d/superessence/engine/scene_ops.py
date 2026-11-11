"""Scène, objets, sélection, transformations, collections, scènes, lumières, caméras."""
from __future__ import annotations

import math

import bpy
from mathutils import Euler, Matrix, Vector

from .colors import to_hex, to_linear
from .core import CommandError, Params, command
from .util import (get_object, link_to_collection, matrix_list, override, select_only, target_objects,
                   unique_name, vec, world_bbox, get_json_prop, ensure_object_mode)


# ---------------------------------------------------------------------------
# Scène par défaut
# ---------------------------------------------------------------------------
def setup_default_scene() -> None:
    sc = bpy.context.scene
    sc.name = "Scène"
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 64
    sc.render.resolution_x, sc.render.resolution_y = 1280, 720
    sc.frame_start, sc.frame_end = 1, 120
    sc.render.fps = 24
    sc.unit_settings.system = "METRIC"
    world = bpy.data.worlds.new("Monde")
    sc.world = world
    _set_world(world, (0.05, 0.055, 0.065, 1.0), 1.0)
    cam_data = bpy.data.cameras.new("Caméra")
    cam = bpy.data.objects.new("Caméra", cam_data)
    sc.collection.objects.link(cam)
    cam.location = (7.0, -7.0, 5.0)
    look_at(cam, Vector((0, 0, 0.8)))
    sc.camera = cam
    key = bpy.data.objects.new("Lumière", bpy.data.lights.new("Lumière", "AREA"))
    key.data.energy = 800
    key.data.size = 4
    key.location = (4, -3, 6)
    look_at(key, Vector((0, 0, 0)))
    sc.collection.objects.link(key)


def _set_world(world, color, strength):
    nt = world.node_tree
    if nt is None:
        if world.node_tree is None:
            world.use_nodes = True
        nt = world.node_tree
    bg = next((n for n in nt.nodes if n.type == "BACKGROUND"), None)
    if bg is None:
        bg = nt.nodes.new("ShaderNodeBackground")
        out = next((n for n in nt.nodes if n.type == "OUTPUT_WORLD"), None) or nt.nodes.new("ShaderNodeOutputWorld")
        nt.links.new(bg.outputs[0], out.inputs[0])
    bg.inputs["Color"].default_value = color
    bg.inputs["Strength"].default_value = strength


def look_at(obj, target: Vector) -> None:
    direction = target - obj.location
    if direction.length < 1e-6:
        return
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


# ---------------------------------------------------------------------------
# Description de la scène
# ---------------------------------------------------------------------------
def object_summary(o: bpy.types.Object) -> dict:
    d = {
        "name": o.name,
        "type": o.type,
        "parent": o.parent.name if o.parent else None,
        "collections": [c.name for c in o.users_collection],
        "visible": o.visible_get() if o.name in bpy.context.view_layer.objects else False,
        "hidden": o.hide_get() if o.name in bpy.context.view_layer.objects else True,
        "selected": o.select_get() if o.name in bpy.context.view_layer.objects else False,
        "location": vec(o.location),
        "rotation": vec(o.rotation_euler),
        "scale": vec(o.scale),
        "dimensions": vec(o.dimensions),
        "matrix_world": matrix_list(o.matrix_world),
        "generated": o.get("se_spec") is not None,
    }
    if o.type == "MESH":
        me = o.data
        d["mesh"] = {"vertices": len(me.vertices), "faces": len(me.polygons), "edges": len(me.edges),
                     "triangles": sum(len(p.vertices) - 2 for p in me.polygons)}
        d["materials"] = [m.name if m else None for m in me.materials]
        d["modifiers"] = [{"name": m.name, "type": m.type, "show_viewport": m.show_viewport} for m in o.modifiers]
    elif o.type == "LIGHT":
        d["light"] = {"type": o.data.type, "energy": o.data.energy, "color": to_hex(o.data.color)}
    elif o.type == "CAMERA":
        d["camera"] = {"lens": o.data.lens, "active": bpy.context.scene.camera == o}
    elif o.type == "ARMATURE":
        d["bones"] = len(o.data.bones)
    if o.animation_data and o.animation_data.action:
        d["action"] = o.animation_data.action.name
    return d


def collection_tree(col: bpy.types.Collection, layer_col=None) -> dict:
    return {
        "name": col.name,
        "objects": [o.name for o in col.objects],
        "children": [collection_tree(c) for c in col.children],
        "hide_viewport": col.hide_viewport,
    }


def scene_info() -> dict:
    sc = bpy.context.scene
    objs = list(sc.objects)
    totals = {"vertices": 0, "faces": 0, "triangles": 0}
    for o in objs:
        if o.type == "MESH":
            totals["vertices"] += len(o.data.vertices)
            totals["faces"] += len(o.data.polygons)
            totals["triangles"] += sum(len(p.vertices) - 2 for p in o.data.polygons)
    act = bpy.context.view_layer.objects.active
    return {
        "scene": sc.name,
        "scenes": [s.name for s in bpy.data.scenes],
        "frame": {"current": sc.frame_current, "start": sc.frame_start, "end": sc.frame_end, "fps": sc.render.fps},
        "render": {"engine": sc.render.engine, "resolution": [sc.render.resolution_x, sc.render.resolution_y],
                   "samples": sc.cycles.samples},
        "camera": sc.camera.name if sc.camera else None,
        "active": act.name if act else None,
        "selected": [o.name for o in sc.objects if o.select_get()],
        "object_count": len(objs),
        "totals": totals,
        "collections": collection_tree(sc.collection),
        "materials": [m.name for m in bpy.data.materials if m.users],
        "world": _world_info(sc.world),
        "unit": "mètres",
    }


def _world_info(world):
    if world is None or world.node_tree is None:
        return None
    bg = next((n for n in world.node_tree.nodes if n.type == "BACKGROUND"), None)
    if bg is None:
        return {"name": world.name}
    return {"name": world.name, "color": to_hex([_lin2s(c) for c in bg.inputs["Color"].default_value[:3]]),
            "strength": bg.inputs["Strength"].default_value}


def _lin2s(c):
    from .colors import linear_to_srgb
    return linear_to_srgb(c)


@command("get_scene_info", history=False)
def cmd_scene_info(engine, p: Params):
    """Résumé de la scène : objets, totaux de polygones, caméra, rendu, collections."""
    info = scene_info()
    info["objects"] = [object_summary(o) for o in bpy.context.scene.objects]
    info["status"] = engine.status()
    return info


@command("list_objects", history=False)
def cmd_list_objects(engine, p: Params):
    """Liste les objets de la scène (filtre facultatif par type)."""
    t = p.str("type", None)
    objs = [o for o in bpy.context.scene.objects if t is None or o.type == t.upper()]
    return {"objects": [object_summary(o) for o in objs]}


@command("get_object_details", history=False)
def cmd_object_details(engine, p: Params):
    """Détails complets d'un objet : transformations, maillage, matériaux, modificateurs, spécification générée."""
    o = get_object(p.str("name"))
    d = object_summary(o)
    d["rotation_mode"] = o.rotation_mode
    d["children"] = [c.name for c in o.children]
    spec = get_json_prop(o, "se_spec")
    if spec:
        d["generation_spec"] = spec
    if o.type == "MESH":
        me = o.data
        d["uv_maps"] = [uv.name for uv in me.uv_layers]
        d["color_attributes"] = [c.name for c in me.color_attributes]
        d["vertex_groups"] = [g.name for g in o.vertex_groups]
        bb = world_bbox([o])
        if bb:
            d["world_bbox"] = {"min": vec(bb[0]), "max": vec(bb[1])}
    if o.type == "ARMATURE":
        d["bone_names"] = [b.name for b in o.data.bones]
    return d


# ---------------------------------------------------------------------------
# Sélection
# ---------------------------------------------------------------------------
@command("select", history=False, changes_scene=True)
def cmd_select(engine, p: Params):
    """Sélectionne des objets. mode: set (remplace), add, remove, toggle, all, none."""
    mode = p.str("mode", "set", choices=["set", "add", "remove", "toggle", "all", "none"])
    names = [str(n) for n in p.list("names", [])]
    if p.has("name"):
        names.append(p.str("name"))
    vl = bpy.context.view_layer
    if mode == "all":
        for o in vl.objects:
            if o.visible_get():
                o.select_set(True)
    elif mode == "none":
        for o in vl.objects:
            o.select_set(False)
    else:
        objs = [get_object(n) for n in names]
        if mode == "set":
            for o in vl.objects:
                o.select_set(False)
        for o in objs:
            if o.name not in vl.objects:
                continue
            if mode == "remove":
                o.select_set(False)
            elif mode == "toggle":
                o.select_set(not o.select_get())
            else:
                o.select_set(True)
        if objs and mode in ("set", "add"):
            vl.objects.active = objs[-1]
    act = vl.objects.active
    return {"selected": [o.name for o in vl.objects if o.select_get()], "active": act.name if act else None}


# ---------------------------------------------------------------------------
# Création / suppression / duplication
# ---------------------------------------------------------------------------
PRIMITIVES = {
    "cube": lambda s: bpy.ops.mesh.primitive_cube_add(size=s),
    "plane": lambda s: bpy.ops.mesh.primitive_plane_add(size=s),
    "sphere": lambda s: bpy.ops.mesh.primitive_uv_sphere_add(radius=s / 2, segments=32, ring_count=16),
    "icosphere": lambda s: bpy.ops.mesh.primitive_ico_sphere_add(radius=s / 2, subdivisions=3),
    "cylinder": lambda s: bpy.ops.mesh.primitive_cylinder_add(radius=s / 2, depth=s, vertices=32),
    "cone": lambda s: bpy.ops.mesh.primitive_cone_add(radius1=s / 2, depth=s, vertices=32),
    "torus": lambda s: bpy.ops.mesh.primitive_torus_add(major_radius=s / 2, minor_radius=s / 8),
    "monkey": lambda s: bpy.ops.mesh.primitive_monkey_add(size=s),
    "grid": lambda s: bpy.ops.mesh.primitive_grid_add(size=s, x_subdivisions=20, y_subdivisions=20),
    "circle": lambda s: bpy.ops.mesh.primitive_circle_add(radius=s / 2, fill_type="NGON"),
}
PRIMITIVE_NAMES_FR = {"cube": "Cube", "plane": "Plan", "sphere": "Sphère", "icosphere": "Icosphère",
                      "cylinder": "Cylindre", "cone": "Cône", "torus": "Tore", "monkey": "Singe",
                      "grid": "Grille", "circle": "Disque"}


@command("create_mesh", label="Créer un maillage")
def cmd_create_mesh(engine, p: Params):
    """Crée un maillage : primitive (cube, sphere, cylinder...) ou maillage libre (vertices + faces)."""
    ensure_object_mode()
    loc = p.vec("location", (0.0, 0.0, 0.0))
    rot = p.vec("rotation", (0.0, 0.0, 0.0))
    scl = p.vec("scale", (1.0, 1.0, 1.0))
    if p.has("vertices"):
        verts = p.list("vertices")
        faces = p.list("faces", [])
        edges = p.list("edges", [])
        try:
            verts = [tuple(float(c) for c in v) for v in verts]
            faces = [tuple(int(i) for i in f) for f in faces]
            edges = [tuple(int(i) for i in e) for e in edges]
        except (TypeError, ValueError):
            raise CommandError("vertices/faces/edges mal formés.") from None
        if any(len(v) != 3 for v in verts):
            raise CommandError("Chaque sommet doit avoir 3 coordonnées.")
        nv = len(verts)
        if any(i < 0 or i >= nv for f in faces + edges for i in f):
            raise CommandError("Un indice de face/arête dépasse le nombre de sommets.")
        if len(verts) > 2_000_000:
            raise CommandError("Trop de sommets (max 2 000 000).")
        name = unique_name(p.str("name", "Maillage"))
        me = bpy.data.meshes.new(name)
        me.from_pydata(verts, edges, faces)
        me.validate()
        me.update()
        obj = bpy.data.objects.new(name, me)
        link_to_collection(obj)
    else:
        kind = p.str("primitive", "cube", choices=list(PRIMITIVES))
        size = p.float("size", 2.0, lo=0.001, hi=10000)
        PRIMITIVES[kind](size)
        obj = bpy.context.active_object
        obj.name = unique_name(p.str("name", PRIMITIVE_NAMES_FR[kind]))
        obj.data.name = obj.name
    obj.location = loc
    obj.rotation_euler = rot
    obj.scale = scl
    if p.bool("smooth", False):
        obj.data.shade_smooth()
    if p.has("color") or p.has("material"):
        from .materials import assign_simple_material
        assign_simple_material(obj, p.str("material", None), p.color("color", (0.8, 0.8, 0.8, 1)))
    select_only([obj])
    return {"name": obj.name, "_label": f"Créer {obj.name}", "object": object_summary(obj)}


@command("delete_object", label="Supprimer", destructive=True)
def cmd_delete(engine, p: Params):
    """Supprime des objets (avec leurs enfants si children=true). Exige confirm=true."""
    objs = target_objects(p)
    names = [o.name for o in objs]
    if p.bool("children", False):
        extra = []
        for o in objs:
            extra.extend(o.children_recursive)
        objs = list({o.name: o for o in objs + extra}.values())
        names = [o.name for o in objs]
    p.confirm(f"supprimer {', '.join(names)}")
    ensure_object_mode()
    for o in objs:
        bpy.data.objects.remove(o, do_unlink=True)
    _purge_orphans()
    return {"deleted": names, "_label": f"Supprimer {', '.join(names)[:60]}"}


def _purge_orphans():
    for coll in (bpy.data.meshes, bpy.data.curves, bpy.data.armatures, bpy.data.lights, bpy.data.cameras):
        for d in list(coll):
            if d.users == 0:
                coll.remove(d)


@command("duplicate_object", label="Dupliquer")
def cmd_duplicate(engine, p: Params):
    """Duplique des objets (copie indépendante, ou liée avec linked=true). Décalage facultatif."""
    objs = target_objects(p)
    linked = p.bool("linked", False)
    offset = Vector(p.vec("offset", (0.0, 0.0, 0.0)))
    count = p.int("count", 1, lo=1, hi=500)
    with_children = p.bool("children", True)
    created = []
    for i in range(count):
        mapping = {}
        roots = objs
        all_objs = []
        for o in roots:
            all_objs.extend([o] + (list(o.children_recursive) if with_children else []))
        for o in all_objs:
            if o.name in mapping:
                continue
            c = o.copy()
            if o.data is not None and not linked:
                c.data = o.data.copy()
            if o.animation_data and o.animation_data.action and not linked:
                c.animation_data.action = o.animation_data.action.copy()
            for col in o.users_collection:
                col.objects.link(c)
            mapping[o.name] = c
        for src, c in mapping.items():
            so = bpy.data.objects[src]
            if so.parent and so.parent.name in mapping:
                c.parent = mapping[so.parent.name]
                c.matrix_parent_inverse = so.matrix_parent_inverse.copy()
            for m in c.modifiers:
                for attr in ("object", "mirror_object", "offset_object"):
                    t = getattr(m, attr, None)
                    if t is not None and t.name in mapping:
                        setattr(m, attr, mapping[t.name])
        for o in roots:
            c = mapping[o.name]
            c.location = o.location + offset * (i + 1)
            created.append(c)
    select_only(created)
    return {"created": [c.name for c in created], "_label": f"Dupliquer {', '.join(o.name for o in objs)[:60]}"}


@command("rename_object", label="Renommer")
def cmd_rename(engine, p: Params):
    """Renomme un objet."""
    o = get_object(p.str("name"))
    new = p.str("new_name", max_len=60).strip()
    if not new:
        raise CommandError("Le nouveau nom est vide.")
    old = o.name
    o.name = new
    return {"name": o.name, "old_name": old, "_label": f"Renommer {old} → {o.name}"}


# ---------------------------------------------------------------------------
# Transformations
# ---------------------------------------------------------------------------
@command("transform_object", label="Transformer")
def cmd_transform(engine, p: Params):
    """Modifie position/rotation (radians, ou degrés avec degrees=true)/échelle. relative=true pour un décalage."""
    objs = target_objects(p)
    rel = p.bool("relative", False)
    deg = p.bool("degrees", False)
    for o in objs:
        if p.has("location"):
            v = Vector(p.vec("location"))
            o.location = o.location + v if rel else v
        if p.has("rotation"):
            r = Vector(p.vec("rotation"))
            if deg:
                r = Vector([math.radians(x) for x in r])
            if rel:
                o.rotation_euler = Euler(Vector(o.rotation_euler) + r)
            else:
                o.rotation_euler = Euler(r)
        if p.has("scale"):
            s = Vector(p.vec("scale"))
            if any(abs(x) < 1e-6 for x in s):
                raise CommandError("Une échelle nulle n'est pas autorisée.")
            o.scale = Vector([a * b for a, b in zip(o.scale, s)]) if rel else s
        if p.has("matrix_world"):
            m = p.list("matrix_world")
            if len(m) != 16:
                raise CommandError("matrix_world doit contenir 16 nombres (colonne-majeur).")
            mat = Matrix([[float(m[c * 4 + r]) for c in range(4)] for r in range(4)])
            o.matrix_world = mat
    bpy.context.view_layer.update()
    return {"objects": [object_summary(o) for o in objs],
            "_label": f"Transformer {', '.join(o.name for o in objs)[:60]}"}


@command("apply_transform", label="Appliquer les transformations")
def cmd_apply_transform(engine, p: Params):
    """Applique position/rotation/échelle au maillage (remet les transformations à zéro)."""
    objs = [o for o in target_objects(p) if o.type == "MESH"]
    if not objs:
        raise CommandError("Aucun maillage visé.")
    ensure_object_mode()
    with override(objs[0], objs):
        bpy.ops.object.transform_apply(location=p.bool("location", False), rotation=p.bool("rotation", True),
                                       scale=p.bool("scale", True))
    return {"objects": [o.name for o in objs]}


@command("set_origin", label="Définir l'origine")
def cmd_set_origin(engine, p: Params):
    """Déplace l'origine : center (géométrie), bottom (bas centré), cursor (point 'point')."""
    objs = [o for o in target_objects(p) if o.type == "MESH"]
    mode = p.str("mode", "center", choices=["center", "bottom", "point"])
    for o in objs:
        me = o.data
        if not me.vertices:
            continue
        cos = [v.co for v in me.vertices]
        lo = Vector(map(min, *cos)) if len(cos) > 1 else cos[0].copy()
        hi = Vector(map(max, *cos)) if len(cos) > 1 else cos[0].copy()
        if mode == "center":
            pivot = (lo + hi) / 2
        elif mode == "bottom":
            pivot = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
        else:
            pivot = o.matrix_world.inverted() @ Vector(p.vec("point"))
        me.transform(Matrix.Translation(-pivot))
        o.matrix_world = o.matrix_world @ Matrix.Translation(pivot)
    return {"objects": [o.name for o in objs]}


@command("set_parent", label="Parenter")
def cmd_set_parent(engine, p: Params):
    """Définit le parent d'objets (parent vide = détacher), en conservant la position mondiale."""
    objs = target_objects(p)
    parent_name = p.str("parent", "")
    parent = get_object(parent_name) if parent_name else None
    for o in objs:
        if parent is not None and (o == parent or parent in o.children_recursive):
            raise CommandError(f"Parentage circulaire impossible pour « {o.name} ».")
        mw = o.matrix_world.copy()
        o.parent = parent
        if parent is not None:
            o.matrix_parent_inverse = parent.matrix_world.inverted()
        o.matrix_world = mw
    return {"objects": [o.name for o in objs], "parent": parent.name if parent else None}


@command("set_visibility", label="Visibilité")
def cmd_visibility(engine, p: Params):
    """Masque ou affiche des objets (hidden=true/false)."""
    objs = target_objects(p)
    hidden = p.bool("hidden")
    for o in objs:
        o.hide_set(hidden)
        o.hide_render = hidden
    return {"objects": [o.name for o in objs], "hidden": hidden}


@command("join_objects", label="Joindre")
def cmd_join(engine, p: Params):
    """Fusionne plusieurs maillages en un seul objet."""
    objs = [o for o in target_objects(p) if o.type == "MESH"]
    if len(objs) < 2:
        raise CommandError("Il faut au moins deux maillages à joindre.")
    ensure_object_mode()
    active = objs[0]
    with override(active, objs):
        bpy.ops.object.join()
    return {"name": active.name}


@command("separate_object", label="Séparer")
def cmd_separate(engine, p: Params):
    """Sépare un maillage en parties non connectées (mode=loose) ou par matériau (mode=material)."""
    o = get_object(p.str("name"))
    mode = p.str("mode", "loose", choices=["loose", "material"])
    before = set(bpy.data.objects.keys())
    select_only([o])
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.separate(type=mode.upper())
    bpy.ops.object.mode_set(mode="OBJECT")
    new = sorted(set(bpy.data.objects.keys()) - before)
    return {"created": new}


# ---------------------------------------------------------------------------
# Collections
# ---------------------------------------------------------------------------
@command("create_collection", label="Nouvelle collection")
def cmd_create_collection(engine, p: Params):
    """Crée une collection (dans la collection parent facultative)."""
    name = p.str("name", "Collection", max_len=60)
    parent_name = p.str("parent", "")
    parent = bpy.data.collections.get(parent_name) if parent_name else bpy.context.scene.collection
    if parent is None:
        raise CommandError(f"Collection parent introuvable : « {parent_name} ».")
    col = bpy.data.collections.new(name)
    parent.children.link(col)
    return {"name": col.name}


@command("move_to_collection", label="Déplacer vers la collection")
def cmd_move_to_collection(engine, p: Params):
    """Déplace des objets vers une collection (créée si absente avec create=true)."""
    objs = target_objects(p)
    cname = p.str("collection")
    col = bpy.data.collections.get(cname)
    if col is None:
        if cname in ("Scene Collection", "Collection de scène"):
            col = bpy.context.scene.collection
        elif p.bool("create", True):
            col = bpy.data.collections.new(cname)
            bpy.context.scene.collection.children.link(col)
        else:
            raise CommandError(f"Collection introuvable : « {cname} ».")
    for o in objs:
        for c in list(o.users_collection):
            c.objects.unlink(o)
        col.objects.link(o)
    return {"collection": col.name, "objects": [o.name for o in objs]}


@command("set_collection_visibility", label="Visibilité de collection")
def cmd_collection_visibility(engine, p: Params):
    """Masque/affiche une collection entière."""
    col = bpy.data.collections.get(p.str("collection"))
    if col is None:
        raise CommandError("Collection introuvable.")
    col.hide_viewport = p.bool("hidden")
    col.hide_render = col.hide_viewport
    return {"collection": col.name, "hidden": col.hide_viewport}


@command("delete_collection", label="Supprimer la collection", destructive=True)
def cmd_delete_collection(engine, p: Params):
    """Supprime une collection. Ses objets sont déplacés vers la scène (ou supprimés avec delete_objects=true)."""
    col = bpy.data.collections.get(p.str("collection"))
    if col is None:
        raise CommandError("Collection introuvable.")
    p.confirm(f"supprimer la collection {col.name}")
    if p.bool("delete_objects", False):
        for o in list(col.all_objects):
            bpy.data.objects.remove(o, do_unlink=True)
    else:
        for o in list(col.objects):
            if len(o.users_collection) == 1:
                bpy.context.scene.collection.objects.link(o)
    name = col.name
    bpy.data.collections.remove(col)
    return {"deleted": name}


# ---------------------------------------------------------------------------
# Scènes
# ---------------------------------------------------------------------------
@command("scene_manage", label="Scènes")
def cmd_scene_manage(engine, p: Params):
    """Gère les scènes : action=create|switch|rename|delete (delete exige confirm=true)."""
    action = p.str("action", choices=["create", "switch", "rename", "delete"])
    win = bpy.context.window
    if action == "create":
        sc = bpy.data.scenes.new(p.str("name", "Scène", max_len=60))
        sc.render.engine = "CYCLES"
        sc.world = bpy.context.scene.world
        _set_context_scene(sc)
        return {"scene": sc.name, "_label": f"Nouvelle scène {sc.name}"}
    sc = bpy.data.scenes.get(p.str("name"))
    if sc is None:
        raise CommandError("Scène introuvable.")
    if action == "switch":
        _set_context_scene(sc)
    elif action == "rename":
        sc.name = p.str("new_name", max_len=60)
    else:
        if len(bpy.data.scenes) == 1:
            raise CommandError("Impossible de supprimer la dernière scène.")
        p.confirm(f"supprimer la scène {sc.name}")
        other = next(s for s in bpy.data.scenes if s != sc)
        _set_context_scene(other)
        bpy.data.scenes.remove(sc)
        return {"scene": other.name}
    return {"scene": bpy.context.scene.name}


def _set_context_scene(sc):
    win = bpy.context.window
    if win is not None:
        win.scene = sc
    else:  # mode module sans fenêtre
        bpy.context.window_manager.windows[0].scene = sc if bpy.context.window_manager.windows else None


# ---------------------------------------------------------------------------
# Lumières
# ---------------------------------------------------------------------------
@command("create_light", label="Créer une lumière")
def cmd_create_light(engine, p: Params):
    """Ajoute une lumière : POINT, SUN, SPOT ou AREA, avec énergie (W), couleur, taille, cible."""
    kind = p.str("type", "AREA", choices=["POINT", "SUN", "SPOT", "AREA"])
    name = unique_name(p.str("name", "Lumière"))
    data = bpy.data.lights.new(name, kind)
    data.energy = p.float("energy", 5.0 if kind == "SUN" else 500.0, lo=0, hi=1e7)
    data.color = p.color("color", (1, 1, 1, 1))[:3]
    if kind == "AREA":
        data.size = p.float("size", 2.0, lo=0.001, hi=1000)
    elif kind in ("POINT", "SPOT"):
        data.shadow_soft_size = p.float("size", 0.25, lo=0, hi=100)
    if kind == "SPOT":
        data.spot_size = math.radians(p.float("spot_angle", 45, lo=1, hi=180))
    obj = bpy.data.objects.new(name, data)
    link_to_collection(obj)
    obj.location = p.vec("location", (4.0, -4.0, 6.0))
    look_at(obj, Vector(p.vec("target", (0.0, 0.0, 0.0))))
    return {"name": obj.name}


@command("create_lighting", label="Éclairage")
def cmd_create_lighting(engine, p: Params):
    """Installe un éclairage complet : studio (3 points), outdoor (soleil + ciel), dramatic, night, product."""
    preset = p.str("preset", "studio", choices=["studio", "outdoor", "dramatic", "night", "product"])
    if p.bool("replace", True):
        for o in list(bpy.context.scene.objects):
            if o.type == "LIGHT":
                bpy.data.objects.remove(o, do_unlink=True)
    bb = world_bbox([o for o in bpy.context.scene.objects if o.type == "MESH"])
    center = (bb[0] + bb[1]) / 2 if bb else Vector((0, 0, 1))
    size = max((bb[1] - bb[0]).length, 1.0) if bb else 4.0
    col = bpy.data.collections.get("Éclairage") or bpy.data.collections.new("Éclairage")
    if col.name not in bpy.context.scene.collection.children:
        bpy.context.scene.collection.children.link(col)

    def add(name, kind, energy, color, loc, lsize=1.0):
        d = bpy.data.lights.new(name, kind)
        d.energy = energy
        d.color = color
        if kind == "AREA":
            d.size = lsize
        o = bpy.data.objects.new(unique_name(name), d)
        col.objects.link(o)
        o.location = center + Vector(loc) * size
        look_at(o, center)
        return o.name

    created = []
    k = size * size * 2.5  # l'énergie suit la taille de la scène
    world_color, world_strength = (0.22, 0.23, 0.26, 1), 0.6
    if preset == "studio":
        created += [add("Principale", "AREA", 120 * k, (1, 0.96, 0.9), (0.8, -0.9, 1.0), size * 0.6),
                    add("Remplissage", "AREA", 40 * k, (0.85, 0.9, 1), (-1.0, -0.6, 0.5), size * 0.8),
                    add("Contre-jour", "AREA", 90 * k, (1, 1, 1), (0.0, 1.1, 1.0), size * 0.5)]
    elif preset == "outdoor":
        created.append(add("Soleil", "SUN", 4.0, (1, 0.95, 0.85), (0.6, -0.5, 1.0)))
        world_color, world_strength = (0.45, 0.6, 0.9, 1), 0.8
    elif preset == "dramatic":
        created += [add("Clé dure", "SPOT", 600 * k, (1, 0.85, 0.7), (1.0, -0.4, 1.2)),
                    add("Contour", "AREA", 150 * k, (0.5, 0.6, 1.0), (-0.6, 1.0, 0.6), size * 0.3)]
        world_color, world_strength = (0.005, 0.005, 0.008, 1), 1.0
    elif preset == "night":
        created += [add("Lune", "SUN", 0.6, (0.6, 0.7, 1.0), (-0.5, 0.4, 1.0)),
                    add("Lueur", "POINT", 80 * k, (1.0, 0.55, 0.2), (0.4, -0.5, 0.2))]
        world_color, world_strength = (0.01, 0.015, 0.035, 1), 1.0
    else:  # product
        created += [add("Boîte haute", "AREA", 150 * k, (1, 1, 1), (0.0, 0.0, 1.3), size),
                    add("Gauche", "AREA", 60 * k, (1, 1, 1), (-1.2, -0.3, 0.4), size * 0.7),
                    add("Droite", "AREA", 60 * k, (1, 1, 1), (1.2, -0.3, 0.4), size * 0.7)]
        world_color, world_strength = (0.8, 0.8, 0.82, 1), 0.6
    if bpy.context.scene.world:
        _set_world(bpy.context.scene.world, to_linear(world_color), world_strength)
    return {"preset": preset, "lights": created, "_label": f"Éclairage {preset}"}


@command("set_light", label="Modifier la lumière")
def cmd_set_light(engine, p: Params):
    """Modifie une lumière : energy, color, size, type."""
    o = get_object(p.str("name"))
    if o.type != "LIGHT":
        raise CommandError("Ce n'est pas une lumière.")
    if p.has("type"):
        o.data.type = p.str("type", choices=["POINT", "SUN", "SPOT", "AREA"])
    if p.has("energy"):
        o.data.energy = p.float("energy", lo=0, hi=1e7)
    if p.has("color"):
        o.data.color = p.color("color")[:3]
    if p.has("size"):
        if o.data.type == "AREA":
            o.data.size = p.float("size", lo=0.001)
        elif o.data.type != "SUN":
            o.data.shadow_soft_size = p.float("size", lo=0)
    return {"name": o.name}


@command("set_world", label="Arrière-plan")
def cmd_set_world(engine, p: Params):
    """Couleur et intensité de l'arrière-plan (éclairage ambiant)."""
    sc = bpy.context.scene
    if sc.world is None:
        sc.world = bpy.data.worlds.new("Monde")
    _set_world(sc.world, to_linear(p.color("color", (0.05, 0.055, 0.065, 1))), p.float("strength", 1.0, lo=0, hi=1000))
    return {"world": _world_info(sc.world)}


# ---------------------------------------------------------------------------
# Caméras
# ---------------------------------------------------------------------------
@command("create_camera", label="Créer une caméra")
def cmd_create_camera(engine, p: Params):
    """Ajoute une caméra (focale en mm). frame_objects=true pour cadrer automatiquement la scène."""
    name = unique_name(p.str("name", "Caméra"))
    data = bpy.data.cameras.new(name)
    data.lens = p.float("lens", 50.0, lo=1, hi=5000)
    cam = bpy.data.objects.new(name, data)
    link_to_collection(cam)
    cam.location = p.vec("location", (7.0, -7.0, 5.0))
    look_at(cam, Vector(p.vec("target", (0.0, 0.0, 0.8))))
    if p.bool("frame_objects", True) and not p.has("location"):
        frame_camera(cam, [o for o in bpy.context.scene.objects if o.type == "MESH" and o.visible_get()])
    if p.bool("active", True):
        bpy.context.scene.camera = cam
    return {"name": cam.name}


def frame_camera(cam, objs, direction=Vector((1.0, -1.1, 0.55)), margin=1.08):
    """Cadre exactement les objets : chaque coin de la boîte englobante doit tenir dans le champ."""
    bb = world_bbox(objs)
    if not bb:
        return
    lo, hi = bb
    center = (lo + hi) / 2
    fwd = -direction.normalized()
    right = fwd.cross(Vector((0, 0, 1))).normalized()
    up = right.cross(fwd).normalized()
    sc = bpy.context.scene
    aspect = sc.render.resolution_x / max(1, sc.render.resolution_y)
    fov = cam.data.angle
    if aspect >= 1:
        th, tv = math.tan(fov / 2), math.tan(fov / 2) / aspect
    else:
        th, tv = math.tan(fov / 2) * aspect, math.tan(fov / 2)
    dist = 0.1
    for x in (lo.x, hi.x):
        for y in (lo.y, hi.y):
            for z in (lo.z, hi.z):
                p = Vector((x, y, z)) - center
                depth = p.dot(fwd)
                dist = max(dist, abs(p.dot(right)) * margin / th - depth, abs(p.dot(up)) * margin / tv - depth)
    cam.location = center - fwd * dist
    look_at(cam, center)
    cam.data.clip_start = max(0.001, dist / 1000)
    cam.data.clip_end = max(cam.data.clip_end, dist * 10 + (hi - lo).length * 2)


@command("set_camera", label="Modifier la caméra")
def cmd_set_camera(engine, p: Params):
    """Modifie une caméra : lens, active, frame_objects (cadrer), target (viser un point)."""
    cam = get_object(p.str("name")) if p.has("name") else bpy.context.scene.camera
    if cam is None or cam.type != "CAMERA":
        raise CommandError("Caméra introuvable.")
    if p.has("lens"):
        cam.data.lens = p.float("lens", lo=1, hi=5000)
    if p.has("location"):
        cam.location = p.vec("location")
    if p.has("target"):
        look_at(cam, Vector(p.vec("target")))
    if p.bool("frame_objects", False):
        names = p.list("objects", [])
        objs = [get_object(n) for n in names] if names else [o for o in bpy.context.scene.objects
                                                             if o.type == "MESH" and o.visible_get()]
        frame_camera(cam, objs)
    if p.bool("active", False):
        bpy.context.scene.camera = cam
    return {"name": cam.name}


@command("set_frame", history=False, changes_scene=True)
def cmd_set_frame(engine, p: Params):
    """Change l'image courante de la timeline (et la plage start/end facultative)."""
    sc = bpy.context.scene
    if p.has("start"):
        sc.frame_start = p.int("start", lo=0, hi=1_000_000)
    if p.has("end"):
        sc.frame_end = p.int("end", lo=sc.frame_start, hi=1_000_000)
    if p.has("fps"):
        sc.render.fps = p.int("fps", lo=1, hi=240)
    if p.has("frame"):
        sc.frame_set(p.int("frame", lo=-1_000_000, hi=1_000_000))
    return {"frame": sc.frame_current, "start": sc.frame_start, "end": sc.frame_end, "fps": sc.render.fps}


@command("undo", history=False, changes_scene=False)
def cmd_undo(engine, p: Params):
    """Annule la dernière opération."""
    return engine.undo()


@command("redo", history=False, changes_scene=False)
def cmd_redo(engine, p: Params):
    """Rétablit l'opération annulée."""
    return engine.redo()


@command("history_goto", history=False, changes_scene=False)
def cmd_history_goto(engine, p: Params):
    """Revient à une étape précise de l'historique."""
    return engine.goto_history(p.int("index", lo=0))


@command("get_history", history=False)
def cmd_get_history(engine, p: Params):
    """Historique des opérations (annuler/rétablir)."""
    return {"history": engine.history.as_list(), "log": engine.log[-100:]}
