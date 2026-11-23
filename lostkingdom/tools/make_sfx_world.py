#!/usr/bin/env python3
"""LOST KINGDOM 3.7.1 — a fuller soundscape (synthesised, no samples).

Ambience loops:
  amb_meadow      grasshoppers and crickets in the sun, a soft breeze in the grass, a far lark now and then
  amb_underwater  the deep muffled rumble under the water, slow bubbles
  amb_desert      a dry wind hissing over sand, grains ticking
One-shots (played around the hero by Game::updateSoundscape, or by the actions):
  bird_chirp, bird_chirp2, bird_chirp3   three songbirds (trills, whistles, a warble)
  gull, eagle, owl, frog, crow, cuckoo   animal calls
  bubbles, chop, pick_stone, tree_fall, leaves, item_get
Usage: python3 -I tools/make_sfx_world.py
"""
import os
import sys, sys
import numpy as np
from scipy import signal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_audio as MA  # noqa: E402  (write_ogg, loopify)

SR = MA.SR
OUT = os.path.join(HERE, "..", "assets", "audio")
rng = np.random.default_rng(3707)


def t_(d): return np.arange(int(d * SR)) / SR
def lp(x, f, o=2): b, a = signal.butter(o, min(0.99, f / (SR / 2)), "low"); return signal.lfilter(b, a, x)
def hp(x, f, o=2): b, a = signal.butter(o, min(0.99, f / (SR / 2)), "high"); return signal.lfilter(b, a, x)
def bp(x, lo, hi, o=2): b, a = signal.butter(o, [max(10, lo) / (SR / 2), min(0.99, hi / (SR / 2))], "band"); return signal.lfilter(b, a, x)
def norm(x, peak=0.9): return x / (np.max(np.abs(x)) + 1e-9) * peak
def env(n, a, d): e = np.ones(n); na = max(1, int(a * SR)); nd = max(1, int(d * SR)); e[:na] = np.linspace(0, 1, na); e[-nd:] *= np.linspace(1, 0, nd); return e
def add(dst, src, at):
    i = int(at * SR)
    if i >= len(dst): return
    n = min(len(src), len(dst) - i); dst[i:i + n] += src[:n]


def tone(f0, f1, d, harm=(1.0,), vib=0.0, vibf=6.0):
    """A pitch glide from f0 to f1 (Hz) with harmonics and vibrato."""
    t = t_(d)
    f = np.geomspace(max(f0, 1), max(f1, 1), len(t)) * (1 + vib * np.sin(2 * np.pi * vibf * t))
    ph = 2 * np.pi * np.cumsum(f) / SR
    return sum(a * np.sin(ph * (k + 1)) for k, a in enumerate(harm))


# ------------------------------------------------------------------------------------------------- one-shots
def bird(kind):
    out = np.zeros(int(1.6 * SR))
    if kind == 0:     # a fast trill of rising notes
        for i in range(rng.integers(6, 11)):
            f = 3600 + rng.uniform(-300, 300) + i * 60
            n = tone(f, f * 1.25, 0.045, (1.0, 0.15)) * env(int(0.045 * SR), 0.005, 0.02)
            add(out, n, 0.05 + i * 0.065)
    elif kind == 1:   # two clear whistles, the second falling
        add(out, tone(2600, 3200, 0.22, (1.0, 0.08), 0.01) * env(int(0.22 * SR), 0.02, 0.08), 0.05)
        add(out, tone(3300, 2300, 0.3, (1.0, 0.08), 0.015) * env(int(0.3 * SR), 0.02, 0.12), 0.38)
    else:             # a warble
        d = 0.9
        add(out, tone(2900, 3400, d, (1.0, 0.2, 0.05), 0.08, 22.0) * env(int(d * SR), 0.03, 0.25), 0.05)
    return norm(hp(out, 1500), 0.6)


