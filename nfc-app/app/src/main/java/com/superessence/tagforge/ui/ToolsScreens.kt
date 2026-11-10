package com.superessence.tagforge.ui

import android.Manifest
import android.app.NotificationManager
import android.content.Intent
import android.net.Uri
import android.nfc.NdefMessage
import android.os.Build
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.superessence.tagforge.data.Store
import com.superessence.tagforge.nfc.*
import com.superessence.tagforge.tasks.TaskActivity
import com.superessence.tagforge.tasks.TaskItem
import com.superessence.tagforge.tasks.TaskType
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

private data class Tool(val icon: ImageVector, val title: String, val desc: String, val color: Color, val action: () -> Unit)

@Composable
fun ToolsScreen() {
    val ctx = LocalContext.current
    var confirm by remember { mutableStateOf<Pair<String, NfcOp>?>(null) }
    val tools = listOf(
        Tool(Icons.Filled.Memory, "Lecture complète", "Puce, signature NXP, compteur, mémoire page par page", Teal) { NfcBus.start(NfcOp.Dump); Nav.tab = Tab.READ },
        Tool(Icons.Filled.ContentCopy, "Copier / dupliquer", "Lire un tag puis l'écrire sur autant de tags que voulu", Teal) { NfcBus.start(NfcOp.CloneRead) },
        Tool(Icons.Filled.CleaningServices, "Effacer", "Vide le contenu du tag", Amber) { confirm = "Effacer tout le contenu du prochain tag ?" to NfcOp.Erase },
        Tool(Icons.Filled.AutoFixHigh, "Formater NDEF", "Prépare un tag neuf ou non formaté", Amber) { NfcBus.start(NfcOp.Format) },
        Tool(Icons.Filled.Key, "Mot de passe", "Protéger / déprotéger un NTAG213/215/216", Violet) { Nav.push(Sub.Password) },
        Tool(Icons.Filled.Lock, "Verrouiller", "Lecture seule DÉFINITIVE", Danger) { confirm = "Le prochain tag sera verrouillé À VIE : plus aucune modification possible. Continuer ?" to NfcOp.Lock },
        Tool(Icons.Filled.PhoneAndroid, "Émulation de tag", "Votre téléphone devient un tag NFC", Violet) { Nav.push(Sub.Emulation) },
        Tool(Icons.Filled.Calculate, "Capacités des puces", "Quel tag acheter pour quel usage", Teal) { Nav.push(Sub.Capacity) },
        Tool(Icons.Filled.School, "Guide « sans appli »", "Voix Google, Wi-Fi, contact… ce qui marche partout", Teal) { Nav.push(Sub.Guide) },
        Tool(Icons.Filled.RecordVoiceOver, "Tester la voix", "Écouter la synthèse vocale du téléphone", Violet) {
            TaskActivity.launch(ctx, listOf(TaskItem(TaskType.SPEAK, listOf("Bonjour ! Voici la voix qui sera utilisée par vos tags TagForge.", "fr-FR", "1.0"))))
        },
        Tool(Icons.Filled.History, "Historique", "Tous les tags lus et écrits", Teal) { Nav.push(Sub.History) },
        Tool(Icons.Filled.Settings, "Réglages & autorisations", "Son, vibration, export, permissions des tâches", Teal) { Nav.push(Sub.Settings) },
        Tool(Icons.Filled.Nfc, "Réglages NFC du téléphone", "Activer le NFC, paiement sans contact", Teal) {
            ctx.startActivity(Intent(Settings.ACTION_NFC_SETTINGS))
        },
    )
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState())) {
        ScreenHeader("Outils", "${tools.size} outils NFC")
        Column(Modifier.padding(horizontal = 16.dp)) {
            tools.chunked(2).forEach { row ->
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp), modifier = Modifier.padding(vertical = 5.dp)) {
                    row.forEach { t ->
                        Panel(Modifier.weight(1f).heightIn(min = 132.dp), onClick = t.action) {
                            IconBubble(t.icon, t.color)
                            Spacer(Modifier.height(10.dp))
                            Text(t.title, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                            Text(t.desc, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                    }
                    if (row.size == 1) Spacer(Modifier.weight(1f))
                }
            }
            Spacer(Modifier.height(24.dp))
        }
    }
    confirm?.let { (msg, op) ->
        AlertDialog(
            onDismissRequest = { confirm = null },
            title = { Text("Confirmer") }, text = { Text(msg) },
            confirmButton = { TextButton(onClick = { NfcBus.start(op); confirm = null }) { Text("Continuer", color = Danger) } },
            dismissButton = { TextButton(onClick = { confirm = null }) { Text("Annuler") } },
        )
    }
}

