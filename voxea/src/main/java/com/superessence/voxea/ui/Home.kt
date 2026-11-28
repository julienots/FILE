package com.superessence.voxea.ui

import android.content.ClipboardManager
import android.content.Context
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
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
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.ArrowBack
import androidx.compose.material.icons.outlined.ContentPaste
import androidx.compose.material.icons.outlined.Delete
import androidx.compose.material.icons.outlined.Description
import androidx.compose.material.icons.outlined.Lightbulb
import androidx.compose.material.icons.outlined.Settings
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
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
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.voxea.core.VoiceType
import com.superessence.voxea.data.Project
import com.superessence.voxea.data.Settings

@Composable
fun HomeScreen(vm: AppViewModel) {
    val projects by vm.projects.collectAsState()
    vm.recVersion.collectAsState().value
    val ctx = LocalContext.current
    var paste by remember { mutableStateOf(false) }
    var toDelete by remember { mutableStateOf<Project?>(null) }
    val open = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri -> uri?.let(vm::importUri) }

    Column(Modifier.fillMaxSize().background(Vx.Bg)) {
        Row(Modifier.fillMaxWidth().padding(start = 20.dp, end = 8.dp, top = 12.dp), verticalAlignment = Alignment.CenterVertically) {
            Emblem(Modifier.size(38.dp, 44.dp))
            Spacer(Modifier.width(10.dp))
            Column(Modifier.weight(1f)) {
                Text("VOXÉA", style = MaterialTheme.typography.headlineMedium)
                Text("by SuperEssence Studio", style = MaterialTheme.typography.bodySmall, color = Vx.Muted)
            }
            IconButton(onClick = { vm.go(Screen.SettingsScreen) }) { Icon(Icons.Outlined.Settings, "Réglages", tint = Vx.Muted) }
        }
        LazyColumn(Modifier.weight(1f), contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            item {
                Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(22.dp)).background(Vx.Surface).padding(18.dp)) {
                    Text("Nouveau dialogue", style = MaterialTheme.typography.titleLarge)
                    Text("Importe le script (par exemple écrit par Claude), lis-le, et récupère un MP3 par réplique.",
                        style = MaterialTheme.typography.bodyMedium, color = Vx.Muted, modifier = Modifier.padding(top = 4.dp, bottom = 14.dp))
                    BigAction(Icons.Outlined.Description, "Importer un fichier texte", Vx.Magenta) { open.launch(arrayOf("text/*", "application/octet-stream")) }
                    Spacer(Modifier.height(10.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                        SmallAction(Icons.Outlined.ContentPaste, "Coller le texte", Modifier.weight(1f)) { paste = true }
                        SmallAction(Icons.Outlined.Lightbulb, "Exemple", Modifier.weight(1f)) { vm.importText(AppViewModel.EXAMPLE, "Exemple — La gare") }
                    }
                }
            }
            item { FormatHelp() }
            if (projects.isNotEmpty()) item { Text("Mes dialogues", style = MaterialTheme.typography.titleMedium, modifier = Modifier.padding(top = 6.dp, start = 4.dp)) }
            items(projects, key = { it.id }) { p ->
                val done = vm.store.recordedCount(p)
                Row(
                    Modifier.fillMaxWidth().clip(RoundedCornerShape(18.dp)).background(Vx.Surface)
                        .clickable { vm.go(Screen.Edit(p.id)) }.padding(start = 16.dp, top = 14.dp, bottom = 14.dp, end = 4.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Column(Modifier.weight(1f)) {
                        Text(p.name, style = MaterialTheme.typography.titleMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
                        Text("${p.lines.size} répliques · ${p.characters.size} personnages", style = MaterialTheme.typography.bodySmall, color = Vx.Muted)
                        Row(Modifier.padding(top = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                            LinearProgressIndicator({ done / p.lines.size.coerceAtLeast(1).toFloat() }, Modifier.weight(1f).height(6.dp).clip(CircleShape),
                                color = if (done == p.lines.size) Vx.Ok else Vx.Magenta, trackColor = Vx.Surface2, drawStopIndicator = {})
                            Text("  $done/${p.lines.size}", style = MaterialTheme.typography.bodySmall, color = Vx.Muted)
                        }
                    }
                    IconButton(onClick = { toDelete = p }) { Icon(Icons.Outlined.Delete, "Supprimer", tint = Vx.Muted) }
                }
            }
        }
    }

    if (paste) PasteDialog(
        initial = (ctx.getSystemService(Context.CLIPBOARD_SERVICE) as? ClipboardManager)?.primaryClip?.getItemAt(0)?.coerceToText(ctx)?.toString().orEmpty(),
        onDismiss = { paste = false },
        onOk = { t, n -> paste = false; vm.importText(t, n) },
    )
    toDelete?.let { p ->
        AlertDialog(
            onDismissRequest = { toDelete = null },
            title = { Text("Supprimer « ${p.name} » ?") },
            text = { Text("Le script, tes enregistrements et les MP3 de l'application seront effacés. Les MP3 déjà copiés dans Musique restent.") },
            confirmButton = { TextButton({ vm.delete(p); toDelete = null }) { Text("Supprimer", color = Vx.Rec) } },
            dismissButton = { TextButton({ toDelete = null }) { Text("Annuler") } },
        )
    }
}

@Composable
private fun FormatHelp() {
    var open by remember { mutableStateOf(false) }
    Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(18.dp)).background(Vx.Surface.copy(alpha = 0.6f)).clickable { open = !open }.padding(16.dp)) {
        Text("Format du script ${if (open) "▴" else "▾"}", style = MaterialTheme.typography.titleMedium, color = Vx.Cyan)
        if (open) {
            Text(
                "Une réplique par ligne : NOM : texte\n" +
                    "Ajoute la voix entre crochets ou parenthèses : [FEMME], [HOMME], [ENFANT], [ÂGÉ], [ROBOT], [MA VOIX].\n" +
                    "Une ligne sans nom = narrateur. Les titres (# …) sont ignorés.\n\n" +
                    "Astuce pour Claude : « Écris un dialogue au format NOM [VOIX] : réplique ».",
                style = MaterialTheme.typography.bodyMedium, color = Vx.Muted, modifier = Modifier.padding(top = 8.dp),
            )
            Text("LÉA [FEMME] : Tu viens ?\nMARC [HOMME] : J'arrive !", fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace,
                fontSize = 13.sp, color = Vx.Cream, modifier = Modifier.padding(top = 10.dp).clip(RoundedCornerShape(10.dp)).background(Vx.Bg).padding(10.dp).fillMaxWidth())
        }
    }
}

