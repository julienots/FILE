"""Montage déterministe sans Claude (EDITOR_MODE=direct) : recette fixe appliquée via les outils MCP CUTTINX.

Moins fin que le mode Claude (pas de regard sur les images) mais gratuit en tokens et prévisible.
"""
from __future__ import annotations

import time
from typing import Callable

from .config import Settings
from .cuttinx_mcp import Cuttinx, CuttinxError, connect, find_key

PERF_TRANSITIONS = ["flash_zoom", "punch_zoom", "whip", "glitch_slide", "zoom_blur", "flash"]
PERF_EFFECTS = ["film_look", "clarity", "teal_orange"]


async def _import(cx: Cuttinx, url: str, name: str) -> str:
    data = await cx.call("import_from_url", url=url, name=name)
    media_id = find_key(data, "media_id", "id")
    if not media_id:
        raise CuttinxError(f"import_from_url n'a pas renvoyé de media_id : {data}")
    return str(media_id)


def _duration(media: dict | None, fallback: float) -> float:
    d = find_key(media or {}, "duration")
    try:
        return float(d) if d else fallback
    except (TypeError, ValueError):
        return fallback


async def _media_info(cx: Cuttinx, media_id: str) -> dict | None:
    data = await cx.call("list_media", type="video")
    for item in find_key(data, "items") or []:
        if item.get("id") == media_id:
            return item
    return None


def _snap(value: float, beats: list[float]) -> float:
    return min(beats, key=lambda b: abs(b - value)) if beats else value


def performance_recipe(source_id: str, duration: float, highlights: list[dict], music: dict | None) -> dict:
    """Moments forts dans l'ordre chronologique, coupes calées sur le tempo, ralenti sur le meilleur moment."""
    bpm = float(find_key(music or {}, "bpm") or 0) or 120.0
    beat = 60.0 / bpm
    clip_len = beat * 4  # 1 mesure
    hl = sorted(highlights or [], key=lambda h: h.get("start", 0))
    if not hl:  # pas de moment fort détecté : découpe régulière
        hl = [{"start": t, "end": min(duration, t + clip_len), "score": 0.5} for t in range(0, int(duration), max(1, int(clip_len * 2)))]
    best = max(hl, key=lambda h: h.get("score", 0))
    clips = []
    for i, h in enumerate(hl[:10]):
        start = max(0.0, float(h.get("start", 0)))
        is_best = h is best
        length = clip_len * (2 if is_best else 1)
        end = min(duration, start + length * (0.5 if is_best else 1))
        if end - start < 0.4:
            continue
        clip: dict = {
            "media_id": source_id,
            "start": round(start, 2),
            "end": round(end, 2),
            "volume": 0.15,
            "zoom": {"from": 1.0, "to": 1.12 if i % 2 == 0 else 1.06, "easing": "smooth"},
        }
        if is_best:
            clip["speed_ramp"] = [{"position": 0, "speed": 1.0}, {"position": 0.35, "speed": 0.4}, {"position": 1, "speed": 1.0}]
        if i > 0:
            clip["transition_in"] = {"id": PERF_TRANSITIONS[i % len(PERF_TRANSITIONS)], "duration": round(min(0.35, beat / 2), 2)}
        clips.append(clip)
    recipe: dict = {
        "clips": clips,
        "format": {"aspect": "9:16", "fps": 30},
        "effect_layers": [{"effect_id": "film_look", "params": {}}],
    }
    if music and music.get("media_id"):
        drops = find_key(music, "drops", "strongest", "highlights") or []
        drop_t = None
        if isinstance(drops, list) and drops:
            first = drops[0]
            drop_t = first.get("time", first.get("start")) if isinstance(first, dict) else first
        downbeats = find_key(music, "downbeats") or []
        start = max(0.0, float(drop_t) - beat * 4) if drop_t else 0.0
        if downbeats:
            start = _snap(start, [float(x) for x in downbeats if isinstance(x, (int, float))])
        recipe["music"] = {"media_id": music["media_id"], "start": round(start, 2), "fade_out": 1.5}
    return recipe


