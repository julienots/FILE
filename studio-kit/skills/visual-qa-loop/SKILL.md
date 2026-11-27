---
name: visual-qa-loop
description: Boucle de contrôle qualité visuel par captures réelles (jeu Godot, site web, app Android) avec autocritique notée et corrections ciblées, jusqu'à un niveau pro. Utiliser après chaque modification visible, avant de montrer un résultat à l'utilisateur, ou quand il demande « peaufine », « améliore », « c'est moche », « fais mieux ».
---

# Boucle QA visuelle

## 1. Capturer le vrai rendu (jamais de maquette présentée comme réelle)
- **Godot** : mode `--capture` (liste de caméras nommées, attente de convergence GI, PNG) + `--shot=1,3` pour ne refaire que certaines vues. Mode `--playtest` pour l'UI en situation.
- **Web** : Playwright (`chromium.launch()`), captures 390×844 et 1440×900, clair + sombre.
- **Android Compose** : tests Robolectric de captures ou `adb exec-out screencap`.
- Planche-contact (PIL) pour juger l'ensemble d'un coup d'œil.

## 2. Critiquer comme un directeur artistique exigeant
Pour chaque image : note /10 + 3 pires défauts, du plus visible au moins visible. Grille :
- **Lumière** : exposition (ni brûlé ni bouché), contraste, ombres crédibles, sources motivées.
- **Matières** : rien d'uni plat sans micro-relief, pas de répétition visible, échelle réelle.
- **Composition** : cadrage, premier plan, sujet lisible, pas d'objet coupé ou flottant.
- **Cohérence** : palette, style, rien d'incongru (objet hors thème, à l'envers, géant).
- **Interface** : hiérarchie, alignements, icônes centrées, textes lisibles sur le fond.

## 3. Corriger la cause, mesurer, recommencer
- Diagnostiquer avant de corriger : script qui liste la géométrie dans une zone, qui imprime positions/vitesses par frame, qui mesure une AABB. Une hypothèse = une vérification.
- Changer peu à la fois ; si une correction dégrade (ex. faux-plafond qui assombrit), **l'annuler** et le dire.
- Arrêter une piste après 2 essais infructueux, consigner le défaut comme limite connue.

## 4. Montrer honnêtement
Tableau avant/après des notes, ce qui a été corrigé, ce qui reste (avec la raison). Publier les images dans une page (Artifact) plutôt que de les décrire.
