"""Moteur du pipeline : dépôt -> montage -> récupération du rendu -> texte -> programmation -> publication.

Chaque vidéo a une « prochaine étape » (step) et une date de prochaine tentative. Le worker prend les vidéos
dues, exécute l'étape, puis passe à la suivante. En cas d'échec : relance avec délai exponentiel, puis statut
« Erreur » + notification après MAX_ATTEMPTS. Tout est persistant (SQLite) : un redémarrage reprend où on en était.
"""
from __future__ import annotations

import asyncio
import logging
import shutil
import time
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo

from . import renders
from .captions import generate_caption
from .config import Settings
from .db import DB
from .instagram import Instagram, build_caption
from .notify import notify
from .scheduler import next_slot
from .security import file_url

log = logging.getLogger("hub.pipeline")

STEP_STATUS = {  # statut affiché pendant l'exécution de chaque étape
    "edit": "editing",
    "fetch_render": "awaiting_render",
    "caption": "captioning",
    "schedule": "captioning",
    "publish": "publishing",
}
STEP_WAITING_STATUS = {  # statut affiché quand l'étape est en attente
    "edit": "queued",
    "fetch_render": "awaiting_render",
    "caption": "rendered",
    "schedule": "rendered",
    "publish": "scheduled",
}
STEP_LABEL = {
    "edit": "montage",
    "fetch_render": "récupération du rendu",
    "caption": "génération du texte",
    "schedule": "programmation",
    "publish": "publication Instagram",
}


class Wait(Exception):
    """L'étape n'est pas en échec mais doit être retentée plus tard (sans compter de tentative)."""

    def __init__(self, seconds: float, message: str = ""):
        super().__init__(message)
        self.seconds = seconds


def default_styles_dir() -> Path:
    return Path(__file__).resolve().parent.parent / "styles"


def load_style(settings: Settings, category: str) -> str:
    custom = settings.styles_dir / f"{category}.md"
    if custom.exists():
        return custom.read_text(encoding="utf-8")
    return (default_styles_dir() / f"{category}.md").read_text(encoding="utf-8")


