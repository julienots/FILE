package com.superessence.voxea.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.ArrowBack
import androidx.compose.material.icons.outlined.CheckCircle
import androidx.compose.material.icons.outlined.Edit
import androidx.compose.material.icons.outlined.FolderZip
import androidx.compose.material.icons.outlined.GraphicEq
import androidx.compose.material.icons.outlined.Mic
import androidx.compose.material.icons.outlined.PlayArrow
import androidx.compose.material.icons.outlined.Share
import androidx.compose.material.icons.outlined.Stop
import androidx.compose.material.icons.outlined.Tune
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Switch
import androidx.compose.material3.Tab
import androidx.compose.material3.TabRow
import androidx.compose.material3.TabRowDefaults
import androidx.compose.material3.TabRowDefaults.tabIndicatorOffset
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.superessence.voxea.core.Line
import com.superessence.voxea.core.VoiceSettings
import com.superessence.voxea.core.VoiceType
import com.superessence.voxea.data.Project
import java.io.File
import kotlin.math.roundToInt

@Composable
fun EditScreen(vm: AppViewModel, id: String, tab: Int) {
    val projects by vm.projects.collectAsState()
    val p = projects.firstOrNull { it.id == id } ?: return
    val rv by vm.recVersion.collectAsState()
    val recorded = remember(rv, p) { vm.store.recorded(p) }
    var rename by remember { mutableStateOf(false) }
    var editScript by remember { mutableStateOf(false) }

    Column(Modifier.fillMaxSize().background(Vx.Bg)) {
        Row(Modifier.fillMaxWidth().padding(start = 4.dp, end = 4.dp, top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
            IconButton(onClick = { vm.back() }) { Icon(Icons.AutoMirrored.Outlined.ArrowBack, "Retour") }
            Column(Modifier.weight(1f).clickable { rename = true }) {
                Text(p.name, style = MaterialTheme.typography.titleLarge, maxLines = 1, overflow = TextOverflow.Ellipsis)
                Text("${recorded.size}/${p.lines.size} répliques enregistrées", style = MaterialTheme.typography.bodySmall, color = Vx.Muted)
            }
            IconButton(onClick = { editScript = true }) { Icon(Icons.Outlined.Edit, "Modifier le script", tint = Vx.Muted) }
        }
        TabRow(
            tab, containerColor = Vx.Bg, contentColor = Vx.Cream,
            indicator = { pos -> TabRowDefaults.SecondaryIndicator(Modifier.tabIndicatorOffset(pos[tab]), color = Vx.Magenta) },
        ) {
            listOf("1 · Voix", "2 · Enregistrer", "3 · MP3").forEachIndexed { i, t ->
                Tab(tab == i, { vm.go(Screen.Edit(p.id, i)) }, text = { Text(t, style = MaterialTheme.typography.labelLarge) })
            }
        }
        when (tab) {
            0 -> CastTab(vm, p, recorded)
            1 -> RecordTab(vm, p, recorded)
            else -> ExportTab(vm, p, recorded)
        }
    }

    if (rename) {
        var name by remember { mutableStateOf(p.name) }
        AlertDialog(
            onDismissRequest = { rename = false }, title = { Text("Nom du dialogue") },
            text = { OutlinedTextField(name, { name = it }, singleLine = true) },
            confirmButton = { TextButton({ vm.rename(p, name); rename = false }) { Text("OK") } },
            dismissButton = { TextButton({ rename = false }) { Text("Annuler") } },
        )
    }
    if (editScript) {
        var raw by remember { mutableStateOf(p.rawScript) }
        AlertDialog(
            onDismissRequest = { editScript = false }, title = { Text("Modifier le script") },
            text = {
                Column {
                    Text("Attention : si l'ordre des répliques change, réenregistre celles qui ont bougé.", style = MaterialTheme.typography.bodySmall, color = Vx.Muted)
                    OutlinedTextField(raw, { raw = it }, modifier = Modifier.fillMaxWidth().heightIn(min = 200.dp, max = 380.dp).padding(top = 8.dp))
                }
            },
            confirmButton = { TextButton({ vm.reparse(p, raw); editScript = false }) { Text("Enregistrer") } },
            dismissButton = { TextButton({ editScript = false }) { Text("Annuler") } },
        )
    }
}

// ---------------- 1. Voix ----------------

@Composable
private fun CastTab(vm: AppViewModel, p: Project, recorded: Set<Int>) {
    var lineFor by remember { mutableStateOf<Line?>(null) }
    val previewing by vm.previewing.collectAsState()
    LazyColumn(contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        item {
            Text("Choisis la voix de chaque personnage. Toutes sont fabriquées à partir de ta voix.",
                style = MaterialTheme.typography.bodyMedium, color = Vx.Muted)
        }
        items(p.characters, key = { "c_$it" }) { c -> CharacterCard(vm, p, c, previewing == c) }
        item {
            Text("Répliques", style = MaterialTheme.typography.titleMedium, modifier = Modifier.padding(top = 8.dp))
            Text("Touche une réplique pour lui donner une voix différente de son personnage.", style = MaterialTheme.typography.bodySmall, color = Vx.Muted)
        }
        items(p.lines, key = { "l_${it.index}" }) { l ->
            LineRow(p, l, l.index in recorded, onClick = { lineFor = l })
        }
        item {
            Spacer(Modifier.height(8.dp))
            BigAction(Icons.Outlined.Mic, "Passer à l'enregistrement", Vx.Magenta) { vm.go(Screen.Edit(p.id, 1)) }
        }
    }
    lineFor?.let { l ->
        AlertDialog(
            onDismissRequest = { lineFor = null },
            title = { Text("Réplique ${l.index + 1} · ${l.character}") },
            text = {
                Column {
                    Text("« ${l.text} »", style = MaterialTheme.typography.bodyMedium, color = Vx.Muted, maxLines = 4, overflow = TextOverflow.Ellipsis)
                    Spacer(Modifier.height(12.dp))
                    VoiceChips(p.lineVoice[l.index]) { vm.setLineVoice(p, l.index, it); lineFor = null }
                    TextButton({ vm.setLineVoice(p, l.index, null); lineFor = null }) { Text("Comme le personnage (${p.castType[l.character]?.short ?: "Ma voix"})") }
                }
            },
            confirmButton = { TextButton({ lineFor = null }) { Text("Fermer") } },
        )
    }
}

@Composable
private fun CharacterCard(vm: AppViewModel, p: Project, c: String, previewing: Boolean) {
    val type = p.castType[c] ?: VoiceType.ORIGINAL
    val s = p.settingsForCharacter(c)
    var fine by remember { mutableStateOf(false) }
    val count = p.lines.count { it.character == c }
    Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(20.dp)).background(Vx.Surface).padding(16.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Box(Modifier.size(42.dp).clip(CircleShape).background(type.color().copy(alpha = 0.18f)), contentAlignment = Alignment.Center) {
                Text(type.emoji(), style = MaterialTheme.typography.titleLarge)
            }
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text(c, style = MaterialTheme.typography.titleMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
                Text("$count réplique${if (count > 1) "s" else ""} · ${type.label}", style = MaterialTheme.typography.bodySmall, color = type.color())
            }
            IconButton(onClick = { vm.preview(p, c, s) }) {
                Icon(if (previewing) Icons.Outlined.Stop else Icons.Outlined.PlayArrow, "Aperçu", tint = Vx.Cyan)
            }
            IconButton(onClick = { fine = !fine }) { Icon(Icons.Outlined.Tune, "Réglages fins", tint = if (fine) Vx.Magenta else Vx.Muted) }
        }
        Spacer(Modifier.height(10.dp))
        VoiceChips(type) { vm.setCharacterVoice(p, c, it) }
        if (fine) {
            Spacer(Modifier.height(8.dp))
            LabeledSlider("Hauteur", "${if (s.pitch > 0) "+" else ""}${"%.1f".format(s.pitch)} demi-tons", s.pitch, -12f..12f) {
                vm.setCharacterSettings(p, c, s.copy(pitch = (it * 2).roundToInt() / 2f))
            }
            LabeledSlider("Timbre", when {
                s.formant > 1.03f -> "plus fin"; s.formant < 0.97f -> "plus ample"; else -> "naturel"
            } + " (${"%.2f".format(s.formant)})", s.formant, 0.7f..1.45f) {
                vm.setCharacterSettings(p, c, s.copy(formant = (it * 100).roundToInt() / 100f))
            }
            LabeledSlider("Vibrato", if (s.vibrato < 0.05f) "aucun" else "%.2f".format(s.vibrato), s.vibrato, 0f..1f) {
                vm.setCharacterSettings(p, c, s.copy(vibrato = (it * 20).roundToInt() / 20f))
            }
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text("Effet robot", Modifier.weight(1f), style = MaterialTheme.typography.bodyMedium)
                Switch(s.robot, { vm.setCharacterSettings(p, c, s.copy(robot = it)) })
            }
            Text("Écoute l'aperçu (▶) après chaque réglage. Il utilise ta première réplique enregistrée.", style = MaterialTheme.typography.bodySmall, color = Vx.Muted)
        }
    }
}

