"""Transfert des maillages Blender vers le viewport (tableaux binaires base64).

Le viewport affiche exactement la géométrie évaluée par Blender (modificateurs,
déformations d'armature, image courante). En mode édition, il reçoit le maillage
de base avec les indices de faces/sommets Blender, pour que la sélection désigne
les vrais éléments du maillage.
"""
from __future__ import annotations

import base64
import hashlib

import bpy
import numpy as np

from .colors import linear_to_srgb
from .core import CommandError, Params, command
from .util import get_object, matrix_list


def _b64(a: np.ndarray) -> str:
    return base64.b64encode(np.ascontiguousarray(a).tobytes()).decode("ascii")


def material_preview(mat) -> dict:
    """Paramètres d'affichage approximatifs pour le viewport (le rendu exact est celui de Blender)."""
    d = {"name": mat.name if mat else "(aucun)", "color": [0.8, 0.8, 0.8], "metallic": 0.0, "roughness": 0.5,
         "emission": [0, 0, 0], "emission_strength": 0.0, "alpha": 1.0, "texture": False}
    if mat is None:
        return d
    prev = mat.get("se_preview")
    bsdf = None
    if mat.node_tree:
        bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
    if bsdf is not None:
        bc = bsdf.inputs["Base Color"]
        if not bc.is_linked:
            d["color"] = [linear_to_srgb(c) for c in bc.default_value[:3]]
        else:
            src = bc.links[0].from_node
            if src.type == "TEX_IMAGE" and src.image is not None:
                d["texture"] = True
        d["metallic"] = float(bsdf.inputs["Metallic"].default_value)
        d["roughness"] = float(bsdf.inputs["Roughness"].default_value)
        d["alpha"] = float(bsdf.inputs["Alpha"].default_value)
        ec = bsdf.inputs.get("Emission Color")
        es = bsdf.inputs.get("Emission Strength")
        if ec is not None and es is not None:
            d["emission"] = [linear_to_srgb(c) for c in ec.default_value[:3]]
            d["emission_strength"] = float(es.default_value)
    else:
        d["color"] = list(mat.diffuse_color[:3])
    if prev:
        try:
            for k in ("color", "metallic", "roughness", "emission", "emission_strength"):
                if k in prev:
                    d[k] = prev[k] if not hasattr(prev[k], "to_list") else prev[k].to_list()
        except Exception:
            pass
    d["color"] = [float(x) for x in d["color"]]
    d["emission"] = [float(x) for x in d["emission"]]
    return d


def mesh_signature(obj) -> str:
    """Empreinte rapide : change si la géométrie, les modificateurs, matériaux ou l'image changent."""
    h = hashlib.blake2b(digest_size=12)
    me = obj.data
    n = len(me.vertices)
    co = np.empty(n * 3, dtype=np.float32)
    me.vertices.foreach_get("co", co)
    h.update(co.tobytes())
    h.update(str((len(me.polygons), len(me.edges), len(me.loops))).encode())
    for m in obj.modifiers:
        h.update(m.type.encode())
        h.update(str(m.show_viewport).encode())
        for prop in m.bl_rna.properties:
            if prop.type in {"INT", "FLOAT", "BOOLEAN", "ENUM", "POINTER"} and not prop.is_readonly:
                try:
                    v = getattr(m, prop.identifier)
                    h.update(str(v.name if hasattr(v, "name") else (tuple(v) if hasattr(v, "__len__") and not isinstance(v, str) else v)).encode())
                except Exception:
                    pass
    for mat in me.materials:
        h.update(str(material_preview(mat)).encode())
    ca = me.color_attributes.active_color
    if ca is not None and len(ca.data):
        arr = np.empty(len(ca.data) * 4, dtype=np.float32)
        ca.data.foreach_get("color", arr)
        h.update(arr.tobytes())
    h.update(str(len(me.uv_layers)).encode())
    if me.uv_layers.active is not None and len(me.loops):
        uv = np.empty(len(me.loops) * 2, dtype=np.float32)
        me.uv_layers.active.data.foreach_get("uv", uv)
        h.update(uv.tobytes())
    deforming = obj.find_armature() is not None or (obj.animation_data is not None) or any(
        m.type in {"ARMATURE", "HOOK"} for m in obj.modifiers)
    if deforming:
        h.update(str(bpy.context.scene.frame_current).encode())
        arm = obj.find_armature()
        if arm is not None:
            for pb in arm.pose.bones:
                h.update(np.array(pb.matrix, dtype=np.float32).tobytes())
    return h.hexdigest()


