# LOST KINGDOM — prompts Tripo pour les personnages

Ce guide sert à générer des modèles 3D avec Tripo et à me les envoyer. Je les importe ensuite dans le jeu avec `tools/import_character.py`, comme le héros.

## 1. Règles techniques (à respecter pour TOUS les modèles)

L'outil d'import pose le squelette du jeu tout seul. Pour cela, il a besoin d'un modèle **statique**, **en pose en T**.

| Règle | Pourquoi |
|---|---|
| **Pose en T** : bras tendus à l'horizontale, paumes vers le bas, jambes légèrement écartées, pieds à plat | L'outil trouve les épaules, coudes, poignets, hanches, genoux et chevilles à partir de cette pose |
| **Un seul personnage**, en entier (de la tête aux pieds), de face | Un décor, un socle ou un 2ᵉ personnage fausse la détection |
| **Mains vides, sans arme ni bouclier** | Le jeu ajoute l'arme équipée dans la main. Une arme sculptée dans le modèle ferait doublon |
| **Pas de socle**, pas de rocher sous les pieds, pas de base | Les pieds doivent toucher le sol du jeu |
| Cape, cheveux, tissus **près du corps** (pas d'éléments qui flottent loin) | Ils suivent le torse ; trop écartés, ils se déforment |
| Textures **PBR** activées ; **10 000 à 20 000 triangles** (25 000 max pour un boss) | Budget mobile (le héros fait environ 10 000 triangles) |
| Export en **GLB**, **sans squelette** (pas besoin de Rig) | L'outil fait lui-même le squelette |

**Modèles lourds ou « flottants » (1.8.1).** `tools/simplify_glb.py` réduit un GLB statique à environ 20 000 triangles et retire les objets qui flottent autour du personnage (planètes, étincelles). Avec `--mirror-leg L` ou `R`, il remplace une jambe pliée par le reflet de l'autre. Ensuite, `tools/import_character.py` fait le squelette.

**Depuis la 1.8.0, les modèles riggés passent aussi.** Un GLB avec le squelette automatique de Tripo (« Rig », comme le ninja envoyé sur le Drive) est importé par `tools/import_rigged.py`. L'outil reprend les poids de peau du modèle, le remet en pose de repos (bras le long du corps, jambes droites) et le réduit à environ 22 000 triangles. Les animations de Tripo, elles, ne sont pas reprises : le jeu anime le squelette en code. Avec un modèle riggé, la pose de départ (T ou A) n'a pas d'importance.

Si Tripo propose une option de pose (« T-pose »), active-la. Si ta version de Tripo a un champ **négatif**, mets-y le texte « À éviter » ci-dessous. Sinon, les règles sont déjà dans chaque prompt.

**À éviter (prompt négatif)** :
```
weapon, sword, shield, staff in hands, base, pedestal, ground, rock under feet, multiple characters, A-pose, arms down, sitting, crouching, dynamic pose, cropped, text, watermark
```

**Astuce qualité** : les meilleurs résultats viennent souvent de **l'image vers 3D**. Génère d'abord une image de concept en pose en T, vue de face, sur fond uni (avec le prompt ci-dessous dans un générateur d'images), puis donne cette image à Tripo.

## 2. Style commun

Tous les prompts finissent par ce **bloc de style**, pour que les modèles aillent ensemble et avec le héros :

```
full body, standing in a strict T-pose, arms straight out horizontally, palms down, legs slightly apart, feet flat on the ground, facing the camera, empty hands, no weapon, no base, single character, stylized fantasy RPG character, hand-painted PBR textures, rich but harmonious colors, clean readable silhouette, heroic proportions, high detail face, game-ready, 15k polygons
```

Les prompts sont en anglais : Tripo comprend mieux l'anglais.

## 3. Prompts par personnage

**Nom du fichier = identifiant du jeu.** Renomme chaque GLB exactement comme indiqué (par exemple `villager_f2.glb`), sinon je ne saurai pas où le mettre.

### 3.0 Héros de la roue (1.8.0)

| Fichier | État | Prompt (+ bloc de style) |
|---|---|---|
| `hero_ninja.glb` | **importé** (le « ninja warrior » du Drive) | — |
| `hero_flyer.glb` | **importé** (la « cosmic deity » du Drive). Le prompt ci-contre sert pour une variante | `an original celestial heroine who can fly, athletic build, fitted deep blue bodysuit with gold star-shaped trims, short flowing cape of golden light, gold bracers and greaves, long platinum blonde hair swept back by the wind, glowing pale blue eyes, a small radiant star emblem on the chest, confident heroic face,` |

Astra est un personnage **original** : pas de costume ni de coiffure copiés d'un héros existant (manga, comics…).

### 3.1 Villageois (priorité 1 : ils sont partout)

Jusqu'à 3 variantes par sexe. Chaque villageois du jeu tire l'une d'elles au hasard (le jeu prend en charge `_2` et `_3` depuis la 1.6.2).

| Fichier | Prompt (+ bloc de style) |
|---|---|
| `villager_m.glb` | `a medieval peasant man in his thirties, linen shirt with rolled sleeves, brown wool tunic, leather belt with a small pouch, simple trousers, worn leather boots, short brown hair, light stubble, friendly face,` |
| `villager_m2.glb` | `a stocky medieval farmer, straw hat, sleeveless padded vest over a cream shirt, rope belt, patched trousers tucked into boots, sunburnt cheeks, short beard,` |
| `villager_m3.glb` | `an elderly medieval craftsman, long grey beard, leather apron over a dark green tunic, rolled sleeves, sturdy boots, kind wrinkled face,` |
| `villager_f.glb` | `a young medieval village woman, long braided auburn hair, cream blouse, laced brown bodice, long blue skirt to the ankles, simple leather shoes, gentle face,` |
| `villager_f2.glb` | `a medieval market woman, headscarf, rolled sleeves, linen apron over a red wool dress, belt with keys, warm smile,` |
| `villager_f3.glb` | `an older medieval woman, grey hair in a bun, knitted shawl over a dark green dress, practical boots, wise kind face,` |

### 3.2 Personnages principaux de l'histoire (priorité 2)

| Fichier | Personnage | Prompt (+ bloc de style) |
|---|---|---|
| `npc_maelle.glb` | Maëlle, doyenne de Brindlemere (mentor) | `an elderly wise woman village elder, long silver hair in a loose braid, embroidered moss-green robe with golden trim, layered shawl, wooden bead necklace with a small crown-shaped pendant, serene mysterious face,` |
| `npc_alaric.glb` | Frère Alaric, archiviste (guide puis traître) | `a scholarly monk in his forties, deep blue and silver monastic robes with an embroidered crown sigil, hood resting on the shoulders, scroll case and leather books on his belt, neat dark hair, short beard, warm but sharp eyes,` |
| `npc_hilda.glb` | Hilda, forgeronne | `a strong female blacksmith, muscular arms, thick leather apron with burn marks, heavy gloves tucked in the belt, rolled sleeves, red hair tied back, soot on the cheeks, confident grin,` |
| `npc_bjorn.glb` | Jarl Bjorn, chef des clans (Frostheim) | `a broad nordic jarl, thick fur cloak over chainmail, iron circlet, long braided blond beard with metal rings, leather bracers, heavy boots, proud weathered face,` |
| `npc_aelith.glb` | Aelith, druidesse (Elderwood) | `an elven druid woman, pointed ears, long green-black hair with leaves and small flowers, flowing robes of moss and bark textures, antler-like wooden circlet, glowing green rune tattoos, calm face,` |
| `npc_tomas.glb` | Tomas, chasseur | `a lean medieval hunter, hooded green cloak, leather jerkin, quiver strap across the chest, fur-trimmed boots, short dark hair, scar on the cheek, alert eyes,` |
| `npc_aubert.glb` | Capitaine Aubert, garde | `a veteran town guard captain, polished steel breastplate over a blue gambeson, tabard with a green tree emblem, open-face steel helmet worn on the head, grey moustache, stern face,` |
| `npc_ysolde.glb` | Mère Ysolde, alchimiste | `an old alchemist woman, round spectacles, purple robes with many pockets, glass vials on a bandolier, frizzy white hair, mischievous smile,` |
| `npc_bram.glb` | Bram, aubergiste | `a jovial round innkeeper, white shirt, brown vest, stained apron, towel over the shoulder, bald head, big moustache, rosy cheeks,` |

### 3.3 Boss humanoïdes (priorité 3)

Pour les boss, ajoute `imposing, larger than life, epic fantasy boss` au bloc de style. Ils seront agrandis par le jeu.

| Fichier | Boss | Prompt |
|---|---|---|
| `b_fallen_knight.glb` | Sire Corwin, le Chevalier Déchu | `a fallen royal champion knight, ornate blackened silver full plate armor with cracked gold filigree, tattered dark crimson cape close to the body, crown-shaped crest on the helmet, faint purple glow from the visor and armor cracks,` |
| `b_skeleton_king.glb` | Le Roi Squelette | `an undead skeleton king, tall bony frame, rusted royal armor pieces, tattered purple royal mantle, jagged iron crown, glowing cyan eyes in the skull, ribcage visible,` |
| `b_root_guardian.glb` | Le Gardien des Racines | `a giant forest guardian spirit made of twisted roots and bark, humanoid shape, moss and mushrooms on the shoulders, glowing green core in the chest, antler-like branches on the head, corrupted dark purple veins,` |
| `b_ice_colossus.glb` | Le Colosse de Glace | `a massive ice giant, body of translucent blue ice and snow-covered rock, frost runes glowing on the chest, icicle crown, heavy fists, cold mist,` |
| `b_stone_titan.glb` | Le Titan de Pierre | `a colossal stone titan, body of carved mountain granite blocks, glowing orange magma cracks, ancient runes engraved on the arms, massive shoulders, small glowing eyes,` |
| `b_abyss_lord.glb` | Malgrath, Seigneur des Abysses | `a dark sorcerer lord of the abyss, tall gaunt figure in layered black and violet robes, spiked shoulder armor, long horns, void-like face with burning violet eyes, floating ragged sleeves kept close to the arms, small bat-like wings folded on the back,` |

Le Dragon Cendré (`b_ash_dragon`) et la Reine des Araignées (`b_spider_queen`) ne sont **pas** humanoïdes : l'outil d'import ne les prend pas encore en charge. Ne les génère pas pour l'instant.

### 3.4 Ennemis humanoïdes (priorité 4)

| Fichier | Ennemi | Prompt |
|---|---|---|
| `bandit.glb` | Bandit coupe-jarret | `a rugged road bandit, dark hooded cloak, patched leather armor, red scarf over the lower face, belts and pouches, mean eyes,` |
| `mountain_bandit.glb` | Brigand des cimes | `a mountain brigand, fur-lined leather armor, wolf pelt on the shoulders, braided hair, war paint, scarred face,` |
| `skeleton.glb` | Squelette guerrier | `an undead skeleton warrior, rusty chainmail shirt, broken helmet, torn cloth, glowing cyan eyes,` |
| `draugr.glb` | Draugr gelé | `a frozen nordic undead warrior, frost-covered ancient armor, horned helmet, icy beard, pale blue skin, glowing blue eyes,` |
| `dark_knight.glb` | Chevalier corrompu | `a corrupted knight, black spiked plate armor with purple glowing runes, torn dark cape close to the body, menacing helmet,` |
| `ash_knight.glb` | Chevalier calciné | `a burnt knight of ash, charred black armor with glowing ember cracks, smoking shoulders, ragged burnt cloak,` |
| `goblin.glb` | Gobelin pillard | `a small mischievous goblin raider, green skin, big pointed ears, crooked teeth, ragged leather scraps, bone necklace,` |
| `ghoul.glb` | Goule | `a gaunt ghoul, grey rotting skin, long clawed fingers, torn burial rags, hunched but standing in T-pose, glowing yellow eyes,` |

### 3.5 Monstres déjà sur le Drive : à refaire avec « Rig » (priorité 1)

Ces trois modèles du Drive sont beaux et vont dans le jeu, mais ils sont **statiques** et **pas en pose en T** : mon outil poserait mal leur squelette. Passe-les dans Tripo avec l'option **Rig** (squelette automatique, comme le ninja), exporte en GLB et renomme :

| Modèle du Drive | Fichier à m'envoyer | Devient dans le jeu |
|---|---|---|
| rock monster | `stone_golem.glb` | Golem de pierre (Hautes-Cimes) |
| galaxy titan | `b_stone_titan.glb` | Le Titan de Pierre (boss) |
| lava creature | `ember_elemental.glb` | Élémentaire de braise (Cendres) |

Le dragon serpent du Drive ne colle pas au squelette des dragons du jeu (dragon ailé à quatre pattes) : je ne l'utilise pas.

## 3a. État (1.11.0)

- **Importés** : les 12 bâtiments de la section 3b (générés avec Tripo, modèle v3.1, textures « detailed », environ 50 crédits chacun) ; le golem, le titan et l'élémentaire du Drive (squelette Tripo, 25 crédits chacun) ; un gobelin, un bandit et un squelette (génération + squelette, 75 crédits chacun) ; la voiture et la moto du Drive (`tools/import_vehicle.py`).
- **Orientation** : les modèles Tripo v3.1 regardent vers +X. Les outils ont une option `--rotate 270` qui les tourne vers +Z (le sens du jeu).
- **Fichiers compressés** : certains GLB récents utilisent la compression meshopt. Il faut les décoder d'abord avec `node tools/decode_glb.mjs in.glb out.glb` (voir l'en-tête du script pour l'installation).

## 3b. Décors : bâtiments et objets (1.9.0)

Le jeu sait maintenant remplacer ses bâtiments construits en code par des **modèles 3D**. Le modèle est mis automatiquement à la taille de l'emprise du bâtiment, posé sur le sol, avec ses collisions.

**Règles pour les décors**
- **Un seul bâtiment ou objet**, entier, **porte d'entrée face à la caméra** (vue de face).
- **Pas de sol, de terrain, de pelouse ni de socle** autour : le bâtiment doit être posé sur sa base. Pas de personnages, ni d'arbres collés au bâtiment.
- **Pas besoin de Rig** (squelette) : un bâtiment ne bouge pas.
- Textures PBR ; **5 000 à 15 000 triangles** (je réduis moi-même si c'est plus).
- Même style que les personnages : finis le prompt par ce bloc :

```
single building, front view, entrance facing the camera, no ground, no terrain, no base, no people, stylized fantasy RPG, hand-painted PBR textures, rich but harmonious colors, clean readable silhouette, game-ready, 10k polygons
```

**Les modèles que je veux (par ordre de priorité)**

| Fichier | Où on le voit | Prompt (+ bloc de style) |
|---|---|---|
| `house.glb` | partout (maisons des villages) | `a cozy medieval timber-framed cottage, white plaster walls with dark oak beams, steep thatched roof, small wooden door, two shuttered windows with flower boxes, stone chimney,` |
| `house_2.glb` | variante de maison | `a medieval village house, stone ground floor, timber-framed upper floor, red clay tile roof, wooden balcony, lantern by the door,` |
| `stonehouse.glb` | Hautes-Cimes, Frostheim | `a sturdy medieval stone house, grey fieldstone walls, slate roof covered with a little snow, heavy wooden door with iron bands, small deep windows,` |
| `inn.glb` | auberges | `a large two-storey medieval inn, timber frame and plaster, wide wooden door, hanging sign with a painted mug, warm lit windows, two chimneys,` |
| `longhouse.glb` | villages du nord | `a long nordic wooden longhouse, dark carved timber walls, steep shingled roof with crossed carved gables, round shields on the walls,` |
| `forge.glb` | forgerons | `an open medieval blacksmith forge, stone furnace with chimney, wooden roof on posts, anvil, tools hanging on the wall, water barrel,` |
| `chapel.glb` | chapelles | `a small medieval stone chapel, arched wooden door, round rose window, slate roof, little bell tower,` |
| `tower.glb` | tours de guet | `a round medieval watchtower in stone, wooden crenellated top with a small roof, narrow arrow slits, wooden door at the base,` |
| `well.glb` | places des villages | `a medieval stone well with a small wooden roof, rope and bucket,` |
| `stall.glb` | marchés | `a medieval market stall, wooden counter, striped cloth canopy, crates of fruit and vegetables,` |
| `tent.glb` | camps de bandits | `a medieval canvas camping tent, patched beige fabric, wooden poles, ropes,` |

Les fichiers vont dans le même dossier Drive que d'habitude.

## 4. Comment m'envoyer les modèles

1. Exporte chaque modèle en **GLB**, renommé avec l'identifiant (par exemple `npc_maelle.glb`).
2. Mets-les dans un dossier Google Drive. Partage le dossier en **« Tous les utilisateurs disposant du lien »** (lecteur), sinon je ne peux pas les télécharger.
3. Envoie-moi le lien. Je les importe, je vérifie l'aperçu du squelette et des poids (pas de « plaques » sous les pieds), je règle la taille et je livre un APK.

Si un modèle ne passe pas (pose pas assez en T, arme dans la main, socle), je te le dis avec la raison, et tu peux le régénérer.

## 5. Animations

- **Ce que le jeu fait aujourd'hui.** Les animations (marche, course, combos, roulade…) sont écrites en code et s'appliquent à tous les modèles importés. La 1.6.2 a refait la démarche et ajouté le corps entier aux attaques (voir le README).
- **Modèles riggés (1.8.0).** Leur squelette et leurs poids sont repris, pas leurs animations (voir la section 1).
- **Pour aller plus loin.** Tripo sait aussi poser un squelette sur un modèle et lui appliquer des animations toutes faites. Le jeu **ne sait pas encore importer** ces animations. C'est faisable : il faut un outil qui transfère les mouvements de leur squelette vers celui du jeu (« retargeting »).

  Si tu veux qu'on aille dans ce sens, envoie en plus, pour le héros, la version riggée de Tripo avec quelques animations (idle, walk, run, attack, roll) en GLB. Je te dirai d'abord honnêtement si elles sont exploitables.
