# Serveur Minecraft Bedrock avec addons

Pensé pour : 1 serveur, 1 à 3 joueurs, ~30 addons légers. Il faut ~3 Go de RAM libres et 1-2 cœurs.

## Démarrer
1. `docker compose up -d` puis attends « Server started » (`docker compose logs -f`).
2. Mets tes fichiers `.mcaddon` / `.mcpack` / `.zip` dans le dossier `addons/`.
3. `python3 install-addons.py` (ajoute le nom du monde si tu l'as changé), puis `docker compose restart`.
4. Dans Minecraft Bedrock : Jouer → Serveurs → Ajouter un serveur → adresse de la machine, port **19132**.

## Ouvrir à des amis
Redirige le port **UDP 19132** de ta box vers la machine (ce n'est pas du TCP).

## Limites à connaître
- Les consoles (Xbox, PlayStation, Switch) ne peuvent pas ajouter de serveur externe sans astuce ; PC, mobile et tablette oui.
- Les joueurs téléchargent les packs en se connectant : garde la taille totale raisonnable.
- Certains addons à scripts demandent d'activer les « fonctionnalités expérimentales » dans les options du monde.
- Les addons du Marketplace payant ne marchent pas sur un serveur dédié.
- Ordre/conflits : si deux addons modifient la même entité, un seul gagne.

## Sauvegarde
Arrête le serveur et copie `data/`.
