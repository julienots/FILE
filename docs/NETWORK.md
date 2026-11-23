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
| `agr.net.send(joueur, valeur [, livraison [, canal]])` | envoie une table (nombres, textes, booléens, tables imbriquées ; 1200 octets en JSON au plus) ; livraison : `true` / `"reliable"` (fiable, dans l'ordre), `false` / `"unreliable"` (défaut), ou `"ordered"` : **non fiable ordonné** sur un des 8 canaux (`canal` 0 à 7) — un message plus ancien que le dernier reçu sur son canal est jeté au lieu d'arriver en retard (positions, visée) ; `agr.net.stats().staleDropped` les compte. C++ : `NetSession::sendOrdered` |
| `agr.net.broadcast(valeur [, livraison [, canal]])` | hôte : à tous les joueurs ; client : à l'hôte |
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
  **Livraison** (comme Godot) : fiable et dans l'ordre par défaut ; le mot `unreliable` dans la
  déclaration (`aim = "authority,unreliable"`) l'envoie sans garantie (visée, effets jetables), `ordered`
  (`move = "authority,ordered"`) en non fiable ordonné (une RPC plus ancienne que la dernière reçue est
  jetée). L'hôte relaie avec la même livraison. C++ : `Replicator::rpc(..., Delivery, canal)`.

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
- **Compression** : `agr.net.setCompression(true)` (C++ : `NetSession::setCompression`) compresse ce
  que cette machine envoie (LZ77 rapide du moteur, seulement quand le paquet y gagne ; chaque paquet le
  signale, l'autre côté n'a rien à régler ; avant le chiffrement). Les états de jeu en JSON répétitifs
  pèsent 3 à 5 fois moins (`stats().bytesBeforeCompression` / `bytesAfterCompression`). Le
  décompresseur vérifie chaque longueur (2 000 paquets corrompus au hasard dans les tests, aucun
  débordement). Test : messages fiables fragmentés reçus intacts avec 15 % de pertes, avec et sans
  chiffrement, plus de 3 fois moins d'octets.
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

## Multijoueur par WebSocket

`agr.net.setTransport("websocket")` avant `host` / `join` (C++ : `NetSession::setTransport(
std::make_unique<WebSocketTransport>())`) fait passer **toute** la session par WebSocket au lieu
d'UDP, comme `WebSocketMultiplayerPeer` de Godot : synchronisation, apparitions, RPC, prédiction,
chiffrement et compression fonctionnent pareil. L'hôte écoute en `ws://` sur son port, les joueurs s'y
connectent (`ws://adresse:port/agr`) ; utile derrière un pare-feu qui bloque l'UDP ou pour un service
web. `"udp"` revient au transport par défaut. Test : connexion, messages fiables (20 Ko) et non fiables
dans les deux sens, lien chiffré et compressé, sur la boucle locale.

## Prédiction côté client et compensation de latence

Ce que Godot ne fournit pas tout fait : un déplacement **décidé par l'hôte** (pas de triche sur la
position) mais **sans retard** pour le joueur qui le commande.

- `agr.net.setController(entité, joueur)` (hôte) : ce joueur pilote l'entité par ses entrées ; l'hôte
  la possède et la simule (`NetSync.controller`).
- Dans le script de l'entité, le déplacement s'écrit une seule fois, dans `onNetInput(input, dt)` ; le
  joueur appelle `self.entity:netInput({ x = axe, saut = appuyé })` à chaque image :
  ```lua
  function Hero:onNetInput(input, dt) self.entity:translate(input.x * 5 * dt, 0, 0) end
  function Hero:onUpdate(dt) self.entity:netInput({ x = agr.input.value("move_x") }) end
  ```
