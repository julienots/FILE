package com.superessence.noctea.generator

import com.superessence.noctea.data.AmbienceLayer
import com.superessence.noctea.data.BankPhrase
import com.superessence.noctea.data.Seg
import kotlinx.serialization.Serializable
import kotlin.math.max
import kotlin.math.roundToInt
import kotlin.math.roundToLong
import kotlin.random.Random

enum class Goal(val label: String, val key: String, val title: String) {
    SLEEP("M'endormir", "sleep", "Vers le sommeil"),
    RELAX("Me détendre", "relax", "Douce détente"),
    CLEAR("Vider mon esprit", "clear", "Esprit clair"),
    ANXIETY("Calmer mon anxiété", "anxiety", "Apaisement"),
    TENSION("Relâcher les tensions", "tension", "Corps libéré"),
    CONFIDENCE("Retrouver confiance", "confidence", "Force tranquille"),
    WAKE("Me réveiller sereinement", "wake", "Éveil serein"),
    DEEP("Méditation profonde", "deep", "Profondeur"),
    LETGO("Lâcher prise", "letgo", "Lâcher-prise"),
    EMOTIONS("Gérer mes émotions", "emotions", "Cœur apaisé"),
}

enum class Ambience(val label: String, val key: String, val suffix: String, val art: String) {
    RAIN("Pluie nocturne", "rain", "sous la pluie", "rain"),
    OCEAN("Océan", "ocean", "au bord de l'océan", "ocean"),
    FOREST("Forêt", "forest", "en forêt", "forest"),
    STARS("Nuit étoilée", "stars", "sous les étoiles", "stars"),
    MOUNTAIN("Chemin de montagne", "mountain", "en montagne", "mountain"),
    FIRE("Cheminée", "fire", "au coin du feu", "fire"),
    WIND("Vent doux", "wind", "dans le vent doux", "clouds"),
    SILENCE("Silence profond", "silence", "dans le silence", "moon"),
    SPACE("Univers spatial", "space", "dans le cosmos", "space"),
    NATURE("Nature", "nature", "en pleine nature", "garden"),
}

enum class NarrationStyle(val label: String, val key: String, val speechRatio: Double, val description: String) {
    SOFT("Très douce", "soft", 0.42, "Voix murmurée, rythme lent"),
    ENVELOPING("Enveloppante", "enveloping", 0.45, "Présence chaleureuse et proche"),
    POETIC("Poétique", "poetic", 0.47, "Images et métaphores nocturnes"),
    COMFORTING("Réconfortante", "comforting", 0.44, "Bienveillance et douceur"),
    DEEP("Profonde", "deep", 0.36, "Plus d'espace, plus de profondeur"),
    MINIMAL("Guidage minimal", "minimal", 0.2, "Peu de mots, de longues pauses"),
}

enum class MusicIntensity(val label: String, val volume: Float) {
    NONE("Aucune musique", 0f),
    LIGHT("Très légère", 0.2f),
    MODERATE("Modérée", 0.38f),
}

val DURATIONS = listOf(3, 5, 10, 15, 20, 30, 45, 60)

@Serializable
data class GeneratorParams(
    val goal: Goal = Goal.SLEEP,
    val minutes: Int = 10,
    val ambience: Ambience = Ambience.RAIN,
    val style: NarrationStyle = NarrationStyle.SOFT,
    val music: MusicIntensity = MusicIntensity.LIGHT,
    val seed: Long = 1L,
)

data class GeneratedMeditation(
    val title: String,
    val params: GeneratorParams,
    val segments: List<Seg>,
    val paragraphs: List<String>,
    val durationMs: Long,
    val speechMs: Long,
    val ambience: List<AmbienceLayer>,
    val art: String,
)

/** Accès à la banque de phrases préenregistrées. */
fun interface PhraseBank {
    fun pool(name: String): List<BankPhrase>
}

/**
 * Générateur de méditations personnalisées.
 *
 * Assemble un script cohérent (intention → installation → respiration → corps → visualisation
 * → cœur de l'objectif → approfondissement → conclusion) à partir de phrases préenregistrées,
 * puis répartit les silences pour atteindre EXACTEMENT la durée demandée.
 * Déterministe pour des paramètres + graine identiques.
 */
