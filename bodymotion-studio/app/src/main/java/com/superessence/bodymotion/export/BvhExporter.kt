package com.superessence.bodymotion.export

import com.superessence.bodymotion.math.Quat
import com.superessence.bodymotion.math.Vec3
import com.superessence.bodymotion.retarget.PoseEvaluator
import com.superessence.bodymotion.retarget.SkeletalPose
import com.superessence.bodymotion.rig.Skeleton
import java.util.Locale

/**
 * Export BVH (Biovision). Le BVH impose des os sans rotation de repos : chaque articulation est exprimée
 * dans un repère aligné sur le monde (rotations = écarts par rapport à la pose de repos), ce qui reproduit
 * exactement le mouvement. Unités en centimètres. Les blendshapes ne sont pas représentables en BVH.
 */
object BvhExporter {

    fun export(skel: Skeleton, clip: PreparedClip, keepNames: Boolean = true): ByteArray {
        var top = clip.hipsNode
        while (skel.nodes[top].parent >= 0 && skel.nodes[skel.nodes[top].parent].isJoint) top = skel.nodes[top].parent
        val joints = ArrayList<Int>()
        fun collect(i: Int) { joints.add(i); skel.nodes[i].children.filter { skel.nodes[it].isJoint }.forEach { collect(it) } }
        collect(top)
        val jointSet = joints.toSet()
        val scale = if (skel.restHeight() < 10) 100.0 else 1.0
        val restRot = joints.associateWith { skel.restGlobal[it].decompose().r }
        val names = uniqueNames(skel, joints)

        val sb = StringBuilder()
        sb.append("HIERARCHY\n")
        fun f(v: Double) = String.format(Locale.US, "%.6f", v)
        fun writeJoint(i: Int, depth: Int) {
            val ind = "\t".repeat(depth)
            val p = skel.nodes[i].parent
            val off = if (i == top) Vec3.ZERO else (skel.restPos(i) - skel.restPos(p)) * scale
            sb.append(ind).append(if (i == top) "ROOT " else "JOINT ").append(names[i]).append('\n')
            sb.append(ind).append("{\n")
            sb.append(ind).append("\tOFFSET ${f(off.x)} ${f(off.y)} ${f(off.z)}\n")
            if (i == top) sb.append(ind).append("\tCHANNELS 6 Xposition Yposition Zposition Zrotation Xrotation Yrotation\n")
            else sb.append(ind).append("\tCHANNELS 3 Zrotation Xrotation Yrotation\n")
            val kids = skel.nodes[i].children.filter { it in jointSet }
            if (kids.isEmpty()) {
                val dir = if (p >= 0) (skel.restPos(i) - skel.restPos(p)).normalized() else Vec3.Y
                val len = if (p >= 0) (skel.restPos(i) - skel.restPos(p)).length() * scale * 0.5 else 5.0
                val e = dir * len.coerceAtLeast(1.0)
                sb.append(ind).append("\tEnd Site\n").append(ind).append("\t{\n")
                sb.append(ind).append("\t\tOFFSET ${f(e.x)} ${f(e.y)} ${f(e.z)}\n").append(ind).append("\t}\n")
            } else kids.forEach { writeJoint(it, depth + 1) }
            sb.append(ind).append("}\n")
        }
        writeJoint(top, 0)

        sb.append("MOTION\n")
        sb.append("Frames: ${clip.frames}\n")
        sb.append("Frame Time: ${String.format(Locale.US, "%.7f", 1.0 / clip.fps)}\n")
        val prevEuler = HashMap<Int, Vec3>()
        for (fr in 0 until clip.frames) {
            val local = Array(skel.nodes.size) { skel.nodes[it].rest.r }
            clip.nodes.forEachIndexed { k, n -> local[n] = clip.rotations[fr][k] }
            val pose = SkeletalPose(local, clip.hipsT[fr], clip.hipsNode)
            val g = PoseEvaluator.globals(skel, pose)
            if (clip.rootT != null) {
                // Pas d'os root en BVH : le déplacement du root est reporté sur la racine.
            }
            val delta = HashMap<Int, Quat>()
            val line = StringBuilder()
            for (j in joints) {
                val cur = g[j].decompose().r
                val d = (cur * restRot[j]!!.conjugate()).normalized()
                delta[j] = d
                val p = skel.nodes[j].parent
                val loc = if (j == top) d else (delta[p]!!.conjugate() * d).normalized()
                if (j == top) {
                    val pos = g[j].translation() * scale
                    line.append(f(pos.x)).append(' ').append(f(pos.y)).append(' ').append(f(pos.z)).append(' ')
                }
                var e = loc.toEulerZXYDegrees()
                prevEuler[j]?.let { pe -> e = Vec3(unwrap(e.x, pe.x), unwrap(e.y, pe.y), unwrap(e.z, pe.z)) }
                prevEuler[j] = e
                line.append(f(e.z)).append(' ').append(f(e.x)).append(' ').append(f(e.y)).append(' ')
            }
            sb.append(line.toString().trimEnd()).append('\n')
        }
        return sb.toString().toByteArray(Charsets.US_ASCII)
    }

    private fun unwrap(a: Double, prev: Double): Double { var x = a; while (x - prev > 180) x -= 360; while (x - prev < -180) x += 360; return x }

    fun uniqueNames(skel: Skeleton, joints: List<Int>): Map<Int, String> {
        val used = HashSet<String>()
        return joints.associateWith { i ->
            var base = skel.nodes[i].name.replace(Regex("[^A-Za-z0-9_:.\\-]"), "_").ifEmpty { "joint$i" }
            var n = base; var k = 1
            while (!used.add(n)) n = "${base}_${k++}"
            n
        }
    }
}
