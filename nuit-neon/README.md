# Nuit Néon

Jeu de course 3D pour mobile (navigateur ou application Android), jouable au tactile, au clavier ou à la manette.

## Modes de jeu

- **Course** : 9 circuits (5 en ville de nuit, 4 en pleine nature de jour, dont une côte au coucher du soleil), 3 à 15 adversaires, 5 niveaux de difficulté jusqu'à Légende.
- **Contre-la-montre** : seul en piste, contre son record.
- **Élimination** : toutes les 20 secondes, le dernier est éliminé.
- **Course à bonus** : des caisses sur la piste donnent un objet (turbo, missile, flaque d'huile, bouclier).
- **Foot Turbo** : match de foot en voiture, de 1 contre 1 à 3 contre 3, avec boost, saut, double saut et vol.
- **Balade libre** : Route des Prés (6 km avec circulation) ou Ligne Droite (8 km sans aucune voiture).
- **Garage** : 6 voitures et 4 motos, 4 stats réglables par véhicule.
- **Vue** : bouton VUE (ou touche C, ou Y à la manette) pour passer à la première personne.

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

Le script indique en tête les outils nécessaires. La clé de signature `android/keystore/nuit-neon.jks` est une clé de projet : elle permet d'installer les mises à jour par-dessus l'ancienne version. Pour une publication sur le Play Store, il faut créer sa propre clé.

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

## Stats

Chaque voiture a 4 stats de 1 à 10 (vitesse max, accélération, maniabilité, adhérence). Les valeurs de `src/carlist.js` sont celles de départ ; elles se règlent ensuite dans le garage et sont gardées sur l'appareil. Les adversaires utilisent les mêmes réglages quand ils conduisent la même voiture.

## Fichiers

- `src/tracks.js` : tracé et ambiance des circuits et de la route libre
- `src/world.js` : décors (ville de nuit, nature de jour), route réfléchissante, herbe
- `src/arena.js` : arène du Foot Turbo
- `src/fx.js` : fumée, flammes, étincelles
- `src/cars.js`, `src/carlist.js` : voitures, import des modèles, animations
- `src/main.js` : conduite, adversaires, modes de jeu, menus
- `src/page.html` : interface
