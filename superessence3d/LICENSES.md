# Licences des composants

SUPERESSENCE 3D STUDIO n'utilise que des composants gratuits, sans abonnement, sans crédits et sans API payante.

| Composant | Rôle | Licence | Usage commercial |
|---|---|---|---|
| Blender / `bpy` 5.3 (Blender Foundation) | moteur 3D, rendu, exports | GPL-3.0-or-later | Oui. Les modèles, images et fichiers que vous créez vous appartiennent : la GPL ne s'applique pas à vos créations. |
| three.js r170 (embarqué dans `web/vendor/three`) | affichage du viewport | MIT | Oui |
| FastAPI, Starlette | serveur local | MIT / BSD-3-Clause | Oui |
| Uvicorn | serveur HTTP | BSD-3-Clause | Oui |
| websockets | WebSocket | BSD-3-Clause | Oui |
| python-multipart | envoi de fichiers | Apache-2.0 | Oui |
| MCP Python SDK (Anthropic / Model Context Protocol) | serveur MCP | MIT | Oui |
| NumPy | calcul (sculpture, images) | BSD-3-Clause | Oui |
| Pydantic | validation | MIT | Oui |
| uv (Astral) | installation de Python | MIT / Apache-2.0 | Oui |

## Code de SUPERESSENCE 3D STUDIO
Le code de ce dossier importe le module Blender `bpy`, qui est sous licence GPL. Il est donc distribué sous **GPL-3.0-or-later**. Vous pouvez l'utiliser, le modifier et le vendre, à condition de fournir le code source si vous distribuez le logiciel. Encore une fois, **les modèles 3D générés ne sont pas concernés** : ce sont vos créations.

## Moteurs neuronaux facultatifs (non inclus)
Aucun n'est installé. Si vous en ajoutez un :
- **TripoSR** (VAST AI Research et Stability AI) : licence MIT pour le code et les poids publiés.
- D'autres modèles ont des licences restrictives : usage non commercial, plafonds d'utilisateurs, territoires exclus. **Lisez la licence avant tout usage commercial.**

## Claude
Claude est une connexion externe **facultative** (Claude Desktop ou Claude Code via MCP), soumise aux conditions d'Anthropic et à votre abonnement Claude. Le studio et la génération procédurale fonctionnent entièrement sans Claude et sans Internet.
