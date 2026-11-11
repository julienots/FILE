"""Fonctions utilitaires partagées par les modules du moteur (bpy)."""
from __future__ import annotations

import json
import math
from typing import Iterable

import bpy
from mathutils import Matrix, Vector

from .core import CommandError, Params


def get_object(name: str) -> bpy.types.Object:
    obj = bpy.data.objects.get(name)
    if obj is None:
        raise CommandError(f"Objet introuvable : « {name} ».")
    return obj


def get_mesh_object(name: str) -> bpy.types.Object:
    obj = get_object(name)
    if obj.type != "MESH":
        raise CommandError(f"« {name} » n'est pas un maillage (type : {obj.type}).")
    return obj


def target_objects(p: Params, key: str = "name", allow_selection: bool = True) -> list[bpy.types.Object]:
    """Objets visés : paramètre `name` (ou `names`), sinon la sélection courante."""
    names = []
    if p.has("names"):
        names = [str(n) for n in p.list("names")]
    elif p.has(key):
        names = [p.str(key)]
    if names:
        return [get_object(n) for n in names]
    if allow_selection:
        sel = [o for o in bpy.context.view_layer.objects if o.select_get()]
        if sel:
            return sel
        act = bpy.context.view_layer.objects.active
        if act:
            return [act]
    raise CommandError("Aucun objet indiqué ni sélectionné.")


def scene() -> bpy.types.Scene:
    return bpy.context.scene


def link_to_collection(obj: bpy.types.Object, collection: bpy.types.Collection | None = None) -> None:
    col = collection or bpy.context.collection or scene().collection
    if obj.name not in col.objects:
        col.objects.link(obj)


def unique_name(base: str) -> str:
    if base not in bpy.data.objects:
        return base
    i = 1
    while f"{base}.{i:03d}" in bpy.data.objects:
        i += 1
    return f"{base}.{i:03d}"


def select_only(objs: Iterable[bpy.types.Object], active: bpy.types.Object | None = None) -> None:
    objs = list(objs)
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    for o in objs:
        if o.name in bpy.context.view_layer.objects:
            o.select_set(True)
    if active is None and objs:
        active = objs[-1]
    if active is not None and active.name in bpy.context.view_layer.objects:
        bpy.context.view_layer.objects.active = active


def override(obj: bpy.types.Object | None = None, objs: list | None = None):
    """Contexte pour les opérateurs bpy.ops qui exigent un objet actif."""
    objs = objs if objs is not None else ([obj] if obj else [])
    kw = dict(selected_objects=objs, selected_editable_objects=objs)
    if obj is not None:
        kw.update(object=obj, active_object=obj)
    return bpy.context.temp_override(**kw)


def ensure_object_mode() -> None:
    o = bpy.context.view_layer.objects.active
    if o is not None and o.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")


def apply_modifiers(obj: bpy.types.Object, only: list[str] | None = None) -> int:
    """Applique les modificateurs (tous, ou ceux nommés). Renvoie le nombre appliqué."""
    ensure_object_mode()
    n = 0
    for m in list(obj.modifiers):
        if only is not None and m.name not in only:
            continue
        if not m.show_viewport:
            obj.modifiers.remove(m)
            continue
        with override(obj):
            try:
                bpy.ops.object.modifier_apply(modifier=m.name)
                n += 1
            except RuntimeError:
                # un modificateur sans effet (ex. booléen sans cible) : on le retire
                obj.modifiers.remove(m)
    return n


def world_bbox(objs: Iterable[bpy.types.Object]) -> tuple[Vector, Vector] | None:
    lo = Vector((math.inf,) * 3)
    hi = Vector((-math.inf,) * 3)
    found = False
    for o in objs:
        if o.type not in {"MESH", "CURVE", "SURFACE", "META", "FONT", "ARMATURE"}:
            continue
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            lo = Vector(map(min, lo, w))
            hi = Vector(map(max, hi, w))
            found = True
    return (lo, hi) if found else None


def hierarchy(root: bpy.types.Object) -> list[bpy.types.Object]:
    out = [root]
    for c in root.children:
        out.extend(hierarchy(c))
    return out


def set_json_prop(id_data, key: str, value) -> None:
    id_data[key] = json.dumps(value, ensure_ascii=False)


