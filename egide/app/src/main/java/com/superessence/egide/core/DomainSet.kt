package com.superessence.egide.core

/**
 * Ensemble compact de domaines : chaque domaine est réduit à une empreinte 64 bits,
 * stockée dans un tableau trié (8 octets par domaine au lieu d'environ 100 avec un HashSet).
 * 360 000 domaines tiennent ainsi dans environ 3 Mo.
 */
class DomainSet private constructor(private val hashes: LongArray) {

    val size: Int get() = hashes.size

    fun containsExact(domain: String): Boolean =
        hashes.isNotEmpty() && java.util.Arrays.binarySearch(hashes, hash(domain)) >= 0

    /** Vrai si le domaine ou l'un de ses parents (ads.exemple.com → exemple.com) est présent. */
    fun matches(host: String): Boolean {
        if (hashes.isEmpty()) return false
        var h = host
        while (true) {
            if (containsExact(h)) return true
            val dot = h.indexOf('.')
            if (dot < 0) return false
            h = h.substring(dot + 1)
            // On ne teste jamais un suffixe public seul (« com », « co.uk »…).
            if (h.indexOf('.') < 0) return false
        }
    }

    class Builder(capacity: Int = 1024) {
        private var data = LongArray(capacity)
        private var n = 0
        fun add(domain: String) {
            if (n == data.size) data = data.copyOf(data.size * 2)
            data[n++] = hash(domain)
        }
        fun build(): DomainSet {
            val arr = data.copyOf(n)
            arr.sort()
            // dédoublonnage
            var w = 0
            for (i in arr.indices) if (i == 0 || arr[i] != arr[i - 1]) arr[w++] = arr[i]
            return DomainSet(arr.copyOf(w))
        }
    }

    fun toLongArray(): LongArray = hashes

    companion object {
        val EMPTY = DomainSet(LongArray(0))

        fun fromSorted(arr: LongArray) = DomainSet(arr)

        /** FNV-1a 64 bits suivi d'un mélange SplitMix64. */
        fun hash(s: String): Long {
            var h = -0x340d631b7bdddcdbL
            for (c in s) {
                h = h xor c.code.toLong()
                h *= 0x100000001b3L
            }
            h = (h xor (h ushr 30)) * -0x40a7b892e31b1a47L
            h = (h xor (h ushr 27)) * -0x6b2fb644ecceee15L
            return h xor (h ushr 31)
        }

        /**
         * Extrait un domaine d'une ligne de liste, quel que soit le format :
         * « 0.0.0.0 pub.com », « 127.0.0.1 pub.com », « pub.com », « ||pub.com^ », « *.pub.com ».
         */
        fun parseLine(raw: String): String? {
            var line = raw.trim()
            if (line.isEmpty() || line[0] == '#' || line[0] == '!' || line[0] == '[') return null
            val hashIdx = line.indexOf('#')
            if (hashIdx > 0) line = line.substring(0, hashIdx).trim()
            if (line.startsWith("||")) {
                line = line.substring(2)
                val end = line.indexOfFirst { it == '^' || it == '/' || it == '$' }
                if (end >= 0) line = line.substring(0, end)
            } else {
                val parts = line.split(' ', '\t').filter { it.isNotEmpty() }
                if (parts.isEmpty()) return null
                line = if (parts.size >= 2 && (parts[0] == "0.0.0.0" || parts[0] == "127.0.0.1" || parts[0] == "::" || parts[0] == "::1")) parts[1] else parts[0]
            }
            line = line.removePrefix("*.").removePrefix(".").lowercase()
            if (line == "localhost" || line == "0.0.0.0" || line.indexOf('.') < 0) return null
            for (c in line) if (!(c in 'a'..'z' || c in '0'..'9' || c == '.' || c == '-' || c == '_')) return null
            return line
        }
    }
}
