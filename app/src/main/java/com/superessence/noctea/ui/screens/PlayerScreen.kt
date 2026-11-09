package com.superessence.noctea.ui.screens

import android.widget.Toast
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.Notes
import androidx.compose.material.icons.filled.Bedtime
import androidx.compose.material.icons.filled.Download
import androidx.compose.material.icons.filled.Favorite
import androidx.compose.material.icons.filled.FastForward
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.Pause
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.FastRewind
import androidx.compose.material.icons.filled.SkipNext
import androidx.compose.material.icons.filled.Speed
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material.icons.filled.Visibility
import androidx.compose.material.icons.outlined.FavoriteBorder
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.noctea.audio.PlayableKind
import com.superessence.noctea.data.BreathBlock
import com.superessence.noctea.data.Sounds
import com.superessence.noctea.ui.LocalApp
import com.superessence.noctea.ui.LocalNav
import com.superessence.noctea.ui.components.ArtIllustration
import com.superessence.noctea.ui.components.NightBackground
import com.superessence.noctea.ui.components.NightChip
import com.superessence.noctea.ui.components.SecondaryButton
import com.superessence.noctea.ui.components.formatClock
import com.superessence.noctea.ui.theme.Cormorant
import com.superessence.noctea.ui.theme.LocalPalette
import kotlinx.coroutines.launch

