#!/usr/bin/env bash
# Personnages provisoires animés (modèles d'exemple three.js, rig Mixamo).
set -e
cd "$(dirname "$0")/.."
mkdir -p assets/thirdparty
for m in Michelle Soldier; do
  curl -sL -o "assets/thirdparty/$m.glb" "https://raw.githubusercontent.com/mrdoob/three.js/r170/examples/models/gltf/$m.glb"
done
ls -la assets/thirdparty
