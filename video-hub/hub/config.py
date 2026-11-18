"""Configuration du hub : tout vient des variables d'environnement (jamais de secret dans le code)."""
from __future__ import annotations

import os
from dataclasses import dataclass, field
from pathlib import Path


def _env(name: str, default: str = "") -> str:
    return os.environ.get(name, default).strip()


def _env_int(name: str, default: int) -> int:
    raw = _env(name)
    return int(raw) if raw else default


def _env_bool(name: str, default: bool) -> bool:
    raw = _env(name).lower()
    if not raw:
        return default
    return raw in {"1", "true", "yes", "oui", "on"}


def _env_list(name: str, default: str) -> list[str]:
    return [x.strip() for x in _env(name, default).split(",") if x.strip()]


@dataclass
class Settings:
    # --- Hub ---
    data_dir: Path = field(default_factory=lambda: Path(_env("HUB_DATA_DIR", "./data")).resolve())
    public_base_url: str = field(default_factory=lambda: _env("PUBLIC_BASE_URL").rstrip("/"))
    hub_username: str = field(default_factory=lambda: _env("HUB_USERNAME", "admin"))
    hub_password: str = field(default_factory=lambda: _env("HUB_PASSWORD"))
    hub_secret: str = field(default_factory=lambda: _env("HUB_SECRET"))
    render_upload_token: str = field(default_factory=lambda: _env("RENDER_UPLOAD_TOKEN"))
    render_inbox_dir: Path | None = field(
        default_factory=lambda: Path(_env("RENDER_INBOX_DIR")).resolve() if _env("RENDER_INBOX_DIR") else None
    )
    render_wait_hours: int = field(default_factory=lambda: _env_int("RENDER_WAIT_HOURS", 12))
    max_upload_mb: int = field(default_factory=lambda: _env_int("MAX_UPLOAD_MB", 2048))

    # --- Pipeline ---
    max_attempts: int = field(default_factory=lambda: _env_int("MAX_ATTEMPTS", 4))
    retry_base_seconds: int = field(default_factory=lambda: _env_int("RETRY_BASE_SECONDS", 60))
    poll_seconds: int = field(default_factory=lambda: _env_int("WORKER_POLL_SECONDS", 5))
    worker_enabled: bool = field(default_factory=lambda: _env_bool("WORKER_ENABLED", True))

    # --- Montage ---
    editor_mode: str = field(default_factory=lambda: _env("EDITOR_MODE", "claude").lower())  # claude | direct
    cuttinx_mcp_url: str = field(default_factory=lambda: _env("CUTTINX_MCP_URL"))
    cuttinx_mcp_token: str = field(default_factory=lambda: _env("CUTTINX_MCP_TOKEN"))
    default_music_media_id: str = field(default_factory=lambda: _env("DEFAULT_MUSIC_MEDIA_ID"))
    render_resolution: int = field(default_factory=lambda: _env_int("RENDER_RESOLUTION", 1080))

    # --- Claude ---
    anthropic_api_key: str = field(default_factory=lambda: _env("ANTHROPIC_API_KEY"))
    claude_model: str = field(default_factory=lambda: _env("CLAUDE_MODEL", "claude-opus-5-5"))
    claude_edit_effort: str = field(default_factory=lambda: _env("CLAUDE_EDIT_EFFORT", "high"))
    claude_caption_effort: str = field(default_factory=lambda: _env("CLAUDE_CAPTION_EFFORT", "medium"))
    claude_fallbacks: bool = field(default_factory=lambda: _env_bool("CLAUDE_FALLBACKS", True))
    claude_max_continuations: int = field(default_factory=lambda: _env_int("CLAUDE_MAX_CONTINUATIONS", 12))

    # --- Instagram ---
    ig_user_id: str = field(default_factory=lambda: _env("IG_USER_ID"))
    ig_access_token: str = field(default_factory=lambda: _env("IG_ACCESS_TOKEN"))
    ig_graph_version: str = field(default_factory=lambda: _env("IG_GRAPH_VERSION", "v21.0"))
    ig_upload_mode: str = field(default_factory=lambda: _env("IG_UPLOAD_MODE", "resumable").lower())  # resumable | url
    ig_username: str = field(default_factory=lambda: _env("IG_USERNAME", "julien.blt.bs"))
    ig_share_to_feed: bool = field(default_factory=lambda: _env_bool("IG_SHARE_TO_FEED", True))
    dry_run_publish: bool = field(default_factory=lambda: _env_bool("DRY_RUN_PUBLISH", False))

    # --- Programmation ---
    timezone: str = field(default_factory=lambda: _env("TIMEZONE", "Europe/Paris"))
    peak_hours_weekday: list[str] = field(default_factory=lambda: _env_list("PEAK_HOURS_WEEKDAY", "12:30,18:30,21:00"))
    peak_hours_weekend: list[str] = field(default_factory=lambda: _env_list("PEAK_HOURS_WEEKEND", "11:00,18:00,20:30"))
    min_gap_minutes: int = field(default_factory=lambda: _env_int("MIN_GAP_MINUTES", 180))

    # --- Hashtags de base ---
    base_hashtags_performance: list[str] = field(
        default_factory=lambda: _env_list(
            "BASE_HASHTAGS_PERFORMANCE", "streetworkout,calisthenics,calisthenicsmotivation,bodyweight"
        )
    )
    base_hashtags_normal: list[str] = field(default_factory=lambda: _env_list("BASE_HASHTAGS_NORMAL", "vlog"))

    # --- Notifications ---
    ntfy_url: str = field(default_factory=lambda: _env("NTFY_URL"))
    webhook_url: str = field(default_factory=lambda: _env("NOTIFY_WEBHOOK_URL"))
    telegram_bot_token: str = field(default_factory=lambda: _env("TELEGRAM_BOT_TOKEN"))
    telegram_chat_id: str = field(default_factory=lambda: _env("TELEGRAM_CHAT_ID"))
    notify_on_retry: bool = field(default_factory=lambda: _env_bool("NOTIFY_ON_RETRY", False))
    notify_on_success: bool = field(default_factory=lambda: _env_bool("NOTIFY_ON_SUCCESS", True))

    @property
    def raw_dir(self) -> Path:
        return self.data_dir / "raw"

    @property
    def renders_dir(self) -> Path:
        return self.data_dir / "renders"

    @property
    def unassigned_dir(self) -> Path:
        return self.data_dir / "unassigned"

    @property
    def styles_dir(self) -> Path:
        return self.data_dir / "styles"

    @property
    def db_path(self) -> Path:
        return self.data_dir / "hub.sqlite3"

    def ensure_dirs(self) -> None:
        for d in (self.data_dir, self.raw_dir, self.renders_dir, self.unassigned_dir, self.styles_dir):
            d.mkdir(parents=True, exist_ok=True)
        if self.render_inbox_dir:
            self.render_inbox_dir.mkdir(parents=True, exist_ok=True)

    def problems(self) -> list[str]:
        """Liste lisible des réglages manquants (affichée dans le tableau de bord)."""
        out: list[str] = []
        if not self.hub_password:
            out.append("HUB_PASSWORD manquant : le tableau de bord n'est pas protégé.")
        if not self.hub_secret:
            out.append("HUB_SECRET manquant : les liens de téléchargement ne sont pas signés de façon sûre.")
        if not self.public_base_url:
            out.append("PUBLIC_BASE_URL manquant : CUTTINX ne peut pas télécharger les vidéos déposées.")
        if not self.cuttinx_mcp_url:
            out.append("CUTTINX_MCP_URL manquant : le montage automatique est impossible.")
        if not self.anthropic_api_key:
            out.append("ANTHROPIC_API_KEY manquant : ni montage Claude ni génération de légendes.")
        if not (self.ig_user_id and self.ig_access_token) and not self.dry_run_publish:
            out.append("IG_USER_ID / IG_ACCESS_TOKEN manquants : publication Instagram impossible.")
        if self.ig_upload_mode == "url" and not self.public_base_url:
            out.append("IG_UPLOAD_MODE=url exige PUBLIC_BASE_URL.")
        return out


settings = Settings()
