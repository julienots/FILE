package com.superessence.bodymotion.ui.screens

import android.Manifest
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.Matrix
import android.os.SystemClock
import android.util.Size
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.Preview
import androidx.camera.core.resolutionselector.ResolutionSelector
import androidx.camera.core.resolutionselector.ResolutionStrategy
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.camera.view.PreviewView
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalLifecycleOwner
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.content.ContextCompat
import androidx.navigation.NavHostController
import com.superessence.bodymotion.BodyMotionApp
import com.superessence.bodymotion.anim.CLIP_CATEGORIES
import com.superessence.bodymotion.data.LoadedCharacter
import com.superessence.bodymotion.mocap.BodyTracker
import com.superessence.bodymotion.mocap.ClipBuilder
import com.superessence.bodymotion.mocap.ImagePoint
import com.superessence.bodymotion.mocap.PL
import com.superessence.bodymotion.mocap.PoseFrame
import com.superessence.bodymotion.mocap.VideoProcessor
import com.superessence.bodymotion.retarget.Calibration
import com.superessence.bodymotion.retarget.RetargetOptions
import com.superessence.bodymotion.retarget.Retargeter
import com.superessence.bodymotion.retarget.SkeletalPose
import com.superessence.bodymotion.ui.Routes
import com.superessence.bodymotion.ui.components.*
import com.superessence.bodymotion.ui.theme.C
import com.superessence.bodymotion.ui.theme.Mono
import com.superessence.bodymotion.ui.theme.T
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.asCoroutineDispatcher
import java.util.concurrent.Executors
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine

private class TrackerHolder { @Volatile var tracker: BodyTracker? = null; @Volatile var smoothing = -1; @Volatile var delegate = ""; @Volatile var gpu = false; var failed = 0 }

private enum class Phase { IDLE, CALIBRATING, COUNTDOWN, RECORDING, PROCESSING, VIDEO }

/** État partagé entre le thread d'analyse et l'interface. */
private class LiveState {
    @Volatile var frame: PoseFrame? = null
    @Volatile var pose: SkeletalPose? = null
    @Volatile var morphs: Map<Int, FloatArray>? = null
    @Volatile var recording = false
    val recorded = ArrayList<PoseFrame>()
    @Volatile var fps = 0f
    private var count = 0; private var since = 0L
    fun tick() { count++; val now = SystemClock.uptimeMillis(); if (since == 0L) since = now; if (now - since >= 1000) { fps = count * 1000f / (now - since); count = 0; since = now } }
}