@Composable
private fun LabeledSlider(label: String, value: String, v: Float, range: ClosedFloatingPointRange<Float>, onChange: (Float) -> Unit) {
    Column(Modifier.padding(top = 4.dp)) {
        Row { Text(label, Modifier.weight(1f), style = MaterialTheme.typography.bodyMedium); Text(value, style = MaterialTheme.typography.bodySmall, color = Vx.Muted) }
        Slider(v.coerceIn(range.start, range.endInclusive), onChange, valueRange = range,
            colors = SliderDefaults.colors(thumbColor = Vx.Magenta, activeTrackColor = Vx.Magenta, inactiveTrackColor = Vx.Surface2))
    }
}

@OptIn(androidx.compose.foundation.layout.ExperimentalLayoutApi::class)
@Composable
fun VoiceChips(selected: VoiceType?, onSelect: (VoiceType) -> Unit) {
    androidx.compose.foundation.layout.FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        VoiceType.entries.forEach { t ->
            val sel = t == selected
            Row(
                Modifier.clip(RoundedCornerShape(50)).background(if (sel) t.color() else Vx.Surface2)
                    .border(1.dp, if (sel) t.color() else Vx.Line, RoundedCornerShape(50))
                    .clickable { onSelect(t) }.padding(horizontal = 12.dp, vertical = 9.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(t.emoji())
                Spacer(Modifier.width(6.dp))
                Text(t.short, style = MaterialTheme.typography.labelLarge, color = if (sel) Vx.Bg else Vx.Cream)
            }
        }
    }
}

