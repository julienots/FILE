---
name: splash-super-essence
description: À appliquer à CHAQUE appli ou jeu. Ajoute l'intro Super Essence Studio (logo officiel), puis l'écran de chargement propre au projet, et fixe l'identité visuelle et le lore du studio pour que rien ne ressemble à du contenu généré par IA.
---

# Super Essence Studio : démarrage et identité

## 1. Le logo officiel (ne jamais le redessiner)
- Texte « SuperEssence. » : « Super » en blanc #FEFEFE, « Essence. » en jaune #FCCC24, police arrondie très épaisse, ombre portée gris foncé décalée vers le bas à droite.
- Fond de l'intro : noir pur #000000.
- Fichiers dans `assets/` de ce skill :
  - `super-essence-logo.png` : logo complet détouré (1021x216, fond transparent).
  - `super-essence-mot.png` et `super-essence-point.png` : le mot et le point séparés (720x152, même cadrage) pour animer le point à part.
  - `demarrage-template.html` : intro + écran de chargement de référence, avec `WORD_SRC` et `DOT_SRC` à remplacer par les images (chemin ou data URI).
  - `demo-demarrage-super-essence.html` : la même chose, prête à ouvrir.
- Copier ces fichiers dans le projet. Ne jamais charger le logo depuis internet, ne jamais changer ses couleurs, ne jamais l'étirer.

## 2. Séquence de démarrage obligatoire (environ 2,4 s)
1. 0,15 s : le mot « SuperEssence » apparaît avec un rebond (échelle 0,62 → 1, courbe à dépassement).
2. 0,72 s : le point jaune tombe d'en haut et rebondit, un anneau jaune s'élargit et disparaît.
3. 1,05 s : un reflet blanc balaie le mot (masqué par la forme du logo).
4. 1,1 s : « STUDIO » en petites capitales blanches à 72 % d'opacité, lettres espacées, apparaît sous le logo.
5. 2,35 s : fondu au noir 0,35 s, puis écran de chargement du projet.
- Un toucher passe l'intro uniquement après un premier visionnage complet (`localStorage` dans un try/catch).
- Respecter `prefers-reduced-motion` : simple fondu, sans rebond ni reflet.
- Attendre que les images soient décodées avant de lancer l'animation, pour éviter un saut.

## 3. Écran de chargement du projet
- Logo, palette, typographie et emblème PROPRES au projet. Jamais le noir, le blanc et le jaune du studio comme couleurs principales.
- Vraie barre de progression reliée au chargeur de ressources (`setProgress(0..100)`).
- Une phrase du lore du projet qui change pendant l'attente.
- Bouton « Toucher pour jouer » d'au moins 48 px de haut quand tout est chargé.
- Zones sûres respectées (`env(safe-area-inset-*)`).

## 4. Implémentation
- HTML seul : reprendre `demarrage-template.html`.
- Phaser : une scène `SplashScene` avant `BootScene`, qui reproduit le même minutage avec des tweens (mot, point, anneau, texte), puis `this.scene.start('Boot')`.
- Android Kotlin : l'API SplashScreen (androidx.core:core-splashscreen) pour le lancement système sur fond noir, puis un écran Compose qui rejoue la séquence du point 2, puis l'écran de chargement du projet.

## 5. Lore du studio (à garder court)
Avant les mondes, il y avait la Veilleuse, une petite flamme qui se souvenait de tout. Chaque goutte d'Essence qui tombe de sa mèche devient un monde. Chaque jeu est une de ces gouttes : un monde fini, avec ses règles et son caractère. Le point jaune du logo est cette goutte. Chaque projet écrit ensuite SON propre lore, sans personnage ni nom existants.

## 6. Style commun aux projets
- Formes rondes et lisibles de loin, contours épais de deux tons, ombres à trois paliers.
- Lumière chaude, ciel qui varie avec l'heure, brume au loin.
- Boutons en relief avec état appuyé, petit rebond et son à chaque appui.
- Deux polices maximum : titre arrondi épais, texte très lisible.
- Par jeu : 5 couleurs maximum, un motif récurrent, un symbole ou une mascotte à lui.
- Icônes dessinées dans le style du jeu, jamais d'emoji dans l'interface.

## 7. Interdits (look « fait par une IA »)
- Dégradés violet-bleu, verre flou partout, cartes blanches arrondies identiques.
- Textes de remplissage vagues, noms génériques répétés (Aether, Nova, Lumina).
- Écrans sans hiérarchie, tout au même poids visuel.
- Illustrations de styles de trait différents dans un même jeu.

Inspirations autorisées : l'esprit de Breath of the Wild, Animal Crossing, Supercell. Jamais de copie d'un personnage, d'un logo ou d'un dessin existant.
