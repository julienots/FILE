#!/usr/bin/env python3
"""
NOCTÉA — télécharge les modèles nécessaires à la production vocale dans tools/models/.

  tools/models/kokoro/        Kokoro-82M (ONNX, Apache-2.0) + voix française ff_siwis
  tools/models/whisper-small/ faster-whisper « small » : contrôle d'intelligibilité de chaque clip
  tools/models/whisper-medium/ (option --with-medium) revue fine de la prononciation (tools/voice_verify.py)
  tools/models/piper/         (option --with-piper) ancienne voix Piper « siwis medium »

Usage : python3 tools/fetch_models.py [--with-piper]
Téléchargement HTTPS direct (aucune bibliothèque Hugging Face requise).
"""
import argparse
import os
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
MODELS = os.path.join(HERE, "models")

FILES = {
    "kokoro": [
        ("https://huggingface.co/onnx-community/Kokoro-82M-v1.0-ONNX/resolve/main/onnx/model.onnx", "model.onnx", 325_000_000),
        ("https://huggingface.co/onnx-community/Kokoro-82M-v1.0-ONNX/resolve/main/voices/ff_siwis.bin", "ff_siwis.bin", 500_000),
    ],
    "whisper-small": [
        (f"https://huggingface.co/Systran/faster-whisper-small/resolve/main/{n}", n, s)
        for n, s in [("config.json", 1_000), ("model.bin", 480_000_000), ("tokenizer.json", 2_000_000), ("vocabulary.txt", 400_000)]
    ],
    "whisper-medium": [
        (f"https://huggingface.co/Systran/faster-whisper-medium/resolve/main/{n}", n, sz)
        for n, sz in [("config.json", 1_000), ("model.bin", 1_400_000_000), ("tokenizer.json", 2_000_000), ("vocabulary.txt", 400_000)]
    ],
    "piper": [
        ("https://huggingface.co/rhasspy/piper-voices/resolve/main/fr/fr_FR/siwis/medium/fr_FR-siwis-medium.onnx", "fr_FR-siwis-medium.onnx", 60_000_000),
        ("https://huggingface.co/rhasspy/piper-voices/resolve/main/fr/fr_FR/siwis/medium/fr_FR-siwis-medium.onnx.json", "fr_FR-siwis-medium.onnx.json", 1_000),
    ],
}


def download(url, dest, min_size):
    if os.path.exists(dest) and os.path.getsize(dest) >= min_size:
        print(f"  ✓ {os.path.basename(dest)} (déjà là)")
        return
    tmp = dest + ".part"
    print(f"  ↓ {os.path.basename(dest)}", flush=True)
    with urllib.request.urlopen(url, timeout=120) as r, open(tmp, "wb") as f:
        while True:
            chunk = r.read(1 << 20)
            if not chunk:
                break
            f.write(chunk)
    if os.path.getsize(tmp) < min_size:
        raise SystemExit(f"Téléchargement incomplet : {url}")
    os.replace(tmp, dest)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--with-piper", action="store_true")
    ap.add_argument("--with-medium", action="store_true", help="Whisper medium (1,5 Go) pour tools/voice_verify.py")
    args = ap.parse_args()
    groups = ["kokoro", "whisper-small"] + (["whisper-medium"] if args.with_medium else []) + (["piper"] if args.with_piper else [])
    for g in groups:
        d = os.path.join(MODELS, g)
        os.makedirs(d, exist_ok=True)
        print(g)
        for url, name, size in FILES[g]:
            download(url, os.path.join(d, name), size)
    print("OK →", MODELS)


if __name__ == "__main__":
    sys.exit(main())
