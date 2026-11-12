#!/usr/bin/env python3
"""Generates the original assets of "Les Ruines d'Ambre" (AGR ENGINE 2.5D game template):
blob shadow, sword slash arc and sparkle sprites (assets/games/) and its sounds and music
(assets/audio/). Everything is drawn / synthesized here; deterministic (fixed seeds).
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
    write("swing.wav", swing())
    write("hit.wav", hit())
    write("hurt.wav", hurt())
    write("gem.wav", gem())
    write("slam.wav", slam())
    write("portal.wav", portal())
    write("ambre_music.wav", music())
