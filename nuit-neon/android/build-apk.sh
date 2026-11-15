#!/bin/bash
# Fabrique release/NuitNeon.apk sans Android Studio ni Gradle.
# Outils requis : JDK 17+, aapt2, zipalign, apksigner, dalvik-exchange (paquets Ubuntu : aapt apksigner zipalign dalvik-exchange)
# et le android.jar d'une plateforme Android, indiqué par la variable ANDROID_JAR
# (API 34 : la 35 est trop récente pour le aapt2 fourni par Ubuntu).
set -euo pipefail
cd "$(dirname "$0")/.."
: "${ANDROID_JAR:?indiquer le chemin de android.jar (API 34) dans ANDROID_JAR}"
KS=android/keystore/nuit-neon.jks
KS_PASS="${NUITNEON_KEYSTORE_PASSWORD:-nuit-neon-jeu}"
OUT=android/build
rm -rf "$OUT" && mkdir -p "$OUT/res" "$OUT/gen" "$OUT/classes" "$OUT/dex" "$OUT/assets/www"

npm run build --silent
cp -r dist/. "$OUT/assets/www/"

aapt2 compile --dir android/res -o "$OUT/res/res.zip"
aapt2 link -o "$OUT/base.apk" -I "$ANDROID_JAR" --manifest android/AndroidManifest.xml \
  --java "$OUT/gen" -A "$OUT/assets" -0 .wasm -0 .woff2 -0 .glb "$OUT/res/res.zip"
javac -nowarn -Xlint:-options -source 8 -target 8 -bootclasspath "$ANDROID_JAR" -d "$OUT/classes" \
  $(find android/src "$OUT/gen" -name '*.java')
dalvik-exchange --dex --min-sdk-version=26 --output="$OUT/dex/classes.dex" "$OUT/classes"
(cd "$OUT/dex" && zip -q -j ../base.apk classes.dex)
zipalign -f -p 4 "$OUT/base.apk" "$OUT/aligned.apk"
mkdir -p release
apksigner sign --ks "$KS" --ks-pass "pass:$KS_PASS" --ks-key-alias nuitneon --out release/NuitNeon.apk "$OUT/aligned.apk"
rm -f release/NuitNeon.apk.idsig
apksigner verify --verbose release/NuitNeon.apk | head -5
ls -la release/NuitNeon.apk
