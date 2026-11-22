package com.superessence.pulsar.engine

enum class TrackId(val label: String, val short: String) {
    KICK("Kick", "KCK"),
    SNARE("Caisse claire", "SNR"),
    HATS("Charleston", "HAT"),
    PERC("Percus", "PRC"),
    BASS("Basse", "BAS"),
    CHORDS("Accords", "ACC"),
    LEAD("Mélodie", "MEL"),
    ARP("Arpège", "ARP"),
    FX("Effets", "FX"),
    TEXTURE("Ambiance", "AMB");

    companion object { val COUNT = entries.size }
}

enum class Scale(val label: String, val steps: IntArray, val minor: Boolean) {
    MAJOR("Majeur", intArrayOf(0, 2, 4, 5, 7, 9, 11), false),
    MINOR("Mineur", intArrayOf(0, 2, 3, 5, 7, 8, 10), true),
    DORIAN("Dorien", intArrayOf(0, 2, 3, 5, 7, 9, 10), true),
    PHRYGIAN("Phrygien", intArrayOf(0, 1, 3, 5, 7, 8, 10), true),
    HARMONIC("Mineur harmonique", intArrayOf(0, 2, 3, 5, 7, 8, 11), true),
    LYDIAN("Lydien", intArrayOf(0, 2, 4, 6, 7, 9, 11), false),
    MIXOLYDIAN("Mixolydien", intArrayOf(0, 2, 4, 5, 7, 9, 10), false);

    /** Note MIDI du degré [deg] (peut être négatif ou > 6) à partir de la fondamentale [root]. */
    fun note(root: Int, deg: Int): Int {
        val n = steps.size
        val oct = Math.floorDiv(deg, n)
        val idx = Math.floorMod(deg, n)
        return root + steps[idx] + 12 * oct
    }

    fun contains(root: Int, midi: Int): Boolean {
        val pc = Math.floorMod(midi - root, 12)
        return steps.any { it % 12 == pc }
    }
}

val NOTE_NAMES = arrayOf("Do", "Do#", "Ré", "Mi♭", "Mi", "Fa", "Fa#", "Sol", "La♭", "La", "Si♭", "Si")

enum class SectionType(val label: String, val energy: Int) {
    INTRO("Intro", 1), VERSE("Couplet", 2), CHORUS("Refrain", 3), BUILD("Montée", 3),
    DROP("DROP", 4), BREAK("Pause", 1), OUTRO("Outro", 1)
}

class NoteEv(
    val step: Float,
    val note: Int,
    val vel: Float,
    val len: Float,
    val slide: Boolean = false,
    val accent: Boolean = false,
    val mod: Float = 0f,
)

class Bar(
    val section: SectionType,
    val sectionIndex: Int,
    val barInSection: Int,
    val sectionBars: Int,
    val chord: IntArray,
    val chordName: String,
    val tracks: Array<List<NoteEv>>,
    val lpFrom: Float = 1f,
    val lpTo: Float = 1f,
    val hpFrom: Float = 0f,
    val hpTo: Float = 0f,
)

class SectionInfo(val type: SectionType, val startBar: Int, val bars: Int)

class Song(
    val genre: Genre,
    val seed: Long,
    val title: String,
    val bpm: Float,
    val swing: Float,
    val root: Int,
    val scale: Scale,
    val bars: List<Bar>,
    val sections: List<SectionInfo>,
    val params: GenParams,
) {
    val keyName get() = "${NOTE_NAMES[Math.floorMod(root, 12)]} ${scale.label.lowercase()}"
    fun durationSeconds(bpmOverride: Float = bpm): Float = bars.size * 4f * 60f / bpmOverride
    val code: String get() = "${genre.id.uppercase()}-${java.lang.Long.toHexString(seed).uppercase().takeLast(6)}"
}

/** Paramètres de génération choisis par l'utilisateur. */
data class GenParams(
    val genreId: String,
    val seed: Long,
    /** null = choisi par le générateur */
    val bpm: Float? = null,
    val root: Int? = null,
    val scale: Scale? = null,
    /** 0 = calme, 1 = explosif */
    val energy: Float = 0.6f,
    /** 0 = simple, 1 = chargé */
    val complexity: Float = 0.5f,
    val swing: Float? = null,
    /** Multiplicateur de longueur de structure (0.5 court … 2 long) */
    val length: Float = 1f,
    val withLead: Boolean = true,
)

object Chords {
    private val qualityNames = mapOf(
        listOf(4, 7) to "", listOf(3, 7) to "m", listOf(3, 6) to "dim", listOf(4, 8) to "aug",
        listOf(4, 7, 11) to "maj7", listOf(3, 7, 10) to "m7", listOf(4, 7, 10) to "7", listOf(3, 6, 10) to "m7♭5",
        listOf(3, 7, 11) to "mMaj7", listOf(2, 7) to "sus2", listOf(5, 7) to "sus4",
    )

    /** Construit un accord diatonique empilé en tierces. ext: 0 triade, 1 septième, 2 neuvième. */
    fun build(root: Int, scale: Scale, degree: Int, ext: Int, base: Int = 48): IntArray {
        val n = 3 + ext.coerceIn(0, 2)
        val notes = IntArray(n) { scale.note(root, degree + it * 2) }
        // place la fondamentale autour de « base »
        var shift = 0
        while (notes[0] + shift < base - 6) shift += 12
        while (notes[0] + shift > base + 6) shift -= 12
        return IntArray(n) { notes[it] + shift }
    }

    fun name(chord: IntArray): String {
        if (chord.isEmpty()) return ""
        val r = chord[0]
        val ints = chord.drop(1).take(3).map { Math.floorMod(it - r, 12) }.sorted()
        val q = qualityNames[ints] ?: qualityNames[ints.take(2)] ?: ""
        return NOTE_NAMES[Math.floorMod(r, 12)] + q
    }

    /** Renverse l'accord pour minimiser le mouvement avec [prev] (conduite des voix). */
    fun voiceLead(chord: IntArray, prev: IntArray?, low: Int = 50, high: Int = 74): IntArray {
        if (prev == null || prev.isEmpty()) return chord
        var best = chord; var bestCost = Int.MAX_VALUE
        val pcs = chord.map { Math.floorMod(it, 12) }
        for (inv in pcs.indices) {
            for (octBase in listOf(low - 12, low, low + 12)) {
                val v = IntArray(pcs.size)
                var cur = octBase - 1
                for (k in pcs.indices) {
                    val pc = pcs[(inv + k) % pcs.size]
                    var nn = cur + 1 + Math.floorMod(pc - (cur + 1), 12)
                    v[k] = nn; cur = nn
                }
                if (v.first() < low - 4 || v.last() > high + 4) continue
                val cost = v.indices.sumOf { k -> kotlin.math.abs(v[k] - prev[k.coerceAtMost(prev.size - 1)]) }
                if (cost < bestCost) { bestCost = cost; best = v }
            }
        }
        return best
    }
}
