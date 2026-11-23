package com.superessence.squad.overlay

import com.superessence.squad.engine.Phase
import com.superessence.squad.model.Act
import com.superessence.squad.model.Mascot
import com.superessence.squad.model.Say
import kotlin.random.Random

/**
 * Le « cerveau » d'une mascotte à l'écran : choisit quoi faire selon l'heure
 * (horaires de l'utilisateur), ses besoins et sa personnalité.
 */
class Brain(private val rnd: Random = Random.Default) {
    var act: Act = Act.IDLE
        private set
    /** Temps restant dans l'action courante (s). */
    private var left = 0f
    /** Action imposée (clic, nourriture…) jusqu'à expiration. */
    private var forced: Act? = null
    private var forcedLeft = 0f
    var wantsTarget = false
    private var chatIn = 8f + rnd.nextFloat() * 20f

    fun force(a: Act, seconds: Float) { forced = a; forcedLeft = seconds; act = a }
    fun clearForce() { forced = null; forcedLeft = 0f }
    val isForced get() = forced != null

    /** Avance d'un pas. Retourne une réplique à dire, ou null. */
    fun step(dt: Float, m: Mascot, phase: Phase, chatter: Float, arrived: Boolean): Say? {
        forced?.let {
            forcedLeft -= dt
            if (forcedLeft <= 0f) { forced = null; left = 0f } else { act = it; return null }
        }
        val a = m.archetype
        left -= dt
        val scheduled: Act? = when {
            phase == Phase.SLEEP -> Act.SLEEP
            phase.isMeal -> Act.EAT
            m.energy < 8f -> Act.SLEEP // sieste de survie
            else -> null
        }
        if (scheduled != null) {
            if (act != scheduled) { act = scheduled; left = 30f }
            return null
        }
        if (act == Act.WALK && arrived) left = 0f
        if (left <= 0f) {
            act = when (phase) {
                Phase.WORKOUT -> pick(mapOf(Act.TRAIN to 4, Act.PUSHUP to 3, Act.FLEX to 2, Act.HANDSTAND to 2, Act.DRINK to 1, Act.WALK to 1))
                Phase.WAKING -> pick(mapOf(Act.COFFEE to 5, Act.YAWN to 2, Act.STRETCH to 3, Act.WALK to 1))
                Phase.WIND_DOWN -> pick(mapOf(Act.YAWN to 3, Act.IDLE to 3, Act.WALK to 2, Act.PHONE to 1))
                else -> {
                    val w = a.hobbies.toMutableMap()
                    w[Act.WALK] = (w[Act.WALK] ?: 1) + 3
                    w[Act.IDLE] = (w[Act.IDLE] ?: 0) + 2
                    if (a.hobbies.containsKey(Act.TRAIN) || a.hobbies.containsKey(Act.PUSHUP)) {
                        w[Act.HANDSTAND] = 1; w[Act.STRETCH] = 1; w[Act.DRINK] = 1
                        if (m.fitness > 55f) w[Act.ONE_ARM_HS] = 1
                    } else w[Act.STRETCH] = 1
                    if (m.energy < 30f) { w[Act.YAWN] = 3; w[Act.IDLE] = (w[Act.IDLE] ?: 0) + 4; w.remove(Act.HANDSTAND); w.remove(Act.ONE_ARM_HS) }
                    pick(w)
                }
            }
            left = when (act) {
                Act.WALK -> 12f
                Act.YAWN -> 2.5f
                Act.WAVE -> 3f
                Act.FLEX -> 4f
                Act.TALK -> 4f
                Act.HANDSTAND, Act.ONE_ARM_HS -> 5f
                Act.DRINK, Act.STRETCH -> 3.5f
                else -> 6f + rnd.nextFloat() * 10f
            }
            wantsTarget = act == Act.WALK
        }
        // Parler spontanément
        chatIn -= dt * chatter * a.chatty
        if (chatIn <= 0f) {
            chatIn = 35f + rnd.nextFloat() * 70f
            return when {
                m.satiety < 25f -> Say.HUNGRY
                m.energy < 20f || phase == Phase.WIND_DOWN -> Say.SLEEPY
                m.mood < 25f -> Say.LOW
                phase == Phase.WAKING -> Say.WAKE
                phase == Phase.WORKOUT -> Say.TRAIN
                else -> Say.RANDOM
            }
        }
        return null
    }

    private fun pick(w: Map<Act, Int>): Act {
        val total = w.values.sum().coerceAtLeast(1)
        var r = rnd.nextInt(total)
        for ((k, v) in w) { r -= v; if (r < 0) return k }
        return Act.IDLE
    }
}
