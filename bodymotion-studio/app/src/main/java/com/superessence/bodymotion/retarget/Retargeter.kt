package com.superessence.bodymotion.retarget

import com.superessence.bodymotion.math.Quat
import com.superessence.bodymotion.math.Vec3
import com.superessence.bodymotion.mocap.PL
import com.superessence.bodymotion.mocap.PoseFrame
import com.superessence.bodymotion.rig.BoneMap
import com.superessence.bodymotion.rig.HumanBone
import com.superessence.bodymotion.rig.HumanBone.*
import com.superessence.bodymotion.rig.Skeleton
import kotlin.math.acos
import kotlin.math.max
import kotlin.math.sqrt

/** Pose du personnage : rotation locale de chaque nœud + translation locale du bassin. */
class SkeletalPose(val local: Array<Quat>, var hipsLocalT: Vec3, val hipsNode: Int) {
    fun copy() = SkeletalPose(local.copyOf(), hipsLocalT, hipsNode)
}

/** Paramètres mesurés lors de la calibration (le sujet se tient droit, face à la caméra). */
data class Calibration(
    val hipX: Float = 0.5f,
    val hipY: Float = 0.55f,
    val torsoImg: Float = 0.25f,
    val groundOffset: Float = 0.35f,
    val subjectLegM: Double = 0.85,
    val subjectTorsoM: Double = 0.5,
    val headCorrection: Quat? = null,
    val calibrated: Boolean = false,
)

data class RetargetOptions(
    val rootMotion: Boolean = true,
    val keepModelFacing: Boolean = true,
    val fingers: Boolean = true,
    val groundFeet: Boolean = true,
)

/**
 * Retargeting direct « par directions » : chaque os du personnage est orienté pour suivre la direction
 * du segment corporel détecté, à partir de SA propre pose de repos. Les longueurs d'os du personnage
 * ne changent jamais : les proportions du modèle sont toujours respectées, quelles qu'elles soient.
 */
class Retargeter(val skel: Skeleton, val map: BoneMap) {
    private val boneOf = HashMap<Int, HumanBone>().apply { map.map.forEach { (b, n) -> put(n, b) } }
    private fun pos(b: HumanBone) = map[b]?.let { skel.restPos(it) }

    /** Repère de repos du corps : X = gauche du personnage, Y = haut. */
    private val restBodyFrame: Quat
    val restForward: Vec3
    private val restBodyUp: Vec3
    val hipsIndex = map[Hips]!!
    private val legLength: Double
    private val restHipsWorld = skel.restPos(hipsIndex)
    private val feet: List<Int>
    private val restFootY: Map<Int, Double>
    private val facingAlign: Quat

    init {
        val l = pos(LeftUpperLeg)!!; val r = pos(RightUpperLeg)!!
        val top = (pos(Neck) ?: pos(Head) ?: pos(UpperChest) ?: pos(Chest) ?: pos(Spine))!!
        restBodyUp = (top - restHipsWorld).normalized()
        restBodyFrame = Quat.fromBasis(l - r, restBodyUp)
        restForward = restBodyFrame.rotate(Vec3.Z)
        legLength = listOf(LeftUpperLeg to LeftLowerLeg, LeftLowerLeg to LeftFoot).sumOf { (a, b) -> (pos(b)!! - pos(a)!!).length() }
        feet = listOfNotNull(map[LeftFoot], map[RightFoot], map[LeftToes], map[RightToes])
        restFootY = feet.associateWith { skel.restPos(it).y }
        val fwdFlat = Vec3(restForward.x, 0.0, restForward.z).normalized()
        facingAlign = if (fwdFlat == Vec3.ZERO) Quat.IDENTITY else Quat.fromTo(Vec3.Z, fwdFlat)
    }

    fun restPose() = SkeletalPose(Array(skel.nodes.size) { skel.nodes[it].rest.r }, skel.nodes[hipsIndex].rest.t, hipsIndex)

