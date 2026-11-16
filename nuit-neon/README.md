# Nuit Néon

Jeu de course 3D pour mobile (navigateur ou application Android), jouable au tactile, au clavier ou à la manette.

## Modes de jeu

- **Course** : 9 circuits (5 en ville de nuit, 4 en pleine nature de jour) et 3 circuits de l'atelier, 1 à 31 adversaires, 1 à 99 tours, 5 niveaux de difficulté jusqu'à Légende.
- **Contre-la-montre** : seul en piste, contre son record.
- **Élimination** : toutes les 20 secondes, le dernier est éliminé.
- **Course à bonus** : des caisses sur la piste donnent l'un des 15 pouvoirs (voir plus bas).
- **Foot Turbo** : match de foot en voiture, de 1 contre 1 à 3 contre 3, avec boost, saut, double saut et vol.
- **Balade libre** : Route des Prés (6 km avec circulation) ou Ligne Droite (8 km sans aucune voiture).
- **Atelier** : trois circuits à créer soi-même, entièrement réglés par des nombres.
- **Garage** : 13 voitures et 4 motos à acheter, réglages à améliorer, moteur extrême.
- **Vue** : bouton VUE (ou touche C, ou Y à la manette) pour passer à la première personne.

## Conduite

- **Dérapage** : bouton DRIFT (Ctrl ou V au clavier, Espace hors Foot et Bonus, X à la manette). L'arrière chasse, la voiture tourne plus serré en perdant un peu de vitesse, les pneus fument et laissent des traces. Chaque glissade rapporte des points, convertis en crédits ; toucher un mur fait perdre la glissade en cours.
- **Collisions** : les véhicules échangent leur élan selon leur masse (une moto pèse trois fois moins qu'une voiture), rebondissent, frottent et pivotent selon le point d'impact. Contre un mur, la vitesse perpendiculaire est renvoyée amortie. Le mur s'arrête au ras de la carrosserie (une moto passe plus près qu'une voiture). Tant que la tôle frotte : gerbe d'étincelles et de braises sur toute la longueur du flanc, poussière, flammèches à haute vitesse, rayures qui restent sur le mur, grincement métallique, caméra qui vibre, et un freinage d'autant plus fort qu'on appuie contre le mur. Deux véhicules qui se frottent font aussi des étincelles.
- **Obstacles** : réglage Aucun / Parfois / Souvent / Chaos, tirés au hasard à chaque départ. Cônes et barrières s'envolent, les caisses éclatent, les piles de pneus font rebondir, les blocs de béton arrêtent net ; les tremplins font sauter (environ 1,5 m de haut à 200 km/h) et les zones turbo accélèrent. Les adversaires les évitent.
- **Motos** : elles se couchent vers l'intérieur du virage et la caméra s'incline avec elles.

## Ambiances

Au départ de chaque course ou balade, le réglage **Ambiance** change le ciel, la lumière et la météo de n'importe quel circuit (y compris ceux de l'atelier) :

- **D'origine** : le circuit tel qu'il a été dessiné.
- **Jour** : grand soleil, y compris sur les circuits de ville.
- **Nuit** : clair de lune, phares et néons allumés (les circuits de ville sont déjà de nuit).
- **Orage violent** : ciel noir, pluie battante poussée par le vent, brouillard, éclairs qui illuminent tout le décor, foudre visible au loin, tonnerre qui arrive après l'éclair selon la distance. La piste détrempée accroche 18 % de moins.

Les transformations sont dans `ambiance()` de `src/tracks.js`.

## Pouvoirs (Course à bonus)

Turbo, Nitro (turbo long et plus fort), Missile, Triple missile, Flaque d'huile, Mine, Bouclier, Éclair (foudroie ceux qui sont devant), Aimant (tire vers la voiture de devant), Super saut, Onde de choc (repousse les voisins), Fantôme (traverse voitures et obstacles), Géant (écrase ce qu'il touche), Gel (verglas pour les voitures de devant), Téléporteur (75 m plus loin).

## Économie

Chaque partie rapporte des crédits (CR) : classement, difficulté, nombre de tours, buts, distance en balade, dérapages. Ils servent à :