@Composable
fun PasswordScreen() {
    var pwd by remember { mutableStateOf("") }
    var current by remember { mutableStateOf("") }
    var protectRead by remember { mutableStateOf(false) }
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState())) {
        ScreenHeader("Mot de passe", "NTAG213 · NTAG215 · NTAG216 · Ultralight EV1", onBack = { Nav.pop() })
        Column(Modifier.padding(horizontal = 16.dp)) {
            InfoNote("La puce stocke un mot de passe de 32 bits. Saisissez 8 chiffres hexadécimaux (ex. 1A2B3C4D) pour le définir exactement, " +
                "ou n'importe quel texte : TagForge en dérive la clé. Le contenu reste lisible (sauf option), mais plus personne ne peut le modifier sans le mot de passe.")
            SectionTitle("Protéger un tag")
            Panel {
                Field(pwd, { pwd = it }, "Nouveau mot de passe", password = true)
                Field(current, { current = it }, "Mot de passe actuel (si déjà protégé)", password = true)
                SwitchRow("Protéger aussi la lecture", protectRead, "Le tag ne sera plus lisible sans mot de passe (et ne s'ouvrira plus seul).") { protectRead = it }
                Button(onClick = { NfcBus.start(NfcOp.SetPassword(pwd, protectRead, current.ifBlank { null })) }, Modifier.fillMaxWidth(), enabled = pwd.isNotBlank()) {
                    Icon(Icons.Filled.Lock, null); Spacer(Modifier.width(6.dp)); Text("Protéger")
                }
            }
            SectionTitle("Retirer la protection")
            var rm by remember { mutableStateOf("") }
            Panel {
                Field(rm, { rm = it }, "Mot de passe du tag", password = true)
                OutlinedButton(onClick = { NfcBus.start(NfcOp.RemovePassword(rm)) }, Modifier.fillMaxWidth(), enabled = rm.isNotBlank()) {
                    Icon(Icons.Filled.LockOpen, null); Spacer(Modifier.width(6.dp)); Text("Retirer le mot de passe")
                }
            }
            Spacer(Modifier.height(12.dp))
            InfoNote("Pour réécrire un tag protégé : onglet Écrire › « Mot de passe actuel du tag ». Attention : après plusieurs essais faux, certaines puces se bloquent.", Icons.Filled.Warning, Amber)
            Spacer(Modifier.height(24.dp))
        }
    }
}

