"""Post-traitement des modèles générés : chaque paramètre du panneau de génération est relié
ici à une vraie opération Blender (échelle, lissage, subdivision, remaillage, décimation,
normales, nettoyage, trous, UV, cuisson PBR, collisions, LOD, préparation moteur de jeu)."""
from __future__ import annotations

import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

from ..util import apply_modifiers, get_json_prop, hierarchy, override, select_only, set_json_prop, world_bbox


def fit_height(roots, height: float) -> float:
    """Met à l'échelle pour une hauteur cible (m) et pose le modèle au sol (z=0 local)."""
    objs = []
    for r in roots:
        objs.extend(hierarchy(r))
    bpy.context.view_layer.update()
    bb = world_bbox(objs)
    if not bb:
        return 0.0
    cur = bb[1].z - bb[0].z
    if cur < 1e-6:
        return 0.0
    for r in roots:
        f = height / cur if height else 1.0
        r.scale = r.scale * f
    bpy.context.view_layer.update()
    bb = world_bbox(objs)
    for r in roots:
        dz = bb[0].z - r.location.z
        if abs(dz) > 1e-6:
            # décale les enfants pour que le bas du modèle soit à l'origine de la racine
            s = r.scale.z if r.scale.z else 1.0
            for c in r.children:
                c.location.z -= dz / s
            skel = get_json_prop(r, "se_skeleton")
            if skel:
                for b in skel:
                    b["head"][2] -= dz / s
                    b["tail"][2] -= dz / s
                set_json_prop(r, "se_skeleton", skel)
    bpy.context.view_layer.update()
    bb = world_bbox(objs)
    return round(bb[1].z - bb[0].z, 4)


def _meshes(m):
    return [o for o in m.parts if o.type == "MESH" and o.name in bpy.data.objects]


