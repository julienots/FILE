package com.superessence.bodymotion.io.fbx

import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.zip.Inflater

/** Nœud FBX générique (nom, propriétés, enfants) utilisé en lecture et en écriture binaire. */
class FbxNode(val name: String, val props: MutableList<Any> = mutableListOf(), val children: MutableList<FbxNode> = mutableListOf()) {
    fun child(n: String) = children.firstOrNull { it.name == n }
    fun all(n: String) = children.filter { it.name == n }
    fun add(n: String, vararg p: Any): FbxNode = FbxNode(n, p.toMutableList()).also { children.add(it) }

    /** Raccourci : Properties70 -> P "Nom", type, label, flags, valeurs… */
    fun p70(name: String, type: String, label: String, flags: String, vararg values: Any) {
        val p = child("Properties70") ?: add("Properties70")
        p.children.add(FbxNode("P", mutableListOf<Any>(name, type, label, flags).apply { addAll(values) }))
    }

    fun prop70(name: String): List<Any>? = child("Properties70")?.children?.firstOrNull { it.props.firstOrNull() == name }?.props?.drop(4)

    override fun toString() = "$name${props.take(3)}"
}

/** Wrapper pour forcer le type « octets bruts » (R) à l'écriture. */
class FbxRaw(val bytes: ByteArray)

object FbxBinary {
    private val HEADER = "Kaydara FBX Binary  ".toByteArray(Charsets.US_ASCII) + byteArrayOf(0, 0x1a, 0)
    val FILE_ID = byteArrayOf(0x28, 0xb3.toByte(), 0x2a, 0xeb.toByte(), 0xb6.toByte(), 0x24, 0xcc.toByte(), 0xc2.toByte(), 0xbf.toByte(), 0xc8.toByte(), 0xb0.toByte(), 0x2a, 0xa9.toByte(), 0x2b, 0xfc.toByte(), 0xf1.toByte())
    private val FOOT_ID = byteArrayOf(0xfa.toByte(), 0xbc.toByte(), 0xab.toByte(), 0x09, 0xd0.toByte(), 0xc8.toByte(), 0xd4.toByte(), 0x66, 0xb1.toByte(), 0x76, 0xfb.toByte(), 0x83.toByte(), 0x1c, 0xf7.toByte(), 0x26, 0x7e)
    private val FOOT_MAGIC = byteArrayOf(0xf8.toByte(), 0x5a, 0x8c.toByte(), 0x6a, 0xde.toByte(), 0xf5.toByte(), 0xd9.toByte(), 0x7e, 0xec.toByte(), 0xe9.toByte(), 0x0c, 0xe3.toByte(), 0x75, 0x8f.toByte(), 0x29, 0x0b)

    // ------------------------------------------------------------------ écriture (7400, offsets 32 bits)

    fun write(top: List<FbxNode>, version: Int = 7400): ByteArray {
        val out = Out()
        out.bytes(HEADER); out.u32(version)
        top.forEachIndexed { i, n -> writeNode(out, n, i == top.size - 1) }
        out.bytes(ByteArray(13))
        out.bytes(FOOT_ID)
        val ofs = out.size and 0x0f
        var pad = ((ofs + 15) and 0x0f.inv()) - ofs
        if (pad == 0) pad = 16
        out.bytes(ByteArray(pad))
        out.u32(version)
        out.bytes(ByteArray(120))
        out.bytes(FOOT_MAGIC)
        return out.toByteArray()
    }

    private fun writeNode(o: Out, n: FbxNode, isLast: Boolean) {
        val start = o.size
        o.u32(0); o.u32(n.props.size); o.u32(0)
        val nameB = n.name.toByteArray(Charsets.UTF_8)
        o.u8(nameB.size); o.bytes(nameB)
        val propStart = o.size
        n.props.forEach { writeProp(o, it) }
        val propLen = o.size - propStart
        if (n.children.isNotEmpty()) {
            n.children.forEachIndexed { i, c -> writeNode(o, c, i == n.children.size - 1) }
            o.bytes(ByteArray(13))
        } else if ((n.props.isEmpty() && !isLast) || n.name == "AnimationStack" || n.name == "AnimationLayer") {
            o.bytes(ByteArray(13))
        }
        o.patchU32(start, o.size); o.patchU32(start + 8, propLen)
    }

    private fun writeProp(o: Out, p: Any) {
        when (p) {
            is Boolean -> { o.u8('C'.code); o.u8(if (p) 1 else 0) }
            is Short -> { o.u8('Y'.code); o.u16(p.toInt()) }
            is Int -> { o.u8('I'.code); o.u32(p) }
            is Long -> { o.u8('L'.code); o.u64(p) }
            is Float -> { o.u8('F'.code); o.f32(p) }
            is Double -> { o.u8('D'.code); o.f64(p) }
            is String -> { val b = p.toByteArray(Charsets.UTF_8); o.u8('S'.code); o.u32(b.size); o.bytes(b) }
            is FbxRaw -> { o.u8('R'.code); o.u32(p.bytes.size); o.bytes(p.bytes) }
            is DoubleArray -> { o.u8('d'.code); o.u32(p.size); o.u32(0); o.u32(p.size * 8); p.forEach { o.f64(it) } }
            is FloatArray -> { o.u8('f'.code); o.u32(p.size); o.u32(0); o.u32(p.size * 4); p.forEach { o.f32(it) } }
            is IntArray -> { o.u8('i'.code); o.u32(p.size); o.u32(0); o.u32(p.size * 4); p.forEach { o.u32(it) } }
            is LongArray -> { o.u8('l'.code); o.u32(p.size); o.u32(0); o.u32(p.size * 8); p.forEach { o.u64(it) } }
            else -> throw IllegalArgumentException("Type FBX non géré : ${p::class}")
        }
    }

