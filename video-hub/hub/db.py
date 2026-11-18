"""Stockage SQLite : vidéos (file d'attente + état) et journal d'événements."""
from __future__ import annotations

import json
import sqlite3
import threading
import time
import uuid
from pathlib import Path
from typing import Any

# Statuts affichés dans le tableau de bord
STATUS_LABELS = {
    "queued": "En attente",
    "editing": "En montage",
    "awaiting_render": "Rendu en cours de récupération",
    "rendered": "Rendue",
    "captioning": "Texte en cours",
    "scheduled": "Programmée",
    "publishing": "Publication",
    "published": "Publiée",
    "error": "Erreur",
}

SCHEMA = """
CREATE TABLE IF NOT EXISTS videos (
    id TEXT PRIMARY KEY,
    original_name TEXT NOT NULL,
    category TEXT NOT NULL,              -- performance | normal
    target_aspect TEXT NOT NULL,         -- 9:16 | 16:9 | 4:5 | 1:1
    publish_mode TEXT NOT NULL,          -- now | scheduled
    notes TEXT NOT NULL DEFAULT '',
    raw_path TEXT NOT NULL,
    music_path TEXT,
    status TEXT NOT NULL,
    step TEXT NOT NULL,                  -- prochaine étape : edit | fetch_render | caption | schedule | publish | done
    attempts INTEGER NOT NULL DEFAULT 0,
    next_attempt_at REAL NOT NULL,
    last_error TEXT,
    edit_feedback TEXT,
    cuttinx_project_id TEXT,
    cuttinx_render_job TEXT,
    cuttinx_render_media_id TEXT,
    render_download_url TEXT,
    edit_summary TEXT,
    rendered_path TEXT,
    render_wait_started REAL,
    caption_hook TEXT,
    caption_text TEXT,
    hashtags TEXT,
    scheduled_at REAL,
    ig_container_id TEXT,
    ig_media_id TEXT,
    ig_permalink TEXT,
    created_at REAL NOT NULL,
    updated_at REAL NOT NULL
);
CREATE TABLE IF NOT EXISTS events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    video_id TEXT NOT NULL,
    at REAL NOT NULL,
    level TEXT NOT NULL,
    message TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_videos_due ON videos(step, next_attempt_at);
CREATE INDEX IF NOT EXISTS idx_events_video ON events(video_id, at);
"""


class DB:
    def __init__(self, path: Path):
        self.path = path
        self._lock = threading.RLock()
        self._conn = sqlite3.connect(str(path), check_same_thread=False, isolation_level=None)
        self._conn.row_factory = sqlite3.Row
        self._conn.execute("PRAGMA journal_mode=WAL")
        self._conn.executescript(SCHEMA)

    # --- vidéos -----------------------------------------------------------
    def create_video(self, **fields: Any) -> dict:
        now = time.time()
        vid = fields.pop("id", None) or uuid.uuid4().hex[:10]
        row = {
            "id": vid,
            "status": "queued",
            "step": "edit",
            "attempts": 0,
            "next_attempt_at": now,
            "created_at": now,
            "updated_at": now,
            **fields,
        }
        cols = ",".join(row)
        qs = ",".join("?" for _ in row)
        with self._lock:
            self._conn.execute(f"INSERT INTO videos ({cols}) VALUES ({qs})", list(row.values()))
        self.log(vid, "info", f"Vidéo déposée ({fields.get('category')}) : {fields.get('original_name')}")
        return self.get(vid)  # type: ignore[return-value]

    def get(self, vid: str) -> dict | None:
        with self._lock:
            r = self._conn.execute("SELECT * FROM videos WHERE id=?", (vid,)).fetchone()
        return _row(r) if r else None

    def update(self, vid: str, **fields: Any) -> dict:
        fields["updated_at"] = time.time()
        if "hashtags" in fields and isinstance(fields["hashtags"], list):
            fields["hashtags"] = json.dumps(fields["hashtags"], ensure_ascii=False)
        sets = ",".join(f"{k}=?" for k in fields)
        with self._lock:
            self._conn.execute(f"UPDATE videos SET {sets} WHERE id=?", [*fields.values(), vid])
        return self.get(vid)  # type: ignore[return-value]

    def delete(self, vid: str) -> None:
        with self._lock:
            self._conn.execute("DELETE FROM videos WHERE id=?", (vid,))
            self._conn.execute("DELETE FROM events WHERE video_id=?", (vid,))

    def list_videos(self, limit: int = 200) -> list[dict]:
        with self._lock:
            rows = self._conn.execute("SELECT * FROM videos ORDER BY created_at DESC LIMIT ?", (limit,)).fetchall()
        return [_row(r) for r in rows]

    def due(self, now: float | None = None) -> list[dict]:
        now = now or time.time()
        with self._lock:
            rows = self._conn.execute(
                "SELECT * FROM videos WHERE step!='done' AND status!='error' AND next_attempt_at<=? "
                "ORDER BY next_attempt_at ASC",
                (now,),
            ).fetchall()
        return [_row(r) for r in rows]

    def by_step(self, step: str) -> list[dict]:
        with self._lock:
            rows = self._conn.execute("SELECT * FROM videos WHERE step=?", (step,)).fetchall()
        return [_row(r) for r in rows]

    def taken_slots(self) -> list[float]:
        """Heures de publication déjà prises (programmées ou publiées récemment)."""
        with self._lock:
            rows = self._conn.execute(
                "SELECT scheduled_at FROM videos WHERE scheduled_at IS NOT NULL AND scheduled_at>?",
                (time.time() - 2 * 86400,),
            ).fetchall()
        return [r[0] for r in rows]

    # --- journal ----------------------------------------------------------
    def log(self, vid: str, level: str, message: str) -> None:
        with self._lock:
            self._conn.execute(
                "INSERT INTO events (video_id, at, level, message) VALUES (?,?,?,?)",
                (vid, time.time(), level, message[:4000]),
            )

    def events(self, vid: str, limit: int = 100) -> list[dict]:
        with self._lock:
            rows = self._conn.execute(
                "SELECT at, level, message FROM events WHERE video_id=? ORDER BY at DESC, id DESC LIMIT ?",
                (vid, limit),
            ).fetchall()
        return [dict(r) for r in rows]


def _row(r: sqlite3.Row) -> dict:
    d = dict(r)
    try:
        d["hashtags"] = json.loads(d["hashtags"]) if d.get("hashtags") else []
    except (TypeError, ValueError):
        d["hashtags"] = []
    d["status_label"] = STATUS_LABELS.get(d["status"], d["status"])
    return d
