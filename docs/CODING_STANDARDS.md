# Règles de code

- C++20, formaté avec `.clang-format` (4 espaces, 120 colonnes).
- Espaces de noms : `agr`, `agr::math`, `agr::ecs`. Types en `PascalCase`, fonctions et variables
  en `camelCase`, membres `m_`, constantes `kName`, macros `AGR_`.
- En-têtes publics dans `engine/<module>/include/agr/<module>/`, implémentation dans `src/`.
- **Aucun avertissement** : la CI compile avec `AGR_WARNINGS_AS_ERRORS=ON` sur GCC et Clang.
- Pas d'exceptions dans le moteur ; erreurs récupérables via `Result<T>` / `Status`,
  invariants via `AGR_ASSERT` (debug) et `AGR_VERIFY` (toujours actif, arrêt contrôlé).
- Pas d'allocation dans les boucles par image ; préférer `LinearAllocator` / `FrameAllocator`.
- Toute fonctionnalité exposée a des tests. Un bug corrigé reçoit un test qui l'aurait détecté.
- Une fonctionnalité n'est marquée « terminée » dans la feuille de route que si elle est
  compilée, testée et utilisée par un exemple.
- Les commentaires de code sont en anglais ; la documentation utilisateur est en français.
