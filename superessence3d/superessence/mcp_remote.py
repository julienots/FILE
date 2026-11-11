"""Connecteur MCP « distant » pour utiliser SUPERESSENCE depuis l'application Claude MOBILE (ou claude.ai).

Pourquoi : l'application Claude sur téléphone n'exécute pas de serveur MCP local. Elle utilise des
« connecteurs » MCP distants, appelés en HTTPS depuis les serveurs d'Anthropic. Ce module :
  1. démarre le studio sur l'ordinateur (mode réseau local, pour voir le résultat sur le téléphone) ;
  2. sert les 45 outils MCP en HTTP (transport « Streamable HTTP ») sur 127.0.0.1:8766, sous une
     adresse secrète ;
  3. ouvre (si cloudflared est installé) un tunnel HTTPS gratuit et affiche l'URL du connecteur à
     coller dans Claude › Paramètres › Connecteurs › Ajouter un connecteur personnalisé.

Lancement : python -m superessence.mcp_remote [--port 8766] [--no-tunnel]
Sécurité : l'URL contient un secret de 32 caractères ; ne la partagez pas. Arrêter ce programme coupe
l'accès. Les opérations destructrices demandent toujours une confirmation, et le code Python reste
désactivé tant que vous ne l'activez pas.
"""
from __future__ import annotations

import argparse
import os
import re
import secrets
import shutil
import subprocess
import sys
import threading
import time

from . import config


def remote_secret(renew: bool = False) -> str:
    p = config.home() / "mcp_mobile_secret"
    if p.exists() and not renew:
        s = p.read_text(encoding="utf-8").strip()
        if len(s) >= 24:
            return s
    s = secrets.token_urlsafe(24)
    p.write_text(s, encoding="utf-8")
    try:
        os.chmod(p, 0o600)
    except OSError:
        pass
    return s


def build_app(secret: str):
    from mcp.server.transport_security import TransportSecuritySettings

    from .mcp_server import mcp
    # L'hôte change selon le tunnel : la protection repose sur le chemin secret.
    sec = TransportSecuritySettings(enable_dns_rebinding_protection=False)
    return mcp.streamable_http_app(streamable_http_path=f"/{secret}/mcp", transport_security=sec)


def start_tunnel(port: int, on_url) -> subprocess.Popen | None:
    exe = shutil.which("cloudflared")
    if exe is None:
        return None
    proc = subprocess.Popen([exe, "tunnel", "--no-autoupdate", "--url", f"http://127.0.0.1:{port}"],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)

    def read():
        for line in proc.stdout:
            m = re.search(r"https://[a-z0-9-]+\.trycloudflare\.com", line)
            if m:
                on_url(m.group(0))
    threading.Thread(target=read, daemon=True).start()
    return proc


def main(argv=None):
    ap = argparse.ArgumentParser(prog="superessence.mcp_remote", description="Connecteur MCP pour Claude mobile")
    ap.add_argument("--port", type=int, default=8766)
    ap.add_argument("--no-tunnel", action="store_true", help="ne pas ouvrir de tunnel HTTPS")
    ap.add_argument("--renew", action="store_true", help="générer une nouvelle adresse secrète")
    args = ap.parse_args(argv)

    os.environ["SUPERESSENCE_LAN"] = "1"  # le studio démarré automatiquement sera visible depuis le téléphone
    from .mcp_server import ensure_studio
    print("[SUPERESSENCE] Démarrage du studio…", flush=True)
    ensure_studio()
    secret = remote_secret(args.renew)
    app = build_app(secret)
    import uvicorn
    server = uvicorn.Server(uvicorn.Config(app, host="127.0.0.1", port=args.port, log_level="warning"))
    th = threading.Thread(target=server.run, daemon=True)
    th.start()
    for _ in range(100):
        if server.started:
            break
        time.sleep(0.05)
    local = f"http://127.0.0.1:{args.port}/{secret}/mcp"
    print(f"[SUPERESSENCE] Serveur MCP HTTP prêt (local) : {local}", flush=True)
    out_file = config.home() / "claude_mobile_connecteur.txt"

    def show(url):
        full = f"{url}/{secret}/mcp"
        out_file.write_text(full + "\n", encoding="utf-8")
        print("\n" + "=" * 72)
        print(" URL DU CONNECTEUR À AJOUTER DANS CLAUDE (gardez-la secrète) :")
        print(f"   {full}")
        print(" Claude (site ou app) › Paramètres › Connecteurs › Ajouter un connecteur personnalisé")
        print(f" (copie enregistrée dans {out_file})")
        print("=" * 72 + "\n", flush=True)

    tunnel = None
    if not args.no_tunnel:
        tunnel = start_tunnel(args.port, show)
        if tunnel is None:
            print("[SUPERESSENCE] « cloudflared » n'est pas installé : impossible d'ouvrir le tunnel HTTPS.\n"
                  "  Windows : winget install Cloudflare.cloudflared   ·   Mac : brew install cloudflared\n"
                  "  puis relancez ce programme. (Ou utilisez votre propre tunnel vers le port "
                  f"{args.port}, avec le chemin /{secret}/mcp.)", flush=True)
        else:
            print("[SUPERESSENCE] Ouverture du tunnel HTTPS (quelques secondes)…", flush=True)
    print("[SUPERESSENCE] Laissez cette fenêtre ouverte. Ctrl+C pour couper l'accès.", flush=True)
    try:
        while th.is_alive():
            time.sleep(0.5)
            if tunnel is not None and tunnel.poll() is not None:
                print("[SUPERESSENCE] Le tunnel s'est arrêté.", flush=True)
                tunnel = None
    except KeyboardInterrupt:
        print("\n[SUPERESSENCE] Accès coupé.")
    finally:
        server.should_exit = True
        if tunnel is not None:
            tunnel.terminate()


if __name__ == "__main__":
    main()
