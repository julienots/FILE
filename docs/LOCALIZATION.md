# Localisation (traductions)

Les textes d'un jeu sont rangés dans une table : une **clé** par ligne, une **colonne par langue**,
enregistrée en CSV (`assets/localization.csv`) pour être modifiable aussi dans un tableur.

```
key,fr,en,es
menu.play,Jouer,Play,Jugar
hud.score,"Score : {1}","Score: {1}","Puntos: {1}"
```

Module `agr::runtime` (`Localization.hpp`). Tests : `tests/test_localization.cpp`, test de script, test
de bout en bout de l'éditeur (bascule français / anglais du HUD de la Vitrine 3D).

## Fonctionnement

- **Langue au démarrage** : celle choisie précédemment par le joueur (mémorisée dans la sauvegarde
  locale), sinon la première des langues du système disponible dans la table (`fr-CA` utilise `fr`),
  sinon la première colonne.
- **Repli** : une cellule vide affiche la traduction de la première langue, une clé absente affiche la clé
  elle-même (les oublis se voient tout de suite).
- **Textes d'interface** : le champ `localizationKey` d'un `UIText` remplace son texte par la traduction
  et suit les changements de langue.
- **Scripts** : `agr.tr("hud.score", score)` (avec `{1}`, `{2}`…), `agr.locale.set("en")`,
  `agr.locale.get()`, `agr.locale.list()`.
- CSV : UTF-8 (avec ou sans BOM), séparateur `,` ou `;` (tableurs en français), champs entre guillemets
  pour les virgules, guillemets doublés et retours à la ligne.

## Dans l'éditeur

*Affichage › Localisation* : tableau des clés et des langues, ajout de langues et de clés, recherche,
cellules manquantes surlignées, langue d'**aperçu** des textes de la scène. Les modifications sont
enregistrées en quittant une cellule. Les modèles de projet contiennent une table français / anglais /
espagnol pour leur HUD.

## Limites actuelles

- Pas de pluriels ni de règles de grammaire par langue (prévoir des clés distinctes).
- Les polices fournies (Inter) couvrent le latin, le grec et le cyrillique : pas les écritures
  chinoise, japonaise, coréenne ou arabe (il faut fournir une police adaptée), et pas d'écriture de droite
  à gauche.
