# Paquets officiels AGR ENGINE

Sources des paquets livrés avec l'éditeur (*Plugins et paquets › En ligne › Bibliothèque officielle*).
Chaque dossier : `package.json` (nom, version, description, dépendances) et `assets/` (installé tel quel
dans les assets du projet). `tools/build_official_packages.sh` construit les `.agrpkg` et leur
`index.json` dans `editor/resources/packages/`. Les scripts sont vérifiés par le test
`official packages` (compilation et analyse statique sur l'API réelle).
