# 🎮 DOUBLE DESTIN — SuperEssence Studio

Deux jeux sans rapport réunis dans une seule application Android, entièrement en français et hors connexion.

- **APK** : [`DoubleDestin-SuperEssence.apk`](DoubleDestin-SuperEssence.apk) (Android 5.0+, signé)
- Lancement : écran studio animé « SuperEssence. » → logo du jeu → menu.

## 🧬 UNE VIE — simulateur de vie
Naissance (aléatoire ou personnalisée, 15 pays), famille, frères et sœurs, signe astrologique. Année par année : école (maternelle → lycée, brevet, bac avec mentions), 25 filières d'études (médecine, droit, ingénieur, Sciences Po…), prêts étudiants et bourses, 50 métiers avec échelons et promotions, entretiens d'embauche, augmentations, licenciements, retraite. Carrières célèbres (acteur, musicien, footballeur, écrivain, influenceur), carrière politique jusqu'à Président de la République. Relations (parents, fratrie, amis, collègues, couple, fiançailles, mariage, divorce, enfants, adoption, animaux), plus de 40 événements à choix, maladies et soins, chirurgie, vacances, boîte de nuit, casino (blackjack, roulette, machine à sous), loterie, réseaux sociaux, permis, émigration, immobilier, voitures/yachts/jets, bourse, crédits, crimes, procès, prison (évasion, émeute, libération conditionnelle), mort, rubans de fin de vie, cimetière et héritage générationnel.

## 🌍 EMPIRE MONDIAL — grande stratégie
Carte du monde réelle découpée en **3 306 provinces/régions** (Natural Earth), **relief ombré** issu de vraies données d'altitude, fleuves, lacs, **6 400 villes**. 199 nations jouables. Troupes par province, recrutement, déplacements, attaques terrestres et débarquements navals, terrain (plaine/collines/montagnes), constructions (bases militaires, forts, usines, ports, bases aériennes, universités, hôpitaux, silos nucléaires, bouclier antimissile), frappes aériennes et nucléaires, technologies, impôts et budget, décrets, élections, coups d'État, révoltes, diplomatie (alliances, OTAN/OTSC au départ, commerce, pactes, vassaux, annexions, ultimatums, sanctions), espionnage, IA qui fait la guerre et la paix, événements mondiaux, actualités, victoire par hégémonie.

## Compiler
Outils : `aapt`, `dalvik-exchange`, `zipalign`, `apksigner` (paquets Ubuntu) et un `android.jar` (API 34).
```
ANDROID_JAR=/chemin/android.jar ./android/build.sh
```
Données de la carte : `tools/build_relief.py` (relief) et `tools/build_world.js` (provinces, villes), à partir de Natural Earth et des tuiles d'altitude Terrarium. Icônes : `tools/make_icons.js`.

Crédits : Natural Earth (domaine public), Terrarium/Mapzen (AWS Open Data), world-countries (ODbL), polices Fredoka et Nunito (OFL).
