package com.superessence.bodymotion.ui.components

import android.view.Choreographer
import android.view.TextureView
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.viewinterop.AndroidView
import com.superessence.bodymotion.data.LoadedCharacter
import com.superessence.bodymotion.render.CharacterViewer
import com.superessence.bodymotion.retarget.Retargeter
import com.superessence.bodymotion.retarget.SkeletalPose
import com.superessence.bodymotion.rig.HumanBone
import com.superessence.bodymotion.ui.theme.C

/** Ce qui est affiché à un instant : pose du corps + poids de blendshapes par nœud. */
class FramePose(val pose: SkeletalPose?, val morphs: Map<Int, FloatArray>? = null)

class ViewportController { var viewer: CharacterViewer? = null; fun resetCamera() = viewer?.resetCamera() }

/**
 * Viewport 3D Filament intégré à Compose. [poseAt] est appelé à chaque image d'affichage.
 * [highlightNode] : articulation sélectionnée (affichée en violet sur le squelette).
 */
@Composable
fun Viewport3D(
    character: LoadedCharacter,
    quality: Int,
    modifier: Modifier = Modifier,
    showBones: Boolean = false,
    highlightNode: Int = -1,
    controller: ViewportController? = null,
    poseAt: (Long) -> FramePose?,
) {
    val currentPose = rememberUpdatedState(poseAt)
    var viewer by remember { mutableStateOf<CharacterViewer?>(null) }
    var joints by remember { mutableStateOf(FloatArray(0)) }
    val bonePairs = remember(character.info.id) { bonePairs(character) }
    val boneIdx = remember(bonePairs) { bonePairs.flatMap { listOf(it.first, it.second) }.distinct().toIntArray() }
    val showB = rememberUpdatedState(showBones)

    if (!com.superessence.bodymotion.BodyMotionApp.renderAvailable) { FallbackViewport(character, modifier, highlightNode, poseAt); return }
    Box(modifier) {
        AndroidView(
            factory = { ctx ->
                TextureView(ctx).also { tv ->
                    val v = CharacterViewer(ctx, tv, quality)
                    runCatching { v.setCharacter(character.glb, character.skeleton, Retargeter(character.skeleton, character.boneMap).restForward) }
                        .onFailure { runCatching { v.setCharacter(character.glb, character.skeleton, com.superessence.bodymotion.math.Vec3.Z) } }
                    viewer = v; controller?.viewer = v
                }
            },
            modifier = Modifier.fillMaxSize(),
        )
        if (showBones && joints.isNotEmpty()) {
            Canvas(Modifier.fillMaxSize()) {
                val pos = HashMap<Int, Offset?>()
                boneIdx.forEachIndexed { k, i -> pos[i] = if (joints[k * 3 + 2] > 0) Offset(joints[k * 3], joints[k * 3 + 1]) else null }
                for ((a, b) in bonePairs) {
                    val pa = pos[a] ?: continue; val pb = pos[b] ?: continue
                    drawLine(C.Cyan.copy(alpha = 0.9f), pa, pb, strokeWidth = 3f, cap = StrokeCap.Round)
                }
                for ((i, p) in pos) if (p != null) {
                    val hl = i == highlightNode
                    drawCircle(Color(0xFF07080B), if (hl) 9f else 5.5f, p)
                    drawCircle(if (hl) C.Violet else Color(0xFFE9FBFF), if (hl) 9f else 5.5f, p, style = androidx.compose.ui.graphics.drawscope.Stroke(2.5f))
                }
            }
        }
    }

    DisposableEffect(viewer) {
        val v = viewer ?: return@DisposableEffect onDispose { }
        var frameCount = 0
        val cb = object : Choreographer.FrameCallback {
            override fun doFrame(t: Long) {
                Choreographer.getInstance().postFrameCallback(this)
                val fp = runCatching { currentPose.value(t) }.getOrNull()
                v.applyPose(fp?.pose)
                fp?.morphs?.let { v.applyMorphs(it) }
                v.render(t)
                if (showB.value && (frameCount++ % 2 == 0)) joints = v.projectJoints(fp?.pose, boneIdx)
            }
        }
        Choreographer.getInstance().postFrameCallback(cb)
        onDispose {
            Choreographer.getInstance().removeFrameCallback(cb)
            controller?.viewer = null
            v.destroy()
        }
    }
}