class MeditationGenerator(private val bank: PhraseBank, private val fx: Map<String, Long>) {

    private data class Piece(val phrase: BankPhrase, val weight: Double, val section: String)

    fun generate(p: GeneratorParams): GeneratedMeditation {
        val rnd = Random(p.seed * 31 + p.minutes * 7 + p.goal.ordinal * 101 + p.ambience.ordinal * 1009)
        val total = p.minutes * 60_000L
        val bellMs = fx["bell"] ?: 0L
        val breathMs = fx["breath"] ?: 0L
        val isSleep = p.goal == Goal.SLEEP || p.goal == Goal.DEEP && p.minutes >= 20
        val endBell = !isSleep

        // Proportion de parole : plus la séance est longue, plus elle laisse de place au silence.
        val lengthFactor = when {
            p.minutes <= 5 -> 1.0
            p.minutes <= 10 -> 0.95
            p.minutes <= 15 -> 0.85
            p.minutes <= 20 -> 0.75
            p.minutes <= 30 -> 0.6
            p.minutes <= 45 -> 0.48
            else -> 0.4
        }
        val fixedMs = bellMs + 2500 + (if (endBell) bellMs else 0L)
        val budget = ((total - fixedMs) * p.style.speechRatio * lengthFactor).roundToLong()

        val pieces = mutableListOf<Piece>()
        var used = 0L
        fun addAll(list: List<BankPhrase>, weight: Double, section: String) {
            list.forEach { pieces += Piece(it, weight, section); used += it.durationMs }
        }

        // 1. Intention et tonalité
        val intro = bank.pool("intro.${p.goal.key}")
        val style = bank.pool("style.${p.style.key}")
        addAll(if (p.minutes <= 3) intro.take(1) else intro, 1.0, "intro")
        addAll(if (p.minutes <= 5) style.take(1) else style, 1.0, "intro")

        // 2. Sections, avec un budget proportionnel
        val closeKey = when (p.goal) {
            Goal.SLEEP -> "close.sleep"
            Goal.WAKE, Goal.CONFIDENCE -> "close.wake"
            Goal.DEEP -> if (p.minutes >= 20) "close.sleep" else "close.soft"
            else -> "close.soft"
        }
        val close = bank.pool(closeKey)
        val closeCount = when {
            p.minutes <= 3 -> 2
            p.minutes <= 10 -> 4
            else -> close.size
        }
        val closePicked = spread(close, closeCount, rnd, jitter = false)
        val closeMs = closePicked.sumOf { it.durationMs }
        val remaining = max(0L, budget - used - closeMs)

        val ambPool = bank.pool("amb.${p.ambience.key}")
        val bodyShare = when (p.goal) {
            Goal.TENSION, Goal.SLEEP -> 0.22
            Goal.WAKE, Goal.CONFIDENCE -> 0.08
            else -> 0.16
        }
        val plan = listOf(
            Triple("arrive", 0.12, 1.1),
            Triple("breath", 0.13, 1.5),
            Triple("body", bodyShare, 1.3),
            Triple("amb", 0.22, 1.25),
            Triple("goal", 0.28, 1.4),
        )
        val shareSum = plan.sumOf { it.second }
        for ((section, share, weight) in plan) {
            val sectionBudget = (remaining * share / shareSum).roundToLong()
            val pool = when (section) {
                "amb" -> ambPool
                "goal" -> bank.pool("goal.${p.goal.key}")
                else -> bank.pool(section)
            }
            val picked = pickWithin(pool, sectionBudget, rnd)
            if (section == "amb" && p.style == NarrationStyle.POETIC) {
                // Images poétiques intercalées dans la visualisation
                val poetic = pickWithin(bank.pool("poetic"), (sectionBudget * 0.45).roundToLong(), rnd)
                val merged = interleave(picked, poetic)
                merged.forEach { (ph, isPoetic) ->
                    pieces += Piece(ph, if (isPoetic) 1.5 else weight, if (isPoetic) "poetic" else section)
                    used += ph.durationMs
                }
            } else {
                addAll(picked, weight, section)
            }
        }

        // 3. Approfondissement : ancres et silences guidés pour les séances longues
        val deep = bank.pool("deep")
        val anchor = bank.pool("anchor")
        val deepWeight = if (p.style == NarrationStyle.MINIMAL) 4.5 else 2.6
        var guard = 0
        var deepIndex = 0
        while (used + closeMs < budget && guard++ < 400) {
            val src = if (guard % 2 == 1) deep else anchor
            if (src.isEmpty()) break
            val ph = if (src === deep) deep[(deepIndex++) % deep.size] else anchor[rnd.nextInt(anchor.size)]
            if (used + closeMs + ph.durationMs > budget + 2_000) break
            pieces += Piece(ph, if (src === deep) deepWeight else 2.0, "deep")
            used += ph.durationMs
        }
        if (pieces.none { it.section == "deep" } && p.minutes >= 5 && deep.isNotEmpty()) {
            pieces += Piece(deep[rnd.nextInt(deep.size)], deepWeight, "deep")
            used += pieces.last().phrase.durationMs
        }

        // 4. Conclusion
        closePicked.forEach { pieces += Piece(it, 1.0, "close"); used += it.durationMs }

        // Si la parole dépasse la durée (styles très bavards sur 3 min), on retire des phrases du milieu.
        val speechCap = total - fixedMs - pieces.size * MIN_GAP
        while (pieces.sumOf { it.phrase.durationMs } > speechCap && pieces.size > 4) {
            val idx = pieces.indexOfLast { it.section == "deep" }.takeIf { it >= 0 }
                ?: pieces.indexOfFirst { it.section == "body" }.takeIf { it >= 0 }
                ?: (pieces.size / 2)
            pieces.removeAt(idx)
        }
        val speech = pieces.sumOf { it.phrase.durationMs }

        // 5. Répartition des silences
        var silence = total - fixedMs - speech
        val tailShare = when {
            isSleep -> 0.18
            p.goal == Goal.WAKE -> 0.01
            else -> 0.04
        }
        val tail = max(3_000L, (silence * tailShare).roundToLong()).coerceAtMost(max(0L, silence))
        silence -= tail
        val weights = pieces.map { it.weight }
        val wSum = weights.sum().takeIf { it > 0 } ?: 1.0
        val gaps = weights.map { w -> max(MIN_GAP, (silence * w / wSum).roundToLong()) }.toMutableList()
        // Ajustement exact (les minimums peuvent dépasser)
        var diff = silence - gaps.sum()
        var i = gaps.size - 1
        while (diff != 0L && gaps.isNotEmpty()) {
            val adj = if (diff > 0) diff else max(diff, -(gaps[i] - 600))
            gaps[i] += adj
            diff -= adj
            i = if (i == 0) gaps.size - 1 else i - 1
            if (diff < 0 && gaps.all { it <= 600 }) break
        }

        val segs = mutableListOf<Seg>()
        if (bellMs > 0) segs += Seg.Fx("bell", bellMs)
        segs += Seg.Silence(2500)
        pieces.forEachIndexed { idx, piece ->
            segs += piece.phrase.segments
            val gap = gaps.getOrElse(idx) { MIN_GAP }
            if (gap >= 6_500 && breathMs > 0 && rnd.nextDouble() < 0.15 && idx < pieces.size - 1) {
                segs += Seg.Silence(gap - breathMs - 300)
                segs += Seg.Fx("breath", breathMs)
                segs += Seg.Silence(300)
            } else {
                segs += Seg.Silence(gap)
            }
        }
        if (endBell && bellMs > 0) segs += Seg.Fx("bell", bellMs)
        segs += Seg.Silence(tail)
        val merged = mergeSilences(segs)
        val duration = merged.sumOf { it.ms }
        // correction finale d'arrondi
        val fixed = if (duration != total && merged.last() is Seg.Silence) {
            val last = merged.last() as Seg.Silence
            merged.dropLast(1) + Seg.Silence(max(0L, last.ms + (total - duration)))
        } else merged

        return GeneratedMeditation(
            title = "${p.goal.title} ${p.ambience.suffix}",
            params = p,
            segments = fixed,
            paragraphs = pieces.map { it.phrase.text },
            durationMs = fixed.sumOf { it.ms },
            speechMs = speech,
            ambience = ambienceFor(p),
            art = p.ambience.art,
        )
    }

