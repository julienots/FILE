#!/usr/bin/env python3
"""
NOCTÉA — chaîne « studio » de la voix (Superessence Studio).

Appliquée à CHAQUE clip de la même façon, pour que tous les clips partagent le même timbre, le même
niveau et la même pièce :

  1. niveau d'entrée commun (parole active)
  2. filtre passe-haut 70 Hz, chaleur de proximité (+1,6 dB vers 190 Hz), présence adoucie (−1 dB vers
     3 kHz), aigus ramenés au niveau d'une voix naturelle : le brut du modèle est ≈ 5 dB plus brillant
     qu'une parole réelle (énergie 5–10 kHz / 0,1–5 kHz : −14 dB mesuré, −20 dB attendu) ; la chaîne
     la ramène à ≈ −19,5 dB (l'ancienne voix, étouffée, était à −23 dB)
  3. de-esser par bande (5,2–10,5 kHz), compression douce (2:1, constantes lentes)
  4. pièce : réponse impulsionnelle synthétique FIXE (premières réflexions + queue diffuse,
     RT60 ≈ 0,30 s) au lieu des deux échos à 23/37 ms de l'ancienne chaîne, qui coloraient la voix
     comme un filtre en peigne
  5. niveau de sortie commun (parole active, −20,5 dBFS), limiteur à genou doux (pas de tanh global)
  6. fondus d'entrée/sortie, queue de réverbération conservée jusqu'à son extinction naturelle
"""
import math

import numpy as np
import scipy.signal as ss

SR = 24000
TARGET_LEVEL_DB = -20.5      # niveau de parole active en sortie (≈ ancien niveau, pour garder l'équilibre voix/ambiance)
PRE_LEVEL_DB = -20.0
CHAIN_VERSION = "v4b"


# --------------------------------------------------------------------------- niveau
def active_level_db(x, gate_db=-45.0):
    win = int(0.02 * SR)
    n = len(x) // win
    if n == 0:
        return -90.0
    f = x[: n * win].reshape(n, win)
    rms = np.sqrt(np.mean(f ** 2, axis=1) + 1e-12)
    act = rms[rms > np.max(rms) * 10 ** (gate_db / 20)]
    act = act[act > 10 ** (-60 / 20)]
    if len(act) == 0:
        return -90.0
    return 20 * math.log10(np.sqrt(np.mean(act ** 2)) + 1e-12)


def set_level(x, target_db):
    cur = active_level_db(x)
    return (x * 10 ** ((target_db - cur) / 20)).astype(np.float32)


def soft_limit(x, thr=0.72):
    """Genou doux : ne touche pas ce qui est sous thr, écrase progressivement au-dessus."""
    ax = np.abs(x)
    over = ax > thr
    if not np.any(over):
        return x
    y = x.copy()
    k = 1.0 - thr
    y[over] = np.sign(x[over]) * (thr + k * np.tanh((ax[over] - thr) / k))
    return y


# --------------------------------------------------------------------------- égalisation (RBJ)
def _biquad(kind, f0, gain_db, q):
    a = 10 ** (gain_db / 40)
    w0 = 2 * math.pi * f0 / SR
    cw, sw = math.cos(w0), math.sin(w0)
    alpha = sw / (2 * q)
    if kind == "peak":
        b = [1 + alpha * a, -2 * cw, 1 - alpha * a]
        den = [1 + alpha / a, -2 * cw, 1 - alpha / a]
    elif kind == "lowshelf":
        sq = 2 * math.sqrt(a) * alpha
        b = [a * ((a + 1) - (a - 1) * cw + sq), 2 * a * ((a - 1) - (a + 1) * cw), a * ((a + 1) - (a - 1) * cw - sq)]
        den = [(a + 1) + (a - 1) * cw + sq, -2 * ((a - 1) + (a + 1) * cw), (a + 1) + (a - 1) * cw - sq]
    elif kind == "highshelf":
        sq = 2 * math.sqrt(a) * alpha
        b = [a * ((a + 1) + (a - 1) * cw + sq), -2 * a * ((a - 1) + (a + 1) * cw), a * ((a + 1) + (a - 1) * cw - sq)]
        den = [(a + 1) - (a - 1) * cw + sq, 2 * ((a - 1) - (a + 1) * cw), (a + 1) - (a - 1) * cw - sq]
    else:
        raise ValueError(kind)
    b = np.array(b) / den[0]
    den = np.array(den) / den[0]
    return np.concatenate([b, den])


