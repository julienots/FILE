#!/usr/bin/env python3
"""
NOCTÉA — vérification fine de la prononciation (Superessence Studio).

Le contrôle de build_content.py utilise Whisper « small » (rapide). Celui-ci repasse les rendus retenus avec un
modèle plus fiable (« medium » par défaut) et rapporte, mot par mot, ce qui diffère du texte voulu. Il sert à
trouver les mots réellement mal dits : un écart n'a de sens que s'il se répète d'un modèle à l'autre et d'une
phrase à l'autre, et si les deux mots ne sont pas homophones (prend/prends, silencieuse/silencieuses…).

Usage :
  python3 tools/voice_verify.py --min-wer 0.10 --out tools/.cache/verify.json     # phrases suspectes du contrôle rapide
  python3 tools/voice_verify.py --all --model large-v3                            # tout, avec un modèle plus fin
"""
import argparse
import difflib
import json
import math
import os
import re
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "content"))
sys.path.insert(0, HERE)

import build_content as B  # noqa: E402

SR = B.SR


def _sound(text):
    """Prononciation française d'un texte (espeak-ng, accents de mot et durées retirés) : sert à repérer les homophones."""
    import voice_engine as E
    ph = E.phonemize_fr(text)
    return re.sub(r"[ˈˌː\s.,;:!?…-]", "", ph)


def same_sound(ref, heard):
    """Vrai si le mot entendu se prononce comme le mot voulu (prend/prends, silencieuse/silencieuses…)."""
    if heard == "∅":
        return False
    a, b = _sound(ref), _sound(heard)
    if a == b:
        return True
    # presque identiques (un seul phonème d'écart sur un mot d'au moins 5 phonèmes) : prononciation correcte, transcription approximative
    import difflib
    return len(a) >= 5 and difflib.SequenceMatcher(a=a, b=b).ratio() >= 0.88


def word_diffs(ref_words, hyp_words):
    """
    [(mot voulu, mot entendu ou '∅')] pour chaque mot de référence en écart. Les écarts sont comparés par blocs :
    « pierres plates » entendu « pierre plate » est le même son, pas une erreur.
    """
    sm = difflib.SequenceMatcher(a=ref_words, b=hyp_words, autojunk=False)
    out = []
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            continue
        heard = " ".join(hyp_words[j1:j2]) or "∅"
        if tag == "replace" and same_sound(" ".join(ref_words[i1:i2]), heard):
            continue
        for i in range(i1, i2):
            out.append((ref_words[i], heard))
    return out


def real_errors(wm, ref, pieces, pad=0.3):
    """Mots réellement différents (hors homophones) entendus par Whisper dans ces morceaux, et la transcription."""
    import scipy.signal as ss
    gap = np.zeros(int(0.7 * SR), np.float32)
    padz = np.zeros(int(pad * SR), np.float32)
    seq = [padz]
    for i, p in enumerate(pieces):
        if i:
            seq.append(gap)
        seq.append(p)
    seq.append(padz)
    x = ss.resample_poly(np.concatenate(seq), 2, 3).astype(np.float32)
    segs, _ = wm.transcribe(x, language="fr", beam_size=5, condition_on_previous_text=False)
    hyp = " ".join(s.text for s in segs).strip()
    errs = word_diffs(B.norm_words(ref).split(), B.norm_words(hyp).split())
    return [e for e in errs if not same_sound(*e)], hyp


