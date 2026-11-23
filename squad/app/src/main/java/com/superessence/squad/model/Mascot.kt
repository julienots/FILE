package com.superessence.squad.model

import org.json.JSONArray
import org.json.JSONObject
import kotlin.math.sqrt

data class LifeEvent(val t: Long, val text: String)

data class Mascot(
    val id: String,
    val arch: String,
    val name: String,
    val look: Look,
    val onScreen: Boolean = true,
    val satiety: Float = 80f,
    val energy: Float = 80f,
    val mood: Float = 75f,
    val fitness: Float = 30f,
    val xp: Int = 0,
    val born: Long = System.currentTimeMillis(),
    val updated: Long = System.currentTimeMillis(),
    val x: Float = -1f,
    val y: Float = -1f,
    val lastPhase: String = "",
    val log: List<LifeEvent> = emptyList(),
) {
    val archetype get() = Archetypes.byId(arch)
    val level get() = 1 + sqrt(xp / 40f).toInt()
    val days get() = ((System.currentTimeMillis() - born) / 86_400_000L).toInt() + 1

    fun withEvent(text: String, t: Long = System.currentTimeMillis()) =
        copy(log = (listOf(LifeEvent(t, text)) + log).take(40))

    fun toJson(): JSONObject = JSONObject()
        .put("id", id).put("arch", arch).put("name", name).put("look", look.toJson()).put("onScreen", onScreen)
        .put("satiety", satiety.toDouble()).put("energy", energy.toDouble()).put("mood", mood.toDouble())
        .put("fitness", fitness.toDouble()).put("xp", xp).put("born", born).put("updated", updated)
        .put("x", x.toDouble()).put("y", y.toDouble()).put("lastPhase", lastPhase)
        .put("log", JSONArray().apply { log.forEach { put(JSONObject().put("t", it.t).put("x", it.text)) } })

    companion object {
        fun fromJson(o: JSONObject): Mascot {
            val la = o.optJSONArray("log") ?: JSONArray()
            return Mascot(
                id = o.getString("id"), arch = o.getString("arch"), name = o.getString("name"),
                look = Look.fromJson(o.getJSONObject("look")), onScreen = o.optBoolean("onScreen", true),
                satiety = o.optDouble("satiety", 80.0).toFloat(), energy = o.optDouble("energy", 80.0).toFloat(),
                mood = o.optDouble("mood", 75.0).toFloat(), fitness = o.optDouble("fitness", 30.0).toFloat(),
                xp = o.optInt("xp"), born = o.optLong("born"), updated = o.optLong("updated"),
                x = o.optDouble("x", -1.0).toFloat(), y = o.optDouble("y", -1.0).toFloat(),
                lastPhase = o.optString("lastPhase"),
                log = (0 until la.length()).map { la.getJSONObject(it).let { e -> LifeEvent(e.getLong("t"), e.getString("x")) } },
            )
        }

        fun create(a: Archetype, name: String = a.name) = Mascot(
            id = java.util.UUID.randomUUID().toString().take(8), arch = a.id, name = name, look = a.look,
        ).withEvent("Arrivé dans ta squad. Salut !")
    }
}

/** Horaires de l'utilisateur, en minutes depuis minuit. */
data class Schedule(
    val wake: Int = 7 * 60,
    val wakeWeekend: Int = 9 * 60,
    val bed: Int = 23 * 60,
    val breakfast: Int = 7 * 60 + 30,
    val lunch: Int = 12 * 60 + 30,
    val dinner: Int = 20 * 60,
    val workout: Int = 18 * 60,
    val workoutOn: Boolean = true,
) {
    fun toJson(): JSONObject = JSONObject().put("wake", wake).put("wakeWeekend", wakeWeekend).put("bed", bed)
        .put("breakfast", breakfast).put("lunch", lunch).put("dinner", dinner).put("workout", workout).put("workoutOn", workoutOn)

    companion object {
        fun fromJson(o: JSONObject?): Schedule {
            if (o == null) return Schedule()
            val d = Schedule()
            return Schedule(
                o.optInt("wake", d.wake), o.optInt("wakeWeekend", d.wakeWeekend), o.optInt("bed", d.bed),
                o.optInt("breakfast", d.breakfast), o.optInt("lunch", d.lunch), o.optInt("dinner", d.dinner),
                o.optInt("workout", d.workout), o.optBoolean("workoutOn", d.workoutOn),
            )
        }
    }
}

data class Settings(
    val size: Float = 1f,
    val speed: Float = 1f,
    val chatter: Float = 1f,
    val showSleeping: Boolean = true,
    val vibrate: Boolean = true,
    val overlayOn: Boolean = false,
) {
    fun toJson(): JSONObject = JSONObject().put("size", size.toDouble()).put("speed", speed.toDouble())
        .put("chatter", chatter.toDouble()).put("showSleeping", showSleeping).put("vibrate", vibrate).put("overlayOn", overlayOn)

    companion object {
        fun fromJson(o: JSONObject?): Settings {
            if (o == null) return Settings()
            return Settings(
                o.optDouble("size", 1.0).toFloat(), o.optDouble("speed", 1.0).toFloat(), o.optDouble("chatter", 1.0).toFloat(),
                o.optBoolean("showSleeping", true), o.optBoolean("vibrate", true), o.optBoolean("overlayOn", false),
            )
        }
    }
}

fun fmtMin(m: Int): String = "%02d:%02d".format((m / 60) % 24, m % 60)
