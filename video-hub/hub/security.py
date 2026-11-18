"""Liens de téléchargement signés (HMAC) pour que CUTTINX / Instagram récupèrent les fichiers sans mot de passe."""
from __future__ import annotations

import hashlib
import hmac
import secrets

from .config import Settings

_fallback_secret = secrets.token_hex(32)  # si HUB_SECRET absent : liens valides jusqu'au redémarrage


def _key(settings: Settings) -> bytes:
    return (settings.hub_secret or _fallback_secret).encode()


def sign(settings: Settings, kind: str, vid: str) -> str:
    return hmac.new(_key(settings), f"{kind}:{vid}".encode(), hashlib.sha256).hexdigest()[:32]


def verify(settings: Settings, kind: str, vid: str, token: str) -> bool:
    return hmac.compare_digest(sign(settings, kind, vid), token)


def file_url(settings: Settings, kind: str, vid: str, filename: str) -> str:
    """kind = raw | music | render"""
    return f"{settings.public_base_url}/files/{kind}/{vid}/{sign(settings, kind, vid)}/{filename}"
