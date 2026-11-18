# Mode relais (recommandé) — rien à héberger

Le hub tourne **sans serveur** : Claude fait lui-même le relais, comme dans la « variante » du brief.

```
Dépôt                         File d'attente (Supabase)              Pilote (tâche Claude, toutes les heures)
─────                         ─────────────────────────              ────────────────────────────────────────
CUTTINX → Claude  ──────────▶ hub_videos (queued)  ◀───────────────  1. importe les nouveaux partages CUTTINX
Appli « Hub vidéo » (≤50 Mo) ─▶ bucket hub-raw + hub_videos           2. monte dans CUTTINX (style, musique, effets)
                                                                      3. rendu final → galerie Movies/Cuttinx
Appli « Hub vidéo » (auto) ───▶ bucket hub-renders + hub_renders ◀──  4. rattache le MP4 envoyé par l'appli
                                                                      5. écrit la légende + hashtags
                                                                      6. publie le Reel via Windsor.ai (@julien.blt.bs)
```

- **Déposer** : partager les vidéos dans **CUTTINX → Claude** (aucune limite de taille ; note facultative « perf »,
  « normal », « programmé »), ou « Partager → Hub vidéo » / bouton « Choisir des vidéos » dans l'appli (50 Mo max).
- **Montage** : Claude, via le connecteur CUTTINX, avec les guides `style_performance` / `style_normal` stockés dans
  `hub_settings` (modifiables en demandant à Claude dans la conversation).
- **Retour du MP4** : l'appli Android envoie automatiquement chaque rendu de `Movies/Cuttinx` (brouillons 480p ignorés).
- **Publication** : connecteur Windsor.ai déjà relié au compte Instagram (pas d'app Facebook ni de jeton à créer).
  Publication immédiate, ou aux heures d'audience `peak_hours` (12 h, 18 h, 21 h, fuseau Europe/Paris).
- **Suivi** : écran principal de l'appli (statuts, légende, erreurs, lien du Reel).
- **Erreurs** : 3 tentatives par étape, puis statut « Erreur » visible dans l'appli ; rapport après chaque passage.

Consignes complètes du pilote : [`PROMPT.md`](PROMPT.md). Limites : fichiers de 50 Mo maximum côté stockage (Reels
< 45 s), téléphone allumé avec CUTTINX « Connecté à Claude » au moment du passage, un passage par heure.
