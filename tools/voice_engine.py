#!/usr/bin/env python3
"""
NOCTÉA — moteurs de synthèse vocale pour la production (Superessence Studio).

Une seule voix pour toute l'application. Moteur par défaut : Kokoro-82M (Apache-2.0), voix
française « ff_siwis » (entraînée sur le corpus SIWIS, CC-BY 4.0), exécuté en ONNX sur CPU.
Pourquoi lui plutôt que le Piper d'origine :
  * déterministe : le même texte donne exactement la même voix (pas de tirage aléatoire par clip) ;
  * mesuré sur 220 fragments réels : dispersion de hauteur entre clips 0,3–0,6 dem-ton contre 1,1–1,6
    pour Piper, dispersion de timbre (centroïde, pente spectrale) divisée par 2 à 3 ;
  * 24 kHz natif, rendu plus net.
Piper reste disponible (--engine piper) pour reproduire l'ancienne voix.

Toutes les sorties sont en float32 mono à SR = 24 000 Hz.
"""
import os
import re

import numpy as np

SR = 24000

# espeak-ng bascule vers l'anglais sur quelques mots français (« dos », « rythme », « assieds »…) et
# les balise « (en)…(fr) ». Laissées telles quelles, les balises sont prononcées comme des syllabes
# parasites (« assieds-toi » → « iain recidze fre toi »). On les retire et on remplace la prononciation
# anglaise par la française. Tout mot balisé inconnu est signalé au lancement de la production.
FR_FIX = {
    "ɹˈɪθm": "ʁˈitm",       # rythme
    "dˈɒs": "dˈo",          # dos
    "ɐsˈɪdz": "asjˈɛ",      # assieds
    "stɹˈɛs": "stʁˈɛs",     # stress
    "mˈeɪl": "mˈɛl",        # mail
}
EN_FLAGGED = re.compile(r"\(en[a-z-]*\)(.*?)(?:\(fr[a-z-]*\)|$)")
ANY_FLAG = re.compile(r"\([a-z]{2,3}(?:-[a-z0-9]+)*\)")
UNMAPPED = set()


def _cache_dir():
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), ".cache")


class KokoroEngine:
    name = "kokoro"
    label = "Siwis · Kokoro-82M"
    # Allure de la narration (1,0 = débit neutre du modèle). 0,70 ≈ 150 mots/min de parole active :
    # les silences font le reste du ralentissement.
    default_speed = 0.70

    def __init__(self, model_dir, voice="ff_siwis", lang="fr-fr", threads=0):
        from kokoro_onnx import Kokoro
        import onnxruntime as rt
        voices_npz = os.path.join(model_dir, "voices.npz")
        if not os.path.exists(voices_npz):
            raw = np.fromfile(os.path.join(model_dir, voice + ".bin"), dtype=np.float32).reshape(-1, 1, 256)
            np.savez(voices_npz, **{voice: raw})
        opts = rt.SessionOptions()
        if threads:
            opts.intra_op_num_threads = int(threads)
            opts.inter_op_num_threads = 1
        sess = rt.InferenceSession(os.path.join(model_dir, "model.onnx"), sess_options=opts,
                                   providers=["CPUExecutionProvider"])
        self.k = Kokoro.from_session(sess, voices_npz)
        self.voice_name = voice
        self.voice = np.asarray(self.k.voices[voice], dtype=np.float32)
        self.lang = lang

    def phonemize(self, text):
        """Texte → phonèmes (espeak-ng « fr-fr »), balises de langue retirées et mots anglais corrigés."""
        ph = self.k.tokenizer.phonemize(text, self.lang)

        def fix(m):
            chunk = m.group(1)
            if chunk in FR_FIX:
                return FR_FIX[chunk]
            UNMAPPED.add((chunk, text))
            return chunk

        ph = EN_FLAGGED.sub(fix, ph)
        return " ".join(ANY_FLAG.sub("", ph).split())

    def phoneme_weights(self, fragments):
        """Poids de durée de chaque fragment = nombre de phonèmes prononcés (espeak), bien plus fidèle que les lettres."""
        out = []
        for f in fragments:
            ph = self.phonemize(f)
            out.append(sum(1 for ch in ph if ch not in " .,;:!?…-—ˈˌ") or 1)
        return out

    def render(self, text, speed=None, shift=0):
        """Rend `text` en une seule passe. `shift` décale la ligne de style (variante de secours)."""
        voice = self.voice if shift == 0 else np.roll(self.voice, -int(shift), axis=0)
        audio, sr = self.k.create(
            self.phonemize(text), voice, speed=float(speed or self.default_speed), lang=self.lang,
            is_phonemes=True, sentence_pause=0.0, clause_pause=0.0,
        )
        assert sr == SR, sr
        return np.asarray(audio, dtype=np.float32)


