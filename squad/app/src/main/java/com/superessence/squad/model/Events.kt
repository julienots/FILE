package com.superessence.squad.model

/** Matériel posé sur l'écran pendant les séances et les fêtes (tailles en dp). */
enum class PropType(val label: String, val w: Int, val h: Int, val gym: Boolean, val street: Boolean = false, val pr: String? = null) {
    BENCH("le banc", 150, 95, true, pr = "Développé couché"),
    PULLEY("la poulie", 120, 150, true),
    RACK("le rack à squat", 130, 150, true, pr = "Squat"),
    BAG("le sac de frappe", 140, 150, true),
    DUMBBELLS("les haltères", 120, 150, true),
    TREADMILL("le tapis de course", 150, 120, true),
    LEG_PRESS("la presse à cuisses", 150, 120, true, pr = "Presse"),
    ROWER("le rameur", 160, 90, true),
    PEC_DECK("le pec deck", 120, 150, true),
    PULLUP_BAR("la barre de traction", 120, 170, true, street = true),
    DIP_BARS("les barres parallèles", 120, 140, true, street = true),
    SPEAKER("la sono", 70, 130, false),
    TABLE("le buffet", 130, 95, false),
    DISCO("la boule à facettes", 90, 110, false),
}

/** Bordel laissé par terre (dp). */
enum class Clutter(val w: Int, val h: Int) { DUMBBELL(46, 22), PLATE(34, 34), KETTLEBELL(30, 34), SHAKER(18, 34), TOWEL(40, 18), BAR(90, 18) }

/** Taches de bouffe sur l'écran. */
enum class Stain(val color: Long) { KETCHUP(0xFFC62828), MUSTARD(0xFFF9C80E), BBQ(0xFF5D2E0C), GREASE(0x99C8A951), CRUMBS(0xFFD7A86E), SHAKE(0xEEF3EFE6), SODA(0xCC4E8B2F), CHOCO(0xFF4E2A12) }

/** Répliques des séances collectives et des fêtes. */
object Events {
    val common = l(
        Say.GYM_START to "Séance collective ! Ramenez le matos !|Allez les gars, on monte une salle ici !|C'est l'heure de pousser, tout le monde au matos !",
        Say.CARRY to "Je ramène du matos !|Attention, ça passe !|C'est lourd, mais c'est bon pour les bras.|Place, place !",
        Say.SPOT to "Allez, pousse !|Encore deux !|C'est toi qui soulèves, pas moi !|Contrôle la descente !|Respire !|Ça monte, ça monte !|Je te pare, vas-y !",
        Say.GYM_END to "Bonne séance, les gars !|Ça, c'est fait.|On range le matos.|Pompe de fou.",
        Say.PARTY to "C'EST LA FÊTE !|Monte le son !|Ce soir on lâche tout !|La squad est en soirée !",
        Say.DANCE to "Regarde-moi ce jeu de jambes !|Ça c'est mon son !|Qui m'arrête ?|Ouuuh !|Allez la piste !",
        Say.CHEERS to "Santé !|Tchin !|À la squad !|À nous !",
        Say.PARTY_END to "Quelle soirée…|On remet ça quand vous voulez.|Je sens plus mes pieds.|Allez, on range.",
        Say.PR_TRY to "Je tente un PR. Quelqu'un me pare ?|Aujourd'hui je bats mon record.|Mettez-moi des disques !|C'est le jour du PR.",
        Say.PR_SPOT to "TOUT TOI ! TOUT TOI !|POUSSE ! POUSSE !|ALLEZ ! ALLEZ ! ALLEZ !|J'Y TOUCHE PAS, C'EST TOI !|MONTE ! MONTE !",
        Say.PR_WIN to "AAAAAAAAH ! PR !!!|NOUVEAU RECORD !!!|JE SUIS UNE BÊTE !!!|C'EST QUI LE PATRON ?!",
        Say.PR_CHEER to "LET'S GOOOOO !|LA BÊTE !!!|ÇA C'EST UN PR !|MONSTRE !|INCROYABLE, FRÉROT !",
        Say.PR_FAIL to "Raaah… pas aujourd'hui.|La prochaine fois, elle monte.|J'étais à deux centimètres…",
        Say.PR_SAVE to "JE L'AI ! JE L'AI !|Je te la remonte !|Doucement, je tiens !",
        Say.BURP to "BUUUUURP !|*rot sonore*|Pardon. Non, en fait, pas pardon.|BRÔÔÔÔT !",
        Say.STREET_START to "Street workout, les gars ! On monte les barres !|Pas besoin de salle, on a les barres !|Session street, tout le monde dehors !",
        Say.STREET to "Regarde ce front lever !|Muscle-up propre, sans élan.|L'équilibre, c'est dans la tête.|Le poids du corps, y a que ça de vrai.|Gainage, gainage !",
        Say.CLEAN to "C'est moi qui nettoie, comme d'hab.|Allez, on range ce bordel.|Qui a mis du ketchup partout ?!|Coup d'éponge !",
        Say.SNORE to "Rrrrrrr…|ZZZRRRRR…|*ronfle comme un tracteur*",
        Say.SHAKER to "Shaker, glou glou.|Fenêtre anabolique !|Protéines vanille. La meilleure.",
    )
    private val byId = mapOf(
        "ragnar" to l(Say.CHEERS to "SKÅL !|À Odin et à la squad !", Say.PARTY to "QU'ON APPORTE L'HYDROMEL !", Say.PR_WIN to "ODIN, TU AS VU ÇA ?!!!", Say.BURP to "BRÔÔÔÔÔÔÔÔT ! Pour Odin."),
        "marco" to l(Say.CHEERS to "Qui veut une merguez ?|Servez-vous, y en a pour tout le monde !", Say.PARTY to "J'allume le barbeuc' !"),
        "zero" to l(Say.DANCE to "Le robot, c'est ma spécialité.|bip… boup… groove.exe"),
        "tank" to l(Say.PR_WIN to "200 KILOS, BÉBÉ !!!|C'EST ÇA LA FONTE !!!", Say.SPOT to "Allez champion, c'est TA série !|Encore une, j'ai dit !", Say.GYM_START to "Tout le monde à la fonte !"),
        "rocco" to l(Say.SPOT to "Les poings serrés, souffle !|Allez, round 12 !", Say.STREET to "Comme à la salle de boxe du quartier."),
        "jo" to l(Say.BURP to "*rot de bûcheron qui fait trembler les arbres*"),
        "sarge" to l(Say.CLEAN to "Corvée de nettoyage. Personne ne bouge, je m'en occupe.|Ce poste est une PORCHERIE !", Say.GYM_START to "Rassemblement ! Montez le matériel, exécution !", Say.SPOT to "Je ne vous entends pas !|Encore dix !"),
        "kenji" to l(Say.DANCE to "Même la danse est un art martial."),
        "boss" to l(Say.CHEERS to "Au succès.|Champagne pour tout le monde."),
        "duke" to l(Say.DANCE to "Danse en ligne, partenaire !"),
        "ace" to l(Say.DANCE to "Mon moonwalk est supersonique !"),
    )

    fun lines(id: String): Map<Say, List<String>> {
        val own = byId[id].orEmpty()
        return common.mapValues { (k, v) -> own[k].orEmpty() + v } + own.filterKeys { it !in common }
    }

    fun sporty(m: Mascot) = m.archetype.hobbies.containsKey(Act.TRAIN) || m.archetype.hobbies.containsKey(Act.PUSHUP) || m.fitness >= 50f
}
