package com.superessence.tagforge.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.superessence.tagforge.data.Store
import com.superessence.tagforge.data.Template
import com.superessence.tagforge.nfc.*

/** Contenu en cours d'édition (partagé entre les écrans). */
object WriteState {
    val items = mutableStateListOf<RecordItem>()
    var batch by mutableStateOf(false)
    var startN by mutableStateOf("1")
    var lock by mutableStateOf(false)
    var protect by mutableStateOf(false)
    var newPassword by mutableStateOf("")
    var protectRead by mutableStateOf(false)
    var authPassword by mutableStateOf("")
    var templateName by mutableStateOf("")

    fun load(t: Template) { items.clear(); items.addAll(t.items); templateName = t.name }

    fun message(n: Int = 1): NdefMessage = RecordTypes.buildMessage(items.map { it.withPlaceholders(n) })

    fun label(): String = items.joinToString(" + ") { it.type?.title ?: it.typeId }

    fun startWrite(ctx: android.content.Context) {
        val snapshot = items.toList()
        try { RecordTypes.buildMessage(snapshot.map { it.withPlaceholders(1) }) } catch (e: Exception) { ctx.toast(e.message ?: "Contenu invalide"); return }
        if (protect && newPassword.isBlank()) { ctx.toast("Indiquez le nouveau mot de passe"); return }
        NfcBus.start(NfcOp.Write(
            produce = { n -> RecordTypes.buildMessage(snapshot.map { it.withPlaceholders(n) }) },
            batch = batch, startN = startN.toIntOrNull() ?: 1,
            authPassword = authPassword.ifBlank { null }, lock = lock,
            newPassword = if (protect) newPassword else null, protectRead = protectRead, label = label(),
        ))
    }
}

private typealias NdefMessage = android.nfc.NdefMessage

