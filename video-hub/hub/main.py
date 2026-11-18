"""Application web : interface de dépôt, tableau de bord, API et worker du pipeline."""
from __future__ import annotations

import asyncio
import logging
import re
import secrets
import uuid
from contextlib import asynccontextmanager
from pathlib import Path

from fastapi import Depends, FastAPI, File, Form, Header, HTTPException, UploadFile
from fastapi.responses import FileResponse, HTMLResponse
from fastapi.security import HTTPBasic, HTTPBasicCredentials
from pydantic import BaseModel

from .config import Settings, settings as default_settings
from .db import DB
from .pipeline import Pipeline, fmt_ts, load_style
from .renders import VIDEO_EXT, list_unassigned, match_video_id
from .security import verify

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")


class Feedback(BaseModel):
    feedback: str


class CaptionEdit(BaseModel):
    hook: str
    text: str
    hashtags: list[str]


class Assign(BaseModel):
    video_id: str


class Style(BaseModel):
    content: str


STATIC = Path(__file__).resolve().parent / "static"
_SAFE = re.compile(r"[^A-Za-z0-9._-]+")


def safe_name(name: str) -> str:
    return _SAFE.sub("_", Path(name or "video").name)[:120] or "video"


def create_app(settings: Settings | None = None, *, start_worker: bool | None = None) -> FastAPI:
    s = settings or default_settings
    s.ensure_dirs()
    db = DB(s.db_path)
    pipeline = Pipeline(s, db)
    run_worker = s.worker_enabled if start_worker is None else start_worker

    @asynccontextmanager
    async def lifespan(app: FastAPI):
        task = asyncio.create_task(pipeline.run_forever()) if run_worker else None
        yield
        pipeline.stop()
        if task:
            await asyncio.gather(task, return_exceptions=True)

    app = FastAPI(title="Hub vidéo", lifespan=lifespan)
    app.state.settings, app.state.db, app.state.pipeline = s, db, pipeline
    basic = HTTPBasic(auto_error=False)

    def auth(creds: HTTPBasicCredentials | None = Depends(basic)) -> None:
        if not s.hub_password:
            return
        ok = creds is not None and secrets.compare_digest(creds.username, s.hub_username) and secrets.compare_digest(
            creds.password, s.hub_password
        )
        if not ok:
            raise HTTPException(401, "Authentification requise", headers={"WWW-Authenticate": 'Basic realm="hub"'})

    def get_video(vid: str) -> dict:
        v = db.get(vid)
        if not v:
            raise HTTPException(404, "Vidéo introuvable")
        return v

    async def save_upload(upload: UploadFile, dst: Path) -> None:
        size, limit = 0, s.max_upload_mb * 1024 * 1024
        with dst.open("wb") as f:
            while chunk := await upload.read(1 << 20):
                size += len(chunk)
                if size > limit:
                    f.close()
                    dst.unlink(missing_ok=True)
                    raise HTTPException(413, f"Fichier trop gros (> {s.max_upload_mb} Mo)")
                f.write(chunk)

    def view(v: dict) -> dict:
        return {
            **{k: v[k] for k in (
                "id", "original_name", "category", "target_aspect", "publish_mode", "notes", "status", "status_label",
                "step", "attempts", "last_error", "cuttinx_project_id", "edit_summary", "caption_hook", "caption_text",
                "hashtags", "ig_permalink",
            )},
            "created": fmt_ts(s, v["created_at"]),
            "scheduled": fmt_ts(s, v["scheduled_at"]),
            "has_render": bool(v.get("rendered_path")),
        }

    # ------------------------------------------------------------------ pages
    @app.get("/", response_class=HTMLResponse, dependencies=[Depends(auth)])
    def index() -> HTMLResponse:
        return HTMLResponse((STATIC / "index.html").read_text(encoding="utf-8"))

    @app.get("/healthz")
    def healthz() -> dict:
        return {"ok": True}

    # ------------------------------------------------------------------ API
    @app.get("/api/state", dependencies=[Depends(auth)])
    def state() -> dict:
        return {
            "videos": [view(v) for v in db.list_videos()],
            "unassigned": list_unassigned(s),
            "problems": s.problems(),
            "config": {
                "editor_mode": s.editor_mode,
                "ig_username": s.ig_username,
                "dry_run": s.dry_run_publish,
                "timezone": s.timezone,
                "peak_weekday": s.peak_hours_weekday,
                "peak_weekend": s.peak_hours_weekend,
            },
        }

    @app.post("/api/videos", dependencies=[Depends(auth)])
    async def upload_videos(
        files: list[UploadFile] = File(...),
        category: str = Form(...),
        target_aspect: str = Form("9:16"),
        publish_mode: str = Form("now"),
        notes: str = Form(""),
        music: UploadFile | None = File(None),
    ) -> dict:
        if category not in ("performance", "normal"):
            raise HTTPException(400, "Catégorie invalide")
        if publish_mode not in ("now", "scheduled"):
            raise HTTPException(400, "Mode de publication invalide")
        if category == "performance":
            target_aspect = "9:16"  # Reels
        if target_aspect not in ("9:16", "16:9", "4:5", "1:1"):
            raise HTTPException(400, "Format invalide")
        created = []
        for up in files:
            vid = uuid.uuid4().hex[:10]
            name = safe_name(up.filename or "video.mp4")
            raw = s.raw_dir / f"{vid}_{name}"
            await save_upload(up, raw)
            music_path = None
            if music is not None and music.filename:
                music_path = s.raw_dir / f"{vid}_music_{safe_name(music.filename)}"
                await music.seek(0)
                await save_upload(music, music_path)
            v = db.create_video(
                id=vid,
                original_name=up.filename or name,
                category=category,
                target_aspect=target_aspect,
                publish_mode=publish_mode,
                notes=notes.strip(),
                raw_path=str(raw),
                music_path=str(music_path) if music_path else None,
            )
            created.append(view(v))
        return {"created": created}

    @app.get("/api/videos/{vid}", dependencies=[Depends(auth)])
    def video_detail(vid: str) -> dict:
        v = get_video(vid)
        events = [{**e, "at": fmt_ts(s, e["at"])} for e in db.events(vid)]
        return {"video": view(v), "events": events}

    @app.post("/api/videos/{vid}/retry", dependencies=[Depends(auth)])
    def retry(vid: str) -> dict:
        get_video(vid)
        pipeline.retry(vid)
        return {"ok": True}

    @app.post("/api/videos/{vid}/reedit", dependencies=[Depends(auth)])
    def reedit(vid: str, body: Feedback) -> dict:
        v = get_video(vid)
        if v["status"] == "published":
            raise HTTPException(409, "Déjà publiée")
        pipeline.reedit(vid, body.feedback.strip() or "Refais un montage plus percutant.")
        return {"ok": True}

    @app.post("/api/videos/{vid}/publish-now", dependencies=[Depends(auth)])
    def publish_now(vid: str) -> dict:
        get_video(vid)
        pipeline.publish_now(vid)
        return {"ok": True}

    @app.put("/api/videos/{vid}/caption", dependencies=[Depends(auth)])
    def edit_caption(vid: str, body: CaptionEdit) -> dict:
        v = get_video(vid)
        if v["status"] in ("published", "publishing"):
            raise HTTPException(409, "Trop tard : publication en cours ou faite")
        db.update(vid, caption_hook=body.hook, caption_text=body.text, hashtags=body.hashtags)
        db.log(vid, "info", "Légende modifiée à la main")
        return {"ok": True}

    @app.delete("/api/videos/{vid}", dependencies=[Depends(auth)])
    def delete_video(vid: str) -> dict:
        v = get_video(vid)
        for key in ("raw_path", "music_path", "rendered_path"):
            if v.get(key):
                Path(v[key]).unlink(missing_ok=True)
        db.delete(vid)
        return {"ok": True}

    @app.post("/api/videos/{vid}/render", dependencies=[Depends(auth)])
    async def manual_render(vid: str, file: UploadFile = File(...)) -> dict:
        get_video(vid)
        tmp = s.unassigned_dir / f".upload_{vid}.mp4"
        await save_upload(file, tmp)
        if not pipeline.attach_render(vid, tmp):
            tmp.unlink(missing_ok=True)
            raise HTTPException(409, "Impossible de rattacher ce rendu")
        return {"ok": True}

    @app.post("/api/renders/inbox")
    async def render_inbox(
        file: UploadFile = File(...),
        video_id: str = Form(""),
        token: str = Form(""),
        short_side: int = Form(0),
        x_upload_token: str = Header(""),
    ) -> dict:
        """Point d'entrée pour l'appli Android / l'automatisation du téléphone (jeton RENDER_UPLOAD_TOKEN).

        Rattachement : `video_id` explicite, sinon « HUB-<id> » dans le nom, sinon la seule vidéo qui attend son rendu.
        Les brouillons (petit côté < 720 px ou « brouillon »/« draft » dans le nom) sont ignorés.
        """
        given = x_upload_token or token
        if not s.render_upload_token or not secrets.compare_digest(given, s.render_upload_token):
            raise HTTPException(401, "Jeton invalide")
        name = safe_name(file.filename or "render.mp4")
        if Path(name).suffix.lower() not in VIDEO_EXT:
            raise HTTPException(400, "Fichier vidéo attendu")
        if (0 < short_side < 720) or re.search(r"brouillon|draft", name, re.IGNORECASE):
            return {"ok": True, "video_id": None, "ignored": True, "note": "Brouillon ignoré."}
        vid = (video_id or match_video_id(name) or "").lower()
        if not vid:
            waiting = pipeline.waiting_for_render()
            if len(waiting) == 1:
                vid = waiting[0]
        tmp = s.unassigned_dir / name
        await save_upload(file, tmp)
        if vid and pipeline.attach_render(vid, tmp):
            return {"ok": True, "video_id": vid}
        return {"ok": True, "video_id": None, "note": "Rendu non attribué : rattachez-le depuis le tableau de bord."}

    @app.post("/api/unassigned/{name}/assign", dependencies=[Depends(auth)])
    def assign(name: str, body: Assign) -> dict:
        src = s.unassigned_dir / safe_name(name)
        if not src.exists():
            raise HTTPException(404, "Fichier introuvable")
        if not pipeline.attach_render(body.video_id, src):
            raise HTTPException(409, "Impossible de rattacher ce rendu")
        return {"ok": True}

    @app.delete("/api/unassigned/{name}", dependencies=[Depends(auth)])
    def delete_unassigned(name: str) -> dict:
        (s.unassigned_dir / safe_name(name)).unlink(missing_ok=True)
        return {"ok": True}

    @app.get("/api/styles/{category}", dependencies=[Depends(auth)])
    def get_style(category: str) -> dict:
        if category not in ("performance", "normal"):
            raise HTTPException(404)
        return {"content": load_style(s, category)}

    @app.put("/api/styles/{category}", dependencies=[Depends(auth)])
    def put_style(category: str, body: Style) -> dict:
        if category not in ("performance", "normal"):
            raise HTTPException(404)
        (s.styles_dir / f"{category}.md").write_text(body.content, encoding="utf-8")
        return {"ok": True}

    # ------------------------------------------------------------------ fichiers signés (CUTTINX / Instagram)
    @app.get("/files/{kind}/{vid}/{token}/{filename}")
    def signed_file(kind: str, vid: str, token: str, filename: str) -> FileResponse:
        if kind not in ("raw", "music", "render") or not verify(s, kind, vid, token):
            raise HTTPException(404)
        v = db.get(vid)
        path = v and v.get({"raw": "raw_path", "music": "music_path", "render": "rendered_path"}[kind])
        if not path or not Path(path).exists():
            raise HTTPException(404)
        media = "video/mp4" if kind != "music" else None
        return FileResponse(path, media_type=media, filename=Path(path).name)

    return app


app = create_app()
