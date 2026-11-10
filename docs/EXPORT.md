# Exporter un jeu (phase 8)

Un jeu exporté = le **lecteur AGR** (`agr_player`, le moteur avec tous ses plugins, sans l'éditeur)
+ les **données du jeu** :

```
game.json         nom, version, identifiant Android, scène de départ, orientation, cadence...
assets/...        les assets du projet (assets://)
scenes/*.agrscene les scènes enregistrées
```

La scène exportée est celle **enregistrée sur le disque** : enregistrez avant d'exporter (la fenêtre
d'export le rappelle).

## Depuis l'éditeur

*Fichier › Exporter le jeu...* :

| Réglage | Rôle |
|---|---|
| Cible | PC (dossier), Android : APK de test (debug), APK signé (release), App Bundle (Google Play) |
| Nom, organisation, version, code de version | Identité ; le **code de version** doit augmenter à chaque envoi sur Google Play |
| Orientation | Paysage, portrait, ou les deux (capteur) |
| Cadence | Celle de l'écran, ou 30 / 60 images/s (30 économise la batterie) |
| Écran allumé, bouton Retour | Garder l'écran allumé pendant le jeu ; le bouton Retour d'Android quitte le jeu |
| Identifiant Android | `com.studio.jeu` (par défaut : organisation + nom) |
| API minimale / cible, processeurs | Android 8.0 (API 26) minimum ; `arm64-v8a` (téléphones), `x86_64` (émulateurs, Chromebooks) |
| Signature | Keystore (fichier), alias, mots de passe ; *Créer un keystore* (`keys/release.p12`) |

Les réglages sont enregistrés dans le projet ; **les mots de passe jamais** (ni sur le disque, ni sur une
ligne de commande : ils passent à Gradle par l'environnement). Gardez une copie du keystore en lieu sûr :
sans lui, impossible de publier une mise à jour. Les nouveaux projets ont un `.gitignore` qui exclut
`build/`, `user/` et `keys/`.

L'export tourne en arrière-plan, son journal s'affiche dans la fenêtre ; le résultat est écrit dans
`build/desktop/` ou `build/android/` du projet (ou le dossier choisi). Un dossier de sortie n'est remplacé
que s'il a été créé par un export précédent.

## En ligne de commande (intégration continue)

```bash
agr_editor --create-project MonJeu --template showcase2d          # optionnel : créer depuis un modèle
agr_editor --project MonJeu --export desktop --output sortie/
agr_editor --project MonJeu --export android-apk --output sortie/
AGR_STORE_PASSWORD=... agr_editor --project MonJeu --export android-aab --output sortie/
```

## PC

Le dossier `<Nom>/` contient l'exécutable (renommé d'après le jeu) et `game/`. Options utiles du
lecteur : `--opengl`, `--gles`, `--vulkan`, `--frames N`, `--autotest` (vérifie que la scène, les
scripts, le rendu, l'interface et l'audio fonctionnent).

## Android

Prérequis sur la machine qui exporte :

| Outil | Détail |
|---|---|
| SDK Android | `ANDROID_HOME` ; plateforme de l'API cible (`sdkmanager "platforms;android-35"`) |
| Gradle 8.13+ et un JDK 17+ | `gradle` dans le `PATH` (ou `AGR_GRADLE`) ; `keytool` du JDK pour créer un keystore |
| Modèles d'export | `export_templates/` à côté de l'éditeur, ou `AGR_EXPORT_TEMPLATES` ([BUILDING.md](BUILDING.md)) |
| Réseau | au premier export, Gradle télécharge le plugin Android (8.13.2) |

L'export génère un **projet Gradle complet** (`build/android/gradle/`, ouvrable dans Android Studio) :
manifeste (orientation, permission `VIBRATE`, catégorie jeu), activité `AgrActivity` (SDL), bibliothèques
natives, données du jeu dans les assets de l'APK, icônes (`assets/icon.png` du projet, sinon le logo
AGR), puis lance Gradle et copie `Nom-version[-debug].apk` ou `.aab` dans le dossier de sortie.
Installer un APK de test : `adb install -r build/android/Nom-1.0.0-debug.apk`.

Vérifié dans la chaîne de tests : un APK de test de la Vitrine 2D (13 Mo pour arm64-v8a), un AAB et un
APK signés avec la clé du projet, contrôlés par `apksigner`, `aapt2` et `zipalign` (pages de 16 Ko).

## Sur l'appareil

| Fonction | Détail |
|---|---|
| Rendu | Vulkan si disponible, sinon OpenGL ES 3.0 ; la surface est libérée en arrière-plan et recréée au retour |
| Données | lues dans l'APK ; sauvegardes (`user://`, `agr.save`) dans le stockage interne de l'application |
| Arrière-plan | son suspendu, sauvegarde écrite, rendu arrêté |
| Écran | plein écran, orientation du projet, zones sûres (encoche) respectées par l'interface |
| Capteurs, vibration | accéléromètre, gyroscope, vibreur ([SCRIPTING.md](SCRIPTING.md)) |

## Limites actuelles

- **Rien n'a encore tourné sur un téléphone réel ni sur un émulateur** : l'environnement de développement
  n'en dispose pas (pas de virtualisation matérielle pour l'émulateur). La compilation, l'assemblage, la
  signature et le contenu des APK / AAB sont vérifiés ; le démarrage, les performances, la mémoire et la
  consommation sur appareil restent à mesurer.
- Pas d'export iOS, Web ni macOS ; l'export PC produit un exécutable pour le système de l'éditeur.
- Pas d'écran de démarrage personnalisé, d'icône adaptative Android (icône simple), de compression de
  textures (ETC2 / ASTC) ni de découpage des assets (Play Asset Delivery).
- Le projet Gradle est régénéré à chaque export (pas de compilation incrémentale : ~30 s à 1 min).
- Pas de publication automatique sur Google Play ni d'ouverture directe sur un appareil connecté.