def fix(args, wm, units, cache):
    """Pour chaque phrase en écart : essaie les variantes de rendu, note chacune avec Whisper medium, garde la meilleure."""
    import math
    prev = json.load(open(args.fix, encoding="utf-8"))
    targets = {k for k, v in prev.items() if v.get("errors")}
    if args.also:
        pat = re.compile(args.also, re.I)
        targets |= {u.key for u in units if pat.search(u.ref)}
    by_key = {u.key: u for u in units}
    todo = [by_key[k] for k in targets if k in by_key]
    if args.only:
        pat_only = re.compile(args.only, re.I)
        todo = [u for u in todo if pat_only.search(u.ref)]
    print(f"{len(todo)} phrases à retravailler", flush=True)
    B._engine = B.E.KokoroEngine(os.path.join(HERE, "models", "kokoro"), threads=args.threads)
    stub = B._Stub(args.engine)
    chosen = dict(B.choices())
    changed = 0
    for n, u in enumerate(todo):
        results = []
        for order, var in enumerate(B.variants_for(stub, u)[: args.max_variants]):
            vid = B.variant_id(var)
            npz, meta = B._paths(cache, u, vid)
            if os.path.exists(meta) and os.path.exists(npz):
                info = json.load(open(meta))
                pcs = B.load_pieces(cache, u, vid)
            else:
                pcs = B.render_variant(u, var)
                if pcs is None:
                    continue
                info = B._evaluate(u, pcs, False)
                info.update(ok=True, vid=vid)
                np.savez_compressed(npz, n=len(pcs), **{f"p{i}": (np.clip(p, -1, 1) * 32767).astype(np.int16) for i, p in enumerate(pcs)})
            if not info.get("ok"):
                continue
            if info.get("f0") and abs(12 * math.log2(info["f0"] / args.f0_ref)) > B.PITCH_OK_ST:
                continue                                    # hauteur hors norme : écarté
            errs, hyp = real_errors(wm, u.ref, pcs)
            info.update(wer_medium=len(errs), hyp_medium=hyp)
            json.dump(info, open(meta, "w"), ensure_ascii=False)
            results.append((len(errs), order, vid, hyp))
            if not errs:
                break
        if not results:
            continue
        results.sort()
        best = results[0]
        cur = prev.get(u.key, {}).get("vid")
        cur_errs = len(prev.get(u.key, {}).get("errors", [])) if cur else None
        if cur_errs is None or best[0] < cur_errs:       # on ne change que pour faire mieux
            chosen[u.key] = best[2]
            changed += 1
        print(f"  {n + 1}/{len(todo)} {u.ref[:60]:60s} : {cur_errs} → {best[0]} ({best[2]})", flush=True)
    json.dump(chosen, open(B.CHOICES_FILE, "w", encoding="utf-8"), ensure_ascii=False, indent=1, sort_keys=True)
    print(f"{changed} choix mis à jour → {B.CHOICES_FILE}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="medium", help="dossier ou nom d'un modèle faster-whisper (tools/models/whisper-<nom>)")
    ap.add_argument("--engine", default="kokoro")
    ap.add_argument("--min-wer", type=float, default=0.10, help="ne revoit que les phrases au-dessus de ce WER du contrôle rapide")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--pad", type=float, default=0.3, help="silence ajouté autour (s)")
    ap.add_argument("--threads", type=int, default=4)
    ap.add_argument("--out", default=os.path.join(B.CACHE_ROOT, "verify.json"))
    ap.add_argument("--f0-ref", type=float, default=204.0)
    ap.add_argument("--fix", default="", help="JSON d'une revue précédente : re-rend et re-note (Whisper medium) les variantes des phrases en écart, garde la meilleure dans tools/voice_choices.json")
    ap.add_argument("--also", default="", help="avec --fix : expressions régulières de phrases à revoir en plus")
    ap.add_argument("--max-variants", type=int, default=7)
    ap.add_argument("--only", default="", help="avec --fix : ne revoit que les phrases qui correspondent à cette expression régulière")
    args = ap.parse_args()

    from faster_whisper import WhisperModel
    import jiwer
    import scipy.signal as ss

    mdir = args.model if os.path.isdir(args.model) else os.path.join(HERE, "models", "whisper-" + args.model)
    if not os.path.isdir(mdir):
        mdir = os.path.join(HERE, "..", "..")  # sera signalé par faster-whisper
    wm = WhisperModel(mdir, device="cpu", compute_type="int8", cpu_threads=args.threads)
    _, _, units = B.inventory()
    cache = os.path.join(B.CACHE_ROOT, args.engine)
    if args.fix:
        return fix(args, wm, units, cache)
    todo = []
    for u in units:
        info = B.best_variant(cache, u, args.f0_ref)
        if info is None:
            continue
        if args.all or info.get("wer", 1) >= args.min_wer:
            todo.append((u, info))
    print(f"{len(todo)} phrases à revoir sur {len(units)} (modèle {os.path.basename(mdir)})", flush=True)
    results = {}
    t0 = time.time()
    for n, (u, info) in enumerate(todo):
        pcs = B.load_pieces(cache, u, info["vid"])
        gap = np.zeros(int(0.7 * SR), np.float32)
        pad = np.zeros(int(args.pad * SR), np.float32)
        seq = [pad]
        for i, p in enumerate(pcs):
            if i:
                seq.append(gap)
            seq.append(p)
        seq.append(pad)
        x = ss.resample_poly(np.concatenate(seq), 2, 3).astype(np.float32)
        segs, _ = wm.transcribe(x, language="fr", beam_size=5, condition_on_previous_text=False)
        hyp = " ".join(s.text for s in segs).strip()
        ref_w, hyp_w = B.norm_words(u.ref).split(), B.norm_words(hyp).split()
        results[u.key] = dict(ref=u.ref, hyp=hyp, wer=float(jiwer.wer(" ".join(ref_w), " ".join(hyp_w))) if ref_w else 0.0,
                              vid=info["vid"], small=info.get("wer"), diffs=word_diffs(ref_w, hyp_w))
        if (n + 1) % 25 == 0:
            el = time.time() - t0
            print(f"  {n + 1}/{len(todo)} ({el / 60:.1f} min, reste ≈ {el / (n + 1) * (len(todo) - n - 1) / 60:.1f} min)", flush=True)
    # écarts réels : on retire les homophones (même prononciation que le mot voulu)
    real = {}
    for k, r in results.items():
        r["errors"] = [(w, h) for w, h in r["diffs"] if not same_sound(w, h)]
        r["wer_real"] = len(r["errors"]) / max(1, len(B.norm_words(r["ref"]).split()))
        for w, h in r["errors"]:
            real.setdefault(w, []).append((h, r["ref"]))
    json.dump(results, open(args.out, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    w = [r["wer"] for r in results.values()]
    wr = [r["wer_real"] for r in results.values()]
    print(f"écart médian {100 * np.median(w):.1f} %, moyen {100 * np.mean(w):.1f} %, > 30 % : {sum(x > 0.3 for x in w)} → {args.out}")
    print(f"sans les homophones : moyen {100 * np.mean(wr):.1f} %, phrases avec au moins un mot réellement différent : {sum(x > 0 for x in wr)}")
    print("\nMots réellement mal dits (par nombre d'occurrences) :")
    for word, hits in sorted(real.items(), key=lambda kv: -len(kv[1]))[:40]:
        heard = ", ".join(sorted({h for h, _ in hits})[:3])
        print(f"  {word:16s} ×{len(hits)}  entendu : {heard}")


if __name__ == "__main__":
    main()