_EQ_SOS = np.vstack([
    ss.butter(2, 70, "highpass", fs=SR, output="sos"),
    _biquad("lowshelf", 190, 1.6, 0.7),
    _biquad("peak", 3000, -1.0, 1.0),
    _biquad("highshelf", 6000, -4.0, 0.7),
    _biquad("peak", 7500, -3.0, 1.2),
])


def tone(x):
    return ss.sosfilt(_EQ_SOS, x).astype(np.float32)


# --------------------------------------------------------------------------- de-esser
_SIB_SOS = ss.butter(4, [5200, 10500], "bandpass", fs=SR, output="sos")


def deess(x, thresh_db=-34.0, ratio=4.0):
    band = ss.sosfilt(_SIB_SOS, x)
    a = 1 - math.exp(-1 / (0.006 * SR))
    env = ss.lfilter([a], [1, -(1 - a)], np.abs(band) * 1.5708)  # ~ RMS d'une sinusoïde
    env_db = 20 * np.log10(env + 1e-9)
    over = np.maximum(env_db - thresh_db, 0.0)
    gain = 10 ** (-(over * (1 - 1 / ratio)) / 20)
    # lisse le gain (évite les craquements)
    gain = ss.lfilter([0.05], [1, -0.95], gain, zi=[0.95 * gain[0]])[0] if len(gain) else gain
    return (x - band + band * gain).astype(np.float32)


# --------------------------------------------------------------------------- compression douce
def compress(x, thresh_db=-27.0, ratio=2.0, attack_ms=18.0, release_ms=240.0):
    hop = int(0.004 * SR)
    n = len(x) // hop
    if n < 2:
        return x
    blocks = x[: n * hop].reshape(n, hop)
    lvl = 10 * np.log10(np.mean(blocks ** 2, axis=1) + 1e-12)          # dB RMS par bloc de 4 ms
    target = -np.maximum(lvl - thresh_db, 0) * (1 - 1 / ratio)         # réduction de gain souhaitée (≤ 0)
    ka = math.exp(-hop / (attack_ms / 1000 * SR))
    kr = math.exp(-hop / (release_ms / 1000 * SR))
    g = np.zeros(n)
    cur = 0.0
    for i in range(n):
        t = target[i]
        k = ka if t < cur else kr            # t plus négatif = on comprime plus : attaque
        cur = k * cur + (1 - k) * t
        g[i] = cur
    gain = np.interp(np.arange(len(x)), (np.arange(n) + 0.5) * hop, 10 ** (g / 20))
    return (x * gain).astype(np.float32)


# --------------------------------------------------------------------------- pièce (réverbération)
def _make_ir():
    rng = np.random.RandomState(20241120)          # IR figée : même pièce pour tous les clips
    n = int(0.50 * SR)
    t = np.arange(n) / SR
    tail = rng.randn(n) * np.exp(-t / (0.30 / 6.9))
    # amortissement des aigus dans la queue (une pièce absorbe les aigus plus vite)
    sos = ss.butter(1, 3800, "lowpass", fs=SR, output="sos")
    tail = ss.sosfilt(sos, tail)
    tail[: int(0.012 * SR)] *= np.linspace(0, 1, int(0.012 * SR))
    ir = tail * 0.42
    for ms, amp in [(5.5, 0.55), (9.0, -0.38), (13.5, 0.30), (19.0, -0.24), (26.0, 0.18), (34.0, -0.12)]:
        ir[int(ms / 1000 * SR)] += amp
    # normalise l'énergie du chemin réverbéré à 1 pour régler précisément le mélange sec/humide
    ir = ir / math.sqrt(np.sum(ir ** 2))
    return ir.astype(np.float32)


_IR = _make_ir()
WET_DB = -20.0          # niveau humide par rapport au signal sec (énergie)
TAIL_MS = 420


