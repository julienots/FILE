#!/usr/bin/env bash
# Construit l'APK sans Gradle : aapt2 + javac + d8 + apksigner.
# Usage : ANDROID_HOME=/chemin/sdk ./build.sh
set -euo pipefail
cd "$(dirname "$0")"
SDK="${ANDROID_HOME:?définir ANDROID_HOME}"
BT="$SDK/build-tools/35.0.0"
JAR="$SDK/platforms/android-35/android.jar"
OUT=build; rm -rf $OUT; mkdir -p $OUT/{res,gen,classes,dex}
"$BT/aapt2" compile --dir res -o $OUT/res/res.zip
"$BT/aapt2" link -I "$JAR" --manifest AndroidManifest.xml -A assets \
   --java $OUT/gen -o $OUT/unsigned.apk $OUT/res/res.zip --auto-add-overlay
javac --release 8 -nowarn -classpath "$JAR" -d $OUT/classes $(find src $OUT/gen -name '*.java') 2>&1 | grep -v 'warning' || true
"$BT/d8" --lib "$JAR" --min-api 26 --output $OUT/dex $(find $OUT/classes -name '*.class')
(cd $OUT/dex && zip -q ../unsigned.apk classes.dex)
"$BT/zipalign" -f -p 4 $OUT/unsigned.apk $OUT/aligned.apk
KS=keystore/bts-ati.jks
[ -f $KS ] || keytool -genkeypair -keystore $KS -alias btsati -keyalg RSA -keysize 2048 -validity 10000 \
   -storepass btsati-cours -keypass btsati-cours -dname "CN=BTS ATI Cours" >/dev/null 2>&1
"$BT/apksigner" sign --ks $KS --ks-pass pass:btsati-cours --key-pass pass:btsati-cours --out BTS-ATI-Cours.apk $OUT/aligned.apk
"$BT/apksigner" verify BTS-ATI-Cours.apk && echo "OK -> $(pwd)/BTS-ATI-Cours.apk"
