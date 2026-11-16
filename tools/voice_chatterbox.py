#!/usr/bin/env python3
"""
NOCTÉA — studio vocal « Chatterbox » (Superessence Studio).

Synthétise chaque phrase de la bibliothèque avec Chatterbox Multilingual (Resemble AI, licence MIT),
en clonant le timbre d'une comédienne française (tools/voice/reference_siwis.wav, CC-BY 4.0).

Chaque phrase est dite d'un seul tenant, avec ses « ... » : la voix garde ainsi une intonation
suspendue avant les silences, comme une vraie guide. Un modèle génératif pouvant inventer ou avaler
des mots, CHAQUE prise est réécoutée par Whisper : on ne garde que celles dont la transcription
correspond au texte et dont les pauses tombent au bon endroit. Sinon on refait une prise, puis on
se rabat sur une synthèse fragment par fragment.

Entrée  : tools/.cache/jobs.json (écrit par build_content.py --engine chatterbox)
Sortie  : tools/.cache/raw/<clé>.wav (24 kHz mono) + <clé>.json (transcription, positions des pauses)

Installation (Python 3.11) :
    pip install chatterbox-tts faster-whisper soundfile "setuptools<81"
Usage :
    python3 tools/voice_chatterbox.py [--ref tools/voice/reference_siwis.wav] [--threads 4]
"""
import argparse
import difflib
import hashlib
import json
import os
import re
import sys
import time
import unicodedata

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(HERE, ".cache")
RAW = os.path.join(CACHE, "raw")

# Réglages d'interprétation : peu d'emphase, débit posé, prosodie stable.
STYLE = dict(exaggeration=0.3, cfg_weight=0.3, temperature=0.6)
MAX_TAKES = 4
MAX_TAKES_SHORT = 7
MIN_RATIO = 0.82
MIN_PAUSE_S = 0.07

ELLIPSIS_SPLIT = re.compile(r"(\.\.\.|…)\s+(?=\S)")
DIGITS = {"0": "zéro", "1": "un", "2": "deux", "3": "trois", "4": "quatre", "5": "cinq", "6": "six",
          "7": "sept", "8": "huit", "9": "neuf", "10": "dix"}
# Whisper écrit parfois phonétiquement les mots isolés.
HOMOPHONES = {"disse": "dix", "dis": "dix", "sis": "six", "cis": "six", "sisse": "six", "cette": "sept",
              "set": "sept", "de": "deux", "d": "deux", "the": "deux", "uit": "huit", "oui": "huit",
              "hein": "un", "trois": "trois", "troie": "trois", "cat": "quatre", "catre": "quatre",
              "sain": "cinq", "saint": "cinq", "sein": "cinq", "sink": "cinq", "tient": "tiens",
              "tien": "tiens", "inspir": "inspire", "inspirent": "inspire", "expir": "expire",
              "expirent": "expire", "retient": "retiens", "retien": "retiens", "pose": "pause"}


def fragments(text):
    parts, last = [], 0
    for m in ELLIPSIS_SPLIT.finditer(text):
        parts.append(text[last:m.end(1)].strip())
        last = m.end()
    parts.append(text[last:].strip())
    return [p for p in parts if p]


def spoken(text):
    """Texte envoyé au moteur : « ... » interne → virgule (intonation suspendue), final → point."""
    t = text.strip().replace("«", "").replace("»", "").replace("  ", " ")
    t = re.sub(r"(\.\.\.|…)\s*$", ".", t)
    t = re.sub(r"\s*(\.\.\.|…)\s+", ", ", t)
    t = re.sub(r"\s+([,.?!])", r"\1", t)
    t = re.sub(r",\s*,", ",", t)
    if t and t[-1] not in ".?!":
        t += "."
    return t[0].upper() + t[1:] if t else t


def norm_words(text):
    t = unicodedata.normalize("NFC", text.lower())
    t = re.sub(r"[’'\-–—]", " ", t)
    t = re.sub(r"[^\w\s]", " ", t)
    out = []
    for w in t.split():
        out.append(DIGITS.get(w, w))
    return out


def canon(w):
    return HOMOPHONES.get(w, w)


