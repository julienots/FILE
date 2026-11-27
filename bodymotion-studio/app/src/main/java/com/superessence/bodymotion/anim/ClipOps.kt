package com.superessence.bodymotion.anim

import com.superessence.bodymotion.math.Quat
import com.superessence.bodymotion.math.Vec3
import com.superessence.bodymotion.rig.BodyGroup
import com.superessence.bodymotion.rig.BoneMap
import java.util.TreeSet
import java.util.UUID
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

/** Opérations d'édition réelles sur les données du clip (toutes testées). */
object ClipOps {

    /** Colonnes (index k dans clip.nodes) appartenant à une piste. */
    fun columns(clip: AnimationClip, map: BoneMap, group: BodyGroup): List<Int> {
        if (group == BodyGroup.Root) return emptyList()
        val nodes = map.map.filterKeys { it.group == group }.values.toSet()
        return clip.nodes.indices.filter { clip.nodes[it] in nodes }
    }

    private fun angleBetween(a: Quat, b: Quat) = (a.conjugate() * b).normalized().angle()

    /**
     * Réduction de clés : on garde les images où l'interpolation linéaire entre clés voisines
     * s'écarterait de plus de [toleranceDeg] degrés (ou 1 cm pour le root).
     */
    fun rebuildKeys(clip: AnimationClip, map: BoneMap, toleranceDeg: Double) {
        val tol = Math.toRadians(toleranceDeg)
        for (g in BodyGroup.entries) {
            val cols = columns(clip, map, g)
            val keys = TreeSet<Int>()
            val n = clip.frameCount
            if (n == 0) { clip.keys[g] = keys; continue }
            keys.add(0); keys.add(n - 1)
            fun error(a: Int, b: Int, f: Int): Double {
                val u = (f - a).toDouble() / (b - a)
                return if (g == BodyGroup.Root) (clip.hipsT[a].lerp(clip.hipsT[b], u) - clip.hipsT[f]).length() / 0.01 * tol
                else cols.maxOfOrNull { k -> angleBetween(clip.rotations[a][k].slerp(clip.rotations[b][k], u), clip.rotations[f][k]) } ?: 0.0
            }
            fun refine(a: Int, b: Int) {
                if (b - a < 2) return
                var worst = -1; var we = tol
                for (f in a + 1 until b) { val e = error(a, b, f); if (e > we) { we = e; worst = f } }
                if (worst >= 0) { keys.add(worst); refine(a, worst); refine(worst, b) }
            }
            refine(0, n - 1)
            clip.keys[g] = keys
        }
    }

    fun addKey(clip: AnimationClip, group: BodyGroup, frame: Int) {
        clip.keys.getOrPut(group) { TreeSet() }.add(frame.coerceIn(0, clip.frameCount - 1))
    }

    /** Supprime une clé : la piste est ré-interpolée entre les clés voisines. */
    fun deleteKey(clip: AnimationClip, map: BoneMap, group: BodyGroup, frame: Int): Boolean {
        val keys = clip.keys[group] ?: return false
        if (!keys.contains(frame)) return false
        val prev = keys.lower(frame); val next = keys.higher(frame)
        keys.remove(frame)
        if (prev == null || next == null) return true
        interpolateSpan(clip, map, group, prev, next)
        return true
    }

    /** Supprime toutes les clés d'une plage et ré-interpole. */
    fun deleteKeysInRange(clip: AnimationClip, map: BoneMap, group: BodyGroup, from: Int, to: Int) {
        val keys = clip.keys[group] ?: return
        keys.subSet(from, true, to, true).toList().forEach { keys.remove(it) }
        val prev = keys.lower(from) ?: return
        val next = keys.higher(to) ?: return
        interpolateSpan(clip, map, group, prev, next)
    }

    private fun interpolateSpan(clip: AnimationClip, map: BoneMap, group: BodyGroup, a: Int, b: Int) {
        val cols = columns(clip, map, group)
        for (f in a + 1 until b) {
            val u = (f - a).toDouble() / (b - a)
            if (group == BodyGroup.Root) clip.hipsT[f] = clip.hipsT[a].lerp(clip.hipsT[b], u)
            else for (k in cols) clip.rotations[f][k] = clip.rotations[a][k].slerp(clip.rotations[b][k], u)
        }
    }

