# LOST KINGDOM 2.0 — « La Chute des Couronnes » : plan de la grande mise à jour

Document de passation pour un agent (Claude) qui reprend le développement. À lire en entier avant de coder.

## 0. Contexte à connaître avant de commencer

**Dépôt et livraison**
- Dépôt `julienots/file`, branche `ccr-49335fa0-ubib49`, projet dans `lostkingdom/`.
- Une application NOCTÉA existe à la racine : **n'y touchez pas**, et ne supprimez aucun asset.

**Technique**
- Jeu en C++17 / OpenGL ES 3.0, moteur maison « AGR Engine ».
- Android NativeActivity, APK construit **sans Gradle** par `build_apk.sh`.
- Tout est procédural (géométrie, textures, audio), sauf le héros, importé depuis un GLB Tripo.
- Langue du jeu : **français**.

**Règles de l'utilisateur, non négociables**
- Ne jamais prétendre qu'une fonctionnalité existe si elle n'est pas faite. Les limites vont dans le README.
- Aucun téléphone ni émulateur n'est disponible : tout est vérifié sur ordinateur (Mesa llvmpipe). Dites-le dans chaque compte rendu.
- Ne jamais commiter de clé de signature (le dossier `keystore/` est ignoré par git).
- Ne pas créer de pull request sauf demande explicite.
- Terminer chaque commit par :
  - `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`
  - `Claude-Session: https://claude.ai/code/session_01AFMFthb8TpW7b5pkhux6mE`
  - (le modèle qui fait le travail peut adapter la première ligne)

### Commandes

```bash
cd lostkingdom
cmake -S . -B build/desk -DCMAKE_BUILD_TYPE=Release && cmake --build build/desk -j8
./build/desk/lk_tests assets                                   # 1 832 vérifications, doivent toutes passer
./build/desk/lk_desktop --assets assets --frames 3600 --dt 0.0333 --autoplay=full   # scénario complet de 71 étapes
#   autres scénarios : touch (test tactile, doit afficher "TOUCH TEST PASSED"), tour, look, hero, gait
#   captures dans build/shots/, à regarder réellement avant de conclure
LK_GPU_PROFILE=1 ./build/desk/lk_desktop ... --profile=2       # coût GPU par passe ; profils 0 Ultra … 4 Économie
cmake -S . -B build/asan -DCMAKE_CXX_FLAGS="-fsanitize=address -g" && cmake --build build/asan   # détecteur mémoire
./build_apk.sh                                                  # → build/apk/LOST-KINGDOM-SUPERESSENCE.apk
```

### Livrer une version

1. Incrémenter `--version-code` / `--version-name` dans `build_apk.sh`, et le texte `v1.x.0 · AGR Engine` dans `game/Menus.cpp`.
2. Lancer les tests, le scénario `touch`, le scénario `full`, puis le scénario `full` sous le détecteur mémoire (zéro erreur).
3. Construire l'APK et le vérifier :
   - `apksigner verify`, `zipalign -c -P 16 4`, `aapt2 dump badging` ;
   - `readelf` : `ANativeActivity_onCreate` présent et alignement à 0x4000 pour les 3 ABI ;
   - `strings` : la bibliothèque contient bien le jeu (« Grotte des Murmures ») et pas le test de rendu.
4. Copier l'APK dans `release/`, mettre à jour le README (nouveautés, SHA-256, nombre de tests), commiter et pousser.

### Carte du code (`game/`)

| Fichier | Rôle |
|---|---|
| `Game.h` | Tout l'état du jeu (classe `Game`), `Input`, `Settings`, composants ECS |
| `Game.cpp` | Boucle `frame()`, `updatePlaying()`, environnement, directeur audio, profileur |
| `Render.cpp` | **Caméra** (début du fichier), rendu des chunks et des personnages, IK |
| `Player.cpp` | Contrôle du héros : `playerAttack`, `playerDodge`, mémorisation des appuis, sorts, monture |
| `Combat.cpp` | `dealDamage` (parade, déséquilibre, coups critiques, hit-stop), projectiles, `updateCharacters` |
| `AI.cpp` | IA des ennemis et des boss (télégraphes, `executeEnemyAttack`, phases), PNJ, animaux |
| `Animations.cpp` | Courbes de marche et de course, clips d'attaque (`ClipId`), états |
| `Data.cpp` | Contenu : objets, ennemis, boss, PNJ, **quêtes** (`quests()`, ligne ≈ 705), sorts, compétences |
| `Rpg.cpp` | Inventaire, `advanceQuests`, dialogues (`openDialog` / `dialogAction`), sauvegarde texte, réglages |
| `Hud.cpp` | Commandes tactiles (`updateTouchControls`, appelée **avant** la simulation), HUD, minimap |
| `Menus.cpp` | Tous les menus, `drawDialog`, écran titre, générique |
| `AutoPlay.cpp` | Robot de test : un scénario par nom |
| `World.cpp` / `Props.cpp` | Génération du monde, décors, structures |
| `Spawn.cpp` | Création des entités ; **registre de modèles importés** (`importedModel(id)`) |

