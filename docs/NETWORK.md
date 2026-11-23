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

## Multijoueur de haut niveau : synchronisation, apparitions, propriétaires, RPC

Par-dessus les messages, `net::Replicator` (dans `NetPlugin`) fait le travail des
`MultiplayerSynchronizer` / `MultiplayerSpawner` / RPC de Godot :

- **`NetSync`** (composant) : l'entité est tenue à jour sur toutes les machines — son `Transform`
  (envoyé `rate` fois par seconde, **interpolé** entre les deux derniers états sur les autres
  machines) et des champs listés dans `fields` (`"Health.value;SpriteRenderer.color"` : nombres,
  booléens, vecteurs, couleurs, textes). Une entité est la même partout grâce à son identifiant de
  scène : les scènes se chargent à l'identique, les entités créées en réseau reçoivent le même
  identifiant sur chaque machine.
- **Propriétaire** (`owner`, 1 = l'hôte) : seule la machine propriétaire simule l'entité et envoie son
  état ; l'hôte relaie aux autres clients ce que les clients envoient, et n'accepte l'état d'une entité
  que de son propriétaire. `agr.net.setOwner(entité, joueur)` (hôte) donne par exemple son vaisseau à
  chaque joueur ; `entity:owner()`, `entity:isOwner()`. Quand un joueur part, ses entités reviennent
  à l'hôte.
- **Apparitions** : `agr.net.spawn("assets://prefabs/rocket.agrprefab", x, y, z [, propriétaire])`
  (hôte) crée le préfabriqué sur toutes les machines ; `agr.net.despawn(entité)` le détruit partout.
  Un joueur qui **arrive en cours de partie** reçoit les entités déjà créées et les propriétaires.
- **RPC** : `self.entity:rpc("fire", x, y)` appelle `fire(x, y)` du script de cette entité sur toutes
  les autres machines (par l'hôte), `entity:rpcTo(joueur, "fn", ...)` sur une seule (1 = l'hôte) ;
  fiables et dans l'ordre. **Sécurité** : seules les fonctions déclarées dans la table `rpc` du script
  peuvent être appelées à distance — `"any"` (n'importe quel joueur) ou `"authority"` (le propriétaire
  de l'entité ou l'hôte) ; `",local"` en plus les exécute aussi sur la machine qui appelle (et hors
  ligne). `agr.net.sender()` donne le joueur à l'origine de l'appel en cours.

```lua
local Ship = { rpc = { fire = "authority,local", chat = "any" } }
function Ship:onUpdate(dt)
    if not self.entity:isOwner() then return end   -- les autres machines voient la position synchronisée
    self.entity:translate(agr.input.value("move_x") * 5 * dt, 0, 0)
    if agr.input.pressed("fire") then self.entity:rpc("fire", 1) end
end
function Ship:fire(power) agr.fx.burst(...) end    -- partout, l'appelant compris
```

- **Visibilité** (`NetSync.visibility`, côté hôte) : `All` (tout le monde), `OwnerOnly` (son seul
  propriétaire : main d'un joueur, inventaire), `Range` (les joueurs qui possèdent une entité à moins de
  `visibleRange` : brouillard de guerre, grands mondes). `agr.net.setVisible(entité, joueur, oui/non)`
  force la visibilité pour un joueur. Une entité qui redevient visible est renvoyée aussitôt.
- **Seulement les changements** (`NetSync.onlyChanges`, actif par défaut) : un état identique au
  précédent n'est pas renvoyé (une fois par seconde quand même, contre les pertes) ; une entité immobile
  ne coûte rien. `ReplicationStats::statesSkipped` / `statesHidden`.
- **Mot de passe** : `agr.net.setPassword("...")` avant `host` / `join` ; l'hôte refuse un mot de passe
  différent (`agr.net.joinError()` → `"wrong password"`, ou `"full"`). Vérifié par une empreinte liée à
  chaque tentative de connexion (le mot de passe ne circule pas en clair).
- **Chiffrement** : `agr.net.setEncryption(true)` sur l'hôte, avec un mot de passe (C++ :
  `NetSession::setEncryption`). Tous les paquets après la poignée de main sont chiffrés et authentifiés
  en **AES-256-GCM** (mbedTLS), avec une clé par connexion dérivée du mot de passe et du jeton aléatoire
  de la connexion (PBKDF2-SHA-256, 4096 tours) ; les clients suivent l'hôte (annoncé dans son
  accueil), `agr.net.encrypted([joueur])` le confirme. Un paquet modifié, rejoué (fenêtre de 64
  paquets) ou non chiffré sur un lien chiffré est jeté (`stats().rejected`). Test : messages fiables
  fragmentés et non fiables reçus, aucun texte lisible dans les datagrammes, paquet rejoué et paquet
  modifié rejetés. 24 octets de plus par paquet.
