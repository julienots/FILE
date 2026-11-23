#!/usr/bin/env python3
"""Génère l'icône de lancement de THE LAST BLOCK (bloc noir aux yeux rouges) pour toutes les densités."""
import os
import sys
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIZES = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}


def rounded(draw, box, r, fill):
    draw.rounded_rectangle(box, radius=r, fill=fill)


def make(size=1024):
    S = size
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    # Fond : dégradé violet-noir avec halo rouge
    bg = Image.new("RGBA", (S, S))
    px = bg.load()
    for y in range(S):
        for x in range(S):
            dx, dy = (x - S * 0.5) / S, (y - S * 0.42) / S
            d = (dx * dx + dy * dy) ** 0.5
            glow = max(0.0, 1.0 - d * 2.2)
            r = int(14 + 120 * glow ** 2)
            g = int(8 + 6 * glow)
            b = int(26 + 30 * glow)
            px[x, y] = (r, g, b, 255)
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, S - 1, S - 1), radius=int(S * 0.22), fill=255)
    img.paste(bg, (0, 0), mask)

    d = ImageDraw.Draw(img)
    # Petits blocs colorés tombant en arrière-plan
    cols = [(0, 214, 255), (255, 203, 43), (170, 80, 255), (60, 230, 120)]
    for i, (cx, cy, c) in enumerate([(0.18, 0.16, 0), (0.80, 0.12, 1), (0.84, 0.80, 2), (0.14, 0.82, 3)]):
        s = S * 0.075
        x, y = S * cx, S * cy
        rounded(d, (x, y, x + s, y + s), int(s * 0.22), cols[c] + (200,))

    # Ombre du bloc
    sh = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(sh).rounded_rectangle((S * 0.25, S * 0.30, S * 0.79, S * 0.84), radius=int(S * 0.09), fill=(0, 0, 0, 200))
    sh = sh.filter(ImageFilter.GaussianBlur(S * 0.03))
    img.alpha_composite(sh)
    # Le bloc noir brillant
    x0, y0, x1, y1 = S * 0.23, S * 0.24, S * 0.77, S * 0.78
    rounded(d, (x0, y0, x1, y1), int(S * 0.09), (10, 10, 14, 255))
    rounded(d, (x0 + S * 0.02, y0 + S * 0.02, x1 - S * 0.02, y0 + S * 0.16), int(S * 0.07), (40, 40, 52, 255))
    # liseré rouge
    d.rounded_rectangle((x0, y0, x1, y1), radius=int(S * 0.09), outline=(255, 40, 50, 255), width=max(2, int(S * 0.012)))
    # Yeux rouges lumineux
    glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    for ex in (0.385, 0.615):
        gd.ellipse((S * (ex - 0.09), S * 0.39, S * (ex + 0.09), S * 0.57), fill=(255, 20, 30, 170))
    glow = glow.filter(ImageFilter.GaussianBlur(S * 0.035))
    img.alpha_composite(glow)
    d = ImageDraw.Draw(img)
    for ex in (0.385, 0.615):
        d.rectangle((S * (ex - 0.045), S * 0.43, S * (ex + 0.045), S * 0.52), fill=(255, 40, 40, 255))
        d.rectangle((S * (ex - 0.018), S * 0.445, S * (ex + 0.012), S * 0.475), fill=(255, 220, 200, 255))
    # Bouche géométrique en dents de scie
    pts = []
    n = 7
    for i in range(n + 1):
        x = S * (0.33 + 0.34 * i / n)
        y = S * (0.64 if i % 2 == 0 else 0.69)
        pts.append((x, y))
    d.line(pts, fill=(255, 50, 50, 255), width=max(2, int(S * 0.016)), joint="curve")
    return img


def main():
    out_res = os.path.join(ROOT, "android", "res")
    big = make(1024)
    for name, px in SIZES.items():
        p = os.path.join(out_res, f"mipmap-{name}")
        os.makedirs(p, exist_ok=True)
        big.resize((px, px), Image.LANCZOS).save(os.path.join(p, "ic_launcher.png"))
    os.makedirs(os.path.join(ROOT, "branding"), exist_ok=True)
    big.resize((512, 512), Image.LANCZOS).save(os.path.join(ROOT, "branding", "icon-512.png"))
    print("icônes générées")


if __name__ == "__main__":
    sys.exit(main())
