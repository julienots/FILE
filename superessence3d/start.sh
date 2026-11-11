#!/usr/bin/env bash
cd "$(dirname "$0")"
if [ ! -x .venv/bin/python ]; then echo "Lancez d'abord ./install_linux_mac.sh"; exit 1; fi
exec .venv/bin/python -m superessence "$@"
