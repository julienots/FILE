package com.superessence.sleepcut.ui

import android.app.TimePickerDialog
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
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
import androidx.compose.material.icons.filled.Bedtime
import androidx.compose.material.icons.filled.MusicNote
import androidx.compose.material.icons.filled.Schedule
import androidx.compose.material.icons.filled.SkipNext
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.sleepcut.Actions
import com.superessence.sleepcut.Settings
import com.superessence.sleepcut.TimerState
import kotlinx.coroutines.delay
import java.text.DateFormat
import java.util.Calendar
import java.util.Date
import kotlin.math.PI
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.roundToInt
import kotlin.math.sin

private fun clock(ms: Long): String = DateFormat.getTimeInstance(DateFormat.SHORT).format(Date(ms))
private fun durLabel(m: Int) = if (m >= 60) "${m / 60} h${if (m % 60 > 0) " %02d".format(m % 60) else ""}" else "$m min"

@Composable
fun MainScreen(st: TimerState, settings: Settings, resumeTick: Int, a: Actions) {
    var now by remember { mutableLongStateOf(System.currentTimeMillis()) }
    LaunchedEffect(Unit) { while (true) { now = System.currentTimeMillis(); delay(250) } }
    val running = st.endAt > now

    Box(Modifier.fillMaxSize().background(S.bg)) {
        Stars()
        Column(Modifier.fillMaxSize().statusBarsPadding().verticalScroll(rememberScrollState()).navigationBarsPadding().padding(bottom = 32.dp)) {
            Row(Modifier.padding(start = 22.dp, end = 22.dp, top = 18.dp), verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.size(44.dp).clip(CircleShape).background(Brush.linearGradient(listOf(S.moon, S.accent))), contentAlignment = Alignment.Center) {
                    Icon(Icons.Filled.Bedtime, null, tint = S.bg)
                }
                Spacer(Modifier.width(12.dp))
                Column {
                    Text("SleepCut", style = MaterialTheme.typography.headlineSmall)
                    Text("Ta musique se coupe pendant que tu t'endors", color = S.dim, fontSize = 13.sp)
                }
            }
            NowPlaying(a, resumeTick, now)

            AnimatedContent(running, transitionSpec = { fadeIn() togetherWith fadeOut() }, label = "mode") { r ->
                if (r) RunningPanel(st, settings, now, a) else SetupPanel(settings, now, a)
            }

            st.lastResult?.let {
                Card {
                    Text("Dernière coupure", fontWeight = FontWeight.Bold)
                    Text(it, color = S.dim, fontSize = 13.sp)
                }
            }
            Reliability(a, resumeTick)
            SettingsCard(settings, a, resumeTick)
            Text(
                "💡 Ajoute la tuile « SleepCut » dans tes réglages rapides, ou garde l'icône appuyée pour 15/30/60 min.\n${settings.stopCount} coupure(s) réussie(s) jusqu'ici.",
                color = S.dim, fontSize = 12.sp, textAlign = TextAlign.Center, modifier = Modifier.fillMaxWidth().padding(horizontal = 28.dp, vertical = 12.dp),
            )
        }
    }
}

@Composable
private fun Stars() {
    val t = rememberInfiniteTransition(label = "stars")
    val tw by t.animateFloat(0.3f, 1f, infiniteRepeatable(tween(3000), RepeatMode.Reverse), label = "tw")
    Canvas(Modifier.fillMaxSize()) {
        drawRect(Brush.verticalGradient(listOf(Color(0xFF16153A), S.bg), endY = size.height * 0.6f))
        val r = java.util.Random(7)
        repeat(70) { i ->
            val x = r.nextFloat() * size.width; val y = r.nextFloat() * size.height * 0.55f
            val alpha = (0.15f + r.nextFloat() * 0.6f) * (if (i % 3 == 0) tw else 1f)
            drawCircle(Color.White.copy(alpha = alpha), 1f + r.nextFloat() * 2.2f, Offset(x, y))
        }
    }
}

