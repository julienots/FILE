COURS.push({
  id: "projet", name: "Projet, qualité, sécurité, maintenance", emoji: "📊", color: "#0891B2",
  desc: "Gestion de projet (planning Gantt et PERT, ressources, risques), économie et coûts, qualité (outils, normes ISO 9001, capabilité), santé-sécurité-environnement, maintenance industrielle, lean et communication technique.",
  chapters: [
    {
      id: "gestion_projet", t: "Gestion de projet : les bases", y: 1,
      html: `
<h3>C'est quoi un projet ?</h3>
<div class="easy box">Un projet est un ensemble d'actions pour atteindre un <b>objectif précis</b>, avec un <b>début et une fin</b>, un <b>budget</b> et des <b>ressources</b> limitées. Le triangle d'or : <b>Qualité – Coût – Délai (QCD)</b>. Si on en modifie un, les autres bougent.</div>
<h3>Les acteurs</h3>
<ul><li><b>Maître d'ouvrage (MOA)</b> : le client, celui qui exprime le besoin et paie.</li><li><b>Maître d'œuvre (MOE)</b> : celui qui réalise (le chef de projet et son équipe).</li><li>Le comité de pilotage (décisions), les experts, les fournisseurs, les sous-traitants.</li></ul>
<h3>Les phases d'un projet</h3>
<ol><li><b>Initialisation</b> : besoin, étude de faisabilité, objectifs SMART.</li><li><b>Conception / planification</b> : cahier des charges, découpage des tâches, planning, budget.</li><li><b>Réalisation</b> : exécution, suivi, réunions, tableaux de bord.</li><li><b>Clôture</b> : réception, levée des réserves, bilan, retour d'expérience (REX).</li></ol>
<h3>Objectifs SMART</h3><p><b>S</b>pécifique, <b>M</b>esurable, <b>A</b>tteignable, <b>R</b>éaliste, <b>T</b>emporellement défini. Ex : « Réduire de 15 % le temps de changement de série sur la ligne 2 d'ici le 30 juin ».</p>
<h3>Découpage : l'OTP / WBS</h3><p>L'Organigramme des Tâches du Projet découpe le projet en lots puis en tâches élémentaires (durée, ressources, coût estimables).</p>
<h3>Matrice RACI</h3><p>Pour chaque tâche : qui est <b>R</b>éalisateur, <b>A</b>pprobateur (responsable final), <b>C</b>onsulté, <b>I</b>nformé.</p>
<h3>Gestion des risques</h3><p>Identifier les risques, évaluer Criticité = Probabilité × Gravité, prévoir des actions (éviter, réduire, transférer, accepter), suivre.</p>
<h3>Outils de suivi</h3><p>Tableau de bord (indicateurs d'avancement, coûts, délais), compte rendu de réunion, jalons, courbe en S, méthode de la valeur acquise. Méthodes agiles (Scrum, Kanban) pour les projets évolutifs.</p>`,
      cards: [["QCD ?", "Qualité, Coût, Délai."], ["MOA ?", "Maître d'ouvrage : le client qui exprime le besoin."], ["MOE ?", "Maître d'œuvre : celui qui réalise."], ["SMART ?", "Spécifique, Mesurable, Atteignable, Réaliste, Temporellement défini."], ["RACI ?", "Réalisateur, Approbateur, Consulté, Informé."], ["OTP / WBS ?", "Découpage du projet en lots et tâches."]],
      quiz: [
        ["Le client qui commande le projet est le :", ["Maître d'œuvre", "Maître d'ouvrage", "Sous-traitant", "Fournisseur"], 1, "MOA = celui pour qui on fait le projet."],
        ["Dans SMART, le M signifie :", ["Motivant", "Mesurable", "Modeste", "Moderne"], 1, "On doit pouvoir mesurer l'atteinte."],
        ["La criticité d'un risque se calcule souvent par :", ["Probabilité × Gravité", "Coût + Délai", "Gravité − Probabilité", "Durée × Ressources"], 0, "Plus elle est élevée, plus le risque est prioritaire."],
        ["La dernière phase d'un projet :", ["Initialisation", "Clôture avec bilan / REX", "Planification", "Réalisation"], 1, "On capitalise les leçons apprises."]
      ]
    },
    {
      id: "planning", t: "Planification : Gantt, PERT, chemin critique", y: 1,
      html: `
<h3>Les antériorités</h3><p>Pour chaque tâche on note sa <b>durée</b> et les tâches qui doivent être finies avant (antériorités). On construit un tableau, puis on calcule les niveaux.</p>
<h3>Le diagramme de Gantt</h3>
<div class="easy box">Un tableau avec les tâches en lignes et le temps en colonnes ; chaque tâche est une barre. Très lisible pour suivre l'avancement et voir qui fait quoi quand.</div>
<h3>Le réseau PERT (ou méthode des potentiels)</h3>
<p>Graphe qui montre l'enchaînement logique des tâches. Pour chaque tâche on calcule :</p>
<ul><li><b>Date au plus tôt</b> (début) : max des fins au plus tôt des tâches précédentes (on part du début, on avance).</li><li><b>Date au plus tard</b> : on part de la fin et on remonte (min des débuts au plus tard des suivantes).</li><li><b>Marge totale</b> = date au plus tard − date au plus tôt.</li></ul>
<div class="key box">Le <b>chemin critique</b> est la suite de tâches avec une marge nulle. C'est le chemin le plus long : il fixe la durée totale du projet. Un retard sur une tâche critique retarde tout le projet !</div>
<h3>Exemple</h3>
<table><tr><th>Tâche</th><th>Durée (j)</th><th>Antériorité</th></tr><tr><td>A</td><td>3</td><td>–</td></tr><tr><td>B</td><td>5</td><td>A</td></tr><tr><td>C</td><td>2</td><td>A</td></tr><tr><td>D</td><td>4</td><td>B, C</td></tr></table>
<div class="ex box">Au plus tôt : A 0→3 ; B 3→8 ; C 3→5 ; D commence à max(8, 5) = 8 → 12. Durée totale = 12 j. Chemin critique A–B–D. C peut finir au plus tard à 8 : marge de C = 8 − 5 = 3 jours.</div>
<h3>Marge libre</h3><p>Retard possible sans décaler la date au plus tôt de la tâche suivante.</p>
<h3>Ressources : lissage et nivellement</h3><p>Si une même personne est prévue sur deux tâches en même temps, on décale une tâche en utilisant sa marge (lissage) pour éviter la surcharge.</p>
<h3>Logiciels</h3><p>MS Project, GanttProject, ProjectLibre, Planner, ou un tableur.</p>`,
      cards: [["Chemin critique ?", "Suite de tâches à marge nulle : fixe la durée du projet."], ["Marge totale ?", "Date au plus tard − date au plus tôt."], ["Calcul des dates au plus tôt ?", "On part du début, on prend le MAX des fins des tâches précédentes."], ["Calcul des dates au plus tard ?", "On part de la fin, on prend le MIN des débuts des suivantes."], ["Avantage du Gantt ?", "Visualisation simple dans le temps et suivi d'avancement."]],
      quiz: [
        ["Une tâche du chemin critique a une marge :", ["Maximale", "Nulle", "Négative", "De 1 jour"], 1, "Tout retard décale la fin du projet."],
        ["A (2 j) puis B (4 j) et C (6 j) en parallèle après A, puis D (1 j) après B et C. Durée totale :", ["13 j", "9 j", "7 j", "8 j"], 1, "2 + max(4, 6) + 1 = 9 j."],
        ["Dans l'exemple précédent, la marge de B est :", ["0 j", "2 j", "4 j", "6 j"], 1, "B peut durer jusqu'à 6 j sans retarder D."],
        ["Pour calculer la date au plus tôt d'une tâche à plusieurs antécédents, on prend :", ["La plus petite fin", "La plus grande fin", "La moyenne", "La somme"], 1, "Il faut attendre que TOUTES soient finies."]
      ]
    },
    {
      id: "couts", t: "Économie : coûts, devis, rentabilité", y: 2,
      html: `
<h3>Les types de coûts</h3>
<ul><li><b>Coûts directs</b> : attribuables directement à un produit/projet (matière, main-d'œuvre directe, sous-traitance).</li><li><b>Coûts indirects</b> : partagés (loyer, administration, énergie générale) → répartis par clés de répartition.</li><li><b>Coûts fixes</b> : ne varient pas avec la quantité (loyer, amortissement). <b>Coûts variables</b> : proportionnels à la quantité (matière).</li></ul>
<h3>Construire un coût de revient</h3>
<div class="f">Coût de revient = matières + main-d'œuvre (heures × taux horaire) + frais machine + frais indirects</div>
<div class="f">Prix de vente HT = coût de revient + marge &nbsp;&nbsp; TTC = HT × 1,20 (TVA 20 %)</div>
<div class="ex box">Pièce : matière 12 €, 0,5 h d'usinage à 60 €/h = 30 €, frais indirects 20 % des coûts directs = 8,40 €. Coût de revient = 50,40 €. Avec 25 % de marge : PV HT = 63 € ; TTC = 75,60 €.</div>
<h3>Seuil de rentabilité</h3>
<div class="f">Marge sur coût variable (MCV) = Prix de vente − coût variable unitaire &nbsp;&nbsp; Seuil (quantité) = Coûts fixes / MCV unitaire</div>
<div class="ex box">Coûts fixes 30 000 €/an, PV 50 €, coût variable 30 € → seuil = 30 000/20 = 1 500 pièces.</div>
<h3>Investissement et retour sur investissement</h3>
<div class="f">Temps de retour (ROI simple) = Investissement / Gain annuel</div>
<div class="ex box">Nouveau robot : 80 000 €, il fait économiser 32 000 €/an → retour en 2,5 ans.</div>
<p>Méthodes plus fines : VAN (valeur actuelle nette, en actualisant les gains futurs), TRI.</p>
<h3>Amortissement</h3><p>Un équipement perd de la valeur : amortissement linéaire = valeur d'achat / durée (ex : 50 000 € sur 5 ans → 10 000 €/an).</p>
<h3>Comparer des solutions</h3><p>Matrice de choix multicritères : critères pondérés (coût, performance, délai, maintenance, sécurité…), note de chaque solution, total pondéré → choix justifié. Coût global (TCO) = achat + installation + exploitation + maintenance + fin de vie.</p>
<h3>Achats</h3><p>Cahier des charges → consultation de plusieurs fournisseurs (appel d'offres) → comparaison des devis → négociation → bon de commande → réception → facture.</p>`,
      cards: [["Coût fixe ?", "Ne varie pas avec la quantité produite."], ["Seuil de rentabilité (quantité) ?", "Coûts fixes / marge sur coût variable unitaire."], ["Temps de retour sur investissement ?", "Investissement / gain annuel."], ["Prix TTC avec TVA 20 % ?", "HT × 1,2"], ["Amortissement linéaire ?", "Valeur d'achat / durée d'utilisation."], ["TCO ?", "Coût global de possession sur toute la durée de vie."]],
      quiz: [
        ["Machine à 60 000 € qui économise 15 000 €/an. Retour en :", ["2 ans", "4 ans", "6 ans", "0,25 an"], 1, "60 000/15 000."],
        ["Le loyer de l'atelier est un coût :", ["Variable", "Fixe", "Direct de matière", "Nul"], 1, "Il ne dépend pas de la production."],
        ["Coûts fixes 10 000 €, PV 25 €, coût variable 15 €. Seuil :", ["400 pièces", "1 000 pièces", "667 pièces", "250 pièces"], 1, "10 000/10."],
        ["Prix HT 200 €, TVA 20 %. TTC =", ["220 €", "240 €", "160 €", "400 €"], 1, "200 × 1,2."]
      ]
    },
    {
      id: "qualite", t: "Qualité : outils et démarches", y: 1,
      html: `
<h3>La qualité, c'est quoi ?</h3>
<div class="easy box">La qualité = l'aptitude à satisfaire les besoins du client, <b>du premier coup</b>. Ne pas faire de qualité coûte cher : rebuts, retouches, retours clients, image de marque.</div>
<h3>La roue de Deming : PDCA</h3>
<p><b>P</b>lan (planifier) → <b>D</b>o (réaliser) → <b>C</b>heck (vérifier) → <b>A</b>ct (agir, ajuster, standardiser) → et on recommence : amélioration continue.</p>
<h3>Les outils de résolution de problèmes</h3>
<table><tr><th>Outil</th><th>À quoi il sert</th></tr>
<tr><td><b>QQOQCCP</b></td><td>Décrire un problème : Qui ? Quoi ? Où ? Quand ? Comment ? Combien ? Pourquoi ?</td></tr>
<tr><td><b>Brainstorming</b></td><td>Trouver un maximum d'idées en groupe (sans critiquer)</td></tr>
<tr><td><b>Ishikawa (5M)</b></td><td>Diagramme en arête de poisson : classer les causes en Matière, Matériel (Machine), Méthode, Main-d'œuvre, Milieu (+ Mesure, Management)</td></tr>
<tr><td><b>5 Pourquoi</b></td><td>Demander « pourquoi ? » 5 fois pour trouver la cause racine</td></tr>
<tr><td><b>Pareto (80/20)</b></td><td>Classer les causes par importance : 20 % des causes font 80 % des effets → traiter d'abord celles-là</td></tr>
<tr><td><b>Feuille de relevé / histogramme</b></td><td>Collecter et visualiser des données</td></tr>
<tr><td><b>Carte de contrôle</b></td><td>Surveiller un procédé dans le temps (limites de contrôle)</td></tr>
<tr><td><b>Vote pondéré / matrice de décision</b></td><td>Choisir une solution</td></tr></table>
<h3>Méthode de résolution de problème (type 8D)</h3><ol><li>Former l'équipe.</li><li>Décrire le problème (QQOQCCP).</li><li>Actions immédiates (protéger le client).</li><li>Chercher les causes racines (Ishikawa, 5 Pourquoi).</li><li>Choisir les actions correctives.</li><li>Les mettre en œuvre et vérifier.</li><li>Prévenir la récidive (standardiser).</li><li>Féliciter l'équipe.</li></ol>
<h3>Normes et certification</h3>
<ul><li><b>ISO 9001</b> : système de management de la qualité (approche processus, satisfaction client, amélioration continue).</li><li><b>ISO 14001</b> : environnement. <b>ISO 45001</b> : santé-sécurité au travail. <b>ISO 50001</b> : énergie.</li><li>IATF 16949 (automobile), EN 9100 (aéronautique).</li></ul>
<h3>Contrôle et capabilité</h3>
<div class="f">Cp = IT / (6σ) &nbsp;&nbsp; Cpk = min(Tsup − x̄ ; x̄ − Tinf) / (3σ)</div>
<p>Un procédé est dit <b>capable</b> si Cp et Cpk ≥ 1,33 (valeur courante en industrie). Cpk &lt; Cp signifie que le procédé est décentré → il faut le régler.</p>
<h3>Coût de la non-qualité</h3><p>Rebuts, retouches, garanties, pénalités, temps perdu… à comparer aux coûts de prévention et de détection.</p>`,
      cards: [["PDCA ?", "Plan, Do, Check, Act (roue de Deming)."], ["Les 5M ?", "Matière, Matériel, Méthode, Main-d'œuvre, Milieu."], ["Loi de Pareto ?", "20 % des causes produisent 80 % des effets."], ["QQOQCCP ?", "Qui, Quoi, Où, Quand, Comment, Combien, Pourquoi."], ["Cp ?", "IT / 6σ"], ["Procédé capable si ?", "Cp et Cpk ≥ 1,33 (généralement)."], ["ISO 9001 ?", "Norme du management de la qualité."]],
      quiz: [
        ["Le diagramme d'Ishikawa sert à :", ["Planifier un projet", "Classer les causes d'un problème", "Calculer un coût", "Dessiner une pièce"], 1, "Diagramme causes-effet en 5M."],
        ["Le « C » de PDCA signifie :", ["Créer", "Check (vérifier)", "Coût", "Client"], 1, "On vérifie les résultats."],
        ["IT = 0,6 mm, σ = 0,05 mm. Cp =", ["2", "1,2", "0,5", "12"], 0, "0,6/(6 × 0,05) = 2."],
        ["Cpk nettement inférieur à Cp signifie :", ["Procédé parfait", "Procédé décentré", "Mesure fausse", "IT trop grand"], 1, "La moyenne est éloignée du centre de la tolérance."],
        ["Pour trouver la cause racine on peut utiliser :", ["Les 5 Pourquoi", "Le Gantt", "Le PERT", "La TVA"], 0, "On creuse jusqu'à l'origine."],
        ["La norme ISO 14001 concerne :", ["La qualité", "L'environnement", "La sécurité", "L'énergie"], 1, "Management environnemental."]
      ]
    },
    {
      id: "amdec", t: "AMDEC et sûreté de fonctionnement", y: 2,
      html: `
<h3>L'AMDEC</h3>
<div class="easy box">Analyse des Modes de Défaillance, de leurs Effets et de leur Criticité. On se demande, composant par composant : « <b>Comment peut-il tomber en panne ? Qu'est-ce que ça provoque ? Pourquoi ? Est-ce grave ?</b> » pour agir <b>avant</b> que ça arrive.</div>
<h3>Le tableau AMDEC</h3>
<table><tr><th>Élément</th><th>Fonction</th><th>Mode de défaillance</th><th>Cause</th><th>Effet</th><th>Détection</th><th>F</th><th>G</th><th>D</th><th>C</th><th>Action</th></tr>
<tr><td>Courroie</td><td>Transmettre</td><td>Rupture</td><td>Usure, tension</td><td>Arrêt ligne</td><td>Aucune</td><td>3</td><td>4</td><td>3</td><td>36</td><td>Contrôle tension mensuel</td></tr></table>
<div class="f">Criticité C = F (fréquence) × G (gravité) × D (non-détection)</div>
<p>Chaque note est souvent de 1 à 4 (ou 1 à 10). Au-dessus d'un seuil fixé, on doit mettre en place des actions, puis recalculer la criticité.</p>
<p>Types : AMDEC produit (conception), AMDEC processus (fabrication), AMDEC moyen (machine, maintenance).</p>
<h3>Sûreté de fonctionnement (FMDS)</h3>
<ul><li><b>Fiabilité</b> : probabilité de fonctionner sans panne pendant un temps donné. MTBF = temps moyen de bon fonctionnement entre défaillances.</li><li><b>Maintenabilité</b> : facilité à réparer. MTTR = temps moyen de réparation.</li><li><b>Disponibilité</b> : D = MTBF / (MTBF + MTTR).</li><li><b>Sécurité</b> : ne pas blesser / endommager.</li></ul>
<div class="ex box">MTBF = 190 h, MTTR = 10 h → D = 190/200 = 95 %.</div>
<h3>Courbe en baignoire</h3><p>Taux de défaillance : élevé au début (défauts de jeunesse), constant pendant la vie utile (pannes aléatoires), puis augmente (usure/vieillesse).</p>
<h3>Arbre de défaillances (ADD)</h3><p>On part de l'événement redouté (en haut) et on descend vers les causes, avec des portes ET / OU.</p>
<h3>Redondance</h3><p>Deux éléments en parallèle : le système fonctionne si au moins un fonctionne. R_parallèle = 1 − (1 − R₁)(1 − R₂). En série : R = R₁ × R₂.</p>`,
      cards: [["AMDEC ?", "Analyse des Modes de Défaillance, de leurs Effets et de leur Criticité."], ["Criticité AMDEC ?", "C = F × G × D"], ["Disponibilité ?", "D = MTBF / (MTBF + MTTR)"], ["MTTR ?", "Temps moyen de réparation."], ["Fiabilité de 2 éléments en série ?", "R = R₁ × R₂"], ["Fiabilité de 2 éléments en parallèle ?", "1 − (1−R₁)(1−R₂)"]],
      quiz: [
        ["F = 2, G = 4, D = 3. Criticité =", ["9", "24", "11", "6"], 1, "2 × 4 × 3."],
        ["MTBF = 450 h, MTTR = 50 h. Disponibilité =", ["90 %", "10 %", "95 %", "45 %"], 0, "450/500."],
        ["Deux composants de fiabilité 0,9 en série :", ["0,81", "0,99", "1,8", "0,9"], 0, "0,9 × 0,9."],
        ["Les mêmes en parallèle (redondance) :", ["0,81", "0,99", "0,9", "1"], 1, "1 − 0,1 × 0,1."],
        ["Dans l'AMDEC, la note D concerne :", ["La durée", "La non-détection", "Le danger", "Le délai"], 1, "Plus c'est difficile à détecter, plus D est grand."]
      ]
    },
    {
      id: "sse", t: "Santé, sécurité, environnement (SSE)", y: 1,
      html: `
<h3>Danger, risque, prévention</h3>
<div class="easy box"><b>Danger</b> = ce qui peut faire mal (une lame, l'électricité). <b>Risque</b> = la probabilité que le danger fasse mal à quelqu'un d'exposé. Si personne n'est exposé, il n'y a pas de risque.</div>
<p>Processus d'apparition d'un dommage : Danger + Personne → Situation dangereuse → Événement déclencheur → Dommage.</p>
<h3>Les 9 principes généraux de prévention (Code du travail)</h3>
<ol><li>Éviter les risques.</li><li>Évaluer ceux qu'on ne peut pas éviter.</li><li>Combattre les risques à la source.</li><li>Adapter le travail à l'homme (ergonomie).</li><li>Tenir compte de l'évolution technique.</li><li>Remplacer ce qui est dangereux par ce qui l'est moins.</li><li>Planifier la prévention.</li><li>Donner la priorité à la protection <b>collective</b> sur la protection individuelle.</li><li>Donner les instructions appropriées (former, informer).</li></ol>
<h3>Le Document Unique (DUERP)</h3><p>Obligatoire dans toute entreprise : il liste les risques de chaque unité de travail, leur évaluation et le plan d'actions. Mis à jour au moins une fois par an (selon les cas) et à chaque changement important.</p>
<h3>Protections</h3>
<ul><li><b>Collectives</b> : carters, barrières immatérielles, garde-corps, aspiration, capotage acoustique.</li><li><b>Individuelles (EPI)</b> : casque, lunettes, gants, chaussures de sécurité, bouchons d'oreilles, harnais.</li></ul>
<h3>Sécurité des machines</h3>
<ul><li>Directive Machines (et nouveau Règlement Machines européen) : marquage <b>CE</b>, déclaration de conformité, notice.</li><li>Analyse de risques (ISO 12100) : identifier les phénomènes dangereux, estimer, réduire (conception sûre → protecteurs → information).</li><li>Fonctions de sécurité : arrêt d'urgence (bouton coup de poing rouge sur fond jaune), interverrouillage de porte, commande bimanuelle, barrières immatérielles, relais de sécurité / automate de sécurité. Niveaux de performance PL a → e (ISO 13849).</li><li>Consignation des énergies (électrique, pneumatique, hydraulique, mécanique) avant intervention.</li></ul>
<h3>Pictogrammes</h3><p>Interdiction (rond rouge barré), obligation (rond bleu), danger (triangle jaune), secours (carré vert), incendie (rouge). Produits chimiques : pictogrammes SGH en losange rouge + FDS (fiche de données de sécurité).</p>
<h3>Environnement</h3><ul><li>Tri et traçabilité des déchets (DIB, déchets dangereux : huiles, solvants), registre des déchets.</li><li>ICPE (installations classées) pour les sites à risque.</li><li>Économies d'énergie (variateurs, chasse aux fuites d'air, LED), bilan carbone.</li></ul>`,
      cards: [["Danger vs risque ?", "Danger = ce qui peut nuire ; risque = probabilité de dommage pour une personne exposée."], ["DUERP ?", "Document Unique d'Évaluation des Risques Professionnels."], ["Priorité : protection collective ou individuelle ?", "Collective."], ["Norme d'analyse de risque machine ?", "ISO 12100"], ["Couleur d'un panneau d'obligation ?", "Rond bleu."], ["FDS ?", "Fiche de Données de Sécurité d'un produit chimique."]],
      quiz: [
        ["Entre un carter et des gants, on privilégie :", ["Les gants", "Le carter (protection collective)", "Aucun", "Les deux sont équivalents"], 1, "Principe n°8."],
        ["Le Document Unique est :", ["Facultatif", "Obligatoire", "Réservé aux grandes entreprises", "Une fiche de paie"], 1, "Obligatoire dès le 1er salarié."],
        ["Un bouton d'arrêt d'urgence est :", ["Vert", "Rouge sur fond jaune", "Bleu", "Noir"], 1, "Coup de poing rouge sur fond jaune."],
        ["Un panneau triangulaire jaune signale :", ["Une obligation", "Un danger", "Un secours", "Une interdiction"], 1, "Avertissement de danger."],
        ["Le marquage CE d'une machine signifie :", ["Fabriquée en Europe", "Conforme aux exigences européennes de sécurité", "Certifiée ISO 9001", "Garantie 2 ans"], 1, "Conformité réglementaire."]
      ]
    },
    {
      id: "maintenance", t: "Maintenance industrielle", y: 2,
      html: `
<h3>Définition</h3><p>Ensemble des actions permettant de maintenir ou rétablir un bien dans un état lui permettant d'accomplir sa fonction (norme NF EN 13306).</p>
<h3>Les types de maintenance</h3>
<table><tr><th>Type</th><th>Principe</th><th>Exemple</th></tr>
<tr><td><b>Corrective palliative</b></td><td>Dépannage provisoire</td><td>Réparation rapide pour relancer</td></tr>
<tr><td><b>Corrective curative</b></td><td>Réparation définitive après panne</td><td>Changer le moteur grillé</td></tr>
<tr><td><b>Préventive systématique</b></td><td>À intervalles fixes (temps, cycles)</td><td>Vidange toutes les 2 000 h</td></tr>
<tr><td><b>Préventive conditionnelle</b></td><td>Selon l'état mesuré (seuil)</td><td>Changer le roulement si vibrations &gt; seuil</td></tr>
<tr><td><b>Préventive prévisionnelle</b></td><td>On prévoit la date de panne (tendance)</td><td>Analyse de tendance des vibrations, IA</td></tr></table>
<h3>Les 5 niveaux de maintenance</h3><ol><li>Réglages simples par l'opérateur (sans outil spécial).</li><li>Opérations simples par un technicien habilité (échange standard).</li><li>Diagnostic et réparation de pannes, par un technicien spécialisé.</li><li>Travaux importants (sauf reconstruction), équipe spécialisée.</li><li>Rénovation, reconstruction (constructeur ou atelier central).</li></ol>
<h3>Indicateurs</h3>
<ul><li>MTBF, MTTR, disponibilité (voir AMDEC).</li><li><b>TRS</b> (taux de rendement synthétique) = Disponibilité × Performance × Qualité. Un TRS de 85 % est considéré comme très bon (« world class »).</li></ul>
<div class="ex box">Disponibilité 90 %, performance 95 %, qualité 98 % → TRS = 0,9 × 0,95 × 0,98 ≈ 84 %.</div>
<h3>Techniques de surveillance (conditionnelle)</h3><p>Analyse vibratoire, thermographie infrarouge, analyse d'huile, ultrasons, mesure d'isolement, endoscopie, mesure d'épaisseur.</p>
<h3>Méthode de diagnostic</h3><ol><li>Recueillir les symptômes (opérateur, historique, alarmes).</li><li>Formuler des hypothèses (5M, schémas).</li><li>Tester les hypothèses (mesures), de la plus probable / plus facile à la moins.</li><li>Réparer, essayer, remettre en service.</li><li>Rédiger le compte rendu (GMAO) pour l'historique.</li></ol>
<h3>GMAO</h3><p>Gestion de Maintenance Assistée par Ordinateur : historique des interventions, plans préventifs, stock de pièces, bons de travail, indicateurs.</p>
<h3>TPM (Total Productive Maintenance)</h3><p>Impliquer tout le monde ; l'opérateur fait la maintenance de 1er niveau (nettoyage, inspection, graissage) : <b>maintenance autonome</b>.</p>`,
      cards: [["Préventive systématique ?", "Intervention à intervalle fixe (temps, nb de cycles)."], ["Préventive conditionnelle ?", "Intervention selon l'état mesuré (seuil)."], ["TRS ?", "Disponibilité × Performance × Qualité"], ["GMAO ?", "Gestion de Maintenance Assistée par Ordinateur."], ["TPM ?", "Total Productive Maintenance : maintenance impliquant tous, dont l'opérateur."], ["Niveau 1 de maintenance ?", "Réglages simples par l'opérateur."]],
      quiz: [
        ["Changer un filtre toutes les 500 h, c'est de la maintenance :", ["Corrective", "Préventive systématique", "Conditionnelle", "Palliative"], 1, "Intervalle fixe."],
        ["Changer un roulement quand ses vibrations dépassent un seuil :", ["Systématique", "Conditionnelle", "Curative", "Palliative"], 1, "Selon l'état mesuré."],
        ["D = 0,8 ; P = 0,9 ; Q = 1. TRS =", ["72 %", "90 %", "80 %", "270 %"], 0, "0,8 × 0,9 × 1."],
        ["La thermographie infrarouge permet de détecter :", ["Des échauffements anormaux", "Des fuites d'huile seulement", "Le bruit", "La couleur"], 0, "Connexions électriques desserrées, roulements…"]
      ]
    },
    {
      id: "lean", t: "Organisation industrielle et Lean", y: 2,
      html: `
<h3>Le Lean manufacturing</h3>
<div class="easy box">Le Lean (inspiré de Toyota) consiste à <b>éliminer tout ce qui n'apporte pas de valeur au client</b> (les gaspillages) pour produire mieux, plus vite et moins cher.</div>
<h3>Les 7 (+1) gaspillages (Muda)</h3>
<ol><li>Surproduction</li><li>Attentes</li><li>Transports inutiles</li><li>Traitements inutiles (sur-qualité)</li><li>Stocks</li><li>Mouvements inutiles</li><li>Défauts (rebuts, retouches)</li><li>(+ sous-utilisation des compétences)</li></ol>
<h3>Les outils</h3>
<table><tr><th>Outil</th><th>Principe</th></tr>
<tr><td><b>5S</b></td><td>Seiri (trier), Seiton (ranger), Seiso (nettoyer), Seiketsu (standardiser), Shitsuke (respecter / faire durer)</td></tr>
<tr><td><b>SMED</b></td><td>Réduire le temps de changement de série (transformer les opérations internes, machine arrêtée, en externes)</td></tr>
<tr><td><b>Kanban</b></td><td>Production tirée par la demande avec des étiquettes</td></tr>
<tr><td><b>Juste-à-temps</b></td><td>Produire la bonne quantité au bon moment</td></tr>
<tr><td><b>Poka-yoke</b></td><td>Détrompeur qui empêche l'erreur (une prise USB-C qui ne rentre que dans le bon sens)</td></tr>
<tr><td><b>VSM</b></td><td>Cartographie de la chaîne de valeur (flux de matière et d'information)</td></tr>
<tr><td><b>Kaizen</b></td><td>Amélioration continue par petits pas</td></tr>
<tr><td><b>Andon</b></td><td>Signal visuel (lumineux) d'un problème</td></tr>
<tr><td><b>Standard de travail</b></td><td>La meilleure façon connue de faire, écrite et affichée</td></tr></table>
<h3>Temps et cadences</h3>
<div class="f">Takt time = temps disponible / demande client</div>
<div class="ex box">7 h de travail par jour (25 200 s), 420 pièces demandées → takt = 60 s : il faut sortir une pièce par minute.</div>
<h3>Types de production</h3><ul><li>Unitaire / petite série (job shop) ; série ; masse (ligne) ; continue (process).</li><li>Production poussée (sur prévisions) ou tirée (sur commande).</li></ul>
<h3>Implantation d'atelier</h3><p>Méthode des chaînons, méthode des gammes : minimiser les distances et croisements de flux. Îlots de production, lignes en U.</p>
<h3>Gestion des stocks</h3><p>Stock de sécurité, point de commande, quantité économique de Wilson : Q = √(2·D·C_c / C_s), méthode ABC (Pareto appliqué aux articles).</p>`,
      cards: [["Les 5S ?", "Trier, Ranger, Nettoyer, Standardiser, Faire durer."], ["SMED ?", "Méthode de réduction du temps de changement de série."], ["Poka-yoke ?", "Détrompeur qui rend l'erreur impossible."], ["Takt time ?", "Temps disponible / demande client."], ["Kaizen ?", "Amélioration continue par petits pas."], ["Kanban ?", "Système de production tirée par étiquettes."]],
      quiz: [
        ["Un détrompeur qui empêche le mauvais montage est un :", ["Kanban", "Poka-yoke", "Andon", "Gantt"], 1, "Poka-yoke = anti-erreur."],
        ["8 h dispo (28 800 s) pour 480 pièces. Takt time :", ["60 s", "30 s", "120 s", "6 s"], 0, "28 800/480."],
        ["Le SMED vise à réduire :", ["Les stocks", "Le temps de changement de série", "Les salaires", "La qualité"], 1, "Single Minute Exchange of Die."],
        ["Lequel n'est PAS un gaspillage du Lean ?", ["Surproduction", "Attente", "Valeur ajoutée pour le client", "Stock"], 2, "La valeur ajoutée est ce qu'on veut garder !"]
      ]
    },
    {
      id: "communication", t: "Communication technique et management", y: 0,
      html: `
<h3>Les écrits professionnels</h3>
<ul><li><b>Compte rendu de réunion</b> : date, participants, ordre du jour, décisions, actions (qui ? quoi ? pour quand ?).</li><li><b>Rapport technique</b> : contexte, analyse, solutions, recommandation chiffrée.</li><li><b>Mail professionnel</b> : objet clair, une idée par paragraphe, formule de politesse, pièces jointes nommées.</li><li><b>Procédure / mode opératoire</b> : étapes numérotées, verbes à l'infinitif, photos, points de sécurité.</li><li><b>Notice</b> : pour l'utilisateur, simple et illustrée.</li></ul>
<h3>Conduire une réunion</h3><ol><li>Préparer : objectif, ordre du jour, invités utiles, durée.</li><li>Animer : rappeler l'objectif, distribuer la parole, gérer le temps, reformuler.</li><li>Conclure : décisions + plan d'actions.</li><li>Diffuser le compte rendu rapidement.</li></ol>
<h3>Présentation orale</h3><p>Structure claire, visuels, chiffres clés, adapter le vocabulaire à l'auditoire (technique pour un ingénieur, simple pour un client).</p>
<h3>Travail en équipe et management</h3><ul><li>Écoute active, reformulation, feedback constructif (faits, impact, proposition).</li><li>Gestion des conflits : séparer la personne du problème, chercher l'intérêt commun.</li><li>Styles de management : directif, persuasif, participatif, délégatif (adapter à l'autonomie de la personne).</li></ul>
<h3>Relation client / fournisseur</h3><p>Écouter le besoin, reformuler, proposer, confirmer par écrit. Toujours tracer les échanges importants (mail, compte rendu).</p>
<h3>Outils numériques</h3><p>Tableur (formules, tableaux croisés, graphiques), traitement de texte (styles, sommaire automatique), diaporama, outils collaboratifs (Teams, SharePoint, Drive), gestion de projet.</p>`,
      cards: [["Contenu clé d'un compte rendu ?", "Participants, décisions, actions (qui/quoi/quand)."], ["Règle d'écriture d'un mode opératoire ?", "Étapes numérotées, verbes à l'infinitif, sécurité, visuels."], ["Feedback constructif ?", "Faits → impact → proposition."]],
      quiz: [
        ["Un compte rendu de réunion doit surtout contenir :", ["Les blagues", "Les décisions et actions avec responsables et délais", "La météo", "Le menu"], 1, "C'est ce qui fait avancer le projet."],
        ["Un mode opératoire est rédigé avec :", ["Des verbes à l'infinitif et des étapes numérotées", "Un long texte sans étapes", "Uniquement des photos floues", "Des abréviations inconnues"], 0, "Clair et actionnable."]
      ]
    }
  ]
});
