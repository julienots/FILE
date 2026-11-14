#!/usr/bin/env python3
"""Generates the original assets of "Les Ruines d'Ambre" (AGR ENGINE 2.5D game template):
sprites (shadow, slash, sparkle, amber gem, skeleton archer, stone brute, pot, chests, sign,
spikes, pressure plate, arrow, boulder) in assets/games/, sounds and music in assets/audio/. Everything is drawn / synthesized here; deterministic (fixed seeds).
Only writes the files listed below (never touches the other template assets).

    python3 tools/generate_ambre_assets.py      # requires Pillow
"""
import math
import os
import random
import struct
import wave

from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "editor", "resources", "templates", "assets")
GAMES, AUDIO = os.path.join(ROOT, "games"), os.path.join(ROOT, "audio")
RATE = 22050


# ---- sprites
def shadow():
    n = 128
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    px = img.load()
    for y in range(n):
        for x in range(n):
            d = math.hypot((x + 0.5 - n / 2) / (n / 2), (y + 0.5 - n / 2) / (n / 2))
            a = max(0.0, 1.0 - d) ** 1.3
            px[x, y] = (0, 0, 0, int(255 * min(1.0, a * 1.4)))
    img.save(os.path.join(GAMES, "shadow.png"), optimize=True)


def slash():
    # A bright crescent (outer disc minus an offset inner disc), warm at the edge, blurred.
    s = 4
    w, h = 256, 160
    big = Image.new("L", (w * s, h * s), 0)
    d = ImageDraw.Draw(big)
    cx, cy, r = w * s * 0.5, h * s * 1.02, 150 * s
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
    r2, oy = 132 * s, 34 * s
    d.ellipse([cx - r2, cy - r2 + oy, cx + r2, cy + r2 + oy], fill=0)
    mask = big.filter(ImageFilter.GaussianBlur(3 * s))
    img = Image.new("RGBA", (w * s, h * s), (0, 0, 0, 0))
    px, mp = img.load(), mask.load()
    for y in range(0, h * s):
        for x in range(0, w * s):
            a = mp[x, y]
            if a:
                k = abs(x - cx) / (w * s * 0.5)  # fades toward the tips
                px[x, y] = (255, int(245 - 70 * k), int(200 - 150 * k), int(a * (1 - k ** 3)))
    img.resize((w, h), Image.LANCZOS).save(os.path.join(GAMES, "slash.png"), optimize=True)


def disc(name, ring):
    # White soft disc (tinted by the game: danger zones, stick knob) or ring (stick base).
    n = 128
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    px = img.load()
    for y in range(n):
        for x in range(n):
            d = math.hypot((x + 0.5 - n / 2) / (n / 2), (y + 0.5 - n / 2) / (n / 2))
            if ring:
                a = max(0.0, 1.0 - abs(d - 0.86) / 0.1) + 0.25 * max(0.0, min(1.0, (0.86 - d) * 8))
            else:
                a = max(0.0, min(1.0, (1.0 - d) * 10)) * (0.55 + 0.45 * d)
            px[x, y] = (255, 255, 255, int(255 * min(1.0, a)))
    img.save(os.path.join(GAMES, name + ".png"), optimize=True)


def sparkle():
    n = 64
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    px = img.load()
    for y in range(n):
        for x in range(n):
            dx, dy = (x + 0.5 - n / 2) / (n / 2), (y + 0.5 - n / 2) / (n / 2)
            star = max(0.0, 1.0 - abs(dx) * 6) * max(0.0, 1.0 - abs(dy)) + max(0.0, 1.0 - abs(dy) * 6) * max(0.0, 1.0 - abs(dx))
            glow = max(0.0, 1.0 - math.hypot(dx, dy)) ** 2
            a = min(1.0, star + glow)
            px[x, y] = (255, 255, 255, int(255 * a))
    img.save(os.path.join(GAMES, "sparkle.png"), optimize=True)




