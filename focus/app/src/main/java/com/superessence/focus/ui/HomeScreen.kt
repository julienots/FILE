package com.superessence.focus.ui

import android.widget.Toast
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.Calculate
import androidx.compose.material.icons.rounded.Lock
import androidx.compose.material.icons.rounded.Pause
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material.icons.rounded.Remove
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.graphics.drawable.toBitmap
import com.superessence.focus.core.Apps
import com.superessence.focus.core.BlockerService
import com.superessence.focus.core.Music
import com.superessence.focus.core.Session
import com.superessence.focus.core.Sound
import com.superessence.focus.core.Store
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import java.time.LocalDate
import java.time.LocalTime
import java.time.format.TextStyle as JTextStyle
import java.util.Locale

fun fmt(ms: Long): String {
    val s = (ms + 999) / 1000
    val h = s / 3600; val m = (s % 3600) / 60; val sec = s % 60
    return if (h > 0) "%d:%02d:%02d".format(h, m, sec) else "%02d:%02d".format(m, sec)
}

@Composable
fun rememberNow(): Long {
    var now by remember { mutableLongStateOf(System.currentTimeMillis()) }
    val ctx = LocalContext.current
    LaunchedEffect(Unit) { while (true) { now = System.currentTimeMillis(); Session.check(ctx); delay(500) } }
    return now
}

