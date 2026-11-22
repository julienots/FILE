# Nuit Néon

Jeu de course 3D pour mobile (navigateur ou application Android), jouable au tactile, au clavier ou à la manette.

## Nouveautés de la 2.4.1

- **Herbe du monde ouvert** : les brins sont posés sur le relief (avant, ils restaient à la hauteur de la route et disparaissaient sous les collines) ; prairie 60 % plus dense, fleurs rondes à cinq pétales.
- **Rouler dans l'herbe** : la carrosserie couche les tiges, qui restent écrasées (plus sombres, on voit leur envers) dans un sillage d'une quarantaine de mètres puis se relèvent lentement ; les roues arrachent des brins verts, de la paille et des mottes de terre, beaucoup plus en patinant ou en dérapant, avec un nuage de terre retournée.
- **Ciel** : il suit la caméra ; il ne devient plus noir quand on s'éloigne du centre de la Vallée Ouverte.

## Nouveautés de la 2.4

- **Monde ouvert : Vallée Ouverte** (Balade libre) : une vallée de 7 km sur 5 traversée par 14 km de route de campagne avec circulation. On peut quitter la route et rouler partout jusqu'à 450 m de part et d'autre : prés, champs, collines (la voiture suit le relief, penche dans les pentes, ralentit en montée et accélère en descente), forêts de feuillus et de sapins, rochers. Les 1 500 arbres et rochers sont de vrais obstacles : on rebondit dessus. Pas de clôture.
- **Quatre nouvelles ambiances**, pour tous les circuits et les balades :
  - **Aube** : soleil rasant rose-orangé, brume dorée, longues ombres, rayons de soleil.
  - **Brouillard** : on voit à peine à 100 m, lumière blanche diffuse, chaussée humide, montagnes noyées dans la brume.
  - **Blizzard** : neige poussée à l'horizontale par un vent violent à rafales, le sol et le haut des feuillages blanchissent, visibilité à 80 m, sifflement du vent.
  - **Tempête de sable** : ciel ocre, soleil voilé, grains de sable qui filent au ras du sol, rafales.
