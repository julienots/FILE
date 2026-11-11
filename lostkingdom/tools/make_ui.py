#!/usr/bin/env python3
"""LOST KINGDOM — UI art kit generator (Superessence Studio art direction).

Palette: dark tanned leather & ink, aged gold trim, parchment, oxblood red accents.
Everything is drawn procedurally at 4x then downsampled (LANCZOS) for clean anti-aliasing.
Output: assets/ui/ui.png (2048x2048 RGBA) + assets/ui/ui.txt ("name x y w h [border]").
"""
import math, os, random
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageChops

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "assets", "ui")
os.makedirs(OUT, exist_ok=True)
SS = 4  # supersampling

GOLD_D = (122, 86, 38)
GOLD = (201, 164, 92)
GOLD_L = (246, 220, 150)
INK = (24, 19, 15)
LEATHER = (46, 36, 28)
LEATHER_L = (68, 53, 40)
PARCH = (231, 214, 174)
PARCH_D = (176, 146, 98)
OX = (122, 32, 28)
IVORY = (244, 233, 205)

rng = random.Random(7)


def noise(w, h, scale, seed=0, octaves=4):
    r = np.random.RandomState(seed)
    out = np.zeros((h, w), np.float32)
    amp, tot = 1.0, 0
    for o in range(octaves):
        s = max(1, int(scale / (2 ** o)))
        g = r.rand(h // s + 2, w // s + 2).astype(np.float32)
        img = Image.fromarray((g * 255).astype(np.uint8)).resize((w + 2 * s, h + 2 * s), Image.BICUBIC)
        out += np.array(img, np.float32)[s:s + h, s:s + w] / 255.0 * amp
        tot += amp
        amp *= 0.5
    return out / tot


def canvas(w, h):
    return Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))


