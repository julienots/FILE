# Paquets d'assets et plugins de projet

Fenêtre **Affichage › Plugins et paquets** (aide : Aide › Guides › Paquets).

## Paquets (`.agrpkg`)

Un paquet regroupe des assets d'un projet (dossiers et fichiers : textures, modèles, scripts,
graphes de logique, prefabs, sons...) pour les réutiliser dans un autre projet ou les partager.

- **Créer un paquet** : nom (minuscules, chiffres, `-` `_` `.`), version, auteur, description,
  dépendances (noms d'autres paquets), éléments des assets à inclure. Le fichier
  `nom-version.agrpkg` est écrit dans le dépôt.
- **Dépôt** : un simple dossier de fichiers `.agrpkg` (par défaut `<projet>/packages`, réglable :
  dossier partagé, clé USB, dossier synchronisé). Aucun service en ligne n'est utilisé.
- **Installer** : un aperçu montre les fichiers nouveaux, les fichiers existants **différents** (remplacés
  seulement si vous cochez la case), les fichiers identiques, les dépendances manquantes (installation
  refusée) et la version déjà installée (mise à jour). La taille et l'empreinte de chaque fichier sont
  vérifiées **avant** toute écriture : un paquet abîmé n'écrit rien.
- **Paquets installés** : liste tenue dans `assets/packages.json`. **Désinstaller** montre d'abord ce qui
  sera supprimé ; les fichiers modifiés depuis l'installation sont **gardés** (sauf demande explicite),
  les dossiers vidés sont retirés, et un paquet dont un autre dépend ne peut pas être désinstallé.

Format : `AGRPKG01`, taille du manifeste (4 octets), manifeste JSON (`format: "agr.package"`, nom,
version, auteur, description, dépendances, fichiers avec taille et empreinte FNV-1a 64), puis les
octets des fichiers. Les chemins absolus, `..` et `packages.json` sont refusés.
Code : `editor/core/include/agr/editor/Package.hpp`. Tests : suite `packages` de `tests/test_editor.cpp`.

## Bibliothèque officielle

*En ligne › Bibliothèque officielle* : paquets livrés avec l'éditeur (sources dans `packages/`,
construits par `tools/build_official_packages.sh`), installés dans `assets/kits/...` :

| Paquet | Contenu |
|---|---|
| `fps-controller` | contrôleur à la première personne (clavier / souris / manette, saut, course) pour `CharacterController3D` |
| `platformer-2d` | mouvement de plateforme : accélération, saut variable, temps de coyote, saut mémorisé, chute franche |
| `follow-camera` | caméra de suivi 2D / 3D : lissage, avance, zone morte, limites, secousses (`:shake(x)`) |
| `day-night` | cycle jour / nuit : soleil, couleurs, intensité du ciel, signaux `dawn` / `dusk` / `hour` |
| `dialogue` | dialogues à embranchements en JSON (conditions, actions, variables), boîte de dialogue avec machine à écrire et boutons de choix, exemple |
| `inventory` | inventaire à emplacements, piles, transfert, valeur, sauvegarde (`agr.save`) |
| `state-machine` | machine à états (enter / update / exit, transitions, historique) et un garde d'exemple (patrouille, poursuite) |
| `health` | points de vie : dégâts, soins, invulnérabilité, mort, signaux déclarés |
| `shader-pack` | graphes de shaders modifiables : dissolution, hologramme, eau, flash et pulsation de sprite (avec leurs `.agrshader`) |

Vérifié par le test `official packages` : chaque script compile et passe l'analyse statique sur l'API
réelle, les 9 paquets s'installent, et l'inventaire, les dialogues et la machine à états sont exécutés
(piles, conditions, retours en arrière) ; les shaders du pack compilent en OpenGL, OpenGL ES et Vulkan.

## Dépôts en ligne

Onglet **En ligne** : l'adresse d'un dépôt (retenue dans le projet), *Charger l'index*, recherche,
*Installer*. Un dépôt est un `index.json` à côté de fichiers `.agrpkg`, sur **n'importe quel site
statique** (GitHub Pages, serveur web, CDN) ou un dossier (`file://`) : aucun service AGR n'est
nécessaire.

- **Publier** : `agr_editor --package-index DOSSIER --name "Ma boutique"` écrit l'index (nom, version,
  auteur, description, dépendances, taille, SHA-256 de chaque paquet) ; mettez le dossier en ligne.
  Créer un paquet en ligne de commande : `agr_editor --project P --create-package nom --include
  dossier,fichier [--version 1.2.0] [--depends a,b] --output nom.agrpkg`.
- **Installer** : les **dépendances sont résolues** (ordre d'installation calculé, déjà installées
  sautées, dépendance absente ou cycle signalés) ; tout est **téléchargé et vérifié** (taille et
  SHA-256 de l'index) avant la première installation, puis chaque paquet passe par l'installateur
  habituel (aperçu, fichiers modifiés protégés). Sans fenêtre : `agr_editor --project P
  --install-package nom --repository https://.../`. Plusieurs versions d'un paquet : la plus récente
  (ordre numérique, 1.10 > 1.9) est proposée.
- HTTPS vérifié (certificats du système), proxy respecté : voir [NETWORK.md](NETWORK.md#http-et-https-agrhttp).
- Vérifié : tests unitaires (index, ordre, cycle, absent, fichier falsifié refusé, installation de 3
  paquets dépendants) et un vrai dépôt servi en HTTP local (`sparks` installe d'abord `base`).

## Plugins

L'onglet **Plugins** liste les plugins du moteur (intégrés ou dynamiques) avec leur version, leurs
dépendances et leur état. Un plugin dynamique (voir [PLUGINS.md](PLUGINS.md)) se **décharge** et se
**recharge** sans redémarrer l'éditeur (`PluginManager::unloadDynamic`) : recompilez la bibliothèque puis
« Recharger ». Un plugin dont un autre dépend ne peut pas être déchargé ; le jeu doit être arrêté.

**Plugins du projet** : les bibliothèques `*agr_plugin*` du dossier `<projet>/plugins` peuvent être
chargées immédiatement ou à chaque ouverture du projet (case « charger à l'ouverture », enregistrée
dans `project.agrproject`, champ `plugins`).

## Limites actuelles

- Pas de signature des paquets par leur auteur (l'intégrité est vérifiée par rapport à l'index, pas l'identité de l'auteur) ; pas encore de dépôt public officiel.
- L'éditeur Android ne charge pas de plugins natifs (les bibliothèques d'un APK sont figées à la signature).
