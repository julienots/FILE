package com.superessence.bodymotion

import com.superessence.bodymotion.anim.AnimationClip
import com.superessence.bodymotion.anim.ClipOps
import com.superessence.bodymotion.export.BvhExporter
import com.superessence.bodymotion.export.EngineProfile
import com.superessence.bodymotion.export.ExportContent
import com.superessence.bodymotion.export.ExportFormat
import com.superessence.bodymotion.export.ExportOptions
import com.superessence.bodymotion.export.ExportPrep
import com.superessence.bodymotion.export.ExportQuality
import com.superessence.bodymotion.export.FbxExporter
import com.superessence.bodymotion.export.GltfExporter
import com.superessence.bodymotion.io.fbx.FbxBinary
import com.superessence.bodymotion.io.fbx.FbxImporter
import com.superessence.bodymotion.io.gltf.GltfDocument
import com.superessence.bodymotion.io.list
import com.superessence.bodymotion.io.obj
import com.superessence.bodymotion.math.Quat
import com.superessence.bodymotion.math.Trs
import com.superessence.bodymotion.math.Vec3
import com.superessence.bodymotion.mocap.ImagePoint
import com.superessence.bodymotion.mocap.PL
import com.superessence.bodymotion.mocap.PoseFrame
import com.superessence.bodymotion.retarget.PoseEvaluator
import com.superessence.bodymotion.retarget.RetargetOptions
import com.superessence.bodymotion.retarget.Retargeter
import com.superessence.bodymotion.retarget.SkeletalPose
import com.superessence.bodymotion.rig.BodyGroup
import com.superessence.bodymotion.rig.FaceMapping
import com.superessence.bodymotion.rig.HumanBone
import com.superessence.bodymotion.rig.HumanBone.*
import com.superessence.bodymotion.rig.HumanoidMapper
import com.superessence.bodymotion.rig.Skeleton
import com.superessence.bodymotion.rig.SkeletonNode
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.File
import kotlin.math.PI
import kotlin.math.acos
import kotlin.math.sin
import kotlin.random.Random

class CoreTest {
    private val glb = File("src/main/assets/mannequin.glb").readBytes()
    private val doc = GltfDocument.parseGlb(glb)
    private val skel = Skeleton.fromGltf(doc)
    private val map = HumanoidMapper.autoMap(doc, skel)
    private val outDir = File("build/test-exports").apply { mkdirs() }

    @Test fun mappingFindsEveryBone() {
        assertTrue("manquants: ${map.missingRequired()}", map.isUsable)
        assertEquals(30, map.fingerCount)
        assertEquals(HumanBone.entries.size - 1, map.map.size) // tout sauf Jaw
        assertTrue(FaceMapping.build(doc, skel).channels.size == 5)
    }

    /** Pose de référence : rotations locales arbitraires sur le mannequin. */
    private fun sourcePose(t: Double): SkeletalPose {
        val p = Retargeter(skel, map).restPose()
        fun rot(b: HumanBone, axis: Vec3, deg: Double) { val i = map[b]!!; p.local[i] = (p.local[i] * Quat.axisAngle(axis, Math.toRadians(deg))).normalized() }
        val s = sin(t)
        rot(LeftUpperArm, Vec3.Z, -60.0 + 25 * s); rot(LeftUpperArm, Vec3.Y, 30.0)
        rot(LeftLowerArm, Vec3.Y, 70.0 + 20 * s)
        rot(RightUpperArm, Vec3.Z, 75.0); rot(RightUpperArm, Vec3.X, -20.0 * s)
        rot(RightLowerArm, Vec3.Y, -40.0)
        rot(Spine, Vec3.Y, 15.0 * s); rot(Chest, Vec3.X, 10.0)
        rot(Head, Vec3.Y, 25.0 * s); rot(Head, Vec3.X, 10.0)
        rot(LeftUpperLeg, Vec3.X, -45.0 * s); rot(LeftLowerLeg, Vec3.X, 50.0 + 30 * s)
        rot(RightUpperLeg, Vec3.X, 35.0 * s); rot(RightLowerLeg, Vec3.X, 20.0)
        rot(LeftFoot, Vec3.X, 15.0)
        rot(LeftIndexProximal, Vec3.Z, -50.0); rot(LeftIndexIntermediate, Vec3.Z, -60.0)
        return p
    }

