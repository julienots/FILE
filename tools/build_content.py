#!/usr/bin/env python3
"""
NOCTÉA — pipeline de production vocale v4 (Superessence Studio).

Objectif : UNE seule voix, réaliste, identique d'un bout à l'autre de l'application.

1. Analyse les scripts (tools/content/library.py) et la banque du générateur (bank.py).
2. Chaque phrase est rendue d'UN SEUL TENANT (la prosodie « Maintenant... tu peux fermer les yeux. »
   reste continue), puis coupée aux silences naturels des « ... » : plus de fragments isolés qui
   « repartent à zéro ». Les mots de comptage (Inspire, Retiens, deux, trois…) sortent d'un même rendu
   continu : même voix, même débit, même hauteur que le reste (l'ancienne « voix de comptage »
   était 3 demi-tons plus grave et plus rapide).
3. Contrôle qualité automatique de CHAQUE phrase : transcription Whisper (écart avec le texte voulu)
   et écart de hauteur avec le reste du corpus. Les rendus douteux sont refaits avec des variantes
   (débit, ligne de style, amorce de contexte, rendu fragment par fragment) et on garde le meilleur.
4. Chaîne studio identique pour tous les clips (voice_studio.py), encodage Opus 36 kb/s.
5. Écrit assets/voice/*.ogg et assets/content/catalog.json (même format que les versions précédentes).

Usage :
  python3 tools/fetch_models.py                    # une fois
  python3 tools/build_content.py                   # production complète (~1 h sur 4 cœurs, reprenable)
  python3 tools/build_content.py --engine piper --model tools/models/piper/fr_FR-siwis-medium.onnx
"""
import argparse
import glob
import hashlib
import json
import math
import multiprocessing as mp
import os
import random
import re
import subprocess
import sys
import tempfile
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "content"))
sys.path.insert(0, HERE)

import bank as BANK_MOD  # noqa: E402
import library as LIB  # noqa: E402
import voice_engine as E  # noqa: E402
import voice_studio as S  # noqa: E402

ROOT = os.path.dirname(HERE)
ASSETS = os.path.join(ROOT, "app", "src", "main", "assets")
CACHE_ROOT = os.path.join(HERE, ".cache", "voice_v4")
SR = E.SR

RENDER_VERSION = "r1"          # change si le texte envoyé au moteur ou la découpe changent
OPUS_KBPS = 36
ELLIPSIS_PAUSE_RANGE = (850, 1350)   # ms : pause réelle à chaque « ... » (variable, pas métronomique)
WER_OK = 0.15          # une variante à ce niveau est acceptée sans chercher mieux
WER_RETRY = 0.25       # au-delà, la phrase est refaite avec des variantes
PITCH_OK_ST = 1.25
PITCH_OK_CUE_ST = 0.5   # le comptage doit avoir la hauteur du récit à un demi-ton près (ancien défaut : −3 dem-tons)
PRIME = "Doucement."
ELLIPSIS_SPLIT = re.compile(r"(\.\.\.|…)\s+(?=\S)")

NUMBERS = ["un", "deux", "trois", "quatre", "cinq", "six", "sept", "huit", "neuf", "dix"]
CUES = {"in": "Inspire...", "hold": "Retiens...", "out": "Expire...", "hold2": "Pause..."}
CUE_FRAGMENTS = [CUES["in"], CUES["hold"], CUES["out"], CUES["hold2"]] + [n.capitalize() + "." for n in NUMBERS]
# Un fragment qui n'est QUE l'un de ces mots (« Dix... », « Inspire... », « un... ») est dit par le clip de comptage :
# même prononciation, même voix partout (les petits mots isolés sont le point faible des moteurs neuronaux).
CUE_WORDS = {f.lower().rstrip(".").rstrip("…").strip(): f for f in CUE_FRAGMENTS}

# Prononciations forcées (texte envoyé au moteur ≠ texte affiché) — complété après contrôle Whisper.
LEXICON = {}


def tts_text(text):
    t = text.replace("…", "...")
    for k, v in LEXICON.items():
        t = t.replace(k, v)
    return " ".join(t.split())


