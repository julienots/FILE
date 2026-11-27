---
name: long-build
description: Discipline pour les gros chantiers autonomes (jeu complet, app multi-écrans, migration) : découpage en modules testables, tâches longues en arrière-plan, commits fréquents, rapports courts et honnêtes, économie de tokens. Utiliser quand une demande prendra plus d'une heure ou des dizaines d'étapes, ou quand l'utilisateur dit « continue », « fais tout », « termine ».
---

# Gros chantier autonome

1. **Plan en modules livrables**, chacun testable seul (ex. joueur → interactions → HUD → IA → build). Liste de tâches à jour ; un seul module « en cours ».
2. **Vérifier l'environnement d'abord** (GPU ? réseau ? SDK ?) avec des sondes rapides ; choisir la techno la plus capable *réellement disponible* et le dire.
3. **Longues opérations en arrière-plan** (rendus, imports, téléchargements, builds) + surveillance qui émet seulement succès/échec/étapes clés ; travailler sur autre chose pendant ce temps ; ne jamais lancer deux rendus CPU en parallèle.
4. **Test automatisé par module** (script PASS/FAIL + captures). Un échec = lire la cause exacte avant de corriger ; une « anomalie » peut être un artefact du test (ex. rendu logiciel lent).
5. **Commit + push après chaque étape stable**, messages en français, assets lourds hors Git mais scripts pour les régénérer.
6. **Actions externes** (workflow CI, publication, envoi) : proposer et attendre l'accord ; si refusé, laisser une version inactive et expliquer.
7. **Rapport** : quelques phrases + tableau ; ce qui marche (vérifié), ce qui ne marche pas, ce qui bloque et ce qu'il faut de l'utilisateur. Pas de récit d'étapes.
8. **Tokens** : lire des extraits ciblés, éditions minimales, ne pas réafficher le code, journaux filtrés (`grep`), vignettes réduites pour juger les images.
