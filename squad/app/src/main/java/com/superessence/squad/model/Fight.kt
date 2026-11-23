package com.superessence.squad.model

/** Caractère de combat de chaque archétype et ses répliques de baston. */
object Fight {
    val aggro = mapOf(
        "tank" to 0.7f, "ragnar" to 0.95f, "boss" to 0.3f, "rex" to 0.75f, "sarge" to 0.6f, "kenji" to 0.35f,
        "ace" to 0.45f, "jo" to 0.3f, "marco" to 0.45f, "rocco" to 0.95f, "zero" to 0.1f, "duke" to 0.55f,
    )
    val skill = mapOf(
        "tank" to 1.2f, "ragnar" to 1.2f, "boss" to 1.0f, "rex" to 1.1f, "sarge" to 1.15f, "kenji" to 1.25f,
        "ace" to 1.0f, "jo" to 1.15f, "marco" to 0.95f, "rocco" to 1.3f, "zero" to 0.8f, "duke" to 1.05f,
    )

    private val byId: Map<String, Map<Say, List<String>>> = mapOf(
        "tank" to l(
            Say.TAUNT to "Viens, je te soulève comme une barre de 20 !|T'as mangé tes protéines ce matin ?|Approche si t'as les épaules.",
            Say.WIN to "C'est ça, 5 ans de squat.|Musculation : 1, toi : 0.|Va boire un shaker et reviens.",
            Say.LOSE to "J'avais pas échauffé…|Leg day hier, ça compte pas.",
        ),
        "ragnar" to l(
            Say.TAUNT to "PAR ODIN, BATS-TOI !|Ta tête ira orner mon drakkar !|Le Valhalla t'attend, petit !",
            Say.WIN to "VICTOIRE ! Qu'on serve l'hydromel !|Les dieux ont vu ma force !|Ha ! Une saga de plus.",
            Say.LOSE to "Un vrai guerrier se relève toujours…|Odin, pourquoi ?",
        ),
        "boss" to l(
            Say.TAUNT to "Tu fais une grosse erreur.|Je vais te faire une offre… de raclée.|Retire tes lunettes ? Non. Les miennes restent.",
            Say.WIN to "Rien de personnel. Juste les affaires.|Ajuste ta cravate, c'est fini.|Le boss reste le boss.",
            Say.LOSE to "Mon avocat va t'appeler.|Tu as froissé le costume. Tu vas payer.",
        ),
        "rex" to l(
            Say.TAUNT to "Tu veux tâter du cuir, frangin ?|On règle ça à l'ancienne.|J'ai mangé du bitume plus dur que toi.",
            Say.WIN to "Remonte sur ta trottinette.|Rex 1, le reste 0.|Ça c'est le code de la route.",
            Say.LOSE to "Ok… t'as gagné le premier round.|J'ai glissé sur une flaque d'huile.",
        ),
        "sarge" to l(
            Say.TAUNT to "Position de combat, soldat !|Tu vas faire 100 pompes après ça.|En garde, recrue !",
            Say.WIN to "Exercice terminé. Rompez.|Neutralisé.|Discipline : 1, chaos : 0.",
            Say.LOSE to "Repli stratégique…|Je… note ça dans le rapport.",
        ),
        "kenji" to l(
            Say.TAUNT to "Ton esprit est agité.|Je ne veux pas me battre… mais je le ferai.|Le premier qui frappe a déjà perdu.",
            Say.WIN to "La colère est un mauvais maître.|Tu as encore beaucoup à apprendre.|Hm. Respire.",
            Say.LOSE to "Même le singe tombe de l'arbre.|Belle leçon.",
        ),
        "ace" to l(
            Say.TAUNT to "Fox Two, j'engage !|Prépare-toi à un piqué.|Tu vas voir ce qu'est un vrai tonneau.",
            Say.WIN to "Cible abattue !|Et en plus je suis coiffé.|Ace : toujours au top.",
            Say.LOSE to "Mayday, mayday…|J'ai décroché. Panne moteur.",
        ),
        "jo" to l(
            Say.TAUNT to "Tu veux vraiment ça, gamin ?|J'ai fendu des chênes moins durs que toi.|Bon. Allez.",
            Say.WIN to "Timber.|Va t'asseoir près du feu.|Pas méchant, hein.",
            Say.LOSE to "Ouh. Ça tape.|Bon… ça arrive.",
        ),
        "marco" to l(
            Say.TAUNT to "Je vais t'attendrir comme une bavette !|Tu veux goûter de la pince ?|Je te mets sur le grill !",
            Say.WIN to "Bien saisi, à point !|Ça, c'est de la cuisson !|Allez, on mange maintenant ?",
            Say.LOSE to "Tu m'as fait tomber ma merguez…|C'est pas juste, j'avais faim !",
        ),
        "rocco" to l(
            Say.TAUNT to "Gants en l'air, champion !|Ding ding ! Premier round !|Tu vas manger mon crochet du gauche.",
            Say.WIN to "K.O. technique !|Et toujours champion !|Mon crochet, mon meilleur ami.",
            Say.LOSE to "Je me relève toujours. TOUJOURS.|L'arbitre a rien vu !",
        ),
        "zero" to l(
            Say.TAUNT to "Je vais te DDoS la tête.|Euh… on peut en parler ?|Tu sais que j'ai ton historique ?",
            Say.WIN to "Exploit réussi.|Wait… j'ai gagné ? GG.|Patch appliqué.",
            Say.LOSE to "Erreur 500.|Ragequit.|Je retourne dans mon terminal.",
        ),
        "duke" to l(
            Say.TAUNT to "Dégaine, étranger.|Cette ville est trop petite pour nous deux.|À midi pile.",
            Say.WIN to "Le plus rapide de l'Ouest.|Rentre au ranch, gamin.|Hmm-hmm.",
            Say.LOSE to "J'ai le soleil dans les yeux…|Bien joué, l'ami. Bien joué.",
        ),
    )

    fun lines(id: String): Map<Say, List<String>> = byId[id].orEmpty()

    fun relLabel(v: Int) = when {
        v >= 60 -> "Meilleur pote"
        v >= 20 -> "Pote"
        v > -20 -> "Connaissance"
        v > -60 -> "Rival"
        else -> "Ennemi juré"
    }
}
