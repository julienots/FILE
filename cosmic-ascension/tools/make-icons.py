#!/usr/bin/env python3
"""Génère l'icône (legacy + adaptive) et les écrans de démarrage Android de Cosmic Ascension.
Usage: python3 tools/make-icons.py   (depuis cosmic-ascension/, après `npx cap add android`)"""
import math, os, random
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.join(os.path.dirname(__file__), '..')
RES = os.path.join(ROOT, 'android/app/src/main/res')
S = 1024

def radial(size, inner, outer, cx=.5, cy=.5, r=.75):
    im = Image.new('RGB', (size, size)); px = im.load()
    for y in range(size):
        for x in range(size):
            d = min(1, math.hypot(x / size - cx, y / size - cy) / r)
            px[x, y] = tuple(int(inner[i] + (outer[i] - inner[i]) * d) for i in range(3))
    return im

def glow(size, xy, rad, color, alpha=200, blur=60):
    layer = Image.new('RGBA', (size, size), (0, 0, 0, 0)); d = ImageDraw.Draw(layer)
    x, y = xy; d.ellipse([x - rad, y - rad, x + rad, y + rad], fill=color + (alpha,))
    return layer.filter(ImageFilter.GaussianBlur(blur))

def background(size):
    bg = radial(size, (26, 14, 56), (2, 1, 8)).convert('RGBA')
    bg.alpha_composite(glow(size, (int(size * .3), int(size * .28)), size * .3, (255, 60, 200), 70, size * .12))
    bg.alpha_composite(glow(size, (int(size * .75), int(size * .7)), size * .32, (40, 120, 255), 80, size * .12))
    rnd = random.Random(7); d = ImageDraw.Draw(bg)
    for _ in range(int(size * .12)):
        x, y, r = rnd.random() * size, rnd.random() * size, rnd.random() * size * .004 + 1
        d.ellipse([x - r, y - r, x + r, y + r], fill=(255, 255, 255, rnd.randint(120, 255)))
    return bg

def star(d, cx, cy, R, r, color, points=5, rot=-math.pi / 2):
    pts = []
    for i in range(points * 2):
        a = rot + i * math.pi / points; rr = R if i % 2 == 0 else r
        pts.append((cx + math.cos(a) * rr, cy + math.sin(a) * rr))
    d.polygon(pts, fill=color)

def creature(size, scale=1.0):
    """Le héros (style sombre): orbe d'obsidienne, liseré néon, yeux fendus lumineux, antenne étoilée, anneau orbital. Pas de sourire."""
    im = Image.new('RGBA', (size, size), (0, 0, 0, 0)); c = size / 2; R = size * .26 * scale
    im.alpha_composite(glow(size, (c, c), R * 1.25, (63, 240, 255), 150, size * .05))
    ring = lambda front: _ring(size, c, R, front)
    im.alpha_composite(ring(False))
    d = ImageDraw.Draw(im)
    d.ellipse([c - R * 1.03, c - R * 1.03, c + R * 1.03, c + R * 1.03], fill=(63, 240, 255, 255))           # liseré néon
    k = R * .97
    body = radial(int(k * 2), (40, 34, 80), (5, 4, 14), .35, .3, .85).convert('RGBA')
    mask = Image.new('L', body.size, 0); ImageDraw.Draw(mask).ellipse([0, 0, body.size[0] - 1, body.size[1] - 1], fill=255)
    im.paste(body, (int(c - k), int(c - k)), mask)
    d = ImageDraw.Draw(im)
    d.line([(c + R * .15, c - R * .95), (c + R * .4, c - R * 1.5)], fill=(63, 240, 255, 255), width=int(R * .07))   # antenne
    star(d, c + R * .42, c - R * 1.55, R * .3, R * .12, (255, 204, 58, 255))
    eyes = Image.new('RGBA', (size, size), (0, 0, 0, 0)); e = ImageDraw.Draw(eyes)
    for s in (-1, 1):                                                                                   # yeux fendus
        ex, ey = c + s * R * .38, c - R * .02
        e.polygon([(ex + s * R * .27, ey - R * .16), (ex - s * R * .2, ey - R * .01), (ex - s * R * .17, ey + R * .12), (ex + s * R * .24, ey + R * .07)], fill=(120, 250, 255, 255))
    im.alpha_composite(eyes.filter(ImageFilter.GaussianBlur(size * .012))); im.alpha_composite(eyes)
    d.arc([c - R * .8, c - R * .8, c + R * .8, c + R * .8], 200, 250, fill=(160, 140, 255, 120), width=int(R * .06))  # reflet
    im.alpha_composite(ring(True))
    return im

