#!/usr/bin/env python3
"""LOST KINGDOM 2.3.4 — more realistic sound effects and ambiences (physically inspired synthesis, no samples).

Replaces the most heard sounds of tools/make_audio.py:
  footsteps  : heel + ball impacts, surface textures (grass leaves, gravel grit, snow crunch, hollow wood, water),
               3 variants each (step_grass, step_grass2, step_grass3 ...)
  combat     : sword whoosh (moving resonance + aeolian tone), flesh impacts, metal plate modes with beating,
               shield block, blade-on-blade parry with scrape
  movement   : jump, land (thud + armour jingle), roll (tumble + rustle)
  horse      : hoof clops with the hollow hoof resonance, 3 variants; a human two-tone whistle
  ambiences  : forest birdsong (several species), night crickets, gusty wind, babbling river, rain on leaves,
               and a new sea surf loop (amb_sea)
Usage: python3 -I tools/make_sfx_real.py [sfx] [amb]
"""
import os, sys, zlib
import numpy as np
from scipy import signal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_audio as MA  # noqa: E402  (write_ogg, loopify, filters)

SR = 48000
OUT = os.path.join(HERE, "..", "assets", "audio")


def t_(d): return np.arange(int(d * SR)) / SR
def lp(x, f, o=2): b, a = signal.butter(o, min(0.99, f / (SR / 2)), "low"); return signal.lfilter(b, a, x)
def hp(x, f, o=2): b, a = signal.butter(o, min(0.99, f / (SR / 2)), "high"); return signal.lfilter(b, a, x)
def bp(x, lo, hi, o=2): b, a = signal.butter(o, [max(10, lo) / (SR / 2), min(0.99, hi / (SR / 2))], "band"); return signal.lfilter(b, a, x)
def norm(x, peak=0.9): return x / (np.max(np.abs(x)) + 1e-9) * peak


def reson(x, f, q):
    """2-pole resonator (a mode)."""
    w = 2 * np.pi * f / SR; r = np.exp(-w / (2 * q))
    return signal.lfilter([1 - r], [1, -2 * r * np.cos(w), r * r], x)


def add(dst, src, at):
    s = int(at * SR)
    if s >= len(dst): return
    n = min(len(src), len(dst) - s); dst[s:s + n] += src[:n]


def grains(rng, d, count, lo, hi, glen=(0.002, 0.006), dens=None, amp=(0.3, 1.0)):
    """Granular texture: many tiny filtered noise bursts (leaves, grit, snow crystals)."""
    x = np.zeros(int(d * SR))
    for _ in range(count):
        at = rng.beta(1.2, 3.0) * d if dens is None else dens(rng) * d
        L = rng.uniform(*glen); n = int(L * SR)
        g = rng.standard_normal(n) * np.exp(-np.arange(n) / (n * 0.3))
        f0 = rng.uniform(lo, hi)
        g = bp(g, f0 * 0.6, f0 * 1.5, 1)
        add(x, g * rng.uniform(*amp) * np.exp(-at / (d * 0.3)), at)   # the texture dies away with the impact
    return x


def impact(rng, d, low_f, body_q, click=0.5, thud=1.0):
    t = t_(d)
    n = rng.standard_normal(len(t))
    x = reson(n * np.exp(-t * 60), low_f, body_q) * thud
    x += hp(n, 2500) * np.exp(-t * 400) * click
    return x