class Studio:
    def __init__(self, ref, threads):
        import torch
        torch.set_num_threads(threads)
        self.torch = torch
        from chatterbox.mtl_tts import ChatterboxMultilingualTTS
        from faster_whisper import WhisperModel
        import librosa
        self.librosa = librosa
        self.tts = ChatterboxMultilingualTTS.from_pretrained(device="cpu")
        self.tts.prepare_conditionals(ref, exaggeration=STYLE["exaggeration"])
        self.sr = self.tts.sr
        self.asr = WhisperModel("turbo", device="cpu", compute_type="int8", cpu_threads=threads)

    def take(self, text, seed):
        self.torch.manual_seed(seed)
        w = self.tts.generate(text, language_id="fr", **STYLE)
        return w.squeeze().numpy().astype(np.float32)

    def listen(self, wav):
        y = self.librosa.resample(wav, orig_sr=self.sr, target_sr=16000)
        segs, _ = self.asr.transcribe(y, language="fr", beam_size=3, word_timestamps=True,
                                      condition_on_previous_text=False)
        toks = []
        for s in segs:
            for w in s.words:
                for t in norm_words(w.word):
                    toks.append((t, float(w.start), float(w.end)))
        return toks

    # ---------------------------------------------------------------- analyse d'une prise
    def envelope(self, wav):
        hop = int(0.01 * self.sr)
        n = len(wav) // hop
        if n == 0:
            return np.zeros(1), hop
        fr = wav[: n * hop].reshape(n, hop)
        return np.sqrt(np.mean(fr ** 2, axis=1) + 1e-12), hop

    def speech_seconds(self, env):
        thr = max(np.percentile(env, 95) * 0.08, 1e-4)
        return float(np.sum(env > thr)) * 0.01

    def islands(self, env):
        """Nombre de blocs de parole séparés par au moins 0,3 s de silence."""
        thr = max(np.percentile(env, 95) * 0.056, 1e-4)  # −25 dB sous le niveau de la voix
        n, gap, inside = 0, 99, False
        for v in env:
            if v > thr:
                if not inside and gap >= 30:
                    n += 1
                inside, gap = True, 0
            else:
                inside = False
                gap += 1
        return n

    def find_pause(self, env, t_from, t_to):
        """Plus long creux d'énergie dans la fenêtre [t_from, t_to] (secondes) → centre, ou None."""
        thr = max(np.percentile(env, 95) * 0.07, 1e-4)
        a = max(0, int((t_from - 0.25) * 100))
        b = min(len(env), int((t_to + 0.25) * 100))
        best, cur_start = None, None
        for i in range(a, b + 1):
            low = i < b and env[i] < thr
            if low and cur_start is None:
                cur_start = i
            if not low and cur_start is not None:
                if best is None or i - cur_start > best[1] - best[0]:
                    best = (cur_start, i)
                cur_start = None
        if best is None or (best[1] - best[0]) * 0.01 < MIN_PAUSE_S:
            return None
        return (best[0] + best[1]) / 2 * 0.01

    def judge(self, text, wav):
        """Retourne dict(ratio, ok, pauses=[s|None], heard)."""
        exp_frags = [norm_words(spoken(f)) for f in fragments(text)]
        exp = [w for f in exp_frags for w in f]
        toks = self.listen(wav)
        heard = [t[0] for t in toks]
        env, _ = self.envelope(wav)
        sp = self.speech_seconds(env)
        said = spoken(text)
        n_chars = len(said)
        isl = self.islands(env)
        if len(exp) <= 2:
            ce, ch = [canon(w) for w in exp], [canon(w) for w in heard]
            ratio = difflib.SequenceMatcher(None, " ".join(ce), " ".join(ch)).ratio()
            ok = len(ch) == len(ce) and ratio >= 0.6 and sp <= 0.11 * n_chars + 0.75 and isl == 1
        else:
            ratio = difflib.SequenceMatcher(None, " ".join(exp), " ".join(heard)).ratio()
            ok = (ratio >= MIN_RATIO and len(heard) <= len(exp) + 2 and sp <= 0.13 * n_chars + 1.2
                  and isl <= len(re.findall(r"[,.?!:;]", said)) + (0 if len(exp) <= 3 else 1))
        pauses = []
        if len(exp_frags) > 1:
            sm = difflib.SequenceMatcher(None, exp, heard, autojunk=False)
            e2h = {}
            for blk in sm.get_matching_blocks():
                for k in range(blk.size):
                    e2h[blk.a + k] = blk.b + k
            idx = 0
            for f in exp_frags[:-1]:
                idx += len(f)
                left = [e2h[i] for i in range(max(0, idx - 3), idx) if i in e2h]
                right = [e2h[i] for i in range(idx, min(len(exp), idx + 3)) if i in e2h]
                if not left or not right:
                    pauses.append(None)
                    continue
                t_from, t_to = toks[left[-1]][2], toks[right[0]][1]
                if t_to < t_from:
                    t_from, t_to = t_to, t_from
                pauses.append(self.find_pause(env, t_from - 0.1, t_to + 0.1))
            if any(a is not None and b is not None and b <= a for a, b in zip(pauses, pauses[1:])):
                pauses = [None] * len(pauses)
        return dict(ratio=round(ratio, 3), ok=bool(ok), pauses=pauses, heard=" ".join(heard), speech=round(sp, 2))

    # ---------------------------------------------------------------- production d'une phrase
    def best_take(self, text, base_seed, max_takes, need_pauses=True, max_speech=None):
        best = None
        for k in range(max_takes):
            wav = self.take(spoken(text), base_seed + k)
            j = self.judge(text, wav)
            full = j["ok"] and (not need_pauses or all(p is not None for p in j["pauses"]))
            if max_speech is not None and j["speech"] > max_speech:
                full = False
                j["ok"] = False
            score = (full, j["ok"], sum(p is not None for p in j["pauses"]), j["ratio"])
            if best is None or score > best[0]:
                best = (score, wav, j, k)
            if full:
                break
        return best

    def produce(self, key, text, settings):
        seed = int(hashlib.sha1(key.encode()).hexdigest()[:6], 16)
        short = len(norm_words(text)) <= 2
        cue = settings == "cue"
        score, wav, j, k = self.best_take(text, seed, MAX_TAKES_SHORT if short else MAX_TAKES,
                                          max_speech=0.80 if cue else None)
        mode = "phrase"
        frs = fragments(text)
        if not score[0] and len(frs) > 1:
            # repli : fragment par fragment (pauses garanties)
            parts, pauses, pos, heard, ok, ratios = [], [], 0, [], True, []
            gap = np.zeros(int(0.2 * self.sr), dtype=np.float32)
            for i, f in enumerate(frs):
                fs = int(hashlib.sha1(f"{key}|{i}".encode()).hexdigest()[:6], 16)
                sc, w, fj, _ = self.best_take(f, fs, MAX_TAKES, need_pauses=False)
                w = self.trim(w)
                if i > 0:
                    pauses.append((pos + len(gap) / 2) / self.sr)
                    parts.append(gap)
                    pos += len(gap)
                parts.append(w)
                pos += len(w)
                heard.append(fj["heard"])
                ok = ok and fj["ok"]
                ratios.append(fj["ratio"])
            wav = np.concatenate(parts)
            j = dict(ratio=min(ratios), ok=ok, pauses=pauses, heard=" / ".join(heard), speech=0)
            mode = "fragments"
        return wav, dict(text=text, said=spoken(text), settings=settings, mode=mode, take=k, **j)

    def trim(self, wav):
        env, hop = self.envelope(wav)
        thr = max(np.percentile(env, 95) * 0.05, 1e-4)
        idx = np.where(env > thr)[0]
        if len(idx) == 0:
            return wav
        a = max(0, idx[0] * hop - int(0.03 * self.sr))
        b = min(len(wav), (idx[-1] + 1) * hop + int(0.08 * self.sr))
        return wav[a:b]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ref", default=os.path.join(HERE, "voice", "reference_siwis.wav"))
    ap.add_argument("--jobs", default=os.path.join(CACHE, "jobs.json"))
    ap.add_argument("--threads", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--redo", nargs="*", default=[], help="clés à refaire (prise suivante)")
    args = ap.parse_args()

    import soundfile as sf
    os.makedirs(RAW, exist_ok=True)
    jobs = json.load(open(args.jobs, encoding="utf-8"))
    todo = [j for j in jobs if j["key"] in args.redo or not (
        os.path.exists(os.path.join(RAW, j["key"] + ".wav")) and os.path.exists(os.path.join(RAW, j["key"] + ".json")))]
    if args.limit:
        todo = todo[: args.limit]
    print(f"{len(jobs)} phrases, {len(todo)} à enregistrer.", flush=True)
    if not todo:
        return
    studio = Studio(args.ref, args.threads)
    t0, done_s = time.time(), 0.0
    for n, job in enumerate(todo):
        key = job["key"]
        k2 = key + ("#redo" if key in args.redo else "")
        wav, meta = studio.produce(k2, job["text"], job["settings"])
        sf.write(os.path.join(RAW, key + ".wav.tmp"), wav, studio.sr, subtype="PCM_16", format="WAV")
        os.replace(os.path.join(RAW, key + ".wav.tmp"), os.path.join(RAW, key + ".wav"))
        meta["sr"] = studio.sr
        json.dump(meta, open(os.path.join(RAW, key + ".json"), "w", encoding="utf-8"), ensure_ascii=False)
        done_s += len(wav) / studio.sr
        el = time.time() - t0
        flag = "" if meta["ok"] and all(p is not None for p in meta["pauses"]) else "  <-- À VÉRIFIER"
        print(f"[{n + 1}/{len(todo)}] {el / 60:5.1f} min, reste ~{el / (n + 1) * (len(todo) - n - 1) / 60:4.0f} min | "
              f"{meta['mode']} prise {meta['take']} r={meta['ratio']} | {job['text'][:60]}{flag}", flush=True)


if __name__ == "__main__":
    sys.exit(main())
