package com.superessence.tagforge.ui

import android.content.Intent
import android.content.pm.ResolveInfo
import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.core.graphics.drawable.toBitmap
import com.superessence.tagforge.nfc.*
import com.superessence.tagforge.tasks.TaskActivity
import com.superessence.tagforge.tasks.TaskCodec
import com.superessence.tagforge.tasks.TaskItem
import com.superessence.tagforge.tasks.TaskType

@Composable
fun RecordEditorScreen(s: Sub.Editor) {
    val ctx = LocalContext.current
    val t = s.type
    val values = remember(s) {
        mutableStateMapOf<String, String>().apply {
            t.fields.forEach { f -> put(f.key, s.initial?.values?.get(f.key) ?: f.default) }
        }
    }
    val preview = runCatching { RecordTypes.buildMessage(listOf(RecordItem(t.id, values.toMap()))) }
    var qr by remember { mutableStateOf<String?>(null) }

    Column(Modifier.fillMaxSize()) {
        ScreenHeader(t.title, t.category.label, onBack = { Nav.pop() })
        Column(Modifier.weight(1f).verticalScroll(rememberScrollState()).padding(horizontal = 16.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                SupportBadge(t.support)
                Spacer(Modifier.width(8.dp))
                Text(t.description, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            Spacer(Modifier.height(8.dp))
            t.fields.forEach { f ->
                val v = values[f.key].orEmpty()
                val label = f.label + if (f.required) " *" else ""
                when (f.kind) {
                    FieldKind.SELECT -> SelectField(v, f.options, label) { values[f.key] = it }
                    FieldKind.SWITCH -> SwitchRow(f.label, v == "true") { values[f.key] = it.toString() }
                    FieldKind.TASKS -> TaskListEditor(v) { values[f.key] = it }
                    FieldKind.APP -> {
                        var pick by remember { mutableStateOf(false) }
                        Field(v, { values[f.key] = it.trim() }, label, f.hint, trailing = { IconButton(onClick = { pick = true }) { Icon(Icons.Filled.Apps, "Choisir") } })
                        TextButton(onClick = { pick = true }) { Text("Choisir parmi les applis installées") }
                        if (pick) AppPickerDialog({ pick = false }) { values[f.key] = it; pick = false }
                    }
                    else -> Field(
                        v, { values[f.key] = it }, label, f.hint,
                        multiline = f.kind == FieldKind.MULTILINE,
                        password = f.kind == FieldKind.PASSWORD,
                        keyboard = when (f.kind) {
                            FieldKind.NUMBER -> KeyboardType.Decimal; FieldKind.PHONE -> KeyboardType.Phone
                            FieldKind.EMAIL -> KeyboardType.Email; FieldKind.URL -> KeyboardType.Uri; else -> KeyboardType.Text
                        },
                    )
                }
            }
            if (t.id == "google_voice") {
                val n = values["text"].orEmpty().length
                Text("$n / 200 caractères", style = MaterialTheme.typography.labelSmall, color = if (n > 200) Danger else MaterialTheme.colorScheme.onSurfaceVariant)
                Spacer(Modifier.height(8.dp))
                InfoNote("Fonctionne sur tout Android, sans appli : à l'approche, le navigateur ouvre la voix Google qui lit le texte. " +
                    "Le téléphone doit être déverrouillé et connecté à Internet. Selon le navigateur, un appui sur ▶ peut être demandé.", Icons.Filled.RecordVoiceOver, Teal)
            }
            if (t.id == "speak" || t.id == "assistant") {
                Spacer(Modifier.height(8.dp))
                InfoNote("Aucun Android ne peut parler ou lancer l'Assistant à partir d'un tag sans appli installée : c'est une limite du système. " +
                    "Avec TagForge installé, c'est instantané, appli fermée, et la voix fonctionne même hors connexion. Pour une voix 100 % sans appli, utilisez « Voix Google — lecture à voix haute ».", Icons.Filled.Info, Violet)
            }
            Spacer(Modifier.height(12.dp))
            preview.exceptionOrNull()?.let { InfoNote(it.message ?: "Champ manquant", Icons.Filled.Warning, Amber) }
            preview.getOrNull()?.let { m ->
                Panel {
                    Text("Aperçu", style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.primary)
                    NdefParser.parse(m).forEach { r -> Text("${r.kind} : ${r.value}", style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(top = 4.dp)) }
                    Text("${m.byteArrayLength} octets", style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.padding(top = 4.dp))
                    Row {
                        val first = NdefParser.parse(m).firstOrNull()
                        first?.uri?.let { u ->
                            TextButton(onClick = { ctx.openUri(u) }) { Text("Tester") }
                            TextButton(onClick = { qr = u }) { Text("QR code") }
                        }
                        first?.tasks?.let { tasks -> TextButton(onClick = { TaskActivity.launch(ctx, tasks) }) { Text("Tester les tâches") } }
                    }
                }
            }
            Spacer(Modifier.height(24.dp))
        }
        Row(Modifier.padding(16.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = {
                val item = RecordItem(t.id, values.toMap())
                runCatching { RecordTypes.buildMessage(listOf(item)) }.onFailure { ctx.toast(it.message ?: "Erreur"); return@OutlinedButton }
                save(s, item); Nav.pop()
            }, Modifier.weight(1f).height(52.dp)) { Text(if (s.index >= 0) "Enregistrer" else "Ajouter") }
            Button(onClick = {
                val item = RecordItem(t.id, values.toMap())
                runCatching { RecordTypes.buildMessage(listOf(item)) }.onFailure { ctx.toast(it.message ?: "Erreur"); return@Button }
                save(s, item); Nav.pop(); Nav.tab = Tab.WRITE; WriteState.startWrite(ctx)
            }, Modifier.weight(1f).height(52.dp)) { Icon(Icons.Filled.Nfc, null); Spacer(Modifier.width(6.dp)); Text("Écrire") }
        }
    }
    qr?.let { QrDialog(it) { qr = null } }
}

private fun save(s: Sub.Editor, item: RecordItem) {
    if (s.index >= 0 && s.index < WriteState.items.size) WriteState.items[s.index] = item else WriteState.items.add(item)
}

@Composable
fun TaskListEditor(json: String, onChange: (String) -> Unit) {
    val tasks = remember { TaskCodec.decode(json).toMutableStateList() }
    var adding by remember { mutableStateOf(false) }
    fun commit() = onChange(TaskCodec.encode(tasks))

    Text("Les tâches s'exécutent dans l'ordre, à l'approche du tag.", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
    tasks.forEachIndexed { i, task ->
        var open by remember(i, task.type) { mutableStateOf(true) }
        Spacer(Modifier.height(8.dp))
        Panel {
            Row(Modifier.clickable { open = !open }, verticalAlignment = Alignment.CenterVertically) {
                Text("${i + 1}.", color = Teal, fontWeight = FontWeight.Bold)
                Spacer(Modifier.width(8.dp))
                Text(task.summary(), Modifier.weight(1f), style = MaterialTheme.typography.bodyMedium)
                if (i > 0) IconButton(onClick = { tasks.add(i - 1, tasks.removeAt(i)); commit() }, Modifier.size(32.dp)) { Icon(Icons.Filled.ArrowUpward, null, Modifier.size(18.dp)) }
                IconButton(onClick = { tasks.removeAt(i); commit() }, Modifier.size(32.dp)) { Icon(Icons.Filled.Delete, null, Modifier.size(18.dp)) }
            }
            if (open) {
                task.type.params.forEachIndexed { pi, p ->
                    val v = task.args.getOrNull(pi) ?: p.default
                    fun set(nv: String) {
                        val a = MutableList(task.type.params.size) { k -> task.args.getOrNull(k) ?: task.type.params[k].default }
                        a[pi] = nv; tasks[i] = TaskItem(task.type, a); commit()
                    }
                    when {
                        p.options.isNotEmpty() -> SelectField(v, p.options, p.label) { set(it) }
                        p.appPicker -> {
                            var pick by remember { mutableStateOf(false) }
                            Field(v, { set(it) }, p.label, p.hint, trailing = { IconButton(onClick = { pick = true }) { Icon(Icons.Filled.Apps, null) } })
                            if (pick) AppPickerDialog({ pick = false }) { set(it); pick = false }
                        }
                        else -> Field(v, { set(it) }, p.label, p.hint, keyboard = if (p.numeric) KeyboardType.Number else KeyboardType.Text)
                    }
                }
            }
        }
    }
    Spacer(Modifier.height(8.dp))
    OutlinedButton(onClick = { adding = true }, Modifier.fillMaxWidth()) { Icon(Icons.Filled.Add, null); Spacer(Modifier.width(6.dp)); Text("Ajouter une tâche (${TaskType.entries.size} disponibles)") }
    if (adding) {
        AlertDialog(
            onDismissRequest = { adding = false },
            confirmButton = { TextButton(onClick = { adding = false }) { Text("Fermer") } },
            title = { Text("Ajouter une tâche") },
            text = {
                LazyColumn(Modifier.heightIn(max = 460.dp)) {
                    items(TaskType.entries) { tt ->
                        Text(tt.title, Modifier.fillMaxWidth().clickable { tasks.add(TaskItem.new(tt)); commit(); adding = false }.padding(vertical = 12.dp))
                    }
                }
            },
        )
    }
}

@Composable
fun AppPickerDialog(onDismiss: () -> Unit, onPick: (String) -> Unit) {
    val ctx = LocalContext.current
    val pm = ctx.packageManager
    val apps = remember {
        pm.queryIntentActivities(Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER), 0)
            .distinctBy { it.activityInfo.packageName }
            .sortedBy { it.loadLabel(pm).toString().lowercase() }
    }
    var q by remember { mutableStateOf("") }
    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = { TextButton(onClick = onDismiss) { Text("Fermer") } },
        title = { Text("Choisir une appli") },
        text = {
            Column {
                Field(q, { q = it }, "Rechercher")
                LazyColumn(Modifier.heightIn(max = 420.dp)) {
                    items(apps.filter { q.isBlank() || it.loadLabel(pm).toString().contains(q, true) || it.activityInfo.packageName.contains(q, true) }) { ri: ResolveInfo ->
                        val icon = remember(ri.activityInfo.packageName) { runCatching { ri.loadIcon(pm).toBitmap(64, 64).asImageBitmap() }.getOrNull() }
                        Row(Modifier.fillMaxWidth().clickable { onPick(ri.activityInfo.packageName) }.padding(vertical = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                            if (icon != null) Image(icon, null, Modifier.size(36.dp))
                            Spacer(Modifier.width(12.dp))
                            Column {
                                Text(ri.loadLabel(pm).toString())
                                Text(ri.activityInfo.packageName, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                            }
                        }
                    }
                }
            }
        },
    )
}
