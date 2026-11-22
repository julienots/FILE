package com.superessence.pulsar.ui

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.AllInclusive
import androidx.compose.material.icons.filled.Bedtime
import androidx.compose.material.icons.filled.Favorite
import androidx.compose.material.icons.filled.FavoriteBorder
import androidx.compose.material.icons.filled.Pause
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Repeat
import androidx.compose.material.icons.filled.Share
import androidx.compose.material.icons.filled.SkipNext
import androidx.compose.material.icons.filled.SkipPrevious
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
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
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.pulsar.engine.Family
import com.superessence.pulsar.engine.GenParams
import com.superessence.pulsar.engine.NOTE_NAMES
import com.superessence.pulsar.engine.Scale
import com.superessence.pulsar.engine.SectionType
import com.superessence.pulsar.engine.TrackId
import com.superessence.pulsar.player.Player
import com.superessence.pulsar.player.PlayerState
import com.superessence.pulsar.player.RadioMode
import kotlinx.coroutines.delay
import kotlin.math.roundToInt

private val TABS = listOf("Mixeur", "Effets", "Compo", "Minuteur", "Export")

@Composable
fun StudioScreen(st: PlayerState, initialTab: Int = 0, onBack: () -> Unit) {
    val song = st.song
    val (ca, cb) = song?.genre?.accents() ?: (P.chill to P.chill2)
    var tab by rememberSaveable { mutableIntStateOf(initialTab) }
    // rafraîchissement de la position (10 fois / s)
    var tick by remember { mutableLongStateOf(0L) }
    LaunchedEffect(Unit) { while (true) { delay(100); tick++ } }

    Box(Modifier.fillMaxSize().background(P.bg)) {
        Box(Modifier.fillMaxWidth().height(520.dp).background(Brush.verticalGradient(listOf(ca.copy(alpha = 0.35f), cb.copy(alpha = 0.08f), Color.Transparent))))
        Column(Modifier.fillMaxSize().statusBarsPadding().verticalScroll(rememberScrollState())) {
            Row(Modifier.fillMaxWidth().padding(horizontal = 6.dp), verticalAlignment = Alignment.CenterVertically) {
                IconButton(onBack) { Icon(Icons.AutoMirrored.Filled.ArrowBack, "Retour") }
                Column(Modifier.weight(1f), horizontalAlignment = Alignment.CenterHorizontally) {
                    Text(if (st.radio != RadioMode.OFF) "📻 RADIO ${if (st.radio == RadioMode.FAMILY) st.radioFamily.label.uppercase() else "GENRE"}" else "STUDIO", style = MaterialTheme.typography.labelSmall, color = P.dim)
                    Text(song?.genre?.let { "${it.emoji} ${it.name}" } ?: "", style = MaterialTheme.typography.titleMedium)
                }
                val fav = remember(st.revision, song) { Player.store.isFavorite(song) }
                IconButton({ Player.toggleFavorite() }) { Icon(if (fav) Icons.Filled.Favorite else Icons.Filled.FavoriteBorder, "Favori", tint = if (fav) P.mad else P.text) }
                val ctx = LocalContext.current
                IconButton({ ctx.startActivity(Player.shareIntent()) }) { Icon(Icons.Filled.Share, "Partager") }
            }

            Box(Modifier.fillMaxWidth().aspectRatio(1.15f), contentAlignment = Alignment.Center) {
                Visualizer(Player.engine, st.playing, ca, cb, Modifier.fillMaxSize())
                if (st.generating) CircularProgressIndicator(color = Color.White)
            }

            if (song != null) {
                tick
                val e = Player.engine
                val secIdx = e.currentSectionIndex
                val sec = song.sections.getOrNull(secIdx)
                Text(song.title, style = MaterialTheme.typography.headlineSmall, textAlign = TextAlign.Center, modifier = Modifier.fillMaxWidth().padding(horizontal = 20.dp), maxLines = 1, overflow = TextOverflow.Ellipsis)
                Row(Modifier.fillMaxWidth().padding(top = 8.dp), horizontalArrangement = Arrangement.Center) {
                    Chip("${e.bpm.roundToInt()} BPM")
                    Chip(song.keyName)
                    Chip(song.bars.getOrNull(e.barIndex)?.chordName ?: "")
                    Chip(sec?.type?.label ?: "", highlight = sec?.type == SectionType.DROP || sec?.type == SectionType.CHORUS, color = ca)
                }
                Timeline(st, ca, cb)
                Transport(st, ca, cb)
                ActionRow(st, ca)
            }

            Row(Modifier.fillMaxWidth().padding(horizontal = 12.dp, vertical = 8.dp).clip(RoundedCornerShape(14.dp)).background(P.surface).padding(4.dp)) {
                TABS.forEachIndexed { i, t ->
                    Box(Modifier.weight(1f).clip(RoundedCornerShape(10.dp)).background(if (tab == i) ca else Color.Transparent).clickable { tab = i }.padding(vertical = 10.dp), contentAlignment = Alignment.Center) {
                        Text(t, fontSize = 12.sp, fontWeight = FontWeight.Bold, color = if (tab == i) Color.Black else P.dim, maxLines = 1)
                    }
                }
            }
            Column(Modifier.navigationBarsPadding().padding(horizontal = 16.dp).padding(bottom = 40.dp)) {
                when (tab) {
                    0 -> MixerTab(ca, tick)
                    1 -> FxTab(ca)
                    2 -> CompoTab(st, ca)
                    3 -> SleepTab(st, ca)
                    4 -> ExportTab(st, ca)
                }
            }
        }
    }
}

