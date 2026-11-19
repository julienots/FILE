#!/usr/bin/env python3
"""Generates the original sounds of the "Duel des Tours" template (AGR ENGINE).

Everything is synthesized (no samples): mono 16-bit WAV at 22050 Hz, written to
editor/resources/templates/assets/games/duel/. Deterministic (fixed random seed).

    bow.wav        plucked string + air whoosh (archers, towers)
    clash.wav      metallic ring of inharmonic partials (melee hits)
    cannon.wav     low boom with a noise crack (mini tank)
    explosion.wav  long filtered noise burst with a sub thump (fireball)
    collapse.wav   rumble and falling stones (tower destroyed)
    deploy.wav     bright rising "pop" (a card is played)
    victory.wav    short major fanfare (match won)

    python3 tools/generate_duel_audio.py
"""
import math
import os
import random
import struct
import wave

RATE = 22050
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "editor", "resources", "templates", "assets", "games", "duel")
rnd = random.Random(1234)


def write(name, samples):
    os.makedirs(OUT, exist_ok=True)
    peak = max(1e-9, max(abs(s) for s in samples))
    gain = min(1.0, 0.9 / peak)
    with wave.open(os.path.join(OUT, name), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s * gain)) * 32767)) for s in samples))
    print(f"{name}: {len(samples) / RATE:.2f} s")


def n(seconds): return int(seconds * RATE)


def lowpass(samples, cutoff):
    """One-pole low-pass; cutoff may be a function of the sample index."""
    out, y = [], 0.0
    for i, x in enumerate(samples):
        c = cutoff(i) if callable(cutoff) else cutoff
        a = 1.0 - math.exp(-2.0 * math.pi * c / RATE)
        y += a * (x - y)
        out.append(y)
    return out


def noise(count): return [rnd.uniform(-1, 1) for _ in range(count)]


def env(i, attack, decay):
    t = i / RATE
    return min(1.0, t / attack) * math.exp(-t / decay) if attack > 0 else math.exp(-t / decay)


def bow():
    # Karplus-Strong pluck (string) + a short band of air noise.
    period = int(RATE / 196)
    buf = noise(period)
    out = []
    for i in range(n(0.45)):
        v = buf[i % period]
        buf[i % period] = 0.5 * (v + buf[(i + 1) % period]) * 0.994
        out.append(v * 0.8)
    air = lowpass(noise(n(0.45)), lambda i: 600 + 5000 * math.exp(-i / n(0.12)))
    return [o + a * 0.35 * env(i, 0.01, 0.12) for i, (o, a) in enumerate(zip(out, air))]


def clash():
    partials = [(1250, 1.0, 0.25), (1730, 0.7, 0.18), (2410, 0.6, 0.12), (3190, 0.4, 0.08), (4710, 0.3, 0.05)]
    hit = lowpass(noise(n(0.05)), 3000)
    out = []
    for i in range(n(0.5)):
        t = i / RATE
        s = sum(a * math.sin(2 * math.pi * f * t) * math.exp(-t / d) for f, a, d in partials)
        out.append(s * 0.5 + (hit[i] * 1.2 if i < len(hit) else 0.0))
    return out


def cannon():
    crack = lowpass(noise(n(0.08)), 4000)
    body = lowpass(noise(n(0.7)), lambda i: 1200 * math.exp(-i / n(0.15)) + 80)
    out = []
    for i in range(n(0.7)):
        t = i / RATE
        thump = math.sin(2 * math.pi * (70 - 30 * t) * t) * math.exp(-t / 0.18)
        out.append(thump * 1.2 + body[i] * 1.5 * env(i, 0.002, 0.2) + (crack[i] * 0.8 if i < len(crack) else 0.0))
    return out


def explosion():
    body = lowpass(noise(n(1.4)), lambda i: 2500 * math.exp(-i / n(0.25)) + 120)
    out = []
    for i in range(n(1.4)):
        t = i / RATE
        sub = math.sin(2 * math.pi * (55 - 20 * t) * t) * math.exp(-t / 0.35)
        out.append(body[i] * 2.0 * env(i, 0.004, 0.45) + sub * 1.3)
    return out


def collapse():
    rumble = lowpass(noise(n(1.8)), 160)
    out = [r * 3.0 * env(i, 0.05, 0.7) for i, r in enumerate(rumble)]
    # Falling stones: short filtered clicks spread over the first second.
    for _ in range(26):
        start = int(rnd.uniform(0.05, 1.2) * RATE)
        click = lowpass(noise(n(0.06)), rnd.uniform(600, 1800))
        amp = rnd.uniform(0.4, 1.0)
        for k, c in enumerate(click):
            if start + k < len(out):
                out[start + k] += c * amp * math.exp(-k / n(0.02))
    return out


def deploy():
    out = []
    for i in range(n(0.35)):
        t = i / RATE
        f = 420 + 900 * min(1.0, t / 0.08)
        s = math.sin(2 * math.pi * f * t) * env(i, 0.005, 0.09)
        s += 0.35 * math.sin(2 * math.pi * 2.0 * f * t) * env(i, 0.005, 0.05)
        out.append(s)
    sparkle = lowpass(noise(n(0.35)), 6000)
    return [o + s * 0.15 * env(i, 0.03, 0.08) for i, (o, s) in enumerate(zip(out, sparkle))]


def victory():
    notes = [(523.25, 0.0, 0.18), (659.25, 0.16, 0.18), (783.99, 0.32, 0.18), (1046.5, 0.48, 0.7)]
    out = [0.0] * n(1.3)
    for f, start, dur in notes:
        s0 = int(start * RATE)
        for k in range(n(dur + 0.3)):
            if s0 + k >= len(out):
                break
            t = k / RATE
            e = min(1.0, t / 0.01) * (math.exp(-t / dur) if t < dur else math.exp(-dur / dur) * math.exp(-(t - dur) / 0.08))
            # Bright brass-like tone: a few harmonics.
            out[s0 + k] += e * sum(math.sin(2 * math.pi * f * h * t) / h for h in (1, 2, 3, 4)) * 0.5
    return out


if __name__ == "__main__":
    for name, fn in [("bow", bow), ("clash", clash), ("cannon", cannon), ("explosion", explosion), ("collapse", collapse),
                     ("deploy", deploy), ("victory", victory)]:
        write(name + ".wav", fn())
