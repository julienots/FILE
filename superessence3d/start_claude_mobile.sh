#!/usr/bin/env bash
cd "$(dirname "$0")"
if [ ! -x .venv/bin/python ]; then echo "Lancez d'abord ./install_linux_mac.sh"; exit 1; fi
if ! command -v cloudflared >/dev/null 2>&1; then echo "Installez cloudflared : brew install cloudflared (Mac)"; fi
exec .venv/bin/python -m superessence.mcp_remote "$@"
