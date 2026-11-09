package com.superessence.noctea.ui.screens

import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.MenuBook
import androidx.compose.material.icons.filled.BarChart
import androidx.compose.material.icons.filled.Bedtime
import androidx.compose.material.icons.filled.ChevronRight
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.History
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.RecordVoiceOver
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Star
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.superessence.noctea.BuildConfig
import com.superessence.noctea.R
import com.superessence.noctea.data.Discovery
import com.superessence.noctea.data.JournalEntry
import com.superessence.noctea.tts.EmbeddedVoice
import com.superessence.noctea.tts.RemoteHttpTtsProvider
import com.superessence.noctea.ui.LocalApp
import com.superessence.noctea.ui.LocalNav
import com.superessence.noctea.ui.Routes
import com.superessence.noctea.ui.components.GlassCard
import com.superessence.noctea.ui.components.MoonMark
import com.superessence.noctea.ui.components.NightBackground
import com.superessence.noctea.ui.components.NightChip
import com.superessence.noctea.ui.components.NocteaWordmark
import com.superessence.noctea.ui.components.PrimaryButton
import com.superessence.noctea.ui.components.SecondaryButton
import com.superessence.noctea.ui.components.formatDuration
import com.superessence.noctea.ui.theme.Cormorant
import com.superessence.noctea.ui.theme.LocalPalette
import com.superessence.noctea.ui.theme.Palettes
import kotlinx.coroutines.launch
import java.io.File
import java.time.LocalDate
import java.time.format.TextStyle
import java.util.Locale
import java.util.UUID