    /** Points « MediaPipe » synthétiques générés depuis une pose du mannequin. */
    private fun landmarks(pose: SkeletalPose, sk: Skeleton = skel, mp: com.superessence.bodymotion.rig.BoneMap = map): PoseFrame {
        val g = PoseEvaluator.globals(sk, pose)
        fun P(b: HumanBone) = g[mp[b]!!].translation()
        val headM = g[mp[Head]!!]
        val head = headM.translation()
        fun hp(v: Vec3) = head + headM.transformDir(v).normalized() * v.length()
        val w = Array(PL.COUNT) { Vec3.ZERO }
        w[PL.NOSE] = hp(Vec3(0.0, 0.1, 0.11)); w[PL.L_EAR] = hp(Vec3(0.085, 0.1, 0.0)); w[PL.R_EAR] = hp(Vec3(-0.085, 0.1, 0.0))
        w[PL.L_EYE] = hp(Vec3(0.035, 0.12, 0.09)); w[PL.R_EYE] = hp(Vec3(-0.035, 0.12, 0.09))
        w[PL.L_SHOULDER] = P(LeftUpperArm); w[PL.R_SHOULDER] = P(RightUpperArm)
        w[PL.L_ELBOW] = P(LeftLowerArm); w[PL.R_ELBOW] = P(RightLowerArm)
        w[PL.L_WRIST] = P(LeftHand); w[PL.R_WRIST] = P(RightHand)
        w[PL.L_INDEX] = P(LeftIndexProximal); w[PL.R_INDEX] = P(RightIndexProximal)
        w[PL.L_PINKY] = P(LeftLittleProximal); w[PL.R_PINKY] = P(RightLittleProximal)
        w[PL.L_THUMB] = P(LeftThumbIntermediate); w[PL.R_THUMB] = P(RightThumbIntermediate)
        w[PL.L_HIP] = P(LeftUpperLeg); w[PL.R_HIP] = P(RightUpperLeg)
        w[PL.L_KNEE] = P(LeftLowerLeg); w[PL.R_KNEE] = P(RightLowerLeg)
        w[PL.L_ANKLE] = P(LeftFoot); w[PL.R_ANKLE] = P(RightFoot)
        w[PL.L_FOOT] = P(LeftToes); w[PL.R_FOOT] = P(RightToes)
        w[PL.L_HEEL] = P(LeftFoot) - (P(LeftToes) - P(LeftFoot)) * 0.3
        w[PL.R_HEEL] = P(RightFoot) - (P(RightToes) - P(RightFoot)) * 0.3
        val c = (w[PL.L_HIP] + w[PL.R_HIP]) * 0.5
        val world = Array(PL.COUNT) { w[it] - c }
        val img = Array(PL.COUNT) { ImagePoint((0.5 + w[it].x * 0.3).toFloat(), (0.9 - w[it].y * 0.3).toFloat(), 1f) }
        // Main gauche détaillée (21 points)
        fun hand(left: Boolean): Array<Vec3> {
            val s = if (left) "Left" else "Right"
            val h = Array(21) { Vec3.ZERO }
            h[0] = P(if (left) LeftHand else RightHand)
            val fingers = listOf("Thumb", "Index", "Middle", "Ring", "Little")
            fingers.forEachIndexed { fi, f ->
                val base = 1 + fi * 4
                listOf("Proximal", "Intermediate", "Distal").forEachIndexed { k, seg -> h[base + k] = P(HumanBone.valueOf(s + f + seg)) }
                val endName = "$s${f}_End"
                h[base + 3] = g[sk.indexOf(endName)].translation()
            }
            return h
        }
        return PoseFrame(0, world, img, 1f, hand(true), hand(false))
    }

    private fun angleDeg(a: Vec3, b: Vec3) = Math.toDegrees(acos((a.normalized() dot b.normalized()).coerceIn(-1.0, 1.0)))

    private val checks = listOf(LeftUpperArm to LeftLowerArm, LeftLowerArm to LeftHand, RightUpperArm to RightLowerArm, RightLowerArm to RightHand,
        LeftUpperLeg to LeftLowerLeg, LeftLowerLeg to LeftFoot, RightUpperLeg to RightLowerLeg, RightLowerLeg to RightFoot, LeftFoot to LeftToes,
        LeftIndexProximal to LeftIndexIntermediate, LeftIndexIntermediate to LeftIndexDistal, Hips to Neck, Neck to Head)

