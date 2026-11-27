package com.superessence.bodymotion.rig

import com.superessence.bodymotion.io.gltf.GltfDocument
import com.superessence.bodymotion.io.int
import com.superessence.bodymotion.io.obj
import com.superessence.bodymotion.io.str

/** Os humanoïdes standards (même nomenclature que VRM / Unity Humanoid). */
enum class HumanBone(val label: String, val required: Boolean = false, val group: BodyGroup) {
    Hips("Bassin", true, BodyGroup.Torso),
    Spine("Colonne", true, BodyGroup.Torso),
    Chest("Poitrine", false, BodyGroup.Torso),
    UpperChest("Haut du torse", false, BodyGroup.Torso),
    Neck("Cou", false, BodyGroup.Head),
    Head("Tête", true, BodyGroup.Head),
    Jaw("Mâchoire", false, BodyGroup.Head),
    LeftShoulder("Épaule G", false, BodyGroup.LeftArm),
    LeftUpperArm("Bras G", true, BodyGroup.LeftArm),
    LeftLowerArm("Avant-bras G", true, BodyGroup.LeftArm),
    LeftHand("Main G", true, BodyGroup.LeftArm),
    RightShoulder("Épaule D", false, BodyGroup.RightArm),
    RightUpperArm("Bras D", true, BodyGroup.RightArm),
    RightLowerArm("Avant-bras D", true, BodyGroup.RightArm),
    RightHand("Main D", true, BodyGroup.RightArm),
    LeftUpperLeg("Cuisse G", true, BodyGroup.LeftLeg),
    LeftLowerLeg("Jambe G", true, BodyGroup.LeftLeg),
    LeftFoot("Pied G", true, BodyGroup.LeftLeg),
    LeftToes("Orteils G", false, BodyGroup.LeftLeg),
    RightUpperLeg("Cuisse D", true, BodyGroup.RightLeg),
    RightLowerLeg("Jambe D", true, BodyGroup.RightLeg),
    RightFoot("Pied D", true, BodyGroup.RightLeg),
    RightToes("Orteils D", false, BodyGroup.RightLeg),
    LeftThumbProximal("Pouce 1 G", group = BodyGroup.LeftArm), LeftThumbIntermediate("Pouce 2 G", group = BodyGroup.LeftArm), LeftThumbDistal("Pouce 3 G", group = BodyGroup.LeftArm),
    LeftIndexProximal("Index 1 G", group = BodyGroup.LeftArm), LeftIndexIntermediate("Index 2 G", group = BodyGroup.LeftArm), LeftIndexDistal("Index 3 G", group = BodyGroup.LeftArm),
    LeftMiddleProximal("Majeur 1 G", group = BodyGroup.LeftArm), LeftMiddleIntermediate("Majeur 2 G", group = BodyGroup.LeftArm), LeftMiddleDistal("Majeur 3 G", group = BodyGroup.LeftArm),
    LeftRingProximal("Annulaire 1 G", group = BodyGroup.LeftArm), LeftRingIntermediate("Annulaire 2 G", group = BodyGroup.LeftArm), LeftRingDistal("Annulaire 3 G", group = BodyGroup.LeftArm),
    LeftLittleProximal("Auriculaire 1 G", group = BodyGroup.LeftArm), LeftLittleIntermediate("Auriculaire 2 G", group = BodyGroup.LeftArm), LeftLittleDistal("Auriculaire 3 G", group = BodyGroup.LeftArm),
    RightThumbProximal("Pouce 1 D", group = BodyGroup.RightArm), RightThumbIntermediate("Pouce 2 D", group = BodyGroup.RightArm), RightThumbDistal("Pouce 3 D", group = BodyGroup.RightArm),
    RightIndexProximal("Index 1 D", group = BodyGroup.RightArm), RightIndexIntermediate("Index 2 D", group = BodyGroup.RightArm), RightIndexDistal("Index 3 D", group = BodyGroup.RightArm),
    RightMiddleProximal("Majeur 1 D", group = BodyGroup.RightArm), RightMiddleIntermediate("Majeur 2 D", group = BodyGroup.RightArm), RightMiddleDistal("Majeur 3 D", group = BodyGroup.RightArm),
    RightRingProximal("Annulaire 1 D", group = BodyGroup.RightArm), RightRingIntermediate("Annulaire 2 D", group = BodyGroup.RightArm), RightRingDistal("Annulaire 3 D", group = BodyGroup.RightArm),
    RightLittleProximal("Auriculaire 1 D", group = BodyGroup.RightArm), RightLittleIntermediate("Auriculaire 2 D", group = BodyGroup.RightArm), RightLittleDistal("Auriculaire 3 D", group = BodyGroup.RightArm);

