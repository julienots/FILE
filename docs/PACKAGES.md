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

## Plugins

L'onglet **Plugins** liste les plugins du moteur (intégrés ou dynamiques) avec leur version, leurs
dépendances et leur état. Un plugin dynamique (voir [PLUGINS.md](PLUGINS.md)) se **décharge** et se
**recharge** sans redémarrer l'éditeur (`PluginManager::unloadDynamic`) : recompilez la bibliothèque puis
« Recharger ». Un plugin dont un autre dépend ne peut pas être déchargé ; le jeu doit être arrêté.

**Plugins du projet** : les bibliothèques `*agr_plugin*` du dossier `<projet>/plugins` peuvent être
chargées immédiatement ou à chaque ouverture du projet (case « charger à l'ouverture », enregistrée
dans `project.agrproject`, champ `plugins`).

## Limites actuelles

- Pas de téléchargement de paquets depuis Internet (dépôt local seulement) ni de signature des paquets.
- Pas de résolution automatique des dépendances (installez-les dans l'ordre indiqué).
- L'éditeur Android ne charge pas de plugins natifs (les bibliothèques d'un APK sont figées à la signature).
