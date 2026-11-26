# THE PERFECT LIFE — Étape A : analyse et prévisualisation

## 1. Projet existant
- Le dépôt contient NOCTÉA (appli Android Kotlin/Compose, audio). Aucun moteur 3D : le jeu est un projet séparé dans `the-perfect-life/`.

## 2. Moteur choisi : Godot 4.5 (Vulkan)
| Critère | Godot 4.5 | Unity / Unreal |
|---|---|---|
| Accessible dans cet environnement (téléchargement, licence, build) | ✅ | ❌ (comptes, licences, installeurs bloqués) |
| Export Android APK natif | ✅ | ✅ |
| PBR, ombres en cascades, SDFGI, SSR, SSAO, SSIL, brouillard volumétrique, ACES | ✅ (Forward+) | ✅ |
| Rendu en ligne de commande pour captures réelles | ✅ | difficile |
| Libre, sans redevance | ✅ (MIT) | ❌ |

Deux pipelines selon le téléphone :
- **Ultra / Élevé** : renderer Forward+ (Vulkan) — GI dynamique SDFGI, SSR, SSIL. Téléphones haut de gamme récents.
- **Moyen / Performance** : renderer Mobile — GI précalculée (LightmapGI) + sondes de réflexion, résolution dynamique. Même direction artistique.

## 3. Direction artistique
« Banlieue américaine idéale, trop parfaite » : lumière dorée du matin, ciel HDRI réel, pelouses impeccables, maisons coloniales en bardage clair, intérieurs chaleureux (bois, marbre, laiton, lin). La perfection *légèrement* excessive est l'outil narratif : plus tard, les répétitions et anomalies se verront justement parce que tout est trop propre.

## 4. Assets
- **Poly Haven (CC0)** : ~60 meubles et objets scannés photoréalistes, 21 matériaux PBR 2K, ciels HDRI. Script : `tools/fetch_assets.py`.
- **Ta bibliothèque Drive** : voitures (supercar bleue, sportive grise, sportive blanche), arbre réaliste. Le reste (médiéval/fantasy) ne correspond pas au thème.
- **Sur mesure (code)** : maison complète (murs, ouvertures, fenêtres à petits bois, volets, porte d'entrée moulurée, escalier, toiture, gouttières, perron, garage), cuisine shaker + marbre, lit capitonné, dressing, TV, piscine (shader d'eau), rue.

## 5. Optimisation mobile déjà en place
- Fusion de la géométrie statique par matériau (`StaticBatcher`) : quelques dizaines de draw calls par maison au lieu de milliers.
- Textures 2K compressées (ETC2/ASTC pour Android), maillages des voitures/arbres simplifiés (-70 % de triangles).
- 4 niveaux de qualité (`scripts/core/quality.gd`).

## 6. Limites connues (honnêteté)
- **Personnages** : aucune source libre de vrais humains modernes réalistes riggés n'est accessible ici. La femme utilise un modèle de démonstration (Mixamo via three.js). Le protagoniste reste à produire → recommandation : générer 2-3 personnages modernes dans Tripo (tu l'utilises déjà) puis les rigger (Mixamo/AccuRig).
- Les captures sont rendues par un GPU logiciel (pas de carte graphique dans l'environnement cloud) : rendu identique, mais lent à produire.
- Interactions, IA, HUD, téléphone, véhicules conduisibles, audio : Étape B, après validation.
