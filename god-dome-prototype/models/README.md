# Modèles 3D externes (Tripo, Blender…)

Déposez ici des fichiers `.glb` et un fichier `manifest.json` qui les décrit :

```json
[
  { "file": "heros.glb",    "role": "player",   "height": 1.8 },
  { "file": "fermiere.glb", "role": "villager", "height": 1.7, "name": "Brune" },
  { "file": "rocher.glb",   "role": "scatter",  "height": 1.2, "count": 30 },
  { "file": "statue.glb",   "role": "prop",     "height": 4, "x": -30, "z": 10, "rotY": 0 }
]
```

- `player` : remplace l'apparence de l'habitant ; `villager` : celle d'un ou de tous les villageois (`name`) ;
- `prop` : objet posé à une position ; `scatter` : copies réparties dans la nature.
- `height` est en mètres : le modèle est mis à l'échelle et posé au sol automatiquement.

Sans `manifest.json`, le jeu utilise ses modèles procéduraux.
Conseil mobile : exporter depuis Tripo en GLB avec textures 1024 px et < 50 000 triangles par modèle.