# ------------------------------------------------------------------------------------------- footsteps
def step(surface, seed):
    rng = np.random.default_rng(seed)
    d = 0.32 if surface != "snow" else 0.38
    x = np.zeros(int(d * SR))
    gap = rng.uniform(0.045, 0.075)           # heel ... ball of the foot
    if surface == "grass":
        for k, (at, a) in enumerate(((0.0, 1.0), (gap, 0.6))):
            add(x, impact(rng, 0.12, rng.uniform(110, 160), 2.5, 0.15, 0.9 * a), at)
            add(x, grains(rng, 0.14, 40, 2500, 8000, (0.002, 0.006), amp=(0.1, 0.45)) * a, at + 0.004)
        x += lp(rng.standard_normal(len(x)), 1800) * np.exp(-t_(d) * 22) * 0.05     # stems brushing
    elif surface == "stone":
        for at, a in ((0.0, 1.0), (gap, 0.55)):
            add(x, impact(rng, 0.1, rng.uniform(140, 200), 4, 0.8, 0.7 * a), at)
            add(x, grains(rng, 0.1, 22, 1500, 6000, (0.001, 0.003), amp=(0.15, 0.6)) * a, at + 0.002)
        scrape = bp(rng.standard_normal(int(0.07 * SR)), 1200, 5000) * np.sin(np.pi * np.linspace(0, 1, int(0.07 * SR))) * 0.08
        add(x, scrape, gap + 0.01)
    elif surface == "snow":
        for at, a in ((0.0, 1.0), (gap + 0.02, 0.7)):
            n = int(0.2 * SR); env = np.minimum(1, np.arange(n) / (0.025 * SR)) * np.exp(-np.arange(n) / (0.07 * SR))
            crunch = lp(grains(rng, 0.2, 160, 600, 3500, (0.002, 0.006), dens=lambda r: r.uniform(0, 0.8), amp=(0.2, 0.8)), 3800) * env
            squeak = sum(np.sin(2 * np.pi * rng.uniform(900, 1700) * t_(0.2)) * np.exp(-((t_(0.2) - rng.uniform(0.02, 0.12)) / 0.008) ** 2) for _ in range(3)) * 0.12
            add(x, (crunch + squeak) * a, at)
            add(x, impact(rng, 0.1, 90, 2, 0.0, 0.5 * a), at)
    elif surface == "sand":
        for at, a in ((0.0, 1.0), (gap + 0.01, 0.65)):
            n = int(0.16 * SR); env = np.minimum(1, np.arange(n) / (0.012 * SR)) * np.exp(-np.arange(n) / (0.045 * SR))
            add(x, lp(grains(rng, 0.16, 90, 700, 3000, (0.002, 0.005), dens=lambda r: r.uniform(0, 0.7), amp=(0.2, 0.7)), 2800) * env * a, at)
            add(x, impact(rng, 0.1, 100, 2, 0.0, 0.45 * a), at)
    elif surface == "wood":
        for at, a in ((0.0, 1.0), (gap, 0.6)):
            ex = rng.standard_normal(int(0.006 * SR)) * np.hanning(int(0.006 * SR))
            e = np.zeros(int(0.28 * SR)); e[:len(ex)] = ex
            body = sum(reson(e, f * rng.uniform(0.97, 1.03), q) * g for f, q, g in ((165, 9, 1.0), (390, 12, 0.6), (720, 14, 0.4), (1180, 16, 0.25), (2100, 10, 0.12)))
            add(x, body * a * 1.6, at)
            add(x, hp(rng.standard_normal(int(0.004 * SR)), 3000) * 0.4 * a, at)
        if rng.random() < 0.5:   # a board creaks a little
            n = int(0.12 * SR); tt = np.arange(n) / SR
            cr = signal.sawtooth(2 * np.pi * np.cumsum(np.full(n, rng.uniform(70, 110)) * (1 + 0.3 * np.sin(2 * np.pi * 9 * tt))) / SR)
            add(x, bp(cr, 400, 1800) * np.sin(np.pi * tt / tt[-1]) * 0.05, gap + 0.03)
    elif surface == "water":
        for at, a in ((0.0, 1.0), (gap + 0.03, 0.6)):
            n = int(0.22 * SR); tt = np.arange(n) / SR
            spl = bp(rng.standard_normal(n), 350, 6000) * (1 - np.exp(-tt * 300)) * np.exp(-tt * 18) * 0.7
            for _ in range(10):   # bubbles: rising sine chirps
                m = int(rng.uniform(0.01, 0.03) * SR); c = np.arange(m) / SR; f = rng.uniform(400, 1300)
                add(spl, np.sin(2 * np.pi * f * c * (1 + 25 * c)) * np.exp(-c * rng.uniform(60, 120)) * rng.uniform(0.1, 0.3), rng.uniform(0.02, 0.15))
            add(x, spl * a, at)
            add(x, impact(rng, 0.1, 80, 1.5, 0, 0.4 * a), at)
    x *= np.r_[np.ones(len(x) - 480), np.linspace(1, 0, 480)]
    return x


