COURS.push({
  id: "elec", name: "Électrotechnique", emoji: "⚡", color: "#CA8A04",
  desc: "Réseau triphasé, transformateur, moteurs (asynchrone, courant continu, pas à pas, brushless), variateurs, schémas électriques, protection des personnes et des biens, habilitation électrique.",
  chapters: [
    {
      id: "triphase", t: "Le réseau triphasé", y: 1,
      html: `
<h3>Pourquoi le triphasé ?</h3>
<div class="easy box">3 tensions sinusoïdales identiques, décalées de 120° (1/3 de tour). Avantages : on transporte plus de puissance avec moins de cuivre, et ça crée naturellement un <b>champ tournant</b> pour faire tourner les moteurs.</div>
<h3>Tensions simples et composées</h3>
<ul><li><b>Tension simple V</b> : entre une phase et le neutre (230 V).</li><li><b>Tension composée U</b> : entre deux phases (400 V).</li></ul>
<div class="f">U = V·√3 &nbsp;&nbsp; (230 × 1,732 ≈ 400 V)</div>
<h3>Couplages des récepteurs</h3>
<table><tr><th></th><th>Étoile (Y)</th><th>Triangle (Δ)</th></tr>
<tr><td>Tension aux bornes d'un enroulement</td><td>V = U/√3</td><td>U</td></tr>
<tr><td>Courant dans un enroulement</td><td>J = I (courant en ligne)</td><td>J = I/√3</td></tr></table>
<div class="key box">Plaque moteur « 230/400 V » sur réseau 400 V → couplage <b>ÉTOILE</b>. Plaque « 400/690 V » sur réseau 400 V → couplage <b>TRIANGLE</b>. Règle : la plus petite tension de la plaque doit être appliquée à chaque enroulement.</div>
<h3>Puissances en triphasé équilibré</h3>
<div class="f">P = √3·U·I·cos φ &nbsp;&nbsp; Q = √3·U·I·sin φ &nbsp;&nbsp; S = √3·U·I</div>
<div class="ex box">Moteur 400 V, I = 10 A, cos φ = 0,85 : P_absorbée = 1,732 × 400 × 10 × 0,85 ≈ 5,9 kW.</div>
<h3>Mesure de puissance</h3><p>Méthode du wattmètre unique (charge équilibrée avec neutre : P = 3 × P_lue) ou méthode des deux wattmètres : P = P₁ + P₂ ; Q = √3(P₁ − P₂).</p>
<h3>Ordre des phases</h3><p>Inverser deux phases inverse le sens du champ tournant → le moteur tourne dans l'autre sens.</p>`,
      cards: [["Relation U et V ?", "U = V√3"], ["Puissance active triphasée ?", "P = √3·U·I·cos φ"], ["Moteur 230/400 V sur réseau 400 V ?", "Couplage étoile."], ["Moteur 400/690 V sur réseau 400 V ?", "Couplage triangle."], ["Comment inverser le sens d'un moteur triphasé ?", "Inverser deux phases."], ["Déphasage entre phases ?", "120°"]],
      quiz: [
        ["Sur le réseau français, la tension entre deux phases vaut :", ["230 V", "400 V", "690 V", "127 V"], 1, "Tension composée U = 400 V."],
        ["Moteur 230/400 V sur réseau 230/400 V :", ["Triangle", "Étoile", "Peu importe", "Impossible"], 1, "Chaque enroulement doit recevoir 230 V → étoile."],
        ["U = 400 V, I = 20 A, cos φ = 0,8. P ≈", ["11 kW", "6,4 kW", "19 kW", "8 kW"], 0, "1,732 × 400 × 20 × 0,8 ≈ 11 085 W."],
        ["Pour inverser le sens d'un moteur triphasé :", ["Inverser deux phases", "Couper le neutre", "Inverser les 3 phases", "Changer le fusible"], 0, "Le champ tourne dans l'autre sens."]
      ]
    },
    {
      id: "transfo", t: "Le transformateur", y: 1,
      html: `
<h3>Principe</h3>
<div class="easy box">Deux bobines enroulées sur un même circuit magnétique en fer. Le courant alternatif du primaire crée un champ magnétique variable qui induit une tension dans le secondaire. Il change la valeur d'une tension <b>alternative</b> (jamais continue !).</div>
<div class="f">m = U₂/U₁ = N₂/N₁ ≈ I₁/I₂</div>
<p>m &lt; 1 : abaisseur ; m &gt; 1 : élévateur. La fréquence ne change pas.</p>
<h3>Puissance et rendement</h3>
<div class="f">S_n = U₂n·I₂n (monophasé) &nbsp;&nbsp; η = P₂/(P₂ + P_fer + P_joule)</div>
<ul><li><b>Pertes fer</b> (dans le circuit magnétique) : mesurées à l'essai à vide, constantes.</li><li><b>Pertes Joule (cuivre)</b> : mesurées à l'essai en court-circuit, ∝ I².</li></ul>
<p>Rendement maximal quand pertes fer = pertes Joule. Un transformateur a un très bon rendement (95–99 %).</p>
<h3>Usages</h3>
<ul><li>Réseau de distribution (20 kV → 400 V).</li><li>Transformateur de commande 400/24 V pour les circuits de commande (sécurité TBTS).</li><li>Transformateur d'isolement (séparation des circuits).</li></ul>
<div class="ex box">Transfo 400/24 V – 100 VA : I₂n = 100/24 ≈ 4,2 A ; I₁n ≈ 100/400 = 0,25 A.</div>`,
      cards: [["Rapport de transformation ?", "m = U₂/U₁ = N₂/N₁"], ["Un transfo fonctionne-t-il en continu ?", "Non, uniquement en alternatif."], ["Essai à vide mesure ?", "Les pertes fer."], ["Essai en court-circuit mesure ?", "Les pertes Joule (cuivre)."]],
      quiz: [
        ["N₁ = 1000 spires, N₂ = 50. U₁ = 230 V. U₂ =", ["11,5 V", "4 600 V", "46 V", "230 V"], 0, "230 × 50/1000."],
        ["Un transformateur abaisseur de tension :", ["Abaisse aussi le courant", "Augmente le courant secondaire", "Change la fréquence", "Fonctionne en continu"], 1, "La puissance se conserve à peu près : U ↓ donc I ↑."],
        ["Les pertes fer se mesurent :", ["À vide", "En court-circuit", "En charge nominale", "On ne les mesure pas"], 0, "Sous tension nominale, sans charge."]
      ]
    },
    {
      id: "moteur_asy", t: "Le moteur asynchrone triphasé", y: 1,
      html: `
<h3>Le moteur le plus utilisé dans l'industrie</h3>
<div class="easy box">Le stator (fixe) crée un <b>champ magnétique tournant</b>. Le rotor (souvent « à cage d'écureuil ») est « entraîné » par ce champ, mais tourne un peu <b>moins vite</b> que lui : c'est le <b>glissement</b>. Simple, robuste, pas cher, peu d'entretien.</div>
<h3>Vitesse de synchronisme</h3>
<div class="f">n_s = 60·f / p &nbsp;&nbsp; <small>(tr/min, p = nombre de paires de pôles)</small></div>
<table><tr><th>p</th><th>1</th><th>2</th><th>3</th><th>4</th></tr><tr><td>n_s à 50 Hz</td><td>3 000</td><td>1 500</td><td>1 000</td><td>750</td></tr></table>
<h3>Glissement</h3>
<div class="f">g = (n_s − n)/n_s &nbsp;&nbsp; <small>(souvent 2 à 6 %)</small></div>
<div class="ex box">Moteur 4 pôles (p = 2), n = 1 440 tr/min : g = (1500 − 1440)/1500 = 4 %.</div>
<h3>Bilan des puissances</h3>
<div class="f">P_a = √3·U·I·cos φ → − pertes stator → P_tr (transmise au rotor) → − pertes Joule rotor (g·P_tr) → − pertes méca → P_u = C_u·Ω</div>
<div class="f">η = P_u / P_a</div>
<h3>Lire une plaque signalétique</h3>
<p>Ex : <code>230/400 V – 7,5/4,3 A – 2,2 kW – 1 430 tr/min – cos φ 0,82 – 50 Hz – IP55</code></p>
<ul><li>2,2 kW = puissance <b>utile</b> (mécanique, sur l'arbre).</li><li>1 430 tr/min → n_s = 1 500, moteur 4 pôles.</li><li>4,3 A en couplage étoile sous 400 V.</li><li>IP55 = protection poussières et jets d'eau.</li></ul>
<div class="ex box">Avec cette plaque : P_a = 1,732 × 400 × 4,3 × 0,82 ≈ 2 443 W → η ≈ 2200/2443 = 90 %. Couple utile : C = 2200/(1430 × 2π/60) ≈ 14,7 N·m.</div>
<h3>Démarrage</h3>
<p>Au démarrage direct, le courant vaut 5 à 8 fois le courant nominal ! Solutions :</p>
<ul><li><b>Démarrage étoile-triangle</b> : courant et couple divisés par 3 (moteur prévu pour Δ sur le réseau).</li><li><b>Démarreur progressif</b> (gradateur) : montée en tension douce.</li><li><b>Variateur de fréquence</b> : la meilleure solution (contrôle complet de la vitesse).</li></ul>
<h3>Point de fonctionnement</h3><p>C'est l'intersection de la courbe du couple moteur C_m(n) et de celle du couple résistant C_r(n) de la charge.</p>`,
      cards: [["Vitesse de synchronisme ?", "n_s = 60f/p"], ["Glissement ?", "g = (n_s − n)/n_s"], ["Puissance sur la plaque d'un moteur ?", "Puissance utile (mécanique)."], ["Courant de démarrage direct ?", "5 à 8 × In"], ["Démarrage étoile-triangle ?", "Divise courant et couple de démarrage par 3."], ["Moteur à 1 450 tr/min, 50 Hz : nb de pôles ?", "4 pôles (p = 2, n_s = 1 500)."]],
      quiz: [
        ["Moteur à 2 paires de pôles sur 50 Hz : n_s =", ["3 000 tr/min", "1 500 tr/min", "1 000 tr/min", "750 tr/min"], 1, "60 × 50 / 2."],
        ["n_s = 1 000 tr/min, n = 960 tr/min. g =", ["4 %", "40 %", "0,4 %", "96 %"], 0, "40/1000."],
        ["La puissance indiquée sur la plaque d'un moteur est :", ["Absorbée", "Utile", "Réactive", "Apparente"], 1, "C'est la puissance mécanique disponible."],
        ["Le moteur asynchrone tourne :", ["Exactement à n_s", "Un peu moins vite que n_s", "Plus vite que n_s", "À vitesse variable seul"], 1, "Le glissement est nécessaire pour créer le couple."],
        ["Pour régler finement la vitesse d'un moteur asynchrone :", ["Variateur de fréquence", "Démarrage étoile-triangle", "Changer le fusible", "Inverser deux phases"], 0, "On fait varier f donc n_s."]
      ]
    },
    {
      id: "autres_moteurs", t: "Moteurs à courant continu, pas à pas, brushless", y: 2,
      html: `
<h3>Moteur à courant continu (MCC)</h3>
<div class="f">E = k·Ω &nbsp;&nbsp; C = k·I &nbsp;&nbsp; U = E + R·I</div>
<div class="easy box">La <b>vitesse</b> dépend de la <b>tension</b>, le <b>couple</b> dépend du <b>courant</b>. Très facile à piloter (variateur = hacheur), mais balais et collecteur s'usent.</div>
<div class="ex box">MCC : U = 24 V, R = 0,5 Ω, k = 0,1 V/(rad/s), I = 4 A. E = 24 − 2 = 22 V → Ω = 220 rad/s ≈ 2 100 tr/min. C = 0,4 N·m.</div>
<h3>Moteur synchrone / brushless</h3><p>Rotor à aimants permanents, tourne exactement à la vitesse du champ (n = 60f/p). Piloté par un variateur (servomoteur) : très précis, très dynamique, bon rendement. Utilisé en robotique, machines-outils, véhicules électriques.</p>
<h3>Moteur pas à pas</h3><p>Tourne d'un angle précis à chaque impulsion (ex : 1,8° → 200 pas/tour). Positionnement sans capteur (boucle ouverte) : imprimantes 3D, petites tables. Attention à la perte de pas si le couple est insuffisant.</p>
<div class="f">Nombre de pas = angle voulu / angle d'un pas</div>
<h3>Choisir un moteur</h3>
<table><tr><th>Besoin</th><th>Moteur conseillé</th></tr>
<tr><td>Vitesse constante, puissant, pas cher</td><td>Asynchrone triphasé</td></tr><tr><td>Vitesse variable</td><td>Asynchrone + variateur</td></tr>
<tr><td>Positionnement précis et rapide</td><td>Servomoteur brushless</td></tr><tr><td>Petit positionnement économique</td><td>Pas à pas</td></tr>
<tr><td>Alimentation batterie basse tension</td><td>MCC ou brushless</td></tr></table>
<h3>Servomoteur et asservissement</h3><p>Un codeur mesure la position/vitesse, le variateur compare à la consigne et corrige (boucle fermée, correcteur PID).</p>`,
      cards: [["MCC : la vitesse dépend de ?", "La tension (E = kΩ)."], ["MCC : le couple dépend de ?", "Le courant (C = kI)."], ["Moteur pas à pas 1,8° : pas par tour ?", "200"], ["Avantage du brushless ?", "Précis, dynamique, bon rendement, sans balais."]],
      quiz: [
        ["Dans un MCC, pour augmenter la vitesse on augmente :", ["Le courant", "La tension", "La résistance", "Le nombre de pôles"], 1, "Ω = E/k ≈ U/k."],
        ["Moteur pas à pas de 1,8°/pas. Pour 90° :", ["50 pas", "90 pas", "162 pas", "200 pas"], 0, "90/1,8 = 50."],
        ["Pour un axe de robot rapide et précis :", ["Asynchrone sans variateur", "Servomoteur brushless", "Moteur universel", "Moteur à essence"], 1, "Boucle fermée avec codeur."]
      ]
    },
    {
      id: "variateurs", t: "Électronique de puissance et variateurs", y: 2,
      html: `
<h3>Les convertisseurs</h3>
<table><tr><th>Conversion</th><th>Nom</th><th>Exemple</th></tr>
<tr><td>Alternatif → continu</td><td>Redresseur</td><td>Chargeur, alimentation</td></tr>
<tr><td>Continu → continu</td><td>Hacheur</td><td>Variateur de MCC, alimentation à découpage</td></tr>
<tr><td>Continu → alternatif</td><td>Onduleur</td><td>Variateur de fréquence, onduleur solaire</td></tr>
<tr><td>Alternatif → alternatif (même f)</td><td>Gradateur</td><td>Démarreur progressif, variateur de lumière</td></tr></table>
<h3>Redresseur</h3><p>Pont de diodes (pont de Graetz). Redressement double alternance : U_moy = 2Û/π. Un condensateur lisse la tension.</p>
<h3>Hacheur série</h3>
<div class="f">U_moy = α·E &nbsp;&nbsp; <small>α = rapport cyclique (0 à 1) = t_on / T</small></div>
<p>On « découpe » la tension très vite (MLI/PWM) : la charge voit la valeur moyenne.</p>
<h3>Variateur de fréquence (pour moteur asynchrone)</h3>
<p>Redresseur → bus continu → onduleur MLI. Il fait varier f et U en gardant U/f constant (couple constant).</p>
<div class="key box">Avantages : démarrage doux (pas de pic de courant), vitesse réglable, économies d'énergie (surtout pompes/ventilateurs : P ∝ n³), rampes d'accélération, protections intégrées.</div>
<h3>Paramétrage de base d'un variateur</h3>
<ul><li>Données de la plaque moteur (U, I, f, n, P, cos φ).</li><li>Rampes d'accélération / décélération.</li><li>Vitesses min / max.</li><li>Source de la consigne (bornier 0–10 V, 4–20 mA, réseau, clavier).</li><li>Commandes marche / sens (bornier ou bus).</li><li>Autoréglage (auto-tuning).</li></ul>
<h3>Composants</h3><p>Diode (sens unique), thyristor (diode commandée), transistor MOSFET / IGBT (interrupteur rapide commandé).</p>
<h3>Perturbations</h3><p>Les variateurs créent des harmoniques et des perturbations CEM : câbles blindés, filtres, selfs de ligne.</p>`,
      cards: [["Convertisseur continu → alternatif ?", "Onduleur."], ["Convertisseur continu → continu ?", "Hacheur."], ["Hacheur : U_moy = ?", "α·E (α = rapport cyclique)."], ["Principe du variateur de fréquence ?", "Redresseur + bus continu + onduleur MLI, U/f = cste."], ["Pompe à vitesse réduite de moitié : puissance ?", "≈ 1/8 (P ∝ n³)."]],
      quiz: [
        ["Un hacheur alimenté en 48 V avec α = 0,25 donne :", ["12 V", "48 V", "192 V", "36 V"], 0, "0,25 × 48."],
        ["Un redresseur convertit :", ["DC → AC", "AC → DC", "AC → AC", "DC → DC"], 1, "Alternatif vers continu."],
        ["Un variateur de fréquence garde constant :", ["U × f", "U/f", "f seulement", "I × f"], 1, "Pour garder le flux et donc le couple."],
        ["Les transistors de puissance utilisés en onduleur sont souvent :", ["IGBT", "Ampoules", "Relais", "Fusibles"], 0, "IGBT / MOSFET."]
      ]
    },
    {
      id: "schemas", t: "Schémas électriques et appareillage", y: 1,
      html: `
<h3>Circuit de puissance et circuit de commande</h3>
<ul><li><b>Puissance</b> : alimente le moteur (400 V triphasé) : sectionneur, fusibles/disjoncteur, contacteur, relais thermique, moteur.</li><li><b>Commande</b> : décide quand le moteur marche (souvent 24 V) : boutons, bobine du contacteur, contacts auxiliaires, API.</li></ul>
<h3>L'appareillage et son rôle</h3>
<table><tr><th>Appareil</th><th>Fonction</th><th>Repère</th></tr>
<tr><td>Sectionneur</td><td>Isoler (séparer) l'installation – se manœuvre à vide !</td><td>Q</td></tr>
<tr><td>Fusible</td><td>Protéger contre les courts-circuits</td><td>F</td></tr>
<tr><td>Disjoncteur</td><td>Protéger contre surcharges et courts-circuits, réarmable</td><td>Q</td></tr>
<tr><td>Disjoncteur-moteur</td><td>Protection magnéto-thermique adaptée au moteur</td><td>Q</td></tr>
<tr><td>Contacteur</td><td>Commuter (établir/couper) la puissance à distance</td><td>KM</td></tr>
<tr><td>Relais thermique</td><td>Protéger le moteur contre les surcharges</td><td>F</td></tr>
<tr><td>Interrupteur différentiel</td><td>Protéger les personnes (défaut d'isolement)</td><td>Q</td></tr></table>
<div class="tip box">Les 4 fonctions de base d'un départ moteur : <b>Sectionner, Protéger (court-circuit), Commuter, Protéger (surcharge)</b>.</div>
<h3>Le circuit de commande « marche / arrêt » avec auto-maintien</h3>
<p>S1 (arrêt, contact NF) en série avec S2 (marche, contact NO). Un contact auxiliaire KM1 (NO) en parallèle sur S2 → quand on relâche S2, KM1 reste alimenté : c'est l'<b>auto-maintien</b>. Appui sur S1 → la bobine se coupe.</p>
<div class="key box">Les boutons d'arrêt et d'arrêt d'urgence sont toujours des contacts <b>NF (normalement fermés)</b> : si un fil se coupe, la machine s'arrête (sécurité positive).</div>
<h3>Inversion de sens de marche</h3><p>Deux contacteurs KM1 (avant) et KM2 (arrière) qui inversent deux phases, avec un <b>verrouillage électrique</b> (contact NF de KM2 dans le circuit de KM1 et inversement) et souvent mécanique, pour éviter un court-circuit.</p>
<h3>Lire les repères</h3><ul><li>Contacts de puissance : 1-2, 3-4, 5-6.</li><li>Contacts auxiliaires : 13-14 (NO), 21-22 (NF).</li><li>Bobine : A1-A2.</li></ul>
<h3>Section des câbles (ordre de grandeur, cuivre)</h3><p>1,5 mm² → 16 A (éclairage) ; 2,5 mm² → 20 A (prises) ; 6 mm² → 32 A (plaque). En industrie on calcule selon le mode de pose, la température et la chute de tension (≤ 3 % éclairage, 5 % autres en général).</p>`,
      cards: [["Rôle du sectionneur ?", "Isoler l'installation (manœuvre à vide)."], ["Rôle du contacteur ?", "Commuter la puissance à distance."], ["Rôle du relais thermique ?", "Protéger le moteur des surcharges."], ["Auto-maintien ?", "Contact NO du contacteur en parallèle du bouton marche."], ["Repère bobine de contacteur ?", "A1-A2"], ["Pourquoi l'arrêt est-il un contact NF ?", "Sécurité positive : un fil coupé arrête la machine."]],
      quiz: [
        ["Quel appareil protège les personnes contre les défauts d'isolement ?", ["Fusible", "Relais thermique", "Différentiel", "Contacteur"], 2, "Il détecte une fuite de courant (30 mA pour les personnes)."],
        ["Un contact auxiliaire 13-14 est :", ["NF", "NO (ouvert au repos)", "Une bobine", "Un fusible"], 1, "Les repères se terminant par 3-4 sont NO."],
        ["Le sectionneur doit être manœuvré :", ["En charge", "À vide", "En court-circuit", "Sous 1 000 A"], 1, "Il n'a pas de pouvoir de coupure."],
        ["Pour inverser le sens d'un moteur on utilise :", ["Un seul contacteur", "Deux contacteurs verrouillés", "Un transformateur", "Un fusible"], 1, "Avec verrouillage pour éviter le court-circuit."],
        ["Le relais thermique protège contre :", ["Les courts-circuits", "Les surcharges", "La foudre", "Les fuites de courant"], 1, "Échauffement lent du moteur."]
      ]
    },
    {
      id: "protection", t: "Protection des personnes : régimes de neutre", y: 2,
      html: `
<h3>Les dangers du courant</h3>
<table><tr><th>Courant</th><th>Effet (ordre de grandeur)</th></tr>
<tr><td>0,5 mA</td><td>seuil de perception</td></tr><tr><td>10 mA</td><td>contraction musculaire (on ne peut plus lâcher)</td></tr>
<tr><td>30 mA</td><td>paralysie respiratoire</td></tr><tr><td>75 mA et +</td><td>fibrillation cardiaque (mortel)</td></tr></table>
<p>Tension limite de sécurité : <b>50 V alternatif</b> en milieu sec (25 V en milieu humide), 120 V continu.</p>
<h3>Contacts directs et indirects</h3>
<ul><li><b>Contact direct</b> : toucher une partie active (fil dénudé). Protection : isolation, enveloppes, éloignement, DDR 30 mA en complément.</li><li><b>Contact indirect</b> : toucher une masse (carcasse métallique) mise accidentellement sous tension. Protection : mise à la terre + coupure automatique.</li></ul>
<h3>Les régimes de neutre (schémas de liaison à la terre)</h3>
<p>2 lettres : 1ʳᵉ = situation du neutre du transfo ; 2ᵉ = situation des masses. T = terre, N = neutre, I = isolé.</p>
<table><tr><th>Régime</th><th>Principe</th><th>Au 1er défaut</th><th>Usage</th></tr>
<tr><td><b>TT</b></td><td>Neutre à la terre, masses à la terre</td><td>Coupure par différentiel (DDR)</td><td>Domestique, petits bâtiments</td></tr>
<tr><td><b>TN</b> (TN-C, TN-S)</td><td>Neutre à la terre, masses reliées au neutre</td><td>Le défaut = court-circuit → coupure par disjoncteur/fusible</td><td>Industrie</td></tr>
<tr><td><b>IT</b></td><td>Neutre isolé (ou impédant), masses à la terre</td><td>Pas de coupure, signalement par CPI (contrôleur permanent d'isolement)</td><td>Continuité de service : hôpitaux, process</td></tr></table>
<div class="key box">IT = « Impossible de s'arrêter » (bloc opératoire, four verrier). Au 2ᵉ défaut il faut couper.</div>
<h3>Le différentiel</h3><p>Il compare le courant qui entre et celui qui sort. S'il manque du courant (fuite vers la terre via une personne ou une masse), il coupe. Sensibilité 30 mA pour les personnes.</p>
<h3>Protection des biens</h3><p>Surcharges (thermique), courts-circuits (magnétique, fusibles), surtensions (parafoudre). Le pouvoir de coupure doit être ≥ au courant de court-circuit possible.</p>
<h3>IP et IK</h3><p>IP XY : X = solides (0 à 6), Y = liquides (0 à 8). IK = résistance aux chocs (00 à 10).</p>`,
      cards: [["Tension limite de sécurité en milieu sec ?", "50 V AC"], ["Seuil de sensibilité d'un DDR pour protéger les personnes ?", "30 mA"], ["Régime TT ?", "Neutre à la terre, masses à la terre, coupure par différentiel."], ["Régime IT : avantage ?", "Continuité de service au 1er défaut (signalé par CPI)."], ["Contact indirect ?", "Toucher une masse mise accidentellement sous tension."], ["IP 65 ?", "Totalement protégé poussière (6), jets d'eau (5)."]],
      quiz: [
        ["Un hôpital (bloc opératoire) utilise de préférence le régime :", ["TT", "TN-C", "IT", "Aucun"], 2, "Pas de coupure au premier défaut."],
        ["Dans le régime TT, la coupure est assurée par :", ["Un fusible seul", "Un dispositif différentiel", "Un CPI", "Le sectionneur"], 1, "Le courant de défaut est trop faible pour un disjoncteur classique."],
        ["Un courant de 30 mA dans le corps peut provoquer :", ["Rien", "Une paralysie respiratoire", "Une simple sensation", "Une brûlure légère"], 1, "D'où les DDR 30 mA."],
        ["Toucher un fil dénudé sous tension est un :", ["Contact indirect", "Contact direct", "Court-circuit", "Défaut d'isolement"], 1, "Contact avec une partie active."]
      ]
    },
    {
      id: "habilitation", t: "Habilitation électrique et sécurité", y: 1,
      html: `
<h3>C'est quoi l'habilitation ?</h3>
<div class="easy box">C'est la reconnaissance, par l'employeur, qu'une personne est capable de travailler en sécurité au voisinage ou sur des installations électriques. Elle est délivrée après une <b>formation</b> (norme NF C 18-510) et doit être recyclée régulièrement (recommandé tous les 3 ans).</div>
<h3>Domaines de tension</h3>
<table><tr><th>Domaine</th><th>Alternatif</th><th>Continu</th></tr>
<tr><td>TBT (très basse tension)</td><td>≤ 50 V</td><td>≤ 120 V</td></tr><tr><td>BT (basse tension)</td><td>50 – 1 000 V</td><td>120 – 1 500 V</td></tr>
<tr><td>HTA</td><td>1 – 50 kV</td><td>1,5 – 75 kV</td></tr><tr><td>HTB</td><td>&gt; 50 kV</td><td>&gt; 75 kV</td></tr></table>
<h3>Lire un symbole d'habilitation : ex. B2V Essai</h3>
<ul><li><b>1ʳᵉ lettre</b> : domaine (B = BT/TBT, H = HT).</li><li><b>Chiffre</b> : 0 = non électricien (travaux non électriques), 1 = exécutant électricien, 2 = chargé de travaux.</li><li><b>Lettre suivante</b> : C = consignation, R = intervention BT générale (dépannage), S = intervention élémentaire (remplacement fusible, lampe), T = travaux sous tension, V = voisinage, E = opérations spécifiques (essai, vérification, mesurage, manœuvre), P = photovoltaïque, X = spéciale.</li></ul>
<div class="ex box">BR : chargé d'intervention générale en BT (dépannage, mesures). B0 : non électricien qui travaille dans un local électrique (peintre…). BC : chargé de consignation.</div>
<h3>Les étapes de la consignation (à connaître !)</h3>
<ol><li><b>Séparer</b> l'installation de toute source (ouvrir le sectionneur).</li><li><b>Condamner</b> en position d'ouverture (cadenas) + signaler (pancarte).</li><li><b>Identifier</b> l'ouvrage (être sûr de travailler au bon endroit).</li><li><b>Vérifier l'absence de tension</b> (VAT) avec un appareil adapté, testé avant et après.</li><li><b>Mettre à la terre et en court-circuit</b> (MALT-CC) – obligatoire en HT, parfois en BT.</li></ol>
<div class="tip box">Mnémo : « <b>S</b>i <b>C</b>harles <b>I</b>dentifie <b>V</b>ite, <b>M</b>arie… » → Séparer, Condamner, Identifier, Vérifier, Mettre à la terre.</div>
<h3>EPI électriques</h3><p>Gants isolants (classe adaptée), écran facial anti-UV (arc électrique), vêtements non propagateurs de flamme, VAT, outils isolés (1 000 V), tapis isolant.</p>`,
      cards: [["Domaine BT en alternatif ?", "50 V à 1 000 V"], ["B0 ?", "Non électricien, travaux d'ordre non électrique en BT."], ["BR ?", "Chargé d'intervention générale en BT."], ["Étapes de consignation ?", "Séparer, Condamner, Identifier, Vérifier l'absence de tension, (Mise à la terre)."], ["VAT ?", "Vérificateur d'Absence de Tension."]],
      quiz: [
        ["La TBT en alternatif correspond à :", ["≤ 50 V", "≤ 230 V", "≤ 1 000 V", "≤ 12 V"], 0, "Très basse tension ≤ 50 V AC."],
        ["La première étape de la consignation :", ["Vérifier l'absence de tension", "Séparer", "Mettre à la terre", "Condamner"], 1, "On ouvre d'abord le circuit (séparation)."],
        ["Un peintre qui travaille dans un local électrique doit être :", ["BR", "B0", "BC", "H2"], 1, "Non électricien."],
        ["Le VAT doit être testé :", ["Jamais", "Avant et après la vérification", "Une fois par an", "Seulement s'il est neuf"], 1, "Pour être sûr qu'il fonctionne."],
        ["La lettre « C » dans une habilitation signifie :", ["Courant", "Consignation", "Câblage", "Contrôle"], 1, "BC = chargé de consignation."]
      ]
    }
  ]
});
