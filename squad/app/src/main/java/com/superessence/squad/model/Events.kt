package com.superessence.squad.model

/** Matériel posé sur l'écran pendant les séances et les fêtes (tailles en dp). */
enum class PropType(val label: String, val w: Int, val h: Int, val gym: Boolean) {
    BENCH("le banc", 150, 95, true),
    PULLEY("la poulie", 120, 150, true),
    RACK("le rack à squat", 130, 150, true),
    BAG("le sac de frappe", 140, 150, true),
    DUMBBELLS("les haltères", 120, 150, true),
    SPEAKER("la sono", 70, 130, false),
    TABLE("le buffet", 130, 95, false),
    DISCO("la boule à facettes", 90, 110, false),
}

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
    )
    private val byId = mapOf(
        "ragnar" to l(Say.CHEERS to "SKÅL !|À Odin et à la squad !", Say.PARTY to "QU'ON APPORTE L'HYDROMEL !"),
        "marco" to l(Say.CHEERS to "Qui veut une merguez ?|Servez-vous, y en a pour tout le monde !", Say.PARTY to "J'allume le barbeuc' !"),
        "zero" to l(Say.DANCE to "Le robot, c'est ma spécialité.|bip… boup… groove.exe"),
        "tank" to l(Say.SPOT to "Allez champion, c'est TA série !|Encore une, j'ai dit !", Say.GYM_START to "Tout le monde à la fonte !"),
        "rocco" to l(Say.SPOT to "Les poings serrés, souffle !|Allez, round 12 !"),
        "sarge" to l(Say.GYM_START to "Rassemblement ! Montez le matériel, exécution !", Say.SPOT to "Je ne vous entends pas !|Encore dix !"),
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
