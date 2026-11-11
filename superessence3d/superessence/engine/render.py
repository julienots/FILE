"""Rendu Blender (Cycles / EEVEE / Workbench) exécuté dans un processus séparé.

Le processus séparé isole le studio : un rendu long ne bloque pas l'interface, et un
pilote graphique défaillant (EEVEE sans GPU) ne peut pas faire planter l'application.
"""
from __future__ import annotations

import os
import subprocess
import sys
import time
from pathlib import Path

import bpy

from .core import CommandError, Params, command

ENGINES = {"cycles": "CYCLES", "eevee": "BLENDER_EEVEE", "workbench": "BLENDER_WORKBENCH"}

WORKER = r'''
import sys, bpy
blend, out, engine, samples, rx, ry, cam, transparent = sys.argv[sys.argv.index("--")+1:]
bpy.ops.wm.open_mainfile(filepath=blend, load_ui=False)
sc = bpy.context.scene
sc.render.engine = engine
if engine == "CYCLES":
    sc.cycles.device = "CPU"
    sc.cycles.samples = int(samples)
    sc.cycles.use_denoising = True
elif engine == "BLENDER_EEVEE":
    try: sc.eevee.taa_render_samples = int(samples)
    except Exception: pass
sc.render.resolution_x = int(rx); sc.render.resolution_y = int(ry); sc.render.resolution_percentage = 100
sc.render.film_transparent = transparent == "1"
if cam and cam in bpy.data.objects: sc.camera = bpy.data.objects[cam]
sc.render.image_settings.file_format = "PNG"
sc.render.filepath = out
bpy.ops.render.render(write_still=True)
print("SE_RENDER_DONE", out)
'''


def render_to_file(engine, out_path: str, engine_name: str = "cycles", samples: int = 64, resolution=(1280, 720),
                   camera: str | None = None, transparent: bool = False, timeout: int = 1800) -> dict:
    sc = bpy.context.scene
    if sc.camera is None and not camera:
        raise CommandError("Aucune caméra active : créez-en une (create_camera).")
    tmp = engine.home / "tmp" / f"render_{os.getpid()}_{int(time.time() * 1000)}.blend"
    # images non sauvegardées : on les intègre pour que le processus de rendu les voie
    for img in bpy.data.images:
        if img.source == "GENERATED" or (img.is_dirty and not img.packed_file):
            try:
                img.pack()
            except RuntimeError:
                pass
    bpy.ops.wm.save_as_mainfile(filepath=str(tmp), copy=True, compress=False, check_existing=False)
    script = engine.home / "tmp" / "render_worker.py"
    script.write_text(WORKER, encoding="utf-8")
    cmd = [sys.executable, str(script), "--", str(tmp), out_path, ENGINES[engine_name], str(samples),
           str(resolution[0]), str(resolution[1]), camera or "", "1" if transparent else "0"]
    t0 = time.time()
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        raise CommandError(f"Rendu interrompu après {timeout} s.") from None
    finally:
        for f in (tmp, Path(str(tmp) + "1")):
            try:
                f.unlink()
            except OSError:
                pass
    if proc.returncode != 0 or not os.path.exists(out_path):
        tail = "\n".join((proc.stderr or proc.stdout or "").strip().splitlines()[-6:])
        hint = ""
        if engine_name == "eevee":
            hint = " EEVEE nécessite une carte graphique (OpenGL/Vulkan) : utilisez Cycles sur cette machine."
        raise CommandError(f"Le rendu a échoué (code {proc.returncode}).{hint}\n{tail}")
    return {"path": out_path, "engine": engine_name, "samples": samples, "resolution": list(resolution),
            "seconds": round(time.time() - t0, 1), "bytes": os.path.getsize(out_path)}


@command("render_scene", label="Rendu", history=False, changes_scene=False)
def cmd_render(engine, p: Params):
    """Rendu réel de la scène par Blender → PNG. engine : cycles (fonctionne partout), eevee (GPU requis),
    workbench. samples, resolution [l, h], camera, transparent."""
    eng = p.str("engine", engine.settings.get("render_engine", "CYCLES").lower().replace("blender_", ""),
                choices=list(ENGINES))
    samples = p.int("samples", int(engine.settings.get("render_samples", 64)), lo=1, hi=8192)
    res = p.list("resolution", [bpy.context.scene.render.resolution_x, bpy.context.scene.render.resolution_y])
    try:
        rx, ry = int(res[0]), int(res[1])
    except (TypeError, ValueError, IndexError):
        raise CommandError("resolution doit être [largeur, hauteur].") from None
    if not (16 <= rx <= 8192 and 16 <= ry <= 8192):
        raise CommandError("Résolution entre 16 et 8192 pixels.")
    name = p.str("filename", f"rendu_{time.strftime('%Y%m%d_%H%M%S')}")
    out = str(engine.home / "renders" / f"{''.join(c for c in name if c.isalnum() or c in '-_') or 'rendu'}.png")
    engine.progress(f"Rendu {eng} {rx}×{ry}, {samples} échantillons…", None)
    r = render_to_file(engine, out, eng, samples, (rx, ry), p.str("camera", None), p.bool("transparent", False))
    engine.info(f"Rendu terminé en {r['seconds']} s : {out}")
    return r
