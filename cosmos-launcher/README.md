# COSMOS — launcher Android TV pour Mi Box

Menu d'accueil animé avec un univers en fond (étoiles en parallaxe, nébuleuses, planète à anneaux, étoiles filantes, vitesse-lumière au lancement d'une appli).

- **APK** : `release/COSMOS-Launcher.apk` (Android 7+, Android TV / Google TV)
- Rangées : Streaming, TV gratuite & direct, Musique & médias, Système + bibliothèque « Toutes les apps »
- Télécommande : flèches = naviguer, OK = ouvrir, MENU = toutes les apps, BACK = fermer la bibliothèque
- Une appli non installée s'affiche en grisé : OK ouvre sa page sur le Play Store
- Écran de veille automatique après 45 s sans action (horloge géante + traversée de l'espace)

## Installer sur la Mi Box
1. Paramètres → Préférences de l'appareil → À propos → cliquer 7 fois sur « Build ».
2. Autoriser les sources inconnues pour l'appli qui ouvrira l'APK (gestionnaire de fichiers).
3. Copier l'APK sur la box (clé USB ou « Send Files to TV ») et l'installer.
4. Ouvrir COSMOS. Pour en faire l'accueil par défaut : `adb shell cmd package set-home-activity app.cosmos.launcher/.MainActivity`

## Compiler
`ANDROID_HOME=/opt/android-sdk gradle assembleDebug` — aperçus d'écran : `gradle testDebugUnitTest -Pshots=true` (dossier `app/build/shots`).

Les identifiants de paquets des applications sont dans `Catalog.kt` (à adapter, par ex. pour « Free TV »).
