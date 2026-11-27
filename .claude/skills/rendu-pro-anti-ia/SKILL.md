---
name: rendu-pro-anti-ia
description: À utiliser dans TOUTE conversation qui produit quelque chose de visuel, de sonore ou de rédigé (site, app, UI, image, vidéo, 3D, jeu, texte, post réseaux) — impose un post-processing soigné et un rendu qui ne ressemble pas à de l'IA. À utiliser OBLIGATOIREMENT pour tout jeu : personnages ultra animés avec mouvements réalistes, et interdiction des jeux de bateau et des jeux cubiques/voxel. Déclencheurs : "fais un jeu", "crée un site", "design", "image", "vidéo", "rendu", "post-process", "animation", "personnage", "ça fait trop IA".
---

# Rendu pro, zéro look IA

Trois règles non négociables, valables pour toute la conversation :

1. **Tout rendu passe par un vrai post-processing** (image, vidéo, 3D, jeu, UI).
2. **Rien ne doit avoir l'air généré par une IA** (visuel comme texte).
3. **Jeux : personnages ultra animés, mouvements réalistes. Pas de bateau, pas de cubique.**

Si une demande contredit une règle, signale-le en une ligne et propose l'alternative la plus proche (ex. jeu de bateau demandé → proposer un autre véhicule ou un autre thème) avant de produire quoi que ce soit.

---

## 1. Post-processing obligatoire

### 3D / jeux (Three.js, Babylon, Unity, Godot, Unreal…)
Chaîne minimale, dans cet ordre :
- **Tone mapping** ACES Filmic (ou AgX), exposition réglée à la main, sortie sRGB.
- **Éclairage** : HDRI/environment map + une lumière clé avec ombres douces (PCF soft / VSM), jamais une seule lumière ambiante plate.
- **SSAO / GTAO** pour ancrer les objets au sol.
- **Bloom** léger, seuil haut (seules les vraies sources lumineuses brillent).
- **Color grading** : LUT ou courbes (ombres légèrement froides, hautes lumières chaudes, saturation retenue).
- **Profondeur de champ** discrète sur les plans cinématiques, **motion blur** par objet si le moteur le permet.
- **Vignette** subtile, **grain de film** fin (animé), **aberration chromatique** très faible sur les bords.
- **Antialiasing** : SMAA/TAA/FXAA en dernier.
- Brouillard atmosphérique / perspective aérienne pour la profondeur.
- Three.js : `EffectComposer` ou la lib `postprocessing` (pmndrs), version épinglée.

