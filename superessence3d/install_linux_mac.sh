#!/usr/bin/env bash
# SUPERESSENCE 3D STUDIO — installation (macOS / Linux)
set -euo pipefail
cd "$(dirname "$0")"
echo "=== SUPERESSENCE 3D STUDIO : installation ==="
if ! command -v uv >/dev/null 2>&1; then
  echo "[1/4] Installation de l'outil « uv » (gestionnaire Python gratuit)…"
  curl -LsSf https://astral.sh/uv/install.sh | sh
  export PATH="$HOME/.local/bin:$PATH"
fi
echo "[2/4] Environnement Python 3.13…"
uv venv --python 3.13 .venv
echo "[3/4] Blender (module bpy, ~400 Mo) et composants…"
uv pip install --python .venv/bin/python -e . pytest
echo "[4/4] Configuration de Claude Desktop…"
.venv/bin/python scripts/make_claude_config.py --install
echo
echo "Installation terminée. Lancez : ./start.sh"
