// Chapitres supplémentaires (v1.1), insérés dans les matières existantes.
function addChapter(sid, ch, afterId) {
  var s = COURS.filter(function (x) { return x.id === sid; })[0]; if (!s) return;
  var i = s.chapters.map(function (c) { return c.id; }).indexOf(afterId);
  if (i < 0) s.chapters.push(ch); else s.chapters.splice(i + 1, 0, ch);
}

addChapter("physique", {
  id: "magnetisme", t: "Électromagnétisme : champ, force de Laplace, induction", y: 2,
  html: `
<h3>Pourquoi c'est important ?</h3>
<div class="easy box">Moteurs, transformateurs, électroaimants, contacteurs, capteurs inductifs, freins… tout fonctionne grâce à <b>deux phénomènes</b> : un courant crée un champ magnétique, et un champ magnétique variable crée une tension (induction).</div>
<h3>Le champ magnétique B</h3>
<p>Il se mesure en <b>tesla (T)</b>. Il est créé par un aimant ou par un courant. Lignes de champ : du pôle Nord vers le pôle Sud à l'extérieur de l'aimant.</p>
<div class="f">Bobine longue (solénoïde) : B = µ₀·N·I / L &nbsp;&nbsp; <small>µ₀ = 4π × 10⁻⁷ H/m</small></div>
<p>Un noyau de fer multiplie le champ par sa perméabilité relative µr (plusieurs milliers) : c'est pour ça que les machines ont des circuits magnétiques en tôles d'acier.</p>
<h3>Force de Laplace</h3>
<div class="f">F = B·I·L·sin α</div>
<p>Un conducteur parcouru par un courant dans un champ magnétique subit une force. Sens : règle des trois doigts de la main droite (pouce = I, index = B, majeur = F).</p>
<div class="ex box">Conducteur de 0,2 m, I = 10 A, B = 0,8 T perpendiculaire : F = 0,8 × 10 × 0,2 = 1,6 N. C'est le principe du moteur à courant continu.</div>
<h3>Flux magnétique</h3>
<div class="f">Φ = B·S·cos θ &nbsp;&nbsp; (weber, Wb)</div>
<h3>Induction (loi de Faraday) et loi de Lenz</h3>
<div class="f">e = −N · dΦ/dt</div>
<p>Si le flux à travers une bobine <b>varie</b>, une tension apparaît. La loi de Lenz (le signe −) dit que le courant induit <b>s'oppose</b> à la cause qui l'a créé.</p>
<ul><li>Alternateur / génératrice : un aimant tourne devant une bobine.</li><li>Transformateur : le flux alternatif du primaire induit une tension au secondaire.</li><li>Freinage par courants de Foucault (poids lourds, trains).</li><li>Chauffage par induction (plaques, fours de trempe).</li></ul>
<h3>Auto-induction</h3>
<div class="f">u = L·di/dt &nbsp;&nbsp; Énergie stockée : W = ½·L·I²</div>
<div class="warn box">Couper brutalement le courant dans une bobine (bobine de contacteur, électrovanne) crée une forte surtension. On protège avec une <b>diode de roue libre</b> (en continu) ou un circuit RC / varistance.</div>
<h3>Pertes dans les circuits magnétiques</h3><p>Pertes par <b>hystérésis</b> (cycle d'aimantation) et par <b>courants de Foucault</b> (réduites en feuilletant le fer en tôles isolées).</p>`,
  cards: [["Unité du champ magnétique ?", "Le tesla (T)."], ["Force de Laplace ?", "F = B·I·L·sin α"], ["Loi de Faraday ?", "e = −N·dΦ/dt : une variation de flux crée une tension."], ["Loi de Lenz ?", "Le courant induit s'oppose à la cause qui le crée."], ["Pourquoi les tôles feuilletées ?", "Réduire les courants de Foucault."], ["Protection d'une bobine en continu ?", "Diode de roue libre."]],
  quiz: [
    ["B = 0,5 T, I = 20 A, L = 0,1 m (perpendiculaire). F =", ["1 N", "10 N", "0,1 N", "100 N"], 0, "0,5 × 20 × 0,1 = 1 N."],
    ["Une tension est induite dans une bobine quand :", ["Le flux est constant", "Le flux varie", "Le courant est nul", "Elle est en cuivre"], 1, "Loi de Faraday : il faut une variation."],
    ["Les circuits magnétiques sont feuilletés pour réduire :", ["Le poids", "Les courants de Foucault", "Le bruit seulement", "Le prix"], 1, "Les tôles isolées coupent les boucles de courant."],
    ["L'unité du flux magnétique est :", ["Tesla", "Weber", "Henry", "Ampère"], 1, "Φ en weber (Wb)."]
  ]
}, "ondes");

