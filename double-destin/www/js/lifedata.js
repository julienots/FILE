"use strict";
// ============ UNE VIE — données ============
const LD = {};
LD.namesM = "Lucas Hugo Louis Gabriel Arthur Jules Raphaël Adam Léo Nathan Tom Ethan Noah Paul Sacha Maël Nolan Théo Timéo Mathis Enzo Axel Antoine Baptiste Clément Maxime Alexandre Thomas Nicolas Julien Kevin Mehdi Yanis Rayan Amir Ibrahima Moussa Karim Samir Youssef Bilal Olivier Pierre Marc Romain Valentin Quentin Florian Benjamin Victor Martin Simon Gaspard Éliott Malo Noé Aaron Ismaël Mamadou Cheikh Félix Émile Augustin Côme Joseph Samuel David Yohan Dylan Loïc Erwan Kylian Anthony Damien Jérémy Cédric Fabrice Laurent Stéphane Christophe Philippe Thierry Didier Patrick Michel Jean Bernard".split(" ");
LD.namesF = "Emma Jade Louise Alice Chloé Lina Léa Rose Anna Mila Inès Ambre Julia Mia Léna Manon Juliette Lou Zoé Camille Sarah Eva Agathe Jeanne Romane Nina Lola Clara Margaux Charlotte Pauline Marie Sophie Laura Julie Anaïs Océane Mélanie Aurélie Céline Nadia Yasmine Salomé Fatou Aïcha Awa Khadija Leïla Sofia Amira Maëlys Capucine Victoire Apolline Héloïse Constance Iris Elsa Gabrielle Margot Lucie Noémie Anouk Alba Lisa Clémence Élise Mathilde Justine Morgane Audrey Valérie Isabelle Nathalie Sandrine Christine Sylvie Catherine Brigitte Martine Monique Françoise Hélène".split(" ");
LD.lastNames = "Martin Bernard Dubois Thomas Robert Richard Petit Durand Leroy Moreau Simon Laurent Lefebvre Michel Garcia David Bertrand Roux Vincent Fournier Morel Girard André Lefèvre Mercier Dupont Lambert Bonnet François Martinez Legrand Garnier Faure Rousseau Blanc Guérin Muller Henry Roussel Nicolas Perrin Morin Mathieu Clément Gauthier Dumont Lopez Fontaine Chevalier Robin Masson Sanchez Gérard Nguyen Boyer Denis Lemaire Duval Joly Gautier Roger Roche Roy Noël Meyer Lucas Meunier Jean Perez Marchand Dufour Blanchard Marie Barbier Brun Dumas Brunet Schmitt Leroux Colin Fernandez Pierre Renard Arnaud Rolland Caron Aubert Giraud Leclerc Vidal Bourgeois Renaud Lemoine Picard Gaillard Philippe Leclercq Lacroix Fabre Dupuis Olivier Rodriguez Da_Silva Hubert Louis Charles Guillot Rivière Le_Gall Guillaume Adam Rey Moulin Gonzalez Berger Lecomte Ménard Fleury Deschamps Carpentier Julien Benoît Paris Maillard Marchal Aubry Vasseur Le_Roux Renault Jacquet Collet Prévost Poirier Charpentier Royer Huet Baron Dupuy Pons Paul Laine Carré Breton Rémy Schneider Perrot Guyot Barre Marty Cousin Diallo Traoré Koné Diop Ndiaye Benali Haddad Mansouri Belkacem Tremblay Gagnon Côté Bouchard Gauthier_Roy Peeters Janssens Maes Favre Rochat Müller".split(" ").map(s => s.replace(/_/g, " "));
LD.countries = [
  { n: "France", flag: "🇫🇷", cur: "€", col: 1, cities: ["Paris", "Lyon", "Marseille", "Toulouse", "Nice", "Nantes", "Strasbourg", "Montpellier", "Bordeaux", "Lille", "Rennes", "Reims", "Le Havre", "Grenoble", "Dijon", "Angers", "Brest", "Tours", "Limoges", "Clermont-Ferrand", "Perpignan", "Metz", "Rouen", "Caen", "Orléans", "Ajaccio", "Pau", "La Rochelle"], retire: 64, legal: 18, drive: 18 },
  { n: "Belgique", flag: "🇧🇪", cur: "€", col: 1, cities: ["Bruxelles", "Liège", "Namur", "Charleroi", "Mons", "Anvers", "Gand", "Louvain", "Bruges"], retire: 66, legal: 18, drive: 18 },
  { n: "Suisse", flag: "🇨🇭", cur: "CHF", col: 1.8, cities: ["Genève", "Lausanne", "Neuchâtel", "Fribourg", "Sion", "Zurich", "Berne", "Bâle"], retire: 65, legal: 18, drive: 18 },
  { n: "Canada", flag: "🇨🇦", cur: "$", col: 1.1, cities: ["Montréal", "Québec", "Gatineau", "Sherbrooke", "Trois-Rivières", "Laval", "Toronto", "Ottawa", "Vancouver"], retire: 65, legal: 18, drive: 16 },
  { n: "Luxembourg", flag: "🇱🇺", cur: "€", col: 1.7, cities: ["Luxembourg", "Esch-sur-Alzette", "Differdange"], retire: 65, legal: 18, drive: 18 },
  { n: "Maroc", flag: "🇲🇦", cur: "€", col: 0.3, cities: ["Casablanca", "Rabat", "Marrakech", "Fès", "Tanger", "Agadir", "Meknès", "Oujda"], retire: 63, legal: 18, drive: 18 },
  { n: "Algérie", flag: "🇩🇿", cur: "€", col: 0.25, cities: ["Alger", "Oran", "Constantine", "Annaba", "Blida", "Sétif", "Tlemcen"], retire: 60, legal: 19, drive: 18 },
  { n: "Tunisie", flag: "🇹🇳", cur: "€", col: 0.25, cities: ["Tunis", "Sfax", "Sousse", "Bizerte", "Nabeul", "Djerba"], retire: 62, legal: 18, drive: 18 },
  { n: "Sénégal", flag: "🇸🇳", cur: "€", col: 0.15, cities: ["Dakar", "Thiès", "Saint-Louis", "Touba", "Ziguinchor", "Kaolack"], retire: 60, legal: 18, drive: 18 },
  { n: "Côte d'Ivoire", flag: "🇨🇮", cur: "€", col: 0.15, cities: ["Abidjan", "Yamoussoukro", "Bouaké", "San-Pédro", "Korhogo"], retire: 60, legal: 18, drive: 18 },
  { n: "Cameroun", flag: "🇨🇲", cur: "€", col: 0.13, cities: ["Douala", "Yaoundé", "Bafoussam", "Garoua"], retire: 60, legal: 21, drive: 18 },
  { n: "États-Unis", flag: "🇺🇸", cur: "$", col: 1.3, cities: ["New York", "Los Angeles", "Miami", "Chicago", "Houston", "La Nouvelle-Orléans", "San Francisco", "Las Vegas", "Seattle"], retire: 67, legal: 21, drive: 16 },
  { n: "Royaume-Uni", flag: "🇬🇧", cur: "£", col: 1.1, cities: ["Londres", "Manchester", "Liverpool", "Édimbourg", "Bristol", "Birmingham"], retire: 66, legal: 18, drive: 17 },
  { n: "Japon", flag: "🇯🇵", cur: "¥", col: 1, cities: ["Tokyo", "Osaka", "Kyoto", "Sapporo", "Fukuoka"], retire: 65, legal: 20, drive: 18 },
  { n: "Brésil", flag: "🇧🇷", cur: "R$", col: 0.35, cities: ["Rio de Janeiro", "São Paulo", "Salvador", "Brasília", "Recife"], retire: 65, legal: 18, drive: 18 },
];
LD.zodiac = [["Capricorne", 20], ["Verseau", 19], ["Poissons", 20], ["Bélier", 20], ["Taureau", 21], ["Gémeaux", 21], ["Cancer", 22], ["Lion", 23], ["Vierge", 23], ["Balance", 23], ["Scorpion", 22], ["Sagittaire", 22], ["Capricorne", 31]];
LD.months = ["janvier", "février", "mars", "avril", "mai", "juin", "juillet", "août", "septembre", "octobre", "novembre", "décembre"];