@OptIn(ExperimentalLayoutApi::class)
@Composable
fun WriteScreen() {
    val ctx = LocalContext.current
    var qr by remember { mutableStateOf<String?>(null) }
    var saveDialog by remember { mutableStateOf(false) }
    val items = WriteState.items
    val built = remember(items.toList()) { runCatching { WriteState.message() } }
    val size = built.getOrNull()?.byteArrayLength ?: 0

    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState())) {
        ScreenHeader("Écrire un tag", if (items.isEmpty()) "Choisissez un ou plusieurs contenus" else "${items.size} contenu(s) · $size octets") {
            if (items.isNotEmpty()) IconButton(onClick = { items.clear(); WriteState.templateName = "" }) { Icon(Icons.Filled.DeleteSweep, "Tout effacer") }
        }
        Column(Modifier.padding(horizontal = 16.dp)) {
            if (items.isEmpty()) {
                InfoNote("Astuce : les contenus marqués « Sans appli » s'ouvrent sur n'importe quel Android dès l'approche, sans rien installer. " +
                    "Ceux marqués « Avec TagForge » (voix instantanée, lampe, silencieux, alarme…) nécessitent TagForge sur le téléphone qui lit.")
                SectionTitle("Populaires")
                val popular = listOf("url", "google_voice", "speak", "wifi", "vcard", "tasks", "whatsapp", "social", "google_review", "maps_dir", "aar", "tel")
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    popular.mapNotNull { RecordTypes.byId(it) }.forEach { t ->
                        FilterChip(false, onClick = { Nav.push(Sub.Editor(t, null, -1)) }, label = { Text(t.title, maxLines = 1, overflow = TextOverflow.Ellipsis) },
                            leadingIcon = { Icon(t.icon, null, Modifier.size(18.dp)) })
                    }
                }
            }
            items.forEachIndexed { i, item ->
                val t = item.type ?: return@forEachIndexed
                Spacer(Modifier.height(8.dp))
                Panel(onClick = { Nav.push(Sub.Editor(t, item, i)) }) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        IconBubble(t.icon, if (t.support == Support.NATIVE) Teal else Violet)
                        Spacer(Modifier.width(12.dp))
                        Column(Modifier.weight(1f)) {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Text(t.title, Modifier.weight(1f, false), style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold, maxLines = 1, overflow = TextOverflow.Ellipsis)
                                Spacer(Modifier.width(6.dp)); SupportBadge(t.support)
                            }
                            Text(runCatching { t.summary(item.values) }.getOrDefault(""), style = MaterialTheme.typography.bodySmall, maxLines = 2, overflow = TextOverflow.Ellipsis,
                                color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                        Column {
                            if (i > 0) IconButton(onClick = { items.add(i - 1, items.removeAt(i)) }, Modifier.size(32.dp)) { Icon(Icons.Filled.ArrowUpward, "Monter", Modifier.size(18.dp)) }
                            IconButton(onClick = { items.removeAt(i) }, Modifier.size(32.dp)) { Icon(Icons.Filled.Delete, "Supprimer", Modifier.size(18.dp)) }
                        }
                    }
                }
            }
            Spacer(Modifier.height(12.dp))
            OutlinedButton(onClick = { Nav.push(Sub.TypePicker) }, Modifier.fillMaxWidth()) {
                Icon(Icons.Filled.Add, null); Spacer(Modifier.width(8.dp)); Text(if (items.isEmpty()) "Choisir un contenu (${RecordTypes.ALL.size} types)" else "Ajouter un autre contenu")
            }

            if (items.isNotEmpty()) {
                built.exceptionOrNull()?.let { Spacer(Modifier.height(8.dp)); InfoNote(it.message ?: "Contenu invalide", Icons.Filled.Warning, Danger) }
                if (items.size > 1 && items.none { it.type?.support == Support.TAGFORGE }) {
                    Spacer(Modifier.height(8.dp))
                    InfoNote("Plusieurs contenus : Android ouvre automatiquement le PREMIER. Les suivants sont lus par les applis NFC (et par TagForge).")
                }
                if (items.any { it.type?.support == Support.TAGFORGE }) {
                    Spacer(Modifier.height(8.dp))
                    InfoNote("Ce tag déclenche TagForge à l'approche, appli fermée. Sur un téléphone sans TagForge, Android propose de l'installer.", Icons.Filled.Bolt, Violet)
                }

                SectionTitle("Capacité")
                Panel {
                    val need = Encoders.tlvSize(size)
                    Text("$size octets (≈ $need avec l'en-tête)", style = MaterialTheme.typography.bodyMedium, fontWeight = FontWeight.SemiBold)
                    Spacer(Modifier.height(6.dp))
                    FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                        Encoders.CHIPS.forEach { (name, cap) ->
                            val ok = need <= cap
                            AssistChip(onClick = {}, label = { Text(name) }, leadingIcon = {
                                Icon(if (ok) Icons.Filled.Check else Icons.Filled.Close, null, tint = if (ok) Ok else Danger, modifier = Modifier.size(16.dp))
                            })
                        }
                    }
                }

                SectionTitle("Options d'écriture")
                Panel {
                    SwitchRow("Écriture en série", WriteState.batch, "Écrire plusieurs tags à la suite. {n} dans un champ = numéro automatique ({n3} → 001, {date}, {time}).") { WriteState.batch = it }
                    if (WriteState.batch) Field(WriteState.startN, { WriteState.startN = it.filter { c -> c.isDigit() } }, "Premier numéro", keyboard = KeyboardType.Number)
                    HorizontalDivider(Modifier.padding(vertical = 6.dp))
                    SwitchRow("Protéger par mot de passe", WriteState.protect, "NTAG213/215/216 : empêche toute modification sans le mot de passe (réversible).") { WriteState.protect = it }
                    if (WriteState.protect) {
                        Field(WriteState.newPassword, { WriteState.newPassword = it }, "Nouveau mot de passe", password = true)
                        SwitchRow("Protéger aussi la lecture", WriteState.protectRead, "Le contenu ne s'ouvrira plus automatiquement.") { WriteState.protectRead = it }
                    }
                    HorizontalDivider(Modifier.padding(vertical = 6.dp))
                    SwitchRow("Verrouiller définitivement", WriteState.lock, "IRRÉVERSIBLE : le tag ne pourra plus jamais être modifié.") { WriteState.lock = it }
                    HorizontalDivider(Modifier.padding(vertical = 6.dp))
                    Field(WriteState.authPassword, { WriteState.authPassword = it }, "Mot de passe actuel du tag (s'il est protégé)", password = true)
                }

                Spacer(Modifier.height(16.dp))
                Button(onClick = { WriteState.startWrite(ctx) }, Modifier.fillMaxWidth().height(56.dp), enabled = built.isSuccess) {
                    Icon(Icons.Filled.Nfc, null); Spacer(Modifier.width(8.dp)); Text(if (WriteState.batch) "Écrire en série" else "Écrire sur un tag", fontWeight = FontWeight.Bold)
                }
                Spacer(Modifier.height(8.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(onClick = { saveDialog = true }, Modifier.weight(1f)) { Icon(Icons.Filled.BookmarkAdd, null); Spacer(Modifier.width(4.dp)); Text("Modèle") }
                    OutlinedButton(onClick = {
                        val first = built.getOrNull()?.records?.firstOrNull()?.let { NdefParser.parse(it) }
                        qr = first?.uri ?: first?.value
                    }, Modifier.weight(1f), enabled = built.isSuccess) { Icon(Icons.Filled.QrCode2, null); Spacer(Modifier.width(4.dp)); Text("QR") }
                    OutlinedButton(onClick = {
                        built.getOrNull()?.let { Store.emulationNdef = it.toByteArray(); Store.emulationLabel = WriteState.label(); Store.saveSettings(); Nav.push(Sub.Emulation) }
                    }, Modifier.weight(1f), enabled = built.isSuccess) { Icon(Icons.Filled.PhoneAndroid, null); Spacer(Modifier.width(4.dp)); Text("Émuler") }
                }
            }
            Spacer(Modifier.height(24.dp))
        }
    }
    qr?.let { QrDialog(it) { qr = null } }
    if (saveDialog) {
        var name by remember { mutableStateOf(WriteState.templateName.ifBlank { WriteState.label().take(40) }) }
        AlertDialog(
            onDismissRequest = { saveDialog = false },
            title = { Text("Enregistrer comme modèle") },
            text = { Field(name, { name = it }, "Nom du modèle") },
            confirmButton = { TextButton(onClick = {
                Store.saveTemplate(Template(name.ifBlank { "Modèle" }, items.toList())); WriteState.templateName = name; saveDialog = false; ctx.toast("Modèle enregistré")
            }) { Text("Enregistrer") } },
            dismissButton = { TextButton(onClick = { saveDialog = false }) { Text("Annuler") } },
        )
    }
}