    /** Mesure de calibration : à appeler avec une image où le sujet est droit, de face. */
    fun calibrate(frame: PoseFrame): Calibration {
        val img = frame.image!!; val w = frame.world!!
        val hx = (img[PL.L_HIP].x + img[PL.R_HIP].x) / 2; val hy = (img[PL.L_HIP].y + img[PL.R_HIP].y) / 2
        val torso = torsoImg(frame)
        val ground = maxOf(img[PL.L_HEEL].y, img[PL.R_HEEL].y, img[PL.L_ANKLE].y, img[PL.R_ANKLE].y)
        val leg = ((w[PL.L_KNEE] - w[PL.L_HIP]).length() + (w[PL.L_ANKLE] - w[PL.L_KNEE]).length() +
            (w[PL.R_KNEE] - w[PL.R_HIP]).length() + (w[PL.R_ANKLE] - w[PL.R_KNEE]).length()) / 2
        val torsoM = (mid(w[PL.L_SHOULDER], w[PL.R_SHOULDER]) - mid(w[PL.L_HIP], w[PL.R_HIP])).length()
        val d = bodyDeltas(w, null, applyHeadCorr = false)
        val headCorr = d.head?.let { h -> d.chest.let { c -> (h.inverse() * c).normalized() } }
        return Calibration(hx, hy, torso, ground - hy, leg, torsoM, headCorr, true)
    }

    private fun torsoImg(f: PoseFrame): Float {
        val i = f.image!!
        val sx = (i[PL.L_SHOULDER].x + i[PL.R_SHOULDER].x) / 2 * f.imageAspect
        val sy = (i[PL.L_SHOULDER].y + i[PL.R_SHOULDER].y) / 2
        val hx = (i[PL.L_HIP].x + i[PL.R_HIP].x) / 2 * f.imageAspect
        val hy = (i[PL.L_HIP].y + i[PL.R_HIP].y) / 2
        return sqrt(((sx - hx) * (sx - hx) + (sy - hy) * (sy - hy)).toDouble()).toFloat().coerceAtLeast(1e-3f)
    }

    private class Deltas(val hips: Quat, val chest: Quat, val head: Quat?)

    private fun bodyDeltas(w: Array<Vec3>, calib: Calibration?, applyHeadCorr: Boolean = true): Deltas {
        val hipsT = Quat.fromBasis(w[PL.L_HIP] - w[PL.R_HIP], mid(w[PL.L_SHOULDER], w[PL.R_SHOULDER]) - mid(w[PL.L_HIP], w[PL.R_HIP]))
        val hips = (hipsT * restBodyFrame.inverse()).normalized()
        val chestT = Quat.fromBasis(w[PL.L_SHOULDER] - w[PL.R_SHOULDER], mid(w[PL.L_SHOULDER], w[PL.R_SHOULDER]) - mid(w[PL.L_HIP], w[PL.R_HIP]))
        val chest = (chestT * restBodyFrame.inverse()).normalized()
        val earL = w[PL.L_EAR]; val earR = w[PL.R_EAR]
        val x = earL - earR
        val f = w[PL.NOSE] - mid(earL, earR)
        var head: Quat? = null
        if (x.length() > 1e-4 && f.length() > 1e-4) {
            val headT = Quat.fromBasis(x, f cross x)
            head = (headT * restBodyFrame.inverse()).normalized()
            if (applyHeadCorr) {
                val corr = calib?.headCorrection ?: (restBodyFrame * Quat.axisAngle(Vec3.X, -0.35) * restBodyFrame.inverse())
                head = (head * corr).normalized()
            }
        }
        return Deltas(hips, chest, head)
    }