@Composable
private fun Chip(text: String, highlight: Boolean = false, color: Color = P.surface2) {
    if (text.isEmpty()) return
    Box(Modifier.padding(3.dp).clip(RoundedCornerShape(10.dp)).background(if (highlight) color else P.surface2).padding(horizontal = 10.dp, vertical = 5.dp)) {
        Text(text, fontSize = 12.sp, fontWeight = FontWeight.Bold, color = if (highlight) Color.Black else P.text, maxLines = 1)
    }
}

@Composable
private fun Timeline(st: PlayerState, ca: Color, cb: Color) {
    val song = st.song ?: return
    val e = Player.engine
    val total = song.bars.size.toFloat()
    val pos = (e.barIndex + (e.stepPos / 16.0).toFloat()) / total
    val bpm = e.bpm
    val secPerBar = 4f * 60f / bpm
    Column(Modifier.padding(horizontal = 20.dp, vertical = 12.dp)) {
        Canvas(
            Modifier.fillMaxWidth().height(34.dp).pointerInput(song) {
                detectTapGestures { o ->
                    val bar = (o.x / size.width * total).toInt()
                    val idx = song.sections.indexOfLast { it.startBar <= bar }
                    Player.engine.jumpToSection(idx)
                }
            },
        ) {
            val gap = 3f
            song.sections.forEach { s ->
                val x0 = s.startBar / total * size.width
                val w = s.bars / total * size.width - gap
                val h = size.height * (0.35f + 0.65f * s.type.energy / 4f)
                val col = when (s.type) { SectionType.DROP -> ca; SectionType.CHORUS -> ca.copy(alpha = 0.8f); SectionType.BUILD -> cb.copy(alpha = 0.8f); else -> Color.White.copy(alpha = 0.22f) }
                drawRoundRect(col, Offset(x0, size.height - h), Size(w, h), CornerRadius(6f, 6f))
            }
            val x = pos * size.width
            drawRect(Color.White, Offset(x - 1.5f, 0f), Size(3f, size.height))
        }
        Row(Modifier.fillMaxWidth().padding(top = 4.dp)) {
            Text(fmt((e.barIndex + e.stepPos / 16.0).toFloat() * secPerBar), fontSize = 11.sp, color = P.dim)
            Spacer(Modifier.weight(1f))
            Text("Touchez une section pour y sauter", fontSize = 11.sp, color = P.dim)
            Spacer(Modifier.weight(1f))
            Text(fmt(total * secPerBar), fontSize = 11.sp, color = P.dim)
        }
    }
}

private fun fmt(sec: Float): String { val s = sec.toInt(); return "%d:%02d".format(s / 60, s % 60) }

