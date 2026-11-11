"""Dépliage UV (vrais opérateurs Blender)."""
from __future__ import annotations

import math

import bpy

from .core import CommandError, Params, command
from .util import ensure_object_mode, select_only, target_objects

METHODS = ["smart", "unwrap", "cube", "cylinder", "sphere", "lightmap"]


def unwrap(obj, method: str = "smart", angle: float = 66.0, margin: float = 0.003) -> None:
    ensure_object_mode()
    select_only([obj])
    if not obj.data.uv_layers:
        obj.data.uv_layers.new(name="UVMap")
    bpy.ops.object.mode_set(mode="EDIT")
    try:
        bpy.ops.mesh.select_all(action="SELECT")
        if method == "smart":
            bpy.ops.uv.smart_project(angle_limit=math.radians(angle), island_margin=margin)
        elif method == "unwrap":
            # coutures automatiques si aucune n'existe, puis dépliage conforme
            bpy.ops.uv.smart_project(angle_limit=math.radians(angle), island_margin=margin)
            bpy.ops.uv.unwrap(method="CONFORMAL", margin=margin)
        elif method == "cube":
            bpy.ops.uv.cube_project(cube_size=max(obj.dimensions) or 1.0)
        elif method == "cylinder":
            bpy.ops.uv.cylinder_project()
        elif method == "sphere":
            bpy.ops.uv.sphere_project()
        elif method == "lightmap":
            bpy.ops.uv.lightmap_pack(PREF_MARGIN_DIV=0.2)
        else:
            raise CommandError(f"Méthode UV inconnue : {method}")
        try:
            bpy.ops.uv.pack_islands(margin=margin)
        except RuntimeError:
            pass
    finally:
        bpy.ops.object.mode_set(mode="OBJECT")


@command("create_uv_map", label="Déplier les UV")
def cmd_uv(engine, p: Params):
    """Crée/recalcule la carte UV : smart (projection intelligente), unwrap, cube, cylinder, sphere, lightmap."""
    objs = [o for o in target_objects(p) if o.type == "MESH"]
    if not objs:
        raise CommandError("Aucun maillage visé.")
    method = p.str("method", "smart", choices=METHODS)
    out = []
    for o in objs:
        unwrap(o, method, p.float("angle", 66.0, lo=1, hi=89), p.float("margin", 0.003, lo=0, hi=0.5))
        uv = o.data.uv_layers.active
        out.append({"name": o.name, "uv_map": uv.name if uv else None, "loops": len(o.data.loops)})
    return {"results": out, "method": method, "_label": f"UV {method}"}
