package com.superessence.squad.ui

import android.app.TimePickerDialog
import android.content.Intent
import android.provider.Settings
import androidx.compose.foundation.Image
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
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.AdsClick
import androidx.compose.material.icons.filled.Apps
import androidx.compose.material.icons.filled.BatteryChargingFull
import androidx.compose.material.icons.filled.CleaningServices
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.DirectionsWalk
import androidx.compose.material.icons.filled.NotificationsActive
import androidx.compose.material.icons.filled.Schedule
import androidx.compose.material.icons.filled.Security
import androidx.compose.material.icons.filled.StickyNote2
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TextField
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalLifecycleOwner
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.graphics.drawable.toBitmap
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import com.superessence.squad.engine.Squad
import com.superessence.squad.engine.SquadState
import com.superessence.squad.model.Act
import com.superessence.squad.model.Reminder
import com.superessence.squad.model.Task
import com.superessence.squad.model.TaskKind
import com.superessence.squad.model.fmtMin
import com.superessence.squad.overlay.OverlayService
import com.superessence.squad.overlay.TapService
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

private class AppEntry(val pkg: String, val label: String, val icon: ImageBitmap?)

fun taskIcon(k: TaskKind): ImageVector = when (k) {
    TaskKind.NONE -> Icons.Filled.DirectionsWalk
    TaskKind.CLEANER -> Icons.Filled.CleaningServices
    TaskKind.LAUNCHER -> Icons.Filled.Apps
    TaskKind.TAPPER -> Icons.Filled.AdsClick
    TaskKind.CLOCK -> Icons.Filled.Schedule
    TaskKind.COACH -> Icons.Filled.NotificationsActive
    TaskKind.BATTERY -> Icons.Filled.BatteryChargingFull
    TaskKind.GUARD -> Icons.Filled.Security
    TaskKind.NOTE -> Icons.Filled.StickyNote2
}

private fun taskPreviewAct(k: TaskKind) = when (k) {
    TaskKind.CLEANER -> Act.WIPE
    TaskKind.TAPPER -> Act.TAP
    TaskKind.GUARD -> Act.GUARD
    TaskKind.COACH -> Act.TALK
    TaskKind.NONE -> Act.WAVE
    else -> Act.SIGN
}

private fun previewSign(k: TaskKind) = when (k) {
    TaskKind.LAUNCHER -> com.superessence.squad.render.Sign(listOf("APPLI"))
    TaskKind.CLOCK -> com.superessence.squad.render.Sign(listOf("12:00"))
    TaskKind.BATTERY -> com.superessence.squad.render.Sign(listOf("80 %"), accent = 0xFF5BC26A.toInt())
    TaskKind.NOTE -> com.superessence.squad.render.Sign(listOf("NOTE"), accent = 0xFF4AA3FF.toInt())
    else -> null
}