def down(img):
    return img.resize((img.width // SS, img.height // SS), Image.LANCZOS)


def tex_fill(w, h, c0, c1, scale=40, seed=1, grain=0.0):
    n = noise(w, h, scale, seed)
    if grain:
        n = n * (1 - grain) + np.random.RandomState(seed + 9).rand(h, w).astype(np.float32) * grain
    a = np.array(c0, np.float32)
    b = np.array(c1, np.float32)
    arr = a[None, None, :] * (1 - n[..., None]) + b[None, None, :] * n[..., None]
    return Image.fromarray(arr.clip(0, 255).astype(np.uint8), "RGB").convert("RGBA")


def rounded_mask(w, h, r):
    m = Image.new("L", (w, h), 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, w - 1, h - 1], r, fill=255)
    return m


def gold_stroke(d, shape_fn, width):
    """Draw a bevelled gold line: dark outer, mid, bright inner highlight."""
    shape_fn(d, GOLD_D, width)
    shape_fn(d, GOLD, max(1, int(width * 0.62)))
    shape_fn(d, GOLD_L, max(1, int(width * 0.22)))


def filigree_corner(d, cx, cy, sx, sy, s):
    """Ornamental corner: diamond + scroll curls, mirrored by (sx, sy)."""
    def P(x, y):
        return (cx + x * sx * s, cy + y * sy * s)
    # diamond
    dia = [P(0, -6), P(6, 0), P(0, 6), P(-6, 0)]
    d.polygon(dia, fill=GOLD_D)
    dia2 = [P(0, -4.2), P(4.2, 0), P(0, 4.2), P(-4.2, 0)]
    d.polygon(dia2, fill=GOLD)
    d.polygon([P(0, -2), P(2, 0), P(0, 2), P(-2, 0)], fill=GOLD_L)
    # curls along both edges
    for axis in (0, 1):
        pts = []
        for i in range(30):
            t = i / 29
            ang = t * 4.2
            r = 7 * (1 - t) + 1.5
            x = 8 + t * 16 + math.cos(ang) * r * 0.4
            y = math.sin(ang) * r * 0.55
            pts.append(P(x, y) if axis == 0 else P(y, x))
        d.line(pts, fill=GOLD_D, width=int(3.2 * s / 4 * SS / SS * 4))
        d.line(pts, fill=GOLD, width=int(1.8 * s / 4 * SS / SS * 4))
        end = pts[-1]
        d.ellipse([end[0] - s * 1.6, end[1] - s * 1.6, end[0] + s * 1.6, end[1] + s * 1.6], fill=GOLD)


def panel(w, h, border, dark=True, ornate=True, seed=3):
    W, H = w * SS, h * SS
    base = tex_fill(W, H, LEATHER if dark else PARCH_D, LEATHER_L if dark else PARCH, scale=120, seed=seed, grain=0.12)
    if not dark:
        # parchment: darker scorched edges
        e = np.zeros((H, W), np.float32)
        yy, xx = np.mgrid[0:H, 0:W]
        dx = np.minimum(xx, W - 1 - xx) / (border * SS)
        dy = np.minimum(yy, H - 1 - yy) / (border * SS)
        edge = np.clip(1 - np.minimum(dx, dy), 0, 1) ** 2
        arr = np.array(base, np.float32)
        arr[..., :3] *= (1 - edge[..., None] * 0.45)
        base = Image.fromarray(arr.clip(0, 255).astype(np.uint8), "RGBA")
    mask = rounded_mask(W, H, int(10 * SS))
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    img.paste(base, (0, 0), mask)
    d = ImageDraw.Draw(img)
    # inner shadow / bevel
    inset = int(border * 0.32 * SS)
    for i in range(6):
        a = int(70 * (1 - i / 6))
        d.rounded_rectangle([i * SS, i * SS, W - 1 - i * SS, H - 1 - i * SS], int(10 * SS), outline=(0, 0, 0, a), width=SS)
    def rect_shape(dd, col, wd):
        dd.rounded_rectangle([inset, inset, W - 1 - inset, H - 1 - inset], int(6 * SS), outline=col, width=wd)
    gold_stroke(d, rect_shape, int(2.6 * SS))
    # thin second line
    inset2 = inset + int(5 * SS)
    d.rounded_rectangle([inset2, inset2, W - 1 - inset2, H - 1 - inset2], int(4 * SS), outline=GOLD_D + (180,), width=int(1 * SS))
    if ornate:
        s = SS * border / 40 * 1.0
        c = inset + int(1 * SS)
        for (cx, cy, sx, sy) in [(c, c, 1, 1), (W - 1 - c, c, -1, 1), (c, H - 1 - c, 1, -1), (W - 1 - c, H - 1 - c, -1, -1)]:
            filigree_corner(d, cx, cy, sx, sy, s * 1.4)
    return down(img)


def button(w, h, hi=False, red=True):
    W, H = w * SS, h * SS
    c0, c1 = ((92, 30, 26), (150, 52, 40)) if red else ((40, 44, 50), (70, 76, 86))
    if hi:
        c0 = tuple(min(255, int(c * 1.35)) for c in c0)
        c1 = tuple(min(255, int(c * 1.35)) for c in c1)
    base = tex_fill(W, H, c0, c1, 90, seed=11 + hi, grain=0.08)
    # vertical gradient sheen
    arr = np.array(base, np.float32)
    g = np.linspace(1.25, 0.7, H)[:, None, None]
    arr[..., :3] *= g
    base = Image.fromarray(arr.clip(0, 255).astype(np.uint8), "RGBA")
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    img.paste(base, (0, 0), rounded_mask(W, H, int(12 * SS)))
    d = ImageDraw.Draw(img)
    def shp(dd, col, wd):
        dd.rounded_rectangle([2 * SS, 2 * SS, W - 1 - 2 * SS, H - 1 - 2 * SS], int(11 * SS), outline=col, width=wd)
    gold_stroke(d, shp, int(3.2 * SS))
    d.line([(16 * SS, 7 * SS), (W - 16 * SS, 7 * SS)], fill=(255, 240, 210, 60), width=int(2 * SS))
    return down(img)


def slot(s, sel=False):
    S_ = s * SS
    base = tex_fill(S_, S_, (18, 15, 13), (34, 28, 23), 60, seed=21, grain=0.1)
    img = Image.new("RGBA", (S_, S_), (0, 0, 0, 0))
    img.paste(base, (0, 0), rounded_mask(S_, S_, int(8 * SS)))
    d = ImageDraw.Draw(img)
    for i in range(8):
        d.rounded_rectangle([i * SS // 2 + 3 * SS, i * SS // 2 + 3 * SS, S_ - 3 * SS - i * SS // 2, S_ - 3 * SS - i * SS // 2], int(6 * SS), outline=(0, 0, 0, 40), width=SS)
    col = GOLD_L if sel else GOLD_D
    def shp(dd, c, wd):
        dd.rounded_rectangle([SS, SS, S_ - 1 - SS, S_ - 1 - SS], int(8 * SS), outline=c, width=wd)
    if sel:
        gold_stroke(d, shp, int(3.5 * SS))
    else:
        shp(d, GOLD_D, int(2 * SS))
        shp(d, (150, 116, 60), int(1 * SS))
    return down(img)


def round_btn(s, hi=False, tint=(30, 26, 24)):
    S_ = s * SS
    img = Image.new("RGBA", (S_, S_), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # drop shadow
    sh = Image.new("RGBA", (S_, S_), (0, 0, 0, 0))
    ImageDraw.Draw(sh).ellipse([6 * SS, 9 * SS, S_ - 4 * SS, S_ - 1 * SS], fill=(0, 0, 0, 110))
    sh = sh.filter(ImageFilter.GaussianBlur(4 * SS))
    img.alpha_composite(sh)
    # glassy dark orb
    yy, xx = np.mgrid[0:S_, 0:S_].astype(np.float32)
    c = S_ / 2
    r = (S_ / 2 - 6 * SS)
    dist = np.sqrt((xx - c) ** 2 + (yy - c) ** 2) / r
    top = np.clip(1 - np.sqrt((xx - c) ** 2 + (yy - c * 0.62) ** 2) / (r * 0.9), 0, 1)
    k = 1.35 if hi else 1.0
    col = np.zeros((S_, S_, 4), np.float32)
    for i in range(3):
        col[..., i] = (tint[i] * (1.2 - dist * 0.5) + top * 70) * k
    col[..., 3] = (dist <= 1.0) * 235
    orb = Image.fromarray(col.clip(0, 255).astype(np.uint8), "RGBA")
    img.alpha_composite(orb)
    def shp(dd, cc, wd):
        dd.ellipse([6 * SS, 6 * SS, S_ - 6 * SS, S_ - 6 * SS], outline=cc, width=wd)
    gold_stroke(d, shp, int(5 * SS))
    # four small rivets
    for a in range(4):
        ang = a * math.pi / 2 + math.pi / 4
        px, py = c + math.cos(ang) * (r + 0.5 * SS), c + math.sin(ang) * (r + 0.5 * SS)
        d.ellipse([px - 2.4 * SS, py - 2.4 * SS, px + 2.4 * SS, py + 2.4 * SS], fill=GOLD_L)
    return down(img)


def bar_frame(w, h):
    W, H = w * SS, h * SS
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, W - 1, H - 1], H // 2, fill=(14, 11, 9, 230))
    def shp(dd, c, wd):
        dd.rounded_rectangle([SS, SS, W - 1 - SS, H - 1 - SS], H // 2 - SS, outline=c, width=wd)
    gold_stroke(d, shp, int(3 * SS))
    return down(img)


def bar_fill(w, h):
    W, H = w * SS, h * SS
    yy = np.linspace(0, 1, H)
    lum = 0.75 + 0.35 * np.exp(-((yy - 0.28) ** 2) / 0.02) - 0.25 * yy
    arr = np.zeros((H, W, 4), np.float32)
    n = noise(W, H, 30, 33) * 0.12 + 0.94
    arr[..., 0] = arr[..., 1] = arr[..., 2] = (lum[:, None] * n * 255)
    arr[..., 3] = 255
    img = Image.fromarray(arr.clip(0, 255).astype(np.uint8), "RGBA")
    m = rounded_mask(W, H, H // 2)
    out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    out.paste(img, (0, 0), m)
    return down(out)


def joystick_base(s):
    S_ = s * SS
    img = Image.new("RGBA", (S_, S_), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = S_ / 2
    d.ellipse([4 * SS, 4 * SS, S_ - 4 * SS, S_ - 4 * SS], fill=(10, 8, 6, 120))
    def shp(dd, cc, wd):
        dd.ellipse([6 * SS, 6 * SS, S_ - 6 * SS, S_ - 6 * SS], outline=cc, width=wd)
    gold_stroke(d, shp, int(3 * SS))
    # rune ticks
    for i in range(24):
        a = i * math.tau / 24
        r0, r1 = S_ / 2 - 16 * SS, S_ / 2 - (22 if i % 6 == 0 else 19) * SS
        d.line([(c + math.cos(a) * r0, c + math.sin(a) * r0), (c + math.cos(a) * r1, c + math.sin(a) * r1)], fill=GOLD + (150,), width=int(1.6 * SS))
    # direction chevrons
    for i in range(4):
        a = i * math.pi / 2 - math.pi / 2
        r = S_ / 2 - 30 * SS
        p = (c + math.cos(a) * r, c + math.sin(a) * r)
        q1 = (c + math.cos(a - 0.12) * (r - 10 * SS), c + math.sin(a - 0.12) * (r - 10 * SS))
        q2 = (c + math.cos(a + 0.12) * (r - 10 * SS), c + math.sin(a + 0.12) * (r - 10 * SS))
        d.polygon([p, q1, q2], fill=GOLD_L + (170,))
    return down(img)


def glow(s):
    S_ = s
    yy, xx = np.mgrid[0:S_, 0:S_].astype(np.float32)
    r = np.sqrt((xx - S_ / 2) ** 2 + (yy - S_ / 2) ** 2) / (S_ / 2)
    a = np.clip(1 - r, 0, 1) ** 2
    arr = np.zeros((S_, S_, 4), np.uint8)
    arr[..., :3] = 255
    arr[..., 3] = (a * 255).astype(np.uint8)
    return Image.fromarray(arr, "RGBA")


def minimap_frame(s):
    S_ = s * SS
    img = Image.new("RGBA", (S_, S_), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = S_ / 2
    def ring(dd, col, wd, r):
        dd.ellipse([c - r, c - r, c + r, c + r], outline=col, width=wd)
    R = S_ / 2 - 10 * SS
    ring(d, GOLD_D, int(9 * SS), R)
    ring(d, GOLD, int(5 * SS), R)
    ring(d, GOLD_L, int(1.5 * SS), R - 2 * SS)
    ring(d, GOLD_D, int(2 * SS), R - 8 * SS)
    # cardinal studs, N larger
    for i, a in enumerate([-math.pi / 2, 0, math.pi / 2, math.pi]):
        px, py = c + math.cos(a) * R, c + math.sin(a) * R
        rr = (12 if i == 0 else 7) * SS
        d.polygon([(px + math.cos(a) * rr * 1.3, py + math.sin(a) * rr * 1.3), (px + math.cos(a + 1.57) * rr * 0.6, py + math.sin(a + 1.57) * rr * 0.6),
                   (px - math.cos(a) * rr * 0.5, py - math.sin(a) * rr * 0.5), (px + math.cos(a - 1.57) * rr * 0.6, py + math.sin(a - 1.57) * rr * 0.6)], fill=GOLD if i else OX, outline=GOLD_L)
    return down(img)


def ribbon(w, h):
    W, H = w * SS, h * SS
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    tail = 40 * SS
    body = [(tail, 8 * SS), (W - tail, 8 * SS), (W - tail, H - 8 * SS), (tail, H - 8 * SS)]
    lt = [(0, 20 * SS), (tail + 10 * SS, 20 * SS), (tail + 10 * SS, H), (0, H), (18 * SS, H * 0.6)]
    rt = [(W, 20 * SS), (W - tail - 10 * SS, 20 * SS), (W - tail - 10 * SS, H), (W, H), (W - 18 * SS, H * 0.6)]
    for poly, col in [(lt, (86, 22, 20)), (rt, (86, 22, 20)), (body, (138, 36, 30))]:
        d.polygon(poly, fill=col)
    d.line(body + [body[0]], fill=GOLD, width=int(3 * SS))
    d.line([(tail, 14 * SS), (W - tail, 14 * SS)], fill=GOLD_L + (120,), width=int(1.2 * SS))
    d.line([(tail, H - 14 * SS), (W - tail, H - 14 * SS)], fill=GOLD_D, width=int(1.2 * SS))
    return down(img)


def divider(w, h):
    W, H = w * SS, h * SS
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = W / 2
    y = H / 2
    for x0, x1 in [(10 * SS, c - 20 * SS), (c + 20 * SS, W - 10 * SS)]:
        d.line([(x0, y), (x1, y)], fill=GOLD_D, width=int(3 * SS))
        d.line([(x0, y), (x1, y)], fill=GOLD, width=int(1.4 * SS))
    d.polygon([(c, y - 9 * SS), (c + 12 * SS, y), (c, y + 9 * SS), (c - 12 * SS, y)], fill=GOLD, outline=GOLD_L)
    for s in (-1, 1):
        d.ellipse([c + s * 26 * SS - 3 * SS, y - 3 * SS, c + s * 26 * SS + 3 * SS, y + 3 * SS], fill=GOLD_L)
    return down(img)


# ---------------------------------------------------------------------------------------- icons
ICON = 64


class Pen:
    """Icon drawing helper in a 0..100 coordinate space, at supersampled resolution."""

    def __init__(self, s=ICON):
        self.S = s * SS
        self.img = Image.new("RGBA", (self.S, self.S), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.img)

    def p(self, x, y):
        return (x / 100 * self.S, y / 100 * self.S)

    def w(self, v):
        return max(1, int(v / 100 * self.S))

    def poly(self, pts, fill, outline=None, width=0):
        self.d.polygon([self.p(*q) for q in pts], fill=fill, outline=outline)
        if outline and width:
            self.d.line([self.p(*q) for q in pts] + [self.p(*pts[0])], fill=outline, width=self.w(width), joint="curve")

    def line(self, pts, col, width):
        self.d.line([self.p(*q) for q in pts], fill=col, width=self.w(width), joint="curve")

    def circle(self, x, y, r, fill=None, outline=None, width=0):
        a, b = self.p(x - r, y - r), self.p(x + r, y + r)
        self.d.ellipse([a, b], fill=fill, outline=outline, width=self.w(width) if width else 0)

    def arc(self, x, y, r, a0, a1, col, width):
        a, b = self.p(x - r, y - r), self.p(x + r, y + r)
        self.d.arc([a, b], a0, a1, fill=col, width=self.w(width))

    def rect(self, x0, y0, x1, y1, fill, r=0):
        self.d.rounded_rectangle([self.p(x0, y0), self.p(x1, y1)], int(r / 100 * self.S), fill=fill)

    def done(self, shadow=True):
        img = self.img
        if shadow:
            a = img.split()[3]
            sh = Image.new("RGBA", img.size, (0, 0, 0, 0))
            sh.putalpha(a.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(SS * 1.5)).point(lambda v: v * 0.85))
            out = Image.new("RGBA", img.size, (0, 0, 0, 0))
            out.alpha_composite(sh, (SS, SS * 2))
            out.alpha_composite(img)
            img = out
        return down(img)


IV = IVORY
IVD = (190, 170, 130)


def ic_sword(P, col=IV):
    P.poly([(50, 6), (57, 18), (55, 64), (45, 64), (43, 18)], col)
    P.line([(50, 12), (50, 60)], IVD, 2)
    P.rect(30, 62, 70, 69, GOLD, 3)
    P.rect(46, 69, 54, 86, (110, 70, 40))
    P.circle(50, 91, 6, GOLD)


def icon_set():
    I = {}

    def new():
        return Pen()

    p = new(); ic_sword(p); I["attack"] = p
    p = new(); p.poly([(50, 8), (84, 20), (80, 58), (50, 92), (20, 58), (16, 20)], (90, 30, 26), GOLD, 5); p.poly([(50, 22), (70, 30), (68, 55), (50, 78), (32, 55), (30, 30)], (130, 44, 36)); p.line([(50, 24), (50, 76)], GOLD, 4); p.line([(32, 42), (68, 42)], GOLD, 4); I["block"] = p
    p = new()
    for k, r in enumerate([36, 26, 16]):
        p.arc(50, 50, r, 200 + k * 30, 420 + k * 30, IV, 7 - k)
    p.poly([(82, 34), (92, 50), (74, 50)], IV); I["dodge"] = p
    p = new(); p.poly([(50, 10), (82, 46), (62, 46), (62, 88), (38, 88), (38, 46), (18, 46)], IV); I["jump"] = p
    p = new(); p.circle(50, 60, 26, (170, 30, 40), IV, 4); p.rect(42, 14, 58, 36, (200, 200, 220), 3); p.rect(38, 10, 62, 18, (120, 80, 50), 3); p.circle(42, 52, 7, (255, 140, 140)); I["potion"] = p
    p = new(); p.circle(50, 60, 26, (40, 70, 190), IV, 4); p.rect(42, 14, 58, 36, (200, 200, 220), 3); p.rect(38, 10, 62, 18, (120, 80, 50), 3); p.circle(42, 52, 7, (150, 180, 255)); I["potion_mana"] = p
    p = new(); p.circle(50, 60, 26, (60, 160, 70), IV, 4); p.rect(42, 14, 58, 36, (200, 200, 220), 3); p.rect(38, 10, 62, 18, (120, 80, 50), 3); p.circle(42, 52, 7, (170, 255, 170)); I["potion_stamina"] = p
    p = new(); p.line([(22, 30), (78, 30)], IV, 8); p.line([(22, 50), (78, 50)], IV, 8); p.line([(22, 70), (78, 70)], IV, 8); I["menu"] = p
    p = new(); p.poly([(14, 22), (38, 14), (62, 22), (86, 14), (86, 78), (62, 86), (38, 78), (14, 86)], (200, 180, 140), IV, 3); p.line([(38, 14), (38, 78)], IVD, 3); p.line([(62, 22), (62, 86)], IVD, 3); p.line([(24, 60), (36, 44), (50, 54), (70, 36)], OX, 4); p.circle(70, 36, 5, OX); I["map"] = p
    p = new(); p.rect(22, 12, 78, 88, (225, 205, 160), 6); p.line([(32, 30), (68, 30)], (120, 90, 60), 4); p.line([(32, 44), (68, 44)], (120, 90, 60), 4); p.line([(32, 58), (58, 58)], (120, 90, 60), 4); p.circle(64, 74, 9, OX); I["quest"] = p
    p = new()
    for (x, y) in [(50, 18), (26, 56), (74, 56), (50, 84)]:
        p.circle(x, y, 9, GOLD, IV, 2)
    p.line([(50, 27), (28, 48)], IV, 3); p.line([(50, 27), (72, 48)], IV, 3); p.line([(28, 64), (48, 76)], IV, 3); p.line([(72, 64), (52, 76)], IV, 3); I["skills"] = p
    p = new(); p.poly([(26, 36), (74, 36), (82, 88), (18, 88)], (140, 96, 56), IV, 3); p.arc(50, 36, 16, 180, 360, IV, 5); p.rect(40, 48, 60, 58, GOLD, 2); I["inventory"] = p
    p = new()
    for i in range(8):
        a = i * math.tau / 8
        p.poly([(50 + math.cos(a - 0.2) * 30, 50 + math.sin(a - 0.2) * 30), (50 + math.cos(a) * 40, 50 + math.sin(a) * 40), (50 + math.cos(a + 0.2) * 30, 50 + math.sin(a + 0.2) * 30)], IV)
    p.circle(50, 50, 30, IV); p.circle(50, 50, 13, (40, 32, 26)); I["settings"] = p
    p = new(); p.rect(18, 18, 82, 82, (70, 90, 130), 5); p.rect(30, 18, 70, 42, (210, 210, 220), 2); p.rect(58, 22, 66, 38, (70, 90, 130)); p.rect(28, 56, 72, 82, (230, 225, 210), 2); I["save"] = p
    p = new(); p.circle(50, 46, 30, IV); p.circle(38, 44, 9, (40, 30, 24)); p.circle(62, 44, 9, (40, 30, 24)); p.poly([(50, 54), (45, 64), (55, 64)], (40, 30, 24)); p.rect(36, 70, 64, 86, IV, 3); p.line([(43, 72), (43, 86)], (40, 30, 24), 2); p.line([(50, 72), (50, 86)], (40, 30, 24), 2); p.line([(57, 72), (57, 86)], (40, 30, 24), 2); I["bestiary"] = p
    p = new(); p.poly([(50, 8), (61, 36), (92, 38), (68, 58), (76, 90), (50, 72), (24, 90), (32, 58), (8, 38), (39, 36)], GOLD, GOLD_L, 2); I["collection"] = p
    p = new(); p.poly([(14, 46), (70, 46), (86, 38), (86, 54), (70, 58), (60, 58), (64, 76), (36, 76), (40, 58), (14, 58)], (150, 150, 160), IV, 2); p.rect(38, 76, 62, 88, (110, 110, 120), 2); p.line([(70, 12), (52, 34)], (140, 100, 60), 5); p.rect(60, 6, 84, 18, (160, 160, 170), 2); I["craft"] = p
    p = new(); p.line([(24, 24), (76, 76)], IV, 11); p.line([(76, 24), (24, 76)], IV, 11); I["close"] = p
    p = new(); p.poly([(18, 50), (52, 18), (52, 38), (84, 38), (84, 62), (52, 62), (52, 82)], IV); I["back"] = p
    p = new(); p.poly([(30, 92), (24, 54), (32, 50), (38, 62), (38, 20), (46, 16), (52, 22), (52, 48), (54, 14), (62, 12), (66, 18), (66, 48), (70, 20), (78, 20), (80, 26), (80, 70), (70, 92)], IV, IVD, 2); I["interact"] = p
    p = new(); p.rect(14, 44, 86, 86, (120, 76, 40), 4); p.poly([(14, 44), (20, 22), (80, 22), (86, 44)], (150, 96, 50)); p.rect(14, 42, 86, 50, GOLD, 1); p.rect(44, 44, 56, 62, GOLD, 2); p.line([(14, 66), (86, 66)], (90, 56, 30), 3); I["chest"] = p
    p = new(); p.poly([(20, 80), (28, 46), (44, 30), (40, 14), (54, 26), (70, 26), (84, 40), (80, 52), (66, 48), (62, 80), (54, 80), (52, 60), (36, 62), (30, 80)], (150, 110, 70), IV, 2); p.circle(70, 36, 2.5, (30, 20, 10)); I["mount"] = p
    p = new(); p.circle(50, 50, 36, GOLD, GOLD_D, 4); p.circle(50, 50, 26, None, GOLD_L, 3); p.poly([(50, 32), (56, 46), (70, 48), (58, 58), (62, 72), (50, 64), (38, 72), (42, 58), (30, 48), (44, 46)], GOLD_D); I["coin"] = p
    p = new(); p.poly([(50, 88), (14, 50), (14, 30), (28, 16), (42, 18), (50, 30), (58, 18), (72, 16), (86, 30), (86, 50)], (200, 40, 50), IV, 3); I["heart"] = p
    p = new(); p.poly([(50, 10), (74, 50), (76, 64), (66, 82), (50, 88), (34, 82), (24, 64), (26, 50)], (50, 110, 230), IV, 3); I["mana"] = p
    p = new(); p.poly([(58, 6), (24, 56), (48, 56), (40, 94), (78, 40), (54, 40)], (240, 200, 70), IV, 2); I["stamina"] = p
    p = new(); p.poly([(50, 6), (60, 40), (94, 50), (60, 60), (50, 94), (40, 60), (6, 50), (40, 40)], GOLD_L, GOLD, 2); I["star"] = p
    p = new(); p.rect(24, 46, 76, 88, GOLD, 6); p.arc(50, 46, 18, 180, 360, IV, 8); p.circle(50, 62, 6, (40, 30, 20)); p.rect(47, 62, 53, 76, (40, 30, 20)); I["lock"] = p
    p = new(); p.line([(18, 52), (40, 74), (84, 26)], (120, 220, 120), 12); I["check"] = p
    p = new(); p.rect(43, 10, 57, 64, (250, 210, 90), 4); p.circle(50, 82, 9, (250, 210, 90)); I["quest_marker"] = p
    p = new(); p.poly([(40, 12), (60, 12), (60, 40), (88, 40), (88, 60), (60, 60), (60, 88), (40, 88), (40, 60), (12, 60), (12, 40), (40, 40)], IV); I["plus"] = p
    p = new(); p.circle(50, 44, 32, (230, 220, 200)); p.circle(37, 42, 10, (30, 20, 20)); p.circle(63, 42, 10, (30, 20, 20)); p.poly([(50, 52), (44, 64), (56, 64)], (30, 20, 20)); p.rect(32, 70, 68, 90, (230, 220, 200), 4)
    p.poly([(20, 22), (34, 6), (36, 26)], (230, 220, 200)); p.poly([(80, 22), (66, 6), (64, 26)], (230, 220, 200)); I["boss"] = p
    p = new()
    p.line([(20, 86), (80, 66)], (110, 70, 40), 9); p.line([(20, 66), (80, 86)], (110, 70, 40), 9)
    p.poly([(50, 14), (66, 44), (62, 70), (50, 76), (38, 70), (34, 44)], (250, 140, 40)); p.poly([(50, 36), (58, 54), (56, 68), (44, 68), (42, 54)], (255, 230, 120)); I["campfire"] = p
    p = new(); p.poly([(10, 30), (90, 30), (90, 42), (72, 50), (68, 66), (80, 80), (20, 80), (32, 66), (28, 50), (10, 42)], (150, 150, 165), IV, 2); I["anvil"] = p
    p = new(); p.poly([(40, 10), (60, 10), (60, 38), (86, 82), (80, 90), (20, 90), (14, 82), (40, 38)], (120, 200, 160), IV, 3); p.poly([(26, 70), (74, 70), (82, 84), (18, 84)], (60, 160, 110)); I["alchemy"] = p
    p = new(); p.line([(24, 88), (64, 30)], (130, 90, 50), 8); p.poly([(36, 20), (70, 10), (92, 36), (80, 40), (66, 26), (44, 28)], (170, 170, 180), IV, 2); I["pickaxe"] = p
    p = new(); p.line([(28, 90), (62, 22)], (130, 90, 50), 8); p.poly([(54, 14), (84, 18), (86, 46), (70, 42), (60, 30)], (170, 170, 180), IV, 2); I["woodaxe"] = p
    p = new(); p.arc(50, 50, 32, 200, 350, (190, 190, 200), 9); p.line([(28, 64), (14, 88)], (130, 90, 50), 8); I["sickle"] = p
    p = new(); p.line([(18, 88), (70, 14)], (130, 90, 50), 5); p.line([(70, 14), (84, 60)], IVD, 1.5); p.arc(80, 66, 6, 0, 200, IV, 3); p.circle(30, 76, 6, (180, 180, 190)); I["fishing"] = p
    p = new(); p.circle(50, 64, 16, IV)
    for (x, y) in [(28, 40), (44, 28), (60, 28), (74, 40)]:
        p.circle(x, y, 9, IV)
    I["hunt"] = p
    p = new(); p.poly([(30, 34), (70, 34), (82, 88), (18, 88)], (160, 120, 70), IV, 2); p.line([(38, 34), (44, 16), (56, 16), (62, 34)], IV, 4); p.circle(50, 62, 10, GOLD); I["merchant"] = p
    p = new(); p.poly([(14, 46), (50, 14), (86, 46)], (150, 60, 40), IV, 2); p.rect(22, 46, 78, 86, (200, 180, 140)); p.rect(42, 60, 58, 86, (90, 60, 40)); I["village"] = p
    p = new(); p.poly([(8, 88), (30, 30), (50, 16), (72, 28), (92, 88)], (110, 105, 100), IV, 2); p.poly([(36, 88), (40, 58), (50, 48), (60, 58), (64, 88)], (20, 16, 14)); I["cave"] = p
    p = new(); p.rect(34, 30, 66, 90, (170, 165, 155)); p.poly([(28, 32), (50, 6), (72, 32)], (70, 70, 110)); p.rect(45, 44, 55, 58, (40, 40, 60)); p.rect(42, 70, 58, 90, (60, 50, 40)); I["tower"] = p
    p = new(); p.rect(14, 44, 86, 90, (170, 165, 155)); p.rect(26, 22, 44, 90, (180, 175, 165)); p.rect(56, 22, 74, 90, (180, 175, 165))
    for x in [26, 34, 56, 64]:
        p.rect(x, 16, x + 6, 24, (180, 175, 165))
    p.rect(42, 62, 58, 90, (60, 50, 40), 6); I["castle"] = p
    p = new(); p.circle(50, 50, 36, (90, 60, 200)); p.circle(50, 50, 26, (150, 110, 255)); p.circle(50, 50, 14, (230, 210, 255)); p.arc(50, 50, 40, 0, 300, IV, 3); I["waypoint"] = p
    p = new(); p.poly([(50, 8), (58, 38), (90, 50), (58, 62), (50, 92), (42, 62), (10, 50), (42, 38)], IV); p.circle(50, 50, 8, OX); I["compass"] = p
    p = new(); p.poly([(50, 10), (78, 82), (50, 66), (22, 82)], GOLD_L, GOLD_D, 2); I["player_arrow"] = p
    p = new(); p.rect(14, 30, 86, 76, (60, 54, 48), 8); p.circle(50, 53, 16, (20, 18, 16), IV, 4); p.rect(26, 22, 44, 32, (60, 54, 48), 3); I["camera"] = p
    p = new(); p.circle(50, 50, 34, None, IV, 6); p.line([(50, 50), (50, 28)], IV, 6); p.line([(50, 50), (66, 58)], IV, 6); I["clock"] = p
    p = new(); p.poly([(20, 20), (80, 20), (80, 64), (50, 64), (34, 82), (36, 64), (20, 64)], (230, 220, 200), IVD, 2); p.circle(36, 42, 4, (60, 50, 40)); p.circle(50, 42, 4, (60, 50, 40)); p.circle(64, 42, 4, (60, 50, 40)); I["talk"] = p
    p = new(); p.poly([(30, 10), (30, 90), (46, 74), (58, 94), (66, 90), (56, 70), (78, 70)], IV, (40, 30, 20), 3); I["pointer"] = p
    p = new(); p.circle(50, 50, 38, (40, 32, 26), GOLD, 4); p.poly([(40, 30), (72, 50), (40, 70)], IV); I["play"] = p
    p = new(); p.rect(30, 24, 44, 76, IV, 3); p.rect(56, 24, 70, 76, IV, 3); I["pause"] = p
    p = new(); p.poly([(50, 90), (20, 60), (36, 60), (36, 12), (64, 12), (64, 60), (80, 60)], IV); I["drop"] = p
    p = new(); p.poly([(18, 20), (82, 20), (74, 88), (26, 88)], (120, 120, 130), IV, 3); p.rect(12, 12, 88, 22, IV, 3); p.rect(40, 4, 60, 12, IV, 2); I["trash"] = p
    p = new(); p.line([(20, 50), (80, 50)], IV, 10); p.poly([(60, 26), (88, 50), (60, 74)], IV); I["arrow_right"] = p
    p = new(); p.line([(80, 50), (20, 50)], IV, 10); p.poly([(40, 26), (12, 50), (40, 74)], IV); I["arrow_left"] = p
    p = new(); p.circle(50, 50, 30, (255, 220, 120)); p.circle(62, 40, 26, (0, 0, 0, 0)); I["night"] = p
    p = new(); p.circle(50, 50, 18, (255, 210, 90))
    for i in range(8):
        a = i * math.tau / 8
        p.line([(50 + math.cos(a) * 26, 50 + math.sin(a) * 26), (50 + math.cos(a) * 38, 50 + math.sin(a) * 38)], (255, 210, 90), 6)
    I["day"] = p
    p = new(); p.circle(50, 38, 18, IV); p.poly([(18, 92), (24, 64), (50, 56), (76, 64), (82, 92)], IV); I["character"] = p
    p = new(); p.line([(20, 30), (50, 70), (80, 30)], IV, 10); I["chevron_down"] = p
    p = new(); p.circle(50, 50, 34, None, IV, 6); p.line([(50, 30), (50, 56)], IV, 8); p.circle(50, 70, 5, IV); I["info"] = p
    p = new(); p.poly([(50, 6), (90, 26), (90, 52), (50, 94), (10, 52), (10, 26)], (60, 50, 90), GOLD, 3); p.circle(50, 46, 14, GOLD_L); I["relic"] = p
    p = new(); p.poly([(20, 70), (40, 30), (60, 46), (80, 20), (90, 34), (66, 70)], (180, 230, 250), IV, 2); I["crystal"] = p

    # --- spells (coloured emblems on a disc) ---
    def disc(c1, c2):
        p = Pen()
        p.circle(50, 50, 46, c1)
        p.circle(50, 50, 40, c2)
        return p

    def fire_shape(p, cx, cy, s, col1, col2):
        p.poly([(cx, cy - 34 * s), (cx + 20 * s, cy + 2 * s), (cx + 14 * s, cy + 24 * s), (cx, cy + 32 * s), (cx - 14 * s, cy + 24 * s), (cx - 20 * s, cy + 2 * s), (cx - 6 * s, cy - 8 * s)], col1)
        p.poly([(cx, cy - 6 * s), (cx + 10 * s, cy + 12 * s), (cx + 6 * s, cy + 26 * s), (cx - 6 * s, cy + 26 * s), (cx - 10 * s, cy + 12 * s)], col2)

    p = disc((120, 30, 10), (60, 14, 8)); fire_shape(p, 50, 52, 1.0, (255, 120, 30), (255, 230, 120)); I["sp_fireball"] = p
    p = disc((120, 30, 10), (60, 14, 8))
    for k in range(3):
        fire_shape(p, 26 + k * 24, 60, 0.6, (255, 110 + k * 20, 30), (255, 220, 110))
    p.arc(50, 90, 50, 225, 315, (255, 190, 90), 4); I["sp_flamewave"] = p
    p = disc((20, 70, 120), (10, 34, 64)); p.poly([(50, 10), (62, 60), (50, 90), (38, 60)], (190, 235, 255), IV, 2); p.poly([(26, 40), (36, 66), (28, 82), (20, 66)], (150, 210, 240)); p.poly([(74, 40), (80, 66), (72, 82), (64, 66)], (150, 210, 240)); I["sp_icespike"] = p
    p = disc((20, 70, 120), (10, 34, 64))
    for i in range(6):
        a = i * math.tau / 6
        p.line([(50, 50), (50 + math.cos(a) * 34, 50 + math.sin(a) * 34)], (200, 240, 255), 5)
        p.line([(50 + math.cos(a) * 22, 50 + math.sin(a) * 22), (50 + math.cos(a + 0.5) * 30, 50 + math.sin(a + 0.5) * 30)], (200, 240, 255), 3)
        p.line([(50 + math.cos(a) * 22, 50 + math.sin(a) * 22), (50 + math.cos(a - 0.5) * 30, 50 + math.sin(a - 0.5) * 30)], (200, 240, 255), 3)
    I["sp_frostnova"] = p
    p = disc((90, 70, 10), (40, 30, 70)); p.poly([(58, 8), (26, 54), (48, 54), (36, 92), (76, 40), (54, 40)], (250, 240, 120), IV, 2); I["sp_thunder"] = p
    p = disc((90, 70, 10), (40, 30, 70)); p.line([(14, 30), (34, 46), (28, 56), (50, 64), (44, 74), (70, 86)], (240, 240, 140), 5); p.line([(60, 14), (54, 34), (70, 40), (64, 56), (86, 64)], (200, 220, 255), 4); I["sp_chain"] = p
    p = disc((30, 90, 30), (14, 44, 16))
    for (x0, y0, x1, y1) in [(20, 86, 40, 30), (50, 90, 50, 22), (80, 86, 62, 30)]:
        p.line([(x0, y0), ((x0 + x1) / 2 + 8, (y0 + y1) / 2), (x1, y1)], (140, 100, 50), 7)
    p.poly([(40, 30), (30, 14), (46, 20)], (120, 220, 90)); p.poly([(50, 22), (56, 6), (62, 22)], (120, 220, 90)); p.poly([(62, 30), (74, 16), (70, 34)], (120, 220, 90)); I["sp_roots"] = p
    p = disc((30, 90, 30), (14, 44, 16)); p.poly([(50, 86), (24, 56), (26, 30), (50, 16), (74, 30), (76, 56)], (110, 210, 90), IV, 2); p.line([(50, 86), (50, 24)], (40, 110, 40), 3); p.rect(44, 40, 56, 70, (240, 255, 240)); p.rect(36, 49, 64, 61, (240, 255, 240)); I["sp_regrowth"] = p
    p = disc((50, 20, 70), (18, 8, 26)); p.circle(50, 50, 28, (130, 60, 170)); p.circle(60, 44, 26, (18, 8, 26)); p.circle(36, 56, 5, (230, 120, 255)); I["sp_shadowstep"] = p
    p = disc((50, 20, 70), (18, 8, 26)); p.poly([(20, 50), (50, 30), (80, 50), (50, 70)], (180, 90, 220), IV, 2); p.circle(50, 50, 10, (30, 10, 40)); p.circle(50, 50, 4, (255, 120, 140))
    for i in range(5):
        a = math.pi * (0.15 + i * 0.175)
        p.line([(50 + math.cos(a) * 34, 50 + math.sin(a) * 34 + 10), (50 + math.cos(a) * 22, 50 + math.sin(a) * 22 + 10)], (180, 90, 220), 3)
    I["sp_souldrain"] = p
    p = disc((130, 110, 40), (70, 56, 20)); p.circle(50, 50, 18, (255, 250, 210))
    for i in range(12):
        a = i * math.tau / 12
        p.line([(50 + math.cos(a) * 24, 50 + math.sin(a) * 24), (50 + math.cos(a) * (38 if i % 2 else 32), 50 + math.sin(a) * (38 if i % 2 else 32))], (255, 240, 170), 4)
    I["sp_holylight"] = p
    p = disc((130, 110, 40), (70, 56, 20)); p.poly([(50, 14), (80, 26), (76, 58), (50, 86), (24, 58), (20, 26)], (255, 230, 140), IV, 3); p.poly([(50, 28), (66, 34), (64, 56), (50, 72), (36, 56), (34, 34)], (255, 250, 220)); I["sp_radiantshield"] = p
    p = disc((70, 40, 120), (30, 18, 60))
    for k, (x, y) in enumerate([(30, 66), (50, 48), (70, 30)]):
        p.poly([(x, y - 12), (x + 9, y), (x, y + 12), (x - 9, y)], (220, 170, 255), IV, 1)
    p.line([(18, 82), (30, 66)], (220, 170, 255), 2); I["sp_missiles"] = p
    p = disc((70, 40, 120), (30, 18, 60))
    for i in range(5):
        a = -math.pi / 2 + i * math.tau / 5
        b = -math.pi / 2 + (i + 2) * math.tau / 5
        p.line([(50 + math.cos(a) * 32, 50 + math.sin(a) * 32), (50 + math.cos(b) * 32, 50 + math.sin(b) * 32)], (230, 190, 255), 4)
    p.circle(50, 50, 36, None, (230, 190, 255), 2); I["sp_arcanenova"] = p

    # --- class emblems ---
    p = disc((90, 30, 24), (40, 14, 12)); ic_sword(p); I["cl_warrior"] = p
    p = disc((40, 80, 40), (18, 36, 18)); p.arc(40, 50, 34, 290, 70, (170, 120, 70), 6); p.line([(52, 18), (52, 82)], IVD, 2); p.line([(30, 50), (86, 50)], IV, 3); p.poly([(86, 50), (76, 44), (76, 56)], IV); I["cl_archer"] = p
    p = disc((40, 40, 50), (16, 16, 22)); p.poly([(30, 80), (60, 20), (66, 24), (40, 84)], (210, 210, 220)); p.poly([(70, 80), (40, 20), (34, 24), (60, 84)], (170, 170, 180)); I["cl_assassin"] = p
    p = disc((50, 40, 120), (20, 16, 60)); p.line([(36, 88), (58, 24)], (150, 100, 60), 6); p.circle(60, 20, 10, (170, 210, 255)); p.circle(60, 20, 5, (255, 255, 255)); I["cl_mage"] = p
    p = disc((140, 120, 50), (70, 56, 24)); p.poly([(50, 12), (80, 24), (76, 56), (50, 88), (24, 56), (20, 24)], (230, 220, 200), GOLD, 3); p.rect(46, 26, 54, 74, GOLD); p.rect(34, 38, 66, 46, GOLD); I["cl_paladin"] = p
    p = disc((90, 60, 30), (40, 26, 14)); p.circle(50, 60, 13, IV)
    for (x, y) in [(32, 40), (44, 30), (58, 30), (70, 40)]:
        p.circle(x, y, 7, IV)
    I["cl_hunter"] = p
    return {k: v.done(shadow=not k.startswith(("sp_", "cl_"))) for k, v in I.items()}


# ---------------------------------------------------------------------------------------- packing
def main():
    items = []  # (name, image, border)
    items.append(("panel", panel(128, 128, 40), 40))
    items.append(("panel_plain", panel(96, 96, 28, ornate=False, seed=5), 28))
    items.append(("parchment", panel(128, 128, 36, dark=False), 36))
    items.append(("button", button(192, 64), 24))
    items.append(("button_hi", button(192, 64, hi=True), 24))
    items.append(("button_dark", button(192, 64, red=False), 24))
    items.append(("button_dark_hi", button(192, 64, hi=True, red=False), 24))
    items.append(("slot", slot(96), 20))
    items.append(("slot_sel", slot(96, True), 20))
    items.append(("round", round_btn(128), 0))
    items.append(("round_hi", round_btn(128, hi=True), 0))
    items.append(("round_red", round_btn(128, tint=(90, 26, 22)), 0))
    items.append(("bar_frame", bar_frame(256, 32), 15))
    items.append(("bar_fill", bar_fill(256, 24), 11))
    items.append(("joy_base", joystick_base(256), 0))
    items.append(("joy_knob", round_btn(112, hi=True, tint=(60, 46, 36)), 0))
    items.append(("glow", glow(128), 0))
    items.append(("minimap_frame", minimap_frame(256), 0))
    items.append(("ribbon", ribbon(512, 96), 0))
    items.append(("divider", divider(512, 24), 0))
    for name, img in icon_set().items():
        items.append(("i_" + name, img, 0))

    A = 2048
    atlas = Image.new("RGBA", (A, A), (0, 0, 0, 0))
    x = y = 2
    rowh = 0
    lines = []
    items.sort(key=lambda t: -t[1].height)
    for name, img, border in items:
        w, h = img.size
        if x + w + 2 > A:
            x = 2
            y += rowh + 2
            rowh = 0
        atlas.paste(img, (x, y))
        lines.append(f"{name} {x} {y} {w} {h} {border}")
        x += w + 2
        rowh = max(rowh, h)
    H = 1 << (y + rowh + 2 - 1).bit_length()
    atlas = atlas.crop((0, 0, A, H))
    atlas.save(os.path.join(OUT, "ui.png"), optimize=True)
    with open(os.path.join(OUT, "ui.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    print("ui atlas", atlas.size, len(lines), "regions")
    # preview sheet for review
    prev = Image.new("RGBA", atlas.size, (60, 50, 42, 255))
    prev.alpha_composite(atlas)
    prev.convert("RGB").save(os.path.join(HERE, "..", "build", "ui_preview.png"))


if __name__ == "__main__":
    main()
