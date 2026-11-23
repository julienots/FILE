# LK Studio — le créateur de mods de LOST KINGDOM

<p align="center"><img src="icon_512.png" width="128" alt="LK Studio"/></p>

LK Studio est une application Android qui sert à **créer des mods pour LOST KINGDOM directement sur le téléphone**, sans ordinateur ni code. On choisit, on règle avec des curseurs, on importe un modèle 3D, on touche « Exporter » : le mod (.zip) est prêt pour le jeu.

- **APK** : `release/LK-Studio.apk` (environ 0,6 Mo). Android 8.0 (API 26) ou plus. Version 1.0.
- **Il faut LOST KINGDOM 3.2.0 ou plus** : les nouvelles créatures et les nouveaux objets utilisent le format de mods 2.0.
- **Tout reste sur le téléphone** : pas de compte, pas de serveur, aucune connexion nécessaire.

## Ce qu'on peut faire

| Onglet | Contenu |
|---|---|
| **Projet** | Nom, auteur, version, couleur et description du mod. Modèles de départ : vide, bestiaire, arsenal, règles du monde. |
| **Créatures** | Nouvelles créatures copiées d'une créature du jeu, qui garde son corps, ses attaques et son intelligence. Réglages : nom, description, région (13 régions, Canyons Écarlates compris), couleur, taille, vie, attaque, niveau, fréquence d'apparition, surtout la nuit, butin (objets et éclats d'étoile), modèle 3D. |
| **Objets** | Nouvelles armes, armures, anneaux et potions copiés d'un objet du jeu. Réglages : nom, rareté (de commun à ancestral), attaque ou défense, vie, critique, dégâts élémentaires, couleur, créature qui le lâche avec sa chance, cadeau au début d'une partie. |
| **3D** | Import de modèles **.glb / .gltf** (Tripo, Meshy, Sketchfab…), réduction du nombre de triangles et **squelette automatique**. On les voit bouger : repos, marche, course, attaque, saut. Modèles possibles : le héros, Kaede, Astra, les villageois, ses créatures, les créatures du jeu, les 12 bâtiments. |
| **Monde** | Vie, force et taille des ennemis, expérience, or, or de départ, vitesse du héros, saut, endurance, montures, gravité, vitesse du temps, heure figée, météo, bombes, pelle. |
| **Modifier le jeu** | Changer un champ d'une créature ou d'un objet du jeu (nom, vie, prix…), plus des lignes libres pour les experts. |
| **Chroniques et PNJ** | Nouvelles pages de Chroniques et nouvelles répliques pour les personnages. |
| **Musiques et sons** | Remplacer une musique ou un bruitage par un fichier .ogg. |
| **Exporter** | Vérification (identifiants en double…), aperçu du `data.txt`, puis création du .zip dans **Téléchargements/LOST KINGDOM Mods**. Boutons Partager et Ouvrir le jeu. On peut aussi rouvrir un mod existant (.zip) pour le modifier. |

## Installer un mod fait avec LK Studio

1. Dans LK Studio : **Exporter → Créer le fichier du mod**.
2. Dans LOST KINGDOM : **Journal → Paramètres → Mods → Importer un mod (.zip)**. Choisir le fichier dans *Téléchargements/LOST KINGDOM Mods*.
3. Redémarrer le jeu.

## Le squelette automatique

C'est une reprise en JavaScript des outils d'import du jeu (`lostkingdom/tools/import_character.py`, `import_quadruped.py`, `import_prop.py`) :
- **Humanoïdes** :
  - le modèle doit être **en pose en T** (bras écartés) ;
  - l'outil trouve la ligne des bras, les épaules, les poignets, les hanches, les genoux et les chevilles, puis place les 21 os du squelette du jeu ;
  - les poids sont répartis **le long de la surface** (algorithme de Dijkstra sur le maillage), avec des règles par zone : une cape suit le torse, jamais les jambes ;
  - les bras sont ensuite baissés en pose de repos.
- **Quadrupèdes** : debout, la tête vers l'avant. Le squelette du jeu (21 os) est étiré sur l'animal ; chaque patte basse ne suit que sa propre jambe.
- **Bâtiments** : un seul os. Le jeu ajuste le modèle à l'emprise du bâtiment.
- **Préparation du modèle** :
  - plusieurs matériaux : jusqu'à 4 textures sont regroupées dans un atlas 2 × 2 ;
  - les triangles mal orientés sont retournés ;
  - la réduction de triangles utilise *meshoptimizer* et respecte les coutures de texture.
- **Fichiers produits** : `.lkm` (format du jeu, voir `lostkingdom/engine/gfx/Model.h`) et `.jpg` (1024 × 1024).

## Vérifié

- **Navigateur sans écran (Chromium, Playwright), à la taille d'un téléphone** :
  - création d'un projet, d'une créature et d'un objet ;
  - import d'un personnage .glb en pose en T, squelette automatique, animations, vue des zones des os ;
  - enregistrement et export du .zip, sans aucune erreur JavaScript.
- **Le .zip exporté dans le jeu (harnais de bureau)** :
  - le mod s'applique avec 0 erreur ;
  - le modèle remplace le héros, texturé et animé (repos, marche, course, attaque) ;
  - le zip se lit avec `ZipInputStream`, la classe qu'utilise le jeu sur Android.
- **Non vérifié** : l'APK n'a jamais été installé sur un téléphone (pas d'appareil ni d'émulateur ici). Le sélecteur de fichiers, l'écriture dans Téléchargements et le partage passent par du code Android standard, mais n'ont pas été essayés.

## Limites

- Formats 3D : .glb (conseillé) et .gltf avec ses textures intégrées. Les modèles compressés en Draco ne sont pas lus (ceux compressés en meshopt, oui). Pas de FBX ni d'OBJ.
- Les nouvelles créatures et les nouveaux objets copient une créature ou un objet du jeu : pas de nouvelles attaques ni de nouvelles quêtes.
- Sons : uniquement des fichiers .ogg (le jeu ne lit que l'OGG Vorbis).

## Compiler

```bash
cd lkstudio
ANDROID_SDK=/chemin/vers/android-sdk ./build_studio.sh     # -> build/apk/LK-Studio.apk
```

- **Prérequis** : Node.js (npm), Android SDK (build-tools 35, platforms/android-35), un JDK, Python 3 avec Pillow.
- **Données du jeu** : `web/src/gamedata.json` (créatures, objets, PNJ, régions, squelettes) est produit par le jeu lui-même : `LK_DUMP_STUDIO=gamedata.json ./build/desk/lk_tests assets` (dans `lostkingdom/`).
- **Structure** : `android/` (activité WebView, fournisseur de partage), `web/src/` (application : `app.js`, `lkm.js` import et squelette, `viewer.js` aperçu 3D, `zip.js`, `store.js`), `web/static/` (page, style, polices OFL).
