#!/usr/bin/env python3
"""
NOCTÉA — pipeline de production vocale (Superessence Studio).

1. Analyse les scripts (tools/content/library.py) et la banque du générateur (bank.py).
2. Découpe chaque phrase sur les « ... » pour insérer de VRAIS silences.
3. Synthétise chaque fragment avec la voix féminine française « siwis » (Piper, CC-BY 4.0),
   débit ralenti, puis traitement studio : égalisation chaude, de-essing, compression douce,
   légère réverbération de pièce, normalisation.
4. Encode en Opus (Ogg) et écrit assets/voice/*.ogg + assets/content/catalog.json.

Usage : python3 tools/build_content.py [--model /opt/voices/fr_FR-siwis-medium.onnx]
"""
import argparse
import hashlib
import json
import math
import os
import random
import re
import subprocess
import sys
import tempfile
import wave
from multiprocessing import Pool

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "content"))
sys.path.insert(0, HERE)

import bank as BANK_MOD  # noqa: E402
import library as LIB  # noqa: E402

ROOT = os.path.dirname(HERE)
ASSETS = os.path.join(ROOT, "app", "src", "main", "assets")
CACHE = os.path.join(HERE, ".cache", "voice")

# Réglages de la voix : débit lent, prosodie stable, rythme naturel.
VOICE = dict(length_scale=1.24, noise_scale=0.55, noise_w_scale=0.78)
CUE_VOICE = dict(length_scale=1.02, noise_scale=0.5, noise_w_scale=0.6)  # mots de comptage (tenir en 1 s)
CHAIN = ("highpass=f=70,equalizer=f=210:t=q:w=1.0:g=2.2,equalizer=f=3000:t=q:w=1.6:g=-1.5,"
         "highshelf=f=7200:g=-4.5,deesser=i=0.35,"
         "acompressor=threshold=-22dB:ratio=2:attack=12:release=220:makeup=1.5,"
         "apad=pad_dur=0.18,aecho=0.88:0.55:23|37:0.09|0.05")
CHAIN_VERSION = "v3"
ELLIPSIS_PAUSE_MS = 1050
SR = 22050

ELLIPSIS_SPLIT = re.compile(r"(\.\.\.|…)\s+(?=\S)")


# --------------------------------------------------------------------------- fragments
def split_fragments(text):
    """'Maintenant... tu peux fermer les yeux.' → ['Maintenant...', 'tu peux fermer les yeux.']"""
    parts = []
    last = 0
    for m in ELLIPSIS_SPLIT.finditer(text):
        parts.append(text[last:m.end(1)].strip())
        last = m.end()
    parts.append(text[last:].strip())
    return [p for p in parts if p]


class Registry:
    def __init__(self):
        self.frags = {}  # key → (text, settings_name)

    def key(self, text, settings="voice"):
        k = hashlib.sha1(f"{CHAIN_VERSION}|{settings}|{text}".encode()).hexdigest()[:16]
        self.frags[k] = (text, settings)
        return k


REG = Registry()


def phrase_segments(text, settings="voice"):
    segs = []
    frags = split_fragments(text)
    for i, f in enumerate(frags):
        if i > 0:
            segs.append(("s", ELLIPSIS_PAUSE_MS))
        segs.append(("c", REG.key(f, settings)))
    return segs


# --------------------------------------------------------------------------- rendu vocal
_voice = None


def _init_worker(model):
    global _voice
    from piper import PiperVoice
    _voice = PiperVoice.load(model)


def _render(job):
    key, text, settings = job
    out_ogg = os.path.join(CACHE, key + ".ogg")
    out_meta = os.path.join(CACHE, key + ".json")
    if os.path.exists(out_ogg) and os.path.exists(out_meta):
        return key, json.load(open(out_meta))["ms"]
    from piper import SynthesisConfig
    cfg = SynthesisConfig(**(VOICE if settings == "voice" else CUE_VOICE))
    chunks = []
    for chunk in _voice.synthesize(text, syn_config=cfg):
        chunks.append(chunk.audio_int16_array.astype(np.float32) / 32768.0)
    x = np.concatenate(chunks) if chunks else np.zeros(SR // 2, dtype=np.float32)
    x = trim(x)
    with tempfile.TemporaryDirectory() as td:
        raw = os.path.join(td, "raw.wav")
        proc = os.path.join(td, "proc.wav")
        write_wav(raw, x)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", raw, "-af", CHAIN,
                        "-ar", str(SR), "-c:a", "pcm_f32le", proc], check=True)
        y = read_wav_f32(proc)
        y = trim(y, keep_tail_ms=160)
        y = level(y)
        n_fade_in = int(0.008 * SR)
        n_fade_out = int(0.09 * SR)
        y[:n_fade_in] *= np.linspace(0, 1, n_fade_in)
        y[-n_fade_out:] *= np.linspace(1, 0, n_fade_out)
        final = os.path.join(td, "final.wav")
        write_wav(final, y)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", final, "-ar", "24000",
                        "-c:a", "libopus", "-b:a", "28k", "-application", "audio", out_ogg], check=True)
    ms = int(round(len(y) / SR * 1000))
    json.dump({"ms": ms, "text": text}, open(out_meta, "w"), ensure_ascii=False)
    return key, ms


