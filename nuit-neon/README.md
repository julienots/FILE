# Nuit Néon

Jeu de course 3D de nuit pour mobile (navigateur) : 5 circuits, 5 adversaires avec 4 niveaux de difficulté, garage avec choix de la voiture, peinture et réglage des stats.

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

- `src/tracks.js` : tracé et ambiance des 5 circuits
- `src/world.js` : ville, route mouillée réfléchissante, décor
- `src/cars.js`, `src/carlist.js` : voitures, import des modèles, animations
- `src/main.js` : conduite, adversaires, course, menus
- `src/page.html` : interface
