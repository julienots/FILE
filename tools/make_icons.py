#!/usr/bin/env python3
"""NOCTÉA — génération du logo (croissant de lune + étoile) en vectoriel Android et en PNG."""
import math
import os

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RES = os.path.join(ROOT, "app", "src", "main", "res")

# Géométrie dans un viewport 108×108 (icône adaptative ; zone sûre = cercle de 66 au centre)
OUTER = (51.0, 56.0, 23.0)
INNER = (61.5, 47.0, 19.5)
STAR = (70.0, 41.5, 7.2)
DOT = (77.5, 60.0, 1.6)


def inside(p, c):
    return (p[0] - c[0]) ** 2 + (p[1] - c[1]) ** 2 < c[2] ** 2


def crescent_points(n=180):
    ox, oy, orr = OUTER
    ix, iy, ir = INNER
    outer = [(ox + orr * math.cos(2 * math.pi * k / n), oy + orr * math.sin(2 * math.pi * k / n)) for k in range(n)]
    inner = [(ix + ir * math.cos(2 * math.pi * k / n), iy + ir * math.sin(2 * math.pi * k / n)) for k in range(n)]
    # arc extérieur hors du disque intérieur, puis arc intérieur dans le disque extérieur (sens inverse)
    keep_o = [not inside(p, INNER) for p in outer]
    start = next(i for i in range(n) if keep_o[i] and not keep_o[i - 1])
    arc_o = []
    i = start
    while keep_o[i % n]:
        arc_o.append(outer[i % n]); i += 1
    keep_i = [inside(p, OUTER) for p in inner]
    # point intérieur le plus proche de la fin de l'arc extérieur, en remontant
    end = arc_o[-1]
    j = min((k for k in range(n) if keep_i[k]), key=lambda k: (inner[k][0] - end[0]) ** 2 + (inner[k][1] - end[1]) ** 2)
    arc_i = []
    while keep_i[j % n]:
        arc_i.append(inner[j % n]); j -= 1
    return arc_o + arc_i


def star_points(cx, cy, r, inner_ratio=0.26):
    pts = []
    for k in range(8):
        ang = -math.pi / 2 + k * math.pi / 4
        rr = r if k % 2 == 0 else r * inner_ratio
        pts.append((cx + rr * math.cos(ang), cy + rr * math.sin(ang)))
    return pts


def path(pts):
    return "M" + " L".join(f"{x:.2f},{y:.2f}" for x, y in pts) + " Z"


def circle_path(cx, cy, r):
    return (f"M{cx - r:.2f},{cy:.2f} a{r:.2f},{r:.2f} 0 1,0 {2 * r:.2f},0 "
            f"a{r:.2f},{r:.2f} 0 1,0 {-2 * r:.2f},0 Z")


def scaled(pts, s, off):
    return [((x - 54) * s + 54 + off[0], (y - 54) * s + 54 + off[1]) for x, y in pts]


VEC_HEAD = ('<vector xmlns:android="http://schemas.android.com/apk/res/android"\n'
            '    xmlns:aapt="http://schemas.android.com/aapt"\n'
            '    android:width="{w}dp" android:height="{w}dp"\n'
            '    android:viewportWidth="108" android:viewportHeight="108">\n')


def grad(x1, y1, x2, y2, stops):
    s = "".join(f'<item android:offset="{o}" android:color="{c}"/>' for o, c in stops)
    return (f'<aapt:attr name="android:fillColor"><gradient android:type="linear" '
            f'android:startX="{x1}" android:startY="{y1}" android:endX="{x2}" android:endY="{y2}">{s}</gradient></aapt:attr>')


def radial(cx, cy, r, stops):
    s = "".join(f'<item android:offset="{o}" android:color="{c}"/>' for o, c in stops)
    return (f'<aapt:attr name="android:fillColor"><gradient android:type="radial" '
            f'android:centerX="{cx}" android:centerY="{cy}" android:gradientRadius="{r}">{s}</gradient></aapt:attr>')


