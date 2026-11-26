---
name: ui-polish
description: Rendre une interface (appli mobile, site web, écran de jeu, dashboard) belle et pro, au-delà du look « fait par une IA ». Utiliser quand on crée ou améliore une UI, quand l'utilisateur dit « c'est moche », « rends-le plus pro / plus beau », ou avant de livrer un écran.
---

# UI polish

## Méthode
1. Capturer l'écran réel (Playwright pour le web, test screenshot pour Android) à taille mobile ET desktop.
2. Noter /10 : hiérarchie, espacement, couleur, typo, cohérence, sensation (animations).
3. Corriger les 3 pires points, recapturer, comparer. 2 tours minimum.

## Checklist
- **Hiérarchie** : 1 action principale évidente par écran ; titres nettement plus gros que le texte.
- **Espacement** : grille de 4/8 px, marges généreuses, alignements stricts. Le vide est un outil.
- **Couleur** : 1 couleur d'accent, neutres teintés (pas de gris pur), contraste texte ≥ 4.5:1, mode sombre soigné.
- **Typo** : 2 polices max, 3-4 tailles max, interligne 1.4-1.6 pour le texte.
- **Détails pro** : coins arrondis cohérents, ombres douces et rares, icônes d'un même jeu, états vides/chargement/erreur dessinés.
- **Mouvement** : transitions 150-300 ms avec easing, retour visuel au toucher, pas d'animation gratuite.
- **Mobile** : zones tactiles ≥ 44-48 px, pas de scroll horizontal, pouce-friendly.
- **Accessibilité** : labels, tailles de texte système respectées, pas d'info portée par la couleur seule.

## À éviter (look IA générique)
Dégradés violet-bleu partout, emojis en guise d'icônes, cartes identiques empilées, texte centré partout, ombres lourdes, 6 couleurs d'accent.
