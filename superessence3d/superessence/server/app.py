"""Serveur HTTP local : interface web, API de commandes, fichiers, WebSocket d'événements.

Sécurité :
- écoute sur 127.0.0.1 uniquement (sauf mode réseau local explicite) ;
- jeton secret obligatoire (en-tête X-SE-Token, ou paramètre ?token= pour images/WebSocket) ;
- contrôle de l'en-tête Host (protection contre le « DNS rebinding ») ;
- les fichiers servis sont limités au dossier de travail SuperEssence3D.
"""
from __future__ import annotations

import asyncio
import json
import mimetypes
import os
import secrets
import time
from pathlib import Path

from fastapi import FastAPI, File, Request, UploadFile, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse, HTMLResponse, JSONResponse, Response

from .. import __version__

WEB = Path(__file__).resolve().parent.parent / "web"


def create_app(engine_box: dict, executor, token: str, lan: bool = False) -> FastAPI:
    app = FastAPI(title="SUPERESSENCE 3D STUDIO", version=__version__, docs_url=None, redoc_url=None,
                  openapi_url=None)
    clients: set[WebSocket] = set()
    loop_box: dict = {}
    home = Path(engine_box["home"]).resolve()

    def broadcast(event: dict):
        loop = loop_box.get("loop")
        if loop is None:
            return
        data = json.dumps(event, ensure_ascii=False, default=str)

        async def send_all():
            dead = []
            for ws in list(clients):
                try:
                    await ws.send_text(data)
                except Exception:
                    dead.append(ws)
            for ws in dead:
                clients.discard(ws)
        try:
            asyncio.run_coroutine_threadsafe(send_all(), loop)
        except RuntimeError:
            pass

    engine_box["broadcast"] = broadcast

    @app.on_event("startup")
    async def _startup():
        loop_box["loop"] = asyncio.get_running_loop()

    # -- sécurité ------------------------------------------------------------
    allowed_hosts = {"127.0.0.1", "localhost", "[::1]"}

    def host_ok(request) -> bool:
        if lan:
            return True
        host = (request.headers.get("host") or "").rsplit(":", 1)[0]
        return host in allowed_hosts

    def token_ok(request) -> bool:
        t = request.headers.get("x-se-token") or request.query_params.get("token") or ""
        return secrets.compare_digest(t, token)

    @app.middleware("http")
    async def guard(request: Request, call_next):
        if not host_ok(request):
            return JSONResponse({"ok": False, "error": "Hôte refusé."}, status_code=403)
        path = request.url.path
        if path.startswith("/api/") and not token_ok(request):
            return JSONResponse({"ok": False, "error": "Jeton d'accès invalide."}, status_code=401)
        resp = await call_next(request)
        resp.headers["X-Content-Type-Options"] = "nosniff"
        resp.headers["Referrer-Policy"] = "no-referrer"
        if path == "/" or path.endswith(".html"):
            resp.headers["Cache-Control"] = "no-store"
        return resp

    async def run(fn, *args, label=None):
        fut = executor.submit(fn, *args, label=label)
        return await asyncio.wrap_future(fut)

    # -- interface -------------------------------------------------------------
    def is_local_client(request) -> bool:
        host = (request.headers.get("host") or "").rsplit(":", 1)[0]
        client = request.client.host if request.client else ""
        return host in allowed_hosts and client in ("127.0.0.1", "::1", "localhost")

    @app.get("/", response_class=HTMLResponse)
    async def index(request: Request):
        # Sur le réseau local, la page n'est servie qu'avec le jeton (obtenu par appairage).
        if not is_local_client(request) and not token_ok(request):
            return HTMLResponse("<h3>SUPERESSENCE 3D STUDIO</h3><p>Accès refusé : appairez l'appareil avec le code "
                                "affiché dans le studio (Paramètres › Connexion mobile).</p>", status_code=401)
        html = (WEB / "index.html").read_text(encoding="utf-8")
        return HTMLResponse(html.replace("__SE_TOKEN__", token).replace("__SE_VERSION__", __version__))

    # -- appairage mobile (application Android compagnon) ------------------------
    pairing: dict = {"code": None, "expires": 0.0, "attempts": 0}

    def lan_addresses() -> list[str]:
        import socket
        ips = set()
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
                s.connect(("10.255.255.255", 1))  # aucun paquet envoyé : sert à choisir l'interface
                ips.add(s.getsockname()[0])
        except OSError:
            pass
        try:
            for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
                ips.add(info[4][0])
        except OSError:
            pass
        return sorted(ip for ip in ips if not ip.startswith("127."))

    @app.get("/ping")
    async def ping():
        """Détection du studio sur le réseau local (aucune donnée sensible)."""
        return {"app": "superessence3d", "version": __version__, "lan": lan}

    @app.post("/api/pair/start")
    async def pair_start(request: Request):
        port = request.url.port or 8765
        pairing["code"] = f"{secrets.randbelow(1_000_000):06d}"
        pairing["expires"] = time.time() + 300
        pairing["attempts"] = 0
        return {"ok": True, "code": pairing["code"], "expires_in": 300, "lan": lan,
                "addresses": [f"{ip}:{port}" for ip in lan_addresses()]}

    @app.post("/pair")
    async def pair(request: Request):
        if not lan:
            return JSONResponse({"ok": False, "error": "Le studio n'est pas en mode réseau local (--lan)."}, status_code=403)
        try:
            body = await request.json()
        except ValueError:
            body = {}
        code = str(body.get("code", "")).strip()
        if not pairing["code"] or time.time() > pairing["expires"]:
            return JSONResponse({"ok": False, "error": "Aucun code actif : affichez un code dans le studio."}, status_code=403)
        pairing["attempts"] += 1
        if pairing["attempts"] > 5:
            pairing["code"] = None
            return JSONResponse({"ok": False, "error": "Trop d'essais : générez un nouveau code."}, status_code=429)
        if not secrets.compare_digest(code, pairing["code"]):
            return JSONResponse({"ok": False, "error": "Code incorrect."}, status_code=403)
        pairing["code"] = None  # usage unique
        broadcast({"type": "log", "level": "info", "message": "Appareil mobile appairé.", "time": time.time()})
        return {"ok": True, "token": token, "version": __version__}

    @app.get("/web/{path:path}")
    async def static(path: str):
        p = (WEB / path).resolve()
        if not str(p).startswith(str(WEB)) or not p.is_file():
            return Response(status_code=404)
        mt = mimetypes.guess_type(str(p))[0] or "application/octet-stream"
        if p.suffix == ".js":
            mt = "text/javascript"
        return FileResponse(p, media_type=mt, headers={"Cache-Control": "no-cache"})

    # -- API -------------------------------------------------------------------
    @app.get("/api/status")
    async def status():
        eng = engine_box.get("engine")
        st = await run(eng.status) if eng else {}
        return {"ok": True, "version": __version__, "busy": executor.busy_label, "engine": st,
                "home": str(home)}

    @app.get("/api/commands")
    async def commands():
        from ..engine.core import COMMANDS
        return {"ok": True, "commands": [{"name": c.name, "label": c.label, "description": c.description,
                                          "history": c.history, "destructive": c.destructive}
                                         for c in COMMANDS.values()]}

    @app.post("/api/cmd/{name}")
    async def cmd(name: str, request: Request):
        try:
            params = await request.json() if (await request.body()) else {}
        except ValueError:
            return JSONResponse({"ok": False, "error": "JSON invalide."}, status_code=400)
        return await execute(name, params)

    async def execute(name, params):
        from ..engine.core import CommandError, ConfirmationRequired
        eng = engine_box["engine"]
        t0 = time.time()
        try:
            result = await run(eng.execute, name, params, label=name)
            return {"ok": True, "result": result, "seconds": round(time.time() - t0, 3)}
        except ConfirmationRequired as e:
            return JSONResponse({"ok": False, "confirmation_required": True, "error": str(e)}, status_code=409)
        except CommandError as e:
            broadcast({"type": "log", "level": "error", "message": f"{name} : {e}", "time": time.time()})
            return JSONResponse({"ok": False, "error": str(e)}, status_code=400)
        except Exception as e:  # erreur inattendue : message clair + journal
            import traceback
            tb = traceback.format_exc()
            print(tb)
            broadcast({"type": "log", "level": "error", "message": f"{name} : erreur interne {type(e).__name__}: {e}",
                       "time": time.time()})
            return JSONResponse({"ok": False, "error": f"Erreur interne ({type(e).__name__}) : {e}"}, status_code=500)

    @app.post("/api/upload")
    async def upload(file: UploadFile = File(...)):
        name = os.path.basename(file.filename or "fichier")
        safe = "".join(c if c.isalnum() or c in "-_." else "_" for c in name)[:120] or "fichier"
        ext = Path(safe).suffix.lower()
        allowed = {".glb", ".gltf", ".obj", ".fbx", ".stl", ".ply", ".blend", ".png", ".jpg", ".jpeg", ".webp",
                   ".bmp", ".tga", ".tif", ".tiff", ".exr", ".bin", ".mtl"}
        if ext not in allowed:
            return JSONResponse({"ok": False, "error": f"Type de fichier refusé : {ext}"}, status_code=400)
        dest_dir = home / "imports"
        dest_dir.mkdir(parents=True, exist_ok=True)
        dest = dest_dir / f"{int(time.time() * 1000)}_{safe}"
        size = 0
        with open(dest, "wb") as f:
            while chunk := await file.read(1 << 20):
                size += len(chunk)
                if size > 2 << 30:
                    f.close()
                    dest.unlink(missing_ok=True)
                    return JSONResponse({"ok": False, "error": "Fichier trop volumineux (max 2 Go)."}, status_code=400)
                f.write(chunk)
        return {"ok": True, "path": str(dest), "bytes": size}

    @app.get("/api/file")
    async def get_file(path: str, download: int = 0):
        p = Path(path).resolve()
        if not str(p).startswith(str(home)) or not p.is_file():
            return JSONResponse({"ok": False, "error": "Fichier non autorisé ou introuvable."}, status_code=404)
        headers = {"Cache-Control": "no-cache"}
        if download:
            return FileResponse(p, filename=p.name, headers=headers)
        mt = mimetypes.guess_type(str(p))[0] or "application/octet-stream"
        if p.suffix == ".glb":
            mt = "model/gltf-binary"
        return FileResponse(p, media_type=mt, headers=headers)

    # -- WebSocket -------------------------------------------------------------
    @app.websocket("/ws")
    async def ws_endpoint(ws: WebSocket):
        t = ws.query_params.get("token", "")
        host = (ws.headers.get("host") or "").rsplit(":", 1)[0]
        if not secrets.compare_digest(t, token) or (not lan and host not in allowed_hosts):
            await ws.close(code=4401)
            return
        await ws.accept()
        clients.add(ws)
        try:
            while True:
                await ws.receive_text()  # ping
        except WebSocketDisconnect:
            pass
        finally:
            clients.discard(ws)

    return app
