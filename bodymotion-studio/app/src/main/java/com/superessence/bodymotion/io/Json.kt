package com.superessence.bodymotion.io

/**
 * JSON minimal et sans dépendance Android : objets -> MutableMap, tableaux -> MutableList,
 * nombres -> Double, plus String, Boolean et null. Utilisé pour lire et réécrire les fichiers glTF
 * sans perdre aucun champ inconnu (matériaux, extensions VRM, etc.).
 */
object Json {
    fun parse(text: String): Any? = Parser(text).run { skipWs(); val v = value(); skipWs(); v }

    fun stringify(v: Any?): String = StringBuilder().also { write(it, v) }.toString()

    private fun write(sb: StringBuilder, v: Any?) {
        when (v) {
            null -> sb.append("null")
            is String -> writeString(sb, v)
            is Boolean -> sb.append(v)
            is Int, is Long, is Short, is Byte -> sb.append(v.toString())
            is Number -> {
                val d = v.toDouble()
                if (!d.isFinite()) sb.append("0")
                else if (d == Math.rint(d) && kotlin.math.abs(d) < 1e15) sb.append(d.toLong())
                else sb.append(d.toString())
            }
            is Map<*, *> -> {
                sb.append('{')
                var first = true
                for ((k, value) in v) {
                    if (!first) sb.append(',')
                    first = false
                    writeString(sb, k.toString()); sb.append(':'); write(sb, value)
                }
                sb.append('}')
            }
            is Iterable<*> -> {
                sb.append('['); var first = true
                for (e in v) { if (!first) sb.append(','); first = false; write(sb, e) }
                sb.append(']')
            }
            is DoubleArray -> write(sb, v.toList())
            is FloatArray -> write(sb, v.map { it.toDouble() })
            is IntArray -> write(sb, v.toList())
            else -> writeString(sb, v.toString())
        }
    }

    private fun writeString(sb: StringBuilder, s: String) {
        sb.append('"')
        for (c in s) when (c) {
            '"' -> sb.append("\\\"")
            '\\' -> sb.append("\\\\")
            '\n' -> sb.append("\\n")
            '\r' -> sb.append("\\r")
            '\t' -> sb.append("\\t")
            else -> if (c < ' ') sb.append(String.format("\\u%04x", c.code)) else sb.append(c)
        }
        sb.append('"')
    }

    private class Parser(val s: String) {
        var i = 0
        fun skipWs() { while (i < s.length && s[i].isWhitespace()) i++ }
        fun value(): Any? {
            skipWs()
            if (i >= s.length) throw IllegalArgumentException("JSON incomplet")
            return when (val c = s[i]) {
                '{' -> obj()
                '[' -> arr()
                '"' -> str()
                't' -> { expect("true"); true }
                'f' -> { expect("false"); false }
                'n' -> { expect("null"); null }
                else -> if (c == '-' || c.isDigit()) num() else throw IllegalArgumentException("JSON invalide à $i")
            }
        }
        fun expect(w: String) { if (!s.startsWith(w, i)) throw IllegalArgumentException("JSON invalide à $i"); i += w.length }
        fun obj(): MutableMap<String, Any?> {
            val m = LinkedHashMap<String, Any?>()
            i++; skipWs()
            if (s[i] == '}') { i++; return m }
            while (true) {
                skipWs(); val k = str(); skipWs()
                if (s[i] != ':') throw IllegalArgumentException("':' attendu à $i")
                i++
                m[k] = value(); skipWs()
                when (s[i]) { ',' -> i++; '}' -> { i++; return m }; else -> throw IllegalArgumentException("JSON invalide à $i") }
            }
        }
        fun arr(): MutableList<Any?> {
            val l = ArrayList<Any?>()
            i++; skipWs()
            if (s[i] == ']') { i++; return l }
            while (true) {
                l.add(value()); skipWs()
                when (s[i]) { ',' -> i++; ']' -> { i++; return l }; else -> throw IllegalArgumentException("JSON invalide à $i") }
            }
        }
        fun str(): String {
            if (s[i] != '"') throw IllegalArgumentException("Chaîne attendue à $i")
            i++
            val sb = StringBuilder()
            while (true) {
                val c = s[i++]
                when (c) {
                    '"' -> return sb.toString()
                    '\\' -> {
                        when (val e = s[i++]) {
                            'n' -> sb.append('\n'); 't' -> sb.append('\t'); 'r' -> sb.append('\r')
                            'b' -> sb.append('\b'); 'f' -> sb.append('\u000c')
                            'u' -> { sb.append(s.substring(i, i + 4).toInt(16).toChar()); i += 4 }
                            else -> sb.append(e)
                        }
                    }
                    else -> sb.append(c)
                }
            }
        }
        fun num(): Double {
            val st = i
            while (i < s.length && (s[i].isDigit() || s[i] in "+-.eE")) i++
            return s.substring(st, i).toDouble()
        }
    }
}

@Suppress("UNCHECKED_CAST")
fun Any?.obj(): MutableMap<String, Any?> = this as MutableMap<String, Any?>
@Suppress("UNCHECKED_CAST")
fun Any?.arr(): MutableList<Any?> = this as MutableList<Any?>
fun Map<String, Any?>.list(key: String): MutableList<Any?> = (this[key] as? MutableList<Any?>) ?: mutableListOf()
fun Map<String, Any?>.int(key: String, def: Int = -1): Int = (this[key] as? Number)?.toInt() ?: def
fun Map<String, Any?>.str(key: String): String? = this[key] as? String
fun Map<String, Any?>.doubles(key: String): List<Double>? = (this[key] as? List<*>)?.map { (it as Number).toDouble() }