@Composable
fun HomeScreen(onOpenTab: (Int) -> Unit) {
    @Suppress("UNUSED_VARIABLE") val rev = Store.rev.intValue
    val now = rememberNow()
    Column(
        Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(horizontal = 16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        Spacer(Modifier.height(6.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            LogoRow()
            Spacer(Modifier.weight(1f))
            Body(greeting(), size = 13.sp)
        }
        StatsRow()
        TimerPanel(now, onOpenTab)
        QuickApps(onOpenTab)
        MusicPanel()
        WeekPanel()
        Body("Une création SuperEssence Studio", size = 12.sp, modifier = Modifier.fillMaxWidth().padding(bottom = 16.dp), color = C.Muted.copy(alpha = 0.6f))
    }
}

private fun greeting(): String {
    val h = LocalTime.now().hour
    return when { h < 6 -> "Bonne nuit 🌙"; h < 12 -> "Bonjour ☀️"; h < 18 -> "Bon après-midi"; else -> "Bonsoir ✨" }
}

@Composable
private fun StatsRow() {
    Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
        Stat("Aujourd'hui", "${Store.minutesOn(LocalDate.now())} min", Modifier.weight(1f))
        Stat("Série", "${Store.streak()} j 🔥", Modifier.weight(1f))
        val t = Store.totalMinutes
        Stat("Total", if (t >= 60) "${t / 60} h ${t % 60}" else "$t min", Modifier.weight(1f))
    }
}

@Composable
private fun Stat(label: String, value: String, modifier: Modifier) {
    Panel(modifier, color = C.Surface) {
        Body(label, size = 12.sp)
        Title(value, 19.sp)
    }
}

@Composable
private fun TimerPanel(now: Long, onOpenTab: (Int) -> Unit) {
    val ctx = LocalContext.current
    val running = Session.running
    var minutes by remember { mutableStateOf(Store.lastDuration) }
    var askSetup by remember { mutableStateOf(false) }
    var askStrict by remember { mutableStateOf(false) }
    val total = (Store.sessionEnd - Store.sessionStart).coerceAtLeast(1)
    val remaining = if (running) Session.remainingMs(now) else minutes * 60_000L
    val progress = if (running) remaining.toFloat() / total else 1f
    val pulse by rememberInfiniteTransition(label = "p").animateFloat(0.4f, 1f, infiniteRepeatable(tween(1400, easing = LinearEasing), RepeatMode.Reverse), label = "p")

    Panel(Modifier.fillMaxWidth()) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Title(if (running) "Concentration en cours" else "Session de révision", 20.sp)
            Spacer(Modifier.weight(1f))
            if (Session.blocking) Pill(color = C.Yellow.copy(alpha = 0.15f)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(Icons.Rounded.Lock, null, tint = C.Yellow, modifier = Modifier.size(14.dp))
                    Spacer(Modifier.width(4.dp))
                    Text("Bloqué", color = C.Yellow, fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 12.sp)
                }
            }
        }
        Spacer(Modifier.height(10.dp))
        Box(Modifier.fillMaxWidth().height(250.dp), contentAlignment = Alignment.Center) {
            Canvas(Modifier.size(240.dp)) {
                val w = 16.dp.toPx()
                val r = size.minDimension / 2 - w
                val tl = Offset(center.x - r, center.y - r)
                drawCircle(C.Surface2, r, center, style = Stroke(w))
                drawArc(
                    Brush.sweepGradient(listOf(C.YellowDeep, C.Yellow, C.YellowDeep), center),
                    -90f, 360f * progress, false, tl, Size(2 * r, 2 * r), style = Stroke(w, cap = StrokeCap.Round),
                )
                if (running) {
                    val a = Math.toRadians((-90.0 + 360 * progress))
                    val p = Offset(center.x + r * kotlin.math.cos(a).toFloat(), center.y + r * kotlin.math.sin(a).toFloat())
                    drawCircle(C.Yellow.copy(alpha = 0.35f * pulse), w * 1.2f, p)
                    drawCircle(Color.White, w * 0.32f, p)
                }
            }
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Text(fmt(remaining), color = C.Text, fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 52.sp)
                Body(
                    if (running) "fin à " + java.time.Instant.ofEpochMilli(Store.sessionEnd).atZone(java.time.ZoneId.systemDefault()).toLocalTime().toString().take(5)
                    else "minutes de focus",
                    size = 13.sp,
                )
            }
        }
        Spacer(Modifier.height(8.dp))
        if (!running) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.Center) {
                RoundIcon(Icons.Rounded.Remove) { minutes = (minutes - 5).coerceAtLeast(5) }
                Spacer(Modifier.width(10.dp))
                Row(Modifier.horizontalScroll(rememberScrollState()).weight(1f), horizontalArrangement = Arrangement.spacedBy(6.dp, Alignment.CenterHorizontally)) {
                    listOf(25, 45, 60, 90).forEach { m -> Chip("$m", m == minutes) { minutes = m } }
                    if (minutes !in listOf(25, 45, 60, 90)) Chip("$minutes", true) {}
                }
                Spacer(Modifier.width(10.dp))
                RoundIcon(Icons.Rounded.Add) { minutes = (minutes + 5).coerceAtMost(240) }
            }
            Spacer(Modifier.height(12.dp))
            ToggleRow("Bloquer les distractions", "Seuls la calculatrice, tes applis autorisées et ${Store.romaneName} restent accessibles", Store.blockPref) { Store.blockPref = it }
            AnimatedVisibility(Store.blockPref) {
                ToggleRow("Verrouillage strict", "Impossible d'arrêter avant la fin du minuteur", Store.strictPref) {
                    if (it) askStrict = true else Store.strictPref = false
                }
            }
            Spacer(Modifier.height(12.dp))
            Button(
                onClick = {
                    if (Store.blockPref && !BlockerService.isEnabled(ctx)) askSetup = true
                    else {
                        Session.start(ctx, minutes, Store.blockPref, Store.strictPref)
                        if (!Music.playing && Store.track != "none") Music.play(ctx)
                    }
                },
                modifier = Modifier.fillMaxWidth().height(56.dp),
                shape = RoundedCornerShape(18.dp),
                colors = ButtonDefaults.buttonColors(C.Yellow, Color.Black),
            ) { Text("Commencer", fontFamily = Fredoka, fontWeight = FontWeight.Bold, fontSize = 19.sp) }
        } else {
            if (Store.sessionStrict) {
                Body("🔒 Verrouillage strict : la session s'arrêtera toute seule à la fin du minuteur.", modifier = Modifier.fillMaxWidth(), color = C.Muted)
            } else {
                HoldToStop { if (!Session.stop(ctx)) Toast.makeText(ctx, "Verrouillage strict actif", Toast.LENGTH_SHORT).show() }
            }
        }
    }

    if (askSetup) AlertDialog(
        onDismissRequest = { askSetup = false },
        containerColor = C.Surface,
        title = { Title("Active le blocage", 20.sp) },
        text = { Body("Pour bloquer les autres applis, active d'abord le service « Focus. » dans l'onglet Blocage (une seule fois).", color = C.Text) },
        confirmButton = { TextButton({ askSetup = false; onOpenTab(3) }) { Text("Configurer", color = C.Yellow) } },
        dismissButton = {
            TextButton({
                askSetup = false
                Session.start(ctx, minutes, false, false)
                if (!Music.playing && Store.track != "none") Music.play(ctx)
            }) { Text("Sans blocage", color = C.Muted) }
        },
    )
    if (askStrict) AlertDialog(
        onDismissRequest = { askStrict = false },
        containerColor = C.Surface,
        title = { Title("Verrouillage strict ?", 20.sp) },
        text = { Body("Pendant la session, il sera impossible d'arrêter le blocage, même depuis Focus. Les appels d'urgence restent possibles. Tu es sûr·e ?", color = C.Text) },
        confirmButton = { TextButton({ askStrict = false; Store.strictPref = true }) { Text("Oui, je m'engage", color = C.Yellow) } },
        dismissButton = { TextButton({ askStrict = false }) { Text("Annuler", color = C.Muted) } },
    )
}

