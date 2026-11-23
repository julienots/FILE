package com.superessence.squad.model

import com.superessence.squad.model.Accessory as A
import com.superessence.squad.model.Beard as B
import com.superessence.squad.model.HairStyle as H
import com.superessence.squad.model.Outfit as O

/** Situations dans lesquelles une mascotte parle. */
enum class Say { TAP, RANDOM, HUNGRY, EAT, SLEEPY, WAKE, NIGHT, DRAG, DROP, TRAIN, PET, FRIEND, LOW, FED, TAUNT, WIN, LOSE, OUCH, REPLY, HIGHFIVE, CHASE, FLEE, CAUGHT, BUMPED }

/** Ce que fait la mascotte (utilisé par le moteur et le rendu). */
enum class Act(val label: String) {
    IDLE("Tranquille"), WALK("Se balade"), EAT("Mange"), TRAIN("S'entraîne"), FLEX("Fait le beau"),
    PHONE("Sur son tel"), COFFEE("Café"), WAVE("Salue"), SLEEP("Dort"), YAWN("Baille"),
    DRAGGED("Porté"), TALK("Parle"), PUSHUP("Pompes"),
    RUN("Court"), PUNCH("Cogne"), GUARD("En garde"), HIT("Encaisse"), KO("K.O."), VICTORY("Victoire"), HIGHFIVE("Check")
}

class Archetype(
    val id: String,
    val name: String,
    val title: String,
    val bio: String,
    val traits: List<String>,
    val nick: String,
    val food: List<String>,
    val look: Look,
    /** Poids des activités libres. */
    val hobbies: Map<Act, Int>,
    /** 0.5 = calme, 1.5 = pile électrique. */
    val energy: Float,
    /** Probabilité relative de parler spontanément. */
    val chatty: Float,
    private val lines: Map<Say, List<String>>,
) {
    fun lines(s: Say): List<String> =
        (lines[s].orEmpty() + Fight.lines(id)[s].orEmpty() + Common.lines[s].orEmpty()).map { it.replace("{nick}", nick) }

    /** Envie de se battre (0..1). */
    val aggro: Float get() = Fight.aggro[id] ?: 0.5f
    /** Talent au combat. */
    val skill: Float get() = Fight.skill[id] ?: 1f
}

internal fun l(vararg pairs: Pair<Say, String>): Map<Say, List<String>> =
    pairs.associate { (k, v) -> k to v.split("|").map { it.trim() }.filter { it.isNotEmpty() } }

/** Répliques communes à tous (personnalisées avec le surnom). */
object Common {
    val lines = l(
        Say.HUNGRY to "J'ai la dalle, {nick}.|Mon ventre fait un bruit de moteur V8.|Si on mange pas bientôt, je mords.",
        Say.SLEEPY to "Je pique du nez…|Encore un peu et je m'écroule.|Mes paupières pèsent 20 kilos.",
        Say.NIGHT to "Zzz… quoi ?|Laisse-moi pioncer…|Mmmh… 5 minutes…|Pas maintenant, je dors.",
        Say.DROP to "Ok, ici c'est bien.|Merci pour le taxi.|Atterrissage validé.",
        Say.FED to "Ça, c'est un vrai repas.|Le plein est fait.|Merci {nick}, t'assures.",
        Say.LOW to "Pas la grande forme aujourd'hui…|Faut que je me reprenne.",
        Say.OUCH to "Aïe !|Ouch !|Hmpf !|Ça pique !|Pas la tête !",
        Say.REPLY to "Grave.|Carrément.|Ha ! Ouais.|T'as raison.|Mouais…|N'importe quoi.|Raconte pas ta vie.|Respect.|Pas faux.|Je valide.|T'es sérieux là ?",
        Say.HIGHFIVE to "CHECK !|Tope là !|Bien joué, frérot !|La team !",
        Say.CHASE to "Reviens ici !|Je vais t'attraper !|Cours, cours !",
        Say.FLEE to "Tu m'auras jamais !|Trop lent !|Ha ha, essaie encore !",
        Say.CAUGHT to "Touché !|Chat ! C'est toi !|Attrapé !",
        Say.BUMPED to "Hé ! Tu m'es tombé dessus !|Tu cherches la bagarre ?!|Qui m'a lancé ce type ?!",
    )
}

