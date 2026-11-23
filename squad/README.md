# SQUAD. — by SuperEssence Studio

Des mascottes viriles qui vivent sur ton écran, même appli fermée.

- **APK** : [`release/SQUAD-SuperEssence-Studio.apk`](release/SQUAD-SuperEssence-Studio.apk) (Android 8.0+, signé, ~1,3 Mo)

![Les 12 gars](branding/squad-lineup.png)
![Écrans](branding/squad-ecrans.png)
![Baston](branding/squad-baston.png)

## Ce que fait l'appli
- **Ouverture animée SuperEssence Studio** : hexagone jaune avec monogramme S, nom lettre par lettre, reflet. Toucher pour passer.
- **12 personnalités** : Tank (bodybuilder), Ragnar (viking), Vincent (le boss), Rex (biker), Sergent Kowal (militaire), Kenji (samouraï), Ace (pilote), Big Jo (bûcheron), Marco (roi du grill), Rocco (boxeur), Zéro (hacker), Duke (cowboy). Chacun a sa bio, ses traits, ses plats, son surnom pour toi et ses propres répliques.
- **Une vie** : ventre, énergie, moral, forme physique, niveau/XP, journal. Il respire (lentement en dormant), cligne des yeux, marche, mange, fait de la muscu, des pompes, prend son café, regarde son tel, baille, dort avec des « z ».
- **Calé sur tes horaires** (Mon rythme) : réveil semaine/week-end, coucher, petit-déj, déjeuner, dîner, séance de sport. Ils dorment quand tu dors et mangent quand tu manges.
- **Sur l'écran, appli fermée** : jusqu'à 8 gars en même temps se baladent partout, se parlent quand ils se croisent.
  - toucher : il parle ; double toucher : « check » ; appui long : ouvre sa fiche ;
  - glisser : tu l'emmènes où tu veux ; lâcher sur la croix en bas : il est rangé ;
  - notification : « Tout rappeler » / « Arrêter ».
- **Ils interagissent entre eux** à l'écran :
  - **bastons** : ils s'approchent, se provoquent, se mettent en garde, échangent des coups (POW!, BAM!), esquivent, jusqu'au K.O. (étoiles qui tournent). Le gagnant fait la danse de la victoire. L'issue dépend du talent de chaque perso, de sa forme physique, de son niveau et de son énergie ;
  - **discussions** (vraies répliques à tour de rôle), **checks**, **jeu du chat** (l'un court après l'autre), **muscu à deux** ;
  - les bagarreurs (Ragnar, Rocco, Rex, Tank) cherchent plus souvent la baston, les calmes discutent ; les rivaux se battent plus, les potes se font des checks ;
  - **lâche un gars sur un autre** : ça part en baston ; bouton **« Défier »** dans sa fiche ;
  - palmarès (victoires/défaites) et **relations** : meilleur pote, pote, rival, ennemi juré.
- **Style** : coupe, barbe, tenue, accessoire, couleurs de peau/cheveux/tenue/pantalon, tatouages, cicatrice.
- Réglages : taille, vitesse, bavardage, fréquence des rencontres, bastons on/off, visibles ou non pendant leur sommeil, vibrations.

## Compiler
```bash
echo "sdk.dir=/chemin/vers/android-sdk" > local.properties
./gradlew assembleRelease
./gradlew testDebugUnitTest   # rendus dans app/build/renders
```
La clé `keystore/squad-release.jks` sert aux mises à jour par-dessus ; remplace-la avant une publication Play Store.
