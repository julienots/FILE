# 🎬 Hub d'automatisation vidéo — CUTTINX → Claude → Instagram

Vous déposez des vidéos brutes, le hub s'occupe du reste : **montage automatique dans CUTTINX**,
**titre, description et hashtags accrocheurs**, puis **publication en Reel sur Instagram** (@julien.blt.bs),
tout de suite ou à une heure de forte audience.

```
 Téléphone / navigateur                         Hub (ce dépôt)                                Services
┌──────────────────────┐   1. dépôt   ┌───────────────────────────────────────┐
│ Interface web        │ ───────────▶ │ File d'attente SQLite + worker        │
│ (catégorie, format,  │              │                                       │  2. montage   ┌──────────────────────┐
│  publication)        │              │  edit ──────────────────────────────────────────────▶ │ API Claude           │
└──────────────────────┘              │                                       │               │ + MCP CUTTINX        │
                                      │                                       │               │ (Claude juge le style│
┌──────────────────────┐ 3. MP4 rendu │  fetch_render ◀── URL CUTTINX,        │               │  et les coupes)      │
│ Téléphone : CUTTINX  │ ───────────▶ │     /api/renders/inbox, dossier synchro│              └──────────────────────┘
│ exporte dans         │              │                                       │  4. légende   ┌──────────────────────┐
│ Movies/Cuttinx       │              │  caption ───────────────────────────────────────────▶ │ API Claude (JSON)    │
└──────────────────────┘              │  schedule (heures d'audience)         │               └──────────────────────┘
                                      │  publish ───────────────────────────────────────────▶ ┌──────────────────────┐
  Notifications ◀──────────────────── │  relances auto + alertes              │  5. Reel      │ Instagram Graph API  │
  (ntfy / Discord / Telegram)         └───────────────────────────────────────┘               └──────────────────────┘
```

