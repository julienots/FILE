# LOST KINGDOM — créer un mod

Depuis la 1.7.0, LOST KINGDOM accepte des **mods** : des paquets de **données et de ressources**, sans aucun code. Un mod ne peut donc ni abîmer le téléphone ni toucher aux sauvegardes.

## 1. Installer un mod (téléphone)

1. Télécharge le fichier `.zip` du mod sur le téléphone.
2. Dans le jeu : **Journal → Paramètres → Mods → Importer un mod (.zip)**, puis choisis le fichier.
3. **Redémarre le jeu** : les mods s'appliquent au lancement.
4. Le menu **Mods** affiche chaque mod avec un interrupteur, le nombre de modifications appliquées et les éventuelles erreurs (avec le numéro de la ligne).

Exemple prêt à essayer : `release/mods/exemple-chroniques-du-vagabond.zip`.

## 2. Structure d'un mod

```
mon_mod.zip
├── mod.txt          (obligatoire) nom, auteur, version, description
├── data.txt         (facultatif) modifications du contenu, une par ligne
└── assets/          (facultatif) fichiers qui remplacent ceux du jeu, même chemin
    ├── models/villager_f.lkm + villager_f.jpg
    ├── audio/music/title.ogg
    └── audio/sfx/swing.ogg
```

`mod.txt` et `data.txt` sont **à la racine du zip**, pas dans un sous-dossier. Le nom du dossier du mod sur le téléphone vient du nom du fichier zip.

### mod.txt
```
name = Mon super mod
author = Moi
version = 1.0
description = Ce que fait le mod.
```

### data.txt

Le format est une modification par ligne : `type.identifiant.champ = valeur`. Les lignes qui commencent par `#` sont des commentaires.