# --------------------------------------------------------------------------- unités de rendu
def split_fragments(text):
    """'Maintenant... tu peux fermer les yeux.' → ['Maintenant...', 'tu peux fermer les yeux.']"""
    parts = []
    last = 0
    for m in ELLIPSIS_SPLIT.finditer(text):
        parts.append(text[last:m.end(1)].strip())
        last = m.end()
    parts.append(text[last:].strip())
    return [p for p in parts if p]


class Unit:
    """Une phrase rendue d'un seul tenant, puis coupée en `frags`."""

    def __init__(self, frags, tts=None, kind="phrase"):
        self.frags = list(frags)
        self.kind = kind
        self.tts = tts if tts is not None else tts_text(" ".join(self.frags))
        self.ref = " ".join(self.frags)
        self.key = hashlib.sha1(f"{RENDER_VERSION}|{kind}|{self.tts}|{'/'.join(self.frags)}".encode()).hexdigest()[:16]

    def clip_key(self, i):
        return f"{self.key}.{i}"

    def to_job(self):
        return dict(frags=self.frags, tts=self.tts, kind=self.kind)


class Registry:
    def __init__(self):
        self.units = {}
        self.cue = None

    def unit(self, text):
        u = Unit(split_fragments(text), tts_text(text))
        self.units.setdefault(u.key, u)
        return self.units[u.key]

    def unit_from(self, frags):
        u = Unit(frags, tts_text(" ".join(frags)))
        self.units.setdefault(u.key, u)
        return self.units[u.key]

    def cue_unit(self):
        if self.cue is None:
            self.cue = Unit(CUE_FRAGMENTS, kind="cue")
            self.units[self.cue.key] = self.cue
        return self.cue

    def cue_key(self, word):
        u = self.cue_unit()
        canon = CUE_WORDS[word.lower().rstrip(".").rstrip("…").strip()]
        return u.clip_key(CUE_FRAGMENTS.index(canon))


REG = Registry()


def ellipsis_pause(text, i):
    h = int(hashlib.sha1(f"{text}|{i}".encode()).hexdigest()[:6], 16)
    lo, hi = ELLIPSIS_PAUSE_RANGE
    return lo + h % (hi - lo + 1)


def cue_word_of(fragment):
    """Le mot de comptage que ce fragment représente exactement, sinon None."""
    return CUE_WORDS.get(fragment.lower().rstrip(".").rstrip("…").rstrip(",").strip())


def phrase_segments(text):
    """Segments d'une phrase : clips + pauses réelles à chaque « ... » (durée variable, jamais métronomique)."""
    frags = split_fragments(text)
    items, group = [], []                    # ("n", fragment-comptage) | ("u", [fragments rendus d'un tenant])
    for f in frags:
        if cue_word_of(f):
            if group:
                items.append(("u", group))
                group = []
            items.append(("n", f))
        else:
            group.append(f)
    if group:
        items.append(("u", group))
    segs, k = [], 0
    for kind, v in items:
        if kind == "n":
            if k > 0:
                segs.append(("s", ellipsis_pause(text, k)))
            segs.append(("c", REG.cue_key(v)))
            k += 1
        else:
            u = REG.unit_from(v)
            for i in range(len(v)):
                if k > 0:
                    segs.append(("s", ellipsis_pause(text, k)))
                segs.append(("c", u.clip_key(i)))
                k += 1
    return segs


# --------------------------------------------------------------------------- rendu + contrôle
_engine = None
_whisper = None
_jiwer = None
_ref_f0 = None

W2D = {"0": "zéro", "1": "un", "2": "deux", "3": "trois", "4": "quatre", "5": "cinq", "6": "six", "7": "sept",
       "8": "huit", "9": "neuf", "10": "dix"}


def norm_words(s):
    s = s.lower().replace("’", "'").replace("…", " ")
    s = re.sub(r"[^\w\s']", " ", s).replace("'", " ")
    return " ".join(W2D.get(w, w) for w in s.split())