/** Choix et réglage du métier d'une mascotte. */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun TaskScreen(state: SquadState, id: String, onBack: () -> Unit) {
    val m = state.mascots.firstOrNull { it.id == id } ?: run { LaunchedEffect(Unit) { onBack() }; return }
    val ctx = LocalContext.current
    val task = m.task
    fun set(t: Task) = Squad.setTask(id, t)
    var appPicker by remember { mutableStateOf(false) }
    var noteEdit by remember { mutableStateOf(false) }
    var addReminder by remember { mutableStateOf<Int?>(null) }
    // statut du service d'accessibilité, relu au retour dans l'appli
    var accessOn by remember { mutableStateOf(TapService.isEnabled(ctx)) }
    val owner = LocalLifecycleOwner.current
    androidx.compose.runtime.DisposableEffect(owner) {
        val obs = LifecycleEventObserver { _, e -> if (e == Lifecycle.Event.ON_RESUME) accessOn = TapService.isEnabled(ctx) }
        owner.lifecycle.addObserver(obs)
        onDispose { owner.lifecycle.removeObserver(obs) }
    }

    Column(Modifier.fillMaxSize()) {
        TopBar("Métier de ${m.name}", onBack)
        Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(horizontal = 16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Text("Choisis ce qu'il fait sur ton écran. Les métiers « à poste » restent à leur place : glisse-le pour changer son poste.", style = Small)
            TaskKind.entries.forEach { k ->
                val sel = task.kind == k
                Row(
                    Modifier.fillMaxWidth().clip(RoundedCornerShape(16.dp)).background(if (sel) C.card2 else C.card)
                        .border(if (sel) 2.dp else 1.dp, if (sel) C.yellow else C.line, RoundedCornerShape(16.dp))
                        .clickable { if (!sel) set(Task(kind = k, note = task.note, reminders = task.reminders)) }.padding(12.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Box(Modifier.size(44.dp, 58.dp)) { MascotCanvas(m.look, taskPreviewAct(k), Modifier.fillMaxSize(), m.fitness, 3, sign = previewSign(k)) }
                    Spacer(Modifier.width(10.dp))
                    Column(Modifier.weight(1f)) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Icon(taskIcon(k), null, tint = if (sel) C.yellow else C.dim, modifier = Modifier.size(18.dp))
                            Spacer(Modifier.width(6.dp))
                            Text(k.label, style = Body.copy(fontWeight = FontWeight.Bold, color = if (sel) C.yellow else C.text))
                        }
                        Text(k.desc, style = Small.copy(fontSize = 12.sp))
                    }
                }
                if (sel) TaskConfig(m.name, task, ::set, accessOn,
                    onPickApp = { appPicker = true }, onNote = { noteEdit = true },
                    onAddReminder = { TimePickerDialog(ctx, { _, h, mi -> addReminder = h * 60 + mi }, 9, 0, true).show() },
                    onPickPoint = {
                        val act = ctx as? MainActivity
                        if (!OverlayService.canRun(ctx)) act?.enableOverlay()
                        else {
                            Squad.edit { it.copy(settings = it.settings.copy(overlayOn = true)) }
                            Squad.editMascot(id) { it.copy(onScreen = true) }
                            OverlayService.pickPoint(ctx, id)
                            act?.goHome()
                        }
                    },
                    onAccess = { runCatching { ctx.startActivity(Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)) } },
                    onPickTime = { cur, f -> TimePickerDialog(ctx, { _, h, mi -> f(h * 60 + mi) }, if (cur >= 0) cur / 60 else 9, if (cur >= 0) cur % 60 else 0, true).show() },
                )
            }
            Spacer(Modifier.height(24.dp))
        }
    }

    if (appPicker) AppPickerDialog(onDismiss = { appPicker = false }) { pkg, label -> set(task.copy(pkg = pkg, appLabel = label)); appPicker = false }
    if (noteEdit) {
        var text by remember { mutableStateOf(task.note) }
        AlertDialog(
            onDismissRequest = { noteEdit = false },
            title = { Text("Sa note", style = H2) },
            text = {
                TextField(text, { text = it.take(120) }, minLines = 2,
                    colors = TextFieldDefaults.colors(focusedContainerColor = C.card2, unfocusedContainerColor = C.card2, focusedIndicatorColor = C.yellow))
            },
            confirmButton = { TextButton({ set(task.copy(note = text.trim())); noteEdit = false }) { Text("OK", color = C.yellow) } },
            dismissButton = { TextButton({ noteEdit = false }) { Text("Annuler", color = C.dim) } },
            containerColor = C.card,
        )
    }
    addReminder?.let { at ->
        var text by remember { mutableStateOf("") }
        AlertDialog(
            onDismissRequest = { addReminder = null },
            title = { Text("Rappel à ${fmtMin(at)}", style = H2) },
            text = {
                TextField(text, { text = it.take(80) }, placeholder = { Text("Ex : appeler maman") }, singleLine = true,
                    colors = TextFieldDefaults.colors(focusedContainerColor = C.card2, unfocusedContainerColor = C.card2, focusedIndicatorColor = C.yellow))
            },
            confirmButton = { TextButton({ if (text.isNotBlank()) set(task.copy(reminders = (task.reminders + Reminder(at, text.trim())).sortedBy { it.at })); addReminder = null }) { Text("Ajouter", color = C.yellow) } },
            dismissButton = { TextButton({ addReminder = null }) { Text("Annuler", color = C.dim) } },
            containerColor = C.card,
        )
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun TaskConfig(
    name: String, task: Task, set: (Task) -> Unit, accessOn: Boolean,
    onPickApp: () -> Unit, onNote: () -> Unit, onAddReminder: () -> Unit, onPickPoint: () -> Unit, onAccess: () -> Unit,
    onPickTime: (Int, (Int) -> Unit) -> Unit,
) {
    val content: (@Composable () -> Unit)? = when (task.kind) {
        TaskKind.LAUNCHER -> { {
            Column {
                Text(if (task.appLabel.isEmpty()) "Aucune appli choisie." else "Il ouvre : ${task.appLabel}", style = Body)
                Spacer(Modifier.height(8.dp))
                YellowButton("Choisir l'appli", Icons.Filled.Apps, Modifier.fillMaxWidth(), onClick = onPickApp)
            }
        } }
        TaskKind.TAPPER -> { {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text("Service « SQUAD. clics »", style = Body.copy(fontWeight = FontWeight.Bold))
                        Text(if (accessOn) "Activé : $name peut cliquer." else "Désactivé : active-le dans Accessibilité › SQUAD. clics.", style = Small.copy(color = if (accessOn) C.green else C.red, fontSize = 12.sp))
                    }
                    if (!accessOn) GhostButton("Activer", onClick = onAccess)
                }
                Text(if (task.tapX < 0f) "Aucun endroit choisi." else "Endroit choisi (${(task.tapX * 100).toInt()} %, ${(task.tapY * 100).toInt()} % de l'écran).", style = Small)
                YellowButton("Choisir l'endroit à cliquer", Icons.Filled.AdsClick, Modifier.fillMaxWidth(), onClick = onPickPoint)
                Text("L'appli se ferme et un viseur apparaît : glisse-le sur l'icône ou le bouton, puis « Valider ».", style = Small.copy(fontSize = 12.sp))
                Text("QUAND ?", style = Label)
                FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    listOf(0 to "Quand je le touche", 5 to "Toutes les 5 min", 15 to "15 min", 30 to "30 min", 60 to "1 h").forEach { (v, l) ->
                        Chip(l, task.everyMin == v) { set(task.copy(everyMin = v)) }
                    }
                }
                ToggleRow("À heure fixe", task.atMin >= 0, if (task.atMin >= 0) "Tous les jours à ${fmtMin(task.atMin)}" else null) { on ->
                    if (on) onPickTime(task.atMin) { set(task.copy(atMin = it)) } else set(task.copy(atMin = -1))
                }
                if (task.atMin >= 0) TimeRow("Heure", task.atMin) { onPickTime(task.atMin) { set(task.copy(atMin = it)) } }
                Text("Il touche l'écran pour de vrai à cet endroit : choisis un endroit sans risque (icône, bouton). Il ne voit pas l'écran, il clique juste où tu lui as dit.", style = Small.copy(fontSize = 12.sp))
            }
        } }
        TaskKind.NOTE -> { {
            Column {
                Text(task.note.ifEmpty { "Pas encore de note." }, style = Body)
                Spacer(Modifier.height(8.dp))
                YellowButton("Écrire la note", Icons.Filled.StickyNote2, Modifier.fillMaxWidth(), onClick = onNote)
            }
        } }
        TaskKind.COACH -> { {
            Column {
                ToggleRow("Boire de l'eau", task.water, "Environ toutes les 75 minutes") { set(task.copy(water = it)) }
                ToggleRow("Bouger", task.move, "Environ toutes les 55 minutes") { set(task.copy(move = it)) }
                ToggleRow("Repas", task.meals, "10 minutes avant chaque repas") { set(task.copy(meals = it)) }
                ToggleRow("Coucher", task.bedtime, "15 minutes avant l'heure du dodo") { set(task.copy(bedtime = it)) }
                Spacer(Modifier.height(6.dp))
                Text("RAPPELS PERSO", style = Label)
                task.reminders.forEach { r ->
                    Row(Modifier.fillMaxWidth().padding(vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
                        Text(fmtMin(r.at), style = Body.copy(color = C.yellow, fontWeight = FontWeight.Bold), modifier = Modifier.width(60.dp))
                        Text(r.text, style = Body, modifier = Modifier.weight(1f))
                        Icon(Icons.Filled.Close, "Supprimer", tint = C.dim, modifier = Modifier.size(20.dp).clickable { set(task.copy(reminders = task.reminders - r)) })
                    }
                }
                Spacer(Modifier.height(6.dp))
                GhostButton("Ajouter un rappel", Icons.Filled.Add, Modifier.fillMaxWidth(), color = C.yellow, onClick = onAddReminder)
                Text("Il le dit dans une bulle, fait vibrer, et envoie une notification.", style = Small.copy(fontSize = 12.sp))
            }
        } }
        TaskKind.GUARD -> { { Text("Place-le où tu veux garder la place libre (en le glissant sur l'écran) : les autres resteront à l'écart.", style = Small) } }
        TaskKind.CLEANER -> { { Text("Plus besoin d'appuyer sur « Nettoyer » : il s'en charge dès que ça se salit. Touche-le pour qu'il nettoie tout de suite.", style = Small) } }
        TaskKind.CLOCK -> { { Text("Touche-le pour qu'il te dise l'heure.", style = Small) } }
        TaskKind.BATTERY -> { { Text("Il prévient à 20 %, 10 % et 5 %, et quand tu branches le chargeur.", style = Small) } }
        TaskKind.NONE -> null
    }
    if (content != null) Card { content() }
}