def _ring(size, c, R, front):
    ring = Image.new('RGBA', (size, size), (0, 0, 0, 0)); rd = ImageDraw.Draw(ring)
    rd.ellipse([c - R * 1.8, c - R * .4 + R * .2, c + R * 1.8, c + R * .4 + R * .2], outline=(255, 60, 200, 255), width=int(R * .08))
    if front: rd.rectangle([0, 0, size, c + R * .2], fill=(0, 0, 0, 0)); rd.rectangle([c - R * .95, 0, c + R * .95, size], fill=(0, 0, 0, 0))
    else: rd.rectangle([0, c + R * .2, size, size], fill=(0, 0, 0, 0))
    g = ring.filter(ImageFilter.GaussianBlur(size * .01)); g.alpha_composite(ring)
    return g.rotate(-14, center=(c, c))

def save(im, path, size):
    os.makedirs(os.path.dirname(path), exist_ok=True); im.resize((size, size), Image.LANCZOS).save(path)

def main():
    bg, fg = background(S), creature(S, .95)
    full = bg.copy(); full.alpha_composite(fg)
    rmask = Image.new('L', (S, S), 0); ImageDraw.Draw(rmask).ellipse([0, 0, S - 1, S - 1], fill=255)
    rnd = Image.new('RGBA', (S, S), (0, 0, 0, 0)); rnd.paste(full, (0, 0), rmask)
    sq = Image.new('L', (S, S), 0); ImageDraw.Draw(sq).rounded_rectangle([0, 0, S - 1, S - 1], radius=S * .22, fill=255)
    legacy = Image.new('RGBA', (S, S), (0, 0, 0, 0)); legacy.paste(full, (0, 0), sq)
    fgA = Image.new('RGBA', (S, S), (0, 0, 0, 0)); fgA.alpha_composite(creature(S, .62))   # zone sûre adaptive (66 %)
    for dpi, n, a in [('mdpi', 48, 108), ('hdpi', 72, 162), ('xhdpi', 96, 216), ('xxhdpi', 144, 324), ('xxxhdpi', 192, 432)]:
        save(legacy, f'{RES}/mipmap-{dpi}/ic_launcher.png', n); save(rnd, f'{RES}/mipmap-{dpi}/ic_launcher_round.png', n)
        save(fgA, f'{RES}/mipmap-{dpi}/ic_launcher_foreground.png', a)
        bgA = background(a); bgA.save(f'{RES}/mipmap-{dpi}/ic_launcher_background.png')
    with open(f'{RES}/mipmap-anydpi-v26/ic_launcher.xml', 'w') as f:
        f.write('<?xml version="1.0" encoding="utf-8"?>\n<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n    <background android:drawable="@mipmap/ic_launcher_background"/>\n    <foreground android:drawable="@mipmap/ic_launcher_foreground"/>\n    <monochrome android:drawable="@mipmap/ic_launcher_foreground"/>\n</adaptive-icon>\n')
    with open(f'{RES}/mipmap-anydpi-v26/ic_launcher_round.xml', 'w') as f:
        f.write(open(f'{RES}/mipmap-anydpi-v26/ic_launcher.xml').read())
    # splash: fond étoilé + créature centrée, aux tailles existantes de chaque dossier drawable*
    for dname in os.listdir(RES):
        p = f'{RES}/{dname}/splash.png'
        if not os.path.exists(p): continue
        w, h = Image.open(p).size; m = max(w, h)
        sp = background(m).crop(((m - w) // 2, (m - h) // 2, (m - w) // 2 + w, (m - h) // 2 + h))
        k = int(min(w, h) * .5); cr = creature(k * 2, .9).resize((k, k), Image.LANCZOS)
        sp.alpha_composite(cr, ((w - k) // 2, (h - k) // 2)); sp.convert('RGB').save(p, optimize=True)
    save(full, os.path.join(ROOT, 'public/icon-512.png'), 512); save(full, os.path.join(ROOT, 'public/favicon.png'), 64)
    print('icônes + splash générés')

if __name__ == '__main__':
    main()
