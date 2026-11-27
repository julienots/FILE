package com.superessence.bodymotion.mocap

import com.superessence.bodymotion.math.Vec3

/** Indices des 33 points du modèle de pose MediaPipe. */
object PL {
    const val NOSE = 0
    const val L_EYE = 2; const val R_EYE = 5
    const val L_EAR = 7; const val R_EAR = 8
    const val MOUTH_L = 9; const val MOUTH_R = 10
    const val L_SHOULDER = 11; const val R_SHOULDER = 12
    const val L_ELBOW = 13; const val R_ELBOW = 14
    const val L_WRIST = 15; const val R_WRIST = 16
    const val L_PINKY = 17; const val R_PINKY = 18
    const val L_INDEX = 19; const val R_INDEX = 20
    const val L_THUMB = 21; const val R_THUMB = 22
    const val L_HIP = 23; const val R_HIP = 24
    const val L_KNEE = 25; const val R_KNEE = 26
    const val L_ANKLE = 27; const val R_ANKLE = 28
    const val L_HEEL = 29; const val R_HEEL = 30
    const val L_FOOT = 31; const val R_FOOT = 32
    const val COUNT = 33

    /** Segments dessinés pour l'aperçu du squelette détecté. */
    val SEGMENTS = listOf(
        L_SHOULDER to R_SHOULDER, L_SHOULDER to L_HIP, R_SHOULDER to R_HIP, L_HIP to R_HIP,
        L_SHOULDER to L_ELBOW, L_ELBOW to L_WRIST, R_SHOULDER to R_ELBOW, R_ELBOW to R_WRIST,
        L_WRIST to L_INDEX, L_WRIST to L_PINKY, R_WRIST to R_INDEX, R_WRIST to R_PINKY,
        L_HIP to L_KNEE, L_KNEE to L_ANKLE, R_HIP to R_KNEE, R_KNEE to R_ANKLE,
        L_ANKLE to L_HEEL, L_HEEL to L_FOOT, L_ANKLE to L_FOOT, R_ANKLE to R_HEEL, R_HEEL to R_FOOT, R_ANKLE to R_FOOT,
        L_EAR to L_EYE, L_EYE to NOSE, NOSE to R_EYE, R_EYE to R_EAR,
    )
}

/** Point 2D normalisé de l'image (0..1) et sa fiabilité. */
data class ImagePoint(val x: Float, val y: Float, val visibility: Float)

/**
 * Une image de capture, indépendante de MediaPipe.
 * Repère « monde » : mètres, origine au centre du bassin, Y vers le haut,
 * +X vers la gauche du sujet (droite de l'image), +Z vers la caméra.
 */
class PoseFrame(
    val timeMs: Long,
    val world: Array<Vec3>?,
    val image: Array<ImagePoint>?,
    val imageAspect: Float,
    val leftHand: Array<Vec3>? = null,
    val rightHand: Array<Vec3>? = null,
    val leftHandImage: Array<ImagePoint>? = null,
    val rightHandImage: Array<ImagePoint>? = null,
    val faceBlendshapes: Map<String, Float>? = null,
) {
    val hasBody get() = world != null && image != null

    /** Qualité moyenne du suivi des points du corps utiles (0..1). */
    fun trackingQuality(): Float {
        val img = image ?: return 0f
        val idx = intArrayOf(PL.L_SHOULDER, PL.R_SHOULDER, PL.L_ELBOW, PL.R_ELBOW, PL.L_WRIST, PL.R_WRIST, PL.L_HIP, PL.R_HIP, PL.L_KNEE, PL.R_KNEE, PL.L_ANKLE, PL.R_ANKLE)
        return idx.map { img[it].visibility }.average().toFloat()
    }

    fun visible(i: Int, threshold: Float = 0.5f) = (image?.get(i)?.visibility ?: 0f) >= threshold
}
