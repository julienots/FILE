# ETERNAL GOD — SuperEssence Studio

Monde ouvert 3D pour Android : vous incarnez un dieu immortel, libre de protéger, d'observer ou de détruire un monde vivant.

## Contenu de la version jouable (0.5)

| Système | État |
|---|---|
| Écran de chargement | Logo SuperEssence Studio (image fournie) + vraie progression, puis logo ETERNAL GOD, « Toucher pour commencer » |
| Le Dieu | Armure or et ivoire, casque fermé, auréole, ailes de plus de 160 plumes animées une à une. Marche lourde, course, saut, vol libre, vol stationnaire, marche sur l'eau, atterrissage avec cratère et onde de choc, projection sans jamais mourir |
| Traces de pas | Fissures au sol, poussière, cratères en courant ; la course renverse arbres et maisons |
| Monde | 1,5 × 1,5 km sans chargement : plaine, Forêt des Anciens, Pics de Givre enneigés, Mont Braise (volcan), Lac des Murmures, rivière, îles célestes flottantes |
| Ciel | 8 ciels photographiques réels (série Qwantani, Poly Haven) fondus selon l'heure ; soleil, couleurs et reflets mesurés sur les photos ; nuit étoilée ; météo (orages, pluie, éclairs) |
| Villages | 4 lieux : Vallon d'Aube, Port-Lac, Clairière des Anciens, Hameau du Col. Rues pavées et chemins, temple, auberge, forge, marché, moulin, tours, champs cultivés, ponton, accessoires scannés |
| Chemins | Routes entre villages tracées selon le relief (A*), ponts, panneaux, bornes, lanternes, voyageurs |
| Habitants | ~140 à 280 selon la qualité, avec nom, métier, famille et routine horaire. Ils s'agenouillent et prient, fuient, se cachent, attaquent (gardes), fêtent, meurent, reconstruisent, ont des enfants. Chaque habitant est fusionné en un seul maillage animé (1 appel de dessin) avec tenue teintée propre |
| Frimeurs | 2 à 5 par village : s'entraînent sur la place, narguent le dieu (figures, coups de pied, roulades, moqueries, jets de cailloux) et ne font plus les malins face à l'aura ou à la destruction |
| Forme humaine | Bouton 🧍 (H) : le dieu devient un homme de 1,9 m animé par de vraies animations (marche, course, poings, coups de pied) avec poses de vol, de comète et d'atterrissage ; il garde tous ses pouvoirs |
| Aura divine | Bouton ⚡ (T) : transformation, flammes dorées, puissance ×2,5, vitesse ×1,6 |
| Combat | Combos (jab, jab, crochet, uppercut/coup de pied) avec poursuite éclair et arrêt sur image ; Titan projeté ; Dragon des cendres (boules de feu, piqués) ; défis depuis le menu Pouvoirs |
| Musique | Orchestre synthétisé adaptatif (exploration, combat, aura), voix permanentes et limiteur (pas de craquements) |
| Faune | Cerfs, biches, renards, loups, chiens, chats ; fuient le dieu |
| Végétation | Arbres et sapins scannés (imposteurs), herbe, fleurs des prés, fougères, orties, souches, troncs morts, rochers ; vent et souffle du vol divin |
| Destruction | Terrain déformable et persistant (cratères, tranchées, lave, brûlures), arbres renversés ou calcinés, bâtiments fissurés puis effondrés en débris physiques, ruines persistantes |
| Pouvoirs | Sans temps de recharge. Poing divin (maintenir = Coup absolu), Explosion céleste, Rayon divin, Météore, Onde de choc, Éclair divin, Singularité, Charge de comète, Chute titanesque, Pluie d'astres, Lames célestes, Cyclone divin, Gravité inversée, Arrêt du temps, Fissure tellurique, Supernova, Déferlante astrale (rayon colossal qui creuse un canyon), Chute de la lune, Lance céleste (pilier orbital qui suit le regard), Séisme titanesque ; Bénir et Relever les ruines ; Téléportation |
| Boss | Titan de magma (~26 m) : émerge, marche sur un village, frappe au sol, lance des rochers, souffle de lave, chancelle, s'effondre ; revient plus tard |
| Réputation | Chaque village se souvient (Crainte / Vénération) : prières ou fuite, attaques des gardes, reconstruction, naissances |
| Interface | Joystick, caméra au doigt (pincer pour zoomer), boutons Poing / Pouvoir / Saut / Vol / Descendre / Interaction / Téléport, roue des pouvoirs, mini-carte, carte du monde, journal, réglages, mode photo (enregistre dans Images/EternalGod) |
| Mobile | 3 niveaux de qualité (détection automatique), résolution dynamique, sauvegarde automatique et à la mise en arrière-plan, bouton retour Android, vibrations |