@Composable
private fun Transport(st: PlayerState, ca: Color, cb: Color) {
    Row(Modifier.fillMaxWidth().padding(vertical = 4.dp), horizontalArrangement = Arrangement.Center, verticalAlignment = Alignment.CenterVertically) {
        IconButton({ Player.previous() }, Modifier.size(56.dp)) { Icon(Icons.Filled.SkipPrevious, "Précédent", Modifier.size(34.dp)) }
        Spacer(Modifier.width(18.dp))
        val scale by animateFloatAsState(if (st.playing) 1f else 0.94f, label = "s")
        Box(
            Modifier.size((78 * scale).dp).clip(CircleShape).background(Brush.linearGradient(listOf(ca, cb))).clickable { Player.toggle() },
            contentAlignment = Alignment.Center,
        ) { Icon(if (st.playing) Icons.Filled.Pause else Icons.Filled.PlayArrow, "Lecture", tint = Color.Black, modifier = Modifier.size(44.dp)) }
        Spacer(Modifier.width(18.dp))
        IconButton({ Player.next() }, Modifier.size(56.dp)) { Icon(Icons.Filled.SkipNext, "Nouveau morceau", Modifier.size(34.dp)) }
    }
}

@Composable
private fun ActionRow(st: PlayerState, ca: Color) {
    val mad = st.song?.genre?.family == Family.MALADE
    Row(Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Pill(if (mad) "💥 DROP !" else "✨ Refrain", true, ca, Modifier.weight(1.2f)) { Player.engine.jumpToNextDrop() }
        var loop by remember { mutableStateOf(Player.live.loopSection) }
        Pill("Boucle", loop, ca, Modifier.weight(1f), Icons.Filled.Repeat) { loop = !loop; Player.live.loopSection = loop }
        Pill("Radio", st.radio != RadioMode.OFF, ca, Modifier.weight(1f), Icons.Filled.AllInclusive) {
            Player.setRadio(if (st.radio == RadioMode.OFF) RadioMode.GENRE else RadioMode.OFF)
        }
    }
}

@Composable
private fun Pill(text: String, on: Boolean, color: Color, modifier: Modifier, icon: androidx.compose.ui.graphics.vector.ImageVector? = null, onClick: () -> Unit) {
    Row(
        modifier.height(44.dp).clip(RoundedCornerShape(14.dp)).background(if (on) color.copy(alpha = 0.9f) else P.surface).border(1.dp, if (on) color else P.line, RoundedCornerShape(14.dp)).clickable(onClick = onClick),
        horizontalArrangement = Arrangement.Center, verticalAlignment = Alignment.CenterVertically,
    ) {
        if (icon != null) { Icon(icon, null, tint = if (on) Color.Black else P.text, modifier = Modifier.size(18.dp)); Spacer(Modifier.width(6.dp)) }
        Text(text, fontWeight = FontWeight.Black, fontSize = 13.sp, color = if (on) Color.Black else P.text)
    }
}

// ─────────────────────────────── Onglets ───────────────────────────────

@Composable
private fun MixerTab(ca: Color, tick: Long) {
    val live = Player.live
    var rev by remember { mutableIntStateOf(0) }
    rev
    TrackId.entries.forEach { t ->
        val i = t.ordinal
        Row(Modifier.fillMaxWidth().padding(vertical = 2.dp), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.width(92.dp)) {
                Text(t.label, fontSize = 13.sp, fontWeight = FontWeight.Bold, maxLines = 1, overflow = TextOverflow.Ellipsis)
                tick
                val lvl = (Player.engine.meters[i] * 1.6f).coerceIn(0f, 1f)
                Box(Modifier.padding(top = 3.dp).fillMaxWidth().height(4.dp).clip(RoundedCornerShape(2.dp)).background(P.surface2)) {
                    Box(Modifier.fillMaxWidth(lvl).fillMaxHeight().background(if (lvl > 0.9f) P.mad else ca))
                }
            }
            var v by remember { mutableFloatStateOf(live.volume[i]) }
            Slider(v, { v = it; live.volume[i] = it }, Modifier.weight(1f).padding(horizontal = 6.dp), valueRange = 0f..1.2f, onValueChangeFinished = { Player.saveMixer() }, colors = sliderColors(ca))
            Toggle("M", live.mute[i], P.mad) { live.mute[i] = !live.mute[i]; rev++ }
            Spacer(Modifier.width(6.dp))
            Toggle("S", live.solo[i], P.mad2) { live.solo[i] = !live.solo[i]; rev++ }
        }
    }
    Text("M = couper la piste · S = écouter la piste seule. « Ambiance » règle le vinyle et la pluie.", color = P.dim, fontSize = 12.sp, modifier = Modifier.padding(top = 8.dp))
}

