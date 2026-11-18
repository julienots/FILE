#!/usr/bin/env bash
# Installe OpenJarvis (https://github.com/open-jarvis/OpenJarvis) depuis les sources,
# avec Ollama + un petit modèle local, puis applique la config de ce dossier.
# Usage : bash openjarvis/install.sh   (macOS, Linux, WSL2)
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
DEST="${OPENJARVIS_DIR:-$HOME/OpenJarvis}"
MODEL="${OPENJARVIS_MODEL:-qwen3.5:4b}"

command -v git >/dev/null || { echo "git est requis"; exit 1; }
command -v uv  >/dev/null || curl -LsSf https://astral.sh/uv/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"

if [ -d "$DEST/.git" ]; then git -C "$DEST" pull --ff-only
else git clone --depth 1 https://github.com/open-jarvis/OpenJarvis.git "$DEST"; fi

cd "$DEST"
uv sync --extra server

if ! command -v ollama >/dev/null; then
  if [ "$(uname)" = "Darwin" ]; then echo "Installe Ollama : https://ollama.com/download"; exit 1; fi
  curl -fsSL https://ollama.com/install.sh | sh
fi
pgrep -x ollama >/dev/null || (ollama serve >/dev/null 2>&1 &) ; sleep 3
ollama pull "$MODEL"

mkdir -p "$HOME/.openjarvis"
[ -f "$HOME/.openjarvis/config.toml" ] && cp "$HOME/.openjarvis/config.toml" "$HOME/.openjarvis/config.toml.bak"
sed "s/^default_model = .*/default_model = \"$MODEL\"/" "$HERE/config.toml" > "$HOME/.openjarvis/config.toml"

uv run jarvis doctor || true
echo
echo "OpenJarvis est prêt. Lance :  cd $DEST && uv run jarvis"