    val isFinger get() = ordinal >= LeftThumbProximal.ordinal
    val isLeft get() = name.startsWith("Left")

    companion object {
        val body = entries.filter { !it.isFinger }
        fun fingers(left: Boolean) = entries.filter { it.isFinger && it.isLeft == left }
    }
}

/** Pistes de la timeline. */
enum class BodyGroup(val label: String) {
    Head("Head"), Torso("Torso"), LeftArm("Left Arm"), RightArm("Right Arm"), LeftLeg("Left Leg"), RightLeg("Right Leg"), Root("Root Motion")
}

/** Correspondance os humanoïde -> index de nœud dans le squelette du personnage. */
data class BoneMap(val map: Map<HumanBone, Int>) {
    operator fun get(b: HumanBone) = map[b]
    fun missingRequired() = HumanBone.entries.filter { it.required && map[it] == null }
    val isUsable get() = missingRequired().isEmpty()
    val fingerCount get() = map.keys.count { it.isFinger }
}

/** Rapport de compatibilité affiché après import. */
data class RigReport(
    val nodeCount: Int,
    val jointCount: Int,
    val mapped: Int,
    val missing: List<HumanBone>,
    val fingers: Int,
    val morphCount: Int,
    val faceCompatible: Boolean,
    val isVrm: Boolean,
    val heightMeters: Double,
    val source: String,
) {
    val hasSkeleton get() = jointCount > 0
    val compatible get() = hasSkeleton && missing.isEmpty()
}

object HumanoidMapper {

    fun report(skel: Skeleton, map: BoneMap, isVrm: Boolean, source: String) = RigReport(
        nodeCount = skel.nodes.size,
        jointCount = skel.jointCount,
        mapped = map.map.size,
        missing = map.missingRequired(),
        fingers = map.fingerCount,
        morphCount = skel.morphTargets.sumOf { it.targetNames.size },
        faceCompatible = FaceMapping.hasFaceTargets(skel),
        isVrm = isVrm,
        heightMeters = skel.restHeight(),
        source = source,
    )

    /** Détection automatique : extension VRM si présente, sinon analyse des noms et de la hiérarchie. */
    fun autoMap(doc: GltfDocument?, skel: Skeleton): BoneMap {
        if (doc != null) vrmMap(doc, skel)?.let { if (it.isUsable) return it }
        return nameMap(skel)
    }

    private fun vrmMap(doc: GltfDocument, skel: Skeleton): BoneMap? {
        val ext = doc.json["extensions"] as? Map<*, *> ?: return null
        val out = HashMap<HumanBone, Int>()
        (ext["VRMC_vrm"] as? Map<*, *>)?.let { v1 ->
            val hb = ((v1["humanoid"] as? Map<*, *>)?.get("humanBones") as? Map<*, *>) ?: return@let
            for ((k, v) in hb) {
                val bone = fromVrmName(k.toString()) ?: continue
                @Suppress("UNCHECKED_CAST")
                val node = (v as? Map<String, Any?>)?.int("node") ?: continue
                if (node in skel.nodes.indices) out[bone] = node
            }
        }
        (ext["VRM"] as? Map<*, *>)?.let { v0 ->
            val list = ((v0["humanoid"] as? Map<*, *>)?.get("humanBones") as? List<*>) ?: return@let
            for (e in list) {
                val m = e.obj()
                val bone = fromVrmName(m.str("bone") ?: continue) ?: continue
                val node = m.int("node")
                if (node in skel.nodes.indices) out[bone] = node
            }
        }
        return if (out.isEmpty()) null else BoneMap(out)
    }