**Points à connaître**
- `Input` contient des événements d'une seule image, effacés en fin d'image. Toute nouvelle action tactile doit être produite dans `updateTouchControls`.
- `timeScale` et `hitStop` existent déjà (`Game.h`). La caméra lit `camYaw`, `camPitch` (0.66), `camDist` (9 à 32), `camDistTarget`, `input.camDrag`, `input.pinch`.
- Pour les modèles importés : `python3 tools/import_character.py modele.glb <id> --preview apercu.png` produit `assets/models/<id>.lkm` et `<id>.jpg`, pris en charge automatiquement. Les identifiants sont `hero`, l'identifiant d'un ennemi humanoïde (dont les boss `b_*`), `npc_<id>`, `villager_m` et `villager_f`.

---

## 1. Caméra libre (priorité 1, la plus attendue)

**Objectif de l'utilisateur :** « voir la carte, le ciel et devant, tout en restant derrière le même personnage, et zoomer ».

### À faire

1. **Inclinaison au doigt.**
   - Dans `Hud.cpp`, la ligne 128 n'alimente que `input.camDrag.x` : ajouter `camDrag.y`.
   - Dans `Render.cpp`, ajouter `camPitch += camDrag.y * k`, avec une limite entre **-0,35** (caméra basse qui regarde vers le haut : le ciel) et **1,35** (vue presque du dessus : la carte).
   - Prévoir une option « Inverser l'axe vertical » dans les Réglages.
2. **La caméra regarde vers le haut sans traverser le sol.** Quand l'inclinaison est sous 0,25 :
   - rapprocher la caméra (`camDist × 0,6`) ;
   - relever le point visé au-dessus de la tête (`look.y += 1,4`) ;
   - garder l'œil à au moins 0,6 m au-dessus du terrain (il y a déjà un minimum à 3 m : le rendre dépendant de l'inclinaison).
   - Le personnage doit rester visible, cadré dans le tiers bas de l'écran.
3. **Zoom.**
   - Le pincement existe : élargir la plage à **4 → 40** et lisser.
   - En zoom maximal, éloigner le brouillard et activer un léger effet maquette (tilt-shift, `env.tiltShift`) : c'est la vue « carte ».
   - En zoom minimal, rendre la cape et l'arme visibles de près.
4. **Retour automatique (optionnel).**
   - Après 4 s sans toucher l'écran **et** en déplacement, l'inclinaison revient doucement à 0,66.
   - Pendant un combat verrouillé, la caméra cadre le héros et la cible.
   - Un double appui dans la zone caméra réinitialise inclinaison et zoom.
5. **Regard vers l'avant.** Quand le héros court, décaler le point visé de 1,5 m dans le sens de la course, pour voir où l'on va.
6. **Collision avec le décor.** Si un bâtiment se trouve entre la caméra et le héros, réduire la distance. Il existe déjà un effet qui rend transparente la géométrie entre la caméra et le joueur ; à garder.
7. Faire suivre l'inclinaison au cadre des cinématiques de boss.

### Critères d'acceptation

- **Nouveau scénario `--autoplay=camera`** : inclinaisons -0,35, 0,0, 0,66, 1,3 et zooms 4, 15, 40.
  - Pour chaque capture, vérifier par code que l'œil est au-dessus du terrain.
  - Vérifier aussi que la position du héros projetée à l'écran (`worldToScreen`) reste dans l'image.
  - Regarder les captures : le ciel est visible à -0,35, la carte vue de haut à 1,3.