@Composable
private fun AppPickerDialog(onDismiss: () -> Unit, onPick: (String, String) -> Unit) {
    val ctx = LocalContext.current
    var query by remember { mutableStateOf("") }
    val apps by produceState<List<AppEntry>?>(null) {
        value = withContext(Dispatchers.IO) {
            val pm = ctx.packageManager
            val intent = Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER)
            pm.queryIntentActivities(intent, 0)
                .filter { it.activityInfo.packageName != ctx.packageName }
                .distinctBy { it.activityInfo.packageName }
                .map { ri ->
                    AppEntry(ri.activityInfo.packageName, ri.loadLabel(pm).toString(),
                        runCatching { ri.loadIcon(pm).toBitmap(96, 96).asImageBitmap() }.getOrNull())
                }
                .sortedBy { it.label.lowercase() }
        }
    }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Quelle appli ?", style = H2) },
        text = {
            Column {
                TextField(query, { query = it }, singleLine = true, placeholder = { Text("Rechercher") },
                    colors = TextFieldDefaults.colors(focusedContainerColor = C.card2, unfocusedContainerColor = C.card2, focusedIndicatorColor = C.yellow),
                    modifier = Modifier.fillMaxWidth())
                Spacer(Modifier.height(8.dp))
                val list = apps
                if (list == null) Text("Chargement…", style = Small)
                else Column(Modifier.height(380.dp).verticalScroll(rememberScrollState())) {
                    list.filter { query.isBlank() || it.label.contains(query, ignoreCase = true) }.forEach { a ->
                        Row(
                            Modifier.fillMaxWidth().clip(RoundedCornerShape(10.dp)).clickable { onPick(a.pkg, a.label) }.padding(vertical = 8.dp, horizontal = 4.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            if (a.icon != null) Image(a.icon, null, Modifier.size(36.dp)) else Box(Modifier.size(36.dp).background(C.card2))
                            Spacer(Modifier.width(12.dp))
                            Text(a.label, style = Body)
                        }
                    }
                }
            }
        },
        confirmButton = { TextButton(onDismiss) { Text("Fermer", color = C.dim) } },
        containerColor = C.card,
    )
}
