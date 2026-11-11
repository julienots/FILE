"""Lancement : python -m superessence [--port 8765] [--no-browser] [--lan]

Le moteur Blender tourne sur le thread principal ; le serveur web sur un thread secondaire.
"""
from __future__ import annotations

import argparse
import os
import socket
import sys
import threading
import time
import webbrowser


def _port_free(port: int, host: str) -> bool:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            s.bind((host, port))
            return True
        except OSError:
            return False


def main(argv=None):
    from . import config
    settings = config.load_settings()
    ap = argparse.ArgumentParser(prog="superessence", description="SUPERESSENCE 3D STUDIO")
    ap.add_argument("--port", type=int, default=int(settings.get("port", config.DEFAULT_PORT)))
    ap.add_argument("--no-browser", action="store_true", help="ne pas ouvrir le navigateur")
    ap.add_argument("--lan", action="store_true", help="écouter sur le réseau local (tablette, téléphone)")
    args = ap.parse_args(argv)
    lan = args.lan or bool(settings.get("lan"))
    host = "0.0.0.0" if lan else "127.0.0.1"
    if not _port_free(args.port, host):
        print(f"[SUPERESSENCE] Le port {args.port} est déjà utilisé : le studio tourne peut-être déjà "
              f"(http://127.0.0.1:{args.port}/).", file=sys.stderr)
        sys.exit(2)
    try:
        import bpy  # noqa: F401
    except ImportError:
        print("[SUPERESSENCE] Le module Blender « bpy » est introuvable. Lancez le script d'installation "
              "(install_windows.bat ou install_linux_mac.sh).", file=sys.stderr)
        sys.exit(3)
    import uvicorn

    from .engine.core import Engine
    from .server.app import create_app
    from .server.executor import MainThreadExecutor

    token = config.token()
    executor = MainThreadExecutor()
    box: dict = {"home": str(config.home())}
    app = create_app(box, executor, token, lan=lan)

    def listener(event):
        b = box.get("broadcast")
        if b:
            b(event)

    print("[SUPERESSENCE] Démarrage du moteur Blender…", flush=True)
    engine = Engine(settings, listener=listener)
    engine.defer = lambda fn, label=None: executor.submit(fn, label=label)
    box["engine"] = engine

    server = uvicorn.Server(uvicorn.Config(app, host=host, port=args.port, log_level="warning"))
    th = threading.Thread(target=server.run, daemon=True)
    th.start()
    url = f"http://127.0.0.1:{args.port}/"
    for _ in range(100):
        if server.started:
            break
        time.sleep(0.05)
    print(f"[SUPERESSENCE] Studio prêt : {url}", flush=True)
    print(f"[SUPERESSENCE] Dossier de travail : {config.home()}", flush=True)
    if lan:
        print("[SUPERESSENCE] Mode réseau local actif : dans le studio, Paramètres › Connexion mobile affiche le code "
              "d'appairage pour l'application Android.", flush=True)
    if not args.no_browser and not os.environ.get("SUPERESSENCE_NO_BROWSER"):
        threading.Timer(0.5, lambda: webbrowser.open(url)).start()
    try:
        executor.run_forever()
    except KeyboardInterrupt:
        print("\n[SUPERESSENCE] Arrêt.")
    finally:
        server.should_exit = True


if __name__ == "__main__":
    main()
