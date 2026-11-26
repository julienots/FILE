---
name: debug-method
description: Méthode rigoureuse pour trouver et corriger un bug sans tourner en rond ni gaspiller de tokens. Utiliser quand quelque chose ne marche pas, plante, affiche une erreur, ou quand une correction précédente n'a pas suffi.
---

# Debug method

1. **Reproduire** : obtenir l'erreur exacte (message, stack, étapes). Pas de correction sans reproduction, sauf impossibilité justifiée.
2. **Localiser** : grep ciblé sur le message / la fonction ; lire seulement les lignes concernées.
3. **Hypothèses** : lister 1-3 causes plausibles, la plus probable d'abord. Vérifier chacune par un test rapide (log, assertion, test unitaire), pas par intuition.
4. **Corriger la cause**, pas le symptôme. Changement minimal.
5. **Prouver** : relancer la reproduction + les tests existants. Ajouter un test qui aurait attrapé le bug si c'est simple.
6. **Rapporter** en 3 lignes : cause, correctif, preuve.

## Règles
- 2 tentatives ratées sur la même piste → s'arrêter, relire l'erreur, changer d'hypothèse.
- Ne jamais désactiver un test, avaler une exception ou ajouter un `try/catch` vide pour « faire passer ».
- Retirer les logs de debug avant de terminer.