    /** Choisit un sous-ensemble ordonné et réparti du pool, dont la durée tient dans le budget. */
    private fun pickWithin(pool: List<BankPhrase>, budget: Long, rnd: Random): List<BankPhrase> {
        if (pool.isEmpty() || budget <= 0) return if (pool.isNotEmpty()) listOf(pool.first()) else emptyList()
        var best = spread(pool, 1, rnd, jitter = false)
        for (k in 1..pool.size) {
            val candidate = spread(pool, k, rnd, jitter = true)
            if (candidate.sumOf { it.durationMs } <= budget) best = candidate else break
        }
        return best
    }

    private fun spread(pool: List<BankPhrase>, k: Int, rnd: Random, jitter: Boolean): List<BankPhrase> {
        if (k >= pool.size) return pool
        if (k <= 1) return listOf(pool.first())
        val idx = (0 until k).map { i ->
            val base = i * (pool.size - 1) / (k - 1).toDouble()
            val j = if (jitter && i in 1 until k - 1) base + rnd.nextDouble(-0.45, 0.45) else base
            j.roundToInt().coerceIn(0, pool.size - 1)
        }.distinct().sorted()
        return idx.map { pool[it] }
    }

    private fun interleave(a: List<BankPhrase>, b: List<BankPhrase>): List<Pair<BankPhrase, Boolean>> {
        val out = mutableListOf<Pair<BankPhrase, Boolean>>()
        val step = if (b.isEmpty()) Int.MAX_VALUE else max(1, a.size / (b.size + 1))
        var bi = 0
        a.forEachIndexed { i, ph ->
            out += ph to false
            if ((i + 1) % step == 0 && bi < b.size) out += b[bi++] to true
        }
        while (bi < b.size) out += b[bi++] to true
        return out
    }