@Composable
fun EmulationScreen() {
    val ctx = LocalContext.current
    DisposableEffect(Unit) { NfcBus.emulating = true; onDispose { NfcBus.emulating = false } }
    val ndef = Store.emulationNdef
    val parsed = remember(ndef, Store.emulationLabel) { ndef?.let { runCatching { NdefParser.parse(NdefMessage(it)) }.getOrNull() } }
    val hce = remember { ctx.packageManager.hasSystemFeature("android.hardware.nfc.hce") }
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState())) {
        ScreenHeader("Émulation de tag", "Votre téléphone se comporte comme un tag", onBack = { Nav.pop() })
        Column(Modifier.padding(horizontal = 16.dp)) {
            if (!hce) InfoNote("Ce téléphone ne prend pas en charge l'émulation de carte (HCE).", Icons.Filled.Warning, Danger)
            Panel {
                Box(Modifier.fillMaxWidth().height(160.dp), contentAlignment = Alignment.Center) {
                    NfcPulse(Modifier.size(150.dp), if (Store.emulationOn) Violet else MaterialTheme.colorScheme.outline)
                }
                SwitchRow("Émulation active", Store.emulationOn, "Un autre téléphone qui approche le dos de celui-ci lit le contenu ci-dessous.") {
                    Store.emulationOn = it; Store.saveSettings()
                }
            }
            SectionTitle("Contenu émulé")
            Panel {
                if (parsed.isNullOrEmpty()) Text("Aucun contenu. Préparez-le dans l'onglet Écrire puis touchez « Émuler », ou copiez un tag existant.")
                else {
                    Text(Store.emulationLabel, fontWeight = FontWeight.Bold)
                    parsed.forEach { Text("${it.kind} : ${it.value}", style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(top = 4.dp)) }
                }
                Spacer(Modifier.height(8.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(onClick = {
                        runCatching { WriteState.message() }.onSuccess { Store.emulationNdef = it.toByteArray(); Store.emulationLabel = WriteState.label(); Store.saveSettings() }
                            .onFailure { ctx.toast("Préparez d'abord un contenu dans l'onglet Écrire") }
                    }, Modifier.weight(1f)) { Text("Depuis l'éditeur") }
                    OutlinedButton(onClick = { NfcBus.emulating = false; NfcBus.start(NfcOp.Emulate) }, Modifier.weight(1f)) { Text("Copier un tag") }
                }
            }
            Spacer(Modifier.height(12.dp))
            InfoNote("Gardez l'écran allumé et déverrouillé. La lecture du NFC est suspendue sur cet écran pour laisser place à l'émulation. " +
                "Compatible avec les téléphones Android (lecture NFC Type 4). Les iPhone lisent aussi ce type de tag via l'appli Raccourcis ou une appli NFC.")
            if (Build.VERSION.SDK_INT >= 26) {
                Spacer(Modifier.height(8.dp))
                TextButton(onClick = { runCatching { ctx.startActivity(Intent(Settings.ACTION_NFC_PAYMENT_SETTINGS)) } }) { Text("Réglages « Sans contact » du téléphone") }
            }
            Spacer(Modifier.height(24.dp))
        }
    }
    LaunchedEffect(NfcBus.pending) { if (NfcBus.pending == null) NfcBus.emulating = true }
}

@Composable
fun SettingsScreen() {
    val ctx = LocalContext.current
    val exportLauncher = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri: Uri? ->
        uri?.let { u -> runCatching { ctx.contentResolver.openOutputStream(u)?.use { it.write(Store.exportJson().toByteArray()) } }
            .onSuccess { ctx.toast("${Store.templates.size} modèles exportés") }.onFailure { ctx.toast("Échec de l'export") } }
    }
    val importLauncher = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        uri?.let { u -> runCatching { ctx.contentResolver.openInputStream(u)?.use { Store.importJson(it.readBytes().toString(Charsets.UTF_8)) } ?: 0 }
            .onSuccess { ctx.toast("$it modèle(s) importé(s)") }.onFailure { ctx.toast("Fichier invalide") } }
    }
    val notifLauncher = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { ctx.toast(if (it) "Notifications autorisées" else "Refusé") }
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState())) {
        ScreenHeader("Réglages", onBack = { Nav.pop() })
        Column(Modifier.padding(horizontal = 16.dp)) {
            SectionTitle("Lecture")
            Panel {
                SwitchRow("Lire toute la mémoire automatiquement", Store.autoDump, "Puce exacte, signature, compteur, pages") { Store.autoDump = it; Store.saveSettings() }
                SwitchRow("Son de confirmation", Store.sound) { Store.sound = it; Store.saveSettings() }
                SwitchRow("Vibration", Store.vibrate) { Store.vibrate = it; Store.saveSettings() }
            }
            SectionTitle("Autorisations des tâches automatiques")
            Panel {
                val nm = ctx.getSystemService(NotificationManager::class.java)
                PermRow("Mode silencieux / Ne pas déranger", nm.isNotificationPolicyAccessGranted) { ctx.startActivity(Intent(Settings.ACTION_NOTIFICATION_POLICY_ACCESS_SETTINGS)) }
                PermRow("Luminosité (paramètres système)", Settings.System.canWrite(ctx)) {
                    ctx.startActivity(Intent(Settings.ACTION_MANAGE_WRITE_SETTINGS, Uri.parse("package:" + ctx.packageName)))
                }
                if (Build.VERSION.SDK_INT >= 33) PermRow("Notifications", nm.areNotificationsEnabled()) { notifLauncher.launch(Manifest.permission.POST_NOTIFICATIONS) }
                PermRow("Voix de synthèse (Services vocaux Google)", null) {
                    runCatching { ctx.startActivity(Intent("com.android.settings.TTS_SETTINGS")) }
                }
            }
            SectionTitle("Données")
            Panel {
                OutlinedButton(onClick = { exportLauncher.launch("tagforge-modeles.json") }, Modifier.fillMaxWidth()) { Icon(Icons.Filled.FileUpload, null); Spacer(Modifier.width(6.dp)); Text("Exporter les modèles") }
                OutlinedButton(onClick = { importLauncher.launch(arrayOf("application/json", "text/plain", "*/*")) }, Modifier.fillMaxWidth()) { Icon(Icons.Filled.FileDownload, null); Spacer(Modifier.width(6.dp)); Text("Importer des modèles") }
                OutlinedButton(onClick = { Store.clearHistory(); ctx.toast("Historique effacé") }, Modifier.fillMaxWidth()) { Icon(Icons.Filled.DeleteSweep, null); Spacer(Modifier.width(6.dp)); Text("Effacer l'historique") }
            }
            Spacer(Modifier.height(16.dp))
            Text("TagForge NFC 1.0 · Superessence Studio\nAucune donnée n'est envoyée : tout reste sur le téléphone.", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Spacer(Modifier.height(24.dp))
        }
    }
}

