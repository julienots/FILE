package com.superessence.squad.engine

import android.content.Context
import android.content.SharedPreferences
import com.superessence.squad.model.Archetype
import com.superessence.squad.model.Archetypes
import com.superessence.squad.model.Mascot
import com.superessence.squad.model.Schedule
import com.superessence.squad.model.Settings
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update
import org.json.JSONArray
import org.json.JSONObject

data class SquadState(
    val mascots: List<Mascot> = emptyList(),
    val schedule: Schedule = Schedule(),
    val settings: Settings = Settings(),
    val onboarded: Boolean = false,
)

/** Source de vérité partagée entre l'appli et le service d'affichage. */
object Squad {
    private lateinit var prefs: SharedPreferences
    private val _state = MutableStateFlow(SquadState())
    val state: StateFlow<SquadState> get() = _state

    const val MAX = 8

    /** Petits signaux de l'appli vers l'écran (« eat », « train »). */
    val pings = kotlinx.coroutines.flow.MutableSharedFlow<Pair<String, String>>(extraBufferCapacity = 16)

    fun init(ctx: Context) {
        if (::prefs.isInitialized) return
        prefs = ctx.getSharedPreferences("squad", Context.MODE_PRIVATE)
        val raw = prefs.getString("state", null)
        _state.value = if (raw == null) SquadState() else runCatching { decode(JSONObject(raw)) }.getOrElse { SquadState() }
        tick()
    }

    private fun decode(o: JSONObject): SquadState {
        val arr = o.optJSONArray("mascots") ?: JSONArray()
        return SquadState(
            mascots = (0 until arr.length()).mapNotNull { runCatching { Mascot.fromJson(arr.getJSONObject(it)) }.getOrNull() },
            schedule = Schedule.fromJson(o.optJSONObject("schedule")),
            settings = Settings.fromJson(o.optJSONObject("settings")),
            onboarded = o.optBoolean("onboarded"),
        )
    }

    private fun save(s: SquadState) {
        if (!::prefs.isInitialized) return
        val o = JSONObject()
            .put("mascots", JSONArray().apply { s.mascots.forEach { put(it.toJson()) } })
            .put("schedule", s.schedule.toJson()).put("settings", s.settings.toJson()).put("onboarded", s.onboarded)
        prefs.edit().putString("state", o.toString()).apply()
    }

    fun edit(f: (SquadState) -> SquadState) {
        _state.update(f)
        save(_state.value)
    }

    fun editMascot(id: String, f: (Mascot) -> Mascot) = edit { s -> s.copy(mascots = s.mascots.map { if (it.id == id) f(it) else it }) }

    fun get(id: String): Mascot? = _state.value.mascots.firstOrNull { it.id == id }

    /** Fait avancer la vie de tout le monde jusqu'à maintenant. */
    fun tick() = edit { s -> s.copy(mascots = s.mascots.map { Life.advance(it, s.schedule) }) }

    fun add(a: Archetype, name: String = a.name): Mascot? {
        if (_state.value.mascots.size >= MAX) return null
        val m = Mascot.create(a, name)
        edit { it.copy(mascots = it.mascots + m) }
        return m
    }

    fun remove(id: String) = edit { s -> s.copy(mascots = s.mascots.filterNot { it.id == id }.map { it.copy(rel = it.rel - id) }) }

    fun feed(id: String): String? {
        val m = get(id) ?: return null
        val food = m.archetype.food.random()
        if (m.satiety > 92) {
            editMascot(id) { it.copy(mood = (it.mood - 3).coerceAtLeast(0f)) }
            return "Je suis plein à craquer…"
        }
        editMascot(id) {
            it.copy(satiety = (it.satiety + 35).coerceAtMost(100f), mood = (it.mood + 8).coerceAtMost(100f))
                .withEvent("Tu lui as servi : $food")
        }
        pings.tryEmit("eat" to id)
        return food
    }

    fun train(id: String): Boolean {
        val m = get(id) ?: return false
        if (m.energy < 15) return false
        editMascot(id) {
            it.copy(
                fitness = (it.fitness + 3).coerceAtMost(100f), xp = it.xp + 15, energy = it.energy - 12,
                satiety = (it.satiety - 6).coerceAtLeast(0f), mood = (it.mood + 4).coerceAtMost(100f),
            ).withEvent("Séance avec toi : +15 XP")
        }
        pings.tryEmit("train" to id)
        return true
    }

