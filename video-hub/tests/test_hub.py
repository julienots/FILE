"""Tests de bout en bout du pipeline avec CUTTINX, Claude et Instagram simulés."""
from __future__ import annotations

import asyncio
import time
from datetime import datetime
from pathlib import Path
from types import SimpleNamespace
from zoneinfo import ZoneInfo

import pytest
from fastapi.testclient import TestClient

from hub import editor_claude
from hub.config import Settings
from hub.db import DB
from hub.editor_direct import normal_recipe, performance_recipe
from hub.instagram import build_caption
from hub.main import create_app
from hub.pipeline import Pipeline
from hub.renders import match_video_id
from hub.scheduler import next_slot


@pytest.fixture
def settings(tmp_path: Path) -> Settings:
    s = Settings()
    s.data_dir = tmp_path / "data"
    s.public_base_url = "https://hub.test"
    s.hub_password = ""
    s.hub_secret = "secret"
    s.render_upload_token = "tok"
    s.render_inbox_dir = tmp_path / "inbox"
    s.retry_base_seconds = 0
    s.max_attempts = 2
    s.ig_user_id, s.ig_access_token = "1789", "token"
    s.ntfy_url = s.webhook_url = s.telegram_bot_token = ""
    s.ensure_dirs()
    return s


def make_video(s: Settings, db: DB, **kw) -> dict:
    raw = s.raw_dir / "rush.mp4"
    raw.write_bytes(b"raw")
    fields = dict(original_name="rush.mp4", category="performance", target_aspect="9:16", publish_mode="now", raw_path=str(raw))
    fields.update(kw)
    return db.create_video(**fields)


class FakeIG:
    def __init__(self):
        self.calls: list[str] = []
        self.status = iter(["IN_PROGRESS", "FINISHED"])

    async def create_reel_container(self, caption, video_url=None):
        self.calls.append("create")
        self.caption = caption
        return {"id": "C1", "uri": "https://rupload.test/C1"}

    async def upload_video(self, cid, path, uri=None):
        self.calls.append(f"upload:{Path(path).name}")
        return {"success": True}

    async def container_status(self, cid):
        return {"status_code": next(self.status)}

    async def publish(self, cid):
        self.calls.append("publish")
        return "M1"

    async def permalink(self, mid):
        return "https://instagram.com/reel/abc"


async def run_until(p: Pipeline, db: DB, vid: str, pred, rounds: int = 40) -> dict:
    for _ in range(rounds):
        db._conn.execute("UPDATE videos SET next_attempt_at=0 WHERE id=? AND status!='error'", (vid,))
        await p.tick()
        await p.drain()
        v = db.get(vid)
        if pred(v):
            return v
    raise AssertionError(f"état final inattendu : {db.get(vid)}")


def test_full_pipeline_with_inbox_render(settings):
    db = DB(settings.db_path)
    ig = FakeIG()
    p = Pipeline(settings, db, instagram=ig)  # type: ignore[arg-type]
    seen = {}

    async def fake_editor(s, video, raw_url, music_url, progress):
        seen["raw_url"] = raw_url
        progress("import_from_url, view_storyboard, create_project, render_project, get_render")
        return {"project_id": "prj1", "render_job_id": "job1", "render_media_id": "med1", "download_url": "", "summary": "Muscle-up au ralenti"}

    async def fake_caption(s, video):
        assert video["edit_summary"] == "Muscle-up au ralenti"
        return {"hook": "Tu tiens combien ? 🔥", "description": "Essaie et dis-moi.", "hashtags": ["streetworkout", "muscleup"]}

    p.editor, p.captioner = fake_editor, fake_caption
    v = make_video(settings, db)

    async def scenario():
        v1 = await run_until(p, db, v["id"], lambda x: x["step"] == "fetch_render")
        assert v1["status"] == "awaiting_render" and v1["cuttinx_project_id"] == "prj1"
        assert seen["raw_url"].startswith("https://hub.test/files/raw/")
        # Le téléphone synchronise le rendu dans le dossier entrant
        f = settings.render_inbox_dir / f"HUB-{v['id']} final.mp4"
        f.write_bytes(b"mp4")
        old = time.time() - 60
        import os

        os.utime(f, (old, old))
        return await run_until(p, db, v["id"], lambda x: x["status"] in ("published", "error"))

    final = asyncio.run(scenario())
    assert final["status"] == "published", db.events(v["id"])
    assert final["ig_permalink"] == "https://instagram.com/reel/abc"
    assert ig.calls == ["create", f"upload:{v['id']}.mp4", "publish"]
    assert "#muscleup" in ig.caption and ig.caption.startswith("Tu tiens combien ?")


