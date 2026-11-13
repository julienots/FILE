# Serveur Minecraft (Docker)

## Prérequis
- Docker + Docker Compose, 2 Go de RAM libres minimum.

## Démarrer
1. Mets ton pseudo dans `OPS=` du fichier `.env` (tu seras admin).
2. `docker compose up -d`
3. Suis le démarrage : `docker compose logs -f` (attends « Done »).
4. Dans Minecraft : Multijoueur → Ajouter un serveur → `localhost` (ou l'IP de la machine).

## Commandes utiles
- Console du serveur : `docker compose exec minecraft rcon-cli`
- Arrêter : `docker compose down` — Mettre à jour : `docker compose pull && docker compose up -d`
- Sauvegarde : copie le dossier `data/` (serveur arrêté).

## Jouer avec des amis (internet)
- Sur ta box : redirige le port TCP **25565** vers la machine qui héberge.
- Donne ton IP publique (ou un DynDNS) à tes amis.
- Active `ENABLE_WHITELIST=TRUE` pour limiter l'accès, et garde `ONLINE_MODE=TRUE`.

## Changer de type
`TYPE` : `PAPER` (plugins, performant), `VANILLA`, `FABRIC` / `FORGE` (mods).
`MEMORY` : `4G` ou plus si beaucoup de joueurs/mods.
