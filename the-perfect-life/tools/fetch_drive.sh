#!/usr/bin/env bash
# Télécharge les modèles retenus de la bibliothèque Drive de l'auteur et les prépare
# (décompression meshopt, textures 2K, simplification). Requiert : curl, npx/gltf-transform.
set -e
cd "$(dirname "$0")/.."
mkdir -p assets/drive /tmp/tpl_drive
GT="gltf-transform"
command -v $GT >/dev/null || GT="npx -y @gltf-transform/cli@4"
dl() { curl -sL -o "/tmp/tpl_drive/$2.glb" "https://drive.usercontent.google.com/download?id=$1&export=download&confirm=t"; }
dl 1XBZFPcAcCS9cCcNnwz9r1PCHt8OxvBiR realistic_supercar
dl 1ATLj2WDT26T2YpjeNhZhLc2EeObTOCV- realistic_sports_car
dl 1WcDXpKqX2QW2l00-iw_-a3aa3efuRHR0 white_sports_car
dl 1oY5YV9hA9ae-trPnlrnRFU_1Ym8VLszk realistic_tree
for f in realistic_supercar realistic_sports_car; do
  $GT dequantize "/tmp/tpl_drive/$f.glb" "/tmp/tpl_drive/${f}_dq.glb"
  $GT simplify "/tmp/tpl_drive/${f}_dq.glb" "/tmp/tpl_drive/${f}_s.glb" --ratio 0.3 --error 0.0005
  $GT resize "/tmp/tpl_drive/${f}_s.glb" "assets/drive/$f.glb" --width 2048 --height 2048
done
for f in white_sports_car realistic_tree; do
  $GT resize "/tmp/tpl_drive/$f.glb" "/tmp/tpl_drive/${f}_r.glb" --width 2048 --height 2048
  $GT simplify "/tmp/tpl_drive/${f}_r.glb" "assets/drive/$f.glb" --ratio 0.25 --error 0.001
done
ls -la assets/drive
