#!/usr/bin/env python3
"""Rend un extrait d'une séance (voix + ambiances) en MP3, exactement comme l'application la mixe."""
import json, os, subprocess, sys, tempfile
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
A = os.path.join(ROOT, "app", "src", "main", "assets")
SR = 48000

def decode(path, ch=1):
    out = subprocess.run(["ffmpeg", "-loglevel", "error", "-i", path, "-f", "f32le", "-ac", str(ch), "-ar", str(SR), "-"], capture_output=True, check=True).stdout
    return np.frombuffer(out, dtype="<f4")

def main(track_id, seconds, out):
    cat = json.load(open(os.path.join(A, "content", "catalog.json")))
    t = next(x for x in cat["tracks"] if x["id"] == track_id)
    n = int(seconds * SR)
    voice = np.zeros(n, dtype=np.float32)
    pos = 0
    for s in t["segments"]:
        if pos >= n: break
        if isinstance(s, int):
            pos += int(s * SR / 1000)
        else:
            f = s[3:] if s.startswith("fx:") else s
            clip = decode(os.path.join(A, "voice", f + ".ogg"))
            k = min(len(clip), n - pos)
            voice[pos:pos + k] += clip[:k]
            pos += len(clip)
    mix = np.stack([voice, voice], axis=1)
    for layer in t["ambience"]:
        bed = decode(os.path.join(A, "sounds", layer["sound"] + ".ogg"), ch=2).reshape(-1, 2)
        reps = int(np.ceil(n / len(bed)))
        bed = np.tile(bed, (reps, 1))[:n]
        fade = np.minimum(1, np.arange(n) / (SR * 3))[:, None]
        mix += bed * layer["volume"] * 0.8 * fade
    end_fade = np.minimum(1, (n - np.arange(n)) / (SR * 4))[:, None]
    mix *= end_fade
    with tempfile.NamedTemporaryFile(suffix=".raw", delete=False) as tmp:
        tmp.write(np.clip(mix, -1, 1).astype("<f4").tobytes())
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-f", "f32le", "-ar", str(SR), "-ac", "2", "-i", tmp.name,
                    "-codec:a", "libmp3lame", "-b:a", "160k", "-metadata", "title=NOCTÉA — " + t["title"],
                    "-metadata", "artist=Superessence Studio", out], check=True)
    os.unlink(tmp.name)
    print(out)

if __name__ == "__main__":
    main(sys.argv[1], float(sys.argv[2]), sys.argv[3])
