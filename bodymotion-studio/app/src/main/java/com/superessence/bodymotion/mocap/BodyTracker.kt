package com.superessence.bodymotion.mocap

import android.content.Context
import android.graphics.Bitmap
import android.util.Log
import com.google.mediapipe.framework.image.BitmapImageBuilder
import com.google.mediapipe.tasks.core.BaseOptions
import com.google.mediapipe.tasks.core.Delegate
import com.google.mediapipe.tasks.vision.core.RunningMode
import com.google.mediapipe.tasks.vision.facelandmarker.FaceLandmarker
import com.google.mediapipe.tasks.vision.handlandmarker.HandLandmarker
import com.google.mediapipe.tasks.vision.poselandmarker.PoseLandmarker
import com.superessence.bodymotion.data.CrashGuard
import com.superessence.bodymotion.math.Vec3

/**
 * Détection locale (sur l'appareil, sans réseau) avec MediaPipe :
 * corps (33 points 2D + 3D), mains (21 points), visage (52 expressions).
 * Mode « vidéo » synchrone : une image analysée => un [PoseFrame] cohérent.
 */
class BodyTracker(ctx: Context, var hands: Boolean, var face: Boolean, smoothing: Int, forceCpu: Boolean = true) : AutoCloseable {
    private var pose: PoseLandmarker
    private var hand: HandLandmarker? = null
    private var faceLm: FaceLandmarker? = null
    var delegateName: String
        private set
    private val app = ctx.applicationContext
    private var lastTs = -1L

    private val worldFilter: PointSetFilter
    private val imgFilter: PointSetFilter
    private val handFilterL: PointSetFilter
    private val handFilterR: PointSetFilter

    init {
        val (mc, beta) = SmoothingPresets.params(smoothing)
        worldFilter = PointSetFilter(PL.COUNT, mc, beta)
        imgFilter = PointSetFilter(PL.COUNT, mc * 1.5, beta * 2)
        handFilterL = PointSetFilter(21, mc * 1.5, beta); handFilterR = PointSetFilter(21, mc * 1.5, beta)
        var d = if (forceCpu) "CPU" else "GPU"
        CrashGuard.step("Détecteur du corps : création ($d)")
        pose = if (forceCpu) createPose(Delegate.CPU) else try { createPose(Delegate.GPU) } catch (e: Throwable) { Log.w("BodyTracker", "GPU indisponible, CPU", e); d = "CPU"; createPose(Delegate.CPU) }
        delegateName = d
    }

    private fun createPose(delegate: Delegate) = PoseLandmarker.createFromOptions(app, PoseLandmarker.PoseLandmarkerOptions.builder()
        .setBaseOptions(BaseOptions.builder().setModelAssetPath("models/pose_landmarker_full.task").setDelegate(delegate).build())
        .setRunningMode(RunningMode.VIDEO).setNumPoses(1)
        .setMinPoseDetectionConfidence(0.5f).setMinPosePresenceConfidence(0.5f).setMinTrackingConfidence(0.5f)
        .build())

    private fun handLm(): HandLandmarker = hand ?: HandLandmarker.createFromOptions(app, HandLandmarker.HandLandmarkerOptions.builder()
        .setBaseOptions(BaseOptions.builder().setModelAssetPath("models/hand_landmarker.task").setDelegate(if (delegateName == "GPU") Delegate.GPU else Delegate.CPU).build())
        .setRunningMode(RunningMode.VIDEO).setNumHands(2).setMinHandDetectionConfidence(0.5f).setMinTrackingConfidence(0.5f).build()).also { hand = it }

    private fun faceLandmarker(): FaceLandmarker = faceLm ?: FaceLandmarker.createFromOptions(app, FaceLandmarker.FaceLandmarkerOptions.builder()
        .setBaseOptions(BaseOptions.builder().setModelAssetPath("models/face_landmarker.task").setDelegate(Delegate.CPU).build())
        .setRunningMode(RunningMode.VIDEO).setNumFaces(1).setOutputFaceBlendshapes(true).build()).also { faceLm = it }