def gull():
    out = np.zeros(int(1.5 * SR))
    for i in range(3):
        d = 0.18 if i < 2 else 0.4
        n = tone(1300, 900, d, (1.0, 0.6, 0.35, 0.2), 0.02) * env(int(d * SR), 0.01, d * 0.6)
        n = n + 0.3 * bp(rng.standard_normal(len(n)), 1000, 3000) * env(len(n), 0.01, d * 0.6)
        add(out, n, 0.05 + i * 0.24)
    return norm(bp(out, 500, 5000), 0.65)


def eagle():
    d = 1.2
    n = tone(2400, 1500, d, (1.0, 0.3, 0.12), 0.03, 30.0) * env(int(d * SR), 0.02, 0.6)
    n += 0.25 * bp(rng.standard_normal(len(n)), 1500, 4500) * env(len(n), 0.01, 0.6)
    out = np.zeros(int(1.5 * SR)); add(out, n, 0.05)
    return norm(out, 0.6)


def owl():
    out = np.zeros(int(2.2 * SR))
    for at, d in ((0.05, 0.25), (0.55, 0.12), (0.75, 0.7)):
        n = tone(420, 380, d, (1.0, 0.25)) * env(int(d * SR), 0.04, d * 0.5)
        add(out, lp(n, 1200), at)
    return norm(out, 0.55)


def frog():
    out = np.zeros(int(0.8 * SR))
    for i in range(2):
        d = 0.16
        t = t_(d)
        pulses = (np.sin(2 * np.pi * 38 * t) > 0.2).astype(float)
        n = tone(220, 180, d, (1.0, 0.7, 0.5, 0.3)) * pulses * env(len(t), 0.005, 0.05)
        add(out, n, 0.05 + i * 0.3)
    return norm(lp(out, 2500), 0.6)


def crow():
    out = np.zeros(int(1.3 * SR))
    for i in range(rng.integers(2, 4)):
        d = 0.25
        n = bp(rng.standard_normal(int(d * SR)), 600, 2200) * 0.7 + tone(700, 520, d, (1.0, 0.8, 0.6, 0.4)) * 0.5
        add(out, n * env(int(d * SR), 0.01, 0.12), 0.05 + i * 0.34)
    return norm(out, 0.55)


def cuckoo():
    out = np.zeros(int(1.4 * SR))
    add(out, tone(760, 740, 0.28, (1.0, 0.1)) * env(int(0.28 * SR), 0.02, 0.1), 0.05)
    add(out, tone(620, 600, 0.38, (1.0, 0.1)) * env(int(0.38 * SR), 0.02, 0.2), 0.42)
    return norm(lp(out, 2000), 0.5)


def bubbles():
    out = np.zeros(int(1.4 * SR))
    for _ in range(rng.integers(6, 12)):
        d = rng.uniform(0.04, 0.09); f = rng.uniform(500, 1400)
        add(out, tone(f, f * 1.8, d) * env(int(d * SR), 0.003, d * 0.6), rng.uniform(0.02, 1.2))
    return norm(lp(out, 3000), 0.55)


def chop():
    d = 0.5; t = t_(d)
    thunk = sum(np.sin(2 * np.pi * f * t) * np.exp(-t * k) for f, k in ((180, 18), (410, 26), (930, 40)))
    crack = hp(rng.standard_normal(len(t)), 1800) * np.exp(-t * 60) * 0.8
    chips = bp(rng.standard_normal(len(t)), 2500, 7000) * np.exp(-t * 14) * 0.25
    return norm(thunk + crack + chips, 0.85)


def pick_stone():
    d = 0.6; t = t_(d)
    ring = sum(a * np.sin(2 * np.pi * f * t) * np.exp(-t * k) for f, a, k in ((2300, 1.0, 9), (3410, 0.6, 12), (5150, 0.35, 16)))
    hit = hp(rng.standard_normal(len(t)), 900) * np.exp(-t * 45)
    grit = bp(rng.standard_normal(len(t)), 300, 2000) * np.exp(-t * 9) * 0.4
    return norm(ring * 0.5 + hit + grit, 0.8)


