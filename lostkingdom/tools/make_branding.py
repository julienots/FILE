#!/usr/bin/env python3
"""LOST KINGDOM — branding images (launcher icons, key art logo, studio logo).

Procedural: crown + castle silhouette over a dusk gradient, Cinzel typography (OFL).
Output:
  android/src/main/res/mipmap-{mdpi..xxxhdpi}/ic_launcher.png  (48..192 px)
  branding/lost_kingdom_logo.png       (2048x1024, transparent)
  branding/superessence_studio.png     (1024x512, transparent)
  branding/key_art.png                 (1920x1080)
"""
import math, os, random
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
FONTS = os.path.join(HERE, "fonts")
random.seed(7)

def font(name, size):
    try:
        f = ImageFont.truetype(os.path.join(FONTS, name), size)
        if "wght" in name:
            try: f.set_variation_by_axes([800])
            except Exception: pass
        return f
    except OSError:
        return ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf", size)

def lerp(a, b, t): return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(len(a)))

def gradient(w, h, stops):
    img = Image.new("RGB", (w, h)); px = img.load()
    for y in range(h):
        t = y / (h - 1)
        for i in range(len(stops) - 1):
            if stops[i][0] <= t <= stops[i + 1][0]:
                c = lerp(stops[i][1], stops[i + 1][1], (t - stops[i][0]) / (stops[i + 1][0] - stops[i][0])); break
        for x in range(w): px[x, y] = c
    return img

def castle(draw, cx, base, s, col):
    """Castle silhouette: keep, towers with crenellations, spire."""
    def rect(x0, y0, x1, y1): draw.rectangle([cx + x0 * s, base - y1 * s, cx + x1 * s, base - y0 * s], fill=col)
    def crenel(x0, x1, y, n):
        wseg = (x1 - x0) / (2 * n - 1)
        for i in range(n): rect(x0 + 2 * i * wseg, y, x0 + (2 * i + 1) * wseg, y + 0.06)
    def spire(x, y, w, hgt): draw.polygon([(cx + (x - w) * s, base - y * s), (cx + (x + w) * s, base - y * s), (cx + x * s, base - (y + hgt) * s)], fill=col)
    rect(-0.55, 0, 0.55, 0.32); crenel(-0.55, 0.55, 0.32, 7)
    rect(-0.18, 0, 0.18, 0.62); spire(0, 0.62, 0.22, 0.38)
    for sx in (-1, 1):
        rect(sx * 0.42 - 0.1, 0, sx * 0.42 + 0.1, 0.5); spire(sx * 0.42, 0.5, 0.14, 0.24)
        rect(sx * 0.72 - 0.08, 0, sx * 0.72 + 0.08, 0.4); crenel(sx * 0.72 - 0.08, sx * 0.72 + 0.08, 0.4, 2)

def crown(draw, cx, cy, s, col, gem=(200, 40, 50)):
    pts = [(-1, 0.45), (-1, -0.35), (-0.55, 0.05), (0, -0.65), (0.55, 0.05), (1, -0.35), (1, 0.45)]
    draw.polygon([(cx + x * s, cy + y * s) for x, y in pts], fill=col)
    draw.rectangle([cx - s, cy + 0.45 * s, cx + s, cy + 0.65 * s], fill=col)
    for x, y in ((-1, -0.35), (0, -0.65), (1, -0.35)):
        r = 0.12 * s; draw.ellipse([cx + x * s - r, cy + y * s - r, cx + x * s + r, cy + y * s + r], fill=col)
    r = 0.13 * s; draw.ellipse([cx - r, cy + 0.2 * s - r, cx + r, cy + 0.2 * s + r], fill=gem)

GOLD = (232, 190, 102); GOLD_D = (150, 104, 40)

