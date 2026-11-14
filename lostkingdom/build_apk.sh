#!/usr/bin/env bash
# LOST KINGDOM — builds the signed Android APK with the official SDK/NDK command-line tools.
#   Requirements: Android SDK (build-tools 35.0.0, platforms/android-35, cmake 4.1.2) and NDK r29.
#   Usage: ANDROID_SDK=/path/to/sdk ./build_apk.sh [--smoke]
set -euo pipefail
cd "$(dirname "$0")"
SDK="${ANDROID_SDK:-${ANDROID_HOME:-/opt/android-sdk}}"
NDK="${ANDROID_NDK:-$SDK/ndk/29.0.14206865}"
BT="$SDK/build-tools/35.0.0"
PLATFORM="$SDK/platforms/android-35/android.jar"
CMAKE="$SDK/cmake/4.1.2/bin/cmake"
NINJA="$SDK/cmake/4.1.2/bin/ninja"
ABIS="${ABIS:-arm64-v8a armeabi-v7a x86_64}"
OUT=build/apk
APK_NAME="${APK_NAME:-LOST-KINGDOM-SUPERESSENCE.apk}"
# Signing: provide LK_KEYSTORE / LK_KEYSTORE_PASSWORD / LK_KEY_ALIAS for your release key.
# Without them a local key is generated once in keystore/ (git-ignored, never committed).
KS="${LK_KEYSTORE:-keystore/lostkingdom-local.jks}"
KEY_ALIAS="${LK_KEY_ALIAS:-lostkingdom}"
if [[ -z "${LK_KEYSTORE_PASSWORD:-}" ]]; then
  mkdir -p keystore
  [[ -f keystore/.password ]] || head -c 24 /dev/urandom | base64 | tr -dc 'A-Za-z0-9' > keystore/.password
  KS_PASS="$(cat keystore/.password)"
  if [[ ! -f "$KS" ]]; then
    keytool -genkeypair -keystore "$KS" -alias "$KEY_ALIAS" -keyalg RSA -keysize 2048 -validity 10000 \
      -storepass "$KS_PASS" -keypass "$KS_PASS" -dname "CN=Superessence Studio, O=Superessence Studio, C=FR" >/dev/null 2>&1
  fi
else
  KS_PASS="$LK_KEYSTORE_PASSWORD"
fi
EXTRA="-DLK_SMOKE=OFF"  # always explicit: a stale CMake cache must never ship the smoke test
[[ "${1:-}" == "--smoke" ]] && EXTRA="-DLK_SMOKE=ON"

rm -rf "$OUT" && mkdir -p "$OUT/lib" "$OUT/res"

echo "== 1/5 native code (AGR Engine + game) for: $ABIS"
for ABI in $ABIS; do
  B=build/android-$ABI
  "$CMAKE" -S . -B "$B" -G Ninja -DCMAKE_MAKE_PROGRAM="$NINJA" \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$ABI" -DANDROID_PLATFORM=android-26 -DANDROID_STL=c++_static \
    -DCMAKE_BUILD_TYPE=Release $EXTRA > "$B.cmake.log"
  "$CMAKE" --build "$B" -j"$(nproc)"
  mkdir -p "$OUT/lib/$ABI"
  "$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" --strip-unneeded -o "$OUT/lib/$ABI/liblostkingdom.so" "$B/liblostkingdom.so"
done

echo "== 2/5 resources"
"$BT/aapt2" compile --dir android/src/main/res -o "$OUT/res/compiled.zip"

echo "== 3/5 link manifest + resources + assets"
"$BT/aapt2" link -I "$PLATFORM" --manifest android/src/main/AndroidManifest.xml \
  --min-sdk-version 26 --target-sdk-version 35 --version-code 8 --version-name 1.6.1 \
  -A assets -0 ogg -0 png -o "$OUT/unaligned.apk" "$OUT/res/compiled.zip"

echo "== 4/5 add native libraries (stored, 16 KB page aligned)"
python3 - "$OUT" <<'PY'
import sys, zipfile, os
out = sys.argv[1]
with zipfile.ZipFile(os.path.join(out, "unaligned.apk"), "a") as z:
    for root, _, files in os.walk(os.path.join(out, "lib")):
        for f in files:
            p = os.path.join(root, f)
            z.write(p, os.path.relpath(p, out), compress_type=zipfile.ZIP_STORED)
PY
"$BT/zipalign" -P 16 -f 4 "$OUT/unaligned.apk" "$OUT/aligned.apk"

echo "== 5/5 sign (APK Signature Scheme v2/v3)"
"$BT/apksigner" sign --ks "$KS" --ks-pass "pass:$KS_PASS" --ks-key-alias "$KEY_ALIAS" --key-pass "pass:$KS_PASS" --out "$OUT/$APK_NAME" "$OUT/aligned.apk"
"$BT/apksigner" verify --verbose "$OUT/$APK_NAME" | head -5
"$BT/zipalign" -c -P 16 -v 4 "$OUT/$APK_NAME" | tail -1
ls -la "$OUT/$APK_NAME"