addChapter("elec", {
  id: "energie_renouv", t: "Efficacité énergétique et énergies renouvelables", y: 2,
  html: `
<h3>Le coût de l'énergie dans l'industrie</h3>
<div class="f">Énergie (kWh) = Puissance (kW) × durée (h) &nbsp;&nbsp; Coût = kWh × prix du kWh</div>
<div class="ex box">Compresseur de 30 kW qui tourne 4 000 h/an à 0,15 €/kWh : 30 × 4000 × 0,15 = 18 000 € par an. Réduire ses fuites de 20 % fait gagner environ 3 600 €/an.</div>
<h3>Les gisements d'économies classiques</h3>
<ul><li><b>Moteurs à haut rendement</b> (classes IE3, IE4) : un moteur coûte souvent bien plus en électricité sur sa vie qu'à l'achat.</li><li><b>Variateurs de vitesse</b> sur pompes et ventilateurs (P ∝ n³).</li><li><b>Air comprimé</b> : chasse aux fuites, baisser la pression de 1 bar ≈ −7 % d'énergie, récupérer la chaleur du compresseur.</li><li><b>Éclairage LED</b> + détecteurs de présence.</li><li><b>Compensation du réactif</b> (condensateurs) pour éviter les pénalités.</li><li><b>Isolation</b> des réseaux chauds (vapeur, eau chaude), récupération de chaleur.</li><li>Arrêter les machines hors production (veilles).</li></ul>
<h3>Mesurer pour agir</h3><p>Compteurs et sous-comptages, analyseur de réseau (P, Q, cos φ, harmoniques), système de management de l'énergie ISO 50001, indicateurs (kWh par pièce produite). Certificats d'économies d'énergie (CEE) pour financer certains travaux.</p>
<h3>Photovoltaïque</h3>
<p>Les cellules convertissent la lumière en courant continu ; un <b>onduleur</b> le transforme en alternatif. Puissance crête en kWc (sous 1 000 W/m², 25 °C). En France, 1 kWc produit environ 1 000 à 1 400 kWh/an selon la région.</p>
<div class="f">P = η × S × E &nbsp;&nbsp; <small>η ≈ 20 % pour un panneau actuel, E = éclairement (W/m²)</small></div>
<div class="ex box">Toiture de 500 m² de panneaux, η = 20 % : puissance crête ≈ 0,2 × 500 × 1000 = 100 kWc → environ 120 000 kWh/an.</div>
<h3>Éolien</h3><div class="f">P = ½ · ρ · S · v³ · Cp &nbsp;&nbsp; <small>Cp ≤ 0,59 (limite de Betz)</small></div><p>La puissance varie avec le <b>cube</b> de la vitesse du vent : un vent 2 fois plus fort donne 8 fois plus de puissance.</p>
<h3>Autres sources et stockage</h3><ul><li>Hydraulique, biomasse, géothermie, pompes à chaleur, solaire thermique.</li><li>Stockage : batteries lithium-ion (Wh = V × Ah), hydrogène, volants d'inertie, stations de pompage.</li><li>Autoconsommation : consommer sur place l'énergie produite.</li></ul>`,
  cards: [["Énergie consommée ?", "E (kWh) = P (kW) × t (h)"], ["Classes de rendement des moteurs ?", "IE1 à IE4 (IE4 = meilleur rendement courant)."], ["Puissance éolienne ?", "P = ½ρSv³Cp"], ["Limite de Betz ?", "Cp max ≈ 0,59"], ["Norme du management de l'énergie ?", "ISO 50001"], ["1 kWc en France produit environ ?", "1 000 à 1 400 kWh/an."]],
  quiz: [
    ["Une machine de 5 kW qui tourne 8 h consomme :", ["40 kWh", "13 kWh", "0,625 kWh", "40 kW"], 0, "5 × 8 = 40 kWh."],
    ["Si la vitesse du vent double, la puissance éolienne est multipliée par :", ["2", "4", "8", "16"], 2, "P ∝ v³."],
    ["Le composant qui convertit le courant continu des panneaux en alternatif :", ["Redresseur", "Onduleur", "Transformateur", "Hacheur"], 1, "DC → AC = onduleur."],
    ["Baisser la pression d'un réseau d'air comprimé :", ["Augmente la consommation", "Réduit la consommation", "Ne change rien", "Est interdit"], 1, "≈ −7 % par bar en moins."]
  ]
}, "habilitation");

