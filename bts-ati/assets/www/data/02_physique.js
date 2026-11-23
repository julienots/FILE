COURS.push({
  id: "physique", name: "Physique-chimie", emoji: "⚛️", color: "#7C3AED",
  desc: "Mécanique du point, énergie, électricité, thermique, mécanique des fluides, ondes et chimie appliquée à l'industrie (corrosion, combustion, matériaux). Toujours avec les unités !",
  chapters: [
    {
      id: "unites", t: "Grandeurs, unités et incertitudes", y: 1,
      html: `
<h3>Les 7 unités de base du Système International (SI)</h3>
<table><tr><th>Grandeur</th><th>Unité</th><th>Symbole</th></tr>
<tr><td>Longueur</td><td>mètre</td><td>m</td></tr><tr><td>Masse</td><td>kilogramme</td><td>kg</td></tr><tr><td>Temps</td><td>seconde</td><td>s</td></tr>
<tr><td>Intensité électrique</td><td>ampère</td><td>A</td></tr><tr><td>Température</td><td>kelvin</td><td>K</td></tr><tr><td>Quantité de matière</td><td>mole</td><td>mol</td></tr><tr><td>Intensité lumineuse</td><td>candela</td><td>cd</td></tr></table>
<h3>Unités dérivées courantes</h3>
<table><tr><th>Grandeur</th><th>Unité</th><th>En unités de base</th></tr>
<tr><td>Force</td><td>newton (N)</td><td>kg·m·s⁻²</td></tr><tr><td>Énergie</td><td>joule (J)</td><td>N·m</td></tr><tr><td>Puissance</td><td>watt (W)</td><td>J/s</td></tr>
<tr><td>Pression</td><td>pascal (Pa)</td><td>N/m²</td></tr><tr><td>Tension</td><td>volt (V)</td><td>W/A</td></tr><tr><td>Fréquence</td><td>hertz (Hz)</td><td>s⁻¹</td></tr></table>
<h3>Conversions à connaître</h3>
<ul><li>1 bar = 10⁵ Pa ; 1 MPa = 1 N/mm²</li><li>1 kWh = 3,6 × 10⁶ J</li><li>T(K) = θ(°C) + 273,15</li><li>1 km/h = 1/3,6 m/s</li><li>1 L = 1 dm³ = 10⁻³ m³</li><li>1 tr/min = 2π/60 rad/s</li></ul>
<div class="warn box">Le piège n°1 au BTS : oublier de convertir. Mets TOUT en unités SI avant de calculer (mm → m, min → s, kW → W…).</div>
<h3>Incertitudes de mesure</h3>
<p>Toute mesure a une incertitude. On écrit : <b>résultat = valeur ± incertitude (unité)</b>. Ex : L = 25,3 ± 0,1 mm.</p>
<ul><li>Incertitude-type de type A : statistique (on répète la mesure) u = σ/√n.</li><li>Type B : d'après la notice de l'appareil (précision constructeur).</li><li>Incertitude relative = Δx/x (en %).</li></ul>
<h3>Chiffres significatifs</h3>
<p>Le résultat ne doit pas être plus précis que la donnée la moins précise. 2,5 × 3,14159 = 7,9 (et non 7,853975).</p>`,
      cards: [["1 bar = ? Pa", "10⁵ Pa"], ["1 kWh = ? J", "3,6 × 10⁶ J"], ["Convertir des tr/min en rad/s ?", "× 2π/60"], ["T(K) en fonction de θ(°C) ?", "T = θ + 273,15"], ["1 MPa = ? N/mm²", "1 N/mm²"]],
      quiz: [
        ["1 500 tr/min en rad/s ≈", ["157 rad/s", "25 rad/s", "9 425 rad/s", "1 500 rad/s"], 0, "1500 × 2π/60 ≈ 157 rad/s."],
        ["Le pascal correspond à :", ["N/m²", "N·m", "J/s", "kg/m³"], 0, "Pression = force / surface."],
        ["20 °C en kelvin :", ["293,15 K", "253 K", "20 K", "−253 K"], 0, "20 + 273,15."],
        ["72 km/h en m/s :", ["20 m/s", "259 m/s", "7,2 m/s", "2 m/s"], 0, "72/3,6 = 20 m/s."],
        ["L'unité de la puissance :", ["joule", "watt", "newton", "volt"], 1, "1 W = 1 J/s."]
      ]
    },
    {
      id: "cinematique", t: "Mouvement : vitesse et accélération", y: 1,
      html: `
<h3>Mouvement rectiligne uniforme (MRU)</h3>
<div class="f">v = constante &nbsp;&nbsp; x(t) = v·t + x₀</div>
<h3>Mouvement rectiligne uniformément varié (MRUV)</h3>
<div class="f">a = constante &nbsp;&nbsp; v(t) = a·t + v₀ &nbsp;&nbsp; x(t) = ½·a·t² + v₀·t + x₀ &nbsp;&nbsp; v² − v₀² = 2a·Δx</div>
<div class="ex box">Un chariot démarre et atteint 2 m/s en 4 s. a = 2/4 = 0,5 m/s². Distance parcourue : ½ × 0,5 × 4² = 4 m.</div>
<h3>Mouvement circulaire</h3>
<div class="f">v = R·ω &nbsp;&nbsp; ω = 2πN/60 (N en tr/min) &nbsp;&nbsp; a_normale = v²/R = R·ω²</div>
<div class="ex box">Une poulie de rayon 0,1 m tourne à 1 500 tr/min : ω ≈ 157 rad/s ; vitesse de la courroie v = 0,1 × 157 ≈ 15,7 m/s.</div>
<h3>Lire un graphique v(t)</h3>
<ul><li>La <b>pente</b> = accélération.</li><li>L'<b>aire</b> sous la courbe = distance parcourue.</li></ul>
<div class="tip box">Profil trapézoïdal (accélération – vitesse constante – décélération) : très utilisé pour les axes motorisés et les ascenseurs. Distance = aire du trapèze.</div>
<h3>Chute libre</h3>
<p>a = g ≈ 9,81 m/s². Hauteur de chute en t secondes : h = ½·g·t².</p>`,
      cards: [["v = ? (mouvement circulaire)", "v = R·ω"], ["Formule MRUV sans le temps ?", "v² − v₀² = 2a·Δx"], ["Dans un graphe v(t), l'aire sous la courbe = ?", "La distance parcourue."], ["g ≈ ?", "9,81 m/s²"]],
      quiz: [
        ["Un objet passe de 0 à 10 m/s en 5 s. a =", ["2 m/s²", "50 m/s²", "0,5 m/s²", "5 m/s²"], 0, "a = Δv/Δt = 10/5."],
        ["Roue de rayon 0,3 m à ω = 10 rad/s. v =", ["3 m/s", "33 m/s", "0,03 m/s", "30 m/s"], 0, "v = Rω."],
        ["Dans un graphe v(t), la pente représente :", ["La distance", "L'accélération", "La position", "La masse"], 1, "a = dv/dt."]
      ]
    },
    {
      id: "dynamique", t: "Forces, lois de Newton et dynamique", y: 1,
      html: `
<h3>Les 3 lois de Newton</h3>
<ol>
<li><b>Principe d'inertie</b> : si la somme des forces est nulle, l'objet est immobile ou en MRU.</li>
<li><b>Principe fondamental de la dynamique (PFD)</b> : ΣF = m·a</li>
<li><b>Actions réciproques</b> : si A pousse B avec F, B pousse A avec −F.</li></ol>
<div class="f">ΣF = m·a &nbsp;&nbsp;&nbsp; Poids : P = m·g</div>
<div class="ex box">Pour accélérer un chariot de 200 kg à 0,5 m/s² (sans frottement) : F = 200 × 0,5 = 100 N.</div>
<h3>En rotation</h3>
<div class="f">ΣC = J·α &nbsp;&nbsp; <small>C = couple (N·m), J = moment d'inertie (kg·m²), α = accélération angulaire (rad/s²)</small></div>
<p>Un moteur doit fournir un couple pour vaincre la charge (couple résistant) ET pour accélérer (J·α).</p>
<h3>Frottements</h3>
<div class="f">T = f·N &nbsp;&nbsp; <small>f = coefficient de frottement (tan φ), N = effort normal</small></div>
<h3>Plan incliné</h3>
<p>Le poids se décompose : composante le long de la pente m·g·sin α, composante perpendiculaire m·g·cos α.</p>
<div class="ex box">Monter une caisse de 50 kg sur une pente de 10° sans frottement à vitesse constante : F = 50 × 9,81 × sin 10° ≈ 85 N.</div>
<h3>Quantité de mouvement</h3>
<p>p = m·v. Conservée lors des chocs pour un système isolé.</p>`,
      cards: [["PFD ?", "ΣF = m·a"], ["PFD en rotation ?", "ΣC = J·α"], ["Poids ?", "P = m·g"], ["Composante du poids le long d'une pente α ?", "m·g·sin α"]],
      quiz: [
        ["Masse 10 kg, a = 2 m/s². Force résultante =", ["20 N", "5 N", "12 N", "98 N"], 0, "F = ma."],
        ["Le poids d'une masse de 100 kg ≈", ["100 N", "981 N", "10 N", "9,81 N"], 1, "P = 100 × 9,81."],
        ["Si ΣF = 0, l'objet :", ["Accélère forcément", "Est immobile ou en MRU", "Tombe", "Tourne"], 1, "Principe d'inertie."]
      ]
    },
    {
      id: "energie", t: "Travail, énergie, puissance et rendement", y: 1,
      html: `
<h3>Travail d'une force</h3>
<div class="f">W = F·d·cos θ &nbsp;&nbsp; (J)</div>
<h3>Énergies</h3>
<div class="f">Énergie cinétique Ec = ½·m·v² &nbsp;&nbsp; (rotation : ½·J·ω²)</div>
<div class="f">Énergie potentielle de pesanteur Ep = m·g·h</div>
<div class="f">Énergie potentielle élastique (ressort) = ½·k·x²</div>
<p>Énergie mécanique Em = Ec + Ep. Sans frottement, Em se conserve.</p>
<h3>Puissance</h3>
<div class="f">P = W/t &nbsp;&nbsp; P = F·v (translation) &nbsp;&nbsp; P = C·ω (rotation)</div>
<div class="ex box">Moteur : C = 20 N·m à 1 450 tr/min. ω = 151,8 rad/s → P = 20 × 151,8 ≈ 3,04 kW.</div>
<h3>Rendement</h3>
<div class="f">η = P_utile / P_absorbée &nbsp;&nbsp; (toujours &lt; 1) &nbsp;&nbsp; Pertes = P_abs − P_utile</div>
<div class="key box">Chaîne de transmission : les rendements se <b>multiplient</b>. Moteur 0,9 × réducteur 0,95 × courroie 0,97 = 0,83.</div>
<div class="ex box">Pour lever 500 kg à 0,2 m/s : P_utile = m·g·v = 500 × 9,81 × 0,2 ≈ 981 W. Avec η = 0,8 : le moteur doit absorber 981/0,8 ≈ 1,23 kW.</div>
<h3>Chaîne d'énergie</h3>
<p>Alimenter → Distribuer → Convertir → Transmettre → Agir. Exemple : réseau → contacteur/variateur → moteur → réducteur → tapis.</p>`,
      cards: [["Énergie cinétique ?", "Ec = ½ m v²"], ["Puissance en rotation ?", "P = C·ω"], ["Rendement ?", "η = P_utile / P_absorbée"], ["Rendement global d'une chaîne ?", "Produit des rendements."], ["Énergie potentielle de pesanteur ?", "Ep = m·g·h"]],
      quiz: [
        ["Ec d'une masse de 2 kg à 3 m/s =", ["9 J", "6 J", "18 J", "3 J"], 0, "½ × 2 × 9 = 9 J."],
        ["C = 10 N·m, ω = 100 rad/s. P =", ["1 000 W", "10 W", "0,1 W", "110 W"], 0, "P = Cω."],
        ["η1 = 0,9 et η2 = 0,8. η global =", ["1,7", "0,72", "0,85", "0,1"], 1, "On multiplie."],
        ["P absorbée 2 kW, rendement 0,75. P utile =", ["1,5 kW", "2,67 kW", "0,5 kW", "2,75 kW"], 0, "2 × 0,75."]
      ]
    },
    {
      id: "electricite", t: "Électricité : lois fondamentales en continu", y: 1,
      html: `
<h3>Les grandeurs</h3>
<ul><li><b>Tension U</b> (V) : la « pression » électrique, mesurée en parallèle au voltmètre.</li><li><b>Intensité I</b> (A) : le « débit » d'électrons, mesuré en série à l'ampèremètre.</li><li><b>Résistance R</b> (Ω) : l'opposition au passage du courant.</li></ul>
<div class="easy box">Analogie de l'eau : la tension = différence de hauteur entre deux réservoirs, le courant = débit dans le tuyau, la résistance = étroitesse du tuyau.</div>
<h3>Loi d'Ohm et puissance</h3>
<div class="f">U = R·I &nbsp;&nbsp; P = U·I = R·I² = U²/R &nbsp;&nbsp; W = P·t</div>
<h3>Lois de Kirchhoff</h3>
<ul><li><b>Loi des nœuds</b> : la somme des courants qui entrent = la somme de ceux qui sortent.</li><li><b>Loi des mailles</b> : la somme algébrique des tensions dans une boucle = 0.</li></ul>
<h3>Associations de résistances</h3>
<div class="f">Série : Req = R₁ + R₂ &nbsp;&nbsp; Parallèle : 1/Req = 1/R₁ + 1/R₂ (deux : R₁R₂/(R₁+R₂))</div>
<h3>Diviseur de tension</h3>
<div class="f">U₂ = U × R₂/(R₁ + R₂)</div>
<h3>Résistance d'un fil</h3>
<div class="f">R = ρ·L/S &nbsp;&nbsp; <small>ρ cuivre ≈ 1,7 × 10⁻⁸ Ω·m</small></div>
<div class="ex box">Câble cuivre 2 × 50 m (aller-retour = 100 m), section 2,5 mm² : R = 1,7×10⁻⁸ × 100 / 2,5×10⁻⁶ = 0,68 Ω. Sous 16 A, chute de tension = 10,9 V.</div>
<h3>Condensateur et bobine</h3>
<div class="f">i = C·du/dt &nbsp;&nbsp; u = L·di/dt &nbsp;&nbsp; τ = RC ou τ = L/R</div>
<ul><li>Le condensateur s'oppose aux variations brusques de <b>tension</b>.</li><li>La bobine s'oppose aux variations brusques de <b>courant</b>.</li></ul>
<h3>Effet Joule</h3><p>Tout conducteur parcouru par un courant chauffe : P = R·I². C'est utile (radiateur) ou une perte (câbles, moteurs).</p>`,
      cards: [["Loi d'Ohm ?", "U = R·I"], ["P en fonction de R et I ?", "P = R·I²"], ["Résistances en série ?", "Req = R₁ + R₂ + …"], ["Deux résistances en parallèle ?", "Req = R₁R₂/(R₁+R₂)"], ["Résistance d'un fil ?", "R = ρL/S"], ["Comment brancher un ampèremètre ?", "En série."]],
      quiz: [
        ["U = 12 V, R = 4 Ω. I =", ["3 A", "48 A", "0,33 A", "16 A"], 0, "I = U/R."],
        ["Deux résistances de 10 Ω en parallèle :", ["20 Ω", "5 Ω", "10 Ω", "100 Ω"], 1, "10×10/20 = 5 Ω."],
        ["Un voltmètre se branche :", ["En série", "En parallèle", "Sur la terre", "N'importe comment"], 1, "La tension se mesure entre deux points."],
        ["Une résistance de 100 Ω parcourue par 0,5 A dissipe :", ["25 W", "50 W", "200 W", "5 W"], 0, "RI² = 100 × 0,25."],
        ["Si on double la longueur d'un câble, sa résistance :", ["Est divisée par 2", "Double", "Ne change pas", "Est multipliée par 4"], 1, "R = ρL/S, proportionnelle à L."]
      ]
    },
    {
      id: "alternatif", t: "Courant alternatif monophasé et puissances", y: 1,
      html: `
<h3>Le signal sinusoïdal</h3>
<div class="f">u(t) = Û·sin(ωt + φ) &nbsp;&nbsp; ω = 2πf &nbsp;&nbsp; T = 1/f &nbsp;&nbsp; U_eff = Û/√2</div>
<p>Réseau français : 230 V efficaces, 50 Hz → Û ≈ 325 V, T = 20 ms.</p>
<h3>Impédances</h3>
<table><tr><th>Dipôle</th><th>Impédance Z</th><th>Déphasage u/i</th></tr>
<tr><td>R</td><td>R</td><td>0</td></tr><tr><td>L</td><td>Lω</td><td>+90°</td></tr><tr><td>C</td><td>1/(Cω)</td><td>−90°</td></tr>
<tr><td>R-L série</td><td>√(R² + (Lω)²)</td><td>0 à +90°</td></tr></table>
<div class="f">U = Z·I (en valeurs efficaces)</div>
<h3>Les 3 puissances</h3>
<div class="f">P = U·I·cos φ (W) &nbsp;&nbsp; Q = U·I·sin φ (var) &nbsp;&nbsp; S = U·I (VA) &nbsp;&nbsp; S² = P² + Q²</div>
<div class="easy box">Imagine une chope de bière : la bière = puissance <b>active P</b> (utile, celle qui fait le travail), la mousse = puissance <b>réactive Q</b> (nécessaire aux champs magnétiques, mais ne fait pas de travail), la chope entière = puissance <b>apparente S</b>.</div>
<h3>Facteur de puissance</h3>
<div class="f">k = cos φ = P/S</div>
<p>Un mauvais cos φ (moteurs, bobines) augmente le courant pour la même puissance utile → câbles plus gros, pertes, pénalités du fournisseur.</p>
<h3>Relever le facteur de puissance</h3>
<p>On ajoute des <b>condensateurs</b> (qui fournissent du réactif) :</p>
<div class="f">Q_C = P(tan φ − tan φ') &nbsp;&nbsp; C = Q_C/(U²·ω)</div>
<div class="ex box">Installation : P = 10 kW, cos φ = 0,7 (tan φ = 1,02). On veut cos φ' = 0,93 (tan φ' = 0,4). Q_C = 10 000 × 0,62 = 6,2 kvar. C = 6200/(230² × 314) ≈ 373 µF.</div>
<h3>Théorème de Boucherot</h3><p>Les puissances actives s'additionnent, les puissances réactives s'additionnent (algébriquement), mais PAS les apparentes : S_tot = √(P_tot² + Q_tot²).</p>`,
      cards: [["U_eff en fonction de Û ?", "Û/√2"], ["Puissance active monophasée ?", "P = U·I·cos φ"], ["Unité de la puissance réactive ?", "var (volt-ampère réactif)"], ["Relation entre P, Q, S ?", "S² = P² + Q²"], ["Comment relever le cos φ ?", "Ajouter des condensateurs."], ["Boucherot ?", "On additionne les P et les Q, pas les S."]],
      quiz: [
        ["Réseau 230 V, valeur crête ≈", ["230 V", "325 V", "400 V", "163 V"], 1, "230 × √2 ≈ 325 V."],
        ["U = 230 V, I = 10 A, cos φ = 0,8. P =", ["2 300 W", "1 840 W", "2 875 W", "1 380 W"], 1, "230 × 10 × 0,8."],
        ["La puissance réactive s'exprime en :", ["W", "VA", "var", "J"], 2, "var = volt-ampère réactif."],
        ["P = 3 kW, Q = 4 kvar. S =", ["7 kVA", "5 kVA", "1 kVA", "12 kVA"], 1, "√(9 + 16) = 5."],
        ["Pour améliorer le facteur de puissance on ajoute :", ["Des bobines", "Des résistances", "Des condensateurs", "Des fusibles"], 2, "Ils compensent le réactif inductif."]
      ]
    },
    {
      id: "thermique", t: "Thermique et thermodynamique", y: 2,
      html: `
<h3>Chaleur et température</h3>
<div class="easy box">La température mesure l'agitation des molécules. La chaleur (Q, en joules) est l'énergie qui passe d'un corps chaud vers un corps froid.</div>
<div class="f">Q = m·c·Δθ &nbsp;&nbsp; <small>c = capacité thermique massique (eau : 4 180 J/(kg·K))</small></div>
<div class="ex box">Chauffer 200 L d'eau (200 kg) de 15 à 60 °C : Q = 200 × 4180 × 45 ≈ 37,6 MJ ≈ 10,4 kWh.</div>
<h3>Changement d'état</h3>
<div class="f">Q = m·L &nbsp;&nbsp; <small>L = chaleur latente (vaporisation de l'eau ≈ 2 260 kJ/kg)</small></div>
<h3>Transferts thermiques</h3>
<ul><li><b>Conduction</b> (dans un solide) : Φ = ΔT / R_th avec R_th = e/(λ·S) pour une paroi.</li><li><b>Convection</b> (par un fluide) : Φ = h·S·ΔT.</li><li><b>Rayonnement</b> (ondes infrarouges, même dans le vide).</li></ul>
<div class="key box">Les résistances thermiques s'additionnent en série, comme en électricité ! Flux Φ (W) ↔ courant, ΔT ↔ tension. Isolant = λ petit.</div>
<div class="ex box">Dissipateur de transistor : P = 20 W, R_th jonction-ambiant = 2,5 K/W, ambiant 30 °C → T_jonction = 30 + 20 × 2,5 = 80 °C.</div>
<h3>Gaz parfaits</h3>
<div class="f">P·V = n·R·T &nbsp;&nbsp; <small>R = 8,314 J/(mol·K), T en kelvin !</small></div>
<p>À température constante : P₁V₁ = P₂V₂ (loi de Boyle-Mariotte), utile pour les réservoirs d'air comprimé.</p>
<h3>1er principe de la thermodynamique</h3>
<div class="f">ΔU = W + Q</div>
<p>L'énergie se conserve : elle se transforme (travail ↔ chaleur).</p>
<h3>Machines thermiques</h3>
<ul><li>Moteur thermique : rendement η = W/Q_chaud (limite de Carnot : 1 − T_froid/T_chaud).</li><li>Pompe à chaleur : COP = Q_chaud/W (souvent 3 à 4).</li><li>Réfrigérateur : efficacité = Q_froid/W.</li></ul>`,
      cards: [["Q = ? (échauffement)", "Q = m·c·Δθ"], ["c de l'eau ?", "≈ 4 180 J/(kg·K)"], ["Loi des gaz parfaits ?", "PV = nRT (T en K)"], ["Résistance thermique d'une paroi ?", "R_th = e/(λS)"], ["COP d'une pompe à chaleur ?", "Q_chaud / W électrique"]],
      quiz: [
        ["Chauffer 1 kg d'eau de 10 °C demande environ :", ["4 180 J", "41 800 J", "418 J", "10 J"], 1, "1 × 4180 × 10."],
        ["Un bon isolant a une conductivité λ :", ["Grande", "Petite", "Nulle forcément", "Négative"], 1, "Plus λ est faible, mieux ça isole."],
        ["Dans PV = nRT, T est en :", ["°C", "K", "°F", "Pa"], 1, "Toujours en kelvins."],
        ["P₁ = 2 bar, V₁ = 10 L, compression isotherme à V₂ = 5 L : P₂ =", ["1 bar", "4 bar", "10 bar", "20 bar"], 1, "P₁V₁ = P₂V₂."]
      ]
    },
    {
      id: "fluides", t: "Mécanique des fluides", y: 2,
      html: `
<h3>Pression et hydrostatique</h3>
<div class="f">p = F/S &nbsp;&nbsp; Δp = ρ·g·h</div>
<div class="ex box">À 10 m de profondeur dans l'eau : Δp = 1000 × 9,81 × 10 ≈ 1 bar de plus.</div>
<p><b>Principe de Pascal</b> : une pression appliquée se transmet intégralement dans tout le fluide → presse hydraulique, vérins.</p>
<div class="f">F₁/S₁ = F₂/S₂</div>
<h3>Poussée d'Archimède</h3><div class="f">F_A = ρ_fluide·V_immergé·g</div>
<h3>Débit</h3>
<div class="f">Qv = S·v (m³/s) &nbsp;&nbsp; Qm = ρ·Qv (kg/s)</div>
<p><b>Conservation du débit</b> : S₁v₁ = S₂v₂. Si le tuyau rétrécit, la vitesse augmente.</p>
<h3>Théorème de Bernoulli (fluide parfait)</h3>
<div class="f">p + ½ρv² + ρgz = constante</div>
<p>Avec une pompe : on ajoute l'énergie de la pompe ; avec des pertes de charge, on en retire.</p>
<h3>Puissance d'une pompe</h3>
<div class="f">P_hydraulique = Qv·Δp &nbsp;&nbsp; P_absorbée = Qv·Δp / η</div>
<div class="ex box">Pompe : 36 m³/h (= 0,01 m³/s), Δp = 3 bar (3 × 10⁵ Pa), η = 0,7 → P = 0,01 × 3×10⁵ / 0,7 ≈ 4,3 kW.</div>
<h3>Régimes d'écoulement</h3>
<div class="f">Re = ρ·v·D/μ = v·D/ν</div>
<p>Re &lt; 2 000 : laminaire ; Re &gt; 4 000 : turbulent. Les pertes de charge dépendent du régime (diagramme de Moody).</p>
<h3>Pertes de charge</h3><p>Régulières (frottement le long des tuyaux, ∝ longueur) et singulières (coudes, vannes, rétrécissements).</p>`,
      cards: [["Pression hydrostatique ?", "Δp = ρ·g·h"], ["Débit volumique ?", "Qv = S·v"], ["Conservation du débit ?", "S₁v₁ = S₂v₂"], ["Bernoulli ?", "p + ½ρv² + ρgz = cste"], ["Puissance hydraulique ?", "P = Qv·Δp"], ["Re < 2000 ?", "Écoulement laminaire."]],
      quiz: [
        ["Si la section d'un tuyau est divisée par 2, la vitesse :", ["Est divisée par 2", "Double", "Ne change pas", "Est ×4"], 1, "S·v = constante."],
        ["Qv = 0,002 m³/s, Δp = 10⁵ Pa. P hydraulique =", ["200 W", "2 000 W", "20 W", "0,2 W"], 0, "0,002 × 100 000 = 200 W."],
        ["Le principe de Pascal explique :", ["Les presses hydrauliques", "La flottaison", "La foudre", "Le frottement"], 0, "La pression se transmet partout."],
        ["Re = 10 000 : l'écoulement est", ["Laminaire", "Turbulent", "Nul", "Critique"], 1, "Au-delà de ~4 000 : turbulent."]
      ]
    },
    {
      id: "ondes", t: "Ondes, acoustique et lumière", y: 2,
      html: `
<h3>Onde progressive</h3>
<div class="f">λ = v·T = v/f</div>
<ul><li>Son dans l'air : v ≈ 340 m/s.</li><li>Lumière : c = 3 × 10⁸ m/s.</li></ul>
<h3>Acoustique</h3>
<div class="f">L = 10·log(I/I₀) (dB) &nbsp;&nbsp; I₀ = 10⁻¹² W/m²</div>
<div class="key box">Doubler le nombre de sources identiques = +3 dB (et non ×2 !). Seuil de danger : 80–85 dB(A) sur 8 h → protections auditives obligatoires.</div>
<h3>Applications industrielles</h3>
<ul><li>Capteurs à ultrasons (mesure de distance : d = v·t/2).</li><li>Contrôle non destructif par ultrasons (fissures).</li><li>Analyse vibratoire (maintenance conditionnelle).</li><li>Fibre optique, lasers, cellules photoélectriques.</li></ul>
<h3>Lumière et photométrie</h3>
<p>Flux lumineux en lumen (lm), éclairement en lux (lx = lm/m²). Un poste de travail demande en général 300 à 500 lx, plus pour un travail de précision.</p>
<h3>Énergie d'un photon</h3><div class="f">E = h·f = h·c/λ &nbsp;&nbsp; <small>h = 6,63 × 10⁻³⁴ J·s</small></div>`,
      cards: [["Longueur d'onde ?", "λ = v/f"], ["Vitesse du son dans l'air ?", "≈ 340 m/s"], ["Deux sources sonores identiques : gain ?", "+3 dB"], ["Mesure de distance par ultrasons ?", "d = v·t/2 (aller-retour)"]],
      quiz: [
        ["Un son de 340 Hz dans l'air a une longueur d'onde de :", ["1 m", "340 m", "0,1 m", "3,4 m"], 0, "λ = 340/340."],
        ["Deux machines de 80 dB chacune donnent :", ["160 dB", "83 dB", "80 dB", "86 dB"], 1, "+3 dB quand on double l'intensité."],
        ["Écho ultrason reçu après 2 ms (v = 340 m/s). Distance :", ["0,68 m", "0,34 m", "680 m", "6,8 m"], 1, "340 × 0,002 / 2 = 0,34 m."]
      ]
    },
    {
      id: "chimie", t: "Chimie appliquée : matière, combustion, corrosion", y: 2,
      html: `
<h3>Matière et quantité de matière</h3>
<div class="f">n = m/M &nbsp;&nbsp; C = n/V (mol/L) &nbsp;&nbsp; ρ = m/V</div>
<h3>Acides et bases</h3>
<p>pH &lt; 7 : acide ; pH = 7 : neutre ; pH &gt; 7 : basique. pH = −log[H₃O⁺]. Les produits corrosifs nécessitent des EPI (lunettes, gants) et un stockage séparé (voir pictogrammes SGH).</p>
<h3>Combustion</h3>
<p>Combustible + comburant (O₂) + énergie d'activation = <b>triangle du feu</b>. Supprimer un des trois éteint le feu.</p>
<div class="f">CH₄ + 2 O₂ → CO₂ + 2 H₂O</div>
<p>Combustion incomplète → monoxyde de carbone CO (gaz mortel, inodore).</p>
<h3>Oxydoréduction</h3>
<div class="easy box">Oxydation = <b>perte</b> d'électrons ; réduction = <b>gain</b> d'électrons. Moyen mnémotechnique : « l'Oxydant Gagne » (des électrons).</div>
<h3>Corrosion</h3>
<p>Le fer s'oxyde (rouille) en présence d'eau et d'oxygène. Quand deux métaux différents se touchent dans un milieu humide, le métal le <b>moins noble</b> se corrode (corrosion galvanique).</p>
<h4>Protections</h4>
<ul><li>Peinture, revêtements (barrière).</li><li><b>Galvanisation</b> (zinc) : le zinc se corrode à la place du fer.</li><li><b>Anode sacrificielle</b> (zinc ou magnésium sur les coques de bateaux, chauffe-eau).</li><li>Protection cathodique par courant imposé.</li><li>Choix de matériaux (inox, aluminium anodisé).</li></ul>
<h3>Piles, accumulateurs, électrolyse</h3>
<p>Une pile convertit de l'énergie chimique en énergie électrique. Capacité en A·h : un accumulateur de 50 A·h fournit 5 A pendant 10 h. Énergie = capacité × tension (Wh). L'électrolyse fait l'inverse : chromage, zingage, production d'hydrogène.</p>
<h3>Matériaux (rappel)</h3>
<ul><li>Métaux : aciers (fer + carbone &lt; 2 %), fontes (&gt; 2 %), alliages d'alu, de cuivre (laiton = Cu + Zn, bronze = Cu + Sn).</li><li>Polymères : thermoplastiques (recyclables à chaud : PE, PP, PVC, PA), thermodurcissables (époxy).</li><li>Composites : fibres (verre, carbone) + matrice.</li></ul>`,
      cards: [["n = ?", "n = m/M (mol)"], ["Triangle du feu ?", "Combustible + comburant + énergie d'activation."], ["Oxydation = ?", "Perte d'électrons."], ["Principe de la galvanisation ?", "Le zinc, moins noble, se corrode à la place du fer."], ["Différence acier / fonte ?", "Acier < 2 % de carbone, fonte > 2 %."], ["Laiton = ?", "Cuivre + zinc."]],
      quiz: [
        ["Un pH de 3 correspond à une solution :", ["Basique", "Neutre", "Acide", "Salée"], 2, "pH < 7 = acide."],
        ["Pour protéger une coque en acier, on fixe une anode en :", ["Cuivre", "Or", "Zinc", "Inox"], 2, "Le zinc est moins noble : il se sacrifie."],
        ["Une combustion incomplète produit surtout :", ["CO (monoxyde de carbone)", "O₂", "N₂", "H₂"], 0, "Gaz toxique et inodore."],
        ["Batterie 12 V – 60 A·h. Énergie stockée :", ["720 Wh", "72 Wh", "5 Wh", "60 Wh"], 0, "12 × 60."],
        ["Un thermoplastique :", ["Ne se recycle pas", "Peut être refondu", "Est un métal", "Est toujours un composite"], 1, "Il ramollit à la chaleur."]
      ]
    }
  ]
});
