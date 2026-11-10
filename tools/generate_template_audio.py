#!/usr/bin/env python3
"""Generates the original sounds of the editor templates (AGR ENGINE).

Everything is synthesized here (no samples): mono 16-bit WAV at 22050 Hz, written to
editor/resources/templates/assets/audio. Deterministic (fixed random seed).

    python3 tools/generate_template_audio.py
"""
import math
import os
import random
import struct
import wave

RATE = 22050
OUT = os.path.join(os.path.dirname(__file__), "..", "editor", "resources", "templates", "assets", "audio")


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


def envelope(t, attack, decay_time):
    a = min(1.0, t / attack) if attack > 0 else 1.0
    return a * math.exp(-t / decay_time)


def pulse(phase, width=0.5):
    return 1.0 if (phase % 1.0) < width else -1.0


def triangle(phase):
    p = phase % 1.0
    return 4.0 * abs(p - 0.5) - 1.0


def jump():
    n = int(0.2 * RATE)
    out, phase = [], 0.0
    for i in range(n):
        t = i / RATE
        f = 320.0 + 520.0 * (t / 0.2)  # rising chirp
        phase += f / RATE
        out.append(0.5 * pulse(phase, 0.3) * envelope(t, 0.005, 0.07))
    return out


def coin():
    n = int(0.32 * RATE)
    out = []
    for i in range(n):
        t = i / RATE
        f = 988.0 if t < 0.07 else 1319.0
        s = math.sin(2 * math.pi * f * t) + 0.3 * math.sin(4 * math.pi * f * t)
        out.append(0.5 * s * envelope(t if t < 0.07 else t - 0.07, 0.002, 0.09 if t < 0.07 else 0.12))
    return out


def click():
    n = int(0.06 * RATE)
    return [0.6 * math.sin(2 * math.pi * 1800 * i / RATE) * math.exp(-i / (0.008 * RATE)) for i in range(n)]


def launch():
    rnd = random.Random(7)
    n = int(0.4 * RATE)
    out, low = [], 0.0
    for i in range(n):
        t = i / RATE
        k = 0.05 + 0.5 * (1 - t / 0.4)  # filter opens then closes
        low += k * (rnd.uniform(-1, 1) - low)
        thump = math.sin(2 * math.pi * (90 - 60 * t) * t) * math.exp(-t / 0.06)
        out.append((0.7 * low + 0.8 * thump) * envelope(t, 0.004, 0.15))
    return out


def music():
    # 4 bars of 2 s (120 BPM, 4 beats), loops seamlessly: A minor, F, C, G.
    chords = [(57, 60, 64), (53, 57, 60), (48, 52, 55), (55, 59, 62)]
    bar, total = 2.0, 8.0
    n = int(total * RATE)
    out = []
    phases = [0.0, 0.0]
    for i in range(n):
        t = i / RATE
        chord = chords[int(t // bar) % 4]
        # Bass: root, one octave down, on each beat.
        beat_t = t % 0.5
        bass_f = 440.0 * 2 ** ((chord[0] - 12 - 69) / 12)
        phases[0] += bass_f / RATE
        bass = 0.45 * triangle(phases[0]) * math.exp(-beat_t / 0.35)
        # Arpeggio: sixteenth notes over the chord, one octave up.
        step = int(t / 0.125)
        note = chord[step % 3] + 12 * (1 + (step // 3) % 2)
        f = 440.0 * 2 ** ((note - 69) / 12)
        phases[1] += f / RATE
        arp = 0.18 * pulse(phases[1], 0.25) * math.exp(-(t % 0.125) / 0.06)
        out.append(bass + arp)
    # Tiny fades at the loop point to avoid clicks.
    fade = int(0.004 * RATE)
    for i in range(fade):
        out[i] *= i / fade
        out[-1 - i] *= i / fade
    return out


def ambient():
    # Wind: low-passed noise with a slow swell; the end is crossfaded into the start.
    rnd = random.Random(11)
    total = 4.0
    n = int(total * RATE)
    extra = int(0.5 * RATE)
    raw, low, low2 = [], 0.0, 0.0
    for i in range(n + extra):
        t = i / RATE
        low += 0.02 * (rnd.uniform(-1, 1) - low)
        low2 += 0.05 * (low - low2)
        swell = 0.6 + 0.4 * math.sin(2 * math.pi * t / total)
        raw.append(low2 * swell)
    out = raw[:n]
    for i in range(extra):
        w = i / extra
        out[i] = out[i] * w + raw[n + i] * (1 - w)
    return out


if __name__ == "__main__":
    write("jump.wav", jump())
    write("coin.wav", coin())
    write("click.wav", click())
    write("launch.wav", launch())
    write("music.wav", music())
    write("ambient.wav", ambient())