addChapter("auto", {
  id: "robotique", t: "Robotique industrielle", y: 2,
  html: `
<h3>Les types de robots</h3>
<table><tr><th>Type</th><th>Structure</th><th>Usage</th></tr>
<tr><td>Anthropomorphe (poly-articulé)</td><td>6 axes rotatifs, comme un bras humain</td><td>soudage, peinture, manipulation</td></tr>
<tr><td>SCARA</td><td>4 axes, rapide dans un plan</td><td>assemblage, pick and place</td></tr>
<tr><td>Cartésien (portique)</td><td>3 axes linéaires X, Y, Z</td><td>palettisation, grandes charges</td></tr>
<tr><td>Delta (parallèle)</td><td>bras parallèles, très rapide</td><td>agroalimentaire, tri</td></tr>
<tr><td>Cobot</td><td>bras léger limité en force et vitesse</td><td>travail à côté de l'opérateur</td></tr>
<tr><td>AGV / AMR</td><td>robot mobile</td><td>logistique interne</td></tr></table>
<h3>Caractéristiques à connaître</h3><ul><li><b>Nombre d'axes</b> (degrés de liberté) : 6 axes pour atteindre n'importe quelle position ET orientation.</li><li><b>Charge utile</b> (kg), incluant l'outil (préhenseur).</li><li><b>Portée / volume de travail</b>.</li><li><b>Répétabilité</b> (ex : ± 0,02 mm) : capacité à revenir au même point.</li><li>Vitesse, temps de cycle.</li></ul>
<h3>Repères</h3><p>Repère <b>monde/base</b> (fixe), repère <b>outil</b> (TCP = Tool Center Point, le bout de l'outil), repère <b>objet / utilisateur</b> (lié à la pièce ou à la table). Bien définir le TCP est indispensable pour des trajectoires correctes.</p>
<h3>Programmation</h3><ul><li><b>Apprentissage</b> (teach) avec le pupitre : on déplace le robot et on enregistre des points.</li><li>Mouvements : <b>PTP / articulaire</b> (le plus rapide, trajectoire non maîtrisée), <b>linéaire</b> (ligne droite de l'outil), <b>circulaire</b>.</li><li>Programmation hors ligne (simulation 3D) pour réduire les arrêts de production.</li><li>Entrées/sorties et dialogue avec l'automate de la cellule.</li></ul>
<h3>Préhenseurs</h3><p>Pinces pneumatiques ou électriques, ventouses (vide), électroaimants, outils (torche de soudage, pistolet de peinture, visseuse). Changeur d'outil automatique.</p>
<h3>Sécurité d'une cellule robotisée</h3><ul><li>Enceinte grillagée avec porte interverrouillée, barrières immatérielles, scrutateurs laser.</li><li>Modes : automatique, manuel vitesse réduite (≤ 250 mm/s) avec bouton de validation 3 positions (homme mort).</li><li>Arrêt d'urgence sur le pupitre.</li><li>Cobots : analyse de risque de l'application (norme ISO/TS 15066) — un cobot avec un outil coupant reste dangereux !</li></ul>`,
  cards: [["Nombre d'axes pour position + orientation quelconques ?", "6"], ["TCP ?", "Tool Center Point : point de travail de l'outil."], ["Répétabilité ?", "Capacité à revenir au même point (± mm)."], ["Mouvement PTP ?", "Point à point articulaire : rapide, trajectoire non maîtrisée."], ["Robot pour assemblage rapide dans un plan ?", "SCARA"], ["Vitesse max en mode manuel réduit ?", "250 mm/s"]],
  quiz: [
    ["Pour souder le long d'une ligne droite, on programme un mouvement :", ["PTP", "Linéaire", "Aléatoire", "Circulaire"], 1, "L'outil suit une ligne droite."],
    ["Le robot le plus adapté au tri très rapide de petits produits :", ["Delta", "Portique géant", "AGV", "Anthropomorphe 500 kg"], 0, "Robot parallèle très rapide."],
    ["Un cobot :", ["N'a jamais besoin d'analyse de risque", "Nécessite une analyse de risque de l'application", "Est toujours sans danger", "Ne peut pas porter d'outil"], 1, "La sécurité dépend de l'application entière."],
    ["La charge utile d'un robot doit inclure :", ["Seulement la pièce", "La pièce + le préhenseur", "Le robot lui-même", "Rien"], 1, "Le préhenseur pèse aussi sur le poignet."]
  ]
}, "asservissement");

