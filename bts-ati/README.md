# BTS ATI Cours

Application Android hors ligne de révision du BTS ATI (Assistance Technique d'Ingénieur), 1re et 2e année.

- **APK** : `BTS-ATI-Cours.apk` (Android 8.0+, signé)
- 11 matières, 80 chapitres, 323 questions de quiz, 403 fiches mémo, 69 exercices corrigés, 14 calculatrices
- Cours expliqués simplement (encadrés « En clair », exemples chiffrés, pièges, astuces)
- Quiz par chapitre, quiz défi, examen blanc chronométré noté /20 avec correction
- Fiches en répétition espacée (révision du jour), formulaire par matière
- Série de jours, objectif quotidien, statistiques, recherche plein texte,
  favoris, filtre 1re/2e année, mode sombre, taille du texte réglable

Le contenu est dans `assets/www/data/*.js` (un fichier par matière).

## Compiler
```bash
ANDROID_HOME=/chemin/android-sdk ./build.sh   # build-tools 35.0.0 + platform android-35
```