@Composable
private fun Toggle(label: String, on: Boolean, color: Color, onClick: () -> Unit) {
    Box(Modifier.size(34.dp).clip(RoundedCornerShape(9.dp)).background(if (on) color else P.surface2).clickable(onClick = onClick), contentAlignment = Alignment.Center) {
        Text(label, fontWeight = FontWeight.Black, color = if (on) Color.Black else P.dim)
    }
}

@Composable
fun sliderColors(c: Color) = SliderDefaults.colors(thumbColor = Color.White, activeTrackColor = c, inactiveTrackColor = P.surface2)

@Composable
private fun LabeledSlider(label: String, value: Float, range: ClosedFloatingPointRange<Float>, ca: Color, display: (Float) -> String, onDone: () -> Unit = { Player.saveMixer() }, onChange: (Float) -> Unit) {
    var v by remember(value) { mutableFloatStateOf(value) }
    Column(Modifier.padding(vertical = 2.dp)) {
        Row {
            Text(label, fontWeight = FontWeight.Bold, fontSize = 14.sp)
            Spacer(Modifier.weight(1f))
            Text(display(v), color = P.dim, fontSize = 13.sp)
        }
        Slider(v, { v = it; onChange(it) }, valueRange = range, onValueChangeFinished = onDone, colors = sliderColors(ca))
    }
}

private fun pct(v: Float) = "${(v * 100).roundToInt()} %"

@Composable
private fun FxTab(ca: Color) {
    val l = Player.live
    val g = Player.state.value.song?.genre
    var reset by remember { mutableIntStateOf(0) }
    key(reset) {
        LabeledSlider("Filtre DJ", l.djFilter, -1f..1f, ca, { when { it < -0.03f -> "Passe-bas ${pct(-it)}"; it > 0.03f -> "Passe-haut ${pct(it)}"; else -> "Neutre" } }, onDone = {}) {
            l.djFilter = if (kotlin.math.abs(it) < 0.04f) 0f else it
        }
        LabeledSlider("Réverbération", l.reverb, 0f..2f, ca, ::pct) { l.reverb = it }
        LabeledSlider("Écho (délai)", l.delay, 0f..2f, ca, ::pct) { l.delay = it }
        LabeledSlider("Saturation", l.drive, 0f..3f, ca, ::pct) { l.drive = it }
        LabeledSlider("Pompe (sidechain)", l.sidechain, 0f..2f, ca, ::pct) { l.sidechain = it }
        LabeledSlider("Couleur lo-fi", l.lofi, 0f..2.5f, ca, ::pct) { l.lofi = it }
        LabeledSlider("Craquements vinyle", if (l.vinyl >= 0f) l.vinyl else (g?.vinyl ?: 0f), 0f..1.5f, ca, ::pct) { l.vinyl = it }
        LabeledSlider("Pluie", if (l.rain >= 0f) l.rain else (g?.rain ?: 0f), 0f..1.5f, ca, ::pct) { l.rain = it }
        LabeledSlider("Largeur stéréo", l.width, 0f..2f, ca, ::pct) { l.width = it }
        LabeledSlider("Volume général", l.masterVolume, 0f..1.2f, ca, ::pct) { l.masterVolume = it }
    }
    Box(Modifier.padding(top = 8.dp).fillMaxWidth().clip(RoundedCornerShape(14.dp)).border(1.dp, P.line, RoundedCornerShape(14.dp)).clickable {
        l.djFilter = 0f; l.reverb = 1f; l.delay = 1f; l.drive = 1f; l.sidechain = 1f; l.lofi = 1f; l.vinyl = -1f; l.rain = -1f; l.width = 1f
        Player.saveMixer(); reset++
    }.padding(14.dp), contentAlignment = Alignment.Center) { Text("Réinitialiser les effets du style", fontWeight = FontWeight.Bold) }
}

