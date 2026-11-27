package com.superessence.bodymotion.export

import com.superessence.bodymotion.anim.AnimationClip
import com.superessence.bodymotion.anim.ClipOps
import com.superessence.bodymotion.math.Quat
import com.superessence.bodymotion.math.Vec3
import com.superessence.bodymotion.rig.Skeleton
import kotlin.math.roundToInt

enum class ExportFormat(val ext: String, val label: String) { FBX("fbx", "FBX"), GLB("glb", "GLB"), GLTF("gltf", "glTF"), BVH("bvh", "BVH") }
enum class ExportContent(val label: String) { FULL("Personnage + animations"), ANIMATION_ONLY("Animations seules (squelette)") }
enum class ExportQuality(val label: String, val toleranceDeg: Double) { LOSSLESS("Sans perte", 0.0), OPTIMIZED("Optimisée", 0.15), COMPACT("Compacte", 0.6) }

enum class EngineProfile(val label: String, val defaultFormat: ExportFormat, val addRootBone: Boolean, val note: String) {
    UNITY("Unity", ExportFormat.FBX, false, "FBX binaire 7.4, Y-up. Dans Unity : Rig › Animation Type = Humanoid, puis « Root Transform » depuis le bassin."),
    UNREAL("Unreal", ExportFormat.FBX, true, "FBX binaire 7.4 avec un os « root » au sol portant le déplacement (root motion Unreal)."),
    GODOT("Godot", ExportFormat.GLB, true, "GLB avec un os « root » au sol. Dans Godot : AnimationTree › Root Motion Track = root."),
    GENERIC("Générique", ExportFormat.GLB, false, "Fichier standard, hiérarchie d'origine inchangée."),
}

data class ExportOptions(
    val format: ExportFormat = ExportFormat.GLB,
    val content: ExportContent = ExportContent.FULL,
    val profile: EngineProfile = EngineProfile.GENERIC,
    val rootMotion: Boolean = true,
    val fps: Int = 30,
    val quality: ExportQuality = ExportQuality.LOSSLESS,
    val keepBoneNames: Boolean = true,
    val includeFace: Boolean = true,
)

/** Données d'un clip prêtes à écrire : cadence finale, root motion appliqué, os « root » éventuel. */
class PreparedClip(
    val name: String,
    val fps: Double,
    val frames: Int,
    val nodes: IntArray,
    val rotations: Array<Array<Quat>>,
    val hipsNode: Int,
    val hipsT: Array<Vec3>,
    /** Translation locale de l'os « root » ajouté (null si pas d'os root). */
    val rootT: Array<Vec3>?,
    val source: AnimationClip,
) {
    /** Images conservées pour une piste de rotation (réduction de clés selon la qualité). */
    fun keyFramesFor(k: Int, tolDeg: Double): IntArray = reduce(frames, tolDeg) { a, b, f ->
        val u = (f - a).toDouble() / (b - a)
        (rotations[a][k].slerp(rotations[b][k], u).conjugate() * rotations[f][k]).normalized().angle()
    }

    fun keyFramesForVec(v: Array<Vec3>, tolDeg: Double): IntArray = reduce(frames, tolDeg) { a, b, f ->
        val u = (f - a).toDouble() / (b - a)
        (v[a].lerp(v[b], u) - v[f]).length() * 50 // ~1 cm ≈ 0.5 unité de tolérance
    }

    private fun reduce(n: Int, tolDeg: Double, err: (Int, Int, Int) -> Double): IntArray {
        if (tolDeg <= 0 || n <= 2) return IntArray(n) { it }
        val tol = Math.toRadians(tolDeg)
        val keep = java.util.TreeSet<Int>().apply { add(0); add(n - 1) }
        fun refine(a: Int, b: Int) {
            if (b - a < 2) return
            var worst = -1; var we = tol
            for (f in a + 1 until b) { val e = err(a, b, f); if (e > we) { we = e; worst = f } }
            if (worst >= 0) { keep.add(worst); refine(a, worst); refine(worst, b) }
        }
        refine(0, n - 1)
        return keep.toIntArray()
    }
}

object ExportPrep {
    /** Index du parent du bassin (le nouvel os « root » est inséré entre les deux). */
    fun prepare(clip: AnimationClip, skel: Skeleton, opt: ExportOptions): PreparedClip {
        val c = if (clip.fps.roundToInt() != opt.fps) ClipOps.resample(clip, opt.fps.toDouble()) else clip
        val hips = c.hipsNode
        val parent = skel.nodes[hips].parent
        val pm = if (parent >= 0) skel.restGlobal[parent] else com.superessence.bodymotion.math.Mat4()
        val pmInv = pm.inverse()
        val world = c.hipsT.map { pm.transformPoint(it) }
        val start = world.firstOrNull() ?: Vec3.ZERO
        val fixedWorld = if (opt.rootMotion) world else world.map { Vec3(start.x, it.y, start.z) }
        val addRoot = opt.profile.addRootBone && opt.format != ExportFormat.BVH
        var rootT: Array<Vec3>? = null
        val hipsLocal: Array<Vec3>
        if (addRoot) {
            val originY = pm.translation().y
            val rootWorld = fixedWorld.map { Vec3(it.x, originY, it.z) }
            rootT = Array(rootWorld.size) { pmInv.transformPoint(rootWorld[it]) }
            hipsLocal = Array(fixedWorld.size) { pmInv.transformPoint(fixedWorld[it]) - rootT[it] }
        } else {
            hipsLocal = Array(fixedWorld.size) { pmInv.transformPoint(fixedWorld[it]) }
        }
        return PreparedClip(c.name, c.fps, c.frameCount, c.nodes, c.rotations, hips, hipsLocal, rootT, c)
    }

    fun safeFileName(s: String) = s.replace(Regex("[^A-Za-z0-9_\\-]+"), "_").trim('_').ifEmpty { "animation" }
}
