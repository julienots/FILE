package com.superessence.tagforge.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.tagforge.data.Store
import com.superessence.tagforge.nfc.NfcBus
import com.superessence.tagforge.nfc.NfcOp
import com.superessence.tagforge.nfc.ParsedRecord
import com.superessence.tagforge.nfc.TagInfo
import com.superessence.tagforge.tasks.TaskActivity
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@OptIn(ExperimentalLayoutApi::class)
@Composable
fun ReadScreen() {
    val info = NfcBus.lastTag
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState())) {
        ScreenHeader("TagForge", "Lecteur & graveur NFC") {
            IconButton(onClick = { Nav.push(Sub.History) }) { Icon(Icons.Filled.History, "Historique") }
            IconButton(onClick = { Nav.push(Sub.Settings) }) { Icon(Icons.Filled.Settings, "Réglages") }
        }
        Column(Modifier.padding(horizontal = 16.dp)) {
            if (info == null) {
                Column(Modifier.fillMaxWidth().padding(top = 24.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                    NfcPulse(Modifier.size(220.dp))
                    Spacer(Modifier.height(16.dp))
                    Text("Approchez un tag NFC", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
                    Text("Tout est lu automatiquement : contenu, puce, mémoire, sécurité.", textAlign = TextAlign.Center,
                        color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.padding(8.dp))
                }
                Spacer(Modifier.height(16.dp))
                QuickStart()
            } else {
                TagCard(info)
            }
        }
        Spacer(Modifier.height(24.dp))
    }
}

@Composable
private fun QuickStart() {
    SectionTitle("Démarrage rapide")
    val items = listOf(
        Triple(Icons.Filled.RecordVoiceOver, "Voix Google à l'approche", "google_voice"),
        Triple(Icons.Filled.Wifi, "Partager mon Wi-Fi", "wifi"),
        Triple(Icons.Filled.ContactPage, "Carte de visite", "vcard"),
        Triple(Icons.Filled.Bolt, "Automatiser (lampe, silence, alarme…)", "tasks"),
    )
    items.forEach { (icon, label, id) ->
        Panel(Modifier.padding(vertical = 4.dp), onClick = {
            val t = com.superessence.tagforge.nfc.RecordTypes.byId(id)!!
            Nav.tab = Tab.WRITE; Nav.push(Sub.Editor(t, null, -1))
        }) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                IconBubble(icon)
                Spacer(Modifier.width(12.dp))
                Text(label, Modifier.weight(1f), style = MaterialTheme.typography.bodyLarge)
                Icon(Icons.Filled.ChevronRight, null)
            }
        }
    }
    Spacer(Modifier.height(8.dp))
    Panel(onClick = { Nav.push(Sub.Guide) }) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            IconBubble(Icons.Filled.School, Violet)
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text("Guide : ce qui marche sans appli", style = MaterialTheme.typography.bodyLarge)
                Text("Voix Google, Wi-Fi, contact, appel, GPS…", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            Icon(Icons.Filled.ChevronRight, null)
        }
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun TagCard(info: TagInfo) {
    val ctx = LocalContext.current
    var showMem by remember(info.time) { mutableStateOf(false) }
    Panel {
        Row(verticalAlignment = Alignment.CenterVertically) {
            IconBubble(Icons.Filled.Nfc, size = 48)
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text(info.chip, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
                Text("UID " + info.uid, style = MaterialTheme.typography.bodySmall, fontFamily = FontFamily.Monospace)
                Text(SimpleDateFormat("dd/MM HH:mm:ss", Locale.FRANCE).format(Date(info.time)), style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            IconButton(onClick = { ctx.copy(info.uid) }) { Icon(Icons.Filled.ContentCopy, "Copier l'UID") }
        }
        Spacer(Modifier.height(10.dp))
        FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            info.techs.forEach { Text(it, Modifier.background(MaterialTheme.colorScheme.surfaceVariant, RoundedCornerShape(8.dp)).padding(horizontal = 8.dp, vertical = 3.dp), style = MaterialTheme.typography.labelSmall) }
        }
        if (info.maxSize > 0) {
            Spacer(Modifier.height(12.dp))
            Row { Text("Mémoire NDEF", Modifier.weight(1f), style = MaterialTheme.typography.bodySmall); Text("${info.usedSize} / ${info.maxSize} octets", style = MaterialTheme.typography.bodySmall) }
            LinearProgressIndicator({ (info.usedSize.toFloat() / info.maxSize).coerceIn(0f, 1f) }, Modifier.fillMaxWidth().padding(top = 4.dp).height(6.dp))
        }
        Spacer(Modifier.height(10.dp))
        info.ndefType?.let { KeyValue("Format", it) }
        KeyValue("Inscriptible", if (info.writable) "Oui" else "Non (verrouillé ou protégé)")
        KeyValue("Verrouillable", if (info.canLock) "Oui" else "Non")
        info.details.forEach { (k, v) -> KeyValue(k, v) }
    }

    SectionTitle(if (info.records.isEmpty()) "Contenu : tag vide" else "Contenu (${info.records.size})")
    info.records.forEachIndexed { i, r -> RecordCard(i, r); Spacer(Modifier.height(8.dp)) }

    SectionTitle("Actions")
    FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        info.message?.let { m ->
            AssistChip(onClick = { NfcBus.start(NfcOp.CloneWrite(m, true)) }, label = { Text("Copier vers d'autres tags") }, leadingIcon = { Icon(Icons.Filled.ContentCopy, null) })
            AssistChip(onClick = {
                Store.emulationNdef = m.toByteArray(); Store.emulationLabel = "Tag ${info.uid}"; Store.saveSettings(); Nav.push(Sub.Emulation)
            }, label = { Text("Émuler avec ce téléphone") }, leadingIcon = { Icon(Icons.Filled.PhoneAndroid, null) })
        }
        AssistChip(onClick = { NfcBus.start(NfcOp.Dump) }, label = { Text("Relire tout") }, leadingIcon = { Icon(Icons.Filled.Memory, null) })
        AssistChip(onClick = { ctx.shareText(report(info)) }, label = { Text("Partager le rapport") }, leadingIcon = { Icon(Icons.Filled.Share, null) })
        AssistChip(onClick = { NfcBus.lastTag = null }, label = { Text("Effacer l'écran") }, leadingIcon = { Icon(Icons.Filled.Close, null) })
    }

    if (info.memory.isNotEmpty()) {
        SectionTitle("Mémoire brute")
        Panel(onClick = { showMem = !showMem }) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text("${info.memory.size} lignes", Modifier.weight(1f))
                Icon(if (showMem) Icons.Filled.ExpandLess else Icons.Filled.ExpandMore, null)
            }
            if (showMem) {
                SelectionContainer {
                    Text(info.memory.joinToString("\n"), Modifier.horizontalScroll(rememberScrollState()).padding(top = 8.dp),
                        fontFamily = FontFamily.Monospace, fontSize = 11.sp, lineHeight = 15.sp)
                }
            }
        }
    }
}

