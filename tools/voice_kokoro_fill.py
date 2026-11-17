#!/usr/bin/env python3
"""
NOCTÉA — complément vocal rapide (Kokoro, voix « ff_siwis », Apache 2.0).

Enregistre en quelques minutes les phrases que le studio Chatterbox n'a pas encore produites
(tools/.cache/raw/<clé>.json absent). Kokoro « ff_siwis » est entraîné sur la même comédienne
(corpus SIWIS) : on ralentit le débit et on abaisse légèrement la hauteur pour se rapprocher du
timbre Chatterbox. Chaque fragment entre « ... » est synthétisé séparément, les pauses sont donc
exactes.

Installation : pip install kokoro-onnx soundfile
Modèles : kokoro-v1.0.onnx et voices-v1.0.bin (github.com/thewh1teagle/kokoro-onnx, releases)
Usage : python3 tools/voice_kokoro_fill.py --model kokoro-v1.0.onnx --voices voices-v1.0.bin
"""
import argparse
import json
import os
import re
import subprocess
import tempfile

import numpy as np
import soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
RAW = os.path.join(HERE, ".cache", "raw")
SR = 24000
SPEED = 0.82
PITCH_SEMITONES = -2.0
ELLIPSIS_SPLIT = re.compile(r"(\.\.\.|…)\s+(?=\S)")


def fragments(text):
    parts, last = [], 0
    for m in ELLIPSIS_SPLIT.finditer(text):
        parts.append(text[last:m.end(1)].strip())
        last = m.end()
    parts.append(text[last:].strip())
    return [p for p in parts if p]


def clean(t):
    t = t.replace("«", "").replace("»", "").strip()
    t = re.sub(r"(\.\.\.|…)\s*$", ".", t)
    t = re.sub(r"\s*(\.\.\.|…)\s+", ", ", t)
    return t


def lower_pitch(x):
    if abs(PITCH_SEMITONES) < 1e-3:
        return x
    with tempfile.TemporaryDirectory() as td:
        a, b = os.path.join(td, "a.wav"), os.path.join(td, "b.wav")
        sf.write(a, x, SR, subtype="FLOAT")
        ratio = 2 ** (PITCH_SEMITONES / 12)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", a, "-af",
                        f"rubberband=pitch={ratio:.5f}:pitchq=quality:formant=preserved", b], check=True)
        y, _ = sf.read(b, dtype="float32")
    return y


def trim(x, rel_db=-35):
    win = int(0.01 * SR)
    n = len(x) // win
    if n < 3:
        return x
    env = np.sqrt(np.mean(x[: n * win].reshape(n, win) ** 2, axis=1) + 1e-12)
    idx = np.where(env > np.max(env) * 10 ** (rel_db / 20))[0]
    if len(idx) == 0:
        return x
    return x[max(0, idx[0] * win - int(0.02 * SR)): min(len(x), (idx[-1] + 1) * win + int(0.06 * SR))]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", required=True)
    ap.add_argument("--voices", required=True)
    ap.add_argument("--jobs", default=os.path.join(HERE, ".cache", "jobs.json"))
    args = ap.parse_args()
    from kokoro_onnx import Kokoro
    k = Kokoro(args.model, args.voices)
    jobs = json.load(open(args.jobs, encoding="utf-8"))
    todo = [j for j in jobs if not os.path.exists(os.path.join(RAW, j["key"] + ".json"))]
    print(f"{len(todo)} phrases à compléter avec Kokoro.", flush=True)
    gap = np.zeros(int(0.25 * SR), dtype=np.float32)
    for n, job in enumerate(todo):
        parts, pauses, pos = [], [], 0
        cue = job["settings"] == "cue"
        for i, f in enumerate(fragments(job["text"])):
            w, sr = k.create(clean(f), voice="ff_siwis", speed=0.95 if cue else SPEED, lang="fr-fr")
            assert sr == SR
            w = trim(np.asarray(w, dtype=np.float32))
            if i > 0:
                pauses.append((pos + len(gap) / 2) / SR)
                parts.append(gap)
                pos += len(gap)
            parts.append(w)
            pos += len(w)
        x = lower_pitch(np.concatenate(parts))
        key = job["key"]
        sf.write(os.path.join(RAW, key + ".wav"), np.clip(x, -1, 1), SR, subtype="PCM_16")
        meta = dict(text=job["text"], said=clean(job["text"]), settings=job["settings"], mode="kokoro",
                    take=0, ratio=1.0, ok=True, pauses=pauses, heard="", speech=0, sr=SR)
        json.dump(meta, open(os.path.join(RAW, key + ".json"), "w", encoding="utf-8"), ensure_ascii=False)
        if (n + 1) % 50 == 0:
            print(f"  {n + 1}/{len(todo)}", flush=True)
    print("OK", flush=True)


if __name__ == "__main__":
    main()
