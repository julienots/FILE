// Calculatrices techniques. inputs: [clé, libellé, valeur par défaut, unité]. calc(v) -> [[libellé, valeur, unité], ...] ou HTML.
var TOOLS = [
  { id: "ohm", emoji: "Ω", name: "Loi d'Ohm & puissance", desc: "Remplis 2 valeurs sur 3 (laisse la 3ᵉ vide).",
    inputs: [["U", "Tension U", "24", "V"], ["I", "Courant I", "", "A"], ["R", "Résistance R", "12", "Ω"]],
    formula: "U = R·I &nbsp;&nbsp; P = U·I = R·I² = U²/R",
    calc: function (v) {
      var U = v.U, I = v.I, R = v.R;
      if (isNaN(U)) U = R * I; else if (isNaN(I)) I = U / R; else if (isNaN(R)) R = U / I;
      return [["Tension U", U, "V"], ["Courant I", I, "A"], ["Résistance R", R, "Ω"], ["Puissance P", U * I, "W"]];
    } },
  { id: "tri", emoji: "⚡", name: "Puissances mono / triphasé", desc: "P, Q, S à partir de U, I et cos φ.",
    inputs: [["U", "Tension (entre phases en tri)", "400", "V"], ["I", "Courant en ligne", "10", "A"], ["k", "cos φ", "0.85", ""], ["n", "1 = monophasé, 3 = triphasé", "3", ""]],
    formula: "Tri : P = √3·U·I·cos φ &nbsp; Mono : P = U·I·cos φ",
    calc: function (v) {
      var f = v.n === 1 ? 1 : Math.sqrt(3), S = f * v.U * v.I, phi = Math.acos(v.k);
      return [["Puissance active P", S * v.k, "W"], ["Puissance réactive Q", S * Math.sin(phi), "var"], ["Puissance apparente S", S, "VA"], ["Déphasage φ", phi * 180 / Math.PI, "°"]];
    } },
  { id: "moteur", emoji: "🔄", name: "Moteur asynchrone", desc: "Vitesse de synchronisme, glissement, couple utile.",
    inputs: [["f", "Fréquence", "50", "Hz"], ["p", "Paires de pôles", "2", ""], ["n", "Vitesse réelle", "1450", "tr/min"], ["P", "Puissance utile", "4000", "W"]],
    formula: "ns = 60f/p &nbsp; g = (ns − n)/ns &nbsp; C = P/Ω",
    calc: function (v) {
      var ns = 60 * v.f / v.p, w = v.n * 2 * Math.PI / 60;
      return [["Vitesse de synchronisme ns", ns, "tr/min"], ["Glissement g", 100 * (ns - v.n) / ns, "%"], ["Vitesse angulaire Ω", w, "rad/s"], ["Couple utile C", v.P / w, "N·m"]];
    } },
  { id: "verin", emoji: "💨", name: "Vérin pneumatique / hydraulique", desc: "Efforts théoriques de sortie et de rentrée.",
    inputs: [["D", "Diamètre du piston", "50", "mm"], ["d", "Diamètre de la tige", "20", "mm"], ["p", "Pression", "6", "bar"], ["t", "Taux de charge", "0.7", ""]],
    formula: "F = p × S &nbsp; S = πD²/4 &nbsp; (1 bar = 0,1 MPa = 0,1 N/mm²)",
    note: "Le taux de charge (0,5 à 0,8) tient compte des frottements et de la dynamique : on choisit le vérin pour que l'effort utile = taux × effort théorique.",
    calc: function (v) {
      var p = v.p * 0.1, Ss = Math.PI * v.D * v.D / 4, Sr = Math.PI * (v.D * v.D - v.d * v.d) / 4;
      return [["Effort théorique sortie", p * Ss, "N"], ["Effort théorique rentrée", p * Sr, "N"], ["Effort utile sortie", p * Ss * v.t, "N"], ["Section piston", Ss, "mm²"]];
    } },
  { id: "rotation", emoji: "⚙️", name: "Vitesses de rotation", desc: "tr/min ↔ rad/s et vitesse linéaire à la périphérie.",
    inputs: [["N", "Vitesse", "1500", "tr/min"], ["D", "Diamètre (poulie, roue, outil)", "200", "mm"]],
    formula: "ω = 2πN/60 &nbsp; v = R·ω &nbsp; Vc = πDN/1000 (m/min)",
    calc: function (v) {
      var w = v.N * 2 * Math.PI / 60;
      return [["Vitesse angulaire ω", w, "rad/s"], ["Vitesse périphérique v", w * v.D / 2000, "m/s"], ["Vitesse de coupe Vc", Math.PI * v.D * v.N / 1000, "m/min"], ["Fréquence", v.N / 60, "tr/s"]];
    } },
  { id: "engrenage", emoji: "🔩", name: "Engrenages / poulies", desc: "Vitesse et couple de sortie d'un étage.",
    inputs: [["Z1", "Dents (ou Ø) menant", "20", ""], ["Z2", "Dents (ou Ø) mené", "60", ""], ["N1", "Vitesse d'entrée", "1450", "tr/min"], ["C1", "Couple d'entrée", "10", "N·m"], ["eta", "Rendement", "0.97", ""]],
    formula: "r = Z1/Z2 &nbsp; N2 = r·N1 &nbsp; C2 = η·C1/r",
    calc: function (v) {
      var r = v.Z1 / v.Z2;
      return [["Rapport r", r, ""], ["Vitesse de sortie", r * v.N1, "tr/min"], ["Couple de sortie", v.eta * v.C1 / r, "N·m"], ["Puissance de sortie", v.eta * v.C1 * v.N1 * 2 * Math.PI / 60, "W"]];
    } },
  { id: "analog", emoji: "📈", name: "Capteur 4–20 mA / 0–10 V", desc: "Mise à l'échelle d'un signal analogique.",
    inputs: [["x", "Signal mesuré", "12", "mA ou V"], ["a", "Signal min (4 ou 0)", "4", ""], ["b", "Signal max (20 ou 10)", "20", ""], ["m", "Mesure min", "0", ""], ["M", "Mesure max", "100", ""]],
    formula: "Valeur = Min + (x − a)/(b − a) × (Max − Min)",
    calc: function (v) {
      var r = v.m + (v.x - v.a) / (v.b - v.a) * (v.M - v.m);
      if (v.a === 4 && v.x < 3.8) return '<b style="color:var(--bad)">⚠️ Signal &lt; 4 mA : défaut probable (câble coupé, capteur HS).</b>';
      return [["Valeur physique", r, ""], ["Pourcentage de l'étendue", 100 * (v.x - v.a) / (v.b - v.a), "%"]];
    } },
  { id: "cable", emoji: "🔌", name: "Câble : résistance et chute de tension", desc: "Monophasé (aller-retour) ou triphasé.",
    inputs: [["L", "Longueur de la ligne (aller)", "30", "m"], ["S", "Section", "2.5", "mm²"], ["I", "Courant", "16", "A"], ["U", "Tension nominale", "230", "V"], ["n", "1 = mono, 3 = tri", "1", ""], ["rho", "Résistivité (cuivre 0,0225 en chaud, alu 0,036)", "0.0225", "Ω·mm²/m"]],
    formula: "R = ρL/S &nbsp; Mono : ΔU = 2·R·I·cos φ &nbsp; Tri : ΔU = √3·R·I·cos φ (cos φ pris = 1 ici)",
    note: "Limites usuelles en installation : 3 % pour l'éclairage, 5 % pour les autres usages (selon la norme NF C 15-100 et le type d'alimentation).",
    calc: function (v) {
      var R = v.rho * v.L / v.S, dU = (v.n === 3 ? Math.sqrt(3) : 2) * R * v.I;
      return [["Résistance d'un conducteur", R, "Ω"], ["Chute de tension ΔU", dU, "V"], ["Chute en %", 100 * dU / v.U, "%"], ["Pertes Joule", (v.n === 3 ? 3 : 2) * R * v.I * v.I, "W"]];
    } },
  { id: "cosphi", emoji: "🔋", name: "Compensation du cos φ", desc: "Condensateurs pour relever le facteur de puissance.",
    inputs: [["P", "Puissance active", "10000", "W"], ["k1", "cos φ actuel", "0.7", ""], ["k2", "cos φ visé", "0.93", ""], ["U", "Tension entre phases (ou mono)", "400", "V"], ["f", "Fréquence", "50", "Hz"]],
    formula: "Qc = P(tan φ − tan φ') &nbsp; Tri, condensateurs en triangle : C = Qc/(3U²ω) &nbsp; Mono : C = Qc/(U²ω)",
    calc: function (v) {
      var t1 = Math.tan(Math.acos(v.k1)), t2 = Math.tan(Math.acos(v.k2)), Qc = v.P * (t1 - t2), w = 2 * Math.PI * v.f;
      return [["Puissance réactive à compenser Qc", Qc, "var"], ["C par phase (tri, en triangle)", 1e6 * Qc / (3 * v.U * v.U * w), "µF"], ["C (monophasé)", 1e6 * Qc / (v.U * v.U * w), "µF"], ["Baisse du courant", 100 * (1 - v.k1 / v.k2), "%"]];
    } },
  { id: "rdm", emoji: "🏗️", name: "RDM : traction et torsion", desc: "Diamètre minimal d'une barre en traction ou d'un arbre en torsion.",
    inputs: [["N", "Effort de traction", "20000", "N"], ["Re", "Limite élastique Re", "235", "MPa"], ["s", "Coefficient de sécurité", "2", ""], ["P", "Puissance transmise (arbre)", "5000", "W"], ["n", "Vitesse de l'arbre", "1000", "tr/min"], ["tau", "τ admissible", "40", "MPa"]],
    formula: "Traction : S ≥ N·s/Re &nbsp; Torsion : d ≥ ∛(16·Mt/(π·τ)) avec Mt = P/ω",
    calc: function (v) {
      var S = v.N * v.s / v.Re, d = Math.sqrt(4 * S / Math.PI), Mt = v.P / (v.n * 2 * Math.PI / 60) * 1000, d2 = Math.cbrt(16 * Mt / (Math.PI * v.tau));
      return [["Traction : section minimale", S, "mm²"], ["Traction : diamètre minimal", d, "mm"], ["Torsion : moment Mt", Mt / 1000, "N·m"], ["Torsion : diamètre minimal", d2, "mm"]];
    } },
  { id: "hydro", emoji: "🛢️", name: "Hydraulique", desc: "Puissance, effort et vitesse d'un vérin hydraulique.",
    inputs: [["p", "Pression", "150", "bar"], ["Q", "Débit", "20", "L/min"], ["D", "Diamètre du vérin", "100", "mm"], ["eta", "Rendement global", "0.85", ""]],
    formula: "P(kW) = p(bar)·Q(L/min)/600 &nbsp; F = p·S &nbsp; v = Q/S",
    calc: function (v) {
      var S = Math.PI * v.D * v.D / 4;
      return [["Puissance hydraulique", v.p * v.Q / 600, "kW"], ["Puissance moteur nécessaire", v.p * v.Q / 600 / v.eta, "kW"], ["Effort du vérin", v.p * 0.1 * S / 1000, "kN"], ["Vitesse de sortie", v.Q * 1e6 / 60 / S, "mm/s"]];
    } },
  { id: "trs", emoji: "📊", name: "TRS / disponibilité", desc: "Taux de rendement synthétique d'un poste.",
    inputs: [["T", "Temps d'ouverture", "480", "min"], ["A", "Arrêts (pannes, réglages)", "60", "min"], ["tc", "Temps de cycle théorique", "1", "min/pièce"], ["N", "Pièces produites", "380", ""], ["B", "Pièces bonnes", "370", ""]],
    formula: "TRS = Disponibilité × Performance × Qualité",
    calc: function (v) {
      var tf = v.T - v.A, D = tf / v.T, P = v.N * v.tc / tf, Q = v.B / v.N;
      return [["Disponibilité", 100 * D, "%"], ["Performance", 100 * P, "%"], ["Qualité", 100 * Q, "%"], ["TRS", 100 * D * P * Q, "%"]];
    } },
  { id: "deg2", emoji: "📐", name: "Équation du 2nd degré", desc: "Résout ax² + bx + c = 0 (racines réelles ou complexes).",
    inputs: [["a", "a", "1", ""], ["b", "b", "-5", ""], ["c", "c", "6", ""]],
    formula: "Δ = b² − 4ac &nbsp; x = (−b ± √Δ)/2a",
    calc: function (v) {
      var D = v.b * v.b - 4 * v.a * v.c, f = window.fmtNum;
      if (D > 0) return [["Δ", D, ""], ["x₁", (-v.b - Math.sqrt(D)) / (2 * v.a), ""], ["x₂", (-v.b + Math.sqrt(D)) / (2 * v.a), ""]];
      if (D === 0) return [["Δ", 0, ""], ["Solution double x₀", -v.b / (2 * v.a), ""]];
      var re = -v.b / (2 * v.a), im = Math.sqrt(-D) / (2 * v.a);
      return [["Δ", D, ""], ["z₁", f(re) + " − " + f(Math.abs(im)) + "j", ""], ["z₂", f(re) + " + " + f(Math.abs(im)) + "j", ""]];
    } },
  { id: "base", emoji: "🔢", name: "Binaire / hexadécimal", desc: "Conversion d'un entier décimal (0 à 4 294 967 295).",
    inputs: [["n", "Nombre décimal", "200", ""]],
    formula: "Binaire : divisions successives par 2 &nbsp; Hexa : par 16",
    calc: function (v) {
      var n = Math.floor(v.n); if (!(n >= 0) || n > 4294967295) return "Entre 0 et 4 294 967 295.";
      var b = n.toString(2), bits = b.length;
      return [["Binaire", b.replace(/\B(?=(\d{4})+(?!\d))/g, " "), ""], ["Hexadécimal", n.toString(16).toUpperCase(), ""], ["Octal", n.toString(8), ""], ["Nombre de bits", bits, ""]];
    } }
];
