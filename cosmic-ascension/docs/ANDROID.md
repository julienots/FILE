# ANDROID — construire et installer l'APK (B9)

Emballage **Capacitor 8** (`android/`, `capacitor.config.json`). Le jeu web (`dist/`) est copié dans l'APK: il tourne hors ligne.

## Prérequis (une fois)
- Node ≥ 22, **JDK 21** (`JAVA_HOME`)
- **Android SDK**: `platform-tools`, `platforms;android-36`, `build-tools;36.0.0` (Android Studio, ou cmdline-tools + `sdkmanager`)
- `export ANDROID_HOME=/chemin/vers/android-sdk` (ou `android/local.properties` → `sdk.dir=…`)

## Construire
```
cd cosmic-ascension
npm install
tools/build-apk.sh            # APK debug   → release/cosmic-ascension-<version>-debug.apk
tools/build-apk.sh release    # APK release → signé avec CA_KEYSTORE… (sinon clé debug)
adb install -r release/cosmic-ascension-0.1.0-debug.apk
```
Le script enchaîne: `vite build` → `npm test` → `npx cap sync android` → Gradle.

Clé de publication (Play Store): `keytool -genkeypair -v -keystore ca-release.jks -alias cosmic -keyalg RSA -keysize 2048 -validity 10000`, puis exporter `CA_KEYSTORE`, `CA_KEYSTORE_PASSWORD`, `CA_KEY_ALIAS`, `CA_KEY_PASSWORD`. **Ne jamais committer la clé.**

## Ce que fait la couche native
- Paysage verrouillé (`sensorLandscape`), plein écran **immersif** (barres système cachées, glisser pour les revoir), encoche utilisée, écran toujours allumé (`MainActivity.java`).
- Bouton retour Android = ferme le panneau ouvert / met en pause (`ui/native.js` → `App.backButton`).
- Mise en arrière-plan = pause + sauvegarde + audio suspendu; retour = barres re-cachées.
- Vibrations via `@capacitor/haptics` (réglage « Vibrations »).
- Icône adaptative + splash générés par `python3 tools/make-icons.py` (PIL).
- Mémoire: `largeHeap`, accélération matérielle; la qualité graphique s'adapte au FPS (A).

## Vérifié / non vérifié
- Vérifié dans le conteneur: `npx cap sync`, build web, tests. **Pas de build Gradle ici** (SDK Android non installé dans l'environnement de la session) → à lancer sur une machine avec le SDK, ou autoriser l'installation du SDK dans la session.