/** Segments du squelette humanoïde à dessiner (os mappés uniquement). */
fun bonePairs(ch: LoadedCharacter): List<Pair<Int, Int>> {
    val m = ch.boneMap
    val pairs = ArrayList<Pair<Int, Int>>()
    val chain = listOf(
        listOf(HumanBone.Hips, HumanBone.Spine, HumanBone.Chest, HumanBone.UpperChest, HumanBone.Neck, HumanBone.Head),
        listOf(HumanBone.UpperChest, HumanBone.LeftUpperArm, HumanBone.LeftLowerArm, HumanBone.LeftHand, HumanBone.LeftMiddleProximal),
        listOf(HumanBone.UpperChest, HumanBone.RightUpperArm, HumanBone.RightLowerArm, HumanBone.RightHand, HumanBone.RightMiddleProximal),
        listOf(HumanBone.Hips, HumanBone.LeftUpperLeg, HumanBone.LeftLowerLeg, HumanBone.LeftFoot, HumanBone.LeftToes),
        listOf(HumanBone.Hips, HumanBone.RightUpperLeg, HumanBone.RightLowerLeg, HumanBone.RightFoot, HumanBone.RightToes),
    )
    for (c in chain) {
        val present = c.mapNotNull { m[it] }
        // UpperChest absent : on rattache les bras au dernier os de colonne présent.
        for (k in 0 until present.size - 1) pairs.add(present[k] to present[k + 1])
    }
    return pairs.distinct()
}

/** Rendu 2D de secours (vue de face) utilisé seulement si le moteur 3D natif n'est pas chargé (tests JVM). */
@Composable
private fun FallbackViewport(ch: LoadedCharacter, modifier: Modifier, highlight: Int, poseAt: (Long) -> FramePose?) {
    var t by remember { mutableLongStateOf(0L) }
    LaunchedEffect(Unit) { while (true) androidx.compose.runtime.withFrameNanos { t = it } }
    val pairs = remember(ch) { bonePairs(ch) }
    val pose = poseAt(t)?.pose
    val g = if (pose != null) com.superessence.bodymotion.retarget.PoseEvaluator.globals(ch.skeleton, pose) else ch.skeleton.restGlobal
    val h = ch.skeleton.restHeight()
    val minY = ch.skeleton.nodes.filter { it.isJoint }.minOf { ch.skeleton.restPos(it.index).y }
    Canvas(modifier.then(Modifier.fillMaxSize())) {
        drawRect(androidx.compose.ui.graphics.Brush.verticalGradient(listOf(Color(0xFF151A22), Color(0xFF090A0D))))
        val ground = size.height * 0.86f
        for (k in -6..6) drawLine(Color(0xFF1E2430), Offset(size.width / 2 + k * size.width / 6, ground), Offset(size.width / 2 + k * size.width / 1.5f, size.height), 1f)
        drawLine(Color(0xFF2A3140), Offset(0f, ground), Offset(size.width, ground), 1.5f)
        val s = (size.height * 0.7f / h).toFloat()
        fun p(i: Int): Offset { val w = g[i].translation(); return Offset(size.width / 2 + (w.x * s).toFloat(), ground - ((w.y - minY) * s).toFloat()) }
        drawOval(Color(0x66000000), Offset(size.width / 2 - 50f, ground - 8f), androidx.compose.ui.geometry.Size(100f, 16f))
        for ((a, b) in pairs) drawLine(Color(0xFFAEB8C6), p(a), p(b), 16f, StrokeCap.Round)
        for ((a, b) in pairs) drawLine(C.Cyan, p(a), p(b), 3f, StrokeCap.Round)
        ch.boneMap[HumanBone.Head]?.let { drawCircle(Color(0xFFC9D1DC), (h * 0.065 * s).toFloat(), p(it)) }
        if (highlight >= 0) drawCircle(C.Violet, 10f, p(highlight), style = androidx.compose.ui.graphics.drawscope.Stroke(3f))
    }
}