# ------------------------------------------------------------------------------------------- combat
def whoosh(d, peak, f_lo, f_hi, seed, tone=0.08):
    """Air torn by a moving blade: noise through a resonance that follows the blade speed (bell curve)."""
    rng = np.random.default_rng(seed)
    n = int(d * SR); tt = np.arange(n) / SR
    sp = np.exp(-((tt / d - peak) / 0.18) ** 2)                      # blade speed profile
    f = f_lo + (f_hi - f_lo) * sp
    x = np.zeros(n); src = rng.standard_normal(n); blk = 256
    zi = None
    for s in range(0, n, blk):
        fc = f[s]; b, a = signal.butter(2, [fc * 0.55 / (SR / 2), min(0.99, fc * 1.6 / (SR / 2))], "band")
        if zi is None or len(zi) != max(len(a), len(b)) - 1: zi = signal.lfilter_zi(b, a) * 0
        x[s:s + blk], zi = signal.lfilter(b, a, src[s:s + blk], zi=zi)
    x *= sp ** 1.6
    ph = np.cumsum(f * 1.1) / SR
    x += np.sin(2 * np.pi * ph) * sp ** 3 * tone                     # faint aeolian whistle of the edge
    return x


def metal(f0, d, seed, ratios=(1.0, 2.32, 4.25, 6.63, 9.38, 12.1), decay=(5, 7, 10, 13, 17, 22), beat=0.004, noise_amt=0.6):
    rng = np.random.default_rng(seed); tt = t_(d)
    x = np.zeros(len(tt))
    for r, k in zip(ratios, decay):
        f = f0 * r * rng.uniform(0.985, 1.015); a = rng.uniform(0.5, 1.0) / (1 + 0.15 * r)
        x += a * np.exp(-tt * k) * (np.sin(2 * np.pi * f * tt) + 0.6 * np.sin(2 * np.pi * f * (1 + beat) * tt + rng.uniform(0, 6)))
    x += hp(rng.standard_normal(len(tt)), 2500) * np.exp(-tt * 70) * noise_amt
    return x


