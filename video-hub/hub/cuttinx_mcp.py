"""Client MCP minimal vers le serveur CUTTINX (mode EDITOR_MODE=direct).

Le hub appelle lui-même les outils CUTTINX (import_from_url, find_highlights, analyze_music,
create_project, render_project, get_render...). En mode « claude », c'est l'API Claude qui se
connecte au même serveur MCP (voir editor_claude.py).
"""
from __future__ import annotations

import json
from contextlib import asynccontextmanager
from typing import Any, AsyncIterator

import httpx2
from mcp import ClientSession
from mcp.client.streamable_http import streamable_http_client

from .config import Settings


class CuttinxError(RuntimeError):
    pass


class Cuttinx:
    def __init__(self, session: ClientSession):
        self._session = session

    async def call(self, tool: str, **args: Any) -> Any:
        args = {k: v for k, v in args.items() if v is not None}
        result = await self._session.call_tool(tool, args, read_timeout_seconds=180)
        if getattr(result, "is_error", False) or getattr(result, "isError", False):
            raise CuttinxError(f"{tool}: {_text(result)}")
        structured = getattr(result, "structured_content", None) or getattr(result, "structuredContent", None)
        if structured:
            data = structured
        else:
            raw = _text(result)
            try:
                data = json.loads(raw)
            except ValueError:
                data = {"text": raw}
        # Commande longue : CUTTINX renvoie un command_id à relancer avec get_command_result
        for _ in range(10):
            cmd = find_key(data, "command_id") if isinstance(data, dict) else None
            if not cmd or find_key(data, "result") is not None:
                break
            data = await self.call("get_command_result", command_id=cmd, wait_seconds=90)
        return data


def _text(result: Any) -> str:
    parts = []
    for c in getattr(result, "content", []) or []:
        t = getattr(c, "text", None)
        if t:
            parts.append(t)
    return "\n".join(parts)


def find_key(data: Any, *keys: str) -> Any:
    """Cherche récursivement la première valeur d'une des clés (les réponses CUTTINX sont du JSON libre)."""
    if isinstance(data, dict):
        for k in keys:
            if k in data and data[k] not in (None, ""):
                return data[k]
        for v in data.values():
            found = find_key(v, *keys)
            if found is not None:
                return found
    elif isinstance(data, list):
        for v in data:
            found = find_key(v, *keys)
            if found is not None:
                return found
    return None


@asynccontextmanager
async def connect(settings: Settings) -> AsyncIterator[Cuttinx]:
    if not settings.cuttinx_mcp_url:
        raise CuttinxError("CUTTINX_MCP_URL n'est pas configuré.")
    headers = {}
    if settings.cuttinx_mcp_token:
        headers["Authorization"] = f"Bearer {settings.cuttinx_mcp_token}"
    http = httpx2.AsyncClient(headers=headers, timeout=httpx2.Timeout(30, read=300))
    async with http:
        async with streamable_http_client(settings.cuttinx_mcp_url, http_client=http) as (read, write):
            async with ClientSession(read, write) as session:
                await session.initialize()
                yield Cuttinx(session)
