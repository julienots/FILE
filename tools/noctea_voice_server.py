#!/usr/bin/env python3
"""
NOCTÉA — serveur vocal : EXACTEMENT la voix de la bibliothèque, pour « Mon texte ».

Sur le téléphone, les textes libres sont dits par le moteur vocal d'Android (une autre voix). Ce serveur
fait tourner le même modèle (Kokoro, voix « Siwis ») et la même chaîne studio que la production de la
bibliothèque : même timbre, même niveau, même pièce. L'application le reconnaît (en-tête
`X-Noctea-Studio: 1`) et ne retouche pas l'audio.

Protocole (celui de « Serveur vocal » dans Profil › Voix) :
    POST /   {"text": "…", "voice": "", "length_scale": 1.0, "speed": 1.0}
    →        audio/wav (PCM 16 bits, mono, 24 kHz)

Usage :
    python3 tools/fetch_models.py                      # une fois
    python3 tools/noctea_voice_server.py --key MON_SECRET
    # puis, dans NOCTÉA : Profil › Voix › Serveur vocal › URL, clé = MON_SECRET

Android n'accepte que les adresses https:// : placez le serveur derrière un proxy HTTPS ou un tunnel
(Tailscale, Cloudflare Tunnel, nginx + certificat…). Par défaut le serveur n'écoute que sur 127.0.0.1.
Test local : curl -s -X POST localhost:8765 -d '{"text":"Bonsoir... ferme les yeux."}' -o test.wav
"""
import argparse
import io
import json
import os
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import voice_engine as E  # noqa: E402
import voice_studio as S  # noqa: E402

PRIME = "Doucement."
MAX_CHARS = 700
LOCK = threading.Lock()
ENGINE = None
KEY = ""


def render_fragment(text, speed_factor=1.0):
    """Une phrase isolée → clip studio. Une amorce de contexte évite l'attaque instable des phrases très courtes."""
    speed = min(1.0, max(0.5, E.KokoroEngine.default_speed * speed_factor))
    primed = ENGINE.render(PRIME + " " + text, speed)
    pieces = E.cut_at_pauses(primed, [PRIME, text])
    if pieces is not None:
        piece = pieces[1]
    else:
        x = ENGINE.render(text, speed)
        a, b = E.speech_region(x)
        piece = x[a:b]
    return S.process(piece)


def to_wav_bytes(y):
    import soundfile as sf
    buf = io.BytesIO()
    sf.write(buf, y, E.SR, format="WAV", subtype="PCM_16")
    return buf.getvalue()


class Handler(BaseHTTPRequestHandler):
    server_version = "NocteaVoice/1.0"

    def _reply(self, code, body=b"", ctype="text/plain; charset=utf-8", extra=None):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        for k, v in (extra or {}).items():
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):  # noqa: N802 — contrôle rapide dans un navigateur
        self._reply(200, "NOCTÉA voix — POST {\"text\": \"…\"}".encode("utf-8"))

    def do_POST(self):  # noqa: N802
        if KEY and self.headers.get("Authorization", "") != f"Bearer {KEY}":
            return self._reply(401, b"cle invalide")
        try:
            n = int(self.headers.get("Content-Length", "0"))
            req = json.loads(self.rfile.read(n).decode("utf-8"))
            text = " ".join(str(req.get("text", "")).split())
            speed = float(req.get("speed", 1.0) or 1.0)
        except Exception:
            return self._reply(400, b"requete invalide")
        if not text or len(text) > MAX_CHARS:
            return self._reply(400, b"texte vide ou trop long")
        try:
            with LOCK:
                y = render_fragment(text, min(1.5, max(0.5, speed)))
            self._reply(200, to_wav_bytes(y), "audio/wav", {"X-Noctea-Studio": "1"})
        except Exception as e:  # noqa: BLE001
            self._reply(500, f"erreur de synthese : {e}".encode("utf-8"))

    def log_message(self, fmt, *args):  # journal discret : jamais le texte
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))


def main():
    global ENGINE, KEY
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--key", default=os.environ.get("NOCTEA_VOICE_KEY", ""), help="clé d'accès (Authorization: Bearer)")
    ap.add_argument("--model", default=os.path.join(HERE, "models", "kokoro"))
    args = ap.parse_args()
    KEY = args.key
    ENGINE = E.KokoroEngine(args.model)
    if args.host not in ("127.0.0.1", "localhost") and not KEY:
        print("⚠ serveur ouvert sans clé d'accès : ajoutez --key", file=sys.stderr)
    print(f"Serveur vocal NOCTÉA sur http://{args.host}:{args.port}", flush=True)
    ThreadingHTTPServer((args.host, args.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
