# God Dome — application Android

Enveloppe Android du jeu `../god-dome-prototype` : une WebView plein écran (WebGL 2, paysage), le jeu est copié dans les assets de l'APK et fonctionne hors connexion.

## Compiler l'APK

Prérequis : JDK 17+, Android SDK (plateforme 35, build-tools 35.0.0).

```bash
cd god-dome-android
echo "sdk.dir=/chemin/vers/android-sdk" > local.properties
./gradlew assembleRelease
```

APK : `app/build/outputs/apk/release/app-release.apk`.

Sans configuration, l'APK est signé avec la clé de debug (installation directe). Pour une clé de publication :
`GODDOME_KEYSTORE=/chemin/cle.jks GODDOME_STORE_PASS=… GODDOME_KEY_PASS=… ./gradlew assembleRelease`. La clé n'est jamais versionnée.
