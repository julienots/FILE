# LOST KINGDOM

*A world waiting to be discovered.* Created by **SUPERESSENCE STUDIO**, powered by **AGR ENGINE**.

<p align="center"><img src="branding/key_art.png" width="720" alt="LOST KINGDOM, key art"/></p>

LOST KINGDOM est un action-RPG en monde ouvert continu, en 2.5D (caméra 3D orbitale au-dessus du héros), pour smartphones Android. Il est écrit entièrement en **C++17 sur OpenGL ES 3.0** avec un moteur maison, l'AGR Engine. Il n'utilise ni Unity, ni Unreal, ni Godot, ni Java/Kotlin : l'application est une `NativeActivity` pure.

- **APK** : `release/LOST-KINGDOM-SUPERESSENCE.apk`, à la racine du dépôt. Signé en v2/v3, aligné à 16 Ko, ABIs arm64-v8a, armeabi-v7a et x86_64, Android 8.0 (API 26) ou plus, OpenGL ES 3.0 requis. Version 1.2.0 (versionCode 3). Taille : environ 22 Mo. SHA-256 : `57934a0b0784925411a80cc0a763656892803bb3d0293cee8421dd4f5192b691`.
- **Langue du jeu** : français.

> **Honnêteté avant tout.** Ce document distingue ce qui est implémenté et vérifié, ce qui est implémenté mais non vérifié sur un appareil réel, et ce qui manque. Lisez la section *Limites connues* avant tout.

### Nouveautés 1.2.0

- **Nouveau héros 3D.** Le modèle a été généré avec Tripo (texturé, environ 10 000 triangles), puis importé par `tools/import_character.py`. Comme il n'avait pas de squelette, l'outil :
  - trouve automatiquement les articulations dans le maillage ;
  - calcule les poids de peau sur le squelette humanoïde du jeu ;
  - fait passer les bras de la pose en T à la pose de repos.

  Toutes les animations existantes (marche, course, attaques, roulade, monture) s'appliquent donc au modèle. L'arme reste tenue en main.
- **Limites du modèle importé.**
  - L'armure ne change plus visuellement avec l'équipement, car elle est peinte dans la texture.
  - La cape fait partie du maillage et suit le torse, sans physique de tissu.
  - Le réglage **Réglages → Héros 3D détaillé** permet de revenir au héros procédural, dont l'armure change avec l'équipement.

### Nouveautés 1.1.0 (suite aux retours du premier test sur téléphone)

- **Commandes tactiles réparées.**
  - Les appuis (attaque, esquive, saut, sorts, potion, interaction, monture), la rotation de caméra et le pincement pour zoomer étaient produits *après* la mise à jour du héros, puis effacés en fin d'image : ils étaient perdus. Ils sont maintenant traités avant la simulation.
  - Un appui très bref sur Attaque, Esquive ou Parade (doigt posé puis levé pendant la même image) est désormais pris en compte.
  - Un appui pendant une animation est mémorisé environ 0,4 s (attaque, combo) ou 0,3 s (esquive), puis exécuté dès que possible.
  - Un doigt n'active qu'un seul bouton, le plus proche. La zone tactile est légèrement plus grande que le bouton.
- **Boutons replacés.**
  - Nouvelle disposition en arcs autour de l'attaque, sans chevauchement (Saut et Interagir se superposaient) et à l'écart des bords de l'écran.
  - La disposition est vérifiée par des tests pour 5 formats d'écran, de 4:3 à 21:9.
  - Les dispositions enregistrées par la version 1.0.0 sont remplacées automatiquement.
  - Les boutons sans effet (emplacement de sort vide, pas de monture, rien avec quoi interagir) sont masqués au lieu d'être affichés sans réagir.
- **Taille de l'écran suivie à chaque image.** Avant, si la fenêtre changeait de taille après le démarrage (masquage de la barre de navigation par le mode immersif, rotation), l'interface gardait l'ancienne échelle et les appuis tombaient à côté des boutons.
- **Flèches de la carte corrigées.**
  - Sur la minimap, la flèche du joueur pointait à l'envers et les repères tournaient dans le mauvais sens par rapport à l'image.
  - La flèche de direction de la quête était inversée.
  - Un repère « N » indique maintenant le nord. Tout cela est vérifié par des tests.