def normal_recipe(source_id: str, duration: float, silences: list[dict], aspect: str, music_id: str | None) -> dict:
    """Montage posé : on retire les silences > 0,35 s et on enchaîne en coupes franches."""
    keep: list[tuple[float, float]] = []
    cursor = 0.0
    for s in sorted(silences or [], key=lambda x: x.get("start", 0)):
        a, b = float(s.get("start", 0)), float(s.get("end", 0))
        if b - a < 0.35:
            continue
        if a - cursor > 0.5:
            keep.append((cursor, a + 0.08))
        cursor = max(cursor, b - 0.08)
    if duration - cursor > 0.5:
        keep.append((cursor, duration))
    if not keep:
        keep = [(0.0, duration)]
    clips = [
        {"media_id": source_id, "start": round(a, 2), "end": round(b, 2), "audio_fade_in": 0.03, "audio_fade_out": 0.03}
        for a, b in keep
    ]
    recipe: dict = {"clips": clips, "format": {"aspect": aspect, "fps": 30}}
    if music_id:
        recipe["music"] = {"media_id": music_id, "volume": 0.2, "fade_out": 2}
    return recipe


async def edit_direct(
    settings: Settings,
    video: dict,
    raw_url: str,
    music_url: str | None,
    on_progress: Callable[[str], None] | None = None,
) -> dict:
    say = on_progress or (lambda _m: None)
    name = f"HUB-{video['id']}"
    async with connect(settings) as cx:
        source_id = await _import(cx, raw_url, f"{name} source")
        say(f"Rush importé dans CUTTINX ({source_id})")
        info = await _media_info(cx, source_id)
        duration = _duration(info, 60.0)

        music_id = None
        if music_url:
            music_id = await _import(cx, music_url, f"{name} musique")
        elif video["category"] == "performance" and settings.default_music_media_id:
            music_id = settings.default_music_media_id

        if video["category"] == "performance":
            hl = await cx.call("find_highlights", media_id=source_id, count=8, segment_seconds=2.5)
            music = None
            if music_id:
                music = await cx.call("analyze_music", media_id=music_id)
                music = {**(music if isinstance(music, dict) else {}), "media_id": music_id}
            recipe = performance_recipe(source_id, duration, find_key(hl, "highlights") or [], music)
            summary = "Montage dynamique des moments forts, coupes sur le tempo, ralenti sur la meilleure figure."
        else:
            audio = await cx.call("analyze_audio", media_id=source_id)
            recipe = normal_recipe(source_id, duration, find_key(audio, "silences") or [], video["target_aspect"], music_id)
            summary = "Montage posé, silences supprimés."

        project = await cx.call("create_project", name=name, project_id=video.get("cuttinx_project_id"), **recipe)
        project_id = str(find_key(project, "project_id", "id") or "")
        if not project_id:
            raise CuttinxError(f"create_project n'a pas renvoyé d'id : {project}")
        say(f"Projet CUTTINX créé ({project_id}), rendu final lancé")

        job = await cx.call("render_project", project_id=project_id, quality="final", resolution=settings.render_resolution)
        job_id = str(find_key(job, "job_id", "id") or "")
        deadline = time.time() + 45 * 60
        while True:
            state = await cx.call("get_render", job_id=job_id, wait_seconds=90)
            status = str(find_key(state, "status", "state") or "").lower()
            if status in {"done", "completed", "finished", "success", "succeeded", "ok"} or find_key(state, "media_id"):
                break
            if status in {"error", "failed", "cancelled", "canceled"}:
                raise CuttinxError(f"Rendu CUTTINX en échec : {state}")
            if time.time() > deadline:
                raise CuttinxError("Rendu CUTTINX trop long (45 min).")
        media_id = str(find_key(state, "media_id", "render_media_id") or "")
        url = find_key(state, "download_url", "url", "file_url") or ""
        return {
            "project_id": project_id,
            "render_job_id": job_id,
            "render_media_id": media_id,
            "download_url": url if str(url).startswith("http") else "",
            "summary": summary,
        }
