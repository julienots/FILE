#!/usr/bin/env python3
"""
Tests de la chaîne vocale (sans modèle : signaux synthétiques) — python3 tools/test_voice_tools.py
Vérifie les garanties qui font « une seule voix » : niveau commun, queue éteinte proprement, coupes aux
silences, timbre identique d'un clip à l'autre.
"""
import os
import sys
import unittest

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import voice_engine as E  # noqa: E402
import voice_qa as Q  # noqa: E402
import voice_studio as S  # noqa: E402

SR = E.SR


def burst(ms, f0=200.0, level=0.2, seed=1):
    """Syllabes voisées synthétiques (harmoniques de f0, enveloppe lente)."""
    rng = np.random.RandomState(seed)
    t = np.arange(int(ms / 1000 * SR)) / SR
    x = sum(a * np.sin(2 * np.pi * f0 * k * t) for k, a in ((1, 1.0), (2, 0.5), (3, 0.3), (5, 0.12)))
    x = x * (0.6 + 0.4 * np.sin(2 * np.pi * 3.5 * t) ** 2) + 0.01 * rng.randn(len(t))
    return (x / np.max(np.abs(x)) * level).astype(np.float32)


def silence(ms):
    return np.zeros(int(ms / 1000 * SR), dtype=np.float32)


class StudioChain(unittest.TestCase):
    def test_every_clip_gets_the_same_level_whatever_the_input_level(self):
        levels = []
        for lvl in (0.02, 0.2, 0.9):
            y = S.process(burst(1500, level=lvl))
            levels.append(Q.active_level_db(y, SR))
        self.assertLess(max(levels) - min(levels), 0.4)
        self.assertAlmostEqual(levels[1], S.TARGET_LEVEL_DB, delta=0.6)

    def test_no_clipping_and_clean_edges(self):
        y = S.process(burst(1200, level=0.95))
        self.assertLess(float(np.max(np.abs(y))), 0.97)
        self.assertLess(abs(float(y[0])), 1e-3)
        self.assertLess(abs(float(y[-1])), 1e-3)
        self.assertTrue(np.all(np.isfinite(y)))

    def test_room_tail_is_kept_but_never_cuts_the_speech(self):
        x = burst(1000)
        y = S.process(x)
        self.assertGreater(len(y), len(x))                 # queue de pièce conservée
        self.assertLess(len(y), len(x) + int(0.6 * SR))    # et bornée
        # la parole n'est pas atteinte par le fondu final : l'énergie reste stable jusqu'à la fin de x
        a, b = int(0.7 * len(x)), int(0.9 * len(x))
        self.assertGreater(float(np.sqrt(np.mean(y[a:b] ** 2))), 0.02)

    def test_same_timbre_for_clips_of_different_pitch_content(self):
        # deux clips du même « locuteur » à des hauteurs légèrement différentes : même pente spectrale ± 1 dB/oct
        slopes = []
        for f0 in (180.0, 205.0, 230.0):
            y = S.process(burst(1500, f0=f0))
            slopes.append(Q.spectral_features(y, SR)[1])
        self.assertLess(max(slopes) - min(slopes), 2.0)


class Cutting(unittest.TestCase):
    def test_cut_at_clear_pauses(self):
        x = np.concatenate([burst(900), silence(300), burst(1700, seed=2), silence(280), burst(600, seed=3)])
        pieces = E.cut_at_pauses(x, ["Un deux trois...", "quatre cinq six sept huit...", "neuf."], weights=[9, 17, 5])
        self.assertIsNotNone(pieces)
        self.assertEqual(len(pieces), 3)
        self.assertAlmostEqual(len(pieces[0]) / SR, 0.9, delta=0.12)
        self.assertAlmostEqual(len(pieces[1]) / SR, 1.7, delta=0.12)
        self.assertAlmostEqual(len(pieces[2]) / SR, 0.6, delta=0.12)

    def test_refuses_when_pauses_are_missing(self):
        x = burst(3000)
        self.assertIsNone(E.cut_at_pauses(x, ["a b c", "d e f"], weights=[5, 5]))

    def test_single_fragment_is_just_trimmed(self):
        x = np.concatenate([silence(200), burst(800), silence(300)])
        pieces = E.cut_at_pauses(x, ["bonsoir."])
        self.assertEqual(len(pieces), 1)
        self.assertAlmostEqual(len(pieces[0]) / SR, 0.8, delta=0.1)


class Phonemes(unittest.TestCase):
    def test_english_flagged_words_are_fixed(self):
        # espeak-ng marque « dos » / « rythme » comme anglais ; les balises ne doivent jamais atteindre le modèle
        self.assertTrue(all(v and "(" not in v for v in E.FR_FIX.values()))
        self.assertTrue(E.EN_FLAGGED.search("sˈy lə (en)dˈɒs(fr), si"))
        self.assertEqual(E.EN_FLAGGED.sub(lambda m: E.FR_FIX[m.group(1)], "sˈy lə (en)dˈɒs(fr), si"), "sˈy lə dˈo, si")


if __name__ == "__main__":
    unittest.main(verbosity=2)