    /**
     * Rotation manuelle d'un os à une image, propagée avec un fondu en cosinus sur ± [falloff] images.
     * Une clé est ajoutée sur la piste.
     */
    fun rotateBone(clip: AnimationClip, map: BoneMap, node: Int, frame: Int, delta: Quat, falloff: Int) {
        val k = clip.nodes.indexOf(node); if (k < 0) return
        for (f in max(0, frame - falloff)..min(clip.frameCount - 1, frame + falloff)) {
            val w = if (falloff == 0) 1.0 else 0.5 * (1 + cos(PI * (f - frame).toDouble() / (falloff + 1)))
            clip.rotations[f][k] = (clip.rotations[f][k] * Quat.IDENTITY.slerp(delta, w)).normalized()
        }
        val group = map.map.entries.firstOrNull { it.value == node }?.key?.group ?: return
        addKey(clip, group, frame)
    }

    fun translateRoot(clip: AnimationClip, frame: Int, delta: Vec3, falloff: Int) {
        for (f in max(0, frame - falloff)..min(clip.frameCount - 1, frame + falloff)) {
            val w = if (falloff == 0) 1.0 else 0.5 * (1 + cos(PI * (f - frame).toDouble() / (falloff + 1)))
            clip.hipsT[f] = clip.hipsT[f] + delta * w
        }
        addKey(clip, BodyGroup.Root, frame)
    }

    /** Lissage gaussien d'une piste (ou de tout le corps si group == null) sur une plage. */
    fun smooth(clip: AnimationClip, map: BoneMap, group: BodyGroup?, from: Int, to: Int, sigmaFrames: Double) {
        val radius = (sigmaFrames * 2.5).roundToInt().coerceAtLeast(1)
        val weights = DoubleArray(2 * radius + 1) { val x = (it - radius) / sigmaFrames; exp(-0.5 * x * x) }
        val groups = group?.let { listOf(it) } ?: BodyGroup.entries
        val a = from.coerceIn(0, clip.frameCount - 1); val b = to.coerceIn(0, clip.frameCount - 1)
        for (g in groups) {
            if (g == BodyGroup.Root) {
                val src = clip.hipsT.copyOf()
                for (f in a..b) {
                    var acc = Vec3.ZERO; var ws = 0.0
                    for (j in -radius..radius) { val i = (f + j).coerceIn(0, clip.frameCount - 1); acc += src[i] * weights[j + radius]; ws += weights[j + radius] }
                    clip.hipsT[f] = acc / ws
                }
            } else for (k in columns(clip, map, g)) {
                val src = Array(clip.frameCount) { clip.rotations[it][k] }
                for (f in a..b) {
                    // Moyenne de quaternions (même hémisphère que l'image courante), puis normalisation.
                    var x = 0.0; var y = 0.0; var z = 0.0; var w = 0.0
                    val ref = src[f]
                    for (j in -radius..radius) {
                        var q = src[(f + j).coerceIn(0, clip.frameCount - 1)]
                        if (q.dot(ref) < 0) q = q.negate()
                        val wt = weights[j + radius]
                        x += q.x * wt; y += q.y * wt; z += q.z * wt; w += q.w * wt
                    }
                    clip.rotations[f][k] = Quat(x, y, z, w).normalized()
                }
            }
        }
    }

    /** Extrait la plage [from, to] dans un nouveau clip. */
    fun trim(clip: AnimationClip, from: Int, to: Int, newName: String = clip.name): AnimationClip {
        val a = from.coerceIn(0, clip.frameCount - 1); val b = to.coerceIn(a, clip.frameCount - 1)
        val c = clip.deepCopy(UUID.randomUUID().toString())
        c.name = newName
        c.rotations = Array(b - a + 1) { clip.rotations[a + it].copyOf() }
        c.hipsT = Array(b - a + 1) { clip.hipsT[a + it] }
        if (clip.morph.isNotEmpty()) c.morph = Array(b - a + 1) { clip.morph[a + it].copyOf() }
        c.keys = clip.keys.mapValues { (_, s) -> TreeSet(s.subSet(a, true, b, true).map { it - a }).apply { add(0); add(b - a) } }.toMutableMap()
        c.interpolatedFrames = TreeSet(clip.interpolatedFrames.subSet(a, true, b, true).map { it - a })
        return c
    }

    fun split(clip: AnimationClip, at: Int): Pair<AnimationClip, AnimationClip> =
        trim(clip, 0, at, clip.name + " A") to trim(clip, at, clip.frameCount - 1, clip.name + " B")

