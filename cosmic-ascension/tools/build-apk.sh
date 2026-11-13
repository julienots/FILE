#!/usr/bin/env bash
# Construit l'APK Android de Cosmic Ascension.  Usage: tools/build-apk.sh [debug|release]
# Prérequis: Node ≥ 22, JDK 21, Android SDK (platform 36, build-tools 36) — ANDROID_HOME ou android/local.properties.
# Release: signé avec la clé indiquée par CA_KEYSTORE / CA_KEYSTORE_PASSWORD / CA_KEY_ALIAS / CA_KEY_PASSWORD (sinon: clé debug).
set -euo pipefail
cd "$(dirname "$0")/.."
MODE="${1:-debug}"
SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
if [ -z "$SDK" ] && ! grep -q '^sdk.dir=' android/local.properties 2>/dev/null; then
  echo "✗ Android SDK introuvable: exporte ANDROID_HOME ou écris sdk.dir=… dans android/local.properties (voir docs/ANDROID.md)"; exit 1
fi
[ -n "$SDK" ] && echo "sdk.dir=$SDK" > android/local.properties
npm run build
npm test
npx cap sync android
cd android
if [ "$MODE" = release ]; then ./gradlew --no-daemon assembleRelease; OUT=app/build/outputs/apk/release/app-release.apk
else ./gradlew --no-daemon assembleDebug; OUT=app/build/outputs/apk/debug/app-debug.apk; fi
cd ..
mkdir -p release
VER=$(node -p "require('./package.json').version")
cp "android/$OUT" "release/cosmic-ascension-$VER-$MODE.apk"
ls -lh "release/cosmic-ascension-$VER-$MODE.apk"
echo "✔ APK: release/cosmic-ascension-$VER-$MODE.apk   (installer: adb install -r <fichier>)"
