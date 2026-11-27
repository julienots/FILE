# claude-kit

Skills pour qu'un agent Claude conçoive et livre des **apps et jeux de niveau pro**, avec une méthode testée sur un vrai projet (jeu 3D réaliste Android sous Godot, appli Android Compose).

## Installer (Claude Code)

```
/plugin marketplace add julienots/file
/plugin install claude-kit@julien-kit
```

> Le dépôt doit être accessible au Claude qui installe (dépôt public, ou accès GitHub accordé).
> Sans plugin : copier `claude-kit/skills/*` dans `~/.claude/skills/` (tous projets) ou `.claude/skills/` (un projet).

## Les 11 skills

| Skill | Rôle |
|---|---|
| `conception` | Idée → cahier d'une page → plan par jalons livrables |
| `prompt-upgrade` | Réécrit un prompt externe (ChatGPT…) en spec exécutable |
| `long-build` | Discipline des gros chantiers autonomes : modules, tâches de fond, commits, rapports honnêtes |
| `game-polish` | Game feel, direction artistique, audio, UI, équilibrage, boucle d'itération notée |
| `godot-realistic-3d` | Pipeline Godot 4.5 : monde en code, PBR, éclairage photoréaliste, batching, navmesh, pièges connus |
| `asset-sourcing` | Assets CC0 pro (Poly Haven API, HDRI, polices), dossiers Drive, gltf-transform |
| `visual-qa-loop` | Captures réelles → critique /10 → corrections ciblées → avant/après |
| `ui-polish` | Interfaces belles et pro, anti-« look IA » |
| `debug-method` | Reproduire, mesurer, corriger la cause, prouver |
| `android-release` | APK/AAB depuis un conteneur : SDK, signature, budget de taille, distribution |
| `ship-check` | Vérification complète avant publication |

## Plugins recommandés (Anthropic Directory)

| Plugin | Pourquoi |
|---|---|
| **Token Shield** | Réduit la consommation de tokens, audit et conseils |
| **Context Compactor** | Garde les longues sessions rapides (`/compact` ciblé) |
| **Design** (Anthropic) | Critique de design, accessibilité, UX writing |

Dans Claude Code : `/plugin` → onglet Discover → rechercher le nom.

## Prompt de démarrage pour un autre Claude

```
Tu disposes du plugin claude-kit. Pour toute demande d'app ou de jeu :
1. prompt-upgrade si la demande vient d'ailleurs, sinon conception ;
2. long-build pour organiser le travail (modules testables, commits, rapports courts) ;
3. pour un jeu 3D : godot-realistic-3d + asset-sourcing ; pour une UI : ui-polish ;
4. après chaque étape visible : visual-qa-loop (captures réelles, note /10, 3 défauts, corriger) ;
5. avant de livrer : android-release si mobile, puis ship-check.
Vérifie toujours l'environnement (GPU, réseau, SDK) avant de choisir la techno.
Ne présente jamais une maquette comme un rendu réel. Signale honnêtement ce qui manque.
Réponds en français, de façon concise.
```

## Licence des contenus
Textes des skills : libres de réutilisation par l'auteur du dépôt. Les assets cités ont leurs propres licences (voir `asset-sourcing`).