- **Latence et trafic** : `agr.net.ping(joueur)` → aller-retour en millisecondes (lissé),
  `agr.net.stats()` → octets et paquets envoyés / reçus, renvois.
- **Gros messages** : les messages fiables sont découpés et recollés automatiquement (jusqu'à
  256 Ko) ; les non fiables restent limités à 1200 octets (un datagramme).
- **Découverte en réseau local** : l'hôte s'annonce (`agr.net.advertise("Partie de Julien")`) ; un
  joueur cherche avec `agr.net.discover(7777)` puis lit `agr.net.games()` →
  `{ {address, port, name, players, max}, ... }` (diffusion UDP ; une partie qui ne répond plus
  depuis 5 s disparaît de la liste), `agr.net.stopDiscovery()`.
- Le modèle **Multijoueur (arène)** utilise un RPC : les effets d'une pièce ramassée (son, particules,
  « +1 ») sont joués sur toutes les machines.

Tests (`tests/test_net.cpp`, `tests/test_script.cpp`) : 3 machines en mémoire avec 10 % de pertes —
position et champs synchronisés, entité donnée à un client puis relayée à l'autre, apparition reçue
par un joueur arrivé après, RPC client → tous (relayé avec le vrai expéditeur), hôte → un joueur,
client → hôte, disparition partout, propriétés rendues à l'hôte au départ d'un joueur ; message de
10 Ko fragmenté avec 30 % de pertes ; découverte ; permissions des RPC.

## Horloge partagée

`agr.net.time()` rend l'horloge de l'**hôte** (secondes) sur toutes les machines, et `true` quand elle
est synchronisée : chaque ping de l'hôte transporte son heure, le client corrige de la moitié de l'aller-
retour et lisse l'écart (à quelques millisecondes près). Pour qu'un compte à rebours, une apparition
ou un rythme tombent au même instant chez tous : l'hôte envoie « à `agr.net.time()` + 3 s », chacun
attend ce moment. C++ : `NetSession::networkTime()`, `clockSynced()`.

## Apparitions selon la visibilité

`NetSync.spawnWhenVisible` (entités créées en réseau, `agr.net.spawn`) : l'entité n'existe que sur les
machines des joueurs qui la voient (selon `visibility` / `visibleRange` / `setVisibleTo`) : elle y est
créée quand le joueur commence à la voir (avec son état à jour) et retirée quand il ne la voit plus.
Brouillard de guerre, butin caché, grands mondes : rien n'est envoyé ni simulé chez les autres, et un
client modifié ne peut pas révéler ce qu'il ne reçoit pas. `stats().spawnsHidden` compte les paires
(entité, joueur) cachées.

## HTTP et HTTPS (`agr.http`)

Client HTTP/1.1 intégré (TLS 1.2 / 1.3 par **mbedTLS 3.6**), sur des fils de travail : le jeu ne se
bloque jamais, le rappel est appelé sur le fil principal au début d'une image.

```lua
agr.http.get("https://api.exemple.com/scores", function(r)
    if r.ok then local scores = agr.json.decode(r.body) else agr.warn(r.status, r.error) end
end)
agr.http.post("https://api.exemple.com/score", {name = "Zoé", score = 1200}, onSent)  -- table = JSON
local id = agr.http.request{url = "https://cdn.exemple.com/dlc1.agrpak", download = "user://packs/dlc1.agrpak",
                            timeout = 120, headers = {Authorization = "Bearer ..."}}
local recu, total = agr.http.progress(id)   -- barre de progression ; agr.http.cancel(id)
-- ensuite : agr.loadPack("user://packs/dlc1.agrpak")
```

- Réponse : `status`, `ok` (2xx sans erreur), `body`, `headers` (noms en minuscules), `error`, `url`
  (après redirections), `bytes`.
