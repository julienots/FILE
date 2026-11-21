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
    "tˈiːˈasɪdz": "tasjˈɛ",  # t'assieds
    "ˈɛsˈasɪdz": "sasjˈɛ",   # s'assieds
    "ˈɛmˈasɪdz": "masjˈɛ",   # m'assieds
    "stɹˈɛs": "stʁˈɛs",     # stress
    "mˈeɪl": "mˈɛl",        # mail
}
EN_FLAGGED = re.compile(r"\(en[a-z-]*\)(.*?)(?:\(fr[a-z-]*\)|$)")
ANY_FLAG = re.compile(r"\([a-z]{2,3}(?:-[a-z0-9]+)*\)")
UNMAPPED = set()

# espeak-ng se trompe sur quelques mots : on envoie au moteur une orthographe de remplacement dont la
# phonétisation donne la bonne prononciation (le texte affiché ne change pas). Chaque entrée est vérifiée par
# tools/test_voice_tools.py : la phonétisation du remplaçant doit être exactement la prononciation voulue.
RESPELL = {
    "sens": "san",               # verbe sentir (« sens la chaleur », « tu te sens ») = /sɑ̃/ ; espeak lisait le nom /sɑ̃s/ → « essence »
    "ressens": "ressan",         # /ʁəsɑ̃/ (espeak : /ʁəsɛn/) ; sans s final : pas de fausse liaison
    "floues": "floue",           # /flu/ (espeak : /flw/)
    "joues": "joue",             # /ʒu/ (espeak : /ʒw/)
    "dénouent": "dénoue",        # /denu/ (espeak : /denw/)
    "apesanteur": "apeuzanteur",  # /apəzɑ̃tœʁ/ (espeak oubliait la voyelle : « absenteur »)
    "lucioles": "luciolles",     # /lysjɔl/
    "cosmos": "cosmoss",         # /kɔsmɔs/ : le s final se prononce
    "rose": "rauze", "chose": "chauze", "pose": "pauze", "repose": "repauze", "dépose": "dépauze",   # o fermé : /ʁoz/, /ʃoz/…
    "montagnes": "montagne", "éloignent": "éloigne", "éteignent": "éteigne",                       # « gn » = /ɲ/
    "hamac": "amac",             # h muet : « un hamac » se lie, /œ̃n amak/ (sans liaison : « zamak »)
    "dehors": "deuhors",         # /dəɔʁ/ : la voyelle de liaison rend le mot net (sinon « d'or », « zor »)
}
_RESPELL_RE = re.compile(r"(?<![\wÀ-ÿ])(" + "|".join(re.escape(k) for k in sorted(RESPELL, key=len, reverse=True)) + r")(?![\wÀ-ÿ])", re.I)

# corrections directes sur la phonétique, quand aucune orthographe de remplacement ne convient
IPA_FIX = {
    "mwalˈø": "mwalˈœ",          # moelleux : voyelle finale plus ouverte, sinon « moelleurs », « moelus »
    "akœjˈi": "akøjˈi",          # accueilli, accueillir : sinon « accri »
    "akœjˈɑ̃": "akøjˈɑ̃",          # accueillant : sinon « accrayant »
    "ɛnɛʒˈe": "ɑ̃nɛʒˈe",          # enneigés : « en- » est nasal
    "elwanjˈe": "elwaɲˈe",       # éloigner, éloigné : « gn » = /ɲ/
    "akɔ̃panjˈe": "akɔ̃paɲˈe",    # accompagner
    "bɛnjˈe": "bɛɲˈe",           # baigné
    "kɔ̃panˈi": "kɔ̃paɲˈi",       # compagnie
    "ki eklˈɛʁ": "ki, eklˈɛʁ",   # « qui éclaire » : l'hiatus i-é se fond, Whisper entend « qui déclare » (0/9 → 6/9 avec une micro-pause)
}

# « est ta » : le « t » de « ta » tombe après « est » (/ɛ ta/ → /ɛ a/, entendu « à ») ; le doubler le rend net (0/9 → 7/9)
_EST_TA = re.compile(r"(?<![^\s])(s?ɛ) tˈ?a ")


def respell(text):
    """Texte → texte envoyé au moteur (orthographes de remplacement ; idempotent)."""
    def sub(m):
        w = m.group(1)
        r = RESPELL[w.lower()]
        return r[0].upper() + r[1:] if w[0].isupper() else r
    return _RESPELL_RE.sub(sub, text)


# La ponctuation finale d'un énoncé fait ajouter au moteur un petit son parasite après le dernier mot (« fois » →
# « foisse », « poings » → « poingues », « temps » → « tangles »). Mesuré sur 33 phrases (Whisper medium, hors
# homophones) : mots finaux en écart 19 avec le point ou « ... », 9 avec une virgule, 3 sans ponctuation.
# Le point d'interrogation est conservé (il porte l'intonation montante).
_FINAL_PUNCT = re.compile(r"\s*(?:\.\.\.|…|[.,;:])+\s*$")


