# Animation (phase 7)

Module `agr::animation`. Deux niveaux, complémentaires de l'animation image par image des sprites
([2D.md](2D.md)) :

1. **Clips de propriétés** (`.agranim`) : des courbes qui animent n'importe quel champ de composant de
   l'entité et de ses enfants.
2. **Machines à états** (`.agrctrl`) : des états (un clip de propriétés et / ou un clip de sprite) reliés
   par des transitions qui dépendent de paramètres réglés par les scripts.

Tests : `tests/test_animation.cpp`, test de script d'animation, test de jeu des modèles (héros piloté
par sa machine à états), test de bout en bout de l'éditeur.

## Clips et courbes

- Une **piste** anime un canal d'un champ réfléchi : `cible` (chemin de noms sous l'entité animée,
  vide = elle-même, `"Bras/Main"`), composant, champ, canal (x, y, z, w d'un vecteur ou d'une couleur).
  Tout champ est animable : nombres, booléens, entiers, énumérations, vecteurs, couleurs, rotations
  (en degrés d'Euler) et textes (`SpriteAnimator.clip`, une texture...).
- **Clés** : interpolation constante, linéaire, **lissée** (tangentes automatiques, sans dépassement
  aux extrémums ni sur des données monotones) ou à **tangentes** réglables.
- **Événements** : à un instant donné, le script de l'entité reçoit `onAnimationEvent(nom)` (une
  fois par passage, boucles comprises : bruits de pas, tirs...).
- Durée, boucle ; un clip non bouclé garde sa dernière valeur.

## Machines à états