def tree_fall():
    d = 3.2; t = t_(d)
    out = np.zeros(len(t))
    # creaking wood: a slow scraping resonance that rises
    cre = tone(70, 140, 1.0, (1.0, 0.7, 0.5, 0.4, 0.3), 0.05, 13.0) * env(int(1.0 * SR), 0.15, 0.3)
    add(out, lp(cre, 1500) * 0.5, 0.0)
    # rushing leaves while it falls
    rush = bp(rng.standard_normal(int(0.9 * SR)), 1500, 7000) * np.linspace(0, 1, int(0.9 * SR)) ** 2 * 0.45
    add(out, rush, 0.45)
    # the crash: a deep thud, branches snapping, leaves settling
    tt = t_(1.6)
    thud = np.sin(2 * np.pi * 48 * tt) * np.exp(-tt * 5) * 1.2 + lp(rng.standard_normal(len(tt)), 300) * np.exp(-tt * 7)
    snaps = np.zeros(len(tt))
    for _ in range(9):
        i = int(rng.uniform(0, 0.5) * SR); n = int(0.03 * SR)
        snaps[i:i + n] += hp(rng.standard_normal(n), 2000) * np.exp(-np.arange(n) / (0.006 * SR))
    settle = bp(rng.standard_normal(len(tt)), 2000, 8000) * np.exp(-tt * 2.5) * 0.3
    add(out, thud + snaps * 0.6 + settle, 1.35)   # the crash lands with the animation (Forestry.cpp: 1.35 s)
    return norm(out, 0.9)


def leaves():
    d = 0.7; t = t_(d)
    n = bp(rng.standard_normal(len(t)), 1800, 8000) * (0.5 + 0.5 * np.abs(np.sin(2 * np.pi * 7 * t)))
    return norm(n * env(len(t), 0.05, 0.35), 0.5)


def item_get():
    out = np.zeros(int(1.4 * SR))
    for i, f in enumerate((784, 988, 1175, 1568)):
        d = 0.6
        n = tone(f, f, d, (1.0, 0.3, 0.1)) * np.exp(-t_(d) * 5)
        add(out, n, i * 0.08)
    out += bp(rng.standard_normal(len(out)), 5000, 12000) * np.exp(-np.arange(len(out)) / (0.25 * SR)) * 0.15
    return norm(out, 0.6)


# ------------------------------------------------------------------------------------------------- loops
def amb_meadow(d=24.0):
    t = t_(d); n = len(t)
    breeze = lp(hp(rng.standard_normal(n), 300), 3500) * (0.5 + 0.5 * lp(rng.standard_normal(n), 0.25) * 30)
    out = breeze * 0.12
    # grasshoppers: bursts of fast stridulation, several insects at different pitches
    for _ in range(26):
        at = rng.uniform(0, d - 2); dur = rng.uniform(0.6, 1.8); f = rng.uniform(5500, 8500)
        tt = t_(dur)
        pulse = (np.sin(2 * np.pi * rng.uniform(25, 45) * tt) > 0.3).astype(float)
        add(out, np.sin(2 * np.pi * f * tt) * pulse * env(len(tt), 0.1, 0.3) * rng.uniform(0.05, 0.12), at)
    # a far lark, twice
    for at in (5.0, 16.0):
        for i in range(14):
            f = 3200 + 500 * np.sin(i * 1.7)
            add(out, tone(f, f * 1.15, 0.05) * env(int(0.05 * SR), 0.005, 0.02) * 0.05, at + i * 0.08)
    return MA.loopify(norm(out, 0.5))


