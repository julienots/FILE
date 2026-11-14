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

- **Valeurs** : les nombres sont limités à des valeurs raisonnables (par exemple `crit` entre 0 et 1, `scale` entre 0,3 et 6).
- **Erreurs** : une ligne invalide est ignorée et signalée dans le menu Mods. Le reste du mod s'applique quand même.
- **Identifiants** : objets, ennemis et PNJ sont listés dans `game/Data.cpp`. Les ennemis et les PNJ sont aussi repris dans `docs/TRIPO_PROMPTS.md`.

### assets/

Tout fichier placé sous `assets/` **remplace** le fichier du jeu de même chemin :
- **modèles 3D** : `models/<id>.lkm` et `<id>.jpg`, produits par `tools/import_character.py` à partir d'un GLB (Tripo, Meshy…). Les identifiants sont `hero`, `villager_m`, `villager_m2`, `villager_m3`, `villager_f`…, `npc_<id>` et `<id d'ennemi>` ;
- **musiques et sons** : `audio/music/*.ogg`, `audio/sfx/*.ogg`, `audio/amb/*.ogg` (OGG Vorbis) ;
- **interface et polices** : `ui/…`, `fonts/…`.

Si plusieurs mods remplacent le même fichier, c'est le premier mod actif (par ordre alphabétique de dossier) qui gagne.

## 3. Ce que les mods ne font pas (encore)

- Pas de nouveaux objets, ennemis, quêtes ou régions : seulement modifier l'existant et ajouter des chroniques.
- Pas de scripts ni de code, volontairement (sécurité).
- Pas d'import direct de GLB sur le téléphone : il faut passer par `tools/import_character.py` sur un ordinateur, ou me demander de le faire.

## 4. Sur ordinateur (harnais de test)

Copie le dossier du mod dans `<dossier de sauvegarde>/mods/<nom>/` (par défaut `build/desktop_save/mods/`).

Le scénario `--autoplay=mods` installe un mod de test, puis vérifie :
- les modifications de données ;
- le remplacement d'un modèle ;
- le menu Mods ;
- la désactivation par un vrai appui.
