---
name: godot-realistic-3d
description: Pipeline éprouvé pour un jeu 3D réaliste (PC/mobile) avec Godot 4.5 sans éditeur graphique : monde construit en code, matériaux PBR, éclairage photoréaliste, optimisation, captures headless, tests automatisés. Utiliser pour tout jeu 3D, simulation, open world, ou dès qu'on parle de Godot, Unity/Unreal indisponibles, rendu réaliste, APK de jeu.
---

# Godot 4.5 — jeu 3D réaliste piloté par le code

## Installer (sans GPU, en conteneur)
- Moteur : `github.com/godotengine/godot-builds/releases/download/4.5.1-stable/Godot_v4.5.1-stable_linux.x86_64.zip`.
- Rendu headless réel : `VK_ICD_FILENAMES=<lavapipe>/lvp_icd.json xvfb-run -a -s "-screen 0 1600x900x24" godot --path . --resolution 1600x900 -- --capture`. Lavapipe = Vulkan logiciel : Forward+ complet (SDFGI, SSR, SSAO, ombres) mais ~3-5 s/image → itérer en 1280×720, finaliser en 1600×900.
- `godot --headless --import --path .` après tout ajout d'asset ou de `class_name`. `--check-only --script` ne charge pas les autoloads (faux positifs « Identifier not found »).

## Architecture qui marche
- `World` (ciel HDRI, soleil, sol, rue, parcelles) ; `HouseBuilder` (murs avec ouvertures, fenêtres, portes, escalier, toit) ; `Interiors` (mobilier) ; `MatLib` (matériaux en cache) ; `Geo` (primitives).
- Boîtes **chanfreinées** (SurfaceTool) partout : jamais d'arêtes vives = fin du look « blockout ».
- Murs extérieurs = âme plâtre + peau bardage décalée ; fenêtres = cadre + petits bois + vitre + appuis + volets.
- `Geo.prop(nom, pos, rot, fit_len|fit_height)` : pose un glTF par sa base, centré, à l'échelle réelle. **Modèles riggés : ne pas ajuster** (AABB de maillage skinné non fiable, unités cm/Z-up).
- Autoloads : `GameClock` (minutes, jour, signaux), `Game` (besoins, flags, sauvegarde JSON `user://`, réglages, input map créée en code).

## Matériaux et lumière (le réalisme vient d'ici)
- Textures Poly Haven en **triplanaire monde** (`uv1_world_triplanar`, échelle = taille réelle) : diff + nor_gl + arm (ORMMaterial3D).
- Murs : peinture mate unie + normale de plâtre (pas de diffuse tachée). Textiles : couleur unie + normale de tissu + rim. Plans de travail : uni brillant + clearcoat.
- Pelouse : shader anti-répétition (2 échelles + rotation + bruit de teinte macro).
- Environnement : ciel HDRI `PanoramaSkyMaterial`, ACES, SDFGI (`frames_to_converge=0` pour captures), SSAO/SSIL/SSR, glow léger, **`fog_sky_affect=0`** (sinon ciel blanc), **exposition auto** `CameraAttributesPractical` (scale ≈ 0.22), **ReflectionProbe par pièce** (box projection, interior, ambiance couleur, se chevauchant).
- Rideaux plissés, lampes émissives + OmniLight à ombres, suspensions SpotLight : ce sont les détails qui « vendent » l'intérieur.

## Performance (mobile = obligatoire)
- `StaticBatcher` : fusionner la géométrie statique par matériau (milliers → dizaines de draw calls). Exclure les objets interactifs par **sous-chaîne** de nom (Godot renomme les doublons `@Nom@123`).
- Compter les triangles des assets AVANT de les instancier (arbres Poly Haven = 1,5–4 M tris) → `gltf-transform simplify` (ratio 0.02–0.3) + `resize 1024` dans `assets/optimized/`, substitution automatique.
- `visibility_range_end` + ombres coupées pour le lointain ; distance d'ombre ≤ 90 m.
- Android : Forward+ aussi sur mobile pour la qualité, 4 presets (Ultra/Élevé/Moyen/Performance), textures `size_limit` 1024 (archi) / 512 (normales et ORM des props).

## Gameplay
- Joueur `CharacterBody3D` + `SpringArm3D` (exclure son RID) + focus d'interaction = objet le plus proche devant (groupe « interactable »).
- **Piège GDScript** : les lambdas capturent les locales **par valeur** → passer l'objet en paramètre du callback (`func(p, it)`), utiliser des Dictionary/Array (références) pour l'état.
- Portes = nœud pivot sur la charnière + Tween ; recréer un pivot si le modèle n'en a pas (`reparent(hinge, true)`).
- Navmesh à l'exécution : `NavigationServer3D.parse_source_geometry_data` + `bake_from_source_geometry_data` sur les collisions statiques (cell 0.08, climb 0.25).
- Reciblage Mixamo → Mixamo : mêmes noms d'os ; garder seulement les pistes de rotation et convertir `q_t = rest_t * (rest_s⁻¹ * q_s)` (sinon personnage à l'envers).

## Boucle qualité
Mode `--capture` (caméras fixes) + mode `--playtest` (script qui pilote le joueur, affiche PASS/FAIL, prend des captures). Après chaque série : noter /10, corriger les 3 pires défauts, recommencer. Voir `visual-qa-loop`.
