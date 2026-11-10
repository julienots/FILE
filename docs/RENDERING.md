# Rendu et ressources (phase 2)

![Sandbox AGR ENGINE](images/sandbox.png)

*La sandbox en profil « high » : maillages éclairés, sol texturé, sprites alpha triés et regroupés,
hiérarchie animée par le plugin dynamique Spin. Capture produite par le moteur lui-même
(`agr_sandbox --profile high --screenshot user://shot.png`).*

## Couches

```
Renderer  (agr::render)   scène ➜ listes de rendu, culling, tri, lots, profils
   │  utilise
AssetManager (agr::assets) URI ➜ textures (cache, async, budget d'upload, rechargement à chaud)
   │  utilisent
rhi::Device (agr::rhi)    API GPU neutre : buffers, textures, pipelines, cibles, passes
   │  implémentée par
GLDevice                  OpenGL 3.3 core (desktop) / OpenGL ES 3.0 (Android)   ✅
VulkanDevice              Vulkan 1.1                                            ⬜ prochaine étape
```

`RenderPlugin` relie le tout au moteur : il crée le device sur le contexte de la plateforme,
applique le profil graphique et publie les services :

```cpp
engine.plugins().addStatic(std::make_unique<agr::RenderPlugin>());   // avant initialize()
...
auto* assets   = engine.service<agr::AssetManager>();
auto* renderer = engine.service<agr::Renderer>();
```

## RHI

- Tous les objets GPU sont des **handles générationnels** : un handle détruit est détecté au lieu de
  pointer vers un objet recyclé.
- Les uniformes vivent dans des **uniform buffers std140** liés à des *slots* numérotés, et les
  textures à des *unités* numérotées. C'est le modèle des descriptor sets Vulkan ; sur GL, les
  liaisons sont posées après l'édition des liens (`glUniformBlockBinding`, `glUniform1i`), car
  GLSL 3.30 et GLSL ES 3.00 n'ont pas `layout(binding = N)`.
- Les données par objet sont écrites une seule fois par image dans **un seul uniform buffer**
  (alignement du device respecté) puis sélectionnées par `bindUniformBuffer(slot, buffer, offset, size)`.
- Les buffers dynamiques réécrits depuis l'offset 0 sont « orphelins » : le driver fournit un nouveau
  stockage, sans attente CPU/GPU.
- `glDrawElementsBaseVertex` n'existe pas en OpenGL ES 3.0 : le *base vertex* est émulé par le
  décalage des pointeurs d'attributs.
- En fin de passe, la profondeur est invalidée (`glInvalidateFramebuffer`) : sur les GPU mobiles à
  tuiles, elle n'est jamais écrite en mémoire.
- Le chargeur OpenGL est intégré (aucune dépendance) : la liste des fonctions requises et optionnelles
  est dans `engine/rhi/src/gl/GL.hpp`.

### Dialecte de shaders

Les shaders sont écrits dans le sous-ensemble commun GLSL 3.30 / GLSL ES 3.00, **sans** ligne
`#version` : le backend ajoute l'en-tête (`#version 300 es` + précisions + `#define AGR_GLES` sur
mobile). Entrées de vertex et sorties de fragment en `layout(location = N)`, blocs d'uniformes
`std140`. Une erreur de compilation renvoie le journal complet du compilateur.

Le backend Vulkan compilera les mêmes sources en SPIR-V (glslang), ce qui évitera de maintenir deux
jeux de shaders.

### Conventions

| Sujet | Convention |
|---|---|
| Repère | Main droite, +Y en haut, la caméra regarde vers −Z |
| Faces avant | Sens antihoraire |
| Textures | Première ligne = haut de l'image ; UV (0,0) = coin haut-gauche (identique sur GL et Vulkan) |
| Profondeur | [−1, 1] en GL, [0, 1] en Vulkan : choisie par le renderer selon le backend |
| Couleur | Couleurs des composants en sRGB ; éclairage calculé en linéaire ; textures couleur en `RGBA8_sRGB` ; sortie encodée gamma dans une cible 8 bits |

## Renderer

Un renderer *forward* :

1. Sélection de la caméra : la première caméra `primary` active (indice d'entité le plus bas en cas d'égalité).
2. Collecte des `MeshRenderer` et des `SpriteRenderer` visibles, **frustum culling** par AABB.
3. Tri : maillages opaques par maillage puis texture ; sprites par calque, ordre, profondeur
   (de l'arrière vers l'avant) puis texture.
4. Une passe : maillages, puis sprites (test de profondeur actif, écriture désactivée : un sprite est
   caché par la géométrie 3D placée devant lui).
5. Si le profil demande du MSAA ou une échelle de rendu ≠ 1 : rendu hors écran, *resolve* MSAA, puis
   mise à l'échelle bilinéaire vers l'écran. Sinon, rendu direct dans le backbuffer, ce qui économise
   la bande passante sur mobile.

**Éclairage actuel :** Lambert + reflet Blinn-Phong, une lumière directionnelle et une ambiance.
Il sera remplacé par le PBR, les ombres et plusieurs lumières en phase 5.