@Composable
private fun key(k: Any, content: @Composable () -> Unit) = androidx.compose.runtime.key(k) { content() }

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun CompoTab(st: PlayerState, ca: Color) {
    val song = st.song ?: return
    val g = song.genre
    val base = st.params ?: return
    var bpm by remember(song) { mutableFloatStateOf(Player.engine.bpm) }
    var root by remember(song) { mutableStateOf(base.root) }
    var scale by remember(song) { mutableStateOf(base.scale) }
    var energy by remember(song) { mutableFloatStateOf(base.energy) }
    var cx by remember(song) { mutableFloatStateOf(base.complexity) }
    var swing by remember(song) { mutableFloatStateOf(base.swing ?: g.swing) }
    var length by remember(song) { mutableFloatStateOf(base.length) }
    var lead by remember(song) { mutableStateOf(base.withLead) }

    Text("Tempo en direct", fontWeight = FontWeight.Bold)
    Row(verticalAlignment = Alignment.CenterVertically) {
        Slider(bpm, { bpm = it; Player.live.bpm = it }, Modifier.weight(1f), valueRange = (g.bpmMin - 30f).coerceAtLeast(50f)..(g.bpmMax + 30f), colors = sliderColors(ca))
        Text("${bpm.roundToInt()}", fontWeight = FontWeight.Black, modifier = Modifier.width(44.dp), textAlign = TextAlign.End)
    }
    Text("Code : ${song.code}", color = P.dim, fontSize = 12.sp)

    Spacer(Modifier.height(14.dp))
    Text("Tonalité", fontWeight = FontWeight.Bold)
    FlowRow(Modifier.padding(vertical = 6.dp)) {
        SelChip("Auto", root == null, ca) { root = null }
        NOTE_NAMES.forEachIndexed { i, n -> SelChip(n, root == i, ca) { root = i } }
    }
    Text("Gamme", fontWeight = FontWeight.Bold)
    FlowRow(Modifier.padding(vertical = 6.dp)) {
        SelChip("Auto", scale == null, ca) { scale = null }
        Scale.entries.forEach { s -> SelChip(s.label, scale == s, ca) { scale = s } }
    }
    LabeledSlider("Énergie", energy, 0f..1f, ca, { if (it < 0.33f) "Douce" else if (it < 0.7f) "Normale" else "Explosive" }, onDone = {}) { energy = it }
    LabeledSlider("Complexité", cx, 0f..1f, ca, { if (it < 0.33f) "Simple" else if (it < 0.7f) "Riche" else "Folle" }, onDone = {}) { cx = it }
    LabeledSlider("Swing / groove", swing, 0f..0.8f, ca, ::pct, onDone = {}) { swing = it }
    LabeledSlider("Longueur", length, 0.5f..2f, ca, { if (it < 0.8f) "Courte" else if (it < 1.3f) "Normale" else "Longue" }, onDone = {}) { length = it }
    Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.padding(vertical = 6.dp)) {
        Text("Mélodie principale", fontWeight = FontWeight.Bold, modifier = Modifier.weight(1f))
        Switch(lead, { lead = it }, colors = SwitchDefaults.colors(checkedTrackColor = ca, checkedThumbColor = Color.Black))
    }
    fun params(seed: Long) = GenParams(g.id, seed, bpm = Player.engine.bpm, root = root, scale = scale, energy = energy, complexity = cx, swing = swing, length = length, withLead = lead)
    Row(Modifier.padding(top = 10.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
        BigButton("Appliquer (même morceau)", P.surface2, P.text, Modifier.weight(1f)) { Player.regenerate(params(base.seed)) }
        BigButton("Nouveau morceau 🎲", ca, Color.Black, Modifier.weight(1f)) { Player.regenerate(params(Player.newSeed())) }
    }
}

@Composable
private fun SelChip(text: String, on: Boolean, c: Color, onClick: () -> Unit) {
    Box(Modifier.padding(3.dp).clip(RoundedCornerShape(10.dp)).background(if (on) c else P.surface2).clickable(onClick = onClick).padding(horizontal = 11.dp, vertical = 7.dp)) {
        Text(text, fontSize = 13.sp, fontWeight = FontWeight.Bold, color = if (on) Color.Black else P.text)
    }
}