addChapter("maths", {
  id: "matrices", t: "Matrices et graphes", y: 2,
  html: `
<h3>Matrices</h3>
<div class="easy box">Une matrice est un tableau de nombres (lignes × colonnes). Elle permet de traiter beaucoup de calculs d'un coup : systèmes d'équations, changements de repère en robotique, chaînes de Markov, calculs de coûts.</div>
<p>Matrice 2×2 : A = [a b ; c d]. Addition : terme à terme. Produit A×B : « ligne de A × colonne de B » (attention, A×B ≠ B×A en général).</p>
<div class="ex box">[1 2 ; 3 4] × [5 ; 6] = [1×5 + 2×6 ; 3×5 + 4×6] = [17 ; 39].</div>
<h3>Déterminant et inverse (2×2)</h3>
<div class="f">det A = ad − bc &nbsp;&nbsp; A⁻¹ = (1/det A) × [d −b ; −c a]</div>
<p>A est inversible si det A ≠ 0. Un système AX = B se résout par X = A⁻¹B.</p>
<div class="ex box">2x + y = 7 et x + 3y = 11 → A = [2 1 ; 1 3], det = 5, A⁻¹ = 1/5 [3 −1 ; −1 2]. X = 1/5 [3×7 − 11 ; −7 + 22] = [2 ; 3].</div>
<h3>Graphes</h3><p>Un graphe = des <b>sommets</b> reliés par des <b>arêtes</b> (ou des arcs s'ils sont orientés). Applications : réseau PERT, tournée de livraison, réseau informatique, graphe des liaisons en mécanique.</p>
<ul><li><b>Degré</b> d'un sommet : nombre d'arêtes qui y arrivent.</li><li><b>Chemin</b> : suite d'arêtes ; <b>cycle</b> : chemin qui revient au départ.</li><li><b>Matrice d'adjacence</b> : 1 si deux sommets sont reliés, 0 sinon. Aⁿ compte les chemins de longueur n.</li><li>Graphe <b>pondéré</b> : chaque arête a un poids (distance, coût, durée).</li></ul>
<h3>Plus court chemin (algorithme de Dijkstra)</h3><ol><li>Distance 0 pour le départ, ∞ pour les autres.</li><li>On prend le sommet non traité le plus proche.</li><li>On met à jour les distances de ses voisins (si on trouve plus court).</li><li>On recommence jusqu'à l'arrivée.</li></ol>
<p>Le <b>chemin critique</b> d'un PERT est au contraire le chemin le <b>plus long</b>.</p>`,
  cards: [["Déterminant d'une matrice 2×2 [a b ; c d] ?", "ad − bc"], ["Condition pour qu'une matrice soit inversible ?", "det ≠ 0"], ["Résoudre AX = B ?", "X = A⁻¹B"], ["Degré d'un sommet ?", "Nombre d'arêtes qui y aboutissent."], ["Algorithme du plus court chemin ?", "Dijkstra"]],
  quiz: [
    ["det [2 3 ; 1 4] =", ["5", "11", "8", "−5"], 0, "2×4 − 3×1 = 5."],
    ["Le produit de matrices est-il commutatif ?", ["Oui toujours", "Non en général", "Seulement pour les 2×2", "Jamais défini"], 1, "A×B ≠ B×A en général."],
    ["Pour trouver le trajet le plus court entre deux ateliers :", ["Dijkstra", "Pareto", "Gantt", "Fourier"], 0, "Algorithme de plus court chemin."]
  ]
}, "fourier");
