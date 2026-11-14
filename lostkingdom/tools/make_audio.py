#!/usr/bin/env python3
"""LOST KINGDOM — procedural audio production (music, ambiences, sound effects).

Everything is synthesised from scratch (no samples): additive/subtractive synthesis,
Karplus-Strong plucks, FM bells, formant choirs, noise design, convolution reverb.
Output: assets/audio/{music,amb,sfx}/*.ogg (Vorbis, 48 kHz) via ffmpeg.
"""
import os, subprocess, sys, tempfile, wave
import numpy as np
from scipy import signal

SR = 48000
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "assets", "audio")
rng = np.random.default_rng(1234)

# ----------------------------------------------------------------------------------- utilities
def t_(d): return np.arange(int(d * SR)) / SR
def midi(n): return 440.0 * 2 ** ((n - 69) / 12.0)
def env_adsr(n, a, d, s, r, sr=SR):
    a, d, r = int(a * sr), int(d * sr), int(r * sr)
    e = np.ones(n) * s
    if a: e[:min(a, n)] = np.linspace(0, 1, min(a, n))
    if d and a < n: e[a:min(a + d, n)] = np.linspace(1, s, min(a + d, n) - a)
    if r: e[max(0, n - r):] *= np.linspace(1, 0, n - max(0, n - r))
    return e
def lowpass(x, fc, order=2): b, a = signal.butter(order, min(0.99, fc / (SR / 2)), "low"); return signal.lfilter(b, a, x)
def highpass(x, fc, order=2): b, a = signal.butter(order, min(0.99, fc / (SR / 2)), "high"); return signal.lfilter(b, a, x)
def bandpass(x, lo, hi, order=2): b, a = signal.butter(order, [lo / (SR / 2), min(0.99, hi / (SR / 2))], "band"); return signal.lfilter(b, a, x)
def noise(d): return rng.standard_normal(int(d * SR))
def saw(f, d, detune=0.0):
    t = t_(d); ph = (f * (1 + detune)) * t
    return 2 * (ph - np.floor(ph + 0.5))
def norm(x, peak=0.9): m = np.max(np.abs(x)) + 1e-9; return x / m * peak

_ir_cache = {}
def reverb(x, size=2.2, wet=0.3, damp=4000):
    """Convolution reverb with a synthetic stereo impulse response."""
    key = (size, damp)
    if key not in _ir_cache:
        n = int(size * SR)
        tt = np.arange(n) / SR
        irs = []
        for ch in range(2):
            ir = rng.standard_normal(n) * np.exp(-tt * 6.9 / size)
            ir = lowpass(ir, damp)
            ir[:int(0.012 * SR)] *= np.linspace(0, 1, int(0.012 * SR))
            irs.append(ir / np.sqrt(np.sum(ir ** 2)))
        _ir_cache[key] = irs
    irs = _ir_cache[key]
    if x.ndim == 1: x = np.stack([x, x])
    out = np.zeros((2, x.shape[1] + len(irs[0]) - 1))
    for ch in range(2):
        out[ch] = signal.fftconvolve(x[ch], irs[ch])
    dry = np.zeros_like(out); dry[:, :x.shape[1]] = x
    return dry * (1 - wet) + out * wet * 2.5

def pan(x, p):  # p -1..1
    a = (p + 1) * np.pi / 4
    return np.stack([x * np.cos(a), x * np.sin(a)])

def write_ogg(path, x, q=3):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if x.ndim == 1: x = np.stack([x, x])
    x = np.clip(x, -1, 1)
    pcm = (x.T * 32767).astype(np.int16)
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
        tmp = f.name
    with wave.open(tmp, "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", tmp, "-c:a", "libvorbis", "-q:a", str(q), path], check=True)
    os.remove(tmp)

def loopify(x, xf=1.5):
    """Make a seamless loop: fold the tail onto the head with an equal-power crossfade."""
    n = int(xf * SR)
    if x.ndim == 1: x = np.stack([x, x])
    head, tail = x[:, :n], x[:, -n:]
    w = np.linspace(0, 1, n)
    mixed = head * np.sqrt(w) + tail * np.sqrt(1 - w)
    return np.concatenate([mixed, x[:, n:-n]], axis=1)

# ----------------------------------------------------------------------------------- instruments
def strings(freqs, d, bright=2500, att=0.6, rel=0.8, vib=True):
    t = t_(d); n = len(t)
    out = np.zeros(n)
    for f in freqs:
        for k, det in enumerate([-0.004, -0.0015, 0.0, 0.0017, 0.0042]):
            ph0 = rng.random()
            v = 1 + (0.0025 * np.sin(2 * np.pi * (5.2 + k * 0.3) * t + ph0 * 6) if vib else 0)
            ph = np.cumsum(f * (1 + det) * v) / SR + ph0
            out += 2 * (ph - np.floor(ph + 0.5))
    out = lowpass(out, bright, 2)
    return out * env_adsr(n, att, 0.2, 0.85, rel) / (len(freqs) * 4)

def pluck(f, d, bright=0.5, decay=0.996):
    """Karplus-Strong string (harp / lute)."""
    n = int(d * SR); p = max(2, int(SR / f))
    buf = rng.uniform(-1, 1, p)
    buf = lowpass(np.concatenate([buf, buf]), 2000 + 6000 * bright)[p:]
    out = np.zeros(n)
    idx = 0
    b = buf.copy()
    for i in range(n):
        out[i] = b[idx]
        nxt = (idx + 1) % p
        b[idx] = decay * 0.5 * (b[idx] + b[nxt])
        idx = nxt
    return out * env_adsr(n, 0.002, 0.0, 1.0, 0.05)

