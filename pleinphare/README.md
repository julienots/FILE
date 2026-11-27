# 🚗 PLEIN PHARE — by SuperEssence Studio

**Toute ta musique, sur la route.** Lecteur de musique Android pensé pour la voiture : toutes les musiques du téléphone, classées par date, avec de très gros boutons.

<p align="center"><img src="branding/pleinphare-icon-512.png" width="160" alt="Logo Plein Phare"/></p>

- **APK** : [`release/PleinPhare-SuperEssence.apk`](release/PleinPhare-SuperEssence.apk) (Android 8.0+, signé, 2 Mo, hors connexion)
- Logo : un phare de voiture dont les faisceaux sont les barres d'un égaliseur.

## Fonctions
| | |
|---|---|
| Chargement | Intro animée « SuperEssence. » (lettre par lettre, reflet, « STUDIO présente »), puis les phares de Plein Phare s'allument. Toucher pour passer. |
| Bibliothèque | Toute la musique du téléphone (mémoire, carte SD, téléchargements) via MediaStore, **classée par date d'ajout** et regroupée par jour (« Aujourd'hui », « Hier », « Jeudi 12 novembre »…). Tri récentes ↔ anciennes. Mise à jour automatique quand tu ajoutes des morceaux. Notes vocales WhatsApp, enregistrements et sonneries sont exclus. |
| Voiture | Lignes de 78 dp, textes grands, boutons Tout lire / Aléatoire / Tri, mini-lecteur géant, lecteur plein écran avec bouton lecture de 112 dp, mode paysage, option **écran toujours allumé**, retour en haut. |
| Lecture | Arrière-plan et écran éteint, notification et écran verrouillé, **commandes au volant et Bluetooth**, pause automatique quand le Bluetooth se déconnecte, aléatoire, répétition (tout / un), recherche sans accents, pochettes. |
| Reprise | La file et la position sont mémorisées : appuyer sur « lecture » dans la voiture reprend là où tu étais, même appli fermée. |
| Android Auto | Navigation : Lecture aléatoire, Par date, Par mois. (Appli installée hors Play Store : activer « Sources inconnues » dans les paramètres développeur d'Android Auto.) |

## Compiler
```bash
echo "sdk.dir=/chemin/android-sdk" > local.properties
./gradlew assembleRelease      # app/build/outputs/apk/release/app-release.apk
./gradlew testDebugUnitTest    # tests + captures dans app/build/screenshots
```
La clé `keystore/pleinphare-release.jks` est une clé de projet ; remplace-la (variables `PP_KEYSTORE*`) avant une publication Play Store.

**Non vérifié ici** : pas d'émulateur ni de téléphone dans l'environnement de compilation. Les écrans ont été rendus sous Robolectric ; la lecture réelle, Bluetooth et Android Auto restent à confirmer sur appareil.
