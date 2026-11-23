// Base de données des cours. Chaque fichier ajoute une matière avec COURS.push({...}).
// Chapitre : { id, t: titre, y: 1|2|0 (année, 0 = les deux), html, cards: [[recto, verso]], quiz: [[question, [choix], indexBonneRéponse, explication]] }
var COURS = [];

COURS.push({
  id: "bts", name: "Le BTS ATI & méthode", emoji: "🧭", color: "#0EA5E9",
  desc: "Comprendre le diplôme, son organisation sur 2 ans, les épreuves, le stage et les méthodes pour réviser efficacement. Commence par ici !",
  chapters: [
    {
      id: "presentation", t: "C'est quoi le BTS ATI ?", y: 0,
      html: `
<h3>Le BTS ATI en une phrase</h3>
<div class="easy box">Le technicien ATI (Assistance Technique d'Ingénieur) est le « bras droit » de l'ingénieur : il comprend un système technique dans son ensemble (mécanique + électricité + automatisme + informatique), il organise, il chiffre, il teste, il rédige et il fait le lien entre le bureau d'études, la production, la maintenance, les clients et les fournisseurs.</div>
<p>C'est un BTS <b>pluritechnique</b> : on ne devient pas spécialiste d'un seul domaine, on devient capable de <b>tout comprendre un peu</b> et de <b>coordonner</b>. C'est pour ça que tu as beaucoup de matières différentes.</p>
<h3>Les grands domaines étudiés</h3>
<ul>
<li><b>Enseignement général</b> : Culture générale et expression, Anglais, Mathématiques, Physique-chimie.</li>
<li><b>Enseignement technique</b> : mécanique (statique, cinématique, RDM), électrotechnique, automatisme (GRAFCET, API), pneumatique, hydraulique, analyse fonctionnelle (SysML), dessin technique / CAO, informatique industrielle et réseaux.</li>
<li><b>Organisation / gestion</b> : gestion de projet, planification, coûts, qualité, sécurité, maintenance, communication technique.</li>
<li><b>Professionnel</b> : stage en entreprise (fin de 1ʳᵉ année en général), projet technique en 2ᵉ année.</li>
</ul>
<h3>Que fait un technicien ATI au travail ?</h3>
<table><tr><th>Mission</th><th>Exemple concret</th></tr>
<tr><td>Analyser un système</td><td>Comprendre comment fonctionne une ligne de conditionnement et trouver pourquoi elle ralentit.</td></tr>
<tr><td>Améliorer / modifier</td><td>Ajouter un capteur et modifier le programme de l'automate pour éviter un bourrage.</td></tr>
<tr><td>Gérer un projet</td><td>Planifier l'installation d'une nouvelle machine, suivre le budget, coordonner les fournisseurs.</td></tr>
<tr><td>Qualité / sécurité</td><td>Faire une AMDEC, rédiger une procédure, vérifier la conformité d'une machine.</td></tr>
<tr><td>Assistance technique</td><td>Rédiger une notice, former un opérateur, répondre à un client.</td></tr>
<tr><td>Essais / mesures</td><td>Mesurer un courant moteur, un débit, une température, et interpréter les résultats.</td></tr></table>
<h3>Et après ?</h3>
<p>Emplois : technicien méthodes, technicien bureau d'études, chargé d'affaires, technicien maintenance, assistant chef de projet, technicien qualité, technico-commercial. Poursuites : licence pro, prépa ATS, école d'ingénieurs (souvent en alternance).</p>
<div class="key box">Ce qu'on attend de toi : <b>comprendre, expliquer, organiser</b>. Les jurys aiment les candidats qui savent justifier leurs choix avec des chiffres et un vocabulaire précis.</div>`,
      cards: [["Que veut dire ATI ?", "Assistance Technique d'Ingénieur"], ["Pourquoi dit-on que le BTS ATI est « pluritechnique » ?", "Parce qu'il mélange mécanique, électricité, automatisme, informatique et gestion de projet."], ["Rôle principal du technicien ATI ?", "Assister l'ingénieur : analyser, organiser, chiffrer, tester, communiquer et coordonner."]],
      quiz: [
        ["Le technicien ATI est surtout :", ["Un spécialiste d'un seul domaine", "Un technicien polyvalent qui coordonne", "Un commercial uniquement", "Un ouvrier de production"], 1, "Le BTS ATI forme des profils polyvalents capables de faire le lien entre tous les services."],
        ["Lequel n'est PAS un domaine du BTS ATI ?", ["Automatisme", "Mécanique", "Médecine", "Gestion de projet"], 2, "La médecine n'a rien à voir ; tout le reste fait partie de la formation."],
        ["Une poursuite d'études possible après le BTS ATI :", ["Prépa ATS", "Médecine", "Droit uniquement", "Aucune"], 0, "La prépa ATS (adaptation technicien supérieur) permet d'intégrer une école d'ingénieurs."]
      ]
    },
    {
      id: "examen", t: "Les épreuves de l'examen", y: 0,
      html: `
<h3>Le principe</h3>
<p>L'examen du BTS est composé de plusieurs <b>épreuves</b> (E1, E2, …), chacune avec un <b>coefficient</b>. Certaines sont des écrits ponctuels en fin de 2ᵉ année, d'autres peuvent être en <b>CCF</b> (contrôle en cours de formation) selon ton statut (scolaire, apprenti, formation continue…). Il faut <b>10/20 de moyenne générale</b> pour l'obtenir.</p>
<div class="warn box">Les coefficients, durées et modalités exactes peuvent changer selon la session et ton statut. <b>Demande toujours la grille officielle à tes professeurs</b> ou consulte le référentiel du BTS ATI.</div>
<h3>Ce qu'on retrouve en général</h3>
<table>
<tr><th>Épreuve</th><th>Contenu</th><th>Conseil</th></tr>
<tr><td>Culture générale et expression</td><td>Synthèse de documents + écriture personnelle sur un des deux thèmes de l'année.</td><td>Lis les œuvres des thèmes, entraîne-toi au plan.</td></tr>
<tr><td>Anglais</td><td>Compréhension écrite/orale, expression, souvent en lien avec le monde technique.</td><td>Apprends le vocabulaire technique (chapitre Anglais).</td></tr>
<tr><td>Mathématiques</td><td>Fonctions, dérivées, intégrales, équations différentielles, probas/stats…</td><td>Fais beaucoup d'exercices types.</td></tr>
<tr><td>Physique-chimie</td><td>Mécanique, électricité, énergie, thermique, chimie appliquée.</td><td>Connais les formules et les unités.</td></tr>
<tr><td>Épreuves techniques (étude de systèmes)</td><td>Analyse fonctionnelle, mécanique, électrotechnique, automatisme sur un dossier industriel.</td><td>Lis le dossier technique en entier avant de répondre.</td></tr>
<tr><td>Organisation / gestion de projet</td><td>Planning, coûts, qualité, sécurité, communication.</td><td>Maîtrise Gantt, PERT, AMDEC, calculs de coûts.</td></tr>
<tr><td>Projet / épreuve professionnelle</td><td>Soutenance orale d'un projet ou de l'activité en entreprise, avec un dossier.</td><td>Prépare un diaporama clair et chronomètre-toi.</td></tr>
</table>
<h3>Méthode pour un sujet technique (dossier industriel)</h3>
<ol>
<li><b>Survole tout le sujet</b> (10 min) : questions + dossier technique + documents réponses.</li>
<li>Repère les <b>parties indépendantes</b> : tu peux commencer par celle que tu maîtrises.</li>
<li><b>Souligne les données</b> (valeurs, unités) dans l'énoncé.</li>
<li>Pour chaque calcul : <b>formule littérale → application numérique → résultat avec unité</b>.</li>
<li>Conclus toujours : « La valeur trouvée est inférieure à…, donc le choix est validé. »</li>
<li>Garde 10 min à la fin pour vérifier les documents réponses (nom, numéro de page).</li>
</ol>
<div class="key box">Une bonne réponse sans unité ou sans conclusion perd des points. Une démarche juste avec une erreur de calcul en gagne quand même !</div>`,
      cards: [["Moyenne nécessaire pour obtenir le BTS ?", "10/20 de moyenne générale (avec les coefficients)."], ["Que veut dire CCF ?", "Contrôle en Cours de Formation."], ["Les 3 étapes d'un calcul bien rédigé ?", "Formule littérale → application numérique → résultat avec unité (+ conclusion)."]],
      quiz: [
        ["Pour avoir le BTS, il faut :", ["10/20 dans chaque matière", "10/20 de moyenne générale", "12/20 de moyenne", "Réussir uniquement le stage"], 1, "C'est la moyenne pondérée par les coefficients qui compte."],
        ["Dans un sujet technique, la première chose à faire :", ["Commencer la question 1 tout de suite", "Survoler tout le sujet et le dossier", "Remplir les documents réponses au hasard", "Lire seulement la dernière page"], 1, "Survoler permet de repérer les parties indépendantes et de gérer son temps."],
        ["Un résultat numérique doit toujours avoir :", ["Une couleur", "Une unité et une conclusion", "Trois décimales minimum", "Un dessin"], 1, "Unité + phrase de conclusion = points assurés."]
      ]
    },
    {
      id: "methode", t: "Bien réviser (méthode efficace)", y: 0,
      html: `
<h3>Les 5 règles d'or</h3>
<ol>
<li><b>Récupération active</b> : fermer le cours et se tester (quiz, fiches) est bien plus efficace que relire.</li>
<li><b>Répétition espacée</b> : revoir une notion le lendemain, puis 3 jours après, puis 1 semaine après.</li>
<li><b>Petites sessions</b> : 25 min de travail concentré + 5 min de pause (méthode Pomodoro).</li>
<li><b>Faire des exercices</b> : en maths, physique, méca, on apprend en faisant, pas en lisant.</li>
<li><b>Expliquer à quelqu'un</b> : si tu sais l'expliquer simplement, tu as compris.</li>
</ol>
<h3>Comment utiliser cette appli</h3>
<ul>
<li>Lis un chapitre (10–15 min), marque-le « appris ».</li>
<li>Fais son <b>quiz</b> : vise 80 % et plus.</li>
<li>Chaque jour : 10 min de <b>fiches</b> d'une matière.</li>
<li>Le week-end : un <b>quiz défi</b> mélangé.</li>
<li>L'onglet <b>Plus</b> te montre les chapitres à retravailler.</li>
</ul>
<h3>Planning type sur une semaine</h3>
<table><tr><th>Jour</th><th>Programme (≈ 45 min)</th></tr>
<tr><td>Lundi</td><td>Maths : 1 chapitre + exercices</td></tr>
<tr><td>Mardi</td><td>Mécanique / RDM</td></tr>
<tr><td>Mercredi</td><td>Électrotechnique + fiches</td></tr>
<tr><td>Jeudi</td><td>Automatisme / GRAFCET</td></tr>
<tr><td>Vendredi</td><td>Gestion de projet / qualité</td></tr>
<tr><td>Samedi</td><td>Anglais + culture générale</td></tr>
<tr><td>Dimanche</td><td>Quiz défi + repos 😴</td></tr></table>
<div class="tip box">Le sommeil fixe les connaissances. Réviser tard la veille de l'examen est moins efficace que bien dormir.</div>`,
      cards: [["Méthode la plus efficace pour mémoriser ?", "Se tester (récupération active) + répétition espacée."], ["Méthode Pomodoro ?", "25 min de travail concentré puis 5 min de pause."]],
      quiz: [
        ["Quelle méthode est la plus efficace pour retenir ?", ["Relire 5 fois", "Surligner tout", "Se tester sans regarder le cours", "Recopier le cours"], 2, "La récupération active (quiz, fiches) est prouvée comme la plus efficace."],
        ["La répétition espacée consiste à :", ["Tout réviser la veille", "Revoir une notion à intervalles croissants", "Réviser une seule fois", "Réviser toujours la même chose"], 1, "J+1, J+3, J+7… la mémoire se consolide à chaque rappel."]
      ]
    },
    {
      id: "stage", t: "Stage, rapport et soutenance orale", y: 0,
      html: `
<h3>Le stage</h3>
<p>Le stage (souvent entre la 1ʳᵉ et la 2ᵉ année) sert à découvrir le métier et à mener des <b>activités réelles</b> : étude technique, amélioration, mise en service, documentation, planification…</p>
<h4>Pendant le stage, note tout !</h4>
<ul><li>Organigramme de l'entreprise, produits, clients, chiffres clés.</li><li>Tes missions : problème de départ, ce que tu as fait, résultats chiffrés.</li><li>Photos (avec autorisation), schémas, plans, devis.</li></ul>
<h3>Plan type du rapport</h3>
<ol>
<li>Page de garde, remerciements, sommaire.</li>
<li><b>Introduction</b> : contexte, entreprise, problématique.</li>
<li><b>Présentation de l'entreprise</b> (courte !).</li>
<li><b>Analyse du besoin / de l'existant</b> (bête à cornes, diagramme des exigences…).</li>
<li><b>Solutions étudiées</b> et critères de choix (tableau comparatif).</li>
<li><b>Réalisation</b> : calculs, plans, programme, planning, coûts.</li>
<li><b>Résultats</b> : gains (temps, argent, sécurité, qualité).</li>
<li><b>Conclusion</b> : bilan technique + bilan personnel + perspectives.</li>
<li>Annexes.</li>
</ol>
<h3>Réussir la soutenance</h3>
<ul>
<li>1 diapo ≈ 1 minute. Peu de texte, des images, des schémas, des chiffres.</li>
<li>Structure : Problème → Démarche → Solution → Résultats → Bilan.</li>
<li>Regarde le jury, parle lentement, ne lis pas tes diapos.</li>
<li>Prépare les questions probables : « Pourquoi ce choix ? Combien ça coûte ? Et la sécurité ? ».</li>
<li>Si tu ne sais pas : « Je ne l'ai pas étudié, mais je procéderais ainsi… ».</li>
</ul>
<div class="key box">Le jury évalue ta <b>démarche</b> et ta capacité à <b>justifier</b>, pas seulement le résultat.</div>`,
      cards: [["Structure d'une soutenance ?", "Problème → Démarche → Solution → Résultats → Bilan."], ["Règle des diapos ?", "≈ 1 diapo par minute, peu de texte, des visuels et des chiffres."]],
      quiz: [
        ["Dans un rapport de stage, la présentation de l'entreprise doit être :", ["La partie la plus longue", "Courte et utile", "Absente", "Copiée du site web"], 1, "Le jury veut surtout voir TON travail technique."],
        ["Le jury te pose une question dont tu ne connais pas la réponse :", ["Tu inventes", "Tu restes muet", "Tu l'admets et proposes une démarche", "Tu changes de sujet"], 2, "L'honnêteté + une démarche logique sont très appréciées."]
      ]
    }
  ]
});