**Lots de sprites :** un seul vertex buffer dynamique par image et un index buffer de quads fixe.
Un lot se termine à chaque changement de texture. Avec une caméra orthographique (jeux 2D), les sprites
d'un même plan ont la même profondeur : le tri les regroupe par texture (10 000 sprites sur 2 textures
alternées = 2 draw calls). Avec une caméra perspective, l'ordre de l'arrière vers l'avant, nécessaire
à une transparence correcte, limite les regroupements. Les atlas de textures (phase 4) supprimeront
la plupart des changements de texture.

### Composants

| Composant | Champs principaux |
|---|---|
| `Camera` | projection (perspective / orthographique), champ de vision, taille ortho, plans proche/lointain, couleur de fond, primaire |
| `DirectionalLight` | couleur, intensité (direction = axe −Z de l'entité) |
| `Environment` | couleur et intensité ambiantes |
| `MeshRenderer` | maillage intégré (Cube, Sphere, Plane, Quad), texture, couleur, répétition UV, visible |
| `SpriteRenderer` | texture, couleur, taille, pivot, rectangle UV (sprite sheets), miroir X/Y, calque, ordre, pixel art, visible |

Tous sont réfléchis : ils sont sauvegardés dans les scènes et seront éditables dans l'inspecteur.

## Ressources

```cpp
auto& assets = *engine.service<agr::AssetManager>();
agr::TextureAsset hero = assets.loadTexture("assets://sprites/hero.png", {.pixelArt = true});
rhi::TextureHandle gpu = assets.gpuTexture(hero);   // blanc pendant le chargement, damier magenta en cas d'erreur
assets.release(hero);                               // détruite au prochain update() sans référence
```

- **Formats :** PNG, JPEG, TGA, BMP, PSD (aplati), GIF (première image), HDR, via stb_image.
  **WEBP et SVG ne sont pas encore pris en charge** (libwebp et nanosvg sont prévus).
- **Asynchrone :** lecture et décodage sur les workers du `JobSystem`, envoi au GPU sur le thread
  principal dans `update()`, limité par un **budget d'octets par image** pour éviter les saccades.
- **Cache :** même URI et mêmes options = même ressource, avec comptage de références.
- **Profil :** les textures plus grandes que `maxTextureSize` sont réduites au chargement.
- **Rechargement à chaud :** `RenderPlugin::setHotReload(true)` surveille la date de modification des
  fichiers ; la texture est remplacée sans changer de handle.
- **Sans GPU** (serveur, tests) : les images sont décodées et gardées en mémoire CPU.

## Profils graphiques

| Profil | Échelle 3D | MSAA | Texture max | Limite FPS | Budget d'upload/image |
|---|---|---|---|---|---|
| ultra | 1,0 | x8 | 8192 | — | 64 Mo |
| high | 1,0 | x4 | 4096 | — | 32 Mo |
| medium | 1,0 | x2 | 2048 | — | 16 Mo |
| low | 0,75 | — | 1024 | 60 | 8 Mo |
| ultra-mobile | 0,5 | — | 512 | 30 | 4 Mo |

Valeurs plafonnées par les capacités du GPU (par exemple MSAA x8 ➜ x4 si le GPU ne fait pas mieux).
Changement à chaud : `RenderPlugin::applyProfile(engine, GraphicsTier::Low)` (touches 1 à 5 dans la sandbox).

**Sélection automatique** (`"graphicsProfile": "auto"`, valeur par défaut) :

| Situation | Profil |
|---|---|
| Rendu logiciel (llvmpipe, SwiftShader…) | low |
| Desktop avec GPU matériel | high (ultra n'est jamais choisi automatiquement) |
| Mobile / GLES, texture max ≥ 8192 et MSAA ≥ 4 | medium |
| Mobile / GLES, texture max ≥ 4096 | low |
| Autre mobile | ultra-mobile |

Cette heuristique ne s'appuie que sur les capacités déclarées par le pilote. Elle sera affinée en
phase 8 avec des mesures sur de vrais téléphones (mémoire, famille de GPU, test de performance au
premier lancement).

## Statistiques (profiler)

`Renderer::stats()` : draw calls, triangles, maillages et sprites visibles ou éliminés, lots de sprites,
résolution interne, MSAA, temps CPU (collecte et soumission séparées), **temps GPU** (requêtes
`GL_TIME_ELAPSED`, ou `EXT_disjoint_timer_query` sur mobile ; −1 si indisponible), mémoire GPU.

## Captures d'écran

`RenderPlugin::requestScreenshot("user://shot.png")` enregistre l'image suivante en PNG
(F12 dans la sandbox). `Device::readBackbuffer` est aussi utilisé par les tests GPU.

## Limites actuelles

- Pas encore de backend Vulkan (la RHI est prête à le recevoir ; c'est l'étape suivante).
- Pas de maillages importés, de PBR, d'ombres, de post-traitement, d'instancing ni de LOD (phase 5).
- Pas d'éditeur de matériaux ou de shaders (phases 5 et 9).
- Contexte GL perdu sur Android lors d'une mise en arrière-plan : la recréation des ressources est
  prévue en phase 8.