- Méthodes quelconques (`GET`, `POST`, `PUT`, `PATCH`, `DELETE`, `HEAD`), corps et en-têtes libres,
  redirections suivies (5 au plus), corps « chunked », délai global, annulation, téléchargement direct
  dans un fichier de `user://` (écrit en `.part` puis renommé : jamais de fichier à moitié reçu).
- Certificats vérifiés avec les autorités du système (Linux : `/etc/ssl/certs`, Windows : magasin
  `ROOT`, Android : `/system/etc/security/cacerts`) ; `SSL_CERT_FILE` ou `AGR_CA_BUNDLE` les remplacent.
  Proxy : `HTTPS_PROXY` / `HTTP_PROXY` (tunnel `CONNECT`), `NO_PROXY` respecté. IPv4 et IPv6.
- C++ : `net::httpRequest` (bloquant) et `net::HttpClient` (asynchrone) dans `agr/net/Http.hpp`.
- Vérifié : serveur local de test (longueur, chunked, fermeture, redirections et boucle, POST, 404,
  hôte inconnu, connexion refusée, délai, annulation, téléchargement de 2 Mo avec progression) et
  une vraie requête HTTPS sur Internet à travers le proxy de l'environnement de développement,
  certificat contrôlé. Le refus d'un certificat expiré / auto-signé n'a pas pu être montré en ligne
  (le proxy bloque les sites de test badssl.com) ; compilé pour Linux, Windows (DLL système
  seulement) et Android.

## WebSocket (`agr.ws`)

Client WebSocket (RFC 6455) sur `ws://` et `wss://` (même TLS, proxy et vérification de certificats que
HTTPS) : chat, salons de jeu, serveurs de matchmaking, services temps réel.

```lua
local id = agr.ws.connect("wss://jeu.exemple.com/salon", {
    onOpen = function() agr.ws.send(id, {type = "join", name = "Zoé"}) end,  -- table = JSON
    onMessage = function(data, binary) local m = agr.json.decode(data) ... end,
    onClose = function(code, err) agr.log("fermé", code, err) end,
}, {Authorization = "Bearer ..."})
agr.ws.send(id, "texte")  ;  agr.ws.send(id, octets, true)  -- binaire
agr.ws.state(id)          -- "connecting", "open", "closing", "closed"
agr.ws.close(id [, 1000, "raison"])
```

- Fil dédié par connexion ; les rappels arrivent sur le fil principal, dans l'ordre (ouverture,
  messages, fermeture). Pings du serveur auto-répondus, messages fragmentés réassemblés, poignée de
  fermeture faite dans les deux sens, sous-protocoles (`Sec-WebSocket-Protocol`), messages jusqu'à 16 Mo.
- C++ : `net::WebSocket` (`connect`, `send`, `sendBinary`, `receive`, `close`, `state`, `closeCode`).
- Vérifié contre un serveur de test local (texte, binaire de 70 000 octets — longueur sur 64 bits —,
  fragments, ping / pong, fermeture par le serveur avec son code et par le client, refus d'un serveur
  HTTP ordinaire, valeur d'acceptation de la RFC). `wss://` vers Internet n'a pas pu être essayé
  (le proxy de l'environnement de développement refuse les serveurs d'écho publics) ; sa couche TLS
  est celle d'HTTPS, testée.

## Limites actuelles

- **IPv4 et IPv6** : l'hôte écoute sur les deux (même numéro de port ; IPv6 seulement si le système
  l'a) ; `join("::1", port)`, `join("[2001:db8::7]", port)` ou un nom (IPv4 préféré quand il a les deux).
  La découverte LAN reste en IPv4 (diffusion). Le chemin IPv6 réel n'a pas pu être testé dans la VM
  de développement (noyau sans IPv6) : seul le repli IPv4 l'est.
- Pas de traversée de NAT ni de serveur relais fourni (sur Internet, l'hôte doit
  ouvrir son port UDP) ; pas de WebRTC ni de serveur WebSocket (client seulement).
- Chiffrement à **clé partagée** (le mot de passe) : pas d'échange de clés Diffie-Hellman ni de
  certificats ; qui connaît le mot de passe et a enregistré la poignée de main peut déchiffrer la
  session (pas de confidentialité persistante). La découverte LAN et la poignée de main restent en clair.
- Interpolation intégrée (NetSync) mais pas de prédiction / réconciliation côté client ni de
  compensation de latence ; `spawnWhenVisible` ne concerne que les entités créées en réseau (les
  entités de la scène existent partout).
