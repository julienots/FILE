# LOST KINGDOM — missions avancées (à confier à un modèle puissant, une mission par session)

Chaque mission est autonome. Le **brief** est à copier tel quel au début de la session, en même temps que le lien du dépôt (branche `ccr-49335fa0-ubib49`). Avant tout, l'agent doit lire `lostkingdom/README.md` et la section 0 de `lostkingdom/docs/PLAN_MAJ_2.0.md`, qui expliquent comment compiler, tester et livrer.

Règles communes, à rappeler dans chaque brief :
- Tout est vérifié sur ordinateur, sans téléphone. Ne jamais dire qu'une chose marche sans l'avoir mesurée ou regardée en capture.
- Ne pas casser `lk_tests`, `--autoplay=touch`, `--autoplay=full` ni le détecteur mémoire.
- Rester dans le budget mobile (coût GPU par profil mesuré avec `LK_GPU_PROFILE=1`).
- Texte du jeu en français.

Les missions sont classées par impact sur le jeu.

---

## M1 — Import automatique des créatures (quadrupèdes, dragon, araignée)

**Pourquoi :** aujourd'hui, seuls les humanoïdes importés (Tripo, GLB) sont animés. Il en manque les loups, les chevaux, le Dragon Cendré et la Reine des Araignées.

**Brief à copier :**
> Dans le projet LOST KINGDOM, ajoute à `tools/import_character.py` (ou dans un nouvel outil `import_creature.py`) un import automatique pour les créatures non humanoïdes générées par Tripo : GLB texturé sans squelette, en pose debout neutre.
> - Ajuste les squelettes existants de `game/Characters.h` : `Q_*` pour les quadrupèdes (loup, sanglier, ours, cheval, cerf), dragon (`Q_*` + `Q_NECK2` + ailes `Q_WINGL1..R2`), `S_*` pour l'araignée.
> - Détecte les pattes, la tête, la queue et les ailes à partir de la géométrie (analyse en composantes, extrémités, symétrie).
> - Calcule des poids géodésiques comme pour les humanoïdes, puis écris un fichier `.lkm` avec ses articulations.
> - Côté jeu : étends le registre `importedModel(id)` de `Spawn.cpp` à `setupVisualCreature`, pour que `wolf.lkm`, `b_ash_dragon.lkm` ou `b_spider_queen.lkm` remplacent le modèle procédural avec toutes les animations existantes.
> - Ajoute des tests (articulations cohérentes, aucune patte attachée au mauvais os, fichiers corrompus rejetés) et un scénario d'autoplay qui filme chaque créature en marche, en course, en attaque et en vol pour le dragon.
> - Fournis des GLB de test : génère toi-même des maillages simples de quadrupède, de dragon et d'araignée pour valider l'outil sans dépendre de Tripo.

---

## M2 — Rendu « nouvelle génération » restant mobile

**Pourquoi :** c'est le plus gros gain visuel possible. Le terrain représente environ 85 % du coût de la scène.

**Brief à copier :**
> Améliore la qualité visuelle de LOST KINGDOM (OpenGL ES 3.0) sans dépasser le budget actuel de chaque profil (`LK_GPU_PROFILE=1`, profils 0 à 4). Dans l'ordre, en mesurant avant et après :
> 1. **Imposteurs octaédriques** pour les arbres lointains (LOD2) : rendu de chaque arbre en atlas, puis affichage en billboard. Cela permet de doubler la distance de vue des forêts.
> 2. **Rayons de lumière volumétriques** (god rays) en écran, à partir de la profondeur et du soleil, à basse résolution.
> 3. **Reflets de l'eau** : réflexion planaire à basse résolution, ou réflexions en espace écran sur les lacs, en Ultra et Élevé uniquement.
> 4. **Lumières en grappes** (*clustered lighting*) pour dépasser 8 lumières ponctuelles dans les villages la nuit.
> 5. **Terrain moins coûteux** : précalculer chaque chunk proche dans une texture (*virtual texturing* simple) au lieu de mélanger 8 couches à chaque pixel.
>
> Pour chaque étape : captures avant et après dans `build/shots`, mesure GPU par passe, réglage par profil, et le README mis à jour avec les chiffres.

---

## M3 — Animation de combat de qualité AAA

**Pourquoi :** les attaques sont des clips à images-clés courts. Le combat « badass » demande de l'anticipation, de l'impact et du poids.

