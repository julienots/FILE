---
name: android-release
description: Produire un APK/AAB Android installable et léger depuis un conteneur (jeu Godot ou app Kotlin/Compose) : SDK, signature, taille, vérification, distribution. Utiliser quand l'utilisateur veut un APK, tester sur son téléphone, publier sur le Play Store, ou quand un build Android échoue.
---

# Build Android

## Outils (sans Android Studio)
- SDK : `commandlinetools-linux-*_latest.zip` (dl.google.com) → `sdkmanager --sdk_root=… "platform-tools" "build-tools;35.0.0" "platforms;android-35"`.
- JDK 17+ (21 fonctionne avec Godot 4.5). Keystore debug : `keytool -genkeypair -keystore debug.keystore -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Android Debug,O=Android,C=US"`.

## Godot 4.5
- Modèles d'export (1,3 Go) : n'extraire que `templates/android_*` + `version.txt` vers `~/.local/share/godot/export_templates/4.5.1.stable/`.
- `~/.config/godot/editor_settings-4.5.tres` : `export/android/android_sdk_path`, `java_sdk_path`, `debug_keystore(_user/_pass)`.
- `export_presets.cfg` versionné (arm64 seul, immersive, `export_filter="all_resources"` + `exclude_filter` des originaux remplacés par des versions optimisées et des dossiers d'outils/captures).
- `godot --headless --import --path . && godot --headless --path . --export-debug "Android" build/Jeu.apk` (« Verifying APK… DONE » = signé et valide).
- **Budget taille** : lister `unzip -l jeu.apk | sort -n -r | head` ; regrouper par catégorie ; réduire via `process/size_limit` dans les `.import` (1024 archi, 512 normales/ORM), HDRI 2K, maillages simplifiés. Visé : ≤ 400 Mo pour un jeu 3D réaliste, idéalement < 150 Mo.

## App Kotlin/Compose
- `./gradlew assembleRelease` avec `signingConfigs` lisant un keystore hors dépôt (variables d'environnement) ; R8 activé ; `bundleRelease` pour le Play Store.

## Distribution
- > 100 Mo : jamais dans Git. Proposer GitHub Actions (artefact + pré-release) **avec l'accord explicite de l'utilisateur** (workflow = action publique), ou un lien de stockage qu'il fournit.
- Dire clairement : APK debug = installable (« sources inconnues ») mais pas publiable ; sans appareil ni émulateur, l'installation n'est pas testée.