def test_retry_then_error(settings):
    db = DB(settings.db_path)
    p = Pipeline(settings, db, instagram=FakeIG())  # type: ignore[arg-type]
    calls = {"n": 0}

    async def broken(*a):
        calls["n"] += 1
        raise RuntimeError("téléphone hors ligne")

    p.editor = broken
    v = make_video(settings, db)
    final = asyncio.run(run_until(p, db, v["id"], lambda x: x["status"] == "error"))
    assert calls["n"] == settings.max_attempts
    assert "téléphone hors ligne" in final["last_error"]
    p.retry(v["id"])
    assert db.get(v["id"])["status"] == "queued"


def test_scheduled_mode_and_dry_run(settings):
    settings.dry_run_publish = True
    db = DB(settings.db_path)
    p = Pipeline(settings, db)

    async def editor(*a):
        return {"project_id": "p", "render_job_id": "j", "render_media_id": "m", "download_url": "", "summary": "s"}

    async def cap(s, v):
        return {"hook": "h", "description": "d", "hashtags": []}

    p.editor, p.captioner = editor, cap
    v = make_video(settings, db, publish_mode="scheduled")
    # rendu envoyé à la main
    up = settings.unassigned_dir / "x.mp4"
    up.write_bytes(b"mp4")

    async def scenario():
        await run_until(p, db, v["id"], lambda x: x["step"] == "fetch_render")
        assert p.attach_render(v["id"], up)
        await p.tick()
        await p.drain()
        for _ in range(5):
            db._conn.execute("UPDATE videos SET next_attempt_at=0 WHERE id=? AND step!='publish'", (v["id"],))
            await p.tick()
            await p.drain()
        sched = db.get(v["id"])
        assert sched["status"] == "scheduled" and sched["scheduled_at"] > time.time()
        p.publish_now(v["id"])
        await p.tick()
        await p.drain()
        return db.get(v["id"])

    final = asyncio.run(scenario())
    assert final["status"] == "published"


def test_next_slot_respects_gap(settings):
    tz = ZoneInfo("Europe/Paris")
    now = datetime(2026, 11, 18, 10, 0, tzinfo=tz)  # mercredi
    first = next_slot(settings, [], now)
    assert (first.hour, first.minute) == (12, 30)
    second = next_slot(settings, [first.timestamp()], now)
    assert (second.hour, second.minute) == (18, 30)


def test_caption_limits():
    cap = build_caption("Hook", "x" * 3000, [f"tag{i}" for i in range(40)] + ["#Tag1", "tag1"])
    assert len(cap) <= 2200
    assert cap.count("#") == 30


def test_match_video_id():
    assert match_video_id("HUB-0123456789 final.mp4") == "0123456789"
    assert match_video_id("hub_abcdef0123.mp4") == "abcdef0123"
    assert match_video_id("Calisthenics.mp4") is None


def test_recipes():
    hl = [{"start": 3, "end": 6, "score": 0.51}, {"start": 21.75, "end": 24.75, "score": 0.52}]
    r = performance_recipe("med", 30, hl, {"media_id": "mus", "bpm": 120, "downbeats": [0, 2, 4, 6]})
    assert r["format"]["aspect"] == "9:16" and r["music"]["media_id"] == "mus"
    assert any("speed_ramp" in c for c in r["clips"])
    assert r["clips"][1]["transition_in"]["id"]
    n = normal_recipe("med", 29.6, [{"start": 8.5, "end": 9.14}, {"start": 28.44, "end": 29.6}], "16:9", None)
    assert [(c["start"], c["end"]) for c in n["clips"]] == [(0.0, 8.58), (9.06, 28.52)]