### Images
- Étalonnage (courbes, balance des blancs volontaire), pas de saturation poussée au max.
- Grain, légère vignette, micro-imperfections (poussière, flou de bord d'objectif, bruit de capteur).
- Cadrage photographique réel : règle des tiers, horizon pas toujours parfait, avant-plan flou.
- Pas de peau plastique, pas de symétrie parfaite, pas de lumière « partout à la fois ».

### Vidéo
- Étalonnage cohérent sur tous les plans, LUT unique.
- Coupes calées sur le rythme (beats), J-cuts / L-cuts pour le son.
- Grain, légère instabilité caméra crédible (pas de shake aléatoire grossier), motion blur à 180°.
- Mixage son : musique ducking sous la voix, ambiances, pas de silence numérique.

### UI / web
- Ombres en couches (2-3 ombres douces superposées), pas une seule `box-shadow` grise.
- Textures légères (grain SVG à 3-5 % d'opacité), bordures fines, micro-interactions avec easing réel (`cubic-bezier`, ressorts), jamais `linear`.

---

## 2. Anti-look IA

### Visuel — à NE PAS faire
- Dégradés violet/bleu/rose « néon tech », fonds noirs avec halos flous génériques.
- Glassmorphism partout, cartes arrondies identiques en grille 3 colonnes.
- Hero centré « titre énorme + sous-titre + 2 boutons » par défaut.
- Police Inter/Poppins partout sans raison, emojis comme icônes, icônes ✨🚀.
- Illustrations 3D « blob » brillantes, personnages « style Pixar » génériques.
- Tout parfaitement symétrique, tout aligné au centre, tout à la même densité.

### Visuel — à FAIRE
- Une direction artistique choisie et nommée (ex. « print suisse », « éditorial magazine », « industriel brut », « illustration gouache »), tenue partout.
- Palette restreinte et inattendue (2-3 couleurs + neutres), typographie avec caractère (une serif ou grotesque marquée + une mono ou une sans sobre).
- Hiérarchie asymétrique, grilles cassées volontairement, espaces blancs généreux mais inégaux.
- Détails « faits main » : traits légèrement irréguliers, textures papier, photos réelles ou crédibles.

### Texte — à NE PAS faire
- Tirets cadratins (—) en série, listes à puces pour tout, titres « Libérez / Plongez / Révolutionnez ».
- Mots IA : « incontournable », « dans un monde où », « en somme », « il est important de noter », « robuste », « fluide », « sans effort », « booster », « voyage ».
- Structure toujours en 3 points, conclusion qui résume ce qui vient d'être dit.

### Texte — à FAIRE
- Phrases de longueurs variées, ton direct, exemples concrets et chiffrés.
- Une opinion assumée plutôt qu'un équilibre neutre forcé.
- Vocabulaire du domaine, pas du marketing générique.

---

## 3. Jeux : animation des personnages

**Tout personnage (joueur, PNJ, ennemi, animal) doit être ultra animé.** Un personnage qui glisse, qui pivote d'un coup ou qui a 2 frames de marche est un échec.

### Base obligatoire
- **Rig squelettique** (3D : glTF/FBX skinné ; 2D : squelette type Spine/DragonBones ou sprites à 8-12+ frames par cycle). Jamais une capsule, un cube ou un sprite fixe qui translate.
- **Machine à états + blend trees** : idle → walk → jog → run mélangés selon la vitesse réelle (blend 1D/2D), transitions en **crossfade** (0.15-0.3 s), jamais de coupe sèche.
- **Vitesse d'animation synchronisée** au déplacement (root motion ou vitesse calculée) : zéro « pieds qui patinent ».

### Couches de réalisme (toutes requises)
- **IK des pieds** : pieds posés sur le relief, bassin ajusté en pente/escaliers.
- **Look-at / head IK** : la tête et le buste suivent la cible ou la caméra.
- **Inclinaison dans les virages** (lean) et **turn-in-place** quand le perso pivote à l'arrêt.
- **Accélération / décélération** avec anticipation (léger recul avant le départ) et freinage (glissade, poids qui part vers l'avant).
- **Mouvement secondaire procédural** : respiration, cheveux/vêtements/accessoires en spring bones ou physique de tissu, follow-through.
- **Saut complet** : anticipation (flexion) → impulsion → apex → chute → réception avec amorti proportionnel à la hauteur.
- **Réactions** : hit reactions directionnelles, trébuchement, ragdoll partiel ou complet mélangé à l'animation (active ragdoll si possible).
- **Idles variés** : 2-4 variations aléatoires (regarder autour, s'étirer, changer d'appui), clignement des yeux, micro-mouvements.
- **Principes d'animation** : anticipation, overshoot, ease-in/out, arcs naturels, squash & stretch dosé selon le style.
- **Sons liés à l'animation** : pas synchronisés sur les contacts au sol (selon la surface), souffle, froissement.

### Mise en œuvre rapide (web / Three.js)
- Modèle glTF animé (Mixamo, Quaternius, ou fait sur mesure) + `AnimationMixer`, `crossFadeTo`, poids de blend pilotés par la vitesse.
- Couche procédurale par-dessus : rotation des os tête/colonne (look-at), offset des pieds par raycast au sol, oscillation de respiration, inclinaison selon la vitesse angulaire.
- Caméra qui accompagne : léger retard (lerp), FOV qui s'élargit en course, bob de caméra discret.

### Vérification
Avant de livrer un jeu, contrôle visuellement (capture/vidéo Playwright si possible) : démarrage, arrêt, demi-tour, saut, pente, collision. Si un seul de ces moments paraît robotique, corrige avant de livrer.

---

## 4. Thèmes de jeux interdits

- ❌ **Jeux de bateau** : bateaux, navires, voiliers, pirates en mer, courses nautiques, pêche en barque, batailles navales. Le joueur ne pilote pas de bateau et le gameplay ne tourne pas autour d'un bateau.
- ❌ **Jeux cubiques / voxel / blocky** : style Minecraft, mondes en blocs, personnages en cubes, low-poly cubique, pixel-art « blocs ».
- Proposer à la place : personnages organiques, environnements aux formes naturelles (terrain lissé, végétation, architecture détaillée), thèmes terrestres ou aériens.

---

## Checklist avant de livrer

- [ ] Post-processing en place et réglé à la main (pas les valeurs par défaut).
- [ ] Aucun élément de la liste « à NE PAS faire » (visuel et texte).
- [ ] Direction artistique nommée et cohérente.
- [ ] Jeu : rig + blend trees + IK pieds + look-at + lean + secondaire + saut complet + réactions + idles variés.
- [ ] Jeu : pas de bateau, pas de cubes/voxels.
- [ ] Vérifié visuellement (capture ou vidéo), pas seulement « ça compile ».
