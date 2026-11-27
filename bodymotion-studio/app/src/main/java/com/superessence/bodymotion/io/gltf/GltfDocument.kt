package com.superessence.bodymotion.io.gltf

import com.superessence.bodymotion.io.Json
import com.superessence.bodymotion.io.arr
import com.superessence.bodymotion.io.int
import com.superessence.bodymotion.io.list
import com.superessence.bodymotion.io.obj
import com.superessence.bodymotion.io.str
import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.Base64

/**
 * Document glTF 2.0 en mémoire, toujours normalisé en « un seul buffer binaire » (forme GLB).
 * Tous les champs JSON sont conservés tels quels : matériaux, textures, extensions (VRM…).
 */
class GltfDocument(val json: MutableMap<String, Any?>, bin: ByteArray) {
    private val binOut = ByteArrayOutputStream().also { it.write(bin) }
    val bin: ByteArray get() = binOut.toByteArray()

    val nodes: MutableList<Any?> get() = json.getOrPut("nodes") { mutableListOf<Any?>() }.arr()
    val skins: MutableList<Any?> get() = json.list("skins")
    val meshes: MutableList<Any?> get() = json.list("meshes")
    val accessors: MutableList<Any?> get() = json.getOrPut("accessors") { mutableListOf<Any?>() }.arr()
    val bufferViews: MutableList<Any?> get() = json.getOrPut("bufferViews") { mutableListOf<Any?>() }.arr()

    fun clearBinary() { binOut.reset() }

    fun isVrm(): Boolean {
        val ext = json["extensions"] as? Map<*, *> ?: return false
        return ext.containsKey("VRM") || ext.containsKey("VRMC_vrm")
    }

    // ---------- Lecture d'accesseurs ----------

    fun readAccessor(index: Int): Array<DoubleArray> {
        val acc = accessors[index].obj()
        val count = acc.int("count")
        val n = components(acc.str("type") ?: "SCALAR")
        val ct = acc.int("componentType")
        val normalized = acc["normalized"] == true
        val out = Array(count) { DoubleArray(n) }
        val bvIndex = acc.int("bufferView")
        if (bvIndex >= 0) {
            val bv = bufferViews[bvIndex].obj()
            val base = bv.int("byteOffset", 0) + acc.int("byteOffset", 0)
            val csize = componentSize(ct)
            val stride = bv.int("byteStride", 0).let { if (it == 0) csize * n else it }
            val buf = ByteBuffer.wrap(bin).order(ByteOrder.LITTLE_ENDIAN)
            for (i in 0 until count) for (c in 0 until n) {
                out[i][c] = readComponent(buf, base + i * stride + c * csize, ct, normalized)
            }
        }
        (acc["sparse"] as? Map<*, *>)?.let { sp ->
            @Suppress("UNCHECKED_CAST") sp as Map<String, Any?>
            val sc = sp.int("count")
            val idx = sp["indices"].obj(); val vals = sp["values"].obj()
            val buf = ByteBuffer.wrap(bin).order(ByteOrder.LITTLE_ENDIAN)
            val ibv = bufferViews[idx.int("bufferView")].obj()
            val vbv = bufferViews[vals.int("bufferView")].obj()
            val ict = idx.int("componentType")
            for (k in 0 until sc) {
                val target = readComponent(buf, ibv.int("byteOffset", 0) + idx.int("byteOffset", 0) + k * componentSize(ict), ict, false).toInt()
                for (c in 0 until n) {
                    out[target][c] = readComponent(buf, vbv.int("byteOffset", 0) + vals.int("byteOffset", 0) + (k * n + c) * componentSize(ct), ct, normalized)
                }
            }
        }
        return out
    }

    private fun readComponent(b: ByteBuffer, off: Int, ct: Int, norm: Boolean): Double = when (ct) {
        5126 -> b.getFloat(off).toDouble()
        5125 -> (b.getInt(off).toLong() and 0xffffffffL).toDouble()
        5123 -> (b.getShort(off).toInt() and 0xffff).toDouble().let { if (norm) it / 65535.0 else it }
        5122 -> b.getShort(off).toDouble().let { if (norm) maxOf(it / 32767.0, -1.0) else it }
        5121 -> (b.get(off).toInt() and 0xff).toDouble().let { if (norm) it / 255.0 else it }
        5120 -> b.get(off).toDouble().let { if (norm) maxOf(it / 127.0, -1.0) else it }
        else -> 0.0
    }