def trim(x, thresh_db=-46, keep_head_ms=30, keep_tail_ms=60):
    win = int(0.01 * SR)
    if len(x) < win * 3:
        return x
    env = np.sqrt(np.convolve(x ** 2, np.ones(win) / win, mode="same"))
    thr = 10 ** (thresh_db / 20)
    idx = np.where(env > thr)[0]
    if len(idx) == 0:
        return x
    a = max(0, idx[0] - int(keep_head_ms / 1000 * SR))
    b = min(len(x), idx[-1] + int(keep_tail_ms / 1000 * SR))
    return x[a:b]


def level(y, target_db=-20.0, peak_db=-2.0):
    win = int(0.02 * SR)
    frames = len(y) // win
    if frames == 0:
        return y
    f = y[: frames * win].reshape(frames, win)
    rms = np.sqrt(np.mean(f ** 2, axis=1))
    active = rms[rms > 10 ** (-40 / 20)]
    cur = np.sqrt(np.mean(active ** 2)) if len(active) else np.sqrt(np.mean(y ** 2)) + 1e-9
    g = 10 ** (target_db / 20) / (cur + 1e-9)
    y = y * g
    pk = np.max(np.abs(y)) + 1e-9
    lim = 10 ** (peak_db / 20)
    if pk > lim:
        y = np.tanh(y / lim) * lim
    return y.astype(np.float32)


def write_wav(path, x):
    pcm = (np.clip(x, -1, 1) * 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())


def read_wav_f32(path):
    data = open(path, "rb").read()
    # recherche du chunk 'data' (WAV float32 mono écrit par ffmpeg)
    i = data.find(b"data")
    n = int.from_bytes(data[i + 4:i + 8], "little")
    return np.frombuffer(data[i + 8:i + 8 + n], dtype="<f4").copy()


# --------------------------------------------------------------------------- scripts
NUMBERS = ["un", "deux", "trois", "quatre", "cinq", "six", "sept", "huit", "neuf", "dix"]
CUES = {"in": "Inspire...", "hold": "Retiens...", "out": "Expire...", "hold2": "Pause..."}


def parse_script(track):
    """Retourne une liste d'instructions abstraites (avant connaissance des durées)."""
    default_pause = track.get("pause", 4.0)
    rnd = random.Random(track["id"])
    ops = []
    for raw in track["script"].strip().splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.startswith("~"):
            ops.append(("silence", int(float(line[1:]) * 1000)))
        elif line == "@bell":
            ops.append(("fx", "bell"))
        elif line == "@breath":
            ops.append(("fx", "breath"))
        elif line.startswith("@pool"):
            _, pool, rng, pause = line.split()
            a, b = (int(v) for v in rng.split("-"))
            items = BANK_MOD.BANK[pool][a:b + 1]
            assert items, f"pool vide {pool} {rng} dans {track['id']}"
            for it in items:
                ops.append(("say", it, int(float(pause) * 1000 * rnd.uniform(0.92, 1.1))))
        elif line.startswith("@cycles"):
            p = line.split()
            ops.append(("cycles", float(p[1]), float(p[2]), float(p[3]), float(p[4]), int(p[5]), p[6]))
        else:
            m = re.match(r"^(.*?)\s*([|=])\s*([\d.]+)\s*$", line)
            if m:
                text, kind, val = m.group(1), m.group(2), float(m.group(3))
                if kind == "|":
                    ops.append(("say", text, int(val * 1000)))
                else:
                    ops.append(("slot", text, int(val * 1000)))
            else:
                ops.append(("say", line, int(default_pause * 1000 * rnd.uniform(0.9, 1.12))))
    return ops


def register_ops(ops):
    for op in ops:
        if op[0] in ("say", "slot"):
            phrase_segments(op[1])
        elif op[0] == "cycles":
            for w in list(CUES.values()) + NUMBERS[1:8]:
                REG.key(w, "cue")


