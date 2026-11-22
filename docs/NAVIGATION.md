# Navigation (recherche de chemin, agents)

Module `agr::navigation`. Deux sortes de terrains, construits **automatiquement** à partir de la
géométrie statique de la scène :

| | 2D (plan XY) | 3D (sol XZ) |
|---|---|---|
| Données | grille de cases (A*, 8 directions, sans couper les coins, chemins lissés par ligne de vue) | maillage de navigation **Recast**, requêtes **Detour** (zlib) |
| Sources | tilemaps avec `TilemapCollider2D`, `Collider2D` statiques | `Collider` 3D statiques (boîtes, sphères, capsules) |
| Marge des agents | obstacles grossis de `agentRadius2D` | murs écartés de `agentRadius` ; hauteur, marche et pente maximales |

Est **statique** un collider qu'aucun corps dynamique ou cinématique ne porte (lui-même ou un parent),
hors déclencheurs et hors entités qui sont elles-mêmes des agents. Quand ces formes changent (vérifié
deux fois par seconde), tout est reconstruit et les agents en route recalculent leur chemin. Les
paramètres se règlent avec un composant `NavigationSettings` (premier actif de la scène).

Tests : `tests/test_navigation.cpp` (grille, maillage, agents 2D et 3D), test de script, étape du test
de bout en bout de l'éditeur.

## Agents

Le composant `NavAgent` (vitesse, distance d'arrêt, orientation, plan Auto / Grid2D / Mesh3D) déplace
l'entité le long du chemin vers la destination donnée par un script. *Auto* utilise le maillage 3D si la
scène en a un, la grille 2D sinon. Il déplace le **Transform** : à utiliser sur des entités sans corps
physique ou avec un corps cinématique (un corps dynamique serait aussi poussé par la simulation).

```lua
function Guard:onUpdate(dt)
    if self.entity:navigationStatus() ~= "moving" then
        local p = self.waypoints[self.next]
        self.entity:navigateTo(p.x, p.y, p.z)   -- false si la destination est inaccessible
        self.next = self.next % #self.waypoints + 1
    end
end
```

| Fonction | Rôle |
|---|---|
| `entity:navigateTo(x, y [, z])` | calcule le chemin et part ; `false` si la destination ne peut pas être atteinte (l'agent va alors au plus près) |
| `entity:stopNavigation()` | s'arrête |
| `entity:navigationStatus()` | `"idle"`, `"moving"`, `"arrived"`, `"unreachable"` |
| `entity:navigationPath()` | coins restants du chemin (`{x, y, z}`) |
| `agr.nav.path(x1, y1, z1, x2, y2, z2 [, "2d" \| "3d"])` | chemin entre deux points, ou `nil` |
| `agr.nav.rebuild()` | reconstruction immédiate (après avoir déplacé beaucoup d'obstacles) |

## Dans l'éditeur

Le bouton **Navigation** de la vue affiche le maillage 3D (cyan), les cases bloquées de la grille 2D
(rouge), les chemins des agents pendant le jeu (jaune) et un résumé (nombre de polygones, taille de la
grille, erreur de construction éventuelle : pente trop forte, aucune surface praticable…).

## Performances (machine de test, version release)

| Mesure | Temps |
|---|---|
| A* sur une grille 512 × 512 (15 % d'obstacles), d'un coin à l'autre, lissage compris | ~10 ms |
| Construction d'un maillage 100 × 100 m avec 200 obstacles (Recast) | ~51 ms (674 polygones) |
| Chemin à travers ce maillage (Detour) | ~0,02 ms |

## Foules, obstacles et liens

- **Évitement** (`NavAgent.avoidance`, actif par défaut, `radius`) : chaque agent choisit à chaque
  image une vitesse parmi 34 essais (la sienne, l'arrêt, 16 directions × 2 vitesses) en pénalisant
  l'écart avec la vitesse voulue, les changements brusques et le **temps avant collision** avec ses
  voisins ; entre agents en mouvement, chacun prend la moitié de l'écart (principe RVO), si bien que
  deux agents face à face se croisent sans se toucher ; un agent à l'arrêt est un obstacle fixe. Un
  agent qui ne progresse plus pendant 1 s (foule bloquée, cercle d'agents immobiles) suit son chemin
  en traversant les autres pendant 0,75 s, puis reprend l'évitement. Les vitesses qui mènent dans un mur (grille 2D) sont
  écartées, et en 3D l'agent est ramené sur le maillage. Sans voisin proche, le chemin est suivi
  exactement. Désactivé, les agents se traversent (`stats().avoiding` compte les agents qui ont dévié).
- **`NavObstacle`** (`radius`) : un rocher qui roule, une créature sans `NavAgent`, une porte fermée.
  Les agents le contournent à la volée ; sa vitesse est estimée d'une image à l'autre. Avec
  `carve`, il est aussi **découpé** de la navigation (le chemin passe autour, comme un collider
  statique ; reconstruit quand il bouge : pour ce qui bouge rarement).
- **`NavLink`** : raccourci entre deux points que le sol ne relie pas — un saut au-dessus d'un vide, une
  échelle, un téléporteur. Il part de l'entité et arrive à `end` (relatif) ; `bidirectional`, `cost`
  (grille 2D). En 3D ce sont des connexions hors maillage de Detour ; en 2D des arêtes de l'A*. Les
  agents le traversent en ligne droite.
- **`NavArea`** (`size`, `cost`, `walkable`) : une zone où marcher coûte plus (boue, eau peu profonde :
  `cost` 3 = évitée sauf si le détour est plus de 3 fois plus long) ou est **interdite** (`walkable`
  décoché : un trou dans la navigation). Boîte centrée sur l'entité (en 2D, son rectangle x / y).
  En 3D ce sont des aires Recast (coût par aire du filtre Detour, 62 zones au plus) ; en 2D un coût par
  cellule de l'A*, et le lissage du chemin ne coupe jamais à travers une zone d'un autre coût. Tests :
  un marais de coût 10 contourné, de la boue à 1,2 traversée, une zone interdite contournée (grille,
  maillage et scène).
- **Modèles importés** : un `MeshRenderer` statique avec un modèle (`NavigationSettings.meshRenderersAsSources`,
  actif par défaut) et les colliders **Mesh** / **Convex** fournissent leurs vrais triangles (même cache
  que la physique 3D) : un niveau importé est navigable sans collider ajouté.

## Tailles d'agents

`NavigationSettings.agentSizes` (`"0.8;1.5"`) : pour chaque rayon, une grille 2D et un maillage 3D de
plus sont construits, avec les obstacles élargis de ce rayon. Chaque `NavAgent` utilise la plus petite
taille qui contient son `radius` (les réglages par défaut `agentRadius2D` / `agentRadius` s'ils
suffisent, sinon la plus grande taille construite) : un char ou un géant contourne les passages où
seul un humain se faufile. Test : un mur percé d'une ouverture de 1,4 m, traversée par un agent de
rayon 0,3, contournée par un agent de rayon 0,9.

## Limites actuelles

- Évitement local (sans planification de foule globale) : dans un couloir très étroit, deux groupes
  face à face peuvent ralentir avant de se départager.
- Un seul maillage 3D (une tuile) : pour de très grands mondes, augmenter `cellSize` ; pas de
  construction par tuiles. `backgroundBuild` reconstruit sur un autre fil (l'ancien maillage sert
  entre-temps) ; la grille 2D et les tailles d'agents supplémentaires restent reconstruites tout de suite.
- Tailles d'agents : une grille et un maillage par taille listée dans `agentSizes` (mémoire et temps de
  construction multipliés d'autant).