    private fun maxError(src: SkeletalPose, sk2: Skeleton, map2: com.superessence.bodymotion.rig.BoneMap, res: SkeletalPose): Double {
        val g1 = PoseEvaluator.globals(skel, src); val g2 = PoseEvaluator.globals(sk2, res)
        val errs = checks.map { (a, b) ->
            "$a" to angleDeg(g1[map[b]!!].translation() - g1[map[a]!!].translation(), g2[map2[b]!!].translation() - g2[map2[a]!!].translation())
        }
        println(errs.joinToString { "%s=%.2f".format(it.first, it.second) })
        return errs.maxOf { it.second }
    }

    @Test fun retargetReproducesSegmentDirections() {
        val rt = Retargeter(skel, map)
        val calib = rt.calibrate(landmarks(rt.restPose()))
        for (t in listOf(0.0, 0.7, 1.9, 3.3)) {
            val src = sourcePose(t)
            val res = rt.solve(landmarks(src), calib, RetargetOptions(rootMotion = false))!!
            val err = maxError(src, skel, map, res)
            assertTrue("erreur max $err°", err < 3.0)
        }
    }

    /** Même test sur un squelette dont chaque os a une orientation de repos aléatoire (rig « réel »). */
    @Test fun retargetWorksWithArbitraryRestOrientations() {
        val rnd = Random(42)
        val frames = skel.nodes.map { if (it.isJoint) Quat.fromEulerXYZDegrees(Vec3(rnd.nextDouble(-180.0, 180.0), rnd.nextDouble(-90.0, 90.0), rnd.nextDouble(-180.0, 180.0))) else Quat.IDENTITY }
        val nodes2 = skel.nodes.map { n ->
            val p = n.parent
            val fp = if (p >= 0) frames[p] else Quat.IDENTITY
            val dp = if (p >= 0) skel.restPos(p) else Vec3.ZERO
            val pos = skel.restPos(n.index)
            val gpRot = if (p >= 0) skel.restGlobalRot[p] else Quat.IDENTITY
            // nouvelle base globale = frames[i] ; local = Fp^-1 * Fi ; translation = Fp^-1 * (pos - posParent)
            SkeletonNode(n.index, n.name, n.parent, n.children,
                Trs(if (p >= 0) fp.conjugate().rotate(pos - dp) else pos, (fp.conjugate() * frames[n.index]).normalized(), Vec3.ONE), n.isJoint)
        }
        val sk2 = Skeleton(nodes2, emptyList())
        val map2 = HumanoidMapper.autoMap(null, sk2)
        assertTrue(map2.isUsable)
        for (n in sk2.nodes) assertTrue((sk2.restPos(n.index) - skel.restPos(n.index)).length() < 1e-6)
        val rt = Retargeter(sk2, map2)
        val calib = rt.calibrate(landmarks(rt.restPose(), sk2, map2))
        for (t in listOf(0.3, 1.2, 2.5)) {
            val src = sourcePose(t)
            val res = rt.solve(landmarks(src), calib, RetargetOptions(rootMotion = false))!!
            val err = maxError(src, sk2, map2, res)
            assertTrue("erreur max $err°", err < 3.0)
        }
    }

    private fun makeClip(n: Int = 60): AnimationClip {
        val rt = Retargeter(skel, map)
        val calib = rt.calibrate(landmarks(rt.restPose()))
        val poses = (0 until n).map { f -> rt.solve(landmarks(sourcePose(f * 2 * PI / n)), calib, RetargetOptions(rootMotion = false))!! }
        return AnimationClip.fromPoses("c1", "Test", "mannequin", 30.0, skel, map, poses)
    }

    @Test fun feetStayOnGround() {
        val rt = Retargeter(skel, map)
        val calib = rt.calibrate(landmarks(rt.restPose()))
        val res = rt.solve(landmarks(sourcePose(1.0)), calib, RetargetOptions())!!
        val g = PoseEvaluator.globals(skel, res)
        val minY = listOf(LeftFoot, RightFoot, LeftToes, RightToes).minOf { g[map[it]!!].translation().y - skel.restPos(map[it]!!).y }
        assertTrue("pieds $minY", minY >= -1e-6 && minY < 0.03)
    }