class Pipeline:
    def __init__(self, settings: Settings, db: DB, instagram: Instagram | None = None):
        self.s = settings
        self.db = db
        self.ig = instagram or Instagram(settings)
        self._inflight: set[str] = set()
        self._edit_sem = asyncio.Semaphore(1)  # un seul téléphone : un montage à la fois
        self._other_sem = asyncio.Semaphore(3)
        self._tasks: set[asyncio.Task] = set()
        self._stop = asyncio.Event()
        # étapes remplaçables (tests)
        self.editor = None
        self.captioner = generate_caption

    # ------------------------------------------------------------------ boucle
    async def run_forever(self) -> None:
        log.info("Worker démarré")
        while not self._stop.is_set():
            try:
                await self.tick()
            except Exception:  # la boucle ne doit jamais mourir
                log.exception("Erreur dans la boucle du worker")
            try:
                await asyncio.wait_for(self._stop.wait(), timeout=self.s.poll_seconds)
            except asyncio.TimeoutError:
                pass

    def stop(self) -> None:
        self._stop.set()

    async def tick(self) -> None:
        self.scan_inbox()
        for video in self.db.due():
            if video["id"] in self._inflight:
                continue
            self._inflight.add(video["id"])
            task = asyncio.create_task(self._run(video["id"]))
            self._tasks.add(task)
            task.add_done_callback(self._tasks.discard)

    async def drain(self) -> None:
        """Attend la fin des étapes en cours (tests)."""
        while self._tasks:
            await asyncio.gather(*list(self._tasks), return_exceptions=True)

    async def _run(self, vid: str) -> None:
        try:
            video = self.db.get(vid)
            if not video:
                return
            sem = self._edit_sem if video["step"] == "edit" else self._other_sem
            async with sem:
                video = self.db.get(vid)  # relire après l'attente du sémaphore
                if not video or video["step"] == "done" or video["status"] == "error":
                    return
                await self._execute(video)
        finally:
            self._inflight.discard(vid)

    async def _execute(self, video: dict) -> None:
        vid, step = video["id"], video["step"]
        handler = getattr(self, f"step_{step}")
        self.db.update(vid, status=STEP_STATUS[step])
        try:
            await handler(video)
        except Wait as w:
            self.db.update(vid, status=STEP_WAITING_STATUS[step], next_attempt_at=time.time() + w.seconds)
            if str(w):
                self.db.log(vid, "info", str(w))
        except Exception as e:  # noqa: BLE001 - toute erreur d'étape est gérée par la relance
            log.exception("Étape %s en échec pour %s", step, vid)
            await self._fail(video, step, e)

    async def _fail(self, video: dict, step: str, exc: Exception) -> None:
        vid = video["id"]
        attempts = video["attempts"] + 1
        msg = f"{type(exc).__name__}: {exc}"[:1500]
        if attempts >= self.s.max_attempts:
            self.db.update(vid, status="error", attempts=attempts, last_error=msg)
            self.db.log(vid, "error", f"Échec définitif de l'étape {STEP_LABEL[step]} après {attempts} tentatives : {msg}")
            await notify(
                self.s,
                f"❌ Échec : {video['original_name']}",
                f"L'étape « {STEP_LABEL[step]} » a échoué {attempts} fois.\n{msg}\nRelance possible depuis le tableau de bord.",
                priority="high",
            )
            return
        delay = self.s.retry_base_seconds * (2 ** (attempts - 1))
        self.db.update(
            vid,
            status=STEP_WAITING_STATUS[step],
            attempts=attempts,
            last_error=msg,
            next_attempt_at=time.time() + delay,
        )
        self.db.log(vid, "warning", f"Échec {STEP_LABEL[step]} (tentative {attempts}/{self.s.max_attempts}), relance dans {delay}s : {msg}")
        if self.s.notify_on_retry:
            await notify(self.s, f"⚠️ Relance : {video['original_name']}", f"{STEP_LABEL[step]} : {msg}")

    def _advance(self, vid: str, step: str, status: str, **fields) -> None:
        self.db.update(vid, step=step, status=status, attempts=0, last_error=None, next_attempt_at=time.time(), **fields)

    # ------------------------------------------------------------------ étapes
    async def step_edit(self, video: dict) -> None:
        vid = video["id"]
        raw_name = Path(video["raw_path"]).name
        raw_url = file_url(self.s, "raw", vid, raw_name)
        music_url = file_url(self.s, "music", vid, Path(video["music_path"]).name) if video.get("music_path") else None
        if not self.s.public_base_url:
            raise RuntimeError("PUBLIC_BASE_URL manquant : CUTTINX ne peut pas télécharger le rush.")
        self.db.log(vid, "info", f"Montage lancé (mode {self.s.editor_mode})")

        def progress(m: str) -> None:
            self.db.log(vid, "info", m)

        if self.editor is not None:
            result = await self.editor(self.s, video, raw_url, music_url, progress)
        elif self.s.editor_mode == "direct":
            from .editor_direct import edit_direct

            result = await edit_direct(self.s, video, raw_url, music_url, progress)
        else:
            from .editor_claude import edit_with_claude

            style = load_style(self.s, video["category"])
            result = await edit_with_claude(self.s, video, raw_url, music_url, style, progress)

        self.db.log(vid, "info", f"Montage terminé : projet {result.get('project_id')} — {result.get('summary', '')}")
        self._advance(
            vid,
            "fetch_render",
            "awaiting_render",
            cuttinx_project_id=result.get("project_id"),
            cuttinx_render_job=result.get("render_job_id"),
            cuttinx_render_media_id=result.get("render_media_id"),
            render_download_url=result.get("download_url") or None,
            edit_summary=result.get("summary"),
            edit_feedback=None,
            render_wait_started=time.time(),
        )

    async def step_fetch_render(self, video: dict) -> None:
        vid = video["id"]
        path = video.get("rendered_path")
        if not (path and Path(path).exists()):
            if video.get("render_download_url"):
                path = str(await renders.download(self.s, vid, video["render_download_url"]))
                self.db.log(vid, "info", "MP4 téléchargé depuis CUTTINX")
            else:
                waited = time.time() - (video.get("render_wait_started") or time.time())
                if waited > self.s.render_wait_hours * 3600:
                    raise RuntimeError(
                        f"Aucun MP4 reçu après {self.s.render_wait_hours} h (synchronisation du téléphone inactive ?)."
                    )
                if waited < 5:
                    self.db.log(vid, "info", f"En attente du MP4 « HUB-{vid} » depuis le téléphone…")
                raise Wait(30)
        self._advance(vid, "caption", "rendered", rendered_path=path)
        self.db.log(vid, "info", "Vidéo rendue récupérée")

    async def step_caption(self, video: dict) -> None:
        vid = video["id"]
        cap = await self.captioner(self.s, video)
        self.db.log(vid, "info", f"Légende générée : {cap['hook']}")
        self._advance(vid, "schedule", "rendered", caption_hook=cap["hook"], caption_text=cap["description"], hashtags=cap["hashtags"])

    async def step_schedule(self, video: dict) -> None:
        vid = video["id"]
        if video["publish_mode"] != "scheduled":
            self._advance(vid, "publish", "publishing", scheduled_at=time.time())
            return
        slot = next_slot(self.s, self.db.taken_slots())
        ts = slot.timestamp()
        self.db.update(vid, step="publish", status="scheduled", attempts=0, last_error=None, scheduled_at=ts, next_attempt_at=ts)
        label = slot.strftime("%d/%m à %H:%M")
        self.db.log(vid, "info", f"Publication programmée le {label}")
        if self.s.notify_on_success:
            await notify(self.s, f"🗓️ Programmée : {video['original_name']}", f"Publication prévue le {label}.\n« {video.get('caption_hook') or ''} »")

    async def step_publish(self, video: dict) -> None:
        vid = video["id"]
        caption = build_caption(video.get("caption_hook") or "", video.get("caption_text") or "", video.get("hashtags") or [])
        if self.s.dry_run_publish:
            self.db.log(vid, "info", "DRY_RUN_PUBLISH : publication simulée.\n" + caption)
            self._finish(vid, "dry-run", None)
            await self._notify_published(video, None)
            return

        container = video.get("ig_container_id")
        if not container:
            rendered = Path(video["rendered_path"])
            if self.s.ig_upload_mode == "url":
                created = await self.ig.create_reel_container(caption, video_url=file_url(self.s, "render", vid, rendered.name))
            else:
                created = await self.ig.create_reel_container(caption)
                await self.ig.upload_video(created["id"], rendered, created.get("uri"))
            self.db.update(vid, ig_container_id=created["id"])
            self.db.log(vid, "info", f"Vidéo envoyée à Instagram (conteneur {created['id']}), traitement en cours")
            raise Wait(20)

        status = await self.ig.container_status(container)
        code = (status.get("status_code") or "").upper()
        if code in ("IN_PROGRESS", ""):
            raise Wait(20)
        if code in ("ERROR", "EXPIRED"):
            self.db.update(vid, ig_container_id=None)
            raise RuntimeError(f"Traitement Instagram {code} : {status.get('status')}")
        if code == "PUBLISHED":  # déjà publié (reprise après crash)
            self._finish(vid, video.get("ig_media_id") or container, video.get("ig_permalink"))
            return
        media_id = await self.ig.publish(container)
        permalink = await self.ig.permalink(media_id)
        self._finish(vid, media_id, permalink)
        await self._notify_published(video, permalink)

    def _finish(self, vid: str, media_id: str, permalink: str | None) -> None:
        self.db.update(vid, step="done", status="published", attempts=0, last_error=None, ig_media_id=media_id, ig_permalink=permalink)
        self.db.log(vid, "info", f"Publiée sur Instagram {permalink or ''}".strip())

    async def _notify_published(self, video: dict, permalink: str | None) -> None:
        if self.s.notify_on_success:
            await notify(self.s, f"✅ Publiée : {video['original_name']}", f"{video.get('caption_hook') or ''}\n{permalink or ''}")

    # ------------------------------------------------------------------ rendus entrants
    def attach_render(self, vid: str, src: Path, *, move: bool = True) -> bool:
        video = self.db.get(vid)
        if not video or video["status"] == "published":
            return False
        dst = renders.store_render(self.s, vid, src, move=move)
        self.db.update(vid, rendered_path=str(dst))
        self.db.log(vid, "info", f"MP4 reçu : {src.name}")
        if video["step"] == "fetch_render":
            self.db.update(vid, next_attempt_at=time.time())
        return True

    def scan_inbox(self) -> None:
        for path, vid in renders.scan_inbox(self.s):
            if vid and self.attach_render(vid, path):
                continue
            target = self.s.unassigned_dir / path.name
            shutil.move(str(path), target)
            log.info("Rendu non attribué : %s", path.name)

    # ------------------------------------------------------------------ actions du tableau de bord
    def retry(self, vid: str) -> None:
        video = self.db.get(vid)
        if not video or video["step"] == "done":
            return
        if video["step"] == "fetch_render":
            self.db.update(vid, render_wait_started=time.time())
        self.db.update(vid, status=STEP_WAITING_STATUS[video["step"]], attempts=0, next_attempt_at=time.time())
        self.db.log(vid, "info", "Relance manuelle")

    def reedit(self, vid: str, feedback: str) -> None:
        video = self.db.get(vid)
        if not video or video["status"] == "published":
            return
        if video.get("rendered_path"):
            Path(video["rendered_path"]).unlink(missing_ok=True)
        self.db.update(
            vid,
            step="edit",
            status="queued",
            attempts=0,
            last_error=None,
            next_attempt_at=time.time(),
            edit_feedback=feedback,
            rendered_path=None,
            render_download_url=None,
            ig_container_id=None,
            scheduled_at=None,
        )
        self.db.log(vid, "info", f"Nouveau montage demandé : {feedback}")

    def publish_now(self, vid: str) -> None:
        video = self.db.get(vid)
        if video and video["step"] == "publish" and video["status"] == "scheduled":
            self.db.update(vid, next_attempt_at=time.time(), scheduled_at=time.time())
            self.db.log(vid, "info", "Publication immédiate demandée")


def fmt_ts(settings: Settings, ts: float | None) -> str:
    if not ts:
        return ""
    return datetime.fromtimestamp(ts, ZoneInfo(settings.timezone)).strftime("%d/%m %H:%M")