// Filières universitaires : années, diplôme, coût annuel
LD.majors = [
  { id: "med", n: "Médecine", y: 9, deg: "doctorat", cost: 4000, intel: 75 },
  { id: "phar", n: "Pharmacie", y: 6, deg: "doctorat", cost: 3500, intel: 65 },
  { id: "dent", n: "Chirurgie dentaire", y: 6, deg: "doctorat", cost: 4500, intel: 68 },
  { id: "vet", n: "École vétérinaire", y: 6, deg: "doctorat", cost: 5000, intel: 70 },
  { id: "droit", n: "Droit", y: 5, deg: "master", cost: 1500, intel: 55 },
  { id: "info", n: "Informatique", y: 5, deg: "master", cost: 1500, intel: 55 },
  { id: "ing", n: "École d'ingénieurs", y: 5, deg: "master", cost: 6000, intel: 65 },
  { id: "com", n: "École de commerce", y: 5, deg: "master", cost: 12000, intel: 45 },
  { id: "eco", n: "Économie & gestion", y: 3, deg: "licence", cost: 600, intel: 40 },
  { id: "psy", n: "Psychologie", y: 5, deg: "master", cost: 700, intel: 45 },
  { id: "lettres", n: "Lettres & langues", y: 3, deg: "licence", cost: 500, intel: 30 },
  { id: "hist", n: "Histoire", y: 3, deg: "licence", cost: 500, intel: 30 },
  { id: "sci", n: "Sciences (bio/chimie/physique)", y: 3, deg: "licence", cost: 600, intel: 45 },
  { id: "math", n: "Mathématiques", y: 3, deg: "licence", cost: 600, intel: 60 },
  { id: "arts", n: "Beaux-arts", y: 3, deg: "licence", cost: 900, intel: 15 },
  { id: "archi", n: "Architecture", y: 5, deg: "master", cost: 1200, intel: 55 },
  { id: "staps", n: "STAPS (sport)", y: 3, deg: "licence", cost: 500, intel: 20 },
  { id: "jour", n: "Journalisme", y: 5, deg: "master", cost: 4000, intel: 45 },
  { id: "infirm", n: "Soins infirmiers (IFSI)", y: 3, deg: "licence", cost: 300, intel: 35 },
  { id: "educ", n: "Sciences de l'éducation (prof)", y: 5, deg: "master", cost: 500, intel: 40 },
  { id: "pol", n: "Sciences Po", y: 5, deg: "master", cost: 9000, intel: 65 },
  { id: "bts", n: "BTS / BUT (technique)", y: 2, deg: "bts", cost: 200, intel: 15 },
  { id: "police", n: "École de police", y: 1, deg: "police", cost: 0, intel: 20 },
  { id: "militaire", n: "Académie militaire", y: 3, deg: "militaire", cost: 0, intel: 40 },
  { id: "cuisine", n: "École hôtelière / cuisine", y: 2, deg: "bts", cost: 3000, intel: 10 },
];
const DEG_RANK = { aucun: 0, brevet: 1, bac: 2, bts: 3, police: 3, militaire: 4, licence: 4, master: 5, doctorat: 6 };

