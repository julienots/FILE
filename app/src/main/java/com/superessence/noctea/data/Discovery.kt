package com.superessence.noctea.data

import java.text.Normalizer

/** Recherche intelligente, recommandations et suggestions selon l'humeur. */
object Discovery {

    /** Humeurs proposées (journal et accueil). */
    val moods = listOf(
        Mood(1, "Épuisé·e", "😴"),
        Mood(2, "Anxieux·se", "😟"),
        Mood(3, "Agité·e", "🌪️"),
        Mood(4, "Triste", "🌧️"),
        Mood(5, "Neutre", "😐"),
        Mood(6, "Serein·e", "🙂"),
        Mood(7, "Heureux·se", "✨"),
    )

    data class Mood(val id: Int, val label: String, val emoji: String)

    private val synonyms = mapOf(
        "dormir" to listOf("sommeil", "nuit", "endormissement", "insomnie"),
        "insomnie" to listOf("sommeil", "nuits agitees", "endormissement"),
        "stress" to listOf("anxiete", "anti-stress", "tension", "journee stressante", "calme"),
        "angoisse" to listOf("anxiete", "calme", "respiration"),
        "anxiete" to listOf("stress", "calme", "respiration", "acceptation"),
        "respirer" to listOf("respiration", "souffle", "coherence"),
        "souffle" to listOf("respiration"),
        "coeur" to listOf("coherence cardiaque", "gratitude"),
        "histoire" to listOf("histoires", "conte", "recit"),
        "conte" to listOf("histoires"),
        "enfant" to listOf("histoires"),
        "mer" to listOf("ocean", "vagues", "plage"),
        "plage" to listOf("ocean"),
        "pluie" to listOf("pluie", "orage"),
        "feu" to listOf("cheminee", "feu"),
        "neige" to listOf("chalet", "hiver"),
        "matin" to listOf("reveil", "matin"),
        "confiance" to listOf("confiance en soi", "estime"),
        "pensees" to listOf("esprit", "mental", "vider"),
        "detente" to listOf("relaxation", "relachement"),
        "lacher" to listOf("lacher-prise", "acceptation"),
        "corps" to listOf("detente corporelle", "scan", "relachement"),
        "etoiles" to listOf("etoiles", "nuit"),
        "foret" to listOf("foret", "cabane", "arbres"),
    )

    fun normalize(s: String): String =
        Normalizer.normalize(s.lowercase(), Normalizer.Form.NFD).replace(Regex("\\p{Mn}+"), "").replace("œ", "oe")

    data class Hit(val track: Track, val score: Int)

    fun search(catalog: Catalog, query: String): List<Hit> {
        val q = normalize(query).trim()
        if (q.length < 2) return emptyList()
        val words = q.split(Regex("\\s+")).filter { it.length >= 2 }
        val expanded = words.flatMap { w -> listOf(w) + synonyms.entries.filter { it.key.startsWith(w) || w.startsWith(it.key) }.flatMap { it.value }.map(::normalize) }
        return catalog.tracks.mapNotNull { t ->
            val title = normalize(t.title)
            val desc = normalize(t.description)
            val cat = normalize(catalog.category(t.category)?.title ?: "")
            val body = normalize(t.text)
            var score = 0
            for (w in expanded) {
                if (title.contains(w)) score += 12
                if (title.split(" ").any { it.startsWith(w) }) score += 6
                if (cat.contains(w)) score += 6
                if (desc.contains(w)) score += 4
                if (body.contains(w)) score += 1
            }
            if (title.contains(q)) score += 20
            if (score > 0) Hit(t, score) else null
        }.sortedByDescending { it.score }
    }

    /** Suggestions adaptées à l'humeur déclarée. */
    fun forMood(catalog: Catalog, moodId: Int): List<Track> {
        val ids = when (moodId) {
            1 -> listOf("sleep_deep", "sleep_release", "story_cabin", "breath_478")
            2 -> listOf("breath_coherence", "breath_antistress", "sleep_restless", "med_acceptance")
            3 -> listOf("breath_slow", "relax_calm", "sleep_disconnect", "relax_mind")
            4 -> listOf("med_acceptance", "med_gratitude", "story_garden", "relax_letgo")
            5 -> listOf("med_mindful", "relax_deep", "story_stars", "breath_belly")
            6 -> listOf("med_gratitude", "story_ocean", "med_visualization", "sleep_journey")
            else -> listOf("med_gratitude", "med_confidence", "story_clouds", "med_visualization")
        }
        return ids.mapNotNull { catalog.track(it) }
    }

    fun moodMessage(moodId: Int) = when (moodId) {
        1 -> "Ton corps réclame du repos. Laisse-toi porter."
        2 -> "Respirons ensemble, tout doucement."
        3 -> "Ralentissons le rythme, pas à pas."
        4 -> "Accueille ce que tu ressens, avec tendresse."
        5 -> "Un moment simple, juste pour toi."
        6 -> "Prolonge cette douceur."
        else -> "Savoure cette lumière intérieure."
    }

    /** Séances recommandées selon l'heure et l'historique. */
    fun recommended(catalog: Catalog, hour: Int, history: List<HistoryEntry>, favorites: Set<String>): List<Track> {
        val pref = when (hour) {
            in 5..10 -> listOf("reveil", "respiration", "meditation")
            in 11..17 -> listOf("relaxation", "respiration", "meditation")
            in 18..20 -> listOf("relaxation", "meditation", "histoires")
            else -> listOf("sommeil", "histoires", "respiration")
        }
        val recent = history.take(10).map { it.mediaId.removePrefix("track:") }.toSet()
        return catalog.tracks
            .sortedWith(
                compareBy<Track>(
                    { pref.indexOf(it.category).let { i -> if (i < 0) 9 else i } },
                    { if (it.id in recent) 1 else 0 },
                    { if (it.id in favorites) 0 else 1 },
                ).thenBy { (it.id.hashCode() + hour) % 7 },
            )
            .take(8)
    }

    fun greeting(hour: Int, name: String): String {
        val base = when (hour) {
            in 5..11 -> "Bonjour"
            in 12..17 -> "Bel après-midi"
            in 18..21 -> "Bonsoir"
            else -> "Douce nuit"
        }
        return if (name.isBlank()) base else "$base, $name"
    }

    /** Série de jours consécutifs avec au moins une minute de pratique (aujourd'hui ou hier inclus). */
    fun streak(days: Map<String, Long>, today: java.time.LocalDate): Int {
        var d = if ((days[today.toString()] ?: 0) >= 60_000) today else today.minusDays(1)
        var n = 0
        while ((days[d.toString()] ?: 0) >= 60_000) { n++; d = d.minusDays(1) }
        return n
    }
}