- **Prédiction** : l'entrée est appliquée tout de suite chez le joueur, envoyée à l'hôte (non fiable,
  chaque paquet répète les 8 dernières entrées non confirmées), qui l'applique dans l'ordre et une seule
  fois (au plus 0,25 s par entrée : pas d'accélération par triche). **Réconciliation** : chaque état de
  l'hôte porte le numéro de la dernière entrée traitée ; le joueur reprend la position de l'hôte et
  rejoue les entrées pas encore traitées. Les autres joueurs voient l'entité interpolée, comme d'habitude.
  En solo, `netInput` applique simplement l'entrée : le même script sert hors ligne.
- **Compensation de latence** : l'hôte garde 1 s d'historique des positions ;
  `agr.net.positionAt(entité, temps)` donne où elle était à ce temps de l'hôte, et
  `agr.net.viewTime()` (chez le client) le temps de ce qu'il voit (horloge réseau − demi-aller-retour −
  délai d'interpolation) : le tireur envoie `viewTime()` avec son tir, l'hôte vérifie le tir là où la
  cible était à l'écran du tireur.
- Statistiques (`Replicator::stats`) : entrées envoyées, appliquées, rejouées, écart de la dernière
  correction.

Test (20 % de pertes et paquets désordonnés) : la première entrée déplace le joueur dans la même image ;
après 60 images de course, hôte et joueur sont au même endroit à 0,1 % près, le troisième joueur aussi ;
un autre joueur ne peut pas piloter l'entité ; une entrée qui réclame 10 s est ramenée à 0,25 s et le
joueur est corrigé ; `positionAt` retrouve la position d'il y a 0,25 s.

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

## Sockets bas niveau et serveur WebSocket (`agr.tcp`, `agr.udp`, `agr.wsserver`)

Pour des protocoles maison, des outils, une appli compagnon sur téléphone ou un tableau de bord web :
non bloquants, interrogés à chaque image (pas de fil), fermés quand le jeu s'arrête.

| Fonction | Rôle |
|---|---|
| `agr.tcp.listen(port [, localSeulement])` → serveur, port ; `agr.tcp.accept(serveur)` → connexion, adresse \| `nil` | serveur TCP (port 0 = libre) |
| `agr.tcp.connect(hôte, port)` → connexion ; `agr.tcp.state(c)` → `"connecting"`, `"open"`, `"closed"` | client TCP (s'ouvre dans les images suivantes) |
| `agr.tcp.send(c, octets)`, `agr.tcp.receive(c)` → octets reçus depuis le dernier appel, `agr.tcp.close(c ou serveur)` | flux d'octets |
| `agr.udp.open([port])` → socket, port ; `agr.udp.send(s, hôte, port, octets)` ; `agr.udp.receive(s)` → `{ {host, port, data}, ... }` ; `agr.udp.close(s)` | datagrammes (diffusion `255.255.255.255` permise) |
| `agr.wsserver.listen(port [, localSeulement])` → serveur, port ; `agr.wsserver.poll(serveur)` → `{ {type = "connected" \| "message" \| "disconnected", client, data, binary}, ... }` | **serveur WebSocket** (RFC 6455) : navigateurs, téléphones, client `agr.ws` |
| `agr.wsserver.send(serveur, client, texte [, binaire])`, `broadcast(serveur, texte [, binaire])`, `clients(serveur)`, `close(serveur [, client])` | envoi, liste, fermeture |

Serveur WebSocket : poignée de main HTTP vérifiée (requête non WebSocket → `400`), trames masquées
exigées des clients, longueurs 7, 16 et 64 bits, messages fragmentés recollés, `ping` → `pong`,
fermeture propre, 16 Mo par message au plus. En `ws://` seulement (pour `wss://`, un proxy TLS devant).
C++ : `agr::net::TcpServer`, `TcpConnection`, `UdpSocket`, `WebSocketServer` (`agr/net/Sockets.hpp`).
Tests : le client WebSocket du moteur se connecte au serveur et échange texte, binaire, messages de
100 Ko et 70 Ko, diffusion, fermeture ; TCP avec 200 Ko envoyés en plusieurs fois, fermeture détectée ;
UDP aller ; les mêmes depuis Lua.

## Cryptographie (`agr.crypto`)

Comme `Crypto` / `HMACContext` / `AESContext` de Godot (mbedTLS) :

| Fonction | Rôle |
|---|---|
| `agr.crypto.hmac(clé, message)` → 32 octets | HMAC-SHA-256 : signer un score, une sauvegarde, une requête vers son serveur |
| `agr.crypto.randomBytes(n)` | octets aléatoires sûrs (jetons, sels, identifiants) |
| `agr.crypto.deriveKey(motDePasse, sel [, itérations])` → 32 octets | PBKDF2-SHA-256 |
| `agr.crypto.encrypt(clé, texte)`, `agr.crypto.decrypt(clé, scellé)` → texte \| `nil` | AES-256-GCM authentifié (nonce aléatoire) : `nil` si les données ont été modifiées ou la clé est fausse |

Les résultats sont des octets bruts (`agr.base64.encode` pour les afficher ou les stocker en JSON).
C++ : `agr::net::cryptobox`. Test : vecteur officiel RFC 4231 pour HMAC, déchiffrement refusé après une
modification d'un octet ou avec une autre clé.

## Ouvrir un port sur la box (UPnP)

Pour héberger depuis chez soi une partie jointe par Internet, le port doit être redirigé par la box
vers la machine. `agr.upnp.openPort(7777 [, "UDP" | "TCP" [, description]])` le demande à la box
(UPnP IGD, comme la classe `UPNP` de Godot), **en arrière-plan** : `agr.upnp.status()` → `"working"`, puis
`"done", adresse publique` (à donner aux amis) ou `"failed", raison` (UPnP désactivé sur la box, pas
de box). `agr.upnp.closePort(7777)` retire la redirection. C++ : `agr::net::UpnpGateway` (`discover`,
`externalAddress`, `addPortMapping`, `deletePortMapping`), appels bloquants.

Recherche SSDP (multicast `239.255.255.250:1900`), description de la box, service `WANIPConnection` ou
`WANPPPConnection` (adresse de contrôle relative ou absolue), requêtes SOAP ; jamais par un proxy HTTP.
Test : une fausse box sur la boucle locale (SSDP, description, SOAP) est trouvée, donne son adresse
publique, reçoit une redirection (adresse locale, protocole, description échappée), la retire, et
refuse d'en retirer une absente avec sa raison ; sans box, échec clair. **Pas essayé avec une vraie
box** (la machine de développement n'en a pas).

## Profileur réseau (éditeur)

*Affichage › Réseau (profileur)* (comme le profileur réseau du débogueur de Godot) : rôle, joueurs avec
leur ping et l'état du chiffrement, octets et paquets (renvoyés, rejetés, périmés), gain de la
compression, **débit par seconde sur la dernière minute** (envoi et réception), synchronisation (entités,
états envoyés / reçus / ignorés / cachés, RPC, prédiction) et un tableau des **entités** classées par
trafic : états envoyés et reçus (nombre et octets), RPC envoyées / reçues et par fonction ; *Remettre à
zéro*. C++ : `NetSession::bandwidthHistory`, `Replicator::profile`. Testé : compteurs par entité et
échantillons de débit (test de prédiction), panneau ouvert dans le test de bout en bout de l'éditeur.

## Comparaison avec Godot 4

| Fonction | Godot 4 | AGR ENGINE |
|---|---|---|
| Synchronisation d'état, intervalle, différences seulement, visibilité par joueur | oui (`MultiplayerSynchronizer`) | oui (`NetSync`) |
| Apparitions réseau, retardataires, apparition selon la visibilité | oui (`MultiplayerSpawner`) | oui |
| RPC : n'importe qui / autorité, appel local, fiable / non fiable / non fiable ordonné, canaux | oui | oui |
| Fiable, non fiable, non fiable ordonné sur canaux, compression | oui (ENet) | oui |
| Chiffrement | DTLS avec certificats | AES-256-GCM avec le mot de passe (sans certificat, sans échange de clés) |
| Transport WebSocket pour le multijoueur, serveur WebSocket | oui | oui (`ws://` ; pas de serveur `wss://`) |
| TCP, UDP, HTTP / HTTPS, client WebSocket / `wss://` | oui | oui |
| UPnP (redirection de port) | oui | oui (essayé contre une fausse box seulement) |
| Cryptographie pour les scripts | HMAC, AES, clés RSA, certificats, signatures | HMAC, AES-GCM, PBKDF2, aléa sûr (pas de RSA ni de certificats) |
| Profileur réseau | oui (débogueur) | oui (débit, joueurs, entités, RPC) |
| IPv6 | oui | oui (non testé en vrai IPv6 ici) |
| WebRTC (pair à pair, traversée de NAT) | oui (extension officielle) | **non** |
| **Prédiction côté client + réconciliation** | non (à écrire soi-même ou greffon tiers) | **oui** (`NetSync.controller`, `onNetInput`) |
| **Compensation de latence** (historique des positions) | non | **oui** (`positionAt`, `viewTime`) |
| **Découverte des parties en réseau local** | non (à écrire) | **oui** (`advertise` / `discover`) |
| **Horloge réseau partagée** | non | **oui** (`agr.net.time`) |
| **Mot de passe de partie** | non | **oui** |

Bilan : AGR fait tout ce que fait le réseau de Godot sauf WebRTC, le serveur TLS et la partie
certificats / RSA de la cryptographie, et ajoute la prédiction avec réconciliation, la compensation de
latence, la découverte LAN, l'horloge partagée et le mot de passe — ce qu'un jeu d'action en ligne doit
sinon écrire lui-même sous Godot.

## Limites actuelles

- **IPv4 et IPv6** : l'hôte écoute sur les deux (même numéro de port ; IPv6 seulement si le système
  l'a) ; `join("::1", port)`, `join("[2001:db8::7]", port)` ou un nom (IPv4 préféré quand il a les deux).
  La découverte LAN reste en IPv4 (diffusion). Le chemin IPv6 réel n'a pas pu être testé dans la VM
  de développement (noyau sans IPv6) : seul le repli IPv4 l'est.
- Pas de traversée de NAT ni de serveur relais fourni (sur Internet, l'hôte ouvre son port UDP, par
  UPnP si sa box le permet) ; pas de WebRTC ; pas de certificats ni de clés RSA / ECDSA dans `agr.crypto` ; serveur WebSocket en `ws://` seulement (pas de TLS intégré
  côté serveur) ; sockets bas niveau en IPv4.
- Chiffrement à **clé partagée** (le mot de passe) : pas d'échange de clés Diffie-Hellman ni de
  certificats ; qui connaît le mot de passe et a enregistré la poignée de main peut déchiffrer la
  session (pas de confidentialité persistante). La découverte LAN et la poignée de main restent en clair.
- Prédiction : le mouvement rejoué est celui du script (`onNetInput`) ; la physique (corps rigides,
  contrôleurs de personnage) n'est pas rembobinée, et la correction est instantanée (pas de lissage
  visuel de l'écart) ; `spawnWhenVisible` ne concerne que les entités créées en réseau (les
  entités de la scène existent partout).