def mesh_payload(obj, evaluated: bool = True, weights_group: str | None = None) -> dict:
    if obj.type != "MESH":
        raise CommandError(f"« {obj.name} » n'est pas un maillage.")
    if evaluated:
        dg = bpy.context.evaluated_depsgraph_get()
        eo = obj.evaluated_get(dg)
        me = eo.to_mesh()
    else:
        eo = None
        me = obj.data
    try:
        me.calc_loop_triangles()
        nt = len(me.loop_triangles)
        nv = len(me.vertices)
        if nt > 1_500_000:
            raise CommandError(f"Maillage trop lourd pour l'affichage ({nt} triangles).")
        co = np.empty(nv * 3, dtype=np.float32)
        me.vertices.foreach_get("co", co)
        co = co.reshape(-1, 3)
        tri_loops = np.empty(nt * 3, dtype=np.int32)
        me.loop_triangles.foreach_get("loops", tri_loops)
        tri_verts = np.empty(nt * 3, dtype=np.int32)
        me.loop_triangles.foreach_get("vertices", tri_verts)
        tri_poly = np.empty(nt, dtype=np.int32)
        me.loop_triangles.foreach_get("polygon_index", tri_poly)
        tri_mat = np.empty(nt, dtype=np.int32)
        me.loop_triangles.foreach_get("material_index", tri_mat)
        positions = co[tri_verts]
        # normales par coin (respecte lissage/arêtes vives)
        cn = me.corner_normals
        nl = len(cn)
        norms = np.empty(nl * 3, dtype=np.float32)
        cn.foreach_get("vector", norms)
        normals = norms.reshape(-1, 3)[tri_loops]
        out = {
            "name": obj.name,
            "evaluated": evaluated,
            "triangles": nt,
            "vertices": nv,
            "faces": len(me.polygons),
            "positions": _b64(positions.astype(np.float32)),
            "normals": _b64(normals.astype(np.float32)),
            "tri_poly": _b64(tri_poly.astype(np.uint32)),
            "tri_verts": _b64(tri_verts.astype(np.uint32)),
        }
        # groupes de matériaux : on trie les triangles par matériau
        order = np.argsort(tri_mat, kind="stable")
        if not np.all(order == np.arange(nt)):
            out["order"] = _b64(order.astype(np.uint32))
        groups = []
        mats_sorted = tri_mat[order]
        if nt:
            starts = np.flatnonzero(np.r_[True, mats_sorted[1:] != mats_sorted[:-1]])
            for i, s in enumerate(starts):
                e = starts[i + 1] if i + 1 < len(starts) else nt
                groups.append({"start": int(s) * 3, "count": int(e - s) * 3, "material": int(mats_sorted[s])})
        out["groups"] = groups
        out["materials"] = [material_preview(m) for m in obj.data.materials] or [material_preview(None)]
        # UV
        if me.uv_layers.active is not None:
            uv = np.empty(len(me.loops) * 2, dtype=np.float32)
            me.uv_layers.active.data.foreach_get("uv", uv)
            out["uvs"] = _b64(uv.reshape(-1, 2)[tri_loops].astype(np.float32))
        # couleurs de sommets / poids
        colors = None
        if weights_group is not None:
            colors = _weight_colors(obj, me, weights_group, tri_verts)
        else:
            ca = me.color_attributes.active_color
            if ca is not None and len(ca.data):
                arr = np.empty(len(ca.data) * 4, dtype=np.float32)
                ca.data.foreach_get("color", arr)
                arr = arr.reshape(-1, 4)[:, :3]
                if ca.domain == "POINT":
                    c = arr[tri_verts]
                else:
                    c = arr[tri_loops]
                # sRGB pour l'affichage
                colors = np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(np.clip(c, 0, 1), 1 / 2.4) - 0.055)
        if colors is not None:
            out["colors"] = _b64(colors.astype(np.float32))
        if not evaluated:
            # données d'édition : sommets et arêtes du maillage de base
            ne = len(me.edges)
            ev = np.empty(ne * 2, dtype=np.int32)
            me.edges.foreach_get("vertices", ev)
            out["edit"] = {"co": _b64(co.astype(np.float32)), "edges": _b64(ev.astype(np.uint32))}
        out["signature"] = mesh_signature(obj)
        out["matrix_world"] = matrix_list(obj.matrix_world)
        return out
    finally:
        if eo is not None:
            eo.to_mesh_clear()


