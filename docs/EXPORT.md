# Exporter un jeu (phase 8)

Un jeu exporté = le **lecteur AGR** (`agr_player`, le moteur avec tous ses plugins, sans l'éditeur)
+ les **données du jeu** :

```
game.json         nom, version, identifiant Android, scène de départ, orientation, cadence...
assets/...        les assets du projet (assets://)
scenes/*.agrscene les scènes enregistrées, converties au format binaire
```

La scène exportée est celle **enregistrée sur le disque** : enregistrez avant d'exporter (la fenêtre
d'export le rappelle).

**Scènes binaires.** À l'export, chaque scène est convertie au format binaire de `SceneSerializer`
(même nom de fichier ; le chargement reconnaît le format à sa signature `AGRSCNB`). Contenu identique au
JSON (composants et champs par nom, références par UUID, instances de prefabs avec leurs modifications),
sans arbre JSON intermédiaire : pour 100 000 entités, ~0,1 s de chargement au lieu de ~0,7 s. La
conversion est **vérifiée** : relue, la scène binaire doit redonner exactement le document JSON du disque ;
(les champs ajoutés depuis l'enregistrement du fichier prennent leur valeur par défaut des deux côtés) ;
sinon (composant inconnu de l'éditeur, prefab introuvable, fichier modifié à la main) la scène est copiée
telle quelle. `ExportReport::binaryScenes` compte les scènes converties. L'éditeur utilise le même format
pour ses instantanés (annuler / rétablir, mode Jeu).

## Textures compressées ETC2 (Android)

*Android › Textures compressées ETC2* (activé par défaut) : à l'export Android, chaque image des assets
(PNG, JPEG, TGA, BMP d'au moins 8 × 8) est convertie en **ETC2 + EAC** (RGBA, 8 bits par pixel au lieu
de 32), avec ses niveaux de mip, dans un conteneur KTX **sous le même nom de fichier** (le chargement
reconnaît le format à sa signature : aucune référence à changer). Sur le téléphone, le GPU (OpenGL ES 3)
lit la texture compressée telle quelle : **4 × moins de mémoire GPU** et des chargements plus légers.
Là où le GPU ne lit pas ETC2 (le rendu Vulkan d'AGR pour l'instant), elle est décodée à la volée, et les
lecteurs CPU (terrain, outils) reçoivent toujours des pixels. *Ne pas compresser* garde certaines images
intactes (par défaut `fonts/**, *height*, *splat*` : données lues en précision). Ligne de commande :
`agr_editor --compress-textures DOSSIER`. Vérifié : le codec (encodeur ETC1/ETC2 + EAC, décodeur complet
avec les modes T, H et planaire) est relu par le décodeur matériel de Mesa en OpenGL et OpenGL ES avec les
bonnes couleurs ; la Vitrine 2D avec ses 91 textures compressées (11 Mo de mémoire GPU en moins) passe
`--autotest` en OpenGL et en Vulkan. Limites : qualité ETC2 (blocs visibles sur les dégradés très fins ou
le pixel art agrandi : l'exclure), pas d'ASTC.

## Filtres : seulement ce qui sert

*Données du jeu › Seulement les assets utilisés* : l'export part des scènes et des fichiers de réglages
(icône, traductions, bus audio, couches physiques, écran de démarrage, `.json` / `.csv` à la racine des
assets) et suit tous les chemins `assets://` qu'ils contiennent — matériaux, textures, shaders, scripts
Lua, graphes de logique, prefabs, machines à états, clips, thèmes —, récursivement ; un modèle emporte
les fichiers à côté de lui (textures, `.bin`, `.mtl`). **Inclure** ajoute des motifs (chemins construits
en jeu : `levels/*.agrscene`, `music/**`), **Exclure** en retire toujours (`*.psd, sources/**`) ; `*` ne
traverse pas les dossiers, `**` si, `?` = un caractère, un motif sans `/` porte sur le nom de fichier.
La Vitrine 2D passe de 5,5 Mo à 0,9 Mo (29 fichiers au lieu de 214) et passe `--autotest` sans fichier
manquant.

## Paquet de données, chiffrement, patchs et DLC

Option *Données du jeu › Un seul fichier* : au lieu de milliers de fichiers, `game/` contient
`game.json` (lisible : nom, orientation...) et **`game.agrpak`**, une archive lue sur place (accès direct,
rien n'est extrait), sur PC comme dans l'APK.

- **Chiffrer (AES-256)** : l'index (les noms de fichiers) et chaque fichier sont chiffrés (AES-256-CTR,
  un nonce par fichier). La clé est créée au premier export dans `keys/data.key` (exclu de git ; **gardez
  une copie** : les patchs doivent utiliser la même) et **écrite dans le lecteur exporté** (un
  emplacement réservé de l'exécutable ou de `libmain.so` ; un ancien lecteur sans emplacement est
  refusé plutôt que de livrer un jeu illisible). Comme pour tous les moteurs, la clé voyage avec le
  jeu : cela empêche d'ouvrir ou d'extraire les assets simplement, pas un attaquant décidé.
- **Patch / DLC** : *Patch / DLC › Exporter un patch* (ou `agr_editor --project P --export patch
  --base game.agrpak --output 001-fix.agrpak`) compare les données actuelles à un export précédent
  (empreintes SHA-256 de l'index) et n'emballe que les fichiers **nouveaux ou modifiés**, chiffrés avec
  la même clé. Le lecteur empile au démarrage `game/patches/*.agrpak` (PC) puis `user://packs/*.agrpak`
  (DLC téléchargés), par ordre alphabétique : le dernier qui contient un fichier gagne. En jeu,
  `agr.loadPack("user://dlc/niveaux.agrpak")` ajoute un paquet sans redémarrer (les assets déjà chargés
  gardent leur version jusqu'à leur rechargement). Un fichier supprimé ne peut pas être retiré par un
  patch (remplacez-le).
- Vérifié de bout en bout : la Vitrine 2D exportée chiffrée (213 fichiers, 5,5 Mo) démarre et passe
  `--autotest` ; dans l'APK, `game.agrpak` est stocké sans compression (accès direct) et la clé est bien écrite dans `libmain.so` ; un patch de 2 scripts modifiés est chargé par-dessus (le nouveau script s'exécute) ; le
  format a été relu par un déchiffreur AES indépendant (Python `cryptography`). Code :
  `engine/core/include/agr/core/Pack.hpp`, `Crypto.hpp` (SHA-256, AES-256 testés sur les vecteurs
  NIST / FIPS-197).

## Depuis l'éditeur

*Fichier › Exporter le jeu...* :

| Réglage | Rôle |
|---|---|
| Cible | PC (dossier), Android : APK de test (debug), APK signé (release), App Bundle (Google Play), **Windows x64** (depuis n'importe quel système) |
| Nom, organisation, version, code de version | Identité ; le **code de version** doit augmenter à chaque envoi sur Google Play |
| Orientation | Paysage, portrait, ou les deux (capteur) |
| Cadence | Celle de l'écran, ou 30 / 60 images/s (30 économise la batterie) |
| Écran allumé, bouton Retour | Garder l'écran allumé pendant le jeu ; le bouton Retour d'Android quitte le jeu |
| Écran de démarrage | Image (`assets://...`), couleur de fond et durée minimale : affichée par le lecteur (toutes plateformes) avant la scène de départ, centrée, proportions gardées |
| Microphone | Permission `RECORD_AUDIO` (jeux qui enregistrent : `agr.audio.startRecording`) |
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
agr_editor --project MonJeu --export windows --output sortie/          # Windows x64, depuis Linux aussi
agr_editor --project MonJeu --export android-apk --output sortie/
AGR_STORE_PASSWORD=... agr_editor --project MonJeu --export android-aab --output sortie/
```

## PC

Le dossier `<Nom>/` contient l'exécutable (renommé d'après le jeu), `game/`, `icon.png` (l'icône du
projet) et, sous Linux, un lanceur `<Nom>.desktop` (catégorie Jeu) qui démarre le jeu depuis son dossier.
Option *Archive .zip* : le dossier est aussi livré en `<Nom>.zip` (`<Nom>-windows.zip` pour Windows),
non compressé, droits d'exécution conservés, prêt à envoyer sur une page de jeu (itch.io, Steam...). Options utiles du
lecteur : `--opengl`, `--gles`, `--vulkan`, `--frames N`, `--autotest` (vérifie que la scène, les
scripts, le rendu, l'interface et l'audio fonctionnent).

## Windows x64

L'export Windows fonctionne depuis Linux ou Windows : il assemble `<Nom>/<Nom>.exe` et `<Nom>/game/` à
partir du **lecteur Windows précompilé**, cherché dans `AGR_EXPORT_TEMPLATES/windows-x64/`,
`export_templates/windows-x64/` à côté de l'éditeur, puis `build/windows-x64/bin/` (preset CMake
`windows-x64`, compilation croisée MinGW-w64 : [BUILDING.md](BUILDING.md)). Aucun compilateur n'est
nécessaire au moment de l'export. Le `.exe` est autonome (DLL système uniquement) et s'ouvre sans
console. **L'icône du jeu** (`assets/icon.png`, sinon le logo AGR) est écrite dans le `.exe` (icône de
l'Explorateur et de la barre des tâches) : le lecteur Windows réserve une ressource d'icône que l'export
remplace par l'image ramenée à 256 × 256 (proportions gardées, marges transparentes) ; vérifiée en la
relisant avec `wrestool` (icoutils). Exemple : la Vitrine 2D exportée pèse 31 Mo (25 Mo de lecteur + 5,5 Mo de données).

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
AGR) classiques et **adaptatives** (Android 8+ : l'image dans la zone sûre d'un premier plan de 108 dp,
sur la couleur de fond choisie dans l'export), puis lance Gradle et copie `Nom-version[-debug].apk` ou `.aab` dans le dossier de sortie.
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
- Pas d'export iOS, Web ni macOS. L'export Windows est produit et vérifié (format PE, dépendances,
  contenu du dossier) mais **n'a pas encore été lancé sur un vrai Windows** ; pas
  d'installateur (l'icône est intégrée au `.exe`).
- Textures : ETC2 seulement (pas d'ASTC) ; pas de découpage des assets (Play Asset Delivery).
- Le projet Gradle est régénéré à chaque export (pas de compilation incrémentale : ~30 s à 1 min).
- Pas de publication automatique sur Google Play ni d'ouverture directe sur un appareil connecté.
