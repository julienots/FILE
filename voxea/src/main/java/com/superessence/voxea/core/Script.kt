package com.superessence.voxea.core

import kotlinx.serialization.Serializable

/** Types de voix proposés pour chaque personnage. */
@Serializable
enum class VoiceType(val label: String, val short: String) {
    ORIGINAL("Ma voix", "Ma voix"),
    FEMME("Voix de femme", "Femme"),
    HOMME("Voix d'homme", "Homme"),
    ENFANT("Voix d'enfant", "Enfant"),
    AGE("Personne âgée", "Âgée"),
    ROBOT("Robot", "Robot"),
}

/** Une réplique du script. [index] commence à 0. */
@Serializable
data class Line(
    val index: Int,
    val character: String,
    val text: String,
    val voiceHint: VoiceType? = null,
)

object ScriptParser {
    const val NARRATOR = "NARRATEUR"

    // \b de Java ne connaît pas les lettres accentuées (« ÂGÉ ») : on borne par \p{L}.
    private fun kw(p: String) = Regex("(?<!\\p{L})$p(?!\\p{L})", setOf(RegexOption.IGNORE_CASE))

    private val keywords: List<Pair<Regex, VoiceType>> = listOf(
        kw("""(ma\s*voix|moi|original|narrat\w*)""") to VoiceType.ORIGINAL,
        kw("""(femme|fille|f[ée]minine?|woman|female|dame)""") to VoiceType.FEMME,
        kw("""(homme|gar[çc]on|masculine?|man|male)""") to VoiceType.HOMME,
        kw("""(enfant|b[ée]b[ée]|child|kid|petit)""") to VoiceType.ENFANT,
        kw("""([âa]g[ée]e?|vieux|vieille|old|papi|mamie|grand-?p[èe]re|grand-?m[èe]re)""") to VoiceType.AGE,
        kw("""(robot|ia|ai|machine|android[e]?)""") to VoiceType.ROBOT,
    )

    /** Retourne le type de voix correspondant à un mot-clé (« femme », « robot »…), ou null. */
    fun voiceFromKeyword(s: String): VoiceType? = keywords.firstOrNull { it.first.containsMatchIn(s) }?.second

    // Balise de voix : [FEMME] ou (femme)
    private val tagRe = Regex("""[\[(]([^\])]{1,30})[\])]""")
    // Séparateur nom / texte : « : » ou tiret long « — » / « – »
    private val sepRe = Regex("""^(.{1,40}?)\s*(?::|\s[—–]\s|[—–])\s*(.+)$""")
    private val decorRe = Regex("""^[\s>*_\-•·]+|[*_]+""")

    fun parse(raw: String): List<Line> {
        data class Acc(var character: String, val sb: StringBuilder, var hint: VoiceType?)

        val out = mutableListOf<Acc>()
        var joinable = false // la ligne précédente n'était pas vide : une ligne simple la prolonge

        for (rawLine in raw.replace("\r\n", "\n").replace('\r', '\n').split('\n')) {
            val trimmed = rawLine.trim()
            if (trimmed.isEmpty()) { joinable = false; continue }
            if (trimmed.startsWith("#") || trimmed.matches(Regex("""^[-=*_]{3,}$"""))) { joinable = false; continue }

            val line = cleanStart(trimmed)
            val dialogue = splitDialogue(line)
            if (dialogue != null) {
                out += Acc(dialogue.first, StringBuilder(dialogue.second), dialogue.third)
                joinable = true
                continue
            }
            // Continuation : rattachée à la réplique ou au paragraphe précédent si pas de ligne vide
            val last = out.lastOrNull()
            if (last != null && joinable) {
                last.sb.append(' ').append(line)
            } else {
                out += Acc(NARRATOR, StringBuilder(line), null)
            }
            joinable = true
        }
        return out.filter { it.sb.isNotBlank() }
            .mapIndexed { i, a -> Line(i, a.character, a.sb.toString().trim(), a.hint) }
    }

    private fun cleanStart(s: String): String = s.replace(Regex("""^[>\-•·*]+\s+"""), "")

    /** (personnage, texte, voix) si la ligne est de la forme « NOM : texte ». */
    private fun splitDialogue(line: String): Triple<String, String, VoiceType?>? {
        var work = line
        var hint: VoiceType? = null
        // Balise en tête de ligne : « [ENFANT] Tom : Coucou »
        Regex("""^\[([^\]]{1,30})\]\s*""").find(work)?.let { m ->
            voiceFromKeyword(m.groupValues[1])?.let { hint = it; work = work.substring(m.range.last + 1) }
        }
        val m = sepRe.find(work) ?: return null
        var name = m.groupValues[1].replace(decorRe, "").trim()
        val text = m.groupValues[2].trim()
        tagRe.findAll(name).toList().forEach { t ->
            voiceFromKeyword(t.groupValues[1])?.let { hint = hint ?: it }
            name = name.replace(t.value, "").trim()
        }
        name = name.replace(decorRe, "").trim()
        if (!looksLikeName(name) || text.isEmpty()) return null
        return Triple(name, text, hint)
    }

    private fun looksLikeName(n: String): Boolean {
        if (n.isEmpty() || n.length > 30) return false
        if (n.last() in ".!?,;") return false
        val words = n.split(Regex("""\s+"""))
        if (words.size > 4) return false
        if (!n.first().isLetterOrDigit()) return false
        // Un nom commence par une majuscule (ou est entièrement en majuscules)
        return n.first().isUpperCase() || n.first().isDigit()
    }
}