@Composable
private fun LineRow(p: Project, l: Line, done: Boolean, onClick: () -> Unit, trailing: @Composable (() -> Unit)? = null) {
    val t = p.voiceTypeFor(l)
    Row(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).background(Vx.Surface.copy(alpha = 0.7f)).clickable(onClick = onClick)
            .padding(start = 12.dp, top = 10.dp, bottom = 10.dp, end = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text("%02d".format(l.index + 1), style = MaterialTheme.typography.bodySmall, color = Vx.Muted, modifier = Modifier.width(26.dp))
        Column(Modifier.weight(1f)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(l.character, style = MaterialTheme.typography.labelLarge, color = t.color(), maxLines = 1)
                Text("  ${t.emoji()} ${t.short}${if (p.lineVoice.containsKey(l.index)) " ★" else ""}", style = MaterialTheme.typography.bodySmall, color = Vx.Muted)
            }
            Text(l.text, style = MaterialTheme.typography.bodyMedium, maxLines = 2, overflow = TextOverflow.Ellipsis)
        }
        if (done) Icon(Icons.Outlined.CheckCircle, "Enregistrée", tint = Vx.Ok, modifier = Modifier.padding(horizontal = 6.dp).size(20.dp))
        trailing?.invoke()
    }
}

// ---------------- 2. Enregistrer ----------------

@Composable
private fun RecordTab(vm: AppViewModel, p: Project, recorded: Set<Int>) {
    val playing by vm.player.playing.collectAsState()
    LazyColumn(contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        item {
            Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(22.dp)).background(Vx.Surface).padding(18.dp)) {
                Text(if (recorded.isEmpty()) "Prêt à enregistrer ?" else if (recorded.size == p.lines.size) "Tout est enregistré ✓" else "On continue ?",
                    style = MaterialTheme.typography.titleLarge)
                Text("Le prompteur affiche chaque réplique en grand. Lis-la, puis touche l'écran (ou un bouton de volume) : " +
                    "la réplique est enregistrée et la suivante s'affiche, sans couper le micro.",
                    style = MaterialTheme.typography.bodyMedium, color = Vx.Muted, modifier = Modifier.padding(top = 4.dp, bottom = 14.dp))
                BigAction(Icons.Outlined.Mic, if (recorded.isEmpty()) "Ouvrir le prompteur" else "Reprendre au prompteur", Vx.Magenta) { vm.openPrompter(p) }
                if (recorded.isNotEmpty()) {
                    Spacer(Modifier.height(10.dp))
                    SmallAction(Icons.Outlined.Mic, "Tout reprendre depuis le début", Modifier.fillMaxWidth()) { vm.openPrompter(p, 0) }
                }
            }
        }
        item { Text("Touche ▶ pour réécouter, ou une réplique pour la refaire.", style = MaterialTheme.typography.bodySmall, color = Vx.Muted) }
        items(p.lines, key = { it.index }) { l ->
            val done = l.index in recorded
            val f = vm.store.recFile(p, l.index)
            LineRow(p, l, done, onClick = { vm.openPrompter(p, l.index) }) {
                if (done) IconButton(onClick = { vm.player.toggle(f) }) {
                    Icon(if (playing == f.absolutePath) Icons.Outlined.Stop else Icons.Outlined.PlayArrow, "Réécouter", tint = Vx.Cyan)
                }
            }
        }
    }
}

// ---------------- 3. MP3 ----------------