// =================================================================== PROFIL
@Composable
fun ProfileScreen() {
    val app = LocalApp.current
    val nav = LocalNav.current
    val p = LocalPalette.current
    val snap by app.store.state.collectAsState()
    val totalMs = snap.practiceDays.values.sum()
    val streak = Discovery.streak(snap.practiceDays, LocalDate.now())
    NightBackground {
        Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(TabContentPadding)) {
            Column(Modifier.statusBarsPadding().padding(20.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                Box(
                    Modifier.size(92.dp).clip(CircleShape).background(Brush.linearGradient(listOf(p.accent.copy(alpha = 0.5f), p.moon.copy(alpha = 0.4f)))),
                    contentAlignment = Alignment.Center,
                ) {
                    Text(snap.settings.userName.take(1).uppercase().ifBlank { "☾" }, fontFamily = Cormorant, fontSize = 42.sp, color = Color.White)
                }
                Spacer(Modifier.height(10.dp))
                Text(snap.settings.userName.ifBlank { "Voyageur de la nuit" }, style = MaterialTheme.typography.headlineMedium)
                Text(if (app.entitlements.isPremium) "NOCTÉA Premium · offre de lancement" else "Version gratuite", style = MaterialTheme.typography.bodySmall, color = p.moon)
            }
            Row(Modifier.padding(horizontal = 20.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                StatBox("${totalMs / 60_000}", "minutes de calme", Modifier.weight(1f))
                StatBox("$streak", if (streak > 1) "soirées d'affilée" else "soirée d'affilée", Modifier.weight(1f))
                StatBox("${snap.practiceDays.count { it.value >= 60_000 }}", "jours de pratique", Modifier.weight(1f))
            }
            Spacer(Modifier.height(18.dp))
            MenuRow(Icons.Filled.Bedtime, "Mon rituel du soir", "Coucher, rappel et réveil doux") { nav.navigate(Routes.RITUAL) }
            MenuRow(Icons.AutoMirrored.Filled.MenuBook, "Journal du soir", "Humeur, pensées et gratitude") { nav.navigate(Routes.JOURNAL) }
            MenuRow(Icons.Filled.BarChart, "Mes statistiques", "Calendrier et habitudes, en douceur") { nav.navigate(Routes.STATS) }
            MenuRow(Icons.Filled.History, "Historique d'écoute", "${snap.history.size} écoutes") { nav.navigate(Routes.HISTORY) }
            MenuRow(Icons.Filled.RecordVoiceOver, "Voix & synthèse vocale", "Voix NOCTÉA, voix de l'appareil, serveur") { nav.navigate(Routes.VOICE) }
            MenuRow(Icons.Filled.Settings, "Réglages", "Thème, mode nuit, notifications, données") { nav.navigate(Routes.SETTINGS) }
            MenuRow(Icons.Filled.Star, "NOCTÉA Premium", "Ce qui est inclus") { nav.navigate(Routes.PREMIUM) }
            MenuRow(Icons.Filled.Info, "À propos", "Une création Superessence Studio") { nav.navigate(Routes.ABOUT) }
        }
    }
}

@Composable
private fun StatBox(value: String, label: String, modifier: Modifier = Modifier) {
    GlassCard(modifier, padding = 12.dp) {
        Text(value, style = MaterialTheme.typography.headlineMedium, color = LocalPalette.current.moon)
        Text(label, style = MaterialTheme.typography.bodySmall, maxLines = 2)
    }
}

@Composable
private fun MenuRow(icon: ImageVector, title: String, subtitle: String, onClick: () -> Unit) {
    val p = LocalPalette.current
    Row(Modifier.fillMaxWidth().clickable(onClick = onClick).padding(horizontal = 20.dp, vertical = 14.dp), verticalAlignment = Alignment.CenterVertically) {
        Icon(icon, null, tint = p.accent)
        Spacer(Modifier.width(16.dp))
        Column(Modifier.weight(1f)) {
            Text(title, style = MaterialTheme.typography.titleMedium)
            Text(subtitle, style = MaterialTheme.typography.bodySmall)
        }
        Icon(Icons.Filled.ChevronRight, null, tint = p.textDim)
    }
}

// =================================================================== RÉGLAGES
@Composable
fun SettingsScreen() {
    val app = LocalApp.current
    val ctx = LocalContext.current
    val p = LocalPalette.current
    val snap by app.store.state.collectAsState()
    val s = snap.settings
    var name by rememberSaveable { mutableStateOf(s.userName) }
    val exportLauncher = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri ->
        uri ?: return@rememberLauncherForActivityResult
        runCatching { ctx.contentResolver.openOutputStream(uri)?.use { it.write(app.store.exportJson().toByteArray()) } }
            .onSuccess { Toast.makeText(ctx, "Données exportées", Toast.LENGTH_SHORT).show() }
            .onFailure { Toast.makeText(ctx, "Export impossible", Toast.LENGTH_SHORT).show() }
    }
    val importLauncher = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        uri ?: return@rememberLauncherForActivityResult
        val ok = runCatching { ctx.contentResolver.openInputStream(uri)?.use { app.store.importJson(it.readBytes().decodeToString()) } == true }.getOrDefault(false)
        Toast.makeText(ctx, if (ok) "Données importées" else "Fichier invalide", Toast.LENGTH_SHORT).show()
        com.superessence.noctea.notify.Reminders.reschedule(ctx)
    }
    SubScreen("Réglages") {
        Text("Profil", style = MaterialTheme.typography.headlineSmall, modifier = Modifier.padding(20.dp, 8.dp))
        OutlinedTextField(
            name, { name = it.take(24); app.store.settings { st -> st.copy(userName = name.trim()) } },
            label = { Text("Prénom") }, singleLine = true, modifier = Modifier.fillMaxWidth().padding(horizontal = 20.dp),
            shape = RoundedCornerShape(16.dp),
            colors = OutlinedTextFieldDefaults.colors(focusedBorderColor = p.accent, unfocusedBorderColor = p.cardBorder),
        )
        Text("Apparence", style = MaterialTheme.typography.headlineSmall, modifier = Modifier.padding(20.dp, 20.dp, 20.dp, 8.dp))
        Row(Modifier.padding(horizontal = 20.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Palettes.all.forEach { pal ->
                Column(
                    Modifier.weight(1f).clip(RoundedCornerShape(18.dp))
                        .background(Brush.verticalGradient(listOf(pal.sky0, pal.sky2)))
                        .clickable { app.store.settings { it.copy(theme = pal.name) } }
                        .padding(12.dp),
                    horizontalAlignment = Alignment.CenterHorizontally,
                ) {
                    Box(Modifier.size(22.dp).clip(CircleShape).background(pal.accent))
                    Spacer(Modifier.height(6.dp))
                    Text(pal.label, style = MaterialTheme.typography.labelMedium, color = pal.text, textAlign = TextAlign.Center)
                    if (s.theme == pal.name) Text("✓", color = pal.moon)
                }
            }
        }
        SettingSwitch("Mode nuit automatique", "Couleurs plus chaudes et moins lumineuses en fin de soirée et la nuit", s.autoNight) { v -> app.store.settings { it.copy(autoNight = v) } }
        SettingSwitch("Lecteur minimaliste par défaut", "Écran presque noir pendant l'écoute", s.minimalPlayer) { v -> app.store.settings { it.copy(minimalPlayer = v) } }
        Text("Minuteur de sommeil par défaut", style = MaterialTheme.typography.titleMedium, modifier = Modifier.padding(20.dp, 12.dp, 20.dp, 6.dp))
        Row(Modifier.padding(horizontal = 20.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            listOf(0, 15, 30, 45, 60).forEach { m ->
                NightChip(if (m == 0) "Aucun" else "$m min", s.defaultSleepTimerMin == m) { app.store.settings { it.copy(defaultSleepTimerMin = m) } }
            }
        }
        Text("Données", style = MaterialTheme.typography.headlineSmall, modifier = Modifier.padding(20.dp, 24.dp, 20.dp, 4.dp))
        Text(
            "Tes données (favoris, journal, créations, préférences) restent sur ton téléphone. Exporte-les pour les sauvegarder ou les transférer. La synchronisation automatique sera proposée lorsqu'un compte NOCTÉA sera disponible ; elle s'appuiera sur ce même format.",
            style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(horizontal = 20.dp),
        )
        Row(Modifier.padding(20.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            SecondaryButton("Exporter") { exportLauncher.launch("noctea-sauvegarde.json") }
            SecondaryButton("Importer") { importLauncher.launch(arrayOf("application/json", "text/plain", "*/*")) }
        }
    }
}

// =================================================================== VOIX
@Composable
fun VoiceSettingsScreen() {
    val app = LocalApp.current
    val ctx = LocalContext.current
    val p = LocalPalette.current
    val scope = rememberCoroutineScope()
    val snap by app.store.state.collectAsState()
    val s = snap.settings
    var voices by remember { mutableStateOf<List<android.speech.tts.Voice>>(emptyList()) }
    var testing by remember { mutableStateOf(false) }
    var url by rememberSaveable { mutableStateOf(s.remoteUrl) }
    var key by rememberSaveable { mutableStateOf(s.remoteApiKey) }
    var remoteVoice by rememberSaveable { mutableStateOf(s.remoteVoice) }
    LaunchedEffect(Unit) { voices = app.systemTts.frenchVoices() }
    val sample = "Maintenant... tu peux simplement fermer les yeux. Ce moment est à toi."

    fun preview(provider: com.superessence.noctea.tts.VoiceProvider) {
        testing = true
        scope.launch {
            try {
                val f = File(ctx.cacheDir, "voice_preview_${UUID.randomUUID()}.${if (provider.id == "system") "wav" else "audio"}")
                val ms = provider.synthesize(sample, f)
                val mp = android.media.MediaPlayer()
                mp.setDataSource(f.absolutePath)
                mp.setOnCompletionListener { it.release(); f.delete() }
                mp.prepare(); mp.start()
                Toast.makeText(ctx, "Aperçu (${ms / 1000} s)", Toast.LENGTH_SHORT).show()
            } catch (e: Exception) {
                Toast.makeText(ctx, e.message ?: "Échec de la synthèse", Toast.LENGTH_LONG).show()
            } finally {
                testing = false
            }
        }
    }

    SubScreen("Voix") {
        GlassCard(Modifier.fillMaxWidth().padding(horizontal = 20.dp)) {
            Text(EmbeddedVoice.NAME, style = MaterialTheme.typography.titleMedium, color = p.moon)
            Text(EmbeddedVoice.NOTE, style = MaterialTheme.typography.bodySmall)
            Spacer(Modifier.height(6.dp))
            Text("Utilisée pour toute la bibliothèque, les histoires et le générateur « Créer ma méditation ».", style = MaterialTheme.typography.bodySmall)
        }
        Text("Textes libres", style = MaterialTheme.typography.headlineSmall, modifier = Modifier.padding(20.dp, 22.dp, 20.dp, 6.dp))
        Row(Modifier.padding(horizontal = 20.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            NightChip("Voix de l'appareil", s.ttsProvider == "system") { app.store.settings { it.copy(ttsProvider = "system") } }
            NightChip("Serveur vocal", s.ttsProvider == "remote") { app.store.settings { it.copy(ttsProvider = "remote") } }
        }
        if (s.ttsProvider == "system") {
            Text(app.systemTts.qualityNote, style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(20.dp, 10.dp))
            LabeledSlider("Débit", s.ttsRate, "x${"%.2f".format(s.ttsRate)}", 0.5f..1.1f) { v -> app.store.settings { it.copy(ttsRate = v) } }
            LabeledSlider("Hauteur", s.ttsPitch, "x${"%.2f".format(s.ttsPitch)}", 0.8f..1.15f) { v -> app.store.settings { it.copy(ttsPitch = v) } }
            Text("Voix françaises installées", style = MaterialTheme.typography.titleSmall, modifier = Modifier.padding(20.dp, 10.dp, 20.dp, 4.dp))
            if (voices.isEmpty()) {
                Text("Aucune voix française détectée. Installe-la depuis Réglages Android › Synthèse vocale (Speech Services by Google conseillé).", style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(horizontal = 20.dp))
            }
            voices.forEach { v ->
                Row(
                    Modifier.fillMaxWidth().clickable { app.store.settings { it.copy(systemVoiceName = v.name) } }.padding(horizontal = 20.dp, vertical = 8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(if (s.systemVoiceName == v.name) "●" else "○", color = p.moon)
                    Spacer(Modifier.width(10.dp))
                    Column(Modifier.weight(1f)) {
                        Text(v.name, style = MaterialTheme.typography.bodyLarge)
                        Text("Qualité ${v.quality}${if (v.isNetworkConnectionRequired) " · en ligne" else " · hors ligne"}", style = MaterialTheme.typography.bodySmall)
                    }
                }
            }
            Box(Modifier.fillMaxWidth().padding(20.dp), contentAlignment = Alignment.Center) {
                SecondaryButton(if (testing) "Synthèse…" else "Écouter un aperçu") {
                    if (!testing) preview(app.systemTts.also { it.rate = s.ttsRate; it.pitch = s.ttsPitch; it.voiceName = s.systemVoiceName })
                }
            }
        } else {
            Text(
                "Connecte un serveur de synthèse vocale (par exemple un serveur Piper auto-hébergé avec la voix « fr_FR-siwis-medium », ou une passerelle vers un fournisseur premium). NOCTÉA envoie une requête POST JSON {\"text\", \"voice\", \"length_scale\"} et attend un fichier audio en réponse. La clé éventuelle reste sur ton appareil.",
                style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(20.dp, 10.dp),
            )
            val colors = OutlinedTextFieldDefaults.colors(focusedBorderColor = p.accent, unfocusedBorderColor = p.cardBorder)
            OutlinedTextField(url, { url = it.trim() }, label = { Text("URL du serveur (https://…)") }, singleLine = true, modifier = Modifier.fillMaxWidth().padding(horizontal = 20.dp, vertical = 4.dp), colors = colors)
            OutlinedTextField(remoteVoice, { remoteVoice = it.trim() }, label = { Text("Nom de la voix (facultatif)") }, singleLine = true, modifier = Modifier.fillMaxWidth().padding(horizontal = 20.dp, vertical = 4.dp), colors = colors)
            OutlinedTextField(
                key, { key = it.trim() }, label = { Text("Clé d'accès (facultatif)") }, singleLine = true,
                visualTransformation = androidx.compose.ui.text.input.PasswordVisualTransformation(),
                modifier = Modifier.fillMaxWidth().padding(horizontal = 20.dp, vertical = 4.dp), colors = colors,
            )
            Row(Modifier.padding(20.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                PrimaryButton("Enregistrer") {
                    app.store.settings { it.copy(remoteUrl = url, remoteApiKey = key, remoteVoice = remoteVoice) }
                    Toast.makeText(ctx, "Serveur enregistré", Toast.LENGTH_SHORT).show()
                }
                SecondaryButton(if (testing) "Test…" else "Tester") {
                    if (!testing && url.isNotBlank()) preview(RemoteHttpTtsProvider(url, key, remoteVoice, 1f))
                }
            }
        }
    }
}

// =================================================================== À PROPOS
@Composable
fun AboutScreen() {
    val p = LocalPalette.current
    val app = LocalApp.current
    SubScreen("À propos") {
        Column(Modifier.fillMaxWidth().padding(20.dp), horizontalAlignment = Alignment.CenterHorizontally) {
            MoonMark(120.dp)
            NocteaWordmark(fontSize = 36)
            Text("Retrouvez le calme. Retrouvez le sommeil.", fontFamily = Cormorant, fontStyle = FontStyle.Italic, fontSize = 19.sp, color = p.accent)
            Text("Version ${BuildConfig.VERSION_NAME}", style = MaterialTheme.typography.bodySmall)
            Spacer(Modifier.height(30.dp))
            Text("UNE CRÉATION", style = MaterialTheme.typography.labelSmall, color = p.textDim)
            Spacer(Modifier.height(10.dp))
            Box(Modifier.fillMaxWidth().clip(RoundedCornerShape(20.dp)).background(Color.Black).padding(vertical = 18.dp, horizontal = 24.dp)) {
                Image(
                    painterResource(R.drawable.superessence_wordmark), contentDescription = "Superessence Studio",
                    modifier = Modifier.fillMaxWidth().aspectRatio(1553f / 277f), contentScale = ContentScale.Fit,
                )
            }
            Spacer(Modifier.height(10.dp))
            Text("Superessence Studio — éditeur de NOCTÉA", style = MaterialTheme.typography.titleSmall)
            Spacer(Modifier.height(24.dp))
            GlassCard(Modifier.fillMaxWidth()) {
                Text("Notre intention", style = MaterialTheme.typography.titleMedium)
                Text(
                    "NOCTÉA est née d'une idée simple : offrir chaque soir une présence douce, comme une voix amie qui t'accompagne vers le repos. Méditations, respirations, histoires et ambiances ont été pensées pour apaiser, sans jamais culpabiliser.",
                    style = MaterialTheme.typography.bodyMedium,
                )
            }
            Spacer(Modifier.height(12.dp))
            GlassCard(Modifier.fillMaxWidth()) {
                Text("Crédits", style = MaterialTheme.typography.titleMedium)
                Text(
                    "Voix : ${app.catalog.voiceName}, modèle neuronal Piper entraîné sur le corpus SIWIS (Université d'Édimbourg), traitement audio Superessence Studio.\n" +
                        "Ambiances et musiques : synthèse procédurale originale Superessence Studio.\n" +
                        "Polices : Cormorant Garamond, Nunito, Fredoka (SIL Open Font License).\n" +
                        "Lecture audio : AndroidX Media3 (Apache 2.0).\n\n" +
                        "NOCTÉA est un outil de bien-être et ne remplace pas un avis médical. En cas de troubles du sommeil persistants, consulte un professionnel de santé.",
                    style = MaterialTheme.typography.bodySmall,
                )
            }
        }
    }
}

// =================================================================== PREMIUM
@Composable
fun PremiumScreen() {
    val app = LocalApp.current
    val p = LocalPalette.current
    val gw = app.entitlements.gatewayInfo
    SubScreen("NOCTÉA Premium") {
        GlassCard(Modifier.fillMaxWidth().padding(horizontal = 20.dp)) {
            Text("✨ ${gw.statusMessage}", style = MaterialTheme.typography.bodyLarge, color = p.moon)
        }
        Spacer(Modifier.height(16.dp))
        Row(Modifier.padding(horizontal = 20.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            TierCard("Gratuit", listOf("Méditations classiques", "Tous les sons d'ambiance", "3 créations personnalisées / jour (jusqu'à 20 min)", "Lecteur complet & minuteur", "Rituel du soir & journal"), Modifier.weight(1f))
            TierCard("Premium", listOf("Créations illimitées", "Méditations longues (45 – 60 min)", "Bibliothèque & histoires complètes", "Téléchargements illimités", "Mixeur jusqu'à 6 couches", "Personnalisation complète"), Modifier.weight(1f), highlight = true)
        }
        Text(
            "Les fonctions essentielles ne sont jamais bloquées. Aucun paiement n'est proposé dans cette version : les abonnements passeront exclusivement par Google Play lorsqu'ils seront disponibles.",
            style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(20.dp),
        )
    }
}

@Composable
private fun TierCard(title: String, items: List<String>, modifier: Modifier, highlight: Boolean = false) {
    val p = LocalPalette.current
    GlassCard(modifier.heightIn(min = 280.dp), padding = 14.dp) {
        Text(title, style = MaterialTheme.typography.headlineSmall, color = if (highlight) p.moon else p.text)
        Spacer(Modifier.height(8.dp))
        items.forEach { Text("• $it", style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(vertical = 3.dp)) }
    }
}

// =================================================================== STATISTIQUES
@Composable
fun StatsScreen() {
    val app = LocalApp.current
    val p = LocalPalette.current
    val snap by app.store.state.collectAsState()
    var monthOffset by remember { mutableStateOf(0L) }
    val month = LocalDate.now().withDayOfMonth(1).minusMonths(monthOffset)
    val days = month.lengthOfMonth()
    val monthMs = (1..days).sumOf { snap.practiceDays[month.withDayOfMonth(it).toString()] ?: 0L }
    SubScreen("Mes statistiques") {
        Text(
            "Chaque moment compte, même court. Il n'y a pas de série à « perdre » : seulement des soirées de douceur à accumuler.",
            style = MaterialTheme.typography.bodyMedium, fontStyle = FontStyle.Italic, modifier = Modifier.padding(horizontal = 20.dp),
        )
        Row(Modifier.fillMaxWidth().padding(20.dp, 16.dp, 20.dp, 6.dp), verticalAlignment = Alignment.CenterVertically) {
            IconButton(onClick = { monthOffset++ }) { Text("‹", fontSize = 26.sp, color = p.text) }
            Text(
                month.month.getDisplayName(TextStyle.FULL, Locale.FRANCE).replaceFirstChar { it.uppercase() } + " " + month.year,
                style = MaterialTheme.typography.headlineSmall, modifier = Modifier.weight(1f), textAlign = TextAlign.Center,
            )
            IconButton(onClick = { if (monthOffset > 0) monthOffset-- }, enabled = monthOffset > 0) { Text("›", fontSize = 26.sp, color = p.text) }
        }
        Row(Modifier.padding(horizontal = 20.dp)) {
            listOf("L", "M", "M", "J", "V", "S", "D").forEach { Text(it, Modifier.weight(1f), textAlign = TextAlign.Center, style = MaterialTheme.typography.labelMedium, color = p.textDim) }
        }
        val lead = month.dayOfWeek.value - 1
        val cells = List(lead) { 0 } + (1..days).toList()
        cells.chunked(7).forEach { week ->
            Row(Modifier.padding(horizontal = 20.dp, vertical = 3.dp)) {
                week.forEach { d ->
                    Box(Modifier.weight(1f).aspectRatio(1f).padding(3.dp), contentAlignment = Alignment.Center) {
                        if (d > 0) {
                            val date = month.withDayOfMonth(d)
                            val ms = snap.practiceDays[date.toString()] ?: 0L
                            val level = (ms / 60_000f / 20f).coerceIn(0f, 1f)
                            val ritual = date.toString() in snap.ritualDone
                            Box(
                                Modifier.fillMaxSize().clip(CircleShape).background(
                                    if (ms > 0) p.moon.copy(alpha = 0.25f + 0.65f * level) else Color.White.copy(alpha = 0.04f),
                                ),
                                contentAlignment = Alignment.Center,
                            ) {
                                Text("$d", style = MaterialTheme.typography.labelMedium, color = if (ms > 0) Color(0xFF1A1430) else p.textDim)
                                if (ritual) Text("☾", fontSize = 9.sp, color = p.accent, modifier = Modifier.align(Alignment.TopEnd))
                            }
                        }
                    }
                }
                repeat(7 - week.size) { Spacer(Modifier.weight(1f)) }
            }
        }
        Spacer(Modifier.height(14.dp))
        Row(Modifier.padding(horizontal = 20.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            StatBox(formatDuration(monthMs), "ce mois-ci", Modifier.weight(1f))
            StatBox("${(1..days).count { (snap.practiceDays[month.withDayOfMonth(it).toString()] ?: 0) >= 60_000 }}", "soirées de pratique", Modifier.weight(1f))
        }
        Spacer(Modifier.height(10.dp))
        val moods = snap.journal.filter { it.date.startsWith(month.toString().take(7)) }
        if (moods.isNotEmpty()) {
            Text("Humeurs du mois", style = MaterialTheme.typography.titleMedium, modifier = Modifier.padding(20.dp, 10.dp))
            Text(moods.sortedBy { it.date }.joinToString("  ") { m -> Discovery.moods.firstOrNull { it.id == m.mood }?.emoji ?: "" }, fontSize = 22.sp, modifier = Modifier.padding(horizontal = 20.dp))
        }
    }
}

// =================================================================== JOURNAL
@Composable
fun JournalScreen() {
    val app = LocalApp.current
    val p = LocalPalette.current
    val snap by app.store.state.collectAsState()
    val today = LocalDate.now().toString()
    val existing = snap.journal.firstOrNull { it.date == today }
    var mood by rememberSaveable { mutableStateOf(existing?.mood ?: 0) }
    var text by rememberSaveable { mutableStateOf(existing?.text ?: "") }
    var gratitude by rememberSaveable { mutableStateOf(existing?.gratitude ?: "") }
    val colors = OutlinedTextFieldDefaults.colors(focusedBorderColor = p.accent, unfocusedBorderColor = p.cardBorder, cursorColor = p.moon)
    SubScreen("Journal du soir") {
        Text("Comment s'est passée ta journée ?", style = MaterialTheme.typography.titleMedium, modifier = Modifier.padding(horizontal = 20.dp))
        Spacer(Modifier.height(8.dp))
        Row(Modifier.padding(horizontal = 14.dp)) {
            Discovery.moods.forEach { m ->
                Column(
                    Modifier.weight(1f).clip(RoundedCornerShape(14.dp)).background(if (mood == m.id) p.accent.copy(alpha = 0.3f) else Color.Transparent)
                        .clickable { mood = m.id }.padding(vertical = 8.dp),
                    horizontalAlignment = Alignment.CenterHorizontally,
                ) { Text(m.emoji, fontSize = 24.sp) }
            }
        }
        if (mood > 0) Text(Discovery.moods.first { it.id == mood }.label + " — " + Discovery.moodMessage(mood), style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(20.dp, 6.dp))
        OutlinedTextField(text, { text = it.take(4000) }, label = { Text("Ce que je dépose ce soir") }, modifier = Modifier.fillMaxWidth().heightIn(min = 140.dp).padding(horizontal = 20.dp, vertical = 6.dp), colors = colors, shape = RoundedCornerShape(16.dp))
        OutlinedTextField(gratitude, { gratitude = it.take(1000) }, label = { Text("Trois choses pour lesquelles je suis reconnaissant·e") }, modifier = Modifier.fillMaxWidth().heightIn(min = 90.dp).padding(horizontal = 20.dp, vertical = 6.dp), colors = colors, shape = RoundedCornerShape(16.dp))
        PrimaryButton("Enregistrer", Modifier.fillMaxWidth().padding(20.dp), enabled = mood > 0 || text.isNotBlank()) {
            val entry = JournalEntry(existing?.id ?: UUID.randomUUID().toString(), today, System.currentTimeMillis(), mood, text.trim(), gratitude.trim())
            app.store.edit { s -> s.copy(journal = listOf(entry) + s.journal.filter { it.date != today }) }
        }
        if (mood > 0) {
            Text("Pour t'accompagner", style = MaterialTheme.typography.titleMedium, modifier = Modifier.padding(horizontal = 20.dp))
            val nav = LocalNav.current
            Discovery.forMood(app.catalog, mood).take(2).forEach { t ->
                com.superessence.noctea.ui.components.TrackRow(t.card(app.catalog, snap.favorites), onClick = { playTrack(app, nav, t.id) }, onFav = null)
            }
        }
        val past = snap.journal.filter { it.date != today }
        if (past.isNotEmpty()) {
            Text("Mes soirées précédentes", style = MaterialTheme.typography.headlineSmall, modifier = Modifier.padding(20.dp, 20.dp, 20.dp, 6.dp))
            past.take(30).forEach { e ->
                GlassCard(Modifier.fillMaxWidth().padding(horizontal = 20.dp, vertical = 5.dp), padding = 14.dp) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(Discovery.moods.firstOrNull { it.id == e.mood }?.emoji ?: "•", fontSize = 20.sp)
                        Spacer(Modifier.width(10.dp))
                        Text(LocalDate.parse(e.date).let { "${it.dayOfMonth} ${it.month.getDisplayName(TextStyle.FULL, Locale.FRANCE)} ${it.year}" }, style = MaterialTheme.typography.titleSmall, modifier = Modifier.weight(1f))
                        IconButton(onClick = { app.store.edit { s -> s.copy(journal = s.journal.filter { it.id != e.id }) } }) { Icon(Icons.Filled.Delete, "Supprimer", tint = p.textDim) }
                    }
                    if (e.text.isNotBlank()) Text(e.text, style = MaterialTheme.typography.bodyMedium)
                    if (e.gratitude.isNotBlank()) Text("Gratitude : ${e.gratitude}", style = MaterialTheme.typography.bodySmall, color = p.moon)
                }
            }
        }
    }
}
