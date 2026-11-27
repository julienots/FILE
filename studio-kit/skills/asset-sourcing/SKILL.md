---
name: asset-sourcing
description: Trouver, télécharger, vérifier et préparer des assets 3D/2D/audio libres de droits de qualité pro (modèles scannés, textures PBR, HDRI, personnages, polices) pour jeux et apps. Utiliser dès qu'un projet a besoin de modèles, textures, ciels, personnages, icônes, polices ou sons, ou quand l'utilisateur fournit un dossier d'assets (Drive, zip).
---

# Assets pro, légalement

## Sources fiables (licence vérifiable)
| Besoin | Source | Licence | Accès |
|---|---|---|---|
| Meubles/objets scannés, végétation | Poly Haven (`api.polyhaven.com/assets?t=models`, `/files/<id>`) | CC0 | API JSON, glTF 1k/2k/4k |
| Textures PBR (diff, nor_gl, arm) | Poly Haven `?t=textures` | CC0 | jpg 1k–8k |
| Ciels HDRI | Poly Haven `?t=hdris` (`*_puresky` = ciel seul) | CC0 | .hdr |
| Personnages animés Mixamo (démo) | dépôt three.js `examples/models/gltf/` (Soldier, Michelle, Xbot…) | usage démo | raw.githubusercontent |
| Polices | Google Fonts (css2 → woff2) | OFL | fonts.gstatic |
| Low-poly stylisé | Kenney, Quaternius | CC0 | souvent bloqués en sandbox |

Toujours : écrire un script `tools/fetch_assets.py` reproductible (les binaires lourds restent hors Git, mais **versionner les `.import`** qui portent les réglages de compression).

## Dossier fourni par l'utilisateur (Google Drive)
- Lister : `gdown --folder <url>` (les noms + IDs s'affichent même si le téléchargement échoue).
- Télécharger : `https://drive.usercontent.google.com/download?id=<ID>&export=download&confirm=t` (passe quand `docs.googleusercontent` est bloqué).
- **Planche-contact** avant de choisir : rendre une vignette par modèle (GLTFDocument au runtime), assembler avec PIL, regarder. Écarter ce qui ne colle pas au thème.

## Préparer (gltf-transform, `npm i -g @gltf-transform/cli`)
- `inspect` : extensions (EXT_meshopt / Draco non chargés par Godot) et triangles.
- `copy` ou `dequantize` : décompresse meshopt + quantization.
- `simplify --ratio 0.25 --error 0.001` : budget mobile (voiture ≤ 200 k tris, arbre ≤ 100 k).
- `resize --width 2048` (ou 1024 mobile) : textures embarquées.

## Règles
- Ne jamais présenter un asset non vérifié comme libre ; noter la source et la licence dans le dépôt (`docs/CREDITS.md`).
- Personnages réalistes modernes : il n'existe pas de bonne source CC0 → demander à l'utilisateur (Tripo/Meshy + rig Mixamo/AccuRig) et utiliser un provisoire animé en attendant, en le disant clairement.
- Pas de marque, logo, personnage ou décor reconnaissable d'une œuvre existante.