MOON_GRAD = [("0", "#FFF4D6"), ("0.55", "#F4D58D"), ("1", "#B9A8FF")]


def foreground_xml(s=1.0, w=108, glow=True):
    cres = scaled(crescent_points(), s, (0, 0))
    star = scaled(star_points(*STAR), s, (0, 0))
    dx, dy, dr = DOT
    d = scaled([(dx, dy)], s, (0, 0))[0]
    x = VEC_HEAD.format(w=w)
    if glow:
        x += f'  <path android:pathData="{circle_path(51 * s + 54 * (1 - s), 56 * s + 54 * (1 - s), 34 * s)}">{radial(51, 56, 34 * s, [("0", "#55F4D58D"), ("0.5", "#22B9A8FF"), ("1", "#00B9A8FF")])}</path>\n'
    x += f'  <path android:pathData="{path(cres)}">{grad(36, 36, 70, 78, MOON_GRAD)}</path>\n'
    x += f'  <path android:pathData="{path(star)}" android:fillColor="#FFF8E7"/>\n'
    x += f'  <path android:pathData="{circle_path(d[0], d[1], dr * s)}" android:fillColor="#CCEEF0FF"/>\n'
    x += "</vector>\n"
    return x


def background_xml():
    x = VEC_HEAD.format(w=108)
    x += f'  <path android:pathData="M0,0h108v108h-108z">{grad(0, 0, 108, 108, [("0", "#1C1F4A"), ("0.55", "#0B0E25"), ("1", "#1E1238")])}</path>\n'
    x += f'  <path android:pathData="{circle_path(54, 54, 60)}">{radial(40, 30, 70, [("0", "#332A2370"), ("1", "#00000000")])}</path>\n'
    for (sx, sy, r) in [(22, 26, 0.9), (86, 22, 0.7), (18, 74, 0.6), (90, 84, 0.9), (30, 92, 0.5), (80, 40, 0.5)]:
        x += f'  <path android:pathData="{circle_path(sx, sy, r)}" android:fillColor="#99EEF0FF"/>\n'
    x += "</vector>\n"
    return x


def mono_xml():
    cres = crescent_points()
    star = star_points(*STAR)
    x = VEC_HEAD.format(w=108)
    x += f'  <path android:pathData="{path(cres)}" android:fillColor="#FFFFFFFF"/>\n'
    x += f'  <path android:pathData="{path(star)}" android:fillColor="#FFFFFFFF"/>\n'
    x += "</vector>\n"
    return x


def notif_xml():
    cres = scaled(crescent_points(), 1.55, (4, -6))
    star = scaled(star_points(*STAR), 1.55, (4, -6))
    x = VEC_HEAD.format(w=24)
    x += f'  <path android:pathData="{path(cres)}" android:fillColor="#FFFFFFFF"/>\n'
    x += f'  <path android:pathData="{path(star)}" android:fillColor="#FFFFFFFF"/>\n'
    x += "</vector>\n"
    return x


