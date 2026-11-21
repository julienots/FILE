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


class Primer(unittest.TestCase):
    def test_primer_is_removed_at_its_gap(self):
        # amorce (0,9 s) · creux 180 ms · phrase (1,6 s)
        x = np.concatenate([burst(900, seed=5), silence(180), burst(1600, seed=6)])
        rest = E.cut_after_primer(x)
        self.assertIsNotNone(rest)
        self.assertAlmostEqual(len(rest) / SR, 1.6, delta=0.12)

    def test_no_gap_in_window_means_no_cut(self):
        self.assertIsNone(E.cut_after_primer(burst(3000)))

    def test_phrase_gap_is_not_mistaken_for_the_primer_gap(self):
        # le creux de l'amorce (≈ 0,9 s) doit l'emporter sur un creux plus tardif de la phrase
        x = np.concatenate([burst(900, seed=5), silence(200), burst(1200, seed=6), silence(150), burst(800, seed=7)])
        rest = E.cut_after_primer(x)
        self.assertAlmostEqual(len(rest) / SR, 1.2 + 0.15 + 0.8, delta=0.15)


class Phonemes(unittest.TestCase):
    def test_english_flagged_words_are_fixed(self):
        # espeak-ng marque « dos » / « rythme » comme anglais ; les balises ne doivent jamais atteindre le modèle
        self.assertTrue(all(v and "(" not in v for v in E.FR_FIX.values()))
        self.assertTrue(E.EN_FLAGGED.search("sˈy lə (en)dˈɒs(fr), si"))
        self.assertEqual(E.EN_FLAGGED.sub(lambda m: E.FR_FIX[m.group(1)], "sˈy lə (en)dˈɒs(fr), si"), "sˈy lə dˈo, si")

    def test_misread_words_get_their_french_pronunciation(self):
        # (mot dans une phrase, prononciation française attendue de ce mot)
        cases = [
            ("Tu te sens léger.", "sˈɑ̃"),             # verbe sentir : /sɑ̃/ et non le nom /sɑ̃s/ (« essence »)
            ("Sens la détente.", "sˈɑ̃"),
            ("Ce que tu ressens est passager.", "ʁəsˈɑ̃"),
            ("Les lucioles deviennent floues.", "flˈu"),
            ("Tes joues se relâchent.", "ʒˈu"),
            ("Tes muscles se dénouent.", "denˈu"),
            ("Le dos droit.", "dˈo"),
            ("Assieds-toi.", "asjˈɛ"),
            ("Tu t'assieds près du poêle.", "tasjˈɛ"),
            ("Un peu de stress.", "stʁˈɛs"),
            ("Pose une main.", "pˈoz"),
            ("Comme on pose un sac.", "pˈoz"),
            ("Il s'éloigne.", "elwˈaɲ"),
            ("Dehors, la neige tombe.", "døˈɔʁ"),
            ("Comme un hamac.", "œ̃n amˈak"),
            ("Des coussins moelleux.", "mwalˈœ"),
            ("Tout est accueilli.", "akøjˈi"),
            ("Pour t'accompagner.", "akɔ̃paɲˈe"),
            ("Ce calme est ta vraie nature.", "ɛt tˈa"),         # le « t » de « ta » tombait : « est à »
            ("C'est ta respiration.", "sɛt tˈa"),
            ("Une lanterne qui éclaire doucement.", "ki, eklˈɛʁ"),  # hiatus « qui éclaire » : « qui déclare »
        ]
        for text, expected in cases:
            ph = E.phonemize_fr(text)
            self.assertIn(expected, ph, f"{text!r} → {ph!r}")
            self.assertNotIn("(", ph)

    def test_no_liaison_before_et(self):
        # « doux et large » ≠ « douze large » ; mais une consonne qui appartient au mot reste (« sel et », « calme et »)
        cases = [("un escalier, doux et large", "dˈu e"), ("Les routes et les villes", "ʁˈut e"),
                 ("Des mots simples et efficaces", "sˈɛ̃pl e"), ("Tu t'allonges dans un lit frais et léger", "fʁˈɛ e"),
                 ("Les vagues vont et viennent", "vˈɔ̃ e"), ("Il sent le sel et la nuit", "sˈɛl e"),
                 ("Un souffle calme et doux", "kˈalm e")]
        for text, expected in cases:
            ph = E.phonemize_fr(text)
            self.assertIn(expected, ph, f"{text!r} → {ph!r}")

    def test_no_liaison_after_third_person_plural_verbs(self):
        # « ils transforment en » ne se lit pas « transformentɑ̃ » (Whisper entendait « dans ») ; les liaisons qui se font restent
        for text, present, absent in [("Les flammes les transforment en chaleur", "tʁɑ̃sfˈɔʁm ɑ̃", "ɔʁmt"),
                                      ("Elles dansent autour de toi", "dˈɑ̃s ", "dˈɑ̃st"),
                                      ("Ils voyagent eux aussi", "vwajˈaʒ øz", "ʒt"),
                                      ("Les étoiles brillent à l'horizon", "bʁˈij a", "bʁˈijt"),
                                      ("Ils sont en route", "sɔ̃t ɑ̃", None),              # « sont » : liaison obligatoire
                                      ("Tout est calme", "tut ɛ", None),
                                      ("Elles sentent en elles la chaleur", "sˈɑ̃t", None)]:   # le « t » fait partie du mot
            ph = E.phonemize_fr(text)
            self.assertIn(present, ph, f"{text!r} → {ph!r}")
            if absent:
                self.assertNotIn(absent, ph, f"{text!r} → {ph!r}")

    def test_final_punctuation_is_stripped_but_not_the_question_mark(self):
        self.assertEqual(E.strip_final("Respire avec moi... inspire sur quatre temps..."), "Respire avec moi... inspire sur quatre temps")
        self.assertEqual(E.strip_final("Ce moment est à toi."), "Ce moment est à toi")
        self.assertEqual(E.strip_final("Bonsoir…"), "Bonsoir")
        self.assertEqual(E.strip_final("Où la sens-tu ?"), "Où la sens-tu ?")        # intonation montante conservée
        self.assertEqual(E.strip_final("Un... deux... trois"), "Un... deux... trois")  # le milieu ne change pas

    def test_displayed_text_is_untouched_and_respelling_is_idempotent(self):
        t = "Sens ton ventre… pose les épaules."
        self.assertEqual(E.respell(E.respell(t)), E.respell(t))
        self.assertTrue(E.respell(t).startswith("San "))      # la majuscule est conservée

    def test_whole_corpus_has_no_english_flag_left(self):
        import build_content as B
        _, _, units = B.inventory()
        E.UNMAPPED.clear()
        for u in units:
            ph = E.phonemize_fr(u.tts)
            self.assertNotIn("(", ph, u.ref)
            self.assertNotIn(")", ph, u.ref)
        self.assertEqual(E.UNMAPPED, set(), "mot balisé anglais inconnu : ajouter son entrée à FR_FIX")


if __name__ == "__main__":
    unittest.main(verbosity=2)