- **Tests unitaires** sur la fonction de calcul de la caméra : la sortir en fonction pure, par exemple `CameraRig::compute(...)`.

---

## 2. Système de combat « badass » (priorité 2)

**Ce qui existe déjà**
- Combo léger en 3 coups, attaque lourde et attaque chargée (en maintenant).
- Parade avec fenêtre `parryT` : une parade réussie déséquilibre l'ennemi (`Combat.cpp` ≈ ligne 67).
- Esquive invulnérable.
- Déséquilibre (*poise*) : texte « Déséquilibré ».
- Hit-stop, tremblement de caméra, vibrations ; télégraphes rouges des ennemis.
- Verrouillage de cible en touchant un ennemi ; 14 sorts.

Chaque mécanique ci-dessous doit être visible, sonore et vibrante. Chacune a son retour visuel, sonore et haptique.

### 2.1 Esquive parfaite « Instinct »
- **Déclenchement :** une esquive lancée moins de 0,18 s avant qu'une attaque ennemie ne touche.
- **Implémentation :** dans `dealDamage`, mémoriser « l'attaque aurait touché pendant l'invulnérabilité de l'esquive ».
- **Effet :**
  - `timeScale = 0.3` pendant 1,2 s (ralenti), sauf pour le héros, qui reçoit un facteur de vitesse ×2,2 : il faut une vitesse propre au joueur ;
  - teinte bleutée et léger effet d'aberration dans le post-traitement (`uFade` ou une nouvelle uniforme) ;
  - son « whoosh » inversé et vibration courte.
- **Fenêtre de riposte :** la prochaine attaque pendant le ralenti fait ×2 de dégâts et ×3 de déséquilibre.