def run(engine, m, params: dict, height: float | None) -> dict:
    rep: dict = {"applied": []}
    meshes = _meshes(m)
    # -- matériaux --------------------------------------------------------
    if not params.get("generate_materials", True):
        gray = bpy.data.materials.new(f"{m.root.name}_gris")
        if gray.node_tree is None:
            gray.use_nodes = True
        for o in meshes:
            o.data.materials.clear()
            o.data.materials.append(gray)
        rep["applied"].append("matériaux désactivés (gris uniforme)")
    # -- symétrie : sans symétrie, légère variation organique -------------
    if not params.get("symmetry", True):
        rng = random.Random(int(m.spec.get("seed", 1)))
        for o in meshes:
            if o.get("se_part") in ("corps",):
                amp = max(o.dimensions) * 0.012
                for v in o.data.vertices:
                    if v.co.x > 0:
                        v.co += Vector((rng.uniform(-amp, amp), rng.uniform(-amp, amp), rng.uniform(-amp, amp)))
        rep["applied"].append("asymétrie organique")
    # -- lissage ------------------------------------------------------------
    for o in meshes:
        if params.get("smoothing", True):
            if not any(mod.type == "BEVEL" for mod in o.modifiers) and o.get("se_part") not in (
                    "murs", "remparts", "caisse", "planches", "toit", "toits", "lame", "fer"):
                o.data.shade_smooth()
            else:
                o.data.set_sharp_from_angle(angle=math.radians(35))
        else:
            o.data.shade_flat()
            for mod in list(o.modifiers):
                if mod.type == "SUBSURF":
                    o.modifiers.remove(mod)
    rep["applied"].append("lissage" if params.get("smoothing", True) else "ombrage plat")
    # -- remaillage (résolution / densité) --------------------------------
    if params.get("remesh") or params.get("resolution"):
        vs = float(params.get("resolution") or 0.02)
        for o in meshes:
            if o.get("se_part") == "corps":
                apply_modifiers(o)
                mod = o.modifiers.new("Remaillage", "REMESH")
                mod.mode = "VOXEL"
                mod.voxel_size = max(vs * max(o.dimensions), 0.002)
                mod.use_smooth_shade = True
                apply_modifiers(o, only=[mod.name])
        rep["applied"].append(f"remaillage voxel (taille relative {vs})")
    # -- échelle -----------------------------------------------------------
    if height:
        rep["height"] = fit_height([m.root], float(height))
        rep["applied"].append(f"échelle : {rep['height']} m de haut")
    # -- application des modificateurs (nécessaire pour décimation / jeu) -
    target = int(params.get("target_polycount") or 0)
    game = bool(params.get("game_ready"))
    if params.get("apply_modifiers") or target or game:
        for o in meshes:
            apply_modifiers(o)
        rep["applied"].append("modificateurs appliqués")
    # -- nettoyage, normales, trous ---------------------------------------
    if params.get("cleanup", True) or params.get("auto_optimize", True) or params.get("fix_normals", True) or \
            params.get("fill_holes"):
        merged = 0
        for o in meshes:
            if any(mod.type in ("SKIN",) for mod in o.modifiers):
                continue
            bm = bmesh.new()
            bm.from_mesh(o.data)
            if params.get("cleanup", True) or params.get("auto_optimize", True):
                n0 = len(bm.verts)
                bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=max(o.dimensions) * 1e-5 + 1e-6)
                merged += n0 - len(bm.verts)
                loose = [v for v in bm.verts if not v.link_faces]
                if loose:
                    bmesh.ops.delete(bm, geom=loose, context="VERTS")
            if params.get("fill_holes"):
                bmesh.ops.holes_fill(bm, edges=bm.edges, sides=64)
            if params.get("fix_normals", True):
                bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
            bm.to_mesh(o.data)
            bm.free()
            o.data.update()
        rep["applied"].append(f"nettoyage ({merged} sommets fusionnés), normales recalculées"
                              + (", trous fermés" if params.get("fill_holes") else ""))
    # -- réduction polygonale ---------------------------------------------
    if target:
        total = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in meshes)
        if total > target:
            ratio = max(0.01, target / total)
            for o in meshes:
                mod = o.modifiers.new("Décimation", "DECIMATE")
                mod.ratio = ratio
                apply_modifiers(o, only=[mod.name])
            after = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in meshes)
            rep["applied"].append(f"réduction polygonale {total} → {after} triangles (cible {target})")
            rep["triangles"] = after
        else:
            rep["applied"].append(f"réduction inutile ({total} ≤ {target} triangles)")
    # -- UV ------------------------------------------------------------------
    tex_res = int(params.get("texture_resolution") or 0)
    if params.get("generate_uv", True) or tex_res:
        from ..uv import unwrap
        n = 0
        for o in meshes:
            if len(o.data.polygons) == 0:
                continue
            try:
                unwrap(o, "smart", 66.0, 0.004)
                n += 1
            except RuntimeError:
                pass
        rep["applied"].append(f"UV générées ({n} maillages)")
    # -- cuisson des textures PBR ------------------------------------------
    if tex_res:
        from ..materials import bake_pbr
        baked = []
        maps = params.get("texture_maps") or ["base_color", "roughness", "normal"]
        targets = [o for o in meshes if o.data.materials and len(o.data.polygons) > 0]
        for i, o in enumerate(targets):
            engine.progress(f"Cuisson PBR {tex_res}px : {o.name} ({i + 1}/{len(targets)})", 0.6 + 0.35 * i / max(1, len(targets)))
            try:
                r = bake_pbr(engine, o, tex_res, maps, samples=8 if tex_res <= 1024 else 16)
                baked.append({"object": o.name, "maps": list(r["maps"]), "seconds": r["seconds"]})
            except Exception as e:  # une pièce qui échoue n'annule pas tout le modèle
                baked.append({"object": o.name, "error": str(e)})
        rep["baked"] = baked
        rep["applied"].append(f"textures PBR cuites en {tex_res}px ({len(baked)} maillages)")
    # -- collisions ------------------------------------------------------------
    if params.get("collision"):
        cols = []
        for o in meshes:
            if len(o.data.vertices) < 4:
                continue
            bm = bmesh.new()
            dg = bpy.context.evaluated_depsgraph_get()
            em = o.evaluated_get(dg).to_mesh()
            bm.from_mesh(em)
            o.evaluated_get(dg).to_mesh_clear()
            res = bmesh.ops.convex_hull(bm, input=bm.verts)
            bmesh.ops.delete(bm, geom=[g for g in res["geom_interior"] + res["geom_unused"] if isinstance(g, bmesh.types.BMVert)],
                             context="VERTS")
            me = bpy.data.meshes.new(f"UCX_{o.name}_00")
            bm.to_mesh(me)
            bm.free()
            col = bpy.data.objects.new(f"UCX_{o.name}_00", me)
            m.collection.objects.link(col)
            col.parent = o.parent
            col.matrix_world = o.matrix_world.copy()
            col.display_type = "WIRE"
            col.hide_render = True
            col["se_collision"] = True
            cols.append(col.name)
        rep["collision"] = cols
        rep["applied"].append(f"{len(cols)} enveloppes de collision convexes (UCX_ : Unreal/Unity ; Godot les reconnaît "
                              "avec le suffixe -colonly, ajouté à l'export Godot)")
    # -- préparation moteur de jeu --------------------------------------------
    if game:
        for o in meshes:
            mw = o.matrix_world.copy()
            o.data.transform(Matrix.Diagonal((*o.matrix_world.to_scale(), 1.0)))
            loc, rot, _ = mw.decompose()
            o.matrix_world = Matrix.LocRotScale(loc, rot, Vector((1, 1, 1)))
        rep["applied"].append("échelles appliquées aux maillages, origine du modèle au sol")
        lods = int(params.get("lods") or 0)
        lod_names = []
        for level in range(1, lods + 1):
            for o in meshes:
                c = o.copy()
                c.data = o.data.copy()
                m.collection.objects.link(c)
                c.name = f"{o.name}_LOD{level}"
                c.parent = o.parent
                c.matrix_world = o.matrix_world.copy()
                mod = c.modifiers.new("LOD", "DECIMATE")
                mod.ratio = 0.5 ** level
                apply_modifiers(c, only=[mod.name])
                c.hide_set(True)
                c.hide_render = True
                lod_names.append(c.name)
        if lods:
            rep["lods"] = lod_names
            rep["applied"].append(f"{lods} niveaux de détail (LOD) créés (masqués)")
    bpy.context.view_layer.update()
    return rep