    /**
     * Enchaîne deux clips du même personnage avec un fondu de [blend] images.
     * Le root motion du second clip est raccordé à la position finale du premier.
     */
    fun merge(a: AnimationClip, b: AnimationClip, blend: Int, name: String): AnimationClip {
        require(a.characterId == b.characterId && a.nodes.contentEquals(b.nodes)) { "Les deux animations doivent utiliser le même personnage." }
        val bb = if (b.fps != a.fps) resample(b, a.fps) else b
        val bl = blend.coerceIn(0, minOf(a.frameCount, bb.frameCount) - 1)
        val offset = a.hipsT.last() - bb.hipsT.first()
        val flatOffset = Vec3(offset.x, 0.0, offset.z)
        val total = a.frameCount + bb.frameCount - bl
        val rots = Array(total) { f ->
            when {
                f < a.frameCount - bl -> a.rotations[f].copyOf()
                f < a.frameCount -> {
                    val j = f - (a.frameCount - bl); val u = (j + 1).toDouble() / (bl + 1)
                    Array(a.nodes.size) { k -> a.rotations[f][k].slerp(bb.rotations[j][k], u) }
                }
                else -> bb.rotations[f - a.frameCount + bl].copyOf()
            }
        }
        val hips = Array(total) { f ->
            when {
                f < a.frameCount - bl -> a.hipsT[f]
                f < a.frameCount -> { val j = f - (a.frameCount - bl); val u = (j + 1).toDouble() / (bl + 1); a.hipsT[f].lerp(bb.hipsT[j] + flatOffset, u) }
                else -> bb.hipsT[f - a.frameCount + bl] + flatOffset
            }
        }
        val c = AnimationClip(UUID.randomUUID().toString(), name, a.category, a.characterId, a.fps, a.nodes.copyOf(), a.hipsNode, rots, hips, a.morphChannels)
        if (a.morph.isNotEmpty() && bb.morph.isNotEmpty() && a.morphChannels == bb.morphChannels) {
            c.morph = Array(total) { f -> if (f < a.frameCount) a.morph[f].copyOf() else bb.morph[f - a.frameCount + bl].copyOf() }
        }
        return c
    }

    /** Rééchantillonne à une autre cadence (slerp). */
    fun resample(clip: AnimationClip, fps: Double): AnimationClip {
        val n = max(1, (clip.duration * fps).roundToInt() + 1)
        val c = clip.deepCopy(clip.id)
        c.fps = fps
        c.rotations = Array(n) { f -> val t = f / fps; val x = (t * clip.fps).coerceIn(0.0, (clip.frameCount - 1).toDouble()); val a = x.toInt(); val b = min(a + 1, clip.frameCount - 1); val u = x - a
            Array(clip.nodes.size) { k -> clip.rotations[a][k].slerp(clip.rotations[b][k], u) } }
        c.hipsT = Array(n) { f -> val x = (f / fps * clip.fps).coerceIn(0.0, (clip.frameCount - 1).toDouble()); val a = x.toInt(); val b = min(a + 1, clip.frameCount - 1); clip.hipsT[a].lerp(clip.hipsT[b], x - a) }
        if (clip.morph.isNotEmpty()) c.morph = Array(n) { f -> val x = (f / fps * clip.fps).coerceIn(0.0, (clip.frameCount - 1).toDouble()); val a = x.toInt(); val b = min(a + 1, clip.frameCount - 1); val u = (x - a).toFloat()
            FloatArray(clip.morphChannels.size) { i -> clip.morph[a][i] * (1 - u) + clip.morph[b][i] * u } }
        val ratio = fps / clip.fps
        c.keys = clip.keys.mapValues { (_, s) -> TreeSet(s.map { (it * ratio).roundToInt().coerceIn(0, n - 1) }) }.toMutableMap()
        c.interpolatedFrames = TreeSet(clip.interpolatedFrames.map { (it * ratio).roundToInt().coerceIn(0, n - 1) })
        return c
    }

    /** Change la vitesse (0.5 = deux fois plus lent) en conservant la cadence. */
    fun changeSpeed(clip: AnimationClip, speed: Double): AnimationClip {
        val tmp = clip.deepCopy(clip.id); tmp.fps = clip.fps * speed
        return resample(tmp, clip.fps)
    }

    /** Rend l'animation bouclable : fondu de la fin vers le début (rotations et hauteur du bassin). */
    fun makeLoop(clip: AnimationClip, blend: Int) {
        val n = clip.frameCount
        val bl = blend.coerceIn(1, n / 2)
        for (j in 0 until bl) {
            val f = n - bl + j
            val u = (j + 1).toDouble() / bl
            for (k in clip.nodes.indices) clip.rotations[f][k] = clip.rotations[f][k].slerp(clip.rotations[0][k], u)
            val h = clip.hipsT[f]; val start = clip.hipsT[0]
            clip.hipsT[f] = Vec3(h.x, h.y + (start.y - h.y) * u, h.z)
        }
        clip.loop = true
    }

    /** Supprime le déplacement horizontal (le personnage reste sur place). */
    fun removeRootMotion(clip: AnimationClip) {
        val s = clip.hipsT.firstOrNull() ?: return
        clip.hipsT = Array(clip.frameCount) { Vec3(s.x, clip.hipsT[it].y, s.z) }
    }
}