def sfx(name):
    rng = np.random.default_rng(zlib.crc32(name.encode()))
    if name == "swing": return whoosh(0.34, 0.45, 450, 2600, 11)
    if name == "swing_heavy": return whoosh(0.55, 0.5, 260, 1500, 12, 0.05) + lp(rng.standard_normal(int(0.55 * SR)), 300) * np.exp(-((t_(0.55) - 0.27) / 0.1) ** 2) * 0.2
    if name == "hit_flesh":
        d = 0.32; tt = t_(d); n = rng.standard_normal(len(tt))
        thump = np.sin(2 * np.pi * np.cumsum(110 * np.exp(-tt * 9) + 55) / SR) * np.exp(-tt * 22)
        body = lp(n, 900) * np.exp(-tt * 30) * 0.9
        wet = bp(n, 500, 1800) * np.exp(-tt * 18) * (0.6 + 0.4 * np.sin(2 * np.pi * 37 * tt)) * 0.35
        cloth = hp(n, 3500) * np.exp(-tt * 120) * 0.3
        return thump + body + wet + cloth
    if name == "hit_metal":
        x = metal(rng.uniform(780, 900), 0.7, 21) * 0.6
        x += lp(rng.standard_normal(len(x)), 500) * np.exp(-t_(0.7) * 35) * 0.6           # the blow behind the plate
        return x
    if name == "block":
        d = 0.6; tt = t_(d)
        wood = sum(reson(np.r_[rng.standard_normal(200) * np.hanning(200), np.zeros(len(tt) - 200)], f, q) * g for f, q, g in ((210, 7, 1), (480, 9, .6), (1050, 10, .3)))
        return metal(560, d, 22, decay=(9, 12, 15, 19, 24, 30), noise_amt=0.4) * 0.45 + wood * 1.4 + lp(rng.standard_normal(len(tt)), 300) * np.exp(-tt * 25) * 0.5
    if name == "parry":
        d = 1.1; tt = t_(d)
        x = metal(1240, d, 23, ratios=(1.0, 2.76, 5.4, 8.93, 13.3), decay=(2.5, 3.5, 5, 7, 9), beat=0.006, noise_amt=0.8) * 0.55
        n = int(0.18 * SR)
        scrape = whoosh(0.18, 0.4, 2500, 6500, 24, 0.0) * 1.2
        add(x, scrape, 0.01)
        return x
    if name == "jump":
        d = 0.3; tt = t_(d)
        cloth = bp(rng.standard_normal(len(tt)), 600, 4000) * np.sin(np.pi * tt / d) ** 2 * 0.35
        return cloth + impact(rng, d, 120, 2, 0.3, 0.6) * 0.7
    if name == "land":
        d = 0.45; tt = t_(d)
        x = impact(rng, d, 75, 2.0, 0.4, 1.4) + impact(rng, d, 120, 2.0, 0.2, 0.7) * 0.6
        add(x, grains(rng, 0.25, 50, 2000, 7000, amp=(0.1, 0.4)), 0.004)
        for _ in range(7):   # armour / buckles jingle
            add(x, metal(rng.uniform(2200, 3800), 0.12, int(rng.integers(1e6)), ratios=(1, 2.4, 4.1), decay=(30, 40, 55), noise_amt=0.1) * rng.uniform(0.04, 0.1), rng.uniform(0.01, 0.12))
        return x
    if name == "roll":
        d = 0.6; x = np.zeros(int(d * SR))
        for at in (0.0, 0.16, 0.3, 0.42):
            add(x, impact(rng, 0.15, rng.uniform(80, 130), 2, 0.1, rng.uniform(0.5, 0.9)), at)
        x += bp(rng.standard_normal(len(x)), 500, 5000) * np.sin(np.pi * t_(d) / d) * 0.25
        add(x, grains(rng, 0.5, 120, 2500, 8000, amp=(0.05, 0.25)), 0.02)
        return x
    if name == "whistle":
        # a human two-note whistle: breathy, slight vibrato, glide between the notes
        d = 0.85; tt = t_(d)
        f = np.where(tt < 0.32, 1750, 2350).astype(float)
        f = lp(f, 25, 1) + np.sin(2 * np.pi * 5.5 * tt) * 18
        tone = np.sin(2 * np.pi * np.cumsum(f) / SR)
        env = np.minimum(1, tt / 0.04) * np.minimum(1, (d - tt) / 0.08) * np.where(np.abs(tt - 0.32) < 0.03, 0.55, 1.0)
        breath = bp(rng.standard_normal(len(tt)), 1500, 3500) * 0.12
        return (tone * 0.5 + breath) * env
    if name == "splash":
        d = 0.9; tt = t_(d)
        x = bp(rng.standard_normal(len(tt)), 300, 7000) * (1 - np.exp(-tt * 200)) * np.exp(-tt * 5) * 0.8
        for _ in range(30):
            m = int(rng.uniform(0.01, 0.04) * SR); c = np.arange(m) / SR; f = rng.uniform(300, 1400)
            add(x, np.sin(2 * np.pi * f * c * (1 + 20 * c)) * np.exp(-c * rng.uniform(40, 100)) * rng.uniform(0.1, 0.35), rng.uniform(0.02, 0.6))
        return x + impact(rng, d, 70, 1.5, 0, 0.6)
    return None


def hoof(seed):
    rng = np.random.default_rng(seed); d = 0.22; x = np.zeros(int(d * SR))
    for at, a in ((0.0, 1.0), (rng.uniform(0.018, 0.03), 0.55)):          # toe then heel of the hoof
        ex = np.r_[rng.standard_normal(150) * np.hanning(150), np.zeros(int(0.15 * SR) - 150)]
        hollow = reson(ex, rng.uniform(950, 1250), 7) * 1.4 + reson(ex, rng.uniform(420, 520), 6) * 0.9   # the hoof's cavity
        add(x, (hollow + impact(rng, 0.15, 140, 2, 0.3, 0.9)) * a, at)
    add(x, grains(rng, 0.12, 30, 1500, 6000, amp=(0.05, 0.25)), 0.01)      # dirt kicked
    return x