@Composable
private fun HoldToStop(onStop: () -> Unit) {
    val p = remember { Animatable(0f) }
    val scope = rememberCoroutineScope()
    Box(
        Modifier.fillMaxWidth().height(56.dp).clip(RoundedCornerShape(18.dp)).background(C.Surface2)
            .pointerInput(Unit) {
                detectTapGestures(onPress = {
                    val job = scope.launch {
                        p.animateTo(1f, tween(2500, easing = LinearEasing))
                        onStop(); p.snapTo(0f)
                    }
                    tryAwaitRelease()
                    if (p.value < 1f) { job.cancel(); scope.launch { p.animateTo(0f, tween(250)) } }
                })
            },
        contentAlignment = Alignment.Center,
    ) {
        Box(Modifier.fillMaxHeight().fillMaxWidth(p.value).align(Alignment.CenterStart).background(C.Red.copy(alpha = 0.35f)))
        Text("Maintenir pour arrêter la session", color = C.Text, fontFamily = Nunito, fontWeight = FontWeight.Bold)
    }
}

@Composable
fun ToggleRow(title: String, sub: String, checked: Boolean, enabled: Boolean = true, onChange: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth().padding(vertical = 6.dp), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Body(title, color = C.Text, size = 15.sp, weight = FontWeight.Bold)
            Body(sub, size = 12.sp)
        }
        Spacer(Modifier.width(10.dp))
        Switch(
            checked, onChange, enabled = enabled,
            colors = SwitchDefaults.colors(checkedTrackColor = C.Yellow, checkedThumbColor = Color.Black, uncheckedTrackColor = C.Surface2, uncheckedBorderColor = C.Line),
        )
    }
}

@Composable
fun Chip(text: String, selected: Boolean, onClick: () -> Unit) {
    Box(
        Modifier.clip(RoundedCornerShape(50)).background(if (selected) C.Yellow else C.Surface2)
            .clickable(onClick = onClick).padding(horizontal = 14.dp, vertical = 9.dp),
    ) { Text(text, color = if (selected) Color.Black else C.Text, fontFamily = Nunito, fontWeight = FontWeight.Bold, fontSize = 14.sp) }
}

@Composable
private fun RoundIcon(icon: androidx.compose.ui.graphics.vector.ImageVector, onClick: () -> Unit) {
    Box(Modifier.size(40.dp).clip(CircleShape).border(1.dp, C.Line, CircleShape).clickable(onClick = onClick), contentAlignment = Alignment.Center) {
        Icon(icon, null, tint = C.Text)
    }
}

@Composable
private fun QuickApps(onOpenTab: (Int) -> Unit) {
    val ctx = LocalContext.current
    @Suppress("UNUSED_VARIABLE") val rev = Store.rev.intValue
    val apps = remember(Store.allowedApps) { Apps.installed(ctx).filter { it.auto || it.pkg in Store.allowedApps } }
    Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
        QuickTile("💬", Store.romaneName) { onOpenTab(2) }
        if (apps.none { it.auto }) QuickTile(null, "Calculatrice", Icons.Rounded.Calculate) {
            Toast.makeText(ctx, "Aucune calculatrice trouvée", Toast.LENGTH_SHORT).show()
        }
        apps.forEach { a ->
            Column(
                Modifier.width(84.dp).clip(RoundedCornerShape(20.dp)).background(C.Surface).clickable { Apps.launch(ctx, a.pkg) }.padding(10.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                val bmp = remember(a.pkg) { a.icon?.toBitmap(96, 96)?.asImageBitmap() }
                if (bmp != null) Image(bmp, null, Modifier.size(38.dp)) else Box(Modifier.size(38.dp))
                Spacer(Modifier.height(6.dp))
                Body(a.label, color = C.Text, size = 12.sp, modifier = Modifier.fillMaxWidth(), weight = FontWeight.SemiBold)
            }
        }
    }
}