# ---- characters and props (drawn at 4x then downsampled, same outline style as the other templates)
SS = 4
OUTLINE = (34, 30, 44, 255)


def cv(w, h=None):
    h = h or w
    img = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
    return img, ImageDraw.Draw(img)


def put(img, name):
    w, h = img.size
    img.resize((w // SS, h // SS), Image.LANCZOS).save(os.path.join(GAMES, name + ".png"), optimize=True)


def sh(c, k, a=None):
    return tuple(max(0, min(255, int(v * k))) for v in c[:3]) + (a if a is not None else (c[3] if len(c) > 3 else 255),)


def el(d, cx, cy, rx, ry, fill, outline=None, width=0):
    d.ellipse([(cx - rx) * SS, (cy - ry) * SS, (cx + rx) * SS, (cy + ry) * SS], fill=fill, outline=outline, width=width * SS)


def rc(d, x0, y0, x1, y1, fill, r=0, outline=None, width=0):
    d.rounded_rectangle([x0 * SS, y0 * SS, x1 * SS, y1 * SS], radius=r * SS, fill=fill, outline=outline, width=width * SS)


def pg(d, pts, fill, outline=None, width=0):
    d.polygon([(x * SS, y * SS) for x, y in pts], fill=fill, outline=outline, width=width * SS)


def ln(d, pts, fill, width):
    d.line([(x * SS, y * SS) for x, y in pts], fill=fill, width=width * SS, joint="curve")


def amber():
    img, d = cv(64)
    c = (255, 160, 40, 255)
    pg(d, [(32, 3), (54, 20), (48, 50), (32, 61), (16, 50), (10, 20)], c, OUTLINE, 3)
    pg(d, [(32, 3), (42, 20), (38, 50), (32, 61), (26, 50), (22, 20)], sh(c, 1.18))
    pg(d, [(10, 20), (54, 20), (42, 20), (32, 3), (22, 20)], (255, 225, 140, 255))
    el(d, 24, 14, 4, 3, (255, 250, 220, 230))
    put(img, "amber")


def archer():  # skeleton archer
    img, d = cv(128)
    bone = (232, 226, 205, 255)
    cloth = (120, 60, 70, 255)
    rc(d, 50, 94, 58, 122, bone, r=3, outline=OUTLINE, width=2)     # legs
    rc(d, 70, 94, 78, 122, bone, r=3, outline=OUTLINE, width=2)
    pg(d, [(44, 60), (84, 60), (90, 100), (38, 100)], cloth, OUTLINE, 3)  # tunic
    for y in (68, 76, 84):
        ln(d, [(54, y), (74, y)], sh(cloth, 0.7), 2)
    el(d, 64, 40, 22, 21, bone, OUTLINE, 3)                          # skull
    rc(d, 52, 52, 76, 64, bone, r=4, outline=OUTLINE, width=2)
    el(d, 55, 40, 6, 7, (40, 20, 30, 255))
    el(d, 73, 40, 6, 7, (40, 20, 30, 255))
    el(d, 55, 40, 2.5, 2.5, (255, 90, 60, 255))
    el(d, 73, 40, 2.5, 2.5, (255, 90, 60, 255))
    for x in (58, 64, 70):
        ln(d, [(x, 56), (x, 63)], OUTLINE, 1)
    d.arc([86 * SS, 36 * SS, 122 * SS, 112 * SS], 270, 90, fill=(130, 85, 45, 255), width=6 * SS)  # bow
    ln(d, [(104, 37), (104, 111)], (240, 240, 240, 255), 1)
    ln(d, [(84, 74), (104, 74)], bone, 5)                            # arm
    put(img, "archer")


def brute():  # stone golem
    img, d = cv(128)
    stone = (130, 125, 120, 255)
    rc(d, 34, 92, 56, 124, sh(stone, 0.85), r=6, outline=OUTLINE, width=3)
    rc(d, 72, 92, 94, 124, sh(stone, 0.85), r=6, outline=OUTLINE, width=3)
    rc(d, 26, 40, 102, 100, stone, r=18, outline=OUTLINE, width=4)
    rc(d, 6, 48, 30, 100, sh(stone, 0.9), r=10, outline=OUTLINE, width=3)
    rc(d, 98, 48, 122, 100, sh(stone, 0.9), r=10, outline=OUTLINE, width=3)
    rc(d, 44, 14, 84, 46, sh(stone, 1.08), r=10, outline=OUTLINE, width=3)
    rc(d, 50, 26, 78, 34, (40, 30, 30, 255), r=3)
    el(d, 57, 30, 4, 3, (255, 150, 40, 255))
    el(d, 71, 30, 4, 3, (255, 150, 40, 255))
    for a, b in (((40, 56), (54, 70)), ((80, 52), (90, 66)), ((60, 78), (72, 90))):
        ln(d, [a, b], sh(stone, 0.6), 3)
    pg(d, [(58, 62), (70, 62), (64, 74)], (255, 170, 60, 255))     # amber core
    put(img, "brute")


def pot():
    img, d = cv(96)
    clay = (190, 110, 70, 255)
    el(d, 48, 60, 30, 30, clay, OUTLINE, 3)
    rc(d, 34, 20, 62, 36, sh(clay, 0.9), r=4, outline=OUTLINE, width=3)
    el(d, 48, 22, 16, 5, sh(clay, 0.6))
    ln(d, [(22, 60), (74, 60)], sh(clay, 0.75), 3)
    ln(d, [(26, 70), (70, 70)], (230, 180, 90, 255), 2)
    el(d, 38, 48, 6, 9, sh(clay, 1.3, 180))
    put(img, "pot")


def chest(opened):
    img, d = cv(128)
    wood = (150, 95, 50, 255)
    gold = (240, 190, 60, 255)
    rc(d, 18, 62, 110, 112, wood, r=6, outline=OUTLINE, width=3)
    for x in (18, 56, 72, 104):
        rc(d, x, 62, x + 6, 112, gold, r=1)
    if opened:
        pg(d, [(18, 62), (110, 62), (100, 28), (28, 28)], sh(wood, 0.6), OUTLINE, 3)
        el(d, 64, 60, 36, 8, (255, 220, 90, 255))
        for i in range(7):
            el(d, 36 + i * 9, 56 - (i % 2) * 4, 5, 4, (255, 230, 120, 255), OUTLINE, 1)
    else:
        d.chord([18 * SS, 30 * SS, 110 * SS, 94 * SS], 180, 360, fill=sh(wood, 1.1), outline=OUTLINE, width=3 * SS)
        rc(d, 56, 54, 72, 74, gold, r=3, outline=OUTLINE, width=2)
        el(d, 64, 63, 3, 4, OUTLINE)
    put(img, "chest_open" if opened else "chest")


def sign():
    img, d = cv(96)
    wood = (170, 120, 70, 255)
    rc(d, 44, 46, 52, 92, sh(wood, 0.75), r=2, outline=OUTLINE, width=2)
    rc(d, 12, 12, 84, 54, wood, r=6, outline=OUTLINE, width=3)
    for y in (24, 33, 42):
        ln(d, [(22, y), (74 - (y % 3) * 6, y)], sh(wood, 0.55), 3)
    put(img, "sign")


def spikes():
    img, d = cv(128, 96)
    steel = (200, 205, 215, 255)
    for i in range(5):
        x = 14 + i * 25
        pg(d, [(x - 11, 88), (x, 10 + (i % 2) * 10), (x + 11, 88)], steel, OUTLINE, 3)
        pg(d, [(x, 10 + (i % 2) * 10), (x + 11, 88), (x + 3, 88)], sh(steel, 0.75))
    rc(d, 0, 84, 128, 96, (80, 80, 90, 255), r=2, outline=OUTLINE, width=2)
    put(img, "spikes")


def plate():  # pressure plate seen from above (lit by the game's tint)
    img, d = cv(128)
    el(d, 64, 64, 58, 58, (95, 90, 85, 255), OUTLINE, 4)
    el(d, 64, 64, 46, 46, (150, 145, 135, 255), OUTLINE, 2)
    for a in range(6):
        t = a * math.pi / 3
        ln(d, [(64 + 18 * math.cos(t), 64 + 18 * math.sin(t)), (64 + 36 * math.cos(t), 64 + 36 * math.sin(t))], (255, 255, 255, 255), 4)
    el(d, 64, 64, 12, 12, (255, 255, 255, 255))
    put(img, "plate")


def arrow():
    img, d = cv(96, 24)
    ln(d, [(10, 12), (80, 12)], (150, 100, 55, 255), 4)
    pg(d, [(76, 4), (94, 12), (76, 20)], (210, 215, 225, 255), OUTLINE, 2)
    pg(d, [(4, 4), (18, 12), (4, 20), (12, 12)], (230, 80, 70, 255))
    put(img, "arrow")


def rock():
    img, d = cv(96)
    c = (140, 110, 90, 255)
    pg(d, [(20, 70), (14, 44), (34, 18), (64, 14), (84, 38), (80, 70), (52, 84)], c, OUTLINE, 3)
    pg(d, [(34, 18), (64, 14), (60, 40), (36, 40)], sh(c, 1.25))
    put(img, "boulder")


# ---- audio
def write(name, samples):
    peak = max(1e-9, max(abs(v) for v in samples))
    gain = min(1.0, 0.9 / peak)
    with wave.open(os.path.join(AUDIO, name), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, v * gain)) * 32767)) for v in samples))
    print(f"{name}: {len(samples) / RATE:.2f} s")


