#!/usr/bin/env python3
"""Génère les visuels d'ETERNAL GOD :
- img/superessence.png : le logo SuperEssence recadré depuis l'image fournie (écran de chargement)
- img/logo.png         : logo ETERNAL GOD (auréole + ailes stylisées + titre)
- res/mipmap-*/ic_launcher.png : icône de l'application
Usage : python3 tools/make_brand.py <image_superessence>
"""
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "app/src/main/assets/game")
RES = os.path.join(ROOT, "app/src/main/res")
FONT = os.path.join(GAME, "fonts/cormorant_600.ttf")

GOLD_HI = (255, 236, 170)
GOLD = (232, 182, 74)
GOLD_LO = (150, 98, 30)


def superessence(src):
    im = Image.open(src).convert("RGB")
    w, h = im.size
    # bornes du texte détectées automatiquement (pixels clairs)
    px = im.load()
    xs, ys = [], []
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            r, g, b = px[x, y]
            if r + g + b > 120:
                xs.append(x)
                ys.append(y)
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    pad_x, pad_y = 60, 50
    box = (max(0, x0 - pad_x), max(0, y0 - pad_y), min(w, x1 + pad_x), min(h, y1 + pad_y))
    crop = im.crop(box)
    crop.save(os.path.join(GAME, "img/superessence.png"), optimize=True)
    print("superessence crop", box, crop.size)


def gradient_gold(size, top=GOLD_HI, mid=GOLD, bot=GOLD_LO):
    w, h = size
    g = Image.new("RGB", size)
    d = ImageDraw.Draw(g)
    for y in range(h):
        t = y / max(1, h - 1)
        if t < 0.5:
            k = t / 0.5
            c = tuple(int(top[i] + (mid[i] - top[i]) * k) for i in range(3))
        else:
            k = (t - 0.5) / 0.5
            c = tuple(int(mid[i] + (bot[i] - mid[i]) * k) for i in range(3))
        d.line([(0, y), (w, y)], fill=c)
    return g


def wing_mask(size, cx, cy, scale, side):
    """Aile stylisée : une série de plumes en arc, du haut vers l'extérieur."""
    m = Image.new("L", size, 0)
    d = ImageDraw.Draw(m)
    n = 9
    for i in range(n):
        t = i / (n - 1)
        # angle de la plume : de presque horizontal (haut) à oblique vers le bas
        ang = math.radians(-18 + t * 62)
        length = scale * (1.0 - 0.52 * t)
        width = scale * (0.085 - 0.03 * t)
        base_x = cx + side * scale * (0.10 + 0.05 * t)
        base_y = cy - scale * 0.22 + t * scale * 0.34
        tip_x = base_x + side * math.cos(ang) * length
        tip_y = base_y + math.sin(ang) * length
        # plume = losange allongé légèrement courbé
        nx, ny = -math.sin(ang), math.cos(ang)
        mid_x = base_x + side * math.cos(ang) * length * 0.55
        mid_y = base_y + math.sin(ang) * length * 0.55
        poly = [
            (base_x, base_y),
            (mid_x + side * nx * width * 0.2, mid_y - ny * width),
            (tip_x, tip_y),
            (mid_x - side * nx * width * 0.2, mid_y + ny * width),
        ]
        d.polygon(poly, fill=255)
    return m


def emblem(size, bg=None):
    """Emblème : auréole dorée, ailes, et une lame de lumière verticale."""
    S = size
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0) if bg is None else bg)
    cx, cy = S / 2, S * 0.47
    # halo lumineux
    glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    r = S * 0.30
    gd.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(255, 210, 120, 90))
    glow = glow.filter(ImageFilter.GaussianBlur(S * 0.06))
    im.alpha_composite(glow)

    gold = gradient_gold((S, S)).convert("RGBA")
    mask = Image.new("L", (S, S), 0)
    md = ImageDraw.Draw(mask)
    # auréole (anneau)
    R, th = S * 0.235, S * 0.028
    md.ellipse([cx - R, cy - R - S * 0.06, cx + R, cy + R - S * 0.06], outline=255, width=int(th))
    # rayons fins de l'auréole
    for k in range(24):
        a = k / 24 * math.tau
        r0, r1 = R + th * 0.9, R + th * (2.6 if k % 2 == 0 else 1.8)
        md.line([(cx + math.cos(a) * r0, cy - S * 0.06 + math.sin(a) * r0),
                 (cx + math.cos(a) * r1, cy - S * 0.06 + math.sin(a) * r1)], fill=255, width=max(1, int(S * 0.006)))
    # ailes
    wl = wing_mask((S, S), cx, cy + S * 0.05, S * 0.42, -1)
    wr = wing_mask((S, S), cx, cy + S * 0.05, S * 0.42, 1)
    mask = Image.composite(Image.new("L", (S, S), 255), mask, wl)
    mask = Image.composite(Image.new("L", (S, S), 255), mask, wr)
    # lame / pilier de lumière (silhouette divine abstraite)
    md = ImageDraw.Draw(mask)
    top, bot = cy - S * 0.20, cy + S * 0.36
    md.polygon([(cx, top), (cx + S * 0.045, cy + S * 0.02), (cx, bot), (cx - S * 0.045, cy + S * 0.02)], fill=255)
    # découpe sombre au centre de la lame (effet gravé)
    md.polygon([(cx, top + S * 0.07), (cx + S * 0.012, cy + S * 0.02), (cx, bot - S * 0.09), (cx - S * 0.012, cy + S * 0.02)], fill=0)
    # ombre portée
    shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    shadow.putalpha(mask.filter(ImageFilter.GaussianBlur(S * 0.01)).point(lambda v: int(v * 0.6)))
    im.alpha_composite(shadow, (int(S * 0.008), int(S * 0.012)))
    layer = gold.copy()
    layer.putalpha(mask)
    im.alpha_composite(layer)
    # étincelle centrale
    sp = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sp)
    rr = S * 0.022
    sd.ellipse([cx - rr, cy + S * 0.02 - rr, cx + rr, cy + S * 0.02 + rr], fill=(255, 255, 240, 255))
    sp = sp.filter(ImageFilter.GaussianBlur(S * 0.008))
    im.alpha_composite(sp)
    return im