@Composable
private fun PermRow(label: String, granted: Boolean?, onClick: () -> Unit) {
    Row(Modifier.fillMaxWidth().padding(vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
        Icon(when (granted) { true -> Icons.Filled.CheckCircle; false -> Icons.Filled.RadioButtonUnchecked; null -> Icons.Filled.Tune }, null,
            tint = if (granted == true) Ok else MaterialTheme.colorScheme.onSurfaceVariant)
        Spacer(Modifier.width(10.dp))
        Text(label, Modifier.weight(1f), style = MaterialTheme.typography.bodyMedium)
        TextButton(onClick = onClick) { Text(if (granted == true) "Gérer" else "Ouvrir") }
    }
}

@Composable
fun CapacityScreen() {
    val rows = listOf(
        listOf("NTAG213", "144 o", "Lien court, téléphone, Wi-Fi, petite tâche", "Le plus courant, pas cher"),
        listOf("NTAG215", "504 o", "Carte de visite, voix Google, scénarios", "Polyvalent (aussi utilisé pour les figurines)"),
        listOf("NTAG216", "888 o", "vCard complète, longs textes, nombreux contenus", "Le plus grand des NTAG"),
        listOf("NTAG424 DNA", "≈ 256 o", "Authentification sécurisée (SUN)", "Lecture NDEF OK"),
        listOf("MIFARE Classic 1K", "716 o", "Badges d'accès anciens", "Lisible seulement sur puces NXP"),
        listOf("MIFARE Ultralight", "46 o", "Tickets de transport", "Très petit"),
        listOf("ICODE SLIX (NFC-V)", "112 o", "Bibliothèques, longue portée", "ISO 15693"),
        listOf("NTAG I²C plus", "1 912 o", "Électronique connectée", "Interface I²C"),
    )
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState())) {
        ScreenHeader("Capacités des puces", "Quel tag choisir ?", onBack = { Nav.pop() })
        Column(Modifier.padding(horizontal = 16.dp)) {
            rows.forEach { r ->
                Panel(Modifier.padding(vertical = 4.dp)) {
                    Row { Text(r[0], Modifier.weight(1f), fontWeight = FontWeight.Bold); Text(r[1], color = Teal, fontWeight = FontWeight.Bold) }
                    Text(r[2], style = MaterialTheme.typography.bodySmall)
                    Text(r[3], style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
            SectionTitle("Ordres de grandeur")
            Panel {
                KeyValue("Lien court", "≈ 20 – 40 o")
                KeyValue("Wi-Fi", "≈ 70 – 110 o")
                KeyValue("Voix Google", "≈ 80 – 260 o")
                KeyValue("Tâche TagForge", "≈ 60 – 200 o")
                KeyValue("Carte de visite", "≈ 120 – 400 o")
            }
            Spacer(Modifier.height(12.dp))
            InfoNote("Conseil : prenez des NTAG215 ou NTAG216 « anti-métal » si le tag est collé sur un support métallique (frigo, voiture). Le métal bloque les tags normaux.")
            Spacer(Modifier.height(24.dp))
        }
    }
}

@Composable
fun GuideScreen() {
    val ctx = LocalContext.current
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState())) {
        ScreenHeader("Guide", "Ce que fait un tag, avec ou sans appli", onBack = { Nav.pop() })
        Column(Modifier.padding(horizontal = 16.dp)) {
            SectionTitle("🔊 La voix Google à l'approche, sans appli")
            Panel {
                Text("Choisissez « Voix Google — lecture à voix haute ». Le tag contient un lien vers la synthèse vocale de Google : " +
                    "n'importe quel Android déverrouillé ouvre le navigateur, qui lit le texte avec la voix Google. Aucune appli à installer.", style = MaterialTheme.typography.bodyMedium)
                Spacer(Modifier.height(6.dp))
                Text("• 200 caractères maximum · Internet nécessaire\n• Service Google non officiel : s'il change, utilisez « Google Traduction » (un appui sur le haut-parleur)\n" +
                    "• Avec TagForge installé : « Annonce vocale instantanée », sans Internet, sans limite, appli fermée", style = MaterialTheme.typography.bodySmall)
                TextButton(onClick = { ctx.openUri("https://translate.google.com/translate_tts?ie=UTF-8&client=tw-ob&tl=fr&q=" + Encoders.enc("Bonjour, ceci est un test de la voix Google.")) }) { Text("▶ Essayer maintenant") }
            }
            SectionTitle("🎙️ Et l'Assistant Google / Gemini ?")
            Panel {
                Text("Android ne permet à aucun tag de lancer l'Assistant sans appli : c'est une protection du système. Avec TagForge installé, le contenu " +
                    "« Assistant Google / Gemini » le lance instantanément à l'approche. Vous pouvez aussi ajouter un tag à une routine via « Ouvrir une application ».", style = MaterialTheme.typography.bodyMedium)
            }
            SectionTitle("✅ Fonctionne partout, sans appli")
            Panel {
                listOf(
                    "Liens web, Smart Poster, recherche Google, vidéo YouTube", "Appel, SMS, e-mail, WhatsApp, Telegram",
                    "Carte de visite (ajout au répertoire)", "Wi-Fi : connexion sans mot de passe à taper", "Appairage Bluetooth (enceinte, casque)",
                    "Position GPS, Google Maps, itinéraire, Waze", "Profils Instagram, TikTok, YouTube, LinkedIn, Snapchat…",
                    "Avis Google, agenda, paiement PayPal / Revolut", "Ouvrir n'importe quelle appli (enregistrement AAR)",
                ).forEach { Text("• $it", style = MaterialTheme.typography.bodyMedium, modifier = Modifier.padding(vertical = 2.dp)) }
            }
            SectionTitle("⚡ Avec TagForge installé (appli fermée)")
            Panel {
                listOf(
                    "Parler à voix haute (hors connexion)", "Assistant Google / Gemini", "Lampe torche", "Silencieux, vibreur, Ne pas déranger",
                    "Volume, musique (lecture, suivant…)", "Alarme, minuteur", "Luminosité", "Ouvrir une appli, un réglage (Wi-Fi, Bluetooth…)",
                    "Notification, presse-papiers, vibration, itinéraire", "Scénarios : plusieurs actions enchaînées",
                ).forEach { Text("• $it", style = MaterialTheme.typography.bodyMedium, modifier = Modifier.padding(vertical = 2.dp)) }
            }
            SectionTitle("📱 Bon à savoir")
            Panel {
                Text("• Le téléphone lit les tags écran allumé et déverrouillé (sécurité Android).\n" +
                    "• La zone NFC est au dos, souvent près de l'appareil photo. Une coque épaisse ou métallique gêne la lecture.\n" +
                    "• Android n'ouvre automatiquement que le premier contenu du tag.\n" +
                    "• iPhone (XS et plus récents) : liens, appels, SMS, e-mails et voix Google s'ouvrent aussi en arrière-plan ; Wi-Fi et tâches TagForge non.\n" +
                    "• Un tag verrouillé ne peut plus jamais être modifié : préférez le mot de passe.", style = MaterialTheme.typography.bodyMedium)
            }
            Spacer(Modifier.height(24.dp))
        }
    }
}

