# BodyMotion Studio — par Super Essence Studio

**Create. Capture. Animate.** Application Android de motion capture : filmez-vous, votre personnage 3D reproduit vos mouvements, puis exportez l'animation pour Unity, Unreal ou Godot.

- **APK signé** : [`release/BodyMotion-Studio-1.0.2.apk`](release/BodyMotion-Studio-1.0.2.apk) (Android 8.0+, arm64 / armv7, ~70 Mo, aucun compte, aucun réseau)
- **Captures de l'application** : [`branding/captures/`](branding/captures/)

## Ce qui fonctionne

| Domaine | Détail |
|---|---|
| Démarrage | Signature animée SUPER ESSENCE STUDIO (symbole lumineux, particules, logotype « SuperEssence. »), puis tracé du logo BodyMotion Studio. Version complète au premier lancement, courte ensuite. Toucher pour passer. |
| Import 3D | **FBX binaire** (convertisseur intégré : hiérarchie, maillages, skin, matériaux, textures intégrées ou dans un .zip, blendshapes), **GLB**, **glTF** (intégré ou en .zip), **VRM** 0.x et 1.0. Rapport : nombre d'os, os humanoïdes trouvés, doigts, blendshapes, taille du rig. Modèle sans squelette détecté et signalé. Correspondance des os modifiable à la main. |
| Retargeting | Détection automatique des os (VRM, Mixamo, Unreal, Unity, Blender/Rigify, VRoid…). Chaque os suit la direction du segment détecté **à partir de sa propre pose de repos** : proportions, tailles et orientations du modèle sont toujours respectées. Bassin, torse réparti sur la colonne, tête, coudes/genoux (plan de flexion), mains, doigts, pieds au sol, root motion activable. |
| Capture caméra | MediaPipe sur l'appareil : corps 33 points 2D+3D, mains 21 points (option), 52 expressions du visage (option). Lissage One-Euro réglable, détection et reprise auto des pertes de suivi, calibration initiale (proportions, sol, tête), aperçu 3D en direct, caméra avant/arrière, délégué GPU avec repli CPU automatique. |
| Capture vidéo | Analyse image par image d'une vidéo de la galerie, à la cadence choisie. |
| Nettoyage | Rééchantillonnage à 24/30/60 FPS, trous ≤ 1,5 s interpolés, trous plus longs figés, images reconstruites signalées en orange dans la timeline. |
| Viewport | Moteur Filament (Google) : PBR, lumière directionnelle + contour, ombres, MSAA 4×, FXAA, occlusion ambiante, sol, caméra orbitale (orbite, zoom, déplacement), qualité Basse / Moyenne / Haute, affichage du squelette. |
| Éditeur | Lecture, pause, image par image, vitesse ×0,25–×2, boucle. Timeline 7 pistes (Head, Torso, Left/Right Arm, Left/Right Leg, Root Motion) avec clés réelles : ajout, suppression (ré-interpolation), suppression sur plage, lissage par piste ou global. Rotation d'articulation X/Y/Z avec aperçu immédiat et fondu sur ± N images, déplacement du root. Couper, scinder, dupliquer, fusionner (avec fondu), rendre bouclable, changer la vitesse, garder sur place. Annuler / rétablir. |
| Bibliothèque | Miniatures animées, catégories (Idle, Walk, Run… Dance), renommer, dupliquer, supprimer, multi-sélection pour l'export. |
| Projets | Personnage + animations + moteur cible, export du projet en une fois. |
| Export | **FBX** binaire 7.4 (squelette + animations, une Take par clip), **GLB / glTF** (personnage complet avec matériaux + animations, ou squelette seul ; blendshapes du visage inclus), **BVH**. Profils Unity / Unreal / Godot / Générique (os « root » au sol pour le root motion Unreal/Godot), FPS, qualité des courbes (sans perte / réduction de clés), root motion on/off, noms d'os d'origine conservés. Partage ou enregistrement sur l'appareil. |

## Validation

- `./gradlew :app:testDebugUnitTest` : 17 tests.
  - Retargeting : les segments des membres sont reproduits à **0,00°** près, y compris sur un squelette aux orientations de repos aléatoires. La colonne, déduite des épaules et du bassin, l'est à ~2°.
  - Le contact des pieds avec le sol, l'édition (clés, coupe, fusion, boucle, vitesse) et la sérialisation sont testés.
  - Les exports sont relus et l'import FBX est vérifié sur un fichier produit par Blender.
  - Les 10 écrans s'affichent sans erreur (Robolectric).
- `tools/validate_exports.py` (avec `pip install bpy`) : chaque export (FBX Unity/Unreal, GLB, glTF, BVH) s'importe dans **Blender 5.3** avec son armature et ses animations. Le FBX converti par l'application redonne exactement les dimensions du modèle d'origine.

## Limites réelles

- **Profondeur** : une seule caméra ne mesure pas la profondeur. Elle est estimée par le modèle de détection : les mouvements vers ou loin de la caméra et les membres masqués sont moins précis. Le déplacement global (root motion) est lui aussi estimé à partir de l'image.
- **Multi-caméra / multi-angle** : non disponible dans cette version.
- **FBX** :
  - Le FBX texte (ASCII) n'est pas accepté à l'import (message explicite).
  - L'export FBX contient le squelette et les animations, sans les maillages. Pour exporter personnage complet + animations, utilisez GLB/glTF.
- **Visage** : la capture faciale ne s'active que si le modèle possède des blendshapes compatibles (noms ARKit, expressions VRM, noms courants).
- **Correction des pieds** : pas de verrouillage des pieds par IK dans l'éditeur. Les pieds sont posés au sol à la capture.
- **Testé** : le cœur et l'interface sont testés automatiquement, mais l'application n'a pas encore été essayée sur un téléphone réel.

## Architecture

```
app/src/main/java/com/superessence/bodymotion/
├── math/       Vec3, Quat, Mat4 (pur Kotlin)
├── io/         Json ; gltf/GltfDocument (lecture/écriture GLB/glTF) ; fbx/FbxNode (FBX binaire lecture+écriture), FbxImporter
├── rig/        Skeleton, Humanoid (os standards + détection auto), FaceMapping (ARKit/VRM)
├── mocap/      BodyTracker (MediaPipe), Filters (One-Euro), ClipBuilder, VideoProcessor, PoseFrame
├── retarget/   Retargeter (directions + plans de flexion + pieds au sol + root motion), Calibration
├── anim/       AnimationClip (format .bmac), ClipOps (édition)
├── export/     ExportOptions/Prep (profils), GltfExporter, FbxExporter, BvhExporter
├── render/     CharacterViewer (Filament)
├── data/       Store (stockage local), ModelImporter
└── ui/         Compose : intro, accueil, personnages, capture, bibliothèque, éditeur, export, projets, paramètres
tools/make_assets.py        mannequin de démonstration + sol
tools/validate_exports.py   validation des exports dans Blender
```

Construction : `./gradlew :app:assembleRelease` (SDK Android 35). La clé de signature du dépôt (`keystore/`) sert aux tests : remplacez-la par la vôtre avant une publication.

Licences tierces : MediaPipe et Filament (Apache 2.0, Google). Polices Inter, Space Grotesk, JetBrains Mono et Fredoka (SIL OFL 1.1).
