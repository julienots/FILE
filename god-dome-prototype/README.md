# GOD DOME

Jeu mobile 3D : vous êtes le Créateur d'un monde enfermé sous un dôme de verre, et vous pouvez y entrer pour devenir l'un de ses habitants.
Lorsque vous levez les yeux, vous voyez la voûte qui enferme votre univers, et derrière elle la silhouette du Créateur.

Le mode Créateur et le mode Habitant affichent **une seule et même scène 3D** : une maison construite depuis l'extérieur est la maison dans laquelle l'habitant entre.

## Lancer le jeu

Aucune installation : c'est une page web WebGL 2 qui fonctionne dans Chrome sur Android et sur ordinateur.

```bash
cd god-dome-prototype
python3 -m http.server 8080
```

Ouvrez ensuite `http://localhost:8080` (ou `http://<ip-de-l-ordinateur>:8080` depuis le téléphone, sur le même Wi-Fi).
Qualité forcée : `?q=high`, `?q=medium` (par défaut sur téléphone), `?q=low`.

### Commandes

| | Tactile | Clavier / souris |
|---|---|---|
| Mode Créateur | glisser = tourner, pincer = zoomer, toucher le sol = point d'entrée | glisser, molette, clic |
| Entrer / sortir | « Entrer dans le monde » / « Redevenir le Créateur » | Entrée / Échap |
| Se déplacer | joystick | ZQSD / WASD / flèches, Maj = courir |
| Sauter / Frapper | boutons | Espace / F |
| Parler / Ramasser | bouton contextuel | E |
| Regarder le ciel | bouton | C |
| Journal / Sac | boutons | J / I |


## Histoire : la Voûte d'Aube

Vous êtes le Créateur. De vos mains est né **Aube**, un monde sous une bulle de verre. Mais le Vide a trouvé une fissure : la Voûte se brise et l'Ombre s'infiltre.

