# Consignes

- Langue : français. Réponses courtes et directes, pas de récapitulatif inutile.
- Jeux : toujours suivre le skill `game-polish` (`claude-kit/skills/game-polish/SKILL.md`).
- Skills perso (claude-kit) : `conception` (nouvelle idée), `prompt-upgrade` (prompt externe), `game-polish` (jeu), `ui-polish` (interface), `debug-method` (bug), `ship-check` (avant publication).
- Prompts prêts à l'emploi : `docs/PROMPTS_JEU.md`.

## Économie de tokens
- Ne lire que les fichiers et lignes utiles (grep/sed ciblés), jamais tout le dépôt.
- Éditions ciblées (Edit) plutôt que réécriture complète.
- Pas de sous-agents sauf demande explicite.
- Ne pas ré-afficher le code déjà écrit ; résumer en quelques lignes.
- Ignorer `app/src/main/assets/voice/` (milliers de fichiers audio) et `release/`.

## Dépôt
- `app/` : NOCTÉA, appli Android Kotlin/Compose (méditation/sommeil). Tests : `./gradlew test`.
