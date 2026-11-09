#!/usr/bin/env python3
"""
NOCTÉA — synthèse procédurale des ambiances sonores (Superessence Studio).

Tous les sons sont générés de façon *circulaire* : bruits façonnés dans le domaine
fréquentiel (FFT périodique) et évènements (gouttes, crépitements, notes) repliés
en fin de boucle. Les boucles sont donc parfaitement continues (aucun « clic »).

Usage : python3 tools/build_sounds.py <dossier_sortie>
Dépendances : numpy, ffmpeg (libvorbis).
"""
import os
import subprocess
import sys
import tempfile
import wave

import numpy as np

SR = 32000
RNG = np.random.default_rng(20261109)


# --------------------------------------------------------------------------- utilitaires
def n_samples(sec):
    return int(round(sec * SR))


def spectral_noise(n, shape_fn, rng=RNG):
    """Bruit périodique de longueur n dont le spectre d'amplitude suit shape_fn(f)."""
    freqs = np.fft.rfftfreq(n, 1.0 / SR)
    mag = shape_fn(np.maximum(freqs, 1e-3))
    mag[0] = 0.0
    phase = rng.uniform(0, 2 * np.pi, len(freqs))
    spec = mag * np.exp(1j * phase)
    x = np.fft.irfft(spec, n)
    return x / (np.std(x) + 1e-12)


def fft_filter(x, shape_fn):
    """Filtrage circulaire (préserve la boucle)."""
    n = len(x)
    freqs = np.fft.rfftfreq(n, 1.0 / SR)
    return np.fft.irfft(np.fft.rfft(x) * shape_fn(np.maximum(freqs, 1e-3)), n)


def bandpass_shape(lo, hi, order=2):
    def f(fr):
        a = 1.0 / np.sqrt(1 + (lo / fr) ** (2 * order))
        b = 1.0 / np.sqrt(1 + (fr / hi) ** (2 * order))
        return a * b
    return f


def lowpass_shape(fc, order=2):
    return lambda fr: 1.0 / np.sqrt(1 + (fr / fc) ** (2 * order))


def highpass_shape(fc, order=2):
    return lambda fr: 1.0 / np.sqrt(1 + (fc / fr) ** (2 * order))


def pink(fr):
    return 1.0 / np.sqrt(fr)


def brown(fr):
    return 1.0 / fr


def slow_lfo(n, max_hz, rng=RNG, lo=0.0, hi=1.0):
    """Modulation lente aléatoire, périodique, normalisée dans [lo, hi]."""
    x = spectral_noise(n, lambda fr: np.where(fr < max_hz, 1.0, 0.0) * (1.0 / np.sqrt(fr)), rng)
    x = (x - x.min()) / (x.max() - x.min() + 1e-12)
    return lo + (hi - lo) * x


def add_wrapped(buf, start, sig):
    """Ajoute sig dans buf à partir de start, en repliant ce qui dépasse (boucle)."""
    n = len(buf)
    start %= n
    end = start + len(sig)
    if end <= n:
        buf[start:end] += sig
    else:
        first = n - start
        buf[start:] += sig[:first]
        rest = sig[first:]
        while len(rest) > 0:
            k = min(len(rest), n)
            buf[:k] += rest[:k]
            rest = rest[k:]


def pan(sig, p):
    """p dans [-1, 1] → stéréo à puissance constante."""
    ang = (p + 1) * np.pi / 4
    return np.cos(ang) * sig, np.sin(ang) * sig


def make_ir(sec=3.5, damp=6000.0, rng=RNG):
    n = n_samples(sec)
    t = np.arange(n) / SR
    env = np.exp(-6.9 * t / sec)
    l = rng.standard_normal(n) * env
    r = rng.standard_normal(n) * env
    # amortissement des aigus progressif : on filtre globalement
    l = fft_filter(np.concatenate([l, np.zeros(n)]), lowpass_shape(damp, 1))[:n]
    r = fft_filter(np.concatenate([r, np.zeros(n)]), lowpass_shape(damp, 1))[:n]
    l[: n_samples(0.012)] *= np.linspace(0, 1, n_samples(0.012))
    r[: n_samples(0.012)] *= np.linspace(0, 1, n_samples(0.012))
    return l / np.sqrt(np.sum(l ** 2)), r / np.sqrt(np.sum(r ** 2))


