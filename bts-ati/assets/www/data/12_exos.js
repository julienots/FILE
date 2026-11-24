// Exercices corrigés, rattachés aux chapitres par la clé "matiere/chapitre".
// { t: titre, lvl: 1..3, q: énoncé (HTML), s: correction (HTML) }
var EXOS = {
  "maths/calcul": [
    { t: "Équation du 2nd degré", lvl: 1, q: "Résoudre 2x² − 3x − 2 = 0.",
      s: "<ol><li>a = 2, b = −3, c = −2.</li><li>Δ = b² − 4ac = 9 + 16 = 25 &gt; 0 → deux solutions, √Δ = 5.</li><li>x₁ = (3 − 5)/4 = <b>−0,5</b> ; x₂ = (3 + 5)/4 = <b>2</b>.</li><li>Vérification : 2×4 − 6 − 2 = 0 ✓.</li></ol>" },
    { t: "Conversions", lvl: 1, q: "Convertir 0,047 µF en nF puis en F. Convertir 2,2 MΩ en Ω.",
      s: "0,047 µF = 0,047 × 1000 nF = <b>47 nF</b> = 47 × 10⁻⁹ F = <b>4,7 × 10⁻⁸ F</b>.<br>2,2 MΩ = 2,2 × 10⁶ Ω = <b>2 200 000 Ω</b>." },
    { t: "Système", lvl: 2, q: "Un lot contient des vis à 0,10 € et des écrous à 0,05 €. Il y a 300 pièces pour 22 €. Combien de vis ?",
      s: "<ol><li>v + e = 300 et 0,10v + 0,05e = 22.</li><li>e = 300 − v → 0,10v + 15 − 0,05v = 22 → 0,05v = 7.</li><li>v = <b>140 vis</b>, e = 160 écrous.</li></ol>" }
  ],
  "maths/fonctions": [
    { t: "Charge d'un condensateur", lvl: 2, q: "u(t) = 12(1 − e^(−t/2)) (t en s, u en V). a) Calculer u(4). b) Au bout de combien de temps u = 9 V ?",
      s: "a) u(4) = 12(1 − e⁻²) = 12 × (1 − 0,1353) ≈ <b>10,4 V</b>.<br>b) 12(1 − e^(−t/2)) = 9 → e^(−t/2) = 0,25 → −t/2 = ln 0,25 → t = −2 ln 0,25 ≈ <b>2,77 s</b>." },
    { t: "Droite d'étalonnage", lvl: 1, q: "Un capteur donne 1 V à 0 °C et 5 V à 100 °C (relation affine). Donner T en fonction de U, puis T pour U = 3,4 V.",
      s: "Pente : (100 − 0)/(5 − 1) = 25 °C/V. T = 25(U − 1). Pour U = 3,4 V : T = 25 × 2,4 = <b>60 °C</b>." }
  ],
  "maths/derivees": [
    { t: "Étude de variations", lvl: 2, q: "f(x) = x³ − 3x² + 2 sur [−1 ; 3]. Étudier les variations et donner les extremums.",
      s: "<ol><li>f'(x) = 3x² − 6x = 3x(x − 2).</li><li>f'(x) = 0 pour x = 0 et x = 2.</li><li>f' &gt; 0 sur [−1 ; 0[, &lt; 0 sur ]0 ; 2[, &gt; 0 sur ]2 ; 3].</li><li>f croissante, puis décroissante, puis croissante.</li><li>Maximum local f(0) = <b>2</b> ; minimum local f(2) = 8 − 12 + 2 = <b>−2</b>. Aux bornes : f(−1) = −2, f(3) = 2.</li></ol>" },
    { t: "Optimisation d'une boîte", lvl: 3, q: "Dans une tôle carrée de 30 cm de côté, on découpe un carré de côté x à chaque coin puis on plie pour faire une boîte sans couvercle. Quelle valeur de x donne le volume maximal ?",
      s: "<ol><li>V(x) = x(30 − 2x)² avec 0 &lt; x &lt; 15.</li><li>V'(x) = (30 − 2x)² − 4x(30 − 2x) = (30 − 2x)(30 − 6x).</li><li>V'(x) = 0 pour x = 5 (x = 15 est exclu).</li><li>V' &gt; 0 avant 5 et &lt; 0 après : maximum en <b>x = 5 cm</b>.</li><li>V(5) = 5 × 20² = <b>2 000 cm³</b> (2 L).</li></ol>" }
  ],
  "maths/integrales": [
    { t: "Valeur moyenne", lvl: 2, q: "Calculer ∫₀^π sin t dt, puis la valeur moyenne de sin t sur [0 ; π].",
      s: "∫₀^π sin t dt = [−cos t]₀^π = −cos π + cos 0 = 1 + 1 = <b>2</b>.<br>Valeur moyenne = 2/π ≈ <b>0,637</b>. C'est aussi la valeur moyenne d'une tension redressée double alternance : U_moy = 2Û/π." },
    { t: "Énergie consommée", lvl: 2, q: "La puissance d'un moteur vaut P(t) = 500t (W) pendant le démarrage de 0 à 4 s, puis 2 000 W pendant 10 s. Énergie totale ?",
      s: "<ol><li>Démarrage : ∫₀⁴ 500t dt = [250t²]₀⁴ = 4 000 J.</li><li>Régime : 2 000 × 10 = 20 000 J.</li><li>Total : <b>24 000 J</b> (≈ 6,7 Wh).</li></ol>" }
  ],
  "maths/complexes": [
    { t: "Circuit R-L", lvl: 2, q: "R = 40 Ω en série avec L = 0,1 H, sous 230 V / 50 Hz. Calculer Z, φ et I.",
      s: "<ol><li>Lω = 0,1 × 2π × 50 ≈ 31,4 Ω → Z = 40 + 31,4j.</li><li>|Z| = √(40² + 31,4²) = √2 586 ≈ <b>50,9 Ω</b>.</li><li>φ = arctan(31,4/40) ≈ <b>38°</b> (courant en retard).</li><li>I = 230/50,9 ≈ <b>4,52 A</b>.</li></ol>" },
    { t: "Opérations", lvl: 1, q: "z₁ = 3 + 4j et z₂ = 1 − j. Calculer z₁ + z₂, z₁·z₂ et z₁/z₂.",
      s: "z₁ + z₂ = <b>4 + 3j</b>.<br>z₁z₂ = 3 − 3j + 4j − 4j² = 3 + j + 4 = <b>7 + j</b>.<br>z₁/z₂ = (3 + 4j)(1 + j)/((1 − j)(1 + j)) = (3 + 3j + 4j − 4)/2 = <b>−0,5 + 3,5j</b>." }
  ],
  "maths/suites": [
    { t: "Dépréciation d'une machine", lvl: 2, q: "Une machine achetée 25 000 € perd 20 % de sa valeur par an. a) Valeur après 4 ans ? b) À partir de quelle année vaut-elle moins de 5 000 € ?",
      s: "a) Suite géométrique de raison 0,8 : 25 000 × 0,8⁴ = 25 000 × 0,4096 = <b>10 240 €</b>.<br>b) 25 000 × 0,8ⁿ &lt; 5 000 ⇔ 0,8ⁿ &lt; 0,2 ⇔ n &gt; ln 0,2 / ln 0,8 ≈ 7,2 (on change le sens car ln 0,8 &lt; 0) → <b>n = 8 ans</b>." }
  ],
  "maths/stats": [
    { t: "Contrôle de diamètres", lvl: 2, q: "5 pièces mesurées : 9,98 ; 10,01 ; 10,00 ; 10,02 ; 9,99 mm. Calculer la moyenne et l'écart type.",
      s: "<ol><li>Moyenne = 50,00 / 5 = <b>10,00 mm</b>.</li><li>Écarts : −0,02 ; 0,01 ; 0 ; 0,02 ; −0,01. Carrés : 4, 1, 0, 4, 1 (× 10⁻⁴) → somme 10 × 10⁻⁴.</li><li>Variance = 10 × 10⁻⁴ / 5 = 2 × 10⁻⁴.</li><li>σ = √(2 × 10⁻⁴) ≈ <b>0,014 mm</b>.</li></ol>" }
  ],
  "maths/probas": [
    { t: "Lot de pièces (binomiale)", lvl: 2, q: "5 % des pièces sont défectueuses. On prélève 20 pièces (tirage assimilé à des tirages avec remise). Probabilité qu'aucune ne soit défectueuse ? Qu'au moins une le soit ? Espérance ?",
      s: "X suit B(20 ; 0,05).<br>P(X = 0) = 0,95²⁰ ≈ <b>0,358</b>.<br>P(X ≥ 1) = 1 − 0,358 = <b>0,642</b>.<br>E(X) = 20 × 0,05 = <b>1 pièce</b> en moyenne." },
    { t: "Loi normale", lvl: 2, q: "Le diamètre suit N(50 ; 0,1) mm. Tolérance 50 ± 0,2 mm. Pourcentage de pièces conformes ?",
      s: "± 0,2 = ± 2σ. Environ <b>95 %</b> des pièces sont dans μ ± 2σ, donc conformes (≈ 5 % de rebut)." },
    { t: "Fiabilité", lvl: 2, q: "Un composant a un taux de défaillance constant λ = 2 × 10⁻⁴ /h. MTBF ? Probabilité de fonctionner 1 000 h sans panne ?",
      s: "MTBF = 1/λ = <b>5 000 h</b>.<br>R(1000) = e^(−2×10⁻⁴ × 1000) = e^(−0,2) ≈ <b>0,82</b> (82 %)." }
  ],
  "maths/equadiff": [
    { t: "Circuit RC", lvl: 2, q: "R = 10 kΩ, C = 100 µF, E = 12 V, condensateur déchargé à t = 0. Donner u(t), u(1 s) et le temps de charge pratique.",
      s: "<ol><li>τ = RC = 10⁴ × 10⁻⁴ = 1 s.</li><li>u(t) = 12(1 − e^(−t)).</li><li>u(1) = 12(1 − e⁻¹) ≈ <b>7,6 V</b> (63 %).</li><li>Charge pratiquement complète à 5τ = <b>5 s</b>.</li></ol>" },
    { t: "Refroidissement", lvl: 3, q: "Une pièce à 80 °C refroidit dans un atelier à 20 °C selon θ' = −0,1(θ − 20) (t en min). Exprimer θ(t) et la température après 10 min.",
      s: "<ol><li>θ' + 0,1θ = 2 : solution générale θ = K·e^(−0,1t) + 20.</li><li>θ(0) = 80 → K = 60 → θ(t) = 20 + 60e^(−0,1t).</li><li>θ(10) = 20 + 60e⁻¹ ≈ <b>42 °C</b>.</li></ol>" }
  ],
  "maths/vecteurs": [
    { t: "Projection d'une force", lvl: 1, q: "Un câble tire avec 500 N à 40° au-dessus de l'horizontale. Composantes ?",
      s: "Fx = 500 cos 40° ≈ <b>383 N</b> ; Fy = 500 sin 40° ≈ <b>321 N</b>. Vérification : √(383² + 321²) ≈ 500 ✓." }
  ],
  "maths/matrices": [
    { t: "Système par matrice", lvl: 2, q: "Résoudre avec les matrices : 3x + 2y = 12 et x + 4y = 14.",
      s: "A = [3 2 ; 1 4], det = 12 − 2 = 10. A⁻¹ = 1/10 [4 −2 ; −1 3].<br>X = A⁻¹ × [12 ; 14] = 1/10 [48 − 28 ; −12 + 42] = [2 ; 3] → <b>x = 2, y = 3</b>." }
  ],

  "physique/cinematique": [
    { t: "Convoyeur", lvl: 1, q: "Un convoyeur passe de 0 à 0,5 m/s en 0,25 s (accélération constante). Accélération et distance parcourue ?",
      s: "a = 0,5/0,25 = <b>2 m/s²</b>. d = ½ × 2 × 0,25² = <b>0,0625 m</b> (6,25 cm)." },
    { t: "Profil trapézoïdal", lvl: 2, q: "Un axe accélère pendant 0,5 s jusqu'à 1 m/s, roule 2 s à vitesse constante, puis freine en 0,5 s. Distance totale ?",
      s: "Aire du trapèze : accélération ½ × 0,5 × 1 = 0,25 m ; palier 2 × 1 = 2 m ; freinage 0,25 m. Total = <b>2,5 m</b>." }
  ],
  "physique/dynamique": [
    { t: "Chariot", lvl: 2, q: "Un chariot de 500 kg doit accélérer à 0,8 m/s². La résistance au roulement vaut 2 % du poids. Effort de traction ?",
      s: "<ol><li>Résistance : 0,02 × 500 × 9,81 ≈ 98 N.</li><li>PFD : F − 98 = 500 × 0,8 = 400.</li><li>F ≈ <b>498 N</b>.</li></ol>" }
  ],
  "physique/energie": [
    { t: "Monte-charge", lvl: 2, q: "Un monte-charge lève 300 kg de 12 m en 20 s. Rendement de l'ensemble : 0,75. Puissance utile et puissance absorbée ?",
      s: "<ol><li>Énergie : Ep = mgh = 300 × 9,81 × 12 ≈ 35 300 J.</li><li>P utile = 35 300 / 20 ≈ <b>1 766 W</b>.</li><li>P absorbée = 1 766 / 0,75 ≈ <b>2 355 W</b>.</li></ol>" }
  ],
  "physique/electricite": [
    { t: "Chute de tension", lvl: 2, q: "Une ligne de 30 m (aller) en cuivre 2,5 mm² alimente un appareil de 16 A sous 230 V. Chute de tension ? Est-elle acceptable (max 5 %) ?",
      s: "<ol><li>Longueur du conducteur : 2 × 30 = 60 m.</li><li>R = ρL/S = 1,7 × 10⁻⁸ × 60 / 2,5 × 10⁻⁶ ≈ 0,41 Ω.</li><li>ΔU = 0,41 × 16 ≈ <b>6,5 V</b> soit 6,5/230 ≈ <b>2,8 %</b>.</li><li>2,8 % &lt; 5 % → <b>acceptable</b>.</li></ol>" },
    { t: "Diviseur de tension", lvl: 1, q: "E = 24 V, R₁ = 10 kΩ, R₂ = 4,7 kΩ en série. Tension aux bornes de R₂ ? Courant ?",
      s: "U₂ = 24 × 4,7/14,7 ≈ <b>7,67 V</b>. I = 24 / 14 700 ≈ <b>1,63 mA</b>." }
  ],
  "physique/alternatif": [
    { t: "Relèvement du facteur de puissance", lvl: 3, q: "Atelier monophasé 230 V : P = 20 kW, cos φ = 0,75. a) Courant, S et Q. b) Puissance réactive des condensateurs pour cos φ' = 0,95. c) Nouveau courant.",
      s: "a) I = P/(U cos φ) = 20 000/(230 × 0,75) ≈ <b>116 A</b> ; S = 20/0,75 ≈ <b>26,7 kVA</b> ; Q = √(26,7² − 20²) ≈ <b>17,6 kvar</b>.<br>b) tan φ = 0,882 ; tan φ' = 0,329 → Qc = 20 000 × (0,882 − 0,329) ≈ <b>11,1 kvar</b>.<br>c) I' = 20 000/(230 × 0,95) ≈ <b>91,5 A</b> : −21 % de courant, câbles moins chargés." }
  ],
  "physique/thermique": [
    { t: "Chauffe-eau", lvl: 2, q: "Chauffer 150 L d'eau de 15 à 55 °C avec une résistance de 3 kW (rendement 0,95). Énergie, durée, coût à 0,25 €/kWh ?",
      s: "<ol><li>Q = mcΔθ = 150 × 4 180 × 40 ≈ 25,1 MJ = 25,1 × 10⁶ / 3,6 × 10⁶ ≈ 6,97 kWh.</li><li>Énergie électrique : 6,97 / 0,95 ≈ <b>7,33 kWh</b>.</li><li>Durée : 7,33 / 3 ≈ <b>2,44 h</b> (≈ 2 h 27).</li><li>Coût : 7,33 × 0,25 ≈ <b>1,83 €</b>.</li></ol>" }
  ],
  "physique/fluides": [
    { t: "Pompe", lvl: 2, q: "Une pompe débite 18 m³/h d'eau pour la monter de 25 m (pertes négligées), rendement 0,65. Puissance absorbée ? Vitesse dans un tuyau Ø40 mm ?",
      s: "<ol><li>Qv = 18/3 600 = 0,005 m³/s.</li><li>Δp = ρgh = 1 000 × 9,81 × 25 ≈ 245 kPa.</li><li>P hydraulique = 0,005 × 245 000 ≈ 1 226 W ; P absorbée = 1 226/0,65 ≈ <b>1,89 kW</b>.</li><li>S = π × 0,02² ≈ 1,257 × 10⁻³ m² → v = 0,005/1,257 × 10⁻³ ≈ <b>4 m/s</b>.</li></ol>" }
  ],
  "physique/magnetisme": [
    { t: "Force sur un conducteur", lvl: 1, q: "Un conducteur de 15 cm, parcouru par 8 A, est placé perpendiculairement à un champ de 0,6 T. Force ?",
      s: "F = BIL = 0,6 × 8 × 0,15 = <b>0,72 N</b>." }
  ],

  "meca/statique": [
    { t: "Levier", lvl: 1, q: "Un levier pivote en O. Une charge de 2 000 N est à 0,2 m de O ; on pousse de l'autre côté à 1 m de O (forces verticales vers le bas). Effort nécessaire et réaction du pivot ?",
      s: "ΣM/O = 0 : F × 1 = 2 000 × 0,2 → F = <b>400 N</b>.<br>ΣFy = 0 : R = 2 000 + 400 = <b>2 400 N</b> vers le haut." },
    { t: "Poutre sur deux appuis", lvl: 2, q: "Poutre de 6 m sur appuis A (x = 0) et B (x = 6 m). Charges : 3 000 N à x = 2 m, 1 500 N à x = 5 m. Réactions ?",
      s: "<ol><li>ΣM/A = 0 : 6B − 3 000 × 2 − 1 500 × 5 = 0 → B = 13 500/6 = <b>2 250 N</b>.</li><li>ΣFy = 0 : A = 4 500 − 2 250 = <b>2 250 N</b>.</li><li>Vérification avec ΣM/B : 6A = 3 000 × 4 + 1 500 × 1 = 13 500 ✓.</li></ol>" }
  ],
  "meca/rdm1": [
    { t: "Dimensionner un tirant", lvl: 2, q: "Tirant en acier S355, effort 50 kN, coefficient de sécurité 3, longueur 2 m (E = 210 000 MPa). Diamètre minimal ? Allongement avec le diamètre choisi ?",
      s: "<ol><li>Rpe = 355/3 ≈ 118 MPa.</li><li>S ≥ 50 000/118 ≈ 423 mm² → d ≥ √(4 × 423/π) ≈ 23,2 mm → on choisit <b>Ø25</b>.</li><li>S = 491 mm² → σ = 50 000/491 ≈ 102 MPa.</li><li>ΔL = σL/E = 102 × 2 000/210 000 ≈ <b>0,97 mm</b>.</li></ol>" },
    { t: "Goupille", lvl: 2, q: "Une goupille Ø6 travaille en double cisaillement. Rpg = 100 MPa. Effort maximal transmissible ?",
      s: "Section cisaillée : 2 × π × 3² ≈ 56,5 mm². T max = 56,5 × 100 ≈ <b>5 650 N</b>." }
  ],
  "meca/rdm2": [
    { t: "Poutre en flexion", lvl: 3, q: "Poutre rectangulaire 60 × 120 mm (h = 120, verticale), sur 2 appuis distants de 3 m, charge 10 kN au milieu. E = 210 000 MPa. Contrainte max et flèche ?",
      s: "<ol><li>Mf max = FL/4 = 10 000 × 3 000/4 = 7,5 × 10⁶ N·mm.</li><li>I/v = bh²/6 = 60 × 120²/6 = 144 000 mm³ → σ = 7,5 × 10⁶/144 000 ≈ <b>52 MPa</b>.</li><li>I = bh³/12 = 8,64 × 10⁶ mm⁴.</li><li>f = FL³/(48EI) = 10 000 × 2,7 × 10¹⁰ / (48 × 210 000 × 8,64 × 10⁶) ≈ <b>3,1 mm</b>.</li></ol>" },
    { t: "Arbre en torsion", lvl: 2, q: "Un arbre transmet 11 kW à 1 450 tr/min. τ admissible = 50 MPa. Diamètre minimal ?",
      s: "<ol><li>ω = 1 450 × 2π/60 ≈ 151,8 rad/s → Mt = 11 000/151,8 ≈ 72,4 N·m = 72 400 N·mm.</li><li>τ = 16Mt/(πd³) ≤ 50 → d³ ≥ 16 × 72 400/(π × 50) ≈ 7 375 mm³.</li><li>d ≥ 19,5 mm → <b>Ø20 mm</b>.</li></ol>" }
  ],
  "meca/transmissions": [
    { t: "Réducteur à deux étages", lvl: 2, q: "Moteur 1 450 tr/min, 20 N·m. Étage 1 : 18/54 dents. Étage 2 : 20/60 dents. Rendement 0,97 par étage. Vitesse et couple de sortie ?",
      s: "<ol><li>r = (18 × 20)/(54 × 60) = 360/3 240 = 1/9.</li><li>N sortie = 1 450/9 ≈ <b>161 tr/min</b>.</li><li>C sortie = 20 × 9 × 0,97² ≈ <b>169 N·m</b>.</li></ol>" },
    { t: "Choisir un motoréducteur", lvl: 3, q: "Un tapis doit avancer à 0,5 m/s avec un tambour Ø200 mm et un effort de traction de 800 N. Rendement du motoréducteur : 0,85. Moteur 4 pôles à 1 450 tr/min. Puissance moteur et rapport ?",
      s: "<ol><li>P utile = F·v = 800 × 0,5 = 400 W.</li><li>P moteur = 400/0,85 ≈ 471 W → moteur normalisé <b>0,55 kW</b>.</li><li>ω tambour = v/R = 0,5/0,1 = 5 rad/s → N = 5 × 60/2π ≈ 47,7 tr/min.</li><li>Rapport : 1 450/47,7 ≈ <b>30,4</b> (r ≈ 1/30). Couple au tambour : 800 × 0,1 = 80 N·m.</li></ol>" }
  ],
  "meca/fabrication": [
    { t: "Conditions de coupe", lvl: 2, q: "Fraisage d'aluminium, fraise Ø50, Vc = 200 m/min, avance 0,2 mm/tr. Fréquence de rotation, vitesse d'avance, temps pour 100 mm ?",
      s: "N = 1 000 × 200/(π × 50) ≈ <b>1 273 tr/min</b>. Vf = 0,2 × 1 273 ≈ <b>255 mm/min</b>. Temps = 100/255 ≈ 0,39 min ≈ <b>24 s</b>." }
  ],
  "meca/liaisons": [
    { t: "Identifier des liaisons", lvl: 1, q: "Donner la liaison entre : a) une roue de vélo et le cadre ; b) un piston et son cylindre ; c) le mors mobile d'un étau et le corps ; d) la vis de l'étau et le corps.",
      s: "a) <b>Pivot</b> (1 rotation). b) <b>Pivot glissant</b> (rotation + translation selon l'axe ; souvent considérée ainsi). c) <b>Glissière</b>. d) <b>Hélicoïdale</b> avec l'écrou (ou pivot si la vis est arrêtée en translation dans le corps)." }
  ],

  "elec/triphase": [
    { t: "Four triphasé", lvl: 2, q: "Trois résistances de 40 Ω sur réseau 230/400 V. Calculer le courant en ligne et la puissance en étoile puis en triangle.",
      s: "<b>Étoile</b> : V = 230 V par résistance → I = 230/40 = <b>5,75 A</b> ; P = 3 × 230²/40 ≈ <b>3,97 kW</b>.<br><b>Triangle</b> : 400 V par résistance → J = 10 A, I = 10√3 ≈ <b>17,3 A</b> ; P = 3 × 400²/40 = <b>12 kW</b>.<br>Le triangle donne 3 fois plus de puissance." }
  ],
  "elec/moteur_asy": [
    { t: "Exploiter une plaque signalétique", lvl: 3, q: "Plaque : 400/690 V – 15/8,7 A – 7,5 kW – 1 455 tr/min – cos φ 0,84 – 50 Hz. Réseau 230/400 V. Donner : couplage, nombre de pôles, glissement, puissance absorbée, rendement, couple utile, courant de démarrage direct (≈ 7 In) et en étoile-triangle.",
      s: "<ol><li>Chaque enroulement supporte 400 V → <b>triangle</b> sur réseau 400 V (I = 15 A).</li><li>n = 1 455 → ns = 1 500 tr/min → p = 2 → <b>4 pôles</b>.</li><li>g = (1 500 − 1 455)/1 500 = <b>3 %</b>.</li><li>Pa = √3 × 400 × 15 × 0,84 ≈ <b>8,73 kW</b>.</li><li>η = 7,5/8,73 ≈ <b>86 %</b>.</li><li>C = 7 500/(1 455 × 2π/60) ≈ <b>49 N·m</b>.</li><li>Id ≈ 7 × 15 = <b>105 A</b> ; en Y-Δ : 105/3 = <b>35 A</b>.</li></ol>" }
  ],
  "elec/transfo": [
    { t: "Transformateur de commande", lvl: 1, q: "Transformateur 400/24 V – 160 VA. Rapport de transformation, courants nominaux primaire et secondaire ?",
      s: "m = 24/400 = <b>0,06</b>. I₂ = 160/24 ≈ <b>6,7 A</b>. I₁ ≈ 160/400 = <b>0,4 A</b>." }
  ],
  "elec/autres_moteurs": [
    { t: "Moteur pas à pas + vis", lvl: 2, q: "Moteur 200 pas/tour entraînant une vis de pas 5 mm. Nombre de pas pour avancer de 50 mm ? Résolution ?",
      s: "50 mm = 10 tours → <b>2 000 pas</b>. Résolution = 5/200 = <b>0,025 mm/pas</b>." }
  ],
  "elec/variateurs": [
    { t: "Économie avec un variateur", lvl: 3, q: "Une pompe de 15 kW tourne 6 000 h/an. Un variateur permet de la faire tourner à 80 % de sa vitesse. Économie annuelle à 0,15 €/kWh (lois de similitude P ∝ n³) ?",
      s: "<ol><li>P à 80 % : 15 × 0,8³ = 15 × 0,512 ≈ 7,7 kW.</li><li>Gain : (15 − 7,7) × 6 000 ≈ 43 900 kWh/an.</li><li>Économie ≈ 43 900 × 0,15 ≈ <b>6 600 €/an</b>.</li></ol>" },
    { t: "Hacheur", lvl: 1, q: "Un hacheur alimenté en 48 V commande un moteur CC. Quel rapport cyclique pour obtenir 28,8 V en moyenne ?",
      s: "α = 28,8/48 = <b>0,6</b> (60 %)." }
  ],
  "elec/protection": [
    { t: "Défaut en régime TT", lvl: 3, q: "Régime TT, résistance de prise de terre des masses Ra = 20 Ω, du neutre Rn = 10 Ω. Défaut franc phase-masse sous 230 V. Courant de défaut, tension de contact ? Sensibilité maximale du différentiel (UL = 50 V) ?",
      s: "<ol><li>Id = 230/(20 + 10) ≈ <b>7,7 A</b> : trop faible pour faire déclencher un disjoncteur de 16 A rapidement.</li><li>Uc = Ra × Id = 20 × 7,7 ≈ <b>153 V</b> &gt; 50 V → dangereux !</li><li>Il faut un différentiel tel que IΔn ≤ UL/Ra = 50/20 = <b>2,5 A</b> (on utilisera par exemple 300 mA, et 30 mA pour les prises).</li></ol>" }
  ],
  "elec/energie_renouv": [
    { t: "Coût des fuites d'air", lvl: 2, q: "Un compresseur de 37 kW tourne 5 000 h/an. Les fuites représentent 25 % de sa production. Combien coûtent-elles à 0,15 €/kWh ?",
      s: "Énergie : 37 × 5 000 = 185 000 kWh/an. Fuites : 25 % → 46 250 kWh, soit <b>≈ 6 940 €/an</b>." }
  ],

  "auto/logique": [
    { t: "Simplification", lvl: 2, q: "Simplifier S = a·b + a·b̄ + ā·b.",
      s: "a·b + a·b̄ = a(b + b̄) = a. Donc S = a + ā·b = <b>a + b</b> (règle a + ā·b = a + b). C'est un simple OU." },
    { t: "Commande bimanuelle", lvl: 1, q: "Une presse ne descend que si les deux boutons b1 et b2 sont appuyés, que le carter est fermé (c = 1) et qu'il n'y a pas d'arrêt d'urgence (au = 1 quand l'AU est enfoncé). Équation de la sortie ?",
      s: "S = <b>b1 · b2 · c · NON(au)</b>. En câblage : contacts en série, l'AU étant un contact NF." }
  ],
  "auto/grafcet": [
    { t: "Malaxeur", lvl: 2, q: "Décrire le GRAFCET (point de vue partie opérative) : au départ cycle (dcy) si la cuve est vide (b), on ouvre la vanne de remplissage V1 jusqu'au niveau haut (h), on malaxe (moteur M) pendant 10 s, puis on ouvre la vanne de vidange V2 jusqu'à cuve vide (b), et on revient à l'attente.",
      s: "<ul><li>Étape <b>0</b> (initiale) : aucune action. Transition : <b>dcy · b</b>.</li><li>Étape <b>1</b> : V1. Transition : <b>h</b>.</li><li>Étape <b>2</b> : M. Transition : <b>10 s/X2</b>.</li><li>Étape <b>3</b> : V2. Transition : <b>b</b> → retour à l'étape 0.</li></ul>" }
  ],
  "auto/api": [
    { t: "Capteur de niveau 4–20 mA", lvl: 1, q: "Un capteur de niveau 0–5 m délivre 4–20 mA. On lit 15,2 mA. Niveau ? Et si on lit 2 mA ?",
      s: "Niveau = (15,2 − 4)/16 × 5 = 11,2/16 × 5 = <b>3,5 m</b>. 2 mA &lt; 4 mA : <b>défaut</b> (câble coupé ou capteur HS), l'automate doit déclencher une alarme." }
  ],
  "auto/pneumatique": [
    { t: "Vérin et consommation d'air", lvl: 3, q: "Vérin double effet Ø40, tige Ø16, course 100 mm, 6 bar, 10 cycles/min. Efforts théoriques et consommation d'air libre ?",
      s: "<ol><li>F sortie = 0,6 × π × 40²/4 ≈ <b>754 N</b> ; F rentrée = 0,6 × π(40² − 16²)/4 ≈ <b>633 N</b>.</li><li>Volumes : sortie 12,57 cm² × 10 cm = 125,7 cm³ ; rentrée 10,56 × 10 = 105,6 cm³ ; total 231 cm³.</li><li>Air libre : × (6 + 1) ≈ 1,62 L par cycle → <b>16,2 L/min</b>.</li></ol>" }
  ],
  "auto/hydraulique": [
    { t: "Presse hydraulique", lvl: 3, q: "Une presse doit fournir 20 t (≈ 196 kN) sous 200 bar, avec une vitesse de travail de 50 mm/s. Diamètre de vérin (gamme : 100, 125, 160 mm), débit et puissance ?",
      s: "<ol><li>S ≥ 196 200/20 = 9 810 mm² → D ≥ 111,8 mm → <b>Ø125</b>.</li><li>S = 12 272 mm² = 122,7 cm² ; Q = 122,7 × 5 cm/s ≈ 614 cm³/s ≈ <b>36,8 L/min</b>.</li><li>P = 200 × 36,8/600 ≈ <b>12,3 kW</b> (hors rendement).</li></ol>" }
  ],
  "auto/asservissement": [
    { t: "Réponse d'un 1er ordre", lvl: 2, q: "H(p) = 3/(1 + 4p). On applique un échelon d'amplitude 2. Valeur finale, valeur à t = 4 s, temps de réponse à 5 % ?",
      s: "Valeur finale = K × E = 3 × 2 = <b>6</b>. À t = τ = 4 s : 63 % → <b>3,8</b>. t5% = 3τ = <b>12 s</b>." }
  ],
  "auto/capteurs": [
    { t: "Choisir un détecteur", lvl: 1, q: "Choisir un détecteur pour : a) présence de bouteilles en verre ; b) position d'une came en acier ; c) niveau de granulés plastiques à travers une paroi ; d) piston de vérin.",
      s: "a) <b>Photoélectrique</b> (barrage de préférence ; attention au verre transparent → cellule spéciale ou ultrasons). b) <b>Inductif</b>. c) <b>Capacitif</b>. d) <b>Magnétique (ILS)</b>." }
  ],
  "auto/robotique": [
    { t: "Temps de cycle", lvl: 1, q: "Un robot fait une prise-dépose en 2,4 s. La demande est de 1 200 pièces/heure. Un robot suffit-il ?",
      s: "Capacité : 3 600/2,4 = <b>1 500 pièces/h</b> ≥ 1 200 → <b>oui</b>, avec 20 % de marge." }
  ],

  "conception/tolerances": [
    { t: "Ajustement Ø30 H7/g6", lvl: 2, q: "Ø30 H7 : 0 / +0,021 mm. Ø30 g6 : −0,007 / −0,020 mm. Jeu max, jeu min, type d'ajustement ?",
      s: "Trou : 30,000 à 30,021. Arbre : 29,980 à 29,993.<br>Jeu max = 30,021 − 29,980 = <b>0,041 mm</b>. Jeu min = 30,000 − 29,993 = <b>0,007 mm</b>.<br>Jeu min &gt; 0 → <b>ajustement avec jeu</b> (glissant)." }
  ],
  "conception/besoin": [
    { t: "Bête à cornes", lvl: 1, q: "Faire la bête à cornes d'un convoyeur à bande dans une usine d'emballage.",
      s: "À qui rend-il service ? <b>À l'entreprise / aux opérateurs de production</b>. Sur quoi agit-il ? <b>Sur les colis</b>. Dans quel but ? <b>Déplacer les colis d'un poste à un autre</b>." }
  ],

  "projet/planning": [
    { t: "Chemin critique", lvl: 3, q: "Tâches : A (3 j) ; B (4 j, après A) ; C (2 j, après A) ; D (5 j, après B) ; E (3 j, après B et C) ; F (2 j, après D et E). Durée du projet, chemin critique, marges de C et E ?",
      s: "<ol><li>Au plus tôt : A 0→3 ; B 3→7 ; C 3→5 ; D 7→12 ; E max(7, 5) = 7→10 ; F max(12, 10) = 12→14. <b>Durée : 14 jours</b>.</li><li>Au plus tard (depuis la fin) : F 12→14 ; D 7→12 ; E 9→12 ; B fin au plus tard min(7, 9) = 7 → 3→7 ; C fin au plus tard 9 → 7→9 ; A 0→3.</li><li>Marges : C = 7 − 3 = <b>4 j</b> ; E = 9 − 7 = <b>2 j</b>.</li><li>Chemin critique : <b>A – B – D – F</b>.</li></ol>" }
  ],
  "projet/couts": [
    { t: "Retour sur investissement d'un robot", lvl: 2, q: "Un robot coûte 120 000 € (installation comprise). Il remplace deux postes coûtant chacun 35 000 €/an, et sa maintenance coûte 10 000 €/an. Temps de retour ?",
      s: "Gain net annuel = 2 × 35 000 − 10 000 = 60 000 €. ROI = 120 000/60 000 = <b>2 ans</b>." },
    { t: "Coût de revient", lvl: 2, q: "Une pièce : matière 8 €, 0,4 h de main-d'œuvre à 45 €/h, 0,2 h machine à 60 €/h, frais indirects 15 % des coûts directs. Coût de revient et prix de vente HT avec 20 % de marge sur le coût ?",
      s: "Directs : 8 + 18 + 12 = 38 €. Indirects : 5,70 €. Coût de revient = <b>43,70 €</b>. PV HT = 43,70 × 1,2 ≈ <b>52,44 €</b>." }
  ],
  "projet/qualite": [
    { t: "Capabilité", lvl: 3, q: "Cote 20 ± 0,05 mm. Mesures : moyenne 20,01 mm, σ = 0,01 mm. Calculer Cp et Cpk. Conclusion ?",
      s: "<ol><li>IT = 0,10 → Cp = 0,10/(6 × 0,01) ≈ <b>1,67</b>.</li><li>Cpk = min(20,05 − 20,01 ; 20,01 − 19,95)/(3 × 0,01) = 0,04/0,03 ≈ <b>1,33</b>.</li><li>Procédé <b>capable</b> (≥ 1,33) mais <b>décentré</b> (Cpk &lt; Cp) : il faut recentrer le réglage sur 20,00.</li></ol>" },
    { t: "Pareto", lvl: 1, q: "Arrêts sur une ligne (en minutes/mois) : bourrage 420, changement de série 300, panne moteur 60, manque matière 120, réglage capteur 100. Sur quoi agir en priorité ?",
      s: "Total : 1 000 min. Tri : bourrage 42 %, changement de série 30 % (cumul 72 %), manque matière 12 % (84 %)… Les deux premières causes font <b>72 %</b> des arrêts : agir sur les <b>bourrages</b> puis le <b>changement de série</b> (SMED)." }
  ],
  "projet/amdec": [
    { t: "TRS", lvl: 3, q: "Poste ouvert 480 min. Pannes 45 min, réglages 15 min. Temps de cycle 1 min. 380 pièces produites dont 10 mauvaises. Calculer disponibilité, performance, qualité et TRS.",
      s: "<ol><li>Temps de fonctionnement : 480 − 60 = 420 min → D = 420/480 = <b>87,5 %</b>.</li><li>Performance : 380 pièces en 420 min possibles → P = 380/420 ≈ <b>90,5 %</b>.</li><li>Qualité : 370/380 ≈ <b>97,4 %</b>.</li><li>TRS = 0,875 × 0,905 × 0,974 ≈ <b>77 %</b> (= 370 bonnes pièces × 1 min / 480 min).</li></ol>" },
    { t: "Criticité AMDEC", lvl: 1, q: "Une électrovanne : F = 3, G = 3, D = 4. Seuil d'action : 24. Faut-il agir ? Proposer une action.",
      s: "C = 3 × 3 × 4 = <b>36 &gt; 24</b> → action nécessaire. Exemple : ajouter un capteur de position du vérin pour <b>détecter</b> la défaillance (D passe à 1 → C = 9) ou faire de la maintenance préventive." }
  ],
  "projet/lean": [
    { t: "Takt time", lvl: 1, q: "Deux équipes de 7 h (pauses déduites), demande de 840 pièces/jour. Takt time ? Le poste goulot a un temps de cycle de 32 s : est-ce suffisant ?",
      s: "Temps disponible : 14 h = 50 400 s. Takt = 50 400/840 = <b>60 s</b>. 32 s &lt; 60 s → <b>oui</b>, le goulot suit la demande." }
  ],

  "info/reseaux": [
    { t: "Adresses IP", lvl: 2, q: "Réseau 192.168.10.0/24, passerelle 192.168.10.1. Quelles adresses peut-on donner à un automate : a) 192.168.10.25 ; b) 192.168.11.25 ; c) 192.168.10.255 ; d) 192.168.10.300 ?",
      s: "a) <b>Oui</b>. b) Non : autre réseau (11). c) Non : adresse de diffusion. d) Non : un octet ne dépasse pas 255." }
  ],
  "info/algo": [
    { t: "Surveillance de température", lvl: 2, q: "Écrire une fonction Python qui reçoit une liste de températures et renvoie la moyenne et le nombre de valeurs au-dessus de 80 °C.",
      s: "<pre>def analyse(temps):\n    moyenne = sum(temps) / len(temps)\n    alarmes = 0\n    for t in temps:\n        if t > 80:\n            alarmes = alarmes + 1\n    return moyenne, alarmes\n\nprint(analyse([75, 82, 79, 90]))   # (81.5, 2)</pre>" }
  ]
};
