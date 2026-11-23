# Tutoriel : un jeu 3D, ramasser des orbes sans se faire attraper

Ce tutoriel construit un petit jeu 3D : un héros qui court et saute sur un sol, des orbes qui tournent
et qu'on ramasse, un compteur, et un ennemi qui poursuit le héros en contournant les obstacles. Faire
d'abord le [tutoriel 2D](TUTORIAL_2D.md) aide (mêmes gestes dans l'éditeur), mais ce n'est pas
obligatoire. Les trois scripts sont **exécutés par les tests du moteur** (test « tutorial 3D » de
`tests/test_game_templates.cpp`) dans la scène décrite ici : le héros se pose, court, ramasse un orbe,
et l'ennemi le rejoint autour d'un mur.

## 1. Le projet et le sol

1. Hub de projets : type **3D**, modèle **Scène 3D vide**, un nom, *Créer*. La scène contient une
   caméra (`Main Camera`), un soleil, un sol `Ground` (un plan) et un cube. Supprimer le cube (Suppr).
2. Sélectionner `Ground`, le renommer `Sol` (F2), échelle `(30, 1, 30)`.
3. *Ajouter un composant › Collider* (forme `Box`, taille `(1, 0.1, 1)` : elle suit l'échelle de l'objet).
   Le plan seul n'est qu'une image : c'est le collider qui rend le sol solide. Sans `RigidBody`, il est
   statique.
4. Placer `Main Camera` au-dessus et en arrière : position `(0, 12, 14)`, rotation `(-40, 0, 0)` ; la vue *Jeu*
   de la vue Scène montre ce qu'elle voit.

## 2. Le héros

1. *Créer › Formes 3D › Cube*, le renommer `Héros`, position `(0, 1, 0)`.
2. *Ajouter un composant › CharacterController3D* : une capsule (pieds à l'origine de l'objet) qui glisse
   le long des murs, monte les marches et les pentes, et pousse les objets dynamiques.
3. Nouveau script `heros.lua` (Assets › *Nouveau script*), glissé sur le héros :

```lua
-- assets/scripts/heros.lua
local Heros = {
    properties = { vitesse = 5, saut = 6 },
}

function Heros:onUpdate(dt)
    local _, vy, _ = self.entity:velocity()
    if self.entity:isGrounded() and agr.input.pressed("jump") then vy = self.saut end
    -- move_y vaut 1 vers le haut de l'écran : on avance vers -Z, loin de la caméra.
    local x, z = agr.input.value("move_x"), agr.input.value("move_y")
    self.entity:setVelocity(x * self.vitesse, vy, -z * self.vitesse)   -- la gravité s'ajoute en l'air
end

return Heros
```

Jouer (Ctrl+P) : flèches ou ZQSD / WASD pour courir, Espace pour sauter, une manette marche aussi.

## 3. Les orbes et le compteur

1. *Créer › Interface (UI) › Canvas*, puis en enfant *Interface (UI) › Texte*, renommé `Score`, texte
   `Orbes : 0`.
2. *Créer › Formes 3D › Sphère*, la renommer `Orbe`, position `(4, 0.8, 0)`, échelle `(0.6, 0.6, 0.6)`.
   Pour la faire briller : un matériau avec une couleur d'émission ([3D.md](3D.md)).
3. *Ajouter un composant › Collider*, forme `Sphere`, cocher `isTrigger`.
4. Script `orbe.lua` :

```lua
-- assets/scripts/orbe.lua
local Orbe = {
    properties = { tours = 90 },   -- degrés par seconde
}

function Orbe:onUpdate(dt)
    self.entity:rotate(0, self.tours * dt, 0)
end

function Orbe:onTrigger(other, began)
    if not began or other:name() ~= "Héros" then return end
    agr.globals.orbes = (agr.globals.orbes or 0) + 1
    local texte = agr.find("Score")
    if texte then texte:setText("Orbes : " .. agr.globals.orbes) end
    self.entity:destroy()
end

return Orbe
```

5. Clic droit › *Créer un prefab*, puis glisser le prefab plusieurs fois dans la vue pour semer des orbes.

## 4. Un ennemi qui poursuit le héros

La **navigation** calcule des chemins qui contournent les obstacles, à partir des colliders statiques
de la scène ([NAVIGATION.md](NAVIGATION.md)).

1. Un obstacle : *Formes 3D › Cube* `Mur`, position `(-3, 1, 0)`, échelle `(1, 2, 8)`, avec un `Collider`.
2. L'ennemi : *Formes 3D › Cube* `Ennemi`, position `(-8, 0.5, 0)`, couleur rouge, et
   *Ajouter un composant › NavAgent* (vitesse `3`). Le bouton *Navigation* de la vue Scène affiche le
   maillage de navigation calculé (en cyan).
3. Script `ennemi.lua` :

```lua
-- assets/scripts/ennemi.lua
local Ennemi = {
    properties = { cible = "Héros", portee = 1.3 },
}

function Ennemi:onStart()
    self.attente = 0
end

function Ennemi:onUpdate(dt)
    local heros = agr.find(self.cible)
    if not heros then return end
    local hx, hy, hz = heros:position()
    -- Nouveau chemin quatre fois par seconde : assez pour suivre un héros qui bouge.
    self.attente = self.attente - dt
    if self.attente <= 0 then
        self.entity:navigateTo(hx, hy, hz)
        self.attente = 0.25
    end
    local ex, _, ez = self.entity:position()
    if (hx - ex) ^ 2 + (hz - ez) ^ 2 < self.portee ^ 2 then
        local texte = agr.find("Score")
        if texte then texte:setText("Attrapé !") end
        agr.time.setScale(0)   -- tout s'arrête ; Arrêter remet le jeu en place
    end
end

return Ennemi
```

Jouer : l'ennemi contourne le mur pour rejoindre le héros. Il faut ramasser les orbes en restant hors
de sa portée.

## 5. Aller plus loin

- **Caméra à la troisième personne** : installer le paquet officiel `follow-camera` (*Affichage › Plugins
  et paquets*, onglet *En ligne*, *Bibliothèque officielle*) ou `fps-controller` pour une vue à la première
  personne ([PACKAGES.md](PACKAGES.md)).
- **Un vrai personnage** : glisser un modèle `.glb` animé dans la vue, puis un `Animator` et sa machine
  à états (marche, course, saut) ([ANIMATION.md](ANIMATION.md)).
- **Un décor** : *Créer › Caméra et lumières › Terrain* (pinceaux de hauteur et de peinture), de l'eau,
  du brouillard, des particules ([3D.md](3D.md), [TERRAIN.md](TERRAIN.md)).
- **Vie et dégâts** : le paquet `health` (signaux `damaged` et `died`), et `pause-menu` pour mettre le
  jeu en pause.
- **Exporter** : *Fichier › Exporter le jeu...* ([EXPORT.md](EXPORT.md)).

Le modèle **Jeu : RPG 3D** du hub reprend ces idées à plus grande échelle (terrain, golems qui
poursuivent le héros sur le maillage de navigation, quêtes).