@Composable
private fun NowPlaying(a: Actions, resumeTick: Int, now: Long) {
    var list by remember { mutableStateOf(emptyList<Pair<String, String>>()) }
    LaunchedEffect(resumeTick, now / 3000) { list = a.nowPlaying() }
    if (list.isEmpty()) return
    Row(Modifier.padding(horizontal = 18.dp, vertical = 10.dp).fillMaxWidth().clip(RoundedCornerShape(16.dp)).background(S.surface).padding(12.dp), verticalAlignment = Alignment.CenterVertically) {
        Icon(Icons.Filled.MusicNote, null, tint = S.accent)
        Spacer(Modifier.width(10.dp))
        Column {
            list.take(2).forEach { (app, title) ->
                Text(app, fontWeight = FontWeight.Bold, fontSize = 13.sp)
                if (title.isNotBlank()) Text(title, color = S.dim, fontSize = 12.sp, maxLines = 1)
            }
        }
    }
}

// ───────────────────────────── Réglage du minuteur ─────────────────────────────

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun SetupPanel(settings: Settings, now: Long, a: Actions) {
    var minutes by remember { mutableIntStateOf(settings.lastMinutes.coerceIn(1, 240)) }
    val ctx = LocalContext.current
    Column(horizontalAlignment = Alignment.CenterHorizontally, modifier = Modifier.fillMaxWidth()) {
        Dial(minutes, Modifier.padding(top = 12.dp).size(290.dp)) { minutes = it }
        Text("Arrêt vers ${clock(now + minutes * 60_000L)}", color = S.dim, modifier = Modifier.padding(top = 4.dp))
        FlowRow(Modifier.padding(horizontal = 16.dp, vertical = 12.dp), horizontalArrangement = Arrangement.Center) {
            listOf(5, 10, 15, 20, 30, 45, 60, 90, 120, 180).forEach { m -> Chip(durLabel(m), minutes == m) { minutes = m } }
        }
        Row(Modifier.padding(horizontal = 18.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Outline("À une heure précise", Icons.Filled.Schedule, Modifier.weight(1f)) {
                val c = Calendar.getInstance()
                TimePickerDialog(ctx, { _, h, m ->
                    val t = Calendar.getInstance().apply { set(Calendar.HOUR_OF_DAY, h); set(Calendar.MINUTE, m); set(Calendar.SECOND, 0) }
                    if (t.timeInMillis <= System.currentTimeMillis()) t.add(Calendar.DAY_OF_MONTH, 1)
                    a.startAt(t.timeInMillis)
                }, (c.get(Calendar.HOUR_OF_DAY) + 1) % 24, 0, true).show()
            }
            val rem = remember(now / 2000) { a.trackRemaining() }
            Outline(if (rem != null) "Fin du morceau (${(rem / 60000)}:${"%02d".format((rem / 1000) % 60)})" else "Fin du morceau", Icons.Filled.SkipNext, Modifier.weight(1f), enabled = rem != null) {
                val r = a.trackRemaining() ?: return@Outline
                a.startAt(System.currentTimeMillis() + r - 400L)
            }
        }
        if (!a.sessionOk()) Text("« Fin du morceau » nécessite l'accès aux lecteurs (voir plus bas).", color = S.dim, fontSize = 11.sp, modifier = Modifier.padding(top = 6.dp))
        Box(
            Modifier.padding(18.dp).fillMaxWidth().height(64.dp).clip(RoundedCornerShape(22.dp))
                .background(Brush.horizontalGradient(listOf(S.accent, S.moon))).clickable { a.start(minutes) },
            contentAlignment = Alignment.Center,
        ) { Text("Lancer · ${durLabel(minutes)}", color = S.bg, fontWeight = FontWeight.Black, fontSize = 20.sp) }
    }
}

@Composable
private fun Dial(minutes: Int, modifier: Modifier, onChange: (Int) -> Unit) {
    var last by remember { mutableIntStateOf(minutes) }
    last = minutes
    Box(modifier, contentAlignment = Alignment.Center) {
        Canvas(
            Modifier.fillMaxSize().pointerInput(Unit) {
                fun angleToMinutes(o: Offset): Int {
                    val c = Offset(size.width / 2f, size.height / 2f)
                    var ang = atan2(o.y - c.y, o.x - c.x) + PI / 2
                    if (ang < 0) ang += 2 * PI
                    val lap = if (last > 120) 120 else 0
                    var m = (ang / (2 * PI) * 120).roundToInt() + lap
                    // continuité : pas de saut 120 → 1 en passant par le haut
                    if (last in 100..120 && m < 20) m += 120
                    if (last in 121..140 && m in 100..120) m = m
                    if (last <= 20 && m > 100 && lap == 0) m = 1
                    return m.coerceIn(1, 240)
                }
                detectDragGestures(onDragStart = { onChange(angleToMinutes(it)) }) { ch, _ -> onChange(angleToMinutes(ch.position)) }
            }.pointerInput(Unit) { detectTapGestures { } },
        ) {
            val stroke = 22.dp.toPx()
            val r = size.minDimension / 2 - stroke
            val c = Offset(size.width / 2, size.height / 2)
            drawCircle(S.surface, r, c, style = Stroke(stroke))
            // graduations
            for (i in 0 until 24) {
                val ang = i / 24f * 2 * PI.toFloat() - PI.toFloat() / 2
                val r0 = r - stroke * 1.1f; val r1 = r0 - if (i % 2 == 0) 12f else 6f
                drawLine(S.dim.copy(alpha = 0.5f), Offset(c.x + cos(ang) * r0, c.y + sin(ang) * r0), Offset(c.x + cos(ang) * r1, c.y + sin(ang) * r1), 3f, StrokeCap.Round)
            }
            val first = minutes.coerceAtMost(120) / 120f
            drawArc(Brush.sweepGradient(listOf(S.accent, S.moon, S.accent), c), -90f, first * 360f, false, Offset(c.x - r, c.y - r), Size(r * 2, r * 2), style = Stroke(stroke, cap = StrokeCap.Round))
            if (minutes > 120) {
                drawArc(S.pink, -90f, (minutes - 120) / 120f * 360f, false, Offset(c.x - r, c.y - r), Size(r * 2, r * 2), style = Stroke(stroke * 0.45f, cap = StrokeCap.Round))
            }
            val ang = ((minutes % 120) / 120f) * 2 * PI.toFloat() - PI.toFloat() / 2
            val k = Offset(c.x + cos(ang) * r, c.y + sin(ang) * r)
            drawCircle(Color.White, stroke * 0.75f, k)
            drawCircle(S.accent, stroke * 0.4f, k)
        }
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Text(if (minutes >= 60) "${minutes / 60}h${"%02d".format(minutes % 60)}" else "$minutes", fontSize = 64.sp, fontWeight = FontWeight.Black, color = S.text)
            Text(if (minutes >= 60) "heure${if (minutes >= 120) "s" else ""}" else "minutes", color = S.dim)
            Text("Tourne le cercle", color = S.dim.copy(alpha = 0.6f), fontSize = 11.sp, modifier = Modifier.padding(top = 4.dp))
        }
    }
}