- acheter des véhicules (de 4 000 à 75 000 CR ; l'Azur et l'Onyx sont offerts) ;
- monter un réglage d'un niveau (300 CR × niveau visé) ;
- débloquer les paliers du **moteur extrême** : 500, 1 000, 2 500, 5 000 puis 10 000 km/h. Une fois un palier acquis, la vitesse de pointe de chaque véhicule se règle librement dans le garage (au km/h près), avec un multiplicateur d'accélération de 1 à 50. Freins et adhérence suivent, et la simulation est découpée en petits pas pour ne rien traverser.

## Atelier de circuits

Trois emplacements. Le tracé est tiré d'une graine : les mêmes nombres redonnent toujours le même circuit.

| Réglage | Plage |
| --- | --- |
| Graine du tracé | 0 à 99 999 |
| Nombre de virages | 3 à 40 |
| Taille du circuit | 100 à 2 500 m |
| Irrégularité | 0 à 100 % |
| Largeur de la route | 8 à 160 m |
| Obstacles | 0 à 600 |
| Tremplins, zones turbo | 0 à 120 chacun |
| Voitures de circulation | 0 à 80 |
| Vitesse de tous les véhicules | × 0,2 à × 30 |
| Adhérence | × 0,1 à × 5 |
| Gravité | × 0,05 à × 5 |
| Hauteur des sauts | × 0,2 à × 10 |
| Puissance du boost | × 0 à × 10 |
| Brouillard | × 0 à × 6 |

Le décor se choisit parmi les 9 ambiances du jeu. « Tout au hasard » tire tous les nombres d'un coup. Modifier le tracé remet le record du circuit à zéro.

## Sons

Tout est synthétisé par le jeu (`src/audio.js`), sans fichier audio. Chaque véhicule a son moteur : V8 au ralenti irrégulier (Rubis, Titan, Nomade), V10 (Azur, Fulgur), V12 (Mirage, Éclipse), biturbo (Cobalt, Spectre, Comète), monoplace, prototype, buggy, quatre-cylindres de moto, bicylindre de roadster. S'y ajoutent une boîte à 7 rapports avec coupure et détonation à chaque passage, le rupteur, la montée libre dans les tours à l'arrêt, le sifflement du turbo et sa soupape de décharge, les pétarades au lever de pied, le vent, le réacteur du boost, le moteur de l'adversaire le plus proche (avec effet Doppler), les explosions et le bang du mur du son. Pour changer le moteur d'un véhicule : champ `son` dans `src/carlist.js`.

## Radio

Un bandeau en haut à gauche (menu et course) affiche la station et le morceau ; ◀ ▶ changent de station, » passe au morceau suivant (R et T au clavier, croix haut et bas à la manette). Choix de la station et volume dans les options.

| Station | Style | Morceaux |
| --- | --- | --- |
| Néon FM 88.6 | synthwave | Néon Drive, Minuit Express, Rêve Chromé, Horizon Rose |
| Turbo FM 101.3 | trance | Turbo Cœur, Circuit Rouge, Hyperespace |
| Soleil FM 94.7 | house | Aube, Plage Dorée, Disco Bitume |
| Basse FM 107.9 | drum and bass | Adrénaline, Tunnel |
| Furie FM 98.2 | rock | Asphalte Brûlant, Dernier Tour |
| Ma musique | tes fichiers | ceux que tu ajoutes |

Les quatorze morceaux des cinq premières stations sont composés et joués en direct par `src/music.js`, sans fichier audio : chacun a sa structure (intro, couplet, refrain, pont, final, environ deux minutes), sa suite d'accords, sa mélodie, sa batterie avec roulements et cymbales, un écho, une réverbération et un effet de pompe sur la grosse caisse. À la fin d'un morceau, la station enchaîne sur le suivant.

**Ma musique** lit de vrais fichiers audio (mp3, m4a, ogg, wav, flac…) : bouton « Ajouter des morceaux » dans les options. Ils sont copiés dans la mémoire du jeu sur l'appareil et restent disponibles aux lancements suivants ; « Vider » les retire. Le jeu n'embarque aucune chanson du commerce.

## Lancer le jeu

```bash
npm install
```

```bash
npm run build
```