@Composable
private fun ExportTab(vm: AppViewModel, p: Project, recorded: Set<Int>) {
    val ex by vm.export.collectAsState()
    val ai by vm.aiEnabled.collectAsState()
    val playing by vm.player.playing.collectAsState()
    val ctx = LocalContext.current
    val hasKey = vm.settings.apiKey.isNotBlank()
    val existing = remember(ex.result, p) { vm.store.outDir(p).listFiles()?.filter { it.extension == "mp3" }?.sortedBy { it.name }.orEmpty() }
    val result = ex.result

    LazyColumn(contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        item {
            Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(22.dp)).background(Vx.Surface).padding(18.dp)) {
                Text("Générer les MP3", style = MaterialTheme.typography.titleLarge)
                Text("Un fichier par réplique (01_LEA.mp3, 02_MARC.mp3…) + le dialogue complet. Copiés dans Musique/VOXEA.",
                    style = MaterialTheme.typography.bodyMedium, color = Vx.Muted, modifier = Modifier.padding(top = 4.dp, bottom = 12.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text(if (ai) "Voix IA ElevenLabs" else "Transformation hors ligne", style = MaterialTheme.typography.titleMedium)
                        Text(if (hasKey) "Plus réaliste avec l'IA (Internet, compte ElevenLabs)." else "Ajoute une clé ElevenLabs dans Réglages pour l'option IA.",
                            style = MaterialTheme.typography.bodySmall, color = Vx.Muted)
                    }
                    Switch(ai, { vm.setUseAi(it) }, enabled = hasKey)
                }
                Spacer(Modifier.height(12.dp))
                if (recorded.size < p.lines.size) {
                    Text("⚠ ${p.lines.size - recorded.size} réplique(s) pas encore enregistrée(s) : elles seront ignorées.",
                        style = MaterialTheme.typography.bodySmall, color = Color(0xFFFFB547), modifier = Modifier.padding(bottom = 10.dp))
                }
                if (ex.running) {
                    Text(ex.label, style = MaterialTheme.typography.bodyMedium)
                    LinearProgressIndicator({ ex.progress }, Modifier.fillMaxWidth().padding(top = 8.dp).height(8.dp).clip(CircleShape),
                        color = Vx.Magenta, trackColor = Vx.Surface2, drawStopIndicator = {})
                } else {
                    BigAction(Icons.Outlined.GraphicEq, if (existing.isEmpty()) "Générer les MP3" else "Régénérer les MP3",
                        if (recorded.isEmpty()) Vx.Surface2 else Vx.Magenta) {
                        if (recorded.isEmpty()) vm.toast("Enregistre d'abord au moins une réplique.") else vm.generate(p)
                    }
                }
                ex.savedTo?.let { Text("✓ Copiés dans $it", color = Vx.Ok, style = MaterialTheme.typography.bodyMedium, modifier = Modifier.padding(top = 10.dp)) }
                ex.notes.forEach { Text(it, color = Color(0xFFFFB547), style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(top = 6.dp)) }
            }
        }
        if (existing.isNotEmpty() && !ex.running) {
            item {
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    SmallAction(Icons.Outlined.Share, "Tout partager", Modifier.weight(1f)) {
                        ctx.startActivity(vm.shareIntent(existing, "Envoyer les MP3"))
                    }
                    SmallAction(Icons.Outlined.FolderZip, "En ZIP", Modifier.weight(1f)) {
                        val r = result ?: com.superessence.voxea.core.Renderer.Result(existing.filter { !it.name.startsWith("00_") }, emptyList(), existing.firstOrNull { it.name.startsWith("00_") })
                        ctx.startActivity(vm.shareIntent(listOf(vm.zipFor(p, r)), "Envoyer le ZIP"))
                    }
                }
            }
            items(existing, key = { it.name }) { f -> Mp3Row(f, playing == f.absolutePath, onPlay = { vm.player.toggle(f) }) { ctx.startActivity(vm.shareIntent(listOf(f), f.name)) } }
        }
    }
}

@Composable
private fun Mp3Row(f: File, playing: Boolean, onPlay: () -> Unit, onShare: () -> Unit) {
    Row(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).background(Vx.Surface.copy(alpha = 0.7f)).padding(start = 6.dp, end = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        IconButton(onClick = onPlay) { Icon(if (playing) Icons.Outlined.Stop else Icons.Outlined.PlayArrow, "Écouter", tint = Vx.Cyan) }
        Column(Modifier.weight(1f)) {
            Text(f.name, style = MaterialTheme.typography.labelLarge, maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text("${(f.length() / 1024).coerceAtLeast(1)} Ko", style = MaterialTheme.typography.bodySmall, color = Vx.Muted)
        }
        IconButton(onClick = onShare) { Icon(Icons.Outlined.Share, "Partager", tint = Vx.Muted) }
    }
}

@Suppress("unused")
private fun VoiceSettings.describe() = "$pitch/$formant"