    private fun fromVrmName(n: String): HumanBone? {
        val norm = n.replace("Metacarpal", "Proximal")
        return HumanBone.entries.firstOrNull { it.name.equals(norm, ignoreCase = true) }
    }

    private data class Cand(val idx: Int, val core: String, val side: Int)

    /** Normalise un nom d'os : retire préfixes d'outils, détecte le côté (-1 droite, 1 gauche, 0 central). */
    private fun analyse(i: Int, raw: String): Cand {
        var s = raw.lowercase()
        s = s.substringAfterLast(':').substringAfterLast('|')
        listOf("mixamorig", "armature", "bip001", "bip01", "def-", "def_", "j_bip_", "j_", "cc_base_", "rig_", "ske_", "bone_")
            .forEach { if (s.startsWith(it)) s = s.removePrefix(it) }
        var side = 0
        val tokens = s.split('_', '.', ' ', '-').filter { it.isNotEmpty() }
        if (s.contains("left")) side = 1
        else if (s.contains("right")) side = -1
        else if (tokens.any { it == "l" || it == "lt" || it == "lft" }) side = 1
        else if (tokens.any { it == "r" || it == "rt" || it == "rgt" }) side = -1
        else if (tokens.isNotEmpty() && tokens.first().length > 1 && tokens.first()[0] == 'l' && Regex("^l(upperarm|forearm|hand|thigh|calf|foot|clavicle|shoulder|arm|leg|toe).*").matches(tokens.first())) side = 1
        else if (tokens.isNotEmpty() && tokens.first().length > 1 && tokens.first()[0] == 'r' && Regex("^r(upperarm|forearm|hand|thigh|calf|foot|clavicle|shoulder|arm|leg|toe).*").matches(tokens.first())) side = -1
        val core = tokens.filterNot { it in setOf("l", "r", "lt", "rt", "lft", "rgt", "c") }.joinToString("")
            .replace("left", "").replace("right", "")
        return Cand(i, core, side)
    }