def circular_reverb(L, R, ir, wet=0.35):
    """Convolution circulaire (la queue de réverbération reboucle proprement)."""
    n = len(L)
    il, ir_ = ir
    def conv(x, h):
        hh = np.zeros(n)
        k = min(len(h), n)
        hh[:k] = h[:k]
        return np.fft.irfft(np.fft.rfft(x) * np.fft.rfft(hh), n)
    wl = conv(L, il)
    wr = conv(R, ir_)
    g = np.std(L + R) / (np.std(wl + wr) + 1e-12)
    return (1 - wet) * L + wet * g * wl, (1 - wet) * R + wet * g * wr


def normalize(L, R, rms_db=-22.0, peak=0.89):
    rms = np.sqrt(np.mean((L ** 2 + R ** 2) / 2)) + 1e-12
    g = 10 ** (rms_db / 20) / rms
    L, R = L * g, R * g
    pk = max(np.max(np.abs(L)), np.max(np.abs(R)))
    if pk > peak:
        # limiteur doux
        L = np.tanh(L / peak) * peak
        R = np.tanh(R / peak) * peak
    return L, R


def write_ogg(path, L, R, quality=3):
    pcm = np.stack([L, R], axis=1)
    pcm = np.clip(pcm, -1, 1)
    pcm16 = (pcm * 32767).astype("<i2")
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as tmp:
        tmpname = tmp.name
    with wave.open(tmpname, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm16.tobytes())
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", tmpname, "-c:a", "libvorbis",
                    "-q:a", str(quality), path], check=True)
    os.unlink(tmpname)


def write_ogg_mono(path, x, quality=3):
    write_ogg(path, x, x, quality)


# --------------------------------------------------------------------------- nature
def rain(sec=40, density=55, level=-22):
    n = n_samples(sec)
    bed = spectral_noise(n, lambda f: pink(f) * bandpass_shape(350, 9000)(f))
    bed2 = spectral_noise(n, lambda f: pink(f) * bandpass_shape(350, 9000)(f))
    rumble = spectral_noise(n, lambda f: brown(f) * lowpass_shape(260)(f))
    mod = slow_lfo(n, 0.15, lo=0.85, hi=1.0)
    L = 0.55 * bed * mod + 0.15 * rumble
    R = 0.55 * bed2 * mod + 0.15 * rumble
    # gouttes individuelles
    count = int(density * sec)
    for _ in range(count):
        dur = RNG.uniform(0.002, 0.009)
        m = n_samples(dur * 6)
        t = np.arange(m) / SR
        burst = RNG.standard_normal(m) * np.exp(-t / dur)
        fc = RNG.uniform(1800, 7000)
        burst = fft_filter(np.concatenate([burst, np.zeros(m)]), bandpass_shape(fc * 0.6, fc * 1.4))[:m]
        amp = RNG.lognormal(-2.6, 0.7)
        l, r = pan(burst * amp, RNG.uniform(-0.9, 0.9))
        s = RNG.integers(0, n)
        add_wrapped(L, s, l)
        add_wrapped(R, s, r)
    # « plic » d'eau occasionnels (bulle à hauteur descendante)
    for _ in range(int(sec * 3)):
        m = n_samples(0.03)
        t = np.arange(m) / SR
        f0 = RNG.uniform(1200, 2600)
        f = f0 * (1 + 0.8 * np.exp(-t / 0.006))
        sig = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / 0.008) * RNG.uniform(0.03, 0.09)
        l, r = pan(sig, RNG.uniform(-0.8, 0.8))
        s = RNG.integers(0, n)
        add_wrapped(L, s, l)
        add_wrapped(R, s, r)
    L = fft_filter(L, lowpass_shape(11000, 1))
    R = fft_filter(R, lowpass_shape(11000, 1))
    return normalize(L, R, level)