def get_json_prop(id_data, key: str, default=None):
    v = id_data.get(key)
    if v is None:
        return default
    try:
        return json.loads(v)
    except (TypeError, ValueError):
        return default


def vec(v) -> list[float]:
    return [round(float(x), 6) for x in v]


def matrix_list(m: Matrix) -> list[float]:
    # ordre colonne-majeur (compatible three.js Matrix4.fromArray)
    return [round(m[r][c], 7) for c in range(4) for r in range(4)]


def rna_value(v):
    """Convertit une valeur RNA en JSON."""
    if isinstance(v, (bool, int, float, str)) or v is None:
        return v
    if hasattr(v, "name") and hasattr(v, "bl_rna"):
        return v.name
    try:
        return [rna_value(x) for x in v]
    except TypeError:
        return str(v)


def rna_properties(struct, skip: set[str] | None = None) -> list[dict]:
    """Introspection RNA : décrit les propriétés modifiables d'une structure Blender.

    Utilisé pour générer automatiquement les panneaux des modificateurs et des nœuds.
    """
    skip = skip or set()
    out = []
    for prop in struct.bl_rna.properties:
        pid = prop.identifier
        if pid in {"rna_type", "name", "type", "is_override_data", "persistent_uid"} or pid in skip:
            continue
        if prop.is_readonly and prop.type != "POINTER":
            continue
        if prop.type == "COLLECTION":
            continue
        d = {"id": pid, "label": prop.name or pid, "type": prop.type, "description": prop.description}
        try:
            val = getattr(struct, pid)
        except AttributeError:
            continue
        if prop.type == "POINTER":
            if prop.is_readonly or prop.fixed_type is None:
                continue
            ftype = prop.fixed_type.identifier
            if ftype not in {"Object", "Collection", "Texture", "Material", "Image"}:
                continue
            d["pointer_type"] = ftype
            d["value"] = val.name if val else None
        elif prop.type == "ENUM":
            if prop.is_enum_flag:
                continue
            d["items"] = [[i.identifier, i.name] for i in prop.enum_items]
            d["value"] = val
        elif prop.type in {"INT", "FLOAT"}:
            d["array"] = prop.array_length
            d["value"] = rna_value(val)
            d["min"] = prop.soft_min if prop.soft_min > -1e30 else None
            d["max"] = prop.soft_max if prop.soft_max < 1e30 else None
            if prop.type == "FLOAT":
                d["subtype"] = prop.subtype
                d["step"] = prop.step
        elif prop.type == "BOOLEAN":
            if prop.array_length:
                d["array"] = prop.array_length
            d["value"] = rna_value(val)
        elif prop.type == "STRING":
            d["value"] = val
        else:
            continue
        out.append(d)
    return out


def rna_set(struct, pid: str, value) -> None:
    prop = struct.bl_rna.properties.get(pid)
    if prop is None:
        raise CommandError(f"Propriété inconnue : « {pid} ».")
    if prop.is_readonly:
        raise CommandError(f"Propriété en lecture seule : « {pid} ».")
    try:
        if prop.type == "POINTER":
            ftype = prop.fixed_type.identifier
            coll = {"Object": bpy.data.objects, "Collection": bpy.data.collections, "Texture": bpy.data.textures,
                    "Material": bpy.data.materials, "Image": bpy.data.images}.get(ftype)
            if coll is None:
                raise CommandError(f"Type de référence non pris en charge : {ftype}.")
            if value in (None, ""):
                setattr(struct, pid, None)
            else:
                target = coll.get(value)
                if target is None:
                    raise CommandError(f"{ftype} introuvable : « {value} ».")
                setattr(struct, pid, target)
        elif prop.type == "ENUM":
            setattr(struct, pid, str(value))
        elif prop.type == "BOOLEAN":
            setattr(struct, pid, value if isinstance(value, list) else bool(value))
        elif prop.type == "INT":
            setattr(struct, pid, [int(v) for v in value] if isinstance(value, list) else int(value))
        elif prop.type == "FLOAT":
            setattr(struct, pid, [float(v) for v in value] if isinstance(value, list) else float(value))
        else:
            setattr(struct, pid, value)
    except (TypeError, ValueError) as e:
        raise CommandError(f"Valeur invalide pour « {pid} » : {e}") from e