def _init_worker(engine_kind, model, whisper_dir, threads, use_asr):
    global _engine, _whisper, _jiwer
    _engine = E.make_engine(engine_kind, model, threads=threads)
    if use_asr:
        from faster_whisper import WhisperModel
        import jiwer
        _whisper = WhisperModel(whisper_dir, device="cpu", compute_type="int8", cpu_threads=max(1, threads))
        _jiwer = jiwer


CUE_SPEED = 0.80       # le comptage est un peu plus vif que le récit, avec la MÊME voix (même style, même chaîne)


def variants_for(engine, unit):
    s0 = engine.default_speed
    if unit.kind == "cue":
        # mots isolés (Inspire, Retiens, deux, trois…) : rendus un par un, les creux d'un rendu continu y sont trop faibles
        c = CUE_SPEED if engine.name == "kokoro" else s0
        return [("sep", c, 0), ("sep", round(c * 0.94, 3), 0), ("sep", round(c * 1.06, 3), 0),
                ("sep", c, 5), ("sep", c, -5)]
    v = [("whole", s0, 0),
         ("whole", round(s0 * 1.07, 3), 0), ("whole", round(s0 * 0.93, 3), 0),
         ("whole", s0, 5), ("whole", s0, -5)]
    if sum(c.isalnum() for c in unit.ref) < 70:
        v += [("prime", s0, 0), ("prime", round(s0 * 1.07, 3), 3)]
    v.append(("sep", s0, 0))
    return v


def variant_id(var):
    return f"{var[0]}_{var[1]:.3f}_{var[2]}"


def render_variant(unit, var):
    mode, speed, shift = var
    frags = unit.frags
    weigh = getattr(_engine, "phoneme_weights", None)
    if mode == "whole":
        x = _engine.render(unit.tts, speed, shift)
        return E.cut_at_pauses(x, frags, weights=weigh(frags) if weigh else None)
    if mode == "prime":
        x = _engine.render(PRIME + " " + unit.tts, speed, shift)
        pieces = E.cut_at_pauses(x, [PRIME] + frags, weights=weigh([PRIME] + frags) if weigh else None)
        return None if pieces is None else pieces[1:]
    pieces = []
    for f in frags:
        y = _engine.render(tts_text(f), speed, shift)
        a, b = E.speech_region(y)
        pieces.append(y[a:b])
    return pieces


def _as_audio(pieces):
    gap = np.zeros(int(0.7 * SR), dtype=np.float32)
    seq = []
    for i, p in enumerate(pieces):
        if i:
            seq.append(gap)
        seq.append(p)
    return np.concatenate(seq)


def _evaluate(unit, pieces, use_asr):
    import voice_qa as Q
    x = _as_audio(pieces)
    res = {}
    f0, _, _ = Q.pitch_and_hnr(x, SR)
    res["f0"] = f0
    res["dur"] = len(x) / SR
    if use_asr:
        import scipy.signal as ss
        segs, _ = _whisper.transcribe(ss.resample_poly(x, 2, 3).astype(np.float32), language="fr", beam_size=3,
                                      condition_on_previous_text=False, vad_filter=False)
        hyp = " ".join(s.text for s in segs)
        ref = norm_words(unit.ref)
        res["hyp"] = hyp.strip()
        res["wer"] = float(_jiwer.wer(ref, norm_words(hyp))) if ref else 0.0
    else:
        res["wer"] = 0.0
    return res


def _paths(cache, unit, vid):
    return (os.path.join(cache, "raw", f"{unit.key}__{vid}.npz"), os.path.join(cache, "raw", f"{unit.key}__{vid}.json"))


