# Focus. — by SuperEssence Studio

Application Android de révision : minuteur de concentration, musique douce, fiches de révision,
et **blocage des distractions** — pendant une session, seules la calculatrice, tes applis autorisées
(ex. ATI) et la conversation avec **Romane** restent accessibles. La session ne peut être arrêtée
que depuis Focus. (les Réglages sont bloqués eux aussi).

- **APK** : [`release/Focus-SuperEssence-Studio.apk`](release/Focus-SuperEssence-Studio.apk) (Android 8.0+, signé)
- **Aperçu** : [`captures/apercu.png`](captures/apercu.png)

## Fonctions
| | |
|---|---|
| Lancement | « SuperEssence. » lettre par lettre (ombre portée, reflet, « STUDIO »), puis le logo Focus. se dessine. Toucher pour passer. |
| Logo | Un anneau ouvert (la lentille qui se resserre) autour du point jaune SuperEssence, avec une étincelle qui s'échappe. Icône adaptative + monochrome. |
| Session | 25/45/60/90 min (±5, jusqu'à 4 h), anneau de progression, « maintenir pour arrêter », verrouillage strict facultatif, cloche + vibration à la fin. |
| Musique douce | Piano doux, Ambient, Nappes, Cosmos + ambiance (pluie, ruisseau, vagues, feu, bruit brun), deux volumes, fondu, lecture en arrière-plan. |
| Blocage | Service d'accessibilité : toute appli non autorisée est remplacée par l'écran « Pas maintenant ». Autorisés : calculatrice (détectée automatiquement), applis cochées, clavier, écran d'accueil, téléphone pendant un appel, 112. |
| Autres conversations | Accès aux notifications : celles de WhatsApp, Snap, Insta, SMS… sont effacées pendant la session. |
| Romane | Conversation SMS intégrée, filtrée sur son seul numéro, + bouton d'appel. Ses messages reçus pendant la session sont notifiés par Focus. Le contact ne peut pas être changé pendant une session. |
| Fiches | Matières, fiches recto/verso, révision avec retournement animé, répétition espacée (Leitner, 5 boîtes). |
| Suivi | Minutes du jour, série de jours, total, graphique de la semaine. |

## Installation sur le téléphone
1. Installer l'APK (autoriser « sources inconnues »).
2. Onglet **Blocage** : activer le service d'accessibilité « Focus. ».
   Sur Android 13+, si l'option est grisée : *Infos sur l'appli › ⋮ › Autoriser les paramètres restreints*.
3. Activer l'accès aux notifications, autoriser les SMS, enregistrer le numéro de Romane.
4. Cocher les applis autorisées (ex. ATI) — les applis dont le nom contient « ATI » sont pré-cochées.

## Compiler
```bash
echo "sdk.dir=/chemin/vers/android-sdk" > local.properties
./gradlew assembleRelease       # APK signé
./gradlew testDebugUnitTest     # rend les écrans dans app/build/screenshots
```
Les sons proviennent des ambiances procédurales de NOCTÉA (`../tools/build_sounds.py`). Polices : Fredoka, Nunito (OFL).

**Non vérifié ici** : pas d'émulateur dans l'environnement de compilation ; l'APK n'a pas encore tourné sur un vrai téléphone.

*Une création SuperEssence Studio.*
