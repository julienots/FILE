package com.superessence.bodymotion.export

import com.superessence.bodymotion.io.Json
import com.superessence.bodymotion.io.arr
import com.superessence.bodymotion.io.gltf.GltfDocument
import com.superessence.bodymotion.io.int
import com.superessence.bodymotion.io.list
import com.superessence.bodymotion.io.obj
import com.superessence.bodymotion.rig.Skeleton
import java.util.Base64

/**
 * Export glTF 2.0 / GLB réel :
 * - FULL : le fichier d'origine du personnage (maillages, matériaux, textures, extensions) + les animations ;
 * - ANIMATION_ONLY : la hiérarchie d'os seule + les animations.
 * Les noms d'os et les index de nœuds d'origine sont conservés.
 */
object GltfExporter {

    fun export(characterGlb: ByteArray, skel: Skeleton, clips: List<PreparedClip>, opt: ExportOptions): ByteArray {
        val doc = GltfDocument.parseGlb(characterGlb)
        if (opt.content == ExportContent.ANIMATION_ONLY) stripToSkeleton(doc)
        val hips = clips.first().hipsNode
        val rootNode = if (clips.any { it.rootT != null }) insertRootBone(doc, skel, hips) else -1
        val anims = doc.json.getOrPut("animations") { mutableListOf<Any?>() }.arr()
        for (c in clips) anims.add(buildAnimation(doc, c, opt, rootNode))
        doc.json.obj()["asset"] = linkedMapOf<String, Any?>("version" to "2.0", "generator" to "BodyMotion Studio — Super Essence Studio")
        val glb = doc.toGlb()
        return if (opt.format == ExportFormat.GLTF) toEmbeddedGltf(doc) else glb
    }

    /** Ne garde que la hiérarchie et les skins (joints + matrices de liaison) : un squelette sans maillage. */
    private fun stripToSkeleton(doc: GltfDocument) {
        val json = doc.json
        val skins = doc.skins.map { s ->
            val m = s.obj()
            val ibm = m.int("inverseBindMatrices").let { if (it >= 0) doc.readAccessor(it) else null }
            Triple(m["joints"], m["name"], ibm)
        }
        for (n in doc.nodes) { val m = n.obj(); m.remove("mesh"); m.remove("skin"); m.remove("camera"); m.remove("weights") }
        listOf("meshes", "skins", "materials", "textures", "images", "samplers", "cameras", "animations",
            "extensions", "extensionsUsed", "extensionsRequired", "accessors", "bufferViews", "buffers").forEach { json.remove(it) }
        doc.clearBinary()
        if (skins.isNotEmpty()) {
            json["skins"] = skins.map { (joints, name, ibm) ->
                val sk = linkedMapOf<String, Any?>("joints" to joints)
                if (name != null) sk["name"] = name
                if (ibm != null) sk["inverseBindMatrices"] = doc.addFloatAccessor(FloatArray(ibm.size * 16) { ibm[it / 16][it % 16].toFloat() }, "MAT4")
                sk
            }.toMutableList<Any?>()
        }
    }

    /** Insère un nœud « root » entre le bassin et son parent (et l'ajoute aux joints des skins). */
    private fun insertRootBone(doc: GltfDocument, skel: Skeleton, hips: Int): Int {
        val nodes = doc.nodes
        val parent = skel.nodes[hips].parent
        val rootIdx = nodes.size
        nodes.add(linkedMapOf<String, Any?>("name" to "root", "children" to mutableListOf<Any?>(hips)))
        if (parent >= 0) {
            val ch = nodes[parent].obj().list("children")
            val i = ch.indexOfFirst { (it as Number).toInt() == hips }
            ch[i] = rootIdx
            nodes[parent].obj()["children"] = ch
        } else {
            for (s in doc.json.list("scenes")) {
                val sn = s.obj().list("nodes")
                val i = sn.indexOfFirst { (it as Number).toInt() == hips }
                if (i >= 0) { sn[i] = rootIdx; s.obj()["nodes"] = sn }
            }
        }
        // Joint supplémentaire : matrice inverse de liaison = inverse de sa position de repos (celle du parent).
        val parentM = if (parent >= 0) skel.restGlobal[parent] else com.superessence.bodymotion.math.Mat4()
        val ibm = parentM.inverse().toFloatArray()
        for (s in doc.skins) {
            val sk = s.obj()
            val joints = sk.list("joints")
            if (joints.none { (it as Number).toInt() == hips }) continue
            val old = sk.int("inverseBindMatrices")
            val values = ArrayList<Float>()
            if (old >= 0) doc.readAccessor(old).forEach { m -> m.forEach { values.add(it.toFloat()) } }
            else repeat(joints.size) { values.addAll(listOf(1f, 0f, 0f, 0f, 0f, 1f, 0f, 0f, 0f, 0f, 1f, 0f, 0f, 0f, 0f, 1f)) }
            joints.add(rootIdx); values.addAll(ibm.toList())
            sk["joints"] = joints
            sk["inverseBindMatrices"] = doc.addFloatAccessor(values.toFloatArray(), "MAT4")
        }
        return rootIdx
    }