def strip_final(text):
    return _FINAL_PUNCT.sub("", text)


_TOKENIZER = None
_STRESS = re.compile(r"[ˈˌː]")


_VOWEL_CHARS = set("aeiouyɑɛɔøœəɐ̃")        # voyelles (et tilde de nasalisation) du jeu de phonèmes espeak-ng


def _no_forbidden_liaison(text, ph, lang):
    """
    Retire deux liaisons que le français parlé ne fait pas et qu'espeak-ng insère pourtant :
      · devant « et » : « doux et large » ≠ « douze large » (routes et → ʁutz e, frais et → fʁɛz e, vont et → vɔ̃t e…) ;
      · après un verbe en « -ent » (3e personne du pluriel) : « transforment en » devenait « transformentɑ̃ », entendu « dans ».
        Les mots en « -ent » qui se terminent par une voyelle nasale (souvent, moment…) gardent leur liaison.
    On retire la consonne de liaison quand le mot, dit seul, ne la porte pas.
    Alignement mot du texte ↔ mot phonétique (même nombre dans 869 phrases sur 870 du corpus ; sinon on ne touche à rien).
    """
    words, toks = text.split(" "), ph.split(" ")
    if len(words) != len(toks):
        return ph
    for i in range(len(words) - 1):
        if re.search(r"[^\wÀ-ÿœŒ'’-]$", words[i]):
            continue                                        # ponctuation entre les deux mots : pas de liaison
        core = _STRESS.sub("", toks[i])
        if not core:
            continue
        before_et = re.sub(r"[^\wÀ-ÿœŒ]", "", words[i + 1]).lower() == "et" and core[-1] in "ztn"
        verb_ent = (re.search(r"\w{3,}ent$", words[i], re.I) is not None and core[-1] == "t"
                    and len(core) > 1 and core[-2] not in _VOWEL_CHARS)
        if not (before_et or verb_ent):
            continue
        iso = _STRESS.sub("", ANY_FLAG.sub("", _TOKENIZER.phonemize(words[i], lang))).strip().split(" ")[-1]
        if not iso.endswith(core[-1]):
            toks[i] = toks[i][:-1]
    return " ".join(toks)


def phonemize_fr(text, lang="fr-fr"):
    """
    Texte → phonèmes (espeak-ng « fr-fr », sans modèle) : orthographes corrigées, balises de langue retirées
    (mots anglais remplacés par leur prononciation française), corrections de phonétique.
    """
    global _TOKENIZER
    if _TOKENIZER is None:
        from kokoro_onnx.tokenizer import Tokenizer
        _TOKENIZER = Tokenizer()
    text = " ".join(respell(text).split())
    ph = _TOKENIZER.phonemize(text, lang)

    def fix(m):
        chunk = m.group(1)
        if chunk in FR_FIX:
            return FR_FIX[chunk]
        UNMAPPED.add((chunk, text))
        return chunk

    ph = EN_FLAGGED.sub(fix, ph)
    ph = ANY_FLAG.sub("", ph)
    ph = " ".join(ph.split())
    ph = _no_forbidden_liaison(text, ph, lang)
    for wrong, right in IPA_FIX.items():
        ph = ph.replace(wrong, right)
    return _EST_TA.sub(r"\1t tˈa ", ph)


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
        return phonemize_fr(text, self.lang)

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


# --------------------------------------------------------------------------- amorce de contexte
# Mesuré sur 18 phrases difficiles (Whisper medium, écart avec le texte voulu) : rendues seules, 49 % d'écart
# (syllabes parasites au début : « Eta respiration », « Utilisez déconnecté », « et d'expirer ») ; précédées de
# l'amorce « Tout doucement... » puis coupées, 4 %. Le moteur traite alors la phrase comme la suite d'un discours.
PRIME = "Tout doucement..."


def cut_after_primer(x, lo_s=0.62, hi_s=1.55):
    """
    Retire l'amorce rendue en tête de x. Retourne l'audio qui suit le creux entre l'amorce et la phrase
    (jusqu'à la fin de la parole), ou None si ce creux est introuvable.
    """
    a, b = speech_region(x)
    cand = []
    for thr, mn in ((-30.0, 100), (-26.0, 70), (-22.0, 50)):          # seuils de plus en plus tolérants
        cand = [g for g in find_gaps(x, thr, mn)
                if lo_s * SR <= g[0] - a <= hi_s * SR and g[1] < b - 0.15 * SR]
        if cand:
            break
    if not cand:
        return None
    # le creux de l'amorce : le plus long de la fenêtre, à longueur voisine le plus proche de 0,95 s
    g = max(cand, key=lambda g: (g[1] - g[0]) / SR * 1000 - 40 * abs((g[0] - a) / SR - 0.95))
    keep = int(0.045 * SR)
    start = max(g[1] - keep, (g[0] + g[1]) // 2)
    return x[start:b]
