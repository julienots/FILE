package com.superessence.bodymotion.mocap

import com.superessence.bodymotion.anim.AnimationClip
import com.superessence.bodymotion.anim.ClipOps
import com.superessence.bodymotion.anim.MorphChannel
import com.superessence.bodymotion.retarget.Calibration
import com.superessence.bodymotion.retarget.RetargetOptions
import com.superessence.bodymotion.retarget.Retargeter
import com.superessence.bodymotion.retarget.SkeletalPose
import com.superessence.bodymotion.rig.FaceRig
import java.util.TreeSet
import java.util.UUID
import kotlin.math.roundToInt

/**
 * Transforme une capture brute (images horodatées, cadence irrégulière, pertes de suivi) en clip propre :
 * retargeting, rééchantillonnage à cadence fixe, interpolation des trous courts, maintien sur les trous longs.
 */
object ClipBuilder {
    const val MIN_QUALITY = 0.35f
    const val MAX_GAP_S = 1.5

    class Report(val clip: AnimationClip, val lostFrames: Int, val totalFrames: Int)

    fun build(
        frames: List<PoseFrame>, rt: Retargeter, calib: Calibration, opt: RetargetOptions, face: FaceRig?,
        fps: Double, name: String, characterId: String,
    ): Report? {
        val good = ArrayList<Pair<Double, SkeletalPose>>()
        val faceSamples = ArrayList<Pair<Double, FloatArray>>()
        val t0 = frames.firstOrNull()?.timeMs ?: return null
        for (f in frames) {
            val t = (f.timeMs - t0) / 1000.0
            if (f.hasBody && f.trackingQuality() >= MIN_QUALITY) rt.solve(f, calib, opt)?.let { good.add(t to it) }
            if (face != null && !face.isEmpty) f.faceBlendshapes?.let { faceSamples.add(t to face.evaluate(it)) }
        }
        if (good.size < 2) return null
        val duration = (frames.last().timeMs - t0) / 1000.0
        val n = (duration * fps).roundToInt() + 1
        val poses = ArrayList<SkeletalPose>(n)
        val interp = TreeSet<Int>()
        var j = 0
        for (k in 0 until n) {
            val t = k / fps
            while (j < good.size - 2 && good[j + 1].first < t) j++
            val (ta, pa) = good[j]; val (tb, pb) = good[minOf(j + 1, good.size - 1)]
            when {
                t <= good.first().first -> { poses.add(good.first().second.copy()); if (good.first().first - t > 0.1) interp.add(k) }
                t >= good.last().first -> { poses.add(good.last().second.copy()); if (t - good.last().first > 0.1) interp.add(k) }
                else -> {
                    val gap = tb - ta
                    if (gap > MAX_GAP_S) { poses.add(pa.copy()); interp.add(k) } // perte longue : on maintient la dernière pose fiable
                    else {
                        val u = ((t - ta) / gap).coerceIn(0.0, 1.0)
                        poses.add(SkeletalPose(Array(pa.local.size) { pa.local[it].slerp(pb.local[it], u) }, pa.hipsLocalT.lerp(pb.hipsLocalT, u), pa.hipsNode))
                        if (gap > 2.5 / fps) interp.add(k)
                    }
                }
            }
        }
        var channels = emptyList<MorphChannel>(); var morph = emptyList<FloatArray>()
        if (face != null && !face.isEmpty && faceSamples.size >= 2) {
            channels = face.channels
            var m = 0
            morph = (0 until n).map { k ->
                val t = k / fps
                while (m < faceSamples.size - 2 && faceSamples[m + 1].first < t) m++
                val (ta, a) = faceSamples[m]; val (tb, b) = faceSamples[minOf(m + 1, faceSamples.size - 1)]
                val u = if (tb > ta) ((t - ta) / (tb - ta)).coerceIn(0.0, 1.0).toFloat() else 0f
                FloatArray(a.size) { a[it] * (1 - u) + b[it] * u }
            }
        }
        val clip = AnimationClip.fromPoses(UUID.randomUUID().toString(), name, characterId, fps, rt.skel, rt.map, poses, channels, morph)
        clip.interpolatedFrames = interp
        // Nettoyage léger supplémentaire : supprime les micro-tremblements résiduels.
        ClipOps.smooth(clip, rt.map, null, 0, clip.frameCount - 1, 0.8)
        ClipOps.rebuildKeys(clip, rt.map, 0.6)
        return Report(clip, interp.size, n)
    }

}
