# Réseau et multijoueur (`agr::net`, `agr.net`)

Le module `agr::net` fournit des **sessions UDP** pour les jeux multijoueurs en réseau local ou sur
Internet (si le port de l'hôte est joignable) : un joueur **héberge**, les autres **rejoignent**.

- Poignée de main, pings de maintien et délai d'expiration (5 s sans nouvelles : joueur déconnecté).
- Messages **non fiables** (positions envoyées à chaque image : un message perdu est remplacé par le
  suivant) et **fiables et ordonnés** (scores, événements : renvoyés toutes les 0,2 s jusqu'à
  l'accusé de réception, livrés une seule fois et dans l'ordre).
- L'hôte est le joueur 1, les clients reçoivent 2, 3… Les clients ne parlent qu'à l'hôte : le script de
  l'hôte décide de l'état du jeu et le diffuse (modèle « hôte autoritaire »).
- Architecture extensible : le transport est une interface (`NetTransport`) ; l'UDP est
  l'implémentation fournie, un serveur relais ou un service de plateforme peut en ajouter une autre
  sans changer les jeux. Les tests utilisent un transport en mémoire qui perd et désordonne 30 % des
  paquets.

## Depuis Lua

| Fonction | Rôle |
|---|---|
| `agr.net.host(port [, joueursMax])` | héberge (port 0 = n'importe lequel, voir `agr.net.port()`) ; `true` ou `nil, message` |
| `agr.net.join(adresse, port)` | rejoint (`"192.168.1.20"`, `"localhost"`, nom d'hôte) ; événement `connected` une fois accepté |
| `agr.net.leave()` | quitte (les autres reçoivent `disconnected`) |
| `agr.net.role()` → `"none"`, `"host"`, `"client"` ; `agr.net.id()` ; `agr.net.peers()` ; `agr.net.port()` | état |
| `agr.net.send(joueur, valeur [, fiable])` | envoie une table (nombres, textes, booléens, tables imbriquées ; 1200 octets en JSON au plus) |
| `agr.net.broadcast(valeur [, fiable])` | hôte : à tous les joueurs ; client : à l'hôte |
| `agr.net.poll()` | événements reçus depuis le dernier appel : `{type = "connected" \| "disconnected" \| "message", peer = id, data = valeur}` |

```lua
function Game:onStart()
    if agr.save.get("role", "host") == "host" then agr.net.host(7777, 4) else agr.net.join("192.168.1.20", 7777) end
end
function Game:onUpdate(dt)
    for _, e in ipairs(agr.net.poll()) do
        if e.type == "message" and e.data.kind == "move" then self:moveRemote(e.peer, e.data.x, e.data.y) end
    end
    agr.net.broadcast({ kind = "move", x = self.x, y = self.y })  -- non fiable : chaque image
end
```

Dans l'éditeur, arrêter le mode Jeu ferme la session. Sur Android, cocher **Réseau / multijoueur**
dans l'export (permission `INTERNET`).

Le modèle de jeu **Multijoueur (arène)** montre une partie locale à 2 joueurs sur un clavier, ainsi
que l'hébergement ou la connexion en réseau avec synchronisation par l'hôte.

## Limites actuelles

- IPv4 uniquement ; pas de traversée de NAT ni de serveur relais fourni (sur Internet, l'hôte doit
  ouvrir son port UDP) ; pas de découverte automatique des parties du réseau local.
- Pas de chiffrement ni d'authentification : ne pas transmettre de données sensibles.
- Pas de prédiction / réconciliation côté client ni d'interpolation intégrées : le jeu les écrit en
  Lua s'il en a besoin (le modèle Multijoueur lisse les positions reçues).
- Messages de 1200 octets au plus (pas de fragmentation).