def env(t, attack, decay):
    return (min(1.0, t / attack) if attack > 0 else 1.0) * math.exp(-t / decay)


def noise_burst(seconds, cutoff_start, cutoff_end, decay, seed):
    rnd = random.Random(seed)
    out, low = [], 0.0
    n = int(seconds * RATE)
    for i in range(n):
        t = i / RATE
        k = cutoff_start + (cutoff_end - cutoff_start) * (t / seconds)
        low += k * (rnd.uniform(-1, 1) - low)
        out.append(low * env(t, 0.002, decay))
    return out


def swing():  # sword whoosh: band of noise sweeping up then down
    rnd = random.Random(3)
    n = int(0.22 * RATE)
    out, low, band = [], 0.0, 0.0
    for i in range(n):
        t = i / RATE
        k = 0.05 + 0.5 * math.sin(math.pi * t / 0.22)
        low += k * (rnd.uniform(-1, 1) - low)
        band += 0.3 * (low - band)
        out.append((low - band) * math.sin(math.pi * t / 0.22) ** 1.5)
    return out


def hit():  # impact: thump + crunchy noise
    rnd = random.Random(5)
    n = int(0.18 * RATE)
    out = []
    for i in range(n):
        t = i / RATE
        thump = math.sin(2 * math.pi * (160 - 400 * t) * t) * math.exp(-t / 0.05)
        crunch = rnd.uniform(-1, 1) * math.exp(-t / 0.025)
        out.append(0.9 * thump + 0.5 * crunch)
    return out


