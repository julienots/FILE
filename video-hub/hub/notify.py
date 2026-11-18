"""Notifications (ntfy, webhook Discord/Slack, Telegram). Une notification ne doit jamais casser le pipeline."""
from __future__ import annotations

import logging

import httpx

from .config import Settings

log = logging.getLogger("hub.notify")


async def notify(settings: Settings, title: str, message: str, *, priority: str = "default") -> None:
    async with httpx.AsyncClient(timeout=15) as client:
        if settings.ntfy_url:
            try:
                await client.post(
                    settings.ntfy_url,
                    content=message.encode(),
                    headers={"Title": title.encode("utf-8").decode("latin-1", "ignore"), "Priority": priority},
                )
            except httpx.HTTPError as e:
                log.warning("ntfy KO: %s", e)
        if settings.webhook_url:
            text = f"**{title}**\n{message}"
            try:
                # "content" = Discord, "text" = Slack / Mattermost / Google Chat
                await client.post(settings.webhook_url, json={"content": text[:1900], "text": text})
            except httpx.HTTPError as e:
                log.warning("webhook KO: %s", e)
        if settings.telegram_bot_token and settings.telegram_chat_id:
            try:
                await client.post(
                    f"https://api.telegram.org/bot{settings.telegram_bot_token}/sendMessage",
                    json={"chat_id": settings.telegram_chat_id, "text": f"{title}\n{message}"[:4000]},
                )
            except httpx.HTTPError as e:
                log.warning("telegram KO: %s", e)
