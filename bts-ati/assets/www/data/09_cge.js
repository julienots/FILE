COURS.push({
  id: "cge", name: "Culture générale et expression", emoji: "📚", color: "#B45309",
  desc: "Méthode de la synthèse de documents, de l'écriture personnelle, de l'argumentation, outils d'analyse de documents (textes, images, graphiques) et conseils pour les thèmes de l'année.",
  chapters: [
    {
      id: "epreuve", t: "L'épreuve de culture générale", y: 0,
      html: `
<h3>Le principe</h3>
<p>Chaque année, <b>deux thèmes</b> sont fixés au niveau national pour la 2ᵉ année de BTS (ils changent : un nouveau thème chaque année, chacun étudié deux ans). Le sujet comporte un <b>corpus</b> de 3 ou 4 documents (textes, image, dessin, graphique) sur l'un des thèmes.</p>
<h3>Deux exercices</h3>
<ol><li><b>La synthèse de documents</b> (≈ 40 points sur 60 de barème) : une confrontation objective et organisée des documents.</li><li><b>L'écriture personnelle</b> (≈ 20 points) : ta réponse argumentée à une question liée au thème.</li></ol>
<div class="warn box">Vérifie avec ton professeur les thèmes de ta session et le barème exact : ils sont publiés au Bulletin officiel chaque année.</div>
<h3>Gestion du temps (épreuve de 4 h)</h3>
<table><tr><th>Étape</th><th>Temps</th></tr>
<tr><td>Lecture et analyse du corpus</td><td>≈ 45 min</td></tr><tr><td>Tableau de confrontation + plan</td><td>≈ 45 min</td></tr>
<tr><td>Rédaction de la synthèse</td><td>≈ 1 h</td></tr><tr><td>Écriture personnelle</td><td>≈ 1 h</td></tr><tr><td>Relecture (orthographe !)</td><td>≈ 15 min</td></tr></table>
<h3>Préparer les thèmes</h3><ul><li>Lis les œuvres et documents travaillés en cours.</li><li>Constitue un carnet d'exemples : livres, films, faits d'actualité, citations, expériences personnelles (stage !).</li><li>Apprends le vocabulaire du thème.</li></ul>`,
      cards: [["Les 2 exercices de l'épreuve ?", "Synthèse de documents + écriture personnelle."], ["Combien de thèmes par session ?", "Deux thèmes nationaux."]],
      quiz: [
        ["Dans la synthèse, on donne son avis :", ["Oui, partout", "Non, jamais", "Seulement en conclusion", "Seulement dans le titre"], 1, "La synthèse est objective ; l'avis va dans l'écriture personnelle."],
        ["L'écriture personnelle consiste à :", ["Résumer chaque document", "Répondre de façon argumentée à une question", "Recopier le corpus", "Faire un dessin"], 1, "On défend sa position avec des arguments et exemples."]
      ]
    },
    {
      id: "synthese", t: "Méthode de la synthèse de documents", y: 0,
      html: `
<h3>Ce qu'on attend</h3>
<div class="easy box">Une synthèse, c'est comme organiser un débat entre les auteurs : tu présentes leurs idées <b>ensemble</b>, en les <b>comparant</b> (accords, désaccords, nuances), selon un <b>plan</b> qui répond à une <b>problématique</b>. Tu ne donnes <b>jamais ton avis</b> et tu n'ajoutes <b>aucune information extérieure</b>.</div>
<h3>Étape 1 : lire et analyser</h3><ul><li>Pour chaque document : auteur, titre, date, nature (article, roman, essai, dessin de presse, graphique…), thème, thèse, arguments clés.</li><li>Surligne avec une couleur par idée commune.</li></ul>
<h3>Étape 2 : le tableau de confrontation</h3><p>En lignes, les idées ; en colonnes, les documents. On repère ce que chaque document dit sur chaque idée → les points communs et les différences sautent aux yeux.</p>
<h3>Étape 3 : la problématique et le plan</h3><p>La problématique est la question à laquelle <b>tous</b> les documents répondent. Le plan : 2 ou 3 parties, chacune avec 2 ou 3 sous-parties ; <b>chaque sous-partie confronte plusieurs documents</b>.</p>
<p>Plans possibles : constat / causes / solutions ; avantages / limites ; thèse / antithèse ; aspects (économique, social, éthique…).</p>
<h3>Étape 4 : rédiger</h3>
<ul><li><b>Introduction</b> : amorce (phrase d'accroche liée au thème), présentation rapide des documents (auteur, titre, nature, date), problématique, annonce du plan.</li><li><b>Développement</b> : parties séparées par une ligne, chaque paragraphe commence par l'idée directrice, puis confronte les documents avec des connecteurs.</li><li><b>Conclusion</b> : réponse à la problématique (bilan), <b>pas d'ouverture personnelle</b> (en général).</li></ul>
<h3>Connecteurs pour confronter</h3><p>De même, à l'instar de, tout comme, rejoint (accord) — en revanche, à l'inverse, alors que, contrairement à (opposition) — nuance, précise, complète, ajoute (nuance).</p>
<h3>Les erreurs à éviter</h3><ul><li>Faire un résumé document par document (« Le doc 1 dit… puis le doc 2 dit… ») : c'est la faute n°1.</li><li>Recopier des phrases (il faut reformuler ; une courte citation entre guillemets est possible).</li><li>Donner son avis (« je pense », « il est évident que »…).</li><li>Oublier un document.</li><li>Dépasser la longueur (souvent 2 à 3 pages environ).</li></ul>`,
      cards: [["Faute n°1 de la synthèse ?", "Analyser les documents un par un au lieu de les confronter."], ["Peut-on donner son avis dans la synthèse ?", "Non, jamais."], ["Contenu de l'introduction ?", "Amorce, présentation des documents, problématique, annonce du plan."], ["Outil pour organiser les idées ?", "Le tableau de confrontation."]],
      quiz: [
        ["Dans une synthèse, chaque sous-partie doit :", ["Parler d'un seul document", "Confronter plusieurs documents", "Donner mon avis", "Être une citation"], 1, "C'est le principe même de la synthèse."],
        ["La conclusion de la synthèse :", ["Donne mon opinion", "Fait le bilan et répond à la problématique", "Présente un nouveau document", "Est facultative"], 1, "Objective, en réponse à la question."],
        ["Un connecteur d'opposition :", ["De même", "En revanche", "Ainsi", "D'ailleurs"], 1, "Il marque une différence."],
        ["La présentation d'un document comprend :", ["Auteur, titre, nature, date", "Seulement le titre", "Mon avis", "Le nombre de mots"], 0, "C'est le minimum attendu."]
      ]
    },
    {
      id: "ecriture", t: "Méthode de l'écriture personnelle", y: 0,
      html: `
<h3>Comprendre la question</h3><p>Analyse chaque mot de la question. Reformule-la. Repère s'il s'agit d'une question fermée (oui/non → plan dialectique possible) ou ouverte (comment ? pourquoi ? → plan thématique).</p>
<h3>Construire l'argumentation</h3>
<div class="key box">Un paragraphe = un <b>argument</b> (idée) + une <b>explication</b> + un <b>exemple</b> précis (+ petite conclusion partielle). C'est la règle AEE.</div>
<h3>Types de plans</h3><ul><li><b>Dialectique</b> : oui… / mais… / (dépassement).</li><li><b>Thématique</b> : différents aspects de la question.</li><li><b>Analytique</b> : constat → causes → conséquences / solutions.</li></ul>
<h3>Les exemples</h3><p>Variés et précis : œuvres étudiées sur le thème, documents du corpus (sans en abuser), films, livres, actualité, histoire, expérience personnelle ou professionnelle (stage). Évite les exemples vagues (« beaucoup de gens pensent que… »).</p>
<h3>Structure</h3><ul><li>Introduction courte : amorce, question reformulée, annonce du plan.</li><li>Développement : 2 ou 3 parties, 2 ou 3 paragraphes chacune.</li><li>Conclusion : réponse claire + ouverture possible.</li></ul>
<h3>Ici tu peux (et dois) t'engager</h3><p>Tu donnes ton point de vue, de façon nuancée et argumentée. Le « je » est accepté avec modération.</p>`,
      cards: [["Règle d'un bon paragraphe argumentatif ?", "Argument + Explication + Exemple (AEE)."], ["Plan dialectique ?", "Oui… / mais… / dépassement."], ["Qualité d'un bon exemple ?", "Précis, varié, vérifiable."]],
      quiz: [
        ["Un argument sans exemple :", ["Est parfait", "Est faible", "Est interdit", "Est obligatoire"], 1, "L'exemple prouve l'argument."],
        ["Dans l'écriture personnelle, on peut :", ["Donner son avis argumenté", "Seulement résumer le corpus", "Ne rien conclure", "Écrire en langage SMS"], 0, "C'est le but de l'exercice."]
      ]
    },
    {
      id: "analyse", t: "Analyser textes, images et graphiques", y: 0,
      html: `
<h3>Les types de textes</h3><ul><li><b>Argumentatif</b> (essai, éditorial, tribune) : défend une thèse.</li><li><b>Narratif</b> (roman, nouvelle) : raconte ; l'idée est souvent implicite.</li><li><b>Informatif / explicatif</b> (article, rapport) : présente des faits.</li><li><b>Poétique / théâtral</b>.</li></ul>
<h3>Analyser une image (photo, dessin de presse, affiche)</h3><ol><li>Identifier : auteur, date, source, nature.</li><li>Décrire : plans, cadrage, couleurs, personnages, texte éventuel.</li><li>Interpréter : quel message ? quel lien avec le thème ? (humour, ironie, symbole).</li></ol>
<h3>Analyser un graphique ou un tableau</h3><ol><li>Titre, source, date, unités.</li><li>Tendance générale (hausse, baisse, stabilité).</li><li>Valeurs remarquables (maximum, rupture).</li><li>Ce que ça montre par rapport au thème (sans sur-interpréter).</li></ol>
<h3>Figures de style utiles</h3><table><tr><th>Figure</th><th>Définition</th></tr>
<tr><td>Métaphore</td><td>Comparaison sans outil (« cette usine est une fourmilière »)</td></tr>
<tr><td>Comparaison</td><td>Avec « comme », « tel »</td></tr><tr><td>Hyperbole</td><td>Exagération</td></tr><tr><td>Antithèse</td><td>Opposition de deux idées</td></tr>
<tr><td>Ironie</td><td>Dire le contraire de ce qu'on pense pour critiquer</td></tr><tr><td>Anaphore</td><td>Répétition en début de phrase</td></tr></table>
<h3>Bien écrire</h3><ul><li>Phrases courtes et claires. Une idée par phrase.</li><li>Vérifie les accords (participe passé avec avoir : accord avec le COD placé avant), la ponctuation, les homophones (a/à, et/est, ce/se, ou/où, leur/leurs).</li><li>Évite les répétitions et le langage familier.</li></ul>`,
      cards: [["Les 3 étapes d'analyse d'une image ?", "Identifier, décrire, interpréter."], ["Métaphore ?", "Comparaison sans outil de comparaison."], ["Ironie ?", "Dire le contraire de ce qu'on pense pour critiquer."]],
      quiz: [
        ["« Le temps est de l'argent » est :", ["Une comparaison", "Une métaphore", "Une hyperbole", "Une anaphore"], 1, "Pas de « comme »."],
        ["Pour un graphique, on commence par :", ["L'interprétation", "Le titre, la source, les unités", "La conclusion", "La couleur préférée"], 1, "On identifie d'abord le document."],
        ["Quel homophone est correct : « Il ___ parti à Lyon »", ["a", "à", "as", "ah"], 0, "Verbe avoir (on peut dire « avait »)."]
      ]
    }
  ]
});