@Composable
fun BigButton(text: String, bg: Color, fg: Color, modifier: Modifier = Modifier, onClick: () -> Unit) {
    Box(modifier.height(52.dp).clip(RoundedCornerShape(16.dp)).background(bg).clickable(onClick = onClick).padding(horizontal = 10.dp), contentAlignment = Alignment.Center) {
        Text(text, color = fg, fontWeight = FontWeight.Black, fontSize = 14.sp, textAlign = TextAlign.Center, maxLines = 2)
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun SleepTab(st: PlayerState, ca: Color) {
    var now by remember { mutableLongStateOf(System.currentTimeMillis()) }
    LaunchedEffect(st.sleepEndAt) { while (true) { now = System.currentTimeMillis(); delay(500) } }
    Row(verticalAlignment = Alignment.CenterVertically) {
        Icon(Icons.Filled.Bedtime, null, tint = ca)
        Spacer(Modifier.width(8.dp))
        Text("Minuteur de sommeil", style = MaterialTheme.typography.titleMedium)
    }
    Text("La musique baisse doucement pendant les 30 dernières secondes puis s'arrête, même écran éteint.", color = P.dim, fontSize = 13.sp, modifier = Modifier.padding(vertical = 6.dp))
    when {
        st.sleepEndAt > 0L -> {
            val left = ((st.sleepEndAt - now) / 1000).coerceAtLeast(0)
            Text("%d:%02d".format(left / 60, left % 60), fontSize = 54.sp, fontWeight = FontWeight.Black, color = ca, modifier = Modifier.fillMaxWidth(), textAlign = TextAlign.Center)
            Text("avant l'arrêt", color = P.dim, modifier = Modifier.fillMaxWidth(), textAlign = TextAlign.Center)
        }
        st.sleepAtSongEnd -> Text("Arrêt à la fin du morceau", fontSize = 22.sp, fontWeight = FontWeight.Black, color = ca, modifier = Modifier.fillMaxWidth().padding(16.dp), textAlign = TextAlign.Center)
    }
    FlowRow(Modifier.padding(vertical = 10.dp)) {
        listOf(5, 10, 15, 20, 30, 45, 60, 90, 120).forEach { m -> SelChip(if (m >= 60) "${m / 60} h${if (m % 60 > 0) " ${m % 60}" else ""}" else "$m min", false, ca) { Player.startSleep(m); if (!st.playing) Player.play() } }
        SelChip("Fin du morceau", st.sleepAtSongEnd, ca) { Player.sleepAtSongEnd() }
    }
    if (st.sleepEndAt > 0L || st.sleepAtSongEnd) BigButton("Annuler le minuteur", P.surface2, P.text, Modifier.fillMaxWidth()) { Player.cancelSleep() }
    Text("Astuce : l'appli SLEEPCUT (fournie avec PULSAR) coupe aussi Spotify, YouTube, Deezer…", color = P.dim, fontSize = 12.sp, modifier = Modifier.padding(top = 12.dp))
}

@Composable
private fun ExportTab(st: PlayerState, ca: Color) {
    val song = st.song ?: return
    Text("Exporter en fichier audio", style = MaterialTheme.typography.titleMedium)
    Text("Le morceau complet (${fmt(song.durationSeconds(Player.engine.bpm))}) est rendu en WAV haute qualité avec ton mixage et tes effets, dans Musique/Pulsar.", color = P.dim, fontSize = 13.sp, modifier = Modifier.padding(vertical = 6.dp))
    if (st.exportProgress >= 0f) {
        LinearProgressIndicator(progress = { st.exportProgress }, modifier = Modifier.fillMaxWidth().padding(vertical = 12.dp), color = ca, trackColor = P.surface2)
        Text("Rendu en cours… ${(st.exportProgress * 100).roundToInt()} %", color = P.dim)
    } else {
        BigButton("⬇  Exporter « ${song.title} »", ca, Color.Black, Modifier.fillMaxWidth().padding(top = 8.dp)) { Player.export() }
    }
    st.lastExport?.let {
        Text("✅ Enregistré : $it", color = P.good, modifier = Modifier.padding(top = 12.dp))
    }
    val ctx = LocalContext.current
    BigButton("Partager le code du morceau", P.surface2, P.text, Modifier.fillMaxWidth().padding(top = 12.dp)) { ctx.startActivity(Player.shareIntent()) }
    Text("Code : ${song.code} — n'importe qui avec PULSAR peut réécouter exactement ce morceau.", color = P.dim, fontSize = 12.sp, modifier = Modifier.padding(top = 8.dp))
}
