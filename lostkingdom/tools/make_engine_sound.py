#!/usr/bin/env python3
"""LOST KINGDOM — synthesises assets/audio/sfx/engine.ogg: a seamless 1 s loop of a combustion engine at idle
(firing pulses of a 4-stroke twin, exhaust rumble, a little mechanical noise). The game loops it and raises its
pitch with the vehicle's speed (AudioEngine::setEngine)."""
import os, subprocess, tempfile, wave
import numpy as np
R = 44100; n = R
t = np.arange(n) / R
rng = np.random.default_rng(7)
fire = 40.0                                   # firing rate (Hz) at idle; integer -> seamless loop
x = np.zeros(n)
for k, a in ((1, 1.0), (2, 0.55), (3, 0.35), (4, 0.22), (6, 0.12), (8, 0.06)):
    x += a * np.sin(2 * np.pi * fire * k * t + rng.uniform(0, 6.28))
pulse = (0.5 + 0.5 * np.sin(2 * np.pi * fire * t)) ** 6          # sharp exhaust pulses
noise = rng.standard_normal(n)
noise = np.convolve(noise, np.ones(30) / 30, mode="same")       # low rumble
x = x * (0.6 + 0.4 * pulse) + 0.35 * noise * pulse
x += 0.15 * np.sin(2 * np.pi * 20.0 * t)                         # sub rumble
x = x / np.max(np.abs(x)) * 0.8
# seamless edges: crossfade the last 20 ms into the start
f = int(0.02 * R); w = np.linspace(0, 1, f)
x[:f] = x[:f] * w + x[-f:] * (1 - w)
pcm = (x * 32767).astype(np.int16)
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "audio", "sfx", "engine.ogg")
with tempfile.TemporaryDirectory() as d:
    wp = os.path.join(d, "e.wav")
    with wave.open(wp, "wb") as wf:
        wf.setnchannels(1); wf.setsampwidth(2); wf.setframerate(R); wf.writeframes(pcm.tobytes())
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wp, "-c:a", "libvorbis", "-q:a", "4", out], check=True)
print("wrote", out, os.path.getsize(out), "bytes")
