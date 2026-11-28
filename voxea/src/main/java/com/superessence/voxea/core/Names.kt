package com.superessence.voxea.core

import java.text.Normalizer

/** « Léa » → « LEA », « Jean-Pierre » → « JEAN_PIERRE ». */
fun asciiTag(s: String, fallback: String = "REPLIQUE"): String {
    val n = Normalizer.normalize(s, Normalizer.Form.NFD).replace(Regex("""\p{M}+"""), "")
        .uppercase().replace(Regex("""[^A-Z0-9]+"""), "_").trim('_')
    return n.ifEmpty { fallback }.take(24)
}

/** Nom du MP3 d'une réplique : « 04_LEA.mp3 ». */
fun fileNameFor(line: Line): String = "%02d_%s.mp3".format(line.index + 1, asciiTag(line.character))
