"""Projets : fichiers .blend natifs (ouvrables aussi dans Blender)."""
from __future__ import annotations

import os
import time
from pathlib import Path

import bpy

from .core import CommandError, Params, command


def _project_path(engine, name_or_path: str) -> str:
    if os.path.isabs(name_or_path) or os.sep in name_or_path or "/" in name_or_path:
        path = name_or_path
    else:
        safe = "".join(c if c.isalnum() or c in "-_ " else "_" for c in name_or_path).strip() or "projet"
        path = str(engine.home / "projects" / f"{safe}.blend")
    if not path.endswith(".blend"):
        path += ".blend"
    return path


@command("save_project", label="Enregistrer", history=False, changes_scene=False)
def cmd_save(engine, p: Params):
    """Enregistre le projet (.blend). `name` (dans le dossier projets) ou `path` complet ; sinon le fichier courant."""
    if p.has("path"):
        path = _project_path(engine, p.str("path"))
    elif p.has("name"):
        path = _project_path(engine, p.str("name"))
    elif engine.project_path:
        path = engine.project_path
    else:
        path = _project_path(engine, f"projet_{time.strftime('%Y%m%d_%H%M%S')}")
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    for img in bpy.data.images:  # textures générées intégrées au fichier
        if img.source == "GENERATED" and not img.packed_file:
            try:
                img.pack()
            except RuntimeError:
                pass
    bpy.ops.wm.save_as_mainfile(filepath=path, copy=True, compress=True, check_existing=False)
    if not os.path.exists(path) or os.path.getsize(path) == 0:
        raise CommandError("L'enregistrement a échoué.")
    engine.project_path = path
    engine.dirty = False
    engine.info(f"Projet enregistré : {path}")
    engine.emit({"type": "project", "path": path})
    return {"path": path, "bytes": os.path.getsize(path)}


@command("load_project", label="Ouvrir un projet", history=False, changes_scene=True, destructive=True)
def cmd_load(engine, p: Params):
    """Ouvre un projet .blend (remplace la scène). Si des modifications ne sont pas enregistrées, exige confirm=true."""
    path = _project_path(engine, p.str("path", None) or p.str("name"))
    if not os.path.isfile(path):
        raise CommandError(f"Projet introuvable : {path}")
    if engine.dirty and engine.history.can_undo():
        p.confirm("ouvrir un projet et abandonner les modifications non enregistrées")
    bpy.ops.wm.open_mainfile(filepath=path, load_ui=False)
    engine.project_path = path
    engine.history.reset(f"Ouverture de {Path(path).name}")
    engine.dirty = False
    engine.emit({"type": "history", "history": engine.history.as_list()})
    engine.emit({"type": "project", "path": path})
    return {"path": path, "objects": len(bpy.context.scene.objects)}


@command("new_project", label="Nouveau projet", history=False, changes_scene=True, destructive=True)
def cmd_new(engine, p: Params):
    """Nouvelle scène vide (caméra + lumière). Exige confirm=true si des modifications ne sont pas enregistrées."""
    if engine.dirty and engine.history.can_undo():
        p.confirm("créer un nouveau projet et abandonner les modifications non enregistrées")
    bpy.ops.wm.read_factory_settings(use_empty=True)
    from .scene_ops import setup_default_scene
    setup_default_scene()
    engine.project_path = None
    engine.history.reset("Nouvelle scène")
    engine.dirty = False
    engine.emit({"type": "history", "history": engine.history.as_list()})
    engine.emit({"type": "project", "path": None})
    return {"ok": True}


@command("list_projects", history=False)
def cmd_list_projects(engine, p: Params):
    """Projets enregistrés dans le dossier de travail."""
    root = engine.home / "projects"
    items = [{"name": f.stem, "path": str(f), "bytes": f.stat().st_size, "modified": f.stat().st_mtime}
             for f in root.glob("*.blend")]
    items.sort(key=lambda x: x["modified"], reverse=True)
    return {"folder": str(root), "projects": items, "current": engine.project_path}


@command("get_project_info", history=False)
def cmd_project_info(engine, p: Params):
    """Informations sur le projet courant et le dossier de travail."""
    return {"status": engine.status(), "home": str(engine.home), "folders": {
        k: str(engine.home / k) for k in ("projects", "exports", "renders", "library", "imports", "textures")},
        "settings": {k: v for k, v in engine.settings.items() if k != "ai_backends"},
        "ai_backends": [b.get("name") for b in engine.settings.get("ai_backends", [])]}


@command("run_python", label="Script Python")
def cmd_run_python(engine, p: Params):
    """Exécute du code Python Blender (bpy). Désactivé par défaut : à activer dans les réglages (allow_python)."""
    if not engine.settings.get("allow_python", False):
        raise CommandError("L'exécution de code est désactivée. Activez « allow_python » dans Paramètres "
                           "si vous faites confiance aux scripts exécutés.")
    code = p.str("code", max_len=200_000)
    import contextlib
    import io
    buf = io.StringIO()
    ns = {"bpy": bpy, "engine": engine, "result": None}
    try:
        with contextlib.redirect_stdout(buf):
            exec(compile(code, "<run_python>", "exec"), ns)
    except Exception as e:
        raise CommandError(f"{type(e).__name__}: {e}\n{buf.getvalue()[-2000:]}") from e
    res = ns.get("result")
    try:
        import json
        json.dumps(res)
    except (TypeError, ValueError):
        res = repr(res)
    return {"stdout": buf.getvalue()[-20000:], "result": res}


@command("set_settings", history=False, changes_scene=False)
def cmd_set_settings(engine, p: Params):
    """Modifie les réglages persistants (render_engine, render_samples, allow_python, history_limit, lan,
    ai_backends)."""
    from .. import config
    s = config.load_settings()
    if p.has("render_engine"):
        s["render_engine"] = p.str("render_engine", choices=["CYCLES", "BLENDER_EEVEE", "BLENDER_WORKBENCH"])
    if p.has("render_samples"):
        s["render_samples"] = p.int("render_samples", lo=1, hi=8192)
    if p.has("allow_python"):
        s["allow_python"] = p.bool("allow_python")
    if p.has("history_limit"):
        s["history_limit"] = p.int("history_limit", lo=5, hi=500)
    if p.has("lan"):
        s["lan"] = p.bool("lan")
    if p.has("ai_backends"):
        backends = p.list("ai_backends")
        for b in backends:
            if not isinstance(b, dict) or not b.get("name") or not b.get("command"):
                raise CommandError("Chaque moteur IA doit avoir « name » et « command ».")
            if "{image}" not in b["command"] or "{output}" not in b["command"]:
                raise CommandError("La commande doit contenir {image} et {output}.")
        s["ai_backends"] = backends
    config.save_settings(s)
    engine.settings.update(s)
    return {"settings": s}