## Commandes

| Tactile | Clavier (essais sur ordinateur) |
|---|---|
| Joystick (moitié gauche) | ZQSD / WASD, Maj pour courir |
| Glisser à droite : caméra ; pincer : zoom | souris |
| ✊ Poing (maintenir 1,2 s : Coup absolu) | — |
| Pouvoir sélectionné (◈ : roue des pouvoirs) | Q, R, chiffres 1–5 |
| ⤒ Saut / monter en vol, ⤓ descendre | Espace, C |
| 🜁 Vol | F |
| ✋ Interaction contextuelle | E |
| ✦ Carte et téléportation, ☰ Menu | M / Échap |
| ⚡ Aura divine | T |
| 🧍 Forme humaine / ✦ forme divine | H |

## Architecture

Application Android native (Java) qui exécute un moteur 3D WebGL 2 (Three.js r170) embarqué hors ligne.

```
app/src/main/java/.../MainActivity.java   WebView plein écran, ressources servies depuis l'APK, vibrations, photos, cycle de vie
app/src/main/assets/game/
├── index.html, css/style.css             page, écran de chargement, interface
└── js/
    ├── main.js        chargement, boucle, sauvegarde, résolution dynamique
    ├── core.js        réglages, qualité, bruit, utilitaires
    ├── terrain.js     relief déformable (grille 3 m), tronçons, textures de données
    ├── terrainmat.js  matériau PBR du sol (11 textures photo, triplanaire)
    ├── sky.js, env.js ciel photographique, lumière, météo, eau, rivière, îles, volcan
    ├── vegetation.js  imposteurs, herbe et fleurs, rochers, souches
    ├── impostor.js    rendu des arbres scannés (32 vues, éclairage dynamique, ombres)
    ├── buildings.js   fabrique des bâtiments + matériau triplanaire
    ├── village.js     villages, dégâts, effondrement, reconstruction (maillage fusionné par village)
    ├── roads.js       routes A*, ponts, panneaux
    ├── people.js      habitants (IA, routines, réactions, poses)
    ├── fauna.js       animaux
    ├── godmodel.js    modèle du dieu ; god.js : contrôle et animation
    ├── humanform.js   forme humaine (animations de personnage + poses)
    ├── aura.js, music.js, dragon.js, ultimates.js
    ├── camera.js, powers.js, monster.js, fx.js, physics.js, audio.js, ui.js
tools/                 génération des logos, conversion des ciels HDR, cuisson des imposteurs, simplification des modèles, aperçus et tests automatisés
```

**Pourquoi WebGL plutôt qu'Unreal ou Unity ?** Ces moteurs ne peuvent pas être installés ni compilés dans l'environnement de développement utilisé ici. Three.js permet de livrer un APK réellement jouable, testé automatiquement, et une architecture modulaire facile à faire évoluer.

## Compiler

Prérequis : JDK 17+, Android SDK (plateforme 35, build-tools 35.0.0).

```bash
echo "sdk.dir=/chemin/vers/android-sdk" > local.properties
gradle assembleRelease      # → app/build/outputs/apk/release/app-release.apk
```

La signature utilise `keystore/eternalgod-release.jks` et `keystore/keystore.properties` (non versionnés : ce sont des secrets).

Tester dans un navigateur : `python3 -m http.server` à la racine de `eternal-god/`, puis ouvrir `app/src/main/assets/game/index.html`. Tests automatisés : `node tools/test/play.cjs` et `node tools/test/perf.cjs <low|medium|high>`.

## Crédits et licences

- Textures, ciels HDR, végétation, rochers, accessoires : [Poly Haven](https://polyhaven.com), licence CC0.
- Habitants et animaux : Quaternius (CC0), via le paquet `@netforge/asset-library-files` (voir `models/chars/NOTICE-netforge.txt`).
- Polices Cormorant, Nunito, Fredoka : SIL Open Font License.
- Three.js : licence MIT.