def hurt():  # player hurt: falling square chirp
    n = int(0.3 * RATE)
    out, ph = [], 0.0
    for i in range(n):
        t = i / RATE
        ph += (420 - 700 * t) / RATE
        out.append((1.0 if ph % 1 < 0.5 else -1.0) * 0.5 * env(t, 0.003, 0.12))
    return out


def gem():  # sparkling arpeggio
    notes = [76, 80, 83, 88]
    out = []
    for k, m in enumerate(notes):
        f = 440 * 2 ** ((m - 69) / 12)
        for i in range(int(0.07 * RATE) if k < 3 else int(0.4 * RATE)):
            t = i / RATE
            out.append(0.5 * (math.sin(2 * math.pi * f * t) + 0.25 * math.sin(6 * math.pi * f * t)) * env(t, 0.002, 0.08 if k < 3 else 0.18))
    return out


def slam():  # boss slam: deep boom + rumble
    rnd = random.Random(9)
    n = int(0.9 * RATE)
    out, low = [], 0.0
    for i in range(n):
        t = i / RATE
        boom = math.sin(2 * math.pi * (70 - 40 * t) * t) * math.exp(-t / 0.25)
        low += 0.03 * (rnd.uniform(-1, 1) - low)
        out.append(boom + 2.5 * low * math.exp(-t / 0.35))
    return out


