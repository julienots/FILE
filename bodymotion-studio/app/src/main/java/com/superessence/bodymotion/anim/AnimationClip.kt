package com.superessence.bodymotion.anim

import com.superessence.bodymotion.math.Quat
import com.superessence.bodymotion.math.Vec3
import com.superessence.bodymotion.retarget.SkeletalPose
import com.superessence.bodymotion.rig.BodyGroup
import com.superessence.bodymotion.rig.BoneMap
import com.superessence.bodymotion.rig.HumanBone
import com.superessence.bodymotion.rig.Skeleton
import java.io.DataInputStream
import java.io.DataOutputStream
import java.io.InputStream
import java.io.OutputStream
import java.util.TreeSet

/** Canal de blendshape animé : nœud de maillage + index de cible. */
data class MorphChannel(val node: Int, val target: Int, val name: String)

/**
 * Animation échantillonnée à cadence fixe, exprimée dans le squelette d'un personnage précis.
 * - rotations[f][k] : rotation locale du nœud nodes[k] à l'image f
 * - hipsT[f] : translation locale du bassin (root motion)
 * - morph[f][c] : poids des blendshapes du visage
 * Les images clés (keys) par piste servent à l'édition : supprimer une clé ré-interpole la piste.
 */
class AnimationClip(
    var id: String,
    var name: String,
    var category: String,
    var characterId: String,
    var fps: Double,
    val nodes: IntArray,
    val hipsNode: Int,
    var rotations: Array<Array<Quat>>,
    var hipsT: Array<Vec3>,
    var morphChannels: List<MorphChannel> = emptyList(),
    var morph: Array<FloatArray> = emptyArray(),
    var keys: MutableMap<BodyGroup, TreeSet<Int>> = mutableMapOf(),
    var loop: Boolean = false,
    var createdAt: Long = System.currentTimeMillis(),
    /** Images où le suivi a été perdu puis reconstruit par interpolation. */
    var interpolatedFrames: TreeSet<Int> = TreeSet(),
) {
    val frameCount get() = rotations.size
    val duration get() = if (frameCount <= 1) 0.0 else (frameCount - 1) / fps

    fun poseAt(frame: Int, skel: Skeleton): SkeletalPose {
        val f = frame.coerceIn(0, frameCount - 1)
        val local = Array(skel.nodes.size) { skel.nodes[it].rest.r }
        nodes.forEachIndexed { k, n -> local[n] = rotations[f][k] }
        return SkeletalPose(local, hipsT[f], hipsNode)
    }

    /** Pose interpolée à un temps continu (secondes). */
    fun poseAtTime(t: Double, skel: Skeleton): SkeletalPose {
        if (frameCount == 0) return SkeletalPose(Array(skel.nodes.size) { skel.nodes[it].rest.r }, skel.nodes[hipsNode].rest.t, hipsNode)
        val x = (t * fps).coerceIn(0.0, (frameCount - 1).toDouble())
        val a = x.toInt(); val b = minOf(a + 1, frameCount - 1); val u = x - a
        val local = Array(skel.nodes.size) { skel.nodes[it].rest.r }
        nodes.forEachIndexed { k, n -> local[n] = rotations[a][k].slerp(rotations[b][k], u) }
        return SkeletalPose(local, hipsT[a].lerp(hipsT[b], u), hipsNode)
    }

    fun morphAt(frame: Int): FloatArray? = if (morph.isEmpty()) null else morph[frame.coerceIn(0, morph.size - 1)]

    fun deepCopy(newId: String = id) = AnimationClip(
        newId, name, category, characterId, fps, nodes.copyOf(), hipsNode,
        Array(frameCount) { rotations[it].copyOf() }, hipsT.copyOf(), morphChannels,
        Array(morph.size) { morph[it].copyOf() },
        keys.mapValues { TreeSet(it.value) }.toMutableMap(), loop, createdAt, TreeSet(interpolatedFrames),
    )

    // ---------- Sérialisation binaire ----------

    fun write(out: OutputStream) {
        val d = DataOutputStream(out.buffered())
        d.writeInt(MAGIC); d.writeInt(2)
        d.writeUTF(id); d.writeUTF(name); d.writeUTF(category); d.writeUTF(characterId)
        d.writeDouble(fps); d.writeBoolean(loop); d.writeLong(createdAt)
        d.writeInt(hipsNode)
        d.writeInt(nodes.size); nodes.forEach { d.writeInt(it) }
        d.writeInt(frameCount)
        for (f in 0 until frameCount) {
            for (q in rotations[f]) { d.writeFloat(q.x.toFloat()); d.writeFloat(q.y.toFloat()); d.writeFloat(q.z.toFloat()); d.writeFloat(q.w.toFloat()) }
            val t = hipsT[f]; d.writeFloat(t.x.toFloat()); d.writeFloat(t.y.toFloat()); d.writeFloat(t.z.toFloat())
        }
        d.writeInt(morphChannels.size)
        morphChannels.forEach { d.writeInt(it.node); d.writeInt(it.target); d.writeUTF(it.name) }
        d.writeInt(morph.size)
        morph.forEach { row -> row.forEach { d.writeFloat(it) } }
        d.writeInt(keys.size)
        keys.forEach { (g, s) -> d.writeUTF(g.name); d.writeInt(s.size); s.forEach { d.writeInt(it) } }
        d.writeInt(interpolatedFrames.size); interpolatedFrames.forEach { d.writeInt(it) }
        d.flush()
    }

    companion object {
        private const val MAGIC = 0x424D4143 // "BMAC"

        fun read(input: InputStream): AnimationClip {
            val d = DataInputStream(input.buffered())
            require(d.readInt() == MAGIC) { "Fichier d'animation invalide" }
            d.readInt()
            val id = d.readUTF(); val name = d.readUTF(); val cat = d.readUTF(); val cid = d.readUTF()
            val fps = d.readDouble(); val loop = d.readBoolean(); val created = d.readLong()
            val hips = d.readInt()
            val nodes = IntArray(d.readInt()) { d.readInt() }
            val fc = d.readInt()
            val rots = Array(fc) { Array(nodes.size) { Quat.IDENTITY } }
            val hipsT = Array(fc) { Vec3.ZERO }
            for (f in 0 until fc) {
                for (k in nodes.indices) rots[f][k] = Quat(d.readFloat().toDouble(), d.readFloat().toDouble(), d.readFloat().toDouble(), d.readFloat().toDouble())
                hipsT[f] = Vec3(d.readFloat().toDouble(), d.readFloat().toDouble(), d.readFloat().toDouble())
            }
            val mc = List(d.readInt()) { MorphChannel(d.readInt(), d.readInt(), d.readUTF()) }
            val mrows = d.readInt()
            val morph = Array(mrows) { FloatArray(mc.size) { d.readFloat() } }
            val keys = mutableMapOf<BodyGroup, TreeSet<Int>>()
            repeat(d.readInt()) {
                val g = BodyGroup.valueOf(d.readUTF())
                keys[g] = TreeSet<Int>().apply { repeat(d.readInt()) { add(d.readInt()) } }
            }
            val interp = TreeSet<Int>().apply { repeat(d.readInt()) { add(d.readInt()) } }
            return AnimationClip(id, name, cat, cid, fps, nodes, hips, rots, hipsT, mc, morph, keys, loop, created, interp)
        }

        /** Construit un clip à partir de poses déjà échantillonnées à cadence fixe. */
        fun fromPoses(
            id: String, name: String, characterId: String, fps: Double, skel: Skeleton, map: BoneMap,
            poses: List<SkeletalPose>, morphChannels: List<MorphChannel> = emptyList(), morph: List<FloatArray> = emptyList(),
        ): AnimationClip {
            val nodes = map.map.values.distinct().sorted().toIntArray()
            val hips = map[HumanBone.Hips]!!
            val rots = Array(poses.size) { f -> Array(nodes.size) { k -> poses[f].local[nodes[k]] } }
            val clip = AnimationClip(id, name, "Capture", characterId, fps, nodes, hips, rots, Array(poses.size) { poses[it].hipsLocalT },
                morphChannels, morph.toTypedArray())
            ClipOps.rebuildKeys(clip, map, 0.6)
            return clip
        }
    }
}

/** Catégories de la bibliothèque. */
val CLIP_CATEGORIES = listOf("Capture", "Idle", "Walk", "Run", "Sprint", "Jump", "Fall", "Land", "Punch", "Kick", "Dodge", "Attack", "Death", "Victory", "Dance", "Autre")
