#!/usr/bin/env bash
# Builds the Android export templates of AGR ENGINE: the game player (libmain.so) and SDL
# (libSDL3.so) for each ABI, stripped, plus SDL's Java activity sources. The editor's Android
# export packages them with the game data (no C++ compilation when exporting a game).
#
#   ANDROID_NDK_HOME=/path/to/ndk tools/build_android_templates.sh [OUTPUT_DIR] [ABI...]
#   default OUTPUT_DIR: build/export_templates ; default ABIs: arm64-v8a x86_64
set -euo pipefail
cd "$(dirname "$0")/.."
: "${ANDROID_NDK_HOME:?set ANDROID_NDK_HOME to an Android NDK (r27 or newer)}"
OUT="${1:-build/export_templates}"
shift || true
ABIS=("$@")
[ ${#ABIS[@]} -eq 0 ] && ABIS=(arm64-v8a x86_64)
STRIP="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip"
[ -x "$STRIP" ] || STRIP="$(ls "$ANDROID_NDK_HOME"/toolchains/llvm/prebuilt/*/bin/llvm-strip | head -1)"

for abi in "${ABIS[@]}"; do
    case "$abi" in
        arm64-v8a) preset=android-arm64 ;;
        x86_64) preset=android-x86_64 ;;
        *) echo "unsupported ABI: $abi" >&2; exit 1 ;;
    esac
    cmake --preset "$preset"
    cmake --build --preset "$preset"
    mkdir -p "$OUT/android/$abi"
    "$STRIP" -o "$OUT/android/$abi/libmain.so" "build/$preset/bin/libmain.so"
    "$STRIP" -o "$OUT/android/$abi/libSDL3.so" "build/$preset/bin/libSDL3.so"
done

# SDL's Java side (same SDL version as the native libraries).
SDL_SRC="$(ls -d build/android-*/_deps/sdl3-src | head -1)"
rm -rf "$OUT/android/java"
mkdir -p "$OUT/android/java/org/libsdl/app"
cp "$SDL_SRC"/android-project/app/src/main/java/org/libsdl/app/*.java "$OUT/android/java/org/libsdl/app/"
cp "$SDL_SRC/LICENSE.txt" "$OUT/android/java/SDL-LICENSE.txt"
echo "AGR ENGINE Android export templates" > "$OUT/android/VERSION"
echo "engine $(git describe --always --dirty 2>/dev/null || echo unknown)" >> "$OUT/android/VERSION"
echo "abis ${ABIS[*]}" >> "$OUT/android/VERSION"
du -sh "$OUT/android"/*
