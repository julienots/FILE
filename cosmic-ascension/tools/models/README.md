# Allègement des modèles Tripo (60 Mo → ~150 Ko)

Les GLB Tripo bruts font ~2 M de triangles avec des textures 8K: inutilisables sur mobile et refusés par l'envoi web de GitHub (25 Mo).

Prérequis (hors dépôt, dans un dossier de travail): `npm i @gltf-transform/cli @gltf-transform/core @gltf-transform/extensions @gltf-transform/functions meshoptimizer sharp`

1. **Cas normal** (maillage propre — a marché pour ninja, alien vert, vaisseau):
   `gltf-transform optimize brut.glb sortie.glb --simplify true --simplify-ratio 0.008 --simplify-error 0.03 --texture-size 1024 --texture-compress webp --compress meshopt`
   Vérifier le nombre de triangles avec `gltf-transform inspect`. S'il reste > 40k, passer au cas 2.
2. **Maillage fragmenté** (îlots UV minuscules: la réduction se bloque ou la texture se craquelle):
   `node bake-vertex-colors.mjs brut.glb tmp.glb 24000` puis `gltf-transform optimize tmp.glb sortie.glb --simplify false --compress meshopt`
   → cuit la texture dans les couleurs de sommets, regroupe les sommets; sortie sans texture.
3. Copier dans `public/models/<clé>.glb`, déclarer dans `manifest.json` (`height`, `rotY` si l'avant n'est pas +Z).
4. Contrôler: `node tests/models.mjs <sous-dossier de public/models> tests/out/models.png` (planche d'aperçu), puis `npm run smoke` et regarder `tests/out/heroes.png`, `enemies.png`, `ship.png`.

Budgets visés: héros ≤ 28k triangles, ennemis ≤ 12k, vaisseau ≤ 15k.