def pluck_fast(f, d, bright=0.5):
    # vectorised approximation of a plucked string: decaying harmonics
    t = t_(d); out = np.zeros(len(t))
    for h in range(1, 9):
        amp = (1.0 / h) * (bright + (1 - bright) / h)
        out += amp * np.sin(2 * np.pi * f * h * t * (1 + 0.0004 * h * h)) * np.exp(-t * (1.8 + h * 1.4))
    return out * env_adsr(len(t), 0.003, 0, 1, 0.05) * 0.6

def flute(f, d, vib=5.0):
    t = t_(d); n = len(t)
    v = 1 + 0.006 * np.sin(2 * np.pi * vib * t) * np.clip(t / 0.4, 0, 1)
    ph = np.cumsum(f * v) / SR
    x = np.sin(2 * np.pi * ph) + 0.25 * np.sin(4 * np.pi * ph) + 0.08 * np.sin(6 * np.pi * ph)
    breath = bandpass(noise(d), f * 0.8, f * 3) * 0.08
    return (x + breath) * env_adsr(n, 0.08, 0.1, 0.85, 0.18) * 0.5

def horn(f, d):
    t = t_(d); n = len(t)
    x = sum(saw(f, d, det) for det in (-0.003, 0, 0.003)) / 3
    fenv = 600 + 1800 * env_adsr(n, 0.15, 0.4, 0.5, 0.3)
    # time-varying lowpass approximated by splitting into blocks
    out = np.zeros(n); blk = 2048
    zi = None
    for s in range(0, n, blk):
        b, a = signal.butter(2, min(0.99, fenv[s] / (SR / 2)), "low")
        if zi is None: zi = signal.lfilter_zi(b, a) * 0
        out[s:s + blk], zi = signal.lfilter(b, a, x[s:s + blk], zi=zi)
    return out * env_adsr(n, 0.12, 0.2, 0.8, 0.35) * 0.7

def choir(freqs, d, vowel="a"):
    t = t_(d); n = len(t)
    form = {"a": [(730, 90), (1090, 110), (2440, 160)], "o": [(570, 80), (840, 100), (2410, 160)], "u": [(300, 70), (870, 90), (2240, 150)]}[vowel]
    src = np.zeros(n)
    for f in freqs:
        for det in (-0.006, 0.0, 0.005):
            v = 1 + 0.004 * np.sin(2 * np.pi * (4.5 + rng.random()) * t + rng.random() * 6)
            ph = np.cumsum(f * (1 + det) * v) / SR + rng.random()
            src += 2 * (ph - np.floor(ph + 0.5))
    out = np.zeros(n)
    for fc, bw in form:
        out += bandpass(src, fc - bw, fc + bw, 2)
    return out * env_adsr(n, 0.8, 0.3, 0.85, 1.0) / (len(freqs) * 2)

def bell(f, d, idx=2.0):
    t = t_(d)
    mod = np.sin(2 * np.pi * f * 3.5 * t) * idx * np.exp(-t * 3)
    x = np.sin(2 * np.pi * f * t + mod) * np.exp(-t * 2.2)
    x += 0.3 * np.sin(2 * np.pi * f * 2.76 * t) * np.exp(-t * 4)
    return x * env_adsr(len(t), 0.002, 0, 1, 0.1) * 0.5

def timpani(f, d=1.6):
    t = t_(d)
    fr = f * (1 + 0.15 * np.exp(-t * 30))
    x = np.sin(2 * np.pi * np.cumsum(fr) / SR) * np.exp(-t * 3.5)
    x += lowpass(noise(d), 300) * np.exp(-t * 25) * 0.4
    return x * 0.9

def drum(d=0.5, f=70, noise_amt=0.6):
    t = t_(d)
    x = np.sin(2 * np.pi * np.cumsum(f * (1 + 2 * np.exp(-t * 40))) / SR) * np.exp(-t * 9)
    x += bandpass(noise(d), 200, 3000) * np.exp(-t * 30) * noise_amt
    return x

def drone(f, d):
    t = t_(d)
    x = sum(np.sin(2 * np.pi * f * h * t + rng.random() * 6) / h for h in (1, 2, 3, 5))
    return lowpass(x, 900) * (0.7 + 0.3 * np.sin(2 * np.pi * 0.07 * t)) * 0.35

# ----------------------------------------------------------------------------------- composition
SCALES = {"major": [0, 2, 4, 5, 7, 9, 11], "minor": [0, 2, 3, 5, 7, 8, 10], "dorian": [0, 2, 3, 5, 7, 9, 10], "lydian": [0, 2, 4, 6, 7, 9, 11],
          "phrygian": [0, 1, 3, 5, 7, 8, 10], "mixolydian": [0, 2, 4, 5, 7, 9, 10], "harmonic": [0, 2, 3, 5, 7, 8, 11]}

