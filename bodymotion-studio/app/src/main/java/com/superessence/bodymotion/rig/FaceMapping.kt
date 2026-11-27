package com.superessence.bodymotion.rig

import com.superessence.bodymotion.anim.MorphChannel
import com.superessence.bodymotion.io.gltf.GltfDocument
import com.superessence.bodymotion.io.int
import com.superessence.bodymotion.io.list
import com.superessence.bodymotion.io.obj
import com.superessence.bodymotion.io.str

/**
 * Relie les 52 expressions détectées (noms ARKit renvoyés par MediaPipe) aux blendshapes du modèle :
 * 1) correspondance directe des noms ARKit,
 * 2) expressions VRM (0.x et 1.0 : clignements, voyelles, sourire…),
 * 3) noms courants (blink, mouthOpen, smile…).
 * Si le modèle n'a aucun blendshape compatible, la capture faciale est indiquée comme indisponible.
 */
class FaceRig(val channels: List<MorphChannel>, private val sources: List<(Map<String, Float>) -> Float>) {
    val isEmpty get() = channels.isEmpty()
    fun evaluate(bs: Map<String, Float>): FloatArray = FloatArray(channels.size) { sources[it](bs).coerceIn(0f, 1f) }
}

object FaceMapping {
    val ARKIT = listOf(
        "browDownLeft", "browDownRight", "browInnerUp", "browOuterUpLeft", "browOuterUpRight", "cheekPuff", "cheekSquintLeft", "cheekSquintRight",
        "eyeBlinkLeft", "eyeBlinkRight", "eyeLookDownLeft", "eyeLookDownRight", "eyeLookInLeft", "eyeLookInRight", "eyeLookOutLeft", "eyeLookOutRight",
        "eyeLookUpLeft", "eyeLookUpRight", "eyeSquintLeft", "eyeSquintRight", "eyeWideLeft", "eyeWideRight", "jawForward", "jawLeft", "jawOpen", "jawRight",
        "mouthClose", "mouthDimpleLeft", "mouthDimpleRight", "mouthFrownLeft", "mouthFrownRight", "mouthFunnel", "mouthLeft", "mouthLowerDownLeft",
        "mouthLowerDownRight", "mouthPressLeft", "mouthPressRight", "mouthPucker", "mouthRight", "mouthRollLower", "mouthRollUpper", "mouthShrugLower",
        "mouthShrugUpper", "mouthSmileLeft", "mouthSmileRight", "mouthStretchLeft", "mouthStretchRight", "mouthUpperUpLeft", "mouthUpperUpRight",
        "noseSneerLeft", "noseSneerRight",
    )
    private val arkitNorm = ARKIT.associateBy { norm(it) }

    private fun norm(s: String) = s.substringAfterLast('.').lowercase().replace(Regex("[^a-z0-9]"), "")
        .removePrefix("blendshape").removePrefix("arkit").removePrefix("fcl")

    private fun avg(bs: Map<String, Float>, vararg k: String) = k.map { bs[it] ?: 0f }.average().toFloat()

    /** Expressions VRM -> combinaison d'expressions ARKit. */
    private val vrmExpr: Map<String, (Map<String, Float>) -> Float> = mapOf(
        "blink" to { b -> avg(b, "eyeBlinkLeft", "eyeBlinkRight") },
        "blinkleft" to { b -> b["eyeBlinkLeft"] ?: 0f }, "blinkl" to { b -> b["eyeBlinkLeft"] ?: 0f },
        "blinkright" to { b -> b["eyeBlinkRight"] ?: 0f }, "blinkr" to { b -> b["eyeBlinkRight"] ?: 0f },
        "aa" to { b -> b["jawOpen"] ?: 0f }, "a" to { b -> b["jawOpen"] ?: 0f },
        "oh" to { b -> b["mouthFunnel"] ?: 0f }, "o" to { b -> b["mouthFunnel"] ?: 0f },
        "ou" to { b -> b["mouthPucker"] ?: 0f }, "u" to { b -> b["mouthPucker"] ?: 0f },
        "ee" to { b -> avg(b, "mouthStretchLeft", "mouthStretchRight") }, "e" to { b -> avg(b, "mouthStretchLeft", "mouthStretchRight") },
        "ih" to { b -> avg(b, "mouthSmileLeft", "mouthSmileRight") * 0.5f }, "i" to { b -> avg(b, "mouthSmileLeft", "mouthSmileRight") * 0.5f },
        "happy" to { b -> avg(b, "mouthSmileLeft", "mouthSmileRight") }, "joy" to { b -> avg(b, "mouthSmileLeft", "mouthSmileRight") },
        "angry" to { b -> avg(b, "browDownLeft", "browDownRight") },
        "sad" to { b -> avg(b, "mouthFrownLeft", "mouthFrownRight") }, "sorrow" to { b -> avg(b, "mouthFrownLeft", "mouthFrownRight") },
        "surprised" to { b -> avg(b, "browInnerUp", "eyeWideLeft", "eyeWideRight") },
    )

