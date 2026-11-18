"""Génération du titre/accroche, de la description et des hashtags avec Claude (sortie JSON structurée)."""
from __future__ import annotations

import asyncio
import base64
import json
import shutil
import tempfile
from pathlib import Path

import anthropic

from .config import Settings

CAPTION_SCHEMA = {
    "type": "object",
    "properties": {
        "hook": {"type": "string", "description": "Accroche / titre percutant, 1 ligne, 90 caractères max, 1-2 emojis max."},
        "description": {
            "type": "string",
            "description": "Texte de légende 2 à 5 phrases courtes avec sauts de ligne, finit par un appel à l'action.",
        },
        "hashtags": {
            "type": "array",
            "items": {"type": "string"},
            "description": "15 à 25 hashtags pertinents SANS le #, mélange niche + large, en minuscules.",
        },
    },
    "required": ["hook", "description", "hashtags"],
    "additionalProperties": False,
}

SYSTEM = """Tu es le social media manager du créateur Instagram @{username} (street workout, calisthenics, vlogs).
Tu écris des légendes de Reels en français, ton aguicheur, énergique et authentique, pensé pour arrêter le scroll :
- l'accroche crée de la curiosité ou un défi (« Tu tiens combien de temps ? », chiffres, contraste) sans mentir sur le contenu ;
- la description donne envie de regarder jusqu'au bout et d'interagir (question, défi, « enregistre pour ta prochaine séance ») ;
- pas de clichés creux, pas de promesses fausses, pas de MAJUSCULES partout ;
- les hashtags mélangent niche (ex. frontlever, muscleup) et large (ex. fitness), selon ce qu'on voit vraiment.
Réponds uniquement avec l'objet JSON demandé."""


async def _extract_frames(video: Path, count: int = 3) -> list[bytes]:
    """Quelques images du rendu (si ffmpeg est installé) pour que Claude voie la vidéo."""
    if not shutil.which("ffmpeg") or not shutil.which("ffprobe"):
        return []
    try:
        probe = await asyncio.create_subprocess_exec(
            "ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", str(video),
            stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL,
        )
        out, _ = await probe.communicate()
        duration = float(out.decode().strip() or 0)
    except (OSError, ValueError):
        return []
    if duration <= 0:
        return []
    frames: list[bytes] = []
    with tempfile.TemporaryDirectory() as tmp:
        for i in range(count):
            t = duration * (i + 1) / (count + 1)
            dst = Path(tmp) / f"f{i}.jpg"
            proc = await asyncio.create_subprocess_exec(
                "ffmpeg", "-y", "-ss", f"{t:.2f}", "-i", str(video), "-frames:v", "1", "-vf", "scale=-2:720",
                "-q:v", "4", str(dst), stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL,
            )
            await proc.wait()
            if dst.exists():
                frames.append(dst.read_bytes())
    return frames


async def generate_caption(settings: Settings, video: dict) -> dict:
    client = anthropic.AsyncAnthropic(api_key=settings.anthropic_api_key or None)
    base_tags = settings.base_hashtags_performance if video["category"] == "performance" else settings.base_hashtags_normal
    brief = {
        "categorie": "Performance street workout / calisthenics" if video["category"] == "performance" else "Vidéo normale (vlog, divers)",
        "nom_du_fichier": video["original_name"],
        "notes_du_createur": video.get("notes") or "",
        "resume_du_montage": video.get("edit_summary") or "",
        "hashtags_a_inclure": base_tags,
    }
    content: list[dict] = []
    if video.get("rendered_path"):
        for jpg in await _extract_frames(Path(video["rendered_path"])):
            content.append(
                {"type": "image", "source": {"type": "base64", "media_type": "image/jpeg", "data": base64.b64encode(jpg).decode()}}
            )
    content.append(
        {
            "type": "text",
            "text": "Écris la légende Instagram de ce Reel.\n" + json.dumps(brief, ensure_ascii=False, indent=2),
        }
    )

    extra: dict = {}
    betas: list[str] = []
    if settings.claude_fallbacks:
        betas.append("server-side-fallback-2026-07-01")
        extra["fallbacks"] = "default"

    response = await client.beta.messages.create(
        model=settings.claude_model,
        max_tokens=16000,
        system=SYSTEM.format(username=settings.ig_username),
        thinking={"type": "adaptive"},
        output_config={
            "effort": settings.claude_caption_effort,
            "format": {"type": "json_schema", "schema": CAPTION_SCHEMA},
        },
        messages=[{"role": "user", "content": content}],
        betas=betas or anthropic.omit,
        **extra,
    )
    if response.stop_reason == "refusal":
        raise RuntimeError("Claude a refusé de générer la légende.")
    if response.stop_reason == "max_tokens":
        raise RuntimeError("Légende tronquée (max_tokens atteint).")
    text = "".join(b.text for b in response.content if b.type == "text")
    data = json.loads(text)
    tags = [t.lstrip("#") for t in base_tags] + [t.lstrip("#") for t in data["hashtags"]]
    return {"hook": data["hook"], "description": data["description"], "hashtags": tags}
