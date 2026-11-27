package com.superessence.bodymotion.ui.components

import android.view.Choreographer
import android.view.TextureView
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.foundation.gestures.detectTransformGestures
import androidx.compose.ui.unit.dp
import androidx.compose.foundation.layout.padding
import androidx.compose.ui.input.pointer.pointerInput
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

class ViewportController { var viewer: CharacterViewer? = null; var onReset: (() -> Unit)? = null; fun resetCamera() { viewer?.resetCamera() ?: onReset?.invoke() } }

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

    val compat = com.superessence.bodymotion.BodyMotionApp.store.settings.collectAsState().value.compatRender
    if (!com.superessence.bodymotion.BodyMotionApp.renderAvailable || compat) { SoftwareViewport(character, modifier, showBones, highlightNode, controller, poseAt); return }
    Box(modifier) {
        AndroidView(
            factory = { ctx ->
                TextureView(ctx).also { tv ->
                    com.superessence.bodymotion.data.CrashGuard.step("Viewport 3D : démarrage du moteur Filament")
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
                runCatching {
                    val fp = runCatching { currentPose.value(t) }.getOrNull()
                    v.applyPose(fp?.pose)
                    fp?.morphs?.let { v.applyMorphs(it) }
                    v.render(t)
                    if (showB.value && (frameCount++ % 2 == 0)) joints = v.projectJoints(fp?.pose, boneIdx)
                }.onFailure { android.util.Log.w("Viewport", "image", it) }
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

/**
 * Viewport « compatible » : rendu 3D logiciel (Canvas), sans moteur natif. Utilisé si le moteur Filament
 * n'est pas disponible ou a planté sur l'appareil. Orbite (1 doigt), zoom (2 doigts), squelette complet.
 */
@Composable
private fun SoftwareViewport(ch: LoadedCharacter, modifier: Modifier, showBones: Boolean, highlight: Int, controller: ViewportController?, poseAt: (Long) -> FramePose?) {
    val sk = ch.skeleton
    val h = remember(ch) { sk.restHeight() }
    val minY = remember(ch) { sk.nodes.filter { it.isJoint }.minOfOrNull { sk.restPos(it.index).y } ?: 0.0 }
    val fwd = remember(ch) { runCatching { Retargeter(sk, ch.boneMap).restForward }.getOrDefault(com.superessence.bodymotion.math.Vec3.Z) }
    val yaw0 = remember(ch) { kotlin.math.atan2(fwd.x, fwd.z) + 0.45 }
    var yaw by remember { mutableDoubleStateOf(yaw0) }
    var pitch by remember { mutableDoubleStateOf(0.18) }
    var dist by remember { mutableDoubleStateOf(1.75) }
    LaunchedEffect(controller) { controller?.onReset = { yaw = yaw0; pitch = 0.18; dist = 1.75 } }
    var t by remember { mutableLongStateOf(0L) }
    LaunchedEffect(Unit) { while (true) androidx.compose.runtime.withFrameNanos { t = it } }
    val humanPairs = remember(ch) { bonePairs(ch).toSet() }
    val allPairs = remember(ch) { sk.nodes.filter { it.isJoint && it.parent >= 0 && sk.nodes[it.parent].isJoint }.map { it.parent to it.index } }
    val headNode = ch.boneMap[HumanBone.Head]
    val fp = poseAt(t)
    val g = if (fp?.pose != null) com.superessence.bodymotion.retarget.PoseEvaluator.globals(sk, fp.pose) else sk.restGlobal
    Box(modifier.pointerInput(ch) {
        detectTransformGestures { _, pan, zoom, _ ->
            yaw -= pan.x * 0.008; pitch = (pitch + pan.y * 0.006).coerceIn(-0.4, 1.3); dist = (dist / zoom).coerceIn(0.8, 8.0)
        }
    }) {
        Canvas(Modifier.fillMaxSize()) {
            drawRect(androidx.compose.ui.graphics.Brush.verticalGradient(listOf(Color(0xFF1A202B), Color(0xFF0A0B0F))))
            // Caméra orbitale (unités : hauteur du personnage = 1)
            val f = com.superessence.bodymotion.math.Vec3(kotlin.math.sin(yaw) * kotlin.math.cos(pitch), kotlin.math.sin(pitch), kotlin.math.cos(yaw) * kotlin.math.cos(pitch))
            val target = com.superessence.bodymotion.math.Vec3(0.0, 0.5, 0.0)
            val cam = target + f * dist
            val right = (com.superessence.bodymotion.math.Vec3.Y cross f).normalized(); val up = f cross right
            val focal = size.height * 1.25f
            val cx = size.width / 2; val cy = size.height / 2
            val ox = g[ch.boneMap[HumanBone.Hips] ?: 0].translation().let { if (fp?.pose != null) 0.0 else 0.0 }
            fun norm(w: com.superessence.bodymotion.math.Vec3) = com.superessence.bodymotion.math.Vec3(w.x / h, (w.y - minY) / h, w.z / h)
            fun proj(p: com.superessence.bodymotion.math.Vec3): Triple<Float, Float, Float>? {
                val q = p - cam; val z = -(q dot f); if (z < 0.05) return null
                return Triple(cx + ((q dot right) / z * focal).toFloat(), cy - ((q dot up) / z * focal).toFloat(), z.toFloat())
            }
            // Sol en perspective
            for (k in -8..8) {
                val a = proj(com.superessence.bodymotion.math.Vec3(k * 0.25, 0.0, -2.0)); val b = proj(com.superessence.bodymotion.math.Vec3(k * 0.25, 0.0, 2.0))
                if (a != null && b != null) drawLine(if (k == 0) Color(0xFF2C3442) else Color(0xFF1C222C), Offset(a.first, a.second), Offset(b.first, b.second), 1.2f)
                val c = proj(com.superessence.bodymotion.math.Vec3(-2.0, 0.0, k * 0.25)); val d = proj(com.superessence.bodymotion.math.Vec3(2.0, 0.0, k * 0.25))
                if (c != null && d != null) drawLine(if (k == 0) Color(0xFF2C3442) else Color(0xFF1C222C), Offset(c.first, c.second), Offset(d.first, d.second), 1.2f)
            }
            // Ombre
            ch.boneMap[HumanBone.Hips]?.let { hi ->
                val p = norm(g[hi].translation()); proj(com.superessence.bodymotion.math.Vec3(p.x, 0.0, p.z))?.let { s0 ->
                    val r = (0.16 / s0.third * focal).toFloat()
                    drawOval(Color(0x77000000), Offset(s0.first - r, s0.second - r * 0.3f), androidx.compose.ui.geometry.Size(2 * r, 0.6f * r))
                }
            }
            // Os (triés du plus loin au plus proche), épaisseur selon la perspective
            data class Seg(val a: Triple<Float, Float, Float>, val b: Triple<Float, Float, Float>, val w: Float, val human: Boolean)
            val segs = allPairs.mapNotNull { (pa, pb) ->
                val a = proj(norm(g[pa].translation())) ?: return@mapNotNull null
                val b = proj(norm(g[pb].translation())) ?: return@mapNotNull null
                val human = (pa to pb) in humanPairs
                Seg(a, b, if (human) 0.045f else 0.012f, human)
            }.sortedByDescending { (it.a.third + it.b.third) / 2 }
            for (s0 in segs) {
                val z = (s0.a.third + s0.b.third) / 2
                val px = (s0.w / z * focal).coerceAtLeast(2f)
                val shade = (1.15f - z / (dist.toFloat() + 0.6f)).coerceIn(0.45f, 1f)
                drawLine(Color(0xFF0B0D12), Offset(s0.a.first, s0.a.second), Offset(s0.b.first, s0.b.second), px + 3f, StrokeCap.Round)
                drawLine(Color(0.62f * shade, 0.67f * shade, 0.74f * shade), Offset(s0.a.first, s0.a.second), Offset(s0.b.first, s0.b.second), px, StrokeCap.Round)
                drawLine(Color(0.85f * shade, 0.9f * shade, 0.96f * shade), Offset(s0.a.first, s0.a.second), Offset(s0.b.first, s0.b.second), px * 0.35f, StrokeCap.Round)
            }
            headNode?.let { hn -> proj(norm(g[hn].translation()) + com.superessence.bodymotion.math.Vec3(0.0, 0.055, 0.0))?.let { s0 ->
                val r = (0.065 / s0.third * focal).toFloat()
                drawCircle(Color(0xFFAEB8C6), r, Offset(s0.first, s0.second)); drawCircle(Color(0xFFDCE3EC), r * 0.55f, Offset(s0.first - r * 0.2f, s0.second - r * 0.25f))
            } }
            if (showBones) {
                for (s0 in segs) if (s0.human) drawLine(C.Cyan, Offset(s0.a.first, s0.a.second), Offset(s0.b.first, s0.b.second), 2.5f, StrokeCap.Round)
                for (n in humanPairs.flatMap { listOf(it.first, it.second) }.toSet()) proj(norm(g[n].translation()))?.let { s0 ->
                    val hl = n == highlight
                    drawCircle(Color(0xFF07080B), if (hl) 9f else 5f, Offset(s0.first, s0.second))
                    drawCircle(if (hl) C.Violet else Color(0xFFE9FBFF), if (hl) 9f else 5f, Offset(s0.first, s0.second), style = androidx.compose.ui.graphics.drawscope.Stroke(2.5f))
                }
            }
            if (ox.isNaN()) Unit
        }
        androidx.compose.material3.Text("AFFICHAGE COMPATIBLE", style = com.superessence.bodymotion.ui.theme.T.label.copy(fontSize = androidx.compose.ui.unit.TextUnit(9f, androidx.compose.ui.unit.TextUnitType.Sp)),
            modifier = Modifier.align(androidx.compose.ui.Alignment.TopEnd).padding(10.dp))
    }
}