def write(rel, content):
    p = os.path.join(RES, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    open(p, "w").write(content)


def render_png(path_out, size=1024, with_text=True):
    S = 4
    W = size * S
    H = int(size * (1.35 if with_text else 1.0)) * S
    img = Image.new("RGBA", (W, H))
    px = img.load()
    top, bot = (28, 31, 74), (11, 14, 37)
    for y in range(H):
        t = y / H
        c = tuple(int(top[i] * (1 - t) + bot[i] * t) for i in range(3))
        for x in range(0, W):
            px[x, y] = c + (255,)
    k = W / 108.0
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse([(51 - 30) * k, (56 - 30) * k, (51 + 30) * k, (56 + 30) * k], fill=(244, 213, 141, 70))
    glow = glow.filter(ImageFilter.GaussianBlur(W / 18))
    img = Image.alpha_composite(img, glow)
    # croissant en dégradé
    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).polygon([(x * k, y * k) for x, y in crescent_points(720)], fill=255)
    gradimg = Image.new("RGBA", (W, H))
    gp = gradimg.load()
    for y in range(0, H, 2):
        for x in range(0, W, 2):
            t = min(1, max(0, ((x / k - 36) + (y / k - 36)) / 76))
            if t < 0.55:
                u = t / 0.55; a, b = (255, 244, 214), (244, 213, 141)
            else:
                u = (t - 0.55) / 0.45; a, b = (244, 213, 141), (185, 168, 255)
            c = tuple(int(a[i] * (1 - u) + b[i] * u) for i in range(3)) + (255,)
            gp[x, y] = c; gp[x + 1, y] = c; gp[x, y + 1] = c; gp[x + 1, y + 1] = c
    img.paste(gradimg, (0, 0), mask)
    d = ImageDraw.Draw(img)
    d.polygon([(x * k, y * k) for x, y in star_points(*STAR)], fill=(255, 248, 231, 255))
    d.ellipse([(DOT[0] - DOT[2]) * k, (DOT[1] - DOT[2]) * k, (DOT[0] + DOT[2]) * k, (DOT[1] + DOT[2]) * k], fill=(238, 240, 255, 220))
    for (sx, sy, r) in [(22, 26, 0.9), (86, 22, 0.7), (18, 74, 0.6), (90, 84, 0.9), (80, 40, 0.5)]:
        d.ellipse([(sx - r) * k, (sy - r) * k, (sx + r) * k, (sy + r) * k], fill=(238, 240, 255, 150))
    if with_text:
        font = ImageFont.truetype(os.path.join(RES, "font", "cormorant_500.ttf"), int(W * 0.16))
        text = "NOCTÉA"
        spacing = W * 0.025
        widths = [d.textlength(ch, font=font) for ch in text]
        total = sum(widths) + spacing * (len(text) - 1)
        x = (W - total) / 2
        for ch, w in zip(text, widths):
            d.text((x, W * 0.98), ch, font=font, fill=(238, 240, 255, 255))
            x += w + spacing
        f2 = ImageFont.truetype(os.path.join(RES, "font", "nunito_400.ttf"), int(W * 0.032))
        sub = "RETROUVEZ LE CALME · RETROUVEZ LE SOMMEIL"
        tw = d.textlength(sub, font=f2)
        d.text(((W - tw) / 2, W * 1.2), sub, font=f2, fill=(185, 168, 255, 230))
    img = img.resize((size, int(H / S)), Image.LANCZOS)
    os.makedirs(os.path.dirname(path_out), exist_ok=True)
    img.save(path_out)


def main():
    write("drawable/ic_launcher_foreground.xml", foreground_xml(0.92))
    write("drawable/ic_launcher_background.xml", background_xml())
    write("drawable/ic_launcher_monochrome.xml", mono_xml())
    write("drawable/ic_splash_moon.xml", foreground_xml(0.8, w=288))
    write("drawable/ic_notification.xml", notif_xml())
    adaptive = ('<?xml version="1.0" encoding="utf-8"?>\n'
                '<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
                '    <background android:drawable="@drawable/ic_launcher_background"/>\n'
                '    <foreground android:drawable="@drawable/ic_launcher_foreground"/>\n'
                '    <monochrome android:drawable="@drawable/ic_launcher_monochrome"/>\n'
                '</adaptive-icon>\n')
    write("mipmap-anydpi-v26/ic_launcher.xml", adaptive)
    write("mipmap-anydpi-v26/ic_launcher_round.xml", adaptive)
    # Logo et icône Play Store en PNG
    render_png(os.path.join(ROOT, "branding", "noctea-logo.png"), 1024, True)
    render_png(os.path.join(ROOT, "branding", "noctea-icon-512.png"), 512, False)
    print("ok")


if __name__ == "__main__":
    main()