def _weight_colors(obj, me, group_name, tri_verts):
    g = obj.vertex_groups.get(group_name)
    w = np.zeros(len(me.vertices), dtype=np.float32)
    if g is not None:
        gi = g.index
        for v in me.vertices:
            for ge in v.groups:
                if ge.group == gi:
                    w[v.index] = ge.weight
    # rampe bleu -> vert -> rouge (comme Blender)
    c = np.zeros((len(w), 3), dtype=np.float32)
    c[:, 0] = np.clip(2 * w - 1, 0, 1)
    c[:, 1] = 1 - np.abs(2 * w - 1)
    c[:, 2] = np.clip(1 - 2 * w, 0, 1)
    return c[tri_verts]


@command("get_mesh", history=False)
def cmd_get_mesh(engine, p: Params):
    """Données de maillage pour le viewport (évaluées, ou maillage de base pour l'édition)."""
    obj = get_object(p.str("name"))
    evaluated = p.bool("evaluated", True)
    known = p.str("signature", "")
    weights = p.str("weights", None)
    if known and known == mesh_signature(obj) and weights is None:
        return {"name": obj.name, "unchanged": True, "matrix_world": matrix_list(obj.matrix_world)}
    return mesh_payload(obj, evaluated=evaluated, weights_group=weights)


@command("get_viewport_state", history=False)
def cmd_viewport_state(engine, p: Params):
    """État léger pour le viewport : objets visibles, matrices, signatures."""
    out = []
    for o in bpy.context.scene.objects:
        if not o.visible_get():
            continue
        d = {"name": o.name, "type": o.type, "matrix_world": matrix_list(o.matrix_world),
             "selected": o.select_get(), "parent": o.parent.name if o.parent else None}
        if o.type == "MESH":
            d["signature"] = mesh_signature(o)
        elif o.type == "LIGHT":
            d["light"] = {"type": o.data.type, "energy": o.data.energy, "color": list(o.data.color)}
        elif o.type == "CAMERA":
            d["camera"] = {"lens": o.data.lens, "active": bpy.context.scene.camera == o,
                           "fov": o.data.angle}
        elif o.type == "ARMATURE":
            bones = []
            for b in o.pose.bones:
                h = o.matrix_world @ b.head
                t = o.matrix_world @ b.tail
                bones.append({"name": b.name, "head": [h.x, h.y, h.z], "tail": [t.x, t.y, t.z]})
            d["bones"] = bones
        out.append(d)
    act = bpy.context.view_layer.objects.active
    sc = bpy.context.scene
    return {"version": engine.version, "objects": out, "active": act.name if act else None,
            "frame": sc.frame_current, "frame_start": sc.frame_start, "frame_end": sc.frame_end,
            "fps": sc.render.fps}
