#!/usr/bin/env bash
# Construit l'APK Android signé de Cubiverse sans Gradle.
# Outils : node/npm, JDK (javac), aapt, dx (dalvik-exchange), zipalign, apksigner, android.jar.
# Sur Ubuntu/Debian : apt-get install aapt dalvik-exchange zipalign apksigner android-sdk-platform-23
set -euo pipefail
cd "$(dirname "$0")"
ANDROID_JAR="${ANDROID_JAR:-/usr/lib/android-sdk/platforms/android-23/android.jar}"
KS="${CUBIVERSE_KEYSTORE:-keystore/cubiverse-release.jks}"
KS_PASS="${CUBIVERSE_KEYSTORE_PASSWORD:-cubiverse-superessence}"
OUT="release/Cubiverse-SuperEssence-Studio.apk"
DX="$(command -v dx || command -v dalvik-exchange)"

echo "▶ Version web"
(cd web && npm run --silent build)

echo "▶ Préparation"
rm -rf build && mkdir -p build/gen build/obj build/dex build/assets/www release
cp -r web/dist/. build/assets/www/

echo "▶ Ressources (R.java)"
aapt package -f -m -J build/gen -M android/AndroidManifest.xml -S android/res -I "$ANDROID_JAR"

echo "▶ Compilation Java"
javac -nowarn --release 8 -classpath "$ANDROID_JAR" -d build/obj $(find android/src build/gen -name '*.java') 2>&1 | grep -v "^warning\|^Note\|^1 warning" || true
test -f build/obj/com/superessence/cubiverse/MainActivity.class

echo "▶ Dex"
"$DX" --dex --min-sdk-version=24 --output=build/dex/classes.dex build/obj

echo "▶ Empaquetage"
aapt package -f -M android/AndroidManifest.xml -S android/res -A build/assets -I "$ANDROID_JAR" -F build/unsigned.apk build/dex
zipalign -f -p 4 build/unsigned.apk build/aligned.apk

echo "▶ Signature"
apksigner sign --ks "$KS" --ks-pass "pass:$KS_PASS" --key-pass "pass:$KS_PASS" --ks-key-alias cubiverse --min-sdk-version 24 --out "$OUT" build/aligned.apk
apksigner verify --print-certs "$OUT" | head -2
ls -lh "$OUT"
