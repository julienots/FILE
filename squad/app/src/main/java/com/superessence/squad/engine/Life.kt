package com.superessence.squad.engine

import com.superessence.squad.model.Mascot
import com.superessence.squad.model.Schedule
import com.superessence.squad.model.fmtMin
import java.util.Calendar

enum class Phase(val label: String) {
    SLEEP("Dort"), WAKING("Se réveille"), BREAKFAST("Petit-déj"), LUNCH("Déjeuner"), DINNER("Dîner"),
    WORKOUT("Entraînement"), WIND_DOWN("Se prépare à dormir"), FREE("Libre");

    val isMeal get() = this == BREAKFAST || this == LUNCH || this == DINNER
}

/**
 * La vie des mascottes : elles suivent les horaires de l'utilisateur
 * (sommeil, repas, sport) et leurs besoins évoluent en continu.
 */
object Life {
    const val MEAL_MIN = 30
    const val WORKOUT_MIN = 45
    const val WAKING_MIN = 20
    const val WIND_DOWN_MIN = 30

    private fun inWindow(m: Int, start: Int, len: Int): Boolean {
        val d = ((m - start) % 1440 + 1440) % 1440
        return d < len
    }

    fun phaseAt(s: Schedule, cal: Calendar): Phase {
        val m = cal.get(Calendar.HOUR_OF_DAY) * 60 + cal.get(Calendar.MINUTE)
        val dow = cal.get(Calendar.DAY_OF_WEEK)
        val weekend = dow == Calendar.SATURDAY || dow == Calendar.SUNDAY
        val wake = if (weekend) s.wakeWeekend else s.wake
        val sleepLen = ((wake - s.bed) % 1440 + 1440) % 1440
        if (sleepLen > 0 && inWindow(m, s.bed, sleepLen)) return Phase.SLEEP
        if (inWindow(m, s.breakfast, MEAL_MIN)) return Phase.BREAKFAST
        if (inWindow(m, s.lunch, MEAL_MIN)) return Phase.LUNCH
        if (inWindow(m, s.dinner, MEAL_MIN)) return Phase.DINNER
        if (inWindow(m, wake, WAKING_MIN)) return Phase.WAKING
        if (s.workoutOn && inWindow(m, s.workout, WORKOUT_MIN)) return Phase.WORKOUT
        if (inWindow(m, s.bed - WIND_DOWN_MIN, WIND_DOWN_MIN)) return Phase.WIND_DOWN
        return Phase.FREE
    }

    fun phaseNow(s: Schedule) = phaseAt(s, Calendar.getInstance())

    /** Prochain événement de la journée, pour l'affichage. */
    fun nextUp(s: Schedule): String {
        val c = Calendar.getInstance()
        val m = c.get(Calendar.HOUR_OF_DAY) * 60 + c.get(Calendar.MINUTE)
        val items = listOfNotNull(
            s.breakfast to "petit-déj", s.lunch to "déjeuner", s.dinner to "dîner",
            if (s.workoutOn) s.workout to "entraînement" else null, s.bed to "dodo",
        )
        val next = items.minByOrNull { ((it.first - m) % 1440 + 1440) % 1440 } ?: return ""
        return "Prochain : ${next.second} à ${fmtMin(next.first)}"
    }

    private fun clamp(v: Float) = v.coerceIn(0f, 100f)

    /** Fait vivre la mascotte du dernier instant connu jusqu'à maintenant, par pas de 5 minutes. */
    fun advance(m0: Mascot, s: Schedule, now: Long = System.currentTimeMillis()): Mascot {
        var m = m0
        var t = maxOf(m.updated, now - 48 * 3_600_000L)
        if (now - t < 60_000L) return m
        val cal = Calendar.getInstance()
        val a = m.archetype
        var last = m.lastPhase
        while (t < now) {
            val step = minOf(5 * 60_000L, now - t)
            val h = step / 3_600_000f
            cal.timeInMillis = t
            val p = phaseAt(s, cal)
            var sat = m.satiety; var en = m.energy; var mood = m.mood; var fit = m.fitness; var xp = m.xp
            when (p) {
                Phase.SLEEP -> { en += 13f * h; sat -= 1.5f * h; mood += 1f * h }
                Phase.BREAKFAST, Phase.LUNCH, Phase.DINNER -> { sat += 90f * h; mood += 10f * h; en -= 1f * h }
                Phase.WORKOUT -> { fit += 4f * h; en -= 10f * h * a.energy; sat -= 8f * h; mood += 6f * h; xp += (40 * h).toInt().coerceAtLeast(1) }
                else -> { en -= 3.5f * h * a.energy; sat -= 5f * h }
            }
            // Le moral suit les besoins
            if (sat < 25f) mood -= 5f * h
            if (en < 20f) mood -= 4f * h
            if (sat > 50f && en > 40f) mood += 1.5f * h
            fit -= 0.25f * h
            m = m.copy(satiety = clamp(sat), energy = clamp(en), mood = clamp(mood), fitness = clamp(fit), xp = xp)
            if (p.name != last) {
                val txt = when (p) {
                    Phase.SLEEP -> "S'est couché"
                    Phase.WAKING -> "Réveillé, café en main"
                    Phase.BREAKFAST, Phase.LUNCH, Phase.DINNER -> "${p.label} : ${a.food.random()}"
                    Phase.WORKOUT -> "Séance d'entraînement"
                    Phase.WIND_DOWN -> "Commence à bâiller"
                    Phase.FREE -> null
                }
                if (txt != null && last.isNotEmpty()) m = m.withEvent(txt, t)
                last = p.name
            }
            t += step
        }
        return m.copy(updated = now, lastPhase = last)
    }

    fun mood(m: Mascot): String = when {
        m.energy < 15 -> "Épuisé"
        m.satiety < 20 -> "Affamé"
        m.mood > 85 -> "Au top"
        m.mood > 65 -> "En forme"
        m.mood > 40 -> "Correct"
        m.mood > 20 -> "Grognon"
        else -> "Déprimé"
    }
}