    /** Analyse une image droite (déjà tournée). [timeMs] doit être croissant. */
    fun process(bmp: Bitmap, timeMs: Long): PoseFrame {
        val ts = if (timeMs <= lastTs) lastTs + 1 else timeMs
        lastTs = ts
        val img = BitmapImageBuilder(bmp).build()
        val aspect = bmp.width.toFloat() / bmp.height
        CrashGuard.step("Analyse : corps${if (hands) " + mains" else ""}${if (face) " + visage" else ""} ($delegateName)")
        val r = try { pose.detectForVideo(img, ts) } catch (e: Throwable) {
            if (delegateName != "GPU") throw e
            // Repli automatique : certains GPU refusent le délégué au moment de l'inférence.
            Log.w("BodyTracker", "GPU en échec, passage au CPU", e)
            runCatching { pose.close() }; pose = createPose(Delegate.CPU); delegateName = "CPU"
            runCatching { hand?.close() }; hand = null
            pose.detectForVideo(img, ts)
        }
        if (r.landmarks().isEmpty() || r.worldLandmarks().isEmpty()) {
            worldFilter.reset(); imgFilter.reset()
            return PoseFrame(ts, null, null, aspect)
        }
        val t = ts / 1000.0
        val lm = r.landmarks()[0]; val wl = r.worldLandmarks()[0]
        // Repère : X = gauche du sujet (droite de l'image), Y haut, Z vers la caméra.
        val world = worldFilter.apply(Array(PL.COUNT) { Vec3(wl[it].x().toDouble(), -wl[it].y().toDouble(), -wl[it].z().toDouble()) }, t)
        val rawImg = Array(PL.COUNT) { Vec3(lm[it].x().toDouble(), lm[it].y().toDouble(), 0.0) }
        val fi = imgFilter.apply(rawImg, t)
        val image = Array(PL.COUNT) { ImagePoint(fi[it].x.toFloat(), fi[it].y.toFloat(), lm[it].visibility().orElse(0f)) }

        var left: Array<Vec3>? = null; var right: Array<Vec3>? = null
        var leftImg: Array<ImagePoint>? = null; var rightImg: Array<ImagePoint>? = null
        if (hands) runCatching {
            val hr = handLm().detectForVideo(img, ts)
            for (h in hr.landmarks().indices) {
                val hl = hr.landmarks()[h]; val hw = hr.worldLandmarks()[h]
                // Attribution par proximité avec les poignets du corps (fiable quelle que soit la caméra).
                val wx = hl[0].x(); val wy = hl[0].y()
                val dl = sq(wx - image[PL.L_WRIST].x) + sq(wy - image[PL.L_WRIST].y)
                val dr = sq(wx - image[PL.R_WRIST].x) + sq(wy - image[PL.R_WRIST].y)
                val pts = Array(21) { Vec3(hw[it].x().toDouble(), -hw[it].y().toDouble(), -hw[it].z().toDouble()) }
                val ip = Array(21) { ImagePoint(hl[it].x(), hl[it].y(), 1f) }
                if (dl <= dr && left == null) { left = handFilterL.apply(pts, t); leftImg = ip } else if (right == null) { right = handFilterR.apply(pts, t); rightImg = ip }
            }
        }.onFailure { Log.w("BodyTracker", "mains", it) }

        var bs: Map<String, Float>? = null
        if (face) runCatching {
            val fr = faceLandmarker().detectForVideo(img, ts)
            fr.faceBlendshapes().orElse(null)?.firstOrNull()?.let { cats -> bs = cats.associate { it.categoryName() to it.score() } }
        }.onFailure { Log.w("BodyTracker", "visage", it) }

        return PoseFrame(ts, world, image, aspect, left, right, leftImg, rightImg, bs)
    }

    private fun sq(f: Float) = f * f

    override fun close() {
        runCatching { pose.close() }; runCatching { hand?.close() }; runCatching { faceLm?.close() }
    }
}