// ───────────────────────────── Minuteur en cours ─────────────────────────────

@Composable
private fun RunningPanel(st: TimerState, settings: Settings, now: Long, a: Actions) {
    val left = (st.endAt - now).coerceAtLeast(0)
    val total = (st.endAt - st.startedAt).coerceAtLeast(1)
    val frac = left.toFloat() / total
    val pulse = rememberInfiniteTransition(label = "p").animateFloat(0.85f, 1f, infiniteRepeatable(tween(2200), RepeatMode.Reverse), label = "pp")
    Column(horizontalAlignment = Alignment.CenterHorizontally, modifier = Modifier.fillMaxWidth()) {
        Box(Modifier.padding(top = 14.dp).size(290.dp), contentAlignment = Alignment.Center) {
            Canvas(Modifier.fillMaxSize()) {
                val stroke = 16.dp.toPx()
                val r = size.minDimension / 2 - stroke
                val c = Offset(size.width / 2, size.height / 2)
                drawCircle(Brush.radialGradient(listOf(S.moon.copy(alpha = 0.22f * pulse.value), Color.Transparent), c, r * 1.2f), r * 1.2f, c)
                drawCircle(S.surface, r, c, style = Stroke(stroke))
                drawArc(Brush.sweepGradient(listOf(S.moon, S.accent, S.moon), c), -90f, frac * 360f, false, Offset(c.x - r, c.y - r), Size(r * 2, r * 2), style = Stroke(stroke, cap = StrokeCap.Round))
            }
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                val s = left / 1000
                Text(if (s >= 3600) "%d:%02d:%02d".format(s / 3600, (s / 60) % 60, s % 60) else "%d:%02d".format(s / 60, s % 60), fontSize = 58.sp, fontWeight = FontWeight.Black)
                Text(if (st.fading) "Fondu en cours… 🌙" else "Coupure à ${clock(st.endAt)}", color = if (st.fading) S.moon else S.dim)
                if (settings.shakeToExtend) Text("Secoue le tel : +5 min", color = S.dim.copy(alpha = 0.7f), fontSize = 11.sp, modifier = Modifier.padding(top = 4.dp))
            }
        }
        Row(Modifier.padding(horizontal = 18.dp, vertical = 12.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Outline("−5 min", null, Modifier.weight(1f)) { a.extend(-5) }
            Outline("+5 min", null, Modifier.weight(1f)) { a.extend(5) }
            Outline("+15 min", null, Modifier.weight(1f)) { a.extend(15) }
        }
        Row(Modifier.padding(horizontal = 18.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Box(Modifier.weight(1f).height(56.dp).clip(RoundedCornerShape(18.dp)).background(S.surface).clickable { a.cancel() }, contentAlignment = Alignment.Center) {
                Text("Annuler", fontWeight = FontWeight.Bold)
            }
            Box(Modifier.weight(1f).height(56.dp).clip(RoundedCornerShape(18.dp)).background(S.pink).clickable { a.stopNow() }, contentAlignment = Alignment.Center) {
                Text("Couper maintenant", fontWeight = FontWeight.Black, color = S.bg)
            }
        }
        Text("Tu peux verrouiller ton téléphone : la coupure se fera quand même.", color = S.dim, fontSize = 12.sp, modifier = Modifier.padding(16.dp), textAlign = TextAlign.Center)
    }
}

// ───────────────────────────── Fiabilité & réglages ─────────────────────────────

@Composable
private fun Reliability(a: Actions, resumeTick: Int) {
    val session = remember(resumeTick) { a.sessionOk() }
    val battery = remember(resumeTick) { a.batteryOk() }
    val exact = remember(resumeTick) { a.exactOk() }
    val allOk = session && battery && exact
    Card {
        Text(if (allOk) "✅ Prêt à couper, même téléphone en veille" else "⚙️ Fiabilité écran éteint", fontWeight = FontWeight.Bold)
        if (!allOk) Text("Active ces options une fois pour que la coupure marche à tous les coups.", color = S.dim, fontSize = 12.sp, modifier = Modifier.padding(bottom = 4.dp))
        Check("Accès aux lecteurs", "Met en pause toutes les applis (Spotify, YouTube, Deezer…) et affiche ce qui joue.", session, a::openNotificationAccess)
        Check("Batterie sans restriction", "Indispensable sur Samsung, Xiaomi, Huawei, Oppo… sinon le système peut endormir l'appli.", battery, a::openBattery)
        Check("Alarmes exactes", "Réveille le téléphone pile au bon moment, même en veille profonde.", exact, a::openExactAlarm)
    }
}

@Composable
private fun Check(title: String, desc: String, ok: Boolean, onFix: () -> Unit) {
    Row(Modifier.fillMaxWidth().padding(vertical = 8.dp), verticalAlignment = Alignment.CenterVertically) {
        Box(Modifier.size(12.dp).clip(CircleShape).background(if (ok) S.good else S.pink))
        Spacer(Modifier.width(12.dp))
        Column(Modifier.weight(1f)) {
            Text(title, fontWeight = FontWeight.SemiBold, fontSize = 14.sp)
            Text(desc, color = S.dim, fontSize = 12.sp)
        }
        if (!ok) Box(Modifier.padding(start = 8.dp).clip(RoundedCornerShape(10.dp)).background(S.accent).clickable(onClick = onFix).padding(horizontal = 12.dp, vertical = 8.dp)) {
            Text("Activer", color = S.bg, fontWeight = FontWeight.Bold, fontSize = 13.sp)
        }
    }
}

@Composable
private fun SettingsCard(s: Settings, a: Actions, resumeTick: Int) {
    Card {
        Text("Réglages", fontWeight = FontWeight.Bold)
        var fade by remember(s.fadeSeconds) { mutableFloatStateOf(s.fadeSeconds.toFloat()) }
        Row(Modifier.padding(top = 8.dp)) {
            Text("Fondu de volume", fontSize = 14.sp)
            Spacer(Modifier.weight(1f))
            Text(if (fade < 1f) "Aucun" else "${fade.roundToInt()} s", color = S.dim)
        }
        Slider(fade, { fade = it }, valueRange = 0f..120f, steps = 23, onValueChangeFinished = { a.settings { st -> st.copy(fadeSeconds = fade.roundToInt()) } },
            colors = SliderDefaults.colors(thumbColor = Color.White, activeTrackColor = S.accent, inactiveTrackColor = S.surface2))
        Toggle("Remettre le volume après", "Le volume revient à son niveau pour demain matin.", s.restoreVolume) { v -> a.settings { it.copy(restoreVolume = v) } }
        Toggle("Secouer pour +5 min", "Encore réveillé ? Secoue le téléphone (ou remonte le volume pendant le fondu).", s.shakeToExtend) { v -> a.settings { it.copy(shakeToExtend = v) } }
        Toggle("Petite vibration à la coupure", null, s.vibrateAtEnd) { v -> a.settings { it.copy(vibrateAtEnd = v) } }
        val dndOk = remember(resumeTick) { a.dndOk() }
        Toggle("Activer « Ne pas déranger »", if (dndOk) "Après la coupure, plus aucune notification ne te réveille." else "Nécessite une autorisation (toucher pour l'accorder).", s.dndAtEnd && dndOk) { v ->
            if (v && !dndOk) a.openDndAccess() else a.settings { it.copy(dndAtEnd = v) }
        }
    }
}

@Composable
private fun Toggle(title: String, desc: String?, on: Boolean, onChange: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth().clickable { onChange(!on) }.padding(vertical = 8.dp), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Text(title, fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
            if (desc != null) Text(desc, color = S.dim, fontSize = 12.sp)
        }
        Switch(on, onChange, colors = SwitchDefaults.colors(checkedTrackColor = S.accent, checkedThumbColor = S.bg))
    }
}