def room(x):
    pad = np.concatenate([x, np.zeros(int(TAIL_MS / 1000 * SR), dtype=np.float32)])
    wet = ss.fftconvolve(pad, _IR)[: len(pad)]
    e_dry = math.sqrt(np.mean(x ** 2) + 1e-12)
    e_wet = math.sqrt(np.mean(wet[: len(x)] ** 2) + 1e-12)
    wet = wet * (e_dry / e_wet) * 10 ** (WET_DB / 20)
    # pré-retard de 8 ms
    d = int(0.008 * SR)
    wet = np.concatenate([np.zeros(d, dtype=np.float32), wet[:-d]])
    return (pad + wet).astype(np.float32)


# --------------------------------------------------------------------------- hauteur
def median_f0(x):
    import parselmouth
    snd = parselmouth.Sound(x.astype(np.float64), sampling_frequency=SR)
    f0 = snd.to_pitch_ac(time_step=0.01, pitch_floor=75, pitch_ceiling=400).selected_array["frequency"]
    f0 = f0[f0 > 0]
    return float(np.median(f0)) if len(f0) >= 5 else None


def match_pitch(x, f0_target, max_st=2.0, min_st=0.15):
    """Ramène la hauteur médiane de x vers f0_target (PSOLA : les formants, donc le timbre, ne bougent pas)."""
    import parselmouth
    from parselmouth.praat import call
    f0 = median_f0(x)
    if not f0:
        return x
    st = max(-max_st, min(max_st, 12 * math.log2(f0_target / f0)))
    if abs(st) < min_st:
        return x
    snd = parselmouth.Sound(x.astype(np.float64), sampling_frequency=SR)
    manip = call(snd, "To Manipulation", 0.01, 75, 400)
    tier = call(manip, "Extract pitch tier")
    call(tier, "Multiply frequencies", snd.xmin, snd.xmax, 2 ** (st / 12))
    call([tier, manip], "Replace pitch tier")
    out = np.asarray(call(manip, "Get resynthesis (overlap-add)").values[0], dtype=np.float32)
    return out


# --------------------------------------------------------------------------- chaîne complète
def edges(x, fade_in_ms=7.0, tail_fade_ms=170.0, speech_end=None):
    """Fondu d'entrée, extinction de la queue (la réverbération se termine naturellement)."""
    y = x.copy()
    n_in = int(fade_in_ms / 1000 * SR)
    y[:n_in] *= np.sin(np.linspace(0, math.pi / 2, n_in)) ** 2
    # La queue de réverbération est coupée quand elle passe sous −75 dBFS (inaudible), puis le fondu
    # cosinus ne s'applique QU'À la queue (jamais sur la parole elle-même).
    if speech_end is None:
        speech_end = len(y) - int(TAIL_MS / 1000 * SR)
    seg = np.abs(y[speech_end:])
    win = int(0.01 * SR)
    end = len(y)
    if len(seg) > win:
        env = np.sqrt(np.convolve(seg ** 2, np.ones(win) / win, mode="same"))
        idx = np.where(env > 10 ** (-75 / 20))[0]
        end = speech_end + (int(idx[-1]) + win if len(idx) else win)
        end = min(end, len(y))
    end = max(end, speech_end + int(0.06 * SR))
    end = min(end, len(y))
    y = y[:end]
    n_out = int(min(tail_fade_ms / 1000 * SR, (end - speech_end) + 0.02 * SR))
    n_out = max(n_out, int(0.03 * SR))
    n_out = min(n_out, len(y))
    y[-n_out:] *= np.cos(np.linspace(0, math.pi / 2, n_out)) ** 2
    return y


def process(x):
    """x : fragment sec (float32, 24 kHz, déjà découpé sur la zone parlée) → clip final."""
    speech_len = len(x)
    y = set_level(x, PRE_LEVEL_DB)
    y = tone(y)
    y = deess(y)
    y = compress(y)
    y = room(y)
    y = set_level(y, TARGET_LEVEL_DB)
    y = soft_limit(y)
    y = edges(y, speech_end=speech_len)
    return np.clip(y, -1, 1).astype(np.float32)


def fx_chain(x):
    """Bruitages (respiration) : même timbre de pièce que la voix, sans compression ni niveau parole."""
    y = tone(x.astype(np.float32))
    y = room(y)
    return edges(y, fade_in_ms=12.0, speech_end=len(x))