// Métiers : t titre, sal salaire annuel (€), deg diplôme requis, maj filière requise, car = échelons
LD.jobs = [
  // Petits boulots (14-17 ans) et sans diplôme
  { t: "Baby-sitter", sal: 3000, pt: 1, cat: "Petits boulots" },
  { t: "Livreur de journaux", sal: 2200, pt: 1, cat: "Petits boulots" },
  { t: "Plongeur en restaurant", sal: 4000, pt: 1, cat: "Petits boulots" },
  { t: "Vendeur (temps partiel)", sal: 5500, pt: 1, cat: "Petits boulots" },
  { t: "Livreur à vélo", sal: 6000, pt: 1, cat: "Petits boulots" },
  { t: "Équipier fast-food", sal: 17000, cat: "Commerce", car: ["Équipier fast-food", "Chef d'équipe", "Manager de restaurant", "Directeur de franchise"] },
  { t: "Caissier", sal: 18500, cat: "Commerce", car: ["Caissier", "Responsable de caisse", "Chef de rayon", "Directeur de magasin"] },
  { t: "Agent d'entretien", sal: 18000, cat: "Services", car: ["Agent d'entretien", "Chef d'équipe propreté", "Responsable de site"] },
  { t: "Serveur", sal: 19000, cat: "Hôtellerie", car: ["Serveur", "Chef de rang", "Maître d'hôtel", "Directeur de salle"] },
  { t: "Manutentionnaire", sal: 19500, cat: "Industrie", car: ["Manutentionnaire", "Cariste", "Chef d'équipe logistique", "Responsable d'entrepôt"] },
  { t: "Agent de sécurité", sal: 20000, cat: "Services", car: ["Agent de sécurité", "Chef de poste", "Responsable sécurité"] },
  { t: "Ouvrier du bâtiment", sal: 21000, cat: "Bâtiment", car: ["Manœuvre", "Ouvrier qualifié", "Chef de chantier", "Conducteur de travaux"] },
  { t: "Chauffeur de taxi / VTC", sal: 22000, cat: "Transport", drive: 1 },
  { t: "Chauffeur routier", sal: 25000, cat: "Transport", drive: 1, car: ["Chauffeur routier", "Chauffeur international", "Chef de parc"] },
  { t: "Facteur", sal: 21000, cat: "Services", car: ["Facteur", "Facteur chef", "Responsable de plateforme"] },
  { t: "Agriculteur", sal: 18000, cat: "Agriculture", car: ["Ouvrier agricole", "Agriculteur", "Exploitant agricole", "Grand propriétaire terrien"] },
  { t: "Aide-soignant", sal: 22000, deg: "bac", cat: "Santé", car: ["Aide-soignant", "Aide-soignant principal"] },
  { t: "Secrétaire", sal: 23000, deg: "bac", cat: "Bureau", car: ["Secrétaire", "Assistant de direction", "Office manager"] },
  { t: "Employé de banque", sal: 26000, deg: "bac", cat: "Finance", car: ["Guichetier", "Conseiller clientèle", "Directeur d'agence"] },
  { t: "Coiffeur", sal: 19000, deg: "bts", maj: "bts", cat: "Artisanat", car: ["Apprenti coiffeur", "Coiffeur", "Coiffeur styliste", "Patron de salon"] },
  { t: "Mécanicien", sal: 23000, deg: "bts", cat: "Artisanat", car: ["Mécanicien", "Chef d'atelier", "Patron de garage"] },
  { t: "Électricien", sal: 26000, deg: "bts", cat: "Bâtiment", car: ["Électricien", "Chef électricien", "Patron d'entreprise d'électricité"] },
  { t: "Plombier", sal: 27000, deg: "bts", cat: "Bâtiment", car: ["Plombier", "Plombier chauffagiste", "Patron de plomberie"] },
  { t: "Cuisinier", sal: 21000, deg: "bts", maj: "cuisine", cat: "Hôtellerie", car: ["Commis de cuisine", "Chef de partie", "Sous-chef", "Chef cuisinier", "Chef étoilé"] },
  { t: "Technicien informatique", sal: 28000, deg: "bts", cat: "Technologie", car: ["Technicien support", "Technicien réseau", "Administrateur systèmes"] },
  { t: "Policier", sal: 27000, deg: "police", maj: "police", cat: "Sécurité", car: ["Gardien de la paix", "Brigadier", "Lieutenant", "Commandant", "Commissaire", "Directeur de la police"] },
  { t: "Militaire", sal: 26000, deg: "militaire", maj: "militaire", cat: "Armée", car: ["Sous-lieutenant", "Lieutenant", "Capitaine", "Commandant", "Colonel", "Général"] },
  { t: "Pompier professionnel", sal: 26000, deg: "bac", cat: "Sécurité", fit: 60, car: ["Sapeur", "Caporal", "Sergent", "Lieutenant", "Capitaine des pompiers"] },
  { t: "Infirmier", sal: 30000, deg: "licence", maj: "infirm", cat: "Santé", car: ["Infirmier", "Infirmier spécialisé", "Cadre de santé"] },
  { t: "Professeur des écoles", sal: 29000, deg: "master", maj: "educ", cat: "Éducation", car: ["Professeur des écoles", "Directeur d'école"] },
  { t: "Professeur de lycée", sal: 32000, deg: "master", cat: "Éducation", car: ["Professeur certifié", "Professeur agrégé", "Proviseur"] },
  { t: "Comptable", sal: 33000, deg: "licence", maj: "eco", cat: "Finance", car: ["Comptable", "Chef comptable", "Expert-comptable", "Directeur financier"] },
  { t: "Journaliste", sal: 31000, deg: "master", maj: "jour", cat: "Médias", car: ["Pigiste", "Journaliste", "Grand reporter", "Rédacteur en chef"] },
  { t: "Psychologue", sal: 36000, deg: "master", maj: "psy", cat: "Santé", car: ["Psychologue", "Psychologue clinicien", "Psychothérapeute réputé"] },
  { t: "Architecte", sal: 42000, deg: "master", maj: "archi", cat: "Bâtiment", car: ["Architecte junior", "Architecte", "Architecte associé", "Architecte star"] },
  { t: "Développeur", sal: 42000, deg: "master", maj: "info", cat: "Technologie", car: ["Développeur junior", "Développeur", "Développeur senior", "Lead développeur", "Directeur technique (CTO)"] },
  { t: "Ingénieur", sal: 46000, deg: "master", maj: "ing", cat: "Ingénierie", car: ["Ingénieur junior", "Ingénieur", "Ingénieur principal", "Directeur de l'ingénierie"] },
  { t: "Commercial", sal: 35000, deg: "licence", cat: "Commerce", car: ["Commercial junior", "Commercial", "Responsable grands comptes", "Directeur commercial"] },
  { t: "Consultant", sal: 48000, deg: "master", maj: "com", cat: "Finance", car: ["Consultant junior", "Consultant", "Manager", "Associé"] },
  { t: "Trader", sal: 70000, deg: "master", maj: "com", intel: 70, cat: "Finance", car: ["Analyste", "Trader", "Trader senior", "Directeur de salle des marchés"] },
  { t: "Avocat", sal: 50000, deg: "master", maj: "droit", cat: "Justice", car: ["Avocat collaborateur", "Avocat", "Avocat associé", "Bâtonnier"] },
  { t: "Juge", sal: 55000, deg: "master", maj: "droit", intel: 70, cat: "Justice", car: ["Juge", "Juge d'instruction", "Président de tribunal", "Juge à la Cour de cassation"] },
  { t: "Pharmacien", sal: 55000, deg: "doctorat", maj: "phar", cat: "Santé", car: ["Pharmacien adjoint", "Pharmacien titulaire"] },
  { t: "Dentiste", sal: 80000, deg: "doctorat", maj: "dent", cat: "Santé", car: ["Dentiste", "Orthodontiste", "Chef de clinique dentaire"] },
  { t: "Vétérinaire", sal: 50000, deg: "doctorat", maj: "vet", cat: "Santé", car: ["Vétérinaire", "Vétérinaire spécialisé", "Directeur de clinique vétérinaire"] },
  { t: "Médecin", sal: 75000, deg: "doctorat", maj: "med", cat: "Santé", car: ["Interne", "Médecin généraliste", "Médecin spécialiste", "Chef de service", "Professeur de médecine"] },
  { t: "Chirurgien", sal: 120000, deg: "doctorat", maj: "med", intel: 80, cat: "Santé", car: ["Chirurgien junior", "Chirurgien", "Chirurgien renommé", "Chef de chirurgie"] },
  { t: "Chercheur", sal: 38000, deg: "master", maj: "sci", cat: "Science", car: ["Doctorant", "Chercheur", "Directeur de recherche", "Prix Nobel en puissance"] },
  { t: "Pilote de ligne", sal: 90000, deg: "licence", intel: 65, cat: "Transport", car: ["Copilote", "Commandant de bord", "Chef pilote"] },
  { t: "Diplomate", sal: 60000, deg: "master", maj: "pol", cat: "Politique", car: ["Attaché d'ambassade", "Conseiller", "Ambassadeur"] },
  { t: "Coach sportif", sal: 25000, deg: "licence", maj: "staps", cat: "Sport", car: ["Coach", "Préparateur physique", "Entraîneur professionnel"] },
  { t: "Graphiste", sal: 28000, deg: "licence", maj: "arts", cat: "Création", car: ["Graphiste", "Directeur artistique", "Directeur de création"] },
];
// Carrières spéciales (célébrité / politique) gérées à part
LD.fameCareers = [
  { id: "acteur", t: "Acteur", ic: "🎬", st: "beaute", lv: ["Figurant", "Second rôle", "Acteur", "Tête d'affiche", "Star de cinéma", "Légende d'Hollywood"] },
  { id: "musicien", t: "Musicien", ic: "🎤", st: "talent", lv: ["Musicien de rue", "Artiste local", "Artiste signé", "Disque d'or", "Star internationale", "Légende de la musique"] },
  { id: "foot", t: "Footballeur", ic: "⚽", st: "sport", lv: ["Joueur amateur", "Centre de formation", "Joueur de Ligue 2", "Joueur de Ligue 1", "International", "Ballon d'or"] },
  { id: "ecrivain", t: "Écrivain", ic: "✍️", st: "intel", lv: ["Auteur inconnu", "Auteur publié", "Auteur à succès", "Best-seller", "Prix Goncourt", "Classique vivant"] },
  { id: "influ", t: "Influenceur", ic: "📱", st: "beaute", lv: ["Micro-influenceur", "Influenceur", "Créateur reconnu", "Star des réseaux", "Icône du web"] },
];
LD.politics = ["Conseiller municipal", "Maire", "Conseiller régional", "Député", "Sénateur", "Ministre", "Premier ministre", "Président de la République"];