    /** Noms génériques fréquents. */
    private val generic: Map<String, (Map<String, Float>) -> Float> = vrmExpr + mapOf(
        "mouthopen" to { b -> b["jawOpen"] ?: 0f }, "jaw" to { b -> b["jawOpen"] ?: 0f }, "open" to { b -> b["jawOpen"] ?: 0f },
        "smile" to { b -> avg(b, "mouthSmileLeft", "mouthSmileRight") },
        "browup" to { b -> b["browInnerUp"] ?: 0f }, "browsup" to { b -> b["browInnerUp"] ?: 0f },
        "eyesclosed" to { b -> avg(b, "eyeBlinkLeft", "eyeBlinkRight") },
    )

    fun hasFaceTargets(skel: Skeleton) = !build(null, skel).isEmpty

    fun build(doc: GltfDocument?, skel: Skeleton): FaceRig {
        val ch = ArrayList<MorphChannel>(); val src = ArrayList<(Map<String, Float>) -> Float>()
        val used = HashSet<Pair<Int, Int>>()
        // 1) et 3) : par les noms des blendshapes.
        for (m in skel.morphTargets) m.targetNames.forEachIndexed { t, name ->
            val n = norm(name)
            val ark = arkitNorm[n]
            val f: ((Map<String, Float>) -> Float)? = if (ark != null) ({ b -> b[ark] ?: 0f }) else generic[n]
            if (f != null && used.add(m.nodeIndex to t)) { ch.add(MorphChannel(m.nodeIndex, t, name)); src.add(f) }
        }
        // 2) : expressions VRM déclarées dans le fichier.
        if (doc != null) addVrm(doc, skel, ch, src, used)
        return FaceRig(ch, src)
    }

    private fun addVrm(doc: GltfDocument, skel: Skeleton, ch: MutableList<MorphChannel>, src: MutableList<(Map<String, Float>) -> Float>, used: MutableSet<Pair<Int, Int>>) {
        val ext = doc.json["extensions"] as? Map<*, *> ?: return
        fun add(exprName: String, node: Int, index: Int, weight: Float) {
            val f = vrmExpr[norm(exprName)] ?: return
            if (node < 0 || !used.add(node to index)) return
            ch.add(MorphChannel(node, index, exprName)); src.add { b -> f(b) * weight }
        }
        // VRM 1.0
        ((ext["VRMC_vrm"] as? Map<*, *>)?.get("expressions") as? Map<*, *>)?.let { ex ->
            val preset = ex["preset"] as? Map<*, *> ?: return@let
            for ((k, v) in preset) {
                val binds = (v as? Map<*, *>)?.get("morphTargetBinds") as? List<*> ?: continue
                for (b in binds) { val m = b.obj(); add(k.toString(), m.int("node"), m.int("index"), ((m["weight"] as? Number)?.toFloat() ?: 1f)) }
            }
        }
        // VRM 0.x : les binds désignent un mesh ; on retrouve le nœud qui l'utilise.
        ((ext["VRM"] as? Map<*, *>)?.get("blendShapeMaster") as? Map<*, *>)?.let { bsm ->
            val groups = bsm["blendShapeGroups"] as? List<*> ?: return@let
            for (g in groups) {
                val gm = g.obj()
                val preset = gm.str("presetName")?.takeIf { it != "unknown" } ?: gm.str("name") ?: continue
                for (b in gm.list("binds")) {
                    val m = b.obj()
                    val meshIdx = m.int("mesh")
                    val node = doc.nodes.indexOfFirst { (it.obj()["mesh"] as? Number)?.toInt() == meshIdx }
                    add(preset, node, m.int("index"), ((m["weight"] as? Number)?.toFloat() ?: 100f) / 100f)
                }
            }
        }
        if (skel.morphTargets.isEmpty()) return
    }
}