| Type | Champs | Exemple |
|---|---|---|
| `item` (objet) | `name`, `desc`, `atk`, `def`, `hp`, `mp`, `crit` (0 à 1), `speed`, `value` | `item.sword_rusty.atk = 12` |
| `enemy` (ennemi) | `name`, `desc`, `hp`, `atk`, `def`, `speed`, `scale`, `aggro` | `enemy.wolf.hp = 80` |
| `npc` (PNJ) | `name`, `title`, `greet` (ajoute une salutation), `greet.replace`, `lore` (ajoute une confidence) | `npc.maelle.greet = Bonjour !` |
| `lore.add` (chronique) | `identifiant \| Titre \| Texte` | `lore.add = ma_page \| Mon titre \| Mon texte` |
| `dig` (pelle, 2.4.0) | `dig.radius` (0,6 à 4 m), `dig.depth` (0,1 à 3 m par coup) | `dig.depth = 1.2` |
| `game` (toute la partie, 2.6.0) | `game.enemy_hp` et `game.enemy_atk` (0,2 à 10), `game.xp` et `game.gold` (0,1 à 10), `game.player_speed` (0,5 à 2,5), `game.stamina_regen` (0,2 à 5) — des multiplicateurs | `game.gold = 3` |
| `game` (2.7.0) | `game.gravity` (0,15 à 3), `game.jump` (0,5 à 3), `game.time_speed` (0 à 20 ; 0 fige le temps), `game.hour` (0 à 23,99 : l'horloge reste à cette heure), `game.mount_speed` (0,5 à 3), `game.enemy_scale` (0,4 à 3), `game.weather` = `clair`, `nuageux`, `pluie`, `orage`, `neige`, `brouillard`, `cendres`, `sable` ou `normal` | `game.weather = neige` |
| `bomb` (bombes, 2.4.0) | `bomb.radius` (1 à 12 m), `bomb.depth` (0,2 à 10 m), `bomb.damage` (0 à 2000), `bomb.fuse` (0,3 à 10 s) | `bomb.radius = 8` |

### Mods 2.0 (LOST KINGDOM 3.2.0) : nouvelles créatures, nouveaux objets

| Ligne | Effet |
|---|---|
| `enemy.add = id \| base \| Nom \| Description` | Nouvelle créature, copie de la créature `base` (corps, attaques, comportement). Ensuite, `enemy.id.*` la règle. |
| `enemy.id.region` | `greenhaven`, `frostheim`, `ashen`, `elderwood`, `ombres`, `highcrowns`, `ambre`, `corail`, `steppes`, `jungle`, `aurore`, `titans` |
| `enemy.id.tint` | Couleur `#rrggbb` |
| `enemy.id.spawn` | Fréquence dans la région : 0 à 20, et 1 = comme les créatures du jeu |
| `enemy.id.level`, `xp`, `gold`, `group`, `night` | Niveau, récompenses, taille des groupes, `1` = surtout la nuit |
| `enemy.id.loot = objet \| chance` | Ajoute un objet au butin (chance de 0 à 1). Exemple : `star_shard` pour des éclats d'étoile. |
| `item.add = id \| base \| Nom \| Description` | Nouvel objet, copie de l'objet `base` (type, apparence). Ensuite, `item.id.*` le règle. |
| `item.id.rarity` | `commun`, `rare`, `epique`, `legendaire`, `mythique`, `ancestral` |
| `item.id.color`, `elem_dmg`, `stamina`, `heal`, `mana` | Couleur `#rrggbb`, dégâts élémentaires, bonus |
| `game.start_item = objet \| nombre` | Objet donné au début d'une nouvelle partie |
| `game.start_weapon`, `game.start_gold` | Arme équipée et or au départ |
| `game.all_heroes = 1` | (3.9.6) Tous les héros jouables sans leur quête. La sauvegarde n'est pas modifiée : en désactivant le mod, on retrouve les héros réellement débloqués. |

- **Identifiants** : minuscules, chiffres et `_`. Un identifiant du jeu ne peut pas être remplacé par `add`.
- **Ordre** : un objet doit être créé (`item.add`) avant d'être utilisé dans un `loot` ou un `start_item`.
- **Modèle 3D** : une créature ajoutée peut avoir son propre modèle, `assets/models/<id>.lkm` et `.jpg`, à condition que sa base soit humanoïde ou quadrupède.
- **Sans ordinateur** : l'application **LK Studio** (`release/LK-Studio.apk`) écrit tout cela et importe les modèles .glb avec un squelette automatique.

- **Valeurs** : les nombres sont limités à des valeurs raisonnables (par exemple `crit` entre 0 et 1, `scale` entre 0,3 et 6).
- **Erreurs** : une ligne invalide est ignorée et signalée dans le menu Mods. Le reste du mod s'applique quand même.
- **Identifiants** : objets, ennemis et PNJ sont listés dans `game/Data.cpp`. Les ennemis et les PNJ sont aussi repris dans `docs/TRIPO_PROMPTS.md`.

### assets/

Tout fichier placé sous `assets/` **remplace** le fichier du jeu de même chemin :
- **modèles 3D** : `models/<id>.lkm` et `<id>.jpg`, produits par `tools/import_character.py` à partir d'un GLB (Tripo, Meshy…). Les identifiants sont `hero`, `villager_m`, `villager_m2`, `villager_m3`, `villager_f`…, `npc_<id>` et `<id d'ennemi>` ;
- **bâtiments (1.9.0)** : `models/props/<nom>.lkm` et `.jpg`, produits par `tools/import_prop.py`. Les noms sont `house`, `inn`, `stonehouse`, `longhouse`, `forge`, `chapel`, `tower`, `well`, `stall`, `tent`, avec des variantes `_2` et `_3` ;
- **musiques et sons** : `audio/music/*.ogg`, `audio/sfx/*.ogg`, `audio/amb/*.ogg` (OGG Vorbis) ;
- **interface et polices** : `ui/…`, `fonts/…`.

Si plusieurs mods remplacent le même fichier, c'est le premier mod actif (par ordre alphabétique de dossier) qui gagne.

### Mods livrés avec le jeu (2.6.0)

Quatorze mods sont inclus dans l'APK (six depuis la 2.6.0, huit de plus en 2.7.0 : Lune basse, Monde des géants, Mini-monstres, Hiver éternel, Nuit éternelle, Destrier céleste, Apocalypse, Terrassier), dans `assets/mods/<id>/` (avec la liste `assets/mods/index.txt`). Au premier lancement, ils sont copiés dans le dossier des mods, **désactivés**, puis s'activent dans le menu Mods. Ce sont de bons exemples à copier pour écrire les vôtres.

## 3. Ce que les mods ne font pas (encore)

- Pas de nouvelles quêtes, régions ou attaques. Depuis la 3.2.0, on peut ajouter des créatures et des objets, mais ce sont des copies réglables de créatures et d'objets du jeu.
- Pas de scripts ni de code, volontairement (sécurité).
- Import de GLB sur le téléphone : avec l'application LK Studio (3.2.0). Sur ordinateur : `tools/import_character.py`.

## 4. Sur ordinateur (harnais de test)

Copie le dossier du mod dans `<dossier de sauvegarde>/mods/<nom>/` (par défaut `build/desktop_save/mods/`).

Le scénario `--autoplay=mods` installe un mod de test, puis vérifie :
- les modifications de données ;
- le remplacement d'un modèle ;
- le menu Mods ;
- la désactivation par un vrai appui.