@Composable
fun CaptureScreen(nav: NavHostController, characterId: String, startWithVideo: Boolean) {
    val store = BodyMotionApp.store
    val ctx = LocalContext.current
    val settings by store.settings.collectAsState()
    var ch by remember { mutableStateOf<LoadedCharacter?>(null) }
    LaunchedEffect(characterId) { ch = withContext(Dispatchers.Default) { store.load(characterId) } }
    val character = ch ?: run { Box(Modifier.fillMaxSize().background(Color.Black), contentAlignment = Alignment.Center) { CircularProgressIndicator(color = C.Cyan) }; return }
    if (!character.boneMap.isUsable) {
        Column(Modifier.fillMaxSize().background(C.Bg)) {
            TopBar("Capture", { nav.popBackStack() })
            Box(Modifier.padding(16.dp)) { InfoBanner("Ce personnage n'est pas prêt : configurez ses os humanoïdes dans sa fiche.") }
        }
        return
    }

    val rt = remember(character) { Retargeter(character.skeleton, character.boneMap) }
    val live = remember { LiveState() }
    var phase by remember { mutableStateOf(if (startWithVideo) Phase.VIDEO else Phase.IDLE) }
    var hands by remember { mutableStateOf(false) }
    val faceAvailable = !character.face.isEmpty
    var face by remember { mutableStateOf(false) }
    var rootMotion by remember { mutableStateOf(true) }
    var front by remember { mutableStateOf(true) }
    var calib by remember { mutableStateOf(Calibration()) }
    var countdown by remember { mutableIntStateOf(0) }
    var recStart by remember { mutableLongStateOf(0L) }
    var tick by remember { mutableLongStateOf(0L) }
    var showOptions by remember { mutableStateOf(false) }
    var result by remember { mutableStateOf<ClipBuilder.Report?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    val scope = rememberCoroutineScope()
    val opt = RetargetOptions(rootMotion = rootMotion, keepModelFacing = settings.keepModelFacing, fingers = hands)
    val optState = rememberUpdatedState(opt)
    val calibState = rememberUpdatedState(calib)

    var hasPermission by remember { mutableStateOf(ContextCompat.checkSelfPermission(ctx, Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED) }
    val permLauncher = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { hasPermission = it }
    LaunchedEffect(Unit) { if (!hasPermission && !startWithVideo) permLauncher.launch(Manifest.permission.CAMERA) }

    // Détecteur : créé et utilisé sur le thread d'analyse (exigence du délégué GPU), recréé si le lissage change.
    val executor = remember { Executors.newSingleThreadExecutor() }
    val holder = remember { TrackerHolder() }
    val handsState = rememberUpdatedState(hands); val faceState = rememberUpdatedState(face); val smoothState = rememberUpdatedState(settings.smoothing); val gpuState = rememberUpdatedState(settings.gpu)
    DisposableEffect(Unit) { onDispose { executor.execute { holder.tracker?.close(); holder.tracker = null }; executor.shutdown() } }
    fun trackerOnAnalysisThread(): BodyTracker? {
        val cur = holder.tracker
        if (cur != null && holder.smoothing == smoothState.value && holder.gpu == gpuState.value) { cur.hands = handsState.value; cur.face = faceState.value; return cur }
        cur?.close()
        if (holder.failed >= 3) return null
        return runCatching { BodyTracker(ctx, handsState.value, faceState.value, smoothState.value, forceCpu = !gpuState.value) }
            .onFailure { if (holder.failed++ == 0) error = "Initialisation de la détection impossible : ${it.message}" }
            .getOrNull().also { holder.tracker = it; holder.smoothing = smoothState.value; holder.gpu = gpuState.value; holder.delegate = it?.delegateName ?: "" }
    }

    // Rafraîchissement de l'interface (~15 Hz)
    LaunchedEffect(Unit) { while (true) { delay(66); tick = SystemClock.uptimeMillis() } }

    fun onFrame(f: PoseFrame) {
        live.tick()
        live.frame = f
        if (f.hasBody) {
            runCatching { rt.solve(f, calibState.value, optState.value) }.getOrNull()?.let { live.pose = it }
            f.faceBlendshapes?.let { bs -> if (faceAvailable) { val w = character.face.evaluate(bs); live.morphs = morphMap(character, w) } }
        }
        if (live.recording) synchronized(live.recorded) { live.recorded.add(f) }
    }

    // ---------------------------------------------------------------- caméra
    val lifecycle = LocalLifecycleOwner.current
    val previewView = remember { PreviewView(ctx).apply { implementationMode = PreviewView.ImplementationMode.COMPATIBLE; scaleType = PreviewView.ScaleType.FILL_CENTER } }
    if (hasPermission && phase != Phase.VIDEO) {
        LaunchedEffect(front) {
            val provider = suspendCoroutine { c -> val f = ProcessCameraProvider.getInstance(ctx); f.addListener({ c.resume(f.get()) }, ContextCompat.getMainExecutor(ctx)) }
            val preview = Preview.Builder().build().also { it.surfaceProvider = previewView.surfaceProvider }
            val analysis = ImageAnalysis.Builder()
                .setResolutionSelector(ResolutionSelector.Builder().setResolutionStrategy(ResolutionStrategy(Size(640, 480), ResolutionStrategy.FALLBACK_RULE_CLOSEST_HIGHER_THEN_LOWER)).build())
                .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                .setOutputImageFormat(ImageAnalysis.OUTPUT_IMAGE_FORMAT_RGBA_8888).build()
            analysis.setAnalyzer(executor) { proxy ->
                try {
                    val bmp = proxy.toBitmap()
                    val rot = proxy.imageInfo.rotationDegrees
                    val upright = if (rot == 0) bmp else Bitmap.createBitmap(bmp, 0, 0, bmp.width, bmp.height, Matrix().apply { postRotate(rot.toFloat()) }, true)
                    trackerOnAnalysisThread()?.let { tr -> onFrame(tr.process(upright, SystemClock.uptimeMillis())); holder.delegate = tr.delegateName }
                } catch (e: Throwable) { android.util.Log.w("Capture", "analyse", e) } finally { proxy.close() }
            }
            provider.unbindAll()
            val selector = if (front) CameraSelector.DEFAULT_FRONT_CAMERA else CameraSelector.DEFAULT_BACK_CAMERA
            runCatching { provider.bindToLifecycle(lifecycle, selector, preview, analysis) }.onFailure { error = "Caméra indisponible : ${it.message}" }
        }
    }
    DisposableEffect(Unit) { onDispose { runCatching { ProcessCameraProvider.getInstance(ctx).get().unbindAll() } } }

    // ---------------------------------------------------------------- calibration / compte à rebours / enregistrement
    fun startRecording() {
        synchronized(live.recorded) { live.recorded.clear() }
        recStart = SystemClock.uptimeMillis(); live.recording = true; phase = Phase.RECORDING
    }
    fun stopRecording() {
        live.recording = false; phase = Phase.PROCESSING
        val frames = synchronized(live.recorded) { live.recorded.toList() }
        scope.launch {
            com.superessence.bodymotion.data.CrashGuard.step("Capture : traitement de l'enregistrement (${frames.size} images)")
            val rep = runCatching {
                withContext(Dispatchers.Default) {
                    ClipBuilder.build(frames, Retargeter(character.skeleton, character.boneMap), calib, opt, if (face && faceAvailable) character.face else null, settings.captureFps.toDouble(), "Capture ${store.clips.value.size + 1}", character.info.id)
                }
            }
            rep.onFailure { error = "Traitement impossible : ${it.message ?: it.javaClass.simpleName}"; phase = Phase.IDLE }
            rep.onSuccess { if (it == null) { error = "Pas assez d'images avec un corps détecté. Placez-vous entièrement dans le cadre, bien éclairé."; phase = Phase.IDLE } else result = it }
        }
    }
    LaunchedEffect(phase) {
        when (phase) {
            Phase.CALIBRATING -> {
                var stableSince = 0L
                while (phase == Phase.CALIBRATING) {
                    val f = live.frame
                    val ok = f != null && f.hasBody && f.trackingQuality() > 0.65f && PL.run { f.visible(L_ANKLE, 0.5f) && f.visible(R_ANKLE, 0.5f) }
                    val now = SystemClock.uptimeMillis()
                    if (ok) { if (stableSince == 0L) stableSince = now; if (now - stableSince > 1200) { runCatching { rt.calibrate(f!!) }.onSuccess { calib = it; phase = Phase.COUNTDOWN }.onFailure { stableSince = 0L } } } else stableSince = 0L
                    delay(50)
                }
            }
            Phase.COUNTDOWN -> { for (i in 3 downTo 1) { countdown = i; delay(1000) }; startRecording() }
            else -> {}
        }
    }

    // ---------------------------------------------------------------- vidéo importée
    var videoProgress by remember { mutableFloatStateOf(0f) }
    var videoBitmap by remember { mutableStateOf<Bitmap?>(null) }
    var videoJob by remember { mutableStateOf<Job?>(null) }
    val videoPicker = rememberLauncherForActivityResult(ActivityResultContracts.GetContent()) { uri ->
        if (uri == null) { if (startWithVideo && videoJob == null) nav.popBackStack(); return@rememberLauncherForActivityResult }
        phase = Phase.VIDEO
        videoJob = scope.launch {
            val videoExec = Executors.newSingleThreadExecutor().asCoroutineDispatcher()
            val vt = withContext(videoExec) { runCatching { BodyTracker(ctx, hands, face, settings.smoothing, forceCpu = true) }.getOrNull() }
            if (vt == null) { error = "Détection indisponible."; phase = Phase.IDLE; videoExec.close(); return@launch }
            try {
                val frames = withContext(videoExec) {
                    VideoProcessor.process(ctx, uri, settings.captureFps, vt) { p, f, b -> videoProgress = p; videoBitmap = b; if (f != null) onFrame(f) }
                }
                phase = Phase.PROCESSING
                val first = frames.firstOrNull { it.hasBody && it.trackingQuality() > 0.6f }
                val cal = first?.let { rt.calibrate(it) } ?: Calibration()
                calib = cal
                val rep = withContext(Dispatchers.Default) { ClipBuilder.build(frames, Retargeter(character.skeleton, character.boneMap), cal, opt, if (face && faceAvailable) character.face else null, settings.captureFps.toDouble(), "Vidéo ${store.clips.value.size + 1}", character.info.id) }
                if (rep == null) { error = "Aucun corps détecté de façon fiable dans cette vidéo."; phase = Phase.IDLE } else result = rep
            } catch (e: Throwable) {
                if (e !is kotlinx.coroutines.CancellationException) { error = "Analyse de la vidéo impossible : ${e.message ?: e.javaClass.simpleName}"; phase = Phase.IDLE }
            } finally { withContext(kotlinx.coroutines.NonCancellable + videoExec) { vt.close() }; videoExec.close(); videoBitmap = null }
        }
    }
    LaunchedEffect(Unit) { if (startWithVideo) videoPicker.launch("video/*") }

    // ---------------------------------------------------------------- interface
    val frame = remember(tick) { live.frame }
    val quality = frame?.trackingQuality() ?: 0f
    val lost = frame == null || !frame.hasBody
    Box(Modifier.fillMaxSize().background(Color.Black)) {
        if (phase == Phase.VIDEO || videoBitmap != null) {
            videoBitmap?.let { Image(it.asImageBitmap(), null, Modifier.fillMaxSize(), contentScale = ContentScale.Fit) }
        } else if (hasPermission) {
            AndroidView({ previewView }, Modifier.fillMaxSize())
        } else {
            Column(Modifier.align(Alignment.Center).padding(24.dp), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text("Accès à la caméra refusé", style = T.h2)
                Text("La capture en direct a besoin de la caméra. Vous pouvez aussi analyser une vidéo.", style = T.small)
                PrimaryButton("Autoriser la caméra") { permLauncher.launch(Manifest.permission.CAMERA) }
                SecondaryButton("Importer une vidéo") { videoPicker.launch("video/*") }
            }
        }
        // Squelette détecté
        frame?.let { f -> SkeletonOverlay(f, mirror = front && phase != Phase.VIDEO && videoBitmap == null, fitInside = videoBitmap != null, imageAspect = f.imageAspect) }

        // Barre du haut
        Row(Modifier.fillMaxWidth().statusBarsPadding().padding(10.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            IconButton({ nav.popBackStack() }, Modifier.background(Color(0xB307080B), Shape6)) { Icon(Icons.Outlined.Close, "Fermer", tint = C.Text) }
            if (phase == Phase.RECORDING) Row(Modifier.background(Color(0xB307080B), Shape6).padding(horizontal = 10.dp, vertical = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.size(8.dp).background(C.Danger, CircleShape)); Spacer(Modifier.width(8.dp))
                Text(formatTime((tick - recStart).coerceAtLeast(0) / 1000.0), style = T.mono.copy(fontSize = 14.sp, color = C.Text))
            }
            Spacer(Modifier.weight(1f))
            Column(Modifier.background(Color(0xB307080B), Shape6).padding(horizontal = 10.dp, vertical = 6.dp), horizontalAlignment = Alignment.End) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    QualityBars(quality, lost); Spacer(Modifier.width(6.dp))
                    Text(if (lost) "Suivi perdu" else "Tracking ${(quality * 100).toInt()} %", style = T.small.copy(color = if (lost) C.Warn else C.CyanSoft))
                }
                Text("${live.fps.toInt()} FPS · 33 points${if (hands) " · mains" else ""}${if (holder.delegate.isNotEmpty()) " · " + holder.delegate else ""}", style = T.label.copy(color = C.Text2))
            }
        }
        // Interrupteurs
        Row(Modifier.statusBarsPadding().padding(top = 66.dp, start = 10.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            ModeToggle("Corps", true, enabled = false) {}
            ModeToggle("Mains", hands) { hands = it }
            ModeToggle("Visage", face && faceAvailable, enabled = faceAvailable) { face = it }
        }
        if (!faceAvailable) Text("Visage : ce modèle n'a pas de blendshapes", style = T.small.copy(color = C.Text2), modifier = Modifier.statusBarsPadding().padding(top = 108.dp, start = 12.dp))

        // Aperçu 3D en direct
        Box(Modifier.align(Alignment.CenterEnd).padding(end = 10.dp, top = 60.dp).size(118.dp, 158.dp).clip(Shape6).border(1.dp, C.Border2, Shape6)) {
            Viewport3D(character, 0, Modifier.fillMaxSize()) { FramePose(live.pose, live.morphs) }
            Text("LIVE · ${character.info.name.uppercase().take(12)}", style = T.label.copy(fontSize = 9.sp, color = C.Text2), modifier = Modifier.align(Alignment.BottomStart).background(Color(0x9907080B)).padding(4.dp))
        }

        // Messages centraux
        val centerMsg = when (phase) {
            Phase.CALIBRATING -> "Calibration\nTenez-vous droit, de face, corps entier visible (pieds compris)."
            Phase.COUNTDOWN -> "$countdown"
            Phase.PROCESSING -> "Traitement de la capture…"
            Phase.VIDEO -> "Analyse de la vidéo… ${(videoProgress * 100).toInt()} %"
            else -> if (lost && hasPermission) "Placez-vous dans le cadre" else null
        }
        centerMsg?.let {
            Text(it, style = if (phase == Phase.COUNTDOWN) T.title.copy(fontSize = 72.sp) else T.bodyM, color = C.Text,
                textAlign = androidx.compose.ui.text.style.TextAlign.Center,
                modifier = Modifier.align(Alignment.Center).padding(32.dp).background(Color(0x9907080B), Shape6).padding(16.dp))
        }

        // Barre du bas
        Column(Modifier.align(Alignment.BottomCenter).fillMaxWidth().background(androidx.compose.ui.graphics.Brush.verticalGradient(listOf(Color.Transparent, Color(0xEB07080B)))).navigationBarsPadding().padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp), horizontalAlignment = Alignment.CenterHorizontally) {
            Text(listOf(
                when (settings.smoothing) { 0 -> "LISSAGE FAIBLE"; 2 -> "LISSAGE FORT"; else -> "LISSAGE MOYEN" },
                "PROFONDEUR ESTIMÉE", if (rootMotion) "ROOT MOTION" else "SUR PLACE", if (calib.calibrated) "CALIBRÉ" else "NON CALIBRÉ",
            ).joinToString(" · "), style = T.label.copy(color = C.Text2))
            if (phase == Phase.VIDEO) {
                LinearProgressIndicator({ videoProgress }, Modifier.fillMaxWidth(), color = C.Cyan, trackColor = C.Panel2)
                SecondaryButton("Annuler") { videoJob?.cancel(); phase = Phase.IDLE; if (startWithVideo) nav.popBackStack() }
            } else Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                SideButton(Icons.Outlined.VideoFile, "Vidéo", enabled = phase == Phase.IDLE) { videoPicker.launch("video/*") }
                SideButton(Icons.Outlined.Settings, "Options", enabled = phase == Phase.IDLE) { showOptions = true }
                val recording = phase == Phase.RECORDING
                Box(Modifier.size(78.dp).border(4.dp, C.Text, CircleShape).clip(CircleShape)
                    .clickable(enabled = hasPermission && phase in setOf(Phase.IDLE, Phase.RECORDING, Phase.CALIBRATING, Phase.COUNTDOWN)) {
                        when (phase) {
                            Phase.IDLE -> phase = if (calib.calibrated) Phase.COUNTDOWN else Phase.CALIBRATING
                            Phase.RECORDING -> stopRecording()
                            else -> phase = Phase.IDLE
                        }
                    }.semantics { contentDescription = if (recording) "Arrêter l'enregistrement" else "Enregistrer" },
                    contentAlignment = Alignment.Center) {
                    Box(Modifier.size(if (recording) 30.dp else 60.dp).background(C.Danger, if (recording) Shape6 else CircleShape))
                }
                SideButton(Icons.Outlined.Cameraswitch, "Caméra", enabled = phase == Phase.IDLE) { front = !front }
                SideButton(Icons.Outlined.Accessibility, "Calibrer", enabled = phase == Phase.IDLE) { calib = Calibration(); phase = Phase.CALIBRATING }
            }
        }
    }

    if (showOptions) AlertDialog(onDismissRequest = { showOptions = false }, containerColor = C.Panel, title = { Text("Options de capture", style = T.h2) },
        confirmButton = { TextButton({ showOptions = false }) { Text("OK") } },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState())) {
                ToggleRow("Root motion", rootMotion, "Déplacements dans la pièce (estimés depuis l'image)") { rootMotion = it }
                ToggleRow("Doigts", hands, "Active la détection des mains (21 points chacune)") { hands = it }
                ToggleRow("Garder l'orientation du modèle", settings.keepModelFacing, "Sinon le personnage fait face à la caméra") { v -> store.updateSettings { it.copy(keepModelFacing = v) } }
                Spacer(Modifier.height(8.dp)); SectionLabel("Lissage des tremblements")
                Segmented(listOf(0, 1, 2), settings.smoothing, { listOf("Faible", "Moyen", "Fort")[it] }) { v -> store.updateSettings { it.copy(smoothing = v) } }
                Spacer(Modifier.height(8.dp)); SectionLabel("Cadence de l'animation")
                Segmented(listOf(24, 30, 60), settings.captureFps, { "$it" }) { v -> store.updateSettings { it.copy(captureFps = v) } }
                Spacer(Modifier.height(8.dp))
                Text("Avec une seule caméra, la profondeur est estimée par le modèle de détection : les mouvements vers l'avant ou l'arrière et les membres cachés sont moins précis. Filmez de face ou de trois-quarts, corps entier, bien éclairé.", style = T.small)
            }
        })

    result?.let { rep ->
        SaveClipDialog(rep, onDiscard = { result = null; phase = Phase.IDLE }) { name, cat ->
            rep.clip.name = name; rep.clip.category = cat
            store.saveClip(rep.clip)
            result = null; phase = Phase.IDLE
            nav.navigate(Routes.editor(rep.clip.id)) { popUpTo(Routes.HOME) }
        }
    }
    error?.let { msg -> AlertDialog(onDismissRequest = { error = null }, containerColor = C.Panel, text = { Text(msg, style = T.body) }, confirmButton = { TextButton({ error = null }) { Text("OK") } }) }
}