def storm(sec=60):
    L, R = rain(sec, density=80, level=-23)
    n = len(L)
    for start in (6.0, 27.0, 46.5):
        dur = RNG.uniform(7, 10)
        m = n_samples(dur)
        t = np.arange(m) / SR
        att = RNG.uniform(0.4, 1.2)
        env = np.minimum(t / att, 1.0) * np.exp(-np.maximum(t - att, 0) / (dur / 3.5))
        # roulement : plusieurs bouffées
        rolls = slow_lfo(m, 1.5, lo=0.4, hi=1.0)
        body = spectral_noise(m, lambda f: brown(f) * lowpass_shape(140, 3)(f))
        crack = spectral_noise(m, lambda f: pink(f) * bandpass_shape(250, 1100)(f)) * np.exp(-t / 0.9)
        sig = (1.0 * body * rolls + 0.18 * crack) * env * 0.55
        p = RNG.uniform(-0.5, 0.5)
        l, r = pan(sig, p)
        s = n_samples(start)
        add_wrapped(L, s, l)
        add_wrapped(R, s, r)
    return normalize(L, R, -22)


def waves(sec=60):
    n = n_samples(sec)
    L = np.zeros(n)
    R = np.zeros(n)
    sea = spectral_noise(n, lambda f: brown(f) * lowpass_shape(400)(f))
    sea2 = spectral_noise(n, lambda f: brown(f) * lowpass_shape(400)(f))
    L += 0.25 * sea
    R += 0.25 * sea2
    t0 = 0.0
    while t0 < sec:
        period = RNG.uniform(7.5, 11.5)
        dur = period + 4.0
        m = n_samples(dur)
        t = np.arange(m) / SR
        rise = RNG.uniform(2.6, 3.8)
        body_env = np.where(t < rise, (t / rise) ** 2, np.exp(-(t - rise) / 2.2))
        foam_env = np.where(t < rise, (t / rise) ** 4, np.exp(-(t - rise) / 3.4))
        body = spectral_noise(m, lambda f: pink(f) * bandpass_shape(80, 900)(f))
        foam = spectral_noise(m, lambda f: pink(f) * bandpass_shape(900, 7000)(f))
        fizz = slow_lfo(m, 6, lo=0.6, hi=1.0)
        sig = 0.9 * body * body_env + 0.55 * foam * foam_env * fizz
        p = RNG.uniform(-0.45, 0.45)
        l, r = pan(sig * RNG.uniform(0.75, 1.0), p)
        s = n_samples(t0)
        add_wrapped(L, s, l)
        add_wrapped(R, s, r)
        t0 += period
    return normalize(L, R, -22)


def stream(sec=40):
    n = n_samples(sec)
    bedL = spectral_noise(n, lambda f: pink(f) * bandpass_shape(250, 3500)(f))
    bedR = spectral_noise(n, lambda f: pink(f) * bandpass_shape(250, 3500)(f))
    mod = slow_lfo(n, 2.0, lo=0.7, hi=1.0)
    L = 0.35 * bedL * mod
    R = 0.35 * bedR * mod
    for _ in range(int(sec * 140)):
        dur = RNG.uniform(0.012, 0.04)
        m = n_samples(dur)
        t = np.arange(m) / SR
        f0 = RNG.uniform(350, 1700)
        f = f0 * (1 + 0.9 * t / dur)  # bulle de Minnaert : hauteur montante
        sig = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.sin(np.pi * t / dur) ** 2
        sig *= RNG.lognormal(-2.3, 0.6)
        l, r = pan(sig, RNG.uniform(-0.9, 0.9))
        s = RNG.integers(0, n)
        add_wrapped(L, s, l)
        add_wrapped(R, s, r)
    return normalize(L, R, -23)