def icon(size=512):
    img = gradient(size, size, [(0, (24, 30, 64)), (0.55, (120, 64, 70)), (0.8, (230, 140, 70)), (1, (40, 26, 30))])
    d = ImageDraw.Draw(img)
    # stars
    for _ in range(40):
        x, y = random.random() * size, random.random() * size * 0.45
        r = random.random() * size * 0.004 + 0.5; d.ellipse([x - r, y - r, x + r, y + r], fill=(255, 240, 210))
    # sun disc behind castle
    glow = Image.new("L", (size, size), 0); gd = ImageDraw.Draw(glow)
    gd.ellipse([size * 0.25, size * 0.42, size * 0.75, size * 0.92], fill=255)
    glow = glow.filter(ImageFilter.GaussianBlur(size * 0.06))
    img = Image.composite(Image.new("RGB", (size, size), (255, 200, 120)), img, glow.point(lambda v: int(v * 0.6)))
    d = ImageDraw.Draw(img)
    # hills + castle silhouette
    d.polygon([(0, size * 0.86), (size * 0.3, size * 0.78), (size * 0.6, size * 0.83), (size, size * 0.76), (size, size), (0, size)], fill=(18, 14, 22))
    castle(d, size * 0.5, size * 0.82, size * 0.36, (18, 14, 22))
    # golden crown on top
    crown(d, size * 0.5, size * 0.24, size * 0.2, GOLD)
    # rounded-square mask with gold border
    mask = Image.new("L", (size, size), 0); md = ImageDraw.Draw(mask)
    md.rounded_rectangle([0, 0, size - 1, size - 1], radius=int(size * 0.22), fill=255)
    out = Image.new("RGBA", (size, size), (0, 0, 0, 0)); out.paste(img, (0, 0), mask)
    od = ImageDraw.Draw(out)
    od.rounded_rectangle([size * 0.02, size * 0.02, size * 0.98, size * 0.98], radius=int(size * 0.2), outline=GOLD, width=max(2, size // 48))
    return out

def text_center(d, cx, y, txt, f, fill, shadow=None, spacing=0):
    w = d.textlength(txt, font=f) + spacing * (len(txt) - 1)
    x = cx - w / 2
    for ch in txt:
        if shadow: d.text((x + 4, y + 5), ch, font=f, fill=shadow)
        d.text((x, y), ch, font=f, fill=fill)
        x += d.textlength(ch, font=f) + spacing

def logo(w=2048, h=1024):
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0)); d = ImageDraw.Draw(img)
    crown(d, w / 2, h * 0.22, h * 0.13, GOLD + (255,), (190, 30, 45, 255))
    size = int(h * 0.24)
    while True:  # shrink until the title (with tracking and swashes) fits in 86% of the width
        ft = font("CinzelDecorative-Bold.ttf", size)
        bb = d.textbbox((0, 0), "LOST KINGDOM", font=ft)
        if (bb[2] - bb[0]) + h * 0.01 * 11 <= w * 0.86 or size < 20: break
        size -= 4
    text_center(d, w / 2, h * 0.34, "LOST KINGDOM", ft, GOLD + (255,), (40, 20, 10, 200), spacing=int(h * 0.01))
    d.line([w * 0.2, h * 0.72, w * 0.8, h * 0.72], fill=GOLD_D + (255,), width=6)
    fs = font("Cinzel[wght].ttf", int(h * 0.06))
    text_center(d, w / 2, h * 0.76, "A world waiting to be discovered.", fs, (240, 228, 200, 255), (0, 0, 0, 160))
    return img

def studio(w=1024, h=512):
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0)); d = ImageDraw.Draw(img)
    # stylised essence drop / gem
    cx, cy, s = w / 2, h * 0.3, h * 0.2
    d.polygon([(cx, cy - s), (cx + s * 0.7, cy + s * 0.1), (cx, cy + s), (cx - s * 0.7, cy + s * 0.1)], fill=(120, 200, 255, 255))
    d.polygon([(cx, cy - s), (cx + s * 0.7, cy + s * 0.1), (cx, cy + s * 0.25)], fill=(200, 240, 255, 255))
    d.polygon([(cx, cy - s), (cx - s * 0.7, cy + s * 0.1), (cx, cy + s * 0.25)], fill=(80, 150, 230, 255))
    text_center(d, w / 2, h * 0.58, "SUPERESSENCE", font("Cinzel[wght].ttf", int(h * 0.15)), (240, 240, 250, 255), spacing=int(h * 0.015))
    text_center(d, w / 2, h * 0.8, "S T U D I O", font("Cinzel[wght].ttf", int(h * 0.07)), (150, 200, 240, 255))
    return img

def key_art(w=1920, h=1080):
    img = gradient(w, h, [(0, (20, 26, 58)), (0.5, (110, 60, 74)), (0.72, (238, 150, 80)), (1, (30, 20, 26))]).convert("RGBA")
    d = ImageDraw.Draw(img)
    for layer, (col, amp, base) in enumerate([((70, 50, 70), 0.08, 0.72), ((44, 30, 44), 0.06, 0.8), ((20, 14, 22), 0.05, 0.88)]):
        pts = [(0, h)]
        for x in range(0, w + 20, 20):
            y = h * (base - amp * (0.5 + 0.5 * math.sin(x * 0.004 + layer * 2) * math.cos(x * 0.0017 + layer)))
            pts.append((x, y))
        pts.append((w, h)); d.polygon(pts, fill=col)
        if layer == 1: castle(d, w * 0.84, h * 0.8, h * 0.24, col)
    lg = logo(1600, 800)
    img.alpha_composite(lg, (int((w - 1600) / 2), int(-h * 0.02)))
    f = font("Cinzel[wght].ttf", 30)
    text_center(d, w / 2, h - 60, "Created by SUPERESSENCE STUDIO  ·  Powered by AGR ENGINE", f, (240, 228, 200, 255))
    return img.convert("RGB")

def main():
    ic = icon(512)
    for dpi, px in (("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192)):
        p = os.path.join(ROOT, "android", "src", "main", "res", "mipmap-" + dpi)
        os.makedirs(p, exist_ok=True)
        ic.resize((px, px), Image.LANCZOS).save(os.path.join(p, "ic_launcher.png"), optimize=True)
    bd = os.path.join(ROOT, "branding"); os.makedirs(bd, exist_ok=True)
    ic.save(os.path.join(bd, "icon_512.png"))
    logo().save(os.path.join(bd, "lost_kingdom_logo.png"))
    studio().save(os.path.join(bd, "superessence_studio.png"))
    key_art().save(os.path.join(bd, "key_art.png"))
    print("branding written")

if __name__ == "__main__":
    main()