LD.cars = [
  { n: "Vieille citadine d'occasion", p: 2500, ic: "🚗" }, { n: "Citadine neuve", p: 15000, ic: "🚗" }, { n: "Berline familiale", p: 28000, ic: "🚙" },
  { n: "SUV", p: 42000, ic: "🚙" }, { n: "Voiture électrique haut de gamme", p: 60000, ic: "🚘" }, { n: "Berline de luxe", p: 95000, ic: "🚘" },
  { n: "Voiture de sport", p: 180000, ic: "🏎️" }, { n: "Supercar italienne", p: 320000, ic: "🏎️" }, { n: "Hypercar de collection", p: 2500000, ic: "🏎️" },
  { n: "Moto 125", p: 4000, ic: "🏍️" }, { n: "Grosse cylindrée", p: 18000, ic: "🏍️" }, { n: "Camping-car", p: 70000, ic: "🚐" },
  { n: "Voilier", p: 90000, ic: "⛵" }, { n: "Yacht", p: 4000000, ic: "🛥️" }, { n: "Jet privé", p: 25000000, ic: "🛩️" },
];
LD.houses = [
  { n: "Studio", p: 90000, ic: "🏢", rent: 6000 }, { n: "Appartement 2 pièces", p: 160000, ic: "🏢", rent: 8400 }, { n: "Appartement familial", p: 260000, ic: "🏢", rent: 11000 },
  { n: "Maison de ville", p: 320000, ic: "🏠", rent: 13000 }, { n: "Maison avec jardin", p: 420000, ic: "🏡", rent: 16000 }, { n: "Loft de standing", p: 650000, ic: "🏙️", rent: 24000 },
  { n: "Ferme rénovée", p: 380000, ic: "🏡", rent: 14000 }, { n: "Villa avec piscine", p: 1200000, ic: "🏖️", rent: 40000 }, { n: "Chalet à la montagne", p: 900000, ic: "🏔️", rent: 32000 },
  { n: "Hôtel particulier", p: 5000000, ic: "🏛️", rent: 150000 }, { n: "Château", p: 9000000, ic: "🏰", rent: 0 }, { n: "Île privée", p: 30000000, ic: "🏝️", rent: 0 },
];
LD.pets = [
  { n: "Chien", ic: "🐕", p: 400, life: [10, 16] }, { n: "Chat", ic: "🐈", p: 150, life: [12, 20] }, { n: "Lapin", ic: "🐇", p: 60, life: [6, 11] },
  { n: "Hamster", ic: "🐹", p: 20, life: [2, 3] }, { n: "Perroquet", ic: "🦜", p: 900, life: [20, 60] }, { n: "Poisson rouge", ic: "🐠", p: 10, life: [2, 10] },
  { n: "Cheval", ic: "🐎", p: 6000, life: [22, 32] }, { n: "Tortue", ic: "🐢", p: 80, life: [30, 80] }, { n: "Serpent", ic: "🐍", p: 250, life: [15, 25] },
];
LD.petNames = "Rex Médor Filou Caramel Pirate Moka Oscar Nala Simba Biscotte Kiwi Choupette Gribouille Tigrou Pompon Noisette Rocky Max Loulou Plume Patate Sushi Cookie Bandit Zorro Pixel Chipie".split(" ");
LD.diseases = [
  { n: "Grippe", sev: 8, cure: 120, heal: .7 }, { n: "Angine", sev: 5, cure: 60, heal: .8 }, { n: "Bronchite", sev: 8, cure: 90, heal: .6 },
  { n: "Varicelle", sev: 4, cure: 50, heal: .9, maxAge: 14 }, { n: "Migraines chroniques", sev: 6, cure: 400, heal: .1 }, { n: "Asthme", sev: 6, cure: 300, heal: .05 },
  { n: "Diabète", sev: 10, cure: 1500, heal: 0, minAge: 30 }, { n: "Hypertension", sev: 8, cure: 600, heal: .05, minAge: 35 }, { n: "Dépression", sev: 10, cure: 1200, heal: .2, mood: 1 },
  { n: "Pneumonie", sev: 18, cure: 2500, heal: .3 }, { n: "Cancer", sev: 25, cure: 60000, heal: 0, minAge: 30, deadly: 1 }, { n: "Maladie cardiaque", sev: 20, cure: 40000, heal: 0, minAge: 45, deadly: 1 },
  { n: "Alzheimer", sev: 15, cure: 99999999, heal: 0, minAge: 65, incurable: 1 }, { n: "Arthrose", sev: 6, cure: 3000, heal: 0, minAge: 50 }, { n: "Insomnie", sev: 5, cure: 200, heal: .2 },
  { n: "Allergies saisonnières", sev: 2, cure: 80, heal: .1 }, { n: "Fracture", sev: 10, cure: 1500, heal: .6 }, { n: "Mononucléose", sev: 8, cure: 150, heal: .5, minAge: 13, maxAge: 30 },
];
LD.vacations = [
  { n: "Week-end à la campagne", p: 300, h: 8 }, { n: "Camping au bord de la mer", p: 600, h: 10 }, { n: "Séjour à Rome", p: 1200, h: 14 }, { n: "Ski dans les Alpes", p: 2000, h: 15 },
  { n: "Road trip aux États-Unis", p: 4500, h: 18 }, { n: "Safari au Kenya", p: 6000, h: 20 }, { n: "Croisière en Méditerranée", p: 3500, h: 16 },
  { n: "Japon, Tokyo et Kyoto", p: 5000, h: 19 }, { n: "Bali, villa privée", p: 7000, h: 20 }, { n: "Maldives, hôtel sur pilotis", p: 15000, h: 25 }, { n: "Voyage dans l'espace (vol suborbital)", p: 450000, h: 40 },
];
LD.crimes = [
  { id: "vol", n: "Vol à l'étalage", ic: "🛒", min: 10, gain: [10, 300], risk: .25, prison: [0, 1], kar: -3 },
  { id: "pick", n: "Pickpocket", ic: "👛", min: 12, gain: [20, 600], risk: .3, prison: [0, 1], kar: -4 },
  { id: "tag", n: "Graffiti / vandalisme", ic: "🎨", min: 10, gain: [0, 0], risk: .25, prison: [0, 1], kar: -2, fun: 6 },
  { id: "cambr", n: "Cambriolage", ic: "🏠", min: 14, gain: [500, 25000], risk: .4, prison: [1, 5], kar: -8 },
  { id: "voiture", n: "Vol de voiture", ic: "🚗", min: 16, gain: [3000, 40000], risk: .45, prison: [1, 6], kar: -8 },
  { id: "deal", n: "Trafic illicite", ic: "💊", min: 16, gain: [2000, 60000], risk: .4, prison: [2, 10], kar: -10 },
  { id: "arnaque", n: "Arnaque en ligne", ic: "💻", min: 15, gain: [500, 80000], risk: .3, prison: [1, 6], kar: -9, intel: 50 },
  { id: "braquage", n: "Braquage de banque", ic: "🏦", min: 18, gain: [50000, 2000000], risk: .65, prison: [8, 25], kar: -15 },
  { id: "bijou", n: "Casse d'une bijouterie", ic: "💎", min: 18, gain: [20000, 500000], risk: .55, prison: [5, 15], kar: -12 },
  { id: "fraude", n: "Fraude fiscale", ic: "📑", min: 21, gain: [5000, 300000], risk: .3, prison: [1, 7], kar: -6, job: 1 },
];
LD.ribbons = {
  riche: ["💰", "Fortuné", "Vous avez amassé une fortune colossale."], celebre: ["⭐", "Célèbre", "Le monde entier connaissait votre nom."],
  criminel: ["🦹", "Hors-la-loi", "Votre casier judiciaire était plus épais qu'un roman."], prison: ["⛓️", "Taulard", "Vous avez passé une bonne partie de votre vie derrière les barreaux."],
  genie: ["🧠", "Génie", "Votre intelligence était hors du commun."], beau: ["😍", "Canon", "Vous avez fait tourner toutes les têtes."],
  heureux: ["😄", "Heureux", "Une vie remplie de joie."], triste: ["😢", "Mélancolique", "La vie ne vous a pas souvent souri."],
  sportif: ["🏃", "Athlète", "Toujours en mouvement, toujours en forme."], romantique: ["💘", "Cœur d'artichaut", "Que d'histoires d'amour !"],
  famille: ["👨‍👩‍👧‍👦", "Patriarche", "Une famille nombreuse et unie."], genereux: ["😇", "Saint", "Votre générosité était légendaire."],
  vieux: ["🎂", "Centenaire", "Vous avez défié le temps."], jeune: ["🥀", "Parti trop tôt", "Une vie trop courte."],
  politique: ["🏛️", "Homme d'État", "Vous avez gouverné votre pays."], ordinaire: ["🙂", "Ordinaire", "Une vie simple, mais la vôtre."],
  diplome: ["🎓", "Érudit", "Diplômes et savoir : votre plus grande fierté."], fetard: ["🎉", "Fêtard", "Pas une soirée sans vous."],
};
