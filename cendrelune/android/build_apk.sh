#!/bin/bash
# Construit l'APK hors ligne : page + game.js + three.js + modèles GLB dans les assets.
set -e
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(dirname "$HERE"); OUT=$HERE/build
THREE=${THREE:-$ROOT/../node_modules/three}
PLATFORM=/usr/lib/android-sdk/platforms/android-23/android.jar
rm -rf "$OUT"; mkdir -p "$OUT/assets/www/models" "$OUT/classes"
W=$OUT/assets/www
{ echo '<!doctype html><html lang="fr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover,user-scalable=no"></head><body>'
  sed 's#https://cdn.jsdelivr.net/npm/three@0.147.0/#three/#g' "$ROOT/index.html"; echo '</body></html>'; } > "$W/index.html"
cp "$ROOT/game.js" "$W/"
for f in build/three.min.js examples/js/loaders/GLTFLoader.js examples/js/utils/BufferGeometryUtils.js examples/js/utils/SkeletonUtils.js; do mkdir -p "$W/three/$(dirname $f)"; cp "$THREE/$f" "$W/three/$f"; done
cp "$ROOT/models/"*.glb "$ROOT/models/manifest.json" "$W/models/"
javac -source 8 -target 8 -nowarn -bootclasspath "$PLATFORM" -d "$OUT/classes" "$HERE/src/fr/cendrelune/MainActivity.java" 2>&1 | grep -v warning || true
dalvik-exchange --dex --output="$OUT/classes.dex" "$OUT/classes"
aapt package -f -M "$HERE/AndroidManifest.xml" -S "$HERE/res" -A "$OUT/assets" -I "$PLATFORM" -0 glb -0 arsc -F "$OUT/unsigned.apk"
(cd "$OUT" && aapt add unsigned.apk classes.dex >/dev/null)
zipalign -f 4 "$OUT/unsigned.apk" "$OUT/aligned.apk"
KS=${KEYSTORE:-$HERE/cendrelune.keystore}
[ -f "$KS" ] || keytool -genkeypair -keystore "$KS" -storepass cendrelune -keypass cendrelune -alias cendrelune -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Cendrelune" >/dev/null 2>&1
apksigner sign --ks "$KS" --ks-pass pass:cendrelune --out "$ROOT/Cendrelune.apk" "$OUT/aligned.apk"
apksigner verify "$ROOT/Cendrelune.apk" && ls -la "$ROOT/Cendrelune.apk"
