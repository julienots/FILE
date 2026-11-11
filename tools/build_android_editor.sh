#!/usr/bin/env bash
# Builds the AGR ENGINE editor for Android (arm64) and packages it as a signed APK.
#   ANDROID_NDK_HOME, ANDROID_HOME and a JDK are required; Gradle downloads its plugins once.
#   tools/build_android_editor.sh [OUTPUT.apk]
# The editor runs as the APK's native library; its resources (fonts, project templates) are
# packed as APK assets and extracted on first launch (editor/app/main.cpp).
set -euo pipefail
cd "$(dirname "$0")/.."
: "${ANDROID_NDK_HOME:?set ANDROID_NDK_HOME to an Android NDK (r27 or newer)}"
: "${ANDROID_HOME:?set ANDROID_HOME to the Android SDK}"
OUT_APK="${1:-dist/AGR-ENGINE-Editor-Android.apk}"
cmake --preset android-editor-arm64
cmake --build --preset android-editor-arm64
TPL=build/editor_templates
rm -rf "$TPL" && mkdir -p "$TPL/android/arm64-v8a"
STRIP="$(ls "$ANDROID_NDK_HOME"/toolchains/llvm/prebuilt/*/bin/llvm-strip | head -1)"
"$STRIP" -o "$TPL/android/arm64-v8a/libmain.so" build/android-editor-arm64/editor_lib/libmain.so
"$STRIP" -o "$TPL/android/arm64-v8a/libSDL3.so" build/android-editor-arm64/bin/libSDL3.so
SDL_SRC=build/android-editor-arm64/_deps/sdl3-src
mkdir -p "$TPL/android/java/org/libsdl/app"
cp "$SDL_SRC"/android-project/app/src/main/java/org/libsdl/app/*.java "$TPL/android/java/org/libsdl/app/"
cp "$SDL_SRC/LICENSE.txt" "$TPL/android/java/SDL-LICENSE.txt"
printf 'AGR ENGINE Android export templates (editor)\nengine %s\nabis arm64-v8a\n' "$(git describe --always --dirty 2>/dev/null || echo unknown)" > "$TPL/android/VERSION"
mkdir -p "$(dirname "$OUT_APK")"
cmake --build --preset debug --target agr_tests
AGR_EXPORT_TEMPLATES="$PWD/$TPL" AGR_EDITOR_APK="$PWD/$OUT_APK" \
    ./build/debug/bin/agr_tests -tc="Android editor APK*"
echo "editor APK: $OUT_APK"
