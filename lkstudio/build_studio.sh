#!/usr/bin/env bash
# LK Studio — builds the signed APK (no Gradle): web bundle (esbuild), aapt2, javac + d8, zipalign, apksigner.
#   Requirements: Node.js (npm), Android SDK (build-tools 35.0.0, platforms/android-35), a JDK, Python 3 + Pillow.
#   Usage: ANDROID_SDK=/path/to/sdk ./build_studio.sh
set -euo pipefail
cd "$(dirname "$0")"
SDK="${ANDROID_SDK:-${ANDROID_HOME:-/opt/android-sdk}}"
BT="$SDK/build-tools/35.0.0"
PLATFORM="$SDK/platforms/android-35/android.jar"
OUT=build/apk
APK_NAME="${APK_NAME:-LK-Studio.apk}"
KS="${LKS_KEYSTORE:-keystore/lkstudio-local.jks}"
KEY_ALIAS="${LKS_KEY_ALIAS:-lkstudio}"
if [[ -z "${LKS_KEYSTORE_PASSWORD:-}" ]]; then
  mkdir -p keystore
  [[ -f keystore/.password ]] || head -c 24 /dev/urandom | base64 | tr -dc 'A-Za-z0-9' > keystore/.password
  KS_PASS="$(cat keystore/.password)"
  [[ -f "$KS" ]] || keytool -genkeypair -keystore "$KS" -alias "$KEY_ALIAS" -keyalg RSA -keysize 2048 -validity 10000 \
      -storepass "$KS_PASS" -keypass "$KS_PASS" -dname "CN=Superessence Studio, O=Superessence Studio, C=FR" >/dev/null 2>&1
else
  KS_PASS="$LKS_KEYSTORE_PASSWORD"
fi

echo "== 1/5 web app (three.js bundled, works offline)"
( cd web && [[ -d node_modules ]] || npm install --silent )
rm -rf build/www && mkdir -p build/www
cp -r web/static/* build/www/
( cd web && npx esbuild src/app.js --bundle --minify --format=iife --target=chrome80 --loader:.json=json --outfile=../build/www/app.js --log-level=warning )

echo "== 2/5 icons and resources"
python3 -I tools/make_icon.py >/dev/null
rm -rf "$OUT" && mkdir -p "$OUT/res" "$OUT/assets"
cp -r build/www "$OUT/assets/www"
"$BT/aapt2" compile --dir android/res -o "$OUT/res/compiled.zip"
"$BT/aapt2" link -I "$PLATFORM" --manifest android/AndroidManifest.xml --min-sdk-version 26 --target-sdk-version 35 \
  --version-code 1 --version-name 1.0 -A "$OUT/assets" -o "$OUT/unaligned.apk" "$OUT/res/compiled.zip"

echo "== 3/5 Java (WebView host) -> classes.dex"
mkdir -p "$OUT/java/classes" "$OUT/java/dex"
javac -source 11 -target 11 -Xlint:-options -classpath "$PLATFORM" -d "$OUT/java/classes" $(find android/java -name '*.java')
"$BT/d8" --release --min-api 26 --lib "$PLATFORM" --output "$OUT/java/dex" $(find "$OUT/java/classes" -name '*.class')
python3 -c "import zipfile,sys; z=zipfile.ZipFile(sys.argv[1],'a'); z.write(sys.argv[2],'classes.dex',compress_type=zipfile.ZIP_DEFLATED); z.close()" "$OUT/unaligned.apk" "$OUT/java/dex/classes.dex"

echo "== 4/5 align"
"$BT/zipalign" -f -P 16 4 "$OUT/unaligned.apk" "$OUT/aligned.apk"

echo "== 5/5 sign"
"$BT/apksigner" sign --ks "$KS" --ks-key-alias "$KEY_ALIAS" --ks-pass "pass:$KS_PASS" --key-pass "pass:$KS_PASS" --out "$OUT/$APK_NAME" "$OUT/aligned.apk"
"$BT/apksigner" verify --print-certs "$OUT/$APK_NAME" | head -2
ls -la "$OUT/$APK_NAME"