@Composable
fun LibraryScreen() {
    val ctx = LocalContext.current
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState())) {
        ScreenHeader("Modèles", "${Store.templates.size} modèles enregistrés") {
            IconButton(onClick = { Nav.push(Sub.History) }) { Icon(Icons.Filled.History, "Historique") }
        }
        Column(Modifier.padding(horizontal = 16.dp)) {
            if (Store.templates.isEmpty()) InfoNote("Aucun modèle. Dans l'onglet Écrire, touchez « Modèle » pour enregistrer un contenu réutilisable.")
            Store.templates.toList().forEach { t ->
                Panel(Modifier.padding(vertical = 4.dp)) {
                    Text(t.name, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                    Text(t.items.joinToString(" + ") { it.type?.let { ty -> runCatching { ty.summary(it.values) }.getOrDefault(ty.title) } ?: it.typeId },
                        style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 3)
                    Row {
                        TextButton(onClick = { WriteState.load(t); Nav.tab = Tab.WRITE }) { Icon(Icons.Filled.Edit, null, Modifier.size(18.dp)); Spacer(Modifier.width(4.dp)); Text("Modifier") }
                        TextButton(onClick = { WriteState.load(t); WriteState.startWrite(ctx) }) { Icon(Icons.Filled.Nfc, null, Modifier.size(18.dp)); Spacer(Modifier.width(4.dp)); Text("Écrire") }
                        Spacer(Modifier.weight(1f))
                        IconButton(onClick = { Store.deleteTemplate(t) }) { Icon(Icons.Filled.Delete, "Supprimer") }
                    }
                }
            }
            Spacer(Modifier.height(24.dp))
        }
    }
}