private enum class Sheet { NONE, TIMER, SOUND, SPEED, TEXT }

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun PlayerScreen() {
    val app = LocalApp.current
    val nav = LocalNav.current
    val p = LocalPalette.current
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    val ui by app.engine.ui.collectAsState()
    val snap by app.store.state.collectAsState()
    val levels by app.engine.mixer.levels.collectAsState()
    var sheet by remember { mutableStateOf(Sheet.NONE) }
    var minimal by rememberSaveable { mutableStateOf(snap.settings.minimalPlayer) }
    var exporting by remember { mutableStateOf<Float?>(null) }
    var dragging by remember { mutableStateOf<Float?>(null) }
    val playable = ui.playable

    // L'écran reste allumé pendant les exercices de respiration (guidage visuel).
    val view = LocalView.current
    val hasBreath = playable?.breath?.isNotEmpty() == true
    DisposableEffect(hasBreath) {
        view.keepScreenOn = hasBreath
        onDispose { view.keepScreenOn = false }
    }

    if (playable == null) {
        NightBackground {
            Column(Modifier.fillMaxSize().padding(32.dp), verticalArrangement = Arrangement.Center, horizontalAlignment = Alignment.CenterHorizontally) {
                Text("Aucune séance en cours", style = MaterialTheme.typography.headlineMedium)
                Spacer(Modifier.height(16.dp))
                SecondaryButton("Retour") { nav.popBackStack() }
            }
        }
        return
    }
    val isMixer = playable.kind == PlayableKind.MIXER
    val activeBreath = playable.breath.firstOrNull { ui.positionMs in it.start until it.end }

    if (minimal) {
        // Mode écran minimaliste : presque noir, juste une lueur et le temps.
        Box(
            Modifier.fillMaxSize().background(Color.Black)
                .clickable(remember { MutableInteractionSource() }, indication = null) { minimal = false },
            contentAlignment = Alignment.Center,
        ) {
            val glow by rememberInfiniteTransition(label = "m").animateFloat(0.25f, 0.6f, infiniteRepeatable(tween(5000), RepeatMode.Reverse), label = "g")
            Canvas(Modifier.size(260.dp)) {
                drawCircle(Brush.radialGradient(listOf(p.moon.copy(alpha = glow * 0.35f), Color.Transparent)))
            }
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                if (activeBreath != null) BreathCircle(activeBreath, ui.positionMs, dim = true)
                Text(
                    if (isMixer) formatClock(ui.positionMs) else "-" + formatClock(ui.durationMs - ui.positionMs),
                    color = Color.White.copy(alpha = 0.35f), fontFamily = Cormorant, fontSize = 30.sp,
                )
                Text("Toucher pour afficher les commandes", color = Color.White.copy(alpha = 0.18f), style = MaterialTheme.typography.bodySmall)
            }
        }
        return
    }

    NightBackground(stars = 50) {
        Column(Modifier.fillMaxSize().statusBarsPadding().navigationBarsPadding()) {
            Row(Modifier.fillMaxWidth().padding(horizontal = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                IconButton(onClick = { nav.popBackStack() }) { Icon(Icons.Filled.KeyboardArrowDown, "Réduire", tint = p.text) }
                Column(Modifier.weight(1f), horizontalAlignment = Alignment.CenterHorizontally) {
                    Text("EN ÉCOUTE", style = MaterialTheme.typography.labelSmall, color = p.textDim)
                    if (ui.queueSize > 1) Text("Rituel · étape ${ui.queueIndex + 1}/${ui.queueSize}", style = MaterialTheme.typography.bodySmall)
                }
                IconButton(onClick = { minimal = true }) { Icon(Icons.Filled.Visibility, "Mode minimaliste", tint = p.text) }
            }

            Column(
                Modifier.weight(1f).verticalScroll(rememberScrollState()).padding(horizontal = 26.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Spacer(Modifier.height(8.dp))
                Box(Modifier.fillMaxWidth().aspectRatio(1f).clip(RoundedCornerShape(36.dp)), contentAlignment = Alignment.Center) {
                    ArtIllustration(playable.art, Modifier.fillMaxSize())
                    if (activeBreath != null) {
                        Box(Modifier.fillMaxSize().background(Color.Black.copy(alpha = 0.35f)))
                        BreathCircle(activeBreath, ui.positionMs)
                    } else if (ui.isPlaying) {
                        PulseHalo()
                    }
                }
                Spacer(Modifier.height(22.dp))
                Text(playable.title, style = MaterialTheme.typography.headlineLarge, textAlign = TextAlign.Center)
                Text(playable.subtitle, style = MaterialTheme.typography.bodyMedium, textAlign = TextAlign.Center)
                ui.error?.let { Text(it, color = Color(0xFFFF9A9A), style = MaterialTheme.typography.bodySmall) }
                Spacer(Modifier.height(18.dp))

                // progression
                if (!isMixer) {
                    val progress = dragging ?: (if (ui.durationMs > 0) ui.positionMs / ui.durationMs.toFloat() else 0f)
                    Slider(
                        value = progress.coerceIn(0f, 1f),
                        onValueChange = { dragging = it },
                        onValueChangeFinished = { dragging?.let { app.engine.seekTo((it * ui.durationMs).toLong()) }; dragging = null },
                        colors = SliderDefaults.colors(thumbColor = p.moon, activeTrackColor = p.moon, inactiveTrackColor = Color.White.copy(alpha = 0.15f)),
                    )
                    Row(Modifier.fillMaxWidth()) {
                        Text(formatClock((progress * ui.durationMs).toLong()), style = MaterialTheme.typography.bodySmall)
                        Spacer(Modifier.weight(1f))
                        Text("-" + formatClock(ui.durationMs - (progress * ui.durationMs).toLong()), style = MaterialTheme.typography.bodySmall)
                    }
                } else {
                    Text("Ambiance en cours · ${formatClock(ui.positionMs)}", style = MaterialTheme.typography.bodyMedium)
                }
                if (ui.sleepTimerEndAt > 0 || ui.sleepAtEndOfSession) {
                    Spacer(Modifier.height(6.dp))
                    val left = ((ui.sleepTimerEndAt - System.currentTimeMillis()) / 60_000 + 1).coerceAtLeast(0)
                    Text(
                        if (ui.sleepAtEndOfSession) "🌙 Arrêt à la fin de la séance" else "🌙 Arrêt automatique dans $left min",
                        style = MaterialTheme.typography.labelMedium, color = p.accent,
                    )
                }
                Spacer(Modifier.height(10.dp))

                // commandes principales
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(18.dp)) {
                    if (!isMixer) RoundIcon(Icons.Filled.FastRewind, "Reculer de 15 secondes", 56.dp) { app.engine.seekBy(-15_000) }
                    val scale by animateFloatAsState(if (ui.isPlaying) 1f else 1.06f, label = "s")
                    Box(
                        Modifier.size((84 * scale).dp).clip(CircleShape)
                            .background(Brush.linearGradient(listOf(p.moon, Color(0xFFF7C9A0), p.accent)))
                            .clickable { app.engine.toggle() },
                        contentAlignment = Alignment.Center,
                    ) {
                        Icon(
                            if (ui.isPlaying) Icons.Filled.Pause else Icons.Filled.PlayArrow,
                            if (ui.isPlaying) "Pause" else "Lecture", tint = Color(0xFF1A1430), modifier = Modifier.size(40.dp),
                        )
                    }
                    if (!isMixer) RoundIcon(Icons.Filled.FastForward, "Avancer de 15 secondes", 56.dp) { app.engine.seekBy(15_000) }
                    if (ui.queueSize > 1 && ui.queueIndex < ui.queueSize - 1) RoundIcon(Icons.Filled.SkipNext, "Étape suivante", 48.dp) { app.engine.next() }
                }
                Text("± 15 s", style = MaterialTheme.typography.labelSmall, color = p.textDim.copy(alpha = 0.6f))
                Spacer(Modifier.height(20.dp))

                // actions secondaires
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                    val fav = playable.mediaId in snap.favorites
                    if (!isMixer) Action(if (fav) Icons.Filled.Favorite else Icons.Outlined.FavoriteBorder, "Favori", if (fav) p.moon else p.text) { app.store.toggleFavorite(playable.mediaId) }
                    Action(Icons.Filled.Bedtime, "Minuteur") { sheet = Sheet.TIMER }
                    Action(Icons.Filled.Tune, "Sons") { sheet = Sheet.SOUND }
                    if (!isMixer) Action(Icons.Filled.Speed, "x${"%.2f".format(ui.speed).trimEnd('0').trimEnd('.', ',')}") { sheet = Sheet.SPEED }
                    if (playable.text.isNotBlank()) Action(Icons.AutoMirrored.Filled.Notes, "Texte") { sheet = Sheet.TEXT }
                }
                if (!isMixer) {
                    Spacer(Modifier.height(14.dp))
                    val e = exporting
                    if (e == null) {
                        Row(
                            Modifier.clip(RoundedCornerShape(50)).clickable {
                                exporting = 0f
                                scope.launch {
                                    try {
                                        val r = app.exporter.export(playable, levels.map { com.superessence.noctea.data.AmbienceLayer(it.key, it.value) }, ui.musicVolume) { pr -> exporting = pr }
                                        Toast.makeText(ctx, "Enregistré : ${r.displayPath}", Toast.LENGTH_LONG).show()
                                    } catch (t: Throwable) {
                                        Toast.makeText(ctx, "Téléchargement impossible sur cet appareil (${t.javaClass.simpleName}).", Toast.LENGTH_LONG).show()
                                    } finally {
                                        exporting = null
                                    }
                                }
                            }.padding(horizontal = 16.dp, vertical = 8.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Icon(Icons.Filled.Download, null, tint = p.accent, modifier = Modifier.size(18.dp))
                            Spacer(Modifier.width(6.dp))
                            Text("Télécharger en fichier audio (voix + ambiance)", style = MaterialTheme.typography.labelMedium, color = p.accent)
                        }
                    } else {
                        Text("Création du fichier audio… ${(e * 100).toInt()} %", style = MaterialTheme.typography.labelMedium, color = p.accent)
                    }
                }
                Spacer(Modifier.height(24.dp))
            }
        }
    }

    if (sheet != Sheet.NONE) {
        ModalBottomSheet(onDismissRequest = { sheet = Sheet.NONE }, containerColor = p.sky1) {
            Column(Modifier.fillMaxWidth().navigationBarsPadding().padding(bottom = 20.dp)) {
                when (sheet) {
                    Sheet.TIMER -> TimerSheet(isMixer) { sheet = Sheet.NONE }
                    Sheet.SOUND -> SoundSheet(levels)
                    Sheet.SPEED -> SpeedSheet(ui.speed) { app.engine.setSpeed(it) }
                    Sheet.TEXT -> {
                        Text("Texte de la séance", style = MaterialTheme.typography.headlineSmall, modifier = Modifier.padding(horizontal = 20.dp))
                        Column(Modifier.height(420.dp).verticalScroll(rememberScrollState()).padding(20.dp)) {
                            playable.text.split("\n").forEach {
                                Text(it, fontFamily = Cormorant, fontSize = 19.sp, color = p.text, lineHeight = 26.sp, modifier = Modifier.padding(bottom = 10.dp))
                            }
                        }
                    }
                    Sheet.NONE -> {}
                }
            }
        }
    }
}

