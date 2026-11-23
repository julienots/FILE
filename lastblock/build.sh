#!/usr/bin/env bash
# Compile l'APK de THE LAST BLOCK sans Gradle : aapt2 + javac + dx + zipalign + apksigner.
# Prérequis (Ubuntu/Debian) : apt install aapt apksigner zipalign dalvik-exchange openjdk-17-jdk
# et un android.jar (API 33) : ANDROID_JAR=/chemin/android.jar ./build.sh
set -euo pipefail
cd "$(dirname "$0")"
ROOT=$(pwd)
ANDROID_JAR=${ANDROID_JAR:-/opt/abuild/android.jar}
KEYSTORE=${LASTBLOCK_KEYSTORE:-$ROOT/keystore/lastblock-release.jks}
KS_PASS=${LASTBLOCK_KEYSTORE_PASSWORD:-lastblock-superessence}
KEY_ALIAS=${LASTBLOCK_KEY_ALIAS:-lastblock}
OUT=$ROOT/build
rm -rf "$OUT" && mkdir -p "$OUT"/{res,gen,classes,dex,apk}

echo "▸ Ressources"
aapt2 compile --dir android/res -o "$OUT/res/res.zip"
aapt2 link -o "$OUT/apk/base.apk" -I "$ANDROID_JAR" --manifest android/AndroidManifest.xml \
  --java "$OUT/gen" --min-sdk-version 26 --target-sdk-version 33 --version-code 1 --version-name 1.0.0 \
  "$OUT/res/res.zip"

echo "▸ Java"
javac -nowarn -source 8 -target 8 -bootclasspath "$ANDROID_JAR" -classpath "$ANDROID_JAR" \
  -d "$OUT/classes" $(find android/src "$OUT/gen" -name '*.java') 2>&1 | grep -v "bootstrap\|source value 8\|target value 8\|To suppress\|^warning" || true
test -f "$OUT/classes/com/superessence/lastblock/MainActivity.class"

echo "▸ Dex"
dalvik-exchange --dex --min-sdk-version=26 --output="$OUT/dex/classes.dex" "$OUT/classes"

echo "▸ Assets du jeu"
mkdir -p "$OUT/apk/assets/www"
cp -r web/. "$OUT/apk/assets/www/"
cp "$OUT/dex/classes.dex" "$OUT/apk/"
rm -rf "$OUT/apk/assets/www/dev"
(cd "$OUT/apk" && zip -q -r base.apk classes.dex assets -x '*.DS_Store')

echo "▸ Alignement et signature"
zipalign -f -p 4 "$OUT/apk/base.apk" "$OUT/aligned.apk"
if [ ! -f "$KEYSTORE" ]; then
  mkdir -p "$(dirname "$KEYSTORE")"
  keytool -genkeypair -v -keystore "$KEYSTORE" -alias "$KEY_ALIAS" -keyalg RSA -keysize 2048 -validity 10000 \
    -storepass "$KS_PASS" -keypass "$KS_PASS" -dname "CN=SuperEssence Studio, O=SuperEssence Studio, C=FR" >/dev/null 2>&1
fi
apksigner sign --ks "$KEYSTORE" --ks-pass "pass:$KS_PASS" --key-pass "pass:$KS_PASS" --ks-key-alias "$KEY_ALIAS" \
  --min-sdk-version 26 --out release/TheLastBlock-SuperEssence.apk "$OUT/aligned.apk"
apksigner verify --min-sdk-version 26 release/TheLastBlock-SuperEssence.apk
ls -lh release/TheLastBlock-SuperEssence.apk
echo "✔ APK : release/TheLastBlock-SuperEssence.apk"
