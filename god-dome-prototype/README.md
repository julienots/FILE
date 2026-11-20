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

- **Herbe GPU** : jusqu'à 134 000 brins individuels générés dans le vertex shader autour du joueur, avec hauteur et masque lus dans des textures, rafales de vent, translucidité à contre-jour, couleur variable par brin et ombres reçues.
- **Sol** : texture de détail générée (sol herbeux, terre et gravier, roche stratifiée), mélange par sommet, roche en projection triplanaire, relief par différences finies.
- **Arbres** : feuillus en grappes de cartes de feuilles dessinées, sapins en verticilles de branches à aiguilles, ombres découpées à l'alpha, translucidité, vent.
- **Eau** : normales animées dans le sens du courant, Fresnel, reflet solaire, écume des berges et cascades, brume.
- **Dôme** : coupole géodésique ; verre (Fresnel, irisation, joints) vu de l'extérieur ; « ciel simulé » vu de l'intérieur, qui s'amincit au zénith et dans la direction du Créateur.
- **Éclairage** : soleil et lune qui tournent sous la voûte, ombres PCF dont le cadrage suit le mode, ciel hémisphérique, brouillard atmosphérique, bloom HDR, tone mapping ACES et étalonnage.
- **Performance** : résolution dynamique sur téléphone, LOD des personnages, village fusionné par matériau, ombres limitées aux personnages proches.

| Qualité | Appels de dessin (Créateur / Habitant) | Triangles (Habitant) |
|---|---|---|
| haute | 388 / 308 | 4,2 M |
| moyenne | 388 / 308 | 2,0 M |
| basse | 375 / 295 | 1,6 M |

Ces chiffres incluent toutes les passes (ombres, scène, bloom). Ils sont mesurés dans un navigateur sans GPU : les images par seconde réelles doivent être relevées sur l'appareil (compteur en haut à droite).

## Code

`src/main.js` (rendu, caméras, modes, UI) · `terrain.js` · `ground.js` · `grassfield.js` · `vegetation.js` · `water.js` · `village.js` · `characters.js` · `dome.js` · `game.js` (quêtes, combat, objets, météo, sauvegarde) · `shared.js` · `textures.js` · `noise.js`.
Moteur : Three.js r170 (copie locale dans `lib/`).

Les captures de `screenshots/` proviennent du jeu réellement exécuté (Chromium headless, rendu logiciel).
