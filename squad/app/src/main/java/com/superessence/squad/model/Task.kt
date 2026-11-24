package com.superessence.squad.model

import org.json.JSONArray
import org.json.JSONObject

/** Métiers qu'on peut confier à une mascotte. */
enum class TaskKind(val label: String, val desc: String, val posted: Boolean) {
    NONE("Libre", "Il vit sa vie : il se balade, s'entraîne, discute, fait la fête.", false),
    CLEANER("Agent d'entretien", "Dès qu'une tache ou du matos traîne sur l'écran, il prend l'éponge et range.", false),
    LAUNCHER("Raccourci d'appli", "Il tient une pancarte avec l'icône d'une appli. Tu le touches, il l'ouvre.", true),
    TAPPER("Cliqueur", "Il va cliquer pour de vrai à l'endroit de ton choix (une icône, un bouton) : quand tu le touches, toutes les X minutes ou à heure fixe.", true),
    CLOCK("Horloge", "Il tient l'heure sur une pancarte et l'annonce à chaque heure pile.", true),
    COACH("Coach rappels", "Il te rappelle de boire, de bouger, de manger, d'aller dormir, et tes rappels perso.", true),
    BATTERY("Surveillant batterie", "Il affiche le niveau de batterie et râle quand elle est faible.", true),
    GUARD("Vigile", "Il garde une zone de ton écran : les autres gars n'ont pas le droit d'y mettre les pieds.", true),
    NOTE("Pense-bête", "Il tient une note que tu écris et te la rappelle de temps en temps.", true),
}

data class Reminder(val at: Int, val text: String)

data class Task(
    val kind: TaskKind = TaskKind.NONE,
    /** Poste de travail (fraction de l'écran), -1 = là où il est. */
    val postX: Float = -1f,
    val postY: Float = -1f,
    // Raccourci d'appli
    val pkg: String = "",
    val appLabel: String = "",
    // Cliqueur
    val tapX: Float = -1f,
    val tapY: Float = -1f,
    val everyMin: Int = 0,
    val atMin: Int = -1,
    // Pense-bête
    val note: String = "",
    // Coach
    val water: Boolean = true,
    val move: Boolean = true,
    val meals: Boolean = true,
    val bedtime: Boolean = true,
    val reminders: List<Reminder> = emptyList(),
) {
    fun toJson(): JSONObject = JSONObject()
        .put("kind", kind.name).put("postX", postX.toDouble()).put("postY", postY.toDouble())
        .put("pkg", pkg).put("appLabel", appLabel).put("tapX", tapX.toDouble()).put("tapY", tapY.toDouble())
        .put("everyMin", everyMin).put("atMin", atMin).put("note", note)
        .put("water", water).put("move", move).put("meals", meals).put("bedtime", bedtime)
        .put("reminders", JSONArray().apply { reminders.forEach { put(JSONObject().put("at", it.at).put("text", it.text)) } })

    val summary: String
        get() = when (kind) {
            TaskKind.LAUNCHER -> if (appLabel.isNotEmpty()) "Ouvre $appLabel" else "Choisis une appli"
            TaskKind.TAPPER -> if (tapX < 0f) "Choisis l'endroit à cliquer" else buildString {
                append("Clique")
                if (everyMin > 0) append(" toutes les $everyMin min")
                if (atMin >= 0) append(" à ${fmtMin(atMin)}")
                if (everyMin <= 0 && atMin < 0) append(" quand tu le touches")
            }
            TaskKind.NOTE -> note.ifEmpty { "Écris ta note" }
            TaskKind.COACH -> "${listOf(water to "eau", move to "bouger", meals to "repas", bedtime to "dodo").filter { it.first }.joinToString(", ") { it.second }}${if (reminders.isNotEmpty()) " + ${reminders.size} rappel(s)" else ""}"
            else -> kind.label
        }

    companion object {
        fun fromJson(o: JSONObject?): Task {
            if (o == null) return Task()
            val ra = o.optJSONArray("reminders") ?: JSONArray()
            return Task(
                kind = enumOr(o.optString("kind"), TaskKind.NONE),
                postX = o.optDouble("postX", -1.0).toFloat(), postY = o.optDouble("postY", -1.0).toFloat(),
                pkg = o.optString("pkg"), appLabel = o.optString("appLabel"),
                tapX = o.optDouble("tapX", -1.0).toFloat(), tapY = o.optDouble("tapY", -1.0).toFloat(),
                everyMin = o.optInt("everyMin"), atMin = o.optInt("atMin", -1), note = o.optString("note"),
                water = o.optBoolean("water", true), move = o.optBoolean("move", true),
                meals = o.optBoolean("meals", true), bedtime = o.optBoolean("bedtime", true),
                reminders = (0 until ra.length()).map { ra.getJSONObject(it).let { r -> Reminder(r.getInt("at"), r.getString("text")) } },
            )
        }
    }
}

/** Répliques liées aux métiers. */
object Work {
    val lines = l(
        Say.TASK_START to "C'est noté, je prends le poste.|Au boulot !|Mission acceptée.|Je m'en occupe, chef.",
        Say.OPEN_APP to "J'ouvre ça tout de suite !|Et hop, c'est ouvert !|Livraison d'appli !|Clic ! C'est parti.",
        Say.TAP_GO to "Je vais cliquer !|J'y vais, bouge pas.|Clic en approche !|Mission clic lancée.",
        Say.GUARD_OUT to "CIRCULEZ !|Zone interdite, dégage !|On passe pas !|Pas de badge, pas d'entrée !",
        Say.WATER to "Bois un verre d'eau, {nick} !|Hydratation ! Un grand verre, go.|T'as bu de l'eau ? Non ? Allez.",
        Say.MOVE to "Lève-toi, bouge un peu !|Ça fait longtemps que t'es assis. Debout !|Étire-toi deux minutes, {nick}.",
        Say.BED to "C'est bientôt l'heure de pioncer, {nick}.|Pose ce téléphone, au lit !|Dodo dans 15 minutes. Prépare-toi.",
        Say.MEAL_SOON to "On mange dans 10 minutes !|Bientôt la bouffe, prépare-toi.|À table dans 10 min, chef !",
        Say.BATT_LOW to "Batterie faible ! Branche-moi ce téléphone !|On va s'éteindre, vite un chargeur !|Plus de jus, chef !",
        Say.BATT_CHARGE to "Ça charge, parfait.|Le plein d'énergie, comme moi.|Branché. Nickel.",
        Say.NEED_ACCESS to "Il me faut le service clic activé dans l'appli.|Je peux pas cliquer sans l'autorisation d'accessibilité.",
    )
}
