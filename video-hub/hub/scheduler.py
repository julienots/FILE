"""Choix des créneaux de forte audience pour la publication programmée."""
from __future__ import annotations

from datetime import datetime, timedelta
from zoneinfo import ZoneInfo

from .config import Settings


def _parse_hhmm(s: str) -> tuple[int, int]:
    h, m = s.split(":")
    return int(h), int(m)


def next_slot(settings: Settings, taken: list[float], now: datetime | None = None) -> datetime:
    """Prochain créneau d'audience (≥ 10 min dans le futur) espacé d'au moins MIN_GAP_MINUTES des autres."""
    tz = ZoneInfo(settings.timezone)
    now = (now or datetime.now(tz)).astimezone(tz)
    earliest = now + timedelta(minutes=10)
    gap = settings.min_gap_minutes * 60
    for day in range(0, 30):
        d = (now + timedelta(days=day)).date()
        hours = settings.peak_hours_weekend if d.weekday() >= 5 else settings.peak_hours_weekday
        for hhmm in sorted(hours):
            h, m = _parse_hhmm(hhmm)
            slot = datetime(d.year, d.month, d.day, h, m, tzinfo=tz)
            if slot < earliest:
                continue
            ts = slot.timestamp()
            if all(abs(ts - t) >= gap for t in taken):
                return slot
    return earliest  # aucune place sur 30 jours : publication au plus tôt