    // ---------- Écriture ----------

    /** Ajoute des octets alignés sur 4 au buffer binaire et retourne l'index du bufferView créé. */
    fun addBufferView(data: ByteArray, target: Int? = null, byteStride: Int? = null): Int {
        while (binOut.size() % 4 != 0) binOut.write(0)
        val offset = binOut.size()
        binOut.write(data)
        val bv = linkedMapOf<String, Any?>("buffer" to 0, "byteOffset" to offset, "byteLength" to data.size)
        if (target != null) bv["target"] = target
        if (byteStride != null) bv["byteStride"] = byteStride
        bufferViews.add(bv)
        syncBufferLength()
        return bufferViews.size - 1
    }

    /** Ajoute un accesseur de flottants (type SCALAR, VEC3, VEC4, MAT4…). */
    fun addFloatAccessor(values: FloatArray, type: String, withMinMax: Boolean = false, target: Int? = null): Int {
        val bb = ByteBuffer.allocate(values.size * 4).order(ByteOrder.LITTLE_ENDIAN)
        values.forEach { bb.putFloat(it) }
        val bv = addBufferView(bb.array(), target)
        val n = components(type)
        val acc = linkedMapOf<String, Any?>("bufferView" to bv, "componentType" to 5126, "count" to values.size / n, "type" to type)
        if (withMinMax) {
            val mn = DoubleArray(n) { Double.MAX_VALUE }; val mx = DoubleArray(n) { -Double.MAX_VALUE }
            for (i in values.indices) { val c = i % n; mn[c] = minOf(mn[c], values[i].toDouble()); mx[c] = maxOf(mx[c], values[i].toDouble()) }
            acc["min"] = mn.toList(); acc["max"] = mx.toList()
        }
        accessors.add(acc)
        return accessors.size - 1
    }

    fun addIndexAccessor(values: IntArray): Int {
        val bb = ByteBuffer.allocate(values.size * 4).order(ByteOrder.LITTLE_ENDIAN)
        values.forEach { bb.putInt(it) }
        val bv = addBufferView(bb.array(), 34963)
        accessors.add(linkedMapOf("bufferView" to bv, "componentType" to 5125, "count" to values.size, "type" to "SCALAR"))
        return accessors.size - 1
    }

    fun addUShortVec4Accessor(values: IntArray): Int {
        val bb = ByteBuffer.allocate(values.size * 2).order(ByteOrder.LITTLE_ENDIAN)
        values.forEach { bb.putShort(it.toShort()) }
        val bv = addBufferView(bb.array(), 34962)
        accessors.add(linkedMapOf("bufferView" to bv, "componentType" to 5123, "count" to values.size / 4, "type" to "VEC4"))
        return accessors.size - 1
    }

    private fun syncBufferLength() {
        val buffers = json.getOrPut("buffers") { mutableListOf<Any?>() }.arr()
        if (buffers.isEmpty()) buffers.add(linkedMapOf<String, Any?>())
        buffers[0].obj().apply { remove("uri"); put("byteLength", binOut.size()) }
    }

    fun toGlb(): ByteArray {
        syncBufferLength()
        val asset = json.getOrPut("asset") { linkedMapOf<String, Any?>("version" to "2.0") }.obj()
        if (asset["version"] == null) asset["version"] = "2.0"
        var jsonBytes = Json.stringify(json).toByteArray(Charsets.UTF_8)
        val jpad = (4 - jsonBytes.size % 4) % 4
        jsonBytes += ByteArray(jpad) { 0x20 }
        var binBytes = bin
        val bpad = (4 - binBytes.size % 4) % 4
        binBytes += ByteArray(bpad)
        val total = 12 + 8 + jsonBytes.size + (if (binBytes.isNotEmpty()) 8 + binBytes.size else 0)
        val bb = ByteBuffer.allocate(total).order(ByteOrder.LITTLE_ENDIAN)
        bb.putInt(0x46546C67); bb.putInt(2); bb.putInt(total)
        bb.putInt(jsonBytes.size); bb.putInt(0x4E4F534A); bb.put(jsonBytes)
        if (binBytes.isNotEmpty()) { bb.putInt(binBytes.size); bb.putInt(0x004E4942); bb.put(binBytes) }
        return bb.array()
    }