@Composable
private fun TimerSheet(isMixer: Boolean, close: () -> Unit) {
    val app = LocalApp.current
    Text("Minuteur de sommeil", style = MaterialTheme.typography.headlineSmall, modifier = Modifier.padding(horizontal = 20.dp))
    Text("Le son s'estompe doucement pendant 25 secondes avant l'arrêt.", style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(horizontal = 20.dp))
    Spacer(Modifier.height(14.dp))
    val options = listOf(5, 10, 15, 20, 30, 45, 60, 90, 120)
    Column(Modifier.padding(horizontal = 20.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        options.chunked(3).forEach { row ->
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                row.forEach { m -> NightChip(if (m >= 60) "${m / 60} h${if (m % 60 > 0) " ${m % 60}" else ""}" else "$m min", false, Modifier.weight(1f)) { app.engine.setSleepTimer(m); close() } }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            if (!isMixer) NightChip("Fin de la séance", false, Modifier.weight(1f)) { app.engine.sleepAtEndOfSession(); close() }
            NightChip("Désactiver", false, Modifier.weight(1f)) { app.engine.cancelSleepTimer(); close() }
        }
    }
}

@Composable
private fun SoundSheet(levels: Map<String, Float>) {
    val app = LocalApp.current
    val ui by app.engine.ui.collectAsState()
    Text("Voix & ambiance", style = MaterialTheme.typography.headlineSmall, modifier = Modifier.padding(horizontal = 20.dp))
    Spacer(Modifier.height(8.dp))
    if (ui.playable?.kind != PlayableKind.MIXER) {
        LabeledSlider("Volume de la voix", ui.voiceVolume, "${(ui.voiceVolume * 100).toInt()} %") { app.engine.setVoiceVolume(it) }
    }
    LabeledSlider("Volume de la musique et de l'ambiance", ui.musicVolume, "${(ui.musicVolume * 100).toInt()} %") { app.engine.setMusicVolume(it) }
    Spacer(Modifier.height(8.dp))
    Text("Couches sonores", style = MaterialTheme.typography.titleSmall, modifier = Modifier.padding(horizontal = 20.dp))
    Column(Modifier.height(300.dp).verticalScroll(rememberScrollState())) {
        val shown = (levels.keys + Sounds.all.map { it.id }).distinct()
        shown.forEach { id ->
            val s = Sounds.get(id) ?: return@forEach
            val v = levels[id] ?: 0f
            var local by remember(id) { mutableFloatStateOf(v) }
            LabeledSlider(s.name, local, if (local == 0f) "—" else "${(local * 100).toInt()} %") {
                local = it
                app.engine.setAmbienceLevel(id, it)
            }
        }
    }
}

@Composable
private fun SpeedSheet(current: Float, onSet: (Float) -> Unit) {
    Text("Vitesse de lecture", style = MaterialTheme.typography.headlineSmall, modifier = Modifier.padding(horizontal = 20.dp))
    Text("Ralentir rend la voix encore plus posée ; les silences s'allongent aussi.", style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(horizontal = 20.dp))
    Spacer(Modifier.height(12.dp))
    Row(Modifier.padding(horizontal = 20.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        listOf(0.8f, 0.9f, 1f, 1.1f).forEach { s ->
            NightChip("x$s".replace(".0", ""), kotlin.math.abs(current - s) < 0.01f, Modifier.weight(1f)) { onSet(s) }
        }
    }
}

@Composable
private fun RoundIcon(icon: ImageVector, desc: String, size: androidx.compose.ui.unit.Dp, onClick: () -> Unit) {
    val p = LocalPalette.current
    Box(Modifier.size(size).clip(CircleShape).background(Color.White.copy(alpha = 0.08f)).clickable(onClick = onClick), contentAlignment = Alignment.Center) {
        Icon(icon, desc, tint = p.text, modifier = Modifier.size(size * 0.5f))
    }
}

@Composable
private fun Action(icon: ImageVector, label: String, tint: Color = LocalPalette.current.text, onClick: () -> Unit) {
    Column(Modifier.clip(RoundedCornerShape(16.dp)).clickable(onClick = onClick).padding(8.dp), horizontalAlignment = Alignment.CenterHorizontally) {
        Icon(icon, label, tint = tint)
        Text(label, style = MaterialTheme.typography.labelSmall, color = LocalPalette.current.textDim)
    }
}

@Composable
private fun PulseHalo() {
    val p = LocalPalette.current
    val t by rememberInfiniteTransition(label = "halo").animateFloat(0.6f, 1f, infiniteRepeatable(tween(4000), RepeatMode.Reverse), label = "h")
    Canvas(Modifier.fillMaxSize()) {
        drawCircle(p.moon.copy(alpha = 0.18f * t), radius = size.minDimension * 0.42f * t, style = Stroke(2.dp.toPx()))
        drawCircle(p.accent.copy(alpha = 0.12f * t), radius = size.minDimension * 0.32f * t, style = Stroke(1.dp.toPx()))
    }
}

/** Cercle de respiration synchronisé avec la position de lecture. */
@Composable
fun BreathCircle(b: BreathBlock, positionMs: Long, dim: Boolean = false) {
    val p = LocalPalette.current
    val inBlock = (positionMs - b.start).coerceAtLeast(0)
    val t = if (b.cycle > 0) inBlock % b.cycle else 0
    val (label, frac) = when {
        t < b.inhale -> "Inspire" to (t / b.inhale.toFloat())
        t < b.inhale + b.hold -> "Retiens" to 1f
        t < b.inhale + b.hold + b.exhale -> "Expire" to (1f - (t - b.inhale - b.hold) / b.exhale.toFloat())
        else -> "Pause" to 0f
    }
    val scale by animateFloatAsState(0.45f + 0.55f * frac, tween(240), label = "breath")
    val alpha = if (dim) 0.4f else 1f
    Box(Modifier.size(220.dp), contentAlignment = Alignment.Center) {
        Canvas(Modifier.fillMaxSize()) {
            val r = size.minDimension / 2
            drawCircle(Brush.radialGradient(listOf(p.accent.copy(alpha = 0.5f * alpha), Color.Transparent), center, r * scale * 1.3f), r * scale * 1.3f, center)
            drawCircle(Color.White.copy(alpha = 0.18f * alpha), r * scale, center)
            drawCircle(p.moon.copy(alpha = 0.8f * alpha), r * scale, center, style = Stroke(2.dp.toPx()))
            drawCircle(Color.White.copy(alpha = 0.25f * alpha), r, center, style = Stroke(1.dp.toPx()))
            @Suppress("UNUSED_VARIABLE") val o = Offset.Zero
        }
        Text(label, fontFamily = Cormorant, fontStyle = FontStyle.Italic, fontSize = 30.sp, color = Color.White.copy(alpha = alpha))
    }
}
