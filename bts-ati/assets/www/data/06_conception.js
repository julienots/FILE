COURS.push({
  id: "conception", name: "Analyse & conception", emoji: "📏", color: "#DB2777",
  desc: "Analyse fonctionnelle (besoin, cahier des charges, FAST, SysML), dessin technique, cotation et tolérances, ajustements, cotation GPS, CAO et dossier technique.",
  chapters: [
    {
      id: "besoin", t: "Analyse du besoin et cahier des charges", y: 1,
      html: `
<h3>La démarche</h3>
<div class="easy box">Avant de concevoir quoi que ce soit, on se demande : <b>À qui ça rend service ? Sur quoi ça agit ? Dans quel but ?</b> Puis on écrit noir sur blanc ce que le produit doit faire et avec quelles performances : c'est le cahier des charges.</div>
<h3>La bête à cornes</h3>
<ul><li>À qui rend-il service ? (l'utilisateur)</li><li>Sur quoi agit-il ? (la matière d'œuvre)</li><li>Dans quel but ? → la <b>fonction globale</b> (verbe à l'infinitif + complément).</li></ul>
<div class="ex box">Une perceuse rend service au bricoleur, agit sur le matériau, dans le but de faire des trous.</div>
<h3>Diagramme pieuvre (des interacteurs)</h3>
<p>On place le produit au centre et tous les éléments de son environnement autour (utilisateur, énergie, normes, environnement, coût, sécurité…).</p>
<ul><li><b>Fonctions principales (FP)</b> : relient deux éléments extérieurs par le produit (raison d'être).</li><li><b>Fonctions contraintes (FC)</b> : adaptation à un élément (respecter les normes, résister à l'humidité…).</li></ul>
<h3>Le Cahier des Charges Fonctionnel (CdCF)</h3>
<p>Pour chaque fonction : <b>critère</b> d'appréciation, <b>niveau</b> attendu et <b>flexibilité</b> (tolérance, classe F0 à F3).</p>
<table><tr><th>Fonction</th><th>Critère</th><th>Niveau</th><th>Flexibilité</th></tr>
<tr><td>FP1 : percer le matériau</td><td>Diamètre</td><td>3 à 13 mm</td><td>F0 (impératif)</td></tr>
<tr><td>FC1 : être alimenté par le secteur</td><td>Tension</td><td>230 V – 50 Hz</td><td>F0</td></tr>
<tr><td>FC2 : être léger</td><td>Masse</td><td>≤ 2 kg</td><td>± 10 %</td></tr></table>
<h3>Validation du besoin</h3><p>Pourquoi ce besoin existe-t-il ? Qu'est-ce qui pourrait le faire évoluer ou disparaître ? (risque de stabilité du besoin)</p>
<h3>Cycle de vie d'un produit</h3><p>Besoin → conception → fabrication → distribution → utilisation → maintenance → fin de vie (recyclage). L'<b>éco-conception</b> réduit l'impact à chaque étape (ACV = analyse du cycle de vie).</p>`,
      cards: [["Les 3 questions de la bête à cornes ?", "À qui rend-il service ? Sur quoi agit-il ? Dans quel but ?"], ["Fonction principale ?", "Relie deux éléments extérieurs via le produit (raison d'être)."], ["Fonction contrainte ?", "Adaptation du produit à un élément de son environnement."], ["Les 3 caractérisations d'une fonction dans le CdCF ?", "Critère, niveau, flexibilité."], ["ACV ?", "Analyse du Cycle de Vie (impacts environnementaux)."]],
      quiz: [
        ["La bête à cornes sert à :", ["Exprimer le besoin", "Calculer une poutre", "Câbler un moteur", "Faire un planning"], 0, "Outil d'expression du besoin."],
        ["Dans un CdCF, la flexibilité F0 signifie :", ["Très négociable", "Impératif, non négociable", "Optionnel", "Inconnu"], 1, "F0 = flexibilité nulle."],
        ["Dans le diagramme pieuvre, une fonction principale :", ["Relie le produit à un seul élément", "Relie deux éléments extérieurs par le produit", "Est une norme", "Est le prix"], 1, "C'est la raison d'être du produit."]
      ]
    },
    {
      id: "fast_sysml", t: "FAST, SADT et SysML", y: 1,
      html: `
<h3>Le diagramme FAST</h3>
<div class="easy box">Le FAST décompose une fonction en sous-fonctions jusqu'aux <b>solutions techniques</b>. En allant vers la droite on répond à « <b>Comment ?</b> », vers la gauche à « <b>Pourquoi ?</b> ».</div>
<div class="ex box">Fonction : « Déplacer le colis » → Comment ? « Convertir l'énergie électrique en énergie mécanique » → Moteur asynchrone ; « Adapter la vitesse » → Réducteur ; « Guider le tapis » → Rouleaux sur roulements.</div>
<h3>SADT / Actigramme A-0</h3><p>Une boîte avec : matière d'œuvre en entrée (gauche), matière d'œuvre + valeur ajoutée en sortie (droite), données de contrôle (en haut : énergie W, configuration C, réglage R, exploitation E), support / système (en bas).</p>
<h3>SysML (le langage actuel)</h3>
<p>SysML est un langage de diagrammes normalisé pour décrire un système. Les principaux :</p>
<table><tr><th>Diagramme</th><th>Abrév.</th><th>Montre</th></tr>
<tr><td>Cas d'utilisation</td><td>uc</td><td>Qui utilise le système et pour quoi faire (acteurs + services)</td></tr>
<tr><td>Exigences</td><td>req</td><td>Ce que le système doit respecter (id, texte, valeurs)</td></tr>
<tr><td>Définition de blocs</td><td>bdd</td><td>La structure : composition du système en blocs</td></tr>
<tr><td>Bloc interne</td><td>ibd</td><td>Les flux (énergie, matière, info) entre les blocs</td></tr>
<tr><td>Séquence</td><td>sd</td><td>Les échanges de messages dans le temps</td></tr>
<tr><td>États-transitions</td><td>stm</td><td>Les états du système et le passage de l'un à l'autre</td></tr>
<tr><td>Activité</td><td>act</td><td>L'enchaînement des actions (proche d'un algorithme)</td></tr>
<tr><td>Paramétrique</td><td>par</td><td>Les équations reliant des grandeurs</td></tr></table>
<div class="tip box">Pour bien répondre au BTS : <b>req</b> = « quoi », <b>bdd/ibd</b> = « avec quoi », <b>sd/stm/act</b> = « comment ça se comporte ».</div>
<h3>Relations dans un diagramme d'exigences</h3><p>« contain » (décomposition), « deriveReqt » (exigence déduite), « satisfy » (un bloc satisfait l'exigence), « verify » (un test vérifie), « refine » (précise).</p>`,
      cards: [["FAST : vers la droite on répond à ?", "Comment ?"], ["FAST : vers la gauche on répond à ?", "Pourquoi ?"], ["Diagramme SysML de la structure ?", "bdd (définition de blocs)."], ["Diagramme SysML des flux entre blocs ?", "ibd (bloc interne)."], ["Diagramme SysML des acteurs et services ?", "Cas d'utilisation (uc)."], ["stm ?", "Diagramme d'états-transitions."]],
      quiz: [
        ["Dans un FAST, à droite on trouve :", ["La fonction globale", "Les solutions techniques", "Le prix", "Les acteurs"], 1, "On répond à « comment »."],
        ["Le diagramme SysML qui liste ce que le système doit respecter :", ["bdd", "req", "sd", "act"], 1, "Requirements = exigences."],
        ["Pour montrer les échanges de messages dans le temps :", ["Diagramme de séquence", "bdd", "req", "par"], 0, "sd = sequence diagram."],
        ["La relation « satisfy » relie :", ["Un bloc et une exigence", "Deux acteurs", "Deux états", "Un test et un acteur"], 0, "Le bloc satisfait l'exigence."]
      ]
    },
    {
      id: "dessin", t: "Lire un dessin technique", y: 1,
      html: `
<h3>Les vues (projection européenne)</h3>
<div class="easy box">On regarde la pièce de plusieurs côtés. Vue de <b>face</b> au centre ; la vue de <b>gauche</b> se place à <b>droite</b> ; la vue de <b>dessus</b> se place <b>en dessous</b>. (C'est l'inverse de l'intuition : la vue est placée du côté opposé à celui d'où on regarde.)</div>
<h3>Les traits</h3>
<table><tr><th>Trait</th><th>Usage</th></tr>
<tr><td>Continu fort</td><td>Arêtes et contours vus</td></tr><tr><td>Interrompu fin (pointillés)</td><td>Arêtes cachées</td></tr>
<tr><td>Mixte fin (trait-point)</td><td>Axes, plans de symétrie</td></tr><tr><td>Continu fin</td><td>Cotes, hachures, lignes d'attache</td></tr></table>
<h3>Coupes et sections</h3><p>On « coupe » la pièce par un plan pour voir l'intérieur. La matière coupée est <b>hachurée</b>. On ne hachure jamais les pièces pleines coupées dans le sens de la longueur (vis, axes, clavettes, billes).</p>
<h3>Le cartouche</h3><p>En bas à droite : titre, échelle, auteur, date, format (A4, A3…), matière, numéro de plan, symbole de projection.</p>
<h3>Échelles</h3><p>1:1 grandeur réelle ; 1:2 réduction (2 fois plus petit) ; 2:1 agrandissement.</p>
<h3>Dessin d'ensemble et nomenclature</h3><p>Le dessin d'ensemble montre toutes les pièces assemblées, repérées par des numéros. La <b>nomenclature</b> liste : repère, nombre, désignation, matière, observations (référence commerciale…).</p>
<h3>Représentations normalisées</h3>
<ul><li>Filetage (vis) : trait fort pour le sommet, trait fin pour le fond du filet.</li><li>Taraudage (trou) : l'inverse.</li><li>Roulements, ressorts, engrenages : représentations simplifiées.</li></ul>
<h3>Perspectives</h3><p>Cavalière, isométrique : pour donner une idée en 3D. Aujourd'hui, la CAO fournit directement des vues 3D.</p>`,
      cards: [["Projection européenne : où se place la vue de gauche ?", "À droite de la vue de face."], ["Trait pour arêtes cachées ?", "Interrompu fin."], ["Trait pour les axes ?", "Mixte fin (trait-point)."], ["Échelle 1:2 ?", "Le dessin est 2 fois plus petit que la réalité."], ["Que hachure-t-on ?", "La matière coupée par le plan de coupe."]],
      quiz: [
        ["En projection européenne, la vue de dessus se place :", ["Au-dessus", "En dessous", "À gauche", "À droite"], 1, "Côté opposé à l'observateur."],
        ["Les axes de symétrie sont tracés en :", ["Trait fort", "Mixte fin", "Pointillés", "Ondulé"], 1, "Trait-point fin."],
        ["Une vis coupée dans sa longueur est :", ["Hachurée", "Non hachurée", "En pointillés", "Effacée"], 1, "Pièces pleines non coupées longitudinalement."],
        ["La nomenclature donne :", ["Les cotes", "La liste des pièces", "Le prix de vente", "Les couleurs"], 1, "Repère, nombre, désignation, matière…"]
      ]
    },
    {
      id: "tolerances", t: "Cotation, tolérances et ajustements", y: 1,
      html: `
<h3>Pourquoi des tolérances ?</h3>
<div class="easy box">Aucune machine ne fabrique une pièce exactement à 20,000 mm. On indique donc un <b>intervalle acceptable</b> : la tolérance. Plus elle est serrée, plus la pièce est chère à fabriquer.</div>
<h3>Cote tolérancée</h3>
<p>Ex : 20 ±0,1 → cote max 20,1, cote min 19,9, IT (intervalle de tolérance) = 0,2.</p>
<div class="f">IT = Cote max − Cote min</div>
<h3>Système ISO (ex : Ø20 H7, Ø20 g6)</h3>
<ul><li>Lettre <b>MAJUSCULE</b> = alésage (trou) ; <b>minuscule</b> = arbre.</li><li>La lettre donne la <b>position</b> de la tolérance (H : écart inférieur = 0 pour le trou ; h : écart supérieur = 0 pour l'arbre).</li><li>Le chiffre = la <b>qualité</b> (6 et 7 : précis, usinage soigné ; 11 : grossier).</li></ul>
<div class="ex box">Ø20 H7 = Ø20 avec écarts 0 / +0,021 → trou entre 20,000 et 20,021 mm.</div>
<h3>Ajustements</h3>
<table><tr><th>Type</th><th>Définition</th><th>Exemple</th><th>Usage</th></tr>
<tr><td>Avec jeu</td><td>Arbre toujours plus petit que le trou</td><td>H7/g6, H8/f7</td><td>pièces mobiles (pivot glissant)</td></tr>
<tr><td>Incertain</td><td>Jeu ou serrage selon les pièces</td><td>H7/k6</td><td>positionnement démontable</td></tr>
<tr><td>Avec serrage</td><td>Arbre toujours plus gros</td><td>H7/p6</td><td>frettage, bague de roulement tournante</td></tr></table>
<div class="f">Jeu max = Trou max − Arbre min &nbsp;&nbsp; Jeu min = Trou min − Arbre max</div>
<div class="key box">Si le jeu min est négatif, c'est du serrage. Le système « alésage normal » (H) est le plus utilisé car il est plus facile de faire varier un arbre que de multiplier les alésoirs.</div>
<h3>Chaînes de cotes</h3><p>Pour garantir une condition fonctionnelle (un jeu par exemple), on trace la chaîne des cotes qui l'influencent. Jeu max = Σ cotes max « positives » − Σ cotes min « négatives ». IT(jeu) = Σ IT des cotes.</p>
<h3>États de surface</h3><p>Ra (rugosité moyenne, en µm) : Ra 3,2 = usinage courant ; Ra 0,8 = rectifié (portée de roulement) ; Ra 12,5 = ébauche.</p>`,
      cards: [["IT ?", "Intervalle de tolérance = cote max − cote min."], ["Ø20 H7 : H majuscule désigne ?", "Un alésage (trou)."], ["Ajustement avec jeu typique ?", "H7/g6"], ["Ajustement avec serrage typique ?", "H7/p6"], ["Jeu min = ?", "Trou min − Arbre max"], ["Ra 0,8 µm ?", "Surface rectifiée, très lisse."]],
      quiz: [
        ["30 ±0,05 : l'IT vaut", ["0,05", "0,1", "30,05", "0,5"], 1, "30,05 − 29,95."],
        ["Dans « Ø25 g6 », g minuscule indique :", ["Un alésage", "Un arbre", "Une vis", "Une rugosité"], 1, "Minuscule = arbre."],
        ["Trou 20,000/20,021, arbre 19,980/19,993. Jeu min =", ["0,007", "0,041", "0,021", "−0,007"], 0, "20,000 − 19,993."],
        ["Un ajustement H7/p6 est :", ["Avec jeu", "Incertain", "Avec serrage", "Glissant"], 2, "p : arbre plus gros que le trou."],
        ["Plus la qualité IT est petite (ex : 6) :", ["Moins c'est précis", "Plus c'est précis et cher", "Rien ne change", "Moins c'est cher"], 1, "Tolérance plus serrée."]
      ]
    },
    {
      id: "gps", t: "Cotation GPS et tolérances géométriques", y: 2,
      html: `
<h3>Le principe</h3>
<div class="easy box">Les tolérances dimensionnelles contrôlent les tailles. Les tolérances <b>géométriques</b> contrôlent les <b>formes</b> et les <b>positions</b> : une surface doit être assez plane, un trou bien perpendiculaire, deux cylindres bien alignés… GPS = Spécification Géométrique des Produits (normes ISO).</div>
<h3>Le cadre de tolérance</h3><p>[symbole | valeur de la tolérance (zone) | référence(s) A, B, C]. La flèche montre l'élément tolérancé ; le triangle noir désigne la surface de référence.</p>
<h3>Les principaux symboles</h3>
<table><tr><th>Famille</th><th>Tolérance</th><th>Référence ?</th></tr>
<tr><td rowspan="4">Forme</td><td>Rectitude ⏤</td><td>Non</td></tr><tr><td>Planéité ⏥</td><td>Non</td></tr><tr><td>Circularité ○</td><td>Non</td></tr><tr><td>Cylindricité ⌭</td><td>Non</td></tr>
<tr><td rowspan="3">Orientation</td><td>Parallélisme ∥</td><td>Oui</td></tr><tr><td>Perpendicularité ⊥</td><td>Oui</td></tr><tr><td>Inclinaison ∠</td><td>Oui</td></tr>
<tr><td rowspan="3">Position</td><td>Localisation ⌖</td><td>Oui</td></tr><tr><td>Coaxialité / concentricité ◎</td><td>Oui</td></tr><tr><td>Symétrie ⌯</td><td>Oui</td></tr>
<tr><td rowspan="2">Battement</td><td>Battement simple ↗</td><td>Oui</td></tr><tr><td>Battement total</td><td>Oui</td></tr></table>
<h3>Zone de tolérance</h3>
<ul><li>Planéité 0,05 : la surface doit être comprise entre deux plans parallèles distants de 0,05 mm.</li><li>Localisation Ø0,1 : l'axe du trou doit être dans un cylindre de Ø0,1 centré sur la position théorique (cotes encadrées = cotes théoriquement exactes).</li></ul>
<h3>Exigences particulières</h3><p>Ⓔ enveloppe (la pièce doit entrer dans une enveloppe parfaite au maximum de matière), Ⓜ maximum de matière (permet un « bonus » de tolérance, utile pour les assemblages par vis).</p>
<h3>Contrôle</h3><p>Marbre + comparateur, machine à mesurer tridimensionnelle (MMT), calibres « entre / n'entre pas ».</p>`,
      cards: [["Tolérances de forme : ont-elles une référence ?", "Non."], ["Planéité 0,05 ?", "Surface entre 2 plans parallèles distants de 0,05 mm."], ["Symbole ⊥ ?", "Perpendicularité."], ["Cote encadrée ?", "Cote théoriquement exacte (pour la localisation)."], ["MMT ?", "Machine à Mesurer Tridimensionnelle."]],
      quiz: [
        ["La planéité est une tolérance de :", ["Forme", "Position", "Orientation", "Battement"], 0, "Elle ne nécessite pas de référence."],
        ["Pour imposer qu'un trou soit à sa place exacte :", ["Localisation", "Rectitude", "Circularité", "Planéité"], 0, "Tolérance de position."],
        ["Le parallélisme nécessite :", ["Une référence", "Aucune référence", "Une rugosité", "Un filetage"], 0, "Parallèle à quoi ? À la référence."]
      ]
    },
    {
      id: "cao", t: "CAO et dossier technique", y: 1,
      html: `
<h3>La CAO (Conception Assistée par Ordinateur)</h3><p>Logiciels : SolidWorks, Fusion 360, CATIA, Inventor, Creo, FreeCAD, Onshape…</p>
<h3>Démarche de modélisation d'une pièce</h3>
<ol><li>Choisir le plan d'esquisse et l'origine (en pensant aux symétries).</li><li><b>Esquisse</b> 2D <b>contrainte</b> (géométriquement : parallèle, tangent… et dimensionnellement : cotes) → elle doit être totalement contrainte (souvent noire).</li><li><b>Fonction volumique</b> : extrusion, révolution, balayage, lissage.</li><li>Fonctions d'enlèvement : enlèvement de matière, perçages (assistant de trous), congés, chanfreins, coques, répétitions.</li><li>Attribuer la matière (masse, centre de gravité automatiques).</li></ol>
<h3>Assemblage</h3><p>On insère les pièces et on ajoute des <b>contraintes d'assemblage</b> (coïncidence, coaxialité, distance…) qui correspondent aux liaisons. On peut vérifier les collisions/interférences, animer les mouvements.</p>
<h3>Mise en plan</h3><p>Génération automatique des vues, coupes, cotes, nomenclature, cartouche.</p>
<h3>Simulation</h3><p>Calcul par éléments finis (contraintes, déformations, coefficient de sécurité), simulation mécanique (vitesses, efforts), écoulement, thermique. Attention : un calcul n'est valable que si les conditions aux limites (fixations, efforts) sont réalistes.</p>
<h3>Le dossier technique</h3><ul><li>Cahier des charges / diagramme d'exigences.</li><li>Plans d'ensemble et de définition, nomenclature.</li><li>Schémas (électriques, pneumatiques, cinématiques).</li><li>Notes de calcul.</li><li>Notices (utilisation, maintenance), analyse de risques, déclaration CE.</li></ul>
<h3>Gestion des données</h3><p>Versions et indices de révision des plans (A, B, C…), PDM/PLM pour gérer le cycle de vie, formats d'échange (STEP, IGES, STL pour l'impression 3D, DXF pour la découpe).</p>`,
      cards: [["Étapes de modélisation d'une pièce ?", "Esquisse contrainte → fonction volumique → enlèvements/détails → matière."], ["Format neutre d'échange 3D ?", "STEP (.stp)"], ["Format pour l'impression 3D ?", "STL (ou 3MF)"], ["Rôle des contraintes d'assemblage ?", "Positionner les pièces comme les liaisons réelles."]],
      quiz: [
        ["Pour créer un arbre cylindrique étagé, la fonction la plus adaptée :", ["Extrusion", "Révolution", "Coque", "Lissage"], 1, "On fait tourner un profil autour d'un axe."],
        ["Le format universel pour échanger un modèle 3D entre logiciels :", ["STEP", "MP3", "PDF", "DOCX"], 0, "Format neutre ISO 10303."],
        ["Une esquisse bien faite doit être :", ["Sans cotes", "Totalement contrainte", "En couleur", "En 3D"], 1, "Sinon le modèle bouge de façon imprévue."]
      ]
    }
  ]
});
