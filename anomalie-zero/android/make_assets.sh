#!/bin/bash
# Copie le jeu + three.js + modèles dans les assets Android (fonctionne hors ligne)
set -e
cd "$(dirname "$0")"
T=${THREE_DIR:-node_modules/three}
OUT=app/src/main/assets
rm -rf $OUT && mkdir -p $OUT/lib/addons/loaders $OUT/models
cp ../models/*.json $OUT/models/
cp $T/build/three.module.js $OUT/lib/
for d in postprocessing shaders utils environments; do cp -r $T/examples/jsm/$d $OUT/lib/addons/; done
cp $T/examples/jsm/loaders/GLTFLoader.js $OUT/lib/addons/loaders/
{ echo '<!doctype html><html lang="fr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover,user-scalable=no">';
  sed -e 's#https://cdn.jsdelivr.net/npm/three@0.160.0/build/three.module.js#./lib/three.module.js#' -e 's#https://cdn.jsdelivr.net/npm/three@0.160.0/examples/jsm/#./lib/addons/#' ../index.html;
  echo '</html>'; } > $OUT/index.html
grep -q './lib/three.module.js' $OUT/index.html
