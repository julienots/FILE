"""Couleurs nommées (français/anglais) et conversion. Python pur (sans bpy)."""
from __future__ import annotations

import re

# Couleurs en sRGB 0..1. Les noms sont normalisés sans accents.
NAMED = {
    "noir": (0.02, 0.02, 0.025), "black": (0.02, 0.02, 0.025),
    "blanc": (0.92, 0.92, 0.9), "white": (0.92, 0.92, 0.9),
    "gris": (0.4, 0.4, 0.42), "grey": (0.4, 0.4, 0.42), "gray": (0.4, 0.4, 0.42),
    "rouge": (0.75, 0.05, 0.04), "red": (0.75, 0.05, 0.04),
    "pourpre": (0.45, 0.04, 0.12), "cramoisi": (0.6, 0.03, 0.08), "crimson": (0.6, 0.03, 0.08),
    "vert": (0.12, 0.5, 0.12), "green": (0.12, 0.5, 0.12),
    "bleu": (0.06, 0.2, 0.75), "blue": (0.06, 0.2, 0.75),
    "jaune": (0.95, 0.8, 0.1), "yellow": (0.95, 0.8, 0.1),
    "orange": (0.95, 0.42, 0.05),
    "violet": (0.42, 0.12, 0.7), "purple": (0.42, 0.12, 0.7), "mauve": (0.6, 0.4, 0.75),
    "rose": (0.95, 0.45, 0.65), "pink": (0.95, 0.45, 0.65),
    "marron": (0.32, 0.18, 0.08), "brun": (0.32, 0.18, 0.08), "brown": (0.32, 0.18, 0.08),
    "beige": (0.8, 0.7, 0.55), "creme": (0.95, 0.9, 0.78), "cream": (0.95, 0.9, 0.78),
    "or": (1.0, 0.72, 0.25), "dore": (1.0, 0.72, 0.25), "gold": (1.0, 0.72, 0.25), "golden": (1.0, 0.72, 0.25),
    "argent": (0.85, 0.86, 0.88), "argente": (0.85, 0.86, 0.88), "silver": (0.85, 0.86, 0.88),
    "bronze": (0.7, 0.42, 0.2), "cuivre": (0.85, 0.45, 0.3), "copper": (0.85, 0.45, 0.3),
    "laiton": (0.85, 0.7, 0.35), "brass": (0.85, 0.7, 0.35),
    "fer": (0.45, 0.45, 0.47), "iron": (0.45, 0.45, 0.47), "acier": (0.6, 0.62, 0.65), "steel": (0.6, 0.62, 0.65),
    "cyan": (0.05, 0.75, 0.85), "turquoise": (0.1, 0.7, 0.65), "teal": (0.05, 0.45, 0.45),
    "emeraude": (0.05, 0.55, 0.3), "emerald": (0.05, 0.55, 0.3),
    "kaki": (0.45, 0.45, 0.25), "olive": (0.4, 0.42, 0.12),
    "sable": (0.85, 0.75, 0.55), "sand": (0.85, 0.75, 0.55),
    "ivoire": (0.95, 0.93, 0.85), "ivory": (0.95, 0.93, 0.85),
    "os": (0.88, 0.84, 0.72), "bone": (0.88, 0.84, 0.72),
    "chair": (0.85, 0.62, 0.5), "skin": (0.85, 0.62, 0.5), "peche": (0.95, 0.7, 0.55),
    "bordeaux": (0.35, 0.03, 0.08), "indigo": (0.2, 0.1, 0.5), "lavande": (0.7, 0.6, 0.9),
    "magenta": (0.85, 0.05, 0.6), "ambre": (1.0, 0.55, 0.05), "amber": (1.0, 0.55, 0.05),
    "bois": (0.45, 0.28, 0.14), "wood": (0.45, 0.28, 0.14),
    "pierre": (0.5, 0.48, 0.45), "stone": (0.5, 0.48, 0.45),
    "obsidienne": (0.03, 0.02, 0.05), "obsidian": (0.03, 0.02, 0.05),
    "sang": (0.45, 0.0, 0.02), "blood": (0.45, 0.0, 0.02),
    "glace": (0.7, 0.88, 1.0), "ice": (0.7, 0.88, 1.0),
    "lave": (1.0, 0.3, 0.02), "lava": (1.0, 0.3, 0.02),
    "feu": (1.0, 0.4, 0.05), "fire": (1.0, 0.4, 0.05),
}

ACCENTS = str.maketrans("àâäáãéèêëíìîïóòôöõúùûüçñœ", "aaaaaeeeeiiiiooooouuuucno")


def normalize(word: str) -> str:
    return word.lower().translate(ACCENTS).strip()


def srgb_to_linear(c: float) -> float:
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def linear_to_srgb(c: float) -> float:
    c = max(0.0, min(1.0, c))
    return c * 12.92 if c <= 0.0031308 else 1.055 * (c ** (1 / 2.4)) - 0.055


def parse_color(v) -> tuple | None:
    """Renvoie une couleur sRGB (r, g, b, a) 0..1, ou None si non reconnue."""
    if isinstance(v, (list, tuple)) and len(v) in (3, 4):
        try:
            vals = [float(x) for x in v]
        except (TypeError, ValueError):
            return None
        if max(vals) > 1.0:  # 0..255
            vals = [x / 255.0 for x in vals]
        vals = [max(0.0, min(1.0, x)) for x in vals]
        if len(vals) == 3:
            vals.append(1.0)
        return tuple(vals)
    if not isinstance(v, str):
        return None
    s = v.strip()
    m = re.fullmatch(r"#?([0-9a-fA-F]{6})([0-9a-fA-F]{2})?", s)
    if m:
        h = m.group(1)
        rgb = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
        a = int(m.group(2), 16) / 255.0 if m.group(2) else 1.0
        return (*rgb, a)
    n = normalize(s)
    # « rouge foncé », « bleu clair »
    words = n.replace("-", " ").split()
    base = None
    for w in words:
        t = w[:-1] if w.endswith("s") else w
        for cand in (w, t, t[:-1] if t.endswith("e") else t, t[:-2] if t.endswith("tte") else t,
                     t[:-2] if t.endswith("lle") else t, t[:-3] + "c" if t.endswith("che") else t):
            if cand in NAMED:
                base = NAMED[cand]
                break
    if base is None:
        return None
    r, g, b = base
    if any(w in ("fonce", "foncee", "sombre", "dark", "deep") for w in words):
        r, g, b = r * 0.45, g * 0.45, b * 0.45
    if any(w in ("clair", "claire", "pale", "light", "bright") for w in words):
        r, g, b = r + (1 - r) * 0.45, g + (1 - g) * 0.45, b + (1 - b) * 0.45
    return (r, g, b, 1.0)


def to_linear(c) -> tuple:
    """sRGB (r,g,b[,a]) -> linéaire (r,g,b,a) pour les sockets Blender."""
    r, g, b = c[0], c[1], c[2]
    a = c[3] if len(c) > 3 else 1.0
    return (srgb_to_linear(r), srgb_to_linear(g), srgb_to_linear(b), a)


def to_hex(c) -> str:
    return "#" + "".join(f"{int(round(max(0, min(1, x)) * 255)):02x}" for x in c[:3])