- **Rayons de soleil** : nouveau shader de lumière volumétrique, qui part du soleil et passe entre les arbres et les nuages (plus marqué à l'aube et dans le brouillard).
- Les effets (fumée, poussière, traces de pneus) naissent au bon niveau sur les collines du monde ouvert.

## Nouveautés de la 2.3

- **Arbres** : feuillage fait de milliers de petites feuilles en bouquets de teintes différentes (jeunes pousses claires, feuilles d'ombre sombres), couronne en sept masses au lieu de quatre, intérieur de la couronne dans l'ombre, translucidité à contre-jour ; troncs évasés au pied avec six branches. En graphismes élevés, 60 % d'arbres en plus (20 % en moyens).
- **Montagnes** : trois chaînes de vrai relief (crêtes, ravines, pics) éclairées par le soleil, forêts sur les pentes basses, roche plus haut, neige sur les sommets des pays froids, et perspective aérienne (plus loin = plus clair et bleuté).
- **Bas-côtés** : bande de gravier et de terre tassée derrière la glissière, qui se fond dans l'herbe.
- **Ciel** : l'horizon s'embrase du côté du soleil, voile clair juste au-dessus de l'horizon.
- **Bitume sec** : le reflet du soleil sur la chaussée sèche n'éblouit plus.

## Nouveautés de la 2.2

- **Absinthe** : nouvelle hypercar offerte, vert clair nacré, la voiture la plus détaillée du jeu : carrosserie deux fois plus fine, carbone tissé (sergé 2×2), calandre, ouïes, écope de toit et grille arrière en nid d'abeille, trois projecteurs par phare et signature lumineuse en « L », barre de feux arrière à lamelles, aileron actif col-de-cygne avec volet et jonc de Gurney, lame avant et canards en carbone, diffuseur à grandes dérives, quatre sorties d'échappement en titane bleui, jantes forgées à dix branches et écrou central, disques carbone-céramique percés, gros étriers jaunes, écrans dans l'habitacle, rétroviseurs profilés. Son propre V12 atmosphérique de 9 200 tr/min avec retours de flamme.
- **Moteurs plus graves et plus réalistes** : régimes redescendus, impulsions d'échappement plus larges, silencieux plus fermés, égalisation (grave renforcé, aigus adoucis) : la fréquence moyenne du son baisse d'un quart environ. Les cylindres sont plus inégaux (grondement), la boîte passe les rapports un peu plus tôt, et l'échappement claque au lever de pied.
- **Circuits de jour plus réalistes** : bitume sec et granuleux au soleil (avant, la route brillait comme un miroir mouillé), soleil plus franc et lumière du ciel moins écrasante (volumes plus lisibles), herbe et sol de prairie plus naturels, couleurs moins saturées, halo lumineux plus discret.
- **Voitures** : paillettes métalliques fines sous un vernis épais sur toutes les peintures ; les modèles importés ne ressemblent plus à du chrome (base moins métallique, vrai vernis).

## Nouveautés de la 2.1

- **Menu des circuits corrigé** : les cartes des circuits n'étaient plus visibles (la liste se faisait écraser par le reste du panneau). Elles s'affichent en entier, avec des filtres Tous / Ville / Nature / Spéciaux / Atelier, le nombre de circuits, la longueur et la largeur de chaque piste ; sur grand écran, deux rangées. Dans le menu principal, la radio est passée en haut à droite et ne cache plus le titre.
- **Pistes plus larges** : 42 m pour les circuits dessinés (32 m avant), de 4 à 10 m de plus pour tous les autres.
- **Quatre nouveaux circuits** (marqués ★) : Docks sous la Pluie, Jungle Émeraude, Champs de Lavande, Périphérique (54 m de large), réunis dans la nouvelle **Coupe Horizon** du Grand Prix.
- **Pluie réaliste** : des milliers de gouttes calculées par la carte graphique, étirées selon la vitesse (à l'arrêt elles tombent, lancé elles filent vers la caméra), éclairées par la ville et par les éclairs ; ronds de pluie animés et film d'eau qui coule sur la chaussée, bitume plus sombre et plus brillant, halo des lumières dans l'air humide. La pluie est aussi active en graphismes bas.
- **Son sans grésillement** : une seule sortie pour la radio et les effets, avec compresseur, limiteur et écrêtage doux (avant, les deux s'additionnaient au-delà du maximum et saturaient) ; contexte audio en mode lecture (tampons plus grands, plus de craquements quand le téléphone est occupé) ; plus aucun saut brusque de volume (passages de rapport, pompe de la grosse caisse, fin des notes, montées) ; au changement de morceau, l'ancien s'éteint en douceur.
- **Moteurs réalistes** : chaque explosion de chaque cylindre est simulée (voir Sons).
- **Radio** : trois nouvelles stations (Funk FM, Techno FM, Nuit Calme) et neuf morceaux de plus sur les autres : 50 morceaux en tout.

## Ouverture

Au lancement, le nom SuperEssence. apparaît lettre par lettre : chaque lettre tombe, s'écrase et rebondit, le point surgit en dernier, un éclat court sur le nom puis STUDIO s'affiche ; un petit son accompagne chaque lettre quand l'appareil le permet. L'écran disparaît au bout de 3,4 s (un toucher le passe). Il est dans `src/page.html` (police Fredoka, `fonts/fredoka-bold.ttf`).

## Modes de jeu

- **Course** : 24 circuits (10 en ville, 14 en pleine nature, dont 2 circuits spéciaux) et 3 circuits de l'atelier, 1 à 31 adversaires, 1 à 99 tours, 5 niveaux de difficulté jusqu'à Légende.
- **Grand Prix** : 7 coupes de 4 courses (3 tours, 11 adversaires). Chaque course rapporte des points (25, 18, 15, 12, 10, 8, 6, 4, 2, 1) ; le classement final donne un trophée d'or, d'argent ou de bronze, gardé par coupe et par difficulté, et une prime en crédits. La Coupe Tempête se court sous l'orage, la Coupe Vitesse sur quatre circuits inédits (Anneau de Vitesse, Lagune Turquoise, Glacier Bleu, Néon Central), la Coupe Horizon sur les circuits de la 2.1 (Docks sous la Pluie, Jungle Émeraude, Champs de Lavande, Périphérique). Les coupes sont décrites dans `COUPES` (`src/tracks.js`).
- **Contre-la-montre** : seul en piste, contre son record.
- **Élimination** : toutes les 20 secondes, le dernier est éliminé.
- **Course à bonus** : des caisses sur la piste donnent l'un des 21 pouvoirs (voir plus bas). Le pictogramme de l'objet tenu s'affiche à l'écran et flotte au-dessus de chaque voiture qui en porte un.
- **Foot Turbo** : match de foot en voiture, de 1 contre 1 à 3 contre 3, avec boost, saut, double saut et vol.
- **Balade libre** : Vallée Ouverte (monde ouvert de 7 km sur 5, on roule partout), Route des Prés (6 km avec circulation) ou Ligne Droite (8 km sans aucune voiture).
- **Atelier** : trois circuits à créer soi-même, entièrement réglés par des nombres.
- **Garage** : 23 voitures (dont 2 volantes et 6 hypercars suralimentées) et 4 motos à acheter, réglages à améliorer, moteur extrême.
- **Ligne de trajectoire** : en option (écran de course, Grand Prix, options, bouton LIGNE ou touche L). La meilleure ligne du circuit est dessinée devant la voiture : verte quand on peut accélérer, jaune quand il faut lever le pied, rouge quand il faut freiner. Elle est calculée pour le véhicule choisi (`src/ligne.js`).
- **Vue** : bouton VUE (ou touche C, ou Y à la manette) pour passer à la première personne.

## Circuits ajoutés

Autodrome du Lac, Forêt d'Automne (feuillages roux), Dunes d'Or, Volcan Noir (basalte, lave, braises dans l'air), Aurore Boréale (toundra de nuit sous la neige), et deux circuits spéciaux : **Station Orbitale** (gravité lunaire, sauts géants, boost renforcé) et **Mégapole 2099** (tout va 60 % plus vite). Leurs tracés sont tirés d'une graine, comme ceux de l'atelier ; les décors sont dans `src/tracks.js`.

En 2.1 : **Docks sous la Pluie** (port industriel, grues, éclairage au sodium et néons turquoise, toujours sous la pluie), **Jungle Émeraude** (forêt tropicale sous une pluie chaude, brume verte), **Champs de Lavande** (Provence au soleil couchant) et **Périphérique** (anneau urbain de six voies, le plus rapide du jeu).

## Réalisme

- **Fumée** : chaque bouffée est un petit nuage au contour irrégulier, éclairé par le haut. Les pneus fument longtemps en dérapage, au blocage des roues et au démarrage pleins gaz (les roues motrices patinent et marquent le sol) ; l'échappement lâche ses gaz au ralenti et à l'accélération, crache des flammes au lever de pied ; après un gros choc, le moteur fume un moment.
- **Eau** : sous la pluie et l'orage, des flaques se forment sur la piste (ronds de pluie, reflet du ciel). Les traverser projette deux murs d'eau et des gouttes, mouille l'objectif, freine la voiture et, au-delà de 130 km/h environ, fait flotter les pneus (aquaplanage). Sur les circuits de bord de mer, trois gués recouvrent toute la largeur de la piste.
- **Poussière** : au ras des bords, les roues soulèvent la terre, le sable ou la neige du décor ; sur le sable et la neige, la piste poudroie derrière les voitures. En forêt d'automne, des feuilles tombent.
- **Blé** : dans les champs (Balade libre), les tiges portent un épi à grains et à barbes, ondulent par rafales, se couchent sous la voiture et restent à plat dans son sillage avant de se relever lentement ; la paille vole, le champ freine davantage que l'herbe et s'entend.
- **Hypercars** : Furie GT et Absinthe (offertes), Tonnerre, Chimère, Apocalypse et Dragster Enragé ont un moteur suralimenté (champ `sur` dans `src/carlist.js`) : de 409 à 522 km/h d'origine, flammes à l'échappement à pleine charge. Un adversaire n'a jamais un moteur plus suralimenté que celui du joueur.

## Conduite

- **Mini-turbo** : pendant un dérapage, les roues crachent des étincelles bleues (0,5 s), orange (1,2 s) puis violettes (2,2 s) ; en relâchant, on reçoit une poussée d'autant plus longue. Chaque pneu laisse trois griffures parallèles au sol.
- **Aspiration** : collé derrière une voiture, on est tiré par son sillage.
- **Voitures volantes** (Nébula, Orion) : elles flottent sur des propulseurs ; le bouton VOL (Espace, LB à la manette) les fait monter jusqu'à une dizaine de mètres, au-dessus des obstacles et des autres voitures, puis elles redescendent en planant. Hors Foot et Course à bonus.
- **Flammes d'échappement** : au boost, deux jets de gaz (cœur bleu, panache orangé, disques de Mach) s'allongent derrière la voiture.

- **Dérapage** : bouton DRIFT (Ctrl ou V au clavier, Espace hors Foot et Bonus, X à la manette). L'arrière chasse, la voiture tourne plus serré en perdant un peu de vitesse, les pneus fument et laissent des traces. Chaque glissade rapporte des points, convertis en crédits ; toucher un mur fait perdre la glissade en cours.
- **Collisions** : les véhicules échangent leur élan selon leur masse (une moto pèse trois fois moins qu'une voiture), rebondissent, frottent et pivotent selon le point d'impact. Contre un mur, la vitesse perpendiculaire est renvoyée amortie. Le mur s'arrête au ras de la carrosserie (une moto passe plus près qu'une voiture). Tant que la tôle frotte : gerbe d'étincelles et de braises sur toute la longueur du flanc, poussière, flammèches à haute vitesse, rayures qui restent sur le mur, grincement métallique, caméra qui vibre, et un freinage d'autant plus fort qu'on appuie contre le mur. Deux véhicules qui se frottent font aussi des étincelles.
- **Obstacles** : réglage Aucun / Parfois / Souvent / Chaos, tirés au hasard à chaque départ. Cônes et barrières s'envolent, les caisses éclatent, les piles de pneus font rebondir, les blocs de béton arrêtent net ; les tremplins font sauter (environ 1,5 m de haut à 200 km/h) et les zones turbo accélèrent. Les adversaires les évitent.
- **Motos** : elles se couchent vers l'intérieur du virage et la caméra s'incline avec elles.

## Ambiances

Au départ de chaque course ou balade, le réglage **Ambiance** change le ciel, la lumière et la météo de n'importe quel circuit (y compris ceux de l'atelier) :

- **D'origine** : le circuit tel qu'il a été dessiné.
- **Jour** : grand soleil, y compris sur les circuits de ville.
- **Nuit** : clair de lune, phares et néons allumés (les circuits de ville sont déjà de nuit).
- **Orage violent** : ciel noir, pluie battante poussée par le vent, brouillard, éclairs qui illuminent tout le décor, foudre visible au loin, tonnerre qui arrive après l'éclair selon la distance. La piste détrempée accroche 18 % de moins.

Les transformations sont dans `ambiance()` de `src/tracks.js`.

Sous la pluie (graphismes moyens ou élevés), des gouttes glissent sur l'objectif et déforment l'image, la pluie éclabousse le sol ; à haute vitesse apparaissent des traits de vitesse et, au boost, l'image est aspirée vers le centre.

## Pouvoirs (Course à bonus)

Bombe (lancée en cloche, souffle tout dans 9 m), Tornade (remonte la piste et soulève les voitures), Traînée de feu, Drone foudroyant (file vers le premier), Rétrécisseur, Jet d'encre (taches sur l'écran), Turbo, Nitro (turbo long et plus fort), Missile, Triple missile, Flaque d'huile, Mine, Bouclier, Éclair (foudroie ceux qui sont devant), Aimant (tire vers la voiture de devant), Super saut, Onde de choc (repousse les voisins), Fantôme (traverse voitures et obstacles), Géant (écrase ce qu'il touche), Gel (verglas pour les voitures de devant), Téléporteur (75 m plus loin).

## Économie

Chaque partie rapporte des crédits (CR) : classement, difficulté, nombre de tours, buts, distance en balade, dérapages. Ils servent à :

- acheter des véhicules (de 4 000 à 75 000 CR ; l'Azur et l'Onyx sont offerts) ;
- monter un réglage d'un niveau (300 CR × niveau visé) ;
- débloquer les paliers du **moteur extrême** : 500, 1 000, 2 500, 5 000 puis 10 000 km/h. Une fois un palier acquis, la vitesse de pointe de chaque véhicule se règle librement dans le garage (au km/h près), avec un multiplicateur d'accélération de 1 à 50. Freins et adhérence suivent, et la simulation est découpée en petits pas pour ne rien traverser.

## Atelier de circuits

Trois emplacements. Le tracé est tiré d'une graine : les mêmes nombres redonnent toujours le même circuit.

| Réglage | Plage |
| --- | --- |
| Graine du tracé | 0 à 99 999 |
| Nombre de virages | 3 à 40 |
| Taille du circuit | 100 à 2 500 m |
| Irrégularité | 0 à 100 % |
| Largeur de la route | 8 à 160 m |
| Obstacles | 0 à 600 |
| Tremplins, zones turbo | 0 à 120 chacun |
| Voitures de circulation | 0 à 80 |
| Vitesse de tous les véhicules | × 0,2 à × 30 |
| Adhérence | × 0,1 à × 5 |
| Gravité | × 0,05 à × 5 |
| Hauteur des sauts | × 0,2 à × 10 |
| Puissance du boost | × 0 à × 10 |
| Brouillard | × 0 à × 6 |

Le décor se choisit parmi les 9 ambiances du jeu. « Tout au hasard » tire tous les nombres d'un coup. Modifier le tracé remet le record du circuit à zéro.

## Sons

Tout est synthétisé par le jeu (`src/audio.js`), sans fichier audio.

**Sortie** : effets et radio passent par une sortie unique (compresseur doux, limiteur, écrêtage doux suréchantillonné) : rien ne peut saturer. Le contexte audio est ouvert en mode « lecture » pour éviter les craquements, et toutes les attaques et coupures sont adoucies (2 à 5 ms) pour ne jamais claquer.

**Moteurs** : un petit processeur audio (AudioWorklet) simule chaque explosion de chaque cylindre dans le cycle de 720°, selon l'ordre d'allumage réel : V8 à vilebrequin croisé (les deux bancs reçoivent leurs impulsions de façon inégale, d'où le « glouglou » du ralenti), V8 biturbo à vilebrequin plat, V10, V12, V10 de course à 18 000 tr/min, V6 de prototype, flat-4 du buggy, quatre-cylindres de moto, bicylindre en V du roadster (allumages à 315° puis 405°). Chaque impulsion varie un peu en force (plus au ralenti qu'à haut régime), résonne dans deux lignes d'échappement (banc gauche à gauche, banc droit à droite, en stéréo) puis dans un silencieux qui s'ouvre avec la charge ; s'y ajoutent le souffle d'admission et le cliquetis des soupapes. Les réglages sont dans `PHYS` (`src/audio.js`). Si l'appareil ne sait pas faire tourner ce processeur, l'ancien moteur à oscillateurs prend le relais.

**Boîte de vitesses** : 7 rapports automatiques ; à fond, elle monte au rupteur avec une coupure d'injection et une détonation ; à mi-gaz elle passe plus tôt ; en décélérant elle rétrograde avec un coup de gaz. Le rupteur coupe l'injection par à-coups. Au départ, l'embrayage patine.

**Autour du moteur** : sifflement et souffle du turbo, soupape de décharge avec son battement, compresseur mécanique des hypercars, pétarades en grappes au lever de pied, roulement des pneus sur l'asphalte, chuintement de la route mouillée, vent, réacteur du boost, crissements (deux résonances qui tremblent), frottements de tôle, chocs avec tôle froissée, gerbes d'eau. L'adversaire le plus proche a son propre moteur : volume et aigus selon la distance, effet Doppler, placé à gauche ou à droite.

**Pluie** : crépitement de milliers de gouttes (fabriqué une fois au chargement), souffle et grondement ; sur tous les circuits pluvieux, plus fort sous l'orage, avec le tonnerre qui arrive après l'éclair selon la distance.

Pour changer le moteur d'un véhicule : champ `son` dans `src/carlist.js`.

## Radio

Un bandeau (en haut à droite dans le menu, en haut à gauche en course) affiche la station et le morceau ; ◀ ▶ changent de station, » passe au morceau suivant (R et T au clavier, croix haut et bas à la manette). Choix de la station et volume dans les options.

| Station | Style | Morceaux |
| --- | --- | --- |
| Néon FM 88.6 | synthwave | Néon Drive, Minuit Express, Rêve Chromé, Horizon Rose, Autoroute Fantôme, Vitesse Lumière, Lignes Blanches |
| Turbo FM 101.3 | trance | Turbo Cœur, Circuit Rouge, Hyperespace, Supernova, Aurore Électrique |
| Soleil FM 94.7 | house | Aube, Plage Dorée, Disco Bitume, Néon Tropical, Club au Couchant |
| Basse FM 107.9 | drum and bass | Adrénaline, Tunnel, Zone Rouge, Tempête |
| Furie FM 98.2 | rock et métal | Asphalte Brûlant, Dernier Tour, Métal Hurlant, Cylindrée, Pied au Plancher |
| Drift FM 91.1 | phonk | Cloche de Minuit, Gomme Brûlée, Dérive à Tokyo, Frein à Main |
| Hyper FM 104.4 | hardstyle | Zinzin, Rupteur, 10 000 km/h, Surcompresseur |
| Podium FM 96.0 | épique | Grand Prix, Dernier Virage, Champions, Ligne d'Arrivée |
| Funk FM 99.9 | funk et disco | Boulevard Disco, Groove de Minuit, Velours Chromé, Talons Aiguilles |
| Techno FM 102.6 | techno et acid | Acide 303, Entrepôt, Hypnose, Circuit Fermé |
| Nuit Calme 89.9 | lo-fi | Pluie sur le Pare-brise, Feux Arrière, Station-service 3 h, Retour à l'Aube |
| Ma musique | tes fichiers | ceux que tu ajoutes |

Funk FM a une basse slappée, un clavinet, des cuivres et un charleston chaloupé ; Techno FM une basse acide (filtre résonant, accents et glissés) et une grosse caisse qui gronde dans la réverbération ; Nuit Calme une batterie feutrée et chaloupée, un piano électrique et des craquements de vinyle.

Les cinquante morceaux des onze premières stations sont composés et joués en direct par `src/music.js`, sans fichier audio : chacun a sa structure (intro, couplet, refrain, pont, final, environ deux minutes), sa suite d'accords, sa mélodie, sa batterie avec roulements et cymbales, un écho, une réverbération et un effet de pompe sur la grosse caisse. À la fin d'un morceau, la station enchaîne sur le suivant.

**Ma musique** lit de vrais fichiers audio (mp3, m4a, ogg, wav, flac…) : bouton « Ajouter des morceaux » dans les options. Ils sont copiés dans la mémoire du jeu sur l'appareil et restent disponibles aux lancements suivants ; « Vider » les retire. Le jeu n'embarque aucune chanson du commerce.

## Lancer le jeu

```bash
npm install
```

```bash
npm run build
```

Le jeu est assemblé dans `dist/` ; il suffit de servir ce dossier (par exemple `python3 -m http.server` depuis `dist/`).

## Application Android (APK)

L'APK prêt à installer est dans `release/NuitNeon.apk` (Android 8.0 ou plus récent, paysage verrouillé, plein écran, fonctionne hors connexion).

Pour le refabriquer après une modification ou l'ajout d'un modèle :

```bash
ANDROID_JAR=/chemin/vers/android.jar ./android/build-apk.sh
```

Le script indique en tête les outils nécessaires. La clé de signature `android/keystore/nuit-neon.jks` n'est pas dans le dépôt : sans elle, le script ne peut pas signer, et un APK signé avec une autre clé ne s'installe pas par-dessus l'ancien (il faut d'abord désinstaller le jeu). Pour une publication sur le Play Store, il faut créer sa propre clé.

## Préparer un modèle 3D

Les modèles bruts pèsent souvent 25 à 70 Mo, bien trop pour un téléphone. `tools/prep-model.mjs` les allège (environ 30 000 triangles, textures 1024 px), les met à l'échelle, tourne l'avant vers -z et découpe les roues en pièces séparées pour qu'elles tournent :

```bash
cd tools && npm install
```

```bash
node prep-model.mjs entree.glb ../models/nom.glb '{"front":"+x","len":4.4,"type":"car"}'
```

Pour la version lointaine (utilisée quand le véhicule est loin de la caméra), refaire la même commande avec `"tris":5000` et la sortie `../models/nom_loin.glb`.

## Ajouter une voiture (modèle 3D)

1. Déposer le fichier `.glb` dans `models/`.
2. Ajouter une ligne dans `src/carlist.js` (un exemple commenté s'y trouve).
3. Relancer `npm run build`.

Le jeu s'occupe du reste : mise à l'échelle, pose au sol, roulis et tangage de la caisse, feux stop lumineux, ombre.
Si les roues sont des pièces séparées dont le nom contient `wheel`, `roue`, `tire`, `tyre`, `rim` ou `jante`, elles tournent et les roues avant braquent.
Si une pièce ou un matériau s'appelle `body`, `paint`, `carrosserie` ou `coque`, la peinture se change dans le garage.

Pour essayer un modèle sans toucher à la liste : ouvrir le jeu avec `?modele=models/fichier.glb` (ajouter `&rotY=3.14` si la voiture roule à l'envers).

## Voitures dessinées par le jeu

Dix-sept voitures (Comète, Nomade, Titan, Mirage, Éclipse, Vortex, Fulgur, Aiglon, Sirocco, les volantes Nébula et Orion, et les six hypercars) ne viennent pas d'un fichier 3D : elles sont construites par `src/procar.js` à partir de quelques tables de nombres dans `src/carlist.js` (longueur, largeur, ligne de capot, ligne d'épaule, garde au sol, position et taille des roues, forme du toit). Le jeu en tire la carrosserie avec ses passages de roues et ses ailes bombées, les jantes, les étriers, les feux, les bandes, le néon sous la caisse, et un habitacle visible à travers les vitres (sièges, planche de bord, volant). Pour en créer une nouvelle, copier une ligne `design` et changer les nombres.

## Stats

Chaque voiture a 4 stats de 1 à 10 (vitesse max, accélération, maniabilité, adhérence). Les valeurs de `src/carlist.js` sont celles de départ ; elles se règlent ensuite dans le garage, jusqu'au niveau débloqué, et sont gardées sur l'appareil. Les adversaires utilisent les mêmes réglages quand ils conduisent la même voiture (le moteur extrême, lui, ne profite qu'au joueur).

## Fichiers

- `src/tracks.js` : tracé et ambiance des circuits et de la route libre
- `src/world.js` : décors (ville de nuit, nature de jour), route réfléchissante, herbe
- `src/arena.js` : arène du Foot Turbo
- `src/fx.js` : fumée, flammes, étincelles, gouttes d'eau, poussière, traces de pneus
- `src/ligne.js` : ligne de trajectoire
- `src/cars.js`, `src/carlist.js` : voitures, import des modèles, animations
- `src/procar.js` : voitures dessinées par le jeu
- `src/custom.js` : réglages et tracé des circuits de l'atelier
- `src/music.js`, `src/audio.js` : radio et morceaux, moteurs, chocs, crissements, orage
- `src/main.js` : conduite, collisions, obstacles, pouvoirs, économie, adversaires, modes de jeu, menus
- `src/page.html` : interface
