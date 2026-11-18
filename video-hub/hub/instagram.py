"""Publication de Reels via l'API officielle Instagram Graph (compte pro/créateur relié à une page Facebook).

Flux : création du conteneur REELS -> envoi de la vidéo (upload « resumable » ou video_url publique)
-> attente du traitement (status_code = FINISHED) -> media_publish -> permalien.
"""
from __future__ import annotations

from pathlib import Path

import httpx

from .config import Settings


class InstagramError(RuntimeError):
    pass


class Instagram:
    def __init__(self, settings: Settings):
        self.s = settings
        self.base = f"https://graph.facebook.com/{settings.ig_graph_version}"

    async def _request(self, method: str, url: str, **kw) -> dict:
        async with httpx.AsyncClient(timeout=httpx.Timeout(60, read=600)) as client:
            r = await client.request(method, url, **kw)
        try:
            data = r.json()
        except ValueError:
            data = {"raw": r.text[:500]}
        if r.status_code >= 400 or (isinstance(data, dict) and "error" in data):
            err = data.get("error", data) if isinstance(data, dict) else data
            raise InstagramError(f"Graph API {r.status_code}: {err}")
        return data

    async def create_reel_container(self, caption: str, video_url: str | None = None) -> dict:
        params = {
            "media_type": "REELS",
            "caption": caption,
            "share_to_feed": "true" if self.s.ig_share_to_feed else "false",
            "access_token": self.s.ig_access_token,
        }
        if video_url:
            params["video_url"] = video_url
        else:
            params["upload_type"] = "resumable"
        return await self._request("POST", f"{self.base}/{self.s.ig_user_id}/media", data=params)

    async def upload_video(self, container_id: str, path: Path, upload_uri: str | None = None) -> dict:
        size = path.stat().st_size
        uri = upload_uri or f"https://rupload.facebook.com/ig-api-upload/{self.s.ig_graph_version}/{container_id}"
        headers = {
            "Authorization": f"OAuth {self.s.ig_access_token}",
            "offset": "0",
            "file_size": str(size),
        }
        return await self._request("POST", uri, headers=headers, content=path.read_bytes())

    async def container_status(self, container_id: str) -> dict:
        return await self._request(
            "GET",
            f"{self.base}/{container_id}",
            params={"fields": "status_code,status", "access_token": self.s.ig_access_token},
        )

    async def publish(self, container_id: str) -> str:
        data = await self._request(
            "POST",
            f"{self.base}/{self.s.ig_user_id}/media_publish",
            data={"creation_id": container_id, "access_token": self.s.ig_access_token},
        )
        return data["id"]

    async def permalink(self, media_id: str) -> str | None:
        try:
            data = await self._request(
                "GET",
                f"{self.base}/{media_id}",
                params={"fields": "permalink", "access_token": self.s.ig_access_token},
            )
            return data.get("permalink")
        except InstagramError:
            return None


def build_caption(hook: str, text: str, hashtags: list[str]) -> str:
    """Assemble la légende finale (limites Instagram : 2 200 caractères, 30 hashtags)."""
    tags: list[str] = []
    seen: set[str] = set()
    for t in hashtags:
        t = t.strip().lstrip("#").replace(" ", "")
        if t and t.lower() not in seen:
            seen.add(t.lower())
            tags.append("#" + t)
    tags = tags[:30]
    tag_line = " ".join(tags)
    body = f"{hook.strip()}\n\n{text.strip()}".strip()
    room = 2200 - len(tag_line) - 4
    if len(body) > room:
        body = body[: max(0, room - 1)].rstrip() + "…"
    return f"{body}\n.\n{tag_line}" if tag_line else body
