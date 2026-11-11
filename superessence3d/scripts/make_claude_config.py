"""Génère la configuration MCP pour Claude Desktop et (option --install) l'ajoute à Claude Desktop.

Claude Desktop lance les serveurs MCP locaux en « stdio » à partir du fichier claude_desktop_config.json :
  Windows : %APPDATA%\\Claude\\claude_desktop_config.json
  macOS   : ~/Library/Application Support/Claude/claude_desktop_config.json
Une copie de sauvegarde est créée avant toute modification.
"""
from __future__ import annotations

import json
import os
import shutil
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def server_entry() -> dict:
    return {"command": sys.executable, "args": ["-m", "superessence.mcp_server"],
            "env": {"PYTHONPATH": str(ROOT)}}


def claude_config_path() -> Path | None:
    if sys.platform == "win32":
        base = os.environ.get("APPDATA")
        return Path(base) / "Claude" / "claude_desktop_config.json" if base else None
    if sys.platform == "darwin":
        return Path.home() / "Library" / "Application Support" / "Claude" / "claude_desktop_config.json"
    return Path.home() / ".config" / "Claude" / "claude_desktop_config.json"


def main():
    cfg = {"mcpServers": {"superessence": server_entry()}}
    out = ROOT / "claude_desktop_config.json"
    out.write_text(json.dumps(cfg, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"Configuration écrite : {out}")
    if "--install" not in sys.argv:
        print("Pour l'ajouter à Claude Desktop : relancez avec --install, ou copiez ce bloc dans "
              "Claude Desktop › Paramètres › Développeur › Modifier la configuration.")
        return
    target = claude_config_path()
    if target is None:
        print("Emplacement de Claude Desktop inconnu sur ce système.")
        return
    target.parent.mkdir(parents=True, exist_ok=True)
    current = {}
    if target.exists():
        try:
            current = json.loads(target.read_text(encoding="utf-8") or "{}")
        except ValueError:
            print(f"Le fichier {target} n'est pas un JSON valide : il n'a pas été modifié.")
            return
        backup = target.with_suffix(f".sauvegarde-{time.strftime('%Y%m%d-%H%M%S')}.json")
        shutil.copy2(target, backup)
        print(f"Sauvegarde : {backup}")
    current.setdefault("mcpServers", {})["superessence"] = server_entry()
    target.write_text(json.dumps(current, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"SUPERESSENCE ajouté à Claude Desktop : {target}")
    print("Redémarrez complètement Claude Desktop pour activer les outils.")


if __name__ == "__main__":
    main()