@Composable
private fun QuickTile(emoji: String?, label: String, icon: androidx.compose.ui.graphics.vector.ImageVector? = null, onClick: () -> Unit) {
    Column(
        Modifier.width(84.dp).clip(RoundedCornerShape(20.dp)).background(C.Surface).clickable(onClick = onClick).padding(10.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Box(Modifier.size(38.dp).clip(CircleShape).background(C.Yellow), contentAlignment = Alignment.Center) {
            if (icon != null) Icon(icon, null, tint = Color.Black) else Text(emoji ?: "", fontSize = 18.sp)
        }
        Spacer(Modifier.height(6.dp))
        Body(label, color = C.Text, size = 12.sp, weight = FontWeight.SemiBold)
    }
}

@Composable
private fun MusicPanel() {
    val ctx = LocalContext.current
    @Suppress("UNUSED_VARIABLE") val rev = Store.rev.intValue
    val bars by rememberInfiniteTransition(label = "m").animateFloat(0f, 1f, infiniteRepeatable(tween(900, easing = LinearEasing), RepeatMode.Reverse), label = "m")
    Panel(Modifier.fillMaxWidth()) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Title("Musique douce", 20.sp)
                Body(
                    (Music.tracks.first { it.id == Store.track }.name) + " · " + (Music.natures.first { it.id == Store.nature }.name),
                    size = 13.sp,
                )
            }
            if (Music.playing) Canvas(Modifier.size(28.dp, 22.dp)) {
                val hs = listOf(0.5f + 0.5f * bars, 1f - 0.6f * bars, 0.3f + 0.6f * bars)
                hs.forEachIndexed { i, h ->
                    val bw = size.width / 5
                    drawRoundRect(C.Yellow, Offset(i * bw * 2, size.height * (1 - h)), Size(bw, size.height * h), androidx.compose.ui.geometry.CornerRadius(bw / 2))
                }
            }
            Spacer(Modifier.width(12.dp))
            Box(
                Modifier.size(54.dp).clip(CircleShape).background(C.Yellow).clickable { Music.toggle(ctx) },
                contentAlignment = Alignment.Center,
            ) { Icon(if (Music.playing) Icons.Rounded.Pause else Icons.Rounded.PlayArrow, null, tint = Color.Black, modifier = Modifier.size(30.dp)) }
        }
        Spacer(Modifier.height(14.dp))
        Body("Mélodie", size = 12.sp, weight = FontWeight.Bold)
        Spacer(Modifier.height(6.dp))
        SoundRow(Music.tracks, Store.track) { Music.select(ctx, trackId = it) }
        VolumeSlider(Store.musicVol) { Music.setVolumes(m = it) }
        Body("Ambiance", size = 12.sp, weight = FontWeight.Bold)
        Spacer(Modifier.height(6.dp))
        SoundRow(Music.natures, Store.nature) { Music.select(ctx, natureId = it) }
        VolumeSlider(Store.natureVol) { Music.setVolumes(n = it) }
    }
}

@Composable
private fun SoundRow(list: List<Sound>, selected: String, onPick: (String) -> Unit) {
    Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        list.forEach { s -> Chip("${s.emoji} ${s.name}", s.id == selected) { onPick(s.id) } }
    }
}

@Composable
private fun VolumeSlider(v: Float, onChange: (Float) -> Unit) {
    Slider(
        v, onChange, Modifier.fillMaxWidth(),
        colors = SliderDefaults.colors(thumbColor = C.Yellow, activeTrackColor = C.Yellow, inactiveTrackColor = C.Surface2),
    )
}

@Composable
private fun WeekPanel() {
    val days = (6 downTo 0).map { LocalDate.now().minusDays(it.toLong()) }
    val values = days.map { Store.minutesOn(it) }
    val max = (values.maxOrNull() ?: 0).coerceAtLeast(30)
    Panel(Modifier.fillMaxWidth()) {
        Row {
            Title("Cette semaine", 18.sp)
            Spacer(Modifier.weight(1f))
            Body("${values.sum()} min · ${Store.sessionsCount} sessions", size = 12.sp)
        }
        Spacer(Modifier.height(14.dp))
        Row(Modifier.fillMaxWidth().height(110.dp), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.Bottom) {
            days.forEachIndexed { i, d ->
                val today = i == 6
                Column(Modifier.weight(1f), horizontalAlignment = Alignment.CenterHorizontally) {
                    if (values[i] > 0) Body("${values[i]}", size = 10.sp, color = if (today) C.Yellow else C.Muted)
                    Box(
                        Modifier.width(18.dp).height((6 + 70f * values[i] / max).dp)
                            .clip(RoundedCornerShape(6.dp)).background(if (today) C.Yellow else if (values[i] > 0) C.Yellow.copy(alpha = 0.45f) else C.Surface2),
                    )
                    Spacer(Modifier.height(6.dp))
                    Body(
                        d.dayOfWeek.getDisplayName(JTextStyle.SHORT, Locale.FRENCH).take(3),
                        size = 11.sp, color = if (today) C.Text else C.Muted,
                    )
                }
            }
        }
    }
}