fun morphMap(ch: LoadedCharacter, w: FloatArray): Map<Int, FloatArray> {
    val out = HashMap<Int, FloatArray>()
    ch.face.channels.forEachIndexed { i, c ->
        val n = ch.skeleton.morphTargets.firstOrNull { it.nodeIndex == c.node }?.targetNames?.size ?: return@forEachIndexed
        val arr = out.getOrPut(c.node) { FloatArray(n) }
        if (c.target < n) arr[c.target] = maxOf(arr[c.target], w[i])
    }
    return out
}

fun formatTime(s: Double): String { val m = (s / 60).toInt(); val sec = s - m * 60; return "%02d:%05.2f".format(m, sec) }

@Composable
private fun QualityBars(q: Float, lost: Boolean) {
    Row(verticalAlignment = Alignment.Bottom, horizontalArrangement = Arrangement.spacedBy(2.dp)) {
        listOf(5, 8, 11, 14).forEachIndexed { i, h ->
            Box(Modifier.width(3.dp).height(h.dp).background(if (!lost && q > i * 0.22f + 0.1f) C.Cyan else Color(0xFF3A4150)))
        }
    }
}

@Composable
private fun ModeToggle(label: String, on: Boolean, enabled: Boolean = true, onChange: (Boolean) -> Unit) {
    Box(Modifier.heightIn(min = 34.dp).clip(Shape4).background(if (on) C.Cyan else Color(0xBF07080B)).border(1.dp, if (on) C.Cyan else Color(0xFF3A4150), Shape4)
        .clickable(enabled = enabled) { onChange(!on) }.padding(horizontal = 12.dp), contentAlignment = Alignment.Center) {
        Text(label, style = T.small.copy(color = if (on) C.Black else if (enabled) C.Text2 else C.Faint, fontFamily = com.superessence.bodymotion.ui.theme.InterF))
    }
}

