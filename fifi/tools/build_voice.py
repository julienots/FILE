#!/usr/bin/env python3
"""Génère toutes les répliques de Fifi avec la voix clonée.

Méthode (gratuite, locale, sans clé) :
  1. Piper (voix française « siwis », très bonne diction) prononce la phrase ;
  2. Chatterbox VC (Resemble AI, licence MIT) convertit cette voix vers le timbre de
     l'enregistrement de Fifi (tools/voix_fifi.m4a) ;
  3. nettoyage : rognage des blancs, égalisation, normalisation, encodage Opus.

    pip install piper-tts                      # dans un environnement
    pip install chatterbox-tts "setuptools<81" # (peut être un autre environnement)
    python3 tools/build_voice.py --ref tools/voix_fifi.m4a --piper /chemin/vers/piper \
        --piper-model fr_FR-siwis-medium.onnx [--shard 0/2]
    python3 tools/build_voice.py --ref tools/voix_fifi.m4a --index-only   # (ré)écrit index.json

Sortie : app/src/main/assets/voice/<categorie>/<n>.ogg + index.json
Cache : tools/.cache (les phrases déjà générées ne sont pas refaites).
"""
import argparse, hashlib, json, os, re, subprocess, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from phrases import PHRASES, ORDERED  # noqa: E402

OUT = HERE.parent / "app/src/main/assets/voice"
CACHE = HERE / ".cache"
VERSION = "piper-siwis+cbvc-v1"
LENGTH_SCALE = "0.93"   # un peu plus rapide que la normale : Fifi est blasée, pas lente


def run(*cmd, **kw):
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, **kw)


def all_jobs():
    """[(categorie, index, texte)] ; les catégories ORDERED gardent leurs index."""
    jobs = []
    for cat, lst in PHRASES.items():
        jobs += [(cat, i, t) for i, t in enumerate(lst)]
    for cat, val in ORDERED.items():
        items = val.items() if isinstance(val, dict) else enumerate(val)
        jobs += [(cat, i, t) for i, t in items]
    return jobs


def key_for(ref_key, text):
    return hashlib.sha1(f"{ref_key}|{VERSION}|{text}".encode()).hexdigest()[:16]


def write_index(ref_key):
    index = {}
    for cat, i, text in all_jobs():
        lst = index.setdefault(cat, [])
        while len(lst) <= i:
            lst.append({"f": "", "t": ""})
        rel = f"{cat}/{i}.ogg"
        cached = CACHE / f"{key_for(ref_key, text)}.ogg"
        if text.strip() and cached.exists():
            (OUT / cat).mkdir(parents=True, exist_ok=True)
            (OUT / rel).write_bytes(cached.read_bytes())
            lst[i] = {"f": rel, "t": text}
        else:
            lst[i] = {"f": "", "t": text}
    (OUT / "index.json").write_text(json.dumps(index, ensure_ascii=False, indent=1), encoding="utf-8")
    missing = sum(1 for c, i, t in all_jobs() if t.strip() and not (CACHE / f"{key_for(ref_key, t)}.ogg").exists())
    print(f"index.json écrit ({missing} phrase(s) manquante(s))")


def piper_text(text):
    # Les points de suspension deviennent de vraies pauses ; les mots isolés gardent une ponctuation.
    t = text.replace("...", "…").replace("…", ", ... ")
    return t if re.search(r"[.!?]$", t.strip()) else t.strip() + "."


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ref", required=True)
    ap.add_argument("--piper", default="piper")
    ap.add_argument("--piper-model", default="fr_FR-siwis-medium.onnx")
    ap.add_argument("--shard", default="0/1")
    ap.add_argument("--threads", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--index-only", action="store_true")
    args = ap.parse_args()

    CACHE.mkdir(exist_ok=True)
    ref = CACHE / "reference.wav"
    if not ref.exists():
        # Nettoyage : passe-haut, débruitage, suppression des blancs, normalisation.
        run("ffmpeg", "-y", "-i", args.ref, "-ac", "1", "-ar", "22050", "-af",
            "highpass=f=70,afftdn=nf=-25,silenceremove=stop_periods=-1:stop_duration=0.35:stop_threshold=-38dB,"
            "loudnorm=I=-18:TP=-2", str(ref))
    ref_key = hashlib.sha1(ref.read_bytes()).hexdigest()[:10]
    if args.index_only:
        return write_index(ref_key)

    import torch, torchaudio
    from chatterbox.vc import ChatterboxVC
    torch.set_num_threads(args.threads)
    vc = ChatterboxVC.from_pretrained("cpu")
    vc.set_target_voice(str(ref))

    k, n = map(int, args.shard.split("/"))
    todo = [j for j in all_jobs() if j[2].strip()]
    todo = [j for idx, j in enumerate(todo) if idx % n == k]
    for num, (cat, i, text) in enumerate(todo, 1):
        cached = CACHE / f"{key_for(ref_key, text)}.ogg"
        if cached.exists():
            continue
        with tempfile.TemporaryDirectory() as td:
            src = Path(td) / "src.wav"
            run(args.piper, "-m", args.piper_model, "--length-scale", LENGTH_SCALE,
                "--sentence-silence", "0.25", "-f", str(src), input=piper_text(text).encode())
            with torch.inference_mode():
                wav = vc.generate(str(src))
            conv = Path(td) / "vc.wav"
            torchaudio.save(str(conv), wav, vc.sr)
            run("ffmpeg", "-y", "-i", str(conv), "-af",
                "silenceremove=start_periods=1:start_threshold=-45dB:start_silence=0.05,"
                "areverse,silenceremove=start_periods=1:start_threshold=-45dB:start_silence=0.12,areverse,"
                "highpass=f=80,equalizer=f=3000:t=q:w=1:g=2,loudnorm=I=-16:TP=-1.5,apad=pad_dur=0.08",
                "-ar", "24000", "-ac", "1", "-c:a", "libopus", "-b:a", "40k", str(cached))
        print(f"[{k}:{num}/{len(todo)}] {cat}/{i} « {text} »", flush=True)
    write_index(ref_key)


if __name__ == "__main__":
    main()
