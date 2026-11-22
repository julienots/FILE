#!/usr/bin/env bash
# Compile l'APK sans Gradle : aapt (ressources), javac, dx (dex), zipalign, apksigner.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$HERE")"
ANDROID_JAR="${ANDROID_JAR:-/opt/android-34.jar}"
OUT="$HERE/build"; rm -rf "$OUT"; mkdir -p "$OUT/gen" "$OUT/classes" "$OUT/assets"
cp -r "$ROOT/www" "$OUT/assets/www"
aapt package -f -m -J "$OUT/gen" -M "$HERE/AndroidManifest.xml" -S "$HERE/res" -A "$OUT/assets" \
  -I "$ANDROID_JAR" -F "$OUT/unsigned.apk" --min-sdk-version 21 --target-sdk-version 34 -0 html -0 js -0 css -0 json
javac -nowarn --release 8 -cp "$ANDROID_JAR" -d "$OUT/classes" $(find "$HERE/src" "$OUT/gen" -name '*.java') 2>&1 | grep -v "obsolete\|warning" || true
dalvik-exchange --dex --min-sdk-version=21 --output="$OUT/classes.dex" "$OUT/classes"
(cd "$OUT" && zip -q unsigned.apk classes.dex)
zipalign -f -p 4 "$OUT/unsigned.apk" "$OUT/aligned.apk"
KS="${KEYSTORE:-$HERE/doubledestin.jks}"
if [ ! -f "$KS" ]; then
  keytool -genkeypair -keystore "$KS" -alias doubledestin -keyalg RSA -keysize 2048 -validity 10000 \
    -storepass superessence -keypass superessence -dname "CN=SuperEssence Studio, O=SuperEssence, C=FR" >/dev/null 2>&1
fi
apksigner sign --ks "$KS" --ks-pass pass:superessence --key-pass pass:superessence --ks-key-alias doubledestin \
  --min-sdk-version 21 --out "$ROOT/DoubleDestin-SuperEssence.apk" "$OUT/aligned.apk"
apksigner verify --verbose "$ROOT/DoubleDestin-SuperEssence.apk" | head -4
ls -la "$ROOT/DoubleDestin-SuperEssence.apk"