@Composable
private fun SideButton(icon: androidx.compose.ui.graphics.vector.ImageVector, label: String, enabled: Boolean, onClick: () -> Unit) {
    Column(Modifier.width(56.dp).clip(Shape6).clickable(enabled = enabled, onClick = onClick).padding(vertical = 6.dp), horizontalAlignment = Alignment.CenterHorizontally) {
        Icon(icon, null, tint = if (enabled) C.Text2 else C.Faint); Text(label, style = T.small.copy(color = if (enabled) C.Text2 else C.Faint))
    }
}

/** Dessine les points détectés par-dessus l'image (avec la même mise à l'échelle que l'aperçu). */
@Composable
private fun SkeletonOverlay(f: PoseFrame, mirror: Boolean, fitInside: Boolean, imageAspect: Float) {
    val img = f.image ?: return
    Canvas(Modifier.fillMaxSize()) {
        val vw = size.width; val vh = size.height
        val iw: Float; val ih: Float
        val viewAspect = vw / vh
        val scaleW: Float; val scaleH: Float
        if ((imageAspect > viewAspect) xor fitInside) { ih = vh; iw = vh * imageAspect } else { iw = vw; ih = vw / imageAspect }
        scaleW = iw; scaleH = ih
        val ox = (vw - scaleW) / 2; val oy = (vh - scaleH) / 2
        fun p(pt: ImagePoint) = Offset(ox + (if (mirror) 1 - pt.x else pt.x) * scaleW, oy + pt.y * scaleH)
        for ((a, b) in PL.SEGMENTS) {
            if (img[a].visibility < 0.3f || img[b].visibility < 0.3f) continue
            drawLine(C.Cyan, p(img[a]), p(img[b]), 4f, StrokeCap.Round)
        }
        for (i in 0 until PL.COUNT) {
            if (i in 1..10 && i != PL.L_EAR && i != PL.R_EAR) continue
            val v = img[i].visibility
            if (v < 0.3f) continue
            val c = p(img[i])
            drawCircle(Color(0xFF07080B), 7f, c); drawCircle(if (v < 0.6f) C.Warn else Color(0xFFE9FBFF), 7f, c, style = Stroke(3f))
        }
        for (hand in listOfNotNull(f.leftHandImage, f.rightHandImage)) {
            val links = listOf(0 to 1, 1 to 2, 2 to 3, 3 to 4, 0 to 5, 5 to 6, 6 to 7, 7 to 8, 5 to 9, 9 to 10, 10 to 11, 11 to 12, 9 to 13, 13 to 14, 14 to 15, 15 to 16, 13 to 17, 0 to 17, 17 to 18, 18 to 19, 19 to 20)
            for ((a, b) in links) drawLine(C.Violet, p(hand[a]), p(hand[b]), 2.5f, StrokeCap.Round)
        }
    }
}

