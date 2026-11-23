COURS.push({
  id: "meca", name: "Mécanique & RDM", emoji: "⚙️", color: "#EA580C",
  desc: "Statique, liaisons, cinématique du solide, résistance des matériaux (traction, cisaillement, flexion, torsion), matériaux, transmissions de puissance et éléments de machines.",
  chapters: [
    {
      id: "liaisons", t: "Liaisons mécaniques et schéma cinématique", y: 1,
      html: `
<h3>Degrés de liberté</h3>
<div class="easy box">Un solide libre dans l'espace peut faire <b>6 mouvements</b> : 3 translations (Tx, Ty, Tz) et 3 rotations (Rx, Ry, Rz). Une liaison supprime certains de ces mouvements.</div>
<h3>Les liaisons normalisées</h3>
<table><tr><th>Liaison</th><th>Mouvements permis</th><th>Nb ddl</th><th>Exemple</th></tr>
<tr><td>Encastrement (fixe)</td><td>aucun</td><td>0</td><td>pièce soudée, vissée</td></tr>
<tr><td>Pivot</td><td>1 rotation</td><td>1</td><td>roue sur son axe, charnière</td></tr>
<tr><td>Glissière</td><td>1 translation</td><td>1</td><td>tiroir, chariot sur rail</td></tr>
<tr><td>Hélicoïdale</td><td>rotation + translation liées</td><td>1</td><td>vis-écrou</td></tr>
<tr><td>Pivot glissant</td><td>1 rotation + 1 translation</td><td>2</td><td>tige de vérin</td></tr>
<tr><td>Rotule (sphérique)</td><td>3 rotations</td><td>3</td><td>rotule de direction, attelage</td></tr>
<tr><td>Appui plan</td><td>2 translations + 1 rotation</td><td>3</td><td>objet posé sur une table</td></tr>
<tr><td>Linéaire annulaire (sphère-cylindre)</td><td>1 translation + 3 rotations</td><td>4</td><td>roulement à rotule</td></tr>
<tr><td>Linéaire rectiligne</td><td>2 translations + 2 rotations</td><td>4</td><td>règle posée sur un plan</td></tr>
<tr><td>Ponctuelle (sphère-plan)</td><td>2 translations + 3 rotations</td><td>5</td><td>bille sur un plan</td></tr></table>
<h3>Méthode pour identifier une liaison</h3>
<ol><li>Repère les surfaces de contact entre les deux pièces.</li><li>Imagine les mouvements possibles d'une pièce par rapport à l'autre.</li><li>Compare au tableau.</li></ol>
<h3>Classes d'équivalence</h3>
<p>On regroupe toutes les pièces <b>fixes entre elles</b> (vissées, collées…) en un même « bloc ». Puis on cherche les liaisons entre blocs. Les vis, écrous, rondelles appartiennent au bloc qu'ils fixent ; les roulements et ressorts sont souvent exclus.</p>
<h3>Graphe des liaisons et schéma cinématique</h3>
<p>Le <b>graphe des liaisons</b> : les blocs sont des cercles, les liaisons des traits. Le <b>schéma cinématique</b> représente les liaisons avec leurs symboles normalisés, dans la bonne disposition, pour comprendre le mouvement.</p>
<div class="tip box">Au BTS, colorie chaque classe d'équivalence d'une couleur différente sur le plan d'ensemble : c'est rapide et ça évite les erreurs.</div>`,
      cards: [["Combien de ddl a un solide libre ?", "6 (3 translations + 3 rotations)."], ["Liaison pivot ?", "1 rotation possible (1 ddl)."], ["Liaison glissière ?", "1 translation possible."], ["Liaison rotule ?", "3 rotations possibles."], ["Classe d'équivalence ?", "Ensemble de pièces sans mouvement relatif entre elles."], ["Liaison vis-écrou ?", "Hélicoïdale : rotation et translation liées."]],
      quiz: [
        ["Une porte sur ses gonds forme une liaison :", ["Glissière", "Pivot", "Rotule", "Encastrement"], 1, "Une seule rotation possible."],
        ["Combien de ddl pour une liaison appui plan ?", ["1", "2", "3", "5"], 2, "2 translations dans le plan + 1 rotation autour de la normale."],
        ["Un tiroir dans un meuble :", ["Pivot", "Glissière", "Hélicoïdale", "Ponctuelle"], 1, "Une seule translation."],
        ["Une tige de vérin dans son corps :", ["Pivot glissant", "Pivot", "Encastrement", "Rotule"], 0, "Elle peut tourner et coulisser."],
        ["Une bille posée sur un plan :", ["Ponctuelle (5 ddl)", "Encastrement", "Glissière", "Pivot"], 0, "Un seul point de contact : seule la translation normale est bloquée."]
      ]
    },
    {
      id: "statique", t: "Statique : équilibre des solides (PFS)", y: 1,
      html: `
<h3>Force et moment</h3>
<p>Une force est un vecteur (point d'application, direction, sens, intensité en N).</p>
<div class="f">Moment d'une force par rapport à un point : M = F × d &nbsp;&nbsp; <small>d = bras de levier (distance perpendiculaire)</small></div>
<div class="easy box">Le moment, c'est « l'effet de rotation » d'une force. Plus tu pousses loin de la charnière, plus la porte s'ouvre facilement. C'est le principe de la clé et du levier.</div>
<h3>Principe Fondamental de la Statique (PFS)</h3>
<p>Un solide est en équilibre si :</p>
<div class="f">ΣF = 0 (somme des forces nulle) &nbsp;&nbsp; et &nbsp;&nbsp; ΣM/A = 0 (somme des moments nulle en tout point A)</div>
<p>En 2D, cela donne <b>3 équations</b> : ΣFx = 0, ΣFy = 0, ΣM = 0.</p>
<h3>Méthode</h3>
<ol><li><b>Isoler</b> le solide (le dessiner seul).</li><li><b>Bilan des actions extérieures</b> (poids, appuis, forces connues).</li><li>Choisir un point où passent beaucoup d'inconnues pour écrire les moments (elles disparaissent).</li><li>Résoudre et vérifier.</li></ol>
<div class="ex box">Poutre de 4 m sur deux appuis A et B, charge de 1 000 N à 1 m de A. ΣM/A = 0 : B × 4 − 1000 × 1 = 0 → B = 250 N. ΣFy = 0 : A = 1000 − 250 = 750 N.</div>
<h3>Cas particuliers</h3>
<ul><li><b>Solide soumis à 2 forces</b> : elles sont égales, opposées et ont la même droite d'action (celle qui passe par les deux points). Ex : une biellette.</li><li><b>Solide soumis à 3 forces non parallèles</b> : elles sont concourantes (se coupent en un point) et leur somme vectorielle forme un triangle fermé (résolution graphique).</li></ul>
<h3>Frottement (loi de Coulomb)</h3>
<div class="f">T ≤ f·N &nbsp;&nbsp; f = tan φ</div>
<p>Acier/acier sec : f ≈ 0,15–0,2 ; avec lubrification ≈ 0,05–0,1.</p>
<h3>Actions mécaniques usuelles</h3>
<ul><li>Pression sur une surface : F = p·S (vérin : F = p × π·D²/4).</li><li>Ressort : F = k·x.</li><li>Poids : P = m·g appliqué au centre de gravité.</li></ul>`,
      cards: [["Moment d'une force ?", "M = F × d (bras de levier perpendiculaire)."], ["PFS ?", "ΣF = 0 et ΣM = 0"], ["Solide soumis à 2 forces ?", "Forces égales, opposées, même droite d'action."], ["Solide soumis à 3 forces non parallèles ?", "Concourantes + triangle des forces fermé."], ["Force d'un vérin ?", "F = p × S = p × πD²/4"]],
      quiz: [
        ["Une force de 50 N à 0,4 m d'un axe crée un moment de :", ["20 N·m", "125 N·m", "50,4 N·m", "200 N·m"], 0, "50 × 0,4."],
        ["En 2D, le PFS donne combien d'équations ?", ["1", "2", "3", "6"], 2, "ΣFx, ΣFy, ΣM."],
        ["Vérin D = 50 mm, p = 6 bar (0,6 MPa). Force de sortie ≈", ["1 178 N", "300 N", "11 780 N", "30 N"], 0, "π × 25² × 0,6 ≈ 1 178 N."],
        ["Pour écrire les moments, on choisit de préférence un point :", ["Au hasard", "Où passent des forces inconnues", "Loin de tout", "Au milieu"], 1, "Leurs moments s'annulent : moins d'inconnues."],
        ["Une poutre de 2 m, charge de 600 N au milieu. Réaction de chaque appui :", ["600 N", "300 N", "1 200 N", "150 N"], 1, "Symétrie : chacun reprend la moitié."]
      ]
    },
    {
      id: "cinesolide", t: "Cinématique du solide", y: 1,
      html: `
<h3>Mouvements plans</h3>
<ul><li><b>Translation</b> : tous les points ont la même vitesse.</li><li><b>Rotation</b> autour d'un axe fixe : v = R·ω, les vitesses sont perpendiculaires au rayon et proportionnelles à la distance à l'axe.</li><li><b>Mouvement plan quelconque</b> : combinaison (ex : une bielle).</li></ul>
<h3>Composition des vitesses</h3>
<div class="f">V(M, 2/0) = V(M, 2/1) + V(M, 1/0)</div>
<div class="easy box">Tu marches à 1 m/s dans un train qui roule à 30 m/s : ta vitesse par rapport au sol = 31 m/s. C'est la composition des vitesses.</div>
<h3>Équiprojectivité</h3>
<p>Pour deux points A et B d'un même solide, les projections de leurs vitesses sur la droite (AB) sont égales. Permet de trouver graphiquement une vitesse inconnue.</p>
<h3>Centre instantané de rotation (CIR)</h3>
<p>Point où la vitesse est nulle à l'instant étudié. Il est à l'intersection des perpendiculaires aux vitesses connues. Ensuite v = distance au CIR × ω.</p>
<h3>Système bielle-manivelle</h3><p>Transforme une rotation en translation alternée (moteur, compresseur). Course = 2 × longueur de la manivelle.</p>
<h3>Vis-écrou</h3><div class="f">V_translation = pas × N (N en tr/s)</div>
<div class="ex box">Vis à billes de pas 5 mm tournant à 600 tr/min (10 tr/s) : v = 5 × 10 = 50 mm/s.</div>`,
      cards: [["Composition des vitesses ?", "V(2/0) = V(2/1) + V(1/0)"], ["Vitesse d'un point en rotation ?", "v = R·ω, perpendiculaire au rayon."], ["CIR ?", "Point de vitesse nulle à l'instant étudié."], ["Vitesse d'un système vis-écrou ?", "v = pas × vitesse de rotation (tr/s)."]],
      quiz: [
        ["Vis de pas 4 mm à 300 tr/min. Vitesse de l'écrou :", ["20 mm/s", "1 200 mm/s", "75 mm/s", "4 mm/s"], 0, "300 tr/min = 5 tr/s ; 4 × 5 = 20 mm/s."],
        ["Dans une translation, les points du solide ont :", ["Des vitesses différentes", "La même vitesse", "Une vitesse nulle", "Une vitesse ∝ distance"], 1, "Définition de la translation."],
        ["Course d'un système bielle-manivelle de manivelle 50 mm :", ["50 mm", "100 mm", "25 mm", "200 mm"], 1, "Course = 2 × manivelle."]
      ]
    },
    {
      id: "rdm1", t: "RDM : traction, compression, cisaillement", y: 1,
      html: `
<h3>À quoi sert la RDM ?</h3>
<div class="easy box">La Résistance Des Matériaux permet de vérifier qu'une pièce <b>ne casse pas</b> et <b>ne se déforme pas trop</b>, et de la dimensionner (choisir sa taille et son matériau).</div>
<h3>Contrainte normale (traction/compression)</h3>
<div class="f">σ = N/S &nbsp;&nbsp; <small>(MPa = N/mm²)</small></div>
<h3>Loi de Hooke (domaine élastique)</h3>
<div class="f">σ = E·ε &nbsp;&nbsp; ε = ΔL/L₀ &nbsp;&nbsp; ΔL = N·L₀/(E·S)</div>
<p>E = module de Young (acier ≈ 200 000 MPa ; alu ≈ 70 000 MPa).</p>
<h3>L'essai de traction</h3>
<ul><li>Zone <b>élastique</b> : la pièce reprend sa forme (comme un ressort).</li><li><b>Re</b> (limite élastique) : au-delà, déformation permanente.</li><li><b>Rm</b> (résistance maximale) : avant la rupture.</li><li>A % : allongement à la rupture (ductilité).</li></ul>
<h3>Condition de résistance</h3>
<div class="f">σ_max ≤ Rpe = Re / s &nbsp;&nbsp; <small>s = coefficient de sécurité (souvent 1,5 à 5)</small></div>
<div class="ex box">Tirant en acier S235 (Re = 235 MPa), s = 2 → Rpe = 117,5 MPa. Effort N = 20 000 N. Section minimale : S = 20000/117,5 ≈ 170 mm² → diamètre ≥ 14,7 mm → on choisit Ø16.</div>
<h3>Cisaillement</h3>
<div class="f">τ = T/S ≤ Rpg &nbsp;&nbsp; <small>(acier : Rpg ≈ 0,5 à 0,8 × Rpe)</small></div>
<p>Exemples : goupilles, rivets, clavettes, axes de chape. Attention : un axe de chape travaille souvent en <b>double cisaillement</b> (2 sections).</p>
<div class="ex box">Axe Ø10 en double cisaillement, effort 10 000 N : S = 2 × π × 5² = 157 mm² → τ = 63,7 MPa.</div>
<h3>Concentration de contraintes</h3><p>Les trous, gorges, épaulements augmentent localement la contrainte : σ_réelle = Kt × σ_nominale.</p>
<h3>Flambage</h3><p>Une pièce longue et fine en compression peut « flamber » (se plier d'un coup) bien avant Re. On vérifie avec la charge critique d'Euler : F_c = π²EI/L_f².</p>`,
      cards: [["Contrainte de traction ?", "σ = N/S"], ["Loi de Hooke ?", "σ = E·ε"], ["Module de Young de l'acier ?", "≈ 200 000 MPa (210 GPa)"], ["Condition de résistance ?", "σ ≤ Re/s"], ["Contrainte de cisaillement ?", "τ = T/S"], ["Re ?", "Limite élastique : au-delà, déformation permanente."]],
      quiz: [
        ["Une barre de 100 mm² supporte 5 000 N. σ =", ["50 MPa", "500 MPa", "0,02 MPa", "5 MPa"], 0, "5000/100 = 50 N/mm²."],
        ["Re = 300 MPa, s = 3. Rpe =", ["900 MPa", "100 MPa", "303 MPa", "297 MPa"], 1, "300/3."],
        ["Au-delà de Re, la pièce :", ["Casse immédiatement", "Garde une déformation permanente", "Redevient comme avant", "Devient plus dure sans conséquence"], 1, "Fin du domaine élastique."],
        ["Un axe de chape travaille souvent en :", ["Simple cisaillement", "Double cisaillement", "Torsion pure", "Flambage"], 1, "L'effort est repris par 2 sections."],
        ["Le risque pour une tige longue et fine comprimée :", ["Le flambage", "La torsion", "La corrosion", "La fatigue thermique"], 0, "Instabilité élastique."]
      ]
    },
    {
      id: "rdm2", t: "RDM : flexion et torsion", y: 2,
      html: `
<h3>Flexion</h3>
<div class="easy box">Une poutre en flexion se courbe : les fibres du haut sont <b>comprimées</b>, celles du bas <b>tendues</b> (ou l'inverse), et au milieu la <b>fibre neutre</b> ne subit rien.</div>
<h4>Efforts intérieurs</h4>
<p>On coupe la poutre et on calcule l'effort tranchant T(x) et le <b>moment fléchissant</b> Mf(x). On trace leurs diagrammes. Le point critique est là où |Mf| est maximal.</p>
<div class="f">σ_max = Mf_max / (I/v) &nbsp;&nbsp; <small>I/v = module de flexion (mm³)</small></div>
<h4>Moments quadratiques à connaître</h4>
<table><tr><th>Section</th><th>I (axe de flexion)</th><th>I/v</th></tr>
<tr><td>Rectangle b × h</td><td>b·h³/12</td><td>b·h²/6</td></tr>
<tr><td>Cercle plein Ø d</td><td>π·d⁴/64</td><td>π·d³/32</td></tr>
<tr><td>Tube D/d</td><td>π(D⁴ − d⁴)/64</td><td>π(D⁴ − d⁴)/(32D)</td></tr></table>
<div class="tip box">Une règle posée « à plat » plie facilement, posée « sur la tranche » elle est très rigide : la hauteur h compte au cube ! C'est pour ça que les poutres sont en I ou en H.</div>
<h4>Cas classiques</h4>
<table><tr><th>Cas</th><th>Mf max</th><th>Flèche max</th></tr>
<tr><td>Poutre sur 2 appuis, charge F au milieu</td><td>F·L/4</td><td>F·L³/(48EI)</td></tr>
<tr><td>Poutre sur 2 appuis, charge répartie q</td><td>q·L²/8</td><td>5qL⁴/(384EI)</td></tr>
<tr><td>Console (encastrée), F au bout</td><td>F·L</td><td>F·L³/(3EI)</td></tr></table>
<div class="ex box">Console de 1 m, F = 1 000 N, section rectangulaire 20 × 40 mm (h = 40). Mf = 10⁶ N·mm. I/v = 20 × 40²/6 ≈ 5 333 mm³. σ = 187,5 MPa.</div>
<h3>Torsion</h3>
<div class="f">τ_max = Mt / (I₀/R) &nbsp;&nbsp; I₀ = π·d⁴/32 (cercle plein) &nbsp;&nbsp; I₀/R = π·d³/16</div>
<div class="f">Angle de torsion : θ = Mt·L/(G·I₀) &nbsp;&nbsp; <small>G acier ≈ 80 000 MPa</small></div>
<div class="ex box">Arbre transmettant 5 kW à 1 000 tr/min : Mt = P/ω = 5000/104,7 ≈ 47,7 N·m. Avec τ admissible 40 MPa : d³ ≥ 16 × 47 700/(π × 40) → d ≥ 18,2 mm → Ø20.</div>
<h3>Sollicitations composées</h3><p>Flexion + torsion (arbres de transmission) : on utilise une contrainte équivalente (critère de Von Mises : σ_eq = √(σ² + 3τ²)).</p>
<h3>Fatigue</h3><p>Une pièce soumise à des efforts répétés peut casser sous Re (rupture par fatigue). On tient compte de la limite d'endurance et de l'état de surface.</p>`,
      cards: [["Contrainte max en flexion ?", "σ = Mf / (I/v)"], ["I/v rectangle b×h ?", "b·h²/6"], ["I rond plein ?", "πd⁴/64"], ["Mf max poutre 2 appuis, F au milieu ?", "F·L/4"], ["Contrainte max en torsion (rond plein) ?", "τ = 16·Mt/(π·d³)"], ["Critère de Von Mises (flexion+torsion) ?", "σ_eq = √(σ² + 3τ²)"]],
      quiz: [
        ["Poutre 2 appuis L = 2 m, F = 1 000 N au milieu. Mf max =", ["500 N·m", "2 000 N·m", "250 N·m", "1 000 N·m"], 0, "FL/4 = 1000 × 2 / 4."],
        ["Si on double la hauteur h d'une poutre rectangulaire, I est multiplié par :", ["2", "4", "8", "16"], 2, "I = bh³/12."],
        ["Dans une poutre fléchie, la fibre neutre :", ["Est la plus contrainte", "N'est ni tendue ni comprimée", "Casse en premier", "N'existe pas"], 1, "Contrainte nulle au centre."],
        ["Mt d'un arbre transmettant P à ω :", ["P × ω", "P/ω", "ω/P", "P + ω"], 1, "P = Mt·ω."],
        ["Une console encastrée est la plus sollicitée :", ["À l'extrémité libre", "À l'encastrement", "Au milieu", "Partout pareil"], 1, "Mf = F·L est maximal à l'encastrement."]
      ]
    },
    {
      id: "materiaux", t: "Matériaux et désignations normalisées", y: 1,
      html: `
<h3>Les familles</h3>
<table><tr><th>Famille</th><th>Exemples</th><th>Atouts</th></tr>
<tr><td>Métaux ferreux</td><td>aciers, fontes, inox</td><td>résistants, pas chers, soudables</td></tr>
<tr><td>Métaux non ferreux</td><td>aluminium, cuivre, laiton, titane</td><td>légers, conducteurs, résistent à la corrosion</td></tr>
<tr><td>Polymères</td><td>PE, PP, PVC, PA (nylon), POM, PMMA</td><td>légers, isolants, moulables</td></tr>
<tr><td>Céramiques</td><td>alumine, carbure</td><td>dures, résistent à la chaleur, fragiles</td></tr>
<tr><td>Composites</td><td>fibre de verre/carbone + résine</td><td>très légers et rigides, chers</td></tr></table>
<h3>Désignation des aciers (norme EN 10027)</h3>
<ul>
<li><b>S235, S355</b> : aciers de construction ; le nombre = Re mini en MPa.</li>
<li><b>E295, E335</b> : aciers de construction mécanique.</li>
<li><b>C35, C45</b> : aciers non alliés pour traitement thermique ; % carbone × 100 (C45 = 0,45 % C).</li>
<li><b>42CrMo4</b> : faiblement allié : 0,42 % C, chrome et molybdène (teneur Cr = 4/4 = 1 %).</li>
<li><b>X5CrNi18-10</b> : fortement allié (inox 304) : 0,05 % C, 18 % Cr, 10 % Ni.</li></ul>
<h3>Fontes</h3><p>EN-GJL-250 (fonte grise lamellaire, Rm = 250 MPa), EN-GJS-400-15 (fonte à graphite sphéroïdal, Rm 400 MPa, A 15 %).</p>
<h3>Aluminium</h3><p>EN AW-2017 (Duralumin, usinage), EN AW-6060/6061 (profilés), EN AW-5754 (tôle, bonne résistance corrosion), EN AW-7075 (très résistant).</p>
<h3>Propriétés à connaître</h3>
<ul><li><b>Dureté</b> (Brinell HB, Vickers HV, Rockwell HRC) : résistance à la pénétration.</li><li><b>Résilience</b> (essai Charpy) : résistance aux chocs.</li><li><b>Ductilité</b> : capacité à se déformer sans casser (A %).</li><li><b>Masse volumique</b> : acier 7 850 kg/m³, alu 2 700, cuivre 8 900.</li></ul>
<h3>Traitements thermiques</h3>
<ul><li><b>Trempe</b> : chauffage puis refroidissement rapide → plus dur, plus fragile.</li><li><b>Revenu</b> : après la trempe, réchauffage modéré → moins fragile.</li><li><b>Recuit</b> : refroidissement lent → plus doux, facile à usiner.</li><li>Traitements de surface : cémentation, nitruration (surface dure, cœur tenace).</li></ul>
<h3>Choisir un matériau</h3><p>Critères : résistance, masse, coût, usinabilité, soudabilité, corrosion, recyclabilité. Outils : diagrammes d'Ashby, logiciels type CES.</p>`,
      cards: [["S235 signifie ?", "Acier de construction, Re mini = 235 MPa."], ["C45 ?", "Acier non allié à 0,45 % de carbone."], ["X5CrNi18-10 ?", "Inox : 0,05 % C, 18 % Cr, 10 % Ni."], ["Masse volumique de l'acier ?", "≈ 7 850 kg/m³"], ["Trempe ?", "Refroidissement rapide : dureté ↑, fragilité ↑."], ["Revenu ?", "Réchauffage après trempe pour réduire la fragilité."]],
      quiz: [
        ["Dans S355, 355 représente :", ["Rm", "Re minimale en MPa", "% de carbone", "La dureté"], 1, "S = acier de construction, 355 = Re mini."],
        ["C35 contient :", ["35 % de carbone", "0,35 % de carbone", "3,5 % de carbone", "35 % de chrome"], 1, "On divise par 100."],
        ["Le matériau le plus léger :", ["Acier", "Cuivre", "Aluminium", "Fonte"], 2, "2 700 kg/m³ environ."],
        ["L'essai Charpy mesure :", ["La dureté", "La résilience (chocs)", "La conductivité", "La corrosion"], 1, "Énergie absorbée lors d'un choc."],
        ["Un acier inoxydable contient au moins environ :", ["10,5 % de chrome", "10 % de carbone", "50 % de nickel", "1 % de zinc"], 0, "Le chrome forme une couche protectrice."]
      ]
    },
    {
      id: "transmissions", t: "Transmissions de puissance", y: 2,
      html: `
<h3>Rapport de transmission</h3>
<div class="f">r = ω_sortie / ω_entrée = N_s/N_e</div>
<p>r &lt; 1 : <b>réducteur</b> (la vitesse baisse, le couple augmente). r &gt; 1 : multiplicateur.</p>
<div class="f">C_sortie = C_entrée × η / r</div>
<h3>Engrenages</h3>
<div class="f">r = Z_menant / Z_mené (= produit des Z menants / produit des Z menés pour un train)</div>
<div class="f">d = m·Z &nbsp;&nbsp; entraxe a = m(Z₁ + Z₂)/2</div>
<p>m = module (taille des dents). Deux roues qui engrènent ont le même module. Engrenage extérieur : inversion du sens de rotation.</p>
<table><tr><th>Type</th><th>Usage</th></tr><tr><td>Droit</td><td>simple, axes parallèles</td></tr><tr><td>Hélicoïdal</td><td>silencieux, axes parallèles</td></tr><tr><td>Conique</td><td>axes concourants (renvoi d'angle)</td></tr><tr><td>Roue et vis sans fin</td><td>grand rapport de réduction, souvent irréversible, rendement faible</td></tr><tr><td>Train épicycloïdal</td><td>grand rapport compact (formule de Willis)</td></tr></table>
<div class="ex box">Moteur 1 500 tr/min, pignon Z = 20 sur roue Z = 60 : N_s = 1500 × 20/60 = 500 tr/min. Couple multiplié par 3 (× η).</div>
<h3>Poulies-courroie</h3><div class="f">r = d_menante / d_menée</div><p>Courroie plate, trapézoïdale, crantée (synchrone, pas de glissement). Avantages : silencieuse, absorbe les chocs, grand entraxe.</p>
<h3>Chaîne et pignons</h3><p>r = Z_menant/Z_mené. Robuste, pas de glissement, nécessite lubrification.</p>
<h3>Autres éléments</h3>
<ul><li><b>Accouplements</b> : relient deux arbres (rigides, élastiques, cardans).</li><li><b>Embrayages / freins</b> : à friction.</li><li><b>Limiteur de couple</b> : protège en cas de blocage.</li><li><b>Roulements</b> : à billes (charges moyennes, vitesses élevées), à rouleaux (fortes charges). Durée de vie L₁₀ = (C/P)³ millions de tours (billes).</li><li><b>Clavettes, cannelures</b> : liaison arbre-moyeu en rotation.</li></ul>
<h3>Choisir un motoréducteur (méthode)</h3>
<ol><li>Calculer la puissance utile (F·v ou C·ω).</li><li>Diviser par les rendements.</li><li>Calculer le rapport : vitesse de sortie / vitesse moteur.</li><li>Choisir dans le catalogue avec un facteur de service (chocs, heures/jour).</li></ol>`,
      cards: [["Rapport d'un engrenage ?", "r = Z_menant / Z_mené"], ["Diamètre primitif ?", "d = m·Z"], ["Entraxe ?", "a = m(Z₁ + Z₂)/2"], ["Rôle d'un réducteur ?", "Diminuer la vitesse et augmenter le couple."], ["Particularité de la roue et vis sans fin ?", "Grand rapport, souvent irréversible, rendement faible."], ["Poulies : rapport ?", "r = d_menante / d_menée"]],
      quiz: [
        ["Pignon 15 dents → roue 45 dents. Le rapport est :", ["3", "1/3", "60", "30"], 1, "15/45."],
        ["m = 2, Z = 30. Diamètre primitif :", ["60 mm", "15 mm", "32 mm", "28 mm"], 0, "d = mZ."],
        ["Un réducteur augmente :", ["La vitesse", "Le couple", "La puissance", "Le rendement"], 1, "Moins de vitesse, plus de couple (la puissance diminue un peu à cause des pertes)."],
        ["Pour transmettre sans glissement par courroie, on prend :", ["Courroie plate", "Courroie crantée", "Courroie ronde", "Corde"], 1, "Transmission synchrone."],
        ["Pour un renvoi d'angle à 90° :", ["Engrenage droit", "Engrenage conique", "Courroie plate", "Clavette"], 1, "Axes concourants."]
      ]
    },
    {
      id: "fabrication", t: "Procédés de fabrication et assemblages", y: 1,
      html: `
<h3>Enlèvement de matière (usinage)</h3>
<ul><li><b>Tournage</b> : la pièce tourne, l'outil avance → pièces cylindriques.</li><li><b>Fraisage</b> : l'outil tourne → surfaces planes, rainures, poches.</li><li><b>Perçage</b>, alésage, taraudage.</li><li><b>Rectification</b> : finition très précise à la meule.</li><li>Électroérosion, découpe laser/jet d'eau/plasma.</li></ul>
<div class="f">Vitesse de coupe Vc = π·D·N/1000 (m/min, D en mm, N en tr/min)</div>
<div class="ex box">Fraise Ø20, Vc = 100 m/min → N = 1000 × 100/(π × 20) ≈ 1 590 tr/min.</div>
<h3>Mise en forme sans enlèvement</h3>
<ul><li><b>Moulage</b> (fonderie sable, coquille, sous pression).</li><li><b>Forgeage</b>, emboutissage, pliage de tôle.</li><li><b>Injection plastique</b> (grandes séries, moule cher).</li><li><b>Extrusion</b> (profilés).</li><li><b>Fabrication additive</b> (impression 3D : FDM, SLA, SLS, métal) → prototypes, pièces complexes, petites séries.</li></ul>
<h3>Assemblages</h3>
<table><tr><th>Type</th><th>Exemples</th><th>Démontable ?</th></tr>
<tr><td>Vissage / boulonnage</td><td>vis, écrous, goujons</td><td>Oui</td></tr><tr><td>Goupilles, clavettes, anneaux élastiques</td><td>positionnement, arrêt</td><td>Oui</td></tr>
<tr><td>Soudage</td><td>MIG/MAG, TIG, à l'arc, par points</td><td>Non</td></tr><tr><td>Rivetage</td><td>rivets aveugles (pop)</td><td>Non</td></tr>
<tr><td>Collage</td><td>époxy, cyanoacrylate</td><td>Non</td></tr><tr><td>Frettage (ajustement serré)</td><td>roulement sur arbre</td><td>Difficilement</td></tr></table>
<h3>Désignation d'une vis</h3><p>Vis H M10 × 40 – 8.8 : tête hexagonale, filetage métrique Ø10, longueur 40 mm, classe de qualité 8.8 (Rm = 800 MPa, Re = 0,8 × 800 = 640 MPa).</p>
<h3>Choisir un procédé</h3><p>Selon la quantité (série), la forme, le matériau, la précision et le coût. Petite série → usinage/impression 3D ; grande série → moulage/injection/emboutissage.</p>`,
      cards: [["Vitesse de coupe ?", "Vc = π·D·N/1000 (m/min)"], ["Vis classe 8.8 ?", "Rm = 800 MPa, Re = 640 MPa"], ["Procédé pour grandes séries de pièces plastiques ?", "Injection plastique."], ["Assemblages non démontables ?", "Soudage, rivetage, collage."]],
      quiz: [
        ["Pour une pièce cylindrique, on utilise surtout :", ["Le fraisage", "Le tournage", "Le pliage", "Le soudage"], 1, "La pièce tourne sur le tour."],
        ["Vis classe 10.9 : Re =", ["1 000 MPa", "900 MPa", "109 MPa", "10,9 MPa"], 1, "Rm = 1000 ; Re = 0,9 × 1000."],
        ["Un prototype unique et complexe en plastique :", ["Injection", "Impression 3D", "Forgeage", "Emboutissage"], 1, "Pas de moule nécessaire."],
        ["Ø50 à 400 tr/min. Vc ≈", ["63 m/min", "20 m/min", "200 m/min", "6,3 m/min"], 0, "π × 50 × 400 / 1000 ≈ 62,8."]
      ]
    }
  ]
});