def portal():  # rising shimmer
    n = int(0.9 * RATE)
    out = []
    for i in range(n):
        t = i / RATE
        f = 300 * 2 ** (2 * t)
        v = sum(math.sin(2 * math.pi * f * h * t + h) / h for h in (1, 2, 3))
        out.append(0.4 * v * math.sin(math.pi * t / 0.9) * (0.7 + 0.3 * math.sin(2 * math.pi * 12 * t)))
    return out


def music():
    # 16 s loop, 90 BPM-ish, D dorian: slow pad, plucked harp ostinato, soft low drum.
    chords = [(50, 57, 62, 65), (48, 55, 60, 64), (46, 53, 58, 62), (48, 55, 60, 67)]
    total, bar = 16.0, 4.0
    n = int(total * RATE)
    out = [0.0] * n
    beat = 60.0 / 90.0
    # pad
    for i in range(n):
        t = i / RATE
        c = chords[int(t // bar) % 4]
        local = t % bar
        fade = min(1.0, local / 0.6, (bar - local) / 0.6)
        v = 0.0
        for m in c:
            f = 440 * 2 ** ((m - 69) / 12)
            v += math.sin(2 * math.pi * f * t) + 0.3 * math.sin(2 * math.pi * f * 2.003 * t)
        out[i] += 0.05 * v * (0.6 + 0.4 * fade)
    # harp ostinato (eighth notes)
    step = beat / 2
    k = 0
    while k * step < total:
        start = k * step
        c = chords[int(start // bar) % 4]
        pattern = [0, 2, 3, 1, 2, 3, 1, 2]
        m = c[pattern[k % 8]] + 12
        f = 440 * 2 ** ((m - 69) / 12)
        for j in range(int(0.6 * RATE)):
            idx = int(start * RATE) + j
            if idx >= n:
                idx -= n  # wraps around the loop
            t = j / RATE
            out[idx] += 0.16 * (math.sin(2 * math.pi * f * t) + 0.4 * math.sin(4 * math.pi * f * t)) * math.exp(-t / 0.22)
        k += 1
    # soft drum on beats 1 and 3
    k = 0
    while k * beat < total:
        if k % 2 == 0:
            for j in range(int(0.3 * RATE)):
                idx = int(k * beat * RATE) + j
                if idx < n:
                    t = j / RATE
                    out[idx] += 0.35 * math.sin(2 * math.pi * (90 - 50 * t) * t) * math.exp(-t / 0.09)
        k += 1
    return out


if __name__ == "__main__":
    os.makedirs(GAMES, exist_ok=True)
    os.makedirs(AUDIO, exist_ok=True)
    shadow()
    slash()
    sparkle()
    disc("disc", False)
    disc("ring", True)
    amber()
    archer()
    brute()
    pot()
    chest(False)
    chest(True)
    sign()
    spikes()
    plate()
    arrow()
    rock()
    write("swing.wav", swing())
    write("hit.wav", hit())
    write("hurt.wav", hurt())
    write("gem.wav", gem())
    write("slam.wav", slam())
    write("portal.wav", portal())
    write("ambre_music.wav", music())