- **Rendu plus réaliste.**
  - Arbres : volumes de feuillage doux, couronne de feuilles qui casse la silhouette.
  - Couleurs : prairies naturelles (vert profond, olive, paille), forêt d'Elderwood moins fluorescente.
  - Terrain : texture sans répétition visible.
  - Eau : plus claire, avec des stries de courant dans les rivières.
  - Caméra : plus basse, champ de vision plus large.
  - Ajustement de l'étalonnage.

---

## Sommaire
1. [Captures](#captures)
2. [Contenu du jeu](#contenu-du-jeu)
3. [État des fonctionnalités](#état-des-fonctionnalités)
4. [Comment le jeu a été vérifié](#comment-le-jeu-a-été-vérifié)
5. [Limites connues](#limites-connues)
6. [Compiler](#compiler)
7. [Architecture](#architecture)
8. [Contrôles](#contrôles)

---

## Captures

Ces images sont prises dans le harnais de bureau : c'est le code réel du jeu, rendu par le rasteriseur logiciel Mesa, pas sur un téléphone.

| | |
|---|---|
| ![Écran titre](docs/screenshots/02_title.jpg) | ![Village de Brindlemere](docs/screenshots/04_village_day.jpg) |
| ![Dialogue](docs/screenshots/05_dialog.jpg) | ![Équipement](docs/screenshots/07_equipment.jpg) |
| ![Combat](docs/screenshots/13_combat.jpg) | ![Monture](docs/screenshots/15_riding.jpg) |
| ![Ashen Lands](docs/screenshots/17_ashen.jpg) | ![Elderwood la nuit](docs/screenshots/18_elderwood_night.jpg) |
| ![Introduction de boss](docs/screenshots/23_boss_intro.jpg) | ![Combat de boss](docs/screenshots/24_boss_fight.jpg) |
| ![Donjon](docs/screenshots/25_dungeon.jpg) | ![Éditeur de commandes](docs/screenshots/28_layout_editor.jpg) |

---

## Contenu du jeu

| Domaine | Contenu |
|---|---|
| **Monde** | Une carte continue de 3 072 m × 3 072 m, sans écran de chargement entre les régions. Six régions : **Greenhaven**, **Frostheim**, **Ashen Lands**, **Elderwood**, **Kingdom of Shadows** et **High Crowns**. Le terrain est façonné par région, avec rivières, lacs, glace et lave, routes, villages, châteaux, ruines, grottes, tours, sanctuaires, camps, mines, fermes et pierres de passage. Le monde est streamé en chunks de 32 m avec 3 niveaux de détail, générés sur des threads de travail. |
| **Héros** | Squelette animé, blend de locomotion, machine à états, transitions, masques haut et bas du corps, IK des pieds sur le terrain, look-at, cape et cheveux simulés (Verlet). L'équipement change l'apparence : casques, armures, épaulières, gantelets, jambières, capes, armes. |
| **Combat** | Types d'armes : épée, épée longue, dague, hache, marteau, lance, arc, arbalète, bâton, bouclier (plus pioche, hache de bûcheron, canne à pêche, torche), combos, attaque lourde, parade avec fenêtre de timing, esquive avec invulnérabilité, éléments (feu, glace, foudre, nature, ombre, lumière, arcane), faiblesses et résistances, traînées d'armes, particules, hit-stop, vibrations. |
| **Ennemis** | **27 types d'ennemis** avec IA (patrouille, alerte, poursuite, encerclement, attaques télégraphiées, fuite, meutes, tireurs, lanceurs de sorts, chargeurs, embusqués, volants, essaims). |
| **Boss** | **8 boss multi-phases**, chacun avec arène, cinématique d'introduction (caméra et titre), changements de phase et récompenses (fragments de couronne). |
| **Histoire et quêtes** | Quête principale « La Chute des Couronnes » (m1 à m7). Quêtes secondaires, cachées, de chasse, de trésor et de faction : **33 quêtes** au total. Dialogues avec portraits, choix et réputation de faction. |
| **PNJ** | **29 PNJ nommés** avec rôles (marchand, forgeron, aubergiste, garde, alchimiste, maître d'écurie, ancien, chasseur, prêtre, fermier, enfant, érudit, barde), emplois du temps jour/nuit et boutiques. |
| **Objets** | **125 objets**, en raretés Commun, Rare, Épique, Légendaire, Mythique et Ancestral, avec **7 panoplies** (bonus 2/4 pièces), consommables, matériaux, reliques et cosmétiques. |
| **Artisanat** | **49 recettes** sur 4 stations (forge, alchimie, feu de camp, établi). Récolte de bois, pierre, minerais, herbes et cristaux, plus pêche. |
| **Magie et compétences** | **14 sorts** répartis dans **7 écoles**. Arbre de **30 compétences** en **6 classes** : Guerrier, Archer, Assassin, Mage, Paladin, Chasseur. |
| **Montures** | **5 montures** : appel au sifflet, galop, endurance. |
| **Donjons** | **6 donjons** générés par graine : salles, couloirs, énigme à leviers, pièges, coffres et boss. |
| **Monde vivant** | Cycle jour/nuit, météo dynamique (pluie, neige, cendres, brouillard, orage), faune, vent dans la végétation, lucioles, fumées de cheminée. |
| **Rendu** | Shaders maison : PBR GGX, ombres douces (Poisson ou PCF), ombres des nuages, brouillard de hauteur, eau (profondeur, écume, reflets), lave, terrain multicouche, herbe instanciée, dissolve, contour, vision à travers les obstacles. Post-traitement : bloom, profondeur de champ, tonemapping ACES, étalonnage par région, vignette, grain, FXAA. Système de particules. |
| **Audio** | Mixeur logiciel, musiques en fondu enchaîné selon la région, l'heure, le combat et le boss, ambiances superposées, réverbération, panoramique spatial. Sortie AAudio sur Android. **14 musiques, 9 ambiances et 65 effets sonores** au format OGG Vorbis 48 kHz. |
| **Interface** | HUD (vie, mana, endurance, XP, raccourcis, boussole, minimap), carte du monde, inventaire, équipement, compétences, quêtes, artisanat, collection, bestiaire, réglages, sauvegardes, éditeur de disposition des commandes tactiles. |
| **Android** | Commandes tactiles personnalisables (position et taille), vibrations, mode immersif, cinq profils graphiques (Ultra, Élevé, Moyen, Faible, Économie d'énergie), contexte EGL conservé lors de la mise en arrière-plan. |
| **Sauvegarde** | 3 emplacements manuels et une sauvegarde automatique toutes les 3 minutes, avec écriture atomique (fichier temporaire puis renommage). |
| **Identité visuelle** | Splash SUPERESSENCE STUDIO, mention « Powered by AGR ENGINE », écran titre « LOST KINGDOM / A world waiting to be discovered. / Created by SUPERESSENCE STUDIO. », générique, icône du lanceur à toutes les densités. |

---

## État des fonctionnalités

Légende :
- ✅ implémenté et vérifié dans le harnais de bureau (exécution réelle du code du jeu, captures d'écran).
- 🟡 implémenté, mais vérifié seulement partiellement.
- ❌ absent.

| Fonctionnalité | État | Remarque |
|---|---|---|
| Monde ouvert continu, 6 régions, streaming | ✅ | Les traversées de régions sont parcourues par le bot de test. |
| Héros animé, IK des pieds, cape | ✅ | |
| Combat, parade, esquive, éléments | ✅ | |
| 27 ennemis et IA | ✅ | Le combat est joué par le bot ; la cohérence des données est vérifiée par les tests unitaires. |
| 8 boss multi-phases et intro | 🟡 | Intro et combat d'un boss capturés. Les 8 n'ont pas tous été battus de bout en bout par le bot. |
| Quêtes, dialogues, choix, réputation | 🟡 | Les références des 33 quêtes (PNJ, objets, ennemis, boss, donjons) sont validées par les tests. La quête principale n'a pas été terminée de bout en bout automatiquement. |
| Inventaire, équipement, panoplies, raretés | ✅ | |
| Artisanat, récolte, pêche | ✅ | |
| Magie (7 écoles), arbre de compétences (6 classes) | ✅ | |
| Montures | ✅ | |
| Donjons (énigme, pièges, coffres, boss) | ✅ | Entrée, énigme et sortie jouées par le bot. |
| Jour/nuit, météo | ✅ | |
| Shaders et post-traitement | ✅ | Vérifiés sur le rasteriseur logiciel Mesa (llvmpipe), pas sur un GPU mobile. |
| Audio (musique, ambiances, SFX) | 🟡 | Le chargement et le mixage sont vérifiés (capture WAV du mixage). La sortie AAudio n'a pas été écoutée sur un appareil. |
| Commandes tactiles et vibrations | 🟡 | Code écrit et éditeur de disposition capturé. Aucun test sur un écran tactile réel. |
| Profils graphiques | 🟡 | Les réglages s'appliquent. Les performances réelles sur un téléphone ne sont pas mesurées. |
| Sauvegarde et chargement | ✅ | Test aller-retour automatique dans le scénario du bot. |
| APK compilé, signé, aligné | ✅ | `apksigner verify`, `zipalign -c` et `aapt2 dump badging` sont vérifiés. |
| Installation et exécution sur un appareil Android | ❌ **non testé** | Aucun émulateur ni appareil n'est disponible dans l'environnement de construction (pas de KVM). |
| Doublage vocal | ❌ | Les dialogues sont uniquement textuels. |
| Modèles 3D d'artistes, motion capture | ❌ | Tout est procédural (voir *Limites*). |

---

## Comment le jeu a été vérifié

1. **Harnais de bureau** (`lk_desktop`). Il exécute *exactement le même code de jeu* que l'APK, sur Linux, dans un contexte EGL OpenGL ES 3.x sans fenêtre (Mesa), avec un pas de temps fixe.
   - L'option `--autoplay=full` lance un bot qui joue un scénario de 71 étapes : splash, titre, nouvelle partie, village, dialogue, tous les menus, combat, monture, traversée des six régions, météo, boss, donjon et énigme, test sauvegarde/chargement, éditeur de commandes, générique.
   - Le bot prend des captures d'écran (`build/shots/`) et peut enregistrer le mixage audio (`--wav`).
2. **AddressSanitizer**. Le même scénario a été exécuté sous ASan. Il a permis de corriger un accès hors limites dans l'IA ennemie et une invalidation de références dans l'ECS.
3. **Tests unitaires** (`lk_tests`), soit 1 809 vérifications :
   - l'ECS ;
   - les mathématiques ;
   - l'intégrité des données (chaque référence d'une quête, d'un butin, d'une recette ou d'une boutique pointe vers un objet existant) ;
   - le déterminisme du monde, la présence des 6 régions et l'absence de NaN ;
   - la génération de chunks aux 3 LOD ;
   - le décodage des fichiers audio ;
   - l'orientation de la minimap, de la flèche du joueur et de la flèche de quête ;
   - la géométrie de la disposition tactile (aucun chevauchement, marges, minimap dégagée) pour 5 formats d'écran.
4. **Test tactile automatique** (`--autoplay=touch`). Il injecte de vrais événements tactiles, appuis brefs dans la même image compris, et vérifie l'effet sur le héros : attaque, esquive, saut sans déclencher la parade, attaque chargée, combo mémorisé, doigt glissé depuis l'extérieur ignoré.
5. **APK**. Il est vérifié par `apksigner verify`, `zipalign -c -P 16` et `aapt2 dump badging`. Pour chaque ABI, `readelf` confirme :
   - que `ANativeActivity_onCreate` est exporté ;
   - que les segments sont alignés sur 16 Ko ;
   - que la bibliothèque ne dépend que de bibliothèques système (libandroid, liblog, libEGL, libGLESv3, libaaudio, libm, libdl, libc) ;
   - qu'elle contient bien le code du jeu complet et non le test de rendu « smoke ».

---

## Limites connues

- **Aucun test sur un appareil réel.** L'APK est construit et vérifié statiquement, mais il n'a jamais été installé sur un téléphone ou un émulateur : l'environnement de construction n'en possède pas. Des problèmes propres aux pilotes GPU mobiles, aux performances ou au tactile sont possibles.
- **Graphismes procéduraux.** Aucun modèle 3D d'artiste, aucune texture photographique, aucune motion capture. Personnages, créatures, bâtiments et végétation sont construits en code, et les animations sont des images-clés et du procédural. Le style est stylisé ; il n'atteint pas la fidélité d'une production AAA disposant d'une équipe artistique.
- **Audio synthétisé.** Musiques, ambiances et effets sont générés par `tools/make_audio.py` (synthèse additive et soustractive, Karplus-Strong, FM, formants, réverbération par convolution). Ce n'est pas un orchestre enregistré.
- **Performance non mesurée sur mobile.** Le harnais de bureau tourne sur un rasteriseur logiciel, à environ 120 ms par image : ce chiffre ne dit rien d'un GPU mobile. Les profils Faible et Économie d'énergie réduisent la distance de vue, les ombres, l'herbe et le post-traitement.
- **Signature locale.** L'APK est signé avec une clé locale générée au moment de la construction, qui n'est pas versionnée. Pour une publication sur le Play Store, il faut le re-signer avec votre propre clé (voir ci-dessous) et produire un AAB.
- **Langue.** L'interface et les dialogues sont en français uniquement.

---

## Compiler

### APK Android

Prérequis :
- Android SDK avec build-tools 35, platforms/android-35 et cmake ;
- NDK r26 ou plus récent ;
- Python 3.

```bash
cd lostkingdom
ANDROID_SDK=/chemin/vers/android-sdk ./build_apk.sh
# -> build/apk/LOST-KINGDOM-SUPERESSENCE.apk
```

- Le script n'utilise **pas Gradle**. Il enchaîne : CMake et NDK pour chaque ABI, `aapt2 compile/link`, ajout des bibliothèques stockées, `zipalign -P 16`, puis `apksigner`.
- **Signature** : définissez `LK_KEYSTORE`, `LK_KEYSTORE_PASSWORD` et `LK_KEY_ALIAS` pour utiliser votre clé. Sinon, une clé locale est générée dans `keystore/`, qui est ignoré par git. Les clés ne doivent jamais être commitées.
- **ABIs** : par défaut `ABIS="arm64-v8a armeabi-v7a x86_64"`.
- **Installation** : `adb install -r build/apk/LOST-KINGDOM-SUPERESSENCE.apk`.

### Harnais de bureau et tests (Linux, Mesa EGL)

```bash
cmake -S . -B build/desk -DCMAKE_BUILD_TYPE=Release && cmake --build build/desk -j
./build/desk/lk_tests assets                     # tests unitaires
./build/desk/lk_desktop --assets assets --frames 3600 --dt 0.0333 --autoplay=full --wav build/autoplay.wav
```

### Régénérer les assets

```bash
python3 tools/make_font.py       # atlas SDF (polices OFL : Cinzel, Cinzel Decorative, Alegreya Sans)
python3 tools/make_ui.py         # kit d'interface et icônes
python3 tools/make_audio.py      # musiques, ambiances, effets (numpy, scipy, ffmpeg)
python3 tools/make_branding.py   # icônes du lanceur, logos, key art
python3 tools/import_character.py modele.glb hero --preview apercu.png   # importe un personnage GLB (pose en T, sans squelette)
```

---

## Architecture

```
lostkingdom/
  engine/            AGR Engine (C++17, OpenGL ES 3.0)
    core/            ECS (sparse sets), maths, bruit, interface plateforme
    gfx/             renderer, shaders (variantes uber-shader), post-traitement, particules, textures, UI SDF
    anim/            squelettes, clips, animator multicouche, IK, Verlet
    audio/           décodeur Vorbis, mixeur, musiques et ambiances, réverbération
    platform/        Android (NativeActivity, EGL, AAudio, JNI vibreur) et bureau (EGL pbuffer)
  game/              LOST KINGDOM
    World.*          plan du monde, terrain, rivières, streaming, collisions
    Props.*          bâtiments et décor procéduraux
    Characters.*     héros, ennemis, créatures, armes (maillages skinnés)
    Animations.cpp   bibliothèque de clips
    Player / Combat / AI / Spawn
    Rpg.cpp          inventaire, artisanat, quêtes, dialogues, sauvegardes, réglages
    Dungeon.cpp      génération des donjons, énigmes, pièges
    Hud / Menus      interface
    Data.*           contenu : objets, ennemis, boss, PNJ, quêtes, sorts, compétences, montures, donjons
    AutoPlay.cpp     bot de test
  tests/             tests unitaires et prévisualisations
  tools/             génération des polices, de l'UI, de l'audio et de l'identité visuelle
  android/           manifeste et ressources
  assets/            polices, UI, audio (empaquetés dans l'APK)
  branding/          logos, icônes, key art
```

---

## Contrôles

| Commande | Action |
|---|---|
| Joystick gauche | Déplacement (pousser fort pour courir) |
| Glisser à droite | Rotation de la caméra ; pincer pour zoomer |
| Attaque / Lourde | Combo léger ou attaque chargée |
| Parade | Bloque ; une parade au bon moment déséquilibre l'ennemi |
| Esquive | Roulade avec invulnérabilité |
| Sorts 1 à 3 | Sorts équipés |
| Interagir | Parler, récolter, ouvrir, pêcher |
| Monture | Siffler la monture, puis monter ou descendre |

La disposition se modifie en jeu dans **Réglages → Commandes → Personnaliser les commandes**.

---

© SUPERESSENCE STUDIO. Polices sous licence SIL Open Font License (voir `tools/fonts/OFL.txt`).
