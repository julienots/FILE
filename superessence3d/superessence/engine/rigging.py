"""Squelettes (armatures), rigging automatique et poids.

- Les modèles générés gardent leur graphe de squelette (propriété « se_skeleton ») :
  l'armature correspond exactement à la structure du corps (pattes, ailes, queue...).
- Pour un maillage quelconque, des gabarits (humanoïde, quadrupède, serpent, chaîne)
  sont ajustés à sa boîte englobante.
- Les poids sont calculés par Blender (« Automatic Weights » par diffusion de chaleur),
  avec repli sur les enveloppes si la diffusion échoue.
"""
from __future__ import annotations

import bpy
from mathutils import Vector

from .core import CommandError, Params, command
from .util import (ensure_object_mode, get_json_prop, get_object, hierarchy, override, select_only, set_json_prop,
                   world_bbox, link_to_collection, unique_name)

TEMPLATES = ["auto", "humanoid", "quadruped", "serpent", "chain", "winged_quadruped"]


def _template_bones(kind: str, lo: Vector, hi: Vector) -> list[dict]:
    """Os d'un gabarit, en coordonnées monde normalisées sur la boîte englobante (face vers -Y)."""
    size = hi - lo
    cx, cy = (lo.x + hi.x) / 2, (lo.y + hi.y) / 2

    def P(x, y, z):  # x,y,z en fractions de la boîte
        return [lo.x + x * size.x, lo.y + y * size.y, lo.z + z * size.z]

    b = []

    def add(name, head, tail, parent, role):
        b.append({"name": name, "head": head, "tail": tail, "parent": parent, "role": role})

    if kind == "humanoid":
        add("racine", P(.5, .5, 0), P(.5, .5, .08), None, "root")
        add("bassin", P(.5, .5, .5), P(.5, .5, .58), "racine", "spine")
        add("colonne", P(.5, .5, .58), P(.5, .5, .7), "bassin", "spine")
        add("torse", P(.5, .5, .7), P(.5, .5, .82), "colonne", "chest")
        add("cou", P(.5, .5, .82), P(.5, .5, .87), "torse", "neck")
        add("tête", P(.5, .5, .87), P(.5, .5, 1.0), "cou", "head")
        for s, x in (("G", .62), ("D", .38)):
            side = "L" if s == "G" else "R"
            sx = 1 if s == "G" else -1
            add(f"épaule.{s}", P(.5 + .04 * sx, .5, .8), P(x, .5, .8), "torse", f"shoulder_{side}")
            add(f"bras.{s}", P(x, .5, .8), P(.5 + .3 * sx, .5, .8), f"épaule.{s}", f"arm_{side}_upper")
            add(f"avant-bras.{s}", P(.5 + .3 * sx, .5, .8), P(.5 + .43 * sx, .5, .8), f"bras.{s}", f"arm_{side}_lower")
            add(f"main.{s}", P(.5 + .43 * sx, .5, .8), P(.5 + .5 * sx, .5, .8), f"avant-bras.{s}", f"arm_{side}_hand")
            add(f"cuisse.{s}", P(.5 + .08 * sx, .5, .5), P(.5 + .09 * sx, .5, .27), "bassin", f"leg_{side}_upper")
            add(f"tibia.{s}", P(.5 + .09 * sx, .5, .27), P(.5 + .09 * sx, .52, .04), f"cuisse.{s}", f"leg_{side}_lower")
            add(f"pied.{s}", P(.5 + .09 * sx, .52, .04), P(.5 + .09 * sx, .2, .01), f"tibia.{s}", f"leg_{side}_foot")
    elif kind in ("quadruped", "winged_quadruped"):
        add("racine", P(.5, .5, 0), P(.5, .5, .08), None, "root")
        add("bassin", P(.5, .72, .62), P(.5, .58, .64), "racine", "spine")
        add("colonne", P(.5, .58, .64), P(.5, .42, .66), "bassin", "spine")
        add("torse", P(.5, .42, .66), P(.5, .3, .68), "colonne", "chest")
        add("cou", P(.5, .3, .68), P(.5, .18, .82), "torse", "neck")
        add("tête", P(.5, .18, .82), P(.5, 0.0, .8), "cou", "head")
        add("queue.1", P(.5, .72, .62), P(.5, .82, .58), "bassin", "tail_1")
        add("queue.2", P(.5, .82, .58), P(.5, .92, .52), "queue.1", "tail_2")
        add("queue.3", P(.5, .92, .52), P(.5, 1.0, .46), "queue.2", "tail_3")
        for s, x in (("G", .7), ("D", .3)):
            side = "L" if s == "G" else "R"
            for fb, y, par in (("avant", .32, "torse"), ("arrière", .68, "bassin")):
                fbk = "front" if fb == "avant" else "back"
                add(f"patte.{fb}.{s}", P(x, y, .6), P(x, y + .02, .32), par, f"leg_{side}_{fbk}_upper")
                add(f"jarret.{fb}.{s}", P(x, y + .02, .32), P(x, y, .05), f"patte.{fb}.{s}", f"leg_{side}_{fbk}_lower")
                add(f"pied.{fb}.{s}", P(x, y, .05), P(x, y - .05, .0), f"jarret.{fb}.{s}", f"leg_{side}_{fbk}_foot")
            if kind == "winged_quadruped":
                sx = 1 if s == "G" else -1
                add(f"aile.{s}.1", P(.5 + .1 * sx, .38, .75), P(.5 + .3 * sx, .4, .85), "torse", f"wing_{side}_1")
                add(f"aile.{s}.2", P(.5 + .3 * sx, .4, .85), P(.5 + .5 * sx, .45, .8), f"aile.{s}.1", f"wing_{side}_2")
    elif kind == "serpent":
        n = 8
        add("racine", P(.5, .5, 0), P(.5, .5, .1), None, "root")
        prev = "racine"
        for i in range(n):
            y0, y1 = i / n, (i + 1) / n
            name = "tête" if i == 0 else f"corps.{i}"
            add(name, P(.5, y1, .5), P(.5, y0, .5), prev if i else "racine", "head" if i == 0 else f"tail_{i}")
            prev = name
    else:  # chain
        n = 4
        add("racine", P(.5, .5, 0), P(.5, .5, .25), None, "root")
        prev = "racine"
        for i in range(1, n):
            add(f"os.{i}", P(.5, .5, i / n), P(.5, .5, (i + 1) / n), prev, f"spine_{i}")
            prev = f"os.{i}"
    return b