def chord_tones(root, scale, degree):
    sc = SCALES[scale]
    return [root + sc[(degree + k) % 7] + 12 * ((degree + k) // 7) for k in (0, 2, 4)]

def melody(root, scale, bars, beats_per_bar, seed, rng_local, rest=0.2, low=0, high=10):
    """Stepwise melodic line in the scale with phrase repetition (A A' B A)."""
    sc = SCALES[scale]
    def phrase(seed_):
        r = np.random.default_rng(seed_)
        notes = []; pos = r.integers(low + 2, high - 2)
        for b in range(2 * beats_per_bar):
            if r.random() < rest and b % beats_per_bar != 0: notes.append(None); continue
            pos = int(np.clip(pos + r.choice([-2, -1, -1, 0, 1, 1, 2]), low, high))
            notes.append(root + 12 + sc[pos % 7] + 12 * (pos // 7))
        return notes
    A, B = phrase(seed), phrase(seed + 7)
    A2 = list(A); A2[-2:] = [A[0], A[0]]
    seq = []
    for p in [A, A2, B, A]:
        seq += p
    return seq[: bars * beats_per_bar]

def compose(name, root, scale, bpm, prog, bars=24, layers=("strings", "harp", "flute"), seed=1, dur=None, reverb_size=2.6, wet=0.32, energy=0.5):
    beat = 60.0 / bpm
    bpb = 4
    total = bars * bpb * beat if dur is None else dur
    n = int(total * SR) + SR * 4
    mixL = np.zeros(n); mixR = np.zeros(n)
    def add(x, start, p=0.0, gain=1.0):
        s = int(start * SR)
        e = min(n, s + len(x))
        st = pan(x[: e - s], p) * gain
        mixL[s:e] += st[0]; mixR[s:e] += st[1]
    r = np.random.default_rng(seed)
    mel = melody(root, scale, bars, bpb, seed, r)
    for bar in range(bars):
        deg = prog[bar % len(prog)]
        ch = chord_tones(root - 12, scale, deg)
        t0 = bar * bpb * beat
        if "strings" in layers:
            add(strings([midi(m) for m in ch] + [midi(ch[0] - 12)], bpb * beat + 0.6, bright=1800 + 1500 * energy), t0, 0.0, 0.55)
        if "drone" in layers:
            add(drone(midi(root - 24), bpb * beat + 0.5), t0, 0.0, 0.5)
        if "choir" in layers and bar % 2 == 0:
            add(choir([midi(m + 12) for m in ch], 2 * bpb * beat + 0.8, "a" if bar % 4 == 0 else "o"), t0, 0.0, 0.45)
        if "harp" in layers:
            arp = [ch[0], ch[1], ch[2], ch[1] + 12, ch[2] + 12, ch[1] + 12, ch[2], ch[1]]
            for k in range(8):
                add(pluck_fast(midi(arp[k] + 12), 1.4, 0.4), t0 + k * beat / 2, (k % 2) * 0.6 - 0.3, 0.28)
        if "lute" in layers:
            for k in range(bpb):
                add(pluck_fast(midi(ch[k % 3] + 12), 0.8, 0.7), t0 + k * beat, -0.3, 0.3)
                add(pluck_fast(midi(ch[(k + 1) % 3] + 24), 0.5, 0.8), t0 + (k + 0.5) * beat, 0.3, 0.18)
        if "bells" in layers:
            for k in range(0, bpb, 2):
                add(bell(midi(ch[(bar + k) % 3] + 24), 3.0), t0 + k * beat + 0.02, r.uniform(-0.6, 0.6), 0.2)
        if "ostinato" in layers:
            for k in range(bpb * 4):
                f = midi(ch[0] - (0 if k % 4 else 12))
                add(lowpass(saw(f, beat / 4 * 0.9) * env_adsr(int(beat / 4 * 0.9 * SR), 0.005, 0.05, 0.5, 0.03), 1600), t0 + k * beat / 4, 0.0, 0.22)
        if "drums" in layers:
            for k in range(bpb):
                add(drum(0.6, 60), t0 + k * beat, 0.0, 0.55 if k % 2 == 0 else 0.35)
                if energy > 0.6: add(drum(0.3, 120, 0.9), t0 + (k + 0.5) * beat, 0.2, 0.22)
        if "timpani" in layers and bar % 2 == 1:
            add(timpani(midi(root - 24)), t0 + 3 * beat, 0.0, 0.6)
            add(timpani(midi(root - 17)), t0 + 3.5 * beat, 0.0, 0.45)
        if "horn" in layers and bar % 4 in (2, 3):
            add(horn(midi(ch[0]), bpb * beat * 0.95), t0, -0.2, 0.32)
            add(horn(midi(ch[2]), bpb * beat * 0.95), t0, 0.2, 0.25)
        # melody (flute or horn lead) enters after 4 bars, takes a breath every 8
        lead = "flute" if "flute" in layers else ("hornlead" if "hornlead" in layers else None)
        if lead and bar >= 4 and (bar // 8) % 3 != 2:
            for k in range(bpb):
                m = mel[(bar * bpb + k) % len(mel)]
                if m is None: continue
                if lead == "flute": add(flute(midi(m + 12), beat * 0.98), t0 + k * beat, 0.15, 0.32)
                else: add(horn(midi(m), beat * 0.98), t0 + k * beat, 0.1, 0.38)
    x = np.stack([mixL, mixR])
    x = reverb(x, reverb_size, wet)
    L = int(total * SR)
    x = x[:, : L + int(1.5 * SR)]
    x = loopify(x, 1.5)
    return norm(x, 0.82)

MUSIC = {
    "title": dict(root=62, scale="major", bpm=72, prog=[0, 4, 5, 3, 0, 4, 3, 4], layers=("strings", "harp", "hornlead", "choir", "timpani"), seed=11, bars=24, energy=0.6),
    "greenhaven": dict(root=62, scale="major", bpm=84, prog=[0, 4, 5, 3], layers=("strings", "harp", "flute"), seed=21, bars=28),
    "frostheim": dict(root=62, scale="dorian", bpm=66, prog=[0, 6, 3, 4], layers=("drone", "strings", "choir", "bells", "hornlead"), seed=31, bars=20, reverb_size=3.4, wet=0.4),
    "ashen": dict(root=60, scale="phrygian", bpm=88, prog=[0, 1, 0, 6], layers=("drone", "strings", "drums", "horn"), seed=41, bars=24, energy=0.7),
    "elderwood": dict(root=64, scale="lydian", bpm=70, prog=[0, 1, 4, 3], layers=("strings", "bells", "flute", "choir"), seed=51, bars=22, reverb_size=3.2, wet=0.42),
    "shadows": dict(root=57, scale="harmonic", bpm=60, prog=[0, 5, 3, 4], layers=("drone", "strings", "choir", "bells"), seed=61, bars=18, reverb_size=3.6, wet=0.45, energy=0.3),
    "highcrowns": dict(root=65, scale="mixolydian", bpm=76, prog=[0, 6, 3, 0], layers=("strings", "horn", "hornlead", "timpani", "harp"), seed=71, bars=24, energy=0.6),
    "village": dict(root=67, scale="major", bpm=104, prog=[0, 3, 4, 0], layers=("lute", "flute", "strings"), seed=81, bars=28, reverb_size=1.6, wet=0.22),
    "night": dict(root=63, scale="major", bpm=60, prog=[0, 5, 3, 4], layers=("strings", "harp", "bells"), seed=91, bars=18, energy=0.2, reverb_size=3.0, wet=0.4),
    "combat": dict(root=62, scale="minor", bpm=128, prog=[0, 5, 6, 4], layers=("ostinato", "drums", "strings", "horn", "hornlead"), seed=101, bars=32, energy=0.9, reverb_size=1.8, wet=0.25),
    "boss": dict(root=60, scale="harmonic", bpm=138, prog=[0, 5, 1, 4], layers=("ostinato", "drums", "choir", "horn", "hornlead", "timpani"), seed=111, bars=36, energy=1.0, reverb_size=2.2, wet=0.3),
    "dungeon": dict(root=57, scale="minor", bpm=58, prog=[0, 5, 0, 6], layers=("drone", "harp", "choir"), seed=121, bars=16, energy=0.25, reverb_size=4.0, wet=0.5),
    "credits": dict(root=62, scale="major", bpm=68, prog=[0, 4, 5, 3, 0, 4, 3, 4], layers=("strings", "harp", "flute", "choir"), seed=11, bars=32, energy=0.4),
}

def studio_jingle():
    d = 6.0
    x = np.zeros(int(d * SR))
    for i, m in enumerate([62, 69, 74, 78, 81, 86]):
        b = bell(midi(m), d - i * 0.18, 1.5)
        s = int((0.4 + i * 0.18) * SR)
        x[s:s + len(b)] += b[: len(x) - s] * 0.5
    pad = strings([midi(62), midi(69), midi(74), midi(78)], d - 0.5, 2200, 1.2, 2.0)
    x[int(0.3 * SR):int(0.3 * SR) + len(pad)] += pad[: len(x) - int(0.3 * SR)] * 0.9
    shimmer = highpass(noise(d), 6000) * np.exp(-np.abs(t_(d) - 1.2) * 2) * 0.05
    x += shimmer
    return norm(reverb(x, 3.5, 0.45), 0.8)

# ----------------------------------------------------------------------------------- ambiences
def amb(name, d=24.0):
    n = int(d * SR); t = t_(d)
    if name == "amb_forest_day":
        x = lowpass(noise(d), 600) * 0.05 * (0.7 + 0.3 * np.sin(2 * np.pi * 0.05 * t))
        for _ in range(40):
            s = rng.uniform(0, d - 1); f = rng.uniform(2500, 5200)
            for k in range(rng.integers(2, 6)):
                c = t_(0.07); chirp = np.sin(2 * np.pi * (f + 900 * np.sin(np.pi * c / 0.07) + k * 120) * c) * np.sin(np.pi * c / 0.07) * 0.12
                st = int((s + k * 0.11) * SR); x[st:st + len(chirp)] += chirp[: n - st]
    elif name == "amb_night":
        x = lowpass(noise(d), 400) * 0.03
        cr = np.sin(2 * np.pi * 4300 * t) * (np.sin(2 * np.pi * 28 * t) > 0.6) * (0.5 + 0.5 * np.sin(2 * np.pi * 0.6 * t)) * 0.04
        x += bandpass(cr, 3500, 5200)
        for _ in range(4):
            s = rng.uniform(1, d - 2)
            for k in range(2):
                c = t_(0.35); hoot = np.sin(2 * np.pi * (390 - 40 * c / 0.35) * c) * np.sin(np.pi * c / 0.35) * 0.08
                st = int((s + k * 0.5) * SR); x[st:st + len(hoot)] += hoot[: n - st]
    elif name == "amb_wind":
        g = 0.6 + 0.4 * np.sin(2 * np.pi * 0.08 * t) * np.sin(2 * np.pi * 0.031 * t + 1)
        x = bandpass(noise(d), 200, 1400) * g * 0.25 + highpass(noise(d), 3000) * g * 0.02
    elif name == "amb_rain":
        x = bandpass(noise(d), 800, 9000) * 0.12
        drops = (rng.random(n) < 0.0009) * rng.uniform(0.2, 1, n)
        x += signal.lfilter([1], [1, -0.95], drops) * 0.08
    elif name == "amb_lava":
        x = lowpass(noise(d), 160) * 0.3 * (0.8 + 0.2 * np.sin(2 * np.pi * 0.1 * t))
        for _ in range(30):
            s = rng.uniform(0, d - 0.3); c = t_(0.15)
            pop = np.sin(2 * np.pi * rng.uniform(120, 300) * c * (1 + c * 3)) * np.exp(-c * 25) * 0.2
            st = int(s * SR); x[st:st + len(pop)] += pop[: n - st]
    elif name == "amb_swamp":
        x = lowpass(noise(d), 500) * 0.04
        for _ in range(26):
            s = rng.uniform(0, d - 0.5); f = rng.uniform(140, 260)
            c = t_(0.25)
            croak = np.sign(np.sin(2 * np.pi * f * c)) * (np.sin(2 * np.pi * 22 * c) > 0) * np.sin(np.pi * c / 0.25) * 0.05
            st = int(s * SR); x[st:st + len(croak)] += lowpass(croak, 900)[: n - st]
        x += bandpass(noise(d), 5000, 7000) * 0.01
    elif name == "amb_cave":
        x = drone(55, d) * 0.4 + lowpass(noise(d), 200) * 0.05
        for _ in range(18):
            s = rng.uniform(0, d - 0.5); c = t_(0.3)
            drip = np.sin(2 * np.pi * rng.uniform(900, 1700) * c * (1 - c)) * np.exp(-c * 30) * 0.15
            st = int(s * SR); x[st:st + len(drip)] += drip[: n - st]
        x = reverb(x, 3.5, 0.5)[0][:n]
    elif name == "amb_river":
        x = bandpass(noise(d), 300, 3500) * 0.18 * (0.85 + 0.15 * np.sin(2 * np.pi * 0.3 * t))
        x += bandpass(noise(d), 2000, 6000) * 0.04
    elif name == "amb_village":
        x = lowpass(noise(d), 700) * 0.03
        for _ in range(60):  # murmured voices (formant noise)
            s = rng.uniform(0, d - 1); L = rng.uniform(0.2, 0.6)
            c = noise(L)
            v = bandpass(c, rng.uniform(400, 700), rng.uniform(900, 1300)) * np.sin(np.pi * t_(L) / L) * 0.05
            st = int(s * SR); x[st:st + len(v)] += v[: n - st]
        for _ in range(10):  # distant hammer
            s = rng.uniform(0, d - 0.3); c = t_(0.25)
            h = sum(np.sin(2 * np.pi * f * c) for f in (1800, 2650, 3900)) * np.exp(-c * 30) * 0.03
            st = int(s * SR); x[st:st + len(h)] += h[: n - st]
    else:
        x = np.zeros(n)
    st = pan(x, 0.0) if x.ndim == 1 else x
    st[1] = np.roll(st[1], int(0.011 * SR))  # stereo width
    return norm(loopify(st, 2.0), 0.6)

# ----------------------------------------------------------------------------------- sound effects
def sweep_noise(d, f0, f1, q=3.0):
    n = int(d * SR); x = noise(d); out = np.zeros(n); blk = 512
    for s in range(0, n, blk):
        f = f0 + (f1 - f0) * (s / n)
        b, a = signal.butter(2, [max(20, f / q) / (SR / 2), min(0.99, f * q / (SR / 2))], "band")
        out[s:s + blk] = signal.lfilter(b, a, x[s:s + blk])
    return out

def sfx(name):
    if name in ("swing", "swing_heavy"):
        d = 0.32 if name == "swing" else 0.5
        x = sweep_noise(d, 400, 2600 if name == "swing" else 1400) * np.sin(np.pi * t_(d) / d) ** 2
        return x
    if name == "hit_flesh":
        d = 0.25; x = lowpass(noise(d), 1200) * np.exp(-t_(d) * 25) + np.sin(2 * np.pi * 90 * t_(d)) * np.exp(-t_(d) * 20) * 0.8
        return x
    if name == "hit_metal" or name == "block" or name == "parry":
        d = 0.8 if name == "parry" else 0.5; t = t_(d)
        x = sum(np.sin(2 * np.pi * f * t) * np.exp(-t * k) for f, k in ((1180, 6), (2750, 9), (3920, 12), (5370, 16))) * 0.4
        x += highpass(noise(d), 3000) * np.exp(-t * 40) * 0.6
        if name == "parry": x += bell(1567, d, 3) * 0.6
        return x
    if name == "hit_wood": d = 0.3; t = t_(d); return (np.sin(2 * np.pi * 320 * t) + 0.5 * np.sin(2 * np.pi * 710 * t)) * np.exp(-t * 22) + bandpass(noise(d), 300, 2000) * np.exp(-t * 35) * 0.5
    if name.startswith("step_"):
        d = 0.16; t = t_(d)
        lo, hi, k = {"step_grass": (300, 3000, 30), "step_stone": (800, 5000, 50), "step_snow": (150, 1800, 18), "step_wood": (150, 1200, 40), "step_water": (400, 5000, 15)}[name]
        x = bandpass(noise(d), lo, hi) * np.exp(-t * k)
        if name == "step_wood": x += np.sin(2 * np.pi * 140 * t) * np.exp(-t * 30) * 0.5
        return x
    if name == "grass_rustle":
        # walking through tall grass: a soft swish made of many tiny leaf brushes
        d = 0.55; t = t_(d)
        env = np.sin(np.pi * t / d) ** 1.5
        x = bandpass(noise(d), 1800, 7500) * env * 0.5
        for i in range(14):
            c = int(rng.uniform(0.03, d - 0.05) * SR); n = int(0.012 * SR)
            x[c:c + n] += bandpass(noise(0.012), 2500, 9000)[:len(x[c:c + n])] * np.exp(-np.arange(len(x[c:c + n])) / SR * 250) * rng.uniform(0.3, 0.8)
        return x
    if name == "jump": d = 0.25; return sweep_noise(d, 300, 900) * np.exp(-t_(d) * 8) * 0.6
    if name == "land": d = 0.3; t = t_(d); return lowpass(noise(d), 500) * np.exp(-t * 18) + np.sin(2 * np.pi * 70 * t) * np.exp(-t * 15) * 0.6
    if name == "roll": d = 0.45; return bandpass(noise(d), 200, 2500) * np.sin(np.pi * t_(d) / d) * 0.6
    if name == "bow": d = 0.4; t = t_(d); return np.sin(2 * np.pi * 180 * t * (1 + 0.3 * np.exp(-t * 20))) * np.exp(-t * 12) + sweep_noise(d, 1500, 600) * np.exp(-t * 6) * 0.4
    if name == "arrow_hit": d = 0.2; t = t_(d); return np.sin(2 * np.pi * 220 * t) * np.exp(-t * 30) + bandpass(noise(d), 500, 2500) * np.exp(-t * 40)
    if name == "fireball": d = 0.9; t = t_(d); return lowpass(noise(d), 1200) * np.sin(np.pi * t / d) * 0.8 + sweep_noise(d, 200, 1200) * 0.4
    if name == "explosion": d = 1.6; t = t_(d); return lowpass(noise(d), 700) * np.exp(-t * 3) * 1.2 + np.sin(2 * np.pi * 50 * t) * np.exp(-t * 4)
    if name == "ice": d = 0.9; t = t_(d); return sum(np.sin(2 * np.pi * f * t) * np.exp(-t * (4 + i)) for i, f in enumerate((2100, 3170, 4430, 5900, 7300))) * 0.25 + highpass(noise(d), 5000) * np.exp(-t * 12) * 0.3
    if name == "lightning": d = 0.8; t = t_(d); return highpass(noise(d) * (rng.random(int(d * SR)) > 0.7), 1500) * np.exp(-t * 6) + lowpass(noise(d), 200) * np.exp(-t * 3) * 0.6
    if name == "heal": d = 1.2; x = np.zeros(int(d * SR)); [x.__setitem__(slice(int(i * 0.09 * SR), int(i * 0.09 * SR) + int(0.8 * SR)), x[int(i * 0.09 * SR):int(i * 0.09 * SR) + int(0.8 * SR)] + bell(midi(m), 0.8, 1)[: len(x[int(i * 0.09 * SR):int(i * 0.09 * SR) + int(0.8 * SR)])]) for i, m in enumerate((72, 76, 79, 84))]; return x
    if name == "shadow": d = 0.9; t = t_(d); return sweep_noise(d, 1500, 120) * np.sin(np.pi * t / d) + np.sin(2 * np.pi * 65 * t) * np.sin(np.pi * t / d) * 0.5
    if name == "arcane": d = 0.7; t = t_(d); return np.sin(2 * np.pi * 880 * t + 4 * np.sin(2 * np.pi * 440 * 1.5 * t) * np.exp(-t * 5)) * np.exp(-t * 4) * 0.6
    if name == "nature": d = 0.8; t = t_(d); return bandpass(noise(d), 1000, 4000) * np.sin(np.pi * t / d) * 0.4 + bell(midi(79), d, 0.8) * 0.4
    if name == "holy": return choir([midi(72), midi(76), midi(79)], 1.4, "a") * 2.5
    if name == "magic_burst": d = 1.0; t = t_(d); return sweep_noise(d, 300, 4000) * np.exp(-t * 3) * 0.6 + bell(midi(86), d, 2) * 0.5
    if name == "pickup": d = 0.18; t = t_(d); return np.sin(2 * np.pi * (900 + 900 * t / d) * t) * np.exp(-t * 14) * 0.5
    if name == "coin": d = 0.45; x = bell(2349, d, 0.6) * 0.6; x[int(0.07 * SR):] += bell(3136, d - 0.07, 0.6)[: len(x) - int(0.07 * SR)] * 0.5; return x
    if name == "chest": d = 1.0; t = t_(d); creak = bandpass(saw(140, d) * (1 + 0.3 * np.sin(2 * np.pi * 9 * t)), 300, 1600) * np.exp(-t * 4) * 0.4; return creak + magic_like(d)
    if name == "levelup": d = 1.6; x = np.zeros(int(d * SR)); notes = (60, 64, 67, 72, 76, 79, 84)
    if name == "levelup":
        for i, m in enumerate(notes):
            b = horn(midi(m), 0.5) if i < 4 else bell(midi(m + 12), 1.0, 1.2)
            s = int(i * 0.08 * SR); x[s:s + len(b)] += b[: len(x) - s] * 0.5
        return x
    if name == "quest": d = 1.4; x = sum(bell(midi(m), d, 1.0) for m in (67, 71, 74, 79)) * 0.5; return x
    if name == "discover": d = 1.6; x = np.zeros(int(d * SR)); [x.__setitem__(slice(int(i * 0.12 * SR), int(i * 0.12 * SR) + int(1.2 * SR)), x[int(i * 0.12 * SR):int(i * 0.12 * SR) + int(1.2 * SR)] + bell(midi(m), 1.2, 1.3)[: len(x[int(i * 0.12 * SR):int(i * 0.12 * SR) + int(1.2 * SR)])]) for i, m in enumerate((74, 79, 83, 86))]; return x
    if name == "ui_click": d = 0.06; t = t_(d); return np.sin(2 * np.pi * 1800 * t) * np.exp(-t * 80) * 0.5
    if name == "ui_open": d = 0.35; return sweep_noise(d, 400, 2500) * np.exp(-t_(d) * 8) * 0.4 + pluck_fast(midi(76), d) * 0.5
    if name == "ui_close": d = 0.3; return sweep_noise(d, 2500, 400) * np.exp(-t_(d) * 8) * 0.4
    if name == "potion": d = 0.7; t = t_(d); return sum(np.sin(2 * np.pi * rng.uniform(200, 500) * t_(0.06)) * np.exp(-t_(0.06) * 40) for _ in range(1)) if False else bubbles(d)
    if name == "craft": d = 0.6; t = t_(d); return bandpass(noise(d), 1000, 5000) * (np.sin(2 * np.pi * 12 * t) > 0) * np.exp(-t * 4) * 0.4 + bell(midi(84), d, 1) * 0.3
    if name == "forge": d = 0.9; t = t_(d); return sum(np.sin(2 * np.pi * f * t) * np.exp(-t * k) for f, k in ((820, 5), (1960, 7), (3210, 10), (4880, 14))) * 0.45 + highpass(noise(d), 2000) * np.exp(-t * 50)
    if name == "harvest_wood": d = 0.35; t = t_(d); return np.sin(2 * np.pi * 180 * t) * np.exp(-t * 25) + bandpass(noise(d), 400, 3000) * np.exp(-t * 30) * 0.7
    if name == "harvest_stone": d = 0.4; t = t_(d); return sum(np.sin(2 * np.pi * f * t) * np.exp(-t * 20) for f in (1300, 2100, 3700)) * 0.3 + bandpass(noise(d), 1000, 6000) * np.exp(-t * 30) * 0.6
    if name == "harvest_herb": d = 0.35; return bandpass(noise(d), 1500, 6000) * np.sin(np.pi * t_(d) / d) * 0.4
    if name == "splash": d = 0.8; t = t_(d); return bandpass(noise(d), 300, 6000) * np.exp(-t * 6) + bubbles(d) * 0.3
    if name == "wolf_howl": d = 2.0; t = t_(d); f = 380 + 220 * np.sin(np.pi * t / d) ** 0.6; ph = np.cumsum(f * (1 + 0.01 * np.sin(2 * np.pi * 6 * t))) / SR; x = np.sin(2 * np.pi * ph) + 0.3 * np.sin(4 * np.pi * ph); return bandpass(x, 300, 2400) * env_adsr(len(t), 0.25, 0.2, 0.9, 0.6) * 0.7
    if name in ("growl", "troll", "roar", "dragon_roar"):
        d = {"growl": 0.9, "troll": 1.0, "roar": 1.8, "dragon_roar": 2.6}[name]; t = t_(d)
        f0 = {"growl": 90, "troll": 70, "roar": 110, "dragon_roar": 60}[name]
        src = saw(f0, d) * (1 + 0.5 * np.sin(2 * np.pi * 23 * t)) + noise(d) * 0.6
        x = bandpass(src, f0 * 2, 1400 if name != "dragon_roar" else 2200) * env_adsr(len(t), 0.1, 0.2, 0.9, 0.4)
        if name == "dragon_roar": x += lowpass(noise(d), 300) * env_adsr(len(t), 0.3, 0.2, 0.8, 0.8) * 0.8
        return x
    if name == "spider": d = 0.6; x = np.zeros(int(d * SR)); [x.__setitem__(slice(s, s + 300), x[s:s + 300] + highpass(noise(300 / SR), 2500) * np.exp(-np.arange(300) / 40)) for s in rng.integers(0, int(d * SR) - 300, 14)]; return x
    if name == "skeleton": d = 0.6; x = np.zeros(int(d * SR)); [x.__setitem__(slice(s, s + 600), x[s:s + 600] + bandpass(noise(600 / SR), 800, 4000) * np.exp(-np.arange(600) / 90)) for s in rng.integers(0, int(d * SR) - 600, 10)]; return x
    if name == "goblin": d = 0.5; t = t_(d); return bandpass(saw(330 + 120 * np.sin(2 * np.pi * 9 * t), d), 600, 3000) * np.sin(np.pi * t / d) * 0.5
    if name == "golem": d = 1.1; t = t_(d); return lowpass(noise(d), 300) * np.sin(np.pi * t / d) + bandpass(noise(d) * (rng.random(len(t)) > 0.9), 300, 1500) * 0.4
    if name == "ghost": d = 1.6; t = t_(d); return np.sin(2 * np.pi * (500 + 120 * np.sin(2 * np.pi * 0.8 * t)) * t) * np.sin(np.pi * t / d) * 0.4 + bandpass(noise(d), 1500, 3000) * 0.08
    if name == "death": d = 0.9; t = t_(d); return lowpass(noise(d), 400) * np.exp(-t * 6) + np.sin(2 * np.pi * 55 * t) * np.exp(-t * 5)
    if name == "player_hurt": d = 0.3; t = t_(d); return bandpass(saw(160, d), 300, 1500) * np.exp(-t * 10) * 0.6 + lowpass(noise(d), 800) * np.exp(-t * 20) * 0.5
    if name == "thunder": d = 3.5; t = t_(d); return lowpass(noise(d), 180) * (np.exp(-t * 1.2) + 0.5 * np.exp(-((t - 0.3) * 8) ** 2)) * 1.5
    if name == "birds": d = 1.5; x = np.zeros(int(d * SR)); [x.__setitem__(slice(s, s + 4000), x[s:s + 4000] + np.sin(2 * np.pi * (3000 + 1500 * np.sin(np.pi * np.arange(4000) / 4000)) * np.arange(4000) / SR) * np.sin(np.pi * np.arange(4000) / 4000) * 0.3) for s in rng.integers(0, int(d * SR) - 4000, 8)]; return x
    if name == "door": d = 1.2; t = t_(d); return bandpass(saw(90, d) * (1 + 0.4 * np.sin(2 * np.pi * 7 * t)), 200, 1500) * np.sin(np.pi * t / d) * 0.5 + lowpass(noise(d), 300) * np.exp(-((t - 1.0) * 10) ** 2)
    if name == "lever": d = 0.5; t = t_(d); return sum(np.sin(2 * np.pi * f * t) * np.exp(-t * 18) for f in (420, 1150, 2300)) * 0.4
    if name == "trap": d = 0.5; t = t_(d); return highpass(noise(d), 3000) * np.exp(-t * 12) + sum(np.sin(2 * np.pi * f * t) * np.exp(-t * 10) for f in (2600, 4100)) * 0.3
    if name == "fish_bite": d = 0.3; t = t_(d); return np.sin(2 * np.pi * (600 - 300 * t / d) * t) * np.exp(-t * 15) * 0.6
    if name == "horse": d = 0.25; t = t_(d); return (bandpass(noise(d), 200, 1200) * np.exp(-t * 40) + np.sin(2 * np.pi * 120 * t) * np.exp(-t * 35)) * 0.8
    if name == "whistle": d = 0.8; t = t_(d); f = np.where(t < 0.35, 1800, 2300); return np.sin(2 * np.pi * np.cumsum(f) / SR) * env_adsr(len(t), 0.03, 0.05, 0.9, 0.1) * 0.4
    if name == "boss_intro": d = 3.0; t = t_(d); return lowpass(noise(d), 120) * np.exp(-t * 1.2) * 1.2 + choir([midi(48), midi(55), midi(63)], d, "o") * 2.0 + timpani(midi(36), d)
    if name == "fire_loop": d = 2.0; return lowpass(noise(d), 1500) * 0.3 + bubbles(d) * 0.2
    return np.zeros(int(0.1 * SR))

def bubbles(d):
    n = int(d * SR); x = np.zeros(n)
    for s in rng.integers(0, max(1, n - 3000), 18):
        c = np.arange(3000) / SR
        f = rng.uniform(300, 900)
        x[s:s + 3000] += np.sin(2 * np.pi * f * c * (1 + 8 * c)) * np.exp(-c * 50) * 0.4
    return x

def magic_like(d):
    return bell(midi(84), d, 1.5) * 0.4 + highpass(noise(d), 6000) * np.exp(-t_(d) * 5) * 0.05

SFX = ["swing", "swing_heavy", "hit_flesh", "hit_metal", "hit_wood", "block", "parry", "step_grass", "step_stone", "step_snow", "step_wood", "step_water", "grass_rustle",
       "jump", "land", "roll", "bow", "arrow_hit", "fireball", "explosion", "ice", "lightning", "heal", "shadow", "arcane", "nature", "holy",
       "pickup", "coin", "chest", "levelup", "quest", "ui_click", "ui_open", "ui_close", "potion", "craft", "forge", "harvest_wood", "harvest_stone", "harvest_herb",
       "splash", "wolf_howl", "growl", "roar", "dragon_roar", "spider", "skeleton", "goblin", "troll", "golem", "ghost", "death", "player_hurt", "thunder", "birds",
       "door", "lever", "trap", "fish_bite", "horse", "whistle", "boss_intro", "discover", "fire_loop", "magic_burst"]
AMB = ["amb_forest_day", "amb_night", "amb_wind", "amb_rain", "amb_lava", "amb_swamp", "amb_cave", "amb_river", "amb_village"]

def main():
    only = sys.argv[1:] if len(sys.argv) > 1 else ["sfx", "amb", "music"]
    # "sfx:name1,name2" regenerates only those effects (the others keep their committed files)
    pick = [a[4:].split(",") for a in only if a.startswith("sfx:")]
    if pick:
        only = only + ["sfx"]
    if "sfx" in only:
        for s in (pick[0] if pick else SFX):
            x = sfx(s)
            x = np.concatenate([x, np.zeros(int(0.03 * SR))])
            x = x * np.r_[np.linspace(0, 1, 48), np.ones(len(x) - 96), np.linspace(1, 0, 48)]
            dry = norm(x, 0.85)
            write_ogg(os.path.join(OUT, "sfx", s + ".ogg"), np.stack([dry, dry]), q=4)
        print("sfx done:", len(SFX))
    if "amb" in only:
        for a in AMB:
            write_ogg(os.path.join(OUT, "amb", a + ".ogg"), amb(a), q=2)
        print("ambiences done:", len(AMB))
    if "music" in only:
        write_ogg(os.path.join(OUT, "music", "studio.ogg"), studio_jingle(), q=3)
        for name, p in MUSIC.items():
            write_ogg(os.path.join(OUT, "music", name + ".ogg"), compose(name, **p), q=2)
            print("music:", name)

if __name__ == "__main__":
    main()