    /**
     * Calcule la pose du personnage pour une image de capture.
     * @return null si le corps n'est pas détecté.
     */
    fun solve(frame: PoseFrame, calib: Calibration, opt: RetargetOptions): SkeletalPose? {
        val w = frame.world ?: return null
        val n = skel.nodes.size
        val local = Array(n) { skel.nodes[it].rest.r }
        val global = arrayOfNulls<Quat>(n)
        val align = if (opt.keepModelFacing) facingAlign else Quat.IDENTITY
        fun alignDir(v: Vec3) = align.rotate(v)
        val d = bodyDeltas(w, calib)
        val dHips = (align * d.hips).normalized()
        val dChest = (align * d.chest).normalized()
        val dHead = d.head?.let { (align * it).normalized() }

        val spineChain = listOf(Spine, Chest, UpperChest).filter { map[it] != null }
        val chainCount = spineChain.size

        // Directions cibles des segments (dans le repère du personnage).
        fun seg(a: Int, b: Int) = alignDir(w[b] - w[a])
        val target = HashMap<HumanBone, Vec3>()
        target[LeftUpperArm] = seg(PL.L_SHOULDER, PL.L_ELBOW)
        target[LeftLowerArm] = seg(PL.L_ELBOW, PL.L_WRIST)
        target[RightUpperArm] = seg(PL.R_SHOULDER, PL.R_ELBOW)
        target[RightLowerArm] = seg(PL.R_ELBOW, PL.R_WRIST)
        target[LeftUpperLeg] = seg(PL.L_HIP, PL.L_KNEE)
        target[LeftLowerLeg] = seg(PL.L_KNEE, PL.L_ANKLE)
        target[RightUpperLeg] = seg(PL.R_HIP, PL.R_KNEE)
        target[RightLowerLeg] = seg(PL.R_KNEE, PL.R_ANKLE)
        target[LeftFoot] = seg(PL.L_HEEL, PL.L_FOOT)
        target[RightFoot] = seg(PL.R_HEEL, PL.R_FOOT)
        val lHandDir = alignDir(mid(w[PL.L_INDEX], w[PL.L_PINKY]) - w[PL.L_WRIST])
        val rHandDir = alignDir(mid(w[PL.R_INDEX], w[PL.R_PINKY]) - w[PL.R_WRIST])
        target[LeftHand] = lHandDir
        target[RightHand] = rHandDir
        val handSide = HashMap<HumanBone, Vec3>()
        handSide[LeftHand] = alignDir(w[PL.L_INDEX] - w[PL.L_PINKY])
        handSide[RightHand] = alignDir(w[PL.R_INDEX] - w[PL.R_PINKY])

        // Mains détaillées (21 points) : doigts + orientation fine du poignet.
        if (opt.fingers) {
            frame.leftHand?.let { addHand(it, true, target, handSide, ::alignDir) }
            frame.rightHand?.let { addHand(it, false, target, handSide, ::alignDir) }
        }

        val fwd = restForward
        for (i in skel.order) {
            val p = skel.nodes[i].parent
            val gp = if (p < 0) Quat.IDENTITY else global[p]!!
            val noSwing = (gp * skel.nodes[i].rest.r).normalized()
            val bone = boneOf[i]
            var g = noSwing
            when {
                bone == Hips -> g = (dHips * skel.restGlobalRot[i]).normalized()
                bone != null && bone in spineChain -> {
                    val k = spineChain.indexOf(bone) + 1
                    g = (dHips.slerp(dChest, k.toDouble() / chainCount) * skel.restGlobalRot[i]).normalized()
                }
                bone == Neck && dHead != null -> g = (dChest.slerp(dHead, 0.5) * skel.restGlobalRot[i]).normalized()
                bone == Head && dHead != null -> g = (dHead * skel.restGlobalRot[i]).normalized()
                bone != null && target[bone] != null -> {
                    val restDir = restDirection(bone, i)
                    if (restDir != null) {
                        val cur = (noSwing * skel.restGlobalRot[i].conjugate()).rotate(restDir)
                        g = (Quat.fromTo(cur, target[bone]!!) * noSwing).normalized()
                        // Torsion : plan de flexion (coude / genou) ou axe latéral de la main.
                        val child = childBone(bone)
                        val bendRest = when (bone) {
                            LeftUpperArm, RightUpperArm -> fwd
                            LeftUpperLeg, RightUpperLeg -> -fwd
                            else -> null
                        }
                        if (child != null && bendRest != null && target[child] != null) {
                            g = applyBendTwist(g, i, restDir, bendRest, target[bone]!!, target[child]!!)
                        }
                        if (bone == LeftHand || bone == RightHand) {
                            g = applyHandTwist(g, i, bone, target[bone]!!, handSide[bone]!!)
                        }
                    }
                }
            }
            global[i] = g
            local[i] = (gp.conjugate() * g).normalized()
        }

        val pose = SkeletalPose(local, skel.nodes[hipsIndex].rest.t, hipsIndex)
        placeHips(pose, frame, calib, opt, align)
        return pose
    }