@Composable
private fun RecordCard(i: Int, r: ParsedRecord) {
    val ctx = LocalContext.current
    var raw by remember { mutableStateOf(false) }
    Panel {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("#${i + 1}", style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.primary)
            Spacer(Modifier.width(8.dp))
            Text(r.kind, Modifier.weight(1f), style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
            Text("${r.size} o", style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        SelectionContainer { Text(r.value, Modifier.padding(top = 6.dp), style = MaterialTheme.typography.bodyMedium) }
        Row(Modifier.padding(top = 6.dp)) {
            r.uri?.let { u -> TextButton(onClick = { ctx.openUri(u) }) { Icon(Icons.Filled.OpenInNew, null, Modifier.size(18.dp)); Spacer(Modifier.width(4.dp)); Text("Ouvrir") } }
            r.tasks?.let { t -> TextButton(onClick = { TaskActivity.launch(ctx, t) }) { Icon(Icons.Filled.PlayArrow, null, Modifier.size(18.dp)); Spacer(Modifier.width(4.dp)); Text("Exécuter") } }
            TextButton(onClick = { ctx.copy(r.uri ?: r.value) }) { Text("Copier") }
            TextButton(onClick = { raw = !raw }) { Text(if (raw) "Masquer" else "Détails") }
        }
        if (raw) {
            KeyValue("TNF", r.tnf)
            KeyValue("Type", r.type)
            Text(r.payloadHex, fontFamily = FontFamily.Monospace, fontSize = 11.sp)
        }
    }
}

private fun report(i: TagInfo) = buildString {
    appendLine("Rapport TagForge NFC")
    appendLine("Puce : ${i.chip}")
    appendLine("UID : ${i.uid}")
    appendLine("Technologies : ${i.techs.joinToString()}")
    i.ndefType?.let { appendLine("Format : $it") }
    if (i.maxSize > 0) appendLine("Mémoire : ${i.usedSize}/${i.maxSize} octets")
    i.details.forEach { appendLine("${it.first} : ${it.second}") }
    appendLine()
    i.records.forEachIndexed { n, r -> appendLine("#${n + 1} ${r.kind}\n${r.value}\n") }
    if (i.memory.isNotEmpty()) { appendLine("Mémoire :"); i.memory.forEach { appendLine(it) } }
}