def _guess_template(objs) -> str:
    bb = world_bbox(objs)
    if not bb:
        return "chain"
    s = bb[1] - bb[0]
    if s.z > 1.4 * max(s.x, s.y):
        return "humanoid"
    if s.y > 4 * max(s.x, s.z):
        return "serpent"
    if s.y > 1.1 * s.z:
        return "quadruped"
    return "chain"


def build_armature(name: str, bones: list[dict], location=None) -> bpy.types.Object:
    """Crée une armature Blender à partir d'une liste d'os {name, head, tail, parent, role} (coord. monde)."""
    arm_data = bpy.data.armatures.new(name)
    arm = bpy.data.objects.new(name, arm_data)
    link_to_collection(arm)
    arm.show_in_front = True
    arm_data.display_type = "OCTAHEDRAL"
    select_only([arm])
    bpy.ops.object.mode_set(mode="EDIT")
    ebones = {}
    try:
        for bd in bones:
            eb = arm_data.edit_bones.new(bd["name"])
            eb.head = Vector(bd["head"])
            eb.tail = Vector(bd["tail"])
            if (eb.tail - eb.head).length < 1e-4:
                eb.tail = eb.head + Vector((0, 0, 0.05))
            d = (eb.tail - eb.head).normalized()
            # convention de roulis : Z vers le haut pour les os horizontaux, vers l'avant (-Y) pour les os verticaux
            if abs(d.z) > 0.7:
                eb.align_roll(Vector((0, -1, 0)))
            else:
                eb.align_roll(Vector((0, 0, 1)))
            ebones[bd["name"]] = eb
        for bd in bones:
            par = bd.get("parent")
            if par and par in ebones:
                ebones[bd["name"]].parent = ebones[par]
                if (ebones[par].tail - ebones[bd["name"]].head).length < 1e-4:
                    ebones[bd["name"]].use_connect = True
    finally:
        bpy.ops.object.mode_set(mode="OBJECT")
    for bd in bones:
        pb = arm.pose.bones.get(bd["name"])
        if pb is not None:
            pb["se_role"] = bd.get("role", "")
            pb.rotation_mode = "XYZ"
    set_json_prop(arm, "se_bones", [{"name": b["name"], "role": b.get("role", "")} for b in bones])
    return arm


