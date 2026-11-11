"""Bibliothèque de modèles : SQLite + fichiers .blend versionnés + GLB (aperçu 3D) + miniatures rendues."""
from __future__ import annotations

import json
import os
import shutil
import sqlite3
import subprocess
import sys
import threading
import time
from pathlib import Path

import bpy

from .core import CommandError, Params, command
from .util import get_object, hierarchy, select_only, get_json_prop

SCHEMA = """
CREATE TABLE IF NOT EXISTS models (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL,
  category TEXT NOT NULL DEFAULT 'Divers',
  tags TEXT NOT NULL DEFAULT '',
  description TEXT NOT NULL DEFAULT '',
  created REAL NOT NULL,
  updated REAL NOT NULL
);
CREATE TABLE IF NOT EXISTS versions (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  model_id INTEGER NOT NULL REFERENCES models(id) ON DELETE CASCADE,
  version INTEGER NOT NULL,
  blend_path TEXT NOT NULL,
  glb_path TEXT,
  thumb_path TEXT,
  note TEXT NOT NULL DEFAULT '',
  stats TEXT NOT NULL DEFAULT '{}',
  created REAL NOT NULL
);
CREATE TABLE IF NOT EXISTS generations (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  prompt TEXT NOT NULL,
  spec TEXT NOT NULL,
  objects TEXT NOT NULL,
  created REAL NOT NULL
);
"""

THUMB_WORKER = r'''
import sys, math, bpy
from mathutils import Vector
src, out = sys.argv[sys.argv.index("--")+1:]
bpy.ops.wm.read_factory_settings(use_empty=True)
with bpy.data.libraries.load(src, link=False) as (a, b):
    b.objects = list(a.objects)
sc = bpy.context.scene
objs = [o for o in b.objects if o is not None]
for o in objs:
    sc.collection.objects.link(o)
bpy.context.view_layer.update()
lo = Vector((1e9,)*3); hi = Vector((-1e9,)*3)
for o in objs:
    if o.type == "MESH":
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c); lo = Vector(map(min, lo, w)); hi = Vector(map(max, hi, w))
if lo.x > hi.x: lo, hi = Vector((-1,-1,-1)), Vector((1,1,1))
center = (lo+hi)/2; r = max((hi-lo).length/2, 0.05)
cam = bpy.data.objects.new("c", bpy.data.cameras.new("c")); sc.collection.objects.link(cam); sc.camera = cam
cam.data.lens = 50
d = Vector((1.0, -1.2, 0.7)).normalized()
cam.location = center + d * r * 1.25 / math.sin(cam.data.angle/2)
cam.rotation_euler = (center - cam.location).to_track_quat("-Z", "Y").to_euler()
cam.data.clip_end = r * 50
for loc, e in (((1,-1,1.4), 1000), ((-1.2,-0.5,0.5), 350), ((0,1.2,1), 600)):
    l = bpy.data.objects.new("l", bpy.data.lights.new("l", "AREA")); l.data.energy = e * r * r; l.data.size = r * 2
    l.location = center + Vector(loc) * r * 3; l.rotation_euler = (center - l.location).to_track_quat("-Z","Y").to_euler()
    sc.collection.objects.link(l)
w = bpy.data.worlds.new("w"); sc.world = w
if w.node_tree is None:
    w.use_nodes = True
bg = next(n for n in w.node_tree.nodes if n.type == "BACKGROUND"); bg.inputs[0].default_value = (0.03,0.035,0.045,1)
sc.render.engine = "CYCLES"; sc.cycles.device = "CPU"; sc.cycles.samples = 24; sc.cycles.use_denoising = True
sc.render.resolution_x = sc.render.resolution_y = 320
sc.render.film_transparent = False
sc.render.filepath = out
bpy.ops.render.render(write_still=True)
'''


class Library:
    def __init__(self, root: Path):
        self.root = root
        self.root.mkdir(parents=True, exist_ok=True)
        self.db_path = root / "library.db"
        with self._conn() as c:
            c.executescript(SCHEMA)

    def _conn(self):
        c = sqlite3.connect(self.db_path, timeout=10)
        c.row_factory = sqlite3.Row
        c.execute("PRAGMA foreign_keys = ON")
        return c


_lib: Library | None = None


def lib(engine) -> Library:
    global _lib
    if _lib is None or _lib.root != engine.home / "library":
        _lib = Library(engine.home / "library")
    return _lib


