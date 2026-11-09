package com.superessence.noctea.tts

import com.superessence.noctea.data.TextClip
import java.io.File

/**
 * Découpe un texte libre en fragments à synthétiser, avec de vraies pauses :
 *  - « ... » → pause courte (≈1,2 s × facteur),
 *  - fin de phrase → pause moyenne,
 *  - ligne vide (nouveau paragraphe) → pause longue,
 *  - [pause 10] → silence explicite de 10 secondes.
 */
object TextComposer {
    data class Fragment(val text: String, val pauseAfterMs: Long)

    private val explicitPause = Regex("""\[pause\s+(\d{1,3})\s*s?]""", RegexOption.IGNORE_CASE)

    fun split(text: String, pauseFactor: Float = 1f): List<Fragment> {
        val out = mutableListOf<Fragment>()
        val paragraphs = text.replace("…", "...").split(Regex("""\n\s*\n"""))
        paragraphs.forEachIndexed { pi, para ->
            val tokens = explicitPause.split(para)
            val pauses = explicitPause.findAll(para).map { it.groupValues[1].toLong() * 1000 }.toList()
            tokens.forEachIndexed { ti, tok ->
                val sentences = splitSentences(tok)
                sentences.forEach { (s, kind) ->
                    val base = when (kind) {
                        Kind.ELLIPSIS -> 1_250L
                        Kind.SENTENCE -> 2_200L
                        Kind.QUESTION -> 2_600L
                    }
                    out += Fragment(s, (base * pauseFactor).toLong())
                }
                if (ti < pauses.size && out.isNotEmpty()) {
                    val last = out.removeAt(out.size - 1)
                    out += last.copy(pauseAfterMs = last.pauseAfterMs + pauses[ti])
                }
            }
            if (pi < paragraphs.size - 1 && out.isNotEmpty()) {
                val last = out.removeAt(out.size - 1)
                out += last.copy(pauseAfterMs = last.pauseAfterMs + (3_500L * pauseFactor).toLong())
            }
        }
        return out.filter { it.text.any(Char::isLetterOrDigit) }
    }

    private enum class Kind { ELLIPSIS, SENTENCE, QUESTION }

    private fun splitSentences(s: String): List<Pair<String, Kind>> {
        val res = mutableListOf<Pair<String, Kind>>()
        val sb = StringBuilder()
        var i = 0
        val t = s.replace('\n', ' ')
        while (i < t.length) {
            val c = t[i]
            sb.append(c)
            if (t.startsWith("...", i)) {
                sb.append(".."); i += 3
                res += sb.toString().trim() to Kind.ELLIPSIS; sb.clear(); continue
            }
            if (c == '.' || c == '!' || c == '?') {
                val next = t.getOrNull(i + 1)
                if (next == null || next == ' ' || next == '"' || next == '»') {
                    res += sb.toString().trim() to (if (c == '?') Kind.QUESTION else Kind.SENTENCE); sb.clear()
                }
            }
            i++
        }
        if (sb.isNotBlank()) res += sb.toString().trim() to Kind.SENTENCE
        return res.filter { it.first.isNotBlank() }
    }

    /** Synthétise tous les fragments dans [dir]. */
    suspend fun synthesize(
        provider: VoiceProvider,
        fragments: List<Fragment>,
        dir: File,
        onProgress: (Int, Int) -> Unit,
    ): List<TextClip> {
        dir.mkdirs()
        val clips = mutableListOf<TextClip>()
        fragments.forEachIndexed { i, f ->
            onProgress(i, fragments.size)
            val ext = if (provider.id == "system") "wav" else "audio"
            val file = File(dir, "f%03d.%s".format(i, ext))
            val ms = provider.synthesize(f.text, file)
            clips += TextClip(file.absolutePath, ms.coerceAtLeast(300), f.pauseAfterMs)
        }
        onProgress(fragments.size, fragments.size)
        return clips
    }
}