def run_unit(job):
    """Rend (ou relit) les variantes d'une unité jusqu'à en trouver une bonne. Retourne le meilleur résumé."""
    cache, unit_d, variants, f0_ref, use_asr = job
    unit = Unit(unit_d["frags"], unit_d["tts"], unit_d.get("kind", "phrase"))
    os.makedirs(os.path.join(cache, "raw"), exist_ok=True)
    for var in variants:
        vid = variant_id(var)
        npz, meta = _paths(cache, unit, vid)
        cached = json.load(open(meta)) if os.path.exists(meta) else None
        if cached is not None and (cached.get("ok") is False or not use_asr or "hyp" in cached):
            info = cached
        else:
            pieces = render_variant(unit, var)
            if pieces is None:
                info = dict(ok=False, wer=1.0, f0=None, vid=vid)
                json.dump(info, open(meta, "w"))
                continue
            info = _evaluate(unit, pieces, use_asr)
            info.update(ok=True, vid=vid)
            np.savez_compressed(npz, n=len(pieces), **{f"p{i}": (np.clip(p, -1, 1) * 32767).astype(np.int16)
                                                      for i, p in enumerate(pieces)})
            json.dump(info, open(meta, "w"), ensure_ascii=False)
        if not info["ok"]:
            continue
        dev = None
        if f0_ref and info.get("f0"):
            dev = 12 * math.log2(info["f0"] / f0_ref)
        pitch_ok = PITCH_OK_CUE_ST if unit.kind == "cue" else PITCH_OK_ST
        if info["wer"] <= WER_OK and (dev is None or abs(dev) <= pitch_ok):
            break
    return unit.key


def score(info, f0_ref):
    dev = 0.0
    if f0_ref and info.get("f0"):
        dev = abs(12 * math.log2(info["f0"] / f0_ref))
    return (round(info["wer"] / 0.05), dev)


def best_variant(cache, unit, f0_ref):
    best, best_s = None, None
    for meta in glob.glob(os.path.join(cache, "raw", f"{unit.key}__*.json")):
        info = json.load(open(meta))
        if not info.get("ok"):
            continue
        s = score(info, f0_ref)
        if best is None or s < best_s:
            best, best_s = info, s
    return best


def load_pieces(cache, unit, vid):
    z = np.load(_paths(cache, unit, vid)[0])
    return [z[f"p{i}"].astype(np.float32) / 32767.0 for i in range(int(z["n"]))]


# --------------------------------------------------------------------------- encodage
def encode_opus(y, path):
    import soundfile as sf
    with tempfile.TemporaryDirectory() as td:
        wav = os.path.join(td, "c.wav")
        sf.write(wav, y, SR, subtype="PCM_16")
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-ar", str(SR), "-ac", "1",
                        "-c:a", "libopus", "-b:a", f"{OPUS_KBPS}k", "-vbr", "on", "-compression_level", "10",
                        "-application", "audio", path], check=True)


# --------------------------------------------------------------------------- scripts
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
            REG.cue_unit()


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
                            word = CUES[ph] if k == 0 else NUMBERS[k]
                            add_clip(REG.cue_key(word))
                            add_sil(max(0, 1000 * (k + 1) - (t - st)))
                    elif mode == "cue":
                        add_clip(REG.cue_key(CUES[ph]))
                    add_sil(total - (t - st))   # si un mot dépasse sa seconde, on se resynchronise en fin de phase
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


# --------------------------------------------------------------------------- bruitages
def build_fx(voice_dir):
    import build_sounds as BS
    BS.SR = SR
    fx = {}
    L, R = BS.bell(5.0)                      # bol court pour l'ouverture/clôture des séances
    x = (L + R) / 2
    n = len(x)
    x[-int(1.8 * SR):] *= np.linspace(1, 0, int(1.8 * SR)) ** 2
    x = (x / np.max(np.abs(x)) * 0.32).astype(np.float32)
    _encode_fx(os.path.join(voice_dir, "bell.ogg"), x, 32)
    fx["bell"] = int(round(n / SR * 1000))
    b = BS.breath_in(1.1).astype(np.float32)  # respiration : même « pièce » que la voix
    b = S.fx_chain(b * 0.9)
    _encode_fx(os.path.join(voice_dir, "breath.ogg"), b, 32)
    fx["breath"] = int(round(len(b) / SR * 1000))
    return fx