    companion object {
        fun components(type: String) = when (type) {
            "SCALAR" -> 1; "VEC2" -> 2; "VEC3" -> 3; "VEC4" -> 4; "MAT2" -> 4; "MAT3" -> 9; "MAT4" -> 16; else -> 1
        }
        fun componentSize(ct: Int) = when (ct) { 5120, 5121 -> 1; 5122, 5123 -> 2; else -> 4 }

        fun isGlb(bytes: ByteArray) = bytes.size >= 12 && bytes[0] == 'g'.code.toByte() && bytes[1] == 'l'.code.toByte() && bytes[2] == 'T'.code.toByte() && bytes[3] == 'F'.code.toByte()

        fun parseGlb(bytes: ByteArray): GltfDocument {
            val bb = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN)
            require(bb.getInt(0) == 0x46546C67) { "Ce fichier n'est pas un GLB valide." }
            var off = 12
            var json: MutableMap<String, Any?>? = null
            var bin = ByteArray(0)
            while (off + 8 <= bytes.size) {
                val len = bb.getInt(off); val type = bb.getInt(off + 4)
                val chunk = bytes.copyOfRange(off + 8, off + 8 + len)
                if (type == 0x4E4F534A) json = Json.parse(String(chunk, Charsets.UTF_8)).obj()
                else if (type == 0x004E4942) bin = chunk
                off += 8 + len
            }
            return GltfDocument(json ?: throw IllegalArgumentException("GLB sans JSON."), bin).also { it.normalizeExternal { null } }
        }

        /**
         * Lit un .gltf texte. [resolve] renvoie le contenu d'un fichier externe (relatif) ou null.
         * Les buffers et images externes sont intégrés : le résultat est autonome.
         */
        fun parseGltf(text: String, resolve: (String) -> ByteArray?): GltfDocument {
            val doc = GltfDocument(Json.parse(text).obj(), ByteArray(0))
            doc.normalizeExternal(resolve)
            return doc
        }

        fun load(bytes: ByteArray, resolve: (String) -> ByteArray? = { null }): GltfDocument =
            if (isGlb(bytes)) parseGlb(bytes) else parseGltf(String(bytes, Charsets.UTF_8), resolve)

        private fun decodeUri(uri: String, resolve: (String) -> ByteArray?): ByteArray? {
            if (uri.startsWith("data:")) {
                val comma = uri.indexOf(',')
                return Base64.getDecoder().decode(uri.substring(comma + 1))
            }
            return resolve(java.net.URLDecoder.decode(uri, "UTF-8")) ?: resolve(uri)
        }
    }

    /** Fusionne tous les buffers en un seul et intègre les images externes. */
    private fun normalizeExternal(resolve: (String) -> ByteArray?) {
        val buffers = json.list("buffers")
        val merged = ByteArrayOutputStream()
        val offsets = IntArray(buffers.size)
        buffers.forEachIndexed { i, b ->
            val bo = b.obj()
            val data: ByteArray = when {
                i == 0 && bo["uri"] == null -> binOut.toByteArray()
                bo["uri"] != null -> decodeUri(bo.str("uri")!!, resolve)
                    ?: throw IllegalArgumentException("Fichier externe introuvable : ${bo.str("uri")}. Importez le .glb, ou un .zip contenant le .gltf et ses fichiers.")
                else -> ByteArray(0)
            }
            while (merged.size() % 4 != 0) merged.write(0)
            offsets[i] = merged.size()
            merged.write(data, 0, minOf(data.size, bo.int("byteLength", data.size)))
        }
        if (buffers.size > 1 || buffers.any { it.obj()["uri"] != null }) {
            for (bv in json.list("bufferViews")) {
                val m = bv.obj()
                val b = m.int("buffer", 0)
                m["byteOffset"] = m.int("byteOffset", 0) + offsets[b]
                m["buffer"] = 0
            }
            binOut.reset(); binOut.write(merged.toByteArray())
            json["buffers"] = mutableListOf<Any?>(linkedMapOf<String, Any?>("byteLength" to binOut.size()))
        }
        for (img in json.list("images")) {
            val m = img.obj()
            val uri = m.str("uri") ?: continue
            val data = decodeUri(uri, resolve) ?: continue
            m.remove("uri")
            m["mimeType"] = m.str("mimeType") ?: if (uri.lowercase().endsWith(".png") || uri.startsWith("data:image/png")) "image/png" else "image/jpeg"
            m["bufferView"] = addBufferView(data)
        }
    }
}
