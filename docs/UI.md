# Interface de jeu (phase 7)

Module `agr::ui`. Une interface est une hiérarchie d'entités sous un **Canvas** ; elle est dessinée
par-dessus la scène, à la résolution de l'écran, et réagit à la souris et au **multi-touch**. Tests :
`tests/test_ui.cpp`, tests GPU `tests/gpu/test_overlay.cpp` (Vulkan, OpenGL, OpenGL ES), test de jeu
des modèles et test de bout en bout de l'éditeur.

## Coordonnées et adaptation aux écrans

- Origine en **haut à gauche**, Y vers le bas, en unités du canvas.
- `UICanvas` : `ScaleWithScreen` (par défaut) adapte les unités à l'écran à partir de la résolution
  de référence (1920 × 1080) ; `match` choisit de suivre la largeur (0), la hauteur (1) ou un
  mélange — le même HUD tient sur un téléphone en portrait, en paysage et sur une tablette.
  `ConstantPixelSize` : 1 unité = `pixelScale` pixels. `sortOrder` : les canvas les plus hauts sont
  dessinés par-dessus. `respectSafeArea` (activé) place les enfants dans la **zone sûre** de l'écran
  (hors encoche, coins arrondis et barre de gestes) ; à désactiver pour un fond plein écran.
- `UIRect` : **ancres** (`anchorMin`, `anchorMax`, de (0, 0) en haut à gauche à (1, 1) en bas à
  droite du parent), **pivot**, `position`, `size`. Ancres confondues : taille fixe placée au point
  d'ancrage ; ancres écartées : l'élément s'étire avec son parent (`size` s'ajoute à la zone).

## Éléments

| Composant | Rôle |
|---|---|
| `UIImage` | Rectangle de couleur ou texture ; bordures **9-slice** (coins non déformés), `preserveAspect`, `pixelArt` |
| `UIText` | Texte UTF-8 (accents compris), police TrueType, taille, couleur, alignement horizontal / vertical, retour à la ligne, interligne |
| `UIButton` | Couleurs normal / survol / appui / désactivé (teinte de l'image) ; clic = `onClick()` du script de l'entité et `message(bouton)` du script de `target` |
| `UISlider` | Curseur horizontal (piste, remplissage, poignée), valeurs min / max, entiers ; changement = `onValueChanged(valeur)` et `message(curseur, valeur)` sur `target` |
| `UILayout` | Range les enfants en **colonne**, **ligne** ou **grille** : espacement, marges, alignement, étirement |

Une image transparente (alpha 0) ne bloque pas le pointeur : utile pour les conteneurs.

## Pointeurs

- Souris et **plusieurs doigts à la fois** : survol, appui, clic (relâché sur le même bouton),
  annulation (relâché ailleurs), glisser des curseurs, plusieurs boutons maintenus simultanément
  (commandes tactiles : gauche + saut).
- Les événements souris que SDL fabrique à partir des touchers sont ignorés (un doigt n'agit pas deux
  fois).
- `agr.ui.pointerOverUI()` dit au gameplay d'ignorer un clic qui touche l'interface ;
  `agr.ui.isPressed(bouton)` lit un bouton maintenu.

## Texte

Polices TrueType rasterisées à la demande (stb_truetype) dans un atlas de glyphes de 1024 × 1024,
à la taille exacte affichée (texte net à toutes les tailles). Crénage, `\n`, retour à la ligne aux
espaces (et dans les mots trop longs). Les modèles de projet fournissent Inter
(`assets/fonts/Inter-Regular.ttf`, licence OFL).

## Depuis Lua

```lua
local HUD = {}
function HUD:onStart() self.score = agr.find("Score") end
function HUD:addPoint() self.points = (self.points or 0) + 1; self.score:setText("Score : " .. self.points) end
function HUD:pause(button) agr.find("Pause Menu"):setActive(true) end   -- message d'un UIButton
function HUD:volume(slider, v) agr.audio.setBusVolume("Music", v) end   -- message d'un UISlider
return HUD
```

## UI Builder (éditeur)

- *Créer › Interface (UI)* : Canvas, Panneau, Texte, Bouton (avec son texte), Curseur, Colonne. Un
  élément créé hors d'un canvas est placé sous le premier canvas (créé si besoin).
- La **vue Jeu** affiche l'interface à la taille de la vue : **cliquer** sélectionne l'élément le plus
  haut, **glisser** le déplace, les **poignées** le redimensionnent (en tenant compte du pivot) ;
  chaque geste est une entrée d'annulation. Ctrl (ou *Magnétisme*) : pas de 10 unités. Les ancres
  de l'élément sélectionné sont marquées dans le parent.
- Inspecteur de `UIRect` : grille de **préréglages d'ancres** (gauche / centre / droite / étirer ×
  haut / milieu / bas / étirer) et taille à l'écran.
- En mode Jeu, la vue Jeu reçoit la souris comme un écran : les boutons du HUD se cliquent.

## Performances

Mise en page + géométrie de 501 éléments (100 boutons avec texte) : ~0,1 ms par image sur la machine
de test ([BENCHMARKS.md](BENCHMARKS.md)).

## Limites actuelles

- Pas de masques / zones défilantes, de champs de saisie de texte, de cases à cocher, de listes
  déroulantes ni de texte enrichi.
- Pas de rotation ni d'échelle des éléments, pas de navigation clavier / manette.
- Atlas de glyphes unique : beaucoup de tailles et d'alphabets différents à la fois le remplissent
  (il est alors vidé et reconstruit, avec un avertissement si cela ne suffit pas).
- UI Builder : un élément à la fois (pas de sélection multiple déplacée ensemble), pas de guides
  d'alignement.
