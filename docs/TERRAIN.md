# Terrain

Composant `Terrain` (module `agr::scene` pour les données, rendu, physique et navigation dans leurs
modules) : une grille de hauteurs carrée centrée sur son entité, peinte avec **4 couches** de textures.

| Champ | Rôle |
|---|---|
| `size`, `resolution` | côté en mètres, sommets par côté (2 à 1025) ; changer la résolution rééchantillonne le relief |
| `layer0` à `layer3`, `tiling` | textures des couches (herbe, roche, terre, sable…) et nombre de répétitions par couche |
| `color`, `roughness` | teinte et rugosité |
| `castShadows`, `collision` | ombres ; collision statique (physique 3D) et source du maillage de navigation |

Les hauteurs (en millimètres) et les poids des couches (4 octets par sommet) sont enregistrés dans la
scène en RLE : un terrain plat ne coûte presque rien, un relief 129 × 129 quelques dizaines de Ko.

## Rendu, physique, navigation

- **Rendu** : variante du shader PBR qui mélange les 4 couches selon une carte de poids (texture par
  terrain, mise à jour à chaque coup de pinceau), éclairage, ombres en cascades, ciel et brouillard comme
  les autres objets. Tests GPU sur les 3 API.
- **Physique** : maillage de collision Jolt statique, reconstruit quand le relief ou la position change.
- **Navigation** : le relief est une source du maillage de navigation 3D (pentes et marches limitées
  par les réglages de navigation).

## Dans l'éditeur

*Créer › Terrain* ajoute un terrain qui utilise les textures du projet (`assets/terrain/` : herbe,
roche, terre, sable, originales, générées par `tools/generate_template_terrain.py`). Dans l'inspecteur,
**Sculpter / peindre dans la vue** active les pinceaux, qui suivent le relief sous la souris :

| Pinceau | Effet (maintenir le clic gauche) |
|---|---|
| Élever / Abaisser | monte ou creuse, force en mètres par seconde au centre, atténuation douce vers le bord |
| Lisser | adoucit les reliefs |
| Aplanir | ramène vers la hauteur cible (Ctrl + clic : prendre la hauteur sous la souris) |
| Peindre | peint la couche choisie (les poids restent normalisés) |

Chaque trait est une étape d'annulation. *Tout aplanir* remet le terrain à zéro. Échap quitte l'outil.
Un clic sur le terrain le sélectionne (intersection exacte avec le relief).

## Limites actuelles

- Un seul maillage par terrain, sans niveaux de détail (LOD) ni découpage en morceaux pour le culling :
  au-delà de 257 × 257 sommets, préférer plusieurs terrains côte à côte.
- Pas de normal maps par couche, pas de trous, pas de végétation ni d'objets répartis automatiquement.
- Pas d'import / export de carte de hauteurs en image (prévu).
