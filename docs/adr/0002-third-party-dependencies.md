# ADR 0002 — Dépendances tierces

**Statut :** accepté pour la phase 1 ; choix des phases suivantes **proposés**, à confirmer
au début de chaque phase.

Critères : licence compatible avec un produit commercial fermé, maintenance active, support
d'Android, taille binaire, et pas de réinvention d'un domaine déjà résolu de façon fiable
(physique, décodage d'images et de sons, import de modèles).

## Utilisées aujourd'hui

| Besoin | Choix | Pourquoi |
|---|---|---|
| Fenêtre, entrées, tactile, capteurs, cycle de vie mobile | **SDL3** (zlib) | Référence sur Android/Windows/Linux, gère `SDL_main` et l'activité Android, capteurs (gyroscope, accéléromètre), vibrations, Vulkan et EGL |
| JSON | **nlohmann/json** (MIT) | Fiable, lisible. Les builds de jeu utiliseront un format binaire (phase 10) |
| Tests | **doctest** (MIT) | En-tête unique, compilation rapide |

## Prévues

| Phase | Besoin | Choix proposé | Alternatives écartées |
|---|---|---|---|
| 2 | Vulkan : chargement / allocation | **volk** + **VulkanMemoryAllocator** (MIT) | chargement manuel, allocateur maison |
| 2 | Shaders | GLSL ➜ SPIR-V (**glslang**), ➜ GLSL ES via **SPIRV-Cross** | maintenir deux jeux de shaders à la main |
| 2 | Images | **stb_image** (PNG, JPG), **libwebp**, **nanosvg** | — |
| 3 | Interface de l'éditeur | **Dear ImGui** (branche *docking*, MIT) + thème AGR | Qt (lourd, licence), UI maison (des années de travail) |
| 5 | Modèles 3D | **cgltf** (glTF/GLB), **ufbx** (FBX, OBJ) | Assimp (volumineux) |
| 5 | Polices | **FreeType** + **msdfgen** | stb_truetype (qualité inférieure) |
| 6 | Physique 3D | **Jolt Physics** (MIT) | Bullet (moins maintenu), PhysX (taille sur mobile) |
| 6 | Physique 2D | **Box2D v3** (MIT) | Chipmunk |
| 6 | Scripting | **Lua 5.4** ou **Luau** + liaisons générées depuis la réflexion | C# : un runtime .NET embarqué ajoute 20 à 40 Mo à l'APK et complique la compilation AOT sur Android |
| 7 | Audio | **miniaudio** (domaine public / MIT-0) : WAV, MP3, FLAC, spatialisation, mixage | FMOD/Wwise (licences payantes) |
| 7 | OGG | **stb_vorbis** | libvorbis |
| 9 | Navigation | **Recast & Detour** (zlib) | — |

Le choix Lua vs Luau sera tranché en phase 6 sur trois critères mesurés : performances sur un
téléphone d'entrée de gamme, typage graduel (Luau) et qualité des messages d'erreur pour les débutants.
