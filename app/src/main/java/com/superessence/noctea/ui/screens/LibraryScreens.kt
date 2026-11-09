package com.superessence.noctea.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Alarm
import androidx.compose.material.icons.filled.ArrowDownward
import androidx.compose.material.icons.filled.ArrowUpward
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Save
import androidx.compose.material.icons.filled.Stop
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TimePicker
import androidx.compose.material3.TimePickerDefaults
import androidx.compose.material3.rememberTimePickerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import com.superessence.noctea.audio.MediaIds
import com.superessence.noctea.audio.PlayableKind
import com.superessence.noctea.data.MixPreset
import com.superessence.noctea.data.RitualStep
import com.superessence.noctea.data.Sounds
import com.superessence.noctea.notify.Reminders
import com.superessence.noctea.ui.LocalApp
import com.superessence.noctea.ui.LocalNav
import com.superessence.noctea.ui.Routes
import com.superessence.noctea.ui.components.CardData
import com.superessence.noctea.ui.components.GlassCard
import com.superessence.noctea.ui.components.NightBackground
import com.superessence.noctea.ui.components.NightChip
import com.superessence.noctea.ui.components.PrimaryButton
import com.superessence.noctea.ui.components.SecondaryButton
import com.superessence.noctea.ui.components.TrackRow
import com.superessence.noctea.ui.components.formatDuration
import com.superessence.noctea.ui.theme.LocalPalette
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.UUID

