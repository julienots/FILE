# VOXÉA — générateur de dialogues avec ma voix (design)

## Intention
Transformer un script de dialogue (souvent écrit par Claude) en fichiers audio, un MP3 par réplique,
en n'utilisant QUE la voix de l'utilisateur : il lit tout le script dans un prompteur, l'app enregistre
chaque réplique séparément, puis transforme sa voix selon le personnage (femme, homme, enfant…).

## Objectifs
- Importer un .txt (ou coller du texte). Format `NOM : texte` ; balises `[FEMME]`, `[HOMME]`, `[ENFANT]`,
  `[ÂGÉ]`, `[ROBOT]`, `[NARRATEUR]`/`[MA VOIX]` sur la ligne ou dans le nom → voix proposée automatiquement.
- Une voix par personnage, modifiable réplique par réplique ; réglages fins hauteur/timbre, aperçu.
- Mode prompteur : réplique en gros, suivante en aperçu, enregistrement continu découpé par réplique ;
  toucher l'écran ou bouton volume = suivant ; refaire / réécouter une réplique.
- Transformation hors ligne sur l'appareil (hauteur + formants séparés).
- Option : conversion IA ElevenLabs (speech-to-speech) avec la clé de l'utilisateur, stockée sur l'appareil.
- Export : un MP3 par réplique `NN_PERSONNAGE.mp3` dans Musique/VOXEA/<projet>, dialogue complet en MP3,
  partage ZIP. Projets sauvegardés.
- Intro animée SuperEssence Studio (reprise de demo-demarrage-super-essence.html) puis écran de chargement du projet.

## Non-objectifs / anti-objectifs
Pas de compte, pas d'abonnement, pas de paiement simulé. Aucune clé dans le code.
Échec si : la transformation fait l'effet « chipmunk » grossier, ou si une erreur oblige à tout réenregistrer.

## Contraintes
Android 8+ (minSdk 26), Kotlin + Compose, même outillage que NOCTÉA (module Gradle séparé `:voxea`, même keystore).

## Approches considérées
- A. Transformation sur l'appareil seule : gratuit, hors ligne, voix reconnaissable comme transformée.
- B. IA seule (ElevenLabs) : très réaliste, payant, réseau obligatoire.
- C. **Choisie** : A par défaut + B en option si clé saisie. APK utile tout de suite, montée en qualité possible.

## Choix techniques (décision Claude)
- Enregistrement : AudioRecord PCM 16 bits mono 44,1 kHz, un fichier WAV par réplique.
- DSP : vocodeur de phase (STFT 2048/hop 512) avec extraction d'enveloppe spectrale par lissage cepstral :
  décalage de hauteur indépendant du déplacement des formants ; + EQ/légère saturation pour le robot.
- MP3 : encodeur LAME Java pur (jump3r, LGPL), 128 kb/s.
- ElevenLabs : POST /v1/speech-to-speech/{voice_id} (multipart audio), voix choisie par type.
