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
| `UISlider` | Curseur horizontal ou **vertical** (`vertical` : bas = minimum) — piste, remplissage, poignée, valeurs min / max, entiers ; changement = `onValueChanged(valeur)` et `message(curseur, valeur)` sur `target` |
| `UILayout` | Range les enfants en **colonne**, **ligne** ou **grille** : espacement, marges, alignement, étirement |
| `UIToggle` | Case à cocher ; même `group` = boutons radio (un seul coché) ; changement = `onValueChanged(1 ou 0)` et `message(case, valeur)` sur `target` |
| `UIInputField` | Champ de saisie (avec un `UIText` sur la même entité) : clic = focus (clavier virtuel sur téléphone), `onTextChanged(texte)`, Entrée = `onSubmit(texte)` et `message(champ, texte)` ; `placeholder`, `maxLength`, `numeric`, `password` ; curseur clignotant |
| `UIProgressBar` | Barre de progression, de vie ou de chargement : valeur, min / max, sens de remplissage (gauche → droite, droite → gauche, bas → haut, haut → bas), couleurs, marge intérieure |
| `UIDropdown` | Liste déroulante : options séparées par `;`, `selected` ; un clic ouvre la liste **par-dessus tout** (au-dessus si la place manque en bas), survol surligné, choix = `onValueChanged(index)` et `message(liste, index)` ; un clic ailleurs la ferme sans rien toucher d'autre ; au clavier / à la manette, Entrée passe à l'option suivante |
| `UITabs` | Onglets : noms séparés par `;`, `current` ; le *i*-ème enfant est la page du *i*-ème onglet, seule la page courante est affichée sous la barre d'onglets ; clic sur un onglet = `onValueChanged(index)` et `message(onglets, index)` |
| `UITooltip` | Infobulle : texte affiché dans une boîte près du pointeur après `delay` secondes de survol |
| `UIScrollView` | Zone défilante : ses enfants sont le contenu, déplacés par `scroll` et **découpés** au bord ; molette, glisser du doigt ou de la souris (un glisser annule le clic du bouton touché), barre de défilement ; limites calculées d'après le contenu |

**Navigation au clavier / à la manette** : un canevas avec `keyboardNavigation` permet de passer
d'un contrôle à l'autre avec les flèches, la croix directionnelle ou le stick (le plus proche dans la
direction), de l'activer avec Entrée, Espace ou A, et de régler un curseur avec gauche / droite ; le
contrôle choisi est encadré. Un pointeur reprend la main. `agr.ui.focus(entité)` choisit le contrôle
(ou donne le clavier à un champ), `agr.ui.isTyping()` dit au jeu d'ignorer les touches pendant la
saisie. Menu Créer › UI : case à cocher, champ de saisie, zone défilante.

Une image transparente (alpha 0) ne bloque pas le pointeur : utile pour les conteneurs. Une image
avec `blocksPointer` désactivé laisse aussi passer les clics et les doigts (voiles d'écran, décors
par-dessus des boutons : c'est le cas du voile de `agr.fx.flash` / `agr.fx.fade`).

## Thèmes

`UICanvas.theme` désigne un fichier `.agrtheme` : il donne l'apparence de **tous les contrôles** du
canevas — teintes des boutons (normal, survol, appui, désactivé), pistes / remplissages / poignées des
curseurs, cases à cocher, barres, listes déroulantes, onglets, infobulles —, la police de tous les
textes et la couleur des textes laissés blancs. Changer de thème change tout le menu d'un coup ; le
fichier est relu quand il change. Les projets reçoivent `assets/themes/sombre.agrtheme` et
`assets/themes/cartoon.agrtheme` (couleurs vives, boutons dorés, accent vert).

```json
{ "version": 1, "font": "", "text": [1, 1, 1, 1], "panel": [0.2, 0.15, 0.38, 0.97],
  "button": [1, 0.72, 0.15, 1], "buttonHover": [1, 0.82, 0.35, 1], "buttonPressed": [0.85, 0.52, 0.08, 1],
  "buttonDisabled": [0.55, 0.5, 0.6, 0.6], "accent": [0.35, 0.86, 0.3, 1], "track": [0.13, 0.1, 0.27, 1],
  "handle": [1, 1, 1, 1], "field": [0.29, 0.22, 0.52, 1] }
```

![Boutique d'exemple avec le thème « cartoon » : texte enrichi, onglets, boutons, barres, curseurs, case à cocher, liste déroulante ouverte](images/ui_widgets.png)

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

**Texte enrichi** (`UIText.richText`) : `[color=#ff8800]...[/color]` (ou un nom : `red`, `green`,
`blue`, `yellow`, `orange`, `purple`, `cyan`, `gray`, `white`, `black`, couleurs imbriquables),
`[b]...[/b]` (gras), `[u]...[/u]` (souligné, espaces compris) ; `[[` pour écrire un crochet ; une
balise inconnue reste affichée telle quelle.

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

- Pas encore d'arbres (`Tree`), de listes d'éléments (`ItemList`), de champs multilignes, de boîtes
  de dialogue ni de sélecteur de couleur ; texte enrichi sans italique, images ni liens ; coins
  arrondis uniquement par textures 9-slice.
- Pas de rotation ni d'échelle des éléments.
- Atlas de glyphes unique : beaucoup de tailles et d'alphabets différents à la fois le remplissent
  (il est alors vidé et reconstruit, avec un avertissement si cela ne suffit pas).
- UI Builder : un élément à la fois (pas de sélection multiple déplacée ensemble), pas de guides
  d'alignement.