    private class Out {
        private val b = ByteArrayOutputStream()
        private var patches = ArrayList<Pair<Int, Int>>()
        val size get() = b.size()
        fun u8(v: Int) = b.write(v)
        fun u16(v: Int) { b.write(v and 0xff); b.write((v shr 8) and 0xff) }
        fun u32(v: Int) { for (i in 0 until 4) b.write((v shr (8 * i)) and 0xff) }
        fun u64(v: Long) { for (i in 0 until 8) b.write(((v shr (8 * i)) and 0xff).toInt()) }
        fun f32(v: Float) = u32(java.lang.Float.floatToIntBits(v))
        fun f64(v: Double) = u64(java.lang.Double.doubleToLongBits(v))
        fun bytes(a: ByteArray) = b.write(a)
        fun patchU32(at: Int, v: Int) { patches.add(at to v) }
        fun toByteArray(): ByteArray {
            val a = b.toByteArray()
            val bb = ByteBuffer.wrap(a).order(ByteOrder.LITTLE_ENDIAN)
            patches.forEach { (at, v) -> bb.putInt(at, v) }
            return a
        }
    }

    // ------------------------------------------------------------------ lecture (7.x, 32 et 64 bits)

    fun isBinary(bytes: ByteArray) = bytes.size > 27 && String(bytes, 0, 18, Charsets.US_ASCII) == "Kaydara FBX Binary"

    fun read(bytes: ByteArray): List<FbxNode> {
        require(isBinary(bytes)) { "Ce FBX est au format texte (ASCII). Réexportez-le en FBX binaire (par défaut dans Blender, Maya, Mixamo)." }
        val bb = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN)
        val version = bb.getInt(23)
        val wide = version >= 7500
        var pos = 27
        val out = ArrayList<FbxNode>()
        while (pos < bytes.size) {
            val (node, next) = readNode(bb, pos, wide) ?: break
            out.add(node); pos = next
        }
        return out
    }

    private fun readNode(bb: ByteBuffer, pos0: Int, wide: Boolean): Pair<FbxNode, Int>? {
        var pos = pos0
        val end: Long; val nProps: Long
        if (wide) { end = bb.getLong(pos); nProps = bb.getLong(pos + 8); pos += 24 } else { end = bb.getInt(pos).toLong() and 0xffffffffL; nProps = bb.getInt(pos + 4).toLong() and 0xffffffffL; pos += 12 }
        if (end == 0L) return null
        val nameLen = bb.get(pos).toInt() and 0xff; pos++
        val name = String(ByteArray(nameLen) { bb.get(pos + it) }, Charsets.UTF_8); pos += nameLen
        val node = FbxNode(name)
        for (i in 0 until nProps) {
            val t = bb.get(pos).toInt().toChar(); pos++
            when (t) {
                'Y' -> { node.props.add(bb.getShort(pos)); pos += 2 }
                'C' -> { node.props.add(bb.get(pos) != 0.toByte()); pos += 1 }
                'I' -> { node.props.add(bb.getInt(pos)); pos += 4 }
                'F' -> { node.props.add(bb.getFloat(pos)); pos += 4 }
                'D' -> { node.props.add(bb.getDouble(pos)); pos += 8 }
                'L' -> { node.props.add(bb.getLong(pos)); pos += 8 }
                'S', 'R' -> {
                    val len = bb.getInt(pos); pos += 4
                    val a = ByteArray(len) { bb.get(pos + it) }; pos += len
                    node.props.add(if (t == 'S') String(a, Charsets.UTF_8) else FbxRaw(a))
                }
                'f', 'd', 'l', 'i', 'b' -> {
                    val count = bb.getInt(pos); val enc = bb.getInt(pos + 4); val clen = bb.getInt(pos + 8); pos += 12
                    var data = ByteArray(clen) { bb.get(pos + it) }; pos += clen
                    val elem = when (t) { 'f', 'i' -> 4; 'd', 'l' -> 8; else -> 1 }
                    if (enc == 1) {
                        val inf = Inflater(); inf.setInput(data)
                        val res = ByteArray(count * elem); var o = 0
                        while (o < res.size && !inf.finished()) { val k = inf.inflate(res, o, res.size - o); if (k == 0 && inf.needsInput()) break; o += k }
                        inf.end(); data = res
                    }
                    val ab = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN)
                    node.props.add(when (t) {
                        'f' -> FloatArray(count) { ab.getFloat(it * 4) }
                        'd' -> DoubleArray(count) { ab.getDouble(it * 8) }
                        'l' -> LongArray(count) { ab.getLong(it * 8) }
                        'i' -> IntArray(count) { ab.getInt(it * 4) }
                        else -> BooleanArray(count) { data[it] != 0.toByte() }
                    })
                }
                else -> throw IllegalArgumentException("Propriété FBX inconnue '$t'")
            }
        }
        while (pos < end) {
            val r = readNode(bb, pos, wide)
            if (r == null) { pos += if (wide) 25 else 13; break }
            node.children.add(r.first); pos = r.second
        }
        return node to end.toInt()
    }
}

fun Any.asDoubles(): DoubleArray = when (this) {
    is DoubleArray -> this
    is FloatArray -> DoubleArray(size) { this[it].toDouble() }
    is IntArray -> DoubleArray(size) { this[it].toDouble() }
    is LongArray -> DoubleArray(size) { this[it].toDouble() }
    else -> DoubleArray(0)
}
fun Any.asInts(): IntArray = when (this) {
    is IntArray -> this
    is LongArray -> IntArray(size) { this[it].toInt() }
    is DoubleArray -> IntArray(size) { this[it].toInt() }
    else -> IntArray(0)
}
fun Any.num(): Double = (this as Number).toDouble()