def amb_underwater(d=20.0):
    t = t_(d); n = len(t)
    rumble = lp(rng.standard_normal(n), 180) * 6 * (0.7 + 0.3 * np.sin(2 * np.pi * 0.07 * t))
    hum = lp(rng.standard_normal(n), 900) * 0.6
    out = rumble + hum
    for _ in range(18):
        b = bubbles() * 0.25
        add(out, b, rng.uniform(0, d - 1.5))
    return MA.loopify(norm(out, 0.55))


def amb_desert(d=22.0):
    t = t_(d); n = len(t)
    gust = 0.5 + 0.5 * np.clip(lp(rng.standard_normal(n), 0.3) * 40, -1, 1)
    hiss = hp(lp(rng.standard_normal(n), 6000), 1200) * gust
    low = lp(rng.standard_normal(n), 250) * 2 * gust
    grains = np.zeros(n)
    for _ in range(900):
        i = rng.integers(0, n - 200); grains[i:i + 60] += hp(rng.standard_normal(60), 5000) * rng.uniform(0.05, 0.2)
    return MA.loopify(norm(hiss * 0.5 + low * 0.3 + grains * gust, 0.5))


def voice():
    # 3.8.0 a soft spoken syllable for the dialogue typewriter: a glottal pulse train through two vowel formants
    d = 0.085; t = t_(d)
    f0 = 190 * (1 + 0.06 * np.sin(2 * np.pi * 9 * t))
    ph = np.cumsum(f0) / SR
    pulse = (np.mod(ph, 1.0) < 0.35).astype(float) - 0.35
    v = bp(pulse, 560, 860, 2) * 0.9 + bp(pulse, 1050, 1450, 2) * 0.45 + bp(pulse, 2300, 2900, 2) * 0.12
    e = np.minimum(1, t / 0.012) * np.exp(-np.maximum(0, t - 0.02) * 32)
    return norm(lp(v * e, 4200), 0.55)


def hoof(i):
    # 3.8.1 one hoof on packed earth: a low thump, the click of the shoe, a little grit
    r = np.random.default_rng(40 + i); d = 0.22; t = t_(d); n = len(t)
    thump = np.sin(2 * np.pi * (70 + 15 * i) * t * (1 - 0.4 * t / d)) * np.exp(-t * 38)
    click = bp(r.standard_normal(n), 1800 + 300 * i, 4200, 2) * np.exp(-t * 160) * 0.5
    grit = bp(r.standard_normal(n), 500, 2500, 2) * np.exp(-t * 22) * 0.18
    return norm(thump + click + grit, 0.8)


def horse_snort():
    r = np.random.default_rng(77); d = 0.9; t = t_(d); n = len(t)
    air = bp(r.standard_normal(n), 250, 1600, 2)
    flutter = 1 + 0.6 * np.sin(2 * np.pi * 34 * t)
    e = np.minimum(1, t / 0.05) * np.exp(-np.maximum(0, t - 0.12) * 5.5)
    return norm(air * flutter * e + lp(r.standard_normal(n), 200) * e * 0.4, 0.6)


def saddle_creak():
    r = np.random.default_rng(91); d = 0.35; t = t_(d)
    f = 140 + 60 * np.sin(2 * np.pi * 3 * t)
    ph = np.cumsum(f) / SR
    stick = (np.mod(ph, 1.0) < 0.15).astype(float)
    v = bp(stick, 300, 1800, 2) * np.sin(np.pi * t / d) ** 2
    jingle = sum(np.sin(2 * np.pi * fr * t) * np.exp(-(t - at).clip(0) * 30) * (t > at) for fr, at in ((3200, 0.05), (4100, 0.12), (3700, 0.2)))
    return norm(v + jingle * 0.08, 0.4)


