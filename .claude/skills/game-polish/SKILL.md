---
name: game-polish
description: Méthode et checklist pour créer ou améliorer un jeu vidéo de qualité pro (game feel, direction artistique, audio, UI, équilibrage, perf). Utiliser dès qu'on crée, prototype, améliore, "polish" ou corrige un jeu (web HTML5/Canvas, Android Compose, Godot…), ou quand l'utilisateur colle un prompt de jeu (ex. venant de ChatGPT).
---

# Game Polish

Objectif : livrer un jeu qui *se sent* pro, pas un prototype brut. Toujours original : pas de recréation d'un jeu existant (nom, perso, art, niveaux).

## 1. Si l'utilisateur colle un prompt externe (ChatGPT, etc.)
1. Le reformuler en mini game design doc (section 2). Garder ses idées, combler les trous.
2. Signaler en 3 lignes max ce qui est flou ou irréaliste, choisir un défaut sensé et avancer (ne pas bloquer en questions).

## 2. Mini GDD (avant de coder, ≤ 25 lignes)
- Pitch en 1 phrase + fantasme du joueur.
- Boucle 30 s / boucle session 5 min / progression long terme.
- 3 mécaniques max, chacune avec son moment « wow ».
- Courbe de difficulté : apprentissage → maîtrise → défi.
- Direction artistique : palette 5-6 couleurs (hex), style, ambiance, 1 référence de *style* (pas de jeu à copier).
- Plateforme + contrôles (clavier ET tactile si web).

## 3. Architecture
- Boucle fixe `update(dt)` / `render()` découplée, `dt` plafonné.
- Machine d'états : `title → playing → paused → gameover`.
- Constantes de gameplay regroupées dans un objet `TUNING` (facile à équilibrer).
- Web : un seul `index.html` autonome si possible ; sons générés (WebAudio) ou assets libres de droits.

## 4. Checklist qualité (tout cocher avant de livrer)
**Game feel / juice**
- [ ] Chaque action du joueur → retour visuel + sonore immédiat
- [ ] Screen shake dosé, hit-stop (30-80 ms) sur impacts
- [ ] Particules (impact, mort, collecte, poussière de course)
- [ ] Squash & stretch, easing (jamais de mouvement linéaire brut)
- [ ] Texte flottant de score/combo, flash blanc au hit
- [ ] Coyote time + input buffering si plateforme/action

**Visuel**
- [ ] Palette cohérente, contraste lisible, fond avec profondeur (parallaxe/dégradé/vignette)
- [ ] Le joueur et les dangers se lisent en 0,5 s
- [ ] Transitions entre écrans (fade/slide), pas de coupure sèche

**Audio**
- [ ] SFX pour chaque interaction, légère variation de pitch aléatoire
- [ ] Musique/ambiance en boucle, bouton mute, volumes équilibrés

**UI / UX**
- [ ] Écran titre soigné, pause, game over avec « rejouer » en 1 input
- [ ] Meilleur score sauvegardé (localStorage en try/catch)
- [ ] Tutoriel intégré au jeu (pas de pavé de texte)
- [ ] Responsive mobile, zones tactiles ≥ 44 px

**Équilibrage & tech**
- [ ] Première mort après ~30-60 s pour un débutant, pas avant
- [ ] Variété : nouveau élément toutes les 30-60 s
- [ ] 60 FPS stables, pas d'allocation dans la boucle chaude (pool d'objets)
- [ ] Aucune erreur console

## 5. Boucle d'itération (obligatoire)
1. Construire un prototype jouable minimal.
2. Le lancer et le tester réellement (Playwright pour le web : simuler des inputs, captures à t=0, 5 s, 20 s, game over).
3. Autocritique honnête, note /10 : **fun, visuel, feel, clarté**. Lister les 3 pires défauts.
4. Corriger ces 3 défauts. Recommencer.
5. Minimum 3 itérations ; livrer seulement si chaque note ≥ 8.

## 6. Livraison
- Résumé court : ce qui a été fait, notes finales, limites connues.
- Proposer 3 idées d'amélioration suivantes, classées par impact.

## Économie de tokens
- Lire seulement les fichiers/sections nécessaires ; préférer `grep` ciblé.
- Captures d'écran à deviceScaleFactor 1 pour l'autocritique.
- Modifier par `Edit` ciblés plutôt que réécrire tout le fichier.
- Réponses concises : pas de récap du code déjà visible.