    private fun nameMap(skel: Skeleton): BoneMap {
        val joints = skel.nodes.filter { it.isJoint }.ifEmpty { skel.nodes }
        val cands = joints.map { analyse(it.index, it.name) }
        val out = HashMap<HumanBone, Int>()

        fun isEnd(c: String) = c.endsWith("end") || c.endsWith("top") || c.endsWith("nub") || c.endsWith("tip")
        fun pick(side: Int, vararg patterns: Regex, exclude: Regex? = null): Int? =
            cands.firstOrNull { c -> c.side == side && !isEnd(c.core) && patterns.any { it.matches(c.core) } && (exclude == null || !exclude.containsMatchIn(c.core)) }?.idx

        out.putIfNotNull(HumanBone.Hips, pick(0, Regex("hips?"), Regex("pelvis"), Regex("root"), Regex("cog")))
        out.putIfNotNull(HumanBone.Neck, pick(0, Regex("neck\\d*"), Regex("neck01")))
        out.putIfNotNull(HumanBone.Head, pick(0, Regex("head"), Regex("head\\d")))
        out.putIfNotNull(HumanBone.Jaw, pick(0, Regex("jaw")))

        for (side in listOf(1, -1)) {
            val L = side == 1
            fun hb(l: HumanBone, r: HumanBone) = if (L) l else r
            out.putIfNotNull(hb(HumanBone.LeftShoulder, HumanBone.RightShoulder), pick(side, Regex("(l|r)?(shoulder|clavicle|collar)\\d*")))
            out.putIfNotNull(hb(HumanBone.LeftUpperArm, HumanBone.RightUpperArm), pick(side, Regex("(l|r)?(upperarm|uparm|arm)(01|1)?"), exclude = Regex("fore|lower|twist|roll")))
            out.putIfNotNull(hb(HumanBone.LeftLowerArm, HumanBone.RightLowerArm), pick(side, Regex("(l|r)?(forearm|lowerarm|elbow)(01|1)?"), exclude = Regex("twist|roll")))
            out.putIfNotNull(hb(HumanBone.LeftHand, HumanBone.RightHand), pick(side, Regex("(l|r)?(hand|wrist)")))
            out.putIfNotNull(hb(HumanBone.LeftUpperLeg, HumanBone.RightUpperLeg), pick(side, Regex("(l|r)?(upleg|thigh|upperleg|hip|femur)(01|1)?"), exclude = Regex("twist|roll")))
            out.putIfNotNull(hb(HumanBone.LeftLowerLeg, HumanBone.RightLowerLeg), pick(side, Regex("(l|r)?(leg|calf|shin|lowerleg|knee)(01|1)?"), exclude = Regex("up|twist|roll")))
            out.putIfNotNull(hb(HumanBone.LeftFoot, HumanBone.RightFoot), pick(side, Regex("(l|r)?(foot|ankle)")))
            out.putIfNotNull(hb(HumanBone.LeftToes, HumanBone.RightToes), pick(side, Regex("(l|r)?(toebase|toes?|ball)(01|1)?")))
            val fingers = listOf("thumb" to "Thumb", "index" to "Index", "middle" to "Middle", "ring" to "Ring", "(pinky|little)" to "Little")
            for ((pat, f) in fingers) {
                val chain = cands.filter { c -> c.side == side && !isEnd(c.core) && Regex(".*$pat.*").matches(c.core) }
                    .sortedBy { depth(skel, it.idx) }
                val segs = listOf("Proximal", "Intermediate", "Distal")
                // Mixamo : Thumb1..3 ; Unreal : thumb_01..03 (+ metacarpal) ; on garde les 3 derniers niveaux
                val trimmed = chain.toMutableList()
                while (trimmed.size > 3 && skel.nodes[trimmed.last().idx].children.isEmpty()) trimmed.removeAt(trimmed.size - 1)
                val use = if (trimmed.size > 3) trimmed.takeLast(3) else trimmed
                use.forEachIndexed { k, c ->
                    val name = (if (L) "Left" else "Right") + f + segs[k]
                    HumanBone.entries.firstOrNull { it.name == name }?.let { out[it] = c.idx }
                }
            }
        }
        // Colonne : os entre le bassin et le cou (ou la tête), par la hiérarchie.
        val hips = out[HumanBone.Hips]
        val top = out[HumanBone.Neck] ?: out[HumanBone.Head]
        if (hips != null && top != null) {
            val chain = ArrayList<Int>()
            var p = skel.nodes[top].parent
            while (p >= 0 && p != hips) { chain.add(0, p); p = skel.nodes[p].parent }
            if (p == hips && chain.isNotEmpty()) {
                out[HumanBone.Spine] = chain.first()
                if (chain.size >= 2) out[HumanBone.Chest] = chain[if (chain.size >= 3) chain.size - 2 else 1]
                if (chain.size >= 3) out[HumanBone.UpperChest] = chain.last()
            }
            if (out[HumanBone.Neck] == null && out[HumanBone.Head] != null && chain.isEmpty()) Unit
        }
        if (out[HumanBone.Spine] == null) out.putIfNotNull(HumanBone.Spine, pick(0, Regex("spine\\d*"), Regex("spine0?1"), Regex("torso")))
        return BoneMap(out)
    }

    private fun depth(skel: Skeleton, i: Int): Int { var d = 0; var p = skel.nodes[i].parent; while (p >= 0) { d++; p = skel.nodes[p].parent }; return d }

    private fun <K, V> HashMap<K, V>.putIfNotNull(k: K, v: V?) { if (v != null && !containsKey(k) && !containsValue(v)) put(k, v) }
}