    private fun addHand(h: Array<Vec3>, left: Boolean, target: HashMap<HumanBone, Vec3>, side: HashMap<HumanBone, Vec3>, alignDir: (Vec3) -> Vec3) {
        val s = if (left) "Left" else "Right"
        val fingers = listOf("Thumb" to 1, "Index" to 5, "Middle" to 9, "Ring" to 13, "Little" to 17)
        val segs = listOf("Proximal", "Intermediate", "Distal")
        for ((f, base) in fingers) {
            for (k in 0 until 3) {
                val a = if (f == "Thumb") base + k else base + k
                val b = a + 1
                val bone = HumanBone.valueOf(s + f + segs[k])
                target[bone] = alignDir(h[b] - h[a])
            }
        }
        val handBone = if (left) LeftHand else RightHand
        target[handBone] = alignDir(h[9] - h[0])
        side[handBone] = alignDir(h[5] - h[17])
    }

    private fun childBone(b: HumanBone) = when (b) {
        LeftUpperArm -> LeftLowerArm; RightUpperArm -> RightLowerArm
        LeftUpperLeg -> LeftLowerLeg; RightUpperLeg -> RightLowerLeg
        else -> null
    }

    /** Direction de repos (monde) d'un os : vers son os enfant humanoïde. */
    private val restDirCache = HashMap<HumanBone, Vec3?>()
    private fun restDirection(b: HumanBone, i: Int): Vec3? = synchronized(restDirCache) { restDirCache.getOrPut(b) {
        val here = skel.restPos(i)
        val nextName: HumanBone? = when (b) {
            LeftUpperArm -> LeftLowerArm; LeftLowerArm -> LeftHand; RightUpperArm -> RightLowerArm; RightLowerArm -> RightHand
            LeftUpperLeg -> LeftLowerLeg; LeftLowerLeg -> LeftFoot; RightUpperLeg -> RightLowerLeg; RightLowerLeg -> RightFoot
            LeftFoot -> LeftToes; RightFoot -> RightToes
            LeftHand -> LeftMiddleProximal; RightHand -> RightMiddleProximal
            else -> fingerNext(b)
        }
        val next = nextName?.let { map[it] }
        when {
            next != null -> (skel.restPos(next) - here).normalized()
            b == LeftHand -> (here - pos(LeftLowerArm)!!).normalized()
            b == RightHand -> (here - pos(RightLowerArm)!!).normalized()
            b == LeftFoot || b == RightFoot -> (restForward - restBodyUp * 0.35).normalized()
            b.isFinger -> {
                // Dernière phalange : prolonge la précédente, ou un enfant non humanoïde.
                val ch = skel.nodes[i].children.firstOrNull()
                if (ch != null) (skel.restPos(ch) - here).normalized()
                else skel.nodes[i].parent.let { p -> if (p >= 0) (here - skel.restPos(p)).normalized() else null }
            }
            else -> null
        }?.takeIf { it != Vec3.ZERO }
    } }

    private fun fingerNext(b: HumanBone): HumanBone? {
        if (!b.isFinger) return null
        val n = b.name
        return when {
            n.endsWith("Proximal") -> HumanBone.valueOf(n.replace("Proximal", "Intermediate"))
            n.endsWith("Intermediate") -> HumanBone.valueOf(n.replace("Intermediate", "Distal"))
            else -> null
        }
    }

    private fun applyBendTwist(g: Quat, i: Int, restDir: Vec3, bendRest: Vec3, upperT: Vec3, lowerT: Vec3): Quat {
        val u = upperT.normalized(); val l = lowerT.normalized()
        val bend = acos((u dot l).coerceIn(-1.0, 1.0))
        val weight = ((bend - 0.25) / 0.35).coerceIn(0.0, 1.0)
        if (weight <= 0) return g
        val nRest = (restDir cross bendRest).normalized()
        if (nRest == Vec3.ZERO) return g
        val nCur = (g * skel.restGlobalRot[i].conjugate()).rotate(nRest)
        val nT = (u cross l).normalized()
        val a = (nCur - u * (nCur dot u)).normalized(); val b = (nT - u * (nT dot u)).normalized()
        if (a == Vec3.ZERO || b == Vec3.ZERO) return g
        val tw = Quat.IDENTITY.slerp(Quat.fromTo(a, b), weight)
        return (tw * g).normalized()
    }