Le jeu est assemblé dans `dist/` ; il suffit de servir ce dossier (par exemple `python3 -m http.server` depuis `dist/`).

## Application Android (APK)

L'APK prêt à installer est dans `release/NuitNeon.apk` (Android 8.0 ou plus récent, paysage verrouillé, plein écran, fonctionne hors connexion).

Pour le refabriquer après une modification ou l'ajout d'un modèle :

```bash
ANDROID_JAR=/chemin/vers/android.jar ./android/build-apk.sh
```

Le script indique en tête les outils nécessaires. La clé de signature `android/keystore/nuit-neon.jks` n'est pas dans le dépôt : sans elle, le script ne peut pas signer, et un APK signé avec une autre clé ne s'installe pas par-dessus l'ancien (il faut d'abord désinstaller le jeu). Pour une publication sur le Play Store, il faut créer sa propre clé.

## Préparer un modèle 3D

Les modèles bruts pèsent souvent 25 à 70 Mo, bien trop pour un téléphone. `tools/prep-model.mjs` les allège (environ 30 000 triangles, textures 1024 px), les met à l'échelle, tourne l'avant vers -z et découpe les roues en pièces séparées pour qu'elles tournent :

```bash
cd tools && npm install
```

```bash
node prep-model.mjs entree.glb ../models/nom.glb '{"front":"+x","len":4.4,"type":"car"}'
```

Pour la version lointaine (utilisée quand le véhicule est loin de la caméra), refaire la même commande avec `"tris":5000` et la sortie `../models/nom_loin.glb`.

## Ajouter une voiture (modèle 3D)

1. Déposer le fichier `.glb` dans `models/`.
2. Ajouter une ligne dans `src/carlist.js` (un exemple commenté s'y trouve).
3. Relancer `npm run build`.

Le jeu s'occupe du reste : mise à l'échelle, pose au sol, roulis et tangage de la caisse, feux stop lumineux, ombre.
Si les roues sont des pièces séparées dont le nom contient `wheel`, `roue`, `tire`, `tyre`, `rim` ou `jante`, elles tournent et les roues avant braquent.
Si une pièce ou un matériau s'appelle `body`, `paint`, `carrosserie` ou `coque`, la peinture se change dans le garage.

Pour essayer un modèle sans toucher à la liste : ouvrir le jeu avec `?modele=models/fichier.glb` (ajouter `&rotY=3.14` si la voiture roule à l'envers).

## Voitures dessinées par le jeu

Sept voitures (Comète, Nomade, Titan, Mirage, Éclipse, Vortex, Fulgur) ne viennent pas d'un fichier 3D : elles sont construites par `src/procar.js` à partir de quelques tables de nombres dans `src/carlist.js` (longueur, largeur, ligne de capot, ligne d'épaule, garde au sol, position et taille des roues, forme du toit). Le jeu en tire la carrosserie avec ses passages de roues et ses ailes bombées, les jantes, les étriers, les feux, les bandes et le néon sous la caisse. Pour en créer une nouvelle, copier une ligne `design` et changer les nombres.

## Stats

Chaque voiture a 4 stats de 1 à 10 (vitesse max, accélération, maniabilité, adhérence). Les valeurs de `src/carlist.js` sont celles de départ ; elles se règlent ensuite dans le garage, jusqu'au niveau débloqué, et sont gardées sur l'appareil. Les adversaires utilisent les mêmes réglages quand ils conduisent la même voiture (le moteur extrême, lui, ne profite qu'au joueur).

## Fichiers

- `src/tracks.js` : tracé et ambiance des circuits et de la route libre
- `src/world.js` : décors (ville de nuit, nature de jour), route réfléchissante, herbe
- `src/arena.js` : arène du Foot Turbo
- `src/fx.js` : fumée, flammes, étincelles, traces de pneus
- `src/cars.js`, `src/carlist.js` : voitures, import des modèles, animations
- `src/procar.js` : voitures dessinées par le jeu
- `src/custom.js` : réglages et tracé des circuits de l'atelier
- `src/music.js`, `src/audio.js` : radio et morceaux, moteurs, chocs, crissements, orage
- `src/main.js` : conduite, collisions, obstacles, pouvoirs, économie, adversaires, modes de jeu, menus
- `src/page.html` : interface