    /** Résultat d'une baston à l'écran. */
    fun recordFight(winner: String, loser: String) {
        val wn = get(winner)?.name ?: return
        val ln = get(loser)?.name ?: return
        edit { s ->
            s.copy(mascots = s.mascots.map {
                when (it.id) {
                    winner -> it.copy(
                        wins = it.wins + 1, xp = it.xp + 12, mood = (it.mood + 8).coerceAtMost(100f), energy = (it.energy - 6).coerceAtLeast(0f),
                    ).bond(loser, -6).withEvent("A mis une raclée à $ln")
                    loser -> it.copy(
                        losses = it.losses + 1, xp = it.xp + 4, mood = (it.mood - 10).coerceAtLeast(0f), energy = (it.energy - 10).coerceAtLeast(0f),
                    ).bond(winner, -10).withEvent("S'est fait sécher par $wn")
                    else -> it
                }
            })
        }
    }

    /** Moment sympa entre deux mascottes. */
    fun bond(a: String, b: String, d: Int, eventA: String? = null, eventB: String? = null) = edit { s ->
        s.copy(mascots = s.mascots.map {
            when (it.id) {
                a -> it.bond(b, d).let { m -> if (eventA != null) m.withEvent(eventA) else m }
                b -> it.bond(a, d).let { m -> if (eventB != null) m.withEvent(eventB) else m }
                else -> it
            }
        })
    }

    /** Fin d'une séance collective. */
    fun recordGym(ids: List<String>, gear: List<String>) = edit { s ->
        val names = s.mascots.filter { it.id in ids }.associate { it.id to it.name }
        s.copy(mascots = s.mascots.map { m ->
            if (m.id !in ids) m else {
                var r = m.copy(
                    fitness = (m.fitness + 4f).coerceAtMost(100f), xp = m.xp + 20,
                    energy = (m.energy - 10f).coerceAtLeast(0f), mood = (m.mood + 6f).coerceAtMost(100f),
                ).withEvent("Séance collective (${gear.joinToString(", ")}) avec ${ids.filter { it != m.id }.mapNotNull { names[it] }.joinToString(", ").ifEmpty { "lui-même" }}")
                ids.filter { it != m.id }.forEach { r = r.bond(it, 5) }
                r
            }
        })
    }

    /** Charge à tenter pour un PR (kg). */
    fun prTarget(m: com.superessence.squad.model.Mascot, lift: String): Float {
        val base = when (lift) { "Squat" -> 90f; "Presse" -> 160f; else -> 70f } * m.archetype.skill
        val start = base + m.fitness * (if (lift == "Presse") 2f else 0.9f) + m.level * 2f
        val cur = m.prs[lift] ?: (start - 5f)
        return (((maxOf(cur, start - 10f) + 2.5f + (0..3).random() * 2.5f) / 2.5f).toInt() * 2.5f)
    }

    fun recordPr(id: String, lift: String, kg: Float, ok: Boolean) = editMascot(id) {
        if (ok) it.copy(prs = it.prs + (lift to kg), xp = it.xp + 25, mood = (it.mood + 15f).coerceAtMost(100f))
            .withEvent("NOUVEAU PR au $lift : ${fmtKg(kg)} !")
        else it.copy(mood = (it.mood - 5f).coerceAtLeast(0f)).withEvent("PR raté au $lift (${fmtKg(kg)})")
    }

    fun fmtKg(kg: Float) = if (kg % 1f == 0f) "${kg.toInt()} kg" else "%.1f kg".format(kg).replace('.', ',')

    /** Fin d'une fête. */
    fun recordParty(ids: List<String>) = edit { s ->
        s.copy(mascots = s.mascots.map { m ->
            if (m.id !in ids) m else {
                var r = m.copy(mood = (m.mood + 15f).coerceAtMost(100f), energy = (m.energy - 8f).coerceAtLeast(0f), xp = m.xp + 8)
                    .withEvent("Grosse fête avec la squad (${ids.size} gars)")
                ids.filter { it != m.id }.forEach { r = r.bond(it, 6) }
                r
            }
        })
    }

    fun pet(id: String) = editMascot(id) { it.copy(mood = (it.mood + 6).coerceAtMost(100f), xp = it.xp + 1) }

    fun starterArchetypes(): List<Archetype> = listOf(Archetypes.byId("tank"), Archetypes.byId("ragnar"))
}