def bind_meshes(arm, meshes, rigid_parts: dict | None = None) -> dict:
    """Lie les maillages à l'armature : poids automatiques, ou parentage rigide à un os."""
    report = {"automatic": [], "envelope": [], "rigid": []}
    rigid_parts = rigid_parts or {}
    for me_obj in meshes:
        bone = rigid_parts.get(me_obj.name) or me_obj.get("se_bone")
        if bone and bone in arm.data.bones:
            mw = me_obj.matrix_world.copy()
            me_obj.parent = arm
            me_obj.parent_type = "BONE"
            me_obj.parent_bone = bone
            bpy.context.view_layer.update()
            me_obj.matrix_world = mw
            report["rigid"].append(f"{me_obj.name} → {bone}")
            continue
        for m in list(me_obj.modifiers):
            if m.type == "ARMATURE":
                me_obj.modifiers.remove(m)
        mw = me_obj.matrix_world.copy()
        select_only([me_obj, arm], active=arm)
        ok = False
        try:
            with override(arm, [me_obj, arm]):
                bpy.ops.object.parent_set(type="ARMATURE_AUTO")
            weighted = sum(1 for v in me_obj.data.vertices if len(v.groups))
            ok = weighted >= 0.9 * len(me_obj.data.vertices)
        except RuntimeError:
            ok = False
        if not ok:
            for m in list(me_obj.modifiers):
                if m.type == "ARMATURE":
                    me_obj.modifiers.remove(m)
            me_obj.vertex_groups.clear()
            with override(arm, [me_obj, arm]):
                bpy.ops.object.parent_set(type="ARMATURE_ENVELOPE")
            _nearest_bone_weights(me_obj, arm)
            report["envelope"].append(me_obj.name)
        else:
            report["automatic"].append(me_obj.name)
        me_obj.matrix_world = mw
    return report


def _nearest_bone_weights(obj, arm):
    """Repli robuste : chaque sommet suit l'os le plus proche (avec fondu vers le second)."""
    bones = [(b.name, arm.matrix_world @ b.head_local, arm.matrix_world @ b.tail_local) for b in arm.data.bones
             if b.use_deform]
    groups = {n: (obj.vertex_groups.get(n) or obj.vertex_groups.new(name=n)) for n, _, _ in bones}
    mw = obj.matrix_world
    for v in obj.data.vertices:
        p = mw @ v.co
        ds = []
        for n, h, t in bones:
            ab = t - h
            tt = max(0.0, min(1.0, (p - h).dot(ab) / max(ab.length_squared, 1e-9)))
            ds.append(((p - (h + ab * tt)).length, n))
        ds.sort()
        if not ds:
            continue
        d0, n0 = ds[0]
        if len(ds) > 1:
            d1, n1 = ds[1]
            w0 = d1 / max(d0 + d1, 1e-9)
            groups[n0].add([v.index], w0, "REPLACE")
            groups[n1].add([v.index], 1 - w0, "REPLACE")
        else:
            groups[n0].add([v.index], 1.0, "REPLACE")


def find_root(obj):
    while obj.parent is not None and obj.parent.type != "ARMATURE":
        obj = obj.parent
    return obj


