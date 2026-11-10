# Audio (phase 7)

Module `agr::audio`, construit sur [miniaudio](https://miniaud.io) 0.11.25. Tout ce qui est décrit
ici est couvert par des tests automatisés (`tests/test_audio.cpp`, test de script audio, test de jeu
des modèles, test de bout en bout de l'éditeur).

## Formats et chargement

- **WAV, Ogg Vorbis, MP3, FLAC**, mono ou stéréo, à n'importe quelle fréquence (rééchantillonnés à
  la volée vers la sortie, 48 kHz par défaut).
- Un clip est **décodé une fois en mémoire** et partagé par toutes ses voix ; avec `stream`, il est
  décodé pendant la lecture (musiques longues : moins de mémoire).
- Un fichier modifié sur le disque est relu à la lecture suivante ; un fichier manquant ou invalide
  donne un message clair (nom du fichier) sans arrêter le jeu.

## Mixeur

| Bus | Usage |
|---|---|
| `Master` | Tout le son ; les autres bus y sont reliés |
| `Music`, `Effects`, `Ambient`, `Interface`, `Voice` | Volume et coupure par catégorie |

- Chaque bus a un volume et une coupure. La **pause du jeu** suspend toutes les voix sauf celles du
  bus `Interface` (sons des menus).
- **Limite de voix** (64 par défaut) : au-delà, la voix non bouclée la plus ancienne est coupée.
- **Audio spatial** : une voix spatiale est atténuée avec la distance (pleine jusqu'à `minDistance`,
  modèle inverse jusqu'à `maxDistance`) et répartie gauche / droite selon sa position par rapport à
  l'auditeur (`AudioListener`, sinon la caméra principale).
- Sortie : le périphérique audio du système ; s'il n'y en a pas (serveur, CI), le mixage continue en
  silence au rythme réel (les sons finissent à l'heure). Sur mobile, la sortie est libérée quand
  l'application passe en arrière-plan et reprise au retour.

## Composants

| Composant | Champs |
|---|---|
| `AudioSource` | `clip`, `bus`, `volume`, `pitch` (vitesse), `loop`, `playOnStart`, `spatial`, `minDistance`, `maxDistance`, `stream` |
| `AudioListener` | `active` : position et orientation d'écoute (par défaut la caméra principale) |

Les sources suivent la scène : démarrage avec le jeu (`playOnStart`), position mise à jour à chaque
image, changement de volume / hauteur en direct, arrêt quand l'entité est désactivée, détruite ou que
le composant est retiré, reprise quand elle est réactivée.

## Depuis Lua

```lua
local voice = agr.audio.play("assets://audio/coin.wav", { volume = 0.8, pitch = 1.1 })
agr.audio.play("assets://audio/boom.wav", { x = 3, y = 0, z = -5, maxDistance = 40 })  -- spatial
agr.audio.stop(voice, 0.5)                  -- fondu de sortie de 0,5 s
agr.audio.setBusVolume("Music", 0.3)
agr.audio.setBusMuted("Effects", true)
self.entity:playSound()                    -- AudioSource de l'entité
```

Liste complète : [SCRIPTING.md](SCRIPTING.md). Sans `AudioPlugin` (serveur, tests), ces fonctions
n'ont aucun effet.

## Dans l'éditeur

- *Créer › Source audio* ; glisser un fichier audio dans le champ `clip`.
- Inspecteur : **Écouter / Arrêter** (aperçu sur le bus `Interface`) et durée du clip.
- Panneau **Mixeur audio** (*Affichage*) : sortie utilisée, voix, mémoire des clips, **vumètre** du
  bus `Master` en dB, volume et coupure de chaque bus.
- Les sources ne jouent qu'en mode Jeu ; *Arrêter* coupe les sons de la scène.

## Performances

Mixage de 32 voix (dont 16 spatiales, rééchantillonnées) : ~0,14 ms pour 10,7 ms de son, soit ~1,3 %
d'un cœur de la machine de test ([BENCHMARKS.md](BENCHMARKS.md)).

## Limites actuelles

- Pas d'effets (réverbération, filtres, compresseur), d'effet Doppler, d'occlusion ni de cônes de
  directivité.
- Les volumes du mixeur réglés dans l'éditeur ne sont pas enregistrés dans le projet (un jeu les
  règle par script).
- Pas encore testé sur un appareil Android (sortie AAudio / OpenSL ES de miniaudio : phase 8).