@Composable
fun TypePickerScreen() {
    var q by remember { mutableStateOf("") }
    var filter by remember { mutableStateOf<Support?>(null) }
    val list = RecordTypes.ALL.filter { t ->
        (filter == null || t.support == filter) &&
            (q.isBlank() || listOf(t.title, t.description, t.category.label).any { it.contains(q, ignoreCase = true) })
    }
    Column(Modifier.fillMaxSize()) {
        ScreenHeader("Choisir un contenu", "${RecordTypes.ALL.size} types disponibles", onBack = { Nav.pop() })
        Column(Modifier.padding(horizontal = 16.dp)) {
            Field(q, { q = it }, "Rechercher", "wifi, voix, instagram, alarme…", trailing = { Icon(Icons.Filled.Search, null) })
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilterChip(filter == null, { filter = null }, { Text("Tous") })
                FilterChip(filter == Support.NATIVE, { filter = Support.NATIVE }, { Text("Sans appli") })
                FilterChip(filter == Support.TAGFORGE, { filter = Support.TAGFORGE }, { Text("Avec TagForge") })
            }
        }
        Column(Modifier.weight(1f).verticalScroll(rememberScrollState()).padding(horizontal = 16.dp)) {
            Category.entries.forEach { cat ->
                val inCat = list.filter { it.category == cat }
                if (inCat.isNotEmpty()) {
                    SectionTitle(cat.label)
                    inCat.forEach { t ->
                        Panel(Modifier.padding(vertical = 4.dp), onClick = { Nav.pop(); Nav.push(Sub.Editor(t, null, -1)) }) {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                IconBubble(t.icon, if (t.support == Support.NATIVE) Teal else Violet)
                                Spacer(Modifier.width(12.dp))
                                Column(Modifier.weight(1f)) {
                                    Text(t.title, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                                    Text(t.description, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 2, overflow = TextOverflow.Ellipsis)
                                    Spacer(Modifier.height(4.dp)); SupportBadge(t.support)
                                }
                            }
                        }
                    }
                }
            }
            Spacer(Modifier.height(24.dp))
        }
    }
}