# ------------------------------------------------------------------------------------------- ambiences
def bird_call(rng, kind):
    if kind == 0:   # warbler: fast trill of FM chirps
        notes = rng.integers(6, 14); f0 = rng.uniform(3500, 5200); out = []
        for i in range(notes):
            L = rng.uniform(0.035, 0.06); c = t_(L)
            f = f0 * (1 + 0.25 * np.sin(np.pi * c / L)) * (1 - 0.01 * i)
            ph = np.cumsum(f) / SR
            out.append((np.sin(2 * np.pi * ph) + 0.25 * np.sin(4 * np.pi * ph)) * np.sin(np.pi * c / L) ** 2)
            out.append(np.zeros(int(rng.uniform(0.01, 0.03) * SR)))
        return np.concatenate(out)
    if kind == 1:   # blackbird-like fluted phrase: slow glides with vibrato
        out = []
        for _ in range(rng.integers(3, 6)):
            L = rng.uniform(0.12, 0.3); c = t_(L); fa, fb = rng.uniform(1600, 2800, 2)
            f = fa + (fb - fa) * (c / L) ** 1.5 + np.sin(2 * np.pi * 28 * c) * 40
            ph = np.cumsum(f) / SR
            out.append((np.sin(2 * np.pi * ph) + 0.15 * np.sin(4 * np.pi * ph)) * np.sin(np.pi * c / L))
            out.append(np.zeros(int(rng.uniform(0.03, 0.08) * SR)))
        return np.concatenate(out)
    if kind == 2:   # distant cuckoo-like two notes
        out = []
        for f in (680, 560):
            L = 0.22; c = t_(L); out.append(np.sin(2 * np.pi * f * c) * np.sin(np.pi * c / L) ** 1.5); out.append(np.zeros(int(0.12 * SR)))
        return lp(np.concatenate(out), 1500) * 0.6
    # sparrow chirps
    out = []
    for _ in range(rng.integers(2, 5)):
        L = rng.uniform(0.05, 0.09); c = t_(L); f = rng.uniform(3000, 4500) - 1500 * c / L
        out.append(np.sin(2 * np.pi * np.cumsum(f) / SR) * np.sin(np.pi * c / L)); out.append(np.zeros(int(rng.uniform(0.05, 0.15) * SR)))
    return np.concatenate(out)


def place(st, mono, at, pan_, gain, dist):
    """Put a mono source into the stereo field, farther = quieter and duller."""
    if dist > 0: mono = lp(mono, 9000 - 6500 * dist)
    a = (pan_ + 1) * np.pi / 4
    s = int(at * SR); n = min(len(mono), st.shape[1] - s)
    if n <= 0: return
    st[0, s:s + n] += mono[:n] * np.cos(a) * gain
    st[1, s:s + n] += mono[:n] * np.sin(a) * gain


def gust_env(rng, d, rate=0.12, lo=0.25):
    n = int(d * SR); k = max(4, int(d * rate * 8))
    pts = rng.uniform(lo, 1.0, k); pts[-1] = pts[0]
    env = np.interp(np.linspace(0, k - 1, n), np.arange(k), pts)
    return lp(env, 0.6, 1)