@Composable
fun BigAction(icon: ImageVector, label: String, color: Color, onClick: () -> Unit) {
    Row(
        Modifier.fillMaxWidth().heightIn(min = 56.dp).clip(RoundedCornerShape(16.dp)).background(color).clickable(onClick = onClick).padding(horizontal = 18.dp),
        verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.Center,
    ) {
        Icon(icon, null, tint = Color.White)
        Spacer(Modifier.width(10.dp))
        Text(label, color = Color.White, style = MaterialTheme.typography.labelLarge, fontSize = 16.sp)
    }
}

@Composable
fun SmallAction(icon: ImageVector, label: String, modifier: Modifier = Modifier, onClick: () -> Unit) {
    Row(
        modifier.heightIn(min = 48.dp).clip(RoundedCornerShape(14.dp)).background(Vx.Surface2).clickable(onClick = onClick).padding(horizontal = 12.dp),
        verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.Center,
    ) {
        Icon(icon, null, tint = Vx.Cream, modifier = Modifier.size(20.dp))
        Spacer(Modifier.width(8.dp))
        Text(label, style = MaterialTheme.typography.labelLarge, maxLines = 1)
    }
}

@Composable
fun PasteDialog(initial: String, onDismiss: () -> Unit, onOk: (String, String) -> Unit) {
    var text by remember { mutableStateOf(initial) }
    var name by remember { mutableStateOf("") }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Coller le script") },
        text = {
            Column {
                OutlinedTextField(name, { name = it }, label = { Text("Nom du dialogue") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                Spacer(Modifier.height(8.dp))
                OutlinedTextField(text, { text = it }, label = { Text("Script") }, modifier = Modifier.fillMaxWidth().heightIn(min = 180.dp, max = 320.dp))
            }
        },
        confirmButton = { TextButton({ onOk(text, name) }, enabled = text.isNotBlank()) { Text("Créer") } },
        dismissButton = { TextButton(onDismiss) { Text("Annuler") } },
    )
}

@Composable
fun SettingsScreen(vm: AppViewModel) {
    val s = vm.settings
    var key by remember { mutableStateOf(s.apiKey) }
    var volume by remember { mutableStateOf(s.volumeKeys) }
    val ids = remember { VoiceType.entries.filter { it != VoiceType.ORIGINAL }.associateWith { mutableStateOf(s.voiceId(it)) } }
    Column(Modifier.fillMaxSize().background(Vx.Bg).verticalScroll(rememberScrollState()).padding(20.dp)) {
        TopBack("Réglages") { vm.back() }
        Section("Prompteur") {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text("Boutons de volume = « Suivant »", style = MaterialTheme.typography.titleMedium)
                    Text("Pratique avec des écouteurs ou le téléphone posé.", style = MaterialTheme.typography.bodySmall, color = Vx.Muted)
                }
                Switch(volume, { volume = it; s.volumeKeys = it })
            }
        }
        Section("Voix IA (facultatif)") {
            Text(
                "Par défaut, VOXÉA transforme ta voix sur le téléphone, sans Internet et gratuitement. " +
                    "Pour un rendu plus réaliste, tu peux utiliser ElevenLabs (compte payant) : ton intonation est gardée, la voix est remplacée. " +
                    "La clé reste sur ce téléphone et n'est envoyée qu'à api.elevenlabs.io.",
                style = MaterialTheme.typography.bodyMedium, color = Vx.Muted,
            )
            Spacer(Modifier.height(10.dp))
            OutlinedTextField(key, { key = it; s.apiKey = it }, label = { Text("Clé API ElevenLabs") }, singleLine = true,
                visualTransformation = PasswordVisualTransformation(), modifier = Modifier.fillMaxWidth())
            Spacer(Modifier.height(10.dp))
            Text("Identifiant de voix ElevenLabs pour chaque type (vide = hors ligne pour ce type) :", style = MaterialTheme.typography.bodySmall, color = Vx.Muted)
            ids.forEach { (t, st) ->
                OutlinedTextField(st.value, { st.value = it; s.setVoiceId(t, it) }, label = { Text("${t.emoji()} ${t.label}") }, singleLine = true,
                    modifier = Modifier.fillMaxWidth().padding(top = 6.dp))
            }
            TextButton({ Settings.DEFAULT_VOICES.forEach { (t, id) -> ids[t]?.value = id; s.setVoiceId(t, id) } }) { Text("Remettre les voix par défaut") }
        }
        Section("À propos") {
            Text("VOXÉA 1.0 — SuperEssence Studio.\nEncodage MP3 : LAME (jump3r, LGPL). Aucun compte, aucune publicité, tes enregistrements restent sur ton téléphone.",
                style = MaterialTheme.typography.bodyMedium, color = Vx.Muted)
        }
    }
}

@Composable
fun TopBack(title: String, onBack: () -> Unit) {
    Row(Modifier.fillMaxWidth().padding(bottom = 8.dp), verticalAlignment = Alignment.CenterVertically) {
        IconButton(onClick = onBack) { Icon(Icons.AutoMirrored.Outlined.ArrowBack, "Retour") }
        Text(title, style = MaterialTheme.typography.headlineMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
    }
}

@Composable
fun Section(title: String, content: @Composable () -> Unit) {
    Column(Modifier.fillMaxWidth().padding(top = 14.dp).clip(RoundedCornerShape(18.dp)).background(Vx.Surface).padding(16.dp)) {
        Text(title, style = MaterialTheme.typography.titleLarge, modifier = Modifier.padding(bottom = 8.dp))
        content()
    }
}