    companion object {
        const val MIN_GAP = 1_500L

        fun mergeSilences(segs: List<Seg>): List<Seg> {
            val out = mutableListOf<Seg>()
            for (s in segs) {
                if (s is Seg.Silence && s.ms <= 0) continue
                val last = out.lastOrNull()
                if (s is Seg.Silence && last is Seg.Silence) out[out.size - 1] = Seg.Silence(last.ms + s.ms)
                else out += s
            }
            return out
        }

        fun ambienceFor(p: GeneratorParams): List<AmbienceLayer> {
            val nature = when (p.ambience) {
                Ambience.RAIN -> listOf(AmbienceLayer("rain_soft", 0.5f))
                Ambience.OCEAN -> listOf(AmbienceLayer("waves", 0.5f))
                Ambience.FOREST -> listOf(AmbienceLayer("wind_trees", 0.35f), AmbienceLayer("night", 0.2f))
                Ambience.STARS -> listOf(AmbienceLayer("night", 0.35f))
                Ambience.MOUNTAIN -> listOf(AmbienceLayer("wind_soft", 0.45f))
                Ambience.FIRE -> listOf(AmbienceLayer("fire", 0.5f))
                Ambience.WIND -> listOf(AmbienceLayer("wind_soft", 0.5f))
                Ambience.SILENCE -> emptyList()
                Ambience.SPACE -> listOf(AmbienceLayer("space", 0.4f))
                Ambience.NATURE -> listOf(AmbienceLayer("stream", 0.4f), AmbienceLayer("night", 0.15f))
            }
            if (p.music == MusicIntensity.NONE) return nature
            val musicSound = when {
                p.ambience == Ambience.SPACE || p.ambience == Ambience.STARS -> "pads"
                p.style == NarrationStyle.POETIC || p.style == NarrationStyle.COMFORTING -> "piano"
                else -> "ambient"
            }
            return nature.filter { it.sound != musicSound } + AmbienceLayer(musicSound, p.music.volume)
        }
    }
}
