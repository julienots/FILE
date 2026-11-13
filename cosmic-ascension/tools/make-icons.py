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
    bg = radial(size, (70, 40, 150), (8, 5, 30)).convert('RGBA')
    bg.alpha_composite(glow(size, (int(size * .3), int(size * .28)), size * .3, (255, 90, 180), 110, size * .12))
    bg.alpha_composite(glow(size, (int(size * .75), int(size * .7)), size * .32, (60, 140, 255), 120, size * .12))
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
    """Le héros « Grain d'Étoile »: boule lumineuse cyan, grands yeux, antenne étoilée, anneau orbital."""
    im = Image.new('RGBA', (size, size), (0, 0, 0, 0)); c = size / 2; R = size * .26 * scale
    im.alpha_composite(glow(size, (c, c + R * .05), R * 1.35, (127, 220, 255), 170, size * .05))
    d = ImageDraw.Draw(im)
    ring = Image.new('RGBA', (size, size), (0, 0, 0, 0)); rd = ImageDraw.Draw(ring)   # anneau (arrière)
    rd.ellipse([c - R * 1.75, c - R * .42 + R * .25, c + R * 1.75, c + R * .42 + R * .25], outline=(255, 215, 106, 255), width=int(R * .11))
    rd.rectangle([0, 0, size, c + R * .2], fill=(0, 0, 0, 0)); im.alpha_composite(ring.rotate(-14, center=(c, c)))
    d.ellipse([c - R, c - R * .95, c + R, c + R * 1.05], fill=(26, 15, 51, 255))                       # contour
    k = R * .93
    body = radial(int(k * 2), (230, 252, 255), (60, 170, 255), .38, .32, .9).convert('RGBA')
    mask = Image.new('L', body.size, 0); ImageDraw.Draw(mask).ellipse([0, 0, body.size[0] - 1, body.size[1] - 1], fill=255)
    im.paste(body, (int(c - k), int(c - k * .95 + R * .05)), mask)
    d = ImageDraw.Draw(im)
    d.line([(c + R * .1, c - R * .85), (c + R * .35, c - R * 1.45)], fill=(26, 15, 51, 255), width=int(R * .1))   # antenne
    star(d, c + R * .38, c - R * 1.5, R * .32, R * .14, (26, 15, 51, 255)); star(d, c + R * .38, c - R * 1.5, R * .25, R * .1, (255, 215, 106, 255))
    for s in (-1, 1):                                                                                   # yeux
        ex, ey = c + s * R * .36, c + R * .02
        d.ellipse([ex - R * .27, ey - R * .33, ex + R * .27, ey + R * .33], fill=(26, 15, 51, 255))
        d.ellipse([ex - R * .21, ey - R * .27, ex + R * .21, ey + R * .27], fill=(255, 255, 255, 255))
        d.ellipse([ex - R * .13 + s * R * .03, ey - R * .14, ex + R * .13 + s * R * .03, ey + R * .2], fill=(26, 15, 51, 255))
        d.ellipse([ex - R * .07, ey - R * .12, ex + R * .0, ey - R * .04], fill=(255, 255, 255, 255))
        d.ellipse([ex + s * R * .3 - R * .1, ey + R * .38, ex + s * R * .3 + R * .1, ey + R * .46], fill=(255, 140, 200, 160))  # joues
    d.arc([c - R * .14, c + R * .32, c + R * .14, c + R * .52], 20, 160, fill=(26, 15, 51, 255), width=int(R * .06))  # sourire
    d.ellipse([c - R * .62, c - R * .7, c - R * .3, c - R * .45], fill=(255, 255, 255, 170))                # reflet
    ring2 = Image.new('RGBA', (size, size), (0, 0, 0, 0)); rd = ImageDraw.Draw(ring2)                   # anneau (avant)
    rd.ellipse([c - R * 1.75, c - R * .42 + R * .25, c + R * 1.75, c + R * .42 + R * .25], outline=(255, 215, 106, 255), width=int(R * .11))
    rd.rectangle([0, c + R * .2, size, size], fill=(0, 0, 0, 0)); rd.rectangle([c - R * .9, 0, c + R * .9, size], fill=(0, 0, 0, 0))
    im.alpha_composite(ring2.rotate(-14, center=(c, c)))
    return im

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