    private fun buildAnimation(doc: GltfDocument, c: PreparedClip, opt: ExportOptions, rootNode: Int): MutableMap<String, Any?> {
        val samplers = mutableListOf<Any?>(); val channels = mutableListOf<Any?>()
        val timeCache = HashMap<String, Int>()
        fun timeAccessor(frames: IntArray): Int = timeCache.getOrPut(frames.joinToString(",")) {
            doc.addFloatAccessor(FloatArray(frames.size) { (frames[it] / c.fps).toFloat() }, "SCALAR", withMinMax = true)
        }
        fun add(node: Int, path: String, input: Int, output: Int) {
            samplers.add(linkedMapOf("input" to input, "output" to output, "interpolation" to "LINEAR"))
            channels.add(linkedMapOf("sampler" to samplers.size - 1, "target" to linkedMapOf("node" to node, "path" to path)))
        }
        val tol = opt.quality.toleranceDeg
        c.nodes.forEachIndexed { k, node ->
            val frames = c.keyFramesFor(k, tol)
            val out = FloatArray(frames.size * 4)
            var prev: com.superessence.bodymotion.math.Quat? = null
            frames.forEachIndexed { i, f ->
                var q = c.rotations[f][k]
                if (prev != null && prev!!.dot(q) < 0) q = q.negate() // continuité (pas de saut d'hémisphère)
                prev = q
                out[i * 4] = q.x.toFloat(); out[i * 4 + 1] = q.y.toFloat(); out[i * 4 + 2] = q.z.toFloat(); out[i * 4 + 3] = q.w.toFloat()
            }
            add(node, "rotation", timeAccessor(frames), doc.addFloatAccessor(out, "VEC4"))
        }
        val hf = c.keyFramesForVec(c.hipsT, tol)
        add(c.hipsNode, "translation", timeAccessor(hf), doc.addFloatAccessor(vecs(c.hipsT, hf), "VEC3"))
        if (c.rootT != null && rootNode >= 0) {
            val rf = c.keyFramesForVec(c.rootT, tol)
            add(rootNode, "translation", timeAccessor(rf), doc.addFloatAccessor(vecs(c.rootT, rf), "VEC3"))
        }
        // Visage : un canal « weights » par nœud de maillage (toutes ses cibles).
        val src = c.source
        if (opt.includeFace && opt.content == ExportContent.FULL && src.morph.isNotEmpty() && src.morphChannels.isNotEmpty()) {
            val byNode = src.morphChannels.withIndex().groupBy { it.value.node }
            val all = IntArray(c.frames) { it }
            for ((node, list) in byNode) {
                val meshIdx = doc.nodes[node].obj().int("mesh"); if (meshIdx < 0) continue
                val prim = doc.meshes[meshIdx].obj().list("primitives").firstOrNull()?.obj() ?: continue
                val nT = prim.list("targets").size; if (nT == 0) continue
                val out = FloatArray(c.frames * nT)
                for (f in 0 until c.frames) {
                    val srcF = ((f.toDouble() / c.fps) * src.fps).toInt().coerceIn(0, src.morph.size - 1)
                    for (e in list) out[f * nT + e.value.target] = src.morph[srcF][e.index]
                }
                add(node, "weights", timeAccessor(all), doc.addFloatAccessor(out, "SCALAR"))
            }
        }
        return linkedMapOf("name" to c.name, "samplers" to samplers, "channels" to channels)
    }

    private fun vecs(v: Array<com.superessence.bodymotion.math.Vec3>, frames: IntArray) = FloatArray(frames.size * 3).also { o ->
        frames.forEachIndexed { i, f -> o[i * 3] = v[f].x.toFloat(); o[i * 3 + 1] = v[f].y.toFloat(); o[i * 3 + 2] = v[f].z.toFloat() }
    }

    /** glTF texte autonome (buffer intégré en base64). */
    private fun toEmbeddedGltf(doc: GltfDocument): ByteArray {
        val bin = doc.bin
        val json = doc.json
        json["buffers"] = mutableListOf<Any?>(linkedMapOf<String, Any?>(
            "byteLength" to bin.size,
            "uri" to "data:application/octet-stream;base64," + Base64.getEncoder().encodeToString(bin),
        ))
        return Json.stringify(json).toByteArray(Charsets.UTF_8)
    }
}