def _model_dir(L: Library, model_id: int) -> Path:
    d = L.root / f"model_{model_id:05d}"
    d.mkdir(parents=True, exist_ok=True)
    return d


def _write_version(engine, L: Library, model_id: int, roots: list, note: str) -> dict:
    objs = []
    for r in roots:
        objs.extend(hierarchy(r))
        arm = r.get("se_armature")
        if arm and arm in bpy.data.objects:
            objs.extend(hierarchy(bpy.data.objects[arm]))
    objs = list({o.name: o for o in objs}.values())
    with L._conn() as c:
        row = c.execute("SELECT COALESCE(MAX(version),0) FROM versions WHERE model_id=?", (model_id,)).fetchone()
        ver = row[0] + 1
    d = _model_dir(L, model_id)
    blend = d / f"v{ver:03d}.blend"
    glb = d / f"v{ver:03d}.glb"
    thumb = d / f"v{ver:03d}.png"
    for img in bpy.data.images:
        if img.source == "GENERATED" and not img.packed_file:
            try:
                img.pack()
            except RuntimeError:
                pass
    bpy.data.libraries.write(str(blend), set(objs), fake_user=True, compress=True)
    # GLB d'aperçu 3D
    prev_sel = [o for o in bpy.context.view_layer.objects if o.select_get()]
    select_only(objs)
    try:
        try:
            bpy.ops.export_scene.gltf(filepath=str(glb), export_format="GLB", use_selection=True, export_apply=True,
                                      export_animations=True, export_vertex_color="ACTIVE")
        except TypeError:
            bpy.ops.export_scene.gltf(filepath=str(glb), export_format="GLB", use_selection=True, export_apply=True)
    finally:
        select_only(prev_sel)
    stats = {"objects": len(objs), "triangles": sum(sum(len(p.vertices) - 2 for p in o.data.polygons)
                                                     for o in objs if o.type == "MESH")}
    with L._conn() as c:
        c.execute("INSERT INTO versions(model_id, version, blend_path, glb_path, thumb_path, note, stats, created) "
                  "VALUES (?,?,?,?,?,?,?,?)", (model_id, ver, str(blend), str(glb) if glb.exists() else None,
                                                None, note, json.dumps(stats), time.time()))
        c.execute("UPDATE models SET updated=? WHERE id=?", (time.time(), model_id))
    _render_thumbnail_async(engine, L, model_id, ver, str(blend), str(thumb))
    return {"version": ver, "blend": str(blend), "glb": str(glb), "stats": stats}


def _render_thumbnail_async(engine, L, model_id, ver, blend, thumb):
    script = engine.home / "tmp" / "thumb_worker.py"
    script.write_text(THUMB_WORKER, encoding="utf-8")

    def run():
        try:
            subprocess.run([sys.executable, str(script), "--", blend, thumb], capture_output=True, timeout=600)
            if os.path.exists(thumb):
                with L._conn() as c:
                    c.execute("UPDATE versions SET thumb_path=? WHERE model_id=? AND version=?", (thumb, model_id, ver))
                engine.emit({"type": "library_changed"})
        except Exception as e:  # la miniature est facultative
            engine.emit({"type": "log", "level": "warning", "message": f"Miniature non générée : {e}",
                         "time": time.time()})
    threading.Thread(target=run, daemon=True).start()


def _model_row(c, mid):
    m = c.execute("SELECT * FROM models WHERE id=?", (mid,)).fetchone()
    if m is None:
        raise CommandError(f"Modèle de bibliothèque introuvable : {mid}")
    return m


def _model_dict(c, m) -> dict:
    vers = c.execute("SELECT * FROM versions WHERE model_id=? ORDER BY version DESC", (m["id"],)).fetchall()
    return {"id": m["id"], "name": m["name"], "category": m["category"],
            "tags": [t for t in m["tags"].split(",") if t], "description": m["description"],
            "created": m["created"], "updated": m["updated"],
            "versions": [{"version": v["version"], "blend": v["blend_path"], "glb": v["glb_path"],
                          "thumbnail": v["thumb_path"], "note": v["note"], "stats": json.loads(v["stats"]),
                          "created": v["created"]} for v in vers]}


