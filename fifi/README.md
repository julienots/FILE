# Fifi, l'assistante qui a la flemme

Une appli Android dans le style de « OK Google », sauf qu'on dit **« Dis Fifi »**.
Fifi répond avec **sa propre voix** (clonée à partir de `tools/voix_fifi.m4a`). Elle est drôle,
souvent de mauvaise foi, a régulièrement **la flemme** de répondre, et t'envoie **chercher toi-même**.

**APK :** [`release/Fifi.apk`](release/Fifi.apk) (Android 8.0 ou plus récent)

## Ce qu'elle sait faire

| Tu dis… | Fifi… |
|---|---|
| « Dis Fifi » | « Ouais ? », « Quoi encore ? », « Je dormais, mais bon. Vas-y. »… puis elle écoute ta question |
| « Dis Fifi, quelle heure il est ? » | donne l'heure (arrondie à 5 minutes, parce qu'elle a la flemme d'être précise) |
| « on est quel jour ? » | donne la date |
| « raconte une blague » | 16 blagues |
| « c'est quoi un ornithorynque ? », « pourquoi le ciel est bleu ? »… | « Va chercher toi-même, t'as des doigts non ? » et elle t'ouvre la recherche Google |
| « allume / éteins la lampe » | allume ou éteint la lampe torche |
| « mets de la musique », « ouvre YouTube », « prends une photo », « appelle… » | ouvre l'appli correspondante (en râlant) |
| « quel temps il fait ? » | « Regarde par la fenêtre, c'est gratuit. » puis ouvre la météo |
| « pile ou face », « lance un dé » | joue (avec effort) |
| bonjour, ça va, qui es-tu, quel âge as-tu, je t'aime, t'es nulle, t'es belle, merci, au revoir, j'ai faim, je suis fatigué, je m'ennuie, chante, tais-toi… | a une réplique (plusieurs, tirées au hasard) pour chaque |

Et n'importe quand, elle peut simplement refuser : « Non. J'ai la flemme. », « Message reçu. Message ignoré. »,
« Je suis en grève. Revendication : plus de sieste. »… Le **niveau de flemme** se règle dans l'appli
(de « Motivée » à « Flemme absolue »).

Il y a environ **260 répliques enregistrées** au total (voir `tools/phrases.py`).

## Utilisation

1. Installe l'APK (autorise « sources inconnues » si le téléphone le demande), puis autorise le **micro**.
2. **Bouton « Parler à Fifi »** : elle écoute ta question tout de suite.
3. **« Écoute permanente « Dis Fifi » »** : Fifi écoute en continu, même appli fermée ou écran éteint
   (une notification reste affichée, c'est obligatoire sur Android). Dis « Dis Fifi » seul puis ta question,
   ou tout d'un coup : « Dis Fifi, quelle heure il est ? ».
4. **Couper le « bip »** : la reconnaissance vocale d'Android fait un bip à chaque relance. Cette option coupe
   le son des médias et des notifications pendant que Fifi attend son nom (le son revient quand elle parle).
5. **Appui long sur Accueil** : dans Réglages › Applications par défaut › Assistant numérique, choisis Fifi
   (selon le téléphone, l'option peut ne pas être proposée).

Pour comprendre ce que tu dis, Fifi utilise la reconnaissance vocale du téléphone (l'appli Google doit être
installée, ce qui est le cas sur presque tous les Android). Quand l'appli est en arrière-plan, Android interdit
d'ouvrir une autre appli : Fifi met alors une notification « Touche ici » à la place.

## La voix

Les répliques ont été générées **une fois pour toutes** sur ordinateur, puis embarquées dans l'APK :
l'appli n'a pas besoin d'Internet pour parler.

- Moteur : **Chatterbox multilingue** (Resemble AI, licence MIT), clonage de voix à partir de l'enregistrement
  `tools/voix_fifi.m4a` (nettoyé : débruitage, suppression des blancs, normalisation).
- Chaque phrase est réécoutée automatiquement par Whisper ; si elle est mal prononcée, elle est régénérée.
- Les fichiers audio comportent un filigrane inaudible (PerTh) ajouté par Chatterbox, qui signale une voix de synthèse.

Une voix clonée n'est pas parfaite : certaines phrases peuvent sonner un peu bizarre. Pour en refaire une ou en
ajouter, modifie `tools/phrases.py` puis :

```bash
pip install chatterbox-tts "setuptools<81"
python3 tools/build_voice.py --ref tools/voix_fifi.m4a
```

Seules les phrases nouvelles ou modifiées sont générées (cache dans `tools/.cache`).

## Compiler

JDK 17+, Android SDK (plateforme 35, build-tools 35.0.0).

```bash
echo "sdk.dir=/chemin/vers/android-sdk" > local.properties
./gradlew assembleRelease     # -> app/build/outputs/apk/release/app-release.apk
./gradlew testReleaseUnitTest # tests du « cerveau » (compréhension des phrases)
```

## Code

```
app/src/main/java/com/fifi/assistant/
├── Brain.kt        compréhension (mots-clés) + choix de la réplique + flemme aléatoire
├── FifiEngine.kt   écoute « Dis Fifi » en boucle, écoute de la question, réponse, actions (lampe, applis…)
├── FifiService.kt  service au premier plan pour l'écoute permanente
├── Voice.kt        lecture des répliques (assets/voice)
├── FaceView.kt     le visage animé de Fifi (dort, écoute, parle)
└── MainActivity.kt l'écran
tools/
├── phrases.py      toutes les répliques
└── build_voice.py  génération de la voix clonée
```