    private fun applyHandTwist(g: Quat, i: Int, bone: HumanBone, dirT: Vec3, sideT: Vec3): Quat {
        val left = bone == LeftHand
        val a0 = map[if (left) LeftIndexProximal else RightIndexProximal]
        val b0 = map[if (left) LeftLittleProximal else RightLittleProximal]
        val sideRest = if (a0 != null && b0 != null) (skel.restPos(a0) - skel.restPos(b0)).normalized() else return g
        val d = dirT.normalized()
        val cur = (g * skel.restGlobalRot[i].conjugate()).rotate(sideRest)
        val a = (cur - d * (cur dot d)).normalized(); val b = (sideT - d * (sideT dot d)).normalized()
        if (a == Vec3.ZERO || b == Vec3.ZERO) return g
        return (Quat.fromTo(a, b) * g).normalized()
    }

    /** Position du bassin : pieds au sol, sauts et déplacements estimés depuis l'image. */
    private fun placeHips(pose: SkeletalPose, frame: PoseFrame, calib: Calibration, opt: RetargetOptions, align: Quat) {
        val img = frame.image ?: return
        val ratio = legLength / calib.subjectLegM.coerceAtLeast(0.2)
        val torso = torsoImg(frame)
        val s = torso / calib.torsoImg
        val metersPerUnit = calib.subjectTorsoM / torso
        val hx = (img[PL.L_HIP].x + img[PL.R_HIP].x) / 2
        val hy = (img[PL.L_HIP].y + img[PL.R_HIP].y) / 2
        var dx = 0.0; var dz = 0.0
        if (opt.rootMotion && calib.calibrated) {
            dx = (hx - calib.hipX) * frame.imageAspect * metersPerUnit
            val d0 = calib.subjectTorsoM / (calib.torsoImg * 2 * 0.577)
            dz = d0 - d0 / s
        }
        val groundNow = hy + calib.groundOffset * s
        val lowestFoot = maxOf(img[PL.L_HEEL].y, img[PL.R_HEEL].y, img[PL.L_FOOT].y, img[PL.R_FOOT].y, img[PL.L_ANKLE].y, img[PL.R_ANKLE].y)
        val lift = if (calib.calibrated) max(0.0, (groundNow - lowestFoot) * metersPerUnit - 0.03) else 0.0
        val move = align.rotate(Vec3(dx, 0.0, dz)) * ratio

        // Pieds au sol : décalage vertical tel que le pied le plus bas retrouve sa hauteur de repos.
        val hipsPar = skel.nodes[hipsIndex].parent
        val parentM = if (hipsPar >= 0) skel.restGlobal[hipsPar] else null
        val restHipsLocal = skel.nodes[hipsIndex].rest.t
        pose.hipsLocalT = restHipsLocal
        var dy = 0.0
        if (opt.groundFeet && feet.isNotEmpty()) {
            val g = PoseEvaluator.globals(skel, pose)
            val m = feet.minOf { g[it].translation().y - restFootY[it]!! }
            dy = -m + lift * ratio
        }
        val world = restHipsWorld + Vec3(move.x, dy, move.z)
        pose.hipsLocalT = parentM?.inverse()?.transformPoint(world) ?: world
    }

    companion object {
        fun mid(a: Vec3, b: Vec3) = (a + b) * 0.5
    }
}

/** Cinématique directe d'une pose (matrices globales de chaque nœud). */
object PoseEvaluator {
    fun globals(skel: Skeleton, pose: SkeletalPose) = skel.computeGlobals { i ->
        val r = skel.nodes[i].rest
        com.superessence.bodymotion.math.Mat4.fromTrs(if (i == pose.hipsNode) pose.hipsLocalT else r.t, pose.local[i], r.s)
    }
}
