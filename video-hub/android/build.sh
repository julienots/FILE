#!/usr/bin/env bash
# Compile l'APK « Hub vidéo » sans Android Studio ni SDK Google :
# aapt / dalvik-exchange / zipalign / apksigner (paquets Ubuntu-Debian) + jar Android 15 de Robolectric (Maven Central).
#   sudo apt-get install -y aapt apksigner zipalign dalvik-exchange android-sdk-platform-23 openjdk-17-jdk-headless
#   ./build.sh            -> build/HubVideo.apk
set -euo pipefail
cd "$(dirname "$0")"

VERSION=$(sed -n 's/.*android:versionName="\([^"]*\)".*/\1/p' AndroidManifest.xml)
P23=/usr/lib/android-sdk/platforms/android-23/android.jar   # ressources du framework (aapt) + java.*
CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/videohub-android"
ALL="$CACHE/android-all-15.jar"                              # API Android 15 pour la compilation Java
KEYSTORE="${HUB_KEYSTORE:-keystore/hub-release.jks}"
KEYPASS_FILE="keystore/password.txt"

for t in aapt dalvik-exchange zipalign apksigner javac keytool; do
  command -v "$t" >/dev/null || { echo "Outil manquant : $t (voir l'en-tête du script)"; exit 1; }
done
[ -f "$P23" ] || { echo "Manquant : $P23 (paquet android-sdk-platform-23)"; exit 1; }
mkdir -p "$CACHE"
if [ ! -f "$ALL" ]; then
  echo "Téléchargement du jar Android 15 (Robolectric, ~180 Mo)…"
  curl -fsSL -o "$ALL.part" https://repo1.maven.org/maven2/org/robolectric/android-all/15-robolectric-12650502/android-all-15-robolectric-12650502.jar
  mv "$ALL.part" "$ALL"
fi

# Clé de signature : créée au premier build, à CONSERVER (sinon les mises à jour exigent de désinstaller l'appli).
if [ ! -f "$KEYSTORE" ]; then
  mkdir -p "$(dirname "$KEYSTORE")"
  PASS="${HUB_KEYSTORE_PASSWORD:-$(head -c 18 /dev/urandom | base64 | tr -d '/+=')}"
  echo -n "$PASS" > "$KEYPASS_FILE"
  keytool -genkeypair -keystore "$KEYSTORE" -storepass "$PASS" -keypass "$PASS" -alias hub \
    -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Hub video, O=julien.blt.bs" >/dev/null 2>&1
  echo "Clé de signature créée : $KEYSTORE (mot de passe dans $KEYPASS_FILE)"
fi
PASS="${HUB_KEYSTORE_PASSWORD:-$(cat "$KEYPASS_FILE")}"

rm -rf build && mkdir -p build/gen build/classes
# Valeurs pré-remplies facultatives (APK personnel) : HUB_DEFAULT_URL (relais Supabase), HUB_DEFAULT_KEY (clé publique).
# Un APK personnel n'est PAS copié dans release/ (à ne pas versionner).
jesc() { printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'; }
mkdir -p build/gen/com/julien/videohub
cat > build/gen/com/julien/videohub/Defaults.java <<JAVA
package com.julien.videohub;
final class Defaults {
    static final String URL = "$(jesc "${HUB_DEFAULT_URL:-}")";
    static final String KEY = "$(jesc "${HUB_DEFAULT_KEY:-}")";
    private Defaults() {}
}
JAVA
aapt package -f -m -J build/gen -M AndroidManifest.xml -S res -A assets -I "$P23" -F build/res.apk
javac -nowarn -Xlint:-options -source 8 -target 8 -encoding UTF-8 \
  -bootclasspath "$ALL:$P23" -d build/classes \
  $(find build/gen src -name '*.java') 2> >(grep -v "Picked up" >&2)
dalvik-exchange --dex --min-sdk-version=26 --output=build/classes.dex build/classes 2> >(grep -v "Picked up" >&2)
cp build/res.apk build/unsigned.apk
(cd build && zip -q unsigned.apk classes.dex)
zipalign -f 4 build/unsigned.apk build/aligned.apk
apksigner sign --ks "$KEYSTORE" --ks-pass "pass:$PASS" --ks-key-alias hub --out build/HubVideo.apk build/aligned.apk 2> >(grep -v "Picked up" >&2)
apksigner verify build/HubVideo.apk 2> >(grep -v "Picked up" >&2)
if [ -n "${HUB_DEFAULT_KEY:-}" ]; then
  echo "OK (APK personnel, non copié dans release/) : build/HubVideo.apk ($(du -h build/HubVideo.apk | cut -f1))"
else
  mkdir -p release && cp build/HubVideo.apk "release/HubVideo-$VERSION.apk"
  echo "OK : build/HubVideo.apk et release/HubVideo-$VERSION.apk ($(du -h build/HubVideo.apk | cut -f1))"
fi