    @Test fun clipEditingAndSerialization() {
        val clip = makeClip()
        assertTrue(clip.keys[BodyGroup.LeftArm]!!.size >= 2)
        val bytes = ByteArrayOutputStream().also { clip.write(it) }.toByteArray()
        val back = AnimationClip.read(ByteArrayInputStream(bytes))
        assertEquals(clip.frameCount, back.frameCount)
        assertTrue((back.rotations[10][3].dot(clip.rotations[10][3])) > 0.9999)
        // suppression de clé => interpolation
        val c2 = clip.deepCopy()
        val keys = c2.keys[BodyGroup.LeftArm]!!.toList()
        val k = keys[keys.size / 2]
        ClipOps.deleteKey(c2, map, BodyGroup.LeftArm, k)
        assertTrue(!c2.keys[BodyGroup.LeftArm]!!.contains(k))
        val trimmed = ClipOps.trim(clip, 10, 29)
        assertEquals(20, trimmed.frameCount)
        val merged = ClipOps.merge(clip, trimmed, 5, "M")
        assertEquals(clip.frameCount + trimmed.frameCount - 5, merged.frameCount)
        ClipOps.smooth(c2, map, null, 0, c2.frameCount - 1, 2.0)
        ClipOps.makeLoop(c2, 8)
        assertTrue(c2.rotations.last()[0].dot(c2.rotations[0][0]) > 0.9999)
        val slow = ClipOps.changeSpeed(clip, 0.5)
        assertEquals(clip.frameCount * 2 - 1, slow.frameCount)
    }

    @Test fun exportsAreReadable() {
        val clip = makeClip()
        val clip2 = ClipOps.trim(clip, 0, 29, "Court")
        for (profile in listOf(EngineProfile.GENERIC, EngineProfile.GODOT)) {
            val opt = ExportOptions(ExportFormat.GLB, ExportContent.FULL, profile)
            val prepared = listOf(clip, clip2).map { ExportPrep.prepare(it, skel, opt) }
            val out = GltfExporter.export(glb, skel, prepared, opt)
            val d = GltfDocument.parseGlb(out)
            assertEquals(2, d.json.list("animations").size)
            assertTrue(d.meshes.size == 2)
            File(outDir, "full_${profile.name}.glb").writeBytes(out)
        }
        val optA = ExportOptions(ExportFormat.GLB, ExportContent.ANIMATION_ONLY, quality = ExportQuality.OPTIMIZED)
        val a = GltfExporter.export(glb, skel, listOf(ExportPrep.prepare(clip, skel, optA)), optA)
        val da = GltfDocument.parseGlb(a)
        assertTrue(da.json["meshes"] == null && da.json.list("animations").size == 1)
        File(outDir, "anim_only.glb").writeBytes(a)
        val optT = ExportOptions(ExportFormat.GLTF, fps = 60)
        File(outDir, "full.gltf").writeBytes(GltfExporter.export(glb, skel, listOf(ExportPrep.prepare(clip, skel, optT)), optT))
        val optB = ExportOptions(ExportFormat.BVH)
        File(outDir, "anim.bvh").writeBytes(BvhExporter.export(skel, ExportPrep.prepare(clip, skel, optB)))
        for (profile in listOf(EngineProfile.UNITY, EngineProfile.UNREAL)) {
            val optF = ExportOptions(ExportFormat.FBX, ExportContent.ANIMATION_ONLY, profile)
            val fbx = FbxExporter.export(skel, listOf(clip, clip2).map { ExportPrep.prepare(it, skel, optF) }, optF)
            val top = FbxBinary.read(fbx)
            assertTrue(top.first { it.name == "Objects" }.all("AnimationCurve").isNotEmpty())
            File(outDir, "anim_${profile.name}.fbx").writeBytes(fbx)
        }
    }

    /** Import FBX : fichier exporté par Blender depuis le mannequin (voir tools/validate_exports.py). */
    @Test fun fbxImportFromBlender() {
        val f = File("build/test-exports/blender_mannequin.fbx")
        if (!f.exists()) return
        val d = FbxImporter().convert(f.readBytes())
        val sk = Skeleton.fromGltf(d)
        val mp = HumanoidMapper.autoMap(d, sk)
        assertTrue("manquants ${mp.missingRequired()}", mp.isUsable)
        assertTrue(sk.jointCount >= 60)
        assertTrue(sk.morphTargets.isNotEmpty())
        val h = sk.restHeight()
        assertTrue("hauteur $h", h > 1.5 && h < 2.0)
        File(outDir, "fbx_converted.glb").writeBytes(d.toGlb())
    }
}
