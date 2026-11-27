package com.superessence.bodymotion.ui.screens

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.Redo
import androidx.compose.material.icons.automirrored.outlined.Undo
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.dp
import androidx.navigation.NavHostController
import com.superessence.bodymotion.BodyMotionApp
import com.superessence.bodymotion.anim.AnimationClip
import com.superessence.bodymotion.anim.ClipOps
import com.superessence.bodymotion.data.LoadedCharacter
import com.superessence.bodymotion.math.Quat
import com.superessence.bodymotion.math.Vec3
import com.superessence.bodymotion.retarget.SkeletalPose
import com.superessence.bodymotion.rig.BodyGroup
import com.superessence.bodymotion.rig.HumanBone
import com.superessence.bodymotion.ui.Routes
import com.superessence.bodymotion.ui.components.*
import com.superessence.bodymotion.ui.go
import com.superessence.bodymotion.ui.theme.C
import com.superessence.bodymotion.ui.theme.T
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlin.math.abs
import kotlin.math.roundToInt

private enum class ToolTab(val label: String) { KEYS("Clés"), JOINT("Articulation"), CLIP("Clip") }

@Composable
fun EditorScreen(nav: NavHostController, clipId: String) {
    val store = BodyMotionApp.store
    val settings by store.settings.collectAsState()
    val loaded by produceState<Pair<AnimationClip, LoadedCharacter>?>(null, clipId) {
        value = withContext(Dispatchers.Default) { store.loadClip(clipId)?.let { c -> store.load(c.characterId)?.let { c to it } } }
    }
    val data = loaded ?: run { Box(Modifier.fillMaxSize().background(C.Bg), contentAlignment = Alignment.Center) { CircularProgressIndicator(color = C.Cyan) }; return }
    val ch = data.second
    var clip by remember(data) { mutableStateOf(data.first) }
    var version by remember { mutableIntStateOf(0) }
    val undo = remember { ArrayDeque<AnimationClip>() }
    val redo = remember { ArrayDeque<AnimationClip>() }

    var time by remember { mutableDoubleStateOf(0.0) }
    var playing by remember { mutableStateOf(false) }
    var speed by remember { mutableDoubleStateOf(1.0) }
    var loopPlay by remember { mutableStateOf(true) }
    var group by remember { mutableStateOf(BodyGroup.LeftArm) }
    var selKey by remember { mutableStateOf<Int?>(null) }
    var rangeIn by remember { mutableStateOf<Int?>(null) }
    var rangeOut by remember { mutableStateOf<Int?>(null) }
    var tab by remember { mutableStateOf(ToolTab.KEYS) }
    var showBones by remember { mutableStateOf(true) }
    var message by remember { mutableStateOf<String?>(null) }
    var mergeDialog by remember { mutableStateOf(false) }
    var renameDialog by remember { mutableStateOf(false) }
    // Édition d'articulation (aperçu en direct avant validation)
    var bone by remember { mutableStateOf<HumanBone?>(null) }
    var rx by remember { mutableFloatStateOf(0f) }; var ry by remember { mutableFloatStateOf(0f) }; var rz by remember { mutableFloatStateOf(0f) }
    var falloff by remember { mutableFloatStateOf(8f) }
    val controller = remember { ViewportController() }

    val frame = (time * clip.fps).roundToInt().coerceIn(0, maxOf(0, clip.frameCount - 1))
    val range = if (rangeIn != null && rangeOut != null) minOf(rangeIn!!, rangeOut!!)..maxOf(rangeIn!!, rangeOut!!) else null

    fun commit(label: String, op: (AnimationClip) -> Unit) {
        undo.addLast(clip.deepCopy()); if (undo.size > 30) undo.removeFirst(); redo.clear()
        op(clip); version++; store.saveClip(clip); message = label
    }
    fun replaceClip(newClip: AnimationClip, label: String) {
        undo.addLast(clip.deepCopy()); redo.clear()
        newClip.id = clip.id; newClip.name = clip.name; newClip.category = clip.category
        clip = newClip; version++; store.saveClip(clip); message = label
        time = time.coerceAtMost(clip.duration)
    }
    val bonesInGroup = remember(group) { HumanBone.entries.filter { it.group == group && ch.boneMap[it] != null } }
    LaunchedEffect(group) { bone = bonesInGroup.firstOrNull(); rx = 0f; ry = 0f; rz = 0f }

    // Lecture
    LaunchedEffect(playing, speed, loopPlay, version) {
        if (!playing) return@LaunchedEffect
        var last = withFrameNanos { it }
        while (playing) {
            withFrameNanos { now ->
                time += (now - last) / 1e9 * speed; last = now
                if (time > clip.duration) { if (loopPlay) time %= maxOf(clip.duration, 1e-3) else { time = clip.duration; playing = false } }
            }
        }
    }

    fun previewPose(): SkeletalPose {
        val p = if (playing) clip.poseAtTime(time, ch.skeleton) else clip.poseAt(frame, ch.skeleton)
        val b = bone
        if (!playing && tab == ToolTab.JOINT && b != null && (rx != 0f || ry != 0f || rz != 0f)) {
            if (group != BodyGroup.Root) ch.boneMap[b]?.let { n -> p.local[n] = (p.local[n] * Quat.fromEulerXYZDegrees(Vec3(rx.toDouble(), ry.toDouble(), rz.toDouble()))).normalized() }
        }
        if (!playing && tab == ToolTab.JOINT && group == BodyGroup.Root) p.hipsLocalT = p.hipsLocalT + rootDelta(ch, clip, rx, ry, rz)
        return p
    }
    val poseState = rememberUpdatedState({ FramePose(previewPose(), clip.morphAt(if (playing) (time * clip.fps).toInt() else frame)?.let { clipMorphMap(ch, clip, it) }) })

    BoxWithConstraints(Modifier.fillMaxSize().background(C.Bg)) {
        val landscape = maxWidth > maxHeight
        Column(Modifier.fillMaxSize()) {
            TopBar(clip.name, onBack = { nav.popBackStack() }, subtitle = "${ch.info.name} · ${clip.fps.toInt()} FPS · ${"%.2f".format(clip.duration)} s", actions = {
                IconButton({ undo.removeLastOrNull()?.let { redo.addLast(clip.deepCopy()); clip = it; version++; store.saveClip(clip); message = "Annulé" } }, enabled = undo.isNotEmpty()) { Icon(Icons.AutoMirrored.Outlined.Undo, "Annuler", tint = if (undo.isNotEmpty()) C.Text2 else C.Faint) }
                IconButton({ redo.removeLastOrNull()?.let { undo.addLast(clip.deepCopy()); clip = it; version++; store.saveClip(clip); message = "Rétabli" } }, enabled = redo.isNotEmpty()) { Icon(Icons.AutoMirrored.Outlined.Redo, "Rétablir", tint = if (redo.isNotEmpty()) C.Text2 else C.Faint) }
                TextButton({ nav.go(Routes.export(listOf(clip.id))) }) { Text("Exporter", color = C.Cyan) }
            })
            val viewport: @Composable (Modifier) -> Unit = { m ->
                Box(m) {
                    Viewport3D(ch, settings.quality, Modifier.fillMaxSize(), showBones = showBones, highlightNode = bone?.let { ch.boneMap[it] } ?: -1, controller = controller) { poseState.value() }
                    Row(Modifier.padding(8.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        FilterChip(showBones, { showBones = !showBones }, label = { Text("Os") }, shape = Shape4)
                        FilterChip(false, { controller.resetCamera() }, label = { Text("Recentrer") }, shape = Shape4)
                    }
                    if (clip.interpolatedFrames.contains(frame)) Text("Image reconstruite (suivi perdu)", style = T.small.copy(color = C.Warn), modifier = Modifier.align(Alignment.BottomStart).padding(8.dp).background(Color(0xB307080B)).padding(4.dp))
                }
            }
            val tools: @Composable (Modifier) -> Unit = { m ->
                Column(m.background(C.Panel)) {
                    TabRow(tab.ordinal, containerColor = C.Panel, contentColor = C.Text, divider = { HorizontalDivider(color = C.Border) }) {
                        ToolTab.entries.forEach { t -> Tab(tab == t, { tab = t }, text = { Text(t.label, style = T.small.copy(color = if (tab == t) C.Text else C.Muted)) }) }
                    }
                    Column(Modifier.verticalScroll(rememberScrollState()).padding(12.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                        when (tab) {
                            ToolTab.KEYS -> KeysPanel(group, frame, selKey, range, clip,
                                onAdd = { commit("Clé ajoutée") { ClipOps.addKey(it, group, frame) } },
                                onDeleteKey = { k -> commit("Clé supprimée, piste ré-interpolée") { ClipOps.deleteKey(it, ch.boneMap, group, k) }; selKey = null },
                                onDeleteRange = { r -> commit("Clés supprimées") { ClipOps.deleteKeysInRange(it, ch.boneMap, group, r.first, r.last) } },
                                onSmooth = { all -> val r = range ?: 0..(clip.frameCount - 1); commit("Lissage appliqué") { ClipOps.smooth(it, ch.boneMap, if (all) null else group, r.first, r.last, 2.0) } },
                                onIn = { rangeIn = frame }, onOut = { rangeOut = frame }, onClearRange = { rangeIn = null; rangeOut = null })
                            ToolTab.JOINT -> JointPanel(ch, group, bonesInGroup, bone, rx, ry, rz, falloff,
                                onBone = { bone = it; rx = 0f; ry = 0f; rz = 0f }, onRx = { rx = it }, onRy = { ry = it }, onRz = { rz = it }, onFalloff = { falloff = it },
                                onApply = {
                                    val b = bone
                                    if (group == BodyGroup.Root) { val d = rootDelta(ch, clip, rx, ry, rz); commit("Déplacement appliqué") { ClipOps.translateRoot(it, frame, d, falloff.toInt()) } }
                                    else if (b != null) ch.boneMap[b]?.let { n -> commit("Rotation appliquée") { ClipOps.rotateBone(it, ch.boneMap, n, frame, Quat.fromEulerXYZDegrees(Vec3(rx.toDouble(), ry.toDouble(), rz.toDouble())), falloff.toInt()) } }
                                    rx = 0f; ry = 0f; rz = 0f
                                }, onReset = { rx = 0f; ry = 0f; rz = 0f })
                            ToolTab.CLIP -> ClipPanel(clip, range, frame,
                                onTrim = { r -> val c = ClipOps.trim(clip, r.first, r.last, clip.name + " (coupé)"); store.saveClip(c); nav.navigate(Routes.editor(c.id)) { popUpTo(Routes.editor(clip.id)) { inclusive = true } } },
                                onSplit = { val (a, b) = ClipOps.split(clip, frame); store.saveClip(a); store.saveClip(b); message = "Deux nouvelles animations créées : « ${a.name} » et « ${b.name} »" },
                                onDuplicate = { store.duplicateClip(clip.id); message = "Copie ajoutée à la bibliothèque" },
                                onMerge = { mergeDialog = true },
                                onLoop = { commit("Animation rendue bouclable") { ClipOps.makeLoop(it, (clip.fps * 0.3).toInt().coerceAtLeast(2)) } },
                                onSpeed = { s -> replaceClip(ClipOps.changeSpeed(clip, s), "Vitesse ×$s appliquée") },
                                onInPlace = { commit("Déplacement horizontal supprimé") { removeRootMotion(it, ch) } },
                                onRename = { renameDialog = true })
                        }
                    }
                }
            }
            if (landscape) {
                Row(Modifier.weight(1f)) {
                    viewport(Modifier.weight(1f).fillMaxHeight())
                    tools(Modifier.width(300.dp).fillMaxHeight())
                }
            } else {
                viewport(Modifier.fillMaxWidth().weight(1f))
            }
            Transport(playing, speed, loopPlay, frame, clip.frameCount, time,
                onPlay = { if (!playing && time >= clip.duration - 1e-6) time = 0.0; playing = !playing },
                onPrev = { playing = false; time = (frame - 1).coerceAtLeast(0) / clip.fps },
                onNext = { playing = false; time = (frame + 1).coerceAtMost(clip.frameCount - 1) / clip.fps },
                onStart = { playing = false; time = 0.0 },
                onSpeed = { speed = when (speed) { 0.25 -> 0.5; 0.5 -> 1.0; 1.0 -> 2.0; else -> 0.25 } },
                onLoop = { loopPlay = !loopPlay })
            key(version) {
                Timeline(clip, ch, frame, group, selKey, range,
                    onScrub = { f -> playing = false; time = f / clip.fps },
                    onSelect = { g, k -> group = g; selKey = k; if (k != null) { playing = false; time = k / clip.fps } })
            }
            if (!landscape) tools(Modifier.fillMaxWidth().height(250.dp).navigationBarsPadding())
        }
        message?.let { msg ->
            LaunchedEffect(msg) { kotlinx.coroutines.delay(2200); message = null }
            Text(msg, style = T.small.copy(color = C.Text), modifier = Modifier.align(Alignment.TopCenter).padding(top = 100.dp).background(C.Panel2, Shape6).border(1.dp, C.Border2, Shape6).padding(10.dp))
        }
    }

    if (mergeDialog) {
        val others = store.clips.collectAsState().value.filter { it.characterId == clip.characterId && it.id != clip.id }
        AlertDialog(onDismissRequest = { mergeDialog = false }, containerColor = C.Panel, title = { Text("Fusionner à la suite", style = T.h2) }, confirmButton = {},
            dismissButton = { TextButton({ mergeDialog = false }) { Text("Annuler") } },
            text = {
                Column(Modifier.verticalScroll(rememberScrollState())) {
                    if (others.isEmpty()) Text("Aucune autre animation de ce personnage.", style = T.small)
                    others.forEach { o -> ListRow(o.name, subtitle = "%.2f s".format(o.duration), onClick = {
                        mergeDialog = false
                        store.loadClip(o.id)?.let { b ->
                            val m = ClipOps.merge(clip, b, (clip.fps * 0.25).toInt(), clip.name + " + " + b.name)
                            store.saveClip(m); nav.navigate(Routes.editor(m.id)) { popUpTo(Routes.editor(clip.id)) { inclusive = true } }
                        }
                    }) }
                }
            })
    }
    if (renameDialog) TextInputDialog("Renommer", clip.name, { renameDialog = false }) { n -> commit("Renommée") { it.name = n }; renameDialog = false }
}

/** Décalage du bassin exprimé dans le repère du parent (sliders en centimètres). */
private fun rootDelta(ch: LoadedCharacter, clip: AnimationClip, x: Float, y: Float, z: Float): Vec3 {
    val p = ch.skeleton.nodes[clip.hipsNode].parent
    val scale = ch.skeleton.restHeight() / 1.75
    val worldD = Vec3(x.toDouble(), y.toDouble(), z.toDouble()) * (0.01 * scale)
    return if (p >= 0) ch.skeleton.restGlobal[p].inverse().transformDir(worldD) else worldD
}

/** Garde le personnage sur place : supprime la translation horizontale du bassin (en repère monde). */
fun removeRootMotion(clip: AnimationClip, ch: LoadedCharacter) {
    val p = ch.skeleton.nodes[clip.hipsNode].parent
    val m = if (p >= 0) ch.skeleton.restGlobal[p] else com.superessence.bodymotion.math.Mat4()
    val inv = m.inverse()
    val w0 = m.transformPoint(clip.hipsT[0])
    clip.hipsT = Array(clip.frameCount) { val w = m.transformPoint(clip.hipsT[it]); inv.transformPoint(Vec3(w0.x, w.y, w0.z)) }
}

fun clipMorphMap(ch: LoadedCharacter, clip: AnimationClip, w: FloatArray): Map<Int, FloatArray> {
    val out = HashMap<Int, FloatArray>()
    clip.morphChannels.forEachIndexed { i, c ->
        val n = ch.skeleton.morphTargets.firstOrNull { it.nodeIndex == c.node }?.targetNames?.size ?: return@forEachIndexed
        val arr = out.getOrPut(c.node) { FloatArray(n) }
        if (c.target < n && i < w.size) arr[c.target] = maxOf(arr[c.target], w[i])
    }
    return out
}

@Composable
private fun Transport(playing: Boolean, speed: Double, loop: Boolean, frame: Int, count: Int, time: Double,
                      onPlay: () -> Unit, onPrev: () -> Unit, onNext: () -> Unit, onStart: () -> Unit, onSpeed: () -> Unit, onLoop: () -> Unit) {
    Row(Modifier.fillMaxWidth().background(C.Bar).padding(horizontal = 8.dp, vertical = 4.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(2.dp)) {
        IconButton(onStart) { Icon(Icons.Outlined.SkipPrevious, "Début", tint = C.Text2) }
        IconButton(onPrev) { Icon(Icons.Outlined.ChevronLeft, "Image précédente", tint = C.Text2) }
        FilledIconButton(onPlay, shape = Shape6, colors = IconButtonDefaults.filledIconButtonColors(containerColor = C.Cyan, contentColor = C.Black)) {
            Icon(if (playing) Icons.Outlined.Pause else Icons.Outlined.PlayArrow, if (playing) "Pause" else "Lecture")
        }
        IconButton(onNext) { Icon(Icons.Outlined.ChevronRight, "Image suivante", tint = C.Text2) }
        Spacer(Modifier.width(6.dp))
        Text("F %03d / %03d".format(frame, maxOf(0, count - 1)), style = T.mono.copy(color = C.Text))
        Spacer(Modifier.weight(1f))
        TextButton(onSpeed) { Text("×${if (speed < 1) speed.toString().replace('.', ',') else speed.toInt()}", style = T.mono) }
        IconButton(onLoop) { Icon(Icons.Outlined.Repeat, "Boucle", tint = if (loop) C.Cyan else C.Faint) }
    }
}

/** Timeline multi-pistes : clés (losanges), images reconstruites (orange), plage sélectionnée, tête de lecture. */
@Composable
private fun Timeline(clip: AnimationClip, ch: LoadedCharacter, frame: Int, group: BodyGroup, selKey: Int?, range: IntRange?,
                     onScrub: (Int) -> Unit, onSelect: (BodyGroup, Int?) -> Unit) {
    val groups = BodyGroup.entries
    val rowH = 24.dp; val labelW = 86.dp; val rulerH = 18.dp
    var zoom by remember { mutableFloatStateOf(1f) }
    val density = LocalDensity.current
    Column(Modifier.fillMaxWidth().background(C.Black)) {
        HorizontalDivider(color = C.Border)
        BoxWithConstraints(Modifier.fillMaxWidth()) {
            val trackW = (maxWidth - labelW - 12.dp) * zoom
            val scroll = rememberScrollState()
            Row {
                Column(Modifier.width(labelW)) {
                    Box(Modifier.height(rulerH).fillMaxWidth(), contentAlignment = Alignment.CenterStart) {
                        Row { TextButton({ zoom = (zoom / 1.6f).coerceAtLeast(1f) }, contentPadding = PaddingValues(0.dp), modifier = Modifier.height(rulerH).width(30.dp)) { Text("−", style = T.mono) }
                            TextButton({ zoom = (zoom * 1.6f).coerceAtMost(12f) }, contentPadding = PaddingValues(0.dp), modifier = Modifier.height(rulerH).width(30.dp)) { Text("+", style = T.mono) } }
                    }
                    groups.forEach { g ->
                        val sel = g == group
                        Box(Modifier.height(rowH).fillMaxWidth().background(if (sel) C.Violet.copy(alpha = 0.10f) else Color.Transparent).clickable { onSelect(g, null) }.padding(start = 10.dp), contentAlignment = Alignment.CenterStart) {
                            Text(g.label, style = T.small.copy(color = if (sel) Color(0xFFDDD6FE) else C.Text2))
                        }
                    }
                }
                Box(Modifier.weight(1f).horizontalScroll(scroll).padding(end = 12.dp)) {
                    val n = maxOf(1, clip.frameCount - 1)
                    Canvas(Modifier.width(trackW).height(rulerH + rowH * groups.size)
                        .pointerInput(clip, zoom) {
                            detectTapGestures { pos ->
                                val f = (pos.x / size.width * n).roundToInt().coerceIn(0, n)
                                val rowIdx = ((pos.y - rulerH.toPx()) / rowH.toPx()).toInt()
                                if (rowIdx < 0) { onScrub(f); return@detectTapGestures }
                                val g = groups.getOrNull(rowIdx) ?: return@detectTapGestures
                                val keys = clip.keys[g] ?: emptySet()
                                val tol = 14.dp.toPx() / size.width * n
                                val near = keys.minByOrNull { abs(it - f) }?.takeIf { abs(it - f) <= tol }
                                onSelect(g, near); if (near == null) onScrub(f)
                            }
                        }
                        .pointerInput(clip, zoom) { detectHorizontalDragGestures { change, _ -> onScrub((change.position.x / size.width * n).roundToInt().coerceIn(0, n)) } }) {
                        val w = size.width
                        val rh = rowH.toPx(); val ruler = rulerH.toPx()
                        fun x(f: Int) = f.toFloat() / n * w
                        // Règle
                        val step = listOf(1, 2, 5, 10, 15, 30, 60, 120).first { x(it) - x(0) > 28f }
                        for (f in 0..n step step) {
                            drawLine(C.Border2, Offset(x(f), ruler * 0.55f), Offset(x(f), ruler), 1f)
                        }
                        // Images reconstruites
                        for (f in clip.interpolatedFrames) drawRect(C.Warn.copy(alpha = 0.55f), Offset(x(f) - 1f, 0f), Size(maxOf(2f, x(1)), ruler * 0.4f))
                        // Plage
                        range?.let { r -> drawRect(C.Cyan.copy(alpha = 0.10f), Offset(x(r.first), ruler), Size(x(r.last) - x(r.first), rh * groups.size)) }
                        groups.forEachIndexed { gi, g ->
                            val y0 = ruler + gi * rh
                            if (g == group) drawRect(C.Violet.copy(alpha = 0.08f), Offset(0f, y0), Size(w, rh))
                            drawLine(Color(0xFF14161C), Offset(0f, y0), Offset(w, y0), 1f)
                            drawLine(if (g == BodyGroup.Root) C.Cyan.copy(alpha = 0.3f) else C.Border2, Offset(0f, y0 + rh / 2), Offset(w, y0 + rh / 2), 2f)
                            val keys = clip.keys[g] ?: emptySet()
                            val col = when { g == group -> Color(0xFFC4B5FD); g == BodyGroup.Root -> C.Cyan; else -> Color(0xFF8C95A4) }
                            for (k in keys) {
                                val cx = x(k); val cy = y0 + rh / 2; val r = if (g == group && k == selKey) 7f else 5f
                                val path = Path().apply { moveTo(cx, cy - r); lineTo(cx + r, cy); lineTo(cx, cy + r); lineTo(cx - r, cy); close() }
                                drawPath(path, if (g == group && k == selKey) Color.White else col)
                            }
                        }
                        // Tête de lecture
                        drawLine(Color(0xFFF2F4F7), Offset(x(frame), 0f), Offset(x(frame), size.height), 2.5f)
                    }
                }
            }
            LaunchedEffect(frame, zoom) {
                // Garde la tête de lecture visible quand on zoome.
                val px = with(density) { (trackW.toPx() * frame / maxOf(1, clip.frameCount - 1)).toInt() }
                val viewW = with(density) { (maxWidth - labelW - 12.dp).toPx().toInt() }
                if (px < scroll.value || px > scroll.value + viewW) scroll.scrollTo((px - viewW / 2).coerceAtLeast(0))
            }
        }
    }
}

@Composable
private fun KeysPanel(group: BodyGroup, frame: Int, selKey: Int?, range: IntRange?, clip: AnimationClip,
                      onAdd: () -> Unit, onDeleteKey: (Int) -> Unit, onDeleteRange: (IntRange) -> Unit, onSmooth: (Boolean) -> Unit,
                      onIn: () -> Unit, onOut: () -> Unit, onClearRange: () -> Unit) {
    val keyHere = selKey ?: frame.takeIf { clip.keys[group]?.contains(it) == true }
    Text("Piste : ${group.label} · ${clip.keys[group]?.size ?: 0} clés", style = T.bodyM)
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        SecondaryButton("+ Clé", Modifier.weight(1f), onClick = onAdd)
        SecondaryButton("Supprimer la clé", Modifier.weight(1f), enabled = keyHere != null, onClick = { keyHere?.let(onDeleteKey) })
    }
    Text("Supprimer une clé ré-interpole la piste entre les clés voisines.", style = T.small)
    SectionLabel("Plage ${range?.let { "${it.first} → ${it.last}" } ?: "(aucune)"}")
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        SecondaryButton("Début [", Modifier.weight(1f), onClick = onIn)
        SecondaryButton("Fin ]", Modifier.weight(1f), onClick = onOut)
        SecondaryButton("Effacer", Modifier.weight(1f), enabled = range != null, onClick = onClearRange)
    }
    SecondaryButton("Supprimer les clés de la plage", Modifier.fillMaxWidth(), enabled = range != null) { range?.let(onDeleteRange) }
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        SecondaryButton("Lisser la piste", Modifier.weight(1f)) { onSmooth(false) }
        SecondaryButton("Lisser tout le corps", Modifier.weight(1f)) { onSmooth(true) }
    }
    Text("Le lissage s'applique à la plage si elle est définie, sinon à tout le clip.", style = T.small)
}

