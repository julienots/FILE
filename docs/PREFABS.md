# Prefabs

Un **prefab** est une hiérarchie d'entités réutilisable, enregistrée dans un fichier `.agrprefab`
(JSON lisible, comme les scènes) : un ennemi avec son arme et son script, une pièce à ramasser, une
voiture et ses roues… On le place autant de fois que nécessaire, dans l'éditeur ou depuis un script.

Module `agr::scene` (`Prefab.hpp`). Tests : `tests/test_prefab.cpp` et le test de script
`agr.instantiate`.

## Instances et modifications

Une **instance** reste liée à son prefab. Dans la scène, elle est enregistrée sous la forme d'une
**référence au prefab et de ses différences**, pas d'une copie :

| Différence enregistrée | Exemple |
|---|---|
| Valeur de champ modifiée | dégâts de l'arme de ce boss = 9 |
| Composant ajouté / retiré | ce garde n'a pas d'armure |
| Entité ajoutée (avec ses enfants) / retirée | un bouclier en plus, sans casque |

Au chargement, le prefab est déplié puis les différences sont réappliquées. Conséquence : **modifier
le prefab met à jour toutes les instances**, même dans des scènes fermées, sauf sur ce que chaque
instance a changé elle-même. La position (Transform de la racine) et le nom de la racine sont toujours
propres à l'instance.

Les entités d'une instance ont des **identifiants dérivés** de celui de la racine de l'instance et de
leur identifiant dans le prefab. Ils sont donc stables d'un chargement à l'autre et uniques par instance :
une référence (champ entité d'un composant, cible d'une caméra…) vers un enfant d'instance reste valide
après sauvegarde, rechargement et modification du prefab.

- **Prefabs imbriqués** : un prefab peut contenir des instances d'autres prefabs (une voiture et ses
  roues). Modifier la roue met à jour les roues de toutes les voitures. Un prefab qui se contient
  lui-même est détecté ; l'instance fautive est remplacée par un emplacement vide, avec un message.
- **Copier / coller, dupliquer** : la copie est une nouvelle instance avec les mêmes modifications et
  ses propres identifiants (ses références internes pointent vers ses propres enfants).
- **Prefab introuvable** (fichier supprimé ou renommé) : un emplacement « Prefab introuvable » garde
  la position, le nom et toutes les modifications enregistrées. La scène peut être réenregistrée sans
  perte, et tout revient quand le fichier est restauré.

## Dans l'éditeur

| Action | Où | Effet |
|---|---|---|
| **Créer un prefab** | clic droit sur une entité dans la hiérarchie | écrit `assets://prefabs/<nom>.agrprefab` ; l'entité devient la première instance |
| **Placer** | glisser un `.agrprefab` de l'explorateur dans la vue ou la hiérarchie | nouvelle instance (une étape d'annulation) |
| **Appliquer au prefab** | inspecteur d'une instance, ou clic droit | les modifications de cette instance deviennent celles du prefab ; les autres instances sont mises à jour en gardant les leurs |
| **Rétablir** | idem | supprime les modifications de l'instance (sauf position et nom) |
| **Dissocier** | idem | l'instance devient des entités ordinaires |

Dans la hiérarchie, les racines d'instances sont en bleu et leurs entités en bleu clair. L'inspecteur
indique le prefab et le nombre de modifications. Un prefab modifié sur le disque (autre outil,
gestion de versions) est rechargé à chaud et les instances de la scène ouverte sont mises à jour.

## Dans les scripts

```lua
function Spawner:onUpdate(dt)
    self.timer = self.timer - dt
    if self.timer <= 0 then
        self.timer = 2
        local enemy, err = agr.instantiate("assets://prefabs/enemy.agrprefab")
        if enemy then enemy:set("Transform", "position", {math.random(-5, 5), 0, 0})
        else agr.warn(err) end
    end
end
```

Les scripts des entités de l'instance démarrent à l'image suivante, comme pour toute entité créée en
jeu.

## Éditer un prefab seul

Double-clic sur un `.agrprefab` dans les assets : la vue ne montre plus que ses entités (bandeau violet en
bas). **Ctrl+S** enregistre le prefab (ses identifiants d'entités sont gardés : chaque instance garde ses
propres modifications) ; **Revenir à la scène** retrouve la scène telle qu'elle était, modifications non
enregistrées comprises, avec ses instances à jour. Testé : deux instances reçoivent l'édition, la
modification propre à l'une d'elles et le travail non enregistré de la scène sont conservés.

## Variantes

Clic droit sur une instance › **Créer une variante** (comme les scènes héritées de Godot) : l'instance et
ses modifications deviennent un nouveau prefab `prefabs/<base>_variante.agrprefab`, enregistré comme « la
base + ces changements ». Les instances de la variante suivent les modifications ultérieures de la base
(un nouvel enfant, une autre couleur) et gardent ce que la variante change (un boss = un ennemi avec plus
d'armure). Une variante peut servir de base à une autre. Par code : `prefab::variantFrom`, `prefab::baseOf`.

## Limites actuelles

- Édition isolée : la scène à laquelle on revient est gardée en mémoire (pas sur le disque) ; charger une
  autre scène pendant l'édition d'un prefab abandonne ce retour.
- Variantes : une variante ne peut pas retirer une entité de sa base (elle peut en ajouter, changer et
  retirer des composants) ; *Appliquer* sur une instance de variante écrit dans la variante, pas dans la base.
- *Appliquer* et *Rétablir* recréent les entités de l'instance (les références depuis l'extérieur de
  l'instance sont restaurées, les poignées d'entités tenues par un script en cours ne le sont pas).
- Modifier un prefab n'entre pas dans l'historique d'annulation de la scène (comme les matériaux).
