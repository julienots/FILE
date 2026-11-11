"""Import/export (exportateurs officiels de Blender) avec profils moteurs de jeu et vérification des fichiers."""
from __future__ import annotations

import json
import os
import struct
import time
from pathlib import Path

import bpy

from .core import CommandError, Params, command
from .util import apply_modifiers, ensure_object_mode, get_object, hierarchy, select_only, target_objects

FORMATS = ["glb", "gltf", "obj", "fbx", "stl", "ply", "blend"]

PROFILES = {
    "unity": {"format": "fbx", "label": "Unity", "notes": "FBX, échelle appliquée (FBX_SCALE_ALL), axes -Z avant / Y haut, modificateurs appliqués, animations incluses."},
    "unreal": {"format": "fbx", "label": "Unreal Engine", "notes": "FBX, échelle unités (1 m = 100 cm géré par Unreal), lissage par faces, animations incluses, collisions UCX_ si présentes."},
    "godot": {"format": "glb", "label": "Godot", "notes": "glTF binaire (format natif recommandé par Godot 4), textures intégrées, animations incluses."},
    "mobile": {"format": "glb", "label": "Jeux mobiles", "notes": "GLB allégé : modificateurs appliqués, décimation vers un budget de triangles, textures limitées en taille."},
    "print3d": {"format": "stl", "label": "Impression 3D", "notes": "STL en millimètres, modificateurs appliqués, vérification étanchéité (non-manifold signalé)."},
    "high_quality": {"format": "glb", "label": "Haute qualité", "notes": "GLB complet : modificateurs appliqués, normales, UV, matériaux, couleurs de sommets, animations."},
}


def _safe(name: str) -> str:
    return "".join(c if c.isalnum() or c in "-_." else "_" for c in name)[:80] or "export"


def verify_file(path: str, fmt: str) -> dict:
    """Vérifie réellement le fichier écrit (en-têtes et contenu)."""
    p = Path(path)
    if not p.exists():
        raise CommandError(f"Le fichier n'a pas été créé : {path}")
    size = p.stat().st_size
    if size == 0:
        raise CommandError(f"Fichier vide : {path}")
    info = {"path": str(p), "bytes": size, "valid": False}
    with open(p, "rb") as f:
        head = f.read(512)
    if fmt == "glb":
        magic, version, length = struct.unpack("<4sII", head[:12])
        info["valid"] = magic == b"glTF" and version == 2 and length == size
        if info["valid"]:
            with open(p, "rb") as f:
                f.seek(12)
                clen, ctype = struct.unpack("<I4s", f.read(8))
                js = json.loads(f.read(clen))
            info.update(meshes=len(js.get("meshes", [])), materials=len(js.get("materials", [])),
                        animations=len(js.get("animations", [])), images=len(js.get("images", [])),
                        nodes=len(js.get("nodes", [])), skins=len(js.get("skins", [])))
    elif fmt == "gltf":
        js = json.loads(p.read_text(encoding="utf-8"))
        info["valid"] = js.get("asset", {}).get("version") == "2.0"
        info.update(meshes=len(js.get("meshes", [])), animations=len(js.get("animations", [])))
        missing = [b.get("uri") for b in js.get("buffers", []) + js.get("images", [])
                   if b.get("uri") and not b["uri"].startswith("data:") and not (p.parent / b["uri"]).exists()]
        if missing:
            info["valid"] = False
            info["missing_files"] = missing
    elif fmt == "fbx":
        info["valid"] = head.startswith(b"Kaydara FBX Binary") or b"FBXHeaderExtension" in head
    elif fmt == "stl":
        if head[:5].lower() == b"solid" and b"facet" in p.read_bytes()[:4096]:
            info["valid"] = True
            info["encoding"] = "ascii"
        else:
            n = struct.unpack("<I", head[80:84])[0]
            info["valid"] = size == 84 + 50 * n
            info["triangles"] = n
            info["encoding"] = "binaire"
    elif fmt == "ply":
        info["valid"] = head.startswith(b"ply")
        for line in head.split(b"\n"):
            if line.startswith(b"element vertex"):
                info["vertices"] = int(line.split()[-1])
            if line.startswith(b"element face"):
                info["faces"] = int(line.split()[-1])
    elif fmt == "obj":
        txt = p.read_text(encoding="utf-8", errors="replace")
        info["vertices"] = txt.count("\nv ")
        info["faces"] = txt.count("\nf ")
        info["valid"] = info["vertices"] > 0
    elif fmt == "blend":
        info["valid"] = head.startswith(b"BLENDER") or head[:4] in (b"\x28\xb5\x2f\xfd", b"\x1f\x8b\x08\x00")
    if not info["valid"]:
        raise CommandError(f"Le fichier exporté semble invalide ({fmt}) : {path}")
    return info


