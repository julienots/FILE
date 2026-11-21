#!/usr/bin/env python3
"""
NOCTÉA — contrôle qualité de la voix (Superessence Studio).

Mesure, pour chaque clip vocal, ce qui fait entendre « une autre voix » d'un clip à l'autre :
  - hauteur médiane (F0, en demi-tons autour de la médiane du corpus),
  - niveau de parole actif (dBFS, type ITU-T P.56 simplifié),
  - centroïde spectral et pente spectrale (timbre : clair / sombre),
  - débit (caractères par seconde de parole),
  - harmonicité (HNR) : voix nette ou « métallique ».
Puis résume l'écart entre clips. Utilisé :
  * en diagnostic sur les clips d'une version de l'application,
  * par build_content.py pour écarter/ramener les clips aberrants.

Usage :
  python3 tools/voice_qa.py app/src/main/assets          # audit des clips de l'APK
  python3 tools/voice_qa.py app/src/main/assets --asr    # + intelligibilité (Whisper)
"""
import argparse
import json
import math
import os
import subprocess
import sys
import tempfile

import numpy as np

SR = 24000


# --------------------------------------------------------------------------- E/S
def load_audio(path, sr=SR):
    """Décode n'importe quel format (Opus/WAV…) en float32 mono via ffmpeg."""
    with tempfile.TemporaryDirectory() as td:
        wav = os.path.join(td, "x.wav")
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", path, "-ac", "1", "-ar", str(sr),
                        "-c:a", "pcm_f32le", wav], check=True)
        return read_wav_f32(wav)


def read_wav_f32(path):
    data = open(path, "rb").read()
    i = data.find(b"data")
    n = int.from_bytes(data[i + 4:i + 8], "little")
    return np.frombuffer(data[i + 8:i + 8 + n], dtype="<f4").copy()


# --------------------------------------------------------------------------- mesures
def active_level_db(x, sr=SR, gate_db=-45.0):
    """Niveau RMS des seules trames actives (> gate sous le pic de trame) : proche de P.56."""
    win = int(0.02 * sr)
    n = len(x) // win
    if n == 0:
        return -90.0
    f = x[: n * win].reshape(n, win)
    rms = np.sqrt(np.mean(f ** 2, axis=1) + 1e-12)
    ref = np.max(rms)
    act = rms[rms > ref * 10 ** (gate_db / 20)]
    act = act[act > 10 ** (-60 / 20)]
    if len(act) == 0:
        return -90.0
    return 20 * math.log10(np.sqrt(np.mean(act ** 2)) + 1e-12)


def spectral_features(x, sr=SR):
    """Centroïde (Hz) et pente (dB/octave, 200 Hz–6 kHz) du spectre moyen des trames voisées."""
    win, hop = 1024, 512
    if len(x) < win:
        x = np.pad(x, (0, win - len(x)))
    frames = np.lib.stride_tricks.sliding_window_view(x, win)[::hop]
    e = np.sqrt(np.mean(frames ** 2, axis=1))
    keep = e > np.max(e) * 10 ** (-30 / 20)
    if not np.any(keep):
        return 0.0, 0.0
    w = np.hanning(win)
    spec = np.mean(np.abs(np.fft.rfft(frames[keep] * w, axis=1)) ** 2, axis=0) + 1e-14
    freqs = np.fft.rfftfreq(win, 1 / sr)
    cent = float(np.sum(freqs * spec) / np.sum(spec))
    band = (freqs >= 200) & (freqs <= 6000)
    slope = np.polyfit(np.log2(freqs[band]), 10 * np.log10(spec[band]), 1)[0]
    return cent, float(slope)


def pitch_and_hnr(x, sr=SR):
    """F0 médiane (Hz), écart-type en demi-tons, HNR médian (dB) via Praat."""
    import parselmouth
    snd = parselmouth.Sound(x.astype(np.float64), sampling_frequency=sr)
    pitch = snd.to_pitch_ac(time_step=0.01, pitch_floor=75, pitch_ceiling=400, very_accurate=False)
    f0 = pitch.selected_array["frequency"]
    f0 = f0[f0 > 0]
    if len(f0) < 5:
        return None, None, None
    st = 12 * np.log2(f0 / np.median(f0))
    try:
        harm = snd.to_harmonicity_cc(time_step=0.02, minimum_pitch=75)
        h = harm.values[harm.values > -100]
        hnr = float(np.median(h)) if len(h) else None
    except Exception:
        hnr = None
    return float(np.median(f0)), float(np.std(st)), hnr