**Brief à copier :**
> Dans LOST KINGDOM (`game/Animations.cpp`, `engine/anim/`), refais la bibliothèque d'animations de combat du héros et des ennemis humanoïdes :
> - Pour chaque type d'arme (épée, épée longue, dague, hache, marteau, lance, arc, bâton) : 3 coups de combo, un coup lourd, un coup chargé, une riposte, une exécution et un coup lanceur.
> - Les clips doivent avoir une anticipation lisible, des contacts précis (`EV_IMPACT`), une phase de récupération et un transfert de poids du bassin.
> - Ajoute une **ragdoll active** simple (Verlet sur le squelette) pour les morts et les gros impacts, avec un retour progressif au contrôle par l'animation.
> - Ajoute la **correspondance de pas** (*foot locking*) pendant les attaques, pour que les pieds ne glissent pas.
> - Filme chaque clip en vue latérale (scénario d'autoplay dédié) et compare avant et après en capture.

---

## M4 — IA de boss avec patterns uniques

**Brief à copier :**
> Remplace l'IA générique des 8 boss de LOST KINGDOM (`game/AI.cpp`) par des arbres de comportement par boss.
> - Chaque boss a 6 à 10 attaques, dont une signature (voir la section 2.8 de `docs/PLAN_MAJ_2.0.md`), et des enchaînements différents selon la phase et la distance.
> - Les attaques sont classées parables, imparables ou magiques, avec un télégraphe de couleur.
> - Les boss réagissent au style du joueur : s'il pare trop, ils feintent ; s'il reste loin, ils rapprochent la distance.
> - Ajoute un mode d'autoplay `bossrush` qui affronte chaque boss avec un bot jouant de façon semi-intelligente (esquives et parades), et qui enregistre la durée des combats et les dégâts subis. Équilibre la difficulté pour qu'un combat dure 2 à 4 minutes au niveau prévu.

---

## M5 — Musique adaptative et sound design

**Brief à copier :**
> Améliore l'audio de LOST KINGDOM (`tools/make_audio.py`, `engine/audio/`) :
> - **Musique en couches** (stems) qui se mélangent selon l'intensité du combat, la phase du boss et le moment de la journée, au lieu de changer de piste.
> - Un **thème principal** décliné par région et par personnage (leitmotiv d'Aldric, d'Alaric, de Malgrath).
> - **Impacts sonores** par matière (chair, métal, os, pierre, glace) avec plusieurs variations aléatoires, plus un son spécial pour l'esquive parfaite, la riposte, l'exécution et la Fureur.
> - Une **réverbération** qui dépend du lieu (grotte, cathédrale, plein air) : elle existe en partie, il faut la relier aux zones.
>
> Tout reste synthétisé par code (pas de samples externes) et en OGG à 48 kHz. Vérifie le volume de chaque fichier (`ffmpeg volumedetect`) et fournis des extraits WAV du mixage.

---

## M6 — Mode test de performance sur téléphone (le chaînon manquant)

**Pourquoi :** rien n'a jamais été mesuré sur un vrai téléphone.

**Brief à copier :**
> Ajoute à LOST KINGDOM un mode **Banc d'essai**, accessible depuis Réglages, qui joue un parcours scripté de 90 s (forêt, village, combat, eau, nuit) sur chaque profil.
> - Il mesure les images par seconde (moyenne, 1 % les plus lentes), le temps de l'image, la température si l'API le permet et la consommation de batterie (`BatteryManager` via JNI).
> - Il produit un **rapport texte copiable** à l'écran et dans un fichier.
> - L'utilisateur pourra lancer ce banc sur son téléphone et coller le rapport dans une session pour qu'on ajuste les profils.
> - Ajoute aussi la capture des plantages (`signal handler` et fichier de log lisible au prochain lancement), avec un bouton « Copier le rapport de plantage ».

---

## M7 — Contenu : villes, donjons, quêtes secondaires à embranchements

**Brief à copier :**
> Enrichis le monde de LOST KINGDOM :
> - Une **ville principale** par région (aujourd'hui ce sont surtout des villages), avec quartiers, marché, taverne, remparts, PNJ avec emploi du temps et dialogues uniques.
> - **6 nouveaux donjons** à énigmes variées (miroirs de lumière, plaques de pression, plateformes, eau qui monte).
> - **15 quêtes secondaires** avec choix moraux et conséquences visibles dans le monde (drapeaux d'histoire, voir la section 3.0 du plan).
>
> Respecte le style procédural existant (`game/Props.cpp`, `game/World.cpp`, `game/Dungeon.cpp`) et les tests de cohérence des données. Vérifie chaque ajout par des captures.

---

## M8 — Moteur : moteur de rendu Vulkan (optionnel, très gros chantier)

**Brief à copier :**
> Évalue puis, si c'est raisonnable, implémente un second moteur de rendu **Vulkan** dans l'AGR Engine (Android API 26+), choisi au lancement quand l'appareil le supporte, avec repli automatique sur OpenGL ES 3.0.
> - Commence par un rapport : gains attendus sur le coût CPU des appels de dessin, risques, travail nécessaire.
> - N'implémente que si le gain est démontrable. Garde le même rendu à l'image près (comparaison de captures).

---

### Conseils pour confier ces missions
- **Une mission par session**, dans l'ordre M1 → M6 si l'objectif est la qualité du jeu. M6 est la plus importante si l'on veut des performances réelles sur téléphone.
- Demandez à chaque fois : **un APK vérifié, le README mis à jour et des captures avant et après**.
- Si une mission dépend de modèles 3D Tripo, envoyez les GLB dans la même session (texture 1K ou 2K pour rester sous la limite d'envoi).