@command("library_save", label="Enregistrer dans la bibliothèque", history=False, changes_scene=False)
def cmd_library_save(engine, p: Params):
    """Enregistre un modèle (objet + enfants + squelette) dans la bibliothèque. `model_id` pour ajouter une
    nouvelle version d'un modèle existant."""
    roots = [get_object(n) for n in (p.list("names") if p.has("names") else [p.str("name")])]
    L = lib(engine)
    now = time.time()
    with L._conn() as c:
        if p.has("model_id"):
            mid = p.int("model_id")
            _model_row(c, mid)
        else:
            spec = get_json_prop(roots[0], "se_spec", {}) or {}
            cat = p.str("category", spec.get("category_label") or spec.get("category") or "Divers", max_len=60)
            tags = ",".join(str(t).strip() for t in p.list("tags", spec.get("tags", [])) if str(t).strip())
            cur = c.execute("INSERT INTO models(name, category, tags, description, created, updated) "
                            "VALUES (?,?,?,?,?,?)", (p.str("model_name", roots[0].name, max_len=80), cat, tags,
                                                     p.str("description", spec.get("prompt", ""), max_len=2000),
                                                     now, now))
            mid = cur.lastrowid
    v = _write_version(engine, L, mid, roots, p.str("note", "", max_len=500))
    engine.emit({"type": "library_changed"})
    return {"model_id": mid, **v}


@command("library_list", history=False)
def cmd_library_list(engine, p: Params):
    """Liste/recherche la bibliothèque : query (nom, tags, description), category."""
    L = lib(engine)
    q = f"%{p.str('query', '').strip()}%"
    cat = p.str("category", "")
    with L._conn() as c:
        sql = "SELECT * FROM models WHERE (name LIKE ? OR tags LIKE ? OR description LIKE ?)"
        args = [q, q, q]
        if cat:
            sql += " AND category = ?"
            args.append(cat)
        sql += " ORDER BY updated DESC"
        rows = c.execute(sql, args).fetchall()
        cats = [r[0] for r in c.execute("SELECT DISTINCT category FROM models ORDER BY category").fetchall()]
        return {"models": [_model_dict(c, m) for m in rows], "categories": cats}


@command("library_load", label="Ajouter depuis la bibliothèque")
def cmd_library_load(engine, p: Params):
    """Ajoute un modèle de la bibliothèque à la scène (dernière version, ou `version`)."""
    L = lib(engine)
    mid = p.int("model_id")
    with L._conn() as c:
        m = _model_row(c, mid)
        if p.has("version"):
            v = c.execute("SELECT * FROM versions WHERE model_id=? AND version=?", (mid, p.int("version"))).fetchone()
        else:
            v = c.execute("SELECT * FROM versions WHERE model_id=? ORDER BY version DESC LIMIT 1", (mid,)).fetchone()
    if v is None or not os.path.exists(v["blend_path"]):
        raise CommandError("Fichier de version introuvable.")
    with bpy.data.libraries.load(v["blend_path"], link=False) as (src, dst):
        dst.objects = list(src.objects)
    new = []
    for o in dst.objects:
        if o is None:
            continue
        bpy.context.scene.collection.objects.link(o)
        new.append(o)
    if p.has("location"):
        from mathutils import Vector
        off = Vector(p.vec("location"))
        roots = [o for o in new if o.parent is None]
        if roots:
            base = roots[0].location.copy()
            for r in roots:
                r.location = r.location - base + off
    select_only(new)
    return {"objects": [o.name for o in new], "model": m["name"], "version": v["version"],
            "_label": f"Bibliothèque : {m['name']}"}


@command("library_update", history=False, changes_scene=False)
def cmd_library_update(engine, p: Params):
    """Renomme / recatégorise / retague un modèle de bibliothèque."""
    L = lib(engine)
    mid = p.int("model_id")
    with L._conn() as c:
        _model_row(c, mid)
        if p.has("model_name"):
            c.execute("UPDATE models SET name=? WHERE id=?", (p.str("model_name", max_len=80), mid))
        if p.has("category"):
            c.execute("UPDATE models SET category=? WHERE id=?", (p.str("category", max_len=60), mid))
        if p.has("tags"):
            c.execute("UPDATE models SET tags=? WHERE id=?", (",".join(str(t) for t in p.list("tags")), mid))
        if p.has("description"):
            c.execute("UPDATE models SET description=? WHERE id=?", (p.str("description", max_len=2000), mid))
        c.execute("UPDATE models SET updated=? WHERE id=?", (time.time(), mid))
        m = _model_dict(c, _model_row(c, mid))
    engine.emit({"type": "library_changed"})
    return m