class PiperEngine:
    """Ancienne voix (Piper « siwis », VITS) — conservée pour comparaison."""
    name = "piper"
    label = "Siwis · Piper"
    default_speed = 1.24  # length_scale

    def __init__(self, model_path, **_):
        from piper import PiperVoice
        self.v = PiperVoice.load(model_path)

    def render(self, text, speed=None, shift=0):
        from piper import SynthesisConfig
        import scipy.signal as ss
        cfg = SynthesisConfig(length_scale=float(speed or self.default_speed), noise_scale=0.55, noise_w_scale=0.78)
        ch = [c.audio_int16_array.astype(np.float32) / 32768.0 for c in self.v.synthesize(text, syn_config=cfg)]
        x = np.concatenate(ch) if ch else np.zeros(SR // 2, dtype=np.float32)
        return ss.resample_poly(x, SR, 22050).astype(np.float32)


def make_engine(kind, model, threads=0):
    if kind == "kokoro":
        return KokoroEngine(model, threads=threads)
    if kind == "piper":
        return PiperEngine(model)
    raise ValueError(kind)


# --------------------------------------------------------------------------- découpe aux silences
def frame_db(x, win_ms=5):
    win = int(win_ms / 1000 * SR)
    n = len(x) // win
    if n == 0:
        return np.array([-90.0]), win
    f = x[: n * win].reshape(n, win)
    return 20 * np.log10(np.sqrt(np.mean(f ** 2, axis=1)) + 1e-9), win


def speech_region(x, thr_db=-38.0):
    """(début, fin) en échantillons de la zone parlée (au-dessus de thr sous le niveau 90 %)."""
    e, win = frame_db(x)
    ref = np.percentile(e, 90)
    idx = np.where(e > ref + thr_db)[0]
    if len(idx) == 0:
        return 0, len(x)
    return int(idx[0] * win), int((idx[-1] + 1) * win)


def find_gaps(x, thr_db=-30.0, min_ms=70):
    """Creux de parole à l'intérieur de la phrase : liste de (début, fin) en échantillons."""
    e, win = frame_db(x)
    ref = np.percentile(e, 90)
    low = e < ref + thr_db
    idx = np.where(~low)[0]
    if len(idx) < 2:
        return []
    a, b = idx[0], idx[-1]
    gaps, i = [], a
    while i <= b:
        if low[i]:
            j = i
            while j <= b and low[j]:
                j += 1
            if (j - i) * 5 >= min_ms:
                gaps.append((i * win, j * win))
            i = j
        else:
            i += 1
    return gaps


def cut_at_pauses(x, fragments, keep_ms=45, weights=None):
    """
    Coupe une phrase rendue d'un seul tenant en ses fragments (aux « ... » ou « . »).
    `weights` : poids de durée attendus de chaque fragment (nombre de phonèmes ; à défaut, lettres).
    Retourne la liste des morceaux audio, ou None si les creux ne sont pas fiables
    (l'appelant rend alors chaque fragment séparément).
    """
    import itertools
    n = len(fragments)
    a, b = speech_region(x)
    if n == 1:
        return [x[a:b]]
    w = [float(max(1, v)) for v in (weights or [sum(c.isalnum() for c in f) for f in fragments])]
    cum = np.cumsum(w)[:-1] / float(sum(w))
    expected = [a + c * (b - a) for c in cum]
    gaps = []
    for thr, mn in ((-30.0, 70), (-26.0, 55), (-22.0, 40)):     # seuils de plus en plus tolérants
        gaps = [g for g in find_gaps(x, thr, mn) if a < g[0] and g[1] < b]
        if len(gaps) >= n - 1:
            break
    if len(gaps) < n - 1:
        return None
    centers = [(g[0] + g[1]) / 2 for g in gaps]
    lens = [(g[1] - g[0]) / SR * 1000 for g in gaps]
    # le premier creux au-delà de 10 % de la parole ne peut pas être le début ; coût = écart de position + creux trop court
    def cost(sel):
        total = 0.0
        for k, gi in enumerate(sel):
            total += abs(centers[gi] - expected[k]) / SR / 0.45 + 1.5 * max(0.0, 1 - lens[gi] / 180.0)
        return total

    best, best_c = None, 1e9
    for sel in itertools.combinations(range(len(gaps)), n - 1):
        c = cost(sel)
        if c < best_c:
            best, best_c = sel, c
    if best is None or best_c > 1.0 * (n - 1) + 0.5:
        return None
    keep = int(keep_ms / 1000 * SR)
    bounds = [a]
    for gi in best:
        g0, g1 = gaps[gi]
        mid = (g0 + g1) // 2
        bounds += [min(g0 + keep, mid), max(g1 - keep, mid)]
    bounds.append(b)
    pieces = [x[bounds[2 * k]:bounds[2 * k + 1]] for k in range(n)]
    # vraisemblance : durée de chaque morceau ~ proportionnelle à son poids
    total = float(sum(len(p) for p in pieces))
    for p, wi in zip(pieces, w):
        share = len(p) / total
        exp = wi / float(sum(w))
        lim = (0.2, 4.5) if wi <= 5 else (0.4, 2.6)
        if len(p) < 0.10 * SR or not (lim[0] < share / exp < lim[1]):
            return None
    return pieces