def amb_town(d=24.0):
    # 3.9.0 a busy market town: a murmur of many voices (formant syllables at random), footsteps on cobbles,
    # a far-off hammer, a cart, now and then a laugh
    rng = np.random.default_rng(311); n = int(d * SR); out = np.zeros(n)
    vowels = ((700, 1200), (400, 2000), (550, 900), (300, 2300), (600, 1700))
    for _ in range(1400):
        dur = rng.uniform(0.06, 0.16); t = t_(dur); f0 = rng.uniform(110, 260) * (1 + 0.08 * np.sin(2 * np.pi * rng.uniform(3, 7) * t))
        ph = np.cumsum(f0) / SR; pulse = (np.mod(ph, 1.0) < 0.4).astype(float) - 0.4
        F1, F2 = vowels[rng.integers(0, len(vowels))]
        syl = bp(pulse, F1 * 0.8, F1 * 1.2, 2) + 0.5 * bp(pulse, F2 * 0.85, F2 * 1.15, 2)
        syl *= np.sin(np.pi * t / dur) ** 2 * rng.uniform(0.2, 1.0)
        add(out, syl, rng.uniform(0, d - dur))
    out = lp(out, 2600) * 0.9
    for _ in range(70):   # steps on the cobbles
        at = rng.uniform(0, d - 0.1); t = t_(0.05)
        add(out, bp(rng.standard_normal(len(t)), 900, 3500, 2) * np.exp(-t * 90) * rng.uniform(0.04, 0.12), at)
    for k in range(18):   # the smith, far away
        at = 3.0 + k * 0.62 + (k // 6) * 2.5; t = t_(0.4)
        if at < d - 0.5: add(out, (np.sin(2 * np.pi * 1840 * t) + 0.6 * np.sin(2 * np.pi * 2710 * t)) * np.exp(-t * 14) * 0.05, at)
    for at in (6.5, 17.0):   # a laugh
        for j in range(5):
            t = t_(0.09); f = 300 + rng.uniform(-20, 20)
            add(out, np.sin(2 * np.pi * f * t) * np.sin(np.pi * t / 0.09) ** 2 * 0.12, at + j * 0.13)
    return MA.loopify(norm(out, 0.5))


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--town":
        MA.write_ogg(os.path.join(OUT, "amb", "amb_town.ogg"), amb_town()); print("wrote amb_town"); return
    if len(sys.argv) > 1 and sys.argv[1] == "--ride":
        for i in range(3): MA.write_ogg(os.path.join(OUT, "sfx", "hoof%d.ogg" % (i + 1)), hoof(i))
        MA.write_ogg(os.path.join(OUT, "sfx", "horse_snort.ogg"), horse_snort())
        MA.write_ogg(os.path.join(OUT, "sfx", "saddle_creak.ogg"), saddle_creak()); print("wrote ride sounds"); return
    if len(sys.argv) > 1 and sys.argv[1] == "--voice":
        MA.write_ogg(os.path.join(OUT, "sfx", "voice.ogg"), voice()); print("wrote voice"); return
    sfx = {"bird_chirp": bird(0), "bird_chirp2": bird(1), "bird_chirp3": bird(2), "gull": gull(), "eagle": eagle(), "owl": owl(),
           "frog": frog(), "crow": crow(), "cuckoo": cuckoo(), "bubbles": bubbles(), "chop": chop(), "pick_stone": pick_stone(),
           "tree_fall": tree_fall(), "leaves": leaves(), "item_get": item_get(), "voice": voice(), "hoof1": hoof(0), "hoof2": hoof(1), "hoof3": hoof(2), "horse_snort": horse_snort(), "saddle_creak": saddle_creak()}
    for k, v in sfx.items():
        MA.write_ogg(os.path.join(OUT, "sfx", k + ".ogg"), v)
    for k, f in (("amb_meadow", amb_meadow), ("amb_underwater", amb_underwater), ("amb_desert", amb_desert), ("amb_town", amb_town)):
        MA.write_ogg(os.path.join(OUT, "amb", k + ".ogg"), f())
    print("wrote", len(sfx), "sounds and 3 ambiences")


if __name__ == "__main__":
    main()