| Élément | Détail |
|---|---|
| Paramètres | nombre, entier, booléen, **déclencheur** (consommé par la transition qu'il active) |
| États | clip de propriétés, clip de sprite (`SpriteAnimator.clip`), vitesse ; un état initial |
| Transitions | depuis un état ou **tout état**, conditions toutes requises (`>`, `<`, `==`, `!=`, vrai, faux), **instant de sortie** (fraction du clip), **fondu** (mélange des valeurs animées pendant la durée) |
| Contrôles | références vérifiées (états, paramètres, transitions qui se déclencheraient en boucle) : messages dans l'inspecteur et la console |

Le composant `Animator` joue un `controller` (machine à états) ou, à défaut, un `clip` seul ; `speed`
et `playing` le règlent. Il tourne après les scripts (qui règlent les paramètres) et avant le rendu.
Le script reçoit `onStateEnter(nom)` à chaque changement d'état.

```lua
function Hero:onUpdate(dt)
    self.entity:setParameter("speed", math.abs(move))
    self.entity:setParameter("grounded", self.grounded)
    if attack then self.entity:setTrigger("attack") end
end
function Hero:onAnimationEvent(name) if name == "step" then agr.audio.play("assets://audio/step.wav") end end
```

## États de mélange (blend tree 1D)

Un état peut mélanger plusieurs clips selon un paramètre `float` ou `int` (l'équivalent du
`BlendSpace1D` de Godot) : par exemple *Locomotion* = Idle à 0, Walk à 2, Run à 5 selon `speed`.

```json
{ "name": "Locomotion", "clip": "", "speed": 1.0,
  "blend": { "parameter": "speed",
             "motions": [ { "clip": "assets://hero.glb#Idle", "threshold": 0 },
                          { "clip": "assets://hero.glb#Walk", "threshold": 2 },
                          { "clip": "assets://hero.glb#Run",  "threshold": 5 } ] } }
```

- Les deux mouvements qui encadrent la valeur sont mélangés (poids linéaire entre leurs seuils) ; en
  dessous du premier seuil ou au-dessus du dernier, un seul joue.
- **Phases synchronisées** : les deux clips avancent de la même fraction de leur propre durée (la durée
  du cycle est interpolée), donc les pieds restent en phase même si Walk dure 0,8 s et Run 0,5 s.
- Fondus, transitions, temps de sortie (`exitTime`, sur la phase) et déclencheurs fonctionnent comme
  pour les autres états, y compris pour entrer dans un mélange ou en sortir.
- Le mélange s'applique aux pistes de transformation (squelettes, nœuds importés) ; les pistes de
  propriétés et les événements viennent du mouvement de seuil inférieur.
- Éditeur : *Machine à états* ➜ état ➜ *Mélange (blend tree 1D)* : paramètre, mouvements (glisser un
  clip, seuil, ajouter / retirer).
- Le *Duel des Tours* l'utilise : `Locomotion` mélange Idle et Walk selon la vitesse réelle de l'unité.

## Animation squelettique (modèles glTF)

- **Import** : les *skins* glTF (articulations, matrices de liaison inverses, `JOINTS_0` /
  `WEIGHTS_0`, 4 influences par sommet, poids normalisés) et les **animations** (translation,
  rotation, échelle ; interpolation pas à pas ou linéaire ; les splines cubiques sont lues par leurs
  valeurs, sans tangentes, avec un avertissement). Les poids de *morph targets* ne sont pas importés.
- **Instanciation** : les nœuds du modèle deviennent des entités (le squelette est une hiérarchie
  d'entités ordinaires, visible et modifiable) ; un `MeshRenderer` à peau référence son skin.
- **Clips** : une animation importée est un clip nommé `"<modèle>#<animation>"`
  (`assets://3d/sprout.glb#Dance`), utilisable comme `clip` d'un Animator ou d'un état de machine à
  états, avec fondus entre clips (interpolation sphérique des rotations). Glisser un modèle animé dans
  la scène lui ajoute un Animator qui joue sa première animation ; l'inspecteur liste ses clips.
- **Rendu** : skinning sur GPU (palette de 128 articulations maximum par maillage, buffer uniforme),
  ombres comprises, sur Vulkan, OpenGL et GLES ; les volumes englobants sont élargis pour le culling.
- **Vitrine 3D** : la plante `sprout.glb` (4 articulations, clips *Idle* et *Dance* générés par
  `tools/generate_template_character.py`) danse quand une balle la touche (script + machine à états).

## Dans l'éditeur

- **Timeline** (*Affichage › Animation*, ou bouton *Timeline* de l'Animator) : clips de l'Animator
  sélectionné (ou de l'un de ses enfants) et de ses états ; *Nouveau clip* ; *+ Piste* (objet,
  composant, champ ; clé à la valeur actuelle) ; règle temporelle avec **tête de lecture** à glisser,
  lecture, **aperçu** dans la scène (les valeurs animées reprennent leur état d'origine à la fin de
  l'aperçu, avant de jouer et avant d'enregistrer la scène) ; clés : double-clic pour en ajouter,
  glisser pour les déplacer (pas de 10 ms), Suppr, valeur / interpolation / tangentes ; *Clé* :
  enregistre la pose actuelle (modifier l'objet dans l'inspecteur ou au gizmo, puis *Clé*) ; vue de la
  courbe de la piste choisie ; événements ; Ctrl + molette : zoom.
- **Machine à états** (*Affichage › Machine à états*, ou bouton de l'Animator) : graphe des états
  (glisser pour placer, clic molette pour déplacer la vue), flèches des transitions, clic droit pour
  ajouter un état, une transition, choisir l'état initial ou supprimer ; propriétés de l'état et de
  la transition (conditions, instant de sortie, fondu) ; paramètres ; problèmes détectés. En mode Jeu,
  l'état courant est encadré en rouge et l'inspecteur montre l'état et les paramètres.
- Les fichiers sont enregistrés par le bouton *Enregistrer* du panneau ; le jeu en cours les recharge.
- Les modèles de projet contiennent `hero.agrctrl` (Idle / Run / Air), des clips de pulsation, de
  rotation et de lumière vacillante.

## Performances

1 000 Animators (dont 500 machines à états), 3 courbes chacun : ~0,18 ms par image sur la machine de
test ([BENCHMARKS.md](BENCHMARKS.md)).

## Limites actuelles

- Mélange en une dimension seulement (pas de *blend space* 2D) ; pas de couches ni de masques
  d'animation, ni de cinématique inverse ; pas de *morph targets* ; au plus 128 articulations par
  maillage, 4 influences par sommet.
- Les clips importés ne s'éditent pas dans la timeline (ils viennent du fichier du modèle).
- Une transition ne peut pas être interrompue pendant son fondu ; une seule transition par image.
- Timeline : pas de sélection de plusieurs clés, de copier-coller ni de poignées de tangentes à
  glisser (valeurs numériques) ; les modifications d'un clip ou d'une machine à états ne passent pas
  par *Annuler* (elles sont enregistrées dans leur fichier avec *Enregistrer*).
- L'aperçu de la timeline ne fonctionne qu'en mode Édition.
