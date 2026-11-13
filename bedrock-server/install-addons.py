#!/usr/bin/env python3
"""Installe tous les .mcaddon / .mcpack / .zip du dossier addons/ dans le serveur Bedrock.

Usage : python3 install-addons.py [nom du monde]    (défaut : "Bedrock level")
Puis  : docker compose restart
"""
import json, re, shutil, sys, zipfile
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).parent
DATA = ROOT / "data"
LEVEL = sys.argv[1] if len(sys.argv) > 1 else "Bedrock level"


def load_json(text):
    """Les manifest.json contiennent parfois des commentaires ou des virgules en trop."""
    text = text.lstrip("﻿")
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        text = re.sub(r'(?m)^\s*//.*$', '', text)
        text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
        text = re.sub(r',(\s*[}\]])', r'\1', text)
        return json.loads(text)


def safe_name(s):
    return re.sub(r'[^\w.\- ]', '_', s).strip() or "pack"


def update_world_file(path, entry):
    path.parent.mkdir(parents=True, exist_ok=True)
    items = json.loads(path.read_text()) if path.exists() else []
    items = [i for i in items if i.get("pack_id") != entry["pack_id"]]
    items.append(entry)
    path.write_text(json.dumps(items, indent=2))


def install(archive):
    count = 0
    with zipfile.ZipFile(archive) as z:
        manifests = [n for n in z.namelist()
                     if PurePosixPath(n).name == "manifest.json" and len(PurePosixPath(n).parts) <= 3]
        for m in manifests:
            folder = str(PurePosixPath(m).parent)
            manifest = load_json(z.read(m).decode("utf-8"))
            header = manifest["header"]
            types = {mod.get("type") for mod in manifest.get("modules", [])}
            if "resources" in types:
                kind, world_file = "resource_packs", "world_resource_packs.json"
            elif types & {"data", "script", "client_data"}:
                kind, world_file = "behavior_packs", "world_behavior_packs.json"
            else:
                print(f"  ? type inconnu ignoré : {m}")
                continue
            name = safe_name(header.get("name", folder))
            name = re.sub(r'§.', '', name)  # codes couleur Minecraft
            dest = DATA / kind / f"{name}_{header['uuid'][:8]}"
            if dest.exists():
                shutil.rmtree(dest)
            prefix = "" if folder == "." else folder + "/"
            for member in z.namelist():
                if member.startswith(prefix) and not member.endswith("/"):
                    rel = PurePosixPath(member[len(prefix):])
                    if ".." in rel.parts:
                        continue
                    target = dest.joinpath(*rel.parts)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(z.read(member))
            update_world_file(DATA / "worlds" / LEVEL / world_file,
                              {"pack_id": header["uuid"], "version": header["version"]})
            print(f"  + {kind[:-1]} : {header.get('name', name)}")
            count += 1
    return count


def main():
    files = sorted(p for p in (ROOT / "addons").iterdir()
                   if p.suffix.lower() in (".mcaddon", ".mcpack", ".zip"))
    if not files:
        sys.exit("Aucun addon trouvé dans addons/ (.mcaddon, .mcpack ou .zip).")
    total = 0
    for f in files:
        print(f.name)
        try:
            total += install(f)
        except (zipfile.BadZipFile, KeyError, json.JSONDecodeError) as e:
            print(f"  ! erreur : {e}")
    print(f"\n{total} pack(s) installé(s) dans le monde « {LEVEL} ». Lance : docker compose restart")


main()