def _collect(p: Params):
    if p.has("name") or p.has("names"):
        roots = target_objects(p, allow_selection=False)
        objs = []
        for r in roots:
            objs.extend(hierarchy(r))
            arm_name = r.get("se_armature")
            if arm_name and arm_name in bpy.data.objects:
                objs.extend(hierarchy(bpy.data.objects[arm_name]))
        return list({o.name: o for o in objs}.values())
    if p.bool("selected_only", False):
        return [o for o in bpy.context.view_layer.objects if o.select_get()]
    return [o for o in bpy.context.scene.objects if o.visible_get() and o.type in
            {"MESH", "ARMATURE", "EMPTY", "CURVE", "LIGHT", "CAMERA"}]


@command("export_model", label="Exporter", history=False, changes_scene=False)
def cmd_export(engine, p: Params):
    """Exporte vers glb, gltf, obj, fbx, stl, ply ou blend, ou selon un profil (unity, unreal, godot, mobile,
    print3d, high_quality). `name` limite l'export à un modèle (et ses enfants). Le fichier est vérifié."""
    ensure_object_mode()
    profile = p.str("profile", None, choices=list(PROFILES)) if p.has("profile") else None
    fmt = p.str("format", PROFILES[profile]["format"] if profile else "glb", choices=FORMATS)
    objs = _collect(p)
    if not objs and fmt != "blend":
        raise CommandError("Rien à exporter.")
    base = p.str("filename", None)
    if not base:
        base = objs[0].name if (p.has("name") and objs) else (bpy.context.scene.name or "scene")
        if profile:
            base += f"_{profile}"
    folder = Path(p.str("folder", str(engine.home / "exports")))
    folder.mkdir(parents=True, exist_ok=True)
    path = str(folder / f"{_safe(base)}.{fmt}")
    if os.path.exists(path) and not p.bool("overwrite", True):
        raise CommandError(f"Le fichier existe déjà : {path}")
    apply_mods = p.bool("apply_modifiers", True)
    anim = p.bool("animations", True)
    t0 = time.time()
    tmp_mods = []
    notes = []
    # préparation par profil, sur des copies temporaires pour ne pas altérer la scène
    work = objs
    temp_objs = []
    if profile in ("mobile", "print3d") or p.has("target_triangles"):
        target = p.int("target_triangles", 15000 if profile == "mobile" else 0, lo=0)
        if target:
            meshes = [o for o in objs if o.type == "MESH"]
            total = 0
            dg = bpy.context.evaluated_depsgraph_get()
            for o in meshes:
                em = o.evaluated_get(dg).to_mesh()
                total += sum(len(pl.vertices) - 2 for pl in em.polygons)
                o.evaluated_get(dg).to_mesh_clear()
            if total > target:
                ratio = max(0.01, target / total)
                for o in meshes:
                    m = o.modifiers.new("_SE_export_decimate", "DECIMATE")
                    m.ratio = ratio
                    tmp_mods.append((o, m.name))
                notes.append(f"décimation {total} → ~{target} triangles")
    select_only(work)
    try:
        if fmt in ("glb", "gltf"):
            kw = dict(filepath=path, export_format="GLB" if fmt == "glb" else "GLTF_SEPARATE", use_selection=True,
                      export_apply=apply_mods, export_animations=anim, export_yup=True,
                      export_texcoords=True, export_normals=True, export_materials="EXPORT")
            try:
                bpy.ops.export_scene.gltf(**kw, export_vertex_color="ACTIVE")
            except TypeError:
                bpy.ops.export_scene.gltf(**kw)
        elif fmt == "fbx":
            kw = dict(filepath=path, use_selection=True, use_mesh_modifiers=apply_mods, bake_anim=anim,
                      add_leaf_bones=False, path_mode="COPY", embed_textures=True, mesh_smooth_type="FACE")
            if profile == "unity":
                kw.update(apply_scale_options="FBX_SCALE_ALL", axis_forward="-Z", axis_up="Y",
                          bake_space_transform=True)
            elif profile == "unreal":
                kw.update(apply_scale_options="FBX_SCALE_NONE", axis_forward="-Y", axis_up="Z")
            bpy.ops.export_scene.fbx(**kw)
        elif fmt == "obj":
            bpy.ops.wm.obj_export(filepath=path, export_selected_objects=True, apply_modifiers=apply_mods,
                                  export_materials=True, export_uv=True, export_normals=True,
                                  path_mode="COPY")
        elif fmt == "stl":
            scale = 1000.0 if profile == "print3d" else p.float("scale", 1.0, lo=1e-6, hi=1e6)
            bpy.ops.wm.stl_export(filepath=path, export_selected_objects=True, apply_modifiers=apply_mods,
                                  global_scale=scale, ascii_format=False)
            if profile == "print3d":
                notes.append("unités : millimètres")
        elif fmt == "ply":
            bpy.ops.wm.ply_export(filepath=path, export_selected_objects=True, apply_modifiers=apply_mods,
                                  export_colors="SRGB", export_normals=True, export_uv=True)
        elif fmt == "blend":
            if p.has("name"):
                bpy.data.libraries.write(path, set(objs), fake_user=True, compress=True)
            else:
                bpy.ops.wm.save_as_mainfile(filepath=path, copy=True, compress=True)
    finally:
        for o, mname in tmp_mods:
            m = o.modifiers.get(mname)
            if m:
                o.modifiers.remove(m)
        for o in temp_objs:
            bpy.data.objects.remove(o, do_unlink=True)
    info = verify_file(path, fmt)
    if profile == "print3d":
        from .modeling import cmd_inspect
        checks = []
        for o in objs:
            if o.type == "MESH":
                r = cmd_inspect(engine, Params({"name": o.name}))
                checks.append({"name": o.name, "watertight": r["watertight"], "issues": r["issues"]})
        info["print_checks"] = checks
    engine.info(f"Export {fmt.upper()} vérifié : {path} ({info['bytes'] // 1024} Ko)")
    return {"path": path, "format": fmt, "profile": profile, "objects": [o.name for o in objs],
            "verification": info, "notes": notes + ([PROFILES[profile]["notes"]] if profile else []),
            "seconds": round(time.time() - t0, 2)}


