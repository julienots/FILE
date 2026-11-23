# Tutoriel : un jeu de plateforme 2D en 30 minutes

Ce tutoriel construit pas à pas un petit jeu de plateforme : un personnage qui court et saute, un sol,
des pièces à ramasser et un compteur à l'écran. Il ne suppose aucune connaissance du moteur ; il faut
seulement savoir ce qu'est une variable. Les deux scripts de ce tutoriel sont **exécutés par les tests
du moteur** (`tests/test_game_templates.cpp`, test « tutorial 2D ») dans une scène construite comme
ci-dessous : le personnage tombe sur le sol, court vers la droite et ramasse la pièce.

## 1. Créer le projet

1. Lancer l'éditeur : le **hub de projets** s'ouvre.
2. Type de jeu **2D**, modèle **Scène 2D vide**, un nom (`Mon platformer`) et un dossier, puis *Créer*.
3. L'éditeur s'ouvre sur une scène qui contient déjà une caméra 2D.

La fenêtre se compose de la **Hiérarchie** (les objets de la scène, à gauche), de la **vue Scène** (au
centre), de l'**Inspecteur** (les réglages de l'objet sélectionné, à droite) et des **Assets** (les
fichiers du projet, en bas). Tout est décrit dans [EDITOR.md](EDITOR.md).

## 2. Le sol

1. Clic droit dans la Hiérarchie › *Créer › 2D › Sprite*. Renommer l'objet `Sol` (F2).
2. Dans l'Inspecteur, régler son *Transform* : position `(0, -1)`, échelle `(12, 1)`.
3. *Ajouter un composant › Collider2D* (forme `Box`). Sans `RigidBody2D`, un collider est **statique** :
   le sol ne tombe pas et bloque ce qui le touche.

Pour une vraie image, glisser un fichier `.png` des Assets sur le champ `texture` du `SpriteRenderer`
(sans image, le sprite est un rectangle blanc, ce qui suffit pour ce tutoriel).

## 3. Le personnage

1. *Créer › 2D › Sprite*, le renommer `Joueur`, position `(0, 2)`.
2. *Ajouter un composant › CharacterController2D*. C'est un personnage déplacé par sa vitesse (comme le
   `CharacterBody2D` de Godot) : il glisse le long des murs, se pose sur les sols et monte les pentes.
   Régler `size` à `(0.8, 1)` pour la taille de sa capsule.
3. Appuyer sur **Jouer** (Ctrl+P) : le personnage tombe et se pose sur le sol. **Arrêter** remet la
   scène dans son état de départ.

## 4. Le faire bouger : premier script

Dans les Assets : *Nouveau script*, le nommer `joueur.lua`, double-cliquer pour l'ouvrir dans
l'éditeur de scripts, et remplacer son contenu par :

```lua
-- assets/scripts/joueur.lua
local Joueur = {
    -- Réglables dans l'Inspecteur, objet par objet.
    properties = { vitesse = 6, saut = 9 },
}

function Joueur:onUpdate(dt)
    local corps = self.entity
    local v = corps:get("CharacterController2D", "velocity")    -- {x =, y =}
    v.x = agr.input.value("move_x") * self.vitesse              -- flèches, A/D (Q/D), stick, tactile
    if corps:get("CharacterController2D", "onFloor") and agr.input.pressed("jump") then
        v.y = self.saut                                         -- Espace, bouton A, tactile
    end
    corps:set("CharacterController2D", "velocity", v)           -- la gravité s'ajoute toute seule
end

return Joueur
```

Enregistrer (Ctrl+S), puis **glisser `joueur.lua` sur le `Joueur`** dans la Hiérarchie (cela ajoute un
composant `Script`). Jouer : les flèches font courir le personnage et Espace le fait sauter. Les
propriétés `vitesse` et `saut` apparaissent dans l'Inspecteur pendant le jeu : les changer modifie le
personnage en direct.

L'éditeur de scripts complète les noms (`agr.input.` puis Tab) et souligne les erreurs pendant la
frappe : une faute comme `self.vitese` ou `corps:sett(...)` est signalée en ambre avant même de jouer.
`move_x` et `jump` sont des **actions** : elles sont liées par défaut au clavier, aux manettes et à
l'écran tactile ([INPUT.md](INPUT.md)).

## 5. Le compteur à l'écran

1. *Créer › Interface (UI) › Canvas*, puis, en enfant du Canvas, *Créer › Interface (UI) › Texte*.
2. Renommer le texte `Score`, écrire `Pièces : 0` dans son champ `text`, et le placer en haut à gauche
   avec les préréglages d'ancres de l'Inspecteur ([UI.md](UI.md)).

## 6. Les pièces

1. *Créer › 2D › Sprite*, le renommer `Pièce`, position `(3, -0.2)`, échelle `(0.4, 0.4)`, couleur jaune.
2. *Ajouter un composant › Collider2D*, cocher `isTrigger` : un **déclencheur** détecte ce qui le traverse
   sans le bloquer.
3. Nouveau script `piece.lua`, glissé sur la pièce :

```lua
-- assets/scripts/piece.lua
local Piece = {
    properties = { valeur = 1 },
}

function Piece:onTrigger(other, began)
    if not began or other:name() ~= "Joueur" then return end
    -- agr.globals garde des valeurs d'une scène à l'autre (score, vies...).
    agr.globals.score = (agr.globals.score or 0) + self.valeur
    local texte = agr.find("Score")
    if texte then texte:setText("Pièces : " .. agr.globals.score) end
    self.entity:destroy()
end

return Piece
```

4. Clic droit sur la pièce › *Créer un prefab* : un fichier `Pièce.agrprefab` apparaît dans les Assets.
   Le glisser plusieurs fois dans la vue pose d'autres pièces, toutes reliées au prefab : changer la
   valeur ou l'image de l'original puis *Appliquer au prefab* met toutes les copies à jour
   ([PREFABS.md](PREFABS.md)).

Jouer : chaque pièce touchée disparaît et le compteur augmente.

## 7. Aller plus loin

- **Caméra qui suit le joueur** : *Affichage › Plugins et paquets*, onglet *En ligne*, *Bibliothèque
  officielle*, installer `follow-camera`, puis glisser `kits/camera/follow_camera.lua` sur la caméra et
  mettre sa propriété `target` à `Joueur`. Les autres paquets officiels (santé,
  dialogues, sauvegardes, menu pause, joystick tactile...) s'installent de la même façon
  ([PACKAGES.md](PACKAGES.md)).
- **Niveau en tuiles** : *Créer › 2D › Tilemap*, une feuille de tuiles, la palette et les pinceaux de
  l'Inspecteur, et un `TilemapCollider2D` pour que les tuiles soient solides ([2D.md](2D.md)).
- **Animations du personnage** : une feuille de sprites découpée dans l'éditeur de feuilles et un
  `SpriteAnimator` ([2D.md](2D.md), [ANIMATION.md](ANIMATION.md)).
- **Sons** : *Créer › Source audio*, ou `agr.audio.play("assets://audio/piece.wav")` dans `piece.lua`
  ([AUDIO.md](AUDIO.md)).
- **Exporter** : *Fichier › Exporter le jeu...* pour un dossier PC, un `.exe` Windows ou un APK Android ;
  avec un téléphone branché, *Installer et lancer sur l'appareil* ([EXPORT.md](EXPORT.md)).

Le modèle **Jeu : Plateforme 2D** du hub montre un jeu complet bâti sur les mêmes idées (ennemis, ressorts,
points de contrôle, menus).