@command("create_rig", label="Créer un squelette")
def cmd_create_rig(engine, p: Params):
    """Crée un squelette et lie le modèle (poids automatiques). template : auto (utilise la structure du modèle
    généré si disponible), humanoid, quadruped, winged_quadruped, serpent, chain. Ou `bones` personnalisés
    [{name, head, tail, parent, role}] en coordonnées monde."""
    ensure_object_mode()
    root = find_root(get_object(p.str("name")))
    parts = hierarchy(root)
    meshes = [o for o in parts if o.type == "MESH"]
    if not meshes:
        raise CommandError("Aucun maillage à lier.")
    template = p.str("template", "auto", choices=TEMPLATES)
    if p.has("bones"):
        bones = p.list("bones")
        for b in bones:
            if not isinstance(b, dict) or not {"name", "head", "tail"} <= set(b):
                raise CommandError("Chaque os doit avoir name, head et tail.")
        source = "personnalisé"
    else:
        skel = get_json_prop(root, "se_skeleton")
        if template == "auto" and skel:
            mw = root.matrix_world
            bones = [{**b, "head": list(mw @ Vector(b["head"])), "tail": list(mw @ Vector(b["tail"]))} for b in skel]
            source = "structure du modèle généré"
        else:
            if template == "auto":
                template = _guess_template(meshes)
            bb = world_bbox(meshes)
            bones = _template_bones(template, bb[0], bb[1])
            source = f"gabarit {template}"
    # supprime une ancienne armature du même modèle
    old = root.get("se_armature")
    if old and old in bpy.data.objects and p.bool("replace", True):
        old_arm = bpy.data.objects[old]
        for o in meshes:
            if o.parent == old_arm:
                mw = o.matrix_world.copy()
                o.parent = None
                o.matrix_world = mw
            for m in list(o.modifiers):
                if m.type == "ARMATURE":
                    o.modifiers.remove(m)
            o.vertex_groups.clear()
        bpy.data.objects.remove(old_arm, do_unlink=True)
    arm = build_armature(unique_name(f"{root.name}_Squelette"), bones)
    rigid = {}
    report = bind_meshes(arm, meshes, rigid)
    root["se_armature"] = arm.name
    # l'armature suit la racine si celle-ci n'est pas un maillage déformé
    return {"armature": arm.name, "bones": len(arm.data.bones), "source": source, "binding": report,
            "_label": f"Squelette {root.name}"}


@command("bind_to_armature", label="Lier à l'armature")
def cmd_bind(engine, p: Params):
    """Lie des maillages à une armature existante (poids automatiques), ou rigidement à un os (`bone`)."""
    arm = get_object(p.str("armature"))
    if arm.type != "ARMATURE":
        raise CommandError("Ce n'est pas une armature.")
    meshes = [get_object(n) for n in p.list("objects")]
    rigid = {o.name: p.str("bone") for o in meshes} if p.has("bone") else {}
    return {"binding": bind_meshes(arm, [m for m in meshes if m.type == "MESH"], rigid)}


@command("pose_bone", label="Poser un os")
def cmd_pose_bone(engine, p: Params):
    """Tourne/déplace/échelonne un os en pose (degrés). keyframe=true insère une clé à l'image courante."""
    import math
    arm = get_object(p.str("armature"))
    pb = arm.pose.bones.get(p.str("bone"))
    if pb is None:
        raise CommandError("Os introuvable.")
    pb.rotation_mode = "XYZ"
    if p.has("rotation"):
        pb.rotation_euler = [math.radians(x) for x in p.vec("rotation")]
    if p.has("location"):
        pb.location = p.vec("location")
    if p.has("scale"):
        pb.scale = p.vec("scale")
    if p.bool("keyframe", False):
        f = bpy.context.scene.frame_current
        for path in ("rotation_euler", "location", "scale"):
            pb.keyframe_insert(path, frame=f)
    if p.bool("reset", False):
        pb.rotation_euler = (0, 0, 0)
        pb.location = (0, 0, 0)
        pb.scale = (1, 1, 1)
    return {"bone": pb.name}