@command("list_export_profiles", history=False)
def cmd_profiles(engine, p: Params):
    """Profils d'export disponibles."""
    return {"profiles": [{"id": k, **v} for k, v in PROFILES.items()], "formats": FORMATS}


@command("import_model", label="Importer")
def cmd_import(engine, p: Params):
    """Importe un fichier glb/gltf/obj/fbx/stl/ply/blend (ajoute à la scène)."""
    ensure_object_mode()
    path = p.str("path")
    if not os.path.isfile(path):
        raise CommandError(f"Fichier introuvable : {path}")
    ext = Path(path).suffix.lower().lstrip(".")
    if ext not in FORMATS:
        raise CommandError(f"Format non pris en charge : .{ext} (formats : {', '.join(FORMATS)})")
    before = set(bpy.data.objects.keys())
    if ext in ("glb", "gltf"):
        bpy.ops.import_scene.gltf(filepath=path)
    elif ext == "fbx":
        bpy.ops.import_scene.fbx(filepath=path)
    elif ext == "obj":
        bpy.ops.wm.obj_import(filepath=path)
    elif ext == "stl":
        bpy.ops.wm.stl_import(filepath=path)
    elif ext == "ply":
        bpy.ops.wm.ply_import(filepath=path)
    elif ext == "blend":
        with bpy.data.libraries.load(path, link=False) as (src, dst):
            dst.objects = list(src.objects)
        for o in dst.objects:
            if o is not None:
                bpy.context.scene.collection.objects.link(o)
    new = sorted(set(bpy.data.objects.keys()) - before)
    if not new:
        raise CommandError("Aucun objet importé (fichier vide ou incompatible).")
    for n in new:
        o = bpy.data.objects[n]
        if n not in bpy.context.scene.objects:
            bpy.context.scene.collection.objects.link(o)
    if p.bool("normalize", False):
        from .generators.postprocess import fit_height
        roots = [bpy.data.objects[n] for n in new if bpy.data.objects[n].parent is None]
        fit_height(roots, p.float("height", 2.0, lo=0.001))
    select_only([bpy.data.objects[n] for n in new])
    return {"imported": new, "path": path, "_label": f"Importer {Path(path).name}"}


@command("list_files", history=False)
def cmd_list_files(engine, p: Params):
    """Fichiers du dossier de travail (exports, rendus, projets, imports, textures)."""
    kind = p.str("kind", "exports", choices=["exports", "renders", "projects", "imports", "textures"])
    root = engine.home / kind
    files = []
    for f in sorted(root.rglob("*"), key=lambda x: x.stat().st_mtime, reverse=True):
        if f.is_file():
            files.append({"name": str(f.relative_to(root)), "path": str(f), "bytes": f.stat().st_size,
                          "modified": f.stat().st_mtime})
        if len(files) >= 300:
            break
    return {"folder": str(root), "files": files}


@command("export_preview", history=False, changes_scene=False)
def cmd_export_preview(engine, p: Params):
    """GLB temporaire de la scène visible (animations comprises) pour la lecture d'animation dans le viewport."""
    ensure_object_mode()
    objs = [o for o in bpy.context.scene.objects if o.visible_get() and o.type in {"MESH", "ARMATURE", "EMPTY"}]
    if not objs:
        raise CommandError("Scène vide.")
    path = str(engine.home / "tmp" / f"apercu_{int(time.time() * 1000)}.glb")
    for old in (engine.home / "tmp").glob("apercu_*.glb"):
        try:
            old.unlink()
        except OSError:
            pass
    select_only(objs)
    try:
        bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True, export_apply=True,
                                  export_animations=True, export_yup=True, export_vertex_color="ACTIVE")
    except TypeError:
        bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True, export_apply=True,
                                  export_animations=True, export_yup=True)
    info = verify_file(path, "glb")
    sc = bpy.context.scene
    return {"path": path, "animations": info.get("animations", 0), "fps": sc.render.fps,
            "frame_start": sc.frame_start, "frame_end": sc.frame_end}
