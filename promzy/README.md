# ⚡ Promzy — by SuperEssence Studio

**Les meilleures promos, en direct.**

- **APK** : [`../release/PROMZY-Superessence-Studio.apk`](../release/PROMZY-Superessence-Studio.apk) (Android 8.0+, signé, 1,6 Mo)
- Aperçu : [`../branding/promzy-apercu.png`](../branding/promzy-apercu.png)

## Fonctions
- **Lancement** : logo « SuperEssence. » animé lettre par lettre + reflet, « STUDIO », puis l'étiquette Promzy tombe en tournant et l'éclair la frappe. Toucher pour passer.
- **Top** : classement des promos les plus chaudes (n°1 mis en avant, rangs, degrés de popularité).
- **En direct** : les nouveaux bons plans à la seconde, rafraîchis toutes les 60 s tant que l'app est ouverte, badge « NOUVEAU ».
- **Chercher** : tu écris ce que tu veux (ex. « ps5 », « airpods ») et l'app liste les meilleures promos correspondantes (recherche Dealabs + filtrage des flux, accents ignorés). Idées et historique.
- **Favoris** (gardés hors connexion), partage, ouverture de l'offre, catégories (High-tech, Jeux vidéo, Maison, Mode, Courses, Voyages, Gratuit), tirer pour rafraîchir, cache hors-ligne.

Source des données : flux RSS publics de Dealabs (`/rss/hot`, `/rss/nouveaux`, `/rss/search`).

## Compiler
```bash
echo "sdk.dir=/chemin/vers/android-sdk" > local.properties
./gradlew assembleRelease
./gradlew testDebugUnitTest -Pshots=true   # tests + captures dans app/build/screenshots
```