def wind(sec=50, leaves=True, level=-23):
    n = n_samples(sec)
    L = np.zeros(n)
    R = np.zeros(n)
    gust = slow_lfo(n, 0.12, lo=0.25, hi=1.0)
    for i, (lo, hi) in enumerate([(120, 260), (240, 520), (480, 1000), (900, 2000), (1800, 3600)]):
        g = slow_lfo(n, 0.25, lo=0.2, hi=1.0)
        a = spectral_noise(n, lambda f: pink(f) * bandpass_shape(lo, hi, 3)(f))
        b = spectral_noise(n, lambda f: pink(f) * bandpass_shape(lo, hi, 3)(f))
        w = 1.0 / (1 + i * 0.5)
        L += w * a * g * gust
        R += w * b * g * gust
    if leaves:
        flutter = slow_lfo(n, 18, lo=0.0, hi=1.0) ** 2
        lv = spectral_noise(n, lambda f: bandpass_shape(2200, 9000, 2)(f))
        lv2 = spectral_noise(n, lambda f: bandpass_shape(2200, 9000, 2)(f))
        L += 0.35 * lv * flutter * gust ** 2
        R += 0.35 * lv2 * np.roll(flutter, n // 3) * gust ** 2
    return normalize(L, R, level)


def fire(sec=40):
    n = n_samples(sec)
    roar = spectral_noise(n, lambda f: brown(f) * lowpass_shape(220)(f))
    roar2 = spectral_noise(n, lambda f: brown(f) * lowpass_shape(220)(f))
    breath = slow_lfo(n, 0.6, lo=0.6, hi=1.0)
    hiss = spectral_noise(n, lambda f: bandpass_shape(2500, 9000)(f)) * slow_lfo(n, 3, lo=0.2, hi=1.0)
    L = 0.6 * roar * breath + 0.04 * hiss
    R = 0.6 * roar2 * breath + 0.04 * np.roll(hiss, n // 2)
    # crépitements par grappes
    t = 0.0
    while t < sec:
        t += RNG.exponential(0.16)
        cluster = RNG.integers(1, 5)
        p = RNG.uniform(-0.7, 0.7)
        base_amp = RNG.lognormal(-1.8, 0.8)
        for c in range(cluster):
            dur = RNG.uniform(0.0004, 0.0025)
            m = n_samples(dur * 8) + 4
            tt = np.arange(m) / SR
            sig = RNG.standard_normal(m) * np.exp(-tt / dur)
            sig = np.diff(np.concatenate([[0], sig]))  # passe-haut simple
            sig *= base_amp * RNG.uniform(0.4, 1.0)
            l, r = pan(sig, p + RNG.uniform(-0.1, 0.1))
            s = n_samples(t + c * RNG.uniform(0.004, 0.03))
            add_wrapped(L, s, l)
            add_wrapped(R, s, r)
    # « pops » plus graves
    for _ in range(int(sec * 0.6)):
        m = n_samples(0.06)
        tt = np.arange(m) / SR
        sig = spectral_noise(m, bandpass_shape(250, 1400)) * np.exp(-tt / 0.012) * RNG.uniform(0.25, 0.6)
        l, r = pan(sig, RNG.uniform(-0.6, 0.6))
        s = RNG.integers(0, n)
        add_wrapped(L, s, l)
        add_wrapped(R, s, r)
    return normalize(L, R, -23)


def night(sec=48):
    n = n_samples(sec)
    bL, bR = wind(sec, leaves=False, level=-34)
    L = bL.copy()
    R = bR.copy()
    crickets = [(4400, 0.72, -0.6, 0.05), (4750, 0.86, 0.5, 0.035), (4150, 1.05, 0.1, 0.025)]
    for f, period, p, amp in crickets:
        t = RNG.uniform(0, period)
        while t < sec:
            for k in range(RNG.integers(3, 5)):
                m = n_samples(0.016)
                tt = np.arange(m) / SR
                sig = np.sin(2 * np.pi * f * tt) * np.sin(np.pi * tt / 0.016) ** 2 * amp
                l, r = pan(sig, p)
                s = n_samples(t + k * 0.03)
                add_wrapped(L, s, l)
                add_wrapped(R, s, r)
            t += period * RNG.uniform(0.92, 1.08)
    # chouette lointaine
    for start in (13.0, 36.0):
        for k, (d, pause) in enumerate([(0.38, 0.55), (0.28, 0.2), (0.55, 0.0)]):
            m = n_samples(d)
            tt = np.arange(m) / SR
            fr = 390 * (1 + 0.02 * np.sin(2 * np.pi * 5 * tt)) * (1 - 0.06 * tt / d)
            env = np.sin(np.pi * tt / d) ** 1.5
            sig = (np.sin(2 * np.pi * np.cumsum(fr) / SR) + 0.15 * np.sin(4 * np.pi * np.cumsum(fr) / SR)) * env * 0.05
            l, r = pan(sig, -0.3)
            s = n_samples(start + sum(x[0] + x[1] for x in [(0.38, 0.55), (0.28, 0.2)][:k]))
            add_wrapped(L, s, l)
            add_wrapped(R, s, r)
    L, R = circular_reverb(L, R, make_ir(2.5, 5000), wet=0.25)
    return normalize(L, R, -26)


def noise(kind, sec=24):
    n = n_samples(sec)
    shapes = {
        "white": lambda f: lowpass_shape(12000, 1)(f),
        "pink": lambda f: pink(f) * highpass_shape(25)(f),
        "brown": lambda f: brown(f) * highpass_shape(22)(f),
    }
    L = spectral_noise(n, shapes[kind])
    R = spectral_noise(n, shapes[kind])
    return normalize(L, R, {"white": -26, "pink": -23, "brown": -21}[kind])


# --------------------------------------------------------------------------- musique
def midi(m):
    return 440.0 * 2 ** ((m - 69) / 12)


def pad_note(freq, dur, att=3.0, rel=4.0, bright=0.5):
    m = n_samples(dur + rel)
    t = np.arange(m) / SR
    env = np.minimum(t / att, 1.0)
    env *= np.where(t > dur, np.exp(-(t - dur) / (rel / 4)), 1.0)
    sig = np.zeros(m)
    for det in (-0.004, 0.0, 0.0045):
        f = freq * (1 + det)
        for h in range(1, 7):
            amp = (1.0 / h ** 1.6) * (bright ** (h - 1))
            sig += amp * np.sin(2 * np.pi * f * h * t + RNG.uniform(0, 6.28))
    return sig * env


def ambient_music(sec=64):
    n = n_samples(sec)
    L = np.zeros(n)
    R = np.zeros(n)
    # Ré majeur 7 – Si mineur 9 – Sol majeur 7 – La sus2
    chords = [[50, 57, 61, 64, 66], [47, 54, 59, 61, 62], [43, 50, 54, 57, 59], [45, 52, 57, 59, 64]]
    step = sec / len(chords)
    for i, ch in enumerate(chords):
        for j, note in enumerate(ch):
            sig = pad_note(midi(note), step + 1.0, att=3.5, rel=5.0, bright=0.42) * (0.9 if j == 0 else 0.55)
            l, r = pan(sig, (j - 2) * 0.3)
            add_wrapped(L, n_samples(i * step), l)
            add_wrapped(R, n_samples(i * step), r)
    # légère scintillation aiguë
    shimmer = slow_lfo(n, 0.2, lo=0.0, hi=1.0)
    tt = np.arange(n) / SR
    for k, note in enumerate([74, 78, 81]):
        f = midi(note)
        f = round(f * sec) / sec  # fréquence entière sur la boucle → continuité
        L += 0.05 * np.sin(2 * np.pi * f * tt) * np.roll(shimmer, k * n // 3)
        R += 0.05 * np.sin(2 * np.pi * f * tt + 1.0) * np.roll(shimmer, k * n // 3 + n // 6)
    L = fft_filter(L, lowpass_shape(3500, 1))
    R = fft_filter(R, lowpass_shape(3500, 1))
    L, R = circular_reverb(L, R, make_ir(4.5, 4000), wet=0.5)
    return normalize(L, R, -24)


def piano_note(freq, vel=0.6, length=7.0):
    m = n_samples(length)
    t = np.arange(m) / SR
    sig = np.zeros(m)
    B = 0.00035
    base_decay = np.interp(freq, [80, 1200], [7.0, 2.2])
    for h in range(1, 14):
        fh = freq * h * np.sqrt(1 + B * h * h)
        if fh > SR / 2.2:
            break
        amp = (1.0 / h ** 1.15) * (vel ** (0.4 * h)) * RNG.uniform(0.8, 1.1)
        d = base_decay / h ** 0.55
        env = 0.55 * np.exp(-t / (d * 0.18)) + 0.45 * np.exp(-t / d)
        for det in (-0.0007, 0.0008):
            sig += 0.5 * amp * env * np.sin(2 * np.pi * fh * (1 + det) * t + RNG.uniform(0, 6.28))
    hammer = RNG.standard_normal(n_samples(0.01)) * np.exp(-np.arange(n_samples(0.01)) / (SR * 0.002))
    hammer = fft_filter(np.concatenate([hammer, np.zeros(len(hammer))]), lowpass_shape(2500))[: len(hammer)]
    sig[: len(hammer)] += 0.08 * hammer
    att = n_samples(0.003)
    sig[:att] *= np.linspace(0, 1, att)
    sig[-n_samples(0.3):] *= np.linspace(1, 0, n_samples(0.3))
    return sig * vel


def piano(sec=72):
    n = n_samples(sec)
    L = np.zeros(n)
    R = np.zeros(n)
    beat = 60.0 / 52.0
    beats = int(round(sec / beat))
    sec_per_beat = sec / beats
    # main gauche : accords arpégés lents (Ré – Si m – Sol – La)
    prog = [[38, 45, 50, 54], [35, 42, 47, 50], [31, 38, 43, 47], [33, 40, 45, 49]]
    bars = beats // 4
    for b in range(bars):
        ch = prog[b % len(prog)]
        for k, note in enumerate(ch):
            sig = piano_note(midi(note), vel=0.42 + 0.05 * (k == 0), length=6.5)
            t0 = (b * 4 + k * 0.5) * sec_per_beat
            l, r = pan(sig, -0.35 + k * 0.1)
            add_wrapped(L, n_samples(t0), l)
            add_wrapped(R, n_samples(t0), r)
    # main droite : mélodie pentatonique rêveuse, éparse
    scale = [62, 64, 66, 69, 71, 74, 76, 78]
    idx = 3
    b = 0.0
    while b < beats - 1:
        b += RNG.choice([1.0, 1.5, 2.0, 2.0, 3.0])
        if b >= beats - 0.5:
            break
        idx = int(np.clip(idx + RNG.choice([-2, -1, -1, 1, 1, 2]), 0, len(scale) - 1))
        sig = piano_note(midi(scale[idx]), vel=RNG.uniform(0.35, 0.5), length=5.5)
        l, r = pan(sig, 0.25 + RNG.uniform(-0.1, 0.1))
        add_wrapped(L, n_samples(b * sec_per_beat), l)
        add_wrapped(R, n_samples(b * sec_per_beat), r)
    L = fft_filter(L, lowpass_shape(6000, 1))
    R = fft_filter(R, lowpass_shape(6000, 1))
    L, R = circular_reverb(L, R, make_ir(3.8, 5000), wet=0.42)
    return normalize(L, R, -24)


def pads(sec=64):
    n = n_samples(sec)
    tt = np.arange(n) / SR
    L = np.zeros(n)
    R = np.zeros(n)
    base = [38, 45, 50, 57, 62]  # Ré – La – Ré – La – Ré (quintes ouvertes)
    for j, note in enumerate(base):
        f0 = midi(note)
        for h in range(1, 6):
            for det in (-0.003, 0.003):
                f = f0 * h * (1 + det)
                f = round(f * sec) / sec
                lfo = slow_lfo(n, 0.08, lo=0.1, hi=1.0)
                amp = 0.3 / (h ** 1.4) * (0.7 if j > 2 else 1.0)
                sig = amp * lfo * np.sin(2 * np.pi * f * tt + RNG.uniform(0, 6.28))
                l, r = pan(sig, RNG.uniform(-0.7, 0.7))
                L += l
                R += r
    air = spectral_noise(n, lambda f: pink(f) * bandpass_shape(400, 3000)(f)) * slow_lfo(n, 0.1, lo=0.05, hi=0.3)
    L += 0.2 * air
    R += 0.2 * np.roll(air, n // 2)
    L, R = circular_reverb(L, R, make_ir(5.0, 3500), wet=0.55)
    return normalize(L, R, -25)


def space(sec=64):
    n = n_samples(sec)
    tt = np.arange(n) / SR
    L = np.zeros(n)
    R = np.zeros(n)
    for note, amp in [(33, 0.5), (40, 0.3), (45, 0.25), (52, 0.12)]:
        f = round(midi(note) * sec) / sec
        lfo = slow_lfo(n, 0.06, lo=0.4, hi=1.0)
        L += amp * lfo * np.sin(2 * np.pi * f * tt)
        R += amp * lfo * np.sin(2 * np.pi * f * 1.0 * tt + 0.7)
    cosmic = spectral_noise(n, lambda f: pink(f) * bandpass_shape(150, 1200, 3)(f)) * slow_lfo(n, 0.05, lo=0.1, hi=0.6)
    L += 0.3 * cosmic
    R += 0.3 * np.roll(cosmic, n // 3)
    for _ in range(14):
        f = midi(RNG.choice([81, 83, 86, 88, 90, 93]))
        m = n_samples(3.0)
        t = np.arange(m) / SR
        sig = np.sin(2 * np.pi * f * t) * np.exp(-t / 0.9) * 0.05
        l, r = pan(sig, RNG.uniform(-0.9, 0.9))
        s = RNG.integers(0, n)
        add_wrapped(L, s, l)
        add_wrapped(R, s, r)
    L, R = circular_reverb(L, R, make_ir(6.0, 3000), wet=0.6)
    return normalize(L, R, -25)


def bell(sec=9.0):
    m = n_samples(sec)
    t = np.arange(m) / SR
    f0 = 293.66
    sig = np.zeros(m)
    for ratio, amp, dec in [(1.0, 1.0, 5.5), (2.71, 0.45, 3.2), (5.15, 0.22, 1.8), (8.43, 0.1, 1.0)]:
        for det in (0.0, 0.0018):
            sig += 0.5 * amp * np.exp(-t / dec) * np.sin(2 * np.pi * f0 * ratio * (1 + det) * t)
    att = n_samples(0.01)
    sig[:att] *= np.linspace(0, 1, att)
    sig[-n_samples(0.5):] *= np.linspace(1, 0, n_samples(0.5))
    L, R = pan(sig, -0.1)
    pk = np.max(np.abs(sig))
    return L / pk * 0.5, R / pk * 0.5


def breath_in(sec=1.1):
    m = n_samples(sec)
    t = np.arange(m) / SR
    env = np.sin(np.pi * t / sec) ** 2 * (t / sec) ** 0.5
    sig = spectral_noise(m, lambda f: pink(f) * bandpass_shape(450, 2600, 2)(f)) * env
    return sig / np.max(np.abs(sig)) * 0.06


SOUNDS = {
    "rain_soft": lambda: rain(),
    "storm": storm,
    "waves": waves,
    "stream": stream,
    "wind_trees": lambda: wind(50, leaves=True),
    "wind_soft": lambda: wind(50, leaves=False, level=-25),
    "fire": fire,
    "white": lambda: noise("white"),
    "pink": lambda: noise("pink"),
    "brown": lambda: noise("brown"),
    "night": night,
    "ambient": ambient_music,
    "piano": piano,
    "pads": pads,
    "space": space,
}


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "app/src/main/assets/sounds"
    only = sys.argv[2:] if len(sys.argv) > 2 else None
    os.makedirs(out, exist_ok=True)
    for name, fn in SOUNDS.items():
        if only and name not in only:
            continue
        print("→", name, flush=True)
        L, R = fn()
        write_ogg(os.path.join(out, name + ".ogg"), L, R, quality=3)
    if not only or "bell" in only:
        L, R = bell()
        write_ogg(os.path.join(out, "bell.ogg"), L, R, quality=4)
    print("ok")


if __name__ == "__main__":
    main()
