---
name: preview-avant-build
description: À utiliser AVANT de coder un jeu ou une appli Super Essence Studio. Produit une fiche concept courte, un comparatif de jeux de référence et une maquette visuelle légère pour validation. Aucun code de gameplay tant que l'utilisateur n'a pas dit oui.
---

# Preview avant build

Objectif : faire juger une idée pour très peu de tokens, avant le vrai développement.

## Règle absolue
Ne jamais écrire le code complet d'un jeu ou d'une appli sans un OUI clair de l'utilisateur sur la fiche et la maquette.

## Étape 1. Fiche concept (une page maximum)
- Pitch en une phrase.
- Boucle de jeu ou d'usage : 30 secondes, 5 minutes, 1 semaine.
- Public et plateforme (Android, navigateur, les deux).
- Ce qui rend le projet différent des jeux ou applis existants.
- Direction artistique : 5 couleurs (codes hex), forme dominante, ambiance, mascotte ou symbole.
- Lore en 5 lignes, original, sans nom générique.
- Risques techniques sur mobile (poids, chauffe, réseau, permissions).

## Étape 2. Comparatif (tableau)
Choisir 3 références et remplir : Référence | Ce qu'on étudie | Ce qu'on garde | Ce qu'on évite.
Regarder de vraies sources (pages officielles, vidéos de gameplay, critiques) avant de citer un jeu. On s'inspire des mécaniques et de la lisibilité, on ne copie ni le lore, ni les noms, ni les dessins.

## Étape 3. Maquette (un seul fichier HTML, moins de 300 lignes)
- 3 écrans statiques : écran titre, écran principal, menu ou boutique.
- Formes CSS ou SVG simples, aucune logique de jeu.
- Format portrait mobile 390x844, zones sûres respectées.
- Utiliser la palette et la police de la fiche, jamais de dégradé violet-bleu générique.

## Étape 4. Validation
Poser une seule question : « On garde, on modifie ou on jette ? »
Si modification : refaire uniquement ce qui change. Si jeter : s'arrêter, rien n'est perdu.

## Règle tokens
Fiche, comparatif et maquette doivent coûter environ 5 % d'un build complet. Pas de recherche longue ni de long texte à cette étape.

## Après le OUI
Appliquer le skill splash-super-essence pour l'écran de démarrage, puis construire par morceaux et appliquer le skill qualite-jeu-mobile à chaque passe.