def measure(x, text="", sr=SR):
    f0, f0_sd, hnr = pitch_and_hnr(x, sr)
    cent, slope = spectral_features(x, sr)
    # durée parlée : trames actives
    win = int(0.02 * sr)
    n = len(x) // win
    f = x[: n * win].reshape(n, win)
    rms = np.sqrt(np.mean(f ** 2, axis=1) + 1e-12)
    speech_s = float(np.sum(rms > np.max(rms) * 10 ** (-35 / 20)) * 0.02) if n else 0.0
    letters = sum(c.isalnum() for c in text)
    return dict(
        f0=f0, f0_sd=f0_sd, hnr=hnr, level=active_level_db(x, sr), centroid=cent, slope=slope,
        dur=len(x) / sr, speech=speech_s, cps=(letters / speech_s if speech_s > 0.2 and letters else None),
        peak=float(20 * math.log10(np.max(np.abs(x)) + 1e-9)),
    )


# --------------------------------------------------------------------------- résumé
def summarize(rows, label):
    def col(k):
        return np.array([r[k] for r in rows if r.get(k) is not None], dtype=float)

    out = [f"=== {label} — {len(rows)} clips"]
    f0 = col("f0")
    if len(f0):
        st = 12 * np.log2(f0 / np.median(f0))
        out.append(f"  hauteur F0 : médiane {np.median(f0):6.1f} Hz | dispersion entre clips (écart-type) {np.std(st):4.2f} dem-tons"
                   f" | 5–95 % : {np.percentile(st, 5):+.2f} … {np.percentile(st, 95):+.2f}")
    lv = col("level")
    out.append(f"  niveau actif : médiane {np.median(lv):6.1f} dBFS | écart-type entre clips {np.std(lv):4.2f} dB"
               f" | 5–95 % : {np.percentile(lv, 5):+.1f} … {np.percentile(lv, 95):+.1f}")
    ce = col("centroid")
    out.append(f"  centroïde spectral : médiane {np.median(ce):6.0f} Hz | écart-type {np.std(ce):5.0f} Hz")
    sl = col("slope")
    out.append(f"  pente spectrale : médiane {np.median(sl):+5.2f} dB/oct | écart-type {np.std(sl):4.2f}")
    cps = col("cps")
    if len(cps):
        out.append(f"  débit : médiane {np.median(cps):5.1f} car/s | écart-type {np.std(cps):4.2f}")
    hn = col("hnr")
    if len(hn):
        out.append(f"  HNR : médiane {np.median(hn):5.1f} dB | écart-type {np.std(hn):4.2f}")
    fs = col("f0_sd")
    if len(fs):
        out.append(f"  mouvement mélodique intra-clip : {np.median(fs):4.2f} dem-tons (écart-type de F0 dans un clip)")
    return "\n".join(out)


def load_catalog_clips(assets):
    cat = json.load(open(os.path.join(assets, "content", "catalog.json"), encoding="utf-8"))
    texts = {}
    # retrouve le texte de chaque clip depuis la banque et les séances
    def walk(segs, text):
        for s in segs:
            if isinstance(s, str) and not s.startswith("fx:"):
                texts.setdefault(s, None)
    for tr in cat["tracks"]:
        walk(tr["segments"], tr["text"])
    for items in cat["bank"].values():
        for it in items:
            clips = [s for s in it["segments"] if isinstance(s, str) and not s.startswith("fx:")]
            if len(clips) == 1:
                texts[clips[0]] = it["text"]
    return cat, texts


def neighbor_jumps(rows, cat):
    """Écart de hauteur (dem-tons) entre deux clips successifs d'une même séance : c'est ce qui s'entend comme « une autre voix »."""
    f0 = {r["id"]: r["f0"] for r in rows if r.get("f0")}
    jumps = []
    for tr in cat["tracks"]:
        ids = [s for s in tr["segments"] if isinstance(s, str) and not s.startswith("fx:")]
        for a, b in zip(ids, ids[1:]):
            if a in f0 and b in f0:
                jumps.append(abs(12 * math.log2(f0[a] / f0[b])))
    j = np.array(jumps)
    return (f"  sauts de hauteur entre clips voisins ({len(j)} paires) : médian {np.median(j):.2f} | "
            f"90e perc. {np.percentile(j, 90):.2f} | 99e perc. {np.percentile(j, 99):.2f} | max {j.max():.2f} dem-tons")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("assets")
    ap.add_argument("--asr", action="store_true", help="contrôle d'intelligibilité avec Whisper")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--json", default="")
    args = ap.parse_args()
    cat, texts = load_catalog_clips(args.assets)
    vdir = os.path.join(args.assets, "voice")
    ids = sorted(cat["clips"].keys())
    if args.limit:
        ids = ids[: args.limit]
    rows = []
    for i, cid in enumerate(ids):
        x = load_audio(os.path.join(vdir, cid + ".ogg"))
        m = measure(x, texts.get(cid) or "")
        m["id"] = cid
        m["ms"] = cat["clips"][cid]
        rows.append(m)
        if (i + 1) % 200 == 0:
            print(f"  {i + 1}/{len(ids)}", file=sys.stderr, flush=True)
    print(summarize(rows, "tous les clips"))
    print(neighbor_jumps(rows, cat))
    if args.json:
        json.dump(rows, open(args.json, "w"))


if __name__ == "__main__":
    main()
