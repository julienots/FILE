# ADR 0003 — Conception de l'ECS

**Statut :** accepté

## Décision

ECS à **sparse sets** écrit pour le moteur (`agr/ecs/Registry.hpp`).

## Options comparées

| | Sparse sets (choisi) | Archétypes (Unity DOTS, flecs) |
|---|---|---|
| Ajout / retrait de composant | O(1), sans déplacement des autres composants | déplace l'entité d'un archétype à l'autre |
| Itération sur 1 composant | contiguë, optimale | contiguë |
| Itération sur N composants | pilotée par le plus petit pool + tests | contiguë |
| Pointeurs stables entre deux ajouts | dans le même pool, jusqu'au prochain ajout | non |
| Complexité | ~400 lignes | plusieurs milliers |

Un éditeur ajoute et retire constamment des composants ; les jeux mobiles visés ont rarement
plus de quelques dizaines de milliers d'entités. Les sparse sets sont le meilleur compromis.
Mesures : [BENCHMARKS.md](../BENCHMARKS.md).

## Identifiants de type

`componentTypeId<T>()` = FNV-1a 64 bits du nom du type obtenu à la compilation. Contrairement à
un compteur statique, il est identique dans l'exécutable et dans chaque plugin.

## Limites connues

- `Registry::get<T>()` fait une recherche dans une table de hachage (~30 ns en accès aléatoire
  sur 1 M d'entités, dominé par les défauts de cache). Les systèmes doivent passer par des vues.
  Un cache d'indice par type est prévu si le profilage le justifie.
- Pas encore de groupes triés ni de filtres d'exclusion ; ils seront ajoutés au besoin du renderer.
