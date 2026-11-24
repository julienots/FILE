# AXON — par SuperEssence Studio

**Claude aux commandes.** Avec AXON, Claude pilote les applications de ton téléphone Android :
il les ouvre, touche, écrit et fait défiler. Tu le regardes faire **en direct**, sur le téléphone
lui-même ou sur une tablette. **Aucun ordinateur n'est nécessaire.**

```
 App Claude (téléphone)  ──connecteur MCP──▶  Relais AXON (Cloudflare, gratuit)
                                                  │            │
                         WebSocket (Internet)     ▼            ▼
                         Téléphone piloté (APK AXON)    Écran de contrôle (tablette :
                         service d'accessibilité          navigateur ou APK AXON)
```

## Contenu
| Dossier | Rôle |
|---|---|
| `android/` | App Android native (APK) : écran de chargement SuperEssence Studio, logo AXON, service d'accessibilité qui exécute les actions de Claude |
| `relay/` | Relais Cloudflare Worker : connecteur MCP pour Claude, écran de contrôle web, retransmission |
| `server/` + `app/` | Variante « ordinateur » : serveur MCP local + ADB (facultatif) |

## 1. Mettre le relais en ligne (une seule fois, ~3 min, faisable sur mobile)
Le relais est la seule pièce hébergée. C'est un Worker Cloudflare gratuit.
1. Crée un compte sur dash.cloudflare.com.
2. Profil › Jetons d'API › Créer un jeton › modèle **Modifier Cloudflare Workers**.
3. Déploiement : dans `relay/`, lance `CLOUDFLARE_API_TOKEN=… npx wrangler deploy`. Tu peux aussi le
   demander à Claude Code en lui donnant le jeton par le formulaire sécurisé.
4. Note l'adresse obtenue, par exemple `https://axon-relay.ton-nom.workers.dev`.

## 2. Installer l'APK sur le téléphone à piloter
1. Ouvre `AXON-1.0.x.apk` et autorise l'installation.
2. Au lancement, après l'écran de chargement, choisis **Claude pilote cet appareil**.
3. **Étape 1** : Paramètres › Accessibilité › AXON › Activer. Si Android affiche « Paramètre restreint » :
   Paramètres › Applications › AXON › ⋮ › **Autoriser les paramètres restreints**, puis réessaie.
4. **Étape 2** : touche *Régler* et colle l'adresse du relais.
5. **Étape 3** : touche *Copier*, puis dans Claude : Paramètres › Connecteurs ›
   **Ajouter un connecteur personnalisé** › nom « AXON » › colle l'adresse.
6. **Étape 4 (facultatif)** : *Partager* envoie le lien de l'écran de contrôle à ta tablette. Elle peut
   l'ouvrir dans le navigateur, ou dans l'APK AXON avec le rôle « Écran de contrôle ».

## Plusieurs appareils en même temps
Un même salon (une clé, un seul connecteur Claude) peut réunir plusieurs téléphones pilotés.
1. Sur le premier téléphone : étape 4 › **Partager**, puis envoie le lien à l'autre téléphone.
2. Sur l'autre : AXON › « Claude pilote cet appareil » › **Réglages avancés › Rejoindre** › colle le lien.
3. Donne un nom à chaque appareil (Réglages avancés) : c'est ce nom que Claude voit.

Claude appelle `list_devices`, puis choisit l'appareil avec `device` (nom ou numéro). `device: "all"` fait la
même action sur tous à la fois. L'écran de contrôle affiche un onglet par appareil et une mosaïque « Tous ».

## Utilisation
- Dans Claude : *« Avec AXON, ouvre YouTube et cherche de la musique lo-fi »*.
- Ou écris ta demande dans AXON, puis dis à Claude *« traite mes demandes AXON »*.
- L'interrupteur **Mettre Claude en pause** bloque instantanément toutes ses actions.

## Outils donnés à Claude
`list_devices`, `do_actions`, `status`, `screenshot`, `get_ui_elements`, `tap_element`, `tap`, `swipe`, `type_text`, `press_key`,
`list_apps`, `launch_app`, `open_url`, `notify`, `get_requests`, `wait_for_request`, `complete_request`.

## Compiler l'APK soi-même
```bash
cd android && ./gradlew assembleRelease -PaxonRelay=https://axon-relay.ton-nom.workers.dev
```
L'option `-PaxonRelay` préremplit l'adresse du relais. Sans elle, il faut la saisir dans l'app.

## Sécurité
- La clé du salon, 28 caractères aléatoires, est générée sur le téléphone. Quiconque connaît l'adresse
  du connecteur peut piloter le téléphone : ne la partage pas. *Réglages › Nouvelle clé* la révoque.
- Claude a pour consigne de ne jamais payer, supprimer ou modifier un compte sans ton accord.
- La clé de signature (`android/keystore`) est incluse pour que les mises à jour s'installent par-dessus.
  Si le dépôt est public, remplace-la par la tienne.

## Limites
- Android 11 ou plus est requis pour que Claude voie l'écran en image. Avant, il ne fait que lire les éléments.
- Certaines apps (banques, Netflix) bloquent la capture : écran noir.
- L'écran du téléphone doit être allumé et déverrouillé pendant que Claude agit.

---
© SuperEssence Studio