@Composable
private fun JointPanel(ch: LoadedCharacter, group: BodyGroup, bones: List<HumanBone>, bone: HumanBone?, rx: Float, ry: Float, rz: Float, falloff: Float,
                       onBone: (HumanBone) -> Unit, onRx: (Float) -> Unit, onRy: (Float) -> Unit, onRz: (Float) -> Unit, onFalloff: (Float) -> Unit,
                       onApply: () -> Unit, onReset: () -> Unit) {
    val isRoot = group == BodyGroup.Root
    if (!isRoot) {
        Text("Articulation (piste ${group.label})", style = T.small)
        Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            bones.forEach { b -> FilterChip(b == bone, { onBone(b) }, label = { Text(b.label) }, shape = Shape4) }
        }
        bone?.let { b -> Text(ch.boneMap[b]?.let { ch.skeleton.nodes[it].name } ?: "", style = T.label) }
    } else Text("Déplacement du bassin (root motion), en centimètres", style = T.small)
    val range = if (isRoot) -50f..50f else -90f..90f
    AxisSlider(if (isRoot) "X" else "X", rx, range, C.AxisX, onRx)
    AxisSlider("Y", ry, range, C.AxisY, onRy)
    AxisSlider("Z", rz, range, C.AxisZ, onRz)
    Text("Influence : ± ${falloff.toInt()} images (fondu)", style = T.small)
    Slider(falloff, onFalloff, valueRange = 0f..30f, steps = 29, colors = SliderDefaults.colors(thumbColor = C.Text, activeTrackColor = C.Violet))
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        PrimaryButton("Appliquer", Modifier.weight(1f), enabled = rx != 0f || ry != 0f || rz != 0f, onClick = onApply)
        SecondaryButton("Réinitialiser", Modifier.weight(1f), onClick = onReset)
    }
    Text("L'aperçu est immédiat ; « Appliquer » enregistre la correction et ajoute une clé.", style = T.small)
}