### 2.2 Parade parfaite et Riposte
- La parade existe : ajouter la **Riposte**.
- Après une parade réussie, la prochaine pression sur Attaque dans les 0,8 s déclenche :
  - un clip dédié (nouveau `ClipId` `C_RIPOSTE`, coup d'estoc tournoyant) ;
  - des dégâts ×3 et un rapprochement de caméra de 0,3 s ;
  - des étincelles dorées et un son métallique grave.
- Pendant la fenêtre, afficher une invite « RIPOSTE » au-dessus du bouton d'attaque.

### 2.3 Exécutions
- **Déclenchement :** quand un ennemi est déséquilibré (`poise <= 0`), il reste au sol ou à genoux 2 s (nouvel état d'IA `Staggered`). Le bouton d'attaque affiche alors « EXÉCUTER ».
- **Exécution :** courte cinématique de 1,2 s ; la caméra passe sur un plan latéral bas et serré.
  - Le héros joue un clip de coup de grâce par type d'arme (épée, lance, marteau, dague…).
  - L'ennemi meurt, sauf un boss : il perd 15 % de ses PV maximum et son déséquilibre est renouvelé.
- **Feedback :** ralenti à 0,2 s, éclaboussure de particules, vibration forte, son de coup de grâce.
- **Rythme :** limite d'une exécution toutes les 6 s sur un boss.

### 2.4 Combo et rang de style
- **Compteur de coups** sans encaisser de dégâts, affiché à droite (« 12 COUPS »).
- **Rang** D, C, B, A, S selon les coups variés, les parades et les esquives parfaites. Le rang diminue avec le temps et retombe d'un cran si l'on prend un coup.
- **Effets du rang :** il multiplie les dégâts (+0 à +25 %) et le gain de Fureur.
- Animation du rang à chaque passage de palier ; police `F_TITLE`, couleur par rang.

### 2.5 Fureur de la Couronne (ultime)
- **Jauge** sous l'endurance. Elle se remplit avec les coups, les parades, les esquives parfaites et les exécutions.
- **Pleine :** le bouton de sort 3 devient « JUGEMENT ».
- **Activation :** saut et frappe au sol en zone (8 m).
  - Cinématique de 1,5 s : la caméra tourne autour du héros, la marque-couronne de son poignet s'illumine ;
  - lumière dorée, onde de choc, et tous les ennemis proches déséquilibrés.
- **Évolution :** l'ultime change de forme une fois la Couronne-Mère reforgée, à l'acte III.

### 2.6 Lisibilité des attaques ennemies
- **Code couleur des télégraphes :**
  - **rouge** : imparable, il faut esquiver ;
  - **or** : parable, la parade parfaite mène à une Riposte ;
  - **violet** : magie, à interrompre par un coup lourd.
- **Implémentation :** ajouter `bool parryable` et `bool unblockable` à `AttackDef` (`Data.h`), puis classer les attaques de chaque ennemi dans `Data.cpp`.
- **Retour visuel :** un éclair brillant sur l'arme ennemie 0,25 s avant l'impact. C'est le repère de la parade parfaite.

### 2.7 Sensations (impact)
- **Hit-stop selon l'arme :** dague 0,03 s, épée 0,05, marteau 0,09, coup critique +0,04.
- **Tremblement directionnel :** dans la direction du coup, et non aléatoire.
- **Flou radial court sur les coups lourds :** nouvelle passe dans le shader de composition, centrée sur le point d'impact projeté à l'écran.
- **Repoussement physique :** l'ennemi recule selon sa masse ; les petits ennemis sont projetés lors des exécutions.
- **Impacts au sol :** traces de brûlure ou de sang très légères, en particules longues durées.

### 2.8 Boss : des combats spectaculaires
- **Une attaque signature par boss**, avec télégraphe long et caméra qui recule :
  - Gardien des Racines : prison de racines ;
  - Roi Squelette : invocation de garde et onde de couronne ;
  - Colosse de Glace : chute de stalactites ;
  - Dragon Cendré : souffle en balayage et passage en vol ;
  - Reine des Araignées : pluie d'œufs ;
  - Chevalier Déchu : duel de parades, avec beaucoup de coups dorés ;
  - Titan de Pierre : poing de météore ;
  - Seigneur des Abysses : vide qui aspire.
- **Changements de phase** (existants dans `AI.cpp`) : ajouter une mini-cinématique de 2 s, un changement d'arène (ciel, couleurs, particules) et une nouvelle attaque.
- **Rage sous 20 % de PV** : vitesse +20 % et aura.
- **Barre de boss :** déséquilibre visible sous la barre de PV, et nom et titre affichés pendant l'intro.
- **Mort du boss :** ralenti, dissolution, pluie de lumière, son de victoire, puis le fragment de couronne flotte vers le héros.

### 2.9 Interface et commandes
- **Invites contextuelles** sur le bouton d'attaque : RIPOSTE, EXÉCUTER, JUGEMENT.
- **Glisser sur le bouton d'attaque :** vers le haut pour un coup lanceur (projection en l'air de l'ennemi léger), vers le bas pour un coup au sol.
- **Verrouillage :** balayer l'écran côté caméra avec deux doigts change de cible.

### Critères d'acceptation (combat)
- **Tests unitaires** : multiplicateurs (rang, riposte, exécution) et classement des attaques ; aucun ennemi sans attaque parable ou esquivable.
- **Nouveau scénario `--autoplay=combat`**, qui affiche `COMBAT TEST PASSED` avec le détail des cas :
  - esquive parfaite (vérifier `timeScale < 1`) ;
  - parade puis riposte ;
  - déséquilibre puis exécution ;
  - montée du rang jusqu'à B au moins ;
  - Fureur pleine puis Jugement ;
  - captures de chaque moment.
- **Aucune régression** : `touch`, `full` et le détecteur mémoire restent propres.

---

## 3. Histoire complète (priorité 3)

### 3.0 Systèmes à ajouter d'abord
1. **Drapeaux d'histoire** `std::set<std::string> storyFlags`, sauvegardés dans le format texte de `Rpg.cpp`. Garder la compatibilité avec les anciennes sauvegardes : une clé absente signifie un ensemble vide.
2. **Choix de dialogue qui posent des drapeaux** (`dialogAction("flag:xxx")`) et quêtes conditionnelles : champ `requiresFlag` dans `QuestDef`.
3. **Étape `Goto` à implémenter.** Elle existe dans l'énumération `StepKind` mais n'est **pas gérée** dans `advanceQuests`. Elle doit se valider à moins de N m d'un lieu.
4. **Cinématiques à plans multiples.**
   - Remplacer `startCinematic(from, to, look, dur, text)` par une liste de plans `{caméra de départ, caméra d'arrivée, point visé, durée, sous-titre, orateur, clip joué par un PNJ}`.
   - Bandes noires, appui pour passer, interruption propre.
   - À écrire dans un nouveau fichier `game/Cinematics.cpp`.
5. **Écran « Acte »** : titre en grand avec une musique dédiée (par exemple « ACTE II — Les Gardiens corrompus »).
6. **Journal** : la quête principale montre un résumé « Ce que je sais » qui se remplit au fil de l'histoire.

### 3.1 Synopsis

**Il y a 300 ans.** Le Haut-Roi **Aldric** régnait sur six royaumes unis par la **Couronne-Mère**. **Malgrath**, ancien mage royal, ouvrit l'**Abîme** sous Morneval pour devenir immortel. Pour le sceller, Aldric brisa la Couronne en six fragments, confiés à six **Gardiens** (un esprit par région), et lia sa propre âme au sceau. Son champion, **Sire Corwin**, jura de garder la porte.

**Aujourd'hui.** Le sceau s'effrite. Les murmures de l'Abîme ont corrompu les Gardiens, devenus les boss du jeu. Un naufragé, le **héros** (sans nom, sans souvenirs), s'échoue sur la grève de Greenhaven avec une marque-couronne brûlante au poignet.

**La vérité, révélée par étapes.** Le héros n'est pas un descendant d'Aldric : **il est l'âme d'Aldric**, rappelée par le sceau mourant dans un corps neuf. Les Pierres d'écho et les souvenirs des Gardiens le révèlent peu à peu.

**La trahison.** **Frère Alaric**, de l'Ordre de Highspire, qui guide le héros, veut la Couronne-Mère pour **rouvrir** l'Abîme et devenir le nouveau Malgrath. Il a orchestré le retour d'Aldric pour qu'il rassemble les fragments à sa place.

### 3.2 Personnages

| Personnage | Rôle | Arc |
|---|---|---|
| Le héros (Aldric réincarné) | joueur | De l'amnésie à l'acceptation, puis au choix final |
| Maëlle | doyenne de Brindlemere, mentor | Gardienne des tapisseries. Elle sait plus qu'elle ne dit ; elle se sacrifie (ou survit si l'on a sauvé Brindlemere à l'acte III) |
| Frère Alaric | guide, puis antagoniste | Érudit sympathique, puis traître à l'acte III ; boss de l'acte IV (nouvelle forme : « Alaric Couronné ») |
| Sire Corwin, le Chevalier Déchu | gardien de la porte | Croit que le héros est un imposteur. Vaincu, il reconnaît son roi et devient un allié de l'acte V, si on l'épargne |
| Malgrath, Seigneur des Abysses | antagoniste originel | Voix dans les rêves dès l'acte I ; boss final |
| Les six Gardiens | boss régionaux | Chacun, une fois purifié, livre un souvenir d'Aldric (flashback jouable de 30 s) |
| Hilda (forgeronne), Jarl Bjorn (Frostheim), Fenra (Elderwood), Tomas | alliés régionaux | Leurs quêtes de faction décident qui vient au combat final |

### 3.3 Structure (quêtes principales)

Identifiants proposés ; les quêtes `m1` à `m7` existantes sont **réécrites ou réutilisées**.

**Prologue — La grève** (`m1`, existe déjà)
- Cinématique d'ouverture : la tempête, le naufrage, une voix (Malgrath) : « Tu es revenu… trop tard. »
- Didacticiel de combat intégré : 2 loups sur la plage, sur le chemin du village.

**Acte I — Les Murmures** (`m2`, `m3` ; ajouter `m2b`)
- `m2` : la Grotte des Murmures, première Pierre d'écho. Premier souvenir : Aldric brisant la couronne.
- `m2b` « La nuit des loups » (nouveau) : le village est attaqué la nuit, à défendre avec les gardes.
  - Première apparition des créatures d'ombre ; Maëlle reconnaît la marque.
  - Choix : poursuivre le chef de meute ou protéger les enfants. Drapeau `act1_children`, qui joue sur la survie de Maëlle à l'acte III.
- `m3` : route des pèlerins vers Highspire, rencontre avec Alaric. Il est chaleureux et donne une carte des Gardiens.

**Acte II — Les Gardiens corrompus** (`m4` éclatée en 6 sous-chaînes ; ordre libre)
- **Structure commune** de chaque région : une quête d'approche (2 ou 3 étapes avec un allié local) → le donjon ou l'arène → le boss → la purification, avec cinématique et souvenir d'Aldric → le fragment.
- **Elderwood** — Fenra, druidesse. Le Gardien des Racines. Souvenir : Aldric plantant l'arbre-mère avec Corwin.
- **Gravemoor** — le Roi Squelette, ancien sénéchal d'Aldric. Souvenir : le sénéchal jurant fidélité. Révélation : « Le roi est revenu. »
- **Frostheim** — Jarl Bjorn. Le Colosse de Glace. Souvenir : la bataille de l'hiver noir contre l'armée de Malgrath.
- **Ashen Lands** — le Dragon Cendré. Souvenir : Malgrath, jeune mage, élève d'Aldric. La trahison originelle.
- **Antre** — la Reine des Araignées. Souvenir : Alaric était déjà là, il y a 300 ans, comme jeune moine. **Premier indice qu'il ne vieillit pas.**
- **Highspire** — le Titan de Pierre, au-dessus du monastère. Souvenir : Aldric confiant la clé de la forge à l'Ordre.
- **Après 3 fragments** : cauchemar scripté. Le héros se réveille dans l'Abîme et Malgrath lui propose une alliance. Choix « refuser » ou « écouter », drapeau `listened_malgrath`, qui débloque une fin.

**Acte III — La Couronne trahie** (`m5`)
- Alaric reforge la Couronne-Mère (cinématique dans la forge), puis la **vole** :
  - le héros, à genoux, est vidé par la marque ;
  - Alaric révèle son âge, sa lignée et son plan ;
  - il s'enfuit vers Morneval avec la Couronne.
- **Siège de Highspire** (nouveau) : les créatures d'ombre attaquent le monastère.
  - Défense par vagues avec les moines ; évacuation des archives.
- **Retour à Brindlemere en flammes.** Maëlle meurt ou survit selon `act1_children` et le temps mis à revenir. Elle donne la **Dernière Pierre d'écho** : la vérité sur l'âme d'Aldric.
- **Le héros accepte qui il est.** L'ultime « Jugement des Couronnes » évolue (voir 2.5).

**Acte IV — Morneval** (`m6`)
- Traversée de Gravemoor, désormais ravagé (variante sombre de la région).
- **Sire Corwin, le Chevalier Déchu** : duel axé sur les parades.
  - À 10 % de PV, il ne meurt pas et se met à genoux.
  - Choix : l'épargner (« Relève-toi, mon champion ») ou l'achever. Drapeau `spared_corwin`.
- **Alaric Couronné** : nouveau boss dans la salle du trône de Morneval. Il porte la Couronne et utilise les attaques des six Gardiens, une par phase.
  - Vaincu, il tombe dans l'Abîme avec la Couronne : il faut l'y suivre.

**Acte V — L'Abîme** (`m7`)
- **Donjon final** en 3 niveaux. Chaque niveau reflète deux régions corrompues.
- **Alliés présents** selon les quêtes de faction : Fenra, Bjorn, Corwin (si épargné), Hilda. Chacun tient une porte pendant une vague (scène scriptée).
- **Malgrath, Seigneur des Abysses** : 3 phases, puis sa forme finale dans le vide (arène flottante, ciel violet).

### 3.4 Fins (choix final, cinématique dédiée à chacune)

1. **Le Sceau Éternel** (par défaut) : Aldric remet la Couronne dans le sceau et y lie son âme. Le royaume est sauvé, le héros disparaît.
   - Épilogue : tapisserie à Brindlemere ; Corwin monte la garde si épargné.
2. **Les Couronnes brisées** : elle exige d'avoir purifié les six Gardiens **et** épargné Corwin. Le héros détruit définitivement la Couronne ; les six royaumes deviennent libres et les Gardiens, purifiés, tiennent le sceau ensemble.
   - Le héros vit. C'est la « vraie » fin, avec une scène après le générique.
3. **Le Roi de l'Abîme** (si `listened_malgrath`) : le héros coiffe la Couronne et absorbe Malgrath. Fin sombre : ciel violet sur tout le royaume, le héros règne.
   - Débloque un mode Nouvelle Partie+ avec ennemis renforcés.

**Après la fin.** Le jeu continue (monde ouvert, quêtes secondaires restantes). Le monde garde une trace de la fin choisie : couleur du ciel, PNJ et dialogues mis à jour.

### 3.5 Écriture
- **Ton :** épique sobre, phrases courtes. Chaque PNJ important a sa voix : Maëlle douce et énigmatique, Alaric érudit et chaleureux puis glacial, Corwin solennel, Malgrath séducteur.
- **Quantité :** environ 120 répliques nouvelles, 7 cinématiques majeures, 6 souvenirs, 3 fins.
- Tout le texte va dans `Data.cpp` (dialogues de quête) et `Cinematics.cpp`.
- **Vérification automatique** : un test qui parcourt la quête principale et vérifie la cohérence des drapeaux, ainsi que l'accessibilité de chaque fin.

### Critères d'acceptation (histoire)
- **Nouveau scénario `--autoplay=story`** qui enchaîne toute la quête principale en raccourci : téléportations, boss tués par commande de débogage, choix scriptés. Il affiche `STORY TEST PASSED` et les fins atteintes.
- Les tests unitaires vérifient les références des nouvelles quêtes. Le test existant les couvre déjà : il vérifie les PNJ, les objets, les boss et les donjons.
- Les anciennes sauvegardes se chargent toujours (test à faire avec une sauvegarde 1.5).

---

## 4. Modèles 3D des boss (en parallèle, quand l'utilisateur fournit les GLB)

- **Humanoïdes** : `b_root_guardian`, `b_skeleton_king`, `b_ice_colossus`, `b_fallen_knight`, `b_stone_titan`, `b_abyss_lord`.
  - Commande : `python3 tools/import_character.py X.glb b_xxx --preview p.png`.
  - Vérifier l'aperçu des poids (pas de « plaque » sous les pieds). Le test existant sur les pieds doit être étendu à tous les fichiers `assets/models/*.lkm`.
  - L'échelle vient de `EnemyDef.scale` : vérifier que la taille finale correspond au boss procédural (comparer `c.height`).
- **Ajuster l'import si nécessaire** : les boss ont des proportions extrêmes (racines, cornes, ailes du Seigneur des Abysses). Faire attacher les ailes à la poitrine et les cornes à la tête.
- **Non humanoïdes** (`b_ash_dragon`, `b_spider_queen`) : il faut un **nouvel import** qui ajuste les squelettes `Q_*` (dragon) et `S_*` (araignée) de `Characters.h`. Mission séparée (voir le document des missions avancées).
- **Budget** : un boss importé fait au plus 25 000 triangles et une texture de 1024 px. Mesurer avec `LK_GPU_PROFILE` dans l'arène.

---

## 5. Ordre de travail et livrables

| Étape | Contenu | Version |
|---|---|---|
| 1 | Caméra libre complète + scénario `camera` + tests | 1.6.0 — **livrée** |
| 2 | Combat : esquive parfaite, riposte, exécutions, rang, Fureur, télégraphes colorés + scénario `combat` | 1.7.0 |
| 3 | Systèmes d'histoire (drapeaux, `Goto`, cinématiques à plans, écran Acte, journal) | 1.8.0 |
| 4 | Histoire actes I à III (quêtes, dialogues, siège, trahison) | 1.9.0 |
| 5 | Actes IV et V, boss Alaric Couronné, fins, Nouvelle Partie+ + scénario `story` | 2.0.0 |
| — | Boss importés, au fil de l'eau quand les GLB arrivent | — |

**À chaque étape**
- Livrer un APK vérifié et envoyé à l'utilisateur.
- Mettre à jour le README (nouveautés et limites honnêtes).
- Commit et push.
- Garder un coût GPU par profil proche de l'actuel. Repères mesurés sur la même scène (unités relatives) : Élevé ≈ 95, Moyen ≈ 61, Faible ≈ 41, Économie ≈ 25.
- Ne jamais casser `touch`, `full` ni le détecteur mémoire.

**Risques**
- Le ralenti global peut casser l'audio et les particules : utiliser `sdt` partout, et une vitesse propre au joueur.
- Les cinématiques doivent se couper proprement en cas de menu ou de pause de l'application.
- Format de sauvegarde : toujours ajouter des clés, ne jamais en changer le sens.
- Performances des arènes de boss avec beaucoup de particules : le budget est géré par `particles.setBudget`.