@Composable
private fun Card(content: @Composable () -> Unit) {
    Column(Modifier.padding(horizontal = 18.dp, vertical = 8.dp).fillMaxWidth().clip(RoundedCornerShape(20.dp)).background(S.surface.copy(alpha = 0.92f)).border(1.dp, S.line, RoundedCornerShape(20.dp)).padding(16.dp)) { content() }
}

@Composable
private fun Chip(text: String, on: Boolean, onClick: () -> Unit) {
    Box(Modifier.padding(4.dp).clip(RoundedCornerShape(12.dp)).background(if (on) S.accent else S.surface).clickable(onClick = onClick).padding(horizontal = 14.dp, vertical = 9.dp)) {
        Text(text, fontWeight = FontWeight.Bold, fontSize = 14.sp, color = if (on) S.bg else S.text)
    }
}

@Composable
private fun Outline(text: String, icon: androidx.compose.ui.graphics.vector.ImageVector?, modifier: Modifier, enabled: Boolean = true, onClick: () -> Unit) {
    Row(
        modifier.height(50.dp).clip(RoundedCornerShape(16.dp)).border(1.dp, if (enabled) S.line else S.line.copy(alpha = 0.4f), RoundedCornerShape(16.dp))
            .clickable(enabled = enabled, onClick = onClick).padding(horizontal = 10.dp),
        horizontalArrangement = Arrangement.Center, verticalAlignment = Alignment.CenterVertically,
    ) {
        if (icon != null) { Icon(icon, null, tint = if (enabled) S.accent else S.dim, modifier = Modifier.size(18.dp)); Spacer(Modifier.width(6.dp)) }
        Text(text, fontWeight = FontWeight.Bold, fontSize = 13.sp, color = if (enabled) S.text else S.dim, maxLines = 2, textAlign = TextAlign.Center)
    }
}