def logo():
    W, H = 1600, 900
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    em = emblem(560)
    im.alpha_composite(em, ((W - 560) // 2, 0))
    font = ImageFont.truetype(FONT, 190)
    text = "ETERNAL GOD"
    spacing = 26
    widths = [font.getbbox(c)[2] - font.getbbox(c)[0] if c != " " else 60 for c in text]
    total = sum(widths) + spacing * (len(text) - 1)
    x = (W - total) // 2
    y = 520
    mask = Image.new("L", (W, H), 0)
    md = ImageDraw.Draw(mask)
    for c, wdt in zip(text, widths):
        if c != " ":
            bb = font.getbbox(c)
            md.text((x - bb[0], y), c, font=font, fill=255)
        x += wdt + spacing
    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    shadow.putalpha(mask.filter(ImageFilter.GaussianBlur(6)).point(lambda v: int(v * 0.75)))
    im.alpha_composite(shadow, (5, 8))
    gold = gradient_gold((W, H), top=(255, 246, 205), mid=(240, 196, 96), bot=(160, 104, 34)).convert("RGBA")
    # le dégradé est recentré sur la hauteur du texte
    g2 = Image.new("RGBA", (W, H))
    band = gradient_gold((W, 200), top=(255, 246, 205), mid=(240, 196, 96), bot=(150, 96, 30)).convert("RGBA")
    g2.paste(band, (0, y + 30))
    g2.paste(band.crop((0, 0, W, 1)).resize((W, y + 30)), (0, 0))
    g2.putalpha(mask)
    im.alpha_composite(g2)
    # fines lignes ornementales
    d = ImageDraw.Draw(im)
    ly = 800
    d.line([(W * 0.30, ly), (W * 0.46, ly)], fill=(232, 182, 74, 200), width=3)
    d.line([(W * 0.54, ly), (W * 0.70, ly)], fill=(232, 182, 74, 200), width=3)
    d.polygon([(W / 2, ly - 12), (W / 2 + 12, ly), (W / 2, ly + 12), (W / 2 - 12, ly)], fill=(255, 226, 150, 255))
    im = im.crop(im.getbbox())
    im.save(os.path.join(GAME, "img/logo.png"), optimize=True)
    print("logo", im.size)


def icons():
    base = Image.new("RGBA", (1024, 1024), (0, 0, 0, 255))
    # fond : nuit céleste dégradée
    d = ImageDraw.Draw(base)
    for y in range(1024):
        t = y / 1023
        c = (int(18 + 20 * (1 - t)), int(16 + 14 * (1 - t)), int(34 + 30 * (1 - t)))
        d.line([(0, y), (1024, y)], fill=c + (255,))
    em = emblem(900)
    base.alpha_composite(em, (62, 90))
    base.convert("RGB").save(os.path.join(ROOT, "tools/icon-1024.png"))
    for name, px in [("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192)]:
        d = os.path.join(RES, f"mipmap-{name}")
        os.makedirs(d, exist_ok=True)
        base.resize((px, px), Image.LANCZOS).convert("RGB").save(os.path.join(d, "ic_launcher.png"))
    base.resize((192, 192), Image.LANCZOS).convert("RGB").save(os.path.join(GAME, "img/icon-192.png"))


if __name__ == "__main__":
    superessence(sys.argv[1])
    logo()
    icons()
