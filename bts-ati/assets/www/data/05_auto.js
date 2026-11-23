COURS.push({
  id: "auto", name: "Automatisme & fluides", emoji: "🤖", color: "#059669",
  desc: "Chaîne d'information et d'énergie, logique combinatoire, GRAFCET, automates programmables (API), capteurs, actionneurs, pneumatique, hydraulique et asservissements / régulation.",
  chapters: [
    {
      id: "chaines", t: "Chaîne d'information et chaîne d'énergie", y: 1,
      html: `
<h3>Le modèle d'un système automatisé</h3>
<div class="easy box">Un système automatisé, c'est comme un corps humain : les <b>capteurs</b> sont les sens, l'<b>automate</b> est le cerveau, les <b>préactionneurs</b> sont les nerfs qui commandent les muscles, et les <b>actionneurs</b> sont les muscles.</div>
<h3>Chaîne d'information</h3>
<p><b>Acquérir</b> (capteurs, boutons) → <b>Traiter</b> (API, microcontrôleur) → <b>Communiquer</b> (IHM, voyants, réseau).</p>
<h3>Chaîne d'énergie</h3>
<p><b>Alimenter</b> (réseau, batterie, compresseur) → <b>Distribuer</b> (contacteur, variateur, distributeur) → <b>Convertir</b> (moteur, vérin) → <b>Transmettre</b> (réducteur, courroie, vis) → <b>Agir</b> sur la matière d'œuvre.</p>
<table><tr><th>Fonction</th><th>Électrique</th><th>Pneumatique</th><th>Hydraulique</th></tr>
<tr><td>Alimenter</td><td>Réseau, sectionneur</td><td>Compresseur, FRL</td><td>Centrale (pompe)</td></tr>
<tr><td>Distribuer</td><td>Contacteur, variateur</td><td>Distributeur</td><td>Distributeur</td></tr>
<tr><td>Convertir</td><td>Moteur</td><td>Vérin, moteur pneu</td><td>Vérin, moteur hydraulique</td></tr></table>
<h3>Matière d'œuvre et valeur ajoutée</h3><p>Le système agit sur une <b>matière d'œuvre</b> (produit, énergie, information) pour lui apporter une <b>valeur ajoutée</b> (la transformer, la déplacer, la contrôler).</p>
<h3>Partie commande / partie opérative</h3><p>PC = ce qui décide (automate). PO = ce qui agit (mécanique, actionneurs). Entre les deux : les ordres (sorties) et les comptes rendus (entrées des capteurs).</p>`,
      cards: [["Les 3 fonctions de la chaîne d'information ?", "Acquérir, Traiter, Communiquer."], ["Les 5 fonctions de la chaîne d'énergie ?", "Alimenter, Distribuer, Convertir, Transmettre, Agir."], ["Préactionneur électrique d'un moteur ?", "Contacteur (ou variateur)."], ["Préactionneur d'un vérin ?", "Distributeur."]],
      quiz: [
        ["Un capteur appartient à la fonction :", ["Acquérir", "Convertir", "Distribuer", "Transmettre"], 0, "Il acquiert une information."],
        ["Un moteur réalise la fonction :", ["Distribuer", "Convertir", "Traiter", "Acquérir"], 1, "Énergie électrique → mécanique."],
        ["Le préactionneur d'un vérin pneumatique est :", ["Un contacteur", "Un distributeur", "Un compresseur", "Un capteur"], 1, "Il distribue l'air dans les chambres."],
        ["L'automate (API) réalise la fonction :", ["Traiter", "Agir", "Alimenter", "Transmettre"], 0, "C'est le cerveau."]
      ]
    },
    {
      id: "logique", t: "Logique combinatoire et numération", y: 1,
      html: `
<h3>Variables logiques</h3><p>Une variable logique vaut 0 (faux, ouvert) ou 1 (vrai, fermé).</p>
<h3>Les opérateurs de base</h3>
<table><tr><th>Fonction</th><th>Équation</th><th>Vrai quand…</th><th>Câblage</th></tr>
<tr><td>NON (NOT)</td><td>S = ā</td><td>a = 0</td><td>contact NF</td></tr>
<tr><td>ET (AND)</td><td>S = a·b</td><td>a ET b = 1</td><td>contacts en série</td></tr>
<tr><td>OU (OR)</td><td>S = a + b</td><td>a OU b = 1</td><td>contacts en parallèle</td></tr>
<tr><td>OU exclusif (XOR)</td><td>S = a ⊕ b</td><td>a ≠ b</td><td>va-et-vient</td></tr>
<tr><td>NON-ET (NAND)</td><td>S = (a·b)̄</td><td>pas les deux</td><td></td></tr>
<tr><td>NON-OU (NOR)</td><td>S = (a+b)̄</td><td>aucun des deux</td><td></td></tr></table>
<h3>Table de vérité</h3><p>On liste toutes les combinaisons des entrées (2ⁿ lignes pour n variables) et la sortie correspondante.</p>
<h3>Règles de simplification (algèbre de Boole)</h3>
<div class="f">a + 0 = a &nbsp; a·1 = a &nbsp; a + 1 = 1 &nbsp; a·0 = 0 &nbsp; a + ā = 1 &nbsp; a·ā = 0 &nbsp; a + a·b = a</div>
<div class="f">Théorèmes de De Morgan : (a·b)̄ = ā + b̄ &nbsp;&nbsp; (a + b)̄ = ā·b̄</div>
<h3>Tableau de Karnaugh</h3><p>Méthode graphique pour simplifier : on regroupe les 1 adjacents par paquets de 1, 2, 4, 8 (les plus gros possibles). Chaque paquet donne un terme simplifié.</p>
<div class="ex box">Une presse ne descend que si les deux boutons (b1, b2) sont appuyés ET le carter fermé (c) : S = b1·b2·c (commande bimanuelle).</div>
<h3>Numération</h3>
<table><tr><th>Décimal</th><th>Binaire</th><th>Hexa</th></tr><tr><td>5</td><td>0101</td><td>5</td></tr><tr><td>10</td><td>1010</td><td>A</td></tr><tr><td>15</td><td>1111</td><td>F</td></tr><tr><td>255</td><td>1111 1111</td><td>FF</td></tr></table>
<p>Binaire → décimal : 1011 = 8 + 0 + 2 + 1 = 11. Un octet = 8 bits (0 à 255). Code Gray : un seul bit change entre deux valeurs consécutives (codeurs absolus).</p>`,
      cards: [["Fonction ET en câblage ?", "Contacts en série."], ["Fonction OU en câblage ?", "Contacts en parallèle."], ["De Morgan : (a·b)̄ = ?", "ā + b̄"], ["a + a·b = ?", "a"], ["1101 en décimal ?", "13"], ["Valeur max d'un octet ?", "255"]],
      quiz: [
        ["S = a·b vaut 1 si :", ["a = 1 ou b = 1", "a = 1 et b = 1", "a = 0", "jamais"], 1, "ET logique."],
        ["(a + b)̄ = ", ["ā + b̄", "ā·b̄", "a·b", "a + b"], 1, "De Morgan."],
        ["Le nombre binaire 1010 vaut :", ["10", "12", "5", "1010"], 0, "8 + 2 = 10."],
        ["Pour 3 variables, la table de vérité a :", ["3 lignes", "6 lignes", "8 lignes", "9 lignes"], 2, "2³ = 8."],
        ["FF en hexadécimal = ", ["255", "16", "100", "256"], 0, "15 × 16 + 15."]
      ]
    },
    {
      id: "grafcet", t: "Le GRAFCET", y: 1,
      html: `
<h3>À quoi sert le GRAFCET ?</h3>
<div class="easy box">Le GRAFCET (norme NF EN 60848) est un « mode d'emploi » graphique qui décrit étape par étape ce que fait un système automatisé : « j'attends ceci, puis je fais cela ». C'est le langage commun entre le mécanicien, l'automaticien et le client.</div>
<h3>Les éléments</h3>
<ul>
<li><b>Étape</b> (carré numéroté) : une situation stable du système. L'étape initiale a un double carré.</li>
<li><b>Action</b> (rectangle à droite de l'étape) : ce qui est fait pendant l'étape (ex : « Sortir vérin » / 14M+).</li>
<li><b>Transition</b> (petit trait horizontal) : passage d'une étape à la suivante.</li>
<li><b>Réceptivité</b> (condition à côté de la transition) : ce qu'il faut pour franchir (ex : capteur a1 = 1).</li>
<li><b>Liaisons orientées</b> : de haut en bas par défaut (flèche si on remonte).</li></ul>
<h3>Les 5 règles d'évolution</h3>
<ol><li>Au départ, seules les étapes initiales sont actives.</li><li>Une transition est <b>validée</b> si toutes les étapes en amont sont actives.</li><li>Elle est <b>franchie</b> si elle est validée ET si sa réceptivité est vraie.</li><li>Le franchissement <b>active</b> les étapes suivantes et <b>désactive</b> les précédentes (en même temps).</li><li>Si une étape doit être activée et désactivée en même temps, elle reste active.</li></ol>
<h3>Structures</h3>
<ul><li><b>Séquence linéaire</b> : étapes les unes après les autres.</li><li><b>Divergence en OU</b> (aiguillage) : un choix entre plusieurs branches (un seul trait horizontal, une transition par branche). Les réceptivités doivent être exclusives.</li><li><b>Divergence en ET</b> (parallélisme) : plusieurs branches démarrent en même temps (double trait horizontal). Elles se rejoignent sur une convergence en ET (double trait) : on attend que toutes soient finies.</li><li><b>Reprise de séquence / saut d'étapes</b>.</li></ul>
<h3>Types d'actions</h3>
<ul><li><b>Continue</b> : active tant que l'étape est active.</li><li><b>Conditionnelle</b> : action + condition (ex : moteur seulement si porte fermée).</li><li><b>Mémorisée</b> : S = 1 (mise à 1 à l'activation), R = 0 (remise à 0).</li><li><b>Temporisation</b> : réceptivité « 5s/X3 » = vraie 5 s après l'activation de l'étape 3.</li></ul>
<div class="ex box">Perceuse automatique : 0 (attente) —dcy·h— 1 Rotation broche + Descente —b— 2 Rotation broche + Montée —h— retour à 0.</div>
<h3>Points de vue</h3><p>Point de vue <b>système</b> (ce que fait le produit), <b>partie opérative</b> (actions sur les actionneurs), <b>partie commande</b> (variables codées : entrées/sorties de l'automate %I, %Q).</p>
<h3>GEMMA</h3><p>Guide d'Étude des Modes de Marche et d'Arrêt : liste les modes (production normale, arrêt d'urgence, réglages, marche de vérification…) et organise les GRAFCET de sécurité, de conduite et de production.</p>`,
      cards: [["Symbole de l'étape initiale ?", "Double carré."], ["Quand une transition est-elle franchie ?", "Quand elle est validée (étapes amont actives) ET réceptivité vraie."], ["Divergence en ET ?", "Plusieurs branches actives simultanément (double trait)."], ["Divergence en OU ?", "Choix d'une seule branche (simple trait)."], ["Réceptivité 3s/X5 ?", "Vraie 3 s après l'activation de l'étape 5."], ["GEMMA ?", "Guide d'Étude des Modes de Marches et d'Arrêts."]],
      quiz: [
        ["L'étape initiale se représente par :", ["Un cercle", "Un double carré", "Un triangle", "Un losange"], 1, "Double carré."],
        ["Une transition est franchie si :", ["La réceptivité est vraie", "L'étape amont est active et la réceptivité vraie", "L'étape aval est active", "Toujours"], 1, "Règles 2 et 3."],
        ["Pour lancer deux opérations en même temps on utilise :", ["Une divergence en OU", "Une divergence en ET", "Un saut d'étape", "Une temporisation"], 1, "Double trait = parallélisme."],
        ["Une action mémorisée « KM1 := 1 » :", ["S'arrête à la fin de l'étape", "Reste active jusqu'à une remise à 0", "N'existe pas", "Dure 1 s"], 1, "Elle est mémorisée."],
        ["Dans une divergence en OU, les réceptivités doivent être :", ["Identiques", "Exclusives", "Toujours vraies", "Temporisées"], 1, "Sinon deux branches pourraient s'activer en même temps."]
      ]
    },
    {
      id: "api", t: "Automates programmables (API)", y: 2,
      html: `
<h3>Structure d'un API</h3>
<ul><li>Alimentation (24 V DC en général).</li><li>Processeur (CPU) + mémoire.</li><li>Cartes d'<b>entrées</b> TOR (tout ou rien) et analogiques (0–10 V, 4–20 mA).</li><li>Cartes de <b>sorties</b> TOR (relais ou transistor) et analogiques.</li><li>Modules de communication (Ethernet, Profinet, Modbus…).</li></ul>
<h3>Le cycle automate</h3>
<div class="f">Lecture des entrées → Exécution du programme → Écriture des sorties → (recommence, ≈ 1 à 50 ms)</div>
<h3>Les langages (norme IEC 61131-3)</h3>
<table><tr><th>Langage</th><th>Description</th></tr>
<tr><td>LD (Ladder / contacts)</td><td>Schéma à contacts, proche de l'électricité</td></tr>
<tr><td>FBD (blocs fonctionnels)</td><td>Portes logiques et blocs</td></tr>
<tr><td>SFC (Grafcet)</td><td>Séquentiel, étapes/transitions</td></tr>
<tr><td>ST (texte structuré)</td><td>Proche du Pascal : IF… THEN, FOR…</td></tr>
<tr><td>IL (liste d'instructions)</td><td>Bas niveau (abandonné)</td></tr></table>
<h3>Programmer un GRAFCET en Ladder (méthode « set/reset »)</h3>
<ul><li>Activation d'une étape : SET Xn si (Xn-1 actif ET réceptivité).</li><li>Désactivation : RESET Xn-1 en même temps.</li><li>Sorties : Q = Xn (+ conditions).</li></ul>
<pre>// Exemple en ST
IF X0 AND dcy AND h THEN X1 := TRUE; X0 := FALSE; END_IF;
IF X1 AND b THEN X2 := TRUE; X1 := FALSE; END_IF;
IF X2 AND h THEN X0 := TRUE; X2 := FALSE; END_IF;
moteur := X1 OR X2;
descente := X1;  montee := X2;</pre>
<h3>Fonctions courantes</h3><ul><li><b>Temporisations</b> : TON (retard à la montée), TOF (retard à la retombée), TP (impulsion).</li><li><b>Compteurs</b> : CTU (comptage), CTD (décomptage).</li><li>Front montant (R_TRIG) / descendant (F_TRIG).</li><li>Comparaisons, calculs, mise à l'échelle des analogiques.</li></ul>
<h3>Mise à l'échelle d'un capteur 4–20 mA</h3>
<div class="f">Valeur = Min + (I − 4)/(20 − 4) × (Max − Min)</div>
<div class="ex box">Capteur de pression 0–10 bar en 4–20 mA, on lit 12 mA → 0 + (8/16) × 10 = 5 bar. 4 mA = 0 bar ; moins de 4 mA = câble coupé (détection de défaut, d'où l'intérêt du 4 mA !).</div>
<h3>Supervision et IHM</h3><p>IHM (écran tactile) pour l'opérateur ; supervision SCADA pour suivre toute l'usine.</p>`,
      cards: [["Cycle automate ?", "Lire entrées → exécuter programme → écrire sorties."], ["Les 5 langages IEC 61131-3 ?", "LD, FBD, SFC, ST, IL."], ["TON ?", "Temporisation retard à la montée."], ["Pourquoi le 4–20 mA plutôt que 0–20 ?", "4 mA = zéro mesure ; < 4 mA = défaut (câble coupé)."], ["12 mA sur un capteur 0–100 °C (4–20 mA) ?", "50 °C"]],
      quiz: [
        ["Le langage Ladder ressemble à :", ["Un schéma électrique à contacts", "Du C++", "Un Grafcet", "Une table de vérité"], 0, "LD = Ladder Diagram."],
        ["Capteur 4–20 mA pour 0–200 °C. On lit 8 mA :", ["40 °C", "50 °C", "80 °C", "100 °C"], 1, "(8−4)/16 × 200 = 50 °C."],
        ["Le cycle d'un automate commence par :", ["Écrire les sorties", "Lire les entrées", "Afficher l'IHM", "Redémarrer"], 1, "Image des entrées en mémoire."],
        ["La temporisation qui retarde l'activation d'une sortie :", ["TOF", "TON", "CTU", "R_TRIG"], 1, "Timer ON delay."],
        ["Le langage SFC correspond à :", ["Grafcet", "Ladder", "Texte", "Blocs"], 0, "Sequential Function Chart."]
      ]
    },
    {
      id: "capteurs", t: "Capteurs et détecteurs", y: 1,
      html: `
<h3>Capteur TOR, analogique, numérique</h3>
<ul><li><b>TOR</b> (tout ou rien) : 0 ou 1 (présence / absence). On parle de détecteur.</li><li><b>Analogique</b> : signal continu proportionnel (0–10 V, 4–20 mA).</li><li><b>Numérique</b> : valeur codée (codeur, bus de terrain, IO-Link).</li></ul>
<h3>Les détecteurs de présence</h3>
<table><tr><th>Type</th><th>Détecte</th><th>Portée</th><th>Remarque</th></tr>
<tr><td>Mécanique (fin de course)</td><td>tout objet par contact</td><td>contact</td><td>simple, robuste, usure</td></tr>
<tr><td>Inductif</td><td>métaux uniquement</td><td>1 à 40 mm</td><td>sans contact, très fiable</td></tr>
<tr><td>Capacitif</td><td>tout matériau (liquides, poudres, plastique)</td><td>1 à 30 mm</td><td>détection de niveau à travers une paroi</td></tr>
<tr><td>Photoélectrique</td><td>tout objet</td><td>jusqu'à des dizaines de m</td><td>barrage, reflex, proximité</td></tr>
<tr><td>Ultrasons</td><td>tout objet (même transparent)</td><td>jusqu'à quelques m</td><td>mesure de distance aussi</td></tr>
<tr><td>ILS (magnétique)</td><td>aimant</td><td>quelques mm</td><td>position des pistons de vérins</td></tr></table>
<h3>Cellules photoélectriques</h3>
<ul><li><b>Barrage</b> : émetteur et récepteur séparés (la plus grande portée, la plus fiable).</li><li><b>Reflex</b> : émetteur-récepteur + réflecteur.</li><li><b>Proximité</b> (diffuse) : la lumière est renvoyée par l'objet lui-même.</li></ul>
<h3>Mesure de grandeurs</h3>
<ul><li>Température : thermocouple (fortes températures), sonde Pt100 (précise : 100 Ω à 0 °C), thermistance CTN/CTP.</li><li>Pression : capteur piézorésistif, pressostat (TOR).</li><li>Force / masse : jauges de contrainte (pont de Wheatstone), pesons.</li><li>Position / vitesse : codeur incrémental (impulsions), codeur absolu (position codée), potentiomètre, LVDT, génératrice tachymétrique.</li><li>Niveau, débit, couleur, vision industrielle (caméra).</li></ul>
<h3>Câblage PNP / NPN (capteurs 3 fils)</h3><p>PNP : la sortie fournit +24 V quand le capteur est actif (standard en Europe). NPN : la sortie relie au 0 V. Fils : marron +24 V, bleu 0 V, noir sortie.</p>
<h3>Caractéristiques à vérifier</h3><p>Étendue de mesure, sensibilité, précision, résolution, temps de réponse, linéarité, indice de protection IP.</p>
<h3>Codeur incrémental</h3><div class="f">Résolution = 360° / nombre de points par tour</div>`,
      cards: [["Détecteur inductif détecte ?", "Uniquement les métaux."], ["Détecteur capacitif détecte ?", "Tous les matériaux (liquides, plastiques…)."], ["Cellule la plus fiable et longue portée ?", "Barrage (émetteur et récepteur séparés)."], ["Pt100 ?", "Sonde de température platine, 100 Ω à 0 °C."], ["Câblage capteur 3 fils ?", "Marron +24 V, bleu 0 V, noir sortie."], ["Codeur incrémental 1 000 pts/tr : résolution ?", "0,36°"]],
      quiz: [
        ["Pour détecter une bouteille en plastique sans contact :", ["Inductif", "Capacitif ou photoélectrique", "ILS", "Fin de course"], 1, "L'inductif ne voit que le métal."],
        ["Pour détecter la position d'un piston de vérin à travers le corps :", ["ILS / capteur magnétique", "Inductif", "Thermocouple", "Pt100"], 0, "Le piston contient un aimant."],
        ["Un capteur PNP actif fournit sur sa sortie :", ["0 V", "+24 V", "−24 V", "230 V"], 1, "Il « source » le courant."],
        ["Pour mesurer 900 °C dans un four :", ["Pt100", "Thermocouple", "ILS", "Capacitif"], 1, "Le thermocouple supporte les hautes températures."]
      ]
    },
    {
      id: "pneumatique", t: "Pneumatique", y: 1,
      html: `
<h3>Avantages et limites</h3>
<p>✅ Propre, rapide, simple, pas cher, sans risque d'incendie, supporte les surcharges. ❌ Efforts limités (air compressible), bruit, positionnement intermédiaire difficile, énergie chère à produire.</p>
<h3>Production et traitement de l'air</h3>
<p>Compresseur → réservoir → sécheur → réseau (souvent 6 à 7 bar) → <b>FRL</b> à l'entrée de chaque machine : <b>F</b>iltre, <b>R</b>égulateur (détendeur) de pression, <b>L</b>ubrificateur (de moins en moins utilisé). + vanne de sectionnement et de mise en pression progressive.</p>
<h3>Vérins</h3>
<ul><li><b>Simple effet</b> : l'air sort la tige, un ressort la rentre.</li><li><b>Double effet</b> : l'air pousse dans les deux sens.</li><li>Vérins sans tige, rotatifs, compacts, pinces, ventouses (avec venturi générateur de vide).</li></ul>
<div class="f">F_sortie = p × π·D²/4 &nbsp;&nbsp; F_rentrée = p × π(D² − d²)/4</div>
<div class="ex box">Vérin Ø63, tige Ø20, 6 bar (0,6 MPa) : F_sortie = 0,6 × π × 63²/4 ≈ 1 870 N. On prend un taux de charge de 0,5 à 0,7 → effort utile ≈ 1 000–1 300 N.</div>
<h3>Distributeurs</h3>
<p>Désignés par « nombre d'orifices / nombre de positions » : <b>3/2</b> (pour simple effet), <b>5/2</b> (double effet), 5/3 (position centrale). Commande : manuelle, mécanique, électrique (électrovanne), pneumatique. <b>Monostable</b> (retour par ressort) ou <b>bistable</b> (mémoire : garde sa position).</p>
<div class="key box">Monostable = le vérin revient quand on coupe le signal. Bistable = il garde sa position même si on coupe le courant (attention à la sécurité au redémarrage !).</div>
<h3>Autres composants</h3><ul><li>Réducteur de débit unidirectionnel (régler la vitesse, de préférence à l'échappement).</li><li>Clapet anti-retour, sélecteur de circuit (OU), cellule ET.</li><li>Silencieux, raccords instantanés.</li></ul>
<h3>Consommation d'air</h3><p>Volume par cycle ≈ (V_sortie + V_rentrée) × (p_relative + 1) en litres d'air libre. Les fuites coûtent très cher : une fuite de 1 mm à 6 bar ≈ plusieurs centaines d'euros par an.</p>`,
      cards: [["FRL ?", "Filtre, Régulateur, Lubrificateur."], ["Distributeur pour vérin double effet ?", "5/2 (ou 5/3)."], ["Monostable vs bistable ?", "Monostable revient au repos par ressort ; bistable garde sa position."], ["Force de sortie d'un vérin ?", "F = p × πD²/4"], ["Comment régler la vitesse d'un vérin ?", "Réducteur de débit (de préférence sur l'échappement)."]],
      quiz: [
        ["Un distributeur 5/2 possède :", ["5 positions et 2 orifices", "5 orifices et 2 positions", "5 vérins", "2 bobines obligatoirement"], 1, "Orifices / positions."],
        ["Le « R » de FRL signifie :", ["Réservoir", "Régulateur de pression", "Relais", "Retour"], 1, "Il fixe la pression d'utilisation."],
        ["Un vérin double effet Ø50 à 5 bar sort avec ≈", ["980 N", "250 N", "9 800 N", "98 N"], 0, "0,5 × π × 25² ≈ 982 N."],
        ["La force de rentrée d'un vérin double effet est :", ["Plus grande qu'en sortie", "Plus petite (section de la tige)", "Égale", "Nulle"], 1, "La tige réduit la surface utile."],
        ["Inconvénient de la pneumatique :", ["Dangereuse (incendie)", "Positionnement précis difficile", "Très lente", "Très sale"], 1, "L'air est compressible."]
      ]
    },
    {
      id: "hydraulique", t: "Hydraulique", y: 2,
      html: `
<h3>Pourquoi l'hydraulique ?</h3><p>L'huile est quasi incompressible : <b>efforts très élevés</b> (presses, engins de chantier, levage), mouvements précis et réguliers, puissance importante dans un petit volume. Inconvénients : fuites, risque d'incendie, entretien (filtration), coût.</p>
<h3>La centrale hydraulique</h3><p>Réservoir → crépine → <b>pompe</b> (engrenages, palettes, pistons) entraînée par un moteur → <b>limiteur de pression</b> (sécurité, obligatoire) → filtre → distributeurs → actionneurs → retour réservoir. Pressions courantes : 100 à 350 bar.</p>
<h3>Formules clés</h3>
<div class="f">F = p·S &nbsp;&nbsp; v = Q/S &nbsp;&nbsp; P = p·Q</div>
<div class="f">Q (L/min) = Cyl (cm³/tr) × N (tr/min) / 1000 × η_v</div>
<div class="ex box">Vérin Ø100 (S = 78,5 cm²) à 150 bar : F = 15 MPa × 7 854 mm² ≈ 118 000 N (≈ 12 tonnes). Avec Q = 20 L/min : v = 20 000 cm³/min / 78,5 cm² ≈ 255 cm/min ≈ 4,2 cm/s.</div>
<div class="ex box">Puissance : 150 bar et 20 L/min → P = 15×10⁶ × (20×10⁻³/60) = 5 000 W. Formule pratique : P(kW) = p(bar) × Q(L/min) / 600.</div>
<h3>Composants</h3>
<ul><li><b>Limiteur de pression</b> : protège le circuit.</li><li><b>Réducteur de pression</b> : pression plus basse sur une partie du circuit.</li><li><b>Régulateur / limiteur de débit</b> : règle les vitesses.</li><li><b>Clapet anti-retour piloté</b> : bloque une charge (sécurité).</li><li><b>Accumulateur</b> : réserve d'énergie (attention : reste sous pression machine arrêtée !).</li><li>Distributeurs tout-ou-rien ou proportionnels / servovalves.</li></ul>
<h3>Maintenance</h3><p>80 % des pannes hydrauliques viennent de la <b>pollution de l'huile</b> : filtration, analyse d'huile, propreté lors des interventions. Toujours décomprimer le circuit (et les accumulateurs) avant intervention.</p>`,
      cards: [["Formule puissance hydraulique pratique ?", "P(kW) = p(bar) × Q(L/min) / 600"], ["Composant de sécurité obligatoire d'une centrale ?", "Limiteur de pression."], ["Vitesse d'un vérin ?", "v = Q/S"], ["Cause principale des pannes hydrauliques ?", "La pollution de l'huile."], ["Danger de l'accumulateur ?", "Il reste sous pression machine arrêtée."]],
      quiz: [
        ["Avantage principal de l'hydraulique :", ["Propreté", "Efforts très importants", "Prix bas", "Pas de fuite"], 1, "Huile incompressible, hautes pressions."],
        ["200 bar et 30 L/min : P ≈", ["10 kW", "6 kW", "100 kW", "1 kW"], 0, "200 × 30 / 600 = 10 kW."],
        ["Pour augmenter la vitesse d'un vérin hydraulique :", ["Augmenter le débit", "Augmenter la pression", "Diminuer le débit", "Changer l'huile"], 0, "v = Q/S."],
        ["Avant d'intervenir sur un circuit hydraulique :", ["Augmenter la pression", "Décomprimer le circuit et les accumulateurs", "Rien de particulier", "Chauffer l'huile"], 1, "Risque d'injection d'huile sous pression."]
      ]
    },
    {
      id: "asservissement", t: "Asservissement et régulation (PID)", y: 2,
      html: `
<h3>Boucle ouverte / boucle fermée</h3>
<div class="easy box">En <b>boucle ouverte</b>, on envoie un ordre sans vérifier le résultat (grille-pain). En <b>boucle fermée</b>, on mesure le résultat et on corrige en permanence (régulateur de vitesse d'une voiture, chauffage avec thermostat).</div>
<h3>Schéma-bloc d'une boucle fermée</h3>
<p>Consigne → <b>comparateur</b> (écart ε = consigne − mesure) → <b>correcteur</b> → actionneur → système → sortie → <b>capteur</b> → retour au comparateur.</p>
<p>Asservissement = suivre une consigne variable (position d'un robot). Régulation = maintenir une consigne fixe malgré les perturbations (température d'un four).</p>
<h3>Les performances</h3>
<ul><li><b>Stabilité</b> : le système ne doit pas osciller sans fin.</li><li><b>Précision</b> : erreur statique (écart final entre consigne et sortie).</li><li><b>Rapidité</b> : temps de réponse à 5 %.</li><li><b>Dépassement</b> : de combien la sortie dépasse la consigne.</li></ul>
<h3>Le correcteur PID</h3>
<table><tr><th>Action</th><th>Effet</th><th>Inconvénient si trop fort</th></tr>
<tr><td><b>P</b> (proportionnelle)</td><td>Plus rapide, réduit l'erreur</td><td>Oscillations, erreur statique restante</td></tr>
<tr><td><b>I</b> (intégrale)</td><td>Annule l'erreur statique</td><td>Ralentit, dépassements</td></tr>
<tr><td><b>D</b> (dérivée)</td><td>Anticipe, amortit, stabilise</td><td>Sensible au bruit de mesure</td></tr></table>
<h3>Modèles de systèmes</h3>
<ul><li><b>1er ordre</b> : H(p) = K/(1 + τp). Réponse à un échelon sans dépassement, 63 % à τ, temps de réponse à 5 % = 3τ.</li><li><b>2nd ordre</b> : H(p) = K/(1 + 2ξp/ω₀ + p²/ω₀²). ξ = amortissement (ξ &lt; 1 → oscillations ; ξ ≈ 0,7 → réponse rapide avec léger dépassement de 5 %).</li></ul>
<div class="ex box">Four : gain K = 2 °C/%, τ = 10 min. Après un échelon de 10 % de puissance : +20 °C à la fin, atteints à 95 % au bout de 30 min.</div>
<h3>Régulation TOR (tout ou rien)</h3><p>Le thermostat simple : marche/arrêt avec un <b>hystérésis</b> (ex : chauffe sous 19 °C, coupe à 21 °C) pour éviter les commutations trop fréquentes.</p>`,
      cards: [["Différence boucle ouverte / fermée ?", "Boucle fermée : on mesure la sortie et on corrige."], ["Rôle de l'action intégrale ?", "Annuler l'erreur statique."], ["Rôle de l'action dérivée ?", "Anticiper, amortir, stabiliser."], ["1er ordre : temps de réponse à 5 % ?", "3τ"], ["Les 4 performances d'un asservissement ?", "Stabilité, précision, rapidité, dépassement."]],
      quiz: [
        ["Un thermostat de chauffage qui mesure la température est un système :", ["En boucle ouverte", "En boucle fermée", "Sans capteur", "Pneumatique"], 1, "Il compare et corrige."],
        ["Pour supprimer l'erreur statique on ajoute une action :", ["Proportionnelle", "Intégrale", "Dérivée", "Aucune"], 1, "I = intégrale de l'erreur."],
        ["Système du 1er ordre τ = 5 s. Temps de réponse à 5 % :", ["5 s", "10 s", "15 s", "25 s"], 2, "3τ."],
        ["Un gain proportionnel trop élevé provoque :", ["Une lenteur", "Des oscillations", "Plus de stabilité", "Rien"], 1, "Le système devient instable."]
      ]
    }
  ]
});
