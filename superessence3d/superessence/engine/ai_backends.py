"""Moteurs IA neuronaux FACULTATIFS (image→3D, texte→3D) exécutés localement.

SUPERESSENCE ne fournit aucun modèle neuronal et ne prétend pas en avoir. Si vous installez
un moteur open source sur votre machine, déclarez sa ligne de commande dans les réglages
(« ai_backends ») : le studio l'exécute, vérifie le fichier produit et l'importe.

Exemple (TripoSR, licence MIT, https://github.com/VAST-AI-Research/TripoSR) :
  {"name": "TripoSR", "license": "MIT",
   "command": "python /chemin/TripoSR/run.py {image} --output-dir {output_dir} --model-save-format glb",
   "output_glob": "{output_dir}/0/mesh.glb"}

Vérifiez toujours la licence du modèle (certains interdisent l'usage commercial ou
restreignent certains territoires).
"""
from __future__ import annotations

import glob
import os
import shlex
import shutil
import subprocess
import threading
import time
import uuid

import bpy

from .core import CommandError, Params, command


def _backends(engine) -> list[dict]:
    return list(engine.settings.get("ai_backends", []) or [])


def backend_status(b: dict) -> dict:
    try:
        exe = shlex.split(b.get("command", ""))[0]
    except (ValueError, IndexError):
        exe = ""
    found = bool(exe) and (shutil.which(exe) is not None or os.path.isfile(exe))
    return {"name": b.get("name"), "license": b.get("license", "?"), "available": found,
            "executable": exe, "kind": b.get("kind", "image_to_3d")}


@command("list_ai_backends", history=False)
def cmd_list_backends(engine, p: Params):
    """Moteurs IA locaux configurés et leur disponibilité (aucun n'est installé par défaut)."""
    return {"backends": [backend_status(b) for b in _backends(engine)],
            "note": "Aucun moteur neuronal n'est inclus. Le mode procédural fonctionne sans eux."}


def run_backend(engine, name: str, image: str, out_name: str | None = None) -> dict:
    b = next((x for x in _backends(engine) if x.get("name") == name), None)
    if b is None:
        raise CommandError(f"Moteur IA non configuré : « {name} ». Ajoutez-le dans Paramètres › Moteurs IA.")
    st = backend_status(b)
    if not st["available"]:
        raise CommandError(f"Le moteur « {name} » est configuré mais introuvable ({st['executable']}).")
    if not os.path.isfile(image):
        raise CommandError(f"Image introuvable : {image}")
    if engine.defer is None:
        raise CommandError("Exécution en arrière-plan indisponible dans ce mode.")
    job = uuid.uuid4().hex[:8]
    out_dir = engine.home / "imports" / f"ia_{job}"
    out_dir.mkdir(parents=True, exist_ok=True)
    output = str(out_dir / "mesh.glb")
    cmd = b["command"].format(image=shlex.quote(image), output=shlex.quote(output), output_dir=shlex.quote(str(out_dir)))

    def worker():
        t0 = time.time()
        engine.emit({"type": "job", "id": job, "status": "running", "message": f"{name} : génération neuronale…"})
        try:
            proc = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=3600)
            produced = output if os.path.exists(output) else None
            if produced is None and b.get("output_glob"):
                hits = glob.glob(b["output_glob"].format(output_dir=str(out_dir)))
                produced = hits[0] if hits else None
            if proc.returncode != 0 or produced is None:
                tail = (proc.stderr or proc.stdout or "")[-1500:]
                engine.emit({"type": "job", "id": job, "status": "error", "message": f"{name} a échoué : {tail}"})
                return

            def do_import():
                return engine.execute("import_model", {"path": produced, "normalize": True})
            engine.defer(do_import, label=f"Import IA {name}")
            engine.emit({"type": "job", "id": job, "status": "done", "path": produced,
                         "message": f"{name} terminé en {time.time() - t0:.0f} s"})
        except subprocess.TimeoutExpired:
            engine.emit({"type": "job", "id": job, "status": "error", "message": f"{name} : délai dépassé"})

    threading.Thread(target=worker, daemon=True).start()
    return {"job": job, "status": "started", "backend": name, "license": b.get("license", "?"),
            "message": "Génération neuronale lancée en arrière-plan ; le modèle sera importé à la fin.",
            "_label": f"IA {name}"}
