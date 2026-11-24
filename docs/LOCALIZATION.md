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

## Pluriels

`agr.trn("coins", n, ...)` choisit la forme selon la langue courante (comme `tr_n` de Godot) : la ligne
`coins.zero` si `n == 0` (facultative), sinon `coins.one`, `coins.two`, `coins.few`, `coins.many` ou
`coins.other` selon les règles CLDR de la langue (le russe a `one` / `few` / `many` : 1, 21 монета ; 3
монеты ; 5 монет ; le français met 0 et 1 au singulier), sinon `coins.other`, sinon `coins`. `{1}` est `n`,
`{2}`... les valeurs suivantes. Une forme vide dans une langue passe à la suivante (pas de texte d'une
autre langue).

```
key,fr,en,ru
coins.zero,Aucune pièce,No coins,
coins.one,{1} pièce,{1} coin,{1} монета
coins.few,,,{1} монеты
coins.many,,,{1} монет
coins.other,{1} pièces,{1} coins,{1} монеты
```

## Limites actuelles

- Pluriels : règles CLDR des langues courantes (anglais, français, langues germaniques et romanes, russe,
  ukrainien, polonais, tchèque, arabe, hébreu, lituanien, roumain, langues sans pluriel) ; pas de genre
  grammatical ni de déclinaisons (prévoir des clés distinctes).
- Les polices fournies (Inter) couvrent le latin, le grec et le cyrillique : pour le chinois, le japonais, le
  coréen, l'arabe ou l'hébreu, il faut fournir une police adaptée (champ `font` du texte).
- Écritures de droite à gauche (arabe, hébreu, persan, ourdou) : lettres arabes liées (formes initiale,
  médiane, finale, ligatures lam-alef, lettres persanes پ چ ژ ک گ ی) et ordre d'affichage selon un algorithme bidirectionnel simplifié
  (direction du paragraphe donnée par sa première lettre, nombres et mots latins gardés de gauche à droite,
  parenthèses retournées ; dans un paragraphe de droite à gauche, l'alignement `Left` part de la droite).
  Pas de mise en forme OpenType complète (HarfBuzz) : les écritures indiennes (devanagari...) et les
  diacritiques arabes empilés ne sont pas composés ; curseur des champs de saisie en ordre logique.