object Archetypes {
    private fun look(skin: Int, hairC: Int, hair: H, beard: B, outfit: O, oc: Long, pc: Long, acc: A, tat: Boolean = false, scar: Boolean = false) =
        Look(Palette.skins[skin], Palette.hairs[hairC], hair, beard, outfit, oc.toInt(), pc.toInt(), acc, tat, scar)

    val all: List<Archetype> = listOf(
        Archetype(
            "tank", "Tank", "Le Bodybuilder",
            "Vit pour la fonte et le poulet-riz. Compte ses protéines au gramme près et te pousse à te dépasser.",
            listOf("Motivé", "Bourrin", "Loyal"), "champion", listOf("poulet-riz", "shaker protéiné", "6 œufs durs", "steak haché"),
            look(2, 0, H.CREW, B.STUBBLE, O.TANK, 0xFF151515, 0xFF2E2E2E, A.NONE, tat = true),
            mapOf(Act.TRAIN to 5, Act.FLEX to 4, Act.PUSHUP to 4, Act.WALK to 3, Act.IDLE to 1), 1.3f, 1.0f,
            l(
                Say.TAP to "Touche pas, ça pompe !|T'as vu ces épaules ?|Pas de jour off, {nick}.|On lâche rien.|Tu soulèves combien au développé ?",
                Say.RANDOM to "Leg day aujourd'hui. Pas de discussion.|La douleur, c'est la faiblesse qui quitte le corps.|Une série de plus. Toujours.|Hydrate-toi, {nick}.|Je sens la pompe arriver…|Le canapé, c'est l'ennemi.",
                Say.EAT to "Protéines : chargées.|Miam, 40 g de prot' minimum.|Le muscle se construit à table.",
                Say.WAKE to "Debout ! Les haltères nous attendent.|Nouveau jour, nouveau record.",
                Say.DRAG to "Eh ! Je pèse 110 kilos de muscle !|Doucement avec la marchandise !",
                Say.TRAIN to "ALLEZ ! Encore une !|Ça brûle, c'est que ça marche !|8… 9… 10 !",
                Say.PET to "Respect, frère.|Check !",
                Say.FRIEND to "Tu fais de la muscu toi ?|Spotte-moi, frérot.",
            ),
        ),
        Archetype(
            "ragnar", "Ragnar", "Le Viking",
            "Débarqué du Nord avec sa hache et sa barbe tressée. Parle fort, rit fort, mange fort.",
            listOf("Intrépide", "Fêtard", "Fier"), "jarl", listOf("cuissot de sanglier", "pain noir et hydromel", "saumon fumé"),
            look(0, 3, H.LONG, B.VIKING, O.FUR, 0xFF5C3A21, 0xFF3B2418, A.HORNS),
            mapOf(Act.WALK to 4, Act.FLEX to 3, Act.TRAIN to 2, Act.IDLE to 2, Act.TALK to 2), 1.2f, 1.2f,
            l(
                Say.TAP to "Par Odin !|Skål, {nick} !|Qui ose toucher Ragnar ?|Ha ! Tu as du cran.|Mon bouclier est à ta disposition.",
                Say.RANDOM to "Le Valhalla attendra, j'ai encore à faire.|Où est le festin ?|Les drakkars me manquent.|Un vrai guerrier ne craint que l'ennui.|Cette tablette est mon nouveau royaume.|Je sens l'orage venir. J'adore.",
                Say.EAT to "UN FESTIN DIGNE DES DIEUX !|Ha ! Du gras, de la viande, du bonheur.",
                Say.WAKE to "Le soleil se lève sur mes terres !|Debout, guerrier !",
                Say.DRAG to "Pose-moi, ou tu goûteras de ma hache !|Je vole comme une Valkyrie !",
                Say.TRAIN to "POUR ODIN !|Que les dieux voient ma force !",
                Say.PET to "Tu mérites une place à ma table.|Frère d'armes !",
                Say.FRIEND to "Toi ! On fait un bras de fer ?|Ce soir on festoie ensemble.",
            ),
        ),
        Archetype(
            "boss", "Vincent", "Le Boss",
            "Costard impeccable, lunettes noires, calme olympien. Il ne court jamais : ce sont les autres qui courent.",
            listOf("Calme", "Stratège", "Classe"), "boss", listOf("entrecôte saignante", "risotto aux truffes", "un espresso serré"),
            look(1, 0, H.SLICK, B.FULL, O.SUIT, 0xFF151515, 0xFF151515, A.SHADES),
            mapOf(Act.PHONE to 5, Act.IDLE to 4, Act.WALK to 2, Act.COFFEE to 2), 0.7f, 0.7f,
            l(
                Say.TAP to "On ne touche pas au costume.|J'ai un appel dans 2 minutes.|Je t'écoute.|Tout est sous contrôle.|Pas de stress, {nick}.",
                Say.RANDOM to "Le business ne dort jamais. Moi si.|Une offre qu'on ne peut pas refuser…|La patience paye toujours.|J'ai acheté ton fond d'écran. Il est à moi maintenant.|Les détails font la différence.|Silence. Je réfléchis.",
                Say.EAT to "Cuisson parfaite. Le chef aura un pourboire.|La qualité, toujours.",
                Say.WAKE to "Café. Noir. Tout de suite.|Les marchés ouvrent, on y va.",
                Say.DRAG to "Tu froisses la veste, là.|Repose-moi. Calmement.",
                Say.PET to "J'apprécie.|Tu iras loin, {nick}.",
                Say.FRIEND to "On a des affaires à régler.|Bon travail, associé.",
            ),
        ),
        Archetype(
            "rex", "Rex", "Le Biker",
            "Cuir, chrome et grosse cylindrée. Un dur au cœur d'or qui ne jure que par la route.",
            listOf("Rebelle", "Loyal", "Brut"), "frangin", listOf("burger double bacon", "chili con carne", "côtes de porc BBQ"),
            look(2, 5, H.BUZZ, B.CHEVRON, O.LEATHER, 0xFF151515, 0xFF1F3A5F, A.BANDANA, tat = true, scar = true),
            mapOf(Act.WALK to 4, Act.IDLE to 3, Act.FLEX to 2, Act.PHONE to 1), 1.0f, 0.9f,
            l(
                Say.TAP to "T'as un problème, {nick} ?|Le cuir, ça se respecte.|Vroom vroom.|Tranquille, frangin.|On roule ce week-end ?",
                Say.RANDOM to "La route m'appelle.|Ma bécane me manque.|Rien ne vaut le bruit d'un V-twin.|Pas de GPS, juste l'instinct.|Le vent dans la face, voilà la vie.|Faut que je change l'huile.",
                Say.EAT to "Ça c'est de la bouffe de routier.|Double bacon. Évidemment.",
                Say.WAKE to "On met les gaz.|Café et route.",
                Say.DRAG to "Lâche-moi le blouson !|Je suis pas un sac de selle !",
                Say.PET to "T'es réglo, toi.|Respect, frangin.",
                Say.FRIEND to "Belle machine que t'as là… ah non.|Ride avec moi, mec.",
            ),
        ),
        Archetype(
            "sarge", "Sergent Kowal", "Le Militaire",
            "Discipline, rigueur, horaires au cordeau. Il te réveille au clairon et te fait faire des pompes.",
            listOf("Discipliné", "Protecteur", "Strict"), "soldat", listOf("ration de combat", "gamelle de pâtes", "steak-purée"),
            look(3, 0, H.CREW, B.NONE, O.CAMO, 0xFF4B5320, 0xFF4B5320, A.BERET, scar = true),
            mapOf(Act.PUSHUP to 5, Act.WALK to 4, Act.TRAIN to 3, Act.IDLE to 1), 1.1f, 1.0f,
            l(
                Say.TAP to "À vos ordres !|Garde-à-vous, {nick} !|Rompez.|Je surveille le périmètre.|Rien à signaler, chef.",
                Say.RANDOM to "Une ronde de plus.|20 pompes, exécution !|La discipline, c'est la liberté.|L'ennemi ne dort jamais. Moi un peu.|Rangée, ta page d'accueil. Bien.|Repos… mais pas trop.",
                Say.EAT to "Ration avalée en 90 secondes.|Mange vite, mange bien.",
                Say.WAKE to "DEBOUT LÀ-DEDANS !|Réveil à l'heure. Parfait.",
                Say.DRAG to "Exfiltration non autorisée !|Soldat en déplacement !",
                Say.TRAIN to "UN ! DEUX ! UN ! DEUX !|On ne s'arrête pas !",
                Say.PET to "Bon soldat.|Mission accomplie.",
                Say.FRIEND to "Toi, au rapport !|En formation, les gars !",
            ),
        ),
        Archetype(
            "kenji", "Kenji", "Le Samouraï",
            "Maître du sabre et du silence. Chaque geste est mesuré. Ses conseils tombent comme des haïkus.",
            listOf("Sage", "Patient", "Redoutable"), "disciple", listOf("bol de ramen", "sushis", "riz et poisson grillé", "thé vert"),
            look(1, 0, H.BUN, B.GOATEE, O.GI, 0xFF1F3A5F, 0xFF151515, A.NONE),
            mapOf(Act.TRAIN to 4, Act.IDLE to 4, Act.WALK to 2, Act.COFFEE to 1), 0.6f, 0.6f,
            l(
                Say.TAP to "…|Le calme est une arme.|Respire, {nick}.|La lame la plus affûtée reste au fourreau.|Hm.",
                Say.RANDOM to "Celui qui maîtrise sa respiration maîtrise sa vie.|Le vent ne lutte pas contre la montagne.|Une notification de moins, une pensée de plus.|L'eau calme reflète la lune.|Mille coupes pour une seule parfaite.",
                Say.EAT to "Itadakimasu.|Chaque grain de riz compte.",
                Say.WAKE to "L'aube est une promesse.|Le sabre d'abord, le thé ensuite.",
                Say.DRAG to "Je… me laisse porter par le courant.|Intéressant.",
                Say.TRAIN to "Hai !|Kiai !|Une coupe. Une seule.",
                Say.PET to "Arigatō.|Tu progresses, {nick}.",
                Say.FRIEND to "Croisons le fer, ami.|Méditons ensemble.",
            ),
        ),
        Archetype(
            "ace", "Ace", "Le Pilote",
            "Pilote de chasse, sourire en coin et blouson aviateur. Toujours une anecdote à 3 000 pieds.",
            listOf("Charmeur", "Casse-cou", "Drôle"), "copilote", listOf("burger-frites", "pizza", "hot-dog de base aérienne"),
            look(1, 2, H.SLICK, B.MUSTACHE, O.BOMBER, 0xFF5C3A21, 0xFF4B5320, A.AVIATORS),
            mapOf(Act.WALK to 4, Act.WAVE to 3, Act.PHONE to 2, Act.FLEX to 2, Act.IDLE to 2), 1.2f, 1.3f,
            l(
                Say.TAP to "Ace au rapport, {nick} !|Prêt au décollage.|Tour de contrôle, ici Ace.|T'as besoin d'un ailier ?|Je sens le besoin… de vitesse.",
                Say.RANDOM to "Altitude de croisière atteinte.|J'ai fait un tonneau sur ton écran, t'as vu ?|La gravité, c'est pour les autres.|Mach 2 avant le petit-déj.|Ciel dégagé, moral au top.|Mayday… je m'ennuie.",
                Say.EAT to "Ravitaillement en vol réussi !|Carburant premium.",
                Say.WAKE to "Briefing matinal, on décolle !|Pleins gaz !",
                Say.DRAG to "WOUHOU ! Turbulences !|Éjection ! Éjection !",
                Say.PET to "T'es mon ailier préféré.|Bien joué, copilote.",
                Say.FRIEND to "On vole en formation ?|Salut l'équipage !",
            ),
        ),
        Archetype(
            "jo", "Big Jo", "Le Bûcheron",
            "Deux mètres de flanelle et de barbe. Coupe du bois, fait du feu, parle peu mais bien.",
            listOf("Calme", "Costaud", "Nature"), "gamin", listOf("ragoût de bœuf", "pancakes au sirop d'érable", "truite grillée au feu de bois"),
            look(1, 3, H.CURLY, B.FULL, O.FLANNEL, 0xFF8B1E1E, 0xFF1F3A5F, A.BEANIE),
            mapOf(Act.IDLE to 4, Act.WALK to 3, Act.COFFEE to 3, Act.TRAIN to 2), 0.8f, 0.7f,
            l(
                Say.TAP to "Ouais ?|Salut, {nick}.|Tranquille.|Ça sent le feu de bois ici, non ?|Tout doux.",
                Say.RANDOM to "Faudrait que je coupe du bois.|Un bon feu, une bonne soupe, voilà.|Les arbres, ça écoute mieux que les gens.|J'ai vu un élan hier. Il m'a regardé de travers.|Pas besoin de Wi-Fi en forêt.|Hmm. Il fait frais.",
                Say.EAT to "Ça tient au corps.|Du sirop d'érable sur tout.",
                Say.WAKE to "Debout avec les oiseaux.|Café de cow-boy, noir comme la nuit.",
                Say.DRAG to "Hé ! Je suis pas une bûche !|Ouh là, ça tangue.",
                Say.PET to "T'es un bon gars.|Merci, gamin.",
                Say.FRIEND to "On se fait un feu ?|Salut l'ami.",
            ),
        ),
        Archetype(
            "marco", "Marco", "Le Roi du Grill",
            "Maître du barbecue, tablier taché et pince à la main. Pour lui, tout problème se règle autour d'une côte de bœuf.",
            listOf("Généreux", "Gourmand", "Râleur"), "mon grand", listOf("côte de bœuf", "brochettes", "merguez-frites", "travers de porc"),
            look(3, 0, H.BALD, B.CHEVRON, O.APRON, 0xFF8B1E1E, 0xFF2E2E2E, A.NONE, tat = true),
            mapOf(Act.EAT to 3, Act.IDLE to 3, Act.WALK to 2, Act.TALK to 3), 0.9f, 1.2f,
            l(
                Say.TAP to "Elle est bien cuite, hein ?|Touche pas à mon grill !|Tu restes manger, {nick} ?|Saignant, à point, ou bien cuit ?|Moi je dis : plus de charbon.",
                Say.RANDOM to "Qui veut une merguez ?|Le secret, c'est la marinade.|Le gaz ? Jamais. Charbon de bois.|Ça manque de sauce ici.|Un barbecue sans potes, c'est triste.|J'ai faim. Encore.",
                Say.EAT to "MAGNIFIQUE. Je pleure.|Ça, c'est de la cuisine !|Ressers-moi, s'il te plaît.",
                Say.WAKE to "On allume les braises ?|Petit-déj copieux, journée réussie.",
                Say.DRAG to "Attention, j'ai la pince à la main !|Pas si vite, je vais renverser la sauce !",
                Say.HUNGRY to "C'est quand qu'on mange ?!|J'ai faim. Et quand j'ai faim, je râle.",
                Say.PET to "Toi t'auras la meilleure côte.|Viens là mon grand !",
                Say.FRIEND to "Je t'ai gardé une brochette.|Barbeuc' ce soir ?",
            ),
        ),
        Archetype(
            "rocco", "Rocco", "Le Boxeur",
            "Nez cassé, poings d'acier, cœur gros comme ça. S'entraîne à toute heure et ne recule devant rien.",
            listOf("Combatif", "Têtu", "Généreux"), "champion", listOf("pâtes bolo", "steak et patates", "smoothie banane"),
            look(4, 0, H.BUZZ, B.STUBBLE, O.BARE, 0xFFC0392B, 0xFFC0392B, A.HEADBAND, scar = true),
            mapOf(Act.TRAIN to 5, Act.PUSHUP to 3, Act.FLEX to 2, Act.WALK to 3), 1.4f, 1.0f,
            l(
                Say.TAP to "Garde haute, {nick} !|Jab, jab, crochet !|Tu veux un round ?|Je suis jamais tombé.|Allez, on se bouge !",
                Say.RANDOM to "C'est pas la force, c'est le cœur.|Encore un round.|Corde à sauter, 15 minutes.|Le ring, c'est ma maison.|Esquive. Contre. Victoire.|Je sens que c'est mon jour.",
                Say.EAT to "Énergie pour 12 rounds.|Ça, ça donne de la frappe.",
                Say.WAKE to "Footing à l'aube, comme un pro !|Debout, champion !",
                Say.DRAG to "Hé, c'est pas réglementaire !|Arbitre ! Il me porte !",
                Say.TRAIN to "Pam ! Pam ! Pam !|Plus vite ! Plus fort !",
                Say.PET to "T'as du cœur toi.|Champion !",
                Say.FRIEND to "On fait un sparring ?|Garde haute, mon pote.",
            ),
        ),
        Archetype(
            "zero", "Zéro", "Le Hacker",
            "Capuche, casque et trois écrans. Il connaît ton téléphone mieux que toi et te le fait savoir.",
            listOf("Malin", "Nocturne", "Pince-sans-rire"), "user", listOf("pizza froide", "boisson énergisante", "nouilles instantanées"),
            look(0, 0, H.CREW, B.STUBBLE, O.HOODIE, 0xFF2E2E2E, 0xFF151515, A.HEADPHONES),
            mapOf(Act.PHONE to 6, Act.IDLE to 3, Act.WALK to 2, Act.COFFEE to 2), 0.8f, 1.0f,
            l(
                Say.TAP to "Accès refusé.|Ping reçu.|Hello, {nick}.|J'optimise ta batterie. Ou pas.|sudo laisse-moi tranquille.",
                Say.RANDOM to "Ton mot de passe, c'est 1234, non ?|Ton téléphone a 47 onglets ouverts. Juste ça.|Compilation en cours…|Il n'y a pas de bug, que des fonctionnalités.|J'ai trouvé une faille… dans le frigo.|Mode furtif activé.",
                Say.EAT to "Carburant de développeur.|Calories : téléchargées.",
                Say.WAKE to "Reboot du système…|Café.exe lancé.",
                Say.DRAG to "Déplacement de fichier en cours…|Erreur 404 : sol introuvable.",
                Say.PET to "Connexion établie.|Ami ajouté.",
                Say.FRIEND to "T'as rebooté récemment ?|Ping… pong.",
            ),
        ),
        Archetype(
            "duke", "Duke", "Le Cowboy",
            "Chapeau, moustache et regard d'acier. Il parle lentement, tire vite, et ne laisse jamais un ami derrière.",
            listOf("Juste", "Flegmatique", "Solitaire"), "partenaire", listOf("haricots au lard", "steak au feu de camp", "café noir"),
            look(2, 1, H.CREW, B.MUSTACHE, O.VEST, 0xFF5C3A21, 0xFF1F3A5F, A.COWBOY),
            mapOf(Act.IDLE to 4, Act.WALK to 3, Act.COFFEE to 3, Act.WAVE to 1), 0.7f, 0.7f,
            l(
                Say.TAP to "Howdy, {nick}.|Y'a pas la place pour nous deux sur cet écran.|Tranquille, partenaire.|Hmm-hmm.|Doucement sur la gâchette.",
                Say.RANDOM to "Le soleil tape fort aujourd'hui.|Mon cheval me manque.|Un homme, c'est sa parole.|Cette ville est trop petite.|J'ai vu passer un virevoltant.|Le silence vaut de l'or.",
                Say.EAT to "Comme au ranch.|Ça, ça tient un cowboy debout.",
                Say.WAKE to "Le coq a chanté, partenaire.|On selle les chevaux.",
                Say.DRAG to "Hé ! J'suis pas un veau au lasso !|Doucement, l'ami !",
                Say.PET to "T'es un bon gars, partenaire.|Merci bien.",
                Say.FRIEND to "Howdy, étranger.|On partage le feu ?",
            ),
        ),
    )

    fun byId(id: String): Archetype = all.firstOrNull { it.id == id } ?: all.first()
}