def amb(name, d=24.0):
    rng = np.random.default_rng(zlib.crc32(name.encode()))
    n = int(d * SR); st = np.zeros((2, n))
    noiseL, noiseR = rng.standard_normal(n), rng.standard_normal(n)
    if name == "amb_forest_day":
        g = gust_env(rng, d, 0.1, 0.3)
        st[0] += bp(noiseL, 300, 3500) * g * 0.05; st[1] += bp(noiseR, 300, 3500) * g * 0.05          # leaves in the breeze
        for _ in range(55):
            kind = int(rng.choice([0, 1, 1, 3, 3, 2], 1)[0]); call = bird_call(rng, kind)
            place(st, call, rng.uniform(0, d - 2), rng.uniform(-0.9, 0.9), rng.uniform(0.05, 0.16), rng.uniform(0.0, 0.8))
    elif name == "amb_night":
        st[0] += lp(noiseL, 500) * 0.02; st[1] += lp(noiseR, 500) * 0.02
        for c in range(6):   # crickets: pulse groups at different rates, pitches, places
            f = rng.uniform(3800, 5200); rate = rng.uniform(1.2, 2.6); pulses = int(rng.integers(3, 5)); pan_ = rng.uniform(-0.9, 0.9); g = rng.uniform(0.02, 0.05)
            chirp = np.concatenate([np.r_[np.sin(2 * np.pi * f * t_(0.012)) * np.hanning(int(0.012 * SR)), np.zeros(int(0.01 * SR))] for _ in range(pulses)])
            tt = rng.uniform(0, 1 / rate)
            while tt < d - 0.2:
                place(st, chirp, tt, pan_, g * rng.uniform(0.8, 1.0), rng.uniform(0, 0.5)); tt += 1 / rate * rng.uniform(0.95, 1.05)
        for _ in range(3):   # owl
            at = rng.uniform(1, d - 3)
            for k, (f, L) in enumerate(((410, 0.35), (380, 0.18), (390, 0.6))):
                c = t_(L); hoot = np.sin(2 * np.pi * (f - 30 * c / L) * c) * np.sin(np.pi * c / L) ** 2
                place(st, lp(hoot + 0.2 * np.sin(4 * np.pi * f * c) * np.sin(np.pi * c / L), 1200), at + k * 0.5, rng.uniform(-0.7, 0.7), 0.08, 0.6)
    elif name == "amb_wind":
        g = gust_env(rng, d, 0.15, 0.2)
        st[0] += (bp(noiseL, 120, 900) * 0.3 + hp(noiseL, 2500) * 0.03) * g
        st[1] += (bp(noiseR, 120, 900) * 0.3 + hp(noiseR, 2500) * 0.03) * np.roll(g, int(0.4 * SR))
        whistle = np.zeros(n); src = rng.standard_normal(n); blk = 4800
        fc = 500 + 350 * g
        for s in range(0, n, blk):
            f = fc[s]; whistle[s:s + blk] = reson(src[s:s + blk], f, 40)[:len(whistle[s:s + blk])]
        st += np.stack([whistle, np.roll(whistle, 500)]) * 0.02 * g ** 2
    elif name == "amb_river":
        bed = np.stack([bp(noiseL, 200, 3000), bp(noiseR, 200, 3000)]) * 0.12
        st += bed * (0.9 + 0.1 * np.sin(2 * np.pi * 0.23 * t_(d)))
        for _ in range(3200):   # babbling: water bubbles of every size
            m = int(rng.uniform(0.006, 0.03) * SR); c = np.arange(m) / SR; f = rng.uniform(250, 2200)
            b = np.sin(2 * np.pi * f * c * (1 + rng.uniform(5, 30) * c)) * np.exp(-c * rng.uniform(60, 200))
            place(st, b, rng.uniform(0, d - 0.05), rng.uniform(-0.8, 0.8), rng.uniform(0.01, 0.05), 0)
    elif name == "amb_rain":
        st += np.stack([bp(noiseL, 500, 9000), bp(noiseR, 500, 9000)]) * 0.06
        st += np.stack([lp(noiseL, 400), lp(noiseR, 400)]) * 0.04
        for _ in range(9000):   # drops on leaves and ground
            m = int(rng.uniform(0.002, 0.008) * SR); dr = rng.standard_normal(m) * np.exp(-np.arange(m) / (m * 0.25))
            f0 = rng.uniform(1500, 8000); dr = bp(dr, f0 * 0.6, min(23000, f0 * 1.6), 1)
            place(st, dr, rng.uniform(0, d - 0.01), rng.uniform(-1, 1), rng.uniform(0.02, 0.1), 0)
        for _ in range(60):   # heavier drips from the branches
            m = int(0.05 * SR); c = np.arange(m) / SR
            place(st, np.sin(2 * np.pi * rng.uniform(600, 1200) * c * (1 + 12 * c)) * np.exp(-c * 70), rng.uniform(0, d - 0.1), rng.uniform(-1, 1), 0.05, 0.2)
    elif name == "amb_sea":
        # surf: each wave swells, breaks (bright), washes up (hiss) and pulls the pebbles back
        tt = t_(d); period = 5.5; k = 0
        while k * period < d + period:
            at = k * period + rng.uniform(-1.2, 1.2); L = rng.uniform(6.5, 8.5)
            m = int(L * SR); c = np.arange(m) / SR
            env = np.where(c < 1.6, (c / 1.6) ** 2, np.exp(-(c - 1.6) * 0.42))
            bright = 600 + 4500 * np.exp(-((c - 1.7) / 0.6) ** 2)
            src = rng.standard_normal(m); w = np.zeros(m); blk = 2400
            for s in range(0, m, blk):
                b, a = signal.butter(2, min(0.99, bright[s] / (SR / 2)), "low")
                w[s:s + blk] = signal.lfilter(b, a, src[s:s + blk])
            w *= env * rng.uniform(0.25, 0.4)
            peb = np.zeros(m)
            for _ in range(250):   # pebbles rolling back
                add(peb, bp(rng.standard_normal(int(0.004 * SR)), 2000, 7000) * np.exp(-np.arange(int(0.004 * SR)) / 40) * rng.uniform(0.02, 0.08), rng.uniform(3.0, L - 0.5))
            for ch, p in ((0, rng.uniform(-0.5, 0)), (1, rng.uniform(0, 0.5))):
                s = int(at * SR)
                for off in (0, -n):   # wrap around: the loop has waves at its seam too
                    a0 = s + off
                    lo = max(0, a0); hi = min(n, a0 + m)
                    if hi > lo: st[ch, lo:hi] += (w + peb)[lo - a0:hi - a0] * (1 + p * (1 if ch else -1))
            k += 1
        st += np.stack([lp(noiseL, 120), lp(noiseR, 120)]) * 0.15        # the low rumble of the open sea
        st += np.stack([bp(noiseL, 150, 2500), bp(noiseR, 150, 2500)]) * 0.07   # the constant wash
        lvl = lp(np.sqrt(np.mean(st ** 2, axis=0)) + 1e-4, 1.5, 1)           # slow compressor: waves stay audible between crests
        st = st / np.maximum(lvl / np.percentile(lvl, 60), 1.0) ** 0.6
    else:
        return None
    st[1] = np.roll(st[1], int(0.007 * SR))
    return norm(MA.loopify(st, 2.0), 0.6)