@Composable
private fun AxisSlider(label: String, v: Float, range: ClosedFloatingPointRange<Float>, color: Color, onChange: (Float) -> Unit) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Text(label, style = T.mono.copy(color = color), modifier = Modifier.width(18.dp))
        Slider(v, onChange, valueRange = range, modifier = Modifier.weight(1f), colors = SliderDefaults.colors(thumbColor = color, activeTrackColor = color, inactiveTrackColor = C.Border2))
        Text("%+.1f".format(v), style = T.mono, modifier = Modifier.width(52.dp))
    }
}

@Composable
private fun ClipPanel(clip: AnimationClip, range: IntRange?, frame: Int, onTrim: (IntRange) -> Unit, onSplit: () -> Unit, onDuplicate: () -> Unit,
                      onMerge: () -> Unit, onLoop: () -> Unit, onSpeed: (Double) -> Unit, onInPlace: () -> Unit, onRename: () -> Unit) {
    Group {
        ListRow("Couper à la plage", Icons.Outlined.ContentCut, range?.let { "Images ${it.first} → ${it.last} (nouvelle animation)" } ?: "Définissez une plage dans l'onglet Clés", onClick = if (range != null) ({ onTrim(range) }) else null)
        GroupDivider(); ListRow("Scinder à l'image $frame", Icons.Outlined.VerticalSplit, "Crée deux animations", onClick = if (frame in 1 until clip.frameCount - 1) onSplit else null)
        GroupDivider(); ListRow("Dupliquer", Icons.Outlined.ContentCopy, onClick = onDuplicate)
        GroupDivider(); ListRow("Fusionner à la suite…", Icons.Outlined.MergeType, "Enchaîne une autre animation (fondu)", onClick = onMerge)
        GroupDivider(); ListRow("Rendre bouclable", Icons.Outlined.Loop, if (clip.loop) "Déjà bouclée" else "Raccorde la fin au début", onClick = onLoop)
        GroupDivider(); ListRow("Garder sur place", Icons.Outlined.PinDrop, "Supprime le déplacement horizontal", onClick = onInPlace)
        GroupDivider(); ListRow("Renommer", Icons.Outlined.Edit, onClick = onRename)
    }
    SectionLabel("Vitesse de l'animation")
    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        listOf(0.5, 0.75, 1.25, 1.5, 2.0).forEach { s -> SecondaryButton("×${s.toString().replace('.', ',').removeSuffix(",0")}", Modifier.weight(1f)) { onSpeed(s) } }
    }
}