def compile_ops(ops, clip_ms, fx_ms, track_id):
    """Instructions → segments concrets + blocs respiratoires (en ms)."""
    segs = []
    breath_blocks = []
    rnd = random.Random("breath-" + track_id)
    t = 0

    def add_clip(key):
        nonlocal t
        segs.append(("c", key))
        t += clip_ms[key]

    def add_sil(ms):
        nonlocal t
        if ms <= 0:
            return
        segs.append(("s", int(ms)))
        t += int(ms)

    def add_phrase(text):
        for kind, v in phrase_segments(text):
            if kind == "c":
                add_clip(v)
            else:
                add_sil(v)

    prev_pause = 0
    for op in ops:
        kind = op[0]
        if kind == "silence":
            add_sil(op[1])
            prev_pause += op[1]
        elif kind == "fx":
            segs.append(("fx", op[1]))
            t += fx_ms[op[1]]
            prev_pause = 0
        elif kind == "say":
            # respiration naturelle discrète avant certaines phrases, dans les longues pauses
            if prev_pause >= 5000 and rnd.random() < 0.18 and segs and segs[-1][0] == "s":
                last = segs.pop()
                t -= last[1]
                add_sil(last[1] - fx_ms["breath"] - 250)
                segs.append(("fx", "breath"))
                t += fx_ms["breath"]
                add_sil(250)
            add_phrase(op[1])
            add_sil(op[2])
            prev_pause = op[2]
        elif kind == "slot":
            start = t
            add_phrase(op[1])
            add_sil(max(300, op[2] - (t - start)))
            prev_pause = op[2]
        elif kind == "cycles":
            _, i_s, h_s, o_s, h2_s, count, mode = op
            block_start = t
            phases = [("in", i_s), ("hold", h_s), ("out", o_s), ("hold2", h2_s)]
            for c in range(count):
                for ph, secs in phases:
                    if secs <= 0:
                        continue
                    total = int(secs * 1000)
                    st = t
                    if mode == "count":
                        for k in range(int(secs)):
                            slot_start = t
                            word = CUES[ph] if k == 0 else NUMBERS[k]
                            key = REG.key(word, "cue")
                            add_clip(key)
                            add_sil(max(0, 1000 * (k + 1) - (t - st)))
                            _ = slot_start
                    elif mode == "cue":
                        add_clip(REG.key(CUES[ph], "cue"))
                    add_sil(total - (t - st))
                    if t - st != total:
                        # le mot dépasse : on resynchronise au cycle suivant
                        pass
            breath_blocks.append(dict(start=block_start, inhale=int(i_s * 1000), hold=int(h_s * 1000),
                                      exhale=int(o_s * 1000), hold2=int(h2_s * 1000), count=count,
                                      end=t))
            prev_pause = 0
    # fusion des silences consécutifs
    merged = []
    for s in segs:
        if s[0] == "s" and merged and merged[-1][0] == "s":
            merged[-1] = ("s", merged[-1][1] + s[1])
        else:
            merged.append(s)
    return merged, breath_blocks, t


def transcript(ops):
    lines = []
    for op in ops:
        if op[0] in ("say", "slot"):
            lines.append(op[1])
        elif op[0] == "cycles":
            lines.append(f"[Exercice respiratoire : {int(op[1])}-{int(op[2])}-{int(op[3])}-{int(op[4])} × {op[5]}]")
    return "\n".join(lines)


# --------------------------------------------------------------------------- fx
def build_fx(voice_dir):
    sys.path.insert(0, HERE)
    import build_sounds as BS
    BS.SR = SR
    fx = {}
    # bol court pour l'ouverture/clôture des séances
    L, R = BS.bell(5.0)
    x = (L + R) / 2
    n = len(x)
    x[-int(1.8 * SR):] *= np.linspace(1, 0, int(1.8 * SR)) ** 2
    x = x / np.max(np.abs(x)) * 0.32
    _encode_fx(os.path.join(voice_dir, "bell.ogg"), x)
    fx["bell"] = int(round(n / SR * 1000))
    b = BS.breath_in(1.1)
    _encode_fx(os.path.join(voice_dir, "breath.ogg"), b * 0.9)
    fx["breath"] = int(round(len(b) / SR * 1000))
    return fx


def _encode_fx(path, x):
    with tempfile.TemporaryDirectory() as td:
        w = os.path.join(td, "fx.wav")
        write_wav(w, x.astype(np.float32))
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", w, "-ar", "24000", "-c:a", "libopus",
                        "-b:a", "32k", path], check=True)


