"""Récupération du MP4 rendu par CUTTINX.

CUTTINX exporte le rendu final dans la galerie du téléphone (Movies/Cuttinx). Trois façons de le ramener au hub :
1. CUTTINX/get_render fournit une URL de téléchargement -> le hub la télécharge directement ;
2. une automatisation du téléphone (HTTP Shortcuts, MacroDroid, Tasker...) envoie le fichier sur
   POST /api/renders/inbox (jeton RENDER_UPLOAD_TOKEN) ;
3. un dossier synchronisé (Syncthing, rclone, FolderSync...) pointé par RENDER_INBOX_DIR.
Le fichier est rattaché à la bonne vidéo grâce au nom de projet « HUB-<id> » présent dans son nom.
Sinon il apparaît dans « Rendus non attribués » du tableau de bord pour un rattachement en un clic.
"""
from __future__ import annotations

import re
import shutil
import time
from pathlib import Path

import httpx

from .config import Settings

VIDEO_EXT = {".mp4", ".mov", ".m4v"}
_ID_RE = re.compile(r"hub[\s_\-\.]*([0-9a-f]{10})", re.IGNORECASE)


def match_video_id(filename: str) -> str | None:
    m = _ID_RE.search(filename)
    return m.group(1).lower() if m else None


def store_render(settings: Settings, vid: str, src: Path, *, move: bool = True) -> Path:
    dst = settings.renders_dir / f"{vid}.mp4"
    if move:
        shutil.move(str(src), dst)
    else:
        shutil.copyfile(src, dst)
    return dst


async def download(settings: Settings, vid: str, url: str) -> Path:
    dst = settings.renders_dir / f"{vid}.mp4"
    tmp = dst.with_suffix(".part")
    async with httpx.AsyncClient(timeout=httpx.Timeout(30, read=600), follow_redirects=True) as client:
        async with client.stream("GET", url) as r:
            r.raise_for_status()
            with tmp.open("wb") as f:
                async for chunk in r.aiter_bytes(1 << 20):
                    f.write(chunk)
    tmp.replace(dst)
    return dst


def _stable(path: Path, min_age: float = 20.0) -> bool:
    """Fichier encore en cours de copie/synchronisation ? On attend qu'il ne bouge plus."""
    try:
        return time.time() - path.stat().st_mtime >= min_age and path.stat().st_size > 0
    except FileNotFoundError:
        return False


def scan_inbox(settings: Settings) -> list[tuple[Path, str | None]]:
    """Fichiers vidéo prêts dans RENDER_INBOX_DIR, avec l'id de vidéo détecté (ou None)."""
    out: list[tuple[Path, str | None]] = []
    if not settings.render_inbox_dir or not settings.render_inbox_dir.exists():
        return out
    for p in sorted(settings.render_inbox_dir.rglob("*")):
        if p.is_file() and p.suffix.lower() in VIDEO_EXT and _stable(p):
            out.append((p, match_video_id(p.name)))
    return out


def list_unassigned(settings: Settings) -> list[dict]:
    items = []
    for p in sorted(settings.unassigned_dir.glob("*"), key=lambda x: x.stat().st_mtime, reverse=True):
        if p.is_file():
            items.append({"name": p.name, "size_mb": round(p.stat().st_size / 1e6, 1), "at": p.stat().st_mtime})
    return items