## Sommaire
1. [Architecture](#1-architecture)
2. [Le point clé : récupérer le MP4 rendu](#2-le-point-clé--récupérer-le-mp4-rendu)
3. [Installation locale](#3-installation-locale)
4. [Configuration des clés](#4-configuration-des-clés)
5. [Déploiement](#5-déploiement)
6. [Utilisation au quotidien](#6-utilisation-au-quotidien)
7. [Itérer le style avec Claude](#7-itérer-le-style-avec-claude)
8. [Erreurs, relances et notifications](#8-erreurs-relances-et-notifications)
9. [Référence API](#9-référence-api)
10. [Limites connues](#10-limites-connues)

---

## 1. Architecture

| Fichier | Rôle |
|---|---|
| `hub/main.py` | Application FastAPI : interface de dépôt + tableau de bord, API, liens de téléchargement signés, démarrage du worker |
| `hub/pipeline.py` | Machine à états et worker : `edit → fetch_render → caption → schedule → publish`, relances, reprise après redémarrage |
| `hub/editor_claude.py` | **Mode « Claude en relais » (par défaut)** : l'API Claude se connecte au serveur MCP CUTTINX et monte la vidéo elle‑même |
| `hub/editor_direct.py` + `hub/cuttinx_mcp.py` | **Mode direct** : le hub appelle lui‑même les outils MCP CUTTINX avec une recette fixe (sans Claude) |
| `hub/captions.py` | Accroche, description et hashtags générés par Claude (sortie JSON validée), avec quelques images du rendu si `ffmpeg` est présent |
| `hub/instagram.py` | Publication Reel via l'API officielle Instagram Graph (upload « resumable » ou `video_url`) |
| `hub/scheduler.py` | Choix des créneaux de forte audience (fuseau, heures semaine/week‑end, écart minimum entre deux posts) |
| `hub/renders.py` | Récupération du MP4 rendu (URL, envoi depuis le téléphone, dossier synchronisé) |
| `hub/notify.py` | Notifications ntfy / webhook Discord‑Slack / Telegram |
| `styles/*.md` | Guides de style par défaut lus par Claude (modifiables depuis le tableau de bord) |

**États d'une vidéo** (tableau de bord) : En attente → En montage → Rendu en cours de récupération → Rendue →
Texte en cours → Programmée → Publication → **Publiée** (ou **Erreur**).

### Les deux modes de montage

- **`EDITOR_MODE=claude` (recommandé, la « variante Claude en relais » du brief).** Pour chaque vidéo, le hub ouvre une
  session avec l'API Claude (modèle `claude-opus-5-5`) en lui donnant le serveur MCP CUTTINX (*MCP connector* de l'API).
  Claude importe le rush, le **regarde** (`view_storyboard`), repère les moments forts, analyse la musique, choisit les
  effets/transitions, crée le projet, vérifie un rendu brouillon, corrige, lance le rendu final et attend sa fin.
  Il suit le guide de style de la catégorie et vos notes/retouches. Tout reste automatique.
- **`EDITOR_MODE=direct`.** Recette déterministe : moments forts (`find_highlights`) calés sur le tempo (`analyze_music`),
  ralenti en rampe sur le meilleur moment, zooms, transitions dynamiques, 9:16 pour les perfs ; suppression des silences
  (`analyze_audio`) et coupes franches pour les vidéos normales. Pas de coût en tokens, mais pas de « regard » sur les images.

Dans les deux cas le projet CUTTINX s'appelle **`HUB-<id>`** et reste modifiable à la main dans l'appli.

La publication, elle, est faite par le hub avec l'API Instagram Graph officielle (plus fiable qu'un MCP Instagram :
reprise après crash, programmation, relances).

---

## 2. Le point clé : récupérer le MP4 rendu

CUTTINX exporte le rendu **sur le téléphone** (galerie `Movies/Cuttinx`). Aujourd'hui `get_render` ne renvoie pas
d'URL de téléchargement : il faut donc que le fichier remonte jusqu'au hub. Le hub accepte trois voies, utilisez‑en une :

| Voie | Mise en place | Automatique ? |
|---|---|---|
| **A. URL renvoyée par CUTTINX** | Si une future version de CUTTINX ajoute `download_url` (ou `url`) au résultat de `get_render`, le hub la télécharge tout seul. Rien à configurer. | ✅ |
| **B. Envoi depuis le téléphone** (recommandé) | Une automatisation Android envoie chaque nouveau fichier de `Movies/Cuttinx` vers `POST {PUBLIC_BASE_URL}/api/renders/inbox` avec l'en‑tête `X-Upload-Token: <RENDER_UPLOAD_TOKEN>` (multipart, champ `file`). Voir ci‑dessous. | ✅ |
| **C. Dossier synchronisé** | Syncthing / FolderSync / rclone synchronise `Movies/Cuttinx` vers un dossier du serveur ; mettez son chemin dans `RENDER_INBOX_DIR`. Le hub le surveille. | ✅ |

Le fichier est rattaché à la bonne vidéo grâce à `HUB-<id>` dans son nom. S'il n'y figure pas, il apparaît dans
**« Rendus non attribués »** du tableau de bord (rattachement en un clic). En dépannage, le bouton
**« Envoyer le MP4 à la main »** existe aussi dans la fiche de chaque vidéo.

**Exemple voie B avec MacroDroid** (gratuit) :
1. Déclencheur : *Fichier modifié* → dossier `Movies/Cuttinx`, *Fichier créé*.
2. Action : *Requête HTTP* → `POST`, URL `https://<votre-hub>/api/renders/inbox`, en‑tête `X-Upload-Token: <jeton>`,
   corps *Fichier* (multipart, nom de champ `file`) = `{trigger_file}`.
   (Équivalent possible avec Tasker + AutoTools, ou HTTP Shortcuts.)

> Si vous êtes à la main sur CUTTINX, l'idéal est d'ajouter dans `get_render` une URL temporaire du MP4 :
> la voie A rendra tout le reste inutile.

**Pré‑requis côté téléphone** : CUTTINX ouvert avec **« Connecté à Claude »** activé pendant les montages. Sinon le
montage échoue proprement, est relancé automatiquement, puis vous êtes notifié.

---

## 3. Installation locale

Pré‑requis : Python 3.11+ (ou Docker), `ffmpeg` conseillé (images envoyées à Claude pour mieux écrire les légendes).

```bash
cd video-hub
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env        # puis remplissez les valeurs (section 4)
set -a && source .env && set +a
uvicorn hub.main:app --reload --port 8000
```

Ouvrez http://localhost:8000. Les réglages manquants s'affichent en haut du tableau de bord.

Pour tester sans rien publier : `DRY_RUN_PUBLISH=true` (le pipeline va jusqu'au bout et journalise la légende).

Tests : `pip install pytest && python -m pytest -q tests`

---

## 4. Configuration des clés

Tout passe par des variables d'environnement (`.env` en local, « Variables/Secrets » chez l'hébergeur).
**Aucun secret dans le code.** `.env` est ignoré par git. Liste complète et commentée : [`.env.example`](.env.example).

### 4.1 Le hub
- `PUBLIC_BASE_URL` : l'adresse HTTPS publique du hub (ex. `https://hub.mondomaine.fr`). **Obligatoire** : CUTTINX
  télécharge les rushs via des liens signés `…/files/raw/<id>/<signature>/…`.
- `HUB_PASSWORD` (+ `HUB_USERNAME`) : protège l'interface (authentification HTTP Basic).
- `HUB_SECRET` : clé de signature des liens. Générez‑la : `python -c "import secrets;print(secrets.token_hex(32))"`.
- `RENDER_UPLOAD_TOKEN` : jeton de l'automatisation du téléphone (voie B). Même commande pour le générer.

### 4.2 Claude (Anthropic)
1. Créez une clé sur https://console.anthropic.com → *API Keys*.
2. `ANTHROPIC_API_KEY=...`
3. Optionnel : `CLAUDE_MODEL` (défaut `claude-opus-5-5`), `CLAUDE_EDIT_EFFORT` (défaut `high`, le montage demande du
   jugement), `CLAUDE_CAPTION_EFFORT` (défaut `medium`). `CLAUDE_FALLBACKS=true` active le repli serveur automatique sur
   un autre modèle si une requête est refusée par les filtres de sécurité.

Coût indicatif : un montage Claude = une session de plusieurs appels d'outils et d'images (storyboards) ; comptez
quelques dizaines de centimes à ~1 € selon la longueur des rushs. Le mode `direct` ne consomme que la légende.

### 4.3 CUTTINX (serveur MCP)
- `CUTTINX_MCP_URL` : l'URL du serveur MCP CUTTINX (celle du connecteur « CUTTINX → Claude » ; transport *Streamable HTTP*).
- `CUTTINX_MCP_TOKEN` : le jeton d'accès associé s'il y en a un (envoyé en `Authorization: Bearer`).
- `DEFAULT_MUSIC_MEDIA_ID` (facultatif) : `media_id` d'une musique déjà partagée dans CUTTINX, utilisée pour les perfs
  quand vous n'en joignez pas au dépôt. Sinon, en mode Claude, il en choisit une parmi vos musiques partagées.

### 4.4 Instagram Graph API (compte pro/créateur relié à une page Facebook)
1. Passez @julien.blt.bs en **compte professionnel** (Créateur ou Entreprise) et **reliez‑le à une page Facebook**
   (Instagram → Paramètres → Compte → Partage vers d'autres applis / Centre de comptes).
2. Sur https://developers.facebook.com créez une app (**type Business**), ajoutez le produit
   *Instagram Graph API* / *Facebook Login for Business*.
3. Dans l'**Explorateur de l'API Graph**, générez un jeton utilisateur avec les permissions :
   `instagram_basic`, `instagram_content_publish`, `pages_show_list`, `pages_read_engagement`.
4. Échangez‑le contre un **jeton longue durée** :
   `GET https://graph.facebook.com/v21.0/oauth/access_token?grant_type=fb_exchange_token&client_id=APP_ID&client_secret=APP_SECRET&fb_exchange_token=JETON_COURT`
5. Récupérez le **jeton de page** (qui n'expire pas s'il vient d'un jeton longue durée) et l'**ID Instagram** :
   `GET https://graph.facebook.com/v21.0/me/accounts?fields=name,access_token,instagram_business_account&access_token=JETON_LONG`
   → `access_token` de votre page = `IG_ACCESS_TOKEN`, `instagram_business_account.id` = `IG_USER_ID`.
6. Tant que l'app est en mode *Développement*, seuls les comptes ayant un rôle sur l'app peuvent publier : c'est suffisant
   pour votre propre compte.

`IG_UPLOAD_MODE=resumable` (défaut) : le hub envoie lui‑même le fichier à Instagram (pas besoin que la vidéo soit publique).
`IG_UPLOAD_MODE=url` : Instagram télécharge le MP4 via un lien signé du hub.

Exigences Instagram pour un Reel : MP4 H.264/AAC, 3 s à 15 min, 9:16 conseillé, 1080 px recommandés — les rendus
CUTTINX `final` 1080 conviennent. Limite : ~50 publications par 24 h via l'API.

### 4.5 Programmation
`TIMEZONE=Europe/Paris`, `PEAK_HOURS_WEEKDAY=12:30,18:30,21:00`, `PEAK_HOURS_WEEKEND=11:00,18:00,20:30`,
`MIN_GAP_MINUTES=180`. Ajustez avec vos statistiques Instagram (Insights → Audience → Moments les plus actifs).

### 4.6 Notifications (au moins une conseillée)
- **ntfy** (le plus simple, push sur le téléphone) : installez l'appli ntfy, abonnez‑vous à un nom de topic difficile à
  deviner, et mettez `NTFY_URL=https://ntfy.sh/<ce-topic>`.
- **Discord/Slack** : `NOTIFY_WEBHOOK_URL=<url du webhook>`.
- **Telegram** : `TELEGRAM_BOT_TOKEN` + `TELEGRAM_CHAT_ID`.

---

## 5. Déploiement

Le hub doit être joignable en **HTTPS public** (CUTTINX et votre téléphone l'appellent) et garder ses fichiers
(dossier `/data` persistant). Un seul processus suffit (le worker tourne dans l'application).

### Option A — VPS avec Docker (recommandé)
```bash
git clone <ce dépôt> && cd <dépôt>/video-hub
cp .env.example .env && nano .env
docker compose up -d --build
```
HTTPS automatique avec Caddy (`/etc/caddy/Caddyfile`) :
```
hub.mondomaine.fr {
    request_body {
        max_size 2GB
    }
    reverse_proxy localhost:8000
}
```
Mise à jour : `git pull && docker compose up -d --build`. Sauvegarde : le dossier `video-hub/data/`.

### Option B — Railway / Render / Fly.io
Déployez le dossier `video-hub` avec son `Dockerfile`, **ajoutez un volume persistant monté sur `/data`**, renseignez les
variables d'environnement, exposez le port 8000 et mettez l'URL fournie dans `PUBLIC_BASE_URL`.
Une seule instance (pas de mise à l'échelle horizontale : la file SQLite est locale).

### Option C — Chez vous
Lancez le conteneur sur un PC/NAS et exposez‑le avec **Cloudflare Tunnel** (`cloudflared tunnel --url http://localhost:8000`)
ou Tailscale Funnel, puis utilisez l'URL obtenue comme `PUBLIC_BASE_URL`. Pratique pour la voie C (Syncthing local).

---

## 6. Utilisation au quotidien

1. Ouvrez le hub sur le téléphone (ajoutez‑le à l'écran d'accueil).
2. Touchez la zone de dépôt, choisissez une ou plusieurs vidéos.
3. Cochez **Performance** (street workout / calisthenics → Reel 9:16 dynamique) ou **Vidéo normale**
   (montage posé, format 9:16 ou 16:9).
4. Choisissez **Dès que c'est prêt** ou **Heure de forte audience**. Facultatif : notes (« ralenti sur la 2e rep ») et musique.
5. **Envoyer.** C'est tout : la vidéo avance seule dans le tableau de bord jusqu'à « Publiée », avec le lien du Reel.

Dans la fiche d'une vidéo vous pouvez, si vous le souhaitez : modifier la légende avant publication, publier tout de
suite une vidéo programmée, demander un **nouveau montage avec des retouches**, relancer une vidéo en erreur.

---

## 7. Itérer le style avec Claude

- **Guide de style** : la section « Style des montages » du tableau de bord édite les consignes lues par Claude à chaque
  montage (une pour les perfs, une pour les vidéos normales). Les versions par défaut sont dans `styles/`.
- **Retouches ciblées** : « Refaire le montage avec ces retouches » renvoie la vidéo à Claude avec vos remarques ; il
  remplace le projet CUTTINX existant.
- **En parallèle dans Claude** : comme les projets s'appellent `HUB-<id>` et restent dans CUTTINX, vous pouvez ouvrir
  une conversation Claude avec le connecteur CUTTINX, demander « regarde le projet HUB‑xxxx et rends les transitions plus
  nerveuses », puis reporter ce qui vous plaît dans le guide de style pour que les prochains montages en profitent.

---

## 8. Erreurs, relances et notifications

- Chaque étape échouée est **relancée automatiquement** avec un délai exponentiel (`RETRY_BASE_SECONDS` × 1, 2, 4…),
  jusqu'à `MAX_ATTEMPTS` (défaut 4). Ensuite : statut **Erreur** + notification, et bouton **Relancer**.
- L'attente du MP4 rendu ne compte pas comme un échec ; au‑delà de `RENDER_WAIT_HOURS` sans fichier, la vidéo passe
  en erreur et vous êtes prévenu.
- L'état est persistant : après un redémarrage, chaque vidéo reprend à son étape. La publication est protégée contre
  les doublons (le conteneur Instagram est mémorisé et son statut vérifié avant de republier).
- Notifications : échec définitif (toujours), programmation et publication réussie (`NOTIFY_ON_SUCCESS`),
  chaque relance (`NOTIFY_ON_RETRY`).
- L'historique détaillé (outils CUTTINX appelés, erreurs, légende…) est dans la fiche de chaque vidéo.

---

## 9. Référence API

Authentification HTTP Basic sauf mention contraire.

| Méthode | Route | Description |
|---|---|---|
| `POST` | `/api/videos` | Dépôt multipart : `files` (1..n), `category` (`performance`/`normal`), `target_aspect`, `publish_mode` (`now`/`scheduled`), `notes`, `music` |
| `GET` | `/api/state` | Liste des vidéos, rendus non attribués, réglages manquants |
| `GET` | `/api/videos/{id}` | Détail + historique |
| `POST` | `/api/videos/{id}/retry` · `/reedit` · `/publish-now` · `/render` | Relancer · nouveau montage (`{"feedback": "..."}`) · publier maintenant · envoyer le MP4 |
| `PUT` | `/api/videos/{id}/caption` | Modifier la légende (`hook`, `text`, `hashtags`) |
| `DELETE` | `/api/videos/{id}` | Supprimer |
| `POST` | `/api/renders/inbox` | **Jeton** `X-Upload-Token` : réception d'un MP4 rendu (`file`, `video_id` facultatif) |
| `GET/PUT` | `/api/styles/{performance\|normal}` | Lire / modifier le guide de style |
| `GET` | `/files/{raw\|music\|render}/{id}/{signature}/{nom}` | **Lien signé** pour CUTTINX / Instagram |
| `GET` | `/healthz` | Santé |

---

## 10. Limites connues

- **Retour du rendu** : tant que CUTTINX ne fournit pas d'URL du MP4, une des voies B/C (section 2) doit être en place.
- **Un téléphone = un montage à la fois** : le hub sérialise les montages ; les autres étapes tournent en parallèle.
- **Téléphone connecté** : CUTTINX doit être ouvert avec « Connecté à Claude » pendant le montage.
- **Programmation** : l'API Instagram ne propose pas de publication différée pour les Reels ; c'est le hub qui publie à
  l'heure choisie, il doit donc rester allumé.
- **Mode direct** : les formats de réponse de certains outils CUTTINX (`analyze_music`, `get_render`, `create_project`)
  sont lus de façon tolérante ; si une version de CUTTINX change ces formats, préférez le mode `claude`.
