#!/usr/bin/env bash
# Versions mobiles des modèles lourds (simplification meshoptimizer via gltf-transform).
# Usage : bash tools/optimize_assets.sh   (après fetch_assets.py)
set -e
cd "$(dirname "$0")/.."
mkdir -p assets/optimized
opt() { # source ratio sortie
  gltf-transform simplify "$1" "/tmp/_s.glb" --ratio "$2" --error 0.01 >/dev/null
  gltf-transform resize "/tmp/_s.glb" "assets/optimized/$3.glb" --width 1024 --height 1024 >/dev/null
  echo "$3 ok"
}
opt assets/polyhaven/models/island_tree_01/island_tree_01.gltf 0.04 island_tree_01
opt assets/polyhaven/models/tree_small_02/tree_small_02.gltf 0.03 tree_small_02
opt assets/polyhaven/models/jacaranda_tree/jacaranda_tree.gltf 0.02 jacaranda_tree
opt assets/polyhaven/models/shrub_01/shrub_01.gltf 0.12 shrub_01
opt assets/polyhaven/models/potted_plant_01/potted_plant_01.gltf 0.2 potted_plant_01
opt assets/drive/realistic_tree.glb 0.2 realistic_tree
opt assets/drive/realistic_supercar.glb 0.35 realistic_supercar
opt assets/drive/realistic_sports_car.glb 0.3 realistic_sports_car
opt assets/drive/white_sports_car.glb 0.3 white_sports_car
