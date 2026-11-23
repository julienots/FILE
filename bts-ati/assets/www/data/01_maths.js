COURS.push({
  id: "maths", name: "Mathématiques", emoji: "📐", color: "#2563EB",
  desc: "Les outils mathématiques du technicien : calcul, fonctions, dérivées, intégrales, équations différentielles, complexes, suites, statistiques et probabilités. Chaque notion est expliquée avec des exemples pas à pas.",
  chapters: [
    {
      id: "calcul", t: "Calcul de base, puissances et équations", y: 1,
      html: `
<h3>Fractions</h3>
<div class="f">a/b + c/d = (ad + bc)/(bd) &nbsp;&nbsp; (a/b) × (c/d) = ac/(bd) &nbsp;&nbsp; (a/b) ÷ (c/d) = (a/b) × (d/c)</div>
<div class="tip box">Diviser par une fraction = multiplier par son inverse.</div>
<h3>Puissances</h3>
<div class="f">aⁿ × aᵐ = aⁿ⁺ᵐ &nbsp;&nbsp; aⁿ / aᵐ = aⁿ⁻ᵐ &nbsp;&nbsp; (aⁿ)ᵐ = aⁿᵐ &nbsp;&nbsp; a⁰ = 1 &nbsp;&nbsp; a⁻ⁿ = 1/aⁿ</div>
<h4>Préfixes utilisés en technique</h4>
<table><tr><th>Préfixe</th><th>Symbole</th><th>Valeur</th></tr>
<tr><td>giga</td><td>G</td><td>10⁹</td></tr><tr><td>méga</td><td>M</td><td>10⁶</td></tr><tr><td>kilo</td><td>k</td><td>10³</td></tr>
<tr><td>milli</td><td>m</td><td>10⁻³</td></tr><tr><td>micro</td><td>µ</td><td>10⁻⁶</td></tr><tr><td>nano</td><td>n</td><td>10⁻⁹</td></tr></table>
<div class="ex box">4,7 kΩ = 4,7 × 10³ Ω = 4 700 Ω. &nbsp; 220 µF = 220 × 10⁻⁶ F = 0,000 22 F.</div>
<h3>Identités remarquables</h3>
<div class="f">(a + b)² = a² + 2ab + b² &nbsp;&nbsp; (a − b)² = a² − 2ab + b² &nbsp;&nbsp; (a + b)(a − b) = a² − b²</div>
<h3>Équation du 1er degré</h3>
<p>On isole x : ce qu'on fait d'un côté, on le fait de l'autre.</p>
<div class="ex box">3x + 5 = 20 → 3x = 15 → x = 5.</div>
<h3>Équation du 2nd degré ax² + bx + c = 0</h3>
<div class="f">Δ = b² − 4ac</div>
<ul><li>Δ &gt; 0 : deux solutions x₁ = (−b − √Δ)/(2a) et x₂ = (−b + √Δ)/(2a)</li><li>Δ = 0 : une solution x₀ = −b/(2a)</li><li>Δ &lt; 0 : pas de solution réelle (mais deux solutions complexes !)</li></ul>
<div class="ex box">x² − 5x + 6 = 0 : Δ = 25 − 24 = 1 ; x₁ = (5 − 1)/2 = 2 ; x₂ = (5 + 1)/2 = 3.</div>
<h3>Systèmes de 2 équations</h3>
<p>Méthode par <b>substitution</b> (on exprime une inconnue et on remplace) ou par <b>combinaison</b> (on additionne les équations multipliées pour éliminer une inconnue).</p>
<div class="ex box">x + y = 10 et x − y = 2. En additionnant : 2x = 12 → x = 6, puis y = 4.</div>
<h3>Proportionnalité et pourcentages</h3>
<div class="f">Augmenter de t % : × (1 + t/100) &nbsp;&nbsp; Baisser de t % : × (1 − t/100)</div>
<div class="warn box">+20 % puis −20 % ne revient PAS au départ : 1,2 × 0,8 = 0,96 → −4 %.</div>`,
      cards: [["aⁿ × aᵐ = ?", "aⁿ⁺ᵐ"], ["a⁻ⁿ = ?", "1/aⁿ"], ["Formule du discriminant ?", "Δ = b² − 4ac"], ["Si Δ < 0 ?", "Pas de solution réelle (2 solutions complexes)."], ["1 µ = ?", "10⁻⁶"], ["(a+b)² = ?", "a² + 2ab + b²"], ["Augmenter de 15 % revient à multiplier par ?", "1,15"]],
      quiz: [
        ["10³ × 10⁻⁵ = ?", ["10⁻²", "10⁸", "10⁻¹⁵", "10²"], 0, "On additionne les exposants : 3 + (−5) = −2."],
        ["Le discriminant de x² + 2x + 5 vaut :", ["−16", "24", "16", "−4"], 0, "Δ = 2² − 4×1×5 = 4 − 20 = −16 : pas de solution réelle."],
        ["2x − 7 = 9 donne x = ", ["1", "8", "16", "−1"], 1, "2x = 16 → x = 8."],
        ["4,7 kΩ = ", ["470 Ω", "4 700 Ω", "47 000 Ω", "0,0047 Ω"], 1, "kilo = 10³ donc 4,7 × 1000 = 4 700 Ω."],
        ["Un prix augmente de 10 % puis baisse de 10 %. Au final :", ["Il est identique", "Il a baissé de 1 %", "Il a augmenté de 1 %", "Il a baissé de 10 %"], 1, "1,1 × 0,9 = 0,99 → −1 %."],
        ["(a − b)(a + b) = ", ["a² + b²", "a² − b²", "a² − 2ab + b²", "2a"], 1, "Identité remarquable n°3."]
      ]
    },
    {
      id: "fonctions", t: "Fonctions usuelles (affine, carré, inverse, exp, ln)", y: 1,
      html: `
<h3>C'est quoi une fonction ?</h3>
<div class="easy box">Une fonction est une « machine » : on met un nombre x à l'entrée, elle donne un seul nombre f(x) à la sortie. Ex : la tension aux bornes d'une résistance en fonction du courant : U(I) = R·I.</div>
<h3>Fonction affine f(x) = ax + b</h3>
<p>Sa courbe est une <b>droite</b>. a = coefficient directeur (pente), b = ordonnée à l'origine.</p>
<div class="f">a = (y₂ − y₁)/(x₂ − x₁)</div>
<p>a &gt; 0 : croissante ; a &lt; 0 : décroissante. Si b = 0, la fonction est <b>linéaire</b> (proportionnalité).</p>
<h3>Fonction carré f(x) = x² et polynômes</h3>
<p>Parabole en « U ». Pour ax² + bx + c, le sommet est en x = −b/(2a).</p>
<h3>Fonction inverse f(x) = 1/x</h3>
<p>Définie pour x ≠ 0. Hyperbole. Exemple : I = U/R, le courant diminue quand R augmente.</p>
<h3>Fonction exponentielle eˣ</h3>
<ul><li>e ≈ 2,718 ; e⁰ = 1 ; eˣ &gt; 0 toujours.</li><li>eᵃ⁺ᵇ = eᵃ × eᵇ ; e⁻ˣ = 1/eˣ.</li><li>Croissance ou décroissance « exponentielle » : charge d'un condensateur, refroidissement, décroissance radioactive.</li></ul>
<div class="f">u(t) = E(1 − e^(−t/τ)) <small>charge d'un condensateur : 63 % à t = τ, ≈ 99 % à t = 5τ</small></div>
<h3>Fonction logarithme népérien ln(x)</h3>
<div class="easy box">ln est la fonction « inverse » de l'exponentielle : ln(eˣ) = x et e^(ln x) = x. Elle sert à « faire descendre » un exposant pour trouver une inconnue.</div>
<div class="f">ln(ab) = ln a + ln b &nbsp;&nbsp; ln(a/b) = ln a − ln b &nbsp;&nbsp; ln(aⁿ) = n·ln a &nbsp;&nbsp; ln 1 = 0</div>
<div class="ex box">Résoudre e^(−t/2) = 0,1 : −t/2 = ln 0,1 → t = −2 ln 0,1 ≈ 4,6 s.</div>
<h3>Logarithme décimal et décibels</h3>
<div class="f">log(x) = ln(x)/ln(10) &nbsp;&nbsp; G(dB) = 20·log(Vs/Ve)</div>
<p>Utilisé en acoustique et en électronique (gain, filtres).</p>
<h3>Fonctions trigonométriques</h3>
<p>sin et cos sont périodiques de période 2π. Un signal alternatif s'écrit u(t) = Û·sin(ωt + φ) avec ω = 2πf.</p>`,
      cards: [["Pente d'une droite passant par (x₁,y₁) et (x₂,y₂) ?", "a = (y₂ − y₁)/(x₂ − x₁)"], ["ln(ab) = ?", "ln a + ln b"], ["ln(aⁿ) = ?", "n·ln a"], ["e⁰ = ? et ln 1 = ?", "1 et 0"], ["Pourcentage de charge d'un condensateur à t = τ ?", "≈ 63 %"], ["Gain en dB ?", "G = 20·log(Vs/Ve)"]],
      quiz: [
        ["La pente de la droite passant par (1 ; 3) et (3 ; 7) est :", ["2", "4", "0,5", "−2"], 0, "(7 − 3)/(3 − 1) = 4/2 = 2."],
        ["ln(e⁵) = ", ["5", "e", "1", "5e"], 0, "ln et exp sont réciproques."],
        ["ln(8) = ", ["3 ln 2", "8 ln 1", "ln 4 + ln 4", "2 ln 3"], 0, "8 = 2³ donc ln 8 = 3 ln 2."],
        ["Un condensateur est quasi chargé (≈ 99 %) au bout de :", ["τ", "2τ", "5τ", "10τ"], 2, "On retient la règle des 5τ."],
        ["Une fonction affine avec b = 0 est :", ["Constante", "Linéaire", "Exponentielle", "Carrée"], 1, "f(x) = ax : proportionnalité."],
        ["e^(−x) est égal à :", ["−eˣ", "1/eˣ", "ln x", "eˣ − 1"], 1, "Exposant négatif = inverse."]
      ]
    },
    {
      id: "derivees", t: "Dérivées et variations", y: 1,
      html: `
<h3>À quoi ça sert ?</h3>
<div class="easy box">La dérivée mesure la <b>vitesse de variation</b> d'une fonction. Si x(t) est la position, x'(t) est la vitesse. Si f'(x) &gt; 0 la fonction monte, si f'(x) &lt; 0 elle descend, si f'(x) = 0 on est sur un sommet ou un creux (maximum / minimum).</div>
<h3>Tableau des dérivées usuelles</h3>
<table><tr><th>f(x)</th><th>f'(x)</th></tr>
<tr><td>k (constante)</td><td>0</td></tr><tr><td>x</td><td>1</td></tr><tr><td>xⁿ</td><td>n·xⁿ⁻¹</td></tr>
<tr><td>1/x</td><td>−1/x²</td></tr><tr><td>√x</td><td>1/(2√x)</td></tr><tr><td>eˣ</td><td>eˣ</td></tr>
<tr><td>ln x</td><td>1/x</td></tr><tr><td>sin x</td><td>cos x</td></tr><tr><td>cos x</td><td>−sin x</td></tr>
<tr><td>e^(ax+b)</td><td>a·e^(ax+b)</td></tr><tr><td>sin(ωt+φ)</td><td>ω·cos(ωt+φ)</td></tr></table>
<h3>Règles de calcul</h3>
<div class="f">(u + v)' = u' + v' &nbsp;&nbsp; (k·u)' = k·u' &nbsp;&nbsp; (uv)' = u'v + uv' &nbsp;&nbsp; (u/v)' = (u'v − uv')/v²</div>
<div class="f">(eᵘ)' = u'·eᵘ &nbsp;&nbsp; (ln u)' = u'/u &nbsp;&nbsp; (uⁿ)' = n·u'·uⁿ⁻¹</div>
<h3>Méthode : étudier les variations</h3>
<ol><li>Calculer f'(x).</li><li>Chercher où f'(x) = 0.</li><li>Faire le tableau de signes de f'(x).</li><li>En déduire le tableau de variations (↗ si f' &gt; 0, ↘ si f' &lt; 0).</li><li>Calculer les valeurs des extremums.</li></ol>
<div class="ex box">f(x) = x² − 4x + 1. f'(x) = 2x − 4 = 0 pour x = 2. Avant 2, f' &lt; 0 (décroît) ; après 2, f' &gt; 0 (croît). Minimum : f(2) = 4 − 8 + 1 = −3.</div>
<h3>Tangente</h3>
<div class="f">y = f'(a)(x − a) + f(a)</div>
<h3>Application technique : optimisation</h3>
<div class="ex box">On veut une boîte de volume maximal, un coût minimal, une puissance maximale… On écrit la grandeur comme une fonction, on dérive, on cherche f' = 0. Ex : la puissance transmise à une charge est maximale quand R charge = R interne.</div>`,
      cards: [["Dérivée de xⁿ ?", "n·xⁿ⁻¹"], ["Dérivée de ln x ?", "1/x"], ["Dérivée de e^(ax) ?", "a·e^(ax)"], ["(uv)' = ?", "u'v + uv'"], ["(u/v)' = ?", "(u'v − uv')/v²"], ["Équation de la tangente en a ?", "y = f'(a)(x − a) + f(a)"], ["f'(x) > 0 sur un intervalle signifie ?", "f est croissante sur cet intervalle."]],
      quiz: [
        ["La dérivée de 3x⁴ est :", ["12x³", "3x³", "12x⁴", "4x³"], 0, "3 × 4x³ = 12x³."],
        ["La dérivée de e^(−2t) est :", ["e^(−2t)", "−2e^(−2t)", "2e^(−2t)", "−2t·e^(−2t)"], 1, "(e^u)' = u'·e^u avec u' = −2."],
        ["f(x) = x² − 6x : le minimum est atteint pour x =", ["6", "3", "−3", "0"], 1, "f'(x) = 2x − 6 = 0 → x = 3."],
        ["La dérivée de ln(2x+1) est :", ["1/(2x+1)", "2/(2x+1)", "2x+1", "ln 2"], 1, "(ln u)' = u'/u avec u' = 2."],
        ["Si f'(a) = 0 et f' change de signe en a :", ["f a un extremum en a", "f est nulle en a", "f n'est pas définie", "f est constante"], 0, "Changement de signe de la dérivée = maximum ou minimum."],
        ["La dérivée de sin(100πt) est :", ["cos(100πt)", "100π·cos(100πt)", "−100π·sin(100πt)", "100π·sin(100πt)"], 1, "(sin(ωt))' = ω·cos(ωt)."]
      ]
    },
    {
      id: "integrales", t: "Primitives et intégrales", y: 1,
      html: `
<h3>Primitive : l'inverse de la dérivée</h3>
<div class="easy box">F est une primitive de f si F' = f. Exemple : la position est une primitive de la vitesse. On « remonte » la dérivée. Il y a toujours une constante C (car la dérivée d'une constante est 0).</div>
<table><tr><th>f(x)</th><th>F(x) (primitive)</th></tr>
<tr><td>k</td><td>kx</td></tr><tr><td>xⁿ (n ≠ −1)</td><td>xⁿ⁺¹/(n+1)</td></tr><tr><td>1/x</td><td>ln x</td></tr>
<tr><td>eˣ</td><td>eˣ</td></tr><tr><td>e^(ax)</td><td>e^(ax)/a</td></tr><tr><td>cos x</td><td>sin x</td></tr><tr><td>sin x</td><td>−cos x</td></tr>
<tr><td>u'·eᵘ</td><td>eᵘ</td></tr><tr><td>u'/u</td><td>ln|u|</td></tr></table>
<h3>Intégrale = aire sous la courbe</h3>
<div class="f">∫ₐᵇ f(x) dx = F(b) − F(a)</div>
<div class="ex box">∫₀² 3x² dx = [x³]₀² = 8 − 0 = 8.</div>
<h3>Propriétés</h3>
<ul><li>Linéarité : ∫(f + g) = ∫f + ∫g et ∫k·f = k·∫f.</li><li>Chasles : ∫ₐᵇ + ∫ᵦᶜ = ∫ₐᶜ.</li><li>Si f ≥ 0 sur [a ; b], l'intégrale est positive (c'est une aire).</li></ul>
<h3>Valeur moyenne et valeur efficace</h3>
<div class="f">Valeur moyenne : ⟨f⟩ = 1/(b − a) · ∫ₐᵇ f(t) dt</div>
<div class="f">Valeur efficace : F_eff = √( 1/T · ∫₀ᵀ f²(t) dt )</div>
<div class="key box">Pour un signal sinusoïdal : valeur moyenne = 0 et valeur efficace = Û/√2. C'est pour ça que 230 V efficaces correspondent à 325 V crête.</div>
<h3>Applications techniques</h3>
<ul><li>Énergie : W = ∫ P(t) dt.</li><li>Déplacement : x = ∫ v(t) dt.</li><li>Charge électrique : Q = ∫ i(t) dt.</li><li>Centre de gravité, moment quadratique (RDM).</li></ul>
<h3>Intégration par parties (2ᵉ année)</h3>
<div class="f">∫ₐᵇ u·v' = [u·v]ₐᵇ − ∫ₐᵇ u'·v</div>
<div class="ex box">∫₀¹ x·eˣ dx : u = x, v' = eˣ → [x·eˣ]₀¹ − ∫₀¹ eˣ dx = e − (e − 1) = 1.</div>`,
      cards: [["Primitive de xⁿ ?", "xⁿ⁺¹/(n+1)"], ["Primitive de 1/x ?", "ln x"], ["Primitive de e^(ax) ?", "e^(ax)/a"], ["∫ₐᵇ f(x)dx = ?", "F(b) − F(a)"], ["Valeur moyenne sur [a;b] ?", "1/(b−a) × ∫ₐᵇ f"], ["Valeur efficace d'une sinusoïde d'amplitude Û ?", "Û/√2"]],
      quiz: [
        ["Une primitive de 4x³ est :", ["x⁴", "12x²", "4x⁴", "x³"], 0, "4 × x⁴/4 = x⁴."],
        ["∫₀¹ 2x dx = ", ["1", "2", "0,5", "0"], 0, "[x²]₀¹ = 1."],
        ["Une primitive de e^(3x) est :", ["3e^(3x)", "e^(3x)/3", "e^(3x)", "3x·e^(3x)"], 1, "On divise par le coefficient a = 3."],
        ["La valeur moyenne d'un signal sinusoïdal sur une période est :", ["Û", "Û/√2", "0", "2Û/π"], 2, "Les alternances positive et négative se compensent."],
        ["L'énergie se calcule à partir de la puissance par :", ["Dérivation", "Intégration", "Multiplication par 2", "Racine carrée"], 1, "W = ∫P dt (si P constante : W = P × t)."]
      ]
    },
    {
      id: "complexes", t: "Nombres complexes", y: 1,
      html: `
<h3>Pourquoi des complexes ?</h3>
<div class="easy box">En électricité alternative, une tension a une <b>amplitude</b> ET un <b>déphasage</b>. Les nombres complexes permettent de manipuler les deux en même temps avec des calculs simples. C'est l'outil n°1 pour les circuits RLC.</div>
<h3>Forme algébrique z = a + jb</h3>
<p>En électricité on note <b>j</b> (et pas i, réservé au courant) avec j² = −1. a = partie réelle, b = partie imaginaire.</p>
<h3>Module et argument</h3>
<div class="f">|z| = √(a² + b²) &nbsp;&nbsp; tan θ = b/a</div>
<div class="f">Forme trigonométrique : z = |z|(cos θ + j sin θ) &nbsp;&nbsp; Forme exponentielle : z = |z|·e^(jθ)</div>
<div class="warn box">Pour l'argument, regarde dans quel quart du plan est le point : si a &lt; 0, il faut ajouter π à arctan(b/a).</div>
<h3>Opérations</h3>
<ul>
<li><b>Addition</b> : forme algébrique (on additionne parties réelles et imaginaires).</li>
<li><b>Multiplication</b> : forme exponentielle → on multiplie les modules et on additionne les arguments.</li>
<li><b>Division</b> : on divise les modules et on soustrait les arguments.</li>
<li><b>Conjugué</b> : z̄ = a − jb ; z·z̄ = a² + b².</li></ul>
<div class="ex box">z₁ = 1 + j : |z₁| = √2, θ = π/4. z₂ = 2e^(jπ/6). z₁·z₂ = 2√2·e^(j(π/4 + π/6)) = 2√2·e^(j5π/12).</div>
<h3>Application : impédances</h3>
<table><tr><th>Composant</th><th>Impédance complexe</th><th>Déphasage</th></tr>
<tr><td>Résistance R</td><td>Z = R</td><td>0</td></tr>
<tr><td>Bobine L</td><td>Z = jLω</td><td>+90° (u en avance)</td></tr>
<tr><td>Condensateur C</td><td>Z = 1/(jCω) = −j/(Cω)</td><td>−90° (u en retard)</td></tr></table>
<div class="ex box">R = 30 Ω en série avec L tel que Lω = 40 Ω : Z = 30 + 40j ; |Z| = 50 Ω ; φ = arctan(40/30) ≈ 53°. Sous 230 V : I = 230/50 = 4,6 A.</div>
<h3>Équation du 2nd degré avec Δ &lt; 0</h3>
<div class="f">z = (−b ± j√(−Δ))/(2a)</div>`,
      cards: [["j² = ?", "−1"], ["Module de a + jb ?", "√(a² + b²)"], ["Forme exponentielle ?", "z = |z|·e^(jθ)"], ["Impédance d'une bobine ?", "jLω"], ["Impédance d'un condensateur ?", "1/(jCω)"], ["Multiplier deux complexes en forme exponentielle ?", "On multiplie les modules, on additionne les arguments."]],
      quiz: [
        ["Le module de 3 + 4j est :", ["5", "7", "25", "1"], 0, "√(9 + 16) = 5."],
        ["(1 + j)(1 − j) = ", ["0", "2", "−2", "2j"], 1, "1 − j² = 1 + 1 = 2."],
        ["L'impédance d'un condensateur :", ["jCω", "1/(jCω)", "R", "Lω"], 1, "Z_C = 1/(jCω) = −j/(Cω)."],
        ["L'argument de j est :", ["0", "π/2", "π", "−π/2"], 1, "j est sur l'axe imaginaire positif."],
        ["Z = 6 + 8j Ω. |Z| = ", ["14 Ω", "10 Ω", "48 Ω", "2 Ω"], 1, "√(36 + 64) = 10 Ω."]
      ]
    },
    {
      id: "suites", t: "Suites numériques", y: 1,
      html: `
<h3>Suite arithmétique : on ajoute toujours la même chose</h3>
<div class="f">uₙ₊₁ = uₙ + r &nbsp;&nbsp; uₙ = u₀ + n·r &nbsp;&nbsp; Somme = (nb de termes) × (1er + dernier)/2</div>
<div class="ex box">Une machine produit 100 pièces le 1er jour et 5 de plus chaque jour : u₀ = 100, r = 5. Au jour 10 : u₁₀ = 150.</div>
<h3>Suite géométrique : on multiplie toujours par la même chose</h3>
<div class="f">uₙ₊₁ = q·uₙ &nbsp;&nbsp; uₙ = u₀·qⁿ &nbsp;&nbsp; Somme = u₀ × (1 − qⁿ⁺¹)/(1 − q)</div>
<div class="ex box">Une machine perd 15 % de sa valeur par an (amortissement dégressif) : q = 0,85. Achetée 20 000 €, après 3 ans : 20 000 × 0,85³ ≈ 12 283 €.</div>
<h3>Limites</h3>
<ul><li>Si 0 &lt; q &lt; 1 : qⁿ → 0 (la suite diminue vers 0).</li><li>Si q &gt; 1 : qⁿ → +∞.</li></ul>
<h3>Trouver un seuil</h3>
<p>« À partir de quand la valeur passe sous 5 000 € ? » → on résout u₀·qⁿ &lt; 5000 avec ln, ou on teste avec la calculatrice / un algorithme.</p>
<pre>u = 20000 ; n = 0
tant que u >= 5000 :
    u = u * 0.85
    n = n + 1
afficher n</pre>`,
      cards: [["Terme général d'une suite arithmétique ?", "uₙ = u₀ + n·r"], ["Terme général d'une suite géométrique ?", "uₙ = u₀·qⁿ"], ["Baisse de 15 % par an : raison q ?", "q = 0,85"]],
      quiz: [
        ["u₀ = 3, r = 4 (arithmétique). u₅ = ", ["23", "20", "7", "3 × 4⁵"], 0, "3 + 5 × 4 = 23."],
        ["u₀ = 1000, q = 1,02. u₂ =", ["1040", "1040,4", "1020", "2040"], 1, "1000 × 1,02² = 1040,4."],
        ["Si 0 < q < 1, qⁿ tend vers :", ["1", "+∞", "0", "q"], 2, "Multiplier sans cesse par un nombre < 1 fait tendre vers 0."]
      ]
    },
    {
      id: "stats", t: "Statistiques", y: 1,
      html: `
<h3>Vocabulaire</h3>
<p><b>Population</b> (ensemble étudié), <b>individu</b>, <b>caractère</b> (ce qu'on mesure : diamètre d'une pièce…), <b>effectif</b> (nombre), <b>fréquence</b> = effectif / effectif total.</p>
<h3>Indicateurs de position</h3>
<ul><li><b>Moyenne</b> x̄ = (Σ nᵢxᵢ)/N</li><li><b>Médiane</b> : partage la série en deux moitiés.</li><li><b>Quartiles</b> Q1 (25 %), Q3 (75 %).</li><li><b>Mode</b> : valeur la plus fréquente.</li></ul>
<h3>Indicateurs de dispersion</h3>
<div class="f">Variance V = (Σ nᵢ(xᵢ − x̄)²)/N &nbsp;&nbsp; Écart type σ = √V</div>
<p>Étendue = max − min. Écart interquartile = Q3 − Q1.</p>
<div class="easy box">L'écart type mesure si les valeurs sont <b>serrées</b> autour de la moyenne (σ petit = production régulière) ou <b>dispersées</b> (σ grand).</div>
<h3>Statistiques à deux variables : régression linéaire</h3>
<p>Nuage de points (x ; y). La droite d'ajustement par les moindres carrés y = ax + b se calcule à la calculatrice. Le coefficient de corrélation r (entre −1 et 1) : si |r| proche de 1, l'ajustement affine est bon.</p>
<div class="ex box">Température d'un moteur en fonction du temps de fonctionnement : on trace le nuage, on trouve y = 0,8x + 20, r = 0,98 → on peut prévoir la température à 60 min : 68 °C.</div>
<h3>Lien avec la qualité</h3>
<p>En contrôle qualité, on vérifie que la moyenne est proche de la cote nominale et que σ est assez petit pour que les pièces restent dans les tolérances (voir Capabilité, chapitre Qualité).</p>`,
      cards: [["Écart type ?", "σ = √(variance) : mesure la dispersion autour de la moyenne."], ["Médiane ?", "Valeur qui partage la série en deux moitiés d'effectif égal."], ["Coefficient de corrélation proche de 1 signifie ?", "Le nuage est bien aligné : bon ajustement affine."]],
      quiz: [
        ["Série : 2 ; 4 ; 6 ; 8. Moyenne =", ["4", "5", "6", "20"], 1, "(2+4+6+8)/4 = 5."],
        ["Un écart type faible signifie :", ["Valeurs dispersées", "Valeurs regroupées autour de la moyenne", "Moyenne faible", "Erreur de mesure"], 1, "σ petit = production régulière."],
        ["Médiane de 3 ; 7 ; 9 ; 12 ; 20 :", ["9", "10,2", "7", "12"], 0, "Valeur du milieu (3ᵉ sur 5)."]
      ]
    },
    {
      id: "probas", t: "Probabilités et lois (binomiale, normale)", y: 2,
      html: `
<h3>Bases</h3>
<div class="f">P(A) = cas favorables / cas possibles &nbsp;&nbsp; P(Ā) = 1 − P(A) &nbsp;&nbsp; P(A∪B) = P(A) + P(B) − P(A∩B)</div>
<p>Probabilité conditionnelle : P_B(A) = P(A∩B)/P(B). Événements indépendants : P(A∩B) = P(A)·P(B). Utilise un <b>arbre pondéré</b> !</p>
<h3>Variable aléatoire, espérance</h3>
<div class="f">E(X) = Σ xᵢ·pᵢ &nbsp;&nbsp; V(X) = Σ pᵢ(xᵢ − E(X))² &nbsp;&nbsp; σ = √V</div>
<div class="easy box">L'espérance, c'est la moyenne qu'on obtiendrait si on répétait l'expérience un très grand nombre de fois (ex : gain moyen, coût moyen de panne).</div>
<h3>Loi binomiale B(n ; p)</h3>
<p>On répète n fois, de façon indépendante, une épreuve à 2 issues (succès de probabilité p). X = nombre de succès.</p>
<div class="f">P(X = k) = C(n,k)·pᵏ·(1 − p)ⁿ⁻ᵏ &nbsp;&nbsp; E(X) = np &nbsp;&nbsp; σ = √(np(1 − p))</div>
<div class="ex box">2 % de pièces défectueuses, lot de 50 pièces. Probabilité de 0 défaut : 0,98⁵⁰ ≈ 0,364. Nombre moyen de défauts : 50 × 0,02 = 1.</div>
<h3>Loi de Poisson P(λ)</h3>
<p>Pour des événements rares (pannes, arrivées…) : P(X = k) = e^(−λ)·λᵏ/k!, E(X) = λ.</p>
<h3>Loi normale N(μ ; σ)</h3>
<p>Courbe en cloche, symétrique autour de μ. Très utilisée pour les cotes de pièces fabriquées.</p>
<div class="key box">P(μ − σ ≤ X ≤ μ + σ) ≈ 68 % &nbsp;·&nbsp; P(μ − 2σ ≤ X ≤ μ + 2σ) ≈ 95 % &nbsp;·&nbsp; P(μ − 3σ ≤ X ≤ μ + 3σ) ≈ 99,7 %</div>
<p>Centrer-réduire : T = (X − μ)/σ suit N(0 ; 1).</p>
<h3>Loi exponentielle (fiabilité)</h3>
<div class="f">P(T ≤ t) = 1 − e^(−λt) &nbsp;&nbsp; Fiabilité R(t) = e^(−λt) &nbsp;&nbsp; MTBF = 1/λ</div>
<h3>Intervalle de confiance et tests (notions)</h3>
<p>À partir d'un échantillon, on estime une moyenne ou une proportion avec un intervalle de confiance à 95 % : f ± 1,96·√(f(1−f)/n) pour une proportion. Un test d'hypothèse permet de décider si un lot est conforme.</p>`,
      cards: [["P(Ā) = ?", "1 − P(A)"], ["Espérance d'une loi binomiale ?", "E(X) = n·p"], ["Loi normale : % entre μ−2σ et μ+2σ ?", "≈ 95 %"], ["Fiabilité avec taux de défaillance constant λ ?", "R(t) = e^(−λt), MTBF = 1/λ"], ["Conditions pour une loi binomiale ?", "n répétitions indépendantes d'une épreuve à 2 issues de même probabilité p."]],
      quiz: [
        ["On lance un dé. P(obtenir 6) =", ["1/6", "1/2", "6", "5/6"], 0, "1 cas favorable sur 6."],
        ["X suit B(100 ; 0,05). E(X) =", ["5", "0,05", "95", "100"], 0, "np = 100 × 0,05 = 5."],
        ["Loi normale : environ quel % des valeurs est dans μ ± 3σ ?", ["68 %", "95 %", "99,7 %", "50 %"], 2, "C'est la règle des 3 sigma."],
        ["λ = 0,001 panne/h. MTBF =", ["1 000 h", "0,001 h", "100 h", "10 000 h"], 0, "MTBF = 1/λ."],
        ["A et B indépendants, P(A)=0,5 et P(B)=0,2. P(A∩B) =", ["0,7", "0,1", "0,3", "0,25"], 1, "0,5 × 0,2 = 0,1."]
      ]
    },
    {
      id: "equadiff", t: "Équations différentielles", y: 2,
      html: `
<h3>C'est quoi ?</h3>
<div class="easy box">Une équation différentielle relie une fonction à ses dérivées. Elle décrit comment un système <b>évolue dans le temps</b> : charge d'un condensateur, vitesse d'un moteur qui démarre, température d'un four, amortisseur…</div>
<h3>1er ordre : ay' + by = 0</h3>
<div class="f">Solutions : y(t) = K·e^(−(b/a)t)</div>
<h3>1er ordre avec second membre constant : ay' + by = c</h3>
<div class="f">y(t) = K·e^(−(b/a)t) + c/b</div>
<p>On trouve K avec la <b>condition initiale</b> y(0).</p>
<div class="ex box">Circuit RC : RC·u' + u = E, u(0) = 0. Solution : u(t) = E(1 − e^(−t/RC)). On appelle τ = RC la constante de temps.</div>
<h3>Méthode générale (second membre quelconque)</h3>
<ol><li>Résoudre l'équation sans second membre → y₀.</li><li>Trouver une solution particulière yₚ (souvent de la même forme que le second membre).</li><li>Solution générale : y = y₀ + yₚ.</li><li>Utiliser la condition initiale pour trouver la constante.</li></ol>
<h3>2nd ordre : ay'' + by' + cy = 0</h3>
<p>Équation caractéristique : ar² + br + c = 0 ; on calcule Δ.</p>
<table><tr><th>Cas</th><th>Solutions</th><th>Comportement</th></tr>
<tr><td>Δ &gt; 0 (r₁, r₂)</td><td>y = A·e^(r₁t) + B·e^(r₂t)</td><td>Apériodique (pas d'oscillation)</td></tr>
<tr><td>Δ = 0 (r)</td><td>y = (At + B)·e^(rt)</td><td>Critique (le plus rapide sans dépasser)</td></tr>
<tr><td>Δ &lt; 0 (r = α ± jβ)</td><td>y = e^(αt)(A cos βt + B sin βt)</td><td>Oscillations amorties</td></tr></table>
<div class="ex box">Système masse-ressort-amortisseur, circuit RLC, régulation de position : tous suivent ce modèle.</div>
<h3>Transformée de Laplace (pour aller plus loin)</h3>
<p>Elle transforme une équation différentielle en équation algébrique (on remplace la dérivée par p). Très utilisée en automatique (fonctions de transfert). L(y') = pY(p) − y(0) ; L(1) = 1/p ; L(e^(−at)) = 1/(p + a).</p>`,
      cards: [["Solution de y' + ay = 0 ?", "y = K·e^(−at)"], ["Solution de RC·u' + u = E avec u(0)=0 ?", "u = E(1 − e^(−t/RC))"], ["2nd ordre, Δ < 0 : comportement ?", "Oscillations amorties."], ["Méthode générale ?", "Solution sans 2nd membre + solution particulière, puis condition initiale."]],
      quiz: [
        ["Les solutions de y' + 3y = 0 sont :", ["K·e^(3t)", "K·e^(−3t)", "3t + K", "K·t³"], 1, "y = K·e^(−(b/a)t) avec b/a = 3."],
        ["Solution particulière constante de 2y' + y = 10 :", ["10", "5", "20", "2"], 0, "Constante : y' = 0 donc y = 10."],
        ["Équation caractéristique de y'' + 4y = 0 :", ["r² + 4 = 0", "r + 4 = 0", "r² + 4r = 0", "4r² = 0"], 0, "On remplace y'' par r² et y par 1."],
        ["Un 2nd ordre avec Δ < 0 a une réponse :", ["Apériodique", "Oscillante amortie", "Constante", "Linéaire"], 1, "Racines complexes → sinus et cosinus amortis."]
      ]
    },
    {
      id: "fourier", t: "Séries de Fourier et signaux", y: 2,
      html: `
<h3>L'idée géniale de Fourier</h3>
<div class="easy box">Tout signal <b>périodique</b> (carré, triangle, dents de scie…) peut s'écrire comme une somme de sinusoïdes : une composante continue + le <b>fondamental</b> (même fréquence que le signal) + des <b>harmoniques</b> (fréquences multiples 2f, 3f, 4f…).</div>
<div class="f">f(t) = a₀ + Σ [aₙ cos(nωt) + bₙ sin(nωt)]</div>
<div class="f">a₀ = valeur moyenne &nbsp;&nbsp; aₙ = (2/T)∫ f(t)cos(nωt)dt &nbsp;&nbsp; bₙ = (2/T)∫ f(t)sin(nωt)dt</div>
<h3>Raccourcis de symétrie</h3>
<ul><li>Fonction <b>paire</b> (symétrique / axe vertical) : tous les bₙ = 0.</li><li>Fonction <b>impaire</b> (symétrique / origine) : a₀ = 0 et tous les aₙ = 0.</li></ul>
<h3>Spectre</h3>
<p>Le spectre est le graphique des amplitudes en fonction de la fréquence. Un signal carré contient seulement les harmoniques impairs (f, 3f, 5f…) avec des amplitudes en 1/n.</p>
<h3>Formule de Parseval</h3>
<div class="f">F_eff² = a₀² + ½ Σ (aₙ² + bₙ²)</div>
<h3>Applications</h3>
<ul><li>Harmoniques sur le réseau électrique (variateurs, alimentations à découpage) → échauffements, perturbations.</li><li>Analyse vibratoire en maintenance : un défaut de roulement crée une raie à une fréquence précise.</li><li>Taux de distorsion harmonique (THD).</li></ul>`,
      cards: [["Que dit le théorème de Fourier ?", "Un signal périodique = composante continue + somme de sinusoïdes (fondamental + harmoniques)."], ["Fonction paire : quels coefficients sont nuls ?", "Les bₙ."], ["Harmoniques d'un signal carré ?", "Seulement les impairs, amplitude en 1/n."]],
      quiz: [
        ["Le fondamental d'un signal de 50 Hz a pour fréquence :", ["25 Hz", "50 Hz", "100 Hz", "150 Hz"], 1, "Même fréquence que le signal."],
        ["L'harmonique de rang 3 d'un signal 50 Hz est à :", ["53 Hz", "150 Hz", "16,7 Hz", "350 Hz"], 1, "3 × 50 = 150 Hz."],
        ["Pour une fonction impaire :", ["bₙ = 0", "aₙ = 0 et a₀ = 0", "Tous nuls", "Rien de particulier"], 1, "Il ne reste que des sinus."]
      ]
    },
    {
      id: "vecteurs", t: "Vecteurs, trigonométrie et géométrie", y: 1,
      html: `
<h3>Trigonométrie dans le triangle rectangle</h3>
<div class="f">sin = opposé/hypoténuse &nbsp;&nbsp; cos = adjacent/hypoténuse &nbsp;&nbsp; tan = opposé/adjacent</div>
<div class="tip box">Moyen mnémotechnique : <b>SOH CAH TOA</b>.</div>
<p>Pythagore : hypoténuse² = a² + b². Radians : π rad = 180°.</p>
<table><tr><th>Angle</th><th>0</th><th>30°</th><th>45°</th><th>60°</th><th>90°</th></tr>
<tr><td>sin</td><td>0</td><td>1/2</td><td>√2/2</td><td>√3/2</td><td>1</td></tr>
<tr><td>cos</td><td>1</td><td>√3/2</td><td>√2/2</td><td>1/2</td><td>0</td></tr></table>
<h3>Vecteurs</h3>
<p>Un vecteur a une <b>direction</b>, un <b>sens</b> et une <b>norme</b> (longueur). En mécanique, forces et vitesses sont des vecteurs.</p>
<div class="f">‖u‖ = √(x² + y² + z²)</div>
<h4>Produit scalaire</h4>
<div class="f">u·v = xx' + yy' + zz' = ‖u‖·‖v‖·cos θ</div>
<p>Si u·v = 0 → vecteurs perpendiculaires. Application : travail d'une force W = F·d·cos θ.</p>
<h4>Produit vectoriel</h4>
<p>u ∧ v est perpendiculaire à u et v, de norme ‖u‖·‖v‖·sin θ. Application : moment d'une force M = r ∧ F.</p>
<h3>Projection d'une force</h3>
<div class="ex box">Une force de 100 N inclinée de 30° par rapport à l'horizontale : Fx = 100·cos 30° ≈ 86,6 N ; Fy = 100·sin 30° = 50 N.</div>
<h3>Matrices (notions)</h3>
<p>Tableaux de nombres utiles pour résoudre des systèmes, faire des changements de repère (robotique) ou des calculs de graphes. Produit matrice × vecteur : ligne × colonne.</p>`,
      cards: [["SOH CAH TOA ?", "Sin = Opp/Hyp, Cos = Adj/Hyp, Tan = Opp/Adj"], ["Produit scalaire nul ?", "Vecteurs perpendiculaires (orthogonaux)."], ["cos 60° = ?", "1/2"], ["180° = combien de radians ?", "π rad"]],
      quiz: [
        ["sin 30° =", ["1/2", "√3/2", "1", "√2/2"], 0, "Valeur à connaître par cœur."],
        ["Hypoténuse d'un triangle rectangle de côtés 3 et 4 :", ["5", "7", "25", "12"], 0, "√(9+16) = 5."],
        ["Composante horizontale d'une force F inclinée de α :", ["F sin α", "F cos α", "F tan α", "F/α"], 1, "Adjacent = hypoténuse × cos."],
        ["u(1;2) et v(2;−1) sont :", ["Colinéaires", "Perpendiculaires", "Égaux", "Opposés"], 1, "1×2 + 2×(−1) = 0."]
      ]
    }
  ]
});