@Composable
private fun SaveClipDialog(rep: ClipBuilder.Report, onDiscard: () -> Unit, onSave: (String, String) -> Unit) {
    var name by remember { mutableStateOf(rep.clip.name) }
    var cat by remember { mutableStateOf("Capture") }
    val lostPct = if (rep.totalFrames > 0) rep.lostFrames * 100 / rep.totalFrames else 0
    AlertDialog(onDismissRequest = {}, containerColor = C.Panel, title = { Text("Capture terminée", style = T.h2) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Text("${rep.clip.frameCount} images · ${"%.2f".format(rep.clip.duration)} s · ${rep.clip.fps.toInt()} FPS", style = T.mono)
                if (rep.lostFrames > 0) Text("$lostPct % des images ont été reconstruites (suivi perdu puis interpolé). Elles sont signalées en orange dans la timeline.", style = T.small.copy(color = C.Warn))
                OutlinedTextField(name, { name = it }, label = { Text("Nom") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                SectionLabel("Catégorie")
                Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    CLIP_CATEGORIES.forEach { c -> FilterChip(c == cat, { cat = c }, label = { Text(c) }, shape = Shape4) }
                }
            }
        },
        confirmButton = { TextButton(enabled = name.isNotBlank(), onClick = { onSave(name.trim(), cat) }) { Text("Enregistrer et éditer") } },
        dismissButton = { TextButton(onDiscard) { Text("Supprimer", color = C.Danger) } })
}

