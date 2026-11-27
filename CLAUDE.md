# Consignes

- Langue : français. Réponses courtes et directes, pas de récapitulatif inutile.
- Jeux : toujours suivre le skill `game-polish` (`claude-kit/skills/game-polish/SKILL.md`).
- Skills perso (claude-kit, voir `claude-kit/README.md`) : `conception`, `prompt-upgrade`, `long-build`, `game-polish`, `godot-realistic-3d`, `asset-sourcing`, `visual-qa-loop`, `ui-polish`, `debug-method`, `android-release`, `ship-check`.
- Prompts prêts à l'emploi : `docs/PROMPTS_JEU.md`.

## Économie de tokens
- Ne lire que les fichiers et lignes utiles (grep/sed ciblés), jamais tout le dépôt.
- Éditions ciblées (Edit) plutôt que réécriture complète.
- Pas de sous-agents sauf demande explicite.
- Ne pas ré-afficher le code déjà écrit ; résumer en quelques lignes.
- Ignorer `app/src/main/assets/voice/` (milliers de fichiers audio) et `release/`.

## Dépôt
- `the-perfect-life/` : jeu Godot 4.5 (voir `the-perfect-life/docs/`).
- `app/` : NOCTÉA, appli Android Kotlin/Compose (méditation/sommeil). Tests : `./gradlew test`.