def test_api_upload_and_inbox(settings):
    app = create_app(settings, start_worker=False)
    with TestClient(app) as c:
        r = c.post(
            "/api/videos",
            files=[("files", ("perf 1.mp4", b"aaa", "video/mp4")), ("files", ("perf2.mp4", b"bbb", "video/mp4"))],
            data={"category": "performance", "publish_mode": "scheduled", "target_aspect": "16:9"},
        )
        assert r.status_code == 200, r.text
        created = r.json()["created"]
        assert len(created) == 2 and created[0]["target_aspect"] == "9:16"
        vid = created[0]["id"]
        # lien signé pour CUTTINX
        from hub.security import file_url

        url = file_url(settings, "raw", vid, "x.mp4").replace("https://hub.test", "")
        assert c.get(url).content == b"aaa"
        assert c.get(url.replace("/raw/", "/render/")).status_code == 404
        # envoi du rendu par l'automatisation du téléphone
        assert c.post("/api/renders/inbox", files={"file": ("a.mp4", b"m", "video/mp4")}, data={"token": "bad"}).status_code == 401
        r = c.post("/api/renders/inbox", files={"file": (f"HUB-{vid}.mp4", b"m", "video/mp4")}, headers={"X-Upload-Token": "tok"})
        assert r.json()["video_id"] == vid
        r = c.post("/api/renders/inbox", files={"file": ("autre.mp4", b"m", "video/mp4")}, data={"token": "tok"})
        assert r.json()["video_id"] is None
        state = c.get("/api/state").json()
        assert [u["name"] for u in state["unassigned"]] == ["autre.mp4"]
        assert c.post("/api/unassigned/autre.mp4/assign", json={"video_id": created[1]["id"]}).json()["ok"]
        assert c.get(f"/api/videos/{vid}").json()["video"]["has_render"]
        # styles
        assert "PERFORMANCE" in c.get("/api/styles/performance").json()["content"]
        c.put("/api/styles/normal", json={"content": "mon style"})
        assert c.get("/api/styles/normal").json()["content"] == "mon style"
        assert c.get("/").status_code == 200
        # légende modifiée à la main + nouveau montage demandé
        assert c.put(f"/api/videos/{vid}/caption", json={"hook": "H", "text": "T", "hashtags": ["a"]}).json()["ok"]
        assert c.post(f"/api/videos/{vid}/reedit", json={"feedback": "plus court"}).json()["ok"]
        detail = c.get(f"/api/videos/{vid}").json()
        assert detail["video"]["step"] == "edit" and not detail["video"]["has_render"]
        assert any("plus court" in e["message"] for e in detail["events"])


def test_auth_required(settings):
    settings.hub_password = "pw"
    app = create_app(settings, start_worker=False)
    with TestClient(app) as c:
        assert c.get("/api/state").status_code == 401
        assert c.get("/api/state", auth=("admin", "pw")).status_code == 200
        assert c.get("/healthz").status_code == 200


def test_claude_editor_loop(settings, monkeypatch):
    """Vérifie la boucle : reprise sur pause_turn puis lecture de report_result, et la forme de la requête."""
    settings.cuttinx_mcp_url = "https://cuttinx.test/mcp"
    settings.cuttinx_mcp_token = "ctok"
    requests: list[dict] = []
    responses = [
        SimpleNamespace(stop_reason="pause_turn", content=[SimpleNamespace(type="mcp_tool_use", name="view_storyboard")]),
        SimpleNamespace(
            stop_reason="tool_use",
            content=[
                SimpleNamespace(
                    type="tool_use",
                    name="report_result",
                    input={"success": True, "project_id": "p", "render_job_id": "j", "render_media_id": "m", "download_url": "", "summary": "ok"},
                )
            ],
        ),
    ]

    class Stream:
        def __init__(self, resp):
            self.resp = resp

        async def __aenter__(self):
            return self

        async def __aexit__(self, *a):
            return False

        async def get_final_message(self):
            return self.resp

    class FakeClient:
        def __init__(self, **kw):
            self.beta = SimpleNamespace(messages=SimpleNamespace(stream=self.stream))

        def stream(self, **kw):
            requests.append({**kw, "messages": list(kw["messages"])})
            return Stream(responses[len(requests) - 1])

    monkeypatch.setattr(editor_claude.anthropic, "AsyncAnthropic", FakeClient)
    video = {"id": "abc", "category": "performance", "target_aspect": "9:16", "notes": "", "edit_feedback": None}
    out = asyncio.run(editor_claude.edit_with_claude(settings, video, "https://hub.test/raw", None, "style"))
    assert out["project_id"] == "p"
    first = requests[0]
    assert first["mcp_servers"][0]["authorization_token"] == "ctok"
    assert first["tools"][0] == {"type": "mcp_toolset", "mcp_server_name": "cuttinx"}
    assert "mcp-client-2025-11-20" in first["betas"]
    assert "HUB-abc" in first["messages"][0]["content"]
    assert requests[1]["messages"][-1]["role"] == "assistant"  # reprise pause_turn sans message utilisateur ajouté
