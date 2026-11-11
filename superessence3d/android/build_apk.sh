#!/usr/bin/env bash
# Construit l'APK sans Gradle, avec le SDK Android de Debian/Ubuntu :
#   sudo apt install android-sdk android-sdk-platform-23 dalvik-exchange apksigner zipalign
set -euo pipefail
cd "$(dirname "$0")"
SDK=${ANDROID_SDK:-/usr/lib/android-sdk}
JAR="$SDK/platforms/android-23/android.jar"
OUT=build
KS=${SE3D_KEYSTORE:-keystore/superessence3d-release.jks}
PASS=${SE3D_KEYSTORE_PASSWORD:-superessence3d}
rm -rf "$OUT" && mkdir -p "$OUT/classes" "$OUT/gen" "$OUT/assets"
# application autonome (moteur 3D embarqué)
cp -r ../mobile/web "$OUT/assets/web"
echo "[1/5] Ressources (aapt)"
aapt package -f -m -J "$OUT/gen" -M AndroidManifest.xml -S res -I "$JAR"
echo "[2/5] Compilation Java"
javac -nowarn -Xlint:-options --release 8 -encoding UTF-8 -classpath "$JAR" -d "$OUT/classes" \
  $(find src "$OUT/gen" -name '*.java')
echo "[3/5] Conversion Dalvik (dx)"
dalvik-exchange --dex --min-sdk-version=23 --output="$OUT/classes.dex" "$OUT/classes"
echo "[4/5] Empaquetage"
aapt package -f -M AndroidManifest.xml -S res -A "$OUT/assets" -I "$JAR" -F "$OUT/unsigned.apk"
(cd "$OUT" && aapt add unsigned.apk classes.dex >/dev/null)
zipalign -f -p 4 "$OUT/unsigned.apk" "$OUT/aligned.apk"
echo "[5/5] Signature"
if [ ! -f "$KS" ]; then
  keytool -genkeypair -keystore "$KS" -storepass "$PASS" -keypass "$PASS" -alias superessence3d \
    -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Superessence Studio, O=Superessence, C=FR" >/dev/null 2>&1
fi
mkdir -p ../release
apksigner sign --ks "$KS" --ks-pass "pass:$PASS" --key-pass "pass:$PASS" --ks-key-alias superessence3d \
  --out ../release/SUPERESSENCE3D-Mobile.apk "$OUT/aligned.apk"
apksigner verify --print-certs ../release/SUPERESSENCE3D-Mobile.apk | head -2
ls -l ../release/SUPERESSENCE3D-Mobile.apk
