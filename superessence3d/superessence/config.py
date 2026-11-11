"""Chemins, réglages persistants et jeton d'accès local."""
from __future__ import annotations

import json
import os
import secrets
from pathlib import Path

DEFAULT_PORT = 8765

DEFAULT_SETTINGS = {
    "port": DEFAULT_PORT,
    # Exécution de code Python arbitraire par Claude (outil MCP run_python).
    # Désactivée par défaut : c'est puissant mais risqué.
    "allow_python": False,
    # Écoute sur le réseau local (téléphone/tablette). Désactivé par défaut.
    "lan": False,
    "render_engine": "CYCLES",
    "render_samples": 64,
    "history_limit": 40,
    # Moteurs IA neuronaux facultatifs (voir superessence/engine/ai_backends.py).
    # Chaque entrée : {"name":..., "command": "... {image} ... {output} ...", "license": "..."}
    "ai_backends": [],
}


def home() -> Path:
    base = os.environ.get("SUPERESSENCE_HOME")
    p = Path(base) if base else Path.home() / "SuperEssence3D"
    for sub in ("projects", "exports", "renders", "library", "history", "imports", "textures", "tmp"):
        (p / sub).mkdir(parents=True, exist_ok=True)
    return p


def settings_path() -> Path:
    return home() / "settings.json"


def load_settings() -> dict:
    s = dict(DEFAULT_SETTINGS)
    p = settings_path()
    if p.exists():
        try:
            s.update(json.loads(p.read_text(encoding="utf-8")))
        except (OSError, ValueError):
            pass
    return s


def save_settings(s: dict) -> None:
    merged = dict(DEFAULT_SETTINGS)
    merged.update(s)
    settings_path().write_text(json.dumps(merged, indent=2, ensure_ascii=False), encoding="utf-8")


def token() -> str:
    """Jeton secret partagé entre l'interface, le serveur MCP et le studio."""
    p = home() / "token"
    if p.exists():
        t = p.read_text(encoding="utf-8").strip()
        if t:
            return t
    t = secrets.token_urlsafe(32)
    p.write_text(t, encoding="utf-8")
    try:
        os.chmod(p, 0o600)
    except OSError:
        pass
    return t
