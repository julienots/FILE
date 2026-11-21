# 🌙 NOCTÉA — by Superessence Studio

**Retrouvez le calme. Retrouvez le sommeil.**

NOCTÉA est une application Android native (Kotlin + Jetpack Compose + Media3) de méditation guidée, de respiration, d'histoires du soir et d'ambiances sonores, éditée par **Superessence Studio**.

<p align="center"><img src="branding/noctea-logo.png" width="320" alt="Logo NOCTÉA"/></p>

- **APK installable** : [`release/NOCTEA-Superessence-Studio.apk`](release/NOCTEA-Superessence-Studio.apk) (Android 8.0+, signé, ~27 Mo, fonctionne hors connexion)
- **Aperçus audio** (mixage réel de l'application) : [`branding/apercu-endormissement-profond.mp3`](branding/apercu-endormissement-profond.mp3), [`branding/apercu-histoire-cabane.mp3`](branding/apercu-histoire-cabane.mp3)

---

## Ce qui est inclus (et fonctionne sans compte, sans abonnement, sans Internet)

| Domaine | Contenu |
|---|---|
| Écran de lancement | Présentation animée lente « SuperEssence. » (lettre par lettre, ombre portée, reflet lumineux, « STUDIO · présente »), puis apparition du logo NOCTÉA. Toucher pour passer. |
| Bibliothèque | **29 séances guidées**, soit près de 4 heures de contenu : Sommeil (6), Relaxation (5), Respiration (5), Méditation (5), Histoires du soir (6), Réveil doux (2). Chaque séance a un titre, une description, une durée, une illustration et son texte. |
| Voix | Voix féminine française douce, **préenregistrée** (1 572 clips, 59 min de parole), **une seule voix pour toute l'application** (même timbre, même hauteur, même niveau d'un clip à l'autre, comptage des respirations compris), débit de méditation (≈ 155 mots/min de parole active), vraies pauses intégrées à l'audio. |
| Créer ma méditation | Générateur : 10 objectifs × 8 durées (3 à 60 min) × 10 ambiances × 6 styles × 3 intensités musicales. Il produit un script cohérent (intention, installation, respiration, scan corporel, visualisation, cœur de l'objectif, approfondissement, conclusion), affichable, avec la voix NOCTÉA, à la **durée exacte** demandée. Historique des créations, variantes, favoris. |
| Mon texte | Transforme un texte libre en séance audio : « ... » = pause, ligne vide = pause longue, `[pause 10]` = 10 s de silence. Voix de l'appareil (Android TTS, ramenée au même niveau, à la même tonalité et aux mêmes silences que la bibliothèque) ou **serveur vocal NOCTÉA** pour retrouver exactement la voix de la bibliothèque. |
| Lecteur | Lecture/pause, ±15 s, barre de progression, temps restant, minuteur de sommeil (fondu de 25 s, ou « fin de la séance »), lecture en arrière-plan et écran éteint, commandes sur l'écran verrouillé et en Bluetooth, volume voix / volume musique séparés, réglage de chaque couche d'ambiance, vitesse (×0,8 à ×1,1), favoris, reprise automatique, texte de la séance, **téléchargement en fichier audio .m4a** (voix + ambiance, dans Musique/NOCTEA), mode écran minimaliste, cercle de respiration synchronisé. |
| Ambiances | 15 boucles sans coupure : pluie douce, orage lointain, vagues, ruisseau, vent dans les arbres, vent doux, feu de cheminée, sons nocturnes, bruits blanc/rose/brun, musique ambient, piano relaxant, pads atmosphériques, univers spatial. |
| Mixeur nocturne | Jusqu'à 6 couches simultanées avec volumes indépendants, mélanges enregistrables, commandes sur l'écran verrouillé, minuteur. |
| Mon rituel | Heure du coucher, rappel par notification, étapes ordonnables (séances, créations, ambiance jusqu'à l'endormissement), lecture enchaînée. |
| Réveil doux | Alarme à l'heure exacte par jour de la semaine, écran au-dessus du verrouillage, séance du matin, répétition de 10 min. |
| Suivi | Calendrier mensuel, minutes de calme, jours de pratique, rituels accomplis. Aucune notion de série « perdue ». |
| Plus | Journal du soir (humeur, texte, gratitude), humeur du jour et suggestions, citations apaisantes, méditation aléatoire, recherche intelligente (accents, synonymes, texte des séances), séances recommandées selon l'heure, historique d'écoute, 3 thèmes, mode nuit automatique (palette chaude et atténuée le soir), profil, export et import des données en JSON. |

### Ce qui nécessite un service externe (facultatif)

| Fonction | État |
|---|---|
| **Même voix pour les textes libres** | Le fournisseur « Serveur vocal personnalisé » est implémenté : NOCTÉA envoie `POST {"text","voice","length_scale","speed"}` et attend un fichier audio (wav/mp3/ogg). `python3 tools/noctea_voice_server.py` fait tourner **le même modèle et la même chaîne studio que la bibliothèque** (l'application reconnaît l'en-tête `X-Noctea-Studio: 1` et ne retouche pas l'audio). Android n'accepte que des adresses `https://` : placez le serveur derrière un proxy HTTPS ou un tunnel. Un serveur Piper ou une passerelle vers un fournisseur premium fonctionnent aussi. **Aucune clé n'est dans le code** : l'URL et la clé sont saisies dans Profil › Voix et restent sur l'appareil. |
| Voix de l'appareil | Utilise le moteur TTS Android installé. La qualité et le timbre dépendent du téléphone (ce n'est donc **pas** la voix de la bibliothèque). NOCTÉA fige la voix pour tout le texte (jamais de repli silencieux vers une autre voix), ramène chaque fragment au même niveau (−20,5 dBFS), à la même égalisation et à des silences réguliers, et les convertit en 24 kHz comme la bibliothèque. Les voix Google « réseau neuronal » sont naturelles, d'autres le sont moins : l'application l'indique honnêtement. |
| Abonnement Premium | L'architecture est prête (`billing/Entitlements.kt` : interface `BillingGateway`, quotas gratuit/premium). **Aucun paiement n'est simulé.** La passerelle « offre de lancement » accorde tout gratuitement et l'écran Premium le dit. Pour activer Google Play Billing : implémenter `BillingGateway` avec `com.android.billingclient:billing` et créer les produits `noctea_premium_monthly` / `noctea_premium_yearly` dans la Play Console. |
| Synchronisation de compte | Pas de compte pour l'instant. Les préférences s'exportent et s'importent en JSON (Réglages › Données), dans le format qu'utiliserait une synchronisation. |

### À propos de la voix (en toute transparence)
La voix embarquée est produite avec **Kokoro-82M** (synthèse neuronale libre, Apache-2.0) et sa voix féminine française **« Siwis »** (entraînée sur le corpus SIWIS, CC-BY 4.0), puis traitée comme une production studio. Elle remplace la version précédente (Piper « Siwis ») après un diagnostic **mesuré** sur les 1 492 clips de l'ancien APK, pour supprimer ce qui s'entendait comme « une autre voix » en cours de séance, puis pour corriger les mots mal prononcés :

| Mesure (29 séances, 1 945 passages d'un clip au suivant) | Avant (Piper) | Après (Kokoro, chaîne v4) |
|---|---|---|
| Hauteur de la voix de comptage (« Inspire… un, deux… ») par rapport au récit | **−2,8 dem-tons** : plus grave, plus rapide, moins nette | **−0,4 dem-ton** : mêmes voix, hauteur et débit |
| Saut de hauteur entre deux clips voisins : médian / 90ᵉ perc. / max | 1,54 / 4,36 / **10,56** dem-tons | 1,05 / 2,62 / **7,00** dem-tons |
| Dispersion de hauteur entre clips (écart-type) | 1,66 dem-ton | 1,00 dem-ton (0,5–0,7 pour les clips de plus de 2,5 s) |
| Niveau de parole entre clips (écart-type) | 0,30 dB | 0,07 dB |
| Débit de parole active | 14,6 car/s (≈ 180 mots/min : débit de conversation) | 12,6 car/s (≈ 155 mots/min) |
| Aigus (énergie 5–10 kHz / 0,1–5 kHz ; ≈ −20 dB pour une voix naturelle) | −23 dB (voix étouffée) | −21,8 dB |

**Ce qui a changé dans la production** (`tools/`) :
- **Un seul moteur, déterministe** : le même texte donne exactement la même voix (Piper tire un bruit aléatoire à chaque rendu). Mesuré sur 100 phrases réelles rendues telles quelles : dispersion de hauteur entre phrases 0,27 dem-ton (Kokoro) contre 1,22 (Piper), dispersion du niveau 0,30 dB contre 1,06 dB, dispersion du timbre (pente spectrale) 0,48 contre 1,06 dB/octave.
- **Phrases rendues d'un seul tenant, puis coupées aux silences naturels des « ... »** : la mélodie de « Maintenant... tu peux fermer les yeux. » reste continue, au lieu de deux fragments isolés qui « repartent à zéro ». Les pauses réelles (850–1 350 ms, variables, jamais métronomiques) sont insérées après la coupe.
- **Débuts de phrase propres (amorce de contexte)** : rendue seule, une phrase courte commençait par une syllabe parasite (« Ta respiration » → « Eta respiration », « Tu es déconnecté » → « Utilisez déconnecté », « Essayons ensemble » → « et essayons en stem bleu »). Chaque phrase est donc rendue précédée de l'amorce « Tout doucement... », coupée ensuite au creux qui la suit. Mesuré sur 18 phrases difficiles (Whisper *medium*) : 49 % d'écart sans amorce, 4 % avec.
- **Fins de phrase propres (sans ponctuation finale)** : le point ou les « ... » en fin d'énoncé faisaient ajouter au moteur un petit son après le dernier mot (« fois » → « foisse », « poings » → « poingues », « temps » → « tangles », « toi » → « toire »). Le texte envoyé au moteur n'a plus de ponctuation finale (le point d'interrogation est conservé). Mesuré sur 33 phrases : mots finaux en écart 19 avec la ponctuation, 9 avec une virgule, 3 sans.
- **Un seul timbre pour le comptage** : les nombres isolés (un… dix) et les consignes (Inspire, Retiens, Expire, Pause) sont dits par des clips dédiés, calés sur la hauteur du récit (PSOLA : le timbre ne bouge pas), partout où ils apparaissent.
- **Phonétique corrigée mot par mot** (`RESPELL`, `IPA_FIX`, `FR_FIX` dans `tools/voice_engine.py`, testées sur tout le corpus par `tools/test_voice_tools.py`). espeak-ng bascule vers l'anglais sur « dos », « rythme », « assieds », « t'assieds », « stress », « mail » (et laisse des balises que Kokoro prononce comme des syllabes parasites). Il lit aussi mal : **« sens »** (verbe sentir, 27 phrases : lu comme le nom /sɑ̃s/, « essence »), « ressens », « floues », « joues », « dénouent » (lus sans leur voyelle), « apesanteur », « lucioles », « cosmos », les mots en « -ose » (o fermé), « montagnes/éloignent/éteignent/accompagner » (« gn »), « hamac » (liaison), « dehors », « moelleux », « accueilli ».
- **Contrôle automatique de chaque phrase** : transcription Whisper *small* comparée au texte voulu et écart de hauteur avec le reste du corpus ; une phrase douteuse est refaite avec des variantes (débit, ligne de style, amorce, rendu fragment par fragment) et on garde la meilleure. Sur les 870 phrases, de la première version Kokoro à l'actuelle : écart de transcription médian 9,1 % → 0 %, moyen 10,3 % → 4,4 %, phrases à plus de 30 % d'écart 27 → 14 (surtout des homophones). **Revue fine** (`tools/voice_verify.py`) : Whisper *medium*, mot par mot, **sans compter les homophones** (même prononciation, comme prend/prends ou pierres plates/pierre plate) ; pour les phrases encore en écart, `--fix` note chaque variante et garde la meilleure dans `tools/voice_choices.json` (36 + 5 choix : de 1 à 4 mots en écart à 0 dans la grande majorité des cas).
- **Chaîne studio identique pour tous les clips** (`voice_studio.py`) : passe-haut 70 Hz, chaleur de proximité, aigus ramenés au niveau d'une voix naturelle (le brut du modèle est ≈ 5 dB plus brillant), de-esser par bande, compression douce, **vraie pièce** (réponse impulsionnelle fixe, RT60 ≈ 0,3 s) à la place des deux échos à 23/37 ms de l'ancienne chaîne qui coloraient la voix comme un filtre en peigne, niveau commun −20,5 dBFS, limiteur à genou doux, queue de réverbération conservée. La respiration est passée dans la même « pièce ». Opus 36 kb/s (28 kb/s auparavant).

C'est une voix douce et naturelle pour une synthèse libre, mais **ce n'est pas une comédienne enregistrée**, et sa cohérence est **mesurée**, pas « écoutée » : les chiffres ci-dessus ne remplacent pas une écoute attentive sur téléphone. Il reste des mots isolés que le contrôle automatique signale (`tools/voice_review.txt`, surtout des homophones que Whisper confond : « silencieuses/silencieuse »). Pour aller plus loin, il suffit de remplacer les fichiers `assets/voice/*.ogg` par des enregistrements humains, ou de régénérer le contenu avec un autre moteur (voir plus bas) : l'application n'a pas à être modifiée. Durée totale de voix : 59 min (52 min avant) ; les 29 séances durent 244 min au total (237 avant, soit +3 %).

**« Mon texte »** : la voix de l'appareil est une autre voix (celle du téléphone). NOCTÉA la fige pour tout le texte, ramène chaque fragment au même niveau, à la même égalisation et à des silences réguliers, et la convertit en 24 kHz comme la bibliothèque (plus de reconfiguration de la piste audio entre un fragment et un silence). Pour obtenir **exactement la voix de la bibliothèque**, lancez `tools/noctea_voice_server.py` (voir plus haut). La version embarquée sur le téléphone n'a pas été faite : elle demanderait espeak-ng (licence GPL) et ≈ 90 Mo de modèle dans l'APK, sans appareil pour la tester ici.

---

## Architecture

```
app/src/main/java/com/superessence/noctea/
├── NocteaApp.kt / MainActivity.kt
├── data/        Catalog (catalogue embarqué), UserStore (persistance locale JSON), Sounds, Discovery (recherche, recommandations)
├── generator/   MeditationGenerator : assemblage du script et calcul exact des silences (Kotlin pur, testé)
├── audio/       AudioEngine (lecteur Media3, minuteur, reprise, historique), NocteaMediaSourceFactory
│                (timeline voix + silences → une seule source continue), AmbienceMixer (boucles multiples),
│                PlaybackService (MediaSessionService : arrière-plan + écran verrouillé), AudioExporter (.m4a),
│                Resampler (sinc fenêtré : l'export garde le timbre de la lecture en direct)
├── tts/         VoiceProvider (architecture modulaire) : voix embarquée, SystemTtsProvider (voix figée pour tout
│                le texte), RemoteHttpTtsProvider ; TextComposer ; VoiceFinisher (niveau, tonalité, silences, 24 kHz)
├── notify/      Rappels du rituel, réveil doux, redémarrage
├── billing/     Entitlements / BillingGateway (prêt pour Google Play Billing)
└── ui/          Thème, composants (ciel étoilé animé, logo, illustrations vectorielles), écrans
tools/
├── build_content.py   Production vocale v4 : scripts → phrases rendues d'un tenant → coupe aux silences → contrôle
│                      Whisper → chaîne studio → Opus + catalog.json
├── voice_engine.py    Moteurs (Kokoro par défaut, Piper en option), phonèmes français, coupe aux silences
├── voice_studio.py    Chaîne studio identique pour tous les clips (égalisation, de-esser, compression, pièce, niveau)
├── voice_qa.py        Mesures : hauteur, niveau, timbre, débit, sauts entre clips voisins
├── voice_verify.py    Revue fine de la prononciation : Whisper medium, mot par mot, sans les homophones ; --fix choisit la meilleure variante
├── voice_choices.json Variantes retenues par la revue fine (clé de phrase → variante)
├── fetch_models.py    Télécharge Kokoro, Whisper (et Piper en option) dans tools/models/
├── noctea_voice_server.py  Serveur HTTP : la voix de la bibliothèque pour « Mon texte »
├── test_voice_tools.py     Tests de la chaîne vocale (sans modèle)
├── content/bank.py    Banque de phrases du générateur (~360 phrases)
├── content/library.py Scripts des 29 séances + citations
├── build_sounds.py    Synthèse procédurale des 15 ambiances (boucles parfaites)
├── make_icons.py      Logo et icônes (vectoriel Android + PNG)
└── render_preview.py  Aperçus MP3 d'une séance
```

**Moteur audio.** Une séance est une *timeline* de clips vocaux et de silences. `NocteaMediaSourceFactory` la transforme en un seul `ConcatenatingMediaSource2`, si bien qu'une séance est un seul élément pour le lecteur, la notification et l'écran verrouillé, avec une durée et un déplacement exacts. Les silences viennent d'un fichier Opus silencieux découpé, au même format que la voix, ce qui évite les coupures. Les ambiances sont des lecteurs en boucle indépendants qui suivent le lecteur principal (lecture, pause, fondu du minuteur). Le mixeur seul s'appuie sur un « silence porteur » pour garder les commandes sur l'écran verrouillé.

---

## Compiler l'application

Prérequis : JDK 17 ou plus récent, Android SDK (plateforme 35, build-tools 35.0.0).

```bash
echo "sdk.dir=/chemin/vers/android-sdk" > local.properties
./gradlew assembleRelease          # → app/build/outputs/apk/release/app-release.apk
./gradlew testDebugUnitTest        # 41 tests (contenu, générateur, moteur audio, voix de l'appareil, rééchantillonnage, 18 écrans, parcours)
python3 tools/test_voice_tools.py  # 15 tests de la chaîne vocale (niveau, coupes, amorce, queue de pièce, phonétique de tout le corpus)
./gradlew testDebugUnitTest --tests '*ScreenshotTest*' -Pscreenshots=true   # captures dans app/build/screenshots
```

**Signature.** `keystore/noctea-release.jks` est une clé de projet, fournie pour que les builds soient reproductibles et que les mises à jour s'installent par-dessus. **Avant une publication sur le Play Store, créez votre propre clé** et passez-la par les variables `NOCTEA_KEYSTORE`, `NOCTEA_KEYSTORE_PASSWORD`, `NOCTEA_KEY_ALIAS` et `NOCTEA_KEY_PASSWORD` (ou utilisez Play App Signing).

## Modifier les contenus

```bash
pip install numpy scipy soundfile onnxruntime kokoro-onnx praat-parselmouth faster-whisper jiwer   # ffmpeg requis
python3 tools/fetch_models.py                       # Kokoro (voix ff_siwis) + Whisper → tools/models/
python3 tools/build_content.py                      # régénère voix + catalogue (≈ 1 h sur 4 cœurs, reprenable : tout est mis en cache)
python3 tools/voice_qa.py app/src/main/assets       # mesure la cohérence de la voix du résultat
python3 tools/voice_verify.py --min-wer 0.10        # revue mot par mot (Whisper medium : python3 tools/fetch_models.py --with-medium)
python3 tools/build_sounds.py app/src/main/assets/sounds   # régénère les ambiances
python3 tools/make_icons.py                         # régénère logo et icônes
```

- **Ajouter une séance** : ajoutez un `dict(...)` dans `tools/content/library.py`. La syntaxe des scripts est documentée en tête du fichier : `|6` pour une pause, `~30` pour un silence, `@cycles 4 7 8 0 6 count` pour une respiration, `@pool body 0-10 5` pour reprendre des phrases de la banque.
- **Enrichir le générateur** : ajoutez des phrases dans `tools/content/bank.py`. L'ordre des listes définit la progression.
- **Changer de voix** : `--engine piper --model tools/models/piper/fr_FR-siwis-medium.onnx` reproduit l'ancienne voix (`python3 tools/fetch_models.py --with-piper`) ; pour un autre moteur, ajoutez une classe dans `tools/voice_engine.py` (une méthode `render(texte, débit, décalage) → audio 24 kHz`). Un cache évite de resynthétiser les phrases inchangées.
- **Un mot est mal prononcé ?** `tools/.cache/voice_v4/kokoro/rapport.txt` liste les phrases que le contrôle Whisper juge douteuses. espeak-ng (le phonétiseur) bascule vers l'anglais sur quelques mots français ; la table `FR_FIX` de `voice_engine.py` donne la bonne prononciation et le lancement signale tout mot balisé inconnu.

## Tests et vérifications effectués
- 41 tests Android automatisés (+ 13 captures d'écran à la demande) et 8 tests de la chaîne vocale, tous verts : intégrité du catalogue (chaque clip existe, chaque timeline totalise sa durée), générateur (600 combinaisons, durée exacte à la milliseconde, cohérence et déterminisme), découpage des pauses, recherche, moteur audio réel sous Robolectric (durée de la timeline concaténée = durée du catalogue, mixeur, file du rituel, minuteur), rendu de 18 écrans sans plantage, parcours « générer puis écouter » et « minuteur », écran de lancement, finition des voix de l'appareil (même niveau pour un fragment faible et un fort, égalisation conforme au studio, WAV 24 kHz mono), rééchantillonnage sans repliement ni images.
- APK release minifié (R8), signé (schéma v2), vérifié avec `apksigner` et `aapt2`.
- **Non vérifié ici** : aucun émulateur ni téléphone n'était disponible dans l'environnement de compilation (pas de virtualisation matérielle). L'APK n'a donc pas encore été installé sur un appareil réel ; l'écoute réelle (décodage Opus, écran verrouillé, alarmes) reste à confirmer sur téléphone.

---

*NOCTÉA est un outil de bien-être et ne remplace pas un avis médical.*

**Une création Superessence Studio.**
