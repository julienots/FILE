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

## Limites actuelles

- Les modèles importés (`MeshRenderer` d'un `.glb`) ne sont pas des sources : la navigation suit les
  **colliders**. Ajouter des colliders statiques (ou un sol) là où l'on veut marcher.
- Pas d'évitement entre agents (foule) ni d'obstacles dynamiques creusés dans le maillage : les
  agents se traversent. DetourCrowd est une piste pour plus tard.
- Un seul maillage 3D (une tuile) : pour de très grands mondes, augmenter `cellSize` ; pas de
  construction par tuiles ni en arrière-plan (la reconstruction bloque l'image le temps du calcul).
- Une seule taille d'agent par scène (celle des réglages).
