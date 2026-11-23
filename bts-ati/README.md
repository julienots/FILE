# BTS ATI Cours

Application Android hors ligne de révision du BTS ATI (Assistance Technique d'Ingénieur), 1re et 2e année.

- **APK** : `BTS-ATI-Cours.apk` (Android 8.0+, signé)
- 11 matières, 76 chapitres, 308 questions de quiz, 380 fiches mémo
- Cours expliqués simplement (encadrés « En clair », exemples chiffrés, pièges, astuces)
- Quiz par chapitre et quiz défi mélangé, fiches recto/verso, recherche plein texte,
  favoris, progression, filtre 1re/2e année, mode sombre

Le contenu est dans `assets/www/data/*.js` (un fichier par matière).

## Compiler
```bash
ANDROID_HOME=/chemin/android-sdk ./build.sh   # build-tools 35.0.0 + platform android-35
```