def main():
    what = sys.argv[1:] or ["sfx", "amb"]
    one = [w[4:] for w in what if w.startswith("amb:")]
    for nm in one:
        MA.write_ogg(os.path.join(OUT, "amb", nm + ".ogg"), amb(nm), q=2); print("amb:", nm)
    if "sfx" in what:
        out = []
        for surf in ("grass", "stone", "snow", "sand", "wood", "water"):
            for v in range(3):
                nm = f"step_{surf}" + ("" if v == 0 else str(v + 1)); out.append((nm, step(surf, 100 + v * 17 + len(surf))))
        for v in range(3): out.append(("horse" + ("" if v == 0 else str(v + 1)), hoof(500 + v)))
        for nm in ("swing", "swing_heavy", "hit_flesh", "hit_metal", "block", "parry", "jump", "land", "roll", "whistle", "splash"):
            out.append((nm, sfx(nm)))
        for nm, x in out:
            x = np.concatenate([x, np.zeros(int(0.02 * SR))])
            x = x * np.r_[np.linspace(0, 1, 24), np.ones(len(x) - 72), np.linspace(1, 0, 48)]
            peak = 0.85 if not nm.startswith(("step_", "horse")) else 0.7
            x = norm(x, peak)
            MA.write_ogg(os.path.join(OUT, "sfx", nm + ".ogg"), np.stack([x, x]), q=4)
        print("sfx:", len(out))
    if "amb" in what:
        for nm in ("amb_forest_day", "amb_night", "amb_wind", "amb_river", "amb_rain", "amb_sea"):
            MA.write_ogg(os.path.join(OUT, "amb", nm + ".ogg"), amb(nm), q=2)
            print("amb:", nm)


if __name__ == "__main__":
    main()
