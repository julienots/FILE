COURS.push({
  id: "anglais", name: "Anglais technique", emoji: "🇬🇧", color: "#DC2626",
  desc: "Vocabulaire technique et industriel, grammaire essentielle, phrases utiles pour l'oral, les mails et les présentations, méthode pour la compréhension écrite et orale.",
  chapters: [
    {
      id: "vocab_tech", t: "Vocabulaire technique essentiel", y: 0,
      html: `
<h3>Mécanique</h3>
<table><tr><th>Anglais</th><th>Français</th></tr>
<tr><td>gear / gearbox</td><td>engrenage / boîte de vitesses, réducteur</td></tr><tr><td>shaft</td><td>arbre</td></tr><tr><td>bearing</td><td>roulement / palier</td></tr>
<tr><td>bolt / nut / screw / washer</td><td>boulon / écrou / vis / rondelle</td></tr><tr><td>spring</td><td>ressort</td></tr><tr><td>belt / pulley</td><td>courroie / poulie</td></tr>
<tr><td>cylinder (pneumatic)</td><td>vérin</td></tr><tr><td>valve</td><td>vanne / distributeur</td></tr><tr><td>pump</td><td>pompe</td></tr><tr><td>stress / strain</td><td>contrainte / déformation</td></tr>
<tr><td>load</td><td>charge</td></tr><tr><td>torque</td><td>couple</td></tr><tr><td>wear</td><td>usure</td></tr><tr><td>drawing / blueprint</td><td>plan, dessin</td></tr></table>
<h3>Électricité / automatisme</h3>
<table><tr><th>Anglais</th><th>Français</th></tr>
<tr><td>wire / cable</td><td>fil / câble</td></tr><tr><td>switch</td><td>interrupteur</td></tr><tr><td>circuit breaker</td><td>disjoncteur</td></tr><tr><td>fuse</td><td>fusible</td></tr>
<tr><td>contactor / relay</td><td>contacteur / relais</td></tr><tr><td>motor / drive (VFD)</td><td>moteur / variateur</td></tr><tr><td>sensor</td><td>capteur</td></tr><tr><td>PLC (Programmable Logic Controller)</td><td>automate (API)</td></tr>
<tr><td>ground / earth</td><td>terre</td></tr><tr><td>short circuit</td><td>court-circuit</td></tr><tr><td>power supply</td><td>alimentation</td></tr><tr><td>wiring diagram</td><td>schéma de câblage</td></tr></table>
<h3>Industrie / entreprise</h3>
<table><tr><th>Anglais</th><th>Français</th></tr>
<tr><td>plant / factory</td><td>usine</td></tr><tr><td>shop floor</td><td>atelier</td></tr><tr><td>maintenance / breakdown</td><td>maintenance / panne</td></tr><tr><td>to troubleshoot</td><td>diagnostiquer, dépanner</td></tr>
<tr><td>supplier / customer</td><td>fournisseur / client</td></tr><tr><td>quote / invoice</td><td>devis / facture</td></tr><tr><td>deadline</td><td>date limite</td></tr><tr><td>schedule</td><td>planning</td></tr>
<tr><td>safety / PPE</td><td>sécurité / EPI</td></tr><tr><td>requirements / specifications</td><td>exigences / cahier des charges</td></tr><tr><td>internship</td><td>stage</td></tr><tr><td>engineer / technician</td><td>ingénieur / technicien</td></tr></table>
<div class="warn box">Faux amis : <b>actually</b> = en fait (pas « actuellement » = currently) ; <b>eventually</b> = finalement ; <b>library</b> = bibliothèque ; <b>to control</b> = commander/réguler (vérifier = to check) ; <b>sensible</b> = raisonnable.</div>`,
      cards: [["shaft ?", "arbre"], ["bearing ?", "roulement / palier"], ["torque ?", "couple"], ["circuit breaker ?", "disjoncteur"], ["sensor ?", "capteur"], ["PLC ?", "Programmable Logic Controller = automate"], ["to troubleshoot ?", "diagnostiquer / dépanner"], ["quote ?", "devis"], ["actually ?", "en fait (faux ami)"], ["internship ?", "stage"], ["supplier ?", "fournisseur"], ["wear ?", "usure"]],
      quiz: [
        ["« Torque » se traduit par :", ["Couple", "Force", "Vitesse", "Tour"], 0, "Le couple moteur = motor torque."],
        ["« Circuit breaker » =", ["Court-circuit", "Disjoncteur", "Contacteur", "Câble"], 1, "Il « casse » le circuit en cas de défaut."],
        ["« Actually » signifie :", ["Actuellement", "En fait", "Activement", "Jamais"], 1, "Faux ami classique !"],
        ["« Bearing » =", ["Ours", "Roulement", "Ressort", "Poulie"], 1, "Roulement / palier."],
        ["« Deadline » =", ["Ligne morte", "Date limite", "Arrêt d'urgence", "Câble HS"], 1, "Délai à respecter."],
        ["Une usine se dit :", ["Plant", "Plane", "Planet", "Plan"], 0, "Plant ou factory."]
      ]
    },
    {
      id: "grammaire", t: "Grammaire essentielle", y: 0,
      html: `
<h3>Les temps à maîtriser</h3>
<table><tr><th>Temps</th><th>Usage</th><th>Exemple</th></tr>
<tr><td>Present simple</td><td>habitudes, vérités, fonctionnement</td><td>The motor <b>drives</b> the conveyor.</td></tr>
<tr><td>Present continuous</td><td>action en cours</td><td>I <b>am checking</b> the wiring.</td></tr>
<tr><td>Preterit (past simple)</td><td>action finie, moment précis</td><td>I <b>did</b> my internship in 2025.</td></tr>
<tr><td>Present perfect</td><td>lien passé-présent, bilan</td><td>We <b>have reduced</b> the downtime by 20 %.</td></tr>
<tr><td>Future (will / be going to)</td><td>prévision / intention</td><td>The machine <b>will be</b> delivered next week.</td></tr></table>
<h3>Le passif (très utilisé en technique !)</h3><div class="f">be + participe passé</div><p>The part <b>is machined</b> on a CNC lathe. The motor <b>was replaced</b> yesterday.</p>
<h3>Les modaux</h3><ul><li><b>must</b> : obligation (You must wear safety glasses).</li><li><b>mustn't</b> : interdiction.</li><li><b>should</b> : conseil.</li><li><b>can / could</b> : capacité, possibilité.</li><li><b>may / might</b> : probabilité.</li></ul>
<h3>Comparatifs</h3><p>short adj. : faster, the fastest ; long adj. : more reliable, the most reliable. Irréguliers : good → better → the best ; bad → worse → the worst.</p>
<h3>Décrire un process (connecteurs)</h3><p>First, … Then, … Next, … After that, … Finally, … — because / so / however / therefore / although.</p>
<h3>Poser des questions</h3><p>What / Where / When / Why / How / How much / How many / Which / Who + auxiliaire (do/does/did/is/are/have) + sujet + verbe.</p>`,
      cards: [["Construction du passif ?", "be + participe passé"], ["Present perfect : usage ?", "Lien passé-présent, bilan (have + participe passé)."], ["must vs should ?", "must = obligation, should = conseil."], ["Comparatif de good ?", "better (the best)"]],
      quiz: [
        ["« The part ___ by a robot. » (passif présent)", ["is welded", "welds", "welding", "has weld"], 0, "be + participe passé."],
        ["Obligation de sécurité :", ["You may wear gloves", "You must wear gloves", "You could wear gloves", "You might wear gloves"], 1, "must = obligation."],
        ["« Since 2024, we ___ three new robots. »", ["install", "have installed", "installing", "installs"], 1, "Since → present perfect."],
        ["Comparatif de « reliable » :", ["reliabler", "more reliable", "most reliable", "reliablest"], 1, "Adjectif long."]
      ]
    },
    {
      id: "oral", t: "Oral, mails et présentations", y: 0,
      html: `
<h3>Se présenter</h3><p>Hello, my name is … I'm a second-year student in a two-year technical degree called BTS ATI, which stands for « Technical Assistant to Engineers ». I'm interested in automation and project management.</p>
<h3>Présenter son stage / projet</h3><ul><li>I did my internship at … , a company which manufactures …</li><li>My main task was to … (improve / design / set up / analyse).</li><li>The problem was that … So I suggested …</li><li>As a result, we saved … € / reduced … by … %.</li><li>This experience taught me how to …</li></ul>
<h3>Décrire un système</h3><p>It consists of … / It is made of (steel) … / It is used to … / It works as follows: … / The sensor detects … and sends a signal to the PLC.</p>
<h3>Décrire un graphique</h3><p>The graph shows … / increase, rise, go up — decrease, fall, drop — remain stable — reach a peak — sharply / slightly / steadily.</p>
<h3>Mail professionnel</h3>
<pre>Subject: Quote request – pneumatic cylinders

Dear Mr Smith,

I am writing to request a quote for 20 double-acting
cylinders (bore 50 mm, stroke 200 mm).
Could you also confirm the delivery time?

I look forward to hearing from you.

Best regards,
Alex Martin
Technical assistant – ACME Industries</pre>
<h3>Au téléphone / en réunion</h3><ul><li>Could you repeat that, please? / Could you speak more slowly?</li><li>Let me check and I'll get back to you.</li><li>I agree / I see your point, but …</li><li>To sum up, …</li></ul>
<h3>Méthode compréhension orale / écrite</h3><ol><li>Anticipe le sujet avec le titre et les images.</li><li>1ʳᵉ écoute : idée générale (qui ? quoi ? où ?).</li><li>2ᵉ écoute : détails, chiffres, opinions.</li><li>Rends compte en français ou en anglais selon la consigne, de façon organisée.</li></ol>`,
      cards: [["« Je vous recontacte » ?", "I'll get back to you."], ["Fin de mail formelle ?", "I look forward to hearing from you. Best regards,"], ["« Le graphique montre » ?", "The graph shows…"], ["« Il est utilisé pour » ?", "It is used to…"]],
      quiz: [
        ["Pour demander de répéter poliment :", ["Repeat!", "Could you repeat that, please?", "What?", "Say again now"], 1, "Formule polie."],
        ["« Augmenter fortement » se dit :", ["to decrease slightly", "to rise sharply", "to remain stable", "to drop"], 1, "Rise sharply."],
        ["Début de mail formel quand on connaît le nom :", ["Hi buddy", "Dear Mr Smith,", "Yo", "To whom"], 1, "Dear + titre + nom."]
      ]
    }
  ]
});