@Composable
fun HistoryScreen() {
    val ctx = LocalContext.current
    val fmt = remember { SimpleDateFormat("dd/MM/yyyy HH:mm", Locale.FRANCE) }
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState())) {
        ScreenHeader("Historique", "${Store.history.size} opérations", onBack = { Nav.pop() }) {
            IconButton(onClick = { ctx.shareText(Store.history.joinToString("\n\n") { "${fmt.format(Date(it.time))} · ${it.action} · ${it.uid}\n${it.detail}" }) }) { Icon(Icons.Filled.Share, "Partager") }
        }
        Column(Modifier.padding(horizontal = 16.dp)) {
            if (Store.history.isEmpty()) InfoNote("Rien pour l'instant. Approchez un tag !")
            Store.history.toList().forEach { h ->
                var open by remember(h.time) { mutableStateOf(false) }
                Panel(Modifier.padding(vertical = 4.dp), onClick = { open = !open }) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        IconBubble(when (h.action) { "Écriture" -> Icons.Filled.EditNote; "Tâches" -> Icons.Filled.Bolt; else -> Icons.Filled.Contactless }, size = 34)
                        Spacer(Modifier.width(10.dp))
                        Column(Modifier.weight(1f)) {
                            Text("${h.action} · ${fmt.format(Date(h.time))}", style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.primary)
                            Text(h.summary, style = MaterialTheme.typography.bodyMedium, maxLines = if (open) 10 else 1)
                            if (h.uid.isNotBlank()) Text(listOf(h.chip, h.uid).filter { it.isNotBlank() }.joinToString(" · "), style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                    }
                    if (open && h.detail.isNotBlank()) Text(h.detail, style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(top = 8.dp))
                }
            }
            Spacer(Modifier.height(24.dp))
        }
    }
}