// =================================================================== FAVORIS
@Composable
fun FavoritesScreen() {
    val app = LocalApp.current
    val nav = LocalNav.current
    val snap by app.store.state.collectAsState()
    var tab by rememberSaveable { mutableStateOf(0) }
    val favs = snap.favorites.mapNotNull { app.resolver.resolve(it) }
    NightBackground {
        LazyColumn(Modifier.fillMaxSize(), contentPadding = TabContentPadding) {
            item {
                Column(Modifier.statusBarsPadding().padding(horizontal = 20.dp, vertical = 12.dp)) {
                    Text("Favoris", style = MaterialTheme.typography.displayMedium)
                    Text("Tes séances préférées, tes créations et ton historique.", style = MaterialTheme.typography.bodyMedium)
                }
                Row(Modifier.padding(horizontal = 20.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    NightChip("Favoris (${favs.size})", tab == 0) { tab = 0 }
                    NightChip("Créations", tab == 1) { tab = 1 }
                    NightChip("Historique", tab == 2) { tab = 2 }
                }
                Spacer(Modifier.height(10.dp))
            }
            when (tab) {
                0 -> {
                    if (favs.isEmpty()) item { EmptyState("Touche le cœur d'une séance pour la retrouver ici.\nTout le contenu de NOCTÉA est inclus dans l'application et disponible hors connexion.") }
                    items(favs, key = { it.mediaId }) { pl ->
                        TrackRow(
                            CardData(pl.mediaId, pl.title, pl.subtitle, pl.art, pl.durationMs, true),
                            onClick = { app.engine.play(pl.mediaId); nav.navigate(Routes.PLAYER) },
                            onFav = { app.store.toggleFavorite(pl.mediaId) },
                        )
                    }
                }
                1 -> {
                    val creations = snap.customs.map { MediaIds.custom(it.id) } + snap.texts.map { MediaIds.text(it.id) }
                    if (creations.isEmpty()) item {
                        EmptyState("Aucune création pour l'instant.")
                        Box(Modifier.fillMaxWidth(), contentAlignment = Alignment.Center) { SecondaryButton("Créer ma méditation") { nav.navigate(Routes.CREATE) } }
                    }
                    items(creations, key = { it }) { id ->
                        val pl = app.resolver.resolve(id) ?: return@items
                        TrackRow(
                            CardData(id, pl.title, pl.subtitle, pl.art, pl.durationMs, id in snap.favorites),
                            onClick = { app.engine.play(id); nav.navigate(Routes.PLAYER) },
                            onFav = { app.store.toggleFavorite(id) },
                        )
                    }
                }
                else -> {
                    if (snap.history.isEmpty()) item { EmptyState("Ton historique d'écoute apparaîtra ici.") }
                    items(snap.history.take(40).withIndex().toList(), key = { "h${it.index}" }) { (_, h) ->
                        HistoryRow(h.mediaId, h.title, h.at, h.listenedMs)
                    }
                }
            }
        }
    }
}

@Composable
private fun HistoryRow(mediaId: String, title: String, at: Long, listened: Long) {
    val app = LocalApp.current
    val nav = LocalNav.current
    val fmt = remember { SimpleDateFormat("EEEE d MMMM · HH:mm", Locale.FRANCE) }
    val pl = app.resolver.resolve(mediaId)
    Row(
        Modifier.fillMaxWidth().clickable(enabled = pl != null) { app.engine.play(mediaId); nav.navigate(Routes.PLAYER) }
            .padding(horizontal = 20.dp, vertical = 10.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(Modifier.weight(1f)) {
            Text(title, style = MaterialTheme.typography.titleSmall)
            Text(fmt.format(Date(at)) + if (listened > 60_000) " · ${formatDuration(listened)} écoutées" else "", style = MaterialTheme.typography.bodySmall)
        }
        if (pl != null) Icon(Icons.Filled.PlayArrow, "Réécouter", tint = LocalPalette.current.accent)
    }
}

@Composable
fun HistoryScreen() {
    val app = LocalApp.current
    val snap by app.store.state.collectAsState()
    SubScreen("Historique d'écoute") {
        if (snap.history.isEmpty()) EmptyState("Aucune écoute pour l'instant.")
        snap.history.take(100).forEach { HistoryRow(it.mediaId, it.title, it.at, it.listenedMs) }
        if (snap.history.isNotEmpty()) {
            Box(Modifier.fillMaxWidth().padding(20.dp), contentAlignment = Alignment.Center) {
                SecondaryButton("Effacer l'historique") { app.store.edit { it.copy(history = emptyList()) } }
            }
        }
    }
}

// =================================================================== MIXEUR
@Composable
fun MixerScreen() {
    val app = LocalApp.current
    val p = LocalPalette.current
    val snap by app.store.state.collectAsState()
    val ui by app.engine.ui.collectAsState()
    val mixerActive = ui.playable?.kind == PlayableKind.MIXER
    var levels by remember { mutableStateOf(snap.lastMix) }
    var saveDialog by remember { mutableStateOf(false) }
    val maxLayers = app.entitlements.maxMixerLayers()

    fun setLevel(id: String, v: Float) {
        val active = levels.filterValues { it > 0f }
        if (v > 0f && id !in active && active.size >= maxLayers) return
        levels = (levels + (id to v)).filterValues { it > 0f }
        app.store.edit { it.copy(lastMix = levels) }
        if (mixerActive) app.engine.setAmbienceLevel(id, v)
    }

    SubScreen("Mixeur nocturne") {
        Text("Compose ton environnement sonore en mélangeant jusqu'à $maxLayers ambiances.", style = MaterialTheme.typography.bodyMedium, modifier = Modifier.padding(horizontal = 20.dp))
        Spacer(Modifier.height(14.dp))
        Row(Modifier.padding(horizontal = 20.dp), horizontalArrangement = Arrangement.spacedBy(10.dp), verticalAlignment = Alignment.CenterVertically) {
            if (mixerActive && ui.isPlaying) {
                PrimaryButton("Arrêter", Modifier.weight(1f), icon = { Icon(Icons.Filled.Stop, null, tint = Color(0xFF1A1430)) }) { app.engine.stop() }
            } else {
                PrimaryButton("Écouter ce mélange", Modifier.weight(1f), enabled = levels.isNotEmpty(), icon = { Icon(Icons.Filled.PlayArrow, null, tint = Color(0xFF1A1430)) }) {
                    app.engine.startMixer()
                }
            }
            IconButton(onClick = { saveDialog = true }, enabled = levels.isNotEmpty()) { Icon(Icons.Filled.Save, "Enregistrer le mélange", tint = p.accent) }
        }
        if (mixerActive) Text("Astuce : règle un minuteur depuis le lecteur pour que le mélange s'estompe pendant ton sommeil.", style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(20.dp, 8.dp))

        if (snap.mixPresets.isNotEmpty()) {
            Text("Mes mélanges", style = MaterialTheme.typography.titleMedium, modifier = Modifier.padding(20.dp, 14.dp, 20.dp, 6.dp))
            snap.mixPresets.forEach { preset ->
                Row(Modifier.fillMaxWidth().padding(horizontal = 20.dp), verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        preset.name + " · " + preset.layers.keys.mapNotNull { Sounds.get(it)?.name }.joinToString(", "),
                        style = MaterialTheme.typography.bodyLarge,
                        modifier = Modifier.weight(1f).clip(RoundedCornerShape(10.dp)).clickable {
                            levels = preset.layers
                            app.store.edit { it.copy(lastMix = preset.layers) }
                            if (mixerActive) app.engine.applyMix(preset.layers.map { com.superessence.noctea.data.AmbienceLayer(it.key, it.value) }) else app.engine.startMixer()
                        }.padding(vertical = 10.dp),
                    )
                    IconButton(onClick = { app.store.edit { s -> s.copy(mixPresets = s.mixPresets.filter { it.id != preset.id }) } }) { Icon(Icons.Filled.Delete, "Supprimer", tint = p.textDim) }
                }
            }
        }

        listOf(Sounds.GROUP_NATURE, Sounds.GROUP_NOISE, Sounds.GROUP_MUSIC).forEach { group ->
            Text(group, style = MaterialTheme.typography.headlineSmall, modifier = Modifier.padding(20.dp, 18.dp, 20.dp, 4.dp))
            Sounds.all.filter { it.group == group }.forEach { s ->
                val v = levels[s.id] ?: 0f
                Row(Modifier.fillMaxWidth().padding(start = 20.dp, end = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                    Box(
                        Modifier.size(38.dp).clip(CircleShape)
                            .background(if (v > 0f) p.accent.copy(alpha = 0.35f) else Color.White.copy(alpha = 0.06f))
                            .border(1.dp, if (v > 0f) p.moon else p.cardBorder, CircleShape)
                            .clickable { setLevel(s.id, if (v > 0f) 0f else 0.5f) },
                        contentAlignment = Alignment.Center,
                    ) { Text(if (v > 0f) "♪" else "+", color = p.text) }
                    Column(Modifier.weight(1f)) {
                        LabeledSlider(s.name, v, if (v == 0f) s.description else "${(v * 100).toInt()} %") { setLevel(s.id, it) }
                    }
                }
            }
        }
        Spacer(Modifier.height(30.dp))
    }

    if (saveDialog) {
        var name by remember { mutableStateOf("Ma nuit") }
        AlertDialog(
            onDismissRequest = { saveDialog = false },
            title = { Text("Enregistrer le mélange") },
            text = { OutlinedTextField(name, { name = it.take(30) }, singleLine = true) },
            confirmButton = {
                TextButton(onClick = {
                    app.store.edit { it.copy(mixPresets = it.mixPresets + MixPreset(UUID.randomUUID().toString(), name.ifBlank { "Mélange" }, levels)) }
                    saveDialog = false
                }) { Text("Enregistrer") }
            },
            dismissButton = { TextButton(onClick = { saveDialog = false }) { Text("Annuler") } },
            containerColor = p.sky1,
        )
    }
}

// =================================================================== RITUEL
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun RitualScreen() {
    val app = LocalApp.current
    val nav = LocalNav.current
    val p = LocalPalette.current
    val ctx = LocalContext.current
    val snap by app.store.state.collectAsState()
    val ritual = snap.ritual
    var picker by remember { mutableStateOf<String?>(null) } // "bed" | "alarm"
    var addStep by remember { mutableStateOf(false) }
    val notifLauncher = androidx.activity.compose.rememberLauncherForActivityResult(
        androidx.activity.result.contract.ActivityResultContracts.RequestPermission(),
    ) { granted ->
        app.store.edit { it.copy(ritual = it.ritual.copy(reminderEnabled = granted)) }
        Reminders.reschedule(ctx)
    }
    val totalMin = ritual.steps.sumOf { it.minutes }

    fun stepTitle(s: RitualStep) = when (s.type) {
        "mixer" -> "Ambiance « ${Sounds.get(s.refId)?.name ?: "mon mélange"} » jusqu'à l'endormissement"
        "custom" -> snap.customs.firstOrNull { it.id == s.refId }?.title ?: "Ma méditation"
        else -> app.catalog.track(s.refId)?.title ?: s.refId
    }

    SubScreen("Mon rituel") {
        GlassCard(Modifier.fillMaxWidth().padding(horizontal = 20.dp)) {
            Text("Heure du coucher", style = MaterialTheme.typography.bodyMedium)
            Text(
                "%02d:%02d".format(ritual.bedtimeMinutes / 60, ritual.bedtimeMinutes % 60),
                style = MaterialTheme.typography.displayLarge,
                modifier = Modifier.clip(RoundedCornerShape(12.dp)).clickable { picker = "bed" },
            )
            val start = ((ritual.bedtimeMinutes - totalMin) % 1440 + 1440) % 1440
            Text("Commence ton rituel vers %02d:%02d · durée %d min".format(start / 60, start % 60, totalMin), style = MaterialTheme.typography.bodySmall)
        }
        SettingSwitch(
            "Rappel avant le coucher",
            "Une notification douce ${ritual.reminderOffsetMin} min avant l'heure du coucher",
            ritual.reminderEnabled,
        ) { on ->
            if (on && !Reminders.canNotify(ctx) && android.os.Build.VERSION.SDK_INT >= 33) {
                notifLauncher.launch(android.Manifest.permission.POST_NOTIFICATIONS)
            } else {
                app.store.edit { it.copy(ritual = it.ritual.copy(reminderEnabled = on)) }
                Reminders.reschedule(ctx)
            }
        }
        if (ritual.reminderEnabled) {
            Row(Modifier.padding(horizontal = 20.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                listOf(15, 30, 45, 60).forEach { m ->
                    NightChip("$m min", ritual.reminderOffsetMin == m) {
                        app.store.edit { it.copy(ritual = it.ritual.copy(reminderOffsetMin = m)) }
                        Reminders.reschedule(ctx)
                    }
                }
            }
        }

        Text("Étapes du rituel", style = MaterialTheme.typography.headlineSmall, modifier = Modifier.padding(20.dp, 22.dp, 20.dp, 8.dp))
        ritual.steps.forEachIndexed { i, s ->
            Row(Modifier.fillMaxWidth().padding(horizontal = 20.dp, vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.size(30.dp).clip(CircleShape).background(p.accent.copy(alpha = 0.3f)), contentAlignment = Alignment.Center) {
                    Text("${i + 1}", style = MaterialTheme.typography.labelLarge, color = p.text)
                }
                Spacer(Modifier.width(12.dp))
                Column(Modifier.weight(1f)) {
                    Text(stepTitle(s), style = MaterialTheme.typography.titleSmall)
                    Text("${s.minutes} min", style = MaterialTheme.typography.bodySmall)
                }
                IconButton(onClick = { app.store.edit { it.copy(ritual = it.ritual.copy(steps = it.ritual.steps.swap(i, i - 1))) } }, enabled = i > 0) {
                    Icon(Icons.Filled.ArrowUpward, "Monter", tint = p.textDim)
                }
                IconButton(onClick = { app.store.edit { it.copy(ritual = it.ritual.copy(steps = it.ritual.steps.swap(i, i + 1))) } }, enabled = i < ritual.steps.size - 1) {
                    Icon(Icons.Filled.ArrowDownward, "Descendre", tint = p.textDim)
                }
                IconButton(onClick = { app.store.edit { it.copy(ritual = it.ritual.copy(steps = it.ritual.steps.filterIndexed { j, _ -> j != i })) } }) {
                    Icon(Icons.Filled.Close, "Retirer", tint = p.textDim)
                }
            }
        }
        Row(Modifier.padding(horizontal = 20.dp, vertical = 8.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            SecondaryButton("+ Ajouter une étape") { addStep = true }
        }
        Spacer(Modifier.height(10.dp))
        PrimaryButton("Commencer mon rituel", Modifier.fillMaxWidth().padding(horizontal = 20.dp), enabled = ritual.steps.isNotEmpty()) {
            val ids = ritual.steps.map {
                when (it.type) {
                    "mixer" -> MediaIds.mixer(it.minutes, it.refId)
                    "custom" -> MediaIds.custom(it.refId)
                    else -> MediaIds.track(it.refId)
                }
            }
            app.store.edit { it.copy(ritualDone = it.ritualDone + java.time.LocalDate.now().toString()) }
            app.engine.play(ids, 0, resume = false)
            nav.navigate(Routes.PLAYER)
        }

        // ---------------------------------------------------------- réveil doux
        Text("Réveil doux", style = MaterialTheme.typography.headlineSmall, modifier = Modifier.padding(20.dp, 28.dp, 20.dp, 4.dp))
        Text("Un réveil progressif : piano, voix douce et invitation à s'étirer.", style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(horizontal = 20.dp))
        GlassCard(Modifier.fillMaxWidth().padding(20.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(Icons.Filled.Alarm, null, tint = p.moon)
                Spacer(Modifier.width(10.dp))
                Text(
                    "%02d:%02d".format(snap.alarm.minutes / 60, snap.alarm.minutes % 60),
                    style = MaterialTheme.typography.displayMedium,
                    modifier = Modifier.weight(1f).clip(RoundedCornerShape(12.dp)).clickable { picker = "alarm" },
                )
                androidx.compose.material3.Switch(
                    checked = snap.alarm.enabled,
                    onCheckedChange = { on ->
                        if (on && !Reminders.canScheduleExact(ctx) && android.os.Build.VERSION.SDK_INT >= 31) {
                            runCatching {
                                ctx.startActivity(android.content.Intent(android.provider.Settings.ACTION_REQUEST_SCHEDULE_EXACT_ALARM, android.net.Uri.parse("package:" + ctx.packageName)))
                            }
                        }
                        app.store.edit { it.copy(alarm = it.alarm.copy(enabled = on)) }
                        Reminders.reschedule(ctx)
                    },
                )
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                listOf(1 to "L", 2 to "M", 3 to "M", 4 to "J", 5 to "V", 6 to "S", 7 to "D").forEach { (d, l) ->
                    val on = d in snap.alarm.days
                    Box(
                        Modifier.size(34.dp).clip(CircleShape).background(if (on) p.accent else Color.White.copy(alpha = 0.06f))
                            .clickable {
                                app.store.edit { it.copy(alarm = it.alarm.copy(days = if (on && it.alarm.days.size > 1) it.alarm.days - d else it.alarm.days + d)) }
                                Reminders.reschedule(ctx)
                            },
                        contentAlignment = Alignment.Center,
                    ) { Text(l, color = if (on) Color(0xFF15102C) else p.text, style = MaterialTheme.typography.labelLarge) }
                }
            }
            Spacer(Modifier.height(8.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                NightChip("Réveil doux", snap.alarm.trackId == "wake_gentle") { app.store.edit { it.copy(alarm = it.alarm.copy(trackId = "wake_gentle")) } }
                NightChip("Matin serein", snap.alarm.trackId == "wake_serene") { app.store.edit { it.copy(alarm = it.alarm.copy(trackId = "wake_serene")) } }
            }
            if (snap.alarm.enabled && !Reminders.canScheduleExact(ctx)) {
                Text("Autorise les « alarmes et rappels » pour NOCTÉA dans les réglages Android afin d'activer le réveil.", color = p.moon, style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(top = 8.dp))
            }
        }
        Spacer(Modifier.height(30.dp))
    }

    picker?.let { which ->
        val init = if (which == "bed") ritual.bedtimeMinutes else snap.alarm.minutes
        val state = rememberTimePickerState(init / 60, init % 60, is24Hour = true)
        AlertDialog(
            onDismissRequest = { picker = null },
            containerColor = p.sky1,
            title = { Text(if (which == "bed") "Heure du coucher" else "Heure du réveil") },
            text = { TimePicker(state, colors = TimePickerDefaults.colors(selectorColor = p.accent)) },
            confirmButton = {
                TextButton(onClick = {
                    val m = state.hour * 60 + state.minute
                    app.store.edit { if (which == "bed") it.copy(ritual = it.ritual.copy(bedtimeMinutes = m)) else it.copy(alarm = it.alarm.copy(minutes = m)) }
                    Reminders.reschedule(ctx)
                    picker = null
                }) { Text("Valider") }
            },
            dismissButton = { TextButton(onClick = { picker = null }) { Text("Annuler") } },
        )
    }

    if (addStep) {
        AlertDialog(
            onDismissRequest = { addStep = false },
            containerColor = p.sky1,
            title = { Text("Ajouter une étape") },
            text = {
                LazyColumn(Modifier.height(420.dp)) {
                    item { Text("Ambiance jusqu'à l'endormissement", style = MaterialTheme.typography.titleSmall, modifier = Modifier.padding(vertical = 6.dp)) }
                    items(Sounds.all.filter { it.group != Sounds.GROUP_NOISE || true }) { s ->
                        Text(
                            "${s.name} · 30 min", style = MaterialTheme.typography.bodyLarge,
                            modifier = Modifier.fillMaxWidth().clickable {
                                app.store.edit { it.copy(ritual = it.ritual.copy(steps = it.ritual.steps + RitualStep("mixer", s.id, 30))) }
                                addStep = false
                            }.padding(vertical = 8.dp),
                        )
                    }
                    if (snap.customs.isNotEmpty()) {
                        item { Text("Mes créations", style = MaterialTheme.typography.titleSmall, modifier = Modifier.padding(vertical = 6.dp)) }
                        items(snap.customs.take(10)) { c ->
                            Text(
                                "${c.title} · ${formatDuration(c.durationMs)}", style = MaterialTheme.typography.bodyLarge,
                                modifier = Modifier.fillMaxWidth().clickable {
                                    app.store.edit { it.copy(ritual = it.ritual.copy(steps = it.ritual.steps + RitualStep("custom", c.id, (c.durationMs / 60_000).toInt()))) }
                                    addStep = false
                                }.padding(vertical = 8.dp),
                            )
                        }
                    }
                    item { Text("Bibliothèque", style = MaterialTheme.typography.titleSmall, modifier = Modifier.padding(vertical = 6.dp)) }
                    items(app.catalog.tracks) { t ->
                        Text(
                            "${t.title} · ${formatDuration(t.durationMs)}", style = MaterialTheme.typography.bodyLarge,
                            modifier = Modifier.fillMaxWidth().clickable {
                                app.store.edit { it.copy(ritual = it.ritual.copy(steps = it.ritual.steps + RitualStep("track", t.id, ((t.durationMs + 30_000) / 60_000).toInt()))) }
                                addStep = false
                            }.padding(vertical = 8.dp),
                        )
                    }
                }
            },
            confirmButton = { TextButton(onClick = { addStep = false }) { Text("Fermer") } },
        )
    }
    @Suppress("UNUSED_VARIABLE") val unusedAdd = Icons.Filled.Add
    @Suppress("UNUSED_VARIABLE") val unusedAlign = TextAlign.Center
}

private fun <T> List<T>.swap(a: Int, b: Int): List<T> {
    if (a !in indices || b !in indices) return this
    val m = toMutableList()
    val t = m[a]; m[a] = m[b]; m[b] = t
    return m
}