@command("library_duplicate", history=False, changes_scene=False)
def cmd_library_duplicate(engine, p: Params):
    """Duplique un modèle de bibliothèque (copie de la dernière version)."""
    L = lib(engine)
    mid = p.int("model_id")
    now = time.time()
    with L._conn() as c:
        m = _model_row(c, mid)
        v = c.execute("SELECT * FROM versions WHERE model_id=? ORDER BY version DESC LIMIT 1", (mid,)).fetchone()
        cur = c.execute("INSERT INTO models(name, category, tags, description, created, updated) VALUES (?,?,?,?,?,?)",
                        (p.str("model_name", f"{m['name']} (copie)"), m["category"], m["tags"], m["description"],
                         now, now))
        nid = cur.lastrowid
        if v is not None:
            d = _model_dir(L, nid)
            paths = {}
            for key, ext in (("blend_path", "blend"), ("glb_path", "glb"), ("thumb_path", "png")):
                if v[key] and os.path.exists(v[key]):
                    dst = d / f"v001.{ext}"
                    shutil.copy2(v[key], dst)
                    paths[key] = str(dst)
            c.execute("INSERT INTO versions(model_id, version, blend_path, glb_path, thumb_path, note, stats, created) "
                      "VALUES (?,?,?,?,?,?,?,?)", (nid, 1, paths.get("blend_path", ""), paths.get("glb_path"),
                                                   paths.get("thumb_path"), "copie", v["stats"], now))
    engine.emit({"type": "library_changed"})
    return {"model_id": nid}


@command("library_delete", history=False, changes_scene=False, destructive=True)
def cmd_library_delete(engine, p: Params):
    """Supprime un modèle de la bibliothèque (toutes ses versions). Exige confirm=true."""
    L = lib(engine)
    mid = p.int("model_id")
    with L._conn() as c:
        m = _model_row(c, mid)
        p.confirm(f"supprimer « {m['name']} » de la bibliothèque (toutes les versions)")
        c.execute("DELETE FROM versions WHERE model_id=?", (mid,))
        c.execute("DELETE FROM models WHERE id=?", (mid,))
    shutil.rmtree(L.root / f"model_{mid:05d}", ignore_errors=True)
    engine.emit({"type": "library_changed"})
    return {"deleted": mid}


@command("library_export", history=False, changes_scene=False)
def cmd_library_export(engine, p: Params):
    """Copie la dernière version d'un modèle (.blend et .glb) vers un dossier."""
    L = lib(engine)
    mid = p.int("model_id")
    folder = Path(p.str("folder", str(engine.home / "exports")))
    folder.mkdir(parents=True, exist_ok=True)
    with L._conn() as c:
        m = _model_row(c, mid)
        v = c.execute("SELECT * FROM versions WHERE model_id=? ORDER BY version DESC LIMIT 1", (mid,)).fetchone()
    if v is None:
        raise CommandError("Aucune version.")
    safe = "".join(ch if ch.isalnum() or ch in "-_" else "_" for ch in m["name"])
    out = []
    for key, ext in (("blend_path", "blend"), ("glb_path", "glb")):
        if v[key] and os.path.exists(v[key]):
            dst = folder / f"{safe}_v{v['version']}.{ext}"
            shutil.copy2(v[key], dst)
            out.append(str(dst))
    return {"files": out}


def record_generation(engine, prompt: str, spec: dict, objects: list[str]) -> int:
    L = lib(engine)
    with L._conn() as c:
        cur = c.execute("INSERT INTO generations(prompt, spec, objects, created) VALUES (?,?,?,?)",
                        (prompt, json.dumps(spec, ensure_ascii=False), json.dumps(objects), time.time()))
        return cur.lastrowid


@command("list_generations", history=False)
def cmd_list_generations(engine, p: Params):
    """Historique des générations (prompts, spécifications, objets créés)."""
    L = lib(engine)
    with L._conn() as c:
        rows = c.execute("SELECT * FROM generations ORDER BY id DESC LIMIT ?", (p.int("limit", 50, lo=1, hi=500),)).fetchall()
    return {"generations": [{"id": r["id"], "prompt": r["prompt"], "spec": json.loads(r["spec"]),
                             "objects": json.loads(r["objects"]), "created": r["created"]} for r in rows]}
