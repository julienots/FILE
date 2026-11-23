COURS.push({
  id: "info", name: "Informatique & réseaux industriels", emoji: "💻", color: "#4F46E5",
  desc: "Algorithmique et programmation (Python, C/Arduino), réseaux informatiques (adresses IP, modèle OSI), réseaux de terrain industriels (Modbus, Profinet…), industrie 4.0, tableur et cybersécurité.",
  chapters: [
    {
      id: "algo", t: "Algorithmique et programmation", y: 1,
      html: `
<h3>Les briques de base</h3>
<div class="easy box">Un algorithme est une recette : une suite d'instructions précises pour résoudre un problème. Il n'existe que 4 briques : les <b>variables</b>, les <b>conditions</b>, les <b>boucles</b> et les <b>fonctions</b>.</div>
<h3>Variables et types</h3><p>entier (int), réel (float), booléen (bool : vrai/faux), chaîne (str), liste/tableau.</p>
<h3>Conditions</h3>
<pre>temperature = 85
if temperature > 80:
    print("Alarme !")
elif temperature > 60:
    print("Attention")
else:
    print("OK")</pre>
<h3>Boucles</h3>
<pre># boucle bornée : on connaît le nombre de tours
for i in range(5):        # i = 0, 1, 2, 3, 4
    print(i)

# boucle non bornée : tant qu'une condition est vraie
niveau = 0
while niveau < 100:
    niveau = niveau + 7</pre>
<h3>Fonctions</h3>
<pre>def puissance_triphasee(U, I, cosphi):
    return 3 ** 0.5 * U * I * cosphi

print(puissance_triphasee(400, 10, 0.85))   # ≈ 5889 W</pre>
<h3>Listes</h3>
<pre>mesures = [12.1, 12.4, 11.9, 12.0]
moyenne = sum(mesures) / len(mesures)
maximum = max(mesures)</pre>
<h3>Programmer un microcontrôleur (Arduino, langage C)</h3>
<pre>const int capteur = 2, led = 13;
void setup() {
  pinMode(capteur, INPUT);
  pinMode(led, OUTPUT);
}
void loop() {                 // tourne en boucle, comme un automate
  if (digitalRead(capteur) == HIGH) digitalWrite(led, HIGH);
  else digitalWrite(led, LOW);
}</pre>
<h3>Représentations</h3><p>Algorigramme (organigramme) : ovale = début/fin, rectangle = action, losange = test. Pseudo-code en français (SI… ALORS… SINON, POUR, TANT QUE).</p>
<div class="tip box">Pour débugger : affiche les valeurs intermédiaires (print), teste avec des cas simples dont tu connais la réponse.</div>`,
      cards: [["Les 4 briques d'un algorithme ?", "Variables, conditions, boucles, fonctions."], ["Boucle quand on connaît le nombre de tours ?", "Boucle POUR (for)."], ["Boucle tant qu'une condition est vraie ?", "TANT QUE (while)."], ["Symbole d'un test dans un algorigramme ?", "Losange."], ["range(5) en Python donne ?", "0, 1, 2, 3, 4"]],
      quiz: [
        ["Combien de fois s'exécute « for i in range(3) » ?", ["2", "3", "4", "Infini"], 1, "i = 0, 1, 2."],
        ["Dans un algorigramme, un test (condition) est un :", ["Rectangle", "Losange", "Ovale", "Cercle"], 1, "Losange avec deux sorties oui/non."],
        ["Un booléen peut valoir :", ["Vrai ou faux", "Un nombre décimal", "Un texte", "Une liste"], 0, "True / False."],
        ["La fonction loop() d'un Arduino :", ["S'exécute une fois", "S'exécute en boucle sans fin", "Ne s'exécute jamais", "S'exécute au démarrage seulement"], 1, "Comme le cycle d'un automate."]
      ]
    },
    {
      id: "reseaux", t: "Réseaux informatiques (IP, OSI)", y: 1,
      html: `
<h3>Le modèle OSI (7 couches)</h3>
<table><tr><th>N°</th><th>Couche</th><th>Exemple</th></tr>
<tr><td>7</td><td>Application</td><td>HTTP, Modbus, OPC UA</td></tr><tr><td>6</td><td>Présentation</td><td>chiffrement, encodage</td></tr><tr><td>5</td><td>Session</td><td>ouverture/fermeture de dialogue</td></tr>
<tr><td>4</td><td>Transport</td><td>TCP (fiable), UDP (rapide) – ports</td></tr><tr><td>3</td><td>Réseau</td><td>IP – adresses, routeur</td></tr>
<tr><td>2</td><td>Liaison</td><td>Ethernet – adresse MAC, switch</td></tr><tr><td>1</td><td>Physique</td><td>câble RJ45, fibre, ondes</td></tr></table>
<div class="tip box">Moyen mnémotechnique (de 1 à 7) : « <b>P</b>our <b>L</b>e <b>R</b>éseau <b>T</b>out <b>S</b>e <b>P</b>asse <b>A</b>utomatiquement ».</div>
<h3>Adresse IP (IPv4)</h3>
<p>4 octets : ex. 192.168.1.10. Le <b>masque</b> (ex. 255.255.255.0 = /24) sépare la partie <b>réseau</b> (192.168.1) et la partie <b>machine</b> (10).</p>
<ul><li>Deux appareils communiquent directement s'ils sont dans le <b>même réseau</b> (même partie réseau).</li><li>Adresse réseau (192.168.1.0) et adresse de diffusion (192.168.1.255) ne sont pas attribuables.</li><li>/24 → 254 machines possibles (2⁸ − 2).</li><li>Passerelle (gateway) : le routeur pour sortir du réseau.</li></ul>
<div class="ex box">API en 192.168.0.10/24 et PC en 192.168.1.20/24 → réseaux différents (0 ≠ 1) : ils ne communiquent pas sans routeur. Solution : mettre le PC en 192.168.0.20.</div>
<h3>Matériel</h3><ul><li><b>Switch</b> (commutateur) : relie les appareils d'un même réseau (couche 2).</li><li><b>Routeur</b> : relie des réseaux différents (couche 3).</li><li>Point d'accès Wi-Fi, pare-feu.</li></ul>
<h3>Services utiles</h3><p>DHCP (attribue les adresses automatiquement), DNS (nom → adresse IP), ping (tester la connexion), ipconfig/ip a (voir sa configuration).</p>
<h3>Topologies</h3><p>Étoile (la plus courante), bus, anneau (redondance industrielle), maillée.</p>`,
      cards: [["Couche 3 OSI ?", "Réseau (IP, routeur)."], ["Couche 2 OSI ?", "Liaison (Ethernet, MAC, switch)."], ["Masque /24 ?", "255.255.255.0"], ["Nombre de machines dans un /24 ?", "254"], ["Rôle du DHCP ?", "Attribuer automatiquement les adresses IP."], ["Switch ou routeur pour relier deux réseaux IP différents ?", "Routeur."]],
      quiz: [
        ["192.168.1.5/24 et 192.168.1.200/24 sont :", ["Dans le même réseau", "Dans des réseaux différents", "Invalides", "Identiques"], 0, "Même partie réseau 192.168.1."],
        ["Le protocole fiable de la couche transport est :", ["UDP", "TCP", "IP", "HTTP"], 1, "TCP vérifie la bonne réception."],
        ["Pour tester si une machine répond on utilise :", ["ping", "print", "DNS", "FTP"], 0, "Envoie des paquets ICMP."],
        ["Le DNS sert à :", ["Donner une adresse IP automatiquement", "Traduire un nom en adresse IP", "Chiffrer", "Router"], 1, "Ex : www.exemple.fr → 93.184…"]
      ]
    },
    {
      id: "bus", t: "Réseaux industriels et industrie 4.0", y: 2,
      html: `
<h3>La pyramide de l'automatisation (CIM)</h3>
<p>Niveau 0 : capteurs/actionneurs → 1 : automates → 2 : supervision (SCADA) → 3 : MES (gestion de production) → 4 : ERP (gestion de l'entreprise).</p>
<h3>Liaisons série</h3><ul><li>RS-232 : point à point, courte distance.</li><li>RS-485 : multipoint, jusqu'à ~1 200 m, différentiel (résiste aux parasites) – support de Modbus RTU.</li></ul>
<h3>Les principaux bus et réseaux de terrain</h3>
<table><tr><th>Réseau</th><th>Caractéristiques</th></tr>
<tr><td><b>Modbus RTU / TCP</b></td><td>Maître/esclave (client/serveur), simple, ouvert, très répandu. Mots (registres) et bits (bobines).</td></tr>
<tr><td><b>Profibus / Profinet</b></td><td>Siemens ; Profinet = sur Ethernet, temps réel.</td></tr>
<tr><td><b>EtherNet/IP</b></td><td>Rockwell, sur Ethernet.</td></tr>
<tr><td><b>EtherCAT</b></td><td>Très rapide, commande d'axes.</td></tr>
<tr><td><b>CANopen</b></td><td>Embarqué, véhicules, machines mobiles.</td></tr>
<tr><td><b>AS-i</b></td><td>Capteurs/actionneurs simples sur 2 fils.</td></tr>
<tr><td><b>IO-Link</b></td><td>Liaison point à point intelligente pour capteurs (paramétrage, diagnostic).</td></tr>
<tr><td><b>OPC UA</b></td><td>Standard d'échange de données sécurisé entre machines et logiciels (industrie 4.0).</td></tr>
<tr><td><b>MQTT</b></td><td>Protocole léger pour l'IoT (publication/abonnement).</td></tr></table>
<h3>Modbus en pratique</h3><p>Le maître envoie une requête : n° esclave + code fonction (01 lire bits, 03 lire registres, 06 écrire un registre, 16 écrire plusieurs registres) + adresse + données + contrôle d'erreur (CRC). L'esclave répond.</p>
<h3>Industrie 4.0 (usine du futur)</h3><ul><li><b>IoT industriel</b> : capteurs connectés partout.</li><li><b>Big data / IA</b> : maintenance prédictive, optimisation.</li><li><b>Jumeau numérique</b> : copie virtuelle d'une machine pour simuler / mettre en service virtuellement.</li><li><b>Cobots</b> : robots collaboratifs travaillant près de l'humain.</li><li><b>Réalité augmentée</b> : aide à la maintenance.</li><li><b>Fabrication additive</b>, cloud / edge computing.</li></ul>`,
      cards: [["Modbus : principe ?", "Maître/esclave (client/serveur), registres et bits."], ["Code fonction Modbus pour lire des registres ?", "03"], ["RS-485 ?", "Liaison série multipoint, longue distance, différentielle."], ["OPC UA ?", "Standard d'échange de données sécurisé industrie 4.0."], ["Jumeau numérique ?", "Copie virtuelle d'un système pour simuler."], ["Cobot ?", "Robot collaboratif travaillant près de l'humain."]],
      quiz: [
        ["Modbus fonctionne en mode :", ["Maître / esclave", "Pair à pair uniquement", "Diffusion radio", "Anneau à jeton"], 0, "Le maître interroge, l'esclave répond."],
        ["Le réseau Ethernet temps réel de Siemens :", ["Profinet", "CANopen", "AS-i", "MQTT"], 0, "Profinet."],
        ["Le niveau ERP de la pyramide concerne :", ["Les capteurs", "La gestion de l'entreprise", "Les automates", "Les moteurs"], 1, "Commandes, stocks, comptabilité."],
        ["Pour la maintenance prédictive on utilise surtout :", ["Des données capteurs et l'analyse (IA)", "Un tournevis", "Un Gantt", "Le DNS"], 0, "Industrie 4.0."]
      ]
    },
    {
      id: "tableur", t: "Tableur et traitement de données", y: 1,
      html: `
<h3>Les bases</h3><ul><li>Une cellule = colonne + ligne (B3).</li><li>Une formule commence par <code>=</code>.</li><li><b>Références relatives</b> (A1) : changent quand on recopie. <b>Absolues</b> ($A$1) : restent fixes (touche F4).</li></ul>
<h3>Fonctions à connaître</h3>
<table><tr><th>Fonction</th><th>Rôle</th></tr>
<tr><td>=SOMME(A1:A10)</td><td>Somme</td></tr><tr><td>=MOYENNE(), =MIN(), =MAX()</td><td>Statistiques simples</td></tr>
<tr><td>=ECARTYPE.STANDARD()</td><td>Écart type</td></tr><tr><td>=SI(B2>80;"Alarme";"OK")</td><td>Condition</td></tr>
<tr><td>=NB.SI(A:A;"Défaut")</td><td>Compter selon un critère</td></tr><tr><td>=SOMME.SI()</td><td>Somme selon un critère</td></tr>
<tr><td>=RECHERCHEV() / =RECHERCHEX()</td><td>Chercher une valeur dans un tableau (catalogue, tarifs)</td></tr>
<tr><td>=ARRONDI(x;2)</td><td>Arrondir</td></tr></table>
<h3>Graphiques</h3><ul><li>Courbe (nuage de points) : évolution d'une grandeur en fonction d'une autre.</li><li>Histogramme / barres : comparer des quantités.</li><li>Secteurs : proportions (peu de catégories).</li><li>Pareto : barres triées + courbe cumulée.</li></ul>
<h3>Outils avancés</h3><p>Tri, filtres, mise en forme conditionnelle, tableaux croisés dynamiques (synthèse rapide), courbe de tendance (régression linéaire avec équation et R²), valeur cible / solveur.</p>
<div class="ex box">Calcul de coûts d'un projet : colonne quantité × prix unitaire, total avec =SOMME, TVA avec une cellule absolue $F$1 = 20 %.</div>`,
      cards: [["Référence absolue ?", "$A$1 (ne change pas lors de la recopie)."], ["Fonction de condition ?", "=SI(test;si_vrai;si_faux)"], ["Chercher un prix dans un catalogue ?", "RECHERCHEV / RECHERCHEX"], ["Graphique pour une évolution x/y ?", "Nuage de points (courbe)."]],
      quiz: [
        ["Pour qu'une cellule ne change pas lors de la recopie :", ["A1", "$A$1", "#A1", "A1!"], 1, "Référence absolue."],
        ["=SI(A1>10;\"Haut\";\"Bas\") avec A1 = 5 affiche :", ["Haut", "Bas", "5", "Erreur"], 1, "5 n'est pas > 10."],
        ["Pour compter le nombre de « NOK » dans une colonne :", ["NB.SI", "SOMME", "MAX", "ARRONDI"], 0, "Compte selon un critère."]
      ]
    },
    {
      id: "cyber", t: "Cybersécurité industrielle", y: 2,
      html: `
<h3>Pourquoi c'est important ?</h3><div class="easy box">Les machines sont maintenant connectées. Un virus ou un pirate peut arrêter une usine, abîmer une machine ou voler des plans. Les rançongiciels (ransomwares) visent aussi les industriels.</div>
<h3>Les bonnes pratiques</h3>
<ul><li>Mots de passe robustes et différents, changer les mots de passe par défaut des automates et IHM.</li><li>Segmenter les réseaux (réseau bureautique séparé du réseau industriel, pare-feu, DMZ).</li><li>Mettre à jour (quand c'est validé) les systèmes et firmwares.</li><li>Contrôler les clés USB (sources fréquentes d'infection).</li><li>Sauvegardes régulières des programmes automates et des configurations (et les tester).</li><li>Limiter les accès à distance (VPN sécurisé, authentification forte).</li><li>Sensibiliser le personnel (hameçonnage / phishing).</li></ul>
<h3>Triade DIC</h3><p><b>D</b>isponibilité, <b>I</b>ntégrité, <b>C</b>onfidentialité. En industrie, la priorité est souvent la disponibilité et l'intégrité (la machine doit tourner et faire ce qu'on lui demande).</p>
<h3>Référentiels</h3><p>Norme IEC 62443 (cybersécurité des systèmes industriels), guides de l'ANSSI, RGPD pour les données personnelles.</p>`,
      cards: [["Triade DIC ?", "Disponibilité, Intégrité, Confidentialité."], ["Norme cybersécurité industrielle ?", "IEC 62443"], ["Pourquoi segmenter le réseau ?", "Isoler le réseau industriel du réseau bureautique et limiter la propagation d'une attaque."]],
      quiz: [
        ["Une bonne pratique de cybersécurité :", ["Garder le mot de passe par défaut", "Segmenter le réseau industriel", "Brancher toute clé USB trouvée", "Désactiver les sauvegardes"], 1, "Limite la propagation."],
        ["Le phishing est :", ["Une technique de pêche", "Un mail piège pour voler des identifiants", "Un réseau de terrain", "Un capteur"], 1, "Hameçonnage."]
      ]
    }
  ]
});