def _encode_fx(path, x, kbps):
    import soundfile as sf
    with tempfile.TemporaryDirectory() as td:
        w = os.path.join(td, "fx.wav")
        sf.write(w, np.clip(x, -1, 1).astype(np.float32), SR, subtype="PCM_16")
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", w, "-ar", str(SR), "-c:a", "libopus",
                        "-b:a", f"{kbps}k", path], check=True)


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
    ap.add_argument("--engine", default="kokoro", choices=["kokoro", "piper"])
    ap.add_argument("--model", default="")
    ap.add_argument("--whisper", default=os.path.join(HERE, "models", "whisper-small"))
    ap.add_argument("--jobs", type=int, default=2, help="processus de rendu en parallèle")
    ap.add_argument("--threads", type=int, default=0, help="threads par processus (défaut : cœurs / jobs)")
    ap.add_argument("--no-asr", action="store_true", help="saute le contrôle Whisper (itérations rapides)")
    ap.add_argument("--stage", default="all", choices=["render", "all"])
    ap.add_argument("--limit", type=int, default=0, help="n'utilise que N unités (essais ; avec --stage render)")
    args = ap.parse_args()
    if not args.model:
        args.model = os.path.join(HERE, "models", "kokoro" if args.engine == "kokoro" else "piper",
                                  "" if args.engine == "kokoro" else "fr_FR-siwis-medium.onnx")
    use_asr = not args.no_asr
    threads = args.threads or max(1, (os.cpu_count() or 4) // args.jobs)
    cache = os.path.join(CACHE_ROOT, args.engine)
    os.makedirs(cache, exist_ok=True)

    voice_dir = os.path.join(ASSETS, "voice")
    content_dir = os.path.join(ASSETS, "content")

    # 1) inventaire des unités de rendu
    track_ops = {}
    for tr in LIB.TRACKS:
        ops = parse_script(tr)
        register_ops(ops)
        track_ops[tr["id"]] = ops
    bank_phr = {}
    for pool, items in BANK_MOD.BANK.items():
        bank_phr[pool] = [(it, phrase_segments(it)) for it in items]
    units = sorted(REG.units.values(), key=lambda u: u.tts)
    if args.limit:
        units = units[: args.limit]
    n_frag = sum(len(u.frags) for u in units)
    print(f"{len(units)} phrases à rendre ({n_frag} clips) — moteur {args.engine}, contrôle Whisper : {use_asr}", flush=True)

    ctx = mp.get_context("spawn")
    init = (args.engine, args.model, args.whisper, threads, use_asr)

    class _Eng:  # débit par défaut, sans charger le modèle dans le processus principal
        name = args.engine
        default_speed = E.KokoroEngine.default_speed if args.engine == "kokoro" else E.PiperEngine.default_speed

    def run_pool(jobs, label):
        t0 = time.time()
        done = 0
        with ctx.Pool(args.jobs, initializer=_init_worker, initargs=init) as pool:
            for _ in pool.imap_unordered(run_unit, jobs, chunksize=2):
                done += 1
                if done % 50 == 0 or done == len(jobs):
                    el = time.time() - t0
                    print(f"  {label} {done}/{len(jobs)}  ({el / 60:.1f} min, reste ≈ {el / done * (len(jobs) - done) / 60:.1f} min)", flush=True)

    # 2) phase A : rendu standard de toutes les phrases + contrôle
    run_pool([(cache, u.to_job(), [variants_for(_Eng, u)[0]], None, use_asr) for u in units], "A")
    f0s = []
    for u in units:
        info = best_variant(cache, u, None)
        if info and info.get("f0") and len(u.ref) > 12:
            f0s.append(info["f0"])
    f0_ref = float(np.median(f0s))
    print(f"hauteur de référence du corpus : {f0_ref:.1f} Hz", flush=True)

    # 3) phase B : variantes pour les rendus douteux (prononciation ou hauteur hors norme)
    bad = []
    for u in units:
        info = best_variant(cache, u, f0_ref)
        dev = 12 * math.log2(info["f0"] / f0_ref) if info and info.get("f0") else 0.0
        if u.kind == "cue":
            if info is None or info["wer"] > WER_OK or abs(dev) > PITCH_OK_CUE_ST:
                bad.append(u)
        elif info is None or info["wer"] > WER_RETRY or abs(dev) > PITCH_OK_ST:
            bad.append(u)
    print(f"{len(bad)} phrases à retravailler avec des variantes ({100 * len(bad) / len(units):.0f} %)", flush=True)
    if bad:
        run_pool([(cache, u.to_job(), variants_for(_Eng, u), f0_ref, use_asr) for u in bad], "B")

    chosen = {}
    wers, devs, still = [], [], []
    for u in units:
        info = best_variant(cache, u, f0_ref)
        chosen[u.key] = info
        wers.append(info["wer"])
        if info.get("f0"):
            devs.append(12 * math.log2(info["f0"] / f0_ref))
        if info["wer"] > 0.3:
            still.append((info["wer"], u.ref, info.get("hyp", "")))
    print(f"contrôle : WER médian {100 * np.median(wers):.1f} %, moyen {100 * np.mean(wers):.1f} %, "
          f"{sum(w > 0.3 for w in wers)} phrases > 30 % ; hauteur : σ {np.std(devs):.2f} dem-tons", flush=True)
    rep = os.path.join(cache, "rapport.txt")
    with open(rep, "w", encoding="utf-8") as f:
        for w, ref, hyp in sorted(still, reverse=True):
            f.write(f"{w:.2f} | {ref} | {hyp}\n")
    print(f"phrases encore douteuses → {rep}", flush=True)
    if args.stage == "render":
        return

    # 4) traitement studio + encodage Opus de chaque clip
    os.makedirs(voice_dir, exist_ok=True)
    os.makedirs(content_dir, exist_ok=True)
    opus_dir = os.path.join(cache, "opus")
    os.makedirs(opus_dir, exist_ok=True)
    clip_ms = {}
    clip_file = {}
    t0 = time.time()
    for n, u in enumerate(units):
        info = chosen[u.key]
        pieces = load_pieces(cache, u, info["vid"])
        assert len(pieces) == len(u.frags), (u.ref, info["vid"])
        for i, p in enumerate(pieces):
            key = u.clip_key(i)
            tag = hashlib.sha1(f"{S.CHAIN_VERSION}|{OPUS_KBPS}|{info['vid']}|{key}".encode()).hexdigest()[:10]
            out = os.path.join(opus_dir, f"{key}.{tag}.ogg")
            if u.kind == "cue":
                p = S.match_pitch(p, f0_ref)      # le comptage prend exactement la hauteur du récit
            y = S.process(p)
            clip_ms[key] = int(round(len(y) / SR * 1000))
            if not os.path.exists(out):
                encode_opus(y, out)
            clip_file[key] = out
        if (n + 1) % 200 == 0:
            print(f"  studio {n + 1}/{len(units)} ({(time.time() - t0) / 60:.1f} min)", flush=True)

    # 5) identifiants courts et copie dans les assets
    for f in os.listdir(voice_dir):
        if f.endswith(".ogg"):
            os.unlink(os.path.join(voice_dir, f))
    keys = sorted(clip_ms.keys(), key=lambda k: (REG.units[k.split(".")[0]].tts, int(k.split(".")[1])))
    clip_ids = {k: f"v{i:04d}" for i, k in enumerate(keys)}
    for k, cid in clip_ids.items():
        with open(clip_file[k], "rb") as src, open(os.path.join(voice_dir, cid + ".ogg"), "wb") as dst:
            dst.write(src.read())
    fx_ms = build_fx(voice_dir)
    # 60 s de silence Opus (même format que la voix → enchaînements sans reconfiguration audio)
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-f", "lavfi", "-i", f"anullsrc=r={SR}:cl=mono",
                    "-t", "60", "-c:a", "libopus", "-b:a", "16k", os.path.join(voice_dir, "silence.ogg")], check=True)

    # 6) compilation des séances
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

    # 7) banque du générateur
    bank = {}
    for pool, items in bank_phr.items():
        entries = []
        for text, segs in items:
            d = sum(clip_ms[v] if k == "c" else v for k, v in segs)
            entries.append(dict(text=text, segments=seg_json(segs, clip_ids), durationMs=d))
        bank[pool] = entries

    catalog = dict(
        version=1,
        voice=dict(name=("Siwis · Kokoro-82M" if args.engine == "kokoro" else "Siwis · Piper"),
                   engine=("kokoro-82m" if args.engine == "kokoro" else "piper-vits"), language="fr-FR",
                   pipeline=f"{RENDER_VERSION}/{S.CHAIN_VERSION}"),
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
