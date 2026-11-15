# Logique visuelle (programmer sans code)

Un **graphe de logique** (`.agrlogic`) se branche sur une entité comme un script Lua : des
**événements** (démarrage, chaque image, touches, collisions, minuteurs, clics, messages) déclenchent
des chaînes d'**actions** reliées par des flèches blanches ; des **valeurs** (nombres, textes,
entités, positions, calculs) arrivent par des liaisons de couleur. Le moteur traduit le graphe en Lua
au chargement : il tourne partout où les scripts tournent (éditeur en mode Jeu, player PC, Android),
avec rechargement à chaud.

## Dans l'éditeur

- **Assets › Nouvelle logique** crée un graphe (« Au démarrage → Écrire dans la console ») et l'ouvre
  dans le panneau **Logique visuelle** (aussi : Affichage › Logique visuelle, clic sur un `.agrlogic`,
  bouton « Éditer la logique visuelle » du composant Script).
- **Ajouter un nœud** : clic droit (ou appui long) sur la toile ; menu par catégories avec recherche.
- **Relier** : tirer d'une broche de sortie (à droite) vers une entrée (à gauche). Une liaison lâchée
  dans le vide ouvre le menu limité aux nœuds compatibles et relie le nœud créé. Tirer depuis une
  entrée déjà reliée détache la liaison.
- **Valeurs** : une entrée non reliée se règle directement sur le nœud (nombre, texte, case à cocher ;
  entité : nom d'une entité de la scène, vide = « moi »).
- **Toile** : glisser le fond pour se déplacer, molette pour zoomer ; Suppr efface le nœud choisi,
  Ctrl+D le duplique, Ctrl+Z / Ctrl+Y annulent / rétablissent, Ctrl+S enregistre.
- **Variables** (panneau de gauche) : nombre, texte ou booléen ; elles deviennent les propriétés du
  script, modifiables par entité dans l'inspecteur.
- **Vérification** en direct : erreurs (nom de variable invalide, valeur non numérique, boucle
  d'exécution...) en rouge, nœuds fautifs encadrés de rouge ; avertissements (événement sans suite,
  nœud isolé) en orange. « Code Lua généré » montre le script produit.
- **Attacher à la sélection** ajoute un composant Script à l'entité choisie et y branche le graphe
  (on peut aussi glisser le `.agrlogic` sur un objet de la vue).

## Nœuds

| Catégorie | Nœuds |
|---|---|
| Événements | Au démarrage, À chaque image (dt), Toutes les N secondes, Touche appuyée / relâchée, Action appuyée, Collision, Entrée / sortie de zone, Clic sur le bouton, Message reçu |
| Flux | Si (vrai / faux), Séquence, Répéter (index), Pour chaque enfant, Une seule fois, Après un délai |
| Actions | Placer, Déplacer (par seconde), Orienter, Tourner, Taille, Vitesse, Impulsion, Afficher / cacher, Détruire, Cloner, Changer le texte, Jouer un son, Écrire dans la console, Changer une variable, Ajouter à une variable, Changer de scène, Envoyer un message, Paramètre / déclencheur d'animation, Sauvegarder, Vibrer |
| Effets | Animer (tween), Gerbe de particules, Secouer la caméra, Texte flottant, Flash d'écran |
| Valeurs | Moi, Entité nommée, Nombre, Texte, Vrai / faux, Variable, Position, Vitesse actuelle, Nom, Est active, Touche enfoncée, Axe d'entrée, Temps, Hasard, Valeur sauvegardée |
| Maths | Additionner, Soustraire, Multiplier, Diviser, Et, Ou, Non, Comparer, Assembler du texte, Distance, Limiter |

« Message reçu » et « Envoyer un message » font dialoguer des graphes et des scripts Lua : le nœud
« Message reçu » nommé `hit` devient la fonction `hit(sender, value)` du script ; un bouton d'UI dont
le champ *message* vaut `hit` l'appelle aussi.

## Format et moteur

`.agrlogic` = JSON (`format: "agr.logic"`, `version: 1`, variables, nœuds avec position et valeurs,
liaisons). Code : `engine/script/include/agr/script/LogicGraph.hpp` (catalogue, édition, JSON,
compilateur `logic::compile`) ; le `ScriptEngine` compile un `.agrlogic` au chargement et signale les
erreurs comme celles d'un script. Panneau : `editor/ui/src/LogicPanel.cpp`.

Tests : suite `logic` de `tests/test_logic.cpp` (catalogue entier compilé, règles de liaison, JSON,
erreurs, exécution réelle : événements, variables, boucles, conditions, délais, messages, touches) ;
le test de bout en bout de l'éditeur (`--smoke-test`) crée un graphe, relie des nœuds, annule /
rétablit, enregistre, l'attache à un cube et vérifie que le cube bouge en mode Jeu.

## Limites actuelles

- Pas de sous-graphes / fonctions réutilisables ni de commentaires sur la toile.
- Les sorties d'actions (« Cloner › Copie ») et l'index des boucles ne servent que dans la suite de
  la même chaîne.
- Pas de débogage pas à pas (le Lua généré et la console aident).