# --------------------------------------------------------------------------- main
def seg_json(segs, clip_ids):
    out = []
    for kind, v in segs:
        if kind == "c":
            out.append(clip_ids[v])
        elif kind == "fx":
            out.append("fx:" + v)
        else:
            out.append(int(v))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="/opt/voices/fr_FR-siwis-medium.onnx")
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args()

    os.makedirs(CACHE, exist_ok=True)
    voice_dir = os.path.join(ASSETS, "voice")
    content_dir = os.path.join(ASSETS, "content")
    os.makedirs(voice_dir, exist_ok=True)
    os.makedirs(content_dir, exist_ok=True)

    # 1) enregistrement de tous les fragments
    track_ops = {}
    for tr in LIB.TRACKS:
        ops = parse_script(tr)
        register_ops(ops)
        track_ops[tr["id"]] = ops
    bank_phr = {}
    for pool, items in BANK_MOD.BANK.items():
        bank_phr[pool] = [(it, phrase_segments(it)) for it in items]

    jobs = [(k, t, s) for k, (t, s) in sorted(REG.frags.items())]
    print(f"{len(jobs)} fragments vocaux à produire…", flush=True)
    clip_ms = {}
    with Pool(args.jobs, initializer=_init_worker, initargs=(args.model,)) as pool:
        for i, (k, ms) in enumerate(pool.imap_unordered(_render, jobs, chunksize=4)):
            clip_ms[k] = ms
            if (i + 1) % 100 == 0:
                print(f"  {i + 1}/{len(jobs)}", flush=True)

    # 2) identifiants courts et copie dans les assets
    for f in os.listdir(voice_dir):
        if f.endswith(".ogg"):
            os.unlink(os.path.join(voice_dir, f))
    keys = sorted(clip_ms.keys(), key=lambda k: (REG.frags[k][1], REG.frags[k][0]))
    clip_ids = {k: f"v{i:04d}" for i, k in enumerate(keys)}
    for k, cid in clip_ids.items():
        with open(os.path.join(CACHE, k + ".ogg"), "rb") as src, open(os.path.join(voice_dir, cid + ".ogg"), "wb") as dst:
            dst.write(src.read())
    fx_ms = build_fx(voice_dir)
    # 60 s de silence Opus (même format que la voix → enchaînements sans reconfiguration audio)
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-f", "lavfi", "-i", "anullsrc=r=24000:cl=mono",
                    "-t", "60", "-c:a", "libopus", "-b:a", "16k", os.path.join(voice_dir, "silence.ogg")], check=True)

    # 3) compilation des séances
    tracks = []
    cat_ids = [c[0] for c in LIB.CATEGORIES]
    for tr in LIB.TRACKS:
        assert tr["cat"] in cat_ids, tr["id"]
        ops = track_ops[tr["id"]]
        segs, blocks, t = compile_ops(ops, clip_ms, fx_ms, tr["id"])
        tail = int(tr.get("tail", 20) * 1000)
        segs.append(("s", tail))
        total = t + tail
        tracks.append(dict(
            id=tr["id"], category=tr["cat"], title=tr["title"], description=tr["desc"], art=tr["art"],
            durationMs=total, premium=tr.get("premium", False),
            ambience=[dict(sound=s, volume=v) for s, v in tr.get("amb", []) if v > 0],
            breath=blocks, segments=seg_json(segs, clip_ids), text=transcript(ops),
        ))

    # 4) banque du générateur
    bank = {}
    for pool, items in bank_phr.items():
        entries = []
        for text, segs in items:
            d = sum(clip_ms[v] if k == "c" else v for k, v in segs)
            entries.append(dict(text=text, segments=seg_json(segs, clip_ids), durationMs=d))
        bank[pool] = entries

    catalog = dict(
        version=1,
        voice=dict(name="Siwis (Piper, CC-BY 4.0)", engine="piper-vits", language="fr-FR"),
        clips={clip_ids[k]: clip_ms[k] for k in keys},
        fx=fx_ms,
        categories=[dict(id=c[0], title=c[1], subtitle=c[2]) for c in LIB.CATEGORIES],
        tracks=tracks,
        bank=bank,
        quotes=[dict(text=q[0], author=q[1]) for q in LIB.QUOTES],
    )
    with open(os.path.join(content_dir, "catalog.json"), "w", encoding="utf-8") as f:
        json.dump(catalog, f, ensure_ascii=False, separators=(",", ":"))
    tot_speech = sum(clip_ms.values()) / 60000
    print(f"OK : {len(keys)} clips ({tot_speech:.1f} min de voix), {len(tracks)} séances.")
    for tr in tracks:
        print(f"  {tr['id']:<20} {tr['durationMs'] / 60000:5.1f} min")


if __name__ == "__main__":
    main()
