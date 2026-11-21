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

## Effets des bus

Chaque bus a une **chaîne d'effets**, appliquée dans l'ordre à tout ce qu'il mixe (dans le graphe de
miniaudio, sur le fil audio : modifier un effet pendant que les sons jouent ne coupe rien) :

| Effet | Réglages | Usages |
|---|---|---|
| `LowPass` (passe-bas) | coupure (Hz), résonance | son étouffé sous l'eau / derrière un mur, musique de pause |
| `HighPass` (passe-haut) | coupure (Hz), résonance | radio, téléphone, enlever les graves |
| `Echo` | délai (s, jusqu'à 2), réinjection, mélange | canyon, voix magique |
| `Reverb` (réverbération, type Freeverb, stéréo) | taille de la salle, amortissement, mélange | grotte, cathédrale, salle |
| `Compressor` | seuil (dB), ratio, attaque / relâchement (ms), gain (dB) | éviter la saturation quand beaucoup de sons jouent, voix régulières |
| `Distortion` (saturation douce) | attaque du signal, mélange | moteurs, radio grésillante, rage |
| `Chorus` | vitesse (Hz), profondeur (ms), mélange | voix de robot, ambiance aquatique, élargir un son |
| `Equalizer` (égaliseur 3 bandes) | graves (étagère), médiums (cloche), aigus (étagère) : gain en dB et fréquence | sonorité de la musique, radio, renforcer les basses d'une explosion |
| `Phaser` | vitesse (Hz), étendue (octaves), réinjection, mélange | science-fiction, guitare, téléportation |
| `PitchShift` (décalage de hauteur) | `pitch` (2 = une octave plus haut), mélange | voix de monstre / d'écureuil sans changer la vitesse |
| `Analyzer` (analyseur de spectre) | — (le son n'est pas modifié) | visualiseurs, jeux de rythme : `AudioSystem::spectrum(bus, bandes)`, `agr.audio.spectrum("Music", 16)` (niveaux 0..1, 40 Hz → 16 kHz) ; le mixeur affiche le spectre |

- **Mixeur** (*Affichage › Mixeur audio*) : cliquer le nom d'un bus montre ses effets ; *+ Ajouter un
  effet*, case pour l'activer, réglages, `^` pour le monter dans l'ordre, `x` pour le retirer — le son
  change tout de suite. *Enregistrer le mixage* écrit volumes, coupures et effets dans
  `assets/audio_buses.json`, que le jeu applique au lancement (et relit s'il change pendant qu'il
  tourne).
- Les nouveaux projets ont un léger **compresseur sur Master** (seuil -6 dB, 4:1) qui évite la
  saturation quand beaucoup de sons se superposent.
- Lua : `agr.audio.addBusEffect("Music", "LowPass", { cutoff = 400 })` (renvoie son numéro),
  `agr.audio.setBusEffect("Music", 1, "cutoff", 800)` ou `(..., "enabled", false)`,
  `agr.audio.clearBusEffects(bus)`, `agr.audio.busEffectCount(bus)`.
- C++ : `AudioSystem::setBusEffects`, `busEffects`, `applyLayout`, `layout` ; `AudioEffectChain`
  traite aussi du son hors mixeur (rendu hors ligne).

## Composants

| Composant | Champs |
|---|---|
| `AudioSource` | `clip`, `bus`, `volume`, `pitch` (vitesse), `loop`, `playOnStart`, `spatial`, `minDistance`, `maxDistance`, `stream` ; spatial : `attenuation` (`None`, `Inverse`, `Linear`, `Exponential`), `rolloff`, `doppler` (0 = sans, 1 = réaliste : un véhicule qui approche sonne plus aigu), cône de directivité (`coneInnerAngle`, `coneOuterAngle`, `coneOuterVolume` ; avant = -Z de l'entité : haut-parleur, voix d'un personnage) ; variations à chaque départ (pas, impacts) : `alternateClips` (autres clips tirés au hasard), `randomPitch`, `randomVolume` |
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

Mixage de 32 voix (dont 16 spatiales, rééchantillonnées) : ~0,12 ms pour 10,7 ms de son, soit ~1,1 %
d'un cœur de la machine de test ; avec une réverbération et un passe-bas sur `Effects` et un compresseur
sur `Master` : ~0,17 ms ([BENCHMARKS.md](BENCHMARKS.md)). Tests : `audio effects` et `bus effects`
(`tests/test_audio.cpp`), API Lua (`tests/test_script.cpp`).

## Générateur (son procédural)

`agr.audio.generator([volume [, fréquence]])` ouvre une voix qui joue les échantillons que le jeu lui
pousse (mono, −1 à 1) : `agr.audio.push(id, {…})` (rend le nombre accepté, la file garde 2 s au plus),
`agr.audio.queued(id)` (en attente : garder au moins une image d'avance), `agr.audio.stopSound(id)`.
Silence quand la file est vide (la voix continue). Synthés, bips calculés, sons de moteur. C++ :
`AudioSystem::playGenerator`, `pushGenerator`, `generatorQueued`.

## Microphone

`agr.audio.startRecording()` commence à enregistrer le micro (mono, au taux du mixeur ;
`agr.audio.isRecording()`), `agr.audio.recordingLevel()` donne le niveau des 50 dernières ms (0 à 1 :
vumètre, « souffler pour jouer »), `agr.audio.stopRecording("user://voix.wav")` arrête et écrit un WAV
16 bits jouable aussitôt (`agr.audio.play`), et rend son chemin et sa durée. Sans micro (ou en sortie
virtuelle), l'enregistrement reçoit les échantillons passés à `AudioSystem::feedRecording` (tests,
sources personnalisées). Sur Android, cocher *Microphone* dans l'export ajoute la permission `RECORD_AUDIO`.

## Limites actuelles

- Enregistrement : mono, au taux du mixeur, gardé en mémoire jusqu'à `stopRecording` (pas de
  traitement en direct du flux du micro) ; pas d'occlusion automatique (un script peut baisser le volume ou
  ajouter un passe-bas selon un rayon) ; pas de bus créés par l'utilisateur (les six bus sont fixes).
- Doppler : vitesses estimées d'une mise à jour à l'autre (téléporter une source produit un saut de
  hauteur pendant une image).
- Pas encore testé sur un appareil Android (sortie AAudio / OpenSL ES de miniaudio : phase 8).