- **Prologue** au premier lancement, et *Chroniques d'Aube* (le lore) dans le Journal.
- **Acte I — Les Éclats de la Voûte** : le Vieil Oswin vous envoie chercher 3 éclats (pied de la cascade, forêt de l'est, rive du lac).
- **Acte II — Le Cœur d'Ombre** : un boss vous attend dans la Faille, au pied des falaises du nord-ouest. Il lance des orbes et invoque des Ombrelins.
- **Épilogue** après sa destruction ; le monde reste ensuite libre à façonner.

**But du jeu : retrouver les 3 éclats puis détruire le Cœur d'Ombre.**

## Foi et pouvoirs

La **Foi** de vos habitants (jauge en haut de l'écran) monte avec les quêtes, les éclats, les Ombrelins chassés et les prières quotidiennes. Elle alimente vos pouvoirs.

| Créateur (vue du dôme) | Habitant (3e personne) |
|---|---|
| ⚡ **Foudre** (20) : anéantit les Ombrelins au point touché, blesse le boss | **Onde de lumière** : repousse et blesse autour de vous (débloquée au premier éclat) |
| ✦ **Bénédiction** (25) : soigne l'habitant, paralyse les Ombres | **Élan** : ruée rapide |
| + Maison (15), + Arbres (5), ☂ Pluie | **Foudre divine** (20) : l'habitant appelle la foudre du Créateur sur l'Ombre la plus proche |


## Pouvoirs de l'habitant (v0.5)

Un seul bouton **Pouvoir**. Touchez-le pour lancer le pouvoir choisi ; glissez dessus ou maintenez-le pour ouvrir la roue et en changer.

| Pouvoir | Effet |
|---|---|
| ☄ Comète | Projectile d'énergie : explosion, **cratère réellement creusé** dans le sol, lave qui refroidit, débris de roche, onde de choc à l'écran |
| ✷ Rayon (maintenir) | Faisceau continu qui laboure une tranchée incandescente |
| ϟ Tempête | 12 éclairs autour de la cible : arbres soufflés, sol criblé de cratères |
| 🜁 Envol | Vol libre (on vole là où l'on regarde), traînées lumineuses, lignes de vitesse ; piquer vers le sol = **Impact météore** |
| ◎ Onde, ➤ Élan | Repousser autour de soi, ruée |

La destruction est réelle : le relief, les collisions, l'herbe et la végétation suivent les cratères. L'énergie (jauge bleue) se recharge seule.

## Interface mobile

- Joystick à gauche (à fond = courir).
- 3 boutons à droite : action contextuelle (Frapper / Parler / Ramasser), Sauter (Monter en vol), Pouvoir.
- Un menu ☰ (Journal, Sac, Ciel, Commandes).
- En mode Créateur : barre d'icônes compacte.

## Fluidité et stabilité (v0.6)

- **Cratères instantanés** : seules les lignes du terrain touchées sont recalculées et renvoyées au GPU, avec des normales locales. Une explosion coûte ~0,5 ms au lieu de 20 à 30 ms de calcul plus un gros transfert ; le Rayon et la Tempête ne saccadent plus.
- **Shaders préchauffés au chargement** : premier cratère, lumière d'explosion, intérieur des maisons, herbe proche. Plus de gel d'une seconde à la première utilisation.
- **Sans fuites mémoire** : géométries et matériaux des effets libérés. Le nombre d'objets GPU reste stable après des centaines d'explosions.
- **Économie au repos** : particules et débris inactifs ne sont plus envoyés au GPU, et la lumière d'explosion comme celle de maison sont retirées de l'éclairage quand elles sont éteintes. L'interface n'écrit dans la page que ce qui change.
- **Boucle plafonnée à 60 i/s** : un écran 120 Hz ne fait plus chauffer puis ralentir le téléphone.
- **Résolution dynamique avec hystérésis** : elle ne fait plus de va-et-vient qui saccade. Un mode « éco » automatique (ombres une image sur deux, sans halo) prend le relais si le téléphone peine encore.
- **Corrections de jeu** :
  - le Rayon tient vraiment tant qu'on maintient le doigt (l'appui long ouvrait la roue) ;
  - les commandes sont relâchées quand l'appli passe en arrière-plan ;
  - reprise propre si Android retire le contexte graphique (au lieu d'un écran noir) ;
  - la lave des cratères a des bords ronds (et non plus en escalier) ;
  - les sbires du boss ne réapparaissent plus après sa mort ;
  - les Ombrelins rétrécissent en mourant ;
  - le compteur d'appels de dessin compte toute l'image.

## Direction artistique

Style « jeu » assumé plutôt qu'un réalisme approximatif :
- ombrage cartoon en paliers sur tous les matériaux, avec un liseré de lumière sur les bords ;
- contours noirs en post-traitement ;
- personnages ronds à grosse tête ;
- maisons aux arêtes arrondies.

## Contenu

- **Mode Créateur** : vue orbitale du monde miniature, cycle jour/nuit et vitesse du temps, pluie, construction de maisons et plantation d'arbres (sauvegardées), choix du point d'entrée.
- **Transition** : la caméra plonge vers la zone choisie, traverse la paroi (onde lumineuse sur le verre), descend jusqu'à hauteur d'homme ; animation inverse pour ressortir.
- **Mode Habitant** : 3e personne, marche, course, saut, collisions (murs, arbres, rochers, falaises), maisons ouvertes et meublées, caméra qui évite les murs.
- **Quêtes** : *Les Éclats de la Voûte* (principale, trois éclats à retrouver), *Les pommes de Brune*, *Ombres dans la forêt*. Journal, objectif affiché à l'écran, marqueurs « ! » au-dessus des personnages.
- **Combat** : Ombrelins hostiles dans la forêt de l'est, coup de bâton, recul, barre de vie, réapparition au village.
- **Objets** : pommes, herbes (soins), éclats, essences, pièces ; inventaire.
- **Monde vivant** : 10 villageois autonomes avec dialogues, moutons qui broutent, oiseaux, moulin, nuages qui dérivent sous la voûte.
- **Sauvegarde** locale (inventaire, quêtes, constructions).

## Rendu

Tout est procédural : aucune texture ni aucun modèle externe.

- **Herbe GPU (v2)** : deux couches de densité générées dans le vertex shader (en qualité haute, jusqu'à ~130 000 brins serrés à moins de 9 m, puis une couche dense jusqu'à 30 m). Les brins se regroupent en touffes ouvertes en éventail. Rafales de vent, normales arrondies, translucidité à contre-jour, reflet spéculaire, occlusion à la racine, ombres reçues.
- **Interaction avec l'herbe** : 16 « pousseurs » (habitant, villageois, moutons, Ombrelins, traînée du joueur). L'herbe s'écarte et se couche, frémit au passage puis se relève en ~3 s, ce qui laisse un sillon visible derrière soi.
- **Passe atmosphère** (post-traitement depuis la profondeur) : occlusion ambiante en espace écran, brouillard de hauteur éclairé vers le soleil, rayons crépusculaires à travers le feuillage.
- **Feuillage proche de la caméra** : il se dissout en trame au lieu de boucher la vue.
- **Sol** : texture de détail générée (sol herbeux, terre et gravier, roche stratifiée), mélange par sommet, roche en projection triplanaire, relief par différences finies.
- **Arbres** : feuillus en grappes de cartes de feuilles dessinées, sapins en verticilles de branches à aiguilles, ombres découpées à l'alpha, translucidité, vent.
- **Eau** : normales animées dans le sens du courant, Fresnel, reflet solaire, écume des berges et cascades, brume.
- **Dôme** : coupole géodésique ; verre (Fresnel, irisation, joints) vu de l'extérieur ; « ciel simulé » vu de l'intérieur, qui s'amincit au zénith et dans la direction du Créateur.
- **Éclairage** : soleil et lune qui tournent sous la voûte, ombres PCF dont le cadrage suit le mode, ciel hémisphérique, brouillard atmosphérique, bloom HDR, tone mapping ACES et étalonnage.
- **Performance** : résolution dynamique sur téléphone, LOD des personnages, village fusionné par matériau, ombres limitées aux personnages proches.

| Qualité | Appels de dessin (Créateur / Habitant) | Triangles (Habitant) |
|---|---|---|
| haute | 389 / 310 | 4,5 M |
| moyenne | 389 / 310 | 2,3 M |
| basse | 375 / 296 | 1,6 M |

Ces chiffres incluent toutes les passes (ombres, scène, bloom). Ils sont mesurés dans un navigateur sans GPU : les images par seconde réelles doivent être relevées sur l'appareil (compteur en haut à droite).

## Code

`src/main.js` (rendu, caméras, modes, UI) · `terrain.js` · `ground.js` · `grassfield.js` · `atmosphere.js` · `models.js` (modèles .glb externes, voir `models/README.md`) · `vegetation.js` · `water.js` · `village.js` · `characters.js` · `dome.js` · `game.js` (quêtes, combat, objets, météo, sauvegarde) · `shared.js` · `textures.js` · `noise.js`.
Moteur : Three.js r170 (copie locale dans `lib/`).

Les captures de `screenshots/` proviennent du jeu réellement exécuté (Chromium headless, rendu logiciel).
